/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBResult.h"

#include "gtest/gtest.h"

using mozilla::ErrorResult;
using namespace mozilla::dom::indexedDB;

TEST(IDBResultTest, ConstructWithValue)
{
  IDBResult<int, IDBSpecialValue::Failure> result(Ok(0));
  EXPECT_FALSE(result.Is(SpecialValues::Failure));
  EXPECT_TRUE(result.Is(Ok));
  EXPECT_EQ(result.Unwrap(), 0);
}

TEST(IDBResultTest, Expand)
{
  IDBResult<int, IDBSpecialValue::Failure> narrow{SpecialValues::Failure};
  IDBResult<int, IDBSpecialValue::Failure, IDBSpecialValue::Invalid> wide{
      std::move(narrow)};
  EXPECT_TRUE(wide.Is(SpecialValues::Failure));
}

IDBResult<int, IDBSpecialValue::Failure> ThrowException() {
  return {SpecialValues::Exception, ErrorResult{NS_ERROR_FAILURE}};
}

TEST(IDBResultTest, ThrowException)
{
  auto result = ThrowException();
  EXPECT_TRUE(result.Is(SpecialValues::Exception));
  result.AsException().SuppressException();
}
