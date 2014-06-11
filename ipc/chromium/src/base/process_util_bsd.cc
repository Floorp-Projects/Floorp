// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// derived from process_util_linux.cc and process_util_mac.cc

#include "base/process_util.h"

#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

#if defined(_POSIX_SPAWN) && _POSIX_SPAWN > 0
#define HAVE_POSIX_SPAWN	1
#endif

/*
 * On platforms that are not gonk based, we fall back to an arbitrary
 * UID. This is generally the UID for user `nobody', albeit it is not
 * always the case.
 */

#if defined(OS_NETBSD) || defined(OS_OPENBSD)
# define CHILD_UNPRIVILEGED_UID 32767
# define CHILD_UNPRIVILEGED_GID 32767
#else
# define CHILD_UNPRIVILEGED_UID 65534
# define CHILD_UNPRIVILEGED_GID 65534
#endif

#ifndef __dso_public
# ifdef __exported
#  define __dso_public	__exported
# else
#  define __dso_public	__attribute__((__visibility__("default")))
# endif
#endif

#ifdef HAVE_POSIX_SPAWN
#include <spawn.h>
extern "C" char **environ __dso_public;
#endif

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

#ifdef HAVE_POSIX_SPAWN

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

  // Copy environment to a new char array and add the variables
  // in env_vars_to_set.
  // Existing variables are overwritten by env_vars_to_set.
  int pos = 0;
  environment_map combined_env_vars = env_vars_to_set;
  while(environ[pos] != NULL) {
    std::string varString = environ[pos];
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

  pid_t pid = 0;
  int spawn_succeeded = (posix_spawnp(&pid,
                                      argv_copy[0],
                                      &file_actions,
                                      NULL,
                                      argv_copy,
                                      vars) == 0);

  FreeEnvVarsArray(vars, varsLen);

  posix_spawn_file_actions_destroy(&file_actions);

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

#else // no posix_spawn, use fork/exec

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
  scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
  // Illegal to allocate memory after fork and before execvp
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  fd_shuffle1.reserve(fds_to_remap.size());
  fd_shuffle2.reserve(fds_to_remap.size());

  pid_t pid = fork();
  if (pid < 0)
    return false;

  if (pid == 0) {
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle1.push_back(InjectionArc(it->first, it->second, false));
      fd_shuffle2.push_back(InjectionArc(it->first, it->second, false));
    }

    if (!ShuffleFileDescriptors(&fd_shuffle1))
      _exit(127);

    CloseSuperfluousFds(fd_shuffle2);

    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;

    SetCurrentProcessPrivileges(privs);

    for (environment_map::const_iterator it = env_vars_to_set.begin();
         it != env_vars_to_set.end(); ++it) {
      if (setenv(it->first.c_str(), it->second.c_str(), 1/*overwrite*/))
        _exit(127);
    }
    execv(argv_cstr[0], argv_cstr.get());
    // if we get here, we're in serious trouble and should complain loudly
    DLOG(ERROR) << "FAILED TO exec() CHILD PROCESS, path: " << argv_cstr[0];
    _exit(127);
  } else {
    gProcessLog.print("==> process %d launched child process %d\n",
                      GetCurrentProcId(), pid);
    if (wait)
      HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden,
               ProcessHandle* process_handle) {
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle);
}

void SetCurrentProcessPrivileges(ChildPrivileges privs) {
  if (privs == PRIVILEGES_INHERIT) {
    return;
  }

  gid_t gid = CHILD_UNPRIVILEGED_GID;
  uid_t uid = CHILD_UNPRIVILEGED_UID;
  if (setgid(gid) != 0) {
    DLOG(ERROR) << "FAILED TO setgid() CHILD PROCESS";
    _exit(127);
  }
  if (setuid(uid) != 0) {
    DLOG(ERROR) << "FAILED TO setuid() CHILD PROCESS";
    _exit(127);
  }
  if (chdir("/") != 0)
    gProcessLog.print("==> could not chdir()\n");
}

#endif

}  // namespace base
