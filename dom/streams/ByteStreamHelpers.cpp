/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ByteStreamHelpers.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

// https://streams.spec.whatwg.org/#transfer-array-buffer
// As some parts of the specifcation want to use the abrupt completion value,
// this function may leave a pending exception if it returns nullptr.
JSObject* TransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject) {
  // Step 1.
  MOZ_ASSERT(!JS::IsDetachedArrayBufferObject(aObject));

  // Step 2+3.
  uint8_t* bufferData = nullptr;
  size_t bufferLength = 0;
  bool isSharedMemory = false;
  JS::GetArrayBufferLengthAndData(aObject, &bufferLength, &isSharedMemory,
                                  &bufferData);

  // Step 4.
  if (!JS::DetachArrayBuffer(aCx, aObject)) {
    return nullptr;
  }

  // Step 5.
  return JS::NewArrayBufferWithContents(aCx, bufferLength, bufferData);
}

// https://streams.spec.whatwg.org/#can-transfer-array-buffer
bool CanTransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject,
                            ErrorResult& aRv) {
  // Step 1. Implicit in types.
  // Step 2.
  MOZ_ASSERT(JS::IsArrayBufferObject(aObject));

  // Step 3.
  if (JS::IsDetachedArrayBufferObject(aObject)) {
    return false;
  }

  // Step 4. WASM memories are the only buffers that would qualify
  // as having an undefined [[ArrayBufferDetachKey]],
  bool hasDefinedArrayBufferDetachKey;
  if (!JS::HasDefinedArrayBufferDetachKey(aCx, aObject,
                                          &hasDefinedArrayBufferDetachKey)) {
    aRv.StealExceptionFromJSContext(aCx);
    return false;
  }
  if (hasDefinedArrayBufferDetachKey) {
    return false;
  }

  // Step 5.
  return true;
}

}  // namespace dom
}  // namespace mozilla
