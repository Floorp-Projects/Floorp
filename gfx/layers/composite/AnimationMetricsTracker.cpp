/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AnimationMetricsTracker.h"

#include <algorithm>
#include <cmath>
#include <inttypes.h>

#define AMT_LOG(...)
// #define AMT_LOG(...) printf_stderr("AMT: " __VA_ARGS__)

namespace mozilla {
namespace layers {

AnimationMetricsTracker::AnimationMetricsTracker()
  : mMaxLayerAreaAnimated(0)
{
}

AnimationMetricsTracker::~AnimationMetricsTracker()
{
}

void
AnimationMetricsTracker::UpdateAnimationInProgress(AnimationProcessTypes aActive,
                                                   uint64_t aLayerArea,
                                                   TimeDuration aVsyncInterval)
{
  bool inProgress = (aActive != AnimationProcessTypes::eNone);
  MOZ_ASSERT(inProgress || aLayerArea == 0);
  if (mCurrentAnimationStart && !inProgress) {
    AnimationEnded();
    mCurrentAnimationStart = TimeStamp();
    mMaxLayerAreaAnimated = 0;
  } else if (inProgress) {
    if (!mCurrentAnimationStart) {
      mCurrentAnimationStart = TimeStamp::Now();
      mMaxLayerAreaAnimated = aLayerArea;
      AnimationStarted();
    } else {
      mMaxLayerAreaAnimated = std::max(mMaxLayerAreaAnimated, aLayerArea);
    }
  }

  UpdateAnimationThroughput("chrome",
                            (aActive & AnimationProcessTypes::eChrome) != AnimationProcessTypes::eNone,
                            mChromeAnimation,
                            aVsyncInterval,
                            Telemetry::COMPOSITOR_ANIMATION_THROUGHPUT_CHROME,
                            Telemetry::COMPOSITOR_ANIMATION_MAX_CONTIGUOUS_DROPS_CHROME);
  UpdateAnimationThroughput("content",
                            (aActive & AnimationProcessTypes::eContent) != AnimationProcessTypes::eNone,
                            mContentAnimation,
                            aVsyncInterval,
                            Telemetry::COMPOSITOR_ANIMATION_THROUGHPUT_CONTENT,
                            Telemetry::COMPOSITOR_ANIMATION_MAX_CONTIGUOUS_DROPS_CONTENT);
}

void
AnimationMetricsTracker::UpdateApzAnimationInProgress(bool aInProgress,
                                                      TimeDuration aVsyncInterval)
{
  UpdateAnimationThroughput("apz",
                            aInProgress,
                            mApzAnimation,
                            aVsyncInterval,
                            Telemetry::COMPOSITOR_ANIMATION_THROUGHPUT_APZ,
                            Telemetry::COMPOSITOR_ANIMATION_MAX_CONTIGUOUS_DROPS_APZ);
}

void
AnimationMetricsTracker::AnimationStarted()
{
}

void
AnimationMetricsTracker::AnimationEnded()
{
  MOZ_ASSERT(mCurrentAnimationStart);

  Telemetry::AccumulateTimeDelta(Telemetry::COMPOSITOR_ANIMATION_DURATION, mCurrentAnimationStart);
  Telemetry::Accumulate(Telemetry::COMPOSITOR_ANIMATION_MAX_LAYER_AREA, mMaxLayerAreaAnimated);
  AMT_LOG("Ended animation; duration: %f ms, area: %" PRIu64 "\n",
    (TimeStamp::Now() - mCurrentAnimationStart).ToMilliseconds(),
    mMaxLayerAreaAnimated);
}

void
AnimationMetricsTracker::UpdateAnimationThroughput(const char* aLabel,
                                                   bool aInProgress,
                                                   AnimationData& aAnimation,
                                                   TimeDuration aVsyncInterval,
                                                   Telemetry::HistogramID aThroughputHistogram,
                                                   Telemetry::HistogramID aMaxDropsHistogram)
{
  if (aInProgress && !aAnimation.mStart) {
    // the animation just started
    aAnimation.mStart = TimeStamp::Now();
    aAnimation.mLastFrameTime = aAnimation.mStart;
    aAnimation.mLongestFrame = TimeDuration();
    aAnimation.mFrameCount = 1;
    AMT_LOG("Compositor animation of type %s just started\n", aLabel);
  } else if (aInProgress && aAnimation.mStart) {
    // the animation continues
    aAnimation.mFrameCount++;
    TimeStamp now = TimeStamp::Now();
    aAnimation.mLongestFrame = std::max(aAnimation.mLongestFrame, now - aAnimation.mLastFrameTime);
    aAnimation.mLastFrameTime = now;
  } else if (!aInProgress && aAnimation.mStart) {
    // the animation just ended

    TimeStamp now = TimeStamp::Now();
    // Get the length and clear aAnimation.mStart before the early-returns below
    TimeDuration animationLength = now - aAnimation.mStart;
    aAnimation.mStart = TimeStamp();

    if (aVsyncInterval == TimeDuration::Forever()) {
      AMT_LOG("Invalid vsync interval: forever\n");
      return;
    }
    double vsyncIntervalMs = aVsyncInterval.ToMilliseconds();
    if (vsyncIntervalMs < 1.0f) {
      // Guard to avoid division by zero or other crazy results below
      AMT_LOG("Invalid vsync interval: %fms\n", vsyncIntervalMs);
      return;
    }

    // We round the expectedFrameCount because it's a count and should be an
    // integer. The animationLength might not be an exact vsync multiple because
    // it's taken during the composition process and the amount of work done
    // between the vsync signal and the Timestamp::Now() call may vary slightly
    // from one composite to another.
    uint32_t expectedFrameCount = std::lround(animationLength.ToMilliseconds() / vsyncIntervalMs);
    AMT_LOG("Type %s ran for %fms (interval: %fms), %u frames (expected: %u)\n",
        aLabel, animationLength.ToMilliseconds(), vsyncIntervalMs,
        aAnimation.mFrameCount, expectedFrameCount);
    if (expectedFrameCount <= 0) {
      // Graceful handling of probably impossible thing, unless the clock
      // changes while running?
      // Note that we also skip the frames-dropped probe if this happens,
      // because we cannot be sure that the frame length measurements are valid.
      return;
    }

    // Scale up by 1000 because telemetry takes ints, truncate intentionally
    // to avoid artificial inflation of the result.
    uint32_t frameHitRatio = (uint32_t)(1000.0f * aAnimation.mFrameCount / expectedFrameCount);
    Telemetry::Accumulate(aThroughputHistogram, frameHitRatio);
    AMT_LOG("Reported frameHitRatio %u\n", frameHitRatio);

    // Get the longest frame time (make sure to check the final frame as well)
    TimeDuration longestFrame = std::max(aAnimation.mLongestFrame, now - aAnimation.mLastFrameTime);
    // As above, we round to get the frame count. Additionally we subtract one
    // from the frame count to get the number of dropped frames.
    uint32_t framesDropped = std::lround(longestFrame.ToMilliseconds() / vsyncIntervalMs) - 1;
    AMT_LOG("Longest frame was %fms (%d drops)\n", longestFrame.ToMilliseconds(), framesDropped);
    Telemetry::Accumulate(aMaxDropsHistogram, framesDropped);
  }
}

} // namespace layers
} // namespace mozilla
