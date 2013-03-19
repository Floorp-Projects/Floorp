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
#include "EnableWebAudioCheck.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "AudioContext.h"
#include "AudioSegment.h"
#include "AudioNodeEngine.h"

struct JSContext;
class JSObject;

namespace mozilla {

class ErrorResult;

namespace dom {

/**
 * An AudioBuffer keeps its data either in the mJSChannels objects, which
 * are Float32Arrays, or in mSharedChannels if the mJSChannels objects have
 * been neutered.
 */
class AudioBuffer MOZ_FINAL : public nsISupports,
                              public nsWrapperCache,
                              public EnableWebAudioCheck
{
public:
  AudioBuffer(AudioContext* aContext, uint32_t aLength,
              float aSampleRate);
  ~AudioBuffer();

  // This function needs to be called in order to allocate
  // all of the channels.  It is fallible!
  bool InitializeBuffers(uint32_t aNumberOfChannels,
                         JSContext* aJSContext);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AudioBuffer)

  AudioContext* GetParentObject() const
  {
    return mContext;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

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

  JSObject* GetChannelData(uint32_t aChannel) const {
    // Doesn't perform bounds checking
    MOZ_ASSERT(aChannel < mJSChannels.Length());
    return mJSChannels[aChannel];
  }

  /**
   * Returns a ThreadSharedFloatArrayBufferList containing the sample data
   * at aRate. Sets *aLength to the number of samples per channel.
   */
  ThreadSharedFloatArrayBufferList* GetThreadSharedChannelsForRate(JSContext* aContext,
                                                                   uint32_t* aRate,
                                                                   uint32_t* aLength);

  // aContents should either come from JS_AllocateArrayBufferContents or
  // JS_StealArrayBufferContents.
  void SetChannelDataFromArrayBufferContents(JSContext* aJSContext,
                                             uint32_t aChannel,
                                             void* aContents);

protected:
  void RestoreJSChannelData(JSContext* aJSContext);
  void ClearJSChannels();

  nsRefPtr<AudioContext> mContext;
  // Float32Arrays
  AutoFallibleTArray<JSObject*,2> mJSChannels;

  // mSharedChannels aggregates the data from mJSChannels. This is non-null
  // if and only if the mJSChannels are neutered.
  nsRefPtr<ThreadSharedFloatArrayBufferList> mSharedChannels;

  uint32_t mLength;
  float mSampleRate;
};

}
}

#endif

