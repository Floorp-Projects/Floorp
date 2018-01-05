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

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

namespace {

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

bool LaunchApp(const std::vector<std::string>& argv,
               const LaunchOptions& options,
               ProcessHandle* process_handle)
{
  mozilla::UniquePtr<char*[]> argv_cstr(new char*[argv.size() + 1]);
  // Illegal to allocate memory after fork and before execvp
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  fd_shuffle1.reserve(options.fds_to_remap.size());
  fd_shuffle2.reserve(options.fds_to_remap.size());

  EnvironmentArray envp = BuildEnvironmentArray(options.environ);

  pid_t pid = fork();
  if (pid < 0)
    return false;

  if (pid == 0) {
    // In the child:
    for (const auto& fd_map : options.fds_to_remap) {
      fd_shuffle1.push_back(InjectionArc(fd_map.first, fd_map.second, false));
      fd_shuffle2.push_back(InjectionArc(fd_map.first, fd_map.second, false));
    }

    if (!ShuffleFileDescriptors(&fd_shuffle1))
      _exit(127);

    CloseSuperfluousFds(fd_shuffle2);

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
  if (options.wait)
    HANDLE_EINTR(waitpid(pid, 0, 0));

  if (process_handle)
    *process_handle = pid;

  return true;
}

bool LaunchApp(const CommandLine& cl,
               const LaunchOptions& options,
               ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), options, process_handle);
}

}  // namespace base
