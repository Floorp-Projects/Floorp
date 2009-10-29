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
 *   IBM Corp.
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

#define __STDC_LIMIT_MACROS

/*
 * JS number type and wrapper class.
 */
#ifdef XP_OS2
#define _PC_53  PC_53
#define _MCW_EM MCW_EM
#define _MCW_PC MCW_PC
#endif
#include <locale.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdtoa.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jsstrinlines.h"
#include "jsvector.h"


#ifndef JS_HAVE_STDINT_H /* Native support is innocent until proven guilty. */

JS_STATIC_ASSERT(uint8_t(-1) == UINT8_MAX);
JS_STATIC_ASSERT(uint16_t(-1) == UINT16_MAX);
JS_STATIC_ASSERT(uint32_t(-1) == UINT32_MAX);
JS_STATIC_ASSERT(uint64_t(-1) == UINT64_MAX);

JS_STATIC_ASSERT(INT8_MAX > INT8_MIN);
JS_STATIC_ASSERT(uint8_t(INT8_MAX) + uint8_t(1) == uint8_t(INT8_MIN));
JS_STATIC_ASSERT(INT16_MAX > INT16_MIN);
JS_STATIC_ASSERT(uint16_t(INT16_MAX) + uint16_t(1) == uint16_t(INT16_MIN));
JS_STATIC_ASSERT(INT32_MAX > INT32_MIN);
JS_STATIC_ASSERT(uint32_t(INT32_MAX) + uint32_t(1) == uint32_t(INT32_MIN));
JS_STATIC_ASSERT(INT64_MAX > INT64_MIN);
JS_STATIC_ASSERT(uint64_t(INT64_MAX) + uint64_t(1) == uint64_t(INT64_MIN));

JS_STATIC_ASSERT(INTPTR_MAX > INTPTR_MIN);
JS_STATIC_ASSERT(uintptr_t(INTPTR_MAX) + uintptr_t(1) == uintptr_t(INTPTR_MIN));
JS_STATIC_ASSERT(uintptr_t(-1) == UINTPTR_MAX);
JS_STATIC_ASSERT(size_t(-1) == SIZE_MAX);
JS_STATIC_ASSERT(PTRDIFF_MAX > PTRDIFF_MIN);
JS_STATIC_ASSERT(ptrdiff_t(PTRDIFF_MAX) == PTRDIFF_MAX);
JS_STATIC_ASSERT(ptrdiff_t(PTRDIFF_MIN) == PTRDIFF_MIN);
JS_STATIC_ASSERT(uintptr_t(PTRDIFF_MAX) + uintptr_t(1) == uintptr_t(PTRDIFF_MIN));

#endif /* JS_HAVE_STDINT_H */

static JSBool
num_isNaN(JSContext *cx, uintN argc, jsval *vp)
{
    jsdouble x;

    if (argc == 0) {
        *vp = JSVAL_TRUE;
        return JS_TRUE;
    }
    x = js_ValueToNumber(cx, &vp[2]);
    if (JSVAL_IS_NULL(vp[2]))
        return JS_FALSE;
    *vp = BOOLEAN_TO_JSVAL(JSDOUBLE_IS_NaN(x));
    return JS_TRUE;
}

static JSBool
num_isFinite(JSContext *cx, uintN argc, jsval *vp)
{
    jsdouble x;

    if (argc == 0) {
        *vp = JSVAL_FALSE;
        return JS_TRUE;
    }
    x = js_ValueToNumber(cx, &vp[2]);
    if (JSVAL_IS_NULL(vp[2]))
        return JS_FALSE;
    *vp = BOOLEAN_TO_JSVAL(JSDOUBLE_IS_FINITE(x));
    return JS_TRUE;
}

static JSBool
num_parseFloat(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    jsdouble d;
    const jschar *bp, *end, *ep;

    if (argc == 0) {
        *vp = cx->runtime->NaNValue;
        return JS_TRUE;
    }
    str = js_ValueToString(cx, vp[2]);
    if (!str)
        return JS_FALSE;
    str->getCharsAndEnd(bp, end);
    if (!js_strtod(cx, bp, end, &ep, &d))
        return JS_FALSE;
    if (ep == bp) {
        *vp = cx->runtime->NaNValue;
        return JS_TRUE;
    }
    return js_NewNumberInRootedValue(cx, d, vp);
}

#ifdef JS_TRACER
static jsdouble FASTCALL
ParseFloat(JSContext* cx, JSString* str)
{
    const jschar* bp;
    const jschar* end;
    const jschar* ep;
    jsdouble d;

    str->getCharsAndEnd(bp, end);
    if (!js_strtod(cx, bp, end, &ep, &d) || ep == bp)
        return js_NaN;
    return d;
}
#endif

/* See ECMA 15.1.2.2. */
static JSBool
num_parseInt(JSContext *cx, uintN argc, jsval *vp)
{
    jsint radix;
    JSString *str;
    jsdouble d;
    const jschar *bp, *end, *ep;

    if (argc == 0) {
        *vp = cx->runtime->NaNValue;
        return JS_TRUE;
    }
    if (argc > 1) {
        radix = js_ValueToECMAInt32(cx, &vp[3]);
        if (JSVAL_IS_NULL(vp[3]))
            return JS_FALSE;
    } else {
        radix = 0;
    }
    if (radix != 0 && (radix < 2 || radix > 36)) {
        *vp = cx->runtime->NaNValue;
        return JS_TRUE;
    }

    if (JSVAL_IS_INT(vp[2]) && (radix == 0 || radix == 10)) {
        *vp = vp[2];
        return JS_TRUE;
    }

    str = js_ValueToString(cx, vp[2]);
    if (!str)
        return JS_FALSE;
    str->getCharsAndEnd(bp, end);
    if (!js_strtointeger(cx, bp, end, &ep, radix, &d))
        return JS_FALSE;
    if (ep == bp) {
        *vp = cx->runtime->NaNValue;
        return JS_TRUE;
    }
    return js_NewNumberInRootedValue(cx, d, vp);
}

#ifdef JS_TRACER
static jsdouble FASTCALL
ParseInt(JSContext* cx, JSString* str)
{
    const jschar* bp;
    const jschar* end;
    const jschar* ep;
    jsdouble d;

    str->getCharsAndEnd(bp, end);
    if (!js_strtointeger(cx, bp, end, &ep, 0, &d) || ep == bp)
        return js_NaN;
    return d;
}

static jsdouble FASTCALL
ParseIntDouble(jsdouble d)
{
    if (!JSDOUBLE_IS_FINITE(d))
        return js_NaN;
    if (d > 0)
        return floor(d);
    if (d < 0)
    	return -floor(-d);
    return 0;
}
#endif

const char js_Infinity_str[]   = "Infinity";
const char js_NaN_str[]        = "NaN";
const char js_isNaN_str[]      = "isNaN";
const char js_isFinite_str[]   = "isFinite";
const char js_parseFloat_str[] = "parseFloat";
const char js_parseInt_str[]   = "parseInt";

#ifdef JS_TRACER

JS_DEFINE_TRCINFO_2(num_parseInt,
    (2, (static, DOUBLE, ParseInt, CONTEXT, STRING,     1, 1)),
    (1, (static, DOUBLE, ParseIntDouble, DOUBLE,        1, 1)))

JS_DEFINE_TRCINFO_1(num_parseFloat,
    (2, (static, DOUBLE, ParseFloat, CONTEXT, STRING,   1, 1)))

#endif /* JS_TRACER */

static JSFunctionSpec number_functions[] = {
    JS_FN(js_isNaN_str,         num_isNaN,           1,0),
    JS_FN(js_isFinite_str,      num_isFinite,        1,0),
    JS_TN(js_parseFloat_str,    num_parseFloat,      1,0, &num_parseFloat_trcinfo),
    JS_TN(js_parseInt_str,      num_parseInt,        2,0, &num_parseInt_trcinfo),
    JS_FS_END
};

JSClass js_NumberClass = {
    js_Number_str,
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_HAS_CACHED_PROTO(JSProto_Number),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
Number(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    jsdouble d;

    if (argc != 0) {
        d = js_ValueToNumber(cx, &argv[0]);
        v = argv[0];
        if (JSVAL_IS_NULL(v))
            return JS_FALSE;
        if (v != JSVAL_TRUE) {
            JS_ASSERT(JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v));
        } else {
            if (!js_NewNumberInRootedValue(cx, d, &argv[0]))
                return JS_FALSE;
            v = argv[0];
        }
    } else {
        v = JSVAL_ZERO;
    }
    if (!JS_IsConstructing(cx))
        *rval = v;
    else
        obj->fslots[JSSLOT_PRIMITIVE_THIS] = v;
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
num_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    jsdouble d;
    char numBuf[DTOSTR_STANDARD_BUFFER_SIZE], *numStr;
    char buf[64];
    JSString *str;

    if (!js_GetPrimitiveThis(cx, vp, &js_NumberClass, &v))
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_NUMBER(v));
    d = JSVAL_IS_INT(v) ? (jsdouble)JSVAL_TO_INT(v) : *JSVAL_TO_DOUBLE(v);
    numStr = JS_dtostr(numBuf, sizeof numBuf, DTOSTR_STANDARD, 0, d);
    if (!numStr) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    JS_snprintf(buf, sizeof buf, "(new %s(%s))", js_NumberClass.name, numStr);
    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif

/* The buf must be big enough for MIN_INT to fit including '-' and '\0'. */
static char *
IntToCString(jsint i, jsint base, char *buf, size_t bufSize)
{
    char *cp;
    jsuint u;

    u = (i < 0) ? -i : i;

    cp = buf + bufSize; /* one past last buffer cell */
    *--cp = '\0';       /* null terminate the string to be */

    /*
     * Build the string from behind. We use multiply and subtraction
     * instead of modulus because that's much faster.
     */
    switch (base) {
    case 10:
      do {
          jsuint newu = u / 10;
          *--cp = (char)(u - newu * 10) + '0';
          u = newu;
      } while (u != 0);
      break;
    case 16:
      do {
          jsuint newu = u / 16;
          *--cp = "0123456789abcdef"[u - newu * 16];
          u = newu;
      } while (u != 0);
      break;
    default:
      JS_ASSERT(base >= 2 && base <= 36);
      do {
          jsuint newu = u / base;
          *--cp = "0123456789abcdefghijklmnopqrstuvwxyz"[u - newu * base];
          u = newu;
      } while (u != 0);
      break;
    }
    if (i < 0)
        *--cp = '-';

    JS_ASSERT(cp >= buf);
    return cp;
}

static JSString * JS_FASTCALL
js_NumberToStringWithBase(JSContext *cx, jsdouble d, jsint base);

static JSBool
num_toString(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    jsdouble d;
    jsint base;
    JSString *str;

    if (!js_GetPrimitiveThis(cx, vp, &js_NumberClass, &v))
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_NUMBER(v));
    d = JSVAL_IS_INT(v) ? (jsdouble)JSVAL_TO_INT(v) : *JSVAL_TO_DOUBLE(v);
    base = 10;
    if (argc != 0 && !JSVAL_IS_VOID(vp[2])) {
        base = js_ValueToECMAInt32(cx, &vp[2]);
        if (JSVAL_IS_NULL(vp[2]))
            return JS_FALSE;
        if (base < 2 || base > 36) {
            char numBuf[12];
            char *numStr = IntToCString(base, 10, numBuf, sizeof numBuf);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_RADIX,
                                 numStr);
            return JS_FALSE;
        }
    }
    str = js_NumberToStringWithBase(cx, d, base);
    if (!str) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
num_toLocaleString(JSContext *cx, uintN argc, jsval *vp)
{
    char thousandsLength, decimalLength;
    const char *numGrouping, *tmpGroup;
    JSRuntime *rt;
    JSString *numStr, *str;
    const char *num, *end, *tmpSrc;
    char *buf, *tmpDest;
    const char *nint;
    int digits, size, remainder, nrepeat;

    /*
     * Create the string, move back to bytes to make string twiddling
     * a bit easier and so we can insert platform charset seperators.
     */
    if (!num_toString(cx, 0, vp))
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_STRING(*vp));
    numStr = JSVAL_TO_STRING(*vp);
    num = js_GetStringBytes(cx, numStr);
    if (!num)
        return JS_FALSE;

    /*
     * Find the first non-integer value, whether it be a letter as in
     * 'Infinity', a decimal point, or an 'e' from exponential notation.
     */
    nint = num;
    if (*nint == '-')
        nint++;
    while (*nint >= '0' && *nint <= '9')
        nint++;
    digits = nint - num;
    end = num + digits;
    if (!digits)
        return JS_TRUE;

    rt = cx->runtime;
    thousandsLength = strlen(rt->thousandsSeparator);
    decimalLength = strlen(rt->decimalSeparator);

    /* Figure out how long resulting string will be. */
    size = digits + (*nint ? strlen(nint + 1) + 1 : 0);
    if (*nint == '.')
        size += decimalLength;

    numGrouping = tmpGroup = rt->numGrouping;
    remainder = digits;
    if (*num == '-')
        remainder--;

    while (*tmpGroup != CHAR_MAX && *tmpGroup != '\0') {
        if (*tmpGroup >= remainder)
            break;
        size += thousandsLength;
        remainder -= *tmpGroup;
        tmpGroup++;
    }
    if (*tmpGroup == '\0' && *numGrouping != '\0') {
        nrepeat = (remainder - 1) / tmpGroup[-1];
        size += thousandsLength * nrepeat;
        remainder -= nrepeat * tmpGroup[-1];
    } else {
        nrepeat = 0;
    }
    tmpGroup--;

    buf = (char *)cx->malloc(size + 1);
    if (!buf)
        return JS_FALSE;

    tmpDest = buf;
    tmpSrc = num;

    while (*tmpSrc == '-' || remainder--)
        *tmpDest++ = *tmpSrc++;
    while (tmpSrc < end) {
        strcpy(tmpDest, rt->thousandsSeparator);
        tmpDest += thousandsLength;
        memcpy(tmpDest, tmpSrc, *tmpGroup);
        tmpDest += *tmpGroup;
        tmpSrc += *tmpGroup;
        if (--nrepeat < 0)
            tmpGroup--;
    }

    if (*nint == '.') {
        strcpy(tmpDest, rt->decimalSeparator);
        tmpDest += decimalLength;
        strcpy(tmpDest, nint + 1);
    } else {
        strcpy(tmpDest, nint);
    }

    if (cx->localeCallbacks && cx->localeCallbacks->localeToUnicode)
        return cx->localeCallbacks->localeToUnicode(cx, buf, vp);

    str = JS_NewString(cx, buf, size);
    if (!str) {
        cx->free(buf);
        return JS_FALSE;
    }

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
num_valueOf(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    JSObject *obj;

    v = vp[1];
    if (JSVAL_IS_NUMBER(v)) {
        *vp = v;
        return JS_TRUE;
    }
    obj = JS_THIS_OBJECT(cx, vp);
    if (!JS_InstanceOf(cx, obj, &js_NumberClass, vp + 2))
        return JS_FALSE;
    *vp = obj->fslots[JSSLOT_PRIMITIVE_THIS];
    return JS_TRUE;
}


#define MAX_PRECISION 100

static JSBool
num_to(JSContext *cx, JSDToStrMode zeroArgMode, JSDToStrMode oneArgMode,
       jsint precisionMin, jsint precisionMax, jsint precisionOffset,
       uintN argc, jsval *vp)
{
    jsval v;
    jsdouble d, precision;
    JSString *str;

    /* Use MAX_PRECISION+1 because precisionOffset can be 1. */
    char buf[DTOSTR_VARIABLE_BUFFER_SIZE(MAX_PRECISION+1)];
    char *numStr;

    if (!js_GetPrimitiveThis(cx, vp, &js_NumberClass, &v))
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_NUMBER(v));
    d = JSVAL_IS_INT(v) ? (jsdouble)JSVAL_TO_INT(v) : *JSVAL_TO_DOUBLE(v);

    if (argc == 0) {
        precision = 0.0;
        oneArgMode = zeroArgMode;
    } else {
        precision = js_ValueToNumber(cx, &vp[2]);
        if (JSVAL_IS_NULL(vp[2]))
            return JS_FALSE;
        precision = js_DoubleToInteger(precision);
        if (precision < precisionMin || precision > precisionMax) {
            numStr = JS_dtostr(buf, sizeof buf, DTOSTR_STANDARD, 0, precision);
            if (!numStr)
                JS_ReportOutOfMemory(cx);
            else
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PRECISION_RANGE, numStr);
            return JS_FALSE;
        }
    }

    numStr = JS_dtostr(buf, sizeof buf, oneArgMode, (jsint)precision + precisionOffset, d);
    if (!numStr) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    str = JS_NewStringCopyZ(cx, numStr);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/*
 * In the following three implementations, we allow a larger range of precision
 * than ECMA requires; this is permitted by ECMA-262.
 */
static JSBool
num_toFixed(JSContext *cx, uintN argc, jsval *vp)
{
    return num_to(cx, DTOSTR_FIXED, DTOSTR_FIXED, -20, MAX_PRECISION, 0,
                  argc, vp);
}

static JSBool
num_toExponential(JSContext *cx, uintN argc, jsval *vp)
{
    return num_to(cx, DTOSTR_STANDARD_EXPONENTIAL, DTOSTR_EXPONENTIAL, 0,
                  MAX_PRECISION, 1, argc, vp);
}

static JSBool
num_toPrecision(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0 || JSVAL_IS_VOID(vp[2]))
        return num_toString(cx, 0, vp);
    return num_to(cx, DTOSTR_STANDARD, DTOSTR_PRECISION, 1, MAX_PRECISION, 0,
                  argc, vp);
}

#ifdef JS_TRACER

JS_DEFINE_TRCINFO_2(num_toString,
    (2, (extern, STRING_RETRY, js_NumberToString,         CONTEXT, THIS_DOUBLE,        1, 1)),
    (3, (static, STRING_RETRY, js_NumberToStringWithBase, CONTEXT, THIS_DOUBLE, INT32, 1, 1)))

#endif /* JS_TRACER */

static JSFunctionSpec number_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,       num_toSource,          0,JSFUN_THISP_NUMBER),
#endif
    JS_TN(js_toString_str,       num_toString,          1,JSFUN_THISP_NUMBER, &num_toString_trcinfo),
    JS_FN(js_toLocaleString_str, num_toLocaleString,    0,JSFUN_THISP_NUMBER),
    JS_FN(js_valueOf_str,        num_valueOf,           0,JSFUN_THISP_NUMBER),
    JS_FN(js_toJSON_str,         num_valueOf,           0,JSFUN_THISP_NUMBER),
    JS_FN("toFixed",             num_toFixed,           1,JSFUN_THISP_NUMBER),
    JS_FN("toExponential",       num_toExponential,     1,JSFUN_THISP_NUMBER),
    JS_FN("toPrecision",         num_toPrecision,       1,JSFUN_THISP_NUMBER),
    JS_FS_END
};

/* NB: Keep this in synch with number_constants[]. */
enum nc_slot {
    NC_NaN,
    NC_POSITIVE_INFINITY,
    NC_NEGATIVE_INFINITY,
    NC_MAX_VALUE,
    NC_MIN_VALUE,
    NC_LIMIT
};

/*
 * Some to most C compilers forbid spelling these at compile time, or barf
 * if you try, so all but MAX_VALUE are set up by js_InitRuntimeNumberState
 * using union jsdpun.
 */
static JSConstDoubleSpec number_constants[] = {
    {0,                         js_NaN_str,          0,{0,0,0}},
    {0,                         "POSITIVE_INFINITY", 0,{0,0,0}},
    {0,                         "NEGATIVE_INFINITY", 0,{0,0,0}},
    {1.7976931348623157E+308,   "MAX_VALUE",         0,{0,0,0}},
    {0,                         "MIN_VALUE",         0,{0,0,0}},
    {0,0,0,{0,0,0}}
};

jsdouble js_NaN;
jsdouble js_PositiveInfinity;
jsdouble js_NegativeInfinity;

#if (defined __GNUC__ && defined __i386__)

/*
 * Set the exception mask to mask all exceptions and set the FPU precision
 * to 53 bit mantissa (64 bit doubles).
 */
inline void FIX_FPU() {
    short control;
    asm("fstcw %0" : "=m" (control) : );
    control &= ~0x300; // Lower bits 8 and 9 (precision control).
    control |= 0x2f3;  // Raise bits 0-5 (exception masks) and 9 (64-bit precision).
    asm("fldcw %0" : : "m" (control) );
}

#else

#define FIX_FPU() ((void)0)

#endif

JSBool
js_InitRuntimeNumberState(JSContext *cx)
{
    JS_STATIC_ASSERT(JSVAL_NULL == jsval(0));

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(JSVAL_IS_NULL(rt->NaNValue));

    FIX_FPU();

    jsdpun u;
    u.s.hi = JSDOUBLE_HI32_EXPMASK | JSDOUBLE_HI32_MANTMASK;
    u.s.lo = 0xffffffff;
    number_constants[NC_NaN].dval = js_NaN = u.d;
    if (!js_NewDoubleInRootedValue(cx, u.d, &rt->NaNValue))
        return false;

    u.s.hi = JSDOUBLE_HI32_EXPMASK;
    u.s.lo = 0x00000000;
    number_constants[NC_POSITIVE_INFINITY].dval = js_PositiveInfinity = u.d;
    if (!js_NewDoubleInRootedValue(cx, u.d, &rt->positiveInfinityValue))
        return false;

    u.s.hi = JSDOUBLE_HI32_SIGNBIT | JSDOUBLE_HI32_EXPMASK;
    u.s.lo = 0x00000000;
    number_constants[NC_NEGATIVE_INFINITY].dval = js_NegativeInfinity = u.d;
    if (!js_NewDoubleInRootedValue(cx, u.d, &rt->negativeInfinityValue))
        return false;

    u.s.hi = 0;
    u.s.lo = 1;
    number_constants[NC_MIN_VALUE].dval = u.d;

    struct lconv *locale = localeconv();
    rt->thousandsSeparator =
        JS_strdup(cx, locale->thousands_sep ? locale->thousands_sep : "'");
    rt->decimalSeparator =
        JS_strdup(cx, locale->decimal_point ? locale->decimal_point : ".");
    rt->numGrouping =
        JS_strdup(cx, locale->grouping ? locale->grouping : "\3\0");

    return rt->thousandsSeparator && rt->decimalSeparator && rt->numGrouping;
}

void
js_TraceRuntimeNumberState(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    if (!JSVAL_IS_NULL(rt->NaNValue))
        JS_CALL_DOUBLE_TRACER(trc, JSVAL_TO_DOUBLE(rt->NaNValue), "NaN");
    if (!JSVAL_IS_NULL(rt->positiveInfinityValue)) {
        JS_CALL_DOUBLE_TRACER(trc, JSVAL_TO_DOUBLE(rt->positiveInfinityValue),
                              "+Infinity");
    }
    if (!JSVAL_IS_NULL(rt->negativeInfinityValue)) {
        JS_CALL_DOUBLE_TRACER(trc, JSVAL_TO_DOUBLE(rt->negativeInfinityValue),
                              "-Infinity");
    }
}

void
js_FinishRuntimeNumberState(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    rt->NaNValue = JSVAL_NULL;
    rt->negativeInfinityValue = JSVAL_NULL;
    rt->positiveInfinityValue = JSVAL_NULL;

    cx->free((void *) rt->thousandsSeparator);
    cx->free((void *) rt->decimalSeparator);
    cx->free((void *) rt->numGrouping);
    rt->thousandsSeparator = rt->decimalSeparator = rt->numGrouping = NULL;
}

JSObject *
js_InitNumberClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto, *ctor;
    JSRuntime *rt;

    /* XXX must do at least once per new thread, so do it per JSContext... */
    FIX_FPU();

    if (!JS_DefineFunctions(cx, obj, number_functions))
        return NULL;

    proto = JS_InitClass(cx, obj, NULL, &js_NumberClass, Number, 1,
                         NULL, number_methods, NULL, NULL);
    if (!proto || !(ctor = JS_GetConstructor(cx, proto)))
        return NULL;
    proto->fslots[JSSLOT_PRIMITIVE_THIS] = JSVAL_ZERO;
    if (!JS_DefineConstDoubles(cx, ctor, number_constants))
        return NULL;

    /* ECMA 15.1.1.1 */
    rt = cx->runtime;
    if (!JS_DefineProperty(cx, obj, js_NaN_str, rt->NaNValue,
                           NULL, NULL, JSPROP_PERMANENT)) {
        return NULL;
    }

    /* ECMA 15.1.1.2 */
    if (!JS_DefineProperty(cx, obj, js_Infinity_str, rt->positiveInfinityValue,
                           NULL, NULL, JSPROP_PERMANENT)) {
        return NULL;
    }
    return proto;
}

JSBool
js_NewNumberInRootedValue(JSContext *cx, jsdouble d, jsval *vp)
{
    jsint i;

    if (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i)) {
        *vp = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return js_NewDoubleInRootedValue(cx, d, vp);
}

JSBool
js_NewWeaklyRootedNumber(JSContext *cx, jsdouble d, jsval *rval)
{
    jsint i;
    if (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i)) {
        *rval = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return JS_NewDoubleValue(cx, d, rval);
}

/*
 * Convert a number to C string. The buf must be large enough to accommodate
 * the result, including '-' and '\0', if base == 10 or d is an integer that
 * fits in 32 bits. The caller must free the resulting pointer if it does not
 * point into buf.
 */
static char *
NumberToCString(JSContext *cx, jsdouble d, jsint base, char *buf, size_t bufSize)
{
    jsint i;
    char *numStr;

    JS_ASSERT(bufSize >= DTOSTR_STANDARD_BUFFER_SIZE);
    if (JSDOUBLE_IS_INT(d, i)) {
        numStr = IntToCString(i, base, buf, bufSize);
    } else {
        if (base == 10)
            numStr = JS_dtostr(buf, bufSize, DTOSTR_STANDARD, 0, d);
        else
            numStr = JS_dtobasestr(base, d);
        if (!numStr) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }
    return numStr;
}

static JSString * JS_FASTCALL
js_NumberToStringWithBase(JSContext *cx, jsdouble d, jsint base)
{
    /*
     * The longest possible result here that would need to fit in buf is
     * (-0x80000000).toString(2), which has length 33.  (This can produce
     * longer results, but in those cases buf is not used; see comment at
     * NumberToCString.)
     */
    char buf[34];
    char *numStr;
    JSString *s;

    /*
     * Caller is responsible for error reporting. When called from trace,
     * returning NULL here will cause us to fall of trace and then retry
     * from the interpreter (which will report the error).
     */
    if (base < 2 || base > 36)
        return NULL;

    jsint i;
    if (JSDOUBLE_IS_INT(d, i)) {
        if (base == 10 && jsuint(i) < INT_STRING_LIMIT)
            return JSString::intString(i);
        if (jsuint(i) < jsuint(base)) {
            if (i < 10)
                return JSString::intString(i);
            return JSString::unitString('a' + i - 10);
        }
    }
    numStr = NumberToCString(cx, d, base, buf, sizeof buf);
    if (!numStr)
        return NULL;
    s = JS_NewStringCopyZ(cx, numStr);
    if (!(numStr >= buf && numStr < buf + sizeof buf))
        js_free(numStr);
    return s;
}

JSString * JS_FASTCALL
js_NumberToString(JSContext *cx, jsdouble d)
{
    return js_NumberToStringWithBase(cx, d, 10);
}

JSBool JS_FASTCALL
js_NumberValueToCharBuffer(JSContext *cx, jsval v, JSCharBuffer &cb)
{
    /* Convert to C-string. */
    static const size_t arrSize = DTOSTR_STANDARD_BUFFER_SIZE;
    char arr[arrSize];
    const char *cstr;
    if (JSVAL_IS_INT(v)) {
        cstr = IntToCString(JSVAL_TO_INT(v), 10, arr, arrSize);
    } else {
        JS_ASSERT(JSVAL_IS_DOUBLE(v));
        cstr = JS_dtostr(arr, arrSize, DTOSTR_STANDARD, 0, *JSVAL_TO_DOUBLE(v));
    }
    if (!cstr)
        return JS_FALSE;

    /*
     * Inflate to jschar string.  The input C-string characters are < 127, so
     * even if jschars are UTF-8, all chars should map to one jschar.
     */
    size_t cstrlen = strlen(cstr);
    JS_ASSERT(cstrlen < arrSize);
    size_t sizeBefore = cb.length();
    if (!cb.growBy(cstrlen))
        return JS_FALSE;
    jschar *appendBegin = cb.begin() + sizeBefore;
#ifdef DEBUG
    size_t oldcstrlen = cstrlen;
    JSBool ok =
#endif
        js_InflateStringToBuffer(cx, cstr, cstrlen, appendBegin, &cstrlen);
    JS_ASSERT(ok && cstrlen == oldcstrlen);
    return JS_TRUE;
}

jsdouble
js_ValueToNumber(JSContext *cx, jsval *vp)
{
    jsval v;
    JSString *str;
    const jschar *bp, *end, *ep;
    jsdouble d;
    JSObject *obj;

    v = *vp;
    for (;;) {
        if (JSVAL_IS_INT(v))
            return (jsdouble) JSVAL_TO_INT(v);
        if (JSVAL_IS_DOUBLE(v))
            return *JSVAL_TO_DOUBLE(v);
        if (JSVAL_IS_STRING(v)) {
            str = JSVAL_TO_STRING(v);

            /*
             * Note that ECMA doesn't treat a string beginning with a '0' as
             * an octal number here. This works because all such numbers will
             * be interpreted as decimal by js_strtod and will never get
             * passed to js_strtointeger (which would interpret them as
             * octal).
             */
            str->getCharsAndEnd(bp, end);

            /* ECMA doesn't allow signed hex numbers (bug 273467). */
            bp = js_SkipWhiteSpace(bp, end);
            if (bp + 2 < end && (*bp == '-' || *bp == '+') &&
                bp[1] == '0' && (bp[2] == 'X' || bp[2] == 'x')) {
                break;
            }

            if ((!js_strtod(cx, bp, end, &ep, &d) ||
                 js_SkipWhiteSpace(ep, end) != end) &&
                (!js_strtointeger(cx, bp, end, &ep, 0, &d) ||
                 js_SkipWhiteSpace(ep, end) != end)) {
                break;
            }

            /*
             * JSVAL_TRUE indicates that double jsval was never constructed
             * for the result.
             */
            *vp = JSVAL_TRUE;
            return d;
        }
        if (JSVAL_IS_BOOLEAN(v)) {
            if (JSVAL_TO_BOOLEAN(v)) {
                *vp = JSVAL_ONE;
                return 1.0;
            } else {
                *vp = JSVAL_ZERO;
                return 0.0;
            }
        }
        if (JSVAL_IS_NULL(v)) {
            *vp = JSVAL_ZERO;
            return 0.0;
        }
        if (JSVAL_IS_VOID(v))
            break;

        JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
        obj = JSVAL_TO_OBJECT(v);

        /*
         * vp roots obj so we cannot use it as an extra root for
         * obj->defaultValue result when calling the hook.
         */
        JSAutoTempValueRooter tvr(cx, v);
        if (!obj->defaultValue(cx, JSTYPE_NUMBER, tvr.addr()))
            obj = NULL;
        else
            v = *vp = tvr.value();
        if (!obj) {
            *vp = JSVAL_NULL;
            return 0.0;
        }
        if (!JSVAL_IS_PRIMITIVE(v))
            break;
    }

    *vp = cx->runtime->NaNValue;
    return js_NaN;
}

int32
js_ValueToECMAInt32(JSContext *cx, jsval *vp)
{
    jsval v;
    jsdouble d;

    v = *vp;
    if (JSVAL_IS_INT(v))
        return JSVAL_TO_INT(v);
    if (JSVAL_IS_DOUBLE(v)) {
        d = *JSVAL_TO_DOUBLE(v);
        *vp = JSVAL_TRUE;
    } else {
        d = js_ValueToNumber(cx, vp);
        if (JSVAL_IS_NULL(*vp))
            return 0;
        *vp = JSVAL_TRUE;
    }
    return js_DoubleToECMAInt32(d);
}

uint32
js_ValueToECMAUint32(JSContext *cx, jsval *vp)
{
    jsval v;
    jsint i;
    jsdouble d;

    v = *vp;
    if (JSVAL_IS_INT(v)) {
        i = JSVAL_TO_INT(v);
        if (i < 0)
            *vp = JSVAL_TRUE;
        return (uint32) i;
    }
    if (JSVAL_IS_DOUBLE(v)) {
        d = *JSVAL_TO_DOUBLE(v);
        *vp = JSVAL_TRUE;
    } else {
        d = js_ValueToNumber(cx, vp);
        if (JSVAL_IS_NULL(*vp))
            return 0;
        *vp = JSVAL_TRUE;
    }
    return js_DoubleToECMAUint32(d);
}

uint32
js_DoubleToECMAUint32(jsdouble d)
{
    int32 i;
    JSBool neg;
    jsdouble two32;

    if (!JSDOUBLE_IS_FINITE(d))
        return 0;

    /*
     * We check whether d fits int32, not uint32, as all but the ">>>" bit
     * manipulation bytecode stores the result as int, not uint. When the
     * result does not fit int jsval, it will be stored as a negative double.
     */
    i = (int32) d;
    if ((jsdouble) i == d)
        return (int32)i;

    neg = (d < 0);
    d = floor(neg ? -d : d);
    d = neg ? -d : d;

    two32 = 4294967296.0;
    d = fmod(d, two32);

    return (uint32) (d >= 0 ? d : d + two32);
}

int32
js_ValueToInt32(JSContext *cx, jsval *vp)
{
    jsval v;
    jsdouble d;

    v = *vp;
    if (JSVAL_IS_INT(v))
        return JSVAL_TO_INT(v);
    d = js_ValueToNumber(cx, vp);
    if (JSVAL_IS_NULL(*vp))
        return 0;
    if (JSVAL_IS_INT(*vp))
        return JSVAL_TO_INT(*vp);

    *vp = JSVAL_TRUE;
    if (JSDOUBLE_IS_NaN(d) || d <= -2147483649.0 || 2147483648.0 <= d) {
        js_ReportValueError(cx, JSMSG_CANT_CONVERT,
                            JSDVG_SEARCH_STACK, v, NULL);
        *vp = JSVAL_NULL;
        return 0;
    }
    return (int32) floor(d + 0.5);  /* Round to nearest */
}

uint16
js_ValueToUint16(JSContext *cx, jsval *vp)
{
    jsdouble d;
    uint16 u;
    jsuint m;
    JSBool neg;

    d = js_ValueToNumber(cx, vp);
    if (JSVAL_IS_NULL(*vp))
        return 0;

    if (JSVAL_IS_INT(*vp)) {
        u = (uint16) JSVAL_TO_INT(*vp);
    } else if (d == 0 || !JSDOUBLE_IS_FINITE(d)) {
        u = (uint16) 0;
    } else {
        u = (uint16) d;
        if ((jsdouble) u != d) {
            neg = (d < 0);
            d = floor(neg ? -d : d);
            d = neg ? -d : d;
            m = JS_BIT(16);
            d = fmod(d, (double) m);
            if (d < 0)
                d += m;
            u = (uint16) d;
        }
    }
    *vp = INT_TO_JSVAL(u);
    return u;
}

JSBool
js_strtod(JSContext *cx, const jschar *s, const jschar *send,
          const jschar **ep, jsdouble *dp)
{
    const jschar *s1;
    size_t length, i;
    char cbuf[32];
    char *cstr, *istr, *estr;
    JSBool negative;
    jsdouble d;

    s1 = js_SkipWhiteSpace(s, send);
    length = send - s1;

    /* Use cbuf to avoid malloc */
    if (length >= sizeof cbuf) {
        cstr = (char *) cx->malloc(length + 1);
        if (!cstr)
           return JS_FALSE;
    } else {
        cstr = cbuf;
    }

    for (i = 0; i != length; i++) {
        if (s1[i] >> 8)
            break;
        cstr[i] = (char)s1[i];
    }
    cstr[i] = 0;

    istr = cstr;
    if ((negative = (*istr == '-')) != 0 || *istr == '+')
        istr++;
    if (*istr == 'I' && !strncmp(istr, js_Infinity_str, sizeof js_Infinity_str - 1)) {
        d = negative ? js_NegativeInfinity : js_PositiveInfinity;
        estr = istr + 8;
    } else {
        int err;
        d = JS_strtod(cstr, &estr, &err);
        if (d == HUGE_VAL)
            d = js_PositiveInfinity;
        else if (d == -HUGE_VAL)
            d = js_NegativeInfinity;
    }

    i = estr - cstr;
    if (cstr != cbuf)
        cx->free(cstr);
    *ep = i ? s1 + i : s;
    *dp = d;
    return JS_TRUE;
}

struct BinaryDigitReader
{
    uintN base;                 /* Base of number; must be a power of 2 */
    uintN digit;                /* Current digit value in radix given by base */
    uintN digitMask;            /* Mask to extract the next bit from digit */
    const jschar *digits;       /* Pointer to the remaining digits */
    const jschar *end;          /* Pointer to first non-digit */
};

/* Return the next binary digit from the number or -1 if done */
static intN GetNextBinaryDigit(struct BinaryDigitReader *bdr)
{
    intN bit;

    if (bdr->digitMask == 0) {
        uintN c;

        if (bdr->digits == bdr->end)
            return -1;

        c = *bdr->digits++;
        if ('0' <= c && c <= '9')
            bdr->digit = c - '0';
        else if ('a' <= c && c <= 'z')
            bdr->digit = c - 'a' + 10;
        else
            bdr->digit = c - 'A' + 10;
        bdr->digitMask = bdr->base >> 1;
    }
    bit = (bdr->digit & bdr->digitMask) != 0;
    bdr->digitMask >>= 1;
    return bit;
}

JSBool
js_strtointeger(JSContext *cx, const jschar *s, const jschar *send,
                const jschar **ep, jsint base, jsdouble *dp)
{
    const jschar *s1, *start;
    JSBool negative;
    jsdouble value;

    s1 = js_SkipWhiteSpace(s, send);
    if (s1 == send)
        goto no_digits;
    if ((negative = (*s1 == '-')) != 0 || *s1 == '+') {
        s1++;
        if (s1 == send)
            goto no_digits;
    }

    if (base == 0) {
        /* No base supplied, or some base that evaluated to 0. */
        if (*s1 == '0') {
            /* It's either hex or octal; only increment char if str isn't '0' */
            if (s1 + 1 != send && (s1[1] == 'X' || s1[1] == 'x')) {
                base = 16;
                s1 += 2;
                if (s1 == send)
                    goto no_digits;
            } else {
                base = 8;
            }
        } else {
            base = 10; /* Default to decimal. */
        }
    } else if (base == 16) {
        /* If base is 16, ignore hex prefix. */
        if (*s1 == '0' && s1 + 1 != send && (s1[1] == 'X' || s1[1] == 'x')) {
            s1 += 2;
            if (s1 == send)
                goto no_digits;
        }
    }

    /*
     * Done with the preliminaries; find some prefix of the string that's
     * a number in the given base.
     */
    JS_ASSERT(s1 < send);
    start = s1;
    value = 0.0;
    do {
        uintN digit;
        jschar c = *s1;
        if ('0' <= c && c <= '9')
            digit = c - '0';
        else if ('a' <= c && c <= 'z')
            digit = c - 'a' + 10;
        else if ('A' <= c && c <= 'Z')
            digit = c - 'A' + 10;
        else
            break;
        if (digit >= (uintN)base)
            break;
        value = value * base + digit;
    } while (++s1 != send);

    if (value >= 9007199254740992.0) {
        if (base == 10) {
            /*
             * If we're accumulating a decimal number and the number is >=
             * 2^53, then the result from the repeated multiply-add above may
             * be inaccurate.  Call JS_strtod to get the correct answer.
             */
            size_t i;
            size_t length = s1 - start;
            char *cstr = (char *) cx->malloc(length + 1);
            char *estr;
            int err=0;

            if (!cstr)
                return JS_FALSE;
            for (i = 0; i != length; i++)
                cstr[i] = (char)start[i];
            cstr[length] = 0;

            value = JS_strtod(cstr, &estr, &err);
            if (err == JS_DTOA_ENOMEM) {
                JS_ReportOutOfMemory(cx);
                cx->free(cstr);
                return JS_FALSE;
            }
            if (err == JS_DTOA_ERANGE && value == HUGE_VAL)
                value = js_PositiveInfinity;
            cx->free(cstr);
        } else if ((base & (base - 1)) == 0) {
            /*
             * The number may also be inaccurate for power-of-two bases.  This
             * happens if the addition in value * base + digit causes a round-
             * down to an even least significant mantissa bit when the first
             * dropped bit is a one.  If any of the following digits in the
             * number (which haven't been added in yet) are nonzero, then the
             * correct action would have been to round up instead of down.  An
             * example occurs when reading the number 0x1000000000000081, which
             * rounds to 0x1000000000000000 instead of 0x1000000000000100.
             */
            struct BinaryDigitReader bdr;
            intN bit, bit2;
            intN j;

            bdr.base = base;
            bdr.digit = 0;      // shut GCC up
            bdr.digitMask = 0;
            bdr.digits = start;
            bdr.end = s1;
            value = 0.0;

            /* Skip leading zeros. */
            do {
                bit = GetNextBinaryDigit(&bdr);
            } while (bit == 0);

            if (bit == 1) {
                /* Gather the 53 significant bits (including the leading 1) */
                value = 1.0;
                for (j = 52; j; j--) {
                    bit = GetNextBinaryDigit(&bdr);
                    if (bit < 0)
                        goto done;
                    value = value*2 + bit;
                }
                /* bit2 is the 54th bit (the first dropped from the mantissa) */
                bit2 = GetNextBinaryDigit(&bdr);
                if (bit2 >= 0) {
                    jsdouble factor = 2.0;
                    intN sticky = 0;  /* sticky is 1 if any bit beyond the 54th is 1 */
                    intN bit3;

                    while ((bit3 = GetNextBinaryDigit(&bdr)) >= 0) {
                        sticky |= bit3;
                        factor *= 2;
                    }
                    value += bit2 & (bit | sticky);
                    value *= factor;
                }
              done:;
            }
        }
    }
    /* We don't worry about inaccurate numbers for any other base. */

    if (s1 == start) {
      no_digits:
        *dp = 0.0;
        *ep = s;
    } else {
        *dp = negative ? -value : value;
        *ep = s1;
    }
    return JS_TRUE;
}
