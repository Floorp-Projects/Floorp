/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemStreamCallbacks.h"

namespace mozilla::dom {

bool FileSystemStreamCallbacks::HasNoRemoteQuotaObjectParents() const {
  return !mRemoteQuotaObjectParents.Count();
}

void FileSystemStreamCallbacks::SetRemoteQuotaObjectParentCallback(
    std::function<void()>&& aCallback) {
  mRemoteQuotaObjectParentCallback = std::move(aCallback);
}

NS_IMPL_ISUPPORTS(FileSystemStreamCallbacks, nsIInterfaceRequestor,
                  quota::RemoteQuotaObjectParentTracker)

NS_IMETHODIMP
FileSystemStreamCallbacks::GetInterface(const nsIID& aIID, void** aResult) {
  return QueryInterface(aIID, aResult);
}

void FileSystemStreamCallbacks::RegisterRemoteQuotaObjectParent(
    NotNull<quota::RemoteQuotaObjectParent*> aActor) {
  MOZ_ASSERT(!mRemoteQuotaObjectParents.Contains(aActor));

  mRemoteQuotaObjectParents.Insert(aActor);
}

void FileSystemStreamCallbacks::UnregisterRemoteQuotaObjectParent(
    NotNull<quota::RemoteQuotaObjectParent*> aActor) {
  MOZ_ASSERT(mRemoteQuotaObjectParents.Contains(aActor));

  mRemoteQuotaObjectParents.Remove(aActor);

  if (!mRemoteQuotaObjectParents.Count() && mRemoteQuotaObjectParentCallback) {
    mRemoteQuotaObjectParentCallback();
  }
}

}  // namespace mozilla::dom
