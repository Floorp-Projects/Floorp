/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_usageinfo_h__
#define mozilla_dom_quota_usageinfo_h__

#include <cstdint>
#include <utility>
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"

namespace mozilla::dom::quota {

enum struct UsageKind { Database, File };

namespace detail {
inline void AddCapped(Maybe<uint64_t>& aValue, const Maybe<uint64_t> aDelta) {
  if (aDelta.isSome()) {
    CheckedUint64 value = aValue.valueOr(0);

    value += aDelta.value();

    aValue = Some(value.isValid() ? value.value() : UINT64_MAX);
  }
}

template <UsageKind Kind>
struct Usage {
  explicit Usage(Maybe<uint64_t> aValue = Nothing{}) : mValue(aValue) {}

  Maybe<uint64_t> GetValue() const { return mValue; }

  Usage& operator+=(const Usage aDelta) {
    AddCapped(mValue, aDelta.mValue);

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
}  // namespace detail

using DatabaseUsageType = detail::Usage<UsageKind::Database>;
using FileUsageType = detail::Usage<UsageKind::File>;

class UsageInfo final {
 public:
  UsageInfo() = default;

  explicit UsageInfo(const DatabaseUsageType aUsage) : mDatabaseUsage(aUsage) {}

  explicit UsageInfo(const FileUsageType aUsage) : mFileUsage(aUsage) {}

  UsageInfo operator+(const UsageInfo& aUsageInfo) {
    UsageInfo res = *this;
    res += aUsageInfo;
    return res;
  }

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

  Maybe<uint64_t> DatabaseUsage() const { return mDatabaseUsage.GetValue(); }

  Maybe<uint64_t> FileUsage() const { return mFileUsage.GetValue(); }

  Maybe<uint64_t> TotalUsage() const {
    Maybe<uint64_t> res = mDatabaseUsage.GetValue();
    detail::AddCapped(res, FileUsage());
    return res;
  }

 private:
  DatabaseUsageType mDatabaseUsage;
  FileUsageType mFileUsage;
};

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_usageinfo_h__
