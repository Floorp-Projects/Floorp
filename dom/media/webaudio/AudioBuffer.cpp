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

AudioBuffer::AudioBuffer(AudioContext* aContext, uint32_t aNumberOfChannels,
                         uint32_t aLength, float aSampleRate,
                         already_AddRefed<ThreadSharedFloatArrayBufferList>
                           aInitialContents)
  : mOwnerWindow(do_GetWeakReference(aContext->GetOwner())),
    mSharedChannels(aInitialContents),
    mLength(aLength),
    mSampleRate(aSampleRate)
{
  MOZ_ASSERT(!mSharedChannels ||
             mSharedChannels->GetChannels() == aNumberOfChannels);
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
                         AudioContext& aAudioContext,
                         const AudioBufferOptions& aOptions,
                         ErrorResult& aRv)
{
  if (!aOptions.mNumberOfChannels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  float sampleRate = aOptions.mSampleRate.WasPassed()
                       ? aOptions.mSampleRate.Value()
                       : aAudioContext.SampleRate();
  return Create(&aAudioContext, aOptions.mNumberOfChannels, aOptions.mLength,
                sampleRate, aRv);
}

void
AudioBuffer::ClearJSChannels()
{
  mJSChannels.Clear();
}

/* static */ already_AddRefed<AudioBuffer>
AudioBuffer::Create(AudioContext* aContext, uint32_t aNumberOfChannels,
                    uint32_t aLength, float aSampleRate,
                    already_AddRefed<ThreadSharedFloatArrayBufferList>
                      aInitialContents,
                    ErrorResult& aRv)
{
  // Note that a buffer with zero channels is permitted here for the sake of
  // AudioProcessingEvent, where channel counts must match parameters passed
  // to createScriptProcessor(), one of which may be zero.
  if (aSampleRate < WebAudioUtils::MinSampleRate ||
      aSampleRate > WebAudioUtils::MaxSampleRate ||
      aNumberOfChannels > WebAudioUtils::MaxChannelCount ||
      !aLength || aLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<AudioBuffer> buffer =
    new AudioBuffer(aContext, aNumberOfChannels, aLength, aSampleRate,
                    Move(aInitialContents));

  return buffer.forget();
}

JSObject*
AudioBuffer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioBufferBinding::Wrap(aCx, this, aGivenProto);
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
                                JS_NewFloat32Array(aJSContext, mLength));
    if (!array) {
      return false;
    }
    if (mSharedChannels) {
      // "4. Attach ArrayBuffers containing copies of the data to the
      // AudioBuffer, to be returned by the next call to getChannelData."
      const float* data = mSharedChannels->GetData(i);
      JS::AutoCheckCannotGC nogc;
      bool isShared;
      mozilla::PodCopy(JS_GetFloat32ArrayData(array, &isShared, nogc), data, mLength);
      MOZ_ASSERT(!isShared); // Was created as unshared above
    }
    mJSChannels[i] = array;
  }

  mSharedChannels = nullptr;

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
      !end.isValid() || end.value() > mLength) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  JS::AutoCheckCannotGC nogc;
  JSObject* channelArray = mJSChannels[aChannelNumber];
  const float* sourceData = nullptr;
  if (channelArray) {
    if (JS_GetTypedArrayLength(channelArray) != mLength) {
      // The array's buffer was detached.
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    bool isShared = false;
    sourceData = JS_GetFloat32ArrayData(channelArray, &isShared, nogc);
    // The sourceData arrays should all have originated in
    // RestoreJSChannelData, where they are created unshared.
    MOZ_ASSERT(!isShared);
  } else if (mSharedChannels) {
    sourceData = mSharedChannels->GetData(aChannelNumber);
  }

  if (sourceData) {
    PodMove(aDestination.Data(), sourceData + aStartInChannel, length);
  } else {
    PodZero(aDestination.Data(), length);
  }
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
      !end.isValid() || end.value() > mLength) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (!RestoreJSChannelData(aJSContext)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::AutoCheckCannotGC nogc;
  JSObject* channelArray = mJSChannels[aChannelNumber];
  if (JS_GetTypedArrayLength(channelArray) != mLength) {
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
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
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
    if (!channelArray || mLength != JS_GetTypedArrayLength(channelArray)) {
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
    auto stolenData = arrayBuffer
      ? static_cast<float*>(JS_StealArrayBufferContents(aJSContext,
                                                        arrayBuffer))
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

ThreadSharedFloatArrayBufferList*
AudioBuffer::GetThreadSharedChannelsForRate(JSContext* aJSContext)
{
  if (!mSharedChannels) {
    mSharedChannels = StealJSArrayDataIntoSharedChannels(aJSContext);
  }

  return mSharedChannels;
}

size_t
AudioBuffer::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = aMallocSizeOf(this);
  amount += mJSChannels.ShallowSizeOfExcludingThis(aMallocSizeOf);
  if (mSharedChannels) {
    amount += mSharedChannels->SizeOfIncludingThis(aMallocSizeOf);
  }
  return amount;
}

} // namespace dom
} // namespace mozilla
