/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "algorithm"

#if defined(MOZ_ENABLE_FORKSERVER)
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#  if defined(DEBUG)
#include "base/message_loop.h"
#  endif
#include "mozilla/DebugOnly.h"
#include "mozilla/ipc/ForkServiceChild.h"

#include "mozilla/Unused.h"
#include "mozilla/ScopeExit.h"

using namespace mozilla::ipc;
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/FileDescriptorShuffle.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/StaticPtr.h"

// WARNING: despite the name, this file is also used on the BSDs and
// Solaris (basically, Unixes that aren't Mac OS), not just Linux.

namespace {

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

#if defined(MOZ_ENABLE_FORKSERVER)
static mozilla::StaticAutoPtr<std::vector<int> > sNoCloseFDs;

void
RegisterForkServerNoCloseFD(int fd) {
  if (!sNoCloseFDs) {
    sNoCloseFDs = new std::vector<int>();
  }
  sNoCloseFDs->push_back(fd);
}

static bool
IsNoCloseFd(int fd) {
  if (!sNoCloseFDs) {
    return false;
  }
  return std::any_of(sNoCloseFDs->begin(), sNoCloseFDs->end(),
                     [fd](int regfd) -> bool { return regfd == fd; });
}

AppProcessBuilder::AppProcessBuilder() {
}

static void
ReplaceEnviroment(const LaunchOptions& options) {
  for (auto& elt : options.env_map) {
    setenv(elt.first.c_str(), elt.second.c_str(), 1);
  }
}

bool
AppProcessBuilder::ForkProcess(const std::vector<std::string>& argv,
                               const LaunchOptions& options, ProcessHandle* process_handle) {
  auto cleanFDs = mozilla::MakeScopeExit([&] {
    for (auto& elt : options.fds_to_remap) {
      auto fd = std::get<0>(elt);
      close(fd);
    }
  });

  argv_ = argv;
  if (!shuffle_.Init(options.fds_to_remap)) {
    return false;
  }

  // Avoid the content of the buffer being sent out by child processes
  // repeatly.
  fflush(stdout);
  fflush(stderr);

#ifdef OS_LINUX
  pid_t pid = options.fork_delegate ? options.fork_delegate->Fork() : fork();
  // WARNING: if pid == 0, only async signal safe operations are permitted from
  // here until exec or _exit.
  //
  // Specifically, heap allocation is not safe: the sandbox's fork substitute
  // won't run the pthread_atfork handlers that fix up the malloc locks.
#else
  pid_t pid = fork();
#endif

  if (pid < 0) {
    return false;
  }

  if (pid == 0) {
    cleanFDs.release();
    ReplaceEnviroment(options);
  } else {
    gProcessLog.print("==> process %d launched child process %d\n",
                      GetCurrentProcId(), pid);
    if (options.wait) HANDLE_EINTR(waitpid(pid, 0, 0));
  }

  if (process_handle) *process_handle = pid;

  return true;
}

void
AppProcessBuilder::ReplaceArguments(int *argcp, char*** argvp) {
  // Change argc & argv of main() with the arguments passing
  // through IPC.
  char** argv = new char*[argv_.size() + 1];
  char** p = argv;
  for (auto& elt : argv_) {
    *p++ = strdup(elt.c_str());
  }
  *p = nullptr;
  *argvp = argv;
  *argcp = argv_.size();
}

void
AppProcessBuilder::InitAppProcess(int *argcp, char*** argvp) {
  MOZ_ASSERT(MessageLoop::current() == nullptr,
             "The message loop of the main thread should have been destroyed");

  // The fork server handle SIGCHLD to read status of content
  // processes to handle Zombies.  But, it is not necessary for
  // content processes.
  signal(SIGCHLD, SIG_DFL);

  for (const auto& fds : shuffle_.Dup2Sequence()) {
    int fd = HANDLE_EINTR(dup2(fds.first, fds.second));
    MOZ_RELEASE_ASSERT(fd == fds.second, "dup2 failed");
  }

  CloseSuperfluousFds(&shuffle_, [](void* ctx, int fd) {
    return static_cast<decltype(&shuffle_)>(ctx)->MapsTo(fd) ||
      IsNoCloseFd(fd);
  });
  // Without this, the destructor of |shuffle_| would try to close FDs
  // created by it, but they have been closed by
  // |CloseSuperfluousFds()|.
  shuffle_.Forget();

  ReplaceArguments(argcp, argvp);
}

static void
handle_sigchld(int s) {
  waitpid(-1, nullptr, WNOHANG);
}

static void
InstallChildSignalHandler() {
  // Since content processes are not children of the chrome process
  // any more, the fork server process has to handle SIGCHLD, or
  // content process would remain zombie after dead.
  signal(SIGCHLD, handle_sigchld);
}

static void
ReserveFileDescriptors() {
  // Reserve the lower positions of the file descriptors to make sure
  // debug files and other files don't take these positions.  So we
  // can keep their file descriptors during CloseSuperfluousFds() with
  // out any confliction with mapping passing from the parent process.
  int fd = open("/dev/null", O_RDONLY);
  for (int i = 1; i < 10; i++) {
    mozilla::Unused << dup(fd);
  }
}

void
InitForkServerProcess() {
  InstallChildSignalHandler();
  ReserveFileDescriptors();
}

static bool
LaunchAppWithForkServer(const std::vector<std::string>& argv,
                      const LaunchOptions& options,
                      ProcessHandle* process_handle) {
  MOZ_ASSERT(ForkServiceChild::Get());

  nsTArray<nsCString> _argv(argv.size());
  nsTArray<mozilla::EnvVar> env(options.env_map.size());
  nsTArray<mozilla::FdMapping> fdsremap(options.fds_to_remap.size());

  for (auto& arg : argv) {
    _argv.AppendElement(arg.c_str());
  }
  for (auto& vv : options.env_map) {
    env.AppendElement(mozilla::EnvVar(nsCString(vv.first.c_str()),
                                      nsCString(vv.second.c_str())));
  }
  for (auto& fdmapping : options.fds_to_remap) {
    fdsremap.AppendElement(mozilla::FdMapping(mozilla::ipc::FileDescriptor(fdmapping.first),
                                              fdmapping.second));
  }

  return ForkServiceChild::Get()->SendForkNewSubprocess(_argv, env,
                                                       fdsremap,
                                                       process_handle);
}
#endif  // MOZ_ENABLE_FORKSERVER

bool LaunchApp(const std::vector<std::string>& argv,
               const LaunchOptions& options, ProcessHandle* process_handle) {
#if defined(MOZ_ENABLE_FORKSERVER)
  if (options.use_forkserver) {
    return LaunchAppWithForkServer(argv, options, process_handle);
  }
#endif

  mozilla::UniquePtr<char*[]> argv_cstr(new char*[argv.size() + 1]);

  EnvironmentArray envp = BuildEnvironmentArray(options.env_map);
  mozilla::ipc::FileDescriptorShuffle shuffle;
  if (!shuffle.Init(options.fds_to_remap)) {
    return false;
  }

#ifdef OS_LINUX
  pid_t pid = options.fork_delegate ? options.fork_delegate->Fork() : fork();
  // WARNING: if pid == 0, only async signal safe operations are permitted from
  // here until exec or _exit.
  //
  // Specifically, heap allocation is not safe: the sandbox's fork substitute
  // won't run the pthread_atfork handlers that fix up the malloc locks.
#else
  pid_t pid = fork();
#endif

  if (pid < 0) return false;

  if (pid == 0) {
    // In the child:
    for (const auto& fds : shuffle.Dup2Sequence()) {
      if (HANDLE_EINTR(dup2(fds.first, fds.second)) != fds.second) {
        // This shouldn't happen, but check for it.  And see below
        // about logging being unsafe here, so this is debug only.
        DLOG(ERROR) << "dup2 failed";
        _exit(127);
      }
    }

    CloseSuperfluousFds(&shuffle, [](void* aCtx, int aFd) {
      return static_cast<decltype(&shuffle)>(aCtx)->MapsTo(aFd);
    });

    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;

    execve(argv_cstr[0], argv_cstr.get(), envp.get());
    // if we get here, we're in serious trouble and should complain loudly
    // NOTE: This is async signal unsafe; it could deadlock instead.  (But
    // only on debug builds; otherwise it's a signal-safe no-op.)
    DLOG(ERROR) << "FAILED TO exec() CHILD PROCESS, path: " << argv_cstr[0];
    _exit(127);
  }

  // In the parent:
  gProcessLog.print("==> process %d launched child process %d\n",
                    GetCurrentProcId(), pid);
  if (options.wait) HANDLE_EINTR(waitpid(pid, 0, 0));

  if (process_handle) *process_handle = pid;

  return true;
}

bool LaunchApp(const CommandLine& cl, const LaunchOptions& options,
               ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), options, process_handle);
}

}  // namespace base
