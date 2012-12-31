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

struct JSContext;
class JSObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class AudioBuffer MOZ_FINAL : public nsISupports,
                              public nsWrapperCache,
                              public EnableWebAudioCheck
{
public:
  AudioBuffer(AudioContext* aContext, uint32_t aLength,
              uint32_t aSampleRate);
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

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope,
                               bool* aTriedToWrap);

  float SampleRate() const
  {
    return mSampleRate;
  }

  uint32_t Length() const
  {
    return mLength;
  }

  double Duration() const
  {
    return mLength / static_cast<double> (mSampleRate);
  }

  uint32_t NumberOfChannels() const
  {
    return mChannels.Length();
  }

  JSObject* GetChannelData(JSContext* aJSContext, uint32_t aChannel,
                           ErrorResult& aRv) const;

private:
  nsRefPtr<AudioContext> mContext;
  FallibleTArray<JSObject*> mChannels;
  uint32_t mLength;
  float mSampleRate;
};

}
}

#endif

