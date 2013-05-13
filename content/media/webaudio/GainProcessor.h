/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GainProcessor_h_
#define GainProcessor_h_

#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"

namespace mozilla {
namespace dom {

// This class implements the gain processing logic used by GainNodeEngine
// and AudioBufferSourceNodeEngine.
class GainProcessor
{
public:
  explicit GainProcessor(AudioDestinationNode* aDestination)
    : mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    , mGain(1.f)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  void SetGainParameter(const AudioParamTimeline& aValue)
  {
    MOZ_ASSERT(mSource && mDestination);
    mGain = aValue;
    WebAudioUtils::ConvertAudioParamToTicks(mGain, mSource, mDestination);
  }

  void ProcessGain(AudioNodeStream* aStream,
                   float aInputVolume,
                   const nsTArray<const void*>& aInputChannelData,
                   AudioChunk* aOutput)
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    if (mGain.HasSimpleValue()) {
      // Optimize the case where we only have a single value set as the volume
      aOutput->mVolume *= mGain.GetValue();
    } else {
      // First, compute a vector of gains for each track tick based on the
      // timeline at hand, and then for each channel, multiply the values
      // in the buffer with the gain vector.

      // Compute the gain values for the duration of the input AudioChunk
      // XXX we need to add a method to AudioEventTimeline to compute this buffer directly.
      float computedGain[WEBAUDIO_BLOCK_SIZE];
      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        TrackTicks tick = aStream->GetCurrentPosition();
        computedGain[counter] = mGain.GetValueAtTime(tick, counter) * aInputVolume;
      }

      // Apply the gain to the output buffer
      MOZ_ASSERT(aInputChannelData.Length() == aOutput->mChannelData.Length());
      for (size_t channel = 0; channel < aOutput->mChannelData.Length(); ++channel) {
        const float* inputBuffer = static_cast<const float*> (aInputChannelData[channel]);
        float* buffer = static_cast<float*> (const_cast<void*>
                          (aOutput->mChannelData[channel]));
        AudioBlockCopyChannelWithScale(inputBuffer, computedGain, buffer);
      }
    }
  }

protected:
  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mGain;
};

}
}

#endif

