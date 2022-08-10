/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "mozilla/DataMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "chrome/common/process_watcher.h"

#ifdef MOZ_ENABLE_FORKSERVER
#  include "mozilla/StaticPrefs_dom.h"
#endif

#if defined(XP_UNIX) && !defined(XP_MACOSX) && 0
// Linux, {Free,Net,Open}BSD, and Solaris; but not macOS, yet.
#  define HAVE_PIPE2 1
#endif

// The basic idea here is a minimal SIGCHLD handler which writes to a
// pipe and a libevent callback on the I/O thread which fires when the
// other end becomes readable.  When we start waiting for process
// termination we check if it had already terminated, and otherwise
// register it to be checked later when SIGCHLD fires.
//
// Making this more complicated is that we usually want to kill the
// process after a timeout, in case it hangs trying to exit, but not
// if it's already exited by that point (see `DelayedKill`).
// But we also support waiting indefinitely, for debug/CI use cases
// like refcount logging / leak detection / code coverage, and in that
// case we block parent process shutdown until all children exit
// (which is done by blocking the I/O thread late in shutdown, which
// isn't ideal, but the Windows implementation has the same issue).

// Maximum amount of time (in milliseconds) to wait for the process to exit.
// XXX/cjones: fairly arbitrary, chosen to match process_watcher_win.cc
static const int kMaxWaitMs = 2000;

namespace {

// Represents a child process being awaited (which is expected to exit
// soon, or already has).
//
// If `mForce` is null then we will wait indefinitely (and block
// parent shutdown; see above); otherwise it will be killed after a
// timeout (or during parent shutdown, if that happens first).
struct PendingChild {
  pid_t mPid;
  nsCOMPtr<nsITimer> mForce;
};

// `EnsureProcessTerminated` is called when a process is expected to
// be shutting down, so there should be relatively few `PendingChild`
// instances at any given time, meaning that using an array and doing
// O(n) operations should be fine.
static mozilla::StaticDataMutex<mozilla::StaticAutoPtr<nsTArray<PendingChild>>>
    gPendingChildren("ProcessWatcher::gPendingChildren");
static int gSignalPipe[2] = {-1, -1};

enum class BlockingWait { NO, YES };

#ifdef MOZ_ENABLE_FORKSERVER
static bool IsForkServerEnabled() {
  return mozilla::StaticPrefs::dom_ipc_forkserver_enable_AtStartup();
}

// With the current design of the fork server we can't waitpid
// directly.  Instead, we simulate it by polling with `kill(pid, 0)`;
// this is unreliable because pids can be reused, so we could report
// that the process is still running when it's exited.
//
// Because of that possibility, the "blocking" mode has an arbitrary
// limit on the amount of polling it does for the process's
// nonexistence; if that limit is exceeded, an error is returned.
//
// See also bug 1752638 to improve the fork server so we can get
// accurate process information.
static pid_t FakeWaitpid(pid_t pid, int* wstatus, int options) {
  // Try the real waitpid first, in case this was a non-fork-server
  // child process.  If it is an indirect child, it will fail with
  // ECHILD.  (This is a pid reuse race hazard but so is everything in
  // here.)
  int real_rv = HANDLE_EINTR(waitpid(pid, wstatus, options | WNOHANG));
  if (real_rv != -1 || errno != ECHILD) {
    return real_rv;
  }

  static constexpr long kDelayMS = 500;
  static constexpr int kAttempts = 10;

  if (options & ~WNOHANG) {
    errno = EINVAL;
    return -1;
  }

  // We can't get the actual exit status, so pretend everything is fine.
  static constexpr int kZero = 0;  // Unfortunately, macros.
  static_assert(WIFEXITED(kZero));
  static_assert(WEXITSTATUS(kZero) == 0);
  if (wstatus) {
    *wstatus = 0;
  }

  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    int rv = kill(pid, 0);
    if (rv == 0) {
      // Process is still running (or its pid was reassigned; oops).
      if (options & WNOHANG) {
        return 0;
      }
    } else {
      if (errno == ESRCH) {
        // Process presumably exited.
        return pid;
      }
      // Some other error (permissions, if it's the wrong process?).
      return -1;
    }

    // Wait and try again.
    struct timespec delay = {(kDelayMS / 1000),
                             (kDelayMS % 1000) * 1000 * 1000};
    HANDLE_EINTR(nanosleep(&delay, &delay));
  }

  errno = ETIME;  // "Timer expired"; close enough.
  return -1;
}
#endif

// A convenient wrapper for `waitpid`; returns true if the child
// process has exited.
static bool WaitForProcess(pid_t pid, BlockingWait aBlock) {
  int wstatus;
  int flags = aBlock == BlockingWait::NO ? WNOHANG : 0;

  pid_t (*waitpidImpl)(pid_t, int*, int) = waitpid;
#ifdef MOZ_ENABLE_FORKSERVER
  if (IsForkServerEnabled()) {
    waitpidImpl = FakeWaitpid;
  }
#endif

  pid_t rv = HANDLE_EINTR(waitpidImpl(pid, &wstatus, flags));
  if (rv < 0) {
    // Shouldn't happen, but maybe the pid was incorrect (not a child
    // of this process), or maybe some other code already waited for
    // it.  This can be caused by issues like bug 227246, but also
    // because of the fork server.
    CHROMIUM_LOG(ERROR) << "waitpid failed (pid " << pid
                        << "): " << strerror(errno);
    return true;
  }

  if (rv == 0) {
    MOZ_ASSERT(aBlock == BlockingWait::NO);
    return false;
  }

  // At this point we have the pid and exit status, and all IPC child
  // processes should go through this point.  Possible future work:
  // allow registering observers.
  if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
    CHROMIUM_LOG(WARNING) << "process " << pid << " exited with status "
                          << WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    CHROMIUM_LOG(WARNING) << "process " << pid << " exited on signal "
                          << WTERMSIG(wstatus);
  }
  return true;
}

// Creates a timer to kill the process after a delay, for the
// `force=true` case.  The timer is bound to the I/O thread, which
// means it needs to be cancelled there (and thus that child exit
// notifications need to be handled on the I/O thread).
already_AddRefed<nsITimer> DelayedKill(pid_t aPid) {
  nsCOMPtr<nsITimer> timer;

  nsresult rv = NS_NewTimerWithCallback(
      getter_AddRefs(timer),
      [aPid](nsITimer*) {
        if (kill(aPid, SIGKILL) != 0) {
          CHROMIUM_LOG(ERROR) << "failed to send SIGKILL to process " << aPid;
        }
      },
      kMaxWaitMs, nsITimer::TYPE_ONE_SHOT, "ProcessWatcher::DelayedKill",
      XRE_GetIOMessageLoop()->SerialEventTarget());

  // This should happen only during shutdown, in which case we're
  // about to kill the process anyway during I/O thread destruction.
  if (NS_FAILED(rv)) {
    CHROMIUM_LOG(WARNING) << "failed to start kill timer for process " << aPid
                          << "; killing immediately";
    kill(aPid, SIGKILL);
    return nullptr;
  }

  return timer.forget();
}

// Most of the logic is here.  Reponds to SIGCHLD via the self-pipe,
// and handles shutdown behavior in `WillDestroyCurrentMessageLoop`.
// There is one instance of this class; it's created the first time
// it's used and destroys itself during IPC shutdown.
class ProcessCleaner final : public MessageLoopForIO::Watcher,
                             public MessageLoop::DestructionObserver {
 public:
  // Safety: this must be called on the I/O thread.
  void Register() {
    MessageLoopForIO* loop = MessageLoopForIO::current();
    loop->AddDestructionObserver(this);
    loop->WatchFileDescriptor(gSignalPipe[0], /* persistent= */ true,
                              MessageLoopForIO::WATCH_READ, &mWatcher, this);
  }

  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(fd == gSignalPipe[0]);
    ssize_t rv;
    // Drain the pipe and prune dead processes.
    do {
      char msg;
      rv = HANDLE_EINTR(read(gSignalPipe[0], &msg, 1));
      CHECK(rv != 0);
      if (rv < 0) {
        DCHECK(errno == EAGAIN || errno == EWOULDBLOCK);
      } else {
        DCHECK(msg == 0);
      }
    } while (rv > 0);
    PruneDeadProcesses();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    CHROMIUM_LOG(FATAL) << "unreachable";
  }

  void WillDestroyCurrentMessageLoop() override {
    mWatcher.StopWatchingFileDescriptor();
    auto lock = gPendingChildren.Lock();
    auto& children = lock.ref();
    if (children) {
      for (const auto& child : *children) {
        // If the child still has force-termination pending, do that now.
        if (child.mForce) {
          // This is too late for timers to run, so no need to Cancel().
          if (kill(child.mPid, SIGKILL) != 0) {
            CHROMIUM_LOG(ERROR)
                << "failed to send SIGKILL to process " << child.mPid;
            continue;
          }
        } else {
          CHROMIUM_LOG(WARNING)
              << "Waiting in WillDestroyCurrentMessageLoop for pid "
              << child.mPid;
        }
        // If the process was just killed, it should exit immediately;
        // otherwise, block until it exits on its own.
        WaitForProcess(child.mPid, BlockingWait::YES);
      }
      children = nullptr;
    }
    delete this;
  }

 private:
  MessageLoopForIO::FileDescriptorWatcher mWatcher;

  static void PruneDeadProcesses() {
    auto lock = gPendingChildren.Lock();
    auto& children = lock.ref();
    if (!children || children->IsEmpty()) {
      return;
    }
    nsTArray<PendingChild> live;
    for (const auto& child : *children) {
      if (WaitForProcess(child.mPid, BlockingWait::NO)) {
        if (child.mForce) {
          child.mForce->Cancel();
        }
      } else {
        live.AppendElement(child);
      }
    }
    *children = std::move(live);
  }
};

static void HandleSigChld(int signum) {
  DCHECK(signum == SIGCHLD);
  char msg = 0;
  HANDLE_EINTR(write(gSignalPipe[1], &msg, 1));
  // Can't log here if this fails (at least not normally; SafeSPrintf
  // from security/sandbox/chromium could be used).
  //
  // (Note that this could fail with EAGAIN if the pipe buffer becomes
  // full; this is extremely unlikely, and it doesn't matter because
  // the reader will be woken up regardless and doesn't care about the
  // number of signals delivered.)
}

static void ProcessWatcherInit() {
  int rv;

#ifdef HAVE_PIPE2
  rv = pipe2(gSignalPipe, O_NONBLOCK | O_CLOEXEC);
  CHECK(rv == 0)
  << "pipe2() failed";
#else
  rv = pipe(gSignalPipe);
  CHECK(rv == 0)
  << "pipe() failed";
  for (int fd : gSignalPipe) {
    rv = fcntl(fd, F_SETFL, O_NONBLOCK);
    CHECK(rv == 0)
    << "O_NONBLOCK failed";
    rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
    CHECK(rv == 0)
    << "FD_CLOEXEC failed";
  }
#endif  // HAVE_PIPE2

  // Currently there are no other SIGCHLD handlers; this is debug
  // asserted.  If the situation changes, it should be relatively
  // simple to delegate; note that this ProcessWatcher doesn't
  // interfere with child processes it hasn't been asked to handle.
  auto oldHandler = signal(SIGCHLD, HandleSigChld);
  CHECK(oldHandler != SIG_ERR);
  DCHECK(oldHandler == SIG_DFL);

  // Start the ProcessCleaner; registering it with the I/O thread must
  // happen on the I/O thread itself.  It's okay for that to happen
  // asynchronously: the callback is level-triggered, so if the signal
  // handler already wrote to the pipe at that point then it will be
  // detected, and the signal itself is async so additional delay
  // doesn't change the semantics.
  XRE_GetIOMessageLoop()->PostTask(
      NS_NewRunnableFunction("ProcessCleaner::Register", [] {
        ProcessCleaner* pc = new ProcessCleaner();
        pc->Register();
      }));
}

}  // namespace

/**
 * Do everything possible to ensure that |process| has been reaped
 * before this process exits.
 *
 * |force| decides how strict to be with the child's shutdown.
 *
 *                | child exit timeout | upon parent shutdown:
 *                +--------------------+----------------------------------
 *   force=true   | 2 seconds          | kill(child, SIGKILL)
 *   force=false  | infinite           | waitpid(child)
 *
 * If a child process doesn't shut down properly, and |force=false|
 * used, then the parent will wait on the child forever.  So,
 * |force=false| is expected to be used when an external entity can be
 * responsible for terminating hung processes, e.g. automated test
 * harnesses.
 */
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process,
                                             bool force) {
  DCHECK(process != base::GetCurrentProcId());
  DCHECK(process > 0);

  static std::once_flag sInited;
  std::call_once(sInited, ProcessWatcherInit);

  auto lock = gPendingChildren.Lock();
  auto& children = lock.ref();

  // Check if the process already exited.  This needs to happen under
  // the `gPendingChildren` lock to prevent this sequence:
  //
  // A1. this non-blocking wait fails
  // B1. the process exits
  // B2. SIGCHLD is handled
  // B3. the ProcessCleaner wakes up and drains the signal pipe
  // A2. the process is added to `gPendingChildren`
  //
  // Holding the lock prevents B3 from occurring between A1 and A2.
  if (WaitForProcess(process, BlockingWait::NO)) {
    return;
  }

  if (!children) {
    children = new nsTArray<PendingChild>();
  }
  // Check for duplicate pids.  This is safe even in corner cases with
  // pid reuse: the pid can't be reused by the OS until the zombie
  // process has been waited, and both the `waitpid` and the following
  // removal of the `PendingChild` object occur while continually
  // holding the lock, which is also held here.
  for (const auto& child : *children) {
    if (child.mPid == process) {
#ifdef MOZ_ENABLE_FORKSERVER
      if (IsForkServerEnabled()) {
        // In theory we can end up here if an earlier child process
        // with the same pid was launched via the fork server, and
        // exited, and had its pid reused for a new process before we
        // noticed that it exited.

        CHROMIUM_LOG(WARNING) << "EnsureProcessTerminated: duplicate process"
                                 " ID "
                              << process
                              << "; assuming this is because of the fork"
                                 " server.";

        // So, we want to end up with a PendingChild for the new
        // process; we can just use the old one.  Ideally we'd fix the
        // `mForce` value, but that would involve needing to cancel a
        // timer when we aren't necessarily on the right thread, and
        // in practice the `force` parameter depends only on the build
        // type.  (Again, see bug 1752638 for the correct solution.)
        return;
      }
#endif
      MOZ_ASSERT(false,
                 "EnsureProcessTerminated must be called at most once for a "
                 "given process");
      return;
    }
  }

  PendingChild child{};
  child.mPid = process;
  if (force) {
    child.mForce = DelayedKill(process);
  }
  children->AppendElement(std::move(child));
}
