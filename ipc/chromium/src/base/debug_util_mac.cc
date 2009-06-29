// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include <signal.h>

#include "base/basictypes.h"

static void ExitSignalHandler(int sig) {
  exit(128 + sig);
}

// static
void DebugUtil::DisableOSCrashDumps() {
  int signals_to_intercept[] ={SIGINT,
                               SIGHUP,
                               SIGTERM,
                               SIGABRT,
                               SIGILL,
                               SIGTRAP,
                               SIGEMT,
                               SIGFPE,
                               SIGBUS,
                               SIGSEGV,
                               SIGSYS,
                               SIGPIPE,
                               SIGXCPU,
                               SIGXFSZ};
  // For all these signals, just wire thing sup so we exit immediately.
  for (size_t i = 0; i < arraysize(signals_to_intercept); ++i) {
    signal(signals_to_intercept[i], ExitSignalHandler);
  }
}
