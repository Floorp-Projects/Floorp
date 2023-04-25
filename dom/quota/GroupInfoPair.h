/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_GROUPINFOPAIR_H_
#define DOM_QUOTA_GROUPINFOPAIR_H_

#include "GroupInfo.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

namespace mozilla::dom::quota {

class GroupInfo;

// XXX Consider a new name for this class, it has other data members now and an
// additional GroupInfo (it's not a pair of GroupInfo objects anymore).
class GroupInfoPair {
 public:
  GroupInfoPair(const nsACString& aSuffix, const nsACString& aGroup)
      : mSuffix(aSuffix), mGroup(aGroup) {
    MOZ_COUNT_CTOR(GroupInfoPair);
  }

  MOZ_COUNTED_DTOR(GroupInfoPair)

  const nsCString& Suffix() const { return mSuffix; }

  const nsCString& Group() const { return mGroup; }

  RefPtr<GroupInfo> LockedGetGroupInfo(PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    return GetGroupInfoForPersistenceType(aPersistenceType);
  }

  void LockedSetGroupInfo(PersistenceType aPersistenceType,
                          GroupInfo* aGroupInfo) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = aGroupInfo;
  }

  void LockedClearGroupInfo(PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = nullptr;
  }

  bool LockedHasGroupInfos() {
    AssertCurrentThreadOwnsQuotaMutex();

    return mTemporaryStorageGroupInfo || mDefaultStorageGroupInfo ||
           mPrivateStorageGroupInfo;
  }

 private:
  RefPtr<GroupInfo>& GetGroupInfoForPersistenceType(
      PersistenceType aPersistenceType);

  const nsCString mSuffix;
  const nsCString mGroup;
  RefPtr<GroupInfo> mTemporaryStorageGroupInfo;
  RefPtr<GroupInfo> mDefaultStorageGroupInfo;
  RefPtr<GroupInfo> mPrivateStorageGroupInfo;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_GROUPINFOPAIR_H_
