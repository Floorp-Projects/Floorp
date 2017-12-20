/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include "base/message_loop.h"
#include "base/object_watcher.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class ChildReaper : public mozilla::Runnable,
                    public base::ObjectWatcher::Delegate,
                    public MessageLoop::DestructionObserver {
 public:
  explicit ChildReaper(base::ProcessHandle process, bool force)
    : mozilla::Runnable("ChildReaper"), process_(process), force_(force) {
    watcher_.StartWatching(process_, this);
  }

  virtual ~ChildReaper() {
    if (process_) {
      KillProcess();
      DCHECK(!process_) << "Make sure to close the handle.";
    }
  }

  // MessageLoop::DestructionObserver -----------------------------------------

  virtual void WillDestroyCurrentMessageLoop()
  {
    MOZ_ASSERT(!force_);
    if (process_) {
      WaitForSingleObject(process_, INFINITE);
      base::CloseProcessHandle(process_);
      process_ = 0;

      MessageLoop::current()->RemoveDestructionObserver(this);
      delete this;
    }
  }

  // Task ---------------------------------------------------------------------

  NS_IMETHOD Run() override {
    MOZ_ASSERT(force_);
    if (process_) {
      KillProcess();
    }
    return NS_OK;
  }

  // MessageLoop::Watcher -----------------------------------------------------

  virtual void OnObjectSignaled(HANDLE object) {
    // When we're called from KillProcess, the ObjectWatcher may still be
    // watching.  the process handle, so make sure it has stopped.
    watcher_.StopWatching();

    base::CloseProcessHandle(process_);
    process_ = 0;

    if (!force_) {
      MessageLoop::current()->RemoveDestructionObserver(this);
      delete this;
    }
  }

 private:
  void KillProcess() {
    MOZ_ASSERT(force_);

    // OK, time to get frisky.  We don't actually care when the process
    // terminates.  We just care that it eventually terminates, and that's what
    // TerminateProcess should do for us. Don't check for the result code since
    // it fails quite often. This should be investigated eventually.
    TerminateProcess(process_, base::PROCESS_END_PROCESS_WAS_HUNG);

    // Now, just cleanup as if the process exited normally.
    OnObjectSignaled(process_);
  }

  // The process that we are watching.
  base::ProcessHandle process_;

  base::ObjectWatcher watcher_;

  bool force_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildReaper);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process, bool force) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    base::CloseProcessHandle(process);
    return;
  }

  MessageLoopForIO* loop = MessageLoopForIO::current();
  if (force) {
    RefPtr<mozilla::Runnable> task = new ChildReaper(process, force);
    loop->PostDelayedTask(task.forget(), kWaitInterval);
  } else {
    loop->AddDestructionObserver(new ChildReaper(process, force));
  }
}
