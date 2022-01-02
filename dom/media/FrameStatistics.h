/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FrameStatistics_h_
#define FrameStatistics_h_

#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

struct FrameStatisticsData {
  // Number of frames parsed and demuxed from media.
  // Access protected by mReentrantMonitor.
  uint64_t mParsedFrames = 0;

  // Number of parsed frames which were actually decoded.
  // Access protected by mReentrantMonitor.
  uint64_t mDecodedFrames = 0;

  // Number of parsed frames which were dropped in the decoder.
  // Access protected by mReentrantMonitor.
  uint64_t mDroppedDecodedFrames = 0;

  // Number of decoded frames which were dropped in the sink
  // Access protected by mReentrantMonitor.
  uint64_t mDroppedSinkFrames = 0;

  // Number of sinked frames which were dropped in the compositor
  // Access protected by mReentrantMonitor.
  uint64_t mDroppedCompositorFrames = 0;

  // Number of decoded frames which were actually sent down the rendering
  // pipeline to be painted ("presented"). Access protected by
  // mReentrantMonitor.
  uint64_t mPresentedFrames = 0;

  // Sum of all inter-keyframe segment durations, in microseconds.
  // Dividing by count will give the average inter-keyframe time.
  uint64_t mInterKeyframeSum_us = 0;
  // Number of inter-keyframe segments summed so far.
  size_t mInterKeyframeCount = 0;

  // Maximum inter-keyframe segment duration, in microseconds.
  uint64_t mInterKeyFrameMax_us = 0;

  FrameStatisticsData() = default;
  FrameStatisticsData(uint64_t aParsed, uint64_t aDecoded, uint64_t aPresented,
                      uint64_t aDroppedDecodedFrames,
                      uint64_t aDroppedSinkFrames,
                      uint64_t aDroppedCompositorFrames)
      : mParsedFrames(aParsed),
        mDecodedFrames(aDecoded),
        mDroppedDecodedFrames(aDroppedDecodedFrames),
        mDroppedSinkFrames(aDroppedSinkFrames),
        mDroppedCompositorFrames(aDroppedCompositorFrames),
        mPresentedFrames(aPresented) {}

  void Accumulate(const FrameStatisticsData& aStats) {
    mParsedFrames += aStats.mParsedFrames;
    mDecodedFrames += aStats.mDecodedFrames;
    mPresentedFrames += aStats.mPresentedFrames;
    mDroppedDecodedFrames += aStats.mDroppedDecodedFrames;
    mDroppedSinkFrames += aStats.mDroppedSinkFrames;
    mDroppedCompositorFrames += aStats.mDroppedCompositorFrames;
    mInterKeyframeSum_us += aStats.mInterKeyframeSum_us;
    mInterKeyframeCount += aStats.mInterKeyframeCount;
    // It doesn't make sense to add max numbers, instead keep the bigger one.
    if (mInterKeyFrameMax_us < aStats.mInterKeyFrameMax_us) {
      mInterKeyFrameMax_us = aStats.mInterKeyFrameMax_us;
    }
  }
};

// Frame decoding/painting related performance counters.
// Threadsafe.
class FrameStatistics {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FrameStatistics);

  FrameStatistics() : mReentrantMonitor("FrameStats") {}

  // Returns a copy of all frame statistics data.
  // Can be called on any thread.
  FrameStatisticsData GetFrameStatisticsData() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mFrameStatisticsData;
  }

  // Returns number of frames which have been parsed from the media.
  // Can be called on any thread.
  uint64_t GetParsedFrames() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mFrameStatisticsData.mParsedFrames;
  }

  // Returns the number of parsed frames which have been decoded.
  // Can be called on any thread.
  uint64_t GetDecodedFrames() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mFrameStatisticsData.mDecodedFrames;
  }

  // Returns the number of decoded frames which have been sent to the rendering
  // pipeline for painting ("presented").
  // Can be called on any thread.
  uint64_t GetPresentedFrames() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return mFrameStatisticsData.mPresentedFrames;
  }

  // Returns the number of presented and dropped frames
  // Can be called on any thread.
  uint64_t GetTotalFrames() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return GetTotalFrames(mFrameStatisticsData);
  }

  static uint64_t GetTotalFrames(const FrameStatisticsData& aData) {
    return aData.mPresentedFrames + GetDroppedFrames(aData);
  }

  // Returns the number of frames that have been skipped because they have
  // missed their composition deadline.
  uint64_t GetDroppedFrames() const {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    return GetDroppedFrames(mFrameStatisticsData);
  }

  static uint64_t GetDroppedFrames(const FrameStatisticsData& aData) {
    return aData.mDroppedDecodedFrames + aData.mDroppedSinkFrames +
           aData.mDroppedCompositorFrames;
  }

  // Increments the parsed and decoded frame counters by the passed in counts.
  // Can be called on any thread.
  void Accumulate(const FrameStatisticsData& aStats) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mFrameStatisticsData.Accumulate(aStats);
  }

  // Increments the presented frame counters.
  // Can be called on any thread.
  void NotifyPresentedFrame() {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    ++mFrameStatisticsData.mPresentedFrames;
  }

  // Stack based class to assist in notifying the frame statistics of
  // parsed and decoded frames. Use inside video demux & decode functions
  // to ensure all parsed and decoded frames are reported on all return paths.
  class AutoNotifyDecoded {
   public:
    explicit AutoNotifyDecoded(FrameStatistics* aFrameStats)
        : mFrameStats(aFrameStats) {}
    ~AutoNotifyDecoded() {
      if (mFrameStats) {
        mFrameStats->Accumulate(mStats);
      }
    }

    FrameStatisticsData mStats;

   private:
    FrameStatistics* mFrameStats;
  };

 private:
  ~FrameStatistics() = default;

  // ReentrantMonitor to protect access of playback statistics.
  mutable ReentrantMonitor mReentrantMonitor;

  FrameStatisticsData mFrameStatisticsData;
};

}  // namespace mozilla

#endif  // FrameStatistics_h_
