// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <spawn.h>
#include <sys/sysctl.h>
#include <sys/types.h>
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

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
  : executable_name_(executable_name),
    index_of_kinfo_proc_(0),
    filter_(filter) {
  // Get a snapshot of all of my processes (yes, as we loop it can go stale, but
  // but trying to find where we were in a constantly changing list is basically
  // impossible.

  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_UID, int(geteuid()) };

  // Since more processes could start between when we get the size and when
  // we get the list, we do a loop to keep trying until we get it.
  bool done = false;
  int try_num = 1;
  const int max_tries = 10;
  do {
    // Get the size of the buffer
    size_t len = 0;
    if (sysctl(mib, arraysize(mib), NULL, &len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to get the size needed for the process list";
      kinfo_procs_.resize(0);
      done = true;
    } else {
      size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
      // Leave some spare room for process table growth (more could show up
      // between when we check and now)
      num_of_kinfo_proc += 4;
      kinfo_procs_.resize(num_of_kinfo_proc);
      len = num_of_kinfo_proc * sizeof(struct kinfo_proc);
      // Load the list of processes
      if (sysctl(mib, arraysize(mib), &kinfo_procs_[0], &len, NULL, 0) < 0) {
        // If we get a mem error, it just means we need a bigger buffer, so
        // loop around again.  Anything else is a real error and give up.
        if (errno != ENOMEM) {
          LOG(ERROR) << "failed to get the process list";
          kinfo_procs_.resize(0);
          done = true;
        }
      } else {
        // Got the list, just make sure we're sized exactly right
        size_t num_of_kinfo_proc = len / sizeof(struct kinfo_proc);
        kinfo_procs_.resize(num_of_kinfo_proc);
        done = true;
      }
    }
  } while (!done && (try_num++ < max_tries));

  if (!done) {
    LOG(ERROR) << "failed to collect the process list in a few tries";
    kinfo_procs_.resize(0);
  }
}

NamedProcessIterator::~NamedProcessIterator() {
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result) {
    return &entry_;
  }

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  std::string executable_name_utf8(WideToUTF8(executable_name_));

  std::string data;
  std::string exec_name;

  for (; index_of_kinfo_proc_ < kinfo_procs_.size(); ++index_of_kinfo_proc_) {
    kinfo_proc* kinfo = &kinfo_procs_[index_of_kinfo_proc_];

    // Skip processes just awaiting collection
    if ((kinfo->kp_proc.p_pid > 0) && (kinfo->kp_proc.p_stat == SZOMB))
      continue;

    int mib[] = { CTL_KERN, KERN_PROCARGS, kinfo->kp_proc.p_pid };

    // Found out what size buffer we need
    size_t data_len = 0;
    if (sysctl(mib, arraysize(mib), NULL, &data_len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to figure out the buffer size for a commandline";
      continue;
    }

    data.resize(data_len);
    if (sysctl(mib, arraysize(mib), &data[0], &data_len, NULL, 0) < 0) {
      LOG(ERROR) << "failed to fetch a commandline";
      continue;
    }

    // Data starts w/ the full path null termed, so we have to extract just the
    // executable name from the path.

    size_t exec_name_end = data.find('\0');
    if (exec_name_end == std::string::npos) {
      LOG(ERROR) << "command line data didn't match expected format";
      continue;
    }
    size_t last_slash = data.rfind('/', exec_name_end);
    if (last_slash == std::string::npos)
      exec_name = data.substr(0, exec_name_end);
    else
      exec_name = data.substr(last_slash + 1, exec_name_end - last_slash - 1);

    // Check the name
    if (executable_name_utf8 == exec_name) {
      entry_.pid = kinfo->kp_proc.p_pid;
      entry_.ppid = kinfo->kp_eproc.e_ppid;
      base::strlcpy(entry_.szExeFile, exec_name.c_str(),
                    sizeof(entry_.szExeFile));
      // Start w/ the next entry next time through
      ++index_of_kinfo_proc_;
      // Done
      return true;
    }
  }
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  // Don't need to check the name, we did that w/in CheckForNextProcess.
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}

}  // namespace base
