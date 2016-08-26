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

RemoteContentController::RemoteContentController()
  : mCompositorThread(MessageLoop::current())
  , mCanSend(true)
  , mMutex("RemoteContentController")
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
RemoteContentController::HandleTap(TapType aTapType,
                                   const LayoutDevicePoint& aPoint,
                                   Modifiers aModifiers,
                                   const ScrollableLayerGuid& aGuid,
                                   uint64_t aInputBlockId)
{
  if (MessageLoop::current() != mCompositorThread) {
    // We have to send messages from the compositor thread
    mCompositorThread->PostTask(NewRunnableMethod<TapType, LayoutDevicePoint, Modifiers,
                                        ScrollableLayerGuid, uint64_t>(this,
                                          &RemoteContentController::HandleTap,
                                          aTapType, aPoint, aModifiers, aGuid,
                                          aInputBlockId));
    return;
  }

  bool callTakeFocusForClickFromTap = (aTapType == TapType::eSingleTap);

  if (mCanSend) {
    Unused << SendHandleTap(aTapType, aPoint,
            aModifiers, aGuid, aInputBlockId, callTakeFocusForClickFromTap);
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
  Unused << SendUpdateOverscrollVelocity(aX, aY, aIsRootContent);
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
  Unused << SendUpdateOverscrollOffset(aX, aY, aIsRootContent);
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
  Unused << SendSetScrollingRootContent(aIsRootContent);
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
  // This controller could possibly be kept alive longer after this
  // by a RefPtr, but it is no longer valid to send messages.
  mCanSend = false;
}

void
RemoteContentController::Destroy()
{
  if (mCanSend) {
    Unused << SendDestroy();
  }
}

} // namespace layers
} // namespace mozilla
