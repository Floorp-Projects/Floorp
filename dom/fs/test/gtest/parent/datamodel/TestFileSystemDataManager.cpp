/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"
#include "TestHelpers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/test/QuotaManagerDependencyFixture.h"

namespace mozilla::dom::fs::test {

class TestFileSystemDataManager
    : public quota::test::QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() {
    EXPECT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));
    ASSERT_NO_FATAL_FAILURE(ShutdownFixture());
  }
};

TEST_F(TestFileSystemDataManager, GetOrCreateFileSystemDataManager) {
  auto backgroundTask = []() {
    bool done = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        GetTestOriginMetadata())
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [](Registered<data::FileSystemDataManager> registeredDataManager) {
              RefPtr<data::FileSystemDataManager> dataManager =
                  registeredDataManager.get();

              registeredDataManager = nullptr;

              return dataManager->OnClose();
            },
            [](nsresult rejectValue) {
              return BoolPromise::CreateAndReject(rejectValue, __func__);
            })
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const BoolPromise::ResolveOrRejectValue&) { done = true; });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

TEST_F(TestFileSystemDataManager,
       GetOrCreateFileSystemDataManager_PendingOpen) {
  auto backgroundTask = []() {
    Registered<data::FileSystemDataManager> rdm1;

    Registered<data::FileSystemDataManager> rdm2;

    {
      bool done1 = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm1, &done1](Registered<data::FileSystemDataManager>
                                  registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm1 = std::move(registeredDataManager);

                done1 = true;
              },
              [&done1](nsresult rejectValue) { done1 = true; });

      bool done2 = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm2, &done2](Registered<data::FileSystemDataManager>
                                  registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm2 = std::move(registeredDataManager);

                done2 = true;
              },
              [&done2](nsresult rejectValue) { done2 = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns,
                         [&done1, &done2]() { return done1 && done2; });
    }

    RefPtr<data::FileSystemDataManager> dm1 = rdm1.unwrap();

    RefPtr<data::FileSystemDataManager> dm2 = rdm2.unwrap();

    {
      bool done1 = false;

      dm1->OnClose()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done1](const BoolPromise::ResolveOrRejectValue&) { done1 = true; });

      bool done2 = false;

      dm2->OnClose()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done2](const BoolPromise::ResolveOrRejectValue&) { done2 = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns,
                         [&done1, &done2]() { return done1 && done2; });
    }
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

TEST_F(TestFileSystemDataManager,
       GetOrCreateFileSystemDataManager_PendingClose) {
  auto backgroundTask = []() {
    Registered<data::FileSystemDataManager> rdm;

    {
      bool done = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm, &done](Registered<data::FileSystemDataManager>
                                registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm = std::move(registeredDataManager);

                done = true;
              },
              [&done](nsresult rejectValue) { done = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
    }

    RefPtr<data::FileSystemDataManager> dm = rdm.unwrap();

    Unused << dm;

    {
      bool done = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [](Registered<data::FileSystemDataManager>
                     registeredDataManager) {
                RefPtr<data::FileSystemDataManager> dataManager =
                    registeredDataManager.get();

                registeredDataManager = nullptr;

                return dataManager->OnClose();
              },
              [](nsresult rejectValue) {
                return BoolPromise::CreateAndReject(rejectValue, __func__);
              })
          ->Then(GetCurrentSerialEventTarget(), __func__,
                 [&done](const BoolPromise::ResolveOrRejectValue&) {
                   done = true;
                 });

      SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
    }
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

}  // namespace mozilla::dom::fs::test
