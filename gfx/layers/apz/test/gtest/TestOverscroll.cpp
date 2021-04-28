/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"

#include "InputUtils.h"

class APZCOverscrollTester : public APZCBasicTester {
 public:
  explicit APZCOverscrollTester(
      AsyncPanZoomController::GestureBehavior aGestureBehavior =
          AsyncPanZoomController::DEFAULT_GESTURES)
      : APZCBasicTester(aGestureBehavior) {}

 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;

  void TestOverscroll() {
    // Pan sufficiently to hit overscroll behavior
    PanIntoOverscroll();

    // Check that we recover from overscroll via an animation.
    ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
    SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
  }

  void PanIntoOverscroll() {
    int touchStart = 500;
    int touchEnd = 10;
    Pan(apzc, touchStart, touchEnd);
    EXPECT_TRUE(apzc->IsOverscrolled());
  }

  /**
   * Sample animations until we recover from overscroll.
   * @param aExpectedScrollOffset the expected reported scroll offset
   *                              throughout the animation
   */
  void SampleAnimationUntilRecoveredFromOverscroll(
      const ParentLayerPoint& aExpectedScrollOffset) {
    const TimeDuration increment = TimeDuration::FromMilliseconds(1);
    bool recoveredFromOverscroll = false;
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    while (apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut)) {
      // The reported scroll offset should be the same throughout.
      EXPECT_EQ(aExpectedScrollOffset, pointOut);

      // Trigger computation of the overscroll tranform, to make sure
      // no assetions fire during the calculation.
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);

      if (!apzc->IsOverscrolled()) {
        recoveredFromOverscroll = true;
      }

      mcc->AdvanceBy(increment);
    }
    EXPECT_TRUE(recoveredFromOverscroll);
    apzc->AssertStateIsReset();
  }

  ScrollableLayerGuid CreateSimpleRootScrollableForWebRender() {
    ScrollableLayerGuid guid;
    if (!gfx::gfxVars::UseWebRender()) {
      return guid;
    }

    guid.mScrollId = ScrollableLayerGuid::START_SCROLL_ID;
    guid.mLayersId = LayersId{0};

    ScrollMetadata metadata;
    FrameMetrics& metrics = metadata.GetMetrics();
    metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
    metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
    metrics.SetScrollId(guid.mScrollId);
    metadata.SetIsLayersIdRoot(true);

    WebRenderLayerScrollData rootLayerScrollData;
    rootLayerScrollData.InitializeRoot(0);
    WebRenderScrollData scrollData;
    rootLayerScrollData.AppendScrollMetadata(scrollData, metadata);
    scrollData.AddLayerData(rootLayerScrollData);

    registration = MakeUnique<ScopedLayerTreeRegistration>(guid.mLayersId, mcc);
    tm->UpdateHitTestingTree(WebRenderScrollDataWrapper(*updater, &scrollData),
                             false, guid.mLayersId, 0);
    return guid;
  }
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, FlingIntoOverscroll) {
  // Enable overscrolling.
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);

  // Scroll down by 25 px. Don't fling for simplicity.
  Pan(apzc, 50, 25, PanOptions::NoFling);

  // Now scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  Pan(apzc, 25, 45);
  const TimeDuration increment = TimeDuration::FromMilliseconds(1);
  bool reachedOverscroll = false;
  bool recoveredFromOverscroll = false;
  while (apzc->AdvanceAnimations(mcc->GetSampleTime())) {
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
#endif

TEST_F(APZCOverscrollTester, PanningTransformNotifications) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

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
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eStartTouch, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _))
        .Times(1);
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartPanning, _))
        .Times(1);
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, _))
        .Times(1);
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eTransformEnd, _))
        .Times(1);
    EXPECT_CALL(check, Call("Complex pan"));
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eStartTouch, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _))
        .Times(1);
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartPanning, _))
        .Times(1);
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, _))
        .Times(1);
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eTransformEnd, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Simple pan");
  Pan(apzc, 50, 25, PanOptions::NoFling);
  check.Call("Complex pan");
  Pan(apzc, 25, 45);
  apzc->AdvanceAnimationsUntilEnd();
  check.Call("Done");
}

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollPanning) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  TestOverscroll();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that an overscroll animation doesn't trigger an assertion failure
// in the case where a sample has a velocity of zero.
TEST_F(APZCOverscrollTester, OverScroll_Bug1152051a) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Doctor the prefs to make the velocity zero at the end of the first sample.

  // This ensures our incoming velocity to the overscroll animation is
  // a round(ish) number, 4.9 (that being the distance of the pan before
  // overscroll, which is 500 - 10 = 490 pixels, divided by the duration of
  // the pan, which is 100 ms).
  SCOPED_GFX_PREF_FLOAT("apz.fling_friction", 0);

  TestOverscroll();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that ending an overscroll animation doesn't leave around state that
// confuses the next overscroll animation.
TEST_F(APZCOverscrollTester, OverScroll_Bug1152051b) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_FLOAT("apz.overscroll.stop_distance_threshold", 0.1f);

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
  APZEventResult result = TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  if (StaticPrefs::layout_css_touch_action_enabled() &&
      result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, result.mInputBlockId);
  }
  TouchUp(apzc, ScreenIntPoint(10, 10), mcc->Time());

  // Sample the second overscroll animation to its end.
  // If the ending of the first overscroll animation fails to clear state
  // properly, this will assert.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
// Tests that the page doesn't get stuck in an
// overscroll animation after a low-velocity pan.
TEST_F(APZCOverscrollTester, OverScrollAfterLowVelocityPan_Bug1343775) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan into overscroll with a velocity less than the
  // apz.fling_min_velocity_threshold preference.
  Pan(apzc, 10, 30);

  EXPECT_TRUE(apzc->IsOverscrolled());

  apzc->AdvanceAnimationsUntilEnd();

  // Check that we recovered from overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollAbort) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan sufficiently to hit overscroll behavior
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  ParentLayerPoint pointOut;
  AsyncTransform viewTransformOut;

  // This sample call will run to the end of the fling animation
  // and will schedule the overscroll animation.
  apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut,
                                       TimeDuration::FromMilliseconds(10000));
  EXPECT_TRUE(apzc->IsOverscrolled());

  // At this point, we have an active overscroll animation.
  // Check that cancelling the animation clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverScrollPanningAbort) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Pan sufficiently to hit overscroll behaviour. Keep the finger down so
  // the pan does not end.
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, touchStart, touchEnd, PanOptions::KeepFingerDown);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that calling CancelAnimation() while the user is still panning
  // (and thus no fling or snap-back animation has had a chance to start)
  // clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
TEST_F(APZCOverscrollTester, OverscrollByVerticalPanGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByVerticalAndHorizontalPanGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByPanMomentumGestures) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we are not yet in overscrolled region.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());

  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, IgnoreMomemtumDuringOverscroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  float yMost = GetScrollRange().YMost();
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost / 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, yMost / 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // And check the overscrolled transform value before/after calling PanGesture
  // to make sure the overscroll amount isn't affected by momentum events.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_EQ(overscrolledTransform, apzc->GetOverscrollTransform(
                                       AsyncPanZoomController::eForHitTesting));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_EQ(overscrolledTransform, apzc->GetOverscrollTransform(
                                       AsyncPanZoomController::eForHitTesting));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_EQ(overscrolledTransform, apzc->GetOverscrollTransform(
                                       AsyncPanZoomController::eForHitTesting));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 2), mcc->Time());
  EXPECT_EQ(overscrolledTransform, apzc->GetOverscrollTransform(
                                       AsyncPanZoomController::eForHitTesting));

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_EQ(overscrolledTransform, apzc->GetOverscrollTransform(
                                       AsyncPanZoomController::eForHitTesting));

  // Check that we've recovered from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, GetScrollRange().YMost());
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, VerticalOnlyOverscroll) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Make the content scrollable only vertically.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  apzc->SetFrameMetrics(metrics);

  // Scroll up into overscroll a bit.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());
  // Now it's overscrolled.
  EXPECT_TRUE(apzc->IsOverscrolled());
  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // The overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  // Happens only vertically.
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Send pan momentum events including horizontal bits.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-10, -100), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // The overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-5, -50), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -2), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, VerticalOnlyOverscrollByPanMomentum) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Make the content scrollable only vertically.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Scrolls the content down a bit.
  metrics.SetVisualScrollOffset(CSSPoint(0, 50));
  apzc->SetFrameMetrics(metrics);

  // Scroll up a bit where overscroll will not happen.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure it's not yet overscrolled.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  // Send pan momentum events including horizontal bits.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-10, -100), mcc->Time());
  // Now it's overscrolled.
  EXPECT_TRUE(apzc->IsOverscrolled());

  AsyncTransformComponentMatrix overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // But the overscroll shouldn't happen horizontally.
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  // Happens only vertically.
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(-5, -50), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -2), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  overscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_TRUE(overscrolledTransform._41 == 0);
  EXPECT_TRUE(overscrolledTransform._42 != 0);

  // Check that we recover from overscroll via an animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, DisallowOverscrollInSingleLineTextControl) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  // Create a horizontal scrollable frame with `vertical disregarded direction`.
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 10));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 10));
  apzc->SetFrameMetrics(metrics);
  metadata.SetDisregardedDirection(Some(ScrollDirection::eVertical));
  apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                            /*aThisLayerTreeUpdated=*/true);

  // Try to overscroll up and left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 5),
             ScreenPoint(0, 0), mcc->Time());

  // No overscrolling should happen.
  EXPECT_TRUE(!apzc->IsOverscrolled());

  // Send pan momentum events too.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-100, -100), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-50, -50), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 5), ScreenPoint(0, 0), mcc->Time());
  // No overscrolling should happen either.
  EXPECT_TRUE(!apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// Tests that horizontal overscroll animation keeps running with vertical
// pan momentum scrolling.
TEST_F(APZCOverscrollTester,
       HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);

  // Send lengthy downward momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount on X axis has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  // There is no overscroll on Y axis.
  EXPECT_EQ(currentOverscrolledTransform._42, 0);
  ParentLayerPoint scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  // The scroll offset shouldn't be changed by the overscroll animation.
  EXPECT_EQ(scrollOffset.y, 0);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount should be managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  // Not yet started scrolling.
  EXPECT_EQ(scrollOffset.y, 0);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // Now it started scrolling vertically.
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  EXPECT_GT(scrollOffset.y, 0);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // The scroll offset on Y axis shouldn't be changed by the overscroll
  // animation.
  EXPECT_EQ(scrollOffset.y, apzc->GetCurrentAsyncScrollOffset(
                                    AsyncPanZoomController::eForHitTesting)
                                .y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // Scrolling keeps going by momentum.
  EXPECT_GT(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      scrollOffset.y);

  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Scrolling keeps going by momentum.
  EXPECT_GT(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      scrollOffset.y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // This momentum event doesn't change the scroll offset since its
  // displacement is zero.
  EXPECT_EQ(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      scrollOffset.y);

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, scrollOffset.y);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// Similar to above
// HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling,
// but having OverscrollAnimation on both axes initially.
TEST_F(APZCOverscrollTester,
       BothAxesOverscrollAnimationWithPanMomentumScrolling) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll up and left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);

  // Send lengthy downward momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  EXPECT_NE(initialOverscrolledTransform._42, currentOverscrolledTransform._42);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // Still being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // But now the overscroll amount on Y axis should be changed by this momentum
  // pan.
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);
  // Actually it's no longer overscrolled.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);

  ParentLayerPoint currentScrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  // Now it started scrolling.
  EXPECT_GT(currentScrollOffset.y, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // But the overscroll on Y axis is no longer affected by the overscroll
  // animation.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);
  // The scroll offset on Y axis shouldn't be changed by the overscroll
  // animation.
  EXPECT_EQ(
      currentScrollOffset.y,
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  currentScrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);
  // Scrolling keeps going by momentum.
  EXPECT_GT(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      currentScrollOffset.y);

  currentScrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);
  // Scrolling keeps going by momentum.
  EXPECT_GT(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      currentScrollOffset.y);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  currentScrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // Keeping no overscrolling on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);
  // This momentum event doesn't change the scroll offset since its
  // displacement is zero.
  EXPECT_EQ(
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting)
          .y,
      currentScrollOffset.y);

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, currentScrollOffset.y);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Maybe fails on Android
// This is another variant of
// HorizontalOverscrollAnimationWithVerticalPanMomentumScrolling. In this test,
// after a horizontal overscroll animation started, upwards pan moments happen,
// thus there should be a new vertical overscroll animation in addition to
// the horizontal one.
TEST_F(
    APZCOverscrollTester,
    VerticalOverscrollAnimationInAdditionToExistingHorizontalOverscrollAnimation) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 1000, 5000));
  // Scrolls the content 50px down.
  metrics.SetVisualScrollOffset(CSSPoint(0, 50));
  apzc->SetFrameMetrics(metrics);

  // Try to overscroll left with pan gestures.
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-10, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(-2, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, ScreenIntPoint(50, 80),
             ScreenPoint(0, 0), mcc->Time());

  // Make sure we've started an overscroll animation.
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  AsyncTransformComponentMatrix initialOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);

  // Send lengthy __upward__ momentums to make sure the overscroll animation
  // doesn't clobber the momentums scrolling.
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount on X axis has started being managed by the overscroll
  // animation.
  AsyncTransformComponentMatrix currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  EXPECT_NE(initialOverscrolledTransform._41, currentOverscrolledTransform._41);
  // There is no overscroll on Y axis.
  EXPECT_EQ(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);
  ParentLayerPoint scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  // The scroll offset shouldn't be changed by the overscroll animation.
  EXPECT_EQ(scrollOffset.y, 50);

  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by this pan
  // momentum start event since the displacement is zero.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll amount should be managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  // Not yet started scrolling.
  EXPECT_EQ(scrollOffset.y, 50);
  EXPECT_EQ(scrollOffset.x, 0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  // Send a long pan momentum.
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -200), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // Now it started scrolling vertically.
  scrollOffset =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  EXPECT_EQ(scrollOffset.y, 0);
  EXPECT_EQ(scrollOffset.x, 0);
  // Actually it's also vertically overscrolled.
  EXPECT_GT(
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42,
      0);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // The overscroll on X axis keeps being managed by the overscroll animation.
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // The overscroll on Y Axis hasn't been changed by the overscroll animation at
  // this moment, sine the last displacement was consumed in the last pan
  // momentum.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -100), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on X axis shouldn't be changed by this momentum pan.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // Now the overscroll amount on Y axis shouldn't be changed by this momentum
  // pan either.
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  EXPECT_NE(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  // And now the overscroll on Y Axis should be also managed by the overscroll
  // animation.
  EXPECT_NE(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, -10), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());
  // The overscroll amount on both axes shouldn't be changed by momentum event.
  EXPECT_EQ(
      currentOverscrolledTransform._41,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._41);
  EXPECT_EQ(
      currentOverscrolledTransform._42,
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting)._42);

  currentOverscrolledTransform =
      apzc->GetOverscrollTransform(AsyncPanZoomController::eForHitTesting);
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, apzc,
             ScreenIntPoint(50, 80), ScreenPoint(0, 0), mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // Check that we recover from the horizontal overscroll via the animation.
  ParentLayerPoint expectedScrollOffset(0, 0);
  SampleAnimationUntilRecoveredFromOverscroll(expectedScrollOffset);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTester, OverscrollByPanGesturesInterruptedByReflowZoom) {
  if (!gfx::gfxVars::UseWebRender()) {
    // This test is only available with WebRender.
    return;
  }

  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);
  SCOPED_GFX_PREF_INT("mousewheel.with_control.action", 3);  // reflow zoom.

  // A sanity check that pan gestures with ctrl modifier will not be handled by
  // APZ.
  PanGestureInput panInput(
      PanGestureInput::PANGESTURE_START, MillisecondsSinceStartup(mcc->Time()),
      mcc->Time(), ScreenIntPoint(5, 5), ScreenPoint(0, -2), MODIFIER_CONTROL);
  WidgetWheelEvent wheelEvent = panInput.ToWidgetEvent(nullptr);
  EXPECT_FALSE(APZInputBridge::ActionForWheelEvent(&wheelEvent).isSome());

  ScrollableLayerGuid rootGuid = CreateSimpleRootScrollableForWebRender();
  RefPtr<AsyncPanZoomController> apzc =
      tm->GetTargetAPZC(rootGuid.mLayersId, rootGuid.mScrollId);

  PanGesture(PanGestureInput::PANGESTURE_START, tm, ScreenIntPoint(50, 80),
             ScreenPoint(0, -2), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  PanGesture(PanGestureInput::PANGESTURE_PAN, tm, ScreenIntPoint(50, 80),
             ScreenPoint(0, -10), mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());

  // Make sure overscrolling has started.
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Press ctrl until PANGESTURE_END.
  PanGestureWithModifiers(PanGestureInput::PANGESTURE_PAN, MODIFIER_CONTROL, tm,
                          ScreenIntPoint(50, 80), ScreenPoint(0, -2),
                          mcc->Time());
  mcc->AdvanceByMillis(5);
  apzc->AdvanceAnimations(mcc->GetSampleTime());
  // At this moment (i.e. PANGESTURE_PAN), still in overscrolling state.
  EXPECT_TRUE(apzc->IsOverscrolled());

  PanGestureWithModifiers(PanGestureInput::PANGESTURE_END, MODIFIER_CONTROL, tm,
                          ScreenIntPoint(50, 80), ScreenPoint(0, 0),
                          mcc->Time());
  // The overscrolling state should have been restored.
  EXPECT_TRUE(!apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, SmoothTransitionFromPanToAnimation) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Start scrolled down to y=500px.
  metrics.SetVisualScrollOffset(CSSPoint(0, 500));
  apzc->SetFrameMetrics(metrics);

  int frameLength = 10;    // milliseconds; 10 to keep the math simple
  float panVelocity = 10;  // pixels per millisecond
  int panPixelsPerFrame = frameLength * panVelocity;  // 100 pixels per frame

  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -1), mcc->Time());
  // Pan up for 6 frames at 100 pixels per frame. This should reduce
  // the vertical scroll offset from 500 to 0, and get us into overscroll.
  for (int i = 0; i < 6; ++i) {
    mcc->AdvanceByMillis(frameLength);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, -panPixelsPerFrame), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Pan further into overscroll at the same input velocity, enough
  // for the frames while we are in overscroll to dominate the computation
  // in the velocity tracker.
  // Importantly, while the input velocity is still 100 pixels per frame,
  // in the overscrolled state the page only visual moves by at most 8 pixels
  // per frame.
  int frames = StaticPrefs::apz_velocity_relevance_time_ms() / frameLength;
  for (int i = 0; i < frames; ++i) {
    mcc->AdvanceByMillis(frameLength);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, -panPixelsPerFrame), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());

  // End the pan, allowing an overscroll animation to start.
  mcc->AdvanceByMillis(frameLength);
  PanGesture(PanGestureInput::PANGESTURE_END, apzc, panPoint, ScreenPoint(0, 0),
             mcc->Time());
  EXPECT_TRUE(apzc->IsOverscrollAnimationRunning());

  // Check that the velocity reflects the actual movement (no more than 8
  // pixels/frame ==> 0.8 pixels per millisecond), not the input velocity
  // (100 pixels/frame ==> 10 pixels per millisecond). This ensures that
  // the transition from the pan to the animation appears smooth.
  // (Note: velocities are negative since they are upwards.)
  EXPECT_LT(apzc->GetVelocityVector().y, 0);
  EXPECT_GT(apzc->GetVelocityVector().y, -0.8);
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, NoOverscrollForMousewheel) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  // Start scrolled down just a few pixels from the top.
  metrics.SetVisualScrollOffset(CSSPoint(0, 3));
  // Set line and page scroll amounts. Otherwise, even though Wheel() uses
  // SCROLLDELTA_PIXEL, the wheel handling code will get confused by things
  // like the "don't scroll more than one page" check.
  metadata.SetPageScrollAmount(LayoutDeviceIntSize(50, 100));
  metadata.SetLineScrollAmount(LayoutDeviceIntSize(5, 10));
  apzc->SetScrollMetadata(metadata);

  // Send a wheel with enough delta to scrollto y=0 *and* overscroll.
  Wheel(apzc, ScreenIntPoint(10, 10), ScreenPoint(0, -10), mcc->Time());

  // Check that we did not actually go into overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

#ifndef MOZ_WIDGET_ANDROID  // Only applies to GenericOverscrollEffect
TEST_F(APZCOverscrollTester, DynamicallyLoadingContent) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetCompositionBounds(ParentLayerRect(0, 0, 100, 100));
  metrics.SetScrollableRect(CSSRect(0, 0, 100, 1000));
  metrics.SetVisualScrollOffset(CSSPoint(0, 0));
  apzc->SetFrameMetrics(metrics);

  // Pan to the bottom of the page, and further, into overscroll.
  ScreenIntPoint panPoint(50, 50);
  PanGesture(PanGestureInput::PANGESTURE_START, apzc, panPoint,
             ScreenPoint(0, -1), mcc->Time());
  for (int i = 0; i < 12; ++i) {
    mcc->AdvanceByMillis(10);
    PanGesture(PanGestureInput::PANGESTURE_PAN, apzc, panPoint,
               ScreenPoint(0, 100), mcc->Time());
  }
  EXPECT_TRUE(apzc->IsOverscrolled());
  EXPECT_TRUE(apzc->GetOverscrollAmount().y > 0);  // overscrolled at bottom

  // Grow the scrollable rect at the bottom, simulating the page loading content
  // dynamically.
  CSSRect scrollableRect = metrics.GetScrollableRect();
  scrollableRect.height += 500;
  metrics.SetScrollableRect(scrollableRect);
  apzc->NotifyLayersUpdated(metadata, false, true);

  // Check that the modified scrollable rect cleared the overscroll.
  EXPECT_FALSE(apzc->IsOverscrolled());
}
#endif

class APZCOverscrollTesterForLayersOnly : public APZCTreeManagerTester {
 public:
  APZCOverscrollTesterForLayersOnly() { mLayersOnly = true; }

  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCOverscrollTesterForLayersOnly, OverscrollHandoff) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  const char* layerTreeSyntax = "c(c)";
  nsIntRegion layerVisibleRegion[] = {nsIntRegion(IntRect(0, 0, 100, 100)),
                                      nsIntRegion(IntRect(0, 0, 100, 50))};
  root =
      CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 200, 200));
  SetScrollableFrameMetrics(layers[1], ScrollableLayerGuid::START_SCROLL_ID + 1,
                            // same size as the visible region so that
                            // the container is not scrollable in any directions
                            // actually. This is simulating overflow: hidden
                            // iframe document in Fission, though we don't set
                            // a different layers id.
                            CSSRect(0, 0, 100, 50));

  SetScrollHandoff(layers[1], root);

  registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, root, mcc);
  UpdateHitTestingTree();
  rootApzc = ApzcOf(root);
  rootApzc->GetFrameMetrics().SetIsRootContent(true);

  // A pan gesture on the child scroller (which is not scrollable though).
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 20),
             ScreenPoint(0, -2), mcc->Time());
  EXPECT_TRUE(rootApzc->IsOverscrolled());
}
#endif
