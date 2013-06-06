/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS math package.
 */

#if defined(XP_WIN)
/* _CRT_RAND_S must be #defined before #including stdlib.h to get rand_s(). */
#define _CRT_RAND_S
#endif

#include "jsmath.h"

#include "mozilla/Constants.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"

#include <fcntl.h>
#include <stdlib.h>

#ifdef XP_UNIX
# include <unistd.h>
#endif

#include "jstypes.h"
#include "prmjtime.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jslibmath.h"
#include "jscompartment.h"

#include "jsobjinlines.h"

using namespace js;

using mozilla::Abs;
using mozilla::DoubleIsInt32;
using mozilla::ExponentComponent;
using mozilla::IsFinite;
using mozilla::IsInfinite;
using mozilla::IsNaN;
using mozilla::IsNegative;
using mozilla::IsNegativeZero;
using mozilla::PositiveInfinity;
using mozilla::NegativeInfinity;
using mozilla::SpecificNaN;

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
    JS_ASSERT(IsNegativeZero(-0.0));
    JS_ASSERT(!IsNegativeZero(+0.0));
    JS_ASSERT(hash(-0.0) != hash(+0.0));
}

size_t
MathCache::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(this);
}

Class js::MathClass = {
    js_Math_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Math),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

JSBool
js_math_abs(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = Abs(x);
    vp->setNumber(z);
    return JS_TRUE;
}

double
js::math_acos_impl(MathCache *cache, double x)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < -1 || 1 < x)
        return js_NaN;
#endif
    return cache->lookup(acos, x);
}

JSBool
js::math_acos(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_acos_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js::math_asin_impl(MathCache *cache, double x)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < -1 || 1 < x)
        return js_NaN;
#endif
    return cache->lookup(asin, x);
}

JSBool
js::math_asin(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_asin_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js::math_atan_impl(MathCache *cache, double x)
{
    return cache->lookup(atan, x);
}

JSBool
js::math_atan(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_atan_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js::ecmaAtan2(double y, double x)
{
#if defined(_MSC_VER)
    /*
     * MSVC's atan2 does not yield the result demanded by ECMA when both x
     * and y are infinite.
     * - The result is a multiple of pi/4.
     * - The sign of y determines the sign of the result.
     * - The sign of x determines the multiplicator, 1 or 3.
     */
    if (IsInfinite(y) && IsInfinite(x)) {
        double z = js_copysign(M_PI / 4, y);
        if (x < 0)
            z *= 3;
        return z;
    }
#endif

#if defined(SOLARIS) && defined(__GNUC__)
    if (y == 0) {
        if (IsNegativeZero(x))
            return js_copysign(M_PI, y);
        if (x == 0)
            return y;
    }
#endif
    return atan2(y, x);
}

JSBool
js::math_atan2(JSContext *cx, unsigned argc, Value *vp)
{
    double x, y, z;

    if (argc <= 1) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x) || !ToNumber(cx, vp[3], &y))
        return JS_FALSE;
    z = ecmaAtan2(x, y);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js_math_ceil_impl(double x)
{
#ifdef __APPLE__
    if (x < 0 && x > -1.0)
        return js_copysign(0, -1);
#endif
    return ceil(x);
}

JSBool
js_math_ceil(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = js_math_ceil_impl(x);
    vp->setNumber(z);
    return JS_TRUE;
}

double
js::math_cos_impl(MathCache *cache, double x)
{
    return cache->lookup(cos, x);
}

JSBool
js::math_cos(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_cos_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js::math_exp_impl(MathCache *cache, double x)
{
#ifdef _WIN32
    if (!IsNaN(x)) {
        if (x == js_PositiveInfinity)
            return js_PositiveInfinity;
        if (x == js_NegativeInfinity)
            return 0.0;
    }
#endif
    return cache->lookup(exp, x);
}

JSBool
js::math_exp(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_exp_impl(mathCache, x);
    vp->setNumber(z);
    return JS_TRUE;
}

double
js_math_floor_impl(double x)
{
    return floor(x);
}

JSBool
js_math_floor(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    z = js_math_floor_impl(x);
    vp->setNumber(z);
    return JS_TRUE;
}

JSBool
js::math_imul(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    uint32_t a = 0, b = 0;
    if (args.hasDefined(0) && !ToUint32(cx, args[0], &a))
        return false;
    if (args.hasDefined(1) && !ToUint32(cx, args[1], &b))
        return false;

    uint32_t product = a * b;
    args.rval().setInt32(product > INT32_MAX
                         ? int32_t(INT32_MIN + (product - INT32_MAX - 1))
                         : int32_t(product));
    return true;
}

double
js::math_log_impl(MathCache *cache, double x)
{
#if defined(SOLARIS) && defined(__GNUC__)
    if (x < 0)
        return js_NaN;
#endif
    return cache->lookup(log, x);
}

JSBool
js::math_log(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_log_impl(mathCache, x);
    vp->setNumber(z);
    return JS_TRUE;
}

JSBool
js_math_max(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    double x;
    double maxval = NegativeInfinity();
    for (unsigned i = 0; i < args.length(); i++) {
        if (!ToNumber(cx, args[i], &x))
            return false;
        // Math.max(num, NaN) => NaN, Math.max(-0, +0) => +0
        if (x > maxval || IsNaN(x) || (x == maxval && IsNegative(maxval)))
            maxval = x;
    }
    args.rval().setNumber(maxval);
    return true;
}

JSBool
js_math_min(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    double x;
    double minval = PositiveInfinity();
    for (unsigned i = 0; i < args.length(); i++) {
        if (!ToNumber(cx, args[i], &x))
            return false;
        // Math.min(num, NaN) => NaN, Math.min(-0, +0) => -0
        if (x < minval || IsNaN(x) || (x == minval && IsNegativeZero(x)))
            minval = x;
    }
    args.rval().setNumber(minval);
    return true;
}

// Disable PGO for Math.pow() and related functions (see bug 791214).
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif
double
js::powi(double x, int y)
{
    unsigned n = (y < 0) ? -y : y;
    double m = x;
    double p = 1;
    while (true) {
        if ((n & 1) != 0) p *= m;
        n >>= 1;
        if (n == 0) {
            if (y < 0) {
                // Unfortunately, we have to be careful when p has reached
                // infinity in the computation, because sometimes the higher
                // internal precision in the pow() implementation would have
                // given us a finite p. This happens very rarely.

                double result = 1.0 / p;
                return (result == 0 && IsInfinite(p))
                       ? pow(x, static_cast<double>(y))  // Avoid pow(double, int).
                       : result;
            }

            return p;
        }
        m *= m;
    }
}
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

// Disable PGO for Math.pow() and related functions (see bug 791214).
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif
double
js::ecmaPow(double x, double y)
{
    /*
     * Use powi if the exponent is an integer-valued double. We don't have to
     * check for NaN since a comparison with NaN is always false.
     */
    if (int32_t(y) == y)
        return powi(x, int32_t(y));

    /*
     * Because C99 and ECMA specify different behavior for pow(),
     * we need to wrap the libm call to make it ECMA compliant.
     */
    if (!IsFinite(y) && (x == 1.0 || x == -1.0))
        return js_NaN;
    /* pow(x, +-0) is always 1, even for x = NaN (MSVC gets this wrong). */
    if (y == 0)
        return 1;
    return pow(x, y);
}
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

// Disable PGO for Math.pow() and related functions (see bug 791214).
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif
JSBool
js_math_pow(JSContext *cx, unsigned argc, Value *vp)
{
    double x, y;

    if (argc <= 1) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x) || !ToNumber(cx, vp[3], &y))
        return JS_FALSE;
    /*
     * Special case for square roots. Note that pow(x, 0.5) != sqrt(x)
     * when x = -0.0, so we have to guard for this.
     */
    if (IsFinite(x) && x != 0.0) {
        if (y == 0.5) {
            vp->setNumber(sqrt(x));
            return JS_TRUE;
        }
        if (y == -0.5) {
            vp->setNumber(1.0/sqrt(x));
            return JS_TRUE;
        }
    }
    /* pow(x, +-0) is always 1, even for x = NaN. */
    if (y == 0) {
        vp->setInt32(1);
        return JS_TRUE;
    }

    double z = ecmaPow(x, y);

    vp->setNumber(z);
    return JS_TRUE;
}
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

static uint64_t
random_generateSeed()
{
    union {
        uint8_t     u8[8];
        uint32_t    u32[2];
        uint64_t    u64;
    } seed;
    seed.u64 = 0;

#if defined(XP_WIN)
    /*
     * Our PRNG only uses 48 bits, so calling rand_s() twice to get 64 bits is
     * probably overkill.
     */
    rand_s(&seed.u32[0]);
#elif defined(XP_UNIX)
    /*
     * In the unlikely event we can't read /dev/urandom, there's not much we can
     * do, so just mix in the fd error code and the current time.
     */
    int fd = open("/dev/urandom", O_RDONLY);
    MOZ_ASSERT(fd >= 0, "Can't open /dev/urandom");
    if (fd >= 0) {
        read(fd, seed.u8, mozilla::ArrayLength(seed.u8));
        close(fd);
    }
    seed.u32[0] ^= fd;
#else
# error "Platform needs to implement random_generateSeed()"
#endif

    seed.u32[1] ^= PRMJ_Now();
    return seed.u64;
}

static const uint64_t RNG_MULTIPLIER = 0x5DEECE66DLL;
static const uint64_t RNG_ADDEND = 0xBLL;
static const uint64_t RNG_MASK = (1LL << 48) - 1;
static const double RNG_DSCALE = double(1LL << 53);

/*
 * Math.random() support, lifted from java.util.Random.java.
 */
static void
random_initState(uint64_t *rngState)
{
    /* Our PRNG only uses 48 bits, so squeeze our entropy into those bits. */
    uint64_t seed = random_generateSeed();
    seed ^= (seed >> 16);
    *rngState = (seed ^ RNG_MULTIPLIER) & RNG_MASK;
}

uint64_t
random_next(uint64_t *rngState, int bits)
{
    MOZ_ASSERT((*rngState & 0xffff000000000000ULL) == 0, "Bad rngState");
    MOZ_ASSERT(bits > 0 && bits <= 48, "bits is out of range");

    if (*rngState == 0) {
        random_initState(rngState);
    }

    uint64_t nextstate = *rngState * RNG_MULTIPLIER;
    nextstate += RNG_ADDEND;
    nextstate &= RNG_MASK;
    *rngState = nextstate;
    return nextstate >> (48 - bits);
}

static inline double
random_nextDouble(JSContext *cx)
{
    uint64_t *rng = &cx->compartment->rngState;
    return double((random_next(rng, 26) << 27) + random_next(rng, 27)) / RNG_DSCALE;
}

double
math_random_no_outparam(JSContext *cx)
{
    /* Calculate random without memory traffic, for use in the JITs. */
    return random_nextDouble(cx);
}

JSBool
js_math_random(JSContext *cx, unsigned argc, Value *vp)
{
    double z = random_nextDouble(cx);
    vp->setDouble(z);
    return JS_TRUE;
}

JSBool /* ES5 15.8.2.15. */
js_math_round(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setDouble(js_NaN);
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    int32_t i;
    if (DoubleIsInt32(x, &i)) {
        args.rval().setInt32(i);
        return true;
    }

    /* Some numbers are so big that adding 0.5 would give the wrong number. */
    if (ExponentComponent(x) >= 52) {
        args.rval().setNumber(x);
        return true;
    }

    args.rval().setNumber(js_copysign(floor(x + 0.5), x));
    return true;
}

double
js::math_sin_impl(MathCache *cache, double x)
{
    return cache->lookup(sin, x);
}

JSBool
js::math_sin(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_sin_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

JSBool
js_math_sqrt(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = mathCache->lookup(sqrt, x);
    vp->setDouble(z);
    return JS_TRUE;
}

double
js::math_tan_impl(MathCache *cache, double x)
{
    return cache->lookup(tan, x);
}

JSBool
js::math_tan(JSContext *cx, unsigned argc, Value *vp)
{
    double x, z;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    if (!ToNumber(cx, vp[2], &x))
        return JS_FALSE;
    MathCache *mathCache = cx->runtime->getMathCache(cx);
    if (!mathCache)
        return JS_FALSE;
    z = math_tan_impl(mathCache, x);
    vp->setDouble(z);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
static JSBool
math_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().Math);
    return JS_TRUE;
}
#endif

static const JSFunctionSpec math_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  math_toSource,        0, 0),
#endif
    JS_FN("abs",            js_math_abs,          1, 0),
    JS_FN("acos",           math_acos,            1, 0),
    JS_FN("asin",           math_asin,            1, 0),
    JS_FN("atan",           math_atan,            1, 0),
    JS_FN("atan2",          math_atan2,           2, 0),
    JS_FN("ceil",           js_math_ceil,         1, 0),
    JS_FN("cos",            math_cos,             1, 0),
    JS_FN("exp",            math_exp,             1, 0),
    JS_FN("floor",          js_math_floor,        1, 0),
    JS_FN("imul",           math_imul,            2, 0),
    JS_FN("log",            math_log,             1, 0),
    JS_FN("max",            js_math_max,          2, 0),
    JS_FN("min",            js_math_min,          2, 0),
    JS_FN("pow",            js_math_pow,          2, 0),
    JS_FN("random",         js_math_random,       0, 0),
    JS_FN("round",          js_math_round,        1, 0),
    JS_FN("sin",            math_sin,             1, 0),
    JS_FN("sqrt",           js_math_sqrt,         1, 0),
    JS_FN("tan",            math_tan,             1, 0),
    JS_FS_END
};

JSObject *
js_InitMathClass(JSContext *cx, HandleObject obj)
{
    RootedObject Math(cx, NewObjectWithClassProto(cx, &MathClass, NULL, obj, SingletonObject));
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

    MarkStandardClassInitializedNoProto(obj, &MathClass);

    return Math;
}
