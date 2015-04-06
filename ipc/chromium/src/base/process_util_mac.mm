// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <spawn.h>
#include <sys/wait.h>

#include <string>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/time.h"

namespace base {

void FreeEnvVarsArray(char* array[], int length)
{
  for (int i = 0; i < length; i++) {
    free(array[i]);
  }
  delete[] array;
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle) {
  return LaunchApp(argv, fds_to_remap, environment_map(),
                   wait, process_handle);
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               const environment_map& env_vars_to_set,
               bool wait, ProcessHandle* process_handle,
               ProcessArchitecture arch) {
  return LaunchApp(argv, fds_to_remap, env_vars_to_set,
                   PRIVILEGES_INHERIT,
                   wait, process_handle);
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               const environment_map& env_vars_to_set,
               ChildPrivileges privs,
               bool wait, ProcessHandle* process_handle,
               ProcessArchitecture arch) {
  bool retval = true;

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  // Make sure we don't leak any FDs to the child process by marking all FDs
  // as close-on-exec.
  SetAllFDsToCloseOnExec();

  // Copy _NSGetEnviron() to a new char array and add the variables
  // in env_vars_to_set.
  // Existing variables are overwritten by env_vars_to_set.
  int pos = 0;
  environment_map combined_env_vars = env_vars_to_set;
  while((*_NSGetEnviron())[pos] != NULL) {
    std::string varString = (*_NSGetEnviron())[pos];
    std::string varName = varString.substr(0, varString.find_first_of('='));
    std::string varValue = varString.substr(varString.find_first_of('=') + 1);
    if (combined_env_vars.find(varName) == combined_env_vars.end()) {
      combined_env_vars[varName] = varValue;
    }
    pos++;
  }
  int varsLen = combined_env_vars.size() + 1;

  char** vars = new char*[varsLen];
  int i = 0;
  for (environment_map::const_iterator it = combined_env_vars.begin();
       it != combined_env_vars.end(); ++it) {
    std::string entry(it->first);
    entry += "=";
    entry += it->second;
    vars[i] = strdup(entry.c_str());
    i++;
  }
  vars[i] = NULL;

  posix_spawn_file_actions_t file_actions;
  if (posix_spawn_file_actions_init(&file_actions) != 0) {
    FreeEnvVarsArray(vars, varsLen);
    return false;
  }

  // Turn fds_to_remap array into a set of dup2 calls.
  for (file_handle_mapping_vector::const_iterator it = fds_to_remap.begin();
       it != fds_to_remap.end();
       ++it) {
    int src_fd = it->first;
    int dest_fd = it->second;

    if (src_fd == dest_fd) {
      int flags = fcntl(src_fd, F_GETFD);
      if (flags != -1) {
        fcntl(src_fd, F_SETFD, flags & ~FD_CLOEXEC);
      }
    } else {
      if (posix_spawn_file_actions_adddup2(&file_actions, src_fd, dest_fd) != 0) {
        posix_spawn_file_actions_destroy(&file_actions);
        FreeEnvVarsArray(vars, varsLen);
        return false;
      }
    }
  }

  // Set up the CPU preference array.
  cpu_type_t cpu_types[1];
  switch (arch) {
    case PROCESS_ARCH_I386:
      cpu_types[0] = CPU_TYPE_X86;
      break;
    case PROCESS_ARCH_X86_64:
      cpu_types[0] = CPU_TYPE_X86_64;
      break;
    case PROCESS_ARCH_PPC:
      cpu_types[0] = CPU_TYPE_POWERPC;
    default:
      cpu_types[0] = CPU_TYPE_ANY;
      break;
  }

  // Initialize spawn attributes.
  posix_spawnattr_t spawnattr;
  if (posix_spawnattr_init(&spawnattr) != 0) {
    FreeEnvVarsArray(vars, varsLen);
    return false;
  }

  // Set spawn attributes.
  size_t attr_count = 1;
  size_t attr_ocount = 0;
  if (posix_spawnattr_setbinpref_np(&spawnattr, attr_count, cpu_types, &attr_ocount) != 0 ||
      attr_ocount != attr_count) {
    FreeEnvVarsArray(vars, varsLen);
    posix_spawnattr_destroy(&spawnattr);
    return false;
  }

  int pid = 0;
  int spawn_succeeded = (posix_spawnp(&pid,
                                      argv_copy[0],
                                      &file_actions,
                                      &spawnattr,
                                      argv_copy,
                                      vars) == 0);

  FreeEnvVarsArray(vars, varsLen);

  posix_spawn_file_actions_destroy(&file_actions);

  posix_spawnattr_destroy(&spawnattr);

  bool process_handle_valid = pid > 0;
  if (!spawn_succeeded || !process_handle_valid) {
    retval = false;
  } else {
    if (wait)
      HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle)
      *process_handle = pid;
  }

  return retval;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  // TODO(playmobil): Do we need to respect the start_hidden flag?
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle);
}

void SetCurrentProcessPrivileges(ChildPrivileges privs) {

}

}  // namespace base
