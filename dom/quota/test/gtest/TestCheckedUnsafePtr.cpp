/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/CheckedUnsafePtr.h"

#include "gtest/gtest.h"

using namespace mozilla;

class TestCheckingPolicy : public CheckCheckedUnsafePtrs<TestCheckingPolicy> {
 protected:
  explicit TestCheckingPolicy(bool& aPassedCheck)
      : mPassedCheck(aPassedCheck) {}

 private:
  friend class mozilla::CheckingPolicyAccess;
  void NotifyCheckFailure() { mPassedCheck = false; }

  bool& mPassedCheck;
};

struct BasePointee : public SupportsCheckedUnsafePtr<TestCheckingPolicy> {
  explicit BasePointee(bool& aCheckPassed)
      : SupportsCheckedUnsafePtr<TestCheckingPolicy>(aCheckPassed) {}
};

struct DerivedPointee : public BasePointee {
  using BasePointee::BasePointee;
};

class CheckedUnsafePtrTest : public ::testing::Test {
 protected:
  bool mPassedCheck = true;
};

TEST_F(CheckedUnsafePtrTest, PointeeWithNoCheckedUnsafePtrs) {
  { DerivedPointee pointee{mPassedCheck}; }
  ASSERT_TRUE(mPassedCheck);
}

template <typename PointerType>
class TypedCheckedUnsafePtrTest : public CheckedUnsafePtrTest {};

TYPED_TEST_CASE_P(TypedCheckedUnsafePtrTest);

TYPED_TEST_P(TypedCheckedUnsafePtrTest, PointeeWithOneCheckedUnsafePtr) {
  {
    DerivedPointee pointee{this->mPassedCheck};
    CheckedUnsafePtr<TypeParam> ptr = &pointee;
  }
  ASSERT_TRUE(this->mPassedCheck);
}

TYPED_TEST_P(TypedCheckedUnsafePtrTest,
             PointeeWithOneDanglingCheckedUnsafePtr) {
  [this]() -> CheckedUnsafePtr<TypeParam> {
    DerivedPointee pointee{this->mPassedCheck};
    return &pointee;
  }();
  ASSERT_FALSE(this->mPassedCheck);
}

TYPED_TEST_P(TypedCheckedUnsafePtrTest,
             PointeeWithOneCopiedDanglingCheckedUnsafePtr) {
  const auto dangling1 = [this]() -> CheckedUnsafePtr<DerivedPointee> {
    DerivedPointee pointee{this->mPassedCheck};
    return &pointee;
  }();
  EXPECT_FALSE(this->mPassedCheck);

  // With AddressSanitizer we would hopefully detect if the copy constructor
  // tries to add dangling2 to the now-gone pointee's unsafe pointer array. No
  // promises though, since it might be optimized away.
  CheckedUnsafePtr<TypeParam> dangling2{dangling1};
  ASSERT_TRUE(dangling2);
}

TYPED_TEST_P(TypedCheckedUnsafePtrTest,
             PointeeWithOneCopyAssignedDanglingCheckedUnsafePtr) {
  const auto dangling1 = [this]() -> CheckedUnsafePtr<DerivedPointee> {
    DerivedPointee pointee{this->mPassedCheck};
    return &pointee;
  }();
  EXPECT_FALSE(this->mPassedCheck);

  // With AddressSanitizer we would hopefully detect if the assignment tries to
  // add dangling2 to the now-gone pointee's unsafe pointer array. No promises
  // though, since it might be optimized away.
  CheckedUnsafePtr<TypeParam> dangling2;
  dangling2 = dangling1;
  ASSERT_TRUE(dangling2);
}

REGISTER_TYPED_TEST_CASE_P(TypedCheckedUnsafePtrTest,
                           PointeeWithOneCheckedUnsafePtr,
                           PointeeWithOneDanglingCheckedUnsafePtr,
                           PointeeWithOneCopiedDanglingCheckedUnsafePtr,
                           PointeeWithOneCopyAssignedDanglingCheckedUnsafePtr);

using BothTypes = ::testing::Types<BasePointee, DerivedPointee>;
INSTANTIATE_TYPED_TEST_CASE_P(InstantiationOf, TypedCheckedUnsafePtrTest,
                              BothTypes);
