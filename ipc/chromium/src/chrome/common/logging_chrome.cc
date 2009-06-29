// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <iostream>
#include <fstream>

#include "chrome/common/logging_chrome.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"

// When true, this means that error dialogs should not be shown.
static bool dialogs_are_suppressed_ = false;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
static bool chrome_logging_initialized_ = false;

// Assertion handler for logging errors that occur when dialogs are
// silenced.  To record a new error, pass the log string associated
// with that error in the str parameter.
MSVC_DISABLE_OPTIMIZE();
static void SilentRuntimeAssertHandler(const std::string& str) {
  DebugUtil::BreakDebugger();
}
static void SilentRuntimeReportHandler(const std::string& str) {
}
MSVC_ENABLE_OPTIMIZE();

// Suppresses error/assertion dialogs and enables the logging of
// those errors into silenced_errors_.
static void SuppressDialogs() {
  if (dialogs_are_suppressed_)
    return;

  logging::SetLogAssertHandler(SilentRuntimeAssertHandler);
  logging::SetLogReportHandler(SilentRuntimeReportHandler);

#if defined(OS_WIN)
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
#endif

  dialogs_are_suppressed_ = true;
}

namespace logging {

void InitChromeLogging(const CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_) <<
    "Attempted to initialize logging when it was already initialized.";

  // only use OutputDebugString in debug mode
#ifdef NDEBUG
  bool enable_logging = false;
  const wchar_t *kInvertLoggingSwitch = switches::kEnableLogging;
  const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_ONLY_TO_FILE;
#else
  bool enable_logging = true;
  const wchar_t *kInvertLoggingSwitch = switches::kDisableLogging;
  const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  logging::LoggingDestination log_mode;
  if (enable_logging) {
    log_mode = kDefaultLoggingMode;
  } else {
    log_mode = logging::LOG_NONE;
  }

#if defined(OS_POSIX)
  std::string log_file_name = WideToUTF8(GetLogFileName());
#elif defined(OS_WIN)
  std::wstring log_file_name = GetLogFileName();
#endif

  logging::InitLogging(log_file_name.c_str(),
                       log_mode,
                       logging::LOCK_LOG_FILE,
                       delete_old_log_file);

  // we want process and thread IDs because we have a lot of things running
  logging::SetLogItems(true, true, false, true);

  // We call running in unattended mode "headless", and allow
  // headless mode to be configured either by the Environment
  // Variable or by the Command Line Switch.  This is for
  // automated test purposes.
  if (base::SysInfo::HasEnvVar(env_vars::kHeadless) ||
      command_line.HasSwitch(switches::kNoErrorDialogs))
    SuppressDialogs();

  std::wstring log_filter_prefix =
      command_line.GetSwitchValue(switches::kLogFilterPrefix);
  logging::SetLogFilterPrefix(WideToUTF8(log_filter_prefix).c_str());

  // Use a minimum log level if the command line has one, otherwise set the
  // default to LOG_WARNING.
  std::wstring log_level = command_line.GetSwitchValue(switches::kLoggingLevel);
  int level = 0;
  if (StringToInt(WideToUTF16Hack(log_level), &level)) {
    if ((level >= 0) && (level < LOG_NUM_SEVERITIES))
      logging::SetMinLogLevel(level);
  } else {
    logging::SetMinLogLevel(LOG_WARNING);
  }

  chrome_logging_initialized_ = true;
}

// This is a no-op, but we'll keep it around in case
// we need to do more cleanup in the future.
void CleanupChromeLogging() {
  DCHECK(chrome_logging_initialized_) <<
    "Attempted to clean up logging when it wasn't initialized.";

  CloseLogFile();

  chrome_logging_initialized_ = false;
}

std::wstring GetLogFileName() {
  std::wstring filename = base::SysInfo::GetEnvVar(env_vars::kLogFileName);
  if (filename != L"")
    return filename;

  const std::wstring log_filename(L"chrome_debug.log");
  std::wstring log_path;

  if (PathService::Get(chrome::DIR_LOGS, &log_path)) {
    file_util::AppendToPath(&log_path, log_filename);
    return log_path;
  } else {
    // error with path service, just use some default file somewhere
    return log_filename;
  }
}

bool DialogsAreSuppressed() {
  return dialogs_are_suppressed_;
}

size_t GetFatalAssertions(AssertionList* assertions) {
  // In this function, we don't assume that assertions is non-null, so
  // that if you just want an assertion count, you can pass in NULL.
  if (assertions)
    assertions->clear();
  size_t assertion_count = 0;

  std::ifstream log_file;
#if defined(OS_WIN)
  log_file.open(GetLogFileName().c_str());
#elif defined(OS_POSIX)
  log_file.open(WideToUTF8(GetLogFileName()).c_str());
#endif
  if (!log_file.is_open())
    return 0;

  std::string utf8_line;
  std::wstring wide_line;
  while(!log_file.eof()) {
    getline(log_file, utf8_line);
    if (utf8_line.find(":FATAL:") != std::string::npos) {
      wide_line = UTF8ToWide(utf8_line);
      if (assertions)
        assertions->push_back(wide_line);
      ++assertion_count;
    }
  }
  log_file.close();

  return assertion_count;
}

} // namespace logging
