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
* Communications Corporation.   Portions created by Netscape are
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

#include <cstdlib>
#include <cstring>
#include <cfloat>
#include "numerics.h"
#include "parser.h"
#include "js2runtime.h"

#include "fdlibm_ns.h"

namespace JS = JavaScript;
using namespace JS;
using namespace JS::JS2Runtime;

//
// Portable double-precision floating point to string and back conversions
//

// ****************************************************************
//
// The author of this software is David M. Gay.
//
// Copyright (c) 1991 by Lucent Technologies.
//
// Permission to use, copy, modify, and distribute this software for any
// purpose without fee is hereby granted, provided that this entire notice
// is included in all copies of any software which is or includes a copy
// or modification of this software and in all copies of the supporting
// documentation for such software.
//
// THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
// WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
// REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
// OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
//
// ***************************************************************

/* Please send bug reports to
David M. Gay
Bell Laboratories, Room 2C-463
600 Mountain Avenue
Murray Hill, NJ 07974-0636
U.S.A.
dmg@bell-labs.com
*/

// On a machine with IEEE extended-precision registers, it is
// necessary to specify double-precision (53-bit) rounding precision
// before invoking strToDouble or doubleToAscii.  If the machine uses (the equivalent
// of) Intel 80x87 arithmetic, the call
//  _control87(PC_53, MCW_PC);
// does this with many compilers.  Whether this or another call is
// appropriate depends on the compiler; for this to work, it may be
// necessary to #include "float.h" or another system-dependent header
// file.


// strToDouble for IEEE-arithmetic machines.
//
// This strToDouble returns a nearest machine number to the input decimal
// string.  With IEEE arithmetic, ties are broken by the IEEE round-even
// rule.  Otherwise ties are broken by biased rounding (add half and chop).
//
// Inspired loosely by William D. Clinger's paper "How to Read Floating
// Point Numbers Accurately" [Proc. ACM SIGPLAN '90, pp. 92-101].
//
// Modifications:
//
//  1. We only require IEEE double-precision
//      arithmetic (not IEEE double-extended).
//  2. We get by with floating-point arithmetic in a case that
//      Clinger missed -- when we're computing d * 10^n
//      for a small integer d and the integer n is not too
//      much larger than 22 (the maximum integer k for which
//      we can represent 10^k exactly), we may be able to
//      compute (d*10^k) * 10^(e-k) with just one roundoff.
//  3. Rather than a bit-at-a-time adjustment of the binary
//      result in the hard case, we use floating-point
//      arithmetic to determine the adjustment to within
//      one bit; only in really hard cases do we need to
//      compute a second residual.
//  4. Because of 3., we don't need a large table of powers of 10
//      for ten-to-e (just some small tables, e.g. of 10^k
//      for 0 <= k <= 22).


// #define IEEE_8087 for IEEE-arithmetic machines where the least
//  significant byte has the lowest address.
// #define IEEE_MC68k for IEEE-arithmetic machines where the most
//  significant byte has the lowest address.
// #define Sudden_Underflow for IEEE-format machines without gradual
//  underflow (i.e., that flush to zero on underflow).
// #define No_leftright to omit left-right logic in fast floating-point
//  computation of doubleToAscii.
// #define Check_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3.
// #define ROUND_BIASED for IEEE-format with biased rounding.
// #define Inaccurate_Divide for IEEE-format with correctly rounded
//  products but inaccurate quotients, e.g., for Intel i860.
// #define NATIVE_INT64 on machines that have "long long"
//  64-bit integer types int64 and uint64.
// #define JS_THREADSAFE if the system offers preemptively scheduled
//  multiple threads.  In this case, you must provide (or suitably
//  #define) two locks, acquired by ACQUIRE_DTOA_LOCK(n) and freed
//  by FREE_DTOA_LOCK(n) for n = 0 or 1.  (The second lock, accessed
//  in pow5Mul, ensures lazy evaluation of only one copy of high
//  powers of 5; omitting this lock would introduce a small
//  probability of wasting memory, but would otherwise be harmless.)
//  You must also invoke freeDToA(s) to free the value s returned by
//  doubleToAscii.  You may do so whether or not JS_THREADSAFE is #defined.
// #define NO_IEEE_Scale to disable new (Feb. 1997) logic in strToDouble that
//  avoids underflows on inputs whose result does not underflow.

#ifdef IS_LITTLE_ENDIAN
# define IEEE_8087
#else
# define IEEE_MC68k
#endif


// Stefan Hanske <sh990154@mail.uni-greifswald.de> reports:
//  ARM is a little endian architecture but 64 bit double words are stored
// differently: the 32 bit words are in little endian byte order, the two words
// are stored in big endian`s way.
#if defined (IEEE_8087) && !defined(__arm)
# define word0(x) ((uint32 *)&x)[1]
# define word1(x) ((uint32 *)&x)[0]
#else
# define word0(x) ((uint32 *)&x)[0]
# define word1(x) ((uint32 *)&x)[1]
#endif

// The following definition of Storeinc is appropriate for MIPS processors.
// An alternative that might be better on some machines is
// #define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
#if defined(IEEE_8087)
#define Storeinc(a,b,c) (((unsigned short *)a)[1] = (unsigned short)b, \
((unsigned short *)a)[0] = (unsigned short)c, a++)
#else
#define Storeinc(a,b,c) (((unsigned short *)a)[0] = (unsigned short)b, \
((unsigned short *)a)[1] = (unsigned short)c, a++)
#endif

// #define P DBL_MANT_DIG
// Ten_pmax = floor(P*log(2)/log(5))
// Bletch = (highest power of 2 < DBL_MAX_10_EXP) / 16
// Quick_max = floor((P-1)*log(FLT_RADIX)/log(10) - 1)
// Int_max = floor(P*log(FLT_RADIX)/log(10) - 1)

#define Exp_shift  20
#define Exp_shift1 20
#define Exp_msk1    0x100000
#define Exp_msk11   0x100000
#define Exp_mask  0x7ff00000
#define P 53
#define Bias 1023
#define Emin (-1022)
#define Exp_1  0x3ff00000
#define Exp_11 0x3ff00000
#define Ebits 11
#define Frac_mask  0xfffff
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Bletch 0x10
#define Bndry_mask  0xfffff
#define Bndry_mask1 0xfffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 1
#define Tiny0 0
#define Tiny1 1
#define Quick_max 14
#define Int_max 14
#define Infinite(x) (word0(x) == 0x7ff00000) // sufficient test for here
#ifndef NO_IEEE_Scale
# define Avoid_Underflow
#endif

#define Big0 (Frac_mask1 | Exp_msk1*(DBL_MAX_EXP+Bias-1))
#define Big1 0xffffffff

#ifdef JS_THREADSAFE
static PRLock *freelist_lock;
# define ACQUIRE_DTOA_LOCK(n) PR_Lock(freelist_lock)
# define FREE_DTOA_LOCK(n) PR_Unlock(freelist_lock)
#else
# define ACQUIRE_DTOA_LOCK(n)
# define FREE_DTOA_LOCK(n)
#endif


//
// Double-precision constants
//


    double JS::positiveInfinity;
    double JS::negativeInfinity;
    double JS::nan;
    double JS::minValue;
    double JS::maxValue;


struct InitNumerics {InitNumerics();};
static InitNumerics initNumerics;

InitNumerics::InitNumerics()
{
    word0(JS::positiveInfinity) = Exp_mask;
    word1(JS::positiveInfinity) = 0;
    word0(JS::negativeInfinity) = Exp_mask | Sign_bit;
    word1(JS::negativeInfinity) = 0;
    word0(JS::nan) = 0x7FFFFFFF;
    word1(JS::nan) = 0xFFFFFFFF;
    word0(JS::minValue) = 0;
    word1(JS::minValue) = 1;
    JS::maxValue = 1.7976931348623157E+308;
}


// had to move these here since they depend upon the values
// initialized above, and we can't guarantee order other than
// lexically in a single file.
JSValue JS::JS2Runtime::kUndefinedValue;
JSValue JS::JS2Runtime::kNaNValue = JSValue(nan);
JSValue JS::JS2Runtime::kTrueValue = JSValue(true);
JSValue JS::JS2Runtime::kFalseValue = JSValue(false);
JSValue JS::JS2Runtime::kNullValue = JSValue(JSValue::null_tag);
JSValue JS::JS2Runtime::kNegativeZero = JSValue(-0.0);
JSValue JS::JS2Runtime::kPositiveZero = JSValue(0.0);
JSValue JS::JS2Runtime::kNegativeInfinity = JSValue(negativeInfinity);
JSValue JS::JS2Runtime::kPositiveInfinity = JSValue(positiveInfinity);

//
// Portable double-precision floating point to string and back conversions
//


// Return the absolute difference between x and the adjacent greater-magnitude
// double number (ignoring exponent overflows).
double JS::ulp(double x)
{
    int32 L;
    double a;

    L = int32((word0(x) & Exp_mask) - (P-1)*Exp_msk1);
#ifndef Sudden_Underflow
    if (L > 0) {
#endif
        word0(a) = uint32(L);
        word1(a) = 0;
#ifndef Sudden_Underflow
    }
    else {
        L = -L >> Exp_shift;
        if (L < Exp_shift) {
            word0(a) = 0x80000u >> L;
            word1(a) = 0;
        }
        else {
            word0(a) = 0;
            L -= Exp_shift;
            word1(a) = L >= 31 ? 1u : 1u << (31 - L);
        }
    }
#endif
    return a;
}


// Return the number (0 through 32) of most significant zero bits in x.
int JS::hi0bits(uint32 x)
{
    int k = 0;

    if (!(x & 0xffff0000)) {
        k = 16;
        x <<= 16;
    }
    if (!(x & 0xff000000)) {
        k += 8;
        x <<= 8;
    }
    if (!(x & 0xf0000000)) {
        k += 4;
        x <<= 4;
    }
    if (!(x & 0xc0000000)) {
        k += 2;
        x <<= 2;
    }
    if (!(x & 0x80000000)) {
        k++;
        if (!(x & 0x40000000))
            return 32;
    }
    return k;
}


// Return the number (0 through 32) of least significant zero bits in y.
// Also shift y to the right past these 0 through 32 zeros so that y's
// least significant bit will be set unless y was originally zero.
static int lo0bits(uint32 &y)
{
    int k;
    uint32 x = y;

    if (x & 7) {
        if (x & 1)
            return 0;
        if (x & 2) {
            y = x >> 1;
            return 1;
        }
        y = x >> 2;
        return 2;
    }
    k = 0;
    if (!(x & 0xffff)) {
        k = 16;
        x >>= 16;
    }
    if (!(x & 0xff)) {
        k += 8;
        x >>= 8;
    }
    if (!(x & 0xf)) {
        k += 4;
        x >>= 4;
    }
    if (!(x & 0x3)) {
        k += 2;
        x >>= 2;
    }
    if (!(x & 1)) {
        k++;
        x >>= 1;
        if (!x & 1)
            return 32;
    }
    y = x;
    return k;
}


uint32 *JS::BigInt::freeLists[maxLgGrossSize+1];


// Allocate a BigInt with 2^lgGrossSize words.  The BigInt must not currently
// contain a number.
void JS::BigInt::allocate(uint lgGrossSize)
{
    ASSERT(lgGrossSize <= maxLgGrossSize);

    BigInt::lgGrossSize = lgGrossSize;
    negative = false;
    grossSize = 1u << lgGrossSize;
    size = 0;
    words = 0;
    ACQUIRE_DTOA_LOCK(0);
    uint32 *w = freeLists[lgGrossSize];
    if (w) {
        freeLists[lgGrossSize] = *reinterpret_cast<uint32 **>(w);
        FREE_DTOA_LOCK(0);
    } else {
        FREE_DTOA_LOCK(0);
        w = static_cast<uint32 *>(STD::malloc(max(uint32(grossSize * sizeof(uint32)), uint32(sizeof(uint32 *)))));
        if (!w) {
            std::bad_alloc outOfMemory;
            throw outOfMemory;
        }
    }
    words = w;
}


// Recycle this BigInt's words array, which must be non-nil.
void JS::BigInt::recycle()
{
    ASSERT(words);
    uint bucket = lgGrossSize;
    uint32 *w = words;
    ACQUIRE_DTOA_LOCK(0);
    *reinterpret_cast<uint32 **>(w) = freeLists[bucket];
    freeLists[bucket] = w;
    FREE_DTOA_LOCK(0);
}


// Copy b into this BigInt, which must be uninitialized.
void JS::BigInt::initCopy(const BigInt &b)
{
    allocate(b.lgGrossSize);
    negative = b.negative;
    size = b.size;
    std::copy(b.words, b.words+size, words);
}


// Move b into this BigInt.  The original words array of this BigInt is recycled
// and must be non-nil.  After the move b has no value.
void JS::BigInt::move(BigInt &b)
{
    recycle();
    lgGrossSize = b.lgGrossSize;
    negative = b.negative;
    grossSize = b.grossSize;
    size = b.size;
    words = b.words;
    b.words = 0;
}


// Change this BigInt's lgGrossSize to the given value without affecting the
// BigInt value contained.  This BigInt must currently have a value that will
// fit in the new lgGrossSize.
void JS::BigInt::setLgGrossSize(uint lgGrossSize)
{
    ASSERT(words);
    BigInt b(lgGrossSize);
    ASSERT(size <= b.grossSize);
    std::copy(words, words+size, b.words);
    move(b);
}


// Set the BigInt to the given integer value.
// The BigInt must not currently have a value.
void JS::BigInt::init(uint32 i)
{
    ASSERT(!words);
    allocate(1);    // Allocate two words to allow a little room for growth.
    if (i) {
        size = 1;
        words[0] = i;
    } else
        size = 0;
}


// Convert d into the form b*2^e, where b is an odd integer.  b is the BigInt
// returned in this, and e is the returned binary exponent.  Return the number
// of significant bits in b in bits.  d must be finite and nonzero.
void JS::BigInt::init(double d, int32 &e, int32 &bits)
{
    allocate(1);

    uint32 *x = words;
    uint32 z = word0(d) & Frac_mask;
    word0(d) &= 0x7fffffff;   // clear sign bit, which we ignore
    int32 de = (int32)(word0(d) >> Exp_shift);
#ifdef Sudden_Underflow
    z |= Exp_msk11;
#else
    if (de)
        z |= Exp_msk1;
#endif
    uint32 y = word1(d);
    int k;
    int i;
    if (y) {
        if ((k = lo0bits(y)) != 0) {
            x[0] = y | z << (32 - k);
            z >>= k;
        }
        else
            x[0] = y;
        i = (x[1] = z) ? 2 : 1;
    } else {
        ASSERT(z);
        k = lo0bits(z);
        x[0] = z;
        i = 1;
        k += 32;
    }
    size = uint32(i);

#ifndef Sudden_Underflow
    if (de) {
#endif
        e = de - Bias - (P-1) + k;
        bits = P - k;
#ifndef Sudden_Underflow
    } else {
        e = de - Bias - (P-1) + 1 + k;
        bits = 32*i - hi0bits(x[i-1]);
    }
#endif
}


// Let this = this*m + a.  Both a and m must be between 0 and 65535 inclusive.
void JS::BigInt::mulAdd(uint m, uint a)
{
#ifdef NATIVE_INT64
    uint64 carry, y;
#else
    uint32 carry, y;
    uint32 xi, z;
#endif

    ASSERT(m <= 0xFFFF && a <= 0xFFFF);
    uint32 sz = size;
    uint32 *x = words;
    carry = a;
    for (uint32 i = 0; i != sz; i++) {
#ifdef NATIVE_INT64
        y = *x * (uint64)m + carry;
        carry = y >> 32;
        *x++ = (uint32)(y & 0xffffffffUL);
#else
        xi = *x;
        y = (xi & 0xffff) * m + carry;
        z = (xi >> 16) * m + (y >> 16);
        carry = z >> 16;
        *x++ = (z << 16) + (y & 0xffff);
#endif
    }
    if (carry) {
        if (sz >= grossSize)
            setLgGrossSize(lgGrossSize+1);
        words[sz++] = (uint32)carry;
        size = sz;
    }
}


// Let this = this*m.
void JS::BigInt::operator*=(const BigInt &m)
{
    const BigInt *a;
    const BigInt *b;
    if (size >= m.size) {
        a = this;
        b = &m;
    } else {
        a = &m;
        b = this;
    }

    uint k = a->lgGrossSize;
    uint32 wa = a->size;
    uint32 wb = b->size;
    uint32 wc = wa + wb;
    if (wc > a->grossSize)
        k++;

    BigInt c(k);
    uint32 *xc;
    uint32 *xce;
    for (xc = c.words, xce = xc + wc; xc < xce; xc++)
        *xc = 0;
    const uint32 *xa = a->words;
    const uint32 *xae = xa + wa;
    const uint32 *xb = b->words;
    const uint32 *xbe = xb + wb;
    uint32 *xc0 = c.words;
    uint32 y;
#ifdef NATIVE_INT64
    for (; xb < xbe; xc0++) {
        if ((y = *xb++) != 0) {
            const uint32 *x = xa;
            xc = xc0;
            uint64 carry = 0;
            do {
                uint64 z = *x++ * (uint64)y + *xc + carry;
                carry = z >> 32;
                *xc++ = (uint32)(z & 0xffffffffUL);
            } while (x < xae);
            *xc = (uint32)carry;
        }
    }
#else
    for (; xb < xbe; xb++, xc0++) {
        uint32 carry;
        uint32 z, z2;
        const uint32 *x;

        if ((y = *xb & 0xffff) != 0) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = (*x & 0xffff) * y + (*xc & 0xffff) + carry;
                carry = z >> 16;
                z2 = (*x++ >> 16) * y + (*xc >> 16) + carry;
                carry = z2 >> 16;
                Storeinc(xc, z2, z);
            } while (x < xae);
            *xc = carry;
        }
        if ((y = *xb >> 16) != 0) {
            x = xa;
            xc = xc0;
            carry = 0;
            z2 = *xc;
            do {
                z = (*x & 0xffff) * y + (*xc >> 16) + carry;
                carry = z >> 16;
                Storeinc(xc, z, z2);
                z2 = (*x++ >> 16) * y + (*xc & 0xffff) + carry;
                carry = z2 >> 16;
            } while (x < xae);
            *xc = z2;
        }
    }
#endif
    for (xc0 = c.words, xc = xc0 + wc; wc && !*--xc; --wc) ;
    c.size = wc;
    move(c);
}


// Let this = this * 2^k.  k must be nonnegative.
void JS::BigInt::pow2Mul(int32 k)
{
    ASSERT(k >= 0);

    uint32 n = uint32(k) >> 5;
    uint k1 = lgGrossSize;
    uint32 n1 = n + size + 1;
    uint32 i;
    for (i = grossSize; n1 > i; i <<= 1)
        k1++;

    BigInt b1(k1);
    uint32 *x1 = b1.words;
    for (i = 0; i < n; i++)
        *x1++ = 0;
    uint32 *x = words;
    uint32 *xe = x + size;
    if (k &= 0x1f) {
        k1 = uint(32 - k);
        uint32 z = 0;
        while (x != xe) {
            *x1++ = *x << k | z;
            z = *x++ >> k1;
        }
        if ((*x1 = z) != 0)
            ++n1;
    }
    else
        while (x != xe)
            *x1++ = *x++;
    b1.size = n1 - 1;
    move(b1);
}


// 'p5s' points to a linked list of Bigints that are powers of 625.
// This list grows on demand, and it can only grow: it won't change
// in any other way.  So if we read 'p5s' or the 'next' field of
// some BigInt on the list, and it is not NULL, we know it won't
// change to NULL or some other value.  Only when the value of
// 'p5s' or 'next' is NULL do we need to acquire the lock and add
// a new BigInt to the list.
struct PowerOf625 {
    PowerOf625 *next;
    JS::BigInt b;
};

static PowerOf625 *p5s;

#ifdef JS_THREADSAFE
static PRLock *p5s_lock;
#endif


// Let this = this * 5^k.  k must be nonnegative.
void JS::BigInt::pow5Mul(int32 k)
{
    static const uint p05[3] = {5, 25, 125};

    ASSERT(k >= 0);
    int32 i = k & 3;
    if (i)
        mulAdd(p05[i-1], 0);

    if (!(k >>= 2))
        return;
    PowerOf625 *p5 = p5s;
    if (!p5) {
        auto_ptr<PowerOf625> p(new PowerOf625);
        p->next = 0;
        p->b.init(625);
#ifdef JS_THREADSAFE
        // We take great care to not call init() and recycle() while holding
        // the lock. lock and check again
        PR_Lock(p5s_lock);
        if (!(p5 = p5s)) {
#endif
            // first time
            p5 = p.release();
            p5s = p5;
#ifdef JS_THREADSAFE
        }
        PR_Unlock(p5s_lock);
#endif
    }
    for (;;) {
        if (k & 1)
            *this *= p5->b;
        if (!(k >>= 1))
            break;
        PowerOf625 *p51 = p5->next;
        if (!p51) {
            auto_ptr<PowerOf625> p(new PowerOf625);
            p->next = 0;
            p->b = p5->b;
            p->b *= p5->b;
#ifdef JS_THREADSAFE
            PR_Lock(p5s_lock);
            if (!(p51 = p5->next)) {
#endif
                p51 = p.release();
                p5->next = p51;
#ifdef JS_THREADSAFE
            }
            PR_Unlock(p5s_lock);
#endif
        }
        p5 = p51;
    }
}


// Return -1, 0, or 1 depending on whether this<b, this==b, or this>b,
// respectively.
int JS::BigInt::cmp(const BigInt &b) const
{
    uint32 i = size;
    uint32 j = b.size;
#ifdef DEBUG
    if (i > 1 && !words[i-1])
        NOT_REACHED("cmp called with words[size-1] == 0");
    if (j > 1 && !b.words[j-1])
        NOT_REACHED("cmp called with b.words[b.size-1] == 0");
#endif
    if (i != j)
        return i<j ? -1 : 1;
    const uint32 *xa0 = words;
    const uint32 *xa = xa0 + j;
    const uint32 *xb = b.words + j;
    while (xa != xa0) {
        if (*--xa != *--xb)
            return *xa < *xb ? -1 : 1;
    }
    return 0;
}


// Initialize this BigInt to m-n.  This BigInt must not have a previous value.
void JS::BigInt::initDiff(const BigInt &m, const BigInt &n)
{
    ASSERT(!words && this != &m && this != &n);

    int i = m.cmp(n);
    if (!i)
        init(0);
    else {
        const BigInt *a;
        const BigInt *b;
        if (i < 0) {
            a = &n;
            b = &m;
        } else {
            a = &m;
            b = &n;
        }
        allocate(a->lgGrossSize);
        negative = i < 0;
        uint32 wa = a->size;
        const uint32 *xa = a->words;
        const uint32 *xae = xa + wa;
        const uint32 *xb = b->words;
        const uint32 *xbe = xb + b->size;
        uint32 *xc = words;
#ifdef NATIVE_INT64
        uint64 borrow = 0;
        uint64 y;
        while (xb < xbe) {
            y = (uint64)*xa++ - *xb++ - borrow;
            borrow = y >> 32 & 1UL;
            *xc++ = (uint32)(y & 0xffffffffUL);
        }
        while (xa < xae) {
            y = *xa++ - borrow;
            borrow = y >> 32 & 1UL;
            *xc++ = (uint32)(y & 0xffffffffUL);
        }
#else
        uint32 borrow = 0;
        uint32 y;
        uint32 z;
        while (xb < xbe) {
            y = (*xa & 0xffff) - (*xb & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*xa++ >> 16) - (*xb++ >> 16) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(xc, z, y);
        }
        while (xa < xae) {
            y = (*xa & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*xa++ >> 16) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(xc, z, y);
        }
#endif
        while (!*--xc)
            wa--;
        size = wa;
    }
}


// Return floor(this/2^k) and set this to be the remainder.
// The returned quotient must be less than 2^32.
uint32 JS::BigInt::quoRem2(int32 k)
{
    int32 n = k >> 5;
    k &= 0x1F;
    uint32 mask = (1u<<k) - 1;

    int32 w = int32(size) - n;
    if (w <= 0)
        return 0;
    ASSERT(w <= 2);
    uint32 *bx = words;
    uint32 *bxe = bx + n;
    uint32 result = *bxe >> k;
    *bxe &= mask;
    if (w == 2) {
        ASSERT(!(bxe[1] & ~mask));
        if (k)
            result |= bxe[1] << (32 - k);
    }
    n++;
    while (!*bxe && bxe != bx) {
        n--;
        bxe--;
    }
    size = uint32(n);
    return result;
}


// Return floor(this/S) and set this to be the remainder.  As added restrictions,
// this must not have more words than S, the most significant word of S must not
// start with a 1 bit, and the returned quotient must be less than 36.
int32 JS::BigInt::quoRem(const BigInt &S)
{
#ifdef NATIVE_INT64
    uint64 borrow, carry, y, ys;
#else
    uint32 borrow, carry, y, ys;
    uint32 si, z, zs;
#endif

    uint32 n = S.size;
    ASSERT(size <= n && n);
    if (size < n)
        return 0;
    const uint32 *sx = S.words;
    const uint32 *sxe = sx + --n;
    uint32 *bx = words;
    uint32 *bxe = bx + n;
    ASSERT(*sxe <= 0x7FFFFFFF);
    uint32 q = *bxe / (*sxe + 1);  // ensure q <= true quotient
    ASSERT(q < 36);
    if (q) {
        borrow = 0;
        carry = 0;
        do {
#ifdef NATIVE_INT64
            ys = *sx++ * (uint64)q + carry;
            carry = ys >> 32;
            y = *bx - (ys & 0xffffffffUL) - borrow;
            borrow = y >> 32 & 1UL;
            *bx++ = (uint32)(y & 0xffffffffUL);
#else
            si = *sx++;
            ys = (si & 0xffff) * q + carry;
            zs = (si >> 16) * q + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#endif
        } while (sx <= sxe);
        if (!*bxe) {
            bx = words;
            while (--bxe > bx && !*bxe)
                --n;
            size = n;
        }
    }
    if (cmp(S) >= 0) {
        q++;
        borrow = 0;
        carry = 0;
        bx = words;
        sx = S.words;
        do {
#ifdef NATIVE_INT64
            ys = *sx++ + carry;
            carry = ys >> 32;
            y = *bx - (ys & 0xffffffffUL) - borrow;
            borrow = y >> 32 & 1UL;
            *bx++ = (uint32)(y & 0xffffffffUL);
#else
            si = *sx++;
            ys = (si & 0xffff) + carry;
            zs = (si >> 16) + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#endif
        } while (sx <= sxe);
        bx = words;
        bxe = bx + n;
        if (!*bxe) {
            while (--bxe > bx && !*bxe)
                --n;
            size = n;
        }
    }
    return int32(q);
}


// Let this = floor(this / divisor), and return the remainder.  this must be
// nonnegative. divisor must be between 1 and 65536.
uint32 JS::BigInt::divRem(uint32 divisor)
{
    uint32 n = size;
    uint32 remainder = 0;

    ASSERT(divisor > 0 && divisor <= 65536);

    if (!n)
        return 0; // this is zero
    uint32 *bx = words;
    uint32 *bp = bx + n;
    do {
        uint32 a = *--bp;
        uint32 dividend = remainder << 16 | a >> 16;
        uint32 quotientHi = dividend / divisor;
        uint32 quotientLo;

        remainder = dividend - quotientHi*divisor;
        ASSERT(quotientHi <= 0xFFFF && remainder < divisor);
        dividend = remainder << 16 | (a & 0xFFFF);
        quotientLo = dividend / divisor;
        remainder = dividend - quotientLo*divisor;
        ASSERT(quotientLo <= 0xFFFF && remainder < divisor);
        *bp = quotientHi << 16 | quotientLo;
    } while (bp != bx);

    // Decrease the size of the number if its most significant word is now zero.
    if (bx[n-1] == 0)
        size--;
    return remainder;
}


double JS::BigInt::b2d(int32 &e) const
{
    double d;

    const uint32 *xa0 = words;
    const uint32 *xa = xa0 + size;
    ASSERT(size);
    uint32 y = *--xa;
    ASSERT(y);
    int k = hi0bits(y);
    e = 32 - k;
    if (k < Ebits) {
        word0(d) = Exp_1 | y >> (Ebits - k);
        uint32 w = xa > xa0 ? *--xa : 0;
        word1(d) = y << (32-Ebits + k) | w >> (Ebits - k);
    } else {
        uint32 z = xa > xa0 ? *--xa : 0;
        if (k -= Ebits) {
            word0(d) = Exp_1 | y << k | z >> (32 - k);
            y = xa > xa0 ? *--xa : 0;
            word1(d) = z << k | y >> (32 - k);
        }
        else {
            word0(d) = Exp_1 | y;
            word1(d) = z;
        }
    }
    return d;
}


double JS::BigInt::ratio(const BigInt &denominator) const
{
    int32 ka, kb;

    double da = b2d(ka);
    double db = denominator.b2d(kb);
    int32 k = ka - kb + 32*int32(size - denominator.size);
    if (k > 0)
        word0(da) += k*Exp_msk1;
    else
        word0(db) -= k*Exp_msk1;
    return da / db;
}


void JS::BigInt::s2b(const char *s, int32 nd0, int32 nd, uint32 y9)
{
    int32 i;
    int32 x, y;

    x = (nd + 8) / 9;
    uint k = 0;
    for (y = 1; x > y; y <<= 1, k++) ;
    ASSERT(!words);
    allocate(k);
    words[0] = y9;
    size = 1;

    i = 9;
    if (9 < nd0) {
        s += 9;
        do mulAdd(10, uint(*s++ - '0'));
        while (++i < nd0);
        s++;
    } else
        s += 10;
    for (; i < nd; i++)
        mulAdd(10, uint(*s++ - '0'));
}



static const double tens[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
        1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
        1e20, 1e21, 1e22
        };

static const double bigtens[] = {1e16, 1e32, 1e64, 1e128, 1e256};
static const double tinytens[] = {1e-16, 1e-32, 1e-64, 1e-128,
#ifdef Avoid_Underflow
                                  9007199254740992.e-256
#else
                                  1e-256
#endif
                                  };
// The factor of 2^53 in tinytens[4] helps us avoid setting the underflow
// flag unnecessarily.  It leads to a song and dance at the end of strToDouble.

const int32 Scale_Bit = 0x10;
const int n_bigtens = 5;


#ifdef JS_THREADSAFE
static bool initialized = false;

// hacked replica of nspr _PR_InitDtoa
static void InitDtoa(void)
{
    freelist_lock = PR_NewLock();
    p5s_lock = PR_NewLock();
    initialized = true;
}
#endif



// Return as a double-precision floating-point number the value represented by
// the character string str.  The string is scanned up to the first unrecognized
// character.  The character sequences 'Infinity', '+Infinity', '-Infinity', and
// 'NaN' are also recognized.
// Return a pointer to the character terminating the scan in numEnd.
// If no number can be formed, set numEnd to str and return zero.
double JS::strToDouble(const char *str, const char *&numEnd)
{
    int32 scale;
    int32 bb2, bb5, bbe, bd2, bd5, bbbits, bs2, c, dsign,
        e, e1, esign, i, j, k, nd, nd0, nf, nz, nz0;
    const char *s, *s0, *s1;
    double aadj, aadj1, adj, rv, rv0;
    int32 L;
    uint32 y, z;

#ifdef JS_THREADSAFE
    if (!initialized) InitDtoa();
#endif

    nz0 = nz = 0;
    bool negative = false;
    bool haveSign = false;
    rv = 0.;
    for (s = str;; s++)
        switch (*s) {
            case '-':
                negative = true;
                // no break
            case '+':
                haveSign = true;
                if (*++s)
                    goto break2;
                // no break
            case 0:
                s = str;
                goto ret;
            case '\t':
            case '\n':
            case '\v':
            case '\f':
            case '\r':
            case ' ':
                continue;
            default:
                goto break2;
        }
  break2:
    switch (*s) {
        case '0':
            nz0 = 1;
            while (*++s == '0') ;
            if (!*s)
                goto ret;
            break;
        case 'I':
            if (!STD::strncmp(s+1, "nfinity", 7)) {
                rv = positiveInfinity;
                s += 8;
                goto ret;
            }
            break;
        case 'N':
            if (!haveSign && !STD::strncmp(s+1, "aN", 2)) {
                rv = nan;
                s += 3;
                goto ret;
            }
    }
    s0 = s;
    y = z = 0;
    for (nd = nf = 0; (c = *s) >= '0' && c <= '9'; nd++, s++)
        if (nd < 9)
            y = 10*y + c - '0';
        else if (nd < 16)
            z = 10*z + c - '0';
    nd0 = nd;
    if (c == '.') {
        c = *++s;
        if (!nd) {
            for (; c == '0'; c = *++s)
                nz++;
            if (c > '0' && c <= '9') {
                s0 = s;
                nf += nz;
                nz = 0;
                goto have_dig;
            }
            goto dig_done;
        }
        for (; c >= '0' && c <= '9'; c = *++s) {
          have_dig:
            nz++;
            if (c -= '0') {
                nf += nz;
                for (i = 1; i < nz; i++)
                    if (nd++ < 9)
                        y *= 10;
                    else if (nd <= DBL_DIG + 1)
                        z *= 10;
                if (nd++ < 9)
                    y = 10*y + c;
                else if (nd <= DBL_DIG + 1)
                    z = 10*z + c;
                nz = 0;
            }
        }
    }
  dig_done:
    e = 0;
    if (c == 'e' || c == 'E') {
        if (!nd && !nz && !nz0) {
            s = str;
            goto ret;
        }
        str = s;
        esign = 0;
        switch (c = *++s) {
            case '-':
                esign = 1;
            case '+':
                c = *++s;
        }
        if (c >= '0' && c <= '9') {
            while (c == '0')
                c = *++s;
            if (c > '0' && c <= '9') {
                L = c - '0';
                s1 = s;
                while ((c = *++s) >= '0' && c <= '9')
                    L = 10*L + c - '0';
                if (s - s1 > 8 || L > 19999)
                    // Avoid confusion from exponents so large that e might overflow.
                    e = 19999; // safe for 16 bit ints
                else
                    e = (int32)L;
                if (esign)
                    e = -e;
            }
            else
                e = 0;
        }
        else
            s = str;
    }
    if (!nd) {
        if (!nz && !nz0)
            s = str;
        goto ret;
    }
    e1 = e -= nf;

    // Now we have nd0 digits, starting at s0, followed by a
    // decimal point, followed by nd-nd0 digits.  The number we're
    // after is the integer represented by those digits times 10**e

    if (!nd0)
        nd0 = nd;
    k = nd < DBL_DIG + 1 ? nd : DBL_DIG + 1;
    rv = y;
    if (k > 9)
        rv = tens[k - 9] * rv + z;
    if (nd <= DBL_DIG && FLT_ROUNDS == 1) {
        if (!e)
            goto ret;
        if (e > 0) {
            if (e <= Ten_pmax) {
                rv *= tens[e];
                goto ret;
            }
            i = DBL_DIG - nd;
            if (e <= Ten_pmax + i) {
                // A fancier test would sometimes let us do this for larger i values.
                e -= i;
                rv *= tens[i];
                rv *= tens[e];
                goto ret;
            }
        }
#ifndef Inaccurate_Divide
        else if (e >= -Ten_pmax) {
            rv /= tens[-e];
            goto ret;
        }
#endif
    }
    e1 += nd - k;

    scale = 0;

    // Get starting approximation = rv * 10**e1

    if (e1 > 0) {
        if ((i = e1 & 15) != 0)
            rv *= tens[i];
        if (e1 &= ~15) {
            if (e1 > DBL_MAX_10_EXP) {
              ovfl:
                // Return infinity.
                rv = positiveInfinity;
                goto ret;
            }
            e1 >>= 4;
            for (j = 0; e1 > 1; j++, e1 >>= 1)
                if (e1 & 1)
                    rv *= bigtens[j];
            // The last multiplication could overflow.
            word0(rv) -= P*Exp_msk1;
            rv *= bigtens[j];
            if ((z = word0(rv) & Exp_mask) > Exp_msk1*(DBL_MAX_EXP+Bias-P))
                goto ovfl;
            if (z > Exp_msk1*(DBL_MAX_EXP+Bias-1-P)) {
                // set to largest number (Can't trust DBL_MAX)
                word0(rv) = Big0;
                word1(rv) = Big1;
            }
            else
                word0(rv) += P*Exp_msk1;
        }
    }
    else if (e1 < 0) {
        e1 = -e1;
        if ((i = e1 & 15) != 0)
            rv /= tens[i];
        if (e1 &= ~15) {
            e1 >>= 4;
            if (e1 >= 1 << n_bigtens)
                goto undfl;
#ifdef Avoid_Underflow
            if (e1 & Scale_Bit)
                scale = P;
            for (j = 0; e1 > 0; j++, e1 >>= 1)
                if (e1 & 1)
                    rv *= tinytens[j];
            if (scale && (j = P + 1 - int32((word0(rv) & Exp_mask) >> Exp_shift)) > 0) {
                // scaled rv is denormal; zap j low bits
                if (j >= 32) {
                    word1(rv) = 0;
                    word0(rv) &= 0xffffffff << (j-32);
                    if (!word0(rv))
                        word0(rv) = 1;
                }
                else
                    word1(rv) &= 0xffffffff << j;
            }
#else
            for (j = 0; e1 > 1; j++, e1 >>= 1)
                if (e1 & 1)
                    rv *= tinytens[j];
            // The last multiplication could underflow.
            rv0 = rv;
            rv *= tinytens[j];
            if (!rv) {
                rv = 2.*rv0;
                rv *= tinytens[j];
#endif
                if (!rv) {
                  undfl:
                    rv = 0.;
                    goto ret;
                }
#ifndef Avoid_Underflow
                word0(rv) = Tiny0;
                word1(rv) = Tiny1;
                // The refinement below will clean this approximation up.
            }
#endif
        }
    }

    // Now the hard part -- adjusting rv to the correct value.
    // Put digits into bd: true value = bd * 10^e
    {
        BigInt bd0;
        bd0.s2b(s0, nd0, nd, y);

        for (;;) {
            BigInt bs;

            BigInt bd = bd0;
            BigInt bb;
            bb.init(rv, bbe, bbbits);    // rv = bb * 2^bbe
            bs.init(1);

            if (e >= 0) {
                bb2 = bb5 = 0;
                bd2 = bd5 = e;
            }
            else {
                bb2 = bb5 = -e;
                bd2 = bd5 = 0;
            }
            if (bbe >= 0)
                bb2 += bbe;
            else
                bd2 -= bbe;
            bs2 = bb2;
#ifdef Sudden_Underflow
            j = P + 1 - bbbits;
#else
#ifdef Avoid_Underflow
            j = bbe - scale;
#else
            j = bbe;
#endif
            i = j + bbbits - 1; // logb(rv)
            if (i < Emin)   // denormal
                j += P - Emin;
            else
                j = P + 1 - bbbits;
#endif
            bb2 += j;
            bd2 += j;
#ifdef Avoid_Underflow
            bd2 += scale;
#endif
            i = bb2 < bd2 ? bb2 : bd2;
            if (i > bs2)
                i = bs2;
            if (i > 0) {
                bb2 -= i;
                bd2 -= i;
                bs2 -= i;
            }
            if (bb5 > 0) {
                bs.pow5Mul(bb5);
                bb *= bs;
            }
            if (bb2 > 0)
                bb.pow2Mul(bb2);
            if (bd5 > 0)
                bd.pow5Mul(bd5);
            if (bd2 > 0)
                bd.pow2Mul(bd2);
            if (bs2 > 0)
                bs.pow2Mul(bs2);

            BigInt delta;
            delta.initDiff(bb, bd);
            dsign = delta.negative;
            delta.negative = false;
            i = delta.cmp(bs);
            if (i < 0) {
                // Error is less than half an ulp -- check for special case of mantissa a power of two.
                if (dsign || word1(rv) || word0(rv) & Bndry_mask
#ifdef Avoid_Underflow
                    || (word0(rv) & Exp_mask) <= Exp_msk1 + P*Exp_msk1
#else
                    || (word0(rv) & Exp_mask) <= Exp_msk1
#endif
                    ) {
#ifdef Avoid_Underflow
                    if (delta.isZero())
                        dsign = 2;
#endif
                    break;
                }
                delta.pow2Mul(Log2P);
                if (delta.cmp(bs) > 0)
                    goto drop_down;
                break;
            }
            if (i == 0) {
                // exactly half-way between
                if (dsign) {
                    if ((word0(rv) & Bndry_mask1) == Bndry_mask1 && word1(rv) == 0xffffffff) {
                        // boundary case -- increment exponent
                        word0(rv) = (word0(rv) & Exp_mask) + Exp_msk1;
                        word1(rv) = 0;
#ifdef Avoid_Underflow
                        dsign = 0;
#endif
                        break;
                    }
                }
                else if (!(word0(rv) & Bndry_mask) && !word1(rv)) {
#ifdef Avoid_Underflow
                    dsign = 2;
#endif
                  drop_down:
                    // boundary case -- decrement exponent
#ifdef Sudden_Underflow
                    L = word0(rv) & Exp_mask;
                    if (L <= Exp_msk1)
                        goto undfl;
                    L -= Exp_msk1;
#else
                    L = int32((word0(rv) & Exp_mask) - Exp_msk1);
#endif
                    word0(rv) = uint32(L) | Bndry_mask1;
                    word1(rv) = 0xffffffff;
                    break;
                }
#ifndef ROUND_BIASED
                if (!(word1(rv) & LSB))
                    break;
#endif
                if (dsign)
                    rv += ulp(rv);
#ifndef ROUND_BIASED
                else {
                    rv -= ulp(rv);
#ifndef Sudden_Underflow
                    if (!rv)
                        goto undfl;
#endif
                }
#ifdef Avoid_Underflow
                dsign = 1 - dsign;
#endif
#endif
                break;
            }
            if ((aadj = delta.ratio(bs)) <= 2.) {
                if (dsign)
                    aadj = aadj1 = 1.;
                else if (word1(rv) || word0(rv) & Bndry_mask) {
#ifndef Sudden_Underflow
                    if (word1(rv) == Tiny1 && !word0(rv))
                        goto undfl;
#endif
                    aadj = 1.;
                    aadj1 = -1.;
                }
                else {
                    // special case -- power of FLT_RADIX to be rounded down...
                    if (aadj < 2./FLT_RADIX)
                        aadj = 1./FLT_RADIX;
                    else
                        aadj *= 0.5;
                    aadj1 = -aadj;
                }
            }
            else {
                aadj *= 0.5;
                aadj1 = dsign ? aadj : -aadj;
#ifdef Check_FLT_ROUNDS
                switch (FLT_ROUNDS) {
                    case 2: // towards +infinity
                        aadj1 -= 0.5;
                        break;
                    case 0: // towards 0
                    case 3: // towards -infinity
                        aadj1 += 0.5;
                }
#else
                if (FLT_ROUNDS == 0)
                    aadj1 += 0.5;
#endif
            }
            y = word0(rv) & Exp_mask;

            // Check for overflow

            if (y == Exp_msk1*(DBL_MAX_EXP+Bias-1)) {
                rv0 = rv;
                word0(rv) -= P*Exp_msk1;
                adj = aadj1 * ulp(rv);
                rv += adj;
                if ((word0(rv) & Exp_mask) >= Exp_msk1*(DBL_MAX_EXP+Bias-P)) {
                    if (word0(rv0) == Big0 && word1(rv0) == Big1)
                        goto ovfl;
                    word0(rv) = Big0;
                    word1(rv) = Big1;
                    continue;
                }
                else
                    word0(rv) += P*Exp_msk1;
            }
            else {
#ifdef Sudden_Underflow
                if ((word0(rv) & Exp_mask) <= P*Exp_msk1) {
                    rv0 = rv;
                    word0(rv) += P*Exp_msk1;
                    adj = aadj1 * ulp(rv);
                    rv += adj;
                    if ((word0(rv) & Exp_mask) <= P*Exp_msk1) {
                        if (word0(rv0) == Tiny0 && word1(rv0) == Tiny1)
                            goto undfl;
                        word0(rv) = Tiny0;
                        word1(rv) = Tiny1;
                        continue;
                    } else
                        word0(rv) -= P*Exp_msk1;
                } else {
                    adj = aadj1 * ulp(rv);
                    rv += adj;
                }
#else
                // Compute adj so that the IEEE rounding rules will correctly round rv + adj in some half-way cases.
                // If rv * ulp(rv) is denormalized (i.e., y <= (P-1)*Exp_msk1), we must adjust aadj to avoid
                // trouble from bits lost to denormalization; example: 1.2e-307.
#ifdef Avoid_Underflow
                if (y <= P*Exp_msk1 && aadj > 1.)
#else
                    if (y <= (P-1)*Exp_msk1 && aadj > 1.)
#endif
                    {
                        aadj1 = (double)(int32)(aadj + 0.5);
                        if (!dsign)
                            aadj1 = -aadj1;
                    }
#ifdef Avoid_Underflow
                if (scale && y <= P*Exp_msk1)
                    word0(aadj1) += (P+1)*Exp_msk1 - y;
#endif
                adj = aadj1 * ulp(rv);
                rv += adj;
#endif
            }
            z = word0(rv) & Exp_mask;
#ifdef Avoid_Underflow
            if (!scale)
#endif
                if (y == z) {
                    // Can we stop now?
                    L = (int32)aadj;
                    aadj -= L;
                    // The tolerances below are conservative.
                    if (dsign || word1(rv) || word0(rv) & Bndry_mask) {
                        if (aadj < .4999999 || aadj > .5000001)
                            break;
                    }
                    else if (aadj < .4999999/FLT_RADIX)
                        break;
                }
        }
#ifdef Avoid_Underflow
        if (scale) {
            word0(rv0) = Exp_1 - P*Exp_msk1;
            word1(rv0) = 0;
            if ((word0(rv) & Exp_mask) <= P*Exp_msk1 && word1(rv) & 1 && dsign != 2) {
                if (dsign) {
#ifdef Sudden_Underflow
                    // rv will be 0, but this would give the right result if only rv *= rv0 worked.
                    word0(rv) += P*Exp_msk1;
                    word0(rv0) = Exp_1 - 2*P*Exp_msk1;
#endif
                    rv += ulp(rv);
                }
                else
                    word1(rv) &= ~1;
            }
            rv *= rv0;
        }
#endif // Avoid_Underflow
    }
  ret:
    numEnd = s;
    return negative ? -rv : rv;
}


// A version of strToDouble that takes a char16 string that begins at str and
// ends just before strEnd.  The char16 string does not have to be
// null-terminated. Leading Unicode whitespace is skipped.
double JS::stringToDouble(const char16 *str, const char16 *strEnd, const char16 *&numEnd)
{
    const char16 *str1 = skipWhiteSpace(str, strEnd);

    CharAutoPtr cstr(new char[strEnd - str1 + 1]);
    char *q = cstr.get();
    for (const char16 *p = str1; p != strEnd; p++) {
        char16 ch = *p;
        if (uint16(ch) >> 8)
            break;
        *q++ = char(ch);
    }
    *q = '\0';

    const char *estr;
    double value = strToDouble(cstr.get(), estr);
    ptrdiff_t i = estr - cstr.get();
    numEnd = i ? str1 + i : str;
    if ((value == 0.0) && (i == 0))
        return nan;
    return value;
}



class BinaryDigitReader
{
    uint base;               // Base of number; must be a power of 2
    uint digit;              // Current digit value in radix given by base
    uint digitMask;          // Mask to extract the next bit from digit
    const char16 *digits;    // Pointer to the remaining digits
    const char16 *digitsEnd; // Pointer to first non-digit

public:
    BinaryDigitReader(uint base, const char16 *digitsBegin, const char16 *digitsEnd) :
            base(base), digitMask(0), digits(digitsBegin), digitsEnd(digitsEnd) {}

    int next();
};


// Return the next binary digit from the number or -1 if done.
int BinaryDigitReader::next()
{
    if (digitMask == 0) {
        if (digits == digitsEnd)
            return -1;

        uint c = *digits++;
        if ('0' <= c && c <= '9')
            digit = c - '0';
        else if ('a' <= c && c <= 'z')
            digit = c - 'a' + 10;
        else digit = c - 'A' + 10;
        digitMask = base >> 1;
    }
    int bit = (digit & digitMask) != 0;
    digitMask >>= 1;
    return bit;
}


// Read an integer from a char16 string that begins at str and ends just before
// strEnd. The char16 string does not have to be null-terminated.  The integer
// is returned as a double, which is guaranteed to be the closest double number
// to the given input when base is 10 or a power of 2.
// May experience roundoff errors for very large numbers of a different radix.
// Return a pointer to the character just past the integer in numEnd.
// If the string does not have a number in it, set numEnd to str and return 0.
// Leading Unicode whitespace is skipped.
double JS::stringToInteger(const char16 *str, const char16 *strEnd, const char16 *&numEnd, uint base)
{
    const char16 *str1 = skipWhiteSpace(str, strEnd);

    bool negative = (*str1 == '-');
    if (negative || *str1 == '+')
        str1++;

    if ((base == 0 || base == 16) &&
        *str1 == '0' && (str1[1] == 'X' || str1[1] == 'x')) {
        // Skip past hex prefix.
        base = 16;
        str1 += 2;
    }
    if (base == 0)
        base = 10; // Default to decimal.

        // Find some prefix of the string that's a number in the given base.
    const char16 *start = str1; // Mark - if string is empty, we return 0.
    double value = 0.0;
    while (true) {
        uint digit;
        char16 c = *str1;
        if ('0' <= c && c <= '9')
            digit = uint(c) - '0';
        else if ('a' <= c && c <= 'z')
            digit = uint(c) - 'a' + 10;
        else if ('A' <= c && c <= 'Z')
            digit = uint(c) - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value*base + digit;
        str1++;
    }

    if (value >= 9007199254740992.0) {
        if (base == 10) {
            // If we're accumulating a decimal number and the number is >= 2^53, then the result from the
            // repeated multiply-add above may be inaccurate.  Call stringToDouble to get the correct answer.
            const char16 *numEnd2;
            value = stringToDouble(start, str1, numEnd2);
            ASSERT(numEnd2 == str1);

        } else if (base == 2 || base == 4 || base == 8 || base == 16 || base == 32) {
            // The number may also be inaccurate for one of these bases.  This happens if the addition in
            // value*base + digit causes a round-down to an even least significant mantissa bit when the
            // first dropped bit is a one.  If any of the following digits in the number (which haven't been
            // added in yet) are nonzero then the correct action would have been to round up instead of down.
            // An example of this occurs when reading the number 0x1000000000000081, which rounds to
            // 0x1000000000000000 instead of 0x1000000000000100.
            BinaryDigitReader bdr(base, start, str1);
            value = 0.0;

            // Skip leading zeros.
            int bit;
            do {
                bit = bdr.next();
            } while (bit == 0);

            if (bit == 1) {
                // Gather the 53 significant bits (including the leading 1)
                int bit2;
                value = 1.0;
                for (int j = 52; j; --j) {
                    bit = bdr.next();
                    if (bit < 0)
                        goto done;
                    value = value*2 + bit;
                }
                bit2 = bdr.next();      // bit2 is the 54th bit (the first dropped from the mantissa)
                if (bit2 >= 0) {
                    double factor = 2.0;
                    int sticky = 0;     // sticky is 1 if any bit beyond the 54th is 1
                    int bit3;

                    while ((bit3 = bdr.next()) >= 0) {
                        sticky |= bit3;
                        factor *= 2;
                    }
                    value += bit2 & (bit | sticky);
                    value *= factor;
                }
              done:;
            }
        }
    }
    // We don't worry about inaccurate numbers for any other base.

    if (str1 == start)
        numEnd = str;
    else {
        numEnd = str1;
        if (negative)
            value = -value;
    }
    return value;
}



// doubleToAscii for IEEE arithmetic (dmg): convert double to ASCII string.
//
// Inspired by "How to Print Floating-Point Numbers Accurately" by
// Guy L. Steele, Jr. and Jon L. White [Proc. ACM SIGPLAN '90, pp. 92-101].
//
// Modifications:
//  1. Rather than iterating, we use a simple numeric overestimate
//     to determine k = floor(log10(d)).  We scale relevant
//     quantities using O(log2(k)) rather than O(k) multiplications.
//  2. For some modes > 2 (corresponding to ecvt and fcvt), we don't
//     try to generate digits strictly left to right.  Instead, we
//     compute with fewer bits and propagate the carry if necessary
//     when rounding the final digit up.  This is often faster.
//  3. Under the assumption that input will be rounded nearest,
//     mode 0 renders 1e23 as 1e23 rather than 9.999999999999999e22.
//     That is, we allow equality in stopping tests when the
//     round-nearest rule will give the same floating-point value
//     as would satisfaction of the stopping test with strict
//     inequality.
//  4. We remove common factors of powers of 2 from relevant
//     quantities.
//  5. When converting floating-point integers less than 1e16,
//     we use floating-point arithmetic rather than resorting
//     to multiple-precision integers.
//  6. When asked to produce fewer than 15 digits, we first try
//     to get by with floating-point arithmetic; we resort to
//     multiple-precision integer arithmetic only if we cannot
//     guarantee that the floating-point calculation has given
//     the correctly rounded result.  For k requested digits and
//     "uniformly" distributed input, the probability is
//     something like 10^(k-15) that we must resort to the int32
//     calculation.
///

// Always emits at least one digit.
// If biasUp is set, then rounding in modes 2 and 3 will round away from zero
// when the number is exactly halfway between two representable values.  For example,
// rounding 2.5 to zero digits after the decimal point will return 3 and not 2.
// 2.49 will still round to 2, and 2.51 will still round to 3.
// The buffer should be at least 20 bytes for modes 0 and 1.  For the other modes,
// the buffer's size should be two greater than the maximum number of output characters expected.
// Return a pointer to the resulting string's trailing null.
static char *doubleToAscii(double d, int mode, bool biasUp, int ndigits, int *decpt, bool *negative, char *buf)
{
        /*  Arguments ndigits, decpt, negative are similar to those
            of ecvt and fcvt; trailing zeros are suppressed from
            the returned string.  If d is +-Infinity or NaN,
            then *decpt is set to 9999.

            mode:
            0 ==> shortest string that yields d when read in
            and rounded to nearest.
            1 ==> like 0, but with Steele & White stopping rule;
            e.g. with IEEE P754 arithmetic , mode 0 gives
            1e23 whereas mode 1 gives 9.999999999999999e22.
            2 ==> max(1,ndigits) significant digits.  This gives a
            return value similar to that of ecvt, except
            that trailing zeros are suppressed.
            3 ==> through ndigits past the decimal point.  This
            gives a return value similar to that from fcvt,
            except that trailing zeros are suppressed, and
            ndigits can be negative.
            4-9 should give the same return values as 2-3, i.e.,
            4 <= mode <= 9 ==> same return as mode
            2 + (mode & 1).  These modes are mainly for
            debugging; often they run slower but sometimes
            faster than modes 2-3.
            4,5,8,9 ==> left-to-right digit generation.
            6-9 ==> don't try fast floating-point estimate
            (if applicable).

            Values of mode other than 0-9 are treated as mode 0.

            Sufficient space is allocated to the return value
            to hold the suppressed trailing zeros.
*/

    int32 bbits, b2, b5, be, dig, i, ieps, ilim, ilim0, ilim1,
        j, j1, k, k0, k_check, leftright, m2, m5, s2, s5, try_quick;
    int32 L;
#ifndef Sudden_Underflow
    int32 denorm;
    uint32 x;
#endif
    double d2, ds, eps;
    char *s;

#ifdef JS_THREADSAFE
    if (!initialized) InitDtoa();
#endif

    if (word0(d) & Sign_bit) {
        // set negative for everything, including 0's and NaNs
        *negative = true;
        word0(d) &= ~Sign_bit;  // clear sign bit
    }
    else
        *negative = false;

    if ((word0(d) & Exp_mask) == Exp_mask) {
        // Infinity or NaN
        *decpt = 9999;
        strcpy(buf, !word1(d) && !(word0(d) & Frac_mask) ? "Infinity" : "NaN");
        return buf[3] ? buf + 8 : buf + 3;
    }
    if (!d) {
      no_digits:
        *decpt = 1;
        buf[0] = '0'; buf[1] = '\0';  // copy "0" to buffer
        return buf + 1;
    }

    BigInt b;
    b.init(d, be, bbits);
#ifdef Sudden_Underflow
    i = (int32)(word0(d) >> Exp_shift1 & (Exp_mask>>Exp_shift1));
#else
    if ((i = (int32)(word0(d) >> Exp_shift1 & (Exp_mask>>Exp_shift1))) != 0) {
#endif
        d2 = d;
        word0(d2) &= Frac_mask1;
        word0(d2) |= Exp_11;

        /* log(x)   ~=~ log(1.5) + (x-1.5)/1.5
         * log10(x)  =  log(x) / log(10)
         *      ~=~ log(1.5)/log(10) + (x-1.5)/(1.5*log(10))
         * log10(d) = (i-Bias)*log(2)/log(10) + log10(d2)
         *
         * This suggests computing an approximation k to log10(d) by
         *
         * k = (i - Bias)*0.301029995663981
         *  + ( (d2-1.5)*0.289529654602168 + 0.176091259055681 );
         *
         * We want k to be too large rather than too small.
         * The error in the first-order Taylor series approximation
         * is in our favor, so we just round up the constant enough
         * to compensate for any error in the multiplication of
         * (i - Bias) by 0.301029995663981; since |i - Bias| <= 1077,
         * and 1077 * 0.30103 * 2^-52 ~=~ 7.2e-14,
         * adding 1e-13 to the constant term more than suffices.
         * Hence we adjust the constant term to 0.1760912590558.
         * (We could get a more accurate k by invoking log10,
         *  but this is probably not worthwhile.)
         */

        i -= Bias;
#ifndef Sudden_Underflow
        denorm = 0;
    }
    else {
        // d is denormalized

        i = bbits + be + (Bias + (P-1) - 1);
        x = i > 32 ? word0(d) << (64 - i) | word1(d) >> (i - 32) : word1(d) << (32 - i);
        d2 = x;
        word0(d2) -= 31*Exp_msk1; // adjust exponent
        i -= (Bias + (P-1) - 1) + 1;
        denorm = 1;
    }
#endif

    // At this point d = f*2^i, where 1 <= f < 2.  d2 is an approximation of f.
    ds = (d2-1.5)*0.289529654602168 + 0.1760912590558 + i*0.301029995663981;
    k = (int32)ds;
    if (ds < 0. && ds != k)
        k--;    // want k = floor(ds)
    k_check = 1;
    if (k >= 0 && k <= Ten_pmax) {
        if (d < tens[k])
            k--;
        k_check = 0;
    }

    // At this point floor(log10(d)) <= k <= floor(log10(d))+1.
    // If k_check is zero, we're guaranteed that k = floor(log10(d)).
    j = bbits - i - 1;
    // At this point d = b/2^j, where b is an odd integer.
    if (j >= 0) {
        b2 = 0;
        s2 = j;
    }
    else {
        b2 = -j;
        s2 = 0;
    }
    if (k >= 0) {
        b5 = 0;
        s5 = k;
        s2 += k;
    }
    else {
        b2 -= k;
        b5 = -k;
        s5 = 0;
    }

    // At this point d/10^k = (b * 2^b2 * 5^b5) / (2^s2 * 5^s5), where b is an odd integer,
    // b2 >= 0, b5 >= 0, s2 >= 0, and s5 >= 0.
    if (mode < 0 || mode > 9)
        mode = 0;
    try_quick = 1;
    if (mode > 5) {
        mode -= 4;
        try_quick = 0;
    }
    leftright = 1;
    ilim = ilim1 = 0;
    switch (mode) {
        case 0:
        case 1:
            ilim = ilim1 = -1;
            i = 18;
            ndigits = 0;
            break;
        case 2:
            leftright = 0;
            // no break
        case 4:
            if (ndigits <= 0)
                ndigits = 1;
            ilim = ilim1 = i = ndigits;
            break;
        case 3:
            leftright = 0;
            // no break
        case 5:
            i = ndigits + k + 1;
            ilim = i;
            ilim1 = i - 1;
            if (i <= 0)
                i = 1;
    }
    // ilim is the maximum number of significant digits we want, based on k and ndigits.
    // ilim1 is the maximum number of significant digits we want, based on k and ndigits,
    // when it turns out that k was computed too high by one.
    s = buf;

    if (ilim >= 0 && ilim <= Quick_max && try_quick) {

        // Try to get by with floating-point arithmetic.

        i = 0;
        d2 = d;
        k0 = k;
        ilim0 = ilim;
        ieps = 2; // conservative

        // Divide d by 10^k, keeping track of the roundoff error and avoiding overflows.
        if (k > 0) {
            ds = tens[k&0xf];
            j = k >> 4;
            if (j & Bletch) {
                // prevent overflows
                j &= Bletch - 1;
                d /= bigtens[n_bigtens-1];
                ieps++;
            }
            for (; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    ds *= bigtens[i];
                }
            d /= ds;
        }
        else if ((j1 = -k) != 0) {
            d *= tens[j1 & 0xf];
            for (j = j1 >> 4; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    d *= bigtens[i];
                }
        }

        // Check that k was computed correctly.
        if (k_check && d < 1. && ilim > 0) {
            if (ilim1 <= 0)
                goto fast_failed;
            ilim = ilim1;
            k--;
            d *= 10.;
            ieps++;
        }

        // eps bounds the cumulative error.
        eps = ieps*d + 7.;
        word0(eps) -= (P-1)*Exp_msk1;
        if (ilim == 0) {
            d -= 5.;
            if (d > eps)
                goto one_digit;
            if (d < -eps)
                goto no_digits;
            goto fast_failed;
        }
#ifndef No_leftright
        if (leftright) {
            // Use Steele & White method of only generating digits needed.
            eps = 0.5/tens[ilim-1] - eps;
            for (i = 0;;) {
                L = (int32)d;
                d -= L;
                *s++ = char('0' + L);
                if (d < eps)
                    goto ret1;
                if (1. - d < eps)
                    goto bump_up;
                if (++i >= ilim)
                    break;
                eps *= 10.;
                d *= 10.;
            }
        }
        else {
#endif
            // Generate ilim digits, then fix them up.
            eps *= tens[ilim-1];
            for (i = 1;; i++, d *= 10.) {
                L = (int32)d;
                d -= L;
                *s++ = char('0' + L);
                if (i == ilim) {
                    if (d > 0.5 + eps)
                        goto bump_up;
                    else if (d < 0.5 - eps) {
                        while (*--s == '0') ;
                        s++;
                        goto ret1;
                    }
                    break;
                }
            }
#ifndef No_leftright
        }
#endif
      fast_failed:
        s = buf;
        d = d2;
        k = k0;
        ilim = ilim0;
    }

    // Do we have a "small" integer?
    if (be >= 0 && k <= Int_max) {
        // Yes.
        ds = tens[k];
        if (ndigits < 0 && ilim <= 0) {
            if (ilim < 0 || d < 5*ds || (!biasUp && d == 5*ds))
                goto no_digits;
          one_digit:
            *s++ = '1';
            k++;
            goto ret1;
        }
        for (i = 1;; i++) {
            L = (int32) (d / ds);
            d -= L*ds;
#ifdef Check_FLT_ROUNDS
            // If FLT_ROUNDS == 2, L will usually be high by 1
            if (d < 0) {
                L--;
                d += ds;
            }
#endif
            *s++ = char('0' + L);
            if (i == ilim) {
                d += d;
                if ((d > ds) || (d == ds && (L & 1 || biasUp))) {
                  bump_up:
                    while (*--s == '9')
                        if (s == buf) {
                            k++;
                            *s = '0';
                            break;
                        }
                    ++*s++;
                }
                break;
            }
            if (!(d *= 10.))
                break;
        }
        goto ret1;
    }

    {
        m2 = b2;
        m5 = b5;
        BigInt mLow;    // If spec_case is false, assume that mLow == mHigh
        BigInt mHigh;

        if (leftright) {
            if (mode < 2) {
                i =
#ifndef Sudden_Underflow
                    denorm ? be + (Bias + (P-1) - 1 + 1) :
#endif
                    1 + P - bbits;
            // i is 1 plus the number of trailing zero bits in d's significand. Thus,
            // (2^m2 * 5^m5) / (2^(s2+i) * 5^s5) = (1/2 lsb of d)/10^k.
            }
            else {
                j = ilim - 1;
                if (m5 >= j)
                    m5 -= j;
                else {
                    s5 += j -= m5;
                    b5 += j;
                    m5 = 0;
                }
                if ((i = ilim) < 0) {
                    m2 -= i;
                    i = 0;
                }
                // (2^m2 * 5^m5) / (2^(s2+i) * 5^s5) = (1/2 * 10^(1-ilim))/10^k.
            }
            b2 += i;
            s2 += i;
            mHigh.init(1);
            // (mHigh * 2^m2 * 5^m5) / (2^s2 * 5^s5) = one-half of last printed (when mode >= 2) or
            // input (when mode < 2) significant digit, divided by 10^k.
        }
        // We still have d/10^k = (b * 2^b2 * 5^b5) / (2^s2 * 5^s5).  Reduce common factors in
        // b2, m2, and s2 without changing the equalities.
        if (m2 > 0 && s2 > 0) {
            i = m2 < s2 ? m2 : s2;
            b2 -= i;
            m2 -= i;
            s2 -= i;
        }

        // Fold b5 into b and m5 into mHigh.
        if (b5 > 0) {
            if (leftright) {
                if (m5 > 0) {
                    mHigh.pow5Mul(m5);
                    b *= mHigh;
                }
                if ((j = b5 - m5) != 0)
                    b.pow5Mul(j);
            }
            else
                b.pow5Mul(b5);
        }
        // Now we have d/10^k = (b * 2^b2) / (2^s2 * 5^s5) and
        // (mHigh * 2^m2) / (2^s2 * 5^s5) = one-half of last printed or input significant digit, divided by 10^k.

        BigInt S;
        S.init(1);
        if (s5 > 0)
            S.pow5Mul(s5);
        // Now we have d/10^k = (b * 2^b2) / (S * 2^s2) and
        // (mHigh * 2^m2) / (S * 2^s2) = one-half of last printed or input significant digit, divided by 10^k.

        // Check for special case that d is a normalized power of 2.
        bool spec_case = false;
        if (mode < 2) {
            if (!word1(d) && !(word0(d) & Bndry_mask)
#ifndef Sudden_Underflow
                && word0(d) & (Exp_mask & Exp_mask << 1)
#endif
                ) {
                // The special case.  Here we want to be within a quarter of the last input
                // significant digit instead of one half of it when the decimal output string's value is less than d.
                b2 += Log2P;
                s2 += Log2P;
                spec_case = true;
            }
        }

        // Arrange for convenient computation of quotients:
        // shift left if necessary so divisor has 4 leading 0 bits.
        //
        // Perhaps we should just compute leading 28 bits of S once
        // and for all and pass them and a shift to quoRem, so it
        // can do shifts and ors to compute the numerator for q.
        if ((i = ((s5 ? 32 - hi0bits(S.word(S.nWords()-1)) : 1) + s2) & 0x1f) != 0)
            i = 32 - i;
        // i is the number of leading zero bits in the most significant word of S*2^s2.
        if (i > 4) {
            i -= 4;
            b2 += i;
            m2 += i;
            s2 += i;
        }
        else if (i < 4) {
            i += 28;
            b2 += i;
            m2 += i;
            s2 += i;
        }
        // Now S*2^s2 has exactly four leading zero bits in its most significant word.
        if (b2 > 0)
            b.pow2Mul(b2);
        if (s2 > 0)
            S.pow2Mul(s2);
        // Now we have d/10^k = b/S and
        // (mHigh * 2^m2) / S = maximum acceptable error, divided by 10^k.
        if (k_check) {
            if (b.cmp(S) < 0) {
                k--;
                b.mulAdd(10, 0);  // we botched the k estimate
                if (leftright)
                    mHigh.mulAdd(10, 0);
                ilim = ilim1;
            }
        }
        // At this point 1 <= d/10^k = b/S < 10.

        if (ilim <= 0 && mode > 2) {
            // We're doing fixed-mode output and d is less than the minimum nonzero output in this mode.
            // Output either zero or the minimum nonzero output depending on which is closer to d.
            // Always emit at least one digit.  If the number appears to be zero
            // using the current mode, then emit one '0' digit and set decpt to 1.
            if (ilim < 0)
                goto no_digits;
            S.mulAdd(5, 0);
            if ((i = b.cmp(S)) < 0 || (i == 0 && !biasUp))
                goto no_digits;
            goto one_digit;
        }
        if (leftright) {
            if (m2 > 0)
                mHigh.pow2Mul(m2);

            // Compute mLow -- check for special case that d is a normalized power of 2.
            if (spec_case) {
                mLow = mHigh;
                mHigh.pow2Mul(Log2P);
            }
            // mLow/S = maximum acceptable error, divided by 10^k, if the output is less than d.
            // mHigh/S = maximum acceptable error, divided by 10^k, if the output is greater than d.

            for (i = 1;;i++) {
                dig = b.quoRem(S) + '0';
                // Do we yet have the shortest decimal string that will round to d?
                j = b.cmp(spec_case ? mLow : mHigh);        // j is b/S compared with mLow/S.
                {
                    BigInt delta;
                    delta.initDiff(S, mHigh);
                    j1 = delta.negative ? 1 : b.cmp(delta);
                }
                // j1 is b/S compared with 1 - mHigh/S.
#ifndef ROUND_BIASED
                if (j1 == 0 && !mode && !(word1(d) & 1)) {
                    if (dig == '9')
                        goto round_9_up;
                    if (j > 0)
                        dig++;
                    *s++ = (char)dig;
                    goto ret;
                }
#endif
                if ((j < 0) || (j == 0 && !mode
#ifndef ROUND_BIASED
                                && !(word1(d) & 1)
#endif
                                )) {
                    if (j1 > 0) {
                        // Either dig or dig+1 would work here as the least significant decimal digit.
                        // Use whichever would produce a decimal value closer to d.
                        b.pow2Mul(1);
                        j1 = b.cmp(S);
                        if (((j1 > 0) || (j1 == 0 && (dig & 1 || biasUp))) && (dig++ == '9'))
                            goto round_9_up;
                    }
                    *s++ = (char)dig;
                    goto ret;
                }
                if (j1 > 0) {
                    if (dig == '9') { // possible if i == 1
                      round_9_up:
                        *s++ = '9';
                        goto roundoff;
                    }
                    *s++ = char(dig + 1);
                    goto ret;
                }
                *s++ = (char)dig;
                if (i == ilim)
                    break;
                b.mulAdd(10, 0);
                if (spec_case)
                    mLow.mulAdd(10, 0);
                mHigh.mulAdd(10, 0);
            }
        }
        else
            for (i = 1;; i++) {
                *s++ = (char)(dig = b.quoRem(S) + '0');
                if (i >= ilim)
                    break;
                b.mulAdd(10, 0);
            }

        // Round off last digit
        b.pow2Mul(1);
        j = b.cmp(S);
        if ((j > 0) || (j == 0 && (dig & 1 || biasUp))) {
          roundoff:
            while (*--s == '9')
                if (s == buf) {
                    k++;
                    *s++ = '1';
                    goto ret;
                }
            ++*s++;
        }
        else {
            // Strip trailing zeros
            while (*--s == '0') ;
            s++;
        }
      ret: ;
        // S, mLow, and mHigh are destroyed at the end of this block scope.
    }
  ret1:
    *s = '\0';
    *decpt = k + 1;
    return s;
}


// Mapping of DToStrMode -> doubleToAscii mode
static const int doubleToAsciiModes[] = {
    0,   // dtosStandard
    0,   // dtosStandardExponential
    3,   // dtosFixed
    2,   // dtosExponential
    2};  // dtosPrecision


// Convert value according to the given mode and return a pointer to the resulting ASCII string.
// The result is held somewhere in buffer, but not necessarily at the beginning.  The size of
// buffer is given in bufferSize, and must be at least as large as given by dtosStandardBufferSize
// or dtosVariableBufferSize, whichever is appropriate.
char *JS::doubleToStr(char *buffer, size_t DEBUG_ONLY(bufferSize), double value, DToStrMode mode, int precision)
{
    int decPt;                  // Position of decimal point relative to first digit returned by doubleToAscii
    bool negative;              // True if the sign bit was set in value
    int nDigits;                // Number of significand digits returned by doubleToAscii
    char *numBegin = buffer+2;  // Pointer to the digits returned by doubleToAscii; the +2 leaves space for
                                // the sign and/or decimal point
    char *numEnd;               // Pointer past the digits returned by doubleToAscii

    ASSERT(bufferSize >= (size_t)(mode <= dtosStandardExponential ? dtosStandardBufferSize : dtosVariableBufferSize(precision)));

    if (mode == dtosFixed && (value >= 1e21 || value <= -1e21))
        mode = dtosStandard;    // Change mode here rather than below because the buffer may not be large enough to hold a large integer.

    numEnd = doubleToAscii(value, doubleToAsciiModes[mode], mode >= dtosFixed, precision, &decPt, &negative, numBegin);
    nDigits = numEnd - numBegin;

    // If Infinity, -Infinity, or NaN, return the string regardless of the mode.
    if (decPt != 9999) {
        bool exponentialNotation = false;
        int minNDigits = 0;         // Minimum number of significand digits required by mode and precision
        char *p;
        char *q;

        switch (mode) {
            case dtosStandard:
                if (decPt < -5 || decPt > 21)
                    exponentialNotation = true;
                else
                    minNDigits = decPt;
                break;

            case dtosFixed:
                if (precision >= 0)
                    minNDigits = decPt + precision;
                else
                    minNDigits = decPt;
                break;

            case dtosExponential:
                ASSERT(precision > 0);
                minNDigits = precision;
                // Fall through
            case dtosStandardExponential:
                exponentialNotation = true;
                break;

            case dtosPrecision:
                ASSERT(precision > 0);
                minNDigits = precision;
                if (decPt < -5 || decPt > precision)
                    exponentialNotation = true;
                break;
        }

        // If the number has fewer than minNDigits, pad it with zeros at the end
        if (nDigits < minNDigits) {
            p = numBegin + minNDigits;
            nDigits = minNDigits;
            do {
                *numEnd++ = '0';
            } while (numEnd != p);
            *numEnd = '\0';
        }

        if (exponentialNotation) {
            // Insert a decimal point if more than one significand digit
            if (nDigits != 1) {
                numBegin--;
                numBegin[0] = numBegin[1];
                numBegin[1] = '.';
            }
            sprintf(numEnd, "e%+d", decPt-1);
        } else if (decPt != nDigits) {
            // Some kind of a fraction in fixed notation
            ASSERT(decPt <= nDigits);
            if (decPt > 0) {
                // dd...dd . dd...dd
                p = --numBegin;
                do {
                    *p = p[1];
                    p++;
                } while (--decPt);
                *p = '.';
            } else {
                // 0 . 00...00dd...dd
                p = numEnd;
                numEnd += 1 - decPt;
                q = numEnd;
                ASSERT(numEnd < buffer + bufferSize);
                *numEnd = '\0';
                while (p != numBegin)
                    *--q = *--p;
                for (p = numBegin + 1; p != q; p++)
                    *p = '0';
                *numBegin = '.';
                *--numBegin = '0';
            }
        }
    }

    // If negative and neither -0.0 nor NaN, output a leading '-'.
    if (negative &&
        !(word0(value) == Sign_bit && word1(value) == 0) &&
        !((word0(value) & Exp_mask) == Exp_mask &&
          (word1(value) || (word0(value) & Frac_mask)))) {
        *--numBegin = '-';
    }
    return numBegin;
}


inline char baseDigit(uint32 digit)
{
    if (digit >= 10)
        return char('a' - 10 + digit);
    else
        return char('0' + digit);
}


// Convert value to a string in the given base.  The integral part of value will be printed exactly
// in that base, regardless of how large it is, because there is no exponential notation for non-base-ten
// numbers.  The fractional part will be rounded to as few digits as possible while still preserving
// the round-trip property (analogous to that of printing decimal numbers).  In other words, if one were
// to read the resulting string in via a hypothetical base-number-reading routine that rounds to the nearest
// IEEE double (and to an even significand if there are two equally near doubles), then the result would
// equal value (except for -0.0, which converts to "0", and NaN, which is not equal to itself).
//
// Store the result in the given buffer, which must have at least dtobasesBufferSize bytes.
// Return the number of characters stored.
size_t JS::doubleToBaseStr(char *buffer, double value, uint base)
{
    ASSERT(base >= 2 && base <= 36);

    char *p = buffer;       // Pointer to current position in the buffer
    if (value < 0.0
#ifdef _WIN32
        && !((word0(value) & Exp_mask) == Exp_mask && ((word0(value) & Frac_mask) || word1(value))) // Visual C++ doesn't know how to compare against NaN
#endif
        ) {
        *p++ = '-';
        value = -value;
    }

    // Check for Infinity and NaN
    if ((word0(value) & Exp_mask) == Exp_mask) {
        strcpy(p, !word1(value) && !(word0(value) & Frac_mask) ? "Infinity" : "NaN");
        return strlen(buffer);
    }

    // Output the integer part of value with the digits in reverse order.
    char *pInt = p;                 // Pointer to the beginning of the integer part of the string
    double valueInt = fd::floor(value); // value truncated to an integer
    uint32 digit;
    if (valueInt <= 4294967295.0) {
        uint32 n = (uint32)valueInt;
        if (n)
            do {
                uint32 m = n / base;
                digit = n - m*base;
                n = m;
                ASSERT(digit < base);
                *p++ = baseDigit(digit);
            } while (n);
        else *p++ = '0';
    } else {
        int32 e;
        int32 bits;  // Number of significant bits in valueInt; not used.
        BigInt b;
        b.init(valueInt, e, bits);
        b.pow2Mul(e);
        do {
            digit = b.divRem(base);
            ASSERT(digit < base);
            *p++ = baseDigit(digit);
        } while (!b.isZero());
    }
    // Reverse the digits of the integer part of value.
    char *q = p-1;
    while (q > pInt) {
        char ch = *pInt;
        *pInt++ = *q;
        *q-- = ch;
    }

    double valueFrac = value - valueInt;        // The fractional part of value
    if (valueFrac != 0.0) {
        // We have a fraction.
        *p++ = '.';

        int32 e, bbits;
        BigInt b;
        b.init(valueFrac, e, bbits);
        ASSERT(e < 0);
        // At this point valueFrac = b * 2^e.  e must be less than zero because 0 < valueFrac < 1.

        int32 s2 = -int32(word0(value) >> Exp_shift1 & Exp_mask>>Exp_shift1);
#ifndef Sudden_Underflow
        if (!s2)
            s2 = -1;
#endif
        s2 += Bias + P;
        // 1/2^s2 = (nextDouble(value) - value)/2
        ASSERT(-s2 < e);

        BigInt mLow;
        BigInt mHigh;
        bool useMHigh = false;  // If false, assume that mHigh == mLow

        mLow.init(1);
        if (!word1(value) && !(word0(value) & Bndry_mask)
#ifndef Sudden_Underflow
            && word0(value) & (Exp_mask & Exp_mask << 1)
#endif
            ) {
            // The special case.  Here we want to be within a quarter of the last input
            // significant digit instead of one half of it when the output string's value is less than value.
            s2 += Log2P;
            useMHigh = true;
            mHigh.init(1u<<Log2P);
        }
        b.pow2Mul(e + s2);

        BigInt s;
        s.init(1);
        s.pow2Mul(s2);

        // At this point we have the following:
        //   s = 2^s2;
        //   1 > valueFrac = b/2^s2 > 0;
        //   (value - prevDouble(value))/2 = mLow/2^s2;
        //   (nextDouble(value) - value)/2 = mHigh/2^s2.

        bool done = false;
        do {
            int32 j, j1;

            b.mulAdd(base, 0);
            digit = b.quoRem2(s2);
            mLow.mulAdd(base, 0);
            if (useMHigh)
                mHigh.mulAdd(base, 0);

            // Do we yet have the shortest string that will round to value?
            j = b.cmp(mLow);
            // j is b/2^s2 compared with mLow/2^s2.
            {
                BigInt delta;
                delta.initDiff(s, useMHigh ? mHigh : mLow);
                j1 = delta.negative ? 1 : b.cmp(delta);
            }
            // j1 is b/2^s2 compared with 1 - mHigh/2^s2.

#ifndef ROUND_BIASED
            if (j1 == 0 && !(word1(value) & 1)) {
                if (j > 0)
                    digit++;
                done = true;
            } else
#endif
                if (j < 0 || (j == 0
#ifndef ROUND_BIASED
                              && !(word1(value) & 1)
#endif
                              )) {
                    if (j1 > 0) {
                        // Either dig or dig+1 would work here as the least significant digit.
                        // Use whichever would produce an output value closer to value.
                        b.pow2Mul(1);
                        j1 = b.cmp(s);
                        if (j1 > 0)
                            // The even test (|| (j1 == 0 && (digit & 1))) is not here because it messes up odd base output
                            // such as 3.5 in base 3.
                            digit++;
                    }
                    done = true;
                } else if (j1 > 0) {
                    digit++;
                    done = true;
                }
            ASSERT(digit < base);
            *p++ = baseDigit(digit);
        } while (!done);
    }
    ASSERT(p < buffer + dtobasesBufferSize);
    *p = '\0';
    return size_t(p - buffer);
}


// A version of doubleToStr that appends to the end of String dst.
// precision should not exceed 101.
void JS::appendDouble(String &dst, double value, DToStrMode mode, int precision)
{
    char buffer[dtosVariableBufferSize(101)];
    ASSERT(uint(precision) <= 101);

    dst += doubleToStr(buffer, sizeof buffer, value, mode, precision);
}


// A version of doubleToStr that prints to Formatter f.
// precision should not exceed 101.
void JS::printDouble(Formatter &f, double value, DToStrMode mode, int precision)
{
    char buffer[dtosVariableBufferSize(101)];
    ASSERT(uint(precision) <= 101);

    f << doubleToStr(buffer, sizeof buffer, value, mode, precision);
}
