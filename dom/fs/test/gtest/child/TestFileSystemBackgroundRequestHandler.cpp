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

using ::testing::Invoke;

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

  nsIGlobalObject* mGlobal = GetGlobal();
  RefPtr<TestOriginPrivateFileSystemChild> mOPFSChild;
};

class TestOPFSChild : public POriginPrivateFileSystemChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(TestOPFSChild)

 protected:
  ~TestOPFSChild() = default;
};

TEST_F(TestFileSystemBackgroundRequestHandler,
       isCreateFileSystemManagerChildSuccessful) {
  EXPECT_CALL(*mOPFSChild, AsBindable())
      .WillOnce(Invoke([]() -> POriginPrivateFileSystemChild* {
        return new TestOPFSChild();
      }));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  bool done = false;
  auto testable = GetFileSystemBackgroundRequestHandler();
  testable->CreateFileSystemManagerChild(mGlobal)->Then(
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
