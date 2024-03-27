/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioData_h
#define mozilla_dom_AudioData_h

#include "MediaData.h"
#include "WebCodecsUtils.h"
#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Span.h"
#include "mozilla/dom/AudioDataBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsIURI;

namespace mozilla::dom {

class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;
struct AudioDataBufferInit;
struct AudioDataCopyToOptions;
struct AudioDataInit;

}  // namespace mozilla::dom

namespace mozilla::dom {

class AudioData;
class AudioDataResource;
struct AudioDataSerializedData;

class AudioData final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AudioData)

 public:
  AudioData(nsIGlobalObject* aParent, const AudioDataSerializedData& aData);
  AudioData(nsIGlobalObject* aParent,
            already_AddRefed<AudioDataResource> aResource,
            const AudioDataInit& aInit);
  AudioData(nsIGlobalObject* aParent,
            already_AddRefed<mozilla::dom::AudioDataResource> aResource,
            int64_t aTimestamp, uint32_t aNumberOfChannels,
            uint32_t aNumberOfFrames, float aSampleRate,
            AudioSampleFormat aAudioSampleFormat);
  AudioData(const AudioData& aOther);

 protected:
  ~AudioData() = default;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<AudioData> Constructor(const GlobalObject& aGlobal,
                                                 const AudioDataInit& aInit,
                                                 ErrorResult& aRv);

  Nullable<mozilla::dom::AudioSampleFormat> GetFormat() const;

  float SampleRate() const;

  uint32_t NumberOfFrames() const;

  uint32_t NumberOfChannels() const;

  uint64_t Duration() const;  // microseconds

  int64_t Timestamp() const;  // microseconds

  uint32_t AllocationSize(const AudioDataCopyToOptions& aOptions,
                          ErrorResult& aRv);

  void CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
      const AudioDataCopyToOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<AudioData> Clone(ErrorResult& aRv);

  void Close();
  bool IsClosed() const;

  // [Serializable] implementations: {Read, Write}StructuredClone
  static JSObject* ReadStructuredClone(JSContext* aCx, nsIGlobalObject* aGlobal,
                                       JSStructuredCloneReader* aReader,
                                       const AudioDataSerializedData& aData);

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder) const;

  // [Transferable] implementations: Transfer, FromTransferred
  using TransferredData = AudioDataSerializedData;

  UniquePtr<TransferredData> Transfer();

  static already_AddRefed<AudioData> FromTransferred(nsIGlobalObject* aGlobal,
                                                     TransferredData* aData);

  nsCString ToString() const;

  RefPtr<mozilla::AudioData> ToAudioData() const;

 private:
  size_t ComputeCopyElementCount(const AudioDataCopyToOptions& aOptions,
                                 ErrorResult& aRv);
  // AudioData can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(AudioData); }
  void CloseIfNeeded();

  nsCOMPtr<nsIGlobalObject> mParent;

  friend struct AudioDataSerializedData;

  int64_t mTimestamp;
  uint32_t mNumberOfChannels;
  uint32_t mNumberOfFrames;
  float mSampleRate;
  Maybe<AudioSampleFormat> mAudioSampleFormat;
  RefPtr<mozilla::dom::AudioDataResource> mResource;
};

class AudioDataResource final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioDataResource);
  explicit AudioDataResource(FallibleTArray<uint8_t>&& aData)
      : mData(std::move(aData)) {}

  explicit AudioDataResource() : mData() {}

  static AudioDataResource* Create(const Span<uint8_t>& aData) {
    AudioDataResource* resource = new AudioDataResource();
    if (!resource->mData.AppendElements(aData, mozilla::fallible_t())) {
      return nullptr;
    }
    return resource;
  }

  static Result<already_AddRefed<AudioDataResource>, nsresult> Construct(
      const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aInit);

  Span<uint8_t> Data() { return Span(mData.Elements(), mData.Length()); };

 private:
  ~AudioDataResource() = default;
  // It's always possible for the allocation to fail -- the size is
  // controled by script.
  FallibleTArray<uint8_t> mData;
};

struct AudioDataSerializedData {
  explicit AudioDataSerializedData(const AudioData& aFrom)
      : mTimestamp(aFrom.Timestamp()),
        mNumberOfChannels(aFrom.NumberOfChannels()),
        mNumberOfFrames(aFrom.NumberOfFrames()),
        mSampleRate(aFrom.SampleRate()),
        mAudioSampleFormat(NullableToMaybe(aFrom.GetFormat())),
        mResource(aFrom.mResource) {}
  int64_t mTimestamp{};
  uint32_t mNumberOfChannels{};
  uint32_t mNumberOfFrames{};
  float mSampleRate{};
  Maybe<AudioSampleFormat> mAudioSampleFormat;
  RefPtr<AudioDataResource> mResource;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioData_h
