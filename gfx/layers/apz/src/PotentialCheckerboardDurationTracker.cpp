/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PotentialCheckerboardDurationTracker.h"

#include "mozilla/Telemetry.h"          // for Telemetry

namespace mozilla {
namespace layers {

PotentialCheckerboardDurationTracker::PotentialCheckerboardDurationTracker()
  : mInCheckerboard(false)
  , mInTransform(false)
{
}

void
PotentialCheckerboardDurationTracker::CheckerboardSeen()
{
  // This might get called while mInCheckerboard is already true
  if (!Tracking()) {
    mCurrentPeriodStart = TimeStamp::Now();
  }
  mInCheckerboard = true;
}

void
PotentialCheckerboardDurationTracker::CheckerboardDone()
{
  MOZ_ASSERT(Tracking());
  mInCheckerboard = false;
  if (!Tracking()) {
    mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::CHECKERBOARD_POTENTIAL_DURATION,
        mCurrentPeriodStart);
  }
}

void
PotentialCheckerboardDurationTracker::InTransform(bool aInTransform)
{
  if (aInTransform == mInTransform) {
    // no-op
    return;
  }

  if (!Tracking()) {
    // Because !Tracking(), mInTransform must be false, and so aInTransform
    // must be true (or we would have early-exited this function already).
    // Therefore, we are starting a potential checkerboard period.
    mInTransform = aInTransform;
    mCurrentPeriodStart = TimeStamp::Now();
    return;
  }

  mInTransform = aInTransform;

  if (!Tracking()) {
    // Tracking() must have been true at the start of this function, or we
    // would have taken the other !Tracking branch above. If it's false now,
    // it means we just stopped tracking, so we are ending a potential
    // checkerboard period.
    mozilla::Telemetry::AccumulateTimeDelta(
        mozilla::Telemetry::CHECKERBOARD_POTENTIAL_DURATION,
        mCurrentPeriodStart);
  }
}

bool
PotentialCheckerboardDurationTracker::Tracking() const
{
  return mInTransform || mInCheckerboard;
}

} // namespace layers
} // namespace mozilla
