/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotaobject_h__
#define mozilla_dom_quota_quotaobject_h__

// Local includes
#include "Client.h"

// Global includes
#include <cstdint>
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsStringFwd.h"

// XXX Avoid including this here by moving function bodies to the cpp file.
#include "mozilla/dom/quota/QuotaCommon.h"

namespace mozilla::dom::quota {

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

  [[nodiscard]] bool MaybeUpdateSize(int64_t aSize, bool aTruncate);

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

  MOZ_COUNTED_DTOR(QuotaObject)

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

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_quotaobject_h__
