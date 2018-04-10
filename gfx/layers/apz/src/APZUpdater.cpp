/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZUpdater.h"

#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "base/task.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

StaticMutex APZUpdater::sWindowIdLock;
std::unordered_map<uint64_t, APZUpdater*> APZUpdater::sWindowIdMap;


APZUpdater::APZUpdater(const RefPtr<APZCTreeManager>& aApz)
  : mApz(aApz)
#ifdef DEBUG
  , mUpdaterThreadQueried(false)
#endif
  , mQueueLock("APZUpdater::QueueLock")
{
  MOZ_ASSERT(aApz);
  mApz->SetUpdater(this);
}

APZUpdater::~APZUpdater()
{
  mApz->SetUpdater(nullptr);

  StaticMutexAutoLock lock(sWindowIdLock);
  if (mWindowId) {
    // Ensure that ClearTree was called and the task got run
    MOZ_ASSERT(sWindowIdMap.find(wr::AsUint64(*mWindowId)) == sWindowIdMap.end());
  }
}

bool
APZUpdater::HasTreeManager(const RefPtr<APZCTreeManager>& aApz)
{
  return aApz.get() == mApz.get();
}

void
APZUpdater::SetWebRenderWindowId(const wr::WindowId& aWindowId)
{
  StaticMutexAutoLock lock(sWindowIdLock);
  MOZ_ASSERT(!mWindowId);
  mWindowId = Some(aWindowId);
  sWindowIdMap[wr::AsUint64(aWindowId)] = this;
}

/*static*/ void
APZUpdater::SetUpdaterThread(const wr::WrWindowId& aWindowId)
{
  if (RefPtr<APZUpdater> updater = GetUpdater(aWindowId)) {
    // Ensure nobody tried to use the updater thread before this point.
    MOZ_ASSERT(!updater->mUpdaterThreadQueried);
    updater->mUpdaterThreadId = Some(PlatformThread::CurrentId());
  }
}

/*static*/ void
APZUpdater::ProcessPendingTasks(const wr::WrWindowId& aWindowId)
{
  if (RefPtr<APZUpdater> updater = GetUpdater(aWindowId)) {
    MutexAutoLock lock(updater->mQueueLock);
    for (auto task : updater->mUpdaterQueue) {
      task->Run();
    }
    updater->mUpdaterQueue.clear();
  }
}

void
APZUpdater::ClearTree()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<APZUpdater> self = this;
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::ClearTree",
    [=]() {
      self->mApz->ClearTree();

      // Once ClearTree is called on the APZCTreeManager, we are in a shutdown
      // phase. After this point it's ok if WebRender cannot get a hold of the
      // updater via the window id, and it's a good point to remove the mapping
      // and avoid leaving a dangling pointer to this object.
      StaticMutexAutoLock lock(sWindowIdLock);
      if (self->mWindowId) {
        sWindowIdMap.erase(wr::AsUint64(*(self->mWindowId)));
      }
    }
  ));
}

void
APZUpdater::UpdateFocusState(LayersId aRootLayerTreeId,
                             LayersId aOriginatingLayersId,
                             const FocusTarget& aFocusTarget)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RunOnUpdaterThread(NewRunnableMethod<LayersId, LayersId, FocusTarget>(
      "APZUpdater::UpdateFocusState",
      mApz,
      &APZCTreeManager::UpdateFocusState,
      aRootLayerTreeId,
      aOriginatingLayersId,
      aFocusTarget));
}

void
APZUpdater::UpdateHitTestingTree(LayersId aRootLayerTreeId,
                                 Layer* aRoot,
                                 bool aIsFirstPaint,
                                 LayersId aOriginatingLayersId,
                                 uint32_t aPaintSequenceNumber)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnUpdaterThread();
  mApz->UpdateHitTestingTree(aRootLayerTreeId, aRoot, aIsFirstPaint,
      aOriginatingLayersId, aPaintSequenceNumber);
}

void
APZUpdater::UpdateScrollDataAndTreeState(LayersId aRootLayerTreeId,
                                         LayersId aOriginatingLayersId,
                                         WebRenderScrollData&& aScrollData)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<APZUpdater> self = this;
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::UpdateHitTestingTree",
    [=,aScrollData=Move(aScrollData)]() {
      self->mApz->UpdateFocusState(aRootLayerTreeId,
          aOriginatingLayersId, aScrollData.GetFocusTarget());

      self->mScrollData[aOriginatingLayersId] = aScrollData;
      auto root = self->mScrollData.find(aRootLayerTreeId);
      if (root == self->mScrollData.end()) {
        return;
      }
      self->mApz->UpdateHitTestingTree(aRootLayerTreeId,
          WebRenderScrollDataWrapper(*self, &(root->second)),
          aScrollData.IsFirstPaint(), aOriginatingLayersId,
          aScrollData.GetPaintSequenceNumber());
    }
  ));
}

void
APZUpdater::NotifyLayerTreeAdopted(LayersId aLayersId,
                                   const RefPtr<APZUpdater>& aOldUpdater)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RunOnUpdaterThread(NewRunnableMethod<LayersId, RefPtr<APZCTreeManager>>(
      "APZUpdater::NotifyLayerTreeAdopted",
      mApz,
      &APZCTreeManager::NotifyLayerTreeAdopted,
      aLayersId,
      aOldUpdater ? aOldUpdater->mApz : nullptr));
}

void
APZUpdater::NotifyLayerTreeRemoved(LayersId aLayersId)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<APZUpdater> self = this;
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::NotifyLayerTreeRemoved",
    [=]() {
      self->mScrollData.erase(aLayersId);
      self->mApz->NotifyLayerTreeRemoved(aLayersId);
    }
  ));
}

bool
APZUpdater::GetAPZTestData(LayersId aLayersId,
                           APZTestData* aOutData)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  RefPtr<APZCTreeManager> apz = mApz;
  bool ret = false;
  SynchronousTask waiter("APZUpdater::GetAPZTestData");
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::GetAPZTestData",
    [&]() {
      AutoCompleteTask notifier(&waiter);
      ret = apz->GetAPZTestData(aLayersId, aOutData);
    }
  ));

  // Wait until the task posted above has run and populated aOutData and ret
  waiter.Wait();

  return ret;
}

void
APZUpdater::SetTestAsyncScrollOffset(LayersId aLayersId,
                                     const FrameMetrics::ViewID& aScrollId,
                                     const CSSPoint& aOffset)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<APZCTreeManager> apz = mApz;
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::SetTestAsyncScrollOffset",
    [=]() {
      RefPtr<AsyncPanZoomController> apzc = apz->GetTargetAPZC(aLayersId, aScrollId);
      if (apzc) {
        apzc->SetTestAsyncScrollOffset(aOffset);
      } else {
        NS_WARNING("Unable to find APZC in SetTestAsyncScrollOffset");
      }
    }
  ));
}

void
APZUpdater::SetTestAsyncZoom(LayersId aLayersId,
                             const FrameMetrics::ViewID& aScrollId,
                             const LayerToParentLayerScale& aZoom)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RefPtr<APZCTreeManager> apz = mApz;
  RunOnUpdaterThread(NS_NewRunnableFunction(
    "APZUpdater::SetTestAsyncZoom",
    [=]() {
      RefPtr<AsyncPanZoomController> apzc = apz->GetTargetAPZC(aLayersId, aScrollId);
      if (apzc) {
        apzc->SetTestAsyncZoom(aZoom);
      } else {
        NS_WARNING("Unable to find APZC in SetTestAsyncZoom");
      }
    }
  ));
}

const WebRenderScrollData*
APZUpdater::GetScrollData(LayersId aLayersId) const
{
  AssertOnUpdaterThread();
  auto it = mScrollData.find(aLayersId);
  return (it == mScrollData.end() ? nullptr : &(it->second));
}

void
APZUpdater::AssertOnUpdaterThread() const
{
  if (APZThreadUtils::GetThreadAssertionsEnabled()) {
    MOZ_ASSERT(IsUpdaterThread());
  }
}

void
APZUpdater::RunOnUpdaterThread(already_AddRefed<Runnable> aTask)
{
  RefPtr<Runnable> task = aTask;

  if (IsUpdaterThread()) {
    task->Run();
    return;
  }

  if (UsingWebRenderUpdaterThread()) {
    // If the updater thread is a WebRender thread, and we're not on it
    // right now, save the task in the queue. We will run tasks from the queue
    // during the callback from the updater thread, which we trigger by the
    // call to WakeSceneBuilder.

    { // scope lock
      MutexAutoLock lock(mQueueLock);
      mUpdaterQueue.push_back(task);
    }
    RefPtr<wr::WebRenderAPI> api = mApz->GetWebRenderAPI();
    if (api) {
      api->WakeSceneBuilder();
    } else {
      // Not sure if this can happen, but it might be possible. If it does,
      // the task is in the queue, but if we didn't get a WebRenderAPI it
      // might never run, or it might run later if we manage to get a
      // WebRenderAPI later. For now let's just emit a warning, this can
      // probably be upgraded to an assert later.
      NS_WARNING("Possibly dropping task posted to updater thread");
    }
    return;
  }

  if (MessageLoop* loop = CompositorThreadHolder::Loop()) {
    loop->PostTask(task.forget());
  } else {
    // Could happen during startup
    NS_WARNING("Dropping task posted to updater thread");
  }
}

bool
APZUpdater::IsUpdaterThread() const
{
  if (UsingWebRenderUpdaterThread()) {
    return PlatformThread::CurrentId() == *mUpdaterThreadId;
  }
  return CompositorThreadHolder::IsInCompositorThread();
}

void
APZUpdater::RunOnControllerThread(already_AddRefed<Runnable> aTask)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  RunOnUpdaterThread(NewRunnableFunction(
      "APZUpdater::RunOnControllerThread",
      &APZThreadUtils::RunOnControllerThread,
      Move(aTask)));
}

bool
APZUpdater::UsingWebRenderUpdaterThread() const
{
  if (!gfxPrefs::WebRenderAsyncSceneBuild()) {
    return false;
  }
  // If mUpdaterThreadId is not set at the point that this is called, then
  // that means that either (a) WebRender is not enabled for the compositor
  // to which this APZUpdater is attached or (b) we are attempting to do
  // something updater-related before WebRender is up and running. In case
  // (a) falling back to the compositor thread is correct, and in case (b)
  // we should stop doing the updater-related thing so early. We catch this
  // case by setting the mUpdaterThreadQueried flag and asserting on WR
  // initialization.
#ifdef DEBUG
  mUpdaterThreadQueried = true;
#endif
  return mUpdaterThreadId.isSome();
}

/*static*/ already_AddRefed<APZUpdater>
APZUpdater::GetUpdater(const wr::WrWindowId& aWindowId)
{
  RefPtr<APZUpdater> updater;
  StaticMutexAutoLock lock(sWindowIdLock);
  auto it = sWindowIdMap.find(wr::AsUint64(aWindowId));
  if (it != sWindowIdMap.end()) {
    updater = it->second;
  }
  return updater.forget();
}

} // namespace layers
} // namespace mozilla

// Rust callback implementations

void
apz_register_updater(mozilla::wr::WrWindowId aWindowId)
{
  mozilla::layers::APZUpdater::SetUpdaterThread(aWindowId);
}

void
apz_pre_scene_swap(mozilla::wr::WrWindowId aWindowId)
{
}

void
apz_post_scene_swap(mozilla::wr::WrWindowId aWindowId,
                    mozilla::wr::WrPipelineInfo* aInfo)
{
}

void
apz_run_updater(mozilla::wr::WrWindowId aWindowId)
{
  // This should never get called unless async scene building is enabled.
  MOZ_ASSERT(gfxPrefs::WebRenderAsyncSceneBuild());
  mozilla::layers::APZUpdater::ProcessPendingTasks(aWindowId);
}

void
apz_deregister_updater(mozilla::wr::WrWindowId aWindowId)
{
  // Run anything that's still left. Note that this function gets called even
  // if async scene building is off, but in that case we don't want to do
  // anything (because the updater thread will be the compositor thread, and
  // this will be called on the scene builder thread).
  if (gfxPrefs::WebRenderAsyncSceneBuild()) {
    mozilla::layers::APZUpdater::ProcessPendingTasks(aWindowId);
  }
}
