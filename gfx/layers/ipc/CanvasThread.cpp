/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasThread.h"

#include "base/task.h"
#include "mozilla/SharedThreadPool.h"
#include "prsystem.h"

bool NS_IsInCanvasThreadOrWorker() {
  return mozilla::layers::CanvasThreadHolder::IsInCanvasThreadOrWorker();
}

namespace mozilla {
namespace layers {

StaticDataMutex<StaticRefPtr<CanvasThreadHolder>>
    CanvasThreadHolder::sCanvasThreadHolder("sCanvasThreadHolder");

CanvasThreadHolder::CanvasThreadHolder(
    UniquePtr<base::Thread> aCanvasThread,
    already_AddRefed<nsIThreadPool> aCanvasWorkers)
    : mCanvasThread(std::move(aCanvasThread)),
      mCanvasWorkers(aCanvasWorkers),
      mCompositorThreadKeepAlive(CompositorThreadHolder::GetSingleton()) {
  MOZ_ASSERT(NS_IsInCompositorThread());
  MOZ_ASSERT(mCanvasThread);
  MOZ_ASSERT(mCanvasWorkers);
}

CanvasThreadHolder::~CanvasThreadHolder() {
  // Note we can't just use NS_IsInCompositorThread() here because
  // sCompositorThreadHolder might have already gone.
  MOZ_ASSERT(mCompositorThreadKeepAlive->GetCompositorThread()->thread_id() ==
             PlatformThread::CurrentId());
}

/* static */
already_AddRefed<CanvasThreadHolder> CanvasThreadHolder::EnsureCanvasThread() {
  MOZ_ASSERT(NS_IsInCompositorThread());

  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  if (!lockedCanvasThreadHolder.ref()) {
    UniquePtr<base::Thread> canvasThread = MakeUnique<base::Thread>("Canvas");
    if (!canvasThread->Start()) {
      return nullptr;
    };

    // Given that the canvas workers are receiving instructions from
    // content processes, it probably doesn't make sense to have more than
    // half the number of processors doing canvas drawing. We set the
    // lower limit to 2, so that even on single processor systems, if
    // there is more than one window with canvas drawing, the OS can
    // manage the load between them.
    uint32_t threadLimit = std::max(2, PR_GetNumberOfProcessors() / 2);
    RefPtr<nsIThreadPool> canvasWorkers =
        SharedThreadPool::Get(NS_LITERAL_CSTRING("CanvasWorkers"), threadLimit);
    if (!canvasWorkers) {
      return nullptr;
    }

    lockedCanvasThreadHolder.ref() =
        new CanvasThreadHolder(std::move(canvasThread), canvasWorkers.forget());
  }

  return do_AddRef(lockedCanvasThreadHolder.ref());
}

/* static */
void CanvasThreadHolder::StaticRelease(
    already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder) {
  RefPtr<CanvasThreadHolder> threadHolder = aCanvasThreadHolder;
  // Note we can't just use NS_IsInCompositorThread() here because
  // sCompositorThreadHolder might have already gone.
  MOZ_ASSERT(threadHolder->mCompositorThreadKeepAlive->GetCompositorThread()
                 ->thread_id() == PlatformThread::CurrentId());
  threadHolder = nullptr;

  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  if (lockedCanvasThreadHolder.ref()->mRefCnt == 1) {
    lockedCanvasThreadHolder.ref() = nullptr;
  }
}

/* static */
void CanvasThreadHolder::ReleaseOnCompositorThread(
    already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder) {
  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  base::Thread* compositorThread =
      lockedCanvasThreadHolder.ref()
          ->mCompositorThreadKeepAlive->GetCompositorThread();
  compositorThread->message_loop()->PostTask(NewRunnableFunction(
      "CanvasThreadHolder::StaticRelease", CanvasThreadHolder::StaticRelease,
      std::move(aCanvasThreadHolder)));
}

/* static */
bool CanvasThreadHolder::IsInCanvasThread() {
  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  return lockedCanvasThreadHolder.ref() &&
         lockedCanvasThreadHolder.ref()->mCanvasThread->thread_id() ==
             PlatformThread::CurrentId();
}

/* static */
bool CanvasThreadHolder::IsInCanvasWorker() {
  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  return lockedCanvasThreadHolder.ref() &&
         lockedCanvasThreadHolder.ref()->mCanvasWorkers->IsOnCurrentThread();
}

/* static */
bool CanvasThreadHolder::IsInCanvasThreadOrWorker() {
  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  return lockedCanvasThreadHolder.ref() &&
         (lockedCanvasThreadHolder.ref()->mCanvasWorkers->IsOnCurrentThread() ||
          lockedCanvasThreadHolder.ref()->mCanvasThread->thread_id() ==
              PlatformThread::CurrentId());
}

/* static */
void CanvasThreadHolder::MaybeDispatchToCanvasThread(
    already_AddRefed<nsIRunnable> aRunnable) {
  auto lockedCanvasThreadHolder = sCanvasThreadHolder.Lock();
  if (!lockedCanvasThreadHolder.ref()) {
    // There is no canvas thread just release the runnable.
    RefPtr<nsIRunnable> runnable = aRunnable;
    return;
  }

  lockedCanvasThreadHolder.ref()->mCanvasThread->message_loop()->PostTask(
      std::move(aRunnable));
}

void CanvasThreadHolder::DispatchToCanvasThread(
    already_AddRefed<nsIRunnable> aRunnable) {
  mCanvasThread->message_loop()->PostTask(std::move(aRunnable));
}

already_AddRefed<TaskQueue> CanvasThreadHolder::CreateWorkerTaskQueue() {
  return MakeAndAddRef<TaskQueue>(do_AddRef(mCanvasWorkers));
}

}  // namespace layers
}  // namespace mozilla
