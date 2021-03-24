/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageCache.h"

#include "LocalStorageManager.h"
#include "StorageIPC.h"
#include "mozilla/dom/LSWriteOptimizer.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "nsDOMString.h"

namespace mozilla {
namespace dom {

void SSWriteOptimizer::Enumerate(nsTArray<SSWriteInfo>& aWriteInfos) {
  AssertIsOnOwningThread();

  // The mWriteInfos hash table contains all write infos, but it keeps them in
  // an arbitrary order, which means write infos need to be sorted before being
  // processed.

  nsTArray<NotNull<WriteInfo*>> writeInfos;
  GetSortedWriteInfos(writeInfos);

  for (const auto& writeInfo : writeInfos) {
    switch (writeInfo->GetType()) {
      case WriteInfo::InsertItem: {
        const auto& insertItemInfo =
            static_cast<const InsertItemInfo&>(*writeInfo);

        aWriteInfos.AppendElement(
            SSSetItemInfo{nsString{insertItemInfo.GetKey()},
                          nsString{insertItemInfo.GetValue()}});

        break;
      }

      case WriteInfo::UpdateItem: {
        const auto& updateItemInfo =
            static_cast<const UpdateItemInfo&>(*writeInfo);

        if (updateItemInfo.UpdateWithMove()) {
          // See the comment in LSWriteOptimizer::InsertItem for more details
          // about the UpdateWithMove flag.

          aWriteInfos.AppendElement(
              SSRemoveItemInfo{nsString{updateItemInfo.GetKey()}});
        }

        aWriteInfos.AppendElement(
            SSSetItemInfo{nsString{updateItemInfo.GetKey()},
                          nsString{updateItemInfo.GetValue()}});

        break;
      }

      case WriteInfo::DeleteItem: {
        const auto& deleteItemInfo =
            static_cast<const DeleteItemInfo&>(*writeInfo);

        aWriteInfos.AppendElement(
            SSRemoveItemInfo{nsString{deleteItemInfo.GetKey()}});

        break;
      }

      case WriteInfo::Truncate: {
        aWriteInfos.AppendElement(SSClearInfo{});

        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }
}

SessionStorageCache::SessionStorageCache()
    : mActor(nullptr), mLoadedOrCloned(false) {}

SessionStorageCache::~SessionStorageCache() {
  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

SessionStorageCache::DataSet* SessionStorageCache::Set(
    DataSetType aDataSetType) {
  if (aDataSetType == eDefaultSetType) {
    return &mDefaultSet;
  }

  MOZ_ASSERT(aDataSetType == eSessionSetType);

  return &mSessionSet;
}

int64_t SessionStorageCache::GetOriginQuotaUsage(DataSetType aDataSetType) {
  return Set(aDataSetType)->mOriginQuotaUsage;
}

uint32_t SessionStorageCache::Length(DataSetType aDataSetType) {
  return Set(aDataSetType)->mKeys.Count();
}

void SessionStorageCache::Key(DataSetType aDataSetType, uint32_t aIndex,
                              nsAString& aResult) {
  aResult.SetIsVoid(true);
  for (auto iter = Set(aDataSetType)->mKeys.ConstIter(); !iter.Done();
       iter.Next()) {
    if (aIndex == 0) {
      aResult = iter.Key();
      return;
    }
    aIndex--;
  }
}

void SessionStorageCache::GetItem(DataSetType aDataSetType,
                                  const nsAString& aKey, nsAString& aResult) {
  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!Set(aDataSetType)->mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }
  aResult = value;
}

void SessionStorageCache::GetKeys(DataSetType aDataSetType,
                                  nsTArray<nsString>& aKeys) {
  AppendToArray(aKeys, Set(aDataSetType)->mKeys.Keys());
}

nsresult SessionStorageCache::SetItem(DataSetType aDataSetType,
                                      const nsAString& aKey,
                                      const nsAString& aValue,
                                      nsString& aOldValue,
                                      bool aRecordWriteInfo) {
  int64_t delta = 0;
  DataSet* dataSet = Set(aDataSetType);
  MOZ_ASSERT(dataSet);

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

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    if (DOMStringIsNull(aOldValue)) {
      dataSet->mWriteOptimizer.InsertItem(aKey, aValue);
    } else {
      dataSet->mWriteOptimizer.UpdateItem(aKey, aValue);
    }
  }

  dataSet->mKeys.InsertOrUpdate(aKey, nsString(aValue));
  return NS_OK;
}

nsresult SessionStorageCache::RemoveItem(DataSetType aDataSetType,
                                         const nsAString& aKey,
                                         nsString& aOldValue,
                                         bool aRecordWriteInfo) {
  DataSet* dataSet = Set(aDataSetType);
  MOZ_ASSERT(dataSet);

  if (!dataSet->mKeys.Get(aKey, &aOldValue)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  dataSet->ProcessUsageDelta(-(static_cast<int64_t>(aOldValue.Length()) +
                               static_cast<int64_t>(aKey.Length())));

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    dataSet->mWriteOptimizer.DeleteItem(aKey);
  }

  dataSet->mKeys.Remove(aKey);
  return NS_OK;
}

void SessionStorageCache::Clear(DataSetType aDataSetType,
                                bool aByUserInteraction,
                                bool aRecordWriteInfo) {
  DataSet* dataSet = Set(aDataSetType);
  MOZ_ASSERT(dataSet);

  dataSet->ProcessUsageDelta(-dataSet->mOriginQuotaUsage);

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    dataSet->mWriteOptimizer.Truncate();
  }

  dataSet->mKeys.Clear();
}

void SessionStorageCache::ResetWriteInfos(DataSetType aDataSetType) {
  Set(aDataSetType)->mWriteOptimizer.Reset();
}

already_AddRefed<SessionStorageCache> SessionStorageCache::Clone() const {
  RefPtr<SessionStorageCache> cache = new SessionStorageCache();

  cache->mDefaultSet.mOriginQuotaUsage = mDefaultSet.mOriginQuotaUsage;
  for (const auto& keyEntry : mDefaultSet.mKeys) {
    cache->mDefaultSet.mKeys.InsertOrUpdate(keyEntry.GetKey(),
                                            keyEntry.GetData());
    cache->mDefaultSet.mWriteOptimizer.InsertItem(keyEntry.GetKey(),
                                                  keyEntry.GetData());
  }

  cache->mSessionSet.mOriginQuotaUsage = mSessionSet.mOriginQuotaUsage;
  for (const auto& keyEntry : mSessionSet.mKeys) {
    cache->mSessionSet.mKeys.InsertOrUpdate(keyEntry.GetKey(),
                                            keyEntry.GetData());
    cache->mSessionSet.mWriteOptimizer.InsertItem(keyEntry.GetKey(),
                                                  keyEntry.GetData());
  }

  return cache.forget();
}

nsTArray<SSSetItemInfo> SessionStorageCache::SerializeData(
    DataSetType aDataSetType) {
  nsTArray<SSSetItemInfo> data;
  for (const auto& keyEntry : Set(aDataSetType)->mKeys) {
    data.EmplaceBack(nsString{keyEntry.GetKey()}, keyEntry.GetData());
  }
  return data;
}

nsTArray<SSWriteInfo> SessionStorageCache::SerializeWriteInfos(
    DataSetType aDataSetType) {
  nsTArray<SSWriteInfo> writeInfos;
  Set(aDataSetType)->mWriteOptimizer.Enumerate(writeInfos);
  return writeInfos;
}

void SessionStorageCache::DeserializeData(
    DataSetType aDataSetType, const nsTArray<SSSetItemInfo>& aData) {
  Clear(aDataSetType, false, /* aRecordWriteInfo */ false);
  for (const auto& keyValuePair : aData) {
    nsString oldValue;
    SetItem(aDataSetType, keyValuePair.key(), keyValuePair.value(), oldValue,
            false);
  }
}

void SessionStorageCache::DeserializeWriteInfos(
    DataSetType aDataSetType, const nsTArray<SSWriteInfo>& aInfos) {
  for (const auto& writeInfo : aInfos) {
    switch (writeInfo.type()) {
      case SSWriteInfo::TSSSetItemInfo: {
        const SSSetItemInfo& info = writeInfo.get_SSSetItemInfo();

        nsString oldValue;
        SetItem(aDataSetType, info.key(), info.value(), oldValue,
                /* aRecordWriteInfo */ false);

        break;
      }

      case SSWriteInfo::TSSRemoveItemInfo: {
        const SSRemoveItemInfo& info = writeInfo.get_SSRemoveItemInfo();

        nsString oldValue;
        RemoveItem(aDataSetType, info.key(), oldValue,
                   /* aRecordWriteInfo */ false);

        break;
      }

      case SSWriteInfo::TSSClearInfo: {
        Clear(aDataSetType, false, /* aRecordWriteInfo */ false);

        break;
      }

      default:
        MOZ_CRASH("Should never get here!");
    }
  }
}

void SessionStorageCache::SetActor(SessionStorageCacheChild* aActor) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

void SessionStorageCache::ClearActor() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);

  mActor = nullptr;
}

bool SessionStorageCache::DataSet::ProcessUsageDelta(int64_t aDelta) {
  // Check limit per this origin
  uint64_t newOriginUsage = mOriginQuotaUsage + aDelta;
  if (aDelta > 0 && newOriginUsage > LocalStorageManager::GetOriginQuota()) {
    return false;
  }

  // Update size in our data set
  mOriginQuotaUsage = newOriginUsage;
  return true;
}

}  // namespace dom
}  // namespace mozilla
