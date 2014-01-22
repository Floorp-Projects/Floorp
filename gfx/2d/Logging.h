/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LOGGING_H_
#define MOZILLA_GFX_LOGGING_H_

#include <string>
#include <sstream>
#include <stdio.h>

#include "nsDebug.h"
#include "Point.h"
#include "Matrix.h"

#ifdef WIN32
// This file gets included from nsGlobalWindow.cpp, which doesn't like
// having windows.h included in it. Since OutputDebugStringA is the only
// thing we need from windows.h, we just declare it here directly.
// Note: the function's documented signature is
//  WINBASEAPI void WINAPI OutputDebugStringA(LPCSTR lpOutputString)
// but if we don't include windows.h, the macros WINBASEAPI, WINAPI, and 
// LPCSTR are not defined, so we need to replace them with their expansions.
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#endif

#if defined(DEBUG) || defined(PR_LOGGING)
#include <prlog.h>

extern PRLogModuleInfo *GetGFX2DLog();
#endif

namespace mozilla {
namespace gfx {

const int LOG_DEBUG = 1;
const int LOG_WARNING = 2;

#if defined(DEBUG) || defined(PR_LOGGING)

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

extern GFX2D_API int sGfxLogLevel;

static inline void OutputMessage(const std::string &aString, int aLevel) {
#if defined(WIN32) && !defined(PR_LOGGING)
  if (aLevel >= sGfxLogLevel) {
    ::OutputDebugStringA(aString.c_str());
  }
#elif defined(PR_LOGGING) && !(defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID))
  if (PR_LOG_TEST(GetGFX2DLog(), PRLogLevelForLevel(aLevel))) {
    PR_LogPrint(aString.c_str());
  }
#else
  if (aLevel >= sGfxLogLevel) {
    printf_stderr("%s", aString.c_str());
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

  Log &operator <<(char aChar) { mMessage << aChar; return *this; }
  Log &operator <<(const std::string &aLogText) { mMessage << aLogText; return *this; }
  Log &operator <<(const char aStr[]) { mMessage << static_cast<const char*>(aStr); return *this; }
  Log &operator <<(bool aBool) { mMessage << (aBool ? "true" : "false"); return *this; }
  Log &operator <<(int aInt) { mMessage << aInt; return *this; }
  Log &operator <<(unsigned int aInt) { mMessage << aInt; return *this; }
  Log &operator <<(long aLong) { mMessage << aLong; return *this; }
  Log &operator <<(unsigned long aLong) { mMessage << aLong; return *this; }
  Log &operator <<(Float aFloat) { mMessage << aFloat; return *this; }
  Log &operator <<(double aDouble) { mMessage << aDouble; return *this; }
  Log &operator <<(const Point &aPoint)
    { mMessage << "Point(" << aPoint.x << "," << aPoint.y << ")"; return *this; }
  Log &operator <<(const Size &aSize)
    { mMessage << "Size(" << aSize.width << "," << aSize.height << ")"; return *this; }
  Log &operator <<(const IntSize &aSize)
    { mMessage << "IntSize(" << aSize.width << "," << aSize.height << ")"; return *this; }
  Log &operator <<(const Rect &aRect)
    { mMessage << "Rect(" << aRect.x << "," << aRect.y << "," << aRect.width << "," << aRect.height << ")"; return *this; }
  Log &operator<<(const Matrix& aMatrix)
    { mMessage << "Matrix(" << aMatrix._11 << " " << aMatrix._12 << " ; " << aMatrix._21 << " " << aMatrix._22 << " ; " << aMatrix._31 << " " << aMatrix._32 << ")"; return *this; }


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
