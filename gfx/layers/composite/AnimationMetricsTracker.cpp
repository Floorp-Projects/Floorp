/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AnimationMetricsTracker.h"

#include <algorithm>
#include <inttypes.h>
#include "mozilla/Telemetry.h"

#define AMT_LOG(...)
// #define AMT_LOG(...) printf_stderr("AMT: " __VA_ARGS__)

namespace mozilla {
namespace layers {

AnimationMetricsTracker::AnimationMetricsTracker()
{
}

AnimationMetricsTracker::~AnimationMetricsTracker()
{
}

void
AnimationMetricsTracker::UpdateAnimationInProgress(bool aInProgress,
                                                   uint64_t aLayerArea)
{
  MOZ_ASSERT(aInProgress || aLayerArea == 0);
  if (mCurrentAnimationStart && !aInProgress) {
    AnimationEnded();
    mCurrentAnimationStart = TimeStamp();
    mMaxLayerAreaAnimated = 0;
  } else if (aInProgress) {
    if (!mCurrentAnimationStart) {
      mCurrentAnimationStart = TimeStamp::Now();
      mMaxLayerAreaAnimated = aLayerArea;
      AnimationStarted();
    } else {
      mMaxLayerAreaAnimated = std::max(mMaxLayerAreaAnimated, aLayerArea);
    }
  }
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

} // namespace layers
} // namespace mozilla
