// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/perf_test_suite.h"

int main(int argc, char** argv) {
  return PerfTestSuite(argc, argv).Run();
}
