/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ErrorCallbackRunnable_h
#define mozilla_dom_ErrorCallbackRunnable_h

#include "FileSystemDirectoryEntry.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PromiseNativeHandler.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class FileSystemEntriesCallback;

class EntryCallbackRunnable final : public Runnable
{
public:
  EntryCallbackRunnable(FileSystemEntryCallback* aCallback,
                        FileSystemEntry* aEntry);

  NS_IMETHOD
  Run() override;

private:
  RefPtr<FileSystemEntryCallback> mCallback;
  RefPtr<FileSystemEntry> mEntry;
};

class ErrorCallbackRunnable final : public Runnable
{
public:
  ErrorCallbackRunnable(nsIGlobalObject* aGlobalObject,
                        ErrorCallback* aCallback,
                        nsresult aError);

  NS_IMETHOD
  Run() override;

private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ErrorCallback> mCallback;
  nsresult mError;
};

class EmptyEntriesCallbackRunnable final : public Runnable
{
public:
  explicit EmptyEntriesCallbackRunnable(FileSystemEntriesCallback* aCallback);

  NS_IMETHOD
  Run() override;

private:
  RefPtr<FileSystemEntriesCallback> mCallback;
};

class GetEntryHelper final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  GetEntryHelper(nsIGlobalObject* aGlobalObject,
                 FileSystem* aFileSystem,
                 FileSystemEntryCallback* aSuccessCallback,
                 ErrorCallback* aErrorCallback,
                 FileSystemDirectoryEntry::GetInternalType aType);

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

private:
  ~GetEntryHelper();

  void
  Error(nsresult aError);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FileSystem> mFileSystem;
  RefPtr<FileSystemEntryCallback> mSuccessCallback;
  RefPtr<ErrorCallback> mErrorCallback;
  FileSystemDirectoryEntry::GetInternalType mType;
};

class ErrorCallbackHelper
{
public:
  static void
  Call(nsIGlobalObject* aGlobal,
       const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
       nsresult aError);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_CallbackRunnables_h
