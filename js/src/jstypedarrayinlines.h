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

#include "jsobjinlines.h"

// Sentinel value used to initialize ArrayBufferViews' NEXT_BUFFER_SLOTs to
// show that they have not yet been added to any ArrayBuffer list
JSObject * const UNSET_BUFFER_LINK = (JSObject*)0x2;

inline void
js::ArrayBufferObject::setElementsHeader(js::ObjectElements *header, uint32_t bytes)
{
    header->flags = 0;
    header->initializedLength = bytes;

    // NB: one or both of these fields is clobbered by GetViewList to store the
    // 'views' link. Set them to 0 to effectively initialize 'views' to NULL.
    header->length = 0;
    header->capacity = 0;
}

inline uint32_t
js::ArrayBufferObject::getElementsHeaderInitializedLength(const js::ObjectElements *header)
{
    return header->initializedLength;
}

inline uint32_t
js::ArrayBufferObject::byteLength() const
{
    JS_ASSERT(is<ArrayBufferObject>());
    return getElementsHeader()->initializedLength;
}

inline uint8_t *
js::ArrayBufferObject::dataPointer() const
{
    return (uint8_t *) elements;
}

namespace js {

inline bool
ArrayBufferObject::hasData() const
{
    return getClass() == &class_;
}

inline bool
ArrayBufferObject::isAsmJSArrayBuffer() const
{
    return getElementsHeader()->isAsmJSArrayBuffer();
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
    return obj->getFixedSlot(LENGTH_SLOT);
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
    return obj->getFixedSlot(BYTEOFFSET_SLOT);
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
    return obj->getFixedSlot(BYTELENGTH_SLOT);
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
    return obj->getFixedSlot(TYPE_SLOT).toInt32();
}

inline Value
TypedArray::bufferValue(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return obj->getFixedSlot(BUFFER_SLOT);
}

inline ArrayBufferObject *
TypedArray::buffer(JSObject *obj)
{
    return &bufferValue(obj).toObject().as<ArrayBufferObject>();
}

inline void *
TypedArray::viewData(JSObject *obj)
{
    JS_ASSERT(obj->isTypedArray());
    return (void *)obj->getPrivate(DATA_SLOT);
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
    return v.isObject() && v.toObject().hasClass(&class_);
}

#ifdef JSGC_GENERATIONAL
class ArrayBufferViewByteOffsetRef : public gc::BufferableRef
{
    JSObject *obj;

  public:
    explicit ArrayBufferViewByteOffsetRef(JSObject *obj) : obj(obj) {}

    bool match(void *location) {
        /* The private field  of obj is not traced, but needs to be updated by mark. */
        return false;
    }

    void mark(JSTracer *trc) {
        /* Update obj's private to point to the moved buffer's array data. */
        MarkObjectUnbarriered(trc, &obj, "TypedArray");
        HeapSlot &bufSlot = obj->getReservedSlotRef(BufferView::BUFFER_SLOT);
        gc::MarkSlot(trc, &bufSlot, "TypedArray::BUFFER_SLOT");
        ArrayBufferObject &buf = bufSlot.toObject().as<ArrayBufferObject>();
        int32_t offset = obj->getReservedSlot(BufferView::BYTEOFFSET_SLOT).toInt32();
        obj->initPrivate(buf.dataPointer() + offset);
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

static NewObjectKind
DataViewNewObjectKind(JSContext *cx, uint32_t byteLength, JSObject *proto)
{
    if (!proto && byteLength >= TypedArray::SINGLETON_TYPE_BYTE_LENGTH)
        return SingletonObject;
    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (!script)
        return GenericObject;
    return types::UseNewTypeForInitializer(cx, script, pc, &DataViewObject::class_);
}

inline DataViewObject *
DataViewObject::create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
                       Handle<ArrayBufferObject*> arrayBuffer, JSObject *protoArg)
{
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    RootedObject proto(cx, protoArg);
    RootedObject obj(cx);

    NewObjectKind newKind = DataViewNewObjectKind(cx, byteLength, proto);
    obj = NewBuiltinClassInstance(cx, &class_, newKind);
    if (!obj)
        return NULL;

    if (proto) {
        types::TypeObject *type = proto->getNewType(cx, &class_);
        if (!type)
            return NULL;
        obj->setType(type);
    } else if (cx->typeInferenceEnabled()) {
        if (byteLength >= TypedArray::SINGLETON_TYPE_BYTE_LENGTH) {
            JS_ASSERT(obj->hasSingletonType());
        } else {
            jsbytecode *pc;
            RootedScript script(cx, cx->stack.currentScript(&pc));
            if (script) {
                if (!types::SetInitializerObjectType(cx, script, pc, obj, newKind))
                    return NULL;
            }
        }
    }

    JS_ASSERT(arrayBuffer->is<ArrayBufferObject>());

    DataViewObject &dvobj = obj->as<DataViewObject>();
    dvobj.setFixedSlot(BYTEOFFSET_SLOT, Int32Value(byteOffset));
    dvobj.setFixedSlot(BYTELENGTH_SLOT, Int32Value(byteLength));
    dvobj.setFixedSlot(BUFFER_SLOT, ObjectValue(*arrayBuffer));
    dvobj.setFixedSlot(NEXT_VIEW_SLOT, PrivateValue(NULL));
    dvobj.setFixedSlot(NEXT_BUFFER_SLOT, PrivateValue(UNSET_BUFFER_LINK));
    InitArrayBufferViewDataPointer(obj, arrayBuffer, byteOffset);
    JS_ASSERT(byteOffset + byteLength <= arrayBuffer->byteLength());

    // Verify that the private slot is at the expected place
    JS_ASSERT(dvobj.numFixedSlots() == DATA_SLOT);

    arrayBuffer->as<ArrayBufferObject>().addView(&dvobj);

    return &dvobj;
}

inline uint32_t
DataViewObject::byteLength()
{
    JS_ASSERT(JSObject::is<DataViewObject>());
    int32_t length = getReservedSlot(BYTELENGTH_SLOT).toInt32();
    JS_ASSERT(length >= 0);
    return static_cast<uint32_t>(length);
}

inline uint32_t
DataViewObject::byteOffset()
{
    JS_ASSERT(JSObject::is<DataViewObject>());
    int32_t offset = getReservedSlot(BYTEOFFSET_SLOT).toInt32();
    JS_ASSERT(offset >= 0);
    return static_cast<uint32_t>(offset);
}

inline void *
DataViewObject::dataPointer()
{
    JS_ASSERT(JSObject::is<DataViewObject>());
    return getPrivate();
}

inline ArrayBufferObject &
DataViewObject::arrayBuffer()
{
    JS_ASSERT(JSObject::is<DataViewObject>());
    return getReservedSlot(BUFFER_SLOT).toObject().as<ArrayBufferObject>();
}

inline bool
DataViewObject::hasBuffer() const
{
    JS_ASSERT(JSObject::is<DataViewObject>());
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
