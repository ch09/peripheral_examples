/**************************************************************************//**
 * @file
 * @brief Use the ADC to take repeated nonblocking measurements on multiple inputs
 * @version 0.0.1
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2018 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silicon Labs Software License Agreement. See
 * "http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt"
 * for details. Before using this software for any purpose, you must agree to the
 * terms of that agreement.
 *
 ******************************************************************************/

#include <stdio.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_iadc.h"
#include "em_gpio.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Set CLK_ADC to 10MHz
#define CLK_SRC_ADC_FREQ          10000000 // CLK_SRC_ADC
#define CLK_ADC_FREQ              10000000 // CLK_ADC - 10MHz max in normal mode

// Number of scan channels
#define NUM_INPUTS 8

// When changing GPIO port/pins below, make sure to change xBUSALLOC macro's
// accordingly.
#define IADC_INPUT_0_BUS          CDBUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_CDBUSALLOC_CDEVEN0_ADC0
#define IADC_INPUT_1_BUS          CDBUSALLOC
#define IADC_INPUT_1_BUSALLOC     GPIO_CDBUSALLOC_CDODD0_ADC0

/*******************************************************************************
 ***************************   GLOBAL VARIABLES   *******************************
 ******************************************************************************/

static volatile double scanResult[NUM_INPUTS];  // Volts

/**************************************************************************//**
 * @brief  IADC Initializer
 *****************************************************************************/
void initIADC (void)
{
  // Declare init structs
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
  IADC_ScanTable_t initScanTable = IADC_SCANTABLE_DEFAULT;    // Scan Table

  // Enable IADC0 and GPIO clock branches
  CMU_ClockEnable(cmuClock_IADC0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Reset IADC to reset configuration in case it has been modified by
  // other code
  IADC_reset(IADC0);

  // Select clock for IADC
  CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);  // FSRCO - 20MHz

  // Modify init structs and initialize
  init.warmup = iadcWarmupKeepWarm;

  // Set the HFSCLK prescale value here
  init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

  // Configuration 0 is used by both scan and single conversions by default
  // Use unbuffered AVDD as reference
  initAllConfigs.configs[0].reference = iadcCfgReferenceVddx;

  // Divides CLK_SRC_ADC to set the CLK_ADC frequency
  initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
                                             CLK_ADC_FREQ,
                                             0,
                                             iadcCfgModeNormal,
                                             init.srcClkPrescale);

  // Scan initialization
  initScan.dataValidLevel = _IADC_SCANFIFOCFG_DVL_VALID4;     // Set the SCANFIFODVL flag when 4 entries in the scan FIFO are valid;
                                                              // Not used as an interrupt or to wake up LDMA; flag is ignored

  // Tag FIFO entry with scan table entry id.
  initScan.showId = true;

  // Configure entries in scan table, CH0 is single-ended from input 0, CH1 is
  // single-ended from input 1
  initScanTable.entries[0].posInput = iadcPosInputPortCPin4; // PC04 -> P25 on BRD4001 J102
  initScanTable.entries[0].negInput = iadcNegInputGnd;
  initScanTable.entries[0].includeInScan = true;

  initScanTable.entries[1].posInput = iadcPosInputPortCPin5; // PC05 -> P27 on BRD4001 J102
  initScanTable.entries[1].negInput = iadcNegInputGnd;
  initScanTable.entries[1].includeInScan = true;

  initScanTable.entries[2].posInput = iadcPosInputAvdd;      // Add AVDD to scan for demonstration purposes
  initScanTable.entries[2].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[2].includeInScan = true;

  initScanTable.entries[3].posInput = iadcPosInputVddio;     // Add VDDIO to scan for demonstration purposes
  initScanTable.entries[3].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[3].includeInScan = true;

  initScanTable.entries[4].posInput = iadcPosInputVss;       // Add VSS to scan for demonstration purposes
  initScanTable.entries[4].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[4].includeInScan = false;            // FIFO is only 4 entries deep

  initScanTable.entries[5].posInput = iadcPosInputDvdd;      // Add DVDD to scan for demonstration purposes
  initScanTable.entries[5].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[5].includeInScan = false;

  initScanTable.entries[6].posInput = iadcPosInputVddx;      // Add VDDx to scan for demonstration purposes
  initScanTable.entries[6].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[6].includeInScan = false;

  initScanTable.entries[7].posInput = iadcPosInputVddlv;     // Add VDDlv to scan for demonstration purposes
  initScanTable.entries[7].negInput = iadcNegInputGnd | 1;   // when measuring a supply, PINNEG needs to be odd (1, 3, 5,...)
  initScanTable.entries[7].includeInScan = false;

  // Initialize IADC
  IADC_init(IADC0, &init, &initAllConfigs);

  // Initialize Scan
  IADC_initScan(IADC0, &initScan, &initScanTable);

  // Allocate the analog bus for ADC0 inputs
  GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
  GPIO->IADC_INPUT_1_BUS |= IADC_INPUT_1_BUSALLOC;

  // Clear any previous interrupt flags
  IADC_clearInt(IADC0, _IADC_IEN_MASK);

  // Enable Scan interrupts
  IADC_enableInt(IADC0, IADC_IEN_SCANTABLEDONE);

  // Enable ADC interrupts
  NVIC_ClearPendingIRQ(IADC_IRQn);
  NVIC_EnableIRQ(IADC_IRQn);
}

/**************************************************************************//**
 * @brief  ADC Handler
 *****************************************************************************/
void IADC_IRQHandler(void)
{
  IADC_Result_t result = {0, 0};

  // Get ADC results
  while(IADC_getScanFifoCnt(IADC0))
  {
    // Read data from the scan FIFO
    result = IADC_pullScanFifoResult(IADC0);

    // Calculate input voltage:
    //  For single-ended the result range is 0 to +Vref, i.e.,
    //  for Vref = AVDD = 3.30V, 12 bits represents 3.30V full scale IADC range.
    scanResult[result.id] = result.data * 3.3 / 0xFFF;

    if((result.id > 1) && (result.id < 7))      // supply voltages other than vddlv are connected through a 1/4 voltage divider
    {
        scanResult[result.id] *= 4;             // rectify voltage divider to get actual voltage
    }
  }

  if(result.id == 3) // first set of table entries
  {
      IADC_setScanMask(IADC0, 0x00F0);  // configure scan mask to measure second set of table entries
  } else
  {
      IADC_setScanMask(IADC0, 0x000F);  // configure scan mask to measure first set of table entries
  }

  // Start next IADC conversion
  IADC_clearInt(IADC0, IADC_IEN_SCANTABLEDONE); // flags are sticky; must be cleared in software
  IADC_command(IADC0, iadcCmdStartScan);
}

/**************************************************************************//**
 * @brief  Main function
 *****************************************************************************/
int main(void)
{
  CHIP_Init();

  // Initialize the IADC
  initIADC();

  // Start first conversion
  IADC_command(IADC0, iadcCmdStartScan);

  // Infinite loop
  while(1);
}
