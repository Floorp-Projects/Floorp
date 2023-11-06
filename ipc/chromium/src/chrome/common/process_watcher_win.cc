/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include <algorithm>
#include <processthreadsapi.h>
#include <synchapi.h>
#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "prenv.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static constexpr int kWaitInterval = 2000;

// This is somewhat arbitrary, but based on Try run results.  When
// changing this, be aware of toolkit.asyncshutdown.crash_timeout
// (currently 60s), after which the parent process will be killed.
#ifdef MOZ_CODE_COVERAGE
// Child processes seem to take longer to shut down on ccov builds, at
// least in the wdspec tests; ~20s has been observed, and we'll spam
// false positives unless this is increased.
static constexpr DWORD kShutdownWaitMs = 80000;
#elif defined(MOZ_ASAN) || defined(MOZ_TSAN)
// Sanitizers also slow things down in some cases; see bug 1806224.
static constexpr DWORD kShutdownWaitMs = 40000;
#else
static constexpr DWORD kShutdownWaitMs = 20000;
#endif

namespace {

static bool IsProcessDead(base::ProcessHandle process) {
  return WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
}

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

  virtual void WillDestroyCurrentMessageLoop() {
    MOZ_ASSERT(!force_);
    if (process_) {
      // Exception for the fake hang tests in ipc/glue/test/browser
      if (!PR_GetEnv("MOZ_TEST_CHILD_EXIT_HANG")) {
        CrashProcessIfHanging();
      }
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

  void CrashProcessIfHanging() {
    if (IsProcessDead(process_)) {
      return;
    }
    DWORD pid = GetProcessId(process_);
    DCHECK(pid != 0);

    // If child processes seems to be hanging on shutdown, wait for a
    // reasonable time.  The wait is global instead of per-process
    // because the child processes should be shutting down in
    // parallel, and also we're potentially racing global timeouts
    // like nsTerminator.  (The counter doesn't need to be atomic;
    // this is always called on the I/O thread.)
    static DWORD sWaitMs = kShutdownWaitMs;
    if (sWaitMs > 0) {
      CHROMIUM_LOG(WARNING)
          << "Process " << pid
          << " may be hanging at shutdown; will wait for up to " << sWaitMs
          << "ms";
    }
    const auto beforeWait = mozilla::TimeStamp::NowLoRes();
    const DWORD waitStatus = WaitForSingleObject(process_, sWaitMs);

    const double elapsed =
        (mozilla::TimeStamp::NowLoRes() - beforeWait).ToMilliseconds();
    sWaitMs -= static_cast<DWORD>(
        std::clamp(elapsed, 0.0, static_cast<double>(sWaitMs)));

    switch (waitStatus) {
      case WAIT_TIMEOUT:
        // The process is still running.
        break;
      case WAIT_OBJECT_0:
        // The process exited.
        return;
      case WAIT_FAILED:
        CHROMIUM_LOG(ERROR) << "Waiting for process " << pid
                            << " failed; error " << GetLastError();
        DCHECK(false) << "WaitForSingleObject failed";
        // Process status unclear; assume it's gone.
        return;
      default:
        DCHECK(false) << "WaitForSingleObject returned " << waitStatus;
        // Again, not clear what's happening so avoid touching the process
        return;
    }

    // We want TreeHerder to flag this log line as an error, so that
    // this is more obviously a deliberate crash; "fatal error" is one
    // of the strings it looks for.
    CHROMIUM_LOG(ERROR)
        << "Process " << pid
        << " hanging at shutdown; attempting crash report (fatal error)";

    // We're going to use CreateRemoteThread to call DbgBreakPoint in
    // the target process; it's in a "known DLL" so it should be at
    // the same address in all processes.  (Normal libraries, like
    // xul.dll, are usually at the same address but can be relocated
    // in case of conflict.)
    //
    // DbgBreakPoint doesn't take an argument, so we can give it an
    // arbitrary value to make it easier to identify these crash
    // reports.  (reinterpret_cast isn't constexpr, so this is
    // declared as an integer and cast to the required type later.)
    // The primary use case for all of this is in CI, where we'll also
    // have log messages, but if these crashes end up in Socorro in
    // significant numbers then we'll be able to look for this value.
    static constexpr uint64_t kIpcMagic = 0x43504900435049;

    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
      CHROMIUM_LOG(ERROR) << "couldn't find ntdll.dll: error "
                          << GetLastError();
      return;
    }
    const auto dbgBreak = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(ntdll, "DbgBreakPoint"));
    if (!dbgBreak) {
      CHROMIUM_LOG(ERROR) << "couldn't find DbgBreakPoint: error "
                          << GetLastError();
      return;
    }

    const DWORD rights = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                         PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                         PROCESS_VM_READ;
    HANDLE process_priv = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), process_, GetCurrentProcess(),
                         &process_priv, rights, /* inherit */ FALSE,
                         /* options */ 0)) {
      const auto error = GetLastError();
      CHROMIUM_LOG(ERROR) << "OpenProcess: error " << error;
    } else {
      DCHECK(process_priv);
      HANDLE thread =
          CreateRemoteThread(process_priv, /* sec attr */ nullptr,
                             /* stack */ 0, dbgBreak, (LPVOID)kIpcMagic,
                             /* flags */ 0, nullptr);
      if (!thread) {
        const auto error = GetLastError();
        CHROMIUM_LOG(ERROR) << "CreateRemoteThread: error " << error;
      } else {
        CloseHandle(thread);
      }
      CloseHandle(process_priv);
    }
  }

  // The process that we are watching.
  base::ProcessHandle process_;

  base::ObjectWatcher watcher_;

  bool force_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildReaper);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process,
                                             bool force) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (IsProcessDead(process)) {
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
