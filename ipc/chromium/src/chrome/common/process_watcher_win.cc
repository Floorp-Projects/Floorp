// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "base/sys_info.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/result_codes.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class TimerExpiredTask : public Task, public base::ObjectWatcher::Delegate {
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

  virtual void Run() {
    if (process_)
      KillProcess();
  }

  // MessageLoop::Watcher -----------------------------------------------------

  virtual void OnObjectSignaled(HANDLE object) {
    // When we're called from KillProcess, the ObjectWatcher may still be
    // watching.  the process handle, so make sure it has stopped.
    watcher_.StopWatching();

    CloseHandle(process_);
    process_ = NULL;
  }

 private:
  void KillProcess() {
    if (base::SysInfo::HasEnvVar(env_vars::kHeadless)) {
     // If running the distributed tests, give the renderer a little time
     // to figure out that the channel is shutdown and unwind.
     if (WaitForSingleObject(process_, kWaitInterval) == WAIT_OBJECT_0) {
       OnObjectSignaled(process_);
       return;
     }
    }

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
    CloseHandle(process);
    return;
  }

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new TimerExpiredTask(process),
                                          kWaitInterval);
}
