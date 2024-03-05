/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EncodedAudioChunk_h
#define mozilla_dom_EncodedAudioChunk_h

#include "js/TypeDecls.h"
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

enum class EncodedAudioChunkType : uint8_t;
struct EncodedAudioChunkInit;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class EncodedAudioChunkData {
 public:
  EncodedAudioChunkData(already_AddRefed<MediaAlignedByteBuffer> aBuffer,
                        const EncodedAudioChunkType& aType, int64_t aTimestamp,
                        Maybe<uint64_t>&& aDuration);
  EncodedAudioChunkData(const EncodedAudioChunkData& aData) = default;
  ~EncodedAudioChunkData() = default;

  UniquePtr<EncodedAudioChunkData> Clone() const;
  already_AddRefed<MediaRawData> TakeData();

 protected:
  // mBuffer's byte length is guaranteed to be smaller than UINT32_MAX.
  RefPtr<MediaAlignedByteBuffer> mBuffer;
  EncodedAudioChunkType mType;
  int64_t mTimestamp;
  Maybe<uint64_t> mDuration;
};

class EncodedAudioChunk final : public EncodedAudioChunkData,
                                public nsISupports,
                                public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(EncodedAudioChunk)

 public:
  EncodedAudioChunk(nsIGlobalObject* aParent,
                    already_AddRefed<MediaAlignedByteBuffer> aBuffer,
                    const EncodedAudioChunkType& aType, int64_t aTimestamp,
                    Maybe<uint64_t>&& aDuration);

  EncodedAudioChunk(nsIGlobalObject* aParent,
                    const EncodedAudioChunkData& aData);

 protected:
  ~EncodedAudioChunk() = default;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<EncodedAudioChunk> Constructor(
      const GlobalObject& aGlobal, const EncodedAudioChunkInit& aInit,
      ErrorResult& aRv);

  EncodedAudioChunkType Type() const;

  int64_t Timestamp() const;

  Nullable<uint64_t> GetDuration() const;

  uint32_t ByteLength() const;

  void CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
      ErrorResult& aRv);

  // [Serializable] implementations: {Read, Write}StructuredClone
  static JSObject* ReadStructuredClone(JSContext* aCx, nsIGlobalObject* aGlobal,
                                       JSStructuredCloneReader* aReader,
                                       const EncodedAudioChunkData& aData);

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder) const;

 private:
  // EncodedAudioChunk can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(EncodedAudioChunk);
  }

  nsCOMPtr<nsIGlobalObject> mParent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EncodedAudioChunk_h
