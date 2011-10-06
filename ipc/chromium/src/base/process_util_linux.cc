// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

#ifdef MOZ_MEMORY_ANDROID
extern "C" {
extern void _malloc_prefork(void);
extern void _malloc_postfork(void);
}
#endif

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

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
#ifdef MOZ_MEMORY_ANDROID
  /* We specifically don't call pthread_atfork in jemalloc because it is not
    available in bionic until 2.3. However without it, jemalloc could
    potentially deadlock, when stl allocates memory through jemalloc, after
    fork and before execvp. Therefore, we must manually inform jemalloc here */
  ::_malloc_prefork();
#endif
  pid_t pid = fork();
#ifdef MOZ_MEMORY_ANDROID
  ::_malloc_postfork();
#endif
  if (pid < 0)
    return false;

  if (pid == 0) {
    InjectiveMultimap fd_shuffle;
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle.push_back(InjectionArc(it->first, it->second, false));
    }

    if (!ShuffleFileDescriptors(fd_shuffle))
      exit(127);

    CloseSuperfluousFds(fd_shuffle);

    for (environment_map::const_iterator it = env_vars_to_set.begin();
         it != env_vars_to_set.end(); ++it) {
      if (setenv(it->first.c_str(), it->second.c_str(), 1/*overwrite*/))
        exit(127);
    }

    scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());
    // if we get here, we're in serious trouble and should complain loudly
    DLOG(ERROR) << "FAILED TO exec() CHILD PROCESS, path: " << argv_cstr[0];
    exit(127);
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

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
    : executable_name_(executable_name), filter_(filter) {
  procfs_dir_ = opendir("/proc");
}

NamedProcessIterator::~NamedProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = NULL;
  }
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result)
    return &entry_;

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  dirent* slot = 0;
  const char* openparen;
  const char* closeparen;

  // Arbitrarily guess that there will never be more than 200 non-process
  // files in /proc.  Hardy has 53.
  int skipped = 0;
  const int kSkipLimit = 200;
  while (skipped < kSkipLimit) {
    slot = readdir(procfs_dir_);
    // all done looking through /proc?
    if (!slot)
      return false;

    // If not a process, keep looking for one.
    bool notprocess = false;
    int i;
    for (i = 0; i < NAME_MAX && slot->d_name[i]; ++i) {
       if (!isdigit(slot->d_name[i])) {
         notprocess = true;
         break;
       }
    }
    if (i == NAME_MAX || notprocess) {
      skipped++;
      continue;
    }

    // Read the process's status.
    char buf[NAME_MAX + 12];
    sprintf(buf, "/proc/%s/stat", slot->d_name);
    FILE *fp = fopen(buf, "r");
    if (!fp)
      return false;
    const char* result = fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (!result)
      return false;

    // Parse the status.  It is formatted like this:
    // %d (%s) %c %d ...
    // pid (name) runstate ppid
    // To avoid being fooled by names containing a closing paren, scan
    // backwards.
    openparen = strchr(buf, '(');
    closeparen = strrchr(buf, ')');
    if (!openparen || !closeparen)
      return false;
    char runstate = closeparen[2];

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate != 'Z')
      break;

    // Nope, it's a zombie; somebody isn't cleaning up after their children.
    // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
    // There could be a lot of zombies, can't really decrement i here.
  }
  if (skipped >= kSkipLimit) {
    NOTREACHED();
    return false;
  }

  entry_.pid = atoi(slot->d_name);
  entry_.ppid = atoi(closeparen + 3);

  // TODO(port): read pid's commandline's $0, like killall does.  Using the
  // short name between openparen and closeparen won't work for long names!
  int len = closeparen - openparen - 1;
  if (len > NAME_MAX)
    len = NAME_MAX;
  memcpy(entry_.szExeFile, openparen + 1, len);
  entry_.szExeFile[len] = 0;

  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  // TODO(port): make this also work for non-ASCII filenames
  if (WideToASCII(executable_name_) != entry_.szExeFile)
    return false;
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  std::string proc_io_contents;
  if (!file_util::ReadFileToString(L"/proc/self/io", &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          (*io_counters).ReadOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "syscw") {
          (*io_counters).WriteOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "rchar") {
          (*io_counters).ReadTransferCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "wchar") {
          (*io_counters).WriteTransferCount = StringToInt64(tokenizer.token());
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

}  // namespace base
