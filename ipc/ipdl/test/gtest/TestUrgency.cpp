/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test that sync messages preempt other messages in the expected way.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestUrgencyChild.h"
#include "mozilla/_ipdltest/PTestUrgencyParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

enum {
  kFirstTestBegin = 1,
  kFirstTestGotReply,
  kSecondTestBegin,
  kSecondTestGotReply,
};

class TestUrgencyParent : public PTestUrgencyParent {
  NS_INLINE_DECL_REFCOUNTING(TestUrgencyParent, override)
 private:
  IPCResult RecvTest1(uint32_t* value) final override {
    EXPECT_TRUE(SendReply1(value));
    EXPECT_EQ(*value, (uint32_t)99) << "unexpected value";
    return IPC_OK();
  }

  IPCResult RecvTest2() final override {
    uint32_t value;
    inreply_ = true;
    EXPECT_TRUE(SendReply2(&value));
    inreply_ = false;
    EXPECT_EQ(value, (uint32_t)500) << "unexpected value";
    return IPC_OK();
  }

  IPCResult RecvTest3(uint32_t* value) final override {
    EXPECT_FALSE(inreply_) << "nested non-urgent on top of urgent message";
    *value = 1000;
    return IPC_OK();
  }

  IPCResult RecvFinalTest_Begin() final override { return IPC_OK(); }

  ~TestUrgencyParent() = default;

  bool inreply_ = false;
};

class TestUrgencyChild : public PTestUrgencyChild {
  NS_INLINE_DECL_REFCOUNTING(TestUrgencyChild, override)
 private:
  IPCResult RecvStart() final override {
    uint32_t result;

    // Send a synchronous message, expect to get an urgent message while
    // blocked.
    test_ = kFirstTestBegin;
    EXPECT_TRUE(SendTest1(&result));
    EXPECT_EQ(result, (uint32_t)99) << "wrong value from SendTest1";
    EXPECT_EQ(test_, kFirstTestGotReply)
        << "never received first urgent message";

    // Initiate the next test by sending an asynchronous message, then becoming
    // blocked. This tests that the urgent message is still delivered properly,
    // and that the parent does not try to service the sync
    test_ = kSecondTestBegin;
    EXPECT_TRUE(SendTest2());
    EXPECT_TRUE(SendTest3(&result));
    EXPECT_EQ(test_, kSecondTestGotReply)
        << "never received second urgent message";
    EXPECT_EQ(result, (uint32_t)1000) << "wrong value from SendTest3";

    EXPECT_TRUE(SendFinalTest_Begin());

    Close();

    return IPC_OK();
  }

  IPCResult RecvReply1(uint32_t* reply) final override {
    EXPECT_EQ(test_, kFirstTestBegin) << "wrong test state in RecvReply1";

    *reply = 99;
    test_ = kFirstTestGotReply;
    return IPC_OK();
  }

  IPCResult RecvReply2(uint32_t* reply) final override {
    EXPECT_EQ(test_, kSecondTestBegin) << "wrong test state in RecvReply2";

    *reply = 500;
    test_ = kSecondTestGotReply;
    return IPC_OK();
  }

  ~TestUrgencyChild() = default;

  uint32_t test_ = 0;
};

// Only run cross-process because we need to send nested sync messages (this can
// only be done from the main thread).
IPDL_TEST_ON(CROSSPROCESS, TestUrgency) { EXPECT_TRUE(mActor->SendStart()); }

}  // namespace mozilla::_ipdltest
