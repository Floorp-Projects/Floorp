/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridgeChild_h
#define mozilla_layers_APZInputBridgeChild_h

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/PAPZInputBridgeChild.h"

#include "mozilla/layers/GeckoContentControllerTypes.h"

namespace mozilla {
namespace layers {

class RemoteCompositorSession;

class APZInputBridgeChild : public PAPZInputBridgeChild, public APZInputBridge {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZInputBridgeChild, final)
  using TapType = GeckoContentController_TapType;

 public:
  static RefPtr<APZInputBridgeChild> Create(
      const uint64_t& aProcessToken,
      Endpoint<PAPZInputBridgeChild>&& aEndpoint);

  void Destroy();

  void SetCompositorSession(RemoteCompositorSession* aSession);

  APZEventResult ReceiveInputEvent(
      InputData& aEvent,
      InputBlockCallback&& aCallback = InputBlockCallback()) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvHandleTap(
      const TapType& aType, const LayoutDevicePoint& aPoint,
      const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid,
      const uint64_t& aInputBlockId,
      const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics);

  mozilla::ipc::IPCResult RecvCallInputBlockCallback(
      uint64_t aInputBlockId, const APZHandledResult& handledResult);

 protected:
  void ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                             ScrollableLayerGuid* aOutTargetGuid,
                             uint64_t* aOutFocusSequenceNumber,
                             LayersId* aOutLayersId) override;

  void UpdateWheelTransaction(
      LayoutDeviceIntPoint aRefPoint, EventMessage aEventMessage,
      const Maybe<ScrollableLayerGuid>& aTargetGuid) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  explicit APZInputBridgeChild(const uint64_t& aProcessToken);
  virtual ~APZInputBridgeChild();

 private:
  void Open(Endpoint<PAPZInputBridgeChild>&& aEndpoint);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void HandleTapOnMainThread(
      const TapType& aType, const LayoutDevicePoint& aPoint,
      const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid,
      const uint64_t& aInputBlockId,
      const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics);

  bool mIsOpen;
  uint64_t mProcessToken;
  MOZ_NON_OWNING_REF RemoteCompositorSession* mCompositorSession = nullptr;

  using InputBlockCallbackMap =
      std::unordered_map<uint64_t, InputBlockCallback>;
  InputBlockCallbackMap mInputBlockCallbacks;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZInputBridgeChild_h
