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

#include "Exports.h"
#include "prtypes.h"

//================================================================================
// 64bit Arithmetic Support Functions

// x86Extract64Bit
//
// Purpose: signed right-aligned field extraction
// In:      64 bit source (on  stack)
//          32 bit extraction size (on stack)
// Out:     64 bit result
// Note:    Only works in range 1 <= b <= 63, b is extraction amount

NS_NATIVECALL(int64)
x86Extract64Bit(int64 src, int b)
{
  if (b <= 32)
    {
      b = 32 - b;
      return (int)src << b >> b;
    }
  else
    {
      b = 64 - b;
      return (int)(src >> 32) << b >> b;
    }
}

// 3WayCompare
//
// Purpose: compare two longs
// In:      two longs on the stack
// Out:     depends on condition flags:
//              less = -1
//              equal = 0
//              greater = 1

NS_NATIVECALL(int64)
x86ThreeWayCMP_L(int64 a, int64 b)
{
  return (a > b) - (a < b);
}

// 3WayCompare
//
// Purpose: compare two longs
// In:      two longs on the stack
// Out:     depends on condition flags:
//              less    =  1
//              equal   =  0
//              greater = -1

NS_NATIVECALL(int64)
x86ThreeWayCMPC_L(int64 a, int64 b)
{
  return (a < b) - (a > b);
}

// llmul
//
// Purpose: long multiply (same for signed/unsigned)
// In:      args are passed on the stack:
//              1st pushed: multiplier (QWORD)
//              2nd pushed: multiplicand (QWORD)
// Out:     EDX:EAX - product of multiplier and multiplicand
// Note:    parameters are removed from the stack
// Uses:    ECX

NS_NATIVECALL(int64)
x86Mul64Bit(int64 a, int64 b)
{
  return a * b;
}

// lldiv
//
// Purpose: signed long divide
// In:      args are passed on the stack:
//              1st pushed: divisor (QWORD)
//              2nd pushed: dividend (QWORD)
// Out:     EDX:EAX contains the quotient (dividend/divisor)
// Note:    parameters are removed from the stack
// Uses:    ECX

NS_NATIVECALL(int64)
x86Div64Bit(int64 dividend, int64 divisor)
{
  return dividend / divisor;
}

// llrem
//
// Purpose: signed long remainder
// In:      args are passed on the stack:
//              1st pushed: divisor (QWORD)
//              2nd pushed: dividend (QWORD)
// Out:     EDX:EAX contains the remainder (dividend/divisor)
// Note:    parameters are removed from the stack
// Uses:    ECX

NS_NATIVECALL(int64)
x86Mod64Bit(int64 dividend, int64 divisor)
{
  return dividend % divisor;
}

// llshl
//
// Purpose: long shift left
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

NS_NATIVECALL(int64)
x86Shl64Bit(int64 src, int amount)
{
  return src << amount;
}

// llshr
//
// Purpose: long shift right
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

NS_NATIVECALL(uint64)
x86Shr64Bit(uint64 src, int amount)
{
  return src >> amount;
}

// llsar
//
// Purpose: long shift right signed
// In:      args are passed on the stack: (FIX make fastcall)
//              1st pushed: amount (int)
//              2nd pushed: source (long)
// Out:     EDX:EAX contains the result
// Note:    parameters are removed from the stack
// Uses:    ECX, destroyed

NS_NATIVECALL(int64)
x86Sar64Bit(int64 src, int amount)
{
  return src >> amount;
}

//================================================================================
