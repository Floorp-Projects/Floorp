// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include "base/platform_thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef OS_WIN
#include <io.h>
#else
#include <unistd.h>
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
{
  const char *e = getenv(varname);
  if (e && *e)
    fname_ = e;
}

EnvironmentLog::~EnvironmentLog()
{
}

void
EnvironmentLog::print(const char* format, ...)
{
  if (!fname_.size())
    return;

  FILE* f;
  if (fname_.compare("-") == 0)
    f = fdopen(dup(STDOUT_FILENO), "a");
  else
    f = fopen(fname_.c_str(), "a");

  if (!f)
    return;

  va_list a;
  va_start(a, format);
  vfprintf(f, format, a);
  va_end(a);
  fclose(f);
}

} // namespace mozilla
