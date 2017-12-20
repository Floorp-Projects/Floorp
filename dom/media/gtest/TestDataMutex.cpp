/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/DataMutex.h"
#include "nsTArray.h"

using mozilla::DataMutex;

struct A
{
  void Set(int a) { mValue = a; }
  int mValue;
};

TEST(DataMutex, Basic)
{
  {
    DataMutex<uint32_t> i(1, "1");
    auto x = i.Lock();
    *x = 4;
    ASSERT_EQ(*x, 4u);
  }
  {
    DataMutex<A> a({ 4 }, "StructA");
    auto x = a.Lock();
    ASSERT_EQ(x->mValue, 4);
    x->Set(8);
    ASSERT_EQ(x->mValue, 8);
  }
  {
    DataMutex<nsTArray<uint32_t>> _a("array");
    auto a = _a.Lock();
    auto& x = a.ref();
    ASSERT_EQ(x.Length(), 0u);
    x.AppendElement(1u);
    ASSERT_EQ(x.Length(), 1u);
    ASSERT_EQ(x[0], 1u);
  }
}
