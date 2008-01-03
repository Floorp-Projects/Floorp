/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=78:
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
#include "jsdtoa.h"
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

    *lengthp = js_DoubleToECMAUint32(d);

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
    JSTempValueRooter tvr;
    jsid id;
    JSBool ok;
    jsint i;

    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    ok = OBJ_GET_PROPERTY(cx, obj, id, &tvr.u.value);
    if (ok) {
        /*
         * Short-circuit, because js_ValueToECMAUint32 fails when called
         * during init time.
         */
        if (JSVAL_IS_INT(tvr.u.value)) {
            i = JSVAL_TO_INT(tvr.u.value);
            *lengthp = (jsuint)i;       /* jsuint cast does ToUint32 */
        } else {
            ok = js_ValueToECMAUint32(cx, tvr.u.value, (uint32 *)lengthp);
        }
    }
    JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
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
BigIndexToId(JSContext *cx, JSObject *obj, jsuint index, JSBool createAtom,
             jsid *idp)
{
    jschar buf[10], *start;
    JSClass *clasp;
    JSAtom *atom;
    JS_STATIC_ASSERT((jsuint)-1 == 4294967295U);

    JS_ASSERT(index > JSVAL_INT_MAX);

    start = JS_ARRAY_END(buf);
    do {
        --start;
        *start = (jschar)('0' + index % 10);
        index /= 10;
    } while (index != 0);

    /*
     * Skip the atomization if the class is known to store atoms corresponding
     * to big indexes together with elements. In such case we know that the
     * array does not have an element at the given index if its atom does not
     * exist.
     */
    if (!createAtom &&
        ((clasp = OBJ_GET_CLASS(cx, obj)) == &js_ArrayClass ||
         clasp == &js_ArgumentsClass ||
         clasp == &js_ObjectClass)) {
        atom = js_GetExistingStringAtom(cx, start, JS_ARRAY_END(buf) - start);
        if (!atom) {
            *idp = JSVAL_VOID;
            return JS_TRUE;
        }
    } else {
        atom = js_AtomizeChars(cx, start, JS_ARRAY_END(buf) - start, 0);
        if (!atom)
            return JS_FALSE;
    }

    *idp = ATOM_TO_JSID(atom);
    return JS_TRUE;
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
static JSBool
GetArrayElement(JSContext *cx, JSObject *obj, jsuint index, JSBool *hole,
                jsval *vp)
{
    jsid id;
    JSObject *obj2;
    JSProperty *prop;

    if (index <= JSVAL_INT_MAX) {
        id = INT_TO_JSID(index);
    } else {
        if (!BigIndexToId(cx, obj, index, JS_FALSE, &id))
            return JS_FALSE;
        if (id == JSVAL_VOID) {
            *hole = JS_TRUE;
            *vp = JSVAL_VOID;
            return JS_TRUE;
        }
    }

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        *hole = JS_TRUE;
        *vp = JSVAL_VOID;
    } else {
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        if (!OBJ_GET_PROPERTY(cx, obj, id, vp))
            return JS_FALSE;
        *hole = JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static JSBool
SetArrayElement(JSContext *cx, JSObject *obj, jsuint index, jsval v)
{
    jsid id;

    if (index <= JSVAL_INT_MAX) {
        id = INT_TO_JSID(index);
    } else {
        if (!BigIndexToId(cx, obj, index, JS_TRUE, &id))
            return JS_FALSE;
        JS_ASSERT(id != JSVAL_VOID);
    }
    return OBJ_SET_PROPERTY(cx, obj, id, &v);
}

static JSBool
DeleteArrayElement(JSContext *cx, JSObject *obj, jsuint index)
{
    jsid id;
    jsval junk;

    if (index <= JSVAL_INT_MAX) {
        id = INT_TO_JSID(index);
    } else {
        if (!BigIndexToId(cx, obj, index, JS_FALSE, &id))
            return JS_FALSE;
        if (id == JSVAL_VOID)
            return JS_TRUE;
    }
    return OBJ_DELETE_PROPERTY(cx, obj, id, &junk);
}

/*
 * When hole is true, delete the property at the given index. Otherwise set
 * its value to v assuming v is rooted.
 */
static JSBool
SetOrDeleteArrayElement(JSContext *cx, JSObject *obj, jsuint index,
                        JSBool hole, jsval v)
{
    if (hole) {
        JS_ASSERT(v == JSVAL_VOID);
        return DeleteArrayElement(cx, obj, index);
    } else {
        return SetArrayElement(cx, obj, index, v);
    }
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
    JSTempValueRooter tvr;
    jsid id;
    JSBool ok;

    older = JS_SetErrorReporter(cx, NULL);
    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    ok = OBJ_GET_PROPERTY(cx, obj, id, &tvr.u.value);
    JS_SetErrorReporter(cx, older);
    if (ok)
        ok = ValueIsLength(cx, tvr.u.value, lengthp);
    JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
}

JSBool
js_IsArrayLike(JSContext *cx, JSObject *obj, JSBool *answerp, jsuint *lengthp)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, obj);
    *answerp = (clasp == &js_ArgumentsClass || clasp == &js_ArrayClass);
    if (!*answerp) {
        *lengthp = 0;
        return JS_TRUE;
    }
    return js_GetLengthProperty(cx, obj, lengthp);
}

/*
 * The 'length' property of all native Array instances is a shared permanent
 * property of Array.prototype, so it appears to be a direct property of each
 * array instance delegating to that Array.prototype. It accesses the private
 * slot reserved by js_ArrayClass.
 *
 * Since SpiderMonkey supports cross-class prototype-based delegation, we have
 * to be careful about the length getter and setter being called on an object
 * not of Array class. For the getter, we search obj's prototype chain for the
 * array that caused this getter to be invoked. In the setter case to overcome
 * the JSPROP_SHARED attribute, we must define a shadowing length property.
 */
static JSBool
array_length_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    do {
        if (OBJ_GET_CLASS(cx, obj) == &js_ArrayClass) {
            *vp = STOBJ_GET_SLOT(obj, JSSLOT_ARRAY_LENGTH);
            break;
        }
    } while ((obj = OBJ_GET_PROTO(cx, obj)) != NULL);
    return JS_TRUE;
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint newlen, oldlen, gap, index;
    jsval junk;
    JSObject *iter;
    JSTempValueRooter tvr;
    JSBool ok;

    if (OBJ_GET_CLASS(cx, obj) != &js_ArrayClass) {
        jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);

        return OBJ_DEFINE_PROPERTY(cx, obj, lengthId, *vp, NULL, NULL,
                                   JSPROP_ENUMERATE, NULL);
    }

    if (!ValueIsLength(cx, *vp, &newlen))
        return JS_FALSE;
    if (!js_GetLengthProperty(cx, obj, &oldlen))
        return JS_FALSE;
    if (oldlen > newlen) {
        if (oldlen - newlen < (1 << 24)) {
            do {
                --oldlen;
                if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) ||
                    !DeleteArrayElement(cx, obj, oldlen)) {
                    return JS_FALSE;
                }
            } while (oldlen != newlen);
        } else {
            /*
             * We are going to remove a lot of indexes in a presumably sparse
             * array. So instead of looping through indexes between newlen and
             * oldlen, we iterate through all properties and remove those that
             * correspond to indexes in the half-open range [newlen, oldlen).
             * See bug 322135.
             */
            iter = JS_NewPropertyIterator(cx, obj);
            if (!iter)
                return JS_FALSE;

            /* Protect iter against GC in OBJ_DELETE_PROPERTY. */
            JS_PUSH_TEMP_ROOT_OBJECT(cx, iter, &tvr);
            gap = oldlen - newlen;
            for (;;) {
                ok = (JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                      JS_NextProperty(cx, iter, &id));
                if (!ok)
                    break;
                if (id == JSVAL_VOID)
                    break;
                if (js_IdIsIndex(id, &index) && index - newlen < gap) {
                    ok = OBJ_DELETE_PROPERTY(cx, obj, id, &junk);
                    if (!ok)
                        break;
                }
            }
            JS_POP_TEMP_ROOT(cx, &tvr);
            if (!ok)
                return JS_FALSE;
        }
    }
    if (!IndexToValue(cx, newlen, vp))
        return JS_FALSE;
    STOBJ_SET_SLOT(obj, JSSLOT_ARRAY_LENGTH, *vp);
    return JS_TRUE;
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
    return js_TryValueOf(cx, obj, type, vp);
}

JSClass js_ArrayClass = {
    "Array",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
    array_addProperty, JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    array_convert,     JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

enum ArrayToStringOp {
    TO_STRING,
    TO_LOCALE_STRING,
    TO_SOURCE
};

/*
 * When op is TO_STRING or TO_LOCALE_STRING sep indicates a separator to use
 * or "," when sep is NULL.
 * When op is TO_SOURCE sep must be NULL.
 */
static JSBool
array_join_sub(JSContext *cx, JSObject *obj, enum ArrayToStringOp op,
               JSString *sep, jsval *rval)
{
    JSBool ok, hole;
    jsuint length, index;
    jschar *chars, *ochars;
    size_t nchars, growth, seplen, tmplen, extratail;
    const jschar *sepstr;
    JSString *str;
    JSHashEntry *he;
    JSAtom *atom;
    int stackDummy;

    if (!JS_CHECK_STACK_SIZE(cx, stackDummy)) {
        js_ReportOverRecursed(cx);
        return JS_FALSE;
    }

    ok = js_GetLengthProperty(cx, obj, &length);
    if (!ok)
        return JS_FALSE;

    he = js_EnterSharpObject(cx, obj, NULL, &chars);
    if (!he)
        return JS_FALSE;
#ifdef DEBUG
    growth = (size_t) -1;
#endif

    if (op == TO_SOURCE) {
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
         * Always allocate 2 extra chars for closing ']' and terminating 0
         * and then preallocate 1 + extratail to include starting '['.
         */
        extratail = 2;
        growth = (1 + extratail) * sizeof(jschar);
        if (!chars) {
            nchars = 0;
            chars = (jschar *) malloc(growth);
            if (!chars)
                goto done;
        } else {
            MAKE_SHARP(he);
            nchars = js_strlen(chars);
            growth += nchars * sizeof(jschar);
            chars = (jschar *)realloc((ochars = chars), growth);
            if (!chars) {
                free(ochars);
                goto done;
            }
        }
        chars[nchars++] = '[';
        JS_ASSERT(sep == NULL);
        sepstr = NULL;  /* indicates to use ", " as separator */
        seplen = 2;
    } else {
        /*
         * Free any sharp variable definition in chars.  Normally, we would
         * MAKE_SHARP(he) so that only the first sharp variable annotation is
         * a definition, and all the rest are references, but in the current
         * case of (op != TO_SOURCE), we don't need chars at all.
         */
        if (chars)
            JS_free(cx, chars);
        chars = NULL;
        nchars = 0;
        extratail = 1;  /* allocate extra char for terminating 0 */

        /* Return the empty string on a cycle as well as on empty join. */
        if (IS_BUSY(he) || length == 0) {
            js_LeaveSharpObject(cx, NULL);
            *rval = JS_GetEmptyStringValue(cx);
            return ok;
        }

        /* Flag he as BUSY so we can distinguish a cycle from a join-point. */
        MAKE_BUSY(he);

        if (sep) {
            JSSTRING_CHARS_AND_LENGTH(sep, sepstr, seplen);
        } else {
            sepstr = NULL;      /* indicates to use "," as separator */
            seplen = 1;
        }
    }

    /* Use rval to locally root each element value as we loop and convert. */
    for (index = 0; index < length; index++) {
        ok = (JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
              GetArrayElement(cx, obj, index, &hole, rval));
        if (!ok)
            goto done;
        if (hole ||
            (op != TO_SOURCE &&
             (JSVAL_IS_VOID(*rval) || JSVAL_IS_NULL(*rval)))) {
            str = cx->runtime->emptyString;
        } else {
            if (op == TO_LOCALE_STRING) {
                JSObject *robj;

                atom = cx->runtime->atomState.toLocaleStringAtom;
                ok = js_ValueToObject(cx, *rval, &robj);
                if (ok) {
                    /* Re-use *rval to protect robj temporarily. */
                    *rval = OBJECT_TO_JSVAL(robj);
                    ok = js_TryMethod(cx, robj, atom, 0, NULL, rval);
                }
                if (!ok)
                    goto done;
                str = js_ValueToString(cx, *rval);
            } else if (op == TO_STRING) {
                str = js_ValueToString(cx, *rval);
            } else {
                JS_ASSERT(op == TO_SOURCE);
                str = js_ValueToSource(cx, *rval);
            }
            if (!str) {
                ok = JS_FALSE;
                goto done;
            }
        }

        /*
         * Do not append separator after the last element unless it is a hole
         * and we are in toSource. In that case we append single ",".
         */
        if (index + 1 == length)
            seplen = (hole && op == TO_SOURCE) ? 1 : 0;

        /* Allocate 1 at end for closing bracket and zero. */
        tmplen = JSSTRING_LENGTH(str);
        growth = nchars + tmplen + seplen + extratail;
        if (nchars > growth || tmplen > growth ||
            growth > (size_t)-1 / sizeof(jschar)) {
            if (chars) {
                free(chars);
                chars = NULL;
            }
            goto done;
        }
        growth *= sizeof(jschar);
        JS_COUNT_OPERATION(cx, JSOW_ALLOCATION);
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

        js_strncpy(&chars[nchars], JSSTRING_CHARS(str), tmplen);
        nchars += tmplen;

        if (seplen) {
            if (sepstr) {
                js_strncpy(&chars[nchars], sepstr, seplen);
            } else {
                JS_ASSERT(seplen == 1 || seplen == 2);
                chars[nchars] = ',';
                if (seplen == 2)
                    chars[nchars + 1] = ' ';
            }
            nchars += seplen;
        }
    }

  done:
    if (op == TO_SOURCE) {
        if (chars)
            chars[nchars++] = ']';
    } else {
        CLEAR_BUSY(he);
    }
    js_LeaveSharpObject(cx, NULL);
    if (!ok) {
        if (chars)
            free(chars);
        return ok;
    }

  make_string:
    if (!chars) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    chars[nchars] = 0;
    JS_ASSERT(growth == (size_t)-1 || (nchars + 1) * sizeof(jschar) == growth);
    str = js_NewString(cx, chars, nchars);
    if (!str) {
        free(chars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
static JSBool
array_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JSVAL_TO_OBJECT(vp[1]);
    if (!JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))
        return JS_FALSE;
    return array_join_sub(cx, obj, TO_SOURCE, NULL, vp);
}
#endif

static JSBool
array_toString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JSVAL_TO_OBJECT(vp[1]);
    if (!JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))
        return JS_FALSE;
    return array_join_sub(cx, obj, TO_STRING, NULL, vp);
}

static JSBool
array_toLocaleString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JSVAL_TO_OBJECT(vp[1]);
    if (!JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))
        return JS_FALSE;

    /*
     *  Passing comma here as the separator. Need a way to get a
     *  locale-specific version.
     */
    return array_join_sub(cx, obj, TO_LOCALE_STRING, NULL, vp);
}

static JSBool
InitArrayElements(JSContext *cx, JSObject *obj, jsuint start, jsuint end,
                  jsval *vector)
{
    while (start != end) {
        if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) ||
            !SetArrayElement(cx, obj, start++, *vector++)) {
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, jsuint length, jsval *vector)
{
    jsval v;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_ArrayClass);
    if (!IndexToValue(cx, length, &v))
        return JS_FALSE;
    STOBJ_SET_SLOT(obj, JSSLOT_ARRAY_LENGTH, v);
    return !vector || InitArrayElements(cx, obj, 0, length, vector);
}

/*
 * Perl-inspired join, reverse, and sort.
 */
static JSBool
array_join(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;

    if (JSVAL_IS_VOID(vp[2])) {
        str = NULL;
    } else {
        str = js_ValueToString(cx, vp[2]);
        if (!str)
            return JS_FALSE;
        vp[2] = STRING_TO_JSVAL(str);
    }
    return array_join_sub(cx, JS_THIS_OBJECT(cx, vp), TO_STRING, str, vp);
}

static JSBool
array_reverse(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    JSTempValueRooter tvr;
    jsuint len, half, i;
    JSBool ok, hole, hole2;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!js_GetLengthProperty(cx, obj, &len))
        return JS_FALSE;

    ok = JS_TRUE;
    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);
    half = len / 2;
    for (i = 0; i < half; i++) {
        ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
             GetArrayElement(cx, obj, i, &hole, &tvr.u.value) &&
             GetArrayElement(cx, obj, len - i - 1, &hole2, vp) &&
             SetOrDeleteArrayElement(cx, obj, len - i - 1, hole, tvr.u.value) &&
             SetOrDeleteArrayElement(cx, obj, i, hole2, *vp);
        if (!ok)
            break;
    }
    JS_POP_TEMP_ROOT(cx, &tvr);

    *vp = OBJECT_TO_JSVAL(obj);
    return ok;
}

typedef struct MSortArgs {
    size_t       elsize;
    JSComparator cmp;
    void         *arg;
    JSBool       fastcopy;
} MSortArgs;

/* Helper function for js_MergeSort. */
static JSBool
MergeArrays(MSortArgs *msa, void *src, void *dest, size_t run1, size_t run2)
{
    void *arg, *a, *b, *c;
    size_t elsize, runtotal;
    int cmp_result;
    JSComparator cmp;
    JSBool fastcopy;

    runtotal = run1 + run2;

    elsize = msa->elsize;
    cmp = msa->cmp;
    arg = msa->arg;
    fastcopy = msa->fastcopy;

#define CALL_CMP(a, b) \
    if (!cmp(arg, (a), (b), &cmp_result)) return JS_FALSE;

    /* Copy runs already in sorted order. */
    b = (char *)src + run1 * elsize;
    a = (char *)b - elsize;
    CALL_CMP(a, b);
    if (cmp_result <= 0) {
        memcpy(dest, src, runtotal * elsize);
        return JS_TRUE;
    }

#define COPY_ONE(p,q,n) \
    (fastcopy ? (void)(*(jsval*)(p) = *(jsval*)(q)) : (void)memcpy(p, q, n))

    a = src;
    c = dest;
    for (; runtotal != 0; runtotal--) {
        JSBool from_a = run2 == 0;
        if (!from_a && run1 != 0) {
            CALL_CMP(a,b);
            from_a = cmp_result <= 0;
        }

        if (from_a) {
            COPY_ONE(c, a, elsize);
            run1--;
            a = (char *)a + elsize;
        } else {
            COPY_ONE(c, b, elsize);
            run2--;
            b = (char *)b + elsize;
        }
        c = (char *)c + elsize;
    }
#undef COPY_ONE
#undef CALL_CMP

    return JS_TRUE;
}

/*
 * This sort is stable, i.e. sequence of equal elements is preserved.
 * See also bug #224128.
 */
JSBool
js_MergeSort(void *src, size_t nel, size_t elsize,
             JSComparator cmp, void *arg, void *tmp)
{
    void *swap, *vec1, *vec2;
    MSortArgs msa;
    size_t i, j, lo, hi, run;
    JSBool fastcopy;
    int cmp_result;

    /* Avoid memcpy overhead for word-sized and word-aligned elements. */
    fastcopy = (elsize == sizeof(jsval) &&
                (((jsuword) src | (jsuword) tmp) & JSVAL_ALIGN) == 0);
#define COPY_ONE(p,q,n) \
    (fastcopy ? (void)(*(jsval*)(p) = *(jsval*)(q)) : (void)memcpy(p, q, n))
#define CALL_CMP(a, b) \
    if (!cmp(arg, (a), (b), &cmp_result)) return JS_FALSE;
#define INS_SORT_INT 4

    /*
     * Apply insertion sort to small chunks to reduce the number of merge
     * passes needed.
     */
    for (lo = 0; lo < nel; lo += INS_SORT_INT) {
        hi = lo + INS_SORT_INT;
        if (hi >= nel)
            hi = nel;
        for (i = lo + 1; i < hi; i++) {
            vec1 = (char *)src + i * elsize;
            vec2 = (char *)vec1 - elsize;
            for (j = i; j > lo; j--) {
                CALL_CMP(vec2, vec1);
                /* "<=" instead of "<" insures the sort is stable */
                if (cmp_result <= 0) {
                    break;
                }

                /* Swap elements, using "tmp" as tmp storage */
                COPY_ONE(tmp, vec2, elsize);
                COPY_ONE(vec2, vec1, elsize);
                COPY_ONE(vec1, tmp, elsize);
                vec1 = vec2;
                vec2 = (char *)vec1 - elsize;
            }
        }
    }
#undef CALL_CMP
#undef COPY_ONE

    msa.elsize = elsize;
    msa.cmp = cmp;
    msa.arg = arg;
    msa.fastcopy = fastcopy;

    vec1 = src;
    vec2 = tmp;
    for (run = INS_SORT_INT; run < nel; run *= 2) {
        for (lo = 0; lo < nel; lo += 2 * run) {
            hi = lo + run;
            if (hi >= nel) {
                memcpy((char *)vec2 + lo * elsize, (char *)vec1 + lo * elsize,
                       (nel - lo) * elsize);
                break;
            }
            if (!MergeArrays(&msa, (char *)vec1 + lo * elsize,
                             (char *)vec2 + lo * elsize, run,
                             hi + run > nel ? nel - hi : run)) {
                return JS_FALSE;
            }
        }
        swap = vec1;
        vec1 = vec2;
        vec2 = swap;
    }
    if (src != vec1)
        memcpy(src, tmp, nel * elsize);

    return JS_TRUE;
}

typedef struct CompareArgs {
    JSContext   *context;
    jsval       fval;
    jsval       *elemroot;      /* stack needed for js_Invoke */
} CompareArgs;

static JSBool
sort_compare(void *arg, const void *a, const void *b, int *result)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;
    CompareArgs *ca = (CompareArgs *) arg;
    JSContext *cx = ca->context;
    jsval *invokevp, *sp;
    jsdouble cmp;

    /**
     * array_sort deals with holes and undefs on its own and they should not
     * come here.
     */
    JS_ASSERT(av != JSVAL_VOID);
    JS_ASSERT(bv != JSVAL_VOID);

    if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP))
        return JS_FALSE;

    invokevp = ca->elemroot;
    sp = invokevp;
    *sp++ = ca->fval;
    *sp++ = JSVAL_NULL;
    *sp++ = av;
    *sp++ = bv;

    if (!js_Invoke(cx, 2, invokevp, JSINVOKE_INTERNAL) ||
        !js_ValueToNumber(cx, *invokevp, &cmp)) {
        return JS_FALSE;
    }

    /* Clamp cmp to -1, 0, 1. */
    *result = 0;
    if (!JSDOUBLE_IS_NaN(cmp) && cmp != 0)
        *result = cmp > 0 ? 1 : -1;

    /*
     * XXX else report some kind of error here?  ECMA talks about 'consistent
     * compare functions' that don't return NaN, but is silent about what the
     * result should be.  So we currently ignore it.
     */

    return JS_TRUE;
}

static int
sort_compare_strings(void *arg, const void *a, const void *b, int *result)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;

    JS_ASSERT(JSVAL_IS_STRING(av));
    JS_ASSERT(JSVAL_IS_STRING(bv));
    if (!JS_CHECK_OPERATION_LIMIT((JSContext *)arg, JSOW_JUMP))
        return JS_FALSE;

    *result = (int) js_CompareStrings(JSVAL_TO_STRING(av), JSVAL_TO_STRING(bv));
    return JS_TRUE;
}

/*
 * The array_sort function below assumes JSVAL_NULL is zero in order to
 * perform initialization using memset.  Other parts of SpiderMonkey likewise
 * "know" that JSVAL_NULL is zero; this static assertion covers all cases.
 */
JS_STATIC_ASSERT(JSVAL_NULL == 0);

static JSBool
array_sort(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv, fval, *vec, *mergesort_tmp, v;
    JSObject *obj;
    CompareArgs ca;
    jsuint len, newlen, i, undefs;
    JSTempValueRooter tvr;
    JSBool hole, ok;
    size_t elemsize;
    JSString *str;

    /*
     * Optimize the default compare function case if all of obj's elements
     * have values of type string.
     */
    JSBool all_strings;

    argv = JS_ARGV(cx, vp);
    if (argc > 0) {
        if (JSVAL_IS_PRIMITIVE(argv[0])) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_SORT_ARG);
            return JS_FALSE;
        }
        fval = argv[0];     /* non-default compare function */
    } else {
        fval = JSVAL_NULL;
    }

    obj = JS_THIS_OBJECT(cx, vp);
    if (!js_GetLengthProperty(cx, obj, &len))
        return JS_FALSE;
    if (len == 0) {
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
    }

    /*
     * We need a temporary array of 2 * len jsvals to hold the array elements
     * and the scratch space for merge sort. Check that its size does not
     * overflow size_t, which would allow for indexing beyond the end of the
     * malloc'd vector.
     */
    if (len > (size_t) -1 / (2 * sizeof(jsval))) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    vec = (jsval *) JS_malloc(cx, 2 * (size_t) len * sizeof(jsval));
    if (!vec)
        return JS_FALSE;

    /*
     * Initialize vec as a root. We will clear elements of vec one by
     * one while increasing tvr.count when we know that the property at
     * the corresponding index exists and its value must be rooted.
     *
     * In this way when sorting a huge mostly sparse array we will not
     * access the tail of vec corresponding to properties that do not
     * exist, allowing OS to avoiding committing RAM. See bug 330812.
     *
     * After this point control must flow through label out: to exit.
     */
    JS_PUSH_TEMP_ROOT(cx, 0, vec, &tvr);

    /*
     * By ECMA 262, 15.4.4.11, a property that does not exist (which we
     * call a "hole") is always greater than an existing property with
     * value undefined and that is always greater than any other property.
     * Thus to sort holes and undefs we simply count them, sort the rest
     * of elements, append undefs after them and then make holes after
     * undefs.
     */
    undefs = 0;
    newlen = 0;
    all_strings = JS_TRUE;
    for (i = 0; i < len; i++) {
        ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP);
        if (!ok)
            goto out;

        /* Clear vec[newlen] before including it in the rooted set. */
        vec[newlen] = JSVAL_NULL;
        tvr.count = newlen + 1;
        ok = GetArrayElement(cx, obj, i, &hole, &vec[newlen]);
        if (!ok)
            goto out;

        if (hole)
            continue;

        if (vec[newlen] == JSVAL_VOID) {
            ++undefs;
            continue;
        }

        /* We know JSVAL_IS_STRING yields 0 or 1, so avoid a branch via &=. */
        all_strings &= JSVAL_IS_STRING(vec[newlen]);

        ++newlen;
    }

    if (newlen == 0) {
        /* The array has only holes and undefs. */
        ok = JS_TRUE;
        goto out;
    }

    /*
     * The first newlen elements of vec are copied from the array object
     * (above). The remaining newlen positions are used as GC-rooted scratch
     * space for mergesort. We must clear the space before including it to
     * the root set covered by tvr.count. We assume JSVAL_NULL==0 to optimize
     * initialization using memset.
     */
    mergesort_tmp = vec + newlen;
    memset(mergesort_tmp, 0, newlen * sizeof(jsval));
    tvr.count = newlen * 2;

    /* Here len == 2 * (newlen + undefs + number_of_holes). */
    if (fval == JSVAL_NULL) {
        /*
         * Sort using the default comparator converting all elements to
         * strings.
         */
        if (all_strings) {
            elemsize = sizeof(jsval);
        } else {
            /*
             * To avoid string conversion on each compare we do it only once
             * prior to sorting. But we also need the space for the original
             * values to recover the sorting result. To reuse
             * sort_compare_strings we move the original values to the odd
             * indexes in vec, put the string conversion results in the even
             * indexes and pass 2 * sizeof(jsval) as an element size to the
             * sorting function. In this way sort_compare_strings will only
             * see the string values when it casts the compare arguments as
             * pointers to jsval.
             *
             * This requires doubling the temporary storage including the
             * scratch space for the merge sort. Since vec already contains
             * the rooted scratch space for newlen elements at the tail, we
             * can use it to rearrange and convert to strings first and try
             * realloc only when we know that we successfully converted all
             * the elements.
             */
            if (newlen > (size_t) -1 / (4 * sizeof(jsval))) {
                JS_ReportOutOfMemory(cx);
                ok = JS_FALSE;
                goto out;
            }

            /*
             * Rearrange and string-convert the elements of the vector from
             * the tail here and, after sorting, move the results back
             * starting from the start to prevent overwrite the existing
             * elements.
             */
            i = newlen;
            do {
                --i;
                ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP);
                if (!ok)
                    goto out;
                v = vec[i];
                str = js_ValueToString(cx, v);
                if (!str) {
                    ok = JS_FALSE;
                    goto out;
                }
                vec[2 * i] = STRING_TO_JSVAL(str);
                vec[2 * i + 1] = v;
            } while (i != 0);

            JS_ASSERT(tvr.u.array == vec);
            vec = JS_realloc(cx, vec, 4 * (size_t) newlen * sizeof(jsval));
            if (!vec) {
                vec = tvr.u.array;
                ok = JS_FALSE;
                goto out;
            }
            tvr.u.array = vec;
            mergesort_tmp = vec + 2 * newlen;
            memset(mergesort_tmp, 0, newlen * 2 * sizeof(jsval));
            tvr.count = newlen * 4;
            elemsize = 2 * sizeof(jsval);
        }
        ok = js_MergeSort(vec, (size_t) newlen, elemsize,
                          sort_compare_strings, cx, mergesort_tmp);
        if (!ok)
            goto out;
        if (!all_strings) {
            /*
             * We want to make the following loop fast and to unroot the
             * cached results of toString invocations before the operation
             * callback has a chance to run the GC. For this reason we do
             * not call JS_CHECK_OPERATION_LIMIT in the loop.
             */
            i = 0;
            do {
                vec[i] = vec[2 * i + 1];
            } while (++i != newlen);
        }
    } else {
        void *mark;

        ca.context = cx;
        ca.fval = fval;
        ca.elemroot  = js_AllocStack(cx, 2 + 2, &mark);
        if (!ca.elemroot) {
            ok = JS_FALSE;
            goto out;
        }
        ok = js_MergeSort(vec, (size_t) newlen, sizeof(jsval),
                          sort_compare, &ca, mergesort_tmp);
        js_FreeStack(cx, mark);
        if (!ok)
            goto out;
    }

    /*
     * We no longer need to root the scratch space for the merge sort, so
     * unroot it now to make the job of a potential GC under InitArrayElements
     * easier.
     */
    tvr.count = newlen;
    ok = InitArrayElements(cx, obj, 0, newlen, vec);
    if (!ok)
        goto out;

  out:
    JS_POP_TEMP_ROOT(cx, &tvr);
    JS_free(cx, vec);
    if (!ok)
        return JS_FALSE;

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) ||
            !SetArrayElement(cx, obj, newlen++, JSVAL_VOID)) {
            return JS_FALSE;
        }
    }

    /* Re-create any holes that sorted to the end of the array. */
    while (len > newlen) {
        if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) ||
            !DeleteArrayElement(cx, obj, --len)) {
            return JS_FALSE;
        }
    }
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/*
 * Perl-inspired push, pop, shift, unshift, and splice methods.
 */
static JSBool
slow_array_push(JSContext *cx, JSObject *obj, uintN argc, jsval *vp)
{
    jsuint length, newlength;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    newlength = length + argc;
    if (!InitArrayElements(cx, obj, length, newlength, vp + 2))
        return JS_FALSE;

    /* Per ECMA-262, return the new array length. */
    if (!IndexToValue(cx, newlength, vp))
        return JS_FALSE;
    return js_SetLengthProperty(cx, obj, newlength);
}

static JSBool
array_push(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsval v;

    /* Insist on one argument and obj of the expected class. */
    obj = JSVAL_TO_OBJECT(vp[1]);
    if (argc != 1 || OBJ_GET_CLASS(cx, obj) != &js_ArrayClass)
        return slow_array_push(cx, obj, argc, vp);

    /* Beware 'length' int to double jsval overflow. */
    v = STOBJ_GET_SLOT(obj, JSSLOT_ARRAY_LENGTH);
    if (!(v & JSVAL_INT) || v == INT_TO_JSVAL(JSVAL_INT_MAX))
        return slow_array_push(cx, obj, argc, vp);

    /*
     * Ok, we can optimize to define a new property identified by the
     * current value of 'length'.  We know no such property exists,
     * because if one did, then js_ArrayClass.addProperty would have
     * increased the value of 'length' to one greater than this index
     * (proof by contradiction).
     */
    if (!js_DefineNativeProperty(cx, obj, INT_JSVAL_TO_JSID(v), vp[2],
                                 NULL, NULL, JSPROP_ENUMERATE, 0, 0,
                                 NULL)) {
        return JS_FALSE;
    }
    v += 2;
    JS_ASSERT(STOBJ_GET_SLOT(obj, JSSLOT_ARRAY_LENGTH) == v);
    *vp = v;
    return JS_TRUE;
}

JSBool
array_pop(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint index;
    JSBool hole;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!js_GetLengthProperty(cx, obj, &index))
        return JS_FALSE;
    if (index == 0) {
        *vp = JSVAL_VOID;
    } else {
        index--;

        /* Get the to-be-deleted property's value into vp. */
        if (!GetArrayElement(cx, obj, index, &hole, vp))
            return JS_FALSE;
        if (!hole && !DeleteArrayElement(cx, obj, index))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, index);
}

static JSBool
array_shift(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint length, i;
    JSBool hole, ok;
    JSTempValueRooter tvr;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0) {
        *vp = JSVAL_VOID;
    } else {
        length--;

        /* Get the to-be-deleted property's value into vp ASAP. */
        if (!GetArrayElement(cx, obj, 0, &hole, vp))
            return JS_FALSE;

        /* Slide down the array above the first element. */
        ok = JS_TRUE;
        JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);
        for (i = 0; i != length; i++) {
            ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                 GetArrayElement(cx, obj, i + 1, &hole, &tvr.u.value) &&
                 SetOrDeleteArrayElement(cx, obj, i, hole, tvr.u.value);
            if (!ok)
                break;
        }
        JS_POP_TEMP_ROOT(cx, &tvr);
        if (!ok)
            return JS_FALSE;

        /* Delete the only or last element when it exist. */
        if (!hole && !DeleteArrayElement(cx, obj, length))
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsval *argv;
    jsuint length, last;
    JSBool hole, ok;
    JSTempValueRooter tvr;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (argc > 0) {
        /* Slide up the array to make room for argc at the bottom. */
        argv = JS_ARGV(cx, vp);
        if (length > 0) {
            last = length;
            ok = JS_TRUE;
            JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);
            do {
                --last;
                ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                     GetArrayElement(cx, obj, last, &hole, &tvr.u.value) &&
                     SetOrDeleteArrayElement(cx, obj, last + argc, hole,
                                             tvr.u.value);
                if (!ok)
                    break;
            } while (last != 0);
            JS_POP_TEMP_ROOT(cx, &tvr);
            if (!ok)
                return JS_FALSE;
        }

        /* Copy from argv to the bottom of the array. */
        if (!InitArrayElements(cx, obj, 0, argc, argv))
            return JS_FALSE;

        length += argc;
        if (!js_SetLengthProperty(cx, obj, length))
            return JS_FALSE;
    }

    /* Follow Perl by returning the new array length. */
    return IndexToValue(cx, length, vp);
}

static JSBool
array_splice(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    JSObject *obj;
    jsuint length, begin, end, count, delta, last;
    jsdouble d;
    JSBool hole, ok;
    JSObject *obj2;
    JSTempValueRooter tvr;

    /* Nothing to do if no args.  Otherwise get length. */
    if (argc == 0)
        return JS_TRUE;
    argv = JS_ARGV(cx, vp);
    obj = JS_THIS_OBJECT(cx, vp);
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

    /*
     * Create a new array value to return.  Our ECMA v2 proposal specs
     * that splice always returns an array value, even when given no
     * arguments.  We think this is best because it eliminates the need
     * for callers to do an extra test to handle the empty splice case.
     */
    obj2 = js_NewArrayObject(cx, 0, NULL);
    if (!obj2)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(obj2);

    /* After this, control must flow through label out: to exit. */
    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);

    /* If there are elements to remove, put them into the return value. */
    if (count > 0) {
        for (last = begin; last < end; last++) {
            ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                 GetArrayElement(cx, obj, last, &hole, &tvr.u.value);
            if (!ok)
                goto out;

            /* Copy tvr.u.value to new array unless it's a hole. */
            if (!hole) {
                ok = SetArrayElement(cx, obj2, last - begin, tvr.u.value);
                if (!ok)
                    goto out;
            }
        }

        ok = js_SetLengthProperty(cx, obj2, end - begin);
        if (!ok)
            goto out;
    }

    /* Find the direction (up or down) to copy and make way for argv. */
    if (argc > count) {
        delta = (jsuint)argc - count;
        last = length;
        /* (uint) end could be 0, so can't use vanilla >= test */
        while (last-- > end) {
            ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                 GetArrayElement(cx, obj, last, &hole, &tvr.u.value) &&
                 SetOrDeleteArrayElement(cx, obj, last + delta, hole,
                                         tvr.u.value);
            if (!ok)
                goto out;
        }
        length += delta;
    } else if (argc < count) {
        delta = count - (jsuint)argc;
        for (last = end; last < length; last++) {
            ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                 GetArrayElement(cx, obj, last, &hole, &tvr.u.value) &&
                 SetOrDeleteArrayElement(cx, obj, last - delta, hole,
                                         tvr.u.value);
            if (!ok)
                goto out;
        }
        length -= delta;
    }

    /* Copy from argv into the hole to complete the splice. */
    ok = InitArrayElements(cx, obj, begin, begin + argc, argv);
    if (!ok)
        goto out;

    /* Update length in case we deleted elements from the end. */
    ok = js_SetLengthProperty(cx, obj, length);

out:
    JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
}

/*
 * Python-esque sequence operations.
 */
static JSBool
array_concat(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv, v;
    JSObject *nobj, *aobj;
    jsuint length, alength, slot;
    uintN i;
    JSBool hole, ok;
    JSTempValueRooter tvr;

    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    argv = JS_ARGV(cx, vp) - 1;
    JS_ASSERT(JS_THIS_OBJECT(cx, vp) == JSVAL_TO_OBJECT(argv[0]));

    /* Create a new Array object and root it using *vp. */
    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(nobj);

    /* After this, control must flow through label out: to exit. */
    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    length = 0;
    for (i = 0; i <= argc; i++) {
        ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP);
        if (!ok)
            goto out;
        v = argv[i];
        if (JSVAL_IS_OBJECT(v)) {
            aobj = JSVAL_TO_OBJECT(v);
            if (aobj && OBJ_GET_CLASS(cx, aobj) == &js_ArrayClass) {
                ok = OBJ_GET_PROPERTY(cx, aobj,
                                      ATOM_TO_JSID(cx->runtime->atomState
                                                   .lengthAtom),
                                      &tvr.u.value);
                if (!ok)
                    goto out;
                ok = ValueIsLength(cx, tvr.u.value, &alength);
                if (!ok)
                    goto out;
                for (slot = 0; slot < alength; slot++) {
                    ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
                         GetArrayElement(cx, aobj, slot, &hole,
                                         &tvr.u.value);
                    if (!ok)
                        goto out;

                    /*
                     * Per ECMA 262, 15.4.4.4, step 9, ignore non-existent
                     * properties.
                     */
                    if (!hole) {
                        ok = SetArrayElement(cx, nobj, length + slot,
                                             tvr.u.value);
                        if (!ok)
                            goto out;
                    }
                }
                length += alength;
                continue;
            }
        }

        ok = SetArrayElement(cx, nobj, length, v);
        if (!ok)
            goto out;
        length++;
    }

    ok = js_SetLengthProperty(cx, nobj, length);

out:
    JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
}

static JSBool
array_slice(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    JSObject *nobj, *obj;
    jsuint length, begin, end, slot;
    jsdouble d;
    JSBool hole, ok;
    JSTempValueRooter tvr;

    argv = JS_ARGV(cx, vp);

    /* Create a new Array object and root it using *vp. */
    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(nobj);

    obj = JS_THIS_OBJECT(cx, vp);
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

    /* After this, control must flow through label out: to exit. */
    JS_PUSH_SINGLE_TEMP_ROOT(cx, JSVAL_NULL, &tvr);

    for (slot = begin; slot < end; slot++) {
        ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
             GetArrayElement(cx, obj, slot, &hole, &tvr.u.value);
        if (!ok)
            goto out;
        if (!hole) {
            ok = SetArrayElement(cx, nobj, slot - begin, tvr.u.value);
            if (!ok)
                goto out;
        }
    }
    ok = js_SetLengthProperty(cx, nobj, end - begin);

out:
    JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
}

#if JS_HAS_ARRAY_EXTRAS

static JSBool
array_indexOfHelper(JSContext *cx, JSBool isLast, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint length, i, stop;
    jsint direction;
    JSBool hole;

    obj = JSVAL_TO_OBJECT(vp[1]);
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0)
        goto not_found;

    if (argc <= 1) {
        i = isLast ? length - 1 : 0;
    } else {
        jsdouble start;

        if (!js_ValueToNumber(cx, vp[3], &start))
            return JS_FALSE;
        start = js_DoubleToInteger(start);
        if (start < 0) {
            start += length;
            if (start < 0) {
                if (isLast)
                    goto not_found;
                i = 0;
            } else {
                i = (jsuint)start;
            }
        } else if (start >= length) {
            if (!isLast)
                goto not_found;
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
        if (!JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) ||
            !GetArrayElement(cx, obj, (jsuint)i, &hole, vp)) {
            return JS_FALSE;
        }
        if (!hole && js_StrictlyEqual(cx, *vp, vp[2]))
            return js_NewNumberValue(cx, i, vp);
        if (i == stop)
            goto not_found;
        i += direction;
    }

  not_found:
    *vp = INT_TO_JSVAL(-1);
    return JS_TRUE;
}

static JSBool
array_indexOf(JSContext *cx, uintN argc, jsval *vp)
{
    return array_indexOfHelper(cx, JS_FALSE, argc, vp);
}

static JSBool
array_lastIndexOf(JSContext *cx, uintN argc, jsval *vp)
{
    return array_indexOfHelper(cx, JS_TRUE, argc, vp);
}

/* Order is important; extras that take a predicate funarg must follow MAP. */
typedef enum ArrayExtraMode {
    FOREACH,
    REDUCE,
    REDUCE_RIGHT,
    MAP,
    FILTER,
    SOME,
    EVERY
} ArrayExtraMode;

#define REDUCE_MODE(mode) ((mode) == REDUCE || (mode) == REDUCE_RIGHT)

static JSBool
array_extra(JSContext *cx, ArrayExtraMode mode, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint length, newlen;
    jsval *argv, *elemroot, *invokevp, *sp;
    JSBool ok, cond, hole;
    JSObject *callable, *thisp, *newarr;
    jsint start, end, step, i;
    void *mark;

    obj = JSVAL_TO_OBJECT(vp[1]);
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /*
     * First, get or compute our callee, so that we error out consistently
     * when passed a non-callable object.
     */
    argv = vp + 2;
    callable = js_ValueToCallableObject(cx, &argv[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return JS_FALSE;

    /*
     * Set our initial return condition, used for zero-length array cases
     * (and pre-size our map return to match our known length, for all cases).
     */
#ifdef __GNUC__ /* quell GCC overwarning */
    newlen = 0;
    newarr = NULL;
#endif
    start = 0, end = length, step = 1;

    switch (mode) {
      case REDUCE_RIGHT:
        start = length - 1, end = -1, step = -1;
        /* FALL THROUGH */
      case REDUCE:
        if (length == 0 && argc == 1) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_EMPTY_ARRAY_REDUCE);
            return JS_FALSE;
        }
        if (argc >= 2) {
            *vp = argv[1];
        } else {
            do {
                if (!GetArrayElement(cx, obj, start, &hole, vp))
                    return JS_FALSE;
                start += step;
            } while (hole && start != end);

            if (hole && start == end) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_EMPTY_ARRAY_REDUCE);
                return JS_FALSE;
            }
        }
        break;
      case MAP:
      case FILTER:
        newlen = (mode == MAP) ? length : 0;
        newarr = js_NewArrayObject(cx, newlen, NULL);
        if (!newarr)
            return JS_FALSE;
        *vp = OBJECT_TO_JSVAL(newarr);
        break;
      case SOME:
        *vp = JSVAL_FALSE;
        break;
      case EVERY:
        *vp = JSVAL_TRUE;
        break;
      case FOREACH:
        *vp = JSVAL_VOID;
        break;
    }

    if (length == 0)
        return JS_TRUE;

    if (argc > 1 && !REDUCE_MODE(mode)) {
        if (!js_ValueToObject(cx, argv[1], &thisp))
            return JS_FALSE;
        argv[1] = OBJECT_TO_JSVAL(thisp);
    } else {
        thisp = NULL;
    }

    /*
     * For all but REDUCE, we call with 3 args (value, index, array). REDUCE
     * requires 4 args (accum, value, index, array).
     */
    argc = 3 + REDUCE_MODE(mode);
    elemroot = js_AllocStack(cx, 1 + 2 + argc, &mark);
    if (!elemroot)
        return JS_FALSE;

    /* From this point the control must flow through out:. */
    ok = JS_TRUE;
    invokevp = elemroot + 1;

    for (i = start; i != end; i += step) {
        ok = JS_CHECK_OPERATION_LIMIT(cx, JSOW_JUMP) &&
             GetArrayElement(cx, obj, i, &hole, elemroot);
        if (!ok)
            goto out;
        if (hole)
            continue;

        /*
         * Push callable and 'this', then args. We must do this for every
         * iteration around the loop since js_Invoke uses spbase[0] for return
         * value storage, while some native functions use spbase[1] for local
         * rooting.
         */
        sp = invokevp;
        *sp++ = OBJECT_TO_JSVAL(callable);
        *sp++ = OBJECT_TO_JSVAL(thisp);
        if (REDUCE_MODE(mode))
            *sp++ = *vp;
        *sp++ = *elemroot;
        *sp++ = INT_TO_JSVAL(i);
        *sp++ = OBJECT_TO_JSVAL(obj);

        /* Do the call. */
        ok = js_Invoke(cx, argc, invokevp, JSINVOKE_INTERNAL);
        if (!ok)
            break;

        if (mode > MAP) {
            if (*invokevp == JSVAL_NULL) {
                cond = JS_FALSE;
            } else if (JSVAL_IS_BOOLEAN(*invokevp)) {
                cond = JSVAL_TO_BOOLEAN(*invokevp);
            } else {
                ok = js_ValueToBoolean(cx, *invokevp, &cond);
                if (!ok)
                    goto out;
            }
        }

        switch (mode) {
          case FOREACH:
            break;
          case REDUCE:
          case REDUCE_RIGHT:
            *vp = *invokevp;
            break;
          case MAP:
            ok = SetArrayElement(cx, newarr, i, *invokevp);
            if (!ok)
                goto out;
            break;
          case FILTER:
            if (!cond)
                break;
            /* The filter passed *elemroot, so push it onto our result. */
            ok = SetArrayElement(cx, newarr, newlen++, *elemroot);
            if (!ok)
                goto out;
            break;
          case SOME:
            if (cond) {
                *vp = JSVAL_TRUE;
                goto out;
            }
            break;
          case EVERY:
            if (!cond) {
                *vp = JSVAL_FALSE;
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
array_forEach(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, FOREACH, argc, vp);
}

static JSBool
array_map(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, MAP, argc, vp);
}

static JSBool
array_reduce(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, REDUCE, argc, vp);
}

static JSBool
array_reduceRight(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, REDUCE_RIGHT, argc, vp);
}

static JSBool
array_filter(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, FILTER, argc, vp);
}

static JSBool
array_some(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, SOME, argc, vp);
}

static JSBool
array_every(JSContext *cx, uintN argc, jsval *vp)
{
    return array_extra(cx, EVERY, argc, vp);
}
#endif

static JSPropertySpec array_props[] = {
    {js_length_str,   -1,   JSPROP_SHARED | JSPROP_PERMANENT,
                            array_length_getter,    array_length_setter},
    {0,0,0,0,0}
};

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,      array_toSource,     0,0,0),
#endif
    JS_FN(js_toString_str,      array_toString,     0,0,0),
    JS_FN(js_toLocaleString_str,array_toLocaleString,0,0,0),

    /* Perl-ish methods. */
    JS_FN("join",               array_join,         1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("reverse",            array_reverse,      0,0,JSFUN_GENERIC_NATIVE),
    JS_FN("sort",               array_sort,         0,1,JSFUN_GENERIC_NATIVE),
    JS_FN("push",               array_push,         1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("pop",                array_pop,          0,0,JSFUN_GENERIC_NATIVE),
    JS_FN("shift",              array_shift,        0,0,JSFUN_GENERIC_NATIVE),
    JS_FN("unshift",            array_unshift,      0,1,JSFUN_GENERIC_NATIVE),
    JS_FN("splice",             array_splice,       0,2,JSFUN_GENERIC_NATIVE),

    /* Python-esque sequence methods. */
    JS_FN("concat",             array_concat,       0,1,JSFUN_GENERIC_NATIVE),
    JS_FN("slice",              array_slice,        0,2,JSFUN_GENERIC_NATIVE),

#if JS_HAS_ARRAY_EXTRAS
    JS_FN("indexOf",            array_indexOf,      1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("lastIndexOf",        array_lastIndexOf,  1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("forEach",            array_forEach,      1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("map",                array_map,          1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduce",             array_reduce,       1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduceRight",        array_reduceRight,  1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("filter",             array_filter,       1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("some",               array_some,         1,1,JSFUN_GENERIC_NATIVE),
    JS_FN("every",              array_every,        1,1,JSFUN_GENERIC_NATIVE),
#endif

    JS_FS_END
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
                         array_props, array_methods, NULL, NULL);

    /* Initialize the Array prototype object so it gets a length property. */
    if (!proto || !InitArrayObject(cx, proto, 0, NULL))
        return NULL;
    return proto;
}

JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, jsval *vector)
{
    JSTempValueRooter tvr;
    JSObject *obj;

    obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
    if (!obj)
        return NULL;

    JS_PUSH_TEMP_ROOT_OBJECT(cx, obj, &tvr);
    if (!InitArrayObject(cx, obj, length, vector))
        obj = NULL;
    JS_POP_TEMP_ROOT(cx, &tvr);

    /* Set/clear newborn root, in case we lost it.  */
    cx->weakRoots.newborn[GCX_OBJECT] = obj;
    return obj;
}
