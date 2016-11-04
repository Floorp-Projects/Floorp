/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SYNCHRONOUSTASK_H
#define MOZILLA_GFX_SYNCHRONOUSTASK_H

#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitor, etc

namespace mozilla {
namespace layers {

// Helper that creates a monitor and a "done" flag, then enters the monitor.
// This can go away when we switch ImageBridge to an XPCOM thread.
class MOZ_STACK_CLASS SynchronousTask
{
  friend class AutoCompleteTask;

public:
  explicit SynchronousTask(const char* name)
   : mMonitor(name),
     mAutoEnter(mMonitor),
     mDone(false)
  {}

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

class MOZ_STACK_CLASS AutoCompleteTask
{
public:
  explicit AutoCompleteTask(SynchronousTask* aTask)
   : mTask(aTask),
     mAutoEnter(aTask->mMonitor)
  {
  }
  ~AutoCompleteTask() {
    mTask->Complete();
  }

private:
  SynchronousTask* mTask;
  ReentrantMonitorAutoEnter mAutoEnter;
};

}
}

#endif
