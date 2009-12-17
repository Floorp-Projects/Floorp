// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include <string>
#include <cstring>

#include "base/basictypes.h"

#ifdef CHROMIUM_MOZILLA_BUILD

#include "prlog.h"

// Replace the Chromium logging code with NSPR-based logging code and
// some C++ wrappers to emulate std::ostream

#define ERROR 0

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

#define LOG_ASSERT(cond) CHECK(ERROR)
#define DLOG_ASSERT(cond) DCHECK(ERROR)

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

#else

#include <sstream>

//
// Optional message capabilities
// -----------------------------
// Assertion failed messages and fatal errors are displayed in a dialog box
// before the application exits. However, running this UI creates a message
// loop, which causes application messages to be processed and potentially
// dispatched to existing application windows. Since the application is in a
// bad state when this assertion dialog is displayed, these messages may not
// get processed and hang the dialog, or the application might go crazy.
//
// Therefore, it can be beneficial to display the error dialog in a separate
// process from the main application. When the logging system needs to display
// a fatal error dialog box, it will look for a program called
// "DebugMessage.exe" in the same directory as the application executable. It
// will run this application with the message as the command line, and will
// not include the name of the application as is traditional for easier
// parsing.
//
// The code for DebugMessage.exe is only one line. In WinMain, do:
//   MessageBox(NULL, GetCommandLineW(), L"Fatal Error", 0);
//
// If DebugMessage.exe is not found, the logging code will use a normal
// MessageBox, potentially causing the problems discussed above.


// Instructions
// ------------
//
// Make a bunch of macros for logging.  The way to log things is to stream
// things to LOG(<a particular severity level>).  E.g.,
//
//   LOG(INFO) << "Found " << num_cookies << " cookies";
//
// You can also do conditional logging:
//
//   LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// The above will cause log messages to be output on the 1st, 11th, 21st, ...
// times it is executed.  Note that the special COUNTER value is used to
// identify which repetition is happening.
//
// The CHECK(condition) macro is active in both debug and release builds and
// effectively performs a LOG(FATAL) which terminates the process and
// generates a crashdump unless a debugger is attached.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(INFO) << "Found cookies";
//
//   DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode
// compiles.  LOG_IF and development flags also work well together
// because the code can be compiled away sometimes.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(FATAL, assert fails) << assertion;
//
// We also override the standard 'assert' to use 'DLOG_ASSERT'.
//
// The supported severity levels for macros that allow you to specify one
// are (in increasing order of severity) INFO, WARNING, ERROR, ERROR_REPORT,
// and FATAL.
//
// Very important: logging a message at the FATAL severity level causes
// the program to terminate (after the message is logged).
//
// Note the special severity of ERROR_REPORT only available/relevant in normal
// mode, which displays error dialog without terminating the program. There is
// no error dialog for severity ERROR or below in normal mode.
//
// There is also the special severity of DFATAL, which logs FATAL in
// debug mode, ERROR_REPORT in normal mode.

namespace logging {

// Where to record logging output? A flat file and/or system debug log via
// OutputDebugString. Defaults on Windows to LOG_ONLY_TO_FILE, and on
// POSIX to LOG_ONLY_TO_SYSTEM_DEBUG_LOG (aka stderr).
enum LoggingDestination { LOG_NONE,
                          LOG_ONLY_TO_FILE,
                          LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                          LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG };

// Indicates that the log file should be locked when being written to.
// Often, there is no locking, which is fine for a single threaded program.
// If logging is being done from multiple threads or there can be more than
// one process doing the logging, the file should be locked during writes to
// make each log outut atomic. Other writers will block.
//
// All processes writing to the log file must have their locking set for it to
// work properly. Defaults to DONT_LOCK_LOG_FILE.
enum LogLockingState { LOCK_LOG_FILE, DONT_LOCK_LOG_FILE };

// On startup, should we delete or append to an existing log file (if any)?
// Defaults to APPEND_TO_OLD_LOG_FILE.
enum OldFileDeletionState { DELETE_OLD_LOG_FILE, APPEND_TO_OLD_LOG_FILE };

// Sets the log file name and other global logging state. Calling this function
// is recommended, and is normally done at the beginning of application init.
// If you don't call it, all the flags will be initialized to their default
// values, and there is a race condition that may leak a critical section
// object if two threads try to do the first log at the same time.
// See the definition of the enums above for descriptions and default values.
//
// The default log file is initialized to "debug.log" in the application
// directory. You probably don't want this, especially since the program
// directory may not be writable on an enduser's system.
#if defined(OS_WIN)
void InitLogging(const wchar_t* log_file, LoggingDestination logging_dest,
                 LogLockingState lock_log, OldFileDeletionState delete_old);
#elif defined(OS_POSIX)
// TODO(avi): do we want to do a unification of character types here?
void InitLogging(const char* log_file, LoggingDestination logging_dest,
                 LogLockingState lock_log, OldFileDeletionState delete_old);
#endif

// Sets the log level. Anything at or above this level will be written to the
// log file/displayed to the user (if applicable). Anything below this level
// will be silently ignored. The log level defaults to 0 (everything is logged)
// if this function is not called.
void SetMinLogLevel(int level);

// Gets the current log level.
int GetMinLogLevel();

// Sets the log filter prefix.  Any log message below LOG_ERROR severity that
// doesn't start with this prefix with be silently ignored.  The filter defaults
// to NULL (everything is logged) if this function is not called.  Messages
// with severity of LOG_ERROR or higher will not be filtered.
void SetLogFilterPrefix(const char* filter);

// Sets the common items you want to be prepended to each log message.
// process and thread IDs default to off, the timestamp defaults to on.
// If this function is not called, logging defaults to writing the timestamp
// only.
void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount);

// Sets the Log Assert Handler that will be used to notify of check failures.
// The default handler shows a dialog box and then terminate the process,
// however clients can use this function to override with their own handling
// (e.g. a silent one for Unit Tests)
typedef void (*LogAssertHandlerFunction)(const std::string& str);
void SetLogAssertHandler(LogAssertHandlerFunction handler);
// Sets the Log Report Handler that will be used to notify of check failures
// in non-debug mode. The default handler shows a dialog box and continues
// the execution, however clients can use this function to override with their
// own handling.
typedef void (*LogReportHandlerFunction)(const std::string& str);
void SetLogReportHandler(LogReportHandlerFunction handler);

typedef int LogSeverity;
const LogSeverity LOG_INFO = 0;
const LogSeverity LOG_WARNING = 1;
const LogSeverity LOG_ERROR = 2;
const LogSeverity LOG_ERROR_REPORT = 3;
const LogSeverity LOG_FATAL = 4;
const LogSeverity LOG_NUM_SEVERITIES = 5;

// LOG_DFATAL_LEVEL is LOG_FATAL in debug mode, ERROR_REPORT in normal mode
#ifdef NDEBUG
const LogSeverity LOG_DFATAL_LEVEL = LOG_ERROR_REPORT;
#else
const LogSeverity LOG_DFATAL_LEVEL = LOG_FATAL;
#endif

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_GOOGLE_LOG_INFO \
  logging::LogMessage(__FILE__, __LINE__)
#define COMPACT_GOOGLE_LOG_WARNING \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_WARNING)
#define COMPACT_GOOGLE_LOG_ERROR \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_ERROR)
#define COMPACT_GOOGLE_LOG_ERROR_REPORT \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_ERROR_REPORT)
#define COMPACT_GOOGLE_LOG_FATAL \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_FATAL)
#define COMPACT_GOOGLE_LOG_DFATAL \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_DFATAL_LEVEL)

// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets
// substituted with 0, and it expands to COMPACT_GOOGLE_LOG_0. To allow us
// to keep using this syntax, we define this macro to do the same thing
// as COMPACT_GOOGLE_LOG_ERROR, and also define ERROR the same way that
// the Windows SDK does for consistency.
#define ERROR 0
#define COMPACT_GOOGLE_LOG_0 \
  logging::LogMessage(__FILE__, __LINE__, logging::LOG_ERROR)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token COMPACT_GOOGLE_LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.

#define LOG(severity) COMPACT_GOOGLE_LOG_ ## severity.stream()
#define SYSLOG(severity) LOG(severity)

#define LOG_IF(severity, condition) \
  !(condition) ? (void) 0 : logging::LogMessageVoidify() & LOG(severity)
#define SYSLOG_IF(severity, condition) LOG_IF(severity, condition)

#define LOG_ASSERT(condition)  \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "
#define SYSLOG_ASSERT(condition) \
  SYSLOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.
#define CHECK(condition) \
  LOG_IF(FATAL, !(condition)) << "Check failed: " #condition ". "

// A container for a string pointer which can be evaluated to a bool -
// true iff the pointer is NULL.
struct CheckOpString {
  CheckOpString(std::string* str) : str_(str) { }
  // No destructor: if str_ is non-NULL, we're about to LOG(FATAL),
  // so there's no point in cleaning up str_.
  operator bool() const { return str_ != NULL; }
  std::string* str_;
};

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline.
template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  std::ostringstream ss;
  ss << names << " (" << v1 << " vs. " << v2 << ")";
  std::string* msg = new std::string(ss.str());
  return msg;
}

extern std::string* MakeCheckOpStringIntInt(int v1, int v2, const char* names);

template<int, int>
std::string* MakeCheckOpString(const int& v1,
                               const int& v2,
                               const char* names) {
  return MakeCheckOpStringIntInt(v1, v2, names);
}

// Plus some debug-logging macros that get compiled to nothing for production
//
// DEBUG_MODE is for uses like
//   if (DEBUG_MODE) foo.CheckThatFoo();
// instead of
//   #ifndef NDEBUG
//     foo.CheckThatFoo();
//   #endif

#ifdef OFFICIAL_BUILD
// We want to have optimized code for an official build so we remove DLOGS and
// DCHECK from the executable.

#define DLOG(severity) \
  true ? (void) 0 : logging::LogMessageVoidify() & LOG(severity)

#define DLOG_IF(severity, condition) \
  true ? (void) 0 : logging::LogMessageVoidify() & LOG(severity)

#define DLOG_ASSERT(condition) \
  true ? (void) 0 : LOG_ASSERT(condition)

enum { DEBUG_MODE = 0 };

// This macro can be followed by a sequence of stream parameters in
// non-debug mode. The DCHECK and friends macros use this so that
// the expanded expression DCHECK(foo) << "asdf" is still syntactically
// valid, even though the expression will get optimized away.
// In order to avoid variable unused warnings for code that only uses a
// variable in a CHECK, we make sure to use the macro arguments.
#define NDEBUG_EAT_STREAM_PARAMETERS \
  logging::LogMessage(__FILE__, __LINE__).stream()

#define DCHECK(condition) \
  while (false && (condition)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_EQ(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_NE(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_LE(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_LT(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_GE(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_GT(val1, val2) \
  while (false && (val1) == (val2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STREQ(str1, str2) \
  while (false && (str1) == (str2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRCASEEQ(str1, str2) \
  while (false && (str1) == (str2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRNE(str1, str2) \
  while (false && (str1) == (str2)) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRCASENE(str1, str2) \
  while (false && (str1) == (str2)) NDEBUG_EAT_STREAM_PARAMETERS

#else
#ifndef NDEBUG
// On a regular debug build, we want to have DCHECKS and DLOGS enabled.

#define DLOG(severity) LOG(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)

// debug-only checking.  not executed in NDEBUG mode.
enum { DEBUG_MODE = 1 };
#define DCHECK(condition) \
  LOG_IF(FATAL, !(condition)) << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
#define DCHECK_OP(name, op, val1, val2)  \
  if (logging::CheckOpString _result = \
      logging::Check##name##Impl((val1), (val2), #val1 " " #op " " #val2)) \
    logging::LogMessage(__FILE__, __LINE__, _result).stream()

// Helper functions for string comparisons.
// To avoid bloat, the definitions are in logging.cc.
#define DECLARE_DCHECK_STROP_IMPL(func, expected) \
  std::string* Check##func##expected##Impl(const char* s1, \
                                           const char* s2, \
                                           const char* names);
DECLARE_DCHECK_STROP_IMPL(strcmp, true)
DECLARE_DCHECK_STROP_IMPL(strcmp, false)
DECLARE_DCHECK_STROP_IMPL(_stricmp, true)
DECLARE_DCHECK_STROP_IMPL(_stricmp, false)
#undef DECLARE_DCHECK_STROP_IMPL

// Helper macro for string comparisons.
// Don't use this macro directly in your code, use CHECK_STREQ et al below.
#define DCHECK_STROP(func, op, expected, s1, s2) \
  while (CheckOpString _result = \
      logging::Check##func##expected##Impl((s1), (s2), \
                                           #s1 " " #op " " #s2)) \
    LOG(FATAL) << *_result.str_

// String (char*) equality/inequality checks.
// CASE versions are case-insensitive.
//
// Note that "s1" and "s2" may be temporary strings which are destroyed
// by the compiler at the end of the current "full expression"
// (e.g. DCHECK_STREQ(Foo().c_str(), Bar().c_str())).

#define DCHECK_STREQ(s1, s2) DCHECK_STROP(strcmp, ==, true, s1, s2)
#define DCHECK_STRNE(s1, s2) DCHECK_STROP(strcmp, !=, false, s1, s2)
#define DCHECK_STRCASEEQ(s1, s2) DCHECK_STROP(_stricmp, ==, true, s1, s2)
#define DCHECK_STRCASENE(s1, s2) DCHECK_STROP(_stricmp, !=, false, s1, s2)

#define DCHECK_INDEX(I,A) DCHECK(I < (sizeof(A)/sizeof(A[0])))
#define DCHECK_BOUND(B,A) DCHECK(B <= (sizeof(A)/sizeof(A[0])))

#else  // NDEBUG
// On a regular release build we want to be able to enable DCHECKS through the
// command line.
#define DLOG(severity) \
  true ? (void) 0 : logging::LogMessageVoidify() & LOG(severity)

#define DLOG_IF(severity, condition) \
  true ? (void) 0 : logging::LogMessageVoidify() & LOG(severity)

#define DLOG_ASSERT(condition) \
  true ? (void) 0 : LOG_ASSERT(condition)

enum { DEBUG_MODE = 0 };

// This macro can be followed by a sequence of stream parameters in
// non-debug mode. The DCHECK and friends macros use this so that
// the expanded expression DCHECK(foo) << "asdf" is still syntactically
// valid, even though the expression will get optimized away.
#define NDEBUG_EAT_STREAM_PARAMETERS \
  logging::LogMessage(__FILE__, __LINE__).stream()

// Set to true in InitLogging when we want to enable the dchecks in release.
extern bool g_enable_dcheck;
#define DCHECK(condition) \
    !logging::g_enable_dcheck ? void (0) : \
        LOG_IF(ERROR_REPORT, !(condition)) << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
#define DCHECK_OP(name, op, val1, val2)  \
  if (logging::g_enable_dcheck) \
    if (logging::CheckOpString _result = \
        logging::Check##name##Impl((val1), (val2), #val1 " " #op " " #val2)) \
      logging::LogMessage(__FILE__, __LINE__, logging::LOG_ERROR_REPORT, \
                          _result).stream()

#define DCHECK_STREQ(str1, str2) \
  while (false) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRCASEEQ(str1, str2) \
  while (false) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRNE(str1, str2) \
  while (false) NDEBUG_EAT_STREAM_PARAMETERS

#define DCHECK_STRCASENE(str1, str2) \
  while (false) NDEBUG_EAT_STREAM_PARAMETERS

#endif  // NDEBUG

// Helper functions for DCHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_DCHECK_OP_IMPL(name, op) \
  template <class t1, class t2> \
  inline std::string* Check##name##Impl(const t1& v1, const t2& v2, \
                                        const char* names) { \
    if (v1 op v2) return NULL; \
    else return MakeCheckOpString(v1, v2, names); \
  } \
  inline std::string* Check##name##Impl(int v1, int v2, const char* names) { \
    if (v1 op v2) return NULL; \
    else return MakeCheckOpString(v1, v2, names); \
  }
DEFINE_DCHECK_OP_IMPL(EQ, ==)
DEFINE_DCHECK_OP_IMPL(NE, !=)
DEFINE_DCHECK_OP_IMPL(LE, <=)
DEFINE_DCHECK_OP_IMPL(LT, < )
DEFINE_DCHECK_OP_IMPL(GE, >=)
DEFINE_DCHECK_OP_IMPL(GT, > )
#undef DEFINE_DCHECK_OP_IMPL

// Equality/Inequality checks - compare two values, and log a LOG_FATAL message
// including the two values when the result is not as expected.  The values
// must have operator<<(ostream, ...) defined.
//
// You may append to the error message like so:
//   DCHECK_NE(1, 2) << ": The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   DCHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These may not compile correctly if one of the arguments is a pointer
// and the other is NULL. To work around this, simply static_cast NULL to the
// type of the desired pointer.

#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)

#endif  // OFFICIAL_BUILD

#define NOTREACHED() DCHECK(false)

// Redefine the standard assert to use our nice log files
#undef assert
#define assert(x) DLOG_ASSERT(x)

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class LogMessage {
 public:
  LogMessage(const char* file, int line, LogSeverity severity, int ctr);

  // Two special constructors that generate reduced amounts of code at
  // LOG call sites for common cases.
  //
  // Used for LOG(INFO): Implied are:
  // severity = LOG_INFO, ctr = 0
  //
  // Using this constructor instead of the more complex constructor above
  // saves a couple of bytes per call site.
  LogMessage(const char* file, int line);

  // Used for LOG(severity) where severity != INFO.  Implied
  // are: ctr = 0
  //
  // Using this constructor instead of the more complex constructor above
  // saves a couple of bytes per call site.
  LogMessage(const char* file, int line, LogSeverity severity);

  // A special constructor used for check failures.
  // Implied severity = LOG_FATAL
  LogMessage(const char* file, int line, const CheckOpString& result);

  // A special constructor used for check failures, with the option to
  // specify severity.
  LogMessage(const char* file, int line, LogSeverity severity,
             const CheckOpString& result);

  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  void Init(const char* file, int line);

  LogSeverity severity_;
  std::ostringstream stream_;
  size_t message_start_;  // Offset of the start of the message (past prefix
                          // info).
#if defined(OS_WIN)
  // Stores the current value of GetLastError in the constructor and restores
  // it in the destructor by calling SetLastError.
  // This is useful since the LogMessage class uses a lot of Win32 calls
  // that will lose the value of GLE and the code that called the log function
  // will have lost the thread error value when the log call returns.
  class SaveLastError {
   public:
    SaveLastError();
    ~SaveLastError();

    unsigned long get_error() const { return last_error_; }

   protected:
    unsigned long last_error_;
  };

  SaveLastError last_error_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

// A non-macro interface to the log facility; (useful
// when the logging level is not a compile-time constant).
inline void LogAtLevel(int const log_level, std::string const &msg) {
  LogMessage(__FILE__, __LINE__, log_level).stream() << msg;
}

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging
//       statements, there's no guarantee that it will stay closed
//       after this call.
void CloseLogFile();

}  // namespace logging

// These functions are provided as a convenience for logging, which is where we
// use streams (it is against Google style to use streams in other places). It
// is designed to allow you to emit non-ASCII Unicode strings to the log file,
// which is normally ASCII. It is relatively slow, so try not to use it for
// common cases. Non-ASCII characters will be converted to UTF-8 by these
// operators.
std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
inline std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << wstr.c_str();
}

// The NOTIMPLEMENTED() macro annotates codepaths which have
// not been implemented yet.
//
// The implementation of this macro is controlled by NOTIMPLEMENTED_POLICY:
//   0 -- Do nothing (stripped by compiler)
//   1 -- Warn at compile time
//   2 -- Fail at compile time
//   3 -- Fail at runtime (DCHECK)
//   4 -- [default] LOG(ERROR) at runtime
//   5 -- LOG(ERROR) at runtime, only once per call-site

#ifndef NOTIMPLEMENTED_POLICY
// Select default policy: LOG(ERROR)
#define NOTIMPLEMENTED_POLICY 4
#endif

#if defined(COMPILER_GCC)
// On Linux, with GCC, we can use __PRETTY_FUNCTION__ to get the demangled name
// of the current function in the NOTIMPLEMENTED message.
#define NOTIMPLEMENTED_MSG "Not implemented reached in " << __PRETTY_FUNCTION__
#else
#define NOTIMPLEMENTED_MSG "NOT IMPLEMENTED"
#endif

#if NOTIMPLEMENTED_POLICY == 0
#define NOTIMPLEMENTED() ;
#elif NOTIMPLEMENTED_POLICY == 1
// TODO, figure out how to generate a warning
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 2
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 3
#define NOTIMPLEMENTED() NOTREACHED()
#elif NOTIMPLEMENTED_POLICY == 4
#define NOTIMPLEMENTED() LOG(ERROR) << NOTIMPLEMENTED_MSG
#elif NOTIMPLEMENTED_POLICY == 5
#define NOTIMPLEMENTED() do {\
  static int count = 0;\
  LOG_IF(ERROR, 0 == count++) << NOTIMPLEMENTED_MSG;\
} while(0)
#endif

#endif // CHROMIUM_MOZILLA_BUILD

#endif  // BASE_LOGGING_H_
