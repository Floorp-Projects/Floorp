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

static std::map<uint64_t, RefPtr<RemoteContentController>> sDestroyedControllers;

RemoteContentController::RemoteContentController(uint64_t aLayersId,
                                                 dom::TabParent* aBrowserParent)
  : mUILoop(MessageLoop::current())
  , mLayersId(aLayersId)
  , mBrowserParent(aBrowserParent)
  , mMutex("RemoteContentController")
{
  MOZ_ASSERT(NS_IsMainThread());
}

RemoteContentController::~RemoteContentController()
{
}

void
RemoteContentController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (CanSend()) {
    Unused << SendRequestContentRepaint(aFrameMetrics);
  }
}

void
RemoteContentController::HandleTap(TapType aTapType,
                                   const LayoutDevicePoint& aPoint,
                                   Modifiers aModifiers,
                                   const ScrollableLayerGuid& aGuid,
                                   uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                                        ScrollableLayerGuid, uint64_t>(this,
                                          &RemoteContentController::HandleTap,
                                          aTapType, aPoint, aModifiers, aGuid,
                                          aInputBlockId));
    return;
  }

  bool callTakeFocusForClickFromTap = (aTapType == TapType::eSingleTap);
  if (callTakeFocusForClickFromTap && mBrowserParent) {
    layout::RenderFrameParent* frame = mBrowserParent->GetRenderFrame();
    if (frame && mLayersId == frame->GetLayersId()) {
      // Avoid going over IPC and back for calling TakeFocusForClickFromTap,
      // since the right RenderFrameParent is living in this process.
      frame->TakeFocusForClickFromTap();
      callTakeFocusForClickFromTap = false;
    }
  }

  if (CanSend()) {
    Unused << SendHandleTap(aTapType, mBrowserParent->AdjustTapToChildWidget(aPoint),
            aModifiers, aGuid, aInputBlockId, callTakeFocusForClickFromTap);
  }
}

void
RemoteContentController::PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->PostTaskToUiThread(Move(aTask), aDelayMs);
#else
  (MessageLoop::current() ? MessageLoop::current() : mUILoop)->
    PostDelayedTask(Move(aTask), aDelayMs);
#endif
}

bool
RemoteContentController::GetTouchSensitiveRegion(CSSRect* aOutRegion)
{
  MutexAutoLock lock(mMutex);
  if (mTouchSensitiveRegion.IsEmpty()) {
    return false;
  }

  *aOutRegion = CSSRect::FromAppUnits(mTouchSensitiveRegion.GetBounds());
  return true;
}

void
RemoteContentController::NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                              APZStateChange aChange,
                                              int aArg)
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<ScrollableLayerGuid,
                                        APZStateChange,
                                        int>(this,
                                             &RemoteContentController::NotifyAPZStateChange,
                                             aGuid, aChange, aArg));
    return;
  }
  if (CanSend()) {
    Unused << SendNotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
  }
}

void
RemoteContentController::NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                                   const nsString& aEvent)
{
  if (MessageLoop::current() != mUILoop) {
    mUILoop->PostTask(NewRunnableMethod<FrameMetrics::ViewID,
                                        nsString>(this,
                                                  &RemoteContentController::NotifyMozMouseScrollEvent,
                                                  aScrollId, aEvent));
    return;
  }

  if (mBrowserParent) {
    Unused << mBrowserParent->SendMouseScrollTestEvent(mLayersId, aScrollId, aEvent);
  }
}

void
RemoteContentController::NotifyFlushComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (CanSend()) {
    Unused << SendNotifyFlushComplete();
  }
}

bool
RemoteContentController::RecvUpdateHitRegion(const nsRegion& aRegion)
{
  MutexAutoLock lock(mMutex);
  mTouchSensitiveRegion = aRegion;
  return true;
}

void
RemoteContentController::ActorDestroy(ActorDestroyReason aWhy)
{
  mBrowserParent = nullptr;

  uint64_t key = mLayersId;
  NS_DispatchToMainThread(NS_NewRunnableFunction([key]() {
    // sDestroyedControllers may or may not contain the key, depending on
    // whether or not SendDestroy() was successfully sent out or not.
    sDestroyedControllers.erase(key);
  }));
}

void
RemoteContentController::Destroy()
{
  RefPtr<RemoteContentController> controller = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction([controller] {
    if (controller->CanSend()) {
      // Gfx code is done with this object, and it will probably get destroyed
      // soon. However, if CanSend() is true, ActorDestroy has not yet been
      // called, which means IPC code still has a handle to this object. We need
      // to keep it alive until we get the ActorDestroy call, either via the
      // __delete__ message or via IPC shutdown on our end.
      uint64_t key = controller->mLayersId;
      MOZ_ASSERT(sDestroyedControllers.find(key) == sDestroyedControllers.end());
      sDestroyedControllers[key] = controller;
      Unused << controller->SendDestroy();
    }
  }));
}

} // namespace layers
} // namespace mozilla
