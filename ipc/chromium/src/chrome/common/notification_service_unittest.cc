// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NotificationServiceTest: public testing::Test {
};

class TestObserver : public NotificationObserver {
public:
  TestObserver() : notification_count_(0) {}

  int notification_count() { return notification_count_; }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    ++notification_count_;
  }

private:
  int notification_count_;
};

// Bogus class to act as a NotificationSource for the messages.
class TestSource {};

}  // namespace


TEST(NotificationServiceTest, Basic) {
  TestSource test_source;
  TestSource other_source;

  // Check the equality operators defined for NotificationSource
  EXPECT_TRUE(
    Source<TestSource>(&test_source) == Source<TestSource>(&test_source));
  EXPECT_TRUE(
    Source<TestSource>(&test_source) != Source<TestSource>(&other_source));

  TestObserver all_types_all_sources;
  TestObserver idle_all_sources;
  TestObserver all_types_test_source;
  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  // Make sure it doesn't freak out when there are no observers.
  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  service->AddObserver(&all_types_all_sources,
                       NotificationType::ALL,
                       NotificationService::AllSources());
  service->AddObserver(&idle_all_sources,
                       NotificationType::IDLE,
                       NotificationService::AllSources());
  service->AddObserver(&all_types_test_source,
                       NotificationType::ALL,
                       Source<TestSource>(&test_source));
  service->AddObserver(&idle_test_source,
                       NotificationType::IDLE,
                       Source<TestSource>(&test_source));

  EXPECT_EQ(0, all_types_all_sources.notification_count());
  EXPECT_EQ(0, idle_all_sources.notification_count());
  EXPECT_EQ(0, all_types_test_source.notification_count());
  EXPECT_EQ(0, idle_test_source.notification_count());

  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(1, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NotificationType::BUSY,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(2, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(3, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(NotificationType::BUSY,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(4, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Try send with NULL source.
  service->Notify(NotificationType::IDLE,
                  NotificationService::AllSources(),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->RemoveObserver(&all_types_all_sources,
                          NotificationType::ALL,
                          NotificationService::AllSources());
  service->RemoveObserver(&idle_all_sources,
                          NotificationType::IDLE,
                          NotificationService::AllSources());
  service->RemoveObserver(&all_types_test_source,
                          NotificationType::ALL,
                          Source<TestSource>(&test_source));
  service->RemoveObserver(&idle_test_source,
                          NotificationType::IDLE,
                          Source<TestSource>(&test_source));

  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Removing an observer that isn't there is a no-op, this should be fine.
  service->RemoveObserver(&all_types_all_sources,
                          NotificationType::ALL,
                          NotificationService::AllSources());
}

TEST(NotificationServiceTest, MultipleRegistration) {
  TestSource test_source;

  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  service->AddObserver(&idle_test_source,
                       NotificationType::IDLE,
                       Source<TestSource>(&test_source));
  service->AddObserver(&idle_test_source,
                       NotificationType::ALL,
                       Source<TestSource>(&test_source));

  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(2, idle_test_source.notification_count());

  service->RemoveObserver(&idle_test_source,
                          NotificationType::IDLE,
                          Source<TestSource>(&test_source));

  service->Notify(NotificationType::IDLE,
                 Source<TestSource>(&test_source),
                 NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());

  service->RemoveObserver(&idle_test_source,
                          NotificationType::ALL,
                          Source<TestSource>(&test_source));

  service->Notify(NotificationType::IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());
}
