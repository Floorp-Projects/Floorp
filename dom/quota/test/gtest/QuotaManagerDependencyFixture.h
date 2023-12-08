/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_
#define DOM_QUOTA_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_

#include "gtest/gtest.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/QuotaManager.h"

#define QM_TEST_FAIL [](nsresult) { FAIL(); }

namespace mozilla::dom::quota::test {

class QuotaManagerDependencyFixture : public testing::Test {
 public:
  static void InitializeFixture();
  static void ShutdownFixture();

  static void InitializeStorage();
  static void StorageInitialized(bool* aResult);
  static void AssertStorageInitialized();
  static void AssertStorageNotInitialized();
  static void ShutdownStorage();

  static void ClearStoragesForOrigin(const OriginMetadata& aOriginMetadata);

  /* Convenience method for tasks which must be called on PBackground thread */
  template <class Invokable, class... Args>
  static void PerformOnBackgroundThread(Invokable&& aInvokable,
                                        Args&&... aArgs) {
    bool done = false;
    auto boundTask =
        // For c++17, bind is cleaner than tuple for parameter pack forwarding
        // NOLINTNEXTLINE(modernize-avoid-bind)
        std::bind(std::forward<Invokable>(aInvokable),
                  std::forward<Args>(aArgs)...);
    InvokeAsync(BackgroundTargetStrongRef(), __func__,
                [boundTask = std::move(boundTask)]() mutable {
                  boundTask();
                  return BoolPromise::CreateAndResolve(true, __func__);
                })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const BoolPromise::ResolveOrRejectValue& /* aValue */) {
                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  }

  /* Convenience method for tasks which must be executed on IO thread */
  template <class Invokable, class... Args>
  static void PerformOnIOThread(Invokable&& aInvokable, Args&&... aArgs) {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;
    auto boundTask =
        // For c++17, bind is cleaner than tuple for parameter pack forwarding
        // NOLINTNEXTLINE(modernize-avoid-bind)
        std::bind(std::forward<Invokable>(aInvokable),
                  std::forward<Args>(aArgs)...);
    InvokeAsync(quotaManager->IOThread(), __func__,
                [boundTask = std::move(boundTask)]() mutable {
                  boundTask();
                  return BoolPromise::CreateAndResolve(true, __func__);
                })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const BoolPromise::ResolveOrRejectValue& value) {
                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  }

  template <class Task>
  static void PerformClientDirectoryTest(const ClientMetadata& aClientMetadata,
                                         Task&& aTask) {
    PerformOnBackgroundThread([clientMetadata = aClientMetadata,
                               task = std::forward<Task>(aTask)]() mutable {
      RefPtr<ClientDirectoryLock> directoryLock;

      QuotaManager* quotaManager = QuotaManager::Get();
      ASSERT_TRUE(quotaManager);

      bool done = false;

      quotaManager->OpenClientDirectory(clientMetadata)
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&directoryLock,
               &done](RefPtr<ClientDirectoryLock> aResolveValue) {
                directoryLock = std::move(aResolveValue);

                done = true;
              },
              [&done](const nsresult aRejectValue) {
                ASSERT_TRUE(false);

                done = true;
              });

      SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

      ASSERT_TRUE(directoryLock);

      PerformOnIOThread(std::move(task), directoryLock->Id());

      directoryLock = nullptr;
    });
  }

  static const nsCOMPtr<nsISerialEventTarget>& BackgroundTargetStrongRef() {
    return sBackgroundTarget;
  }

  static OriginMetadata GetTestOriginMetadata();
  static ClientMetadata GetTestClientMetadata();

  static OriginMetadata GetOtherTestOriginMetadata();
  static ClientMetadata GetOtherTestClientMetadata();

 private:
  static void EnsureQuotaManager();

  static nsCOMPtr<nsISerialEventTarget> sBackgroundTarget;
};

}  // namespace mozilla::dom::quota::test

#endif  // DOM_QUOTA_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_
