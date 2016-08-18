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
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/unused.h"
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
    Unused << SendUpdateFrame(aFrameMetrics);
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

bool
RemoteContentController::RecvZoomToRect(const uint32_t& aPresShellId,
                                        const ViewID& aViewId,
                                        const CSSRect& aRect,
                                        const uint32_t& aFlags)
{
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    apzcTreeManager->ZoomToRect(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                aRect, aFlags);
  }
  return true;
}

bool
RemoteContentController::RecvContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                                       const uint64_t& aInputBlockId,
                                                       const bool& aPreventDefault)
{
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvContentReceivedInputBlock; dropping message...");
    return false;
  }
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod<uint64_t,
                                                            bool>(apzcTreeManager,
                                                                  &IAPZCTreeManager::ContentReceivedInputBlock,
                                                                  aInputBlockId, aPreventDefault));

  }
  return true;
}

bool
RemoteContentController::RecvStartScrollbarDrag(const AsyncDragMetrics& aDragMetrics)
{
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    ScrollableLayerGuid guid(mLayersId, aDragMetrics.mPresShellId,
                             aDragMetrics.mViewId);

    APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                          <ScrollableLayerGuid,
                                           AsyncDragMetrics>(apzcTreeManager,
                                                             &IAPZCTreeManager::StartScrollbarDrag,
                                                             guid, aDragMetrics));
  }
  return true;
}

bool
RemoteContentController::RecvSetTargetAPZC(const uint64_t& aInputBlockId,
                                           nsTArray<ScrollableLayerGuid>&& aTargets)
{
  for (size_t i = 0; i < aTargets.Length(); i++) {
    if (aTargets[i].mLayersId != mLayersId) {
      // Guard against bad data from hijacked child processes
      NS_ERROR("Unexpected layers id in SetTargetAPZC; dropping message...");
      return false;
    }
  }
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    // need a local var to disambiguate between the SetTargetAPZC overloads.
    void (IAPZCTreeManager::*setTargetApzcFunc)(uint64_t, const nsTArray<ScrollableLayerGuid>&)
        = &IAPZCTreeManager::SetTargetAPZC;
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                          <uint64_t,
                                           StoreCopyPassByRRef<nsTArray<ScrollableLayerGuid>>>
                                          (apzcTreeManager, setTargetApzcFunc, aInputBlockId, aTargets));

  }
  return true;
}

bool
RemoteContentController::RecvSetAllowedTouchBehavior(const uint64_t& aInputBlockId,
                                                     nsTArray<TouchBehaviorFlags>&& aFlags)
{
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                          <uint64_t,
                                           StoreCopyPassByRRef<nsTArray<TouchBehaviorFlags>>>
                                          (apzcTreeManager,
                                           &IAPZCTreeManager::SetAllowedTouchBehavior,
                                           aInputBlockId, Move(aFlags)));
  }
  return true;
}

bool
RemoteContentController::RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                                                   const ViewID& aViewId,
                                                   const MaybeZoomConstraints& aConstraints)
{
  if (RefPtr<IAPZCTreeManager> apzcTreeManager = GetApzcTreeManager()) {
    apzcTreeManager->UpdateZoomConstraints(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                           aConstraints);
  }
  return true;
}

void
RemoteContentController::ActorDestroy(ActorDestroyReason aWhy)
{
  {
    MutexAutoLock lock(mMutex);
    mApzcTreeManager = nullptr;
  }
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

void
RemoteContentController::ChildAdopted()
{
  // Clear the cached APZCTreeManager.
  MutexAutoLock lock(mMutex);
  mApzcTreeManager = nullptr;
}

already_AddRefed<IAPZCTreeManager>
RemoteContentController::GetApzcTreeManager()
{
  // We can't get a ref to the APZCTreeManager until after the child is
  // created and the static getter knows which CompositorBridgeParent is
  // instantiated with this layers ID. That's why try to fetch it when
  // we first need it and cache the result.
  MutexAutoLock lock(mMutex);
  if (!mApzcTreeManager) {
    mApzcTreeManager = GPUProcessManager::Get()->GetAPZCTreeManagerForLayers(mLayersId);
  }
  RefPtr<IAPZCTreeManager> apzcTreeManager(mApzcTreeManager);
  return apzcTreeManager.forget();
}

} // namespace layers
} // namespace mozilla
