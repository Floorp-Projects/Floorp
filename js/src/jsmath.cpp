/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
 * JS math package.
 */
#include <stdlib.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jslong.h"
#include "prmjtime.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jslibmath.h"
#include "jscompartment.h"

using namespace js;

#ifndef M_E
#define M_E             2.7182818284590452354
#endif
#ifndef M_LOG2E
#define M_LOG2E         1.4426950408889634074
#endif
#ifndef M_LOG10E
#define M_LOG10E        0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2           0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10          2.30258509299404568402
#endif
#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2         1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2       0.70710678118654752440
#endif

static JSConstDoubleSpec math_constants[] = {
    {M_E,       "E",            0, {0,0,0}},
    {M_LOG2E,   "LOG2E",        0, {0,0,0}},
    {M_LOG10E,  "LOG10E",       0, {0,0,0}},
    {M_LN2,     "LN2",          0, {0,0,0}},
    {M_LN10,    "LN10",         0, {0,0,0}},
    {M_PI,      "PI",           0, {0,0,0}},
    {M_SQRT2,   "SQRT2",        0, {0,0,0}},
    {M_SQRT1_2, "SQRT1_2",      0, {0,0,0}},
    {0,0,0,{0,0,0}}
};

MathCache::MathCache() {
    memset(table, 0, sizeof(table));

    /* See comments in lookup(). */
    JS_ASSERT(JSDOUBLE_IS_NEGZERO(-0.0));
    JS_ASSERT(!JSDOUBLE_IS_NEGZERO(+0.0));
    JS_ASSERT(hash(-0.0) != hash(+0.0));
}

Class js_MathClass = {
    js_Math_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Math),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

JSBool
js_math_abs(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = fabs(x);
    vp->setNumber(z);
    return JS_TRUE;
}

static JSBool
math_acos(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < -1 || 1 < x) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
#endif
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(acos, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static JSBool
math_asin(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < -1 || 1 < x) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
#endif
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(asin, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static JSBool
math_atan(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(atan, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static inline jsdouble JS_FASTCALL
math_atan2_kernel(jsdouble x, jsdouble y)
{
#if defined(_MSC_VER)
    /*
     * MSVC's atan2 does not yield the result demanded by ECMA when both x
     * and y are infinite.
     * - The result is a multiple of pi/4.
     * - The sign of x determines the sign of the result.
     * - The sign of y determines the multiplicator, 1 or 3.
     */
    if (JSDOUBLE_IS_INFINITE(x) && JSDOUBLE_IS_INFINITE(y)) {
        jsdouble z = js_copysign(M_PI / 4, x);
        if (y < 0)
            z *= 3;
        return z;
    }
#endif

#if defined(SOLARIS) && defined(__GNUC__)
    if (x == 0) {
        if (JSDOUBLE_IS_NEGZERO(y))
            return js_copysign(M_PI, x);
        if (y == 0)
            return x;
    }
#endif
    return atan2(x, y);
}

static JSBool
math_atan2(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, y, z;

    if (argc <= 1) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    if (!ValueToNumber(cx, vp[3], &y))
        return JS_FALSE;
    z = math_atan2_kernel(x, y);
    vp->setDouble(z);
    return JS_TRUE;
}

jsdouble
js_math_ceil_impl(jsdouble x)
{
#ifdef __APPLE__
    if (x < 0 && x > -1.0)
        return js_copysign(0, -1);
#endif
    return ceil(x);
}

JSBool
js_math_ceil(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = js_math_ceil_impl(x);
    vp->setNumber(z);
    return JS_TRUE;
}

static JSBool
math_cos(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(cos, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static double
math_exp_body(double d)
{
#ifdef _WIN32
    if (!JSDOUBLE_IS_NaN(d)) {
        if (d == js_PositiveInfinity)
            return js_PositiveInfinity;
        if (d == js_NegativeInfinity)
            return 0.0;
    }
#endif
    return exp(d);
}

static JSBool
math_exp(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(math_exp_body, x);
    vp->setNumber(z);
    return JS_TRUE;
}

jsdouble
js_math_floor_impl(jsdouble x)
{
    return floor(x);
}

JSBool
js_math_floor(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = js_math_floor_impl(x);
    vp->setNumber(z);
    return JS_TRUE;
}

static JSBool
math_log(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
#endif
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(log, x);
    vp->setNumber(z);
    return JS_TRUE;
}

JSBool
js_math_max(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z = js_NegativeInfinity;
    Value *argv;
    uintN i;

    if (argc == 0) {
        vp->setDouble(js_NegativeInfinity);
        return JS_TRUE;
    }
    argv = vp + 2;
    for (i = 0; i < argc; i++) {
        if (!ValueToNumber(cx, argv[i], &x))
            return JS_FALSE;
        if (JSDOUBLE_IS_NaN(x)) {
            vp->setDouble(js_NaN);
            return JS_TRUE;
        }
        if (x == 0 && x == z) {
            if (js_copysign(1.0, z) == -1)
                z = x;
        } else {
            z = (x > z) ? x : z;
        }
    }
    vp->setNumber(z);
    return JS_TRUE;
}

JSBool
js_math_min(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z = js_PositiveInfinity;
    Value *argv;
    uintN i;

    if (argc == 0) {
        vp->setDouble(js_PositiveInfinity);
        return JS_TRUE;
    }
    argv = vp + 2;
    for (i = 0; i < argc; i++) {
        if (!ValueToNumber(cx, argv[i], &x))
            return JS_FALSE;
        if (JSDOUBLE_IS_NaN(x)) {
            vp->setDouble(js_NaN);
            return JS_TRUE;
        }
        if (x == 0 && x == z) {
            if (js_copysign(1.0, x) == -1)
                z = x;
        } else {
            z = (x < z) ? x : z;
        }
    }
    vp->setNumber(z);
    return JS_TRUE;
}

static jsdouble
powi(jsdouble x, jsint y)
{
    jsuint n = (y < 0) ? -y : y;
    jsdouble m = x;
    jsdouble p = 1;
    while (true) {
        if ((n & 1) != 0) p *= m;
        n >>= 1;
        if (n == 0) {
            if (y < 0) {
                // Unfortunately, we have to be careful when p has reached
                // infinity in the computation, because sometimes the higher
                // internal precision in the pow() implementation would have
                // given us a finite p. This happens very rarely.
                
                jsdouble result = 1.0 / p;
                return (result == 0 && JSDOUBLE_IS_INFINITE(p))
                       ? pow(x, static_cast<jsdouble>(y))  // Avoid pow(double, int).
                       : result;
            }

            return p;
        }
        m *= m;
    }
}

static JSBool
math_pow(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, y, z;

    if (argc <= 1) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    if (!ValueToNumber(cx, vp[3], &y))
        return JS_FALSE;
    /*
     * Special case for square roots. Note that pow(x, 0.5) != sqrt(x)
     * when x = -0.0, so we have to guard for this.
     */
    if (JSDOUBLE_IS_FINITE(x) && x != 0.0) {
        if (y == 0.5) {
            vp->setNumber(sqrt(x));
            return JS_TRUE;
        }
        if (y == -0.5) {
            vp->setNumber(1.0/sqrt(x));
            return JS_TRUE;
        }
    }
    /*
     * Because C99 and ECMA specify different behavior for pow(),
     * we need to wrap the libm call to make it ECMA compliant.
     */
    if (!JSDOUBLE_IS_FINITE(y) && (x == 1.0 || x == -1.0)) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    /* pow(x, +-0) is always 1, even for x = NaN. */
    if (y == 0) {
        vp->setInt32(1);
        return JS_TRUE;
    }

    if (vp[3].isInt32())
        z = powi(x, vp[3].toInt32());
    else
        z = pow(x, y);

    vp->setNumber(z);
    return JS_TRUE;
}

static const int64 RNG_MULTIPLIER = 0x5DEECE66DLL;
static const int64 RNG_ADDEND = 0xBLL;
static const int64 RNG_MASK = (1LL << 48) - 1;
static const jsdouble RNG_DSCALE = jsdouble(1LL << 53);

/*
 * Math.random() support, lifted from java.util.Random.java.
 */
static inline void
random_setSeed(JSContext *cx, int64 seed)
{
    cx->rngSeed = (seed ^ RNG_MULTIPLIER) & RNG_MASK;
}

void
js_InitRandom(JSContext *cx)
{
    /*
     * Set the seed from current time. Since we have a RNG per context and we often bring
     * up several contexts at the same time, we xor in some additional values, namely
     * the context and its successor. We don't just use the context because it might be
     * possible to reverse engineer the context pointer if one guesses the time right.
     */
    random_setSeed(cx,
                   (PRMJ_Now() / 1000) ^
                   int64(cx) ^
                   int64(cx->link.next));
}

static inline uint64
random_next(JSContext *cx, int bits)
{
    uint64 nextseed = cx->rngSeed * RNG_MULTIPLIER;
    nextseed += RNG_ADDEND;
    nextseed &= RNG_MASK;
    cx->rngSeed = nextseed;
    return nextseed >> (48 - bits);
}

static inline jsdouble
random_nextDouble(JSContext *cx)
{
    return jsdouble((random_next(cx, 26) << 27) + random_next(cx, 27)) / RNG_DSCALE;
}

static JSBool
math_random(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble z = random_nextDouble(cx);
    vp->setDouble(z);
    return JS_TRUE;
}

#if defined _WIN32 && !defined WINCE && _MSC_VER < 1400
/* Try to work around apparent _copysign bustage in VC7.x. */
double
js_copysign(double x, double y)
{
    jsdpun xu, yu;

    xu.d = x;
    yu.d = y;
    xu.s.hi &= ~JSDOUBLE_HI32_SIGNBIT;
    xu.s.hi |= yu.s.hi & JSDOUBLE_HI32_SIGNBIT;
    return xu.d;
}
#endif

jsdouble
js_math_round_impl(jsdouble x)
{
    return js_copysign(floor(x + 0.5), x);
}

JSBool
js_math_round(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = js_copysign(floor(x + 0.5), x);
    vp->setNumber(z);
    return JS_TRUE;
}

static JSBool
math_sin(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(sin, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static JSBool
math_sqrt(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(sqrt, x);
    vp->setDouble(z);
    return JS_TRUE;
}

static JSBool
math_tan(JSContext *cx, uintN argc, Value *vp)
{
    jsdouble x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = GetMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(tan, x);
    vp->setDouble(z);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
static JSBool
math_toSource(JSContext *cx, uintN argc, Value *vp)
{
    vp->setString(ATOM_TO_STRING(CLASS_ATOM(cx, Math)));
    return JS_TRUE;
}
#endif

#ifdef JS_TRACER

#define MATH_BUILTIN_1(name, cfun)                                            \
    static jsdouble FASTCALL name##_tn(MathCache *cache, jsdouble d) {        \
        return cache->lookup(cfun, d);                                        \
    }                                                                         \
    JS_DEFINE_TRCINFO_1(name,                                                 \
        (2, (static, DOUBLE, name##_tn, MATHCACHE, DOUBLE, 1, nanojit::ACCSET_NONE)))

MATH_BUILTIN_1(js_math_abs, fabs)
MATH_BUILTIN_1(math_atan, atan)
MATH_BUILTIN_1(math_sin, sin)
MATH_BUILTIN_1(math_cos, cos)
MATH_BUILTIN_1(math_sqrt, sqrt)
MATH_BUILTIN_1(math_tan, tan)

static jsdouble FASTCALL
math_acos_tn(MathCache *cache, jsdouble d)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (d < -1 || 1 < d) {
        return js_NaN;
    }
#endif
    return cache->lookup(acos, d);
}

static jsdouble FASTCALL
math_asin_tn(MathCache *cache, jsdouble d)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (d < -1 || 1 < d) {
        return js_NaN;
    }
#endif
    return cache->lookup(asin, d);
}

static jsdouble FASTCALL
math_exp_tn(MathCache *cache, jsdouble d)
{
    return cache->lookup(math_exp_body, d);
}

JS_DEFINE_TRCINFO_1(math_exp,
    (2, (static, DOUBLE, math_exp_tn, MATHCACHE, DOUBLE,  1, nanojit::ACCSET_NONE)))

static jsdouble FASTCALL
math_log_tn(MathCache *cache, jsdouble d)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (d < 0)
        return js_NaN;
#endif
    return cache->lookup(log, d);
}

static jsdouble FASTCALL
math_max_tn(jsdouble d, jsdouble p)
{
    if (JSDOUBLE_IS_NaN(d) || JSDOUBLE_IS_NaN(p))
        return js_NaN;

    if (p == 0 && p == d) {
        // Max prefers 0.0 to -0.0.
        if (js_copysign(1.0, d) == -1)
            return p;
        return d;
    }
    return (p > d) ? p : d;
}

static jsdouble FASTCALL
math_min_tn(jsdouble d, jsdouble p)
{
    if (JSDOUBLE_IS_NaN(d) || JSDOUBLE_IS_NaN(p))
        return js_NaN;

    if (p == 0 && p == d) {
        // Min prefers -0.0 to 0.0.
        if (js_copysign (1.0, p) == -1)
            return p;
        return d;
    }
    return (p < d) ? p : d;
}

static jsdouble FASTCALL
math_pow_tn(jsdouble d, jsdouble p)
{
    /*
     * Special case for square roots. Note that pow(x, 0.5) != sqrt(x)
     * when x = -0.0, so we have to guard for this.
     */
    if (JSDOUBLE_IS_FINITE(d) && d != 0.0) {
        if (p == 0.5)
            return sqrt(d);

        if (p == -0.5)
            return 1.0/sqrt(d);
    }
    if (!JSDOUBLE_IS_FINITE(p) && (d == 1.0 || d == -1.0))
        return js_NaN;
    if (p == 0)
        return 1.0;
    int32_t i;
    if (JSDOUBLE_IS_INT32(p, &i))
        return powi(d, i);

    return pow(d, p);
}

static jsdouble FASTCALL
math_random_tn(JSContext *cx)
{
    return random_nextDouble(cx);
}

static jsdouble FASTCALL
math_round_tn(jsdouble x)
{
    return js_math_round_impl(x);
}

static jsdouble FASTCALL
math_ceil_tn(jsdouble x)
{
    return js_math_ceil_impl(x);
}

static jsdouble FASTCALL
math_floor_tn(jsdouble x)
{
    return js_math_floor_impl(x);
}

JS_DEFINE_TRCINFO_1(math_acos,
    (2, (static, DOUBLE, math_acos_tn, MATHCACHE, DOUBLE, 1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(math_asin,
    (2, (static, DOUBLE, math_asin_tn, MATHCACHE, DOUBLE, 1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(math_atan2,
    (2, (static, DOUBLE, math_atan2_kernel, DOUBLE, DOUBLE, 1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(js_math_floor,
    (1, (static, DOUBLE, math_floor_tn, DOUBLE,         1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(math_log,
    (2, (static, DOUBLE, math_log_tn, MATHCACHE, DOUBLE,  1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(js_math_max,
    (2, (static, DOUBLE, math_max_tn, DOUBLE, DOUBLE,   1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(js_math_min,
    (2, (static, DOUBLE, math_min_tn, DOUBLE, DOUBLE,   1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(math_pow,
    (2, (static, DOUBLE, math_pow_tn, DOUBLE, DOUBLE,   1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(math_random,
    (1, (static, DOUBLE, math_random_tn, CONTEXT,       0, nanojit::ACCSET_STORE_ANY)))
JS_DEFINE_TRCINFO_1(js_math_round,
    (1, (static, DOUBLE, math_round_tn, DOUBLE,         1, nanojit::ACCSET_NONE)))
JS_DEFINE_TRCINFO_1(js_math_ceil,
    (1, (static, DOUBLE, math_ceil_tn, DOUBLE,          1, nanojit::ACCSET_NONE)))

#endif /* JS_TRACER */

static JSFunctionSpec math_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  math_toSource,        0, 0),
#endif
    JS_TN("abs",            js_math_abs,          1, 0, &js_math_abs_trcinfo),
    JS_TN("acos",           math_acos,            1, 0, &math_acos_trcinfo),
    JS_TN("asin",           math_asin,            1, 0, &math_asin_trcinfo),
    JS_TN("atan",           math_atan,            1, 0, &math_atan_trcinfo),
    JS_TN("atan2",          math_atan2,           2, 0, &math_atan2_trcinfo),
    JS_TN("ceil",           js_math_ceil,         1, 0, &js_math_ceil_trcinfo),
    JS_TN("cos",            math_cos,             1, 0, &math_cos_trcinfo),
    JS_TN("exp",            math_exp,             1, 0, &math_exp_trcinfo),
    JS_TN("floor",          js_math_floor,        1, 0, &js_math_floor_trcinfo),
    JS_TN("log",            math_log,             1, 0, &math_log_trcinfo),
    JS_TN("max",            js_math_max,          2, 0, &js_math_max_trcinfo),
    JS_TN("min",            js_math_min,          2, 0, &js_math_min_trcinfo),
    JS_TN("pow",            math_pow,             2, 0, &math_pow_trcinfo),
    JS_TN("random",         math_random,          0, 0, &math_random_trcinfo),
    JS_TN("round",          js_math_round,        1, 0, &js_math_round_trcinfo),
    JS_TN("sin",            math_sin,             1, 0, &math_sin_trcinfo),
    JS_TN("sqrt",           math_sqrt,            1, 0, &math_sqrt_trcinfo),
    JS_TN("tan",            math_tan,             1, 0, &math_tan_trcinfo),
    JS_FS_END
};

bool
js_IsMathFunction(JSNative native)
{
    for (size_t i=0; math_static_methods[i].name != NULL; i++) {
        if (native == math_static_methods[i].call)
            return true;
    }
    return false;
}

JSObject *
js_InitMathClass(JSContext *cx, JSObject *obj)
{
    JSObject *Math;

    Math = JS_NewObject(cx, Jsvalify(&js_MathClass), NULL, obj);
    if (!Math)
        return NULL;
    if (!JS_DefineProperty(cx, obj, js_Math_str, OBJECT_TO_JSVAL(Math),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    if (!JS_DefineFunctions(cx, Math, math_static_methods))
        return NULL;
    if (!JS_DefineConstDoubles(cx, Math, math_constants))
        return NULL;
    return Math;
}
