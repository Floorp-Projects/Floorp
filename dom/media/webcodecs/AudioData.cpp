/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/AudioData.h"
#include "mozilla/dom/AudioDataBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "nsStringFwd.h"

#include <utility>

#include "AudioSampleFormat.h"
#include "WebCodecsUtils.h"
#include "js/StructuredClone.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla::dom {

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gWebCodecsLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOGD
#  undef LOGD
#endif  // LOGD
#define LOGD(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

// Only needed for refcounted objects.
//
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AudioData)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioData)
  tmp->CloseIfNeeded();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioData)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioData)
// AudioData should be released as soon as its refcount drops to zero,
// without waiting for async deletion by the cycle collector, since it may hold
// a large-size PCM buffer.
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(AudioData, CloseIfNeeded())
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/*
 * W3C Webcodecs AudioData implementation
 */

AudioData::AudioData(nsIGlobalObject* aParent,
                     const AudioDataSerializedData& aData)
    : mParent(aParent),
      mTimestamp(aData.mTimestamp),
      mNumberOfChannels(aData.mNumberOfChannels),
      mNumberOfFrames(aData.mNumberOfFrames),
      mSampleRate(aData.mSampleRate),
      mAudioSampleFormat(aData.mAudioSampleFormat),
      // The resource is not copied, but referenced
      mResource(aData.mResource) {
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(mResource,
             "Resource should always be present then receiving a transfer.");
}

AudioData::AudioData(const AudioData& aOther)
    : mParent(aOther.mParent),
      mTimestamp(aOther.mTimestamp),
      mNumberOfChannels(aOther.mNumberOfChannels),
      mNumberOfFrames(aOther.mNumberOfFrames),
      mSampleRate(aOther.mSampleRate),
      mAudioSampleFormat(aOther.mAudioSampleFormat),
      // The resource is not copied, but referenced
      mResource(aOther.mResource) {
  MOZ_ASSERT(mParent);
}

Result<already_AddRefed<AudioDataResource>, nsresult>
AudioDataResource::Construct(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aInit) {
  FallibleTArray<uint8_t> copied;
  uint8_t* rv = ProcessTypedArraysFixed(
      aInit, [&](const Span<uint8_t>& aData) -> uint8_t* {
        return copied.AppendElements(aData.Elements(), aData.Length(),
                                     fallible);
      });
  if (!rv) {
    LOGE("AudioDataResource::Ctor: OOM");
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }
  return MakeAndAddRef<AudioDataResource>(std::move(copied));
}

AudioData::AudioData(
    nsIGlobalObject* aParent,
    already_AddRefed<mozilla::dom::AudioDataResource> aResource,
    const AudioDataInit& aInit)
    : mParent(aParent),
      mTimestamp(aInit.mTimestamp),
      mNumberOfChannels(aInit.mNumberOfChannels),
      mNumberOfFrames(aInit.mNumberOfFrames),
      mSampleRate(aInit.mSampleRate),
      mAudioSampleFormat(Some(aInit.mFormat)),
      mResource(std::move(aResource)) {
  MOZ_ASSERT(mParent);
}

nsIGlobalObject* AudioData::GetParentObject() const {
  AssertIsOnOwningThread();

  return mParent.get();
}

JSObject* AudioData::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  return AudioData_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t BytesPerSamples(const mozilla::dom::AudioSampleFormat& aFormat) {
  switch (aFormat) {
    case AudioSampleFormat::U8:
    case AudioSampleFormat::U8_planar:
      return sizeof(uint8_t);
    case AudioSampleFormat::S16:
    case AudioSampleFormat::S16_planar:
      return sizeof(int16_t);
    case AudioSampleFormat::S32:
    case AudioSampleFormat::F32:
    case AudioSampleFormat::S32_planar:
    case AudioSampleFormat::F32_planar:
      return sizeof(float);
  }
}

Result<Ok, nsCString> IsValidAudioDataInit(const AudioDataInit& aInit) {
  return Ok();
}

const char* FormatToString(AudioSampleFormat aFormat) {
  switch (aFormat) {
    case AudioSampleFormat::U8:
      return "u8";
    case AudioSampleFormat::S16:
      return "s16";
    case AudioSampleFormat::S32:
      return "s32";
    case AudioSampleFormat::F32:
      return "f32";
    case AudioSampleFormat::U8_planar:
      return "u8-planar";
    case AudioSampleFormat::S16_planar:
      return "s16-planar";
    case AudioSampleFormat::S32_planar:
      return "s32-planar";
    case AudioSampleFormat::F32_planar:
      return "f32-planar";
  }
}

/* static */
already_AddRefed<AudioData> AudioData::Constructor(const GlobalObject& aGlobal,
                                                   const AudioDataInit& aInit,
                                                   ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  LOGD("[%p] AudioData(fmt: %s, rate: %f, ch: %" PRIu32 ", ts: %" PRId64 ")",
       global.get(), FormatToString(aInit.mFormat), aInit.mSampleRate,
       aInit.mNumberOfChannels, aInit.mTimestamp);
  if (!global) {
    LOGE("Global unavailable");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeAndAddRef<mozilla::dom::AudioData>(global, resource.unwrap(),
                                                aInit);
}

// https://w3c.github.io/webcodecs/#dom-audiodata-format
Nullable<mozilla::dom::AudioSampleFormat> AudioData::GetFormat() const {
  AssertIsOnOwningThread();
  return MaybeToNullable(mAudioSampleFormat);
}

// https://w3c.github.io/webcodecs/#dom-audiodata-samplerate
float AudioData::SampleRate() const {
  AssertIsOnOwningThread();
  return mSampleRate;
}

// https://w3c.github.io/webcodecs/#dom-audiodata-numberofframes
uint32_t AudioData::NumberOfFrames() const {
  AssertIsOnOwningThread();
  return mNumberOfFrames;
}

// https://w3c.github.io/webcodecs/#dom-audiodata-numberofchannels
uint32_t AudioData::NumberOfChannels() const {
  AssertIsOnOwningThread();
  return mNumberOfChannels;
}

// https://w3c.github.io/webcodecs/#dom-audiodata-duration
uint64_t AudioData::Duration() const {
  AssertIsOnOwningThread();
  // The spec isn't clear in which direction to convert to integer.
  // https://github.com/w3c/webcodecs/issues/726
  return static_cast<uint64_t>(
      static_cast<float>(USECS_PER_S * mNumberOfFrames) / mSampleRate);
}

// https://w3c.github.io/webcodecs/#dom-audiodata-timestamp
int64_t AudioData::Timestamp() const {
  AssertIsOnOwningThread();
  return mTimestamp;
}
nsCString AudioData::ToString() const {
  if (!mResource) {
    return nsCString("AudioData[detached]");
  }
  return nsPrintfCString("AudioData[%zu bytes %s %fHz %" PRIu32 "x%" PRIu32
                         "ch]",
                         mResource->Data().LengthBytes(),
                         FormatToString(mAudioSampleFormat.value()),
                         mSampleRate, mNumberOfFrames, mNumberOfChannels);
}

// https://w3c.github.io/webcodecs/#dom-audiodata-copyto
void AudioData::CopyTo(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
    const AudioDataCopyToOptions& aOptions, ErrorResult& aRv) {
  AssertIsOnOwningThread();

}

// https://w3c.github.io/webcodecs/#dom-audiodata-clone
already_AddRefed<AudioData> AudioData::Clone(ErrorResult& aRv) {
  AssertIsOnOwningThread();
}

// https://w3c.github.io/webcodecs/#close-audiodata
void AudioData::Close() {
  AssertIsOnOwningThread();
}

// https://w3c.github.io/webcodecs/#ref-for-deserialization-steps%E2%91%A1
/* static */
JSObject* AudioData::ReadStructuredClone(JSContext* aCx,
                                         nsIGlobalObject* aGlobal,
                                         JSStructuredCloneReader* aReader,
                                         const AudioDataSerializedData& aData) {
}

// https://w3c.github.io/webcodecs/#ref-for-audiodata%E2%91%A2%E2%91%A2
bool AudioData::WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                                     StructuredCloneHolder* aHolder) const {
  AssertIsOnOwningThread();
}

// https://w3c.github.io/webcodecs/#ref-for-transfer-steps
UniquePtr<AudioData::TransferredData> AudioData::Transfer() {
  AssertIsOnOwningThread();
}

// https://w3c.github.io/webcodecs/#ref-for-transfer-receiving-steps
/* static */
already_AddRefed<AudioData> AudioData::FromTransferred(nsIGlobalObject* aGlobal,
                                                       TransferredData* aData) {
}

void AudioData::CloseIfNeeded() {
  if (mResource) {
    mResource = nullptr;
  }
}

#undef LOGD
#undef LOGE
#undef LOG_INTERNAL

}  // namespace mozilla::dom
