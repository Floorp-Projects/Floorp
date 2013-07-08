#include "TestFailedCtor.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent
void
TestFailedCtorParent::Main()
{
    PTestFailedCtorSubParent* p = CallPTestFailedCtorSubConstructor();
    if (p)
        fail("expected ctor to fail");

    Close();
}

PTestFailedCtorSubParent*
TestFailedCtorParent::AllocPTestFailedCtorSubParent()
{
    return new TestFailedCtorSubParent();
}
bool
TestFailedCtorParent::DeallocPTestFailedCtorSubParent(PTestFailedCtorSubParent* actor)
{
    delete actor;
    return true;
}

PTestFailedCtorSubsubParent*
TestFailedCtorSubParent::AllocPTestFailedCtorSubsubParent()
{
    TestFailedCtorSubsub* a = new TestFailedCtorSubsub();
    if (!mOne) {
        return mOne = a;
    } else if (!mTwo) {
        return mTwo = a;
    } else if (!mThree) {
        return mThree = a;
    } else {
        fail("unexpected Alloc()");
        return nullptr;
    }
}
bool
TestFailedCtorSubParent::DeallocPTestFailedCtorSubsubParent(PTestFailedCtorSubsubParent* actor)
{
    static_cast<TestFailedCtorSubsub*>(actor)->mDealloced = true;
    return true;
}

void
TestFailedCtorSubParent::ActorDestroy(ActorDestroyReason why)
{

    if (mOne->mWhy != Deletion)
        fail("Subsub one got wrong ActorDestroyReason");
    if (mTwo->mWhy != AncestorDeletion)
        fail("Subsub two got wrong ActorDestroyReason");
    if (mThree->mWhy != AncestorDeletion)
        fail("Subsub three got wrong ActorDestroyReason");

    if (FailedConstructor != why)
        fail("unexpected destruction!");
}

TestFailedCtorSubParent::~TestFailedCtorSubParent()
{
    if (!(mOne->mDealloced && mTwo->mDealloced && mThree->mDealloced))
        fail("Not all subsubs were Dealloc'd");
    delete mOne;
    delete mTwo;
    delete mThree;
}


//-----------------------------------------------------------------------------
// child

PTestFailedCtorSubChild*
TestFailedCtorChild::AllocPTestFailedCtorSubChild()
{
    return new TestFailedCtorSubChild();
}

bool
TestFailedCtorChild::AnswerPTestFailedCtorSubConstructor(PTestFailedCtorSubChild* actor)
{
    PTestFailedCtorSubsubChild* c1 = actor->SendPTestFailedCtorSubsubConstructor();
    PTestFailedCtorSubsubChild::Send__delete__(c1);

    if (!actor->SendPTestFailedCtorSubsubConstructor() ||
        !actor->SendPTestFailedCtorSubsubConstructor() ||
        !actor->SendSync())
        fail("setting up test");

    // This causes our process to die
    return false;
}

bool
TestFailedCtorChild::DeallocPTestFailedCtorSubChild(PTestFailedCtorSubChild* actor)
{
    delete actor;
    return true;
}

void
TestFailedCtorChild::ProcessingError(Result what)
{
    if (OtherProcess() != 0) // thread-mode
        _exit(0);
}

PTestFailedCtorSubsubChild*
TestFailedCtorSubChild::AllocPTestFailedCtorSubsubChild()
{
    return new TestFailedCtorSubsub();
}

bool
TestFailedCtorSubChild::DeallocPTestFailedCtorSubsubChild(PTestFailedCtorSubsubChild* actor)
{
    delete actor;
    return true;
}

void
TestFailedCtorSubChild::ActorDestroy(ActorDestroyReason why)
{
}


} // namespace _ipdltest
} // namespace mozilla
