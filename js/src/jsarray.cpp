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
 * access over a vector of slots with high load factor.  Array methods
 * optimize for denseness by testing that the object's class is
 * &ArrayClass, and can then directly manipulate the slots for efficiency.
 *
 * We track these pieces of metadata for arrays in dense mode:
 *  - The array's length property as a uint32, accessible with
 *    getArrayLength(), setArrayLength().
 *  - The number of element slots (capacity), gettable with
 *    getDenseArrayCapacity().
 *  - The array's initialized length, accessible with
 *    getDenseArrayInitializedLength().
 *
 * In dense mode, holes in the array are represented by
 * MagicValue(JS_ARRAY_HOLE) invalid values.
 *
 * NB: the capacity and length of a dense array are entirely unrelated!  The
 * length may be greater than, less than, or equal to the capacity. The first
 * case may occur when the user writes "new Array(100)", in which case the
 * length is 100 while the capacity remains 0 (indices below length and above
 * capacity must be treated as holes). See array_length_setter for another
 * explanation of how the first case may occur.
 *
 * The initialized length of a dense array specifies the number of elements
 * that have been initialized. All elements above the initialized length are
 * holes in the array, and the memory for all elements between the initialized
 * length and capacity is left uninitialized. When type inference is disabled,
 * the initialized length always equals the array's capacity. When inference is
 * enabled, the initialized length is some value less than or equal to both the
 * array's length and the array's capacity.
 *
 * With inference enabled, there is flexibility in exactly the value the
 * initialized length must hold, e.g. if an array has length 5, capacity 10,
 * completely empty, it is valid for the initialized length to be any value
 * between zero and 5, as long as the in memory values below the initialized
 * length have been initialized with a hole value. However, in such cases we
 * want to keep the initialized length as small as possible: if the array is
 * known to have no hole values below its initialized length, then it is a
 * "packed" array and can be accessed much faster by JIT code.
 *
 * Arrays are converted to use SlowArrayClass when any of these conditions
 * are met:
 *  - there are more than MIN_SPARSE_INDEX slots total and the load factor
 *    (COUNT / capacity) is less than 0.25
 *  - a property is set that is not indexed (and not "length")
 *  - a property is defined that has non-default property attributes.
 *
 * Dense arrays do not track property creation order, so unlike other native
 * objects and slow arrays, enumerating an array does not necessarily visit the
 * properties in the order they were created.  We could instead maintain the
 * scope to track property enumeration order, but still use the fast slot
 * access.  That would have the same memory cost as just using a
 * SlowArrayClass, but have the same performance characteristics as a dense
 * array for slot accesses, at some cost in code complexity.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "mozilla/RangedPtr.h"

#include "jstypes.h"
#include "jsutil.h"

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jswrapper.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/StubCalls.h"
#include "methodjit/StubCalls-inl.h"

#include "vm/ArgumentsObject.h"
#include "vm/MethodGuard.h"
#include "vm/StringBuffer-inl.h"

#include "ds/Sort.h"

#include "jsarrayinlines.h"
#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsstrinlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/Stack-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;

JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, uint32_t *lengthp)
{
    if (obj->isArray()) {
        *lengthp = obj->getArrayLength();
        return true;
    }

    if (obj->isArguments()) {
        ArgumentsObject &argsobj = obj->asArguments();
        if (!argsobj.hasOverriddenLength()) {
            *lengthp = argsobj.initialLength();
            return true;
        }
    }

    AutoValueRooter tvr(cx);
    if (!obj->getProperty(cx, cx->runtime->atomState.lengthAtom, tvr.addr()))
        return false;

    if (tvr.value().isInt32()) {
        *lengthp = uint32_t(tvr.value().toInt32()); /* uint32_t cast does ToUint32_t */
        return true;
    }

    
    return ToUint32(cx, tvr.value(), (uint32_t *)lengthp);
}

namespace js {

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
 * This means the largest allowed index is actually 2^32-2 (4294967294).
 *
 * In our implementation, it would be sufficient to check for JSVAL_IS_INT(id)
 * except that by using signed 31-bit integers we miss the top half of the
 * valid range. This function checks the string representation itself; note
 * that calling a standard conversion routine might allow strings such as
 * "08" or "4.0" as array indices, which they are not.
 *
 */
JS_FRIEND_API(bool)
StringIsArrayIndex(JSLinearString *str, uint32_t *indexp)
{
    const jschar *s = str->chars();
    uint32_t length = str->length();
    const jschar *end = s + length;

    if (length == 0 || length > (sizeof("4294967294") - 1) || !JS7_ISDEC(*s))
        return false;

    uint32_t c = 0, previous = 0;
    uint32_t index = JS7_UNDEC(*s++);

    /* Don't allow leading zeros. */
    if (index == 0 && s != end)
        return false;

    for (; s < end; s++) {
        if (!JS7_ISDEC(*s))
            return false;

        previous = index;
        c = JS7_UNDEC(*s);
        index = 10 * index + c;
    }

    /* Make sure we didn't overflow. */
    if (previous < (MAX_ARRAY_INDEX / 10) || (previous == (MAX_ARRAY_INDEX / 10) &&
        c <= (MAX_ARRAY_INDEX % 10))) {
        JS_ASSERT(index <= MAX_ARRAY_INDEX);
        *indexp = index;
        return true;
    }

    return false;
}

}

static JSBool
BigIndexToId(JSContext *cx, JSObject *obj, uint32_t index, JSBool createAtom,
             jsid *idp)
{
    
    JS_ASSERT(index > JSID_INT_MAX);

    jschar buf[10];
    jschar *start = ArrayEnd(buf);
    do {
        --start;
        *start = (jschar)('0' + index % 10);
        index /= 10;
    } while (index != 0);

    /*
     * Skip the atomization if the class is known to store atoms corresponding
     * to big indexes together with elements. In such case we know that the
     * array does not have an element at the given index if its atom does not
     * exist.  Dense arrays don't use atoms for any indexes, though it would be
     * rare to see them have a big index in any case.
     */
    JSAtom *atom;
    if (!createAtom && (obj->isSlowArray() || obj->isArguments() || obj->isObject())) {
        atom = js_GetExistingStringAtom(cx, start, ArrayEnd(buf) - start);
        if (!atom) {
            *idp = JSID_VOID;
            return JS_TRUE;
        }
    } else {
        atom = js_AtomizeChars(cx, start, ArrayEnd(buf) - start);
        if (!atom)
            return JS_FALSE;
    }

    *idp = ATOM_TO_JSID(atom);
    return JS_TRUE;
}

bool
JSObject::willBeSparseDenseArray(unsigned requiredCapacity, unsigned newElementsHint)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(requiredCapacity > MIN_SPARSE_INDEX);

    unsigned cap = getDenseArrayCapacity();
    JS_ASSERT(requiredCapacity >= cap);

    if (requiredCapacity >= JSObject::NELEMENTS_LIMIT)
        return true;

    unsigned minimalDenseCount = requiredCapacity / 4;
    if (newElementsHint >= minimalDenseCount)
        return false;
    minimalDenseCount -= newElementsHint;

    if (minimalDenseCount > cap)
        return true;

    unsigned len = getDenseArrayInitializedLength();
    const Value *elems = getDenseArrayElements();
    for (unsigned i = 0; i < len; i++) {
        if (!elems[i].isMagic(JS_ARRAY_HOLE) && !--minimalDenseCount)
            return false;
    }
    return true;
}

static bool
ReallyBigIndexToId(JSContext* cx, double index, jsid* idp)
{
    return js_ValueToStringId(cx, DoubleValue(index), idp);
}

static bool
IndexToId(JSContext* cx, JSObject* obj, double index, JSBool* hole, jsid* idp,
          JSBool createAtom = JS_FALSE)
{
    if (index <= JSID_INT_MAX) {
        *idp = INT_TO_JSID(int(index));
        return JS_TRUE;
    }

    if (index <= uint32_t(-1)) {
        if (!BigIndexToId(cx, obj, uint32_t(index), createAtom, idp))
            return JS_FALSE;
        if (hole && JSID_IS_VOID(*idp))
            *hole = JS_TRUE;
        return JS_TRUE;
    }

    return ReallyBigIndexToId(cx, index, idp);
}

bool
JSObject::arrayGetOwnDataElement(JSContext *cx, size_t i, Value *vp)
{
    JS_ASSERT(isArray());

    if (isDenseArray()) {
        if (i >= getArrayLength())
            vp->setMagic(JS_ARRAY_HOLE);
        else
            *vp = getDenseArrayElement(uint32_t(i));
        return true;
    }

    JSBool hole;
    jsid id;
    if (!IndexToId(cx, this, i, &hole, &id))
        return false;

    const Shape *shape = nativeLookup(cx, id);
    if (!shape || !shape->isDataDescriptor())
        vp->setMagic(JS_ARRAY_HOLE);
    else
        *vp = getSlot(shape->slot());
    return true;
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
static inline JSBool
DoGetElement(JSContext *cx, JSObject *obj, double index, JSBool *hole, Value *vp)
{
    AutoIdRooter idr(cx);

    *hole = JS_FALSE;
    if (!IndexToId(cx, obj, index, hole, idr.addr()))
        return JS_FALSE;
    if (*hole) {
        vp->setUndefined();
        return JS_TRUE;
    }

    JSObject *obj2;
    JSProperty *prop;
    if (!obj->lookupGeneric(cx, idr.id(), &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        vp->setUndefined();
        *hole = JS_TRUE;
    } else {
        if (!obj->getGeneric(cx, idr.id(), vp))
            return JS_FALSE;
        *hole = JS_FALSE;
    }
    return JS_TRUE;
}

static inline JSBool
DoGetElement(JSContext *cx, JSObject *obj, uint32_t index, JSBool *hole, Value *vp)
{
    bool present;
    if (!obj->getElementIfPresent(cx, obj, index, vp, &present))
        return false;

    *hole = !present;
    if (*hole)
        vp->setUndefined();

    return true;
}

template<typename IndexType>
static JSBool
GetElement(JSContext *cx, JSObject *obj, IndexType index, JSBool *hole, Value *vp)
{
    JS_ASSERT(index >= 0);
    if (obj->isDenseArray() && index < obj->getDenseArrayInitializedLength() &&
        !(*vp = obj->getDenseArrayElement(uint32_t(index))).isMagic(JS_ARRAY_HOLE)) {
        *hole = JS_FALSE;
        return JS_TRUE;
    }
    if (obj->isArguments()) {
        if (obj->asArguments().getElement(uint32_t(index), vp)) {
            *hole = JS_FALSE;
            return true;
        }
    }

    return DoGetElement(cx, obj, index, hole, vp);
}

namespace js {

static bool
GetElementsSlow(JSContext *cx, JSObject *aobj, uint32_t length, Value *vp)
{
    for (uint32_t i = 0; i < length; i++) {
        if (!aobj->getElement(cx, i, &vp[i]))
            return false;
    }

    return true;
}

bool
GetElements(JSContext *cx, JSObject *aobj, uint32_t length, Value *vp)
{
    if (aobj->isDenseArray() && length <= aobj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(cx, aobj)) {
        /* The prototype does not have indexed properties so hole = undefined */
        const Value *srcbeg = aobj->getDenseArrayElements();
        const Value *srcend = srcbeg + length;
        const Value *src = srcbeg;
        for (Value *dst = vp; src < srcend; ++dst, ++src)
            *dst = src->isMagic(JS_ARRAY_HOLE) ? UndefinedValue() : *src;
        return true;
    }

    if (aobj->isArguments()) {
        ArgumentsObject &argsobj = aobj->asArguments();
        if (!argsobj.hasOverriddenLength()) {
            if (argsobj.getElements(0, length, vp))
                return true;
        }
    }

    return GetElementsSlow(cx, aobj, length, vp);
}

}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static JSBool
SetArrayElement(JSContext *cx, JSObject *obj, double index, const Value &v)
{
    JS_ASSERT(index >= 0);

    if (obj->isDenseArray()) {
        /* Predicted/prefetched code should favor the remains-dense case. */
        JSObject::EnsureDenseResult result = JSObject::ED_SPARSE;
        do {
            if (index > uint32_t(-1))
                break;
            uint32_t idx = uint32_t(index);
            result = obj->ensureDenseArrayElements(cx, idx, 1);
            if (result != JSObject::ED_OK)
                break;
            if (idx >= obj->getArrayLength())
                obj->setDenseArrayLength(idx + 1);
            obj->setDenseArrayElementWithType(cx, idx, v);
            return true;
        } while (false);

        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!obj->makeDenseArraySlow(cx))
            return JS_FALSE;
    }

    AutoIdRooter idr(cx);

    if (!IndexToId(cx, obj, index, NULL, idr.addr(), JS_TRUE))
        return JS_FALSE;
    JS_ASSERT(!JSID_IS_VOID(idr.id()));

    Value tmp = v;
    return obj->setGeneric(cx, idr.id(), &tmp, true);
}

/*
 * Delete the element |index| from |obj|. If |strict|, do a strict
 * deletion: throw if the property is not configurable.
 *
 * - Return 1 if the deletion succeeds (that is, ES5's [[Delete]] would
 *   return true)
 *
 * - Return 0 if the deletion fails because the property is not
 *   configurable (that is, [[Delete]] would return false). Note that if
 *   |strict| is true we will throw, not return zero.
 *
 * - Return -1 if an exception occurs (that is, [[Delete]] would throw).
 */
static int
DeleteArrayElement(JSContext *cx, JSObject *obj, double index, bool strict)
{
    JS_ASSERT(index >= 0);
    JS_ASSERT(floor(index) == index);

    if (obj->isDenseArray()) {
        if (index <= UINT32_MAX) {
            uint32_t idx = uint32_t(index);
            if (idx < obj->getDenseArrayInitializedLength()) {
                obj->markDenseArrayNotPacked(cx);
                obj->setDenseArrayElement(idx, MagicValue(JS_ARRAY_HOLE));
                if (!js_SuppressDeletedElement(cx, obj, idx))
                    return -1;
            }
        }
        return 1;
    }

    Value v;
    if (index <= UINT32_MAX) {
        if (!obj->deleteElement(cx, uint32_t(index), &v, strict))
            return -1;
    } else {
        if (!obj->deleteByValue(cx, DoubleValue(index), &v, strict))
            return -1;
    }

    return v.isTrue() ? 1 : 0;
}

/*
 * When hole is true, delete the property at the given index. Otherwise set
 * its value to v assuming v is rooted.
 */
static JSBool
SetOrDeleteArrayElement(JSContext *cx, JSObject *obj, double index,
                        JSBool hole, const Value &v)
{
    if (hole) {
        JS_ASSERT(v.isUndefined());
        return DeleteArrayElement(cx, obj, index, true) >= 0;
    }
    return SetArrayElement(cx, obj, index, v);
}

JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, double length)
{
    Value v = NumberValue(length);

    /* We don't support read-only array length yet. */
    return obj->setProperty(cx, cx->runtime->atomState.lengthAtom, &v, false);
}

/*
 * Since SpiderMonkey supports cross-class prototype-based delegation, we have
 * to be careful about the length getter and setter being called on an object
 * not of Array class. For the getter, we search obj's prototype chain for the
 * array that caused this getter to be invoked. In the setter case to overcome
 * the JSPROP_SHARED attribute, we must define a shadowing length property.
 */
static JSBool
array_length_getter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    do {
        if (obj->isArray()) {
            vp->setNumber(obj->getArrayLength());
            return JS_TRUE;
        }
    } while ((obj = obj->getProto()) != NULL);
    return JS_TRUE;
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!obj->isArray()) {
        return obj->defineProperty(cx, cx->runtime->atomState.lengthAtom, *vp,
                                   NULL, NULL, JSPROP_ENUMERATE);
    }

    uint32_t newlen;
    if (!ToUint32(cx, *vp, &newlen))
        return false;

    double d;
    if (!ToNumber(cx, *vp, &d))
        return false;

    if (d != newlen) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    uint32_t oldlen = obj->getArrayLength();
    if (oldlen == newlen)
        return true;

    vp->setNumber(newlen);
    if (oldlen < newlen) {
        obj->setArrayLength(cx, newlen);
        return true;
    }

    if (obj->isDenseArray()) {
        /*
         * Don't reallocate if we're not actually shrinking our slots. If we do
         * shrink slots here, shrink the initialized length too.  This permits us
         * us to disregard length when reading from arrays as long we are within
         * the initialized capacity.
         */
        uint32_t oldcap = obj->getDenseArrayCapacity();
        uint32_t oldinit = obj->getDenseArrayInitializedLength();
        if (oldinit > newlen)
            obj->setDenseArrayInitializedLength(newlen);
        if (oldcap > newlen)
            obj->shrinkElements(cx, newlen);
    } else if (oldlen - newlen < (1 << 24)) {
        do {
            --oldlen;
            if (!JS_CHECK_OPERATION_LIMIT(cx)) {
                obj->setArrayLength(cx, oldlen + 1);
                return false;
            }
            int deletion = DeleteArrayElement(cx, obj, oldlen, strict);
            if (deletion <= 0) {
                obj->setArrayLength(cx, oldlen + 1);
                return deletion >= 0;
            }
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

        uint32_t gap = oldlen - newlen;
        for (;;) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) || !JS_NextProperty(cx, iter, &id))
                return false;
            if (JSID_IS_VOID(id))
                break;
            uint32_t index;
            Value junk;
            if (js_IdIsIndex(id, &index) && index - newlen < gap &&
                !obj->deleteElement(cx, index, &junk, false)) {
                return false;
            }
        }
    }

    obj->setArrayLength(cx, newlen);
    return true;
}

/* Returns true if the dense array has an own property at the index. */
static inline bool
IsDenseArrayIndex(JSObject *obj, uint32_t index)
{
    JS_ASSERT(obj->isDenseArray());

    return index < obj->getDenseArrayInitializedLength() &&
           !obj->getDenseArrayElement(index).isMagic(JS_ARRAY_HOLE);
}

/*
 * We have only indexed properties up to initialized length, plus the
 * length property. For all else, we delegate to the prototype.
 */
static inline bool
IsDenseArrayId(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t i;
    return JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
           (js_IdIsIndex(id, &i) && IsDenseArrayIndex(obj, i));
}

static JSBool
array_lookupGeneric(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                    JSProperty **propp)
{
    if (!obj->isDenseArray())
        return js_LookupProperty(cx, obj, id, objp, propp);

    if (IsDenseArrayId(cx, obj, id)) {
        *propp = (JSProperty *) 1;  /* non-null to indicate found */
        *objp = obj;
        return JS_TRUE;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        *objp = NULL;
        *propp = NULL;
        return JS_TRUE;
    }
    return proto->lookupGeneric(cx, id, objp, propp);
}

static JSBool
array_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name, JSObject **objp,
                     JSProperty **propp)
{
    return array_lookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

static JSBool
array_lookupElement(JSContext *cx, JSObject *obj, uint32_t index, JSObject **objp,
                    JSProperty **propp)
{
    if (!obj->isDenseArray())
        return js_LookupElement(cx, obj, index, objp, propp);

    if (IsDenseArrayIndex(obj, index)) {
        *propp = (JSProperty *) 1;  /* non-null to indicate found */
        *objp = obj;
        return true;
    }

    if (JSObject *proto = obj->getProto())
        return proto->lookupElement(cx, index, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

static JSBool
array_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp,
                    JSProperty **propp)
{
    return array_lookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

JSBool
js_GetDenseArrayElementValue(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t i;
    if (!js_IdIsIndex(id, &i)) {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
        vp->setNumber(obj->getArrayLength());
        return JS_TRUE;
    }
    *vp = obj->getDenseArrayElement(i);
    return JS_TRUE;
}

static JSBool
array_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name, Value *vp)
{
    if (name == cx->runtime->atomState.lengthAtom) {
        vp->setNumber(obj->getArrayLength());
        return true;
    }

    if (name == cx->runtime->atomState.protoAtom) {
        vp->setObjectOrNull(obj->getProto());
        return true;
    }

    if (!obj->isDenseArray())
        return js_GetProperty(cx, obj, receiver, ATOM_TO_JSID(name), vp);

    JSObject *proto = obj->getProto();
    if (!proto) {
        vp->setUndefined();
        return true;
    }

    return proto->getProperty(cx, receiver, name, vp);
}

static JSBool
array_getElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
{
    if (!obj->isDenseArray())
        return js_GetElement(cx, obj, receiver, index, vp);

    if (index < obj->getDenseArrayInitializedLength()) {
        *vp = obj->getDenseArrayElement(index);
        if (!vp->isMagic(JS_ARRAY_HOLE)) {
            /* Type information for dense array elements must be correct. */
            JS_ASSERT_IF(!obj->hasSingletonType(),
                         js::types::TypeHasProperty(cx, obj->type(), JSID_VOID, *vp));

            return true;
        }
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        vp->setUndefined();
        return true;
    }

    return proto->getElement(cx, receiver, index, vp);
}

static JSBool
array_getSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
{
    if (obj->isDenseArray() && !obj->getProto()) {
        vp->setUndefined();
        return true;
    }

    return js_GetProperty(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

static JSBool
array_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    Value idval = IdToValue(id);

    uint32_t index;
    if (IsDefinitelyIndex(idval, &index))
        return array_getElement(cx, obj, receiver, index, vp);

    SpecialId sid;
    if (ValueIsSpecial(obj, &idval, &sid, cx))
        return array_getSpecial(cx, obj, receiver, sid, vp);

    JSAtom *atom;
    if (!js_ValueToAtom(cx, idval, &atom))
        return false;

    if (atom->isIndex(&index))
        return array_getElement(cx, obj, receiver, index, vp);

    return array_getProperty(cx, obj, receiver, atom->asPropertyName(), vp);
}

static JSBool
slowarray_addProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    uint32_t index, length;

    if (!js_IdIsIndex(id, &index))
        return JS_TRUE;
    length = obj->getArrayLength();
    if (index >= length)
        obj->setArrayLength(cx, index + 1);
    return JS_TRUE;
}

static JSType
array_typeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

static JSBool
array_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    uint32_t i;

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
        return array_length_setter(cx, obj, id, strict, vp);

    if (!obj->isDenseArray())
        return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);

    do {
        if (!js_IdIsIndex(id, &i))
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, i, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (i >= obj->getArrayLength())
            obj->setDenseArrayLength(i + 1);
        obj->setDenseArrayElementWithType(cx, i, *vp);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);
}

static JSBool
array_setProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
{
    return array_setGeneric(cx, obj, ATOM_TO_JSID(name), vp, strict);
}

static JSBool
array_setElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    if (!obj->isDenseArray())
        return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);

    do {
        /*
         * UINT32_MAX is not an array index and must not affect the length
         * property, so specifically reject it.
         */
        if (index == UINT32_MAX)
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, index, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (index >= obj->getArrayLength())
            obj->setDenseArrayLength(index + 1);
        obj->setDenseArrayElementWithType(cx, index, *vp);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);
}

static JSBool
array_setSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
{
    return array_setGeneric(cx, obj, SPECIALID_TO_JSID(sid), vp, strict);
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
        if (obj->isIndexed())
            return JS_TRUE;
    }
    return JS_FALSE;
}

static JSBool
array_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                    JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
        return JS_TRUE;

    if (!obj->isDenseArray())
        return js_DefineProperty(cx, obj, id, value, getter, setter, attrs);

    do {
        uint32_t i = 0;       // init to shut GCC up
        bool isIndex = js_IdIsIndex(id, &i);
        if (!isIndex || attrs != JSPROP_ENUMERATE)
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, i, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (i >= obj->getArrayLength())
            obj->setDenseArrayLength(i + 1);
        obj->setDenseArrayElementWithType(cx, i, *value);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_DefineProperty(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
array_defineProperty(JSContext *cx, JSObject *obj, PropertyName *name, const Value *value,
                     JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return array_defineGeneric(cx, obj, ATOM_TO_JSID(name), value, getter, setter, attrs);
}

namespace js {

/* non-static for direct definition of array elements within the engine */
JSBool
array_defineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (!obj->isDenseArray())
        return js_DefineElement(cx, obj, index, value, getter, setter, attrs);

    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    do {
        /*
         * UINT32_MAX is not an array index and must not affect the length
         * property, so specifically reject it.
         */
        if (attrs != JSPROP_ENUMERATE || index == UINT32_MAX)
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, index, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (index >= obj->getArrayLength())
            obj->setDenseArrayLength(index + 1);
        obj->setDenseArrayElementWithType(cx, index, *value);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_DefineElement(cx, obj, index, value, getter, setter, attrs);
}

} // namespace js

static JSBool
array_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return array_defineGeneric(cx, obj, SPECIALID_TO_JSID(sid), value, getter, setter, attrs);
}

static JSBool
array_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    *attrsp = JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)
        ? JSPROP_PERMANENT : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    *attrsp = (name == cx->runtime->atomState.lengthAtom)
              ? JSPROP_PERMANENT
              : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_deleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return js_DeleteProperty(cx, obj, name, rval, strict);

    if (name == cx->runtime->atomState.lengthAtom) {
        rval->setBoolean(false);
        return true;
    }

    rval->setBoolean(true);
    return true;
}

namespace js {

/* non-static for direct deletion of array elements within the engine */
JSBool
array_deleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return js_DeleteElement(cx, obj, index, rval, strict);

    if (index < obj->getDenseArrayInitializedLength()) {
        obj->markDenseArrayNotPacked(cx);
        obj->setDenseArrayElement(index, MagicValue(JS_ARRAY_HOLE));
    }

    if (!js_SuppressDeletedElement(cx, obj, index))
        return false;

    rval->setBoolean(true);
    return true;
}

} // namespace js

static JSBool
array_deleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return js_DeleteSpecial(cx, obj, sid, rval, strict);

    rval->setBoolean(true);
    return true;
}

static void
array_trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t initLength = obj->getDenseArrayInitializedLength();
    MarkArraySlots(trc, initLength, obj->getDenseArrayElements(), "element");
}

static JSBool
array_fix(JSContext *cx, JSObject *obj, bool *success, AutoIdVector *props)
{
    JS_ASSERT(obj->isDenseArray());

    /*
     * We must slowify dense arrays; otherwise, we'd need to detect assignments to holes,
     * since that is effectively adding a new property to the array.
     */
    if (!obj->makeDenseArraySlow(cx) ||
        !GetPropertyNames(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY, props))
        return false;

    *success = true;
    return true;
}

Class js::ArrayClass = {
    "Array",
    Class::NON_NATIVE | JSCLASS_HAS_CACHED_PROTO(JSProto_Array) | JSCLASS_FOR_OF_ITERATION,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* hasInstance */
    array_trace,    /* trace       */
    {
        NULL,       /* equality    */
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        JS_ElementIteratorStub,
        NULL,       /* unused      */
        false,      /* isWrappedNative */
    },
    {
        array_lookupGeneric,
        array_lookupProperty,
        array_lookupElement,
        array_lookupSpecial,
        array_defineGeneric,
        array_defineProperty,
        array_defineElement,
        array_defineSpecial,
        array_getGeneric,
        array_getProperty,
        array_getElement,
        NULL, /* getElementIfPresent, because this is hard for now for
                 slow arrays */
        array_getSpecial,
        array_setGeneric,
        array_setProperty,
        array_setElement,
        array_setSpecial,
        array_getGenericAttributes,
        array_getPropertyAttributes,
        array_getElementAttributes,
        array_getSpecialAttributes,
        array_setGenericAttributes,
        array_setPropertyAttributes,
        array_setElementAttributes,
        array_setSpecialAttributes,
        array_deleteProperty,
        array_deleteElement,
        array_deleteSpecial,
        NULL,       /* enumerate      */
        array_typeOf,
        array_fix,
        NULL,       /* thisObject     */
        NULL,       /* clear          */
    }
};

Class js::SlowArrayClass = {
    "Array",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array) | JSCLASS_FOR_OF_ITERATION,
    slowarray_addProperty,
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* hasInstance */
    NULL,           /* trace       */
    {
        NULL,       /* equality    */
        NULL,       /* outerObject */
        NULL,       /* innerObject */
        JS_ElementIteratorStub,
        NULL,       /* unused      */
        false,      /* isWrappedNative */
    }
};

bool
JSObject::allocateSlowArrayElements(JSContext *cx)
{
    JS_ASSERT(hasClass(&js::SlowArrayClass));
    JS_ASSERT(elements == emptyObjectElements);

    ObjectElements *header = cx->new_<ObjectElements>(0, 0);
    if (!header)
        return false;

    elements = header->elements();
    return true;
}

static bool
AddLengthProperty(JSContext *cx, JSObject *obj)
{
    /*
     * Add the 'length' property for a newly created or converted slow array,
     * and update the elements to be an empty array owned by the object.
     * The shared emptyObjectElements singleton cannot be used for slow arrays,
     * as accesses to 'length' will use the elements header.
     */

    const jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    JS_ASSERT(!obj->nativeLookup(cx, lengthId));

    if (!obj->allocateSlowArrayElements(cx))
        return false;

    return obj->addProperty(cx, lengthId, array_length_getter, array_length_setter,
                            SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_SHARED, 0, 0);
}

/*
 * Convert an array object from fast-and-dense to slow-and-flexible.
 */
JSBool
JSObject::makeDenseArraySlow(JSContext *cx)
{
    JS_ASSERT(isDenseArray());

    MarkTypeObjectFlags(cx, this,
                        OBJECT_FLAG_NON_PACKED_ARRAY |
                        OBJECT_FLAG_NON_DENSE_ARRAY);

    uint32_t arrayCapacity = getDenseArrayCapacity();
    uint32_t arrayInitialized = getDenseArrayInitializedLength();

    /*
     * Get an allocated array of the existing elements, evicting from the fixed
     * slots if necessary.
     */
    if (!hasDynamicElements()) {
        if (!growElements(cx, arrayCapacity))
            return false;
        JS_ASSERT(hasDynamicElements());
    }

    /*
     * Save old map now, before calling InitScopeForObject. We'll have to undo
     * on error. This is gross, but a better way is not obvious. Note: the
     * exact contents of the array are not preserved on error.
     */
    js::Shape *oldShape = lastProperty();

    /* Create a native scope. */
    gc::AllocKind kind = getAllocKind();
    Shape *shape = EmptyShape::getInitialShape(cx, &SlowArrayClass, getProto(),
                                               oldShape->getObjectParent(), kind);
    if (!shape)
        return false;
    this->shape_ = shape;

    /* Take ownership of the dense elements, reset to an empty dense array. */
    HeapSlot *elems = elements;
    elements = emptyObjectElements;

    /* Root all values in the array during conversion. */
    AutoValueArray autoArray(cx, (Value *) elems, arrayInitialized);

    /*
     * Begin with the length property to share more of the property tree.
     * The getter/setter here will directly access the object's private value.
     */
    if (!AddLengthProperty(cx, this)) {
        this->shape_ = oldShape;
        if (elements != emptyObjectElements)
            cx->free_(getElementsHeader());
        elements = elems;
        return false;
    }

    /*
     * Create new properties pointing to existing elements. Pack the array to
     * remove holes, so that shapes use successive slots (as for other objects).
     */
    uint32_t next = 0;
    for (uint32_t i = 0; i < arrayInitialized; i++) {
        /* Dense array indexes can always fit in a jsid. */
        jsid id;
        JS_ALWAYS_TRUE(ValueToId(cx, Int32Value(i), &id));

        if (elems[i].isMagic(JS_ARRAY_HOLE))
            continue;

        if (!addDataProperty(cx, id, next, JSPROP_ENUMERATE)) {
            this->shape_ = oldShape;
            cx->free_(getElementsHeader());
            elements = elems;
            return false;
        }

        initSlot(next, elems[i]);

        next++;
    }

    ObjectElements *oldheader = ObjectElements::fromElements(elems);

    getElementsHeader()->length = oldheader->length;
    cx->free_(oldheader);

    return true;
}

#if JS_HAS_TOSOURCE
class ArraySharpDetector
{
    JSContext *cx;
    bool success;
    bool alreadySeen;
    bool sharp;

  public:
    ArraySharpDetector(JSContext *cx)
      : cx(cx),
        success(false),
        alreadySeen(false),
        sharp(false)
    {}

    bool init(JSObject *obj) {
        success = js_EnterSharpObject(cx, obj, NULL, &alreadySeen, &sharp);
        if (!success)
            return false;
        return true;
    }

    bool initiallySharp() const {
        JS_ASSERT_IF(sharp, alreadySeen);
        return sharp;
    }

    ~ArraySharpDetector() {
        if (success && !sharp)
            js_LeaveSharpObject(cx, NULL);
    }
};

static JSBool
array_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (!obj->isArray())
        return HandleNonGenericMethodClassMismatch(cx, args, array_toSource, &ArrayClass);

    ArraySharpDetector detector(cx);
    if (!detector.init(obj))
        return false;

    StringBuffer sb(cx);

    if (detector.initiallySharp()) {
        if (!sb.append("[]"))
            return false;
        goto make_string;
    }

    if (!sb.append('['))
        return false;

    uint32_t length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    for (uint32_t index = 0; index < length; index++) {
        JSBool hole;
        Value elt;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, index, &hole, &elt)) {
            return false;
        }

        /* Get element's character string. */
        JSString *str;
        if (hole) {
            str = cx->runtime->emptyString;
        } else {
            str = js_ValueToSource(cx, elt);
            if (!str)
                return false;
        }

        /* Append element to buffer. */
        if (!sb.append(str))
            return false;
        if (index + 1 != length) {
            if (!sb.append(", "))
                return false;
        } else if (hole) {
            if (!sb.append(','))
                return false;
        }
    }

    /* Finalize the buffer. */
    if (!sb.append(']'))
        return false;

  make_string:
    JSString *str = sb.finishString();
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}
#endif

class AutoArrayCycleDetector
{
    JSContext *cx;
    JSObject *obj;
    uint32_t genBefore;
    BusyArraysSet::AddPtr hashPointer;
    bool cycle;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoArrayCycleDetector(JSContext *cx, JSObject *obj JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx),
        obj(obj),
        cycle(true)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    bool init()
    {
        BusyArraysSet &set = cx->busyArrays;
        hashPointer = set.lookupForAdd(obj);
        if (!hashPointer) {
            if (!set.add(hashPointer, obj))
                return false;
            cycle = false;
            genBefore = set.generation();
        }
        return true;
    }

    ~AutoArrayCycleDetector()
    {
        if (!cycle) {
            if (genBefore == cx->busyArrays.generation())
                cx->busyArrays.remove(hashPointer);
            else
                cx->busyArrays.remove(obj);
        }
    }

    bool foundCycle() { return cycle; }

  protected:
};

static JSBool
array_toString_sub(JSContext *cx, JSObject *obj, JSBool locale,
                   JSString *sepstr, CallArgs &args)
{
    static const jschar comma = ',';
    const jschar *sep;
    size_t seplen;
    if (sepstr) {
        seplen = sepstr->length();
        sep = sepstr->getChars(cx);
        if (!sep)
            return false;
    } else {
        sep = &comma;
        seplen = 1;
    }

    AutoArrayCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    if (detector.foundCycle()) {
        args.rval().setString(cx->runtime->atomState.emptyAtom);
        return true;
    }

    uint32_t length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    StringBuffer sb(cx);

    if (!locale && !seplen && obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj)) {
        const Value *start = obj->getDenseArrayElements();
        const Value *end = start + obj->getDenseArrayInitializedLength();
        const Value *elem;
        for (elem = start; elem < end; elem++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            /*
             * Object stringifying is slow; delegate it to a separate loop to
             * keep this one tight.
             */
            if (elem->isObject())
                break;

            if (!elem->isMagic(JS_ARRAY_HOLE) && !elem->isNullOrUndefined()) {
                if (!ValueToStringBuffer(cx, *elem, sb))
                    return false;
            }
        }

        for (uint32_t i = uint32_t(PointerRangeSize(start, elem)); i < length; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            JSBool hole;
            Value v;
            if (!GetElement(cx, obj, i, &hole, &v))
                return false;
            if (!hole && !v.isNullOrUndefined()) {
                if (!ValueToStringBuffer(cx, v, sb))
                    return false;
            }
        }
    } else {
        for (uint32_t index = 0; index < length; index++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            JSBool hole;
            Value elt;
            if (!GetElement(cx, obj, index, &hole, &elt))
                return false;

            if (!hole && !elt.isNullOrUndefined()) {
                if (locale) {
                    JSObject *robj = ToObject(cx, &elt);
                    if (!robj)
                        return false;
                    jsid id = ATOM_TO_JSID(cx->runtime->atomState.toLocaleStringAtom);
                    if (!robj->callMethod(cx, id, 0, NULL, &elt))
                        return false;
                }
                if (!ValueToStringBuffer(cx, elt, sb))
                    return false;
            }

            if (index + 1 != length) {
                if (!sb.append(sep, seplen))
                    return false;
            }
        }
    }

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/* ES5 15.4.4.2. NB: The algorithm here differs from the one in ES3. */
static JSBool
array_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    Value join = args.calleev();
    if (!obj->getProperty(cx, cx->runtime->atomState.joinAtom, &join))
        return false;

    if (!js_IsCallable(join)) {
        JSString *str = obj_toStringHelper(cx, obj);
        if (!str)
            return false;
        args.rval().setString(str);
        return true;
    }

    InvokeArgsGuard ag;
    if (!cx->stack.pushInvokeArgs(cx, 0, &ag))
        return false;

    ag.calleev() = join;
    ag.thisv().setObject(*obj);

    /* Do the call. */
    if (!Invoke(cx, ag))
        return false;
    args.rval() = ag.rval();
    return true;
}

static JSBool
array_toLocaleString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /*
     *  Passing comma here as the separator. Need a way to get a
     *  locale-specific version.
     */
    return array_toString_sub(cx, obj, JS_TRUE, NULL, args);
}

static inline bool
InitArrayTypes(JSContext *cx, TypeObject *type, const Value *vector, unsigned count)
{
    if (cx->typeInferenceEnabled() && !type->unknownProperties()) {
        AutoEnterTypeInference enter(cx);

        TypeSet *types = type->getProperty(cx, JSID_VOID, true);
        if (!types)
            return false;

        for (unsigned i = 0; i < count; i++) {
            if (vector[i].isMagic(JS_ARRAY_HOLE))
                continue;
            Type valtype = GetValueType(cx, vector[i]);
            types->addType(cx, valtype);
        }
    }
    return true;
}

enum ShouldUpdateTypes
{
    UpdateTypes = true,
    DontUpdateTypes = false
};

static bool
InitArrayElements(JSContext *cx, JSObject *obj, uint32_t start, uint32_t count, const Value *vector, ShouldUpdateTypes updateTypes)
{
    JS_ASSERT(count <= MAX_ARRAY_INDEX);

    if (count == 0)
        return true;

    if (updateTypes && !InitArrayTypes(cx, obj->getType(cx), vector, count))
        return false;

    /*
     * Optimize for dense arrays so long as adding the given set of elements
     * wouldn't otherwise make the array slow.
     */
    do {
        if (!obj->isDenseArray())
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, start, count);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }
        uint32_t newlen = start + count;
        if (newlen > obj->getArrayLength())
            obj->setDenseArrayLength(newlen);

        JS_ASSERT(count < UINT32_MAX / sizeof(Value));
        obj->copyDenseArrayElements(start, vector, count);
        JS_ASSERT_IF(count != 0, !obj->getDenseArrayElement(newlen - 1).isMagic(JS_ARRAY_HOLE));
        return true;
    } while (false);

    const Value* end = vector + count;
    while (vector < end && start <= MAX_ARRAY_INDEX) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !SetArrayElement(cx, obj, start++, *vector++)) {
            return false;
        }
    }

    if (vector == end)
        return true;

    /* Finish out any remaining elements past the max array index. */
    if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
        return false;

    JS_ASSERT(start == MAX_ARRAY_INDEX + 1);
    AutoValueRooter tvr(cx);
    AutoIdRooter idr(cx);
    Value idval = DoubleValue(MAX_ARRAY_INDEX + 1);
    do {
        *tvr.addr() = *vector++;
        if (!js_ValueToStringId(cx, idval, idr.addr()) ||
            !obj->setGeneric(cx, idr.id(), tvr.addr(), true)) {
            return false;
        }
        idval.getDoubleRef() += 1;
    } while (vector != end);

    return true;
}

#if 0
static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, uint32_t length, const Value *vector)
{
    JS_ASSERT(obj->isArray());

    JS_ASSERT(obj->isDenseArray());
    obj->setArrayLength(cx, length);
    if (!vector || !length)
        return true;

    if (!InitArrayTypes(cx, obj->getType(cx), vector, length))
        return false;

    /* Avoid ensureDenseArrayElements to skip sparse array checks there. */
    if (!obj->ensureElements(cx, length))
        return false;

    obj->setDenseArrayInitializedLength(length);

    bool hole = false;
    for (uint32_t i = 0; i < length; i++) {
        obj->setDenseArrayElement(i, vector[i]);
        hole |= vector[i].isMagic(JS_ARRAY_HOLE);
    }
    if (hole)
        obj->markDenseArrayNotPacked(cx);

    return true;
}
#endif

/*
 * Perl-inspired join, reverse, and sort.
 */
static JSBool
array_join(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str;
    if (args.hasDefined(0)) {
        str = ToString(cx, args[0]);
        if (!str)
            return JS_FALSE;
        args[0].setString(str);
    } else {
        str = NULL;
    }
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    return array_toString_sub(cx, obj, JS_FALSE, str, args);
}

static JSBool
array_reverse(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    do {
        if (!obj->isDenseArray())
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        /* An empty array or an array with no elements is already reversed. */
        if (len == 0 || obj->getDenseArrayCapacity() == 0) {
            args.rval().setObject(*obj);
            return true;
        }

        /*
         * It's actually surprisingly complicated to reverse an array due to the
         * orthogonality of array length and array capacity while handling
         * leading and trailing holes correctly.  Reversing seems less likely to
         * be a common operation than other array mass-mutation methods, so for
         * now just take a probably-small memory hit (in the absence of too many
         * holes in the array at its start) and ensure that the capacity is
         * sufficient to hold all the elements in the array if it were full.
         */
        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, len, 0);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        /* Fill out the array's initialized length to its proper length. */
        obj->ensureDenseArrayInitializedLength(cx, len, 0);

        uint32_t lo = 0, hi = len - 1;
        for (; lo < hi; lo++, hi--) {
            Value origlo = obj->getDenseArrayElement(lo);
            Value orighi = obj->getDenseArrayElement(hi);
            obj->setDenseArrayElement(lo, orighi);
            if (orighi.isMagic(JS_ARRAY_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(lo))) {
                return false;
            }
            obj->setDenseArrayElement(hi, origlo);
            if (origlo.isMagic(JS_ARRAY_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(hi))) {
                return false;
            }
        }

        /*
         * Per ECMA-262, don't update the length of the array, even if the new
         * array has trailing holes (and thus the original array began with
         * holes).
         */
        args.rval().setObject(*obj);
        return true;
    } while (false);

    Value lowval, hival;
    for (uint32_t i = 0, half = len / 2; i < half; i++) {
        JSBool hole, hole2;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, i, &hole, &lowval) ||
            !GetElement(cx, obj, len - i - 1, &hole2, &hival) ||
            !SetOrDeleteArrayElement(cx, obj, len - i - 1, hole, lowval) ||
            !SetOrDeleteArrayElement(cx, obj, i, hole2, hival)) {
            return false;
        }
    }
    args.rval().setObject(*obj);
    return true;
}

namespace {

inline bool
CompareStringValues(JSContext *cx, const Value &a, const Value &b, bool *lessOrEqualp)
{
    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return false;

    JSString *astr = a.toString();
    JSString *bstr = b.toString();
    int32_t result;
    if (!CompareStrings(cx, astr, bstr, &result))
        return false;

    *lessOrEqualp = (result <= 0);
    return true;
}

static uint32_t const powersOf10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};

static inline unsigned
NumDigitsBase10(uint32_t n)
{
    /*
     * This is just floor_log10(n) + 1
     * Algorithm taken from
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
     */
    uint32_t log2, t;
    JS_CEILING_LOG2(log2, n);
    t = log2 * 1233 >> 12;
    return t - (n < powersOf10[t]) + 1;
}

static JS_ALWAYS_INLINE uint32_t
NegateNegativeInt32(int32_t i)
{
    /*
     * We cannot simply return '-i' because this is undefined for INT32_MIN.
     * 2s complement does actually give us what we want, however.  That is,
     * ~0x80000000 + 1 = 0x80000000 which is correct when interpreted as a
     * uint32_t. To avoid undefined behavior, we write out 2s complement
     * explicitly and rely on the peephole optimizer to generate 'neg'.
     */
    return ~uint32_t(i) + 1;
}

inline bool
CompareLexicographicInt32(JSContext *cx, const Value &a, const Value &b, bool *lessOrEqualp)
{
    int32_t aint = a.toInt32();
    int32_t bint = b.toInt32();

    /*
     * If both numbers are equal ... trivial
     * If only one of both is negative --> arithmetic comparison as char code
     * of '-' is always less than any other digit
     * If both numbers are negative convert them to positive and continue
     * handling ...
     */
    if (aint == bint) {
        *lessOrEqualp = true;
    } else if ((aint < 0) && (bint >= 0)) {
        *lessOrEqualp = true;
    } else if ((aint >= 0) && (bint < 0)) {
        *lessOrEqualp = false;
    } else {
        uint32_t auint, buint;
        if (aint >= 0) {
            auint = aint;
            buint = bint;
        } else {
            auint = NegateNegativeInt32(aint);
            buint = NegateNegativeInt32(bint);
        }

        /*
         *  ... get number of digits of both integers.
         * If they have the same number of digits --> arithmetic comparison.
         * If digits_a > digits_b: a < b*10e(digits_a - digits_b).
         * If digits_b > digits_a: a*10e(digits_b - digits_a) <= b.
         */
        unsigned digitsa = NumDigitsBase10(auint);
        unsigned digitsb = NumDigitsBase10(buint);
        if (digitsa == digitsb)
            *lessOrEqualp = (auint <= buint);
        else if (digitsa > digitsb)
            *lessOrEqualp = (uint64_t(auint) < uint64_t(buint) * powersOf10[digitsa - digitsb]);
        else /* if (digitsb > digitsa) */
            *lessOrEqualp = (uint64_t(auint) * powersOf10[digitsb - digitsa] <= uint64_t(buint));
    }

    return true;
}

inline bool
CompareSubStringValues(JSContext *cx, const jschar *s1, size_t l1,
                       const jschar *s2, size_t l2, bool *lessOrEqualp)
{
    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return false;

    int32_t result;
    if (!s1 || !s2 || !CompareChars(s1, l1, s2, l2, &result))
        return false;

    *lessOrEqualp = (result <= 0);
    return true;
}

struct SortComparatorStrings
{
    JSContext   *const cx;

    SortComparatorStrings(JSContext *cx)
      : cx(cx) {}

    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp) {
        return CompareStringValues(cx, a, b, lessOrEqualp);
    }
};

struct SortComparatorLexicographicInt32
{
    JSContext   *const cx;

    SortComparatorLexicographicInt32(JSContext *cx)
      : cx(cx) {}

    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp) {
        return CompareLexicographicInt32(cx, a, b, lessOrEqualp);
    }
};

struct StringifiedElement
{
    size_t charsBegin;
    size_t charsEnd;
    size_t elementIndex;
};

struct SortComparatorStringifiedElements
{
    JSContext           *const cx;
    const StringBuffer  &sb;

    SortComparatorStringifiedElements(JSContext *cx, const StringBuffer &sb)
      : cx(cx), sb(sb) {}

    bool operator()(const StringifiedElement &a, const StringifiedElement &b, bool *lessOrEqualp) {
        return CompareSubStringValues(cx, sb.begin() + a.charsBegin, a.charsEnd - a.charsBegin,
                                      sb.begin() + b.charsBegin, b.charsEnd - b.charsBegin,
                                      lessOrEqualp);
    }
};

struct SortComparatorFunction
{
    JSContext          *const cx;
    const Value        &fval;
    InvokeArgsGuard    &ag;

    SortComparatorFunction(JSContext *cx, const Value &fval, InvokeArgsGuard &ag)
      : cx(cx), fval(fval), ag(ag) { }

    bool operator()(const Value &a, const Value &b, bool *lessOrEqualp);
};

bool
SortComparatorFunction::operator()(const Value &a, const Value &b, bool *lessOrEqualp)
{
    /*
     * array_sort deals with holes and undefs on its own and they should not
     * come here.
     */
    JS_ASSERT(!a.isMagic() && !a.isUndefined());
    JS_ASSERT(!a.isMagic() && !b.isUndefined());

    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return false;

    if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 2, &ag))
        return false;

    ag.setCallee(fval);
    ag.thisv() = UndefinedValue();
    ag[0] = a;
    ag[1] = b;

    if (!Invoke(cx, ag))
        return false;

    double cmp;
    if (!ToNumber(cx, ag.rval(), &cmp))
        return false;

    /*
     * XXX eport some kind of error here if cmp is NaN? ECMA talks about
     * 'consistent compare functions' that don't return NaN, but is silent
     * about what the result should be. So we currently ignore it.
     */
    *lessOrEqualp = (JSDOUBLE_IS_NaN(cmp) || cmp <= 0);
    return true;
}

} /* namespace anonymous */

JSBool
js::array_sort(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value fval;
    if (args.hasDefined(0)) {
        if (args[0].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_SORT_ARG);
            return false;
        }
        fval = args[0];     /* non-default compare function */
    } else {
        fval.setNull();
    }

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;
    if (len == 0) {
        args.rval().setObject(*obj);
        return true;
    }

    /*
     * We need a temporary array of 2 * len Value to hold the array elements
     * and the scratch space for merge sort. Check that its size does not
     * overflow size_t, which would allow for indexing beyond the end of the
     * malloc'd vector.
     */
#if JS_BITS_PER_WORD == 32
    if (size_t(len) > size_t(-1) / (2 * sizeof(Value))) {
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
     */
    size_t n, undefs;
    {
        AutoValueVector vec(cx);
        if (!vec.reserve(2 * size_t(len)))
            return false;

        /*
         * By ECMA 262, 15.4.4.11, a property that does not exist (which we
         * call a "hole") is always greater than an existing property with
         * value undefined and that is always greater than any other property.
         * Thus to sort holes and undefs we simply count them, sort the rest
         * of elements, append undefs after them and then make holes after
         * undefs.
         */
        undefs = 0;
        bool allStrings = true;
        bool allInts = true;
        for (uint32_t i = 0; i < len; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            /* Clear vec[newlen] before including it in the rooted set. */
            JSBool hole;
            Value v;
            if (!GetElement(cx, obj, i, &hole, &v))
                return false;
            if (hole)
                continue;
            if (v.isUndefined()) {
                ++undefs;
                continue;
            }
            vec.infallibleAppend(v);
            allStrings = allStrings && v.isString();
            allInts = allInts && v.isInt32();
        }

        n = vec.length();
        if (n == 0) {
            args.rval().setObject(*obj);
            return true; /* The array has only holes and undefs. */
        }

        JS_ALWAYS_TRUE(vec.resize(n * 2));

        /* Here len == n + undefs + number_of_holes. */
        Value *result = vec.begin();
        if (fval.isNull()) {
            /*
             * Sort using the default comparator converting all elements to
             * strings.
             */
            if (allStrings) {
                if (!MergeSort(vec.begin(), n, vec.begin() + n, SortComparatorStrings(cx)))
                    return false;
            } else if (allInts) {
                if (!MergeSort(vec.begin(), n, vec.begin() + n,
                               SortComparatorLexicographicInt32(cx))) {
                    return false;
                }
            } else {
                /*
                 * Convert all elements to a jschar array in StringBuffer.
                 * Store the index and length of each stringified element with
                 * the corresponding index of the element in the array. Sort
                 * the stringified elements and with this result order the
                 * original array.
                 */
                StringBuffer sb(cx);
                Vector<StringifiedElement, 0, TempAllocPolicy> strElements(cx);
                if (!strElements.reserve(2 * n))
                    return false;

                int cursor = 0;
                for (size_t i = 0; i < n; i++) {
                    if (!JS_CHECK_OPERATION_LIMIT(cx))
                        return false;

                    if (!ValueToStringBuffer(cx, vec[i], sb))
                        return false;

                    StringifiedElement el = { cursor, sb.length(), i };
                    strElements.infallibleAppend(el);
                    cursor = sb.length();
                }

                /* Resize strElements so we can perform the sorting */
                JS_ALWAYS_TRUE(strElements.resize(2 * n));

                if (!MergeSort(strElements.begin(), n, strElements.begin() + n,
                               SortComparatorStringifiedElements(cx, sb))) {
                    return false;
                }

                /* Order vec[n:2n-1] using strElements.index */
                for (size_t i = 0; i < n; i ++)
                    vec[n + i] = vec[strElements[i].elementIndex];

                result = vec.begin() + n;
            }
        } else {
            InvokeArgsGuard args;
            if (!MergeSort(vec.begin(), n, vec.begin() + n,
                           SortComparatorFunction(cx, fval, args))) {
                return false;
            }
        }

        if (!InitArrayElements(cx, obj, 0, uint32_t(n), result, DontUpdateTypes))
            return false;
    }

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        if (!JS_CHECK_OPERATION_LIMIT(cx) || !SetArrayElement(cx, obj, n++, UndefinedValue()))
            return false;
    }

    /* Re-create any holes that sorted to the end of the array. */
    while (len > n) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) || DeleteArrayElement(cx, obj, --len, true) < 0)
            return false;
    }
    args.rval().setObject(*obj);
    return true;
}

/*
 * Perl-inspired push, pop, shift, unshift, and splice methods.
 */
static bool
array_push_slowly(JSContext *cx, JSObject *obj, CallArgs &args)
{
    uint32_t length;

    if (!js_GetLengthProperty(cx, obj, &length))
        return false;
    if (!InitArrayElements(cx, obj, length, args.length(), args.array(), UpdateTypes))
        return false;

    /* Per ECMA-262, return the new array length. */
    double newlength = length + double(args.length());
    args.rval().setNumber(newlength);
    return js_SetLengthProperty(cx, obj, newlength);
}

static bool
array_push1_dense(JSContext* cx, JSObject* obj, CallArgs &args)
{
    JS_ASSERT(args.length() == 1);

    uint32_t length = obj->getArrayLength();
    JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, length, 1);
    if (result != JSObject::ED_OK) {
        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!obj->makeDenseArraySlow(cx))
            return false;
        return array_push_slowly(cx, obj, args);
    }

    obj->setDenseArrayLength(length + 1);
    obj->setDenseArrayElementWithType(cx, length, args[0]);
    args.rval().setNumber(obj->getArrayLength());
    return true;
}

JS_ALWAYS_INLINE JSBool
NewbornArrayPushImpl(JSContext *cx, JSObject *obj, const Value &v)
{
    JS_ASSERT(!v.isMagic());

    uint32_t length = obj->getArrayLength();
    if (obj->isSlowArray()) {
        /* This can happen in one evil case. See bug 630377. */
        jsid id;
        return IndexToId(cx, length, &id) &&
               js_DefineProperty(cx, obj, id, &v, NULL, NULL, JSPROP_ENUMERATE);
    }

    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(length <= obj->getDenseArrayCapacity());

    if (!obj->ensureElements(cx, length + 1))
        return false;

    obj->setDenseArrayInitializedLength(length + 1);
    obj->setDenseArrayLength(length + 1);
    obj->initDenseArrayElementWithType(cx, length, v);
    return true;
}

JSBool
js_NewbornArrayPush(JSContext *cx, JSObject *obj, const Value &vp)
{
    return NewbornArrayPushImpl(cx, obj, vp);
}

JSBool
js::array_push(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Insist on one argument and obj of the expected class. */
    if (args.length() != 1 || !obj->isDenseArray())
        return array_push_slowly(cx, obj, args);

    return array_push1_dense(cx, obj, args);
}

static JSBool
array_pop_slowly(JSContext *cx, JSObject* obj, CallArgs &args)
{
    uint32_t index;
    if (!js_GetLengthProperty(cx, obj, &index))
        return false;

    if (index == 0) {
        args.rval().setUndefined();
        return js_SetLengthProperty(cx, obj, index);
    }

    index--;

    JSBool hole;
    Value elt;
    if (!GetElement(cx, obj, index, &hole, &elt))
        return false;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return false;

    args.rval() = elt;
    return js_SetLengthProperty(cx, obj, index);
}

static JSBool
array_pop_dense(JSContext *cx, JSObject* obj, CallArgs &args)
{
    uint32_t index = obj->getArrayLength();
    if (index == 0) {
        args.rval().setUndefined();
        return JS_TRUE;
    }

    index--;

    JSBool hole;
    Value elt;
    if (!GetElement(cx, obj, index, &hole, &elt))
        return JS_FALSE;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return JS_FALSE;
    if (obj->getDenseArrayInitializedLength() > index)
        obj->setDenseArrayInitializedLength(index);

    obj->setArrayLength(cx, index);

    args.rval() = elt;
    return JS_TRUE;
}

JSBool
js::array_pop(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (obj->isDenseArray())
        return array_pop_dense(cx, obj, args);
    return array_pop_slowly(cx, obj, args);
}

#ifdef JS_METHODJIT
void JS_FASTCALL
mjit::stubs::ArrayShift(VMFrame &f)
{
    JSObject *obj = &f.regs.sp[-1].toObject();
    JS_ASSERT(obj->isDenseArray());

    /*
     * At this point the length and initialized length have already been
     * decremented and the result fetched, so just shift the array elements
     * themselves.
     */
    uint32_t initlen = obj->getDenseArrayInitializedLength();
    obj->moveDenseArrayElementsUnbarriered(0, 1, initlen);
}
#endif /* JS_METHODJIT */

JSBool
js::array_shift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return JS_FALSE;

    uint32_t length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    if (length == 0) {
        args.rval().setUndefined();
    } else {
        length--;

        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            length < obj->getDenseArrayCapacity() &&
            0 < obj->getDenseArrayInitializedLength()) {
            args.rval() = obj->getDenseArrayElement(0);
            if (args.rval().isMagic(JS_ARRAY_HOLE))
                args.rval().setUndefined();
            obj->moveDenseArrayElements(0, 1, obj->getDenseArrayInitializedLength() - 1);
            obj->setDenseArrayInitializedLength(obj->getDenseArrayInitializedLength() - 1);
            obj->setArrayLength(cx, length);
            if (!js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(length)))
                return JS_FALSE;
            return JS_TRUE;
        }

        JSBool hole;
        if (!GetElement(cx, obj, 0u, &hole, &args.rval()))
            return JS_FALSE;

        /* Slide down the array above the first element. */
        AutoValueRooter tvr(cx);
        for (uint32_t i = 0; i < length; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, i + 1, &hole, tvr.addr()) ||
                !SetOrDeleteArrayElement(cx, obj, i, hole, tvr.value())) {
                return JS_FALSE;
            }
        }

        /* Delete the only or last element when it exists. */
        if (!hole && DeleteArrayElement(cx, obj, length, true) < 0)
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    uint32_t length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    double newlen = length;
    if (args.length() > 0) {
        /* Slide up the array to make room for all args at the bottom. */
        if (length > 0) {
            bool optimized = false;
            do {
                if (!obj->isDenseArray())
                    break;
                if (js_PrototypeHasIndexedProperties(cx, obj))
                    break;
                JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, length, args.length());
                if (result != JSObject::ED_OK) {
                    if (result == JSObject::ED_FAILED)
                        return false;
                    JS_ASSERT(result == JSObject::ED_SPARSE);
                    break;
                }
                obj->moveDenseArrayElements(args.length(), 0, length);
                for (uint32_t i = 0; i < args.length(); i++)
                    obj->setDenseArrayElement(i, MagicValue(JS_ARRAY_HOLE));
                optimized = true;
            } while (false);

            if (!optimized) {
                double last = length;
                double upperIndex = last + args.length();
                AutoValueRooter tvr(cx);
                do {
                    --last, --upperIndex;
                    JSBool hole;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                        !GetElement(cx, obj, last, &hole, tvr.addr()) ||
                        !SetOrDeleteArrayElement(cx, obj, upperIndex, hole, tvr.value())) {
                        return JS_FALSE;
                    }
                } while (last != 0);
            }
        }

        /* Copy from args to the bottom of the array. */
        if (!InitArrayElements(cx, obj, 0, args.length(), args.array(), UpdateTypes))
            return JS_FALSE;

        newlen += args.length();
    }
    if (!js_SetLengthProperty(cx, obj, newlen))
        return JS_FALSE;

    /* Follow Perl by returning the new array length. */
    args.rval().setNumber(newlen);
    return JS_TRUE;
}

static inline void
TryReuseArrayType(JSObject *obj, JSObject *nobj)
{
    /*
     * Try to change the type of a newly created array nobj to the same type
     * as obj. This can only be performed if the original object is an array
     * and has the same prototype.
     */
    JS_ASSERT(nobj->isDenseArray());
    JS_ASSERT(nobj->getProto()->hasNewType(nobj->type()));

    if (obj->isArray() && !obj->hasSingletonType() && obj->getProto() == nobj->getProto())
        nobj->setType(obj->type());
}

/*
 * Returns true if this is a dense array whose |count| properties starting from
 * |startingIndex| may be accessed (get, set, delete) directly through its
 * contiguous vector of elements without fear of getters, setters, etc. along
 * the prototype chain, or of enumerators requiring notification of
 * modifications.
 */
static inline bool
CanOptimizeForDenseStorage(JSObject *arr, uint32_t startingIndex, uint32_t count, JSContext *cx)
{
    /* If the desired properties overflow dense storage, we can't optimize. */
    if (UINT32_MAX - startingIndex < count)
        return false;

    /* There's no optimizing possible if it's not a dense array. */
    if (!arr->isDenseArray())
        return false;

    /*
     * Don't optimize if the array might be in the midst of iteration.  We
     * rely on this to be able to safely move dense array elements around with
     * just a memmove (see JSObject::moveDenseArrayElements), without worrying
     * about updating any in-progress enumerators for properties implicitly
     * deleted if a hole is moved from one location to another location not yet
     * visited.  See bug 690622.
     *
     * Another potential wrinkle: what if the enumeration is happening on an
     * object which merely has |arr| on its prototype chain?  It turns out this
     * case can't happen, because any dense array used as the prototype of
     * another object is first slowified, for type inference's sake.
     */
    if (JS_UNLIKELY(arr->getType(cx)->hasAllFlags(OBJECT_FLAG_ITERATED)))
        return false;

    /* Now just watch out for getters and setters along the prototype chain. */
    return !js_PrototypeHasIndexedProperties(cx, arr) &&
           startingIndex + count <= arr->getDenseArrayInitializedLength();
}

/* ES5 15.4.4.12. */
static JSBool
array_splice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Steps 3-4. */
    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 5. */
    double relativeStart;
    if (!ToInteger(cx, argc >= 1 ? args[0] : UndefinedValue(), &relativeStart))
        return false;

    /* Step 6. */
    uint32_t actualStart;
    if (relativeStart < 0)
        actualStart = JS_MAX(len + relativeStart, 0);
    else
        actualStart = JS_MIN(relativeStart, len);

    /* Step 7. */
    uint32_t actualDeleteCount;
    if (argc != 1) {
        double deleteCountDouble;
        if (!ToInteger(cx, argc >= 2 ? args[1] : Int32Value(0), &deleteCountDouble))
            return false;
        actualDeleteCount = JS_MIN(JS_MAX(deleteCountDouble, 0), len - actualStart);
    } else {
        /*
         * Non-standard: if start was specified but deleteCount was omitted,
         * delete to the end of the array.  See bug 668024 for discussion.
         */
        actualDeleteCount = len - actualStart;
    }

    JS_ASSERT(len - actualStart >= actualDeleteCount);

    /* Steps 2, 8-9. */
    JSObject *arr;
    if (CanOptimizeForDenseStorage(obj, actualStart, actualDeleteCount, cx)) {
        arr = NewDenseCopiedArray(cx, actualDeleteCount,
                                  obj->getDenseArrayElements() + actualStart);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);
    } else {
        arr = NewDenseAllocatedArray(cx, actualDeleteCount);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);

        for (uint32_t k = 0; k < actualDeleteCount; k++) {
            JSBool hole;
            Value fromValue;
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, actualStart + k, &hole, &fromValue) ||
                (!hole && !arr->defineElement(cx, k, fromValue)))
            {
                return false;
            }
        }
    }

    /* Step 11. */
    uint32_t itemCount = (argc >= 2) ? (argc - 2) : 0;

    if (itemCount < actualDeleteCount) {
        /* Step 12: the array is being shrunk. */
        uint32_t sourceIndex = actualStart + actualDeleteCount;
        uint32_t targetIndex = actualStart + itemCount;
        uint32_t finalLength = len - actualDeleteCount + itemCount;

        if (CanOptimizeForDenseStorage(obj, 0, len, cx)) {
            /* Steps 12(a)-(b). */
            obj->moveDenseArrayElements(targetIndex, sourceIndex, len - sourceIndex);

            /*
             * Update the initialized length. Do so before shrinking so that we
             * can apply the write barrier to the old slots.
             */
            if (cx->typeInferenceEnabled())
                obj->setDenseArrayInitializedLength(finalLength);

            /* Steps 12(c)-(d). */
            obj->shrinkElements(cx, finalLength);

            /* Fix running enumerators for the deleted items. */
            if (!js_SuppressDeletedElements(cx, obj, finalLength, len))
                return false;
        } else {
            /*
             * This is all very slow if the length is very large. We don't yet
             * have the ability to iterate in sorted order, so we just do the
             * pessimistic thing and let JS_CHECK_OPERATION_LIMIT handle the
             * fallout.
             */

            /* Steps 12(a)-(b). */
            for (uint32_t from = sourceIndex, to = targetIndex; from < len; from++, to++) {
                JSBool hole;
                Value fromValue;
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetElement(cx, obj, from, &hole, &fromValue) ||
                    !SetOrDeleteArrayElement(cx, obj, to, hole, fromValue))
                {
                    return false;
                }
            }

            /* Steps 12(c)-(d). */
            for (uint32_t k = len; k > finalLength; k--) {
                if (DeleteArrayElement(cx, obj, k - 1, true) < 0)
                    return false;
            }
        }
    } else if (itemCount > actualDeleteCount) {
        /* Step 13. */

        /*
         * Optimize only if the array is already dense and we can extend it to
         * its new length.
         */
        if (obj->isDenseArray()) {
            JSObject::EnsureDenseResult res =
                obj->ensureDenseArrayElements(cx, obj->getArrayLength(),
                                              itemCount - actualDeleteCount);
            if (res == JSObject::ED_FAILED)
                return false;

            if (res == JSObject::ED_SPARSE) {
                if (!obj->makeDenseArraySlow(cx))
                    return false;
            } else {
                JS_ASSERT(res == JSObject::ED_OK);
            }
        }

        if (CanOptimizeForDenseStorage(obj, len, itemCount - actualDeleteCount, cx)) {
            obj->moveDenseArrayElements(actualStart + itemCount,
                                        actualStart + actualDeleteCount,
                                        len - (actualStart + actualDeleteCount));

            if (cx->typeInferenceEnabled())
                obj->setDenseArrayInitializedLength(len + itemCount - actualDeleteCount);
        } else {
            for (double k = len - actualDeleteCount; k > actualStart; k--) {
                double from = k + actualDeleteCount - 1;
                double to = k + itemCount - 1;

                JSBool hole;
                Value fromValue;
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetElement(cx, obj, from, &hole, &fromValue) ||
                    !SetOrDeleteArrayElement(cx, obj, to, hole, fromValue))
                {
                    return false;
                }
            }
        }
    }

    /* Step 10. */
    Value *items = args.array() + 2;

    /* Steps 14-15. */
    for (uint32_t k = actualStart, i = 0; i < itemCount; i++, k++) {
        if (!SetArrayElement(cx, obj, k, items[i]))
            return false;
    }

    /* Step 16. */
    double finalLength = double(len) - actualDeleteCount + itemCount;
    if (!js_SetLengthProperty(cx, obj, finalLength))
        return false;

    /* Step 17. */
    args.rval().setObject(*arr);
    return true;
}

#ifdef JS_METHODJIT
void JS_FASTCALL
mjit::stubs::ArrayConcatTwoArrays(VMFrame &f)
{
    JSObject *result = &f.regs.sp[-3].toObject();
    JSObject *obj1 = &f.regs.sp[-2].toObject();
    JSObject *obj2 = &f.regs.sp[-1].toObject();

    JS_ASSERT(result->isDenseArray() && obj1->isDenseArray() && obj2->isDenseArray());

    uint32_t initlen1 = obj1->getDenseArrayInitializedLength();
    JS_ASSERT(initlen1 == obj1->getArrayLength());

    uint32_t initlen2 = obj2->getDenseArrayInitializedLength();
    JS_ASSERT(initlen2 == obj2->getArrayLength());

    /* No overflow here due to nslots limit. */
    uint32_t len = initlen1 + initlen2;

    if (!result->ensureElements(f.cx, len))
        THROW();

    JS_ASSERT(!result->getDenseArrayInitializedLength());
    result->setDenseArrayInitializedLength(len);

    result->initDenseArrayElements(0, obj1->getDenseArrayElements(), initlen1);
    result->initDenseArrayElements(initlen1, obj2->getDenseArrayElements(), initlen2);

    result->setDenseArrayLength(len);
}
#endif /* JS_METHODJIT */

/*
 * Python-esque sequence operations.
 */
JSBool
js::array_concat(JSContext *cx, unsigned argc, Value *vp)
{
    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    Value *p = JS_ARGV(cx, vp) - 1;

    /* Create a new Array object and root it using *vp. */
    JSObject *aobj = ToObject(cx, &vp[1]);
    if (!aobj)
        return false;

    JSObject *nobj;
    uint32_t length;
    if (aobj->isDenseArray()) {
        length = aobj->getArrayLength();
        const Value *vector = aobj->getDenseArrayElements();
        uint32_t initlen = aobj->getDenseArrayInitializedLength();
        nobj = NewDenseCopiedArray(cx, initlen, vector);
        if (!nobj)
            return JS_FALSE;
        TryReuseArrayType(aobj, nobj);
        nobj->setArrayLength(cx, length);
        vp->setObject(*nobj);
        if (argc == 0)
            return JS_TRUE;
        argc--;
        p++;
    } else {
        nobj = NewDenseEmptyArray(cx);
        if (!nobj)
            return JS_FALSE;
        vp->setObject(*nobj);
        length = 0;
    }

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    for (unsigned i = 0; i <= argc; i++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        const Value &v = p[i];
        if (v.isObject()) {
            JSObject &obj = v.toObject();
            if (ObjectClassIs(obj, ESClass_Array, cx)) {
                uint32_t alength;
                if (!js_GetLengthProperty(cx, &obj, &alength))
                    return false;
                for (uint32_t slot = 0; slot < alength; slot++) {
                    JSBool hole;
                    Value tmp;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) || !GetElement(cx, &obj, slot, &hole, &tmp))
                        return false;

                    /*
                     * Per ECMA 262, 15.4.4.4, step 9, ignore nonexistent
                     * properties.
                     */
                    if (!hole && !SetArrayElement(cx, nobj, length + slot, tmp))
                        return false;
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
array_slice(JSContext *cx, unsigned argc, Value *vp)
{
    JSObject *nobj;
    uint32_t length, begin, end, slot;
    JSBool hole;

    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    begin = 0;
    end = length;

    if (args.length() > 0) {
        double d;
        if (!ToInteger(cx, args[0], &d))
            return false;
        if (d < 0) {
            d += length;
            if (d < 0)
                d = 0;
        } else if (d > length) {
            d = length;
        }
        begin = (uint32_t)d;

        if (args.hasDefined(1)) {
            if (!ToInteger(cx, args[1], &d))
                return false;
            if (d < 0) {
                d += length;
                if (d < 0)
                    d = 0;
            } else if (d > length) {
                d = length;
            }
            end = (uint32_t)d;
        }
    }

    if (begin > end)
        begin = end;

    if (obj->isDenseArray() && end <= obj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(cx, obj)) {
        nobj = NewDenseCopiedArray(cx, end - begin, obj->getDenseArrayElements() + begin);
        if (!nobj)
            return JS_FALSE;
        TryReuseArrayType(obj, nobj);
        args.rval().setObject(*nobj);
        return JS_TRUE;
    }

    nobj = NewDenseAllocatedArray(cx, end - begin);
    if (!nobj)
        return JS_FALSE;
    TryReuseArrayType(obj, nobj);

    AutoValueRooter tvr(cx);
    for (slot = begin; slot < end; slot++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, slot, &hole, tvr.addr())) {
            return JS_FALSE;
        }
        if (!hole && !SetArrayElement(cx, nobj, slot - begin, tvr.value()))
            return JS_FALSE;
    }

    args.rval().setObject(*nobj);
    return JS_TRUE;
}

enum IndexOfKind {
    IndexOf,
    LastIndexOf
};

static JSBool
array_indexOfHelper(JSContext *cx, IndexOfKind mode, CallArgs &args)
{
    uint32_t length, i, stop;
    Value tosearch;
    int direction;
    JSBool hole;

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0)
        goto not_found;

    if (args.length() <= 1) {
        i = (mode == LastIndexOf) ? length - 1 : 0;
        tosearch = (args.length() != 0) ? args[0] : UndefinedValue();
    } else {
        double start;

        tosearch = args[0];
        if (!ToInteger(cx, args[1], &start))
            return false;
        if (start < 0) {
            start += length;
            if (start < 0) {
                if (mode == LastIndexOf)
                    goto not_found;
                i = 0;
            } else {
                i = (uint32_t)start;
            }
        } else if (start >= length) {
            if (mode == IndexOf)
                goto not_found;
            i = length - 1;
        } else {
            i = (uint32_t)start;
        }
    }

    if (mode == LastIndexOf) {
        stop = 0;
        direction = -1;
    } else {
        stop = length - 1;
        direction = 1;
    }

    for (;;) {
        Value elt;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, (uint32_t)i, &hole, &elt)) {
            return JS_FALSE;
        }
        if (!hole) {
            bool equal;
            if (!StrictlyEqual(cx, elt, tosearch, &equal))
                return false;
            if (equal) {
                args.rval().setNumber(i);
                return true;
            }
        }
        if (i == stop)
            goto not_found;
        i += direction;
    }

  not_found:
    args.rval().setInt32(-1);
    return JS_TRUE;
}

static JSBool
array_indexOf(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_indexOfHelper(cx, IndexOf, args);
}

static JSBool
array_lastIndexOf(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_indexOfHelper(cx, LastIndexOf, args);
}

/* ECMA 15.4.4.16-15.4.4.18. */
class ArrayForEachBehavior
{
  public:
    static bool shouldExit(Value &callval, Value *rval) { return false; }
    static Value lateExitValue() { return UndefinedValue(); }
};

class ArrayEveryBehavior
{
  public:
    static bool shouldExit(Value &callval, Value *rval)
    {
        if (!js_ValueToBoolean(callval)) {
            *rval = BooleanValue(false);
            return true;
        }
        return false;
    }
    static Value lateExitValue() { return BooleanValue(true); }
};

class ArraySomeBehavior
{
  public:
    static bool shouldExit(Value &callval, Value *rval)
    {
        if (js_ValueToBoolean(callval)) {
            *rval = BooleanValue(true);
            return true;
        }
        return false;
    }
    static Value lateExitValue() { return BooleanValue(false); }
};

template <class Behavior>
static inline bool
array_readonlyCommon(JSContext *cx, CallArgs &args)
{
    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    JSObject *callable = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return false;

    /* Step 5. */
    Value thisv = args.length() >= 2 ? args[1] : UndefinedValue();

    /* Step 6. */
    uint32_t k = 0;

    /* Step 7. */
    InvokeArgsGuard ag;
    while (k < len) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        Value kValue;
        JSBool kNotPresent;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.thisv() = thisv;
            ag[0] = kValue;
            ag[1] = NumberValue(k);
            ag[2] = ObjectValue(*obj);
            if (!Invoke(cx, ag))
                return false;

            if (Behavior::shouldExit(ag.rval(), &args.rval()))
                return true;
        }

        /* Step d. */
        k++;
    }

    /* Step 8. */
    args.rval() = Behavior::lateExitValue();
    return true;
 }

/* ES5 15.4.4.16. */
static JSBool
array_every(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_readonlyCommon<ArrayEveryBehavior>(cx, args);
}

/* ES5 15.4.4.17. */
static JSBool
array_some(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_readonlyCommon<ArraySomeBehavior>(cx, args);
}

/* ES5 15.4.4.18. */
static JSBool
array_forEach(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_readonlyCommon<ArrayForEachBehavior>(cx, args);
}

/* ES5 15.4.4.19. */
static JSBool
array_map(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    JSObject *callable = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return false;

    /* Step 5. */
    Value thisv = args.length() >= 2 ? args[1] : UndefinedValue();

    /* Step 6. */
    JSObject *arr = NewDenseAllocatedArray(cx, len);
    if (!arr)
        return false;
    TypeObject *newtype = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!newtype)
        return false;
    arr->setType(newtype);

    /* Step 7. */
    uint32_t k = 0;

    /* Step 8. */
    InvokeArgsGuard ag;
    while (k < len) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        JSBool kNotPresent;
        Value kValue;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.thisv() = thisv;
            ag[0] = kValue;
            ag[1] = NumberValue(k);
            ag[2] = ObjectValue(*obj);
            if (!Invoke(cx, ag))
                return false;
            if(!SetArrayElement(cx, arr, k, ag.rval()))
                return false;
        }

        /* Step d. */
        k++;
    }

    /* Step 9. */
    args.rval().setObject(*arr);
    return true;
}

/* ES5 15.4.4.20. */
static JSBool
array_filter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    JSObject *callable = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return false;

    /* Step 5. */
    Value thisv = args.length() >= 2 ? args[1] : UndefinedValue();

    /* Step 6. */
    JSObject *arr = NewDenseAllocatedArray(cx, 0);
    if (!arr)
        return false;
    TypeObject *newtype = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!newtype)
        return false;
    arr->setType(newtype);

    /* Step 7. */
    uint32_t k = 0;

    /* Step 8. */
    uint32_t to = 0;

    /* Step 9. */
    InvokeArgsGuard ag;
    while (k < len) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        JSBool kNotPresent;
        Value kValue;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.thisv() = thisv;
            ag[0] = kValue;
            ag[1] = NumberValue(k);
            ag[2] = ObjectValue(*obj);
            if (!Invoke(cx, ag))
                return false;

            if (js_ValueToBoolean(ag.rval())) {
                if(!SetArrayElement(cx, arr, to, kValue))
                    return false;
                to++;
            }
        }

        /* Step d. */
        k++;
    }

    /* Step 10. */
    args.rval().setObject(*arr);
    return true;
}

/* ES5 15.4.4.21-15.4.4.22. */
class ArrayReduceBehavior
{
  public:
    static void initialize(uint32_t len, uint32_t *start, uint32_t *end, int32_t *step)
    {
        *start = 0;
        *step = 1;
        *end = len;
    }
};

class ArrayReduceRightBehavior
{
  public:
    static void initialize(uint32_t len, uint32_t *start, uint32_t *end, int32_t *step)
    {
        *start = len - 1;
        *step = -1;
        /*
         * We rely on (well defined) unsigned integer underflow to check our
         * end condition after visiting the full range (including 0).
         */
        *end = UINT32_MAX;
    }
};

template<class Behavior>
static inline bool
array_reduceCommon(JSContext *cx, CallArgs &args)
{
    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    JSObject *callable = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return false;

    /* Step 5. */
    if (len == 0 && args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_EMPTY_ARRAY_REDUCE);
        return false;
    }

    /* Step 6. */
    uint32_t k, end;
    int32_t step;
    Behavior::initialize(len, &k, &end, &step);

    /* Step 7-8. */
    Value accumulator;
    if (args.length() >= 2) {
        accumulator = args[1];
    } else {
        JSBool kNotPresent = true;
        while (kNotPresent && k != end) {
            if (!GetElement(cx, obj, k, &kNotPresent, &accumulator))
                return false;
            k += step;
        }
        if (kNotPresent) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_EMPTY_ARRAY_REDUCE);
            return false;
        }
    }

    /* Step 9. */
    InvokeArgsGuard ag;
    while (k != end) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        JSBool kNotPresent;
        Value kValue;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 4, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.thisv() = UndefinedValue();
            ag[0] = accumulator;
            ag[1] = kValue;
            ag[2] = NumberValue(k);
            ag[3] = ObjectValue(*obj);
            if (!Invoke(cx, ag))
                return false;
            accumulator = ag.rval();
        }

        /* Step d. */
        k += step;
    }

    /* Step 10. */
    args.rval() = accumulator;
    return true;
}

/* ES5 15.4.4.21. */
static JSBool
array_reduce(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_reduceCommon<ArrayReduceBehavior>(cx, args);
}

/* ES5 15.4.4.22. */
static JSBool
array_reduceRight(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_reduceCommon<ArrayReduceRightBehavior>(cx, args);
}

static JSBool
array_isArray(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool isArray = args.length() > 0 && IsObjectWithClass(args[0], ESClass_Array, cx);
    args.rval().setBoolean(isArray);
    return true;
}

#define GENERIC JSFUN_GENERIC_NATIVE

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,      array_toSource,     0,0),
#endif
    JS_FN(js_toString_str,      array_toString,     0,0),
    JS_FN(js_toLocaleString_str,array_toLocaleString,0,0),

    /* Perl-ish methods. */
    JS_FN("join",               array_join,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("reverse",            array_reverse,      0,JSFUN_GENERIC_NATIVE),
    JS_FN("sort",               array_sort,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("push",               array_push,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("pop",                array_pop,          0,JSFUN_GENERIC_NATIVE),
    JS_FN("shift",              array_shift,        0,JSFUN_GENERIC_NATIVE),
    JS_FN("unshift",            array_unshift,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("splice",             array_splice,       2,JSFUN_GENERIC_NATIVE),

    /* Pythonic sequence methods. */
    JS_FN("concat",             array_concat,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("slice",              array_slice,        2,JSFUN_GENERIC_NATIVE),

    JS_FN("indexOf",            array_indexOf,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("lastIndexOf",        array_lastIndexOf,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("forEach",            array_forEach,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("map",                array_map,          1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduce",             array_reduce,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduceRight",        array_reduceRight,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("filter",             array_filter,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("some",               array_some,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("every",              array_every,        1,JSFUN_GENERIC_NATIVE),

    JS_FS_END
};

static JSFunctionSpec array_static_methods[] = {
    JS_FN("isArray",            array_isArray,      1,0),
    JS_FS_END
};

/* ES5 15.4.2 */
JSBool
js_Array(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    TypeObject *type = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!type)
        return JS_FALSE;

    if (args.length() != 1 || !args[0].isNumber()) {
        if (!InitArrayTypes(cx, type, args.array(), args.length()))
            return false;
        JSObject *obj = (args.length() == 0)
                        ? NewDenseEmptyArray(cx)
                        : NewDenseCopiedArray(cx, args.length(), args.array());
        if (!obj)
            return false;
        obj->setType(type);
        args.rval().setObject(*obj);
        return true;
    }

    uint32_t length;
    if (args[0].isInt32()) {
        int32_t i = args[0].toInt32();
        if (i < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
        length = uint32_t(i);
    } else {
        double d = args[0].toDouble();
        length = js_DoubleToECMAUint32(d);
        if (d != double(length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
    }

    JSObject *obj = NewDenseUnallocatedArray(cx, length);
    if (!obj)
        return false;

    obj->setType(type);

    /* If the length calculation overflowed, make sure that is marked for the new type. */
    if (obj->getArrayLength() > INT32_MAX)
        obj->setArrayLength(cx, obj->getArrayLength());

    args.rval().setObject(*obj);
    return true;
}

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    RootedVar<GlobalObject*> global(cx);
    global = &obj->asGlobal();

    RootedVarObject arrayProto(cx);
    arrayProto = global->createBlankPrototype(cx, &SlowArrayClass);
    if (!arrayProto || !AddLengthProperty(cx, arrayProto))
        return NULL;
    arrayProto->setArrayLength(cx, 0);

    RootedVarFunction ctor(cx);
    ctor = global->createConstructor(cx, js_Array, &ArrayClass,
                                     CLASS_ATOM(cx, Array), 1);
    if (!ctor)
        return NULL;

    /*
     * The default 'new' type of Array.prototype is required by type inference
     * to have unknown properties, to simplify handling of e.g. heterogenous
     * arrays in JSON and script literals and allows setDenseArrayElement to
     * be used without updating the indexed type set for such default arrays.
     */
    if (!arrayProto->setNewTypeUnknown(cx))
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, arrayProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, arrayProto, NULL, array_methods) ||
        !DefinePropertiesAndBrand(cx, ctor, NULL, array_static_methods))
    {
        return NULL;
    }

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Array, ctor, arrayProto))
        return NULL;

    return arrayProto;
}

/*
 * Array allocation functions.
 */
namespace js {

static inline bool
EnsureNewArrayElements(JSContext *cx, JSObject *obj, uint32_t length)
{
    /*
     * If ensureElements creates dynamically allocated slots, then having
     * fixedSlots is a waste.
     */
    DebugOnly<uint32_t> cap = obj->getDenseArrayCapacity();

    if (!obj->ensureElements(cx, length))
        return false;

    JS_ASSERT_IF(cap, !obj->hasDynamicElements());

    return true;
}

template<bool allocateCapacity>
static JS_ALWAYS_INLINE JSObject *
NewArray(JSContext *cx, uint32_t length, JSObject *proto)
{
    gc::AllocKind kind = GuessArrayGCKind(length);

#ifdef JS_THREADSAFE
    JS_ASSERT(CanBeFinalizedInBackground(kind, &ArrayClass));
    kind = GetBackgroundAllocKind(kind);
#endif

    GlobalObject *parent = GetCurrentGlobal(cx);

    NewObjectCache &cache = cx->compartment->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (cache.lookupGlobal(&ArrayClass, parent, kind, &entry)) {
        JSObject *obj = cache.newObjectFromHit(cx, entry);
        if (!obj)
            return NULL;
        /* Fixup the elements pointer and length, which may be incorrect. */
        obj->setFixedElements();
        obj->setArrayLength(cx, length);
        if (allocateCapacity && !EnsureNewArrayElements(cx, obj, length))
            return NULL;
        return obj;
    }

    Root<GlobalObject*> parentRoot(cx, &parent);

    if (!proto && !FindProto(cx, &ArrayClass, parentRoot, &proto))
        return NULL;

    RootObject protoRoot(cx, &proto);
    RootedVarTypeObject type(cx);

    type = proto->getNewType(cx);
    if (!type)
        return NULL;

    /*
     * Get a shape with zero fixed slots, regardless of the size class.
     * See JSObject::createDenseArray.
     */
    RootedVarShape shape(cx);
    shape = EmptyShape::getInitialShape(cx, &ArrayClass, proto,
                                        parent, gc::FINALIZE_OBJECT0);
    if (!shape)
        return NULL;

    JSObject* obj = JSObject::createDenseArray(cx, kind, shape, type, length);
    if (!obj)
        return NULL;

    if (entry != -1)
        cache.fillGlobal(entry, &ArrayClass, parent, kind, obj);

    if (allocateCapacity && !EnsureNewArrayElements(cx, obj, length))
        return NULL;

    Probes::createObject(cx, obj);
    return obj;
}

JSObject * JS_FASTCALL
NewDenseEmptyArray(JSContext *cx, JSObject *proto)
{
    return NewArray<false>(cx, 0, proto);
}

JSObject * JS_FASTCALL
NewDenseAllocatedArray(JSContext *cx, uint32_t length, JSObject *proto)
{
    return NewArray<true>(cx, length, proto);
}

JSObject * JS_FASTCALL
NewDenseAllocatedEmptyArray(JSContext *cx, uint32_t length, JSObject *proto)
{
    return NewArray<true>(cx, length, proto);
}

JSObject * JS_FASTCALL
NewDenseUnallocatedArray(JSContext *cx, uint32_t length, JSObject *proto)
{
    return NewArray<false>(cx, length, proto);
}

#ifdef JS_METHODJIT
JSObject * JS_FASTCALL
mjit::stubs::NewDenseUnallocatedArray(VMFrame &f, uint32_t length)
{
    JSObject *proto = (JSObject *) f.scratch;
    JSObject *obj = NewArray<false>(f.cx, length, proto);
    if (!obj) {
        js_ReportOutOfMemory(f.cx);
        THROWV(NULL);
    }
    return obj;
}
#endif

JSObject *
NewDenseCopiedArray(JSContext *cx, uint32_t length, const Value *vp, JSObject *proto /* = NULL */)
{
    JSObject* obj = NewArray<true>(cx, length, proto);
    if (!obj)
        return NULL;

    JS_ASSERT(obj->getDenseArrayCapacity() >= length);

    obj->setDenseArrayInitializedLength(vp ? length : 0);

    if (vp)
        obj->initDenseArrayElements(0, vp, length);

    return obj;
}

JSObject *
NewSlowEmptyArray(JSContext *cx)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &SlowArrayClass);
    if (!obj || !AddLengthProperty(cx, obj))
        return NULL;

    obj->setArrayLength(cx, 0);
    return obj;
}

}


#ifdef DEBUG
JSBool
js_ArrayInfo(JSContext *cx, unsigned argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *array;

    for (unsigned i = 0; i < args.length(); i++) {
        Value arg = args[i];

        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, NULL);
        if (!bytes)
            return JS_FALSE;
        if (arg.isPrimitive() ||
            !(array = arg.toObjectOrNull())->isArray()) {
            fprintf(stderr, "%s: not array\n", bytes);
            cx->free_(bytes);
            continue;
        }
        fprintf(stderr, "%s: %s (len %u", bytes,
                array->isDenseArray() ? "dense" : "sparse",
                array->getArrayLength());
        if (array->isDenseArray()) {
            fprintf(stderr, ", capacity %u",
                    array->getDenseArrayCapacity());
        }
        fputs(")\n", stderr);
        cx->free_(bytes);
    }

    args.rval().setUndefined();
    return true;
}
#endif
