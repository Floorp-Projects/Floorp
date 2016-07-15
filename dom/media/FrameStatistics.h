/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FrameStatistics_h_
#define FrameStatistics_h_

namespace mozilla {

// Frame decoding/painting related performance counters.
// Threadsafe.
class FrameStatistics
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FrameStatistics);

  FrameStatistics()
    : mReentrantMonitor("FrameStats")
    , mParsedFrames(0)
    , mDecodedFrames(0)
    , mPresentedFrames(0)
    , mDroppedFrames(0)
  {}

  // Returns number of frames which have been parsed from the media.
  // Can be called on any thread.
  uint32_t GetParsedFrames() const
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mParsedFrames;
  }

  // Returns the number of parsed frames which have been decoded.
  // Can be called on any thread.
  uint32_t GetDecodedFrames() const
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mDecodedFrames;
  }

  // Returns the number of decoded frames which have been sent to the rendering
  // pipeline for painting ("presented").
  // Can be called on any thread.
  uint32_t GetPresentedFrames() const
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mPresentedFrames;
  }

  // Returns the number of frames that have been skipped because they have
  // missed their composition deadline.
  uint32_t GetDroppedFrames() const
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mDroppedFrames;
  }

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                           uint32_t aDropped)
  {
    if (aParsed == 0 && aDecoded == 0 && aDropped == 0) {
      return;
    }
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mParsedFrames += aParsed;
    mDecodedFrames += aDecoded;
    mDroppedFrames += aDropped;
  }

  // Increments the presented frame counters.
  // Can be called on any thread.
  void NotifyPresentedFrame()
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    ++mPresentedFrames;
  }

private:
  ~FrameStatistics() {}

  // ReentrantMonitor to protect access of playback statistics.
  mutable ReentrantMonitor mReentrantMonitor;

  // Number of frames parsed and demuxed from media.
  // Access protected by mReentrantMonitor.
  uint32_t mParsedFrames;

  // Number of parsed frames which were actually decoded.
  // Access protected by mReentrantMonitor.
  uint32_t mDecodedFrames;

  // Number of decoded frames which were actually sent down the rendering
  // pipeline to be painted ("presented"). Access protected by mReentrantMonitor.
  uint32_t mPresentedFrames;

  // Number of frames that have been skipped because they have missed their
  // composition deadline.
  uint32_t mDroppedFrames;
};

} // namespace mozilla

#endif // FrameStatistics_h_
