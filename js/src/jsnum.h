/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsnum_h___
#define jsnum_h___

#include <math.h>
#if defined(XP_WIN) || defined(XP_OS2)
#include <float.h>
#endif
#ifdef SOLARIS
#include <ieeefp.h>
#endif
#include "jsvalue.h"

#include "jsstdint.h"
#include "jsstr.h"
#include "jsobj.h"

/*
 * JS number (IEEE double) interface.
 *
 * JS numbers are optimistically stored in the top 31 bits of 32-bit integers,
 * but floating point literals, results that overflow 31 bits, and division and
 * modulus operands and results require a 64-bit IEEE double.  These are GC'ed
 * and pointed to by 32-bit jsvals on the stack and in object properties.
 */

/*
 * The ARM architecture supports two floating point models: VFP and FPA. When
 * targetting FPA, doubles are mixed-endian on little endian ARMs (meaning that
 * the high and low words are in big endian order).
 */
#if defined(__arm) || defined(__arm32__) || defined(__arm26__) || defined(__arm__)
#if !defined(__VFP_FP__)
#define FPU_IS_ARM_FPA
#endif
#endif

typedef union jsdpun {
    struct {
#if defined(IS_LITTLE_ENDIAN) && !defined(FPU_IS_ARM_FPA)
        uint32 lo, hi;
#else
        uint32 hi, lo;
#endif
    } s;
    uint64   u64;
    jsdouble d;
} jsdpun;

static inline int
JSDOUBLE_IS_NaN(jsdouble d)
{
#ifdef WIN32
    return _isnan(d);
#else
    return isnan(d);
#endif
}

static inline int
JSDOUBLE_IS_FINITE(jsdouble d)
{
#ifdef WIN32
    return _finite(d);
#else
    return finite(d);
#endif
}

static inline int
JSDOUBLE_IS_INFINITE(jsdouble d)
{
#ifdef WIN32
    int c = _fpclass(d);
    return c == _FPCLASS_NINF || c == _FPCLASS_PINF;
#elif defined(SOLARIS)
    return !finite(d) && !isnan(d);
#else
    return isinf(d);
#endif
}

#define JSDOUBLE_HI32_SIGNBIT   0x80000000
#define JSDOUBLE_HI32_EXPMASK   0x7ff00000
#define JSDOUBLE_HI32_MANTMASK  0x000fffff
#define JSDOUBLE_HI32_NAN       0x7ff80000
#define JSDOUBLE_LO32_NAN       0x00000000

static inline bool
JSDOUBLE_IS_NEG(jsdouble d)
{
#ifdef WIN32
    return JSDOUBLE_IS_NEGZERO(d) || d < 0;
#elif defined(SOLARIS)
    return copysign(1, d) < 0;
#else
    return signbit(d);
#endif
}

static inline uint32
JS_HASH_DOUBLE(jsdouble d)
{
    jsdpun u;
    u.d = d;
    return u.s.lo ^ u.s.hi;
}

#if defined(XP_WIN)
#define JSDOUBLE_COMPARE(LVAL, OP, RVAL, IFNAN)                               \
    ((JSDOUBLE_IS_NaN(LVAL) || JSDOUBLE_IS_NaN(RVAL))                         \
     ? (IFNAN)                                                                \
     : (LVAL) OP (RVAL))
#else
#define JSDOUBLE_COMPARE(LVAL, OP, RVAL, IFNAN) ((LVAL) OP (RVAL))
#endif

extern jsdouble js_NaN;
extern jsdouble js_PositiveInfinity;
extern jsdouble js_NegativeInfinity;

/* Initialize number constants and runtime state for the first context. */
extern JSBool
js_InitRuntimeNumberState(JSContext *cx);

extern void
js_FinishRuntimeNumberState(JSContext *cx);

/* Initialize the Number class, returning its prototype object. */
extern js::Class js_NumberClass;

inline bool
JSObject::isNumber() const
{
    return getClass() == &js_NumberClass;
}

extern JSObject *
js_InitNumberClass(JSContext *cx, JSObject *obj);

/*
 * String constants for global function names, used in jsapi.c and jsnum.c.
 */
extern const char js_Infinity_str[];
extern const char js_NaN_str[];
extern const char js_isNaN_str[];
extern const char js_isFinite_str[];
extern const char js_parseFloat_str[];
extern const char js_parseInt_str[];

extern JSString * JS_FASTCALL
js_IntToString(JSContext *cx, jsint i);

/*
 * When base == 10, this function implements ToString() as specified by
 * ECMA-262-5 section 9.8.1; but note that it handles integers specially for
 * performance.  See also js::NumberToCString().
 */
extern JSString * JS_FASTCALL
js_NumberToString(JSContext *cx, jsdouble d);

/*
 * Convert an integer or double (contained in the given value) to a string and
 * append to the given buffer.
 */
extern JSBool JS_FASTCALL
js_NumberValueToCharBuffer(JSContext *cx, const js::Value &v, JSCharBuffer &cb);

namespace js {

/*
 * Usually a small amount of static storage is enough, but sometimes we need
 * to dynamically allocate much more.  This struct encapsulates that.
 * Dynamically allocated memory will be freed when the object is destroyed.
 */
struct ToCStringBuf
{
    /*
     * The longest possible result that would need to fit in sbuf is
     * (-0x80000000).toString(2), which has length 33.  Longer cases are
     * possible, but they'll go in dbuf.
     */
    static const size_t sbufSize = 34;
    char sbuf[sbufSize];
    char *dbuf;     /* must be allocated with js_malloc() */

    ToCStringBuf();
    ~ToCStringBuf();
};

/*
 * Convert a number to a C string.  When base==10, this function implements
 * ToString() as specified by ECMA-262-5 section 9.8.1.  It handles integral
 * values cheaply.  Return NULL if we ran out of memory.  See also
 * js_NumberToCString().
 */
extern char *
NumberToCString(JSContext *cx, ToCStringBuf *cbuf, jsdouble d, jsint base = 10);

/*
 * The largest positive integer such that all positive integers less than it
 * may be precisely represented using the IEEE-754 double-precision format.
 */
const double DOUBLE_INTEGRAL_PRECISION_LIMIT = uint64(1) << 53;

/*
 * Compute the positive integer of the given base described immediately at the
 * start of the range [start, end) -- no whitespace-skipping, no magical
 * leading-"0" octal or leading-"0x" hex behavior, no "+"/"-" parsing, just
 * reading the digits of the integer.  Return the index one past the end of the
 * digits of the integer in *endp, and return the integer itself in *dp.  If
 * base is 10 or a power of two the returned integer is the closest possible
 * double; otherwise extremely large integers may be slightly inaccurate.
 *
 * If [start, end) does not begin with a number with the specified base,
 * *dp == 0 and *endp == start upon return.
 */
extern bool
GetPrefixInteger(JSContext *cx, const jschar *start, const jschar *end, int base,
                 const jschar **endp, jsdouble *dp);

/*
 * Convert a value to a number, returning the converted value in 'out' if the
 * conversion succeeds.
 */
JS_ALWAYS_INLINE bool
ValueToNumber(JSContext *cx, const js::Value &v, double *out)
{
    if (v.isNumber()) {
        *out = v.toNumber();
        return true;
    }
    extern bool ValueToNumberSlow(JSContext *, js::Value, double *);
    return ValueToNumberSlow(cx, v, out);
}

/* Convert a value to a number, replacing 'vp' with the converted value. */
JS_ALWAYS_INLINE bool
ValueToNumber(JSContext *cx, js::Value *vp)
{
    if (vp->isNumber())
        return true;
    double d;
    extern bool ValueToNumberSlow(JSContext *, js::Value, double *);
    if (!ValueToNumberSlow(cx, *vp, &d))
        return false;
    vp->setNumber(d);
    return true;
}

/*
 * Convert a value to an int32 or uint32, according to the ECMA rules for
 * ToInt32 and ToUint32. Return converted value in *out on success, !ok on
 * failure.
 */
JS_ALWAYS_INLINE bool
ValueToECMAInt32(JSContext *cx, const js::Value &v, int32_t *out)
{
    if (v.isInt32()) {
        *out = v.toInt32();
        return true;
    }
    extern bool ValueToECMAInt32Slow(JSContext *, const js::Value &, int32_t *);
    return ValueToECMAInt32Slow(cx, v, out);
}

JS_ALWAYS_INLINE bool
ValueToECMAUint32(JSContext *cx, const js::Value &v, uint32_t *out)
{
    if (v.isInt32()) {
        *out = (uint32_t)v.toInt32();
        return true;
    }
    extern bool ValueToECMAUint32Slow(JSContext *, const js::Value &, uint32_t *);
    return ValueToECMAUint32Slow(cx, v, out);
}

/*
 * Convert a value to a number, then to an int32 if it fits by rounding to
 * nearest. Return converted value in *out on success, !ok on failure. As a
 * side effect, *vp will be mutated to match *out.
 */
JS_ALWAYS_INLINE bool
ValueToInt32(JSContext *cx, const js::Value &v, int32_t *out)
{
    if (v.isInt32()) {
        *out = v.toInt32();
        return true;
    }
    extern bool ValueToInt32Slow(JSContext *, const js::Value &, int32_t *);
    return ValueToInt32Slow(cx, v, out);
}

/*
 * Convert a value to a number, then to a uint16 according to the ECMA rules
 * for ToUint16. Return converted value on success, !ok on failure. v must be a
 * copy of a rooted value.
 */
JS_ALWAYS_INLINE bool
ValueToUint16(JSContext *cx, const js::Value &v, uint16_t *out)
{
    if (v.isInt32()) {
        *out = (uint16_t)v.toInt32();
        return true;
    }
    extern bool ValueToUint16Slow(JSContext *, const js::Value &, uint16_t *);
    return ValueToUint16Slow(cx, v, out);
}

}  /* namespace js */

/*
 * Specialized ToInt32 and ToUint32 converters for doubles.
 */
/*
 * From the ES3 spec, 9.5
 *  2.  If Result(1) is NaN, +0, -0, +Inf, or -Inf, return +0.
 *  3.  Compute sign(Result(1)) * floor(abs(Result(1))).
 *  4.  Compute Result(3) modulo 2^32; that is, a finite integer value k of Number
 *      type with positive sign and less than 2^32 in magnitude such the mathematical
 *      difference of Result(3) and k is mathematically an integer multiple of 2^32.
 *  5.  If Result(4) is greater than or equal to 2^31, return Result(4)- 2^32,
 *  otherwise return Result(4).
 */
static inline int32
js_DoubleToECMAInt32(jsdouble d)
{
#if defined(__i386__) || defined(__i386)
    jsdpun du, duh, two32;
    uint32 di_h, u_tmp, expon, shift_amount;
    int32 mask32;

    /*
     * Algorithm Outline
     *  Step 1. If d is NaN, +/-Inf or |d|>=2^84 or |d|<1, then return 0
     *          All of this is implemented based on an exponent comparison.
     *  Step 2. If |d|<2^31, then return (int)d
     *          The cast to integer (conversion in RZ mode) returns the correct result.
     *  Step 3. If |d|>=2^32, d:=fmod(d, 2^32) is taken  -- but without a call
     *  Step 4. If |d|>=2^31, then the fractional bits are cleared before
     *          applying the correction by 2^32:  d - sign(d)*2^32
     *  Step 5. Return (int)d
     */

    du.d = d;
    di_h = du.s.hi;

    u_tmp = (di_h & 0x7ff00000) - 0x3ff00000;
    if (u_tmp >= (0x45300000-0x3ff00000)) {
        // d is Nan, +/-Inf or +/-0, or |d|>=2^(32+52) or |d|<1, in which case result=0
        return 0;
    }

    if (u_tmp < 0x01f00000) {
        // |d|<2^31
        return int32_t(d);
    }

    if (u_tmp > 0x01f00000) {
        // |d|>=2^32
        expon = u_tmp >> 20;
        shift_amount = expon - 21;
        duh.u64 = du.u64;
        mask32 = 0x80000000;
        if (shift_amount < 32) {
            mask32 >>= shift_amount;
            duh.s.hi = du.s.hi & mask32;
            duh.s.lo = 0;
        } else {
            mask32 >>= (shift_amount-32);
            duh.s.hi = du.s.hi;
            duh.s.lo = du.s.lo & mask32;
        }
        du.d -= duh.d;
    }

    di_h = du.s.hi;

    // eliminate fractional bits
    u_tmp = (di_h & 0x7ff00000);
    if (u_tmp >= 0x41e00000) {
        // |d|>=2^31
        expon = u_tmp >> 20;
        shift_amount = expon - (0x3ff - 11);
        mask32 = 0x80000000;
        if (shift_amount < 32) {
            mask32 >>= shift_amount;
            du.s.hi &= mask32;
            du.s.lo = 0;
        } else {
            mask32 >>= (shift_amount-32);
            du.s.lo &= mask32;
        }
        two32.s.hi = 0x41f00000 ^ (du.s.hi & 0x80000000);
        two32.s.lo = 0;
        du.d -= two32.d;
    }

    return int32(du.d);
#elif defined (__arm__) && defined (__GNUC__)
    int32_t i;
    uint32_t    tmp0;
    uint32_t    tmp1;
    uint32_t    tmp2;
    asm (
    // We use a pure integer solution here. In the 'softfp' ABI, the argument
    // will start in r0 and r1, and VFP can't do all of the necessary ECMA
    // conversions by itself so some integer code will be required anyway. A
    // hybrid solution is faster on A9, but this pure integer solution is
    // notably faster for A8.

    // %0 is the result register, and may alias either of the %[QR]1 registers.
    // %Q4 holds the lower part of the mantissa.
    // %R4 holds the sign, exponent, and the upper part of the mantissa.
    // %1, %2 and %3 are used as temporary values.

    // Extract the exponent.
"   mov     %1, %R4, LSR #20\n"
"   bic     %1, %1, #(1 << 11)\n"  // Clear the sign.

    // Set the implicit top bit of the mantissa. This clobbers a bit of the
    // exponent, but we have already extracted that.
"   orr     %R4, %R4, #(1 << 20)\n"

    // Special Cases
    //   We should return zero in the following special cases:
    //    - Exponent is 0x000 - 1023: +/-0 or subnormal.
    //    - Exponent is 0x7ff - 1023: +/-INFINITY or NaN
    //      - This case is implicitly handled by the standard code path anyway,
    //        as shifting the mantissa up by the exponent will result in '0'.
    //
    // The result is composed of the mantissa, prepended with '1' and
    // bit-shifted left by the (decoded) exponent. Note that because the r1[20]
    // is the bit with value '1', r1 is effectively already shifted (left) by
    // 20 bits, and r0 is already shifted by 52 bits.
    
    // Adjust the exponent to remove the encoding offset. If the decoded
    // exponent is negative, quickly bail out with '0' as such values round to
    // zero anyway. This also catches +/-0 and subnormals.
"   sub     %1, %1, #0xff\n"
"   subs    %1, %1, #0x300\n"
"   bmi     8f\n"

    //  %1 = (decoded) exponent >= 0
    //  %R4 = upper mantissa and sign

    // ---- Lower Mantissa ----
"   subs    %3, %1, #52\n"         // Calculate exp-52
"   bmi     1f\n"

    // Shift r0 left by exp-52.
    // Ensure that we don't overflow ARM's 8-bit shift operand range.
    // We need to handle anything up to an 11-bit value here as we know that
    // 52 <= exp <= 1024 (0x400). Any shift beyond 31 bits results in zero
    // anyway, so as long as we don't touch the bottom 5 bits, we can use
    // a logical OR to push long shifts into the 32 <= (exp&0xff) <= 255 range.
"   bic     %2, %3, #0xff\n"
"   orr     %3, %3, %2, LSR #3\n"
    // We can now perform a straight shift, avoiding the need for any
    // conditional instructions or extra branches.
"   mov     %Q4, %Q4, LSL %3\n"
"   b       2f\n"
"1:\n" // Shift r0 right by 52-exp.
    // We know that 0 <= exp < 52, and we can shift up to 255 bits so 52-exp
    // will always be a valid shift and we can sk%3 the range check for this case.
"   rsb     %3, %1, #52\n"
"   mov     %Q4, %Q4, LSR %3\n"

    //  %1 = (decoded) exponent
    //  %R4 = upper mantissa and sign
    //  %Q4 = partially-converted integer

"2:\n"
    // ---- Upper Mantissa ----
    // This is much the same as the lower mantissa, with a few different
    // boundary checks and some masking to hide the exponent & sign bit in the
    // upper word.
    // Note that the upper mantissa is pre-shifted by 20 in %R4, but we shift
    // it left more to remove the sign and exponent so it is effectively
    // pre-shifted by 31 bits.
"   subs    %3, %1, #31\n"          // Calculate exp-31
"   mov     %1, %R4, LSL #11\n"     // Re-use %1 as a temporary register.
"   bmi     3f\n"

    // Shift %R4 left by exp-31.
    // Avoid overflowing the 8-bit shift range, as before.
"   bic     %2, %3, #0xff\n"
"   orr     %3, %3, %2, LSR #3\n"
    // Perform the shift.
"   mov     %2, %1, LSL %3\n"
"   b       4f\n"
"3:\n" // Shift r1 right by 31-exp.
    // We know that 0 <= exp < 31, and we can shift up to 255 bits so 31-exp
    // will always be a valid shift and we can skip the range check for this case.
"   rsb     %3, %3, #0\n"          // Calculate 31-exp from -(exp-31)
"   mov     %2, %1, LSR %3\n"      // Thumb-2 can't do "LSR %3" in "orr".

    //  %Q4 = partially-converted integer (lower)
    //  %R4 = upper mantissa and sign
    //  %2 = partially-converted integer (upper)

"4:\n"
    // Combine the converted parts.
"   orr     %Q4, %Q4, %2\n"
    // Negate the result if we have to, and move it to %0 in the process. To
    // avoid conditionals, we can do this by inverting on %R4[31], then adding
    // %R4[31]>>31.
"   eor     %Q4, %Q4, %R4, ASR #31\n"
"   add     %0, %Q4, %R4, LSR #31\n"
"   b       9f\n"
"8:\n"
    // +/-INFINITY, +/-0, subnormals, NaNs, and anything else out-of-range that
    // will result in a conversion of '0'.
"   mov     %0, #0\n"
"9:\n"
    : "=r" (i), "=&r" (tmp0), "=&r" (tmp1), "=&r" (tmp2)
    : "r" (d)
    : "cc"
        );
    return i;
#else
    int32 i;
    jsdouble two32, two31;

    if (!JSDOUBLE_IS_FINITE(d))
        return 0;

    i = (int32) d;
    if ((jsdouble) i == d)
        return i;

    two32 = 4294967296.0;
    two31 = 2147483648.0;
    d = fmod(d, two32);
    d = (d >= 0) ? floor(d) : ceil(d) + two32;
    return (int32) (d >= two31 ? d - two32 : d);
#endif
}

uint32
js_DoubleToECMAUint32(jsdouble d);

/*
 * Convert a jsdouble to an integral number, stored in a jsdouble.
 * If d is NaN, return 0.  If d is an infinity, return it without conversion.
 */
static inline jsdouble
js_DoubleToInteger(jsdouble d)
{
    if (d == 0)
        return d;

    if (!JSDOUBLE_IS_FINITE(d)) {
        if (JSDOUBLE_IS_NaN(d))
            return 0;
        return d;
    }

    JSBool neg = (d < 0);
    d = floor(neg ? -d : d);

    return neg ? -d : d;
}

/*
 * Similar to strtod except that it replaces overflows with infinities of the
 * correct sign, and underflows with zeros of the correct sign.  Guaranteed to
 * return the closest double number to the given input in dp.
 *
 * Also allows inputs of the form [+|-]Infinity, which produce an infinity of
 * the appropriate sign.  The case of the "Infinity" string must match exactly.
 * If the string does not contain a number, set *ep to s and return 0.0 in dp.
 * Return false if out of memory.
 */
extern JSBool
js_strtod(JSContext *cx, const jschar *s, const jschar *send,
          const jschar **ep, jsdouble *dp);

namespace js {

static JS_ALWAYS_INLINE bool
ValueFitsInInt32(const Value &v, int32_t *pi)
{
    if (v.isInt32()) {
        *pi = v.toInt32();
        return true;
    }
    return v.isDouble() && JSDOUBLE_IS_INT32(v.toDouble(), pi);
}

template<typename T> struct NumberTraits { };
template<> struct NumberTraits<int32> {
  static JS_ALWAYS_INLINE int32 NaN() { return 0; }
  static JS_ALWAYS_INLINE int32 toSelfType(int32 i) { return i; }
  static JS_ALWAYS_INLINE int32 toSelfType(jsdouble d) { return js_DoubleToECMAUint32(d); }
};
template<> struct NumberTraits<jsdouble> {
  static JS_ALWAYS_INLINE jsdouble NaN() { return js_NaN; }
  static JS_ALWAYS_INLINE jsdouble toSelfType(int32 i) { return i; }
  static JS_ALWAYS_INLINE jsdouble toSelfType(jsdouble d) { return d; }
};

template<typename T>
static JS_ALWAYS_INLINE T
StringToNumberType(JSContext *cx, JSString *str)
{
    if (str->length() == 1) {
        jschar c = str->chars()[0];
        if ('0' <= c && c <= '9')
            return NumberTraits<T>::toSelfType(T(c - '0'));
        if (JS_ISSPACE(c))
            return NumberTraits<T>::toSelfType(T(0));
        return NumberTraits<T>::NaN();
    }

    const jschar* bp;
    const jschar* end;
    const jschar* ep;
    jsdouble d;

    str->getCharsAndEnd(bp, end);
    bp = js_SkipWhiteSpace(bp, end);

    /* ECMA doesn't allow signed hex numbers (bug 273467). */
    if (end - bp >= 2 && bp[0] == '0' && (bp[1] == 'x' || bp[1] == 'X')) {
        /* Looks like a hex number. */
        const jschar *endptr;
        if (!GetPrefixInteger(cx, bp + 2, end, 16, &endptr, &d) ||
            js_SkipWhiteSpace(endptr, end) != end) {
            return NumberTraits<T>::NaN();
        }
        return NumberTraits<T>::toSelfType(d);
    }

    /*
     * Note that ECMA doesn't treat a string beginning with a '0' as
     * an octal number here. This works because all such numbers will
     * be interpreted as decimal by js_strtod.  Also, any hex numbers
     * that have made it here (which can only be negative ones) will
     * be treated as 0 without consuming the 'x' by js_strtod.
     */
    if (!js_strtod(cx, bp, end, &ep, &d) ||
        js_SkipWhiteSpace(ep, end) != end) {
        return NumberTraits<T>::NaN();
    }

    return NumberTraits<T>::toSelfType(d);
}
}

#endif /* jsnum_h___ */
