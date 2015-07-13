/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SCRATCHBUFFER_H_
#define MOZILLA_SCRATCHBUFFER_H_
#include <mozilla/PodOperations.h>
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
template<typename T, uint32_t CHANNELS>
class AudioCallbackBufferWrapper
{
public:
  AudioCallbackBufferWrapper()
    : mBuffer(nullptr),
      mSamples(0),
      mSampleWriteOffset(1)
  {}
  /**
   * Set the buffer in this wrapper. This is to be called at the beginning of
   * the callback.
   */
  void SetBuffer(T* aBuffer, uint32_t aFrames) {
    MOZ_ASSERT(!mBuffer && !mSamples,
        "SetBuffer called twice.");
    mBuffer = aBuffer;
    mSamples = FramesToSamples(CHANNELS, aFrames);
    mSampleWriteOffset = 0;
  }

  /**
   * Write some frames to the internal buffer. Free space in the buffer should
   * be check prior to calling this.
   */
  void WriteFrames(T* aBuffer, uint32_t aFrames) {
    MOZ_ASSERT(aFrames <= Available(),
        "Writing more that we can in the audio buffer.");

    PodCopy(mBuffer + mSampleWriteOffset, aBuffer, FramesToSamples(CHANNELS,
                                                                   aFrames));
    mSampleWriteOffset += FramesToSamples(CHANNELS, aFrames);
  }

  /**
   * Number of frames that can be written to the buffer.
   */
  uint32_t Available() {
    return SamplesToFrames(CHANNELS, mSamples - mSampleWriteOffset);
  }

  /**
   * Check that the buffer is completly filled, and reset internal state so this
   * instance can be reused.
   */
  void BufferFilled() {
    // It's okay to have exactly zero samples here, it can happen we have an
    // audio callback driver because of a hint on MSG creation, but the
    // AudioOutputStream has not been created yet, or if all the streams have finished
    // but we're still running.
    // Note: it's also ok if we had data in the scratch buffer - and we usually do - and
    // all the streams were ended (no mixer callback occured).
    // XXX Remove this warning, or find a way to avoid it if the mixer callback
    // isn't called.
    NS_WARN_IF_FALSE(Available() == 0 || mSampleWriteOffset == 0,
            "Audio Buffer is not full by the end of the callback.");
    // Make sure the data returned is always set and not random!
    if (Available()) {
      PodZero(mBuffer + mSampleWriteOffset, FramesToSamples(CHANNELS, Available()));
    }
    MOZ_ASSERT(mSamples, "Buffer not set.");
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
};

/**
 * This is a class that interfaces with the AudioCallbackBufferWrapper, and is
 * responsible for storing the excess of data produced by the MediaStreamGraph
 * because of different rounding constraints, to be used the next time the audio
 * backend calls back.
 */
template<typename T, uint32_t BLOCK_SIZE, uint32_t CHANNELS>
class SpillBuffer
{
public:
  SpillBuffer()
  : mPosition(0)
  {
    PodArrayZero(mBuffer);
  }
  /* Empty the spill buffer into the buffer of the audio callback. This returns
   * the number of frames written. */
  uint32_t Empty(AudioCallbackBufferWrapper<T, CHANNELS>& aBuffer) {
    uint32_t framesToWrite = std::min(aBuffer.Available(),
                                      SamplesToFrames(CHANNELS, mPosition));

    aBuffer.WriteFrames(mBuffer, framesToWrite);

    mPosition -= FramesToSamples(CHANNELS, framesToWrite);
    // If we didn't empty the spill buffer for some reason, shift the remaining data down
    if (mPosition > 0) {
      PodMove(mBuffer, mBuffer + FramesToSamples(CHANNELS, framesToWrite),
              mPosition);
    }

    return framesToWrite;
  }
  /* Fill the spill buffer from aInput, containing aFrames frames, return the
   * number of frames written to the spill buffer */
  uint32_t Fill(T* aInput, uint32_t aFrames) {
    uint32_t framesToWrite = std::min(aFrames,
                                      BLOCK_SIZE - SamplesToFrames(CHANNELS,
                                                                   mPosition));

    PodCopy(mBuffer + mPosition, aInput, FramesToSamples(CHANNELS,
                                                         framesToWrite));

    mPosition += FramesToSamples(CHANNELS, framesToWrite);

    return framesToWrite;
  }
private:
  /* The spilled data. */
  T mBuffer[BLOCK_SIZE * CHANNELS];
  /* The current write position, in samples, in the buffer when filling, or the
   * amount of buffer filled when emptying. */
  uint32_t mPosition;
};

} // namespace mozilla

#endif // MOZILLA_SCRATCHBUFFER_H_
