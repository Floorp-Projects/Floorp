/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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


static JSValue Math_abs(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    else
        return JSValue(fd::fabs(argv[0].toNumber(cx).f64));
}
static JSValue Math_acos(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::acos(argv[0].toNumber(cx).f64));
}
static JSValue Math_asin(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::asin(argv[0].toNumber(cx).f64));
}
static JSValue Math_atan(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::atan(argv[0].toNumber(cx).f64));
}
static JSValue Math_atan2(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc <= 1)
        return kNaNValue;
    float64 y = argv[0].toNumber(cx).f64;
    float64 x = argv[1].toNumber(cx).f64;
    return JSValue(fd::atan2(y, x));
}
static JSValue Math_ceil(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::ceil(argv[0].toNumber(cx).f64));
}
static JSValue Math_cos(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::cos(argv[0].toNumber(cx).f64));
}
static JSValue Math_exp(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::exp(argv[0].toNumber(cx).f64));
}
static JSValue Math_floor(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    else
        return JSValue(fd::floor(argv[0].toNumber(cx).f64));
}
static JSValue Math_log(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::log(argv[0].toNumber(cx).f64));
}
static JSValue Math_max(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    float64 result = argv[0].toNumber(cx).f64;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = argv[i].toNumber(cx).f64;
        if (arg > result)
            result = arg;
    }
    return JSValue(result);
}
static JSValue Math_min(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    float64 result = argv[0].toNumber(cx).f64;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = argv[i].toNumber(cx).f64;
        if (arg < result)
            result = arg;
    }
    return JSValue(result);
}
static JSValue Math_pow(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc < 1)
        return kNaNValue;
    return JSValue(fd::pow(argv[0].toNumber(cx).f64, argv[1].toNumber(cx).f64));
}
static JSValue Math_random(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue * /*argv*/, uint32 /*argc*/)
{
    return JSValue(42.0);
}
static JSValue Math_round(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    float64 x = argv[0].toNumber(cx).f64;
    return JSValue( fd::copysign( fd::floor(x + 0.5), x ) );
}
static JSValue Math_sin(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::sin(argv[0].toNumber(cx).f64));
}
static JSValue Math_sqrt(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::sqrt(argv[0].toNumber(cx).f64));
}
static JSValue Math_tan(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 argc)   
{
    if (argc == 0)
        return kNaNValue;
    return JSValue(fd::tan(argv[0].toNumber(cx).f64));
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
} MathObjectFunctions[] = {
    { "abs",    Math_abs },
    { "acos",   Math_acos },
    { "asin",   Math_asin },
    { "atan",   Math_atan },
    { "atan2",  Math_atan2 },
    { "ceil",   Math_ceil },
    { "cos",    Math_cos },
    { "exp",    Math_exp },
    { "floor",  Math_floor },
    { "log",    Math_log },
    { "max",    Math_max },
    { "min",    Math_min },
    { "pow",    Math_pow },
    { "random", Math_random },
    { "round",  Math_round },
    { "sin",    Math_sin },
    { "sqrt",   Math_sqrt },
    { "tan",    Math_tan },    
};

void initMathObject(Context *cx, JSObject *mathObj)
{
    uint32 i;
    for (i = 0; i < M_CONSTANTS_COUNT; i++)
        mathObj->defineVariable(cx, widenCString(MathObjectConstants[i].name), 
                                    (NamespaceList *)(NULL), Number_Type, JSValue(MathObjectConstants[i].value));

    for (i = 0; i < sizeof(MathObjectFunctions) / sizeof(MathObjectFunctionDef); i++) {
        JSFunction *f = new JSFunction(cx, MathObjectFunctions[i].imp, Number_Type);
        mathObj->defineVariable(cx, widenCString(MathObjectFunctions[i].name), 
                                    (NamespaceList *)(NULL), Number_Type, JSValue(f));
    }
}    



}
}
