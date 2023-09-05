/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <fcntl.h>
#include <os/availability.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "mozilla/ipc/FileDescriptorShuffle.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Result.h"

#include "mozilla/ipc/LaunchError.h"

extern "C" {
// N.B. the syscalls are available back to 10.5, but the C wrappers
// only in 10.12.  Fortunately, 10.15 is our current baseline.
int pthread_fchdir_np(int fd) API_AVAILABLE(macosx(10.12));

int responsibility_spawnattrs_setdisclaim(posix_spawnattr_t attrs, int disclaim)
    API_AVAILABLE(macosx(10.14));
}

namespace {

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

Result<Ok, LaunchError> LaunchApp(const std::vector<std::string>& argv,
                                  const LaunchOptions& options,
                                  ProcessHandle* process_handle) {
  Result<Ok, LaunchError> retval = Ok();

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  EnvironmentArray env_storage;
  const EnvironmentArray& vars =
      options.full_env ? options.full_env
                       : (env_storage = BuildEnvironmentArray(options.env_map));

  posix_spawn_file_actions_t file_actions;
  int err = posix_spawn_file_actions_init(&file_actions);
  if (err != 0) {
    DLOG(WARNING) << "posix_spawn_file_actions_init failed";
    return Err(LaunchError("posix_spawn_file_actions_init", err));
  }
  auto file_actions_guard = mozilla::MakeScopeExit(
      [&file_actions] { posix_spawn_file_actions_destroy(&file_actions); });

  // Turn fds_to_remap array into a set of dup2 calls.
  //
  // Init() there will call fcntl(F_DUPFD/F_DUPFD_CLOEXEC) under the hood in
  // https://searchfox.org/mozilla-central/rev/55d5c4b9dffe5e59eb6b019c1a930ec9ada47e10/ipc/glue/FileDescriptorShuffle.cpp#72
  // so it will set errno.
  mozilla::ipc::FileDescriptorShuffle shuffle;
  if (!shuffle.Init(options.fds_to_remap)) {
    DLOG(WARNING) << "FileDescriptorShuffle::Init failed";
    return Err(LaunchError("FileDescriptorShuffle", errno));
  }
  for (const auto& fd_map : shuffle.Dup2Sequence()) {
    int src_fd = fd_map.first;
    int dest_fd = fd_map.second;

    int rv = posix_spawn_file_actions_adddup2(&file_actions, src_fd, dest_fd);
    if (rv != 0) {
      DLOG(WARNING) << "posix_spawn_file_actions_adddup2 failed";
      return Err(LaunchError("posix_spawn_file_actions_adddup2", rv));
    }
  }

  if (!options.workdir.empty()) {
    int rv = posix_spawn_file_actions_addchdir_np(&file_actions,
                                                  options.workdir.c_str());
    if (rv != 0) {
      DLOG(WARNING) << "posix_spawn_file_actions_addchdir_np failed";
      return Err(LaunchError("posix_spawn_file_actions_addchdir", rv));
    }
  }

  // Initialize spawn attributes.
  posix_spawnattr_t spawnattr;
  err = posix_spawnattr_init(&spawnattr);
  if (err != 0) {
    DLOG(WARNING) << "posix_spawnattr_init failed";
    return Err(LaunchError("posix_spawnattr_init", err));
  }
  auto spawnattr_guard = mozilla::MakeScopeExit(
      [&spawnattr] { posix_spawnattr_destroy(&spawnattr); });

#if defined(XP_MACOSX) && defined(__aarch64__)
  if (options.arch == PROCESS_ARCH_X86_64) {
    cpu_type_t cpu_pref = CPU_TYPE_X86_64;
    size_t count = 1;
    size_t ocount = 0;
    int rv =
        posix_spawnattr_setbinpref_np(&spawnattr, count, &cpu_pref, &ocount);
    if ((rv != 0) || (ocount != count)) {
      DLOG(WARNING) << "posix_spawnattr_setbinpref_np failed";
      return Err(LaunchError("posix_spawnattr_setbinpref_np", rv));
    }
  }
#endif

  if (options.disclaim) {
    int err = responsibility_spawnattrs_setdisclaim(&spawnattr, 1);
    if (err != 0) {
      DLOG(WARNING) << "responsibility_spawnattrs_setdisclaim failed";
      return Err(LaunchError("responsibility_spawnattrs_setdisclaim", err));
    }
  }

  // Prevent the child process from inheriting any file descriptors
  // that aren't named in `file_actions`.  (This is an Apple-specific
  // extension to posix_spawn.)
  err = posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_CLOEXEC_DEFAULT);
  if (err != 0) {
    DLOG(WARNING) << "posix_spawnattr_setflags failed";
    return Err(LaunchError("posix_spawnattr_setflags", err));
  }

  // Exempt std{in,out,err} from being closed by POSIX_SPAWN_CLOEXEC_DEFAULT.
  for (int fd = 0; fd <= STDERR_FILENO; ++fd) {
    err = posix_spawn_file_actions_addinherit_np(&file_actions, fd);
    if (err != 0) {
      DLOG(WARNING) << "posix_spawn_file_actions_addinherit_np failed";
      return Err(LaunchError("posix_spawn_file_actions_addinherit_np", err));
    }
  }

  int pid = 0;
  int spawn_succeeded = (posix_spawnp(&pid, argv_copy[0], &file_actions,
                                      &spawnattr, argv_copy, vars.get()) == 0);

  bool process_handle_valid = pid > 0;
  if (!spawn_succeeded || !process_handle_valid) {
    DLOG(WARNING) << "posix_spawnp failed";
    retval = Err(LaunchError("posix_spawnp", spawn_succeeded));
  } else {
    gProcessLog.print("==> process %d launched child process %d\n",
                      GetCurrentProcId(), pid);
    if (options.wait) HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle) *process_handle = pid;
  }

  return retval;
}

Result<Ok, LaunchError> LaunchApp(const CommandLine& cl,
                                  const LaunchOptions& options,
                                  ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), options, process_handle);
}

}  // namespace base
