/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SCRATCHBUFFER_H_
#define MOZILLA_SCRATCHBUFFER_H_

#include "AudioSegment.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"
#include "nsDebug.h"

#include <algorithm>

namespace mozilla {

/**
 * The classes in this file provide a interface that uses frames as a unit.
 * However, they store their offsets in samples (because it's handy for pointer
 * operations). Those functions can convert between the two units.
 */
static inline uint32_t FramesToSamples(uint32_t aChannels, uint32_t aFrames) {
  return aFrames * aChannels;
}

static inline uint32_t SamplesToFrames(uint32_t aChannels, uint32_t aSamples) {
  MOZ_ASSERT(!(aSamples % aChannels), "Frame alignment is wrong.");
  return aSamples / aChannels;
}

/**
 * Class that gets a buffer pointer from an audio callback and provides a safe
 * interface to manipulate this buffer, and to ensure we are not missing frames
 * by the end of the callback.
 */
template <typename T>
class AudioCallbackBufferWrapper {
 public:
  AudioCallbackBufferWrapper()
      : mBuffer(nullptr), mSamples(0), mSampleWriteOffset(1), mChannels(0) {}

  explicit AudioCallbackBufferWrapper(uint32_t aChannels)
      : mBuffer(nullptr),
        mSamples(0),
        mSampleWriteOffset(1),
        mChannels(aChannels)

  {
    MOZ_ASSERT(aChannels);
  }

  AudioCallbackBufferWrapper& operator=(
      const AudioCallbackBufferWrapper& aOther) {
    MOZ_ASSERT(!aOther.mBuffer,
               "Don't use this ctor after AudioCallbackDriver::Init");
    MOZ_ASSERT(aOther.mSamples == 0,
               "Don't use this ctor after AudioCallbackDriver::Init");
    MOZ_ASSERT(aOther.mSampleWriteOffset == 1,
               "Don't use this ctor after AudioCallbackDriver::Init");
    MOZ_ASSERT(aOther.mChannels != 0);

    mBuffer = nullptr;
    mSamples = 0;
    mSampleWriteOffset = 1;
    mChannels = aOther.mChannels;

    return *this;
  }

  /**
   * Set the buffer in this wrapper. This is to be called at the beginning of
   * the callback.
   */
  void SetBuffer(T* aBuffer, uint32_t aFrames) {
    MOZ_ASSERT(!mBuffer && !mSamples, "SetBuffer called twice.");
    mBuffer = aBuffer;
    mSamples = FramesToSamples(mChannels, aFrames);
    mSampleWriteOffset = 0;
  }

  /**
   * Write some frames to the internal buffer. Free space in the buffer should
   * be checked prior to calling these.
   */
  void WriteFrames(T* aBuffer, uint32_t aFrames) {
    MOZ_ASSERT(aFrames <= Available(),
               "Writing more that we can in the audio buffer.");

    PodCopy(mBuffer + mSampleWriteOffset, aBuffer,
            FramesToSamples(mChannels, aFrames));
    mSampleWriteOffset += FramesToSamples(mChannels, aFrames);
  }
  void WriteFrames(const AudioChunk& aChunk, uint32_t aFrames) {
    MOZ_ASSERT(aFrames <= Available(),
               "Writing more that we can in the audio buffer.");

    InterleaveAndConvertBuffer(aChunk.ChannelData<T>().Elements(), aFrames,
                               aChunk.mVolume, aChunk.ChannelCount(),
                               mBuffer + mSampleWriteOffset);
    mSampleWriteOffset += FramesToSamples(mChannels, aFrames);
  }

  /**
   * Number of frames that can be written to the buffer.
   */
  uint32_t Available() {
    return SamplesToFrames(mChannels, mSamples - mSampleWriteOffset);
  }

  /**
   * Check that the buffer is completly filled, and reset internal state so this
   * instance can be reused.
   */
  void BufferFilled() {
    MOZ_ASSERT(Available() == 0, "Frames should have been written");
    MOZ_ASSERT(mBuffer, "Buffer not set.");
    mSamples = 0;
    mSampleWriteOffset = 0;
    mBuffer = nullptr;
  }

 private:
  /* This is not an owned pointer, but the pointer passed to use via the audio
   * callback. */
  T* mBuffer;
  /* The number of samples of this audio buffer. */
  uint32_t mSamples;
  /* The position at which new samples should be written. We want to return to
   * the audio callback iff this is equal to mSamples. */
  uint32_t mSampleWriteOffset;
  uint32_t mChannels;
};

/**
 * This is a class that interfaces with the AudioCallbackBufferWrapper, and is
 * responsible for storing the excess of data produced by the MediaTrackGraph
 * because of different rounding constraints, to be used the next time the audio
 * backend calls back.
 */
template <typename T, uint32_t BLOCK_SIZE>
class SpillBuffer {
 public:
  SpillBuffer() : mBuffer(nullptr), mPosition(0), mChannels(0) {}

  explicit SpillBuffer(uint32_t aChannels)
      : mPosition(0), mChannels(aChannels) {
    MOZ_ASSERT(aChannels);
    mBuffer = MakeUnique<T[]>(BLOCK_SIZE * mChannels);
    PodZero(mBuffer.get(), BLOCK_SIZE * mChannels);
  }

  SpillBuffer& operator=(SpillBuffer& aOther) {
    MOZ_ASSERT(aOther.mPosition == 0,
               "Don't use this ctor after AudioCallbackDriver::Init");
    MOZ_ASSERT(aOther.mChannels != 0);
    MOZ_ASSERT(aOther.mBuffer);

    mPosition = aOther.mPosition;
    mChannels = aOther.mChannels;
    mBuffer = std::move(aOther.mBuffer);

    return *this;
  }

  SpillBuffer& operator=(SpillBuffer&& aOther) {
    return this->operator=(aOther);
  }

  /* Empty the spill buffer into the buffer of the audio callback. This returns
   * the number of frames written. */
  uint32_t Empty(AudioCallbackBufferWrapper<T>& aBuffer) {
    uint32_t framesToWrite =
        std::min(aBuffer.Available(), SamplesToFrames(mChannels, mPosition));

    aBuffer.WriteFrames(mBuffer.get(), framesToWrite);

    mPosition -= FramesToSamples(mChannels, framesToWrite);
    // If we didn't empty the spill buffer for some reason, shift the remaining
    // data down
    if (mPosition > 0) {
      MOZ_ASSERT(FramesToSamples(mChannels, framesToWrite) + mPosition <=
                 BLOCK_SIZE * mChannels);
      PodMove(mBuffer.get(),
              mBuffer.get() + FramesToSamples(mChannels, framesToWrite),
              mPosition);
    }

    return framesToWrite;
  }
  /* Fill the spill buffer from aInput.
   * Return the number of frames written to the spill buffer */
  uint32_t Fill(const AudioChunk& aInput) {
    uint32_t framesToWrite =
        std::min(static_cast<uint32_t>(aInput.mDuration),
                 BLOCK_SIZE - SamplesToFrames(mChannels, mPosition));

    MOZ_ASSERT(FramesToSamples(mChannels, framesToWrite) + mPosition <=
               BLOCK_SIZE * mChannels);
    InterleaveAndConvertBuffer(
        aInput.ChannelData<T>().Elements(), framesToWrite, aInput.mVolume,
        aInput.ChannelCount(), mBuffer.get() + mPosition);

    mPosition += FramesToSamples(mChannels, framesToWrite);

    return framesToWrite;
  }

 private:
  /* The spilled data. */
  UniquePtr<T[]> mBuffer;
  /* The current write position, in samples, in the buffer when filling, or the
   * amount of buffer filled when emptying. */
  uint32_t mPosition;
  uint32_t mChannels;
};

}  // namespace mozilla

#endif  // MOZILLA_SCRATCHBUFFER_H_
