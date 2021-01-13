/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferViewObject.h"

#include "builtin/DataViewObject.h"
#include "gc/Nursery.h"
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferView{Data,Buffer,Length,ByteOffset}, JS_GetObjectAsArrayBufferView, JS_IsArrayBufferViewObject
#include "js/SharedArrayBuffer.h"
#include "vm/JSContext.h"
#include "vm/TypedArrayObject.h"

#include "gc/Nursery-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

/*
 * This method is used to trace TypedArrayObjects and DataViewObjects. We need
 * a custom tracer to move the object's data pointer if its owner was moved and
 * stores its data inline.
 */
/* static */
void ArrayBufferViewObject::trace(JSTracer* trc, JSObject* objArg) {
  ArrayBufferViewObject* obj = &objArg->as<ArrayBufferViewObject>();
  HeapSlot& bufSlot = obj->getFixedSlotRef(BUFFER_SLOT);
  TraceEdge(trc, &bufSlot, "ArrayBufferViewObject.buffer");

  // Update obj's data pointer if it moved.
  if (bufSlot.isObject()) {
    if (gc::MaybeForwardedObjectIs<ArrayBufferObject>(&bufSlot.toObject())) {
      ArrayBufferObject& buf =
          gc::MaybeForwardedObjectAs<ArrayBufferObject>(&bufSlot.toObject());
      BufferSize offset = obj->byteOffset();

      MOZ_ASSERT_IF(buf.dataPointer() == nullptr, offset.get() == 0);

      // The data may or may not be inline with the buffer. The buffer
      // can only move during a compacting GC, in which case its
      // objectMoved hook has already updated the buffer's data pointer.
      size_t nfixed = obj->numFixedSlotsMaybeForwarded();
      obj->setPrivateUnbarriered(nfixed, buf.dataPointer() + offset.get());
    }
  }
}

template <>
bool JSObject::is<js::ArrayBufferViewObject>() const {
  return is<DataViewObject>() || is<TypedArrayObject>();
}

void ArrayBufferViewObject::notifyBufferDetached() {
  MOZ_ASSERT(!isSharedMemory());
  MOZ_ASSERT(hasBuffer());

  setFixedSlot(LENGTH_SLOT, PrivateValue(size_t(0)));
  setFixedSlot(BYTEOFFSET_SLOT, PrivateValue(size_t(0)));

  setPrivate(nullptr);
}

/* static */
ArrayBufferObjectMaybeShared* ArrayBufferViewObject::bufferObject(
    JSContext* cx, Handle<ArrayBufferViewObject*> thisObject) {
  if (thisObject->is<TypedArrayObject>()) {
    Rooted<TypedArrayObject*> typedArray(cx,
                                         &thisObject->as<TypedArrayObject>());
    if (!TypedArrayObject::ensureHasBuffer(cx, typedArray)) {
      return nullptr;
    }
  }
  return thisObject->bufferEither();
}

bool ArrayBufferViewObject::init(JSContext* cx,
                                 ArrayBufferObjectMaybeShared* buffer,
                                 BufferSize byteOffset, BufferSize length,
                                 uint32_t bytesPerElement) {
  MOZ_ASSERT_IF(!buffer, byteOffset.get() == 0);
  MOZ_ASSERT_IF(buffer, !buffer->isDetached());

  MOZ_ASSERT(byteOffset.get() <= ArrayBufferObject::maxBufferByteLength());
  MOZ_ASSERT(length.get() <= ArrayBufferObject::maxBufferByteLength());
  MOZ_ASSERT(byteOffset.get() + length.get() <=
             ArrayBufferObject::maxBufferByteLength());

  MOZ_ASSERT_IF(
      is<TypedArrayObject>(),
      length.get() < TypedArrayObject::maxByteLength() / bytesPerElement);

  // The isSharedMemory property is invariant.  Self-hosting code that
  // sets BUFFER_SLOT or the private slot (if it does) must maintain it by
  // always setting those to reference shared memory.
  if (buffer && buffer->is<SharedArrayBufferObject>()) {
    setIsSharedMemory();
  }

  initFixedSlot(BYTEOFFSET_SLOT, PrivateValue(byteOffset.get()));
  initFixedSlot(LENGTH_SLOT, PrivateValue(length.get()));
  initFixedSlot(BUFFER_SLOT, ObjectOrNullValue(buffer));

  if (buffer) {
    SharedMem<uint8_t*> ptr = buffer->dataPointerEither();
    initDataPointer(ptr + byteOffset.get());

    // Only ArrayBuffers used for inline typed objects can have
    // nursery-allocated data and we shouldn't see such buffers here.
    MOZ_ASSERT_IF(buffer->byteLength().get() > 0, !cx->nursery().isInside(ptr));
  } else {
    MOZ_ASSERT(is<TypedArrayObject>());
    MOZ_ASSERT(length.get() * bytesPerElement <=
               TypedArrayObject::INLINE_BUFFER_LIMIT);
    void* data = fixedData(TypedArrayObject::FIXED_DATA_START);
    initPrivate(data);
    memset(data, 0, length.get() * bytesPerElement);
#ifdef DEBUG
    if (length.get() == 0) {
      uint8_t* elements = static_cast<uint8_t*>(data);
      elements[0] = ZeroLengthArrayData;
    }
#endif
  }

#ifdef DEBUG
  if (buffer) {
    size_t viewByteLength = length.get() * bytesPerElement;
    size_t viewByteOffset = byteOffset.get();
    size_t bufferByteLength = buffer->byteLength().get();
    // Unwraps are safe: both are for the pointer value.
    MOZ_ASSERT_IF(IsArrayBuffer(buffer),
                  buffer->dataPointerEither().unwrap(/*safe*/) <=
                      dataPointerEither().unwrap(/*safe*/));
    MOZ_ASSERT(bufferByteLength - viewByteOffset >= viewByteLength);
    MOZ_ASSERT(viewByteOffset <= bufferByteLength);
  }

  // Verify that the private slot is at the expected place.
  MOZ_ASSERT(numFixedSlots() == DATA_SLOT);
#endif

  // ArrayBufferObjects track their views to support detaching.
  if (buffer && buffer->is<ArrayBufferObject>()) {
    if (!buffer->as<ArrayBufferObject>().addView(cx, this)) {
      return false;
    }
  }

  return true;
}

/* JS Friend API */

JS_FRIEND_API bool JS_IsArrayBufferViewObject(JSObject* obj) {
  return obj->canUnwrapAs<ArrayBufferViewObject>();
}

JS_FRIEND_API JSObject* js::UnwrapArrayBufferView(JSObject* obj) {
  return obj->maybeUnwrapIf<ArrayBufferViewObject>();
}

JS_FRIEND_API void* JS_GetArrayBufferViewData(JSObject* obj,
                                              bool* isSharedMemory,
                                              const JS::AutoRequireNoGC&) {
  ArrayBufferViewObject* view = obj->maybeUnwrapAs<ArrayBufferViewObject>();
  if (!view) {
    return nullptr;
  }

  *isSharedMemory = view->isSharedMemory();
  return view->dataPointerEither().unwrap(
      /*safe - caller sees isSharedMemory flag*/);
}

JS_FRIEND_API JSObject* JS_GetArrayBufferViewBuffer(JSContext* cx,
                                                    HandleObject obj,
                                                    bool* isSharedMemory) {
  AssertHeapIsIdle();
  CHECK_THREAD(cx);
  cx->check(obj);

  Rooted<ArrayBufferViewObject*> unwrappedView(
      cx, obj->maybeUnwrapAs<ArrayBufferViewObject>());
  if (!unwrappedView) {
    ReportAccessDenied(cx);
    return nullptr;
  }

  ArrayBufferObjectMaybeShared* unwrappedBuffer;
  {
    AutoRealm ar(cx, unwrappedView);
    unwrappedBuffer = ArrayBufferViewObject::bufferObject(cx, unwrappedView);
    if (!unwrappedBuffer) {
      return nullptr;
    }
  }
  *isSharedMemory = unwrappedBuffer->is<SharedArrayBufferObject>();

  RootedObject buffer(cx, unwrappedBuffer);
  if (!cx->compartment()->wrap(cx, &buffer)) {
    return nullptr;
  }

  return buffer;
}

JS_FRIEND_API uint32_t JS_GetArrayBufferViewByteLength(JSObject* obj) {
  obj = obj->maybeUnwrapAs<ArrayBufferViewObject>();
  if (!obj) {
    return 0;
  }
  BufferSize length = obj->is<DataViewObject>()
                          ? obj->as<DataViewObject>().byteLength()
                          : obj->as<TypedArrayObject>().byteLength();
  return length.deprecatedGetUint32();
}

JS_FRIEND_API uint32_t JS_GetArrayBufferViewByteOffset(JSObject* obj) {
  obj = obj->maybeUnwrapAs<ArrayBufferViewObject>();
  if (!obj) {
    return 0;
  }
  BufferSize offset = obj->is<DataViewObject>()
                          ? obj->as<DataViewObject>().byteOffset()
                          : obj->as<TypedArrayObject>().byteOffset();
  return offset.deprecatedGetUint32();
}

JS_FRIEND_API JSObject* JS_GetObjectAsArrayBufferView(JSObject* obj,
                                                      uint32_t* length,
                                                      bool* isSharedMemory,
                                                      uint8_t** data) {
  obj = obj->maybeUnwrapIf<ArrayBufferViewObject>();
  if (!obj) {
    return nullptr;
  }

  js::GetArrayBufferViewLengthAndData(obj, length, isSharedMemory, data);
  return obj;
}

JS_FRIEND_API void js::GetArrayBufferViewLengthAndData(JSObject* obj,
                                                       uint32_t* length,
                                                       bool* isSharedMemory,
                                                       uint8_t** data) {
  MOZ_ASSERT(obj->is<ArrayBufferViewObject>());

  BufferSize byteLength = obj->is<DataViewObject>()
                              ? obj->as<DataViewObject>().byteLength()
                              : obj->as<TypedArrayObject>().byteLength();
  *length = byteLength.deprecatedGetUint32();

  ArrayBufferViewObject& view = obj->as<ArrayBufferViewObject>();
  *isSharedMemory = view.isSharedMemory();
  *data = static_cast<uint8_t*>(
      view.dataPointerEither().unwrap(/*safe - caller sees isShared flag*/));
}

JS_PUBLIC_API bool JS::IsArrayBufferViewShared(JSObject* obj) {
  ArrayBufferViewObject* view = obj->maybeUnwrapAs<ArrayBufferViewObject>();
  if (!view) {
    return false;
  }
  return view->isSharedMemory();
}
