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

}  // namespace mozilla::dom::fs::test
