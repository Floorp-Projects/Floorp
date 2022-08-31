/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"
#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"

namespace mozilla::dom::fs::test {

namespace {

constexpr auto kTestOriginName = "http:://example.com"_ns;

}  // namespace

TEST(TestFileSystemDataManager, GetOrCreateFileSystemDataManager)
{
  bool done = false;

  data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
      Origin(kTestOriginName))
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
}

TEST(TestFileSystemDataManager, GetOrCreateFileSystemDataManager_PendingOpen)
{
  Registered<data::FileSystemDataManager> rdm1;

  Registered<data::FileSystemDataManager> rdm2;

  {
    bool done1 = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        Origin(kTestOriginName))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&rdm1, &done1](
                Registered<data::FileSystemDataManager> registeredDataManager) {
              ASSERT_TRUE(registeredDataManager->IsOpen());

              rdm1 = std::move(registeredDataManager);

              done1 = true;
            },
            [&done1](nsresult rejectValue) { done1 = true; });

    bool done2 = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        Origin(kTestOriginName))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&rdm2, &done2](
                Registered<data::FileSystemDataManager> registeredDataManager) {
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
}

TEST(TestFileSystemDataManager, GetOrCreateFileSystemDataManager_PendingClose)
{
  Registered<data::FileSystemDataManager> rdm;

  {
    bool done = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        Origin(kTestOriginName))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&rdm, &done](
                Registered<data::FileSystemDataManager> registeredDataManager) {
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
        Origin(kTestOriginName))
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
  }
}

}  // namespace mozilla::dom::fs::test
