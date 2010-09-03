#include "TestRacyReentry.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestRacyReentryParent::TestRacyReentryParent() : mRecvdE(false)
{
    MOZ_COUNT_CTOR(TestRacyReentryParent);
}

TestRacyReentryParent::~TestRacyReentryParent()
{
    MOZ_COUNT_DTOR(TestRacyReentryParent);
}

void
TestRacyReentryParent::Main()
{
    if (!SendStart())
        fail("sending Start");

    if (!SendN())
        fail("sending N");
}

bool
TestRacyReentryParent::AnswerE()
{
    if (!mRecvdE) {
        mRecvdE = true;
        return true;
    }

    if (!CallH())
        fail("calling H");

    return true;
}

//-----------------------------------------------------------------------------
// child

TestRacyReentryChild::TestRacyReentryChild()
{
    MOZ_COUNT_CTOR(TestRacyReentryChild);
}

TestRacyReentryChild::~TestRacyReentryChild()
{
    MOZ_COUNT_DTOR(TestRacyReentryChild);
}

bool
TestRacyReentryChild::RecvStart()
{
    if (!CallE())
        fail("calling E");

    Close();

    return true;
}

bool
TestRacyReentryChild::RecvN()
{
    if (!CallE())
        fail("calling E");
    return true;
}

bool
TestRacyReentryChild::AnswerH()
{
    return true;
}

} // namespace _ipdltest
} // namespace mozilla
