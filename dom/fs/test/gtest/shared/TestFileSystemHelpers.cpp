/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FileSystemHelpers.h"

namespace mozilla::dom::fs {

namespace {

class TestObject {
 public:
  explicit TestObject(uint32_t aExpectedAddRefCnt = 0,
                      uint32_t aExpectedAddRegCnt = 0)
      : mExpectedAddRefCnt(aExpectedAddRefCnt),
        mExpectedAddRegCnt(aExpectedAddRegCnt),
        mAddRefCnt(0),
        mAddRegCnt(0),
        mRefCnt(0),
        mRegCnt(0),
        mClosed(false) {}

  uint32_t AddRef() {
    mRefCnt++;
    mAddRefCnt++;
    return mRefCnt;
  }

  uint32_t Release() {
    EXPECT_TRUE(mRefCnt > 0);
    mRefCnt--;
    if (mRefCnt == 0) {
      delete this;
      return 0;
    }
    return mRefCnt;
  }

  void Register() {
    EXPECT_FALSE(mClosed);
    mRegCnt++;
    mAddRegCnt++;
  }

  void Unregister() {
    EXPECT_FALSE(mClosed);
    EXPECT_TRUE(mRegCnt > 0);
    mRegCnt--;
    if (mRegCnt == 0) {
      mClosed = true;
    }
  }

  void Foo() const {}

 private:
  ~TestObject() {
    if (mExpectedAddRefCnt > 0) {
      EXPECT_EQ(mAddRefCnt, mExpectedAddRefCnt);
    }
    if (mExpectedAddRegCnt > 0) {
      EXPECT_EQ(mAddRegCnt, mExpectedAddRegCnt);
    }
  }

  uint32_t mExpectedAddRefCnt;
  uint32_t mExpectedAddRegCnt;
  uint32_t mAddRefCnt;
  uint32_t mAddRegCnt;
  uint32_t mRefCnt;
  uint32_t mRegCnt;
  bool mClosed;
};

}  // namespace

TEST(TestFileSystemHelpers_Registered, Construct_Default)
{
  { Registered<TestObject> testObject; }
}

TEST(TestFileSystemHelpers_Registered, Construct_Copy)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(2, 2));
    Registered<TestObject> testObject2(testObject1);
    testObject2 = nullptr;
  }
}

TEST(TestFileSystemHelpers_Registered, Construct_Move)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    Registered<TestObject> testObject2(std::move(testObject1));
  }
}

TEST(TestFileSystemHelpers_Registered, Construct_FromRefPtr)
{
  { Registered<TestObject> testObject(MakeRefPtr<TestObject>(1, 1)); }
}

TEST(TestFileSystemHelpers_Registered, Operator_Assign_FromNullPtr)
{
  {
    Registered<TestObject> testObject;
    testObject = nullptr;
  }
}

TEST(TestFileSystemHelpers_Registered, Operator_Assign_Copy)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(2, 2));
    Registered<TestObject> testObject2;
    testObject2 = testObject1;
  }
}

TEST(TestFileSystemHelpers_Registered, Operator_Assign_Move)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    Registered<TestObject> testObject2;
    testObject2 = std::move(testObject1);
  }
}

TEST(TestFileSystemHelpers_Registered, Method_Inspect)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    const RefPtr<TestObject>& testObject2 = testObject1.inspect();
    Unused << testObject2;
  }
}

TEST(TestFileSystemHelpers_Registered, Method_Unwrap)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    RefPtr<TestObject> testObject2 = testObject1.unwrap();
    Unused << testObject2;
  }
}

TEST(TestFileSystemHelpers_Registered, Method_Get)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    TestObject* testObject2 = testObject1.get();
    Unused << testObject2;
  }
}

TEST(TestFileSystemHelpers_Registered, Operator_Conversion_ToRawPtr)
{
  {
    Registered<TestObject> testObject1(MakeRefPtr<TestObject>(1, 1));
    TestObject* testObject2 = testObject1;
    Unused << testObject2;
  }
}

TEST(TestFileSystemHelpers_Registered, Operator_Dereference_ToRawPtr)
{
  {
    Registered<TestObject> testObject(MakeRefPtr<TestObject>(1, 1));
    testObject->Foo();
  }
}

}  // namespace mozilla::dom::fs
