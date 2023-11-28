/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RemoteContentController.h"

#include "CompositorThread.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/MatrixMessage.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Unused.h"
#include "Units.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

static mozilla::LazyLogModule sApzRemoteLog("apz.cc.remote");

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

RemoteContentController::RemoteContentController()
    : mCompositorThread(NS_GetCurrentThread()), mCanSend(true) {
  MOZ_ASSERT(CompositorThread()->IsOnCurrentThread());
}

RemoteContentController::~RemoteContentController() = default;

void RemoteContentController::NotifyLayerTransforms(
    nsTArray<MatrixMessage>&& aTransforms) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(
        NewRunnableMethod<StoreCopyPassByRRef<nsTArray<MatrixMessage>>>(
            "layers::RemoteContentController::NotifyLayerTransforms", this,
            &RemoteContentController::NotifyLayerTransforms,
            std::move(aTransforms)));
    return;
  }

  if (mCanSend) {
    Unused << SendLayerTransforms(aTransforms);
  }
}

void RemoteContentController::RequestContentRepaint(
    const RepaintRequest& aRequest) {
  MOZ_ASSERT(IsRepaintThread());

  if (mCanSend) {
    Unused << SendRequestContentRepaint(aRequest);
  }
}

void RemoteContentController::HandleTapOnMainThread(
    TapType aTapType, LayoutDevicePoint aPoint, Modifiers aModifiers,
    ScrollableLayerGuid aGuid, uint64_t aInputBlockId,
    const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics) {
  MOZ_LOG(sApzRemoteLog, LogLevel::Debug,
          ("HandleTapOnMainThread(%d)", (int)aTapType));
  MOZ_ASSERT(NS_IsMainThread());

  dom::BrowserParent* tab =
      dom::BrowserParent::GetBrowserParentFromLayersId(aGuid.mLayersId);
  if (tab) {
    tab->SendHandleTap(aTapType, aPoint, aModifiers, aGuid, aInputBlockId,
                       aDoubleTapToZoomMetrics);
  }
}

void RemoteContentController::HandleTapOnCompositorThread(
    TapType aTapType, LayoutDevicePoint aPoint, Modifiers aModifiers,
    ScrollableLayerGuid aGuid, uint64_t aInputBlockId,
    const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics) {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(mCompositorThread->IsOnCurrentThread());

  // The raw pointer to APZCTreeManagerParent is ok here because we are on
  // the compositor thread.
  APZCTreeManagerParent* apzctmp =
      CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
  if (apzctmp) {
    Unused << apzctmp->SendHandleTap(aTapType, aPoint, aModifiers, aGuid,
                                     aInputBlockId, aDoubleTapToZoomMetrics);
  }
}

void RemoteContentController::HandleTap(
    TapType aTapType, const LayoutDevicePoint& aPoint, Modifiers aModifiers,
    const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId,
    const Maybe<DoubleTapToZoomMetrics>& aDoubleTapToZoomMetrics) {
  MOZ_LOG(sApzRemoteLog, LogLevel::Debug, ("HandleTap(%d)", (int)aTapType));
  APZThreadUtils::AssertOnControllerThread();

  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    if (mCompositorThread->IsOnCurrentThread()) {
      HandleTapOnCompositorThread(aTapType, aPoint, aModifiers, aGuid,
                                  aInputBlockId, aDoubleTapToZoomMetrics);
    } else {
      // We have to send messages from the compositor thread
      mCompositorThread->Dispatch(
          NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                            ScrollableLayerGuid, uint64_t,
                            Maybe<DoubleTapToZoomMetrics>>(
              "layers::RemoteContentController::HandleTapOnCompositorThread",
              this, &RemoteContentController::HandleTapOnCompositorThread,
              aTapType, aPoint, aModifiers, aGuid, aInputBlockId,
              aDoubleTapToZoomMetrics));
    }
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  if (NS_IsMainThread()) {
    HandleTapOnMainThread(aTapType, aPoint, aModifiers, aGuid, aInputBlockId,
                          aDoubleTapToZoomMetrics);
  } else {
    // We must be on Android, running on the Java UI thread
#ifndef MOZ_WIDGET_ANDROID
    MOZ_ASSERT(false);
#else
    // We don't want to get the BrowserParent or call
    // BrowserParent::SendHandleTap() from a non-main thread, so we need to
    // redispatch to the main thread. However, we should use the same
    // mechanism that the Android widget uses when dispatching input events
    // to Gecko, which is nsAppShell::PostEvent. Note in particular that
    // using NS_DispatchToMainThread would post to a different message loop,
    // and introduces the possibility of this tap event getting processed
    // out of order with respect to the touch events that synthesized it.
    mozilla::jni::DispatchToGeckoPriorityQueue(
        NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                          ScrollableLayerGuid, uint64_t,
                          Maybe<DoubleTapToZoomMetrics>>(
            "layers::RemoteContentController::HandleTapOnMainThread", this,
            &RemoteContentController::HandleTapOnMainThread, aTapType, aPoint,
            aModifiers, aGuid, aInputBlockId, aDoubleTapToZoomMetrics));
#endif
  }
}

void RemoteContentController::NotifyPinchGestureOnCompositorThread(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    const LayoutDevicePoint& aFocusPoint, LayoutDeviceCoord aSpanChange,
    Modifiers aModifiers) {
  MOZ_ASSERT(mCompositorThread->IsOnCurrentThread());

  // The raw pointer to APZCTreeManagerParent is ok here because we are on
  // the compositor thread.
  APZCTreeManagerParent* apzctmp =
      CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
  if (apzctmp) {
    Unused << apzctmp->SendNotifyPinchGesture(aType, aGuid, aFocusPoint,
                                              aSpanChange, aModifiers);
  }
}

void RemoteContentController::NotifyPinchGesture(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    const LayoutDevicePoint& aFocusPoint, LayoutDeviceCoord aSpanChange,
    Modifiers aModifiers) {
  APZThreadUtils::AssertOnControllerThread();

  // For now we only ever want to handle this NotifyPinchGesture message in
  // the parent process, even if the APZ is sending it to a content process.

  // If we're in the GPU process, try to find a handle to the parent process
  // and send it there.
  if (XRE_IsGPUProcess()) {
    if (mCompositorThread->IsOnCurrentThread()) {
      NotifyPinchGestureOnCompositorThread(aType, aGuid, aFocusPoint,
                                           aSpanChange, aModifiers);
    } else {
      mCompositorThread->Dispatch(
          NewRunnableMethod<PinchGestureInput::PinchGestureType,
                            ScrollableLayerGuid, LayoutDevicePoint,
                            LayoutDeviceCoord, Modifiers>(
              "layers::RemoteContentController::"
              "NotifyPinchGestureOnCompositorThread",
              this,
              &RemoteContentController::NotifyPinchGestureOnCompositorThread,
              aType, aGuid, aFocusPoint, aSpanChange, aModifiers));
    }
    return;
  }

  // If we're in the parent process, handle it directly. We don't have a
  // handle to the widget though, so we fish out the ChromeProcessController
  // and delegate to that instead.
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<GeckoContentController> rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      rootController->NotifyPinchGesture(aType, aGuid, aFocusPoint, aSpanChange,
                                         aModifiers);
    }
  }
}

bool RemoteContentController::IsRepaintThread() {
  return mCompositorThread->IsOnCurrentThread();
}

void RemoteContentController::DispatchToRepaintThread(
    already_AddRefed<Runnable> aTask) {
  mCompositorThread->Dispatch(std::move(aTask));
}

void RemoteContentController::NotifyAPZStateChange(
    const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg,
    Maybe<uint64_t> aInputBlockId) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(
        NewRunnableMethod<ScrollableLayerGuid, APZStateChange, int,
                          Maybe<uint64_t>>(
            "layers::RemoteContentController::NotifyAPZStateChange", this,
            &RemoteContentController::NotifyAPZStateChange, aGuid, aChange,
            aArg, aInputBlockId));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAPZStateChange(aGuid, aChange, aArg, aInputBlockId);
  }
}

void RemoteContentController::UpdateOverscrollVelocity(
    const ScrollableLayerGuid& aGuid, float aX, float aY, bool aIsRootContent) {
  if (XRE_IsParentProcess()) {
#ifdef MOZ_WIDGET_ANDROID
    // We always want these to go to the parent process on Android
    if (!NS_IsMainThread()) {
      mozilla::jni::DispatchToGeckoPriorityQueue(
          NewRunnableMethod<ScrollableLayerGuid, float, float, bool>(
              "layers::RemoteContentController::UpdateOverscrollVelocity", this,
              &RemoteContentController::UpdateOverscrollVelocity, aGuid, aX, aY,
              aIsRootContent));
      return;
    }
#endif

    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    RefPtr<GeckoContentController> rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      rootController->UpdateOverscrollVelocity(aGuid, aX, aY, aIsRootContent);
    }
  } else if (XRE_IsGPUProcess()) {
    if (!mCompositorThread->IsOnCurrentThread()) {
      mCompositorThread->Dispatch(
          NewRunnableMethod<ScrollableLayerGuid, float, float, bool>(
              "layers::RemoteContentController::UpdateOverscrollVelocity", this,
              &RemoteContentController::UpdateOverscrollVelocity, aGuid, aX, aY,
              aIsRootContent));
      return;
    }

    MOZ_RELEASE_ASSERT(mCompositorThread->IsOnCurrentThread());
    GeckoContentController* rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      MOZ_RELEASE_ASSERT(rootController->IsRemote());
      Unused << static_cast<RemoteContentController*>(rootController)
                    ->SendUpdateOverscrollVelocity(aGuid, aX, aY,
                                                   aIsRootContent);
    }
  }
}

void RemoteContentController::UpdateOverscrollOffset(
    const ScrollableLayerGuid& aGuid, float aX, float aY, bool aIsRootContent) {
  if (XRE_IsParentProcess()) {
#ifdef MOZ_WIDGET_ANDROID
    // We always want these to go to the parent process on Android
    if (!NS_IsMainThread()) {
      mozilla::jni::DispatchToGeckoPriorityQueue(
          NewRunnableMethod<ScrollableLayerGuid, float, float, bool>(
              "layers::RemoteContentController::UpdateOverscrollOffset", this,
              &RemoteContentController::UpdateOverscrollOffset, aGuid, aX, aY,
              aIsRootContent));
      return;
    }
#endif

    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    RefPtr<GeckoContentController> rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      rootController->UpdateOverscrollOffset(aGuid, aX, aY, aIsRootContent);
    }
  } else if (XRE_IsGPUProcess()) {
    if (!mCompositorThread->IsOnCurrentThread()) {
      mCompositorThread->Dispatch(
          NewRunnableMethod<ScrollableLayerGuid, float, float, bool>(
              "layers::RemoteContentController::UpdateOverscrollOffset", this,
              &RemoteContentController::UpdateOverscrollOffset, aGuid, aX, aY,
              aIsRootContent));
      return;
    }

    MOZ_RELEASE_ASSERT(mCompositorThread->IsOnCurrentThread());
    GeckoContentController* rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      MOZ_RELEASE_ASSERT(rootController->IsRemote());
      Unused << static_cast<RemoteContentController*>(rootController)
                    ->SendUpdateOverscrollOffset(aGuid, aX, aY, aIsRootContent);
    }
  }
}

void RemoteContentController::NotifyMozMouseScrollEvent(
    const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(
        NewRunnableMethod<ScrollableLayerGuid::ViewID, nsString>(
            "layers::RemoteContentController::NotifyMozMouseScrollEvent", this,
            &RemoteContentController::NotifyMozMouseScrollEvent, aScrollId,
            aEvent));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyMozMouseScrollEvent(aScrollId, aEvent);
  }
}

void RemoteContentController::NotifyFlushComplete() {
  MOZ_ASSERT(IsRepaintThread());

  if (mCanSend) {
    Unused << SendNotifyFlushComplete();
  }
}

void RemoteContentController::NotifyAsyncScrollbarDragInitiated(
    uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
    ScrollDirection aDirection) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(
        NewRunnableMethod<uint64_t, ScrollableLayerGuid::ViewID,
                          ScrollDirection>(
            "layers::RemoteContentController::"
            "NotifyAsyncScrollbarDragInitiated",
            this, &RemoteContentController::NotifyAsyncScrollbarDragInitiated,
            aDragBlockId, aScrollId, aDirection));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAsyncScrollbarDragInitiated(aDragBlockId, aScrollId,
                                                    aDirection);
  }
}

void RemoteContentController::NotifyAsyncScrollbarDragRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
        "layers::RemoteContentController::NotifyAsyncScrollbarDragRejected",
        this, &RemoteContentController::NotifyAsyncScrollbarDragRejected,
        aScrollId));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAsyncScrollbarDragRejected(aScrollId);
  }
}

void RemoteContentController::NotifyAsyncAutoscrollRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  if (!mCompositorThread->IsOnCurrentThread()) {
    // We have to send messages from the compositor thread
    mCompositorThread->Dispatch(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
        "layers::RemoteContentController::NotifyAsyncAutoscrollRejected", this,
        &RemoteContentController::NotifyAsyncAutoscrollRejected, aScrollId));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAsyncAutoscrollRejected(aScrollId);
  }
}

void RemoteContentController::CancelAutoscroll(
    const ScrollableLayerGuid& aGuid) {
  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    CancelAutoscrollCrossProcess(aGuid);
  } else {
    CancelAutoscrollInProcess(aGuid);
  }
}

void RemoteContentController::CancelAutoscrollInProcess(
    const ScrollableLayerGuid& aGuid) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NewRunnableMethod<ScrollableLayerGuid>(
        "layers::RemoteContentController::CancelAutoscrollInProcess", this,
        &RemoteContentController::CancelAutoscrollInProcess, aGuid));
    return;
  }

  APZCCallbackHelper::CancelAutoscroll(aGuid.mScrollId);
}

void RemoteContentController::CancelAutoscrollCrossProcess(
    const ScrollableLayerGuid& aGuid) {
  MOZ_ASSERT(XRE_IsGPUProcess());

  if (!mCompositorThread->IsOnCurrentThread()) {
    mCompositorThread->Dispatch(NewRunnableMethod<ScrollableLayerGuid>(
        "layers::RemoteContentController::CancelAutoscrollCrossProcess", this,
        &RemoteContentController::CancelAutoscrollCrossProcess, aGuid));
    return;
  }

  // The raw pointer to APZCTreeManagerParent is ok here because we are on
  // the compositor thread.
  if (APZCTreeManagerParent* parent =
          CompositorBridgeParent::GetApzcTreeManagerParentForRoot(
              aGuid.mLayersId)) {
    Unused << parent->SendCancelAutoscroll(aGuid.mScrollId);
  }
}

void RemoteContentController::NotifyScaleGestureComplete(
    const ScrollableLayerGuid& aGuid, float aScale) {
  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    NotifyScaleGestureCompleteCrossProcess(aGuid, aScale);
  } else {
    NotifyScaleGestureCompleteInProcess(aGuid, aScale);
  }
}

void RemoteContentController::NotifyScaleGestureCompleteInProcess(
    const ScrollableLayerGuid& aGuid, float aScale) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NewRunnableMethod<ScrollableLayerGuid, float>(
        "layers::RemoteContentController::"
        "NotifyScaleGestureCompleteInProcess",
        this, &RemoteContentController::NotifyScaleGestureCompleteInProcess,
        aGuid, aScale));
    return;
  }

  RefPtr<GeckoContentController> rootController =
      CompositorBridgeParent::GetGeckoContentControllerForRoot(aGuid.mLayersId);
  if (rootController) {
    MOZ_ASSERT(rootController != this);
    if (rootController != this) {
      rootController->NotifyScaleGestureComplete(aGuid, aScale);
    }
  }
}

void RemoteContentController::NotifyScaleGestureCompleteCrossProcess(
    const ScrollableLayerGuid& aGuid, float aScale) {
  MOZ_ASSERT(XRE_IsGPUProcess());

  if (!mCompositorThread->IsOnCurrentThread()) {
    mCompositorThread->Dispatch(NewRunnableMethod<ScrollableLayerGuid, float>(
        "layers::RemoteContentController::"
        "NotifyScaleGestureCompleteCrossProcess",
        this, &RemoteContentController::NotifyScaleGestureCompleteCrossProcess,
        aGuid, aScale));
    return;
  }

  // The raw pointer to APZCTreeManagerParent is ok here because we are on
  // the compositor thread.
  if (APZCTreeManagerParent* parent =
          CompositorBridgeParent::GetApzcTreeManagerParentForRoot(
              aGuid.mLayersId)) {
    Unused << parent->SendNotifyScaleGestureComplete(aGuid.mScrollId, aScale);
  }
}

void RemoteContentController::ActorDestroy(ActorDestroyReason aWhy) {
  // This controller could possibly be kept alive longer after this
  // by a RefPtr, but it is no longer valid to send messages.
  mCanSend = false;
}

void RemoteContentController::Destroy() {
  if (mCanSend) {
    mCanSend = false;
    Unused << SendDestroy();
  }
}

mozilla::ipc::IPCResult RemoteContentController::RecvDestroy() {
  // The actor on the other side is about to get destroyed, so let's not
  // send it any more messages.
  mCanSend = false;
  return IPC_OK();
}

bool RemoteContentController::IsRemote() { return true; }

}  // namespace layers
}  // namespace mozilla
