/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBuffer.h"
#include "mozilla/dom/AudioBufferBinding.h"
#include "jsfriendapi.h"
#include "mozilla/ErrorResult.h"
#include "AudioSegment.h"
#include "AudioChannelFormat.h"
#include "mozilla/PodOperations.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MemoryReporting.h"
#include "AudioNodeEngine.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioBuffer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mJSChannels)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->ClearJSChannels();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioBuffer)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  for (uint32_t i = 0; i < tmp->mJSChannels.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJSChannels[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioBuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioBuffer, Release)

/**
 * AudioBuffers can be shared between AudioContexts, so we need a separate
 * mechanism to track their memory usage. This thread-safe class keeps track of
 * all the AudioBuffers, and gets called back by the memory reporting system
 * when a memory report is needed, reporting how much memory is used by the
 * buffers backing AudioBuffer objects. */
class AudioBufferMemoryTracker : public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

private:
  AudioBufferMemoryTracker();
  virtual ~AudioBufferMemoryTracker();

public:
  /* Those methods can be called on any thread. */
  static void RegisterAudioBuffer(const AudioBuffer* aAudioBuffer);
  static void UnregisterAudioBuffer(const AudioBuffer* aAudioBuffer);
private:
  static AudioBufferMemoryTracker* GetInstance();
  /* Those methods must be called with the lock held. */
  void RegisterAudioBufferInternal(const AudioBuffer* aAudioBuffer);
  /* Returns the number of buffers still present in the hash table. */
  uint32_t UnregisterAudioBufferInternal(const AudioBuffer* aAudioBuffer);
  void Init();

  /* This protects all members of this class. */
  static StaticMutex sMutex;
  static StaticRefPtr<AudioBufferMemoryTracker> sSingleton;
  nsTHashtable<nsPtrHashKey<const AudioBuffer>> mBuffers;
};

StaticRefPtr<AudioBufferMemoryTracker> AudioBufferMemoryTracker::sSingleton;
StaticMutex AudioBufferMemoryTracker::sMutex;

NS_IMPL_ISUPPORTS(AudioBufferMemoryTracker, nsIMemoryReporter);

AudioBufferMemoryTracker* AudioBufferMemoryTracker::GetInstance()
{
  sMutex.AssertCurrentThreadOwns();
  if (!sSingleton) {
    sSingleton = new AudioBufferMemoryTracker();
    sSingleton->Init();
  }
  return sSingleton;
}

AudioBufferMemoryTracker::AudioBufferMemoryTracker()
{
}

void
AudioBufferMemoryTracker::Init()
{
  RegisterWeakMemoryReporter(this);
}

AudioBufferMemoryTracker::~AudioBufferMemoryTracker()
{
  UnregisterWeakMemoryReporter(this);
}

void
AudioBufferMemoryTracker::RegisterAudioBuffer(const AudioBuffer* aAudioBuffer)
{
  StaticMutexAutoLock lock(sMutex);
  AudioBufferMemoryTracker* tracker = AudioBufferMemoryTracker::GetInstance();
  tracker->RegisterAudioBufferInternal(aAudioBuffer);
}

void
AudioBufferMemoryTracker::UnregisterAudioBuffer(const AudioBuffer* aAudioBuffer)
{
  StaticMutexAutoLock lock(sMutex);
  AudioBufferMemoryTracker* tracker = AudioBufferMemoryTracker::GetInstance();
  uint32_t count;
  count = tracker->UnregisterAudioBufferInternal(aAudioBuffer);
  if (count == 0) {
    sSingleton = nullptr;
  }
}

void
AudioBufferMemoryTracker::RegisterAudioBufferInternal(const AudioBuffer* aAudioBuffer)
{
  sMutex.AssertCurrentThreadOwns();
  mBuffers.PutEntry(aAudioBuffer);
}

uint32_t
AudioBufferMemoryTracker::UnregisterAudioBufferInternal(const AudioBuffer* aAudioBuffer)
{
  sMutex.AssertCurrentThreadOwns();
  mBuffers.RemoveEntry(aAudioBuffer);
  return mBuffers.Count();
}

MOZ_DEFINE_MALLOC_SIZE_OF(AudioBufferMemoryTrackerMallocSizeOf)

NS_IMETHODIMP
AudioBufferMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                         nsISupports* aData, bool)
{
  size_t amount = 0;

  for (auto iter = mBuffers.Iter(); !iter.Done(); iter.Next()) {
    amount += iter.Get()->GetKey()->SizeOfIncludingThis(AudioBufferMemoryTrackerMallocSizeOf);
  }

  MOZ_COLLECT_REPORT(
    "explicit/webaudio/audiobuffer", KIND_HEAP, UNITS_BYTES, amount,
    "Memory used by AudioBuffer objects (Web Audio).");

  return NS_OK;
}

AudioBuffer::AudioBuffer(nsPIDOMWindowInner* aWindow,
                         uint32_t aNumberOfChannels,
                         uint32_t aLength,
                         float aSampleRate,
                         ErrorResult& aRv)
  : mOwnerWindow(do_GetWeakReference(aWindow)),
    mSampleRate(aSampleRate)
{
  // Note that a buffer with zero channels is permitted here for the sake of
  // AudioProcessingEvent, where channel counts must match parameters passed
  // to createScriptProcessor(), one of which may be zero.
  if (aSampleRate < WebAudioUtils::MinSampleRate ||
      aSampleRate > WebAudioUtils::MaxSampleRate ||
      aNumberOfChannels > WebAudioUtils::MaxChannelCount ||
      !aLength || aLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  mSharedChannels.mDuration = aLength;
  mJSChannels.SetLength(aNumberOfChannels);
  mozilla::HoldJSObjects(this);
  AudioBufferMemoryTracker::RegisterAudioBuffer(this);
}

AudioBuffer::~AudioBuffer()
{
  AudioBufferMemoryTracker::UnregisterAudioBuffer(this);
  ClearJSChannels();
  mozilla::DropJSObjects(this);
}

/* static */ already_AddRefed<AudioBuffer>
AudioBuffer::Constructor(const GlobalObject& aGlobal,
                         const AudioBufferOptions& aOptions,
                         ErrorResult& aRv)
{
  if (!aOptions.mNumberOfChannels) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window =
    do_QueryInterface(aGlobal.GetAsSupports());

  return Create(window, aOptions.mNumberOfChannels, aOptions.mLength,
                aOptions.mSampleRate, aRv);
}

void
AudioBuffer::ClearJSChannels()
{
  mJSChannels.Clear();
}

void
AudioBuffer::SetSharedChannels(
  already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
{
  RefPtr<ThreadSharedFloatArrayBufferList> buffer = aBuffer;
  uint32_t channelCount = buffer->GetChannels();
  mSharedChannels.mChannelData.SetLength(channelCount);
  for (uint32_t i = 0; i < channelCount; ++i) {
    mSharedChannels.mChannelData[i] = buffer->GetData(i);
  }
  mSharedChannels.mBuffer = buffer.forget();
  mSharedChannels.mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

/* static */ already_AddRefed<AudioBuffer>
AudioBuffer::Create(nsPIDOMWindowInner* aWindow, uint32_t aNumberOfChannels,
                    uint32_t aLength, float aSampleRate,
                    already_AddRefed<ThreadSharedFloatArrayBufferList>
                      aInitialContents,
                    ErrorResult& aRv)
{
  RefPtr<ThreadSharedFloatArrayBufferList> initialContents = aInitialContents;
  RefPtr<AudioBuffer> buffer =
    new AudioBuffer(aWindow, aNumberOfChannels, aLength, aSampleRate, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (initialContents) {
    MOZ_ASSERT(initialContents->GetChannels() == aNumberOfChannels);
    buffer->SetSharedChannels(initialContents.forget());
  }

  return buffer.forget();
}

/* static */ already_AddRefed<AudioBuffer>
AudioBuffer::Create(nsPIDOMWindowInner* aWindow, float aSampleRate,
                    AudioChunk&& aInitialContents)
{
  AudioChunk initialContents = aInitialContents;
  ErrorResult rv;
  RefPtr<AudioBuffer> buffer =
    new AudioBuffer(aWindow, initialContents.ChannelCount(),
                    initialContents.mDuration, aSampleRate, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  buffer->mSharedChannels = std::move(aInitialContents);

  return buffer.forget();
}

JSObject*
AudioBuffer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioBufferBinding::Wrap(aCx, this, aGivenProto);
}

static void
CopyChannelDataToFloat(const AudioChunk& aChunk, uint32_t aChannel,
                       uint32_t aSrcOffset, float* aOutput, uint32_t aLength)
{
  MOZ_ASSERT(aChunk.mVolume == 1.0f);
  if (aChunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
    mozilla::PodCopy(aOutput,
                     aChunk.ChannelData<float>()[aChannel] + aSrcOffset,
                     aLength);
  } else {
    MOZ_ASSERT(aChunk.mBufferFormat == AUDIO_FORMAT_S16);
    ConvertAudioSamples(aChunk.ChannelData<int16_t>()[aChannel] + aSrcOffset,
                        aOutput, aLength);
  }
}

bool
AudioBuffer::RestoreJSChannelData(JSContext* aJSContext)
{
  for (uint32_t i = 0; i < mJSChannels.Length(); ++i) {
    if (mJSChannels[i]) {
      // Already have data in JS array.
      continue;
    }

    // The following code first zeroes the array and then copies our data
    // into it. We could avoid this with additional JS APIs to construct
    // an array (or ArrayBuffer) containing initial data.
    JS::Rooted<JSObject*> array(aJSContext,
                                JS_NewFloat32Array(aJSContext, Length()));
    if (!array) {
      return false;
    }
    if (!mSharedChannels.IsNull()) {
      // "4. Attach ArrayBuffers containing copies of the data to the
      // AudioBuffer, to be returned by the next call to getChannelData."
      JS::AutoCheckCannotGC nogc;
      bool isShared;
      float* jsData = JS_GetFloat32ArrayData(array, &isShared, nogc);
      MOZ_ASSERT(!isShared); // Was created as unshared above
      CopyChannelDataToFloat(mSharedChannels, i, 0, jsData, Length());
    }
    mJSChannels[i] = array;
  }

  mSharedChannels.SetNull(Length());

  return true;
}

void
AudioBuffer::CopyFromChannel(const Float32Array& aDestination, uint32_t aChannelNumber,
                             uint32_t aStartInChannel, ErrorResult& aRv)
{
  aDestination.ComputeLengthAndData();

  uint32_t length = aDestination.Length();
  CheckedInt<uint32_t> end = aStartInChannel;
  end += length;
  if (aChannelNumber >= NumberOfChannels() ||
      !end.isValid() || end.value() > Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  JS::AutoCheckCannotGC nogc;
  JSObject* channelArray = mJSChannels[aChannelNumber];
  if (channelArray) {
    if (JS_GetTypedArrayLength(channelArray) != Length()) {
      // The array's buffer was detached.
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    bool isShared = false;
    const float* sourceData =
      JS_GetFloat32ArrayData(channelArray, &isShared, nogc);
    // The sourceData arrays should all have originated in
    // RestoreJSChannelData, where they are created unshared.
    MOZ_ASSERT(!isShared);
    PodMove(aDestination.Data(), sourceData + aStartInChannel, length);
    return;
  }

  if (!mSharedChannels.IsNull()) {
    CopyChannelDataToFloat(mSharedChannels, aChannelNumber, aStartInChannel,
                           aDestination.Data(), length);
    return;
  }

  PodZero(aDestination.Data(), length);
}

void
AudioBuffer::CopyToChannel(JSContext* aJSContext, const Float32Array& aSource,
                           uint32_t aChannelNumber, uint32_t aStartInChannel,
                           ErrorResult& aRv)
{
  aSource.ComputeLengthAndData();

  uint32_t length = aSource.Length();
  CheckedInt<uint32_t> end = aStartInChannel;
  end += length;
  if (aChannelNumber >= NumberOfChannels() ||
      !end.isValid() || end.value() > Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (!RestoreJSChannelData(aJSContext)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::AutoCheckCannotGC nogc;
  JSObject* channelArray = mJSChannels[aChannelNumber];
  if (JS_GetTypedArrayLength(channelArray) != Length()) {
    // The array's buffer was detached.
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  bool isShared = false;
  float* channelData = JS_GetFloat32ArrayData(channelArray, &isShared, nogc);
  // The channelData arrays should all have originated in
  // RestoreJSChannelData, where they are created unshared.
  MOZ_ASSERT(!isShared);
  PodMove(channelData + aStartInChannel, aSource.Data(), length);
}

void
AudioBuffer::GetChannelData(JSContext* aJSContext, uint32_t aChannel,
                            JS::MutableHandle<JSObject*> aRetval,
                            ErrorResult& aRv)
{
  if (aChannel >= NumberOfChannels()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (!RestoreJSChannelData(aJSContext)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aRetval.set(mJSChannels[aChannel]);
}

already_AddRefed<ThreadSharedFloatArrayBufferList>
AudioBuffer::StealJSArrayDataIntoSharedChannels(JSContext* aJSContext)
{
  // "1. If any of the AudioBuffer's ArrayBuffer have been detached, abort
  // these steps, and return a zero-length channel data buffers to the
  // invoker."
  for (uint32_t i = 0; i < mJSChannels.Length(); ++i) {
    JSObject* channelArray = mJSChannels[i];
    if (!channelArray || Length() != JS_GetTypedArrayLength(channelArray)) {
      // Either empty buffer or one of the arrays' buffers was detached.
      return nullptr;
    }
  }

  // "2. Detach all ArrayBuffers for arrays previously returned by
  // getChannelData on this AudioBuffer."
  // "3. Retain the underlying data buffers from those ArrayBuffers and return
  // references to them to the invoker."
  RefPtr<ThreadSharedFloatArrayBufferList> result =
    new ThreadSharedFloatArrayBufferList(mJSChannels.Length());
  for (uint32_t i = 0; i < mJSChannels.Length(); ++i) {
    JS::Rooted<JSObject*> arrayBufferView(aJSContext, mJSChannels[i]);
    bool isSharedMemory;
    JS::Rooted<JSObject*> arrayBuffer(aJSContext,
                                      JS_GetArrayBufferViewBuffer(aJSContext,
                                                                  arrayBufferView,
                                                                  &isSharedMemory));
    // The channel data arrays should all have originated in
    // RestoreJSChannelData, where they are created unshared.
    MOZ_ASSERT(!isSharedMemory);
    auto stolenData =
      arrayBuffer ? static_cast<float*>(
                      JS_StealArrayBufferContents(aJSContext, arrayBuffer))
                  : nullptr;
    if (stolenData) {
      result->SetData(i, stolenData, js_free, stolenData);
    } else {
      NS_ASSERTION(i == 0, "some channels lost when contents not acquired");
      return nullptr;
    }
  }

  for (uint32_t i = 0; i < mJSChannels.Length(); ++i) {
    mJSChannels[i] = nullptr;
  }

  return result.forget();
}

const AudioChunk&
AudioBuffer::GetThreadSharedChannelsForRate(JSContext* aJSContext)
{
  if (mSharedChannels.IsNull()) {
    // mDuration is set in constructor
    RefPtr<ThreadSharedFloatArrayBufferList> buffer =
      StealJSArrayDataIntoSharedChannels(aJSContext);

    if (buffer) {
      SetSharedChannels(buffer.forget());
    }
  }

  return mSharedChannels;
}

size_t
AudioBuffer::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = aMallocSizeOf(this);
  amount += mJSChannels.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mSharedChannels.SizeOfExcludingThis(aMallocSizeOf, false);
  return amount;
}

} // namespace dom
} // namespace mozilla
