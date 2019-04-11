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
  explicit SynchronousTask(const char* name)
      : mMonitor(name), mAutoEnter(mMonitor), mDone(false) {}

  void Wait() {
    while (!mDone) {
      mMonitor.Wait();
    }
  }

 private:
  void Complete() {
    mDone = true;
    mMonitor.NotifyAll();
  }

 private:
  ReentrantMonitor mMonitor;
  ReentrantMonitorAutoEnter mAutoEnter;
  bool mDone;
};

class MOZ_STACK_CLASS AutoCompleteTask final {
 public:
  explicit AutoCompleteTask(SynchronousTask* aTask)
      : mTask(aTask), mAutoEnter(aTask->mMonitor) {}
  ~AutoCompleteTask() { mTask->Complete(); }

 private:
  SynchronousTask* mTask;
  ReentrantMonitorAutoEnter mAutoEnter;
};

}  // namespace layers
}  // namespace mozilla

#endif
