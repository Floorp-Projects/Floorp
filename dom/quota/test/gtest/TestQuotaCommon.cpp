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
#include "mozilla/dom/quota/QuotaTestParent.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTLiteralString.h"

class nsISupports;

namespace mozilla::dom::quota {

namespace {

void CheckUnknownFileEntry(nsIFile& aBase, const nsAString& aName,
                           const bool aWarnIfFile, const bool aWarnIfDir) {
  nsCOMPtr<nsIFile> file;
  nsresult rv = aBase.Clone(getter_AddRefs(file));
  ASSERT_EQ(rv, NS_OK);

  rv = file->Append(aName);
  ASSERT_EQ(rv, NS_OK);

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  ASSERT_EQ(rv, NS_OK);

  auto okOrErr = WARN_IF_FILE_IS_UNKNOWN(*file);
  ASSERT_TRUE(okOrErr.isOk());

#ifdef DEBUG
  EXPECT_TRUE(okOrErr.inspect() == aWarnIfFile);
#else
  EXPECT_TRUE(okOrErr.inspect() == false);
#endif

  rv = file->Remove(false);
  ASSERT_EQ(rv, NS_OK);

  rv = file->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(rv, NS_OK);

  okOrErr = WARN_IF_FILE_IS_UNKNOWN(*file);
  ASSERT_TRUE(okOrErr.isOk());

#ifdef DEBUG
  EXPECT_TRUE(okOrErr.inspect() == aWarnIfDir);
#else
  EXPECT_TRUE(okOrErr.inspect() == false);
#endif

  rv = file->Remove(false);
  ASSERT_EQ(rv, NS_OK);
}

}  // namespace

TEST(QuotaCommon_WarnIfFileIsUnknown, Basics)
{
  nsCOMPtr<nsIFile> base;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  ASSERT_EQ(rv, NS_OK);

  rv = base->Append(u"mozquotatests"_ns);
  ASSERT_EQ(rv, NS_OK);

  base->Remove(true);

  rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(rv, NS_OK);

  CheckUnknownFileEntry(*base, u"foo.bar"_ns, true, true);
  CheckUnknownFileEntry(*base, u".DS_Store"_ns, false, true);
  CheckUnknownFileEntry(*base, u".desktop"_ns, false, true);
  CheckUnknownFileEntry(*base, u"desktop.ini"_ns, false, true);
  CheckUnknownFileEntry(*base, u"DESKTOP.INI"_ns, false, true);
  CheckUnknownFileEntry(*base, u"thumbs.db"_ns, false, true);
  CheckUnknownFileEntry(*base, u"THUMBS.DB"_ns, false, true);
  CheckUnknownFileEntry(*base, u".xyz"_ns, false, true);

  rv = base->Remove(true);
  ASSERT_EQ(rv, NS_OK);
}

mozilla::ipc::IPCResult QuotaTestParent::RecvTry_Success_CustomErr_QmIpcFail(
    bool* aTryDidNotReturn) {
  QM_TRY(MOZ_TO_RESULT(NS_OK), QM_IPC_FAIL(this));

  *aTryDidNotReturn = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult QuotaTestParent::RecvTry_Success_CustomErr_IpcFail(
    bool* aTryDidNotReturn) {
  QM_TRY(MOZ_TO_RESULT(NS_OK), IPC_FAIL(this, "Custom why"));

  *aTryDidNotReturn = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult
QuotaTestParent::RecvTryInspect_Success_CustomErr_QmIpcFail(
    bool* aTryDidNotReturn) {
  QM_TRY_INSPECT(const auto& x, (mozilla::Result<int32_t, nsresult>{42}),
                 QM_IPC_FAIL(this));
  Unused << x;

  *aTryDidNotReturn = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult
QuotaTestParent::RecvTryInspect_Success_CustomErr_IpcFail(
    bool* aTryDidNotReturn) {
  QM_TRY_INSPECT(const auto& x, (mozilla::Result<int32_t, nsresult>{42}),
                 IPC_FAIL(this, "Custom why"));
  Unused << x;

  *aTryDidNotReturn = true;

  return IPC_OK();
}

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunreachable-code"
#endif

TEST(QuotaCommon_Try, Success)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(MOZ_TO_RESULT(NS_OK));

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_OK);
}

TEST(QuotaCommon_Try, Success_CustomErr_QmIpcFail)
{
  auto foo = MakeRefPtr<QuotaTestParent>();

  bool tryDidNotReturn = false;

  auto res = foo->RecvTry_Success_CustomErr_QmIpcFail(&tryDidNotReturn);

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_TRUE(res);
}

TEST(QuotaCommon_Try, Success_CustomErr_IpcFail)
{
  auto foo = MakeRefPtr<QuotaTestParent>();

  bool tryDidNotReturn = false;

  auto res = foo->RecvTry_Success_CustomErr_IpcFail(&tryDidNotReturn);

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_TRUE(res);
}

#ifdef DEBUG
TEST(QuotaCommon_Try, Success_CustomErr_AssertUnreachable)
{
  bool tryDidNotReturn = false;

  nsresult rv = [&tryDidNotReturn]() -> nsresult {
    QM_TRY(MOZ_TO_RESULT(NS_OK), QM_ASSERT_UNREACHABLE);

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
    QM_TRY(MOZ_TO_RESULT(NS_OK), QM_ASSERT_UNREACHABLE_VOID);

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
    QM_TRY(MOZ_TO_RESULT(NS_OK), QM_DIAGNOSTIC_ASSERT_UNREACHABLE);

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
    QM_TRY(MOZ_TO_RESULT(NS_OK), QM_DIAGNOSTIC_ASSERT_UNREACHABLE_VOID);

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

TEST(QuotaCommon_Try, Success_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                                 \
  {                                                                  \
    bool tryDidNotReturn = false;                                    \
                                                                     \
    nsresult rv = [&tryDidNotReturn]() -> nsresult {                 \
      QM_TRY(MOZ_TO_RESULT(NS_OK), [](__VA_ARGS__) { return aRv; }); \
                                                                     \
      tryDidNotReturn = true;                                        \
                                                                     \
      return NS_OK;                                                  \
    }();                                                             \
                                                                     \
    EXPECT_TRUE(tryDidNotReturn);                                    \
    EXPECT_EQ(rv, NS_OK);                                            \
  }

  SUBTEST(const char*, nsresult aRv);
  SUBTEST(nsresult aRv);

#undef SUBTEST
}

TEST(QuotaCommon_Try, Success_WithCleanup)
{
  bool tryCleanupRan = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&tryCleanupRan, &tryDidNotReturn]() -> nsresult {
    QM_TRY(MOZ_TO_RESULT(NS_OK), QM_PROPAGATE,
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
    QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE));

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
    QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), NS_ERROR_UNEXPECTED);

    tryDidNotReturn = true;

    return NS_OK;
  }();

  EXPECT_FALSE(tryDidNotReturn);
  EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);
}

TEST(QuotaCommon_Try, Failure_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                           \
  {                                                            \
    bool tryDidNotReturn = false;                              \
                                                               \
    nsresult rv = [&tryDidNotReturn]() -> nsresult {           \
      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE),                  \
             [](__VA_ARGS__) { return NS_ERROR_UNEXPECTED; }); \
                                                               \
      tryDidNotReturn = true;                                  \
                                                               \
      return NS_OK;                                            \
    }();                                                       \
                                                               \
    EXPECT_FALSE(tryDidNotReturn);                             \
    EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);                        \
  }

  SUBTEST(const char* aFunc, nsresult);
  SUBTEST(nsresult rv);

#undef SUBTEST
}

TEST(QuotaCommon_Try, Failure_NoErr)
{
  bool tryDidNotReturn = false;

  [&tryDidNotReturn]() -> void {
    QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), QM_VOID);

    tryDidNotReturn = true;
  }();

  EXPECT_FALSE(tryDidNotReturn);
}

TEST(QuotaCommon_Try, Failure_WithCleanup)
{
  bool tryCleanupRan = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&tryCleanupRan, &tryDidNotReturn]() -> nsresult {
    QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), QM_PROPAGATE,
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
    QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), QM_VOID,
           ([&tryCleanupRan, &aRv](auto& result) {
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

TEST(QuotaCommon_Try, Failure_WithCleanupAndPredicate)
{
  auto predicate = []() {
    static bool calledOnce = false;
    const bool result = !calledOnce;
    calledOnce = true;
    return result;
  };

  {
    bool tryDidNotReturn = false;

    nsresult rv = [&predicate, &tryDidNotReturn]() -> nsresult {
      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), QM_PROPAGATE, QM_NO_CLEANUP,
             predicate);

      tryDidNotReturn = true;

      return NS_OK;
    }();

    EXPECT_FALSE(tryDidNotReturn);
    EXPECT_EQ(rv, NS_ERROR_FAILURE);
  }

  {
    bool tryDidNotReturn = false;

    nsresult rv = [&predicate, &tryDidNotReturn]() -> nsresult {
      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE), QM_PROPAGATE, QM_NO_CLEANUP,
             predicate);

      tryDidNotReturn = true;

      return NS_OK;
    }();

    EXPECT_FALSE(tryDidNotReturn);
    EXPECT_EQ(rv, NS_ERROR_FAILURE);
  }
}

TEST(QuotaCommon_Try, SameLine)
{
  // clang-format off
  QM_TRY(MOZ_TO_RESULT(NS_OK), QM_VOID); QM_TRY(MOZ_TO_RESULT(NS_OK), QM_VOID);
  // clang-format on
}

TEST(QuotaCommon_Try, NestingMadness_Success)
{
  bool nestedTryDidNotReturn = false;
  bool tryDidNotReturn = false;

  nsresult rv = [&nestedTryDidNotReturn, &tryDidNotReturn]() -> nsresult {
    QM_TRY(([&nestedTryDidNotReturn]() -> Result<Ok, nsresult> {
      QM_TRY(MOZ_TO_RESULT(NS_OK));

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
      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE));

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
      QM_TRY(MOZ_TO_RESULT(NS_OK));

      nestedTry1DidNotReturn = true;

      QM_TRY(MOZ_TO_RESULT(NS_OK));

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
      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE));

      nestedTry1DidNotReturn = true;

      QM_TRY(MOZ_TO_RESULT(NS_OK));

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
      QM_TRY(MOZ_TO_RESULT(NS_OK));

      nestedTry1DidNotReturn = true;

      QM_TRY(MOZ_TO_RESULT(NS_ERROR_FAILURE));

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

TEST(QuotaCommon_TryInspect, Success_CustomErr_QmIpcFail)
{
  auto foo = MakeRefPtr<QuotaTestParent>();

  bool tryDidNotReturn = false;

  auto res = foo->RecvTryInspect_Success_CustomErr_QmIpcFail(&tryDidNotReturn);

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_TRUE(res);
}

TEST(QuotaCommon_TryInspect, Success_CustomErr_IpcFail)
{
  auto foo = MakeRefPtr<QuotaTestParent>();

  bool tryDidNotReturn = false;

  auto res = foo->RecvTryInspect_Success_CustomErr_IpcFail(&tryDidNotReturn);

  EXPECT_TRUE(tryDidNotReturn);
  EXPECT_TRUE(res);
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

TEST(QuotaCommon_TryInspect, Success_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                                 \
  {                                                                  \
    bool tryInspectDidNotReturn = false;                             \
                                                                     \
    nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {          \
      QM_TRY_INSPECT(const auto& x, (Result<int32_t, nsresult>{42}), \
                     [](__VA_ARGS__) { return aRv; });               \
      EXPECT_EQ(x, 42);                                              \
                                                                     \
      tryInspectDidNotReturn = true;                                 \
                                                                     \
      return NS_OK;                                                  \
    }();                                                             \
                                                                     \
    EXPECT_TRUE(tryInspectDidNotReturn);                             \
    EXPECT_EQ(rv, NS_OK);                                            \
  }

  SUBTEST(const char*, nsresult aRv);
  SUBTEST(nsresult aRv);

#undef SUBTEST
}

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

TEST(QuotaCommon_TryInspect, Failure_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                                     \
  {                                                                      \
    bool tryInspectDidNotReturn = false;                                 \
                                                                         \
    nsresult rv = [&tryInspectDidNotReturn]() -> nsresult {              \
      QM_TRY_INSPECT(const auto& x,                                      \
                     (Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}), \
                     [](__VA_ARGS__) { return NS_ERROR_UNEXPECTED; });   \
      Unused << x;                                                       \
                                                                         \
      tryInspectDidNotReturn = true;                                     \
                                                                         \
      return NS_OK;                                                      \
    }();                                                                 \
                                                                         \
    EXPECT_FALSE(tryInspectDidNotReturn);                                \
    EXPECT_EQ(rv, NS_ERROR_UNEXPECTED);                                  \
  }

  SUBTEST(const char*, nsresult);
  SUBTEST(nsresult);

#undef SUBTEST
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
    QM_TRY_RETURN(MOZ_TO_RESULT(NS_OK));

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

TEST(QuotaCommon_TryReturn, Success_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                                \
  {                                                                 \
    bool tryReturnDidNotReturn = false;                             \
                                                                    \
    auto res = [&tryReturnDidNotReturn]() -> Result<Ok, nsresult> { \
      QM_TRY_RETURN(MOZ_TO_RESULT(NS_OK),                           \
                    [](__VA_ARGS__) { return Err(aRv); });          \
                                                                    \
      tryReturnDidNotReturn = true;                                 \
    }();                                                            \
                                                                    \
    EXPECT_FALSE(tryReturnDidNotReturn);                            \
    EXPECT_TRUE(res.isOk());                                        \
  }

  SUBTEST(const char*, nsresult aRv);
  SUBTEST(nsresult aRv);

#undef SUBTEST
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
    QM_TRY_RETURN(MOZ_TO_RESULT(NS_ERROR_FAILURE));

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

TEST(QuotaCommon_TryReturn, Failure_CustomErr_CustomLambda)
{
#define SUBTEST(...)                                                       \
  {                                                                        \
    bool tryReturnDidNotReturn = false;                                    \
                                                                           \
    auto res = [&tryReturnDidNotReturn]() -> Result<int32_t, nsresult> {   \
      QM_TRY_RETURN((Result<int32_t, nsresult>{Err(NS_ERROR_FAILURE)}),    \
                    [](__VA_ARGS__) { return Err(NS_ERROR_UNEXPECTED); }); \
                                                                           \
      tryReturnDidNotReturn = true;                                        \
    }();                                                                   \
                                                                           \
    EXPECT_FALSE(tryReturnDidNotReturn);                                   \
    EXPECT_TRUE(res.isErr());                                              \
    EXPECT_EQ(res.unwrapErr(), NS_ERROR_UNEXPECTED);                       \
  }

  SUBTEST(const char*, nsresult);
  SUBTEST(nsresult);

#undef SUBTEST
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

TEST(QuotaCommon_WarnOnlyTry, Success)
{
  bool warnOnlyTryDidNotReturn = false;

  const auto res =
      [&warnOnlyTryDidNotReturn]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY(OkIf(true));

    warnOnlyTryDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTry, Success_WithCleanup)
{
  bool warnOnlyTryCleanupRan = false;
  bool warnOnlyTryDidNotReturn = false;

  const auto res =
      [&warnOnlyTryCleanupRan,
       &warnOnlyTryDidNotReturn]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY(OkIf(true), [&warnOnlyTryCleanupRan](const auto&) {
      warnOnlyTryCleanupRan = true;
    });

    warnOnlyTryDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_FALSE(warnOnlyTryCleanupRan);
  EXPECT_TRUE(warnOnlyTryDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTry, Failure)
{
  bool warnOnlyTryDidNotReturn = false;

  const auto res =
      [&warnOnlyTryDidNotReturn]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY(OkIf(false));

    warnOnlyTryDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTry, Failure_WithCleanup)
{
  bool warnOnlyTryCleanupRan = false;
  bool warnOnlyTryDidNotReturn = false;

  const auto res =
      [&warnOnlyTryCleanupRan,
       &warnOnlyTryDidNotReturn]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY(OkIf(false), ([&warnOnlyTryCleanupRan](const auto&) {
                      warnOnlyTryCleanupRan = true;
                    }));

    warnOnlyTryDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryCleanupRan);
  EXPECT_TRUE(warnOnlyTryDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTryUnwrap, Success)
{
  bool warnOnlyTryUnwrapDidNotReturn = false;

  const auto res = [&warnOnlyTryUnwrapDidNotReturn]()
      -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY_UNWRAP(const auto x, (Result<int32_t, NotOk>{42}));
    EXPECT_TRUE(x);
    EXPECT_EQ(*x, 42);

    warnOnlyTryUnwrapDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryUnwrapDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTryUnwrap, Success_WithCleanup)
{
  bool warnOnlyTryUnwrapCleanupRan = false;
  bool warnOnlyTryUnwrapDidNotReturn = false;

  const auto res = [&warnOnlyTryUnwrapCleanupRan,
                    &warnOnlyTryUnwrapDidNotReturn]()
      -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY_UNWRAP(const auto x, (Result<int32_t, NotOk>{42}),
                           [&warnOnlyTryUnwrapCleanupRan](const auto&) {
                             warnOnlyTryUnwrapCleanupRan = true;
                           });
    EXPECT_TRUE(x);
    EXPECT_EQ(*x, 42);

    warnOnlyTryUnwrapDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_FALSE(warnOnlyTryUnwrapCleanupRan);
  EXPECT_TRUE(warnOnlyTryUnwrapDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTryUnwrap, Failure)
{
  bool warnOnlyTryUnwrapDidNotReturn = false;

  const auto res = [&warnOnlyTryUnwrapDidNotReturn]()
      -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY_UNWRAP(const auto x,
                           (Result<int32_t, NotOk>{Err(NotOk{})}));
    EXPECT_FALSE(x);

    warnOnlyTryUnwrapDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryUnwrapDidNotReturn);
}

TEST(QuotaCommon_WarnOnlyTryUnwrap, Failure_WithCleanup)
{
  bool warnOnlyTryUnwrapCleanupRan = false;
  bool warnOnlyTryUnwrapDidNotReturn = false;

  const auto res = [&warnOnlyTryUnwrapCleanupRan,
                    &warnOnlyTryUnwrapDidNotReturn]()
      -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_WARNONLY_TRY_UNWRAP(const auto x, (Result<int32_t, NotOk>{Err(NotOk{})}),
                           [&warnOnlyTryUnwrapCleanupRan](const auto&) {
                             warnOnlyTryUnwrapCleanupRan = true;
                           });
    EXPECT_FALSE(x);

    warnOnlyTryUnwrapDidNotReturn = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(warnOnlyTryUnwrapCleanupRan);
  EXPECT_TRUE(warnOnlyTryUnwrapDidNotReturn);
}

TEST(QuotaCommon_OrElseWarn, Success)
{
  bool fallbackRun = false;
  bool tryContinued = false;

  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN(OkIf(true), ([&fallbackRun](const NotOk) {
                             fallbackRun = true;
                             return mozilla::Result<mozilla::Ok, NotOk>{
                                 mozilla::Ok{}};
                           })));

    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_FALSE(fallbackRun);
  EXPECT_TRUE(tryContinued);
}

TEST(QuotaCommon_OrElseWarn, Failure_MappedToSuccess)
{
  bool fallbackRun = false;
  bool tryContinued = false;

  // XXX Consider allowing to set a custom error handler, so that we can
  // actually assert that a warning was emitted.
  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN(OkIf(false), ([&fallbackRun](const NotOk) {
                             fallbackRun = true;
                             return mozilla::Result<mozilla::Ok, NotOk>{
                                 mozilla::Ok{}};
                           })));
    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(fallbackRun);
  EXPECT_TRUE(tryContinued);
}

TEST(QuotaCommon_OrElseWarn, Failure_MappedToError)
{
  bool fallbackRun = false;
  bool tryContinued = false;

  // XXX Consider allowing to set a custom error handler, so that we can
  // actually assert that a warning was emitted.
  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN(OkIf(false), ([&fallbackRun](const NotOk) {
                             fallbackRun = true;
                             return mozilla::Result<mozilla::Ok, NotOk>{
                                 NotOk{}};
                           })));
    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isErr());
  EXPECT_TRUE(fallbackRun);
  EXPECT_FALSE(tryContinued);
}

TEST(QuotaCommon_OrElseWarnIf, Success)
{
  bool predicateRun = false;
  bool fallbackRun = false;
  bool tryContinued = false;

  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN_IF(
        OkIf(true),
        [&predicateRun](const NotOk) {
          predicateRun = true;
          return false;
        },
        ([&fallbackRun](const NotOk) {
          fallbackRun = true;
          return mozilla::Result<mozilla::Ok, NotOk>{mozilla::Ok{}};
        })));

    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_FALSE(predicateRun);
  EXPECT_FALSE(fallbackRun);
  EXPECT_TRUE(tryContinued);
}

TEST(QuotaCommon_OrElseWarnIf, Failure_PredicateReturnsFalse)
{
  bool predicateRun = false;
  bool fallbackRun = false;
  bool tryContinued = false;

  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN_IF(
        OkIf(false),
        [&predicateRun](const NotOk) {
          predicateRun = true;
          return false;
        },
        ([&fallbackRun](const NotOk) {
          fallbackRun = true;
          return mozilla::Result<mozilla::Ok, NotOk>{mozilla::Ok{}};
        })));

    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isErr());
  EXPECT_TRUE(predicateRun);
  EXPECT_FALSE(fallbackRun);
  EXPECT_FALSE(tryContinued);
}

TEST(QuotaCommon_OrElseWarnIf, Failure_PredicateReturnsTrue_MappedToSuccess)
{
  bool predicateRun = false;
  bool fallbackRun = false;
  bool tryContinued = false;

  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN_IF(
        OkIf(false),
        [&predicateRun](const NotOk) {
          predicateRun = true;
          return true;
        },
        ([&fallbackRun](const NotOk) {
          fallbackRun = true;
          return mozilla::Result<mozilla::Ok, NotOk>{mozilla::Ok{}};
        })));

    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isOk());
  EXPECT_TRUE(predicateRun);
  EXPECT_TRUE(fallbackRun);
  EXPECT_TRUE(tryContinued);
}

TEST(QuotaCommon_OrElseWarnIf, Failure_PredicateReturnsTrue_MappedToError)
{
  bool predicateRun = false;
  bool fallbackRun = false;
  bool tryContinued = false;

  const auto res = [&]() -> mozilla::Result<mozilla::Ok, NotOk> {
    QM_TRY(QM_OR_ELSE_WARN_IF(
        OkIf(false),
        [&predicateRun](const NotOk) {
          predicateRun = true;
          return true;
        },
        ([&fallbackRun](const NotOk) {
          fallbackRun = true;
          return mozilla::Result<mozilla::Ok, NotOk>{mozilla::NotOk{}};
        })));

    tryContinued = true;
    return mozilla::Ok{};
  }();

  EXPECT_TRUE(res.isErr());
  EXPECT_TRUE(predicateRun);
  EXPECT_TRUE(fallbackRun);
  EXPECT_FALSE(tryContinued);
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

TEST(QuotaCommon_IsSpecificError, Match)
{ EXPECT_TRUE(IsSpecificError<NS_ERROR_FAILURE>(NS_ERROR_FAILURE)); }

TEST(QuotaCommon_IsSpecificError, Mismatch)
{ EXPECT_FALSE(IsSpecificError<NS_ERROR_FAILURE>(NS_ERROR_UNEXPECTED)); }

TEST(QuotaCommon_ErrToOk, Bool_True)
{
  auto res = ErrToOk<true>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), true);
}

TEST(QuotaCommon_ErrToOk, Bool_False)
{
  auto res = ErrToOk<false>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), false);
}

TEST(QuotaCommon_ErrToOk, Int_42)
{
  auto res = ErrToOk<42>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), 42);
}

TEST(QuotaCommon_ErrToOk, NsCOMPtr_nullptr)
{
  auto res = ErrToOk<nullptr, nsCOMPtr<nsISupports>>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), nullptr);
}

TEST(QuotaCommon_ErrToDefaultOk, Ok)
{
  auto res = ErrToDefaultOk<Ok>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
}

TEST(QuotaCommon_ErrToDefaultOk, NsCOMPtr)
{
  auto res = ErrToDefaultOk<nsCOMPtr<nsISupports>>(NS_ERROR_FAILURE);
  EXPECT_TRUE(res.isOk());
  EXPECT_EQ(res.unwrap(), nullptr);
}

class StringPairParameterized
    : public ::testing::TestWithParam<std::pair<const char*, const char*>> {};

TEST_P(StringPairParameterized, AnonymizedOriginString) {
  const auto [in, expectedAnonymized] = GetParam();
  const auto anonymized = AnonymizedOriginString(nsDependentCString(in));
  EXPECT_STREQ(anonymized.get(), expectedAnonymized);
}

INSTANTIATE_TEST_SUITE_P(
    QuotaCommon, StringPairParameterized,
    ::testing::Values(
        // XXX Do we really want to anonymize about: origins?
        std::pair("about:home", "about:aaaa"),
        std::pair("https://foo.bar.com", "https://aaa.aaa.aaa"),
        std::pair("https://foo.bar.com:8000", "https://aaa.aaa.aaa:DDDD"),
        std::pair("file://UNIVERSAL_FILE_ORIGIN",
                  "file://aaaaaaaaa_aaaa_aaaaaa")));

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

TEST(QuotaCommon_MakeSourceFileRelativePath, ThisSourceFile)
{
  static constexpr auto thisSourceFileRelativePath =
      "dom/quota/test/gtest/TestQuotaCommon.cpp"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          nsLiteralCString(__FILE__))};

  EXPECT_STREQ(sourceFileRelativePath.get(), thisSourceFileRelativePath.get());
}

static nsCString MakeTreePath(const nsACString& aBasePath,
                              const nsACString& aRelativePath) {
  nsCString path{aBasePath};

  path.Append("/");
  path.Append(aRelativePath);

  return path;
}

static nsCString MakeSourceTreePath(const nsACString& aRelativePath) {
  return MakeTreePath(mozilla::dom::quota::detail::GetSourceTreeBase(),
                      aRelativePath);
}

static nsCString MakeObjdirDistIncludeTreePath(
    const nsACString& aRelativePath) {
  return MakeTreePath(
      mozilla::dom::quota::detail::GetObjdirDistIncludeTreeBase(),
      aRelativePath);
}

TEST(QuotaCommon_MakeSourceFileRelativePath, DomQuotaSourceFile)
{
  static constexpr auto domQuotaSourceFileRelativePath =
      "dom/quota/ActorsParent.cpp"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          MakeSourceTreePath(domQuotaSourceFileRelativePath))};

  EXPECT_STREQ(sourceFileRelativePath.get(),
               domQuotaSourceFileRelativePath.get());
}

TEST(QuotaCommon_MakeSourceFileRelativePath, DomQuotaSourceFile_Exported)
{
  static constexpr auto mozillaDomQuotaSourceFileRelativePath =
      "mozilla/dom/quota/QuotaCommon.h"_ns;

  static constexpr auto domQuotaSourceFileRelativePath =
      "dom/quota/QuotaCommon.h"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          MakeObjdirDistIncludeTreePath(
              mozillaDomQuotaSourceFileRelativePath))};

  EXPECT_STREQ(sourceFileRelativePath.get(),
               domQuotaSourceFileRelativePath.get());
}

TEST(QuotaCommon_MakeSourceFileRelativePath, DomIndexedDBSourceFile)
{
  static constexpr auto domIndexedDBSourceFileRelativePath =
      "dom/indexedDB/ActorsParent.cpp"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          MakeSourceTreePath(domIndexedDBSourceFileRelativePath))};

  EXPECT_STREQ(sourceFileRelativePath.get(),
               domIndexedDBSourceFileRelativePath.get());
}

TEST(QuotaCommon_MakeSourceFileRelativePath,
     DomLocalstorageSourceFile_Exported_Mapped)
{
  static constexpr auto mozillaDomSourceFileRelativePath =
      "mozilla/dom/LocalStorageCommon.h"_ns;

  static constexpr auto domLocalstorageSourceFileRelativePath =
      "dom/localstorage/LocalStorageCommon.h"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          MakeObjdirDistIncludeTreePath(mozillaDomSourceFileRelativePath))};

  EXPECT_STREQ(sourceFileRelativePath.get(),
               domLocalstorageSourceFileRelativePath.get());
}

TEST(QuotaCommon_MakeSourceFileRelativePath, NonDomSourceFile)
{
  static constexpr auto nonDomSourceFileRelativePath =
      "storage/mozStorageService.cpp"_ns;

  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          MakeSourceTreePath(nonDomSourceFileRelativePath))};

  EXPECT_STREQ(sourceFileRelativePath.get(),
               nonDomSourceFileRelativePath.get());
}

TEST(QuotaCommon_MakeSourceFileRelativePath, OtherSourceFile)
{
  constexpr auto otherSourceFilePath = "/foo/bar/Test.cpp"_ns;
  const nsCString sourceFileRelativePath{
      mozilla::dom::quota::detail::MakeSourceFileRelativePath(
          otherSourceFilePath)};

  EXPECT_STREQ(sourceFileRelativePath.get(), "Test.cpp");
}

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

}  // namespace mozilla::dom::quota
