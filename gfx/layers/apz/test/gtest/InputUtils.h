/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

template <class InputReceiver>
void SetDefaultAllowedTouchBehavior(const RefPtr<InputReceiver>& aTarget,
                                    uint64_t aInputBlockId,
                                    int touchPoints = 1) {
  nsTArray<uint32_t> defaultBehaviors;
  // use the default value where everything is allowed
  for (int i = 0; i < touchPoints; i++) {
    defaultBehaviors.AppendElement(
        mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN |
        mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN |
        mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM |
        mozilla::layers::AllowedTouchBehavior::DOUBLE_TAP_ZOOM);
  }
  aTarget->SetAllowedTouchBehavior(aInputBlockId, defaultBehaviors);
}

inline MultiTouchInput CreateMultiTouchInput(
    MultiTouchInput::MultiTouchType aType, TimeStamp aTime) {
  return MultiTouchInput(aType, MillisecondsSinceStartup(aTime), aTime, 0);
}

template <class InputReceiver>
nsEventStatus TouchDown(const RefPtr<InputReceiver>& aTarget,
                        const ScreenIntPoint& aPoint, TimeStamp aTime,
                        uint64_t* aOutInputBlockId = nullptr) {
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus TouchMove(const RefPtr<InputReceiver>& aTarget,
                        const ScreenIntPoint& aPoint, TimeStamp aTime) {
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template <class InputReceiver>
nsEventStatus TouchUp(const RefPtr<InputReceiver>& aTarget,
                      const ScreenIntPoint& aPoint, TimeStamp aTime) {
  MultiTouchInput mti =
      CreateMultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aPoint));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template <class InputReceiver>
nsEventStatus Wheel(const RefPtr<InputReceiver>& aTarget,
                    const ScreenIntPoint& aPoint, const ScreenPoint& aDelta,
                    TimeStamp aTime, uint64_t* aOutInputBlockId = nullptr) {
  ScrollWheelInput input(MillisecondsSinceStartup(aTime), aTime, 0,
                         ScrollWheelInput::SCROLLMODE_INSTANT,
                         ScrollWheelInput::SCROLLDELTA_PIXEL, aPoint, aDelta.x,
                         aDelta.y, false, WheelDeltaAdjustmentStrategy::eNone);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus SmoothWheel(const RefPtr<InputReceiver>& aTarget,
                          const ScreenIntPoint& aPoint,
                          const ScreenPoint& aDelta, TimeStamp aTime,
                          uint64_t* aOutInputBlockId = nullptr) {
  ScrollWheelInput input(MillisecondsSinceStartup(aTime), aTime, 0,
                         ScrollWheelInput::SCROLLMODE_SMOOTH,
                         ScrollWheelInput::SCROLLDELTA_LINE, aPoint, aDelta.x,
                         aDelta.y, false, WheelDeltaAdjustmentStrategy::eNone);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus MouseDown(const RefPtr<InputReceiver>& aTarget,
                        const ScreenIntPoint& aPoint, TimeStamp aTime,
                        uint64_t* aOutInputBlockId = nullptr) {
  MouseInput input(MouseInput::MOUSE_DOWN, MouseInput::ButtonType::LEFT_BUTTON,
                   0, 0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus MouseMove(const RefPtr<InputReceiver>& aTarget,
                        const ScreenIntPoint& aPoint, TimeStamp aTime,
                        uint64_t* aOutInputBlockId = nullptr) {
  MouseInput input(MouseInput::MOUSE_MOVE, MouseInput::ButtonType::LEFT_BUTTON,
                   0, 0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus MouseUp(const RefPtr<InputReceiver>& aTarget,
                      const ScreenIntPoint& aPoint, TimeStamp aTime,
                      uint64_t* aOutInputBlockId = nullptr) {
  MouseInput input(MouseInput::MOUSE_UP, MouseInput::ButtonType::LEFT_BUTTON, 0,
                   0, aPoint, MillisecondsSinceStartup(aTime), aTime, 0);
  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

template <class InputReceiver>
nsEventStatus PanGesture(PanGestureInput::PanGestureType aType,
                         const RefPtr<InputReceiver>& aTarget,
                         const ScreenIntPoint& aPoint,
                         const ScreenPoint& aDelta, TimeStamp aTime,
                         uint64_t* aOutInputBlockId = nullptr) {
  PanGestureInput input(aType, MillisecondsSinceStartup(aTime), aTime, aPoint,
                        aDelta, 0 /* Modifiers */);
  if (aType == PanGestureInput::PANGESTURE_END) {
    input.mFollowedByMomentum = true;
  }

  return aTarget->ReceiveInputEvent(input, nullptr, aOutInputBlockId);
}

#endif  // mozilla_layers_InputUtils_h
