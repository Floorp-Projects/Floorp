/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_VelocityTracker_h
#define mozilla_layers_VelocityTracker_h

#include <utility>
#include <cstdint>

#include "Axis.h"
#include "mozilla/Attributes.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class SimpleVelocityTracker : public VelocityTracker {
 public:
  explicit SimpleVelocityTracker(Axis* aAxis);
  void StartTracking(ParentLayerCoord aPos, uint32_t aTimestamp) override;
  Maybe<float> AddPosition(ParentLayerCoord aPos,
                           uint32_t aTimestampMs) override;
  Maybe<float> ComputeVelocity(uint32_t aTimestampMs) override;
  void Clear() override;

 private:
  void AddVelocityToQueue(uint32_t aTimestampMs, float aVelocity);
  float ApplyFlingCurveToVelocity(float aVelocity) const;

  // The Axis that uses this velocity tracker.
  // This is a raw pointer because the Axis owns the velocity tracker
  // by UniquePtr, so the velocity tracker cannot outlive the Axis.
  Axis* MOZ_NON_OWNING_REF mAxis;

  // A queue of (timestamp, velocity) pairs; these are the historical
  // velocities at the given timestamps. Timestamps are in milliseconds,
  // velocities are in screen pixels per ms. This member can only be
  // accessed on the controller/UI thread.
  nsTArray<std::pair<uint32_t, float>> mVelocityQueue;

  // mVelocitySampleTimeMs and mVelocitySamplePos are the time and position
  // used in the last velocity sampling. They get updated when a new sample is
  // taken (which may not happen on every input event, if the time delta is too
  // small).
  uint32_t mVelocitySampleTimeMs;
  ParentLayerCoord mVelocitySamplePos;
};

}  // namespace layers
}  // namespace mozilla

#endif
