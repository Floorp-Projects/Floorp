/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeParent.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "InputData.h"

namespace mozilla {
namespace layers {

/* static */
APZInputBridgeParent* APZInputBridgeParent::Create(
    const LayersId& aLayersId, Endpoint<PAPZInputBridgeParent>&& aEndpoint) {
  APZInputBridgeParent* parent = new APZInputBridgeParent(aLayersId);
  if (!aEndpoint.Bind(parent)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind APZInputBridgeParent to endpoint");
  }

  CompositorBridgeParent::SetAPZInputBridgeParent(aLayersId, parent);
  return parent;
}

APZInputBridgeParent::APZInputBridgeParent(const LayersId& aLayersId) {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mLayersId = aLayersId;
  mTreeManager = CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
  MOZ_ASSERT(mTreeManager);
}

APZInputBridgeParent::~APZInputBridgeParent() = default;

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveMultiTouchInputEvent(
    const MultiTouchInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, MultiTouchInput* aOutEvent) {
  MultiTouchInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveMouseInputEvent(
    const MouseInput& aEvent, bool aWantsCallback, APZEventResult* aOutResult,
    MouseInput* aOutEvent) {
  MouseInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceivePanGestureInputEvent(
    const PanGestureInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, PanGestureInput* aOutEvent) {
  PanGestureInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceivePinchGestureInputEvent(
    const PinchGestureInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, PinchGestureInput* aOutEvent) {
  PinchGestureInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveTapGestureInputEvent(
    const TapGestureInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, TapGestureInput* aOutEvent) {
  TapGestureInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveScrollWheelInputEvent(
    const ScrollWheelInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, ScrollWheelInput* aOutEvent) {
  ScrollWheelInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvReceiveKeyboardInputEvent(
    const KeyboardInput& aEvent, bool aWantsCallback,
    APZEventResult* aOutResult, KeyboardInput* aOutEvent) {
  KeyboardInput event = aEvent;

  APZInputBridge::InputBlockCallback callback;
  if (aWantsCallback) {
    callback = [self = RefPtr<APZInputBridgeParent>(this)](
                   uint64_t aInputBlockId,
                   const APZHandledResult& aHandledResult) {
      Unused << self->SendCallInputBlockCallback(aInputBlockId, aHandledResult);
    };
  }

  *aOutResult = mTreeManager->InputBridge()->ReceiveInputEvent(
      event, std::move(callback));
  *aOutEvent = event;

  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeParent::RecvUpdateWheelTransaction(
    const LayoutDeviceIntPoint& aRefPoint, const EventMessage& aEventMessage,
    const Maybe<ScrollableLayerGuid>& aTargetGuid) {
  mTreeManager->InputBridge()->UpdateWheelTransaction(aRefPoint, aEventMessage,
                                                      aTargetGuid);
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
  StaticMonitorAutoLock lock(CompositorBridgeParent::sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state =
      CompositorBridgeParent::sIndirectLayerTrees[mLayersId];
  state.mApzInputBridgeParent = nullptr;
  // We shouldn't need it after this
  mTreeManager = nullptr;
}

}  // namespace layers
}  // namespace mozilla
