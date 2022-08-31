/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"
#include "gtest/gtest.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/ResultExtensions.h"

using namespace mozilla;
using namespace mozilla::dom::quota;

namespace {
class TestClass {
 public:
  static constexpr int kTestValue = 42;

  nsresult NonOverloadedNoInputComplex(std::pair<int, int>* aOut) {
    *aOut = std::pair{kTestValue, kTestValue};
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFailsComplex(std::pair<int, int>* aOut) {
    return NS_ERROR_FAILURE;
  }
};
}  // namespace

class DOM_Quota_ResultExtensions_ToResult : public DOM_Quota_Test {};
class DOM_Quota_ResultExtensions_GenericErrorResult : public DOM_Quota_Test {};

TEST_F(DOM_Quota_ResultExtensions_ToResult, FromBool) {
  // success
  {
    auto res = ToResult(true);
    static_assert(std::is_same_v<decltype(res), Result<Ok, nsresult>>);
    EXPECT_TRUE(res.isOk());
  }

  // failure
  {
    auto res = ToResult(false);
    static_assert(std::is_same_v<decltype(res), Result<Ok, nsresult>>);
    EXPECT_TRUE(res.isErr());
    EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
  }
}

TEST_F(DOM_Quota_ResultExtensions_ToResult, FromQMResult_Failure) {
  // copy
  {
    const auto res = ToQMResult(NS_ERROR_FAILURE);
    auto okOrErr = ToResult<QMResult>(res);
    static_assert(std::is_same_v<decltype(okOrErr), OkOrErr>);
    ASSERT_TRUE(okOrErr.isErr());
    auto err = okOrErr.unwrapErr();

#ifdef QM_ERROR_STACKS_ENABLED
    IncreaseExpectedStackId();

    ASSERT_EQ(err.StackId(), ExpectedStackId());
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
#else
    ASSERT_EQ(err, NS_ERROR_FAILURE);
#endif
  }

  // move
  {
    auto res = ToQMResult(NS_ERROR_FAILURE);
    auto okOrErr = ToResult<QMResult>(std::move(res));
    static_assert(std::is_same_v<decltype(okOrErr), OkOrErr>);
    ASSERT_TRUE(okOrErr.isErr());
    auto err = okOrErr.unwrapErr();

#ifdef QM_ERROR_STACKS_ENABLED
    IncreaseExpectedStackId();

    ASSERT_EQ(err.StackId(), ExpectedStackId());
    ASSERT_EQ(err.FrameId(), 1u);
    ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
#else
    ASSERT_EQ(err, NS_ERROR_FAILURE);
#endif
  }
}

TEST_F(DOM_Quota_ResultExtensions_ToResult, FromNSResult_Failure_Macro) {
  auto okOrErr = QM_TO_RESULT(NS_ERROR_FAILURE);
  static_assert(std::is_same_v<decltype(okOrErr), OkOrErr>);
  ASSERT_TRUE(okOrErr.isErr());
  auto err = okOrErr.unwrapErr();

#ifdef QM_ERROR_STACKS_ENABLED
  IncreaseExpectedStackId();

  ASSERT_EQ(err.StackId(), ExpectedStackId());
  ASSERT_EQ(err.FrameId(), 1u);
  ASSERT_EQ(err.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err, NS_ERROR_FAILURE);
#endif
}

TEST_F(DOM_Quota_ResultExtensions_GenericErrorResult, ErrorPropagation) {
  OkOrErr okOrErr1 = ToResult<QMResult>(ToQMResult(NS_ERROR_FAILURE));
  const auto& err1 = okOrErr1.inspectErr();

#ifdef QM_ERROR_STACKS_ENABLED
  IncreaseExpectedStackId();

  ASSERT_EQ(err1.StackId(), ExpectedStackId());
  ASSERT_EQ(err1.FrameId(), 1u);
  ASSERT_EQ(err1.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err1, NS_ERROR_FAILURE);
#endif

  OkOrErr okOrErr2 = okOrErr1.propagateErr();
  const auto& err2 = okOrErr2.inspectErr();

#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(err2.StackId(), ExpectedStackId());
  ASSERT_EQ(err2.FrameId(), 2u);
  ASSERT_EQ(err2.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err2, NS_ERROR_FAILURE);
#endif

  OkOrErr okOrErr3 = okOrErr2.propagateErr();
  const auto& err3 = okOrErr3.inspectErr();

#ifdef QM_ERROR_STACKS_ENABLED
  ASSERT_EQ(err3.StackId(), ExpectedStackId());
  ASSERT_EQ(err3.FrameId(), 3u);
  ASSERT_EQ(err3.NSResult(), NS_ERROR_FAILURE);
#else
  ASSERT_EQ(err3, NS_ERROR_FAILURE);
#endif
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_NoInput)
{
  auto res = ToResultGet<int32_t>([](nsresult* aRv) -> int32_t {
    *aRv = NS_OK;
    return 42;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_NoInput_Err)
{
  auto res = ToResultGet<int32_t>([](nsresult* aRv) -> int32_t {
    *aRv = NS_ERROR_FAILURE;
    return -1;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_WithInput)
{
  auto res = ToResultGet<int32_t>(
      [](int32_t aValue, nsresult* aRv) -> int32_t {
        *aRv = NS_OK;
        return aValue * 2;
      },
      42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 84);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_WithInput_Err)
{
  auto res = ToResultGet<int32_t>(
      [](int32_t aValue, nsresult* aRv) -> int32_t {
        *aRv = NS_ERROR_FAILURE;
        return -1;
      },
      42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_NoInput_Macro_Typed)
{
  auto res = MOZ_TO_RESULT_GET_TYPED(int32_t, [](nsresult* aRv) -> int32_t {
    *aRv = NS_OK;
    return 42;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_NoInput_Macro_Typed_Parens)
{
  auto res =
      MOZ_TO_RESULT_GET_TYPED((std::pair<int32_t, int32_t>),
                              [](nsresult* aRv) -> std::pair<int32_t, int32_t> {
                                *aRv = NS_OK;
                                return std::pair{42, 42};
                              });

  static_assert(std::is_same_v<decltype(res),
                               Result<std::pair<int32_t, int32_t>, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), (std::pair{42, 42}));
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_NoInput_Err_Macro_Typed)
{
  auto res = MOZ_TO_RESULT_GET_TYPED(int32_t, [](nsresult* aRv) -> int32_t {
    *aRv = NS_ERROR_FAILURE;
    return -1;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_WithInput_Macro_Typed)
{
  auto res = MOZ_TO_RESULT_GET_TYPED(
      int32_t,
      [](int32_t aValue, nsresult* aRv) -> int32_t {
        *aRv = NS_OK;
        return aValue * 2;
      },
      42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 84);
}

TEST(DOM_Quota_ResultExtensions_ToResultGet, Lambda_WithInput_Err_Macro_Typed)
{
  auto res = MOZ_TO_RESULT_GET_TYPED(
      int32_t,
      [](int32_t aValue, nsresult* aRv) -> int32_t {
        *aRv = NS_ERROR_FAILURE;
        return -1;
      },
      42);

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(DOM_Quota_ResultExtensions_ToResultInvoke, Lambda_NoInput_Complex)
{
  TestClass foo;

  // success
  {
    auto valOrErr =
        ToResultInvoke<std::pair<int, int>>([&foo](std::pair<int, int>* out) {
          return foo.NonOverloadedNoInputComplex(out);
        });
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ((std::pair{TestClass::kTestValue, TestClass::kTestValue}),
              valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvoke<std::pair<int, int>>([&foo](std::pair<int, int>* out) {
          return foo.NonOverloadedNoInputFailsComplex(out);
        });
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(DOM_Quota_ResultExtensions_ToResultInvoke,
     Lambda_NoInput_Complex_Macro_Typed)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_TYPED(
        (std::pair<int, int>), [&foo](std::pair<int, int>* out) {
          return foo.NonOverloadedNoInputComplex(out);
        });
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ((std::pair{TestClass::kTestValue, TestClass::kTestValue}),
              valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_TYPED(
        (std::pair<int, int>), [&foo](std::pair<int, int>* out) {
          return foo.NonOverloadedNoInputFailsComplex(out);
        });
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}
