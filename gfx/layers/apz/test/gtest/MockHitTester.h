/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_MockHitTester_h
#define mozilla_layers_MockHitTester_h

#include "apz/src/IAPZHitTester.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/LayersTypes.h"

#include <queue>

namespace mozilla::layers {

// IAPZHitTester implementation for APZ gtests.
// This does not actually perform hit-testing, it just allows
// the test code to specify the expected hit test results.
class MockHitTester final : public IAPZHitTester {
 public:
  HitTestResult GetAPZCAtPoint(
      const ScreenPoint& aHitTestPoint,
      const RecursiveMutexAutoLock& aProofOfTreeLock) override;

  // Queue a hit test result whose target APZC is the APZC
  // with scroll id |aScrollId|, and the provided hit test flags.
  void QueueHitResult(ScrollableLayerGuid::ViewID aScrollId,
                      gfx::CompositorHitTestInfo aHitInfo);

  // Queue a hit test result whose target is the scrollbar of the APZC
  // with scroll id |aScrollId| in the direction specified by |aDirection|.
  void QueueScrollbarThumbHitResult(ScrollableLayerGuid::ViewID aScrollId,
                                    ScrollDirection aDirection);

 private:
  std::queue<HitTestResult> mQueuedResults;
};

}  // namespace mozilla::layers

#endif  // define mozilla_layers_MockHitTester_h
