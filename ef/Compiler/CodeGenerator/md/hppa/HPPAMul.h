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

#ifndef _HPPA_MUL_H_
#define _HPPA_MUL_H_

#include "Fundamentals.h"

enum MultiplicationOperation
{
  maZero,                           // total = 0
  maMultiplicand,				    // total = multiplicand
  maShiftValue,					    // total = value << shiftBy
  maAddValueToShiftMultiplicand,    // total = value + (multiplicand << shiftBy)
  maSubShiftMultiplicandFromValue,  // total = value - (multiplicand << shiftBy)
  maAddValueToShiftValue,		    // total = (value << shiftBy) + value
  maSubValueFromShiftValue,		    // total = (value << shiftBy) - value
  maAddMultiplicandToShiftValue,    // total = (value << shiftBy) + multiplicand
  maSubMultiplicandFromShiftValue,  // total = (value << shiftBy) - multiplicand
};

struct MulAlgorithm
{
  PRInt16                 cost;
  PRUint16                nOperations;
  MultiplicationOperation operations[32];
  PRUint8                 shiftAmount[32];
};

extern bool getMulAlgorithm(MulAlgorithm* algorithm, PRUint32 multiplicand, PRInt16 maxCost);

#endif /* _HPPA_MUL_H_ */
