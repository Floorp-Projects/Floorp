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

class UsageInfo final {
 public:
  void Append(const UsageInfo& aUsageInfo) {
    IncrementUsage(mDatabaseUsage, aUsageInfo.mDatabaseUsage);
    IncrementUsage(mFileUsage, aUsageInfo.mFileUsage);
  }

  void AppendToDatabaseUsage(const Maybe<uint64_t>& aUsage) {
    IncrementUsage(mDatabaseUsage, aUsage);
  }

  void AppendToFileUsage(const Maybe<uint64_t>& aUsage) {
    IncrementUsage(mFileUsage, aUsage);
  }

  const Maybe<uint64_t>& DatabaseUsage() const { return mDatabaseUsage; }

  const Maybe<uint64_t>& FileUsage() const { return mFileUsage; }

  Maybe<uint64_t> TotalUsage() {
    Maybe<uint64_t> totalUsage;

    IncrementUsage(totalUsage, mDatabaseUsage);
    IncrementUsage(totalUsage, mFileUsage);

    return totalUsage;
  }

  static void IncrementUsage(Maybe<uint64_t>& aUsage,
                             const Maybe<uint64_t>& aDelta) {
    if (aDelta.isNothing()) {
      return;
    }

    CheckedUint64 value = aUsage.valueOr(0);

    value += aDelta.value();

    if (value.isValid()) {
      aUsage = Some(value.value());
    } else {
      aUsage = Some(UINT64_MAX);
    }
  }

 private:
  Maybe<uint64_t> mDatabaseUsage;
  Maybe<uint64_t> mFileUsage;
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_usageinfo_h__
