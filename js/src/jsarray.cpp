/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS array class.
 *
 * Array objects begin as "dense" arrays, optimized for index-only property
 * access over a vector of slots with high load factor.  Array methods
 * optimize for denseness by testing that the object's class is
 * &ArrayClass, and can then directly manipulate the slots for efficiency.
 *
 * We track these pieces of metadata for arrays in dense mode:
 *  - The array's length property as a uint32_t, accessible with
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

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/Util.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

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
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jswrapper.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/StubCalls.h"
#include "methodjit/StubCalls-inl.h"

#include "gc/Marking.h"
#include "vm/ArgumentsObject.h"
#include "vm/ForkJoin.h"
#include "vm/NumericConversions.h"
#include "vm/StringBuffer.h"
#include "vm/ThreadPool.h"

#include "ds/Sort.h"

#include "jsarrayinlines.h"
#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsstrinlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/ObjectImpl-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::PointerRangeSize;

JSBool
js::GetLengthProperty(JSContext *cx, HandleObject obj, uint32_t *lengthp)
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

    RootedValue value(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().length, &value))
        return false;

    if (value.isInt32()) {
        *lengthp = uint32_t(value.toInt32()); /* uint32_t cast does ToUint32_t */
        return true;
    }

    return ToUint32(cx, value, (uint32_t *)lengthp);
}

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
js::StringIsArrayIndex(JSLinearString *str, uint32_t *indexp)
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

UnrootedShape
js::GetDenseArrayShape(JSContext *cx, HandleObject globalObj)
{
    JS_ASSERT(globalObj);

    JSObject *proto = globalObj->global().getOrCreateArrayPrototype(cx);
    if (!proto)
        return UnrootedShape(NULL);

    return EmptyShape::getInitialShape(cx, &ArrayClass, proto, proto->getParent(),
                                       gc::FINALIZE_OBJECT0);
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

    jsid id;
    if (!IndexToId(cx, i, &id))
        return false;

    UnrootedShape shape = nativeLookup(cx, id);
    if (!shape || !shape->isDataDescriptor())
        vp->setMagic(JS_ARRAY_HOLE);
    else
        *vp = getSlot(shape->slot());
    return true;
}

bool
DoubleIndexToId(JSContext *cx, double index, jsid *id)
{
    if (index == uint32_t(index))
        return IndexToId(cx, uint32_t(index), id);

    return ValueToId(cx, DoubleValue(index), id);
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
static inline bool
DoGetElement(JSContext *cx, HandleObject obj, double index, JSBool *hole, MutableHandleValue vp)
{
    RootedId id(cx);

    if (!DoubleIndexToId(cx, index, id.address()))
        return false;

    RootedObject obj2(cx);
    RootedShape prop(cx);
    if (!JSObject::lookupGeneric(cx, obj, id, &obj2, &prop))
        return false;

    if (!prop) {
        vp.setUndefined();
        *hole = true;
    } else {
        if (!JSObject::getGeneric(cx, obj, obj, id, vp))
            return false;
        *hole = false;
    }
    return true;
}

static inline bool
DoGetElement(JSContext *cx, HandleObject obj, uint32_t index, JSBool *hole, MutableHandleValue vp)
{
    bool present;
    if (!JSObject::getElementIfPresent(cx, obj, obj, index, vp, &present))
        return false;

    *hole = !present;
    if (*hole)
        vp.setUndefined();

    return true;
}

template<typename IndexType>
static void
AssertGreaterThanZero(IndexType index)
{
    JS_ASSERT(index >= 0);
    JS_ASSERT(index == floor(index));
}

template<>
void
AssertGreaterThanZero(uint32_t index)
{
}

template<typename IndexType>
static JSBool
GetElement(JSContext *cx, HandleObject obj, IndexType index, JSBool *hole, MutableHandleValue vp)
{
    AssertGreaterThanZero(index);
    if (obj->isDenseArray() && index < obj->getDenseArrayInitializedLength()) {
        vp.set(obj->getDenseArrayElement(uint32_t(index)));
        if (!vp.isMagic(JS_ARRAY_HOLE)) {
            *hole = JS_FALSE;
            return JS_TRUE;
        }
    }
    if (obj->isArguments()) {
        if (obj->asArguments().maybeGetElement(uint32_t(index), vp)) {
            *hole = JS_FALSE;
            return true;
        }
    }

    return DoGetElement(cx, obj, index, hole, vp);
}

static bool
GetElementsSlow(JSContext *cx, HandleObject aobj, uint32_t length, Value *vp)
{
    for (uint32_t i = 0; i < length; i++) {
        if (!JSObject::getElement(cx, aobj, aobj, i, MutableHandleValue::fromMarkedLocation(&vp[i])))
            return false;
    }

    return true;
}

bool
js::GetElements(JSContext *cx, HandleObject aobj, uint32_t length, Value *vp)
{
    if (aobj->isDenseArray() && length <= aobj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(aobj)) {
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
            if (argsobj.maybeGetElements(0, length, vp))
                return true;
        }
    }

    return GetElementsSlow(cx, aobj, length, vp);
}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static JSBool
SetArrayElement(JSContext *cx, HandleObject obj, double index, HandleValue v)
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
            JSObject::setDenseArrayElementWithType(cx, obj, idx, v);
            return true;
        } while (false);

        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!JSObject::makeDenseArraySlow(cx, obj))
            return JS_FALSE;
    }

    RootedId id(cx);
    if (!DoubleIndexToId(cx, index, id.address()))
        return false;

    RootedValue tmp(cx, v);
    return JSObject::setGeneric(cx, obj, obj, id, &tmp, true);
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
DeleteArrayElement(JSContext *cx, HandleObject obj, double index, bool strict)
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

    RootedValue v(cx);
    if (index <= UINT32_MAX) {
        if (!JSObject::deleteElement(cx, obj, uint32_t(index), &v, strict))
            return -1;
    } else {
        if (!JSObject::deleteByValue(cx, obj, DoubleValue(index), &v, strict))
            return -1;
    }

    return v.isTrue() ? 1 : 0;
}

/*
 * When hole is true, delete the property at the given index. Otherwise set
 * its value to v assuming v is rooted.
 */
static JSBool
SetOrDeleteArrayElement(JSContext *cx, HandleObject obj, double index,
                        JSBool hole, HandleValue v)
{
    if (hole) {
        JS_ASSERT(v.isUndefined());
        return DeleteArrayElement(cx, obj, index, true) >= 0;
    }
    return SetArrayElement(cx, obj, index, v);
}

JSBool
js::SetLengthProperty(JSContext *cx, HandleObject obj, double length)
{
    RootedValue v(cx, NumberValue(length));

    /* We don't support read-only array length yet. */
    return JSObject::setProperty(cx, obj, obj, cx->names().length, &v, false);
}

/*
 * Since SpiderMonkey supports cross-class prototype-based delegation, we have
 * to be careful about the length getter and setter being called on an object
 * not of Array class. For the getter, we search obj's prototype chain for the
 * array that caused this getter to be invoked. In the setter case to overcome
 * the JSPROP_SHARED attribute, we must define a shadowing length property.
 */
static JSBool
array_length_getter(JSContext *cx, HandleObject obj_, HandleId id, MutableHandleValue vp)
{
    RootedObject obj(cx, obj_);
    do {
        if (obj->isArray()) {
            vp.setNumber(obj->getArrayLength());
            return true;
        }
        if (!JSObject::getProto(cx, obj, &obj))
            return false;
    } while (obj);
    return true;
}

static JSBool
array_length_setter(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, MutableHandleValue vp)
{
    if (!obj->isArray()) {
        return JSObject::defineProperty(cx, obj, cx->names().length, vp,
                                        NULL, NULL, JSPROP_ENUMERATE);
    }

    uint32_t newlen;
    if (!ToUint32(cx, vp, &newlen))
        return false;

    double d;
    if (!ToNumber(cx, vp, &d))
        return false;

    if (d != newlen) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    uint32_t oldlen = obj->getArrayLength();
    if (oldlen == newlen)
        return true;

    vp.setNumber(newlen);
    if (oldlen < newlen) {
        JSObject::setArrayLength(cx, obj, newlen);
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
                JSObject::setArrayLength(cx, obj, oldlen + 1);
                return false;
            }
            int deletion = DeleteArrayElement(cx, obj, oldlen, strict);
            if (deletion <= 0) {
                JSObject::setArrayLength(cx, obj, oldlen + 1);
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
        RootedObject iter(cx, JS_NewPropertyIterator(cx, obj));
        if (!iter)
            return false;

        uint32_t gap = oldlen - newlen;
        for (;;) {
            jsid nid;
            if (!JS_CHECK_OPERATION_LIMIT(cx) || !JS_NextProperty(cx, iter, &nid))
                return false;
            if (JSID_IS_VOID(nid))
                break;
            uint32_t index;
            RootedValue junk(cx);
            if (js_IdIsIndex(nid, &index) && index - newlen < gap &&
                !JSObject::deleteElement(cx, obj, index, &junk, false)) {
                return false;
            }
        }
    }

    JSObject::setArrayLength(cx, obj, newlen);
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
    return JSID_IS_ATOM(id, cx->names().length) ||
           (js_IdIsIndex(id, &i) && IsDenseArrayIndex(obj, i));
}

static JSBool
array_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    if (!obj->isDenseArray())
        return baseops::LookupProperty(cx, obj, id, objp, propp);

    if (IsDenseArrayId(cx, obj, id)) {
        MarkNonNativePropertyFound(obj, propp);
        objp.set(obj);
        return JS_TRUE;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        objp.set(NULL);
        propp.set(NULL);
        return JS_TRUE;
    }
    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

static JSBool
array_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                     MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return array_lookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
array_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    if (!obj->isDenseArray())
        return baseops::LookupElement(cx, obj, index, objp, propp);

    if (IsDenseArrayIndex(obj, index)) {
        MarkNonNativePropertyFound(obj, propp);
        objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return JSObject::lookupElement(cx, proto, index, objp, propp);

    objp.set(NULL);
    propp.set(NULL);
    return true;
}

static JSBool
array_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return array_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
js_GetDenseArrayElementValue(JSContext *cx, HandleObject obj, jsid id, Value *vp)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t i;
    if (!js_IdIsIndex(id, &i)) {
        JS_ASSERT(JSID_IS_ATOM(id, cx->names().length));
        vp->setNumber(obj->getArrayLength());
        return JS_TRUE;
    }
    *vp = obj->getDenseArrayElement(i);
    return JS_TRUE;
}

static JSBool
array_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                  MutableHandleValue vp)
{
    if (name == cx->names().length) {
        vp.setNumber(obj->getArrayLength());
        return true;
    }

    if (!obj->isDenseArray()) {
        Rooted<jsid> id(cx, NameToId(name));
        return baseops::GetProperty(cx, obj, receiver, id, vp);
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getProperty(cx, proto, receiver, name, vp);
}

static JSBool
array_getElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                 MutableHandleValue vp)
{
    if (!obj->isDenseArray())
        return baseops::GetElement(cx, obj, receiver, index, vp);

    if (index < obj->getDenseArrayInitializedLength()) {
        vp.set(obj->getDenseArrayElement(index));
        if (!vp.isMagic(JS_ARRAY_HOLE)) {
            /* Type information for dense array elements must be correct. */
            JS_ASSERT_IF(!obj->hasSingletonType(),
                         js::types::TypeHasProperty(cx, obj->type(), JSID_VOID, vp));

            return true;
        }
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getElement(cx, proto, receiver, index, vp);
}

static JSBool
array_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                 MutableHandleValue vp)
{
    if (obj->isDenseArray() && !obj->getProto()) {
        vp.setUndefined();
        return true;
    }

    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return baseops::GetProperty(cx, obj, receiver, id, vp);
}

static JSBool
array_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                 MutableHandleValue vp)
{
    RootedValue idval(cx, IdToValue(id));

    uint32_t index;
    if (IsDefinitelyIndex(idval, &index))
        return array_getElement(cx, obj, receiver, index, vp);

    Rooted<SpecialId> sid(cx);
    if (ValueIsSpecial(obj, &idval, sid.address(), cx))
        return array_getSpecial(cx, obj, receiver, sid, vp);

    JSAtom *atom = ToAtom(cx, idval);
    if (!atom)
        return false;

    if (atom->isIndex(&index))
        return array_getElement(cx, obj, receiver, index, vp);

    Rooted<PropertyName*> name(cx, atom->asPropertyName());
    return array_getProperty(cx, obj, receiver, name, vp);
}

static JSBool
slowarray_addProperty(JSContext *cx, HandleObject obj, HandleId id,
                      MutableHandleValue vp)
{
    uint32_t index, length;

    if (!js_IdIsIndex(id, &index))
        return JS_TRUE;
    length = obj->getArrayLength();
    if (index >= length)
        JSObject::setArrayLength(cx, obj, index + 1);
    return JS_TRUE;
}

static JSBool
array_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                 MutableHandleValue vp, JSBool strict)
{
    if (JSID_IS_ATOM(id, cx->names().length))
        return array_length_setter(cx, obj, id, strict, vp);

    if (!obj->isDenseArray())
        return baseops::SetPropertyHelper(cx, obj, obj, id, 0, vp, strict);

    do {
        uint32_t i;
        if (!js_IdIsIndex(id, &i))
            break;
        if (js_PrototypeHasIndexedProperties(obj))
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
        JSObject::setDenseArrayElementWithType(cx, obj, i, vp);
        return true;
    } while (false);

    if (!JSObject::makeDenseArraySlow(cx, obj))
        return false;
    return baseops::SetPropertyHelper(cx, obj, obj, id, 0, vp, strict);
}

static JSBool
array_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                  MutableHandleValue vp, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return array_setGeneric(cx, obj, id, vp, strict);
}

static JSBool
array_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                 MutableHandleValue vp, JSBool strict)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;

    if (!obj->isDenseArray())
        return baseops::SetPropertyHelper(cx, obj, obj, id, 0, vp, strict);

    do {
        /*
         * UINT32_MAX is not an array index and must not affect the length
         * property, so specifically reject it.
         */
        if (index == UINT32_MAX)
            break;
        if (js_PrototypeHasIndexedProperties(obj))
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
        JSObject::setDenseArrayElementWithType(cx, obj, index, vp);
        return true;
    } while (false);

    if (!JSObject::makeDenseArraySlow(cx, obj))
        return false;
    return baseops::SetPropertyHelper(cx, obj, obj, id, 0, vp, strict);
}

static JSBool
array_setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                 MutableHandleValue vp, JSBool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return array_setGeneric(cx, obj, id, vp, strict);
}

JSBool
js_PrototypeHasIndexedProperties(JSObject *obj)
{
    JS_ASSERT(obj->isDenseArray());

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
array_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                    JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (JSID_IS_ATOM(id, cx->names().length))
        return JS_TRUE;

    if (!obj->isDenseArray())
        return baseops::DefineGeneric(cx, obj, id, value, getter, setter, attrs);

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
        JSObject::setDenseArrayElementWithType(cx, obj, i, value);
        return true;
    } while (false);

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    if (!JSObject::makeDenseArraySlow(cx, obj))
        return false;
    return baseops::DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
array_defineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
                     JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return array_defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

/* non-static for direct definition of array elements within the engine */
JSBool
js::array_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                        PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (!obj->isDenseArray())
        return baseops::DefineElement(cx, obj, index, value, getter, setter, attrs);

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
        JSObject::setDenseArrayElementWithType(cx, obj, index, value);
        return true;
    } while (false);

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    if (!JSObject::makeDenseArraySlow(cx, obj))
        return false;
    return baseops::DefineElement(cx, obj, index, value, getter, setter, attrs);
}

static JSBool
array_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return array_defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
array_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    *attrsp = JSID_IS_ATOM(id, cx->names().length)
        ? JSPROP_PERMANENT : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    *attrsp = (name == cx->names().length)
              ? JSPROP_PERMANENT
              : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                     MutableHandleValue rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return baseops::DeleteProperty(cx, obj, name, rval, strict);

    if (name == cx->names().length) {
        rval.setBoolean(false);
        return true;
    }

    rval.setBoolean(true);
    return true;
}

/* non-static for direct deletion of array elements within the engine */
JSBool
js::array_deleteElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue rval,
                        JSBool strict)
{
    if (!obj->isDenseArray())
        return baseops::DeleteElement(cx, obj, index, rval, strict);

    if (index < obj->getDenseArrayInitializedLength()) {
        obj->markDenseArrayNotPacked(cx);
        obj->setDenseArrayElement(index, MagicValue(JS_ARRAY_HOLE));
    }

    if (!js_SuppressDeletedElement(cx, obj, index))
        return false;

    rval.setBoolean(true);
    return true;
}

static JSBool
array_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                    MutableHandleValue rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return baseops::DeleteSpecial(cx, obj, sid, rval, strict);

    rval.setBoolean(true);
    return true;
}

static void
array_trace(JSTracer *trc, RawObject obj)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t initLength = obj->getDenseArrayInitializedLength();
    MarkArraySlots(trc, initLength, obj->getDenseArrayElements(), "element");
}

Class js::ArrayClass = {
    "Array",
    Class::NON_NATIVE | JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
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
        NULL,       /* iteratorObject  */
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
        NULL,       /* typeOf         */
        NULL,       /* thisObject     */
    }
};

Class js::SlowArrayClass = {
    "Array",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
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
        NULL,       /* iteratorObject  */
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
AddLengthProperty(JSContext *cx, HandleObject obj)
{
    /*
     * Add the 'length' property for a newly created or converted slow array,
     * and update the elements to be an empty array owned by the object.
     * The shared emptyObjectElements singleton cannot be used for slow arrays,
     * as accesses to 'length' will use the elements header.
     */

    RootedId lengthId(cx, NameToId(cx->names().length));
    JS_ASSERT(!obj->nativeLookup(cx, lengthId));

    if (!obj->allocateSlowArrayElements(cx))
        return false;

    return JSObject::addProperty(cx, obj, lengthId, array_length_getter, array_length_setter,
                                 SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_SHARED, 0, 0);
}

/*
 * Convert an array object from fast-and-dense to slow-and-flexible.
 */
/* static */ bool
JSObject::makeDenseArraySlow(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isDenseArray());

    MarkTypeObjectFlags(cx, obj,
                        OBJECT_FLAG_NON_PACKED_ARRAY |
                        OBJECT_FLAG_NON_DENSE_ARRAY);

    uint32_t arrayCapacity = obj->getDenseArrayCapacity();
    uint32_t arrayInitialized = obj->getDenseArrayInitializedLength();

    /*
     * Get an allocated array of the existing elements, evicting from the fixed
     * slots if necessary.
     */
    if (!obj->hasDynamicElements()) {
        if (!obj->growElements(cx, arrayCapacity))
            return false;
        JS_ASSERT(obj->hasDynamicElements());
    }

    /* Take ownership of the dense elements. */
    HeapSlot *elems = obj->elements;

    /* Root all values in the array during conversion. */
    AutoValueArray autoArray(cx, (Value *) elems, arrayInitialized);

    /*
     * Save old map now, before calling InitScopeForObject. We'll have to undo
     * on error. This is gross, but a better way is not obvious. Note: the
     * exact contents of the array are not preserved on error.
     */
    RootedShape oldShape(cx, obj->lastProperty());

    /* Create a native scope. */
    {
        gc::AllocKind kind = obj->getAllocKind();
        UnrootedShape shape = EmptyShape::getInitialShape(cx, &SlowArrayClass, obj->getProto(),
                                                          oldShape->getObjectParent(), kind);
        if (!shape)
            return false;

        /*
         * In case an incremental GC is already running, we need to write barrier
         * the elements before (temporarily) destroying them.
         *
         * Note: this has to happen after getInitialShape (which can trigger
         * incremental GC) and *before* we overwrite shape, making us no longer a
         * dense array.
         */
        if (obj->compartment()->needsBarrier())
            obj->prepareElementRangeForOverwrite(0, arrayInitialized);

        obj->shape_ = shape;
    }

    /* Reset to an empty dense array. */
    obj->elements = emptyObjectElements;

    /*
     * Begin with the length property to share more of the property tree.
     * The getter/setter here will directly access the object's private value.
     */
    if (!AddLengthProperty(cx, obj)) {
        obj->shape_ = oldShape;
        if (obj->elements != emptyObjectElements)
            js_free(obj->getElementsHeader());
        obj->elements = elems;
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

        if (!obj->addDataProperty(cx, id, next, JSPROP_ENUMERATE)) {
            obj->shape_ = oldShape;
            js_free(obj->getElementsHeader());
            obj->elements = elems;
            return false;
        }

        obj->initSlot(next, elems[i]);

        next++;
    }

    ObjectElements *oldheader = ObjectElements::fromElements(elems);

    obj->getElementsHeader()->length = oldheader->length;
    js_free(oldheader);

    return true;
}

#if JS_HAS_TOSOURCE
JS_ALWAYS_INLINE bool
IsArray(const Value &v)
{
    return v.isObject() && v.toObject().isArray();
}

JS_ALWAYS_INLINE bool
array_toSource_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArray(args.thisv()));

    Rooted<JSObject*> obj(cx, &args.thisv().toObject());
    RootedValue elt(cx);

    AutoCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    StringBuffer sb(cx);

    if (detector.foundCycle()) {
        if (!sb.append("[]"))
            return false;
        goto make_string;
    }

    if (!sb.append('['))
        return false;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    for (uint32_t index = 0; index < length; index++) {
        JSBool hole;
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

JSBool
array_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArray, array_toSource_impl>(cx, args);
}
#endif

static bool
array_join_sub(JSContext *cx, CallArgs &args, bool locale)
{
    // This method is shared by Array.prototype.join and
    // Array.prototype.toLocaleString. The steps in ES5 are nearly the same, so
    // the annotations in this function apply to both toLocaleString and join.

    // Step 1
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    AutoCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    if (detector.foundCycle()) {
        args.rval().setString(cx->names().empty);
        return true;
    }

    // Steps 2 and 3
    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return false;

    // Steps 4 and 5
    RootedString sepstr(cx, NULL);
    if (!locale && args.hasDefined(0)) {
        sepstr = ToString(cx, args[0]);
        if (!sepstr)
            return false;
    }
    static const jschar comma = ',';
    const jschar *sep;
    size_t seplen;
    if (sepstr) {
        sep = NULL;
        seplen = sepstr->length();
    } else {
        sep = &comma;
        seplen = 1;
    }

    // Step 6 is implicit in the loops below

    StringBuffer sb(cx);

    // Various optimized versions of steps 7-10
    if (!locale && !seplen && obj->isDenseArray() && !js_PrototypeHasIndexedProperties(obj)) {
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

        RootedValue v(cx);
        for (uint32_t i = uint32_t(PointerRangeSize(start, elem)); i < length; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            JSBool hole;
            if (!GetElement(cx, obj, i, &hole, &v))
                return false;
            if (!hole && !v.isNullOrUndefined()) {
                if (!ValueToStringBuffer(cx, v, sb))
                    return false;
            }
        }
    } else {
        RootedValue elt(cx);
        for (uint32_t index = 0; index < length; index++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            JSBool hole;
            if (!GetElement(cx, obj, index, &hole, &elt))
                return false;

            if (!hole && !elt.isNullOrUndefined()) {
                if (locale) {
                    JSObject *robj = ToObject(cx, elt);
                    if (!robj)
                        return false;
                    RootedId id(cx, NameToId(cx->names().toLocaleString));
                    if (!robj->callMethod(cx, id, 0, NULL, &elt))
                        return false;
                }
                if (!ValueToStringBuffer(cx, elt, sb))
                    return false;
            }

            if (index + 1 != length) {
                const jschar *sepchars = sep ? sep : sepstr->getChars(cx);
                if (!sepchars || !sb.append(sepchars, seplen))
                    return false;
            }
        }
    }

    // Step 11
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
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedValue join(cx, args.calleev());
    if (!JSObject::getProperty(cx, obj, obj, cx->names().join, &join))
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

    ag.setCallee(join);
    ag.setThis(ObjectValue(*obj));

    /* Do the call. */
    if (!Invoke(cx, ag))
        return false;
    args.rval().set(ag.rval());
    return true;
}

/* ES5 15.4.4.3 */
static JSBool
array_toLocaleString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);

    return array_join_sub(cx, args, true);
}

/* ES5 15.4.4.5 */
static JSBool
array_join(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    return array_join_sub(cx, args, false);
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

/* vector must point to rooted memory. */
static bool
InitArrayElements(JSContext *cx, HandleObject obj, uint32_t start, uint32_t count, const Value *vector, ShouldUpdateTypes updateTypes)
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
        if (js_PrototypeHasIndexedProperties(obj))
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
            !SetArrayElement(cx, obj, start++, HandleValue::fromMarkedLocation(vector++))) {
            return false;
        }
    }

    if (vector == end)
        return true;

    /* Finish out any remaining elements past the max array index. */
    if (obj->isDenseArray() && !JSObject::makeDenseArraySlow(cx, obj))
        return false;

    JS_ASSERT(start == MAX_ARRAY_INDEX + 1);
    RootedValue value(cx);
    RootedId id(cx);
    Value idval = DoubleValue(MAX_ARRAY_INDEX + 1);
    do {
        value = *vector++;
        if (!ValueToId(cx, idval, id.address()) ||
            !JSObject::setGeneric(cx, obj, obj, id, &value, true)) {
            return false;
        }
        idval.getDoubleRef() += 1;
    } while (vector != end);

    return true;
}

static JSBool
array_reverse(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    do {
        if (!obj->isDenseArray())
            break;
        if (js_PrototypeHasIndexedProperties(obj))
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

    RootedValue lowval(cx), hival(cx);
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

static uint64_t const powersOf10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 1000000000000ULL
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
        if (digitsa == digitsb) {
            *lessOrEqualp = (auint <= buint);
        } else if (digitsa > digitsb) {
            JS_ASSERT((digitsa - digitsb) < ArrayLength(powersOf10));
            *lessOrEqualp = (uint64_t(auint) < uint64_t(buint) * powersOf10[digitsa - digitsb]);
        } else { /* if (digitsb > digitsa) */
            JS_ASSERT((digitsb - digitsa) < ArrayLength(powersOf10));
            *lessOrEqualp = (uint64_t(auint) * powersOf10[digitsb - digitsa] <= uint64_t(buint));
        }
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
    FastInvokeGuard    &fig;

    SortComparatorFunction(JSContext *cx, const Value &fval, FastInvokeGuard &fig)
      : cx(cx), fval(fval), fig(fig) { }

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

    InvokeArgsGuard &ag = fig.args();
    if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 2, &ag))
        return false;

    ag.setCallee(fval);
    ag.setThis(UndefinedValue());
    ag[0] = a;
    ag[1] = b;

    if (!fig.invoke(cx))
        return false;

    double cmp;
    if (!ToNumber(cx, ag.rval(), &cmp))
        return false;

    /*
     * XXX eport some kind of error here if cmp is NaN? ECMA talks about
     * 'consistent compare functions' that don't return NaN, but is silent
     * about what the result should be. So we currently ignore it.
     */
    *lessOrEqualp = (MOZ_DOUBLE_IS_NaN(cmp) || cmp <= 0);
    return true;
}

} /* namespace anonymous */

JSBool
js::array_sort(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue fvalRoot(cx);
    Value &fval = fvalRoot.get();

    if (args.hasDefined(0)) {
        if (args[0].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_SORT_ARG);
            return false;
        }
        fval = args[0];     /* non-default compare function */
    } else {
        fval.setNull();
    }

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
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
        RootedValue v(cx);
        for (uint32_t i = 0; i < len; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            /* Clear vec[newlen] before including it in the rooted set. */
            JSBool hole;
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

                size_t cursor = 0;
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
            FastInvokeGuard fig(cx, fval);
            if (!MergeSort(vec.begin(), n, vec.begin() + n,
                           SortComparatorFunction(cx, fval, fig))) {
                return false;
            }
        }

        if (!InitArrayElements(cx, obj, 0, uint32_t(n), result, DontUpdateTypes))
            return false;
    }

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        RootedValue undefinedValue(cx);
        if (!JS_CHECK_OPERATION_LIMIT(cx) || !SetArrayElement(cx, obj, n++, undefinedValue))
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
array_push_slowly(JSContext *cx, HandleObject obj, CallArgs &args)
{
    uint32_t length;

    if (!GetLengthProperty(cx, obj, &length))
        return false;
    if (!InitArrayElements(cx, obj, length, args.length(), args.array(), UpdateTypes))
        return false;

    /* Per ECMA-262, return the new array length. */
    double newlength = length + double(args.length());
    args.rval().setNumber(newlength);
    return SetLengthProperty(cx, obj, newlength);
}

static bool
array_push1_dense(JSContext* cx, HandleObject obj, CallArgs &args)
{
    JS_ASSERT(args.length() == 1);

    uint32_t length = obj->getArrayLength();
    JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, length, 1);
    if (result != JSObject::ED_OK) {
        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!JSObject::makeDenseArraySlow(cx, obj))
            return false;
        return array_push_slowly(cx, obj, args);
    }

    obj->setDenseArrayLength(length + 1);
    JSObject::setDenseArrayElementWithType(cx, obj, length, args[0]);
    args.rval().setNumber(obj->getArrayLength());
    return true;
}

JS_ALWAYS_INLINE JSBool
NewbornArrayPushImpl(JSContext *cx, HandleObject obj, const Value &v)
{
    JS_ASSERT(!v.isMagic());

    uint32_t length = obj->getArrayLength();
    if (obj->isSlowArray()) {
        /* This can happen in one evil case. See bug 630377. */
        RootedId id(cx);
        RootedValue nv(cx, v);
        return IndexToId(cx, length, id.address()) &&
               baseops::DefineGeneric(cx, obj, id, nv, NULL, NULL, JSPROP_ENUMERATE);
    }

    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(length <= obj->getDenseArrayCapacity());

    if (!obj->ensureElements(cx, length + 1))
        return false;

    obj->setDenseArrayInitializedLength(length + 1);
    obj->setDenseArrayLength(length + 1);
    JSObject::initDenseArrayElementWithType(cx, obj, length, v);
    return true;
}

JSBool
js_NewbornArrayPush(JSContext *cx, HandleObject obj, const Value &vp)
{
    return NewbornArrayPushImpl(cx, obj, vp);
}

JSBool
js::array_push(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Insist on one argument and obj of the expected class. */
    if (args.length() != 1 || !obj->isDenseArray())
        return array_push_slowly(cx, obj, args);

    return array_push1_dense(cx, obj, args);
}

static JSBool
array_pop_slowly(JSContext *cx, HandleObject obj, CallArgs &args)
{
    uint32_t index;
    if (!GetLengthProperty(cx, obj, &index))
        return false;

    if (index == 0) {
        args.rval().setUndefined();
        return SetLengthProperty(cx, obj, index);
    }

    index--;

    JSBool hole;
    RootedValue elt(cx);
    if (!GetElement(cx, obj, index, &hole, &elt))
        return false;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return false;

    args.rval().set(elt);
    return SetLengthProperty(cx, obj, index);
}

static JSBool
array_pop_dense(JSContext *cx, HandleObject obj, CallArgs &args)
{
    uint32_t index = obj->getArrayLength();
    if (index == 0) {
        args.rval().setUndefined();
        return true;
    }

    index--;

    JSBool hole;
    RootedValue elt(cx);
    if (!GetElement(cx, obj, index, &hole, &elt))
        return false;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return false;

    args.rval().set(elt);

    // obj may not be a dense array any more, e.g. if the element was a missing
    // and a getter supplied by the prototype modified the object.
    if (obj->isDenseArray()) {
        if (obj->getDenseArrayInitializedLength() > index)
            obj->setDenseArrayInitializedLength(index);

        JSObject::setArrayLength(cx, obj, index);
        return true;
    }

    return SetLengthProperty(cx, obj, index);
}

JSBool
js::array_pop(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;
    if (obj->isDenseArray())
        return array_pop_dense(cx, obj, args);
    return array_pop_slowly(cx, obj, args);
}

void
js::ArrayShiftMoveElements(JSObject *obj)
{
    JS_ASSERT(obj->isDenseArray());

    /*
     * At this point the length and initialized length have already been
     * decremented and the result fetched, so just shift the array elements
     * themselves.
     */
    uint32_t initlen = obj->getDenseArrayInitializedLength();
    obj->moveDenseArrayElementsUnbarriered(0, 1, initlen);
}

#ifdef JS_METHODJIT
void JS_FASTCALL
mjit::stubs::ArrayShift(VMFrame &f)
{
    JSObject *obj = &f.regs.sp[-1].toObject();
    ArrayShiftMoveElements(obj);
}
#endif /* JS_METHODJIT */

JSBool
js::array_shift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return JS_FALSE;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    if (length == 0) {
        args.rval().setUndefined();
    } else {
        length--;

        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(obj) &&
            length < obj->getDenseArrayCapacity() &&
            0 < obj->getDenseArrayInitializedLength()) {
            args.rval().set(obj->getDenseArrayElement(0));
            if (args.rval().isMagic(JS_ARRAY_HOLE))
                args.rval().setUndefined();
            obj->moveDenseArrayElements(0, 1, obj->getDenseArrayInitializedLength() - 1);
            obj->setDenseArrayInitializedLength(obj->getDenseArrayInitializedLength() - 1);
            JSObject::setArrayLength(cx, obj, length);
            if (!js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(length)))
                return JS_FALSE;
            return JS_TRUE;
        }

        JSBool hole;
        if (!GetElement(cx, obj, 0u, &hole, args.rval()))
            return JS_FALSE;

        /* Slide down the array above the first element. */
        RootedValue value(cx);
        for (uint32_t i = 0; i < length; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, i + 1, &hole, &value) ||
                !SetOrDeleteArrayElement(cx, obj, i, hole, value)) {
                return JS_FALSE;
            }
        }

        /* Delete the only or last element when it exists. */
        if (!hole && DeleteArrayElement(cx, obj, length, true) < 0)
            return JS_FALSE;
    }
    return SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    uint32_t length;
    if (!GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    double newlen = length;
    if (args.length() > 0) {
        /* Slide up the array to make room for all args at the bottom. */
        if (length > 0) {
            bool optimized = false;
            do {
                if (!obj->isDenseArray())
                    break;
                if (js_PrototypeHasIndexedProperties(obj))
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
                RootedValue value(cx);
                do {
                    --last, --upperIndex;
                    JSBool hole;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                        !GetElement(cx, obj, last, &hole, &value) ||
                        !SetOrDeleteArrayElement(cx, obj, upperIndex, hole, value)) {
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
    if (!SetLengthProperty(cx, obj, newlen))
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
    return !js_PrototypeHasIndexedProperties(arr) &&
           startingIndex + count <= arr->getDenseArrayInitializedLength();
}

/* ES5 15.4.4.12. */
static JSBool
array_splice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Steps 3-4. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 5. */
    double relativeStart;
    if (!ToInteger(cx, argc >= 1 ? args[0] : UndefinedValue(), &relativeStart))
        return false;

    /* Step 6. */
    uint32_t actualStart;
    if (relativeStart < 0)
        actualStart = Max(len + relativeStart, 0.0);
    else
        actualStart = Min(relativeStart, double(len));

    /* Step 7. */
    uint32_t actualDeleteCount;
    if (argc != 1) {
        double deleteCountDouble;
        if (!ToInteger(cx, argc >= 2 ? args[1] : Int32Value(0), &deleteCountDouble))
            return false;
        actualDeleteCount = Min(Max(deleteCountDouble, 0.0), double(len - actualStart));
    } else {
        /*
         * Non-standard: if start was specified but deleteCount was omitted,
         * delete to the end of the array.  See bug 668024 for discussion.
         */
        actualDeleteCount = len - actualStart;
    }

    JS_ASSERT(len - actualStart >= actualDeleteCount);

    /* Steps 2, 8-9. */
    RootedObject arr(cx);
    if (CanOptimizeForDenseStorage(obj, actualStart, actualDeleteCount, cx)) {
        arr = NewDenseCopiedArray(cx, actualDeleteCount, obj, actualStart);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);
    } else {
        arr = NewDenseAllocatedArray(cx, actualDeleteCount);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);

        RootedValue fromValue(cx);
        for (uint32_t k = 0; k < actualDeleteCount; k++) {
            JSBool hole;
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, actualStart + k, &hole, &fromValue) ||
                (!hole && !JSObject::defineElement(cx, arr, k, fromValue)))
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
            RootedValue fromValue(cx);
            for (uint32_t from = sourceIndex, to = targetIndex; from < len; from++, to++) {
                JSBool hole;
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
                if (!JSObject::makeDenseArraySlow(cx, obj))
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
            RootedValue fromValue(cx);
            for (double k = len - actualDeleteCount; k > actualStart; k--) {
                double from = k + actualDeleteCount - 1;
                double to = k + itemCount - 1;

                JSBool hole;
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
        if (!SetArrayElement(cx, obj, k, HandleValue::fromMarkedLocation(&items[i])))
            return false;
    }

    /* Step 16. */
    double finalLength = double(len) - actualDeleteCount + itemCount;
    if (!SetLengthProperty(cx, obj, finalLength))
        return false;

    /* Step 17. */
    args.rval().setObject(*arr);
    return true;
}

#ifdef JS_METHODJIT
bool
js::array_concat_dense(JSContext *cx, HandleObject obj1, HandleObject obj2, HandleObject result)
{
    JS_ASSERT(result->isDenseArray() && obj1->isDenseArray() && obj2->isDenseArray());

    uint32_t initlen1 = obj1->getDenseArrayInitializedLength();
    JS_ASSERT(initlen1 == obj1->getArrayLength());

    uint32_t initlen2 = obj2->getDenseArrayInitializedLength();
    JS_ASSERT(initlen2 == obj2->getArrayLength());

    /* No overflow here due to nelements limit. */
    uint32_t len = initlen1 + initlen2;

    if (!result->ensureElements(cx, len))
        return false;

    JS_ASSERT(!result->getDenseArrayInitializedLength());
    result->setDenseArrayInitializedLength(len);

    result->initDenseArrayElements(0, obj1->getDenseArrayElements(), initlen1);
    result->initDenseArrayElements(initlen1, obj2->getDenseArrayElements(), initlen2);

    result->setDenseArrayLength(len);
    return true;
}

void JS_FASTCALL
mjit::stubs::ArrayConcatTwoArrays(VMFrame &f)
{
    RootedObject result(f.cx, &f.regs.sp[-3].toObject());
    RootedObject obj1(f.cx, &f.regs.sp[-2].toObject());
    RootedObject obj2(f.cx, &f.regs.sp[-1].toObject());

    if (!array_concat_dense(f.cx, obj1, obj2, result))
        THROW();
}
#endif /* JS_METHODJIT */

/*
 * Python-esque sequence operations.
 */
JSBool
js::array_concat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    Value *p = args.array() - 1;

    /* Create a new Array object and root it using *vp. */
    RootedObject aobj(cx, ToObject(cx, args.thisv()));
    if (!aobj)
        return false;

    RootedObject nobj(cx);
    uint32_t length;
    if (aobj->isDenseArray()) {
        length = aobj->getArrayLength();
        uint32_t initlen = aobj->getDenseArrayInitializedLength();
        nobj = NewDenseCopiedArray(cx, initlen, aobj, 0);
        if (!nobj)
            return JS_FALSE;
        TryReuseArrayType(aobj, nobj);
        JSObject::setArrayLength(cx, nobj, length);
        args.rval().setObject(*nobj);
        if (argc == 0)
            return JS_TRUE;
        argc--;
        p++;
    } else {
        nobj = NewDenseEmptyArray(cx);
        if (!nobj)
            return JS_FALSE;
        args.rval().setObject(*nobj);
        length = 0;
    }

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    for (unsigned i = 0; i <= argc; i++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        HandleValue v = HandleValue::fromMarkedLocation(&p[i]);
        if (v.isObject()) {
            RootedObject obj(cx, &v.toObject());
            if (ObjectClassIs(*obj, ESClass_Array, cx)) {
                uint32_t alength;
                if (!GetLengthProperty(cx, obj, &alength))
                    return false;
                RootedValue tmp(cx);
                for (uint32_t slot = 0; slot < alength; slot++) {
                    JSBool hole;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) || !GetElement(cx, obj, slot, &hole, &tmp))
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

    return SetLengthProperty(cx, nobj, length);
}

static JSBool
array_slice(JSContext *cx, unsigned argc, Value *vp)
{
    uint32_t length, begin, end, slot;
    JSBool hole;

    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    if (!GetLengthProperty(cx, obj, &length))
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

    RootedObject nobj(cx);

    if (obj->isDenseArray() && end <= obj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(obj)) {
        nobj = NewDenseCopiedArray(cx, end - begin, obj, begin);
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

    RootedValue value(cx);
    for (slot = begin; slot < end; slot++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, slot, &hole, &value)) {
            return JS_FALSE;
        }
        if (!hole && !SetArrayElement(cx, nobj, slot - begin, value))
            return JS_FALSE;
    }

    args.rval().setObject(*nobj);
    return JS_TRUE;
}

/* ES5 15.4.4.19. */
static JSBool
array_map(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    RootedObject callable(cx, ValueToCallable(cx, &args[0]));
    if (!callable)
        return false;

    /* Step 5. */
    RootedValue thisv(cx, args.length() >= 2 ? args[1] : UndefinedValue());

    /* Step 6. */
    RootedObject arr(cx, NewDenseAllocatedArray(cx, len));
    if (!arr)
        return false;
    TypeObject *newtype = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!newtype)
        return false;
    arr->setType(newtype);

    /* Step 7. */
    uint32_t k = 0;

    /* Step 8. */
    RootedValue kValue(cx);
    FastInvokeGuard fig(cx, ObjectValue(*callable));
    InvokeArgsGuard &ag = fig.args();
    while (k < len) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        JSBool kNotPresent;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.setThis(thisv);
            ag[0] = kValue;
            ag[1] = NumberValue(k);
            ag[2] = ObjectValue(*obj);
            if (!fig.invoke(cx))
                return false;
            kValue = ag.rval();
            if (!SetArrayElement(cx, arr, k, kValue))
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
    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    /* Step 2-3. */
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 4. */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }
    RootedObject callable(cx, ValueToCallable(cx, &args[0]));
    if (!callable)
        return false;

    /* Step 5. */
    RootedValue thisv(cx, args.length() >= 2 ? args[1] : UndefinedValue());

    /* Step 6. */
    RootedObject arr(cx, NewDenseAllocatedArray(cx, 0));
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
    FastInvokeGuard fig(cx, ObjectValue(*callable));
    InvokeArgsGuard &ag = fig.args();
    RootedValue kValue(cx);
    while (k < len) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;

        /* Step a, b, and c.i. */
        JSBool kNotPresent;
        if (!GetElement(cx, obj, k, &kNotPresent, &kValue))
            return false;

        /* Step c.ii-iii. */
        if (!kNotPresent) {
            if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 3, &ag))
                return false;
            ag.setCallee(ObjectValue(*callable));
            ag.setThis(thisv);
            ag[0] = kValue;
            ag[1] = NumberValue(k);
            ag[2] = ObjectValue(*obj);
            if (!fig.invoke(cx))
                return false;

            if (ToBoolean(ag.rval())) {
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

         {"lastIndexOf",        {NULL, NULL},       1,0, "ArrayLastIndexOf"},
         {"indexOf",            {NULL, NULL},       1,0, "ArrayIndexOf"},
         {"forEach",            {NULL, NULL},       1,0, "ArrayForEach"},
    JS_FN("map",                array_map,          1,JSFUN_GENERIC_NATIVE),
         {"reduce",             {NULL, NULL},       1,0, "ArrayReduce"},
         {"reduceRight",        {NULL, NULL},       1,0, "ArrayReduceRight"},
    JS_FN("filter",             array_filter,       1,JSFUN_GENERIC_NATIVE),
         {"some",               {NULL, NULL},       1,0, "ArraySome"},
         {"every",              {NULL, NULL},       1,0, "ArrayEvery"},

    JS_FN("iterator",           JS_ArrayIterator,   0,0),
    JS_FS_END
};

static JSFunctionSpec array_static_methods[] = {
    JS_FN("isArray",            array_isArray,      1,0),
         {"lastIndexOf",        {NULL, NULL},       2,0, "ArrayStaticLastIndexOf"},
         {"indexOf",            {NULL, NULL},       2,0, "ArrayStaticIndexOf"},
         {"forEach",            {NULL, NULL},       2,0, "ArrayStaticForEach"},
         {"every",              {NULL, NULL},       2,0, "ArrayStaticEvery"},
         {"some",               {NULL, NULL},       2,0, "ArrayStaticSome"},
         {"reduce",             {NULL, NULL},       2,0, "ArrayStaticReduce"},
         {"reduceRight",        {NULL, NULL},       2,0, "ArrayStaticReduceRight"},
    JS_FS_END
};

/* ES5 15.4.2 */
JSBool
js_Array(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedTypeObject type(cx, GetTypeCallerInitObject(cx, JSProto_Array));
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
        length = ToUint32(d);
        if (d != double(length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
    }

    RootedObject obj(cx, NewDenseUnallocatedArray(cx, length));
    if (!obj)
        return false;

    obj->setType(type);

    /* If the length calculation overflowed, make sure that is marked for the new type. */
    if (obj->getArrayLength() > INT32_MAX)
        JSObject::setArrayLength(cx, obj, obj->getArrayLength());

    args.rval().setObject(*obj);
    return true;
}

JSObject *
js_InitArrayClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    RootedObject arrayProto(cx, global->createBlankPrototype(cx, &SlowArrayClass));
    if (!arrayProto || !AddLengthProperty(cx, arrayProto))
        return NULL;
    JSObject::setArrayLength(cx, arrayProto, 0);

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, js_Array, cx->names().Array, 1);
    if (!ctor)
        return NULL;

    /*
     * The default 'new' type of Array.prototype is required by type inference
     * to have unknown properties, to simplify handling of e.g. heterogenous
     * arrays in JSON and script literals and allows setDenseArrayElement to
     * be used without updating the indexed type set for such default arrays.
     */
    if (!JSObject::setNewTypeUnknown(cx, arrayProto))
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
NewArray(JSContext *cx, uint32_t length, RawObject protoArg)
{
    gc::AllocKind kind = GuessArrayGCKind(length);
    JS_ASSERT(CanBeFinalizedInBackground(kind, &ArrayClass));
    kind = GetBackgroundAllocKind(kind);

    NewObjectCache &cache = cx->runtime->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (cache.lookupGlobal(&ArrayClass, cx->global(), kind, &entry)) {
        RootedObject obj(cx, cache.newObjectFromHit(cx, entry));
        if (obj) {
            /* Fixup the elements pointer and length, which may be incorrect. */
            obj->setFixedElements();
            JSObject::setArrayLength(cx, obj, length);
            if (allocateCapacity && !EnsureNewArrayElements(cx, obj, length))
                return NULL;
            return obj;
        }
    }

    RootedObject proto(cx, protoArg);
    if (protoArg)
        PoisonPtr(&protoArg);

    if (!proto && !FindProto(cx, &ArrayClass, &proto))
        return NULL;

    RootedTypeObject type(cx, proto->getNewType(cx));
    if (!type)
        return NULL;

    /*
     * Get a shape with zero fixed slots, regardless of the size class.
     * See JSObject::createDenseArray.
     */
    RootedShape shape(cx, EmptyShape::getInitialShape(cx, &ArrayClass, TaggedProto(proto),
                                                      cx->global(), gc::FINALIZE_OBJECT0));
    if (!shape)
        return NULL;

    JSObject* obj = JSObject::createDenseArray(cx, kind, shape, type, length);
    if (!obj)
        return NULL;

    if (entry != -1)
        cache.fillGlobal(entry, &ArrayClass, cx->global(), kind, obj);

    if (allocateCapacity && !EnsureNewArrayElements(cx, obj, length))
        return NULL;

    Probes::createObject(cx, obj);
    return obj;
}

JSObject * JS_FASTCALL
js::NewDenseEmptyArray(JSContext *cx, RawObject proto /* = NULL */)
{
    return NewArray<false>(cx, 0, proto);
}

JSObject * JS_FASTCALL
js::NewDenseAllocatedArray(JSContext *cx, uint32_t length, RawObject proto /* = NULL */)
{
    return NewArray<true>(cx, length, proto);
}

JSObject * JS_FASTCALL
js::NewDenseUnallocatedArray(JSContext *cx, uint32_t length, RawObject proto /* = NULL */)
{
    return NewArray<false>(cx, length, proto);
}

#ifdef JS_METHODJIT
JSObject * JS_FASTCALL
mjit::stubs::NewDenseUnallocatedArray(VMFrame &f, uint32_t length)
{
    JSObject *obj = NewArray<false>(f.cx, length, (RawObject)f.scratch);
    if (!obj)
        THROWV(NULL);

    return obj;
}
#endif

JSObject *
js::NewDenseCopiedArray(JSContext *cx, uint32_t length, HandleObject src, uint32_t elementOffset,
                        RawObject proto /* = NULL */)
{
    JSObject* obj = NewArray<true>(cx, length, proto);
    if (!obj)
        return NULL;

    JS_ASSERT(obj->getDenseArrayCapacity() >= length);

    const Value* vp = src->getDenseArrayElements() + elementOffset;
    obj->setDenseArrayInitializedLength(vp ? length : 0);

    if (vp)
        obj->initDenseArrayElements(0, vp, length);

    return obj;
}

// values must point at already-rooted Value objects
JSObject *
js::NewDenseCopiedArray(JSContext *cx, uint32_t length, const Value *values,
                        RawObject proto /* = NULL */)
{
    JSObject* obj = NewArray<true>(cx, length, proto);
    if (!obj)
        return NULL;

    JS_ASSERT(obj->getDenseArrayCapacity() >= length);

    obj->setDenseArrayInitializedLength(values ? length : 0);

    if (values)
        obj->initDenseArrayElements(0, values, length);

    return obj;
}

JSObject *
js::NewSlowEmptyArray(JSContext *cx)
{
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &SlowArrayClass));
    if (!obj || !AddLengthProperty(cx, obj))
        return NULL;

    JSObject::setArrayLength(cx, obj, 0);
    return obj;
}

#ifdef DEBUG
JSBool
js_ArrayInfo(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *array;

    for (unsigned i = 0; i < args.length(); i++) {
        RootedValue arg(cx, args[i]);

        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, NullPtr());
        if (!bytes)
            return JS_FALSE;
        if (arg.isPrimitive() ||
            !(array = arg.toObjectOrNull())->isArray()) {
            fprintf(stderr, "%s: not array\n", bytes);
            js_free(bytes);
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
        js_free(bytes);
    }

    args.rval().setUndefined();
    return true;
}
#endif
