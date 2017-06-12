/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#include "TestEndpointBridgeMain.h"

#include "base/task.h"
#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

using namespace std;

namespace mozilla {
namespace _ipdltest {


//-----------------------------------------------------------------------------
// main process
void
TestEndpointBridgeMainParent::Main()
{
  if (!SendStart()) {
    fail("sending Start");
  }
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainParent::RecvBridged(Endpoint<PTestEndpointBridgeMainSubParent>&& endpoint)
{
  TestEndpointBridgeMainSubParent* a = new TestEndpointBridgeMainSubParent();
  if (!endpoint.Bind(a)) {
    fail("Bind failed");
  }
  return IPC_OK();
}

void
TestEndpointBridgeMainParent::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }
  passed("ok");
  QuitParent();
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainSubParent::RecvHello()
{
  if (!SendHi()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainSubParent::RecvHelloSync()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainSubParent::AnswerHelloRpc()
{
  if (!CallHiRpc()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

void
TestEndpointBridgeMainSubParent::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }

  // ActorDestroy() is just a callback from IPDL-generated code,
  // which needs the top-level actor (this) to stay alive a little
  // longer so other things can be cleaned up.
  MessageLoop::current()->PostTask(
    do_AddRef(new DeleteTask<TestEndpointBridgeMainSubParent>(this)));
}

//-----------------------------------------------------------------------------
// sub process --- child of main
TestEndpointBridgeMainChild* gEndpointBridgeMainChild;

TestEndpointBridgeMainChild::TestEndpointBridgeMainChild()
 : mSubprocess(nullptr)
{
  gEndpointBridgeMainChild = this;
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainChild::RecvStart()
{
  vector<string> subsubArgs;
  subsubArgs.push_back("TestEndpointBridgeSub");

  mSubprocess = new IPDLUnitTestSubprocess();
  if (!mSubprocess->SyncLaunch(subsubArgs)) {
    fail("problem launching subprocess");
  }

  IPC::Channel* transport = mSubprocess->GetChannel();
  if (!transport) {
    fail("no transport");
  }

  TestEndpointBridgeSubParent* bsp = new TestEndpointBridgeSubParent();
  bsp->Open(transport, base::GetProcId(mSubprocess->GetChildProcessHandle()));

  bsp->Main();
  return IPC_OK();
}

void
TestEndpointBridgeMainChild::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }
  // NB: this is kosher because QuitChild() joins with the IO thread
  XRE_GetIOMessageLoop()->PostTask(
    do_AddRef(new DeleteTask<IPDLUnitTestSubprocess>(mSubprocess)));
  QuitChild();
}

void
TestEndpointBridgeSubParent::Main()
{
  if (!SendPing()) {
    fail("sending Ping");
  }
}

mozilla::ipc::IPCResult
TestEndpointBridgeSubParent::RecvBridgeEm()
{
  Endpoint<PTestEndpointBridgeMainSubParent> parent;
  Endpoint<PTestEndpointBridgeMainSubChild> child;
  nsresult rv;
  rv = PTestEndpointBridgeMainSub::CreateEndpoints(
    gEndpointBridgeMainChild->OtherPid(), OtherPid(),
    &parent, &child);
  if (NS_FAILED(rv)) {
    fail("opening PTestEndpointOpensOpened");
  }

  if (!gEndpointBridgeMainChild->SendBridged(mozilla::Move(parent))) {
    fail("SendBridge failed for parent");
  }
  if (!SendBridged(mozilla::Move(child))) {
    fail("SendBridge failed for child");
  }

  return IPC_OK();
}

void
TestEndpointBridgeSubParent::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }
  gEndpointBridgeMainChild->Close();

  // ActorDestroy() is just a callback from IPDL-generated code,
  // which needs the top-level actor (this) to stay alive a little
  // longer so other things can be cleaned up.
  MessageLoop::current()->PostTask(
    do_AddRef(new DeleteTask<TestEndpointBridgeSubParent>(this)));
}

//-----------------------------------------------------------------------------
// subsub process --- child of sub

static TestEndpointBridgeSubChild* gBridgeSubChild;

TestEndpointBridgeSubChild::TestEndpointBridgeSubChild()
{
    gBridgeSubChild = this;
}

mozilla::ipc::IPCResult
TestEndpointBridgeSubChild::RecvPing()
{
  if (!SendBridgeEm()) {
    fail("sending BridgeEm");
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestEndpointBridgeSubChild::RecvBridged(Endpoint<PTestEndpointBridgeMainSubChild>&& endpoint)
{
  TestEndpointBridgeMainSubChild* a = new TestEndpointBridgeMainSubChild();

  if (!endpoint.Bind(a)) {
    fail("failed to Bind");
  }

  if (!a->SendHello()) {
    fail("sending Hello");
  }

  return IPC_OK();
}

void
TestEndpointBridgeSubChild::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }
  QuitChild();
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainSubChild::RecvHi()
{
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
    NewNonOwningRunnableMethod("ipc::IToplevelProtocol::Close",
                               this,
                               &TestEndpointBridgeMainSubChild::Close));
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestEndpointBridgeMainSubChild::AnswerHiRpc()
{
  mGotHi = true;              // d00d
  return IPC_OK();
}

void
TestEndpointBridgeMainSubChild::ActorDestroy(ActorDestroyReason why)
{
  if (NormalShutdown != why) {
    fail("unexpected destruction!");
  }

  gBridgeSubChild->Close();

  // ActorDestroy() is just a callback from IPDL-generated code,
  // which needs the top-level actor (this) to stay alive a little
  // longer so other things can be cleaned up.
  MessageLoop::current()->PostTask(
    do_AddRef(new DeleteTask<TestEndpointBridgeMainSubChild>(this)));
}

} // namespace mozilla
} // namespace _ipdltest
