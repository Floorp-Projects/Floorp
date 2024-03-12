/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test that many child allocations work correctly.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsChild.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsSubChild.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsSubParent.h"

using namespace mozilla::ipc;

#define NALLOCS 10

namespace mozilla::_ipdltest {

class TestManyChildAllocsSubParent : public PTestManyChildAllocsSubParent {
  NS_INLINE_DECL_REFCOUNTING(TestManyChildAllocsSubParent, override)
 private:
  IPCResult RecvHello() final override { return IPC_OK(); }

  ~TestManyChildAllocsSubParent() = default;
};

class TestManyChildAllocsSubChild : public PTestManyChildAllocsSubChild {
  NS_INLINE_DECL_REFCOUNTING(TestManyChildAllocsSubChild, override)
 private:
  ~TestManyChildAllocsSubChild() = default;
};

class TestManyChildAllocsParent : public PTestManyChildAllocsParent {
  NS_INLINE_DECL_REFCOUNTING(TestManyChildAllocsParent, override)
 private:
  IPCResult RecvDone() final override {
    Close();

    return IPC_OK();
  }

  already_AddRefed<PTestManyChildAllocsSubParent>
  AllocPTestManyChildAllocsSubParent() final override {
    return MakeAndAddRef<TestManyChildAllocsSubParent>();
  }

  ~TestManyChildAllocsParent() = default;
};

class TestManyChildAllocsChild : public PTestManyChildAllocsChild {
  NS_INLINE_DECL_REFCOUNTING(TestManyChildAllocsChild, override)
 private:
  IPCResult RecvGo() final override {
    for (int i = 0; i < NALLOCS; ++i) {
      auto child = MakeRefPtr<TestManyChildAllocsSubChild>();
      EXPECT_TRUE(SendPTestManyChildAllocsSubConstructor(child));
      EXPECT_TRUE(child->SendHello()) << "can't send Hello()";
    }

    size_t len = ManagedPTestManyChildAllocsSubChild().Count();
    EXPECT_EQ((size_t)NALLOCS, len)
        << "expected " << NALLOCS << " kids, got " << len;

    EXPECT_TRUE(SendDone()) << "can't send Done()";

    return IPC_OK();
  }

  ~TestManyChildAllocsChild() = default;
};

IPDL_TEST(TestManyChildAllocs) {
  EXPECT_TRUE(mActor->SendGo()) << "can't send Go()";
}

}  // namespace mozilla::_ipdltest
