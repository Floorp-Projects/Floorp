/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_TIMEPACKETIZER_H_
#define DOM_MEDIA_TIMEPACKETIZER_H_

#include "AudioPacketizer.h"
#include "TimeUnits.h"

namespace mozilla {
/**
 * This class wraps an AudioPacketizer and provides packets of audio with
 * timestamps.
 */
template <typename InputType, typename OutputType>
class TimedPacketizer {
 public:
  TimedPacketizer(uint32_t aPacketSize, uint32_t aChannels,
                  int64_t aInitialTick, int64_t aBase)
      : mPacketizer(aPacketSize, aChannels),
        mTimeStamp(media::TimeUnit(aInitialTick, aBase)),
        mBase(aBase) {}

  void Input(const InputType* aFrames, uint32_t aFrameCount) {
    mPacketizer.Input(aFrames, aFrameCount);
  }

  media::TimeUnit Output(OutputType* aOutputBuffer) {
    MOZ_ASSERT(mPacketizer.PacketsAvailable());
    media::TimeUnit pts = mTimeStamp;
    mPacketizer.Output(aOutputBuffer);
    mTimeStamp += media::TimeUnit(mPacketizer.mPacketSize, mBase);
    return pts;
  }

  media::TimeUnit Drain(OutputType* aOutputBuffer, uint32_t& aWritten) {
    MOZ_ASSERT(!mPacketizer.PacketsAvailable(),
               "Consume all packets before calling drain");
    media::TimeUnit pts = mTimeStamp;
    aWritten = mPacketizer.Output(aOutputBuffer);
    mTimeStamp += media::TimeUnit(mPacketizer.mPacketSize, mBase);
    return pts;
  }

  // Call this when a discontinuity in input has been detected, e.g. based on
  // the pts of input packets.
  void Discontinuity(int64_t aNewTick) {
    MOZ_ASSERT(!mPacketizer.FramesAvailable(),
               "Drain before enqueueing discontinuous audio");
    mTimeStamp = media::TimeUnit(aNewTick, mBase);
  }

  void Clear() { mPacketizer.Clear(); }

  uint32_t PacketSize() const { return mPacketizer.mPacketSize; }

  uint32_t ChannelCount() const { return mPacketizer.mChannels; }

  uint32_t PacketsAvailable() const { return mPacketizer.PacketsAvailable(); }

  uint32_t FramesAvailable() const { return mPacketizer.FramesAvailable(); }

 private:
  AudioPacketizer<InputType, OutputType> mPacketizer;
  media::TimeUnit mTimeStamp;
  uint64_t mBase;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_TIMEPACKETIZER_H_
