/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOMIXER_H_
#define MOZILLA_AUDIOMIXER_H_

#include "AudioSampleFormat.h"
#include "nsTArray.h"
#include "mozilla/PodOperations.h"

namespace mozilla {
typedef void(*MixerFunc)(AudioDataValue* aMixedBuffer,
                         AudioSampleFormat aFormat,
                         uint32_t aChannels,
                         uint32_t aFrames,
                         uint32_t aSampleRate);

/**
 * This class mixes multiple streams of audio together to output a single audio
 * stream.
 *
 * AudioMixer::Mix is to be called repeatedly with buffers that have the same
 * length, sample rate, sample format and channel count.
 *
 * When all the tracks have been mixed, calling FinishMixing will call back with
 * a buffer containing the mixed audio data.
 *
 * This class is not thread safe.
 */
class AudioMixer
{
public:
  AudioMixer(MixerFunc aCallback)
    : mCallback(aCallback),
      mFrames(0),
      mChannels(0),
      mSampleRate(0)
  { }

  /* Get the data from the mixer. This is supposed to be called when all the
   * tracks have been mixed in. The caller should not hold onto the data. */
  void FinishMixing() {
    mCallback(mMixedAudio.Elements(),
              AudioSampleTypeToFormat<AudioDataValue>::Format,
              mChannels,
              mFrames,
              mSampleRate);
    PodZero(mMixedAudio.Elements(), mMixedAudio.Length());
    mSampleRate = mChannels = mFrames = 0;
  }

  /* Add a buffer to the mix. aSamples is interleaved. */
  void Mix(AudioDataValue* aSamples,
           uint32_t aChannels,
           uint32_t aFrames,
           uint32_t aSampleRate) {
    if (!mFrames && !mChannels) {
      mFrames = aFrames;
      mChannels = aChannels;
      mSampleRate = aSampleRate;
      EnsureCapacityAndSilence();
    }

    MOZ_ASSERT(aFrames == mFrames);
    MOZ_ASSERT(aChannels == mChannels);
    MOZ_ASSERT(aSampleRate == mSampleRate);

    for (uint32_t i = 0; i < aFrames * aChannels; i++) {
      mMixedAudio[i] += aSamples[i];
    }
  }
private:
  void EnsureCapacityAndSilence() {
    if (mFrames * mChannels > mMixedAudio.Length()) {
      mMixedAudio.SetLength(mFrames* mChannels);
    }
    PodZero(mMixedAudio.Elements(), mMixedAudio.Length());
  }

  /* Function that is called when the mixing is done. */
  MixerFunc mCallback;
  /* Number of frames for this mixing block. */
  uint32_t mFrames;
  /* Number of channels for this mixing block. */
  uint32_t mChannels;
  /* Sample rate the of the mixed data. */
  uint32_t mSampleRate;
  /* Buffer containing the mixed audio data. */
  nsTArray<AudioDataValue> mMixedAudio;
};
}

#endif // MOZILLA_AUDIOMIXER_H_
