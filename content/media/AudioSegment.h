/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOSEGMENT_H_
#define MOZILLA_AUDIOSEGMENT_H_

#include "MediaSegment.h"
#include "nsISupportsImpl.h"
#include "nsAudioStream.h"
#include "SharedBuffer.h"

namespace mozilla {

struct AudioChunk {
  typedef nsAudioStream::SampleFormat SampleFormat;

  // Generic methods
  void SliceTo(TrackTicks aStart, TrackTicks aEnd)
  {
    NS_ASSERTION(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
                 "Slice out of bounds");
    if (mBuffer) {
      mOffset += PRInt32(aStart);
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
      NS_ASSERTION(aOther.mBufferFormat == mBufferFormat && aOther.mBufferLength == mBufferLength,
                   "Wrong metadata about buffer");
      return aOther.mOffset == mOffset + mDuration && aOther.mVolume == mVolume;
    }
    return true;
  }
  bool IsNull() const { return mBuffer == nsnull; }
  void SetNull(TrackTicks aDuration)
  {
    mBuffer = nsnull;
    mDuration = aDuration;
    mOffset = 0;
    mVolume = 1.0f;
  }

  TrackTicks mDuration;           // in frames within the buffer
  nsRefPtr<SharedBuffer> mBuffer; // null means data is all zeroes
  PRInt32 mBufferLength;          // number of frames in mBuffer (only meaningful if mBuffer is nonnull)
  SampleFormat mBufferFormat;     // format of frames in mBuffer (only meaningful if mBuffer is nonnull)
  PRInt32 mOffset;                // in frames within the buffer (zero if mBuffer is null)
  float mVolume;                  // volume multiplier to apply (1.0f if mBuffer is nonnull)
};

/**
 * A list of audio samples consisting of a sequence of slices of SharedBuffers.
 * The audio rate is determined by the track, not stored in this class.
 */
class AudioSegment : public MediaSegmentBase<AudioSegment, AudioChunk> {
public:
  typedef nsAudioStream::SampleFormat SampleFormat;

  static int GetSampleSize(SampleFormat aFormat)
  {
    switch (aFormat) {
    case nsAudioStream::FORMAT_U8: return 1;
    case nsAudioStream::FORMAT_S16_LE: return 2;
    case nsAudioStream::FORMAT_FLOAT32: return 4;
    }
    NS_ERROR("Bad format");
    return 0;
  }

  AudioSegment() : MediaSegmentBase<AudioSegment, AudioChunk>(AUDIO), mChannels(0) {}

  bool IsInitialized()
  {
    return mChannels > 0;
  }
  void Init(PRInt32 aChannels)
  {
    NS_ASSERTION(aChannels > 0, "Bad number of channels");
    NS_ASSERTION(!IsInitialized(), "Already initialized");
    mChannels = aChannels;
  }
  PRInt32 GetChannels()
  {
    NS_ASSERTION(IsInitialized(), "Not initialized");
    return mChannels;
  }
  /**
   * Returns the format of the first audio frame that has data, or
   * FORMAT_FLOAT32 if there is none.
   */
  SampleFormat GetFirstFrameFormat()
  {
    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      if (ci->mBuffer) {
        return ci->mBufferFormat;
      }
    }
    return nsAudioStream::FORMAT_FLOAT32;
  }
  void AppendFrames(already_AddRefed<SharedBuffer> aBuffer, PRInt32 aBufferLength,
                    PRInt32 aStart, PRInt32 aEnd, SampleFormat aFormat)
  {
    NS_ASSERTION(mChannels > 0, "Not initialized");
    AudioChunk* chunk = AppendChunk(aEnd - aStart);
    chunk->mBuffer = aBuffer;
    chunk->mBufferFormat = aFormat;
    chunk->mBufferLength = aBufferLength;
    chunk->mOffset = aStart;
    chunk->mVolume = 1.0f;
  }
  void ApplyVolume(float aVolume);
  /**
   * aOutput must have a matching number of channels, but we will automatically
   * convert sample formats.
   */
  void WriteTo(nsAudioStream* aOutput);

  void AppendFrom(AudioSegment* aSource)
  {
    NS_ASSERTION(aSource->mChannels == mChannels, "Non-matching channels");
    MediaSegmentBase<AudioSegment, AudioChunk>::AppendFrom(aSource);
  }

  // Segment-generic methods not in MediaSegmentBase
  void InitFrom(const AudioSegment& aOther)
  {
    NS_ASSERTION(mChannels == 0, "Channels already set");
    mChannels = aOther.mChannels;
  }
  void SliceFrom(const AudioSegment& aOther, TrackTicks aStart, TrackTicks aEnd)
  {
    InitFrom(aOther);
    BaseSliceFrom(aOther, aStart, aEnd);
  }
  static Type StaticType() { return AUDIO; }

protected:
  PRInt32 mChannels;
};

}

#endif /* MOZILLA_AUDIOSEGMENT_H_ */
