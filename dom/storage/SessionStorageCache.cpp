/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageCache.h"

#include "LocalStorageManager.h"
#include "StorageIPC.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/LSWriteOptimizer.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "nsDOMString.h"

namespace mozilla::dom {

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

int64_t SessionStorageCache::GetOriginQuotaUsage() {
  return mDataSet.mOriginQuotaUsage;
}

uint32_t SessionStorageCache::Length() { return mDataSet.mKeys.Count(); }

void SessionStorageCache::Key(uint32_t aIndex, nsAString& aResult) {
  aResult.SetIsVoid(true);
  for (auto iter = mDataSet.mKeys.ConstIter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aResult = iter.Key();
      return;
    }
    aIndex--;
  }
}

void SessionStorageCache::GetItem(const nsAString& aKey, nsAString& aResult) {
  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!mDataSet.mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }
  aResult = value;
}

void SessionStorageCache::GetKeys(nsTArray<nsString>& aKeys) {
  AppendToArray(aKeys, mDataSet.mKeys.Keys());
}

nsresult SessionStorageCache::SetItem(const nsAString& aKey,
                                      const nsAString& aValue,
                                      nsString& aOldValue,
                                      bool aRecordWriteInfo) {
  int64_t delta = 0;

  if (!mDataSet.mKeys.Get(aKey, &aOldValue)) {
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

  if (!mDataSet.ProcessUsageDelta(delta)) {
    return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
  }

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    if (DOMStringIsNull(aOldValue)) {
      mDataSet.mWriteOptimizer.InsertItem(aKey, aValue);
    } else {
      mDataSet.mWriteOptimizer.UpdateItem(aKey, aValue);
    }
  }

  mDataSet.mKeys.InsertOrUpdate(aKey, nsString(aValue));
  return NS_OK;
}

nsresult SessionStorageCache::RemoveItem(const nsAString& aKey,
                                         nsString& aOldValue,
                                         bool aRecordWriteInfo) {
  if (!mDataSet.mKeys.Get(aKey, &aOldValue)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  mDataSet.ProcessUsageDelta(-(static_cast<int64_t>(aOldValue.Length()) +
                               static_cast<int64_t>(aKey.Length())));

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    mDataSet.mWriteOptimizer.DeleteItem(aKey);
  }

  mDataSet.mKeys.Remove(aKey);
  return NS_OK;
}

void SessionStorageCache::Clear(bool aByUserInteraction,
                                bool aRecordWriteInfo) {
  mDataSet.ProcessUsageDelta(-mDataSet.mOriginQuotaUsage);

  if (aRecordWriteInfo && XRE_IsContentProcess()) {
    mDataSet.mWriteOptimizer.Truncate();
  }

  mDataSet.mKeys.Clear();
}

void SessionStorageCache::ResetWriteInfos() {
  mDataSet.mWriteOptimizer.Reset();
}

already_AddRefed<SessionStorageCache> SessionStorageCache::Clone() const {
  RefPtr<SessionStorageCache> cache = new SessionStorageCache();

  cache->mDataSet.mOriginQuotaUsage = mDataSet.mOriginQuotaUsage;
  for (const auto& keyEntry : mDataSet.mKeys) {
    cache->mDataSet.mKeys.InsertOrUpdate(keyEntry.GetKey(), keyEntry.GetData());
    cache->mDataSet.mWriteOptimizer.InsertItem(keyEntry.GetKey(),
                                               keyEntry.GetData());
  }

  return cache.forget();
}

nsTArray<SSSetItemInfo> SessionStorageCache::SerializeData() {
  nsTArray<SSSetItemInfo> data;
  for (const auto& keyEntry : mDataSet.mKeys) {
    data.EmplaceBack(nsString{keyEntry.GetKey()}, keyEntry.GetData());
  }
  return data;
}

nsTArray<SSWriteInfo> SessionStorageCache::SerializeWriteInfos() {
  nsTArray<SSWriteInfo> writeInfos;
  mDataSet.mWriteOptimizer.Enumerate(writeInfos);
  return writeInfos;
}

void SessionStorageCache::DeserializeData(
    const nsTArray<SSSetItemInfo>& aData) {
  Clear(false, /* aRecordWriteInfo */ false);
  for (const auto& keyValuePair : aData) {
    nsString oldValue;
    SetItem(keyValuePair.key(), keyValuePair.value(), oldValue, false);
  }
}

void SessionStorageCache::DeserializeWriteInfos(
    const nsTArray<SSWriteInfo>& aInfos) {
  for (const auto& writeInfo : aInfos) {
    switch (writeInfo.type()) {
      case SSWriteInfo::TSSSetItemInfo: {
        const SSSetItemInfo& info = writeInfo.get_SSSetItemInfo();

        nsString oldValue;
        SetItem(info.key(), info.value(), oldValue,
                /* aRecordWriteInfo */ false);

        break;
      }

      case SSWriteInfo::TSSRemoveItemInfo: {
        const SSRemoveItemInfo& info = writeInfo.get_SSRemoveItemInfo();

        nsString oldValue;
        RemoveItem(info.key(), oldValue,
                   /* aRecordWriteInfo */ false);

        break;
      }

      case SSWriteInfo::TSSClearInfo: {
        Clear(false, /* aRecordWriteInfo */ false);

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

}  // namespace mozilla::dom
