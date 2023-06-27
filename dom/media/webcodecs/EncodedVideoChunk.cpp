/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(EncodedVideoChunk)
NS_IMPL_CYCLE_COLLECTING_ADDREF(EncodedVideoChunk)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EncodedVideoChunk)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EncodedVideoChunk)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsIGlobalObject* EncodedVideoChunk::GetParentObject() const { return nullptr; }

JSObject* EncodedVideoChunk::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return EncodedVideoChunk_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<EncodedVideoChunk> EncodedVideoChunk::Constructor(
    const GlobalObject& global, const EncodedVideoChunkInit& init,
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

EncodedVideoChunkType EncodedVideoChunk::Type() const {
  return EncodedVideoChunkType::EndGuard_;
}

int64_t EncodedVideoChunk::Timestamp() const { return 0; }

Nullable<uint64_t> EncodedVideoChunk::GetDuration() const { return nullptr; }

uint32_t EncodedVideoChunk::ByteLength() const { return 0; }

void EncodedVideoChunk::CopyTo(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& destination,
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

}  // namespace mozilla::dom
