/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "base/task.h"
#include "base/thread.h"

#include "TestEndpointOpens.h"

#include "IPDLUnitTests.h"  // fail etc.

using namespace mozilla::ipc;

using base::ProcessHandle;
using base::Thread;

namespace mozilla {
// NB: this is generally bad style, but I am lazy.
using namespace _ipdltest;
using namespace _ipdltest2;

static MessageLoop* gMainThread;

static void AssertNotMainThread() {
  if (!gMainThread) {
    fail("gMainThread is not initialized");
  }
  if (MessageLoop::current() == gMainThread) {
    fail("unexpectedly called on the main thread");
  }
}

//-----------------------------------------------------------------------------
// parent

// Thread on which TestEndpointOpensOpenedParent runs
static Thread* gParentThread;

void TestEndpointOpensParent::Main() {
  if (!SendStart()) {
    fail("sending Start");
  }
}

static void OpenParent(TestEndpointOpensOpenedParent* aParent,
                       Endpoint<PTestEndpointOpensOpenedParent>&& aEndpoint) {
  AssertNotMainThread();

  // Open the actor on the off-main thread to park it there.
  // Messages will be delivered to this thread's message loop
  // instead of the main thread's.
  if (!aEndpoint.Bind(aParent)) {
    fail("binding Parent");
  }
}

mozilla::ipc::IPCResult TestEndpointOpensParent::RecvStartSubprotocol(
    mozilla::ipc::Endpoint<PTestEndpointOpensOpenedParent>&& endpoint) {
  gMainThread = MessageLoop::current();

  gParentThread = new Thread("ParentThread");
  if (!gParentThread->Start()) {
    fail("starting parent thread");
  }

  TestEndpointOpensOpenedParent* a = new TestEndpointOpensOpenedParent();
  gParentThread->message_loop()->PostTask(
      NewRunnableFunction("OpenParent", OpenParent, a, std::move(endpoint)));

  return IPC_OK();
}

void TestEndpointOpensParent::ActorDestroy(ActorDestroyReason why) {
  // Stops the thread and joins it
  delete gParentThread;

  if (NormalShutdown != why) {
    fail("unexpected destruction A!");
  }
  passed("ok");
  QuitParent();
}

mozilla::ipc::IPCResult TestEndpointOpensOpenedParent::RecvHello() {
  AssertNotMainThread();
  if (!SendHi()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult TestEndpointOpensOpenedParent::RecvHelloSync() {
  AssertNotMainThread();
  return IPC_OK();
}

mozilla::ipc::IPCResult TestEndpointOpensOpenedParent::AnswerHelloRpc() {
  AssertNotMainThread();
  if (!CallHiRpc()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

static void ShutdownTestEndpointOpensOpenedParent(
    TestEndpointOpensOpenedParent* parent, Transport* transport) {
  delete parent;
}

void TestEndpointOpensOpenedParent::ActorDestroy(ActorDestroyReason why) {
  AssertNotMainThread();

  if (NormalShutdown != why) {
    fail("unexpected destruction B!");
  }

  // ActorDestroy() is just a callback from IPDL-generated code,
  // which needs the top-level actor (this) to stay alive a little
  // longer so other things can be cleaned up.
  gParentThread->message_loop()->PostTask(NewRunnableFunction(
      "ShutdownTestEndpointOpensOpenedParent",
      ShutdownTestEndpointOpensOpenedParent, this, GetTransport()));
}

//-----------------------------------------------------------------------------
// child

static TestEndpointOpensChild* gOpensChild;
// Thread on which TestEndpointOpensOpenedChild runs
static Thread* gChildThread;

TestEndpointOpensChild::TestEndpointOpensChild() { gOpensChild = this; }

static void OpenChild(TestEndpointOpensOpenedChild* aChild,
                      Endpoint<PTestEndpointOpensOpenedChild>&& endpoint) {
  AssertNotMainThread();

  // Open the actor on the off-main thread to park it there.
  // Messages will be delivered to this thread's message loop
  // instead of the main thread's.
  if (!endpoint.Bind(aChild)) {
    fail("binding child endpoint");
  }

  // Kick off the unit tests
  if (!aChild->SendHello()) {
    fail("sending Hello");
  }
}

mozilla::ipc::IPCResult TestEndpointOpensChild::RecvStart() {
  Endpoint<PTestEndpointOpensOpenedParent> parent;
  Endpoint<PTestEndpointOpensOpenedChild> child;
  nsresult rv;
  rv = PTestEndpointOpensOpened::CreateEndpoints(
      OtherPid(), base::GetCurrentProcId(), &parent, &child);
  if (NS_FAILED(rv)) {
    fail("opening PTestEndpointOpensOpened");
  }

  gMainThread = MessageLoop::current();

  gChildThread = new Thread("ChildThread");
  if (!gChildThread->Start()) {
    fail("starting child thread");
  }

  TestEndpointOpensOpenedChild* a = new TestEndpointOpensOpenedChild();
  gChildThread->message_loop()->PostTask(
      NewRunnableFunction("OpenChild", OpenChild, a, std::move(child)));

  if (!SendStartSubprotocol(std::move(parent))) {
    fail("send StartSubprotocol");
  }

  return IPC_OK();
}

void TestEndpointOpensChild::ActorDestroy(ActorDestroyReason why) {
  // Stops the thread and joins it
  delete gChildThread;

  if (NormalShutdown != why) {
    fail("unexpected destruction C!");
  }
  QuitChild();
}

mozilla::ipc::IPCResult TestEndpointOpensOpenedChild::RecvHi() {
  AssertNotMainThread();

  if (!SendHelloSync()) {
    fail("sending HelloSync");
  }
  if (!CallHelloRpc()) {
    fail("calling HelloRpc");
  }
  if (!mGotHi) {
    fail("didn't answer HiRpc");
  }

  // Need to close the channel without message-processing frames on
  // the C++ stack
  MessageLoop::current()->PostTask(
      NewNonOwningRunnableMethod("ipc::IToplevelProtocol::Close", this,
                                 &TestEndpointOpensOpenedChild::Close));
  return IPC_OK();
}

mozilla::ipc::IPCResult TestEndpointOpensOpenedChild::AnswerHiRpc() {
  AssertNotMainThread();

  mGotHi = true;  // d00d
  return IPC_OK();
}

static void ShutdownTestEndpointOpensOpenedChild(
    TestEndpointOpensOpenedChild* child, Transport* transport) {
  delete child;

  // Kick off main-thread shutdown.
  gMainThread->PostTask(
      NewNonOwningRunnableMethod("ipc::IToplevelProtocol::Close", gOpensChild,
                                 &TestEndpointOpensChild::Close));
}

void TestEndpointOpensOpenedChild::ActorDestroy(ActorDestroyReason why) {
  AssertNotMainThread();

  if (NormalShutdown != why) {
    fail("unexpected destruction D!");
  }

  // ActorDestroy() is just a callback from IPDL-generated code,
  // which needs the top-level actor (this) to stay alive a little
  // longer so other things can be cleaned up.  Defer shutdown to
  // let cleanup finish.
  gChildThread->message_loop()->PostTask(NewRunnableFunction(
      "ShutdownTestEndpointOpensOpenedChild",
      ShutdownTestEndpointOpensOpenedChild, this, GetTransport()));
}

}  // namespace mozilla
