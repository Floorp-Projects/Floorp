/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Flatten.h"

#include "gtest/gtest.h"

#include "mozilla/Unused.h"
#include "nsTArray.h"

namespace mozilla::dom::quota {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunreachable-code-loop-increment"
#endif
TEST(Flatten, FlatEmpty)
{
  for (const auto& item : Flatten<int>(nsTArray<int>{})) {
    Unused << item;
    FAIL();
  }
}

TEST(Flatten, NestedOuterEmpty)
{
  for (const auto& item : Flatten<int>(nsTArray<CopyableTArray<int>>{})) {
    Unused << item;
    FAIL();
  }
}

TEST(Flatten, NestedInnerEmpty)
{
  for (const auto& item :
       Flatten<int>(nsTArray<CopyableTArray<int>>{CopyableTArray<int>{}})) {
    Unused << item;
    FAIL();
  }
}
#ifdef __clang__
#  pragma clang diagnostic pop
#endif

TEST(Flatten, NestedInnerSingular)
{
  nsTArray<int> flattened;
  for (const auto& item :
       Flatten<int>(nsTArray<CopyableTArray<int>>{CopyableTArray<int>{1}})) {
    flattened.AppendElement(item);
  }

  EXPECT_EQ(nsTArray{1}, flattened);
}

TEST(Flatten, NestedInnerSingulars)
{
  nsTArray<int> flattened;
  for (const auto& item : Flatten<int>(nsTArray<CopyableTArray<int>>{
           CopyableTArray<int>{1}, CopyableTArray<int>{2}})) {
    flattened.AppendElement(item);
  }

  EXPECT_EQ((nsTArray<int>{{1, 2}}), flattened);
}

TEST(Flatten, NestedInnerNonSingulars)
{
  nsTArray<int> flattened;
  for (const auto& item : Flatten<int>(nsTArray<CopyableTArray<int>>{
           CopyableTArray<int>{1, 2}, CopyableTArray<int>{3, 4}})) {
    flattened.AppendElement(item);
  }

  EXPECT_EQ((nsTArray<int>{{1, 2, 3, 4}}), flattened);
}

}  // namespace mozilla::dom::quota
