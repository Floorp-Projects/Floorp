/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/RemoteContentController.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Unused.h"
#include "Units.h"
#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

RemoteContentController::RemoteContentController()
  : mCompositorThread(MessageLoop::current())
  , mCanSend(true)
{
}

RemoteContentController::~RemoteContentController()
{
}

void
RemoteContentController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(IsRepaintThread());

  if (mCanSend) {
    Unused << SendRequestContentRepaint(aFrameMetrics);
  }
}

void
RemoteContentController::HandleTapOnMainThread(TapType aTapType,
                                               LayoutDevicePoint aPoint,
                                               Modifiers aModifiers,
                                               ScrollableLayerGuid aGuid,
                                               uint64_t aInputBlockId)
{
  MOZ_ASSERT(NS_IsMainThread());

  dom::TabParent* tab = dom::TabParent::GetTabParentFromLayersId(aGuid.mLayersId);
  if (tab) {
    tab->SendHandleTap(aTapType, aPoint, aModifiers, aGuid, aInputBlockId);
  }
}

void
RemoteContentController::HandleTap(TapType aTapType,
                                   const LayoutDevicePoint& aPoint,
                                   Modifiers aModifiers,
                                   const ScrollableLayerGuid& aGuid,
                                   uint64_t aInputBlockId)
{
  APZThreadUtils::AssertOnControllerThread();

  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    MOZ_ASSERT(MessageLoop::current() == mCompositorThread);

    // The raw pointer to APZCTreeManagerParent is ok here because we are on the
    // compositor thread.
    APZCTreeManagerParent* apzctmp =
        CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
    if (apzctmp) {
      Unused << apzctmp->SendHandleTap(aTapType, aPoint, aModifiers, aGuid, aInputBlockId);
    }

    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  if (NS_IsMainThread()) {
    HandleTapOnMainThread(aTapType, aPoint, aModifiers, aGuid, aInputBlockId);
  } else {
    // We don't want to get the TabParent or call TabParent::SendHandleTap() from a non-main thread (this might happen
    // on Android, where this is called from the Java UI thread)
    NS_DispatchToMainThread(NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers, ScrollableLayerGuid, uint64_t>
        (this, &RemoteContentController::HandleTapOnMainThread, aTapType, aPoint, aModifiers, aGuid, aInputBlockId));
  }
}

void
RemoteContentController::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                            const ScrollableLayerGuid& aGuid,
                                            LayoutDeviceCoord aSpanChange,
                                            Modifiers aModifiers)
{
  APZThreadUtils::AssertOnControllerThread();

  // For now we only ever want to handle this NotifyPinchGesture message in
  // the parent process, even if the APZ is sending it to a content process.

  // If we're in the GPU process, try to find a handle to the parent process
  // and send it there.
  if (XRE_IsGPUProcess()) {
    MOZ_ASSERT(MessageLoop::current() == mCompositorThread);

    // The raw pointer to APZCTreeManagerParent is ok here because we are on the
    // compositor thread.
    APZCTreeManagerParent* apzctmp =
        CompositorBridgeParent::GetApzcTreeManagerParentForRoot(aGuid.mLayersId);
    if (apzctmp) {
      Unused << apzctmp->SendNotifyPinchGesture(aType, aGuid, aSpanChange, aModifiers);
      return;
    }
  }

  // If we're in the parent process, handle it directly. We don't have a handle
  // to the widget though, so we fish out the ChromeProcessController and
  // delegate to that instead.
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<GeckoContentController> rootController =
        CompositorBridgeParent::GetGeckoContentControllerForRoot(aGuid.mLayersId);
    if (rootController) {
      rootController->NotifyPinchGesture(aType, aGuid, aSpanChange, aModifiers);
    }
  }
}

void
RemoteContentController::PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->PostTaskToUiThread(Move(aTask), aDelayMs);
#else
  (MessageLoop::current() ? MessageLoop::current() : mCompositorThread)->
    PostDelayedTask(Move(aTask), aDelayMs);
#endif
}

bool
RemoteContentController::IsRepaintThread()
{
  return MessageLoop::current() == mCompositorThread;
}

void
RemoteContentController::DispatchToRepaintThread(already_AddRefed<Runnable> aTask)
{
  mCompositorThread->PostTask(Move(aTask));
}

void
RemoteContentController::NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                              APZStateChange aChange,
                                              int aArg)
{
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<ScrollableLayerGuid,
                                        APZStateChange,
                                        int>(this,
                                             &RemoteContentController::NotifyAPZStateChange,
                                             aGuid, aChange, aArg));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyAPZStateChange(aGuid, aChange, aArg);
  }
}

void
RemoteContentController::UpdateOverscrollVelocity(float aX, float aY, bool aIsRootContent)
{
  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<float,
                                        float, bool>(this,
                                             &RemoteContentController::UpdateOverscrollVelocity,
                                             aX, aY, aIsRootContent));
    return;
  }
  if (mCanSend) {
    Unused << SendUpdateOverscrollVelocity(aX, aY, aIsRootContent);
  }
}

void
RemoteContentController::UpdateOverscrollOffset(float aX, float aY, bool aIsRootContent)
{
  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<float,
                                        float, bool>(this,
                                             &RemoteContentController::UpdateOverscrollOffset,
                                             aX, aY, aIsRootContent));
    return;
  }
  if (mCanSend) {
    Unused << SendUpdateOverscrollOffset(aX, aY, aIsRootContent);
  }
}

void
RemoteContentController::SetScrollingRootContent(bool aIsRootContent)
{
  if (MessageLoop::current() != mCompositorThread) {
    mCompositorThread->PostTask(NewRunnableMethod<bool>(this,
                                             &RemoteContentController::SetScrollingRootContent,
                                             aIsRootContent));
    return;
  }
  if (mCanSend) {
    Unused << SendSetScrollingRootContent(aIsRootContent);
  }
}

void
RemoteContentController::NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                                   const nsString& aEvent)
{
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<FrameMetrics::ViewID,
                                        nsString>(this,
                                                  &RemoteContentController::NotifyMozMouseScrollEvent,
                                                  aScrollId, aEvent));
    return;
  }

  if (mCanSend) {
    Unused << SendNotifyMozMouseScrollEvent(aScrollId, aEvent);
  }
}

void
RemoteContentController::NotifyFlushComplete()
{
  MOZ_ASSERT(IsRepaintThread());

  if (mCanSend) {
    Unused << SendNotifyFlushComplete();
  }
}

void
RemoteContentController::ActorDestroy(ActorDestroyReason aWhy)
{
  // This controller could possibly be kept alive longer after this
  // by a RefPtr, but it is no longer valid to send messages.
  mCanSend = false;
}

void
RemoteContentController::Destroy()
{
  if (mCanSend) {
    mCanSend = false;
    Unused << SendDestroy();
  }
}

} // namespace layers
} // namespace mozilla
