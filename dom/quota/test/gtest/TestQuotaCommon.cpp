/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/ResultExtensions.h"

using namespace mozilla;
using namespace mozilla::dom::quota;

TEST(QuotaCommon_Try, Success)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    QM_TRY(NS_OK);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_OK);
  EXPECT_TRUE(flag);
}

TEST(QuotaCommon_Try, Failure_PropagateErr)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_Try, Failure_CustomErr)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE, NS_ERROR_UNEXPECTED);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_Try, Failure_NoErr)
{
  bool flag = false;

  [&]() -> void {
    QM_TRY(NS_ERROR_FAILURE, QM_VOID);

    flag = true;
  }();

  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_TryVar, Success)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    auto task = []() -> Result<int32_t, nsresult> { return 42; };

    int32_t x = 0;
    QM_TRY_VAR(x, task());
    EXPECT_EQ(x, 42);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_OK);
  EXPECT_TRUE(flag);
}

TEST(QuotaCommon_TryVar, Failure_PropagateErr)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    auto task = []() -> Result<int32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    int32_t x = 0;
    QM_TRY_VAR(x, task());
    EXPECT_EQ(x, 0);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_TryVar, Failure_CustomErr)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    auto task = []() -> Result<int32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    int32_t x = 0;
    QM_TRY_VAR(x, task(), NS_ERROR_UNEXPECTED);
    EXPECT_EQ(x, 0);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_TryVar, Failure_NoErr)
{
  bool flag = false;

  [&]() -> void {
    auto task = []() -> Result<int32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    int32_t x = 0;
    QM_TRY_VAR(x, task(), QM_VOID);
    EXPECT_EQ(x, 0);

    flag = true;
  }();

  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_TryVar, Decl)
{
  auto task = []() -> Result<int32_t, nsresult> { return 42; };

  QM_TRY_VAR(int32_t x, task(), QM_VOID);

  static_assert(std::is_same_v<decltype(x), int32_t>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryVar, ConstDecl)
{
  auto task = []() -> Result<int32_t, nsresult> { return 42; };

  QM_TRY_VAR(const int32_t x, task(), QM_VOID);

  static_assert(std::is_same_v<decltype(x), const int32_t>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryVar, SameScopeDecl)
{
  auto task = []() -> Result<int32_t, nsresult> { return 42; };

  QM_TRY_VAR(const int32_t x, task(), QM_VOID);
  EXPECT_EQ(x, 42);

  QM_TRY_VAR(const int32_t y, task(), QM_VOID);
  EXPECT_EQ(y, 42);
}

TEST(QuotaCommon_TryVar, ParenDecl)
{
  auto task = []() -> Result<std::pair<int32_t, bool>, nsresult> {
    return std::pair{42, true};
  };

  QM_TRY_VAR((const auto [x, y]), task(), QM_VOID);

  static_assert(std::is_same_v<decltype(x), const int32_t>);
  static_assert(std::is_same_v<decltype(y), const bool>);

  EXPECT_EQ(x, 42);
  EXPECT_EQ(y, true);
}

TEST(QuotaCommon_OkIf, True)
{
  auto res = OkIf(true);

  EXPECT_TRUE(res.isOk());
}

TEST(QuotaCommon_SuccessIf, False)
{
  auto res = OkIf(false);

  EXPECT_TRUE(res.isErr());
}

TEST(QuotaCommon_ToResult, SuccessEnforcer_Success)
{
  bool flag = false;

  auto res = ToResult(NS_OK, [&](auto aValue) {
    flag = true;

    return false;
  });

  EXPECT_TRUE(res.isOk());
  EXPECT_FALSE(res.unwrap());
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_ToResult, SuccessEnforcer_Failure_EnforcerReturnsTrue)
{
  bool flag = false;

  auto res = ToResult(NS_ERROR_FAILURE, [&](auto aValue) {
    flag = true;

    return true;
  });

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(res.unwrap());
  EXPECT_TRUE(flag);
}

TEST(QuotaCommon_ToResult, SuccessEnforcer_Failure_EnforcerReturnsFalse)
{
  bool flag = false;

  auto res = ToResult(NS_ERROR_FAILURE, [&](auto aValue) {
    flag = true;

    return false;
  });

  EXPECT_TRUE(res.isErr());
  EXPECT_TRUE(flag);
}
