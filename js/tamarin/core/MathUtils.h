/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#ifndef __avmplus_MathUtils__
#define __avmplus_MathUtils__

#include "BigInteger.h"

namespace avmplus
{
#undef min
#undef max

	/**
	 * Data structure for state of fast random number generator.
	 */
	struct TRandomFast
	{
		/* Random result and seed for next random result. */
		uint32  uValue;

		/* XOR mask for generating the next random value. */
		uint32  uXorMask;

		/* This is the number of values which will be generated in the
		   /  sequence given the initial value of n. */
		uint32  uSequenceLength;
	}; 
	//
	// Random number generator
	//
	typedef struct TRandomFast *pTRandomFast;

	/**
	 * The pure random number generator returns random numbers between zero
	 * and this number.  This value is useful to know for scaling random
	 * numbers to a desired range.
	 */
	#define kRandomPureMax 0x7FFFFFFFL

	/**
	 * The MathUtils class is a utility class which supports many
	 * common mathematical operations, particularly those defined in
	 * ECMAScript's Math class.
	 */
	class MathUtils
	{
	public:
		static double abs(double value);
		static double acos(double value);
		static double asin(double value);
		static double atan(double value);						
		static double atan2(double y, double x);
		static double ceil(double value);								
		static double cos(double value);		
		static bool equals(double x, double y);
		static double exp(double value);
		static double floor(double value);
		static uint64  frexp(double x, int *eptr);
		static double infinity();
		static int isInfinite(double value);
		static bool isNaN(double value) { return value != value; }
		static bool isNegZero(double x);
		static bool isHexNumber(const wchar *ptr);
		static double log(double value);		
		static double max(double x, double y) { return (x > y) ? x : y; }
		static double min(double x, double y) { return (x < y) ? x : y; }
		static double mod(double x, double y);
		static double nan();
		static int nextPowerOfTwo(int n);
		static double parseInt(const wchar *s, int len, int radix=10, bool strict=true);
		static double pow(double x, double y);
		static double powInternal(double x, double y);
		static void initRandom(TRandomFast *seed);
		static double random(TRandomFast *seed);
		static double round(double value);
		static double sin(double value);
		static double sqrt(double value);
		static double tan(double value);
		static double toInt(double value);
		#ifdef WIN32
		// This routine will return 0x80000000 if the double value overflows
		// and integer and is not between -2^31 and 2^31-1. 
		static int32 real2int(double value);
		#else
		static int32 real2int(double val) { return (int32) val; }		
		#endif
		static bool convertIntegerToString(int value,
										   wchar *buffer,
										   int& len,
										   int radix=10,
										   bool treatAsUnsigned=false);
		static Stringp convertDoubleToStringRadix(AvmCore *core,
												  double value,
										          int radix);
		enum {
			DTOSTR_NORMAL,
			DTOSTR_FIXED,
			DTOSTR_PRECISION,
			DTOSTR_EXPONENTIAL
		};
		static void convertDoubleToString(double value,
										  wchar *buffer,
										  int &len,
										  int mode = DTOSTR_NORMAL,
										  int precision = 15);
		static bool convertStringToDouble(const wchar *s,
										  int len,
										  double *value,
										  bool strict=false);
		static double convertStringToNumber(const wchar* ptr, int strlen);
		static int nextDigit(double *value);

	private:
		static double powerOfTen(int exponent, double value);
		static wchar *handleSign(const wchar *s, bool& negative);
		static wchar *skipSpaces(const wchar *s);
		static int parseIntDigit(wchar ch);
		static int roundInt(double x);

		static void RandomFastInit(pTRandomFast pRandomFast);
		static sint32 RandomPureHasher(sint32 iSeed);
		static sint32 GenerateRandomNumber(pTRandomFast pRandomFast);
		static sint32 Random(sint32 range, pTRandomFast pRandomFast);

	};

	/*  D2A class implemention, used for converting double values to		   */
	/*   strings accurately.												   */

	// This algorithm for printing floating point numbers is based on the paper
	//  "Printing floating-point numbers quickly and accurately" by Robert G Burger
	//  and R. Kent Dybvig  http://www.cs.indiana.edu/~burger/FP-Printing-PLDI96.pdf
	//  See that paper for details.
	//
	//  The algorithm generates the shortest, correctly rounded output string that 
	//   converts to the same number when read back in.  This implementation assumes
	//   IEEE unbiased rounding, that the input is a 64 bit (base 2) floating point 
	//   number composed of a sign bit, a hidden bit (valued 1) and 52 explict bits of
	//   mantissa data, and an 11 bit exponent (in base 2).  It also assumes the output
	//   desired is base 10.
	class D2A {

	public:
		D2A(double value, int mode, int minPrecision=0);
		~D2A();

		int nextDigit();
		int expBase10() { return base10Exp; }

		double value;	   // double value for quick work when e and mantissa are small;
		int e;			   	   
		uint64 mantissa;   // on input, value = mantissa*2^e;  Only last 53 bits are used
		int mantissaPrec;  // how many bits of precision are actually used in the mantissa;
		int base10Exp;	   // the (derived) base 10 exponent of value.
		bool finished;	   // set to true when we've output all relevant digits.
		bool bFastEstimateOk; // if minPrecision < 16, use double, rather than BigInteger math

	private:
		int	 minPrecision;    // precision requested
		
		bool lowOk;    // for IEEE unbiased rounding, this is true when mantissa is even.  When true, use >= in mMinus test instead of >
		bool highOk;   //  ditto, but for mPlus test.

		// if !bFastEstimateOk, use these
		BigInteger r;	   // on initialization, input double <value> = r / s.  After each nextDigit() call, r = r % s
		BigInteger s; 
		BigInteger mPlus;  // when (r+mPlus) > s, we have generated all relevant digits.  Just return 0 for remaining nextDigit requests
		BigInteger mMinus; // when (r < mMins), we have generated all relevant digits.  Just return 0 form remaining nextDigit requests

		// if bFastEstimateOk, use these
		double      dr;    // same as above, but integer value stored in double
		double		ds;
		double      dMPlus;
		double		dMMinus;

		int scale();   // Estimate base 10 exponent of number, scale r,s,mPlus,mMinus appropriately.
					   //  Returns result of fixup_ExponentEstimate(est).
		int fixup_ExponentEstimate(int expEst); // Used by scale to adjust for possible off-by-one error 
											    //  in the base 10 exponent estimate.  Returns exact base10 exponent of number.
	
	};

}

#endif /* __avmplus__MathUtils__ */
