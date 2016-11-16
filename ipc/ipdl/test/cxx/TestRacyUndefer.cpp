#include "base/basictypes.h"

#include "TestRacyUndefer.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestRacyUndeferParent::TestRacyUndeferParent()
{
    MOZ_COUNT_CTOR(TestRacyUndeferParent);
}

TestRacyUndeferParent::~TestRacyUndeferParent()
{
    MOZ_COUNT_DTOR(TestRacyUndeferParent);
}

void
TestRacyUndeferParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

mozilla::ipc::IPCResult
TestRacyUndeferParent::AnswerSpam()
{
    static bool spammed = false;
    static bool raced = false;
    if (!spammed) {
        spammed = true;

        if (!SendAwakenSpam())
            fail("sending AwakenSpam");
    }
    else if (!raced) {
        raced = true;

        if (!SendAwakenRaceWinTwice())
            fail("sending WinRaceTwice");

        if (!CallRace())
            fail("calling Race1");
    }
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyUndeferParent::AnswerRaceWinTwice()
{
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyUndeferParent::RecvDone()
{
    Close();
    return IPC_OK();
}


//-----------------------------------------------------------------------------
// child

TestRacyUndeferChild::TestRacyUndeferChild()
{
    MOZ_COUNT_CTOR(TestRacyUndeferChild);
}

TestRacyUndeferChild::~TestRacyUndeferChild()
{
    MOZ_COUNT_DTOR(TestRacyUndeferChild);
}

mozilla::ipc::IPCResult
TestRacyUndeferChild::RecvStart()
{
    if (!CallSpam())
        fail("calling Spam");

    if (!SendDone())
        fail("sending Done");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyUndeferChild::RecvAwakenSpam()
{
    if (!CallSpam())
        fail("calling Spam");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyUndeferChild::RecvAwakenRaceWinTwice()
{
    if (!CallRaceWinTwice())
        fail("calling RaceWinTwice");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyUndeferChild::AnswerRace()
{
    return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
