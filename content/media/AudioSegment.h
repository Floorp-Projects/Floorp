/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOSEGMENT_H_
#define MOZILLA_AUDIOSEGMENT_H_

#include "MediaSegment.h"
#include "nsISupportsImpl.h"
#include "AudioSampleFormat.h"
#include "SharedBuffer.h"

namespace mozilla {

class AudioStream;

/**
 * An AudioChunk represents a multi-channel buffer of audio samples.
 * It references an underlying ThreadSharedObject which manages the lifetime
 * of the buffer. An AudioChunk maintains its own duration and channel data
 * pointers so it can represent a subinterval of a buffer without copying.
 * An AudioChunk can store its individual channels anywhere; it maintains
 * separate pointers to each channel's buffer.
 */
struct AudioChunk {
  typedef mozilla::AudioSampleFormat SampleFormat;

  // Generic methods
  void SliceTo(TrackTicks aStart, TrackTicks aEnd)
  {
    NS_ASSERTION(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
                 "Slice out of bounds");
    if (mBuffer) {
      MOZ_ASSERT(aStart < INT32_MAX, "Can't slice beyond 32-bit sample lengths");
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        mChannelData[channel] = AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(aStart));
      }
    }
    mDuration = aEnd - aStart;
  }
  TrackTicks GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const AudioChunk& aOther) const
  {
    if (aOther.mBuffer != mBuffer) {
      return false;
    }
    if (mBuffer) {
      NS_ASSERTION(aOther.mBufferFormat == mBufferFormat,
                   "Wrong metadata about buffer");
      NS_ASSERTION(aOther.mChannelData.Length() == mChannelData.Length(),
                   "Mismatched channel count");
      if (mDuration > INT32_MAX) {
        return false;
      }
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        if (aOther.mChannelData[channel] != AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(mDuration))) {
          return false;
        }
      }
    }
    return true;
  }
  bool IsNull() const { return mBuffer == nullptr; }
  void SetNull(TrackTicks aDuration)
  {
    mBuffer = nullptr;
    mChannelData.Clear();
    mDuration = aDuration;
    mVolume = 1.0f;
  }

  TrackTicks mDuration; // in frames within the buffer
  nsRefPtr<ThreadSharedObject> mBuffer; // the buffer object whose lifetime is managed; null means data is all zeroes
  nsTArray<const void*> mChannelData; // one pointer per channel; empty if and only if mBuffer is null
  float mVolume; // volume multiplier to apply (1.0f if mBuffer is nonnull)
  SampleFormat mBufferFormat; // format of frames in mBuffer (only meaningful if mBuffer is nonnull)
};

/**
 * A list of audio samples consisting of a sequence of slices of SharedBuffers.
 * The audio rate is determined by the track, not stored in this class.
 */
class AudioSegment : public MediaSegmentBase<AudioSegment, AudioChunk> {
public:
  typedef mozilla::AudioSampleFormat SampleFormat;

  AudioSegment() : MediaSegmentBase<AudioSegment, AudioChunk>(AUDIO), mChannels(0) {}

  bool IsInitialized()
  {
    return mChannels > 0;
  }
  void Init(int32_t aChannels)
  {
    NS_ASSERTION(aChannels > 0, "Bad number of channels");
    NS_ASSERTION(!IsInitialized(), "Already initialized");
    mChannels = aChannels;
  }
  int32_t GetChannels()
  {
    NS_ASSERTION(IsInitialized(), "Not initialized");
    return mChannels;
  }
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const float*>& aChannelData,
                    int32_t aDuration)
  {
    NS_ASSERTION(mChannels > 0, "Not initialized");
    NS_ASSERTION(!aBuffer.get() || aChannelData.Length() == uint32_t(mChannels),
                 "Wrong number of channels");
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
  }
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const int16_t*>& aChannelData,
                    int32_t aDuration)
  {
    NS_ASSERTION(mChannels > 0, "Not initialized");
    NS_ASSERTION(!aBuffer.get() || aChannelData.Length() == uint32_t(mChannels),
                 "Wrong number of channels");
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_S16;
  }
  void ApplyVolume(float aVolume);
  /**
   * aOutput must have a matching number of channels, but we will automatically
   * convert sample formats.
   */
  void WriteTo(AudioStream* aOutput);

  // Segment-generic methods not in MediaSegmentBase
  void InitFrom(const AudioSegment& aOther)
  {
    NS_ASSERTION(mChannels == 0, "Channels already set");
    mChannels = aOther.mChannels;
  }
  void CheckCompatible(const AudioSegment& aOther) const
  {
    NS_ASSERTION(aOther.mChannels == mChannels, "Non-matching channels");
  }
  static Type StaticType() { return AUDIO; }

protected:
  int32_t mChannels;
};

}

#endif /* MOZILLA_AUDIOSEGMENT_H_ */
