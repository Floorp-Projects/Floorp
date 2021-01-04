/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeParent.h"

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "InputData.h"

namespace mozilla {
namespace layers {

APZInputBridgeParent::APZInputBridgeParent(const LayersId& aLayersId) {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mTreeManager = CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
  MOZ_ASSERT(mTreeManager);
}

APZInputBridgeParent::~APZInputBridgeParent() = default;

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveMultiTouchInputEvent(
    const MultiTouchInput& aEvent, APZEventResult* aOutResult,
    MultiTouchInput* aOutEvent) {
  MultiTouchInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveMouseInputEvent(
    const MouseInput& aEvent, APZEventResult* aOutResult,
    MouseInput* aOutEvent) {
  MouseInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceivePanGestureInputEvent(
    const PanGestureInput& aEvent, APZEventResult* aOutResult,
    PanGestureInput* aOutEvent) {
  PanGestureInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceivePinchGestureInputEvent(
    const PinchGestureInput& aEvent, APZEventResult* aOutResult,
    PinchGestureInput* aOutEvent) {
  PinchGestureInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveTapGestureInputEvent(
    const TapGestureInput& aEvent, APZEventResult* aOutResult,
    TapGestureInput* aOutEvent) {
  TapGestureInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveScrollWheelInputEvent(
    const ScrollWheelInput& aEvent, APZEventResult* aOutResult,
    ScrollWheelInput* aOutEvent) {
  ScrollWheelInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveKeyboardInputEvent(
    const KeyboardInput& aEvent, APZEventResult* aOutResult,
    KeyboardInput* aOutEvent) {
  KeyboardInput event = aEvent;

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(event);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvUpdateWheelTransaction(
    const LayoutDeviceIntPoint& aRefPoint, const EventMessage& aEventMessage) {
  mTreeManager->InputBridge()->UpdateWheelTransaction(aRefPoint, aEventMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvProcessUnhandledEvent(
    const LayoutDeviceIntPoint& aRefPoint, LayoutDeviceIntPoint* aOutRefPoint,
    ScrollableLayerGuid* aOutTargetGuid, uint64_t* aOutFocusSequenceNumber,
    LayersId* aOutLayersId) {
  LayoutDeviceIntPoint refPoint = aRefPoint;
  mTreeManager->InputBridge()->ProcessUnhandledEvent(
      &refPoint, aOutTargetGuid, aOutFocusSequenceNumber, aOutLayersId);
  *aOutRefPoint = refPoint;

  return IPC_OK();
}

void APZInputBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  // We shouldn't need it after this
  mTreeManager = nullptr;
}

}  // namespace layers
}  // namespace mozilla
