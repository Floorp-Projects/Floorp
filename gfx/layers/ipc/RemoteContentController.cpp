/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RemoteContentController.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/MatrixMessage.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Unused.h"
#include "Units.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

RemoteContentController::RemoteContentController()
    : mCompositorThread(MessageLoop::current()), mCanSend(true) {}

RemoteContentController::~RemoteContentController() {}

void RemoteContentController::NotifyLayerTransforms(
    const nsTArray<MatrixMessage>& aTransforms) {
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<nsTArray<MatrixMessage>>(
        "layers::RemoteContentController::NotifyLayerTransforms", this,
        &RemoteContentController::NotifyLayerTransforms, aTransforms));
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

void RemoteContentController::HandleTapOnMainThread(TapType aTapType,
                                                    LayoutDevicePoint aPoint,
                                                    Modifiers aModifiers,
                                                    ScrollableLayerGuid aGuid,
                                                    uint64_t aInputBlockId) {
  MOZ_ASSERT(NS_IsMainThread());

  dom::BrowserParent* tab =
      dom::BrowserParent::GetBrowserParentFromLayersId(aGuid.mLayersId);
  if (tab) {
    tab->SendHandleTap(aTapType, aPoint, aModifiers, aGuid, aInputBlockId);
  }
}

void RemoteContentController::HandleTapOnCompositorThread(
    TapType aTapType, LayoutDevicePoint aPoint, Modifiers aModifiers,
    ScrollableLayerGuid aGuid, uint64_t aInputBlockId) {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(MessageLoop::current() == mCompositorThread);

  // The raw pointer to APZCTreeManagerParent is ok here because we are on the
  // compositor thread.
  APZCTreeManagerParent* apzctmp =
      CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
  if (apzctmp) {
    Unused << apzctmp->SendHandleTap(aTapType, aPoint, aModifiers, aGuid,
                                     aInputBlockId);
  }
}

void RemoteContentController::HandleTap(TapType aTapType,
                                        const LayoutDevicePoint& aPoint,
                                        Modifiers aModifiers,
                                        const ScrollableLayerGuid& aGuid,
                                        uint64_t aInputBlockId) {
  APZThreadUtils::AssertOnControllerThread();

  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    if (MessageLoop::current() == mCompositorThread) {
      HandleTapOnCompositorThread(aTapType, aPoint, aModifiers, aGuid,
                                  aInputBlockId);
    } else {
      // We have to send messages from the compositor thread
      mCompositorThread->PostTask(
          NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                            ScrollableLayerGuid, uint64_t>(
              "layers::RemoteContentController::HandleTapOnCompositorThread",
              this, &RemoteContentController::HandleTapOnCompositorThread,
              aTapType, aPoint, aModifiers, aGuid, aInputBlockId));
    }
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  if (NS_IsMainThread()) {
    HandleTapOnMainThread(aTapType, aPoint, aModifiers, aGuid, aInputBlockId);
  } else {
    // We don't want to get the BrowserParent or call
    // BrowserParent::SendHandleTap() from a non-main thread (this might happen
    // on Android, where this is called from the Java UI thread)
    NS_DispatchToMainThread(
        NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                          ScrollableLayerGuid, uint64_t>(
            "layers::RemoteContentController::HandleTapOnMainThread", this,
            &RemoteContentController::HandleTapOnMainThread, aTapType, aPoint,
            aModifiers, aGuid, aInputBlockId));
  }
}

void RemoteContentController::NotifyPinchGestureOnCompositorThread(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    LayoutDeviceCoord aSpanChange, Modifiers aModifiers) {
  MOZ_ASSERT(MessageLoop::current() == mCompositorThread);

  // The raw pointer to APZCTreeManagerParent is ok here because we are on the
  // compositor thread.
  APZCTreeManagerParent* apzctmp =
      CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
  if (apzctmp) {
    Unused << apzctmp->SendNotifyPinchGesture(aType, aGuid, aSpanChange,
                                              aModifiers);
  }
}

void RemoteContentController::NotifyPinchGesture(
    PinchGestureInput::PinchGestureType aType, const ScrollableLayerGuid& aGuid,
    LayoutDeviceCoord aSpanChange, Modifiers aModifiers) {
  APZThreadUtils::AssertOnControllerThread();

  // For now we only ever want to handle this NotifyPinchGesture message in
  // the parent process, even if the APZ is sending it to a content process.

  // If we're in the GPU process, try to find a handle to the parent process
  // and send it there.
  if (XRE_IsGPUProcess()) {
    if (MessageLoop::current() == mCompositorThread) {
      NotifyPinchGestureOnCompositorThread(aType, aGuid, aSpanChange,
                                           aModifiers);
    } else {
      mCompositorThread->PostTask(
          NewRunnableMethod<PinchGestureInput::PinchGestureType,
                            ScrollableLayerGuid, LayoutDeviceCoord, Modifiers>(
              "layers::RemoteContentController::"
              "NotifyPinchGestureOnCompositorThread",
              this,
              &RemoteContentController::NotifyPinchGestureOnCompositorThread,
              aType, aGuid, aSpanChange, aModifiers));
    }
    return;
  }

  // If we're in the parent process, handle it directly. We don't have a handle
  // to the widget though, so we fish out the ChromeProcessController and
  // delegate to that instead.
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<GeckoContentController> rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(
            aGuid.mLayersId);
    if (rootController) {
      rootController->NotifyPinchGesture(aType, aGuid, aSpanChange, aModifiers);
    }
  }
}

void RemoteContentController::PostDelayedTask(already_AddRefed<Runnable> aTask,
                                              int aDelayMs) {
  (MessageLoop::current() ? MessageLoop::current() : mCompositorThread)
      ->PostDelayedTask(std::move(aTask), aDelayMs);
}

bool RemoteContentController::IsRepaintThread() {
  return MessageLoop::current() == mCompositorThread;
}

void RemoteContentController::DispatchToRepaintThread(
    already_AddRefed<Runnable> aTask) {
  mCompositorThread->PostTask(std::move(aTask));
}

void RemoteContentController::NotifyAPZStateChange(
    const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg) {
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(
        NewRunnableMethod<ScrollableLayerGuid, APZStateChange, int>(
            "layers::RemoteContentController::NotifyAPZStateChange", this,
            &RemoteContentController::NotifyAPZStateChange, aGuid, aChange,
            aArg));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAPZStateChange(aGuid, aChange, aArg);
  }
}

void RemoteContentController::UpdateOverscrollVelocity(float aX, float aY,
                                                       bool aIsRootContent) {
  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<float, float, bool>(
        "layers::RemoteContentController::UpdateOverscrollVelocity", this,
        &RemoteContentController::UpdateOverscrollVelocity, aX, aY,
        aIsRootContent));
    return;
  }
  if (mCanSend) {
    Unused << SendUpdateOverscrollVelocity(aX, aY, aIsRootContent);
  }
}

void RemoteContentController::UpdateOverscrollOffset(float aX, float aY,
                                                     bool aIsRootContent) {
  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<float, float, bool>(
        "layers::RemoteContentController::UpdateOverscrollOffset", this,
        &RemoteContentController::UpdateOverscrollOffset, aX, aY,
        aIsRootContent));
    return;
  }
  if (mCanSend) {
    Unused << SendUpdateOverscrollOffset(aX, aY, aIsRootContent);
  }
}

void RemoteContentController::NotifyMozMouseScrollEvent(
    const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(
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
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<uint64_t,
                                                  ScrollableLayerGuid::ViewID,
                                                  ScrollDirection>(
        "layers::RemoteContentController::NotifyAsyncScrollbarDragInitiated",
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
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
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
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<ScrollableLayerGuid::ViewID>(
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

  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<ScrollableLayerGuid>(
        "layers::RemoteContentController::CancelAutoscrollCrossProcess", this,
        &RemoteContentController::CancelAutoscrollCrossProcess, aGuid));
    return;
  }

  // The raw pointer to APZCTreeManagerParent is ok here because we are on the
  // compositor thread.
  if (APZCTreeManagerParent* parent =
          CompositorBridgeParent::GetApzcTreeManagerParentForRoot(
              aGuid.mLayersId)) {
    Unused << parent->SendCancelAutoscroll(aGuid.mScrollId);
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
  // The actor on the other side is about to get destroyed, so let's not send
  // it any more messages.
  mCanSend = false;
  return IPC_OK();
}

bool RemoteContentController::IsRemote() { return true; }

}  // namespace layers
}  // namespace mozilla
