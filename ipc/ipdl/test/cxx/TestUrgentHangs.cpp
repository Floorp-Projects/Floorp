/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
#include "TestUrgentHangs.h"

#include "IPDLUnitTests.h"      // fail etc.
#if defined(OS_POSIX)
#include <unistd.h>
#else
#include <windows.h>
#endif

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestUrgentHangsParent>
{
    static void RetainCallee(mozilla::_ipdltest::TestUrgentHangsParent* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestUrgentHangsParent* obj) { }
};

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestUrgentHangsParent::TestUrgentHangsParent()
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
        FROM_HERE,
        NewRunnableMethod(this, &TestUrgentHangsParent::FinishTesting),
        3000);
}

void
TestUrgentHangsParent::FinishTesting()
{
    // Send an async message that waits 2 seconds and then sends a sync message
    // (which should be processed).
    if (!SendTest4())
        fail("sending Test4");

    // Send a sync message that will time out because the child is waiting
    // inside RecvTest4.
    if (SendTest4_1())
        fail("sending Test4_1");

    // Close the channel after the child finishes its work in RecvTest4.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &TestUrgentHangsParent::Close),
        3000);
}

bool
TestUrgentHangsParent::RecvTest1_2()
{
    if (!SendTest1_3())
        fail("sending Test1_3");
    return true;
}

bool
TestUrgentHangsParent::RecvTestInner()
{
    fail("TestInner should never be dispatched");
    return true;
}

//-----------------------------------------------------------------------------
// child

bool
TestUrgentHangsChild::RecvTest1_1()
{
    if (!SendTest1_2())
        fail("sending Test1_2");

    return true;
}

bool
TestUrgentHangsChild::RecvTest1_3()
{
    sleep(2);

    return true;
}

bool
TestUrgentHangsChild::RecvTest2()
{
    sleep(2);

    // Should fail because of the timeout.
    if (SendTestInner())
        fail("sending TestInner");

    return true;
}

bool
TestUrgentHangsChild::RecvTest3()
{
    fail("RecvTest3 should never be called");
    return true;
}

bool
TestUrgentHangsChild::RecvTest4()
{
    sleep(2);

    // This should fail because Test4_1 timed out and hasn't gotten a response
    // yet.
    if (SendTestInner())
        fail("sending TestInner");

    return true;
}

bool
TestUrgentHangsChild::RecvTest4_1()
{
    // This should fail because Test4_1 timed out and hasn't gotten a response
    // yet.
    if (SendTestInner())
        fail("sending TestInner");

    return true;
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
