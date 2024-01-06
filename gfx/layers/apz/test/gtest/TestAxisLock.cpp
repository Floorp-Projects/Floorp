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

class APZCAxisLockCompatTester : public APZCTreeManagerTester,
                                 public testing::WithParamInterface<int> {
 public:
  APZCAxisLockCompatTester() : oldAxisLockMode(0) { CreateMockHitTester(); }

  int oldAxisLockMode;

  UniquePtr<ScopedLayerTreeRegistration> registration;

  RefPtr<TestAsyncPanZoomController> apzc;

  void SetUp() {
    APZCTreeManagerTester::SetUp();

    oldAxisLockMode = Preferences::GetInt("apz.axis_lock.mode");

    Preferences::SetInt("apz.axis_lock.mode", GetParam());
  }

  void TearDown() {
    APZCTreeManagerTester::TearDown();

    Preferences::SetInt("apz.axis_lock.mode", oldAxisLockMode);
  }

  static std::string PrintFromParam(const testing::TestParamInfo<int>& info) {
    switch (info.param) {
      case 0:
        return "FREE";
      case 1:
        return "STANDARD";
      case 2:
        return "STICKY";
      case 3:
        return "DOMINANT_AXIS";
      default:
        return "UNKNOWN";
    }
  }
};

class APZCAxisLockTester : public APZCTreeManagerTester {
 public:
  APZCAxisLockTester() { CreateMockHitTester(); }

  UniquePtr<ScopedLayerTreeRegistration> registration;

  RefPtr<TestAsyncPanZoomController> apzc;

  void SetupBasicTest() {
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));

    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

    UpdateHitTestingTree();
  }

  void BreakStickyAxisLockTestGesture(const ScrollDirections& aDirections) {
    float panX = 0;
    float panY = 0;

    if (aDirections.contains(ScrollDirection::eVertical)) {
      panY = 30;
    }
    if (aDirections.contains(ScrollDirection::eHorizontal)) {
      panX = 30;
    }

    // Kick off the gesture that may lock onto an axis
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
               ScreenPoint(panX, panY), mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());

    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
               ScreenPoint(panX, panY), mcc->Time());
  }

  void BreakStickyAxisLockTest(const ScrollDirections& aDirections) {
    // Create the gesture for the test.
    BreakStickyAxisLockTestGesture(aDirections);

    // Based on the scroll direction(s) ensure the state is what we expect.
    if (aDirections == ScrollDirection::eVertical) {
      apzc->AssertStateIsPanningLockedY();
      apzc->AssertAxisLocked(ScrollDirection::eVertical);
      EXPECT_GT(apzc->GetVelocityVector().y, 0);
      EXPECT_EQ(apzc->GetVelocityVector().x, 0);
    } else if (aDirections == ScrollDirection::eHorizontal) {
      apzc->AssertStateIsPanningLockedX();
      apzc->AssertAxisLocked(ScrollDirection::eHorizontal);
      EXPECT_GT(apzc->GetVelocityVector().x, 0);
      EXPECT_EQ(apzc->GetVelocityVector().y, 0);
    } else {
      apzc->AssertStateIsPanning();
      apzc->AssertNotAxisLocked();
      EXPECT_GT(apzc->GetVelocityVector().x, 0);
      EXPECT_GT(apzc->GetVelocityVector().y, 0);
    }

    // Cleanup for next test.
    apzc->AdvanceAnimationsUntilEnd();
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
      AsyncTransformConsumer::eForEventHandling);
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
      AsyncTransformConsumer::eForEventHandling);

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
      AsyncTransformConsumer::eForEventHandling);
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
      AsyncTransformConsumer::eForEventHandling);
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
      AsyncTransformConsumer::eForEventHandling);

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
      AsyncTransformConsumer::eForEventHandling);

  EXPECT_GT(finalOffset.y, 0);
  // Ensure that we did not scroll on the X axis after the vertical scroll
  // started.
  EXPECT_EQ(finalOffset.x, afterBreakOffset.x);
}

TEST_F(APZCAxisLockTester, BreakStickyAxisLock) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.lock_angle", M_PI / 6.0f);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.breakout_angle", M_PI / 6.0f);

  SetupBasicTest();

  apzc = ApzcOf(root);

  // Start a gesture to get us locked onto the Y axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have locked onto the Y axis.
  apzc->AssertStateIsPanningLockedY();

  // Test switch to locking onto the X axis.
  BreakStickyAxisLockTest(ScrollDirection::eHorizontal);

  // Test switch back to locking onto the Y axis.
  BreakStickyAxisLockTest(ScrollDirection::eVertical);

  // Test breaking all axis locks from a Y axis lock.
  BreakStickyAxisLockTest(ScrollDirections(ScrollDirection::eHorizontal,
                                           ScrollDirection::eVertical));

  // We should be in a panning state.
  apzc->AssertStateIsPanning();
  apzc->AssertNotAxisLocked();

  // Lock back to the X axis.
  BreakStickyAxisLockTestGesture(ScrollDirection::eHorizontal);

  // End the gesture.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Start a gesture to get us locked onto the X axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have locked onto the X axis.
  apzc->AssertStateIsPanningLockedX();

  // Test breaking all axis locks from a X axis lock.
  BreakStickyAxisLockTest(ScrollDirections(ScrollDirection::eHorizontal,
                                           ScrollDirection::eVertical));

  // We should be in a panning state.
  apzc->AssertStateIsPanning();
  apzc->AssertNotAxisLocked();

  // Test switch back to locking onto the Y axis.
  BreakStickyAxisLockTest(ScrollDirection::eVertical);
}

TEST_F(APZCAxisLockTester, BreakAxisLockByLockAngle) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.lock_angle", M_PI / 4.0f);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.breakout_angle", M_PI / 8.0f);

  SetupBasicTest();

  apzc = ApzcOf(root);

  // Start a gesture to get us locked onto the Y axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have locked onto the Y axis.
  apzc->AssertStateIsPanningLockedY();

  // Stay within 45 degrees from the X axis, and more than 22.5 degrees from
  // the Y axis. This should break the Y lock and lock us to the X axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(12, 10), mcc->Time());
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have locked onto the X axis.
  apzc->AssertStateIsPanningLockedX();

  // End the gesture.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  apzc->AdvanceAnimations(mcc->GetSampleTime());
}

TEST_F(APZCAxisLockTester, TestDominantAxisScrolling) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 3);

  int panY;
  int panX;

  SetupBasicTest();

  apzc = ApzcOf(root);

  ParentLayerPoint lastOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncPanZoomController::eForEventHandling);

  // In dominant axis mode, test pan gesture events with varying gesture
  // angles and ensure that we only pan on one axis.
  for (panX = 0, panY = 50; panY >= 0; panY -= 10, panX += 5) {
    // Gesture that should be locked onto one axis
    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
    PanGesture(PanGestureInput::PANGESTURE_START, manager,
               ScreenIntPoint(50, 50), ScreenIntPoint(panX, panY), mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());

    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
               ScreenPoint(static_cast<float>(panX), static_cast<float>(panY)),
               mcc->Time());
    mcc->AdvanceByMillis(5);
    apzc->AdvanceAnimations(mcc->GetSampleTime());

    QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
    PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
               ScreenPoint(0, 0), mcc->Time());
    apzc->AdvanceAnimationsUntilEnd();

    ParentLayerPoint scrollOffset = apzc->GetCurrentAsyncScrollOffset(
        AsyncPanZoomController::eForEventHandling);

    if (panX > panY) {
      // If we're closer to the X axis ensure that we moved on the horizontal
      // axis and there was no movement on the vertical axis.
      EXPECT_GT(scrollOffset.x, lastOffset.x);
      EXPECT_EQ(scrollOffset.y, lastOffset.y);
    } else {
      // If we're closer to the Y axis ensure that we moved on the vertical
      // axis and there was no movement on the horizontal axis.
      EXPECT_GT(scrollOffset.y, lastOffset.y);
      EXPECT_EQ(scrollOffset.x, lastOffset.x);
    }

    lastOffset = scrollOffset;
  }
}

TEST_F(APZCAxisLockTester, TestCanScrollWithAxisLock) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);

  SetupBasicTest();

  apzc = ApzcOf(root);

  // The axis locks do not impact CanScroll()
  apzc->SetAxisLocked(ScrollDirection::eHorizontal, true);
  EXPECT_EQ(apzc->CanScroll(ParentLayerPoint(10, 0)), true);

  apzc->SetAxisLocked(ScrollDirection::eHorizontal, false);
  apzc->SetAxisLocked(ScrollDirection::eVertical, true);
  EXPECT_EQ(apzc->CanScroll(ParentLayerPoint(0, 10)), true);
}

TEST_F(APZCAxisLockTester, TestScrollHandoffAxisLockConflict) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);

  // Create two scrollable frames. One parent frame with one child.
  const char* treeShape = "x(x)";
  LayerIntRect layerVisibleRect[] = {
      LayerIntRect(0, 0, 100, 100),
      LayerIntRect(0, 0, 100, 100),
  };
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 500, 500));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 500, 500));
  SetScrollHandoff(layers[1], root);

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> rootApzc = ApzcOf(root);
  apzc = ApzcOf(layers[1]);

  // Create a gesture on the y-axis that should lock the x axis.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(0, 15), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimationsUntilEnd();

  // We are locked onto the y-axis.
  apzc->AssertAxisLocked(ScrollDirection::eVertical);

  // There should be movement in the child.
  ParentLayerPoint childCurrentOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncTransformConsumer::eForEventHandling);
  EXPECT_GT(childCurrentOffset.y, 0);
  EXPECT_EQ(childCurrentOffset.x, 0);

  // There should be no movement in the parent.
  ParentLayerPoint parentCurrentOffset = rootApzc->GetCurrentAsyncScrollOffset(
      AsyncTransformConsumer::eForEventHandling);
  EXPECT_EQ(parentCurrentOffset.y, 0);
  EXPECT_EQ(parentCurrentOffset.x, 0);

  // Create a gesture on the x-axis, that should be directed
  // at the child, even if the x-axis is locked.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimationsUntilEnd();

  // We broke the y-axis lock and are now locked onto the x-axis.
  apzc->AssertAxisLocked(ScrollDirection::eHorizontal);

  // There should be some movement in the child on the x-axis.
  ParentLayerPoint childFinalOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncTransformConsumer::eForEventHandling);
  EXPECT_GT(childFinalOffset.x, 0);

  // There should still be no movement in the parent.
  ParentLayerPoint parentFinalOffset = rootApzc->GetCurrentAsyncScrollOffset(
      AsyncTransformConsumer::eForEventHandling);
  EXPECT_EQ(parentFinalOffset.y, 0);
  EXPECT_EQ(parentFinalOffset.x, 0);
}

// The delta from the initial pan gesture should be reflected in the
// current offset for all axis locking modes.
TEST_P(APZCAxisLockCompatTester, TestPanGestureStart) {
  const char* treeShape = "x";
  LayerIntRect layerVisibleRect[] = {
      LayerIntRect(0, 0, 100, 100),
  };
  CreateScrollData(treeShape, layerVisibleRect);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 500, 500));

  registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

  UpdateHitTestingTree();

  apzc = ApzcOf(root);

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimationsUntilEnd();
  ParentLayerPoint currentOffset = apzc->GetCurrentAsyncScrollOffset(
      AsyncTransformConsumer::eForEventHandling);

  EXPECT_EQ(currentOffset.x, 0);
  EXPECT_EQ(currentOffset.y, 10);
}

// All APZCAxisLockCompatTester tests should be run for each apz.axis_lock.mode.
// If another mode is added, the value should be added to this list.
INSTANTIATE_TEST_SUITE_P(APZCAxisLockCompat, APZCAxisLockCompatTester,
                         testing::Values(0, 1, 2, 3),
                         APZCAxisLockCompatTester::PrintFromParam);
