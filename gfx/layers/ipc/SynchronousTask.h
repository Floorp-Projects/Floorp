/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SYNCHRONOUSTASK_H
#define MOZILLA_GFX_SYNCHRONOUSTASK_H

#include "mozilla/ReentrantMonitor.h"  // for ReentrantMonitor, etc

namespace mozilla {
namespace layers {

// Helper that creates a monitor and a "done" flag, then enters the monitor.
class MOZ_STACK_CLASS SynchronousTask {
  friend class AutoCompleteTask;

 public:
  explicit SynchronousTask(const char* name) : mMonitor(name), mDone(false) {}

  nsresult Wait(PRIntervalTime aInterval = PR_INTERVAL_NO_TIMEOUT) {
    ReentrantMonitorAutoEnter lock(mMonitor);

    // For indefinite timeouts, wait in a while loop to handle spurious
    // wakeups.
    while (aInterval == PR_INTERVAL_NO_TIMEOUT && !mDone) {
      mMonitor.Wait();
    }

    // For finite timeouts, we only check once for completion, and otherwise
    // rely on the ReentrantMonitor to manage the interval. If the monitor
    // returns too early, we'll never know, but we can check if the mDone
    // flag was set to true, indicating that the task finished successfully.
    if (!mDone) {
      // We ignore the return value from ReentrantMonitor::Wait, because it's
      // always NS_OK, even in the case of timeout.
      mMonitor.Wait(aInterval);

      if (!mDone) {
        return NS_ERROR_ABORT;
      }
    }

    return NS_OK;
  }

 private:
  void Complete() {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mDone = true;
    mMonitor.NotifyAll();
  }

 private:
  ReentrantMonitor mMonitor MOZ_UNANNOTATED;
  bool mDone;
};

class MOZ_STACK_CLASS AutoCompleteTask final {
 public:
  explicit AutoCompleteTask(SynchronousTask* aTask) : mTask(aTask) {}
  ~AutoCompleteTask() { mTask->Complete(); }

 private:
  SynchronousTask* mTask;
};

}  // namespace layers
}  // namespace mozilla

#endif
