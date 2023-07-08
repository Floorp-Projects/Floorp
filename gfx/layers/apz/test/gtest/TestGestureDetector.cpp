/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "APZCBasicTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"
#include "mozilla/StaticPrefs_apz.h"

// Note: There are additional tests that test gesture detection behaviour
//       with multiple APZCs in TestTreeManager.cpp.

class APZCGestureDetectorTester : public APZCBasicTester {
 public:
  APZCGestureDetectorTester()
      : APZCBasicTester(AsyncPanZoomController::USE_GESTURE_DETECTOR) {}

 protected:
  FrameMetrics GetPinchableFrameMetrics() {
    FrameMetrics fm;
    fm.SetCompositionBounds(ParentLayerRect(200, 200, 100, 200));
    fm.SetScrollableRect(CSSRect(0, 0, 980, 1000));
    fm.SetVisualScrollOffset(CSSPoint(300, 300));
    fm.SetZoom(CSSToParentLayerScale(2.0));
    // APZC only allows zooming on the root scrollable frame.
    fm.SetIsRootContent(true);
    // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100
    return fm;
  }
};

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCGestureDetectorTester, Pan_After_Pinch) {
  SCOPED_GFX_PREF_INT("apz.axis_lock.mode", 2);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.lock_angle", M_PI / 6.0f);
  SCOPED_GFX_PREF_FLOAT("apz.axis_lock.breakout_angle", M_PI / 8.0f);

  FrameMetrics originalMetrics = GetPinchableFrameMetrics();
  apzc->SetFrameMetrics(originalMetrics);

  MakeApzcZoomable();

  // Test parameters
  float zoomAmount = 1.25;
  float pinchLength = 100.0;
  float pinchLengthScaled = pinchLength * zoomAmount;
  int focusX = 250;
  int focusY = 300;
  int panDistance = 20;
  const TimeDuration TIME_BETWEEN_TOUCH_EVENT =
      TimeDuration::FromMilliseconds(50);

  int firstFingerId = 0;
  int secondFingerId = firstFingerId + 1;

  // Put fingers down
  MultiTouchInput mti =
      MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, focusX, focusY));
  mti.mTouches.AppendElement(
      CreateSingleTouchData(secondFingerId, focusX, focusY));
  apzc->ReceiveInputEvent(mti, Some(nsTArray<uint32_t>{kDefaultTouchBehavior}));
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Spread fingers out to enter the pinch state
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      firstFingerId, static_cast<int32_t>(focusX - pinchLength), focusY));
  mti.mTouches.AppendElement(CreateSingleTouchData(
      secondFingerId, static_cast<int32_t>(focusX + pinchLength), focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Do the actual pinch of 1.25x
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      firstFingerId, static_cast<int32_t>(focusX - pinchLengthScaled), focusY));
  mti.mTouches.AppendElement(CreateSingleTouchData(
      secondFingerId, static_cast<int32_t>(focusX + pinchLengthScaled),
      focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Verify that the zoom changed, just to make sure our code above did what it
  // was supposed to.
  FrameMetrics zoomedMetrics = apzc->GetFrameMetrics();
  float newZoom = zoomedMetrics.GetZoom().scale;
  EXPECT_EQ(originalMetrics.GetZoom().scale * zoomAmount, newZoom);

  // Now we lift one finger...
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      secondFingerId, static_cast<int32_t>(focusX + pinchLengthScaled),
      focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // ... and pan with the remaining finger. This pan just breaks through the
  // distance threshold.
  focusY += StaticPrefs::apz_touch_start_tolerance() * tm->GetDPI();
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      firstFingerId, static_cast<int32_t>(focusX - pinchLengthScaled), focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // This one does an actual pan of 20 pixels
  focusY += panDistance;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      firstFingerId, static_cast<int32_t>(focusX - pinchLengthScaled), focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Lift the remaining finger
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, mcc->Time(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(
      firstFingerId, static_cast<int32_t>(focusX - pinchLengthScaled), focusY));
  apzc->ReceiveInputEvent(mti);
  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Verify that we scrolled
  FrameMetrics finalMetrics = apzc->GetFrameMetrics();
  EXPECT_EQ(zoomedMetrics.GetVisualScrollOffset().y - (panDistance / newZoom),
            finalMetrics.GetVisualScrollOffset().y);

  // Clear out any remaining fling animation and pending tasks
  apzc->AdvanceAnimationsUntilEnd();
  while (mcc->RunThroughDelayedTasks())
    ;
  apzc->AssertStateIsReset();
}
#endif

TEST_F(APZCGestureDetectorTester, Pan_With_Tap) {
  SCOPED_GFX_PREF_FLOAT("apz.touch_start_tolerance", 0.1);

  FrameMetrics originalMetrics = GetPinchableFrameMetrics();
  apzc->SetFrameMetrics(originalMetrics);

  // Making the APZC zoomable isn't really needed for the correct operation of
  // this test, but it could help catch regressions where we accidentally enter
  // a pinch state.
  MakeApzcZoomable();

  // Test parameters
  int touchX = 250;
  int touchY = 300;
  int panDistance = 20;

  int firstFingerId = 0;
  int secondFingerId = firstFingerId + 1;

  const float panThreshold =
      StaticPrefs::apz_touch_start_tolerance() * tm->GetDPI();

  // Put finger down
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, Some(nsTArray<uint32_t>{kDefaultTouchBehavior}));

  // Start a pan, break through the threshold
  touchY += panThreshold;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti);

  // Do an actual pan for a bit
  touchY += panDistance;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti);

  // Put a second finger down
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  mti.mTouches.AppendElement(
      CreateSingleTouchData(secondFingerId, touchX + 10, touchY));
  apzc->ReceiveInputEvent(mti, Some(nsTArray<uint32_t>{kDefaultTouchBehavior}));

  // Lift the second finger
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(secondFingerId, touchX + 10, touchY));
  apzc->ReceiveInputEvent(mti);

  // Bust through the threshold again
  touchY += panThreshold;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti);

  // Do some more actual panning
  touchY += panDistance;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti);

  // Lift the first finger
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(
      CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti);

  // Verify that we scrolled
  FrameMetrics finalMetrics = apzc->GetFrameMetrics();
  float zoom = finalMetrics.GetZoom().scale;
  EXPECT_EQ(
      originalMetrics.GetVisualScrollOffset().y - (panDistance * 2 / zoom),
      finalMetrics.GetVisualScrollOffset().y);

  // Clear out any remaining fling animation and pending tasks
  apzc->AdvanceAnimationsUntilEnd();
  while (mcc->RunThroughDelayedTasks())
    ;
  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, SecondTapIsFar_Bug1586496) {
  // Test that we receive two single-tap events when two tap gestures are
  // close in time but far in distance.
  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, _, 0, apzc->GetGuid(), _))
      .Times(2);

  TimeDuration brief =
      TimeDuration::FromMilliseconds(StaticPrefs::apz_max_tap_time() / 10.0);

  ScreenIntPoint point(10, 10);
  Tap(apzc, point, brief);

  mcc->AdvanceBy(brief);

  point.x += static_cast<int32_t>(apzc->GetSecondTapTolerance() * 2);
  point.y += static_cast<int32_t>(apzc->GetSecondTapTolerance() * 2);

  Tap(apzc, point, brief);
}

class APZCFlingStopTester : public APZCGestureDetectorTester {
 protected:
  // Start a fling, and then tap while the fling is ongoing. When
  // aSlow is false, the tap will happen while the fling is at a
  // high velocity, and we check that the tap doesn't trigger sending a tap
  // to content. If aSlow is true, the tap will happen while the fling
  // is at a slow velocity, and we check that the tap does trigger sending
  // a tap to content. See bug 1022956.
  void DoFlingStopTest(bool aSlow) {
    int touchStart = 50;
    int touchEnd = 10;

    // Start the fling down.
    Pan(apzc, touchStart, touchEnd);
    // The touchstart from the pan will leave some cancelled tasks in the queue,
    // clear them out

    // If we want to tap while the fling is fast, let the fling advance for 10ms
    // only. If we want the fling to slow down more, advance to 2000ms. These
    // numbers may need adjusting if our friction and threshold values change,
    // but they should be deterministic at least.
    int timeDelta = aSlow ? 2000 : 10;
    int tapCallsExpected = aSlow ? 2 : 1;

    // Advance the fling animation by timeDelta milliseconds.
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(
        &viewTransformOut, pointOut, TimeDuration::FromMilliseconds(timeDelta));

    // Deliver a tap to abort the fling. Ensure that we get a SingleTap
    // call out of it if and only if the fling is slow.
    EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, _, 0, apzc->GetGuid(), _))
        .Times(tapCallsExpected);
    Tap(apzc, ScreenIntPoint(10, 10), 0);
    while (mcc->RunThroughDelayedTasks())
      ;

    // Deliver another tap, to make sure that taps are flowing properly once
    // the fling is aborted.
    Tap(apzc, ScreenIntPoint(100, 100), 0);
    while (mcc->RunThroughDelayedTasks())
      ;

    // Verify that we didn't advance any further after the fling was aborted, in
    // either case.
    ParentLayerPoint finalPointOut;
    apzc->SampleContentTransformForFrame(&viewTransformOut, finalPointOut);
    EXPECT_EQ(pointOut.x, finalPointOut.x);
    EXPECT_EQ(pointOut.y, finalPointOut.y);

    apzc->AssertStateIsReset();
  }

  void DoFlingStopWithSlowListener(bool aPreventDefault) {
    MakeApzcWaitForMainThread();

    int touchStart = 50;
    int touchEnd = 10;
    uint64_t blockId = 0;

    // Start the fling down.
    Pan(apzc, touchStart, touchEnd, PanOptions::None, nullptr, nullptr,
        &blockId);
    apzc->ConfirmTarget(blockId);
    apzc->ContentReceivedInputBlock(blockId, false);

    // Sample the fling a couple of times to ensure it's going.
    ParentLayerPoint point, finalPoint;
    AsyncTransform viewTransform;
    apzc->SampleContentTransformForFrame(&viewTransform, point,
                                         TimeDuration::FromMilliseconds(10));
    apzc->SampleContentTransformForFrame(&viewTransform, finalPoint,
                                         TimeDuration::FromMilliseconds(10));
    EXPECT_GT(finalPoint.y, point.y);

    // Now we put our finger down to stop the fling
    blockId =
        TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time()).mInputBlockId;

    // Re-sample to make sure it hasn't moved
    apzc->SampleContentTransformForFrame(&viewTransform, point,
                                         TimeDuration::FromMilliseconds(10));
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // respond to the touchdown that stopped the fling.
    // even if we do a prevent-default on it, the animation should remain
    // stopped.
    apzc->ContentReceivedInputBlock(blockId, aPreventDefault);

    // Verify the page hasn't moved
    apzc->SampleContentTransformForFrame(&viewTransform, point,
                                         TimeDuration::FromMilliseconds(70));
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // clean up
    TouchUp(apzc, ScreenIntPoint(10, 10), mcc->Time());

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCFlingStopTester, FlingStop) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  DoFlingStopTest(false);
}

TEST_F(APZCFlingStopTester, FlingStopTap) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  DoFlingStopTest(true);
}

TEST_F(APZCFlingStopTester, FlingStopSlowListener) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  DoFlingStopWithSlowListener(false);
}

TEST_F(APZCFlingStopTester, FlingStopPreventDefault) {
  SCOPED_GFX_PREF_FLOAT("apz.fling_min_velocity_threshold", 0.0f);
  DoFlingStopWithSlowListener(true);
}

TEST_F(APZCGestureDetectorTester, ShortPress) {
  MakeApzcUnzoomable();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    // This verifies that the single tap notification is sent after the
    // touchup is fully processed. The ordering here is important.
    EXPECT_CALL(check, Call("pre-tap"));
    EXPECT_CALL(check, Call("post-tap"));
    EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10),
                                0, apzc->GetGuid(), _))
        .Times(1);
  }

  check.Call("pre-tap");
  TapAndCheckStatus(apzc, ScreenIntPoint(10, 10),
                    TimeDuration::FromMilliseconds(100));
  check.Call("post-tap");

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, MediumPress) {
  MakeApzcUnzoomable();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    // This verifies that the single tap notification is sent after the
    // touchup is fully processed. The ordering here is important.
    EXPECT_CALL(check, Call("pre-tap"));
    EXPECT_CALL(check, Call("post-tap"));
    EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10),
                                0, apzc->GetGuid(), _))
        .Times(1);
  }

  check.Call("pre-tap");
  TapAndCheckStatus(apzc, ScreenIntPoint(10, 10),
                    TimeDuration::FromMilliseconds(400));
  check.Call("post-tap");

  apzc->AssertStateIsReset();
}

class APZCLongPressTester : public APZCGestureDetectorTester {
 protected:
  void DoLongPressTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    APZEventResult result =
        TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, result.GetStatus());
    uint64_t blockId = result.mInputBlockId;

    if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(blockId, allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedInputBlock(blockId, false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      blockId++;
      EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, LayoutDevicePoint(10, 10),
                                  0, apzc->GetGuid(), blockId))
          .Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));

      EXPECT_CALL(check, Call("preHandleLongTapUp"));
      EXPECT_CALL(*mcc,
                  HandleTap(TapType::eLongTapUp, LayoutDevicePoint(10, 10), 0,
                            apzc->GetGuid(), _))
          .Times(1);
      EXPECT_CALL(check, Call("postHandleLongTapUp"));
    }

    // Manually invoke the longpress while the touch is currently down.
    check.Call("preHandleLongTap");
    mcc->RunThroughDelayedTasks();
    check.Call("postHandleLongTap");

    // Dispatching the longpress event starts a new touch block, which
    // needs a new content response and also has a pending timeout task
    // in the queue. Deal with those here. We do the content response first
    // with preventDefault=false, and then we run the timeout task which
    // "loses the race" and does nothing.
    apzc->ContentReceivedInputBlock(blockId, false);
    mcc->AdvanceByMillis(1000);

    // Finally, simulate lifting the finger. Since the long-press wasn't
    // prevent-defaulted, we should get a long-tap-up event.
    check.Call("preHandleLongTapUp");
    result = TouchUp(apzc, ScreenIntPoint(10, 10), mcc->Time());
    mcc->RunThroughDelayedTasks();
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, result.GetStatus());
    check.Call("postHandleLongTapUp");

    apzc->AssertStateIsReset();
  }

  void DoLongPressPreventDefaultTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);

    int touchX = 10, touchStartY = 50, touchEndY = 10;

    APZEventResult result =
        TouchDown(apzc, ScreenIntPoint(touchX, touchStartY), mcc->Time());
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, result.GetStatus());
    uint64_t blockId = result.mInputBlockId;

    if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(blockId, allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedInputBlock(blockId, false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      blockId++;
      EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap,
                                  LayoutDevicePoint(touchX, touchStartY), 0,
                                  apzc->GetGuid(), blockId))
          .Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));
    }

    // Manually invoke the longpress while the touch is currently down.
    check.Call("preHandleLongTap");
    mcc->RunThroughDelayedTasks();
    check.Call("postHandleLongTap");

    // There should be a TimeoutContentResponse task in the queue still,
    // waiting for the response from the longtap event dispatched above.
    // Send the signal that content has handled the long-tap, and then run
    // the timeout task (it will be a no-op because the content "wins" the
    // race. This takes the place of the "contextmenu" event.
    apzc->ContentReceivedInputBlock(blockId, true);
    mcc->AdvanceByMillis(1000);

    MultiTouchInput mti =
        CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
    mti.mTouches.AppendElement(CreateSingleTouchData(0, touchX, touchEndY));
    result = apzc->ReceiveInputEvent(mti);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, result.GetStatus());

    EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap,
                                LayoutDevicePoint(touchX, touchEndY), 0,
                                apzc->GetGuid(), _))
        .Times(0);
    result = TouchUp(apzc, ScreenIntPoint(touchX, touchEndY), mcc->Time());
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, result.GetStatus());

    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCLongPressTester, LongPress) {
  DoLongPressTest(kDefaultTouchBehavior);
}

TEST_F(APZCLongPressTester, LongPressPreventDefault) {
  DoLongPressPreventDefaultTest(kDefaultTouchBehavior);
}

TEST_F(APZCGestureDetectorTester, DoubleTap) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  apzc->GetFrameMetrics().SetIsRootContent(true);

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(0);
  EXPECT_CALL(*mcc, HandleTap(TapType::eDoubleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, ScreenIntPoint(10, 10), &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapNotZoomable) {
  MakeApzcWaitForMainThread();
  MakeApzcUnzoomable();

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);
  EXPECT_CALL(*mcc, HandleTap(TapType::eSecondTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);
  EXPECT_CALL(*mcc, HandleTap(TapType::eDoubleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, ScreenIntPoint(10, 10), &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultFirstOnly) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);
  EXPECT_CALL(*mcc, HandleTap(TapType::eDoubleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, ScreenIntPoint(10, 10), &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultBoth) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(0);
  EXPECT_CALL(*mcc, HandleTap(TapType::eDoubleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, ScreenIntPoint(10, 10), &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], true);

  apzc->AssertStateIsReset();
}

// Test for bug 947892
// We test whether we dispatch tap event when the tap is followed by pinch.
TEST_F(APZCGestureDetectorTester, TapFollowedByPinch) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);

  Tap(apzc, ScreenIntPoint(10, 10), TimeDuration::FromMilliseconds(100));

  int inputId = 0;
  MultiTouchInput mti;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20),
                                             ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(
      inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20),
                                             ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(
      inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, TapFollowedByMultipleTouches) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);

  Tap(apzc, ScreenIntPoint(10, 10), TimeDuration::FromMilliseconds(100));

  int inputId = 0;
  MultiTouchInput mti;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20),
                                             ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, Some(nsTArray<uint32_t>{kDefaultTouchBehavior}));

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20),
                                             ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(
      inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, Some(nsTArray<uint32_t>{kDefaultTouchBehavior}));

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20),
                                             ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(
      inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, LongPressInterruptedByWheel) {
  // Since we try to allow concurrent input blocks of different types to
  // co-exist, the wheel block shouldn't interrupt the long-press detection.
  // But more importantly, this shouldn't crash, which is what it did at one
  // point in time.
  EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, _, _, _, _)).Times(1);

  APZEventResult result = TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  uint64_t touchBlockId = result.mInputBlockId;
  if (result.GetStatus() != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(apzc, touchBlockId);
  }
  mcc->AdvanceByMillis(10);
  uint64_t wheelBlockId =
      Wheel(apzc, ScreenIntPoint(10, 10), ScreenPoint(0, -10), mcc->Time())
          .mInputBlockId;
  EXPECT_NE(touchBlockId, wheelBlockId);
  mcc->AdvanceByMillis(1000);
}

TEST_F(APZCGestureDetectorTester, TapTimeoutInterruptedByWheel) {
  // In this test, even though the wheel block comes right after the tap, the
  // tap should still be dispatched because it completes fully before the wheel
  // block arrived.
  EXPECT_CALL(*mcc, HandleTap(TapType::eSingleTap, LayoutDevicePoint(10, 10), 0,
                              apzc->GetGuid(), _))
      .Times(1);

  // We make the APZC zoomable so the gesture detector needs to wait to
  // distinguish between tap and double-tap. During that timeout is when we
  // insert the wheel event.
  MakeApzcZoomable();

  uint64_t touchBlockId = 0;
  Tap(apzc, ScreenIntPoint(10, 10), TimeDuration::FromMilliseconds(100),
      nullptr, &touchBlockId);
  mcc->AdvanceByMillis(10);
  uint64_t wheelBlockId =
      Wheel(apzc, ScreenIntPoint(10, 10), ScreenPoint(0, -10), mcc->Time())
          .mInputBlockId;
  EXPECT_NE(touchBlockId, wheelBlockId);
  while (mcc->RunThroughDelayedTasks())
    ;
}

TEST_F(APZCGestureDetectorTester, LongPressWithInputQueueDelay) {
  // In this test, we ensure that any time spent waiting in the input queue for
  // the content response is subtracted from the long-press timeout in the
  // GestureEventListener. In this test the content response timeout is longer
  // than the long-press timeout.
  SCOPED_GFX_PREF_INT("apz.content_response_timeout", 60);
  SCOPED_GFX_PREF_INT("ui.click_hold_context_menus.delay", 30);

  MakeApzcWaitForMainThread();

  MockFunction<void(std::string checkPointName)> check;

  {
    InSequence s;
    EXPECT_CALL(check, Call("pre long-tap dispatch"));
    EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, LayoutDevicePoint(10, 10), 0,
                                apzc->GetGuid(), _))
        .Times(1);
    EXPECT_CALL(check, Call("post long-tap dispatch"));
  }

  // Touch down
  APZEventResult result = TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  uint64_t touchBlockId = result.mInputBlockId;
  // Simulate content response after 10ms
  mcc->AdvanceByMillis(10);
  apzc->ContentReceivedInputBlock(touchBlockId, false);
  apzc->SetAllowedTouchBehavior(touchBlockId, {kDefaultTouchBehavior});
  apzc->ConfirmTarget(touchBlockId);
  // Ensure long-tap event happens within 20ms after that
  check.Call("pre long-tap dispatch");
  mcc->AdvanceByMillis(20);
  check.Call("post long-tap dispatch");
}

TEST_F(APZCGestureDetectorTester, LongPressWithInputQueueDelay2) {
  // Similar to the previous test, except this time we don't simulate the
  // content response at all, and still expect the long-press to happen on
  // schedule.
  SCOPED_GFX_PREF_INT("apz.content_response_timeout", 60);
  SCOPED_GFX_PREF_INT("ui.click_hold_context_menus.delay", 30);

  MakeApzcWaitForMainThread();

  MockFunction<void(std::string checkPointName)> check;

  {
    InSequence s;
    EXPECT_CALL(check, Call("pre long-tap dispatch"));
    EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, LayoutDevicePoint(10, 10), 0,
                                apzc->GetGuid(), _))
        .Times(1);
    EXPECT_CALL(check, Call("post long-tap dispatch"));
  }

  // Touch down
  TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  // Ensure the long-tap happens within 30ms even though there's no content
  // response.
  check.Call("pre long-tap dispatch");
  mcc->AdvanceByMillis(30);
  check.Call("post long-tap dispatch");
}

TEST_F(APZCGestureDetectorTester, LongPressWithInputQueueDelay3) {
  // Similar to the previous test, except now we have the long-press delay
  // being longer than the content response timeout.
  SCOPED_GFX_PREF_INT("apz.content_response_timeout", 30);
  SCOPED_GFX_PREF_INT("ui.click_hold_context_menus.delay", 60);

  MakeApzcWaitForMainThread();

  MockFunction<void(std::string checkPointName)> check;

  {
    InSequence s;
    EXPECT_CALL(check, Call("pre long-tap dispatch"));
    EXPECT_CALL(*mcc, HandleTap(TapType::eLongTap, LayoutDevicePoint(10, 10), 0,
                                apzc->GetGuid(), _))
        .Times(1);
    EXPECT_CALL(check, Call("post long-tap dispatch"));
  }

  // Touch down
  TouchDown(apzc, ScreenIntPoint(10, 10), mcc->Time());
  // Ensure the long-tap happens at the 60ms mark even though the input event
  // waits in the input queue for the full content response timeout of 30ms
  mcc->AdvanceByMillis(59);
  check.Call("pre long-tap dispatch");
  mcc->AdvanceByMillis(1);
  check.Call("post long-tap dispatch");
}
