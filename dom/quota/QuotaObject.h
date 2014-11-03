/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotaobject_h__
#define mozilla_dom_quota_quotaobject_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsDataHashtable.h"

#include "PersistenceType.h"

BEGIN_QUOTA_NAMESPACE

class GroupInfo;
class GroupInfoPair;
class OriginInfo;
class QuotaManager;

class QuotaObject
{
  friend class OriginInfo;
  friend class QuotaManager;

public:
  void
  AddRef();

  void
  Release();

  void
  UpdateSize(int64_t aSize);

  bool
  MaybeAllocateMoreSpace(int64_t aOffset, int32_t aCount);

private:
  QuotaObject(OriginInfo* aOriginInfo, const nsAString& aPath, int64_t aSize)
  : mOriginInfo(aOriginInfo), mPath(aPath), mSize(aSize)
  {
    MOZ_COUNT_CTOR(QuotaObject);
  }

  ~QuotaObject()
  {
    MOZ_COUNT_DTOR(QuotaObject);
  }

  already_AddRefed<QuotaObject>
  LockedAddRef()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    ++mRefCnt;

    nsRefPtr<QuotaObject> result = dont_AddRef(this);
    return result.forget();
  }

  mozilla::ThreadSafeAutoRefCnt mRefCnt;

  OriginInfo* mOriginInfo;
  nsString mPath;
  int64_t mSize;
};

class OriginInfo MOZ_FINAL
{
  friend class GroupInfo;
  friend class QuotaManager;
  friend class QuotaObject;

public:
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin, uint64_t aLimit,
             uint64_t aUsage, int64_t aAccessTime)
  : mGroupInfo(aGroupInfo), mOrigin(aOrigin), mLimit(aLimit), mUsage(aUsage),
    mAccessTime(aAccessTime)
  {
    MOZ_COUNT_CTOR(OriginInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  int64_t
  AccessTime() const
  {
    return mAccessTime;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~OriginInfo()
  {
    MOZ_COUNT_DTOR(OriginInfo);
  }

  void
  LockedDecreaseUsage(int64_t aSize);

  void
  LockedUpdateAccessTime(int64_t aAccessTime)
  {
    AssertCurrentThreadOwnsQuotaMutex();

    mAccessTime = aAccessTime;
  }

  void
  LockedClearOriginInfos()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    mQuotaObjects.EnumerateRead(ClearOriginInfoCallback, nullptr);
  }

  static PLDHashOperator
  ClearOriginInfoCallback(const nsAString& aKey,
                          QuotaObject* aValue, void* aUserArg);

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  GroupInfo* mGroupInfo;
  nsCString mOrigin;
  uint64_t mLimit;
  uint64_t mUsage;
  int64_t mAccessTime;
};

class OriginInfoLRUComparator
{
public:
  bool
  Equals(const OriginInfo* a, const OriginInfo* b) const
  {
    return
      a && b ? a->AccessTime() == b->AccessTime() : !a && !b ? true : false;
  }

  bool
  LessThan(const OriginInfo* a, const OriginInfo* b) const
  {
    return a && b ? a->AccessTime() < b->AccessTime() : b ? true : false;
  }
};

class GroupInfo MOZ_FINAL
{
  friend class GroupInfoPair;
  friend class OriginInfo;
  friend class QuotaManager;
  friend class QuotaObject;

public:
  GroupInfo(PersistenceType aPersistenceType, const nsACString& aGroup)
  : mPersistenceType(aPersistenceType), mGroup(aGroup), mUsage(0)
  {
    MOZ_COUNT_CTOR(GroupInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GroupInfo)

  bool
  IsForPersistentStorage() const
  {
    return mPersistenceType == PERSISTENCE_TYPE_PERSISTENT;
  }

  bool
  IsForTemporaryStorage() const
  {
    return mPersistenceType == PERSISTENCE_TYPE_TEMPORARY;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~GroupInfo()
  {
    MOZ_COUNT_DTOR(GroupInfo);
  }

  already_AddRefed<OriginInfo>
  LockedGetOriginInfo(const nsACString& aOrigin);

  void
  LockedAddOriginInfo(OriginInfo* aOriginInfo);

  void
  LockedRemoveOriginInfo(const nsACString& aOrigin);

  void
  LockedRemoveOriginInfos();

  void
  LockedRemoveOriginInfosForPattern(const nsACString& aPattern);

  bool
  LockedHasOriginInfos()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return !mOriginInfos.IsEmpty();
  }

  nsTArray<nsRefPtr<OriginInfo> > mOriginInfos;

  PersistenceType mPersistenceType;
  nsCString mGroup;
  uint64_t mUsage;
};

class GroupInfoPair
{
  friend class QuotaManager;

public:
  GroupInfoPair()
  {
    MOZ_COUNT_CTOR(GroupInfoPair);
  }

  ~GroupInfoPair()
  {
    MOZ_COUNT_DTOR(GroupInfoPair);
  }

private:
  already_AddRefed<GroupInfo>
  LockedGetGroupInfo(PersistenceType aPersistenceType)
  {
    AssertCurrentThreadOwnsQuotaMutex();

    nsRefPtr<GroupInfo> groupInfo =
      GetGroupInfoForPersistenceType(aPersistenceType);
    return groupInfo.forget();
  }

  void
  LockedSetGroupInfo(GroupInfo* aGroupInfo)
  {
    AssertCurrentThreadOwnsQuotaMutex();

    nsRefPtr<GroupInfo>& groupInfo =
      GetGroupInfoForPersistenceType(aGroupInfo->mPersistenceType);
    groupInfo = aGroupInfo;
  }

  void
  LockedClearGroupInfo(PersistenceType aPersistenceType)
  {
    AssertCurrentThreadOwnsQuotaMutex();

    nsRefPtr<GroupInfo>& groupInfo =
      GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = nullptr;
  }

  bool
  LockedHasGroupInfos()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return mPersistentStorageGroupInfo || mTemporaryStorageGroupInfo;
  }

  nsRefPtr<GroupInfo>&
  GetGroupInfoForPersistenceType(PersistenceType aPersistenceType);

  nsRefPtr<GroupInfo> mPersistentStorageGroupInfo;
  nsRefPtr<GroupInfo> mTemporaryStorageGroupInfo;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_quotaobject_h__
