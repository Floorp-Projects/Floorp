/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBResult.h"

#include "gtest/gtest.h"

using mozilla::ErrorResult;
using namespace mozilla::dom::indexedDB;

TEST(IDBResultTest, ConstructWithValue)
{
  ErrorResult rv;
  IDBResult<int, IDBSpecialValue::Failure> result(Ok(0));
  EXPECT_FALSE(result.Is(Failure, rv));
  EXPECT_TRUE(result.Is(Ok, rv));
  EXPECT_EQ(result.Unwrap(rv), 0);
}

TEST(IDBResultTest, Expand)
{
  ErrorResult rv;
  IDBResult<int, IDBSpecialValue::Failure> narrow{Failure};
  IDBResult<int, IDBSpecialValue::Failure, IDBSpecialValue::Invalid> wide{
      narrow};
  EXPECT_TRUE(wide.Is(Failure, rv));
}

IDBResult<int, IDBSpecialValue::Failure> ThrowException(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_FAILURE);
  return Exception;
}

TEST(IDBResultTest, ThrowException)
{
  ErrorResult rv;
  const auto result = ThrowException(rv);
  EXPECT_TRUE(result.Is(Exception, rv));
  rv.SuppressException();
}
