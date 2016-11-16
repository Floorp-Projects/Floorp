#include "TestDesc.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent
void
TestDescParent::Main()
{
    PTestDescSubParent* p = CallPTestDescSubConstructor(0);
    if (!p)
        fail("can't allocate Sub");

    PTestDescSubsubParent* pp = p->CallPTestDescSubsubConstructor();
    if (!pp)
        fail("can't allocate Subsub");

    if (!SendTest(pp))
        fail("can't send Subsub");
}

mozilla::ipc::IPCResult
TestDescParent::RecvOk(PTestDescSubsubParent* a)
{
    if (!a)
        fail("didn't receive Subsub");

    if (!PTestDescSubsubParent::Call__delete__(a))
        fail("deleting Subsub");

    Close();

    return IPC_OK();
}


PTestDescSubParent*
TestDescParent::AllocPTestDescSubParent(PTestDescSubsubParent* dummy) {
    if (dummy)
        fail("actor supposed to be null");
    return new TestDescSubParent();
}
bool
TestDescParent::DeallocPTestDescSubParent(PTestDescSubParent* actor)
{
    delete actor;
    return true;
}

PTestDescSubsubParent*
TestDescSubParent::AllocPTestDescSubsubParent()
{
    return new TestDescSubsubParent();
}
bool
TestDescSubParent::DeallocPTestDescSubsubParent(PTestDescSubsubParent* actor)
{
    delete actor;
    return true;
}


//-----------------------------------------------------------------------------
// child

mozilla::ipc::IPCResult
TestDescChild::RecvTest(PTestDescSubsubChild* a)
{
    if (!a)
        fail("didn't receive Subsub");
    if (!SendOk(a))
        fail("couldn't send Ok()");
    return IPC_OK();
}

PTestDescSubChild*
TestDescChild::AllocPTestDescSubChild(PTestDescSubsubChild* dummy) {
    if (dummy)
        fail("actor supposed to be null");
    return new TestDescSubChild();
}
bool
TestDescChild::DeallocPTestDescSubChild(PTestDescSubChild* actor)
{
    delete actor;
    return true;
}

PTestDescSubsubChild*
TestDescSubChild::AllocPTestDescSubsubChild()
{
    return new TestDescSubsubChild();
}
bool
TestDescSubChild::DeallocPTestDescSubsubChild(PTestDescSubsubChild* actor)
{
    delete actor;
    return true;
}

} // namespace _ipdltest
} // namespace mozilla
