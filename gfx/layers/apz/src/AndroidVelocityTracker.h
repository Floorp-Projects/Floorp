/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AndroidVelocityTracker_h
#define mozilla_layers_AndroidVelocityTracker_h

#include <utility>
#include <cstdint>

#include "Axis.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class AndroidVelocityTracker : public VelocityTracker {
 public:
  explicit AndroidVelocityTracker();
  void StartTracking(ParentLayerCoord aPos, uint32_t aTimestamp) override;
  Maybe<float> AddPosition(ParentLayerCoord aPos,
                           uint32_t aTimestampMs) override;
  float HandleDynamicToolbarMovement(uint32_t aStartTimestampMs,
                                     uint32_t aEndTimestampMs,
                                     ParentLayerCoord aDelta) override;
  Maybe<float> ComputeVelocity(uint32_t aTimestampMs) override;
  void Clear() override;

 private:
  // A queue of (timestamp, position) pairs; these are the historical
  // positions at the given timestamps. Timestamps are in milliseconds.
  nsTArray<std::pair<uint32_t, ParentLayerCoord>> mHistory;
  // The last time an event was added to the tracker (in milliseconds),
  // or zero if no events have been added.
  uint32_t mLastEventTime;
  // The amount by which the page has moved relative to the screen (caused
  // by dynamic toolbar movement) since we have started tracking velocity.
  ParentLayerCoord mAdditionalDelta;
};

}  // namespace layers
}  // namespace mozilla

#endif
