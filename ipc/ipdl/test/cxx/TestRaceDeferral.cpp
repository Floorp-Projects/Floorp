#include "TestRaceDeferral.h"

#include "IPDLUnitTests.h"      // fail etc.

using namespace mozilla::ipc;
typedef mozilla::ipc::MessageChannel::Message Message;

namespace mozilla {
namespace _ipdltest {

static RacyInterruptPolicy
MediateRace(const Message& parent, const Message& child)
{
    return (PTestRaceDeferral::Msg_Win__ID == parent.type()) ?
        RIPParentWins : RIPChildWins;
}

//-----------------------------------------------------------------------------
// parent

TestRaceDeferralParent::TestRaceDeferralParent()
    : mProcessedLose(false)
{
    MOZ_COUNT_CTOR(TestRaceDeferralParent);
}

TestRaceDeferralParent::~TestRaceDeferralParent()
{
    MOZ_COUNT_DTOR(TestRaceDeferralParent);

    if (!mProcessedLose)
        fail("never processed Lose");
}

void
TestRaceDeferralParent::Main()
{
    Test1();

    Close();
}

void
TestRaceDeferralParent::Test1()
{
    if (!SendStartRace())
        fail("sending StartRace");

    if (!CallWin())
        fail("calling Win");
    if (mProcessedLose)
        fail("Lose didn't lose");

    if (!CallRpc())
        fail("calling Rpc");
    if (!mProcessedLose)
        fail("didn't resolve Rpc vs. Lose 'race' correctly");
}

bool
TestRaceDeferralParent::AnswerLose()
{
    if (mProcessedLose)
        fail("processed Lose twice");
    mProcessedLose = true;
    return true;
}

RacyInterruptPolicy
TestRaceDeferralParent::MediateInterruptRace(const Message& parent,
                                       const Message& child)
{
    return MediateRace(parent, child);
}

//-----------------------------------------------------------------------------
// child

TestRaceDeferralChild::TestRaceDeferralChild()
{
    MOZ_COUNT_CTOR(TestRaceDeferralChild);
}

TestRaceDeferralChild::~TestRaceDeferralChild()
{
    MOZ_COUNT_DTOR(TestRaceDeferralChild);
}

bool
TestRaceDeferralChild::RecvStartRace()
{
    if (!CallLose())
        fail("calling Lose");
    return true;
}

bool
TestRaceDeferralChild::AnswerWin()
{
    return true;
}

bool
TestRaceDeferralChild::AnswerRpc()
{
    return true;
}

RacyInterruptPolicy
TestRaceDeferralChild::MediateInterruptRace(const Message& parent,
                                      const Message& child)
{
    return MediateRace(parent, child);
}

} // namespace _ipdltest
} // namespace mozilla
