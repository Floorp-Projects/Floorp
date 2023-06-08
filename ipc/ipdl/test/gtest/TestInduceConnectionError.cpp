/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestInduceConnectionErrorChild.h"
#include "mozilla/_ipdltest/PTestInduceConnectionErrorParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestInduceConnectionErrorChild : public PTestInduceConnectionErrorChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestInduceConnectionErrorChild,
                                        override)

 public:
  IPCResult RecvBegin() override {
    SendFirstMessage();
    SendFollowupMessage();
    Close();
    return IPC_OK();
  }

 private:
  ~TestInduceConnectionErrorChild() = default;
};

class TestInduceConnectionErrorParent
    : public PTestInduceConnectionErrorParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestInduceConnectionErrorParent,
                                        override)

  IPCResult RecvFirstMessage() override {
    EXPECT_TRUE(CanSend()) << "Actor still alive before inducing";
    GetIPCChannel()->InduceConnectionError();
    EXPECT_TRUE(CanSend())
        << "Actor still alive after inducing - notification will be async";
    mRecvdFirstMessage = true;

    GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
        "AfterRecvFirstMessage", [self = RefPtr{this}]() {
          EXPECT_FALSE(self->CanSend())
              << "Actor shut down after spinning the event loop";
          self->mTestComplete = true;
        }));
    return IPC_OK();
  }

  IPCResult RecvFollowupMessage() override {
    MOZ_CRASH(
        "Should never receive followup message, despite them being sent "
        "together");
  }

  void ActorDestroy(ActorDestroyReason aReason) override {
    EXPECT_TRUE(mRecvdFirstMessage)
        << "Should have mRecvdFirstMessage set before ActorDestroy";
    EXPECT_FALSE(mTestComplete) << "The test has not completed";
    EXPECT_EQ(aReason, ActorDestroyReason::AbnormalShutdown)
        << "Should be an abnormal shutdown";
    mDestroyReason = Some(aReason);
  }

  bool mRecvdFirstMessage = false;
  bool mTestComplete = false;
  Maybe<ActorDestroyReason> mDestroyReason;

 private:
  ~TestInduceConnectionErrorParent() = default;
};

IPDL_TEST(TestInduceConnectionError) {
  bool ok = mActor->SendBegin();
  GTEST_ASSERT_TRUE(ok);

  // Wait for the actor to be shut down.
  SpinEventLoopUntil("TestInduceConnectionError"_ns,
                     [&] { return mActor->mTestComplete; });
  EXPECT_TRUE(mActor->mRecvdFirstMessage)
      << "Actor should have received first message, but not second one";
  EXPECT_FALSE(mActor->CanSend()) << "Actor can no longer send";
  GTEST_ASSERT_TRUE(mActor->mDestroyReason.isSome())
      << "Actor should have been destroyed";
  EXPECT_EQ(*mActor->mDestroyReason,
            IProtocol::ActorDestroyReason::AbnormalShutdown)
      << "Actor should have an 'abnormal shutdown'";
}

}  // namespace mozilla::_ipdltest
