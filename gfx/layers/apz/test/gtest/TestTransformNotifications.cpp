/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCBasicTester.h"
#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"
#include "apz/util/APZEventState.h"

#include "InputUtils.h"

class APZCTransformNotificationTester : public APZCTreeManagerTester {
 public:
  explicit APZCTransformNotificationTester() { CreateMockHitTester(); }

  UniquePtr<ScopedLayerTreeRegistration> mRegistration;

  RefPtr<TestAsyncPanZoomController> mRootApzc;

  void SetupBasicTest() {
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 500, 500));

    mRegistration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

    UpdateHitTestingTree();

    mRootApzc = ApzcOf(root);
  }

  void SetupNonScrollableTest() {
    const char* treeShape = "x";
    LayerIntRect layerVisibleRect[] = {
        LayerIntRect(0, 0, 100, 100),
    };
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 100, 100));

    mRegistration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);

    UpdateHitTestingTree();

    mRootApzc = ApzcOf(root);

    mRootApzc->GetFrameMetrics().SetIsRootContent(true);
  }
};

TEST_F(APZCTransformNotificationTester, PanningTransformNotifications) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  SetupBasicTest();

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
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartTouch, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eStartPanning, _, _))
        .Times(1);
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Complex pan"));
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartTouch, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eStartPanning, _, _))
        .Times(1);
    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Simple pan");
  Pan(mRootApzc, 50, 25, PanOptions::NoFling);
  check.Call("Complex pan");
  Pan(mRootApzc, 25, 45);
  mRootApzc->AdvanceAnimationsUntilEnd();
  check.Call("Done");
}

TEST_F(APZCTransformNotificationTester, PanWithMomentumTransformNotifications) {
  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Panning"));
    EXPECT_CALL(check, Call("Pan End"));
    EXPECT_CALL(check, Call("Momentum Start"));

    EXPECT_CALL(check, Call("Momentum Pan"));
    EXPECT_CALL(check, Call("Momentum End"));
    // The TransformEnd should only be sent after the momentum pan.
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Panning");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Momentum Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMSTART, manager,
             ScreenIntPoint(50, 50), ScreenPoint(30, 90), mcc->Time());
  mcc->AdvanceByMillis(10);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Momentum Pan");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMPAN, manager,
             ScreenIntPoint(50, 50), ScreenPoint(10, 30), mcc->Time());
  mcc->AdvanceByMillis(10);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Momentum End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_MOMENTUMEND, manager,
             ScreenIntPoint(50, 50), ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(10);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Done");
}

TEST_F(APZCTransformNotificationTester,
       PanWithoutMomentumTransformNotifications) {
  // Ensure that the TransformEnd delay is 100ms.
  SCOPED_GFX_PREF_INT("apz.scrollend-event.content.delay_ms", 100);

  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Panning"));
    EXPECT_CALL(check, Call("Pan End"));
    EXPECT_CALL(check, Call("TransformEnd delay"));
    // The TransformEnd should only be sent after the pan gesture and 100ms
    // timer fire.
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Panning");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(55);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("TransformEnd delay");
  mcc->AdvanceByMillis(55);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Done");
}

TEST_F(APZCTransformNotificationTester,
       PanFollowedByNewPanTransformNotifications) {
  // Ensure that the TransformEnd delay is 100ms.
  SCOPED_GFX_PREF_INT("apz.scrollend-event.content.delay_ms", 100);

  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Panning"));
    EXPECT_CALL(check, Call("Pan End"));
    // The TransformEnd delay should be cut short and delivered before the
    // new pan gesture begins.
    EXPECT_CALL(check, Call("New Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("New Pan End"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Panning");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(55);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("New Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("New Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(105);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Done");
}

TEST_F(APZCTransformNotificationTester,
       PanFollowedByWheelTransformNotifications) {
  // Needed because the test uses SmoothWheel()
  SCOPED_GFX_PREF_BOOL("general.smoothScroll", true);
  // Ensure that the TransformEnd delay is 100ms.
  SCOPED_GFX_PREF_INT("apz.scrollend-event.content.delay_ms", 100);

  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Panning"));
    EXPECT_CALL(check, Call("Pan End"));
    // The TransformEnd delay should be cut short and delivered before the
    // new wheel event begins.
    EXPECT_CALL(check, Call("Wheel Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Wheel End"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Panning");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, 30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(55);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Wheel Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  SmoothWheel(manager, ScreenIntPoint(50, 50), ScreenPoint(10, 10),
              mcc->Time());
  mcc->AdvanceByMillis(10);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Wheel End");

  mRootApzc->AdvanceAnimationsUntilEnd();

  check.Call("Done");
}

#ifndef MOZ_WIDGET_ANDROID  // Currently fails on Android
TEST_F(APZCTransformNotificationTester, PanOverscrollTransformNotifications) {
  SCOPED_GFX_PREF_BOOL("apz.overscroll.enabled", true);

  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Pan Start"));
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformBegin, _, _))
        .Times(1);

    EXPECT_CALL(check, Call("Panning Into Overscroll"));
    EXPECT_CALL(check, Call("Pan End"));
    EXPECT_CALL(check, Call("Overscroll Animation End"));
    // The TransformEnd should only be sent after the overscroll animation
    // completes.
    EXPECT_CALL(
        *mcc,
        NotifyAPZStateChange(
            _, GeckoContentController::APZStateChange::eTransformEnd, _, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Pan Start");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_START, manager, ScreenIntPoint(50, 50),
             ScreenIntPoint(1, 2), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Panning Into Overscroll");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_PAN, mRootApzc, ScreenIntPoint(50, 50),
             ScreenPoint(15, -30), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  // Ensure that we have overscrolled.
  EXPECT_TRUE(mRootApzc->IsOverscrolled());

  check.Call("Pan End");
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  PanGesture(PanGestureInput::PANGESTURE_END, manager, ScreenIntPoint(50, 50),
             ScreenPoint(0, 0), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  // Wait for the overscroll animation to complete and the TransformEnd
  // notification to be sent.
  check.Call("Overscroll Animation End");
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimationsUntilEnd();
  EXPECT_FALSE(mRootApzc->IsOverscrolled());

  check.Call("Done");
}
#endif

TEST_F(APZCTransformNotificationTester, ScrollableTouchStateChange) {
  // Create a scroll frame with available space for a scroll.
  SetupBasicTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    EXPECT_CALL(check, Call("Start"));
    // We receive a touch-start with the flag indicating that the
    // touch-start occurred over a scrollable element.
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartTouch, 1, _))
        .Times(1);

    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, 1, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Start");

  // Conduct a touch down and touch up in the scrollable element,
  // and ensure the correct state change notifications are sent.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  TouchDown(mRootApzc, ScreenIntPoint(10, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  TouchUp(mRootApzc, ScreenIntPoint(10, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Done");
}

TEST_F(APZCTransformNotificationTester, NonScrollableTouchStateChange) {
  // Create a non-scrollable frame with no space to scroll.
  SetupNonScrollableTest();

  MockFunction<void(std::string checkPointName)> check;
  {
    EXPECT_CALL(check, Call("Start"));
    // We receive a touch-start with the flag indicating that the
    // touch-start occurred over a non-scrollable element.
    EXPECT_CALL(
        *mcc, NotifyAPZStateChange(
                  _, GeckoContentController::APZStateChange::eStartTouch, 0, _))
        .Times(1);

    EXPECT_CALL(*mcc,
                NotifyAPZStateChange(
                    _, GeckoContentController::APZStateChange::eEndTouch, 1, _))
        .Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  check.Call("Start");

  // Conduct a touch down and touch up in the non-scrollable element,
  // and ensure the correct state change notifications are sent.
  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  TouchDown(mRootApzc, ScreenIntPoint(10, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  QueueMockHitResult(ScrollableLayerGuid::START_SCROLL_ID);
  TouchUp(mRootApzc, ScreenIntPoint(10, 10), mcc->Time());
  mcc->AdvanceByMillis(5);
  mRootApzc->AdvanceAnimations(mcc->GetSampleTime());

  check.Call("Done");
}
