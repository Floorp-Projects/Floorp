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
#include <list>
#include <map>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "jslong.h"
#include "prmjtime.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

#include "fdlibm_ns.h"

namespace JavaScript {    
namespace MetaData {

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


static js2val Math_abs(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    else
        return meta->engine->allocNumber(fd::fabs(meta->engine->toFloat64(argv[0])));
}
static js2val Math_acos(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::acos(meta->engine->toFloat64(argv[0])));
}
static js2val Math_asin(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::asin(meta->engine->toFloat64(argv[0])));
}
static js2val Math_atan(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::atan(meta->engine->toFloat64(argv[0])));
}
static js2val Math_atan2(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc <= 1)
        return meta->engine->nanValue;
    float64 y = meta->engine->toFloat64(argv[0]);
    float64 x = meta->engine->toFloat64(argv[1]);
    return meta->engine->allocNumber(fd::atan2(y, x));
}
static js2val Math_ceil(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::ceil(meta->engine->toFloat64(argv[0])));
}
static js2val Math_cos(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::cos(meta->engine->toFloat64(argv[0])));
}
static js2val Math_exp(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::exp(meta->engine->toFloat64(argv[0])));
}
static js2val Math_floor(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    else
        return meta->engine->allocNumber(fd::floor(meta->engine->toFloat64(argv[0])));
}
static js2val Math_log(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::log(meta->engine->toFloat64(argv[0])));
}
static js2val Math_max(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->negInfValue;
    float64 result = meta->engine->toFloat64(argv[0]);
    if (JSDOUBLE_IS_NaN(result)) return meta->engine->nanValue;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = meta->engine->toFloat64(argv[i]);
        if (JSDOUBLE_IS_NaN(arg)) return meta->engine->nanValue;
        if (arg > result)
            result = arg;
    }
    return meta->engine->allocNumber(result);
}
static js2val Math_min(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->posInfValue;
    float64 result = meta->engine->toFloat64(argv[0]);
    if (JSDOUBLE_IS_NaN(result)) return meta->engine->nanValue;
    for (uint32 i = 1; i < argc; ++i) {
        float64 arg = meta->engine->toFloat64(argv[i]);
        if (JSDOUBLE_IS_NaN(arg)) return meta->engine->nanValue;
        if ((arg < result) || (JSDOUBLE_IS_POSZERO(result) && JSDOUBLE_IS_NEGZERO(arg)))
            result = arg;
    }
    return meta->engine->allocNumber(result);
}
static js2val Math_pow(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc < 1)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::pow(meta->engine->toFloat64(argv[0]), meta->engine->toFloat64(argv[1])));
}

/*
 * Math.random() support, lifted from java.util.Random.java.
 */
static void random_setSeed(JS2Metadata *meta, int64 seed)
{
    int64 tmp;

    JSLL_I2L(tmp, 1000);
    JSLL_DIV(seed, seed, tmp);
    JSLL_XOR(tmp, seed, meta->rngMultiplier);
    JSLL_AND(meta->rngSeed, tmp, meta->rngMask);
}

static void random_init(JS2Metadata *meta)
{
    int64 tmp, tmp2;

    /* Do at most once. */
    if (meta->rngInitialized)
	return;
    meta->rngInitialized = true;

    /* meta->rngMultiplier = 0x5DEECE66DL */
    JSLL_ISHL(tmp, 0x5D, 32);
    JSLL_UI2L(tmp2, 0xEECE66DL);
    JSLL_OR(meta->rngMultiplier, tmp, tmp2);

    /* meta->rngAddend = 0xBL */
    JSLL_I2L(meta->rngAddend, 0xBL);

    /* meta->rngMask = (1L << 48) - 1 */
    JSLL_I2L(tmp, 1);
    JSLL_SHL(tmp2, tmp, 48);
    JSLL_SUB(meta->rngMask, tmp2, tmp);

    /* meta->rngDscale = (jsdouble)(1L << 54) */
    JSLL_SHL(tmp2, tmp, 54);
    JSLL_L2D(meta->rngDscale, tmp2);

    /* Finally, set the seed from current time. */
    random_setSeed(meta, PRMJ_Now());
}

static uint32 random_next(JS2Metadata *meta, int bits)
{
    int64 nextseed, tmp;
    uint32 retval;

    JSLL_MUL(nextseed, meta->rngSeed, meta->rngMultiplier);
    JSLL_ADD(nextseed, nextseed, meta->rngAddend);
    JSLL_AND(nextseed, nextseed, meta->rngMask);
    meta->rngSeed = nextseed;
    JSLL_USHR(tmp, nextseed, 48 - bits);
    JSLL_L2I(retval, tmp);
    return retval;
}

static float64 random_nextDouble(JS2Metadata *meta)
{
    int64 tmp, tmp2;
    float64 d;

    JSLL_ISHL(tmp, random_next(meta, 27), 27);
    JSLL_UI2L(tmp2, random_next(meta, 27));
    JSLL_ADD(tmp, tmp, tmp2);
    JSLL_L2D(d, tmp);
    return d / meta->rngDscale;
}


static js2val Math_random(JS2Metadata *meta, const js2val /*thisValue*/, js2val * /* argv */, uint32 /*argc*/)
{
    random_init(meta);
    return meta->engine->allocNumber(random_nextDouble(meta));
}
static js2val Math_round(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    float64 x = meta->engine->toFloat64(argv[0]);
    return meta->engine->allocNumber( fd::copysign( fd::floor(x + 0.5), x ) );
}
static js2val Math_sin(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::sin(meta->engine->toFloat64(argv[0])));
}
static js2val Math_sqrt(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::sqrt(meta->engine->toFloat64(argv[0])));
}
static js2val Math_tan(JS2Metadata *meta, const js2val /*thisValue*/, js2val *argv, uint32 argc)   
{
    if (argc == 0)
        return meta->engine->nanValue;
    return meta->engine->allocNumber(fd::tan(meta->engine->toFloat64(argv[0])));
}


void initMathObject(JS2Metadata *meta)
{
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

    NamespaceList publicNamespaceList;
    publicNamespaceList.push_back(meta->publicNamespace);
    
    uint32 i;
    meta->env.addFrame(meta->mathClass);
    for (i = 0; i < M_CONSTANTS_COUNT; i++)
    {
        Variable *v = new Variable(meta->numberClass, meta->engine->allocNumber(MathObjectConstants[i].value), true);
        meta->defineStaticMember(&meta->env, &meta->world.identifiers[MathObjectConstants[i].name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
    }
    meta->env.removeTopFrame();

    typedef struct {
        char *name;
        uint16 length;
        NativeCode *code;
    } PrototypeFunction;

    PrototypeFunction prototypeFunctions[] =
    {
        { "abs",    1,    Math_abs },
        { "acos",   1,    Math_acos },
        { "asin",   1,    Math_asin },
        { "atan",   1,    Math_atan },
        { "atan2",  2,    Math_atan2 },
        { "ceil",   1,    Math_ceil },
        { "cos",    1,    Math_cos },
        { "exp",    1,    Math_exp },
        { "floor",  1,    Math_floor },
        { "log",    1,    Math_log },
        { "max",    2,    Math_max },
        { "min",    2,    Math_min },
        { "pow",    2,    Math_pow },
        { "random", 1,    Math_random },
        { "round",  1,    Math_round },
        { "sin",    1,    Math_sin },
        { "sqrt",   1,    Math_sqrt },
        { "tan",    1,    Math_tan },
        { NULL },
    };

    meta->env.addFrame(meta->mathClass);
    PrototypeFunction *pf = &prototypeFunctions[0];
    while (pf->name) {
        FixedInstance *fInst = new FixedInstance(meta->functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code);
        Variable *v = new Variable(meta->functionClass, OBJECT_TO_JS2VAL(fInst), true);
        meta->defineStaticMember(&meta->env, &meta->world.identifiers[pf->name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        pf++;
    }
    meta->env.removeTopFrame();




}    



}
}
