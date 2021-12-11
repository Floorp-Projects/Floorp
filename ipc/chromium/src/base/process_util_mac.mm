/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>

#include <string>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "mozilla/ipc/FileDescriptorShuffle.h"
#include "mozilla/ScopeExit.h"

namespace {

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

bool LaunchApp(const std::vector<std::string>& argv, const LaunchOptions& options,
               ProcessHandle* process_handle) {
  bool retval = true;

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  EnvironmentArray vars = BuildEnvironmentArray(options.env_map);

  posix_spawn_file_actions_t file_actions;
  if (posix_spawn_file_actions_init(&file_actions) != 0) {
    return false;
  }
  auto file_actions_guard =
      mozilla::MakeScopeExit([&file_actions] { posix_spawn_file_actions_destroy(&file_actions); });

  // Turn fds_to_remap array into a set of dup2 calls.
  mozilla::ipc::FileDescriptorShuffle shuffle;
  if (!shuffle.Init(options.fds_to_remap)) {
    return false;
  }
  for (const auto& fd_map : shuffle.Dup2Sequence()) {
    int src_fd = fd_map.first;
    int dest_fd = fd_map.second;

    if (posix_spawn_file_actions_adddup2(&file_actions, src_fd, dest_fd) != 0) {
      return false;
    }
  }

  // Initialize spawn attributes.
  posix_spawnattr_t spawnattr;
  if (posix_spawnattr_init(&spawnattr) != 0) {
    return false;
  }
  auto spawnattr_guard =
      mozilla::MakeScopeExit([&spawnattr] { posix_spawnattr_destroy(&spawnattr); });

#if defined(XP_MACOSX) && defined(__aarch64__)
  if (options.arch == PROCESS_ARCH_X86_64) {
    cpu_type_t cpu_pref = CPU_TYPE_X86_64;
    size_t count = 1;
    size_t ocount = 0;
    int rv;
    rv = posix_spawnattr_setbinpref_np(&spawnattr, count, &cpu_pref, &ocount);
    if ((rv != 0) || (ocount != count)) {
      return false;
    }
  }
#endif

  // Prevent the child process from inheriting any file descriptors
  // that aren't named in `file_actions`.  (This is an Apple-specific
  // extension to posix_spawn.)
  if (posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_CLOEXEC_DEFAULT) != 0) {
    return false;
  }

  // Exempt std{in,out,err} from being closed by POSIX_SPAWN_CLOEXEC_DEFAULT.
  for (int fd = 0; fd <= STDERR_FILENO; ++fd) {
    if (posix_spawn_file_actions_addinherit_np(&file_actions, fd) != 0) {
      return false;
    }
  }

  int pid = 0;
  int spawn_succeeded =
      (posix_spawnp(&pid, argv_copy[0], &file_actions, &spawnattr, argv_copy, vars.get()) == 0);

  bool process_handle_valid = pid > 0;
  if (!spawn_succeeded || !process_handle_valid) {
    retval = false;
  } else {
    gProcessLog.print("==> process %d launched child process %d\n", GetCurrentProcId(), pid);
    if (options.wait) HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle) *process_handle = pid;
  }

  return retval;
}

bool LaunchApp(const CommandLine& cl, const LaunchOptions& options, ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), options, process_handle);
}

}  // namespace base
