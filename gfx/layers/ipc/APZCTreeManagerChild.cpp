/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZCTreeManagerChild.h"

#include "InputData.h"                              // for InputData
#include "mozilla/dom/BrowserParent.h"              // for BrowserParent
#include "mozilla/layers/APZCCallbackHelper.h"      // for APZCCallbackHelper
#include "mozilla/layers/APZInputBridgeChild.h"     // for APZInputBridgeChild
#include "mozilla/layers/GeckoContentController.h"  // for GeckoContentController
#include "mozilla/layers/RemoteCompositorSession.h"  // for RemoteCompositorSession

namespace mozilla {
namespace layers {

APZCTreeManagerChild::APZCTreeManagerChild()
    : mCompositorSession(nullptr), mIPCOpen(false) {}

APZCTreeManagerChild::~APZCTreeManagerChild() = default;

void APZCTreeManagerChild::SetCompositorSession(
    RemoteCompositorSession* aSession) {
  // Exactly one of mCompositorSession and aSession must be null (i.e. either
  // we're setting mCompositorSession or we're clearing it).
  MOZ_ASSERT(!mCompositorSession ^ !aSession);
  mCompositorSession = aSession;
}

void APZCTreeManagerChild::SetInputBridge(APZInputBridgeChild* aInputBridge) {
  // The input bridge only exists from the UI process to the GPU process.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!mInputBridge);

  mInputBridge = aInputBridge;
}

void APZCTreeManagerChild::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mInputBridge) {
    mInputBridge->Destroy();
    mInputBridge = nullptr;
  }
}

void APZCTreeManagerChild::SetKeyboardMap(const KeyboardMap& aKeyboardMap) {
  MOZ_ASSERT(NS_IsMainThread());
  SendSetKeyboardMap(aKeyboardMap);
}

void APZCTreeManagerChild::ZoomToRect(const ScrollableLayerGuid& aGuid,
                                      const ZoomTarget& aZoomTarget,
                                      const uint32_t aFlags) {
  MOZ_ASSERT(NS_IsMainThread());
  SendZoomToRect(aGuid, aZoomTarget, aFlags);
}

void APZCTreeManagerChild::ContentReceivedInputBlock(uint64_t aInputBlockId,
                                                     bool aPreventDefault) {
  MOZ_ASSERT(NS_IsMainThread());
  SendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

void APZCTreeManagerChild::SetTargetAPZC(
    uint64_t aInputBlockId, const nsTArray<ScrollableLayerGuid>& aTargets) {
  MOZ_ASSERT(NS_IsMainThread());
  SendSetTargetAPZC(aInputBlockId, aTargets);
}

void APZCTreeManagerChild::UpdateZoomConstraints(
    const ScrollableLayerGuid& aGuid,
    const Maybe<ZoomConstraints>& aConstraints) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mIPCOpen) {
    SendUpdateZoomConstraints(aGuid, aConstraints);
  }
}

void APZCTreeManagerChild::SetDPI(float aDpiValue) {
  MOZ_ASSERT(NS_IsMainThread());
  SendSetDPI(aDpiValue);
}

void APZCTreeManagerChild::SetAllowedTouchBehavior(
    uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aValues) {
  MOZ_ASSERT(NS_IsMainThread());
  SendSetAllowedTouchBehavior(aInputBlockId, aValues);
}

void APZCTreeManagerChild::StartScrollbarDrag(
    const ScrollableLayerGuid& aGuid, const AsyncDragMetrics& aDragMetrics) {
  MOZ_ASSERT(NS_IsMainThread());
  SendStartScrollbarDrag(aGuid, aDragMetrics);
}

bool APZCTreeManagerChild::StartAutoscroll(const ScrollableLayerGuid& aGuid,
                                           const ScreenPoint& aAnchorLocation) {
  MOZ_ASSERT(NS_IsMainThread());
  return SendStartAutoscroll(aGuid, aAnchorLocation);
}

void APZCTreeManagerChild::StopAutoscroll(const ScrollableLayerGuid& aGuid) {
  MOZ_ASSERT(NS_IsMainThread());
  SendStopAutoscroll(aGuid);
}

void APZCTreeManagerChild::SetLongTapEnabled(bool aTapGestureEnabled) {
  MOZ_ASSERT(NS_IsMainThread());
  SendSetLongTapEnabled(aTapGestureEnabled);
}

APZInputBridge* APZCTreeManagerChild::InputBridge() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mInputBridge);

  return mInputBridge.get();
}

void APZCTreeManagerChild::AddInputBlockCallback(
    uint64_t aInputBlockId, InputBlockCallback&& aCallback) {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NewRunnableMethod<uint64_t, InputBlockCallback&&>(
        "layers::APZCTreeManagerChild::AddInputBlockCallback", this,
        &APZCTreeManagerChild::AddInputBlockCallback, aInputBlockId,
        std::move(aCallback)));
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  mInputBlockCallbacks.emplace(aInputBlockId, std::move(aCallback));

  SendAddInputBlockCallback(aInputBlockId);
}

void APZCTreeManagerChild::AddIPDLReference() {
  MOZ_ASSERT(mIPCOpen == false);
  mIPCOpen = true;
  AddRef();
}

void APZCTreeManagerChild::ReleaseIPDLReference() {
  mIPCOpen = false;
  Release();
}

void APZCTreeManagerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

mozilla::ipc::IPCResult APZCTreeManagerChild::RecvHandleTap(
    const TapType& aType, const LayoutDevicePoint& aPoint,
    const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (mCompositorSession &&
      mCompositorSession->RootLayerTreeId() == aGuid.mLayersId &&
      mCompositorSession->GetContentController()) {
    RefPtr<GeckoContentController> controller =
        mCompositorSession->GetContentController();
    controller->HandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId);
    return IPC_OK();
  }
  dom::BrowserParent* tab =
      dom::BrowserParent::GetBrowserParentFromLayersId(aGuid.mLayersId);
  if (tab) {
    tab->SendHandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult APZCTreeManagerChild::RecvNotifyPinchGesture(
    const PinchGestureType& aType, const ScrollableLayerGuid& aGuid,
    const LayoutDevicePoint& aFocusPoint, const LayoutDeviceCoord& aSpanChange,
    const Modifiers& aModifiers) {
  // This will only get sent from the GPU process to the parent process, so
  // this function should never get called in the content process.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // We want to handle it in this process regardless of what the target guid
  // of the pinch is. This may change in the future.
  if (mCompositorSession && mCompositorSession->GetWidget()) {
    APZCCallbackHelper::NotifyPinchGesture(aType, aFocusPoint, aSpanChange,
                                           aModifiers,
                                           mCompositorSession->GetWidget());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult APZCTreeManagerChild::RecvCancelAutoscroll(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  // This will only get sent from the GPU process to the parent process, so
  // this function should never get called in the content process.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  APZCCallbackHelper::CancelAutoscroll(aScrollId);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZCTreeManagerChild::RecvCallInputBlockCallback(
    uint64_t aInputBlockId, const APZHandledResult& aHandledResult) {
  auto it = mInputBlockCallbacks.find(aInputBlockId);
  if (it != mInputBlockCallbacks.end()) {
    it->second(aInputBlockId, aHandledResult);
    // The callback is one-shot; discard it after calling it.
    mInputBlockCallbacks.erase(it);
  }

  return IPC_OK();
}

}  // namespace layers
}  // namespace mozilla
