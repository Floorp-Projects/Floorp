// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include "base/platform_thread.h"

#include <stdlib.h>

#ifdef OS_WIN
#include <io.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

bool DebugUtil::WaitForDebugger(int wait_seconds, bool silent) {
  for (int i = 0; i < wait_seconds * 10; ++i) {
    if (BeingDebugged()) {
      if (!silent)
        BreakDebugger();
      return true;
    }
    PlatformThread::Sleep(100);
  }
  return false;
}

const void *const *StackTrace::Addresses(size_t* count) {
  *count = trace_.size();
  if (trace_.size())
    return &trace_[0];
  return NULL;
}

namespace mozilla {

EnvironmentLog::EnvironmentLog(const char* varname)
  : fd_(NULL)
{
  const char *e = getenv(varname);
  if (e && *e) {
    if (!strcmp(e, "-")) {
      fd_ = fdopen(dup(STDOUT_FILENO), "a");
    }
    else {
      fd_ = fopen(e, "a");
    }
  }
}

EnvironmentLog::~EnvironmentLog()
{
  if (fd_)
    fclose(fd_);
}

void
EnvironmentLog::print(const char* format, ...)
{
  va_list a;
  va_start(a, format);
  if (fd_) {
    vfprintf(fd_, format, a);
    fflush(fd_);
  }
  va_end(a);
}

} // namespace mozilla
