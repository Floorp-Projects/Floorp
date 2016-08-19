/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallbackRunnables.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileSystemDirectoryReaderBinding.h"
#include "mozilla/dom/FileSystemFileEntry.h"
#include "mozilla/dom/Promise.h"
#include "nsIGlobalObject.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

EntryCallbackRunnable::EntryCallbackRunnable(FileSystemEntryCallback* aCallback,
                                             FileSystemEntry* aEntry)
  : mCallback(aCallback)
  , mEntry(aEntry)
{
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aEntry);
}

NS_IMETHODIMP
EntryCallbackRunnable::Run()
{
  mCallback->HandleEvent(*mEntry);
  return NS_OK;
}

ErrorCallbackRunnable::ErrorCallbackRunnable(nsIGlobalObject* aGlobalObject,
                                             ErrorCallback* aCallback,
                                             nsresult aError)
  : mGlobal(aGlobalObject)
  , mCallback(aCallback)
  , mError(aError)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(NS_FAILED(aError));
}

NS_IMETHODIMP
ErrorCallbackRunnable::Run()
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DOMError> error = new DOMError(window, mError);
  mCallback->HandleEvent(*error);
  return NS_OK;
}

EmptyEntriesCallbackRunnable::EmptyEntriesCallbackRunnable(FileSystemEntriesCallback* aCallback)
  : mCallback(aCallback)
{
  MOZ_ASSERT(aCallback);
}

NS_IMETHODIMP
EmptyEntriesCallbackRunnable::Run()
{
  Sequence<OwningNonNull<FileSystemEntry>> sequence;
  mCallback->HandleEvent(sequence);
  return NS_OK;
}

GetEntryHelper::GetEntryHelper(nsIGlobalObject* aGlobalObject,
                               FileSystem* aFileSystem,
                               FileSystemEntryCallback* aSuccessCallback,
                               ErrorCallback* aErrorCallback,
                               FileSystemDirectoryEntry::GetInternalType aType)
  : mGlobal(aGlobalObject)
  , mFileSystem(aFileSystem)
  , mSuccessCallback(aSuccessCallback)
  , mErrorCallback(aErrorCallback)
  , mType(aType)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aSuccessCallback || aErrorCallback);
}

GetEntryHelper::~GetEntryHelper()
{}

void
GetEntryHelper::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  if(NS_WARN_IF(!aValue.isObject())) {
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  if (mType == FileSystemDirectoryEntry::eGetFile) {
    RefPtr<File> file;
    if (NS_FAILED(UNWRAP_OBJECT(File, obj, file))) {
      Error(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
    }

    RefPtr<FileSystemFileEntry> entry =
      new FileSystemFileEntry(mGlobal, file, mFileSystem);
    mSuccessCallback->HandleEvent(*entry);
    return;
  }

  MOZ_ASSERT(mType == FileSystemDirectoryEntry::eGetDirectory);

  RefPtr<Directory> directory;
  if (NS_FAILED(UNWRAP_OBJECT(Directory, obj, directory))) {
    Error(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
    return;
  }

  RefPtr<FileSystemDirectoryEntry> entry =
    new FileSystemDirectoryEntry(mGlobal, directory, mFileSystem);
  mSuccessCallback->HandleEvent(*entry);
}

void
GetEntryHelper::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  Error(NS_ERROR_DOM_NOT_FOUND_ERR);
}

void
GetEntryHelper::Error(nsresult aError)
{
  MOZ_ASSERT(NS_FAILED(aError));

  if (mErrorCallback) {
    RefPtr<ErrorCallbackRunnable> runnable =
      new ErrorCallbackRunnable(mGlobal, mErrorCallback, aError);
    nsresult rv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

NS_IMPL_ISUPPORTS0(GetEntryHelper);

/* static */ void
ErrorCallbackHelper::Call(nsIGlobalObject* aGlobal,
                          const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                          nsresult aError)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(NS_FAILED(aError));

  if (aErrorCallback.WasPassed()) {
    RefPtr<ErrorCallbackRunnable> runnable =
      new ErrorCallbackRunnable(aGlobal, &aErrorCallback.Value(), aError);
    nsresult rv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

} // dom namespace
} // mozilla namespace

