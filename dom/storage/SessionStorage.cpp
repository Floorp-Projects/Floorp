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
#include "nsIWebProgressListener.h"
#include "nsPIDOMWindow.h"

#define DATASET                                          \
  IsSessionOnly() ? SessionStorageCache::eSessionSetType \
                  : SessionStorageCache::eDefaultSetType

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(SessionStorage, Storage, mManager);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStorage)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(SessionStorage, Storage)
NS_IMPL_RELEASE_INHERITED(SessionStorage, Storage)

SessionStorage::SessionStorage(nsPIDOMWindowInner* aWindow,
                               nsIPrincipal* aPrincipal,
                               SessionStorageCache* aCache,
                               SessionStorageManager* aManager,
                               const nsAString& aDocumentURI, bool aIsPrivate)
    : Storage(aWindow, aPrincipal, aPrincipal),
      mCache(aCache),
      mManager(aManager),
      mDocumentURI(aDocumentURI),
      mIsPrivate(aIsPrivate) {
  MOZ_ASSERT(aCache);
}

SessionStorage::~SessionStorage() {}

already_AddRefed<SessionStorage> SessionStorage::Clone() const {
  MOZ_ASSERT(Principal() == StoragePrincipal());
  RefPtr<SessionStorage> storage =
      new SessionStorage(GetParentObject(), Principal(), mCache, mManager,
                         mDocumentURI, mIsPrivate);
  return storage.forget();
}

int64_t SessionStorage::GetOriginQuotaUsage() const {
  return mCache->GetOriginQuotaUsage(DATASET);
}

uint32_t SessionStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
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

  mCache->Key(DATASET, aIndex, aResult);
}

void SessionStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
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

  mCache->GetKeys(DATASET, aKeys);
}

void SessionStorage::SetItem(const nsAString& aKey, const nsAString& aValue,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsString oldValue;
  nsresult rv = mCache->SetItem(DATASET, aKey, aValue, oldValue);
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

  nsString oldValue;
  nsresult rv = mCache->RemoveItem(DATASET, aKey, oldValue);
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

  mCache->Clear(DATASET);
  BroadcastChangeNotification(VoidString(), VoidString(), VoidString());
}

void SessionStorage::BroadcastChangeNotification(const nsAString& aKey,
                                                 const nsAString& aOldValue,
                                                 const nsAString& aNewValue) {
  NotifyChange(this, Principal(), aKey, aOldValue, aNewValue, u"sessionStorage",
               mDocumentURI, mIsPrivate, false);
}

bool SessionStorage::IsForkOf(const Storage* aOther) const {
  MOZ_ASSERT(aOther);
  if (aOther->Type() != eSessionStorage) {
    return false;
  }

  return mCache == static_cast<const SessionStorage*>(aOther)->mCache;
}

}  // namespace dom
}  // namespace mozilla
