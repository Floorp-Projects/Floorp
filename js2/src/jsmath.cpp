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

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"
#include "jslong.h"
#include "prmjtime.h"

#include "jsmath.h"

#include "fdlibm_ns.h"

namespace JavaScript {    
namespace JS2Runtime {

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
#define M_CONSTANTS_COUNT     8


static js2val Math_abs(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    else
        return JSValue::newNumber(fd::fabs(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_acos(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::acos(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_asin(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::asin(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_atan(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::atan(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_atan2(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc <= 1)
        return kNaNValue;
    float64 y = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    float64 x = JSValue::f64(JSValue::toNumber(cx, argv[1]));
    return JSValue::newNumber(fd::atan2(y, x));
}
static js2val Math_ceil(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::ceil(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_cos(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::cos(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_exp(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::exp(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_floor(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    else
        return JSValue::newNumber(fd::floor(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_log(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::log(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_max(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNegativeInfinity;
    float64 result = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    if (JSDOUBLE_IS_NaN(result)) return kNaNValue;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = JSValue::f64(JSValue::toNumber(cx, argv[i]));
        if (JSDOUBLE_IS_NaN(arg)) return kNaNValue;
        if (arg > result)
            result = arg;
    }
    return JSValue::newNumber(result);
}
static js2val Math_min(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kPositiveInfinity;
    float64 result = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    if (JSDOUBLE_IS_NaN(result)) return kNaNValue;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = JSValue::f64(JSValue::toNumber(cx, argv[i]));
        if (JSDOUBLE_IS_NaN(arg)) return kNaNValue;
        if ((arg < result) || (JSDOUBLE_IS_POSZERO(result) && JSDOUBLE_IS_NEGZERO(arg)))
            result = arg;
    }
    return JSValue::newNumber(result);
}
static js2val Math_pow(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc < 1)
        return kNaNValue;
    return JSValue::newNumber(fd::pow(JSValue::f64(JSValue::toNumber(cx, argv[0])), JSValue::f64(JSValue::toNumber(cx, argv[1]))));
}

/*
 * Math.random() support, lifted from java.util.Random.java.
 */
static void random_setSeed(Context *cx, int64 seed)
{
    int64 tmp;

    JSLL_I2L(tmp, 1000);
    JSLL_DIV(seed, seed, tmp);
    JSLL_XOR(tmp, seed, cx->mWorld.rngMultiplier);
    JSLL_AND(cx->mWorld.rngSeed, tmp, cx->mWorld.rngMask);
}

static void random_init(Context *cx)
{
    int64 tmp, tmp2;

    /* Do at most once. */
    if (cx->mWorld.rngInitialized)
	return;
    cx->mWorld.rngInitialized = true;

    /* cx->mWorld.rngMultiplier = 0x5DEECE66DL */
    JSLL_ISHL(tmp, 0x5D, 32);
    JSLL_UI2L(tmp2, 0xEECE66DL);
    JSLL_OR(cx->mWorld.rngMultiplier, tmp, tmp2);

    /* cx->mWorld.rngAddend = 0xBL */
    JSLL_I2L(cx->mWorld.rngAddend, 0xBL);

    /* cx->mWorld.rngMask = (1L << 48) - 1 */
    JSLL_I2L(tmp, 1);
    JSLL_SHL(tmp2, tmp, 48);
    JSLL_SUB(cx->mWorld.rngMask, tmp2, tmp);

    /* cx->mWorld.rngDscale = (jsdouble)(1L << 54) */
    JSLL_SHL(tmp2, tmp, 54);
    JSLL_L2D(cx->mWorld.rngDscale, tmp2);

    /* Finally, set the seed from current time. */
    random_setSeed(cx, PRMJ_Now());
}

static uint32 random_next(Context *cx, int bits)
{
    int64 nextseed, tmp;
    uint32 retval;

    JSLL_MUL(nextseed, cx->mWorld.rngSeed, cx->mWorld.rngMultiplier);
    JSLL_ADD(nextseed, nextseed, cx->mWorld.rngAddend);
    JSLL_AND(nextseed, nextseed, cx->mWorld.rngMask);
    cx->mWorld.rngSeed = nextseed;
    JSLL_USHR(tmp, nextseed, 48 - bits);
    JSLL_L2I(retval, tmp);
    return retval;
}

static float64 random_nextDouble(Context *cx)
{
    int64 tmp, tmp2;
    float64 d;

    JSLL_ISHL(tmp, random_next(cx, 27), 27);
    JSLL_UI2L(tmp2, random_next(cx, 27));
    JSLL_ADD(tmp, tmp, tmp2);
    JSLL_L2D(d, tmp);
    return d / cx->mWorld.rngDscale;
}


static js2val Math_random(Context *cx, const js2val /*thisValue*/, js2val * /* argv */, uint32 /*argc*/)
{
    random_init(cx);
    return JSValue::newNumber(random_nextDouble(cx));
}
static js2val Math_round(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    float64 x = JSValue::f64(JSValue::toNumber(cx, argv[0]));
    return JSValue::newNumber( fd::copysign( fd::floor(x + 0.5), x ) );
}
static js2val Math_sin(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::sin(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_sqrt(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::sqrt(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}
static js2val Math_tan(Context *cx, const js2val /*thisValue*/, js2val *argv, uint32 argc)   
{
    if (argc == 0)
        return kNaNValue;
    return JSValue::newNumber(fd::tan(JSValue::f64(JSValue::toNumber(cx, argv[0]))));
}


struct {
    char *name;
    float64 value;
} MathObjectConstants[M_CONSTANTS_COUNT] = {
    { "E",      M_E },
    { "LOG2E",  M_LOG2E },
    { "LOG10E", M_LOG10E },
    { "LN2",    M_LN2 },
    { "LN10",   M_LN10 },
    { "PI",     M_PI },
    { "SQRT2",  M_SQRT2 },
    { "SQRT1_2",M_SQRT1_2 }
};

struct MathObjectFunctionDef {
    char *name;
    JSFunction::NativeCode *imp;
    uint32 length;
} MathObjectFunctions[] = {
    { "abs",    Math_abs,    1 },
    { "acos",   Math_acos,   1 },
    { "asin",   Math_asin,   1 },
    { "atan",   Math_atan,   1 },
    { "atan2",  Math_atan2,  2 },
    { "ceil",   Math_ceil,   1 },
    { "cos",    Math_cos,    1 },
    { "exp",    Math_exp,    1 },
    { "floor",  Math_floor,  1 },
    { "log",    Math_log,    1 },
    { "max",    Math_max,    2 },
    { "min",    Math_min,    2 },
    { "pow",    Math_pow,    2 },
    { "random", Math_random, 1 },
    { "round",  Math_round,  1 },
    { "sin",    Math_sin,    1 },
    { "sqrt",   Math_sqrt,   1 },
    { "tan",    Math_tan,    1 },    
};

void initMathObject(Context *cx, JSObject *mathObj)
{
    uint32 i;
    for (i = 0; i < M_CONSTANTS_COUNT; i++)
        mathObj->defineVariable(cx, widenCString(MathObjectConstants[i].name), 
                                    (NamespaceList *)(NULL), Property::ReadOnly | Property::DontDelete, 
                                    Number_Type, JSValue::newNumber(MathObjectConstants[i].value));

    for (i = 0; i < sizeof(MathObjectFunctions) / sizeof(MathObjectFunctionDef); i++) {
        JSFunction *f = new JSFunction(cx, MathObjectFunctions[i].imp, Number_Type, NULL);
        f->setParameterCounts(cx, MathObjectFunctions[i].length, 0, 0, false);
        mathObj->defineVariable(cx, widenCString(MathObjectFunctions[i].name), 
                                    (NamespaceList *)(NULL), Property::ReadOnly | Property::DontDelete, 
                                    Number_Type, JSValue::newFunction(f));
    }
}    



}
}
