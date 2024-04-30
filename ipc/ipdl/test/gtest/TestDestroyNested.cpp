/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test an actor being destroyed during another ActorDestroy.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestDestroyNestedChild.h"
#include "mozilla/_ipdltest/PTestDestroyNestedParent.h"
#include "mozilla/_ipdltest/PTestDestroyNestedSubChild.h"
#include "mozilla/_ipdltest/PTestDestroyNestedSubParent.h"

#include "mozilla/SpinEventLoopUntil.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestDestroyNestedSubParent : public PTestDestroyNestedSubParent {
  NS_INLINE_DECL_REFCOUNTING(TestDestroyNestedSubParent, override)

  void ActorDestroy(ActorDestroyReason aReason) override;

  bool mActorDestroyCalled = false;
  bool mCloseManager = false;

  nsrefcnt GetRefCnt() const { return mRefCnt; }

 private:
  ~TestDestroyNestedSubParent() = default;
};

class TestDestroyNestedSubChild : public PTestDestroyNestedSubChild {
  NS_INLINE_DECL_REFCOUNTING(TestDestroyNestedSubChild, override)
 private:
  ~TestDestroyNestedSubChild() = default;
};

class TestDestroyNestedParent : public PTestDestroyNestedParent {
  NS_INLINE_DECL_REFCOUNTING(TestDestroyNestedParent, override)

  void ActorDestroy(ActorDestroyReason aReason) override;

  bool mActorDestroyCalled = false;

 private:
  ~TestDestroyNestedParent() = default;
};

class TestDestroyNestedChild : public PTestDestroyNestedChild {
  NS_INLINE_DECL_REFCOUNTING(TestDestroyNestedChild, override)

  already_AddRefed<PTestDestroyNestedSubChild> AllocPTestDestroyNestedSubChild()
      override {
    return MakeAndAddRef<TestDestroyNestedSubChild>();
  }

 private:
  ~TestDestroyNestedChild() = default;
};

void TestDestroyNestedSubParent::ActorDestroy(ActorDestroyReason aReason) {
  // Destroy our manager from within ActorDestroy() and assert that we don't
  // re-enter.
  EXPECT_FALSE(mActorDestroyCalled) << "re-entered ActorDestroy()";
  mActorDestroyCalled = true;

  if (mCloseManager) {
    EXPECT_FALSE(
        static_cast<TestDestroyNestedParent*>(Manager())->mActorDestroyCalled)
        << "manager already destroyed";
    Manager()->Close();
    EXPECT_TRUE(
        static_cast<TestDestroyNestedParent*>(Manager())->mActorDestroyCalled)
        << "manager successfully destroyed";
  }

  // Make sure we also process any pending events, because we might be spinning
  // a nested event loop within `ActorDestroy`.
  NS_ProcessPendingEvents(nullptr);
}

void TestDestroyNestedParent::ActorDestroy(ActorDestroyReason aReason) {
  // Destroy our manager from within ActorDestroy() and assert that we don't
  // re-enter.
  EXPECT_FALSE(mActorDestroyCalled) << "re-entered ActorDestroy()";
  mActorDestroyCalled = true;
}

IPDL_TEST(TestDestroyNested) {
  auto p = MakeRefPtr<TestDestroyNestedSubParent>();
  p->mCloseManager = true;
  auto* rv1 = mActor->SendPTestDestroyNestedSubConstructor(p);
  ASSERT_EQ(p, rv1) << "can't allocate Sub";

  bool rv2 = PTestDestroyNestedSubParent::Send__delete__(p);
  ASSERT_TRUE(rv2)
  << "Send__delete__ failed";

  ASSERT_TRUE(mActor->mActorDestroyCalled)
  << "Parent not destroyed";
  ASSERT_TRUE(p->mActorDestroyCalled)
  << "Sub not destroyed";

  ASSERT_EQ(p->GetRefCnt(), 1u) << "Outstanding references to Sub remain";

  // Try to allocate a new actor under the already-destroyed manager, and ensure
  // that it is destroyed as expected.
  auto p2 = MakeRefPtr<TestDestroyNestedSubParent>();
  auto* rv3 = mActor->SendPTestDestroyNestedSubConstructor(p2);
  ASSERT_EQ(rv3, nullptr) << "construction succeeded unexpectedly";

  ASSERT_TRUE(p2->mActorDestroyCalled)
  << "Sub not destroyed";
}

}  // namespace mozilla::_ipdltest
