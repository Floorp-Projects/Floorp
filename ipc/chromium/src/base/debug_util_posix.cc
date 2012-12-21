// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/debug_util.h"

#define MOZ_HAVE_EXECINFO_H (defined(OS_LINUX) && !defined(ANDROID))

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#if MOZ_HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#if defined(OS_MACOSX) || defined(OS_BSD)
#if defined(OS_OPENBSD)
#include <sys/proc.h>
#endif
#include <sys/sysctl.h>
#endif

#if defined(OS_DRAGONFLY) || defined(OS_FREEBSD)
#include <sys/user.h>
#endif

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"

#if defined(OS_NETBSD)
#undef KERN_PROC
#define KERN_PROC KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif

#if defined(OS_MACOSX)
#define KP_FLAGS kp_proc.p_flag
#elif defined(OS_DRAGONFLY)
#define KP_FLAGS kp_flags
#elif defined(OS_FREEBSD)
#define KP_FLAGS ki_flag
#else
#define KP_FLAGS p_flag
#endif

// static
bool DebugUtil::SpawnDebuggerOnProcess(unsigned /* process_id */) {
  NOTIMPLEMENTED();
  return false;
}

#if defined(OS_MACOSX) || defined(OS_BSD)

// Based on Apple's recommended method as described in
// http://developer.apple.com/qa/qa2004/qa1361.html
// static
bool DebugUtil::BeingDebugged() {
  // If the process is sandboxed then we can't use the sysctl, so cache the
  // value.
  static bool is_set = false;
  static bool being_debugged = false;

  if (is_set) {
    return being_debugged;
  }

  // Initialize mib, which tells sysctl what info we want.  In this case,
  // we're looking for information about a specific process ID.
  int mib[] = {
    CTL_KERN,
    KERN_PROC,
    KERN_PROC_PID,
    getpid(),
#if defined(OS_NETBSD) || defined(OS_OPENBSD)
    sizeof(KINFO_PROC),
    1,
#endif
  };

  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.  The source and
  // binary interfaces may change.
  KINFO_PROC info;
  size_t info_size = sizeof(info);

  int sysctl_result = sysctl(mib, arraysize(mib), &info, &info_size, NULL, 0);
  DCHECK(sysctl_result == 0);
  if (sysctl_result != 0) {
    is_set = true;
    being_debugged = false;
    return being_debugged;
  }

  // This process is being debugged if the P_TRACED flag is set.
  is_set = true;
  being_debugged = (info.KP_FLAGS & P_TRACED) != 0;
  return being_debugged;
}

#elif defined(OS_LINUX)

// We can look in /proc/self/status for TracerPid.  We are likely used in crash
// handling, so we are careful not to use the heap or have side effects.
// Another option that is common is to try to ptrace yourself, but then we
// can't detach without forking(), and that's not so great.
// static
bool DebugUtil::BeingDebugged() {
  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf)));
  HANDLE_EINTR(close(status_fd));

  if (num_read <= 0)
    return false;

  StringPiece status(buf, num_read);
  StringPiece tracer("TracerPid:\t");

  StringPiece::size_type pid_index = status.find(tracer);
  if (pid_index == StringPiece::npos)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += tracer.size();
  return pid_index < status.size() && status[pid_index] != '0';
}

#endif  // OS_LINUX

// static
void DebugUtil::BreakDebugger() {
#if defined(ARCH_CPU_X86_FAMILY)
  asm ("int3");
#endif
}

StackTrace::StackTrace() {
  const int kMaxCallers = 256;

  void* callers[kMaxCallers];
#if MOZ_HAVE_EXECINFO_H
  int count = backtrace(callers, kMaxCallers);
#else
  int count = 0;
#endif

  // Though the backtrace API man page does not list any possible negative
  // return values, we still still exclude them because they would break the
  // memcpy code below.
  if (count > 0) {
    trace_.resize(count);
    memcpy(&trace_[0], callers, sizeof(callers[0]) * count);
  } else {
    trace_.resize(0);
  }
}

void StackTrace::PrintBacktrace() {
  fflush(stderr);
#if MOZ_HAVE_EXECINFO_H
  backtrace_symbols_fd(&trace_[0], trace_.size(), STDERR_FILENO);
#endif
}

void StackTrace::OutputToStream(std::ostream* os) {
  return;
}
