/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlushableTaskQueue.h"

namespace mozilla {

void
FlushableTaskQueue::Flush()
{
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  FlushLocked();
  AwaitIdleLocked();
}

nsresult
FlushableTaskQueue::FlushAndDispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  MonitorAutoLock mon(mQueueMonitor);
  AutoSetFlushing autoFlush(this);
  FlushLocked();
  nsCOMPtr<nsIRunnable> r = dont_AddRef(aRunnable.take());
  nsresult rv = DispatchLocked(r.forget(), IgnoreFlushing, AssertDispatchSuccess);
  NS_ENSURE_SUCCESS(rv, rv);
  AwaitIdleLocked();
  return NS_OK;
}

void
FlushableTaskQueue::FlushLocked()
{
  // Make sure there are no tasks for this queue waiting in the caller's tail
  // dispatcher.
  MOZ_ASSERT_IF(AbstractThread::GetCurrent(),
                !AbstractThread::GetCurrent()->TailDispatcher().HasTasksFor(this));

  mQueueMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mIsFlushing);

  // Clear the tasks. If this strikes you as awful, stop using a
  // FlushableTaskQueue.
  while (!mTasks.empty()) {
    mTasks.pop();
  }
}

} // namespace mozilla
