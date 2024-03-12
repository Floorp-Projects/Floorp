/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * These tests ensure that async function return values work as expected.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestAsyncReturnsChild.h"
#include "mozilla/_ipdltest/PTestAsyncReturnsParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

static uint32_t sMagic1 = 0x105b59fb;
static uint32_t sMagic2 = 0x09b6f5e3;

class TestAsyncReturnsChild : public PTestAsyncReturnsChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestAsyncReturnsChild, override)
 private:
  IPCResult RecvPing(PingResolver&& resolve) final override {
    SendPong(
        [resolve](const std::tuple<uint32_t, uint32_t>& aParam) {
          EXPECT_EQ(std::get<0>(aParam), sMagic1);
          EXPECT_EQ(std::get<1>(aParam), sMagic2);
          resolve(true);
        },
        [](ResponseRejectReason&& aReason) { FAIL() << "sending Pong"; });
    return IPC_OK();
  }

  IPCResult RecvNoReturn(NoReturnResolver&& resolve) final override {
    // Not calling `resolve` intentionally
    return IPC_OK();
  }

  ~TestAsyncReturnsChild() = default;
};

class TestAsyncReturnsParent : public PTestAsyncReturnsParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestAsyncReturnsParent, override)
 private:
  IPCResult RecvPong(PongResolver&& resolve) final override {
    resolve(std::tuple<const uint32_t&, const uint32_t&>(sMagic1, sMagic2));
    return IPC_OK();
  }

  ~TestAsyncReturnsParent() = default;
};

IPDL_TEST(TestAsyncReturns, NoReturn) {
  mActor->SendNoReturn(
      [](bool unused) { FAIL() << "resolve handler should not be called"; },
      [=](ResponseRejectReason&& aReason) {
        if (aReason != ResponseRejectReason::ResolverDestroyed) {
          FAIL() << "reject with wrong reason";
        }
        mActor->Close();
      });
}

IPDL_TEST(TestAsyncReturns, PingPong) {
  mActor->SendPing(
      [=](bool one) {
        EXPECT_TRUE(one);
        mActor->Close();
      },
      [](ResponseRejectReason&& aReason) { FAIL() << "sending Ping"; });
}

}  // namespace mozilla::_ipdltest
