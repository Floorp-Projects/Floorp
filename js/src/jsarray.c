/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80:
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
 * JS array class.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

/* 2^32 - 1 as a number and a string */
#define MAXINDEX 4294967295u
#define MAXSTR   "4294967295"

/* A useful value for identifying a hole in an array */
#define JSVAL_HOLE BOOLEAN_TO_JSVAL(2)

/*
 * Determine if the id represents an array index or an XML property index.
 *
 * An id is an array index according to ECMA by (15.4):
 *
 * "Array objects give special treatment to a certain class of property names.
 * A property name P (in the form of a string value) is an array index if and
 * only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal
 * to 2^32-1."
 *
 * In our implementation, it would be sufficient to check for JSVAL_IS_INT(id)
 * except that by using signed 32-bit integers we miss the top half of the
 * valid range. This function checks the string representation itself; note
 * that calling a standard conversion routine might allow strings such as
 * "08" or "4.0" as array indices, which they are not.
 */
JSBool
js_IdIsIndex(jsval id, jsuint *indexp)
{
    JSString *str;
    jschar *cp;

    if (JSVAL_IS_INT(id)) {
        jsint i;
        i = JSVAL_TO_INT(id);
        if (i < 0)
            return JS_FALSE;
        *indexp = (jsuint)i;
        return JS_TRUE;
    }

    /* NB: id should be a string, but jsxml.c may call us with an object id. */
    if (!JSVAL_IS_STRING(id))
        return JS_FALSE;

    str = JSVAL_TO_STRING(id);
    cp = JSSTRING_CHARS(str);
    if (JS7_ISDEC(*cp) && JSSTRING_LENGTH(str) < sizeof(MAXSTR)) {
        jsuint index = JS7_UNDEC(*cp++);
        jsuint oldIndex = 0;
        jsuint c = 0;
        if (index != 0) {
            while (JS7_ISDEC(*cp)) {
                oldIndex = index;
                c = JS7_UNDEC(*cp);
                index = 10*index + c;
                cp++;
            }
        }

        /* Ensure that all characters were consumed and we didn't overflow. */
        if (*cp == 0 &&
             (oldIndex < (MAXINDEX / 10) ||
              (oldIndex == (MAXINDEX / 10) && c < (MAXINDEX % 10))))
        {
            *indexp = index;
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

static JSBool
ValueIsLength(JSContext *cx, jsval v, jsuint *lengthp)
{
    jsint i;
    jsdouble d;

    if (JSVAL_IS_INT(v)) {
        i = JSVAL_TO_INT(v);
        if (i < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_ARRAY_LENGTH);
            return JS_FALSE;
        }
        *lengthp = (jsuint) i;
        return JS_TRUE;
    }

    if (!js_ValueToNumber(cx, v, &d)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_ARRAY_LENGTH);
        return JS_FALSE;
    }
    if (!js_DoubleToECMAUint32(cx, d, (uint32 *)lengthp)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_ARRAY_LENGTH);
        return JS_FALSE;
    }
    if (JSDOUBLE_IS_NaN(d) || d != *lengthp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_ARRAY_LENGTH);
        return JS_FALSE;
    }
    return JS_TRUE;
}

JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    jsid id;
    jsint i;
    jsval v;

    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
        return JS_FALSE;

    /* Short-circuit, because js_ValueToECMAUint32 fails when
     * called during init time.
     */
    if (JSVAL_IS_INT(v)) {
        i = JSVAL_TO_INT(v);
        /* jsuint cast does ToUint32. */
        *lengthp = (jsuint)i;
        return JS_TRUE;
    }
    return js_ValueToECMAUint32(cx, v, (uint32 *)lengthp);
}

static JSBool
IndexToValue(JSContext *cx, jsuint index, jsval *vp)
{
    if (index <= JSVAL_INT_MAX) {
        *vp = INT_TO_JSVAL(index);
        return JS_TRUE;
    }
    return js_NewDoubleValue(cx, (jsdouble)index, vp);
}

static JSBool
IndexToId(JSContext *cx, jsuint index, jsid *idp)
{
    JSString *str;
    JSAtom *atom;

    if (index <= JSVAL_INT_MAX) {
        *idp = INT_TO_JSID(index);
    } else {
        str = js_NumberToString(cx, (jsdouble)index);
        if (!str)
            return JS_FALSE;
        atom = js_AtomizeString(cx, str, 0);
        if (!atom)
            return JS_FALSE;
        *idp = ATOM_TO_JSID(atom);

    }
    return JS_TRUE;
}

static JSBool
PropertyExists(JSContext *cx, JSObject *obj, jsid id, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;

    *foundp = prop != NULL;
    if (*foundp) {
        OBJ_DROP_PROPERTY(cx, obj2, prop);
    }

    return JS_TRUE;
}

#define JSID_HOLE       JSVAL_NULL

static JSBool
IndexToExistingId(JSContext *cx, JSObject *obj, jsuint index, jsid *idp)
{
    JSBool exists;

    if (!IndexToId(cx, index, idp))
        return JS_FALSE;
    if (!PropertyExists(cx, obj, *idp, &exists))
        return JS_FALSE;
    if (!exists)
        *idp = JSID_HOLE;
    return JS_TRUE;
}

JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsuint length)
{
    jsval v;
    jsid id;

    if (!IndexToValue(cx, length, &v))
        return JS_FALSE;
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    return OBJ_SET_PROPERTY(cx, obj, id, &v);
}

JSBool
js_HasLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    JSErrorReporter older;
    jsid id;
    JSBool ok;
    jsval v;

    older = JS_SetErrorReporter(cx, NULL);
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    ok = OBJ_GET_PROPERTY(cx, obj, id, &v);
    JS_SetErrorReporter(cx, older);
    if (!ok)
        return JS_FALSE;
    return ValueIsLength(cx, v, lengthp);
}

/*
 * This get function is specific to Array.prototype.length and other array
 * instance length properties.  It calls back through the class get function
 * in case some magic happens there (see call_getProperty in jsfun.c).
 */
static JSBool
array_length_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return OBJ_GET_CLASS(cx, obj)->getProperty(cx, obj, id, vp);
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint newlen, oldlen, slot;
    jsid id2;
    jsval junk;

    if (!ValueIsLength(cx, *vp, &newlen))
        return JS_FALSE;
    if (!js_GetLengthProperty(cx, obj, &oldlen))
        return JS_FALSE;
    slot = oldlen;
    while (slot > newlen) {
        --slot;
        if (!IndexToId(cx, slot, &id2))
            return JS_FALSE;
        if (!OBJ_DELETE_PROPERTY(cx, obj, id2, &junk))
            return JS_FALSE;
    }
    return IndexToValue(cx, newlen, vp);
}

static JSBool
array_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint index, length;

    if (!js_IdIsIndex(id, &index))
        return JS_TRUE;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (index >= length) {
        length = index + 1;
        return js_SetLengthProperty(cx, obj, length);
    }
    return JS_TRUE;
}

static JSBool
array_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    jsuint length;

    if (JS_VERSION_IS_1_2(cx)) {
        if (!js_GetLengthProperty(cx, obj, &length))
            return JS_FALSE;
        switch (type) {
          case JSTYPE_NUMBER:
            return IndexToValue(cx, length, vp);
          case JSTYPE_BOOLEAN:
            *vp = BOOLEAN_TO_JSVAL(length > 0);
            return JS_TRUE;
          default:
            return JS_TRUE;
        }
    }
    return js_TryValueOf(cx, obj, type, vp);
}

JSClass js_ArrayClass = {
    "Array",
    0,
    array_addProperty, JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    array_convert,     JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
array_join_sub(JSContext *cx, JSObject *obj, JSString *sep, JSBool literalize,
               jsval *rval, JSBool localeString)
{
    JSBool ok;
    jsuint length, index;
    jschar *chars, *ochars;
    size_t nchars, growth, seplen, tmplen;
    const jschar *sepstr;
    JSString *str;
    JSHashEntry *he;
    JSObject *obj2;
    int stackDummy;

    if (!JS_CHECK_STACK_SIZE(cx, stackDummy)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_OVER_RECURSED);
        return JS_FALSE;
    }

    ok = js_GetLengthProperty(cx, obj, &length);
    if (!ok)
        return JS_FALSE;

    he = js_EnterSharpObject(cx, obj, NULL, &chars);
    if (!he)
        return JS_FALSE;
    if (literalize) {
        if (IS_SHARP(he)) {
#if JS_HAS_SHARP_VARS
            nchars = js_strlen(chars);
#else
            chars[0] = '[';
            chars[1] = ']';
            chars[2] = 0;
            nchars = 2;
#endif
            goto make_string;
        }

        /*
         * Allocate 1 + 3 + 1 for "[", the worst-case closing ", ]", and the
         * terminating 0.
         */
        growth = (1 + 3 + 1) * sizeof(jschar);
        if (!chars) {
            nchars = 0;
            chars = (jschar *) malloc(growth);
            if (!chars)
                goto done;
        } else {
            MAKE_SHARP(he);
            nchars = js_strlen(chars);
            chars = (jschar *)
                realloc((ochars = chars), nchars * sizeof(jschar) + growth);
            if (!chars) {
                free(ochars);
                goto done;
            }
        }
        chars[nchars++] = '[';
    } else {
        /*
         * Free any sharp variable definition in chars.  Normally, we would
         * MAKE_SHARP(he) so that only the first sharp variable annotation is
         * a definition, and all the rest are references, but in the current
         * case of (!literalize), we don't need chars at all.
         */
        if (chars)
            JS_free(cx, chars);
        chars = NULL;
        nchars = 0;

        /* Return the empty string on a cycle as well as on empty join. */
        if (IS_BUSY(he) || length == 0) {
            js_LeaveSharpObject(cx, NULL);
            *rval = JS_GetEmptyStringValue(cx);
            return ok;
        }

        /* Flag he as BUSY so we can distinguish a cycle from a join-point. */
        MAKE_BUSY(he);
    }
    sepstr = NULL;
    seplen = JSSTRING_LENGTH(sep);

    /* Use rval to locally root each element value as we loop and convert. */
#define v (*rval)

    v = JSVAL_NULL;
    for (index = 0; index < length; index++) {
        ok = JS_GetElement(cx, obj, index, &v);
        if (!ok)
            goto done;

        if ((!literalize || JS_VERSION_IS_1_2(cx)) &&
            (JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v))) {
            str = cx->runtime->emptyString;
        } else {
            if (localeString) {
                if (!js_ValueToObject(cx, v, &obj2) ||
                    !js_TryMethod(cx, obj2,
                                  cx->runtime->atomState.toLocaleStringAtom,
                                  0, NULL, &v)) {
                    str = NULL;
                } else {
                    str = js_ValueToString(cx, v);
                }
            } else {
                str = (literalize ? js_ValueToSource : js_ValueToString)(cx, v);
            }
            if (!str) {
                ok = JS_FALSE;
                goto done;
            }
        }

        /* Allocate 3 + 1 at end for ", ", closing bracket, and zero. */
        growth = (nchars + (sepstr ? seplen : 0) +
                  JSSTRING_LENGTH(str) +
                  3 + 1) * sizeof(jschar);
        if (!chars) {
            chars = (jschar *) malloc(growth);
            if (!chars)
                goto done;
        } else {
            chars = (jschar *) realloc((ochars = chars), growth);
            if (!chars) {
                free(ochars);
                goto done;
            }
        }

        if (sepstr) {
            js_strncpy(&chars[nchars], sepstr, seplen);
            nchars += seplen;
        }
        sepstr = JSSTRING_CHARS(sep);

        tmplen = JSSTRING_LENGTH(str);
        js_strncpy(&chars[nchars], JSSTRING_CHARS(str), tmplen);
        nchars += tmplen;
    }

  done:
    if (literalize) {
        if (chars) {
            if (JSVAL_IS_VOID(v)) {
                chars[nchars++] = ',';
                chars[nchars++] = ' ';
            }
            chars[nchars++] = ']';
        }
    } else {
        CLEAR_BUSY(he);
    }
    js_LeaveSharpObject(cx, NULL);
    if (!ok) {
        if (chars)
            free(chars);
        return ok;
    }

#undef v

  make_string:
    if (!chars) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    chars[nchars] = 0;
    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
        free(chars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static jschar   comma_space_ucstr[] = {',', ' ', 0};
static jschar   comma_ucstr[]       = {',', 0};
static JSString comma_space         = {2, comma_space_ucstr};
static JSString comma               = {1, comma_ucstr};

#if JS_HAS_TOSOURCE
static JSBool
array_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    return array_join_sub(cx, obj, &comma_space, JS_TRUE, rval, JS_FALSE);
}
#endif

static JSBool
array_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSBool literalize;

    /*
     * JS1.2 arrays convert to array literals, with a comma followed by a space
     * between each element.
     */
    literalize = JS_VERSION_IS_1_2(cx);
    return array_join_sub(cx, obj, literalize ? &comma_space : &comma,
                          literalize, rval, JS_FALSE);
}

static JSBool
array_toLocaleString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    /*
     *  Passing comma here as the separator. Need a way to get a
     *  locale-specific version.
     */
    return array_join_sub(cx, obj, &comma, JS_FALSE, rval, JS_TRUE);
}

static JSBool
InitArrayElements(JSContext *cx, JSObject *obj, jsuint length, jsval *vector)
{
    jsuint index;
    jsid id;

    for (index = 0; index < length; index++) {
        JS_ASSERT(vector[index] != JSVAL_HOLE);

        if (!IndexToId(cx, index, &id))
            return JS_FALSE;
        if (!OBJ_SET_PROPERTY(cx, obj, id, &vector[index]))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, jsuint length, jsval *vector)
{
    jsval v;
    jsid id;

    if (!IndexToValue(cx, length, &v))
        return JS_FALSE;
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    if (!OBJ_DEFINE_PROPERTY(cx, obj, id, v,
                             array_length_getter, array_length_setter,
                             JSPROP_PERMANENT,
                             NULL)) {
          return JS_FALSE;
    }
    if (!vector)
        return JS_TRUE;
    return InitArrayElements(cx, obj, length, vector);
}

#if JS_HAS_SOME_PERL_FUN
/*
 * Perl-inspired join, reverse, and sort.
 */
static JSBool
array_join(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;

    if (JSVAL_IS_VOID(argv[0]))
        return array_join_sub(cx, obj, &comma, JS_FALSE, rval, JS_FALSE);
    str = js_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    argv[0] = STRING_TO_JSVAL(str);
    return array_join_sub(cx, obj, str, JS_FALSE, rval, JS_FALSE);
}

static JSBool
array_reverse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    jsuint len, half, i;
    jsid id, id2;
    jsval *tmproot, *tmproot2;
    JSBool idexists, id2exists;

    if (!js_GetLengthProperty(cx, obj, &len))
        return JS_FALSE;

    /*
     * Use argv[argc] and argv[argc + 1] as local roots to hold temporarily
     * array elements for GC-safe swap.
     */
    tmproot = argv + argc;
    tmproot2 = argv + argc + 1;
    half = len / 2;
    for (i = 0; i < half; i++) {
        if (!IndexToId(cx, i, &id))
            return JS_FALSE;
        if (!IndexToId(cx, len - i - 1, &id2))
            return JS_FALSE;

        /* Check for holes to make sure they don't get filled. */
        if (!PropertyExists(cx, obj, id, &idexists) ||
            !PropertyExists(cx, obj, id2, &id2exists)) {
            return JS_FALSE;
        }

        /*
         * Get both of the values now. Note that we don't use v, or v2 based on
         * idexists and id2exists.
         */
        if (!OBJ_GET_PROPERTY(cx, obj, id, tmproot) ||
            !OBJ_GET_PROPERTY(cx, obj, id2, tmproot2)) {
            return JS_FALSE;
        }

        if (idexists) {
            if (!OBJ_SET_PROPERTY(cx, obj, id2, tmproot))
                return JS_FALSE;
        } else {
            if (!OBJ_DELETE_PROPERTY(cx, obj, id2, tmproot))
                return JS_FALSE;
        }
        if (id2exists) {
            if (!OBJ_SET_PROPERTY(cx, obj, id, tmproot2))
                return JS_FALSE;
        } else {
            if (!OBJ_DELETE_PROPERTY(cx, obj, id, tmproot2))
                return JS_FALSE;
        }
    }

    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

typedef struct HSortArgs {
    void         *vec;
    size_t       elsize;
    void         *pivot;
    JSComparator cmp;
    void         *arg;
    JSBool       fastcopy;
} HSortArgs;

static int
sort_compare(const void *a, const void *b, void *arg);

static int
sort_compare_strings(const void *a, const void *b, void *arg);

static void
HeapSortHelper(JSBool building, HSortArgs *hsa, size_t lo, size_t hi)
{
    void *pivot, *vec, *vec2, *arg, *a, *b;
    size_t elsize;
    JSComparator cmp;
    JSBool fastcopy;
    size_t j, hiDiv2;

    pivot = hsa->pivot;
    vec = hsa->vec;
    elsize = hsa->elsize;
    vec2 =  (char *)vec - 2 * elsize;
    cmp = hsa->cmp;
    arg = hsa->arg;

    fastcopy = hsa->fastcopy;
#define MEMCPY(p,q,n) \
    (fastcopy ? (void)(*(jsval*)(p) = *(jsval*)(q)) : (void)memcpy(p, q, n))

    if (lo == 1) {
        j = 2;
        b = (char *)vec + elsize;
        if (j < hi && cmp(vec, b, arg) < 0)
            j++;
        a = (char *)vec + (hi - 1) * elsize;
        b = (char *)vec2 + j * elsize;

        /*
         * During sorting phase b points to a member of heap that cannot be
         * bigger then biggest of vec[0] and vec[1], and cmp(a, b, arg) <= 0
         * always holds.
         */
        if ((building || hi == 2) && cmp(a, b, arg) >= 0)
            return;

        MEMCPY(pivot, a, elsize);
        MEMCPY(a, b, elsize);
        lo = j;
    } else {
        a = (char *)vec2 + lo * elsize;
        MEMCPY(pivot, a, elsize);
    }

    hiDiv2 = hi/2;
    while (lo <= hiDiv2) {
        j = lo + lo;
        a = (char *)vec2 + j * elsize;
        b = (char *)vec + (j - 1) * elsize;
        if (j < hi && cmp(a, b, arg) < 0)
            j++;
        b = (char *)vec2 + j * elsize;
        if (cmp(pivot, b, arg) >= 0)
            break;

        a = (char *)vec2 + lo * elsize;
        MEMCPY(a, b, elsize);
        lo = j;
    }

    a = (char *)vec2 + lo * elsize;
    MEMCPY(a, pivot, elsize);
#undef MEMCPY
}

void
js_HeapSort(void *vec, size_t nel, void *pivot, size_t elsize,
            JSComparator cmp, void *arg)
{
    HSortArgs hsa;
    size_t i;

    hsa.vec = vec;
    hsa.elsize = elsize;
    hsa.pivot = pivot;
    hsa.cmp = cmp;
    hsa.arg = arg;
    hsa.fastcopy = (cmp == sort_compare || cmp == sort_compare_strings);

    for (i = nel/2; i != 0; i--)
        HeapSortHelper(JS_TRUE, &hsa, i, nel);
    while (nel > 2)
        HeapSortHelper(JS_FALSE, &hsa, 1, --nel);
}

typedef struct CompareArgs {
    JSContext   *context;
    jsval       fval;
    jsval       *localroot;     /* need one local root, for sort_compare */
    JSBool      status;
} CompareArgs;

static int
sort_compare(const void *a, const void *b, void *arg)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;
    CompareArgs *ca = (CompareArgs *) arg;
    JSContext *cx = ca->context;
    jsdouble cmp = -1;
    jsval fval, argv[2], rval, special;
    JSBool ok;

    fval = ca->fval;

    /*
     * By ECMA 262, 15.4.4.11, existence of the property with value undefined
     * takes precedence over an undefined property (which we call a "hole").
     */
    if (av == JSVAL_HOLE || bv == JSVAL_HOLE)
        special = JSVAL_HOLE;
    else if (av == JSVAL_VOID || bv == JSVAL_VOID)
        special = JSVAL_VOID;
    else
        special = JSVAL_NULL;

    if (special != JSVAL_NULL) {
        if (av == bv)
            cmp = 0;
        else if (av != special)
            cmp = -1;
        else
            cmp = 1;
    } else if (fval == JSVAL_NULL) {
        JSString *astr, *bstr;

        if (av == bv) {
            cmp = 0;
        } else {
            /*
             * Set our local root to astr in case the second js_ValueToString
             * displaces the newborn root in cx, and the GC nests under that
             * call.  Don't bother guarding the local root store with an astr
             * non-null test.  If we tag null as a string, the GC will untag,
             * null-test, and avoid dereferencing null.
             */
            astr = js_ValueToString(cx, av);
            *ca->localroot = STRING_TO_JSVAL(astr);
            if (astr && (bstr = js_ValueToString(cx, bv)))
                cmp = js_CompareStrings(astr, bstr);
            else
                ca->status = JS_FALSE;
        }
    } else {
        argv[0] = av;
        argv[1] = bv;
        ok = js_InternalCall(cx,
                             OBJ_GET_PARENT(cx, JSVAL_TO_OBJECT(fval)),
                             fval, 2, argv, &rval);
        if (ok) {
            ok = js_ValueToNumber(cx, rval, &cmp);

            /* Clamp cmp to -1, 0, 1. */
            if (ok) {
                if (JSDOUBLE_IS_NaN(cmp)) {
                    /*
                     * XXX report some kind of error here?  ECMA talks about
                     * 'consistent compare functions' that don't return NaN,
                     * but is silent about what the result should be.  So we
                     * currently ignore it.
                     */
                    cmp = 0;
                } else if (cmp != 0) {
                    cmp = cmp > 0 ? 1 : -1;
                }
            }
        }
        if (!ok)
            ca->status = ok;
    }
    return (int)cmp;
}

static int
sort_compare_strings(const void *a, const void *b, void *arg)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;

    return (int) js_CompareStrings(JSVAL_TO_STRING(av), JSVAL_TO_STRING(bv));
}

static JSBool
array_sort(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval, *vec, *pivotroot;
    CompareArgs ca;
    jsuint len, newlen, i;
    JSStackFrame *fp;
    jsid id;
    size_t nbytes;

    /*
     * Optimize the default compare function case if all of obj's elements
     * have values of type string.
     */
    JSBool all_strings;

    if (argc > 0) {
        if (JSVAL_IS_PRIMITIVE(argv[0])) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_SORT_ARG);
            return JS_FALSE;
        }
        fval = argv[0];
        all_strings = JS_FALSE; /* non-default compare function */
    } else {
        fval = JSVAL_NULL;
        all_strings = JS_TRUE;  /* check for all string values */
    }

    if (!js_GetLengthProperty(cx, obj, &len))
        return JS_FALSE;
    if (len == 0) {
        *rval = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
    }

    /*
     * We need a temporary array of len jsvals to hold elements of the array.
     * Check that its size does not overflow size_t, which would allow for
     * indexing beyond the end of the malloc'd vector.
     */
    if (len > ((size_t) -1) / sizeof(jsval)) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    nbytes = ((size_t) len) * sizeof(jsval);

    vec = (jsval *) JS_malloc(cx, nbytes);
    if (!vec)
        return JS_FALSE;

    /* Root vec, clearing it first in case a GC nests while we're filling it. */
    memset(vec, 0, nbytes);
    fp = cx->fp;
    fp->vars = vec;
    fp->nvars = len;

    newlen = 0;
    for (i = 0; i < len; i++) {
        ca.status = IndexToExistingId(cx, obj, i, &id);
        if (!ca.status)
            goto out;

        if (id == JSID_HOLE) {
            vec[i] = JSVAL_HOLE;
            all_strings = JS_FALSE;
            continue;
        }
        newlen++;

        ca.status = OBJ_GET_PROPERTY(cx, obj, id, &vec[i]);
        if (!ca.status)
            goto out;

        /* We know JSVAL_IS_STRING yields 0 or 1, so avoid a branch via &=. */
        all_strings &= JSVAL_IS_STRING(vec[i]);
    }

    ca.context = cx;
    ca.fval = fval;
    ca.localroot = argv + argc;       /* local GC root for temporary string */
    pivotroot    = argv + argc + 1;   /* local GC root for pivot val */
    ca.status = JS_TRUE;
    js_HeapSort(vec, (size_t) len, pivotroot, sizeof(jsval),
                all_strings ? sort_compare_strings : sort_compare,
                &ca);

    if (ca.status) {
        ca.status = InitArrayElements(cx, obj, newlen, vec);
        if (ca.status)
            *rval = OBJECT_TO_JSVAL(obj);

        /* Re-create any holes that sorted to the end of the array. */
        while (len > newlen) {
            jsval junk;

            if (!IndexToId(cx, --len, &id))
                return JS_FALSE;
            if (!OBJ_DELETE_PROPERTY(cx, obj, id, &junk))
                return JS_FALSE;
        }
    }

out:
    if (vec)
        JS_free(cx, vec);
    return ca.status;
}
#endif /* JS_HAS_SOME_PERL_FUN */

#if JS_HAS_MORE_PERL_FUN
/*
 * Perl-inspired push, pop, shift, unshift, and splice methods.
 */
static JSBool
array_push(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length;
    uintN i;
    jsid id;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    for (i = 0; i < argc; i++) {
        if (!IndexToId(cx, length + i, &id))
            return JS_FALSE;
        if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
            return JS_FALSE;
    }

    /*
     * If JS1.2, follow Perl4 by returning the last thing pushed.  Otherwise,
     * return the new array length.
     */
    length += argc;
    if (JS_VERSION_IS_1_2(cx)) {
        *rval = argc ? argv[argc-1] : JSVAL_VOID;
    } else {
        if (!IndexToValue(cx, length, rval))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_pop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint index;
    jsid id;
    jsval junk;

    if (!js_GetLengthProperty(cx, obj, &index))
        return JS_FALSE;
    if (index > 0) {
        index--;
        if (!IndexToId(cx, index, &id))
            return JS_FALSE;

        /* Get the to-be-deleted property's value into rval. */
        if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
            return JS_FALSE;

        if (!OBJ_DELETE_PROPERTY(cx, obj, id, &junk))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, index);
}

static JSBool
array_shift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length, i;
    jsid id, id2;
    jsval junk;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length > 0) {
        length--;
        id = JSVAL_ZERO;

        /* Get the to-be-deleted property's value into rval ASAP. */
        if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
            return JS_FALSE;

        /*
         * Slide down the array above the first element.
         */
        if (length > 0) {
            for (i = 1; i <= length; i++) {
                if (!IndexToId(cx, i, &id))
                    return JS_FALSE;
                if (!IndexToId(cx, i - 1, &id2))
                    return JS_FALSE;
                if (!OBJ_GET_PROPERTY(cx, obj, id, &argv[0]))
                    return JS_FALSE;
                if (!OBJ_SET_PROPERTY(cx, obj, id2, &argv[0]))
                    return JS_FALSE;
            }
        }

        /* Delete the only or last element. */
        if (!OBJ_DELETE_PROPERTY(cx, obj, id, &junk))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    jsuint length, last;
    uintN i;
    jsid id, id2;
    jsval *vp, junk;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (argc > 0) {
        /* Slide up the array to make room for argc at the bottom. */
        if (length > 0) {
            last = length;
            vp = argv + argc;
            while (last--) {
                if (!IndexToExistingId(cx, obj, last, &id))
                    return JS_FALSE;
                if (id == JSID_HOLE) {
                    OBJ_DELETE_PROPERTY(cx, obj, id2, &junk);
                    continue;
                }
                if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
                    return JS_FALSE;
                if (!IndexToId(cx, last + argc, &id2))
                    return JS_FALSE;
                if (!OBJ_SET_PROPERTY(cx, obj, id2, vp))
                    return JS_FALSE;
            }
        }

        /* Copy from argv to the bottom of the array. */
        for (i = 0; i < argc; i++) {
            if (!IndexToId(cx, i, &id))
                return JS_FALSE;
            if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
                return JS_FALSE;
        }

        /* Follow Perl by returning the new array length. */
        length += argc;
        if (!js_SetLengthProperty(cx, obj, length))
            return JS_FALSE;
    }
    return IndexToValue(cx, length, rval);
}

static JSBool
array_splice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval *vp, junk;
    jsuint length, begin, end, count, delta, last;
    jsdouble d;
    jsid id, id2;
    JSObject *obj2;
    uintN i;

    /*
     * Nothing to do if no args.  Otherwise point vp at our one explicit local
     * root and get length.
     */
    if (argc == 0)
        return JS_TRUE;
    vp = argv + argc;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /* Convert the first argument into a starting index. */
    if (!js_ValueToNumber(cx, *argv, &d))
        return JS_FALSE;
    d = js_DoubleToInteger(d);
    if (d < 0) {
        d += length;
        if (d < 0)
            d = 0;
    } else if (d > length) {
        d = length;
    }
    begin = (jsuint)d; /* d has been clamped to uint32 */
    argc--;
    argv++;

    /* Convert the second argument from a count into a fencepost index. */
    delta = length - begin;
    if (argc == 0) {
        count = delta;
        end = length;
    } else {
        if (!js_ValueToNumber(cx, *argv, &d))
            return JS_FALSE;
        d = js_DoubleToInteger(d);
        if (d < 0)
            d = 0;
        else if (d > delta)
            d = delta;
        count = (jsuint)d;
        end = begin + count;
        argc--;
        argv++;
    }

    if (count == 1 && JS_VERSION_IS_1_2(cx)) {
        /*
         * JS lacks "list context", whereby in Perl one turns the single
         * scalar that's spliced out into an array just by assigning it to
         * @single instead of $single, or by using it as Perl push's first
         * argument, for instance.
         *
         * JS1.2 emulated Perl too closely and returned a non-Array for
         * the single-splice-out case, requiring callers to test and wrap
         * in [] if necessary.  So JS1.3, default, and other versions all
         * return an array of length 1 for uniformity.
         */
        if (!IndexToId(cx, begin, &id))
            return JS_FALSE;
        if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
            return JS_FALSE;
    } else {
        if (!JS_VERSION_IS_1_2(cx) || count > 0) {
            /*
             * Create a new array value to return.  Our ECMA v2 proposal specs
             * that splice always returns an array value, even when given no
             * arguments.  We think this is best because it eliminates the need
             * for callers to do an extra test to handle the empty splice case.
             */
            obj2 = js_NewArrayObject(cx, 0, NULL);
            if (!obj2)
                return JS_FALSE;
            *rval = OBJECT_TO_JSVAL(obj2);

            /* If there are elements to remove, put them into the return value. */
            if (count > 0) {
                for (last = begin; last < end; last++) {
                    if (!IndexToExistingId(cx, obj, last, &id))
                        return JS_FALSE;
                    if (id == JSID_HOLE)
                        continue;       /* don't fill holes in the new array */
                    if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
                        return JS_FALSE;
                    if (!IndexToId(cx, last - begin, &id2))
                        return JS_FALSE;
                    if (!OBJ_SET_PROPERTY(cx, obj2, id2, vp))
                        return JS_FALSE;
                }

                if (!js_SetLengthProperty(cx, obj2, end - begin))
                    return JS_FALSE;
            }
        }
    }

    /* Find the direction (up or down) to copy and make way for argv. */
    if (argc > count) {
        delta = (jsuint)argc - count;
        last = length;
        /* (uint) end could be 0, so can't use vanilla >= test */
        while (last-- > end) {
            if (!IndexToExistingId(cx, obj, last, &id))
                return JS_FALSE;
            if (!IndexToId(cx, last + delta, &id2))
                return JS_FALSE;
            if (id != JSID_HOLE) {
                if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
                    return JS_FALSE;
                if (!OBJ_SET_PROPERTY(cx, obj, id2, vp))
                    return JS_FALSE;
            } else {
                if (!OBJ_DELETE_PROPERTY(cx, obj, id2, &junk))
                    return JS_FALSE;
            }
        }
        length += delta;
    } else if (argc < count) {
        delta = count - (jsuint)argc;
        for (last = end; last < length; last++) {
            if (!IndexToExistingId(cx, obj, last, &id))
                return JS_FALSE;
            if (!IndexToId(cx, last - delta, &id2))
                return JS_FALSE;
            if (id != JSID_HOLE) {
                if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
                    return JS_FALSE;
                if (!OBJ_SET_PROPERTY(cx, obj, id2, vp))
                    return JS_FALSE;
            } else {
                if (!OBJ_DELETE_PROPERTY(cx, obj, id2, &junk))
                    return JS_FALSE;
            }
        }
        length -= delta;
    }

    /* Copy from argv into the hole to complete the splice. */
    for (i = 0; i < argc; i++) {
        if (!IndexToId(cx, begin + i, &id))
            return JS_FALSE;
        if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
            return JS_FALSE;
    }

    /* Update length in case we deleted elements from the end. */
    return js_SetLengthProperty(cx, obj, length);
}
#endif /* JS_HAS_MORE_PERL_FUN */

#if JS_HAS_SEQUENCE_OPS
/*
 * Python-esque sequence operations.
 */
static JSBool
array_concat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval *vp, v;
    JSObject *nobj, *aobj;
    jsuint length, alength, slot;
    uintN i;
    jsid id, id2;

    /* Hoist the explicit local root address computation. */
    vp = argv + argc;

    /* Treat obj as the first argument; see ECMA 15.4.4.4. */
    --argv;
    JS_ASSERT(obj == JSVAL_TO_OBJECT(argv[0]));

    /* Create a new Array object and store it in the rval local root. */
    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(nobj);

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    length = 0;
    for (i = 0; i <= argc; i++) {
        v = argv[i];
        if (JSVAL_IS_OBJECT(v)) {
            aobj = JSVAL_TO_OBJECT(v);
            if (aobj && OBJ_GET_CLASS(cx, aobj) == &js_ArrayClass) {
                if (!OBJ_GET_PROPERTY(cx, aobj,
                                      ATOM_TO_JSID(cx->runtime->atomState
                                                   .lengthAtom),
                                      vp)) {
                    return JS_FALSE;
                }
                if (!ValueIsLength(cx, *vp, &alength))
                    return JS_FALSE;
                for (slot = 0; slot < alength; slot++) {
                    if (!IndexToExistingId(cx, aobj, slot, &id))
                        return JS_FALSE;
                    if (id == JSID_HOLE) {
                        /*
                         * Per ECMA 262, 15.4.4.4, step 9, ignore non-existent
                         * properties.
                         */
                        continue;
                    }
                    if (!OBJ_GET_PROPERTY(cx, aobj, id, vp))
                        return JS_FALSE;
                    if (!IndexToId(cx, length + slot, &id2))
                        return JS_FALSE;
                    if (!OBJ_SET_PROPERTY(cx, nobj, id2, vp))
                        return JS_FALSE;
                }
                length += alength;
                continue;
            }
        }

        *vp = v;
        if (!IndexToId(cx, length, &id))
            return JS_FALSE;
        if (!OBJ_SET_PROPERTY(cx, nobj, id, vp))
            return JS_FALSE;
        length++;
    }

    return js_SetLengthProperty(cx, nobj, length);
}

static JSBool
array_slice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval *vp;
    JSObject *nobj;
    jsuint length, begin, end, slot;
    jsdouble d;
    jsid id, id2;

    /* Hoist the explicit local root address computation. */
    vp = argv + argc;

    /* Create a new Array object and store it in the rval local root. */
    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(nobj);

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    begin = 0;
    end = length;

    if (argc > 0) {
        if (!js_ValueToNumber(cx, argv[0], &d))
            return JS_FALSE;
        d = js_DoubleToInteger(d);
        if (d < 0) {
            d += length;
            if (d < 0)
                d = 0;
        } else if (d > length) {
            d = length;
        }
        begin = (jsuint)d;

        if (argc > 1) {
            if (!js_ValueToNumber(cx, argv[1], &d))
                return JS_FALSE;
            d = js_DoubleToInteger(d);
            if (d < 0) {
                d += length;
                if (d < 0)
                    d = 0;
            } else if (d > length) {
                d = length;
            }
            end = (jsuint)d;
        }
    }

    if (begin > end)
        begin = end;

    for (slot = begin; slot < end; slot++) {
        if (!IndexToExistingId(cx, obj, slot, &id))
            return JS_FALSE;
        if (id == JSID_HOLE)
            continue;
        if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
            return JS_FALSE;
        if (!IndexToId(cx, slot - begin, &id2))
            return JS_FALSE;
        if (!OBJ_SET_PROPERTY(cx, nobj, id2, vp))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, nobj, end - begin);
}
#endif /* JS_HAS_SEQUENCE_OPS */

#if JS_HAS_ARRAY_EXTRAS

static JSBool
array_indexOfHelper(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                    jsval *rval, JSBool isLast)
{
    jsuint length, i, stop;
    jsint direction;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0)
        goto not_found;

    if (argc <= 1) {
        i = isLast ? length - 1 : 0;
    } else {
        jsdouble start;

        if (!js_ValueToNumber(cx, argv[1], &start))
            return JS_FALSE;
        start = js_DoubleToInteger(start);
        if (start < 0) {
            start += length;
            i = (start < 0) ? 0 : (jsuint)start;
        } else if (start >= length) {
            i = length - 1;
        } else {
            i = (jsuint)start;
        }
    }

    if (isLast) {
        stop = 0;
        direction = -1;
    } else {
        stop = length - 1;
        direction = 1;
    }

    for (;;) {
        jsid id;
        jsval v;

        if (!IndexToExistingId(cx, obj, (jsuint)i, &id))
            return JS_FALSE;
        if (id != JSID_HOLE) {
            if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
                return JS_FALSE;
            if (js_StrictlyEqual(v, argv[0]))
                return js_NewNumberValue(cx, i, rval);
        }

        if (i == stop)
            goto not_found;
        i += direction;
    }

  not_found:
    *rval = INT_TO_JSVAL(-1);
    return JS_TRUE;
}

static JSBool
array_indexOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    return array_indexOfHelper(cx, obj, argc, argv, rval, JS_FALSE);
}

static JSBool
array_lastIndexOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    return array_indexOfHelper(cx, obj, argc, argv, rval, JS_TRUE);
}

/* Order is important; extras that use a caller's predicate must follow MAP. */
typedef enum ArrayExtraMode {
    FOREACH,
    MAP,
    FILTER,
    SOME,
    EVERY
} ArrayExtraMode;

static JSBool
array_extra(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval,
            ArrayExtraMode mode)
{
    jsval *vp, *sp, *origsp, *oldsp;
    jsuint length, newlen, i;
    JSObject *callable, *thisp, *newarr;
    void *mark;
    JSStackFrame *fp;
    JSBool ok, b;

    /* Hoist the explicit local root address computation. */
    vp = argv + argc;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /*
     * First, get or compute our callee, so that we error out consistently
     * when passed a non-callable object.
     */
    callable = js_ValueToCallableObject(cx, &argv[0], 0);
    if (!callable)
        return JS_FALSE;

    /*
     * Set our initial return condition, used for zero-length array cases
     * (and pre-size our map return to match our known length, for all cases).
     */
#ifdef __GNUC__ /* quell GCC overwarning */
    newlen = 0;
    newarr = NULL;
    ok = JS_TRUE;
#endif
    switch (mode) {
      case MAP:
      case FILTER:
        newlen = (mode == MAP) ? length : 0;
        newarr = js_NewArrayObject(cx, newlen, NULL);
        if (!newarr)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(newarr);
        break;
      case SOME:
        *rval = JSVAL_FALSE;
        break;
      case EVERY:
        *rval = JSVAL_TRUE;
        break;
      case FOREACH:
        break;
    }

    if (length == 0)
        return JS_TRUE;

    if (argc > 1) {
        if (!js_ValueToObject(cx, argv[1], &thisp))
            return JS_FALSE;
        argv[1] = OBJECT_TO_JSVAL(thisp);
    } else {
        JSObject *tmp;
        thisp = callable;
        while ((tmp = OBJ_GET_PARENT(cx, thisp)) != NULL)
            thisp = tmp;
    }

    /* We call with 3 args (value, index, array), plus room for rval. */
    origsp = js_AllocStack(cx, 2 + 3 + 1, &mark);
    if (!origsp)
        return JS_FALSE;

    /* Lift current frame to include our args. */
    fp = cx->fp;
    oldsp = fp->sp;

    for (i = 0; i < length; i++) {
        jsid id;
        jsval rval2;

        ok = IndexToExistingId(cx, obj, i, &id);
        if (!ok)
            break;
        if (id == JSID_HOLE)
            continue;
        ok = OBJ_GET_PROPERTY(cx, obj, id, vp);
        if (!ok)
            break;

        /* Push callable and 'this', then args. */
        sp = origsp;
        *sp++ = OBJECT_TO_JSVAL(callable);
        *sp++ = OBJECT_TO_JSVAL(thisp);
        *sp++ = *vp;
        *sp++ = INT_TO_JSVAL(i);
        *sp++ = OBJECT_TO_JSVAL(obj);

        /* Do the call. */
        fp->sp = sp;
        ok = js_Invoke(cx, 3, JSINVOKE_INTERNAL);
        rval2 = fp->sp[-1];
        fp->sp = oldsp;
        if (!ok)
            break;

        if (mode > MAP) {
            if (rval2 == JSVAL_NULL) {
                b = JS_FALSE;
            } else if (JSVAL_IS_BOOLEAN(rval2)) {
                b = JSVAL_TO_BOOLEAN(rval2);
            } else {
                ok = js_ValueToBoolean(cx, rval2, &b);
                if (!ok)
                    goto out;
            }
        }

        switch (mode) {
          case FOREACH:
            break;
          case MAP:
            ok = OBJ_SET_PROPERTY(cx, newarr, id, &rval2);
            if (!ok)
                goto out;
            break;
          case FILTER:
            if (!b)
                break;
            /* Filter passed *vp, push as result. */
            ok = IndexToId(cx, newlen++, &id);
            if (!ok)
                goto out;
            ok = OBJ_SET_PROPERTY(cx, newarr, id, vp);
            if (!ok)
                goto out;
            break;
          case SOME:
            if (b) {
                *rval = JSVAL_TRUE;
                goto out;
            }
            break;
          case EVERY:
            if (!b) {
                *rval = JSVAL_FALSE;
                goto out;
            }
            break;
        }
    }

 out:
    js_FreeStack(cx, mark);
    if (ok && mode == FILTER)
        ok = js_SetLengthProperty(cx, newarr, newlen);
    return ok;
}

static JSBool
array_forEach(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    return array_extra(cx, obj, argc, argv, rval, FOREACH);
}

static JSBool
array_map(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
          jsval *rval)
{
    return array_extra(cx, obj, argc, argv, rval, MAP);
}

static JSBool
array_filter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    return array_extra(cx, obj, argc, argv, rval, FILTER);
}

static JSBool
array_some(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
           jsval *rval)
{
    return array_extra(cx, obj, argc, argv, rval, SOME);
}

static JSBool
array_every(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
           jsval *rval)
{
    return array_extra(cx, obj, argc, argv, rval, EVERY);
}
#endif

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,       array_toSource,         0,0,0},
#endif
    {js_toString_str,       array_toString,         0,0,0},
    {js_toLocaleString_str, array_toLocaleString,   0,0,0},

    /* Perl-ish methods. */
#if JS_HAS_SOME_PERL_FUN
    {"join",                array_join,             1,JSFUN_GENERIC_NATIVE,0},
    {"reverse",             array_reverse,          0,JSFUN_GENERIC_NATIVE,2},
    {"sort",                array_sort,             1,JSFUN_GENERIC_NATIVE,2},
#endif
#if JS_HAS_MORE_PERL_FUN
    {"push",                array_push,             1,JSFUN_GENERIC_NATIVE,0},
    {"pop",                 array_pop,              0,JSFUN_GENERIC_NATIVE,0},
    {"shift",               array_shift,            0,JSFUN_GENERIC_NATIVE,1},
    {"unshift",             array_unshift,          1,JSFUN_GENERIC_NATIVE,1},
    {"splice",              array_splice,           2,JSFUN_GENERIC_NATIVE,1},
#endif

    /* Python-esque sequence methods. */
#if JS_HAS_SEQUENCE_OPS
    {"concat",              array_concat,           1,JSFUN_GENERIC_NATIVE,1},
    {"slice",               array_slice,            2,JSFUN_GENERIC_NATIVE,1},
#endif

#if JS_HAS_ARRAY_EXTRAS
    {"indexOf",             array_indexOf,          1,JSFUN_GENERIC_NATIVE,0},
    {"lastIndexOf",         array_lastIndexOf,      1,JSFUN_GENERIC_NATIVE,0},
    {"forEach",             array_forEach,          1,JSFUN_GENERIC_NATIVE,1},
    {"map",                 array_map,              1,JSFUN_GENERIC_NATIVE,1},
    {"filter",              array_filter,           1,JSFUN_GENERIC_NATIVE,1},
    {"some",                array_some,             1,JSFUN_GENERIC_NATIVE,1},
    {"every",               array_every,            1,JSFUN_GENERIC_NATIVE,1},
#endif

    {0,0,0,0,0}
};

static JSBool
Array(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length;
    jsval *vector;

    /* If called without new, replace obj with a new Array object. */
    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    if (argc == 0) {
        length = 0;
        vector = NULL;
    } else if (JS_VERSION_IS_1_2(cx)) {
        length = (jsuint) argc;
        vector = argv;
    } else if (argc > 1) {
        length = (jsuint) argc;
        vector = argv;
    } else if (!JSVAL_IS_NUMBER(argv[0])) {
        length = 1;
        vector = argv;
    } else {
        if (!ValueIsLength(cx, argv[0], &length))
            return JS_FALSE;
        vector = NULL;
    }
    return InitArrayObject(cx, obj, length, vector);
}

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &js_ArrayClass, Array, 1,
                         NULL, array_methods, NULL, NULL);

    /* Initialize the Array prototype object so it gets a length property. */
    if (!proto || !InitArrayObject(cx, proto, 0, NULL))
        return NULL;
    return proto;
}

JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, jsval *vector)
{
    JSObject *obj;

    obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
    if (!obj)
        return NULL;
    if (!InitArrayObject(cx, obj, length, vector)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    return obj;
}
