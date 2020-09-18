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
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(NS_OK);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

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
             EXPECT_TRUE(result.isErr());
             EXPECT_EQ(result.inspectErr(), NS_ERROR_FAILURE);

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
             EXPECT_TRUE(result.isErr());
             EXPECT_EQ(result.inspectErr(), NS_ERROR_FAILURE);

             aRv = result.unwrapErr();

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

TEST(QuotaCommon_DebugTry, Success)
{
  bool debugTryBodyRan = false;
  bool debugTryDidNotReturn = false;

  nsresult rv = [
#ifdef DEBUG
                    &debugTryBodyRan, &debugTryDidNotReturn
#else
                    &debugTryDidNotReturn
#endif
  ]() -> nsresult {
    QM_DEBUG_TRY(([&debugTryBodyRan]() -> Result<Ok, nsresult> {
      debugTryBodyRan = true;

      return Ok();
    }()));

    debugTryDidNotReturn = true;

    return NS_OK;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryBodyRan);
#else
  EXPECT_FALSE(debugTryBodyRan);
#endif
  EXPECT_TRUE(debugTryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_DebugTry, Failure)
{
  bool debugTryBodyRan = false;
  bool debugTryDidNotReturn = false;

  nsresult rv = [
#ifdef DEBUG
                    &debugTryBodyRan, &debugTryDidNotReturn
#else
                    &debugTryDidNotReturn
#endif
  ]() -> nsresult {
    QM_DEBUG_TRY(([&debugTryBodyRan]() -> Result<Ok, nsresult> {
      debugTryBodyRan = true;

      return Err(NS_ERROR_FAILURE);
    }()));

    debugTryDidNotReturn = true;

    return NS_OK;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryBodyRan);
  EXPECT_FALSE(debugTryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
#else
  EXPECT_FALSE(debugTryBodyRan);
  EXPECT_TRUE(debugTryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
#endif
}

TEST(QuotaCommon_TryVar, Success)
{
  bool tryVarDidNotReturn = false;

  nsresult rv = [&tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}));
    EXPECT_EQ(x, 42);

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryVar, Success_WithCleanup)
{
  bool tryVarCleanupRan = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&tryVarCleanupRan, &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}), QM_PROPAGATE,
               [&tryVarCleanupRan](const auto&) { tryVarCleanupRan = true; });
    EXPECT_EQ(x, 42);

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryVarCleanupRan);
  EXPECT_TRUE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryVar, Failure_PropagateErr)
{
  bool tryVarDidNotReturn = false;

  nsresult rv = [&tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x,
               (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));
    Unused << x;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryVar, Failure_CustomErr)
{
  bool tryVarDidNotReturn = false;

  nsresult rv = [&tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
               NS_ERROR_UNEXPECTED);
    Unused << x;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_TryVar, Failure_NoErr)
{
  bool tryVarDidNotReturn = false;

  [&tryVarDidNotReturn]() -> void {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
               QM_VOID);
    Unused << x;

    tryVarDidNotReturn = true;
  }();

  EXPECT_FALSE(tryVarDidNotReturn);
}

TEST(QuotaCommon_TryVar, Failure_WithCleanup)
{
  bool tryVarCleanupRan = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&tryVarCleanupRan, &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
               QM_PROPAGATE, [&tryVarCleanupRan](const auto& result) {
                 EXPECT_TRUE(result.isErr());
                 EXPECT_EQ(result.inspectErr(), NS_ERROR_FAILURE);

                 tryVarCleanupRan = true;
               });
    Unused << x;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryVarCleanupRan);
  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryVar, Failure_WithCleanup_UnwrapErr)
{
  bool tryVarCleanupRan = false;
  bool tryVarDidNotReturn = false;

  nsresult rv;

  [&tryVarCleanupRan, &tryVarDidNotReturn](nsresult& aRv) -> void {
    QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),
               QM_VOID, ([&tryVarCleanupRan, &aRv](auto& result) {
                 EXPECT_TRUE(result.isErr());
                 EXPECT_EQ(result.inspectErr(), NS_ERROR_FAILURE);

                 aRv = result.unwrapErr();

                 tryVarCleanupRan = true;
               }));
    Unused << x;

    tryVarDidNotReturn = true;

    aRv = NS_OK;
  }(rv);

  EXPECT_TRUE(tryVarCleanupRan);
  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryVar, Decl)
{
  QM_TRY_VAR(int32_t x, (Result<int32_t, nsresult>{42}), QM_VOID);

  static_assert(std::is_same_v<decltype(x), int32_t>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryVar, ConstDecl)
{
  QM_TRY_VAR(const int32_t x, (Result<int32_t, nsresult>{42}), QM_VOID);

  static_assert(std::is_same_v<decltype(x), const int32_t>);

  EXPECT_EQ(x, 42);
}

TEST(QuotaCommon_TryVar, SameScopeDecl)
{
  QM_TRY_VAR(const int32_t x, (Result<int32_t, nsresult>{42}), QM_VOID);
  EXPECT_EQ(x, 42);

  QM_TRY_VAR(const int32_t y, (Result<int32_t, nsresult>{42}), QM_VOID);
  EXPECT_EQ(y, 42);
}

TEST(QuotaCommon_TryVar, SameLine)
{
  // clang-format off
  QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}), QM_VOID); QM_TRY_VAR(const auto y, (Result<int32_t, nsresult>{42}), QM_VOID);
  // clang-format on

  EXPECT_EQ(x, 42);
  EXPECT_EQ(y, 42);
}

TEST(QuotaCommon_TryVar, ParenDecl)
{
  QM_TRY_VAR((const auto [x, y]),
             (Result<std::pair<int32_t, bool>, nsresult>{std::pair{42, true}}),
             QM_VOID);

  static_assert(std::is_same_v<decltype(x), const int32_t>);
  static_assert(std::is_same_v<decltype(y), const bool>);

  EXPECT_EQ(x, 42);
  EXPECT_EQ(y, true);
}

TEST(QuotaCommon_TryVar, NestingMadness_Success)
{
  bool nestedTryVarDidNotReturn = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&nestedTryVarDidNotReturn, &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x,
               ([&nestedTryVarDidNotReturn]() -> Result<int32_t, nsresult> {
                 QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}));

                 nestedTryVarDidNotReturn = true;

                 return x;
               }()));
    EXPECT_EQ(x, 42);

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryVarDidNotReturn);
  EXPECT_TRUE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryVar, NestingMadness_Failure)
{
  bool nestedTryVarDidNotReturn = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&nestedTryVarDidNotReturn, &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto x,
               ([&nestedTryVarDidNotReturn]() -> Result<int32_t, nsresult> {
                 QM_TRY_VAR(const auto x,
                            (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

                 nestedTryVarDidNotReturn = true;

                 return x;
               }()));
    Unused << x;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTryVarDidNotReturn);
  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryVar, NestingMadness_Multiple_Success)
{
  bool nestedTryVar1DidNotReturn = false;
  bool nestedTryVar2DidNotReturn = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&nestedTryVar1DidNotReturn, &nestedTryVar2DidNotReturn,
                 &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto z,
               ([&nestedTryVar1DidNotReturn,
                 &nestedTryVar2DidNotReturn]() -> Result<int32_t, nsresult> {
                 QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}));

                 nestedTryVar1DidNotReturn = true;

                 QM_TRY_VAR(const auto y, (Result<int32_t, nsresult>{42}));

                 nestedTryVar2DidNotReturn = true;

                 return x + y;
               }()));
    EXPECT_EQ(z, 84);

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryVar1DidNotReturn);
  EXPECT_TRUE(nestedTryVar2DidNotReturn);
  EXPECT_TRUE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_TryVar, NestingMadness_Multiple_Failure1)
{
  bool nestedTryVar1DidNotReturn = false;
  bool nestedTryVar2DidNotReturn = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&nestedTryVar1DidNotReturn, &nestedTryVar2DidNotReturn,
                 &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto z,
               ([&nestedTryVar1DidNotReturn,
                 &nestedTryVar2DidNotReturn]() -> Result<int32_t, nsresult> {
                 QM_TRY_VAR(const auto x,
                            (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

                 nestedTryVar1DidNotReturn = true;

                 QM_TRY_VAR(const auto y, (Result<int32_t, nsresult>{42}));

                 nestedTryVar2DidNotReturn = true;

                 return x + y;
               }()));
    Unused << z;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(nestedTryVar1DidNotReturn);
  EXPECT_FALSE(nestedTryVar2DidNotReturn);
  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_TryVar, NestingMadness_Multiple_Failure2)
{
  bool nestedTryVar1DidNotReturn = false;
  bool nestedTryVar2DidNotReturn = false;
  bool tryVarDidNotReturn = false;

  nsresult rv = [&nestedTryVar1DidNotReturn, &nestedTryVar2DidNotReturn,
                 &tryVarDidNotReturn]() -> nsresult {
    QM_TRY_VAR(const auto z,
               ([&nestedTryVar1DidNotReturn,
                 &nestedTryVar2DidNotReturn]() -> Result<int32_t, nsresult> {
                 QM_TRY_VAR(const auto x, (Result<int32_t, nsresult>{42}));

                 nestedTryVar1DidNotReturn = true;

                 QM_TRY_VAR(const auto y,
                            (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

                 nestedTryVar2DidNotReturn = true;

                 return x + y;
               }()));
    Unused << z;

    tryVarDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(nestedTryVar1DidNotReturn);
  EXPECT_FALSE(nestedTryVar2DidNotReturn);
  EXPECT_FALSE(tryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
}

TEST(QuotaCommon_DebugTryVar, Success)
{
  bool debugTryVarBodyRan = false;
  bool debugTryVarDidNotReturn = false;

  nsresult rv = [
#ifdef DEBUG
                    &debugTryVarBodyRan, &debugTryVarDidNotReturn
#else
                    &debugTryVarDidNotReturn
#endif
  ]() -> nsresult {
    QM_DEBUG_TRY_VAR(const auto x,
                     ([&debugTryVarBodyRan]() -> Result<int32_t, nsresult> {
                       debugTryVarBodyRan = true;

                       return 42;
                     }()));
#ifdef DEBUG
    EXPECT_EQ(x, 42);
#endif

    debugTryVarDidNotReturn = true;

    return NS_OK;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryVarBodyRan);
#else
  EXPECT_FALSE(debugTryVarBodyRan);
#endif
  EXPECT_TRUE(debugTryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_DebugTryVar, Failure)
{
  bool debugTryVarBodyRan = false;
  bool debugTryVarDidNotReturn = false;

  nsresult rv = [
#ifdef DEBUG
                    &debugTryVarBodyRan, &debugTryVarDidNotReturn
#else
                    &debugTryVarDidNotReturn
#endif
  ]() -> nsresult {
    QM_DEBUG_TRY_VAR(const auto x,
                     ([&debugTryVarBodyRan]() -> Result<int32_t, nsresult> {
                       debugTryVarBodyRan = true;

                       return Err(NS_ERROR_FAILURE);
                     }()));
#ifdef DEBUG
    Unused << x;
#endif

    debugTryVarDidNotReturn = true;

    return NS_OK;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryVarBodyRan);
  EXPECT_FALSE(debugTryVarDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
#else
  EXPECT_FALSE(debugTryVarBodyRan);
  EXPECT_TRUE(debugTryVarDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
#endif
}

TEST(QuotaCommon_TryReturn, Success)
{
  bool tryReturnDidNotReturn = false;

  auto res = [&tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN((Result<int32_t, nsresult>{42}));

    tryReturnDidNotReturn = true;
  }();

  EXPECT_FALSE(tryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

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

  auto res = [&tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}));

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
                    EXPECT_TRUE(result.isErr());
                    EXPECT_EQ(result.inspectErr(), NS_ERROR_FAILURE);

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
  auto res1 = []() -> Result<int32_t, nsresult> { QM_TRY_RETURN((Result<int32_t, nsresult>{42})); }(); auto res2 = []() -> Result<int32_t, nsresult> { QM_TRY_RETURN((Result<int32_t, nsresult>{42})); }();
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

  auto res = [&nestedTryReturnDidNotReturn,
              &tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN(
        ([&nestedTryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
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

  auto res = [&nestedTryReturnDidNotReturn,
              &tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
    QM_TRY_RETURN(
        ([&nestedTryReturnDidNotReturn]() -> Result<int32_t, nsresult> {
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

TEST(QuotaCommon_DebugTryReturn, Success)
{
  bool debugTryReturnBodyRan = false;
  bool debugTryReturnDidNotReturn = false;

  auto res = [
#ifdef DEBUG
                 &debugTryReturnBodyRan, &debugTryReturnDidNotReturn
#else
                 &debugTryReturnDidNotReturn
#endif
  ]() -> Result<int32_t, nsresult> {
    QM_DEBUG_TRY_RETURN(
        ([&debugTryReturnBodyRan]() -> Result<int32_t, nsresult> {
          debugTryReturnBodyRan = true;

          return 42;
        }()));

    debugTryReturnDidNotReturn = true;

    return 42;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryReturnBodyRan);
  EXPECT_FALSE(debugTryReturnDidNotReturn);
#else
  EXPECT_FALSE(debugTryReturnBodyRan);
  EXPECT_TRUE(debugTryReturnDidNotReturn);
#endif
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_DebugTryReturn, Failure)
{
  bool debugTryReturnBodyRan = false;
  bool debugTryReturnDidNotReturn = false;

  auto res = [
#ifdef DEBUG
                 &debugTryReturnBodyRan, &debugTryReturnDidNotReturn
#else
                 &debugTryReturnDidNotReturn
#endif
  ]() -> Result<int32_t, nsresult> {
    QM_DEBUG_TRY_RETURN(
        ([&debugTryReturnBodyRan]() -> Result<int32_t, nsresult> {
          debugTryReturnBodyRan = true;

          return Err(NS_ERROR_FAILURE);
        }()));

    debugTryReturnDidNotReturn = true;

    return 42;
  }();

#ifdef DEBUG
  EXPECT_TRUE(debugTryReturnBodyRan);
  EXPECT_FALSE(debugTryReturnDidNotReturn);
  EXPECT_TRUE(res.isErr());
  EXPECT_EQ(res.unwrapErr(), NS_ERROR_FAILURE);
#else
  EXPECT_FALSE(debugTryReturnBodyRan);
  EXPECT_TRUE(debugTryReturnDidNotReturn);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
#endif
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

TEST(QuotaCommon_DebugFail, ReturnValue)
{
  bool debugFailDidNotReturn = false;

  nsresult rv = [&debugFailDidNotReturn]() -> nsresult {
    QM_DEBUG_FAIL(NS_ERROR_FAILURE);

    debugFailDidNotReturn = true;

    return NS_OK;
  }();

#ifdef DEBUG
  EXPECT_FALSE(debugFailDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_FAILURE);
#else
  EXPECT_TRUE(debugFailDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
#endif
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
