/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHandle.h"
#include "fs/FileSystemRequestHandler.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FileSystemHandleBinding.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemHandle);
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemHandle);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemHandle, mGlobal);

FileSystemHandle::FileSystemHandle(
    nsIGlobalObject* aGlobal, const fs::FileSystemEntryMetadata& aMetadata,
    fs::FileSystemRequestHandler* aRequestHandler)
    : mGlobal(aGlobal),
      mMetadata(aMetadata),
      mRequestHandler(aRequestHandler) {}

// WebIDL Boilerplate

nsIGlobalObject* FileSystemHandle::GetParentObject() const { return mGlobal; }

JSObject* FileSystemHandle::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return FileSystemHandle_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

void FileSystemHandle::GetName(nsAString& aResult) {
  aResult = mMetadata.entryName();
}

already_AddRefed<Promise> FileSystemHandle::IsSameEntry(
    FileSystemHandle& aOther, ErrorResult& aError) const {
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aError);
  if (aError.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_IMPLEMENTED);

  return promise.forget();
}

}  // namespace mozilla::dom
