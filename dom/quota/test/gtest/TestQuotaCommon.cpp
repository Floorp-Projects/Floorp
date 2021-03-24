/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/QuotaCommon.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <new>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>
#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Unused.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTLiteralString.h"

class nsISupports;

using namespace mozilla;
using namespace mozilla::dom::quota;

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunreachable-code"
#endif

TEST(QuotaCommon_Try, Success)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_OK);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

#ifdef DEBUG
TEST(QuotaCommon_Try, Success_CustomErr_AssertUnreachable)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_OK, QM_ASSERT_UNREACHABLE);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, Success_NoErr_AssertUnreachable)
{
  bool tryDidNotReturn = false;

  [&tryDidNotReturn]() -> void {
    QM_TRY(NS_OK, QM_ASSERT_UNREACHABLE_VOID);

    tryDidNotReturn = true;
  }();

  EXPECT_TRUE(tryDidNotReturn);
}
#else
#  if defined(QM_ASSERT_UNREACHABLE) || defined(QM_ASSERT_UNREACHABLE_VOID)
#error QM_ASSERT_UNREACHABLE and QM_ASSERT_UNREACHABLE_VOID should not be defined.
#  endif
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
TEST(QuotaCommon_Try, Success_CustomErr_DiagnosticAssertUnreachable)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_OK, QM_DIAGNOSTIC_ASSERT_UNREACHABLE);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, Success_NoErr_DiagnosticAssertUnreachable)
{
  bool tryDidNotReturn = false;

  [&tryDidNotReturn]() -> void {
    QM_TRY(NS_OK, QM_DIAGNOSTIC_ASSERT_UNREACHABLE_VOID);

    tryDidNotReturn = true;
  }();

  EXPECT_TRUE(tryDidNotReturn);
}
#else
#  if defined(QM_DIAGNOSTIC_ASSERT_UNREACHABLE) || \
      defined(QM_DIAGNOSTIC_ASSERT_UNREACHABLE_VOID)
#error QM_DIAGNOSTIC_ASSERT_UNREACHABLE and QM_DIAGNOSTIC_ASSERT_UNREACHABLE_VOID should not be defined.
#  endif
#endif

TEST(QuotaCommon_Try, Success_WithCleanup)
{
  bool tryCleanupRan = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&tryCleanupRan, &tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_OK, QM_PROPAGATE,
           [&tryCleanupRan](const auto&) { tryCleanupRan = true; });

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryCleanupRan);
  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, Failure_PropagateErr)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Try, Failure_CustomErr)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE, NS_ERROR_UNEXPECTED);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_Try, Failure_NoErr)
{
  bool tryDidNotReturn = false;

  [&tryDidNotReturn]() -> void {
    QM_TRY(NS_ERROR_FAILURE, QM_VOID);

    tryDidNotReturn = true;
  }();

  EXPECT_FALSE(tryDidNotReturn);
}

TEST(QuotaCommon_Try, Failure_WithCleanup)
{
  bool tryCleanupRan = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&tryCleanupRan, &tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_ERROR_FAILURE, QM_PROPAGATE,
           [&tryCleanupRan](const auto& result) {
             EXPECT_EQ(result, NS_ERROR_FAILURE);

             tryCleanupRan = true;
           });

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryCleanupRan);
  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Try, Failure_WithCleanup_UnwrapErr)
{
  bool tryCleanupRan = false;
  bool tryDidNotReturn = false;

  nsresult rv;

  [&tryCleanupRan, &tryDidNotReturn](nsresult& aRv) -> void {
    QM_TRY(NS_ERROR_FAILURE, QM_VOID, ([&tryCleanupRan, &aRv](auto& result) {
             EXPECT_EQ(result, NS_ERROR_FAILURE);

             aRv = result;

             tryCleanupRan = true;
           }));

    tryDidNotReturn = true;

    aRv = NS_OK;
  }(rv);

  EXPECT_TRUE(tryCleanupRan);
  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Try, SameLine)
{
  // clang-format off
  QM_TRY(NS_OK, QM_VOID); QM_TRY(NS_OK, QM_VOID);
  // clang-format on
}

TEST(QuotaCommon_Try, NestingMadness_Success)
{
  bool nestedTryDidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTryDidNotReturn, &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTryDidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(NS_OK);

      nestedTryDidNotReturn = true;

      return Ok();
    }()));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryDidNotReturn);
  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, NestingMadness_Failure)
{
  bool nestedTryDidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTryDidNotReturn, &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTryDidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(NS_ERROR_FAILURE);

      nestedTryDidNotReturn = true;

      return Ok();
    }()));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTryDidNotReturn);
  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Try, NestingMadness_Multiple_Success)
{
  bool nestedTry1DidNotReturn = false;
  bool nestedTry2DidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTry1DidNotReturn, &nestedTry2DidNotReturn,
                 &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTry1DidNotReturn,
             &nestedTry2DidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(NS_OK);

      nestedTry1DidNotReturn = true;

      QM_TRY(NS_OK);

      nestedTry2DidNotReturn = true;

      return Ok();
    }()));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTry1DidNotReturn);
  EXPECT_TRUE(nestedTry2DidNotReturn);
  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, NestingMadness_Multiple_Failure1)
{
  bool nestedTry1DidNotReturn = false;
  bool nestedTry2DidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTry1DidNotReturn, &nestedTry2DidNotReturn,
                 &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTry1DidNotReturn,
             &nestedTry2DidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(NS_ERROR_FAILURE);

      nestedTry1DidNotReturn = true;

      QM_TRY(NS_OK);

      nestedTry2DidNotReturn = true;

      return Ok();
    }()));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTry1DidNotReturn);
  EXPECT_FALSE(nestedTry2DidNotReturn);
  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Try, NestingMadness_Multiple_Failure2)
{
  bool nestedTry1DidNotReturn = false;
  bool nestedTry2DidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTry1DidNotReturn, &nestedTry2DidNotReturn,
                 &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTry1DidNotReturn,
             &nestedTry2DidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(NS_OK);

      nestedTry1DidNotReturn = true;

      QM_TRY(NS_ERROR_FAILURE);

      nestedTry2DidNotReturn = true;

      return Ok();
    }()));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTry1DidNotReturn);
  EXPECT_FALSE(nestedTry2DidNotReturn);
  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, Success)
{
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}));
    EXPECT_EQ(x, 42);

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

#ifdef DEBUG
TEST(QuotaCommon_TryInspect, Success_CustomErr_AssertUnreachable)
{
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}),
                   QM_ASSERT_UNREACHABLE);
    EXPECT_EQ(x, 42);

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryInspect, Success_NoErr_AssertUnreachable)
{
  bool tryInspectDidNotReturn = false;

  [&tryInspectDidNotReturn]() -> void {
    QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}),
                   QM_ASSERT_UNREACHABLE_VOID);
    EXPECT_EQ(x, 42);

    tryInspectDidNotReturn = true;
  }();

  EXPECT_TRUE(tryInspectDidNotReturn);
}
#endif

TEST(QuotaCommon_TryInspect, Success_WithCleanup)
{
  bool tryInspectCleanupRan = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectCleanupRan, &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& x, (Result<int32_t, nsresult>{42}), QM_PROPAGATE,
        [&tryInspectCleanupRan](const auto&) { tryInspectCleanupRan = true; });
    EXPECT_EQ(x, 42);

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryInspectCleanupRan);
  EXPECT_TRUE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryInspect, Failure_PropagateErr)
{
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(const auto& x,
                   (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));
    Unused << x;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, Failure_CustomErr)
{
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(const auto& x,
                   (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
                   NS_ERROR_UNEXPECTED);
    Unused << x;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_TryInspect, Failure_NoErr)
{
  bool tryInspectDidNotReturn = false;

  [&tryInspectDidNotReturn]() -> void {
    QM_TRY_INSPECT(const auto& x,
                   (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}), QM_VOID);
    Unused << x;

    tryInspectDidNotReturn = true;
  }();

  EXPECT_FALSE(tryInspectDidNotReturn);
}

TEST(QuotaCommon_TryInspect, Failure_WithCleanup)
{
  bool tryInspectCleanupRan = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&tryInspectCleanupRan, &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(const auto& x,
                   (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
                   QM_PROPAGATE, [&tryInspectCleanupRan](const auto& result) {
                     EXPECT_EQ(result, NS_ERROR_FAILURE);

                     tryInspectCleanupRan = true;
                   });
    Unused << x;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryInspectCleanupRan);
  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, Failure_WithCleanup_UnwrapErr)
{
  bool tryInspectCleanupRan = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv;

  [&tryInspectCleanupRan, &tryInspectDidNotReturn](nsresult& aRv) -> void {
    QM_TRY_INSPECT(const auto& x,
                   (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}), QM_VOID,
                   ([&tryInspectCleanupRan, &aRv](auto& result) {
                     EXPECT_EQ(result, NS_ERROR_FAILURE);

                     aRv = result;

                     tryInspectCleanupRan = true;
                   }));
    Unused << x;

    tryInspectDidNotReturn = true;

    aRv = NS_OK;
  }(rv);

  EXPECT_TRUE(tryInspectCleanupRan);
  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, ConstDecl)
{
  QM_TRY_INSPECT(const int32_t& x, (Result<int32_t, nsresult>{42}), QM_VOID);

  static_assert(std::is_same_v<decltype(x), const int32_t&>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryInspect, SameScopeDecl)
{
  QM_TRY_INSPECT(const int32_t& x, (Result<int32_t, nsresult>{42}), QM_VOID);
  EXPECT_EQ(x, 42);

  QM_TRY_INSPECT(const int32_t& y, (Result<int32_t, nsresult>{42}), QM_VOID);
  EXPECT_EQ(y, 42);
}

TEST(QuotaCommon_TryInspect, SameLine)
{
  // clang-format off
  QM_TRY_INSPECT(const auto &x, (Result<int32_t, nsresult>{42}), QM_VOID); QM_TRY_INSPECT(const auto &y, (Result<int32_t, nsresult>{42}), QM_VOID);
  // clang-format on

  EXPECT_EQ(x, 42);
  EXPECT_EQ(y, 42);
}

TEST(QuotaCommon_TryInspect, NestingMadness_Success)
{
  bool nestedTryInspectDidNotReturn = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&nestedTryInspectDidNotReturn,
                 &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& x,
        ([&nestedTryInspectDidNotReturn]() -> Result<int32_t, nsresult> {
          QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}));

          nestedTryInspectDidNotReturn = true;

          return x;
        }()));
    EXPECT_EQ(x, 42);

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryInspectDidNotReturn);
  EXPECT_TRUE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryInspect, NestingMadness_Failure)
{
  bool nestedTryInspectDidNotReturn = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&nestedTryInspectDidNotReturn,
                 &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& x,
        ([&nestedTryInspectDidNotReturn]() -> Result<int32_t, nsresult> {
          QM_TRY_INSPECT(const auto& x,
                         (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

          nestedTryInspectDidNotReturn = true;

          return x;
        }()));
    Unused << x;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTryInspectDidNotReturn);
  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, NestingMadness_Multiple_Success)
{
  bool nestedTryInspect1DidNotReturn = false;
  bool nestedTryInspect2DidNotReturn = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&nestedTryInspect1DidNotReturn, &nestedTryInspect2DidNotReturn,
                 &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& z,
        ([&nestedTryInspect1DidNotReturn,
          &nestedTryInspect2DidNotReturn]() -> Result<int32_t, nsresult> {
          QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}));

          nestedTryInspect1DidNotReturn = true;

          QM_TRY_INSPECT(const auto& y, (Result<int32_t, nsresult>{42}));

          nestedTryInspect2DidNotReturn = true;

          return x + y;
        }()));
    EXPECT_EQ(z, 84);

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryInspect1DidNotReturn);
  EXPECT_TRUE(nestedTryInspect2DidNotReturn);
  EXPECT_TRUE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryInspect, NestingMadness_Multiple_Failure1)
{
  bool nestedTryInspect1DidNotReturn = false;
  bool nestedTryInspect2DidNotReturn = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&nestedTryInspect1DidNotReturn, &nestedTryInspect2DidNotReturn,
                 &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& z,
        ([&nestedTryInspect1DidNotReturn,
          &nestedTryInspect2DidNotReturn]() -> Result<int32_t, nsresult> {
          QM_TRY_INSPECT(const auto& x,
                         (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

          nestedTryInspect1DidNotReturn = true;

          QM_TRY_INSPECT(const auto& y, (Result<int32_t, nsresult>{42}));

          nestedTryInspect2DidNotReturn = true;

          return x + y;
        }()));
    Unused << z;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTryInspect1DidNotReturn);
  EXPECT_FALSE(nestedTryInspect2DidNotReturn);
  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryInspect, NestingMadness_Multiple_Failure2)
{
  bool nestedTryInspect1DidNotReturn = false;
  bool nestedTryInspect2DidNotReturn = false;
  bool tryInspectDidNotReturn = false;

  nsresult rv = [&nestedTryInspect1DidNotReturn, &nestedTryInspect2DidNotReturn,
                 &tryInspectDidNotReturn]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& z,
        ([&nestedTryInspect1DidNotReturn,
          &nestedTryInspect2DidNotReturn]() -> Result<int32_t, nsresult> {
          QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}));

          nestedTryInspect1DidNotReturn = true;

          QM_TRY_INSPECT(const auto& y,
                         (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

          nestedTryInspect2DidNotReturn = true;

          return x + y;
        }()));
    Unused << z;

    tryInspectDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryInspect1DidNotReturn);
  EXPECT_FALSE(nestedTryInspect2DidNotReturn);
  EXPECT_FALSE(tryInspectDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

// We are not repeating all QM_TRY_INSPECT test cases for QM_TRY_UNWRAP, since
// they are largely based on the same implementation. We just add some where
// inspecting and unwrapping differ.

TEST(QuotaCommon_TryUnwrap, NonConstDecl)
{
  QM_TRY_UNWRAP(int32_t x, (Result<int32_t, nsresult>{42}), QM_VOID);

  static_assert(std::is_same_v<decltype(x), int32_t>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryUnwrap, RvalueDecl)
{
  QM_TRY_UNWRAP(int32_t && x, (Result<int32_t, nsresult>{42}), QM_VOID);

  static_assert(std::is_same_v<decltype(x), int32_t&&>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryUnwrap, ParenDecl)
{
  QM_TRY_UNWRAP(
      (auto&& [x, y]),
      (Result<std::pair<int32_t, bool>, nsresult>{std::pair{42, true}}),
      QM_VOID);

  static_assert(std::is_same_v<decltype(x), int32_t>);
  static_assert(std::is_same_v<decltype(y), bool>);

  EXPECT_EQ(x, 42);
  EXPECT_EQ(y, true);
}

TEST(QuotaCommon_TryReturn, Success)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn] {
    QM_TRY_RETURN((Result<int32_t, nsresult>{42}));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_TryReturn, Success_nsresult)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn] {
    QM_TRY_RETURN(NS_OK);

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
}

#ifdef DEBUG
TEST(QuotaCommon_TryReturn, Success_CustomErr_AssertUnreachable)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN((Result<int32_t, nsresult>{42}), QM_ASSERT_UNREACHABLE);

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}
#endif

TEST(QuotaCommon_TryReturn, Success_WithCleanup)
{
  bool tryReturnCleanupRan = false;
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnCleanupRan,
              &tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN(
        (Result<int32_t, nsresult>{42}), QM_PROPAGATE,
        [&tryReturnCleanupRan](const auto&) { tryReturnCleanupRan = true; });

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnCleanupRan);
  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_TryReturn, Failure_PropagateErr)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn] {
    QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryReturn, Failure_PropagateErr_nsresult)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn] {
    QM_TRY_RETURN(NS_ERROR_FAILURE);

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryReturn, Failure_CustomErr)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
                  Err(NS_ERROR_UNEXPECTED));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_TryReturn, Failure_WithCleanup)
{
  bool tryReturnCleanupRan = false;
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnCleanupRan,
              &tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
                  QM_PROPAGATE, [&tryReturnCleanupRan](const auto& result) {
                    EXPECT_EQ(result, NS_ERROR_FAILURE);

                    tryReturnCleanupRan = true;
                  });

    tryReturnDidNotReturn = true;
  }();

  EXPECT_TRUE(tryReturnCleanupRan);
  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryReturn, SameLine)
{
  // clang-format off
  auto res1 = [] { QM_TRY_RETURN((Result<int32_t, nsresult>{42})); }(); auto res2 = []() -> Result<int32_t, nsresult> { QM_TRY_RETURN((Result<int32_t, nsresult>{42})); }();
  // clang-format on

  EXPECT_TRUE(res1.isOk());
  EXPECT_EQ(res1.unwrap(), 42);
  EXPECT_TRUE(res2.isOk());
  EXPECT_EQ(res2.unwrap(), 42);
}

TEST(QuotaCommon_TryReturn, NestingMadness_Success)
{
  bool nestedTryReturnDidNotReturn = false;
  bool tryReturnDidNotReturn = false;

  auto res = [&nestedTryReturnDidNotReturn, &tryReturnDidNotReturn] {
    QM_TRY_RETURN(([&nestedTryReturnDidNotReturn] {
      QM_TRY_RETURN((Result<int32_t, nsresult>{42}));

      nestedTryReturnDidNotReturn = true;
    }()));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(nestedTryReturnDidNotReturn);
  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_TryReturn, NestingMadness_Failure)
{
  bool nestedTryReturnDidNotReturn = false;
  bool tryReturnDidNotReturn = false;

  auto res = [&nestedTryReturnDidNotReturn, &tryReturnDidNotReturn] {
    QM_TRY_RETURN(([&nestedTryReturnDidNotReturn] {
      QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

      nestedTryReturnDidNotReturn = true;
    }()));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(nestedTryReturnDidNotReturn);
  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Fail, ReturnValue)
{
  bool failDidNotReturn = false;

  nsresult rv = [&failDidNotReturn]() -> nsresult {
    QM_FAIL(NS_ERROR_FAILURE);

    failDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(failDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_Fail, ReturnValue_WithCleanup)
{
  bool failCleanupRan = false;
  bool failDidNotReturn = false;

  nsresult rv = [&failCleanupRan, &failDidNotReturn]() -> nsresult {
    QM_FAIL(NS_ERROR_FAILURE, [&failCleanupRan]() { failCleanupRan = true; });

    failDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(failCleanupRan);
  EXPECT_FALSE(failDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
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

TEST(QuotaCommon_OkToOk, Bool_True)
{
  auto res = OkToOk<true>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), true);
}

TEST(QuotaCommon_OkToOk, Bool_False)
{
  auto res = OkToOk<false>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), false);
}

TEST(QuotaCommon_OkToOk, Int_42)
{
  auto res = OkToOk<42>(Ok());
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ErrToOkOrErr, Bool_True)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, true>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), true);
}

TEST(QuotaCommon_ErrToOkOrErr, Bool_True_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, true>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, Bool_False)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, false>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), false);
}

TEST(QuotaCommon_ErrToOkOrErr, Bool_False_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, false>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, Int_42)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, 42>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ErrToOkOrErr, Int_42_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, 42>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToOkOrErr, NsCOMPtr_nullptr)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, nullptr, nsCOMPtr<nsISupports>>(
      NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), nullptr);
}

TEST(QuotaCommon_ErrToOkOrErr, NsCOMPtr_nullptr_Err)
{
  auto res = ErrToOkOrErr<NS_ERROR_FAILURE, nullptr, nsCOMPtr<nsISupports>>(
      NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToDefaultOkOrErr, Ok)
{
  auto res = ErrToDefaultOkOrErr<NS_ERROR_FAILURE, Ok>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
}

TEST(QuotaCommon_ErrToDefaultOkOrErr, Ok_Err)
{
  auto res = ErrToDefaultOkOrErr<NS_ERROR_FAILURE, Ok>(NS_ERROR_UNEXPECTED);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_ErrToDefaultOkOrErr, NsCOMPtr)
{
  auto res = ErrToDefaultOkOrErr<NS_ERROR_FAILURE, nsCOMPtr<nsISupports>>(
      NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), nullptr);
}

TEST(QuotaCommon_ErrToDefaultOkOrErr, NsCOMPtr_Err)
{
  auto res = ErrToDefaultOkOrErr<NS_ERROR_FAILURE, nsCOMPtr<nsISupports>>(
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
  auto res = ToResultGet<int32_t>([](nsresult* aRv) -> int32_t {
    *aRv = NS_OK;
    return 42;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ToResultGet, Lambda_NoInput_Err)
{
  auto res = ToResultGet<int32_t>([](nsresult* aRv) -> int32_t {
    *aRv = NS_ERROR_FAILURE;
    return -1;
  });

  static_assert(std::is_same_v<decltype(res), Result<int32_t, nsresult>>);

  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
}

TEST(QuotaCommon_ToResultGet, Lambda_WithInput)
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

TEST(QuotaCommon_ToResultGet, Lambda_WithInput_Err)
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

// BEGIN COPY FROM mfbt/tests/TestResult.cpp
struct Failed {};

static GenericErrorResult<Failed> Fail() { return Err(Failed()); }

static Result<Ok, Failed> Task1(bool pass) {
  if (!pass) {
    return Fail();  // implicit conversion from GenericErrorResult to Result
  }
  return Ok();
}
// END COPY FROM mfbt/tests/TestResult.cpp

static Result<bool, Failed> Condition(bool aNoError, bool aResult) {
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
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed>>);
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
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed>>);
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
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed>>);
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
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed>>);
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
  static_assert(std::is_same_v<decltype(result), Result<Ok, Failed>>);
  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(1 == bodyExecutions);
  MOZ_RELEASE_ASSERT(2 == conditionExecutions);
}

TEST(QuotaCommon_CollectEachInRange, Success)
{
  size_t bodyExecutions = 0;
  const auto result = CollectEachInRange(
      std::array<int, 5>{{1, 2, 3, 4, 5}},
      [&bodyExecutions](const int val) -> Result<Ok, nsresult> {
        ++bodyExecutions;
        return Ok{};
      });

  MOZ_RELEASE_ASSERT(result.isOk());
  MOZ_RELEASE_ASSERT(5 == bodyExecutions);
}

TEST(QuotaCommon_CollectEachInRange, FailureShortCircuit)
{
  size_t bodyExecutions = 0;
  const auto result = CollectEachInRange(
      std::array<int, 5>{{1, 2, 3, 4, 5}},
      [&bodyExecutions](const int val) -> Result<Ok, nsresult> {
        ++bodyExecutions;
        return val == 3 ? Err(NS_ERROR_FAILURE) : Result<Ok, nsresult>{Ok{}};
      });

  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(NS_ERROR_FAILURE == result.inspectErr());
  MOZ_RELEASE_ASSERT(3 == bodyExecutions);
}

TEST(QuotaCommon_ReduceEach, Success)
{
  const auto result = ReduceEach(
      [i = int{0}]() mutable -> Result<int, Failed> {
        if (i < 5) {
          return ++i;
        }
        return 0;
      },
      0, [](int val, int add) -> Result<int, Failed> { return val + add; });
  static_assert(std::is_same_v<decltype(result), const Result<int, Failed>>);

  MOZ_RELEASE_ASSERT(result.isOk());
  MOZ_RELEASE_ASSERT(15 == result.inspect());
}

TEST(QuotaCommon_ReduceEach, StepError)
{
  const auto result = ReduceEach(
      [i = int{0}]() mutable -> Result<int, Failed> {
        if (i < 5) {
          return ++i;
        }
        return 0;
      },
      0,
      [](int val, int add) -> Result<int, Failed> {
        if (val > 2) {
          return Err(Failed{});
        }
        return val + add;
      });
  static_assert(std::is_same_v<decltype(result), const Result<int, Failed>>);

  MOZ_RELEASE_ASSERT(result.isErr());
}

TEST(QuotaCommon_ReduceEach, GeneratorError)
{
  size_t generatorExecutions = 0;
  const auto result = ReduceEach(
      [i = int{0}, &generatorExecutions]() mutable -> Result<int, Failed> {
        ++generatorExecutions;
        if (i < 1) {
          return ++i;
        }
        return Err(Failed{});
      },
      0,
      [](int val, int add) -> Result<int, Failed> {
        if (val > 2) {
          return Err(Failed{});
        }
        return val + add;
      });
  static_assert(std::is_same_v<decltype(result), const Result<int, Failed>>);

  MOZ_RELEASE_ASSERT(result.isErr());
  MOZ_RELEASE_ASSERT(2 == generatorExecutions);
}

TEST(QuotaCommon_Reduce, Success)
{
  const auto range = std::vector{0, 1, 2, 3, 4, 5};
  const auto result = Reduce(
      range, 0, [](int val, Maybe<const int&> add) -> Result<int, Failed> {
        return val + add.ref();
      });
  static_assert(std::is_same_v<decltype(result), const Result<int, Failed>>);

  MOZ_RELEASE_ASSERT(result.isOk());
  MOZ_RELEASE_ASSERT(15 == result.inspect());
}

TEST(QuotaCommon_ScopedLogExtraInfo, AddAndRemove)
{
  static constexpr auto text = "foo"_ns;

  {
    const auto extraInfo =
        ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQuery, text};

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
    const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

    EXPECT_EQ(text, *extraInfoMap.at(ScopedLogExtraInfo::kTagQuery));
#endif
  }

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
  const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

  EXPECT_EQ(0u, extraInfoMap.count(ScopedLogExtraInfo::kTagQuery));
#endif
}

TEST(QuotaCommon_ScopedLogExtraInfo, Nested)
{
  static constexpr auto text = "foo"_ns;
  static constexpr auto nestedText = "bar"_ns;

  {
    const auto extraInfo =
        ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQuery, text};

    {
      const auto extraInfo =
          ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQuery, nestedText};

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
      const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();
      EXPECT_EQ(nestedText, *extraInfoMap.at(ScopedLogExtraInfo::kTagQuery));
#endif
    }

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
    const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();
    EXPECT_EQ(text, *extraInfoMap.at(ScopedLogExtraInfo::kTagQuery));
#endif
  }

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
  const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

  EXPECT_EQ(0u, extraInfoMap.count(ScopedLogExtraInfo::kTagQuery));
#endif
}

TEST(QuotaCommon_CallWithDelayedRetriesIfAccessDenied, NoFailures)
{
  uint32_t tries = 0;

  auto res = CallWithDelayedRetriesIfAccessDenied(
      [&tries]() -> Result<Ok, nsresult> {
        ++tries;
        return Ok{};
      },
      10, 2);

  EXPECT_EQ(tries, 1u);
  EXPECT_TRUE(res.isOk());
}

TEST(QuotaCommon_CallWithDelayedRetriesIfAccessDenied, PermanentFailures)
{
  uint32_t tries = 0;

  auto res = CallWithDelayedRetriesIfAccessDenied(
      [&tries]() -> Result<Ok, nsresult> {
        ++tries;
        return Err(NS_ERROR_FILE_IS_LOCKED);
      },
      10, 2);

  EXPECT_EQ(tries, 11u);
  EXPECT_TRUE(res.isErr());
}

TEST(QuotaCommon_CallWithDelayedRetriesIfAccessDenied, FailuresAndSuccess)
{
  uint32_t tries = 0;

  auto res = CallWithDelayedRetriesIfAccessDenied(
      [&tries]() -> Result<Ok, nsresult> {
        if (++tries == 5) {
          return Ok{};
        }
        return Err(NS_ERROR_FILE_ACCESS_DENIED);
      },
      10, 2);

  EXPECT_EQ(tries, 5u);
  EXPECT_TRUE(res.isOk());
}

static constexpr auto thisFileRelativeSourceFileName =
    "dom/quota/test/gtest/TestQuotaCommon.cpp"_ns;

TEST(QuotaCommon_MakeRelativeSourceFileName, ThisDomQuotaFile)
{
  const nsCString relativeSourceFilePath{
      mozilla::dom::quota::detail::MakeRelativeSourceFileName(
          nsLiteralCString(__FILE__))};

  EXPECT_STREQ(relativeSourceFilePath.get(),
               thisFileRelativeSourceFileName.get());
}

static nsCString MakeFullPath(const nsACString& aRelativePath) {
  nsCString path{mozilla::dom::quota::detail::GetSourceTreeBase()};

  path.Append("/");
  path.Append(aRelativePath);

  return path;
}

TEST(QuotaCommon_MakeRelativeSourceFileName, DomIndexedDBFile)
{
  static constexpr auto domIndexedDBFileRelativePath =
      "dom/indexedDB/ActorsParent.cpp"_ns;

  const nsCString relativeSourceFilePath{
      mozilla::dom::quota::detail::MakeRelativeSourceFileName(
          MakeFullPath(domIndexedDBFileRelativePath))};

  EXPECT_STREQ(relativeSourceFilePath.get(),
               domIndexedDBFileRelativePath.get());
}

TEST(QuotaCommon_MakeRelativeSourceFileName, NonDomFile)
{
  static constexpr auto nonDomFileRelativePath =
      "storage/mozStorageService.cpp"_ns;

  const nsCString relativeSourceFilePath{
      mozilla::dom::quota::detail::MakeRelativeSourceFileName(
          MakeFullPath(nonDomFileRelativePath))};

  EXPECT_STREQ(relativeSourceFilePath.get(), nonDomFileRelativePath.get());
}

TEST(QuotaCommon_MakeRelativeSourceFileName, OtherFile)
{
  constexpr auto otherName = "/foo/bar/Test.cpp"_ns;
  const nsCString relativeSourceFilePath{
      mozilla::dom::quota::detail::MakeRelativeSourceFileName(otherName)};

  EXPECT_STREQ(relativeSourceFilePath.get(), "Test.cpp");
}

#ifdef __clang__
#  pragma clang diagnostic pop
#endif
