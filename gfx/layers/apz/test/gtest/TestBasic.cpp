/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"

TEST_F(APZCBasicTester, Overzoom) {
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100
  FrameMetrics fm;
  fm.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  fm.SetScrollableRect(CSSRect(0, 0, 125, 150));
  fm.SetVisualScrollOffset(CSSPoint(10, 0));
  fm.SetZoom(CSSToParentLayerScale2D(1.0, 1.0));
  fm.SetIsRootContent(true);
  apzc->SetFrameMetrics(fm);

  MakeApzcZoomable();

  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(50, 50), 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.8f, fm.GetZoom().ToScaleFactor().scale);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(std::abs(fm.GetVisualScrollOffset().x), 1e-5);
  EXPECT_LT(std::abs(fm.GetVisualScrollOffset().y), 1e-5);
}

TEST_F(APZCBasicTester, SimpleTransform) {
  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

  EXPECT_EQ(ParentLayerPoint(), pointOut);
  EXPECT_EQ(AsyncTransform(), viewTransformOut);
}

TEST_F(APZCBasicTester, ComplexTransform) {
  // This test assumes there is a page that gets rendered to
  // two layers. In CSS pixels, the first layer is 50x50 and
  // the second layer is 25x50. The widget scale factor is 3.0
  // and the presShell resolution is 2.0. Therefore, these layers
  // end up being 300x300 and 150x300 in layer pixels.
  //
  // The second (child) layer has an additional CSS transform that
  // stretches it by 2.0 on the x-axis. Therefore, after applying
  // CSS transforms, the two layers are the same size in screen
  // pixels.
  //
  // The screen itself is 24x24 in screen pixels (therefore 4x4 in
  // CSS pixels). The displayport is 1 extra CSS pixel on all
  // sides.

  RefPtr<TestAsyncPanZoomController> childApzc =
      new TestAsyncPanZoomController(LayersId{0}, mcc, tm);

  const char* layerTreeSyntax = "c(c)";
  // LayerID                     0 1
  nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, 300, 300)),
      nsIntRegion(IntRect(0, 0, 150, 300)),
  };
  Matrix4x4 transforms[] = {
      Matrix4x4(),
      Matrix4x4(),
  };
  transforms[0].PostScale(
      0.5f, 0.5f,
      1.0f);  // this results from the 2.0 resolution on the root layer
  transforms[1].PostScale(
      2.0f, 1.0f,
      1.0f);  // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion,
                                       transforms, lm, layers);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 24, 24));
  metrics.SetDisplayPort(CSSRect(-1, -1, 6, 6));
  metrics.SetVisualScrollOffset(CSSPoint(10, 10));
  metrics.SetLayoutViewport(CSSRect(10, 10, 8, 8));
  metrics.SetScrollableRect(CSSRect(0, 0, 50, 50));
  metrics.SetCumulativeResolution(LayoutDeviceToLayerScale2D(2, 2));
  metrics.SetPresShellResolution(2.0f);
  metrics.SetZoom(CSSToParentLayerScale2D(6, 6));
  metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(3));
  metrics.SetScrollId(ScrollableLayerGuid::START_SCROLL_ID);

  ScrollMetadata childMetadata = metadata;
  FrameMetrics& childMetrics = childMetadata.GetMetrics();
  childMetrics.SetScrollId(ScrollableLayerGuid::START_SCROLL_ID + 1);

  layers[0]->SetScrollMetadata(metadata);
  layers[1]->SetScrollMetadata(childMetadata);

  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here,
  // because the CSS transform on the child layer does not affect the
  // SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metadata, true, true);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint()),
            viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetadata, true, true);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint()),
            viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.ScrollBy(CSSPoint(5, 0));
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(
      AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)),
      viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  childMetrics.ScrollBy(CSSPoint(5, 0));
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(
      AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)),
      viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.ZoomBy(1.5f);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(
      AsyncTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)),
      viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childMetrics.ZoomBy(1.5f);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(
      AsyncTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)),
      viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childApzc->Destroy();
}

TEST_F(APZCBasicTester, Fling) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  int touchStart = 50;
  int touchEnd = 10;
  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // Fling down. Each step scroll further down
  Pan(apzc, touchStart, touchEnd);
  ParentLayerPoint lastPoint;
  for (int i = 1; i < 50; i += 1) {
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut,
                                         TimeDuration::FromMilliseconds(1));
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
TEST_F(APZCBasicTester, ResumeInterruptedTouchDrag_Bug1592435) {
  // Start a touch-drag and scroll some amount, not lifting the finger.
  SCOPED_GFX_PREF_FLOAT("apz.touch_start_tolerance", 1.0f / 1000.0f);
  ScreenIntPoint touchPos(10, 50);
  uint64_t touchBlock = TouchDown(apzc, touchPos, mcc->Time()).mInputBlockId;
  SetDefaultAllowedTouchBehavior(apzc, touchBlock);
  for (int i = 0; i < 20; ++i) {
    touchPos.y -= 1;
    mcc->AdvanceByMillis(1);
    TouchMove(apzc, touchPos, mcc->Time());
  }

  // Take note of the scroll offset before the interruption.
  CSSPoint scrollOffsetBeforeInterruption =
      apzc->GetFrameMetrics().GetVisualScrollOffset();

  // Have the main thread interrupt the touch-drag by sending
  // a main thread scroll update to a nearby location.
  CSSPoint mainThreadOffset = scrollOffsetBeforeInterruption;
  mainThreadOffset.y -= 5;
  ScrollMetadata metadata = apzc->GetScrollMetadata();
  metadata.GetMetrics().SetLayoutScrollOffset(mainThreadOffset);
  nsTArray<ScrollPositionUpdate> scrollUpdates;
  scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
      ScrollOrigin::Other, CSSPoint::ToAppUnits(mainThreadOffset)));
  metadata.SetScrollUpdates(scrollUpdates);
  metadata.GetMetrics().SetScrollGeneration(
      scrollUpdates.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(metadata, false, true);

  // Continue and finish the touch-drag gesture.
  for (int i = 0; i < 20; ++i) {
    touchPos.y -= 1;
    mcc->AdvanceByMillis(1);
    TouchMove(apzc, touchPos, mcc->Time());
  }

  // Check that the portion of the touch-drag that occurred after
  // the interruption caused additional scrolling.
  CSSPoint finalScrollOffset = apzc->GetFrameMetrics().GetVisualScrollOffset();
  EXPECT_GT(finalScrollOffset.y, scrollOffsetBeforeInterruption.y);

  // Now do the same thing, but for a visual scroll update.
  scrollOffsetBeforeInterruption =
      apzc->GetFrameMetrics().GetVisualScrollOffset();
  mainThreadOffset = scrollOffsetBeforeInterruption;
  mainThreadOffset.y -= 5;
  metadata = apzc->GetScrollMetadata();
  metadata.GetMetrics().SetVisualDestination(mainThreadOffset);
  metadata.GetMetrics().SetScrollGeneration(ScrollGeneration::New());
  metadata.GetMetrics().SetVisualScrollUpdateType(FrameMetrics::eMainThread);
  scrollUpdates.Clear();
  metadata.SetScrollUpdates(scrollUpdates);
  apzc->NotifyLayersUpdated(metadata, false, true);
  for (int i = 0; i < 20; ++i) {
    touchPos.y -= 1;
    mcc->AdvanceByMillis(1);
    TouchMove(apzc, touchPos, mcc->Time());
  }
  finalScrollOffset = apzc->GetFrameMetrics().GetVisualScrollOffset();
  EXPECT_GT(finalScrollOffset.y, scrollOffsetBeforeInterruption.y);

  // Clean up by ending the touch gesture.
  mcc->AdvanceByMillis(1);
  TouchUp(apzc, touchPos, mcc->Time());
}
#endif

TEST_F(APZCBasicTester, RelativeScrollOffset) {
  // Set up initial conditions: zoomed in, layout offset at (100, 100),
  // visual offset at (120, 120); the relative offset is therefore (20, 20).
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 1000));
  metrics.SetLayoutViewport(CSSRect(100, 100, 100, 100));
  metrics.SetZoom(CSSToParentLayerScale2D(2.0, 2.0));
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetVisualScrollOffset(CSSPoint(120, 120));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  // Scroll the layout viewport to (200, 200).
  ScrollMetadata mainThreadMetadata = metadata;
  FrameMetrics& mainThreadMetrics = mainThreadMetadata.GetMetrics();
  mainThreadMetrics.SetLayoutScrollOffset(CSSPoint(200, 200));
  nsTArray<ScrollPositionUpdate> scrollUpdates;
  scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
      ScrollOrigin::Other, CSSPoint::ToAppUnits(CSSPoint(200, 200))));
  mainThreadMetadata.SetScrollUpdates(scrollUpdates);
  mainThreadMetrics.SetScrollGeneration(
      scrollUpdates.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(mainThreadMetadata, /*isFirstPaint=*/false,
                            /*thisLayerTreeUpdated=*/true);

  // Check that the relative offset has been preserved.
  metrics = apzc->GetFrameMetrics();
  EXPECT_EQ(metrics.GetLayoutScrollOffset(), CSSPoint(200, 200));
  EXPECT_EQ(metrics.GetVisualScrollOffset(), CSSPoint(220, 220));
}

TEST_F(APZCBasicTester, MultipleSmoothScrollsSmooth) {
  // We want to test that if we send multiple smooth scroll requests that we
  // still smoothly animate, ie that we get non-zero change every frame while
  // the animation is running.

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 10000));
  metrics.SetLayoutViewport(CSSRect(0, 0, 100, 100));
  metrics.SetZoom(CSSToParentLayerScale2D(1.0, 1.0));
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  // Structure of this test.
  //   -send a pure relative smooth scroll request via NotifyLayersUpdated
  //   -advance animations a few times, check that scroll offset is increasing
  //    after the first few advances
  //   -send a pure relative smooth scroll request via NotifyLayersUpdated
  //   -advance animations a few times, check that scroll offset is increasing
  //   -send a pure relative smooth scroll request via NotifyLayersUpdated
  //   -advance animations a few times, check that scroll offset is increasing

  ScrollMetadata metadata2 = metadata;
  nsTArray<ScrollPositionUpdate> scrollUpdates2;
  scrollUpdates2.AppendElement(ScrollPositionUpdate::NewPureRelativeScroll(
      ScrollOrigin::Other, ScrollMode::Smooth,
      CSSPoint::ToAppUnits(CSSPoint(0, 200))));
  metadata2.SetScrollUpdates(scrollUpdates2);
  metadata2.GetMetrics().SetScrollGeneration(
      scrollUpdates2.LastElement().GetGeneration());
  apzc->NotifyLayersUpdated(metadata2, /*isFirstPaint=*/false,
                            /*thisLayerTreeUpdated=*/true);

  // Get the animation going
  for (uint32_t i = 0; i < 3; i++) {
    SampleAnimationOneFrame();
  }

  float offset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForCompositing)
          .y;
  ASSERT_GT(offset, 0);
  float lastOffset = offset;

  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 3; j++) {
      SampleAnimationOneFrame();
      offset = apzc->GetCurrentAsyncScrollOffset(
                       AsyncPanZoomController::eForCompositing)
                   .y;
      ASSERT_GT(offset, lastOffset);
      lastOffset = offset;
    }

    ScrollMetadata metadata3 = metadata;
    nsTArray<ScrollPositionUpdate> scrollUpdates3;
    scrollUpdates3.AppendElement(ScrollPositionUpdate::NewPureRelativeScroll(
        ScrollOrigin::Other, ScrollMode::Smooth,
        CSSPoint::ToAppUnits(CSSPoint(0, 200))));
    metadata3.SetScrollUpdates(scrollUpdates3);
    metadata3.GetMetrics().SetScrollGeneration(
        scrollUpdates3.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata3, /*isFirstPaint=*/false,
                              /*thisLayerTreeUpdated=*/true);
  }

  for (uint32_t j = 0; j < 7; j++) {
    SampleAnimationOneFrame();
    offset = apzc->GetCurrentAsyncScrollOffset(
                     AsyncPanZoomController::eForCompositing)
                 .y;
    ASSERT_GT(offset, lastOffset);
    lastOffset = offset;
  }
}

TEST_F(APZCBasicTester, ZoomAndScrollableRectChangeAfterZoomChange) {
  // We want to check that a small scrollable rect change (which causes us to
  // reclamp our scroll position, including in the sampled state) does not move
  // the scroll offset in the sample state based the zoom in the apzc, only
  // based on the zoom in the sampled state.

  // First we zoom in to the right hand side. Then start zooming out, then send
  // a scrollable rect change and check that it doesn't change the sampled state
  // scroll offset.

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  metrics.SetLayoutViewport(CSSRect(0, 0, 100, 100));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  metrics.SetZoom(CSSToParentLayerScale2D(1.0, 1.0));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  MakeApzcZoomable();

  // Zoom to right side.
  ZoomTarget zoomTarget{CSSRect(75, 25, 25, 25), Nothing()};
  apzc->ZoomToRect(zoomTarget, 0);

  // Run the animation to completion, should take 250ms/16.67ms = 15 frames, but
  // do extra to make sure.
  for (uint32_t i = 0; i < 30; i++) {
    SampleAnimationOneFrame();
  }

  EXPECT_FALSE(apzc->IsAsyncZooming());

  // Zoom out.
  ZoomTarget zoomTarget2{CSSRect(0, 0, 100, 100), Nothing()};
  apzc->ZoomToRect(zoomTarget2, 0);

  // Run the animation a few times to get it going.
  for (uint32_t i = 0; i < 2; i++) {
    SampleAnimationOneFrame();
  }

  // Check that it is decreasing in scale.
  float prevScale =
      apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
          .scale;
  for (uint32_t i = 0; i < 2; i++) {
    SampleAnimationOneFrame();
    float scale =
        apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
            .scale;
    ASSERT_GT(prevScale, scale);
    prevScale = scale;
  }

  float offset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForCompositing)
          .x;

  // Change the scrollable rect slightly to trigger a reclamp.
  ScrollMetadata metadata2 = metadata;
  metadata2.GetMetrics().SetScrollableRect(CSSRect(0, 0, 100, 1000.2));
  apzc->NotifyLayersUpdated(metadata2, /*isFirstPaint=*/false,
                            /*thisLayerTreeUpdated=*/true);

  float newOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForCompositing)
          .x;

  ASSERT_EQ(newOffset, offset);
}

TEST_F(APZCBasicTester, ZoomToRectAndCompositionBoundsChange) {
  // We want to check that content sending a composition bounds change (due to
  // addition of scrollbars) during a zoom animation does not cause us to take
  // the out of date content resolution.

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetCompositionBoundsWidthIgnoringScrollbars(ParentLayerCoord{100});
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  metrics.SetLayoutViewport(CSSRect(0, 0, 100, 100));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  metrics.SetZoom(CSSToParentLayerScale2D(1.0, 1.0));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  MakeApzcZoomable();

  // Start a zoom to a rect.
  ZoomTarget zoomTarget{CSSRect(25, 25, 25, 25), Nothing()};
  apzc->ZoomToRect(zoomTarget, 0);

  // Run the animation a few times to get it going.
  // Check that it is increasing in scale.
  float prevScale =
      apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
          .scale;
  for (uint32_t i = 0; i < 3; i++) {
    SampleAnimationOneFrame();
    float scale =
        apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
            .scale;
    ASSERT_GE(scale, prevScale);
    prevScale = scale;
  }

  EXPECT_TRUE(apzc->IsAsyncZooming());

  // Simulate the appearance of a scrollbar by reducing the width of
  // the composition bounds, while keeping
  // mCompositionBoundsWidthIgnoringScrollbars unchanged.
  ScrollMetadata metadata2 = metadata;
  metadata2.GetMetrics().SetCompositionBounds(ParentLayerRect(0, 0, 90, 100));
  apzc->NotifyLayersUpdated(metadata2, /*isFirstPaint=*/false,
                            /*thisLayerTreeUpdated=*/true);

  float scale =
      apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
          .scale;

  ASSERT_EQ(scale, prevScale);

  // Run the rest of the animation to completion, should take 250ms/16.67ms = 15
  // frames total, but do extra to make sure.
  for (uint32_t i = 0; i < 30; i++) {
    SampleAnimationOneFrame();
    scale =
        apzc->GetCurrentPinchZoomScale(AsyncPanZoomController::eForCompositing)
            .scale;
    ASSERT_GE(scale, prevScale);
    prevScale = scale;
  }

  EXPECT_FALSE(apzc->IsAsyncZooming());
}
