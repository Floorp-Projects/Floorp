/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test multiple levels of descendant managed protocols.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestDescendantChild.h"
#include "mozilla/_ipdltest/PTestDescendantParent.h"
#include "mozilla/_ipdltest/PTestDescendantSubChild.h"
#include "mozilla/_ipdltest/PTestDescendantSubParent.h"
#include "mozilla/_ipdltest/PTestDescendantSubsubChild.h"
#include "mozilla/_ipdltest/PTestDescendantSubsubParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestDescendantSubsubParent : public PTestDescendantSubsubParent {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantSubsubParent, override)
 private:
  ~TestDescendantSubsubParent() = default;
};

class TestDescendantSubsubChild : public PTestDescendantSubsubChild {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantSubsubChild, override)
 private:
  ~TestDescendantSubsubChild() = default;
};

class TestDescendantSubParent : public PTestDescendantSubParent {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantSubParent, override)
 private:
  ~TestDescendantSubParent() = default;
};

class TestDescendantSubChild : public PTestDescendantSubChild {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantSubChild, override)
 private:
  already_AddRefed<PTestDescendantSubsubChild> AllocPTestDescendantSubsubChild()
      final override {
    return MakeAndAddRef<TestDescendantSubsubChild>();
  }

  ~TestDescendantSubChild() = default;
};

class TestDescendantParent : public PTestDescendantParent {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantParent, override)
 private:
  IPCResult RecvOk(NotNull<PTestDescendantSubsubParent*> a) final override {
    EXPECT_TRUE(PTestDescendantSubsubParent::Send__delete__(a))
        << "deleting Subsub";

    Close();

    return IPC_OK();
  }

  ~TestDescendantParent() = default;
};

class TestDescendantChild : public PTestDescendantChild {
  NS_INLINE_DECL_REFCOUNTING(TestDescendantChild, override)
 private:
  already_AddRefed<PTestDescendantSubChild> AllocPTestDescendantSubChild(
      PTestDescendantSubsubChild* dummy) final override {
    EXPECT_FALSE(dummy) << "actor supposed to be null";
    return MakeAndAddRef<TestDescendantSubChild>();
  }

  IPCResult RecvTest(NotNull<PTestDescendantSubsubChild*> a) final override {
    EXPECT_TRUE(SendOk(a)) << "couldn't send Ok()";
    return IPC_OK();
  }

  ~TestDescendantChild() = default;
};

IPDL_TEST(TestDescendant) {
  auto p = MakeRefPtr<TestDescendantSubParent>();
  auto* rv1 = mActor->SendPTestDescendantSubConstructor(p, nullptr);
  EXPECT_EQ(p, rv1) << "can't allocate Sub";

  auto pp = MakeRefPtr<TestDescendantSubsubParent>();
  auto* rv2 = p->SendPTestDescendantSubsubConstructor(pp);
  EXPECT_EQ(pp, rv2) << "can't allocate Subsub";

  EXPECT_TRUE(mActor->SendTest(WrapNotNull(pp))) << "can't send Subsub";
}

}  // namespace mozilla::_ipdltest
