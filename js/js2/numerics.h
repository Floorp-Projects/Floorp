// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef numerics_h
#define numerics_h

#include "utilities.h"
#include <cmath>
// Use platform-defined floating-point routines.  On platforms with faulty floating-point code
// ifdef these out and replace by custom implementations.
using std::abs;
using std::floor;
using std::ceil;
using std::sqrt;
using std::sin;
using std::cos;
using std::tan;
using std::asin;
using std::acos;
using std::atan;

namespace JavaScript {

//
// Portable double-precision floating point to string and back conversions
//

	double ulp(double x);
	int hi0bits(uint32 x);

	class BigInt {
		static const int maxLgGrossSize = 15;	// Maximum value of lg2(grossSize)
		static uint32 *freeLists[maxLgGrossSize+1];
		
	    uint lgGrossSize;				// lg2(grossSize)
	  public:
	    bool negative;					// True if negative.  Ignored by most BigInt routines!
	  private:
	    uint32 grossSize;				// Number of words allocated for <words>
	    uint32 size;					// Actual number of words.  If the number is nonzero, the most significant word must be nonzero.
	    								// If the number is zero, then size is also 0.
	    uint32 *words;					// <size> words of the number, in little endian order
	    
	    void allocate(uint lgGrossSize);
	    void recycle();
	    void initCopy(const BigInt &b);
	    void move(BigInt &b);
	  public:
	    BigInt(): words(0) {}
	    explicit BigInt(uint lgGrossSize) {allocate(lgGrossSize);}
	    BigInt(const BigInt &b) {initCopy(b);}
		void operator=(const BigInt &b) {ASSERT(!words); initCopy(b);}
	    ~BigInt() {if (words) recycle();}
	    
	    void setLgGrossSize(uint lgGrossSize);
	    void init(uint32 i);
		void init(double d, int32 &e, int32 &bits);
		void mulAdd(uint m, uint a);
		void operator*=(const BigInt &m);
		void pow2Mul(int32 k);
		void pow5Mul(int32 k);
		bool isZero() const {ASSERT(words); return !size;}
		int cmp(const BigInt &b) const;
		void initDiff(const BigInt &m, const BigInt &n);
		uint32 quoRem2(int32 k);
		int32 quoRem(const BigInt &S);
		uint32 divRem(uint32 divisor);
		double b2d(int32 &e) const;
		double ratio(const BigInt &denominator) const;
		void s2b(const char *s, int32 nd0, int32 nd, uint32 y9);
		
		uint32 nWords() const {return size;}
		uint32 word(uint32 i) const {ASSERT(i < size); return words[i];}
	};


	// Modes for converting floating-point numbers to strings.
	//
	// Some of the modes can round-trip; this means that if the number is converted to
	// a string using one of these mode and then converted back to a number, the result
	// will be identical to the original number (except that, due to ECMA, -0 will get converted
	// to +0).  These round-trip modes return the minimum number of significand digits that
	// permit the round trip.
	//
	// Some of the modes take an integer parameter <precision>.
	//
	// Keep this in sync with number_constants[].
	enum DToStrMode {
	    DTOSTR_STANDARD,				// Either fixed or exponential format; round-trip
	    DTOSTR_STANDARD_EXPONENTIAL,	// Always exponential format; round-trip
	    DTOSTR_FIXED,					// Round to <precision> digits after the decimal point; exponential if number is large
	    DTOSTR_EXPONENTIAL,				// Always exponential format; <precision> significant digits
	    DTOSTR_PRECISION				// Either fixed or exponential format; <precision> significant digits
	};


	// Maximum number of characters (including trailing null) that a DTOSTR_STANDARD or DTOSTR_STANDARD_EXPONENTIAL
	// conversion can produce.  This maximum is reached for a number like -1.2345678901234567e+123.
	const int DTOSTR_STANDARD_BUFFER_SIZE = 25;

	// Maximum number of characters (including trailing null) that one of the other conversions
	// can produce.  This maximum is reached for TO_FIXED, which can generate up to 21 digits before the decimal point.
	#define DTOSTR_VARIABLE_BUFFER_SIZE(precision) ((precision)+24 > DTOSTR_STANDARD_BUFFER_SIZE ? (precision)+24 : DTOSTR_STANDARD_BUFFER_SIZE)

	// "-0.0000...(1073 zeros after decimal point)...0001\0" is the longest string that we could produce,
	// which occurs when printing -5e-324 in binary.  We could compute a better estimate of the size of
	// the output string and malloc fewer bytes depending on d and base, but why bother?
	const int DTOBASESTR_BUFFER_SIZE = 1078;

	double strToD(const char *s00, char **se);
	char *dToStr(char *buffer, size_t bufferSize, DToStrMode mode, int precision, double dval);
	void dToBaseStr(char *buffer, uint base, double d);

}
#endif
