/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGININFO_H_
#define DOM_QUOTA_ORIGININFO_H_

#include "Assertions.h"
#include "ClientUsageArray.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

class CanonicalQuotaObject;
class GroupInfo;

class OriginInfo final {
  friend class CanonicalQuotaObject;
  friend class GroupInfo;
  friend class QuotaManager;

 public:
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
             const nsACString& aStorageOrigin, bool aIsPrivate,
             const ClientUsageArray& aClientUsages, uint64_t aUsage,
             int64_t aAccessTime, bool aPersisted, bool aDirectoryExists);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  GroupInfo* GetGroupInfo() const { return mGroupInfo; }

  const nsCString& Origin() const { return mOrigin; }

  int64_t LockedUsage() const {
    AssertCurrentThreadOwnsQuotaMutex();

#ifdef DEBUG
    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    uint64_t usage = 0;
    for (Client::Type type : quotaManager->AllClientTypes()) {
      AssertNoOverflow(usage, mClientUsages[type].valueOr(0));
      usage += mClientUsages[type].valueOr(0);
    }
    MOZ_ASSERT(mUsage == usage);
#endif

    return mUsage;
  }

  int64_t LockedAccessTime() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mAccessTime;
  }

  bool LockedPersisted() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mPersisted;
  }

  OriginMetadata FlattenToOriginMetadata() const;

  FullOriginMetadata LockedFlattenToFullOriginMetadata() const;

  nsresult LockedBindToStatement(mozIStorageStatement* aStatement) const;

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~OriginInfo() {
    MOZ_COUNT_DTOR(OriginInfo);

    MOZ_ASSERT(!mCanonicalQuotaObjects.Count());
  }

  void LockedDecreaseUsage(Client::Type aClientType, int64_t aSize);

  void LockedResetUsageForClient(Client::Type aClientType);

  UsageInfo LockedGetUsageForClient(Client::Type aClientType);

  void LockedUpdateAccessTime(int64_t aAccessTime) {
    AssertCurrentThreadOwnsQuotaMutex();

    mAccessTime = aAccessTime;
    if (!mAccessed) {
      mAccessed = true;
    }
  }

  void LockedPersist();

  bool IsExtensionOrigin() { return mIsExtension; }

  nsTHashMap<nsStringHashKey, NotNull<CanonicalQuotaObject*>>
      mCanonicalQuotaObjects;
  ClientUsageArray mClientUsages;
  GroupInfo* mGroupInfo;
  const nsCString mOrigin;
  const nsCString mStorageOrigin;
  bool mIsExtension;
  uint64_t mUsage;
  int64_t mAccessTime;
  bool mIsPrivate;
  bool mAccessed;
  bool mPersisted;
  /**
   * In some special cases like the LocalStorage client where it's possible to
   * create a Quota-using representation but not actually write any data, we
   * want to be able to track quota for an origin without creating its origin
   * directory or the per-client files until they are actually needed to store
   * data. In those cases, the OriginInfo will be created by
   * EnsureQuotaForOrigin and the resulting mDirectoryExists will be false until
   * the origin actually needs to be created. It is possible for mUsage to be
   * greater than zero while mDirectoryExists is false, representing a state
   * where a client like LocalStorage has reserved quota for disk writes, but
   * has not yet flushed the data to disk.
   */
  bool mDirectoryExists;
};

class OriginInfoAccessTimeComparator {
 public:
  bool Equals(const NotNull<RefPtr<const OriginInfo>>& a,
              const NotNull<RefPtr<const OriginInfo>>& b) const {
    return a->LockedAccessTime() == b->LockedAccessTime();
  }

  bool LessThan(const NotNull<RefPtr<const OriginInfo>>& a,
                const NotNull<RefPtr<const OriginInfo>>& b) const {
    return a->LockedAccessTime() < b->LockedAccessTime();
  }
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGININFO_H_
