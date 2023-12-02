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

#if defined(XP_WIN)
#  include "mozilla/ipc/EnvironmentMap.h"
#  include <windows.h>
#  include <tlhelp32.h>
#elif defined(XP_LINUX) || defined(__GLIBC__)
#  include <dirent.h>
#  include <limits.h>
#  include <sys/types.h>
#elif defined(XP_DARWIN)
#  include <mach/mach.h>
#endif

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/process.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"

#include "mozilla/ipc/LaunchError.h"

#if defined(MOZ_ENABLE_FORKSERVER)
#  include "nsStringFwd.h"
#  include "mozilla/ipc/FileDescriptorShuffle.h"

namespace mozilla {
namespace ipc {
class FileDescriptor;
}
}  // namespace mozilla
#endif

#if defined(XP_DARWIN)
struct kinfo_proc;
#endif

class CommandLine;

namespace base {

using mozilla::Err;
using mozilla::Ok;
using mozilla::Result;
using mozilla::ipc::LaunchError;

enum ProcessArchitecture {
  PROCESS_ARCH_INVALID = 0x0,
  PROCESS_ARCH_I386 = 0x1,
  PROCESS_ARCH_X86_64 = 0x2,
  PROCESS_ARCH_PPC = 0x4,
  PROCESS_ARCH_PPC_64 = 0x8,
  PROCESS_ARCH_ARM = 0x10,
  PROCESS_ARCH_ARM_64 = 0x20
};

// A minimalistic but hopefully cross-platform set of exit codes.
// Do not change the enumeration values or you will break third-party
// installers.
enum {
  PROCESS_END_NORMAL_TERMINATON = 0,
  PROCESS_END_KILLED_BY_USER = 1,
  PROCESS_END_PROCESS_WAS_HUNG = 2
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

#if defined(XP_UNIX)
// Close all file descriptors, except for std{in,out,err} and those
// for which the given function returns true.  Only call this function
// in a child process where you know that there aren't any other
// threads.
void CloseSuperfluousFds(void* aCtx, bool (*aShouldPreserve)(void*, int));

typedef std::vector<std::pair<int, int> > file_handle_mapping_vector;
typedef std::map<std::string, std::string> environment_map;

// Deleter for the array of strings allocated within BuildEnvironmentArray.
struct FreeEnvVarsArray {
  void operator()(char** array);
};

typedef mozilla::UniquePtr<char*[], FreeEnvVarsArray> EnvironmentArray;
#endif

struct LaunchOptions {
  // If true, wait for the process to terminate.  Otherwise, return
  // immediately.
  bool wait = false;

#if defined(XP_WIN)
  bool start_hidden = false;

  // Start as an independent process rather than a process that is closed by the
  // parent job. This will pass the flag CREATE_BREAKAWAY_FROM_JOB.
  bool start_independent = false;

  // Environment variables to be applied in addition to the current
  // process's environment, replacing them where necessary.
  EnvironmentMap env_map;

  std::vector<HANDLE> handles_to_inherit;
#endif
#if defined(XP_UNIX)
  environment_map env_map;

  // If non-null, specifies the entire environment to use for the
  // child process, instead of inheriting from the parent; env_map is
  // ignored in that case.  Note that the strings are allocated using
  // malloc (e.g., with strdup), but the array of pointers is
  // allocated with new[] and is terminated with a null pointer.
  EnvironmentArray full_env;

  // If non-empty, set the child process's current working directory.
  std::string workdir;

  // A mapping of (src fd -> dest fd) to propagate into the child
  // process.  All other fds will be closed, except std{in,out,err}.
  file_handle_mapping_vector fds_to_remap;
#endif

#if defined(MOZ_ENABLE_FORKSERVER)
  bool use_forkserver = false;
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  // These fields are used by the sandboxing code in SandboxLaunch.cpp.
  // It's not ideal to have them here, but trying to abstract them makes
  // it harder to serialize LaunchOptions for the fork server.
  //
  // (fork_flags holds extra flags for the clone() syscall, and
  // sandbox_chroot indicates whether the child process will be given
  // the ability to chroot() itself to an empty directory.)
  int fork_flags = 0;
  bool sandbox_chroot = false;
#endif

#ifdef XP_DARWIN
  // On macOS 10.14+, disclaims responsibility for the child process
  // with respect to privacy/security permission prompts and
  // decisions.  Ignored if not supported by the OS.
  bool disclaim = false;
#  ifdef __aarch64__
  // The architecture to launch when launching a "universal" binary.
  // Note: the implementation only supports launching x64 child
  // processes from arm64 parent processes.
  uint32_t arch = PROCESS_ARCH_INVALID;
#  endif  // __aarch64__
#endif    // XP_DARWIN
};

#if defined(XP_WIN)
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
Result<Ok, LaunchError> LaunchApp(const std::wstring& cmdline,
                                  const LaunchOptions& options,
                                  ProcessHandle* process_handle);

Result<Ok, LaunchError> LaunchApp(const CommandLine& cl, const LaunchOptions&,
                                  ProcessHandle* process_handle);
#else
// Runs the application specified in argv[0] with the command line argv.
//
// The pid will be stored in process_handle if that pointer is
// non-null.
//
// Note that the first argument in argv must point to the filename,
// and must be fully specified (i.e., this will not search $PATH).
Result<Ok, LaunchError> LaunchApp(const std::vector<std::string>& argv,
                                  LaunchOptions&& options,
                                  ProcessHandle* process_handle);

// Merge an environment map with the current environment.
// Existing variables are overwritten by env_vars_to_set.
EnvironmentArray BuildEnvironmentArray(const environment_map& env_vars_to_set);
#endif

#if defined(MOZ_ENABLE_FORKSERVER)
/**
 * Create and initialize a new process as a content process.
 *
 * This class is used only by the fork server.
 * To create a new content process, two steps are
 *  - calling |ForkProcess()| to create a new process, and
 *  - calling |InitAppProcess()| in the new process, the child
 *    process, to initialize it for running WEB content later.
 *
 * The fork server can clean up it's resources in-between the first
 * and second step, that is why two steps.
 */
class AppProcessBuilder {
 public:
  AppProcessBuilder();
  // This function will fork a new process for use as a
  // content processes.
  bool ForkProcess(const std::vector<std::string>& argv,
                   LaunchOptions&& options, ProcessHandle* process_handle);
  // This function will be called in the child process to initializes
  // the environment of the content process.  It should be called
  // after the message loop of the main thread, to make sure the fork
  // server is destroyed properly in the child process.
  //
  // The message loop may allocate resources like file descriptors.
  // If this function is called before the end of the loop, the
  // reosurces may be destroyed while the loop is still alive.
  void InitAppProcess(int* argcp, char*** argvp);

 private:
  void ReplaceArguments(int* argcp, char*** argvp);

  mozilla::ipc::FileDescriptorShuffle shuffle_;
  std::vector<std::string> argv_;
};

void InitForkServerProcess();

/**
 * Make a FD not being closed when create a new content process.
 *
 * AppProcessBuilder would close most unrelated FDs for new content
 * processes.  You may want to reserve some of FDs to keep using them
 * in content processes.
 */
void RegisterForkServerNoCloseFD(int aFd);
#endif

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process, int exit_code);

#ifdef XP_UNIX
// Returns whether the given process has exited.  If it returns true,
// the process status has been consumed and `IsProcessDead` should not
// be called again on the same process (like `waitpid`).
//
// In various error cases (e.g., the process doesn't exist or isn't a
// child of this process) it will also return true to indicate that
// the caller should give up and not try again.
//
// If the `blocking` parameter is set to true, this function will try
// to block the calling thread indefinitely until the process exits.
// This may not be possible (if the child is also being debugged by
// the parent process, e.g. due to the crash reporter), in which case
// it will return false and the caller will need to wait and retry.
bool IsProcessDead(ProcessHandle handle, bool blocking = false);
#endif

}  // namespace base

namespace mozilla {

class EnvironmentLog {
 public:
  template <size_t N>
  explicit EnvironmentLog(const char (&varname)[N])
      : EnvironmentLog(varname, N) {}

  ~EnvironmentLog() {}

  void print(const char* format, ...);

 private:
  explicit EnvironmentLog(const char* varname, size_t len);

#if defined(XP_WIN)
  std::wstring fname_;
#else
  std::string fname_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(EnvironmentLog);
};

#if defined(MOZ_ENABLE_FORKSERVER)
typedef std::tuple<nsCString, nsCString> EnvVar;
typedef std::tuple<mozilla::ipc::FileDescriptor, int> FdMapping;
#endif

}  // namespace mozilla

#if defined(XP_WIN)
// Undo the windows.h damage
#  undef GetMessage
#  undef CreateEvent
#  undef GetClassName
#  undef GetBinaryType
#  undef RemoveDirectory
#  undef LoadImage
#  undef LoadIcon
#endif

#endif  // BASE_PROCESS_UTIL_H_
