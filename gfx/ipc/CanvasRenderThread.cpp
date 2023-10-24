/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderThread.h"

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/CanvasManagerParent.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsThread.h"
#include "prsystem.h"
#include "transport/runnable_utils.h"

bool NS_IsInCanvasThreadOrWorker() {
  return mozilla::gfx::CanvasRenderThread::IsInCanvasRenderOrWorkerThread();
}

namespace mozilla::gfx {

static StaticRefPtr<CanvasRenderThread> sCanvasRenderThread;
static mozilla::BackgroundHangMonitor* sBackgroundHangMonitor;
#ifdef DEBUG
static bool sCanvasRenderThreadEverStarted = false;
#endif

CanvasRenderThread::CanvasRenderThread(nsCOMPtr<nsIThread>&& aThread,
                                       nsCOMPtr<nsIThreadPool>&& aWorkers,
                                       bool aCreatedThread)
    : mThread(std::move(aThread)),
      mWorkers(std::move(aWorkers)),
      mCreatedThread(aCreatedThread) {}

CanvasRenderThread::~CanvasRenderThread() = default;

// static
void CanvasRenderThread::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sCanvasRenderThread);

#ifdef DEBUG
  // Check to ensure nobody will try to ever start us more than once during
  // the process' lifetime (in particular after Stop).
  MOZ_ASSERT(!sCanvasRenderThreadEverStarted);
  sCanvasRenderThreadEverStarted = true;
#endif

  int32_t threadPref =
      StaticPrefs::gfx_canvas_remote_worker_threads_AtStartup();
  uint32_t threadLimit;
  if (threadPref < 0) {
    // Given that the canvas workers are receiving instructions from
    // content processes, it probably doesn't make sense to have more than
    // half the number of processors doing canvas drawing. We set the
    // lower limit to 2, so that even on single processor systems, if
    // there is more than one window with canvas drawing, the OS can
    // manage the load between them.
    threadLimit = std::max(2, PR_GetNumberOfProcessors() / 2);
  } else {
    threadLimit = uint32_t(threadPref);
  }

  // We don't spawn any workers if the user set the limit to 0. Instead we will
  // use the CanvasRenderThread virtual thread.
  nsCOMPtr<nsIThreadPool> workers;
  if (threadLimit > 0) {
    workers = SharedThreadPool::Get("CanvasWorkers"_ns, threadLimit);
    if (NS_WARN_IF(!workers)) {
      return;
    }
  }

  nsCOMPtr<nsIThread> thread;
  if (!gfxVars::SupportsThreadsafeGL()) {
    thread = wr::RenderThread::GetRenderThread();
    MOZ_ASSERT(thread);
  } else if (!gfxVars::UseCanvasRenderThread()) {
    thread = layers::CompositorThread();
    MOZ_ASSERT(thread);
  }

  if (thread) {
    sCanvasRenderThread = new CanvasRenderThread(
        std::move(thread), std::move(workers), /* aCreatedThread */ false);
    return;
  }

  // This is 4M, which is higher than the default 256K.
  // Increased with bug 1753349 to accommodate the `chromium/5359` branch of
  // ANGLE, which has large peak stack usage for some pathological shader
  // compilations.
  //
  // Previously increased to 512K to accommodate Mesa in bug 1753340.
  //
  // Previously increased to 320K to avoid a stack overflow in the
  // Intel Vulkan driver initialization in bug 1716120.
  //
  // Note: we only override it if it's limited already.
  const uint32_t stackSize =
      nsIThreadManager::DEFAULT_STACK_SIZE ? 4096 << 10 : 0;

  nsresult rv = NS_NewNamedThread(
      "CanvasRenderer", getter_AddRefs(thread),
      NS_NewRunnableFunction(
          "CanvasRender::BackgroundHangSetup",
          []() {
            sBackgroundHangMonitor = new mozilla::BackgroundHangMonitor(
                "CanvasRendererBHM",
                /* Timeout values are powers-of-two to enable us get better
                   data. 128ms is chosen for transient hangs because 8Hz should
                   be the minimally acceptable goal for Compositor
                   responsiveness (normal goal is 60Hz). */
                128,
                /* 2048ms is chosen for permanent hangs because it's longer than
                 * most Compositor hangs seen in the wild, but is short enough
                 * to not miss getting native hang stacks. */
                2048);
            nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
            nsThread* nsthread = static_cast<nsThread*>(thread.get());
            nsthread->SetUseHangMonitor(true);
            nsthread->SetPriority(nsISupportsPriority::PRIORITY_HIGH);
          }),
      {.stackSize = stackSize});

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  sCanvasRenderThread = new CanvasRenderThread(
      std::move(thread), std::move(workers), /* aCreatedThread */ true);
}

// static
void CanvasRenderThread::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  // It is possible we never initialized this thread in the parent process,
  // because we used the GPU process instead.
  if (!sCanvasRenderThread) {
    MOZ_ASSERT(XRE_IsParentProcess());
    return;
  }

  CanvasManagerParent::Shutdown();

  // Null out sCanvasRenderThread before we enter synchronous Shutdown,
  // from here on we are to be considered shut down for our consumers.
  nsCOMPtr<nsIThreadPool> oldWorkers = sCanvasRenderThread->mWorkers;
  nsCOMPtr<nsIThread> oldThread;
  if (sCanvasRenderThread->mCreatedThread) {
    oldThread = sCanvasRenderThread->GetCanvasRenderThread();
    MOZ_ASSERT(oldThread);
  }
  sCanvasRenderThread = nullptr;

  if (oldWorkers) {
    oldWorkers->Shutdown();
  }

  // We do a synchronous shutdown here while spinning the MT event loop, but
  // only if we created a dedicated CanvasRender thread.
  if (oldThread) {
    oldThread->Shutdown();
  }
}

// static
bool CanvasRenderThread::IsInCanvasRenderThread() {
  return sCanvasRenderThread &&
         sCanvasRenderThread->mThread == NS_GetCurrentThread();
}

/* static */ bool CanvasRenderThread::IsInCanvasWorkerThread() {
  // It is possible there are no worker threads, and the worker is the same as
  // the CanvasRenderThread itself.
  return sCanvasRenderThread &&
         ((sCanvasRenderThread->mWorkers &&
           sCanvasRenderThread->mWorkers->IsOnCurrentThread()) ||
          (!sCanvasRenderThread->mWorkers &&
           sCanvasRenderThread->mThread == NS_GetCurrentThread()));
}

/* static */ bool CanvasRenderThread::IsInCanvasRenderOrWorkerThread() {
  // It is possible there are no worker threads, and the worker is the same as
  // the CanvasRenderThread itself.
  return sCanvasRenderThread &&
         (sCanvasRenderThread->mThread == NS_GetCurrentThread() ||
          (sCanvasRenderThread->mWorkers &&
           sCanvasRenderThread->mWorkers->IsOnCurrentThread()));
}

// static
already_AddRefed<nsIThread> CanvasRenderThread::GetCanvasRenderThread() {
  nsCOMPtr<nsIThread> thread;
  if (sCanvasRenderThread) {
    thread = sCanvasRenderThread->mThread;
  }
  return thread.forget();
}

/* static */ already_AddRefed<TaskQueue>
CanvasRenderThread::CreateWorkerTaskQueue() {
  if (!sCanvasRenderThread || !sCanvasRenderThread->mWorkers) {
    return nullptr;
  }

  return TaskQueue::Create(do_AddRef(sCanvasRenderThread->mWorkers),
                           "CanvasWorker")
      .forget();
}

/* static */ void CanvasRenderThread::Dispatch(
    already_AddRefed<nsIRunnable> aRunnable) {
  if (!sCanvasRenderThread) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "Dispatching after CanvasRenderThread shutdown!");
    return;
  }
  sCanvasRenderThread->mThread->Dispatch(std::move(aRunnable));
}

}  // namespace mozilla::gfx
