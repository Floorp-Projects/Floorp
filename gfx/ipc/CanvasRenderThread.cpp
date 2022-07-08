/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasRenderThread.h"

#include "mozilla/BackgroundHangMonitor.h"
#include "transport/runnable_utils.h"

namespace mozilla::gfx {

static StaticRefPtr<CanvasRenderThread> sCanvasRenderThread;
static mozilla::BackgroundHangMonitor* sBackgroundHangMonitor;

CanvasRenderThread::CanvasRenderThread(RefPtr<nsIThread> aThread)
    : mThread(std::move(aThread)) {}

CanvasRenderThread::~CanvasRenderThread() {}

// static
CanvasRenderThread* CanvasRenderThread::Get() { return sCanvasRenderThread; }

// static
void CanvasRenderThread::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sCanvasRenderThread);

  // This is 512K, which is higher than the default 256K.
  // Increased to accommodate Mesa in bug 1753340.
  //
  // Previously increased to 320K to avoid a stack overflow in the
  // Intel Vulkan driver initialization in bug 1716120.
  //
  // Note: we only override it if it's limited already.
  const uint32_t stackSize =
      nsIThreadManager::DEFAULT_STACK_SIZE ? 512 << 10 : 0;

  RefPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread(
      "CanvasRenderer", getter_AddRefs(thread),
      NS_NewRunnableFunction(
          "CanvasRender::BackgroundHanSetup",
          []() {
            sBackgroundHangMonitor = new mozilla::BackgroundHangMonitor(
                "CanvasRenderer",
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
      stackSize);

  if (NS_FAILED(rv)) {
    return;
  }

  sCanvasRenderThread = new CanvasRenderThread(thread);
}

// static
void CanvasRenderThread::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sCanvasRenderThread);

  layers::SynchronousTask task("CanvasRenderThread");
  RefPtr<Runnable> runnable =
      WrapRunnable(RefPtr<CanvasRenderThread>(sCanvasRenderThread.get()),
                   &CanvasRenderThread::ShutDownTask, &task);
  sCanvasRenderThread->PostRunnable(runnable.forget());
  task.Wait();

  sCanvasRenderThread = nullptr;
}

void CanvasRenderThread::ShutDownTask(layers::SynchronousTask* aTask) {
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInCanvasRenderThread());
}

// static
bool CanvasRenderThread::IsInCanvasRenderThread() {
  return sCanvasRenderThread &&
         sCanvasRenderThread->mThread == NS_GetCurrentThread();
}

// static
already_AddRefed<nsIThread> CanvasRenderThread::GetCanvasRenderThread() {
  nsCOMPtr<nsIThread> thread;
  if (sCanvasRenderThread) {
    thread = sCanvasRenderThread->mThread;
  }
  return thread.forget();
}

void CanvasRenderThread::PostRunnable(already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  mThread->Dispatch(runnable.forget());
}

}  // namespace mozilla::gfx
