/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferViewObject.h"

#include "builtin/DataViewObject.h"
#include "gc/Nursery.h"
#include "vm/JSContext.h"
#include "vm/TypedArrayObject.h"

#include "gc/Nursery-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

/*
 * This method is used to trace TypedArrayObjects and DataViewObjects. We need
 * a custom tracer to move the object's data pointer if its owner was moved and
 * stores its data inline.
 */
/* static */ void
ArrayBufferViewObject::trace(JSTracer* trc, JSObject* objArg)
{
    NativeObject* obj = &objArg->as<NativeObject>();
    HeapSlot& bufSlot = obj->getFixedSlotRef(BUFFER_SLOT);
    TraceEdge(trc, &bufSlot, "ArrayBufferViewObject.buffer");

    // Update obj's data pointer if it moved.
    if (bufSlot.isObject()) {
        if (IsArrayBuffer(&bufSlot.toObject())) {
            ArrayBufferObject& buf = AsArrayBuffer(MaybeForwarded(&bufSlot.toObject()));
            uint32_t offset = uint32_t(obj->getFixedSlot(BYTEOFFSET_SLOT).toInt32());
            MOZ_ASSERT(offset <= INT32_MAX);

            // We don't expose the underlying ArrayBuffer for typed objects,
            // and we don't allow constructing a TypedObject from an arbitrary
            // ArrayBuffer, so we should never have a TypedArray/DataView with
            // a buffer that has TypedObject views.
            MOZ_RELEASE_ASSERT(!buf.forInlineTypedObject());

            MOZ_ASSERT_IF(buf.dataPointer() == nullptr, offset == 0);

            // The data may or may not be inline with the buffer. The buffer
            // can only move during a compacting GC, in which case its
            // objectMoved hook has already updated the buffer's data pointer.
            size_t nfixed = obj->numFixedSlotsMaybeForwarded();
            obj->setPrivateUnbarriered(nfixed, buf.dataPointer() + offset);
        }
    }
}

template <>
bool
JSObject::is<js::ArrayBufferViewObject>() const
{
    return is<DataViewObject>() || is<TypedArrayObject>();
}

void
ArrayBufferViewObject::notifyBufferDetached(JSContext* cx, void* newData)
{
    if (isSharedMemory()) {
        return;
    }

    MOZ_ASSERT(!isSharedMemory());
    setFixedSlot(LENGTH_SLOT, Int32Value(0));
    setFixedSlot(BYTEOFFSET_SLOT, Int32Value(0));

    // If the object is in the nursery, the buffer will be freed by the next
    // nursery GC. Free the data slot pointer if the object has no inline data.
    if (is<TypedArrayObject>()) {
        TypedArrayObject& tarr = as<TypedArrayObject>();
        Nursery& nursery = cx->nursery();
        if (isTenured() && !hasBuffer() && !tarr.hasInlineElements() &&
            !nursery.isInside(tarr.elements()))
        {
            js_free(tarr.elements());
        }
    }

    setPrivate(newData);
}

uint8_t*
ArrayBufferViewObject::dataPointerUnshared(const JS::AutoRequireNoGC& nogc)
{
    return static_cast<uint8_t*>(dataPointerUnshared());
}

void
ArrayBufferViewObject::setDataPointerUnshared(uint8_t* data)
{
    MOZ_ASSERT(!isSharedMemory());
    setPrivate(data);
}

/* static */ ArrayBufferObjectMaybeShared*
ArrayBufferViewObject::bufferObject(JSContext* cx, Handle<ArrayBufferViewObject*> thisObject)
{
    if (thisObject->is<TypedArrayObject>()) {
        Rooted<TypedArrayObject*> typedArray(cx, &thisObject->as<TypedArrayObject>());
        if (!TypedArrayObject::ensureHasBuffer(cx, typedArray)) {
            return nullptr;
        }
    }
    return thisObject->bufferEither();
}

/* JS Friend API */

JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj && obj->is<ArrayBufferViewObject>();
}

JS_FRIEND_API(JSObject*)
js::UnwrapArrayBufferView(JSObject* obj)
{
    if (JSObject* unwrapped = CheckedUnwrap(obj)) {
        return unwrapped->is<ArrayBufferViewObject>() ? unwrapped : nullptr;
    }
    return nullptr;
}

JS_FRIEND_API(void*)
JS_GetArrayBufferViewData(JSObject* obj, bool* isSharedMemory, const JS::AutoRequireNoGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj) {
        return nullptr;
    }

    ArrayBufferViewObject& view = obj->as<ArrayBufferViewObject>();
    *isSharedMemory = view.isSharedMemory();
    return view.dataPointerEither().unwrap(/*safe - caller sees isSharedMemory flag*/);
}

JS_FRIEND_API(JSObject*)
JS_GetArrayBufferViewBuffer(JSContext* cx, HandleObject objArg, bool* isSharedMemory)
{
    AssertHeapIsIdle();
    CHECK_THREAD(cx);
    cx->check(objArg);

    JSObject* obj = CheckedUnwrap(objArg);
    if (!obj) {
        return nullptr;
    }
    Rooted<ArrayBufferViewObject*> viewObject(cx, &obj->as<ArrayBufferViewObject>());
    ArrayBufferObjectMaybeShared* buffer = ArrayBufferViewObject::bufferObject(cx, viewObject);
    *isSharedMemory = buffer->is<SharedArrayBufferObject>();
    return buffer;
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj) {
        return 0;
    }
    return obj->is<DataViewObject>()
           ? obj->as<DataViewObject>().byteLength()
           : obj->as<TypedArrayObject>().byteLength();
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteOffset(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj) {
        return 0;
    }
    return obj->is<DataViewObject>()
           ? obj->as<DataViewObject>().byteOffset()
           : obj->as<TypedArrayObject>().byteOffset();
}

JS_FRIEND_API(JSObject*)
JS_GetObjectAsArrayBufferView(JSObject* obj, uint32_t* length, bool* isSharedMemory, uint8_t** data)
{
    if (!(obj = CheckedUnwrap(obj))) {
        return nullptr;
    }
    if (!(obj->is<ArrayBufferViewObject>())) {
        return nullptr;
    }

    js::GetArrayBufferViewLengthAndData(obj, length, isSharedMemory, data);
    return obj;
}

JS_FRIEND_API(void)
js::GetArrayBufferViewLengthAndData(JSObject* obj, uint32_t* length, bool* isSharedMemory,
                                    uint8_t** data)
{
    MOZ_ASSERT(obj->is<ArrayBufferViewObject>());

    *length = obj->is<DataViewObject>()
              ? obj->as<DataViewObject>().byteLength()
              : obj->as<TypedArrayObject>().byteLength();

    ArrayBufferViewObject& view = obj->as<ArrayBufferViewObject>();
    *isSharedMemory = view.isSharedMemory();
    *data = static_cast<uint8_t*>(
            view.dataPointerEither().unwrap(/*safe - caller sees isShared flag*/));
}
