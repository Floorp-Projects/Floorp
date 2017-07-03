/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_util.h"
#include "nsXPCOM.h"
#include "mozilla/Move.h"

namespace mozilla {

Logger::~Logger()
{
  LogLevel prlevel = LogLevel::Debug;
  int xpcomlevel = -1;

  switch (mSeverity) {
  case LOG_INFO:
    prlevel = LogLevel::Debug;
    xpcomlevel = -1;
    break;

  case LOG_WARNING:
    prlevel = LogLevel::Warning;
    xpcomlevel = NS_DEBUG_WARNING;
    break;

  case LOG_ERROR:
    prlevel = LogLevel::Error;
    xpcomlevel = NS_DEBUG_WARNING;
    break;

  case LOG_ERROR_REPORT:
    prlevel = LogLevel::Error;
    xpcomlevel = NS_DEBUG_ASSERTION;
    break;

  case LOG_FATAL:
    prlevel = LogLevel::Error;
    xpcomlevel = NS_DEBUG_ABORT;
    break;
  }

  MOZ_LOG(gChromiumPRLog, prlevel, ("%s:%i: %s", mFile, mLine, mMsg ? mMsg.get() : "<no message>"));
  if (xpcomlevel != -1)
    NS_DebugBreak(xpcomlevel, mMsg.get(), NULL, mFile, mLine);
}

void
Logger::printf(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  mMsg = mozilla::VsmprintfAppend(mozilla::Move(mMsg), fmt, args);
  va_end(args);
}

LazyLogModule Logger::gChromiumPRLog("chromium");

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

} // namespace mozilla
