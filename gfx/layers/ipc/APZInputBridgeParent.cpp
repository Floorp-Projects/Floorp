/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeParent.h"

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/IAPZCTreeManager.h"

namespace mozilla {
namespace layers {

APZInputBridgeParent::APZInputBridgeParent(const uint64_t& aLayersId)
{
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mTreeManager = CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
  MOZ_ASSERT(mTreeManager);
}

APZInputBridgeParent::~APZInputBridgeParent()
{
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceiveMultiTouchInputEvent(
    const MultiTouchInput& aEvent,
    nsEventStatus* aOutStatus,
    MultiTouchInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  MultiTouchInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceiveMouseInputEvent(
    const MouseInput& aEvent,
    nsEventStatus* aOutStatus,
    MouseInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  MouseInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceivePanGestureInputEvent(
    const PanGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    PanGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  PanGestureInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceivePinchGestureInputEvent(
    const PinchGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    PinchGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  PinchGestureInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceiveTapGestureInputEvent(
    const TapGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    TapGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  TapGestureInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceiveScrollWheelInputEvent(
    const ScrollWheelInput& aEvent,
    nsEventStatus* aOutStatus,
    ScrollWheelInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  ScrollWheelInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvReceiveKeyboardInputEvent(
        const KeyboardInput& aEvent,
        nsEventStatus* aOutStatus,
        KeyboardInput* aOutEvent,
        ScrollableLayerGuid* aOutTargetGuid,
        uint64_t* aOutInputBlockId)
{
  KeyboardInput event = aEvent;

  *aOutStatus = mTreeManager->InputBridge()->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvUpdateWheelTransaction(
        const LayoutDeviceIntPoint& aRefPoint,
        const EventMessage& aEventMessage)
{
  mTreeManager->InputBridge()->UpdateWheelTransaction(
        aRefPoint, aEventMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
APZInputBridgeParent::RecvProcessUnhandledEvent(
        const LayoutDeviceIntPoint& aRefPoint,
        LayoutDeviceIntPoint* aOutRefPoint,
        ScrollableLayerGuid*  aOutTargetGuid,
        uint64_t*             aOutFocusSequenceNumber)
{
  LayoutDeviceIntPoint refPoint = aRefPoint;
  mTreeManager->InputBridge()->ProcessUnhandledEvent(
        &refPoint, aOutTargetGuid, aOutFocusSequenceNumber);
  *aOutRefPoint = refPoint;

  return IPC_OK();
}

void
APZInputBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // We shouldn't need it after this
  mTreeManager = nullptr;
}

} // namespace layers
} // namespace mozilla
