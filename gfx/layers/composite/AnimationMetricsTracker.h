/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationMetricsTracker_h
#define mozilla_layers_AnimationMetricsTracker_h

#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {

/**
 * Tracks the start and end of compositor animations.
 */
class AnimationMetricsTracker {
public:
  AnimationMetricsTracker();
  ~AnimationMetricsTracker();

  /**
   * This function should be called per composite, to inform the metrics
   * tracker if any animation is in progress, and if so, what area is
   * being animated. The aLayerArea is in Layer pixels squared.
   */
  void UpdateAnimationInProgress(bool aInProgress, uint64_t aLayerArea);

private:
  void AnimationStarted();
  void AnimationEnded();

  TimeStamp mCurrentAnimationStart;
  uint64_t mMaxLayerAreaAnimated;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AnimationMetricsTracker_h
