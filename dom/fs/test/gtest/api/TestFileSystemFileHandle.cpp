/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemMocks.h"
#include "fs/FileSystemChildFactory.h"
#include "gtest/gtest.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FileSystemFileHandle.h"
#include "mozilla/dom/FileSystemFileHandleBinding.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StorageManager.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom::fs::test {

class TestFileSystemFileHandle : public ::testing::Test {
 protected:
  void SetUp() override {
    mRequestHandler = MakeUnique<MockFileSystemRequestHandler>();
    mMetadata =
        FileSystemEntryMetadata("file"_ns, u"File"_ns, /* directory */ false);
    mManager = MakeAndAddRef<FileSystemManager>(mGlobal, nullptr);
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  UniquePtr<MockFileSystemRequestHandler> mRequestHandler;
  FileSystemEntryMetadata mMetadata;
  RefPtr<FileSystemManager> mManager;
};

TEST_F(TestFileSystemFileHandle, constructFileHandleRefPointer) {
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);
}

TEST_F(TestFileSystemFileHandle, isHandleKindFile) {
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  ASSERT_EQ(FileSystemHandleKind::File, fileHandle->Kind());
}

TEST_F(TestFileSystemFileHandle, isFileReturned) {
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->GetFile(rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemFileHandle, doesGetFileFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->GetFile(rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

TEST_F(TestFileSystemFileHandle, isWritableReturned) {
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  FileSystemCreateWritableOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->CreateWritable(options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemFileHandle, doesCreateWritableFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  FileSystemCreateWritableOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->CreateWritable(options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

TEST_F(TestFileSystemFileHandle, isSyncAccessHandleReturned) {
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->CreateSyncAccessHandle(rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemFileHandle, doesCreateSyncAccessHandleFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemFileHandle> fileHandle = MakeAndAddRef<FileSystemFileHandle>(
      mGlobal, mManager, mMetadata, mRequestHandler.release());

  ASSERT_TRUE(fileHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->CreateSyncAccessHandle(rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

}  // namespace mozilla::dom::fs::test
