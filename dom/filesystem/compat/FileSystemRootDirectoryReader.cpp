/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemRootDirectoryReader.h"
#include "CallbackRunnables.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/FileSystemUtils.h"

namespace mozilla {
namespace dom {

namespace {

class EntriesCallbackRunnable final : public Runnable {
 public:
  EntriesCallbackRunnable(FileSystemEntriesCallback* aCallback,
                          const Sequence<RefPtr<FileSystemEntry>>& aEntries)
      : Runnable("EntriesCallbackRunnable"),
        mCallback(aCallback),
        mEntries(aEntries) {
    MOZ_ASSERT(aCallback);
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    Sequence<OwningNonNull<FileSystemEntry>> entries;
    for (uint32_t i = 0; i < mEntries.Length(); ++i) {
      if (!entries.AppendElement(mEntries[i].forget(), fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    mCallback->Call(entries);
    return NS_OK;
  }

 private:
  const RefPtr<FileSystemEntriesCallback> mCallback;
  Sequence<RefPtr<FileSystemEntry>> mEntries;
};

}  // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemRootDirectoryReader,
                                   FileSystemDirectoryReader, mEntries)

NS_IMPL_ADDREF_INHERITED(FileSystemRootDirectoryReader,
                         FileSystemDirectoryReader)
NS_IMPL_RELEASE_INHERITED(FileSystemRootDirectoryReader,
                          FileSystemDirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemRootDirectoryReader)
NS_INTERFACE_MAP_END_INHERITING(FileSystemDirectoryReader)

FileSystemRootDirectoryReader::FileSystemRootDirectoryReader(
    FileSystemDirectoryEntry* aParentEntry, FileSystem* aFileSystem,
    const Sequence<RefPtr<FileSystemEntry>>& aEntries)
    : FileSystemDirectoryReader(aParentEntry, aFileSystem, nullptr),
      mEntries(aEntries),
      mAlreadyRead(false) {
  MOZ_ASSERT(aParentEntry);
  MOZ_ASSERT(aFileSystem);
}

FileSystemRootDirectoryReader::~FileSystemRootDirectoryReader() {}

void FileSystemRootDirectoryReader::ReadEntries(
    FileSystemEntriesCallback& aSuccessCallback,
    const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
    ErrorResult& aRv) {
  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
        new EmptyEntriesCallbackRunnable(&aSuccessCallback);

    aRv =
        FileSystemUtils::DispatchRunnable(GetParentObject(), runnable.forget());
    return;
  }

  // This object can be used only once.
  mAlreadyRead = true;

  RefPtr<EntriesCallbackRunnable> runnable =
      new EntriesCallbackRunnable(&aSuccessCallback, mEntries);

  aRv = FileSystemUtils::DispatchRunnable(GetParentObject(), runnable.forget());
}

}  // namespace dom
}  // namespace mozilla
