/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "base/sys_info.h"
#include "chrome/common/result_codes.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class TimerExpiredTask : public mozilla::Runnable,
                         public base::ObjectWatcher::Delegate {
 public:
  explicit TimerExpiredTask(base::ProcessHandle process) : process_(process) {
    watcher_.StartWatching(process_, this);
  }

  virtual ~TimerExpiredTask() {
    if (process_) {
      KillProcess();
      DCHECK(!process_) << "Make sure to close the handle.";
    }
  }

  // Task ---------------------------------------------------------------------

  NS_IMETHOD Run() override {
    if (process_)
      KillProcess();
    return NS_OK;
  }

  // MessageLoop::Watcher -----------------------------------------------------

  virtual void OnObjectSignaled(HANDLE object) {
    // When we're called from KillProcess, the ObjectWatcher may still be
    // watching.  the process handle, so make sure it has stopped.
    watcher_.StopWatching();

    base::CloseProcessHandle(process_);
    process_ = NULL;
  }

 private:
  void KillProcess() {
    // OK, time to get frisky.  We don't actually care when the process
    // terminates.  We just care that it eventually terminates, and that's what
    // TerminateProcess should do for us. Don't check for the result code since
    // it fails quite often. This should be investigated eventually.
    TerminateProcess(process_, ResultCodes::HUNG);

    // Now, just cleanup as if the process exited normally.
    OnObjectSignaled(process_);
  }

  // The process that we are watching.
  base::ProcessHandle process_;

  base::ObjectWatcher watcher_;

  DISALLOW_EVIL_CONSTRUCTORS(TimerExpiredTask);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process
                                             , bool force
) {
  DCHECK(process != GetCurrentProcess());

  if (!force) {
    WaitForSingleObject(process, INFINITE);
    base::CloseProcessHandle(process);
    return;
  }

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    base::CloseProcessHandle(process);
    return;
  }

  RefPtr<mozilla::Runnable> task = new TimerExpiredTask(process);

  MessageLoop::current()->PostDelayedTask(task.forget(),
                                          kWaitInterval);
}
