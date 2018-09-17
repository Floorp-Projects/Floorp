/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageCache.h"

namespace mozilla {
namespace dom {

SessionStorageCache::SessionStorageCache()
  : mSessionDataSetActive(false)
{}

SessionStorageCache::DataSet*
SessionStorageCache::Set(DataSetType aDataSetType)
{
  if (aDataSetType == eDefaultSetType) {
    return &mDefaultSet;
  }

  MOZ_ASSERT(aDataSetType == eSessionSetType);

  if (!mSessionDataSetActive) {
    mSessionSet.mOriginQuotaUsage = mDefaultSet.mOriginQuotaUsage;

    for (auto iter = mDefaultSet.mKeys.ConstIter(); !iter.Done(); iter.Next()) {
      mSessionSet.mKeys.Put(iter.Key(), iter.Data());
    }

    mSessionDataSetActive = true;
  }

  return &mSessionSet;
}

int64_t
SessionStorageCache::GetOriginQuotaUsage(DataSetType aDataSetType)
{
  return Set(aDataSetType)->mOriginQuotaUsage;
}

uint32_t
SessionStorageCache::Length(DataSetType aDataSetType)
{
  return Set(aDataSetType)->mKeys.Count();
}

void
SessionStorageCache::Key(DataSetType aDataSetType, uint32_t aIndex,
                         nsAString& aResult)
{
  aResult.SetIsVoid(true);
  for (auto iter = Set(aDataSetType)->mKeys.Iter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aResult = iter.Key();
      return;
    }
    aIndex--;
  }
}

void
SessionStorageCache::GetItem(DataSetType aDataSetType, const nsAString& aKey,
                             nsAString& aResult)
{
  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!Set(aDataSetType)->mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }
  aResult = value;
}

void
SessionStorageCache::GetKeys(DataSetType aDataSetType, nsTArray<nsString>& aKeys)
{
  for (auto iter = Set(aDataSetType)->mKeys.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

nsresult
SessionStorageCache::SetItem(DataSetType aDataSetType, const nsAString& aKey,
                             const nsAString& aValue, nsString& aOldValue)
{
  int64_t delta = 0;
  DataSet* dataSet = Set(aDataSetType);

  if (!dataSet->mKeys.Get(aKey, &aOldValue)) {
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

  if (!dataSet->ProcessUsageDelta(delta)) {
    return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
  }

  dataSet->mKeys.Put(aKey, nsString(aValue));
  return NS_OK;
}

nsresult
SessionStorageCache::RemoveItem(DataSetType aDataSetType, const nsAString& aKey,
                                nsString& aOldValue)
{
  DataSet* dataSet = Set(aDataSetType);

  if (!dataSet->mKeys.Get(aKey, &aOldValue)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  dataSet->ProcessUsageDelta(-(static_cast<int64_t>(aOldValue.Length()) +
                               static_cast<int64_t>(aKey.Length())));

  dataSet->mKeys.Remove(aKey);
  return NS_OK;
}

void
SessionStorageCache::Clear(DataSetType aDataSetType, bool aByUserInteraction)
{
  DataSet* dataSet = Set(aDataSetType);
  dataSet->ProcessUsageDelta(-dataSet->mOriginQuotaUsage);
  dataSet->mKeys.Clear();

  if (!aByUserInteraction && aDataSetType == eSessionSetType) {
    mSessionDataSetActive = false;
  }
}

already_AddRefed<SessionStorageCache>
SessionStorageCache::Clone() const
{
  RefPtr<SessionStorageCache> cache = new SessionStorageCache();

  cache->mSessionDataSetActive = mSessionDataSetActive;

  cache->mDefaultSet.mOriginQuotaUsage = mDefaultSet.mOriginQuotaUsage;
  for (auto iter = mDefaultSet.mKeys.ConstIter(); !iter.Done(); iter.Next()) {
    cache->mDefaultSet.mKeys.Put(iter.Key(), iter.Data());
  }

  cache->mSessionSet.mOriginQuotaUsage = mSessionSet.mOriginQuotaUsage;
  for (auto iter = mSessionSet.mKeys.ConstIter(); !iter.Done(); iter.Next()) {
    cache->mSessionSet.mKeys.Put(iter.Key(), iter.Data());
  }

  return cache.forget();
}

bool
SessionStorageCache::DataSet::ProcessUsageDelta(int64_t aDelta)
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

} // dom namespace
} // mozilla namespace
