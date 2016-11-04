/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemRootDirectoryReader.h"
#include "CallbackRunnables.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

namespace {

class EntriesCallbackRunnable final : public Runnable
{
public:
  EntriesCallbackRunnable(FileSystemEntriesCallback* aCallback,
                          const Sequence<RefPtr<FileSystemEntry>>& aEntries)
    : mCallback(aCallback)
    , mEntries(aEntries)
  {
    MOZ_ASSERT(aCallback);
  }

  NS_IMETHOD
  Run() override
  {
    Sequence<OwningNonNull<FileSystemEntry>> entries;
    for (uint32_t i = 0; i < mEntries.Length(); ++i) {
      if (!entries.AppendElement(mEntries[i].forget(), fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    mCallback->HandleEvent(entries);
    return NS_OK;
  }

private:
  RefPtr<FileSystemEntriesCallback> mCallback;
  Sequence<RefPtr<FileSystemEntry>> mEntries;
};

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemRootDirectoryReader,
                                   FileSystemDirectoryReader, mEntries)

NS_IMPL_ADDREF_INHERITED(FileSystemRootDirectoryReader,
                         FileSystemDirectoryReader)
NS_IMPL_RELEASE_INHERITED(FileSystemRootDirectoryReader,
                          FileSystemDirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileSystemRootDirectoryReader)
NS_INTERFACE_MAP_END_INHERITING(FileSystemDirectoryReader)

FileSystemRootDirectoryReader::FileSystemRootDirectoryReader(FileSystemDirectoryEntry* aParentEntry,
                                                             FileSystem* aFileSystem,
                                                             const Sequence<RefPtr<FileSystemEntry>>& aEntries)
  : FileSystemDirectoryReader(aParentEntry, aFileSystem, nullptr)
  , mEntries(aEntries)
  , mAlreadyRead(false)
{
  MOZ_ASSERT(aParentEntry);
  MOZ_ASSERT(aFileSystem);
}

FileSystemRootDirectoryReader::~FileSystemRootDirectoryReader()
{}

void
FileSystemRootDirectoryReader::ReadEntries(FileSystemEntriesCallback& aSuccessCallback,
                                           const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                                           ErrorResult& aRv)
{
  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
      new EmptyEntriesCallbackRunnable(&aSuccessCallback);
    aRv = NS_DispatchToMainThread(runnable);
    NS_WARNING_ASSERTION(!aRv.Failed(), "NS_DispatchToMainThread failed");
    return;
  }

  // This object can be used only once.
  mAlreadyRead = true;

  RefPtr<EntriesCallbackRunnable> runnable =
    new EntriesCallbackRunnable(&aSuccessCallback, mEntries);
  aRv = NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(!aRv.Failed(), "NS_DispatchToMainThread failed");
}

} // dom namespace
} // mozilla namespace
