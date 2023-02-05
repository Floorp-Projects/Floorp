/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemStreamCallbacks.h"

#include "mozilla/dom/quota/RemoteQuotaObjectParent.h"

namespace mozilla::dom {

FileSystemStreamCallbacks::FileSystemStreamCallbacks()
    : mRemoteQuotaObjectParent(nullptr) {}

NS_IMPL_ISUPPORTS(FileSystemStreamCallbacks, nsIInterfaceRequestor,
                  quota::RemoteQuotaObjectParentTracker)

NS_IMETHODIMP
FileSystemStreamCallbacks::GetInterface(const nsIID& aIID, void** aResult) {
  return QueryInterface(aIID, aResult);
}

void FileSystemStreamCallbacks::RegisterRemoteQuotaObjectParent(
    NotNull<quota::RemoteQuotaObjectParent*> aActor) {
  MOZ_ASSERT(!mRemoteQuotaObjectParent);

  mRemoteQuotaObjectParent = aActor;
}

void FileSystemStreamCallbacks::UnregisterRemoteQuotaObjectParent(
    NotNull<quota::RemoteQuotaObjectParent*> aActor) {
  MOZ_ASSERT(mRemoteQuotaObjectParent);

  mRemoteQuotaObjectParent = nullptr;
}

}  // namespace mozilla::dom
