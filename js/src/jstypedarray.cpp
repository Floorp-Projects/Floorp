/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "mozilla/FloatingPoint.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jscpucfg.h"
#include "jsversion.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jstypedarray.h"

#include "gc/Marking.h"
#include "mozilla/Util.h"
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
 * the output value is in the range [0, length].
 */
static bool
ToClampedIndex(JSContext *cx, const Value &v, uint32_t length, uint32_t *out)
{
    int32_t result;
    if (!ToInt32(cx, v, &result))
        return false;
    if (result < 0) {
        result += length;
        if (result < 0)
            result = 0;
    } else if (uint32_t(result) > length) {
        result = length;
    }
    *out = uint32_t(result);
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
 * Walks up the prototype chain to find the actual ArrayBuffer data, if any.
 */
static ArrayBufferObject *
getArrayBuffer(JSObject *obj)
{
    while (obj && !obj->isArrayBuffer())
        obj = obj->getProto();
    return obj ? &obj->asArrayBuffer() : NULL;
}

JSBool
ArrayBufferObject::byteLengthGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, byteLengthGetter, &ArrayBufferClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    JS_SET_RVAL(cx, vp, Int32Value(thisObj->asArrayBuffer().byteLength()));
    return true;
}

JSBool
ArrayBufferObject::fun_slice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_slice, &ArrayBufferClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    ArrayBufferObject &arrayBuffer = thisObj->asArrayBuffer();

    // these are the default values
    uint32_t length = arrayBuffer.byteLength();
    uint32_t begin = 0, end = length;

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

JSObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, uint8_t *contents)
{
    SkipRoot skip(cx, &contents);

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ArrayBufferObject::protoClass));
    if (!obj)
        return NULL;
    JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT16_BACKGROUND);
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
ArrayBufferObject::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                     JSObject **objp, JSProperty **propp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        *propp = PROPERTY_FOUND;
        *objp = getArrayBuffer(obj);
        return true;
    }

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
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
ArrayBufferObject::obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                      JSObject **objp, JSProperty **propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
ArrayBufferObject::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                     JSObject **objp, JSProperty **propp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
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
ArrayBufferObject::obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                     JSObject **objp, JSProperty **propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
ArrayBufferObject::obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom))
        return true;

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineGeneric(cx, delegate, id, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineProperty(JSContext *cx, HandleObject obj,
                                      HandlePropertyName name, const Value *v,
                                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineElement(cx, delegate, index, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, const Value *v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

JSBool
ArrayBufferObject::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                                  HandleId id, Value *vp)
{
    RootedObject nobj(cx, getArrayBuffer(obj));
    JS_ASSERT(nobj);

    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        vp->setInt32(nobj->asArrayBuffer().byteLength());
        return true;
    }

    nobj = ArrayBufferDelegate(cx, nobj);
    if (!nobj)
        return false;
    return baseops::GetProperty(cx, nobj, receiver, id, vp);
}

JSBool
ArrayBufferObject::obj_getProperty(JSContext *cx, HandleObject obj,
                                   HandleObject receiver, HandlePropertyName name, Value *vp)
{
    RootedObject nobj(cx, getArrayBuffer(obj));

    if (!nobj) {
        JSAutoByteString bs(cx, name);
        if (!bs)
            return false;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO, "ArrayBuffer", bs.ptr(), "object");
        return false;
    }

    if (name == cx->runtime->atomState.byteLengthAtom) {
        vp->setInt32(nobj->asArrayBuffer().byteLength());
        return true;
    }

    nobj = ArrayBufferDelegate(cx, nobj);
    if (!nobj)
        return false;
    Rooted<jsid> id(cx, NameToId(name));
    return baseops::GetProperty(cx, nobj, receiver, id, vp);
}

JSBool
ArrayBufferObject::obj_getElement(JSContext *cx, HandleObject obj,
                                  HandleObject receiver, uint32_t index, Value *vp)
{
    RootedObject buffer(cx, getArrayBuffer(obj));
    RootedObject delegate(cx, ArrayBufferDelegate(cx, buffer));
    if (!delegate)
        return false;
    return baseops::GetElement(cx, delegate, receiver, index, vp);
}

JSBool
ArrayBufferObject::obj_getElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver,
                                           uint32_t index, Value *vp, bool *present)
{
    RootedObject buffer(cx, getArrayBuffer(obj));
    RootedObject delegate(cx, ArrayBufferDelegate(cx, buffer));
    if (!delegate)
        return false;
    return delegate->getElementIfPresent(cx, receiver, index, vp, present);
}

JSBool
ArrayBufferObject::obj_getSpecial(JSContext *cx, HandleObject obj,
                                  HandleObject receiver, HandleSpecialId sid, Value *vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

JSBool
ArrayBufferObject::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom))
        return true;

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
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

        RootedObject oldDelegateProto(cx, delegate->getProto());

        if (!baseops::SetPropertyHelper(cx, delegate, delegate, id, 0, vp, strict))
            return false;

        if (delegate->getProto() != oldDelegateProto) {
            // actual __proto__ was set and not a plain property called
            // __proto__
            if (!obj->isExtensible()) {
                obj->reportNotExtensible(cx);
                return false;
            }
            Rooted<JSObject*> newProto(cx, vp->toObjectOrNull());
            if (!SetProto(cx, obj, newProto, true)) {
                // this can be caused for example by setting x.__proto__ = x
                // restore delegate prototype chain
                SetProto(cx, delegate, oldDelegateProto, true);
                return false;
            }
        }
        return true;
    }

    return baseops::SetPropertyHelper(cx, delegate, obj, id, 0, vp, strict);
}

JSBool
ArrayBufferObject::obj_setProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name, Value *vp, JSBool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

JSBool
ArrayBufferObject::obj_setElement(JSContext *cx, HandleObject obj,
                                  uint32_t index, Value *vp, JSBool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    return baseops::SetElementHelper(cx, delegate, obj, index, 0, vp, strict);
}

JSBool
ArrayBufferObject::obj_setSpecial(JSContext *cx, HandleObject obj,
                                  HandleSpecialId sid, Value *vp, JSBool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

JSBool
ArrayBufferObject::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                            HandleId id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY;
        return true;
    }

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::GetAttributes(cx, delegate, id, attrsp);
}

JSBool
ArrayBufferObject::obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                             HandlePropertyName name, unsigned *attrsp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
ArrayBufferObject::obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                            uint32_t index, unsigned *attrsp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::GetElementAttributes(cx, delegate, index, attrsp);
}

JSBool
ArrayBufferObject::obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                            HandleSpecialId sid, unsigned *attrsp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
ArrayBufferObject::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                            HandleId id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.byteLengthAtom)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_SET_ARRAY_ATTRS);
        return false;
    }

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::SetAttributes(cx, delegate, id, attrsp);
}

JSBool
ArrayBufferObject::obj_setPropertyAttributes(JSContext *cx, HandleObject obj,
                                             HandlePropertyName name, unsigned *attrsp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_setGenericAttributes(cx, obj, id, attrsp);
}

JSBool
ArrayBufferObject::obj_setElementAttributes(JSContext *cx, HandleObject obj,
                                            uint32_t index, unsigned *attrsp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::SetElementAttributes(cx, delegate, index, attrsp);
}

JSBool
ArrayBufferObject::obj_setSpecialAttributes(JSContext *cx, HandleObject obj,
                                            HandleSpecialId sid, unsigned *attrsp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGenericAttributes(cx, obj, id, attrsp);
}

JSBool
ArrayBufferObject::obj_deleteProperty(JSContext *cx, HandleObject obj,
                                      HandlePropertyName name, Value *rval, JSBool strict)
{
    if (name == cx->runtime->atomState.byteLengthAtom) {
        rval->setBoolean(false);
        return true;
    }

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteProperty(cx, delegate, name, rval, strict);
}

JSBool
ArrayBufferObject::obj_deleteElement(JSContext *cx, HandleObject obj,
                                     uint32_t index, Value *rval, JSBool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteElement(cx, delegate, index, rval, strict);
}

JSBool
ArrayBufferObject::obj_deleteSpecial(JSContext *cx, HandleObject obj,
                                     HandleSpecialId sid, Value *rval, JSBool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteSpecial(cx, delegate, sid, rval, strict);
}

JSBool
ArrayBufferObject::obj_enumerate(JSContext *cx, HandleObject obj,
                                 JSIterateOp enum_op, Value *statep, jsid *idp)
{
    statep->setNull();
    return true;
}

JSType
ArrayBufferObject::obj_typeOf(JSContext *cx, HandleObject obj)
{
    return JSTYPE_OBJECT;
}

/*
 * ArrayBufferViews of various sorts
 */

static JSObject *
GetProtoForClass(JSContext *cx, Class *clasp)
{
    // Pass in the proto from this compartment
    Rooted<GlobalObject*> parent(cx, GetCurrentGlobal(cx));
    JSObject *proto;
    if (!FindProto(cx, clasp, parent, &proto))
        return NULL;
    return proto;
}

/*
 * TypedArray
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

static JSObject *
getTypedArray(JSObject *obj)
{
    MOZ_ASSERT(obj);
    do {
        if (obj->isTypedArray())
            return obj;
    } while ((obj = obj->getProto()));
    return NULL;
}

inline bool
TypedArray::isArrayIndex(JSContext *cx, JSObject *obj, jsid id, uint32_t *ip)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index) && index < length(obj)) {
        if (ip)
            *ip = index;
        return true;
    }

    return false;
}

bool
js::IsDataView(JSObject* obj)
{
    JS_ASSERT(obj);
    return obj->isDataView();
}

JSBool
TypedArray::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
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
TypedArray::obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                               JSObject **objp, JSProperty **propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
TypedArray::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                              JSObject **objp, JSProperty **propp)
{
    JSObject *tarray = getTypedArray(obj);
    JS_ASSERT(tarray);

    if (index < length(tarray)) {
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
TypedArray::obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                              JSObject **objp, JSProperty **propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
TypedArray::obj_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    *attrsp = (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
              ? JSPROP_PERMANENT | JSPROP_READONLY
              : JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

JSBool
TypedArray::obj_getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
TypedArray::obj_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

JSBool
TypedArray::obj_setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
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
        return &TypedArray::classes[ArrayTypeID()];
    }

    static void
    obj_trace(JSTracer *trc, JSObject *obj)
    {
        MarkSlot(trc, &obj->getFixedSlotRef(FIELD_BUFFER), "typedarray.buffer");
    }

    static JSBool
    obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                    Value *vp)
    {
        JSObject *tarray = getTypedArray(obj);

        if (name == cx->runtime->atomState.lengthAtom) {
            *vp = lengthValue(tarray);
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
    obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp)
    {
        JSObject *tarray = getTypedArray(obj);

        if (index < length(tarray)) {
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
    obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid, Value *vp)
    {
        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return true;
        }

        return proto->getSpecial(cx, receiver, sid, vp);
    }

    static JSBool
    obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, Value *vp)
    {
        Value idval = IdToValue(id);

        uint32_t index;
        if (IsDefinitelyIndex(idval, &index))
            return obj_getElement(cx, obj, receiver, index, vp);

        Rooted<SpecialId> sid(cx);
        if (ValueIsSpecial(obj, &idval, sid.address(), cx))
            return obj_getSpecial(cx, obj, receiver, sid, vp);

        JSAtom *atom = ToAtom(cx, idval);
        if (!atom)
            return false;

        if (atom->isIndex(&index))
            return obj_getElement(cx, obj, receiver, index, vp);

        Rooted<PropertyName*> name(cx, atom->asPropertyName());
        return obj_getProperty(cx, obj, receiver, name, vp);
    }

    static JSBool
    obj_getElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp, bool *present)
    {
        // Fast-path the common case of index < length
        JSObject *tarray = getTypedArray(obj);

        if (index < length(tarray)) {
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
    toDoubleForTypedArray(JSContext *cx, Value *vp, double *d)
    {
        if (vp->isDouble()) {
            *d = vp->toDouble();
        } else if (vp->isNull()) {
            *d = 0.0;
        } else if (vp->isPrimitive()) {
            JS_ASSERT(vp->isString() || vp->isUndefined() || vp->isBoolean());
            if (vp->isString()) {
                if (!ToNumber(cx, *vp, d))
                    return false;
            } else if (vp->isUndefined()) {
                *d = js_NaN;
            } else {
                *d = double(vp->toBoolean());
            }
        } else {
            // non-primitive assignments become NaN or 0 (for float/int arrays)
            *d = js_NaN;
        }

        return true;
    }

    static bool
    setElementTail(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp, JSBool strict)
    {
        JS_ASSERT(tarray);
        JS_ASSERT(index < length(tarray));

        if (vp->isInt32()) {
            setIndex(tarray, index, NativeType(vp->toInt32()));
            return true;
        }

        double d;
        if (!toDoubleForTypedArray(cx, vp, &d))
            return false;

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
    obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict)
    {
        RootedObject tarray(cx, getTypedArray(obj));
        JS_ASSERT(tarray);

        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
            vp->setNumber(length(tarray));
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
    obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *vp, JSBool strict)
    {
        Rooted<jsid> id(cx, NameToId(name));
        return obj_setGeneric(cx, obj, id, vp, strict);
    }

    static JSBool
    obj_setElement(JSContext *cx, HandleObject obj, uint32_t index, Value *vp, JSBool strict)
    {
        RootedObject tarray(cx, getTypedArray(obj));
        JS_ASSERT(tarray);

        if (index >= length(tarray)) {
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
    obj_setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *vp, JSBool strict)
    {
        Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
        return obj_setGeneric(cx, obj, id, vp, strict);
    }

    static JSBool
    obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
            return true;

        Value tmp = *v;
        return obj_setGeneric(cx, obj, id, &tmp, false);
    }

    static JSBool
    obj_defineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Rooted<jsid> id(cx, NameToId(name));
        return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
    }

    static JSBool
    obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, const Value *v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Value tmp = *v;
        return obj_setElement(cx, obj, index, &tmp, false);
    }

    static JSBool
    obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, const Value *v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
        return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
    }

    static JSBool
    obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *rval, JSBool strict)
    {
        if (name == cx->runtime->atomState.lengthAtom) {
            rval->setBoolean(false);
            return true;
        }

        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index, Value *rval, JSBool strict)
    {
        JSObject *tarray = getTypedArray(obj);
        JS_ASSERT(tarray);

        if (index < length(tarray)) {
            rval->setBoolean(false);
            return true;
        }

        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *rval, JSBool strict)
    {
        rval->setBoolean(true);
        return true;
    }

    static JSBool
    obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
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
                *idp = ::INT_TO_JSID(length(tarray) + 1);
            break;

          case JSENUMERATE_INIT:
            statep->setInt32(0);
            if (idp)
                *idp = ::INT_TO_JSID(length(tarray));
            break;

          case JSENUMERATE_NEXT:
            if (statep->isTrue()) {
                *idp = NameToId(cx->runtime->atomState.lengthAtom);
                statep->setInt32(0);
            } else {
                uint32_t index = statep->toInt32();
                if (index < length(tarray)) {
                    *idp = ::INT_TO_JSID(index);
                    statep->setInt32(index + 1);
                } else {
                    JS_ASSERT(index == length(tarray));
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
    obj_typeOf(JSContext *cx, HandleObject obj)
    {
        return JSTYPE_OBJECT;
    }

    static JSObject *
    makeInstance(JSContext *cx, HandleObject bufobj, uint32_t byteOffset, uint32_t len,
                 HandleObject proto)
    {
        RootedObject obj(cx, NewBuiltinClassInstance(cx, protoClass()));
        if (!obj)
            return NULL;
        JS_ASSERT(obj->getAllocKind() == gc::FINALIZE_OBJECT8_BACKGROUND);

        if (proto) {
            types::TypeObject *type = proto->getNewType(cx);
            if (!type)
                return NULL;
            obj->setType(type);
        } else if (cx->typeInferenceEnabled()) {
            if (len * sizeof(NativeType) >= TypedArray::SINGLETON_TYPE_BYTE_LENGTH) {
                if (!obj->setSingletonType(cx))
                    return NULL;
            } else {
                jsbytecode *pc;
                RootedScript script(cx, cx->stack.currentScript(&pc));
                if (script) {
                    if (!types::SetInitializerObjectType(cx, script, pc, obj))
                        return NULL;
                }
            }
        }

        obj->setSlot(FIELD_TYPE, Int32Value(ArrayTypeID()));
        obj->setSlot(FIELD_BUFFER, ObjectValue(*bufobj));

        JS_ASSERT(bufobj->isArrayBuffer());
        Rooted<ArrayBufferObject *> buffer(cx, &bufobj->asArrayBuffer());

        /*
         * N.B. The base of the array's data is stored in the object's
         * private data rather than a slot, to avoid alignment restrictions
         * on private Values.
         */
        obj->setPrivate(buffer->dataPointer() + byteOffset);

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

#ifdef DEBUG
        uint32_t bufferByteLength = buffer->byteLength();
        uint32_t arrayByteLength = static_cast<uint32_t>(byteLengthValue(obj).toInt32());
        uint32_t arrayByteOffset = static_cast<uint32_t>(byteOffsetValue(obj).toInt32());
        JS_ASSERT(buffer->dataPointer() <= viewData(obj));
        JS_ASSERT(bufferByteLength - byteOffsetValue(obj).toInt32() >= arrayByteLength);
        JS_ASSERT(arrayByteOffset <= bufferByteLength);

        JS_ASSERT(obj->numFixedSlots() == NUM_FIXED_SLOTS);
#endif

        return obj;
    }

    static JSObject *
    makeInstance(JSContext *cx, HandleObject bufobj, uint32_t byteOffset, uint32_t len)
    {
        RootedObject nullproto(cx, NULL);
        return makeInstance(cx, bufobj, byteOffset, len, nullproto);
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

    static JSBool
    fromBuffer(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        RootedObject buffer(cx, &args[0].toObject());
        RootedObject proto(cx, &args[3].toObject());
        JSObject *obj = fromBuffer(cx, buffer, args[1].toInt32(), args[2].toInt32(), proto);
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
        if (argc == 0 || ValueIsLength(cx, argv[0], &len))
            return fromLength(cx, len);

        /* (not an object) */
        if (!argv[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL;
        }

        RootedObject dataObj(cx, &argv[0].toObject());

        /*
         * (typedArray)
         * (type[] array)
         *
         * Otherwise create a new typed array and copy elements 0..len-1
         * properties from the object, treating it as some sort of array.
         * Note that offset and length will be ignored
         */
        if (!UnwrapObject(dataObj)->isArrayBuffer())
            return fromArray(cx, dataObj);

        /* (ArrayBuffer, [byteOffset, [length]]) */
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

        Rooted<JSObject*> proto(cx, NULL);
        return fromBuffer(cx, dataObj, byteOffset, length, proto);
    }

    // ValueGetter is a function that takes an unwrapped typed array object and
    // returns a Value. Given such a function, Getter<> is a native that
    // retrieves a given Value, probably from a slot on the object.
    template<Value ValueGetter(JSObject *)>
    static JSBool
    Getter(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        JSObject *thisObj;
        if (!NonGenericMethodGuard(cx, args, Getter<ValueGetter>, fastClass(), &thisObj))
            return false;
        if (!thisObj)
            return true;

        JS_SET_RVAL(cx, vp, ValueGetter(thisObj));
        return true;
    }

    // Define an accessor for a read-only property that invokes a native getter
    template<Value ValueGetter(JSObject *)>
    static bool
    DefineGetter(JSContext *cx, PropertyName *name, HandleObject proto)
    {
        RootedId id(cx, NameToId(name));
        unsigned flags = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_SHARED | JSPROP_GETTER;

        Rooted<GlobalObject*> global(cx, &cx->compartment->global());
        JSObject *getter = js_NewFunction(cx, NULL, Getter<ValueGetter>, 0, 0, global, NULL);
        if (!getter)
            return false;

        return DefineNativeProperty(cx, proto, id, UndefinedValue(),
                                    JS_DATA_TO_FUNC_PTR(PropertyOp, getter), NULL,
                                    flags, 0, 0);
    }

    static
    bool defineGetters(JSContext *cx, HandleObject proto)
    {
        if (!DefineGetter<lengthValue>(cx, cx->runtime->atomState.lengthAtom, proto))
            return false;

        if (!DefineGetter<bufferValue>(cx, cx->runtime->atomState.bufferAtom, proto))
            return false;

        if (!DefineGetter<byteLengthValue>(cx, cx->runtime->atomState.byteLengthAtom, proto))
            return false;

        if (!DefineGetter<byteOffsetValue>(cx, cx->runtime->atomState.byteOffsetAtom, proto))
            return false;

        return true;
    }

    /* subarray(start[, end]) */
    static JSBool
    fun_subarray(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        JSObject *thisObj;
        if (!NonGenericMethodGuard(cx, args, fun_subarray, fastClass(), &thisObj))
            return false;
        if (!thisObj)
            return true;

        JSObject *tarray = getTypedArray(thisObj);
        if (!tarray)
            return true;

        // these are the default values
        uint32_t begin = 0, end = length(tarray);
        uint32_t length = TypedArray::length(tarray);

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

    /* move(begin, end, dest) */
    static JSBool
    fun_move(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        JSObject *obj;
        if (!NonGenericMethodGuard(cx, args, fun_move, fastClass(), &obj))
            return false;
        if (!obj)
            return true;

        if (args.length() < 3) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t srcBegin;
        uint32_t srcEnd;
        uint32_t dest;

        JSObject *tarray = getTypedArray(obj);
        uint32_t length = TypedArray::length(tarray);
        if (!ToClampedIndex(cx, args[0], length, &srcBegin) ||
            !ToClampedIndex(cx, args[1], length, &srcEnd) ||
            !ToClampedIndex(cx, args[2], length, &dest) ||
            srcBegin > srcEnd)
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t nelts = srcEnd - srcBegin;

        JS_ASSERT(dest + nelts >= dest);
        if (dest + nelts > length) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t byteDest = dest * sizeof(NativeType);
        uint32_t byteSrc = srcBegin * sizeof(NativeType);
        uint32_t byteSize = nelts * sizeof(NativeType);

#ifdef DEBUG
        uint32_t viewByteLength = byteLengthValue(tarray).toInt32();
        JS_ASSERT(byteDest <= viewByteLength);
        JS_ASSERT(byteSrc <= viewByteLength);
        JS_ASSERT(byteDest + byteSize <= viewByteLength);
        JS_ASSERT(byteSrc + byteSize <= viewByteLength);

        // Should not overflow because size is limited to 2^31
        JS_ASSERT(byteDest + byteSize >= byteDest);
        JS_ASSERT(byteSrc + byteSize >= byteSrc);
#endif

        uint8_t *data = static_cast<uint8_t*>(viewData(tarray));
        memmove(&data[byteDest], &data[byteSrc], byteSize);
        args.rval().setUndefined();
        return true;
    }

    /* set(array[, offset]) */
    static JSBool
    fun_set(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        RootedObject thisObj(cx);
        if (!NonGenericMethodGuard(cx, args, fun_set, fastClass(), thisObj.address()))
            return false;
        if (!thisObj)
            return true;

        RootedObject tarray(cx, getTypedArray(thisObj));
        if (!tarray)
            return true;

        // first arg must be either a typed array or a JS array
        if (args.length() == 0 || !args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        int32_t offset = 0;
        if (args.length() > 1) {
            if (!ToInt32(cx, args[1], &offset))
                return false;

            if (offset < 0 || uint32_t(offset) > length(tarray)) {
                // the given offset is bogus
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_INDEX, "2");
                return false;
            }
        }

        if (!args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        RootedObject arg0(cx, args[0].toObjectOrNull());
        RootedObject src(cx, getTypedArray(arg0));
        if (src) {
            if (length(src) > length(tarray) - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
                return false;
            }

            if (!copyFromTypedArray(cx, thisObj, src, offset))
                return false;
        } else {
            src = arg0;
            uint32_t len;
            if (!js_GetLengthProperty(cx, src, &len))
                return false;

            // avoid overflow; we know that offset <= length
            if (len > length(tarray) - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
                return false;
            }

            if (!copyFromArray(cx, thisObj, src, len, offset))
                return false;
        }

        args.rval().setUndefined();
        return true;
    }

  public:
    static JSObject *
    fromBuffer(JSContext *cx, HandleObject bufobj, int32_t byteOffsetInt, int32_t lengthInt,
               HandleObject proto)
    {
        if (!ObjectClassIs(*bufobj, ESClass_ArrayBuffer, cx)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL; // must be arrayBuffer
        }

        JS_ASSERT(bufobj->isArrayBuffer() || bufobj->isProxy());
        if (bufobj->isProxy()) {
            /*
             * Normally, NonGenericMethodGuard handles the case of transparent
             * wrappers. However, we have a peculiar situation: we want to
             * construct the new typed array in the compartment of the buffer,
             * so that the typed array can point directly at their buffer's
             * data without crossing compartment boundaries. So we use the
             * machinery underlying NonGenericMethodGuard directly to proxy the
             * native call. We will end up with a wrapper in the origin
             * compartment for a view in the target compartment referencing the
             * ArrayBuffer in that same compartment.
             */
            JSObject *wrapped = UnwrapObjectChecked(cx, bufobj);
            if (!wrapped)
                return NULL;
            if (wrapped->isArrayBuffer()) {
                /*
                 * And for even more fun, the new view's prototype should be
                 * set to the origin compartment's prototype object, not the
                 * target's (specifically, the actual view in the target
                 * compartment will use as its prototype a wrapper around the
                 * origin compartment's view.prototype object)
                 */
                JSObject *proto = GetProtoForClass(cx, fastClass());
                if (!proto)
                    return NULL;
                Value argv[] = { UndefinedValue(),
                                 MagicValue(JS_IS_CONSTRUCTING),
                                 ObjectValue(*bufobj),
                                 Int32Value(byteOffsetInt),
                                 Int32Value(lengthInt),
                                 ObjectValue(*proto) };
                uint32_t argc = sizeof(argv) / sizeof(argv[0]) - 2;

                CallArgs args = CallArgsFromVp(argc, argv);
                if (!Proxy::nativeCall(cx, bufobj, &ArrayBufferClass, fromBuffer, args))
                    return NULL;
                return &args.rval().toObject();
            }
        }

        if (!bufobj->isArrayBuffer()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return NULL; // must be arrayBuffer
        }

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

        return makeInstance(cx, bufobj, boffset, len, proto);
    }

    static JSObject *
    fromLength(JSContext *cx, int32_t nelements)
    {
        RootedObject buffer(cx, createBufferWithSizeAndCount(cx, nelements));
        if (!buffer)
            return NULL;
        return makeInstance(cx, buffer, 0, nelements);
    }

    static JSObject *
    fromArray(JSContext *cx, HandleObject other)
    {
        uint32_t len;
        if (!js_GetLengthProperty(cx, other, &len))
            return NULL;

        RootedObject bufobj(cx, createBufferWithSizeAndCount(cx, len));
        if (!bufobj)
            return NULL;

        RootedObject obj(cx, makeInstance(cx, bufobj, 0, len));
        if (!obj || !copyFromArray(cx, obj, other, len))
            return NULL;
        return obj;
    }

    static const NativeType
    getIndex(JSObject *obj, uint32_t index)
    {
        return *(static_cast<const NativeType*>(viewData(obj)) + index);
    }

    static void
    setIndex(JSObject *obj, uint32_t index, NativeType val)
    {
        *(static_cast<NativeType*>(viewData(obj)) + index) = val;
    }

    static void copyIndexToValue(JSContext *cx, JSObject *tarray, uint32_t index, Value *vp);

    static JSObject *
    createSubarray(JSContext *cx, JSObject *tarray, uint32_t begin, uint32_t end)
    {
        JS_ASSERT(tarray);

        JS_ASSERT(begin <= length(tarray));
        JS_ASSERT(end <= length(tarray));

        RootedObject bufobj(cx, buffer(tarray));
        JS_ASSERT(bufobj);

        JS_ASSERT(begin <= end);
        uint32_t length = end - begin;

        JS_ASSERT(begin < UINT32_MAX / sizeof(NativeType));
        uint32_t arrayByteOffset = byteOffsetValue(tarray).toInt32();
        JS_ASSERT(UINT32_MAX - begin * sizeof(NativeType) >= arrayByteOffset);
        uint32_t byteOffset = arrayByteOffset + begin * sizeof(NativeType);

        return makeInstance(cx, bufobj, byteOffset, length);
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
                  HandleObject ar, uint32_t len, uint32_t offset = 0)
    {
        thisTypedArrayObj = getTypedArray(thisTypedArrayObj);
        JS_ASSERT(thisTypedArrayObj);

        JS_ASSERT(offset <= length(thisTypedArrayObj));
        JS_ASSERT(len <= length(thisTypedArrayObj) - offset);
        NativeType *dest = static_cast<NativeType*>(viewData(thisTypedArrayObj)) + offset;
        SkipRoot skip(cx, &dest);

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

        JS_ASSERT(offset <= length(thisTypedArrayObj));
        JS_ASSERT(length(tarray) <= length(thisTypedArrayObj) - offset);
        if (buffer(tarray) == buffer(thisTypedArrayObj))
            return copyFromWithOverlap(cx, thisTypedArrayObj, tarray, offset);

        NativeType *dest = static_cast<NativeType*>(viewData(thisTypedArrayObj)) + offset;

        if (type(tarray) == type(thisTypedArrayObj)) {
            js_memcpy(dest, viewData(tarray), byteLengthValue(tarray).toInt32());
            return true;
        }

        unsigned srclen = length(tarray);
        switch (type(tarray)) {
          case TypedArray::TYPE_INT8: {
            int8_t *src = static_cast<int8_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED: {
            uint8_t *src = static_cast<uint8_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT16: {
            int16_t *src = static_cast<int16_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT16: {
            uint16_t *src = static_cast<uint16_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT32: {
            int32_t *src = static_cast<int32_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT32: {
            uint32_t *src = static_cast<uint32_t*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT32: {
            float *src = static_cast<float*>(viewData(tarray));
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT64: {
            double *src = static_cast<double*>(viewData(tarray));
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
        JS_ASSERT(offset <= length(self));

        NativeType *dest = static_cast<NativeType*>(viewData(self)) + offset;
        uint32_t byteLength = byteLengthValue(tarray).toInt32();

        if (type(tarray) == type(self)) {
            memmove(dest, viewData(tarray), byteLength);
            return true;
        }

        // We have to make a copy of the source array here, since
        // there's overlap, and we have to convert types.
        void *srcbuf = cx->malloc_(byteLength);
        if (!srcbuf)
            return false;
        js_memcpy(srcbuf, viewData(tarray), byteLength);

        switch (type(tarray)) {
          case TypedArray::TYPE_INT8: {
            int8_t *src = (int8_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED: {
            uint8_t *src = (uint8_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT16: {
            int16_t *src = (int16_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT16: {
            uint16_t *src = (uint16_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_INT32: {
            int32_t *src = (int32_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_UINT32: {
            uint32_t *src = (uint32_t*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT32: {
            float *src = (float*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case TypedArray::TYPE_FLOAT64: {
            double *src = (double*) srcbuf;
            for (unsigned i = 0; i < length(tarray); ++i)
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
    vp->setDouble(JS_CANONICALIZE_NAN(dval));
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
    vp->setDouble(JS_CANONICALIZE_NAN(val));
}

JSBool
DataViewObject::construct(JSContext *cx, JSObject *bufobj, const CallArgs &args, JSObject *proto)
{
    if (!bufobj->isArrayBuffer()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_EXPECTED_TYPE,
                             "DataView", "ArrayBuffer", bufobj->getClass()->name);
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &bufobj->asArrayBuffer());
    uint32_t bufferLength = buffer->byteLength();
    uint32_t byteOffset = 0;
    uint32_t byteLength = bufferLength;

    if (args.length() > 1) {
        if (!ToUint32(cx, args[1], &byteOffset))
            return false;
        if (byteOffset > INT32_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
            return false;
        }

        if (args.length() > 2) {
            if (!ToUint32(cx, args[2], &byteLength))
                return false;
            if (byteLength > INT32_MAX) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
                return false;
            }
        } else {
            if (byteOffset > bufferLength) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
                return false;
            }

            byteLength = bufferLength - byteOffset;
        }
    }

    /* The sum of these cannot overflow a uint32_t */
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    if (byteOffset + byteLength > bufferLength) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
        return false;
    }

    JSObject *obj = DataViewObject::create(cx, byteOffset, byteLength, buffer, proto);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

JSBool
DataViewObject::constructWithProto(JSContext *cx, unsigned argc, Value *vp)
{
    // Pop the proto argument off the end
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject &proto = args[args.length() - 1].toObject();

    // And now mimic class_constructor for everything else, but pass in the proto
    args = CallArgsFromVp(argc - 1, vp);
    JSObject *bufobj;
    if (!GetFirstArgumentAsObject(cx, args.length(), args.base(), "DataView constructor", &bufobj))
        return false;
    return construct(cx, bufobj, args, &proto);
}

JSBool
DataViewObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *bufobj;
    if (!GetFirstArgumentAsObject(cx, args.length(), args.base(), "DataView constructor", &bufobj))
        return false;

    if (bufobj->isWrapper() && UnwrapObject(bufobj)->isArrayBuffer()) {
        JSObject *proto = GetProtoForClass(cx, &DataViewClass);
        if (!proto)
            return false;

        Vector<Value, 6> argv(cx);
        argv.resize(argc + 2 + 1);
        memcpy(argv.begin(), args.base(), sizeof(Value) * (argc + 2));
        argv[argc + 2].setObject(*proto);
        argv[0].setUndefined(); // We want to use a different callee (avoid an assertion)

        // Appease 'thisv' assertion in CrossCompartmentWrapper::nativeCall
        argv[1].setMagic(JS_IS_CONSTRUCTING);

        CallArgs proxyArgs = CallArgsFromVp(argc + 1, argv.begin());
        if (!Proxy::nativeCall(cx, bufobj, &DataViewClass, constructWithProto, proxyArgs))
            return false;
        args.rval() = proxyArgs.rval();
        return true;
    }

    return construct(cx, bufobj, args, NULL);
}

bool
DataViewObject::getDataPointer(JSContext *cx, CallArgs args, size_t typeSize, uint8_t **data)
{
    JS_ASSERT(isDataView());

    uint32_t offset;
    JS_ASSERT(args.length() > 0);
    if (!ToUint32(cx, args[0], &offset))
        return false;
    if (offset > UINT32_MAX - typeSize || offset + typeSize > byteLength()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
        return false;
    }

    *data = static_cast<uint8_t*>(dataPointer()) + offset;
    return true;
}

static inline bool
needToSwapBytes(bool littleEndian)
{
#if IS_LITTLE_ENDIAN
    return !littleEndian;
#else
    return littleEndian;
#endif
}

static inline uint8_t
swapBytes(uint8_t x)
{
    return x;
}

static inline uint16_t
swapBytes(uint16_t x)
{
    return ((x & 0xff) << 8) | (x >> 8);
}

static inline uint32_t
swapBytes(uint32_t x)
{
    return ((x & 0xff) << 24) |
           ((x & 0xff00) << 8) |
           ((x & 0xff0000) >> 8) |
           ((x & 0xff000000) >> 24);
}

static inline uint64_t
swapBytes(uint64_t x)
{
    uint32_t a = x & UINT32_MAX;
    uint32_t b = x >> 32;
    return (uint64_t(swapBytes(a)) << 32) | swapBytes(b);
}

template <typename DataType> struct DataToRepType { typedef DataType result; };
template <> struct DataToRepType<int8_t>   { typedef uint8_t result; };
template <> struct DataToRepType<uint8_t>  { typedef uint8_t result; };
template <> struct DataToRepType<int16_t>  { typedef uint16_t result; };
template <> struct DataToRepType<uint16_t> { typedef uint16_t result; };
template <> struct DataToRepType<int32_t>  { typedef uint32_t result; };
template <> struct DataToRepType<uint32_t> { typedef uint32_t result; };
template <> struct DataToRepType<float>    { typedef uint32_t result; };
template <> struct DataToRepType<double>   { typedef uint64_t result; };

template <typename DataType>
struct DataViewIO
{
    typedef typename DataToRepType<DataType>::result ReadWriteType;

    static void fromBuffer(DataType *dest, const uint8_t *unalignedBuffer, bool wantSwap)
    {
        JS_ASSERT((reinterpret_cast<uintptr_t>(dest) & (Min<size_t>(JS_ALIGN_OF_POINTER, sizeof(DataType)) - 1)) == 0);
        memcpy((void *) dest, unalignedBuffer, sizeof(ReadWriteType));
        if (wantSwap) {
            ReadWriteType *rwDest = reinterpret_cast<ReadWriteType *>(dest);
            *rwDest = swapBytes(*rwDest);
        }
    }

    static void toBuffer(uint8_t *unalignedBuffer, const DataType *src, bool wantSwap)
    {
        JS_ASSERT((reinterpret_cast<uintptr_t>(src) & (Min<size_t>(JS_ALIGN_OF_POINTER, sizeof(DataType)) - 1)) == 0);
        ReadWriteType temp = *reinterpret_cast<const ReadWriteType *>(src);
        if (wantSwap)
            temp = swapBytes(temp);
        memcpy(unalignedBuffer, (void *) &temp, sizeof(ReadWriteType));
    }
};

template<typename NativeType>
bool
DataViewObject::read(JSContext *cx, CallArgs &args, NativeType *val, const char *method)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_MORE_ARGS_NEEDED, method, "0", "s");
        return false;
    }

    uint8_t *data;
    if (!getDataPointer(cx, args, sizeof(NativeType), &data))
        return false;

    bool fromLittleEndian = args.length() >= 2 && js_ValueToBoolean(args[1]);
    DataViewIO<NativeType>::fromBuffer(val, data, needToSwapBytes(fromLittleEndian));
    return true;
}

template <typename NativeType>
static inline bool
WebIDLCast(JSContext *cx, const Value &value, NativeType *out)
{
    int32_t temp;
    if (!ToInt32(cx, value, &temp))
        return false;
    // Technically, the behavior of assigning an out of range value to a signed
    // variable is undefined. In practice, compilers seem to do what we want
    // without issuing any warnings.
    *out = static_cast<NativeType>(temp);
    return true;
}

template <>
inline bool
WebIDLCast<float>(JSContext *cx, const Value &value, float *out)
{
    double temp;
    if (!ToNumber(cx, value, &temp))
        return false;
    *out = static_cast<float>(temp);
    return true;
}

template <>
inline bool
WebIDLCast<double>(JSContext *cx, const Value &value, double *out)
{
    return ToNumber(cx, value, out);
}

template<typename NativeType>
bool
DataViewObject::write(JSContext *cx, CallArgs &args, const char *method)
{
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_MORE_ARGS_NEEDED, method, "1", "");
        return false;
    }

    uint8_t *data;
    if (!getDataPointer(cx, args, sizeof(NativeType), &data))
        return false;

    NativeType value;
    if (!WebIDLCast(cx, args[1], &value))
        return false;

    bool toLittleEndian = args.length() >= 3 && js_ValueToBoolean(args[2]);
    DataViewIO<NativeType>::toBuffer(data, &value, needToSwapBytes(toLittleEndian));
    return true;
}

JSBool
DataViewObject::fun_getInt8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getInt8, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    int8_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getInt8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

JSBool
DataViewObject::fun_getUint8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getUint8, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    uint8_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getUint8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

JSBool
DataViewObject::fun_getInt16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getInt16, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    int16_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getInt16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

JSBool
DataViewObject::fun_getUint16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getUint16, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    uint16_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getUint16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

JSBool
DataViewObject::fun_getInt32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getInt32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    int32_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getInt32"))
        return false;
    args.rval().setInt32(val);
    return true;
}

JSBool
DataViewObject::fun_getUint32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getUint32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    uint32_t val;
    if (!thisObj->asDataView().read(cx, args, &val, "getUint32"))
        return false;
    args.rval().setNumber(val);
    return true;
}

JSBool
DataViewObject::fun_getFloat32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getFloat32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    float val;
    if (!thisObj->asDataView().read(cx, args, &val, "getFloat32"))
        return false;

    args.rval().setDouble(JS_CANONICALIZE_NAN(val));
    return true;
}

JSBool
DataViewObject::fun_getFloat64(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_getFloat64, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    double val;
    if (!thisObj->asDataView().read(cx, args, &val, "getFloat64"))
        return false;

    args.rval().setDouble(JS_CANONICALIZE_NAN(val));
    return true;
}

JSBool
DataViewObject::fun_setInt8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setInt8, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<int8_t>(cx, args, "setInt8"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setUint8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setUint8, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<uint8_t>(cx, args, "setUint8"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setInt16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setInt16, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<int16_t>(cx, args, "setInt16"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setUint16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setUint16, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<uint16_t>(cx, args, "setUint16"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setInt32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setInt32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<int32_t>(cx, args, "setInt32"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setUint32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setUint32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<uint32_t>(cx, args, "setUint32"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setFloat32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setFloat32, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<float>(cx, args, "setFloat32"))
        return false;
    args.rval().setUndefined();
    return true;
}

JSBool
DataViewObject::fun_setFloat64(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, fun_setFloat64, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    if (!thisObj->asDataView().write<double>(cx, args, "setFloat64"))
        return false;
    args.rval().setUndefined();
    return true;
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

JSFunctionSpec ArrayBufferObject::jsfuncs[] = {
    JS_FN("slice", ArrayBufferObject::fun_slice, 2, JSFUN_GENERIC_NATIVE),
    JS_FS_END
};

/*
 * TypedArray boilerplate
 */

#define IMPL_TYPED_ARRAY_STATICS(_typedArray)                                  \
JSFunctionSpec _typedArray::jsfuncs[] = {                                      \
    JS_FN("subarray", _typedArray::fun_subarray, 2, JSFUN_GENERIC_NATIVE),     \
    JS_FN("set", _typedArray::fun_set, 2, JSFUN_GENERIC_NATIVE),               \
    JS_FN("move", _typedArray::fun_move, 3, JSFUN_GENERIC_NATIVE),             \
    JS_FS_END                                                                  \
}

#define IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Name,NativeType)                                 \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## Array(JSContext *cx, uint32_t nelements)       \
  {                                                                                          \
      MOZ_ASSERT(nelements <= INT32_MAX);                                                    \
      return TypedArrayTemplate<NativeType>::fromLength(cx, nelements);                      \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayFromArray(JSContext *cx, JSObject *other_)\
  {                                                                                          \
      Rooted<JSObject*> other(cx, other_);                                                   \
      return TypedArrayTemplate<NativeType>::fromArray(cx, other);                           \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayWithBuffer(JSContext *cx,                 \
                               JSObject *arrayBuffer_, uint32_t byteoffset, int32_t length)  \
  {                                                                                          \
      MOZ_ASSERT(byteoffset <= INT32_MAX);                                                   \
      Rooted<JSObject*> arrayBuffer(cx, arrayBuffer_);                                       \
      Rooted<JSObject*> proto(cx, NULL);                                                     \
      return TypedArrayTemplate<NativeType>::fromBuffer(cx, arrayBuffer, byteoffset, length, \
                                                        proto);                              \
  }                                                                                          \
  JS_FRIEND_API(JSBool) JS_Is ## Name ## Array(JSObject *obj, JSContext *cx)                 \
  {                                                                                          \
      if (!(obj = UnwrapObjectChecked(cx, obj)))                                             \
          return false;                                                                      \
      Class *clasp = obj->getClass();                                                        \
      return (clasp == &TypedArray::classes[TypedArrayTemplate<NativeType>::ArrayTypeID()]); \
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
    #_typedArray "Prototype",                                                  \
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
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_typedArray) |                          \
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
InitTypedArrayClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, &cx->compartment->global());
    RootedObject proto(cx, global->createBlankPrototype(cx, ArrayType::protoClass()));
    if (!proto)
        return NULL;

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, ArrayType::class_constructor,
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

    RootedObject foo(cx);

    if (!ArrayType::defineGetters(cx, proto))
        return NULL;

    if (!JS_DefineFunctions(cx, proto, ArrayType::jsfuncs))
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

Class TypedArray::classes[TYPE_MAX] = {
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
InitArrayBufferClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, &cx->compartment->global());
    RootedObject arrayBufferProto(cx, global->createBlankPrototype(cx, &ArrayBufferObject::protoClass));
    if (!arrayBufferProto)
        return NULL;

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, ArrayBufferObject::class_constructor,
                                     CLASS_NAME(cx, ArrayBuffer), 1);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, arrayBufferProto))
        return NULL;

    RootedId byteLengthId(cx, NameToId(cx->runtime->atomState.byteLengthAtom));
    unsigned flags = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_SHARED | JSPROP_GETTER;
    JSObject *getter = js_NewFunction(cx, NULL, ArrayBufferObject::byteLengthGetter, 0, 0, global, NULL);
    if (!getter)
        return NULL;

    if (!DefineNativeProperty(cx, arrayBufferProto, byteLengthId, UndefinedValue(),
                              JS_DATA_TO_FUNC_PTR(PropertyOp, getter), NULL, flags, 0, 0))
        return NULL;

    if (!JS_DefineFunctions(cx, arrayBufferProto, ArrayBufferObject::jsfuncs))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_ArrayBuffer, ctor, arrayBufferProto))
        return NULL;

    return arrayBufferProto;
}

Class js::DataViewObject::protoClass = {
    "DataViewPrototype",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(DataViewObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class js::DataViewClass = {
    "DataView",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(DataViewObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
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
    NULL,           /* trace */
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS
};

JSFunctionSpec DataViewObject::jsfuncs[] = {
    JS_FN("getInt8",    DataViewObject::fun_getInt8,      1,0),
    JS_FN("getUint8",   DataViewObject::fun_getUint8,     1,0),
    JS_FN("getInt16",   DataViewObject::fun_getInt16,     2,0),
    JS_FN("getUint16",  DataViewObject::fun_getUint16,    2,0),
    JS_FN("getInt32",   DataViewObject::fun_getInt32,     2,0),
    JS_FN("getUint32",  DataViewObject::fun_getUint32,    2,0),
    JS_FN("getFloat32", DataViewObject::fun_getFloat32,   2,0),
    JS_FN("getFloat64", DataViewObject::fun_getFloat64,   2,0),
    JS_FN("setInt8",    DataViewObject::fun_setInt8,      2,0),
    JS_FN("setUint8",   DataViewObject::fun_setUint8,     2,0),
    JS_FN("setInt16",   DataViewObject::fun_setInt16,     3,0),
    JS_FN("setUint16",  DataViewObject::fun_setUint16,    3,0),
    JS_FN("setInt32",   DataViewObject::fun_setInt32,     3,0),
    JS_FN("setUint32",  DataViewObject::fun_setUint32,    3,0),
    JS_FN("setFloat32", DataViewObject::fun_setFloat32,   3,0),
    JS_FN("setFloat64", DataViewObject::fun_setFloat64,   3,0),
    JS_FS_END
};

template<Value ValueGetter(DataViewObject &)>
static JSBool
DataViewGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *thisObj;
    if (!NonGenericMethodGuard(cx, args, DataViewGetter<ValueGetter>, &DataViewClass, &thisObj))
        return false;
    if (!thisObj)
        return true;

    JS_SET_RVAL(cx, vp, ValueGetter(thisObj->asDataView()));
    return true;
}

template<Value ValueGetter(DataViewObject&)>
bool
DefineDataViewGetter(JSContext *cx, PropertyName *name, HandleObject proto)
{
    RootedId id(cx, NameToId(name));
    unsigned flags = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_SHARED | JSPROP_GETTER;

    Rooted<GlobalObject*> global(cx, &cx->compartment->global());
    JSObject *getter = js_NewFunction(cx, NULL, DataViewGetter<ValueGetter>, 0, 0, global, NULL);
    if (!getter)
        return false;

    return DefineNativeProperty(cx, proto, id, UndefinedValue(),
                                JS_DATA_TO_FUNC_PTR(PropertyOp, getter), NULL,
                                flags, 0, 0);
}

JSObject *
DataViewObject::initClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, &cx->compartment->global());
    RootedObject proto(cx, global->createBlankPrototype(cx, &DataViewObject::protoClass));
    if (!proto)
        return NULL;

    RootedFunction ctor(cx, global->createConstructor(cx, DataViewObject::class_constructor,
                                                      CLASS_NAME(cx, DataView), 3));
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    if (!DefineDataViewGetter<bufferValue>(cx, cx->runtime->atomState.bufferAtom, proto))
        return NULL;

    if (!DefineDataViewGetter<byteLengthValue>(cx, cx->runtime->atomState.byteLengthAtom, proto))
        return NULL;

    if (!DefineDataViewGetter<byteOffsetValue>(cx, cx->runtime->atomState.byteOffsetAtom, proto))
        return NULL;

    if (!JS_DefineFunctions(cx, proto, DataViewObject::jsfuncs))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_DataView, ctor, proto))
        return NULL;

    return proto;
}

JSObject *
js_InitTypedArrayClasses(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    /* Idempotency required: we initialize several things, possibly lazily. */
    JSObject *stop;
    if (!js_GetClassObject(cx, global, JSProto_ArrayBuffer, &stop))
        return NULL;
    if (stop)
        return stop;

    if (!InitTypedArrayClass<Int8Array>(cx) ||
        !InitTypedArrayClass<Uint8Array>(cx) ||
        !InitTypedArrayClass<Int16Array>(cx) ||
        !InitTypedArrayClass<Uint16Array>(cx) ||
        !InitTypedArrayClass<Int32Array>(cx) ||
        !InitTypedArrayClass<Uint32Array>(cx) ||
        !InitTypedArrayClass<Float32Array>(cx) ||
        !InitTypedArrayClass<Float64Array>(cx) ||
        !InitTypedArrayClass<Uint8ClampedArray>(cx) ||
        !DataViewObject::initClass(cx))
    {
        return NULL;
    }

    return InitArrayBufferClass(cx);
}

/* JS Friend API */

// The typed array friend API defines a number of accessor functions that want
// to unwrap an argument, but in certain rare cases may not have a cx available
// and so pass in NULL instead. Use UnwrapObjectChecked when possible.
static JSObject *
CheckedUnwrap(JSContext *cx, JSObject *obj)
{
    if (!cx)
        return UnwrapObject(obj);
    MOZ_ASSERT(!cx->isExceptionPending());
    obj = UnwrapObjectChecked(cx, obj);
    MOZ_ASSERT(obj);
    return obj;
}

JS_FRIEND_API(JSBool)
JS_IsArrayBufferObject(JSObject *obj, JSContext *cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    if (!(obj = UnwrapObjectChecked(cx, obj))) {
        cx->clearPendingException();
        return false;
    }
    return obj->isArrayBuffer();
}

JS_FRIEND_API(JSBool)
JS_IsTypedArrayObject(JSObject *obj, JSContext *cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    if (!(obj = UnwrapObjectChecked(cx, obj))) {
        cx->clearPendingException();
        return false;
    }
    return obj->isTypedArray();
}

JS_FRIEND_API(JSBool)
JS_IsArrayBufferViewObject(JSObject *obj, JSContext *cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    if (!(obj = UnwrapObjectChecked(cx, obj))) {
        cx->clearPendingException();
        return false;
    }
    return obj->isTypedArray() || obj->isDataView();
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    return obj->asArrayBuffer().byteLength();
}

JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
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
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_LENGTH).toInt32();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteOffset(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_BYTEOFFSET).toInt32();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteLength(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    JS_ASSERT(obj->isTypedArray());
    return obj->getSlot(TypedArray::FIELD_BYTELENGTH).toInt32();
}

JS_FRIEND_API(JSArrayBufferViewType)
JS_GetTypedArrayType(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return ArrayBufferView::TYPE_MAX;
    JS_ASSERT(obj->isTypedArray());
    return static_cast<JSArrayBufferViewType>(obj->getSlot(TypedArray::FIELD_TYPE).toInt32());
}

JS_FRIEND_API(int8_t *)
JS_GetInt8ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT8);
    return static_cast<int8_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT8);
    return static_cast<uint8_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ClampedArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT8_CLAMPED);
    return static_cast<uint8_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(int16_t *)
JS_GetInt16ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT16);
    return static_cast<int16_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(uint16_t *)
JS_GetUint16ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT16);
    return static_cast<uint16_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(int32_t *)
JS_GetInt32ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_INT32);
    return static_cast<int32_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(uint32_t *)
JS_GetUint32ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_UINT32);
    return static_cast<uint32_t *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(float *)
JS_GetFloat32ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_FLOAT32);
    return static_cast<float *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(double *)
JS_GetFloat64ArrayData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray());
    JS_ASSERT(obj->getSlot(TypedArray::FIELD_TYPE).toInt32() == ArrayBufferView::TYPE_FLOAT64);
    return static_cast<double *>(TypedArray::viewData(obj));
}

JS_FRIEND_API(JSBool)
JS_IsDataViewObject(JSContext *cx, JSObject *obj, JSBool *isDataView)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return false;
    *isDataView = obj->isDataView();
    return true;
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteOffset(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    return obj->asDataView().byteOffset();
}

JS_FRIEND_API(void *)
JS_GetDataViewData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isDataView());
    return obj->asDataView().dataPointer();
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteLength(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    JS_ASSERT(obj->isDataView());
    return obj->asDataView().byteLength();
}

JS_FRIEND_API(void *)
JS_GetArrayBufferViewData(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return NULL;
    JS_ASSERT(obj->isTypedArray() || obj->isDataView());
    return obj->isDataView() ? obj->asDataView().dataPointer() : TypedArray::viewData(obj);
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject *obj, JSContext *cx)
{
    if (!(obj = CheckedUnwrap(cx, obj)))
        return 0;
    JS_ASSERT(obj->isTypedArray() || obj->isDataView());
    return obj->isDataView() ? obj->asDataView().byteLength() : TypedArray::byteLengthValue(obj).toInt32();
}
