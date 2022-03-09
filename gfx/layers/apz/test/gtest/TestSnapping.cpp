/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_mousewheel.h"

class APZCSnappingTesterMock : public APZCTreeManagerTester {
 public:
  APZCSnappingTesterMock() { CreateMockHitTester(); }
};

TEST_F(APZCSnappingTesterMock, Bug1265510) {
  const char* treeShape = "x(x)";
  nsIntRegion layerVisibleRegion[] = {nsIntRegion(IntRect(0, 0, 100, 100)),
                                      nsIntRegion(IntRect(0, 100, 100, 100))};
  CreateScrollData(treeShape, layerVisibleRegion);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            CSSRect(0, 0, 100, 200));
  SetScrollHandoff(layers[1], root);

  ScrollSnapInfo snap;
  snap.mScrollSnapStrictnessY = StyleScrollSnapStrictness::Mandatory;
  snap.mSnapPositionY.AppendElement(0 * AppUnitsPerCSSPixel());
  snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());

  ModifyFrameMetrics(root, [&](ScrollMetadata& aSm, FrameMetrics&) {
    aSm.SetSnapInfo(ScrollSnapInfo(snap));
  });

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();

  TestAsyncPanZoomController* outer = ApzcOf(layers[0]);
  TestAsyncPanZoomController* inner = ApzcOf(layers[1]);

  // Position the mouse near the bottom of the outer frame and scroll by 60px.
  // (6 lines of 10px each). APZC will actually scroll to y=100 because of the
  // mandatory snap coordinate there.
  TimeStamp now = mcc->Time();
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  SmoothWheel(manager, ScreenIntPoint(50, 80), ScreenPoint(0, 6), now);
  // Advance in 5ms increments until we've scrolled by 70px. At this point, the
  // closest snap point is y=100, and the inner frame should be under the mouse
  // cursor.
  while (outer
             ->GetCurrentAsyncScrollOffset(
                 AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
             .y < 70) {
    mcc->AdvanceByMillis(5);
    outer->AdvanceAnimations(mcc->GetSampleTime());
  }
  // Now do another wheel in a new transaction. This should start scrolling the
  // inner frame; we verify that it does by checking the inner scroll position.
  TimeStamp newTransactionTime =
      now + TimeDuration::FromMilliseconds(
                StaticPrefs::mousewheel_transaction_timeout() + 100);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID + 1);
  SmoothWheel(manager, ScreenIntPoint(50, 80), ScreenPoint(0, 6),
              newTransactionTime);
  inner->AdvanceAnimationsUntilEnd();
  EXPECT_LT(
      0.0f,
      inner
          ->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
          .y);

  // However, the outer frame should also continue to the snap point, otherwise
  // it is demonstrating incorrect behaviour by violating the mandatory
  // snapping.
  outer->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      100.0f,
      outer
          ->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
          .y);
}

TEST_F(APZCSnappingTesterMock, Snap_After_Pinch) {
  const char* treeShape = "x";
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  CreateScrollData(treeShape, layerVisibleRegion);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));

  // Set up some basic scroll snapping
  ScrollSnapInfo snap;
  snap.mScrollSnapStrictnessY = StyleScrollSnapStrictness::Mandatory;

  snap.mSnapPositionY.AppendElement(0 * AppUnitsPerCSSPixel());
  snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());

  // Save the scroll snap info on the root APZC.
  // Also mark the root APZC as "root content", since APZC only allows
  // zooming on the root content APZC.
  ModifyFrameMetrics(root, [&](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aSm.SetSnapInfo(ScrollSnapInfo(snap));
    aMetrics.SetIsRootContent(true);
  });

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Allow zooming
  apzc->UpdateZoomConstraints(ZoomConstraints(
      true, true, CSSToParentLayerScale(0.25f), CSSToParentLayerScale(4.0f)));

  PinchWithPinchInput(apzc, ScreenIntPoint(50, 50), ScreenIntPoint(50, 50),
                      1.2f);

  apzc->AssertStateIsSmoothMsdScroll();
}

// Currently fails on Android because on the platform we have a different
// VelocityTracker.
#ifndef MOZ_WIDGET_ANDROID
TEST_F(APZCSnappingTesterMock, SnapOnPanEndWithZeroVelocity) {
  // Use pref values for desktop everywhere.
  SCOPED_GFX_PREF_FLOAT("apz.fling_friction", 0.002);
  SCOPED_GFX_PREF_FLOAT("apz.fling_stopped_threshold", 0.01);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_x1", 0.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_x2", 1.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_y1", 0.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_y2", 1.0);
  SCOPED_GFX_PREF_INT("apz.velocity_relevance_time_ms", 100);

  const char* treeShape = "x";
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  CreateScrollData(treeShape, layerVisibleRegion);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 400));

  // Set up two snap points, 30 and 100.
  ScrollSnapInfo snap;
  snap.mScrollSnapStrictnessY = StyleScrollSnapStrictness::Mandatory;
  snap.mSnapPositionY.AppendElement(30 * AppUnitsPerCSSPixel());
  snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());

  // Save the scroll snap info on the root APZC.
  ModifyFrameMetrics(root, [&](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aSm.SetSnapInfo(ScrollSnapInfo(snap));
  });

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Send a series of pan gestures to scroll to position at 50.
  const ScreenIntPoint position = ScreenIntPoint(50, 30);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, position,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, position,
             ScreenPoint(0, 40), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Make sure the velocity just before sending a pan-end is zero.
  EXPECT_EQ(apzc->GetVelocityVector().y, 0);

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, position,
             ScreenPoint(0, 0), mcc->Time());

  // Now a smooth animation has been triggered for snapping to 30.
  apzc->AssertStateIsSmoothMsdScroll();

  apzc->AdvanceAnimationsUntilEnd();
  // The snapped position should be 30 rather than 100 because it's the nearest
  // snap point.
  EXPECT_EQ(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      30);
}

// Smililar to above SnapOnPanEndWithZeroVelocity but with positive velocity so
// that the snap position would be the one in the scrolling direction.
TEST_F(APZCSnappingTesterMock, SnapOnPanEndWithPositiveVelocity) {
  // Use pref values for desktop everywhere.
  SCOPED_GFX_PREF_FLOAT("apz.fling_friction", 0.002);
  SCOPED_GFX_PREF_FLOAT("apz.fling_stopped_threshold", 0.01);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_x1", 0.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_x2", 1.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_y1", 0.0);
  SCOPED_GFX_PREF_FLOAT("apz.fling_curve_function_y2", 1.0);
  SCOPED_GFX_PREF_INT("apz.velocity_relevance_time_ms", 100);

  const char* treeShape = "x";
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  CreateScrollData(treeShape, layerVisibleRegion);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 400));

  // Set up two snap points, 30 and 100.
  ScrollSnapInfo snap;
  snap.mScrollSnapStrictnessY = StyleScrollSnapStrictness::Mandatory;
  snap.mSnapPositionY.AppendElement(30 * AppUnitsPerCSSPixel());
  snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());

  // Save the scroll snap info on the root APZC.
  ModifyFrameMetrics(root, [&](ScrollMetadata& aSm, FrameMetrics& aMetrics) {
    aSm.SetSnapInfo(ScrollSnapInfo(snap));
  });

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Send a series of pan gestures that a pan-end event happens at 65
  const ScreenIntPoint position = ScreenIntPoint(50, 30);
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, position,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, position,
             ScreenPoint(0, 35), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, position,
             ScreenPoint(0, 20), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // There should be positive velocity in this case.
  EXPECT_GT(apzc->GetVelocityVector().y, 0);

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, position,
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);

  // A smooth animation has been triggered by the pan-end event above.
  apzc->AssertStateIsSmoothMsdScroll();

  apzc->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      100);
}
#endif
