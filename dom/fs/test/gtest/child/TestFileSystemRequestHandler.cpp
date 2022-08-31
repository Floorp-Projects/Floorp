/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FileSystemBackgroundRequestHandler.h"
#include "FileSystemMocks.h"
#include "fs/FileSystemRequestHandler.h"

#include "mozilla/dom/IPCBlob.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/FileSystemManagerChild.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/IPCCore.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UniquePtr.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Invoke;
using ::testing::Return;

namespace mozilla::dom::fs::test {

class TestFileSystemRequestHandler : public ::testing::Test {
 protected:
  void SetUp() override {
    mListener = MakeAndAddRef<ExpectResolveCalled>();

    mChild = FileSystemChildMetadata("parent"_ns, u"ChildName"_ns);
    mEntry = FileSystemEntryMetadata("myid"_ns, u"EntryName"_ns);
    mName = u"testDir"_ns;
    mFileSystemManagerChild = MakeAndAddRef<TestFileSystemManagerChild>();
    mManager = MakeAndAddRef<FileSystemManager>(
        mGlobal, nullptr,
        MakeRefPtr<FileSystemBackgroundRequestHandler>(
            mFileSystemManagerChild));
  }

  already_AddRefed<Promise> GetDefaultPromise() {
    IgnoredErrorResult rv;
    RefPtr<Promise> result = Promise::Create(mGlobal, rv);
    mListener->ClearDone();
    result->AppendNativeHandler(mListener->AsHandler());

    return result.forget();
  }

  UniquePtr<FileSystemRequestHandler> GetFileSystemRequestHandler() {
    return MakeUnique<FileSystemRequestHandler>();
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  RefPtr<ExpectResolveCalled> mListener;

  FileSystemChildMetadata mChild;
  FileSystemEntryMetadata mEntry;
  nsString mName;
  RefPtr<TestFileSystemManagerChild> mFileSystemManagerChild;
  RefPtr<FileSystemManager> mManager;
};

TEST_F(TestFileSystemRequestHandler, isGetRootHandleSuccessful) {
  auto fakeResponse = [](auto&& aResolve, auto&& /* aReject */) {
    EntryId expected = "expected"_ns;
    FileSystemGetHandleResponse response(expected);
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mFileSystemManagerChild, SendGetRootHandle(_, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetRootHandle(mManager, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetDirectoryHandleSuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    EntryId expected = "expected"_ns;
    FileSystemGetHandleResponse response(expected);
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mFileSystemManagerChild, SendGetDirectoryHandle(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetDirectoryHandle(mManager, mChild,
                               /* create */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetFileHandleSuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    EntryId expected = "expected"_ns;
    FileSystemGetHandleResponse response(expected);
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mFileSystemManagerChild, SendGetFileHandle(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetFileHandle(mManager, mChild, /* create */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetFileSuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    TimeStamp last_modified_ms = 0;
    mozilla::dom::IPCBlob file;
    ContentType type = u"txt"_ns;

    nsTArray<Name> path;
    path.AppendElement(u"root"_ns);
    path.AppendElement(u"Trash"_ns);

    FileSystemFileProperties properties(last_modified_ms, file, type, path);
    FileSystemGetFileResponse response(properties);
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mFileSystemManagerChild, SendGetFile(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetFile(mManager, mEntry, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetEntriesSuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    nsTArray<FileSystemEntryMetadata> files;
    nsTArray<FileSystemEntryMetadata> directories;
    FileSystemDirectoryListing listing(files, directories);
    FileSystemGetEntriesResponse response(listing);
    aResolve(std::move(response));
  };

  RefPtr<ExpectNotImplemented> listener = MakeAndAddRef<ExpectNotImplemented>();
  IgnoredErrorResult rv;
  listener->ClearDone();
  RefPtr<Promise> promise = Promise::Create(mGlobal, rv);
  promise->AppendNativeHandler(listener);

  EXPECT_CALL(*mFileSystemManagerChild, SendGetEntries(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  auto testable = GetFileSystemRequestHandler();
  ArrayAppendable sink;
  testable->GetEntries(mManager, mEntry.entryId(), /* page */ 0, promise, sink);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [listener]() { return listener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isRemoveEntrySuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    FileSystemRemoveEntryResponse response(mozilla::void_t{});
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mFileSystemManagerChild, SendRemoveEntry(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mFileSystemManagerChild, Shutdown())
      .WillOnce([fileSystemManagerChild =
                     static_cast<void*>(mFileSystemManagerChild.get())]() {
        static_cast<TestFileSystemManagerChild*>(fileSystemManagerChild)
            ->FileSystemManagerChild::Shutdown();
      });

  auto testable = GetFileSystemRequestHandler();
  RefPtr<Promise> promise = GetDefaultPromise();
  testable->RemoveEntry(mManager, mChild, /* recursive */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

}  // namespace mozilla::dom::fs::test
