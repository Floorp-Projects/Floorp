/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemFileHandle.h"

#include "fs/FileSystemRequestHandler.h"
#include "js/StructuredClone.h"
#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemFileHandleBinding.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(FileSystemFileHandle,
                                               FileSystemHandle)
NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemFileHandle, FileSystemHandle)

FileSystemFileHandle::FileSystemFileHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    const fs::FileSystemEntryMetadata& aMetadata,
    fs::FileSystemRequestHandler* aRequestHandler)
    : FileSystemHandle(aGlobal, aManager, aMetadata, aRequestHandler) {}

FileSystemFileHandle::FileSystemFileHandle(
    nsIGlobalObject* aGlobal, RefPtr<FileSystemManager>& aManager,
    const fs::FileSystemEntryMetadata& aMetadata)
    : FileSystemFileHandle(aGlobal, aManager, aMetadata,
                           new fs::FileSystemRequestHandler()) {}

// WebIDL Boilerplate

JSObject* FileSystemFileHandle::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return FileSystemFileHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

FileSystemHandleKind FileSystemFileHandle::Kind() const {
  return FileSystemHandleKind::File;
}

already_AddRefed<Promise> FileSystemFileHandle::GetFile(ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  mRequestHandler->GetFile(mManager, mMetadata, promise);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemFileHandle::CreateWritable(
    const FileSystemCreateWritableOptions& aOptions, ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

already_AddRefed<Promise> FileSystemFileHandle::CreateSyncAccessHandle(
    ErrorResult& aError) {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  mRequestHandler->GetAccessHandle(mManager, mMetadata, promise);

  return promise.forget();
}

// [Serializable] implementation

// static
already_AddRefed<FileSystemFileHandle>
FileSystemFileHandle::ReadStructuredClone(JSContext* aCx,
                                          nsIGlobalObject* aGlobal,
                                          JSStructuredCloneReader* aReader) {
  uint32_t kind = static_cast<uint32_t>(FileSystemHandleKind::EndGuard_);

  if (!JS_ReadBytes(aReader, reinterpret_cast<void*>(&kind),
                    sizeof(uint32_t))) {
    return nullptr;
  }

  if (kind != static_cast<uint32_t>(FileSystemHandleKind::File)) {
    return nullptr;
  }

  RefPtr<FileSystemFileHandle> result =
      FileSystemHandle::ConstructFileHandle(aCx, aGlobal, aReader);
  if (!result) {
    return nullptr;
  }

  return result.forget();
}

}  // namespace mozilla::dom
