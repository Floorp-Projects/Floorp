#include "TestSyncError.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestSyncErrorParent::TestSyncErrorParent()
{
    MOZ_COUNT_CTOR(TestSyncErrorParent);
}

TestSyncErrorParent::~TestSyncErrorParent()
{
    MOZ_COUNT_DTOR(TestSyncErrorParent);
}

void
TestSyncErrorParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

mozilla::ipc::IPCResult
TestSyncErrorParent::RecvError()
{
    return IPC_FAIL_NO_REASON(this);
}


//-----------------------------------------------------------------------------
// child

TestSyncErrorChild::TestSyncErrorChild()
{
    MOZ_COUNT_CTOR(TestSyncErrorChild);
}

TestSyncErrorChild::~TestSyncErrorChild()
{
    MOZ_COUNT_DTOR(TestSyncErrorChild);
}

mozilla::ipc::IPCResult
TestSyncErrorChild::RecvStart()
{
    if (SendError())
        fail("Error() should have return false");

    Close();

    return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
