/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef numerics_h
#define numerics_h

#include "systemtypes.h"
#include "utilities.h"
#include "strings.h"
#include "formatter.h"

// Use platform-defined floating-point routines.  On platforms with faulty
// floating-point code ifdef these out and replace by custom implementations.

#ifndef _WIN32
// Microsoft VC6 bug: standard identifiers should be in std namespace
using std::abs;
using std::floor;
using std::ceil;
using std::fmod;
using std::sqrt;
using std::sin;
using std::cos;
using std::tan;
using std::asin;
using std::acos;
using std::atan;
#endif

namespace JavaScript {

//
// Double-precision constants
//

    extern double positiveInfinity;
    extern double negativeInfinity;
    extern double nan;
    extern double minValue;
    extern double maxValue;

//
// Portable double-precision floating point to string and back conversions
//

    double ulp(double x);
    int hi0bits(uint32 x);

    class BigInt {
        enum {maxLgGrossSize = 15};     // Maximum value of lg2(grossSize)
        static uint32 *freeLists[maxLgGrossSize+1];

        uint lgGrossSize;               // lg2(grossSize)
    public:
        bool negative;                  // True if negative.  Ignored by most BigInt routines!
    private:
        uint32 grossSize;               // Number of words allocated for <words>
        uint32 size;                    // Actual number of words.  If the number is nonzero, the most significant word must be nonzero.
                                        // If the number is zero, then size is also 0.
        uint32 *words;                  // <size> words of the number, in little endian order
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
    // Some of the modes can round-trip; this means that if the number is
    // converted to a string using one of these mode and then converted back
    // to a number, the result will be identical to the original number (except
    // that, due to ECMA, -0 will get converted to +0).  These round-trip modes
    // return the minimum number of significand digits that permit the round
    // trip.
    //
    // Some of the modes take an integer parameter <precision>.
    //
    // Keep this in sync with doubleToAsciiModes[].
    enum DToStrMode {
        dtosStandard,               // Either fixed or exponential format; round-trip
        dtosStandardExponential,    // Always exponential format; round-trip
        dtosFixed,                  // Round to <precision> digits after the decimal point; exponential if number is large
        dtosExponential,            // Always exponential format; <precision> significant digits
        dtosPrecision               // Either fixed or exponential format; <precision> significant digits
    };


    // Maximum number of characters (including trailing null) that a
    // dtosStandard or dtosStandardExponential conversion can produce.  This
    // maximum is reached for a number like -1.2345678901234567e+123.
    const int dtosStandardBufferSize = 25;

    // Maximum number of characters (including trailing null) that one of the
    // other conversions can produce.  This maximum is reached for TO_FIXED,
    // which can generate up to 21 digits before the decimal point.
#define dtosVariableBufferSize(precision)       \
    ((precision)+24 > dtosStandardBufferSize ?  \
     (precision)+24 : dtosStandardBufferSize)   \

    // "-0.0000...(1073 zeros after decimal point)...0001\0" is the longest
    // string that we could produce, which occurs when printing -5e-324 in
    // binary.  We could compute a better estimate of the size of the output
    // string and malloc fewer bytes depending on d and base, but why bother?
    const int dtobasesBufferSize = 1078;

    double strToDouble(const char *str, const char *&numEnd);
    double stringToDouble(const char16 *str, const char16 *strEnd, const char16 *&numEnd);
    double stringToInteger(const char16 *str, const char16 *strEnd, const char16 *&numEnd, uint base);

    char *doubleToStr(char *buffer, size_t bufferSize, double value, DToStrMode mode, int precision);
    size_t doubleToBaseStr(char *buffer, double value, uint base);
    void appendDouble(String &dst, double value, DToStrMode mode = dtosStandard, int precision = 0);
    inline String &operator+=(String &s, double value) {appendDouble(s, value); return s;}
    void printDouble(Formatter &f, double value, DToStrMode mode = dtosStandard, int precision = 0);
    inline Formatter &operator<<(Formatter &f, double value) {printDouble(f, value); return f;}
}

#endif
