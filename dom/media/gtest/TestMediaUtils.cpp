/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "MediaUtils.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/gtest/MozHelpers.h"

using namespace mozilla;
using namespace mozilla::gtest;
using namespace mozilla::media;

// Spawning the death test child process aborts on Android.
#if !defined(ANDROID)

// Kept here for reference as it can be handy during development.
#  define DISABLE_CRASH_REPORTING  \
    gtest::DisableCrashReporter(); \
    ZERO_GDB_SLEEP();

void DoCreateTicketBeforeAppShutdownOnMain() {
  auto reporter = ScopedTestResultReporter::Create(ExitMode::ExitOnDtor);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown);

  Monitor mon("TestMonitor");
  bool pastAppShutdown = false;
  bool backgroundTaskFinished = false;

  UniquePtr ticket = ShutdownBlockingTicket::Create(
      u"Test"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);

  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchBackgroundTask(NS_NewRunnableFunction(__func__, [&] {
        TimeStamp now = TimeStamp::Now();
        TimeStamp end = now + TimeDuration::FromSeconds(0.2);
        MonitorAutoLock lock(mon);
        while (!pastAppShutdown && (end - now) > TimeDuration()) {
          lock.Wait(end - now);
          now = TimeStamp::Now();
        }
        EXPECT_FALSE(pastAppShutdown);
        ticket = nullptr;
        while (!pastAppShutdown) {
          lock.Wait();
        }
        EXPECT_TRUE(pastAppShutdown);
        backgroundTaskFinished = true;
        lock.Notify();
      })));

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown);

  {
    MonitorAutoLock lock(mon);
    pastAppShutdown = true;
    lock.Notify();
    while (!backgroundTaskFinished) {
      lock.Wait();
    }
  }

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry);

  NS_ShutdownXPCOM(nullptr);
}

void DoCreateTicketAfterAppShutdownOnMain() {
  auto reporter = ScopedTestResultReporter::Create(ExitMode::ExitOnDtor);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown);

  auto ticket = ShutdownBlockingTicket::Create(
      u"Test"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  EXPECT_FALSE(ticket);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry);

  NS_ShutdownXPCOM(nullptr);
}

void DoCreateTicketBeforeAppShutdownOffMain() {
  auto reporter = ScopedTestResultReporter::Create(ExitMode::ExitOnDtor);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown);

  Monitor mon("TestMonitor");
  bool pastAppShutdown = false;
  bool ticketCreated = false;
  bool backgroundTaskFinished = false;

  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchBackgroundTask(NS_NewRunnableFunction(__func__, [&] {
        MonitorAutoLock lock(mon);
        auto ticket = ShutdownBlockingTicket::Create(
            u"Test"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
        EXPECT_TRUE(ticket);
        ticketCreated = true;
        lock.Notify();

        TimeStamp now = TimeStamp::Now();
        TimeStamp end = now + TimeDuration::FromSeconds(0.2);
        while (!pastAppShutdown && (end - now) > TimeDuration()) {
          lock.Wait(end - now);
          now = TimeStamp::Now();
        }
        EXPECT_FALSE(pastAppShutdown);
        ticket = nullptr;
        while (!pastAppShutdown) {
          lock.Wait();
        }
        EXPECT_TRUE(pastAppShutdown);
        backgroundTaskFinished = true;
        lock.Notify();
      })));

  {
    MonitorAutoLock lock(mon);
    while (!ticketCreated) {
      lock.Wait();
    }
  }

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown);

  MonitorAutoLock lock(mon);
  pastAppShutdown = true;
  lock.Notify();
  while (!backgroundTaskFinished) {
    lock.Wait();
  }

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry);

  NS_ShutdownXPCOM(nullptr);
}

void DoCreateTicketAfterAppShutdownOffMain() {
  auto reporter = ScopedTestResultReporter::Create(ExitMode::ExitOnDtor);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown);

  UniquePtr<ShutdownBlockingTicket> ticket;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchBackgroundTask(
      MakeAndAddRef<SyncRunnable>(NS_NewRunnableFunction(__func__, [&] {
        ticket = ShutdownBlockingTicket::Create(
            u"Test"_ns, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
      }))));

  EXPECT_FALSE(ticket);

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry);

  NS_ShutdownXPCOM(nullptr);
}

void DoTwoTicketsWithSameNameBothBlockShutdown() {
  auto reporter = ScopedTestResultReporter::Create(ExitMode::ExitOnDtor);

  const auto name = u"Test"_ns;
  auto ticket1 = ShutdownBlockingTicket::Create(
      name, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  EXPECT_TRUE(ticket1);
  auto ticket2 = ShutdownBlockingTicket::Create(
      name, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  EXPECT_TRUE(ticket2);

  ticket1 = nullptr;

  // A copyable holder for the std::function in NS_NewTimerWithCallback.
  auto ticket2Holder =
      MakeRefPtr<Refcountable<UniquePtr<ShutdownBlockingTicket>>>(
          ticket2.release());

  const auto waitBeforeDestroyingTicket = TimeDuration::FromMilliseconds(100);
  TimeStamp before = TimeStamp::Now();
  auto timerResult = NS_NewTimerWithCallback(
      [t = std::move(ticket2Holder)](nsITimer* aTimer) {},
      waitBeforeDestroyingTicket, nsITimer::TYPE_ONE_SHOT, __func__);
  ASSERT_TRUE(timerResult.isOk());

  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM);
  AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry);

  NS_ShutdownXPCOM(nullptr);
  TimeStamp after = TimeStamp::Now();
  EXPECT_GT((after - before).ToMilliseconds(),
            waitBeforeDestroyingTicket.ToMilliseconds());
}

TEST(ShutdownBlockingTicketDeathTest, CreateTicketBeforeAppShutdownOnMain)
{
  EXPECT_EXIT(DoCreateTicketBeforeAppShutdownOnMain(),
              testing::ExitedWithCode(0), "");
}

TEST(ShutdownBlockingTicketDeathTest, CreateTicketAfterAppShutdownOnMain)
{
  EXPECT_EXIT(DoCreateTicketAfterAppShutdownOnMain(),
              testing::ExitedWithCode(0), "");
}

TEST(ShutdownBlockingTicketDeathTest, CreateTicketBeforeAppShutdownOffMain)
{
  EXPECT_EXIT(DoCreateTicketBeforeAppShutdownOffMain(),
              testing::ExitedWithCode(0), "");
}

TEST(ShutdownBlockingTicketDeathTest, CreateTicketAfterAppShutdownOffMain)
{
  EXPECT_EXIT(DoCreateTicketAfterAppShutdownOffMain(),
              testing::ExitedWithCode(0), "");
}

TEST(ShutdownBlockingTicketDeathTest, TwoTicketsWithSameNameBothBlockShutdown)
{
  EXPECT_EXIT(DoTwoTicketsWithSameNameBothBlockShutdown(),
              testing::ExitedWithCode(0), "");
}

#  undef DISABLE_CRASH_REPORTING

#endif
