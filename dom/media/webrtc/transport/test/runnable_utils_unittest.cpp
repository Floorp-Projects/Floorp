/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <iostream>

#include "nsCOMPtr.h"
#include "nsNetCID.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;

namespace {

// Helper used to make sure args are properly copied and/or moved.
struct CtorDtorState {
  enum class State { Empty, Copy, Explicit, Move, Moved };

  static const char* ToStr(const State& state) {
    switch (state) {
      case State::Empty:
        return "empty";
      case State::Copy:
        return "copy";
      case State::Explicit:
        return "explicit";
      case State::Move:
        return "move";
      case State::Moved:
        return "moved";
      default:
        return "unknown";
    }
  }

  void DumpState() const { std::cerr << ToStr(state_) << std::endl; }

  CtorDtorState() { DumpState(); }

  explicit CtorDtorState(int* destroyed)
      : dtor_count_(destroyed), state_(State::Explicit) {
    DumpState();
  }

  CtorDtorState(const CtorDtorState& other)
      : dtor_count_(other.dtor_count_), state_(State::Copy) {
    DumpState();
  }

  // Clear the other's dtor counter so it's not counted if moved.
  CtorDtorState(CtorDtorState&& other)
      : dtor_count_(std::exchange(other.dtor_count_, nullptr)),
        state_(State::Move) {
    other.state_ = State::Moved;
    DumpState();
  }

  ~CtorDtorState() {
    const char* const state = ToStr(state_);
    std::cerr << "Destructor called with end state: " << state << std::endl;

    if (dtor_count_) {
      ++*dtor_count_;
    }
  }

  int* dtor_count_ = nullptr;
  State state_ = State::Empty;
};

class Destructor {
 private:
  ~Destructor() {
    std::cerr << "Destructor called" << std::endl;
    *destroyed_ = true;
  }

 public:
  explicit Destructor(bool* destroyed) : destroyed_(destroyed) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Destructor)

 private:
  bool* destroyed_;
};

class TargetClass {
 public:
  explicit TargetClass(int* ran) : ran_(ran) {}

  void m1(int x) {
    std::cerr << __FUNCTION__ << " " << x << std::endl;
    *ran_ = 1;
  }

  void m2(int x, int y) {
    std::cerr << __FUNCTION__ << " " << x << " " << y << std::endl;
    *ran_ = 2;
  }

  void m1set(bool* z) {
    std::cerr << __FUNCTION__ << std::endl;
    *z = true;
  }
  int return_int(int x) {
    std::cerr << __FUNCTION__ << std::endl;
    return x;
  }

  void destructor_target_ref(RefPtr<Destructor> destructor) {}

  int* ran_;
};

class RunnableArgsTest : public MtransportTest {
 public:
  RunnableArgsTest() : ran_(0), cl_(&ran_) {}

  void Test1Arg() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m1, 1);
    r->Run();
    ASSERT_EQ(1, ran_);
  }

  void Test2Args() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m2, 1, 2);
    r->Run();
    ASSERT_EQ(2, ran_);
  }

 private:
  int ran_;
  TargetClass cl_;
};

class DispatchTest : public MtransportTest {
 public:
  DispatchTest() : ran_(0), cl_(&ran_) {}

  void SetUp() {
    MtransportTest::SetUp();

    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  void Test1Arg() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m1, 1);
    NS_DispatchAndSpinEventLoopUntilComplete("DispatchTest::Test1Arg"_ns,
                                             target_, do_AddRef(r));
    ASSERT_EQ(1, ran_);
  }

  void Test2Args() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m2, 1, 2);
    NS_DispatchAndSpinEventLoopUntilComplete("DispatchTest::Test2Args"_ns,
                                             target_, do_AddRef(r));
    ASSERT_EQ(2, ran_);
  }

  void Test1Set() {
    bool x = false;
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::Test1Set"_ns, target_,
        do_AddRef(WrapRunnable(&cl_, &TargetClass::m1set, &x)));
    ASSERT_TRUE(x);
  }

  void TestRet() {
    int z;
    int x = 10;

    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestRet"_ns, target_,
        do_AddRef(WrapRunnableRet(&z, &cl_, &TargetClass::return_int, x)));
    ASSERT_EQ(10, z);
  }

 protected:
  int ran_;
  TargetClass cl_;
  nsCOMPtr<nsIEventTarget> target_;
};

TEST_F(RunnableArgsTest, OneArgument) { Test1Arg(); }

TEST_F(RunnableArgsTest, TwoArguments) { Test2Args(); }

TEST_F(DispatchTest, OneArgument) { Test1Arg(); }

TEST_F(DispatchTest, TwoArguments) { Test2Args(); }

TEST_F(DispatchTest, Test1Set) { Test1Set(); }

TEST_F(DispatchTest, TestRet) { TestRet(); }

void SetNonMethod(TargetClass* cl, int x) { cl->m1(x); }

int SetNonMethodRet(TargetClass* cl, int x) {
  cl->m1(x);

  return x;
}

TEST_F(DispatchTest, TestNonMethod) {
  test_utils_->SyncDispatchToSTS(WrapRunnableNM(SetNonMethod, &cl_, 10));

  ASSERT_EQ(1, ran_);
}

TEST_F(DispatchTest, TestNonMethodRet) {
  int z;

  test_utils_->SyncDispatchToSTS(
      WrapRunnableNMRet(&z, SetNonMethodRet, &cl_, 10));

  ASSERT_EQ(1, ran_);
  ASSERT_EQ(10, z);
}

TEST_F(DispatchTest, TestDestructorRef) {
  bool destroyed = false;
  {
    RefPtr<Destructor> destructor = new Destructor(&destroyed);
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestDestructorRef"_ns, target_,
        do_AddRef(WrapRunnable(&cl_, &TargetClass::destructor_target_ref,
                               destructor)));
    ASSERT_FALSE(destroyed);
  }
  ASSERT_TRUE(destroyed);

  // Now try with a move.
  destroyed = false;
  {
    RefPtr<Destructor> destructor = new Destructor(&destroyed);
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestDestructorRef"_ns, target_,
        do_AddRef(WrapRunnable(&cl_, &TargetClass::destructor_target_ref,
                               std::move(destructor))));
    ASSERT_TRUE(destroyed);
  }
}

TEST_F(DispatchTest, TestMove) {
  int destroyed = 0;
  {
    CtorDtorState state(&destroyed);

    // Dispatch with:
    //   - moved arg
    //   - by-val capture in function should consume a move
    //   - expect destruction in the function scope
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestMove"_ns, target_,
        do_AddRef(WrapRunnableNM([](CtorDtorState s) {}, std::move(state))));
    ASSERT_EQ(1, destroyed);
  }
  // Still shouldn't count when we go out of scope as it was moved.
  ASSERT_EQ(1, destroyed);

  {
    CtorDtorState state(&destroyed);

    // Dispatch with:
    //   - copied arg
    //   - by-val capture in function should consume a move
    //   - expect destruction in the function scope and call scope
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestMove"_ns, target_,
        do_AddRef(WrapRunnableNM([](CtorDtorState s) {}, state)));
    ASSERT_EQ(2, destroyed);
  }
  // Original state should be destroyed
  ASSERT_EQ(3, destroyed);

  {
    CtorDtorState state(&destroyed);

    // Dispatch with:
    //   - moved arg
    //   - by-ref in function should accept a moved arg
    //   - expect destruction in the wrapper invocation scope
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestMove"_ns, target_,
        do_AddRef(
            WrapRunnableNM([](const CtorDtorState& s) {}, std::move(state))));
    ASSERT_EQ(4, destroyed);
  }
  // Still shouldn't count when we go out of scope as it was moved.
  ASSERT_EQ(4, destroyed);

  {
    CtorDtorState state(&destroyed);

    // Dispatch with:
    //   - moved arg
    //   - r-value function should accept a moved arg
    //   - expect destruction in the wrapper invocation scope
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestMove"_ns, target_,
        do_AddRef(WrapRunnableNM([](CtorDtorState&& s) {}, std::move(state))));
    ASSERT_EQ(5, destroyed);
  }
  // Still shouldn't count when we go out of scope as it was moved.
  ASSERT_EQ(5, destroyed);
}

TEST_F(DispatchTest, TestUniquePtr) {
  // Test that holding the class in UniquePtr works
  int ran = 0;
  auto cl = MakeUnique<TargetClass>(&ran);

  NS_DispatchAndSpinEventLoopUntilComplete(
      "DispatchTest::TestUniquePtr"_ns, target_,
      do_AddRef(WrapRunnable(std::move(cl), &TargetClass::m1, 1)));
  ASSERT_EQ(1, ran);

  // Test that UniquePtr works as a param to the runnable
  int destroyed = 0;
  {
    auto state = MakeUnique<CtorDtorState>(&destroyed);

    // Dispatch with:
    //   - moved arg
    //   - Function should move construct from arg
    //   - expect destruction in the wrapper invocation scope
    NS_DispatchAndSpinEventLoopUntilComplete(
        "DispatchTest::TestUniquePtr"_ns, target_,
        do_AddRef(WrapRunnableNM([](UniquePtr<CtorDtorState> s) {},
                                 std::move(state))));
    ASSERT_EQ(1, destroyed);
  }
  // Still shouldn't count when we go out of scope as it was moved.
  ASSERT_EQ(1, destroyed);
}

}  // end of namespace
