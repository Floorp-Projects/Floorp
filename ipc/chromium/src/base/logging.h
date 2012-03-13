// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include <assert.h>
#include <string>
#include <cstring>

#include "base/basictypes.h"
#include "prlog.h"

// Replace the Chromium logging code with NSPR-based logging code and
// some C++ wrappers to emulate std::ostream

namespace mozilla {

enum LogSeverity {
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_ERROR_REPORT,
  LOG_FATAL,
  LOG_0 = LOG_ERROR
};

class Logger
{
public:
  Logger(LogSeverity severity, const char* file, int line)
    : mSeverity(severity)
    , mFile(file)
    , mLine(line)
    , mMsg(NULL)
  { }

  ~Logger();

  // not private so that the operator<< overloads can get to it
  void printf(const char* fmt, ...);

private:
  static PRLogModuleInfo* gChromiumPRLog;
  static PRLogModuleInfo* GetLog();

  LogSeverity mSeverity;
  const char* mFile;
  int mLine;
  char* mMsg;

  DISALLOW_EVIL_CONSTRUCTORS(Logger);
};

class LogWrapper
{
public:
  LogWrapper(LogSeverity severity, const char* file, int line) :
    log(severity, file, line) { }

  operator Logger&() const { return log; }

private:
  mutable Logger log;

  DISALLOW_EVIL_CONSTRUCTORS(LogWrapper);
};

struct EmptyLog
{
};

} // namespace mozilla

mozilla::Logger& operator<<(mozilla::Logger& log, const char* s);
mozilla::Logger& operator<<(mozilla::Logger& log, const std::string& s);
mozilla::Logger& operator<<(mozilla::Logger& log, int i);
mozilla::Logger& operator<<(mozilla::Logger& log, const std::wstring& s);
mozilla::Logger& operator<<(mozilla::Logger& log, void* p);

template<class T>
const mozilla::EmptyLog& operator <<(const mozilla::EmptyLog& log, const T&)
{
  return log;
}

#define LOG(info) mozilla::LogWrapper(mozilla::LOG_ ## info, __FILE__, __LINE__)
#define LOG_IF(info, condition) \
  if (!(condition)) mozilla::LogWrapper(mozilla::LOG_ ## info, __FILE__, __LINE__)

#ifdef DEBUG
#define DLOG(info) LOG(info)
#define DLOG_IF(info) LOG_IF(info)
#define DCHECK(condition) CHECK(condition)
#else
#define DLOG(info) mozilla::EmptyLog()
#define DLOG_IF(info, condition) mozilla::EmptyLog()
#define DCHECK(condition) while (false && (condition)) mozilla::EmptyLog()
#endif

#define LOG_ASSERT(cond) CHECK(0)
#define DLOG_ASSERT(cond) DCHECK(0)

#define NOTREACHED() LOG(ERROR)
#define NOTIMPLEMENTED() LOG(ERROR)

#define CHECK(condition) LOG_IF(FATAL, condition)

#define DCHECK_EQ(v1, v2) DCHECK((v1) == (v2))
#define DCHECK_NE(v1, v2) DCHECK((v1) != (v2))
#define DCHECK_LE(v1, v2) DCHECK((v1) <= (v2))
#define DCHECK_LT(v1, v2) DCHECK((v1) < (v2))
#define DCHECK_GE(v1, v2) DCHECK((v1) >= (v2))
#define DCHECK_GT(v1, v2) DCHECK((v1) > (v2))

#ifdef assert
#undef assert
#endif
#define assert DLOG_ASSERT

#endif  // BASE_LOGGING_H_
