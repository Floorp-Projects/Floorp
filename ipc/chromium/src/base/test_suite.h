// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SUITE_H_
#define BASE_TEST_SUITE_H_

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/file_path.h"
#include "base/icu_util.h"
#include "base/logging.h"
#include "base/multiprocess_test.h"
#include "base/scoped_nsautorelease_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

class TestSuite {
 public:
  TestSuite(int argc, char** argv) {
    base::EnableTerminationOnHeapCorruption();
    CommandLine::Init(argc, argv);
    testing::InitGoogleTest(&argc, argv);
#if defined(OS_LINUX)
    gtk_init_check(&argc, &argv);
#endif
    // Don't add additional code to this constructor.  Instead add it to
    // Initialize().  See bug 6436.
  }

  virtual ~TestSuite() {
    CommandLine::Terminate();
  }

  // Don't add additional code to this method.  Instead add it to
  // Initialize().  See bug 6436.
  int Run() {
    base::ScopedNSAutoreleasePool scoped_pool;

    Initialize();
    std::wstring client_func =
        CommandLine::ForCurrentProcess()->GetSwitchValue(kRunClientProcess);
    // Check to see if we are being run as a client process.
    if (!client_func.empty()) {
      // Convert our function name to a usable string for GetProcAddress.
      std::string func_name(client_func.begin(), client_func.end());

      return multi_process_function_list::InvokeChildProcessTest(func_name);
    }
    int result = RUN_ALL_TESTS();

    // This MUST happen before Shutdown() since Shutdown() tears down
    // objects (such as NotificationService::current()) that Cocoa
    // objects use to remove themselves as observers.
    scoped_pool.Recycle();

    Shutdown();

    return result;
  }

 protected:
  // All fatal log messages (e.g. DCHECK failures) imply unit test failures.
  static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
  }

#if defined(OS_WIN)
  // Disable crash dialogs so that it doesn't gum up the buildbot
  virtual void SuppressErrorDialogs() {
    UINT new_flags = SEM_FAILCRITICALERRORS |
                     SEM_NOGPFAULTERRORBOX |
                     SEM_NOOPENFILEERRORBOX;

    // Preserve existing error mode, as discussed at
    // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);
  }
#endif

  // Override these for custom initialization and shutdown handling.  Use these
  // instead of putting complex code in your constructor/destructor.

  virtual void Initialize() {
    // Initialize logging.
    FilePath exe;
    PathService::Get(base::FILE_EXE, &exe);
    FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
    logging::InitLogging(log_filename.value().c_str(),
                         logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                         logging::LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);
    // We want process and thread IDs because we may have multiple processes.
    // Note: temporarily enabled timestamps in an effort to catch bug 6361.
    logging::SetLogItems(true, true, true, true);

#if defined(OS_WIN)
    // In some cases, we do not want to see standard error dialogs.
    if (!IsDebuggerPresent() &&
        !CommandLine::ForCurrentProcess()->HasSwitch(L"show-error-dialogs")) {
      SuppressErrorDialogs();
#if !defined(PURIFY)
      // When the code in this file moved around, bug 6436 resurfaced.
      // As a hack workaround, just #ifdef out this code for Purify builds.
      logging::SetLogAssertHandler(UnitTestAssertHandler);
#endif
    }
#endif

    icu_util::Initialize();
  }

  virtual void Shutdown() {
  }

  // Make sure that we setup an AtExitManager so Singleton objects will be
  // destroyed.
  base::AtExitManager at_exit_manager_;
};

#endif  // BASE_TEST_SUITE_H_
