// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#ifdef CHROMIUM_MOZILLA_BUILD

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

#else

#if defined(OS_WIN)
#include <windows.h>
typedef HANDLE FileHandle;
typedef HANDLE MutexHandle;
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#elif defined(OS_LINUX)
#include <sys/syscall.h>
#include <time.h>
#endif

#if defined(OS_POSIX)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
typedef pthread_mutex_t* MutexHandle;
#endif

#include <ctime>
#include <iomanip>
#include <cstring>
#include <algorithm>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/lock_impl.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"

namespace logging {

bool g_enable_dcheck = false;

const char* const log_severity_names[LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL" };

int min_log_level = 0;
LogLockingState lock_log_file = LOCK_LOG_FILE;

// The default set here for logging_destination will only be used if
// InitLogging is not called.  On Windows, use a file next to the exe;
// on POSIX platforms, where it may not even be possible to locate the
// executable on disk, use stderr.
#if defined(OS_WIN)
LoggingDestination logging_destination = LOG_ONLY_TO_FILE;
#elif defined(OS_POSIX)
LoggingDestination logging_destination = LOG_ONLY_TO_SYSTEM_DEBUG_LOG;
#endif

const int kMaxFilteredLogLevel = LOG_WARNING;
std::string* log_filter_prefix;

// For LOG_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOG_ERROR;

// Which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
#if defined(OS_WIN)
typedef wchar_t PathChar;
typedef std::wstring PathString;
#else
typedef char PathChar;
typedef std::string PathString;
#endif
PathString* log_file_name = NULL;

// this file is lazily opened and the handle may be NULL
FileHandle log_file = NULL;

// what should be prepended to each message?
bool log_process_id = false;
bool log_thread_id = false;
bool log_timestamp = true;
bool log_tickcount = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog and process termination.
LogAssertHandlerFunction log_assert_handler = NULL;
// An report handler override specified by the client to be called instead of
// the debug message dialog.
LogReportHandlerFunction log_report_handler = NULL;

// The lock is used if log file locking is false. It helps us avoid problems
// with multiple threads writing to the log file at the same time.  Use
// LockImpl directly instead of using Lock, because Lock makes logging calls.
static LockImpl* log_lock = NULL;

// When we don't use a lock, we are using a global mutex. We need to do this
// because LockFileEx is not thread safe.
#if defined(OS_WIN)
MutexHandle log_mutex = NULL;
#elif defined(OS_POSIX)
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Helper functions to wrap platform differences.

int32 CurrentProcessId() {
#if defined(OS_WIN)
  return GetCurrentProcessId();
#elif defined(OS_POSIX)
  return getpid();
#endif
}

int32 CurrentThreadId() {
#if defined(OS_WIN)
  return GetCurrentThreadId();
#elif defined(OS_MACOSX)
  return mach_thread_self();
#elif defined(OS_LINUX)
  return syscall(__NR_gettid);
#endif
}

uint64 TickCount() {
#if defined(OS_WIN)
  return GetTickCount();
#elif defined(OS_MACOSX)
  return mach_absolute_time();
#elif defined(OS_LINUX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64 absolute_micro =
    static_cast<int64>(ts.tv_sec) * 1000000 +
    static_cast<int64>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

void CloseFile(FileHandle log) {
#if defined(OS_WIN)
  CloseHandle(log);
#else
  fclose(log);
#endif
}

void DeleteFilePath(const PathString& log_name) {
#if defined(OS_WIN)
  DeleteFile(log_name.c_str());
#else
  unlink(log_name.c_str());
#endif
}

// Called by logging functions to ensure that debug_file is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. debug_file will be NULL in this case.
bool InitializeLogFileHandle() {
  if (log_file)
    return true;

  if (!log_file_name) {
    // Nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to a default.
#if defined(OS_WIN)
    // On Windows we use the same path as the exe.
    wchar_t module_name[MAX_PATH];
    GetModuleFileName(NULL, module_name, MAX_PATH);
    log_file_name = new std::wstring(module_name);
    std::wstring::size_type last_backslash =
        log_file_name->rfind('\\', log_file_name->size());
    if (last_backslash != std::wstring::npos)
      log_file_name->erase(last_backslash + 1);
    *log_file_name += L"debug.log";
#elif defined(OS_POSIX)
    // On other platforms we just use the current directory.
    log_file_name = new std::string("debug.log");
#endif
  }

  if (logging_destination == LOG_ONLY_TO_FILE ||
      logging_destination == LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG) {
#if defined(OS_WIN)
    log_file = CreateFile(log_file_name->c_str(), GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file == INVALID_HANDLE_VALUE || log_file == NULL) {
      // try the current directory
      log_file = CreateFile(L".\\debug.log", GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if (log_file == INVALID_HANDLE_VALUE || log_file == NULL) {
        log_file = NULL;
        return false;
      }
    }
    SetFilePointer(log_file, 0, 0, FILE_END);
#elif defined(OS_POSIX)
    log_file = fopen(log_file_name->c_str(), "a");
    if (log_file == NULL)
      return false;
#endif
  }

  return true;
}

void InitLogMutex() {
#if defined(OS_WIN)
  if (!log_mutex) {
    // \ is not a legal character in mutex names so we replace \ with /
    std::wstring safe_name(*log_file_name);
    std::replace(safe_name.begin(), safe_name.end(), '\\', '/');
    std::wstring t(L"Global\\");
    t.append(safe_name);
    log_mutex = ::CreateMutex(NULL, FALSE, t.c_str());
  }
#elif defined(OS_POSIX)
  // statically initialized
#endif
}

void InitLogging(const PathChar* new_log_file, LoggingDestination logging_dest,
                 LogLockingState lock_log, OldFileDeletionState delete_old) {
  g_enable_dcheck =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableDCHECK);

  if (log_file) {
    // calling InitLogging twice or after some log call has already opened the
    // default log file will re-initialize to the new options
    CloseFile(log_file);
    log_file = NULL;
  }

  lock_log_file = lock_log;
  logging_destination = logging_dest;

  // ignore file options if logging is disabled or only to system
  if (logging_destination == LOG_NONE ||
      logging_destination == LOG_ONLY_TO_SYSTEM_DEBUG_LOG)
    return;

  if (!log_file_name)
    log_file_name = new PathString();
  *log_file_name = new_log_file;
  if (delete_old == DELETE_OLD_LOG_FILE)
    DeleteFilePath(*log_file_name);

  if (lock_log_file == LOCK_LOG_FILE) {
    InitLogMutex();
  } else if (!log_lock) {
    log_lock = new LockImpl();
  }

  InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  min_log_level = level;
}

int GetMinLogLevel() {
  return min_log_level;
}

void SetLogFilterPrefix(const char* filter)  {
  if (log_filter_prefix) {
    delete log_filter_prefix;
    log_filter_prefix = NULL;
  }

  if (filter)
    log_filter_prefix = new std::string(filter);
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  log_process_id = enable_process_id;
  log_thread_id = enable_thread_id;
  log_timestamp = enable_timestamp;
  log_tickcount = enable_tickcount;
}

void SetLogAssertHandler(LogAssertHandlerFunction handler) {
  log_assert_handler = handler;
}

void SetLogReportHandler(LogReportHandlerFunction handler) {
  log_report_handler = handler;
}

// Displays a message box to the user with the error message in it. For
// Windows programs, it's possible that the message loop is messed up on
// a fatal error, and creating a MessageBox will cause that message loop
// to be run. Instead, we try to spawn another process that displays its
// command line. We look for "Debug Message.exe" in the same directory as
// the application. If it exists, we use it, otherwise, we use a regular
// message box.
void DisplayDebugMessage(const std::string& str) {
  if (str.empty())
    return;

#if defined(OS_WIN)
  // look for the debug dialog program next to our application
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);
  wchar_t* backslash = wcsrchr(prog_name, '\\');
  if (backslash)
    backslash[1] = 0;
  wcscat_s(prog_name, MAX_PATH, L"debug_message.exe");

  std::wstring cmdline = base::SysUTF8ToWide(str);
  if (cmdline.empty())
    return;

  STARTUPINFO startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  PROCESS_INFORMATION process_info;
  if (CreateProcessW(prog_name, &cmdline[0], NULL, NULL, false, 0, NULL,
                     NULL, &startup_info, &process_info)) {
    WaitForSingleObject(process_info.hProcess, INFINITE);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
  } else {
    // debug process broken, let's just do a message box
    MessageBoxW(NULL, &cmdline[0], L"Fatal error",
                MB_OK | MB_ICONHAND | MB_TOPMOST);
  }
#else
  fprintf(stderr, "%s\n", str.c_str());
#endif
}

#if defined(OS_WIN)
LogMessage::SaveLastError::SaveLastError() : last_error_(::GetLastError()) {
}

LogMessage::SaveLastError::~SaveLastError() {
  ::SetLastError(last_error_);
}
#endif  // defined(OS_WIN)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       int ctr)
    : severity_(severity) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const CheckOpString& result)
    : severity_(LOG_FATAL) {
  Init(file, line);
  stream_ << "Check failed: " << (*result.str_);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       const CheckOpString& result)
    : severity_(severity) {
  Init(file, line);
  stream_ << "Check failed: " << (*result.str_);
}

LogMessage::LogMessage(const char* file, int line)
     : severity_(LOG_INFO) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity) {
  Init(file, line);
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  // log only the filename
  const char* last_slash = strrchr(file, '\\');
  if (last_slash)
    file = last_slash + 1;

  // TODO(darin): It might be nice if the columns were fixed width.

  stream_ <<  '[';
  if (log_process_id)
    stream_ << CurrentProcessId() << ':';
  if (log_thread_id)
    stream_ << CurrentThreadId() << ':';
  if (log_timestamp) {
     time_t t = time(NULL);
#if _MSC_VER >= 1400
    struct tm local_time = {0};
    localtime_s(&local_time, &t);
    struct tm* tm_time = &local_time;
#else
    struct tm* tm_time = localtime(&t);
#endif
    stream_ << std::setfill('0')
            << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday
            << '/'
            << std::setw(2) << tm_time->tm_hour
            << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec
            << ':';
  }
  if (log_tickcount)
    stream_ << TickCount() << ':';
  stream_ << log_severity_names[severity_] << ":" << file <<
             "(" << line << ")] ";

  message_start_ = stream_.tellp();
}

LogMessage::~LogMessage() {
  // TODO(brettw) modify the macros so that nothing is executed when the log
  // level is too high.
  if (severity_ < min_log_level)
    return;

  std::string str_newline(stream_.str());
#if defined(OS_WIN)
  str_newline.append("\r\n");
#else
  str_newline.append("\n");
#endif

  if (log_filter_prefix && severity_ <= kMaxFilteredLogLevel &&
      str_newline.compare(message_start_, log_filter_prefix->size(),
                          log_filter_prefix->data()) != 0) {
    return;
  }

  if (logging_destination == LOG_ONLY_TO_SYSTEM_DEBUG_LOG ||
      logging_destination == LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG) {
#if defined(OS_WIN)
    OutputDebugStringA(str_newline.c_str());
    if (severity_ >= kAlwaysPrintErrorLevel)
#endif
    // TODO(erikkay): this interferes with the layout tests since it grabs
    // stderr and stdout and diffs them against known data. Our info and warn
    // logs add noise to that.  Ideally, the layout tests would set the log
    // level to ignore anything below error.  When that happens, we should
    // take this fprintf out of the #else so that Windows users can benefit
    // from the output when running tests from the command-line.  In the
    // meantime, we leave this in for Mac and Linux, but until this is fixed
    // they won't be able to pass any layout tests that have info or warn logs.
    // See http://b/1343647
    fprintf(stderr, "%s", str_newline.c_str());
  } else if (severity_ >= kAlwaysPrintErrorLevel) {
    // When we're only outputting to a log file, above a certain log level, we
    // should still output to stderr so that we can better detect and diagnose
    // problems with unit tests, especially on the buildbots.
    fprintf(stderr, "%s", str_newline.c_str());
  }

  // write to log file
  if (logging_destination != LOG_NONE &&
      logging_destination != LOG_ONLY_TO_SYSTEM_DEBUG_LOG &&
      InitializeLogFileHandle()) {
    // We can have multiple threads and/or processes, so try to prevent them
    // from clobbering each other's writes.
    if (lock_log_file == LOCK_LOG_FILE) {
      // Ensure that the mutex is initialized in case the client app did not
      // call InitLogging. This is not thread safe. See below.
      InitLogMutex();

#if defined(OS_WIN)
      DWORD r = ::WaitForSingleObject(log_mutex, INFINITE);
      DCHECK(r != WAIT_ABANDONED);
#elif defined(OS_POSIX)
      pthread_mutex_lock(&log_mutex);
#endif
    } else {
      // use the lock
      if (!log_lock) {
        // The client app did not call InitLogging, and so the lock has not
        // been created. We do this on demand, but if two threads try to do
        // this at the same time, there will be a race condition to create
        // the lock. This is why InitLogging should be called from the main
        // thread at the beginning of execution.
        log_lock = new LockImpl();
      }
      log_lock->Lock();
    }

#if defined(OS_WIN)
    SetFilePointer(log_file, 0, 0, SEEK_END);
    DWORD num_written;
    WriteFile(log_file,
              static_cast<const void*>(str_newline.c_str()),
              static_cast<DWORD>(str_newline.length()),
              &num_written,
              NULL);
#else
    fprintf(log_file, "%s", str_newline.c_str());
#endif

    if (lock_log_file == LOCK_LOG_FILE) {
#if defined(OS_WIN)
      ReleaseMutex(log_mutex);
#elif defined(OS_POSIX)
      pthread_mutex_unlock(&log_mutex);
#endif
    } else {
      log_lock->Unlock();
    }
  }

  if (severity_ == LOG_FATAL) {
    // display a message or break into the debugger on a fatal error
    if (DebugUtil::BeingDebugged()) {
      DebugUtil::BreakDebugger();
    } else {
#ifndef NDEBUG
      // Dump a stack trace on a fatal.
      StackTrace trace;
      stream_ << "\n";  // Newline to separate from log message.
      trace.OutputToStream(&stream_);
#endif

      if (log_assert_handler) {
        // make a copy of the string for the handler out of paranoia
        log_assert_handler(std::string(stream_.str()));
      } else {
        // Don't use the string with the newline, get a fresh version to send to
        // the debug message process. We also don't display assertions to the
        // user in release mode. The enduser can't do anything with this
        // information, and displaying message boxes when the application is
        // hosed can cause additional problems.
#ifndef NDEBUG
        DisplayDebugMessage(stream_.str());
#endif
        // Crash the process to generate a dump.
        DebugUtil::BreakDebugger();
      }
    }
  } else if (severity_ == LOG_ERROR_REPORT) {
    // We are here only if the user runs with --enable-dcheck in release mode.
    if (log_report_handler) {
      log_report_handler(std::string(stream_.str()));
    } else {
      DisplayDebugMessage(stream_.str());
    }
  }
}

void CloseLogFile() {
  if (!log_file)
    return;

  CloseFile(log_file);
  log_file = NULL;
}

}  // namespace logging

std::ostream& operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << base::SysWideToUTF8(std::wstring(wstr));
}

#endif // CHROMIUM_MOZILLA_BUILD
