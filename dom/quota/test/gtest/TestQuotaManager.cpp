/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/gtest/MozAssertions.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

class TestQuotaManager : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

// Test simple ShutdownStorage.
TEST_F(TestQuotaManager, ShutdownStorage_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());

    ASSERT_NS_SUCCEEDED(quotaManager->EnsureStorageIsInitialized());

    ASSERT_TRUE(quotaManager->IsStorageInitialized());
  });

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());
  });

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing.
TEST_F(TestQuotaManager, ShutdownStorage_Ongoing) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());

    ASSERT_NS_SUCCEEDED(quotaManager->EnsureStorageIsInitialized());

    ASSERT_TRUE(quotaManager->IsStorageInitialized());
  });

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());
  });

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing and storage
// initialization is scheduled after that.
TEST_F(TestQuotaManager, ShutdownStorage_OngoingWithScheduledInitialization) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());

    ASSERT_NS_SUCCEEDED(quotaManager->EnsureStorageIsInitialized());

    ASSERT_TRUE(quotaManager->IsStorageInitialized());
  });

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    // XXX We have to use ClearPrivateRepository for now because there's no
    // dedicated method for initializing storage on the PBackground thread yet.
    // ClearPrivateRepository triggers storage initialization before it clear
    // the private repository.
    promises.AppendElement(quotaManager->ClearPrivateRepository());
    promises.AppendElement(quotaManager->ShutdownStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_FALSE(quotaManager->IsStorageInitialized());
  });

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

}  // namespace mozilla::dom::quota::test
