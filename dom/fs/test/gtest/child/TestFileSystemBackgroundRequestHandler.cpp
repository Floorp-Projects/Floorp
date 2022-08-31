/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemBackgroundRequestHandler.h"
#include "FileSystemMocks.h"
#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/OriginPrivateFileSystemChild.h"
#include "mozilla/dom/POriginPrivateFileSystem.h"

namespace mozilla::dom::fs::test {

class TestFileSystemBackgroundRequestHandler : public ::testing::Test {
 protected:
  void SetUp() override {
    mOPFSChild = MakeAndAddRef<TestOriginPrivateFileSystemChild>();
  }

  UniquePtr<FileSystemBackgroundRequestHandler>
  GetFileSystemBackgroundRequestHandler() {
    return MakeUnique<FileSystemBackgroundRequestHandler>(
        new TestFileSystemChildFactory(mOPFSChild));
  }

  mozilla::ipc::PrincipalInfo mPrincipalInfo = GetPrincipalInfo();
  RefPtr<TestOriginPrivateFileSystemChild> mOPFSChild;
};

TEST_F(TestFileSystemBackgroundRequestHandler,
       isCreateFileSystemManagerChildSuccessful) {
  EXPECT_CALL(*mOPFSChild, Shutdown()).WillOnce([this]() {
    mOPFSChild->OriginPrivateFileSystemChild::Shutdown();
  });

  bool done = false;
  auto testable = GetFileSystemBackgroundRequestHandler();
  testable->CreateFileSystemManagerChild(mPrincipalInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done](const RefPtr<OriginPrivateFileSystemChild>& child) {
            done = true;
          },
          [&done](nsresult) { done = true; });
  // MozPromise should be rejected
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [&done]() { return done; });
}

}  // namespace mozilla::dom::fs::test
