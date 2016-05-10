#include "TestInterruptRaces.h"

#include "IPDLUnitTests.h"      // fail etc.

using mozilla::ipc::MessageChannel;

namespace mozilla {
namespace _ipdltest {

ipc::RacyInterruptPolicy
MediateRace(const MessageChannel::MessageInfo& parent,
            const MessageChannel::MessageInfo& child)
{
    return (PTestInterruptRaces::Msg_Child__ID == parent.type()) ?
                ipc::RIPParentWins : ipc::RIPChildWins;
}

//-----------------------------------------------------------------------------
// parent
void
TestInterruptRacesParent::Main()
{
    if (!SendStart())
        fail("sending Start()");
}

bool
TestInterruptRacesParent::RecvStartRace()
{
    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestInterruptRacesParent::OnRaceTime));
    return true;
}

void
TestInterruptRacesParent::OnRaceTime()
{
    if (!CallRace(&mChildHasReply))
        fail("problem calling Race()");

    if (!mChildHasReply)
        fail("child should have got a reply already");

    mHasReply = true;

    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestInterruptRacesParent::Test2));
}

bool
TestInterruptRacesParent::AnswerRace(bool* hasReply)
{
    if (mHasReply)
        fail("apparently the parent won the Interrupt race!");
    *hasReply = hasReply;
    return true;
}

void
TestInterruptRacesParent::Test2()
{
    puts("  passed");
    puts("Test 2");

    mHasReply = false;
    mChildHasReply = false;

    if (!CallStackFrame())
        fail("can't set up a stack frame");

    puts("  passed");

    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestInterruptRacesParent::Test3));
}

bool
TestInterruptRacesParent::AnswerStackFrame()
{
    if (!SendWakeup())
        fail("can't wake up the child");

    if (!CallRace(&mChildHasReply))
        fail("can't set up race condition");
    mHasReply = true;

    if (!mChildHasReply)
        fail("child should have got a reply already");

    return true;
}

void
TestInterruptRacesParent::Test3()
{
    puts("Test 3");

    if (!CallStackFrame3())
        fail("can't set up a stack frame");

    puts("  passed");

    Close();
}

bool
TestInterruptRacesParent::AnswerStackFrame3()
{
    if (!SendWakeup3())
        fail("can't wake up the child");

    if (!CallChild())
        fail("can't set up race condition");

    return true;
}

bool
TestInterruptRacesParent::AnswerParent()
{
    mAnsweredParent = true;
    return true;
}

bool
TestInterruptRacesParent::RecvGetAnsweredParent(bool* answeredParent)
{
    *answeredParent = mAnsweredParent;
    return true;
}

//-----------------------------------------------------------------------------
// child
bool
TestInterruptRacesChild::RecvStart()
{
    puts("Test 1");

    if (!SendStartRace())
        fail("problem sending StartRace()");

    bool dontcare;
    if (!CallRace(&dontcare))
        fail("problem calling Race()");

    mHasReply = true;
    return true;
}

bool
TestInterruptRacesChild::AnswerRace(bool* hasReply)
{
    if (!mHasReply)
        fail("apparently the child lost the Interrupt race!");

    *hasReply = mHasReply;

    return true;
}

bool
TestInterruptRacesChild::AnswerStackFrame()
{
    // reset for the second test
    mHasReply = false;

    if (!CallStackFrame())
        fail("can't set up stack frame");

    if (!mHasReply)
        fail("should have had reply by now");

    return true;
}

bool
TestInterruptRacesChild::RecvWakeup()
{
    bool dontcare;
    if (!CallRace(&dontcare))
        fail("can't set up race condition");

    mHasReply = true;
    return true;
}

bool
TestInterruptRacesChild::AnswerStackFrame3()
{
    if (!CallStackFrame3())
        fail("can't set up stack frame");
    return true;
}

bool
TestInterruptRacesChild::RecvWakeup3()
{
    if (!CallParent())
        fail("can't set up race condition");
    return true;
}

bool
TestInterruptRacesChild::AnswerChild()
{
    bool parentAnsweredParent;
    // the parent is supposed to win the race, which means its
    // message, Child(), is supposed to be processed before the
    // child's message, Parent()
    if (!SendGetAnsweredParent(&parentAnsweredParent))
        fail("sending GetAnsweredParent");

    if (parentAnsweredParent)
        fail("parent was supposed to win the race!");

    return true;
}

} // namespace _ipdltest
} // namespace mozilla
