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

#include "FloatUtils.h"
#include <math.h>           // For fmod()

#ifdef WIN32
#include <limits>
Flt32 floatPositiveInfinity;
Flt64 doublePositiveInfinity;
Flt32 floatNegativeInfinity;
Flt64 doubleNegativeInfinity;
Flt32 floatNegativeZero;
Flt64 doubleNegativeZero;
Flt32 floatNaN;
Flt64 doubleNaN;

struct DummyInit
{
	DummyInit(Flt32 fZero, Flt64 dZero);
};

DummyInit dummyFloatInit(0.0f, 0.0);

DummyInit::DummyInit(Flt32, Flt64)
{
	floatPositiveInfinity = std::numeric_limits<Flt32>::infinity();
	doublePositiveInfinity = std::numeric_limits<Flt64>::infinity();
	floatNegativeInfinity = -std::numeric_limits<Flt32>::infinity();
	doubleNegativeInfinity = -std::numeric_limits<Flt64>::infinity();
	floatNegativeZero = -1.0f/floatPositiveInfinity;
	doubleNegativeZero = -1.0/doublePositiveInfinity;
	floatNaN = std::numeric_limits<Flt32>::quiet_NaN();
	doubleNaN = std::numeric_limits<Flt64>::quiet_NaN();
}

#elif defined __GNUC__
Flt32 floatPositiveInfinity;
Flt64 doublePositiveInfinity;
Flt32 floatNegativeInfinity;
Flt64 doubleNegativeInfinity;
Flt32 floatNaN;
Flt64 doubleNaN;

#ifdef IS_LITTLE_ENDIAN
#define DOUBLE_HI32(x)        (((uint32 *)&(x))[1])
#define DOUBLE_LO32(x)        (((uint32 *)&(x))[0])
#else
#define DOUBLE_HI32(x)        (((uint32 *)&(x))[0])
#define DOUBLE_LO32(x)        (((uint32 *)&(x))[1])
#endif
#define DOUBLE_HI32_SIGNBIT   0x80000000
#define DOUBLE_HI32_EXPMASK   0x7ff00000
#define DOUBLE_HI32_MANTMASK  0x000fffff

union dpun {
    struct {
#ifdef IS_LITTLE_ENDIAN
	uint32 lo, hi;
#else
	uint32 hi, lo;
#endif
    } s;
    Flt64 d;
};

union spun {
  uint32 s;
  Flt32 f;
};

struct DummyInit
{
  DummyInit() {
    union dpun du;
    union spun su;

    su.s = 0x7f800000;
    floatPositiveInfinity = su.f;
    
	du.s.hi = DOUBLE_HI32_EXPMASK;
	du.s.lo = 0x00000000;
	doublePositiveInfinity = du.d;

    su.s = 0xff800000;
    floatNegativeInfinity = su.f;
    
	du.s.hi = DOUBLE_HI32_SIGNBIT | DOUBLE_HI32_EXPMASK;
	du.s.lo = 0x00000000;
	doubleNegativeInfinity = du.d;

    su.s = 0x7f900000;
    floatNaN = su.f;

	du.s.hi = DOUBLE_HI32_EXPMASK | DOUBLE_HI32_MANTMASK;
	du.s.lo = 0xffffffff;
	doubleNaN = du.d;
  }
};

DummyInit dummyFloatInit;

#endif

// Wrapper around fmod() is necessary because some implementations doesn't
// handle infinities properly, e.g. MSVC.
double javaFMod(double dividend, double divisor) {
    if (isNaN(dividend) || isNaN(divisor) || isInfinite(dividend) || (divisor == 0.0))
        return doubleNaN;
    
    if ((dividend == 0.0) || isInfinite(divisor))
        return dividend;
   
	return fmod(dividend, divisor);
}

Int32 flt64ToInt32(Flt64 d)
{
    if (isNaN(d))
        return 0;
    if (d >= (Flt64)(Int32)0x7fffffff)
        return 0x7fffffff;
    if (d <= (Flt64)(Int32)0x80000000)
        return 0x80000000;
    return (Int32)d;
}

Int64 flt64ToInt64(Flt64 d)
{
    if (isNaN(d))
        return CONST64(0);
    if (d >= (Flt64)CONST64(0x7fffffffffffffff))
        return CONST64(0x7fffffffffffffff);
    if (d <= (Flt64)CONST64(0x8000000000000000))
        return CONST64(0x8000000000000000);
    return (Int64)d;
}

#ifdef DEBUG
template <class T>
void testFloat(T myNaN, T myNegZero, T myPosInf, T myNegInf)
{
	T plusZero = 0.0;
	T minusZero = myNegZero;
	T plusOne = 1.0;
	T plusInf = plusOne/plusZero;
	T minusInf = plusOne/minusZero;
	T nan = plusInf + minusInf;

	assert(plusZero == minusZero);
	assert(plusZero != plusOne);
	assert(plusInf > 0.0);
	assert(minusInf < 0.0);
	assert(!(nan == 0.0));
	assert(!(nan == nan));
	assert(!(nan < nan));
	assert(!(nan > nan));
	assert(!(myNaN == 0.0));
	assert(!(myNaN == myNaN));
	assert(!(myNaN < myNaN));
	assert(!(myNaN > myNaN));
	assert(plusInf == myPosInf);
	assert(minusInf == myNegInf);

	assert(isPositiveZero(plusZero));
	assert(!isNegativeZero(plusZero));
	assert(!isNaN(plusZero));
	assert(!isPositiveZero(minusZero));
	assert(isNegativeZero(minusZero));
	assert(!isNaN(minusZero));
	assert(!isPositiveZero(plusOne));
	assert(!isNegativeZero(plusOne));
	assert(!isNaN(plusOne));
	assert(!isPositiveZero(plusInf));
	assert(!isNegativeZero(plusInf));
	assert(!isNaN(plusInf));
	assert(!isPositiveZero(minusInf));
	assert(!isNegativeZero(minusInf));
	assert(!isNaN(minusInf));
	assert(!isPositiveZero(nan));
	assert(!isNegativeZero(nan));
	assert(isNaN(nan));
	assert(!isPositiveZero(myNaN));
	assert(!isNegativeZero(myNaN));
	assert(isNaN(myNaN));

	assert(isPositiveZero(plusZero + minusZero));
	assert(!isNegativeZero(plusZero + minusZero));
	assert(!isNaN(plusZero + minusZero));
}


static bool testGt(double a, double b)
{
	return a>b;
}

static bool testGe(double a, double b)
{
	return a>=b;
}

static bool testLt(double a, double b)
{
	return a<b;
}

static bool testLe(double a, double b)
{
	return a<=b;
}

//
// Test the definitions in FloatUtils.h.
//
void testFloatUtils()
{
	assert(!testGt(doubleNaN, doubleNaN));
	assert(!testGe(doubleNaN, doubleNaN));
	assert(!testLt(doubleNaN, doubleNaN));
	assert(!testLe(doubleNaN, doubleNaN));
	assert(!(doubleNaN > doubleNaN));
	assert(!(doubleNaN >= doubleNaN));
	assert(doubleNaN != doubleNaN);
	testFloat(floatNaN, floatNegativeZero, floatPositiveInfinity, floatNegativeInfinity);
	testFloat(doubleNaN, doubleNegativeZero, doublePositiveInfinity, doubleNegativeInfinity);
}
#endif
