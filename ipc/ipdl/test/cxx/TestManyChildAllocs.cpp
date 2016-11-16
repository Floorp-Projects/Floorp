#include "TestManyChildAllocs.h"

#include "IPDLUnitTests.h"      // fail etc.


#define NALLOCS 10

namespace mozilla {
namespace _ipdltest {

// parent code

TestManyChildAllocsParent::TestManyChildAllocsParent()
{
    MOZ_COUNT_CTOR(TestManyChildAllocsParent);
}

TestManyChildAllocsParent::~TestManyChildAllocsParent()
{
    MOZ_COUNT_DTOR(TestManyChildAllocsParent);
}

void
TestManyChildAllocsParent::Main()
{
    if (!SendGo())
        fail("can't send Go()");
}

mozilla::ipc::IPCResult
TestManyChildAllocsParent::RecvDone()
{
    // explicitly *not* cleaning up, so we can sanity-check IPDL's
    // auto-shutdown/cleanup handling
    Close();

    return IPC_OK();
}

bool
TestManyChildAllocsParent::DeallocPTestManyChildAllocsSubParent(
    PTestManyChildAllocsSubParent* __a)
{
    delete __a; return true;
}

PTestManyChildAllocsSubParent*
TestManyChildAllocsParent::AllocPTestManyChildAllocsSubParent()
{
    return new TestManyChildAllocsSubParent();
}


// child code

TestManyChildAllocsChild::TestManyChildAllocsChild()
{
    MOZ_COUNT_CTOR(TestManyChildAllocsChild);
}

TestManyChildAllocsChild::~TestManyChildAllocsChild()
{
    MOZ_COUNT_DTOR(TestManyChildAllocsChild);
}

mozilla::ipc::IPCResult TestManyChildAllocsChild::RecvGo()
{
    for (int i = 0; i < NALLOCS; ++i) {
        PTestManyChildAllocsSubChild* child =
            SendPTestManyChildAllocsSubConstructor();

        if (!child)
            fail("can't send ctor()");

        if (!child->SendHello())
            fail("can't send Hello()");
    }

    size_t len = ManagedPTestManyChildAllocsSubChild().Count();
    if (NALLOCS != len)
        fail("expected %lu kids, got %lu", NALLOCS, len);

    if (!SendDone())
        fail("can't send Done()");

    return IPC_OK();
}

bool
TestManyChildAllocsChild::DeallocPTestManyChildAllocsSubChild(
    PTestManyChildAllocsSubChild* __a)
{
    delete __a; return true;
}

PTestManyChildAllocsSubChild*
TestManyChildAllocsChild::AllocPTestManyChildAllocsSubChild()
{
    return new TestManyChildAllocsSubChild();
}


} // namespace _ipdltest
} // namespace mozilla
