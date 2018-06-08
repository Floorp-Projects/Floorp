/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "gfxPrefs.h"
#include "InputUtils.h"

class APZCSnappingTester : public APZCTreeManagerTester
{
};

TEST_F(APZCSnappingTester, Bug1265510)
{
  SCOPED_GFX_VAR(UseWebRender, bool, false);

  const char* layerTreeSyntax = "c(t)";
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(IntRect(0, 0, 100, 100)),
    nsIntRegion(IntRect(0, 100, 100, 100))
  };
  root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 200));
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 200));
  SetScrollHandoff(layers[1], root);

  ScrollSnapInfo snap;
  snap.mScrollSnapTypeY = NS_STYLE_SCROLL_SNAP_TYPE_MANDATORY;
  snap.mScrollSnapIntervalY = Some(100 * AppUnitsPerCSSPixel());

  ScrollMetadata metadata = root->GetScrollMetadata(0);
  metadata.SetSnapInfo(ScrollSnapInfo(snap));
  root->SetScrollMetadata(metadata);

  UniquePtr<ScopedLayerTreeRegistration> registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
  manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);

  TestAsyncPanZoomController* outer = ApzcOf(layers[0]);
  TestAsyncPanZoomController* inner = ApzcOf(layers[1]);

  // Position the mouse near the bottom of the outer frame and scroll by 60px.
  // (6 lines of 10px each). APZC will actually scroll to y=100 because of the
  // mandatory snap coordinate there.
  TimeStamp now = mcc->Time();
  SmoothWheel(manager, ScreenIntPoint(50, 80), ScreenPoint(0, 6), now);
  // Advance in 5ms increments until we've scrolled by 70px. At this point, the
  // closest snap point is y=100, and the inner frame should be under the mouse
  // cursor.
  while (outer->GetCurrentAsyncScrollOffset(AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting).y < 70) {
    mcc->AdvanceByMillis(5);
    outer->AdvanceAnimations(mcc->Time());
  }
  // Now do another wheel in a new transaction. This should start scrolling the
  // inner frame; we verify that it does by checking the inner scroll position.
  TimeStamp newTransactionTime = now + TimeDuration::FromMilliseconds(gfxPrefs::MouseWheelTransactionTimeoutMs() + 100);
  SmoothWheel(manager, ScreenIntPoint(50, 80), ScreenPoint(0, 6), newTransactionTime);
  inner->AdvanceAnimationsUntilEnd();
  EXPECT_LT(0.0f, inner->GetCurrentAsyncScrollOffset(AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting).y);

  // However, the outer frame should also continue to the snap point, otherwise
  // it is demonstrating incorrect behaviour by violating the mandatory snapping.
  outer->AdvanceAnimationsUntilEnd();
  EXPECT_EQ(100.0f, outer->GetCurrentAsyncScrollOffset(AsyncPanZoomController::AsyncTransformConsumer::eForHitTesting).y);
}

TEST_F(APZCSnappingTester, Snap_After_Pinch)
{
  SCOPED_GFX_VAR(UseWebRender, bool, false);

  const char* layerTreeSyntax = "c";
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 200));

  // Set up some basic scroll snapping
  ScrollSnapInfo snap;
  snap.mScrollSnapTypeY = NS_STYLE_SCROLL_SNAP_TYPE_MANDATORY;
  snap.mScrollSnapIntervalY = Some(100 * AppUnitsPerCSSPixel());

  // Save the scroll snap info on the root APZC.
  // Also mark the root APZC as "root content", since APZC only allows
  // zooming on the root content APZC.
  ScrollMetadata metadata = root->GetScrollMetadata(0);
  metadata.SetSnapInfo(ScrollSnapInfo(snap));
  metadata.GetMetrics().SetIsRootContent(true);
  root->SetScrollMetadata(metadata);

  UniquePtr<ScopedLayerTreeRegistration> registration = MakeUnique<ScopedLayerTreeRegistration>(manager, LayersId{0}, root, mcc);
  manager->UpdateHitTestingTree(LayersId{0}, root, false, LayersId{0}, 0);

  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // Allow zooming
  apzc->UpdateZoomConstraints(ZoomConstraints(true, true, CSSToParentLayerScale(0.25f), CSSToParentLayerScale(4.0f)));

  PinchWithPinchInput(apzc, ScreenIntPoint(50, 50), ScreenIntPoint(50, 50), 1.2f);

  apzc->AssertStateIsSmoothScroll();
}
