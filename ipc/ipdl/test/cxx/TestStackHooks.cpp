#include "TestStackHooks.h"

#include "IPDLUnitTests.h"      // fail etc.



namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestStackHooksParent::TestStackHooksParent() : mOnStack(false)
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


bool
TestStackHooksParent::AnswerStackFrame()
{
    if (!mOnStack)
        fail("not on C++ stack?!");

    if (!CallStackFrame())
        fail("calling StackFrame()");

    if (!mOnStack)
        fail("not on C++ stack?!");

    return true;
}

//-----------------------------------------------------------------------------
// child

TestStackHooksChild::TestStackHooksChild() :
    mOnStack(false),
    mEntered(0),
    mExited(0)
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

bool
TestStackHooksChild::RecvStart()
{
    if (!mOnStack)
        fail("missed stack notification");

    // kick off tests from a runnable so that we can start with
    // RPCChannel code on the C++ stack
    MessageLoop::current()->PostTask(FROM_HERE,
                                     NewRunnableFunction(RunTestsFn));

    return true;
}

bool
TestStackHooksChild::AnswerStackFrame()
{
    if (!mOnStack)
        fail("missed stack notification");

    // FIXME use IPDL state instead
    if (4 == mEntered) {
        if (!SendAsync())
            fail("sending Async()");
    }
    else if (5 == mEntered) {
        if (!SendSync())
            fail("sending Sync()");
    }
    else {
        fail("unexpected |mEntered| count");
    }

    if (!mOnStack)
        fail("bad stack exit notification");

    return true;
}

void
TestStackHooksChild::RunTests()
{
    // 1 because of RecvStart()
    if (1 != mEntered)
        fail("missed stack notification");
    if (mOnStack)
        fail("spurious stack notification");

    if (!SendAsync())
        fail("sending Async()");
    if (mOnStack)
        fail("spurious stack notification");
    if (2 != mEntered)
        fail("missed stack notification");

    if (!SendSync())
        fail("sending Sync()");
    if (mOnStack)
        fail("spurious stack notification");
    if (3 != mEntered)
        fail("missed stack notification");

    if (!CallRpc())
        fail("calling RPC()");
    if (mOnStack)
        fail("spurious stack notification");
    if (4 != mEntered)
        fail("missed stack notification");

    if (!CallStackFrame())
        fail("calling StackFrame()");
    if (mOnStack)
        fail("spurious stack notification");
    if (5 != mEntered)
        fail("missed stack notification");

    Close();
}

} // namespace _ipdltest
} // namespace mozilla
