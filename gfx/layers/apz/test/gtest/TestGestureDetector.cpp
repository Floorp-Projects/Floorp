/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZTestCommon.h"

class APZCGestureDetectorTester : public APZCBasicTester {
public:
  APZCGestureDetectorTester()
    : APZCBasicTester(AsyncPanZoomController::USE_GESTURE_DETECTOR)
  {
  }

protected:
  FrameMetrics GetPinchableFrameMetrics()
  {
    FrameMetrics fm;
    fm.SetCompositionBounds(ParentLayerRect(200, 200, 100, 200));
    fm.SetScrollableRect(CSSRect(0, 0, 980, 1000));
    fm.SetScrollOffset(CSSPoint(300, 300));
    fm.SetZoom(CSSToParentLayerScale2D(2.0, 2.0));
    // APZC only allows zooming on the root scrollable frame.
    fm.SetIsRootContent(true);
    // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100
    return fm;
  }
};

TEST_F(APZCGestureDetectorTester, Pan_After_Pinch) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);

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

  int firstFingerId = 0;
  int secondFingerId = firstFingerId + 1;

  // Put fingers down
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX, focusY));
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, focusX, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Spread fingers out to enter the pinch state
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX - pinchLength, focusY));
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, focusX + pinchLength, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Do the actual pinch of 1.25x
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX - pinchLengthScaled, focusY));
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, focusX + pinchLengthScaled, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Verify that the zoom changed, just to make sure our code above did what it
  // was supposed to.
  FrameMetrics zoomedMetrics = apzc->GetFrameMetrics();
  float newZoom = zoomedMetrics.GetZoom().ToScaleFactor().scale;
  EXPECT_EQ(originalMetrics.GetZoom().ToScaleFactor().scale * zoomAmount, newZoom);

  // Now we lift one finger...
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, focusX + pinchLengthScaled, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // ... and pan with the remaining finger. This pan just breaks through the
  // distance threshold.
  focusY += 40;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX - pinchLengthScaled, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // This one does an actual pan of 20 pixels
  focusY += panDistance;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX - pinchLengthScaled, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Lift the remaining finger
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, focusX - pinchLengthScaled, focusY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Verify that we scrolled
  FrameMetrics finalMetrics = apzc->GetFrameMetrics();
  EXPECT_EQ(zoomedMetrics.GetScrollOffset().y - (panDistance / newZoom), finalMetrics.GetScrollOffset().y);

  // Clear out any remaining fling animation and pending tasks
  apzc->AdvanceAnimationsUntilEnd();
  while (mcc->RunThroughDelayedTasks());
  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, Pan_With_Tap) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);

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

  // Put finger down
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Start a pan, break through the threshold
  touchY += 40;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Do an actual pan for a bit
  touchY += panDistance;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Put a second finger down
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, touchX + 10, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Lift the second finger
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(secondFingerId, touchX + 10, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Bust through the threshold again
  touchY += 40;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Do some more actual panning
  touchY += panDistance;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Lift the first finger
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(firstFingerId, touchX, touchY));
  apzc->ReceiveInputEvent(mti, nullptr);

  // Verify that we scrolled
  FrameMetrics finalMetrics = apzc->GetFrameMetrics();
  float zoom = finalMetrics.GetZoom().ToScaleFactor().scale;
  EXPECT_EQ(originalMetrics.GetScrollOffset().y - (panDistance * 2 / zoom), finalMetrics.GetScrollOffset().y);

  // Clear out any remaining fling animation and pending tasks
  apzc->AdvanceAnimationsUntilEnd();
  while (mcc->RunThroughDelayedTasks());
  apzc->AssertStateIsReset();
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
    Pan(apzc, mcc, touchStart, touchEnd);
    // The touchstart from the pan will leave some cancelled tasks in the queue, clear them out

    // If we want to tap while the fling is fast, let the fling advance for 10ms only. If we want
    // the fling to slow down more, advance to 2000ms. These numbers may need adjusting if our
    // friction and threshold values change, but they should be deterministic at least.
    int timeDelta = aSlow ? 2000 : 10;
    int tapCallsExpected = aSlow ? 2 : 1;

    // Advance the fling animation by timeDelta milliseconds.
    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut, TimeDuration::FromMilliseconds(timeDelta));

    // Deliver a tap to abort the fling. Ensure that we get a HandleSingleTap
    // call out of it if and only if the fling is slow.
    EXPECT_CALL(*mcc, HandleSingleTap(_, 0, apzc->GetGuid())).Times(tapCallsExpected);
    Tap(apzc, 10, 10, mcc, 0);
    while (mcc->RunThroughDelayedTasks());

    // Deliver another tap, to make sure that taps are flowing properly once
    // the fling is aborted.
    Tap(apzc, 100, 100, mcc, 0);
    while (mcc->RunThroughDelayedTasks());

    // Verify that we didn't advance any further after the fling was aborted, in either case.
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
    Pan(apzc, mcc, touchStart, touchEnd, false, nullptr, nullptr, &blockId);
    apzc->ConfirmTarget(blockId);
    apzc->ContentReceivedInputBlock(blockId, false);

    // Sample the fling a couple of times to ensure it's going.
    ParentLayerPoint point, finalPoint;
    AsyncTransform viewTransform;
    apzc->SampleContentTransformForFrame(&viewTransform, point, TimeDuration::FromMilliseconds(10));
    apzc->SampleContentTransformForFrame(&viewTransform, finalPoint, TimeDuration::FromMilliseconds(10));
    EXPECT_GT(finalPoint.y, point.y);

    // Now we put our finger down to stop the fling
    TouchDown(apzc, 10, 10, mcc->Time(), &blockId);

    // Re-sample to make sure it hasn't moved
    apzc->SampleContentTransformForFrame(&viewTransform, point, TimeDuration::FromMilliseconds(10));
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // respond to the touchdown that stopped the fling.
    // even if we do a prevent-default on it, the animation should remain stopped.
    apzc->ContentReceivedInputBlock(blockId, aPreventDefault);

    // Verify the page hasn't moved
    apzc->SampleContentTransformForFrame(&viewTransform, point, TimeDuration::FromMilliseconds(70));
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // clean up
    TouchUp(apzc, 10, 10, mcc->Time());

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCFlingStopTester, FlingStop) {
  DoFlingStopTest(false);
}

TEST_F(APZCFlingStopTester, FlingStopTap) {
  DoFlingStopTest(true);
}

TEST_F(APZCFlingStopTester, FlingStopSlowListener) {
  DoFlingStopWithSlowListener(false);
}

TEST_F(APZCFlingStopTester, FlingStopPreventDefault) {
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
    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  }

  check.Call("pre-tap");
  TapAndCheckStatus(apzc, 10, 10, mcc, TimeDuration::FromMilliseconds(100));
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
    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  }

  check.Call("pre-tap");
  TapAndCheckStatus(apzc, 10, 10, mcc, TimeDuration::FromMilliseconds(400));
  check.Call("post-tap");

  apzc->AssertStateIsReset();
}

class APZCLongPressTester : public APZCGestureDetectorTester {
protected:
  void DoLongPressTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    uint64_t blockId = 0;

    nsEventStatus status = TouchDown(apzc, 10, 10, mcc->Time(), &blockId);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
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
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(10, 10), 0, apzc->GetGuid(), blockId)).Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));

      EXPECT_CALL(check, Call("preHandleSingleTap"));
      EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
      EXPECT_CALL(check, Call("postHandleSingleTap"));
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
    check.Call("preHandleSingleTap");
    status = TouchUp(apzc, 10, 10, mcc->Time());
    mcc->RunThroughDelayedTasks();
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);
    check.Call("postHandleSingleTap");

    apzc->AssertStateIsReset();
  }

  void DoLongPressPreventDefaultTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);

    int touchX = 10,
        touchStartY = 10,
        touchEndY = 50;

    uint64_t blockId = 0;
    nsEventStatus status = TouchDown(apzc, touchX, touchStartY, mcc->Time(), &blockId);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
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
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(touchX, touchStartY), 0, apzc->GetGuid(), blockId)).Times(1);
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

    MultiTouchInput mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, mcc->Time());
    mti.mTouches.AppendElement(SingleTouchData(0, ParentLayerPoint(touchX, touchEndY), ScreenSize(0, 0), 0, 0));
    status = apzc->ReceiveInputEvent(mti, nullptr);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(touchX, touchEndY), 0, apzc->GetGuid())).Times(0);
    status = TouchUp(apzc, touchX, touchEndY, mcc->Time());
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    ParentLayerPoint pointOut;
    AsyncTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(&viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(AsyncTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCLongPressTester, LongPress) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

TEST_F(APZCLongPressTester, LongPressPreventDefault) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressPreventDefaultWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

TEST_F(APZCGestureDetectorTester, DoubleTap) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, mcc, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapNotZoomable) {
  MakeApzcWaitForMainThread();
  MakeApzcUnzoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(2);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, mcc, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultFirstOnly) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, mcc, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultBoth) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, mcc, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], true);

  apzc->AssertStateIsReset();
}

// Test for bug 947892
// We test whether we dispatch tap event when the tap is followed by pinch.
TEST_F(APZCGestureDetectorTester, TapFollowedByPinch) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  Tap(apzc, 10, 10, mcc, TimeDuration::FromMilliseconds(100));

  int inputId = 0;
  MultiTouchInput mti;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, TapFollowedByMultipleTouches) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  Tap(apzc, 10, 10, mcc, TimeDuration::FromMilliseconds(100));

  int inputId = 0;
  MultiTouchInput mti;
  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, mcc->Time());
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  apzc->AssertStateIsReset();
}

