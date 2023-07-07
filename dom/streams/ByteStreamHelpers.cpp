/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ByteStreamHelpers.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"
#include "js/experimental/TypedData.h"
#include "mozilla/ErrorResult.h"

namespace mozilla::dom {

// https://streams.spec.whatwg.org/#transfer-array-buffer
// As some parts of the specifcation want to use the abrupt completion value,
// this function may leave a pending exception if it returns nullptr.
JSObject* TransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject) {
  MOZ_ASSERT(JS::IsArrayBufferObject(aObject));

  // Step 1.
  MOZ_ASSERT(!JS::IsDetachedArrayBufferObject(aObject));

  // Step 3 (Reordered)
  size_t bufferLength = JS::GetArrayBufferByteLength(aObject);

  // Step 2 (Reordered)
  UniquePtr<void, JS::FreePolicy> bufferData{
      JS::StealArrayBufferContents(aCx, aObject)};

  // Step 4.
  if (!JS::DetachArrayBuffer(aCx, aObject)) {
    return nullptr;
  }

  // Step 5.
  return JS::NewArrayBufferWithContents(aCx, bufferLength,
                                        std::move(bufferData));
}

// https://streams.spec.whatwg.org/#can-transfer-array-buffer
bool CanTransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject,
                            ErrorResult& aRv) {
  // Step 1. Assert: Type(O) is Object. (Implicit in types)
  // Step 2. Assert: O has an [[ArrayBufferData]] internal slot.
  MOZ_ASSERT(JS::IsArrayBufferObject(aObject));

  // Step 3. If ! IsDetachedBuffer(O) is true, return false.
  if (JS::IsDetachedArrayBufferObject(aObject)) {
    return false;
  }

  // Step 4. If SameValue(O.[[ArrayBufferDetachKey]], undefined) is false,
  // return false.
  // Step 5. Return true.
  // Note: WASM memories are the only buffers that would qualify
  // as having an [[ArrayBufferDetachKey]] which is not undefined.
  bool hasDefinedArrayBufferDetachKey = false;
  if (!JS::HasDefinedArrayBufferDetachKey(aCx, aObject,
                                          &hasDefinedArrayBufferDetachKey)) {
    aRv.StealExceptionFromJSContext(aCx);
    return false;
  }
  return !hasDefinedArrayBufferDetachKey;
}

// https://streams.spec.whatwg.org/#abstract-opdef-cloneasuint8array
JSObject* CloneAsUint8Array(JSContext* aCx, JS::Handle<JSObject*> aObject) {
  // Step 1. Assert: Type(O) is Object. Implicit.
  // Step 2. Assert: O has an [[ViewedArrayBuffer]] internal slot.
  MOZ_ASSERT(JS_IsArrayBufferViewObject(aObject));

  // Step 3. Assert: !IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is false.
  bool isShared;
  JS::Rooted<JSObject*> viewedArrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, aObject, &isShared));
  if (!viewedArrayBuffer) {
    return nullptr;
  }
  MOZ_ASSERT(!JS::IsDetachedArrayBufferObject(viewedArrayBuffer));

  // Step 4. Let buffer be ?CloneArrayBuffer(O.[[ViewedArrayBuffer]],
  //         O.[[ByteOffset]], O.[[ByteLength]], %ArrayBuffer%).
  size_t byteOffset = JS_GetTypedArrayByteOffset(aObject);
  size_t byteLength = JS_GetTypedArrayByteLength(aObject);
  JS::Rooted<JSObject*> buffer(
      aCx,
      JS::ArrayBufferClone(aCx, viewedArrayBuffer, byteOffset, byteLength));
  if (!buffer) {
    return nullptr;
  }

  // Step 5. Let array be ! Construct(%Uint8Array%, « buffer »).
  JS::Rooted<JSObject*> array(
      aCx, JS_NewUint8ArrayWithBuffer(aCx, buffer, 0,
                                      static_cast<int64_t>(byteLength)));
  if (!array) {
    return nullptr;
  }

  // Step 6. Return array.
  return array;
}

}  // namespace mozilla::dom
