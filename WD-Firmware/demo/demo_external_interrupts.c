/*
* SPDX-License-Identifier: Apache-2.0
* Copyright 2019 Western Digital Corporation or its affiliates.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http:*www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
* include files
*/
#include "psp_api.h"
#include "demo_platform_al.h"
#include "external_interrupts.h"  /* BSP file - for generation of external interrupt upon demo test call */

/**
* definitions
*/


/* Verification points at each ISR */
#define D_DEMO_INITIAL_STATE               0
#define D_DEMO_EXT_INT_ISR_JUMPED          1
#define D_DEMO_EXT_INT_PENDING_BIT_SET     2
#define D_DEMO_EXT_INT_CORRECT_PRIORITY    3


/**
* macros
*/

/**
* types
*/


/**
* local prototypes
*/

/**
* external prototypes
*/

/* General vector table */
extern void psp_vect_table(void);
/* External interrupts vector table */
extern pspInterruptHandler_t G_Ext_Interrupt_Handlers[];


/**
* global variables
*/

/* Array of test results. It is used repeatedly per test. Each place in the array represents the results of corresponding ISR in the current test.
 * Note - As only irq3 and irq4 are functional, then places 0, 1 and 2 in the array are not in use */
u32_t g_ucDemoExtIntsPassFailResult[D_BSP_LAST_IRQ_NUM+1];

/* Array of priority-level set by the test function for each external-interrupt source-id */
u32_t g_usPriorityLevelPerSourceId[D_BSP_LAST_IRQ_NUM+1];



/**
* functions
*/

/**
* Initialize (zero) the array of test results - to be called before each test
*
*/
void demoInitializeTestResults(void)
{
	u32_t uiTestResultsIndex;

	for(uiTestResultsIndex=0; uiTestResultsIndex <= D_BSP_LAST_IRQ_NUM; uiTestResultsIndex++)
	{
		/* Set test-result of all ISRs to "initial state" (0) */
		g_ucDemoExtIntsPassFailResult[uiTestResultsIndex] = D_DEMO_INITIAL_STATE;

		/* Make sure the external-interrupt triggers are cleared */
		bspClearExtInterrupt(uiTestResultsIndex);
	}
}


/**
* demoLoopForDelay - run a loop to create a delay in the application
*
* uiNumberOfCycles - how many loop-iterations to run here
*
*
* */
void demoLoopForDelay(u32_t uiNumberOfIterations)
{
	u32_t uiIndex=0;

	/* Display the number of iterations going to be run here */
    demoOutputMsg("Loop %d Iterations:\n", uiNumberOfIterations);

	for (;uiIndex < uiNumberOfIterations; uiIndex++)
	{
		demoOutputMsg("-- Iteration -- \n");
	}
}



/**
 * ISR for test #1, #2, #3 ,#4 and #5 (see the tests later in this file)
 *
 * The ISR indicates/marks 3 things:
 * - ISR of the current claim-id has been occurred
 * - Whether corresponding "pending" bit is set or not
 * - Corresponding "priority" field is correct
 *
 * Tests 1,2,3,4 and 5 does different things each. However all of them use this ISR.
 */
void demoExtIntTest_1_2_3_4_5_ISR(void)
{
	/* Get the claim-id (== source-id) of the current ISR */
	u08_t ucIntSourceId = pspExtInterruptGetClaimId();

	/* Indication that this ISR has been occurred */
	g_ucDemoExtIntsPassFailResult[ucIntSourceId] = D_DEMO_EXT_INT_ISR_JUMPED;

	/* Mark whether corresponding pending-bit is set or not */
	if (D_PSP_ON == pspExtInterruptIsPending(ucIntSourceId))
	{
		g_ucDemoExtIntsPassFailResult[ucIntSourceId] = D_DEMO_EXT_INT_PENDING_BIT_SET;
	}

	/* Verify correct priority-level (clidpri) field in meicidpl CSR */
	if (pspExtInterruptGetPriority() ==  g_usPriorityLevelPerSourceId[ucIntSourceId])
	{
		g_ucDemoExtIntsPassFailResult[ucIntSourceId] = D_DEMO_EXT_INT_CORRECT_PRIORITY;
	}

	/* Clear the gateway */
	/* pspExtInterruptClearGateway(ucIntSourceId); */ /* Nati - TBD see next comment */

	/* Stop generation of external interrupts - all sources */
	bspClearExtInterrupt(ucIntSourceId);
}


/* TBD - following ISRs */


/**
 *  Test # 1 - After full external interrupts initialization, disable external interrupts in mie CSR.
 *             Verify that external interrupts not occur at all.
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest1GlobalDisabled(void)
{
	u32_t uiSourceId, uiExtIntsIndex, uiTestResult = 0;

    /* Set Standard priority order */
	pspExtInterruptSetPriorityOrder(D_PSP_EXT_INT_STANDARD_PRIORITY);

    /* Set interrupts threshold to minimal (== all interrupts should be served) */
	pspExtInterruptsSetThreshold(M_PSP_EXT_INT_THRESHOLD_UNMASK_ALL_VALUE);

	/* Initialize all Interrupt-sources & Register ISR for all */
	for (uiSourceId=D_BSP_FIRST_IRQ_NUM; uiSourceId <= D_BSP_LAST_IRQ_NUM; uiSourceId++)
	{
		/* Set Gateway Interrupt type (Level) */
		pspExtInterruptSetType(uiSourceId, D_PSP_EXT_INT_LEVEL_TRIG_TYPE);

		/* Set gateway Polarity (Active high) */
		pspExtInterruptSetPolarity(uiSourceId, D_PSP_EXT_INT_ACTIVE_HIGH);

		/* Clear the gateway */
		pspExtInterruptClearGateway(uiSourceId);

		/* Set the priority level to highest to all interrupt sources */
		g_usPriorityLevelPerSourceId[uiSourceId] = M_PSP_EXT_INT_PRIORITY_SET_TO_HIGHEST_VALUE;
		/* Priority-level is checked later so store it here as expected value */
		pspExtInterruptSetPriority(uiSourceId, g_usPriorityLevelPerSourceId[uiSourceId]);

		/* Enable each one of the interrupts in the PIC */
		pspExternalInterruptEnableNumber(uiSourceId);

		/* Register ISRs to all interrupt sources (here we use same ISR to all of them) */
		pspExternalInterruptRegisterISR(uiSourceId, demoExtIntTest_1_2_3_4_5_ISR, 0);
	}

	/* Enable interrupts in mstatus CSR */
	M_PSP_INTERRUPTS_ENABLE_IN_MACHINE_LEVEL();

	/* Disable external interrupts in mie CSR */
	M_PSP_CLEAR_CSR(D_PSP_MIE_NUM, D_PSP_MIE_MEIE_MASK);

	/* Generate external interrupts 3 & 4 (with Active-High polarity, Level trigger type) */
	bspGenerateExtInterrupt(D_BSP_IRQ_3, D_PSP_EXT_INT_ACTIVE_HIGH, D_PSP_EXT_INT_LEVEL_TRIG_TYPE );
	bspGenerateExtInterrupt(D_BSP_IRQ_4, D_PSP_EXT_INT_ACTIVE_HIGH, D_PSP_EXT_INT_LEVEL_TRIG_TYPE );

	/* Verify no external interrupts have been occurred */
	for (uiExtIntsIndex = D_BSP_FIRST_IRQ_NUM; uiExtIntsIndex <= D_BSP_LAST_IRQ_NUM; uiExtIntsIndex++)
	{
		if (D_DEMO_INITIAL_STATE != g_ucDemoExtIntsPassFailResult[uiExtIntsIndex])
		{
			/* Output a failure message */
 			demoOutputMsg("External Interrupts, Test #1 Failed\n");
 			demoOutputMsg("ISR # %d unexpectedly jumped\n ",uiExtIntsIndex);

			/* Loop here to let debug */
			M_ENDLESS_LOOP();
		}
	}

    return uiTestResult;
}


/**
 *  Test # 2 - Enable IRQ3 and disable IRQ4 in meieS CSRs.
 *             Verify that only enabled external interrupt source did occurred
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest2SpecificDisabled(void)
{
	u32_t uiSourceId, uiTestResult = 0;

    /* Set Standard priority order */
	pspExtInterruptSetPriorityOrder(D_PSP_EXT_INT_STANDARD_PRIORITY);

    /* Set interrupts threshold to minimal (== all interrupts should be served) */
	pspExtInterruptsSetThreshold(M_PSP_EXT_INT_THRESHOLD_UNMASK_ALL_VALUE);

	/* Initialize all Interrupt-sources & Register ISR for all */
	for (uiSourceId=D_BSP_FIRST_IRQ_NUM; uiSourceId <= D_BSP_LAST_IRQ_NUM; uiSourceId++)
	{
		/* Set Gateway Interrupt type (Level) */
		pspExtInterruptSetType(uiSourceId, D_PSP_EXT_INT_LEVEL_TRIG_TYPE);

		/* Set gateway Polarity (Active high) */
		pspExtInterruptSetPolarity(uiSourceId, D_PSP_EXT_INT_ACTIVE_HIGH);

		/* Clear the gateway */
		pspExtInterruptClearGateway(uiSourceId);

		/* Set the priority level to highest to all interrupt sources */
		pspExtInterruptSetPriority(uiSourceId, M_PSP_EXT_INT_PRIORITY_SET_TO_HIGHEST_VALUE);

		/* Enable each one of the interrupts in the PIC */
		pspExternalInterruptEnableNumber(uiSourceId);

		/* Register ISRs to all interrupt sources (here we use same ISR to all of them) */
		pspExternalInterruptRegisterISR(uiSourceId, demoExtIntTest_1_2_3_4_5_ISR, 0);
	}

	/* Enable interrupts in mstatus CSR */
	M_PSP_INTERRUPTS_ENABLE_IN_MACHINE_LEVEL();

	/* Enable external interrupts in mie CSR */
	M_PSP_SET_CSR(D_PSP_MIE_NUM, D_PSP_MIE_MEIE_MASK);

	/* Disable IRQ4 */
	pspExternalInterruptDisableNumber(D_BSP_IRQ_4);

	/* Generate external interrupts 3 & 4 (with Active-High, Level trigger type) */
	bspGenerateExtInterrupt(D_BSP_IRQ_3, D_PSP_EXT_INT_ACTIVE_HIGH, D_PSP_EXT_INT_LEVEL_TRIG_TYPE );
	bspGenerateExtInterrupt(D_BSP_IRQ_4, D_PSP_EXT_INT_ACTIVE_HIGH, D_PSP_EXT_INT_LEVEL_TRIG_TYPE );

	/* Loop for a while.. */
	demoLoopForDelay(0x10); /* Nati - TBD - maybe not needed here at all */

	/* Verify external-interrupt 3 did happen with correct indications */
    if (D_DEMO_EXT_INT_CORRECT_PRIORITY != g_ucDemoExtIntsPassFailResult[D_BSP_IRQ_3])
    {
	    /* Output a failure message */
		demoOutputMsg("External Interrupts, Test #2 Failed:\n");
		demoOutputMsg("ISR 3 did not jump or it has wrong indications\n");
	    /* Loop here to let debug */
	    M_ENDLESS_LOOP();
    }
	/* Verify external-interrupt 4 did not happen */
	if (D_DEMO_INITIAL_STATE != g_ucDemoExtIntsPassFailResult[D_BSP_IRQ_4])
	{
		/* Output a failure message */
		demoOutputMsg("External Interrupts, Test #2 Failed:\n");
		demoOutputMsg("ISR # 4 unexpectedly jumped\n");
		/* Loop here to let debug */
		M_ENDLESS_LOOP();
	}

    return uiTestResult;
}

/**
 *  Test # 3 - Set priority & threshold in standard priority-order
 *             Verify external interrupts occurred only on sources with priority higher than threshold
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest3PriorityStandardOrder(void)
{
	u32_t uiTestResult = 0;

	/* Initialize external interrupts */

	/* Enable external interrupts # 3..8 */

	/* Set standard priority order */

	/* Set theshold to 7 */

	/* Set irq 3 priority to 5 */

	/* Set irq 4 priority to 6 */

	/* Set irq 5 priority to 7 */

	/* Set irq 6 priority to 8 */

	/* Set irq 7 priority to 9 */

	/* Set irq 8 priority to 10 */

    /* Generate external interrupt at sources 3..8 */

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

	/* Verify external interrupts occurred only on sources with priority higher than the threshold (i.e. 6,7,8) */

    return uiTestResult;
}

/**
 *  Test # 4 - Set priority & threshold in reversed priority-order
 *             Verify external interrupts occurred only on sources with priority lower than threshold
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest4PriorityReversedOrder(void)
{
	u32_t uiTestResult = 0;

	/* Initialize external interrupts */

	/* Enable external interrupts # 3..8 */

	/* Set reversed priority order */

	/* Set theshold to 7 */

	/* Set irq 3 priority to 5 */

	/* Set irq 4 priority to 6 */

	/* Set irq 5 priority to 7 */

	/* Set irq 6 priority to 8 */

	/* Set irq 7 priority to 9 */

	/* Set irq 8 priority to 10 */

    /* Generate external interrupt at sources 3..8 */

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

	/* Verify external interrupts occurred only on sources with priority less than the threshold (i.e. 3 and 4) */

    return uiTestResult;
}


/**
 *  Test # 5 - Change priority & threshold on the go
 *             Verify external interrupts occurred only on sources with priority higher than threshold
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest5ThresholdAndPriorityChanging(void)
{
	u32_t uiTestResult = 0;

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

    return uiTestResult;
}


/**
 *  Test # 6 - Test correct gateway (level-triggered or edge-triggered) behavior
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest6GatweayConfiguration(void)
{
	u32_t uiTestResult = 0;

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

	return uiTestResult;
}


/**
 *  Test # 7 - Test external interrupts with lower priority sources do not preempt higher source interrupt
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest7NestedInteeruptLowerPriority(void)
{
	u32_t uiTestResult = 0;

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

    return uiTestResult;
}


/**
 *  Test # 8 - Test external interrupts with same priority sources as current interrupt do not preempt it
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest8NestedInteeruptSamePriority(void)
{
	u32_t uiTestResult = 0;

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

    return uiTestResult;
}


/**
 *  Test # 9 - Test external interrupts with higher priority sources then current interrupt do preempt it
 *
 * @return - returns 0 (success). In case of a failure the function enters an endless loop
 */
u32_t demoExtIntsTest9NestedInteeruptHigherPriority(void)
{
	u32_t uiTestResult = 0;

	/* Loop for a while.. Let all pending ISRs to run */
	demoLoopForDelay(0x100);

    return uiTestResult;
}



/**
 * demoStart - startup point of the demo application. called from main function.
 *
 */
void demoStart(void)
{
   /* Register interrupt vector */
   M_PSP_WRITE_CSR(D_PSP_MTVEC_NUM, &psp_vect_table);

   /* Register external-interrupts vector */
   M_PSP_WRITE_CSR(D_PSP_MEIVT_NUM, G_Ext_Interrupt_Handlers);

   /* Register external interrupt ISR for source #0 - "no interrupt pending" case */
   /* TBD */

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #1 - Global disable exernal interrups */
   demoExtIntsTest1GlobalDisabled();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #2 - Disable specific exernal interrup */
   demoExtIntsTest2SpecificDisabled();
#if 0
   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #3 - Priority & Threshold - standard order */
   demoExtIntsTest3PriorityStandardOrder();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #4 - Priority & Threshold - reversed order*/
   demoExtIntsTest4PriorityReversedOrder();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #5 - Changing Priority & Threshold - */
   demoExtIntsTest5ThresholdAndPriorityChanging();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #6 - Gateways Level/Edge setting*/
   demoExtIntsTest6GatweayConfiguration();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #7 - Nested interrupts - lower priority */
   demoExtIntsTest7NestedInteeruptLowerPriority();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #8 - Nested interrupts - same priority */
   demoExtIntsTest8NestedInteeruptSamePriority();

   /* Initialize test results array and parameters */
   demoInitializeTestResults();
   /* Test #9 - Nested interrupts - higher priority */
   demoExtIntsTest9NestedInteeruptHigherPriority();
#endif
   /* Arriving here means all tests passed successfully */
   demoOutputMsg("External Interrupts tests passed successfully\n");

   /* Loop here to let debug */
   M_ENDLESS_LOOP();
}

