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

  int64_t GetOriginQuotaUsage() const
  {
    return mOriginQuotaUsage;
  }

  uint32_t Length() const { return mKeys.Count(); }

  void Key(uint32_t aIndex, nsAString& aResult);

  void GetItem(const nsAString& aKey, nsAString& aResult);

  void GetKeys(nsTArray<nsString>& aKeys);

  nsresult SetItem(const nsAString& aKey, const nsAString& aValue,
                   nsString& aOldValue);

  nsresult RemoveItem(const nsAString& aKey,
                      nsString& aOldValue);

  void Clear();

  already_AddRefed<SessionStorageCache>
  Clone() const;

private:
  ~SessionStorageCache() = default;

  bool ProcessUsageDelta(int64_t aDelta);

  int64_t mOriginQuotaUsage;
  nsDataHashtable<nsStringHashKey, nsString> mKeys;
};

} // dom namespace
} // mozilla namespace

#endif //mozilla_dom_SessionStorageCache_h
