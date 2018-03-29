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

namespace mozilla {
namespace layers {

APZUpdater::APZUpdater(const RefPtr<APZCTreeManager>& aApz)
  : mApz(aApz)
{
  MOZ_ASSERT(aApz);
  mApz->SetUpdater(this);
}

APZUpdater::~APZUpdater()
{
  mApz->SetUpdater(nullptr);
}

bool
APZUpdater::HasTreeManager(const RefPtr<APZCTreeManager>& aApz)
{
  return aApz.get() == mApz.get();
}

void
APZUpdater::ClearTree()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  RunOnUpdaterThread(NewRunnableMethod(
      "APZUpdater::ClearTree",
      mApz,
      &APZCTreeManager::ClearTree));
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

  MessageLoop* loop = CompositorThreadHolder::Loop();
  if (!loop) {
    // Could happen during startup
    NS_WARNING("Dropping task posted to updater thread");
    return;
  }

  if (IsUpdaterThread()) {
    task->Run();
  } else {
    loop->PostTask(task.forget());
  }
}

bool
APZUpdater::IsUpdaterThread()
{
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

} // namespace layers
} // namespace mozilla
