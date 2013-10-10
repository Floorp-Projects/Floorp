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
#include "mozilla/MemoryReporting.h"

#include <algorithm>  // for std::max
#include <fcntl.h>

#ifdef XP_UNIX
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jslibmath.h"
#include "jstypes.h"
#include "prmjtime.h"

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
using JS::ToNumber;
using JS::GenericNaN;

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

static const JSConstDoubleSpec math_constants[] = {
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
MathCache::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    return mallocSizeOf(this);
}

const Class js::MathClass = {
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

bool
js_math_abs(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    double z = Abs(x);
    args.rval().setNumber(z);
    return true;
}

#if defined(SOLARIS) && defined(__GNUC__)
#define ACOS_IF_OUT_OF_RANGE(x) if (x < -1 || 1 < x) return GenericNaN();
#else
#define ACOS_IF_OUT_OF_RANGE(x)
#endif

double
js::math_acos_impl(MathCache *cache, double x)
{
    ACOS_IF_OUT_OF_RANGE(x);
    return cache->lookup(acos, x);
}

double
js::math_acos_uncached(double x)
{
    ACOS_IF_OUT_OF_RANGE(x);
    return acos(x);
}

#undef ACOS_IF_OUT_OF_RANGE

bool
js::math_acos(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_acos_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
}

#if defined(SOLARIS) && defined(__GNUC__)
#define ASIN_IF_OUT_OF_RANGE(x) if (x < -1 || 1 < x) return GenericNaN();
#else
#define ASIN_IF_OUT_OF_RANGE(x)
#endif

double
js::math_asin_impl(MathCache *cache, double x)
{
    ASIN_IF_OUT_OF_RANGE(x);
    return cache->lookup(asin, x);
}

double
js::math_asin_uncached(double x)
{
    ASIN_IF_OUT_OF_RANGE(x);
    return asin(x);
}

#undef ASIN_IF_OUT_OF_RANGE

bool
js::math_asin(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_asin_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
}

double
js::math_atan_impl(MathCache *cache, double x)
{
    return cache->lookup(atan, x);
}

double
js::math_atan_uncached(double x)
{
    return atan(x);
}

bool
js::math_atan(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_atan_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
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

bool
js::math_atan2(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() <= 1) {
        args.rval().setNaN();
        return true;
    }

    double x, y;
    if (!ToNumber(cx, args[0], &x) || !ToNumber(cx, args[1], &y))
        return false;

    double z = ecmaAtan2(x, y);
    args.rval().setDouble(z);
    return true;
}

double
js::math_ceil_impl(double x)
{
#ifdef __APPLE__
    if (x < 0 && x > -1.0)
        return js_copysign(0, -1);
#endif
    return ceil(x);
}

bool
js::math_ceil(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    double z = math_ceil_impl(x);
    args.rval().setNumber(z);
    return true;
}

double
js::math_cos_impl(MathCache *cache, double x)
{
    return cache->lookup(cos, x);
}

double
js::math_cos_uncached(double x)
{
    return cos(x);
}

bool
js::math_cos(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_cos_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
}

#ifdef _WIN32
#define EXP_IF_OUT_OF_RANGE(x)                  \
    if (!IsNaN(x)) {                            \
        if (x == PositiveInfinity())            \
            return PositiveInfinity();          \
        if (x == NegativeInfinity())            \
            return 0.0;                         \
    }
#else
#define EXP_IF_OUT_OF_RANGE(x)
#endif

double
js::math_exp_impl(MathCache *cache, double x)
{
    EXP_IF_OUT_OF_RANGE(x);
    return cache->lookup(exp, x);
}

double
js::math_exp_uncached(double x)
{
    EXP_IF_OUT_OF_RANGE(x);
    return exp(x);
}

#undef EXP_IF_OUT_OF_RANGE

bool
js::math_exp(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_exp_impl(mathCache, x);
    args.rval().setNumber(z);
    return true;
}

double
js::math_floor_impl(double x)
{
    return floor(x);
}

bool
js::math_floor(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    double z = math_floor_impl(x);
    args.rval().setNumber(z);
    return true;
}

bool
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

bool
js::math_fround(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    float f = x;
    args.rval().setDouble(static_cast<double>(f));
    return true;
}

#if defined(SOLARIS) && defined(__GNUC__)
#define LOG_IF_OUT_OF_RANGE(x) if (x < 0) return GenericNaN();
#else
#define LOG_IF_OUT_OF_RANGE(x)
#endif

double
js::math_log_impl(MathCache *cache, double x)
{
    LOG_IF_OUT_OF_RANGE(x);
    return cache->lookup(log, x);
}

double
js::math_log_uncached(double x)
{
    LOG_IF_OUT_OF_RANGE(x);
    return log(x);
}

#undef LOG_IF_OUT_OF_RANGE

bool
js::math_log(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_log_impl(mathCache, x);
    args.rval().setNumber(z);
    return true;
}

bool
js_math_max(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    double maxval = NegativeInfinity();
    for (unsigned i = 0; i < args.length(); i++) {
        double x;
        if (!ToNumber(cx, args[i], &x))
            return false;
        // Math.max(num, NaN) => NaN, Math.max(-0, +0) => +0
        if (x > maxval || IsNaN(x) || (x == maxval && IsNegative(maxval)))
            maxval = x;
    }
    args.rval().setNumber(maxval);
    return true;
}

bool
js_math_min(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    double minval = PositiveInfinity();
    for (unsigned i = 0; i < args.length(); i++) {
        double x;
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
        return GenericNaN();
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
bool
js_math_pow(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() <= 1) {
        args.rval().setNaN();
        return true;
    }

    double x, y;
    if (!ToNumber(cx, args[0], &x) || !ToNumber(cx, args[1], &y))
        return false;

    /*
     * Special case for square roots. Note that pow(x, 0.5) != sqrt(x)
     * when x = -0.0, so we have to guard for this.
     */
    if (IsFinite(x) && x != 0.0) {
        if (y == 0.5) {
            args.rval().setNumber(sqrt(x));
            return true;
        }
        if (y == -0.5) {
            args.rval().setNumber(1.0/sqrt(x));
            return true;
        }
    }

    /* pow(x, +-0) is always 1, even for x = NaN. */
    if (y == 0) {
        args.rval().setInt32(1);
        return true;
    }

    double z = ecmaPow(x, y);

    args.rval().setNumber(z);
    return true;
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
    uint64_t *rng = &cx->compartment()->rngState;
    return double((random_next(rng, 26) << 27) + random_next(rng, 27)) / RNG_DSCALE;
}

double
math_random_no_outparam(JSContext *cx)
{
    /* Calculate random without memory traffic, for use in the JITs. */
    return random_nextDouble(cx);
}

bool
js_math_random(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double z = random_nextDouble(cx);
    args.rval().setDouble(z);
    return true;
}

double
js::math_round_impl(double x)
{
    int32_t i;
    if (DoubleIsInt32(x, &i))
        return double(i);

    /* Some numbers are so big that adding 0.5 would give the wrong number. */
    if (ExponentComponent(x) >= 52)
        return x;

    return js_copysign(floor(x + 0.5), x);
}

bool /* ES5 15.8.2.15. */
js::math_round(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    double z = math_round_impl(x);
    args.rval().setNumber(z);
    return true;
}

double
js::math_sin_impl(MathCache *cache, double x)
{
    return cache->lookup(sin, x);
}

double
js::math_sin_uncached(double x)
{
    return sin(x);
}

bool
js::math_sin(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_sin_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
}

bool
js_math_sqrt(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = mathCache->lookup(sqrt, x);
    args.rval().setDouble(z);
    return true;
}

double
js::math_tan_impl(MathCache *cache, double x)
{
    return cache->lookup(tan, x);
}

double
js::math_tan_uncached(double x)
{
    return tan(x);
}

bool
js::math_tan(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNaN();
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;

    double z = math_tan_impl(mathCache, x);
    args.rval().setDouble(z);
    return true;
}


typedef double (*UnaryMathFunctionType)(MathCache *cache, double);

template <UnaryMathFunctionType F>
static bool math_function(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        args.rval().setNumber(GenericNaN());
        return true;
    }

    double x;
    if (!ToNumber(cx, args[0], &x))
        return false;

    MathCache *mathCache = cx->runtime()->getMathCache(cx);
    if (!mathCache)
        return false;
    double z = F(mathCache, x);
    args.rval().setNumber(z);

    return true;
}



double
js::math_log10_impl(MathCache *cache, double x)
{
    return cache->lookup(log10, x);
}

double
js::math_log10_uncached(double x)
{
    return log10(x);
}

bool
js::math_log10(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_log10_impl>(cx, argc, vp);
}

#if !HAVE_LOG2
double log2(double x)
{
    return log(x) / M_LN2;
}
#endif

double
js::math_log2_impl(MathCache *cache, double x)
{
    return cache->lookup(log2, x);
}

double
js::math_log2_uncached(double x)
{
    return log2(x);
}

bool
js::math_log2(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_log2_impl>(cx, argc, vp);
}

#if !HAVE_LOG1P
double log1p(double x)
{
    if (fabs(x) < 1e-4) {
        /*
         * Use Taylor approx. log(1 + x) = x - x^2 / 2 + x^3 / 3 - x^4 / 4 with error x^5 / 5
         * Since |x| < 10^-4, |x|^5 < 10^-20, relative error less than 10^-16
         */
        double z = -(x * x * x * x) / 4 + (x * x * x) / 3 - (x * x) / 2 + x;
        return z;
    } else {
        /* For other large enough values of x use direct computation */
        return log(1.0 + x);
    }
}
#endif

#ifdef __APPLE__
// Ensure that log1p(-0) is -0.
#define LOG1P_IF_OUT_OF_RANGE(x) if (x == 0) return x;
#else
#define LOG1P_IF_OUT_OF_RANGE(x)
#endif

double
js::math_log1p_impl(MathCache *cache, double x)
{
    LOG1P_IF_OUT_OF_RANGE(x);
    return cache->lookup(log1p, x);
}

double
js::math_log1p_uncached(double x)
{
    LOG1P_IF_OUT_OF_RANGE(x);
    return log1p(x);
}

#undef LOG1P_IF_OUT_OF_RANGE

bool
js::math_log1p(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_log1p_impl>(cx, argc, vp);
}

#if !HAVE_EXPM1
double expm1(double x)
{
    /* Special handling for -0 */
    if (x == 0.0)
        return x;

    if (fabs(x) < 1e-5) {
        /*
         * Use Taylor approx. exp(x) - 1 = x + x^2 / 2 + x^3 / 6 with error x^4 / 24
         * Since |x| < 10^-5, |x|^4 < 10^-20, relative error less than 10^-15
         */
        double z = (x * x * x) / 6 + (x * x) / 2 + x;
        return z;
    } else {
        /* For other large enough values of x use direct computation */
        return exp(x) - 1.0;
    }
}
#endif

double
js::math_expm1_impl(MathCache *cache, double x)
{
    return cache->lookup(expm1, x);
}

double
js::math_expm1_uncached(double x)
{
    return expm1(x);
}

bool
js::math_expm1(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_expm1_impl>(cx, argc, vp);
}

#if !HAVE_SQRT1PM1
/* This algorithm computes sqrt(1+x)-1 for small x */
double sqrt1pm1(double x)
{
    if (fabs(x) > 0.75)
        return sqrt(1 + x) - 1;

    return expm1(log1p(x) / 2);
}
#endif


double
js::math_cosh_impl(MathCache *cache, double x)
{
    return cache->lookup(cosh, x);
}

double
js::math_cosh_uncached(double x)
{
    return cosh(x);
}

bool
js::math_cosh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_cosh_impl>(cx, argc, vp);
}

double
js::math_sinh_impl(MathCache *cache, double x)
{
    return cache->lookup(sinh, x);
}

double
js::math_sinh_uncached(double x)
{
    return sinh(x);
}

bool
js::math_sinh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_sinh_impl>(cx, argc, vp);
}

double
js::math_tanh_impl(MathCache *cache, double x)
{
    return cache->lookup(tanh, x);
}

double
js::math_tanh_uncached(double x)
{
    return tanh(x);
}

bool
js::math_tanh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_tanh_impl>(cx, argc, vp);
}

#if !HAVE_ACOSH
double acosh(double x)
{
    const double SQUARE_ROOT_EPSILON = sqrt(std::numeric_limits<double>::epsilon());

    if ((x - 1) >= SQUARE_ROOT_EPSILON) {
        if (x > 1 / SQUARE_ROOT_EPSILON) {
            /*
             * http://functions.wolfram.com/ElementaryFunctions/ArcCosh/06/01/06/01/0001/
             * approximation by laurent series in 1/x at 0+ order from -1 to 0
             */
            return log(x) + M_LN2;
        } else if (x < 1.5) {
            // This is just a rearrangement of the standard form below
            // devised to minimize loss of precision when x ~ 1:
            double y = x - 1;
            return log1p(y + sqrt(y * y + 2 * y));
        } else {
            // http://functions.wolfram.com/ElementaryFunctions/ArcCosh/02/
            return log(x + sqrt(x * x - 1));
        }
    } else {
        // see http://functions.wolfram.com/ElementaryFunctions/ArcCosh/06/01/04/01/0001/
        double y = x - 1;
        // approximation by taylor series in y at 0 up to order 2.
        // If x is less than 1, sqrt(2 * y) is NaN and the result is NaN.
        return sqrt(2 * y) * (1 - y / 12 + 3 * y * y / 160);
    }
}
#endif

double
js::math_acosh_impl(MathCache *cache, double x)
{
    return cache->lookup(acosh, x);
}

double
js::math_acosh_uncached(double x)
{
    return acosh(x);
}

bool
js::math_acosh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_acosh_impl>(cx, argc, vp);
}

#if !HAVE_ASINH
// Bug 899712 - gcc incorrectly rewrites -asinh(-x) to asinh(x) when overriding
// asinh.
static double my_asinh(double x)
{
    const double SQUARE_ROOT_EPSILON = sqrt(std::numeric_limits<double>::epsilon());
    const double FOURTH_ROOT_EPSILON = sqrt(SQUARE_ROOT_EPSILON);

    if (x >= FOURTH_ROOT_EPSILON) {
        if (x > 1 / SQUARE_ROOT_EPSILON)
            // http://functions.wolfram.com/ElementaryFunctions/ArcSinh/06/01/06/01/0001/
            // approximation by laurent series in 1/x at 0+ order from -1 to 1
            return M_LN2 + log(x) + 1 / (4 * x * x);
        else if (x < 0.5)
            return log1p(x + sqrt1pm1(x * x));
        else
            return log(x + sqrt(x * x + 1));
    } else if (x <= -FOURTH_ROOT_EPSILON) {
        return -my_asinh(-x);
    } else {
        // http://functions.wolfram.com/ElementaryFunctions/ArcSinh/06/01/03/01/0001/
        // approximation by taylor series in x at 0 up to order 2
        double result = x;

        if (fabs(x) >= SQUARE_ROOT_EPSILON) {
            double x3 = x * x * x;
            // approximation by taylor series in x at 0 up to order 4
            result -= x3 / 6;
        }

        return result;
    }
}
#endif

double
js::math_asinh_impl(MathCache *cache, double x)
{
#ifdef HAVE_ASINH
    return cache->lookup(asinh, x);
#else
    return cache->lookup(my_asinh, x);
#endif
}

double
js::math_asinh_uncached(double x)
{
#ifdef HAVE_ASINH
    return asinh(x);
#else
    return my_asinh(x);
#endif
}

bool
js::math_asinh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_asinh_impl>(cx, argc, vp);
}

#if !HAVE_ATANH
double atanh(double x)
{
    const double EPSILON = std::numeric_limits<double>::epsilon();
    const double SQUARE_ROOT_EPSILON = sqrt(EPSILON);
    const double FOURTH_ROOT_EPSILON = sqrt(SQUARE_ROOT_EPSILON);

    if (fabs(x) >= FOURTH_ROOT_EPSILON) {
        // http://functions.wolfram.com/ElementaryFunctions/ArcTanh/02/
        if (fabs(x) < 0.5)
            return (log1p(x) - log1p(-x)) / 2;

        return log((1 + x) / (1 - x)) / 2;
    } else {
        // http://functions.wolfram.com/ElementaryFunctions/ArcTanh/06/01/03/01/
        // approximation by taylor series in x at 0 up to order 2
        double result = x;

        if (fabs(x) >= SQUARE_ROOT_EPSILON) {
            double x3 = x * x * x;
            result += x3 / 3;
        }

        return result;
    }
}
#endif

double
js::math_atanh_impl(MathCache *cache, double x)
{
    return cache->lookup(atanh, x);
}

double
js::math_atanh_uncached(double x)
{
    return atanh(x);
}

bool
js::math_atanh(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_atanh_impl>(cx, argc, vp);
}

// Math.hypot is disabled pending the resolution of spec issues (bug 896264).
#if 0
#if !HAVE_HYPOT
double hypot(double x, double y)
{
    if (mozilla::IsInfinite(x) || mozilla::IsInfinite(y))
        return PositiveInfinity();

    if (mozilla::IsNaN(x) || mozilla::IsNaN(y))
        return GenericNaN();

    double xabs = mozilla::Abs(x);
    double yabs = mozilla::Abs(y);

    double min = std::min(xabs, yabs);
    double max = std::max(xabs, yabs);

    if (min == 0) {
        return max;
    } else {
        double u = min / max;
        return max * sqrt(1 + u * u);
    }
}
#endif

double
js::math_hypot_impl(double x, double y)
{
#ifdef XP_WIN
    // On Windows, hypot(NaN, Infinity) is NaN. ES6 requires Infinity.
    if (mozilla::IsInfinite(x) || mozilla::IsInfinite(y))
        return PositiveInfinity();
#endif
    return hypot(x, y);
}

bool
js::math_hypot(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 2) {
        args.rval().setNumber(GenericNaN());
        return true;
    }

    double x, y;
    if (!ToNumber(cx, args[0], &x))
        return false;

    if (!ToNumber(cx, args[1], &y))
        return false;

    if (args.length() == 2) {
        args.rval().setNumber(math_hypot_impl(x, y));
        return true;
    }

    /* args.length() > 2 */
    double z;
    if (!ToNumber(cx, args[2], &z)) {
        return false;
    }

    args.rval().setNumber(math_hypot_impl(math_hypot_impl(x, y), z));
    return true;
}
#endif

#if !HAVE_TRUNC
double trunc(double x)
{
    return x > 0 ? floor(x) : ceil(x);
}
#endif

double
js::math_trunc_impl(MathCache *cache, double x)
{
    return cache->lookup(trunc, x);
}

double
js::math_trunc_uncached(double x)
{
    return trunc(x);
}

bool
js::math_trunc(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_trunc_impl>(cx, argc, vp);
}

static double sign(double x)
{
    if (mozilla::IsNaN(x))
        return GenericNaN();

    return x == 0 ? x : x < 0 ? -1 : 1;
}

double
js::math_sign_impl(MathCache *cache, double x)
{
    return cache->lookup(sign, x);
}

double
js::math_sign_uncached(double x)
{
    return sign(x);
}

bool
js::math_sign(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_sign_impl>(cx, argc, vp);
}

#if !HAVE_CBRT
double cbrt(double x)
{
    if (x > 0) {
        return pow(x, 1.0 / 3.0);
    } else if (x == 0) {
        return x;
    } else {
        return -pow(-x, 1.0 / 3.0);
    }
}
#endif

double
js::math_cbrt_impl(MathCache *cache, double x)
{
    return cache->lookup(cbrt, x);
}

double
js::math_cbrt_uncached(double x)
{
    return cbrt(x);
}

bool
js::math_cbrt(JSContext *cx, unsigned argc, Value *vp)
{
    return math_function<math_cbrt_impl>(cx, argc, vp);
}

#if JS_HAS_TOSOURCE
static bool
math_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Math);
    return true;
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
    JS_FN("ceil",           math_ceil,            1, 0),
    JS_FN("cos",            math_cos,             1, 0),
    JS_FN("exp",            math_exp,             1, 0),
    JS_FN("floor",          math_floor,           1, 0),
    JS_FN("imul",           math_imul,            2, 0),
    JS_FN("fround",         math_fround,          1, 0),
    JS_FN("log",            math_log,             1, 0),
    JS_FN("max",            js_math_max,          2, 0),
    JS_FN("min",            js_math_min,          2, 0),
    JS_FN("pow",            js_math_pow,          2, 0),
    JS_FN("random",         js_math_random,       0, 0),
    JS_FN("round",          math_round,           1, 0),
    JS_FN("sin",            math_sin,             1, 0),
    JS_FN("sqrt",           js_math_sqrt,         1, 0),
    JS_FN("tan",            math_tan,             1, 0),
    JS_FN("log10",          math_log10,           1, 0),
    JS_FN("log2",           math_log2,            1, 0),
    JS_FN("log1p",          math_log1p,           1, 0),
    JS_FN("expm1",          math_expm1,           1, 0),
    JS_FN("cosh",           math_cosh,            1, 0),
    JS_FN("sinh",           math_sinh,            1, 0),
    JS_FN("tanh",           math_tanh,            1, 0),
    JS_FN("acosh",          math_acosh,           1, 0),
    JS_FN("asinh",          math_asinh,           1, 0),
    JS_FN("atanh",          math_atanh,           1, 0),
// Math.hypot is disabled pending the resolution of spec issues (bug 896264).
#if 0
    JS_FN("hypot",          math_hypot,           2, 0),
#endif
    JS_FN("trunc",          math_trunc,           1, 0),
    JS_FN("sign",           math_sign,            1, 0),
    JS_FN("cbrt",           math_cbrt,            1, 0),
    JS_FS_END
};

JSObject *
js_InitMathClass(JSContext *cx, HandleObject obj)
{
    RootedObject Math(cx, NewObjectWithClassProto(cx, &MathClass, nullptr, obj, SingletonObject));
    if (!Math)
        return nullptr;

    if (!JS_DefineProperty(cx, obj, js_Math_str, OBJECT_TO_JSVAL(Math),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return nullptr;
    }

    if (!JS_DefineFunctions(cx, Math, math_static_methods))
        return nullptr;
    if (!JS_DefineConstDoubles(cx, Math, math_constants))
        return nullptr;

    MarkStandardClassInitializedNoProto(obj, &MathClass);

    return Math;
}
