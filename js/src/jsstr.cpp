/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * JS string type implementation.
 *
 * In order to avoid unnecessary js_LockGCThing/js_UnlockGCThing calls, these
 * native methods store strings (possibly newborn) converted from their 'this'
 * parameter and arguments on the stack: 'this' conversions at argv[-1], arg
 * conversions at their index (argv[0], argv[1]).  This is a legitimate method
 * of rooting things that might lose their newborn root due to subsequent GC
 * allocations in the same native method.
 */
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsfun.h"      /* for JS_ARGS_LENGTH_MAX */
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscope.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "jsbit.h"
#include "jsvector.h"
#include "jsversion.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsstrinlines.h"

using namespace js;

#define JSSTRDEP_RECURSION_LIMIT        100

JS_STATIC_ASSERT(size_t(JSString::MAX_LENGTH) <= size_t(JSVAL_INT_MAX));
JS_STATIC_ASSERT(INT_FITS_IN_JSVAL(JSString::MAX_LENGTH));

static size_t
MinimizeDependentStrings(JSString *str, int level, JSString **basep)
{
    JSString *base;
    size_t start, length;

    JS_ASSERT(str->isDependent());
    base = str->dependentBase();
    start = str->dependentStart();
    if (base->isDependent()) {
        if (level < JSSTRDEP_RECURSION_LIMIT) {
            start += MinimizeDependentStrings(base, level + 1, &base);
        } else {
            do {
                start += base->dependentStart();
                base = base->dependentBase();
            } while (base->isDependent());
        }
        length = str->dependentLength();
        str->initDependent(base, start, length);
    }
    *basep = base;
    return start;
}

jschar *
js_GetDependentStringChars(JSString *str)
{
    size_t start;
    JSString *base;

    start = MinimizeDependentStrings(str, 0, &base);
    JS_ASSERT(start < base->flatLength());
    return base->flatChars() + start;
}

const jschar *
js_GetStringChars(JSContext *cx, JSString *str)
{
    if (!js_MakeStringImmutable(cx, str))
        return NULL;
    return str->flatChars();
}

JSString * JS_FASTCALL
js_ConcatStrings(JSContext *cx, JSString *left, JSString *right)
{
    size_t rn, ln, lrdist, n;
    jschar *ls, *s;
    const jschar *rs;
    JSString *ldep;             /* non-null if left should become dependent */
    JSString *str;

    right->getCharsAndLength(rs, rn);
    if (rn == 0)
        return left;

    left->getCharsAndLength(const_cast<const jschar *&>(ls), ln);
    if (ln == 0)
        return right;

    if (!left->isMutable()) {
        /* We must copy if left does not own a buffer to realloc. */
        s = (jschar *) cx->malloc((ln + rn + 1) * sizeof(jschar));
        if (!s)
            return NULL;
        js_strncpy(s, ls, ln);
        ldep = NULL;
    } else {
        /* We can realloc left's space and make it depend on our result. */
        JS_ASSERT(left->isFlat());
        s = (jschar *) cx->realloc(ls, (ln + rn + 1) * sizeof(jschar));
        if (!s)
            return NULL;

        /* Take care: right could depend on left! */
        lrdist = (size_t)(rs - ls);
        if (lrdist < ln)
            rs = s + lrdist;
        left->mChars = ls = s;
        ldep = left;
    }

    js_strncpy(s + ln, rs, rn);
    n = ln + rn;
    s[n] = 0;

    str = js_NewString(cx, s, n);
    if (!str) {
        /* Out of memory: clean up any space we (re-)allocated. */
        if (!ldep) {
            cx->free(s);
        } else {
            s = (jschar *) cx->realloc(ls, (ln + 1) * sizeof(jschar));
            if (s)
                left->mChars = s;
        }
    } else {
        str->flatSetMutable();

        /* Morph left into a dependent string if we realloc'd its buffer. */
        if (ldep) {
            ldep->initDependent(str, 0, ln);
#ifdef DEBUG
            {
                JSRuntime *rt = cx->runtime;
                JS_RUNTIME_METER(rt, liveDependentStrings);
                JS_RUNTIME_METER(rt, totalDependentStrings);
                JS_LOCK_RUNTIME_VOID(rt,
                    (rt->strdepLengthSum += (double)ln,
                     rt->strdepLengthSquaredSum += (double)ln * (double)ln));
            }
#endif
        }
    }

    return str;
}

const jschar *
js_UndependString(JSContext *cx, JSString *str)
{
    size_t n, size;
    jschar *s;

    if (str->isDependent()) {
        n = str->dependentLength();
        size = (n + 1) * sizeof(jschar);
        s = (jschar *) cx->malloc(size);
        if (!s)
            return NULL;

        js_strncpy(s, str->dependentChars(), n);
        s[n] = 0;
        str->initFlat(s, n);

#ifdef DEBUG
        {
            JSRuntime *rt = cx->runtime;
            JS_RUNTIME_UNMETER(rt, liveDependentStrings);
            JS_RUNTIME_UNMETER(rt, totalDependentStrings);
            JS_LOCK_RUNTIME_VOID(rt,
                (rt->strdepLengthSum -= (double)n,
                 rt->strdepLengthSquaredSum -= (double)n * (double)n));
        }
#endif
    }

    return str->flatChars();
}

JSBool
js_MakeStringImmutable(JSContext *cx, JSString *str)
{
    if (str->isDependent() && !js_UndependString(cx, str)) {
        JS_RUNTIME_METER(cx->runtime, badUndependStrings);
        return JS_FALSE;
    }
    str->flatClearMutable();
    return JS_TRUE;
}

static JSString *
ArgToRootedString(JSContext *cx, uintN argc, jsval *vp, uintN arg)
{
    if (arg >= argc)
        return ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    vp += 2 + arg;

    if (!JSVAL_IS_PRIMITIVE(*vp) && !DefaultValue(cx, JSVAL_TO_OBJECT(*vp), JSTYPE_STRING, vp))
        return NULL;

    JSString *str;
    if (JSVAL_IS_STRING(*vp)) {
        str = JSVAL_TO_STRING(*vp);
    } else if (JSVAL_IS_BOOLEAN(*vp)) {
        str = ATOM_TO_STRING(cx->runtime->atomState.booleanAtoms[
                                  JSVAL_TO_BOOLEAN(*vp)? 1 : 0]);
    } else if (JSVAL_IS_NULL(*vp)) {
        str = ATOM_TO_STRING(cx->runtime->atomState.nullAtom);
    } else if (JSVAL_IS_VOID(*vp)) {
        str = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    }
    else {
        if (JSVAL_IS_INT(*vp)) {
            str = js_NumberToString(cx, JSVAL_TO_INT(*vp));
        } else {
            JS_ASSERT(JSVAL_IS_DOUBLE(*vp));
            str = js_NumberToString(cx, *JSVAL_TO_DOUBLE(*vp));
        }
        if (str)
            *vp = STRING_TO_JSVAL(str);
    }
    return str;
}

/*
 * Forward declarations for URI encode/decode and helper routines
 */
static JSBool
str_decodeURI(JSContext *cx, uintN argc, jsval *vp);

static JSBool
str_decodeURI_Component(JSContext *cx, uintN argc, jsval *vp);

static JSBool
str_encodeURI(JSContext *cx, uintN argc, jsval *vp);

static JSBool
str_encodeURI_Component(JSContext *cx, uintN argc, jsval *vp);

static const uint32 OVERLONG_UTF8 = UINT32_MAX;

static uint32
Utf8ToOneUcs4Char(const uint8 *utf8Buffer, int utf8Length);

/*
 * Contributions from the String class to the set of methods defined for the
 * global object.  escape and unescape used to be defined in the Mocha library,
 * but as ECMA decided to spec them, they've been moved to the core engine
 * and made ECMA-compliant.  (Incomplete escapes are interpreted as literal
 * characters by unescape.)
 */

/*
 * Stuff to emulate the old libmocha escape, which took a second argument
 * giving the type of escape to perform.  Retained for compatibility, and
 * copied here to avoid reliance on net.h, mkparse.c/NET_EscapeBytes.
 */

#define URL_XALPHAS     ((uint8) 1)
#define URL_XPALPHAS    ((uint8) 2)
#define URL_PATH        ((uint8) 4)

static const uint8 urlCharType[256] =
/*      Bit 0           xalpha          -- the alphas
 *      Bit 1           xpalpha         -- as xalpha but
 *                             converts spaces to plus and plus to %20
 *      Bit 2 ...       path            -- as xalphas but doesn't escape '/'
 */
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 0x */
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 1x */
         0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,       /* 2x   !"#$%&'()*+,-./  */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,       /* 3x  0123456789:;<=>?  */
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 4x  @ABCDEFGHIJKLMNO  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,       /* 5X  PQRSTUVWXYZ[\]^_  */
         0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 6x  `abcdefghijklmno  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,       /* 7X  pqrstuvwxyz{\}~  DEL */
         0, };

/* This matches the ECMA escape set when mask is 7 (default.) */

#define IS_OK(C, mask) (urlCharType[((uint8) (C))] & (mask))

/* See ECMA-262 Edition 3 B.2.1 */
JSBool
js_str_escape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    size_t i, ni, length, newlength;
    const jschar *chars;
    jschar *newchars;
    jschar ch;
    jsint mask;
    jsdouble d;
    const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
    if (argc > 1) {
        if (!ValueToNumber(cx, argv[1], &d))
            return JS_FALSE;
        if (!JSDOUBLE_IS_FINITE(d) ||
            (mask = (jsint)d) != d ||
            mask & ~(URL_XALPHAS | URL_XPALPHAS | URL_PATH))
        {
            char numBuf[12];
            JS_snprintf(numBuf, sizeof numBuf, "%lx", (unsigned long) mask);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_STRING_MASK, numBuf);
            return JS_FALSE;
        }
    }

    str = ArgToRootedString(cx, argc, argv - 2, 0);
    if (!str)
        return JS_FALSE;

    str->getCharsAndLength(chars, length);
    newlength = length;

    /* Take a first pass and see how big the result string will need to be. */
    for (i = 0; i < length; i++) {
        if ((ch = chars[i]) < 128 && IS_OK(ch, mask))
            continue;
        if (ch < 256) {
            if (mask == URL_XPALPHAS && ch == ' ')
                continue;   /* The character will be encoded as '+' */
            newlength += 2; /* The character will be encoded as %XX */
        } else {
            newlength += 5; /* The character will be encoded as %uXXXX */
        }

        /*
         * This overflow test works because newlength is incremented by at
         * most 5 on each iteration.
         */
        if (newlength < length) {
            js_ReportAllocationOverflow(cx);
            return JS_FALSE;
        }
    }

    if (newlength >= ~(size_t)0 / sizeof(jschar)) {
        js_ReportAllocationOverflow(cx);
        return JS_FALSE;
    }

    newchars = (jschar *) cx->malloc((newlength + 1) * sizeof(jschar));
    if (!newchars)
        return JS_FALSE;
    for (i = 0, ni = 0; i < length; i++) {
        if ((ch = chars[i]) < 128 && IS_OK(ch, mask)) {
            newchars[ni++] = ch;
        } else if (ch < 256) {
            if (mask == URL_XPALPHAS && ch == ' ') {
                newchars[ni++] = '+'; /* convert spaces to pluses */
            } else {
                newchars[ni++] = '%';
                newchars[ni++] = digits[ch >> 4];
                newchars[ni++] = digits[ch & 0xF];
            }
        } else {
            newchars[ni++] = '%';
            newchars[ni++] = 'u';
            newchars[ni++] = digits[ch >> 12];
            newchars[ni++] = digits[(ch & 0xF00) >> 8];
            newchars[ni++] = digits[(ch & 0xF0) >> 4];
            newchars[ni++] = digits[ch & 0xF];
        }
    }
    JS_ASSERT(ni == newlength);
    newchars[newlength] = 0;

    str = js_NewString(cx, newchars, newlength);
    if (!str) {
        cx->free(newchars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#undef IS_OK

static JSBool
str_escape(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    return obj && js_str_escape(cx, obj, argc, vp + 2, vp);
}

/* See ECMA-262 Edition 3 B.2.2 */
static JSBool
str_unescape(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    size_t i, ni, length;
    const jschar *chars;
    jschar *newchars;
    jschar ch;

    str = ArgToRootedString(cx, argc, vp, 0);
    if (!str)
        return JS_FALSE;

    str->getCharsAndLength(chars, length);

    /* Don't bother allocating less space for the new string. */
    newchars = (jschar *) cx->malloc((length + 1) * sizeof(jschar));
    if (!newchars)
        return JS_FALSE;
    ni = i = 0;
    while (i < length) {
        ch = chars[i++];
        if (ch == '%') {
            if (i + 1 < length &&
                JS7_ISHEX(chars[i]) && JS7_ISHEX(chars[i + 1]))
            {
                ch = JS7_UNHEX(chars[i]) * 16 + JS7_UNHEX(chars[i + 1]);
                i += 2;
            } else if (i + 4 < length && chars[i] == 'u' &&
                       JS7_ISHEX(chars[i + 1]) && JS7_ISHEX(chars[i + 2]) &&
                       JS7_ISHEX(chars[i + 3]) && JS7_ISHEX(chars[i + 4]))
            {
                ch = (((((JS7_UNHEX(chars[i + 1]) << 4)
                        + JS7_UNHEX(chars[i + 2])) << 4)
                      + JS7_UNHEX(chars[i + 3])) << 4)
                    + JS7_UNHEX(chars[i + 4]);
                i += 5;
            }
        }
        newchars[ni++] = ch;
    }
    newchars[ni] = 0;

    str = js_NewString(cx, newchars, ni);
    if (!str) {
        cx->free(newchars);
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#if JS_HAS_UNEVAL
static JSBool
str_uneval(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    str = js_ValueToSource(cx, argc != 0 ? vp[2] : JSVAL_VOID);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif

const char js_escape_str[] = "escape";
const char js_unescape_str[] = "unescape";
#if JS_HAS_UNEVAL
const char js_uneval_str[] = "uneval";
#endif
const char js_decodeURI_str[] = "decodeURI";
const char js_encodeURI_str[] = "encodeURI";
const char js_decodeURIComponent_str[] = "decodeURIComponent";
const char js_encodeURIComponent_str[] = "encodeURIComponent";

static JSFunctionSpec string_functions[] = {
    JS_FN(js_escape_str,             str_escape,                1,0),
    JS_FN(js_unescape_str,           str_unescape,              1,0),
#if JS_HAS_UNEVAL
    JS_FN(js_uneval_str,             str_uneval,                1,0),
#endif
    JS_FN(js_decodeURI_str,          str_decodeURI,             1,0),
    JS_FN(js_encodeURI_str,          str_encodeURI,             1,0),
    JS_FN(js_decodeURIComponent_str, str_decodeURI_Component,   1,0),
    JS_FN(js_encodeURIComponent_str, str_encodeURI_Component,   1,0),

    JS_FS_END
};

jschar      js_empty_ucstr[]  = {0};
JSSubString js_EmptySubString = {0, js_empty_ucstr};

static JSBool
str_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsval v;
    JSString *str;

    if (id == ATOM_KEY(cx->runtime->atomState.lengthAtom)) {
        if (obj->getClass() == &js_StringClass) {
            /* Follow ECMA-262 by fetching intrinsic length of our string. */
            v = obj->getPrimitiveThis();
            JS_ASSERT(JSVAL_IS_STRING(v));
            str = JSVAL_TO_STRING(v);
        } else {
            /* Preserve compatibility: convert obj to a string primitive. */
            str = js_ValueToString(cx, OBJECT_TO_JSVAL(obj));
            if (!str)
                return JS_FALSE;
        }

        *vp = INT_TO_JSVAL((jsint) str->length());
    }

    return JS_TRUE;
}

#define STRING_ELEMENT_ATTRS (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)

static JSBool
str_enumerate(JSContext *cx, JSObject *obj)
{
    jsval v;
    JSString *str, *str1;
    size_t i, length;

    v = obj->getPrimitiveThis();
    JS_ASSERT(JSVAL_IS_STRING(v));
    str = JSVAL_TO_STRING(v);

    length = str->length();
    for (i = 0; i < length; i++) {
        str1 = js_NewDependentString(cx, str, i, 1);
        if (!str1)
            return JS_FALSE;
        if (!obj->defineProperty(cx, INT_TO_JSID(i), STRING_TO_JSVAL(str1),
                                 JS_PropertyStub, JS_PropertyStub,
                                 STRING_ELEMENT_ATTRS)) {
            return JS_FALSE;
        }
    }

    return obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom),
                               JSVAL_VOID, NULL, NULL,
                               JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_SHARED);
}

static JSBool
str_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
            JSObject **objp)
{
    jsval v;
    JSString *str, *str1;
    jsint slot;

    if (!JSVAL_IS_INT(id) || (flags & JSRESOLVE_ASSIGNING))
        return JS_TRUE;

    v = obj->getPrimitiveThis();
    JS_ASSERT(JSVAL_IS_STRING(v));
    str = JSVAL_TO_STRING(v);

    slot = JSVAL_TO_INT(id);
    if ((size_t)slot < str->length()) {
        str1 = JSString::getUnitString(cx, str, size_t(slot));
        if (!str1)
            return JS_FALSE;
        if (!obj->defineProperty(cx, INT_TO_JSID(slot), STRING_TO_JSVAL(str1), NULL, NULL,
                                 STRING_ELEMENT_ATTRS)) {
            return JS_FALSE;
        }
        *objp = obj;
    }
    return JS_TRUE;
}

JSClass js_StringClass = {
    js_String_str,
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_String),
    JS_PropertyStub,   JS_PropertyStub,   str_getProperty,   JS_PropertyStub,
    str_enumerate, (JSResolveOp)str_resolve, JS_ConvertStub, NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#define NORMALIZE_THIS(cx,vp,str)                                             \
    JS_BEGIN_MACRO                                                            \
        if (JSVAL_IS_STRING(vp[1])) {                                         \
            str = JSVAL_TO_STRING(vp[1]);                                     \
        } else {                                                              \
            str = NormalizeThis(cx, vp);                                      \
            if (!str)                                                         \
                return JS_FALSE;                                              \
        }                                                                     \
    JS_END_MACRO

static JSString *
NormalizeThis(JSContext *cx, jsval *vp)
{
    JSString *str;

    if (JSVAL_IS_NULL(vp[1]) && JSVAL_IS_NULL(JS_THIS(cx, vp)))
        return NULL;

    /*
     * js_GetPrimitiveThis seems to do a bunch of work (like calls to
     * JS_THIS_OBJECT) which we don't need in the common case (where
     * vp[1] is a String object) here.  Note that vp[1] can still be a
     * primitive value at this point.
     */
    if (!JSVAL_IS_PRIMITIVE(vp[1])) {
        JSObject *obj = JSVAL_TO_OBJECT(vp[1]);
        if (obj->getClass() == &js_StringClass) {
            vp[1] = obj->getPrimitiveThis();
            return JSVAL_TO_STRING(vp[1]);
        }
    }

    str = js_ValueToString(cx, vp[1]);
    if (!str)
        return NULL;
    vp[1] = STRING_TO_JSVAL(str);
    return str;
}

#if JS_HAS_TOSOURCE

/*
 * String.prototype.quote is generic (as are most string methods), unlike
 * toSource, toString, and valueOf.
 */
static JSBool
str_quote(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    NORMALIZE_THIS(cx, vp, str);
    str = js_QuoteString(cx, str, '"');
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
str_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    jsval v;
    JSString *str;
    size_t i, j, k, n;
    char buf[16];
    const jschar *s;
    jschar *t;

    if (!js_GetPrimitiveThis(cx, vp, &js_StringClass, &v))
        return JS_FALSE;
    JS_ASSERT(JSVAL_IS_STRING(v));
    str = js_QuoteString(cx, JSVAL_TO_STRING(v), '"');
    if (!str)
        return JS_FALSE;
    j = JS_snprintf(buf, sizeof buf, "(new %s(", js_StringClass.name);
    str->getCharsAndLength(s, k);
    n = j + k + 2;
    t = (jschar *) cx->malloc((n + 1) * sizeof(jschar));
    if (!t)
        return JS_FALSE;
    for (i = 0; i < j; i++)
        t[i] = buf[i];
    for (j = 0; j < k; i++, j++)
        t[i] = s[j];
    t[i++] = ')';
    t[i++] = ')';
    t[i] = 0;
    str = js_NewString(cx, t, n);
    if (!str) {
        cx->free(t);
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#endif /* JS_HAS_TOSOURCE */

JSBool
js_str_toString(JSContext *cx, uintN argc, jsval *vp)
{
    return js_GetPrimitiveThis(cx, vp, &js_StringClass, vp);
}

/*
 * Java-like string native methods.
 */

static JSString *
SubstringTail(JSContext *cx, JSString *str, jsdouble length, jsdouble begin, jsdouble end)
{
    if (begin < 0)
        begin = 0;
    else if (begin > length)
        begin = length;

    if (end < 0)
        end = 0;
    else if (end > length)
        end = length;
    if (end < begin) {
        /* ECMA emulates old JDK1.0 java.lang.String.substring. */
        jsdouble tmp = begin;
        begin = end;
        end = tmp;
    }

    return js_NewDependentString(cx, str, (size_t)begin, (size_t)(end - begin));
}

static JSBool
str_substring(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    jsdouble d;
    jsdouble length, begin, end;

    NORMALIZE_THIS(cx, vp, str);
    if (argc != 0) {
        if (!ValueToNumber(cx, vp[2], &d))
            return JS_FALSE;
        length = str->length();
        begin = js_DoubleToInteger(d);
        if (argc == 1) {
            end = length;
        } else {
            if (!ValueToNumber(cx, vp[3], &d))
                return JS_FALSE;
            end = js_DoubleToInteger(d);
        }

        str = SubstringTail(cx, str, length, begin, end);
        if (!str)
            return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#ifdef JS_TRACER
static JSString* FASTCALL
String_p_toString(JSContext* cx, JSObject* obj)
{
    if (!JS_InstanceOf(cx, obj, &js_StringClass, NULL))
        return NULL;
    jsval v = obj->getPrimitiveThis();
    JS_ASSERT(JSVAL_IS_STRING(v));
    return JSVAL_TO_STRING(v);
}
#endif

JSString* JS_FASTCALL
js_toLowerCase(JSContext *cx, JSString *str)
{
    size_t i, n;
    const jschar *s;
    jschar *news;

    str->getCharsAndLength(s, n);
    news = (jschar *) cx->malloc((n + 1) * sizeof(jschar));
    if (!news)
        return NULL;
    for (i = 0; i < n; i++)
        news[i] = JS_TOLOWER(s[i]);
    news[n] = 0;
    str = js_NewString(cx, news, n);
    if (!str) {
        cx->free(news);
        return NULL;
    }
    return str;
}

static JSBool
str_toLowerCase(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    NORMALIZE_THIS(cx, vp, str);
    str = js_toLowerCase(cx, str);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
str_toLocaleLowerCase(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    /*
     * Forcefully ignore the first (or any) argument and return toLowerCase(),
     * ECMA has reserved that argument, presumably for defining the locale.
     */
    if (cx->localeCallbacks && cx->localeCallbacks->localeToLowerCase) {
        NORMALIZE_THIS(cx, vp, str);
        return cx->localeCallbacks->localeToLowerCase(cx, str, vp);
    }
    return str_toLowerCase(cx, 0, vp);
}

JSString* JS_FASTCALL
js_toUpperCase(JSContext *cx, JSString *str)
{
    size_t i, n;
    const jschar *s;
    jschar *news;

    str->getCharsAndLength(s, n);
    news = (jschar *) cx->malloc((n + 1) * sizeof(jschar));
    if (!news)
        return NULL;
    for (i = 0; i < n; i++)
        news[i] = JS_TOUPPER(s[i]);
    news[n] = 0;
    str = js_NewString(cx, news, n);
    if (!str) {
        cx->free(news);
        return NULL;
    }
    return str;
}

static JSBool
str_toUpperCase(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    NORMALIZE_THIS(cx, vp, str);
    str = js_toUpperCase(cx, str);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
str_toLocaleUpperCase(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    /*
     * Forcefully ignore the first (or any) argument and return toUpperCase(),
     * ECMA has reserved that argument, presumably for defining the locale.
     */
    if (cx->localeCallbacks && cx->localeCallbacks->localeToUpperCase) {
        NORMALIZE_THIS(cx, vp, str);
        return cx->localeCallbacks->localeToUpperCase(cx, str, vp);
    }
    return str_toUpperCase(cx, 0, vp);
}

static JSBool
str_localeCompare(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str, *thatStr;

    NORMALIZE_THIS(cx, vp, str);
    if (argc == 0) {
        *vp = JSVAL_ZERO;
    } else {
        thatStr = js_ValueToString(cx, vp[2]);
        if (!thatStr)
            return JS_FALSE;
        if (cx->localeCallbacks && cx->localeCallbacks->localeCompare) {
            vp[2] = STRING_TO_JSVAL(thatStr);
            return cx->localeCallbacks->localeCompare(cx, str, thatStr, vp);
        }
        *vp = INT_TO_JSVAL(js_CompareStrings(str, thatStr));
    }
    return JS_TRUE;
}

static JSBool
str_charAt(JSContext *cx, uintN argc, jsval *vp)
{
    jsval t;
    JSString *str;
    jsint i;
    jsdouble d;

    t = vp[1];
    if (JSVAL_IS_STRING(t) && argc != 0 && JSVAL_IS_INT(vp[2])) {
        str = JSVAL_TO_STRING(t);
        i = JSVAL_TO_INT(vp[2]);
        if ((size_t)i >= str->length())
            goto out_of_range;
    } else {
        NORMALIZE_THIS(cx, vp, str);

        if (argc == 0) {
            d = 0.0;
        } else {
            if (!ValueToNumber(cx, vp[2], &d))
                return JS_FALSE;
            d = js_DoubleToInteger(d);
        }

        if (d < 0 || str->length() <= d)
            goto out_of_range;
        i = (jsint) d;
    }

    str = JSString::getUnitString(cx, str, size_t(i));
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;

out_of_range:
    *vp = JS_GetEmptyStringValue(cx);
    return JS_TRUE;
}

static JSBool
str_charCodeAt(JSContext *cx, uintN argc, jsval *vp)
{
    jsval t;
    JSString *str;
    jsint i;
    jsdouble d;

    t = vp[1];
    if (JSVAL_IS_STRING(t) && argc != 0 && JSVAL_IS_INT(vp[2])) {
        str = JSVAL_TO_STRING(t);
        i = JSVAL_TO_INT(vp[2]);
        if ((size_t)i >= str->length())
            goto out_of_range;
    } else {
        NORMALIZE_THIS(cx, vp, str);

        if (argc == 0) {
            d = 0.0;
        } else {
            if (!ValueToNumber(cx, vp[2], &d))
                return JS_FALSE;
            d = js_DoubleToInteger(d);
        }

        if (d < 0 || str->length() <= d)
            goto out_of_range;
        i = (jsint) d;
    }

    *vp = INT_TO_JSVAL(str->chars()[i]);
    return JS_TRUE;

out_of_range:
    *vp = JS_GetNaNValue(cx);
    return JS_TRUE;
}

#ifdef JS_TRACER

jsdouble FASTCALL
js_String_p_charCodeAt(JSString* str, jsdouble d)
{
    d = js_DoubleToInteger(d);
    if (d < 0 || (int32)str->length() <= d)
        return js_NaN;
    return jsdouble(str->chars()[jsuint(d)]);
}

int32 FASTCALL
js_String_p_charCodeAt_int_int(JSString* str, jsint i)
{
    if (i < 0 || (int32)str->length() <= i)
        return 0;
    return str->chars()[i];
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_String_p_charCodeAt_int_int,  STRING, INT32, 1,
                     nanojit::ACC_NONE)

int32 FASTCALL
js_String_p_charCodeAt_double_int(JSString* str, double d)
{
    d = js_DoubleToInteger(d);
    if (d < 0 || (int32)str->length() <= d)
        return 0;
    return str->chars()[jsuint(d)];
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_String_p_charCodeAt_double_int,  STRING, DOUBLE, 1,
                     nanojit::ACC_NONE)

jsdouble FASTCALL
js_String_p_charCodeAt0(JSString* str)
{
    if ((int32)str->length() == 0)
        return js_NaN;
    return jsdouble(str->chars()[0]);
}

/*
 * The FuncFilter replaces the generic double version of charCodeAt with the
 * integer fast path if appropriate.
 */
int32 FASTCALL
js_String_p_charCodeAt0_int(JSString* str)
{
    if ((int32)str->length() == 0)
        return 0;
    return str->chars()[0];
}
JS_DEFINE_CALLINFO_1(extern, INT32, js_String_p_charCodeAt0_int, STRING, 1, nanojit::ACC_NONE)
#endif

jsint
js_BoyerMooreHorspool(const jschar *text, jsuint textlen,
                      const jschar *pat, jsuint patlen)
{
    uint8 skip[sBMHCharSetSize];

    JS_ASSERT(0 < patlen && patlen <= sBMHPatLenMax);
    for (jsuint i = 0; i < sBMHCharSetSize; i++)
        skip[i] = (uint8)patlen;
    jsuint m = patlen - 1;
    for (jsuint i = 0; i < m; i++) {
        jschar c = pat[i];
        if (c >= sBMHCharSetSize)
            return sBMHBadPattern;
        skip[c] = (uint8)(m - i);
    }
    jschar c;
    for (jsuint k = m;
         k < textlen;
         k += ((c = text[k]) >= sBMHCharSetSize) ? patlen : skip[c]) {
        for (jsuint i = k, j = m; ; i--, j--) {
            if (text[i] != pat[j])
                break;
            if (j == 0)
                return static_cast<jsint>(i);  /* safe: max string size */
        }
    }
    return -1;
}

namespace {

struct MemCmp {
    typedef jsuint Extent;
    static JS_ALWAYS_INLINE Extent computeExtent(const jschar *, jsuint patlen) {
        return (patlen - 1) * sizeof(jschar);
    }
    static JS_ALWAYS_INLINE bool match(const jschar *p, const jschar *t, Extent extent) {
        return memcmp(p, t, extent) == 0;
    }
};

struct ManualCmp {
    typedef const jschar *Extent;
    static JS_ALWAYS_INLINE Extent computeExtent(const jschar *pat, jsuint patlen) {
        return pat + patlen;
    }
    static JS_ALWAYS_INLINE bool match(const jschar *p, const jschar *t, Extent extent) {
        for (; p != extent; ++p, ++t) {
            if (*p != *t)
                return false;
        }
        return true;
    }
};

}

template <class InnerMatch>
static jsint
UnrolledMatch(const jschar *text, jsuint textlen, const jschar *pat, jsuint patlen)
{
    JS_ASSERT(patlen > 0 && textlen > 0);
    const jschar *textend = text + textlen - (patlen - 1);
    const jschar p0 = *pat;
    const jschar *const patNext = pat + 1;
    const typename InnerMatch::Extent extent = InnerMatch::computeExtent(pat, patlen);
    uint8 fixup;

    const jschar *t = text;
    switch ((textend - t) & 7) {
      case 0: if (*t++ == p0) { fixup = 8; goto match; }
      case 7: if (*t++ == p0) { fixup = 7; goto match; }
      case 6: if (*t++ == p0) { fixup = 6; goto match; }
      case 5: if (*t++ == p0) { fixup = 5; goto match; }
      case 4: if (*t++ == p0) { fixup = 4; goto match; }
      case 3: if (*t++ == p0) { fixup = 3; goto match; }
      case 2: if (*t++ == p0) { fixup = 2; goto match; }
      case 1: if (*t++ == p0) { fixup = 1; goto match; }
    }
    while (t != textend) {
      if (t[0] == p0) { t += 1; fixup = 8; goto match; }
      if (t[1] == p0) { t += 2; fixup = 7; goto match; }
      if (t[2] == p0) { t += 3; fixup = 6; goto match; }
      if (t[3] == p0) { t += 4; fixup = 5; goto match; }
      if (t[4] == p0) { t += 5; fixup = 4; goto match; }
      if (t[5] == p0) { t += 6; fixup = 3; goto match; }
      if (t[6] == p0) { t += 7; fixup = 2; goto match; }
      if (t[7] == p0) { t += 8; fixup = 1; goto match; }
        t += 8;
        continue;
        do {
            if (*t++ == p0) {
              match:
                if (!InnerMatch::match(patNext, t, extent))
                    goto failed_match;
                return t - text - 1;
            }
          failed_match:;
        } while (--fixup > 0);
    }
    return -1;
}

static JS_ALWAYS_INLINE jsint
StringMatch(const jschar *text, jsuint textlen,
            const jschar *pat, jsuint patlen)
{
    if (patlen == 0)
        return 0;
    if (textlen < patlen)
        return -1;

#if defined(__i386__) || defined(_M_IX86) || defined(__i386)
    /*
     * Given enough registers, the unrolled loop below is faster than the
     * following loop. 32-bit x86 does not have enough registers.
     */
    if (patlen == 1) {
        const jschar p0 = *pat;
        for (const jschar *c = text, *end = text + textlen; c != end; ++c) {
            if (*c == p0)
                return c - text;
        }
        return -1;
    }
#endif

    /*
     * If the text or pattern string is short, BMH will be more expensive than
     * the basic linear scan due to initialization cost and a more complex loop
     * body. While the correct threshold is input-dependent, we can make a few
     * conservative observations:
     *  - When |textlen| is "big enough", the initialization time will be
     *    proportionally small, so the worst-case slowdown is minimized.
     *  - When |patlen| is "too small", even the best case for BMH will be
     *    slower than a simple scan for large |textlen| due to the more complex
     *    loop body of BMH.
     * From this, the values for "big enough" and "too small" are determined
     * empirically. See bug 526348.
     */
    if (textlen >= 512 && patlen >= 11 && patlen <= sBMHPatLenMax) {
        jsint index = js_BoyerMooreHorspool(text, textlen, pat, patlen);
        if (index != sBMHBadPattern)
            return index;
    }

    /*
     * For big patterns with large potential overlap we want the SIMD-optimized
     * speed of memcmp. For small patterns, a simple loop is faster.
     *
     * FIXME: Linux memcmp performance is sad and the manual loop is faster.
     */
    return
#if !defined(__linux__)
           patlen > 128 ? UnrolledMatch<MemCmp>(text, textlen, pat, patlen)
                        :
#endif
                          UnrolledMatch<ManualCmp>(text, textlen, pat, patlen);
}

static JSBool
str_indexOf(JSContext *cx, uintN argc, jsval *vp)
{

    JSString *str;
    NORMALIZE_THIS(cx, vp, str);

    JSString *patstr = ArgToRootedString(cx, argc, vp, 0);
    if (!patstr)
        return JS_FALSE;

    const jschar *text = str->chars();
    jsuint textlen = str->length();
    const jschar *pat = patstr->chars();
    jsuint patlen = patstr->length();

    jsuint start;
    if (argc > 1) {
        jsval indexVal = vp[3];
        if (JSVAL_IS_INT(indexVal)) {
            jsint i = JSVAL_TO_INT(indexVal);
            if (i <= 0) {
                start = 0;
            } else if (jsuint(i) > textlen) {
                start = 0;
                textlen = 0;
            } else {
                start = i;
                text += start;
                textlen -= start;
            }
        } else {
            jsdouble d;
            if (!ValueToNumber(cx, vp[3], &d))
                return JS_FALSE;
            d = js_DoubleToInteger(d);
            if (d <= 0) {
                start = 0;
            } else if (d > textlen) {
                start = 0;
                textlen = 0;
            } else {
                start = (jsint)d;
                text += start;
                textlen -= start;
            }
        }
    } else {
        start = 0;
    }

    jsint match = StringMatch(text, textlen, pat, patlen);
    *vp = INT_TO_JSVAL((match == -1) ? -1 : start + match);
    return true;
}

static JSBool
str_lastIndexOf(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str, *str2;
    const jschar *text, *pat;
    jsint i, j, textlen, patlen;
    jsdouble d;

    NORMALIZE_THIS(cx, vp, str);
    text = str->chars();
    textlen = (jsint) str->length();

    if (argc != 0 && JSVAL_IS_STRING(vp[2])) {
        str2 = JSVAL_TO_STRING(vp[2]);
    } else {
        str2 = ArgToRootedString(cx, argc, vp, 0);
        if (!str2)
            return JS_FALSE;
    }
    pat = str2->chars();
    patlen = (jsint) str2->length();

    i = textlen - patlen; // Start searching here
    if (i < 0) {
        *vp = INT_TO_JSVAL(-1);
        return JS_TRUE;
    }

    if (argc > 1) {
        if (JSVAL_IS_INT(vp[3])) {
            j = JSVAL_TO_INT(vp[3]);
            if (j <= 0)
                i = 0;
            else if (j < i)
                i = j;
        } else {
            if (!ValueToNumber(cx, vp[3], &d))
                return JS_FALSE;
            if (!JSDOUBLE_IS_NaN(d)) {
                d = js_DoubleToInteger(d);
                if (d <= 0)
                    i = 0;
                else if (d < i)
                    i = (jsint)d;
            }
        }
    }

    if (patlen == 0) {
        *vp = INT_TO_JSVAL(i);
        return JS_TRUE;
    }

    const jschar *t = text + i;
    const jschar *textend = text - 1;
    const jschar p0 = *pat;
    const jschar *patNext = pat + 1;
    const jschar *patEnd = pat + patlen;

    for (; t != textend; --t) {
        if (*t == p0) {
            const jschar *t1 = t + 1;
            for (const jschar *p1 = patNext; p1 != patEnd; ++p1, ++t1) {
                if (*t1 != *p1)
                    goto break_continue;
            }
            *vp = INT_TO_JSVAL(t - text);
            return JS_TRUE;
        }
      break_continue:;
    }

    *vp = INT_TO_JSVAL(-1);
    return JS_TRUE;
}

static JSBool
js_TrimString(JSContext *cx, jsval *vp, JSBool trimLeft, JSBool trimRight)
{
    JSString *str;
    const jschar *chars;
    size_t length, begin, end;

    NORMALIZE_THIS(cx, vp, str);
    str->getCharsAndLength(chars, length);
    begin = 0;
    end = length;

    if (trimLeft) {
        while (begin < length && JS_ISSPACE(chars[begin]))
            ++begin;
    }

    if (trimRight) {
        while (end > begin && JS_ISSPACE(chars[end-1]))
            --end;
    }

    str = js_NewDependentString(cx, str, begin, end - begin);
    if (!str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
str_trim(JSContext *cx, uintN argc, jsval *vp)
{
    return js_TrimString(cx, vp, JS_TRUE, JS_TRUE);
}

static JSBool
str_trimLeft(JSContext *cx, uintN argc, jsval *vp)
{
    return js_TrimString(cx, vp, JS_TRUE, JS_FALSE);
}

static JSBool
str_trimRight(JSContext *cx, uintN argc, jsval *vp)
{
    return js_TrimString(cx, vp, JS_FALSE, JS_TRUE);
}

/*
 * Perl-inspired string functions.
 */

/*
 * RegExpGuard factors logic out of String regexp operations. After each
 * operation completes, RegExpGuard data members become available, according to
 * the comments below.
 *
 * Notes on parameters to RegExpGuard member functions:
 *  - 'optarg' indicates in which argument position RegExp flags will be found,
 *    if present. This is a Mozilla extension and not part of any ECMA spec.
 *  - 'flat' indicates that the given pattern string will not be interpreted as
 *    a regular expression, hence regexp meta-characters are ignored.
 */
class RegExpGuard
{
    RegExpGuard(const RegExpGuard &);
    void operator=(const RegExpGuard &);

    JSContext *mCx;
    JSObject *mReobj;
    JSRegExp *mRe;

  public:
    RegExpGuard(JSContext *cx) : mCx(cx), mRe(NULL) {}

    ~RegExpGuard() {
        if (mRe)
            DROP_REGEXP(mCx, mRe);
    }

    JSContext* cx() const { return mCx; }

    /* init must succeed in order to call tryFlatMatch or normalizeRegExp. */
    bool
    init(uintN argc, jsval *vp)
    {
        jsval patval = vp[2];
        if (argc != 0 && VALUE_IS_REGEXP(mCx, patval)) {
            mReobj = JSVAL_TO_OBJECT(patval);
            mRe = (JSRegExp *) mReobj->getPrivate();
            HOLD_REGEXP(mCx, mRe);
        } else {
            patstr = ArgToRootedString(mCx, argc, vp, 0);
            if (!patstr)
                return false;
        }
        return true;
    }

    /*
     * Upper bound on the number of characters we are willing to potentially
     * waste on searching for RegExp meta-characters.
     */
    static const size_t sMaxFlatPatLen = 256;

    /*
     * Attempt to match |patstr| with |textstr|.  Return false if flat matching
     * could not be used.
     */
    bool
    tryFlatMatch(JSString *textstr, bool flat, uintN optarg, uintN argc)
    {
        if (mRe)
            return false;
        patstr->getCharsAndLength(pat, patlen);
        if (optarg < argc ||
            (!flat &&
             (patlen > sMaxFlatPatLen || js_ContainsRegExpMetaChars(pat, patlen)))) {
            return false;
        }
        textstr->getCharsAndLength(text, textlen);
        match = StringMatch(text, textlen, pat, patlen);
        return true;
    }

    /* Data available on successful return from |tryFlatMatch|. */
    JSString *patstr;
    const jschar *pat;
    size_t patlen;
    const jschar *text;
    size_t textlen;
    jsint match;

    /* If the pattern is not already a regular expression, make it so. */
    bool
    normalizeRegExp(bool flat, uintN optarg, uintN argc, jsval *vp)
    {
        /* If we don't have a RegExp, build RegExp from pattern string. */
        if (mRe)
            return true;
        JSString *opt;
        if (optarg < argc) {
            opt = js_ValueToString(mCx, vp[2 + optarg]);
            if (!opt)
                return false;
        } else {
            opt = NULL;
        }
        mRe = js_NewRegExpOpt(mCx, patstr, opt, flat);
        if (!mRe)
            return false;
        mReobj = NULL;
        return true;
    }

    /* Data available on successful return from |normalizeRegExp|. */
    JSObject *reobj() const { return mReobj; }  /* nullable */
    JSRegExp *re() const { return mRe; }        /* non-null */
};

/* js_ExecuteRegExp indicates success in two ways, based on the 'test' flag. */
static JS_ALWAYS_INLINE bool
Matched(bool test, jsval v)
{
    return test ? (v == JSVAL_TRUE) : !JSVAL_IS_NULL(v);
}

typedef bool (*DoMatchCallback)(JSContext *cx, size_t count, void *data);

/*
 * BitOR-ing these flags allows the DoMatch caller to control when how the
 * RegExp engine is called and when callbacks are fired.
 */
enum MatchControlFlags {
   TEST_GLOBAL_BIT         = 0x1, /* use RegExp.test for global regexps */
   TEST_SINGLE_BIT         = 0x2, /* use RegExp.test for non-global regexps */
   CALLBACK_ON_SINGLE_BIT  = 0x4, /* fire callback on non-global match */

   MATCH_ARGS    = TEST_GLOBAL_BIT,
   MATCHALL_ARGS = CALLBACK_ON_SINGLE_BIT,
   REPLACE_ARGS  = TEST_GLOBAL_BIT | TEST_SINGLE_BIT | CALLBACK_ON_SINGLE_BIT
};

/* Factor out looping and matching logic. */
static bool
DoMatch(JSContext *cx, jsval *vp, JSString *str, const RegExpGuard &g,
        DoMatchCallback callback, void *data, MatchControlFlags flags)
{
    if (g.re()->flags & JSREG_GLOB) {
        /* global matching ('g') */
        bool testGlobal = flags & TEST_GLOBAL_BIT;
        if (g.reobj())
            g.reobj()->zeroRegExpLastIndex();
        for (size_t count = 0, i = 0, length = str->length(); i <= length; ++count) {
            if (!js_ExecuteRegExp(cx, g.re(), str, &i, testGlobal, vp))
                return false;
            if (!Matched(testGlobal, *vp))
                break;
            if (!callback(cx, count, data))
                return false;
            if (cx->regExpStatics.lastMatch.length == 0)
                ++i;
        }
    } else {
        /* single match */
        bool testSingle = !!(flags & TEST_SINGLE_BIT),
             callbackOnSingle = !!(flags & CALLBACK_ON_SINGLE_BIT);
        size_t i = 0;
        if (!js_ExecuteRegExp(cx, g.re(), str, &i, testSingle, vp))
            return false;
        if (callbackOnSingle && Matched(testSingle, *vp) &&
            !callback(cx, 0, data)) {
            return false;
        }
    }
    return true;
}

/*
 * DoMatch will only callback on global matches, hence this function builds
 * only the "array of matches" returned by match on global regexps.
 */
static bool
MatchCallback(JSContext *cx, size_t count, void *p)
{
    JS_ASSERT(count <= JSVAL_INT_MAX);  /* by max string length */

    jsval &arrayval = *static_cast<jsval *>(p);
    JSObject *arrayobj = JSVAL_TO_OBJECT(arrayval);
    if (!arrayobj) {
        arrayobj = js_NewArrayObject(cx, 0, NULL);
        if (!arrayobj)
            return false;
        arrayval = OBJECT_TO_JSVAL(arrayobj);
    }

    JSString *str = cx->regExpStatics.input;
    JSSubString &match = cx->regExpStatics.lastMatch;
    ptrdiff_t off = match.chars - str->chars();
    JS_ASSERT(off >= 0 && size_t(off) <= str->length());
    JSString *matchstr = js_NewDependentString(cx, str, off, match.length);
    if (!matchstr)
        return false;

    jsval v = STRING_TO_JSVAL(matchstr);

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);
    return !!arrayobj->setProperty(cx, INT_TO_JSID(count), &v);
}

static bool
BuildFlatMatchArray(JSContext *cx, JSString *textstr, const RegExpGuard &g,
                    jsval *vp)
{
    if (g.match < 0) {
        *vp = JSVAL_NULL;
        return true;
    }

    /* For this non-global match, produce a RegExp.exec-style array. */
    JSObject *obj = js_NewSlowArrayObject(cx);
    if (!obj)
        return false;
    *vp = OBJECT_TO_JSVAL(obj);

    return obj->defineProperty(cx, INT_TO_JSID(0), STRING_TO_JSVAL(g.patstr)) &&
           obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.indexAtom),
                               INT_TO_JSVAL(g.match)) &&
           obj->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.inputAtom),
                               STRING_TO_JSVAL(textstr));
}

static JSBool
str_match(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    NORMALIZE_THIS(cx, vp, str);

    RegExpGuard g(cx);
    if (!g.init(argc, vp))
        return false;
    if (g.tryFlatMatch(str, false, 1, argc))
        return BuildFlatMatchArray(cx, str, g, vp);
    if (!g.normalizeRegExp(false, 1, argc, vp))
        return false;

    AutoValueRooter array(cx, JSVAL_NULL);
    if (!DoMatch(cx, vp, str, g, MatchCallback, array.addr(), MATCH_ARGS))
        return false;

    /* When not global, DoMatch will leave |RegEx.exec()| in *vp. */
    if (g.re()->flags & JSREG_GLOB)
        *vp = array.value();
    return true;
}

static JSBool
str_search(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    NORMALIZE_THIS(cx, vp, str);

    RegExpGuard g(cx);
    if (!g.init(argc, vp))
        return false;
    if (g.tryFlatMatch(str, false, 1, argc)) {
        *vp = INT_TO_JSVAL(g.match);
        return true;
    }
    if (!g.normalizeRegExp(false, 1, argc, vp))
        return false;

    size_t i = 0;
    if (!js_ExecuteRegExp(cx, g.re(), str, &i, true, vp))
        return false;

    if (*vp == JSVAL_TRUE)
        *vp = INT_TO_JSVAL(cx->regExpStatics.leftContext.length);
    else
        *vp = INT_TO_JSVAL(-1);
    return true;
}

struct ReplaceData
{
    ReplaceData(JSContext *cx)
     : g(cx), cb(cx)
    {}

    bool argsPushed() const {
        return args.getvp() != NULL;
    }

    JSString        *str;           /* 'this' parameter object as a string */
    RegExpGuard     g;              /* regexp parameter object and private data */
    JSObject        *lambda;        /* replacement function object or null */
    JSString        *repstr;        /* replacement string */
    jschar          *dollar;        /* null or pointer to first $ in repstr */
    jschar          *dollarEnd;     /* limit pointer for js_strchr_limit */
    jsint           index;          /* index in result of next replacement */
    jsint           leftIndex;      /* left context index in str->chars */
    JSSubString     dollarStr;      /* for "$$" InterpretDollar result */
    bool            calledBack;     /* record whether callback has been called */
    InvokeArgsGuard args;           /* arguments for lambda's js_Invoke call */
    JSCharBuffer    cb;             /* buffer built during DoMatch */
};

static JSSubString *
InterpretDollar(JSContext *cx, jschar *dp, jschar *ep, ReplaceData &rdata,
                size_t *skip)
{
    JSRegExpStatics *res;
    jschar dc, *cp;
    uintN num, tmp;

    JS_ASSERT(*dp == '$');

    /* If there is only a dollar, bail now */
    if (dp + 1 >= ep)
        return NULL;

    /* Interpret all Perl match-induced dollar variables. */
    res = &cx->regExpStatics;
    dc = dp[1];
    if (JS7_ISDEC(dc)) {
        /* ECMA-262 Edition 3: 1-9 or 01-99 */
        num = JS7_UNDEC(dc);
        if (num > res->parens.length())
            return NULL;

        cp = dp + 2;
        if (cp < ep && (dc = *cp, JS7_ISDEC(dc))) {
            tmp = 10 * num + JS7_UNDEC(dc);
            if (tmp <= res->parens.length()) {
                cp++;
                num = tmp;
            }
        }
        if (num == 0)
            return NULL;

        /* Adjust num from 1 $n-origin to 0 array-index-origin. */
        num--;
        *skip = cp - dp;
        return (num < res->parens.length()) ? &res->parens[num] : &js_EmptySubString;
    }

    *skip = 2;
    switch (dc) {
      case '$':
        rdata.dollarStr.chars = dp;
        rdata.dollarStr.length = 1;
        return &rdata.dollarStr;
      case '&':
        return &res->lastMatch;
      case '+':
        return &res->lastParen;
      case '`':
        return &res->leftContext;
      case '\'':
        return &res->rightContext;
    }
    return NULL;
}

static JS_ALWAYS_INLINE bool
PushRegExpSubstr(JSContext *cx, const JSSubString &sub, jsval *&sp)
{
    JSString *whole = cx->regExpStatics.input;
    size_t off = sub.chars - whole->chars();
    JSString *str = js_NewDependentString(cx, whole, off, sub.length);
    if (!str)
        return false;
    *sp++ = STRING_TO_JSVAL(str);
    return true;
}

class PreserveRegExpStatics {
    JSContext *cx;
    JSRegExpStatics save;

  public:
    PreserveRegExpStatics(JSContext *cx) : cx(cx), save(cx) {
        save.copy(cx->regExpStatics);
    }

    ~PreserveRegExpStatics() {
        cx->regExpStatics.copy(save);
    }
};

static bool
FindReplaceLength(JSContext *cx, ReplaceData &rdata, size_t *sizep)
{
    JSString *repstr;
    size_t replen, skip;
    jschar *dp, *ep;
    JSSubString *sub;
    JSObject *lambda;

    lambda = rdata.lambda;
    if (lambda) {
        LeaveTrace(cx);

        /*
         * In the lambda case, not only do we find the replacement string's
         * length, we compute repstr and return it via rdata for use within
         * DoReplace.  The lambda is called with arguments ($&, $1, $2, ...,
         * index, input), i.e., all the properties of a regexp match array.
         * For $&, etc., we must create string jsvals from cx->regExpStatics.
         * We grab up stack space to keep the newborn strings GC-rooted.
         */
        uintN p = rdata.g.re()->parenCount;
        uintN argc = 1 + p + 2;

        if (!rdata.argsPushed() && !cx->stack().pushInvokeArgs(cx, argc, rdata.args))
            return false;

        PreserveRegExpStatics save(cx);

        /* Push lambda and its 'this' parameter. */
        jsval *sp = rdata.args.getvp();
        *sp++ = OBJECT_TO_JSVAL(lambda);
        *sp++ = OBJECT_TO_JSVAL(lambda->getParent());

        /* Push $&, $1, $2, ... */
        if (!PushRegExpSubstr(cx, cx->regExpStatics.lastMatch, sp))
            return false;

        uintN i = 0;
        for (uintN n = cx->regExpStatics.parens.length(); i < n; i++) {
            if (!PushRegExpSubstr(cx, cx->regExpStatics.parens[i], sp))
                return false;
        }

        /* Make sure to push undefined for any unmatched parens. */
        for (; i < p; i++)
            *sp++ = JSVAL_VOID;

        /* Push match index and input string. */
        *sp++ = INT_TO_JSVAL((jsint)cx->regExpStatics.leftContext.length);
        *sp++ = STRING_TO_JSVAL(rdata.str);

        if (!js_Invoke(cx, rdata.args, 0))
            return false;

        /*
         * NB: we count on the newborn string root to hold any string
         * created by this js_ValueToString that would otherwise be GC-
         * able, until we use rdata.repstr in DoReplace.
         */
        repstr = js_ValueToString(cx, *rdata.args.getvp());
        if (!repstr)
            return false;

        rdata.repstr = repstr;
        *sizep = repstr->length();

        return true;
    }

    repstr = rdata.repstr;
    replen = repstr->length();
    for (dp = rdata.dollar, ep = rdata.dollarEnd; dp;
         dp = js_strchr_limit(dp, '$', ep)) {
        sub = InterpretDollar(cx, dp, ep, rdata, &skip);
        if (sub) {
            replen += sub->length - skip;
            dp += skip;
        }
        else
            dp++;
    }
    *sizep = replen;
    return true;
}

static void
DoReplace(JSContext *cx, ReplaceData &rdata, jschar *chars)
{
    JSString *repstr;
    jschar *bp, *cp, *dp, *ep;
    size_t len, skip;
    JSSubString *sub;

    repstr = rdata.repstr;
    bp = cp = repstr->chars();
    for (dp = rdata.dollar, ep = rdata.dollarEnd; dp;
         dp = js_strchr_limit(dp, '$', ep)) {
        len = dp - cp;
        js_strncpy(chars, cp, len);
        chars += len;
        cp = dp;
        sub = InterpretDollar(cx, dp, ep, rdata, &skip);
        if (sub) {
            len = sub->length;
            js_strncpy(chars, sub->chars, len);
            chars += len;
            cp += skip;
            dp += skip;
        } else {
            dp++;
        }
    }
    js_strncpy(chars, cp, repstr->length() - (cp - bp));
}

static bool
ReplaceCallback(JSContext *cx, size_t count, void *p)
{
    ReplaceData &rdata = *static_cast<ReplaceData *>(p);

    rdata.calledBack = true;
    JSString *str = rdata.str;
    size_t leftoff = rdata.leftIndex;
    const jschar *left = str->chars() + leftoff;
    size_t leftlen = cx->regExpStatics.lastMatch.chars - left;
    rdata.leftIndex = cx->regExpStatics.lastMatch.chars - str->chars();
    rdata.leftIndex += cx->regExpStatics.lastMatch.length;

    size_t replen = 0;  /* silence 'unused' warning */
    if (!FindReplaceLength(cx, rdata, &replen))
        return false;

    size_t growth = leftlen + replen;
    if (!rdata.cb.growBy(growth))
        return false;

    jschar *chars = rdata.cb.begin() + rdata.index;
    rdata.index += growth;
    js_strncpy(chars, left, leftlen);
    chars += leftlen;
    DoReplace(cx, rdata, chars);
    return true;
}

static bool
BuildFlatReplacement(JSContext *cx, JSString *textstr, JSString *repstr,
                     const RegExpGuard &g, jsval *vp)
{
    if (g.match == -1) {
        *vp = STRING_TO_JSVAL(textstr);
        return true;
    }

    const jschar *rep;
    size_t replen;
    repstr->getCharsAndLength(rep, replen);

    JSCharBuffer cb(cx);
    if (!cb.reserve(g.textlen - g.patlen + replen) ||
        !cb.append(g.text, static_cast<size_t>(g.match)) ||
        !cb.append(rep, replen) ||
        !cb.append(g.text + g.match + g.patlen, g.text + g.textlen)) {
        return false;
    }

    JSString *str = js_NewStringFromCharBuffer(cx, cb);
    if (!str)
        return false;
    *vp = STRING_TO_JSVAL(str);
    return true;
}

static JSBool
str_replace(JSContext *cx, uintN argc, jsval *vp)
{
    ReplaceData rdata(cx);
    NORMALIZE_THIS(cx, vp, rdata.str);

    /* Extract replacement string/function. */
    if (argc >= 2 && js_IsCallable(vp[3])) {
        rdata.lambda = JSVAL_TO_OBJECT(vp[3]);
        rdata.repstr = NULL;
        rdata.dollar = rdata.dollarEnd = NULL;
    } else {
        rdata.lambda = NULL;
        rdata.repstr = ArgToRootedString(cx, argc, vp, 1);
        if (!rdata.repstr)
            return false;

        /* We're about to store pointers into the middle of our string. */
        if (!js_MakeStringImmutable(cx, rdata.repstr))
            return false;
        rdata.dollarEnd = rdata.repstr->chars() + rdata.repstr->length();
        rdata.dollar = js_strchr_limit(rdata.repstr->chars(), '$',
                                       rdata.dollarEnd);
    }

    if (!rdata.g.init(argc, vp))
        return false;
    if (!rdata.dollar && !rdata.lambda &&
        rdata.g.tryFlatMatch(rdata.str, true, 2, argc)) {
        return BuildFlatReplacement(cx, rdata.str, rdata.repstr, rdata.g, vp);
    }
    if (!rdata.g.normalizeRegExp(true, 2, argc, vp))
        return false;

    rdata.index = 0;
    rdata.leftIndex = 0;
    rdata.calledBack = false;

    if (!DoMatch(cx, vp, rdata.str, rdata.g, ReplaceCallback, &rdata, REPLACE_ARGS))
        return false;

    if (!rdata.calledBack) {
        /* Didn't match, so the string is unmodified. */
        *vp = STRING_TO_JSVAL(rdata.str);
        return true;
    }

    JSSubString *sub = &cx->regExpStatics.rightContext;
    if (!rdata.cb.append(sub->chars, sub->length))
        return false;

    JSString *retstr = js_NewStringFromCharBuffer(cx, rdata.cb);
    if (!retstr)
        return false;

    *vp = STRING_TO_JSVAL(retstr);
    return true;
}

/*
 * Subroutine used by str_split to find the next split point in str, starting
 * at offset *ip and looking either for the separator substring given by sep, or
 * for the next re match.  In the re case, return the matched separator in *sep,
 * and the possibly updated offset in *ip.
 *
 * Return -2 on error, -1 on end of string, >= 0 for a valid index of the next
 * separator occurrence if found, or str->length if no separator is found.
 */
static jsint
find_split(JSContext *cx, JSString *str, JSRegExp *re, jsint *ip,
           JSSubString *sep)
{
    jsint i;
    size_t length;
    jschar *chars;

    /*
     * Stop if past end of string.  If at end of string, we will compare the
     * null char stored there (by js_NewString*) to sep->chars[j] in the while
     * loop at the end of this function, so that
     *
     *  "ab,".split(',') => ["ab", ""]
     *
     * and the resulting array converts back to the string "ab," for symmetry.
     * However, we ape Perl and do this only if there is a sufficiently large
     * limit argument (see str_split).
     */
    i = *ip;
    length = str->length();
    if ((size_t)i > length)
        return -1;

    chars = str->chars();

    /*
     * Match a regular expression against the separator at or above index i.
     * Call js_ExecuteRegExp with true for the test argument.  On successful
     * match, get the separator from cx->regExpStatics.lastMatch.
     */
    if (re) {
        size_t index;
        jsval rval;

      again:
        /* JS1.2 deviated from Perl by never matching at end of string. */
        index = (size_t)i;
        if (!js_ExecuteRegExp(cx, re, str, &index, JS_TRUE, &rval))
            return -2;
        if (rval != JSVAL_TRUE) {
            /* Mismatch: ensure our caller advances i past end of string. */
            sep->length = 1;
            return length;
        }
        i = (jsint)index;
        *sep = cx->regExpStatics.lastMatch;
        if (sep->length == 0) {
            /*
             * Empty string match: never split on an empty match at the start
             * of a find_split cycle.  Same rule as for an empty global match
             * in DoMatch.
             */
            if (i == *ip) {
                /*
                 * "Bump-along" to avoid sticking at an empty match, but don't
                 * bump past end of string -- our caller must do that by adding
                 * sep->length to our return value.
                 */
                if ((size_t)i == length)
                    return -1;
                i++;
                goto again;
            }
            if ((size_t)i == length) {
                /*
                 * If there was a trivial zero-length match at the end of the
                 * split, then we shouldn't output the matched string at the end
                 * of the split array. See ECMA-262 Ed. 3, 15.5.4.14, Step 15.
                 */
                sep->chars = NULL;
            }
        }
        JS_ASSERT((size_t)i >= sep->length);
        return i - sep->length;
    }

    /*
     * Special case: if sep is the empty string, split str into one character
     * substrings.  Let our caller worry about whether to split once at end of
     * string into an empty substring.
     */
    if (sep->length == 0)
        return ((size_t)i == length) ? -1 : i + 1;

    /*
     * Now that we know sep is non-empty, search starting at i in str for an
     * occurrence of all of sep's chars.  If we find them, return the index of
     * the first separator char.  Otherwise, return length.
     */
    jsint match = StringMatch(chars + i, length - i, sep->chars, sep->length);
    return match == -1 ? length : match + i;
}

static JSBool
str_split(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    NORMALIZE_THIS(cx, vp, str);

    if (argc == 0) {
        jsval v = STRING_TO_JSVAL(str);
        JSObject *aobj = js_NewArrayObject(cx, 1, &v);
        if (!aobj)
            return false;
        *vp = OBJECT_TO_JSVAL(aobj);
        return true;
    }

    JSRegExp *re;
    JSSubString *sep, tmp;
    if (VALUE_IS_REGEXP(cx, vp[2])) {
        re = (JSRegExp *) JSVAL_TO_OBJECT(vp[2])->getPrivate();
        sep = &tmp;

        /* Set a magic value so we can detect a successful re match. */
        sep->chars = NULL;
        sep->length = 0;
    } else {
        JSString *str2 = js_ValueToString(cx, vp[2]);
        if (!str2)
            return false;
        vp[2] = STRING_TO_JSVAL(str2);

        /*
         * Point sep at a local copy of str2's header because find_split
         * will modify sep->length.
         */
        str2->getCharsAndLength(tmp.chars, tmp.length);
        sep = &tmp;
        re = NULL;
    }

    /* Use the second argument as the split limit, if given. */
    uint32 limit = 0; /* Avoid warning. */
    bool limited = (argc > 1) && !JSVAL_IS_VOID(vp[3]);
    if (limited) {
        jsdouble d;
        if (!ValueToNumber(cx, vp[3], &d))
            return false;

        /* Clamp limit between 0 and 1 + string length. */
        limit = js_DoubleToECMAUint32(d);
        if (limit > str->length())
            limit = 1 + str->length();
    }

    AutoValueVector splits(cx);

    jsint i, j;
    uint32 len = i = 0;
    while ((j = find_split(cx, str, re, &i, sep)) >= 0) {
        if (limited && len >= limit)
            break;

        JSString *sub = js_NewDependentString(cx, str, i, size_t(j - i));
        if (!sub || !splits.append(sub))
            return false;
        len++;

        /*
         * Imitate perl's feature of including parenthesized substrings that
         * matched part of the delimiter in the new array, after the split
         * substring that was delimited.
         */
        if (re && sep->chars) {
            JSRegExpStatics *res = &cx->regExpStatics;
            for (uintN num = 0; num < res->parens.length(); num++) {
                if (limited && len >= limit)
                    break;
                JSSubString *parsub = &res->parens[num];
                sub = js_NewStringCopyN(cx, parsub->chars, parsub->length);
                if (!sub || !splits.append(sub))
                    return false;
                len++;
            }
            sep->chars = NULL;
        }
        i = j + sep->length;
    }

    if (j == -2)
        return false;

    JSObject *aobj = js_NewArrayObject(cx, splits.length(), splits.begin());
    if (!aobj)
        return false;
    *vp = OBJECT_TO_JSVAL(aobj);
    return true;
}

#if JS_HAS_PERL_SUBSTR
static JSBool
str_substr(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    jsdouble d;
    jsdouble length, begin, end;

    NORMALIZE_THIS(cx, vp, str);
    if (argc != 0) {
        if (!ValueToNumber(cx, vp[2], &d))
            return JS_FALSE;
        length = str->length();
        begin = js_DoubleToInteger(d);
        if (begin < 0) {
            begin += length;
            if (begin < 0)
                begin = 0;
        } else if (begin > length) {
            begin = length;
        }

        if (argc == 1) {
            end = length;
        } else {
            if (!ValueToNumber(cx, vp[3], &d))
                return JS_FALSE;
            end = js_DoubleToInteger(d);
            if (end < 0)
                end = 0;
            end += begin;
            if (end > length)
                end = length;
        }

        str = js_NewDependentString(cx, str,
                                    (size_t)begin,
                                    (size_t)(end - begin));
        if (!str)
            return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif /* JS_HAS_PERL_SUBSTR */

/*
 * Python-esque sequence operations.
 */
static JSBool
str_concat(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str, *str2;
    jsval *argv;
    uintN i;

    NORMALIZE_THIS(cx, vp, str);

    /* Set vp (aka rval) early to handle the argc == 0 case. */
    *vp = STRING_TO_JSVAL(str);

    for (i = 0, argv = vp + 2; i < argc; i++) {
        str2 = js_ValueToString(cx, argv[i]);
        if (!str2)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str2);

        str = js_ConcatStrings(cx, str, str2);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
    }

    return JS_TRUE;
}

static JSBool
str_slice(JSContext *cx, uintN argc, jsval *vp)
{
    jsval t, v;
    JSString *str;

    t = vp[1];
    v = vp[2];
    if (argc == 1 && JSVAL_IS_STRING(t) && JSVAL_IS_INT(v)) {
        size_t begin, end, length;

        str = JSVAL_TO_STRING(t);
        begin = JSVAL_TO_INT(v);
        end = str->length();
        if (begin <= end) {
            length = end - begin;
            if (length == 0) {
                str = cx->runtime->emptyString;
            } else {
                str = (length == 1)
                      ? JSString::getUnitString(cx, str, begin)
                      : js_NewDependentString(cx, str, begin, length);
                if (!str)
                    return JS_FALSE;
            }
            *vp = STRING_TO_JSVAL(str);
            return JS_TRUE;
        }
    }

    NORMALIZE_THIS(cx, vp, str);

    if (argc != 0) {
        double begin, end, length;

        if (!ValueToNumber(cx, vp[2], &begin))
            return JS_FALSE;
        begin = js_DoubleToInteger(begin);
        length = str->length();
        if (begin < 0) {
            begin += length;
            if (begin < 0)
                begin = 0;
        } else if (begin > length) {
            begin = length;
        }

        if (argc == 1) {
            end = length;
        } else {
            if (!ValueToNumber(cx, vp[3], &end))
                return JS_FALSE;
            end = js_DoubleToInteger(end);
            if (end < 0) {
                end += length;
                if (end < 0)
                    end = 0;
            } else if (end > length) {
                end = length;
            }
            if (end < begin)
                end = begin;
        }

        str = js_NewDependentString(cx, str,
                                    (size_t)begin,
                                    (size_t)(end - begin));
        if (!str)
            return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#if JS_HAS_STR_HTML_HELPERS
/*
 * HTML composition aids.
 */
static JSBool
tagify(JSContext *cx, const char *begin, JSString *param, const char *end,
       jsval *vp)
{
    JSString *str;
    jschar *tagbuf;
    size_t beglen, endlen, parlen, taglen;
    size_t i, j;

    NORMALIZE_THIS(cx, vp, str);

    if (!end)
        end = begin;

    beglen = strlen(begin);
    taglen = 1 + beglen + 1;                            /* '<begin' + '>' */
    parlen = 0; /* Avoid warning. */
    if (param) {
        parlen = param->length();
        taglen += 2 + parlen + 1;                       /* '="param"' */
    }
    endlen = strlen(end);
    taglen += str->length() + 2 + endlen + 1;    /* 'str</end>' */

    if (taglen >= ~(size_t)0 / sizeof(jschar)) {
        js_ReportAllocationOverflow(cx);
        return JS_FALSE;
    }

    tagbuf = (jschar *) cx->malloc((taglen + 1) * sizeof(jschar));
    if (!tagbuf)
        return JS_FALSE;

    j = 0;
    tagbuf[j++] = '<';
    for (i = 0; i < beglen; i++)
        tagbuf[j++] = (jschar)begin[i];
    if (param) {
        tagbuf[j++] = '=';
        tagbuf[j++] = '"';
        js_strncpy(&tagbuf[j], param->chars(), parlen);
        j += parlen;
        tagbuf[j++] = '"';
    }
    tagbuf[j++] = '>';
    js_strncpy(&tagbuf[j], str->chars(), str->length());
    j += str->length();
    tagbuf[j++] = '<';
    tagbuf[j++] = '/';
    for (i = 0; i < endlen; i++)
        tagbuf[j++] = (jschar)end[i];
    tagbuf[j++] = '>';
    JS_ASSERT(j == taglen);
    tagbuf[j] = 0;

    str = js_NewString(cx, tagbuf, taglen);
    if (!str) {
        js_free((char *)tagbuf);
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
tagify_value(JSContext *cx, uintN argc, jsval *vp,
             const char *begin, const char *end)
{
    JSString *param;

    param = ArgToRootedString(cx, argc, vp, 0);
    if (!param)
        return JS_FALSE;
    return tagify(cx, begin, param, end, vp);
}

static JSBool
str_bold(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "b", NULL, NULL, vp);
}

static JSBool
str_italics(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "i", NULL, NULL, vp);
}

static JSBool
str_fixed(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "tt", NULL, NULL, vp);
}

static JSBool
str_fontsize(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify_value(cx, argc, vp, "font size", "font");
}

static JSBool
str_fontcolor(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify_value(cx, argc, vp, "font color", "font");
}

static JSBool
str_link(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify_value(cx, argc, vp, "a href", "a");
}

static JSBool
str_anchor(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify_value(cx, argc, vp, "a name", "a");
}

static JSBool
str_strike(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "strike", NULL, NULL, vp);
}

static JSBool
str_small(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "small", NULL, NULL, vp);
}

static JSBool
str_big(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "big", NULL, NULL, vp);
}

static JSBool
str_blink(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "blink", NULL, NULL, vp);
}

static JSBool
str_sup(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "sup", NULL, NULL, vp);
}

static JSBool
str_sub(JSContext *cx, uintN argc, jsval *vp)
{
    return tagify(cx, "sub", NULL, NULL, vp);
}
#endif /* JS_HAS_STR_HTML_HELPERS */

#ifdef JS_TRACER
JSString* FASTCALL
js_String_getelem(JSContext* cx, JSString* str, int32 i)
{
    if ((size_t)i >= str->length())
        return NULL;
    return JSString::getUnitString(cx, str, size_t(i));
}
#endif

JS_DEFINE_TRCINFO_1(js_str_toString,
    (2, (extern, STRING_RETRY,      String_p_toString, CONTEXT, THIS,               1,
         nanojit::ACC_NONE)))
JS_DEFINE_TRCINFO_1(str_charAt,
    (3, (extern, STRING_RETRY,      js_String_getelem, CONTEXT, THIS_STRING, INT32, 1,
         nanojit::ACC_NONE)))
JS_DEFINE_TRCINFO_2(str_charCodeAt,
    (1, (extern, DOUBLE,            js_String_p_charCodeAt0, THIS_STRING,           1,
         nanojit::ACC_NONE)),
    (2, (extern, DOUBLE,            js_String_p_charCodeAt, THIS_STRING, DOUBLE,    1,
         nanojit::ACC_NONE)))
JS_DEFINE_TRCINFO_1(str_concat,
    (3, (extern, STRING_RETRY,      js_ConcatStrings, CONTEXT, THIS_STRING, STRING, 1,
         nanojit::ACC_NONE)))

#define GENERIC           JSFUN_GENERIC_NATIVE
#define PRIMITIVE         JSFUN_THISP_PRIMITIVE
#define GENERIC_PRIMITIVE (GENERIC | PRIMITIVE)

static JSFunctionSpec string_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN("quote",             str_quote,             0,GENERIC_PRIMITIVE),
    JS_FN(js_toSource_str,     str_toSource,          0,JSFUN_THISP_STRING),
#endif

    /* Java-like methods. */
    JS_TN(js_toString_str,     js_str_toString,       0,JSFUN_THISP_STRING, &js_str_toString_trcinfo),
    JS_FN(js_valueOf_str,      js_str_toString,       0,JSFUN_THISP_STRING),
    JS_FN(js_toJSON_str,       js_str_toString,       0,JSFUN_THISP_STRING),
    JS_FN("substring",         str_substring,         2,GENERIC_PRIMITIVE),
    JS_FN("toLowerCase",       str_toLowerCase,       0,GENERIC_PRIMITIVE),
    JS_FN("toUpperCase",       str_toUpperCase,       0,GENERIC_PRIMITIVE),
    JS_TN("charAt",            str_charAt,            1,GENERIC_PRIMITIVE, &str_charAt_trcinfo),
    JS_TN("charCodeAt",        str_charCodeAt,        1,GENERIC_PRIMITIVE, &str_charCodeAt_trcinfo),
    JS_FN("indexOf",           str_indexOf,           1,GENERIC_PRIMITIVE),
    JS_FN("lastIndexOf",       str_lastIndexOf,       1,GENERIC_PRIMITIVE),
    JS_FN("trim",              str_trim,              0,GENERIC_PRIMITIVE),
    JS_FN("trimLeft",          str_trimLeft,          0,GENERIC_PRIMITIVE),
    JS_FN("trimRight",         str_trimRight,         0,GENERIC_PRIMITIVE),
    JS_FN("toLocaleLowerCase", str_toLocaleLowerCase, 0,GENERIC_PRIMITIVE),
    JS_FN("toLocaleUpperCase", str_toLocaleUpperCase, 0,GENERIC_PRIMITIVE),
    JS_FN("localeCompare",     str_localeCompare,     1,GENERIC_PRIMITIVE),

    /* Perl-ish methods (search is actually Python-esque). */
    JS_FN("match",             str_match,             1,GENERIC_PRIMITIVE),
    JS_FN("search",            str_search,            1,GENERIC_PRIMITIVE),
    JS_FN("replace",           str_replace,           2,GENERIC_PRIMITIVE),
    JS_FN("split",             str_split,             2,GENERIC_PRIMITIVE),
#if JS_HAS_PERL_SUBSTR
    JS_FN("substr",            str_substr,            2,GENERIC_PRIMITIVE),
#endif

    /* Python-esque sequence methods. */
    JS_TN("concat",            str_concat,            1,GENERIC_PRIMITIVE, &str_concat_trcinfo),
    JS_FN("slice",             str_slice,             2,GENERIC_PRIMITIVE),

    /* HTML string methods. */
#if JS_HAS_STR_HTML_HELPERS
    JS_FN("bold",              str_bold,              0,PRIMITIVE),
    JS_FN("italics",           str_italics,           0,PRIMITIVE),
    JS_FN("fixed",             str_fixed,             0,PRIMITIVE),
    JS_FN("fontsize",          str_fontsize,          1,PRIMITIVE),
    JS_FN("fontcolor",         str_fontcolor,         1,PRIMITIVE),
    JS_FN("link",              str_link,              1,PRIMITIVE),
    JS_FN("anchor",            str_anchor,            1,PRIMITIVE),
    JS_FN("strike",            str_strike,            0,PRIMITIVE),
    JS_FN("small",             str_small,             0,PRIMITIVE),
    JS_FN("big",               str_big,               0,PRIMITIVE),
    JS_FN("blink",             str_blink,             0,PRIMITIVE),
    JS_FN("sup",               str_sup,               0,PRIMITIVE),
    JS_FN("sub",               str_sub,               0,PRIMITIVE),
#endif

    JS_FS_END
};

#define C(c) c, 0x00

/*
 * String data for all unit strings (including zero-char backstop required for independent strings),
 * packed into single array.
 */
static const jschar UnitStringData[] = {
    C(0x00), C(0x01), C(0x02), C(0x03), C(0x04), C(0x05), C(0x06), C(0x07),
    C(0x08), C(0x09), C(0x0a), C(0x0b), C(0x0c), C(0x0d), C(0x0e), C(0x0f),
    C(0x10), C(0x11), C(0x12), C(0x13), C(0x14), C(0x15), C(0x16), C(0x17),
    C(0x18), C(0x19), C(0x1a), C(0x1b), C(0x1c), C(0x1d), C(0x1e), C(0x1f),
    C(0x20), C(0x21), C(0x22), C(0x23), C(0x24), C(0x25), C(0x26), C(0x27),
    C(0x28), C(0x29), C(0x2a), C(0x2b), C(0x2c), C(0x2d), C(0x2e), C(0x2f),
    C(0x30), C(0x31), C(0x32), C(0x33), C(0x34), C(0x35), C(0x36), C(0x37),
    C(0x38), C(0x39), C(0x3a), C(0x3b), C(0x3c), C(0x3d), C(0x3e), C(0x3f),
    C(0x40), C(0x41), C(0x42), C(0x43), C(0x44), C(0x45), C(0x46), C(0x47),
    C(0x48), C(0x49), C(0x4a), C(0x4b), C(0x4c), C(0x4d), C(0x4e), C(0x4f),
    C(0x50), C(0x51), C(0x52), C(0x53), C(0x54), C(0x55), C(0x56), C(0x57),
    C(0x58), C(0x59), C(0x5a), C(0x5b), C(0x5c), C(0x5d), C(0x5e), C(0x5f),
    C(0x60), C(0x61), C(0x62), C(0x63), C(0x64), C(0x65), C(0x66), C(0x67),
    C(0x68), C(0x69), C(0x6a), C(0x6b), C(0x6c), C(0x6d), C(0x6e), C(0x6f),
    C(0x70), C(0x71), C(0x72), C(0x73), C(0x74), C(0x75), C(0x76), C(0x77),
    C(0x78), C(0x79), C(0x7a), C(0x7b), C(0x7c), C(0x7d), C(0x7e), C(0x7f),
    C(0x80), C(0x81), C(0x82), C(0x83), C(0x84), C(0x85), C(0x86), C(0x87),
    C(0x88), C(0x89), C(0x8a), C(0x8b), C(0x8c), C(0x8d), C(0x8e), C(0x8f),
    C(0x90), C(0x91), C(0x92), C(0x93), C(0x94), C(0x95), C(0x96), C(0x97),
    C(0x98), C(0x99), C(0x9a), C(0x9b), C(0x9c), C(0x9d), C(0x9e), C(0x9f),
    C(0xa0), C(0xa1), C(0xa2), C(0xa3), C(0xa4), C(0xa5), C(0xa6), C(0xa7),
    C(0xa8), C(0xa9), C(0xaa), C(0xab), C(0xac), C(0xad), C(0xae), C(0xaf),
    C(0xb0), C(0xb1), C(0xb2), C(0xb3), C(0xb4), C(0xb5), C(0xb6), C(0xb7),
    C(0xb8), C(0xb9), C(0xba), C(0xbb), C(0xbc), C(0xbd), C(0xbe), C(0xbf),
    C(0xc0), C(0xc1), C(0xc2), C(0xc3), C(0xc4), C(0xc5), C(0xc6), C(0xc7),
    C(0xc8), C(0xc9), C(0xca), C(0xcb), C(0xcc), C(0xcd), C(0xce), C(0xcf),
    C(0xd0), C(0xd1), C(0xd2), C(0xd3), C(0xd4), C(0xd5), C(0xd6), C(0xd7),
    C(0xd8), C(0xd9), C(0xda), C(0xdb), C(0xdc), C(0xdd), C(0xde), C(0xdf),
    C(0xe0), C(0xe1), C(0xe2), C(0xe3), C(0xe4), C(0xe5), C(0xe6), C(0xe7),
    C(0xe8), C(0xe9), C(0xea), C(0xeb), C(0xec), C(0xed), C(0xee), C(0xef),
    C(0xf0), C(0xf1), C(0xf2), C(0xf3), C(0xf4), C(0xf5), C(0xf6), C(0xf7),
    C(0xf8), C(0xf9), C(0xfa), C(0xfb), C(0xfc), C(0xfd), C(0xfe), C(0xff)
};

#define U(c) { 1, 0, JSString::ATOMIZED, {(jschar *)UnitStringData + (c) * 2} }

#ifdef __SUNPRO_CC
#pragma pack(8)
#else
#pragma pack(push, 8)
#endif

JSString JSString::unitStringTable[]
#ifdef __GNUC__
__attribute__ ((aligned (8)))
#endif
= {
    U(0x00), U(0x01), U(0x02), U(0x03), U(0x04), U(0x05), U(0x06), U(0x07),
    U(0x08), U(0x09), U(0x0a), U(0x0b), U(0x0c), U(0x0d), U(0x0e), U(0x0f),
    U(0x10), U(0x11), U(0x12), U(0x13), U(0x14), U(0x15), U(0x16), U(0x17),
    U(0x18), U(0x19), U(0x1a), U(0x1b), U(0x1c), U(0x1d), U(0x1e), U(0x1f),
    U(0x20), U(0x21), U(0x22), U(0x23), U(0x24), U(0x25), U(0x26), U(0x27),
    U(0x28), U(0x29), U(0x2a), U(0x2b), U(0x2c), U(0x2d), U(0x2e), U(0x2f),
    U(0x30), U(0x31), U(0x32), U(0x33), U(0x34), U(0x35), U(0x36), U(0x37),
    U(0x38), U(0x39), U(0x3a), U(0x3b), U(0x3c), U(0x3d), U(0x3e), U(0x3f),
    U(0x40), U(0x41), U(0x42), U(0x43), U(0x44), U(0x45), U(0x46), U(0x47),
    U(0x48), U(0x49), U(0x4a), U(0x4b), U(0x4c), U(0x4d), U(0x4e), U(0x4f),
    U(0x50), U(0x51), U(0x52), U(0x53), U(0x54), U(0x55), U(0x56), U(0x57),
    U(0x58), U(0x59), U(0x5a), U(0x5b), U(0x5c), U(0x5d), U(0x5e), U(0x5f),
    U(0x60), U(0x61), U(0x62), U(0x63), U(0x64), U(0x65), U(0x66), U(0x67),
    U(0x68), U(0x69), U(0x6a), U(0x6b), U(0x6c), U(0x6d), U(0x6e), U(0x6f),
    U(0x70), U(0x71), U(0x72), U(0x73), U(0x74), U(0x75), U(0x76), U(0x77),
    U(0x78), U(0x79), U(0x7a), U(0x7b), U(0x7c), U(0x7d), U(0x7e), U(0x7f),
    U(0x80), U(0x81), U(0x82), U(0x83), U(0x84), U(0x85), U(0x86), U(0x87),
    U(0x88), U(0x89), U(0x8a), U(0x8b), U(0x8c), U(0x8d), U(0x8e), U(0x8f),
    U(0x90), U(0x91), U(0x92), U(0x93), U(0x94), U(0x95), U(0x96), U(0x97),
    U(0x98), U(0x99), U(0x9a), U(0x9b), U(0x9c), U(0x9d), U(0x9e), U(0x9f),
    U(0xa0), U(0xa1), U(0xa2), U(0xa3), U(0xa4), U(0xa5), U(0xa6), U(0xa7),
    U(0xa8), U(0xa9), U(0xaa), U(0xab), U(0xac), U(0xad), U(0xae), U(0xaf),
    U(0xb0), U(0xb1), U(0xb2), U(0xb3), U(0xb4), U(0xb5), U(0xb6), U(0xb7),
    U(0xb8), U(0xb9), U(0xba), U(0xbb), U(0xbc), U(0xbd), U(0xbe), U(0xbf),
    U(0xc0), U(0xc1), U(0xc2), U(0xc3), U(0xc4), U(0xc5), U(0xc6), U(0xc7),
    U(0xc8), U(0xc9), U(0xca), U(0xcb), U(0xcc), U(0xcd), U(0xce), U(0xcf),
    U(0xd0), U(0xd1), U(0xd2), U(0xd3), U(0xd4), U(0xd5), U(0xd6), U(0xd7),
    U(0xd8), U(0xd9), U(0xda), U(0xdb), U(0xdc), U(0xdd), U(0xde), U(0xdf),
    U(0xe0), U(0xe1), U(0xe2), U(0xe3), U(0xe4), U(0xe5), U(0xe6), U(0xe7),
    U(0xe8), U(0xe9), U(0xea), U(0xeb), U(0xec), U(0xed), U(0xee), U(0xef),
    U(0xf0), U(0xf1), U(0xf2), U(0xf3), U(0xf4), U(0xf5), U(0xf6), U(0xf7),
    U(0xf8), U(0xf9), U(0xfa), U(0xfb), U(0xfc), U(0xfd), U(0xfe), U(0xff)
};

#ifdef __SUNPRO_CC
#pragma pack(0)
#else
#pragma pack(pop)
#endif

#undef U

#define O0(c) 0x30, C(c)
#define O1(c) 0x31, C(c) /* template for 10 .. 19 */
#define O2(c) 0x32, C(c) /* template for 20 .. 29 */
#define O3(c) 0x33, C(c)
#define O4(c) 0x34, C(c)
#define O5(c) 0x35, C(c)
#define O6(c) 0x36, C(c)
#define O7(c) 0x37, C(c)
#define O8(c) 0x38, C(c)
#define O9(c) 0x39, C(c)

#define O10(c) 0x31, O0(c) /* template for 100 .. 109 */
#define O11(c) 0x31, O1(c) /* template for 110 .. 119 */
#define O12(c) 0x31, O2(c)
#define O13(c) 0x31, O3(c)
#define O14(c) 0x31, O4(c)
#define O15(c) 0x31, O5(c)
#define O16(c) 0x31, O6(c)
#define O17(c) 0x31, O7(c)
#define O18(c) 0x31, O8(c)
#define O19(c) 0x31, O9(c)
#define O20(c) 0x32, O0(c)
#define O21(c) 0x32, O1(c)
#define O22(c) 0x32, O2(c)
#define O23(c) 0x32, O3(c)
#define O24(c) 0x32, O4(c)
#define O25(c) 0x32, O5(c)

/*
 * Array starts with 100, 101, 102... (0x31 0x30 0x30 0x00 for 100\0)
 * 100, 101, 102 also share the pointers to 0, 1, 2 ...
 * 110, 111, 112 also share the pointers to 10, 11, 12...
 */
static const jschar Hundreds[] = {
    O10(0x30), O10(0x31), O10(0x32), O10(0x33), O10(0x34), O10(0x35), O10(0x36), O10(0x37), O10(0x38), O10(0x39),
    O11(0x30), O11(0x31), O11(0x32), O11(0x33), O11(0x34), O11(0x35), O11(0x36), O11(0x37), O11(0x38), O11(0x39),
    O12(0x30), O12(0x31), O12(0x32), O12(0x33), O12(0x34), O12(0x35), O12(0x36), O12(0x37), O12(0x38), O12(0x39),
    O13(0x30), O13(0x31), O13(0x32), O13(0x33), O13(0x34), O13(0x35), O13(0x36), O13(0x37), O13(0x38), O13(0x39),
    O14(0x30), O14(0x31), O14(0x32), O14(0x33), O14(0x34), O14(0x35), O14(0x36), O14(0x37), O14(0x38), O14(0x39),
    O15(0x30), O15(0x31), O15(0x32), O15(0x33), O15(0x34), O15(0x35), O15(0x36), O15(0x37), O15(0x38), O15(0x39),
    O16(0x30), O16(0x31), O16(0x32), O16(0x33), O16(0x34), O16(0x35), O16(0x36), O16(0x37), O16(0x38), O16(0x39),
    O17(0x30), O17(0x31), O17(0x32), O17(0x33), O17(0x34), O17(0x35), O17(0x36), O17(0x37), O17(0x38), O17(0x39),
    O18(0x30), O18(0x31), O18(0x32), O18(0x33), O18(0x34), O18(0x35), O18(0x36), O18(0x37), O18(0x38), O18(0x39),
    O19(0x30), O19(0x31), O19(0x32), O19(0x33), O19(0x34), O19(0x35), O19(0x36), O19(0x37), O19(0x38), O19(0x39),
    O20(0x30), O20(0x31), O20(0x32), O20(0x33), O20(0x34), O20(0x35), O20(0x36), O20(0x37), O20(0x38), O20(0x39),
    O21(0x30), O21(0x31), O21(0x32), O21(0x33), O21(0x34), O21(0x35), O21(0x36), O21(0x37), O21(0x38), O21(0x39),
    O22(0x30), O22(0x31), O22(0x32), O22(0x33), O22(0x34), O22(0x35), O22(0x36), O22(0x37), O22(0x38), O22(0x39),
    O23(0x30), O23(0x31), O23(0x32), O23(0x33), O23(0x34), O23(0x35), O23(0x36), O23(0x37), O23(0x38), O23(0x39),
    O24(0x30), O24(0x31), O24(0x32), O24(0x33), O24(0x34), O24(0x35), O24(0x36), O24(0x37), O24(0x38), O24(0x39),
    O25(0x30), O25(0x31), O25(0x32), O25(0x33), O25(0x34), O25(0x35)
};

#define L1(c) { 1, 0, JSString::ATOMIZED, {(jschar *)Hundreds + 2 + (c) * 4} } /* length 1: 0..9 */
#define L2(c) { 2, 0, JSString::ATOMIZED, {(jschar *)Hundreds + 41 + (c - 10) * 4} } /* length 2: 10..99 */
#define L3(c) { 3, 0, JSString::ATOMIZED, {(jschar *)Hundreds + (c - 100) * 4} } /* length 3: 100..255 */

#ifdef __SUNPRO_CC
#pragma pack(8)
#else
#pragma pack(push, 8)
#endif

JSString JSString::intStringTable[]
#ifdef __GNUC__
__attribute__ ((aligned (8)))
#endif
= {
    L1(0x00), L1(0x01), L1(0x02), L1(0x03), L1(0x04), L1(0x05), L1(0x06), L1(0x07),
    L1(0x08), L1(0x09), L2(0x0a), L2(0x0b), L2(0x0c), L2(0x0d), L2(0x0e), L2(0x0f),
    L2(0x10), L2(0x11), L2(0x12), L2(0x13), L2(0x14), L2(0x15), L2(0x16), L2(0x17),
    L2(0x18), L2(0x19), L2(0x1a), L2(0x1b), L2(0x1c), L2(0x1d), L2(0x1e), L2(0x1f),
    L2(0x20), L2(0x21), L2(0x22), L2(0x23), L2(0x24), L2(0x25), L2(0x26), L2(0x27),
    L2(0x28), L2(0x29), L2(0x2a), L2(0x2b), L2(0x2c), L2(0x2d), L2(0x2e), L2(0x2f),
    L2(0x30), L2(0x31), L2(0x32), L2(0x33), L2(0x34), L2(0x35), L2(0x36), L2(0x37),
    L2(0x38), L2(0x39), L2(0x3a), L2(0x3b), L2(0x3c), L2(0x3d), L2(0x3e), L2(0x3f),
    L2(0x40), L2(0x41), L2(0x42), L2(0x43), L2(0x44), L2(0x45), L2(0x46), L2(0x47),
    L2(0x48), L2(0x49), L2(0x4a), L2(0x4b), L2(0x4c), L2(0x4d), L2(0x4e), L2(0x4f),
    L2(0x50), L2(0x51), L2(0x52), L2(0x53), L2(0x54), L2(0x55), L2(0x56), L2(0x57),
    L2(0x58), L2(0x59), L2(0x5a), L2(0x5b), L2(0x5c), L2(0x5d), L2(0x5e), L2(0x5f),
    L2(0x60), L2(0x61), L2(0x62), L2(0x63), L3(0x64), L3(0x65), L3(0x66), L3(0x67),
    L3(0x68), L3(0x69), L3(0x6a), L3(0x6b), L3(0x6c), L3(0x6d), L3(0x6e), L3(0x6f),
    L3(0x70), L3(0x71), L3(0x72), L3(0x73), L3(0x74), L3(0x75), L3(0x76), L3(0x77),
    L3(0x78), L3(0x79), L3(0x7a), L3(0x7b), L3(0x7c), L3(0x7d), L3(0x7e), L3(0x7f),
    L3(0x80), L3(0x81), L3(0x82), L3(0x83), L3(0x84), L3(0x85), L3(0x86), L3(0x87),
    L3(0x88), L3(0x89), L3(0x8a), L3(0x8b), L3(0x8c), L3(0x8d), L3(0x8e), L3(0x8f),
    L3(0x90), L3(0x91), L3(0x92), L3(0x93), L3(0x94), L3(0x95), L3(0x96), L3(0x97),
    L3(0x98), L3(0x99), L3(0x9a), L3(0x9b), L3(0x9c), L3(0x9d), L3(0x9e), L3(0x9f),
    L3(0xa0), L3(0xa1), L3(0xa2), L3(0xa3), L3(0xa4), L3(0xa5), L3(0xa6), L3(0xa7),
    L3(0xa8), L3(0xa9), L3(0xaa), L3(0xab), L3(0xac), L3(0xad), L3(0xae), L3(0xaf),
    L3(0xb0), L3(0xb1), L3(0xb2), L3(0xb3), L3(0xb4), L3(0xb5), L3(0xb6), L3(0xb7),
    L3(0xb8), L3(0xb9), L3(0xba), L3(0xbb), L3(0xbc), L3(0xbd), L3(0xbe), L3(0xbf),
    L3(0xc0), L3(0xc1), L3(0xc2), L3(0xc3), L3(0xc4), L3(0xc5), L3(0xc6), L3(0xc7),
    L3(0xc8), L3(0xc9), L3(0xca), L3(0xcb), L3(0xcc), L3(0xcd), L3(0xce), L3(0xcf),
    L3(0xd0), L3(0xd1), L3(0xd2), L3(0xd3), L3(0xd4), L3(0xd5), L3(0xd6), L3(0xd7),
    L3(0xd8), L3(0xd9), L3(0xda), L3(0xdb), L3(0xdc), L3(0xdd), L3(0xde), L3(0xdf),
    L3(0xe0), L3(0xe1), L3(0xe2), L3(0xe3), L3(0xe4), L3(0xe5), L3(0xe6), L3(0xe7),
    L3(0xe8), L3(0xe9), L3(0xea), L3(0xeb), L3(0xec), L3(0xed), L3(0xee), L3(0xef),
    L3(0xf0), L3(0xf1), L3(0xf2), L3(0xf3), L3(0xf4), L3(0xf5), L3(0xf6), L3(0xf7),
    L3(0xf8), L3(0xf9), L3(0xfa), L3(0xfb), L3(0xfc), L3(0xfd), L3(0xfe), L3(0xff)
};

#ifdef __SUNPRO_CC
#pragma pack(0)
#else
#pragma pack(pop)
#endif

#undef L1
#undef L2
#undef L3

static const char AsciiHundreds[] = {
    O10(0x30), O10(0x31), O10(0x32), O10(0x33), O10(0x34), O10(0x35), O10(0x36), O10(0x37), O10(0x38), O10(0x39),
    O11(0x30), O11(0x31), O11(0x32), O11(0x33), O11(0x34), O11(0x35), O11(0x36), O11(0x37), O11(0x38), O11(0x39),
    O12(0x30), O12(0x31), O12(0x32), O12(0x33), O12(0x34), O12(0x35), O12(0x36), O12(0x37), O12(0x38), O12(0x39),
    O13(0x30), O13(0x31), O13(0x32), O13(0x33), O13(0x34), O13(0x35), O13(0x36), O13(0x37), O13(0x38), O13(0x39),
    O14(0x30), O14(0x31), O14(0x32), O14(0x33), O14(0x34), O14(0x35), O14(0x36), O14(0x37), O14(0x38), O14(0x39),
    O15(0x30), O15(0x31), O15(0x32), O15(0x33), O15(0x34), O15(0x35), O15(0x36), O15(0x37), O15(0x38), O15(0x39),
    O16(0x30), O16(0x31), O16(0x32), O16(0x33), O16(0x34), O16(0x35), O16(0x36), O16(0x37), O16(0x38), O16(0x39),
    O17(0x30), O17(0x31), O17(0x32), O17(0x33), O17(0x34), O17(0x35), O17(0x36), O17(0x37), O17(0x38), O17(0x39),
    O18(0x30), O18(0x31), O18(0x32), O18(0x33), O18(0x34), O18(0x35), O18(0x36), O18(0x37), O18(0x38), O18(0x39),
    O19(0x30), O19(0x31), O19(0x32), O19(0x33), O19(0x34), O19(0x35), O19(0x36), O19(0x37), O19(0x38), O19(0x39),
    O20(0x30), O20(0x31), O20(0x32), O20(0x33), O20(0x34), O20(0x35), O20(0x36), O20(0x37), O20(0x38), O20(0x39),
    O21(0x30), O21(0x31), O21(0x32), O21(0x33), O21(0x34), O21(0x35), O21(0x36), O21(0x37), O21(0x38), O21(0x39),
    O22(0x30), O22(0x31), O22(0x32), O22(0x33), O22(0x34), O22(0x35), O22(0x36), O22(0x37), O22(0x38), O22(0x39),
    O23(0x30), O23(0x31), O23(0x32), O23(0x33), O23(0x34), O23(0x35), O23(0x36), O23(0x37), O23(0x38), O23(0x39),
    O24(0x30), O24(0x31), O24(0x32), O24(0x33), O24(0x34), O24(0x35), O24(0x36), O24(0x37), O24(0x38), O24(0x39),
    O25(0x30), O25(0x31), O25(0x32), O25(0x33), O25(0x34), O25(0x35)
};

#define L1(c) (AsciiHundreds + 2 + (c) * 4)             /* length 1: 0..9 */
#define L2(c) (AsciiHundreds + 41 + (c - 10) * 4)       /* length 2: 10..99 */
#define L3(c) (AsciiHundreds + (c - 100) * 4)           /* length 3: 100..255 */

const char *JSString::deflatedIntStringTable[] = {
    L1(0x00), L1(0x01), L1(0x02), L1(0x03), L1(0x04), L1(0x05), L1(0x06), L1(0x07),
    L1(0x08), L1(0x09), L2(0x0a), L2(0x0b), L2(0x0c), L2(0x0d), L2(0x0e), L2(0x0f),
    L2(0x10), L2(0x11), L2(0x12), L2(0x13), L2(0x14), L2(0x15), L2(0x16), L2(0x17),
    L2(0x18), L2(0x19), L2(0x1a), L2(0x1b), L2(0x1c), L2(0x1d), L2(0x1e), L2(0x1f),
    L2(0x20), L2(0x21), L2(0x22), L2(0x23), L2(0x24), L2(0x25), L2(0x26), L2(0x27),
    L2(0x28), L2(0x29), L2(0x2a), L2(0x2b), L2(0x2c), L2(0x2d), L2(0x2e), L2(0x2f),
    L2(0x30), L2(0x31), L2(0x32), L2(0x33), L2(0x34), L2(0x35), L2(0x36), L2(0x37),
    L2(0x38), L2(0x39), L2(0x3a), L2(0x3b), L2(0x3c), L2(0x3d), L2(0x3e), L2(0x3f),
    L2(0x40), L2(0x41), L2(0x42), L2(0x43), L2(0x44), L2(0x45), L2(0x46), L2(0x47),
    L2(0x48), L2(0x49), L2(0x4a), L2(0x4b), L2(0x4c), L2(0x4d), L2(0x4e), L2(0x4f),
    L2(0x50), L2(0x51), L2(0x52), L2(0x53), L2(0x54), L2(0x55), L2(0x56), L2(0x57),
    L2(0x58), L2(0x59), L2(0x5a), L2(0x5b), L2(0x5c), L2(0x5d), L2(0x5e), L2(0x5f),
    L2(0x60), L2(0x61), L2(0x62), L2(0x63), L3(0x64), L3(0x65), L3(0x66), L3(0x67),
    L3(0x68), L3(0x69), L3(0x6a), L3(0x6b), L3(0x6c), L3(0x6d), L3(0x6e), L3(0x6f),
    L3(0x70), L3(0x71), L3(0x72), L3(0x73), L3(0x74), L3(0x75), L3(0x76), L3(0x77),
    L3(0x78), L3(0x79), L3(0x7a), L3(0x7b), L3(0x7c), L3(0x7d), L3(0x7e), L3(0x7f),
    L3(0x80), L3(0x81), L3(0x82), L3(0x83), L3(0x84), L3(0x85), L3(0x86), L3(0x87),
    L3(0x88), L3(0x89), L3(0x8a), L3(0x8b), L3(0x8c), L3(0x8d), L3(0x8e), L3(0x8f),
    L3(0x90), L3(0x91), L3(0x92), L3(0x93), L3(0x94), L3(0x95), L3(0x96), L3(0x97),
    L3(0x98), L3(0x99), L3(0x9a), L3(0x9b), L3(0x9c), L3(0x9d), L3(0x9e), L3(0x9f),
    L3(0xa0), L3(0xa1), L3(0xa2), L3(0xa3), L3(0xa4), L3(0xa5), L3(0xa6), L3(0xa7),
    L3(0xa8), L3(0xa9), L3(0xaa), L3(0xab), L3(0xac), L3(0xad), L3(0xae), L3(0xaf),
    L3(0xb0), L3(0xb1), L3(0xb2), L3(0xb3), L3(0xb4), L3(0xb5), L3(0xb6), L3(0xb7),
    L3(0xb8), L3(0xb9), L3(0xba), L3(0xbb), L3(0xbc), L3(0xbd), L3(0xbe), L3(0xbf),
    L3(0xc0), L3(0xc1), L3(0xc2), L3(0xc3), L3(0xc4), L3(0xc5), L3(0xc6), L3(0xc7),
    L3(0xc8), L3(0xc9), L3(0xca), L3(0xcb), L3(0xcc), L3(0xcd), L3(0xce), L3(0xcf),
    L3(0xd0), L3(0xd1), L3(0xd2), L3(0xd3), L3(0xd4), L3(0xd5), L3(0xd6), L3(0xd7),
    L3(0xd8), L3(0xd9), L3(0xda), L3(0xdb), L3(0xdc), L3(0xdd), L3(0xde), L3(0xdf),
    L3(0xe0), L3(0xe1), L3(0xe2), L3(0xe3), L3(0xe4), L3(0xe5), L3(0xe6), L3(0xe7),
    L3(0xe8), L3(0xe9), L3(0xea), L3(0xeb), L3(0xec), L3(0xed), L3(0xee), L3(0xef),
    L3(0xf0), L3(0xf1), L3(0xf2), L3(0xf3), L3(0xf4), L3(0xf5), L3(0xf6), L3(0xf7),
    L3(0xf8), L3(0xf9), L3(0xfa), L3(0xfb), L3(0xfc), L3(0xfd), L3(0xfe), L3(0xff)
};

#undef L1
#undef L2
#undef L3

/* Static table for common UTF8 encoding */
#define U8(c)   char(((c) >> 6) | 0xc0), char(((c) & 0x3f) | 0x80), 0
#define U(c)    U8(c), U8(c+1), U8(c+2), U8(c+3), U8(c+4), U8(c+5), U8(c+6), U8(c+7)

const char JSString::deflatedUnitStringTable[] = {
    U(0x80), U(0x88), U(0x90), U(0x98), U(0xa0), U(0xa8), U(0xb0), U(0xb8),
    U(0xc0), U(0xc8), U(0xd0), U(0xd8), U(0xe0), U(0xe8), U(0xf0), U(0xf8)
};

#undef U
#undef U8

#undef C

#undef O0
#undef O1
#undef O2
#undef O3
#undef O4
#undef O5
#undef O6
#undef O7
#undef O8
#undef O9
#undef O10
#undef O11
#undef O12
#undef O13
#undef O14
#undef O15
#undef O16
#undef O17
#undef O18
#undef O19
#undef O20
#undef O21
#undef O22
#undef O23
#undef O24
#undef O25

JSBool
js_String(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;

    if (argc > 0) {
        str = js_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        argv[0] = STRING_TO_JSVAL(str);
    } else {
        str = cx->runtime->emptyString;
    }
    if (!JS_IsConstructing(cx)) {
        *rval = STRING_TO_JSVAL(str);
        return JS_TRUE;
    }
    obj->setPrimitiveThis(STRING_TO_JSVAL(str));
    return JS_TRUE;
}

#ifdef JS_TRACER

JSObject* FASTCALL
js_String_tn(JSContext* cx, JSObject* proto, JSString* str)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    return js_NewObjectWithClassProto(cx, &js_StringClass, proto, STRING_TO_JSVAL(str));
}
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_String_tn, CONTEXT, CALLEE_PROTOTYPE, STRING, 0,
                     nanojit::ACC_STORE_ANY)

#endif /* !JS_TRACER */

static JSBool
str_fromCharCode(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    uintN i;
    jschar *chars;
    JSString *str;

    argv = vp + 2;
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    if (argc == 1) {
        uint16_t code;
        if (!ValueToUint16(cx, argv[0], &code))
            return JS_FALSE;
        if (code < UNIT_STRING_LIMIT) {
            str = JSString::unitString(code);
            if (!str)
                return JS_FALSE;
            *vp = STRING_TO_JSVAL(str);
            return JS_TRUE;
        }
        argv[0] = INT_TO_JSVAL(code);
    }
    chars = (jschar *) cx->malloc((argc + 1) * sizeof(jschar));
    if (!chars)
        return JS_FALSE;
    for (i = 0; i < argc; i++) {
        uint16_t code;
        if (!ValueToUint16(cx, argv[i], &code)) {
            cx->free(chars);
            return JS_FALSE;
        }
        chars[i] = (jschar)code;
    }
    chars[i] = 0;
    str = js_NewString(cx, chars, argc);
    if (!str) {
        cx->free(chars);
        return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#ifdef JS_TRACER
static JSString* FASTCALL
String_fromCharCode(JSContext* cx, int32 i)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    jschar c = (jschar)i;
    if (c < UNIT_STRING_LIMIT)
        return JSString::unitString(c);
    return js_NewStringCopyN(cx, &c, 1);
}
#endif

JS_DEFINE_TRCINFO_1(str_fromCharCode,
    (2, (static, STRING_RETRY, String_fromCharCode, CONTEXT, INT32, 1, nanojit::ACC_NONE)))

static JSFunctionSpec string_static_methods[] = {
    JS_TN("fromCharCode", str_fromCharCode, 1, 0, &str_fromCharCode_trcinfo),
    JS_FS_END
};

JSObject *
js_InitStringClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    /* Define the escape, unescape functions in the global object. */
    if (!JS_DefineFunctions(cx, obj, string_functions))
        return NULL;

    proto = JS_InitClass(cx, obj, NULL, &js_StringClass, js_String, 1,
                         NULL, string_methods,
                         NULL, string_static_methods);
    if (!proto)
        return NULL;
    proto->setPrimitiveThis(STRING_TO_JSVAL(cx->runtime->emptyString));
    if (!js_DefineNativeProperty(cx, proto, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom),
                                 JSVAL_VOID, NULL, NULL,
                                 JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0,
                                 NULL)) {
        return JS_FALSE;
    }

    return proto;
}

JSString *
js_NewString(JSContext *cx, jschar *chars, size_t length)
{
    JSString *str;

    if (length > JSString::MAX_LENGTH) {
        if (JS_ON_TRACE(cx)) {
            /*
             * If we can't leave the trace, signal OOM condition, otherwise
             * exit from trace before throwing.
             */
            if (!CanLeaveTrace(cx))
                return NULL;

            LeaveTrace(cx);
        }
        js_ReportAllocationOverflow(cx);
        return NULL;
    }

    str = js_NewGCString(cx);
    if (!str)
        return NULL;
    str->initFlat(chars, length);
#ifdef DEBUG
  {
    JSRuntime *rt = cx->runtime;
    JS_RUNTIME_METER(rt, liveStrings);
    JS_RUNTIME_METER(rt, totalStrings);
    JS_LOCK_RUNTIME_VOID(rt,
        (rt->lengthSum += (double)length,
         rt->lengthSquaredSum += (double)length * (double)length));
  }
#endif
    return str;
}

static const size_t sMinWasteSize = 16;

JSString *
js_NewStringFromCharBuffer(JSContext *cx, JSCharBuffer &cb)
{
    if (cb.empty())
        return ATOM_TO_STRING(cx->runtime->atomState.emptyAtom);

    size_t length = cb.length();
    if (!cb.append('\0'))
        return NULL;

    size_t capacity = cb.capacity();

    jschar *buf = cb.extractRawBuffer();
    if (!buf)
        return NULL;

    /* For medium/big buffers, avoid wasting more than 1/4 of the memory. */
    JS_ASSERT(capacity >= length);
    if (capacity > sMinWasteSize && capacity - length > (length >> 2)) {
        size_t bytes = sizeof(jschar) * (length + 1);
        jschar *tmp = (jschar *)cx->realloc(buf, bytes);
        if (!tmp) {
            cx->free(buf);
            return NULL;
        }
        buf = tmp;
    }

    JSString *str = js_NewString(cx, buf, length);
    if (!str)
        cx->free(buf);
    return str;
}

JSString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start,
                      size_t length)
{
    JSString *ds;

    if (length == 0)
        return cx->runtime->emptyString;

    if (start == 0 && length == base->length())
        return base;

    ds = js_NewGCString(cx);
    if (!ds)
        return NULL;
    ds->initDependent(base, start, length);
#ifdef DEBUG
  {
    JSRuntime *rt = cx->runtime;
    JS_RUNTIME_METER(rt, liveDependentStrings);
    JS_RUNTIME_METER(rt, totalDependentStrings);
    JS_RUNTIME_METER(rt, liveStrings);
    JS_RUNTIME_METER(rt, totalStrings);
    JS_LOCK_RUNTIME_VOID(rt,
        (rt->strdepLengthSum += (double)length,
         rt->strdepLengthSquaredSum += (double)length * (double)length));
    JS_LOCK_RUNTIME_VOID(rt,
        (rt->lengthSum += (double)length,
         rt->lengthSquaredSum += (double)length * (double)length));
  }
#endif
    return ds;
}

#ifdef DEBUG
#include <math.h>

void printJSStringStats(JSRuntime *rt)
{
    double mean, sigma;

    mean = JS_MeanAndStdDev(rt->totalStrings, rt->lengthSum,
                            rt->lengthSquaredSum, &sigma);

    fprintf(stderr, "%lu total strings, mean length %g (sigma %g)\n",
            (unsigned long)rt->totalStrings, mean, sigma);

    mean = JS_MeanAndStdDev(rt->totalDependentStrings, rt->strdepLengthSum,
                            rt->strdepLengthSquaredSum, &sigma);

    fprintf(stderr, "%lu total dependent strings, mean length %g (sigma %g)\n",
            (unsigned long)rt->totalDependentStrings, mean, sigma);
}
#endif

JSString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    jschar *news;
    JSString *str;

    news = (jschar *) cx->malloc((n + 1) * sizeof(jschar));
    if (!news)
        return NULL;
    js_strncpy(news, s, n);
    news[n] = 0;
    str = js_NewString(cx, news, n);
    if (!str)
        cx->free(news);
    return str;
}

JSString *
js_NewStringCopyZ(JSContext *cx, const jschar *s)
{
    size_t n, m;
    jschar *news;
    JSString *str;

    n = js_strlen(s);
    m = (n + 1) * sizeof(jschar);
    news = (jschar *) cx->malloc(m);
    if (!news)
        return NULL;
    memcpy(news, s, m);
    str = js_NewString(cx, news, n);
    if (!str)
        cx->free(news);
    return str;
}

JS_FRIEND_API(const char *)
js_ValueToPrintable(JSContext *cx, jsval v, JSValueToStringFun v2sfun)
{
    JSString *str;

    str = v2sfun(cx, v);
    if (!str)
        return NULL;
    str = js_QuoteString(cx, str, 0);
    if (!str)
        return NULL;
    return js_GetStringBytes(cx, str);
}

JS_FRIEND_API(JSString *)
js_ValueToString(JSContext *cx, jsval v)
{
    JSString *str;

    if (!JSVAL_IS_PRIMITIVE(v) && !DefaultValue(cx, JSVAL_TO_OBJECT(v), JSTYPE_STRING, &v))
        return NULL;

    if (JSVAL_IS_STRING(v)) {
        str = JSVAL_TO_STRING(v);
    } else if (JSVAL_IS_INT(v)) {
        str = js_NumberToString(cx, JSVAL_TO_INT(v));
    } else if (JSVAL_IS_DOUBLE(v)) {
        str = js_NumberToString(cx, *JSVAL_TO_DOUBLE(v));
    } else if (JSVAL_IS_BOOLEAN(v)) {
        str = js_BooleanToString(cx, JSVAL_TO_BOOLEAN(v));
    } else if (JSVAL_IS_NULL(v)) {
        str = ATOM_TO_STRING(cx->runtime->atomState.nullAtom);
    } else {
        str = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
    }
    return str;
}

static inline JSBool
AppendAtom(JSAtom *atom, JSCharBuffer &cb)
{
    JSString *str = ATOM_TO_STRING(atom);
    const jschar *chars;
    size_t length;
    str->getCharsAndLength(chars, length);
    return cb.append(chars, length);
}

/* This function implements E-262-3 section 9.8, toString. */
JS_FRIEND_API(JSBool)
js_ValueToCharBuffer(JSContext *cx, jsval v, JSCharBuffer &cb)
{
    if (!JSVAL_IS_PRIMITIVE(v) && !DefaultValue(cx, JSVAL_TO_OBJECT(v), JSTYPE_STRING, &v))
        return JS_FALSE;

    if (JSVAL_IS_STRING(v)) {
        JSString *str = JSVAL_TO_STRING(v);
        const jschar *chars;
        size_t length;
        str->getCharsAndLength(chars, length);
        return cb.append(chars, length);
    }
    if (JSVAL_IS_NUMBER(v))
        return js_NumberValueToCharBuffer(cx, v, cb);
    if (JSVAL_IS_BOOLEAN(v))
        return js_BooleanToCharBuffer(cx, JSVAL_TO_BOOLEAN(v), cb);
    if (JSVAL_IS_NULL(v))
        return AppendAtom(cx->runtime->atomState.nullAtom, cb);
    JS_ASSERT(JSVAL_IS_VOID(v));
    return AppendAtom(cx->runtime->atomState.typeAtoms[JSTYPE_VOID], cb);
}

JS_FRIEND_API(JSString *)
js_ValueToSource(JSContext *cx, jsval v)
{
    if (JSVAL_IS_VOID(v))
        return ATOM_TO_STRING(cx->runtime->atomState.void0Atom);
    if (JSVAL_IS_STRING(v))
        return js_QuoteString(cx, JSVAL_TO_STRING(v), '"');
    if (JSVAL_IS_PRIMITIVE(v)) {
        /* Special case to preserve negative zero, _contra_ toString. */
        if (JSVAL_IS_DOUBLE(v) && JSDOUBLE_IS_NEGZERO(*JSVAL_TO_DOUBLE(v))) {
            /* NB: _ucNstr rather than _ucstr to indicate non-terminated. */
            static const jschar js_negzero_ucNstr[] = {'-', '0'};

            return js_NewStringCopyN(cx, js_negzero_ucNstr, 2);
        }
        return js_ValueToString(cx, v);
    }

    JSAtom *atom = cx->runtime->atomState.toSourceAtom;
    AutoValueRooter tvr(cx, JSVAL_NULL);
    if (!js_TryMethod(cx, JSVAL_TO_OBJECT(v), atom, 0, NULL, tvr.addr()))
        return NULL;
    return js_ValueToString(cx, tvr.value());
}

/*
 * str is not necessarily a GC thing here.
 */
uint32
js_HashString(JSString *str)
{
    const jschar *s;
    size_t n;
    uint32 h;

    str->getCharsAndLength(s, n);
    for (h = 0; n; s++, n--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;
    return h;
}

/*
 * str is not necessarily a GC thing here.
 */
JSBool JS_FASTCALL
js_EqualStrings(JSString *str1, JSString *str2)
{
    size_t n;
    const jschar *s1, *s2;

    JS_ASSERT(str1);
    JS_ASSERT(str2);

    /* Fast case: pointer equality could be a quick win. */
    if (str1 == str2)
        return JS_TRUE;

    n = str1->length();
    if (n != str2->length())
        return JS_FALSE;

    if (n == 0)
        return JS_TRUE;

    s1 = str1->chars(), s2 = str2->chars();
    do {
        if (*s1 != *s2)
            return JS_FALSE;
        ++s1, ++s2;
    } while (--n != 0);

    return JS_TRUE;
}
JS_DEFINE_CALLINFO_2(extern, BOOL, js_EqualStrings, STRING, STRING, 1, nanojit::ACC_NONE)

int32 JS_FASTCALL
js_CompareStrings(JSString *str1, JSString *str2)
{
    size_t l1, l2, n, i;
    const jschar *s1, *s2;
    intN cmp;

    JS_ASSERT(str1);
    JS_ASSERT(str2);

    /* Fast case: pointer equality could be a quick win. */
    if (str1 == str2)
        return 0;

    str1->getCharsAndLength(s1, l1);
    str2->getCharsAndLength(s2, l2);
    n = JS_MIN(l1, l2);
    for (i = 0; i < n; i++) {
        cmp = s1[i] - s2[i];
        if (cmp != 0)
            return cmp;
    }
    return (intN)(l1 - l2);
}
JS_DEFINE_CALLINFO_2(extern, INT32, js_CompareStrings, STRING, STRING, 1, nanojit::ACC_NONE)

size_t
js_strlen(const jschar *s)
{
    const jschar *t;

    for (t = s; *t != 0; t++)
        continue;
    return (size_t)(t - s);
}

jschar *
js_strchr(const jschar *s, jschar c)
{
    while (*s != 0) {
        if (*s == c)
            return (jschar *)s;
        s++;
    }
    return NULL;
}

jschar *
js_strchr_limit(const jschar *s, jschar c, const jschar *limit)
{
    while (s < limit) {
        if (*s == c)
            return (jschar *)s;
        s++;
    }
    return NULL;
}

jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t *lengthp)
{
    size_t nbytes, nchars, i;
    jschar *chars;
#ifdef DEBUG
    JSBool ok;
#endif

    nbytes = *lengthp;
    if (js_CStringsAreUTF8) {
        if (!js_InflateStringToBuffer(cx, bytes, nbytes, NULL, &nchars))
            goto bad;
        chars = (jschar *) cx->malloc((nchars + 1) * sizeof (jschar));
        if (!chars)
            goto bad;
#ifdef DEBUG
        ok =
#endif
            js_InflateStringToBuffer(cx, bytes, nbytes, chars, &nchars);
        JS_ASSERT(ok);
    } else {
        nchars = nbytes;
        chars = (jschar *) cx->malloc((nchars + 1) * sizeof(jschar));
        if (!chars)
            goto bad;
        for (i = 0; i < nchars; i++)
            chars[i] = (unsigned char) bytes[i];
    }
    *lengthp = nchars;
    chars[nchars] = 0;
    return chars;

  bad:
    /*
     * For compatibility with callers of JS_DecodeBytes we must zero lengthp
     * on errors.
     */
    *lengthp = 0;
    return NULL;
}

/*
 * May be called with null cx by js_GetStringBytes, see below.
 */
char *
js_DeflateString(JSContext *cx, const jschar *chars, size_t nchars)
{
    size_t nbytes, i;
    char *bytes;
#ifdef DEBUG
    JSBool ok;
#endif

    if (js_CStringsAreUTF8) {
        nbytes = js_GetDeflatedStringLength(cx, chars, nchars);
        if (nbytes == (size_t) -1)
            return NULL;
        bytes = (char *) (cx ? cx->malloc(nbytes + 1) : js_malloc(nbytes + 1));
        if (!bytes)
            return NULL;
#ifdef DEBUG
        ok =
#endif
            js_DeflateStringToBuffer(cx, chars, nchars, bytes, &nbytes);
        JS_ASSERT(ok);
    } else {
        nbytes = nchars;
        bytes = (char *) (cx ? cx->malloc(nbytes + 1) : js_malloc(nbytes + 1));
        if (!bytes)
            return NULL;
        for (i = 0; i < nbytes; i++)
            bytes[i] = (char) chars[i];
    }
    bytes[nbytes] = 0;
    return bytes;
}

size_t
js_GetDeflatedStringLength(JSContext *cx, const jschar *chars, size_t nchars)
{
    if (!js_CStringsAreUTF8)
        return nchars;

    return js_GetDeflatedUTF8StringLength(cx, chars, nchars);
}

/*
 * May be called with null cx through js_GetStringBytes, see below.
 */
size_t
js_GetDeflatedUTF8StringLength(JSContext *cx, const jschar *chars, size_t nchars)
{
    size_t nbytes;
    const jschar *end;
    uintN c, c2;
    char buffer[10];

    nbytes = nchars;
    for (end = chars + nchars; chars != end; chars++) {
        c = *chars;
        if (c < 0x80)
            continue;
        if (0xD800 <= c && c <= 0xDFFF) {
            /* Surrogate pair. */
            chars++;
            if (c >= 0xDC00 || chars == end)
                goto bad_surrogate;
            c2 = *chars;
            if (c2 < 0xDC00 || c2 > 0xDFFF)
                goto bad_surrogate;
            c = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
        }
        c >>= 11;
        nbytes++;
        while (c) {
            c >>= 5;
            nbytes++;
        }
    }
    return nbytes;

  bad_surrogate:
    if (cx) {
        JS_snprintf(buffer, 10, "0x%x", c);
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage,
                                     NULL, JSMSG_BAD_SURROGATE_CHAR, buffer);
    }
    return (size_t) -1;
}

JSBool
js_DeflateStringToBuffer(JSContext *cx, const jschar *src, size_t srclen,
                         char *dst, size_t *dstlenp)
{
    size_t dstlen, i;

    dstlen = *dstlenp;
    if (!js_CStringsAreUTF8) {
        if (srclen > dstlen) {
            for (i = 0; i < dstlen; i++)
                dst[i] = (char) src[i];
            if (cx) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BUFFER_TOO_SMALL);
            }
            return JS_FALSE;
        }
        for (i = 0; i < srclen; i++)
            dst[i] = (char) src[i];
        *dstlenp = srclen;
        return JS_TRUE;
    }

    return js_DeflateStringToUTF8Buffer(cx, src, srclen, dst, dstlenp);
}

JSBool
js_DeflateStringToUTF8Buffer(JSContext *cx, const jschar *src, size_t srclen,
                             char *dst, size_t *dstlenp)
{
    size_t dstlen, i, origDstlen, utf8Len;
    jschar c, c2;
    uint32 v;
    uint8 utf8buf[6];

    dstlen = *dstlenp;
    origDstlen = dstlen;
    while (srclen) {
        c = *src++;
        srclen--;
        if ((c >= 0xDC00) && (c <= 0xDFFF))
            goto badSurrogate;
        if (c < 0xD800 || c > 0xDBFF) {
            v = c;
        } else {
            if (srclen < 1)
                goto badSurrogate;
            c2 = *src;
            if ((c2 < 0xDC00) || (c2 > 0xDFFF))
                goto badSurrogate;
            src++;
            srclen--;
            v = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
        }
        if (v < 0x0080) {
            /* no encoding necessary - performance hack */
            if (dstlen == 0)
                goto bufferTooSmall;
            *dst++ = (char) v;
            utf8Len = 1;
        } else {
            utf8Len = js_OneUcs4ToUtf8Char(utf8buf, v);
            if (utf8Len > dstlen)
                goto bufferTooSmall;
            for (i = 0; i < utf8Len; i++)
                *dst++ = (char) utf8buf[i];
        }
        dstlen -= utf8Len;
    }
    *dstlenp = (origDstlen - dstlen);
    return JS_TRUE;

badSurrogate:
    *dstlenp = (origDstlen - dstlen);
    /* Delegate error reporting to the measurement function. */
    if (cx)
        js_GetDeflatedStringLength(cx, src - 1, srclen + 1);
    return JS_FALSE;

bufferTooSmall:
    *dstlenp = (origDstlen - dstlen);
    if (cx) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BUFFER_TOO_SMALL);
    }
    return JS_FALSE;
}

JSBool
js_InflateStringToBuffer(JSContext *cx, const char *src, size_t srclen,
                         jschar *dst, size_t *dstlenp)
{
    size_t dstlen, i;

    if (!js_CStringsAreUTF8) {
        if (dst) {
            dstlen = *dstlenp;
            if (srclen > dstlen) {
                for (i = 0; i < dstlen; i++)
                    dst[i] = (unsigned char) src[i];
                if (cx) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_BUFFER_TOO_SMALL);
                }
                return JS_FALSE;
            }
            for (i = 0; i < srclen; i++)
                dst[i] = (unsigned char) src[i];
        }
        *dstlenp = srclen;
        return JS_TRUE;
    }

    return js_InflateUTF8StringToBuffer(cx, src, srclen, dst, dstlenp);
}

JSBool
js_InflateUTF8StringToBuffer(JSContext *cx, const char *src, size_t srclen,
                             jschar *dst, size_t *dstlenp)
{
    size_t dstlen, origDstlen, offset, j, n;
    uint32 v;

    dstlen = dst ? *dstlenp : (size_t) -1;
    origDstlen = dstlen;
    offset = 0;

    while (srclen) {
        v = (uint8) *src;
        n = 1;
        if (v & 0x80) {
            while (v & (0x80 >> n))
                n++;
            if (n > srclen)
                goto bufferTooSmall;
            if (n == 1 || n > 4)
                goto badCharacter;
            for (j = 1; j < n; j++) {
                if ((src[j] & 0xC0) != 0x80)
                    goto badCharacter;
            }
            v = Utf8ToOneUcs4Char((uint8 *)src, n);
            if (v >= 0x10000) {
                v -= 0x10000;
                if (v > 0xFFFFF || dstlen < 2) {
                    *dstlenp = (origDstlen - dstlen);
                    if (cx) {
                        char buffer[10];
                        JS_snprintf(buffer, 10, "0x%x", v + 0x10000);
                        JS_ReportErrorFlagsAndNumber(cx,
                                                     JSREPORT_ERROR,
                                                     js_GetErrorMessage, NULL,
                                                     JSMSG_UTF8_CHAR_TOO_LARGE,
                                                     buffer);
                    }
                    return JS_FALSE;
                }
                if (dst) {
                    *dst++ = (jschar)((v >> 10) + 0xD800);
                    v = (jschar)((v & 0x3FF) + 0xDC00);
                }
                dstlen--;
            }
        }
        if (!dstlen)
            goto bufferTooSmall;
        if (dst)
            *dst++ = (jschar) v;
        dstlen--;
        offset += n;
        src += n;
        srclen -= n;
    }
    *dstlenp = (origDstlen - dstlen);
    return JS_TRUE;

badCharacter:
    *dstlenp = (origDstlen - dstlen);
    if (cx) {
        char buffer[10];
        JS_snprintf(buffer, 10, "%d", offset);
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                     js_GetErrorMessage, NULL,
                                     JSMSG_MALFORMED_UTF8_CHAR,
                                     buffer);
    }
    return JS_FALSE;

bufferTooSmall:
    *dstlenp = (origDstlen - dstlen);
    if (cx) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BUFFER_TOO_SMALL);
    }
    return JS_FALSE;
}

namespace js {

DeflatedStringCache::DeflatedStringCache()
{
#ifdef JS_THREADSAFE
    lock = NULL;
#endif
}

bool
DeflatedStringCache::init()
{
#ifdef JS_THREADSAFE
    JS_ASSERT(!lock);
    lock = JS_NEW_LOCK();
    if (!lock)
        return false;
#endif

    /*
     * Make room for 2K deflated strings that a typical browser session
     * creates.
     */
    return map.init(2048);
}

DeflatedStringCache::~DeflatedStringCache()
{
#ifdef JS_THREADSAFE
    if (lock)
        JS_DESTROY_LOCK(lock);
#endif
}

void
DeflatedStringCache::sweep(JSContext *cx)
{
    /*
     * We must take a lock even during the GC as JS_GetStringBytes() can be
     * called outside the request.
     */
    JS_ACQUIRE_LOCK(lock);

    for (Map::Enum e(map); !e.empty(); e.popFront()) {
        JSString *str = e.front().key;
        if (js_IsAboutToBeFinalized(str)) {
            char *bytes = e.front().value;
            e.removeFront();

            /*
             * We cannot use cx->free here as bytes may come from the
             * embedding that calls JS_NewString(cx, bytes, length). Those
             * bytes may not be allocated via js_malloc and may not have
             * space for the background free list.
             */
            js_free(bytes);
        }
    }

    JS_RELEASE_LOCK(lock);
}

void
DeflatedStringCache::remove(JSString *str)
{
    JS_ACQUIRE_LOCK(lock);

    Map::Ptr p = map.lookup(str);
    if (p) {
        js_free(p->value);
        map.remove(p);
    }

    JS_RELEASE_LOCK(lock);
}

bool
DeflatedStringCache::setBytes(JSContext *cx, JSString *str, char *bytes)
{
    JS_ACQUIRE_LOCK(lock);

    Map::AddPtr p = map.lookupForAdd(str);
    JS_ASSERT(!p);
    bool ok = map.add(p, str, bytes);

    JS_RELEASE_LOCK(lock);

    if (!ok)
        js_ReportOutOfMemory(cx);
    return ok;
}

char *
DeflatedStringCache::getBytes(JSContext *cx, JSString *str)
{
    JS_ACQUIRE_LOCK(lock);
    Map::AddPtr p = map.lookupForAdd(str);
    char *bytes = p ? p->value : NULL;
    JS_RELEASE_LOCK(lock);

    if (bytes)
        return bytes;

    bytes = js_DeflateString(cx, str->chars(), str->length());
    if (!bytes)
        return NULL;

    /*
     * In the single-threaded case we use the add method as js_DeflateString
     * cannot mutate the map. In particular, it cannot run the GC that may
     * delete entries from the map. But the JS_THREADSAFE version requires to
     * deal with other threads adding the entries to the map.
     */
    char *bytesToFree = NULL;
    JSBool ok;
#ifdef JS_THREADSAFE
    JS_ACQUIRE_LOCK(lock);
    ok = map.relookupOrAdd(p, str, bytes);
    if (ok && p->value != bytes) {
        /* Some other thread has asked for str bytes .*/
        JS_ASSERT(!strcmp(p->value, bytes));
        bytesToFree = bytes;
        bytes = p->value;
    }
    JS_RELEASE_LOCK(lock);
#else  /* !JS_THREADSAFE */
    ok = map.add(p, str, bytes);
#endif
    if (!ok) {
        bytesToFree = bytes;
        bytes = NULL;
        if (cx)
            js_ReportOutOfMemory(cx);
    }

    if (bytesToFree) {
        if (cx)
            cx->free(bytesToFree);
        else
            js_free(bytesToFree);
    }
    return bytes;
}

} /* namespace js */

const char *
js_GetStringBytes(JSContext *cx, JSString *str)
{
    JSRuntime *rt;
    char *bytes;

    if (JSString::isUnitString(str)) {
#ifdef IS_LITTLE_ENDIAN
        /* Unit string data is {c, 0, 0, 0} so we can just cast. */
        bytes = (char *)str->chars();
#else
        /* Unit string data is {0, c, 0, 0} so we can point into the middle. */
        bytes = (char *)str->chars() + 1;
#endif
        return ((*bytes & 0x80) && js_CStringsAreUTF8)
               ? JSString::deflatedUnitStringTable + ((*bytes & 0x7f) * 3)
               : bytes;
    }

    if (JSString::isIntString(str)) {
        /*
         * We must burn some space on deflated int strings to preserve static
         * allocation (which is to say, JSRuntime independence).
         */
        return JSString::deflatedIntStringTable[str - JSString::intStringTable];
    }

    if (cx) {
        rt = cx->runtime;
    } else {
        /* JS_GetStringBytes calls us with null cx. */
        rt = js_GetGCThingRuntime(str);
    }

    return rt->deflatedStringCache->getBytes(cx, str);
}

/*
 * From java.lang.Character.java:
 *
 * The character properties are currently encoded into 32 bits in the
 * following manner:
 *
 * 10 bits      signed offset used for converting case
 *  1 bit       if 1, adding the signed offset converts the character to
 *              lowercase
 *  1 bit       if 1, subtracting the signed offset converts the character to
 *              uppercase
 *  1 bit       if 1, character has a titlecase equivalent (possibly itself)
 *  3 bits      0  may not be part of an identifier
 *              1  ignorable control; may continue a Unicode identifier or JS
 *                 identifier
 *              2  may continue a JS identifier but not a Unicode identifier
 *                 (unused)
 *              3  may continue a Unicode identifier or JS identifier
 *              4  is a JS whitespace character
 *              5  may start or continue a JS identifier;
 *                 may continue but not start a Unicode identifier (_)
 *              6  may start or continue a JS identifier but not a Unicode
 *                 identifier ($)
 *              7  may start or continue a Unicode identifier or JS identifier
 *              Thus:
 *                 5, 6, 7 may start a JS identifier
 *                 1, 2, 3, 5, 6, 7 may continue a JS identifier
 *                 7 may start a Unicode identifier
 *                 1, 3, 5, 7 may continue a Unicode identifier
 *                 1 is ignorable within an identifier
 *                 4 is JS whitespace
 *  2 bits      0  this character has no numeric property
 *              1  adding the digit offset to the character code and then
 *                 masking with 0x1F will produce the desired numeric value
 *              2  this character has a "strange" numeric value
 *              3  a JS supradecimal digit: adding the digit offset to the
 *                 character code, then masking with 0x1F, then adding 10
 *                 will produce the desired numeric value
 *  5 bits      digit offset
 *  1 bit       XML 1.0 name start character
 *  1 bit       XML 1.0 name character
 *  2 bits      reserved for future use
 *  5 bits      character type
 */

/* The X table has 1024 entries for a total of 1024 bytes. */

const uint8 js_X[] = {
  0,   1,   2,   3,   4,   5,   6,   7,  /*  0x0000 */
  8,   9,  10,  11,  12,  13,  14,  15,  /*  0x0200 */
 16,  17,  18,  19,  20,  21,  22,  23,  /*  0x0400 */
 24,  25,  26,  27,  28,  28,  28,  28,  /*  0x0600 */
 28,  28,  28,  28,  29,  30,  31,  32,  /*  0x0800 */
 33,  34,  35,  36,  37,  38,  39,  40,  /*  0x0A00 */
 41,  42,  43,  44,  45,  46,  28,  28,  /*  0x0C00 */
 47,  48,  49,  50,  51,  52,  53,  28,  /*  0x0E00 */
 28,  28,  54,  55,  56,  57,  58,  59,  /*  0x1000 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1200 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1400 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1600 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1800 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1A00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x1C00 */
 60,  60,  61,  62,  63,  64,  65,  66,  /*  0x1E00 */
 67,  68,  69,  70,  71,  72,  73,  74,  /*  0x2000 */
 75,  75,  75,  76,  77,  78,  28,  28,  /*  0x2200 */
 79,  80,  81,  82,  83,  83,  84,  85,  /*  0x2400 */
 86,  85,  28,  28,  87,  88,  89,  28,  /*  0x2600 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x2800 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x2A00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x2C00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x2E00 */
 90,  91,  92,  93,  94,  56,  95,  28,  /*  0x3000 */
 96,  97,  98,  99,  83, 100,  83, 101,  /*  0x3200 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3400 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3600 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3800 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3A00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3C00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x3E00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4000 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4200 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4400 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4600 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4800 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4A00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0x4C00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x4E00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5A00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5C00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x5E00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6A00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6C00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x6E00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7A00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7C00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x7E00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8A00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8C00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x8E00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9A00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0x9C00 */
 56,  56,  56,  56,  56,  56, 102,  28,  /*  0x9E00 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xA000 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xA200 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xA400 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xA600 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xA800 */
 28,  28,  28,  28,  28,  28,  28,  28,  /*  0xAA00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xAC00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xAE00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xB000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xB200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xB400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xB600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xB800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xBA00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xBC00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xBE00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xC000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xC200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xC400 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xC600 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xC800 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xCA00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xCC00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xCE00 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xD000 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xD200 */
 56,  56,  56,  56,  56,  56,  56,  56,  /*  0xD400 */
 56,  56,  56,  56,  56,  56, 103,  28,  /*  0xD600 */
104, 104, 104, 104, 104, 104, 104, 104,  /*  0xD800 */
104, 104, 104, 104, 104, 104, 104, 104,  /*  0xDA00 */
104, 104, 104, 104, 104, 104, 104, 104,  /*  0xDC00 */
104, 104, 104, 104, 104, 104, 104, 104,  /*  0xDE00 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xE000 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xE200 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xE400 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xE600 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xE800 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xEA00 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xEC00 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xEE00 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xF000 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xF200 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xF400 */
105, 105, 105, 105, 105, 105, 105, 105,  /*  0xF600 */
105, 105, 105, 105,  56,  56,  56,  56,  /*  0xF800 */
106,  28,  28,  28, 107, 108, 109, 110,  /*  0xFA00 */
 56,  56,  56,  56, 111, 112, 113, 114,  /*  0xFC00 */
115, 116,  56, 117, 118, 119, 120, 121   /*  0xFE00 */
};

/* The Y table has 7808 entries for a total of 7808 bytes. */

const uint8 js_Y[] = {
  0,   0,   0,   0,   0,   0,   0,   0,  /*    0 */
  0,   1,   1,   1,   1,   1,   0,   0,  /*    0 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    0 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    0 */
  2,   3,   3,   3,   4,   3,   3,   3,  /*    0 */
  5,   6,   3,   7,   3,   8,   3,   3,  /*    0 */
  9,   9,   9,   9,   9,   9,   9,   9,  /*    0 */
  9,   9,   3,   3,   7,   7,   7,   3,  /*    0 */
  3,  10,  10,  10,  10,  10,  10,  10,  /*    1 */
 10,  10,  10,  10,  10,  10,  10,  10,  /*    1 */
 10,  10,  10,  10,  10,  10,  10,  10,  /*    1 */
 10,  10,  10,   5,   3,   6,  11,  12,  /*    1 */
 11,  13,  13,  13,  13,  13,  13,  13,  /*    1 */
 13,  13,  13,  13,  13,  13,  13,  13,  /*    1 */
 13,  13,  13,  13,  13,  13,  13,  13,  /*    1 */
 13,  13,  13,   5,   7,   6,   7,   0,  /*    1 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    2 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    2 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    2 */
  0,   0,   0,   0,   0,   0,   0,   0,  /*    2 */
  2,   3,   4,   4,   4,   4,  15,  15,  /*    2 */
 11,  15,  16,   5,   7,   8,  15,  11,  /*    2 */
 15,   7,  17,  17,  11,  16,  15,   3,  /*    2 */
 11,  18,  16,   6,  19,  19,  19,   3,  /*    2 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*    3 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*    3 */
 20,  20,  20,  20,  20,  20,  20,   7,  /*    3 */
 20,  20,  20,  20,  20,  20,  20,  16,  /*    3 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*    3 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*    3 */
 21,  21,  21,  21,  21,  21,  21,   7,  /*    3 */
 21,  21,  21,  21,  21,  21,  21,  22,  /*    3 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    4 */
 25,  26,  23,  24,  23,  24,  23,  24,  /*    4 */
 16,  23,  24,  23,  24,  23,  24,  23,  /*    4 */
 24,  23,  24,  23,  24,  23,  24,  23,  /*    5 */
 24,  16,  23,  24,  23,  24,  23,  24,  /*    5 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    5 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    5 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    5 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    5 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    5 */
 27,  23,  24,  23,  24,  23,  24,  28,  /*    5 */
 16,  29,  23,  24,  23,  24,  30,  23,  /*    6 */
 24,  31,  31,  23,  24,  16,  32,  32,  /*    6 */
 33,  23,  24,  31,  34,  16,  35,  36,  /*    6 */
 23,  24,  16,  16,  35,  37,  16,  38,  /*    6 */
 23,  24,  23,  24,  23,  24,  38,  23,  /*    6 */
 24,  39,  40,  16,  23,  24,  39,  23,  /*    6 */
 24,  41,  41,  23,  24,  23,  24,  42,  /*    6 */
 23,  24,  16,  40,  23,  24,  40,  40,  /*    6 */
 40,  40,  40,  40,  43,  44,  45,  43,  /*    7 */
 44,  45,  43,  44,  45,  23,  24,  23,  /*    7 */
 24,  23,  24,  23,  24,  23,  24,  23,  /*    7 */
 24,  23,  24,  23,  24,  16,  23,  24,  /*    7 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    7 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    7 */
 16,  43,  44,  45,  23,  24,  46,  46,  /*    7 */
 46,  46,  23,  24,  23,  24,  23,  24,  /*    7 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    8 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    8 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    8 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    9 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*    9 */
 16,  16,  16,  47,  48,  16,  49,  49,  /*    9 */
 50,  50,  16,  51,  16,  16,  16,  16,  /*    9 */
 49,  16,  16,  52,  16,  16,  16,  16,  /*    9 */
 53,  54,  16,  16,  16,  16,  16,  54,  /*    9 */
 16,  16,  55,  16,  16,  16,  16,  16,  /*    9 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*    9 */
 16,  16,  16,  56,  16,  16,  16,  16,  /*   10 */
 56,  16,  57,  57,  16,  16,  16,  16,  /*   10 */
 16,  16,  58,  16,  16,  16,  16,  16,  /*   10 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   10 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   10 */
 16,  46,  46,  46,  46,  46,  46,  46,  /*   10 */
 59,  59,  59,  59,  59,  59,  59,  59,  /*   10 */
 59,  11,  11,  59,  59,  59,  59,  59,  /*   10 */
 59,  59,  11,  11,  11,  11,  11,  11,  /*   11 */
 11,  11,  11,  11,  11,  11,  11,  11,  /*   11 */
 59,  59,  11,  11,  11,  11,  11,  11,  /*   11 */
 11,  11,  11,  11,  11,  11,  11,  46,  /*   11 */
 59,  59,  59,  59,  59,  11,  11,  11,  /*   11 */
 11,  11,  46,  46,  46,  46,  46,  46,  /*   11 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   11 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   11 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   12 */
 60,  60,  60,  60,  60,  60,  46,  46,  /*   13 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   13 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   13 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   13 */
 60,  60,  46,  46,  46,  46,  46,  46,  /*   13 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   13 */
 46,  46,  46,  46,   3,   3,  46,  46,  /*   13 */
 46,  46,  59,  46,  46,  46,   3,  46,  /*   13 */
 46,  46,  46,  46,  11,  11,  61,   3,  /*   14 */
 62,  62,  62,  46,  63,  46,  64,  64,  /*   14 */
 16,  20,  20,  20,  20,  20,  20,  20,  /*   14 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*   14 */
 20,  20,  46,  20,  20,  20,  20,  20,  /*   14 */
 20,  20,  20,  20,  65,  66,  66,  66,  /*   14 */
 16,  21,  21,  21,  21,  21,  21,  21,  /*   14 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*   14 */
 21,  21,  16,  21,  21,  21,  21,  21,  /*   15 */
 21,  21,  21,  21,  67,  68,  68,  46,  /*   15 */
 69,  70,  38,  38,  38,  71,  72,  46,  /*   15 */
 46,  46,  38,  46,  38,  46,  38,  46,  /*   15 */
 38,  46,  23,  24,  23,  24,  23,  24,  /*   15 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   15 */
 73,  74,  16,  40,  46,  46,  46,  46,  /*   15 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   15 */
 46,  75,  75,  75,  75,  75,  75,  75,  /*   16 */
 75,  75,  75,  75,  75,  46,  75,  75,  /*   16 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*   16 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*   16 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*   16 */
 20,  20,  20,  20,  20,  20,  20,  20,  /*   16 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*   16 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*   16 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*   17 */
 21,  21,  21,  21,  21,  21,  21,  21,  /*   17 */
 46,  74,  74,  74,  74,  74,  74,  74,  /*   17 */
 74,  74,  74,  74,  74,  46,  74,  74,  /*   17 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   17 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   17 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   17 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   17 */
 23,  24,  15,  60,  60,  60,  60,  46,  /*   18 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   18 */
 40,  23,  24,  23,  24,  46,  46,  23,  /*   19 */
 24,  46,  46,  23,  24,  46,  46,  46,  /*   19 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   19 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   19 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   19 */
 23,  24,  23,  24,  46,  46,  23,  24,  /*   19 */
 23,  24,  23,  24,  23,  24,  46,  46,  /*   19 */
 23,  24,  46,  46,  46,  46,  46,  46,  /*   19 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   20 */
 46,  76,  76,  76,  76,  76,  76,  76,  /*   20 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   20 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   21 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   21 */
 76,  76,  76,  76,  76,  76,  76,  46,  /*   21 */
 46,  59,   3,   3,   3,   3,   3,   3,  /*   21 */
 46,  77,  77,  77,  77,  77,  77,  77,  /*   21 */
 77,  77,  77,  77,  77,  77,  77,  77,  /*   21 */
 77,  77,  77,  77,  77,  77,  77,  77,  /*   21 */
 77,  77,  77,  77,  77,  77,  77,  77,  /*   21 */
 77,  77,  77,  77,  77,  77,  77,  16,  /*   22 */
 46,   3,  46,  46,  46,  46,  46,  46,  /*   22 */
 46,  60,  60,  60,  60,  60,  60,  60,  /*   22 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   22 */
 60,  60,  46,  60,  60,  60,  60,  60,  /*   22 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   22 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   22 */
 60,  60,  46,  60,  60,  60,   3,  60,  /*   22 */
  3,  60,  60,   3,  60,  46,  46,  46,  /*   23 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   23 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   23 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   23 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   23 */
 40,  40,  40,  46,  46,  46,  46,  46,  /*   23 */
 40,  40,  40,   3,   3,  46,  46,  46,  /*   23 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   23 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   24 */
 46,  46,  46,  46,   3,  46,  46,  46,  /*   24 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   24 */
 46,  46,  46,   3,  46,  46,  46,   3,  /*   24 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   24 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   24 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   24 */
 40,  40,  40,  46,  46,  46,  46,  46,  /*   24 */
 59,  40,  40,  40,  40,  40,  40,  40,  /*   25 */
 40,  40,  40,  60,  60,  60,  60,  60,  /*   25 */
 60,  60,  60,  46,  46,  46,  46,  46,  /*   25 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   25 */
 78,  78,  78,  78,  78,  78,  78,  78,  /*   25 */
 78,  78,   3,   3,   3,   3,  46,  46,  /*   25 */
 60,  40,  40,  40,  40,  40,  40,  40,  /*   25 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   25 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   26 */
 46,  46,  40,  40,  40,  40,  40,  46,  /*   26 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   27 */
 40,  40,  40,  40,  40,  40,  40,  46,  /*   27 */
 40,  40,  40,  40,   3,  40,  60,  60,  /*   27 */
 60,  60,  60,  60,  60,  79,  79,  60,  /*   27 */
 60,  60,  60,  60,  60,  59,  59,  60,  /*   27 */
 60,  15,  60,  60,  60,  60,  46,  46,  /*   27 */
  9,   9,   9,   9,   9,   9,   9,   9,  /*   27 */
  9,   9,  46,  46,  46,  46,  46,  46,  /*   27 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   28 */
 46,  60,  60,  80,  46,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   29 */
 40,  40,  46,  46,  60,  40,  80,  80,  /*   29 */
 80,  60,  60,  60,  60,  60,  60,  60,  /*   30 */
 60,  80,  80,  80,  80,  60,  46,  46,  /*   30 */
 15,  60,  60,  60,  60,  46,  46,  46,  /*   30 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   30 */
 40,  40,  60,  60,   3,   3,  81,  81,  /*   30 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   30 */
  3,  46,  46,  46,  46,  46,  46,  46,  /*   30 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   30 */
 46,  60,  80,  80,  46,  40,  40,  40,  /*   31 */
 40,  40,  40,  40,  40,  46,  46,  40,  /*   31 */
 40,  46,  46,  40,  40,  40,  40,  40,  /*   31 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   31 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   31 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   31 */
 40,  46,  40,  46,  46,  46,  40,  40,  /*   31 */
 40,  40,  46,  46,  60,  46,  80,  80,  /*   31 */
 80,  60,  60,  60,  60,  46,  46,  80,  /*   32 */
 80,  46,  46,  80,  80,  60,  46,  46,  /*   32 */
 46,  46,  46,  46,  46,  46,  46,  80,  /*   32 */
 46,  46,  46,  46,  40,  40,  46,  40,  /*   32 */
 40,  40,  60,  60,  46,  46,  81,  81,  /*   32 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   32 */
 40,  40,   4,   4,  82,  82,  82,  82,  /*   32 */
 19,  83,  15,  46,  46,  46,  46,  46,  /*   32 */
 46,  46,  60,  46,  46,  40,  40,  40,  /*   33 */
 40,  40,  40,  46,  46,  46,  46,  40,  /*   33 */
 40,  46,  46,  40,  40,  40,  40,  40,  /*   33 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   33 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   33 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   33 */
 40,  46,  40,  40,  46,  40,  40,  46,  /*   33 */
 40,  40,  46,  46,  60,  46,  80,  80,  /*   33 */
 80,  60,  60,  46,  46,  46,  46,  60,  /*   34 */
 60,  46,  46,  60,  60,  60,  46,  46,  /*   34 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   34 */
 46,  40,  40,  40,  40,  46,  40,  46,  /*   34 */
 46,  46,  46,  46,  46,  46,  81,  81,  /*   34 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   34 */
 60,  60,  40,  40,  40,  46,  46,  46,  /*   34 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   34 */
 46,  60,  60,  80,  46,  40,  40,  40,  /*   35 */
 40,  40,  40,  40,  46,  40,  46,  40,  /*   35 */
 40,  40,  46,  40,  40,  40,  40,  40,  /*   35 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   35 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   35 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   35 */
 40,  46,  40,  40,  46,  40,  40,  40,  /*   35 */
 40,  40,  46,  46,  60,  40,  80,  80,  /*   35 */
 80,  60,  60,  60,  60,  60,  46,  60,  /*   36 */
 60,  80,  46,  80,  80,  60,  46,  46,  /*   36 */
 15,  46,  46,  46,  46,  46,  46,  46,  /*   36 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   36 */
 40,  46,  46,  46,  46,  46,  81,  81,  /*   36 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   36 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   36 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   36 */
 46,  60,  80,  80,  46,  40,  40,  40,  /*   37 */
 40,  40,  40,  40,  40,  46,  46,  40,  /*   37 */
 40,  46,  46,  40,  40,  40,  40,  40,  /*   37 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   37 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   37 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   37 */
 40,  46,  40,  40,  46,  46,  40,  40,  /*   37 */
 40,  40,  46,  46,  60,  40,  80,  60,  /*   37 */
 80,  60,  60,  60,  46,  46,  46,  80,  /*   38 */
 80,  46,  46,  80,  80,  60,  46,  46,  /*   38 */
 46,  46,  46,  46,  46,  46,  60,  80,  /*   38 */
 46,  46,  46,  46,  40,  40,  46,  40,  /*   38 */
 40,  40,  46,  46,  46,  46,  81,  81,  /*   38 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   38 */
 15,  46,  46,  46,  46,  46,  46,  46,  /*   38 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   38 */
 46,  46,  60,  80,  46,  40,  40,  40,  /*   39 */
 40,  40,  40,  46,  46,  46,  40,  40,  /*   39 */
 40,  46,  40,  40,  40,  40,  46,  46,  /*   39 */
 46,  40,  40,  46,  40,  46,  40,  40,  /*   39 */
 46,  46,  46,  40,  40,  46,  46,  46,  /*   39 */
 40,  40,  40,  46,  46,  46,  40,  40,  /*   39 */
 40,  40,  40,  40,  40,  40,  46,  40,  /*   39 */
 40,  40,  46,  46,  46,  46,  80,  80,  /*   39 */
 60,  80,  80,  46,  46,  46,  80,  80,  /*   40 */
 80,  46,  80,  80,  80,  60,  46,  46,  /*   40 */
 46,  46,  46,  46,  46,  46,  46,  80,  /*   40 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   40 */
 46,  46,  46,  46,  46,  46,  46,  81,  /*   40 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   40 */
 84,  19,  19,  46,  46,  46,  46,  46,  /*   40 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   40 */
 46,  80,  80,  80,  46,  40,  40,  40,  /*   41 */
 40,  40,  40,  40,  40,  46,  40,  40,  /*   41 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   41 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   41 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   41 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   41 */
 40,  40,  40,  40,  46,  40,  40,  40,  /*   41 */
 40,  40,  46,  46,  46,  46,  60,  60,  /*   41 */
 60,  80,  80,  80,  80,  46,  60,  60,  /*   42 */
 60,  46,  60,  60,  60,  60,  46,  46,  /*   42 */
 46,  46,  46,  46,  46,  60,  60,  46,  /*   42 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   42 */
 40,  40,  46,  46,  46,  46,  81,  81,  /*   42 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   42 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   42 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   42 */
 46,  46,  80,  80,  46,  40,  40,  40,  /*   43 */
 40,  40,  40,  40,  40,  46,  40,  40,  /*   43 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   43 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   43 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   43 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   43 */
 40,  40,  40,  40,  46,  40,  40,  40,  /*   43 */
 40,  40,  46,  46,  46,  46,  80,  60,  /*   43 */
 80,  80,  80,  80,  80,  46,  60,  80,  /*   44 */
 80,  46,  80,  80,  60,  60,  46,  46,  /*   44 */
 46,  46,  46,  46,  46,  80,  80,  46,  /*   44 */
 46,  46,  46,  46,  46,  46,  40,  46,  /*   44 */
 40,  40,  46,  46,  46,  46,  81,  81,  /*   44 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   44 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   44 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   44 */
 46,  46,  80,  80,  46,  40,  40,  40,  /*   45 */
 40,  40,  40,  40,  40,  46,  40,  40,  /*   45 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   45 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   45 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   45 */
 40,  46,  40,  40,  40,  40,  40,  40,  /*   45 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   45 */
 40,  40,  46,  46,  46,  46,  80,  80,  /*   45 */
 80,  60,  60,  60,  46,  46,  80,  80,  /*   46 */
 80,  46,  80,  80,  80,  60,  46,  46,  /*   46 */
 46,  46,  46,  46,  46,  46,  46,  80,  /*   46 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   46 */
 40,  40,  46,  46,  46,  46,  81,  81,  /*   46 */
 81,  81,  81,  81,  81,  81,  81,  81,  /*   46 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   46 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   46 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   47 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   47 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   47 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   47 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   47 */
 40,  40,  40,  40,  40,  40,  40,   3,  /*   47 */
 40,  60,  40,  40,  60,  60,  60,  60,  /*   47 */
 60,  60,  60,  46,  46,  46,  46,   4,  /*   47 */
 40,  40,  40,  40,  40,  40,  59,  60,  /*   48 */
 60,  60,  60,  60,  60,  60,  60,  15,  /*   48 */
  9,   9,   9,   9,   9,   9,   9,   9,  /*   48 */
  9,   9,   3,   3,  46,  46,  46,  46,  /*   48 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   48 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   48 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   48 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   48 */
 46,  40,  40,  46,  40,  46,  46,  40,  /*   49 */
 40,  46,  40,  46,  46,  40,  46,  46,  /*   49 */
 46,  46,  46,  46,  40,  40,  40,  40,  /*   49 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   49 */
 46,  40,  40,  40,  46,  40,  46,  40,  /*   49 */
 46,  46,  40,  40,  46,  40,  40,   3,  /*   49 */
 40,  60,  40,  40,  60,  60,  60,  60,  /*   49 */
 60,  60,  46,  60,  60,  40,  46,  46,  /*   49 */
 40,  40,  40,  40,  40,  46,  59,  46,  /*   50 */
 60,  60,  60,  60,  60,  60,  46,  46,  /*   50 */
  9,   9,   9,   9,   9,   9,   9,   9,  /*   50 */
  9,   9,  46,  46,  40,  40,  46,  46,  /*   50 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   50 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   50 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   50 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   50 */
 15,  15,  15,  15,   3,   3,   3,   3,  /*   51 */
  3,   3,   3,   3,   3,   3,   3,   3,  /*   51 */
  3,   3,   3,  15,  15,  15,  15,  15,  /*   51 */
 60,  60,  15,  15,  15,  15,  15,  15,  /*   51 */
 78,  78,  78,  78,  78,  78,  78,  78,  /*   51 */
 78,  78,  85,  85,  85,  85,  85,  85,  /*   51 */
 85,  85,  85,  85,  15,  60,  15,  60,  /*   51 */
 15,  60,   5,   6,   5,   6,  80,  80,  /*   51 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   52 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   52 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   52 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   52 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   52 */
 40,  40,  46,  46,  46,  46,  46,  46,  /*   52 */
 46,  60,  60,  60,  60,  60,  60,  60,  /*   52 */
 60,  60,  60,  60,  60,  60,  60,  80,  /*   52 */
 60,  60,  60,  60,  60,   3,  60,  60,  /*   53 */
 60,  60,  60,  60,  46,  46,  46,  46,  /*   53 */
 60,  60,  60,  60,  60,  60,  46,  60,  /*   53 */
 46,  60,  60,  60,  60,  60,  60,  60,  /*   53 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   53 */
 60,  60,  60,  60,  60,  60,  46,  46,  /*   53 */
 46,  60,  60,  60,  60,  60,  60,  60,  /*   53 */
 46,  60,  46,  46,  46,  46,  46,  46,  /*   53 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   54 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   54 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   54 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   54 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   54 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   54 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   54 */
 76,  76,  76,  76,  76,  76,  76,  76,  /*   54 */
 76,  76,  76,  76,  76,  76,  46,  46,  /*   55 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   55 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   55 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   55 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   55 */
 16,  16,  16,  16,  16,  16,  16,  16,  /*   55 */
 16,  16,  16,  16,  16,  16,  16,  46,  /*   55 */
 46,  46,  46,   3,  46,  46,  46,  46,  /*   55 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   56 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  46,  46,  46,  46,  46,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   57 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  46,  46,  46,  46,  46,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   58 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   59 */
 40,  40,  46,  46,  46,  46,  46,  46,  /*   59 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   60 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  16,  16,  /*   61 */
 16,  16,  16,  16,  46,  46,  46,  46,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   61 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  23,  24,  23,  24,  23,  24,  /*   62 */
 23,  24,  46,  46,  46,  46,  46,  46,  /*   62 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   63 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   63 */
 86,  86,  86,  86,  86,  86,  46,  46,  /*   63 */
 87,  87,  87,  87,  87,  87,  46,  46,  /*   63 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   63 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   63 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   63 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   63 */
 86,  86,  86,  86,  86,  86,  46,  46,  /*   64 */
 87,  87,  87,  87,  87,  87,  46,  46,  /*   64 */
 16,  86,  16,  86,  16,  86,  16,  86,  /*   64 */
 46,  87,  46,  87,  46,  87,  46,  87,  /*   64 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   64 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   64 */
 88,  88,  89,  89,  89,  89,  90,  90,  /*   64 */
 91,  91,  92,  92,  93,  93,  46,  46,  /*   64 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   65 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   65 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   65 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   65 */
 86,  86,  86,  86,  86,  86,  86,  86,  /*   65 */
 87,  87,  87,  87,  87,  87,  87,  87,  /*   65 */
 86,  86,  16,  94,  16,  46,  16,  16,  /*   65 */
 87,  87,  95,  95,  96,  11,  38,  11,  /*   65 */
 11,  11,  16,  94,  16,  46,  16,  16,  /*   66 */
 97,  97,  97,  97,  96,  11,  11,  11,  /*   66 */
 86,  86,  16,  16,  46,  46,  16,  16,  /*   66 */
 87,  87,  98,  98,  46,  11,  11,  11,  /*   66 */
 86,  86,  16,  16,  16,  99,  16,  16,  /*   66 */
 87,  87, 100, 100, 101,  11,  11,  11,  /*   66 */
 46,  46,  16,  94,  16,  46,  16,  16,  /*   66 */
102, 102, 103, 103,  96,  11,  11,  46,  /*   66 */
  2,   2,   2,   2,   2,   2,   2,   2,  /*   67 */
  2,   2,   2,   2, 104, 104, 104, 104,  /*   67 */
  8,   8,   8,   8,   8,   8,   3,   3,  /*   67 */
  5,   6,   5,   5,   5,   6,   5,   5,  /*   67 */
  3,   3,   3,   3,   3,   3,   3,   3,  /*   67 */
105, 106, 104, 104, 104, 104, 104,  46,  /*   67 */
  3,   3,   3,   3,   3,   3,   3,   3,  /*   67 */
  3,   5,   6,   3,   3,   3,   3,  12,  /*   67 */
 12,   3,   3,   3,   7,   5,   6,  46,  /*   68 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   68 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   68 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   68 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   68 */
 46,  46, 104, 104, 104, 104, 104, 104,  /*   68 */
 17,  46,  46,  46,  17,  17,  17,  17,  /*   68 */
 17,  17,   7,   7,   7,   5,   6,  16,  /*   68 */
107, 107, 107, 107, 107, 107, 107, 107,  /*   69 */
107, 107,   7,   7,   7,   5,   6,  46,  /*   69 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   69 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   69 */
  4,   4,   4,   4,   4,   4,   4,   4,  /*   69 */
  4,   4,   4,   4,  46,  46,  46,  46,  /*   69 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   69 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   69 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   70 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   70 */
 60,  60,  60,  60,  60,  60,  60,  60,  /*   70 */
 60,  60,  60,  60,  60,  79,  79,  79,  /*   70 */
 79,  60,  46,  46,  46,  46,  46,  46,  /*   70 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   70 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   70 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   70 */
 15,  15,  38,  15,  15,  15,  15,  38,  /*   71 */
 15,  15,  16,  38,  38,  38,  16,  16,  /*   71 */
 38,  38,  38,  16,  15,  38,  15,  15,  /*   71 */
 38,  38,  38,  38,  38,  38,  15,  15,  /*   71 */
 15,  15,  15,  15,  38,  15,  38,  15,  /*   71 */
 38,  15,  38,  38,  38,  38,  16,  16,  /*   71 */
 38,  38,  15,  38,  16,  40,  40,  40,  /*   71 */
 40,  46,  46,  46,  46,  46,  46,  46,  /*   71 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   72 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   72 */
 46,  46,  46,  19,  19,  19,  19,  19,  /*   72 */
 19,  19,  19,  19,  19,  19,  19, 108,  /*   72 */
109, 109, 109, 109, 109, 109, 109, 109,  /*   72 */
109, 109, 109, 109, 110, 110, 110, 110,  /*   72 */
111, 111, 111, 111, 111, 111, 111, 111,  /*   72 */
111, 111, 111, 111, 112, 112, 112, 112,  /*   72 */
113, 113, 113,  46,  46,  46,  46,  46,  /*   73 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   73 */
  7,   7,   7,   7,   7,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   73 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   74 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   74 */
 15,  15,   7,  15,   7,  15,  15,  15,  /*   74 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   74 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   74 */
 15,  15,  15,  46,  46,  46,  46,  46,  /*   74 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   74 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   74 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   75 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,   7,   7,   7,   7,   7,   7,  /*   76 */
  7,   7,  46,  46,  46,  46,  46,  46,  /*   76 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   76 */
 15,  46,  15,  15,  15,  15,  15,  15,  /*   77 */
  7,   7,   7,   7,  15,  15,  15,  15,  /*   77 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   77 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   77 */
  7,   7,  15,  15,  15,  15,  15,  15,  /*   77 */
 15,   5,   6,  15,  15,  15,  15,  15,  /*   77 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   77 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   77 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   78 */
 15,  15,  15,  46,  46,  46,  46,  46,  /*   78 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   79 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   79 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   79 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   79 */
 15,  15,  15,  15,  15,  46,  46,  46,  /*   79 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   79 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   79 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   79 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   80 */
 15,  15,  15,  46,  46,  46,  46,  46,  /*   80 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   80 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   80 */
114, 114, 114, 114, 114, 114, 114, 114,  /*   80 */
114, 114, 114, 114, 114, 114, 114, 114,  /*   80 */
114, 114, 114, 114,  82,  82,  82,  82,  /*   80 */
 82,  82,  82,  82,  82,  82,  82,  82,  /*   80 */
 82,  82,  82,  82,  82,  82,  82,  82,  /*   81 */
115, 115, 115, 115, 115, 115, 115, 115,  /*   81 */
115, 115, 115, 115, 115, 115, 115, 115,  /*   81 */
115, 115, 115, 115,  15,  15,  15,  15,  /*   81 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   81 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   81 */
 15,  15,  15,  15,  15,  15, 116, 116,  /*   81 */
116, 116, 116, 116, 116, 116, 116, 116,  /*   81 */
116, 116, 116, 116, 116, 116, 116, 116,  /*   82 */
116, 116, 116, 116, 116, 116, 116, 116,  /*   82 */
117, 117, 117, 117, 117, 117, 117, 117,  /*   82 */
117, 117, 117, 117, 117, 117, 117, 117,  /*   82 */
117, 117, 117, 117, 117, 117, 117, 117,  /*   82 */
117, 117, 118,  46,  46,  46,  46,  46,  /*   82 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   82 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   82 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   83 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  46,  46,  /*   84 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   84 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   85 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   85 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   85 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  46,  46,  46,  46,  /*   86 */
 46,  46,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   86 */
 46,  15,  15,  15,  15,  46,  15,  15,  /*   87 */
 15,  15,  46,  46,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 46,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   87 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   88 */
 15,  15,  15,  15,  46,  15,  46,  15,  /*   88 */
 15,  15,  15,  46,  46,  46,  15,  46,  /*   88 */
 15,  15,  15,  15,  15,  15,  15,  46,  /*   88 */
 46,  15,  15,  15,  15,  15,  15,  15,  /*   88 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   88 */
 46,  46,  46,  46,  46,  46, 119, 119,  /*   88 */
119, 119, 119, 119, 119, 119, 119, 119,  /*   88 */
114, 114, 114, 114, 114, 114, 114, 114,  /*   89 */
114, 114,  83,  83,  83,  83,  83,  83,  /*   89 */
 83,  83,  83,  83,  15,  46,  46,  46,  /*   89 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   89 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   89 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   89 */
 46,  15,  15,  15,  15,  15,  15,  15,  /*   89 */
 15,  15,  15,  15,  15,  15,  15,  46,  /*   89 */
  2,   3,   3,   3,  15,  59,   3, 120,  /*   90 */
  5,   6,   5,   6,   5,   6,   5,   6,  /*   90 */
  5,   6,  15,  15,   5,   6,   5,   6,  /*   90 */
  5,   6,   5,   6,   8,   5,   6,   5,  /*   90 */
 15, 121, 121, 121, 121, 121, 121, 121,  /*   90 */
121, 121,  60,  60,  60,  60,  60,  60,  /*   90 */
  8,  59,  59,  59,  59,  59,  15,  15,  /*   90 */
 46,  46,  46,  46,  46,  46,  46,  15,  /*   90 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   91 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  46,  46,  46,  /*   92 */
 46,  60,  60,  59,  59,  59,  59,  46,  /*   92 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   92 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   93 */
 40,  40,  40,   3,  59,  59,  59,  46,  /*   93 */
 46,  46,  46,  46,  46,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  46,  46,  46,  /*   94 */
 46,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   94 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*   95 */
 40,  40,  40,  40,  40,  40,  40,  46,  /*   95 */
 15,  15,  85,  85,  85,  85,  15,  15,  /*   95 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   95 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   95 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   95 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   95 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   95 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  15,  46,  46,  46,  /*   96 */
 85,  85,  85,  85,  85,  85,  85,  85,  /*   96 */
 85,  85,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   96 */
 15,  15,  15,  15,  46,  46,  46,  46,  /*   97 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   97 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   97 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   97 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   97 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   97 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   97 */
 15,  15,  15,  15,  46,  46,  46,  15,  /*   97 */
114, 114, 114, 114, 114, 114, 114, 114,  /*   98 */
114, 114,  15,  15,  15,  15,  15,  15,  /*   98 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   98 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   98 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   98 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   98 */
 15,  46,  46,  46,  46,  46,  46,  46,  /*   98 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*   98 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  46,  46,  46,  46,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  46,  /*   99 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  46,  /*  100 */
 46,  46,  46,  15,  15,  15,  15,  15,  /*  100 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  46,  46,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  15,  /*  101 */
 15,  15,  15,  15,  15,  15,  15,  46,  /*  101 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  102 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  102 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  102 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  102 */
 40,  40,  40,  40,  40,  40,  46,  46,  /*  102 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  102 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  102 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  102 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  103 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  103 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  103 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  103 */
 40,  40,  40,  40,  46,  46,  46,  46,  /*  103 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  103 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  103 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  103 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
122, 122, 122, 122, 122, 122, 122, 122,  /*  104 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
123, 123, 123, 123, 123, 123, 123, 123,  /*  105 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  106 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  106 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  106 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  106 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  106 */
 40,  40,  40,  40,  40,  40,  46,  46,  /*  106 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  106 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  106 */
 16,  16,  16,  16,  16,  16,  16,  46,  /*  107 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  107 */
 46,  46,  46,  16,  16,  16,  16,  16,  /*  107 */
 46,  46,  46,  46,  46,  46,  60,  40,  /*  107 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  107 */
 40,   7,  40,  40,  40,  40,  40,  40,  /*  107 */
 40,  40,  40,  40,  40,  40,  40,  46,  /*  107 */
 40,  40,  40,  40,  40,  46,  40,  46,  /*  107 */
 40,  40,  46,  40,  40,  46,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  108 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  109 */
 40,  40,  46,  46,  46,  46,  46,  46,  /*  109 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  109 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  110 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  110 */
 46,  46,  46,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  110 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  111 */
 40,  40,  40,  40,  40,  40,   5,   6,  /*  111 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  112 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  112 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 46,  46,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  113 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  114 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  114 */
 40,  40,  40,  40,  46,  46,  46,  46,  /*  114 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  115 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  115 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  115 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  115 */
 60,  60,  60,  60,  46,  46,  46,  46,  /*  115 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  115 */
  3,   8,   8,  12,  12,   5,   6,   5,  /*  115 */
  6,   5,   6,   5,   6,   5,   6,   5,  /*  115 */
  6,   5,   6,   5,   6,  46,  46,  46,  /*  116 */
 46,   3,   3,   3,   3,  12,  12,  12,  /*  116 */
  3,   3,   3,  46,   3,   3,   3,   3,  /*  116 */
  8,   5,   6,   5,   6,   5,   6,   3,  /*  116 */
  3,   3,   7,   8,   7,   7,   7,  46,  /*  116 */
  3,   4,   3,   3,  46,  46,  46,  46,  /*  116 */
 40,  40,  40,  46,  40,  46,  40,  40,  /*  116 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  116 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  117 */
 40,  40,  40,  40,  40,  46,  46, 104,  /*  117 */
 46,   3,   3,   3,   4,   3,   3,   3,  /*  118 */
  5,   6,   3,   7,   3,   8,   3,   3,  /*  118 */
  9,   9,   9,   9,   9,   9,   9,   9,  /*  118 */
  9,   9,   3,   3,   7,   7,   7,   3,  /*  118 */
  3,  10,  10,  10,  10,  10,  10,  10,  /*  118 */
 10,  10,  10,  10,  10,  10,  10,  10,  /*  118 */
 10,  10,  10,  10,  10,  10,  10,  10,  /*  118 */
 10,  10,  10,   5,   3,   6,  11,  12,  /*  118 */
 11,  13,  13,  13,  13,  13,  13,  13,  /*  119 */
 13,  13,  13,  13,  13,  13,  13,  13,  /*  119 */
 13,  13,  13,  13,  13,  13,  13,  13,  /*  119 */
 13,  13,  13,   5,   7,   6,   7,  46,  /*  119 */
 46,   3,   5,   6,   3,   3,  40,  40,  /*  119 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  119 */
 59,  40,  40,  40,  40,  40,  40,  40,  /*  119 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  119 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  59,  59,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  40,  /*  120 */
 40,  40,  40,  40,  40,  40,  40,  46,  /*  120 */
 46,  46,  40,  40,  40,  40,  40,  40,  /*  121 */
 46,  46,  40,  40,  40,  40,  40,  40,  /*  121 */
 46,  46,  40,  40,  40,  40,  40,  40,  /*  121 */
 46,  46,  40,  40,  40,  46,  46,  46,  /*  121 */
  4,   4,   7,  11,  15,   4,   4,  46,  /*  121 */
  7,   7,   7,   7,   7,  15,  15,  46,  /*  121 */
 46,  46,  46,  46,  46,  46,  46,  46,  /*  121 */
 46,  46,  46,  46,  46,  15,  46,  46   /*  121 */
};

/* The A table has 124 entries for a total of 496 bytes. */

const uint32 js_A[] = {
0x0001000F,  /*    0   Cc, ignorable */
0x0004000F,  /*    1   Cc, whitespace */
0x0004000C,  /*    2   Zs, whitespace */
0x00000018,  /*    3   Po */
0x0006001A,  /*    4   Sc, currency */
0x00000015,  /*    5   Ps */
0x00000016,  /*    6   Pe */
0x00000019,  /*    7   Sm */
0x00000014,  /*    8   Pd */
0x00036089,  /*    9   Nd, identifier part, decimal 16 */
0x0827FF81,  /*   10   Lu, hasLower (add 32), identifier start, supradecimal 31 */
0x0000001B,  /*   11   Sk */
0x00050017,  /*   12   Pc, underscore */
0x0817FF82,  /*   13   Ll, hasUpper (subtract 32), identifier start, supradecimal 31 */
0x0000000C,  /*   14   Zs */
0x0000001C,  /*   15   So */
0x00070182,  /*   16   Ll, identifier start */
0x0000600B,  /*   17   No, decimal 16 */
0x0000500B,  /*   18   No, decimal 8 */
0x0000800B,  /*   19   No, strange */
0x08270181,  /*   20   Lu, hasLower (add 32), identifier start */
0x08170182,  /*   21   Ll, hasUpper (subtract 32), identifier start */
0xE1D70182,  /*   22   Ll, hasUpper (subtract -121), identifier start */
0x00670181,  /*   23   Lu, hasLower (add 1), identifier start */
0x00570182,  /*   24   Ll, hasUpper (subtract 1), identifier start */
0xCE670181,  /*   25   Lu, hasLower (add -199), identifier start */
0x3A170182,  /*   26   Ll, hasUpper (subtract 232), identifier start */
0xE1E70181,  /*   27   Lu, hasLower (add -121), identifier start */
0x4B170182,  /*   28   Ll, hasUpper (subtract 300), identifier start */
0x34A70181,  /*   29   Lu, hasLower (add 210), identifier start */
0x33A70181,  /*   30   Lu, hasLower (add 206), identifier start */
0x33670181,  /*   31   Lu, hasLower (add 205), identifier start */
0x32A70181,  /*   32   Lu, hasLower (add 202), identifier start */
0x32E70181,  /*   33   Lu, hasLower (add 203), identifier start */
0x33E70181,  /*   34   Lu, hasLower (add 207), identifier start */
0x34E70181,  /*   35   Lu, hasLower (add 211), identifier start */
0x34670181,  /*   36   Lu, hasLower (add 209), identifier start */
0x35670181,  /*   37   Lu, hasLower (add 213), identifier start */
0x00070181,  /*   38   Lu, identifier start */
0x36A70181,  /*   39   Lu, hasLower (add 218), identifier start */
0x00070185,  /*   40   Lo, identifier start */
0x36670181,  /*   41   Lu, hasLower (add 217), identifier start */
0x36E70181,  /*   42   Lu, hasLower (add 219), identifier start */
0x00AF0181,  /*   43   Lu, hasLower (add 2), hasTitle, identifier start */
0x007F0183,  /*   44   Lt, hasUpper (subtract 1), hasLower (add 1), hasTitle, identifier start */
0x009F0182,  /*   45   Ll, hasUpper (subtract 2), hasTitle, identifier start */
0x00000000,  /*   46   unassigned */
0x34970182,  /*   47   Ll, hasUpper (subtract 210), identifier start */
0x33970182,  /*   48   Ll, hasUpper (subtract 206), identifier start */
0x33570182,  /*   49   Ll, hasUpper (subtract 205), identifier start */
0x32970182,  /*   50   Ll, hasUpper (subtract 202), identifier start */
0x32D70182,  /*   51   Ll, hasUpper (subtract 203), identifier start */
0x33D70182,  /*   52   Ll, hasUpper (subtract 207), identifier start */
0x34570182,  /*   53   Ll, hasUpper (subtract 209), identifier start */
0x34D70182,  /*   54   Ll, hasUpper (subtract 211), identifier start */
0x35570182,  /*   55   Ll, hasUpper (subtract 213), identifier start */
0x36970182,  /*   56   Ll, hasUpper (subtract 218), identifier start */
0x36570182,  /*   57   Ll, hasUpper (subtract 217), identifier start */
0x36D70182,  /*   58   Ll, hasUpper (subtract 219), identifier start */
0x00070084,  /*   59   Lm, identifier start */
0x00030086,  /*   60   Mn, identifier part */
0x09A70181,  /*   61   Lu, hasLower (add 38), identifier start */
0x09670181,  /*   62   Lu, hasLower (add 37), identifier start */
0x10270181,  /*   63   Lu, hasLower (add 64), identifier start */
0x0FE70181,  /*   64   Lu, hasLower (add 63), identifier start */
0x09970182,  /*   65   Ll, hasUpper (subtract 38), identifier start */
0x09570182,  /*   66   Ll, hasUpper (subtract 37), identifier start */
0x10170182,  /*   67   Ll, hasUpper (subtract 64), identifier start */
0x0FD70182,  /*   68   Ll, hasUpper (subtract 63), identifier start */
0x0F970182,  /*   69   Ll, hasUpper (subtract 62), identifier start */
0x0E570182,  /*   70   Ll, hasUpper (subtract 57), identifier start */
0x0BD70182,  /*   71   Ll, hasUpper (subtract 47), identifier start */
0x0D970182,  /*   72   Ll, hasUpper (subtract 54), identifier start */
0x15970182,  /*   73   Ll, hasUpper (subtract 86), identifier start */
0x14170182,  /*   74   Ll, hasUpper (subtract 80), identifier start */
0x14270181,  /*   75   Lu, hasLower (add 80), identifier start */
0x0C270181,  /*   76   Lu, hasLower (add 48), identifier start */
0x0C170182,  /*   77   Ll, hasUpper (subtract 48), identifier start */
0x00034089,  /*   78   Nd, identifier part, decimal 0 */
0x00000087,  /*   79   Me */
0x00030088,  /*   80   Mc, identifier part */
0x00037489,  /*   81   Nd, identifier part, decimal 26 */
0x00005A0B,  /*   82   No, decimal 13 */
0x00006E0B,  /*   83   No, decimal 23 */
0x0000740B,  /*   84   No, decimal 26 */
0x0000000B,  /*   85   No */
0xFE170182,  /*   86   Ll, hasUpper (subtract -8), identifier start */
0xFE270181,  /*   87   Lu, hasLower (add -8), identifier start */
0xED970182,  /*   88   Ll, hasUpper (subtract -74), identifier start */
0xEA970182,  /*   89   Ll, hasUpper (subtract -86), identifier start */
0xE7170182,  /*   90   Ll, hasUpper (subtract -100), identifier start */
0xE0170182,  /*   91   Ll, hasUpper (subtract -128), identifier start */
0xE4170182,  /*   92   Ll, hasUpper (subtract -112), identifier start */
0xE0970182,  /*   93   Ll, hasUpper (subtract -126), identifier start */
0xFDD70182,  /*   94   Ll, hasUpper (subtract -9), identifier start */
0xEDA70181,  /*   95   Lu, hasLower (add -74), identifier start */
0xFDE70181,  /*   96   Lu, hasLower (add -9), identifier start */
0xEAA70181,  /*   97   Lu, hasLower (add -86), identifier start */
0xE7270181,  /*   98   Lu, hasLower (add -100), identifier start */
0xFE570182,  /*   99   Ll, hasUpper (subtract -7), identifier start */
0xE4270181,  /*  100   Lu, hasLower (add -112), identifier start */
0xFE670181,  /*  101   Lu, hasLower (add -7), identifier start */
0xE0270181,  /*  102   Lu, hasLower (add -128), identifier start */
0xE0A70181,  /*  103   Lu, hasLower (add -126), identifier start */
0x00010010,  /*  104   Cf, ignorable */
0x0004000D,  /*  105   Zl, whitespace */
0x0004000E,  /*  106   Zp, whitespace */
0x0000400B,  /*  107   No, decimal 0 */
0x0000440B,  /*  108   No, decimal 2 */
0x0427438A,  /*  109   Nl, hasLower (add 16), identifier start, decimal 1 */
0x0427818A,  /*  110   Nl, hasLower (add 16), identifier start, strange */
0x0417638A,  /*  111   Nl, hasUpper (subtract 16), identifier start, decimal 17 */
0x0417818A,  /*  112   Nl, hasUpper (subtract 16), identifier start, strange */
0x0007818A,  /*  113   Nl, identifier start, strange */
0x0000420B,  /*  114   No, decimal 1 */
0x0000720B,  /*  115   No, decimal 25 */
0x06A0001C,  /*  116   So, hasLower (add 26) */
0x0690001C,  /*  117   So, hasUpper (subtract 26) */
0x00006C0B,  /*  118   No, decimal 22 */
0x0000560B,  /*  119   No, decimal 11 */
0x0007738A,  /*  120   Nl, identifier start, decimal 25 */
0x0007418A,  /*  121   Nl, identifier start, decimal 0 */
0x00000013,  /*  122   Cs */
0x00000012   /*  123   Co */
};

const jschar js_uriReservedPlusPound_ucstr[] =
    {';', '/', '?', ':', '@', '&', '=', '+', '$', ',', '#', 0};
const jschar js_uriUnescaped_ucstr[] =
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
     'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
     'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
     'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
     'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
     '-', '_', '.', '!', '~', '*', '\'', '(', ')', 0};

/*
 * This table allows efficient testing for the regular expression \w which is
 * defined by ECMA-262 15.10.2.6 to be [0-9A-Z_a-z].
 */
const bool js_alnum[] = {
/*       0      1      2      3      4      5      5      7      8      9      */
/*  0 */ false, false, false, false, false, false, false, false, false, false,
/*  1 */ false, false, false, false, false, false, false, false, false, false,
/*  2 */ false, false, false, false, false, false, false, false, false, false,
/*  3 */ false, false, false, false, false, false, false, false, false, false,
/*  4 */ false, false, false, false, false, false, false, false, true,  true,
/*  5 */ true,  true,  true,  true,  true,  true,  true,  true,  false, false,
/*  6 */ false, false, false, false, false, true,  true,  true,  true,  true,
/*  7 */ true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
/*  8 */ true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
/*  9 */ true,  false, false, false, false, true,  false, true,  true,  true,
/* 10 */ true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
/* 11 */ true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
/* 12 */ true,  true,  true,  false, false, false, false, false
};

#define URI_CHUNK 64U

static inline bool
TransferBufferToString(JSContext *cx, JSCharBuffer &cb, jsval *rval)
{
    JSString *str = js_NewStringFromCharBuffer(cx, cb);
    if (!str)
        return false;
    *rval = STRING_TO_JSVAL(str);
    return true;;
}

/*
 * ECMA 3, 15.1.3 URI Handling Function Properties
 *
 * The following are implementations of the algorithms
 * given in the ECMA specification for the hidden functions
 * 'Encode' and 'Decode'.
 */
static JSBool
Encode(JSContext *cx, JSString *str, const jschar *unescapedSet,
       const jschar *unescapedSet2, jsval *rval)
{
    size_t length, j, k, L;
    JSCharBuffer cb(cx);
    const jschar *chars;
    jschar c, c2;
    uint32 v;
    uint8 utf8buf[4];
    jschar hexBuf[4];
    static const char HexDigits[] = "0123456789ABCDEF"; /* NB: uppercase */

    str->getCharsAndLength(chars, length);
    if (length == 0) {
        *rval = STRING_TO_JSVAL(cx->runtime->emptyString);
        return JS_TRUE;
    }

    /* From this point the control must goto bad on failures. */
    hexBuf[0] = '%';
    hexBuf[3] = 0;
    for (k = 0; k < length; k++) {
        c = chars[k];
        if (js_strchr(unescapedSet, c) ||
            (unescapedSet2 && js_strchr(unescapedSet2, c))) {
            if (!cb.append(c))
                return JS_FALSE;
        } else {
            if ((c >= 0xDC00) && (c <= 0xDFFF)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_URI, NULL);
                return JS_FALSE;
            }
            if (c < 0xD800 || c > 0xDBFF) {
                v = c;
            } else {
                k++;
                if (k == length) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_URI, NULL);
                    return JS_FALSE;
                }
                c2 = chars[k];
                if ((c2 < 0xDC00) || (c2 > 0xDFFF)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_URI, NULL);
                    return JS_FALSE;
                }
                v = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
            }
            L = js_OneUcs4ToUtf8Char(utf8buf, v);
            for (j = 0; j < L; j++) {
                hexBuf[1] = HexDigits[utf8buf[j] >> 4];
                hexBuf[2] = HexDigits[utf8buf[j] & 0xf];
                if (!cb.append(hexBuf, 3))
                    return JS_FALSE;
            }
        }
    }

    return TransferBufferToString(cx, cb, rval);
}

static JSBool
Decode(JSContext *cx, JSString *str, const jschar *reservedSet, jsval *rval)
{
    size_t length, start, k;
    JSCharBuffer cb(cx);
    const jschar *chars;
    jschar c, H;
    uint32 v;
    jsuint B;
    uint8 octets[4];
    intN j, n;

    str->getCharsAndLength(chars, length);
    if (length == 0) {
        *rval = STRING_TO_JSVAL(cx->runtime->emptyString);
        return JS_TRUE;
    }

    /* From this point the control must goto bad on failures. */
    for (k = 0; k < length; k++) {
        c = chars[k];
        if (c == '%') {
            start = k;
            if ((k + 2) >= length)
                goto report_bad_uri;
            if (!JS7_ISHEX(chars[k+1]) || !JS7_ISHEX(chars[k+2]))
                goto report_bad_uri;
            B = JS7_UNHEX(chars[k+1]) * 16 + JS7_UNHEX(chars[k+2]);
            k += 2;
            if (!(B & 0x80)) {
                c = (jschar)B;
            } else {
                n = 1;
                while (B & (0x80 >> n))
                    n++;
                if (n == 1 || n > 4)
                    goto report_bad_uri;
                octets[0] = (uint8)B;
                if (k + 3 * (n - 1) >= length)
                    goto report_bad_uri;
                for (j = 1; j < n; j++) {
                    k++;
                    if (chars[k] != '%')
                        goto report_bad_uri;
                    if (!JS7_ISHEX(chars[k+1]) || !JS7_ISHEX(chars[k+2]))
                        goto report_bad_uri;
                    B = JS7_UNHEX(chars[k+1]) * 16 + JS7_UNHEX(chars[k+2]);
                    if ((B & 0xC0) != 0x80)
                        goto report_bad_uri;
                    k += 2;
                    octets[j] = (char)B;
                }
                v = Utf8ToOneUcs4Char(octets, n);
                if (v >= 0x10000) {
                    v -= 0x10000;
                    if (v > 0xFFFFF)
                        goto report_bad_uri;
                    c = (jschar)((v & 0x3FF) + 0xDC00);
                    H = (jschar)((v >> 10) + 0xD800);
                    if (!cb.append(H))
                        return JS_FALSE;
                } else {
                    c = (jschar)v;
                }
            }
            if (js_strchr(reservedSet, c)) {
                if (!cb.append(chars + start, k - start + 1))
                    return JS_FALSE;
            } else {
                if (!cb.append(c))
                    return JS_FALSE;
            }
        } else {
            if (!cb.append(c))
                return JS_FALSE;
        }
    }

    return TransferBufferToString(cx, cb, rval);

  report_bad_uri:
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_URI);
    /* FALL THROUGH */

    return JS_FALSE;
}

static JSBool
str_decodeURI(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    str = ArgToRootedString(cx, argc, vp, 0);
    if (!str)
        return JS_FALSE;
    return Decode(cx, str, js_uriReservedPlusPound_ucstr, vp);
}

static JSBool
str_decodeURI_Component(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    str = ArgToRootedString(cx, argc, vp, 0);
    if (!str)
        return JS_FALSE;
    return Decode(cx, str, js_empty_ucstr, vp);
}

static JSBool
str_encodeURI(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    str = ArgToRootedString(cx, argc, vp, 0);
    if (!str)
        return JS_FALSE;
    return Encode(cx, str, js_uriReservedPlusPound_ucstr, js_uriUnescaped_ucstr,
                  vp);
}

static JSBool
str_encodeURI_Component(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    str = ArgToRootedString(cx, argc, vp, 0);
    if (!str)
        return JS_FALSE;
    return Encode(cx, str, js_uriUnescaped_ucstr, NULL, vp);
}

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 4 bytes long.  Return the number of UTF-8 bytes of data written.
 */
int
js_OneUcs4ToUtf8Char(uint8 *utf8Buffer, uint32 ucs4Char)
{
    int utf8Length = 1;

    JS_ASSERT(ucs4Char <= 0x10FFFF);
    if (ucs4Char < 0x80) {
        *utf8Buffer = (uint8)ucs4Char;
    } else {
        int i;
        uint32 a = ucs4Char >> 11;
        utf8Length = 2;
        while (a) {
            a >>= 5;
            utf8Length++;
        }
        i = utf8Length;
        while (--i) {
            utf8Buffer[i] = (uint8)((ucs4Char & 0x3F) | 0x80);
            ucs4Char >>= 6;
        }
        *utf8Buffer = (uint8)(0x100 - (1 << (8-utf8Length)) + ucs4Char);
    }
    return utf8Length;
}

/*
 * Convert a utf8 character sequence into a UCS-4 character and return that
 * character.  It is assumed that the caller already checked that the sequence
 * is valid.
 */
static uint32
Utf8ToOneUcs4Char(const uint8 *utf8Buffer, int utf8Length)
{
    uint32 ucs4Char;
    uint32 minucs4Char;
    /* from Unicode 3.1, non-shortest form is illegal */
    static const uint32 minucs4Table[] = {
        0x00000080, 0x00000800, 0x00010000
    };

    JS_ASSERT(utf8Length >= 1 && utf8Length <= 4);
    if (utf8Length == 1) {
        ucs4Char = *utf8Buffer;
        JS_ASSERT(!(ucs4Char & 0x80));
    } else {
        JS_ASSERT((*utf8Buffer & (0x100 - (1 << (7-utf8Length)))) ==
                  (0x100 - (1 << (8-utf8Length))));
        ucs4Char = *utf8Buffer++ & ((1<<(7-utf8Length))-1);
        minucs4Char = minucs4Table[utf8Length-2];
        while (--utf8Length) {
            JS_ASSERT((*utf8Buffer & 0xC0) == 0x80);
            ucs4Char = ucs4Char<<6 | (*utf8Buffer++ & 0x3F);
        }
        if (JS_UNLIKELY(ucs4Char < minucs4Char)) {
            ucs4Char = OVERLONG_UTF8;
        } else if (ucs4Char == 0xFFFE || ucs4Char == 0xFFFF) {
            ucs4Char = 0xFFFD;
        }
    }
    return ucs4Char;
}

#if defined DEBUG || defined JS_DUMP_CONSERVATIVE_GC_ROOTS

JS_FRIEND_API(size_t)
js_PutEscapedStringImpl(char *buffer, size_t bufferSize, FILE *fp,
                        JSString *str, uint32 quote)
{
    const jschar *chars, *charsEnd;
    size_t n;
    const char *escape;
    char c;
    uintN u, hex, shift;
    enum {
        STOP, FIRST_QUOTE, LAST_QUOTE, CHARS, ESCAPE_START, ESCAPE_MORE
    } state;

    JS_ASSERT(quote == 0 || quote == '\'' || quote == '"');
    JS_ASSERT_IF(buffer, bufferSize != 0);
    JS_ASSERT_IF(!buffer, bufferSize == 0);
    JS_ASSERT_IF(fp, !buffer);

    str->getCharsAndEnd(chars, charsEnd);
    n = 0;
    --bufferSize;
    state = FIRST_QUOTE;
    shift = 0;
    hex = 0;
    u = 0;
    c = 0;  /* to quell GCC warnings */

    for (;;) {
        switch (state) {
          case STOP:
            goto stop;
          case FIRST_QUOTE:
            state = CHARS;
            goto do_quote;
          case LAST_QUOTE:
            state = STOP;
          do_quote:
            if (quote == 0)
                continue;
            c = (char)quote;
            break;
          case CHARS:
            if (chars == charsEnd) {
                state = LAST_QUOTE;
                continue;
            }
            u = *chars++;
            if (u < ' ') {
                if (u != 0) {
                    escape = strchr(js_EscapeMap, (int)u);
                    if (escape) {
                        u = escape[1];
                        goto do_escape;
                    }
                }
                goto do_hex_escape;
            }
            if (u < 127) {
                if (u == quote || u == '\\')
                    goto do_escape;
                c = (char)u;
            } else if (u < 0x100) {
                goto do_hex_escape;
            } else {
                shift = 16;
                hex = u;
                u = 'u';
                goto do_escape;
            }
            break;
          do_hex_escape:
            shift = 8;
            hex = u;
            u = 'x';
          do_escape:
            c = '\\';
            state = ESCAPE_START;
            break;
          case ESCAPE_START:
            JS_ASSERT(' ' <= u && u < 127);
            c = (char)u;
            state = ESCAPE_MORE;
            break;
          case ESCAPE_MORE:
            if (shift == 0) {
                state = CHARS;
                continue;
            }
            shift -= 4;
            u = 0xF & (hex >> shift);
            c = (char)(u + (u < 10 ? '0' : 'A' - 10));
            break;
        }
        if (buffer) {
            if (n == bufferSize)
                break;
            buffer[n] = c;
        } else if (fp) {
            fputc(c, fp);
        }
        n++;
    }
  stop:
    if (buffer)
        buffer[n] = '\0';
    return n;
}

#endif
