/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZTestCommon.h"

#include "InputUtils.h"

static ScrollGenerationCounter sGenerationCounter;

TEST_F(APZCBasicTester, Overzoom) {
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100
  FrameMetrics fm;
  fm.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  fm.SetScrollableRect(CSSRect(0, 0, 125, 150));
  fm.SetVisualScrollOffset(CSSPoint(10, 0));
  fm.SetZoom(CSSToParentLayerScale(1.0));
  fm.SetIsRootContent(true);
  apzc->SetFrameMetrics(fm);

  MakeApzcZoomable();

  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(50, 50), 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.8f, fm.GetZoom().scale);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(std::abs(fm.GetVisualScrollOffset().x), 1e-5);
  EXPECT_LT(std::abs(fm.GetVisualScrollOffset().y), 1e-5);
}

TEST_F(APZCBasicTester, ZoomLimits) {
  SCOPED_GFX_PREF_FLOAT("apz.min_zoom", 0.9f);
  SCOPED_GFX_PREF_FLOAT("apz.max_zoom", 2.0f);

  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100
  FrameMetrics fm;
  fm.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  fm.SetScrollableRect(CSSRect(0, 0, 125, 150));
  fm.SetZoom(CSSToParentLayerScale(1.0));
  fm.SetIsRootContent(true);
  apzc->SetFrameMetrics(fm);

  MakeApzcZoomable();

  // This should take the zoom scale to 0.8, but we've capped it at 0.9.
  PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(50, 50), 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.9f, fm.GetZoom().scale);

  // This should take the zoom scale to 2.7, but we've capped it at 2.
  PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(50, 50), 3, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(2.0f, fm.GetZoom().scale);
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

  const char* treeShape = "x(x)";
  // LayerID                     0 1
  LayerIntRect layerVisibleRect[] = {
      LayerIntRect(0, 0, 300, 300),
      LayerIntRect(0, 0, 150, 300),
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

  auto layers = TestWRScrollData::Create(treeShape, *updater, layerVisibleRect,
                                         transforms);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 24, 24));
  metrics.SetDisplayPort(CSSRect(-1, -1, 6, 6));
  metrics.SetVisualScrollOffset(CSSPoint(10, 10));
  metrics.SetLayoutViewport(CSSRect(10, 10, 8, 8));
  metrics.SetScrollableRect(CSSRect(0, 0, 50, 50));
  metrics.SetCumulativeResolution(LayoutDeviceToLayerScale(2));
  metrics.SetPresShellResolution(2.0f);
  metrics.SetZoom(CSSToParentLayerScale(6));
  metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(3));
  metrics.SetScrollId(ScrollableLayerGuid::START_SCROLL_ID);

  ScrollMetadata childMetadata = metadata;
  FrameMetrics& childMetrics = childMetadata.GetMetrics();
  childMetrics.SetScrollId(ScrollableLayerGuid::START_SCROLL_ID + 1);

  layers[0]->AppendScrollMetadata(layers, metadata);
  layers[1]->AppendScrollMetadata(layers, childMetadata);

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
  metadata.GetMetrics().SetScrollGeneration(
      sGenerationCounter.NewMainThreadGeneration());
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
  metrics.SetZoom(CSSToParentLayerScale(2.0));
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
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  // We want to test that if we send multiple smooth scroll requests that we
  // still smoothly animate, ie that we get non-zero change every frame while
  // the animation is running.

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 10000));
  metrics.SetLayoutViewport(CSSRect(0, 0, 100, 100));
  metrics.SetZoom(CSSToParentLayerScale(1.0));
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

class APZCSmoothScrollTester : public APZCBasicTester {
 public:
  // Test that a smooth scroll animation correctly handles its destination
  // being updated by a relative scroll delta from the main thread (a "content
  // shift").
  void TestContentShift() {
    // Set up scroll frame. Starting scroll position is (0, 0).
    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetScrollableRect(CSSRect(0, 0, 100, 10000));
    metrics.SetLayoutViewport(CSSRect(0, 0, 100, 100));
    metrics.SetZoom(CSSToParentLayerScale(1.0));
    metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
    metrics.SetVisualScrollOffset(CSSPoint(0, 0));
    metrics.SetIsRootContent(true);
    apzc->SetFrameMetrics(metrics);

    // Start smooth scroll via main-thread request.
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewPureRelativeScroll(
        ScrollOrigin::Other, ScrollMode::Smooth,
        CSSPoint::ToAppUnits(CSSPoint(0, 1000))));
    metadata.SetScrollUpdates(scrollUpdates);
    metrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata, false, true);

    // Sample the smooth scroll animation until we get past y=500.
    apzc->AssertStateIsSmoothScroll();
    float y = 0;
    while (y < 500) {
      SampleAnimationOneFrame();
      y = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    }

    // Send a relative scroll of y = -400.
    scrollUpdates.Clear();
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewRelativeScroll(
        CSSPoint::ToAppUnits(CSSPoint(0, 500)),
        CSSPoint::ToAppUnits(CSSPoint(0, 100))));
    metadata.SetScrollUpdates(scrollUpdates);
    metrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata, false, false);

    // Verify the relative scroll was applied but didn't cancel the animation.
    float y2 = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_EQ(y2, y - 400);
    apzc->AssertStateIsSmoothScroll();

    // Sample the animation again and check that it respected the relative
    // scroll.
    SampleAnimationOneFrame();
    float y3 = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_GT(y3, y2);
    ASSERT_LT(y3, 500);

    // Continue animation until done and check that it ended up at a correctly
    // adjusted destination.
    apzc->AdvanceAnimationsUntilEnd();
    float y4 = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_EQ(y4, 600);  // 1000 (initial destination) - 400 (relative scroll)
  }

  // Test that a smooth scroll animation correctly handles a content
  // shift, followed by an UpdateDelta due to a new input event.
  void TestContentShiftThenUpdateDelta() {
    // Set up scroll frame. Starting position is (0, 0).
    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetScrollableRect(CSSRect(0, 0, 1000, 10000));
    metrics.SetLayoutViewport(CSSRect(0, 0, 1000, 1000));
    metrics.SetZoom(CSSToParentLayerScale(1.0));
    metrics.SetCompositionBounds(ParentLayerRect(0, 0, 1000, 1000));
    metrics.SetVisualScrollOffset(CSSPoint(0, 0));
    metrics.SetIsRootContent(true);
    // Set the line scroll amount to 100 pixels. Note that SmoothWheel() takes
    // a delta denominated in lines.
    metadata.SetLineScrollAmount({100, 100});
    // The page scroll amount also needs to be set, otherwise the wheel handling
    // code will get confused by things like the "don't scroll more than one
    // page" check.
    metadata.SetPageScrollAmount({1000, 1000});
    apzc->SetScrollMetadata(metadata);

    // Send a wheel event to trigger smooth scrolling by 5 lines (= 500 pixels).
    SmoothWheel(apzc, ScreenIntPoint(50, 50), ScreenPoint(0, 5), mcc->Time());
    apzc->AssertStateIsWheelScroll();

    // Sample the wheel scroll animation until we get past y=200.
    float y = 0;
    while (y < 200) {
      SampleAnimationOneFrame();
      y = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    }

    // Apply a content shift of y=100.
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewRelativeScroll(
        CSSPoint::ToAppUnits(CSSPoint(0, 200)),
        CSSPoint::ToAppUnits(CSSPoint(0, 300))));
    metadata.SetScrollUpdates(scrollUpdates);
    metrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata, false, true);

    // Check that the content shift was applied but didn't cancel the animation.
    // At this point, the animation's internal state should be targeting a
    // destination of y=600.
    float y2 = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_EQ(y2, y + 100);
    apzc->AssertStateIsWheelScroll();

    // Sample the animation until we get past y=400.
    while (y < 400) {
      SampleAnimationOneFrame();
      y = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    }

    // Send another wheel event to trigger smooth scrolling by another 5 lines
    // (=500 pixels). This should update the animation to target a destination
    // of y=1100.
    SmoothWheel(apzc, ScreenIntPoint(50, 50), ScreenPoint(0, 5), mcc->Time());

    // Continue the animation until done and check that it ended up at y=1100.
    apzc->AdvanceAnimationsUntilEnd();
    float yEnd = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_EQ(yEnd, 1100);
  }

  // Test that a content shift does not cause a smooth scroll animation to
  // overshoot its (updated) destination.
  void TestContentShiftDoesNotCauseOvershoot() {
    // Follow the same steps as in TestContentShiftThenUpdateDelta(),
    // except use a content shift of y=1000.
    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetScrollableRect(CSSRect(0, 0, 1000, 10000));
    metrics.SetLayoutViewport(CSSRect(0, 0, 1000, 1000));
    metrics.SetZoom(CSSToParentLayerScale(1.0));
    metrics.SetCompositionBounds(ParentLayerRect(0, 0, 1000, 1000));
    metrics.SetVisualScrollOffset(CSSPoint(0, 0));
    metrics.SetIsRootContent(true);
    metadata.SetLineScrollAmount({100, 100});
    metadata.SetPageScrollAmount({1000, 1000});
    apzc->SetScrollMetadata(metadata);

    // First wheel event, smooth scroll destination is y=500.
    SmoothWheel(apzc, ScreenIntPoint(50, 50), ScreenPoint(0, 5), mcc->Time());
    apzc->AssertStateIsWheelScroll();

    // Sample until we get past y=200.
    float y = 0;
    while (y < 200) {
      SampleAnimationOneFrame();
      y = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    }

    // Apply a content shift of y=1000. The current scroll position is now
    // y>1200, and the updated destination is y=1500.
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewRelativeScroll(
        CSSPoint::ToAppUnits(CSSPoint(0, 200)),
        CSSPoint::ToAppUnits(CSSPoint(0, 1200))));
    metadata.SetScrollUpdates(scrollUpdates);
    metrics.SetScrollGeneration(scrollUpdates.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata, false, true);
    float y2 = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    ASSERT_EQ(y2, y + 1000);
    apzc->AssertStateIsWheelScroll();

    // Sample until we get past y=1300.
    while (y < 1300) {
      SampleAnimationOneFrame();
      y = apzc->GetFrameMetrics().GetVisualScrollOffset().y;
    }

    // Second wheel event, destination is now y=2000.
    // MSD physics has a bug where the UpdateDelta() causes the content shift
    // to be applied in duplicate on the next sample, causing the scroll
    // position to be y>2000!
    SmoothWheel(apzc, ScreenIntPoint(50, 50), ScreenPoint(0, 5), mcc->Time());

    // Check that the scroll position remains <= 2000 until the end of the
    // animation.
    while (apzc->IsWheelScrollAnimationRunning()) {
      SampleAnimationOneFrame();
      ASSERT_LE(apzc->GetFrameMetrics().GetVisualScrollOffset().y, 2000);
    }
    ASSERT_EQ(2000, apzc->GetFrameMetrics().GetVisualScrollOffset().y);
  }
};

TEST_F(APZCSmoothScrollTester, ContentShiftBezier) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", false);
  TestContentShift();
}

TEST_F(APZCSmoothScrollTester, ContentShiftMsd) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", true);
  TestContentShift();
}

TEST_F(APZCSmoothScrollTester, ContentShiftThenUpdateDeltaBezier) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", false);
  TestContentShiftThenUpdateDelta();
}

TEST_F(APZCSmoothScrollTester, ContentShiftThenUpdateDeltaMsd) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", true);
  TestContentShiftThenUpdateDelta();
}

TEST_F(APZCSmoothScrollTester, ContentShiftDoesNotCauseOvershootBezier) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", false);
  TestContentShiftDoesNotCauseOvershoot();
}

TEST_F(APZCSmoothScrollTester, ContentShiftDoesNotCauseOvershootMsd) {
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  SCOPED_GFX_PREF_BOOL("general.smoothScroll.msdPhysics.enabled", true);
  TestContentShiftDoesNotCauseOvershoot();
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
  metrics.SetZoom(CSSToParentLayerScale(1.0));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  MakeApzcZoomable();

  // Zoom to right side.
  ZoomTarget zoomTarget{CSSRect(75, 25, 25, 25)};
  apzc->ZoomToRect(zoomTarget, 0);

  // Run the animation to completion, should take 250ms/16.67ms = 15 frames, but
  // do extra to make sure.
  for (uint32_t i = 0; i < 30; i++) {
    SampleAnimationOneFrame();
  }

  EXPECT_FALSE(apzc->IsAsyncZooming());

  // Zoom out.
  ZoomTarget zoomTarget2{CSSRect(0, 0, 100, 100)};
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
  metrics.SetZoom(CSSToParentLayerScale(1.0));
  metrics.SetIsRootContent(true);
  apzc->SetFrameMetrics(metrics);

  MakeApzcZoomable();

  // Start a zoom to a rect.
  ZoomTarget zoomTarget{CSSRect(25, 25, 25, 25)};
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

TEST_F(APZCBasicTester, StartTolerance) {
  SCOPED_GFX_PREF_FLOAT("apz.touch_start_tolerance", 10 / tm->GetDPI());

  FrameMetrics fm;
  fm.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  fm.SetScrollableRect(CSSRect(0, 0, 100, 300));
  fm.SetVisualScrollOffset(CSSPoint(0, 50));
  fm.SetIsRootContent(true);
  apzc->SetFrameMetrics(fm);

  uint64_t touchBlock = TouchDown(apzc, {50, 50}, mcc->Time()).mInputBlockId;
  SetDefaultAllowedTouchBehavior(apzc, touchBlock);

  CSSPoint initialScrollOffset =
      apzc->GetFrameMetrics().GetVisualScrollOffset();

  mcc->AdvanceByMillis(1);
  TouchMove(apzc, {50, 70}, mcc->Time());

  // Expect 10 pixels of scrolling: the distance from (50,50) to (50,70)
  // minus the 10-pixel touch start tolerance.
  ASSERT_EQ(initialScrollOffset.y - 10,
            apzc->GetFrameMetrics().GetVisualScrollOffset().y);

  mcc->AdvanceByMillis(1);
  TouchMove(apzc, {50, 90}, mcc->Time());

  // Expect 30 pixels of scrolling: the distance from (50,50) to (50,90)
  // minus the 10-pixel touch start tolerance.
  ASSERT_EQ(initialScrollOffset.y - 30,
            apzc->GetFrameMetrics().GetVisualScrollOffset().y);

  // Clean up by ending the touch gesture.
  mcc->AdvanceByMillis(1);
  TouchUp(apzc, {50, 90}, mcc->Time());
}
