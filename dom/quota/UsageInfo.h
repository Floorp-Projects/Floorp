/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_usageinfo_h__
#define mozilla_dom_quota_usageinfo_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/Atomics.h"
#include "mozilla/CheckedInt.h"

BEGIN_QUOTA_NAMESPACE

class UsageInfo
{
public:
  UsageInfo()
    : mDatabaseUsage(0)
    , mFileUsage(0)
    , mLimit(0)
  { }

  virtual ~UsageInfo()
  { }

  void
  Append(const UsageInfo& aUsageInfo)
  {
    IncrementUsage(&mDatabaseUsage, aUsageInfo.mDatabaseUsage);
    IncrementUsage(&mFileUsage, aUsageInfo.mFileUsage);
  }

  void
  AppendToDatabaseUsage(uint64_t aUsage)
  {
    IncrementUsage(&mDatabaseUsage, aUsage);
  }

  void
  AppendToFileUsage(uint64_t aUsage)
  {
    IncrementUsage(&mFileUsage, aUsage);
  }

  void
  SetLimit(uint64_t aLimit)
  {
    mLimit = aLimit;
  }

  uint64_t
  DatabaseUsage()
  {
    return mDatabaseUsage;
  }

  uint64_t
  FileUsage()
  {
    return mFileUsage;
  }

  uint64_t
  Limit()
  {
    return mLimit;
  }

  uint64_t
  TotalUsage()
  {
    uint64_t totalUsage = mDatabaseUsage;
    IncrementUsage(&totalUsage, mFileUsage);
    return totalUsage;
  }

  void
  ResetUsage()
  {
    mDatabaseUsage = 0;
    mFileUsage = 0;
  }

  static void
  IncrementUsage(uint64_t* aUsage, uint64_t aDelta)
  {
    MOZ_ASSERT(aUsage);
    CheckedUint64 value = *aUsage;
    value += aDelta;
    if (value.isValid()) {
      *aUsage = value.value();
    } else {
      *aUsage = UINT64_MAX;
    }
  }

private:
  uint64_t mDatabaseUsage;
  uint64_t mFileUsage;
  uint64_t mLimit;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_usageinfo_h__
