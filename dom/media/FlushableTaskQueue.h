/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FlushableTaskQueue_h_
#define FlushableTaskQueue_h_

#include "mozilla/TaskQueue.h"

//
// WARNING: THIS CLASS IS DEPRECATED AND GOING AWAY. DO NOT USE IT!
//

namespace mozilla {

class FlushableTaskQueue : public TaskQueue
{
public:
  explicit FlushableTaskQueue(already_AddRefed<SharedThreadPool> aPool) : TaskQueue(Move(aPool)) {}
  nsresult FlushAndDispatch(already_AddRefed<nsIRunnable> aRunnable);
  void Flush();

  bool IsDispatchReliable() override { return false; }

private:

  class MOZ_STACK_CLASS AutoSetFlushing
  {
  public:
    explicit AutoSetFlushing(FlushableTaskQueue* aTaskQueue) : mTaskQueue(aTaskQueue)
    {
      mTaskQueue->mQueueMonitor.AssertCurrentThreadOwns();
      mTaskQueue->mIsFlushing = true;
    }
    ~AutoSetFlushing()
    {
      mTaskQueue->mQueueMonitor.AssertCurrentThreadOwns();
      mTaskQueue->mIsFlushing = false;
    }

  private:
    FlushableTaskQueue* mTaskQueue;
  };

  void FlushLocked();

};

}  // namespace mozilla

#endif // FlushableTaskQueue_h_
