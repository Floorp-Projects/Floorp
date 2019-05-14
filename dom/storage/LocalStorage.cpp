/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorage.h"
#include "LocalStorageCache.h"
#include "LocalStorageManager.h"
#include "StorageUtils.h"

#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsICookiePermission.h"

#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/StorageBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/EnumSet.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(LocalStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(LocalStorage, Storage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(LocalStorage, Storage)
  CycleCollectionNoteChild(
      cb, NS_ISUPPORTS_CAST(nsIDOMStorageManager*, tmp->mManager.get()),
      "mManager");
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LocalStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(LocalStorage, Storage)
NS_IMPL_RELEASE_INHERITED(LocalStorage, Storage)

LocalStorage::LocalStorage(nsPIDOMWindowInner* aWindow,
                           LocalStorageManager* aManager,
                           LocalStorageCache* aCache,
                           const nsAString& aDocumentURI,
                           nsIPrincipal* aPrincipal,
                           nsIPrincipal* aStoragePrincipal, bool aIsPrivate)
    : Storage(aWindow, aPrincipal, aStoragePrincipal),
      mManager(aManager),
      mCache(aCache),
      mDocumentURI(aDocumentURI),
      mIsPrivate(aIsPrivate) {
  mCache->Preload();
}

LocalStorage::~LocalStorage() {}

int64_t LocalStorage::GetOriginQuotaUsage() const {
  return mCache->GetOriginQuotaUsage(this);
}

uint32_t LocalStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  uint32_t length;
  aRv = mCache->GetLength(this, &length);
  return length;
}

void LocalStorage::Key(uint32_t aIndex, nsAString& aResult,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetKey(this, aIndex, aResult);
}

void LocalStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetItem(this, aKey, aResult);
}

void LocalStorage::SetItem(const nsAString& aKey, const nsAString& aData,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsString data;
  bool ok = data.Assign(aData, fallible);
  if (!ok) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsString old;
  aRv = mCache->SetItem(this, aKey, data, old);
  if (aRv.Failed()) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    OnChange(aKey, old, aData);
  }
}

void LocalStorage::RemoveItem(const nsAString& aKey,
                              nsIPrincipal& aSubjectPrincipal,
                              ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsAutoString old;
  aRv = mCache->RemoveItem(this, aKey, old);
  if (aRv.Failed()) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    OnChange(aKey, old, VoidString());
  }
}

void LocalStorage::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->Clear(this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    OnChange(VoidString(), VoidString(), VoidString());
  }
}

void LocalStorage::OnChange(const nsAString& aKey, const nsAString& aOldValue,
                            const nsAString& aNewValue) {
  NotifyChange(/* aStorage */ this, StoragePrincipal(), aKey, aOldValue,
               aNewValue, /* aStorageType */ u"localStorage", mDocumentURI,
               mIsPrivate, /* aImmediateDispatch */ false);
}

void LocalStorage::ApplyEvent(StorageEvent* aStorageEvent) {
  MOZ_ASSERT(aStorageEvent);

  nsAutoString key;
  nsAutoString old;
  nsAutoString value;

  aStorageEvent->GetKey(key);
  aStorageEvent->GetNewValue(value);

  // No key means clearing the full storage.
  if (key.IsVoid()) {
    MOZ_ASSERT(value.IsVoid());
    mCache->Clear(this, LocalStorageCache::E10sPropagated);
    return;
  }

  // No new value means removing the key.
  if (value.IsVoid()) {
    mCache->RemoveItem(this, key, old, LocalStorageCache::E10sPropagated);
    return;
  }

  // Otherwise, we set the new value.
  mCache->SetItem(this, key, value, old, LocalStorageCache::E10sPropagated);
}

void LocalStorage::GetSupportedNames(nsTArray<nsString>& aKeys) {
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(this, aKeys);
}

bool LocalStorage::IsForkOf(const Storage* aOther) const {
  MOZ_ASSERT(aOther);
  if (aOther->Type() != eLocalStorage) {
    return false;
  }

  return mCache == static_cast<const LocalStorage*>(aOther)->mCache;
}

}  // namespace dom
}  // namespace mozilla
