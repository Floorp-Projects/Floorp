/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZPublicUtils.h"

#include "AsyncPanZoomController.h"
#include "mozilla/StaticPrefs_general.h"

namespace mozilla {
namespace layers {

namespace apz {

/*static*/ void InitializeGlobalState() {
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
}

/*static*/ const ScreenMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics, const ParentLayerPoint& aVelocity) {
  return AsyncPanZoomController::CalculatePendingDisplayPort(aFrameMetrics,
                                                             aVelocity);
}

/*static*/ std::pair<int32_t, int32_t> GetMouseWheelAnimationDurations() {
  // Special code for migrating users from old values to new values over
  // a period of time. The old values are defaults prior to bug 1660933, which
  // we hard-code here. The user's migration percentage is stored in the
  // migration pref. If the migration percentage is zero, the user gets the old
  // values, and at a 100 percentage the user gets the new values. Linear
  // interpolation in between. We can control the speed of migration by
  // increasing the percentage value over time (e.g. by increasing the min
  // bound on the clamped migration value). Once it reaches 100 the migration
  // code can be removed.

  int32_t minMS = StaticPrefs::general_smoothScroll_mouseWheel_durationMinMS();
  int32_t maxMS = StaticPrefs::general_smoothScroll_mouseWheel_durationMaxMS();

  const int32_t oldMin = 200;
  const int32_t oldMax = 400;

  int32_t migration =
      StaticPrefs::general_smoothScroll_mouseWheel_migrationPercent();
  migration = clamped(migration, 0, 100);

  minMS = ((oldMin * (100 - migration)) + (minMS * migration)) / 100;
  maxMS = ((oldMax * (100 - migration)) + (maxMS * migration)) / 100;

  return std::make_pair(minMS, maxMS);
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
