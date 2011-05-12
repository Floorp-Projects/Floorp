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

bool
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
    return true;
}

bool
TestRacyUndeferParent::AnswerRaceWinTwice()
{
    return true;
}

bool
TestRacyUndeferParent::RecvDone()
{
    Close();
    return true;
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

bool
TestRacyUndeferChild::RecvStart()
{
    if (!CallSpam())
        fail("calling Spam");

    if (!SendDone())
        fail("sending Done");

    return true;
}

bool
TestRacyUndeferChild::RecvAwakenSpam()
{
    if (!CallSpam())
        fail("calling Spam");
    return true;
}

bool
TestRacyUndeferChild::RecvAwakenRaceWinTwice()
{
    if (!CallRaceWinTwice())
        fail("calling RaceWinTwice");
    return true;
}

bool
TestRacyUndeferChild::AnswerRace()
{
    return true;
}

} // namespace _ipdltest
} // namespace mozilla
