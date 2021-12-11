/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <iostream>
#include <string>

#include "AccessibleCaretManager.h"

#include "mozilla/AccessibleCaretEventHub.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;

// -----------------------------------------------------------------------------
// This file test the state transitions of AccessibleCaretEventHub under
// various combination of events and callbacks.

namespace mozilla {

class MockAccessibleCaretManager : public AccessibleCaretManager {
 public:
  MockAccessibleCaretManager() : AccessibleCaretManager(nullptr) {}

  MOCK_METHOD2(PressCaret,
               nsresult(const nsPoint& aPoint, EventClassID aEventClass));
  MOCK_METHOD1(DragCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD0(ReleaseCaret, nsresult());
  MOCK_METHOD1(TapCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD1(SelectWordOrShortcut, nsresult(const nsPoint& aPoint));
  MOCK_METHOD0(OnScrollStart, void());
  MOCK_METHOD0(OnScrollEnd, void());
  MOCK_METHOD0(OnScrollPositionChanged, void());
  MOCK_METHOD0(OnBlur, void());
};

class MockAccessibleCaretEventHub : public AccessibleCaretEventHub {
 public:
  using AccessibleCaretEventHub::DragCaretState;
  using AccessibleCaretEventHub::LongTapState;
  using AccessibleCaretEventHub::NoActionState;
  using AccessibleCaretEventHub::PressCaretState;
  using AccessibleCaretEventHub::PressNoCaretState;
  using AccessibleCaretEventHub::ScrollState;

  MockAccessibleCaretEventHub() : AccessibleCaretEventHub(nullptr) {
    mManager = MakeUnique<MockAccessibleCaretManager>();
    mInitialized = true;
  }

  nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                int32_t aIdentifier) const override {
    // Return the device point directly.
    LayoutDeviceIntPoint touchIntPoint = aEvent->mTouches[0]->mRefPoint;
    return nsPoint(touchIntPoint.x, touchIntPoint.y);
  }

  nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent) const override {
    // Return the device point directly.
    LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->mRefPoint;
    return nsPoint(mouseIntPoint.x, mouseIntPoint.y);
  }

  MockAccessibleCaretManager* GetMockAccessibleCaretManager() {
    return static_cast<MockAccessibleCaretManager*>(mManager.get());
  }
};

// Print the name of the state for debugging.
static ::std::ostream& operator<<(
    ::std::ostream& aOstream,
    const MockAccessibleCaretEventHub::State* aState) {
  return aOstream << aState->Name();
}

class AccessibleCaretEventHubTester : public ::testing::Test {
 public:
  AccessibleCaretEventHubTester() {
    DefaultValue<nsresult>::Set(NS_OK);
    EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());

    // AccessibleCaretEventHub requires the caller to hold a ref to it. We just
    // add ref here for the sake of convenience.
    mHub.get()->AddRef();
  }

  ~AccessibleCaretEventHubTester() override {
    // Release the ref added in the constructor.
    mHub.get()->Release();
  }

  static UniquePtr<WidgetEvent> CreateMouseEvent(EventMessage aMessage,
                                                 nscoord aX, nscoord aY) {
    auto event = MakeUnique<WidgetMouseEvent>(true, aMessage, nullptr,
                                              WidgetMouseEvent::eReal);

    event->mButton = MouseButton::ePrimary;
    event->mRefPoint = LayoutDeviceIntPoint(aX, aY);

    return std::move(event);
  }

  static UniquePtr<WidgetEvent> CreateMousePressEvent(nscoord aX, nscoord aY) {
    return CreateMouseEvent(eMouseDown, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseMoveEvent(nscoord aX, nscoord aY) {
    return CreateMouseEvent(eMouseMove, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseReleaseEvent(nscoord aX,
                                                        nscoord aY) {
    return CreateMouseEvent(eMouseUp, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateLongTapEvent(nscoord aX, nscoord aY) {
    return CreateMouseEvent(eMouseLongTap, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchEvent(EventMessage aMessage,
                                                 nscoord aX, nscoord aY) {
    auto event = MakeUnique<WidgetTouchEvent>(true, aMessage, nullptr);
    int32_t identifier = 0;
    LayoutDeviceIntPoint point(aX, aY);
    LayoutDeviceIntPoint radius(19, 19);
    float rotationAngle = 0;
    float force = 1;

    RefPtr<dom::Touch> touch(
        new dom::Touch(identifier, point, radius, rotationAngle, force));
    event->mTouches.AppendElement(touch);

    return std::move(event);
  }

  static UniquePtr<WidgetEvent> CreateTouchStartEvent(nscoord aX, nscoord aY) {
    return CreateTouchEvent(eTouchStart, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchMoveEvent(nscoord aX, nscoord aY) {
    return CreateTouchEvent(eTouchMove, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchEndEvent(nscoord aX, nscoord aY) {
    return CreateTouchEvent(eTouchEnd, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchCancelEvent(nscoord aX, nscoord aY) {
    return CreateTouchEvent(eTouchCancel, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateWheelEvent(EventMessage aMessage) {
    auto event = MakeUnique<WidgetWheelEvent>(true, aMessage, nullptr);

    return std::move(event);
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestAsyncPanZoomScroll();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void HandleEventAndCheckState(
      UniquePtr<WidgetEvent> aEvent,
      MockAccessibleCaretEventHub::State* aExpectedState,
      nsEventStatus aExpectedEventStatus) {
    RefPtr<MockAccessibleCaretEventHub> hub(mHub);
    nsEventStatus rv = hub->HandleEvent(aEvent.get());
    EXPECT_EQ(hub->GetState(), aExpectedState);
    EXPECT_EQ(rv, aExpectedEventStatus);
  }

  void CheckState(MockAccessibleCaretEventHub::State* aExpectedState) {
    EXPECT_EQ(mHub->GetState(), aExpectedState);
  }

  template <typename PressEventCreator, typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestPressReleaseOnNoCaret(
      PressEventCreator aPressEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestPressReleaseOnCaret(
      PressEventCreator aPressEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestPressMoveReleaseOnNoCaret(
      PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestPressMoveReleaseOnCaret(
      PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestLongTapWithSelectWordSuccessful(
      PressEventCreator aPressEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestLongTapWithSelectWordFailed(
      PressEventCreator aPressEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void TestEventDrivenAsyncPanZoomScroll(
      PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
      ReleaseEventCreator aReleaseEventCreator);

  // Member variables
  RefPtr<MockAccessibleCaretEventHub> mHub{new MockAccessibleCaretEventHub()};

};  // class AccessibleCaretEventHubTester

TEST_F(AccessibleCaretEventHubTester, TestMousePressReleaseOnNoCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressReleaseOnNoCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressReleaseOnNoCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressReleaseOnNoCaret(CreateTouchStartEvent, CreateTouchEndEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestPressReleaseOnNoCaret(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
      .WillOnce(Return(NS_ERROR_FAILURE));

  EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), ReleaseCaret()).Times(0);

  EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), TapCaret(_)).Times(0);

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestMousePressReleaseOnCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressReleaseOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressReleaseOnCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressReleaseOnCaret(CreateTouchStartEvent, CreateTouchEndEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestPressReleaseOnCaret(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
        .Times(0);

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), ReleaseCaret());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), TapCaret(_));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);
}

TEST_F(AccessibleCaretEventHubTester, TestMousePressMoveReleaseOnNoCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressMoveReleaseOnNoCaret(CreateMousePressEvent, CreateMouseMoveEvent,
                                CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressMoveReleaseOnNoCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressMoveReleaseOnNoCaret(CreateTouchStartEvent, CreateTouchMoveEvent,
                                CreateTouchEndEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestPressMoveReleaseOnNoCaret(
    PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_)).Times(0);
  }

  HandleEventAndCheckState(aPressEventCreator(x0, y0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  // A small move with the distance between (x0, y0) and (x1, y1) below the
  // tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x1, y1),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  // A large move to simulate a dragging to select text since the distance
  // between (x0, y0) and (x2, y2) is above the tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x2, y2),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(x3, y3),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestMousePressMoveReleaseOnCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressMoveReleaseOnCaret(CreateMousePressEvent, CreateMouseMoveEvent,
                              CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressMoveReleaseOnCaret)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestPressMoveReleaseOnCaret(CreateTouchStartEvent, CreateTouchMoveEvent,
                              CreateTouchEndEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestPressMoveReleaseOnCaret(
    PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_))
        .Times(2)  // two valid drag operations
        .WillRepeatedly(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), ReleaseCaret())
        .WillOnce(Return(NS_OK));
  }

  HandleEventAndCheckState(aPressEventCreator(x0, y0),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // A small move with the distance between (x0, y0) and (x1, y1) below the
  // tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x1, y1),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // A large move forms a valid drag since the distance between (x0, y0) and
  // (x2, y2) is above the tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x2, y2),
                           MockAccessibleCaretEventHub::DragCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  // Also a valid drag since the distance between (x0, y0) and (x3, y3) above
  // the tolerance value even if the distance between (x2, y2) and (x3, y3) is
  // below the tolerance value.
  HandleEventAndCheckState(aMoveEventCreator(x3, y3),
                           MockAccessibleCaretEventHub::DragCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(x3, y3),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);
}

TEST_F(AccessibleCaretEventHubTester,
       TestTouchStartMoveEndOnCaretWithTouchCancelIgnored)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), ReleaseCaret())
        .WillOnce(Return(NS_OK));
  }

  // All the eTouchCancel events should be ignored in this test.

  HandleEventAndCheckState(CreateTouchStartEvent(x0, y0),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(CreateTouchCancelEvent(x0, y0),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eIgnore);

  // A small move with the distance between (x0, y0) and (x1, y1) below the
  // tolerance value.
  HandleEventAndCheckState(CreateTouchMoveEvent(x1, y1),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(CreateTouchCancelEvent(x1, y1),
                           MockAccessibleCaretEventHub::PressCaretState(),
                           nsEventStatus_eIgnore);

  // A large move forms a valid drag since the distance between (x0, y0) and
  // (x2, y2) is above the tolerance value.
  HandleEventAndCheckState(CreateTouchMoveEvent(x2, y2),
                           MockAccessibleCaretEventHub::DragCaretState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(CreateTouchCancelEvent(x2, y2),
                           MockAccessibleCaretEventHub::DragCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateTouchEndEvent(x3, y3),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(CreateTouchCancelEvent(x3, y3),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestMouseLongTapWithSelectWordSuccessful)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestLongTapWithSelectWordSuccessful(CreateMousePressEvent,
                                      CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchLongTapWithSelectWordSuccessful)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestLongTapWithSelectWordSuccessful(CreateTouchStartEvent,
                                      CreateTouchEndEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestLongTapWithSelectWordSuccessful(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(check, Call("longtap with scrolling"));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
        .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());
  }

  // Test long tap without scrolling.
  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockAccessibleCaretEventHub::LongTapState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  // On Fennec, after long tap, the script might scroll and zoom the input field
  // to the center of the screen to make typing easier before the user lifts the
  // finger.
  check.Call("longtap with scrolling");

  HandleEventAndCheckState(aPressEventCreator(1, 1),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(1, 1),
                           MockAccessibleCaretEventHub::LongTapState(),
                           nsEventStatus_eIgnore);

  RefPtr<MockAccessibleCaretEventHub> hub(mHub);
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());

  HandleEventAndCheckState(aReleaseEventCreator(1, 1),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestMouseLongTapWithSelectWordFailed)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestLongTapWithSelectWordFailed(CreateMousePressEvent,
                                  CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchLongTapWithSelectWordFailed)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestLongTapWithSelectWordFailed(CreateTouchStartEvent, CreateTouchEndEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestLongTapWithSelectWordFailed(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
        .WillOnce(Return(NS_ERROR_FAILURE));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockAccessibleCaretEventHub::LongTapState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchEventDrivenAsyncPanZoomScroll)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestEventDrivenAsyncPanZoomScroll(CreateTouchStartEvent, CreateTouchMoveEvent,
                                    CreateTouchEndEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestMouseEventDrivenAsyncPanZoomScroll)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  TestEventDrivenAsyncPanZoomScroll(CreateMousePressEvent, CreateMouseMoveEvent,
                                    CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void AccessibleCaretEventHubTester::TestEventDrivenAsyncPanZoomScroll(
    PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
    ReleaseEventCreator aReleaseEventCreator) {
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_)).Times(0);

    EXPECT_CALL(check, Call("1"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());

    EXPECT_CALL(check, Call("2"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_, _))
        .WillOnce(Return(NS_ERROR_FAILURE));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_)).Times(0);

    EXPECT_CALL(check, Call("3"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());
  }

  // Receive press event.
  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aMoveEventCreator(100, 100),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  check.Call("1");

  // Event driven scroll started
  RefPtr<MockAccessibleCaretEventHub> hub(mHub);
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  HandleEventAndCheckState(aMoveEventCreator(160, 160),
                           MockAccessibleCaretEventHub::ScrollState(),
                           nsEventStatus_eIgnore);

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  // Event driven scroll ended
  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());

  HandleEventAndCheckState(aReleaseEventCreator(210, 210),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  check.Call("2");

  // Receive another press event.
  HandleEventAndCheckState(aPressEventCreator(220, 220),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aMoveEventCreator(230, 230),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  check.Call("3");

  // Another APZ scroll started
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  // Another APZ scroll ended
  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());

  HandleEventAndCheckState(aReleaseEventCreator(310, 310),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestAsyncPanZoomScroll)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION { TestAsyncPanZoomScroll(); }

void AccessibleCaretEventHubTester::TestAsyncPanZoomScroll() {
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(check, Call("1"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(),
                OnScrollPositionChanged());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());

    EXPECT_CALL(check, Call("2"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(),
                OnScrollPositionChanged());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());
  }

  // First APZ scrolling.
  check.Call("1");

  RefPtr<MockAccessibleCaretEventHub> hub(mHub);
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());

  // Second APZ scrolling.
  check.Call("2");

  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

TEST_F(AccessibleCaretEventHubTester, TestAsyncPanZoomScrollStartedThenBlur)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd()).Times(0);
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnBlur());
  }

  RefPtr<MockAccessibleCaretEventHub> hub(mHub);
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->NotifyBlur(true);
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

TEST_F(AccessibleCaretEventHubTester, TestAsyncPanZoomScrollEndedThenBlur)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnBlur());
  }

  RefPtr<MockAccessibleCaretEventHub> hub(mHub);
  hub->AsyncPanZoomStarted();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->ScrollPositionChanged();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  hub->AsyncPanZoomStopped();
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());

  hub->NotifyBlur(true);
  EXPECT_EQ(hub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

}  // namespace mozilla
