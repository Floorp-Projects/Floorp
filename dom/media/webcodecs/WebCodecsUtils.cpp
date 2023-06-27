/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebCodecsUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "nsDebug.h"

namespace mozilla::dom {

/*
 * The below are helpers to operate ArrayBuffer or ArrayBufferView.
 */
template <class T>
Result<Span<uint8_t>, nsresult> GetArrayBufferData(const T& aBuffer) {
  // Get buffer's data and length before using it.
  aBuffer.ComputeState();

  CheckedInt<size_t> byteLength(sizeof(typename T::element_type));
  byteLength *= aBuffer.Length();
  if (NS_WARN_IF(!byteLength.isValid())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  return Span<uint8_t>(aBuffer.Data(), byteLength.value());
}

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBufferView()) {
    return GetArrayBufferData(aBuffer.GetAsArrayBufferView());
  }

  MOZ_ASSERT(aBuffer.IsArrayBuffer());
  return GetArrayBufferData(aBuffer.GetAsArrayBuffer());
}

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer) {
  if (aBuffer.IsArrayBufferView()) {
    return GetArrayBufferData(aBuffer.GetAsArrayBufferView());
  }

  MOZ_ASSERT(aBuffer.IsArrayBuffer());
  return GetArrayBufferData(aBuffer.GetAsArrayBuffer());
}

}  // namespace mozilla::dom
