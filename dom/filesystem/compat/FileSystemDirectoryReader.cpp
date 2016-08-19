/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryReader.h"
#include "CallbackRunnables.h"
#include "FileSystemFileEntry.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

namespace {

class PromiseHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  PromiseHandler(nsIGlobalObject* aGlobalObject,
                 FileSystem* aFileSystem,
                 FileSystemEntriesCallback* aSuccessCallback,
                 ErrorCallback* aErrorCallback)
    : mGlobal(aGlobalObject)
    , mFileSystem(aFileSystem)
    , mSuccessCallback(aSuccessCallback)
    , mErrorCallback(aErrorCallback)
  {
    MOZ_ASSERT(aGlobalObject);
    MOZ_ASSERT(aFileSystem);
    MOZ_ASSERT(aSuccessCallback);
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if(NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

    uint32_t length;
    if (NS_WARN_IF(!JS_GetArrayLength(aCx, obj, &length))) {
      return;
    }

    Sequence<OwningNonNull<FileSystemEntry>> sequence;
    if (NS_WARN_IF(!sequence.SetLength(length, fallible))) {
      return;
    }

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, obj, i, &value))) {
        return;
      }

      if(NS_WARN_IF(!value.isObject())) {
        return;
      }

      JS::Rooted<JSObject*> valueObj(aCx, &value.toObject());

      RefPtr<File> file;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(File, valueObj, file))) {
        RefPtr<FileSystemFileEntry> entry =
          new FileSystemFileEntry(mGlobal, file, mFileSystem);
        sequence[i] = entry;
        continue;
      }

      RefPtr<Directory> directory;
      if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Directory, valueObj,
                                             directory)))) {
        return;
      }

      RefPtr<FileSystemDirectoryEntry> entry =
        new FileSystemDirectoryEntry(mGlobal, directory, mFileSystem);
      sequence[i] = entry;
    }

    mSuccessCallback->HandleEvent(sequence);
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (mErrorCallback) {
      RefPtr<ErrorCallbackRunnable> runnable =
        new ErrorCallbackRunnable(mGlobal, mErrorCallback,
                                  NS_ERROR_DOM_INVALID_STATE_ERR);
      nsresult rv = NS_DispatchToMainThread(runnable);
      NS_WARN_IF(NS_FAILED(rv));
    }
  }

private:
  ~PromiseHandler() {}

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FileSystem> mFileSystem;
  RefPtr<FileSystemEntriesCallback> mSuccessCallback;
  RefPtr<ErrorCallback> mErrorCallback;
};

NS_IMPL_ISUPPORTS0(PromiseHandler);

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemDirectoryReader, mParent,
                                      mDirectory, mFileSystem)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemDirectoryReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemDirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemDirectoryReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FileSystemDirectoryReader::FileSystemDirectoryReader(nsIGlobalObject* aGlobal,
                                                     FileSystem* aFileSystem,
                                                     Directory* aDirectory)
  : mParent(aGlobal)
  , mFileSystem(aFileSystem)
  , mDirectory(aDirectory)
  , mAlreadyRead(false)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aFileSystem);
}

FileSystemDirectoryReader::~FileSystemDirectoryReader()
{}

JSObject*
FileSystemDirectoryReader::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemDirectoryReaderBinding::Wrap(aCx, this, aGivenProto);
}

void
FileSystemDirectoryReader::ReadEntries(FileSystemEntriesCallback& aSuccessCallback,
                                       const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                                       ErrorResult& aRv)
{
  MOZ_ASSERT(mDirectory);

  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
      new EmptyEntriesCallbackRunnable(&aSuccessCallback);
    aRv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(aRv.Failed());
    return;
  }

  // This object can be used only once.
  mAlreadyRead = true;

  ErrorResult rv;
  RefPtr<Promise> promise = mDirectory->GetFilesAndDirectories(rv);
  if (NS_WARN_IF(rv.Failed())) {
    ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                              rv.StealNSResult());
    return;
  }

  RefPtr<PromiseHandler> handler =
    new PromiseHandler(GetParentObject(), mFileSystem, &aSuccessCallback,
                       aErrorCallback.WasPassed()
                         ? &aErrorCallback.Value() : nullptr);
  promise->AppendNativeHandler(handler);
}

} // dom namespace
} // mozilla namespace
