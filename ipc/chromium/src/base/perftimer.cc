// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/perftimer.h"

#include <stdio.h>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"

static FILE* perf_log_file = NULL;

bool InitPerfLog(const FilePath& log_file) {
  if (perf_log_file) {
    // trying to initialize twice
    NOTREACHED();
    return false;
  }

  perf_log_file = file_util::OpenFile(log_file, "w");
  return perf_log_file != NULL;
}

void FinalizePerfLog() {
  if (!perf_log_file) {
    // trying to cleanup without initializing
    NOTREACHED();
    return;
  }
  file_util::CloseFile(perf_log_file);
}

void LogPerfResult(const char* test_name, double value, const char* units) {
  if (!perf_log_file) {
    NOTREACHED();
    return;
  }

  fprintf(perf_log_file, "%s\t%g\t%s\n", test_name, value, units);
  printf("%s\t%g\t%s\n", test_name, value, units);
}
