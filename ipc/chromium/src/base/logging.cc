// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "prmem.h"
#include "prprf.h"
#include "base/string_util.h"
#include "nsXPCOM.h"

namespace mozilla {

Logger::~Logger()
{
  PRLogModuleLevel prlevel = PR_LOG_DEBUG;
  int xpcomlevel = -1;

  switch (mSeverity) {
  case LOG_INFO:
    prlevel = PR_LOG_DEBUG;
    xpcomlevel = -1;
    break;

  case LOG_WARNING:
    prlevel = PR_LOG_WARNING;
    xpcomlevel = NS_DEBUG_WARNING;
    break;

  case LOG_ERROR:
    prlevel = PR_LOG_ERROR;
    xpcomlevel = NS_DEBUG_WARNING;
    break;

  case LOG_ERROR_REPORT:
    prlevel = PR_LOG_ERROR;
    xpcomlevel = NS_DEBUG_ASSERTION;
    break;

  case LOG_FATAL:
    prlevel = PR_LOG_ERROR;
    xpcomlevel = NS_DEBUG_ABORT;
    break;
  }

  PR_LOG(GetLog(), prlevel, ("%s:%i: %s", mFile, mLine, mMsg ? mMsg : "<no message>"));
  if (xpcomlevel != -1)
    NS_DebugBreak(xpcomlevel, mMsg, NULL, mFile, mLine);

  PR_Free(mMsg);
}

void
Logger::printf(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  mMsg = PR_vsprintf_append(mMsg, fmt, args);
  va_end(args);
}

PRLogModuleInfo* Logger::gChromiumPRLog;

PRLogModuleInfo* Logger::GetLog()
{
  if (!gChromiumPRLog)
    gChromiumPRLog = PR_NewLogModule("chromium");
  return gChromiumPRLog;
}

} // namespace mozilla 

mozilla::Logger&
operator<<(mozilla::Logger& log, const char* s)
{
  log.printf("%s", s);
  return log;
}

mozilla::Logger&
operator<<(mozilla::Logger& log, const std::string& s)
{
  log.printf("%s", s.c_str());
  return log;
}

mozilla::Logger&
operator<<(mozilla::Logger& log, int i)
{
  log.printf("%i", i);
  return log;
}

mozilla::Logger&
operator<<(mozilla::Logger& log, const std::wstring& s)
{
  log.printf("%s", WideToASCII(s).c_str());
  return log;
}

mozilla::Logger&
operator<<(mozilla::Logger& log, void* p)
{
  log.printf("%p", p);
  return log;
}
