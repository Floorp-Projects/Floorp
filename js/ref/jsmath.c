/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS math package.
 */
#include "jsstddef.h"
#include <math.h>
#include <stdlib.h>
#include "prtypes.h"
#include "prlong.h"
#include "prtime.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"

#ifndef M_E
#define M_E		2.7182818284590452354
#endif
#ifndef M_LOG2E
#define M_LOG2E		1.4426950408889634074
#endif
#ifndef M_LOG10E
#define M_LOG10E	0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2		0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10		2.30258509299404568402
#endif
#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2	0.70710678118654752440
#endif

static JSConstDoubleSpec math_constants[] = {
    {M_E,       "E"},
    {M_LOG2E,   "LOG2E"},
    {M_LOG10E,  "LOG10E"},
    {M_LN2,     "LN2"},
    {M_LN10,    "LN10"},
    {M_PI,      "PI"},
    {M_SQRT2,   "SQRT2"},
    {M_SQRT1_2, "SQRT1_2"},
    {0}
};

static JSClass math_class = {
    "Math",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSBool
math_abs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = (x < 0) ? -x : x;
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_acos(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = acos(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_asin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = asin(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_atan(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = atan(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_atan2(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, y, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    if (!js_ValueToNumber(cx, argv[1], &y))
	return JS_FALSE;
    z = atan2(x, y);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_ceil(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = ceil(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_cos(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = cos(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_exp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = exp(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_floor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = floor(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = log(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_max(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, y, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    if (!js_ValueToNumber(cx, argv[1], &y))
	return JS_FALSE;
    z = (x > y) ? x : y;
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_min(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, y, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    if (!js_ValueToNumber(cx, argv[1], &y))
	return JS_FALSE;
    z = (x < y) ? x : y;
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_pow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, y, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    if (!js_ValueToNumber(cx, argv[1], &y))
	return JS_FALSE;
    z = pow(x, y);
    return js_NewNumberValue(cx, z, rval);
}

/*
 * Math.random() support, lifted from java.util.Random.java.
 */
static void
random_setSeed(JSRuntime *rt, int64 seed)
{
    int64 tmp;

    LL_I2L(tmp, 1000);
    LL_DIV(seed, seed, tmp);
    LL_XOR(tmp, seed, rt->rngMultiplier);
    LL_AND(rt->rngSeed, tmp, rt->rngMask);
}

static void
random_init(JSRuntime *rt)
{
    int64 tmp, tmp2;

    /* Do at most once. */
    if (rt->rngInitialized)
	return;
    rt->rngInitialized = JS_TRUE;

    /* rt->rngMultiplier = 0x5DEECE66DL */
    LL_ISHL(tmp, 0x5D, 32);
    LL_UI2L(tmp2, 0xEECE66DL);
    LL_OR(rt->rngMultiplier, tmp, tmp2);

    /* rt->rngAddend = 0xBL */
    LL_I2L(rt->rngAddend, 0xBL);

    /* rt->rngMask = (1L << 48) - 1 */
    LL_I2L(tmp, 1);
    LL_SHL(tmp2, tmp, 48);
    LL_SUB(rt->rngMask, tmp2, tmp);

    /* rt->rngDscale = (jsdouble)(1L << 54) */
    LL_SHL(tmp2, tmp, 54);
    LL_L2D(rt->rngDscale, tmp2);

    /* Finally, set the seed from current time. */
    random_setSeed(rt, PR_Now());
}

static uint32
random_next(JSRuntime *rt, int bits)
{
    int64 nextseed, tmp;
    uint32 retval;

    LL_MUL(nextseed, rt->rngSeed, rt->rngMultiplier);
    LL_ADD(nextseed, nextseed, rt->rngAddend);
    LL_AND(nextseed, nextseed, rt->rngMask);
    rt->rngSeed = nextseed;
    LL_USHR(tmp, nextseed, 48 - bits);
    LL_L2I(retval, tmp);
    return retval;
}

static jsdouble
random_nextDouble(JSRuntime *rt)
{
    int64 tmp, tmp2;
    jsdouble d;

    LL_ISHL(tmp, random_next(rt, 27), 27);
    LL_UI2L(tmp2, random_next(rt, 27));
    LL_ADD(tmp, tmp, tmp2);
    LL_L2D(d, tmp);
    return d / rt->rngDscale;
}

static JSBool
math_random(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSRuntime *rt;
    jsdouble z;

    rt = cx->runtime;
    JS_LOCK_RUNTIME(rt);
    random_init(rt);
    z = random_nextDouble(rt);
    JS_UNLOCK_RUNTIME(rt);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_round(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = floor(x + 0.5);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_sin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = sin(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_sqrt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = sqrt(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSBool
math_tan(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x, z;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    z = tan(x);
    return js_NewNumberValue(cx, z, rval);
}

static JSFunctionSpec math_static_methods[] = {
    {"abs",		math_abs,		1},
    {"acos",		math_acos,		1},
    {"asin",		math_asin,		1},
    {"atan",		math_atan,		1},
    {"atan2",		math_atan2,		2},
    {"ceil",		math_ceil,		1},
    {"cos",		math_cos,		1},
    {"exp",		math_exp,		1},
    {"floor",		math_floor,		1},
    {"log",		math_log,		1},
    {"max",		math_max,		2},
    {"min",		math_min,		2},
    {"pow",		math_pow,		2},
    {"random",		math_random,		0},
    {"round",		math_round,		1},
    {"sin",		math_sin,		1},
    {"sqrt",		math_sqrt,		1},
    {"tan",		math_tan,		1},
    {0}
};

#if JS_HAS_TOSOURCE
static JSBool
math_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    *rval = ATOM_KEY(cx->runtime->atomState.MathAtom);
    return JS_TRUE;
}

static JSFunctionSpec math_methods[] = {
    {js_toSource_str,   math_toSource,  0},
    {0}
};
#else
#define math_methods    NULL
#endif

JSObject *
js_InitMathClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &math_class, NULL, 0,
		         NULL, math_methods, NULL, math_static_methods);
    if (!proto)
	return NULL;
    if (!JS_DefineConstDoubles(cx, proto, math_constants))
	return NULL;
    return proto;
}
