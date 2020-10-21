/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/UsageInfo.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>
#include "mozilla/Maybe.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/fallible.h"

using namespace mozilla;
using namespace mozilla::dom::quota;

namespace {
constexpr uint64_t kTestValue = 42;
constexpr uint64_t kTestValueZero = 0;
}  // namespace

TEST(DOM_Quota_UsageInfo, DefaultConstructed)
{
  const UsageInfo usageInfo;
  EXPECT_EQ(Nothing(), usageInfo.FileUsage());
  EXPECT_EQ(Nothing(), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, FileOnly)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Some(kTestValue), usageInfo.FileUsage());
  EXPECT_EQ(Some(kTestValue), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, DatabaseOnly)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += DatabaseUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Nothing(), usageInfo.FileUsage());
  EXPECT_EQ(Some(kTestValue), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, FileOnly_Zero)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(kTestValueZero));
    return usageInfo;
  }();
  EXPECT_EQ(Some(kTestValueZero), usageInfo.FileUsage());
  EXPECT_EQ(Some(kTestValueZero), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, DatabaseOnly_Zero)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += DatabaseUsageType(Some(kTestValueZero));
    return usageInfo;
  }();
  EXPECT_EQ(Nothing(), usageInfo.FileUsage());
  EXPECT_EQ(Some(kTestValueZero), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, Both)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(kTestValue));
    usageInfo += DatabaseUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Some(kTestValue), usageInfo.FileUsage());
  EXPECT_EQ(Some(2 * kTestValue), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, Both_Zero)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(kTestValueZero));
    usageInfo += DatabaseUsageType(Some(kTestValueZero));
    return usageInfo;
  }();
  EXPECT_EQ(Some(kTestValueZero), usageInfo.FileUsage());
  EXPECT_EQ(Some(kTestValueZero), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, CapCombined)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(UINT64_MAX));
    usageInfo += DatabaseUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Some(UINT64_MAX), usageInfo.FileUsage());
  EXPECT_EQ(Some(UINT64_MAX), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, CapFileUsage)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += FileUsageType(Some(UINT64_MAX));
    usageInfo += FileUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Some(UINT64_MAX), usageInfo.FileUsage());
  EXPECT_EQ(Some(UINT64_MAX), usageInfo.TotalUsage());
}

TEST(DOM_Quota_UsageInfo, CapDatabaseUsage)
{
  const UsageInfo usageInfo = [] {
    UsageInfo usageInfo;
    usageInfo += DatabaseUsageType(Some(UINT64_MAX));
    usageInfo += DatabaseUsageType(Some(kTestValue));
    return usageInfo;
  }();
  EXPECT_EQ(Nothing(), usageInfo.FileUsage());
  EXPECT_EQ(Some(UINT64_MAX), usageInfo.TotalUsage());
}
