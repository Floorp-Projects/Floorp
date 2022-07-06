/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FileSystemMocks.h"

#include "mozilla/dom/FileSystemDirectoryHandle.h"
#include "mozilla/dom/FileSystemFileHandle.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom::fs::test {

class TestFileSystemHandle : public ::testing::Test {
 protected:
  void SetUp() override {
    mDirMetadata = FileSystemEntryMetadata("dir"_ns, u"Directory"_ns);
    mFileMetadata = FileSystemEntryMetadata("file"_ns, u"File"_ns);
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  FileSystemEntryMetadata mDirMetadata;
  FileSystemEntryMetadata mFileMetadata;
};

TEST_F(TestFileSystemHandle, createAndDestroyHandles) {
  RefPtr<FileSystemHandle> dirHandle =
      new FileSystemDirectoryHandle(mGlobal, mDirMetadata);
  RefPtr<FileSystemHandle> fileHandle =
      new FileSystemFileHandle(mGlobal, mFileMetadata);

  EXPECT_TRUE(dirHandle);
  EXPECT_TRUE(fileHandle);
}

TEST_F(TestFileSystemHandle, areFileNamesAsExpected) {
  RefPtr<FileSystemHandle> dirHandle =
      new FileSystemDirectoryHandle(mGlobal, mDirMetadata);
  RefPtr<FileSystemHandle> fileHandle =
      new FileSystemFileHandle(mGlobal, mFileMetadata);

  auto GetEntryName = [](const RefPtr<FileSystemHandle>& aHandle) {
    DOMString domName;
    aHandle->GetName(domName);
    nsString result;
    domName.ToString(result);
    return result;
  };

  const nsString& dirName = GetEntryName(dirHandle);
  EXPECT_TRUE(mDirMetadata.entryName().Equals(dirName));

  const nsString& fileName = GetEntryName(fileHandle);
  EXPECT_TRUE(mFileMetadata.entryName().Equals(fileName));
}

TEST_F(TestFileSystemHandle, isParentObjectReturned) {
  ASSERT_TRUE(mGlobal);
  RefPtr<FileSystemHandle> dirHandle =
      new FileSystemDirectoryHandle(mGlobal, mDirMetadata);

  ASSERT_EQ(mGlobal, dirHandle->GetParentObject());
}

TEST_F(TestFileSystemHandle, areHandleKindsAsExpected) {
  RefPtr<FileSystemHandle> dirHandle =
      new FileSystemDirectoryHandle(mGlobal, mDirMetadata);
  RefPtr<FileSystemHandle> fileHandle =
      new FileSystemFileHandle(mGlobal, mFileMetadata);

  EXPECT_EQ(FileSystemHandleKind::Directory, dirHandle->Kind());
  EXPECT_EQ(FileSystemHandleKind::File, fileHandle->Kind());
}

TEST_F(TestFileSystemHandle, isDifferentEntry) {
  RefPtr<FileSystemHandle> dirHandle =
      new FileSystemDirectoryHandle(mGlobal, mDirMetadata);
  RefPtr<FileSystemHandle> fileHandle =
      new FileSystemFileHandle(mGlobal, mFileMetadata);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->IsSameEntry(*fileHandle, rv);
  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
  ASSERT_TRUE(promise);
  ASSERT_EQ(Promise::PromiseState::Rejected, promise->State());
}

TEST_F(TestFileSystemHandle, isSameEntry) {
  RefPtr<FileSystemHandle> fileHandle =
      new FileSystemFileHandle(mGlobal, mFileMetadata);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = fileHandle->IsSameEntry(*fileHandle, rv);
  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
  ASSERT_TRUE(promise);
  ASSERT_EQ(Promise::PromiseState::Rejected, promise->State());
}

}  // namespace mozilla::dom::fs::test
