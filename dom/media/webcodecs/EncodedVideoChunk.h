/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EncodedVideoChunk_h
#define mozilla_dom_EncodedVideoChunk_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/Buffer.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class MediaAlignedByteBuffer;
class MediaRawData;

namespace dom {

class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class StructuredCloneHolder;

enum class EncodedVideoChunkType : uint8_t;
struct EncodedVideoChunkInit;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class EncodedVideoChunkData {
 public:
  EncodedVideoChunkData(already_AddRefed<MediaAlignedByteBuffer> aBuffer,
                        const EncodedVideoChunkType& aType, int64_t aTimestamp,
                        Maybe<uint64_t>&& aDuration);
  EncodedVideoChunkData(const EncodedVideoChunkData& aData) = default;
  ~EncodedVideoChunkData();

  UniquePtr<EncodedVideoChunkData> Clone() const;
  already_AddRefed<MediaRawData> TakeData();

 protected:
  // mBuffer's byte length is guaranteed to be smaller than UINT32_MAX.
  RefPtr<MediaAlignedByteBuffer> mBuffer;
  EncodedVideoChunkType mType;
  int64_t mTimestamp;
  Maybe<uint64_t> mDuration;
};

class EncodedVideoChunk final : public EncodedVideoChunkData,
                                public nsISupports,
                                public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(EncodedVideoChunk)

 public:
  EncodedVideoChunk(nsIGlobalObject* aParent,
                    already_AddRefed<MediaAlignedByteBuffer> aBuffer,
                    const EncodedVideoChunkType& aType, int64_t aTimestamp,
                    Maybe<uint64_t>&& aDuration);

  EncodedVideoChunk(nsIGlobalObject* aParent,
                    const EncodedVideoChunkData& aData);

 protected:
  ~EncodedVideoChunk() = default;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<EncodedVideoChunk> Constructor(
      const GlobalObject& aGlobal, const EncodedVideoChunkInit& aInit,
      ErrorResult& aRv);

  EncodedVideoChunkType Type() const;

  int64_t Timestamp() const;

  Nullable<uint64_t> GetDuration() const;

  uint32_t ByteLength() const;

  void CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
      ErrorResult& aRv);

  // [Serializable] implementations: {Read, Write}StructuredClone
  static JSObject* ReadStructuredClone(JSContext* aCx, nsIGlobalObject* aGlobal,
                                       JSStructuredCloneReader* aReader,
                                       const EncodedVideoChunkData& aData);

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder) const;

 private:
  // EncodedVideoChunk can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(EncodedVideoChunk);
  }

  nsCOMPtr<nsIGlobalObject> mParent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EncodedVideoChunk_h
