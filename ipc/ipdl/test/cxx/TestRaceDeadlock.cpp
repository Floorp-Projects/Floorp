#include "TestRaceDeadlock.h"

#include "IPDLUnitTests.h"      // fail etc.

// #define TEST_TIMEOUT 5000

using namespace mozilla::ipc;
typedef mozilla::ipc::MessageChannel::Message Message;
typedef mozilla::ipc::MessageChannel::MessageInfo MessageInfo;

namespace mozilla {
namespace _ipdltest {

static RacyInterruptPolicy
MediateRace(const MessageInfo& parent, const MessageInfo& child)
{
    return (PTestRaceDeadlock::Msg_Win__ID == parent.type()) ?
        RIPParentWins : RIPChildWins;
}

//-----------------------------------------------------------------------------
// parent

TestRaceDeadlockParent::TestRaceDeadlockParent()
{
    MOZ_COUNT_CTOR(TestRaceDeadlockParent);
}

TestRaceDeadlockParent::~TestRaceDeadlockParent()
{
    MOZ_COUNT_DTOR(TestRaceDeadlockParent);
}

void
TestRaceDeadlockParent::Main()
{
    Test1();

    Close();
}

bool
TestRaceDeadlockParent::ShouldContinueFromReplyTimeout()
{
    fail("This test should not hang");
    GetIPCChannel()->CloseWithTimeout();
    return false;
}

void
TestRaceDeadlockParent::Test1()
{
#if defined(TEST_TIMEOUT)
    SetReplyTimeoutMs(TEST_TIMEOUT);
#endif
    if (!SendStartRace()) {
        fail("sending StartRace");
    }
    if (!CallRpc()) {
        fail("calling Rpc");
    }
}

bool
TestRaceDeadlockParent::AnswerLose()
{
    return true;
}

RacyInterruptPolicy
TestRaceDeadlockParent::MediateInterruptRace(const MessageInfo& parent,
                                             const MessageInfo& child)
{
    return MediateRace(parent, child);
}

//-----------------------------------------------------------------------------
// child

TestRaceDeadlockChild::TestRaceDeadlockChild()
{
    MOZ_COUNT_CTOR(TestRaceDeadlockChild);
}

TestRaceDeadlockChild::~TestRaceDeadlockChild()
{
    MOZ_COUNT_DTOR(TestRaceDeadlockChild);
}

bool
TestRaceDeadlockParent::RecvStartRace()
{
    if (!CallWin()) {
        fail("calling Win");
    }
    return true;
}

bool
TestRaceDeadlockChild::RecvStartRace()
{
    if (!SendStartRace()) {
        fail("calling SendStartRace");
    }
    if (!CallLose()) {
        fail("calling Lose");
    }
    return true;
}

bool
TestRaceDeadlockChild::AnswerWin()
{
    return true;
}

bool
TestRaceDeadlockChild::AnswerRpc()
{
    return true;
}

RacyInterruptPolicy
TestRaceDeadlockChild::MediateInterruptRace(const MessageInfo& parent,
                                            const MessageInfo& child)
{
    return MediateRace(parent, child);
}

} // namespace _ipdltest
} // namespace mozilla
