/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
  fm.SetScrollOffset(CSSPoint(10, 0));
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
  EXPECT_LT(std::abs(fm.GetScrollOffset().x), 1e-5);
  EXPECT_LT(std::abs(fm.GetScrollOffset().y), 1e-5);
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
      new TestAsyncPanZoomController(0, mcc, tm);

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
  transforms[0].PostScale(0.5f, 0.5f, 1.0f); // this results from the 2.0 resolution on the root layer
  transforms[1].PostScale(2.0f, 1.0f, 1.0f); // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 24, 24));
  metrics.SetDisplayPort(CSSRect(-1, -1, 6, 6));
  metrics.SetScrollOffset(CSSPoint(10, 10));
  metrics.SetScrollableRect(CSSRect(0, 0, 50, 50));
  metrics.SetCumulativeResolution(LayoutDeviceToLayerScale2D(2, 2));
  metrics.SetPresShellResolution(2.0f);
  metrics.SetZoom(CSSToParentLayerScale2D(6, 6));
  metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(3));
  metrics.SetScrollId(FrameMetrics::START_SCROLL_ID);

  ScrollMetadata childMetadata = metadata;
  FrameMetrics& childMetrics = childMetadata.GetMetrics();
  childMetrics.SetScrollId(FrameMetrics::START_SCROLL_ID + 1);

  layers[0]->SetScrollMetadata(metadata);
  layers[1]->SetScrollMetadata(childMetadata);

  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metadata, true, true);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint()), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetadata, true, true);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint()), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.ScrollBy(CSSPoint(5, 0));
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  childMetrics.ScrollBy(CSSPoint(5, 0));
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.ZoomBy(1.5f);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childMetrics.ZoomBy(1.5f);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);
  EXPECT_EQ(AsyncTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childApzc->Destroy();
}

TEST_F(APZCBasicTester, Fling) {
  int touchStart = 50;
  int touchEnd = 10;
  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // Fling down. Each step scroll further down
  Pan(apzc, touchStart, touchEnd);
  ParentLayerPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut, TimeDuration::FromMilliseconds(1));
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

TEST_F(APZCBasicTester, FlingIntoOverscroll) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Scroll down by 25 px. Don't fling for simplicity.
  ApzcPanNoFling(apzc, 50, 25);

  // Now scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  Pan(apzc, 25, 45);
  const TimeDuration increment = TimeDuration::FromMilliseconds(1);
  bool reachedOverscroll = false;
  bool recoveredFromOverscroll = false;
  while (apzc->AdvanceAnimations(mcc->Time())) {
    if (!reachedOverscroll && apzc->IsOverscrolled()) {
      reachedOverscroll = true;
    }
    if (reachedOverscroll && !apzc->IsOverscrolled()) {
      recoveredFromOverscroll = true;
    }
    mcc->AdvanceBy(increment);
  }
  EXPECT_TRUE(reachedOverscroll);
  EXPECT_TRUE(recoveredFromOverscroll);
}

TEST_F(APZCBasicTester, PanningTransformNotifications) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Scroll down by 25 px. Ensure we only get one set of
  // state change notifications.
  //
  // Then, scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  // Again, ensure we only get one set of state change notifications for
  // this entire procedure.

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Simple pan"));
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformBegin,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartPanning,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::EndTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformEnd,_)).Times(1);
    EXPECT_CALL(check, Call("Complex pan"));
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformBegin,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartPanning,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::EndTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformEnd,_)).Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Simple pan");
  ApzcPanNoFling(apzc, 50, 25);
  check.Call("Complex pan");
  Pan(apzc, 25, 45);
  apzc->AdvanceAnimationsUntilEnd();
  check.Call("Done");
}

void APZCBasicTester::PanIntoOverscroll()
{
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());
}

void APZCBasicTester::TestOverscroll()
{
  // Pan sufficiently to hit overscroll behavior
  PanIntoOverscroll();

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}


TEST_F(APZCBasicTester, OverScrollPanning) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  TestOverscroll();
}

// Tests that an overscroll animation doesn't trigger an assertion failure
// in the case where a sample has a velocity of zero.
TEST_F(APZCBasicTester, OverScroll_Bug1152051a) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Doctor the prefs to make the velocity zero at the end of the first sample.

  // This ensures our incoming velocity to the overscroll animation is
  // a round(ish) number, 4.9 (that being the distance of the pan before
  // overscroll, which is 500 - 10 = 490 pixels, divided by the duration of
  // the pan, which is 100 ms).
  SCOPED_GFX_PREF(APZFlingFriction, float, 0);

  // To ensure the velocity after the first sample is 0, set the spring
  // stiffness to the incoming velocity (4.9) divided by the overscroll
  // (400 pixels) times the step duration (1 ms).
  SCOPED_GFX_PREF(APZOverscrollSpringStiffness, float, 0.01225f);

  TestOverscroll();
}

// Tests that ending an overscroll animation doesn't leave around state that
// confuses the next overscroll animation.
TEST_F(APZCBasicTester, OverScroll_Bug1152051b) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  SCOPED_GFX_PREF(APZOverscrollStopDistanceThreshold, float, 0.1f);

  // Pan sufficiently to hit overscroll behavior
  PanIntoOverscroll();

  // Sample animations once, to give the fling animation started on touch-up
  // a chance to realize it's overscrolled, and schedule a call to
  // HandleFlingOverscroll().
  SampleAnimationOnce();

  // This advances the time and runs the HandleFlingOverscroll task scheduled in
  // the previous call, which starts an overscroll animation. It then samples
  // the overscroll animation once, to get it to initialize the first overscroll
  // sample.
  SampleAnimationOnce();

  // Do a touch-down to cancel the overscroll animation, and then a touch-up
  // to schedule a new one since we're still overscrolled. We don't pan because
  // panning can trigger functions that clear the overscroll animation state
  // in other ways.
  TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time(), nullptr);
  TouchUp(apzc, ScreenIntPoint(10, 10), mcc->Time());

  // Sample the second overscroll animation to its end.
  // If the ending of the first overscroll animation fails to clear state
  // properly, this will assert.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}

TEST_F(APZCBasicTester, OverScrollAbort) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behavior
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // This sample call will run to the end of the fling animation
  // and will schedule the overscroll animation.
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut, TimeDuration::FromMilliseconds(10000));
  EXPECT_TRUE(apzc->IsOverscrolled());

  // At this point, we have an active overscroll animation.
  // Check that cancelling the animation clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}

TEST_F(APZCBasicTester, OverScrollPanningAbort) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behaviour. Keep the finger down so
  // the pan does not end.
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd, true); // keep finger down
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that calling CancelAnimation() while the user is still panning
  // (and thus no fling or snap-back animation has had a chance to start)
  // clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}
