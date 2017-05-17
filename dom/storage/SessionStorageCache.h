/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageCache_h
#define mozilla_dom_SessionStorageCache_h

#include "nsDataHashtable.h"

namespace mozilla {
namespace dom {

class SessionStorageCache final
{
public:
  NS_INLINE_DECL_REFCOUNTING(SessionStorageCache)

  SessionStorageCache();

  enum DataSetType {
    eDefaultSetType,
    eSessionSetType,
  };

  int64_t GetOriginQuotaUsage(DataSetType aDataSetType);

  uint32_t Length(DataSetType aDataSetType);

  void Key(DataSetType aDataSetType, uint32_t aIndex, nsAString& aResult);

  void GetItem(DataSetType aDataSetType, const nsAString& aKey,
               nsAString& aResult);

  void GetKeys(DataSetType aDataSetType, nsTArray<nsString>& aKeys);

  nsresult SetItem(DataSetType aDataSetType, const nsAString& aKey,
                   const nsAString& aValue, nsString& aOldValue);

  nsresult RemoveItem(DataSetType aDataSetType, const nsAString& aKey,
                      nsString& aOldValue);

  void Clear(DataSetType aDataSetType, bool aByUserInteraction = true);

  already_AddRefed<SessionStorageCache>
  Clone() const;

private:
  ~SessionStorageCache() = default;

  struct DataSet
  {
    DataSet()
      : mOriginQuotaUsage(0)
    {}

    bool ProcessUsageDelta(int64_t aDelta);

    int64_t mOriginQuotaUsage;
    nsDataHashtable<nsStringHashKey, nsString> mKeys;
  };

  DataSet* Set(DataSetType aDataSetType);

  DataSet mDefaultSet;
  DataSet mSessionSet;
  bool mSessionDataSetActive;
};

} // dom namespace
} // mozilla namespace

#endif //mozilla_dom_SessionStorageCache_h
