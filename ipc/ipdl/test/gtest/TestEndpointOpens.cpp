/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test Endpoint usage.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestEndpointOpensChild.h"
#include "mozilla/_ipdltest/PTestEndpointOpensParent.h"
#include "mozilla/_ipdltest/PTestEndpointOpensOpenedChild.h"
#include "mozilla/_ipdltest/PTestEndpointOpensOpenedParent.h"

#include <memory>

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

class TestEndpointOpensOpenedParent : public PTestEndpointOpensOpenedParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestEndpointOpensOpenedParent, override)
 private:
  IPCResult RecvHello() final override {
    EXPECT_FALSE(NS_IsMainThread());
    if (!SendHi()) {
      return IPC_TEST_FAIL(this);
    }
    return IPC_OK();
  }

  IPCResult RecvHelloSync() final override {
    EXPECT_FALSE(NS_IsMainThread());
    return IPC_OK();
  }

  void ActorDestroy(ActorDestroyReason why) final override {
    EXPECT_FALSE(NS_IsMainThread());
  }

  ~TestEndpointOpensOpenedParent() = default;
};

class TestEndpointOpensChild : public PTestEndpointOpensChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestEndpointOpensChild, override)
 private:
  IPCResult RecvStart() final override;

  ~TestEndpointOpensChild() = default;
};

class TestEndpointOpensOpenedChild : public PTestEndpointOpensOpenedChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestEndpointOpensOpenedChild, override)

  explicit TestEndpointOpensOpenedChild(TestEndpointOpensChild* opensChild)
      : mOpensChild(opensChild) {}

 private:
  IPCResult RecvHi() final override {
    EXPECT_FALSE(NS_IsMainThread());

    EXPECT_TRUE(SendHelloSync());

    Close();
    return IPC_OK();
  }

  void ActorDestroy(ActorDestroyReason why) final override {
    EXPECT_FALSE(NS_IsMainThread());

    // Kick off main-thread shutdown.
    NS_DispatchToMainThread(NewRunnableMethod("ipc::IToplevelProtocol::Close",
                                              mOpensChild,
                                              &TestEndpointOpensChild::Close));
  }

  ~TestEndpointOpensOpenedChild() = default;

  TestEndpointOpensChild* mOpensChild;
};

class TestEndpointOpensParent : public PTestEndpointOpensParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestEndpointOpensParent, override)
 private:
  IPCResult RecvStartSubprotocol(
      Endpoint<PTestEndpointOpensOpenedParent>&& endpoint) final override {
    nsCOMPtr<nsISerialEventTarget> eventTarget;
    auto rv = NS_CreateBackgroundTaskQueue("ParentThread",
                                           getter_AddRefs(eventTarget));
    if (NS_FAILED(rv)) {
      ADD_FAILURE() << "creating background task queue for child";
      return IPC_TEST_FAIL(this);
    }

    eventTarget->Dispatch(NS_NewRunnableFunction(
        "OpenParent", [endpoint{std::move(endpoint)}]() mutable {
          EXPECT_FALSE(NS_IsMainThread());

          // Open the actor on the off-main thread to park it there.
          // Messages will be delivered to this thread's message loop
          // instead of the main thread's.
          auto actor = MakeRefPtr<TestEndpointOpensOpenedParent>();
          ASSERT_TRUE(endpoint.Bind(actor));
        }));

    return IPC_OK();
  }

  ~TestEndpointOpensParent() = default;
};

IPCResult TestEndpointOpensChild::RecvStart() {
  Endpoint<PTestEndpointOpensOpenedParent> parent;
  Endpoint<PTestEndpointOpensOpenedChild> child;
  nsresult rv;
  rv = PTestEndpointOpensOpened::CreateEndpoints(
      OtherPidMaybeInvalid(), base::GetCurrentProcId(), &parent, &child);
  if (NS_FAILED(rv)) {
    ADD_FAILURE() << "opening PTestEndpointOpensOpened";
    return IPC_TEST_FAIL(this);
  }

  nsCOMPtr<nsISerialEventTarget> childEventTarget;
  rv = NS_CreateBackgroundTaskQueue("ChildThread",
                                    getter_AddRefs(childEventTarget));
  if (NS_FAILED(rv)) {
    ADD_FAILURE() << "creating background task queue for child";
    return IPC_TEST_FAIL(this);
  }

  auto actor = MakeRefPtr<TestEndpointOpensOpenedChild>(this);
  childEventTarget->Dispatch(NS_NewRunnableFunction(
      "OpenChild",
      [actor{std::move(actor)}, endpoint{std::move(child)}]() mutable {
        EXPECT_FALSE(NS_IsMainThread());

        // Open the actor on the off-main thread to park it there.
        // Messages will be delivered to this thread's message loop
        // instead of the main thread's.
        ASSERT_TRUE(endpoint.Bind(actor));

        // Kick off the unit tests
        ASSERT_TRUE(actor->SendHello());
      }));

  EXPECT_TRUE(SendStartSubprotocol(std::move(parent)));

  return IPC_OK();
}

IPDL_TEST_ON(CROSSPROCESS, TestEndpointOpens) {
  EXPECT_TRUE(mActor->SendStart());
}

}  // namespace mozilla::_ipdltest
