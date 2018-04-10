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
#include "mozilla/layers/WebRenderScrollData.h"
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
APZUpdater::UpdateHitTestingTree(LayersId aRootLayerTreeId,
                                 const WebRenderScrollData& aScrollData,
                                 bool aIsFirstPaint,
                                 LayersId aOriginatingLayersId,
                                 uint32_t aPaintSequenceNumber)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  // use the local variable to resolve the function overload.
  auto func = static_cast<void (APZCTreeManager::*)(LayersId,
                                                    const WebRenderScrollData&,
                                                    bool,
                                                    LayersId,
                                                    uint32_t)>
      (&APZCTreeManager::UpdateHitTestingTree);
  RunOnUpdaterThread(NewRunnableMethod<LayersId,
                                       WebRenderScrollData,
                                       bool,
                                       LayersId,
                                       uint32_t>(
      "APZUpdater::UpdateHitTestingTree",
      mApz,
      func,
      aRootLayerTreeId,
      aScrollData,
      aIsFirstPaint,
      aOriginatingLayersId,
      aPaintSequenceNumber));
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
  RunOnUpdaterThread(NewRunnableMethod<LayersId>(
      "APZUpdater::NotifyLayerTreeRemoved",
      mApz,
      &APZCTreeManager::NotifyLayerTreeRemoved,
      aLayersId));
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

void
APZUpdater::AssertOnUpdaterThread()
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
    // TODO
    NS_WARNING("Dropping task posted to updater thread");
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
APZUpdater::IsUpdaterThread()
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
}

void
apz_deregister_updater(mozilla::wr::WrWindowId aWindowId)
{
}
