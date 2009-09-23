// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trace events to track application performance.  Events consist of a name
// a type (BEGIN, END or INSTANT), a tracking id and extra string data.
// In addition, the current process id, thread id, a timestamp down to the
// microsecond and a file and line number of the calling location.
//
// The current implementation logs these events into a log file of the form
// trace_<pid>.log where it's designed to be post-processed to generate a
// trace report.  In the future, it may use another mechansim to facilitate
// real-time analysis.

#ifndef BASE_TRACE_EVENT_H_
#define BASE_TRACE_EVENT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/time.h"
#include "base/timer.h"

// Use the following macros rather than using the TraceLog class directly as the
// underlying implementation may change in the future.  Here's a sample usage:
// TRACE_EVENT_BEGIN("v8.run", documentId, scriptLocation);
// RunScript(script);
// TRACE_EVENT_END("v8.run", documentId, scriptLocation);

// Record that an event (of name, id) has begun.  All BEGIN events should have
// corresponding END events with a matching (name, id).
#define TRACE_EVENT_BEGIN(name, id, extra) \
  Singleton<base::TraceLog>::get()->Trace(name, \
                                          base::TraceLog::EVENT_BEGIN, \
                                          reinterpret_cast<const void*>(id), \
                                          extra, \
                                          __FILE__, \
                                          __LINE__)

// Record that an event (of name, id) has ended.  All END events should have
// corresponding BEGIN events with a matching (name, id).
#define TRACE_EVENT_END(name, id, extra) \
  Singleton<base::TraceLog>::get()->Trace(name, \
                                          base::TraceLog::EVENT_END, \
                                          reinterpret_cast<const void*>(id), \
                                          extra, \
                                          __FILE__, \
                                          __LINE__)

// Record that an event (of name, id) with no duration has happened.
#define TRACE_EVENT_INSTANT(name, id, extra) \
  Singleton<base::TraceLog>::get()->Trace(name, \
                                          base::TraceLog::EVENT_INSTANT, \
                                          reinterpret_cast<const void*>(id), \
                                          extra, \
                                          __FILE__, \
                                          __LINE__)

namespace base {
class ProcessMetrics;
}

namespace base {

class TraceLog {
 public:
  enum EventType {
    EVENT_BEGIN,
    EVENT_END,
    EVENT_INSTANT
  };

  // Is tracing currently enabled.
  static bool IsTracing();
  // Start logging trace events.
  static bool StartTracing();
  // Stop logging trace events.
  static void StopTracing();

  // Log a trace event of (name, type, id) with the optional extra string.
  void Trace(const std::string& name,
             EventType type,
             const void* id,
             const std::wstring& extra,
             const char* file,
             int line);
  void Trace(const std::string& name,
             EventType type,
             const void* id,
             const std::string& extra,
             const char* file,
             int line);

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct DefaultSingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  bool OpenLogFile();
  void CloseLogFile();
  bool Start();
  void Stop();
  void Heartbeat();
  void Log(const std::string& msg);

  bool enabled_;
  FILE* log_file_;
  Lock file_lock_;
  TimeTicks trace_start_time_;
  scoped_ptr<base::ProcessMetrics> process_metrics_;
  RepeatingTimer<TraceLog> timer_;
};

} // namespace base

#endif // BASE_TRACE_EVENT_H_
