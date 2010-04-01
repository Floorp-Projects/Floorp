/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Array objects begin as "dense" arrays, optimized for index-only property
 * access over a vector of slots (obj->dslots) with high load factor.  Array
 * methods optimize for denseness by testing that the object's class is
 * &js_ArrayClass, and can then directly manipulate the slots for efficiency.
 *
 * We track these pieces of metadata for arrays in dense mode:
 *  - the array's length property as a uint32, in JSSLOT_ARRAY_LENGTH,
 *  - the number of indices that are filled (non-holes), in JSSLOT_ARRAY_COUNT,
 *  - the net number of slots starting at dslots (capacity), in dslots[-1] if
 *    dslots is non-NULL.
 *
 * In dense mode, holes in the array are represented by JSVAL_HOLE.  The final
 * slot in fslots is unused.
 *
 * NB: the capacity and length of a dense array are entirely unrelated!  The
 * length may be greater than, less than, or equal to the capacity.  See
 * array_length_setter for an explanation of how the first, most surprising
 * case may occur.
 *
 * Arrays are converted to use js_SlowArrayClass when any of these conditions
 * are met:
 *  - the load factor (COUNT / capacity) is less than 0.25, and there are
 *    more than MIN_SPARSE_INDEX slots total
 *  - a property is set that is not indexed (and not "length"); or
 *  - a property is defined that has non-default property attributes.
 *
 * Dense arrays do not track property creation order, so unlike other native
 * objects and slow arrays, enumerating an array does not necessarily visit the
 * properties in the order they were created.  We could instead maintain the
 * scope to track property enumeration order, but still use the fast slot
 * access.  That would have the same memory cost as just using a
 * js_SlowArrayClass, but have the same performance characteristics as a dense
 * array for slot accesses, at some cost in code complexity.
 */
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbit.h"
#include "jsbool.h"
#include "jstracer.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h" /* for js_TraceWatchPoints */
#include "jsdtoa.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jsstaticcheck.h"
#include "jsvector.h"

#include "jsatominlines.h"

using namespace js;

/* 2^32 - 1 as a number and a string */
#define MAXINDEX 4294967295u
#define MAXSTR   "4294967295"

/* Small arrays are dense, no matter what. */
#define MIN_SPARSE_INDEX 256

static inline bool
INDEX_TOO_BIG(jsuint index)
{
    return index > JS_BIT(29) - 1;
}

#define INDEX_TOO_SPARSE(array, index)                                         \
    (INDEX_TOO_BIG(index) ||                                                   \
     ((index) > js_DenseArrayCapacity(array) && (index) >= MIN_SPARSE_INDEX && \
      (index) > (uint32)((array)->fslots[JSSLOT_ARRAY_COUNT] + 1) * 4))

JS_STATIC_ASSERT(sizeof(JSScopeProperty) > 4 * sizeof(jsval));

#define ENSURE_SLOW_ARRAY(cx, obj)                                             \
    (OBJ_GET_CLASS(cx, obj) == &js_SlowArrayClass || js_MakeArraySlow(cx, obj))

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
    cp = str->chars();
    if (JS7_ISDEC(*cp) && str->length() < sizeof(MAXSTR)) {
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

static jsuint
ValueIsLength(JSContext *cx, jsval* vp)
{
    jsint i;
    jsdouble d;
    jsuint length;

    if (JSVAL_IS_INT(*vp)) {
        i = JSVAL_TO_INT(*vp);
        if (i < 0)
            goto error;
        return (jsuint) i;
    }

    d = js_ValueToNumber(cx, vp);
    if (JSVAL_IS_NULL(*vp))
        goto error;

    if (JSDOUBLE_IS_NaN(d))
        goto error;
    length = (jsuint) d;
    if (d != (jsdouble) length)
        goto error;
    return length;

  error:
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_BAD_ARRAY_LENGTH);
    *vp = JSVAL_NULL;
    return 0;
}

JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    if (obj->isArray()) {
        *lengthp = obj->fslots[JSSLOT_ARRAY_LENGTH];
        return true;
    }

    if (obj->isArguments() && !IsOverriddenArgsLength(obj)) {
        *lengthp = GetArgsLength(obj);
        return true;
    }

    AutoValueRooter tvr(cx, JSVAL_NULL);
    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom), tvr.addr()))
        return false;

    if (JSVAL_IS_INT(tvr.value())) {
        *lengthp = jsuint(jsint(JSVAL_TO_INT(tvr.value()))); /* jsuint cast does ToUint32 */
        return true;
    }

    *lengthp = js_ValueToECMAUint32(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

static JSBool
IndexToValue(JSContext *cx, jsdouble index, jsval *vp)
{
    return js_NewWeaklyRootedNumber(cx, index, vp);
}

JSBool JS_FASTCALL
js_IndexToId(JSContext *cx, jsuint index, jsid *idp)
{
    JSString *str;

    if (index <= JSVAL_INT_MAX) {
        *idp = INT_TO_JSID(index);
        return JS_TRUE;
    }
    str = js_NumberToString(cx, index);
    if (!str)
        return JS_FALSE;
    return js_ValueToStringId(cx, STRING_TO_JSVAL(str), idp);
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
     * exist.  Fast arrays (clasp == &js_ArrayClass) don't use atoms for
     * any indexes, though it would be rare to see them have a big index
     * in any case.
     */
    if (!createAtom &&
        ((clasp = OBJ_GET_CLASS(cx, obj)) == &js_SlowArrayClass ||
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

static JSBool
ResizeSlots(JSContext *cx, JSObject *obj, uint32 oldlen, uint32 newlen,
            bool initializeAllSlots = true)
{
    jsval *slots, *newslots;

    if (newlen == 0) {
        if (obj->dslots) {
            cx->free(obj->dslots - 1);
            obj->dslots = NULL;
        }
        return JS_TRUE;
    }

    if (newlen > MAX_DSLOTS_LENGTH) {
        js_ReportAllocationOverflow(cx);
        return JS_FALSE;
    }

    slots = obj->dslots ? obj->dslots - 1 : NULL;
    newslots = (jsval *) cx->realloc(slots, (size_t(newlen) + 1) * sizeof(jsval));
    if (!newslots)
        return JS_FALSE;

    obj->dslots = newslots + 1;
    js_SetDenseArrayCapacity(obj, newlen);

    if (initializeAllSlots) {
        for (slots = obj->dslots + oldlen; slots < obj->dslots + newlen; slots++)
            *slots = JSVAL_HOLE;
    }

    return JS_TRUE;
}

/*
 * When a dense array with CAPACITY_DOUBLING_MAX or fewer slots needs to grow,
 * double its capacity, to push() N elements in amortized O(N) time.
 *
 * Above this limit, grow by 12.5% each time. Speed is still amortized O(N),
 * with a higher constant factor, and we waste less space.
 */
#define CAPACITY_DOUBLING_MAX    (1024 * 1024)

/*
 * Round up all large allocations to a multiple of this (1MB), so as not to
 * waste space if malloc gives us 1MB-sized chunks (as jemalloc does).
 */
#define CAPACITY_CHUNK  (1024 * 1024 / sizeof(jsval))

static JSBool
EnsureCapacity(JSContext *cx, JSObject *obj, uint32 newcap,
               bool initializeAllSlots = true)
{
    uint32 oldcap = js_DenseArrayCapacity(obj);

    if (newcap > oldcap) {
        /*
         * If this overflows uint32, newcap is very large. nextsize will end
         * up being less than newcap, the code below will thus disregard it,
         * and ResizeSlots will fail.
         *
         * The way we use dslots[-1] forces a few +1s and -1s here. For
         * example, (oldcap * 2 + 1) produces the sequence 7, 15, 31, 63, ...
         * which makes the total allocation size (with dslots[-1]) a power
         * of two.
         */
        uint32 nextsize = (oldcap <= CAPACITY_DOUBLING_MAX)
                          ? oldcap * 2 + 1
                          : oldcap + (oldcap >> 3);

        uint32 actualCapacity = JS_MAX(newcap, nextsize);
        if (actualCapacity >= CAPACITY_CHUNK)
            actualCapacity = JS_ROUNDUP(actualCapacity + 1, CAPACITY_CHUNK) - 1; /* -1 for dslots[-1] */
        else if (actualCapacity < ARRAY_CAPACITY_MIN)
            actualCapacity = ARRAY_CAPACITY_MIN;
        if (!ResizeSlots(cx, obj, oldcap, actualCapacity, initializeAllSlots))
            return JS_FALSE;
        if (!initializeAllSlots) {
            /*
             * Initialize the slots caller didn't actually ask for.
             */
            for (jsval *slots = obj->dslots + newcap;
                 slots < obj->dslots + actualCapacity;
                 slots++) {
                *slots = JSVAL_HOLE;
            }
        }
    }
    return JS_TRUE;
}

static bool
ReallyBigIndexToId(JSContext* cx, jsdouble index, jsid* idp)
{
    AutoValueRooter dval(cx);
    if (!js_NewDoubleInRootedValue(cx, index, dval.addr()) ||
        !js_ValueToStringId(cx, dval.value(), idp)) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

static bool
IndexToId(JSContext* cx, JSObject* obj, jsdouble index, JSBool* hole, jsid* idp,
          JSBool createAtom = JS_FALSE)
{
    if (index <= JSVAL_INT_MAX) {
        *idp = INT_TO_JSID(int(index));
        return JS_TRUE;
    }

    if (index <= jsuint(-1)) {
        if (!BigIndexToId(cx, obj, jsuint(index), createAtom, idp))
            return JS_FALSE;
        if (hole && JSVAL_IS_VOID(*idp))
            *hole = JS_TRUE;
        return JS_TRUE;
    }

    return ReallyBigIndexToId(cx, index, idp);
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
static JSBool
GetArrayElement(JSContext *cx, JSObject *obj, jsdouble index, JSBool *hole,
                jsval *vp)
{
    JS_ASSERT(index >= 0);
    if (obj->isDenseArray() && index < js_DenseArrayCapacity(obj) &&
        (*vp = obj->dslots[jsuint(index)]) != JSVAL_HOLE) {
        *hole = JS_FALSE;
        return JS_TRUE;
    }

    AutoIdRooter idr(cx);

    *hole = JS_FALSE;
    if (!IndexToId(cx, obj, index, hole, idr.addr()))
        return JS_FALSE;
    if (*hole) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JSObject *obj2;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, idr.id(), &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        *hole = JS_TRUE;
        *vp = JSVAL_VOID;
    } else {
        obj2->dropProperty(cx, prop);
        if (!obj->getProperty(cx, idr.id(), vp))
            return JS_FALSE;
        *hole = JS_FALSE;
    }
    return JS_TRUE;
}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static JSBool
SetArrayElement(JSContext *cx, JSObject *obj, jsdouble index, jsval v)
{
    JS_ASSERT(index >= 0);

    if (obj->isDenseArray()) {
        /* Predicted/prefetched code should favor the remains-dense case. */
        if (index <= jsuint(-1)) {
            jsuint idx = jsuint(index);
            if (!INDEX_TOO_SPARSE(obj, idx)) {
                JS_ASSERT(idx + 1 > idx);
                if (!EnsureCapacity(cx, obj, idx + 1))
                    return JS_FALSE;
                if (idx >= uint32(obj->fslots[JSSLOT_ARRAY_LENGTH]))
                    obj->fslots[JSSLOT_ARRAY_LENGTH] = idx + 1;
                if (obj->dslots[idx] == JSVAL_HOLE)
                    obj->fslots[JSSLOT_ARRAY_COUNT]++;
                obj->dslots[idx] = v;
                return JS_TRUE;
            }
        }

        if (!js_MakeArraySlow(cx, obj))
            return JS_FALSE;
    }

    AutoIdRooter idr(cx);

    if (!IndexToId(cx, obj, index, NULL, idr.addr(), JS_TRUE))
        return JS_FALSE;
    JS_ASSERT(!JSVAL_IS_VOID(idr.id()));

    return obj->setProperty(cx, idr.id(), &v);
}

static JSBool
DeleteArrayElement(JSContext *cx, JSObject *obj, jsdouble index)
{
    JS_ASSERT(index >= 0);
    if (obj->isDenseArray()) {
        if (index <= jsuint(-1)) {
            jsuint idx = jsuint(index);
            if (!INDEX_TOO_SPARSE(obj, idx) && idx < js_DenseArrayCapacity(obj)) {
                if (obj->dslots[idx] != JSVAL_HOLE)
                    obj->fslots[JSSLOT_ARRAY_COUNT]--;
                obj->dslots[idx] = JSVAL_HOLE;
                return JS_TRUE;
            }
        }
        return JS_TRUE;
    }

    AutoIdRooter idr(cx);

    if (!IndexToId(cx, obj, index, NULL, idr.addr()))
        return JS_FALSE;
    if (JSVAL_IS_VOID(idr.id()))
        return JS_TRUE;

    jsval junk;
    return obj->deleteProperty(cx, idr.id(), &junk);
}

/*
 * When hole is true, delete the property at the given index. Otherwise set
 * its value to v assuming v is rooted.
 */
static JSBool
SetOrDeleteArrayElement(JSContext *cx, JSObject *obj, jsdouble index,
                        JSBool hole, jsval v)
{
    if (hole) {
        JS_ASSERT(JSVAL_IS_VOID(v));
        return DeleteArrayElement(cx, obj, index);
    }
    return SetArrayElement(cx, obj, index, v);
}

JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsdouble length)
{
    jsval v;
    jsid id;

    if (!IndexToValue(cx, length, &v))
        return JS_FALSE;
    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    return obj->setProperty(cx, id, &v);
}

JSBool
js_HasLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    JSErrorReporter older = JS_SetErrorReporter(cx, NULL);
    AutoValueRooter tvr(cx, JSVAL_NULL);
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    JSBool ok = obj->getProperty(cx, id, tvr.addr());
    JS_SetErrorReporter(cx, older);
    if (!ok)
        return false;

    *lengthp = ValueIsLength(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JSBool
js_IsArrayLike(JSContext *cx, JSObject *obj, JSBool *answerp, jsuint *lengthp)
{
    JSObject *wrappedObj = js_GetWrappedObject(cx, obj);

    *answerp = wrappedObj->isArguments() || wrappedObj->isArray();
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
        if (obj->isArray())
            return IndexToValue(cx, obj->fslots[JSSLOT_ARRAY_LENGTH], vp);
    } while ((obj = obj->getProto()) != NULL);
    return JS_TRUE;
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint newlen, oldlen, gap, index;
    jsval junk;

    if (!obj->isArray()) {
        jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);

        return obj->defineProperty(cx, lengthId, *vp, NULL, NULL, JSPROP_ENUMERATE);
    }

    newlen = ValueIsLength(cx, vp);
    if (JSVAL_IS_NULL(*vp))
        return false;
    oldlen = obj->fslots[JSSLOT_ARRAY_LENGTH];

    if (oldlen == newlen)
        return true;

    if (!IndexToValue(cx, newlen, vp))
        return false;

    if (oldlen < newlen) {
        obj->fslots[JSSLOT_ARRAY_LENGTH] = newlen;
        return true;
    }

    if (obj->isDenseArray()) {
        /* Don't reallocate if we're not actually shrinking our slots. */
        jsuint capacity = js_DenseArrayCapacity(obj);
        if (capacity > newlen && !ResizeSlots(cx, obj, capacity, newlen))
            return false;
    } else if (oldlen - newlen < (1 << 24)) {
        do {
            --oldlen;
            if (!JS_CHECK_OPERATION_LIMIT(cx) || !DeleteArrayElement(cx, obj, oldlen))
                return false;
        } while (oldlen != newlen);
    } else {
        /*
         * We are going to remove a lot of indexes in a presumably sparse
         * array. So instead of looping through indexes between newlen and
         * oldlen, we iterate through all properties and remove those that
         * correspond to indexes in the half-open range [newlen, oldlen).  See
         * bug 322135.
         */
        JSObject *iter = JS_NewPropertyIterator(cx, obj);
        if (!iter)
            return false;

        /* Protect iter against GC under JSObject::deleteProperty. */
        AutoValueRooter tvr(cx, iter);

        gap = oldlen - newlen;
        for (;;) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) || !JS_NextProperty(cx, iter, &id))
                return false;
            if (JSVAL_IS_VOID(id))
                break;
            if (js_IdIsIndex(id, &index) && index - newlen < gap &&
                !obj->deleteProperty(cx, id, &junk)) {
                return false;
            }
        }
    }

    obj->fslots[JSSLOT_ARRAY_LENGTH] = newlen;
    return true;
}

/*
 * We have only indexed properties up to capacity (excepting holes), plus the
 * length property. For all else, we delegate to the prototype.
 */
static inline bool
IsDenseArrayId(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(obj->isDenseArray());

    uint32 i;
    return id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom) ||
           (js_IdIsIndex(id, &i) &&
            obj->fslots[JSSLOT_ARRAY_LENGTH] != 0 &&
            i < js_DenseArrayCapacity(obj) &&
            obj->dslots[i] != JSVAL_HOLE);
}

static JSBool
array_lookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                     JSProperty **propp)
{
    if (!obj->isDenseArray())
        return js_LookupProperty(cx, obj, id, objp, propp);

    if (IsDenseArrayId(cx, obj, id)) {
        *propp = (JSProperty *) id;
        *objp = obj;
        return JS_TRUE;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        *objp = NULL;
        *propp = NULL;
        return JS_TRUE;
    }
    return proto->lookupProperty(cx, id, objp, propp);
}

static void
array_dropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    JS_ASSERT(IsDenseArrayId(cx, obj, (jsid) prop));
}

JSBool
js_GetDenseArrayElementValue(JSContext *cx, JSObject *obj, JSProperty *prop,
                             jsval *vp)
{
    jsid id = (jsid) prop;
    JS_ASSERT(IsDenseArrayId(cx, obj, id));

    uint32 i;
    if (!js_IdIsIndex(id, &i)) {
        JS_ASSERT(id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom));
        return IndexToValue(cx, obj->fslots[JSSLOT_ARRAY_LENGTH], vp);
    }
    *vp = obj->dslots[i];
    return JS_TRUE;
}

static JSBool
array_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    uint32 i;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom))
        return IndexToValue(cx, obj->fslots[JSSLOT_ARRAY_LENGTH], vp);

    if (id == ATOM_TO_JSID(cx->runtime->atomState.protoAtom)) {
        *vp = OBJECT_TO_JSVAL(obj->getProto());
        return JS_TRUE;
    }

    if (!obj->isDenseArray())
        return js_GetProperty(cx, obj, id, vp);

    if (!js_IdIsIndex(ID_TO_VALUE(id), &i) || i >= js_DenseArrayCapacity(obj) ||
        obj->dslots[i] == JSVAL_HOLE) {
        JSObject *obj2;
        JSProperty *prop;
        JSScopeProperty *sprop;

        JSObject *proto = obj->getProto();
        if (!proto) {
            *vp = JSVAL_VOID;
            return JS_TRUE;
        }

        *vp = JSVAL_VOID;
        if (js_LookupPropertyWithFlags(cx, proto, id, cx->resolveFlags,
                                       &obj2, &prop) < 0)
            return JS_FALSE;

        if (prop) {
            if (obj2->isNative()) {
                sprop = (JSScopeProperty *) prop;
                if (!js_NativeGet(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, vp))
                    return JS_FALSE;
            }
            obj2->dropProperty(cx, prop);
        }
        return JS_TRUE;
    }

    *vp = obj->dslots[i];
    return JS_TRUE;
}

static JSBool
slowarray_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint index, length;

    if (!js_IdIsIndex(id, &index))
        return JS_TRUE;
    length = obj->fslots[JSSLOT_ARRAY_LENGTH];
    if (index >= length)
        obj->fslots[JSSLOT_ARRAY_LENGTH] = index + 1;
    return JS_TRUE;
}

static JSBool
slowarray_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                    jsval *statep, jsid *idp);

static JSType
array_typeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

/* The same as js_ObjectOps except for the .enumerate and .call hooks. */
static JSObjectOps js_SlowArrayObjectOps = {
    NULL,
    js_LookupProperty,      js_DefineProperty,
    js_GetProperty,         js_SetProperty,
    js_GetAttributes,       js_SetAttributes,
    js_DeleteProperty,      js_DefaultValue,
    slowarray_enumerate,    js_CheckAccess,
    array_typeOf,           js_TraceObject,
    NULL,                   NATIVE_DROP_PROPERTY,
    NULL,                   js_Construct,
    js_HasInstance,         js_Clear
};

static JSObjectOps *
slowarray_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_SlowArrayObjectOps;
}

static JSBool
array_setProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    uint32 i;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom))
        return array_length_setter(cx, obj, id, vp);

    if (!obj->isDenseArray())
        return js_SetProperty(cx, obj, id, vp);

    if (!js_IdIsIndex(id, &i) || INDEX_TOO_SPARSE(obj, i)) {
        if (!js_MakeArraySlow(cx, obj))
            return JS_FALSE;
        return js_SetProperty(cx, obj, id, vp);
    }

    if (!EnsureCapacity(cx, obj, i + 1))
        return JS_FALSE;

    if (i >= (uint32)obj->fslots[JSSLOT_ARRAY_LENGTH])
        obj->fslots[JSSLOT_ARRAY_LENGTH] = i + 1;
    if (obj->dslots[i] == JSVAL_HOLE)
        obj->fslots[JSSLOT_ARRAY_COUNT]++;
    obj->dslots[i] = *vp;
    return JS_TRUE;
}

JSBool
js_PrototypeHasIndexedProperties(JSContext *cx, JSObject *obj)
{
    /*
     * Walk up the prototype chain and see if this indexed element already
     * exists. If we hit the end of the prototype chain, it's safe to set the
     * element on the original object.
     */
    while ((obj = obj->getProto()) != NULL) {
        /*
         * If the prototype is a non-native object (possibly a dense array), or
         * a native object (possibly a slow array) that has indexed properties,
         * return true.
         */
        if (!obj->isNative())
            return JS_TRUE;
        if (OBJ_SCOPE(obj)->hadIndexedProperties())
            return JS_TRUE;
    }
    return JS_FALSE;
}

#ifdef JS_TRACER

static inline JSBool FASTCALL
dense_grow(JSContext* cx, JSObject* obj, jsint i, jsval v)
{
    /*
     * Let the interpreter worry about negative array indexes.
     */
    JS_ASSERT((MAX_DSLOTS_LENGTH > MAX_DSLOTS_LENGTH32) == (sizeof(jsval) != sizeof(uint32)));
    if (MAX_DSLOTS_LENGTH > MAX_DSLOTS_LENGTH32) {
        /*
         * Have to check for negative values bleeding through on 64-bit machines only,
         * since we can't allocate large enough arrays for this on 32-bit machines.
         */
        if (i < 0)
            return JS_FALSE;
    }

    /*
     * If needed, grow the array as long it remains dense, otherwise fall off trace.
     */
    jsuint u = jsuint(i);
    jsuint capacity = js_DenseArrayCapacity(obj);
    if ((u >= capacity) && (INDEX_TOO_SPARSE(obj, u) || !EnsureCapacity(cx, obj, u + 1)))
        return JS_FALSE;

    if (obj->dslots[u] == JSVAL_HOLE) {
        if (js_PrototypeHasIndexedProperties(cx, obj))
            return JS_FALSE;

        if (u >= jsuint(obj->fslots[JSSLOT_ARRAY_LENGTH]))
            obj->fslots[JSSLOT_ARRAY_LENGTH] = u + 1;
        ++obj->fslots[JSSLOT_ARRAY_COUNT];
    }

    obj->dslots[u] = v;
    return JS_TRUE;
}


JSBool FASTCALL
js_Array_dense_setelem(JSContext* cx, JSObject* obj, jsint i, jsval v)
{
    JS_ASSERT(obj->isDenseArray());
    return dense_grow(cx, obj, i, v);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_Array_dense_setelem, CONTEXT, OBJECT, INT32, JSVAL, 0,
                     nanojit::ACC_STORE_ANY)

JSBool FASTCALL
js_Array_dense_setelem_int(JSContext* cx, JSObject* obj, jsint i, int32 j)
{
    JS_ASSERT(obj->isDenseArray());

    jsval v;
    if (JS_LIKELY(INT_FITS_IN_JSVAL(j))) {
        v = INT_TO_JSVAL(j);
    } else {
        jsdouble d = (jsdouble)j;
        if (!js_NewDoubleInRootedValue(cx, d, &v))
            return JS_FALSE;
    }

    return dense_grow(cx, obj, i, v);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_Array_dense_setelem_int, CONTEXT, OBJECT, INT32, INT32, 0,
                     nanojit::ACC_STORE_ANY)

JSBool FASTCALL
js_Array_dense_setelem_double(JSContext* cx, JSObject* obj, jsint i, jsdouble d)
{
    JS_ASSERT(obj->isDenseArray());

    jsval v;
    jsint j;

    if (JS_LIKELY(JSDOUBLE_IS_INT(d, j) && INT_FITS_IN_JSVAL(j))) {
        v = INT_TO_JSVAL(j);
    } else {
        if (!js_NewDoubleInRootedValue(cx, d, &v))
            return JS_FALSE;
    }

    return dense_grow(cx, obj, i, v);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_Array_dense_setelem_double, CONTEXT, OBJECT, INT32, DOUBLE,
                     0, nanojit::ACC_STORE_ANY)
#endif

static JSBool
array_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                     JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    uint32 i = 0;       // init to shut GCC up
    JSBool isIndex;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom))
        return JS_TRUE;

    isIndex = js_IdIsIndex(ID_TO_VALUE(id), &i);
    if (!isIndex || attrs != JSPROP_ENUMERATE || !obj->isDenseArray() || INDEX_TOO_SPARSE(obj, i)) {
        if (!ENSURE_SLOW_ARRAY(cx, obj))
            return JS_FALSE;
        return js_DefineProperty(cx, obj, id, value, getter, setter, attrs);
    }

    return array_setProperty(cx, obj, id, &value);
}

static JSBool
array_getAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                    uintN *attrsp)
{
    *attrsp = id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)
        ? JSPROP_PERMANENT : JSPROP_ENUMERATE;
    return JS_TRUE;
}

static JSBool
array_setAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                    uintN *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_CANT_SET_ARRAY_ATTRS);
    return JS_FALSE;
}

static JSBool
array_deleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *rval)
{
    uint32 i;

    if (!obj->isDenseArray())
        return js_DeleteProperty(cx, obj, id, rval);

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)) {
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }

    if (js_IdIsIndex(id, &i) && i < js_DenseArrayCapacity(obj) &&
        obj->dslots[i] != JSVAL_HOLE) {
        obj->fslots[JSSLOT_ARRAY_COUNT]--;
        obj->dslots[i] = JSVAL_HOLE;
    }

    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

/*
 * JSObjectOps.enumerate implementation.
 *
 * For a fast array, JSENUMERATE_INIT captures in the enumeration state both
 * the length of the array and the bitmap indicating the positions of holes in
 * the array. This ensures that adding or deleting array elements does not
 * affect the sequence of indexes JSENUMERATE_NEXT returns.
 *
 * For a common case of an array without holes, to represent the state we pack
 * the (nextEnumerationIndex, arrayLength) pair as a pseudo-boolean jsval.
 * This is possible when length <= PACKED_UINT_PAIR_BITS. For arrays with
 * greater length or holes we allocate the JSIndexIterState structure and
 * store it as an int-tagged private pointer jsval. For a slow array we
 * delegate the enumeration implementation to js_Enumerate in
 * slowarray_enumerate.
 *
 * Array mutations can turn a fast array into a slow one after the enumeration
 * starts. When this happens, slowarray_enumerate receives a state created
 * when the array was fast. To distinguish such fast state from a slow state,
 * which is an int-tagged pointer that js_Enumerate creates, we set not one
 * but two lowest bits when tagging a JSIndexIterState pointer -- see
 * INDEX_ITER_TAG usage below. Thus, when slowarray_enumerate receives a state
 * tagged with JSVAL_SPECIAL or with two lowest bits set, it knows that this
 * is a fast state so it calls array_enumerate to continue enumerating the
 * indexes present in the original fast array.
 */

#define PACKED_UINT_PAIR_BITS           14
#define PACKED_UINT_PAIR_MASK           JS_BITMASK(PACKED_UINT_PAIR_BITS)

#define UINT_PAIR_TO_SPECIAL_JSVAL(i,j)                                \
    (JS_ASSERT((uint32) (i) <= PACKED_UINT_PAIR_MASK),                        \
     JS_ASSERT((uint32) (j) <= PACKED_UINT_PAIR_MASK),                        \
     ((jsval) (i) << (PACKED_UINT_PAIR_BITS + JSVAL_TAGBITS)) |               \
     ((jsval) (j) << (JSVAL_TAGBITS)) |                                       \
     (jsval) JSVAL_SPECIAL)

#define SPECIAL_JSVAL_TO_UINT_PAIR(v,i,j)                              \
    (JS_ASSERT(JSVAL_IS_SPECIAL(v)),                                   \
     (i) = (uint32) ((v) >> (PACKED_UINT_PAIR_BITS + JSVAL_TAGBITS)),         \
     (j) = (uint32) ((v) >> JSVAL_TAGBITS) & PACKED_UINT_PAIR_MASK,           \
     JS_ASSERT((i) <= PACKED_UINT_PAIR_MASK))

JS_STATIC_ASSERT(PACKED_UINT_PAIR_BITS * 2 + JSVAL_TAGBITS <= JS_BITS_PER_WORD);

typedef struct JSIndexIterState {
    uint32          index;
    uint32          length;
    JSBool          hasHoles;

    /*
     * Variable-length bitmap representing array's holes. It must not be
     * accessed when hasHoles is false.
     */
    jsbitmap        holes[1];
} JSIndexIterState;

#define INDEX_ITER_TAG      3

JS_STATIC_ASSERT(JSVAL_INT == 1);

static JSBool
array_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                jsval *statep, jsid *idp)
{
    uint32 capacity, i;
    JSIndexIterState *ii;

    switch (enum_op) {
      case JSENUMERATE_INIT:
        JS_ASSERT(obj->isDenseArray());
        capacity = js_DenseArrayCapacity(obj);
        if (idp)
            *idp = INT_TO_JSVAL(obj->fslots[JSSLOT_ARRAY_COUNT]);
        ii = NULL;
        for (i = 0; i != capacity; ++i) {
            if (obj->dslots[i] == JSVAL_HOLE) {
                if (!ii) {
                    ii = (JSIndexIterState *)
                         cx->malloc(offsetof(JSIndexIterState, holes) +
                                   JS_BITMAP_SIZE(capacity));
                    if (!ii)
                        return JS_FALSE;
                    ii->hasHoles = JS_TRUE;
                    memset(ii->holes, 0, JS_BITMAP_SIZE(capacity));
                }
                JS_SET_BIT(ii->holes, i);
            }
        }
        if (!ii) {
            /* Array has no holes. */
            if (capacity <= PACKED_UINT_PAIR_MASK) {
                *statep = UINT_PAIR_TO_SPECIAL_JSVAL(0, capacity);
                break;
            }
            ii = (JSIndexIterState *)
                 cx->malloc(offsetof(JSIndexIterState, holes));
            if (!ii)
                return JS_FALSE;
            ii->hasHoles = JS_FALSE;
        }
        ii->index = 0;
        ii->length = capacity;
        *statep = (jsval) ii | INDEX_ITER_TAG;
        JS_ASSERT(*statep & JSVAL_INT);
        break;

      case JSENUMERATE_NEXT:
        if (JSVAL_IS_SPECIAL(*statep)) {
            SPECIAL_JSVAL_TO_UINT_PAIR(*statep, i, capacity);
            if (i != capacity) {
                *idp = INT_TO_JSID(i);
                *statep = UINT_PAIR_TO_SPECIAL_JSVAL(i + 1, capacity);
                break;
            }
        } else {
            JS_ASSERT((*statep & INDEX_ITER_TAG) == INDEX_ITER_TAG);
            ii = (JSIndexIterState *) (*statep & ~INDEX_ITER_TAG);
            i = ii->index;
            if (i != ii->length) {
                /* Skip holes if any. */
                if (ii->hasHoles) {
                    while (JS_TEST_BIT(ii->holes, i) && ++i != ii->length)
                        continue;
                }
                if (i != ii->length) {
                    ii->index = i + 1;
                    return js_IndexToId(cx, i, idp);
                }
            }
        }
        /* FALL THROUGH */

      case JSENUMERATE_DESTROY:
        if (!JSVAL_IS_SPECIAL(*statep)) {
            JS_ASSERT((*statep & INDEX_ITER_TAG) == INDEX_ITER_TAG);
            ii = (JSIndexIterState *) (*statep & ~INDEX_ITER_TAG);
            cx->free(ii);
        }
        *statep = JSVAL_NULL;
        break;
    }
    return JS_TRUE;
}

static JSBool
slowarray_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                    jsval *statep, jsid *idp)
{
    JSBool ok;

    /* Are we continuing an enumeration that started when we were dense? */
    if (enum_op != JSENUMERATE_INIT) {
        if (JSVAL_IS_SPECIAL(*statep) ||
            (*statep & INDEX_ITER_TAG) == INDEX_ITER_TAG) {
            return array_enumerate(cx, obj, enum_op, statep, idp);
        }
        JS_ASSERT((*statep & INDEX_ITER_TAG) == JSVAL_INT);
    }
    ok = js_Enumerate(cx, obj, enum_op, statep, idp);
    JS_ASSERT(*statep == JSVAL_NULL || (*statep & INDEX_ITER_TAG) == JSVAL_INT);
    return ok;
}

static void
array_finalize(JSContext *cx, JSObject *obj)
{
    if (obj->dslots)
        cx->free(obj->dslots - 1);
    obj->dslots = NULL;
}

static void
array_trace(JSTracer *trc, JSObject *obj)
{
    uint32 capacity;
    size_t i;
    jsval v;

    JS_ASSERT(obj->isDenseArray());
    obj->traceProtoAndParent(trc);

    capacity = js_DenseArrayCapacity(obj);
    for (i = 0; i < capacity; i++) {
        v = obj->dslots[i];
        if (JSVAL_IS_TRACEABLE(v)) {
            JS_SET_TRACING_INDEX(trc, "array_dslots", i);
            js_CallGCMarker(trc, JSVAL_TO_TRACEABLE(v), JSVAL_TRACE_KIND(v));
        }
    }
}

extern JSObjectOps js_ArrayObjectOps;

static const JSObjectMap SharedArrayMap(&js_ArrayObjectOps, JSObjectMap::SHAPELESS);

JSObjectOps js_ArrayObjectOps = {
    &SharedArrayMap,
    array_lookupProperty, array_defineProperty,
    array_getProperty,    array_setProperty,
    array_getAttributes,  array_setAttributes,
    array_deleteProperty, js_DefaultValue,
    array_enumerate,      js_CheckAccess,
    array_typeOf,         array_trace,
    NULL,                 array_dropProperty,
    NULL,                 NULL,
    js_HasInstance,       NULL
};

static JSObjectOps *
array_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_ArrayObjectOps;
}

JSClass js_ArrayClass = {
    "Array",
    JSCLASS_HAS_RESERVED_SLOTS(2) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array) |
    JSCLASS_NEW_ENUMERATE,
    JS_PropertyStub,    JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,   JS_ResolveStub,    js_TryValueOf,     array_finalize,
    array_getObjectOps, NULL,              NULL,              NULL,
    NULL,               NULL,              NULL,              NULL
};

JSClass js_SlowArrayClass = {
    "Array",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
    slowarray_addProperty, JS_PropertyStub, JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub,      JS_ResolveStub,  js_TryValueOf,    NULL,
    slowarray_getObjectOps, NULL,           NULL,             NULL,
    NULL,                  NULL,            NULL,             NULL
};

/*
 * Convert an array object from fast-and-dense to slow-and-flexible.
 */
JSBool
js_MakeArraySlow(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &js_ArrayClass);

    /*
     * Create a native scope. All slow arrays other than Array.prototype get
     * the same initial shape.
     */
    uint32 emptyShape;
    JSObject *arrayProto = obj->getProto();
    if (arrayProto->getClass() == &js_ObjectClass) {
        /* obj is Array.prototype. */
        emptyShape = js_GenerateShape(cx, false);
    } else {
        /* arrayProto is Array.prototype. */
        JS_ASSERT(arrayProto->getClass() == &js_SlowArrayClass);
        emptyShape = OBJ_SCOPE(arrayProto)->emptyScope->shape;
    }
    JSScope *scope = JSScope::create(cx, &js_SlowArrayObjectOps, &js_SlowArrayClass, obj,
                                     emptyShape);
    if (!scope)
        return JS_FALSE;

    uint32 capacity = js_DenseArrayCapacity(obj);
    if (capacity) {
        scope->freeslot = obj->numSlots() + JS_INITIAL_NSLOTS;
        obj->dslots[-1] = JS_INITIAL_NSLOTS + capacity;
    } else {
        scope->freeslot = obj->numSlots();
    }

    /* Create new properties pointing to existing values in dslots */
    for (uint32 i = 0; i < capacity; i++) {
        jsid id;
        JSScopeProperty *sprop;

        if (!JS_ValueToId(cx, INT_TO_JSVAL(i), &id))
            goto out_bad;

        if (obj->dslots[i] == JSVAL_HOLE) {
            obj->dslots[i] = JSVAL_VOID;
            continue;
        }

        sprop = scope->addDataProperty(cx, id, JS_INITIAL_NSLOTS + i,
                                       JSPROP_ENUMERATE);
        if (!sprop)
            goto out_bad;
    }

    /*
     * Render our formerly-reserved count property GC-safe.
     * We do not need to make the length slot GC-safe as this slot is private
     * where the implementation can store an arbitrary value.
     */
    {
        JS_STATIC_ASSERT(JSSLOT_ARRAY_LENGTH == JSSLOT_PRIVATE);
        JS_ASSERT(js_SlowArrayClass.flags & JSCLASS_HAS_PRIVATE);
        obj->fslots[JSSLOT_ARRAY_COUNT] = JSVAL_VOID;
    }

    /* Make sure we preserve any flags borrowing bits in classword. */
    obj->classword ^= (jsuword) &js_ArrayClass;
    obj->classword |= (jsuword) &js_SlowArrayClass;

    obj->map = scope;
    return JS_TRUE;

  out_bad:
    scope->destroy(cx);
    return JS_FALSE;
}

/* Transfer ownership of buffer to returned string. */
static inline JSBool
BufferToString(JSContext *cx, JSCharBuffer &cb, jsval *rval)
{
    JSString *str = js_NewStringFromCharBuffer(cx, cb);
    if (!str)
        return false;
    *rval = STRING_TO_JSVAL(str);
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
array_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj ||
        (OBJ_GET_CLASS(cx, obj) != &js_SlowArrayClass &&
         !JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))) {
        return false;
    }

    /* Find joins or cycles in the reachable object graph. */
    jschar *sharpchars;
    JSHashEntry *he = js_EnterSharpObject(cx, obj, NULL, &sharpchars);
    if (!he)
        return false;
    bool initiallySharp = IS_SHARP(he);

    /* After this point, all paths exit through the 'out' label. */
    MUST_FLOW_THROUGH("out");
    bool ok = false;

    /*
     * This object will take responsibility for the jschar buffer until the
     * buffer is transferred to the returned JSString.
     */
    JSCharBuffer cb(cx);

    /* Cycles/joins are indicated by sharp objects. */
#if JS_HAS_SHARP_VARS
    if (IS_SHARP(he)) {
        JS_ASSERT(sharpchars != 0);
        cb.replaceRawBuffer(sharpchars, js_strlen(sharpchars));
        goto make_string;
    } else if (sharpchars) {
        MAKE_SHARP(he);
        cb.replaceRawBuffer(sharpchars, js_strlen(sharpchars));
    }
#else
    if (IS_SHARP(he)) {
        if (!js_AppendLiteral(cb, "[]"))
            goto out;
        cx->free(sharpchars);
        goto make_string;
    }
#endif

    if (!cb.append('['))
        goto out;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        goto out;

    for (jsuint index = 0; index < length; index++) {
        /* Use vp to locally root each element value. */
        JSBool hole;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetArrayElement(cx, obj, index, &hole, vp)) {
            goto out;
        }

        /* Get element's character string. */
        JSString *str;
        if (hole) {
            str = cx->runtime->emptyString;
        } else {
            str = js_ValueToSource(cx, *vp);
            if (!str)
                goto out;
        }
        *vp = STRING_TO_JSVAL(str);
        const jschar *chars;
        size_t charlen;
        str->getCharsAndLength(chars, charlen);

        /* Append element to buffer. */
        if (!cb.append(chars, charlen))
            goto out;
        if (index + 1 != length) {
            if (!js_AppendLiteral(cb, ", "))
                goto out;
        } else if (hole) {
            if (!cb.append(','))
                goto out;
        }
    }

    /* Finalize the buffer. */
    if (!cb.append(']'))
        goto out;

  make_string:
    if (!BufferToString(cx, cb, vp))
        goto out;

    ok = true;

  out:
    if (!initiallySharp)
        js_LeaveSharpObject(cx, NULL);
    return ok;
}
#endif

static JSBool
array_toString_sub(JSContext *cx, JSObject *obj, JSBool locale,
                   JSString *sepstr, jsval *rval)
{
    JS_CHECK_RECURSION(cx, return false);

    /*
     * Use HashTable entry as the cycle indicator. On first visit, create the
     * entry, and, when leaving, remove the entry.
     */
    typedef js::HashSet<JSObject *> ObjSet;
    ObjSet::AddPtr hashp = cx->busyArrays.lookupForAdd(obj);
    uint32 genBefore;
    if (!hashp) {
        /* Not in hash table, so not a cycle. */
        if (!cx->busyArrays.add(hashp, obj)) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        genBefore = cx->busyArrays.generation();
    } else {
        /* Cycle, so return empty string. */
        *rval = ATOM_KEY(cx->runtime->atomState.emptyAtom);
        return true;
    }

    AutoValueRooter tvr(cx, obj);

    /* After this point, all paths exit through the 'out' label. */
    MUST_FLOW_THROUGH("out");
    bool ok = false;

    /* Get characters to use for the separator. */
    static const jschar comma = ',';
    const jschar *sep;
    size_t seplen;
    if (sepstr) {
        sepstr->getCharsAndLength(sep, seplen);
    } else {
        sep = &comma;
        seplen = 1;
    }

    /*
     * This object will take responsibility for the jschar buffer until the
     * buffer is transferred to the returned JSString.
     */
    JSCharBuffer cb(cx);

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        goto out;

    for (jsuint index = 0; index < length; index++) {
        /* Use rval to locally root each element value. */
        JSBool hole;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetArrayElement(cx, obj, index, &hole, rval)) {
            goto out;
        }

        /* Get element's character string. */
        if (!(hole || JSVAL_IS_VOID(*rval) || JSVAL_IS_NULL(*rval))) {
            if (locale) {
                /* Work on obj.toLocalString() instead. */
                JSObject *robj;

                if (!js_ValueToObject(cx, *rval, &robj))
                    goto out;
                *rval = OBJECT_TO_JSVAL(robj);
                JSAtom *atom = cx->runtime->atomState.toLocaleStringAtom;
                if (!js_TryMethod(cx, robj, atom, 0, NULL, rval))
                    goto out;
            }

            if (!js_ValueToCharBuffer(cx, *rval, cb))
                goto out;
        }

        /* Append the separator. */
        if (index + 1 != length) {
            if (!cb.append(sep, seplen))
                goto out;
        }
    }

    /* Finalize the buffer. */
    if (!BufferToString(cx, cb, rval))
        goto out;

    ok = true;

  out:
    if (genBefore == cx->busyArrays.generation())
        cx->busyArrays.remove(hashp);
    else
        cx->busyArrays.remove(obj);
    return ok;
}

static JSBool
array_toString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj ||
        (OBJ_GET_CLASS(cx, obj) != &js_SlowArrayClass &&
         !JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))) {
        return JS_FALSE;
    }

    return array_toString_sub(cx, obj, JS_FALSE, NULL, vp);
}

static JSBool
array_toLocaleString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj ||
        (OBJ_GET_CLASS(cx, obj) != &js_SlowArrayClass &&
         !JS_InstanceOf(cx, obj, &js_ArrayClass, vp + 2))) {
        return JS_FALSE;
    }

    /*
     *  Passing comma here as the separator. Need a way to get a
     *  locale-specific version.
     */
    return array_toString_sub(cx, obj, JS_TRUE, NULL, vp);
}

enum TargetElementsType {
    TargetElementsAllHoles,
    TargetElementsMayContainValues
};

enum SourceVectorType {
    SourceVectorAllValues,
    SourceVectorMayContainHoles
};

static JSBool
InitArrayElements(JSContext *cx, JSObject *obj, jsuint start, jsuint count, jsval *vector,
                  TargetElementsType targetType, SourceVectorType vectorType)
{
    JS_ASSERT(count < MAXINDEX);

    /*
     * Optimize for dense arrays so long as adding the given set of elements
     * wouldn't otherwise make the array slow.
     */
    if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
        start <= MAXINDEX - count && !INDEX_TOO_BIG(start + count)) {

#ifdef DEBUG_jwalden
        {
            /* Verify that overwriteType and writeType were accurate. */
            AutoIdRooter idr(cx);
            for (jsuint i = 0; i < count; i++) {
                JS_ASSERT_IF(vectorType == SourceVectorAllValues, vector[i] != JSVAL_HOLE);

                jsdouble index = jsdouble(start) + i;
                if (targetType == TargetElementsAllHoles && index < jsuint(-1)) {
                    JS_ASSERT(ReallyBigIndexToId(cx, index, idr.addr()));
                    JSObject* obj2;
                    JSProperty* prop;
                    JS_ASSERT(obj->lookupProperty(cx, idr.id(), &obj2, &prop));
                    JS_ASSERT(!prop);
                }
            }
        }
#endif

        jsuint newlen = start + count;
        JS_ASSERT(jsdouble(start) + count == jsdouble(newlen));
        if (!EnsureCapacity(cx, obj, newlen))
            return JS_FALSE;

        if (newlen > uint32(obj->fslots[JSSLOT_ARRAY_LENGTH]))
            obj->fslots[JSSLOT_ARRAY_LENGTH] = newlen;

        JS_ASSERT(count < size_t(-1) / sizeof(jsval));
        if (targetType == TargetElementsMayContainValues) {
            jsuint valueCount = 0;
            for (jsuint i = 0; i < count; i++) {
                 if (obj->dslots[start + i] != JSVAL_HOLE)
                     valueCount++;
            }
            JS_ASSERT(uint32(obj->fslots[JSSLOT_ARRAY_COUNT]) >= valueCount);
            obj->fslots[JSSLOT_ARRAY_COUNT] -= valueCount;
        }
        memcpy(obj->dslots + start, vector, sizeof(jsval) * count);
        if (vectorType == SourceVectorAllValues) {
            obj->fslots[JSSLOT_ARRAY_COUNT] += count;
        } else {
            jsuint valueCount = 0;
            for (jsuint i = 0; i < count; i++) {
                 if (obj->dslots[start + i] != JSVAL_HOLE)
                     valueCount++;
            }
            obj->fslots[JSSLOT_ARRAY_COUNT] += valueCount;
        }
        JS_ASSERT_IF(count != 0, obj->dslots[newlen - 1] != JSVAL_HOLE);
        return JS_TRUE;
    }

    jsval* end = vector + count;
    while (vector != end && start < MAXINDEX) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !SetArrayElement(cx, obj, start++, *vector++)) {
            return JS_FALSE;
        }
    }

    if (vector == end)
        return JS_TRUE;

    /* Finish out any remaining elements past the max array index. */
    if (obj->isDenseArray() && !ENSURE_SLOW_ARRAY(cx, obj))
        return JS_FALSE;

    JS_ASSERT(start == MAXINDEX);
    jsval tmp[2] = {JSVAL_NULL, JSVAL_NULL};
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(tmp), tmp);
    if (!js_NewDoubleInRootedValue(cx, MAXINDEX, &tmp[0]))
        return JS_FALSE;
    jsdouble *dp = JSVAL_TO_DOUBLE(tmp[0]);
    JS_ASSERT(*dp == MAXINDEX);
    AutoIdRooter idr(cx);
    do {
        tmp[1] = *vector++;
        if (!js_ValueToStringId(cx, tmp[0], idr.addr()) ||
            !obj->setProperty(cx, idr.id(), &tmp[1])) {
            return JS_FALSE;
        }
        *dp += 1;
    } while (vector != end);

    return JS_TRUE;
}

static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, jsuint length, const jsval *vector,
                bool holey = false)
{
    JS_ASSERT(obj->isArray());

    obj->fslots[JSSLOT_ARRAY_LENGTH] = length;

    if (vector) {
        if (!EnsureCapacity(cx, obj, length))
            return JS_FALSE;

        jsuint count = length;
        if (!holey) {
            memcpy(obj->dslots, vector, length * sizeof (jsval));
        } else {
            for (jsuint i = 0; i < length; i++) {
                if (vector[i] == JSVAL_HOLE)
                    --count;
                obj->dslots[i] = vector[i];
            }
        }
        obj->fslots[JSSLOT_ARRAY_COUNT] = count;
    } else {
        obj->fslots[JSSLOT_ARRAY_COUNT] = 0;
    }
    return JS_TRUE;
}

#ifdef JS_TRACER
static JSString* FASTCALL
Array_p_join(JSContext* cx, JSObject* obj, JSString *str)
{
    AutoValueRooter tvr(cx);
    if (!array_toString_sub(cx, obj, JS_FALSE, str, tvr.addr())) {
        SetBuiltinError(cx);
        return NULL;
    }
    return JSVAL_TO_STRING(tvr.value());
}

static JSString* FASTCALL
Array_p_toString(JSContext* cx, JSObject* obj)
{
    AutoValueRooter tvr(cx);
    if (!array_toString_sub(cx, obj, JS_FALSE, NULL, tvr.addr())) {
        SetBuiltinError(cx);
        return NULL;
    }
    return JSVAL_TO_STRING(tvr.value());
}
#endif

/*
 * Perl-inspired join, reverse, and sort.
 */
static JSBool
array_join(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    JSObject *obj;

    if (argc == 0 || JSVAL_IS_VOID(vp[2])) {
        str = NULL;
    } else {
        str = js_ValueToString(cx, vp[2]);
        if (!str)
            return JS_FALSE;
        vp[2] = STRING_TO_JSVAL(str);
    }
    obj = JS_THIS_OBJECT(cx, vp);
    return obj && array_toString_sub(cx, obj, JS_FALSE, str, vp);
}

static JSBool
array_reverse(JSContext *cx, uintN argc, jsval *vp)
{
    jsuint len;
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &len))
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(obj);

    if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj)) {
        /* An empty array or an array with no elements is already reversed. */
        if (len == 0 || !obj->dslots)
            return JS_TRUE;

        /*
         * It's actually surprisingly complicated to reverse an array due to the
         * orthogonality of array length and array capacity while handling
         * leading and trailing holes correctly.  Reversing seems less likely to
         * be a common operation than other array mass-mutation methods, so for
         * now just take a probably-small memory hit (in the absence of too many
         * holes in the array at its start) and ensure that the capacity is
         * sufficient to hold all the elements in the array if it were full.
         */
        if (!EnsureCapacity(cx, obj, len))
            return JS_FALSE;

        jsval* lo = &obj->dslots[0];
        jsval* hi = &obj->dslots[len - 1];
        for (; lo < hi; lo++, hi--) {
             jsval tmp = *lo;
             *lo = *hi;
             *hi = tmp;
        }

        /*
         * Per ECMA-262, don't update the length of the array, even if the new
         * array has trailing holes (and thus the original array began with
         * holes).
         */
        return JS_TRUE;
    }

    AutoValueRooter tvr(cx);
    for (jsuint i = 0, half = len / 2; i < half; i++) {
        JSBool hole, hole2;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetArrayElement(cx, obj, i, &hole, tvr.addr()) ||
            !GetArrayElement(cx, obj, len - i - 1, &hole2, vp) ||
            !SetOrDeleteArrayElement(cx, obj, len - i - 1, hole, tvr.value()) ||
            !SetOrDeleteArrayElement(cx, obj, i, hole2, *vp)) {
            return false;
        }
    }
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
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

static JS_REQUIRES_STACK JSBool
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
    JS_ASSERT(!JSVAL_IS_VOID(av));
    JS_ASSERT(!JSVAL_IS_VOID(bv));

    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return JS_FALSE;

    invokevp = ca->elemroot;
    sp = invokevp;
    *sp++ = ca->fval;
    *sp++ = JSVAL_NULL;
    *sp++ = av;
    *sp++ = bv;

    if (!js_Invoke(cx, 2, invokevp, 0))
        return JS_FALSE;

    cmp = js_ValueToNumber(cx, invokevp);
    if (JSVAL_IS_NULL(*invokevp))
        return JS_FALSE;

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

typedef JSBool (JS_REQUIRES_STACK *JSRedComparator)(void*, const void*,
                                                    const void*, int *);

static inline JS_IGNORE_STACK JSComparator
comparator_stack_cast(JSRedComparator func)
{
    return func;
}

static int
sort_compare_strings(void *arg, const void *a, const void *b, int *result)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;

    JS_ASSERT(JSVAL_IS_STRING(av));
    JS_ASSERT(JSVAL_IS_STRING(bv));
    if (!JS_CHECK_OPERATION_LIMIT((JSContext *)arg))
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
    jsval fval;
    jsuint len, newlen, i, undefs;
    size_t elemsize;
    JSString *str;

    jsval *argv = JS_ARGV(cx, vp);
    if (argc > 0) {
        if (JSVAL_IS_PRIMITIVE(argv[0])) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_SORT_ARG);
            return false;
        }
        fval = argv[0];     /* non-default compare function */
    } else {
        fval = JSVAL_NULL;
    }

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &len))
        return false;
    if (len == 0) {
        *vp = OBJECT_TO_JSVAL(obj);
        return true;
    }

    /*
     * We need a temporary array of 2 * len jsvals to hold the array elements
     * and the scratch space for merge sort. Check that its size does not
     * overflow size_t, which would allow for indexing beyond the end of the
     * malloc'd vector.
     */
#if JS_BITS_PER_WORD == 32
    if (size_t(len) > size_t(-1) / (2 * sizeof(jsval))) {
        js_ReportAllocationOverflow(cx);
        return false;
    }
#endif

    /*
     * Initialize vec as a root. We will clear elements of vec one by
     * one while increasing the rooted amount of vec when we know that the
     * property at the corresponding index exists and its value must be rooted.
     *
     * In this way when sorting a huge mostly sparse array we will not
     * access the tail of vec corresponding to properties that do not
     * exist, allowing OS to avoiding committing RAM. See bug 330812.
     *
     * After this point control must flow through label out: to exit.
     */
    {
        jsval *vec = (jsval *) cx->malloc(2 * size_t(len) * sizeof(jsval));
        if (!vec)
            return false;

        struct AutoFreeVector {
            AutoFreeVector(JSContext *cx, jsval *&vec) : cx(cx), vec(vec) { }
            ~AutoFreeVector() {
                cx->free(vec);
            }
            JSContext * const cx;
            jsval *&vec;
        } free(cx, vec);

        AutoArrayRooter tvr(cx, 0, vec);

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
        bool allStrings = true;
        for (i = 0; i < len; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            /* Clear vec[newlen] before including it in the rooted set. */
            JSBool hole;
            vec[newlen] = JSVAL_NULL;
            tvr.changeLength(newlen + 1);
            if (!GetArrayElement(cx, obj, i, &hole, &vec[newlen]))
                return false;

            if (hole)
                continue;

            if (JSVAL_IS_VOID(vec[newlen])) {
                ++undefs;
                continue;
            }

            allStrings = allStrings && JSVAL_IS_STRING(vec[newlen]);

            ++newlen;
        }

        if (newlen == 0)
            return true; /* The array has only holes and undefs. */

        /*
         * The first newlen elements of vec are copied from the array object
         * (above). The remaining newlen positions are used as GC-rooted scratch
         * space for mergesort. We must clear the space before including it to
         * the root set covered by tvr.count. We assume JSVAL_NULL==0 to optimize
         * initialization using memset.
         */
        jsval *mergesort_tmp = vec + newlen;
        PodZero(mergesort_tmp, newlen);
        tvr.changeLength(newlen * 2);

        /* Here len == 2 * (newlen + undefs + number_of_holes). */
        if (fval == JSVAL_NULL) {
            /*
             * Sort using the default comparator converting all elements to
             * strings.
             */
            if (allStrings) {
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
#if JS_BITS_PER_WORD == 32
                if (size_t(newlen) > size_t(-1) / (4 * sizeof(jsval))) {
                    js_ReportAllocationOverflow(cx);
                    return false;
                }
#endif

                /*
                 * Rearrange and string-convert the elements of the vector from
                 * the tail here and, after sorting, move the results back
                 * starting from the start to prevent overwrite the existing
                 * elements.
                 */
                i = newlen;
                do {
                    --i;
                    if (!JS_CHECK_OPERATION_LIMIT(cx))
                        return false;
                    jsval v = vec[i];
                    str = js_ValueToString(cx, v);
                    if (!str)
                        return false;
                    vec[2 * i] = STRING_TO_JSVAL(str);
                    vec[2 * i + 1] = v;
                } while (i != 0);

                JS_ASSERT(tvr.array == vec);
                vec = (jsval *) cx->realloc(vec, 4 * size_t(newlen) * sizeof(jsval));
                if (!vec) {
                    vec = tvr.array;
                    return false;
                }
                mergesort_tmp = vec + 2 * newlen;
                PodZero(mergesort_tmp, newlen * 2);
                tvr.changeArray(vec, newlen * 4);
                elemsize = 2 * sizeof(jsval);
            }
            if (!js_MergeSort(vec, size_t(newlen), elemsize,
                              sort_compare_strings, cx, mergesort_tmp)) {
                return false;
            }
            if (!allStrings) {
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

            LeaveTrace(cx);

            CompareArgs ca;
            ca.context = cx;
            ca.fval = fval;
            ca.elemroot  = js_AllocStack(cx, 2 + 2, &mark);
            if (!ca.elemroot)
                return false;
            bool ok = !!js_MergeSort(vec, size_t(newlen), sizeof(jsval),
                                     comparator_stack_cast(sort_compare),
                                     &ca, mergesort_tmp);
            js_FreeStack(cx, mark);
            if (!ok)
                return false;
        }

        /*
         * We no longer need to root the scratch space for the merge sort, so
         * unroot it now to make the job of a potential GC under
         * InitArrayElements easier.
         */
        tvr.changeLength(newlen);
        if (!InitArrayElements(cx, obj, 0, newlen, vec, TargetElementsMayContainValues,
                               SourceVectorAllValues)) {
            return false;
        }
    }

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        if (!JS_CHECK_OPERATION_LIMIT(cx) || !SetArrayElement(cx, obj, newlen++, JSVAL_VOID))
            return false;
    }

    /* Re-create any holes that sorted to the end of the array. */
    while (len > newlen) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) || !DeleteArrayElement(cx, obj, --len))
            return JS_FALSE;
    }
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
}

/*
 * Perl-inspired push, pop, shift, unshift, and splice methods.
 */
static JSBool
array_push_slowly(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (!InitArrayElements(cx, obj, length, argc, argv, TargetElementsMayContainValues,
                           SourceVectorAllValues)) {
        return JS_FALSE;
    }

    /* Per ECMA-262, return the new array length. */
    jsdouble newlength = length + jsdouble(argc);
    if (!IndexToValue(cx, newlength, rval))
        return JS_FALSE;
    return js_SetLengthProperty(cx, obj, newlength);
}

static JSBool
array_push1_dense(JSContext* cx, JSObject* obj, jsval v, jsval *rval)
{
    uint32 length = obj->fslots[JSSLOT_ARRAY_LENGTH];
    if (INDEX_TOO_SPARSE(obj, length)) {
        if (!js_MakeArraySlow(cx, obj))
            return JS_FALSE;
        return array_push_slowly(cx, obj, 1, &v, rval);
    }

    if (!EnsureCapacity(cx, obj, length + 1))
        return JS_FALSE;
    obj->fslots[JSSLOT_ARRAY_LENGTH] = length + 1;

    JS_ASSERT(obj->dslots[length] == JSVAL_HOLE);
    obj->fslots[JSSLOT_ARRAY_COUNT]++;
    obj->dslots[length] = v;
    return IndexToValue(cx, obj->fslots[JSSLOT_ARRAY_LENGTH], rval);
}

JSBool JS_FASTCALL
js_ArrayCompPush(JSContext *cx, JSObject *obj, jsval v)
{
    JS_ASSERT(obj->isDenseArray());
    uint32_t length = (uint32_t) obj->fslots[JSSLOT_ARRAY_LENGTH];
    JS_ASSERT(length <= js_DenseArrayCapacity(obj));

    if (length == js_DenseArrayCapacity(obj)) {
        if (length > JS_ARGS_LENGTH_MAX) {
            JS_ReportErrorNumberUC(cx, js_GetErrorMessage, NULL,
                                   JSMSG_ARRAY_INIT_TOO_BIG);
            return JS_FALSE;
        }

        if (!EnsureCapacity(cx, obj, length + 1))
            return JS_FALSE;
    }
    obj->fslots[JSSLOT_ARRAY_LENGTH] = length + 1;
    obj->fslots[JSSLOT_ARRAY_COUNT]++;
    obj->dslots[length] = v;
    return JS_TRUE;
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_ArrayCompPush, CONTEXT, OBJECT, JSVAL, 0,
                     nanojit::ACC_STORE_ANY)

#ifdef JS_TRACER
static jsval FASTCALL
Array_p_push1(JSContext* cx, JSObject* obj, jsval v)
{
    AutoValueRooter tvr(cx, v);
    if (obj->isDenseArray()
        ? array_push1_dense(cx, obj, v, tvr.addr())
        : array_push_slowly(cx, obj, 1, tvr.addr(), tvr.addr())) {
        return tvr.value();
    }
    SetBuiltinError(cx);
    return JSVAL_VOID;
}
#endif

static JSBool
array_push(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    /* Insist on one argument and obj of the expected class. */
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    if (argc != 1 || !obj->isDenseArray())
        return array_push_slowly(cx, obj, argc, vp + 2, vp);

    return array_push1_dense(cx, obj, vp[2], vp);
}

static JSBool
array_pop_slowly(JSContext *cx, JSObject* obj, jsval *vp)
{
    jsuint index;
    JSBool hole;

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
array_pop_dense(JSContext *cx, JSObject* obj, jsval *vp)
{
    jsuint index;
    JSBool hole;

    index = obj->fslots[JSSLOT_ARRAY_LENGTH];
    if (index == 0) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }
    index--;
    if (!GetArrayElement(cx, obj, index, &hole, vp))
        return JS_FALSE;
    if (!hole && !DeleteArrayElement(cx, obj, index))
        return JS_FALSE;
    obj->fslots[JSSLOT_ARRAY_LENGTH] = index;
    return JS_TRUE;
}

#ifdef JS_TRACER
static jsval FASTCALL
Array_p_pop(JSContext* cx, JSObject* obj)
{
    AutoValueRooter tvr(cx);
    if (obj->isDenseArray()
        ? array_pop_dense(cx, obj, tvr.addr())
        : array_pop_slowly(cx, obj, tvr.addr())) {
        return tvr.value();
    }
    SetBuiltinError(cx);
    return JSVAL_VOID;
}
#endif

static JSBool
array_pop(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    if (obj->isDenseArray())
        return array_pop_dense(cx, obj, vp);
    return array_pop_slowly(cx, obj, vp);
}

static JSBool
array_shift(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint length, i;
    JSBool hole;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0) {
        *vp = JSVAL_VOID;
    } else {
        length--;

        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            length < js_DenseArrayCapacity(obj)) {
            if (JS_LIKELY(obj->dslots != NULL)) {
                *vp = obj->dslots[0];
                if (*vp == JSVAL_HOLE)
                    *vp = JSVAL_VOID;
                else
                    obj->fslots[JSSLOT_ARRAY_COUNT]--;
                memmove(obj->dslots, obj->dslots + 1, length * sizeof(jsval));
                obj->dslots[length] = JSVAL_HOLE;
            } else {
                /*
                 * We don't need to modify the indexed properties of an empty array
                 * with an explicitly set non-zero length when shift() is called on
                 * it, but note fallthrough to reduce the length by one.
                 */
                JS_ASSERT(obj->fslots[JSSLOT_ARRAY_COUNT] == 0);
                *vp = JSVAL_VOID;
            }
        } else {
            /* Get the to-be-deleted property's value into vp ASAP. */
            if (!GetArrayElement(cx, obj, 0, &hole, vp))
                return JS_FALSE;

            /* Slide down the array above the first element. */
            AutoValueRooter tvr(cx);
            for (i = 0; i != length; i++) {
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetArrayElement(cx, obj, i + 1, &hole, tvr.addr()) ||
                    !SetOrDeleteArrayElement(cx, obj, i, hole, tvr.value())) {
                    return JS_FALSE;
                }
            }

            /* Delete the only or last element when it exists. */
            if (!hole && !DeleteArrayElement(cx, obj, length))
                return JS_FALSE;
        }
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsval *argv;
    jsuint length;
    JSBool hole;
    jsdouble last, newlen;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    newlen = length;
    if (argc > 0) {
        /* Slide up the array to make room for argc at the bottom. */
        argv = JS_ARGV(cx, vp);
        if (length > 0) {
            if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
                !INDEX_TOO_SPARSE(obj, unsigned(newlen + argc))) {
                JS_ASSERT(newlen + argc == length + argc);
                if (!EnsureCapacity(cx, obj, length + argc))
                    return JS_FALSE;
                memmove(obj->dslots + argc, obj->dslots, length * sizeof(jsval));
                for (uint32 i = 0; i < argc; i++)
                    obj->dslots[i] = JSVAL_HOLE;
            } else {
                last = length;
                jsdouble upperIndex = last + argc;
                AutoValueRooter tvr(cx);
                do {
                    --last, --upperIndex;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                        !GetArrayElement(cx, obj, last, &hole, tvr.addr()) ||
                        !SetOrDeleteArrayElement(cx, obj, upperIndex, hole, tvr.value())) {
                        return JS_FALSE;
                    }
                } while (last != 0);
            }
        }

        /* Copy from argv to the bottom of the array. */
        if (!InitArrayElements(cx, obj, 0, argc, argv, TargetElementsAllHoles, SourceVectorAllValues))
            return JS_FALSE;

        newlen += argc;
        if (!js_SetLengthProperty(cx, obj, newlen))
            return JS_FALSE;
    }

    /* Follow Perl by returning the new array length. */
    return IndexToValue(cx, newlen, vp);
}

static JSBool
array_splice(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    JSObject *obj;
    jsuint length, begin, end, count, delta, last;
    jsdouble d;
    JSBool hole;
    JSObject *obj2;

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

    /* Nothing to do if no args.  Otherwise get length. */
    if (argc == 0)
        return JS_TRUE;
    argv = JS_ARGV(cx, vp);
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /* Convert the first argument into a starting index. */
    d = js_ValueToNumber(cx, argv);
    if (JSVAL_IS_NULL(*argv))
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
        d = js_ValueToNumber(cx, argv);
        if (JSVAL_IS_NULL(*argv))
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

    AutoValueRooter tvr(cx, JSVAL_NULL);

    /* If there are elements to remove, put them into the return value. */
    if (count > 0) {
        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            !js_PrototypeHasIndexedProperties(cx, obj2) &&
            end <= js_DenseArrayCapacity(obj)) {
            if (!InitArrayObject(cx, obj2, count, obj->dslots + begin,
                                 obj->fslots[JSSLOT_ARRAY_COUNT] !=
                                 obj->fslots[JSSLOT_ARRAY_LENGTH])) {
                return JS_FALSE;
            }
        } else {
            for (last = begin; last < end; last++) {
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetArrayElement(cx, obj, last, &hole, tvr.addr())) {
                    return JS_FALSE;
                }

                /* Copy tvr.value() to the new array unless it's a hole. */
                if (!hole && !SetArrayElement(cx, obj2, last - begin, tvr.value()))
                    return JS_FALSE;
            }

            if (!js_SetLengthProperty(cx, obj2, count))
                return JS_FALSE;
        }
    }

    /* Find the direction (up or down) to copy and make way for argv. */
    if (argc > count) {
        delta = (jsuint)argc - count;
        last = length;
        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            length <= js_DenseArrayCapacity(obj) &&
            (length == 0 || obj->dslots[length - 1] != JSVAL_HOLE)) {
            if (!EnsureCapacity(cx, obj, length + delta))
                return JS_FALSE;
            /* (uint) end could be 0, so we can't use a vanilla >= test. */
            while (last-- > end) {
                jsval srcval = obj->dslots[last];
                jsval* dest = &obj->dslots[last + delta];
                if (*dest == JSVAL_HOLE && srcval != JSVAL_HOLE)
                    obj->fslots[JSSLOT_ARRAY_COUNT]++;
                *dest = srcval;
            }
            obj->fslots[JSSLOT_ARRAY_LENGTH] += delta;
        } else {
            /* (uint) end could be 0, so we can't use a vanilla >= test. */
            while (last-- > end) {
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetArrayElement(cx, obj, last, &hole, tvr.addr()) ||
                    !SetOrDeleteArrayElement(cx, obj, last + delta, hole, tvr.value())) {
                    return JS_FALSE;
                }
            }
        }
        length += delta;
    } else if (argc < count) {
        delta = count - (jsuint)argc;
        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            length <= js_DenseArrayCapacity(obj)) {
            /* (uint) end could be 0, so we can't use a vanilla >= test. */
            for (last = end; last < length; last++) {
                jsval srcval = obj->dslots[last];
                jsval* dest = &obj->dslots[last - delta];
                if (*dest == JSVAL_HOLE && srcval != JSVAL_HOLE)
                    obj->fslots[JSSLOT_ARRAY_COUNT]++;
                *dest = srcval;
            }
        } else {
            for (last = end; last < length; last++) {
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetArrayElement(cx, obj, last, &hole, tvr.addr()) ||
                    !SetOrDeleteArrayElement(cx, obj, last - delta, hole, tvr.value())) {
                    return JS_FALSE;
                }
            }
        }
        length -= delta;
    }

    /*
     * Copy from argv into the hole to complete the splice, and update length in
     * case we deleted elements from the end.
     */
    return InitArrayElements(cx, obj, begin, argc, argv, TargetElementsMayContainValues,
                             SourceVectorAllValues) &&
           js_SetLengthProperty(cx, obj, length);
}

/*
 * Python-esque sequence operations.
 */
static JSBool
array_concat(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv, v;
    JSObject *aobj, *nobj;
    jsuint length, alength, slot;
    uintN i;
    JSBool hole;

    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    argv = JS_ARGV(cx, vp) - 1;
    JS_ASSERT(JS_THIS_OBJECT(cx, vp) == JSVAL_TO_OBJECT(argv[0]));

    /* Create a new Array object and root it using *vp. */
    aobj = JS_THIS_OBJECT(cx, vp);
    if (aobj->isDenseArray()) {
        /*
         * Clone aobj but pass the minimum of its length and capacity, to
         * handle a = [1,2,3]; a.length = 10000 "dense" cases efficiently. In
         * such a case we'll pass 8 (not 3) due to ARRAY_CAPACITY_MIN, which
         * will cause nobj to be over-allocated to 16. But in the normal case
         * where length is <= capacity, nobj and aobj will have the same
         * capacity.
         */
        length = aobj->fslots[JSSLOT_ARRAY_LENGTH];
        jsuint capacity = js_DenseArrayCapacity(aobj);
        nobj = js_NewArrayObject(cx, JS_MIN(length, capacity), aobj->dslots,
                                 aobj->fslots[JSSLOT_ARRAY_COUNT] !=
                                 (jsval) length);
        if (!nobj)
            return JS_FALSE;
        nobj->fslots[JSSLOT_ARRAY_LENGTH] = length;
        *vp = OBJECT_TO_JSVAL(nobj);
        if (argc == 0)
            return JS_TRUE;
        argc--;
        argv++;
    } else {
        nobj = js_NewArrayObject(cx, 0, NULL);
        if (!nobj)
            return JS_FALSE;
        *vp = OBJECT_TO_JSVAL(nobj);
        length = 0;
    }

    AutoValueRooter tvr(cx, JSVAL_NULL);

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    for (i = 0; i <= argc; i++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        v = argv[i];
        if (!JSVAL_IS_PRIMITIVE(v)) {
            JSObject *wobj;

            aobj = JSVAL_TO_OBJECT(v);
            wobj = js_GetWrappedObject(cx, aobj);
            if (wobj->isArray()) {
                jsid id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
                if (!aobj->getProperty(cx, id, tvr.addr()))
                    return false;
                alength = ValueIsLength(cx, tvr.addr());
                if (JSVAL_IS_NULL(tvr.value()))
                    return false;
                for (slot = 0; slot < alength; slot++) {
                    if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                        !GetArrayElement(cx, aobj, slot, &hole, tvr.addr())) {
                        return false;
                    }

                    /*
                     * Per ECMA 262, 15.4.4.4, step 9, ignore non-existent
                     * properties.
                     */
                    if (!hole &&
                        !SetArrayElement(cx, nobj, length+slot, tvr.value())) {
                        return false;
                    }
                }
                length += alength;
                continue;
            }
        }

        if (!SetArrayElement(cx, nobj, length, v))
            return false;
        length++;
    }

    return js_SetLengthProperty(cx, nobj, length);
}

static JSBool
array_slice(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv;
    JSObject *nobj, *obj;
    jsuint length, begin, end, slot;
    jsdouble d;
    JSBool hole;

    argv = JS_ARGV(cx, vp);

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    begin = 0;
    end = length;

    if (argc > 0) {
        d = js_ValueToNumber(cx, &argv[0]);
        if (JSVAL_IS_NULL(argv[0]))
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
            d = js_ValueToNumber(cx, &argv[1]);
            if (JSVAL_IS_NULL(argv[1]))
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

    if (obj->isDenseArray() && end <= js_DenseArrayCapacity(obj) &&
        !js_PrototypeHasIndexedProperties(cx, obj)) {
        nobj = js_NewArrayObject(cx, end - begin, obj->dslots + begin,
                                 obj->fslots[JSSLOT_ARRAY_COUNT] !=
                                 obj->fslots[JSSLOT_ARRAY_LENGTH]);
        if (!nobj)
            return JS_FALSE;
        *vp = OBJECT_TO_JSVAL(nobj);
        return JS_TRUE;
    }

    /* Create a new Array object and root it using *vp. */
    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(nobj);

    AutoValueRooter tvr(cx);
    for (slot = begin; slot < end; slot++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetArrayElement(cx, obj, slot, &hole, tvr.addr())) {
            return JS_FALSE;
        }
        if (!hole && !SetArrayElement(cx, nobj, slot - begin, tvr.value()))
            return JS_FALSE;
    }

    return js_SetLengthProperty(cx, nobj, end - begin);
}

#if JS_HAS_ARRAY_EXTRAS

static JSBool
array_indexOfHelper(JSContext *cx, JSBool isLast, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsuint length, i, stop;
    jsval tosearch;
    jsint direction;
    JSBool hole;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0)
        goto not_found;

    if (argc <= 1) {
        i = isLast ? length - 1 : 0;
        tosearch = (argc != 0) ? vp[2] : JSVAL_VOID;
    } else {
        jsdouble start;

        tosearch = vp[2];
        start = js_ValueToNumber(cx, &vp[3]);
        if (JSVAL_IS_NULL(vp[3]))
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
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetArrayElement(cx, obj, (jsuint)i, &hole, vp)) {
            return JS_FALSE;
        }
        if (!hole && js_StrictlyEqual(cx, *vp, tosearch))
            return js_NewNumberInRootedValue(cx, i, vp);
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

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /*
     * First, get or compute our callee, so that we error out consistently
     * when passed a non-callable object.
     */
    if (argc == 0) {
        js_ReportMissingArg(cx, vp, 0);
        return JS_FALSE;
    }
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
    LeaveTrace(cx);
    argc = 3 + REDUCE_MODE(mode);
    elemroot = js_AllocStack(cx, 1 + 2 + argc, &mark);
    if (!elemroot)
        return JS_FALSE;

    MUST_FLOW_THROUGH("out");
    ok = JS_TRUE;
    invokevp = elemroot + 1;

    for (i = start; i != end; i += step) {
        ok = JS_CHECK_OPERATION_LIMIT(cx) &&
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
        ok = js_Invoke(cx, argc, invokevp, 0);
        if (!ok)
            break;

        if (mode > MAP)
            cond = js_ValueToBoolean(*invokevp);
#ifdef __GNUC__ /* quell GCC overwarning */
        else
            cond = JS_FALSE;
#endif

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

static JSBool
array_isArray(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = BOOLEAN_TO_JSVAL(argc > 0 &&
                           !JSVAL_IS_PRIMITIVE(vp[2]) &&
                           js_GetWrappedObject(cx, JSVAL_TO_OBJECT(vp[2]))->isArray());
    return JS_TRUE;
}

static JSPropertySpec array_props[] = {
    {js_length_str,   -1,   JSPROP_SHARED | JSPROP_PERMANENT,
                            array_length_getter,    array_length_setter},
    {0,0,0,0,0}
};

JS_DEFINE_TRCINFO_1(array_toString,
    (2, (static, STRING_FAIL, Array_p_toString, CONTEXT, THIS,      0, nanojit::ACC_STORE_ANY)))
JS_DEFINE_TRCINFO_1(array_join,
    (3, (static, STRING_FAIL, Array_p_join, CONTEXT, THIS, STRING,  0, nanojit::ACC_STORE_ANY)))
JS_DEFINE_TRCINFO_1(array_push,
    (3, (static, JSVAL_FAIL, Array_p_push1, CONTEXT, THIS, JSVAL,   0, nanojit::ACC_STORE_ANY)))
JS_DEFINE_TRCINFO_1(array_pop,
    (2, (static, JSVAL_FAIL, Array_p_pop, CONTEXT, THIS,            0, nanojit::ACC_STORE_ANY)))

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,      array_toSource,     0,0),
#endif
    JS_TN(js_toString_str,      array_toString,     0,0, &array_toString_trcinfo),
    JS_FN(js_toLocaleString_str,array_toLocaleString,0,0),

    /* Perl-ish methods. */
    JS_TN("join",               array_join,         1,JSFUN_GENERIC_NATIVE, &array_join_trcinfo),
    JS_FN("reverse",            array_reverse,      0,JSFUN_GENERIC_NATIVE),
    JS_FN("sort",               array_sort,         1,JSFUN_GENERIC_NATIVE),
    JS_TN("push",               array_push,         1,JSFUN_GENERIC_NATIVE, &array_push_trcinfo),
    JS_TN("pop",                array_pop,          0,JSFUN_GENERIC_NATIVE, &array_pop_trcinfo),
    JS_FN("shift",              array_shift,        0,JSFUN_GENERIC_NATIVE),
    JS_FN("unshift",            array_unshift,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("splice",             array_splice,       2,JSFUN_GENERIC_NATIVE),

    /* Pythonic sequence methods. */
    JS_FN("concat",             array_concat,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("slice",              array_slice,        2,JSFUN_GENERIC_NATIVE),

#if JS_HAS_ARRAY_EXTRAS
    JS_FN("indexOf",            array_indexOf,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("lastIndexOf",        array_lastIndexOf,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("forEach",            array_forEach,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("map",                array_map,          1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduce",             array_reduce,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduceRight",        array_reduceRight,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("filter",             array_filter,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("some",               array_some,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("every",              array_every,        1,JSFUN_GENERIC_NATIVE),
#endif

    JS_FS_END
};

static JSFunctionSpec array_static_methods[] = {
    JS_FN("isArray",            array_isArray,      1,0),
    JS_FS_END
};

JSBool
js_Array(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length;
    const jsval *vector;

    /* If called without new, replace obj with a new Array object. */
    if (!JS_IsConstructing(cx)) {
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
        length = ValueIsLength(cx, &argv[0]);
        if (JSVAL_IS_NULL(argv[0]))
            return JS_FALSE;
        vector = NULL;
    }
    return InitArrayObject(cx, obj, length, vector);
}

JS_STATIC_ASSERT(JSSLOT_PRIVATE == JSSLOT_ARRAY_LENGTH);
JS_STATIC_ASSERT(JSSLOT_ARRAY_LENGTH + 1 == JSSLOT_ARRAY_COUNT);

JSObject* JS_FASTCALL
js_NewEmptyArray(JSContext* cx, JSObject* proto)
{
    JS_ASSERT(proto->isArray());

    JSObject* obj = js_NewGCObject(cx);
    if (!obj)
        return NULL;

    /* Initialize all fields of JSObject. */
    obj->map = const_cast<JSObjectMap *>(&SharedArrayMap);
    obj->classword = jsuword(&js_ArrayClass);
    obj->setProto(proto);
    obj->setParent(proto->getParent());

    obj->fslots[JSSLOT_ARRAY_LENGTH] = 0;
    obj->fslots[JSSLOT_ARRAY_COUNT] = 0;
    for (unsigned i = JSSLOT_ARRAY_COUNT + 1; i != JS_INITIAL_NSLOTS; ++i)
        obj->fslots[i] = JSVAL_VOID;
    obj->dslots = NULL;
    return obj;
}
#ifdef JS_TRACER
JS_DEFINE_CALLINFO_2(extern, OBJECT, js_NewEmptyArray, CONTEXT, OBJECT, 0, nanojit::ACC_STORE_ANY)
#endif

JSObject* JS_FASTCALL
js_NewEmptyArrayWithLength(JSContext* cx, JSObject* proto, int32 len)
{
    if (len < 0)
        return NULL;
    JSObject *obj = js_NewEmptyArray(cx, proto);
    if (!obj)
        return NULL;
    obj->fslots[JSSLOT_ARRAY_LENGTH] = len;
    return obj;
}
#ifdef JS_TRACER
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_NewEmptyArrayWithLength, CONTEXT, OBJECT, INT32, 0,
                     nanojit::ACC_STORE_ANY)
#endif

JSObject* JS_FASTCALL
js_NewArrayWithSlots(JSContext* cx, JSObject* proto, uint32 len)
{
    JSObject* obj = js_NewEmptyArray(cx, proto);
    if (!obj)
        return NULL;
    obj->fslots[JSSLOT_ARRAY_LENGTH] = len;
    if (!ResizeSlots(cx, obj, 0, JS_MAX(len, ARRAY_CAPACITY_MIN)))
        return NULL;
    return obj;
}
#ifdef JS_TRACER
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_NewArrayWithSlots, CONTEXT, OBJECT, UINT32, 0,
                     nanojit::ACC_STORE_ANY)
#endif

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = JS_InitClass(cx, obj, NULL, &js_ArrayClass, js_Array, 1,
                                   array_props, array_methods, NULL, array_static_methods);

    /* Initialize the Array prototype object so it gets a length property. */
    if (!proto || !InitArrayObject(cx, proto, 0, NULL))
        return NULL;
    return proto;
}

JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, const jsval *vector, bool holey)
{
    JSObject *obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
    if (!obj)
        return NULL;

    /*
     * If this fails, the global object was not initialized and its class does
     * not have JSCLASS_IS_GLOBAL.
     */
    JS_ASSERT(obj->getProto());

    {
        AutoValueRooter tvr(cx, obj);
        if (!InitArrayObject(cx, obj, length, vector, holey))
            obj = NULL;
    }

    /* Set/clear newborn root, in case we lost it.  */
    cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = obj;
    return obj;
}

JSObject *
js_NewSlowArrayObject(JSContext *cx)
{
    JSObject *obj = js_NewObject(cx, &js_SlowArrayClass, NULL, NULL);
    if (obj)
        obj->fslots[JSSLOT_ARRAY_LENGTH] = 0;
    return obj;
}

#ifdef DEBUG_ARRAYS
JSBool
js_ArrayInfo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSObject *array;

    for (i = 0; i < argc; i++) {
        char *bytes;

        bytes = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, argv[i],
                                           NULL);
        if (!bytes)
            return JS_FALSE;
        if (JSVAL_IS_PRIMITIVE(argv[i]) ||
            !(array = JSVAL_TO_OBJECT(argv[i]))->isArray()) {
            fprintf(stderr, "%s: not array\n", bytes);
            cx->free(bytes);
            continue;
        }
        fprintf(stderr, "%s: %s (len %lu", bytes,
                array->isDenseArray()) ? "dense" : "sparse",
                array->fslots[JSSLOT_ARRAY_LENGTH]);
        if (array->isDenseArray()) {
            fprintf(stderr, ", count %lu, capacity %lu",
                    array->fslots[JSSLOT_ARRAY_COUNT],
                    js_DenseArrayCapacity(array));
        }
        fputs(")\n", stderr);
        cx->free(bytes);
    }
    return JS_TRUE;
}
#endif

JS_FRIEND_API(JSBool)
js_CoerceArrayToCanvasImageData(JSObject *obj, jsuint offset, jsuint count,
                                JSUint8 *dest)
{
    uint32 length;

    if (!obj || !obj->isDenseArray())
        return JS_FALSE;

    length = obj->fslots[JSSLOT_ARRAY_LENGTH];
    if (length < offset + count)
        return JS_FALSE;

    JSUint8 *dp = dest;
    for (uintN i = offset; i < offset+count; i++) {
        jsval v = obj->dslots[i];
        if (JSVAL_IS_INT(v)) {
            jsint vi = JSVAL_TO_INT(v);
            if (jsuint(vi) > 255)
                vi = (vi < 0) ? 0 : 255;
            *dp++ = JSUint8(vi);
        } else if (JSVAL_IS_DOUBLE(v)) {
            jsdouble vd = *JSVAL_TO_DOUBLE(v);
            if (!(vd >= 0)) /* Not < so that NaN coerces to 0 */
                *dp++ = 0;
            else if (vd > 255)
                *dp++ = 255;
            else {
                jsdouble toTruncate = vd + 0.5;
                JSUint8 val = JSUint8(toTruncate);

                /*
                 * now val is rounded to nearest, ties rounded up.  We want
                 * rounded to nearest ties to even, so check whether we had a
                 * tie.
                 */
                if (val == toTruncate) {
                  /*
                   * It was a tie (since adding 0.5 gave us the exact integer
                   * we want).  Since we rounded up, we either already have an
                   * even number or we have an odd number but the number we
                   * want is one less.  So just unconditionally masking out the
                   * ones bit should do the trick to get us the value we
                   * want.
                   */
                  *dp++ = (val & ~1);
                } else {
                  *dp++ = val;
                }
            }
        } else {
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

JS_FRIEND_API(JSObject *)
js_NewArrayObjectWithCapacity(JSContext *cx, jsuint capacity, jsval **vector)
{
    JSObject *obj = js_NewArrayObject(cx, capacity, NULL);
    if (!obj)
        return NULL;

    AutoValueRooter tvr(cx, obj);
    if (!EnsureCapacity(cx, obj, capacity, JS_FALSE))
        obj = NULL;

    /* Set/clear newborn root, in case we lost it.  */
    cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = obj;
    if (!obj)
        return NULL;

    obj->fslots[JSSLOT_ARRAY_COUNT] = capacity;
    *vector = obj->dslots;
    return obj;
}
