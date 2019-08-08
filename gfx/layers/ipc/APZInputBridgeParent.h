/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridgeParent_h
#define mozilla_layers_APZInputBridgeParent_h

#include "mozilla/layers/PAPZInputBridgeParent.h"

namespace mozilla {
namespace layers {

class IAPZCTreeManager;

class APZInputBridgeParent : public PAPZInputBridgeParent {
  NS_INLINE_DECL_REFCOUNTING(APZInputBridgeParent, final)

 public:
  explicit APZInputBridgeParent(const LayersId& aLayersId);

  mozilla::ipc::IPCResult RecvReceiveMultiTouchInputEvent(
      const MultiTouchInput& aEvent, nsEventStatus* aOutStatus,
      MultiTouchInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceiveMouseInputEvent(
      const MouseInput& aEvent, nsEventStatus* aOutStatus,
      MouseInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceivePanGestureInputEvent(
      const PanGestureInput& aEvent, nsEventStatus* aOutStatus,
      PanGestureInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceivePinchGestureInputEvent(
      const PinchGestureInput& aEvent, nsEventStatus* aOutStatus,
      PinchGestureInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceiveTapGestureInputEvent(
      const TapGestureInput& aEvent, nsEventStatus* aOutStatus,
      TapGestureInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceiveScrollWheelInputEvent(
      const ScrollWheelInput& aEvent, nsEventStatus* aOutStatus,
      ScrollWheelInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvReceiveKeyboardInputEvent(
      const KeyboardInput& aEvent, nsEventStatus* aOutStatus,
      KeyboardInput* aOutEvent, ScrollableLayerGuid* aOutTargetGuid,
      uint64_t* aOutInputBlockId);

  mozilla::ipc::IPCResult RecvUpdateWheelTransaction(
      const LayoutDeviceIntPoint& aRefPoint, const EventMessage& aEventMessage);

  mozilla::ipc::IPCResult RecvProcessUnhandledEvent(
      const LayoutDeviceIntPoint& aRefPoint, LayoutDeviceIntPoint* aOutRefPoint,
      ScrollableLayerGuid* aOutTargetGuid, uint64_t* aOutFocusSequenceNumber,
      LayersId* aOutLayersId);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~APZInputBridgeParent();

 private:
  RefPtr<IAPZCTreeManager> mTreeManager;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZInputBridgeParent_h
