/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/EncodedVideoChunkBinding.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/PodOperations.h"
#include "mozilla/dom/WebCodecsUtils.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(EncodedVideoChunk, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(EncodedVideoChunk)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EncodedVideoChunk)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EncodedVideoChunk)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

EncodedVideoChunk::EncodedVideoChunk(nsIGlobalObject* aParent,
                                     UniquePtr<uint8_t[]>&& aBuffer,
                                     size_t aByteLength,
                                     const EncodedVideoChunkType& aType,
                                     int64_t aTimestamp,
                                     Maybe<uint64_t>&& aDuration)
    : mParent(aParent),
      mBuffer(std::move(aBuffer)),
      mByteLength(aByteLength),
      mType(aType),
      mTimestamp(aTimestamp),
      mDuration(std::move(aDuration)) {
  MOZ_ASSERT(mByteLength <=
             static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
}

nsIGlobalObject* EncodedVideoChunk::GetParentObject() const {
  AssertIsOnOwningThread();

  return mParent.get();
}

JSObject* EncodedVideoChunk::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return EncodedVideoChunk_Binding::Wrap(aCx, this, aGivenProto);
}

// https://w3c.github.io/webcodecs/#encodedvideochunk-constructors
/* static */
already_AddRefed<EncodedVideoChunk> EncodedVideoChunk::Constructor(
    const GlobalObject& aGlobal, const EncodedVideoChunkInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  auto r = GetSharedArrayBufferData(aInit.mData);
  if (r.isErr()) {
    aRv.Throw(r.unwrapErr());
    return nullptr;
  }
  Span<uint8_t> buf = r.unwrap();

  // Make sure it's in uint32_t's range.
  CheckedUint32 byteLength(buf.size_bytes());
  if (!byteLength.isValid()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  UniquePtr<uint8_t[]> buffer(new (fallible) uint8_t[buf.size_bytes()]);
  if (!buffer) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  PodCopy(buffer.get(), buf.data(), buf.size_bytes());

  RefPtr<EncodedVideoChunk> chunk(new EncodedVideoChunk(
      global, std::move(buffer), buf.size_bytes(), aInit.mType,
      aInit.mTimestamp,
      aInit.mDuration.WasPassed() ? Some(aInit.mDuration.Value()) : Nothing()));
  return aRv.Failed() ? nullptr : chunk.forget();
}

EncodedVideoChunkType EncodedVideoChunk::Type() const {
  AssertIsOnOwningThread();

  return mType;
}

int64_t EncodedVideoChunk::Timestamp() const {
  AssertIsOnOwningThread();

  return mTimestamp;
}

Nullable<uint64_t> EncodedVideoChunk::GetDuration() const {
  AssertIsOnOwningThread();

  if (mDuration) {
    return Nullable<uint64_t>(*mDuration);
  }
  return nullptr;
}

uint32_t EncodedVideoChunk::ByteLength() const {
  AssertIsOnOwningThread();

  return static_cast<uint32_t>(mByteLength);
}

// https://w3c.github.io/webcodecs/#dom-encodedvideochunk-copyto
void EncodedVideoChunk::CopyTo(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  auto r = GetSharedArrayBufferData(aDestination);
  if (r.isErr()) {
    aRv.Throw(r.unwrapErr());
    return;
  }
  Span<uint8_t> buf = r.unwrap();

  if (mByteLength > buf.size_bytes()) {
    aRv.ThrowTypeError(
        "Destination ArrayBuffer smaller than source EncodedVideoChunk");
    return;
  }

  PodCopy(buf.data(), mBuffer.get(), mByteLength);
}

}  // namespace mozilla::dom
