/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla WebGL impl
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include <string.h>

#include "mozilla/FloatingPoint.h"
#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jstypedarray.h"

#include "vm/GlobalObject.h"
#include "vm/NumericConversions.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jstypedarrayinlines.h"

#include "vm/MethodGuard-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;

/*
 * Allocate array buffers with the maximum number of fixed slots marked as
 * reserved, so that the fixed slots may be used for the buffer's contents.
 * The last fixed slot is kept for the object's private data.
 */
static const uint8_t ARRAYBUFFER_RESERVED_SLOTS = JSObject::MAX_FIXED_SLOTS - 1;

static bool
ValueIsLength(JSContext *cx, const Value &v, uint32_t *len)
{
    if (v.isInt32()) {
        int32_t i = v.toInt32();
        if (i < 0)
            return false;
        *len = i;
        return true;
    }

    if (v.isDouble()) {
        double d = v.toDouble();
        if (MOZ_DOUBLE_IS_NaN(d))
            return false;

        uint32_t length = uint32_t(d);
        if (d != double(length))
            return false;

        *len = length;
        return true;
    }

    return false;
}

/*
 * Convert |v| to an array index for an array of length |length| per
 * the Typed Array Specification section 7.0, |subarray|. If successful,
 * the output value is in the range [0, length).
 */
static bool
ToClampedIndex(JSContext *cx, const Value &v, int32_t length, int32_t *out)
{
    if (!ToInt32(cx, v, out))
        return false;
    if (*out < 0) {
        *out += length;
        if (*out < 0)
            *out = 0;
    } else if (*out > length) {
        *out = length;
    }
    return true;
}

/*
 * ArrayBuffer
 *
 * This class holds the underlying raw buffer that the TypedArray classes
 * access.  It can be created explicitly and passed to a TypedArray, or
 * can be created implicitly by constructing a TypedArray with a size.
 */

/**
 * Walks up the prototype chain to find the actual ArrayBuffer data.
 * This MAY return NULL. Callers should always use isArrayBuffer()
 * first.
 */
JSObject *
ArrayBufferObject::getArrayBuffer(JSObject *obj)
{
    while (obj && !obj->isArrayBuffer())
        obj = obj->getProto();
    return obj;
}

JSBool
ArrayBufferObject::prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSObject *bufobj = getArrayBuffer(obj);
    if (!bufobj) {
        vp->setInt32(0);
        return true;
    }

    ArrayBufferObject &arrayBuffer = bufobj->asArrayBuffer();
    vp->setInt32(int32_t(arrayBuffer.byteLength()));
    return true;
}

JSBool
ArrayBufferObject::fun_slice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, fun_slice, &ArrayBufferClass, &ok);
    if (!obj)
        return ok;

    ArrayBufferObject &arrayBuffer = obj->asArrayBuffer();

    // these are the default values
    int32_t length = int32_t(arrayBuffer.byteLength());
    int32_t begin = 0, end = length;

    if (args.length() > 0) {
        if (!ToClampedIndex(cx, args[0], length, &begin))
            return false;

        if (args.length() > 1) {
            if (!ToClampedIndex(cx, args[1], length, &end))
                return false;
        }
    }

    if (begin > end)
        begin = end;

    JSObject *nobj = createSlice(cx, arrayBuffer, begin, end);
    if (!nobj)
        return false;
    args.rval().setObject(*nobj);
    return true;
}

/*
 * new ArrayBuffer(byteLength)
 */
JSBool
ArrayBufferObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    int32_t nbytes = 0;
    if (argc > 0 && !ToInt32(cx, vp[2], &nbytes))
        return false;

    if (nbytes < 0) {
        /*
         * We're just not going to support arrays that are bigger than what will fit
         * as an integer value; if someone actually ever complains (validly), then we
         * can fix.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    JSObject *bufobj = create(cx, uint32_t(nbytes));
    if (!bufobj)
        return false;
    vp->setObject(*bufobj);
    return true;
}

bool
ArrayBufferObject::allocateSlots(JSContext *cx, uint32_t size, uint8_t *contents)
{
    /*
     * ArrayBufferObjects delegate added properties to another JSObject, so
     * their internal layout can use the object's fixed slots for storage.
     * Set up the object to look like an array with an elements header.
     */
    JS_ASSERT(isArrayBuffer() && !hasDynamicSlots() && !hasDynamicElements());

    size_t usableSlots = ARRAYBUFFER_RESERVED_SLOTS - ObjectElements::VALUES_PER_HEADER;

    if (size > sizeof(Value) * usableSlots) {
        ObjectElements *newheader = (ObjectElements *)cx->calloc_(size + sizeof(ObjectElements));
        if (!newheader)
            return false;
        elements = newheader->elements();
        if (contents)
            memcpy(elements, contents, size);
    } else {
        elements = fixedElements();
        if (contents)
            memcpy(elements, contents, size);
        else
            memset(elements, 0, size);
    }

    ObjectElements *header = getElementsHeader();

    /*
     * Note that |bytes| may not be a multiple of |sizeof(Value)|, so
     * |capacity * sizeof(Value)| may underestimate the size by up to
     * |sizeof(Value) - 1| bytes.
     */
    header->capacity = size / sizeof(Value);
    header->initializedLength = 0;
    header->length = size;
    header->unused = 0;

    return true;
}

static JSObject *
DelegateObject(JSContext *cx, JSObject *obj)
{
    if (!obj->getPrivate()) {
        JSObject *delegate = NewObjectWithGivenProto(cx, &ObjectClass, obj->getProto(), NULL);
        obj->setPrivate(delegate);
        return delegate;
    }
    return static_cast<JSObject*>(obj->getPrivate());
}

JSObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, uint8_t *contents)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &ArrayBufferObject::protoClass);
    if (!obj)
        return NULL;
#ifdef JS_THREADSAFE
    JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT16_BACKGROUND);
#else
    JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT16);
#endif

    JS_ASSERT(obj->getClass() == &ArrayBufferObject::protoClass);

    js::Shape *empty = EmptyShape::getInitialShape(cx, &ArrayBufferClass,
                                                   obj->getProto(), obj->getParent(),
                                                   gc::FINALIZE_OBJECT16);
    if (!empty)
        return NULL;
    obj->setLastPropertyInfallible(empty);

    /*
     * The first 8 bytes hold the length.
     * The rest of it is a flat data store for the array buffer.
     */
    if (!obj->asArrayBuffer().allocateSlots(cx, nbytes, contents))
        return NULL;

    return obj;
}

JSObject *
ArrayBufferObject::createSlice(JSContext *cx, ArrayBufferObject &arrayBuffer,
                               uint32_t begin, uint32_t end)
{
    JS_ASSERT(begin <= arrayBuffer.byteLength());
    JS_ASSERT(end <= arrayBuffer.byteLength());
    JS_ASSERT(begin <= end);
    uint32_t length = end - begin;

    if (arrayBuffer.hasData())
        return create(cx, length, arrayBuffer.dataPointer() + begin);

    return create(cx, 0);
}

void
ArrayBufferObject::obj_trace(JSTracer *trc, JSObject *obj)
{
    /*
     * If this object changes, it will get marked via the private data barrier,
     * so it's safe to leave it Unbarriered.
     */
    JSObject *delegate = static_cast<JSObject*>(obj->getPrivate());
    if (delegate) {
        MarkObjectUnbarriered(trc, &delegate, "arraybuffer.delegate");
        obj->setPrivateUnbarriered(delegate);
    }
}

static JSProperty * const PROPERTY_FOUND = reinterpret_cast<JSProperty *>(1);

JSBool
ArrayBufferObject::obj_lookupGeneric(JSContext *cx, JSObject *obj, jsid id,
                                     JSObject **objp, JSProperty **propp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        *propp = PROPERTY_FOUND;
        *objp = getArrayBuffer(obj);
        return true;
    }

    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;

    JSBool delegateResult = delegate->lookupGeneric(cx, id, objp, propp);

    /* If false, there was an error, so propagate it.
     * Otherwise, if propp is non-null, the property
     * was found. Otherwise it was not
     * found so look in the prototype chain.
     */
    if (!delegateResult)
        return false;

    if (*propp != NULL) {
        if (*objp == delegate)
            *objp = obj;
        return true;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        *objp = NULL;
        *propp = NULL;
        return true;
    }

    return proto->lookupGeneric(cx, id, objp, propp);
}

JSBool
ArrayBufferObject::obj_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name,
                                      JSObject **objp, JSProperty **propp)
{
    return obj_lookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

JSBool
ArrayBufferObject::obj_lookupElement(JSContext *cx, JSObject *obj, uint32_t index,
                                     JSObject **objp, JSProperty **propp)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;

    /*
     * If false, there was an error, so propagate it.
     * Otherwise, if propp is non-null, the property
     * was found. Otherwise it was not
     * found so look in the prototype chain.
     */
    if (!delegate->lookupElement(cx, index, objp, propp))
        return false;

    if (*propp != NULL) {
        if (*objp == delegate)
            *objp = obj;
        return true;
    }

    if (JSObject *proto = obj->getProto())
        return proto->lookupElement(cx, index, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

JSBool
ArrayBufferObject::obj_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid,
                                     JSObject **objp, JSProperty **propp)
{
    return obj_lookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

JSBool
ArrayBufferObject::obj_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom))
        return true;

    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_DefineProperty(cx, delegate, id, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineProperty(JSContext *cx, JSObject *obj,
                                      PropertyName *name, const Value *v,
                                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return obj_defineGeneric(cx, obj, ATOM_TO_JSID(name), v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_DefineElement(cx, delegate, index, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return obj_defineGeneric(cx, obj, SPECIALID_TO_JSID(sid), v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    obj = getArrayBuffer(obj);
    JS_ASSERT(obj);
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        vp->setInt32(obj->asArrayBuffer().byteLength());
        return true;
    }

    RootedVarObject delegate(cx, DelegateObject(cx, obj));
    if (!delegate)
        return false;
    return js_GetProperty(cx, delegate, RootedVarObject(cx, receiver), id, vp);
}

JSBool
ArrayBufferObject::obj_getProperty(JSContext *cx, JSObject *obj,
                                   JSObject *receiver, PropertyName *name, Value *vp)
{
    obj = getArrayBuffer(obj);
    if (name == cx->runtime->atomState.byteLengthAtom) {
        vp->setInt32(obj->asArrayBuffer().byteLength());
        return true;
    }

    RootedVarObject delegate(cx, DelegateObject(cx, obj));
    if (!delegate)
        return false;
    return js_GetProperty(cx, delegate, RootedVarObject(cx, receiver), ATOM_TO_JSID(name), vp);
}

JSBool
ArrayBufferObject::obj_getElement(JSContext *cx, JSObject *obj,
                                  JSObject *receiver, uint32_t index, Value *vp)
{
    RootedVarObject delegate(cx, DelegateObject(cx, getArrayBuffer(obj)));
    if (!delegate)
        return false;
    return js_GetElement(cx, delegate, RootedVarObject(cx, receiver), index, vp);
}

JSBool
ArrayBufferObject::obj_getElementIfPresent(JSContext *cx, JSObject *obj, JSObject *receiver,
                                           uint32_t index, Value *vp, bool *present)
{
    JSObject *delegate = DelegateObject(cx, getArrayBuffer(obj));
    if (!delegate)
        return false;
    return delegate->getElementIfPresent(cx, receiver, index, vp, present);
}

JSBool
ArrayBufferObject::obj_getSpecial(JSContext *cx, JSObject *obj,
                                  JSObject *receiver, SpecialId sid, Value *vp)
{
    return obj_getGeneric(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

JSBool
ArrayBufferObject::obj_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom))
        return true;

    RootedVarObject delegate(cx, DelegateObject(cx, obj));
    if (!delegate)
        return false;

    if (JSID_IS_ATOM(id, cx->runtime->atomState.protoAtom)) {
        // setting __proto__ = null
        // effectively removes the prototype chain.
        // any attempt to set __proto__ on native
        // objects after setting them to null makes
        // __proto__ just a plain property.
        // the following code simulates this behaviour on arrays.
        //
        // we first attempt to set the prototype on
        // the delegate which is a native object
        // so that existing code handles the case
        // of treating it as special or plain.
        // if the delegate's prototype has now changed
        // then we change our prototype too.
        //
        // otherwise __proto__ was a plain property
        // and we don't modify our prototype chain
        // since obj_getProperty will fetch it as a plain
        // property from the delegate.

        JSObject *oldDelegateProto = delegate->getProto();

        if (!js_SetPropertyHelper(cx, delegate, id, 0, vp, strict))
            return false;

        if (delegate->getProto() != oldDelegateProto) {
            // actual __proto__ was set and not a plain property called
            // __proto__
            if (!obj->isExtensible()) {
                obj->reportNotExtensible(cx);
                return false;
            }
            if (!SetProto(cx, obj, vp->toObjectOrNull(), true)) {
                // this can be caused for example by setting x.__proto__ = x
                // restore delegate prototype chain
                SetProto(cx, delegate, oldDelegateProto, true);
                return false;
            }
        }
        return true;
    }

    return js_SetPropertyHelper(cx, delegate, id, 0, vp, strict);
}

JSBool
ArrayBufferObject::obj_setProperty(JSContext *cx, JSObject *obj,
                                   PropertyName *name, Value *vp, JSBool strict)
{
    return obj_setGeneric(cx, obj, ATOM_TO_JSID(name), vp, strict);
}

JSBool
ArrayBufferObject::obj_setElement(JSContext *cx, JSObject *obj,
                                  uint32_t index, Value *vp, JSBool strict)
{
    RootedVarObject delegate(cx, DelegateObject(cx, obj));
    if (!delegate)
        return false;

    return js_SetElementHelper(cx, delegate, index, 0, vp, strict);
}

JSBool
ArrayBufferObject::obj_setSpecial(JSContext *cx, JSObject *obj,
                                  SpecialId sid, Value *vp, JSBool strict)
{
    return obj_setGeneric(cx, obj, SPECIALID_TO_JSID(sid), vp, strict);
}

JSBool
ArrayBufferObject::obj_getGenericAttributes(JSContext *cx, JSObject *obj,
                                            jsid id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY;
        return true;
    }

    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_GetAttributes(cx, delegate, id, attrsp);
}

JSBool
ArrayBufferObject::obj_getPropertyAttributes(JSContext *cx, JSObject *obj,
                                             PropertyName *name, unsigned *attrsp)
{
    return obj_getGenericAttributes(cx, obj, ATOM_TO_JSID(name), attrsp);
}

JSBool
ArrayBufferObject::obj_getElementAttributes(JSContext *cx, JSObject *obj,
                                            uint32_t index, unsigned *attrsp)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_GetElementAttributes(cx, delegate, index, attrsp);
}

JSBool
ArrayBufferObject::obj_getSpecialAttributes(JSContext *cx, JSObject *obj,
                                            SpecialId sid, unsigned *attrsp)
{
    return obj_getGenericAttributes(cx, obj, SPECIALID_TO_JSID(sid), attrsp);
}

JSBool
ArrayBufferObject::obj_setGenericAttributes(JSContext *cx, JSObject *obj,
                                            jsid id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_SET_ARRAY_ATTRS);
        return false;
    }

    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_SetAttributes(cx, delegate, id, attrsp);
}

JSBool
ArrayBufferObject::obj_setPropertyAttributes(JSContext *cx, JSObject *obj,
                                             PropertyName *name, unsigned *attrsp)
{
    return obj_setGenericAttributes(cx, obj, ATOM_TO_JSID(name), attrsp);
}

JSBool
ArrayBufferObject::obj_setElementAttributes(JSContext *cx, JSObject *obj,
                                            uint32_t index, unsigned *attrsp)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_SetElementAttributes(cx, delegate, index, attrsp);
}

JSBool
ArrayBufferObject::obj_setSpecialAttributes(JSContext *cx, JSObject *obj,
                                            SpecialId sid, unsigned *attrsp)
{
    return obj_setGenericAttributes(cx, obj, SPECIALID_TO_JSID(sid), attrsp);
}

JSBool
ArrayBufferObject::obj_deleteProperty(JSContext *cx, JSObject *obj,
                                      PropertyName *name, Value *rval, JSBool strict)
{
    if (name == cx->runtime->atomState.byteLengthAtom) {
        rval->setBoolean(false);
        return true;
    }

    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_DeleteProperty(cx, delegate, name, rval, strict);
}

JSBool
ArrayBufferObject::obj_deleteElement(JSContext *cx, JSObject *obj,
                                     uint32_t index, Value *rval, JSBool strict)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_DeleteElement(cx, delegate, index, rval, strict);
}

JSBool
ArrayBufferObject::obj_deleteSpecial(JSContext *cx, JSObject *obj,
                                     SpecialId sid, Value *rval, JSBool strict)
{
    JSObject *delegate = DelegateObject(cx, obj);
    if (!delegate)
        return false;
    return js_DeleteSpecial(cx, delegate, sid, rval, strict);
}

JSBool
ArrayBufferObject::obj_enumerate(JSContext *cx, JSObject *obj,
                                 JSIterateOp enum_op, Value *statep, jsid *idp)
{
    statep->setNull();
    return true;
}

JSType
ArrayBufferObject::obj_typeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

/*
 * TypedArray
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

JSObject *
TypedArray::getTypedArray(JSObject *obj)
{
    while (!obj->isTypedArray())
        obj = obj->getProto();
    return obj;
}

inline bool
TypedArray::isArrayIndex(JSContext *cx, JSObject *obj, jsid id, uint32_t *ip)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index) && index < getLength(obj)) {
        if (ip)
            *ip = index;
        return true;
    }

    return false;
}

typedef Value (* TypedArrayPropertyGetter)(JSObject *tarray);

template <TypedArrayPropertyGetter Get>
class TypedArrayGetter {
  public:
    static inline bool get(JSContext *cx, JSObject *obj, jsid id, Value *vp) {
        do {
            if (obj->isTypedArray()) {
                JSObject *tarray = TypedArray::getTypedArray(obj);
                if (tarray)
                    *vp = Get(tarray);
                return true;
            }
        } while ((obj = obj->getProto()) != NULL);
        return true;
    }
};

/*
 * For now (until slots directly hold data)
 * slots data element points to the JSObject representing the ArrayBuffer.
 */
inline Value
getBufferValue(JSObject *tarray)
{
    JSObject *buffer = TypedArray::getBuffer(tarray);
    return ObjectValue(*buffer);
}

JSBool
TypedArray::prop_getBuffer(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return TypedArrayGetter<getBufferValue>::get(cx, obj, id, vp);
}

inline Value
getByteOffsetValue(JSObject *tarray)
{
    return Int32Value(TypedArray::getByteOffset(tarray));
}

JSBool
TypedArray::prop_getByteOffset(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return TypedArrayGetter<getByteOffsetValue>::get(cx, obj, id, vp);
}

inline Value
getByteLengthValue(JSObject *tarray)
{
    return Int32Value(TypedArray::getByteLength(tarray));
}

JSBool
TypedArray::prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return TypedArrayGetter<getByteLengthValue>::get(cx, obj, id, vp);
}

inline Value
getLengthValue(JSObject *tarray)
{
    return Int32Value(TypedArray::getLength(tarray));
}

JSBool
TypedArray::prop_getLength(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return TypedArrayGetter<getLengthValue>::get(cx, obj, id, vp);
}

JSBool
TypedArray::obj_lookupGeneric(JSContext *cx, JSObject *obj, jsid id,
                              JSObject **objp, JSProperty **propp)
{
    JSObject *tarray = getTypedArray(obj);
    JS_ASSERT(tarray);

    if (isArrayIndex(cx, tarray, id)) {
        *propp = PROPERTY_FOUND;
        *objp = obj;
        return true;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        *objp = NULL;
        *propp = NULL;
        return true;
    }

    return proto->lookupGeneric(cx, id, objp, propp);
}

JSBool
TypedArray::obj_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name,
                               JSObject **objp, JSProperty **propp)
{
    return obj_lookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

JSBool
TypedArray::obj_lookupElement(JSContext *cx, JSObject *obj, uint32_t index,
                              JSObject **objp, JSProperty **propp)
{
    JSObject *tarray = getTypedArray(obj);
    JS_ASSERT(tarray);

    if (index < getLength(tarray)) {
        *propp = PROPERTY_FOUND;
        *objp = obj;
        return true;
    }

    if (JSObject *proto = obj->getProto())
        return proto->lookupElement(cx, index, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

JSBool
TypedArray::obj_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid,
                              JSObject **objp, JSProperty **propp)
{
    return obj_lookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

JSBool
TypedArray::obj_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    *attrsp = (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
              ? JSPROP_PERMANENT | JSPROP_READONLY
              : JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    return obj_getGenericAttributes(cx, obj, SPECIALID_TO_JSID(sid), attrsp);
}

JSBool
TypedArray::obj_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

/* static */ int
TypedArray::lengthOffset()
{
    return JSObject::getFixedSlotOffset(FIELD_LENGTH);
}

/* static */ int
TypedArray::dataOffset()
{
    return JSObject::getPrivateDataOffset(NUM_FIXED_SLOTS);
}

/* Helper clamped uint8_t type */

uint32_t JS_FASTCALL
js::ClampDoubleToUint8(const double x)
{
    // Not < so that NaN coerces to 0
    if (!(x >= 0))
        return 0;

    if (x > 255)
        return 255;

    double toTruncate = x + 0.5;
    uint8_t y = uint8_t(toTruncate);

    /*
     * now val is rounded to nearest, ties rounded up.  We want
     * rounded to nearest ties to even, so check whether we had a
     * tie.
     */
    if (y == toTruncate) {
        /*
         * It was a tie (since adding 0.5 gave us the exact integer
         * we want).  Since we rounded up, we either already have an
         * even number or we have an odd number but the number we
         * want is one less.  So just unconditionally masking out the
         * ones bit should do the trick to get us the value we
         * want.
         */
        return (y & ~1);
    }

    return y;
}

template<typename NativeType> static inline const int TypeIDOfType();
template<> inline const int TypeIDOfType<int8_t>() { return TypedArray::TYPE_INT8; }
template<> inline const int TypeIDOfType<uint8_t>() { return TypedArray::TYPE_UINT8; }
template<> inline const int TypeIDOfType<int16_t>() { return TypedArray::TYPE_INT16; }
template<> inline const int TypeIDOfType<uint16_t>() { return TypedArray::TYPE_UINT16; }
template<> inline const int TypeIDOfType<int32_t>() { return TypedArray::TYPE_INT32; }
template<> inline const int TypeIDOfType<uint32_t>() { return TypedArray::TYPE_UINT32; }
template<> inline const int TypeIDOfType<float>() { return TypedArray::TYPE_FLOAT32; }
template<> inline const int TypeIDOfType<double>() { return TypedArray::TYPE_FLOAT64; }
template<> inline const int TypeIDOfType<uint8_clamped>() { return TypedArray::TYPE_UINT8_CLAMPED; }

template<typename NativeType> static inline const bool ElementTypeMayBeDouble() { return false; }
template<> inline const bool ElementTypeMayBeDouble<uint32_t>() { return true; }
template<> inline const bool ElementTypeMayBeDouble<float>() { return true; }
template<> inline const bool ElementTypeMayBeDouble<double>() { return true; }

template<typename NativeType> class TypedArrayTemplate;

template<typename ElementType>
static inline JSObject *
NewArray(JSContext *cx, uint32_t nelements);

template<typename NativeType>
class TypedArrayTemplate
  : public TypedArray
{
    template<typename ElementType>
    friend JSObject *NewTypedArrayFromArray(JSContext *cx, JSObject *other);

    template<typename ElementType>
    friend JSObject *NewArray(JSContext *cx, uint32_t nelements);

  public:
    typedef NativeType ThisType;
    typedef TypedArrayTemplate<NativeType> ThisTypeArray;
    static const int ArrayTypeID() { return TypeIDOfType<NativeType>(); }
    static const bool ArrayTypeIsUnsigned() { return TypeIsUnsigned<NativeType>(); }
    static const bool ArrayTypeIsFloatingPoint() { return TypeIsFloatingPoint<NativeType>(); }
    static const bool ArrayElementTypeMayBeDouble() { return ElementTypeMayBeDouble<NativeType>(); }

    static const size_t BYTES_PER_ELEMENT = sizeof(ThisType);

    static inline Class *protoClass()
    {
        return &TypedArray::protoClasses[ArrayTypeID()];
    }

    static inline Class *fastClass()
    {
        return &TypedArray::fastClasses[ArrayTypeID()];
    }

    static void
    obj_trace(JSTracer *trc, JSObject *obj)
    {
        MarkSlot(trc, &obj->getFixedSlotRef(FIELD_BUFFER), "typedarray.buffer");
    }

    static JSBool
    obj_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name,
                    Value *vp)
    {
        JSObject *tarray = getTypedArray(obj);

        if (name == cx->runtime->atomState.lengthAtom) {
            vp->setNumber(getLength(tarray));
            return true;
        }

        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return true;
        }

        return proto->getProperty(cx, receiver, name, vp);
    }

    static JSBool
    obj_getElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
    {
        JSObject *tarray = getTypedArray(obj);

        if (index < getLength(tarray)) {
            copyIndexToValue(cx, tarray, index, vp);
            return true;
        }

        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return true;
        }

        return proto->getElement(cx, receiver, index, vp);
    }

    static JSBool
    obj_getSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
    {
        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return true;
        }

        return proto->getSpecial(cx, receiver, sid, vp);
    }

    static JSBool
    obj_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
    {
        Value idval = IdToValue(id);

        uint32_t index;
        if (IsDefinitelyIndex(idval, &index))
            return obj_getElement(cx, obj, receiver, index, vp);

        SpecialId sid;
        if (ValueIsSpecial(obj, &idval, &sid, cx))
            return obj_getSpecial(cx, obj, receiver, sid, vp);

        JSAtom *atom;
        if (!js_ValueToAtom(cx, idval, &atom))
            return false;

        if (atom->isIndex(&index))
            return obj_getElement(cx, obj, receiver, index, vp);

        return obj_getProperty(cx, obj, receiver, atom->asPropertyName(), vp);
    }

    static JSBool
    obj_getElementIfPresent(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp, bool *present)
    {
        // Fast-path the common case of index < length
        JSObject *tarray = getTypedArray(obj);

        if (index < getLength(tarray)) {
            // this inline function is specialized for each type
            copyIndexToValue(cx, tarray, index, vp);
            *present = true;
            return true;
        }

        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return true;
        }

        return proto->getElementIfPresent(cx, receiver, index, vp, present);
    }

    static bool
    setElementTail(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp, JSBool strict)
    {
        JS_ASSERT(tarray);
        JS_ASSERT(index < getLength(tarray));

        if (vp->isInt32()) {
            setIndex(tarray, index, NativeType(vp->toInt32()));
            return true;
        }

        double d;
        if (vp->isDouble()) {
            d = vp->toDouble();
        } else if (vp->isNull()) {
            d = 0.0;
        } else if (vp->isPrimitive()) {
            JS_ASSERT(vp->isString() || vp->isUndefined() || vp->isBoolean());
            if (vp->isString()) {
                JS_ALWAYS_TRUE(ToNumber(cx, *vp, &d));
            } else if (vp->isUndefined()) {
                d = js_NaN;
            } else {
                d = double(vp->toBoolean());
            }
        } else {
            // non-primitive assignments become NaN or 0 (for float/int arrays)
            d = js_NaN;
        }

        // If the array is an integer array, we only handle up to
        // 32-bit ints from this point on.  if we want to handle
        // 64-bit ints, we'll need some changes.

        // Assign based on characteristics of the destination type
        if (ArrayTypeIsFloatingPoint()) {
            setIndex(tarray, index, NativeType(d));
        } else if (ArrayTypeIsUnsigned()) {
            JS_ASSERT(sizeof(NativeType) <= 4);
            uint32_t n = ToUint32(d);
            setIndex(tarray, index, NativeType(n));
        } else if (ArrayTypeID() == TypedArray::TYPE_UINT8_CLAMPED) {
            // The uint8_clamped type has a special rounding converter
            // for doubles.
            setIndex(tarray, index, NativeType(d));
        } else {
            JS_ASSERT(sizeof(NativeType) <= 4);
            int32_t n = ToInt32(d);
            setIndex(tarray, index, NativeType(n));
        }

        return true;
    }

    static JSBool
    obj_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
    {
        JSObject *tarray = getTypedArray(obj);
        JS_ASSERT(tarray);

        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
            vp->setNumber(getLength(tarray));
            return true;
        }

        uint32_t index;
        // We can't just chain to js_SetPropertyHelper, because we're not a normal object.
        if (!isArrayIndex(cx, tarray, id, &index)) {
            // Silent ignore is better than an exception here, because
            // at some point we may want to support other properties on
            // these objects.  This is especially true when these arrays
            // are used to implement HTML Canvas 2D's PixelArray objects,
            // which used to be plain old arrays.
            vp->setUndefined();
            return true;
        }

        return setElementTail(cx, tarray, index, vp, strict);
    }

    static JSBool
    obj_setProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
    {
        return obj_setGeneric(cx, obj, ATOM_TO_JSID(name), vp, strict);
    }

    static JSBool
    obj_setElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict)
    {
        JSObject *tarray = getTypedArray(obj);
        JS_ASSERT(tarray);

        if (index >= getLength(tarray)) {
            // Silent ignore is better than an exception here, because
            // at some point we may want to support other properties on
            // these objects.  This is especially true when these arrays
            // are used to implement HTML Canvas 2D's PixelArray objects,
            // which used to be plain old arrays.
            vp->setUndefined();
            return true;
        }

        return setElementTail(cx, tarray, index, vp, strict);
    }

    static JSBool
    obj_setSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
    {
        return obj_setGeneric(cx, obj, SPECIALID_TO_JSID(sid), vp, strict);
    }

    static JSBool
    obj_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
            return true;

        Value tmp = *v;
        return obj_setGeneric(cx, obj, id, &tmp, false);
    }

    static JSBool
    obj_defineProperty(JSContext *cx, JSObject *obj, PropertyName *name, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        return obj_defineGeneric(cx, obj, ATOM_TO_JSID(name), v, getter, setter, attrs);
    }

    static JSBool
    obj_defineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Value tmp = *v;
        return obj_setElement(cx, obj, index, &tmp, false);
    }

    static JSBool
    obj_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        return obj_defineGeneric(cx, obj, SPECIALID_TO_JSID(sid), v, getter, setter, attrs);
    }

    static JSBool
    obj_deleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
    {
        if (name == cx->runtime->atomState.lengthAtom) {
            rval->setBoolean(false);
            return true;
        }

        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_deleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
    {
        JSObject *tarray = TypedArray::getTypedArray(obj);
        JS_ASSERT(tarray);

        if (index < getLength(tarray)) {
            rval->setBoolean(false);
            return true;
        }

        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_deleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
    {
        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                  Value *statep, jsid *idp)
    {
        JSObject *tarray = getTypedArray(obj);
        JS_ASSERT(tarray);

        /*
         * Iteration is "length" (if JSENUMERATE_INIT_ALL), then [0, length).
         * *statep is JSVAL_TRUE if enumerating "length" and
         * JSVAL_TO_INT(index) when enumerating index.
         */
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
            statep->setBoolean(true);
            if (idp)
                *idp = ::INT_TO_JSID(getLength(tarray) + 1);
            break;

          case JSENUMERATE_INIT:
            statep->setInt32(0);
            if (idp)
                *idp = ::INT_TO_JSID(getLength(tarray));
            break;

          case JSENUMERATE_NEXT:
            if (statep->isTrue()) {
                *idp = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
                statep->setInt32(0);
            } else {
                uint32_t index = statep->toInt32();
                if (index < getLength(tarray)) {
                    *idp = ::INT_TO_JSID(index);
                    statep->setInt32(index + 1);
                } else {
                    JS_ASSERT(index == getLength(tarray));
                    statep->setNull();
                }
            }
            break;

          case JSENUMERATE_DESTROY:
            statep->setNull();
            break;
        }

        return true;
    }

    static JSType
    obj_typeOf(JSContext *cx, JSObject *obj)
    {
        return JSTYPE_OBJECT;
    }

    static JSObject *
    createTypedArray(JSContext *cx, JSObject *bufobj, uint32_t byteOffset, uint32_t len)
    {
        JSObject *obj = NewBuiltinClassInstance(cx, protoClass());
        if (!obj)
            return NULL;
#ifdef JS_THREADSAFE
        JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT8_BACKGROUND);
#else
        JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT8);
#endif

        /*
         * Specialize the type of the object on the current scripted location,
         * and mark the type as definitely a typed array.
         */
        JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(protoClass());
        types::TypeObject *type = types::GetTypeCallerInitObject(cx, key);
        if (!type)
            return NULL;
        obj->setType(type);

        obj->setSlot(FIELD_TYPE, Int32Value(ArrayTypeID()));
        obj->setSlot(FIELD_BUFFER, ObjectValue(*bufobj));

        JS_ASSERT(bufobj->isArrayBuffer());
        ArrayBufferObject &buffer = bufobj->asArrayBuffer();

        /*
         * N.B. The base of the array's data is stored in the object's
         * private data rather than a slot, to avoid alignment restrictions
         * on private Values.
         */
        obj->setPrivate(buffer.dataPointer() + byteOffset);

        obj->setSlot(FIELD_LENGTH, Int32Value(len));
        obj->setSlot(FIELD_BYTEOFFSET, Int32Value(byteOffset));
        obj->setSlot(FIELD_BYTELENGTH, Int32Value(len * sizeof(NativeType)));

        JS_ASSERT(obj->getClass() == protoClass());

        js::Shape *empty = EmptyShape::getInitialShape(cx, fastClass(),
                                                       obj->getProto(), obj->getParent(),
                                                       gc::FINALIZE_OBJECT8,
                                                       BaseShape::NOT_EXTENSIBLE);
        if (!empty)
            return NULL;
        obj->setLastPropertyInfallible(empty);

        DebugOnly<uint32_t> bufferByteLength = buffer.byteLength();
        JS_ASSERT(bufferByteLength - getByteOffset(obj) >= getByteLength(obj));
        JS_ASSERT(getByteOffset(obj) <= bufferByteLength);
        JS_ASSERT(buffer.dataPointer() <= getDataOffset(obj));
        JS_ASSERT(getDataOffset(obj) <= offsetData(obj, bufferByteLength));

        JS_ASSERT(obj->numFixedSlots() == NUM_FIXED_SLOTS);

        return obj;
    }

    /*
     * new [Type]Array(length)
     * new [Type]Array(otherTypedArray)
     * new [Type]Array(JSArray)
     * new [Type]Array(ArrayBuffer, [optional] byteOffset, [optional] length)
     */
    static JSBool
    class_constructor(JSContext *cx, unsigned argc, Value *vp)
    {
        /* N.B. this is a constructor for protoClass, not fastClass! */
        JSObject *obj = create(cx, argc, JS_ARGV(cx, vp));
        if (!obj)
            return false;
        vp->setObject(*obj);
        return true;
    }

    static JSObject *
    create(JSContext *cx, unsigned argc, Value *argv)
    {
        /* N.B. there may not be an argv[-2]/argv[-1]. */

        /* () or (number) */
        uint32_t len = 0;
        if (argc == 0 || ValueIsLength(cx, argv[0], &len)) {
            JSObject *bufobj = createBufferWithSizeAndCount(cx, len);
            if (!bufobj)
                return NULL;

            return createTypedArray(cx, bufobj, 0, len);
        }

        /* (not an object) */
        if (!argv[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL;
        }

        JSObject *dataObj = &argv[0].toObject();

        /* (typedArray) */
        if (dataObj->isTypedArray()) {
            JSObject *otherTypedArray = getTypedArray(dataObj);
            JS_ASSERT(otherTypedArray);

            uint32_t len = getLength(otherTypedArray);
            JSObject *bufobj = createBufferWithSizeAndCount(cx, len);
            if (!bufobj)
                return NULL;

            JSObject *obj = createTypedArray(cx, bufobj, 0, len);
            if (!obj || !copyFromTypedArray(cx, obj, otherTypedArray, 0))
                return NULL;
            return obj;
        }

        /* (obj, byteOffset, length). */
        int32_t byteOffset = -1;
        int32_t length = -1;

        if (argc > 1) {
            if (!ToInt32(cx, argv[1], &byteOffset))
                return NULL;
            if (byteOffset < 0) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_TYPED_ARRAY_NEGATIVE_ARG, "1");
                return NULL;
            }

            if (argc > 2) {
                if (!ToInt32(cx, argv[2], &length))
                    return NULL;
                if (length < 0) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_TYPED_ARRAY_NEGATIVE_ARG, "2");
                    return NULL;
                }
            }
        }

        /* (obj, byteOffset, length) */
        return createTypedArrayWithOffsetLength(cx, dataObj, byteOffset, length);
    }

    /* subarray(start[, end]) */
    static JSBool
    fun_subarray(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        bool ok;
        JSObject *obj = NonGenericMethodGuard(cx, args, fun_subarray, fastClass(), &ok);
        if (!obj)
            return ok;

        JSObject *tarray = getTypedArray(obj);
        if (!tarray)
            return true;

        // these are the default values
        int32_t begin = 0, end = getLength(tarray);
        int32_t length = int32_t(getLength(tarray));

        if (args.length() > 0) {
            if (!ToClampedIndex(cx, args[0], length, &begin))
                return false;

            if (args.length() > 1) {
                if (!ToClampedIndex(cx, args[1], length, &end))
                    return false;
            }
        }

        if (begin > end)
            begin = end;

        JSObject *nobj = createSubarray(cx, tarray, begin, end);
        if (!nobj)
            return false;
        args.rval().setObject(*nobj);
        return true;
    }

    /* set(array[, offset]) */
    static JSBool
    fun_set(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        bool ok;
        JSObject *obj = NonGenericMethodGuard(cx, args, fun_set, fastClass(), &ok);
        if (!obj)
            return ok;

        JSObject *tarray = getTypedArray(obj);
        if (!tarray)
            return true;

        // these are the default values
        int32_t off = 0;

        if (args.length() > 1) {
            if (!ToInt32(cx, args[1], &off))
                return false;

            if (off < 0 || uint32_t(off) > getLength(tarray)) {
                // the given offset is bogus
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_TYPED_ARRAY_BAD_ARGS);
                return false;
            }
        }

        uint32_t offset(off);

        // first arg must be either a typed array or a JS array
        if (args.length() == 0 || !args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        JSObject *arg0 = args[0].toObjectOrNull();
        if (arg0->isTypedArray()) {
            JSObject *src = TypedArray::getTypedArray(arg0);
            if (!src ||
                getLength(src) > getLength(tarray) - offset)
            {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_TYPED_ARRAY_BAD_ARGS);
                return false;
            }

            if (!copyFromTypedArray(cx, obj, src, offset))
                return false;
        } else {
            uint32_t len;
            if (!js_GetLengthProperty(cx, arg0, &len))
                return false;

            // avoid overflow; we know that offset <= length
            if (len > getLength(tarray) - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_TYPED_ARRAY_BAD_ARGS);
                return false;
            }

            if (!copyFromArray(cx, obj, arg0, len, offset))
                return false;
        }

        args.rval().setUndefined();
        return true;
    }

  public:
    static JSObject *
    createTypedArrayWithBuffer(JSContext *cx, JSObject *bufobj,
                               int32_t byteOffsetInt, int32_t lengthInt)
    {
        uint32_t boffset = (byteOffsetInt == -1) ? 0 : uint32_t(byteOffsetInt);

        ArrayBufferObject &buffer = bufobj->asArrayBuffer();

        if (boffset > buffer.byteLength() || boffset % sizeof(NativeType) != 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL; // invalid byteOffset
        }

        uint32_t len;
        if (lengthInt == -1) {
            len = (buffer.byteLength() - boffset) / sizeof(NativeType);
            if (len * sizeof(NativeType) != buffer.byteLength() - boffset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
                return NULL; // given byte array doesn't map exactly to sizeof(NativeType) * N
            }
        } else {
            len = uint32_t(lengthInt);
        }

        // Go slowly and check for overflow.
        uint32_t arrayByteLength = len * sizeof(NativeType);
        if (len >= INT32_MAX / sizeof(NativeType) || boffset >= INT32_MAX - arrayByteLength) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL; // overflow when calculating boffset + len * sizeof(NativeType)
        }

        if (arrayByteLength + boffset > buffer.byteLength()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL; // boffset + len is too big for the arraybuffer
        }

        return createTypedArray(cx, bufobj, boffset, len);
    }

    static JSObject *
    createTypedArrayFromArray(JSContext *cx, JSObject *other)
    {
        uint32_t len;
        if (!js_GetLengthProperty(cx, other, &len))
            return NULL;

        JSObject *bufobj = createBufferWithSizeAndCount(cx, len);
        if (!bufobj)
            return NULL;

        JSObject *obj = createTypedArray(cx, bufobj, 0, len);
        if (!obj || !copyFromArray(cx, obj, other, len))
            return NULL;
        return obj;
    }

    /*
     * Note: the offset and length arguments are ignored if an array is passed in.
     */
    static JSObject *
    createTypedArrayWithOffsetLength(JSContext *cx, JSObject *other,
                                     int32_t byteOffsetInt, int32_t lengthInt)
    {
        JS_ASSERT(!other->isTypedArray());

        if (other->isArrayBuffer()) {
            /* Handle creation from an ArrayBuffer not ArrayBuffer.prototype. */
            return createTypedArrayWithBuffer(cx, other, byteOffsetInt, lengthInt);
        }

        /*
         * Otherwise create a new typed array and copy len properties from
         * the object.
         */
        return createTypedArrayFromArray(cx, other);
    }

    static const NativeType
    getIndex(JSObject *obj, uint32_t index)
    {
        return *(static_cast<const NativeType*>(getDataOffset(obj)) + index);
    }

    static void
    setIndex(JSObject *obj, uint32_t index, NativeType val)
    {
        *(static_cast<NativeType*>(getDataOffset(obj)) + index) = val;
    }

    static void copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp);

    static JSObject *
    createSubarray(JSContext *cx, JSObject *tarray, uint32_t begin, uint32_t end)
    {
        JS_ASSERT(tarray);

        JS_ASSERT(begin <= getLength(tarray));
        JS_ASSERT(end <= getLength(tarray));

        JSObject *bufobj = getBuffer(tarray);
        JS_ASSERT(bufobj);

        JS_ASSERT(begin <= end);
        uint32_t length = end - begin;

        JS_ASSERT(begin < UINT32_MAX / sizeof(NativeType));
        JS_ASSERT(UINT32_MAX - begin * sizeof(NativeType) >= getByteOffset(tarray));
        uint32_t byteOffset = getByteOffset(tarray) + begin * sizeof(NativeType);

        return createTypedArray(cx, bufobj, byteOffset, length);
    }

  protected:
    static NativeType
    nativeFromDouble(double d)
    {
        if (!ArrayTypeIsFloatingPoint() && JS_UNLIKELY(MOZ_DOUBLE_IS_NaN(d)))
            return NativeType(int32_t(0));
        if (TypeIsFloatingPoint<NativeType>())
            return NativeType(d);
        if (TypeIsUnsigned<NativeType>())
            return NativeType(ToUint32(d));
        return NativeType(ToInt32(d));
    }

    static NativeType
    nativeFromValue(JSContext *cx, const Value &v)
    {
        if (v.isInt32())
            return NativeType(v.toInt32());

        if (v.isDouble())
            return nativeFromDouble(v.toDouble());

        /*
         * The condition guarantees that holes and undefined values
         * are treated identically.
         */
        if (v.isPrimitive() && !v.isMagic() && !v.isUndefined()) {
            double dval;
            JS_ALWAYS_TRUE(ToNumber(cx, v, &dval));
            return nativeFromDouble(dval);
        }

        return ArrayTypeIsFloatingPoint()
               ? NativeType(js_NaN)
               : NativeType(int32_t(0));
    }

    static bool
    copyFromArray(JSContext *cx, JSObject *thisTypedArrayObj,
                  JSObject *ar, uint32_t len, uint32_t offset = 0)
    {
        thisTypedArrayObj = getTypedArray(thisTypedArrayObj);
        JS_ASSERT(thisTypedArrayObj);

        JS_ASSERT(offset <= getLength(thisTypedArrayObj));
        JS_ASSERT(len <= getLength(thisTypedArrayObj) - offset);
        NativeType *dest = static_cast<NativeType*>(getDataOffset(thisTypedArrayObj)) + offset;

        if (ar->isDenseArray() && ar->getDenseArrayInitializedLength() >= len) {
            JS_ASSERT(ar->getArrayLength() == len);

            const Value *src = ar->getDenseArrayElements();

            /*
             * It is valid to skip the hole check here because nativeFromValue
             * treats a hole as undefined.
             */
            for (unsigned i = 0; i < len; ++i)
                *dest++ = nativeFromValue(cx, *src++);
        } else {
            Value v;

            for (unsigned i = 0; i < len; ++i) {
                if (!ar->getElement(cx, i, &v))
                    return false;
                *dest++ = nativeFromValue(cx, v);
            }
        }

        return true;
    }

    static bool
    copyFromTypedArray(JSContext *cx, JSObject *thisTypedArrayObj, JSObject *tarray, uint32_t offset)
    {
        thisTypedArrayObj = getTypedArray(thisTypedArrayObj);
        JS_ASSERT(thisTypedArrayObj);

        JS_ASSERT(offset <= getLength(thisTypedArrayObj));
        JS_ASSERT(getLength(tarray) <= getLength(thisTypedArrayObj) - offset);
        if (getBuffer(tarray) == getBuffer(thisTypedArrayObj))
            return copyFromWithOverlap(cx, thisTypedArrayObj, tarray, offset);

        NativeType *dest = static_cast<NativeType*>((void*)getDataOffset(thisTypedArrayObj)) + offset;

        if (getType(tarray) == getType(thisTypedArrayObj)) {
            js_memcpy(dest, getDataOffset(tarray), getByteLength(tarray));
            return true;
        }

        unsigned srclen = getLength(tarray);
        switch (getType(tarray)) {
          case TypedArray::TYPE_INT8: {
            int8_t *src = static_cast<int8_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED: {
            uint8_t *src = static_cast<uint8_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT16: {
            int16_t *src = static_cast<int16_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT16: {
            uint16_t *src = static_cast<uint16_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT32: {
            int32_t *src = static_cast<int32_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT32: {
            uint32_t *src = static_cast<uint32_t*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT32: {
            float *src = static_cast<float*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT64: {
            double *src = static_cast<double*>(getDataOffset(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            JS_NOT_REACHED("copyFrom with a TypedArray of unknown type");
            break;
        }

        return true;
    }

    static bool
    copyFromWithOverlap(JSContext *cx, JSObject *self, JSObject *tarray, uint32_t offset)
    {
        JS_ASSERT(offset <= getLength(self));

        NativeType *dest = static_cast<NativeType*>(getDataOffset(self)) + offset;

        if (getType(tarray) == getType(self)) {
            memmove(dest, getDataOffset(tarray), getByteLength(tarray));
            return true;
        }

        // We have to make a copy of the source array here, since
        // there's overlap, and we have to convert types.
        void *srcbuf = cx->malloc_(getByteLength(tarray));
        if (!srcbuf)
            return false;
        js_memcpy(srcbuf, getDataOffset(tarray), getByteLength(tarray));

        switch (getType(tarray)) {
          case TypedArray::TYPE_INT8: {
            int8_t *src = (int8_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED: {
            uint8_t *src = (uint8_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT16: {
            int16_t *src = (int16_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT16: {
            uint16_t *src = (uint16_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT32: {
            int32_t *src = (int32_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT32: {
            uint32_t *src = (uint32_t*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT32: {
            float *src = (float*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT64: {
            double *src = (double*) srcbuf;
            for (unsigned i = 0; i < getLength(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            JS_NOT_REACHED("copyFromWithOverlap with a TypedArray of unknown type");
            break;
        }

        UnwantedForeground::free_(srcbuf);
        return true;
    }

    static void *
    offsetData(JSObject *obj, uint32_t offs) {
        return (void*)(((uint8_t*)getDataOffset(obj)) + offs);
    }

    static JSObject *
    createBufferWithSizeAndCount(JSContext *cx, uint32_t count)
    {
        size_t size = sizeof(NativeType);
        if (size != 0 && count >= INT32_MAX / size) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_NEED_DIET, "size and count");
            return NULL;
        }

        uint32_t bytelen = size * count;
        return ArrayBufferObject::create(cx, bytelen);
    }
};

class Int8Array : public TypedArrayTemplate<int8_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_INT8 };
    static const JSProtoKey key = JSProto_Int8Array;
    static JSFunctionSpec jsfuncs[];
};
class Uint8Array : public TypedArrayTemplate<uint8_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_UINT8 };
    static const JSProtoKey key = JSProto_Uint8Array;
    static JSFunctionSpec jsfuncs[];
};
class Int16Array : public TypedArrayTemplate<int16_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_INT16 };
    static const JSProtoKey key = JSProto_Int16Array;
    static JSFunctionSpec jsfuncs[];
};
class Uint16Array : public TypedArrayTemplate<uint16_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_UINT16 };
    static const JSProtoKey key = JSProto_Uint16Array;
    static JSFunctionSpec jsfuncs[];
};
class Int32Array : public TypedArrayTemplate<int32_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_INT32 };
    static const JSProtoKey key = JSProto_Int32Array;
    static JSFunctionSpec jsfuncs[];
};
class Uint32Array : public TypedArrayTemplate<uint32_t> {
  public:
    enum { ACTUAL_TYPE = TYPE_UINT32 };
    static const JSProtoKey key = JSProto_Uint32Array;
    static JSFunctionSpec jsfuncs[];
};
class Float32Array : public TypedArrayTemplate<float> {
  public:
    enum { ACTUAL_TYPE = TYPE_FLOAT32 };
    static const JSProtoKey key = JSProto_Float32Array;
    static JSFunctionSpec jsfuncs[];
};
class Float64Array : public TypedArrayTemplate<double> {
  public:
    enum { ACTUAL_TYPE = TYPE_FLOAT64 };
    static const JSProtoKey key = JSProto_Float64Array;
    static JSFunctionSpec jsfuncs[];
};
class Uint8ClampedArray : public TypedArrayTemplate<uint8_clamped> {
  public:
    enum { ACTUAL_TYPE = TYPE_UINT8_CLAMPED };
    static const JSProtoKey key = JSProto_Uint8ClampedArray;
    static JSFunctionSpec jsfuncs[];
};

// this default implementation is only valid for integer types
// less than 32-bits in size.
template<typename NativeType>
void
TypedArrayTemplate<NativeType>::copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp)
{
    JS_STATIC_ASSERT(sizeof(NativeType) < 4);

    vp->setInt32(getIndex(tarray, index));
}

// and we need to specialize for 32-bit integers and floats
template<>
void
TypedArrayTemplate<int32_t>::copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp)
{
    int32_t val = getIndex(tarray, index);
    vp->setInt32(val);
}

template<>
void
TypedArrayTemplate<uint32_t>::copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp)
{
    uint32_t val = getIndex(tarray, index);
    vp->setNumber(val);
}

template<>
void
TypedArrayTemplate<float>::copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp)
{
    float val = getIndex(tarray, index);
    double dval = val;

    /*
     * Doubles in typed arrays could be typed-punned arrays of integers. This
     * could allow user code to break the engine-wide invariant that only
     * canonical nans are stored into jsvals, which means user code could
     * confuse the engine into interpreting a double-typed jsval as an
     * object-typed jsval.
     *
     * This could be removed for platforms/compilers known to convert a 32-bit
     * non-canonical nan to a 64-bit canonical nan.
     */
    if (JS_UNLIKELY(MOZ_DOUBLE_IS_NaN(dval)))
        dval = js_NaN;

    vp->setDouble(dval);
}

template<>
void
TypedArrayTemplate<double>::copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp)
{
    double val = getIndex(tarray, index);

    /*
     * Doubles in typed arrays could be typed-punned arrays of integers. This
     * could allow user code to break the engine-wide invariant that only
     * canonical nans are stored into jsvals, which means user code could
     * confuse the engine into interpreting a double-typed jsval as an
     * object-typed jsval.
     */
    if (JS_UNLIKELY(MOZ_DOUBLE_IS_NaN(val)))
        val = js_NaN;

    vp->setDouble(val);
}

/***
 *** JS impl
 ***/

/*
 * ArrayBufferObject (base)
 */

Class ArrayBufferObject::protoClass = {
    "ArrayBufferPrototype",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(ARRAYBUFFER_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class js::ArrayBufferClass = {
    "ArrayBuffer",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS |
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(ARRAYBUFFER_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,           /* finalize    */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* hasInstance */
    ArrayBufferObject::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        ArrayBufferObject::obj_lookupGeneric,
        ArrayBufferObject::obj_lookupProperty,
        ArrayBufferObject::obj_lookupElement,
        ArrayBufferObject::obj_lookupSpecial,
        ArrayBufferObject::obj_defineGeneric,
        ArrayBufferObject::obj_defineProperty,
        ArrayBufferObject::obj_defineElement,
        ArrayBufferObject::obj_defineSpecial,
        ArrayBufferObject::obj_getGeneric,
        ArrayBufferObject::obj_getProperty,
        ArrayBufferObject::obj_getElement,
        ArrayBufferObject::obj_getElementIfPresent,
        ArrayBufferObject::obj_getSpecial,
        ArrayBufferObject::obj_setGeneric,
        ArrayBufferObject::obj_setProperty,
        ArrayBufferObject::obj_setElement,
        ArrayBufferObject::obj_setSpecial,
        ArrayBufferObject::obj_getGenericAttributes,
        ArrayBufferObject::obj_getPropertyAttributes,
        ArrayBufferObject::obj_getElementAttributes,
        ArrayBufferObject::obj_getSpecialAttributes,
        ArrayBufferObject::obj_setGenericAttributes,
        ArrayBufferObject::obj_setPropertyAttributes,
        ArrayBufferObject::obj_setElementAttributes,
        ArrayBufferObject::obj_setSpecialAttributes,
        ArrayBufferObject::obj_deleteProperty,
        ArrayBufferObject::obj_deleteElement,
        ArrayBufferObject::obj_deleteSpecial,
        ArrayBufferObject::obj_enumerate,
        ArrayBufferObject::obj_typeOf,
        NULL,       /* thisObject      */
        NULL,       /* clear           */
    }
};

JSPropertySpec ArrayBufferObject::jsprops[] = {
    { "byteLength",
      -1, JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY,
      ArrayBufferObject::prop_getByteLength, JS_StrictPropertyStub },
    {0,0,0,0,0}
};

JSFunctionSpec ArrayBufferObject::jsfuncs[] = {
    JS_FN("slice", ArrayBufferObject::fun_slice, 2, JSFUN_GENERIC_NATIVE),
    JS_FS_END
};

/*
 * shared TypedArray
 */

JSPropertySpec TypedArray::jsprops[] = {
    { js_length_str,
      -1, JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY,
      TypedArray::prop_getLength, JS_StrictPropertyStub },
    { "byteLength",
      -1, JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY,
      TypedArray::prop_getByteLength, JS_StrictPropertyStub },
    { "byteOffset",
      -1, JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY,
      TypedArray::prop_getByteOffset, JS_StrictPropertyStub },
    { "buffer",
      -1, JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY,
      TypedArray::prop_getBuffer, JS_StrictPropertyStub },
    {0,0,0,0,0}
};

/*
 * TypedArray boilerplate
 */

#define IMPL_TYPED_ARRAY_STATICS(_typedArray)                                  \
JSFunctionSpec _typedArray::jsfuncs[] = {                                      \
    JS_FN("subarray", _typedArray::fun_subarray, 2, JSFUN_GENERIC_NATIVE),     \
    JS_FN("set", _typedArray::fun_set, 2, JSFUN_GENERIC_NATIVE),               \
    JS_FS_END                                                                  \
}

template<typename ElementType>
static inline JSObject *
NewArray(JSContext *cx, uint32_t nelements)
{
    JSObject *buffer = TypedArrayTemplate<ElementType>::createBufferWithSizeAndCount(cx, nelements);
    if (!buffer)
        return NULL;
    return TypedArrayTemplate<ElementType>::createTypedArray(cx, buffer, 0, nelements);
}

template<typename ElementType>
static inline JSObject *
NewArrayWithBuffer(JSContext *cx, JSObject *arrayBuffer, int32_t byteoffset, int32_t intLength)
{
    if (!arrayBuffer->isArrayBuffer()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return NULL; // must be arrayBuffer
    }

    return TypedArrayTemplate<ElementType>::createTypedArrayWithBuffer(cx, arrayBuffer,
                                                                       byteoffset, intLength);
}

#define IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Name,NativeType)                                 \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## Array(JSContext *cx, uint32_t nelements)       \
  {                                                                                          \
      MOZ_ASSERT(nelements <= INT32_MAX);                                                    \
      return NewArray<NativeType>(cx, nelements);                                            \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayFromArray(JSContext *cx, JSObject *other) \
  {                                                                                          \
      return TypedArrayTemplate<NativeType>::createTypedArrayFromArray(cx, other);           \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayWithBuffer(JSContext *cx,                 \
                               JSObject *arrayBuffer, uint32_t byteoffset, int32_t length)   \
  {                                                                                          \
      MOZ_ASSERT(byteoffset <= INT32_MAX);                                                   \
      return NewArrayWithBuffer<NativeType>(cx, arrayBuffer, byteoffset, length);            \
  }                                                                                          \
  JS_FRIEND_API(JSBool) JS_Is ## Name ## Array(JSObject *obj, JSContext *cx)                 \
  {                                                                                          \
      obj = UnwrapObject(obj);                                                               \
      Class *clasp = obj->getClass();                                                        \
      return (clasp == &TypedArray::fastClasses[TypedArrayTemplate<NativeType>::ArrayTypeID()]); \
  }

IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int8, int8_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint8, uint8_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint8Clamped, uint8_clamped)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int16, int16_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint16, uint16_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int32, int32_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint32, uint32_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Float32, float)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Float64, double)

#define IMPL_TYPED_ARRAY_PROTO_CLASS(_typedArray)                               \
{                                                                              \
    #_typedArray,                                                              \
    JSCLASS_HAS_RESERVED_SLOTS(TypedArray::FIELD_MAX) |                        \
    JSCLASS_HAS_PRIVATE |                                                      \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_typedArray),                           \
    JS_PropertyStub,         /* addProperty */                                 \
    JS_PropertyStub,         /* delProperty */                                 \
    JS_PropertyStub,         /* getProperty */                                 \
    JS_StrictPropertyStub,   /* setProperty */                                 \
    JS_EnumerateStub,                                                          \
    JS_ResolveStub,                                                            \
    JS_ConvertStub                                                             \
}

#define IMPL_TYPED_ARRAY_FAST_CLASS(_typedArray)                               \
{                                                                              \
    #_typedArray,                                                              \
    JSCLASS_HAS_RESERVED_SLOTS(TypedArray::FIELD_MAX) |                        \
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |                        \
    JSCLASS_FOR_OF_ITERATION |                                                 \
    Class::NON_NATIVE,                                                         \
    JS_PropertyStub,         /* addProperty */                                 \
    JS_PropertyStub,         /* delProperty */                                 \
    JS_PropertyStub,         /* getProperty */                                 \
    JS_StrictPropertyStub,   /* setProperty */                                 \
    JS_EnumerateStub,                                                          \
    JS_ResolveStub,                                                            \
    JS_ConvertStub,                                                            \
    NULL,                    /* finalize    */                                 \
    NULL,                    /* checkAccess */                                 \
    NULL,                    /* call        */                                 \
    NULL,                    /* construct   */                                 \
    NULL,                    /* hasInstance */                                 \
    _typedArray::obj_trace,  /* trace       */                                 \
    {                                                                          \
        NULL,       /* equality    */                                          \
        NULL,       /* outerObject */                                          \
        NULL,       /* innerObject */                                          \
        JS_ElementIteratorStub,                                                \
        NULL,       /* unused      */                                          \
        false,      /* isWrappedNative */                                      \
    },                                                                         \
    {                                                                          \
        _typedArray::obj_lookupGeneric,                                        \
        _typedArray::obj_lookupProperty,                                       \
        _typedArray::obj_lookupElement,                                        \
        _typedArray::obj_lookupSpecial,                                        \
        _typedArray::obj_defineGeneric,                                        \
        _typedArray::obj_defineProperty,                                       \
        _typedArray::obj_defineElement,                                        \
        _typedArray::obj_defineSpecial,                                        \
        _typedArray::obj_getGeneric,                                           \
        _typedArray::obj_getProperty,                                          \
        _typedArray::obj_getElement,                                           \
        _typedArray::obj_getElementIfPresent,                                  \
        _typedArray::obj_getSpecial,                                           \
        _typedArray::obj_setGeneric,                                           \
        _typedArray::obj_setProperty,                                          \
        _typedArray::obj_setElement,                                           \
        _typedArray::obj_setSpecial,                                           \
        _typedArray::obj_getGenericAttributes,                                 \
        _typedArray::obj_getPropertyAttributes,                                \
        _typedArray::obj_getElementAttributes,                                 \
        _typedArray::obj_getSpecialAttributes,                                 \
        _typedArray::obj_setGenericAttributes,                                 \
        _typedArray::obj_setPropertyAttributes,                                \
        _typedArray::obj_setElementAttributes,                                 \
        _typedArray::obj_setSpecialAttributes,                                 \
        _typedArray::obj_deleteProperty,                                       \
        _typedArray::obj_deleteElement,                                        \
        _typedArray::obj_deleteSpecial,                                        \
        _typedArray::obj_enumerate,                                            \
        _typedArray::obj_typeOf,                                               \
        NULL,                /* thisObject  */                                 \
        NULL,                /* clear       */                                 \
    }                                                                          \
}

template<class ArrayType>
static inline JSObject *
InitTypedArrayClass(JSContext *cx, GlobalObject *global)
{
    JSObject *proto = global->createBlankPrototype(cx, ArrayType::protoClass());
    if (!proto)
        return NULL;

    JSFunction *ctor =
        global->createConstructor(cx, ArrayType::class_constructor,
                                  cx->runtime->atomState.classAtoms[ArrayType::key], 3);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    if (!ctor->defineProperty(cx, cx->runtime->atomState.BYTES_PER_ELEMENTAtom,
                              Int32Value(ArrayType::BYTES_PER_ELEMENT),
                              JS_PropertyStub, JS_StrictPropertyStub,
                              JSPROP_PERMANENT | JSPROP_READONLY) ||
        !proto->defineProperty(cx, cx->runtime->atomState.BYTES_PER_ELEMENTAtom,
                               Int32Value(ArrayType::BYTES_PER_ELEMENT),
                               JS_PropertyStub, JS_StrictPropertyStub,
                               JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return NULL;
    }

    if (!DefinePropertiesAndBrand(cx, proto, ArrayType::jsprops, ArrayType::jsfuncs))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, ArrayType::key, ctor, proto))
        return NULL;

    return proto;
}

IMPL_TYPED_ARRAY_STATICS(Int8Array);
IMPL_TYPED_ARRAY_STATICS(Uint8Array);
IMPL_TYPED_ARRAY_STATICS(Int16Array);
IMPL_TYPED_ARRAY_STATICS(Uint16Array);
IMPL_TYPED_ARRAY_STATICS(Int32Array);
IMPL_TYPED_ARRAY_STATICS(Uint32Array);
IMPL_TYPED_ARRAY_STATICS(Float32Array);
IMPL_TYPED_ARRAY_STATICS(Float64Array);
IMPL_TYPED_ARRAY_STATICS(Uint8ClampedArray);

Class TypedArray::fastClasses[TYPE_MAX] = {
    IMPL_TYPED_ARRAY_FAST_CLASS(Int8Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint8Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Int16Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint16Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Int32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Float32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Float64Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint8ClampedArray)
};

Class TypedArray::protoClasses[TYPE_MAX] = {
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int8Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int16Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint16Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float64Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8ClampedArray)
};

static JSObject *
InitArrayBufferClass(JSContext *cx, GlobalObject *global)
{
    JSObject *arrayBufferProto = global->createBlankPrototype(cx, &ArrayBufferObject::protoClass);
    if (!arrayBufferProto)
        return NULL;

    JSFunction *ctor =
        global->createConstructor(cx, ArrayBufferObject::class_constructor,
                                  CLASS_ATOM(cx, ArrayBuffer), 1);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, arrayBufferProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, arrayBufferProto, ArrayBufferObject::jsprops, ArrayBufferObject::jsfuncs))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_ArrayBuffer, ctor, arrayBufferProto))
        return NULL;

    return arrayBufferProto;
}

JSObject *
js_InitTypedArrayClasses(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    GlobalObject *global = &obj->asGlobal();

    /* Idempotency required: we initialize several things, possibly lazily. */
    JSObject *stop;
    if (!js_GetClassObject(cx, global, JSProto_ArrayBuffer, &stop))
        return NULL;
    if (stop)
        return stop;

    if (!InitTypedArrayClass<Int8Array>(cx, global) ||
        !InitTypedArrayClass<Uint8Array>(cx, global) ||
        !InitTypedArrayClass<Int16Array>(cx, global) ||
        !InitTypedArrayClass<Uint16Array>(cx, global) ||
        !InitTypedArrayClass<Int32Array>(cx, global) ||
        !InitTypedArrayClass<Uint32Array>(cx, global) ||
        !InitTypedArrayClass<Float32Array>(cx, global) ||
        !InitTypedArrayClass<Float64Array>(cx, global) ||
        !InitTypedArrayClass<Uint8ClampedArray>(cx, global))
    {
        return NULL;
    }

    return InitArrayBufferClass(cx, global);
}

bool
js::IsFastTypedArrayClass(const Class *clasp)
{
    return &TypedArray::fastClasses[0] <= clasp &&
           clasp < &TypedArray::fastClasses[TypedArray::TYPE_MAX];
}

static inline bool
IsSlowTypedArrayClass(const Class *clasp)
{
    return &TypedArray::protoClasses[0] <= clasp &&
           clasp < &TypedArray::protoClasses[TypedArray::TYPE_MAX];
}

bool
js::IsFastOrSlowTypedArray(JSObject *obj)
{
    Class *clasp = obj->getClass();
    return IsFastTypedArrayClass(clasp) || IsSlowTypedArrayClass(clasp);
}

/* JS Friend API */

JS_FRIEND_API(JSBool)
JS_IsArrayBufferObject(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    return obj->isArrayBuffer();
}

JS_FRIEND_API(JSBool)
JS_IsTypedArrayObject(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    return obj->isTypedArray();
}

JS_FRIEND_API(JSBool)
JS_IsArrayBufferViewObject(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    return obj->isTypedArray();
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    return obj->asArrayBuffer().byteLength();
}

JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    return obj->asArrayBuffer().dataPointer();
}

JS_FRIEND_API(JSObject *)
JS_NewArrayBuffer(JSContext *cx, uint32_t nbytes)
{
    JS_ASSERT(nbytes <= INT32_MAX);
    return ArrayBufferObject::create(cx, nbytes);
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayLength(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_LENGTH).toInt32();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteOffset(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_BYTEOFFSET).toInt32();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteLength(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_BYTELENGTH).toInt32();
}

JS_FRIEND_API(JSArrayBufferViewType)
JS_GetTypedArrayType(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return static_cast<JSArrayBufferViewType>(obj->getSlot(TypedArray::FIELD_TYPE).toInt32());
}

JS_FRIEND_API(int8_t *)
JS_GetInt8ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT8);
    return static_cast<int8_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT8);
    return static_cast<uint8_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ClampedArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT8_CLAMPED);
    return static_cast<uint8_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(int16_t *)
JS_GetInt16ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT16);
    return static_cast<int16_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(uint16_t *)
JS_GetUint16ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT16);
    return static_cast<uint16_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(int32_t *)
JS_GetInt32ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT32);
    return static_cast<int32_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(uint32_t *)
JS_GetUint32ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT32);
    return static_cast<uint32_t *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(float *)
JS_GetFloat32ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_FLOAT32);
    return static_cast<float *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(double *)
JS_GetFloat64ArrayData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_FLOAT64);
    return static_cast<double *>(TypedArray::getDataOffset(obj));
}

JS_FRIEND_API(void *)
JS_GetArrayBufferViewData(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return TypedArray::getDataOffset(obj);
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject *obj, JSContext *cx)
{
    obj = UnwrapObject(obj);
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_BYTELENGTH).toInt32();
}
