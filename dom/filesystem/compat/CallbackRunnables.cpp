/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallbackRunnables.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileSystem.h"
#include "mozilla/dom/FileSystemDirectoryReaderBinding.h"
#include "mozilla/dom/FileSystemFileEntry.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Unused.h"
#include "nsIGlobalObject.h"
#include "nsIFile.h"
#include "nsPIDOMWindow.h"

#include "../GetFileOrDirectoryTask.h"

namespace mozilla::dom {

EntryCallbackRunnable::EntryCallbackRunnable(FileSystemEntryCallback* aCallback,
                                             FileSystemEntry* aEntry)
    : Runnable("EntryCallbackRunnable"), mCallback(aCallback), mEntry(aEntry) {
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aEntry);
}

NS_IMETHODIMP
EntryCallbackRunnable::Run() {
  mCallback->Call(*mEntry);
  return NS_OK;
}

ErrorCallbackRunnable::ErrorCallbackRunnable(nsIGlobalObject* aGlobalObject,
                                             ErrorCallback* aCallback,
                                             nsresult aError)
    : Runnable("ErrorCallbackRunnable"),
      mGlobal(aGlobalObject),
      mCallback(aCallback),
      mError(aError) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(NS_FAILED(aError));
}

NS_IMETHODIMP
ErrorCallbackRunnable::Run() {
  RefPtr<DOMException> exception = DOMException::Create(mError);
  mCallback->Call(*exception);
  return NS_OK;
}

EmptyEntriesCallbackRunnable::EmptyEntriesCallbackRunnable(
    FileSystemEntriesCallback* aCallback)
    : Runnable("EmptyEntriesCallbackRunnable"), mCallback(aCallback) {
  MOZ_ASSERT(aCallback);
}

NS_IMETHODIMP
EmptyEntriesCallbackRunnable::Run() {
  Sequence<OwningNonNull<FileSystemEntry>> sequence;
  mCallback->Call(sequence);
  return NS_OK;
}

GetEntryHelper::GetEntryHelper(FileSystemDirectoryEntry* aParentEntry,
                               Directory* aDirectory,
                               nsTArray<nsString>& aParts,
                               FileSystem* aFileSystem,
                               FileSystemEntryCallback* aSuccessCallback,
                               ErrorCallback* aErrorCallback,
                               FileSystemDirectoryEntry::GetInternalType aType)
    : mParentEntry(aParentEntry),
      mDirectory(aDirectory),
      mParts(aParts.Clone()),
      mFileSystem(aFileSystem),
      mSuccessCallback(aSuccessCallback),
      mErrorCallback(aErrorCallback),
      mType(aType) {
  MOZ_ASSERT(aParentEntry);
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(!aParts.IsEmpty());
  MOZ_ASSERT(aFileSystem);
  MOZ_ASSERT(aSuccessCallback || aErrorCallback);
}

GetEntryHelper::~GetEntryHelper() = default;

namespace {

nsresult DOMPathToRealPath(Directory* aDirectory, const nsAString& aPath,
                           nsIFile** aFile) {
  nsString relativePath;
  relativePath = aPath;

  // Trim white spaces.
  static const char kWhitespace[] = "\b\t\r\n ";
  relativePath.Trim(kWhitespace);

  nsTArray<nsString> parts;
  if (!FileSystemUtils::IsValidRelativeDOMPath(relativePath, parts)) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->GetInternalNsIFile()->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t i = 0; i < parts.Length(); ++i) {
    rv = file->AppendRelativePath(parts[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  file.forget(aFile);
  return NS_OK;
}

}  // namespace

void GetEntryHelper::Run() {
  MOZ_ASSERT(!mParts.IsEmpty());

  nsCOMPtr<nsIFile> realPath;
  nsresult error =
      DOMPathToRealPath(mDirectory, mParts[0], getter_AddRefs(realPath));

  ErrorResult rv;
  RefPtr<FileSystemBase> fs = mDirectory->GetFileSystem(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    Error(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  RefPtr<GetFileOrDirectoryTaskChild> task =
      GetFileOrDirectoryTaskChild::Create(fs, realPath, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    Error(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  task->SetError(error);
  task->Start();

  RefPtr<Promise> promise = task->GetPromise();

  mParts.RemoveElementAt(0);
  promise->AppendNativeHandler(this);
}

void GetEntryHelper::ResolvedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue) {
  if (NS_WARN_IF(!aValue.isObject())) {
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

  // This is not the last part of the path.
  if (!mParts.IsEmpty()) {
    ContinueRunning(obj);
    return;
  }

  CompleteOperation(obj);
}

void GetEntryHelper::CompleteOperation(JSObject* aObj) {
  MOZ_ASSERT(mParts.IsEmpty());

  if (mType == FileSystemDirectoryEntry::eGetFile) {
    RefPtr<File> file;
    if (NS_FAILED(UNWRAP_OBJECT(File, aObj, file))) {
      Error(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
    }

    RefPtr<FileSystemFileEntry> entry = new FileSystemFileEntry(
        mParentEntry->GetParentObject(), file, mParentEntry, mFileSystem);
    mSuccessCallback->Call(*entry);
    return;
  }

  MOZ_ASSERT(mType == FileSystemDirectoryEntry::eGetDirectory);

  RefPtr<Directory> directory;
  if (NS_FAILED(UNWRAP_OBJECT(Directory, aObj, directory))) {
    Error(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
    return;
  }

  RefPtr<FileSystemDirectoryEntry> entry = new FileSystemDirectoryEntry(
      mParentEntry->GetParentObject(), directory, mParentEntry, mFileSystem);
  mSuccessCallback->Call(*entry);
}

void GetEntryHelper::ContinueRunning(JSObject* aObj) {
  MOZ_ASSERT(!mParts.IsEmpty());

  RefPtr<Directory> directory;
  if (NS_FAILED(UNWRAP_OBJECT(Directory, aObj, directory))) {
    Error(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
    return;
  }

  RefPtr<FileSystemDirectoryEntry> entry = new FileSystemDirectoryEntry(
      mParentEntry->GetParentObject(), directory, mParentEntry, mFileSystem);

  // Update the internal values.
  mParentEntry = entry;
  mDirectory = directory;

  Run();
}

void GetEntryHelper::RejectedCallback(JSContext* aCx,
                                      JS::Handle<JS::Value> aValue) {
  Error(NS_ERROR_DOM_NOT_FOUND_ERR);
}

void GetEntryHelper::Error(nsresult aError) {
  MOZ_ASSERT(NS_FAILED(aError));

  if (mErrorCallback) {
    RefPtr<ErrorCallbackRunnable> runnable = new ErrorCallbackRunnable(
        mParentEntry->GetParentObject(), mErrorCallback, aError);

    FileSystemUtils::DispatchRunnable(mParentEntry->GetParentObject(),
                                      runnable.forget());
  }
}

NS_IMPL_ISUPPORTS0(GetEntryHelper);

/* static */
void FileSystemEntryCallbackHelper::Call(
    nsIGlobalObject* aGlobalObject,
    const Optional<OwningNonNull<FileSystemEntryCallback>>& aEntryCallback,
    FileSystemEntry* aEntry) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aEntry);

  if (aEntryCallback.WasPassed()) {
    RefPtr<EntryCallbackRunnable> runnable =
        new EntryCallbackRunnable(&aEntryCallback.Value(), aEntry);

    FileSystemUtils::DispatchRunnable(aGlobalObject, runnable.forget());
  }
}

/* static */
void ErrorCallbackHelper::Call(
    nsIGlobalObject* aGlobal,
    const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
    nsresult aError) {
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(NS_FAILED(aError));

  if (aErrorCallback.WasPassed()) {
    RefPtr<ErrorCallbackRunnable> runnable =
        new ErrorCallbackRunnable(aGlobal, &aErrorCallback.Value(), aError);

    FileSystemUtils::DispatchRunnable(aGlobal, runnable.forget());
  }
}

}  // namespace mozilla::dom
