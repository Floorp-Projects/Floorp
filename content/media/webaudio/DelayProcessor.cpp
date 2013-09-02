/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayProcessor.h"

#include "mozilla/PodOperations.h"
#include "AudioSegment.h"

namespace mozilla {

void
DelayProcessor::Process(const double *aPerFrameDelays,
                        const float* const* aInputChannels,
                        float* const* aOutputChannels,
                        int aChannelCount, int aFramesToProcess)
{
  if (!EnsureBuffer(aChannelCount)) {
    for (int channel = 0; channel < aChannelCount; ++channel) {
      PodZero(aOutputChannels[channel], aFramesToProcess);
    }
    return;
  }

  for (int channel = 0; channel < aChannelCount; ++channel) {
    double currentDelayFrames = mCurrentDelay;
    int writeIndex = mWriteIndex;

    float* buffer = mBuffer[channel].Elements();
    const uint32_t bufferLength = mBuffer[channel].Length();
    const float* input = aInputChannels ? aInputChannels[channel] : nullptr;
    float* output = aOutputChannels[channel];

    for (int i = 0; i < aFramesToProcess; ++i) {
      currentDelayFrames = clamped(aPerFrameDelays[i],
                                   0.0, static_cast<double>(mMaxDelayFrames));

      // Write the input sample to the correct location in our buffer
      buffer[writeIndex] = input ? input[i] : 0.0f;

      // Now, determine the correct read position.  We adjust the read position to be
      // from currentDelayFrames frames in the past.  We also interpolate the two input
      // frames in case the read position does not match an integer index.
      double readPosition = writeIndex + bufferLength - currentDelayFrames;
      if (readPosition >= bufferLength) {
        readPosition -= bufferLength;
      }
      MOZ_ASSERT(readPosition >= 0.0, "Why are we reading before the beginning of the buffer?");

      // Here is a the reason why readIndex1 and readIndex will never be out
      // of bounds.  The maximum value for bufferLength is 180 * 48000 (see
      // AudioContext::CreateDelay).  The maximum value for mCurrentDelay is
      // 180.0, so initially readPosition cannot be more than bufferLength +
      // a fraction less than 1.  Then we take care of that case by
      // subtracting bufferLength from it if needed.  So, if
      // |bufferLength-readPosition<1.0|, readIndex1 will end up being zero.
      // If |1.0<=bufferLength-readPosition<2.0|, readIndex1 will be
      // bufferLength-1 and readIndex2 will be 0.
      int readIndex1 = int(readPosition);
      int readIndex2 = (readIndex1 + 1) % bufferLength;
      double interpolationFactor = readPosition - readIndex1;

      output[i] = (1.0 - interpolationFactor) * buffer[readIndex1] +
                         interpolationFactor  * buffer[readIndex2];
      writeIndex = (writeIndex + 1) % bufferLength;
    }

    // Remember currentDelayFrames and writeIndex for the next ProduceAudioBlock
    // call when processing the last channel.
    if (channel == aChannelCount - 1) {
      mCurrentDelay = currentDelayFrames;
      mWriteIndex = writeIndex;
    }
  }
}

void
DelayProcessor::Process(double aDelayFrames, const float* const* aInputChannels,
                        float* const* aOutputChannels, int aChannelCount,
                        int aFramesToProcess)
{
  const bool firstTime = !mBuffer.Length();
  double currentDelay = firstTime ? aDelayFrames : mCurrentDelay;

  nsAutoTArray<double, WEBAUDIO_BLOCK_SIZE> computedDelay;
  computedDelay.SetLength(aFramesToProcess);

  for (int i = 0; i < aFramesToProcess; ++i) {
    // If the value has changed, smoothly approach it
    currentDelay += (aDelayFrames - currentDelay) * mSmoothingRate;
    computedDelay[i] = currentDelay;
  }

  Process(computedDelay.Elements(), aInputChannels, aOutputChannels,
          aChannelCount, aFramesToProcess);
}

bool
DelayProcessor::EnsureBuffer(uint32_t aNumberOfChannels)
{
  if (aNumberOfChannels == 0) {
    return false;
  }
  if (mBuffer.Length() == 0) {
    if (!mBuffer.SetLength(aNumberOfChannels)) {
      return false;
    }
    // The length of the buffer is one greater than the maximum delay so that
    // writing an input frame does not overwrite the frame that would
    // subsequently be read at maximum delay.
    const int numFrames = mMaxDelayFrames + 1;
    for (uint32_t channel = 0; channel < aNumberOfChannels; ++channel) {
      if (!mBuffer[channel].SetLength(numFrames)) {
        return false;
      }
      PodZero(mBuffer[channel].Elements(), numFrames);
    }
  } else if (mBuffer.Length() != aNumberOfChannels) {
    // TODO: Handle changes in the channel count
    return false;
  }
  return true;
}

} // mozilla
