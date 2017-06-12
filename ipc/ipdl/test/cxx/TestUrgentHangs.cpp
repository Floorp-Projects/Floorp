/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
#include "TestUrgentHangs.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "prthread.h"
#if defined(OS_POSIX)
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestUrgentHangsParent::TestUrgentHangsParent()
  : mInnerCount(0),
    mInnerUrgentCount(0)
{
    MOZ_COUNT_CTOR(TestUrgentHangsParent);
}

TestUrgentHangsParent::~TestUrgentHangsParent()
{
    MOZ_COUNT_DTOR(TestUrgentHangsParent);
}

void
TestUrgentHangsParent::Main()
{
    SetReplyTimeoutMs(1000);

    // Should succeed despite the nested sleep call because the content process
    // responded to the transaction.
    if (!SendTest1_1())
        fail("sending Test1_1");

    // Fails with a timeout.
    if (SendTest2())
        fail("sending Test2");

    // Also fails since we haven't gotten a response for Test2 yet.
    if (SendTest3())
        fail("sending Test3");

    // Do a second round of testing once the reply to Test2 comes back.
    MessageLoop::current()->PostDelayedTask(
      NewNonOwningRunnableMethod(
        "_ipdltest::TestUrgentHangsParent::SecondStage",
        this,
        &TestUrgentHangsParent::SecondStage),
      3000);
}

void
TestUrgentHangsParent::SecondStage()
{
    // Send an async message that waits 2 seconds and then sends a sync message
    // (which should be processed).
    if (!SendTest4())
        fail("sending Test4");

    // Send a sync message that will time out because the child is waiting
    // inside RecvTest4.
    if (SendTest4_1())
        fail("sending Test4_1");

    MessageLoop::current()->PostDelayedTask(
      NewNonOwningRunnableMethod("_ipdltest::TestUrgentHangsParent::ThirdStage",
                                 this,
                                 &TestUrgentHangsParent::ThirdStage),
      3000);
}

void
TestUrgentHangsParent::ThirdStage()
{
    // The third stage does the same thing as the second stage except that the
    // child sends an urgent message to us. In this case, we actually answer
    // that message unconditionally.

    // Send an async message that waits 2 seconds and then sends a sync message
    // (which should be processed).
    if (!SendTest5())
        fail("sending Test5");

    // Send a sync message that will time out because the child is waiting
    // inside RecvTest5.
    if (SendTest5_1())
        fail("sending Test5_1");

    // Close the channel after the child finishes its work in RecvTest5.
    MessageLoop::current()->PostDelayedTask(
      NewNonOwningRunnableMethod(
        "ipc::IToplevelProtocol::Close", this, &TestUrgentHangsParent::Close),
      3000);
}

mozilla::ipc::IPCResult
TestUrgentHangsParent::RecvTest1_2()
{
    if (!SendTest1_3())
        fail("sending Test1_3");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsParent::RecvTestInner()
{
    mInnerCount++;
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsParent::RecvTestInnerUrgent()
{
    mInnerUrgentCount++;
    return IPC_OK();
}

//-----------------------------------------------------------------------------
// child

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest1_1()
{
    if (!SendTest1_2())
        fail("sending Test1_2");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest1_3()
{
    PR_Sleep(PR_SecondsToInterval(2));

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest2()
{
    PR_Sleep(PR_SecondsToInterval(2));

    // Should fail because of the timeout.
    if (SendTestInner())
        fail("sending TestInner");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest3()
{
    fail("RecvTest3 should never be called");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest4()
{
    PR_Sleep(PR_SecondsToInterval(2));

    // This won't fail because we should handle Test4_1 here before actually
    // sending TestInner to the parent.
    if (!SendTestInner())
        fail("sending TestInner");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest4_1()
{
    // This should fail because Test4_1 timed out and hasn't gotten a response
    // yet.
    if (SendTestInner())
        fail("sending TestInner");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest5()
{
    PR_Sleep(PR_SecondsToInterval(2));

    // This message will actually be handled by the parent even though it's in
    // the timeout state.
    if (!SendTestInnerUrgent())
        fail("sending TestInner");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestUrgentHangsChild::RecvTest5_1()
{
    // This message will actually be handled by the parent even though it's in
    // the timeout state.
    if (!SendTestInnerUrgent())
        fail("sending TestInner");

    return IPC_OK();
}

TestUrgentHangsChild::TestUrgentHangsChild()
{
    MOZ_COUNT_CTOR(TestUrgentHangsChild);
}

TestUrgentHangsChild::~TestUrgentHangsChild()
{
    MOZ_COUNT_DTOR(TestUrgentHangsChild);
}

} // namespace _ipdltest
} // namespace mozilla
