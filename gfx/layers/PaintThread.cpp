/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintThread.h"

#include <algorithm>

#include "base/task.h"
#include "gfxPlatform.h"
#include "GeckoProfiler.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SyncRunnable.h"
#ifdef XP_MACOSX
#include "nsCocoaFeatures.h"
#endif
#include "nsIPropertyBag2.h"
#include "nsIThreadManager.h"
#include "nsServiceManagerUtils.h"
#include "prsystem.h"

// Uncomment the following line to dispatch sync runnables when
// painting so that rasterization happens synchronously from
// the perspective of the main thread
// #define OMTP_FORCE_SYNC

namespace mozilla {
namespace layers {

using namespace gfx;

void PaintTask::DropTextureClients() { mClients.Clear(); }

StaticAutoPtr<PaintThread> PaintThread::sSingleton;
StaticRefPtr<nsIThread> PaintThread::sThread;
PlatformThreadId PaintThread::sThreadId;

PaintThread::PaintThread() {}

void PaintThread::Release() {}

void PaintThread::AddRef() {}

/* static */
int32_t PaintThread::CalculatePaintWorkerCount() {
  int32_t cpuCores = PR_GetNumberOfProcessors();
  int32_t workerCount = StaticPrefs::layers_omtp_paint_workers_AtStartup();

  // If not manually specified, default to (cpuCores * 3) / 4, and clamp
  // between 1 and 4. If a user wants more, they can manually specify it
  if (workerCount < 1) {
    workerCount = std::min(std::max((cpuCores * 3) / 4, 1), 4);
  }

  return workerCount;
}

/* static */
void PaintThread::Start() {
  PaintThread::sSingleton = new PaintThread();

  if (!PaintThread::sSingleton->Init()) {
    gfxCriticalNote << "Unable to start paint thread";
    PaintThread::sSingleton = nullptr;
  }
}

static uint32_t GetPaintThreadStackSize() {
#ifndef XP_MACOSX
  return nsIThreadManager::DEFAULT_STACK_SIZE;
#else
  // Workaround bug 1578075 by increasing the stack size of paint threads
  if (nsCocoaFeatures::OnCatalinaOrLater()) {
    static const uint32_t kCatalinaPaintThreadStackSize = 512 * 1024;
    static_assert(kCatalinaPaintThreadStackSize >= nsIThreadManager::DEFAULT_STACK_SIZE,
                  "update default stack size of paint "
                  "workers");
    return kCatalinaPaintThreadStackSize;
  }
  return nsIThreadManager::DEFAULT_STACK_SIZE;
#endif
}

bool PaintThread::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("PaintThread", getter_AddRefs(thread),
                                  nullptr, GetPaintThreadStackSize());
  if (NS_FAILED(rv)) {
    return false;
  }
  sThread = thread;

  // Only create paint workers for tiling if we are using tiling or could
  // expect to dynamically switch to tiling in the future
  if (gfxPlatform::GetPlatform()->UsesTiling()) {
    InitPaintWorkers();
  }

  nsCOMPtr<nsIRunnable> paintInitTask = NewRunnableMethod(
      "PaintThread::InitOnPaintThread", this, &PaintThread::InitOnPaintThread);
  SyncRunnable::DispatchToThread(sThread, paintInitTask);
  return true;
}

void PaintThread::InitOnPaintThread() {
  MOZ_ASSERT(!NS_IsMainThread());
  sThreadId = PlatformThread::CurrentId();
}

void PaintThread::InitPaintWorkers() {
  MOZ_ASSERT(NS_IsMainThread());
  int32_t count = PaintThread::CalculatePaintWorkerCount();
  if (count != 1) {
    mPaintWorkers =
        SharedThreadPool::Get(NS_LITERAL_CSTRING("PaintWorker"), count);
    mPaintWorkers->SetThreadStackSize(GetPaintThreadStackSize());
  }
}

void DestroyPaintThread(UniquePtr<PaintThread>&& pt) {
  MOZ_ASSERT(PaintThread::IsOnPaintThread());
  pt->ShutdownOnPaintThread();
}

/* static */
void PaintThread::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<PaintThread> pt(sSingleton.forget());
  if (!pt) {
    return;
  }

  sThread->Dispatch(NewRunnableFunction("DestroyPaintThreadRunnable",
                                        DestroyPaintThread, std::move(pt)));
  sThread->Shutdown();
  sThread = nullptr;
}

void PaintThread::ShutdownOnPaintThread() { MOZ_ASSERT(IsOnPaintThread()); }

/* static */
PaintThread* PaintThread::Get() { return PaintThread::sSingleton.get(); }

/* static */
bool PaintThread::IsOnPaintThread() {
  return sThreadId == PlatformThread::CurrentId();
}

bool PaintThread::IsOnPaintWorkerThread() {
  return (mPaintWorkers && mPaintWorkers->IsOnCurrentThread()) ||
         (sThreadId == PlatformThread::CurrentId());
}

void PaintThread::Dispatch(RefPtr<Runnable>& aRunnable) {
#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(aRunnable.forget());
#else
  SyncRunnable::DispatchToThread(sThread, aRunnable);
#endif
}

void PaintThread::UpdateRenderMode() {
  if (!!mPaintWorkers != gfxPlatform::GetPlatform()->UsesTiling()) {
    if (mPaintWorkers) {
      mPaintWorkers = nullptr;
    } else {
      InitPaintWorkers();
    }
  }
}

void PaintThread::QueuePaintTask(UniquePtr<PaintTask>&& aTask) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);

  if (StaticPrefs::layers_omtp_dump_capture() && aTask->mCapture) {
    aTask->mCapture->Dump();
  }

  MOZ_RELEASE_ASSERT(aTask->mCapture->hasOneRef());

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());
  cbc->NotifyBeginAsyncPaint(aTask.get());

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("PaintThread::AsyncPaintTask",
                             [self, cbc, task = std::move(aTask)]() -> void {
                               self->AsyncPaintTask(cbc, task.get());
                             });

  nsIEventTarget* paintThread =
      mPaintWorkers ? static_cast<nsIEventTarget*>(mPaintWorkers.get())
                    : static_cast<nsIEventTarget*>(sThread.get());

#ifndef OMTP_FORCE_SYNC
  paintThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(paintThread, task);
#endif
}

void PaintThread::AsyncPaintTask(CompositorBridgeChild* aBridge,
                                 PaintTask* aTask) {
  AUTO_PROFILER_LABEL("PaintThread::AsyncPaintTask", GRAPHICS);

  MOZ_ASSERT(IsOnPaintWorkerThread());
  MOZ_ASSERT(aTask);

  gfx::DrawTargetCapture* capture = aTask->mCapture;
  gfx::DrawTarget* target = aTask->mTarget;

  if (target->IsValid()) {
    // Do not replay to invalid targets. This can happen on device resets and
    // the browser will ensure the graphics stack is reinitialized on the main
    // thread.
    target->DrawCapturedDT(capture, Matrix());
    target->Flush();
  }

  if (StaticPrefs::layers_omtp_release_capture_on_main_thread()) {
    // This should ensure the capture drawtarget, which may hold on to
    // UnscaledFont objects, gets destroyed on the main thread (See bug
    // 1404742). This assumes (unflushed) target DrawTargets do not themselves
    // hold on to UnscaledFonts.
    NS_ReleaseOnMainThreadSystemGroup("PaintTask::DrawTargetCapture",
                                      aTask->mCapture.forget());
  }

  if (aBridge->NotifyFinishedAsyncWorkerPaint(aTask)) {
    AsyncEndLayerTransaction(aBridge);
  }
}

void PaintThread::QueueEndLayerTransaction(SyncObjectClient* aSyncObject) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());

  if (cbc->NotifyBeginAsyncEndLayerTransaction(aSyncObject)) {
    RefPtr<PaintThread> self = this;
    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "PaintThread::AsyncEndLayerTransaction",
        [self, cbc]() -> void { self->AsyncEndLayerTransaction(cbc); });

#ifndef OMTP_FORCE_SYNC
    sThread->Dispatch(task.forget());
#else
    SyncRunnable::DispatchToThread(sThread, task);
#endif
  }
}

void PaintThread::AsyncEndLayerTransaction(CompositorBridgeChild* aBridge) {
  MOZ_ASSERT(IsOnPaintWorkerThread());

  aBridge->NotifyFinishedAsyncEndLayerTransaction();
}

}  // namespace layers
}  // namespace mozilla
