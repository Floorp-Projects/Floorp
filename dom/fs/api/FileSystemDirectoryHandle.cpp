/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDirectoryHandle.h"

#include "FileSystemDirectoryIteratorFactory.h"
#include "fs/FileSystemRequestHandler.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemDirectoryHandleBinding.h"
#include "mozilla/dom/FileSystemDirectoryIterator.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/StorageManager.h"
#include "nsJSUtils.h"

namespace mozilla::dom {

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    const fs::FileSystemEntryMetadata& aMetadata,
    fs::FileSystemRequestHandler* aRequestHandler)
    : FileSystemHandle(aGlobal, aManager, aMetadata, aRequestHandler) {}

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    const fs::FileSystemEntryMetadata& aMetadata)
    : FileSystemDirectoryHandle(aGlobal, aManager, aMetadata,
                                new fs::FileSystemRequestHandler()) {}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemDirectoryHandle,
                                               FileSystemHandle)
NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemDirectoryHandle, FileSystemHandle)

// WebIDL Boilerplate

JSObject* FileSystemDirectoryHandle::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return FileSystemDirectoryHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

FileSystemHandleKind FileSystemDirectoryHandle::Kind() const {
  return FileSystemHandleKind::Directory;
}

void FileSystemDirectoryHandle::InitAsyncIterator(
    FileSystemDirectoryHandle::iterator_t* aIterator, ErrorResult& aError) {
  aIterator->SetData(
      static_cast<void*>(fs::FileSystemDirectoryIteratorFactory::Create(
                             mMetadata, aIterator->GetIteratorType())
                             .release()));
}

void FileSystemDirectoryHandle::DestroyAsyncIterator(
    FileSystemDirectoryHandle::iterator_t* aIterator) {
  auto* it =
      static_cast<FileSystemDirectoryIterator::Impl*>(aIterator->GetData());
  delete it;
  aIterator->SetData(nullptr);
}

already_AddRefed<Promise> FileSystemDirectoryHandle::GetNextPromise(
    JSContext* /* aCx */, FileSystemDirectoryHandle::iterator_t* aIterator,
    ErrorResult& aError) {
  return static_cast<FileSystemDirectoryIterator::Impl*>(aIterator->GetData())
      ->Next(mGlobal, mManager, aError);
}

already_AddRefed<Promise> FileSystemDirectoryHandle::GetFileHandle(
    const nsAString& aName, const FileSystemGetFileOptions& aOptions,
    ErrorResult& aError) {
  MOZ_ASSERT(!mMetadata.entryId().IsEmpty());

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  fs::Name name(aName);
  fs::FileSystemChildMetadata metadata(mMetadata.entryId(), name);
  mRequestHandler->GetFileHandle(mManager, metadata, aOptions.mCreate, promise);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::GetDirectoryHandle(
    const nsAString& aName, const FileSystemGetDirectoryOptions& aOptions,
    ErrorResult& aError) {
  MOZ_ASSERT(!mMetadata.entryId().IsEmpty());

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  fs::Name name(aName);
  fs::FileSystemChildMetadata metadata(mMetadata.entryId(), name);
  mRequestHandler->GetDirectoryHandle(mManager, metadata, aOptions.mCreate,
                                      promise);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::RemoveEntry(
    const nsAString& aName, const FileSystemRemoveOptions& aOptions,
    ErrorResult& aError) {
  MOZ_ASSERT(!mMetadata.entryId().IsEmpty());

  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  fs::Name name(aName);
  fs::FileSystemChildMetadata metadata(mMetadata.entryId(), name);

  mRequestHandler->RemoveEntry(mManager, metadata, aOptions.mRecursive,
                               promise);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemDirectoryHandle::Resolve(
    FileSystemHandle& aPossibleDescendant, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

// [Serializable] implementation

// static
already_AddRefed<FileSystemDirectoryHandle>
FileSystemDirectoryHandle::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  uint32_t kind = static_cast<uint32_t>(FileSystemHandleKind::EndGuard_);

  if (!JS_ReadBytes(aReader, reinterpret_cast<void*>(&kind),
                    sizeof(uint32_t))) {
    return nullptr;
  }

  if (kind != static_cast<uint32_t>(FileSystemHandleKind::Directory)) {
    return nullptr;
  }

  RefPtr<FileSystemDirectoryHandle> result =
      FileSystemHandle::ConstructDirectoryHandle(aCx, aGlobal, aReader);
  if (!result) {
    return nullptr;
  }

  return result.forget();
}

}  // namespace mozilla::dom
