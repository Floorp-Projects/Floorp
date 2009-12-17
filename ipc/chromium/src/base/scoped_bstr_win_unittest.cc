// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_bstr_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const wchar_t kTestString1[] = L"123";
static const wchar_t kTestString2[] = L"456789";
uint32 test1_len = arraysize(kTestString1) - 1;
uint32 test2_len = arraysize(kTestString2) - 1;

void DumbBstrTests() {
  ScopedBstr b;
  EXPECT_TRUE(b == NULL);
  EXPECT_TRUE(b.Length() == 0);
  EXPECT_TRUE(b.ByteLength() == 0);
  b.Reset(NULL);
  EXPECT_TRUE(b == NULL);
  EXPECT_TRUE(b.Release() == NULL);
  ScopedBstr b2;
  b.Swap(b2);
  EXPECT_TRUE(b2 == NULL);
}

void GiveMeABstr(BSTR* ret) {
  *ret = SysAllocString(kTestString1);
}

void BasicBstrTests() {
  ScopedBstr b1(kTestString1);
  EXPECT_TRUE(b1.Length() == test1_len);
  EXPECT_TRUE(b1.ByteLength() == test1_len * sizeof(kTestString1[0]));

  ScopedBstr b2;
  b1.Swap(b2);
  EXPECT_TRUE(b2.Length() == test1_len);
  EXPECT_TRUE(b1.Length() == 0);
  EXPECT_TRUE(lstrcmpW(b2, kTestString1) == 0);
  BSTR tmp = b2.Release();
  EXPECT_TRUE(tmp != NULL);
  EXPECT_TRUE(lstrcmpW(tmp, kTestString1) == 0);
  EXPECT_TRUE(b2 == NULL);
  SysFreeString(tmp);

  GiveMeABstr(b2.Receive());
  EXPECT_TRUE(b2 != NULL);
  b2.Reset();
  EXPECT_TRUE(b2.AllocateBytes(100) != NULL);
  EXPECT_TRUE(b2.ByteLength() == 100);
  EXPECT_TRUE(b2.Length() == 100 / sizeof(kTestString1[0]));
  lstrcpyW(static_cast<BSTR>(b2), kTestString1);
  EXPECT_TRUE(lstrlen(b2) == test1_len);
  EXPECT_TRUE(b2.Length() == 100 / sizeof(kTestString1[0]));
  b2.SetByteLen(lstrlen(b2) * sizeof(kTestString2[0]));
  EXPECT_TRUE(lstrlen(b2) == b2.Length());

  EXPECT_TRUE(b1.Allocate(kTestString2) != NULL);
  EXPECT_TRUE(b1.Length() == test2_len);
  b1.SetByteLen((test2_len - 1) * sizeof(kTestString2[0]));
  EXPECT_TRUE(b1.Length() == test2_len - 1);
}

}  // namespace

TEST(ScopedBstrTest, ScopedBstr) {
  DumbBstrTests();
  BasicBstrTests();
}

#define kSourceStr L"this is a string"
#define kSourceStrEmpty L""

TEST(StackBstrTest, StackBstr) {
  ScopedBstr system_bstr(kSourceStr);
  StackBstrVar(kSourceStr, stack_bstr);
  EXPECT_EQ(VarBstrCmp(system_bstr, stack_bstr, LOCALE_USER_DEFAULT, 0),
            VARCMP_EQ);

  StackBstrVar(kSourceStrEmpty, empty);
  uint32 l1 = SysStringLen(stack_bstr);
  uint32 l2 = SysStringLen(StackBstr(kSourceStr));
  uint32 l3 = SysStringLen(system_bstr);
  EXPECT_TRUE(l1 == l2);
  EXPECT_TRUE(l2 == l3);
  EXPECT_TRUE(SysStringLen(empty) == 0);

  const wchar_t one_more_test[] = L"this is my const string";
  EXPECT_EQ(SysStringLen(StackBstr(one_more_test)),
            lstrlenW(one_more_test));
}
