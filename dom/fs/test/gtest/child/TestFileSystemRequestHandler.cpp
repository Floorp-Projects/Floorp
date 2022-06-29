/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "FileSystemMocks.h"
#include "fs/FileSystemRequestHandler.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::dom::fs::test {

class TestFileSystemRequestHandler : public ::testing::Test {
 protected:
  void SetUp() override {
    mListener = MakeAndAddRef<ExpectNotImplemented>();

    mRequestHandler = MakeUnique<FileSystemRequestHandler>();

    mChild = FileSystemChildMetadata("parent"_ns, u"ChildName"_ns);
    mEntry = FileSystemEntryMetadata("myid"_ns, u"EntryName"_ns);
    mName = u"testDir"_ns;
    mActor = MakeAndAddRef<FileSystemActorHolder>();
  }

  already_AddRefed<Promise> GetDefaultPromise() {
    IgnoredErrorResult rv;
    RefPtr<Promise> result = Promise::Create(mGlobal, rv);
    mListener->ClearDone();
    result->AppendNativeHandler(mListener->AsHandler());

    return result.forget();
  }

  nsIGlobalObject* mGlobal = GetGlobal();
  RefPtr<ExpectNotImplemented> mListener;
  UniquePtr<FileSystemRequestHandler> mRequestHandler;

  FileSystemChildMetadata mChild;
  FileSystemEntryMetadata mEntry;
  nsString mName;
  RefPtr<FileSystemActorHolder> mActor;
};

TEST_F(TestFileSystemRequestHandler, isGetRootSuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  mRequestHandler->GetRoot(promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetDirectoryHandleSuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  mRequestHandler->GetDirectoryHandle(mActor, mChild,
                                      /* create */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetFileHandleSuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  mRequestHandler->GetFileHandle(mActor, mChild, /* create */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetFileSuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  mRequestHandler->GetFile(mActor, mEntry, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isGetEntriesSuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  ArrayAppendable sink;
  mRequestHandler->GetEntries(mActor, mEntry.entryId(), /* page */ 0, promise,
                              sink);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

TEST_F(TestFileSystemRequestHandler, isRemoveEntrySuccessful) {
  RefPtr<Promise> promise = GetDefaultPromise();
  mRequestHandler->RemoveEntry(mActor, mChild, /* recursive */ true, promise);
  SpinEventLoopUntil("Promise is fulfilled or timeout"_ns,
                     [this]() { return mListener->IsDone(); });
}

}  // namespace mozilla::dom::fs::test
