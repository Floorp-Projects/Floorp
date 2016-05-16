#include "TestInterruptShutdownRace.h"

#include "base/task.h"
#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

namespace {

// NB: this test does its own shutdown, rather than going through
// QuitParent(), because it's testing degenerate edge cases

void DeleteSubprocess()
{
    delete gSubprocess;
    gSubprocess = nullptr;
}

void Done()
{
    passed(__FILE__);
    QuitParent();
}

} // namespace <anon>

TestInterruptShutdownRaceParent::TestInterruptShutdownRaceParent()
{
    MOZ_COUNT_CTOR(TestInterruptShutdownRaceParent);
}

TestInterruptShutdownRaceParent::~TestInterruptShutdownRaceParent()
{
    MOZ_COUNT_DTOR(TestInterruptShutdownRaceParent);
}

void
TestInterruptShutdownRaceParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

bool
TestInterruptShutdownRaceParent::RecvStartDeath()
{
    // this will be ordered before the OnMaybeDequeueOne event of
    // Orphan in the queue
    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this,
				   &TestInterruptShutdownRaceParent::StartShuttingDown));
    return true;
}

void
TestInterruptShutdownRaceParent::StartShuttingDown()
{
    // NB: we sleep here to try and avoid receiving the Orphan message
    // while waiting for the CallExit() reply.  if we fail at that, it
    // will cause the test to pass spuriously, because there won't be
    // an OnMaybeDequeueOne task for Orphan
    PR_Sleep(2000);

    if (CallExit())
        fail("connection was supposed to be interrupted");

    Close();

    delete static_cast<TestInterruptShutdownRaceParent*>(gParentActor);
    gParentActor = nullptr;

    XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(DeleteSubprocess));

    // this is ordered after the OnMaybeDequeueOne event in the queue
    MessageLoop::current()->PostTask(NewRunnableFunction(Done));

    // |this| has been deleted, be mindful
}

bool
TestInterruptShutdownRaceParent::RecvOrphan()
{
    // it would be nice to fail() here, but we'll process this message
    // while waiting for the reply CallExit().  The OnMaybeDequeueOne
    // task will still be in the queue, it just wouldn't have had any
    // work to do, if we hadn't deleted ourself
    return true;
}

//-----------------------------------------------------------------------------
// child

TestInterruptShutdownRaceChild::TestInterruptShutdownRaceChild()
{
    MOZ_COUNT_CTOR(TestInterruptShutdownRaceChild);
}

TestInterruptShutdownRaceChild::~TestInterruptShutdownRaceChild()
{
    MOZ_COUNT_DTOR(TestInterruptShutdownRaceChild);
}

bool
TestInterruptShutdownRaceChild::RecvStart()
{
    if (!SendStartDeath())
        fail("sending StartDeath");

    // See comment in StartShuttingDown(): we want to send Orphan()
    // while the parent is in its PR_Sleep()
    PR_Sleep(1000);

    if (!SendOrphan())
        fail("sending Orphan");

    return true;
}

bool
TestInterruptShutdownRaceChild::AnswerExit()
{
    _exit(0);
    NS_RUNTIMEABORT("unreached");
    return false;
}


} // namespace _ipdltest
} // namespace mozilla
