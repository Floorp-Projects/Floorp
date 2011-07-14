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

bool
TestSyncErrorParent::RecvError()
{
    return false;
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

bool
TestSyncErrorChild::RecvStart()
{
    if (SendError())
        fail("Error() should have return false");

    Close();

    return true;
}


} // namespace _ipdltest
} // namespace mozilla
