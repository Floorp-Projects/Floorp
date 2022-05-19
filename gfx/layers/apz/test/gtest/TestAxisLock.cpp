/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"
#include "gtest/gtest.h"

#include <cmath>

class APZCAxisLockTester : public APZCTreeManagerTester {
 public:
  APZCAxisLockTester() { CreateMockHitTester(); }

  UniquePtr<ScopedLayerTreeRegistration> registration;

  RefPtr<TestAsyncPanZoomController> apzc;

  void SetupBasicTest() {
    const char* treeShape = "x";
    LayerIntRegion layerVisibleRegion[] = {
        LayerIntRect(0, 0, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRegion);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

    UpdateHitTestingTree();
  }
};

TEST_F(APZCAxisLockTester, BasicDominantAxisUse) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 1);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.lock_angle", M_PI / 4.0f);

  SetupBasicTest();

  apzc = ApzcOf(root);

  // Kick off the initial gesture that triggers the momentum scroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());

  // Should be in a PANNING_LOCKED_Y state with no horizontal velocity.
  apzc->AssertStateIsPanningLockedY();
  apzc->AssertAxisLocked(ScrollDirection::eVertical);
  EXPECT_GT(apzc->GetVelocityVector().y, 0);
  EXPECT_EQ(apzc->GetVelocityVector().x, 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have not panned on the horizontal axis.
  ParentLayerPoint panEndOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);
  EXPECT_EQ(panEndOffset.x, 0);

  // The lock onto the Y axis extends into momentum scroll.
  apzc->AssertAxisLocked(ScrollDirection::eVertical);

  // Start the momentum scroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 50), ScreenPoint(30, 90), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 50), ScreenPoint(10, 30), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 50), ScreenPoint(10, 30), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // In momentum locking mode, we should still be locked onto the Y axis.
  apzc->AssertStateIsPanMomentum();
  apzc->AssertAxisLocked(ScrollDirection::eVertical);
  EXPECT_GT(apzc->GetVelocityVector().y, 0);
  EXPECT_EQ(apzc->GetVelocityVector().x, 0);

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager,
             ScreenIntPoint(50, 50), ScreenPoint(0, 0), mcc->Time());

  // After momentum scroll end, ensure we are no longer locked onto an axis.
  apzc->AssertNotAxisLocked();

  // Wait until the end of the animation and ensure the final state is
  // reasonable.
  apzc->AdvanceAnimationsUntilEnd();
  ParentLayerPoint finalOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);

  // Ensure we have scrolled some amount on the Y axis in momentum scroll.
  EXPECT_GT(finalOffset.y, panEndOffset.y);
  EXPECT_EQ(finalOffset.x, 0.0f);
}

TEST_F(APZCAxisLockTester, NewGestureBreaksMomentumAxisLock) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 1);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.lock_angle", M_PI / 4.0f);

  SetupBasicTest();

  apzc = ApzcOf(root);

  // Kick off the initial gesture that triggers the momentum scroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(2, 1), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(30, 15), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(30, 15), mcc->Time());

  // Should be in a PANNING_LOCKED_X state with no vertical velocity.
  apzc->AssertStateIsPanningLockedX();
  apzc->AssertAxisLocked(ScrollDirection::eHorizontal);
  EXPECT_GT(apzc->GetVelocityVector().x, 0);
  EXPECT_EQ(apzc->GetVelocityVector().y, 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Double check that we have not panned on the vertical axis.
  ParentLayerPoint panEndOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);
  EXPECT_EQ(panEndOffset.y, 0);

  // Ensure that the axis locks extends into momentum scroll.
  apzc->AssertAxisLocked(ScrollDirection::eHorizontal);

  // Start the momentum scroll.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 50), ScreenPoint(80, 40), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 50), ScreenPoint(20, 10), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 50), ScreenPoint(20, 10), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // In momentum locking mode, we should still be locked onto the X axis.
  apzc->AssertStateIsPanMomentum();
  apzc->AssertAxisLocked(ScrollDirection::eHorizontal);
  EXPECT_GT(apzc->GetVelocityVector().x, 0);
  EXPECT_EQ(apzc->GetVelocityVector().y, 0);

  ParentLayerPoint beforeBreakOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);
  EXPECT_EQ(beforeBreakOffset.y, 0);
  // Ensure we have scrolled some amount on the X axis in momentum scroll.
  EXPECT_GT(beforeBreakOffset.x, panEndOffset.x);

  // Kick off the gesture that breaks the lock onto the X axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  ParentLayerPoint afterBreakOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());

  // The lock onto the X axis should be broken and we now should be locked
  // onto the Y axis.
  apzc->AssertStateIsPanningLockedY();
  apzc->AssertAxisLocked(ScrollDirection::eVertical);
  EXPECT_GT(apzc->GetVelocityVector().y, 0);
  EXPECT_EQ(apzc->GetVelocityVector().x, 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // The lock onto the Y axis extends into momentum scroll.
  apzc->AssertAxisLocked(ScrollDirection::eVertical);

  // Wait until the end of the animation and ensure the final state is
  // reasonable.
  apzc->AdvanceAnimationsUntilEnd();
  ParentLayerPoint finalOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting);

  EXPECT_GT(finalOffset.y, 0);
  // Ensure that we did not scroll on the X axis after the vertical scroll
  // started.
  EXPECT_EQ(finalOffset.x, afterBreakOffset.x);
}
