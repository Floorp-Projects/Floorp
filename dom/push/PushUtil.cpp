/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushUtil.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla::dom {

/* static */
bool PushUtil::CopyBufferSourceToArray(
    const OwningArrayBufferViewOrArrayBuffer& aSource,
    nsTArray<uint8_t>& aArray) {
  MOZ_ASSERT(aArray.IsEmpty());
  return AppendTypedArrayDataTo(aSource, aArray);
}

/* static */
void PushUtil::CopyArrayToArrayBuffer(JSContext* aCx,
                                      const nsTArray<uint8_t>& aArray,
                                      JS::MutableHandle<JSObject*> aValue,
                                      ErrorResult& aRv) {
  if (aArray.IsEmpty()) {
    aValue.set(nullptr);
    return;
  }
  JS::Rooted<JSObject*> buffer(aCx, ArrayBuffer::Create(aCx, aArray, aRv));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  aValue.set(buffer);
}

}  // namespace mozilla::dom
