// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>
#include <process.h>

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

class MockTimeTicks : public TimeTicks {
 public:
  static DWORD Ticker() {
    return static_cast<int>(InterlockedIncrement(&ticker_));
  }

  static void InstallTicker() {
    old_tick_function_ = SetMockTickFunction(&Ticker);
    ticker_ = -5;
  }

  static void UninstallTicker() {
    SetMockTickFunction(old_tick_function_);
  }

 private:
  static volatile LONG ticker_;
  static TickFunctionType old_tick_function_;
};

volatile LONG MockTimeTicks::ticker_;
MockTimeTicks::TickFunctionType MockTimeTicks::old_tick_function_;

HANDLE g_rollover_test_start;

unsigned __stdcall RolloverTestThreadMain(void* param) {
  int64 counter = reinterpret_cast<int64>(param);
  DWORD rv = WaitForSingleObject(g_rollover_test_start, INFINITE);
  EXPECT_EQ(rv, WAIT_OBJECT_0);

  TimeTicks last = TimeTicks::Now();
  for (int index = 0; index < counter; index++) {
    TimeTicks now = TimeTicks::Now();
    int64 milliseconds = (now - last).InMilliseconds();
    // This is a tight loop; we could have looped faster than our
    // measurements, so the time might be 0 millis.
    EXPECT_GE(milliseconds, 0);
    EXPECT_LT(milliseconds, 250);
    last = now;
  }
  return 0;
}

}  // namespace

TEST(TimeTicks, WinRollover) {
  // The internal counter rolls over at ~49days.  We'll use a mock
  // timer to test this case.
  // Basic test algorithm:
  //   1) Set clock to rollover - N
  //   2) Create N threads
  //   3) Start the threads
  //   4) Each thread loops through TimeTicks() N times
  //   5) Each thread verifies integrity of result.

  const int kThreads = 8;
  // Use int64 so we can cast into a void* without a compiler warning.
  const int64 kChecks = 10;

  // It takes a lot of iterations to reproduce the bug!
  // (See bug 1081395)
  for (int loop = 0; loop < 4096; loop++) {
    // Setup
    MockTimeTicks::InstallTicker();
    g_rollover_test_start = CreateEvent(0, TRUE, FALSE, 0);
    HANDLE threads[kThreads];

    for (int index = 0; index < kThreads; index++) {
      void* argument = reinterpret_cast<void*>(kChecks);
      unsigned thread_id;
      threads[index] = reinterpret_cast<HANDLE>(
        _beginthreadex(NULL, 0, RolloverTestThreadMain, argument, 0,
          &thread_id));
      EXPECT_NE((HANDLE)NULL, threads[index]);
    }

    // Start!
    SetEvent(g_rollover_test_start);

    // Wait for threads to finish
    for (int index = 0; index < kThreads; index++) {
      DWORD rv = WaitForSingleObject(threads[index], INFINITE);
      EXPECT_EQ(rv, WAIT_OBJECT_0);
    }

    CloseHandle(g_rollover_test_start);

    // Teardown
    MockTimeTicks::UninstallTicker();
  }
}

TEST(TimeTicks, SubMillisecondTimers) {
  // Loop for a bit getting timers quickly.  We want to
  // see at least one case where we get a new sample in
  // less than one millisecond.
  bool saw_submillisecond_timer = false;
  int64 min_timer = 1000;
  TimeTicks last_time = TimeTicks::HighResNow();
  for (int index = 0; index < 1000; index++) {
    TimeTicks now = TimeTicks::HighResNow();
    TimeDelta delta = now - last_time;
    if (delta.InMicroseconds() > 0 &&
        delta.InMicroseconds() < 1000) {
      if (min_timer > delta.InMicroseconds())
        min_timer = delta.InMicroseconds();
      saw_submillisecond_timer = true;
    }
    last_time = now;
  }
  EXPECT_TRUE(saw_submillisecond_timer);
  printf("Min timer is: %ldus\n", static_cast<long>(min_timer));
}

TEST(TimeTicks, TimeGetTimeCaps) {
  // Test some basic assumptions that we expect about how timeGetDevCaps works.

  TIMECAPS caps;
  MMRESULT status = timeGetDevCaps(&caps, sizeof(caps));
  EXPECT_EQ(TIMERR_NOERROR, status);
  if (status != TIMERR_NOERROR) {
    printf("Could not get timeGetDevCaps\n");
    return;
  }

  EXPECT_GE(static_cast<int>(caps.wPeriodMin), 1);
  EXPECT_GT(static_cast<int>(caps.wPeriodMax), 1);
  EXPECT_GE(static_cast<int>(caps.wPeriodMin), 1);
  EXPECT_GT(static_cast<int>(caps.wPeriodMax), 1);
  printf("timeGetTime range is %d to %dms\n", caps.wPeriodMin,
    caps.wPeriodMax);
}

TEST(TimeTicks, QueryPerformanceFrequency) {
  // Test some basic assumptions that we expect about QPC.

  LARGE_INTEGER frequency;
  BOOL rv = QueryPerformanceFrequency(&frequency);
  EXPECT_EQ(TRUE, rv);
  EXPECT_GT(frequency.QuadPart, 1000000);  // Expect at least 1MHz
  printf("QueryPerformanceFrequency is %5.2fMHz\n",
    frequency.QuadPart / 1000000.0);
}

TEST(TimeTicks, TimerPerformance) {
  // Verify that various timer mechanisms can always complete quickly.
  // Note:  This is a somewhat arbitrary test.
  const int kLoops = 10000;
  // Due to the fact that these run on bbots, which are horribly slow,
  // we can't really make any guarantees about minimum runtime.
  // Really, we want these to finish in ~10ms, and that is generous.
  const int kMaxTime = 35;  // Maximum acceptible milliseconds for test.

  typedef TimeTicks (*TestFunc)();
  struct TestCase {
    TestFunc func;
    char *description;
  };
  // Cheating a bit here:  assumes sizeof(TimeTicks) == sizeof(Time)
  // in order to create a single test case list.
  COMPILE_ASSERT(sizeof(TimeTicks) == sizeof(Time),
                 test_only_works_with_same_sizes);
  TestCase cases[] = {
    { reinterpret_cast<TestFunc>(Time::Now), "Time::Now" },
    { TimeTicks::Now, "TimeTicks::Now" },
    { TimeTicks::HighResNow, "TimeTicks::HighResNow" },
    { NULL, "" }
  };

  int test_case = 0;
  while (cases[test_case].func) {
    TimeTicks start = TimeTicks::HighResNow();
    for (int index = 0; index < kLoops; index++)
      cases[test_case].func();
    TimeTicks stop = TimeTicks::HighResNow();
    // Turning off the check for acceptible delays.  Without this check,
    // the test really doesn't do much other than measure.  But the
    // measurements are still useful for testing timers on various platforms.
    // The reason to remove the check is because the tests run on many
    // buildbots, some of which are VMs.  These machines can run horribly
    // slow, and there is really no value for checking against a max timer.
    //EXPECT_LT((stop - start).InMilliseconds(), kMaxTime);
    printf("%s: %1.2fus per call\n", cases[test_case].description,
      (stop - start).InMillisecondsF() * 1000 / kLoops);
    test_case++;
  }
}
