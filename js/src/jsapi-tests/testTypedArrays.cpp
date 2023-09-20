/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"  // JS::{NewArrayBuffer,IsArrayBufferObject,GetArrayBuffer{ByteLength,Data}}
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferViewBuffer, JS_GetTypedArray{Length,ByteOffset,ByteLength}, JS_Get{{Ui,I}nt{8,16,32},Float{32,64},Uint8Clamped}ArrayData, JS_IsTypedArrayObject, JS_New{{Ui,I}nt{8,16,32},Float{32,64},Uint8Clamped}Array{,FromArray,WithBuffer}
#include "js/PropertyAndElement.h"      // JS_GetElement, JS_SetElement
#include "js/SharedArrayBuffer.h"  // JS::{NewSharedArrayBuffer,GetSharedArrayBufferData}
#include "jsapi-tests/tests.h"
#include "vm/Realm.h"

using namespace js;

BEGIN_TEST(testTypedArrays) {
  bool ok = true;

  ok = ok &&
       TestPlainTypedArray<JS_NewInt8Array, int8_t, JS_GetInt8ArrayData>(cx) &&
       TestPlainTypedArray<JS_NewUint8Array, uint8_t, JS_GetUint8ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewUint8ClampedArray, uint8_t,
                           JS_GetUint8ClampedArrayData>(cx) &&
       TestPlainTypedArray<JS_NewInt16Array, int16_t, JS_GetInt16ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewUint16Array, uint16_t, JS_GetUint16ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewInt32Array, int32_t, JS_GetInt32ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewUint32Array, uint32_t, JS_GetUint32ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewFloat32Array, float, JS_GetFloat32ArrayData>(
           cx) &&
       TestPlainTypedArray<JS_NewFloat64Array, double, JS_GetFloat64ArrayData>(
           cx);

  size_t nbytes = sizeof(double) * 8;
  RootedObject buffer(cx, JS::NewArrayBuffer(cx, nbytes));
  CHECK(JS::IsArrayBufferObject(buffer));

  RootedObject proto(cx);
  JS_GetPrototype(cx, buffer, &proto);
  CHECK(!JS::IsArrayBufferObject(proto));

  {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    CHECK_EQUAL(JS::GetArrayBufferByteLength(buffer), nbytes);
    memset(JS::GetArrayBufferData(buffer, &isShared, nogc), 1, nbytes);
    CHECK(!isShared);  // Because ArrayBuffer
  }

  ok =
      ok &&
      TestArrayFromBuffer<JS_NewInt8ArrayWithBuffer, JS_NewInt8ArrayFromArray,
                          int8_t, false, JS_GetInt8ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint8ArrayWithBuffer, JS_NewUint8ArrayFromArray,
                          uint8_t, false, JS_GetUint8ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint8ClampedArrayWithBuffer,
                          JS_NewUint8ClampedArrayFromArray, uint8_t, false,
                          JS_GetUint8ClampedArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewInt16ArrayWithBuffer, JS_NewInt16ArrayFromArray,
                          int16_t, false, JS_GetInt16ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint16ArrayWithBuffer,
                          JS_NewUint16ArrayFromArray, uint16_t, false,
                          JS_GetUint16ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewInt32ArrayWithBuffer, JS_NewInt32ArrayFromArray,
                          int32_t, false, JS_GetInt32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint32ArrayWithBuffer,
                          JS_NewUint32ArrayFromArray, uint32_t, false,
                          JS_GetUint32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewFloat32ArrayWithBuffer,
                          JS_NewFloat32ArrayFromArray, float, false,
                          JS_GetFloat32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewFloat64ArrayWithBuffer,
                          JS_NewFloat64ArrayFromArray, double, false,
                          JS_GetFloat64ArrayData>(cx);

  ok =
      ok &&
      TestArrayFromBuffer<JS_NewInt8ArrayWithBuffer, JS_NewInt8ArrayFromArray,
                          int8_t, true, JS_GetInt8ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint8ArrayWithBuffer, JS_NewUint8ArrayFromArray,
                          uint8_t, true, JS_GetUint8ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint8ClampedArrayWithBuffer,
                          JS_NewUint8ClampedArrayFromArray, uint8_t, true,
                          JS_GetUint8ClampedArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewInt16ArrayWithBuffer, JS_NewInt16ArrayFromArray,
                          int16_t, true, JS_GetInt16ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint16ArrayWithBuffer,
                          JS_NewUint16ArrayFromArray, uint16_t, true,
                          JS_GetUint16ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewInt32ArrayWithBuffer, JS_NewInt32ArrayFromArray,
                          int32_t, true, JS_GetInt32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewUint32ArrayWithBuffer,
                          JS_NewUint32ArrayFromArray, uint32_t, true,
                          JS_GetUint32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewFloat32ArrayWithBuffer,
                          JS_NewFloat32ArrayFromArray, float, true,
                          JS_GetFloat32ArrayData>(cx) &&
      TestArrayFromBuffer<JS_NewFloat64ArrayWithBuffer,
                          JS_NewFloat64ArrayFromArray, double, true,
                          JS_GetFloat64ArrayData>(cx);

  return ok;
}

// Test pinning a view's length.
bool TestViewLengthPinning(Handle<JSObject*> view) {
  // Pin the length of an inline view. (Fails if shared memory.)
  bool isShared = view.as<NativeObject>()->isSharedMemory();
  CHECK(JS::PinArrayBufferOrViewLength(view, true) == !isShared);

  // Fail to pin an already-pinned length.
  CHECK(!JS::PinArrayBufferOrViewLength(view, true));

  // Extract an ArrayBuffer. This may cause it to be created, in which case it
  // will inherit the pinned status from the view.
  bool bufferIsShared;
  Rooted<JSObject*> buffer(
      cx, JS_GetArrayBufferViewBuffer(cx, view, &bufferIsShared));
  CHECK(isShared == bufferIsShared);

  // Cannot pin the buffer, since it is already pinned.
  CHECK(!JS::PinArrayBufferOrViewLength(buffer, true));

  // Should fail to be detached, since its length is pinned.
  CHECK(!JS::DetachArrayBuffer(cx, buffer));
  CHECK(cx->isExceptionPending());
  cx->clearPendingException();

  // Unpin (fails if shared memory).
  CHECK(JS::PinArrayBufferOrViewLength(view, false) == !isShared);

  // Fail to unpin when already unpinned.
  CHECK(!JS::PinArrayBufferOrViewLength(view, false));

  return true;
}

// Test pinning the length of an ArrayBuffer or SharedArrayBuffer.
bool TestBufferLengthPinning(Handle<JSObject*> buffer) {
  // Pin the length of an inline view. (Fails if shared memory.)
  bool isShared = !buffer->is<ArrayBufferObject>();
  CHECK(JS::PinArrayBufferOrViewLength(buffer, true) == !isShared);

  // Fail to pin an already-pinned length.
  CHECK(!JS::PinArrayBufferOrViewLength(buffer, true));

  // Should fail to be detached, since its length is pinned.
  CHECK(!JS::DetachArrayBuffer(cx, buffer));
  CHECK(cx->isExceptionPending());
  cx->clearPendingException();

  // Unpin (fails if shared memory).
  CHECK(JS::PinArrayBufferOrViewLength(buffer, false) == !isShared);

  // Fail to unpin when already unpinned.
  CHECK(!JS::PinArrayBufferOrViewLength(buffer, false));

  return true;
}

// Shared memory can only be mapped by a TypedArray by creating the
// TypedArray with a SharedArrayBuffer explicitly, so no tests here.

template <JSObject* Create(JSContext*, size_t), typename Element,
          Element* GetData(JSObject*, bool* isShared,
                           const JS::AutoRequireNoGC&)>
bool TestPlainTypedArray(JSContext* cx) {
  {
    RootedObject notArray(cx, Create(cx, SIZE_MAX));
    CHECK(!notArray);
    JS_ClearPendingException(cx);
  }

  RootedObject array(cx, Create(cx, 7));
  CHECK(JS_IsTypedArrayObject(array));
  RootedObject proto(cx);
  JS_GetPrototype(cx, array, &proto);
  CHECK(!JS_IsTypedArrayObject(proto));

  CHECK_EQUAL(JS_GetTypedArrayLength(array), 7u);
  CHECK_EQUAL(JS_GetTypedArrayByteOffset(array), 0u);
  CHECK_EQUAL(JS_GetTypedArrayByteLength(array), sizeof(Element) * 7);

  TestViewLengthPinning(array);

  {
    JS::AutoCheckCannotGC nogc;
    Element* data;
    bool isShared;
    CHECK(data = GetData(array, &isShared, nogc));
    CHECK(!isShared);  // Because ArrayBuffer
    *data = 13;
  }
  RootedValue v(cx);
  CHECK(JS_GetElement(cx, array, 0, &v));
  CHECK_SAME(v, Int32Value(13));

  return true;
}

template <
    JSObject* CreateWithBuffer(JSContext*, JS::HandleObject, size_t, int64_t),
    JSObject* CreateFromArray(JSContext*, JS::HandleObject), typename Element,
    bool Shared, Element* GetData(JSObject*, bool*, const JS::AutoRequireNoGC&)>
bool TestArrayFromBuffer(JSContext* cx) {
  if (Shared &&
      !cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()) {
    return true;
  }

  size_t elts = 8;
  size_t nbytes = elts * sizeof(Element);
  RootedObject buffer(cx, Shared ? JS::NewSharedArrayBuffer(cx, nbytes)
                                 : JS::NewArrayBuffer(cx, nbytes));

  TestBufferLengthPinning(buffer);

  {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    void* data = Shared ? JS::GetSharedArrayBufferData(buffer, &isShared, nogc)
                        : JS::GetArrayBufferData(buffer, &isShared, nogc);
    CHECK_EQUAL(Shared, isShared);
    memset(data, 1, nbytes);
  }

  {
    RootedObject notArray(cx, CreateWithBuffer(cx, buffer, UINT32_MAX, -1));
    CHECK(!notArray);
    JS_ClearPendingException(cx);
  }

  RootedObject array(cx, CreateWithBuffer(cx, buffer, 0, -1));
  CHECK_EQUAL(JS_GetTypedArrayLength(array), elts);
  CHECK_EQUAL(JS_GetTypedArrayByteOffset(array), 0u);
  CHECK_EQUAL(JS_GetTypedArrayByteLength(array), nbytes);
  {
    bool isShared;
    CHECK_EQUAL(JS_GetArrayBufferViewBuffer(cx, array, &isShared),
                (JSObject*)buffer);
    CHECK_EQUAL(Shared, isShared);
  }

  TestViewLengthPinning(array);

  {
    JS::AutoCheckCannotGC nogc;
    Element* data;
    bool isShared;

    CHECK(data = GetData(array, &isShared, nogc));
    CHECK_EQUAL(Shared, isShared);

    CHECK_EQUAL(
        (void*)data,
        Shared ? (void*)JS::GetSharedArrayBufferData(buffer, &isShared, nogc)
               : (void*)JS::GetArrayBufferData(buffer, &isShared, nogc));
    CHECK_EQUAL(Shared, isShared);

    CHECK_EQUAL(*reinterpret_cast<uint8_t*>(data), 1u);
  }

  RootedObject shortArray(cx, CreateWithBuffer(cx, buffer, 0, elts / 2));
  CHECK_EQUAL(JS_GetTypedArrayLength(shortArray), elts / 2);
  CHECK_EQUAL(JS_GetTypedArrayByteOffset(shortArray), 0u);
  CHECK_EQUAL(JS_GetTypedArrayByteLength(shortArray), nbytes / 2);

  RootedObject ofsArray(cx, CreateWithBuffer(cx, buffer, nbytes / 2, -1));
  CHECK_EQUAL(JS_GetTypedArrayLength(ofsArray), elts / 2);
  CHECK_EQUAL(JS_GetTypedArrayByteOffset(ofsArray), nbytes / 2);
  CHECK_EQUAL(JS_GetTypedArrayByteLength(ofsArray), nbytes / 2);

  // Make sure all 3 views reflect the same buffer at the expected locations
  JS::RootedValue v(cx, JS::Int32Value(39));
  CHECK(JS_SetElement(cx, array, 0, v));
  JS::RootedValue v2(cx);
  CHECK(JS_GetElement(cx, array, 0, &v2));
  CHECK_SAME(v, v2);
  CHECK(JS_GetElement(cx, shortArray, 0, &v2));
  CHECK_SAME(v, v2);
  {
    JS::AutoCheckCannotGC nogc;
    Element* data;
    bool isShared;
    CHECK(data = GetData(array, &isShared, nogc));
    CHECK_EQUAL(Shared, isShared);
    CHECK_EQUAL(long(v.toInt32()), long(reinterpret_cast<Element*>(data)[0]));
  }

  v.setInt32(40);
  CHECK(JS_SetElement(cx, array, elts / 2, v));
  CHECK(JS_GetElement(cx, array, elts / 2, &v2));
  CHECK_SAME(v, v2);
  CHECK(JS_GetElement(cx, ofsArray, 0, &v2));
  CHECK_SAME(v, v2);
  {
    JS::AutoCheckCannotGC nogc;
    Element* data;
    bool isShared;
    CHECK(data = GetData(array, &isShared, nogc));
    CHECK_EQUAL(Shared, isShared);
    CHECK_EQUAL(long(v.toInt32()),
                long(reinterpret_cast<Element*>(data)[elts / 2]));
  }

  v.setInt32(41);
  CHECK(JS_SetElement(cx, array, elts - 1, v));
  CHECK(JS_GetElement(cx, array, elts - 1, &v2));
  CHECK_SAME(v, v2);
  CHECK(JS_GetElement(cx, ofsArray, elts / 2 - 1, &v2));
  CHECK_SAME(v, v2);
  {
    JS::AutoCheckCannotGC nogc;
    Element* data;
    bool isShared;
    CHECK(data = GetData(array, &isShared, nogc));
    CHECK_EQUAL(Shared, isShared);
    CHECK_EQUAL(long(v.toInt32()),
                long(reinterpret_cast<Element*>(data)[elts - 1]));
  }

  JS::RootedObject copy(cx, CreateFromArray(cx, array));
  CHECK(JS_GetElement(cx, array, 0, &v));
  CHECK(JS_GetElement(cx, copy, 0, &v2));
  CHECK_SAME(v, v2);

  /* The copy should not see changes in the original */
  v2.setInt32(42);
  CHECK(JS_SetElement(cx, array, 0, v2));
  CHECK(JS_GetElement(cx, copy, 0, &v2));
  CHECK_SAME(v2, v); /* v is still the original value from 'array' */

  return true;
}

END_TEST(testTypedArrays)
