#include "TestMultiMgrs.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

void
TestMultiMgrsParent::Main()
{
    TestMultiMgrsLeftParent* leftie = new TestMultiMgrsLeftParent();
    if (!SendPTestMultiMgrsLeftConstructor(leftie))
        fail("error sending ctor");

    TestMultiMgrsRightParent* rightie = new TestMultiMgrsRightParent();
    if (!SendPTestMultiMgrsRightConstructor(rightie))
        fail("error sending ctor");

    TestMultiMgrsBottomParent* bottomL = new TestMultiMgrsBottomParent();
    if (!leftie->SendPTestMultiMgrsBottomConstructor(bottomL))
        fail("error sending ctor");

    TestMultiMgrsBottomParent* bottomR = new TestMultiMgrsBottomParent();
    if (!rightie->SendPTestMultiMgrsBottomConstructor(bottomR))
        fail("error sending ctor");

    if (!leftie->HasChild(bottomL))
        fail("leftie didn't have a child it was supposed to!");
    if (leftie->HasChild(bottomR))
        fail("leftie had rightie's child!");

    if (!rightie->HasChild(bottomR))
        fail("rightie didn't have a child it was supposed to!");
    if (rightie->HasChild(bottomL))
        fail("rightie had rightie's child!");

    if (!SendCheck())
        fail("couldn't kick off the child-side check");
}

mozilla::ipc::IPCResult
TestMultiMgrsParent::RecvOK()
{
    Close();
    return IPC_OK();
}

//-----------------------------------------------------------------------------
// child

mozilla::ipc::IPCResult
TestMultiMgrsLeftChild::RecvPTestMultiMgrsBottomConstructor(
    PTestMultiMgrsBottomChild* actor)
{
    static_cast<TestMultiMgrsChild*>(Manager())->mBottomL = actor;
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestMultiMgrsRightChild::RecvPTestMultiMgrsBottomConstructor(
    PTestMultiMgrsBottomChild* actor)
{
    static_cast<TestMultiMgrsChild*>(Manager())->mBottomR = actor;
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestMultiMgrsChild::RecvCheck()
{
    if (1 != ManagedPTestMultiMgrsLeftChild().Count())
        fail("where's leftie?");
    if (1 != ManagedPTestMultiMgrsRightChild().Count())
        fail("where's rightie?");

    TestMultiMgrsLeftChild* leftie =
        static_cast<TestMultiMgrsLeftChild*>(
            LoneManagedOrNullAsserts(ManagedPTestMultiMgrsLeftChild()));
    TestMultiMgrsRightChild* rightie =
        static_cast<TestMultiMgrsRightChild*>(
            LoneManagedOrNullAsserts(ManagedPTestMultiMgrsRightChild()));

    if (!leftie->HasChild(mBottomL))
        fail("leftie didn't have a child it was supposed to!");
    if (leftie->HasChild(mBottomR))
        fail("leftie had rightie's child!");

    if (!rightie->HasChild(mBottomR))
        fail("rightie didn't have a child it was supposed to!");
    if (rightie->HasChild(mBottomL))
        fail("rightie had leftie's child!");

    if (!SendOK())
        fail("couldn't send OK()");

    return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
