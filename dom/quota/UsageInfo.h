/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_usageinfo_h__
#define mozilla_dom_quota_usageinfo_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/CheckedInt.h"

BEGIN_QUOTA_NAMESPACE

enum struct UsageKind { Database, File };

template <UsageKind Kind>
struct Usage {
  explicit Usage(Maybe<uint64_t> aValue = Nothing{}) : mValue(aValue) {}

  Maybe<uint64_t> GetValue() const { return mValue; }

  Usage& operator+=(const Usage aDelta) {
    if (aDelta.mValue.isSome()) {
      CheckedUint64 value = mValue.valueOr(0);

      value += aDelta.mValue.value();

      mValue = Some(value.isValid() ? value.value() : UINT64_MAX);
    }

    return *this;
  }

  Usage operator+(const Usage aDelta) const {
    Usage res = *this;
    res += aDelta;
    return res;
  }

 private:
  Maybe<uint64_t> mValue;
};

using DatabaseUsageType = Usage<UsageKind::Database>;
using FileUsageType = Usage<UsageKind::File>;

class UsageInfo final {
 public:
  UsageInfo& operator+=(const UsageInfo& aUsageInfo) {
    mDatabaseUsage += aUsageInfo.mDatabaseUsage;
    mFileUsage += aUsageInfo.mFileUsage;
    return *this;
  }

  UsageInfo& operator+=(const DatabaseUsageType aUsage) {
    mDatabaseUsage += aUsage;
    return *this;
  }

  UsageInfo& operator+=(const FileUsageType aUsage) {
    mFileUsage += aUsage;
    return *this;
  }

  Maybe<uint64_t> FileUsage() const { return mFileUsage.GetValue(); }

  Maybe<uint64_t> TotalUsage() const {
    const uint64_t res =
        mDatabaseUsage.GetValue().valueOr(0) + FileUsage().valueOr(0);
    return res ? Some(res) : Nothing();
  }

 private:
  DatabaseUsageType mDatabaseUsage;
  FileUsageType mFileUsage;
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_usageinfo_h__
