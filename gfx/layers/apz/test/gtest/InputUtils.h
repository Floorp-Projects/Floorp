/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InputUtils_h
#define mozilla_layers_InputUtils_h

/**
 * Defines a set of utility functions for generating input events
 * to an APZC/APZCTM during APZ gtests.
 */

#include "APZTestCommon.h"
#include "gfxPrefs.h"

/* The InputReceiver template parameter used in the helper functions below needs
 * to be a class that implements functions with the signatures:
 * nsEventStatus ReceiveInputEvent(const InputData& aEvent,
 *                                 ScrollableLayerGuid* aGuid,
 *                                 uint64_t* aOutInputBlockId);
 * void SetAllowedTouchBehavior(uint64_t aInputBlockId,
 *                              const nsTArray<uint32_t>& aBehaviours);
 * The classes that currently implement these are APZCTreeManager and
 * TestAsyncPanZoomController. Using this template allows us to test individual
 * APZC instances in isolation and also an entire APZ tree, while using the same
 * code to dispatch input events.
 */

// Some helper functions for constructing input event objects suitable to be
// passed either to an APZC (which expects an transformed point), or to an APZTM
// (which expects an untransformed point). We handle both cases by setting both
// the transformed and untransformed fields to the same value.
SingleTouchData
CreateSingleTouchData(int32_t aIdentifier, const ScreenIntPoint& aPoint)
{
  SingleTouchData touch(aIdentifier, aPoint, ScreenSize(0, 0), 0, 0);
  touch.mLocalScreenPoint = ParentLayerPoint(aPoint.x, aPoint.y);
  return touch;
}

// Convenience wrapper for CreateSingleTouchData() that takes loose coordinates.
SingleTouchData
CreateSingleTouchData(int32_t aIdentifier, ScreenIntCoord aX, ScreenIntCoord aY)
{
  return CreateSingleTouchData(aIdentifier, ScreenIntPoint(aX, aY));
}

PinchGestureInput
CreatePinchGestureInput(PinchGestureInput::PinchGestureType aType,
                        const ScreenIntPoint& aFocus,
                        float aCurrentSpan, float aPreviousSpan)
{
  ParentLayerPoint localFocus(aFocus.x, aFocus.y);
  PinchGestureInput result(aType, 0, TimeStamp(), localFocus,
                           aCurrentSpan, aPreviousSpan, 0);
  result.mFocusPoint = aFocus;
  return result;
}

template<class InputReceiver>
void
SetDefaultAllowedTouchBehavior(const RefPtr<InputReceiver>& aTarget,
                               uint64_t aInputBlockId,
                               int touchPoints = 1)
{
  nsTArray<uint32_t> defaultBehaviors;
  // use the default value where everything is allowed
  for (int i = 0; i < touchPoints; i++) {
    defaultBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                 | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                 | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM
                                 | mozilla::layers::AllowedTouchBehavior::DOUBLE_TAP_ZOOM);
  }
  aTarget->SetAllowedTouchBehavior(aInputBlockId, defaultBehaviors);
}


MultiTouchInput
CreateMultiTouchInput(MultiTouchInput::MultiTouchType aType, TimeStamp aTime)
{
  return MultiTouchInput(aType, MillisecondsSinceStartup(aTime), aTime, 0);
}

template<class InputReceiver>
nsEventStatus
TouchDown(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
          TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  MultiTouchInput mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, aOutInputBlockId);
}

template<class InputReceiver>
nsEventStatus
TouchMove(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
          TimeStamp aTime)
{
  MultiTouchInput mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template<class InputReceiver>
nsEventStatus
TouchUp(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
        TimeStamp aTime)
{
  MultiTouchInput mti = CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template<class InputReceiver>
void
PinchWithPinchInput(const RefPtr<InputReceiver>& aTarget,
                    const ScreenIntPoint& aFocus,
                    const ScreenIntPoint& aSecondFocus, float aScale,
                    nsEventStatus (*aOutEventStatuses)[3] = nullptr)
{
  nsEventStatus actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                              aFocus, 10.0, 10.0),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = actualStatus;
  }
  actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                              aSecondFocus, 10.0 * aScale, 10.0),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = actualStatus;
  }
  actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_END,
                              // note: negative values here tell APZC
                              //       not to turn the pinch into a pan
                              ScreenIntPoint(-1, -1), 10.0 * aScale, 10.0 * aScale),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = actualStatus;
  }
}

template<class InputReceiver>
void
PinchWithPinchInputAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                                  const ScreenIntPoint& aFocus, float aScale,
                                  bool aShouldTriggerPinch)
{
  nsEventStatus statuses[3];  // scalebegin, scale, scaleend
  PinchWithPinchInput(aTarget, aFocus, aFocus, aScale, &statuses);

  nsEventStatus expectedStatus = aShouldTriggerPinch
      ? nsEventStatus_eConsumeNoDefault
      : nsEventStatus_eIgnore;
  EXPECT_EQ(expectedStatus, statuses[0]);
  EXPECT_EQ(expectedStatus, statuses[1]);
}

template<class InputReceiver>
void
PinchWithTouchInput(const RefPtr<InputReceiver>& aTarget,
                    const ScreenIntPoint& aFocus, float aScale,
                    int& inputId,
                    nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
                    nsEventStatus (*aOutEventStatuses)[4] = nullptr,
                    uint64_t* aOutInputBlockId = nullptr)
{
  // Having pinch coordinates in float type may cause problems with high-precision scale values
  // since SingleTouchData accepts integer value. But for trivial tests it should be ok.
  float pinchLength = 100.0;
  float pinchLengthScaled = pinchLength * aScale;

  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  MultiTouchInput mtiStart = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocus));
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocus));
  nsEventStatus status = aTarget->ReceiveInputEvent(mtiStart, aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  if (aAllowedTouchBehaviors) {
    EXPECT_EQ(2UL, aAllowedTouchBehaviors->Length());
    aTarget->SetAllowedTouchBehavior(*aOutInputBlockId, *aAllowedTouchBehaviors);
  } else if (gfxPrefs::TouchActionEnabled()) {
    SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId, 2);
  }

  MultiTouchInput mtiMove1 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocus.x - pinchLength, aFocus.y));
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocus.x + pinchLength, aFocus.y));
  status = aTarget->ReceiveInputEvent(mtiMove1, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  MultiTouchInput mtiMove2 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocus.x - pinchLengthScaled, aFocus.y));
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocus.x + pinchLengthScaled, aFocus.y));
  status = aTarget->ReceiveInputEvent(mtiMove2, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  MultiTouchInput mtiEnd = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocus.x - pinchLengthScaled, aFocus.y));
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocus.x + pinchLengthScaled, aFocus.y));
  status = aTarget->ReceiveInputEvent(mtiEnd, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }

  inputId += 2;
}

template<class InputReceiver>
void
PinchWithTouchInputAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                                  const ScreenIntPoint& aFocus, float aScale,
                                  int& inputId, bool aShouldTriggerPinch,
                                  nsTArray<uint32_t>* aAllowedTouchBehaviors)
{
  nsEventStatus statuses[4];  // down, move, move, up
  PinchWithTouchInput(aTarget, aFocus, aScale, inputId, aAllowedTouchBehaviors, &statuses);

  nsEventStatus expectedMoveStatus = aShouldTriggerPinch
      ? nsEventStatus_eConsumeDoDefault
      : nsEventStatus_eIgnore;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(expectedMoveStatus, statuses[1]);
  EXPECT_EQ(expectedMoveStatus, statuses[2]);
}

template<class InputReceiver>
nsEventStatus
Wheel(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
      const ScreenPoint& aDelta, TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  ScrollWheelInput input(MillisecondsSinceStartup(aTime), aTime, 0,
      ScrollWheelInput::SCROLLMODE_INSTANT, ScrollWheelInput::SCROLLDELTA_PIXEL,
      aPoint, aDelta.x, aDelta.y, false);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template<class InputReceiver>
nsEventStatus
SmoothWheel(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
            const ScreenPoint& aDelta, TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  ScrollWheelInput input(MillisecondsSinceStartup(aTime), aTime, 0,
      ScrollWheelInput::SCROLLMODE_SMOOTH, ScrollWheelInput::SCROLLDELTA_LINE,
      aPoint, aDelta.x, aDelta.y, false);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template<class InputReceiver>
nsEventStatus
MouseDown(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
          TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  MouseInput input(MouseInput::MOUSE_DOWN, MouseInput::ButtonType::LEFT_BUTTON,
        0, 0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template<class InputReceiver>
nsEventStatus
MouseMove(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
          TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  MouseInput input(MouseInput::MOUSE_MOVE, MouseInput::ButtonType::LEFT_BUTTON,
        0, 0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template<class InputReceiver>
nsEventStatus
MouseUp(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
        TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr)
{
  MouseInput input(MouseInput::MOUSE_UP, MouseInput::ButtonType::LEFT_BUTTON,
        0, 0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}


#endif // mozilla_layers_InputUtils_h
