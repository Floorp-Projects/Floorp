/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"
#include "mozilla/StaticPrefs_layout.h"

class APZCSnappingOnMomentumTesterLayersOnly : public APZCTreeManagerTester {
 public:
  APZCSnappingOnMomentumTesterLayersOnly() { mLayersOnly = true; }
};

TEST_F(APZCSnappingOnMomentumTesterLayersOnly, Snap_On_Momentum) {
  const char* layerTreeSyntax = "c";
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  root =
      CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 500));

  // Set up some basic scroll snapping
  ScrollSnapInfo snap;
  snap.mScrollSnapStrictnessY = StyleScrollSnapStrictness::Mandatory;

  snap.mSnapPositionY.AppendElement(0 * AppUnitsPerCSSPixel());
  snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());

  ScrollMetadata metadata = root->GetScrollMetadata(0);
  metadata.SetSnapInfo(ScrollSnapInfo(snap));
  root->SetScrollMetadata(metadata);

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, root, mcc);
  UpdateHitTestingTree();

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  TimeStamp now = mcc->Time();

  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), now);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 25), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 25), mcc->Time());

  // The velocity should be positive when panning with positive displacement.
  EXPECT_GT(apzc->GetVelocityVector().y, 3.0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // After lifting the fingers, the velocity should still be positive.
  EXPECT_GT(apzc->GetVelocityVector().y, 3.0);

  mcc->AdvanceByMillis(5);

  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 50), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());

  apzc->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      100.0f,
      apzc->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
          .y);
}
