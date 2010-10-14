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
#include "jsutil.h"
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
#include "jstracer.h"
#include "jsvector.h"

#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsstrinlines.h"

using namespace js;

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

namespace {

/*
 * If we're accumulating a decimal number and the number is >= 2^53, then the
 * fast result from the loop in GetPrefixInteger may be inaccurate. Call
 * js_strtod_harder to get the correct answer.
 */
bool
ComputeAccurateDecimalInteger(JSContext *cx, const jschar *start, const jschar *end, jsdouble *dp)
{
    size_t length = end - start;
    char *cstr = static_cast<char *>(cx->malloc(length + 1));
    if (!cstr)
        return false;

    for (size_t i = 0; i < length; i++) {
        char c = char(start[i]);
        JS_ASSERT(('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
        cstr[i] = c;
    }
    cstr[length] = 0;

    char *estr;
    int err = 0;
    *dp = js_strtod_harder(JS_THREAD_DATA(cx)->dtoaState, cstr, &estr, &err);
    if (err == JS_DTOA_ENOMEM) {
        JS_ReportOutOfMemory(cx);
        cx->free(cstr);
        return false;
    }
    if (err == JS_DTOA_ERANGE && *dp == HUGE_VAL)
        *dp = js_PositiveInfinity;
    cx->free(cstr);
    return true;
}

class BinaryDigitReader
{
    const int base;      /* Base of number; must be a power of 2 */
    int digit;           /* Current digit value in radix given by base */
    int digitMask;       /* Mask to extract the next bit from digit */
    const jschar *start; /* Pointer to the remaining digits */
    const jschar *end;   /* Pointer to first non-digit */

  public:
    BinaryDigitReader(int base, const jschar *start, const jschar *end)
      : base(base), digit(0), digitMask(0), start(start), end(end)
    {
    }

    /* Return the next binary digit from the number, or -1 if done. */
    int nextDigit() {
        if (digitMask == 0) {
            if (start == end)
                return -1;

            int c = *start++;
            JS_ASSERT(('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
            if ('0' <= c && c <= '9')
                digit = c - '0';
            else if ('a' <= c && c <= 'z')
                digit = c - 'a' + 10;
            else
                digit = c - 'A' + 10;
            digitMask = base >> 1;
        }

        int bit = (digit & digitMask) != 0;
        digitMask >>= 1;
        return bit;
    }
};

/*
 * The fast result might also have been inaccurate for power-of-two bases. This
 * happens if the addition in value * 2 + digit causes a round-down to an even
 * least significant mantissa bit when the first dropped bit is a one.  If any
 * of the following digits in the number (which haven't been added in yet) are
 * nonzero, then the correct action would have been to round up instead of
 * down.  An example occurs when reading the number 0x1000000000000081, which
 * rounds to 0x1000000000000000 instead of 0x1000000000000100.
 */
jsdouble
ComputeAccurateBinaryBaseInteger(JSContext *cx, const jschar *start, const jschar *end, int base)
{
    BinaryDigitReader bdr(base, start, end);

    /* Skip leading zeroes. */
    int bit;
    do {
        bit = bdr.nextDigit();
    } while (bit == 0);

    JS_ASSERT(bit == 1); // guaranteed by GetPrefixInteger

    /* Gather the 53 significant bits (including the leading 1). */
    jsdouble value = 1.0;
    for (int j = 52; j > 0; j--) {
        bit = bdr.nextDigit();
        if (bit < 0)
            return value;
        value = value * 2 + bit;
    }

    /* bit2 is the 54th bit (the first dropped from the mantissa). */
    int bit2 = bdr.nextDigit();
    if (bit2 >= 0) {
        jsdouble factor = 2.0;
        int sticky = 0;  /* sticky is 1 if any bit beyond the 54th is 1 */
        int bit3;

        while ((bit3 = bdr.nextDigit()) >= 0) {
            sticky |= bit3;
            factor *= 2;
        }
        value += bit2 & (bit | sticky);
        value *= factor;
    }

    return value;
}

} // namespace

namespace js {

bool
GetPrefixInteger(JSContext *cx, const jschar *start, const jschar *end, int base,
                 const jschar **endp, jsdouble *dp)
{
    JS_ASSERT(start <= end);
    JS_ASSERT(2 <= base && base <= 36);

    const jschar *s = start;
    jsdouble d = 0.0;
    for (; s < end; s++) {
        int digit;
        jschar c = *s;
        if ('0' <= c && c <= '9')
            digit = c - '0';
        else if ('a' <= c && c <= 'z')
            digit = c - 'a' + 10;
        else if ('A' <= c && c <= 'Z')
            digit = c - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        d = d * base + digit;
    }

    *endp = s;
    *dp = d;

    /* If we haven't reached the limit of integer precision, we're done. */
    if (d < DOUBLE_INTEGRAL_PRECISION_LIMIT)
        return true;

    /*
     * Otherwise compute the correct integer from the prefix of valid digits
     * if we're computing for base ten or a power of two.  Don't worry about
     * other bases; see 15.1.2.2 step 13.
     */
    if (base == 10)
        return ComputeAccurateDecimalInteger(cx, start, s, dp);
    if ((base & (base - 1)) == 0)
        *dp = ComputeAccurateBinaryBaseInteger(cx, start, s, base);

    return true;
}

} // namespace js

static JSBool
num_isNaN(JSContext *cx, uintN argc, Value *vp)
{
    if (argc == 0) {
        vp->setBoolean(true);
        return JS_TRUE;
    }
    jsdouble x;
    if (!ValueToNumber(cx, vp[2], &x))
        return false;
    vp->setBoolean(JSDOUBLE_IS_NaN(x));
    return JS_TRUE;
}

static JSBool
num_isFinite(JSContext *cx, uintN argc, Value *vp)
{
    if (argc == 0) {
        vp->setBoolean(false);
        return JS_TRUE;
    }
    jsdouble x;
    if (!ValueToNumber(cx, vp[2], &x))
        return JS_FALSE;
    vp->setBoolean(JSDOUBLE_IS_FINITE(x));
    return JS_TRUE;
}

static JSBool
num_parseFloat(JSContext *cx, uintN argc, Value *vp)
{
    JSString *str;
    jsdouble d;
    const jschar *bp, *end, *ep;

    if (argc == 0) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    str = js_ValueToString(cx, vp[2]);
    if (!str)
        return JS_FALSE;
    str->getCharsAndEnd(bp, end);
    if (!js_strtod(cx, bp, end, &ep, &d))
        return JS_FALSE;
    if (ep == bp) {
        vp->setDouble(js_NaN);
        return JS_TRUE;
    }
    vp->setNumber(d);
    return JS_TRUE;
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

namespace {

bool
ParseIntStringHelper(JSContext *cx, const jschar *ws, const jschar *end, int maybeRadix,
                     bool stripPrefix, jsdouble *dp)
{
    JS_ASSERT(maybeRadix == 0 || (2 <= maybeRadix && maybeRadix <= 36));
    JS_ASSERT(ws <= end);

    const jschar *s = js_SkipWhiteSpace(ws, end);
    JS_ASSERT(ws <= s);
    JS_ASSERT(s <= end);

    /* 15.1.2.2 steps 3-4. */
    bool negative = (s != end && s[0] == '-');

    /* 15.1.2.2 step 5. */
    if (s != end && (s[0] == '-' || s[0] == '+'))
        s++;

    /* 15.1.2.2 step 9. */
    int radix = maybeRadix;
    if (radix == 0) {
        if (end - s >= 2 && s[0] == '0' && (s[1] != 'x' && s[1] != 'X')) {
            /*
             * Non-standard: ES5 requires that parseInt interpret leading-zero
             * strings not starting with "0x" or "0X" as decimal (absent an
             * explicitly specified non-zero radix), but we continue to
             * interpret such strings as octal, as per ES3 and web practice.
             */
            radix = 8;
        } else {
            radix = 10;
        }
    }

    /* 15.1.2.2 step 10. */
    if (stripPrefix) {
        if (end - s >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            s += 2;
            radix = 16;
        }
    }

    /* 15.1.2.2 steps 11-14. */
    const jschar *actualEnd;
    if (!GetPrefixInteger(cx, s, end, radix, &actualEnd, dp))
        return false;
    if (s == actualEnd)
        *dp = js_NaN;
    else if (negative)
        *dp = -*dp;
    return true;
}

jsdouble
ParseIntDoubleHelper(jsdouble d)
{
    if (!JSDOUBLE_IS_FINITE(d))
        return js_NaN;
    if (d > 0)
        return floor(d);
    if (d < 0)
        return -floor(-d);
    return 0;
}

} // namespace

/* See ECMA 15.1.2.2. */
static JSBool
num_parseInt(JSContext *cx, uintN argc, Value *vp)
{
    /* Fast paths and exceptional cases. */
    if (argc == 0) {
        vp->setDouble(js_NaN);
        return true;
    }

    if (argc == 1 || (vp[3].isInt32() && (vp[3].toInt32() == 0 || vp[3].toInt32() == 10))) {
        if (vp[2].isInt32()) {
            *vp = vp[2];
            return true;
        }
        if (vp[2].isDouble()) {
            vp->setDouble(ParseIntDoubleHelper(vp[2].toDouble()));
            return true;
        }
    }

    /* Step 1. */
    JSString *inputString = js_ValueToString(cx, vp[2]);
    if (!inputString)
        return false;
    vp[2].setString(inputString);

    /* 15.1.2.2 steps 6-8. */
    bool stripPrefix = true;
    int32_t radix = 0;
    if (argc > 1) {
        if (!ValueToECMAInt32(cx, vp[3], &radix))
            return false;
        if (radix != 0) {
            if (radix < 2 || radix > 36) {
                vp->setDouble(js_NaN);
                return true;
            }
            if (radix != 16)
                stripPrefix = false;
        }
    }

    /* Steps 2-5, 9-14. */
    const jschar *ws, *end;
    inputString->getCharsAndEnd(ws, end);

    jsdouble number;
    if (!ParseIntStringHelper(cx, ws, end, radix, stripPrefix, &number))
        return false;

    /* Step 15. */
    vp->setNumber(number);
    return true;
}

#ifdef JS_TRACER
static jsdouble FASTCALL
ParseInt(JSContext* cx, JSString* str)
{
    const jschar *start, *end;
    str->getCharsAndEnd(start, end);

    jsdouble d;
    if (!ParseIntStringHelper(cx, start, end, 0, true, &d)) {
        SetBuiltinError(cx);
        return js_NaN;
    }
    return d;
}

static jsdouble FASTCALL
ParseIntDouble(jsdouble d)
{
    return ParseIntDoubleHelper(d);
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
    (2, (static, DOUBLE_FAIL, ParseInt, CONTEXT, STRING,1, nanojit::ACCSET_NONE)),
    (1, (static, DOUBLE, ParseIntDouble, DOUBLE,        1, nanojit::ACCSET_NONE)))

JS_DEFINE_TRCINFO_1(num_parseFloat,
    (2, (static, DOUBLE, ParseFloat, CONTEXT, STRING,   1, nanojit::ACCSET_NONE)))

#endif /* JS_TRACER */

static JSFunctionSpec number_functions[] = {
    JS_FN(js_isNaN_str,         num_isNaN,           1,0),
    JS_FN(js_isFinite_str,      num_isFinite,        1,0),
    JS_TN(js_parseFloat_str,    num_parseFloat,      1,0, &num_parseFloat_trcinfo),
    JS_TN(js_parseInt_str,      num_parseInt,        2,0, &num_parseInt_trcinfo),
    JS_FS_END
};

Class js_NumberClass = {
    js_Number_str,
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_HAS_CACHED_PROTO(JSProto_Number),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

static JSBool
Number(JSContext *cx, uintN argc, Value *vp)
{
    /* Sample JS_CALLEE before clobbering. */
    bool isConstructing = IsConstructing(vp);

    if (argc > 0) {
        if (!ValueToNumber(cx, &vp[2]))
            return false;
        vp[0] = vp[2];
    } else {
        vp[0].setInt32(0);
    }

    if (!isConstructing)
        return true;
    
    JSObject *obj = NewBuiltinClassInstance(cx, &js_NumberClass);
    if (!obj)
        return false;
    obj->setPrimitiveThis(vp[0]);
    vp->setObject(*obj);
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
num_toSource(JSContext *cx, uintN argc, Value *vp)
{
    double d;
    if (!GetPrimitiveThis(cx, vp, &d))
        return false;

    ToCStringBuf cbuf;
    char *numStr = NumberToCString(cx, &cbuf, d);
    if (!numStr) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    char buf[64];
    JS_snprintf(buf, sizeof buf, "(new %s(%s))", js_NumberClass.name, numStr);
    JSString *str = js_NewStringCopyZ(cx, buf);
    if (!str)
        return false;
    vp->setString(str);
    return true;
}
#endif

ToCStringBuf::ToCStringBuf() :dbuf(NULL)
{
    JS_STATIC_ASSERT(sbufSize >= DTOSTR_STANDARD_BUFFER_SIZE);
}

ToCStringBuf::~ToCStringBuf()
{
    if (dbuf)
        js_free(dbuf);
}

/* Returns a non-NULL pointer to inside cbuf.  */
static char *
IntToCString(ToCStringBuf *cbuf, jsint i, jsint base = 10)
{
    char *cp;
    jsuint u;

    u = (i < 0) ? -i : i;

    cp = cbuf->sbuf + cbuf->sbufSize;   /* one past last buffer cell */
    *--cp = '\0';                       /* null terminate the string to be */

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

    JS_ASSERT(cp >= cbuf->sbuf);
    return cp;
}

static JSString * JS_FASTCALL
js_NumberToStringWithBase(JSContext *cx, jsdouble d, jsint base);

static JSBool
num_toString(JSContext *cx, uintN argc, Value *vp)
{
    double d;
    if (!GetPrimitiveThis(cx, vp, &d))
        return false;

    int32_t base = 10;
    if (argc != 0 && !vp[2].isUndefined()) {
        if (!ValueToECMAInt32(cx, vp[2], &base))
            return JS_FALSE;

        if (base < 2 || base > 36) {
            ToCStringBuf cbuf;
            char *numStr = IntToCString(&cbuf, base);   /* convert the base itself to a string */
            JS_ASSERT(numStr);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_RADIX,
                                 numStr);
            return JS_FALSE;
        }
    }
    JSString *str = js_NumberToStringWithBase(cx, d, base);
    if (!str) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    vp->setString(str);
    return JS_TRUE;
}

static JSBool
num_toLocaleString(JSContext *cx, uintN argc, Value *vp)
{
    size_t thousandsLength, decimalLength;
    const char *numGrouping, *tmpGroup;
    JSRuntime *rt;
    JSString *str;
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
    JS_ASSERT(vp->isString());
    num = js_GetStringBytes(cx, vp->toString());
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
        return cx->localeCallbacks->localeToUnicode(cx, buf, Jsvalify(vp));

    str = JS_NewString(cx, buf, size);
    if (!str) {
        cx->free(buf);
        return JS_FALSE;
    }

    vp->setString(str);
    return JS_TRUE;
}

static JSBool
num_valueOf(JSContext *cx, uintN argc, Value *vp)
{
    double d;
    if (!GetPrimitiveThis(cx, vp, &d))
        return false;

    vp->setNumber(d);
    return true;
}


#define MAX_PRECISION 100

static JSBool
num_to(JSContext *cx, JSDToStrMode zeroArgMode, JSDToStrMode oneArgMode,
       jsint precisionMin, jsint precisionMax, jsint precisionOffset,
       uintN argc, Value *vp)
{
    /* Use MAX_PRECISION+1 because precisionOffset can be 1. */
    char buf[DTOSTR_VARIABLE_BUFFER_SIZE(MAX_PRECISION+1)];
    char *numStr;

    double d;
    if (!GetPrimitiveThis(cx, vp, &d))
        return false;

    double precision;
    if (argc == 0) {
        precision = 0.0;
        oneArgMode = zeroArgMode;
    } else {
        if (!ValueToNumber(cx, vp[2], &precision))
            return JS_FALSE;
        precision = js_DoubleToInteger(precision);
        if (precision < precisionMin || precision > precisionMax) {
            ToCStringBuf cbuf;
            numStr = IntToCString(&cbuf, jsint(precision));
            JS_ASSERT(numStr);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PRECISION_RANGE, numStr);
            return JS_FALSE;
        }
    }

    numStr = js_dtostr(JS_THREAD_DATA(cx)->dtoaState, buf, sizeof buf,
                       oneArgMode, (jsint)precision + precisionOffset, d);
    if (!numStr) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    JSString *str = js_NewStringCopyZ(cx, numStr);
    if (!str)
        return JS_FALSE;
    vp->setString(str);
    return JS_TRUE;
}

/*
 * In the following three implementations, we allow a larger range of precision
 * than ECMA requires; this is permitted by ECMA-262.
 */
static JSBool
num_toFixed(JSContext *cx, uintN argc, Value *vp)
{
    return num_to(cx, DTOSTR_FIXED, DTOSTR_FIXED, -20, MAX_PRECISION, 0,
                  argc, vp);
}

static JSBool
num_toExponential(JSContext *cx, uintN argc, Value *vp)
{
    return num_to(cx, DTOSTR_STANDARD_EXPONENTIAL, DTOSTR_EXPONENTIAL, 0, MAX_PRECISION, 1,
                  argc, vp);
}

static JSBool
num_toPrecision(JSContext *cx, uintN argc, Value *vp)
{
    if (argc == 0 || vp[2].isUndefined())
        return num_toString(cx, 0, vp);
    return num_to(cx, DTOSTR_STANDARD, DTOSTR_PRECISION, 1, MAX_PRECISION, 0,
                  argc, vp);
}

#ifdef JS_TRACER

JS_DEFINE_TRCINFO_2(num_toString,
    (2, (extern, STRING_RETRY, js_NumberToString,         CONTEXT, THIS_DOUBLE,
         1, nanojit::ACCSET_NONE)),
    (3, (static, STRING_RETRY, js_NumberToStringWithBase, CONTEXT, THIS_DOUBLE, INT32,
         1, nanojit::ACCSET_NONE)))

#endif /* JS_TRACER */

static JSFunctionSpec number_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,       num_toSource,          0, JSFUN_PRIMITIVE_THIS),
#endif
    JS_TN(js_toString_str,       num_toString,          1, JSFUN_PRIMITIVE_THIS, &num_toString_trcinfo),
    JS_FN(js_toLocaleString_str, num_toLocaleString,    0, JSFUN_PRIMITIVE_THIS),
    JS_FN(js_valueOf_str,        num_valueOf,           0, JSFUN_PRIMITIVE_THIS),
    JS_FN(js_toJSON_str,         num_valueOf,           0, JSFUN_PRIMITIVE_THIS),
    JS_FN("toFixed",             num_toFixed,           1, JSFUN_PRIMITIVE_THIS),
    JS_FN("toExponential",       num_toExponential,     1, JSFUN_PRIMITIVE_THIS),
    JS_FN("toPrecision",         num_toPrecision,       1, JSFUN_PRIMITIVE_THIS),
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

#if (defined __GNUC__ && defined __i386__) || \
    (defined __SUNPRO_CC && defined __i386)

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
    JSRuntime *rt = cx->runtime;

    FIX_FPU();

    jsdpun u;
    u.s.hi = JSDOUBLE_HI32_NAN;
    u.s.lo = JSDOUBLE_LO32_NAN;
    number_constants[NC_NaN].dval = js_NaN = u.d;
    rt->NaNValue.setDouble(u.d);

    u.s.hi = JSDOUBLE_HI32_EXPMASK;
    u.s.lo = 0x00000000;
    number_constants[NC_POSITIVE_INFINITY].dval = js_PositiveInfinity = u.d;
    rt->positiveInfinityValue.setDouble(u.d);

    u.s.hi = JSDOUBLE_HI32_SIGNBIT | JSDOUBLE_HI32_EXPMASK;
    u.s.lo = 0x00000000;
    number_constants[NC_NEGATIVE_INFINITY].dval = js_NegativeInfinity = u.d;
    rt->negativeInfinityValue.setDouble(u.d);

    u.s.hi = 0;
    u.s.lo = 1;
    number_constants[NC_MIN_VALUE].dval = u.d;

#ifndef HAVE_LOCALECONV
    rt->thousandsSeparator = JS_strdup(cx, "'");
    rt->decimalSeparator = JS_strdup(cx, ".");
    rt->numGrouping = JS_strdup(cx, "\3\0");
#else
    struct lconv *locale = localeconv();
    rt->thousandsSeparator =
        JS_strdup(cx, locale->thousands_sep ? locale->thousands_sep : "'");
    rt->decimalSeparator =
        JS_strdup(cx, locale->decimal_point ? locale->decimal_point : ".");
    rt->numGrouping =
        JS_strdup(cx, locale->grouping ? locale->grouping : "\3\0");
#endif

    return rt->thousandsSeparator && rt->decimalSeparator && rt->numGrouping;
}

void
js_FinishRuntimeNumberState(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

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

    proto = js_InitClass(cx, obj, NULL, &js_NumberClass, Number, 1,
                         NULL, number_methods, NULL, NULL);
    if (!proto || !(ctor = JS_GetConstructor(cx, proto)))
        return NULL;
    proto->setPrimitiveThis(Int32Value(0));
    if (!JS_DefineConstDoubles(cx, ctor, number_constants))
        return NULL;

    /* ECMA 15.1.1.1 */
    rt = cx->runtime;
    if (!JS_DefineProperty(cx, obj, js_NaN_str, Jsvalify(rt->NaNValue),
                           JS_PropertyStub, JS_PropertyStub,
                           JSPROP_PERMANENT | JSPROP_READONLY)) {
        return NULL;
    }

    /* ECMA 15.1.1.2 */
    if (!JS_DefineProperty(cx, obj, js_Infinity_str, Jsvalify(rt->positiveInfinityValue),
                           JS_PropertyStub, JS_PropertyStub,
                           JSPROP_PERMANENT | JSPROP_READONLY)) {
        return NULL;
    }
    return proto;
}

namespace v8 {
namespace internal {
extern char* DoubleToCString(double v, char* buffer, int buflen);
}
}

namespace js {

static char *
FracNumberToCString(JSContext *cx, ToCStringBuf *cbuf, jsdouble d, jsint base = 10)
{
#ifdef DEBUG
    {
        int32_t _;
        JS_ASSERT(!JSDOUBLE_IS_INT32(d, &_));
    }
#endif

    char* numStr;
    if (base == 10) {
        /*
         * This is V8's implementation of the algorithm described in the
         * following paper:
         *
         *   Printing floating-point numbers quickly and accurately with integers. 
         *   Florian Loitsch, PLDI 2010.
         *
         * It fails on a small number of cases, whereupon we fall back to
         * js_dtostr() (which uses David Gay's dtoa).
         */
        numStr = v8::internal::DoubleToCString(d, cbuf->sbuf, cbuf->sbufSize);
        if (!numStr)
            numStr = js_dtostr(JS_THREAD_DATA(cx)->dtoaState, cbuf->sbuf, cbuf->sbufSize,
                               DTOSTR_STANDARD, 0, d);
    } else {
        numStr = cbuf->dbuf = js_dtobasestr(JS_THREAD_DATA(cx)->dtoaState, base, d);
    }
    return numStr;
}

char *
NumberToCString(JSContext *cx, ToCStringBuf *cbuf, jsdouble d, jsint base/* = 10*/)
{
    int32_t i;
    return (JSDOUBLE_IS_INT32(d, &i))
           ? IntToCString(cbuf, i, base)
           : FracNumberToCString(cx, cbuf, d, base);
}

}

JSString * JS_FASTCALL
js_IntToString(JSContext *cx, jsint i)
{
    if (jsuint(i) < INT_STRING_LIMIT)
        return JSString::intString(i);

    ToCStringBuf cbuf;
    return js_NewStringCopyZ(cx, IntToCString(&cbuf, i));
}

static JSString * JS_FASTCALL
js_NumberToStringWithBase(JSContext *cx, jsdouble d, jsint base)
{
    ToCStringBuf cbuf;
    char *numStr;
    JSString *s;
    JSThreadData *data;

    /*
     * Caller is responsible for error reporting. When called from trace,
     * returning NULL here will cause us to fall of trace and then retry
     * from the interpreter (which will report the error).
     */
    if (base < 2 || base > 36)
        return NULL;

    int32_t i;
    if (JSDOUBLE_IS_INT32(d, &i)) {
        if (base == 10 && jsuint(i) < INT_STRING_LIMIT)
            return JSString::intString(i);
        if (jsuint(i) < jsuint(base)) {
            if (i < 10)
                return JSString::intString(i);
            return JSString::unitString(jschar('a' + i - 10));
        }

        data = JS_THREAD_DATA(cx);
        if (data->dtoaCache.s && data->dtoaCache.base == base && data->dtoaCache.d == d)
            return data->dtoaCache.s;

        numStr = IntToCString(&cbuf, i, base);
        JS_ASSERT(!cbuf.dbuf && numStr >= cbuf.sbuf && numStr < cbuf.sbuf + cbuf.sbufSize);
    } else {
        data = JS_THREAD_DATA(cx);
        if (data->dtoaCache.s && data->dtoaCache.base == base && data->dtoaCache.d == d)
            return data->dtoaCache.s;

        numStr = FracNumberToCString(cx, &cbuf, d, base);
        if (!numStr) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        JS_ASSERT_IF(base == 10,
                     !cbuf.dbuf && numStr >= cbuf.sbuf && numStr < cbuf.sbuf + cbuf.sbufSize);
        JS_ASSERT_IF(base != 10,
                     cbuf.dbuf && cbuf.dbuf == numStr);
    }

    s = js_NewStringCopyZ(cx, numStr);

    data->dtoaCache.base = base;
    data->dtoaCache.d = d;
    data->dtoaCache.s = s;

    return s;
}

JSString * JS_FASTCALL
js_NumberToString(JSContext *cx, jsdouble d)
{
    return js_NumberToStringWithBase(cx, d, 10);
}

JSBool JS_FASTCALL
js_NumberValueToCharBuffer(JSContext *cx, const Value &v, JSCharBuffer &cb)
{
    /* Convert to C-string. */
    ToCStringBuf cbuf;
    const char *cstr;
    if (v.isInt32()) {
        cstr = IntToCString(&cbuf, v.toInt32());
    } else {
        cstr = NumberToCString(cx, &cbuf, v.toDouble());
        if (!cstr) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }

    /*
     * Inflate to jschar string.  The input C-string characters are < 127, so
     * even if jschars are UTF-8, all chars should map to one jschar.
     */
    size_t cstrlen = strlen(cstr);
    JS_ASSERT(!cbuf.dbuf && cstrlen < cbuf.sbufSize);
    size_t sizeBefore = cb.length();
    if (!cb.growByUninitialized(cstrlen))
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

namespace js {

bool
ValueToNumberSlow(JSContext *cx, Value v, double *out)
{
    JS_ASSERT(!v.isNumber());
    goto skip_int_double;
    for (;;) {
        if (v.isNumber()) {
            *out = v.toNumber();
            return true;
        }
      skip_int_double:
        if (v.isString()) {
            jsdouble d = StringToNumberType<jsdouble>(cx, v.toString());
            if (JSDOUBLE_IS_NaN(d))
                break;
            *out = d;
            return true;
        }
        if (v.isBoolean()) {
            if (v.toBoolean()) {
                *out = 1.0;
                return true;
            }
            *out = 0.0;
            return true;
        }
        if (v.isNull()) {
            *out = 0.0;
            return true;
        }
        if (v.isUndefined())
            break;

        JS_ASSERT(v.isObject());
        if (!DefaultValue(cx, &v.toObject(), JSTYPE_NUMBER, &v))
            return false;
        if (v.isObject())
            break;
    }

    *out = js_NaN;
    return true;
}

bool
ValueToECMAInt32Slow(JSContext *cx, const Value &v, int32_t *out)
{
    JS_ASSERT(!v.isInt32());
    jsdouble d;
    if (v.isDouble()) {
        d = v.toDouble();
    } else {
        if (!ValueToNumberSlow(cx, v, &d))
            return false;
    }
    *out = js_DoubleToECMAInt32(d);
    return true;
}

bool
ValueToECMAUint32Slow(JSContext *cx, const Value &v, uint32_t *out)
{
    JS_ASSERT(!v.isInt32());
    jsdouble d;
    if (v.isDouble()) {
        d = v.toDouble();
    } else {
        if (!ValueToNumberSlow(cx, v, &d))
            return false;
    }
    *out = js_DoubleToECMAUint32(d);
    return true;
}

}  /* namespace js */

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
     * result does not fit int Value, it will be stored as a negative double.
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

namespace js {

bool
ValueToInt32Slow(JSContext *cx, const Value &v, int32_t *out)
{
    JS_ASSERT(!v.isInt32());
    jsdouble d;
    if (v.isDouble()) {
        d = v.toDouble();
    } else if (!ValueToNumberSlow(cx, v, &d)) {
        return false;
    }

    if (JSDOUBLE_IS_NaN(d) || d <= -2147483649.0 || 2147483648.0 <= d) {
        js_ReportValueError(cx, JSMSG_CANT_CONVERT,
                            JSDVG_SEARCH_STACK, v, NULL);
        return false;
    }
    *out = (int32) floor(d + 0.5);  /* Round to nearest */
    return true;
}

bool
ValueToUint16Slow(JSContext *cx, const Value &v, uint16_t *out)
{
    JS_ASSERT(!v.isInt32());
    jsdouble d;
    if (v.isDouble()) {
        d = v.toDouble();
    } else if (!ValueToNumberSlow(cx, v, &d)) {
        return false;
    }

    if (d == 0 || !JSDOUBLE_IS_FINITE(d)) {
        *out = 0;
        return true;
    }

    uint16 u = (uint16) d;
    if ((jsdouble)u == d) {
        *out = u;
        return true;
    }

    bool neg = (d < 0);
    d = floor(neg ? -d : d);
    d = neg ? -d : d;
    jsuint m = JS_BIT(16);
    d = fmod(d, (double) m);
    if (d < 0)
        d += m;
    *out = (uint16_t) d;
    return true;
}

}  /* namespace js */

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
        d = js_strtod_harder(JS_THREAD_DATA(cx)->dtoaState, cstr, &estr, &err);
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
