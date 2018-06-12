/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file/namespace contains utility functions for enumerating, ending and
// computing statistics of processes.

#ifndef BASE_PROCESS_UTIL_H_
#define BASE_PROCESS_UTIL_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#elif defined(OS_LINUX) || defined(__GLIBC__)
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#elif defined(OS_MACOSX)
#include <mach/mach.h>
#endif

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#ifndef OS_WIN
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/process.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/EnvironmentMap.h"

#if defined(OS_MACOSX)
struct kinfo_proc;
#endif

namespace base {

// A minimalistic but hopefully cross-platform set of exit codes.
// Do not change the enumeration values or you will break third-party
// installers.
enum {
  PROCESS_END_NORMAL_TERMINATON = 0,
  PROCESS_END_KILLED_BY_USER    = 1,
  PROCESS_END_PROCESS_WAS_HUNG  = 2
};

// Returns the id of the current process.
ProcessId GetCurrentProcId();

// Returns the ProcessHandle of the current process.
ProcessHandle GetCurrentProcessHandle();

// Converts a PID to a process handle. This handle must be closed by
// CloseProcessHandle when you are done with it. Returns true on success.
bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle);

// Converts a PID to a process handle. On Windows the handle is opened
// with more access rights and must only be used by trusted code.
// You have to close returned handle using CloseProcessHandle. Returns true
// on success.
bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle);

// Closes the process handle opened by OpenProcessHandle.
void CloseProcessHandle(ProcessHandle process);

// Returns the unique ID for the specified process. This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before
// Win XP SP1 as well.
ProcessId GetProcId(ProcessHandle process);

#if defined(OS_POSIX)
// Sets all file descriptors to close on exec except for stdin, stdout
// and stderr.
// TODO(agl): remove this function
// WARNING: do not use. It's inherently race-prone in the face of
// multi-threading.
void SetAllFDsToCloseOnExec();
// Close all file descriptors, except for std{in,out,err} and those
// for which the given function returns true.  Only call this function
// in a child process where you know that there aren't any other
// threads.
void CloseSuperfluousFds(std::function<bool(int)>&& should_preserve);

typedef std::vector<std::pair<int, int> > file_handle_mapping_vector;
typedef std::map<std::string, std::string> environment_map;
#endif

struct LaunchOptions {
  // If true, wait for the process to terminate.  Otherwise, return
  // immediately.
  bool wait = false;

#if defined(OS_WIN)
  bool start_hidden = false;

  // Environment variables to be applied in addition to the current
  // process's environment, replacing them where necessary.
  EnvironmentMap env_map;

  std::vector<HANDLE> handles_to_inherit;
#endif
#if defined(OS_POSIX)
  environment_map env_map;

  // A mapping of (src fd -> dest fd) to propagate into the child
  // process.  All other fds will be closed, except std{in,out,err}.
  file_handle_mapping_vector fds_to_remap;
#endif

#if defined(OS_LINUX) || defined(OS_SOLARIS)
  struct ForkDelegate {
    virtual ~ForkDelegate() { }
    virtual pid_t Fork() = 0;
  };

  // If non-null, the fork delegate will be called instead of fork().
  mozilla::UniquePtr<ForkDelegate> fork_delegate = nullptr;
#endif
};

#if defined(OS_WIN)
// Runs the given application name with the given command line. Normally, the
// first command line argument should be the path to the process, and don't
// forget to quote it.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
//
// If process_handle is non-NULL, the process handle of the launched app will be
// stored there on a successful launch.
// NOTE: In this case, the caller is responsible for closing the handle so
//       that it doesn't leak!
bool LaunchApp(const std::wstring& cmdline,
               const LaunchOptions& options,
               ProcessHandle* process_handle);

#elif defined(OS_POSIX)
// Runs the application specified in argv[0] with the command line argv.
//
// The pid will be stored in process_handle if that pointer is
// non-null.
//
// Note that the first argument in argv must point to the filename,
// and must be fully specified (i.e., this will not search $PATH).
bool LaunchApp(const std::vector<std::string>& argv,
               const LaunchOptions& options,
               ProcessHandle* process_handle);

// Deleter for the array of strings allocated within BuildEnvironmentArray.
struct FreeEnvVarsArray
{
  void operator()(char** array);
};

typedef mozilla::UniquePtr<char*[], FreeEnvVarsArray> EnvironmentArray;

// Merge an environment map with the current environment.
// Existing variables are overwritten by env_vars_to_set.
EnvironmentArray BuildEnvironmentArray(const environment_map& env_vars_to_set);
#endif

// Executes the application specified by cl. This function delegates to one
// of the above two platform-specific functions.
bool LaunchApp(const CommandLine& cl,
               const LaunchOptions&,
               ProcessHandle* process_handle);

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code. If |wait| is true, wait
// for the process to be actually terminated before returning.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process, int exit_code, bool wait);

// Get the termination status (exit code) of the process and return true if the
// status indicates the process crashed. |child_exited| is set to true iff the
// child process has terminated. (|child_exited| may be NULL.)
//
// On Windows, it is an error to call this if the process hasn't terminated
// yet. On POSIX, |child_exited| is set correctly since we detect terminate in
// a different manner on POSIX.
bool DidProcessCrash(bool* child_exited, ProcessHandle handle);

}  // namespace base

namespace mozilla {

class EnvironmentLog
{
public:
  explicit EnvironmentLog(const char* varname) {
    const char *e = getenv(varname);
    if (e && *e) {
      fname_ = e;
    }
  }

  ~EnvironmentLog() {}

  void print(const char* format, ...) {
    if (!fname_.size())
      return;

    FILE* f;
    if (fname_.compare("-") == 0) {
      f = fdopen(dup(STDOUT_FILENO), "a");
    } else {
      f = fopen(fname_.c_str(), "a");
    }

    if (!f)
      return;

    va_list a;
    va_start(a, format);
    vfprintf(f, format, a);
    va_end(a);
    fclose(f);
  }

private:
  std::string fname_;

  DISALLOW_EVIL_CONSTRUCTORS(EnvironmentLog);
};

} // namespace mozilla

#if defined(OS_WIN)
// Undo the windows.h damage
#undef GetMessage
#undef CreateEvent
#undef GetClassName
#undef GetBinaryType
#undef RemoveDirectory
#undef LoadImage
#undef LoadIcon
#endif

#endif  // BASE_PROCESS_UTIL_H_
