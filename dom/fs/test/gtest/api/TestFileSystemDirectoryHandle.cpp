/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FileSystemMocks.h"

#include "mozilla/dom/FileSystemDirectoryHandle.h"
#include "mozilla/dom/FileSystemDirectoryHandleBinding.h"
#include "mozilla/dom/FileSystemDirectoryIterator.h"
#include "mozilla/dom/FileSystemHandle.h"
#include "mozilla/dom/FileSystemHandleBinding.h"

#include "mozilla/UniquePtr.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom::fs::test {

class TestFileSystemDirectoryHandle : public ::testing::Test {
 protected:
  void SetUp() override {
    mRequestHandler = MakeUnique<MockFileSystemRequestHandler>();
    mMetadata = FileSystemEntryMetadata("dir"_ns, u"Directory"_ns);
    mName = u"testDir"_ns;
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  UniquePtr<MockFileSystemRequestHandler> mRequestHandler;
  FileSystemEntryMetadata mMetadata;
  nsString mName;
};

TEST_F(TestFileSystemDirectoryHandle, constructDirectoryHandleRefPointer) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata);

  ASSERT_TRUE(dirHandle);
}

TEST_F(TestFileSystemDirectoryHandle, areEntriesReturned) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  RefPtr<FileSystemDirectoryIterator> entries = dirHandle->Entries();
  ASSERT_TRUE(entries);
}

TEST_F(TestFileSystemDirectoryHandle, areKeysReturned) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  RefPtr<FileSystemDirectoryIterator> keys = dirHandle->Keys();
  ASSERT_TRUE(keys);
}

TEST_F(TestFileSystemDirectoryHandle, areValuesReturned) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  RefPtr<FileSystemDirectoryIterator> values = dirHandle->Values();
  ASSERT_TRUE(values);
}

TEST_F(TestFileSystemDirectoryHandle, isHandleKindDirectory) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  ASSERT_EQ(FileSystemHandleKind::Directory, dirHandle->Kind());
}

TEST_F(TestFileSystemDirectoryHandle, isFileHandleReturned) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  FileSystemGetFileOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->GetFileHandle(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemDirectoryHandle, doesGetFileHandleFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata);

  ASSERT_TRUE(dirHandle);

  FileSystemGetFileOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->GetFileHandle(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

TEST_F(TestFileSystemDirectoryHandle, isDirectoryHandleReturned) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  FileSystemGetDirectoryOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->GetDirectoryHandle(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemDirectoryHandle, doesGetDirectoryHandleFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata);

  ASSERT_TRUE(dirHandle);

  FileSystemGetDirectoryOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->GetDirectoryHandle(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

TEST_F(TestFileSystemDirectoryHandle, isRemoveEntrySuccessful) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  FileSystemRemoveOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->RemoveEntry(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemDirectoryHandle, doesRemoveEntryFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata);

  ASSERT_TRUE(dirHandle);

  FileSystemRemoveOptions options;
  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->RemoveEntry(mName, options, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

TEST_F(TestFileSystemDirectoryHandle, isResolveSuccessful) {
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata,
                                               mRequestHandler.release());

  ASSERT_TRUE(dirHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->Resolve(*dirHandle, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_OK));
}

TEST_F(TestFileSystemDirectoryHandle, doesResolveFailOnNullGlobal) {
  mGlobal = nullptr;
  RefPtr<FileSystemDirectoryHandle> dirHandle =
      MakeAndAddRef<FileSystemDirectoryHandle>(mGlobal, mMetadata);

  ASSERT_TRUE(dirHandle);

  IgnoredErrorResult rv;
  RefPtr<Promise> promise = dirHandle->Resolve(*dirHandle, rv);

  ASSERT_TRUE(rv.ErrorCodeIs(NS_ERROR_UNEXPECTED));
}

}  // namespace mozilla::dom::fs::test
