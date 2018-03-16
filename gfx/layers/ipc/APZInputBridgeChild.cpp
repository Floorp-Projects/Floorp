/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeChild.h"

namespace mozilla {
namespace layers {

APZInputBridgeChild::APZInputBridgeChild()
  : mDestroyed(false)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

APZInputBridgeChild::~APZInputBridgeChild()
{
}

void
APZInputBridgeChild::Destroy()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mDestroyed) {
    return;
  }

  Send__delete__(this);
  mDestroyed = true;
}

void
APZInputBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mDestroyed = true;
}

nsEventStatus
APZInputBridgeChild::ReceiveInputEvent(
    InputData& aEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    MultiTouchInput& event = aEvent.AsMultiTouchInput();
    MultiTouchInput processedEvent;

    nsEventStatus res;
    SendReceiveMultiTouchInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case MOUSE_INPUT: {
    MouseInput& event = aEvent.AsMouseInput();
    MouseInput processedEvent;

    nsEventStatus res;
    SendReceiveMouseInputEvent(event,
                               &res,
                               &processedEvent,
                               aOutTargetGuid,
                               aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case PANGESTURE_INPUT: {
    PanGestureInput& event = aEvent.AsPanGestureInput();
    PanGestureInput processedEvent;

    nsEventStatus res;
    SendReceivePanGestureInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case PINCHGESTURE_INPUT: {
    PinchGestureInput& event = aEvent.AsPinchGestureInput();
    PinchGestureInput processedEvent;

    nsEventStatus res;
    SendReceivePinchGestureInputEvent(event,
                                      &res,
                                      &processedEvent,
                                      aOutTargetGuid,
                                      aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case TAPGESTURE_INPUT: {
    TapGestureInput& event = aEvent.AsTapGestureInput();
    TapGestureInput processedEvent;

    nsEventStatus res;
    SendReceiveTapGestureInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case SCROLLWHEEL_INPUT: {
    ScrollWheelInput& event = aEvent.AsScrollWheelInput();
    ScrollWheelInput processedEvent;

    nsEventStatus res;
    SendReceiveScrollWheelInputEvent(event,
                                     &res,
                                     &processedEvent,
                                     aOutTargetGuid,
                                     aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case KEYBOARD_INPUT: {
    KeyboardInput& event = aEvent.AsKeyboardInput();
    KeyboardInput processedEvent;

    nsEventStatus res;
    SendReceiveKeyboardInputEvent(event,
                                  &res,
                                  &processedEvent,
                                  aOutTargetGuid,
                                  aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Invalid InputData type.");
    return nsEventStatus_eConsumeNoDefault;
  }
  }
}

void APZInputBridgeChild::ProcessUnhandledEvent(
    LayoutDeviceIntPoint* aRefPoint,
    ScrollableLayerGuid*  aOutTargetGuid,
    uint64_t*             aOutFocusSequenceNumber)
{
  SendProcessUnhandledEvent(*aRefPoint,
                            aRefPoint,
                            aOutTargetGuid,
                            aOutFocusSequenceNumber);
}

void
APZInputBridgeChild::UpdateWheelTransaction(
    LayoutDeviceIntPoint aRefPoint,
    EventMessage aEventMessage)
{
  SendUpdateWheelTransaction(aRefPoint, aEventMessage);
}

} // namespace layers
} // namespace mozilla
