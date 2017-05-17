/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorage.h"
#include "SessionStorageManager.h"
#include "StorageManager.h"

#include "mozilla/dom/StorageBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

// ----------------------------------------------------------------------------
// SessionStorage
// ----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_INHERITED(SessionStorage, Storage, mManager);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStorage)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(SessionStorage, Storage)
NS_IMPL_RELEASE_INHERITED(SessionStorage, Storage)

SessionStorage::SessionStorage(nsPIDOMWindowInner* aWindow,
                               nsIPrincipal* aPrincipal,
                               SessionStorageCache* aCache,
                               SessionStorageManager* aManager,
                               const nsAString& aDocumentURI,
                               bool aIsPrivate)
  : Storage(aWindow, aPrincipal)
  , mCache(aCache)
  , mManager(aManager)
  , mDocumentURI(aDocumentURI)
  , mIsPrivate(aIsPrivate)
{
  MOZ_ASSERT(aCache);
}

SessionStorage::~SessionStorage()
{
}

already_AddRefed<SessionStorage>
SessionStorage::Clone() const
{
  RefPtr<SessionStorage> storage =
    new SessionStorage(GetParentObject(), Principal(), mCache, mManager,
                       mDocumentURI, mIsPrivate);
  return storage.forget();
}

int64_t
SessionStorage::GetOriginQuotaUsage() const
{
  return mCache->GetOriginQuotaUsage();
}

uint32_t
SessionStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  return mCache->Length();
}

void
SessionStorage::Key(uint32_t aIndex, nsAString& aResult,
                    nsIPrincipal& aSubjectPrincipal,
                    ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mCache->Key(aIndex, aResult);
}

void
SessionStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                        nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mCache->GetItem(aKey, aResult);
}

void
SessionStorage::GetSupportedNames(nsTArray<nsString>& aKeys)
{
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(aKeys);
}

void
SessionStorage::SetItem(const nsAString& aKey, const nsAString& aValue,
                        nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aRv)
{
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

  BroadcastChangeNotification(aKey, oldValue, aValue);
}

void
SessionStorage::RemoveItem(const nsAString& aKey,
                           nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aRv)
{
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

  BroadcastChangeNotification(aKey, oldValue, NullString());
}

void
SessionStorage::Clear(nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  uint32_t length = GetLength(aSubjectPrincipal, aRv);
  if (!length) {
    return;
  }

  mCache->Clear();
  BroadcastChangeNotification(NullString(), NullString(), NullString());
}

bool
SessionStorage::CanUseStorage(nsIPrincipal& aSubjectPrincipal)
{
  // This method is responsible for correct setting of mIsSessionOnly.
  // It doesn't work with mIsPrivate flag at all, since it is checked
  // regardless mIsSessionOnly flag in DOMStorageCache code.

  if (!mozilla::Preferences::GetBool("dom.storage.enabled")) {
    return false;
  }

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(Principal());

  if (access == nsContentUtils::StorageAccess::eDeny) {
    return false;
  }

  return CanAccess(&aSubjectPrincipal);
}

void
SessionStorage::BroadcastChangeNotification(const nsAString& aKey,
                                            const nsAString& aOldValue,
                                            const nsAString& aNewValue)
{
  // TODO
}

// ----------------------------------------------------------------------------
// SessionStorageCache
// ----------------------------------------------------------------------------

SessionStorageCache::SessionStorageCache()
  : mOriginQuotaUsage(0)
{}

void
SessionStorageCache::Key(uint32_t aIndex, nsAString& aResult)
{
  aResult.SetIsVoid(true);
  for (auto iter = mKeys.Iter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aResult = iter.Key();
      return;
    }
    aIndex--;
  }
}

void
SessionStorageCache::GetItem(const nsAString& aKey, nsAString& aResult)
{
  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }
  aResult = value;
}

void
SessionStorageCache::GetKeys(nsTArray<nsString>& aKeys)
{
  for (auto iter = mKeys.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

nsresult
SessionStorageCache::SetItem(const nsAString& aKey, const nsAString& aValue,
                             nsString& aOldValue)
{
  int64_t delta = 0;

  if (!mKeys.Get(aKey, &aOldValue)) {
    SetDOMStringToNull(aOldValue);

    // We only consider key size if the key doesn't exist before.
    delta = static_cast<int64_t>(aKey.Length());
  }

  delta += static_cast<int64_t>(aValue.Length()) -
           static_cast<int64_t>(aOldValue.Length());

  if (aValue == aOldValue &&
      DOMStringIsNull(aValue) == DOMStringIsNull(aOldValue)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  if (!ProcessUsageDelta(delta)) {
    return NS_ERROR_DOM_QUOTA_REACHED;
  }

  mKeys.Put(aKey, nsString(aValue));
  return NS_OK;
}

nsresult
SessionStorageCache::RemoveItem(const nsAString& aKey, nsString& aOldValue)
{
  if (!mKeys.Get(aKey, &aOldValue)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  ProcessUsageDelta(-(static_cast<int64_t>(aOldValue.Length()) +
                      static_cast<int64_t>(aKey.Length())));

  mKeys.Remove(aKey);
  return NS_OK;
}

void
SessionStorageCache::Clear()
{
  ProcessUsageDelta(-mOriginQuotaUsage);
  mKeys.Clear();
}

bool
SessionStorageCache::ProcessUsageDelta(int64_t aDelta)
{
  // Check limit per this origin
  uint64_t newOriginUsage = mOriginQuotaUsage + aDelta;
  if (aDelta > 0 && newOriginUsage > StorageManagerBase::GetQuota()) {
    return false;
  }

  // Update size in our data set
  mOriginQuotaUsage = newOriginUsage;
  return true;
}

already_AddRefed<SessionStorageCache>
SessionStorageCache::Clone() const
{
  RefPtr<SessionStorageCache> cache = new SessionStorageCache();
  cache->mOriginQuotaUsage = mOriginQuotaUsage;

  for (auto iter = mKeys.ConstIter(); !iter.Done(); iter.Next()) {
    cache->mKeys.Put(iter.Key(), iter.Data());
  }

  return cache.forget();
}

} // dom namespace
} // mozilla namespace
