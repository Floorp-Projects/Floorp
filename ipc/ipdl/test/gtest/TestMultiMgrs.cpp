/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test a chain of managers, ensuring ownership is maintained correctly.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestMultiMgrsChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomParent.h"

#include <algorithm>

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestMultiMgrsBottomParent : public PTestMultiMgrsBottomParent {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsBottomParent, override)
 private:
  ~TestMultiMgrsBottomParent() = default;
};

class TestMultiMgrsBottomChild : public PTestMultiMgrsBottomChild {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsBottomChild, override)
 private:
  ~TestMultiMgrsBottomChild() = default;
};

class TestMultiMgrsChild : public PTestMultiMgrsChild {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsChild, override)
 public:
  PTestMultiMgrsBottomChild* mBottomL = nullptr;
  PTestMultiMgrsBottomChild* mBottomR = nullptr;

 private:
  already_AddRefed<PTestMultiMgrsLeftChild> AllocPTestMultiMgrsLeftChild()
      final override;

  already_AddRefed<PTestMultiMgrsRightChild> AllocPTestMultiMgrsRightChild()
      final override;

  IPCResult RecvCheck() final override;

  ~TestMultiMgrsChild() = default;
};

class TestMultiMgrsLeftParent : public PTestMultiMgrsLeftParent {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsLeftParent, override)
 public:
  bool HasChild(PTestMultiMgrsBottomParent* c) {
    const auto& managed = ManagedPTestMultiMgrsBottomParent();
    return std::find(managed.begin(), managed.end(), c) != managed.end();
  }

 private:
  ~TestMultiMgrsLeftParent() = default;
};

class TestMultiMgrsLeftChild : public PTestMultiMgrsLeftChild {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsLeftChild, override)
 public:
  bool HasChild(PTestMultiMgrsBottomChild* c) {
    const auto& managed = ManagedPTestMultiMgrsBottomChild();
    return std::find(managed.begin(), managed.end(), c) != managed.end();
  }

 private:
  already_AddRefed<PTestMultiMgrsBottomChild> AllocPTestMultiMgrsBottomChild()
      final override {
    return MakeAndAddRef<TestMultiMgrsBottomChild>();
  }

  IPCResult RecvPTestMultiMgrsBottomConstructor(
      PTestMultiMgrsBottomChild* actor) final override {
    static_cast<TestMultiMgrsChild*>(Manager())->mBottomL = actor;
    return IPC_OK();
  }

  ~TestMultiMgrsLeftChild() = default;
};

class TestMultiMgrsRightParent : public PTestMultiMgrsRightParent {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsRightParent, override)
 public:
  bool HasChild(PTestMultiMgrsBottomParent* c) {
    const auto& managed = ManagedPTestMultiMgrsBottomParent();
    return std::find(managed.begin(), managed.end(), c) != managed.end();
  }

 private:
  ~TestMultiMgrsRightParent() = default;
};

class TestMultiMgrsRightChild : public PTestMultiMgrsRightChild {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsRightChild, override)
 public:
  bool HasChild(PTestMultiMgrsBottomChild* c) {
    const auto& managed = ManagedPTestMultiMgrsBottomChild();
    return std::find(managed.begin(), managed.end(), c) != managed.end();
  }

 private:
  already_AddRefed<PTestMultiMgrsBottomChild> AllocPTestMultiMgrsBottomChild()
      final override {
    return MakeAndAddRef<TestMultiMgrsBottomChild>();
  }

  IPCResult RecvPTestMultiMgrsBottomConstructor(
      PTestMultiMgrsBottomChild* actor) final override {
    static_cast<TestMultiMgrsChild*>(Manager())->mBottomR = actor;
    return IPC_OK();
  }

  ~TestMultiMgrsRightChild() = default;
};

class TestMultiMgrsParent : public PTestMultiMgrsParent {
  NS_INLINE_DECL_REFCOUNTING(TestMultiMgrsParent, override)
 private:
  IPCResult RecvOK() final override {
    Close();
    return IPC_OK();
  }

  ~TestMultiMgrsParent() = default;
};

already_AddRefed<PTestMultiMgrsLeftChild>
TestMultiMgrsChild::AllocPTestMultiMgrsLeftChild() {
  return MakeAndAddRef<TestMultiMgrsLeftChild>();
}

already_AddRefed<PTestMultiMgrsRightChild>
TestMultiMgrsChild::AllocPTestMultiMgrsRightChild() {
  return MakeAndAddRef<TestMultiMgrsRightChild>();
}

IPCResult TestMultiMgrsChild::RecvCheck() {
  EXPECT_EQ(ManagedPTestMultiMgrsLeftChild().Count(), (uint32_t)1)
      << "where's leftie?";
  EXPECT_EQ(ManagedPTestMultiMgrsRightChild().Count(), (uint32_t)1)
      << "where's rightie?";

  TestMultiMgrsLeftChild* leftie = static_cast<TestMultiMgrsLeftChild*>(
      LoneManagedOrNullAsserts(ManagedPTestMultiMgrsLeftChild()));
  TestMultiMgrsRightChild* rightie = static_cast<TestMultiMgrsRightChild*>(
      LoneManagedOrNullAsserts(ManagedPTestMultiMgrsRightChild()));

  EXPECT_TRUE(leftie->HasChild(mBottomL))
      << "leftie didn't have a child it was supposed to!";
  EXPECT_FALSE(leftie->HasChild(mBottomR)) << "leftie had rightie's child!";

  EXPECT_TRUE(rightie->HasChild(mBottomR))
      << "rightie didn't have a child it was supposed to!";
  EXPECT_FALSE(rightie->HasChild(mBottomL)) << "rightie had leftie's child!";

  EXPECT_TRUE(SendOK()) << "couldn't send OK()";

  return IPC_OK();
}

IPDL_TEST(TestMultiMgrs) {
  auto leftie = MakeRefPtr<TestMultiMgrsLeftParent>();
  EXPECT_TRUE(mActor->SendPTestMultiMgrsLeftConstructor(leftie))
      << "error sending ctor";

  auto rightie = MakeRefPtr<TestMultiMgrsRightParent>();
  EXPECT_TRUE(mActor->SendPTestMultiMgrsRightConstructor(rightie))
      << "error sending ctor";

  auto bottomL = MakeRefPtr<TestMultiMgrsBottomParent>();
  EXPECT_TRUE(leftie->SendPTestMultiMgrsBottomConstructor(bottomL))
      << "error sending ctor";

  auto bottomR = MakeRefPtr<TestMultiMgrsBottomParent>();
  EXPECT_TRUE(rightie->SendPTestMultiMgrsBottomConstructor(bottomR))
      << "error sending ctor";

  EXPECT_TRUE(leftie->HasChild(bottomL))
      << "leftie didn't have a child it was supposed to!";
  EXPECT_FALSE(leftie->HasChild(bottomR)) << "leftie had rightie's child!";

  EXPECT_TRUE(rightie->HasChild(bottomR))
      << "rightie didn't have a child it was supposed to!";
  EXPECT_FALSE(rightie->HasChild(bottomL)) << "rightie had rightie's child!";

  EXPECT_TRUE(mActor->SendCheck()) << "couldn't kick off the child-side check";
}

}  // namespace mozilla::_ipdltest
