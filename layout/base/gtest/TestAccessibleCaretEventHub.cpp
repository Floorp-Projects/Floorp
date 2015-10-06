/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <iostream>
#include <string>

#include "AccessibleCaretEventHub.h"
#include "AccessibleCaretManager.h"
#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::_;

// -----------------------------------------------------------------------------
// This file test the state transitions of AccessibleCaretEventHub under
// various combination of events and callbacks.

namespace mozilla
{

class MockAccessibleCaretManager : public AccessibleCaretManager
{
public:
  explicit MockAccessibleCaretManager()
    : AccessibleCaretManager(nullptr)
  {
  }

  MOCK_METHOD1(PressCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD1(DragCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD0(ReleaseCaret, nsresult());
  MOCK_METHOD1(TapCaret, nsresult(const nsPoint& aPoint));
  MOCK_METHOD1(SelectWordOrShortcut, nsresult(const nsPoint& aPoint));
  MOCK_METHOD0(OnScrollStart, void());
  MOCK_METHOD0(OnScrollEnd, void());
  MOCK_METHOD0(OnScrollPositionChanged, void());
  MOCK_METHOD0(OnBlur, void());
};

class MockAccessibleCaretEventHub : public AccessibleCaretEventHub
{
public:
  using AccessibleCaretEventHub::NoActionState;
  using AccessibleCaretEventHub::PressCaretState;
  using AccessibleCaretEventHub::DragCaretState;
  using AccessibleCaretEventHub::PressNoCaretState;
  using AccessibleCaretEventHub::ScrollState;
  using AccessibleCaretEventHub::PostScrollState;
  using AccessibleCaretEventHub::FireScrollEnd;

  explicit MockAccessibleCaretEventHub()
  {
    mManager = MakeUnique<MockAccessibleCaretManager>();
    mInitialized = true;
  }

  virtual nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                        int32_t aIdentifier) const override
  {
    // Return the device point directly.
    LayoutDeviceIntPoint touchIntPoint = aEvent->touches[0]->mRefPoint;
    return nsPoint(touchIntPoint.x, touchIntPoint.y);
  }

  virtual nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent) const override
  {
    // Return the device point directly.
    LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->refPoint;
    return nsPoint(mouseIntPoint.x, mouseIntPoint.y);
  }

  MockAccessibleCaretManager* GetMockAccessibleCaretManager()
  {
    return static_cast<MockAccessibleCaretManager*>(mManager.get());
  }
};

// Print the name of the state for debugging.
::std::ostream& operator<<(::std::ostream& aOstream,
                           const MockAccessibleCaretEventHub::State* aState)
{
  return aOstream << aState->Name();
}

class AccessibleCaretEventHubTester : public ::testing::Test
{
public:
  explicit AccessibleCaretEventHubTester()
  {
    DefaultValue<nsresult>::Set(NS_OK);
    EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());
  }

  static UniquePtr<WidgetEvent> CreateMouseEvent(EventMessage aMessage,
                                                 nscoord aX,
                                                 nscoord aY)
  {
    auto event = MakeUnique<WidgetMouseEvent>(true, aMessage, nullptr,
                                              WidgetMouseEvent::eReal);

    event->button = WidgetMouseEvent::eLeftButton;
    event->refPoint = LayoutDeviceIntPoint(aX, aY);

    return Move(event);
  }

  static UniquePtr<WidgetEvent> CreateMousePressEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(eMouseDown, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseMoveEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(eMouseMove, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateMouseReleaseEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(eMouseUp, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateLongTapEvent(nscoord aX, nscoord aY)
  {
    return CreateMouseEvent(eMouseLongTap, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchEvent(EventMessage aMessage,
                                                 nscoord aX,
                                                 nscoord aY)
  {
    auto event = MakeUnique<WidgetTouchEvent>(true, aMessage, nullptr);
    int32_t identifier = 0;
    LayoutDeviceIntPoint point(aX, aY);
    nsIntPoint radius(19, 19);
    float rotationAngle = 0;
    float force = 1;

    nsRefPtr<dom::Touch> touch(
      new dom::Touch(identifier, point, radius, rotationAngle, force));
    event->touches.AppendElement(touch);

    return Move(event);
  }

  static UniquePtr<WidgetEvent> CreateTouchPressEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(eTouchStart, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchMoveEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(eTouchMove, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateTouchReleaseEvent(nscoord aX, nscoord aY)
  {
    return CreateTouchEvent(eTouchEnd, aX, aY);
  }

  static UniquePtr<WidgetEvent> CreateWheelEvent(EventMessage aMessage)
  {
    auto event = MakeUnique<WidgetWheelEvent>(true, aMessage, nullptr);

    return Move(event);
  }

  void HandleEventAndCheckState(UniquePtr<WidgetEvent> aEvent,
                                MockAccessibleCaretEventHub::State* aExpectedState,
                                nsEventStatus aExpectedEventStatus)
  {
    nsEventStatus rv = mHub->HandleEvent(aEvent.get());
    EXPECT_EQ(mHub->GetState(), aExpectedState);
    EXPECT_EQ(rv, aExpectedEventStatus);
  }

  void CheckState(MockAccessibleCaretEventHub::State* aExpectedState)
  {
    EXPECT_EQ(mHub->GetState(), aExpectedState);
  }

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestPressReleaseOnNoCaret(PressEventCreator aPressEventCreator,
                                 ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestPressReleaseOnCaret(PressEventCreator aPressEventCreator,
                               ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  void TestPressMoveReleaseOnNoCaret(PressEventCreator aPressEventCreator,
                                     MoveEventCreator aMoveEventCreator,
                                     ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  void TestPressMoveReleaseOnCaret(PressEventCreator aPressEventCreator,
                                   MoveEventCreator aMoveEventCreator,
                                   ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestLongTapWithSelectWordSuccessful(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename ReleaseEventCreator>
  void TestLongTapWithSelectWordFailed(
    PressEventCreator aPressEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  template <typename PressEventCreator, typename MoveEventCreator,
            typename ReleaseEventCreator>
  void TestEventDrivenAsyncPanZoomScroll(
    PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
    ReleaseEventCreator aReleaseEventCreator);

  // Member variables
  nsRefPtr<MockAccessibleCaretEventHub> mHub{new MockAccessibleCaretEventHub()};

}; // class AccessibleCaretEventHubTester

TEST_F(AccessibleCaretEventHubTester, TestMousePressReleaseOnNoCaret)
{
  TestPressReleaseOnNoCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressReleaseOnNoCaret)
{
  TestPressReleaseOnNoCaret(CreateTouchPressEvent, CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestPressReleaseOnNoCaret(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
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
{
  TestPressReleaseOnCaret(CreateMousePressEvent, CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressReleaseOnCaret)
{
  TestPressReleaseOnCaret(CreateTouchPressEvent, CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestPressReleaseOnCaret(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
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
{
  TestPressMoveReleaseOnNoCaret(CreateMousePressEvent, CreateMouseMoveEvent,
                                CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressMoveReleaseOnNoCaret)
{
  TestPressMoveReleaseOnNoCaret(CreateTouchPressEvent, CreateTouchMoveEvent,
                                CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestPressMoveReleaseOnNoCaret(
  PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
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
{
  TestPressMoveReleaseOnCaret(CreateMousePressEvent, CreateMouseMoveEvent,
                              CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchPressMoveReleaseOnCaret)
{
  TestPressMoveReleaseOnCaret(CreateTouchPressEvent, CreateTouchMoveEvent,
                              CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestPressMoveReleaseOnCaret(
  PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  nscoord x0 = 0, y0 = 0;
  nscoord x1 = 100, y1 = 100;
  nscoord x2 = 300, y2 = 300;
  nscoord x3 = 400, y3 = 400;

  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
      .WillOnce(Return(NS_OK));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_))
      .Times(2) // two valid drag operations
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

TEST_F(AccessibleCaretEventHubTester, TestMouseLongTapWithSelectWordSuccessful)
{
  TestLongTapWithSelectWordSuccessful(CreateMousePressEvent,
                                      CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchLongTapWithSelectWordSuccessful)
{
  TestLongTapWithSelectWordSuccessful(CreateTouchPressEvent,
                                      CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestLongTapWithSelectWordSuccessful(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
      .WillOnce(Return(NS_OK));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eConsumeNoDefault);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestMouseLongTapWithSelectWordFailed)
{
  TestLongTapWithSelectWordFailed(CreateMousePressEvent,
                                  CreateMouseReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchLongTapWithSelectWordFailed)
{
  TestLongTapWithSelectWordFailed(CreateTouchPressEvent,
                                  CreateTouchReleaseEvent);
}

template <typename PressEventCreator, typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestLongTapWithSelectWordFailed(
  PressEventCreator aPressEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), SelectWordOrShortcut(_))
      .WillOnce(Return(NS_ERROR_FAILURE));
  }

  HandleEventAndCheckState(aPressEventCreator(0, 0),
                           MockAccessibleCaretEventHub::PressNoCaretState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(CreateLongTapEvent(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);

  HandleEventAndCheckState(aReleaseEventCreator(0, 0),
                           MockAccessibleCaretEventHub::NoActionState(),
                           nsEventStatus_eIgnore);
}

TEST_F(AccessibleCaretEventHubTester, TestTouchEventDrivenAsyncPanZoomScroll)
{
  TestEventDrivenAsyncPanZoomScroll(CreateTouchPressEvent, CreateTouchMoveEvent,
                                    CreateTouchReleaseEvent);
}

TEST_F(AccessibleCaretEventHubTester, TestMouseEventDrivenAsyncPanZoomScroll)
{
  TestEventDrivenAsyncPanZoomScroll(CreateMousePressEvent, CreateMouseMoveEvent,
                                    CreateMouseReleaseEvent);
}

template <typename PressEventCreator, typename MoveEventCreator,
          typename ReleaseEventCreator>
void
AccessibleCaretEventHubTester::TestEventDrivenAsyncPanZoomScroll(
  PressEventCreator aPressEventCreator, MoveEventCreator aMoveEventCreator,
  ReleaseEventCreator aReleaseEventCreator)
{
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_)).Times(0);

    EXPECT_CALL(check, Call("1"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());

    EXPECT_CALL(check, Call("2"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), PressCaret(_))
      .WillOnce(Return(NS_ERROR_FAILURE));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), DragCaret(_)).Times(0);

    EXPECT_CALL(check, Call("3"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());

    EXPECT_CALL(check, Call("4"));
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
  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  HandleEventAndCheckState(aMoveEventCreator(160, 160),
                           MockAccessibleCaretEventHub::ScrollState(),
                           nsEventStatus_eIgnore);

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  // Event driven scroll ended
  mHub->AsyncPanZoomStopped();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::PostScrollState());

  HandleEventAndCheckState(aReleaseEventCreator(210, 210),
                           MockAccessibleCaretEventHub::PostScrollState(),
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
  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  // Another APZ scroll ended
  mHub->AsyncPanZoomStopped();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::PostScrollState());

  HandleEventAndCheckState(aReleaseEventCreator(310, 310),
                           MockAccessibleCaretEventHub::PostScrollState(),
                           nsEventStatus_eIgnore);

  check.Call("4");

  // Simulate scroll end fired by timer.
  MockAccessibleCaretEventHub::FireScrollEnd(nullptr, mHub);
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

TEST_F(AccessibleCaretEventHubTester, TestNoEventAsyncPanZoomScroll)
{
  MockFunction<void(::std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(check, Call("1"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(),
                OnScrollPositionChanged()).Times(0);

    EXPECT_CALL(check, Call("2"));
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd());
  }

  check.Call("1");

  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->AsyncPanZoomStopped();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::PostScrollState());

  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->AsyncPanZoomStopped();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::PostScrollState());

  check.Call("2");

  // Simulate scroll end fired by timer.
  MockAccessibleCaretEventHub::FireScrollEnd(nullptr, mHub);
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

TEST_F(AccessibleCaretEventHubTester, TestAsyncPanZoomScrollStartedThenBlur)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd()).Times(0);
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnBlur());
  }

  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->NotifyBlur(true);
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

TEST_F(AccessibleCaretEventHubTester, TestAsyncPanZoomScrollEndedThenBlur)
{
  {
    InSequence dummy;

    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollStart());
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnScrollEnd()).Times(0);
    EXPECT_CALL(*mHub->GetMockAccessibleCaretManager(), OnBlur());
  }

  mHub->AsyncPanZoomStarted();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->ScrollPositionChanged();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::ScrollState());

  mHub->AsyncPanZoomStopped();
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::PostScrollState());

  mHub->NotifyBlur(true);
  EXPECT_EQ(mHub->GetState(), MockAccessibleCaretEventHub::NoActionState());
}

} // namespace mozilla
