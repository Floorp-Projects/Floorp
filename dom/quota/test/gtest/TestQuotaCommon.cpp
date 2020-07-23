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
    auto task = []() -> Result<uint32_t, nsresult> { return 42; };

    uint32_t x;
    QM_TRY_VAR(x, task());

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
    auto task = []() -> Result<uint32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    uint32_t x;
    QM_TRY_VAR(x, task());

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
    auto task = []() -> Result<uint32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    uint32_t x;
    QM_TRY_VAR(x, task(), NS_ERROR_UNEXPECTED);

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
    auto task = []() -> Result<uint32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    uint32_t x;
    QM_TRY_VAR(x, task(), QM_VOID);

    flag = true;
  }();

  EXPECT_FALSE(flag);
}
