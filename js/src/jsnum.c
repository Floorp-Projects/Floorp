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
 * JS number type and wrapper class.
 */
#include "jsstddef.h"
#include <errno.h>
#ifdef XP_PC
#include <float.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "prdtoa.h"
#include "prprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsstr.h"

union dpun {
    struct {
#ifdef IS_LITTLE_ENDIAN
	uint32 lo, hi;
#else
	uint32 hi, lo;
#endif
    } s;
    jsdouble d;
};

static JSBool
num_isNaN(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    *rval = BOOLEAN_TO_JSVAL(JSDOUBLE_IS_NaN(x));
    return JS_TRUE;
}

static JSBool
num_isFinite(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble x;

    if (!js_ValueToNumber(cx, argv[0], &x))
	return JS_FALSE;
    *rval = BOOLEAN_TO_JSVAL(JSDOUBLE_IS_FINITE(x));
    return JS_TRUE;
}

static JSBool
num_parseFloat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSString *str;
    jsdouble d, *dp;
    jschar *ep;

    str = js_ValueToString(cx, argv[0]);
    if (!str)
	return JS_FALSE;
    if (!js_strtod(str->chars, &ep, &d) || ep == str->chars) {
	*rval = DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
    } else {
	dp = js_NewDouble(cx, d);
	if (!dp)
	    return JS_FALSE;
	*rval = DOUBLE_TO_JSVAL(dp);
    }
    return JS_TRUE;
}

/* See ECMA 15.1.2.2. */
static JSBool
num_parseInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    jsint radix;
    const jschar *chars, *start;
    jsdouble sum;
    JSBool negative;
    uintN digit, newDigit;
    jschar c;
    jschar digitMax = '9';
    jschar lowerCaseBound = 'a';
    jschar upperCaseBound = 'A';
    
    str = js_ValueToString(cx, argv[0]);
    if (!str)
	return JS_FALSE;
    chars = str->chars;

    /* This assumes that the char strings are null-terminated - JS_ISSPACE will
     * always evaluate to false at the end of a string containing only
     * whitespace.
     */
    while (JS_ISSPACE(*chars))
        chars++;

    if ((negative = (*chars == '-')) != 0 || *chars == '+')
        chars++;

    if (argc > 1) {
	if (!js_ValueToECMAInt32(cx, argv[1], &radix))
	    return JS_FALSE;
    } else {
        radix = 0;
    }

    if (radix != 0) {
        /* Some explicit radix was supplied */
        if (radix < 2 || radix > 36) {
            *rval = DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
            return JS_TRUE;
        } else {
            if (radix == 16 && *chars == '0' &&
                (*(chars + 1) == 'X' || *(chars + 1) == 'x'))
                /* If radix is 16, ignore hex prefix. */
                chars += 2;
        }
    } else {
        /* No radix supplied, or some radix that evaluated to 0. */ 
        if (*chars == '0') {
            /* It's either hex or octal; only increment char if str isn't '0' */
            if (*(chars + 1) == 'X' || *(chars + 1) == 'x') /* Hex */
            {
                chars += 2;
                radix = 16;
            } else { /* Octal */
                radix = 8;
            }
        } else {
            radix = 10; /* Default to decimal. */
        }
    }

    /* Done with the preliminaries; find some prefix of the string that's
     * a number in the given radix.  XXX might want to farm this out to
     * a utility function, to share with the lexer's handling of
     * literal constants.
     */
    start = chars; /* Mark - if string is empty, we return NaN. */
    if (radix < 10) {
        digitMax = (char) ('0' + radix - 1);
    } else {
        digitMax = '9';
    }
    if (radix > 10) {
        lowerCaseBound = (char) ('a' + radix - 10);
        upperCaseBound = (char) ('A' + radix - 10);
    } else {
        lowerCaseBound = 'a';
        upperCaseBound = 'A';
    }
    digit = 0; /* how many digits have we seen? (if radix == 10) */
    sum = 0;
    while ((c = *chars) != 0) { /* 0 is never a valid character. */
        if ('0' <= c && c <= digitMax)
            newDigit = c - '0';
        else if ('a' <= c && c < lowerCaseBound)
            newDigit = c - 'a' + 10;
        else if ('A' <= c && c < upperCaseBound)
            newDigit = c - 'A' + 10;
        else
            break;
        /* ECMA: 'But if R is 10 and Z contains more than 20 significant
         * digits, every digit after the 20th may be replaced by a 0...'
         * I'm not sure if doing this way is actually an improvement.
         * XXX this may bear closer examination......
         */
        if (radix == 10 && ++digit > 20)
            sum = sum*radix;
        else
            sum = sum*radix + newDigit;
        chars++;
    }

    if (chars == start) {
        *rval = DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
    } else {
        if (!js_NewNumberValue(cx, (negative ? -sum : sum), rval))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec number_functions[] = {
    {"isNaN",           num_isNaN,              1},
    {"isFinite",        num_isFinite,           1},
    {"parseFloat",      num_parseFloat,         1},
    {"parseInt",        num_parseInt,           2},
    {0}
};

static JSClass number_class = {
    "Number",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSBool
Number(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsdouble d;
    jsval v;

    if (argc != 0) {
	if (!js_ValueToNumber(cx, argv[0], &d))
	    return JS_FALSE;
    } else {
	d = 0.0;
    }
    if (!js_NewNumberValue(cx, d, &v))
	return JS_FALSE;
    if (!cx->fp->constructing) {
	*rval = v;
	return JS_TRUE;
    }
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, v);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
static JSBool
num_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    jsdouble d;
    size_t i;
    char buf[64];
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &number_class, argv))
	return JS_FALSE;
    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (!JSVAL_IS_NUMBER(v))
	return js_obj_toSource(cx, obj, argc, argv, rval);
    d = JSVAL_IS_INT(v) ? (jsdouble)JSVAL_TO_INT(v) : *JSVAL_TO_DOUBLE(v);
    i = PR_snprintf(buf, sizeof buf, "(new %s(", number_class.name);

    PR_cnvtf(buf + i, sizeof buf - i, 20, d);
    i = strlen(buf);
    PR_snprintf(buf + i, sizeof buf - i, "))");
    str = JS_NewStringCopyZ(cx, buf);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif

static JSBool
num_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    jsdouble d;
    jsint base, ival, dval;
    char *bp, buf[32];
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &number_class, argv))
	return JS_FALSE;
    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (!JSVAL_IS_NUMBER(v))
	return js_obj_toString(cx, obj, argc, argv, rval);
    d = JSVAL_IS_INT(v) ? (jsdouble)JSVAL_TO_INT(v) : *JSVAL_TO_DOUBLE(v);
    if (argc != 0) {
	if (!js_ValueToECMAInt32(cx, argv[0], &base))
	    return JS_FALSE;
	if (base < 2 || base > 36) {
	    JS_ReportError(cx, "illegal radix %d", base);
	    return JS_FALSE;
	}
	if (base != 10 && JSDOUBLE_IS_FINITE(d)) {
	    ival = (jsint) js_DoubleToInteger(d);
	    bp = buf + sizeof buf;
	    for (*--bp = '\0'; ival != 0 && --bp >= buf; ival /= base) {
		dval = ival % base;
		*bp = (char)((dval >= 10) ? 'a' - 10 + dval : '0' + dval);
	    }
	    if (*bp == '\0')
		*--bp = '0';
	    str = JS_NewStringCopyZ(cx, bp);
	} else {
	    str = js_NumberToString(cx, d);
	}
    } else {
	str = js_NumberToString(cx, d);
    }
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
num_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &number_class, argv))
	return JS_FALSE;
    *rval = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    return JS_TRUE;
}

static JSFunctionSpec number_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   num_toSource,   0},
#endif
    {js_toString_str,	num_toString,	0},
    {js_valueOf_str,	num_valueOf,	0},
    {0}
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
 * if you try, so all but MAX_VALUE are set at runtime by js_InitNumberClass
 * using union dpun.
 */
static JSConstDoubleSpec number_constants[] = {
    {0,                         "NaN"},
    {0,                         "POSITIVE_INFINITY"},
    {0,                         "NEGATIVE_INFINITY"},
    {1.7976931348623157E+308,   "MAX_VALUE"},
    {0,                         "MIN_VALUE"},
    {0}
};

static jsdouble NaN;

JSObject *
js_InitNumberClass(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    union dpun u;
    JSObject *proto, *ctor;

    rt = cx->runtime;
    if (!rt->jsNaN) {
#ifdef XP_PC
#ifdef XP_OS2
	/*DSR071597 - I have no idea what this really does other than mucking with the floating     */
	/*point unit, but it does fix a "floating point underflow" exception I am getting, and there*/
	/*is similar code in the Hursley java. Making sure we have the same code in Javascript      */
	/*where Netscape was calling control87 on Windows...                                        */
	_control87(MCW_EM+PC_53+RC_NEAR,MCW_EM+MCW_PC+MCW_RC);
#else
	_control87(MCW_EM, MCW_EM);
#endif
#endif

	u.s.hi = JSDOUBLE_HI32_EXPMASK | JSDOUBLE_HI32_MANTMASK;
	u.s.lo = 0xffffffff;
	number_constants[NC_NaN].dval = NaN = u.d;
	rt->jsNaN = js_NewDouble(cx, NaN);
	if (!rt->jsNaN || !js_LockGCThing(cx, rt->jsNaN))
	    return NULL;

	u.s.hi = JSDOUBLE_HI32_EXPMASK;
	u.s.lo = 0x00000000;
	number_constants[NC_POSITIVE_INFINITY].dval = u.d;
	rt->jsPositiveInfinity = js_NewDouble(cx, u.d);
	if (!rt->jsPositiveInfinity ||
	    !js_LockGCThing(cx, rt->jsPositiveInfinity)) {
	    return NULL;
	}

	u.s.hi = JSDOUBLE_HI32_SIGNBIT | JSDOUBLE_HI32_EXPMASK;
	u.s.lo = 0x00000000;
	number_constants[NC_NEGATIVE_INFINITY].dval = u.d;
	rt->jsNegativeInfinity = js_NewDouble(cx, u.d);
	if (!rt->jsNegativeInfinity ||
	    !js_LockGCThing(cx, rt->jsNegativeInfinity)) {
	    return NULL;
	}

	u.s.hi = 0;
	u.s.lo = 1;
	number_constants[NC_MIN_VALUE].dval = u.d;
    }

    if (!JS_DefineFunctions(cx, obj, number_functions))
	return NULL;

    proto = JS_InitClass(cx, obj, NULL, &number_class, Number, 1,
			 NULL, number_methods, NULL, NULL);
    if (!proto || !(ctor = JS_GetConstructor(cx, proto)))
	return NULL;
    OBJ_SET_SLOT(cx, proto, JSSLOT_PRIVATE, JSVAL_ZERO);
    if (!JS_DefineConstDoubles(cx, ctor, number_constants))
	return NULL;

    /* ECMA 15.1.1.1 */
    if (!JS_DefineProperty(cx, obj, "NaN", DOUBLE_TO_JSVAL(rt->jsNaN),
			   NULL, NULL, 0)) {
	return NULL;
    }

    /* ECMA 15.1.1.2 */
    if (!JS_DefineProperty(cx, obj, "Infinity",
			   DOUBLE_TO_JSVAL(rt->jsPositiveInfinity),
			   NULL, NULL, 0)) {
	return NULL;
    }
    return proto;
}

jsdouble *
js_NewDouble(JSContext *cx, jsdouble d)
{
    jsdouble *dp;

    dp = js_AllocGCThing(cx, GCX_DOUBLE);
    if (!dp)
	return NULL;
    *dp = d;
    return dp;
}

void
js_FinalizeDouble(JSContext *cx, jsdouble *dp)
{
    *dp = NaN;
}

JSBool
js_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval)
{
    jsdouble *dp;

    dp = js_NewDouble(cx, d);
    if (!dp)
	return JS_FALSE;
    *rval = DOUBLE_TO_JSVAL(dp);
    return JS_TRUE;
}

JSBool
js_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    jsint i;

    i = (jsint)d;
    if (JSDOUBLE_IS_INT(d, i) && INT_FITS_IN_JSVAL(i)) {
	*rval = INT_TO_JSVAL(i);
    } else {
	if (!js_NewDoubleValue(cx, d, rval))
	    return JS_FALSE;
    }
    return JS_TRUE;
}

JSObject *
js_NumberToObject(JSContext *cx, jsdouble d)
{
    JSObject *obj;
    jsval v;

    obj = js_NewObject(cx, &number_class, NULL, NULL);
    if (!obj)
	return NULL;
    if (!js_NewNumberValue(cx, d, &v)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, v);
    return obj;
}

/* XXXbe rewrite me to be ECMA-based! */
JSString *
js_NumberToString(JSContext *cx, jsdouble d)
{
    jsint i;
    char buf[32];

    i = (jsint)d;
    if (JSDOUBLE_IS_INT(d, i)) {
	PR_snprintf(buf, sizeof buf, "%ld", (long)i);
    } else {
	PR_cnvtf(buf, sizeof buf, 20, d);
    }
    return JS_NewStringCopyZ(cx, buf);
}

JSBool
js_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    JSObject *obj;
    JSString *str;
    jschar *ep;
    jsdouble d;

    if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (!obj) {
	    *dp = 0;
	    return JS_TRUE;
	}
	if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_NUMBER, &v))
	    return JS_FALSE;
    }
    if (JSVAL_IS_INT(v)) {
	*dp = (jsdouble)JSVAL_TO_INT(v);
    } else if (JSVAL_IS_DOUBLE(v)) {
	*dp = *JSVAL_TO_DOUBLE(v);
    } else if (JSVAL_IS_STRING(v)) {
	str = JSVAL_TO_STRING(v);
	errno = 0;
	if ((!js_strtod(str->chars, &ep, &d) || *ep != 0) &&
	    (!js_strtol(str->chars, &ep, 0, &d) || *ep != 0)) {
	    goto badstr;
	}
	*dp = d;
    } else if (JSVAL_IS_BOOLEAN(v)) {
	*dp = JSVAL_TO_BOOLEAN(v) ? 1 : 0;
    } else {
#if JS_BUG_FALLIBLE_TONUM
	str = js_DecompileValueGenerator(cx, v, NULL);
badstr:
	if (str) {
	    JS_ReportError(cx, "%s is not a number",
			   JS_GetStringBytes(str));
	}
	return JS_FALSE;
#else
badstr:
	*dp = *cx->runtime->jsNaN;
#endif
    }
    return JS_TRUE;
}

JSBool
js_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    jsdouble d;

    if (!js_ValueToNumber(cx, v, &d))
        return JS_FALSE;
    return js_DoubleToECMAInt32(cx, d, ip);
}

JSBool
js_DoubleToECMAInt32(JSContext *cx, jsdouble d, int32 *ip)
{
    jsdouble two32 = 4294967296.0;
    jsdouble two31 = 2147483648.0;

    if (!JSDOUBLE_IS_FINITE(d) || d == 0) {
        *ip = 0;
        return JS_TRUE;
    }
    d = fmod(d, two32);
    d = d >= 0 ? d : d + two32;
    if (d >= two31)
        *ip = (int32)(d - two32);
    else
        *ip = (int32)d;
    return JS_TRUE;
}

JSBool
js_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    jsdouble d;

    if (!js_ValueToNumber(cx, v, &d))
        return JS_FALSE;
    return js_DoubleToECMAUint32(cx, d, ip);
}

JSBool
js_DoubleToECMAUint32(JSContext *cx, jsdouble d, uint32 *ip)
{
    JSBool neg;
    jsdouble two32 = 4294967296.0;

    if (!JSDOUBLE_IS_FINITE(d) || d == 0) {
        *ip = 0;
        return JS_TRUE;
    }

    neg = (d < 0);
    d = floor(neg ? -d : d);
    d = neg ? -d : d;

    d = fmod(d, two32);

    d = d >= 0 ? d : d + two32;
    *ip = (uint32)d;
    return JS_TRUE;
}

JSBool
js_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    jsdouble d;
    JSString *str;

    if (!js_ValueToNumber(cx, v, &d))
	return JS_FALSE;
    if (JSDOUBLE_IS_NaN(d) || d <= -2147483649.0 || 2147483648.0 <= d) {
	str = js_DecompileValueGenerator(cx, v, NULL);
	if (str) {
	    JS_ReportError(cx, "can't convert %s to an integer",
			   JS_GetStringBytes(str));
	}
	return JS_FALSE;
    }
    *ip = (int32)floor(d + 0.5);     /* Round to nearest */
    return JS_TRUE;
}

JSBool
js_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    jsdouble d;
    jsuint i, m;
    JSBool neg;

    if (!js_ValueToNumber(cx, v, &d))
	return JS_FALSE;
    if (d == 0 || !JSDOUBLE_IS_FINITE(d)) {
	*ip = 0;
	return JS_TRUE;
    }
    i = (jsuint)d;
    if ((jsdouble)i == d) {
	*ip = (uint16)i;
	return JS_TRUE;
    }
    neg = (d < 0);
    d = floor(neg ? -d : d);
    d = neg ? -d : d;
    m = PR_BIT(16);
    d = fmod(d, m);
    if (d < 0)
	d += m;
    *ip = (uint16) d;
    return JS_TRUE;
}

jsdouble
js_DoubleToInteger(jsdouble d)
{
    JSBool neg;

    if (d == 0)
	return d;
    if (!JSDOUBLE_IS_FINITE(d)) {
	if (JSDOUBLE_IS_NaN(d))
	    return 0;
	return d;
    }
    neg = (d < 0);
    d = floor(neg ? -d : d);
    return neg ? -d : d;
}

/* XXXbe rewrite me! */
JSBool
js_strtod(const jschar *s, jschar **ep, jsdouble *dp)
{
    size_t i, n;
    char *cstr, *estr;
    jsdouble d;

    n = js_strlen(s);
    cstr = malloc(n + 1);
    if (!cstr)
	return JS_FALSE;
    for (i = 0; i <= n; i++) {
	if (s[i] >> 8) {
	    free(cstr);
	    return JS_FALSE;
	}
	cstr[i] = (char)s[i];
    }
    errno = 0;
    d = PR_strtod(cstr, &estr);
    free(cstr);
    if (errno == ERANGE)
	return JS_FALSE;
    *ep = (jschar *)s + (estr - cstr);
    *dp = d;
    return JS_TRUE;
}

/* XXXbe rewrite me! */
JSBool
js_strtol(const jschar *s, jschar **ep, jsint base, jsdouble *dp)
{
    size_t i, n;
    char *cstr, *estr;
    long l;

    n = js_strlen(s);
    cstr = malloc(n + 1);
    if (!cstr)
	return JS_FALSE;
    for (i = 0; i <= n; i++) {
	if (s[i] >> 8) {
	    free(cstr);
	    return JS_FALSE;
	}
	cstr[i] = (char)s[i];
    }
    errno = 0;
    l = strtol(cstr, &estr, base);
    free(cstr);
    if (errno == ERANGE)
	return JS_FALSE;
    *ep = (jschar *)s + (estr - cstr);
    *dp = (jsdouble)l;
    return JS_TRUE;
}
