/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskQueueWrapper.h"

#include "mozilla/media/MediaUtils.h"
#include "MediaEventSource.h"

namespace mozilla {
namespace {
class MainWorker {
 public:
  const UniquePtr<TaskQueueWrapper> mTaskQueue;
  MediaEventListener mShutdownListener;
  const UniquePtr<media::ShutdownBlockingTicket> mTicket;
  explicit MainWorker(UniquePtr<TaskQueueWrapper> aTaskQueue)
      : mTaskQueue(std::move(aTaskQueue)),
        mTicket(MakeUnique<media::ShutdownBlockingTicket>(
            u"MainWorker::mTicket"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
            __LINE__)) {}
};

UniquePtr<MainWorker> gMainWorker;
}  // namespace

TaskQueueWrapper* TaskQueueWrapper::GetMainWorker() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gMainWorker) {
    gMainWorker = MakeUnique<MainWorker>(
        MakeUnique<TaskQueueWrapper>(MakeRefPtr<TaskQueue>(
            do_AddRef(GetMainThreadEventTarget()), "MainWorker")));
    gMainWorker->mShutdownListener =
        gMainWorker->mTicket->ShutdownEvent().Connect(
            GetMainThreadEventTarget(), [] {
              gMainWorker->mShutdownListener.Disconnect();
              // Destroying the main thread worker on main thread will deadlock
              // if there are pending tasks, since it's sync. Do it async.
              gMainWorker->mTaskQueue->mTaskQueue->BeginShutdown()->Then(
                  GetMainThreadSerialEventTarget(), __func__,
                  [worker = std::move(gMainWorker)] { Unused << worker; });
            });
  }
  return gMainWorker->mTaskQueue.get();
}
}  // namespace mozilla
