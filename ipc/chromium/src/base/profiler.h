// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An abstraction for functions used to control execution time profiling.
// All methods are effectively no-ops unless this program is being run through
// a supported tool (currently only Quantify, a companion tool to Purify)

#ifndef BASE_PROFILER_H__
#define BASE_PROFILER_H__

#include "base/basictypes.h"

namespace base {

class Profiler {
 public:
  // Starts or resumes recording.
  static void StartRecording();

  // Stops recording until StartRecording is called or the program exits.
  static void StopRecording();

  // Throw away data collected so far. This can be useful to call before
  // your first call to StartRecording, for instance to avoid counting any
  // time in application startup.
  static void ClearData();

  // Sets the name of the current thread for display in the profiler's UI.
  static void SetThreadName(const char *name);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Profiler);
};

}  // namespace base

#endif  // BASE_PROFILER_H__
