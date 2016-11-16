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

mozilla::ipc::IPCResult
TestRacyReentryParent::AnswerE()
{
    if (!mRecvdE) {
        mRecvdE = true;
        return IPC_OK();
    }

    if (!CallH())
        fail("calling H");

    return IPC_OK();
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

mozilla::ipc::IPCResult
TestRacyReentryChild::RecvStart()
{
    if (!CallE())
        fail("calling E");

    Close();

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyReentryChild::RecvN()
{
    if (!CallE())
        fail("calling E");
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestRacyReentryChild::AnswerH()
{
    return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
