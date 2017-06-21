/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/process_util.h"

#include "chrome/common/process_watcher.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
// XXX/cjones: fairly arbitrary, chosen to match process_watcher_win.cc
static const int kMaxWaitMs = 2000;

namespace {

bool
IsProcessDead(pid_t process)
{
  bool exited = false;
  // don't care if the process crashed, just if it exited
  base::DidProcessCrash(&exited, process);
  return exited;
}


class ChildReaper : public base::MessagePumpLibevent::SignalEvent,
                    public base::MessagePumpLibevent::SignalWatcher
{
public:
  explicit ChildReaper(pid_t process) : process_(process)
  {
  } 

  virtual ~ChildReaper()
  {
    // subclasses should have cleaned up |process_| already
    DCHECK(!process_);

    // StopCatching() is implicit
  }

  // @override
  virtual void OnSignal(int sig)
  {
    DCHECK(SIGCHLD == sig);
    DCHECK(process_);

    // this may be the SIGCHLD for a process other than |process_|
    if (IsProcessDead(process_)) {
      process_ = 0;
      StopCatching();
    }
  }

protected:
  void WaitForChildExit()
  {
    DCHECK(process_);
    HANDLE_EINTR(waitpid(process_, NULL, 0));
  }

  pid_t process_;

private:
  DISALLOW_EVIL_CONSTRUCTORS(ChildReaper);
};


// Fear the reaper
class ChildGrimReaper : public ChildReaper,
                        public mozilla::Runnable
{
public:
  explicit ChildGrimReaper(pid_t process) : ChildReaper(process)
  {
  } 

  virtual ~ChildGrimReaper()
  {
    if (process_)
      KillProcess();
  }

  NS_IMETHOD Run() override
  {
    // we may have already been signaled by the time this runs
    if (process_)
      KillProcess();

    return NS_OK;
  }

private:
  void KillProcess()
  {
    DCHECK(process_);

    if (IsProcessDead(process_)) {
      process_ = 0;
      return;
    }

    if (0 == kill(process_, SIGKILL)) {
      // XXX this will block for whatever amount of time it takes the
      // XXX OS to tear down the process's resources.  might need to
      // XXX rethink this if it proves expensive
      WaitForChildExit();
    }
    else {
      CHROMIUM_LOG(ERROR) << "Failed to deliver SIGKILL to " << process_ << "!"
                          << "("<< errno << ").";
    }
    process_ = 0;
  }

  DISALLOW_EVIL_CONSTRUCTORS(ChildGrimReaper);
};


class ChildLaxReaper : public ChildReaper,
                       public MessageLoop::DestructionObserver
{
public:
  explicit ChildLaxReaper(pid_t process) : ChildReaper(process)
  {
  } 

  virtual ~ChildLaxReaper()
  {
    // WillDestroyCurrentMessageLoop() should have reaped process_ already
    DCHECK(!process_);
  }

  // @override
  virtual void OnSignal(int sig)
  {
    ChildReaper::OnSignal(sig);

    if (!process_) {
      MessageLoop::current()->RemoveDestructionObserver(this);
      delete this;
    }
  }

  // @override
  virtual void WillDestroyCurrentMessageLoop()
  {
    DCHECK(process_);

    WaitForChildExit();
    process_ = 0;

    // XXX don't think this is necessary, since destruction can only
    // be observed once, but can't hurt
    MessageLoop::current()->RemoveDestructionObserver(this);
    delete this;
  }

private:
  DISALLOW_EVIL_CONSTRUCTORS(ChildLaxReaper);
};

}  // namespace <anon>


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
void
ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process,
                                        bool force)
{
  DCHECK(process != base::GetCurrentProcId());
  DCHECK(process > 0);

  if (IsProcessDead(process))
    return;

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
