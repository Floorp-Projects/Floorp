/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZCPinchTester : public APZCBasicTester {
public:
  explicit APZCPinchTester(AsyncPanZoomController::GestureBehavior aGestureBehavior = AsyncPanZoomController::DEFAULT_GESTURES)
    : APZCBasicTester(aGestureBehavior)
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

  void DoPinchTest(bool aShouldTriggerPinch,
                   nsTArray<uint32_t> *aAllowedTouchBehaviors = nullptr)
  {
    apzc->SetFrameMetrics(GetPinchableFrameMetrics());
    MakeApzcZoomable();

    if (aShouldTriggerPinch) {
      // One repaint request for each gesture.
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(2);
    } else {
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
    }

    int touchInputId = 0;
    if (mGestureBehavior == AsyncPanZoomController::USE_GESTURE_DETECTOR) {
      PinchWithTouchInputAndCheckStatus(apzc, ScreenIntPoint(250, 300), 1.25,
          touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(250, 300), 1.25,
          aShouldTriggerPinch);
    }

    apzc->AssertStateIsReset();

    FrameMetrics fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=305 y=310 w=40 h=80
      EXPECT_EQ(2.5f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(305, fm.GetScrollOffset().x);
      EXPECT_EQ(310, fm.GetScrollOffset().y);
    } else {
      // The frame metrics should stay the same since touch-action:none makes
      // apzc ignore pinch gestures.
      EXPECT_EQ(2.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(300, fm.GetScrollOffset().x);
      EXPECT_EQ(300, fm.GetScrollOffset().y);
    }

    // part 2 of the test, move to the top-right corner of the page and pinch and
    // make sure we stay in the correct spot
    fm.SetZoom(CSSToParentLayerScale2D(2.0, 2.0));
    fm.SetScrollOffset(CSSPoint(930, 5));
    apzc->SetFrameMetrics(fm);
    // the visible area of the document in CSS pixels is x=930 y=5 w=50 h=100

    if (mGestureBehavior == AsyncPanZoomController::USE_GESTURE_DETECTOR) {
      PinchWithTouchInputAndCheckStatus(apzc, ScreenIntPoint(250, 300), 0.5,
          touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      PinchWithPinchInputAndCheckStatus(apzc, ScreenIntPoint(250, 300), 0.5,
          aShouldTriggerPinch);
    }

    apzc->AssertStateIsReset();

    fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=880 y=0 w=100 h=200
      EXPECT_EQ(1.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(880, fm.GetScrollOffset().x);
      EXPECT_EQ(0, fm.GetScrollOffset().y);
    } else {
      EXPECT_EQ(2.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(930, fm.GetScrollOffset().x);
      EXPECT_EQ(5, fm.GetScrollOffset().y);
    }
  }
};

class APZCPinchGestureDetectorTester : public APZCPinchTester {
public:
  APZCPinchGestureDetectorTester()
    : APZCPinchTester(AsyncPanZoomController::USE_GESTURE_DETECTOR)
  {
  }

  void DoPinchWithPreventDefaultTest() {
    FrameMetrics originalMetrics = GetPinchableFrameMetrics();
    apzc->SetFrameMetrics(originalMetrics);

    MakeApzcWaitForMainThread();
    MakeApzcZoomable();

    int touchInputId = 0;
    uint64_t blockId = 0;
    PinchWithTouchInput(apzc, ScreenIntPoint(250, 300), 1.25, touchInputId,
        nullptr, nullptr, &blockId);

    // Send the prevent-default notification for the touch block
    apzc->ContentReceivedInputBlock(blockId, true);

    // verify the metrics didn't change (i.e. the pinch was ignored)
    FrameMetrics fm = apzc->GetFrameMetrics();
    EXPECT_EQ(originalMetrics.GetZoom(), fm.GetZoom());
    EXPECT_EQ(originalMetrics.GetScrollOffset().x, fm.GetScrollOffset().x);
    EXPECT_EQ(originalMetrics.GetScrollOffset().y, fm.GetScrollOffset().y);

    apzc->AssertStateIsReset();
  }
};

class APZCPinchLockingTester : public APZCPinchTester {
private:
  static const int mDPI = 160;

  ScreenIntPoint mFocus;
  float mSpan;

public:
  APZCPinchLockingTester()
    : APZCPinchTester(AsyncPanZoomController::USE_GESTURE_DETECTOR)
    , mFocus(ScreenIntPoint(200, 300))
    , mSpan(10.0)
  {
  }

  virtual void SetUp() {
    APZCPinchTester::SetUp();
    tm->SetDPI(mDPI);
    apzc->SetFrameMetrics(GetPinchableFrameMetrics());
    MakeApzcZoomable();

    apzc->ReceiveInputEvent(
        CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                                mFocus, mSpan, mSpan),
        nullptr);
  }

  void twoFingerPan() {
    ScreenCoord panDistance
      = gfxPrefs::APZPinchLockScrollLockThreshold() * 1.2 * tm->GetDPI();

    mFocus = ScreenIntPoint(
      (int) (mFocus.x + panDistance),
      (int) (mFocus.y)
    );

    apzc->ReceiveInputEvent(
        CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                                mFocus, mSpan, mSpan),
        nullptr);
  }

  void twoFingerZoom() {
    float pinchDistance
      = gfxPrefs::APZPinchLockSpanBreakoutThreshold() * 1.2 * tm->GetDPI();

    float newSpan = mSpan + pinchDistance;

    apzc->ReceiveInputEvent(
        CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                                mFocus, newSpan, mSpan),
        nullptr);
    mSpan = newSpan;
  }

  bool isPinchLockActive() {
    FrameMetrics originalMetrics = apzc->GetFrameMetrics();

    // Send a small scale input to the APZC
    float pinchDistance
      = gfxPrefs::APZPinchLockSpanBreakoutThreshold() * 0.8 * tm->GetDPI();
    apzc->ReceiveInputEvent(
        CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                                mFocus, mSpan + pinchDistance, mSpan),
        nullptr);

    FrameMetrics result = apzc->GetFrameMetrics();
    bool lockActive = originalMetrics.GetZoom() == result.GetZoom()
      && originalMetrics.GetScrollOffset().x == result.GetScrollOffset().x
      && originalMetrics.GetScrollOffset().y == result.GetScrollOffset().y;

    // Avoid side effects, reset to original frame metrics
    apzc->SetFrameMetrics(originalMetrics);
    return lockActive;
  }
};

TEST_F(APZCPinchTester, Pinch_DefaultGestures_NoTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPinchTest(true);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_NoTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPinchTest(true);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionNone) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors = { mozilla::layers::AllowedTouchBehavior::NONE,
                                   mozilla::layers::AllowedTouchBehavior::NONE };
  DoPinchTest(false, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionZoom) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(true, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionNotAllowZoom) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(false, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionNone_NoAPZZoom) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  SCOPED_GFX_PREF(APZAllowZooming, bool, false);

  // Since we are preventing the pinch action via touch-action we should not be
  // sending the pinch gesture notifications that would normally be sent when
  // APZAllowZooming is false.
  EXPECT_CALL(*mcc, NotifyPinchGesture(_, _, _, _)).Times(0);
  nsTArray<uint32_t> behaviors = { mozilla::layers::AllowedTouchBehavior::NONE,
                                   mozilla::layers::AllowedTouchBehavior::NONE };
  DoPinchTest(false, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_PreventDefault) {
  DoPinchWithPreventDefaultTest();
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_PreventDefault_NoAPZZoom) {
  SCOPED_GFX_PREF(APZAllowZooming, bool, false);

  // Since we are preventing the pinch action we should not be sending the pinch
  // gesture notifications that would normally be sent when APZAllowZooming is
  // false.
  EXPECT_CALL(*mcc, NotifyPinchGesture(_, _, _, _)).Times(0);

  DoPinchWithPreventDefaultTest();
}

TEST_F(APZCPinchGestureDetectorTester, Panning_TwoFingerFling_ZoomDisabled) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);

  apzc->SetFrameMetrics(GetPinchableFrameMetrics());
  MakeApzcUnzoomable();

  // Perform a two finger pan
  int touchInputId = 0;
  uint64_t blockId = 0;
  PinchWithTouchInput(apzc, ScreenIntPoint(100, 200), ScreenIntPoint(100, 100),
      1, touchInputId, nullptr, nullptr, &blockId);

  // Expect to be in a flinging state
  apzc->AssertStateIsFling();
}

TEST_F(APZCPinchGestureDetectorTester, Panning_TwoFingerFling_ZoomEnabled) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);

  apzc->SetFrameMetrics(GetPinchableFrameMetrics());
  MakeApzcZoomable();

  // Perform a two finger pan
  int touchInputId = 0;
  uint64_t blockId = 0;
  PinchWithTouchInput(apzc, ScreenIntPoint(100, 200), ScreenIntPoint(100, 100),
      1, touchInputId, nullptr, nullptr, &blockId);

  // Expect to NOT be in flinging state
  apzc->AssertStateIsReset();
}

TEST_F(APZCPinchGestureDetectorTester, Panning_TwoThenOneFingerFling_ZoomEnabled) {
  SCOPED_GFX_PREF(APZFlingMinVelocityThreshold, float, 0.0f);

  apzc->SetFrameMetrics(GetPinchableFrameMetrics());
  MakeApzcZoomable();

  // Perform a two finger pan lifting only the first finger
  int touchInputId = 0;
  uint64_t blockId = 0;
  PinchWithTouchInput(apzc, ScreenIntPoint(100, 200), ScreenIntPoint(100, 100),
      1, touchInputId, nullptr, nullptr, &blockId, PinchOptions::LiftFinger2);

  // Lift second finger after a pause
  mcc->AdvanceBy(TimeDuration::FromMilliseconds(50));
  TouchUp(apzc, ScreenIntPoint(100, 100), mcc->Time());

  // Expect to NOT be in flinging state
  apzc->AssertStateIsReset();
}

TEST_F(APZCPinchTester, Panning_TwoFinger_ZoomDisabled) {
  // set up APZ
  apzc->SetFrameMetrics(GetPinchableFrameMetrics());
  MakeApzcUnzoomable();

  nsEventStatus statuses[3];  // scalebegin, scale, scaleend
  PinchWithPinchInput(apzc, ScreenIntPoint(250, 350), ScreenIntPoint(200, 300),
      10, &statuses);

  FrameMetrics fm = apzc->GetFrameMetrics();

  // It starts from (300, 300), then moves the focus point from (250, 350) to
  // (200, 300) pans by (50, 50) screen pixels, but there is a 2x zoom, which
  // causes the scroll offset to change by half of that (25, 25) pixels.
  EXPECT_EQ(325, fm.GetScrollOffset().x);
  EXPECT_EQ(325, fm.GetScrollOffset().y);
  EXPECT_EQ(2.0, fm.GetZoom().ToScaleFactor().scale);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_APZZoom_Disabled) {
  SCOPED_GFX_PREF(APZAllowZooming, bool, false);

  FrameMetrics originalMetrics = GetPinchableFrameMetrics();
  apzc->SetFrameMetrics(originalMetrics);

  // When APZAllowZooming is false, the ZoomConstraintsClient produces
  // ZoomConstraints with mAllowZoom set to false.
  MakeApzcUnzoomable();

  // With APZAllowZooming false, we expect the NotifyPinchGesture function to
  // get called as the pinch progresses, but the metrics shouldn't change.
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_START, apzc->GetGuid(), LayoutDeviceCoord(0), _)).Times(1);
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_SCALE, apzc->GetGuid(), _, _)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_END, apzc->GetGuid(), LayoutDeviceCoord(0), _)).Times(1);

  int touchInputId = 0;
  uint64_t blockId = 0;
  PinchWithTouchInput(apzc, ScreenIntPoint(250, 300), 1.25, touchInputId,
      nullptr, nullptr, &blockId);

  // verify the metrics didn't change (i.e. the pinch was ignored inside APZ)
  FrameMetrics fm = apzc->GetFrameMetrics();
  EXPECT_EQ(originalMetrics.GetZoom(), fm.GetZoom());
  EXPECT_EQ(originalMetrics.GetScrollOffset().x, fm.GetScrollOffset().x);
  EXPECT_EQ(originalMetrics.GetScrollOffset().y, fm.GetScrollOffset().y);

  apzc->AssertStateIsReset();
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_NoSpan) {
  SCOPED_GFX_PREF(APZAllowZooming, bool, false);
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);

  FrameMetrics originalMetrics = GetPinchableFrameMetrics();
  apzc->SetFrameMetrics(originalMetrics);

  // When APZAllowZooming is false, the ZoomConstraintsClient produces
  // ZoomConstraints with mAllowZoom set to false.
  MakeApzcUnzoomable();

  // With APZAllowZooming false, we expect the NotifyPinchGesture function to
  // get called as the pinch progresses, but the metrics shouldn't change.
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_START, apzc->GetGuid(), LayoutDeviceCoord(0), _)).Times(1);
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_SCALE, apzc->GetGuid(), _, _)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, NotifyPinchGesture(PinchGestureInput::PINCHGESTURE_END, apzc->GetGuid(), LayoutDeviceCoord(0), _)).Times(1);

  int inputId = 0;
  ScreenIntPoint focus(250, 300);

  // Do a pinch holding a zero span and moving the focus by y=100

  MultiTouchInput mtiStart = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId, focus));
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, focus));
  apzc->ReceiveInputEvent(mtiStart, nullptr);

  focus.y -= 35 + 1; // this is to get over the PINCH_START_THRESHOLD in GestureEventListener.cpp
  MultiTouchInput mtiMove1 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId, focus));
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, focus));
  apzc->ReceiveInputEvent(mtiMove1, nullptr);

  focus.y -= 100; // do a two-finger scroll of 100 screen pixels
  MultiTouchInput mtiMove2 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId, focus));
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, focus));
  apzc->ReceiveInputEvent(mtiMove2, nullptr);

  MultiTouchInput mtiEnd = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId, focus));
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, focus));
  apzc->ReceiveInputEvent(mtiEnd, nullptr);

  // Done, check the metrics to make sure we scrolled by 100 screen pixels,
  // which is 50 CSS pixels for the pinchable frame metrics.

  FrameMetrics fm = apzc->GetFrameMetrics();
  EXPECT_EQ(originalMetrics.GetZoom(), fm.GetZoom());
  EXPECT_EQ(originalMetrics.GetScrollOffset().x, fm.GetScrollOffset().x);
  EXPECT_EQ(originalMetrics.GetScrollOffset().y + 50, fm.GetScrollOffset().y);

  apzc->AssertStateIsReset();
}

TEST_F(APZCPinchTester, Pinch_TwoFinger_APZZoom_Disabled_Bug1354185) {
  // Set up APZ such that mZoomConstraints.mAllowZoom is false.
  SCOPED_GFX_PREF(APZAllowZooming, bool, false);
  apzc->SetFrameMetrics(GetPinchableFrameMetrics());
  MakeApzcUnzoomable();

  // We expect a repaint request for scrolling.
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // Send only the PINCHGESTURE_START and PINCHGESTURE_SCALE events,
  // in order to trigger a call to AsyncPanZoomController::OnScale
  // but not to AsyncPanZoomController::OnScaleEnd.
  ScreenIntPoint aFocus(250, 350);
  ScreenIntPoint aSecondFocus(200, 300);
  float aScale = 10;
  apzc->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                              aFocus, 10.0, 10.0),
      nullptr);

  apzc->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                              aSecondFocus, 10.0 * aScale, 10.0),
      nullptr);
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Free) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 0); // PINCH_FREE

  twoFingerPan();
  EXPECT_FALSE(isPinchLockActive());
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Normal_Lock) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 1); // PINCH_NORMAL

  twoFingerPan();
  EXPECT_TRUE(isPinchLockActive());
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Normal_Lock_Break) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 1); // PINCH_NORMAL

  twoFingerPan();
  twoFingerZoom();
  EXPECT_TRUE(isPinchLockActive());
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Sticky_Lock) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 2); // PINCH_STICKY

  twoFingerPan();
  EXPECT_TRUE(isPinchLockActive());
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Sticky_Lock_Break) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 2); // PINCH_STICKY

  twoFingerPan();
  twoFingerZoom();
  EXPECT_FALSE(isPinchLockActive());
}

TEST_F(APZCPinchLockingTester, Pinch_Locking_Sticky_Lock_Break_Lock) {
  SCOPED_GFX_PREF(APZPinchLockMode, int32_t, 2); // PINCH_STICKY

  twoFingerPan();
  twoFingerZoom();
  twoFingerPan();
  EXPECT_TRUE(isPinchLockActive());
}

