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

#include <math.h>
#include "jsmath.h"

namespace JavaScript {
namespace JSMathClass {

using namespace JSTypes;

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


/*
    Concept copied from SpiderMonkey -
    fd_XXX needs to be defined either as a call to the fdlibm routine
    or the native C library routine depending on the platform
*/

#define JS_USE_FDLIBM_MATH 0


#if !JS_USE_FDLIBM_MATH
/*
 * Use system provided math routines.
 */


#define fd_acos acos
#define fd_asin asin
#define fd_atan atan
#define fd_atan2 atan2
#define fd_ceil ceil
#define fd_copysign copysign
#define fd_cos cos
#define fd_exp exp
#define fd_fabs fabs
#define fd_floor floor
#define fd_fmod fmod
#define fd_log log
#define fd_pow pow
#define fd_sin sin
#define fd_sqrt sqrt
#define fd_tan tan

#endif

static JSValue math_abs(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue num = argv[1].toNumber();
        if (num.isNaN()) return num;
        if (num.isNegativeZero()) return kPositiveZero;
        if (num.isNegativeInfinity()) return kPositiveInfinity;
        if (num.f64 < 0) return JSValue(-num.f64);
        return num;
    }
    return kUndefinedValue;
}

static JSValue math_acos(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue num = argv[1].toNumber();
        return JSValue(fd_acos(num.f64));
    }
    return kUndefinedValue;
}

static JSValue math_asin(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue num = argv[1].toNumber();
        return JSValue(fd_asin(num.f64));
    }
    return kUndefinedValue;
}

static JSValue math_atan(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue num = argv[1].toNumber();
        return JSValue(fd_atan(num.f64));
    }
    return kUndefinedValue;
}

static JSValue math_atan2(Context *, const JSValues& argv)
{
    if (argv.size() > 1) {
        JSValue num1 = argv[1].toNumber();
        JSValue num2 = argv[1].toNumber();
        return JSValue(fd_atan2(num1.f64, num2.f64));
    }
    return kUndefinedValue;
}

static JSValue math_ceil(Context *, const JSValues& argv)
{
    if (argv.size() > 0) {
        JSValue num = argv[1].toNumber();
        return JSValue(fd_ceil(num.f64));
    }
    return kUndefinedValue;
}

struct MathFunctionEntry {
    char *name;
    JSNativeFunction::JSCode fn;
} MathFunctions[] = {
    { "abs",     math_abs },
    { "acos",    math_acos },
    { "asin",    math_asin },
    { "atan",    math_atan },
    { "atan2",   math_atan2 },
    { "ceil",    math_ceil },
    { "acos",    math_acos },
    { "acos",    math_acos }
};

struct MathConstantEntry {
    char *name;
    double value;
} MathConstants[] = {
    { "E",           M_E },
    { "LOG2E",       M_LOG2E },
    { "LOG10E",      M_LOG10E },
    { "LN2",         M_LN2 },
    { "LN10",        M_LN10 },
    { "PI",          M_PI },
    { "SQRT2",       M_SQRT2 },
    { "SQRT1_2",     M_SQRT1_2 }
};

// There is no constructor for Math, we simply initialize
//  the properties of the Math object
void JSMath::initMathObject(JSScope *g)
{
    uint i;
    JSMath *m = new JSMath();
    m->setClass(new JSString("Math"));

    for (i = 0; i < sizeof(MathFunctions) / sizeof(MathFunctionEntry); i++)
        m->setProperty(widenCString(MathFunctions[i].name), JSValue(new JSNativeFunction(MathFunctions[i].fn) ) );

    for (i = 0; i < sizeof(MathConstants) / sizeof(MathConstantEntry); i++)
        m->setProperty(widenCString(MathConstants[i].name), JSValue(MathConstants[i].value) );

    g->setProperty(widenCString("Math"), JSValue(m));
    
}

} /* JSMathClass */
} /* JavaScript */
