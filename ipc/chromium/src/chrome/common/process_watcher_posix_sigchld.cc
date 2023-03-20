/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "prenv.h"

#include "chrome/common/process_watcher.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
// XXX/cjones: fairly arbitrary, chosen to match process_watcher_win.cc
static constexpr int kMaxWaitMs = 2000;

// This is also somewhat arbitrary, but loosely based on Try results.
// See also toolkit.asyncshutdown.crash_timeout (currently 60s) after
// which the parent process will be killed.
#ifdef MOZ_CODE_COVERAGE
// Code coverage instrumentation can be slow (especially when writing
// out data, which has to take a lock on the data files).
static constexpr int kShutdownWaitMs = 80000;
#elif defined(MOZ_ASAN) || defined(MOZ_TSAN)
// Sanitizers slow things down in some cases; see bug 1806224.
static constexpr int kShutdownWaitMs = 40000;
#else
static constexpr int kShutdownWaitMs = 8000;
#endif

namespace {

class ChildReaper : public base::MessagePumpLibevent::SignalEvent,
                    public base::MessagePumpLibevent::SignalWatcher {
 public:
  explicit ChildReaper(pid_t process) : process_(process) {}

  virtual ~ChildReaper() {
    // subclasses should have cleaned up |process_| already
    DCHECK(!process_);

    // StopCatching() is implicit
  }

  virtual void OnSignal(int sig) override {
    DCHECK(SIGCHLD == sig);
    DCHECK(process_);

    // this may be the SIGCHLD for a process other than |process_|
    if (base::IsProcessDead(process_)) {
      process_ = 0;
      StopCatching();
    }
  }

 protected:
  void WaitForChildExit() {
    CHECK(process_);
    while (!base::IsProcessDead(process_, true)) {
      // It doesn't matter if this is interrupted; we just need to
      // wait for some amount of time while the other process status
      // event is (hopefully) handled.  This is used only during an
      // error case at shutdown, so a 1s wait won't be too noticeable.
      sleep(1);
    }
  }

  pid_t process_;

 private:
  ChildReaper(const ChildReaper&) = delete;

  const ChildReaper& operator=(const ChildReaper&) = delete;
};

// Fear the reaper
class ChildGrimReaper : public ChildReaper, public mozilla::Runnable {
 public:
  explicit ChildGrimReaper(pid_t process)
      : ChildReaper(process), mozilla::Runnable("ChildGrimReaper") {}

  virtual ~ChildGrimReaper() {
    if (process_) KillProcess();
  }

  NS_IMETHOD Run() override {
    // we may have already been signaled by the time this runs
    if (process_) KillProcess();

    return NS_OK;
  }

 private:
  void KillProcess() {
    DCHECK(process_);

    if (base::IsProcessDead(process_)) {
      process_ = 0;
      return;
    }

    if (0 == kill(process_, SIGKILL)) {
      // XXX this will block for whatever amount of time it takes the
      // XXX OS to tear down the process's resources.  might need to
      // XXX rethink this if it proves expensive
      WaitForChildExit();
    } else {
      CHROMIUM_LOG(ERROR) << "Failed to deliver SIGKILL to " << process_ << "!"
                          << "(" << errno << ").";
    }
    process_ = 0;
  }

  ChildGrimReaper(const ChildGrimReaper&) = delete;

  const ChildGrimReaper& operator=(const ChildGrimReaper&) = delete;
};

class ChildLaxReaper : public ChildReaper,
                       public MessageLoop::DestructionObserver {
 public:
  explicit ChildLaxReaper(pid_t process) : ChildReaper(process) {}

  virtual ~ChildLaxReaper() {
    // WillDestroyCurrentMessageLoop() should have reaped process_ already
    DCHECK(!process_);
  }

  virtual void OnSignal(int sig) override {
    ChildReaper::OnSignal(sig);

    if (!process_) {
      MessageLoop::current()->RemoveDestructionObserver(this);
      delete this;
    }
  }

  virtual void WillDestroyCurrentMessageLoop() override {
    DCHECK(process_);
    if (!process_) {
      return;
    }

    // Exception for the fake hang tests in ipc/glue/test/browser
    if (!PR_GetEnv("MOZ_TEST_CHILD_EXIT_HANG")) {
      CrashProcessIfHanging();
    }
    if (process_) {
      WaitForChildExit();
      process_ = 0;
    }

    // XXX don't think this is necessary, since destruction can only
    // be observed once, but can't hurt
    MessageLoop::current()->RemoveDestructionObserver(this);
    delete this;
  }

 private:
  ChildLaxReaper(const ChildLaxReaper&) = delete;

  void CrashProcessIfHanging() {
    if (base::IsProcessDead(process_)) {
      process_ = 0;
      return;
    }

    // If child processes seems to be hanging on shutdown, wait for a
    // reasonable time.  The wait is global instead of per-process
    // because the child processes should be shutting down in
    // parallel, and also we're potentially racing global timeouts
    // like nsTerminator.  (The counter doesn't need to be atomic;
    // this is always called on the I/O thread.)
    static int sWaitMs = kShutdownWaitMs;
    if (sWaitMs > 0) {
      CHROMIUM_LOG(WARNING)
          << "Process " << process_
          << " may be hanging at shutdown; will wait for up to " << sWaitMs
          << "ms";
    }
    // There isn't a way to do a time-limited wait that's both
    // portable and doesn't require messing with signals.  Instead, we
    // sleep in short increments and poll the process status.
    while (sWaitMs > 0) {
      static constexpr int kWaitTickMs = 200;
      struct timespec ts = {kWaitTickMs / 1000, (kWaitTickMs % 1000) * 1000000};
      HANDLE_EINTR(nanosleep(&ts, &ts));
      sWaitMs -= kWaitTickMs;

      if (base::IsProcessDead(process_)) {
        process_ = 0;
        return;
      }
    }

    // We want TreeHerder to flag this log line as an error, so that
    // this is more obviously a deliberate crash; "fatal error" is one
    // of the strings it looks for.
    CHROMIUM_LOG(ERROR)
        << "Process " << process_
        << " hanging at shutdown; attempting crash report (fatal error).";

    kill(process_, SIGABRT);
  }

  const ChildLaxReaper& operator=(const ChildLaxReaper&) = delete;
};

}  // namespace

/**
 * Do everything possible to ensure that |process| has been reaped
 * before this process exits.
 *
 * |grim| decides how strict to be with the child's shutdown.
 *
 *                | child exit timeout | upon parent shutdown:
 *                +--------------------+----------------------------------
 *   force=true   | 2 seconds          | kill(child, SIGKILL)
 *   force=false  | infinite           | waitpid(child)
 *
 * If a child process doesn't shut down properly, and |grim=false|
 * used, then the parent will wait on the child forever.  So,
 * |force=false| is expected to be used when an external entity can be
 * responsible for terminating hung processes, e.g. automated test
 * harnesses.
 */
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process,
                                             bool force) {
  DCHECK(process != base::GetCurrentProcId());
  DCHECK(process > 0);

  if (base::IsProcessDead(process)) return;

  MessageLoopForIO* loop = MessageLoopForIO::current();
  if (force) {
    RefPtr<ChildGrimReaper> reaper = new ChildGrimReaper(process);

    loop->CatchSignal(SIGCHLD, reaper, reaper);
    // |loop| takes ownership of |reaper|
    loop->PostDelayedTask(reaper.forget(), kMaxWaitMs);
  } else {
    ChildLaxReaper* reaper = new ChildLaxReaper(process);

    loop->CatchSignal(SIGCHLD, reaper, reaper);
    // |reaper| destroys itself after destruction notification
    loop->AddDestructionObserver(reaper);
  }
}
