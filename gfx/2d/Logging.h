/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LOGGING_H_
#define MOZILLA_GFX_LOGGING_H_

#include <string>
#include <sstream>
#include <stdio.h>
#include <vector>

#ifdef MOZ_LOGGING
#include "mozilla/Logging.h"
#endif

#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
#include "nsDebug.h"
#endif
#include "Point.h"
#include "BaseRect.h"
#include "Matrix.h"

extern GFX2D_API PRLogModuleInfo *GetGFX2DLog();

namespace mozilla {
namespace gfx {

// Attempting to be consistent with prlog values, but that isn't critical
// (and note that 5 has a special meaning - see the description
// with sGfxLogLevel)
const int LOG_CRITICAL = 1;
const int LOG_WARNING = 2;
const int LOG_DEBUG = 3;
const int LOG_DEBUG_PRLOG = 4;
const int LOG_EVERYTHING = 5; // This needs to be the highest value

#if defined(DEBUG)
const int LOG_DEFAULT = LOG_EVERYTHING;
#else
const int LOG_DEFAULT = LOG_CRITICAL;
#endif

inline mozilla::LogLevel PRLogLevelForLevel(int aLevel) {
  switch (aLevel) {
  case LOG_CRITICAL:
    return LogLevel::Error;
  case LOG_WARNING:
    return LogLevel::Warning;
  case LOG_DEBUG:
    return LogLevel::Debug;
  case LOG_DEBUG_PRLOG:
    return LogLevel::Debug;
  case LOG_EVERYTHING:
    return LogLevel::Error;
  }
  return LogLevel::Debug;
}

class PreferenceAccess
{
public:
  virtual ~PreferenceAccess();

  // This should connect the variable aVar to be updated whenever a preference
  // aName is modified.  aDefault would be used if the preference is undefined,
  // so that we always get the valid value for aVar.
  virtual void LivePref(const char* aName, int32_t* aVar, int32_t aDefault);

public:
  static void SetAccess(PreferenceAccess* aAccess);

public:
  // For each preference that needs to be accessed in Moz2D, add a variable
  // to hold it, as well as the call to LivePref in the RegisterAll() method
  // below.

  // Used to choose the level of logging we get.  The higher the number,
  // the more logging we get.  Value of zero will give you no logging,
  // 1 just errors, 2 adds warnings and 3 adds logging/debug.  4 is used to
  // selectively enable logging on the configurations that
  // support prlog (on other systems, 3 and 4 are the same.)  For prlog,
  // in addition to setting the value to 4, you will need to set an
  // environment variable NSPR_LOG_MODULES to gfx:4. See prlog.h for details.
  static int32_t sGfxLogLevel;

private:
  static void RegisterAll() {
    // The default values (last parameter) should match the initialization
    // values in Factory.cpp, otherwise the standalone Moz2D will get different
    // defaults.
    sAccess->LivePref("gfx.logging.level", &sGfxLogLevel, LOG_DEFAULT);
  }
  static PreferenceAccess* sAccess;
};

/// Graphics logging is available in both debug and release builds and is
/// controlled with a gfx.logging.level preference. If not set, the default
/// for the preference is 5 in the debug builds, 1 in the release builds.
///
/// gfxDebug only works in the debug builds, and is used for information
/// level messages, helping with debugging.  In addition to only working
/// in the debug builds, the value of the above preference of 3 or higher
/// is required.
///
/// gfxWarning messages are available in both debug and release builds,
/// on by default in the debug builds, and off by default in the release builds.
/// Setting the preference gfx.logging.level to a value of 2 or higher will
/// show the warnings.
///
/// gfxCriticalError is available in debug and release builds by default.
/// It is only unavailable if gfx.logging.level is set to 0 (or less.)
/// It outputs the message to stderr or equivalent, like gfxWarning.
/// In the event of a crash, the crash report is annotated with first and
/// the last few of these errors, under the key GraphicsCriticalError.
/// The total number of errors stored in the crash report is controlled
/// by preference gfx.logging.crash.length (default is six, so by default,
/// the first as well as the last five would show up in the crash log.)
///
/// On platforms that support PR_LOGGING, the story is slightly more involved.
/// In that case, unless gfx.logging.level is set to 4 or higher, the output
/// is further controlled by "gfx2d" PR logging module.  However, in the case
/// where such module would disable the output, in all but gfxDebug cases,
/// we will still send a printf.
struct BasicLogger
{
  // For efficiency, this method exists and copies the logic of the
  // OutputMessage below.  If making any changes here, also make it
  // in the appropriate places in that method.
  static bool ShouldOutputMessage(int aLevel) {
    if (PreferenceAccess::sGfxLogLevel >= aLevel) {
#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
      return true;
#else
      if (MOZ_LOG_TEST(GetGFX2DLog(), PRLogLevelForLevel(aLevel))) {
        return true;
      } else if ((PreferenceAccess::sGfxLogLevel >= LOG_DEBUG_PRLOG) ||
                 (aLevel < LOG_DEBUG)) {
        return true;
      }
#endif
    }
    return false;
  }

  static void OutputMessage(const std::string &aString,
                            int aLevel,
                            bool aNoNewline) {
    // This behavior (the higher the preference, the more we log)
    // is consistent with what prlog does in general.  Note that if prlog
    // is in the build, but disabled, we will printf if the preferences
    // requires us to log something (see sGfxLogLevel for the special
    // treatment of LOG_DEBUG and LOG_DEBUG_PRLOG)
    //
    // If making any logic changes to this method, you should probably
    // make the corresponding change in the ShouldOutputMessage method
    // above.
    if (PreferenceAccess::sGfxLogLevel >= aLevel) {
#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
      printf_stderr("%s%s", aString.c_str(), aNoNewline ? "" : "\n");
#else
      if (MOZ_LOG_TEST(GetGFX2DLog(), PRLogLevelForLevel(aLevel))) {
        PR_LogPrint("%s%s", aString.c_str(), aNoNewline ? "" : "\n");
      } else if ((PreferenceAccess::sGfxLogLevel >= LOG_DEBUG_PRLOG) ||
                 (aLevel < LOG_DEBUG)) {
        printf("%s%s", aString.c_str(), aNoNewline ? "" : "\n");
      }
#endif
    }
  }
};

struct CriticalLogger {
  static void OutputMessage(const std::string &aString, int aLevel, bool aNoNewline);
};

// Implement this interface and init the Factory with an instance to
// forward critical logs.
class LogForwarder {
public:
  virtual ~LogForwarder() {}
  virtual void Log(const std::string &aString) = 0;

  // Provide a copy of the logs to the caller.  The int is the index
  // of the Log call, if the number of logs exceeds some preset capacity
  // we may not get all of them, so the indices help figure out which
  // ones we did save.
  virtual std::vector<std::pair<int32_t,std::string> > StringsVectorCopy() = 0;
};

class NoLog
{
public:
  NoLog() {}
  ~NoLog() {}

  // No-op
  MOZ_IMPLICIT NoLog(const NoLog&) {}

  template<typename T>
  NoLog &operator <<(const T &aLogText) { return *this; }
};

enum class LogOptions : int {
  NoNewline = 0x01,
  AutoPrefix = 0x02,
  AssertOnCall = 0x04
};

template<typename T>
struct Hexa {
  explicit Hexa(T aVal) : mVal(aVal) {}
  T mVal;
};
template<typename T>
Hexa<T> hexa(T val) { return Hexa<T>(val); }

template<int L, typename Logger = BasicLogger>
class Log
{
public:
  // The default is to have the prefix, have the new line, and for critical
  // logs assert on each call.
  static int DefaultOptions(bool aWithAssert = true) {
    return (int(LogOptions::AutoPrefix) |
            (aWithAssert ? int(LogOptions::AssertOnCall) : 0));
  }

  // Note that we're calling BasicLogger::ShouldOutputMessage, rather than
  // Logger::ShouldOutputMessage.  Since we currently don't have a different
  // version of that method for different loggers, this is OK. Once we do,
  // change BasicLogger::ShouldOutputMessage to Logger::ShouldOutputMessage.
  explicit Log(int aOptions = Log::DefaultOptions(L == LOG_CRITICAL)) {
    Init(aOptions, BasicLogger::ShouldOutputMessage(L));
  }

  ~Log() {
    Flush();
  }

  void Flush() {
    if (MOZ_LIKELY(!LogIt())) return;

    std::string str = mMessage.str();
    if (!str.empty()) {
      WriteLog(str);
    }
    mMessage.str("");
  }

  Log &operator <<(char aChar) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aChar;
    }
    return *this;
  }
  Log &operator <<(const std::string &aLogText) { 
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aLogText;
    }
    return *this;
  }
  Log &operator <<(const char aStr[]) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << static_cast<const char*>(aStr);
    }
    return *this;
  }
  Log &operator <<(bool aBool) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << (aBool ? "true" : "false");
    }
    return *this;
  }
  Log &operator <<(int aInt) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aInt;
    }
    return *this;
  }
  Log &operator <<(unsigned int aInt) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aInt;
    }
    return *this;
  }
  Log &operator <<(long aLong) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aLong;
    }
    return *this;
  }
  Log &operator <<(unsigned long aLong) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aLong;
    }
    return *this;
  }
  Log &operator <<(long long aLong) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aLong;
    }
    return *this;
  }
  Log &operator <<(unsigned long long aLong) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aLong;
    }
    return *this;
  }
  Log &operator <<(Float aFloat) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aFloat;
    }
    return *this;
  }
  Log &operator <<(double aDouble) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << aDouble;
    }
    return *this;
  }
  template <typename T, typename Sub, typename Coord>
  Log &operator <<(const BasePoint<T, Sub, Coord>& aPoint) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << "Point" << aPoint;
    }
    return *this;
  }
  template <typename T, typename Sub>
  Log &operator <<(const BaseSize<T, Sub>& aSize) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << "Size(" << aSize.width << "," << aSize.height << ")";
    }
    return *this;
  }
  template <typename T, typename Sub, typename Point, typename SizeT, typename Margin>
  Log &operator <<(const BaseRect<T, Sub, Point, SizeT, Margin>& aRect) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << "Rect" << aRect;
    }
    return *this;
  }
  Log &operator<<(const Matrix& aMatrix) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << "Matrix(" << aMatrix._11 << " " << aMatrix._12 << " ; " << aMatrix._21 << " " << aMatrix._22 << " ; " << aMatrix._31 << " " << aMatrix._32 << ")";
    }
    return *this;
  }
  template<typename T>
  Log &operator<<(Hexa<T> aHex) {
    if (MOZ_UNLIKELY(LogIt())) {
      mMessage << "0x" << std::hex << aHex.mVal << std::dec;
    }
    return *this;
  }

  Log& operator<<(SurfaceFormat aFormat) {
    if (MOZ_UNLIKELY(LogIt())) {
      switch(aFormat) {
        case SurfaceFormat::B8G8R8A8:
          mMessage << "SurfaceFormat::B8G8R8A8";
          break;
        case SurfaceFormat::B8G8R8X8:
          mMessage << "SurfaceFormat::B8G8R8X8";
          break;
        case SurfaceFormat::R8G8B8A8:
          mMessage << "SurfaceFormat::R8G8B8A8";
          break;
        case SurfaceFormat::R8G8B8X8:
          mMessage << "SurfaceFormat::R8G8B8X8";
          break;
        case SurfaceFormat::R5G6B5:
          mMessage << "SurfaceFormat::R5G6B5";
          break;
        case SurfaceFormat::A8:
          mMessage << "SurfaceFormat::A8";
          break;
        case SurfaceFormat::YUV:
          mMessage << "SurfaceFormat::YUV";
          break;
        case SurfaceFormat::UNKNOWN:
          mMessage << "SurfaceFormat::UNKNOWN";
          break;
        default:
          mMessage << "Invalid SurfaceFormat (" << (int)aFormat << ")";
          break;
      }
    }
    return *this;
  }

  Log& operator<<(SurfaceType aType) {
    if (MOZ_UNLIKELY(LogIt())) {
      switch(aType) {
        case SurfaceType::DATA:
          mMessage << "SurfaceType::DATA";
          break;
        case SurfaceType::D2D1_BITMAP:
          mMessage << "SurfaceType::D2D1_BITMAP";
          break;
        case SurfaceType::D2D1_DRAWTARGET:
          mMessage << "SurfaceType::D2D1_DRAWTARGET";
          break;
        case SurfaceType::CAIRO:
          mMessage << "SurfaceType::CAIRO";
          break;
        case SurfaceType::CAIRO_IMAGE:
          mMessage << "SurfaceType::CAIRO_IMAGE";
          break;
        case SurfaceType::COREGRAPHICS_IMAGE:
          mMessage << "SurfaceType::COREGRAPHICS_IMAGE";
          break;
        case SurfaceType::COREGRAPHICS_CGCONTEXT:
          mMessage << "SurfaceType::COREGRAPHICS_CGCONTEXT";
          break;
        case SurfaceType::SKIA:
          mMessage << "SurfaceType::SKIA";
          break;
        case SurfaceType::DUAL_DT:
          mMessage << "SurfaceType::DUAL_DT";
          break;
        case SurfaceType::D2D1_1_IMAGE:
          mMessage << "SurfaceType::D2D1_1_IMAGE";
          break;
        case SurfaceType::RECORDING:
          mMessage << "SurfaceType::RECORDING";
          break;
        case SurfaceType::TILED:
          mMessage << "SurfaceType::TILED";
          break;
        default:
          mMessage << "Invalid SurfaceType (" << (int)aType << ")";
          break;
      }
    }
    return *this;
  }

  inline bool LogIt() const { return mLogIt; }
  inline bool NoNewline() const { return mOptions & int(LogOptions::NoNewline); }
  inline bool AutoPrefix() const { return mOptions & int(LogOptions::AutoPrefix); }

  // We do not want this version to do any work, and stringstream can't be
  // copied anyway.  It does come in handy for the "Once" macro defined below.
  MOZ_IMPLICIT Log(const Log& log) { Init(log.mOptions, false); }

private:
  // Initialization common to two constructors
  void Init(int aOptions, bool aLogIt) {
    mOptions = aOptions;
    mLogIt = aLogIt;
    if (mLogIt && AutoPrefix()) {
      if (mOptions & int(LogOptions::AssertOnCall)) {
        mMessage << "[GFX" << L << "]: ";
      } else {
        mMessage << "[GFX" << L << "-]: ";
      }
    }
  }

  void WriteLog(const std::string &aString) {
    if (MOZ_UNLIKELY(LogIt())) {
      Logger::OutputMessage(aString, L, NoNewline());
      if (mOptions & int(LogOptions::AssertOnCall)) {
        MOZ_ASSERT(false, "An assert from the graphics logger");
      }
    }
  }

  std::stringstream mMessage;
  int mOptions;
  bool mLogIt;
};

typedef Log<LOG_DEBUG> DebugLog;
typedef Log<LOG_WARNING> WarningLog;
typedef Log<LOG_CRITICAL, CriticalLogger> CriticalLog;

// Macro to glue names to get us less chance of name clashing.
#if defined GFX_LOGGING_GLUE1 || defined GFX_LOGGING_GLUE
#error "Clash of the macro GFX_LOGGING_GLUE1 or GFX_LOGGING_GLUE"
#endif
#define GFX_LOGGING_GLUE1(x, y)  x##y
#define GFX_LOGGING_GLUE(x, y)   GFX_LOGGING_GLUE1(x, y)

// This log goes into crash reports, use with care.
#define gfxCriticalError mozilla::gfx::CriticalLog
#define gfxCriticalErrorOnce static gfxCriticalError GFX_LOGGING_GLUE(sOnceAtLine,__LINE__) = gfxCriticalError

// This is a shortcut for errors we want logged in crash reports/about support
// but we do not want asserting.  These are available in all builds, so it is
// not worth trying to do magic to avoid matching the syntax of gfxCriticalError.
// So, this one is used as
// gfxCriticalNote << "Something to report and not assert";
// while the critical error is
// gfxCriticalError() << "Something to report and assert";
#define gfxCriticalNote gfxCriticalError(gfxCriticalError::DefaultOptions(false))

// The "once" versions will only trigger the first time through. You can do this:
// gfxCriticalErrorOnce() << "This message only shows up once;
// instead of the usual:
// static bool firstTime = true;
// if (firstTime) {
//   firstTime = false;
//   gfxCriticalError() << "This message only shows up once;
// }
#ifdef GFX_LOG_DEBUG
#define gfxDebug mozilla::gfx::DebugLog
#define gfxDebugOnce static gfxDebug GFX_LOGGING_GLUE(sOnceAtLine,__LINE__) = gfxDebug
#else
#define gfxDebug if (1) ; else mozilla::gfx::NoLog
#define gfxDebugOnce if (1) ; else mozilla::gfx::NoLog
#endif
#ifdef GFX_LOG_WARNING
#define gfxWarning mozilla::gfx::WarningLog
#define gfxWarningOnce static gfxWarning GFX_LOGGING_GLUE(sOnceAtLine,__LINE__) = gfxWarning
#else
#define gfxWarning if (1) ; else mozilla::gfx::NoLog
#define gfxWarningOnce if (1) ; else mozilla::gfx::NoLog
#endif

// See nsDebug.h and the NS_WARN_IF macro

#ifdef __cplusplus
 // For now, have MOZ2D_ERROR_IF available in debug and non-debug builds
inline bool MOZ2D_error_if_impl(bool aCondition, const char* aExpr,
                                const char* aFile, int32_t aLine)
{
  if (MOZ_UNLIKELY(aCondition)) {
    gfxCriticalError() << aExpr << " at " << aFile << ":" << aLine;
  }
  return aCondition;
}
#define MOZ2D_ERROR_IF(condition) \
  MOZ2D_error_if_impl(condition, #condition, __FILE__, __LINE__)

#ifdef DEBUG
inline bool MOZ2D_warn_if_impl(bool aCondition, const char* aExpr,
                               const char* aFile, int32_t aLine)
{
  if (MOZ_UNLIKELY(aCondition)) {
    gfxWarning() << aExpr << " at " << aFile << ":" << aLine;
  }
  return aCondition;
}
#define MOZ2D_WARN_IF(condition) \
  MOZ2D_warn_if_impl(condition, #condition, __FILE__, __LINE__)
#else
#define MOZ2D_WARN_IF(condition) (bool)(condition)
#endif
#endif

const int INDENT_PER_LEVEL = 2;

class TreeLog
{
public:
  explicit TreeLog(const std::string& aPrefix = "")
        : mLog(int(LogOptions::NoNewline)),
          mPrefix(aPrefix),
          mDepth(0),
          mStartOfLine(true),
          mConditionedOnPref(false),
          mPrefFunction(nullptr) {}

  template <typename T>
  TreeLog& operator<<(const T& aObject) {
    if (mConditionedOnPref && !mPrefFunction()) {
      return *this;
    }
    if (mStartOfLine) {
      mLog << '[' << mPrefix << "] " << std::string(mDepth * INDENT_PER_LEVEL, ' ');
      mStartOfLine = false;
    }
    mLog << aObject;
    if (EndsInNewline(aObject)) {
      // Don't indent right here as the user may change the indent
      // between now and the first output to the next line.
      mLog.Flush();
      mStartOfLine = true;
    }
    return *this;
  }

  void IncreaseIndent() { ++mDepth; }
  void DecreaseIndent() { --mDepth; }

  void ConditionOnPrefFunction(bool(*aPrefFunction)()) {
    mConditionedOnPref = true;
    mPrefFunction = aPrefFunction;
  }
private:
  Log<LOG_DEBUG> mLog;
  std::string mPrefix;
  uint32_t mDepth;
  bool mStartOfLine;
  bool mConditionedOnPref;
  bool (*mPrefFunction)();

  template <typename T>
  static bool EndsInNewline(const T& aObject) {
    return false;
  }

  static bool EndsInNewline(const std::string& aString) {
    return !aString.empty() && aString[aString.length() - 1] == '\n';
  }

  static bool EndsInNewline(char aChar) {
    return aChar == '\n';
  }

  static bool EndsInNewline(const char* aString) {
    return EndsInNewline(std::string(aString));
  }
};

class TreeAutoIndent
{
public:
  explicit TreeAutoIndent(TreeLog& aTreeLog) : mTreeLog(aTreeLog) {
    mTreeLog.IncreaseIndent();
  }
  ~TreeAutoIndent() {
    mTreeLog.DecreaseIndent();
  }
private:
  TreeLog& mTreeLog;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_LOGGING_H_ */
