// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/third_party/nspr/prtime.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// time_t representation of 15th Oct 2007 12:45:00 PDT
PRTime comparison_time_pdt = 1192477500 * Time::kMicrosecondsPerSecond;

// Specialized test fixture allowing time strings without timezones to be
// tested by comparing them to a known time in the local zone.
class PRTimeTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Use mktime to get a time_t, and turn it into a PRTime by converting
    // seconds to microseconds.  Use 15th Oct 2007 12:45:00 local.  This
    // must be a time guaranteed to be outside of a DST fallback hour in
    // any timezone.
    struct tm local_comparison_tm = {
      0,            // second
      45,           // minute
      12,           // hour
      15,           // day of month
      10 - 1,       // month
      2007 - 1900,  // year
      0,            // day of week (ignored, output only)
      0,            // day of year (ignored, output only)
      -1            // DST in effect, -1 tells mktime to figure it out
    };
    comparison_time_local_ = mktime(&local_comparison_tm) *
                             Time::kMicrosecondsPerSecond;
    ASSERT_GT(comparison_time_local_, 0);
  }

  PRTime comparison_time_local_;
};

// Tests the PR_ParseTimeString nspr helper function for
// a variety of time strings.
TEST_F(PRTimeTest, ParseTimeTest1) {
  time_t current_time = 0;
  time(&current_time);

  const int BUFFER_SIZE = 64;
  struct tm local_time = {0};
  char time_buf[BUFFER_SIZE] = {0};
#if defined(OS_WIN)
  localtime_s(&local_time, &current_time);
  asctime_s(time_buf, arraysize(time_buf), &local_time);
#elif defined(OS_POSIX)
  localtime_r(&current_time, &local_time);
  asctime_r(&local_time, time_buf);
#endif

  PRTime current_time64 = static_cast<PRTime>(current_time) * PR_USEC_PER_SEC;

  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString(time_buf, PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(current_time64, parsed_time);
}

TEST_F(PRTimeTest, ParseTimeTest2) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Mon, 15 Oct 2007 19:45:00 GMT",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest3) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15 Oct 07 12:45:00", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest4) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15 Oct 07 19:45 GMT", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest5) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Mon Oct 15 12:45 PDT 2007",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

TEST_F(PRTimeTest, ParseTimeTest6) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Monday, Oct 15, 2007 12:45 PM",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest7) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("10/15/07 12:45:00 PM", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest8) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("15-OCT-2007 12:45pm", PR_FALSE,
                                       &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

TEST_F(PRTimeTest, ParseTimeTest9) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("16 Oct 2007 4:45-JST (Tuesday)",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(parsed_time, comparison_time_pdt);
}

// This tests the Time::FromString wrapper over PR_ParseTimeString
TEST_F(PRTimeTest, ParseTimeTest10) {
  Time parsed_time;
  bool result = Time::FromString(L"15/10/07 12:45", &parsed_time);
  EXPECT_EQ(true, result);

  time_t computed_time = parsed_time.ToTimeT();
  time_t time_to_compare = comparison_time_local_ /
                           Time::kMicrosecondsPerSecond;
  EXPECT_EQ(computed_time, time_to_compare);
}

// This tests the Time::FromString wrapper over PR_ParseTimeString
TEST_F(PRTimeTest, ParseTimeTest11) {
  Time parsed_time;
  bool result = Time::FromString(L"Mon, 15 Oct 2007 19:45:00 GMT",
                                 &parsed_time);
  EXPECT_EQ(true, result);

  time_t computed_time = parsed_time.ToTimeT();
  time_t time_to_compare = comparison_time_pdt / Time::kMicrosecondsPerSecond;
  EXPECT_EQ(computed_time, time_to_compare);
}

// Test some of edge cases around epoch, etc.
TEST_F(PRTimeTest, ParseTimeTestEpoch0) {
  Time parsed_time;

  // time_t == epoch == 0
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 01:00:00 +0100 1970",
                                   &parsed_time));
  EXPECT_EQ(0, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 00:00:00 GMT 1970",
                                   &parsed_time));
  EXPECT_EQ(0, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEpoch1) {
  Time parsed_time;

  // time_t == 1 second after epoch == 1
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 01:00:01 +0100 1970",
                                   &parsed_time));
  EXPECT_EQ(1, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 00:00:01 GMT 1970",
                                   &parsed_time));
  EXPECT_EQ(1, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEpoch2) {
  Time parsed_time;

  // time_t == 2 seconds after epoch == 2
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 01:00:02 +0100 1970",
                                   &parsed_time));
  EXPECT_EQ(2, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 00:00:02 GMT 1970",
                                   &parsed_time));
  EXPECT_EQ(2, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEpochNeg1) {
  Time parsed_time;

  // time_t == 1 second before epoch == -1
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 00:59:59 +0100 1970",
                                   &parsed_time));
  EXPECT_EQ(-1, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Wed Dec 31 23:59:59 GMT 1969",
                                   &parsed_time));
  EXPECT_EQ(-1, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEpochNeg2) {
  Time parsed_time;

  // time_t == 2 seconds before epoch == -2
  EXPECT_EQ(true, Time::FromString(L"Thu Jan 01 00:59:58 +0100 1970",
                                   &parsed_time));
  EXPECT_EQ(-2, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Wed Dec 31 23:59:58 GMT 1969",
                                   &parsed_time));
  EXPECT_EQ(-2, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEpoch1960) {
  Time parsed_time;

  // time_t before Epoch, in 1960
  EXPECT_EQ(true, Time::FromString(L"Wed Jun 29 19:40:01 +0100 1960",
                                   &parsed_time));
  EXPECT_EQ(-299999999, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Wed Jun 29 18:40:01 GMT 1960",
                                   &parsed_time));
  EXPECT_EQ(-299999999, parsed_time.ToTimeT());
  EXPECT_EQ(true, Time::FromString(L"Wed Jun 29 17:40:01 GMT 1960",
                                   &parsed_time));
  EXPECT_EQ(-300003599, parsed_time.ToTimeT());
}

TEST_F(PRTimeTest, ParseTimeTestEmpty) {
  Time parsed_time;
  EXPECT_FALSE(Time::FromString(L"", &parsed_time));
}

// This test should not crash when compiled with Visual C++ 2005 (see
// http://crbug.com/4387).
TEST_F(PRTimeTest, ParseTimeTestOutOfRange) {
  PRTime parsed_time = 0;
  // Note the lack of timezone in the time string.  The year has to be 3001.
  // The date has to be after 23:59:59, December 31, 3000, US Pacific Time, so
  // we use January 2, 3001 to make sure it's after the magic maximum in any
  // timezone.
  PRStatus result = PR_ParseTimeString("Sun Jan  2 00:00:00 3001",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
}

TEST_F(PRTimeTest, ParseTimeTestNotNormalized1) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Mon Oct 15 12:44:60 PDT 2007",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(comparison_time_pdt, parsed_time);
}

TEST_F(PRTimeTest, ParseTimeTestNotNormalized2) {
  PRTime parsed_time = 0;
  PRStatus result = PR_ParseTimeString("Sun Oct 14 36:45 PDT 2007",
                                       PR_FALSE, &parsed_time);
  EXPECT_EQ(PR_SUCCESS, result);
  EXPECT_EQ(comparison_time_pdt, parsed_time);
}

}  // namespace
