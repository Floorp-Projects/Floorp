/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryReader.h"
#include "CallbackRunnables.h"
#include "FileSystemFileEntry.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

namespace mozilla {
namespace dom {

namespace {

class PromiseHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  PromiseHandler(FileSystemDirectoryEntry* aParentEntry,
                 FileSystem* aFileSystem,
                 FileSystemEntriesCallback* aSuccessCallback,
                 ErrorCallback* aErrorCallback)
      : mParentEntry(aParentEntry),
        mFileSystem(aFileSystem),
        mSuccessCallback(aSuccessCallback),
        mErrorCallback(aErrorCallback) {
    MOZ_ASSERT(aParentEntry);
    MOZ_ASSERT(aFileSystem);
    MOZ_ASSERT(aSuccessCallback);
  }

  MOZ_CAN_RUN_SCRIPT
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    if (NS_WARN_IF(!aValue.isObject())) {
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

      if (NS_WARN_IF(!value.isObject())) {
        return;
      }

      JS::Rooted<JSObject*> valueObj(aCx, &value.toObject());

      RefPtr<File> file;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(File, valueObj, file))) {
        RefPtr<FileSystemFileEntry> entry = new FileSystemFileEntry(
            mParentEntry->GetParentObject(), file, mParentEntry, mFileSystem);
        sequence[i] = entry;
        continue;
      }

      RefPtr<Directory> directory;
      if (NS_WARN_IF(
              NS_FAILED(UNWRAP_OBJECT(Directory, valueObj, directory)))) {
        return;
      }

      RefPtr<FileSystemDirectoryEntry> entry =
          new FileSystemDirectoryEntry(mParentEntry->GetParentObject(),
                                       directory, mParentEntry, mFileSystem);
      sequence[i] = entry;
    }

    mSuccessCallback->Call(sequence);
  }

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    if (mErrorCallback) {
      RefPtr<ErrorCallbackRunnable> runnable = new ErrorCallbackRunnable(
          mParentEntry->GetParentObject(), mErrorCallback,
          NS_ERROR_DOM_INVALID_STATE_ERR);

      FileSystemUtils::DispatchRunnable(mParentEntry->GetParentObject(),
                                        runnable.forget());
    }
  }

 private:
  ~PromiseHandler() {}

  RefPtr<FileSystemDirectoryEntry> mParentEntry;
  RefPtr<FileSystem> mFileSystem;
  const RefPtr<FileSystemEntriesCallback> mSuccessCallback;
  RefPtr<ErrorCallback> mErrorCallback;
};

NS_IMPL_ISUPPORTS0(PromiseHandler);

}  // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemDirectoryReader, mParentEntry,
                                      mDirectory, mFileSystem)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemDirectoryReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemDirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemDirectoryReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FileSystemDirectoryReader::FileSystemDirectoryReader(
    FileSystemDirectoryEntry* aParentEntry, FileSystem* aFileSystem,
    Directory* aDirectory)
    : mParentEntry(aParentEntry),
      mFileSystem(aFileSystem),
      mDirectory(aDirectory),
      mAlreadyRead(false) {
  MOZ_ASSERT(aParentEntry);
  MOZ_ASSERT(aFileSystem);
}

FileSystemDirectoryReader::~FileSystemDirectoryReader() {}

JSObject* FileSystemDirectoryReader::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemDirectoryReader_Binding::Wrap(aCx, this, aGivenProto);
}

void FileSystemDirectoryReader::ReadEntries(
    FileSystemEntriesCallback& aSuccessCallback,
    const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
    ErrorResult& aRv) {
  MOZ_ASSERT(mDirectory);

  if (mAlreadyRead) {
    RefPtr<EmptyEntriesCallbackRunnable> runnable =
        new EmptyEntriesCallbackRunnable(&aSuccessCallback);

    FileSystemUtils::DispatchRunnable(GetParentObject(), runnable.forget());
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

  RefPtr<PromiseHandler> handler = new PromiseHandler(
      mParentEntry, mFileSystem, &aSuccessCallback,
      aErrorCallback.WasPassed() ? &aErrorCallback.Value() : nullptr);
  promise->AppendNativeHandler(handler);
}

}  // namespace dom
}  // namespace mozilla
