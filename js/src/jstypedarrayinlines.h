/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jstypedarrayinlines_h
#define jstypedarrayinlines_h

#include "jsapi.h"
#include "jsobj.h"
#include "jstypedarray.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

namespace js {

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
TypedArrayObject::lengthValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(LENGTH_SLOT);
}

inline uint32_t
TypedArrayObject::length(JSObject *obj)
{
    return lengthValue(obj).toInt32();
}

inline Value
TypedArrayObject::byteOffsetValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(BYTEOFFSET_SLOT);
}

inline uint32_t
TypedArrayObject::byteOffset(JSObject *obj)
{
    return byteOffsetValue(obj).toInt32();
}

inline Value
TypedArrayObject::byteLengthValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(BYTELENGTH_SLOT);
}

inline uint32_t
TypedArrayObject::byteLength(JSObject *obj)
{
    return byteLengthValue(obj).toInt32();
}

inline uint32_t
TypedArrayObject::type(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(TYPE_SLOT).toInt32();
}

inline Value
TypedArrayObject::bufferValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(BUFFER_SLOT);
}

inline ArrayBufferObject *
TypedArrayObject::buffer(JSObject *obj)
{
    return &bufferValue(obj).toObject().as<ArrayBufferObject>();
}

inline void *
TypedArrayObject::viewData(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return (void *)obj->getPrivate(DATA_SLOT);
}

inline uint32_t
TypedArrayObject::slotWidth(int atype) {
    switch (atype) {
    case js::TypedArrayObject::TYPE_INT8:
    case js::TypedArrayObject::TYPE_UINT8:
    case js::TypedArrayObject::TYPE_UINT8_CLAMPED:
        return 1;
    case js::TypedArrayObject::TYPE_INT16:
    case js::TypedArrayObject::TYPE_UINT16:
        return 2;
    case js::TypedArrayObject::TYPE_INT32:
    case js::TypedArrayObject::TYPE_UINT32:
    case js::TypedArrayObject::TYPE_FLOAT32:
        return 4;
    case js::TypedArrayObject::TYPE_FLOAT64:
        return 8;
    default:
        MOZ_ASSUME_UNREACHABLE("invalid typed array type");
    }
}

inline int
TypedArrayObject::slotWidth(JSObject *obj) {
    return slotWidth(type(obj));
}

#ifdef JSGC_GENERATIONAL
class ArrayBufferViewByteOffsetRef : public gc::BufferableRef
{
    JSObject *obj;

  public:
    explicit ArrayBufferViewByteOffsetRef(JSObject *obj) : obj(obj) {}

    void mark(JSTracer *trc) {
        MarkObjectUnbarriered(trc, &obj, "TypedArray");
        obj->getClass()->trace(trc, obj);
    }
};
#endif

static inline void
InitArrayBufferViewDataPointer(JSObject *obj, ArrayBufferObject *buffer, size_t byteOffset)
{
    /*
     * N.B. The base of the array's data is stored in the object's
     * private data rather than a slot to avoid alignment restrictions
     * on private Values.
     */
    obj->initPrivate(buffer->dataPointer() + byteOffset);
#ifdef JSGC_GENERATIONAL
    if (IsInsideNursery(obj->runtime(), buffer) && buffer->hasFixedElements())
        obj->runtime()->gcStoreBuffer.putGeneric(ArrayBufferViewByteOffsetRef(obj));
#endif
}

} /* namespace js */

#endif /* jstypedarrayinlines_h */
