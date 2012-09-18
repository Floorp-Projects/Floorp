// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

namespace base {

static const char* kEventTypeNames[] = {
  "BEGIN",
  "END",
  "INSTANT"
};

static const FilePath::CharType* kLogFileName =
    FILE_PATH_LITERAL("trace_%d.log");

TraceLog::TraceLog() : enabled_(false), log_file_(NULL) {
  base::ProcessHandle proc = base::GetCurrentProcessHandle();
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(proc));
}

TraceLog::~TraceLog() {
  Stop();
}

// static
bool TraceLog::IsTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->enabled_;
}

// static
bool TraceLog::StartTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->Start();
}

bool TraceLog::Start() {
  if (enabled_)
    return true;
  enabled_ = OpenLogFile();
  if (enabled_) {
    Log("var raw_trace_events = [\n");
    trace_start_time_ = TimeTicks::Now();
    timer_.Start(TimeDelta::FromMilliseconds(250), this, &TraceLog::Heartbeat);
  }
  return enabled_;
}

// static
void TraceLog::StopTracing() {
  TraceLog* trace = Singleton<TraceLog>::get();
  return trace->Stop();
}

void TraceLog::Stop() {
  if (enabled_) {
    enabled_ = false;
    Log("];\n");
    CloseLogFile();
    timer_.Stop();
  }
}

void TraceLog::Heartbeat() {
  std::string cpu = StringPrintf("%d", process_metrics_->GetCPUUsage());
  TRACE_EVENT_INSTANT("heartbeat.cpu", 0, cpu);
}

void TraceLog::CloseLogFile() {
  if (log_file_) {
    file_util::CloseFile(log_file_);
  }
}

bool TraceLog::OpenLogFile() {
  FilePath::StringType pid_filename =
      StringPrintf(kLogFileName, base::GetCurrentProcId());
  FilePath log_file_path;
  if (!PathService::Get(base::DIR_EXE, &log_file_path))
    return false;
  log_file_path = log_file_path.Append(pid_filename);
  log_file_ = file_util::OpenFile(log_file_path, "a");
  if (!log_file_) {
    // try the current directory
    log_file_ = file_util::OpenFile(FilePath(pid_filename), "a");
    if (!log_file_) {
      return false;
    }
  }
  return true;
}

void TraceLog::Trace(const std::string& name,
                     EventType type,
                     const void* id,
                     const std::wstring& extra,
                     const char* file,
                     int line) {
  if (!enabled_)
    return;
  Trace(name, type, id, WideToUTF8(extra), file, line);
}

void TraceLog::Trace(const std::string& name,
                     EventType type,
                     const void* id,
                     const std::string& extra,
                     const char* file,
                     int line) {
  if (!enabled_)
    return;

#ifdef USE_UNRELIABLE_NOW
  TimeTicks tick = TimeTicks::HighResNow();
#else
  TimeTicks tick = TimeTicks::Now();
#endif
  TimeDelta delta = tick - trace_start_time_;
  int64_t usec = delta.InMicroseconds();
  std::string msg =
    StringPrintf("{'pid':'0x%lx', 'tid':'0x%lx', 'type':'%s', "
                 "'name':'%s', 'id':'0x%lx', 'extra':'%s', 'file':'%s', "
                 "'line_number':'%d', 'usec_begin': %I64d},\n",
                 base::GetCurrentProcId(),
                 PlatformThread::CurrentId(),
                 kEventTypeNames[type],
                 name.c_str(),
                 id,
                 extra.c_str(),
                 file,
                 line,
                 usec);

  Log(msg);
}

void TraceLog::Log(const std::string& msg) {
  AutoLock lock(file_lock_);

  fprintf(log_file_, "%s", msg.c_str());
}

} // namespace base
