/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationMetricsTracker_h
#define mozilla_layers_AnimationMetricsTracker_h

#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace layers {

enum class AnimationProcessTypes {
  eNone = 0x0,
  eContent = 0x1,
  eChrome = 0x2
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(AnimationProcessTypes)

/**
 * Tracks the start and end of compositor animations.
 */
class AnimationMetricsTracker {
public:
  AnimationMetricsTracker();
  ~AnimationMetricsTracker();

  /**
   * This function should be called per composite, to inform the metrics
   * tracker which processes have active animations. If there is are animations
   * in progress, the sum of their areas should also be provided, along with
   * the vsync interval.
   */
  void UpdateAnimationInProgress(AnimationProcessTypes aActive, uint64_t aLayerArea,
                                 TimeDuration aVsyncInterval);

private:
  void AnimationStarted();
  void AnimationEnded();
  void UpdateAnimationThroughput(const char* aLabel,
                                 bool aInProgress,
                                 TimeStamp& aStartTime,
                                 uint32_t& aFrameCount,
                                 TimeDuration aVsyncInterval,
                                 Telemetry::HistogramID aHistogram);

  // The start time of the current compositor animation. This just tracks
  // whether the compositor is running an animation, without regard to which
  // process the animation is coming from.
  TimeStamp mCurrentAnimationStart;
  // The max area (in layer pixels) that the current compositor animation
  // has touched on any given animation frame.
  uint64_t mMaxLayerAreaAnimated;

  // The start time of the current chrome-process animation.
  TimeStamp mChromeAnimationStart;
  // The number of frames composited for the current chrome-process animation.
  uint32_t mChromeAnimationFrameCount;
  // The start time of the current content-process animation.
  TimeStamp mContentAnimationStart;
  // The number of frames composited for the current content-process animation.
  uint32_t mContentAnimationFrameCount;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AnimationMetricsTracker_h
