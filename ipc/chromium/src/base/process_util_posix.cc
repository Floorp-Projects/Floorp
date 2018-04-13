/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>
#include <set>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "base/dir_reader_posix.h"

#include "mozilla/UniquePtr.h"
// For PR_DuplicateEnvironment:
#include "prenv.h"
#include "prmem.h"

const int kMicrosecondsPerSecond = 1000000;

namespace base {

ProcessId GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

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

ProcessId GetProcId(ProcessHandle process) {
  return process;
}

// Attempts to kill the process identified by the given process
// entry structure.  Ignores specified exit_code; posix can't force that.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process_id, int exit_code, bool wait) {
  bool result = kill(process_id, SIGTERM) == 0;

  if (!result && (errno == ESRCH)) {
    result = true;
    wait = false;
  }

  if (result && wait) {
    int tries = 60;
    bool exited = false;
    // The process may not end immediately due to pending I/O
    while (tries-- > 0) {
      int pid = HANDLE_EINTR(waitpid(process_id, NULL, WNOHANG));
      if (pid == process_id) {
        exited = true;
        break;
      } else if (errno == ECHILD) {
        exited = true;
        break;
      }

      sleep(1);
    }

    if (!exited) {
      result = kill(process_id, SIGKILL) == 0;
    }
  }

  if (!result)
    DLOG(ERROR) << "Unable to terminate process.";

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


void CloseSuperfluousFds(const base::InjectiveMultimap& saved_mapping) {
  // DANGER: no calls to malloc are allowed from now on:
  // http://crbug.com/36678
#if defined(ANDROID)
  static const rlim_t kSystemDefaultMaxFds = 1024;
  static const char kFDDir[] = "/proc/self/fd";
#elif defined(OS_LINUX) || defined(OS_SOLARIS)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char kFDDir[] = "/proc/self/fd";
#elif defined(OS_MACOSX)
  static const rlim_t kSystemDefaultMaxFds = 256;
  static const char kFDDir[] = "/dev/fd";
#elif defined(OS_BSD)
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

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  DirReaderPosix fd_dir(kFDDir);

  if (!fd_dir.IsValid()) {
    // Fallback case: Try every possible fd.
    for (rlim_t i = 0; i < max_fds; ++i) {
      const int fd = static_cast<int>(i);
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
        continue;
      InjectiveMultimap::const_iterator j;
      for (j = saved_mapping.begin(); j != saved_mapping.end(); j++) {
        if (fd == j->dest)
          break;
      }
      if (j != saved_mapping.end())
        continue;

      // Since we're just trying to close anything we can find,
      // ignore any error return values of close().
      IGNORE_EINTR(close(fd));
    }
    return;
  }

  const int dir_fd = fd_dir.fd();

  for ( ; fd_dir.Next(); ) {
    // Skip . and .. entries.
    if (fd_dir.name()[0] == '.')
      continue;

    char *endptr;
    errno = 0;
    const long int fd = strtol(fd_dir.name(), &endptr, 10);
    if (fd_dir.name()[0] == 0 || *endptr || fd < 0 || errno)
      continue;
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
      continue;
    InjectiveMultimap::const_iterator i;
    for (i = saved_mapping.begin(); i != saved_mapping.end(); i++) {
      if (fd == i->dest)
        break;
    }
    if (i != saved_mapping.end())
      continue;
    if (fd == dir_fd)
      continue;

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

// Sets all file descriptors to close on exec except for stdin, stdout
// and stderr.
// TODO(agl): Remove this function. It's fundamentally broken for multithreaded
// apps.
void SetAllFDsToCloseOnExec() {
#if defined(OS_LINUX) || defined(OS_SOLARIS)
  const char fd_dir[] = "/proc/self/fd";
#elif defined(OS_MACOSX) || defined(OS_BSD)
  const char fd_dir[] = "/dev/fd";
#endif
  ScopedDIR dir_closer(opendir(fd_dir));
  DIR *dir = dir_closer.get();
  if (NULL == dir) {
    DLOG(ERROR) << "Unable to open " << fd_dir;
    return;
  }

  struct dirent *ent;
  while ((ent = readdir(dir))) {
    // Skip . and .. entries.
    if (ent->d_name[0] == '.')
      continue;
    int i = atoi(ent->d_name);
    // We don't close stdin, stdout or stderr.
    if (i <= STDERR_FILENO)
      continue;

    int flags = fcntl(i, F_GETFD);
    if ((flags == -1) || (fcntl(i, F_SETFD, flags | FD_CLOEXEC) == -1)) {
      DLOG(ERROR) << "fcntl failure.";
    }
  }
}

bool DidProcessCrash(bool* child_exited, ProcessHandle handle) {
  int status;
  const int result = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  if (result == -1) {
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
    CHROMIUM_LOG(ERROR) << "waitpid failed pid:" << handle << " errno:" << errno;
    if (child_exited)
      *child_exited = true;
    return false;
  } else if (result == 0) {
    // the child hasn't exited yet.
    if (child_exited)
      *child_exited = false;
    return false;
  }

  if (child_exited)
    *child_exited = true;

  if (WIFSIGNALED(status)) {
    switch(WTERMSIG(status)) {
      case SIGSYS:
      case SIGSEGV:
      case SIGILL:
      case SIGABRT:
      case SIGFPE:
        return true;
      default:
        return false;
    }
  }

  if (WIFEXITED(status))
    return WEXITSTATUS(status) != 0;

  return false;
}

void
FreeEnvVarsArray::operator()(char** array)
{
  for (char** varPtr = array; *varPtr != nullptr; ++varPtr) {
    free(*varPtr);
  }
  delete[] array;
}

EnvironmentArray
BuildEnvironmentArray(const environment_map& env_vars_to_set)
{
  base::environment_map combined_env_vars = env_vars_to_set;
  char **environ = PR_DuplicateEnvironment();
  for (char** varPtr = environ; *varPtr != nullptr; ++varPtr) {
    std::string varString = *varPtr;
    size_t equalPos = varString.find_first_of('=');
    std::string varName = varString.substr(0, equalPos);
    std::string varValue = varString.substr(equalPos + 1);
    if (combined_env_vars.find(varName) == combined_env_vars.end()) {
      combined_env_vars[varName] = varValue;
    }
    PR_Free(*varPtr); // PR_DuplicateEnvironment() uses PR_Malloc().
  }
  PR_Free(environ); // PR_DuplicateEnvironment() uses PR_Malloc().

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
