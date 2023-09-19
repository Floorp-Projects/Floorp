/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestCrossProcessSemaphoreChild.h"
#include "mozilla/_ipdltest/PTestCrossProcessSemaphoreParent.h"

#include "mozilla/ipc/CrossProcessSemaphore.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestCrossProcessSemaphoreChild : public PTestCrossProcessSemaphoreChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestCrossProcessSemaphoreChild,
                                        override)

 public:
  IPCResult RecvCrossProcessSemaphore(
      CrossProcessSemaphoreHandle&& aSem) override {
    UniquePtr<CrossProcessSemaphore> cps(
        CrossProcessSemaphore::Create(std::move(aSem)));
    EXPECT_TRUE(bool(cps));
    cps->Signal();
    Close();
    return IPC_OK();
  }

 private:
  ~TestCrossProcessSemaphoreChild() = default;
};

class TestCrossProcessSemaphoreParent
    : public PTestCrossProcessSemaphoreParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestCrossProcessSemaphoreParent,
                                        override)

 private:
  ~TestCrossProcessSemaphoreParent() = default;
};

IPDL_TEST(TestCrossProcessSemaphore) {
  // Create a semaphore with an initial count of 1. The test will then try to
  // send the semaphore over IPDL to the other end which will signal, increasing
  // the count to 2. The test will then try to wait on (decrement) the semaphore
  // twice, which should succeed only if the semaphore was properly signaled.
  UniquePtr<CrossProcessSemaphore> cps(
      CrossProcessSemaphore::Create("TestCrossProcessSemaphore", 1));
  ASSERT_TRUE(bool(cps));
  CrossProcessSemaphoreHandle handle = cps->CloneHandle();
  ASSERT_TRUE(bool(handle));
  bool ok = mActor->SendCrossProcessSemaphore(std::move(handle));
  ASSERT_TRUE(ok);
  EXPECT_TRUE(cps->Wait());
  EXPECT_TRUE(cps->Wait(Some(TimeDuration::FromSeconds(10))));
}

}  // namespace mozilla::_ipdltest
