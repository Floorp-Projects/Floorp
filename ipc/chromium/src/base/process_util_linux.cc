/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 autoindent cindent expandtab: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "nsLiteralString.h"

#ifdef MOZ_B2G_LOADER
#include "ProcessUtils.h"

using namespace mozilla::ipc;
#endif	// MOZ_B2G_LOADER

#ifdef MOZ_WIDGET_GONK
/*
 * AID_APP is the first application UID used by Android. We're using
 * it as our unprivilegied UID.  This ensure the UID used is not
 * shared with any other processes than our own childs.
 */
# include <private/android_filesystem_config.h>
# define CHILD_UNPRIVILEGED_UID AID_APP
# define CHILD_UNPRIVILEGED_GID AID_APP
#else
/*
 * On platforms that are not gonk based, we fall back to an arbitrary
 * UID. This is generally the UID for user `nobody', albeit it is not
 * always the case.
 */
# define CHILD_UNPRIVILEGED_UID 65534
# define CHILD_UNPRIVILEGED_GID 65534
#endif

#ifdef ANDROID
#include <pthread.h>
/*
 * Currently, PR_DuplicateEnvironment is implemented in
 * mozglue/build/BionicGlue.cpp
 */
#define HAVE_PR_DUPLICATE_ENVIRONMENT

#include "plstr.h"
#include "prenv.h"
#include "prmem.h"
/* Temporary until we have PR_DuplicateEnvironment in prenv.h */
extern "C" { NSPR_API(pthread_mutex_t *)PR_GetEnvLock(void); }
#endif

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

#ifdef HAVE_PR_DUPLICATE_ENVIRONMENT
/* 
 * I tried to put PR_DuplicateEnvironment down in mozglue, but on android 
 * this winds up failing because the strdup/free calls wind up mismatching. 
 */

static char **
PR_DuplicateEnvironment(void)
{
    char **result = NULL;
    char **s;
    char **d;
    pthread_mutex_lock(PR_GetEnvLock());
    for (s = environ; *s != NULL; s++)
      ;
    if ((result = (char **)PR_Malloc(sizeof(char *) * (s - environ + 1))) != NULL) {
      for (s = environ, d = result; *s != NULL; s++, d++) {
        *d = PL_strdup(*s);
      }
      *d = NULL;
    }
    pthread_mutex_unlock(PR_GetEnvLock());
    return result;
}

class EnvironmentEnvp
{
public:
  EnvironmentEnvp()
    : mEnvp(PR_DuplicateEnvironment()) {}

  EnvironmentEnvp(const environment_map &em)
  {
    mEnvp = (char **)PR_Malloc(sizeof(char *) * (em.size() + 1));
    if (!mEnvp) {
      return;
    }
    char **e = mEnvp;
    for (environment_map::const_iterator it = em.begin();
         it != em.end(); ++it, ++e) {
      std::string str = it->first;
      str += "=";
      str += it->second;
      *e = PL_strdup(str.c_str());
    }
    *e = NULL;
  }

  ~EnvironmentEnvp()
  {
    if (!mEnvp) {
      return;
    }
    for (char **e = mEnvp; *e; ++e) {
      PL_strfree(*e);
    }
    PR_Free(mEnvp);
  }

  char * const *AsEnvp() { return mEnvp; }

  void ToMap(environment_map &em)
  {
    if (!mEnvp) {
      return;
    }
    em.clear();
    for (char **e = mEnvp; *e; ++e) {
      const char *eq;
      if ((eq = strchr(*e, '=')) != NULL) {
        std::string varname(*e, eq - *e);
        em[varname.c_str()] = &eq[1];
      }
    }
  }

private:
  char **mEnvp;
};

class Environment : public environment_map
{
public:
  Environment()
  {
    EnvironmentEnvp envp;
    envp.ToMap(*this);
  }

  char * const *AsEnvp() {
    mEnvp.reset(new EnvironmentEnvp(*this));
    return mEnvp->AsEnvp();
  }

  void Merge(const environment_map &em)
  {
    for (const_iterator it = em.begin(); it != em.end(); ++it) {
      (*this)[it->first] = it->second;
    }
  }
private:
  std::auto_ptr<EnvironmentEnvp> mEnvp;
};
#endif // HAVE_PR_DUPLICATE_ENVIRONMENT

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

#ifdef MOZ_B2G_LOADER
/**
 * Launch an app using B2g Loader.
 */
static bool
LaunchAppProcLoader(const std::vector<std::string>& argv,
                    const file_handle_mapping_vector& fds_to_remap,
                    const environment_map& env_vars_to_set,
                    ChildPrivileges privs,
                    ProcessHandle* process_handle) {
  size_t i;
  scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
  for (i = 0; i < argv.size(); i++) {
    argv_cstr[i] = const_cast<char*>(argv[i].c_str());
  }
  argv_cstr[argv.size()] = nullptr;

  scoped_array<char*> env_cstr(new char*[env_vars_to_set.size() + 1]);
  i = 0;
  for (environment_map::const_iterator it = env_vars_to_set.begin();
       it != env_vars_to_set.end(); ++it) {
    env_cstr[i++] = strdup((it->first + "=" + it->second).c_str());
  }
  env_cstr[env_vars_to_set.size()] = nullptr;

  bool ok = ProcLoaderLoad((const char **)argv_cstr.get(),
                           (const char **)env_cstr.get(),
                           fds_to_remap, privs,
                           process_handle);
  MOZ_ASSERT(ok, "ProcLoaderLoad() failed");

  for (size_t i = 0; i < env_vars_to_set.size(); i++) {
    free(env_cstr[i]);
  }

  return ok;
}

static bool
IsLaunchingNuwa(const std::vector<std::string>& argv) {
  std::vector<std::string>::const_iterator it;
  for (it = argv.begin(); it != argv.end(); ++it) {
    if (*it == std::string("-nuwa")) {
      return true;
    }
  }
  return false;
}
#endif // MOZ_B2G_LOADER

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               const environment_map& env_vars_to_set,
               ChildPrivileges privs,
               bool wait, ProcessHandle* process_handle,
               ProcessArchitecture arch) {
#ifdef MOZ_B2G_LOADER
  static bool beforeFirstNuwaLaunch = true;
  if (!wait && beforeFirstNuwaLaunch && IsLaunchingNuwa(argv)) {
    beforeFirstNuwaLaunch = false;
    return LaunchAppProcLoader(argv, fds_to_remap, env_vars_to_set,
                               privs, process_handle);
  }
#endif // MOZ_B2G_LOADER

  scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
  // Illegal to allocate memory after fork and before execvp
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  fd_shuffle1.reserve(fds_to_remap.size());
  fd_shuffle2.reserve(fds_to_remap.size());

#ifdef HAVE_PR_DUPLICATE_ENVIRONMENT
  Environment env;
  env.Merge(env_vars_to_set);
  char * const *envp = env.AsEnvp();
  if (!envp) {
    DLOG(ERROR) << "FAILED to duplicate environment for: " << argv_cstr[0];
    return false;
  }
#endif

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

#ifdef HAVE_PR_DUPLICATE_ENVIRONMENT
    execve(argv_cstr[0], argv_cstr.get(), envp);
#else
    for (environment_map::const_iterator it = env_vars_to_set.begin();
         it != env_vars_to_set.end(); ++it) {
      if (setenv(it->first.c_str(), it->second.c_str(), 1/*overwrite*/))
        _exit(127);
    }
    execv(argv_cstr[0], argv_cstr.get());
#endif
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
#ifdef MOZ_WIDGET_GONK
  {
    static bool checked_pix_max, pix_max_ok;
    if (!checked_pix_max) {
      checked_pix_max = true;
      int fd = open("/proc/sys/kernel/pid_max", O_CLOEXEC | O_RDONLY);
      if (fd < 0) {
        DLOG(ERROR) << "Failed to open pid_max";
        _exit(127);
      }
      char buf[PATH_MAX];
      ssize_t len = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      if (len < 0) {
        DLOG(ERROR) << "Failed to read pid_max";
        _exit(127);
      }
      buf[len] = '\0';
      int pid_max = atoi(buf);
      pix_max_ok =
        (pid_max + CHILD_UNPRIVILEGED_UID > CHILD_UNPRIVILEGED_UID);
    }
    if (!pix_max_ok) {
      DLOG(ERROR) << "Can't safely get unique uid/gid";
      _exit(127);
    }
    gid += getpid();
    uid += getpid();
  }
#endif
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

}  // namespace base
