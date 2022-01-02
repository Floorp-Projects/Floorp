/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageCache_h
#define mozilla_dom_SessionStorageCache_h

#include "mozilla/UniquePtr.h"
#include "mozilla/dom/LSWriteOptimizerImpl.h"
#include "nsTHashMap.h"

namespace mozilla::dom {

class SSSetItemInfo;
class SSWriteInfo;
class SessionStorageCacheChild;

/**
 * Coalescing manipulation queue used by `SessionStorageCache`.  Used by
 * `SessionStorageCache` to buffer and coalesce manipulations before they
 * are sent to the parent process.
 */
class SSWriteOptimizer final : public LSWriteOptimizer<nsAString, nsString> {
 public:
  void Enumerate(nsTArray<SSWriteInfo>& aWriteInfos);
};

class SessionStorageCache final {
 public:
  NS_INLINE_DECL_REFCOUNTING(SessionStorageCache)

  SessionStorageCache();

  int64_t GetOriginQuotaUsage();

  uint32_t Length();

  void Key(uint32_t aIndex, nsAString& aResult);

  void GetItem(const nsAString& aKey, nsAString& aResult);

  void GetKeys(nsTArray<nsString>& aKeys);

  nsresult SetItem(const nsAString& aKey, const nsAString& aValue,
                   nsString& aOldValue, bool aRecordWriteInfo = true);

  nsresult RemoveItem(const nsAString& aKey, nsString& aOldValue,
                      bool aRecordWriteInfo = true);

  void Clear(bool aByUserInteraction = true, bool aRecordWriteInfo = true);

  void ResetWriteInfos();

  already_AddRefed<SessionStorageCache> Clone() const;

  nsTArray<SSSetItemInfo> SerializeData();

  nsTArray<SSWriteInfo> SerializeWriteInfos();

  void DeserializeData(const nsTArray<SSSetItemInfo>& aData);

  void DeserializeWriteInfos(const nsTArray<SSWriteInfo>& aInfos);

  void SetActor(SessionStorageCacheChild* aActor);

  SessionStorageCacheChild* Actor() const { return mActor; }

  void ClearActor();

  void SetLoadedOrCloned() { mLoadedOrCloned = true; }

  bool WasLoadedOrCloned() const { return mLoadedOrCloned; }

 private:
  ~SessionStorageCache();

  struct DataSet {
    DataSet() : mOriginQuotaUsage(0) {}

    bool ProcessUsageDelta(int64_t aDelta);

    nsTHashMap<nsStringHashKey, nsString> mKeys;

    SSWriteOptimizer mWriteOptimizer;

    int64_t mOriginQuotaUsage;
  };

  DataSet mDataSet;

  SessionStorageCacheChild* mActor;
  bool mLoadedOrCloned;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStorageCache_h
