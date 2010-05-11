#if defined(OS_POSIX)
#include <unistd.h>             // sleep()
#endif

#include "TestSyncWakeup.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestSyncWakeupParent::TestSyncWakeupParent()
{
    MOZ_COUNT_CTOR(TestSyncWakeupParent);
}

TestSyncWakeupParent::~TestSyncWakeupParent()
{
    MOZ_COUNT_DTOR(TestSyncWakeupParent);
}

void
TestSyncWakeupParent::Main()
{
    if (!SendStart())
        fail("sending Start()");
}

bool
TestSyncWakeupParent::AnswerStackFrame()
{
    if (!CallStackFrame())
        fail("calling StackFrame()");
    return true;
}

bool
TestSyncWakeupParent::RecvSync1()
{
    if (!SendNote1())
        fail("sending Note1()");

    // XXX ugh ... need to ensure that the async message and sync
    // reply come in "far enough" apart that this test doesn't pass on
    // accident
#if defined(OS_POSIX)
    // NB: can't use PR_Sleep (i.e. Sleep() on windows) because it's
    // only spec'd to block the current thread, not the current
    // process.  We need the IO thread to sleep as well.
    puts(" (sleeping for 5 seconds. sorry!)");
    sleep(5);
#endif

    return true;
}

bool
TestSyncWakeupParent::RecvSync2()
{
    if (!SendNote2())
        fail("sending Note2()");

#if defined(OS_POSIX)
    // see above
    sleep(5);
    puts(" (sleeping for 5 seconds. sorry!)");
#endif

    return true;
}

//-----------------------------------------------------------------------------
// child

TestSyncWakeupChild::TestSyncWakeupChild() : mDone(false)
{
    MOZ_COUNT_CTOR(TestSyncWakeupChild);
}

TestSyncWakeupChild::~TestSyncWakeupChild()
{
    MOZ_COUNT_DTOR(TestSyncWakeupChild);
}

bool
TestSyncWakeupChild::RecvStart()
{
    // First test: the parent fires back an async message while
    // replying to a sync one
    if (!SendSync1())
        fail("sending Sync()");

    // drop back into the event loop to get Note1(), then kick off the
    // second test
    return true;
}

bool
TestSyncWakeupChild::RecvNote1()
{
    // Second test: the parent fires back an async message while
    // replying to a sync one, with a frame on the RPC stack
    if (!CallStackFrame())
        fail("calling StackFrame()");

    if (!mDone)
        fail("should have received Note2()!");

    Close();

    return true;
}

bool
TestSyncWakeupChild::AnswerStackFrame()
{
    if (!SendSync2())
        fail("sending Sync()");

    return true;
}

bool
TestSyncWakeupChild::RecvNote2()
{
    mDone = true;
    return true;
}

} // namespace _ipdltest
} // namespace mozilla
