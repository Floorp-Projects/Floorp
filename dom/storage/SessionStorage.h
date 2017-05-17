/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorage_h
#define mozilla_dom_SessionStorage_h

#include "Storage.h"
#include "nsDataHashtable.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class SessionStorageCache;

class SessionStorage final : public Storage
{
public:
  SessionStorage(nsPIDOMWindowInner* aWindow,
                 nsIPrincipal* aPrincipal,
                 SessionStorageCache* aCache);

  int64_t GetOriginQuotaUsage() const override;

  // WebIDL
  uint32_t GetLength(nsIPrincipal& aSubjectPrincipal,
                     ErrorResult& aRv) override;

  void Key(uint32_t aIndex, nsAString& aResult,
           nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aRv) override;

  void GetItem(const nsAString& aKey, nsAString& aResult,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv) override;

  void GetSupportedNames(nsTArray<nsString>& aKeys) override;

  void SetItem(const nsAString& aKey, const nsAString& aValue,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv) override;

  void RemoveItem(const nsAString& aKey,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv) override;

  void Clear(nsIPrincipal& aSubjectPrincipal,
             ErrorResult& aRv) override;

  bool IsSessionOnly() const override { return true; }

private:
  ~SessionStorage();

  bool CanUseStorage(nsIPrincipal& aSubjectPrincipal);

  bool ProcessUsageDelta(int64_t aDelta);

  void
  BroadcastChangeNotification(const nsAString& aKey,
                              const nsAString& aOldValue,
                              const nsAString& aNewValue);

  RefPtr<SessionStorageCache> mCache;
};

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

private:
  ~SessionStorageCache() = default;

  bool ProcessUsageDelta(int64_t aDelta);

  int64_t mOriginQuotaUsage;
  nsDataHashtable<nsStringHashKey, nsString> mKeys;
};

} // dom namespace
} // mozilla namespace

#endif //mozilla_dom_SessionStorage_h
