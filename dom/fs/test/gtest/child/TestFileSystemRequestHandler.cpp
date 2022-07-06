/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FileSystemMocks.h"
#include "fs/FileSystemRequestHandler.h"

#include "mozilla/dom/IPCBlob.h"
#include "mozilla/dom/OriginPrivateFileSystemChild.h"
#include "mozilla/dom/POriginPrivateFileSystem.h"
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
    mOPFSChild = MakeAndAddRef<TestOriginPrivateFileSystemChild>();
    mActor = MakeAndAddRef<FileSystemActorHolder>(mOPFSChild.get());
  }

  already_AddRefed<Promise> GetDefaultPromise() {
    IgnoredErrorResult rv;
    RefPtr<Promise> result = Promise::Create(mGlobal, rv);
    mListener->ClearDone();
    result->AppendNativeHandler(mListener->AsHandler());

    return result.forget();
  }

  UniquePtr<FileSystemRequestHandler> GetFileSystemRequestHandler() {
    return MakeUnique<FileSystemRequestHandler>(
        new TestFileSystemChildFactory(mOPFSChild));
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  RefPtr<ExpectResolveCalled> mListener;

  FileSystemChildMetadata mChild;
  FileSystemEntryMetadata mEntry;
  nsString mName;
  RefPtr<TestOriginPrivateFileSystemChild> mOPFSChild;
  RefPtr<FileSystemActorHolder> mActor;
};

class TestOPFSChild : public POriginPrivateFileSystemChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(TestOPFSChild)

  MOCK_METHOD(void, Close, ());

 protected:
  ~TestOPFSChild() {
    mozilla::ipc::MessageChannel* channel = GetIPCChannel();
    channel->Close();
  }
};

TEST_F(TestFileSystemRequestHandler, isGetRootSuccessful) {
  EXPECT_CALL(*mOPFSChild, AsBindable())
      .WillOnce(Invoke([]() -> POriginPrivateFileSystemChild* {
        return new TestOPFSChild();
      }));

  // 1) In the parent process, at the end of get root
  // 2) In the content process, when the handle is destroyed
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(2);

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetRoot(promise);
  // Promise should be rejected
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
  mOPFSChild->ManualRelease();  // Otherwise leak
}

TEST_F(TestFileSystemRequestHandler, isGetDirectoryHandleSuccessful) {
  auto fakeResponse = [](const auto& /* aRequest */, auto&& aResolve,
                         auto&& /* aReject */) {
    EntryId expected = "expected"_ns;
    FileSystemGetHandleResponse response(expected);
    aResolve(std::move(response));
  };

  EXPECT_CALL(mListener->GetSuccessHandler(), InvokeMe());
  EXPECT_CALL(*mOPFSChild, SendGetDirectoryHandle(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillOnce(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetDirectoryHandle(mActor, mChild,
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
  EXPECT_CALL(*mOPFSChild, SendGetFileHandle(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillOnce(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetFileHandle(mActor, mChild, /* create */ true, promise);
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
  EXPECT_CALL(*mOPFSChild, SendGetFile(_, _, _)).WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillOnce(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  RefPtr<Promise> promise = GetDefaultPromise();
  auto testable = GetFileSystemRequestHandler();
  testable->GetFile(mActor, mEntry, promise);
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

  EXPECT_CALL(*mOPFSChild, SendGetEntries(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillOnce(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  auto testable = GetFileSystemRequestHandler();
  ArrayAppendable sink;
  testable->GetEntries(mActor, mEntry.entryId(), /* page */ 0, promise, sink);
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
  EXPECT_CALL(*mOPFSChild, SendRemoveEntry(_, _, _))
      .WillOnce(Invoke(fakeResponse));
  EXPECT_CALL(*mOPFSChild, IsCloseable()).WillOnce(Return(true));
  EXPECT_CALL(*mOPFSChild, Close()).Times(1);

  auto testable = GetFileSystemRequestHandler();
  RefPtr<Promise> promise = GetDefaultPromise();
  testable->RemoveEntry(mActor, mChild, /* recursive */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

}  // namespace mozilla::dom::fs::test
