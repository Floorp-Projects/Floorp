/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"

#include "mozilla/ipc/UtilityProcessManager.h"

#if defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)
#  include "nsIAppShellService.h"
#  include "nsServiceManagerUtils.h"
#endif  // defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)

#if defined(XP_WIN)
#  include "mozilla/gtest/MozHelpers.h"
#  include "mozilla/ipc/UtilityProcessImpl.h"
#endif  // defined(XP_WIN)

#ifdef MOZ_WIDGET_ANDROID
#  define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/widget/appshell/android;1"
#endif  // MOZ_WIDGET_ANDROID

#ifdef XP_MACOSX
#  define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/widget/appshell/mac;1"
#endif  // XP_MACOSX

using namespace mozilla;
using namespace mozilla::ipc;

#define WAIT_FOR_EVENTS \
  SpinEventLoopUntil("UtilityProcess::emptyUtil"_ns, [&]() { return done; });

bool setupDone = false;

class UtilityProcess : public ::testing::Test {
 protected:
  void SetUp() override {
    if (setupDone) {
      return;
    }

#if defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)
    appShell = do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
#endif  // defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    mozilla::SandboxBroker::GeckoDependentInitialize();
#endif  // defined(XP_WIN) && defined(MOZ_SANDBOX)

    setupDone = true;
  }

#if defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)
  nsCOMPtr<nsIAppShellService> appShell;
#endif  // defined(MOZ_WIDGET_ANDROID) || defined(XP_MACOSX)
};

TEST_F(UtilityProcess, ProcessManager) {
  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  ASSERT_NE(utilityProc, nullptr);
}

TEST_F(UtilityProcess, NoProcess) {
  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  EXPECT_NE(utilityProc, nullptr);

  Maybe<int32_t> noPid =
      utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
  ASSERT_TRUE(noPid.isNothing());
}

TEST_F(UtilityProcess, LaunchProcess) {
  bool done = false;

  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  EXPECT_NE(utilityProc, nullptr);

  int32_t thisPid = base::GetCurrentProcId();
  EXPECT_GE(thisPid, 1);

  utilityProc->LaunchProcess(SandboxingKind::GENERIC_UTILITY)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&]() mutable {
            EXPECT_TRUE(true);

            Maybe<int32_t> utilityPid =
                utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
            EXPECT_TRUE(utilityPid.isSome());
            EXPECT_GE(*utilityPid, 1);
            EXPECT_NE(*utilityPid, thisPid);

            printf_stderr("UtilityProcess running as %d\n", *utilityPid);

            done = true;
          },
          [&](LaunchError const&) {
            EXPECT_TRUE(false);
            done = true;
          });

  WAIT_FOR_EVENTS;
}

TEST_F(UtilityProcess, DestroyProcess) {
  bool done = false;

  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();

  utilityProc->LaunchProcess(SandboxingKind::GENERIC_UTILITY)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&]() {
            Maybe<int32_t> utilityPid =
                utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
            EXPECT_TRUE(utilityPid.isSome());
            EXPECT_GE(*utilityPid, 1);

            utilityProc->CleanShutdown(SandboxingKind::GENERIC_UTILITY);

            utilityPid =
                utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
            EXPECT_TRUE(utilityPid.isNothing());

            EXPECT_TRUE(true);
            done = true;
          },
          [&](LaunchError const&) {
            EXPECT_TRUE(false);
            done = true;
          });

  WAIT_FOR_EVENTS;
}

#if defined(XP_WIN)
static void LoadLibraryCrash_Test() {
  mozilla::gtest::DisableCrashReporter();
  // Just a uuidgen name to have something random
  UtilityProcessImpl::LoadLibraryOrCrash(
      L"2b49036e-6ba3-400c-a297-38fa1f6c5255.dll");
}

TEST_F(UtilityProcess, LoadLibraryCrash) {
  ASSERT_DEATH_IF_SUPPORTED(LoadLibraryCrash_Test(), "");
}
#endif  // defined(XP_WIN)

#undef WAIT_FOR_EVENTS
