// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <windows.h>

#include "base/command_line.h"
#include "base/basictypes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class ChromeLoggingTest : public testing::Test {
   public:
    // Stores the current value of the log file name environment
    // variable and sets the variable to new_value.
    void SaveEnvironmentVariable(std::wstring new_value) {
      unsigned status = GetEnvironmentVariable(env_vars::kLogFileName,
                                               environment_filename_,
                                               MAX_PATH);
      if (!status) {
        wcscpy_s(environment_filename_, L"");
      }

      SetEnvironmentVariable(env_vars::kLogFileName, new_value.c_str());
    }

    // Restores the value of the log file nave environment variable
    // previously saved by SaveEnvironmentVariable().
    void RestoreEnvironmentVariable() {
      SetEnvironmentVariable(env_vars::kLogFileName, environment_filename_);
    }

   private:
    wchar_t environment_filename_[MAX_PATH];  // Saves real environment value.
  };
};

// Tests the log file name getter without an environment variable.
TEST_F(ChromeLoggingTest, LogFileName) {
  SaveEnvironmentVariable(std::wstring());

  std::wstring filename = logging::GetLogFileName();
  ASSERT_NE(std::wstring::npos, filename.find(L"chrome_debug.log"));

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable(std::wstring(L"test value"));

  std::wstring filename = logging::GetLogFileName();
  ASSERT_EQ(std::wstring(L"test value"), filename);

  RestoreEnvironmentVariable();
}

#ifndef NDEBUG  // We don't have assertions in release builds.
// Tests whether we correctly fail on browser assertions during tests.
class AssertionTest : public UITest {
 protected:
 AssertionTest() : UITest()
  {
    // Initial loads will never complete due to assertion.
    wait_for_initial_loads_ = false;

    // We're testing the renderer rather than the browser assertion here,
    // because the browser assertion would flunk the test during SetUp()
    // (since TAU wouldn't be able to find the browser window).
    launch_arguments_.AppendSwitch(switches::kRendererAssertTest);
  }
};

// Launch the app in assertion test mode, then close the app.
TEST_F(AssertionTest, Assertion) {
  if (UITest::in_process_renderer()) {
    // in process mode doesn't do the crashing.
    expected_errors_ = 0;
    expected_crashes_ = 0;
  } else {
    expected_errors_ = 1;
    expected_crashes_ = 1;
  }
}
#endif  // NDEBUG

// Tests whether we correctly fail on browser crashes during UI Tests.
class RendererCrashTest : public UITest {
 protected:
 RendererCrashTest() : UITest()
  {
    // Initial loads will never complete due to crash.
    wait_for_initial_loads_ = false;

    launch_arguments_.AppendSwitch(switches::kRendererCrashTest);
  }
};

// Launch the app in renderer crash test mode, then close the app.
TEST_F(RendererCrashTest, Crash) {
  if (UITest::in_process_renderer()) {
    // in process mode doesn't do the crashing.
    expected_crashes_ = 0;
  } else {
    // Wait while the process is writing the crash dump.
    PlatformThread::Sleep(5000);
    expected_crashes_ = 1;
  }
}

// Tests whether we correctly fail on browser crashes during UI Tests.
class BrowserCrashTest : public UITest {
 protected:
 BrowserCrashTest() : UITest()
  {
    // Initial loads will never complete due to crash.
    wait_for_initial_loads_ = false;

    launch_arguments_.AppendSwitch(switches::kBrowserCrashTest);
  }
};

// Launch the app in browser crash test mode.
// This test is disabled. See bug 1198934.
TEST_F(BrowserCrashTest, DISABLED_Crash) {
  // Wait while the process is writing the crash dump.
  PlatformThread::Sleep(5000);
  expected_crashes_ = 1;
}
