/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageCache.h"

namespace mozilla {
namespace dom {

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
  if (aDelta > 0 && newOriginUsage > LocalStorageManager::GetQuota()) {
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
