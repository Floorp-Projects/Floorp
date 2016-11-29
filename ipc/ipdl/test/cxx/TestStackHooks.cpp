#include "TestStackHooks.h"

#include "base/task.h"
#include "IPDLUnitTests.h"      // fail etc.



namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestStackHooksParent::TestStackHooksParent() :
    mOnStack(false), mIncallDepth(0)
{
    MOZ_COUNT_CTOR(TestStackHooksParent);
}

TestStackHooksParent::~TestStackHooksParent()
{
    MOZ_COUNT_DTOR(TestStackHooksParent);
}

void
TestStackHooksParent::Main()
{
    if (!SendStart())
        fail("sending Start()");
}


mozilla::ipc::IPCResult
TestStackHooksParent::AnswerStackFrame()
{
    if (!mOnStack)
        fail("not on C++ stack?!");

    if (!CallStackFrame())
        fail("calling StackFrame()");

    if (!mOnStack)
        fail("not on C++ stack?!");

    if (1 != mIncallDepth)
        fail("missed EnteredCall or ExitedCall hook");

    return IPC_OK();
}

//-----------------------------------------------------------------------------
// child

TestStackHooksChild::TestStackHooksChild() :
    mOnStack(false),
    mEntered(0),
    mExited(0),
    mIncallDepth(0),
    mNumAnswerStackFrame(0)
{
    MOZ_COUNT_CTOR(TestStackHooksChild);
}

TestStackHooksChild::~TestStackHooksChild()
{
    MOZ_COUNT_DTOR(TestStackHooksChild);
}

namespace {
void RunTestsFn() {
    static_cast<TestStackHooksChild*>(gChildActor)->RunTests();
}
}

mozilla::ipc::IPCResult
TestStackHooksChild::RecvStart()
{
    if (!mOnStack)
        fail("missed stack notification");

    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");

    // kick off tests from a runnable so that we can start with
    // MessageChannel code on the C++ stack
    MessageLoop::current()->PostTask(NewRunnableFunction(RunTestsFn));

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestStackHooksChild::AnswerStackFrame()
{
    ++mNumAnswerStackFrame;

    if (!mOnStack)
        fail("missed stack notification");

    if (1 != mIncallDepth)
        fail("missed EnteredCall or ExitedCall hook");

    if (mNumAnswerStackFrame == 1) {
        // XXX This assertion will be deleted as part of bug 1316757.
        MOZ_ASSERT(PTestStackHooks::TEST4_3 == state());
        if (!SendAsync())
            fail("sending Async()");
    } else if (mNumAnswerStackFrame == 2) {
        // XXX This assertion will be deleted as part of bug 1316757.
        MOZ_ASSERT(PTestStackHooks::TEST5_3 == state());
        if (!SendSync())
            fail("sending Sync()");
    } else {
        fail("unexpected state");
    }

    if (!mOnStack)
        fail("bad stack exit notification");

    return IPC_OK();
}

void
TestStackHooksChild::RunTests()
{
    // 1 because of RecvStart()
    if (1 != mEntered)
        fail("missed stack notification");
    if (mOnStack)
        fail("spurious stack notification");
    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");

    if (!SendAsync())
        fail("sending Async()");
    if (mOnStack)
        fail("spurious stack notification");
    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");
    if (2 != mEntered)
        fail("missed stack notification");

    if (!SendSync())
        fail("sending Sync()");
    if (mOnStack)
        fail("spurious stack notification");
    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");
    if (3 != mEntered)
        fail("missed stack notification");

    if (!CallRpc())
        fail("calling RPC()");
    if (mOnStack)
        fail("spurious stack notification");
    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");
    if (4 != mEntered)
        fail("missed stack notification");

    if (!CallStackFrame())
        fail("calling StackFrame()");
    if (mOnStack)
        fail("spurious stack notification");
    if (0 != mIncallDepth)
        fail("EnteredCall/ExitedCall malfunction");
    if (5 != mEntered)
        fail("missed stack notification");

    Close();
}

} // namespace _ipdltest
} // namespace mozilla
