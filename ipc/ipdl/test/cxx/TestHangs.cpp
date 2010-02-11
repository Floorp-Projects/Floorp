#include "base/process_util.h"

#include "TestHangs.h"

#include "IPDLUnitTests.h"      // fail etc.

using base::KillProcess;

// XXX could drop this; very conservative
static const int kTimeoutSecs = 5;

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestHangsParent::TestHangsParent() : mFramesToGo(2)
{
    MOZ_COUNT_CTOR(TestHangsParent);
}

TestHangsParent::~TestHangsParent()
{
    MOZ_COUNT_DTOR(TestHangsParent);
}

void
TestHangsParent::Main()
{
    SetReplyTimeoutMs(1000 * kTimeoutSecs);

    if (CallStackFrame())
        fail("should have timed out!");

    Close();
}

bool
TestHangsParent::ShouldContinueFromReplyTimeout()
{
    // If we kill the subprocess here, then the "channel error" event
    // posted by the IO thread will race with the |Close()| above, in
    // |Main()|.  As killing the child process will probably be a
    // common action to take from ShouldContinue(), we need to ensure
    // that *Channel can deal.

    // XXX: OtherProcess() is a semi-private API, but using it is
    // OK until we start worrying about inter-thread comm
    if (!KillProcess(OtherProcess(), 0, false))
        fail("terminating child process");

    return false;
}

bool
TestHangsParent::AnswerStackFrame()
{
    if (--mFramesToGo) {
        if (CallStackFrame())
            fail("should have timed out!");
    }
    else {
        if (CallHang())
            fail("should have timed out!");
    }

    return true;
}


//-----------------------------------------------------------------------------
// child

TestHangsChild::TestHangsChild()
{
    MOZ_COUNT_CTOR(TestHangsChild);
}

TestHangsChild::~TestHangsChild()
{
    MOZ_COUNT_DTOR(TestHangsChild);
}

bool
TestHangsChild::AnswerHang()
{
    puts(" (child process is hanging now)");

    // XXX: pause() is right for this, but windows doesn't appear to
    // implement it.  So sleep for 100,000 seconds instead.
    PR_Sleep(PR_SecondsToInterval(100000));

    fail("should have been killed!");
    return false;               // not reached
}

} // namespace _ipdltest
} // namespace mozilla
