/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_TESTHELPERS_H_
#define DOM_FS_TEST_GTEST_TESTHELPERS_H_

#include "ErrorList.h"
#include "gtest/gtest.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/QuotaCommon.h"

namespace testing::internal {

GTEST_API_ ::testing::AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                                     const char* s2_expression,
                                                     const nsAString& s1,
                                                     const nsAString& s2);

GTEST_API_ ::testing::AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                                     const char* s2_expression,
                                                     const nsACString& s1,
                                                     const nsACString& s2);

}  // namespace testing::internal

#define ASSERT_NSEQ(lhs, rhs) \
  ASSERT_STREQ(GetStaticErrorName((lhs)), GetStaticErrorName((rhs)))

#define TEST_TRY_UNWRAP_META(tempVar, target, expr)                \
  auto MOZ_REMOVE_PAREN(tempVar) = (expr);                         \
  ASSERT_TRUE(MOZ_REMOVE_PAREN(tempVar).isOk())                    \
  << GetStaticErrorName(                                           \
      mozilla::ToNSResult(MOZ_REMOVE_PAREN(tempVar).unwrapErr())); \
  MOZ_REMOVE_PAREN(target) = MOZ_REMOVE_PAREN(tempVar).unwrap();

#define TEST_TRY_UNWRAP_ERR_META(tempVar, target, expr) \
  auto MOZ_REMOVE_PAREN(tempVar) = (expr);              \
  ASSERT_TRUE(MOZ_REMOVE_PAREN(tempVar).isErr());       \
  MOZ_REMOVE_PAREN(target) =                            \
      mozilla::ToNSResult(MOZ_REMOVE_PAREN(tempVar).unwrapErr());

#define TEST_TRY_UNWRAP(target, expr) \
  TEST_TRY_UNWRAP_META(MOZ_UNIQUE_VAR(testVar), target, expr)

#define TEST_TRY_UNWRAP_ERR(target, expr) \
  TEST_TRY_UNWRAP_ERR_META(MOZ_UNIQUE_VAR(testVar), target, expr)

namespace mozilla::dom {

namespace quota {

struct OriginMetadata;

}  // namespace quota

namespace fs::test {

quota::OriginMetadata GetTestOriginMetadata();

const Origin& GetTestOrigin();

}  // namespace fs::test
}  // namespace mozilla::dom

#endif  // DOM_FS_TEST_GTEST_TESTHELPERS_H_
