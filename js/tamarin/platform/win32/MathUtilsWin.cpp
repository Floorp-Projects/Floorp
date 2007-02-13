/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <math.h>

#include "avmplus.h"

#define X86_MATH

namespace avmplus
{
	double MathUtils::abs(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fabs;
#else
		return ::fabs(value);
#endif /* X86_MATH */
	}
	
	double MathUtils::acos(double value)
	{
#ifdef X86_MATH
		return MathUtils::atan2(MathUtils::sqrt(1.0-value*value), value);
#else
		return ::acos(value);
#endif /* X86_MATH */
	}
	
	double MathUtils::asin(double value)
	{
#ifdef X86_MATH
		return MathUtils::atan2(value, MathUtils::sqrt(1.0-value*value));
#else
		return ::asin(value);
#endif /* X86_MATH */
	}
	
	double MathUtils::atan(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fld1;
		_asm fpatan;
#else
		return ::atan(value);
#endif /* X86_MATH */
	}

	double MathUtils::atan2(double y, double x)
	{
#ifdef X86_MATH
		_asm fld [y];
		_asm fld [x];
		_asm fpatan;
#else
		return ::atan2(y, x);
#endif /* X86_MATH */
	}
	
	double MathUtils::ceil(double value)
	{
#ifdef X86_MATH
		// todo avoid control word modification
		short oldcw, newcw;
		_asm fnstcw [oldcw];
		_asm mov ax, [oldcw];
		_asm and ax, 0xf3ff; // Set to round down.
		_asm or  ax, 0x800;
		_asm mov [newcw], ax;
		_asm fldcw [newcw];
		_asm fld [value];
		_asm frndint;
		_asm fldcw [oldcw];
#else
		return ::ceil(value);
#endif /* X86_MATH */
	}

	double MathUtils::cos(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fcos;
#else
		return ::cos(value);
#endif /* X86_MATH */
	}
	
#ifdef X86_MATH
	// Utility function, this module only.
	static double expInternal(double x)
	{
		double value, exponent;
		_asm fld [x];
		_asm fldl2e;
		_asm _emit 0xD8; // fmul st(1);
		_asm _emit 0xC9;
		_asm _emit 0xDD; // fst st(1);
		_asm _emit 0xD1;
		_asm frndint;
		_asm fxch;
		_asm _emit 0xD8; // fsub st1;
		_asm _emit 0xE1;
		_asm f2xm1;
		_asm fstp [value];
		_asm fstp [exponent];

		value += 1.0;

		_asm fld [exponent];
		_asm fld [value];
		_asm fscale;

		_asm fxch;
		_asm _emit 0xDD; // fstp st(0);
		_asm _emit 0xD8;
	}
#endif /* X86_MATH */
	
	double MathUtils::exp(double value)
	{
#ifdef X86_MATH
		switch (isInfinite(value)) {
		case 1:
			return infinity();
		case -1:
			return +0;
		default:
			return expInternal(value);
		}
#else
		return ::exp(value);
#endif /* X86_MATH */
	}

	double MathUtils::floor(double value)
	{
#ifdef X86_MATH
		// todo avoid control word modification
		short oldcw, newcw;
		_asm fnstcw [oldcw];
		_asm mov ax, [oldcw];
		_asm and ax, 0xf3ff; // Set to round down.
		_asm or  ax, 0x400;
		_asm mov [newcw], ax;
		_asm fldcw [newcw];
		_asm fld [value];
		_asm frndint;
		_asm fldcw [oldcw];
#else
		return ::floor(value);
#endif /* X86_MATH */
	}

#ifdef X86_MATH
	/* @(#)s_frexp.c 5.1 93/09/24 */
	/*
	 * ====================================================
	 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
	 *
	 * Developed at SunPro, a Sun Microsystems, Inc. business.
	 * Permission to use, copy, modify, and distribute this
	 * software is freely granted, provided that this notice
	 * is preserved.
	 * ====================================================
	 */

	/*
	 * for non-zero x
	 *	x = frexp(arg,&exp);
	 * return a double fp quantity x such that 0.5 <= |x| <1.0
	 * and the corresponding binary exponent "exp". That is
	 *	arg = x*2^exp.
	 * If arg is inf, 0.0, or NaN, then frexp(arg,&exp) returns arg
	 * with *exp=0.
	 */

	/*
	 * NOTE: This is little-endian, must be adjusted to work for
	 * big-endian systems.
	 */
#define EXTRACT_WORDS(hx, lx, x) {DWORD *ptr = (DWORD*)&x; hx=ptr[1]; lx=ptr[0];}
#define SET_HIGH_WORD(x, hx) {DWORD *ptr = (DWORD*)&x; ptr[1]=hx;}
#define GET_HIGH_WORD(hx, x) {DWORD *ptr = (DWORD*)&x; hx=ptr[1];}
	static const double two54 =  1.80143985094819840000e+16; /* 0x43500000, 0x00000000 */

	static double ExtractFraction(double x, int *eptr)
	{
		DWORD hx, ix, lx;
		EXTRACT_WORDS(hx,lx,x);
		ix = 0x7fffffff&hx;
		*eptr = 0;
		if(ix>=0x7ff00000||((ix|lx)==0)) return x;	/* 0,inf,nan */
		if (ix<0x00100000) {		/* subnormal */
			x *= two54;
			GET_HIGH_WORD(hx,x);
			ix = hx&0x7fffffff;
			*eptr = -54;
		}
		*eptr += (ix>>20)-1022;
		hx = (hx&0x800fffff)|0x3fe00000;
		SET_HIGH_WORD(x,hx);
		return x;
	}

	uint64 MathUtils::frexp(double x, int *eptr)
	{
		double fracMantissa = ExtractFraction(x, eptr);
		// correct mantissa and eptr to get integer values
		//  for both
		*eptr -= 53; // 52 mantissa bits + the hidden bit
		return (uint64)((fracMantissa) * (double)(1LL << 53));
	}
#else
	double MathUtils::frexp(double x, int *eptr)
	{
		return ::frexp(x, eptr);
	}
#endif /* X86_MATH */
	
	double MathUtils::log(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fldln2;
		_asm fxch;
		_asm fyl2x;
#else
		return ::log(value);
#endif /* X86_MATH */
	}

#ifdef X86_MATH
	// utility function for mod
	static double modInternal(double x, double y)
	{
		_asm    fld [y];
		_asm    fld [x];
	  ModLoop:
		_asm    fprem;
		_asm    fnstsw ax;
		_asm    sahf;
		_asm    jp ModLoop;
		_asm _emit 0xDD; // fstp st(1);
		_asm _emit 0xD9;
	}
#endif /* X86_MATH */
	
	double MathUtils::mod(double x, double y)
	{
#ifdef X86_MATH
		if (!y) {
			return nan();
		}
		return modInternal(x, y);
#else
		return ::fmod(x, y);
#endif /* X86_MATH */
	}

	// Std. library pow()
	double MathUtils::powInternal(double x, double y)
	{
#ifdef X86_MATH
		double value, exponent;

		_asm fld1;
		_asm fld [x];
		_asm fyl2x;
		_asm fstp [value];

		_asm fld [value];
		_asm fld [y];
		_asm _emit 0xD8; // fmul st(1);
		_asm _emit 0xC9;
		_asm _emit 0xDD; // fst st(1);
		_asm _emit 0xD1;
		_asm frndint;
		_asm fxch;
		_asm _emit 0xD8; // fsub st1;
		_asm _emit 0xE1;
		_asm f2xm1;
		_asm fstp [value];
		_asm fstp [exponent];

		value += 1.0;

		_asm fld [exponent];
		_asm fld [value];
		_asm fscale;

		_asm fxch;
		_asm _emit 0xDD; // fstp st(0);
		_asm _emit 0xD8;
#else
		return ::pow(x, y);
#endif /* X86_MATH */
	}
	
	double MathUtils::sin(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fsin;
#else
		return ::sin(value);
#endif /* X86_MATH */
	}

	double MathUtils::sqrt(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fsqrt;		
#else
		return ::sqrt(value);
#endif /* X86_MATH */
	}

	double MathUtils::tan(double value)
	{
#ifdef X86_MATH
		_asm fld [value];
		_asm fptan;
		_asm _emit 0xDD; // fstp st(0);
		_asm _emit 0xD8;
#else
		return ::tan(value);
#endif /* X86_MATH */
	}

	int32 MathUtils::real2int(double value)
	{
		uint16 oldcw, newcw;
		int32 intval;
		_asm fnstcw [oldcw];
		_asm mov ax,[oldcw];
		_asm or ax,0xc3f;
		_asm mov [newcw],ax;
		_asm fldcw [newcw];
		_asm fld [value];
		_asm fistp [intval];
		_asm fldcw [oldcw];
		return intval;
	}
}
