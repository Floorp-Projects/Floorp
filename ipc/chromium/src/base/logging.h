/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include <string>
#include <cstring>

#include "base/basictypes.h"
#include "mozilla/Logging.h"

#ifdef NO_CHROMIUM_LOGGING
#include <sstream>
#endif

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
  static mozilla::LazyLogModule gChromiumPRLog;
//  static PRLogModuleInfo* GetLog();

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

#ifdef NO_CHROMIUM_LOGGING
#define CHROMIUM_LOG(info) std::stringstream()
#define LOG_IF(info, condition) if (!(condition)) std::stringstream()
#else
#define CHROMIUM_LOG(info) mozilla::LogWrapper(mozilla::LOG_ ## info, __FILE__, __LINE__)
#define LOG_IF(info, condition) \
  if (!(condition)) mozilla::LogWrapper(mozilla::LOG_ ## info, __FILE__, __LINE__)
#endif


#ifdef DEBUG
#define DLOG(info) CHROMIUM_LOG(info)
#define DLOG_IF(info) LOG_IF(info)
#define DCHECK(condition) CHECK(condition)
#else
#define DLOG(info) mozilla::EmptyLog()
#define DLOG_IF(info, condition) mozilla::EmptyLog()
#define DCHECK(condition) while (false && (condition)) mozilla::EmptyLog()
#endif

#undef LOG_ASSERT
#define LOG_ASSERT(cond) CHECK(0)
#define DLOG_ASSERT(cond) DCHECK(0)

#define NOTREACHED() CHROMIUM_LOG(ERROR)
#define NOTIMPLEMENTED() CHROMIUM_LOG(ERROR)

#undef CHECK
#define CHECK(condition) LOG_IF(FATAL, condition)

#define DCHECK_EQ(v1, v2) DCHECK((v1) == (v2))
#define DCHECK_NE(v1, v2) DCHECK((v1) != (v2))
#define DCHECK_LE(v1, v2) DCHECK((v1) <= (v2))
#define DCHECK_LT(v1, v2) DCHECK((v1) < (v2))
#define DCHECK_GE(v1, v2) DCHECK((v1) >= (v2))
#define DCHECK_GT(v1, v2) DCHECK((v1) > (v2))

#endif  // BASE_LOGGING_H_
