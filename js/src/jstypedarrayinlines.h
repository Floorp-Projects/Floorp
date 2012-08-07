/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jstypedarrayinlines_h
#define jstypedarrayinlines_h

#include "jsapi.h"
#include "jsobj.h"
#include "jstypedarray.h"

#include "jsobjinlines.h"

inline uint32_t
js::ArrayBufferObject::byteLength() const
{
    JS_ASSERT(isArrayBuffer());
    return getElementsHeader()->length;
}

inline uint8_t *
js::ArrayBufferObject::dataPointer() const
{
    return (uint8_t *) elements;
}

inline js::ArrayBufferObject &
JSObject::asArrayBuffer()
{
    JS_ASSERT(isArrayBuffer());
    return *static_cast<js::ArrayBufferObject *>(this);
}

inline js::DataViewObject &
JSObject::asDataView()
{
    JS_ASSERT(isDataView());
    return *static_cast<js::DataViewObject *>(this);
}

namespace js {

inline bool
ArrayBufferObject::hasData() const
{
    return getClass() == &ArrayBufferClass;
}

static inline int32_t
ClampIntForUint8Array(int32_t x)
{
    if (x < 0)
        return 0;
    if (x > 255)
        return 255;
    return x;
}

inline Value
TypedArray::lengthValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_LENGTH);
}

inline uint32_t
TypedArray::length(JSObject *obj)
{
    return lengthValue(obj).toInt32();
}

inline Value
TypedArray::byteOffsetValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_BYTEOFFSET);
}

inline uint32_t
TypedArray::byteOffset(JSObject *obj)
{
    return byteOffsetValue(obj).toInt32();
}

inline Value
TypedArray::byteLengthValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_BYTELENGTH);
}

inline uint32_t
TypedArray::byteLength(JSObject *obj)
{
    return byteLengthValue(obj).toInt32();
}

inline uint32_t
TypedArray::type(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_TYPE).toInt32();
}

inline Value
TypedArray::bufferValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(FIELD_BUFFER);
}

inline ArrayBufferObject *
TypedArray::buffer(JSObject *obj)
{
    return &bufferValue(obj).toObject().asArrayBuffer();
}

inline void *
TypedArray::viewData(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return (void *)obj->getPrivate(NUM_FIXED_SLOTS);
}

inline uint32_t
TypedArray::slotWidth(int atype) {
    switch (atype) {
    case js::TypedArray::TYPE_INT8:
    case js::TypedArray::TYPE_UINT8:
    case js::TypedArray::TYPE_UINT8_CLAMPED:
        return 1;
    case js::TypedArray::TYPE_INT16:
    case js::TypedArray::TYPE_UINT16:
        return 2;
    case js::TypedArray::TYPE_INT32:
    case js::TypedArray::TYPE_UINT32:
    case js::TypedArray::TYPE_FLOAT32:
        return 4;
    case js::TypedArray::TYPE_FLOAT64:
        return 8;
    default:
        JS_NOT_REACHED("invalid typed array type");
        return 0;
    }
}

inline int
TypedArray::slotWidth(JSObject *obj) {
    return slotWidth(type(obj));
}

bool
DataViewObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&DataViewClass);
}

inline DataViewObject *
DataViewObject::create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
                       Handle<ArrayBufferObject*> arrayBuffer, JSObject *protoArg)
{
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    RootedObject proto(cx, protoArg);
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &DataViewClass));
    if (!obj)
        return NULL;

    if (proto) {
        types::TypeObject *type = proto->getNewType(cx);
        if (!type)
            return NULL;
        obj->setType(type);
    } else if (cx->typeInferenceEnabled()) {
        if (byteLength >= TypedArray::SINGLETON_TYPE_BYTE_LENGTH) {
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

    JS_ASSERT(arrayBuffer->isArrayBuffer());

    DataViewObject &dvobj = obj->asDataView();
    dvobj.setFixedSlot(BYTEOFFSET_SLOT, Int32Value(byteOffset));
    dvobj.setFixedSlot(BYTELENGTH_SLOT, Int32Value(byteLength));
    dvobj.setFixedSlot(BUFFER_SLOT, ObjectValue(*arrayBuffer));
    dvobj.setPrivate(arrayBuffer->dataPointer() + byteOffset);
    JS_ASSERT(byteOffset + byteLength <= arrayBuffer->byteLength());

    JS_ASSERT(dvobj.numFixedSlots() == RESERVED_SLOTS);

    return &dvobj;
}

inline uint32_t
DataViewObject::byteLength()
{
    JS_ASSERT(isDataView());
    int32_t length = getReservedSlot(BYTELENGTH_SLOT).toInt32();
    JS_ASSERT(length >= 0);
    return static_cast<uint32_t>(length);
}

inline uint32_t
DataViewObject::byteOffset()
{
    JS_ASSERT(isDataView());
    int32_t offset = getReservedSlot(BYTEOFFSET_SLOT).toInt32();
    JS_ASSERT(offset >= 0);
    return static_cast<uint32_t>(offset);
}

inline void *
DataViewObject::dataPointer()
{
    JS_ASSERT(isDataView());
    return getPrivate();
}

inline JSObject &
DataViewObject::arrayBuffer()
{
    JS_ASSERT(isDataView());
    return getReservedSlot(BUFFER_SLOT).toObject();
}

inline bool
DataViewObject::hasBuffer() const
{
    JS_ASSERT(isDataView());
    return getReservedSlot(BUFFER_SLOT).isObject();
}

inline Value
DataViewObject::bufferValue(DataViewObject &view)
{
    return view.hasBuffer() ? ObjectValue(view.arrayBuffer()) : UndefinedValue();
}

inline Value
DataViewObject::byteOffsetValue(DataViewObject &view)
{
    return Int32Value(view.byteOffset());
}

inline Value
DataViewObject::byteLengthValue(DataViewObject &view)
{
    return Int32Value(view.byteLength());
}

} /* namespace js */

#endif /* jstypedarrayinlines_h */
