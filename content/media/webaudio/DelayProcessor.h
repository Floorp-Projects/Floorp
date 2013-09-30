/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DelayProcessor_h_
#define DelayProcessor_h_

#include "nsTArray.h"

namespace mozilla {

class DelayProcessor {
public:
  // See WebAudioUtils::ComputeSmoothingRate() for frame to frame exponential
  // |smoothingRate| multiplier.
  DelayProcessor(int aMaxDelayFrames, double aSmoothingRate)
    : mSmoothingRate(aSmoothingRate)
    , mCurrentDelay(0.)
    , mMaxDelayFrames(aMaxDelayFrames)
    , mWriteIndex(0)
  {
  }

  // Process with an array of delays, in frames, for each frame.
  void Process(const double *aPerFrameDelays,
               const float* const* aInputChannels,
               float* const* aOutputChannels,
               int aChannelCount, int aFramesToProcess);

  // Process with a constant delay, which will be smoothed with the previous
  // delay.
  void Process(double aDelayFrames, const float* const* aInputChannels,
               float* const* aOutputChannels, int aChannelCount,
               int aFramesToProcess);

  void Reset() { mBuffer.Clear(); };

  int MaxDelayFrames() const { return mMaxDelayFrames; }
  int BufferChannelCount() const { return mBuffer.Length(); }

private:
  bool EnsureBuffer(uint32_t aNumberOfChannels);

  // Circular buffer for capturing delayed samples.
  AutoFallibleTArray<FallibleTArray<float>, 2> mBuffer;
  double mSmoothingRate;
  // Current delay, in fractional frames
  double mCurrentDelay;
  // Maximum delay, in frames
  int mMaxDelayFrames;
  // Write index for the buffer, to write the frames to the correct index of the buffer
  // given the current delay.
  int mWriteIndex;
};

} // mozilla

#endif // DelayProcessor_h_
