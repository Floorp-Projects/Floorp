/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootDirectoryReader.h"
#include "CallbackRunnables.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

namespace {

class EntriesCallbackRunnable final : public Runnable
{
public:
  EntriesCallbackRunnable(EntriesCallback* aCallback,
                          const Sequence<RefPtr<Entry>>& aEntries)
    : mCallback(aCallback)
    , mEntries(aEntries)
  {
    MOZ_ASSERT(aCallback);
  }

  NS_IMETHOD
  Run() override
  {
    Sequence<OwningNonNull<Entry>> entries;
    for (uint32_t i = 0; i < mEntries.Length(); ++i) {
      if (!entries.AppendElement(mEntries[i].forget(), fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    mCallback->HandleEvent(entries);
    return NS_OK;
  }

private:
  RefPtr<EntriesCallback> mCallback;
  Sequence<RefPtr<Entry>> mEntries;
};

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(RootDirectoryReader, DirectoryReader, mEntries)

NS_IMPL_ADDREF_INHERITED(RootDirectoryReader, DirectoryReader)
NS_IMPL_RELEASE_INHERITED(RootDirectoryReader, DirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(RootDirectoryReader)
NS_INTERFACE_MAP_END_INHERITING(DirectoryReader)

RootDirectoryReader::RootDirectoryReader(nsIGlobalObject* aGlobal,
                                         DOMFileSystem* aFileSystem,
                                         const Sequence<RefPtr<Entry>>& aEntries)
  : DirectoryReader(aGlobal, aFileSystem, nullptr)
  , mEntries(aEntries)
  , mAlreadyRead(false)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aFileSystem);
}

RootDirectoryReader::~RootDirectoryReader()
{}

void
RootDirectoryReader::ReadEntries(EntriesCallback& aSuccessCallback,
                                 const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                                 ErrorResult& aRv)
{
  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
      new EmptyEntriesCallbackRunnable(&aSuccessCallback);
    aRv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(aRv.Failed());
    return;
  }

  // This object can be used only once.
  mAlreadyRead = true;

  RefPtr<EntriesCallbackRunnable> runnable =
    new EntriesCallbackRunnable(&aSuccessCallback, mEntries);
  aRv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(aRv.Failed());
}

} // dom namespace
} // mozilla namespace
