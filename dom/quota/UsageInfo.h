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

class UsageInfo final {
 public:
  UsageInfo& operator+=(const UsageInfo& aUsageInfo) {
    mDatabaseUsage += aUsageInfo.mDatabaseUsage;
    mFileUsage += aUsageInfo.mFileUsage;
    return *this;
  }

  void IncrementDatabaseUsage(const Maybe<uint64_t>& aUsage) {
    mDatabaseUsage += aUsage;
  }

  void IncrementFileUsage(const Maybe<uint64_t>& aUsage) {
    mFileUsage += aUsage;
  }

  Maybe<uint64_t> FileUsage() const { return mFileUsage.GetValue(); }

  Maybe<uint64_t> TotalUsage() const { return mDatabaseUsage + mFileUsage; }

  struct Usage {
    MOZ_IMPLICIT Usage(Maybe<uint64_t> aValue = Nothing{}) : mValue(aValue) {}

    MOZ_IMPLICIT operator Maybe<uint64_t>() const { return mValue; }

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

 private:
  Usage mDatabaseUsage;
  Usage mFileUsage;
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_usageinfo_h__
