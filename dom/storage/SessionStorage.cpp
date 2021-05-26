/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorage.h"

#include "SessionStorageCache.h"
#include "SessionStorageManager.h"

#include "mozilla/dom/StorageBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"

#define DATASET                                    \
  (!IsPrivateBrowsing() && IsSessionScopedOrLess() \
       ? SessionStorageCache::eSessionSetType      \
       : SessionStorageCache::eDefaultSetType)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(SessionStorage, Storage, mManager);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStorage)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(SessionStorage, Storage)
NS_IMPL_RELEASE_INHERITED(SessionStorage, Storage)

SessionStorage::SessionStorage(nsPIDOMWindowInner* aWindow,
                               nsIPrincipal* aPrincipal,
                               nsIPrincipal* aStoragePrincipal,
                               SessionStorageCache* aCache,
                               SessionStorageManager* aManager,
                               const nsAString& aDocumentURI, bool aIsPrivate)
    : Storage(aWindow, aPrincipal, aStoragePrincipal),
      mCache(aCache),
      mManager(aManager),
      mDocumentURI(aDocumentURI),
      mIsPrivate(aIsPrivate),
      mHasPendingStableStateCallback(false) {
  MOZ_ASSERT(aCache);
}

SessionStorage::~SessionStorage() = default;

int64_t SessionStorage::GetOriginQuotaUsage() const {
  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return 0;
  }

  return mCache->GetOriginQuotaUsage(DATASET);
}

uint32_t SessionStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return 0;
  }

  return mCache->Length(DATASET);
}

void SessionStorage::Key(uint32_t aIndex, nsAString& aResult,
                         nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  mCache->Key(DATASET, aIndex, aResult);
}

void SessionStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  mCache->GetItem(DATASET, aKey, aResult);
}

void SessionStorage::GetSupportedNames(nsTArray<nsString>& aKeys) {
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(DATASET, aKeys);
}

void SessionStorage::SetItem(const nsAString& aKey, const nsAString& aValue,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsString oldValue;
  rv = mCache->SetItem(DATASET, aKey, aValue, oldValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return;
  }

  BroadcastChangeNotification(aKey, oldValue, aValue);
}

void SessionStorage::RemoveItem(const nsAString& aKey,
                                nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsString oldValue;
  rv = mCache->RemoveItem(DATASET, aKey, oldValue);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return;
  }

  BroadcastChangeNotification(aKey, oldValue, VoidString());
}

void SessionStorage::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  uint32_t length = GetLength(aSubjectPrincipal, aRv);
  if (!length) {
    return;
  }

  nsresult rv = EnsureCacheLoadedOrCloned();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  mCache->Clear(DATASET);
  BroadcastChangeNotification(VoidString(), VoidString(), VoidString());
}

void SessionStorage::BroadcastChangeNotification(const nsAString& aKey,
                                                 const nsAString& aOldValue,
                                                 const nsAString& aNewValue) {
  NotifyChange(this, StoragePrincipal(), aKey, aOldValue, aNewValue,
               u"sessionStorage", mDocumentURI, mIsPrivate, false);

  // Sync changes on SessionStorageCache at the next statble state.
  if (mManager->CanLoadData()) {
    MaybeScheduleStableStateCallback();
  }
}

bool SessionStorage::IsForkOf(const Storage* aOther) const {
  MOZ_ASSERT(aOther);
  if (aOther->Type() != eSessionStorage) {
    return false;
  }

  return mCache == static_cast<const SessionStorage*>(aOther)->mCache;
}

void SessionStorage::MaybeScheduleStableStateCallback() {
  AssertIsOnOwningThread();

  if (!mHasPendingStableStateCallback) {
    nsContentUtils::RunInStableState(
        NewRunnableMethod("SessionStorage::StableStateCallback", this,
                          &SessionStorage::StableStateCallback));

    mHasPendingStableStateCallback = true;
  }
}

void SessionStorage::StableStateCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mHasPendingStableStateCallback);
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mCache);

  mHasPendingStableStateCallback = false;

  if (mManager->CanLoadData()) {
    mManager->CheckpointData(*StoragePrincipal(), *mCache);
  }
}

nsresult SessionStorage::EnsureCacheLoadedOrCloned() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mManager);

  if (!mManager->CanLoadData()) {
    return NS_OK;
  }

  // Ensure manager actor.
  nsresult rv = mManager->EnsureManager();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Ensure cache is loaded or cloned.
  if (mCache->WasLoadedOrCloned()) {
    return NS_OK;
  }

  return mManager->LoadData(*StoragePrincipal(), *mCache);
}

}  // namespace dom
}  // namespace mozilla
