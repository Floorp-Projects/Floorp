// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

class PowerTest : public base::SystemMonitor::PowerObserver {
 public:
  PowerTest()
    : battery_(false),
      power_state_changes_(0),
      suspends_(0),
      resumes_(0) {};

  // PowerObserver callbacks.
  void OnPowerStateChange(base::SystemMonitor*) { power_state_changes_++; };
  void OnSuspend(base::SystemMonitor*) { suspends_++; };
  void OnResume(base::SystemMonitor*) { resumes_++; };

  // Test status counts.
  bool battery() { return battery_; }
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  bool battery_;   // Do we currently think we're on battery power.
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;  // Count of OnSuspend notifications.
  int resumes_;  // Count of OnResume notifications.
};

TEST(SystemMonitor, PowerNotifications) {
  const int kObservers = 5;

  // Initialize a message loop for this to run on.
  MessageLoop loop;
  // Initialize time() since it registers as a SystemMonitor observer.
  base::Time now = base::Time::Now();

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  PowerTest test[kObservers];
  for (int index = 0; index < kObservers; ++index)
    monitor->AddObserver(&test[index]);

  // Send a bunch of power changes.  Since the battery power hasn't
  // actually changed, we shouldn't get notifications.
  for (int index = 0; index < 5; index++) {
    monitor->ProcessPowerMessage(base::SystemMonitor::POWER_STATE_EVENT);
    EXPECT_EQ(test[0].power_state_changes(), 0);
  }

  // Sending resume when not suspended should have no effect.
  monitor->ProcessPowerMessage(base::SystemMonitor::RESUME_EVENT);
  loop.RunAllPending();
  EXPECT_EQ(test[0].resumes(), 0);

  // Pretend we suspended.
  monitor->ProcessPowerMessage(base::SystemMonitor::SUSPEND_EVENT);
  loop.RunAllPending();
  EXPECT_EQ(test[0].suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  monitor->ProcessPowerMessage(base::SystemMonitor::SUSPEND_EVENT);
  loop.RunAllPending();
  EXPECT_EQ(test[0].suspends(), 1);

  // Pretend we were awakened.
  monitor->ProcessPowerMessage(base::SystemMonitor::RESUME_EVENT);
  loop.RunAllPending();
  EXPECT_EQ(test[0].resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  monitor->ProcessPowerMessage(base::SystemMonitor::RESUME_EVENT);
  loop.RunAllPending();
  EXPECT_EQ(test[0].resumes(), 1);
}
