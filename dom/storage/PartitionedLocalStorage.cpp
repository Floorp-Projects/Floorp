/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartitionedLocalStorage.h"
#include "SessionStorageCache.h"
#include "nsContentUtils.h"

#include "mozilla/dom/StorageBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PartitionedLocalStorage, Storage);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PartitionedLocalStorage)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(PartitionedLocalStorage, Storage)
NS_IMPL_RELEASE_INHERITED(PartitionedLocalStorage, Storage)

PartitionedLocalStorage::PartitionedLocalStorage(
    nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
    nsIPrincipal* aStoragePrincipal, SessionStorageCache* aCache)
    : Storage(aWindow, aPrincipal, aStoragePrincipal), mCache(aCache) {}

PartitionedLocalStorage::~PartitionedLocalStorage() = default;

int64_t PartitionedLocalStorage::GetOriginQuotaUsage() const {
  return mCache->GetOriginQuotaUsage();
}

uint32_t PartitionedLocalStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                                            ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  return mCache->Length();
}

void PartitionedLocalStorage::Key(uint32_t aIndex, nsAString& aResult,
                                  nsIPrincipal& aSubjectPrincipal,
                                  ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mCache->Key(aIndex, aResult);
}

void PartitionedLocalStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                                      nsIPrincipal& aSubjectPrincipal,
                                      ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mCache->GetItem(aKey, aResult);
}

void PartitionedLocalStorage::GetSupportedNames(nsTArray<nsString>& aKeys) {
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(aKeys);
}

void PartitionedLocalStorage::SetItem(const nsAString& aKey,
                                      const nsAString& aValue,
                                      nsIPrincipal& aSubjectPrincipal,
                                      ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsString oldValue;
  nsresult rv = mCache->SetItem(aKey, aValue, oldValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return;
  }
}

void PartitionedLocalStorage::RemoveItem(const nsAString& aKey,
                                         nsIPrincipal& aSubjectPrincipal,
                                         ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsString oldValue;
  nsresult rv = mCache->RemoveItem(aKey, oldValue);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return;
  }
}

void PartitionedLocalStorage::Clear(nsIPrincipal& aSubjectPrincipal,
                                    ErrorResult& aRv) {
  uint32_t length = GetLength(aSubjectPrincipal, aRv);
  if (!length) {
    return;
  }

  mCache->Clear();
}

bool PartitionedLocalStorage::IsForkOf(const Storage* aOther) const {
  MOZ_ASSERT(aOther);
  if (aOther->Type() != eLocalStorage) {
    return false;
  }

  return mCache == static_cast<const PartitionedLocalStorage*>(aOther)->mCache;
}

}  // namespace dom
}  // namespace mozilla
