/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "Fundamentals.h"
#include "prbit.h"

#include "HPPAMul.h"

/*
 *-----------------------------------------------------------------------
 *
 * Local data for an HPPA proc. This should be obtained by asking the 
 * Code Generator to fill these arrays.
 *
 *-----------------------------------------------------------------------
 */
 
static PRInt16 shiftCosts[32] = 
{
  0,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   4,   4,   4,   4,   4
};

static PRInt16 shiftAddCosts[32] =
{
  4,   4,   4,   4,   999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999
};

static PRInt16 shiftSubCosts[32] =
{
  4,   999, 999, 999, 999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999,
  999, 999, 999, 999, 999, 999, 999, 999
};

static PRInt16 addCost = 4;

/*
 *-----------------------------------------------------------------------
 *
 * getMulAlgorithm --
 *
 *   Return the best algorithm for an immediate multiplication.
 *   If algorithm.cost >= maxCost or retval is false then no algorithm 
 *   was found and a regular multiplication should be used.
 *
 *   This algorithm does not work if the register to multiply is larger
 *   than an PRUint32 and the multiplicand does not fit exactely in an PRUint32.
 *   e.g.: long long mul by a negative value.
 *
 *-----------------------------------------------------------------------
 */
bool
getMulAlgorithm(MulAlgorithm* algorithm, PRUint32 multiplicand, PRInt16 maxCost)
{
  PRUint32 mask;
  PRUint8  currentCost;
  PRInt8   shiftBy;

  algorithm->cost = maxCost;
  if (maxCost <= 0)
	return false;

  if (multiplicand == 0x1)
	{
	  algorithm->nOperations = 1;
	  algorithm->cost = 0;
	  algorithm->operations[0] = maMultiplicand;
	  return true;
	}

  if (multiplicand == 0x0)
	{
	  algorithm->nOperations = 1;
	  algorithm->cost = 0;
	  algorithm->operations[0] = maZero;
	  return true;
	}

  MulAlgorithm* downAlgorithm = new MulAlgorithm();
  MulAlgorithm* bestAlgorithm = new MulAlgorithm();
  MulAlgorithm* swapAlgorithm;

  // we try to do a shift if there is a group of 0 bits.
  if ((multiplicand & 0x1) == 0x0)
	{
	  // number of low zero bits.
	  mask = multiplicand & -multiplicand;
	  PR_FLOOR_LOG2(shiftBy, mask);

	  currentCost = shiftCosts[shiftBy];
	  getMulAlgorithm(downAlgorithm, multiplicand >> shiftBy, maxCost - currentCost);
	  currentCost += downAlgorithm->cost;

	  if (currentCost < maxCost)
		{
		  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
		  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = shiftBy;
		  bestAlgorithm->operations[bestAlgorithm->nOperations] = maShiftValue;
		  maxCost = currentCost;
		}
	}

  // If this is an odd number, we can try to add one or to subtract one.
  if ((multiplicand & 0x1) != 0x0)
	{
	  for (mask = 1; (mask & multiplicand) != 0x0; mask <<= 1);

	  if (mask > 2 && multiplicand != 3)
		{
		  currentCost = addCost;
		  getMulAlgorithm(downAlgorithm, multiplicand + 1, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;

		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = 0;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maSubShiftMultiplicandFromValue;
			  maxCost = currentCost;
			}
		}
	  else
		{
		  currentCost = addCost;
		  getMulAlgorithm(downAlgorithm, multiplicand - 1, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;

		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = 0;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maAddValueToShiftMultiplicand;
			  maxCost = currentCost;
			}
		}
	}

  mask = multiplicand - 1;
  PR_FLOOR_LOG2(shiftBy, mask);
  while (shiftBy >= 2)
    {
	  PRUint32 d;

      d = (0x1 << shiftBy) + 1;
      if (multiplicand % d == 0 && multiplicand > d)
		{
		  currentCost = PR_MIN(shiftAddCosts[shiftBy], addCost + shiftCosts[shiftBy]);
		  getMulAlgorithm(downAlgorithm, multiplicand / d, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;

		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = shiftBy;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maAddValueToShiftValue;
			  maxCost = currentCost;
			}
		  break;
		}

	  d = (0x1 << shiftBy) - 1;
      if (multiplicand % d == 0 && multiplicand > d)
		{
		  currentCost = PR_MIN(shiftSubCosts[shiftBy], addCost + shiftCosts[shiftBy]);
		  getMulAlgorithm(downAlgorithm, multiplicand / d, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;

		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = shiftBy;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maSubValueFromShiftValue;
			  maxCost = currentCost;
			}
		  break;
		}
	  shiftBy--;
	}
  
  if ((multiplicand & 0x1) != 0x0)
    {
      mask = multiplicand - 1;
      mask = mask & -mask;
	  if (mask != 0 && (mask & (mask - 1)) == 0)
		{
		  PR_FLOOR_LOG2(shiftBy, mask);
		  currentCost = shiftAddCosts[shiftBy];
		  getMulAlgorithm(downAlgorithm, (multiplicand - 1) >> shiftBy, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;
		  
		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = shiftBy;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maAddMultiplicandToShiftValue;
			  maxCost = currentCost;
			}
	    }

      mask = multiplicand + 1;
      mask = mask & -mask;
	  if (mask != 0 && (mask & (mask - 1)) == 0)
		{
		  PR_FLOOR_LOG2(shiftBy, mask);
		  currentCost = shiftSubCosts[shiftBy];
		  getMulAlgorithm(downAlgorithm, (multiplicand + 1) >> shiftBy, maxCost - currentCost);
		  currentCost += downAlgorithm->cost;

		  if (currentCost < maxCost)
			{
			  swapAlgorithm = downAlgorithm, downAlgorithm = bestAlgorithm, bestAlgorithm = swapAlgorithm;
			  bestAlgorithm->shiftAmount[bestAlgorithm->nOperations] = shiftBy;
			  bestAlgorithm->operations[bestAlgorithm->nOperations] = maSubMultiplicandFromShiftValue;
			  maxCost = currentCost;
			}
		}
    }

  // No algorithm found.
  if (maxCost == algorithm->cost)
	return false;
  
  // Too long
  if (bestAlgorithm->nOperations == 32)
	return false;

  algorithm->nOperations = bestAlgorithm->nOperations + 1;
  algorithm->cost = maxCost;
  
  copy(bestAlgorithm->operations, &bestAlgorithm->operations[algorithm->nOperations], algorithm->operations);
  copy(bestAlgorithm->shiftAmount, &bestAlgorithm->shiftAmount[algorithm->nOperations], algorithm->shiftAmount);

  // we better find a way to avoid allocating memory each time.
  delete downAlgorithm;
  delete bestAlgorithm;

  return true;
}
