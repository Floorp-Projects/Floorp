/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileEntry.h"
#include "CallbackRunnables.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

namespace {

class BlobCallbackRunnable final : public Runnable
{
public:
  BlobCallbackRunnable(BlobCallback* aCallback, File* aFile)
    : mCallback(aCallback)
    , mFile(aFile)
  {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aFile);
  }

  NS_IMETHOD
  Run() override
  {
    mCallback->HandleEvent(mFile);
    return NS_OK;
  }

private:
  RefPtr<BlobCallback> mCallback;
  RefPtr<File> mFile;
};

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileEntry, Entry, mFile)

NS_IMPL_ADDREF_INHERITED(FileEntry, Entry)
NS_IMPL_RELEASE_INHERITED(FileEntry, Entry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileEntry)
NS_INTERFACE_MAP_END_INHERITING(Entry)

FileEntry::FileEntry(nsIGlobalObject* aGlobal,
                     File* aFile,
                     DOMFileSystem* aFileSystem)
  : Entry(aGlobal, aFileSystem)
  , mFile(aFile)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(mFile);
}

FileEntry::~FileEntry()
{}

JSObject*
FileEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileEntryBinding::Wrap(aCx, this, aGivenProto);
}

void
FileEntry::GetName(nsAString& aName, ErrorResult& aRv) const
{
  mFile->GetName(aName);
}

void
FileEntry::GetFullPath(nsAString& aPath, ErrorResult& aRv) const
{
  mFile->GetPath(aPath);
}

void
FileEntry::CreateWriter(VoidCallback& aSuccessCallback,
                        const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const
{
  ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                            NS_ERROR_DOM_SECURITY_ERR);
}

void
FileEntry::GetFile(BlobCallback& aSuccessCallback,
                   const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const
{
  RefPtr<BlobCallbackRunnable> runnable =
    new BlobCallbackRunnable(&aSuccessCallback, mFile);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

} // dom namespace
} // mozilla namespace
