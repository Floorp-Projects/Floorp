/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotaobject_h__
#define mozilla_dom_quota_quotaobject_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsDataHashtable.h"

#include "Client.h"
#include "PersistenceType.h"

BEGIN_QUOTA_NAMESPACE

class OriginInfo;
class QuotaManager;

class QuotaObject {
  friend class OriginInfo;
  friend class QuotaManager;

  class StoragePressureRunnable;

 public:
  void AddRef();

  void Release();

  const nsAString& Path() const { return mPath; }

  bool MaybeUpdateSize(int64_t aSize, bool aTruncate);

  bool IncreaseSize(int64_t aDelta);

  void DisableQuotaCheck();

  void EnableQuotaCheck();

 private:
  QuotaObject(OriginInfo* aOriginInfo, Client::Type aClientType,
              const nsAString& aPath, int64_t aSize)
      : mOriginInfo(aOriginInfo),
        mPath(aPath),
        mSize(aSize),
        mClientType(aClientType),
        mQuotaCheckDisabled(false),
        mWritingDone(false) {
    MOZ_COUNT_CTOR(QuotaObject);
  }

  ~QuotaObject() { MOZ_COUNT_DTOR(QuotaObject); }

  already_AddRefed<QuotaObject> LockedAddRef() {
    AssertCurrentThreadOwnsQuotaMutex();

    ++mRefCnt;

    RefPtr<QuotaObject> result = dont_AddRef(this);
    return result.forget();
  }

  bool LockedMaybeUpdateSize(int64_t aSize, bool aTruncate);

  mozilla::ThreadSafeAutoRefCnt mRefCnt;

  OriginInfo* mOriginInfo;
  nsString mPath;
  int64_t mSize;
  Client::Type mClientType;
  bool mQuotaCheckDisabled;
  bool mWritingDone;
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_quotaobject_h__
