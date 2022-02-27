/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"
#include "mozilla/StaticPrefs_layout.h"

class APZCSnappingOnMomentumTesterMock : public APZCTreeManagerTester {
 public:
  APZCSnappingOnMomentumTesterMock() { CreateMockHitTester(); }
};

TEST_F(APZCSnappingOnMomentumTesterMock, Snap_On_Momentum) {
  const char* treeShape = "x";
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  CreateScrollData(treeShape, layerVisibleRegion);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 500));

  // Set up some basic scroll snapping
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

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  TimeStamp now = mcc->Time();

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), now);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 25), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 25), mcc->Time());

  // The velocity should be positive when panning with positive displacement.
  EXPECT_GT(apzc->GetVelocityVector().y, 3.0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // After lifting the fingers, the velocity should be zero and a smooth
  // animation should have been triggered for scroll snap.
  EXPECT_EQ(apzc->GetVelocityVector().y, 0);
  apzc->AssertStateIsSmoothMsdScroll();

  mcc->AdvanceByMillis(5);

  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 50), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());

  apzc->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      100.0f,
      apzc->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
          .y);
}

// Smililar to APZCSnappingTesterMock::SnapOnPanEnd but with momentum events.
TEST_F(APZCSnappingOnMomentumTesterMock, SnapOnPanMomentumEnd) {
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

  // Send a series of pan gestures that a pan-end event happens at 65 and some
  // pan momentum events happen subsequently.
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
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, position,
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);

  // A smooth animation has been triggered by the pan-end event above.
  apzc->AssertStateIsSmoothMsdScroll();

  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager, position,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);

  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager, position,
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);

  apzc->AdvanceAnimations(mcc->GetSampleTime());
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager, position,
             ScreenPoint(0, 0), mcc->Time());

  apzc->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      100);
}
