/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeChild.h"

#include "InputData.h"  // for InputData, etc
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/SynchronousTask.h"

#include "mozilla/layers/GeckoContentController.h"  // for GeckoContentController
#include "mozilla/layers/DoubleTapToZoom.h"  // for DoubleTapToZoomMetrics
#include "mozilla/layers/RemoteCompositorSession.h"  // for RemoteCompositorSession
#include "mozilla/dom/BrowserParent.h"               // for BrowserParent
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"  // for DispatchToGeckoPriorityQueue
#endif

namespace mozilla {
namespace layers {

/* static */
RefPtr<APZInputBridgeChild> APZInputBridgeChild::Create(
    const uint64_t& aProcessToken, Endpoint<PAPZInputBridgeChild>&& aEndpoint) {
  RefPtr<APZInputBridgeChild> child = new APZInputBridgeChild(aProcessToken);

  MOZ_ASSERT(APZThreadUtils::IsControllerThreadAlive());

  APZThreadUtils::RunOnControllerThread(
      NewRunnableMethod<Endpoint<PAPZInputBridgeChild>&&>(
          "layers::APZInputBridgeChild::Open", child,
          &APZInputBridgeChild::Open, std::move(aEndpoint)));

  return child;
}

APZInputBridgeChild::APZInputBridgeChild(const uint64_t& aProcessToken)
    : mIsOpen(false),
      mProcessToken(aProcessToken),
      mCompositorSession(nullptr) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

APZInputBridgeChild::~APZInputBridgeChild() = default;

void APZInputBridgeChild::SetCompositorSession(
    RemoteCompositorSession* aSession) {
  mCompositorSession = aSession;
}

void APZInputBridgeChild::Open(Endpoint<PAPZInputBridgeChild>&& aEndpoint) {
  APZThreadUtils::AssertOnControllerThread();

  mIsOpen = aEndpoint.Bind(this);

  if (!mIsOpen) {
    // The GPU Process Manager might be gone if we receive ActorDestroy very
    // late in shutdown.
    if (gfx::GPUProcessManager* gpm = gfx::GPUProcessManager::Get()) {
      gpm->NotifyRemoteActorDestroyed(mProcessToken);
    }
    return;
  }
}

void APZInputBridgeChild::Destroy() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // Destroy will get called from the main thread, so we must synchronously
  // dispatch to the controller thread to close the bridge.
  layers::SynchronousTask task("layers::APZInputBridgeChild::Destroy");
  APZThreadUtils::RunOnControllerThread(
      NS_NewRunnableFunction("layers::APZInputBridgeChild::Destroy", [&]() {
        APZThreadUtils::AssertOnControllerThread();
        AutoCompleteTask complete(&task);

        // Clear the process token so that we don't notify the GPUProcessManager
        // about an abnormal shutdown, thereby tearing down the GPU process.
        mProcessToken = 0;

        if (mIsOpen) {
          PAPZInputBridgeChild::Close();
          mIsOpen = false;
        }
      }));

  task.Wait();
}

void APZInputBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIsOpen = false;

  if (mProcessToken) {
    gfx::GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
    mProcessToken = 0;
  }
}

APZEventResult APZInputBridgeChild::ReceiveInputEvent(
    InputData& aEvent, InputBlockCallback&& aCallback) {
  MOZ_ASSERT(mIsOpen);
  APZThreadUtils::AssertOnControllerThread();

  APZEventResult res;
  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      MultiTouchInput& event = aEvent.AsMultiTouchInput();
      MultiTouchInput processedEvent;

      SendReceiveMultiTouchInputEvent(event, !!aCallback, &res,
                                      &processedEvent);

      event = processedEvent;
      break;
    }
    case MOUSE_INPUT: {
      MouseInput& event = aEvent.AsMouseInput();
      MouseInput processedEvent;

      SendReceiveMouseInputEvent(event, !!aCallback, &res, &processedEvent);

      event = processedEvent;
      break;
    }
    case PANGESTURE_INPUT: {
      PanGestureInput& event = aEvent.AsPanGestureInput();
      PanGestureInput processedEvent;

      SendReceivePanGestureInputEvent(event, !!aCallback, &res,
                                      &processedEvent);

      event = processedEvent;
      break;
    }
    case PINCHGESTURE_INPUT: {
      PinchGestureInput& event = aEvent.AsPinchGestureInput();
      PinchGestureInput processedEvent;

      SendReceivePinchGestureInputEvent(event, !!aCallback, &res,
                                        &processedEvent);

      event = processedEvent;
      break;
    }
    case TAPGESTURE_INPUT: {
      TapGestureInput& event = aEvent.AsTapGestureInput();
      TapGestureInput processedEvent;

      SendReceiveTapGestureInputEvent(event, !!aCallback, &res,
                                      &processedEvent);

      event = processedEvent;
      break;
    }
    case SCROLLWHEEL_INPUT: {
      ScrollWheelInput& event = aEvent.AsScrollWheelInput();
      ScrollWheelInput processedEvent;

      SendReceiveScrollWheelInputEvent(event, !!aCallback, &res,
                                       &processedEvent);

      event = processedEvent;
      break;
    }
    case KEYBOARD_INPUT: {
      KeyboardInput& event = aEvent.AsKeyboardInput();
      KeyboardInput processedEvent;

      SendReceiveKeyboardInputEvent(event, !!aCallback, &res, &processedEvent);

      event = processedEvent;
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("Invalid InputData type.");
      res.SetStatusAsConsumeNoDefault();
      break;
    }
  }

  if (aCallback && res.WillHaveDelayedResult()) {
    mInputBlockCallbacks.emplace(res.mInputBlockId, std::move(aCallback));
  }

  return res;
}

void APZInputBridgeChild::HandleTapOnMainThread(
    const TapType& aType, const LayoutDevicePoint& aPoint,
    const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId,
    const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics) {
  if (mCompositorSession &&
      mCompositorSession->RootLayerTreeId() == aGuid.mLayersId &&
      mCompositorSession->GetContentController()) {
    RefPtr<GeckoContentController> controller =
        mCompositorSession->GetContentController();
    controller->HandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId,
                          aDoubleTapToZoomMetrics);
    return;
  }
  dom::BrowserParent* tab =
      dom::BrowserParent::GetBrowserParentFromLayersId(aGuid.mLayersId);
  if (tab) {
#ifdef MOZ_WIDGET_ANDROID
    // On Android, touch events are dispatched from the UI thread to the main
    // thread using the Android priority queue. It is possible that this tap has
    // made it to the GPU process and back before they have been processed. We
    // must therefore dispatch this message to the same queue, otherwise the tab
    // may receive the tap event before the touch events that synthesized it.
    mozilla::jni::DispatchToGeckoPriorityQueue(
        NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                          ScrollableLayerGuid, uint64_t,
                          Maybe<DoubleTapToZoomMetrics>>(
            "dom::BrowserParent::SendHandleTap", tab,
            &dom::BrowserParent::SendHandleTap, aType, aPoint, aModifiers,
            aGuid, aInputBlockId, aDoubleTapToZoomMetrics));
#else
    tab->SendHandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId,
                       aDoubleTapToZoomMetrics);
#endif
  }
}

mozilla::ipc::IPCResult APZInputBridgeChild::RecvHandleTap(
    const TapType& aType, const LayoutDevicePoint& aPoint,
    const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId,
    const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics) {
  if (NS_IsMainThread()) {
    HandleTapOnMainThread(aType, aPoint, aModifiers, aGuid, aInputBlockId,
                          aDoubleTapToZoomMetrics);
  } else {
    NS_DispatchToMainThread(
        NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                          ScrollableLayerGuid, uint64_t,
                          Maybe<DoubleTapToZoomMetrics>>(
            "layers::APZInputBridgeChild::HandleTapOnMainThread", this,
            &APZInputBridgeChild::HandleTapOnMainThread, aType, aPoint,
            aModifiers, aGuid, aInputBlockId, aDoubleTapToZoomMetrics));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult APZInputBridgeChild::RecvCallInputBlockCallback(
    uint64_t aInputBlockId, const APZHandledResult& aHandledResult) {
  auto it = mInputBlockCallbacks.find(aInputBlockId);
  if (it != mInputBlockCallbacks.end()) {
    it->second(aInputBlockId, aHandledResult);
    // The callback is one-shot; discard it after calling it.
    mInputBlockCallbacks.erase(it);
  }

  return IPC_OK();
}

void APZInputBridgeChild::ProcessUnhandledEvent(
    LayoutDeviceIntPoint* aRefPoint, ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutFocusSequenceNumber, LayersId* aOutLayersId) {
  MOZ_ASSERT(mIsOpen);
  APZThreadUtils::AssertOnControllerThread();

  SendProcessUnhandledEvent(*aRefPoint, aRefPoint, aOutTargetGuid,
                            aOutFocusSequenceNumber, aOutLayersId);
}

void APZInputBridgeChild::UpdateWheelTransaction(
    LayoutDeviceIntPoint aRefPoint, EventMessage aEventMessage,
    const Maybe<ScrollableLayerGuid>& aTargetGuid) {
  MOZ_ASSERT(mIsOpen);
  APZThreadUtils::AssertOnControllerThread();

  SendUpdateWheelTransaction(aRefPoint, aEventMessage, aTargetGuid);
}

}  // namespace layers
}  // namespace mozilla
