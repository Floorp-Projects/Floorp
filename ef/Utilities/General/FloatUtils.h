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

#ifndef FLOATUTILS_H
#define FLOATUTILS_H

#include "Fundamentals.h"

// Wrapper around fmod() is necessary because some implementations doesn't
// handle infinities properly, e.g. MSVC.
double javaFMod(double dividend, double divisor);

// Simple queries on IEEE floating-point values.
// It's amazing that the standard C headers don't have these operations
// standardized...

#ifdef WIN32
 // Microsoft's compiler is quite buggy when it comes to handling floating
 // point zeros and NaNs, so we need workarounds here...
 extern Flt32 floatPositiveInfinity;
 extern Flt64 doublePositiveInfinity;
 extern Flt32 floatNegativeInfinity;
 extern Flt64 doubleNegativeInfinity;
 extern Flt32 floatNegativeZero;
 extern Flt64 doubleNegativeZero;
 extern Flt32 floatNaN;
 extern Flt64 doubleNaN;
#elif defined __GNUC__
 // GCC is also broken when it comes to immediate floating point expressions.
 extern Flt32 floatPositiveInfinity;
 extern Flt64 doublePositiveInfinity;
 extern Flt32 floatNegativeInfinity;
 extern Flt64 doubleNegativeInfinity;
 const Flt32 floatNegativeZero = -0.0f;
 const Flt64 doubleNegativeZero = -0.0;
 extern Flt32 floatNaN;
 extern Flt64 doubleNaN;
#else
 const Flt32 floatPositiveInfinity = 1.0f/0.0f;
 const Flt64 doublePositiveInfinity = 1.0/0.0;
 const Flt32 floatNegativeInfinity = -1.0f/0.0f;
 const Flt64 doubleNegativeInfinity = -1.0/0.0;
 const Flt32 floatNegativeZero = -0.0f;
 const Flt64 doubleNegativeZero = -0.0;
 const Flt32 floatNaN = 0.0f/0.0f;
 const Flt64 doubleNaN = 0.0/0.0;
#endif

inline bool isPositiveZero(Flt32 x);
inline bool isPositiveZero(Flt64 x);
inline bool isNegativeZero(Flt32 x);
inline bool isNegativeZero(Flt64 x);
inline bool isNaN(Flt32 x);
inline bool isNaN(Flt64 x);

DEBUG_ONLY(void testFloatUtils();)

// Conversions between integral and floating point types
// These may need to be defined specially on platforms that handle rounding
// or out-of-range values weirdly.

// Rounds to nearest when result is inexact.
inline Flt32 int32ToFlt32(Int32 i) {return (Flt32)i;}
inline Flt64 int32ToFlt64(Int32 i) {return (Flt64)i;}
inline Flt32 int64ToFlt32(Int64 l) {return (Flt32)l;}
inline Flt64 int64ToFlt64(Int64 l) {return (Flt64)l;}

// Ordinary values round towards zero.
// NaN becomes zero.
// -inf or values below -2^31 (or -2^63) become -2^31 (or -2^63).
// +inf or values above 2^31-1 (or 2^63-1) become 2^31-1 (or 2^63-1).
extern Int32 flt64ToInt32(Flt64 d);
extern Int64 flt64ToInt64(Flt64 d);
inline Int32 flt32ToInt32(Flt32 f) {return flt64ToInt32((Flt64)f);}
inline Int64 flt32ToInt64(Flt32 f) {return flt64ToInt64((Flt64)f);}



// --- INLINES ----------------------------------------------------------------


inline bool isPositiveZero(Flt32 x)
	{return *(Uint32 *)&x == 0x00000000;}

inline bool isPositiveZero(Flt64 x)
	{return *(Uint64 *)&x == CONST64(0x0000000000000000);}

inline bool isNegativeZero(Flt32 x)
	{return *(Uint32 *)&x == 0x80000000;}

inline bool isNegativeZero(Flt64 x)
	{return *(Uint64 *)&x == CONST64(0x8000000000000000);}

inline bool isNaN(Flt32 x)
	{return (~(*(Uint32 *)&x) & 0x7fc00000) == 0;}

inline bool isNaN(Flt64 x)
	{return (~(*(Uint64 *)&x) & CONST64(0x7ff8000000000000)) == 0;}

inline bool isInfinite(Flt32 v)
    {return v == floatPositiveInfinity || v == floatNegativeInfinity;}

inline bool isInfinite(Flt64 v)
    {return v == doublePositiveInfinity || v == doubleNegativeInfinity;}

#endif
