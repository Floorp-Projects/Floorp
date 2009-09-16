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

/*
 * JS number (IEEE double) interface.
 *
 * JS numbers are optimistically stored in the top 31 bits of 32-bit integers,
 * but floating point literals, results that overflow 31 bits, and division and
 * modulus operands and results require a 64-bit IEEE double.  These are GC'ed
 * and pointed to by 32-bit jsvals on the stack and in object properties.
 */

JS_BEGIN_EXTERN_C

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

static inline int
JSDOUBLE_IS_NEGZERO(jsdouble d)
{
#ifdef WIN32
    return (d == 0 && (_fpclass(d) & _FPCLASS_NZ));
#elif defined(SOLARIS)
    return (d == 0 && copysign(1, d) < 0);
#else
    return (d == 0 && signbit(d));
#endif
}

#define JSDOUBLE_HI32_SIGNBIT   0x80000000
#define JSDOUBLE_HI32_EXPMASK   0x7ff00000
#define JSDOUBLE_HI32_MANTMASK  0x000fffff

static inline int
JSDOUBLE_IS_INT(jsdouble d, jsint& i)
{
    if (JSDOUBLE_IS_NEGZERO(d))
        return false;
    return d == (i = jsint(d));
}

static inline int
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

/* Initialize number constants and runtime state for the first context. */
extern JSBool
js_InitRuntimeNumberState(JSContext *cx);

extern void
js_TraceRuntimeNumberState(JSTracer *trc);

extern void
js_FinishRuntimeNumberState(JSContext *cx);

/* Initialize the Number class, returning its prototype object. */
extern JSClass js_NumberClass;

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

/*
 * vp must be a root.
 */
extern JSBool
js_NewNumberInRootedValue(JSContext *cx, jsdouble d, jsval *vp);

/*
 * Create a weakly rooted integer or double jsval as appropriate for the given
 * jsdouble.
 */
extern JSBool
js_NewWeaklyRootedNumber(JSContext *cx, jsdouble d, jsval *vp);

/* Convert a number to a GC'ed string. */
extern JSString * JS_FASTCALL
js_NumberToString(JSContext *cx, jsdouble d);

/*
 * Convert an integer or double (contained in the given jsval) to a string and
 * append to the given buffer.
 */
extern JSBool JS_FASTCALL
js_NumberValueToCharBuffer(JSContext *cx, jsval v, JSCharBuffer &cb);

/*
 * Convert a value to a number. On exit JSVAL_IS_NULL(*vp) iff there was an
 * error. If on exit JSVAL_IS_NUMBER(*vp), then *vp holds the jsval that
 * matches the result. Otherwise *vp is JSVAL_TRUE indicating that the jsval
 * for result has to be created explicitly using, for example, the
 * js_NewNumberInRootedValue function.
 */
extern jsdouble
js_ValueToNumber(JSContext *cx, jsval* vp);

/*
 * Convert a value to an int32 or uint32, according to the ECMA rules for
 * ToInt32 and ToUint32. On exit JSVAL_IS_NULL(*vp) iff there was an error. If
 * on exit JSVAL_IS_INT(*vp), then *vp holds the jsval matching the result.
 * Otherwise *vp is JSVAL_TRUE indicating that the jsval for result has to be
 * created explicitly using, for example, the js_NewNumberInRootedValue
 * function.
 */
extern int32
js_ValueToECMAInt32(JSContext *cx, jsval *vp);

extern uint32
js_ValueToECMAUint32(JSContext *cx, jsval *vp);

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
#ifdef __i386__
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

extern uint32
js_DoubleToECMAUint32(jsdouble d);

/*
 * Convert a value to a number, then to an int32 if it fits by rounding to
 * nearest; but failing with an error report if the double is out of range
 * or unordered. On exit JSVAL_IS_NULL(*vp) iff there was an error. If on exit
 * JSVAL_IS_INT(*vp), then *vp holds the jsval matching the result. Otherwise
 * *vp is JSVAL_TRUE indicating that the jsval for result has to be created
 * explicitly using, for example, the js_NewNumberInRootedValue function.
 */
extern int32
js_ValueToInt32(JSContext *cx, jsval *vp);

/*
 * Convert a value to a number, then to a uint16 according to the ECMA rules
 * for ToUint16. On exit JSVAL_IS_NULL(*vp) iff there was an error, otherwise
 * vp is jsval matching the result.
 */
extern uint16
js_ValueToUint16(JSContext *cx, jsval *vp);

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

/*
 * Similar to strtol except that it handles integers of arbitrary size.
 * Guaranteed to return the closest double number to the given input when radix
 * is 10 or a power of 2.  Callers may see round-off errors for very large
 * numbers of a different radix than 10 or a power of 2.
 *
 * If the string does not contain a number, set *ep to s and return 0.0 in dp.
 * Return false if out of memory.
 */
extern JSBool
js_strtointeger(JSContext *cx, const jschar *s, const jschar *send,
                const jschar **ep, jsint radix, jsdouble *dp);

JS_END_EXTERN_C

#endif /* jsnum_h___ */
