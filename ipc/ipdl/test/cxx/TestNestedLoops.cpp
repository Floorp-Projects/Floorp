#include "base/basictypes.h"

#include "nsThreadUtils.h"

#include "TestNestedLoops.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestNestedLoopsParent::TestNestedLoopsParent() : mBreakNestedLoop(false)
{
    MOZ_COUNT_CTOR(TestNestedLoopsParent);
}

TestNestedLoopsParent::~TestNestedLoopsParent()
{
    MOZ_COUNT_DTOR(TestNestedLoopsParent);
}

void
TestNestedLoopsParent::Main()
{
    if (!SendStart())
        fail("sending Start");

    // sigh ... spin for a while to let Nonce arrive
    puts(" (sleeping to wait for nonce ... sorry)");
    PR_Sleep(5000);

    // while waiting for the reply to R, we'll receive Nonce
    if (!CallR())
        fail("calling R");

    Close();
}

mozilla::ipc::IPCResult
TestNestedLoopsParent::RecvNonce()
{
    // if we have an OnMaybeDequeueOne waiting for us (we may not, due
    // to the inherent race condition in this test, then this event
    // must be ordered after it in the queue
    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestNestedLoopsParent::BreakNestedLoop));

    // sigh ... spin for a while to let the reply to R arrive
    puts(" (sleeping to wait for reply to R ... sorry)");
    PR_Sleep(5000);

    // sigh ... we have no idea when code might do this
    do {
        if (!NS_ProcessNextEvent(nullptr, false))
            fail("expected at least one pending event");
    } while (!mBreakNestedLoop);

    return IPC_OK();
}

void
TestNestedLoopsParent::BreakNestedLoop()
{
    mBreakNestedLoop = true;
}

//-----------------------------------------------------------------------------
// child

TestNestedLoopsChild::TestNestedLoopsChild()
{
    MOZ_COUNT_CTOR(TestNestedLoopsChild);
}

TestNestedLoopsChild::~TestNestedLoopsChild()
{
    MOZ_COUNT_DTOR(TestNestedLoopsChild);
}

mozilla::ipc::IPCResult
TestNestedLoopsChild::RecvStart()
{
    if (!SendNonce())
        fail("sending Nonce");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestNestedLoopsChild::AnswerR()
{
    return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
