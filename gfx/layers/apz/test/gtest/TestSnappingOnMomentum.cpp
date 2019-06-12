/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"
#include "mozilla/StaticPrefs.h"

class APZCSnappingOnMomentumTester : public APZCTreeManagerTester {};

TEST_F(APZCSnappingOnMomentumTester, Snap_On_Momentum) {
  SCOPED_GFX_VAR(UseWebRender, bool, false);

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
  snap.mScrollSnapTypeY = StyleScrollSnapStrictness::Mandatory;

  if (StaticPrefs::layout_css_scroll_snap_v1_enabled()) {
    snap.mSnapPositionY.AppendElement(0 * AppUnitsPerCSSPixel());
    snap.mSnapPositionY.AppendElement(100 * AppUnitsPerCSSPixel());
  } else {
    snap.mScrollSnapIntervalY = Some(100 * AppUnitsPerCSSPixel());
  }

  ScrollMetadata metadata = root->GetScrollMetadata(0);
  metadata.SetSnapInfo(ScrollSnapInfo(snap));
  root->SetScrollMetadata(metadata);

  UniquePtr<ScopedLayerTreeRegistration> registration =
      MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
  manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  TimeStamp now = mcc->Time();

  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), now);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->Time());
  PanGesture(PanGestureInput::PANGESTURE_PAN, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 50), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->Time());
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);

  apzc->AdvanceAnimations(mcc->Time());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->Time());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 50), mcc->Time());
  mcc->AdvanceByMillis(10);
  apzc->AdvanceAnimations(mcc->Time());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());

  apzc->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(
      100.0f,
      apzc->GetCurrentAsyncScrollOffset(
              AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting)
          .y);
}
