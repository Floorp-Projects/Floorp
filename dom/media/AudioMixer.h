/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOMIXER_H_
#define MOZILLA_AUDIOMIXER_H_

#include "AudioSampleFormat.h"
#include "AudioSegment.h"
#include "AudioStream.h"
#include "nsTArray.h"
#include "mozilla/NotNull.h"
#include "mozilla/PodOperations.h"

namespace mozilla {

struct MixerCallbackReceiver {
  // MixerCallback MAY modify aMixedBuffer but MUST clear
  // aMixedBuffer->mBuffer if its data is to live longer than the duration of
  // the callback.
  virtual void MixerCallback(AudioChunk* aMixedBuffer,
                             uint32_t aSampleRate) = 0;
};
/**
 * This class mixes multiple streams of audio together to output a single audio
 * stream.
 *
 * AudioMixer::Mix is to be called repeatedly with buffers that have the same
 * length, sample rate, sample format and channel count. This class works with
 * planar buffers.
 *
 * When all the tracks have been mixed, calling MixedChunk() will provide
 * a buffer containing the mixed audio data.
 *
 * This class is not thread safe.
 */
class AudioMixer {
 public:
  AudioMixer() { mChunk.mBufferFormat = AUDIO_OUTPUT_FORMAT; }

  ~AudioMixer() = default;

  void StartMixing() {
    mChunk.mDuration = 0;
    mSampleRate = 0;
  }

  /* Get the data from the mixer. This is supposed to be called when all the
   * tracks have been mixed in. The caller MAY modify the chunk but MUST clear
   * mBuffer if its data needs to survive the next call to Mix(). */
  AudioChunk* MixedChunk() {
    MOZ_ASSERT(mSampleRate, "Mix not called for this cycle?");
    mSampleRate = 0;
    return &mChunk;
  };

  /* Add a buffer to the mix. The buffer can be null if there's nothing to mix
   * but the callback is still needed. */
  void Mix(AudioDataValue* aSamples, uint32_t aChannels, uint32_t aFrames,
           uint32_t aSampleRate) {
    if (!mChunk.mDuration) {
      mChunk.mDuration = aFrames;
      MOZ_ASSERT(aChannels > 0);
      mChunk.mChannelData.SetLength(aChannels);
      mSampleRate = aSampleRate;
      EnsureCapacityAndSilence();
    }

    MOZ_ASSERT(aFrames == mChunk.mDuration);
    MOZ_ASSERT(aChannels == mChunk.ChannelCount());
    MOZ_ASSERT(aSampleRate == mSampleRate);

    if (!aSamples) {
      return;
    }

    for (uint32_t i = 0; i < aFrames * aChannels; i++) {
      mChunk.ChannelDataForWrite<AudioDataValue>(0)[i] += aSamples[i];
    }
  }

 private:
  void EnsureCapacityAndSilence() {
    uint32_t sampleCount = mChunk.mDuration * mChunk.ChannelCount();
    if (!mChunk.mBuffer || sampleCount > mSampleCapacity) {
      CheckedInt<size_t> bufferSize(sizeof(AudioDataValue));
      bufferSize *= sampleCount;
      mChunk.mBuffer = SharedBuffer::Create(bufferSize);
      mSampleCapacity = sampleCount;
    }
    MOZ_ASSERT(!mChunk.mBuffer->IsShared());
    mChunk.mChannelData[0] =
        static_cast<SharedBuffer*>(mChunk.mBuffer.get())->Data();
    for (size_t i = 1; i < mChunk.ChannelCount(); ++i) {
      mChunk.mChannelData[i] =
          mChunk.ChannelData<AudioDataValue>()[0] + i * mChunk.mDuration;
    }
    PodZero(mChunk.ChannelDataForWrite<AudioDataValue>(0), sampleCount);
  }

  /* Buffer containing the mixed audio data. */
  AudioChunk mChunk;
  /* Size allocated for mChunk.mBuffer. */
  uint32_t mSampleCapacity = 0;
  /* Sample rate the of the mixed data. */
  uint32_t mSampleRate = 0;
};

}  // namespace mozilla

#endif  // MOZILLA_AUDIOMIXER_H_
