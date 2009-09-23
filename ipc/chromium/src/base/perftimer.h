// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PERFTIMER_H_
#define BASE_PERFTIMER_H_

#include <string>
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"

// ----------------------------------------------------------------------
// Initializes and finalizes the perf log. These functions should be
// called at the beginning and end (respectively) of running all the
// performance tests. The init function returns true on success.
// ----------------------------------------------------------------------
bool InitPerfLog(const FilePath& log_path);
void FinalizePerfLog();

// ----------------------------------------------------------------------
// LogPerfResult
//   Writes to the perf result log the given 'value' resulting from the
//   named 'test'. The units are to aid in reading the log by people.
// ----------------------------------------------------------------------
void LogPerfResult(const char* test_name, double value, const char* units);

// ----------------------------------------------------------------------
// PerfTimer
//   A simple wrapper around Now()
// ----------------------------------------------------------------------
class PerfTimer {
 public:
  PerfTimer() {
    begin_ = base::TimeTicks::Now();
  }

  // Returns the time elapsed since object construction
  base::TimeDelta Elapsed() const {
    return base::TimeTicks::Now() - begin_;
  }

 private:
  base::TimeTicks begin_;
};

// ----------------------------------------------------------------------
// PerfTimeLogger
//   Automates calling LogPerfResult for the common case where you want
//   to measure the time that something took. Call Done() when the test
//   is complete if you do extra work after the test or there are stack
//   objects with potentially expensive constructors. Otherwise, this
//   class with automatically log on destruction.
// ----------------------------------------------------------------------
class PerfTimeLogger {
 public:
  explicit PerfTimeLogger(const char* test_name)
      : logged_(false),
        test_name_(test_name) {
  }

  ~PerfTimeLogger() {
    if (!logged_)
      Done();
  }

  void Done() {
    // we use a floating-point millisecond value because it is more
    // intuitive than microseconds and we want more precision than
    // integer milliseconds
    LogPerfResult(test_name_.c_str(), timer_.Elapsed().InMillisecondsF(), "ms");
    logged_ = true;
  }

 private:
  bool logged_;
  std::string test_name_;
  PerfTimer timer_;
};

#endif  // BASE_PERFTIMER_H_
