/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotaobject_h__
#define mozilla_dom_quota_quotaobject_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsDataHashtable.h"

BEGIN_QUOTA_NAMESPACE

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
  { }

  virtual ~QuotaObject()
  { }

  mozilla::ThreadSafeAutoRefCnt mRefCnt;

  OriginInfo* mOriginInfo;
  nsString mPath;
  int64_t mSize;
};

class OriginInfo
{
  friend class QuotaManager;
  friend class QuotaObject;

public:
  OriginInfo(const nsACString& aOrigin, int64_t aLimit, int64_t aUsage)
  : mOrigin(aOrigin), mLimit(aLimit), mUsage(aUsage)
  {
    mQuotaObjects.Init();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

private:
  void
#ifdef DEBUG
  LockedClearOriginInfos();
#else
  LockedClearOriginInfos()
  {
    mQuotaObjects.EnumerateRead(ClearOriginInfoCallback, nullptr);
  }
#endif

  static PLDHashOperator
  ClearOriginInfoCallback(const nsAString& aKey,
                          QuotaObject* aValue, void* aUserArg);

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  nsCString mOrigin;
  int64_t mLimit;
  int64_t mUsage;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_quotaobject_h__
