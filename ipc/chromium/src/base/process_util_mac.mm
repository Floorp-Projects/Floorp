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
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/common/mach_ipc_mac.h"

namespace base {

static std::string MachErrorCode(kern_return_t err) {
  return StringPrintf("0x%x %s", err, mach_error_string(err));
}

// Forks the current process and returns the child's |task_t| in the parent
// process.
static pid_t fork_and_get_task(task_t* child_task) {
  const int kTimeoutMs = 100;
  kern_return_t err;

  // Put a random number into the channel name, so that a compromised renderer
  // can't pretend being the child that's forked off.
  std::string mach_connection_name = StringPrintf(
      "org.mozilla.samplingfork.%p.%d",
      child_task, base::RandInt(0, std::numeric_limits<int>::max()));
  ReceivePort parent_recv_port(mach_connection_name.c_str());

  // Error handling philosophy: If Mach IPC fails, don't touch |child_task| but
  // return a valid pid. If IPC fails in the child, the parent will have to wait
  // until kTimeoutMs is over. This is not optimal, but I've never seen it
  // happen, and stuff should still mostly work.
  pid_t pid = fork();
  switch (pid) {
    case -1:
      return pid;
    case 0: {  // child
      ReceivePort child_recv_port;

      MachSendMessage child_message(/* id= */0);
      if (!child_message.AddDescriptor(mach_task_self())) {
        LOG(ERROR) << "child AddDescriptor(mach_task_self()) failed.";
        return pid;
      }
      mach_port_t raw_child_recv_port = child_recv_port.GetPort();
      if (!child_message.AddDescriptor(raw_child_recv_port)) {
        LOG(ERROR) << "child AddDescriptor(" << raw_child_recv_port
                   << ") failed.";
        return pid;
      }

      MachPortSender child_sender(mach_connection_name.c_str());
      err = child_sender.SendMessage(child_message, kTimeoutMs);
      if (err != KERN_SUCCESS) {
        LOG(ERROR) << "child SendMessage() failed: " << MachErrorCode(err);
        return pid;
      }

      MachReceiveMessage parent_message;
      err = child_recv_port.WaitForMessage(&parent_message, kTimeoutMs);
      if (err != KERN_SUCCESS) {
        LOG(ERROR) << "child WaitForMessage() failed: " << MachErrorCode(err);
        return pid;
      }

      if (parent_message.GetTranslatedPort(0) == MACH_PORT_NULL) {
        LOG(ERROR) << "child GetTranslatedPort(0) failed.";
        return pid;
      }
      err = task_set_bootstrap_port(mach_task_self(),
                                    parent_message.GetTranslatedPort(0));
      if (err != KERN_SUCCESS) {
        LOG(ERROR) << "child task_set_bootstrap_port() failed: "
                   << MachErrorCode(err);
        return pid;
      }
      break;
    }
    default: {  // parent
      MachReceiveMessage child_message;
      err = parent_recv_port.WaitForMessage(&child_message, kTimeoutMs);
      if (err != KERN_SUCCESS) {
        LOG(ERROR) << "parent WaitForMessage() failed: " << MachErrorCode(err);
        return pid;
      }

      if (child_message.GetTranslatedPort(0) == MACH_PORT_NULL) {
        LOG(ERROR) << "parent GetTranslatedPort(0) failed.";
        return pid;
      }
      *child_task = child_message.GetTranslatedPort(0);

      if (child_message.GetTranslatedPort(1) == MACH_PORT_NULL) {
        LOG(ERROR) << "parent GetTranslatedPort(1) failed.";
        return pid;
      }
      MachPortSender parent_sender(child_message.GetTranslatedPort(1));

      MachSendMessage parent_message(/* id= */0);
      if (!parent_message.AddDescriptor(bootstrap_port)) {
        LOG(ERROR) << "parent AddDescriptor(" << bootstrap_port << ") failed.";
        return pid;
      }

      err = parent_sender.SendMessage(parent_message, kTimeoutMs);
      if (err != KERN_SUCCESS) {
        LOG(ERROR) << "parent SendMessage() failed: " << MachErrorCode(err);
        return pid;
      }
      break;
    }
  }
  return pid;
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle,
               task_t* process_task) {
  return LaunchApp(argv, fds_to_remap, environment_map(),
                   wait, process_handle, process_task);
}

bool LaunchApp(
    const std::vector<std::string>& argv,
    const file_handle_mapping_vector& fds_to_remap,
    const environment_map& environ,
    bool wait,
    ProcessHandle* process_handle,
    task_t* task_handle) {
  pid_t pid;

  if (task_handle == NULL) {
    pid = fork();
  } else {
    // On OS X, the task_t for a process is needed for several reasons. Sadly,
    // the function task_for_pid() requires privileges a normal user doesn't
    // have. Instead, a short-lived Mach IPC connection is opened between parent
    // and child, and the child sends its task_t to the parent at fork time.
    *task_handle = MACH_PORT_NULL;
    pid = fork_and_get_task(task_handle);
  }

  if (pid < 0)
    return false;

  if (pid == 0) {
    // Child process

    InjectiveMultimap fd_shuffle;
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle.push_back(InjectionArc(it->first, it->second, false));
    }

    for (environment_map::const_iterator it = environ.begin();
         it != environ.end(); ++it) {
      if (it->first.empty())
        continue;

      if (it->second.empty()) {
        unsetenv(it->first.c_str());
      } else {
        setenv(it->first.c_str(), it->second.c_str(), 1);
      }
    }

    // Obscure fork() rule: in the child, if you don't end up doing exec*(),
    // you call _exit() instead of exit(). This is because _exit() does not
    // call any previously-registered (in the parent) exit handlers, which
    // might do things like block waiting for threads that don't even exist
    // in the child.
    if (!ShuffleFileDescriptors(fd_shuffle))
      _exit(127);

    // If we are using the SUID sandbox, it sets a magic environment variable
    // ("SBX_D"), so we remove that variable from the environment here on the
    // off chance that it's already set.
    unsetenv("SBX_D");

    CloseSuperfluousFds(fd_shuffle);

    scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());
    _exit(127);
  } else {
    // Parent process
    if (wait)
      HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  // TODO(playmobil): Do we need to respect the start_hidden flag?
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle, NULL);
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
  : executable_name_(executable_name),
    index_of_kinfo_proc_(0),
    filter_(filter) {
  // Get a snapshot of all of my processes (yes, as we loop it can go stale, but
  // but trying to find where we were in a constantly changing list is basically
  // impossible.

  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_UID, geteuid() };

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

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  // TODO(pinkerton): can we implement this? On linux it relies on /proc.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
