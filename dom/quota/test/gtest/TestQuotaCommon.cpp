/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

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

TEST(QuotaCommon_Try, Success_WithCleanup)
{
  bool flag = false;
  bool flag2 = false;

  nsresult rv = [&]() -> nsresult {
    QM_TRY(NS_OK, QM_PROPAGATE, [&flag2]() { flag2 = true; });

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_OK);
  EXPECT_TRUE(flag);
  EXPECT_FALSE(flag2);
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

TEST(QuotaCommon_Try, Failure_WithCleanup)
{
  bool flag = false;
  bool flag2 = false;

  nsresult rv = [&]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE, QM_PROPAGATE, [&flag2]() { flag2 = true; });

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
  EXPECT_TRUE(flag2);
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

TEST(QuotaCommon_TryVar, Success_WithCleanup)
{
  bool flag = false;
  bool flag2 = false;

  nsresult rv = [&]() -> nsresult {
    auto task = []() -> Result<int32_t, nsresult> { return 42; };

    int32_t x = 0;
    QM_TRY_VAR(x, task(), QM_PROPAGATE, [&flag2]() { flag2 = true; });
    EXPECT_EQ(x, 42);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_OK);
  EXPECT_TRUE(flag);
  EXPECT_FALSE(flag2);
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

TEST(QuotaCommon_TryVar, Failure_WithCleanup)
{
  bool flag = false;
  bool flag2 = false;

  nsresult rv = [&]() -> nsresult {
    auto task = []() -> Result<int32_t, nsresult> {
      return Err(NS_ERROR_FAILURE);
    };

    int32_t x = 0;
    QM_TRY_VAR(x, task(), QM_PROPAGATE, [&flag2]() { flag2 = true; });
    EXPECT_EQ(x, 0);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
  EXPECT_TRUE(flag2);
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

TEST(QuotaCommon_Fail, ReturnValue)
{
  bool flag = false;

  nsresult rv = [&]() -> nsresult {
    QM_FAIL(NS_ERROR_FAILURE);

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
}

TEST(QuotaCommon_Fail, ReturnValue_WithCleanup)
{
  bool flag = false;
  bool flag2 = false;

  nsresult rv = [&]() -> nsresult {
    QM_FAIL(NS_ERROR_FAILURE, [&flag2]() { flag2 = true; });

    flag = true;

    return NS_OK;
  }();

  EXPECT_EQ(rv, NS_ERROR_FAILURE);
  EXPECT_FALSE(flag);
  EXPECT_TRUE(flag2);
}

TEST(QuotaCommon_OkIf, True)
{
  auto res = OkIf(true);

  EXPECT_TRUE(res.isOk());
}

TEST(QuotaCommon_OkIf, False)
{
  auto res = OkIf(false);

  EXPECT_TRUE(res.isErr());
}

TEST(QuotaCommon_OkToOk, OkToBool_True)
{
  auto res = OkToOk<true>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), true);
}

TEST(QuotaCommon_OkToOk, OkToBool_False)
{
  auto res = OkToOk<false>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), false);
}

TEST(QuotaCommon_OkToOk, OkToInt_42)
{
  auto res = OkToOk<42>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToBool_True)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, true>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), true);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToBool_True_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, true>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToBool_False)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, false>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), false);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToBool_False_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, false>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToInt_42)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, 42>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToInt_42_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, 42>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToNsCOMPtr_nullptr)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, nullptr, nsCOMPtr<nsISupports>>(
      NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), nullptr);
}

TEST(QuotaCommon_ErrToOkOrErr, NsresultToNsCOMPtr_nullptr_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, nullptr, nsCOMPtr<nsISupports>>(
      NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

class StringPairParameterized
    : public ::testing::TestWithParam<std::pair<const char*, const char*>> {};

TEST_P(StringPairParameterized, AnonymizedOriginString) {
  const auto [in, expectedAnonymized] = GetParam();
  const auto anonymized = AnonymizedOriginString(nsDependentCString(in));
  EXPECT_STREQ(anonymized.get(), expectedAnonymized);
}

INSTANTIATE_TEST_CASE_P(
    QuotaCommon, StringPairParameterized,
    ::testing::Values(
        // XXX Do we really want to anonymize about: origins?
        std::pair("about:home", "about:aaaa"),
        std::pair("https://foo.bar.com", "https://aaa.aaa.aaa"),
        std::pair("https://foo.bar.com:8000", "https://aaa.aaa.aaa:DDDD"),
        std::pair("file://UNIVERSAL_FILE_ORIGIN",
                  "file://aaaaaaaaa_aaaa_aaaaaa")));
TEST(QuotaCommon_ToResultGet, Lambda_NoInput)
{
  auto task = [](nsresult* aRv) -> int32_t {
    *aRv = NS_OK;
    return 42;
  };

  auto res = ToResultGet<int32_t>(task);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ToResultGet, Lambda_NoInput_Err)
{
  auto task = [](nsresult* aRv) -> int32_t {
    *aRv = NS_ERROR_FAILURE;
    return -1;
  };

  auto res = ToResultGet<int32_t>(task);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_ToResultGet, Lambda_WithInput)
{
  auto task = [](int32_t aValue, nsresult* aRv) -> int32_t {
    *aRv = NS_OK;
    return aValue * 2;
  };

  auto res = ToResultGet<int32_t>(task, 42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 84);
}

TEST(QuotaCommon_ToResultGet, Lambda_WithInput_Err)
{
  auto task = [](int32_t aValue, nsresult* aRv) -> int32_t {
    *aRv = NS_ERROR_FAILURE;
    return -1;
  };

  auto res = ToResultGet<int32_t>(task, 42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

// BEGIN COPY FROM mfbt/tests/TestResult.cpp
struct Failed {
  int x;
};

static GenericErrorResult<Failed&> Fail() {
  static Failed failed;
  return Err<Failed&>(failed);
}

static Result<Ok, Failed&> Task1(bool pass) {
  if (!pass) {
    return Fail();  // implicit conversion from GenericErrorResult to Result
  }
  return Ok();
}
// END COPY FROM mfbt/tests/TestResult.cpp

static Result<bool, Failed&> Condition(bool aNoError, bool aResult) {
  return Task1(aNoError).map([aResult](auto) { return aResult; });
}

TEST(QuotaCommon_CollectWhileTest, NoFailures)
{
  const size_t loopCount = 5;
  size_t conditionExecutions = 0;
  size_t bodyExecutions = 0;
  auto result = CollectWhile(
      [&conditionExecutions] {
        ++conditionExecutions;
        return Condition(true, conditionExecutions <= loopCount);
      },
      [&bodyExecutions] {
        ++bodyExecutions;
        return Task1(true);
      });
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed&>>);
  MOZ_RELEASE_ASSERT(result.isOk());
  MOZ_RELEASE_ASSERT(loopCount == bodyExecutions);
  MOZ_RELEASE_ASSERT(1 + loopCount == conditionExecutions);
}

TEST(QuotaCommon_CollectWhileTest, BodyFailsImmediately)
{
  size_t conditionExecutions = 0;
  size_t bodyExecutions = 0;
  auto result = CollectWhile(
      [&conditionExecutions] {
        ++conditionExecutions;
        return Condition(true, true);
      },
      [&bodyExecutions] {
        ++bodyExecutions;
        return Task1(false);
      });
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed&>>);
  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(1 == bodyExecutions);
  MOZ_RELEASE_ASSERT(1 == conditionExecutions);
}

TEST(QuotaCommon_CollectWhileTest, BodyFailsOnSecondExecution)
{
  size_t conditionExecutions = 0;
  size_t bodyExecutions = 0;
  auto result = CollectWhile(
      [&conditionExecutions] {
        ++conditionExecutions;
        return Condition(true, true);
      },
      [&bodyExecutions] {
        ++bodyExecutions;
        return Task1(bodyExecutions < 2);
      });
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed&>>);
  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(2 == bodyExecutions);
  MOZ_RELEASE_ASSERT(2 == conditionExecutions);
}

TEST(QuotaCommon_CollectWhileTest, ConditionFailsImmediately)
{
  size_t conditionExecutions = 0;
  size_t bodyExecutions = 0;
  auto result = CollectWhile(
      [&conditionExecutions] {
        ++conditionExecutions;
        return Condition(false, true);
      },
      [&bodyExecutions] {
        ++bodyExecutions;
        return Task1(true);
      });
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed&>>);
  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(0 == bodyExecutions);
  MOZ_RELEASE_ASSERT(1 == conditionExecutions);
}

TEST(QuotaCommon_CollectWhileTest, ConditionFailsOnSecondExecution)
{
  size_t conditionExecutions = 0;
  size_t bodyExecutions = 0;
  auto result = CollectWhile(
      [&conditionExecutions] {
        ++conditionExecutions;
        return Condition(conditionExecutions < 2, true);
      },
      [&bodyExecutions] {
        ++bodyExecutions;
        return Task1(true);
      });
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed&>>);
  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(1 == bodyExecutions);
  MOZ_RELEASE_ASSERT(2 == conditionExecutions);
}
