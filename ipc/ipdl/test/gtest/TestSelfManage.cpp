/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test that actors can manage other actors of the same type.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestSelfManageChild.h"
#include "mozilla/_ipdltest/PTestSelfManageParent.h"
#include "mozilla/_ipdltest/PTestSelfManageRootChild.h"
#include "mozilla/_ipdltest/PTestSelfManageRootParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestSelfManageParent : public PTestSelfManageParent {
  NS_INLINE_DECL_REFCOUNTING(TestSelfManageParent, override)
 public:
  ActorDestroyReason mWhy = ActorDestroyReason(-1);

 private:
  void ActorDestroy(ActorDestroyReason why) final override { mWhy = why; }

  ~TestSelfManageParent() = default;
};

class TestSelfManageChild : public PTestSelfManageChild {
  NS_INLINE_DECL_REFCOUNTING(TestSelfManageChild, override)
 private:
  already_AddRefed<PTestSelfManageChild> AllocPTestSelfManageChild()
      final override {
    return MakeAndAddRef<TestSelfManageChild>();
  }

  ~TestSelfManageChild() = default;
};

class TestSelfManageRootParent : public PTestSelfManageRootParent {
  NS_INLINE_DECL_REFCOUNTING(TestSelfManageRootParent, override)
 private:
  ~TestSelfManageRootParent() = default;
};

class TestSelfManageRootChild : public PTestSelfManageRootChild {
  NS_INLINE_DECL_REFCOUNTING(TestSelfManageRootChild, override)
 private:
  already_AddRefed<PTestSelfManageChild> AllocPTestSelfManageChild()
      final override {
    return MakeAndAddRef<TestSelfManageChild>();
  }

  ~TestSelfManageRootChild() = default;
};

IPDL_TEST(TestSelfManageRoot) {
  auto child = MakeRefPtr<TestSelfManageParent>();
  EXPECT_TRUE(mActor->SendPTestSelfManageConstructor(child));

  EXPECT_EQ(mActor->ManagedPTestSelfManageParent().Count(), 1u);

  {
    auto childsChild = MakeRefPtr<TestSelfManageParent>();
    EXPECT_TRUE(child->SendPTestSelfManageConstructor(childsChild));

    EXPECT_EQ(mActor->ManagedPTestSelfManageParent().Count(), 1u);
    EXPECT_EQ(child->ManagedPTestSelfManageParent().Count(), 1u);

    EXPECT_TRUE(PTestSelfManageParent::Send__delete__(childsChild));

    EXPECT_EQ(childsChild->mWhy, IProtocol::Deletion);
    EXPECT_EQ(mActor->ManagedPTestSelfManageParent().Count(), 1u);
    EXPECT_EQ(child->ManagedPTestSelfManageParent().Count(), 0u);
  }

  {
    auto childsChild = MakeRefPtr<TestSelfManageParent>();
    EXPECT_TRUE(child->SendPTestSelfManageConstructor(childsChild));

    EXPECT_EQ(mActor->ManagedPTestSelfManageParent().Count(), 1u);
    EXPECT_EQ(child->ManagedPTestSelfManageParent().Count(), 1u);

    EXPECT_TRUE(PTestSelfManageParent::Send__delete__(child));

    EXPECT_EQ(child->mWhy, IProtocol::Deletion);
    EXPECT_EQ(childsChild->mWhy, IProtocol::AncestorDeletion);
    EXPECT_EQ(mActor->ManagedPTestSelfManageParent().Count(), 0u);
    EXPECT_EQ(child->ManagedPTestSelfManageParent().Count(), 0u);
  }

  mActor->Close();
}

}  // namespace mozilla::_ipdltest
