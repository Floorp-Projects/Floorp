/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBResult.h"

#include "gtest/gtest.h"

using mozilla::ErrorResult;
using namespace mozilla::dom::indexedDB;

TEST(IDBResultTest, ConstructWithValue)
{
  IDBResult<int, IDBSpecialValue::Failure> result(0);
  EXPECT_FALSE(result.isErr() &&
               result.inspectErr().Is(SpecialValues::Failure));
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(result.unwrap(), 0);
}

TEST(IDBResultTest, Expand)
{
  IDBResult<int, IDBSpecialValue::Failure> narrow{
      mozilla::Err(SpecialValues::Failure)};
  IDBResult<int, IDBSpecialValue::Failure, IDBSpecialValue::Invalid> wide{
      narrow.propagateErr()};
  EXPECT_TRUE(wide.isErr() && wide.inspectErr().Is(SpecialValues::Failure));
}

IDBResult<int, IDBSpecialValue::Failure> ThrowException() {
  return mozilla::Err(IDBException(NS_ERROR_FAILURE));
}

TEST(IDBResultTest, ThrowException)
{
  auto result = ThrowException();
  EXPECT_TRUE(result.isErr() &&
              result.inspectErr().Is(SpecialValues::Exception));
  result.unwrapErr().AsException().SuppressException();
}
