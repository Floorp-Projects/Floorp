/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMStorage.h"
#include "nsDOMStorageMemoryDB.h"
#include "nsNetUtil.h"

nsresult
nsDOMStorageMemoryDB::Init(nsDOMStoragePersistentDB* aPreloadDB)
{
  mData.Init(20);

  mPreloadDB = aPreloadDB;
  return NS_OK;
}

static PLDHashOperator
AllKeyEnum(nsSessionStorageEntry* aEntry, void* userArg)
{
  nsDOMStorageMemoryDB::nsStorageItemsTable *target =
      (nsDOMStorageMemoryDB::nsStorageItemsTable *)userArg;

  nsDOMStorageMemoryDB::nsInMemoryItem* item =
      new nsDOMStorageMemoryDB::nsInMemoryItem();
  if (!item)
    return PL_DHASH_STOP;

  aEntry->mItem->GetValue(item->mValue);
  nsresult rv = aEntry->mItem->GetSecure(&item->mSecure);
  if (NS_FAILED(rv))
    item->mSecure = false;

  target->Put(aEntry->GetKey(), item);
  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorageMemoryDB::GetItemsTable(DOMStorageImpl* aStorage,
                                    nsInMemoryStorage** aMemoryStorage)
{
  if (mData.Get(aStorage->GetScopeDBKey(), aMemoryStorage))
    return NS_OK;

  *aMemoryStorage = nullptr;

  nsInMemoryStorage* storageData = new nsInMemoryStorage();
  if (!storageData)
    return NS_ERROR_OUT_OF_MEMORY;

  storageData->mTable.Init();

  if (mPreloadDB) {
    nsresult rv;

    nsTHashtable<nsSessionStorageEntry> keys;
    keys.Init();

    rv = mPreloadDB->GetAllKeys(aStorage, &keys);
    NS_ENSURE_SUCCESS(rv, rv);

    mPreloading = true;
    keys.EnumerateEntries(AllKeyEnum, &storageData->mTable);
    mPreloading = false;
  }

  mData.Put(aStorage->GetScopeDBKey(), storageData);
  *aMemoryStorage = storageData;

  return NS_OK;
}

struct GetAllKeysEnumStruc
{
  nsTHashtable<nsSessionStorageEntry>* mTarget;
  DOMStorageImpl* mStorage;
};

static PLDHashOperator
GetAllKeysEnum(const nsAString& keyname,
               nsDOMStorageMemoryDB::nsInMemoryItem* item,
               void *closure)
{
  GetAllKeysEnumStruc* struc = (GetAllKeysEnumStruc*)closure;

  nsSessionStorageEntry* entry = struc->mTarget->PutEntry(keyname);
  if (!entry)
    return PL_DHASH_STOP;

  entry->mItem = new nsDOMStorageItem(struc->mStorage,
                                      keyname,
                                      EmptyString(),
                                      item->mSecure);
  if (!entry->mItem)
    return PL_DHASH_STOP;

  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorageMemoryDB::GetAllKeys(DOMStorageImpl* aStorage,
                                 nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  GetAllKeysEnumStruc struc;
  struc.mTarget = aKeys;
  struc.mStorage = aStorage;
  storage->mTable.EnumerateRead(GetAllKeysEnum, &struc);

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::GetKeyValue(DOMStorageImpl* aStorage,
                                  const nsAString& aKey,
                                  nsAString& aValue,
                                  bool* aSecure)
{
  if (mPreloading) {
    NS_PRECONDITION(mPreloadDB, "Must have a preload DB set when preloading");
    return mPreloadDB->GetKeyValue(aStorage, aKey, aValue, aSecure);
  }

  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  nsInMemoryItem* item;
  if (!storage->mTable.Get(aKey, &item))
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  aValue = item->mValue;
  *aSecure = item->mSecure;
  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::SetKey(DOMStorageImpl* aStorage,
                             const nsAString& aKey,
                             const nsAString& aValue,
                             bool aSecure,
                             PRInt32 aQuota,
                             bool aExcludeOfflineFromUsage,
                             PRInt32 *aNewUsage)
{
  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 usage = 0;
  if (!aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage).IsEmpty()) {
    rv = GetUsage(aStorage, aExcludeOfflineFromUsage, &usage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  usage += aKey.Length() + aValue.Length();

  nsInMemoryItem* item;
  if (!storage->mTable.Get(aKey, &item)) {
    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }

    item = new nsInMemoryItem();
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;

    storage->mTable.Put(aKey, item);
    storage->mUsageDelta += aKey.Length();
  }
  else
  {
    if (!aSecure && item->mSecure)
      return NS_ERROR_DOM_SECURITY_ERR;
    usage -= aKey.Length() + item->mValue.Length();
    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }
  }

  storage->mUsageDelta += aValue.Length() - item->mValue.Length();

  item->mValue = aValue;
  item->mSecure = aSecure;

  *aNewUsage = usage;

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::SetSecure(DOMStorageImpl* aStorage,
                                const nsAString& aKey,
                                const bool aSecure)
{
  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  nsInMemoryItem* item;
  if (!storage->mTable.Get(aKey, &item))
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  item->mSecure = aSecure;

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::RemoveKey(DOMStorageImpl* aStorage,
                                const nsAString& aKey,
                                bool aExcludeOfflineFromUsage,
                                PRInt32 aKeyUsage)
{
  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  nsInMemoryItem* item;
  if (!storage->mTable.Get(aKey, &item))
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  storage->mUsageDelta -= aKey.Length() + item->mValue.Length();
  storage->mTable.Remove(aKey);

  MarkScopeDirty(aStorage);

  return NS_OK;
}

static PLDHashOperator
RemoveAllKeysEnum(const nsAString& keyname,
                  nsAutoPtr<nsDOMStorageMemoryDB::nsInMemoryItem>& item,
                  void *closure)
{
  nsDOMStorageMemoryDB::nsInMemoryStorage* storage =
      (nsDOMStorageMemoryDB::nsInMemoryStorage*)closure;

  storage->mUsageDelta -= keyname.Length() + item->mValue.Length();
  return PL_DHASH_REMOVE;
}

nsresult
nsDOMStorageMemoryDB::ClearStorage(DOMStorageImpl* aStorage)
{
  nsresult rv;

  nsInMemoryStorage* storage;
  rv = GetItemsTable(aStorage, &storage);
  NS_ENSURE_SUCCESS(rv, rv);

  storage->mTable.Enumerate(RemoveAllKeysEnum, storage);

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::DropStorage(DOMStorageImpl* aStorage)
{
  mData.Remove(aStorage->GetScopeDBKey());
  MarkScopeDirty(aStorage);
  return NS_OK;
}

struct RemoveOwnersStruc
{
  nsCString* mSubDomain;
  bool mMatch;
};

static PLDHashOperator
RemoveOwnersEnum(const nsACString& key,
                 nsAutoPtr<nsDOMStorageMemoryDB::nsInMemoryStorage>& storage,
                 void *closure)
{
  RemoveOwnersStruc* struc = (RemoveOwnersStruc*)closure;

  if (StringBeginsWith(key, *(struc->mSubDomain)) == struc->mMatch)
    return PL_DHASH_REMOVE;

  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorageMemoryDB::RemoveOwner(const nsACString& aOwner,
                                  bool aIncludeSubDomains)
{
  nsCAutoString subdomainsDBKey;
  nsDOMStorageDBWrapper::CreateDomainScopeDBKey(aOwner, subdomainsDBKey);

  if (!aIncludeSubDomains)
    subdomainsDBKey.AppendLiteral(":");

  RemoveOwnersStruc struc;
  struc.mSubDomain = &subdomainsDBKey;
  struc.mMatch = true;
  mData.Enumerate(RemoveOwnersEnum, &struc);

  MarkAllScopesDirty();

  return NS_OK;
}


nsresult
nsDOMStorageMemoryDB::RemoveOwners(const nsTArray<nsString> &aOwners,
                                   bool aIncludeSubDomains,
                                   bool aMatch)
{
  if (aOwners.Length() == 0) {
    if (aMatch) {
      return NS_OK;
    }

    return RemoveAll();
  }

  for (PRUint32 i = 0; i < aOwners.Length(); i++) {
    nsCAutoString quotaKey;
    nsDOMStorageDBWrapper::CreateDomainScopeDBKey(
      NS_ConvertUTF16toUTF8(aOwners[i]), quotaKey);

    if (!aIncludeSubDomains)
      quotaKey.AppendLiteral(":");

    RemoveOwnersStruc struc;
    struc.mSubDomain = &quotaKey;
    struc.mMatch = aMatch;
    mData.Enumerate(RemoveOwnersEnum, &struc);
  }

  MarkAllScopesDirty();

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::RemoveAll()
{
  mData.Clear(); // XXX Check this releases all instances

  MarkAllScopesDirty();

  return NS_OK;
}

nsresult
nsDOMStorageMemoryDB::GetUsage(DOMStorageImpl* aStorage,
                               bool aExcludeOfflineFromUsage, PRInt32 *aUsage)
{
  return GetUsageInternal(aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage),
                          aExcludeOfflineFromUsage, aUsage);
}

nsresult
nsDOMStorageMemoryDB::GetUsage(const nsACString& aDomain,
                               bool aIncludeSubDomains,
                               PRInt32 *aUsage)
{
  nsresult rv;

  nsCAutoString quotadomainDBKey;
  rv = nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(aDomain,
                                                     aIncludeSubDomains,
                                                     false,
                                                     quotadomainDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetUsageInternal(quotadomainDBKey, false, aUsage);
}

struct GetUsageEnumStruc
{
  PRInt32 mUsage;
  PRInt32 mExcludeOfflineFromUsage;
  nsCString mSubdomain;
};

static PLDHashOperator
GetUsageEnum(const nsACString& key,
             nsDOMStorageMemoryDB::nsInMemoryStorage* storageData,
             void *closure)
{
  GetUsageEnumStruc* struc = (GetUsageEnumStruc*)closure;

  if (StringBeginsWith(key, struc->mSubdomain)) {
    if (struc->mExcludeOfflineFromUsage) {
      nsCAutoString domain;
      nsresult rv = nsDOMStorageDBWrapper::GetDomainFromScopeKey(key, domain);
      if (NS_SUCCEEDED(rv) && IsOfflineAllowed(domain))
        return PL_DHASH_NEXT;
    }

    struc->mUsage += storageData->mUsageDelta;
  }

  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorageMemoryDB::GetUsageInternal(const nsACString& aQuotaDomainDBKey,
                                       bool aExcludeOfflineFromUsage,
                                       PRInt32 *aUsage)
{
  GetUsageEnumStruc struc;
  struc.mUsage = 0;
  struc.mExcludeOfflineFromUsage = aExcludeOfflineFromUsage;
  struc.mSubdomain = aQuotaDomainDBKey;

  if (mPreloadDB) {
    nsresult rv;

    rv = mPreloadDB->GetUsageInternal(aQuotaDomainDBKey,
                                      aExcludeOfflineFromUsage, &struc.mUsage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mData.EnumerateRead(GetUsageEnum, &struc);

  *aUsage = struc.mUsage;
  return NS_OK;
}
