/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioBuffer_h_
#define AudioBuffer_h_

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "AudioContext.h"
#include "js/TypeDecls.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

class ErrorResult;
class ThreadSharedFloatArrayBufferList;

namespace dom {

class AudioContext;

/**
 * An AudioBuffer keeps its data either in the mJSChannels objects, which
 * are Float32Arrays, or in mSharedChannels if the mJSChannels objects have
 * been neutered.
 */
class AudioBuffer MOZ_FINAL : public nsWrapperCache
{
public:
  AudioBuffer(AudioContext* aContext, uint32_t aLength,
              float aSampleRate);
  ~AudioBuffer();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // This function needs to be called in order to allocate
  // all of the channels.  It is fallible!
  bool InitializeBuffers(uint32_t aNumberOfChannels,
                         JSContext* aJSContext);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AudioBuffer)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioBuffer)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  float SampleRate() const
  {
    return mSampleRate;
  }

  int32_t Length() const
  {
    return mLength;
  }

  double Duration() const
  {
    return mLength / static_cast<double> (mSampleRate);
  }

  uint32_t NumberOfChannels() const
  {
    return mJSChannels.Length();
  }

  /**
   * If mSharedChannels is non-null, copies its contents to
   * new Float32Arrays in mJSChannels. Returns a Float32Array.
   */
  JSObject* GetChannelData(JSContext* aJSContext, uint32_t aChannel,
                           ErrorResult& aRv);

  void CopyFromChannel(const Float32Array& aDestination, uint32_t aChannelNumber,
                       uint32_t aStartInChannel, ErrorResult& aRv);
  void CopyToChannel(JSContext* aJSContext, const Float32Array& aSource,
                     uint32_t aChannelNumber, uint32_t aStartInChannel,
                     ErrorResult& aRv);

  /**
   * Returns a ThreadSharedFloatArrayBufferList containing the sample data.
   * Can return null if there is no data.
   */
  ThreadSharedFloatArrayBufferList* GetThreadSharedChannelsForRate(JSContext* aContext);

  // This replaces the contents of the JS array for the given channel.
  // This function needs to be called on an AudioBuffer which has not been
  // handed off to the content yet, and right after the object has been
  // initialized.
  void SetRawChannelContents(JSContext* aJSContext,
                             uint32_t aChannel,
                             float* aContents);

  void MixToMono(JSContext* aJSContext);

protected:
  bool RestoreJSChannelData(JSContext* aJSContext);
  void ClearJSChannels();

  nsRefPtr<AudioContext> mContext;
  // Float32Arrays
  AutoFallibleTArray<JS::Heap<JSObject*>, 2> mJSChannels;

  // mSharedChannels aggregates the data from mJSChannels. This is non-null
  // if and only if the mJSChannels are neutered.
  nsRefPtr<ThreadSharedFloatArrayBufferList> mSharedChannels;

  uint32_t mLength;
  float mSampleRate;
};

}
}

#endif

