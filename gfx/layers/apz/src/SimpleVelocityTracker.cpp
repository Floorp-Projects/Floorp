/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleVelocityTracker.h"

#include "mozilla/ServoStyleConsts.h"  // for StyleComputedTimingFunction
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPtr.h"  // for StaticAutoPtr

static mozilla::LazyLogModule sApzSvtLog("apz.simplevelocitytracker");
#define SVT_LOG(...) MOZ_LOG(sApzSvtLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

// When we compute the velocity we do so by taking two input events and
// dividing the distance delta over the time delta. In some cases the time
// delta can be really small, which can make the velocity computation very
// volatile. To avoid this we impose a minimum time delta below which we do
// not recompute the velocity.
const TimeDuration MIN_VELOCITY_SAMPLE_TIME = TimeDuration::FromMilliseconds(5);

extern StaticAutoPtr<StyleComputedTimingFunction> gVelocityCurveFunction;

SimpleVelocityTracker::SimpleVelocityTracker(Axis* aAxis)
    : mAxis(aAxis), mVelocitySamplePos(0) {}

void SimpleVelocityTracker::StartTracking(ParentLayerCoord aPos,
                                          TimeStamp aTimestamp) {
  Clear();
  mVelocitySampleTime = aTimestamp;
  mVelocitySamplePos = aPos;
}

Maybe<float> SimpleVelocityTracker::AddPosition(ParentLayerCoord aPos,
                                                TimeStamp aTimestamp) {
  if (aTimestamp <= mVelocitySampleTime + MIN_VELOCITY_SAMPLE_TIME) {
    // See also the comment on MIN_VELOCITY_SAMPLE_TIME.
    // We don't update either mVelocitySampleTime or mVelocitySamplePos so that
    // eventually when we do get an event with the required time delta we use
    // the corresponding distance delta as well.
    SVT_LOG("%p|%s skipping velocity computation for small time delta %f ms\n",
            mAxis->OpaqueApzcPointer(), mAxis->Name(),
            (aTimestamp - mVelocitySampleTime).ToMilliseconds());
    return Nothing();
  }

  float newVelocity =
      (float)(mVelocitySamplePos - aPos) /
      (float)(aTimestamp - mVelocitySampleTime).ToMilliseconds();

  newVelocity = ApplyFlingCurveToVelocity(newVelocity);

  SVT_LOG("%p|%s updating velocity to %f with touch\n",
          mAxis->OpaqueApzcPointer(), mAxis->Name(), newVelocity);
  mVelocitySampleTime = aTimestamp;
  mVelocitySamplePos = aPos;

  AddVelocityToQueue(aTimestamp, newVelocity);

  return Some(newVelocity);
}

Maybe<float> SimpleVelocityTracker::ComputeVelocity(TimeStamp aTimestamp) {
  float velocity = 0;
  int count = 0;
  for (const auto& e : mVelocityQueue) {
    TimeDuration timeDelta = (aTimestamp - e.first);
    if (timeDelta < TimeDuration::FromMilliseconds(
                        StaticPrefs::apz_velocity_relevance_time_ms())) {
      count++;
      velocity += e.second;
    }
  }
  mVelocityQueue.Clear();
  if (count > 1) {
    velocity /= count;
  }
  return Some(velocity);
}

void SimpleVelocityTracker::Clear() { mVelocityQueue.Clear(); }

void SimpleVelocityTracker::AddVelocityToQueue(TimeStamp aTimestamp,
                                               float aVelocity) {
  mVelocityQueue.AppendElement(std::make_pair(aTimestamp, aVelocity));
  if (mVelocityQueue.Length() >
      StaticPrefs::apz_max_velocity_queue_size_AtStartup()) {
    mVelocityQueue.RemoveElementAt(0);
  }
}

float SimpleVelocityTracker::ApplyFlingCurveToVelocity(float aVelocity) const {
  float newVelocity = aVelocity;
  if (StaticPrefs::apz_max_velocity_inches_per_ms() > 0.0f) {
    bool velocityIsNegative = (newVelocity < 0);
    newVelocity = fabs(newVelocity);

    float maxVelocity =
        mAxis->ToLocalVelocity(StaticPrefs::apz_max_velocity_inches_per_ms());
    newVelocity = std::min(newVelocity, maxVelocity);

    if (StaticPrefs::apz_fling_curve_threshold_inches_per_ms() > 0.0f &&
        StaticPrefs::apz_fling_curve_threshold_inches_per_ms() <
            StaticPrefs::apz_max_velocity_inches_per_ms()) {
      float curveThreshold = mAxis->ToLocalVelocity(
          StaticPrefs::apz_fling_curve_threshold_inches_per_ms());
      if (newVelocity > curveThreshold) {
        // here, 0 < curveThreshold < newVelocity <= maxVelocity, so we apply
        // the curve
        float scale = maxVelocity - curveThreshold;
        float funcInput = (newVelocity - curveThreshold) / scale;
        float funcOutput =
            gVelocityCurveFunction->At(funcInput, /* aBeforeFlag = */ false);
        float curvedVelocity = (funcOutput * scale) + curveThreshold;
        SVT_LOG("%p|%s curving up velocity from %f to %f\n",
                mAxis->OpaqueApzcPointer(), mAxis->Name(), newVelocity,
                curvedVelocity);
        newVelocity = curvedVelocity;
      }
    }

    if (velocityIsNegative) {
      newVelocity = -newVelocity;
    }
  }

  return newVelocity;
}

}  // namespace layers
}  // namespace mozilla
