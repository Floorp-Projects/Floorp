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
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/PFileSystemManager.h"

namespace mozilla::dom::fs::test {

class TestFileSystemBackgroundRequestHandler : public ::testing::Test {
 protected:
  void SetUp() override {
    mFileSystemManagerChild = MakeAndAddRef<TestFileSystemManagerChild>();
  }

  RefPtr<FileSystemBackgroundRequestHandler>
  GetFileSystemBackgroundRequestHandler() {
    return MakeRefPtr<FileSystemBackgroundRequestHandler>(
        new TestFileSystemChildFactory(mFileSystemManagerChild));
  }

  mozilla::ipc::PrincipalInfo mPrincipalInfo = GetPrincipalInfo();
  RefPtr<TestFileSystemManagerChild> mFileSystemManagerChild;
};

TEST_F(TestFileSystemBackgroundRequestHandler,
       isCreateFileSystemManagerChildSuccessful) {
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  bool done = false;
  auto testable = GetFileSystemBackgroundRequestHandler();
  testable->CreateFileSystemManagerChild(mPrincipalInfo)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done](bool) { done = true; }, [&done](nsresult) { done = true; });
  // MozPromise should be rejected
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [&done]() { return done; });
}

}  // namespace mozilla::dom::fs::test
