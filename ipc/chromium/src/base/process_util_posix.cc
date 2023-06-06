/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>
#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "base/dir_reader_posix.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
// For PR_DuplicateEnvironment:
#include "prenv.h"
#include "prmem.h"

#ifdef MOZ_ENABLE_FORKSERVER
#  include "mozilla/ipc/ForkServiceChild.h"
#endif

// We could configure-test for `waitid`, but it's been in POSIX for a
// long time and OpenBSD seems to be the only Unix we target that
// doesn't have it.  Note that `waitid` is used to resolve a conflict
// with the crash reporter, which isn't available on OpenBSD.
#ifndef __OpenBSD__
#  define HAVE_WAITID
#endif

namespace base {

ProcessId GetCurrentProcId() { return getpid(); }

ProcessHandle GetCurrentProcessHandle() { return GetCurrentProcId(); }

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // On Posix platforms, process handles are the same as PIDs, so we
  // don't need to do anything.
  *handle = pid;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // On POSIX permissions are checked for each operation on process,
  // not when opening a "handle".
  return OpenProcessHandle(pid, handle);
}

void CloseProcessHandle(ProcessHandle process) {
  // See OpenProcessHandle, nothing to do.
  return;
}

ProcessId GetProcId(ProcessHandle process) { return process; }

// Attempts to kill the process identified by the given process
// entry structure.  Ignores specified exit_code; posix can't force that.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process_id, int exit_code) {
  // It's too easy to accidentally kill pid 0 (meaning the caller's
  // process group) or pid -1 (all other processes killable by this
  // user), and neither they nor other negative numbers (process
  // groups) are legitimately used by this function's callers, so
  // reject them all.
  if (process_id <= 0) {
    CHROMIUM_LOG(WARNING) << "base::KillProcess refusing to kill pid "
                          << process_id;
    return false;
  }

  bool result = kill(process_id, SIGTERM) == 0;

  if (!result && (errno == ESRCH)) {
    result = true;
  }

  if (!result) DLOG(ERROR) << "Unable to terminate process.";

  return result;
}

#ifdef ANDROID
typedef unsigned long int rlim_t;
#endif

// A class to handle auto-closing of DIR*'s.
class ScopedDIRClose {
 public:
  inline void operator()(DIR* x) const {
    if (x) {
      closedir(x);
    }
  }
};
typedef mozilla::UniquePtr<DIR, ScopedDIRClose> ScopedDIR;

void CloseSuperfluousFds(void* aCtx, bool (*aShouldPreserve)(void*, int)) {
  // DANGER: no calls to malloc (or locks, etc.) are allowed from now on:
  // https://crbug.com/36678
  // Also, beware of STL iterators: https://crbug.com/331459
#if defined(ANDROID)
  static const rlim_t kSystemDefaultMaxFds = 1024;
  static const char kFDDir[] = "/proc/self/fd";
#elif defined(XP_LINUX) || defined(XP_SOLARIS)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char kFDDir[] = "/proc/self/fd";
#elif defined(XP_DARWIN)
  static const rlim_t kSystemDefaultMaxFds = 256;
  static const char kFDDir[] = "/dev/fd";
#elif defined(__DragonFly__) || defined(XP_FREEBSD) || defined(XP_NETBSD) || \
    defined(XP_OPENBSD)
  // the getrlimit below should never fail, so whatever ..
  static const rlim_t kSystemDefaultMaxFds = 1024;
  // at least /dev/fd will exist
  static const char kFDDir[] = "/dev/fd";
#endif

  // Get the maximum number of FDs possible.
  struct rlimit nofile;
  rlim_t max_fds;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // getrlimit failed. Take a best guess.
    max_fds = kSystemDefaultMaxFds;
    DLOG(ERROR) << "getrlimit(RLIMIT_NOFILE) failed: " << errno;
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX) max_fds = INT_MAX;

  DirReaderPosix fd_dir(kFDDir);

  if (!fd_dir.IsValid()) {
    // Fallback case: Try every possible fd.
    for (rlim_t i = 0; i < max_fds; ++i) {
      const int fd = static_cast<int>(i);
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO ||
          aShouldPreserve(aCtx, fd)) {
        continue;
      }

      // Since we're just trying to close anything we can find,
      // ignore any error return values of close().
      close(fd);
    }
    return;
  }

  const int dir_fd = fd_dir.fd();

  for (; fd_dir.Next();) {
    // Skip . and .. entries.
    if (fd_dir.name()[0] == '.') continue;

    char* endptr;
    errno = 0;
    const long int fd = strtol(fd_dir.name(), &endptr, 10);
    if (fd_dir.name()[0] == 0 || *endptr || fd < 0 || errno) continue;
    if (fd == dir_fd) continue;
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO ||
        aShouldPreserve(aCtx, fd)) {
      continue;
    }

    // When running under Valgrind, Valgrind opens several FDs for its
    // own use and will complain if we try to close them.  All of
    // these FDs are >= |max_fds|, so we can check against that here
    // before closing.  See https://bugs.kde.org/show_bug.cgi?id=191758
    if (fd < static_cast<int>(max_fds)) {
      int ret = IGNORE_EINTR(close(fd));
      if (ret != 0) {
        DLOG(ERROR) << "Problem closing fd";
      }
    }
  }
}

bool IsProcessDead(ProcessHandle handle, bool blocking) {
  auto handleForkServer = [handle]() -> mozilla::Maybe<bool> {
#ifdef MOZ_ENABLE_FORKSERVER
    if (errno == ECHILD && mozilla::ipc::ForkServiceChild::Get()) {
      // We only know if a process exists, but not if it has crashed.
      //
      // Since content processes are not direct children of the chrome
      // process any more, it is impossible to use |waitpid()| to wait for
      // them.
      const int r = kill(handle, 0);
      // FIXME: for unexpected errors we should probably log a warning
      // and return true, so that the caller doesn't loop / hang /
      // try to kill the process.  (Bug 1658072 will rewrite this code.)
      return mozilla::Some(r < 0 && errno == ESRCH);
    }
#else
    mozilla::Unused << handle;
#endif
    return mozilla::Nothing();
  };

#ifdef HAVE_WAITID

  // We use `WNOWAIT` to read the process status without
  // side-effecting it, in case it's something unexpected like a
  // ptrace-stop for the crash reporter.  If is an exit, the call is
  // reissued (see the end of this function) without that flag in
  // order to collect the process.
  siginfo_t si{};
  const int wflags = WEXITED | WNOWAIT | (blocking ? 0 : WNOHANG);
  int result = HANDLE_EINTR(waitid(P_PID, handle, &si, wflags));
  if (result == -1) {
    if (auto forkServerReturn = handleForkServer()) {
      return *forkServerReturn;
    }

    // This shouldn't happen, but sometimes it does.  The error is
    // probably ECHILD and the reason is probably that a pid was
    // waited on again after a previous wait reclaimed its zombie.
    // (It could also occur if the process isn't a direct child, but
    // don't do that.)  This is bad, because it risks interfering with
    // an unrelated child process if the pid is reused.
    //
    // So, lacking reliable information, we indicate that the process
    // is dead, in the hope that the caller will give up and stop
    // calling us.  See also bug 943174 and bug 933680.
    CHROMIUM_LOG(ERROR) << "waitid failed pid:" << handle << " errno:" << errno;
    return true;
  }

  if (si.si_pid == 0) {
    // the child hasn't exited yet.
    return false;
  }

  DCHECK(si.si_pid == handle);
  switch (si.si_code) {
    case CLD_STOPPED:
    case CLD_CONTINUED:
      DCHECK(false) << "waitid returned an event type that it shouldn't have";
      [[fallthrough]];
    case CLD_TRAPPED:
      CHROMIUM_LOG(WARNING) << "ignoring non-exit event for process " << handle;
      return false;

    case CLD_KILLED:
    case CLD_DUMPED:
      CHROMIUM_LOG(WARNING)
          << "process " << handle << " exited on signal " << si.si_status;
      break;

    case CLD_EXITED:
      if (si.si_status != 0) {
        CHROMIUM_LOG(WARNING)
            << "process " << handle << " exited with status " << si.si_status;
      }
      break;

    default:
      CHROMIUM_LOG(ERROR) << "unexpected waitid si_code value: " << si.si_code;
      DCHECK(false);
      // This shouldn't happen, but assume that the process exited to
      // avoid the caller possibly ending up in a loop.
  }

  // Now consume the status / collect the dead process
  const int old_si_code = si.si_code;
  si.si_pid = 0;
  // In theory it shouldn't matter either way if we use `WNOHANG` at
  // this point, but just in case, avoid unexpected blocking.
  result = HANDLE_EINTR(waitid(P_PID, handle, &si, WEXITED | WNOHANG));
  DCHECK(result == 0);
  DCHECK(si.si_pid == handle);
  DCHECK(si.si_code == old_si_code);
  return true;

#else  // no waitid

  int status;
  const int result = waitpid(handle, &status, blocking ? 0 : WNOHANG);
  if (result == -1) {
    if (auto forkServerReturn = handleForkServer()) {
      return *forkServerReturn;
    }

    CHROMIUM_LOG(ERROR) << "waitpid failed pid:" << handle
                        << " errno:" << errno;
    return true;
  }
  if (result == 0) {
    return false;
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    CHROMIUM_LOG(WARNING) << "process " << handle << " exited with status "
                          << WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    CHROMIUM_LOG(WARNING) << "process " << handle << " exited on signal "
                          << WTERMSIG(status);
  }
  return true;

#endif  // waitid
}

void FreeEnvVarsArray::operator()(char** array) {
  for (char** varPtr = array; *varPtr != nullptr; ++varPtr) {
    free(*varPtr);
  }
  delete[] array;
}

EnvironmentArray BuildEnvironmentArray(const environment_map& env_vars_to_set) {
  base::environment_map combined_env_vars = env_vars_to_set;
  char** environ = PR_DuplicateEnvironment();
  for (char** varPtr = environ; *varPtr != nullptr; ++varPtr) {
    std::string varString = *varPtr;
    size_t equalPos = varString.find_first_of('=');
    std::string varName = varString.substr(0, equalPos);
    std::string varValue = varString.substr(equalPos + 1);
    if (combined_env_vars.find(varName) == combined_env_vars.end()) {
      combined_env_vars[varName] = varValue;
    }
    PR_Free(*varPtr);  // PR_DuplicateEnvironment() uses PR_Malloc().
  }
  PR_Free(environ);  // PR_DuplicateEnvironment() uses PR_Malloc().

  EnvironmentArray array(new char*[combined_env_vars.size() + 1]);
  size_t i = 0;
  for (const auto& key_val : combined_env_vars) {
    std::string entry(key_val.first);
    entry += "=";
    entry += key_val.second;
    array[i] = strdup(entry.c_str());
    i++;
  }
  array[i] = nullptr;
  return array;
}

}  // namespace base

namespace mozilla {

EnvironmentLog::EnvironmentLog(const char* varname, size_t len) {
  const char* e = getenv(varname);
  if (e && *e) {
    fname_ = e;
  }
}

void EnvironmentLog::print(const char* format, ...) {
  if (!fname_.size()) return;

  FILE* f;
  if (fname_.compare("-") == 0) {
    f = fdopen(dup(STDOUT_FILENO), "a");
  } else {
    f = fopen(fname_.c_str(), "a");
  }

  if (!f) return;

  va_list a;
  va_start(a, format);
  vfprintf(f, format, a);
  va_end(a);
  fclose(f);
}

}  // namespace mozilla
