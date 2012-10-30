/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LOGGING_H_
#define MOZILLA_GFX_LOGGING_H_

#include <string>
#include <sstream>
#include <stdio.h>

#include "Point.h"
#include "Matrix.h"

#ifdef WIN32
#include <windows.h>
#endif

#ifdef PR_LOGGING
#include <prlog.h>

extern PRLogModuleInfo *GetGFX2DLog();
#endif

namespace mozilla {
namespace gfx {

const int LOG_DEBUG = 1;
const int LOG_WARNING = 2;

#ifdef PR_LOGGING

inline PRLogModuleLevel PRLogLevelForLevel(int aLevel) {
  switch (aLevel) {
  case LOG_DEBUG:
    return PR_LOG_DEBUG;
  case LOG_WARNING:
    return PR_LOG_WARNING;
  }
  return PR_LOG_DEBUG;
}

#endif

extern int sGfxLogLevel;

static inline void OutputMessage(const std::string &aString, int aLevel) {
#if defined(WIN32) && !defined(PR_LOGGING)
  if (aLevel >= sGfxLogLevel) {
    ::OutputDebugStringA(aString.c_str());
  }
#elif defined(PR_LOGGING)
  if (PR_LOG_TEST(GetGFX2DLog(), PRLogLevelForLevel(aLevel))) {
    PR_LogPrint(aString.c_str());
  }
#else
  if (aLevel >= sGfxLogLevel) {
    printf("%s", aString.c_str());
  }
#endif
}

class NoLog
{
public:
  NoLog() {}
  ~NoLog() {}

  template<typename T>
  NoLog &operator <<(const T &aLogText) { return *this; }
};

template<int L>
class Log
{
public:
  Log() {}
  ~Log() { mMessage << '\n'; WriteLog(mMessage.str()); }

  Log &operator <<(const std::string &aLogText) { mMessage << aLogText; return *this; }
  Log &operator <<(unsigned int aInt) { mMessage << aInt; return *this; }
  Log &operator <<(const Size &aSize)
    { mMessage << "(" << aSize.width << "x" << aSize.height << ")"; return *this; }
  Log &operator <<(const IntSize &aSize)
    { mMessage << "(" << aSize.width << "x" << aSize.height << ")"; return *this; }
  Log &operator<<(const Matrix& aMatrix)
    { mMessage << "[ " << aMatrix._11 << " " << aMatrix._12 << " ; " << aMatrix._21 << " " << aMatrix._22 << " ; " << aMatrix._31 << " " << aMatrix._32 << " ]"; return *this; }


private:

  void WriteLog(const std::string &aString) {
    OutputMessage(aString, L);
  }

  std::stringstream mMessage;
};

typedef Log<LOG_DEBUG> DebugLog;
typedef Log<LOG_WARNING> WarningLog;

#ifdef GFX_LOG_DEBUG
#define gfxDebug DebugLog
#else
#define gfxDebug if (1) ; else NoLog
#endif
#ifdef GFX_LOG_WARNING
#define gfxWarning WarningLog
#else
#define gfxWarning if (1) ; else NoLog
#endif

}
}

#endif /* MOZILLA_GFX_LOGGING_H_ */
