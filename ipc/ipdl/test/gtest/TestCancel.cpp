/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test that IPC channel transaction cancellation (which applies to nested sync
 * messages) works as expected.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestCancelChild.h"
#include "mozilla/_ipdltest/PTestCancelParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestCancelParent : public PTestCancelParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestCancelParent, override)
 private:
  IPCResult RecvCallNestedCancel() final override {
    EXPECT_FALSE(SendNestedCancel()) << "SendNestedCancel should fail";
    EXPECT_EQ(GetIPCChannel()->LastSendError(),
              SyncSendError::CancelledAfterSend)
        << "SendNestedCancel should be cancelled";

    return IPC_OK();
  }

  IPCResult RecvNestedCancelParent() final override {
    GetIPCChannel()->CancelCurrentTransaction();
    return IPC_OK();
  }

  IPCResult RecvCheckParent(uint32_t* reply) final override {
    *reply = 42;
    return IPC_OK();
  }

  ~TestCancelParent() = default;
};

class TestCancelChild : public PTestCancelChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestCancelChild, override)
 private:
  IPCResult RecvImmediateCancel() final override {
    GetIPCChannel()->CancelCurrentTransaction();

    uint32_t value = 0;
    EXPECT_FALSE(SendCheckParent(&value)) << "channel should be closing";

    return IPC_OK();
  }

  IPCResult RecvStartNestedCancel() final override {
    EXPECT_FALSE(SendCallNestedCancel());

    Close();
    return IPC_OK();
  }

  IPCResult RecvNestedCancel() final override {
    GetIPCChannel()->CancelCurrentTransaction();

    uint32_t value = 0;
    EXPECT_TRUE(SendCheckParent(&value)) << "channel should be closing";

    return IPC_OK();
  }

  IPCResult RecvStartNestedCancelParent() final override {
    EXPECT_FALSE(SendNestedCancelParent())
        << "SendNestedCancelParent should fail";
    EXPECT_EQ(GetIPCChannel()->LastSendError(),
              SyncSendError::CancelledAfterSend)
        << "SendNestedCancelParent should be cancelled";

    uint32_t value = 0;
    EXPECT_FALSE(SendCheckParent(&value));

    return IPC_OK();
  }

  IPCResult RecvCheckChild(uint32_t* reply) final override {
    *reply = 42;
    return IPC_OK();
  }

  ~TestCancelChild() = default;
};

// Nested sync messages can only be received on the main thread, so threaded
// tests can't be run (the child actor won't be on the main thread).

IPDL_TEST_ON(CROSSPROCESS, TestCancel, ImmediateCancel) {
  EXPECT_FALSE(mActor->SendImmediateCancel()) << "should immediately cancel";
  EXPECT_EQ(mActor->GetIPCChannel()->LastSendError(),
            SyncSendError::CancelledAfterSend);

  uint32_t value = 0;
  EXPECT_TRUE(mActor->SendCheckChild(&value));
  EXPECT_EQ(value, (uint32_t)42);

  mActor->Close();
}

IPDL_TEST_ON(CROSSPROCESS, TestCancel, NestedCancel) {
  EXPECT_TRUE(mActor->SendStartNestedCancel());
}

IPDL_TEST_ON(CROSSPROCESS, TestCancel, NestedCancelParent) {
  EXPECT_FALSE(mActor->SendStartNestedCancelParent())
      << "StartNestedCancelParent should be cancelled";

  uint32_t value = 0;
  EXPECT_TRUE(mActor->SendCheckChild(&value));
  EXPECT_EQ(value, (uint32_t)42);

  mActor->Close();
}

}  // namespace mozilla::_ipdltest
