/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_usageinfo_h__
#define mozilla_dom_quota_usageinfo_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/Atomics.h"
#include "Utilities.h"

BEGIN_QUOTA_NAMESPACE

class UsageInfo
{
public:
  UsageInfo()
  : mCanceled(false), mDatabaseUsage(0), mFileUsage(0)
  { }

  virtual ~UsageInfo()
  { }

  bool
  Canceled()
  {
    return mCanceled;
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

protected:
  mozilla::Atomic<bool> mCanceled;

private:
  uint64_t mDatabaseUsage;
  uint64_t mFileUsage;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_usageinfo_h__
