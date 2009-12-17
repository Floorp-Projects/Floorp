#include "TestShutdown.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side
void
TestShutdownParent::Main()
{
    SendStart();
}

void
TestShutdownParent::ActorDestroy(ActorDestroyReason why)
{
    if (AbnormalShutdown != why)
        fail("should have ended test with crash!");

    passed("ok");

    QuitParent();
}

void
TestShutdownSubParent::ActorDestroy(ActorDestroyReason why)
{
    nsTArray<PTestShutdownSubParent*> broArr; // grumble grumble
    Manager()->ManagedPTestShutdownSubParent(broArr);
    if (broArr.Length() == 0)
        fail("manager should still have managees!");

    if (mExpectCrash && AbnormalShutdown != why)
        fail("expected crash!");
    else if (!mExpectCrash && AbnormalShutdown == why)
        fail("wasn't expecting crash!");

    nsTArray<PTestShutdownSubsubParent*> kidsArr;
    ManagedPTestShutdownSubsubParent(kidsArr);
    if (mExpectCrash && 0 == kidsArr.Length())
        fail("expected to *still* have kids");
}

void
TestShutdownSubsubParent::ActorDestroy(ActorDestroyReason why)
{
    nsTArray<PTestShutdownSubsubParent*> broArr;
    Manager()->ManagedPTestShutdownSubsubParent(broArr);
    if (broArr.Length() == 0)
        fail("manager should still have managees!");

    if (mExpectParentDeleted && AncestorDeletion != why)
        fail("expected ParentDeleted == why");
    else if (!mExpectParentDeleted && AncestorDeletion == why)
        fail("wasn't expecting parent delete");
}

//-----------------------------------------------------------------------------
// Child side

bool
TestShutdownChild::RecvStart()
{
    // test 1: alloc some actors and subactors, delete in
    // managee-before-manager order
    {
        bool expectCrash = false, expectParentDeleted = false;

        PTestShutdownSubChild* c1 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c1)
            fail("problem sending ctor");

        PTestShutdownSubChild* c2 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c1s1 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c1s2 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c2s1 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c2s2 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild::Send__delete__(c1s1);
        PTestShutdownSubsubChild::Send__delete__(c1s2);
        PTestShutdownSubsubChild::Send__delete__(c2s1);
        PTestShutdownSubsubChild::Send__delete__(c2s2);

        PTestShutdownSubChild::Send__delete__(c1);
        PTestShutdownSubChild::Send__delete__(c2);
    }

    // test 2: alloc some actors and subactors, delete managers first
    {
        bool expectCrash = false, expectParentDeleted = true;

        PTestShutdownSubChild* c1 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c1)
            fail("problem sending ctor");

        PTestShutdownSubChild* c2 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c1s1 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c1s2 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c2s1 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c2s2 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s2)
            fail("problem sending ctor");

        // delete parents without deleting kids
        PTestShutdownSubChild::Send__delete__(c1);
        PTestShutdownSubChild::Send__delete__(c2);
    }

    // test 3: alloc some actors and subactors, then crash
    {
        bool expectCrash = true, expectParentDeleted = false;

        PTestShutdownSubChild* c1 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c1)
            fail("problem sending ctor");

        PTestShutdownSubChild* c2 =
            SendPTestShutdownSubConstructor(expectCrash);
        if (!c2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c1s1 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c1s2 =
            c1->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c1s2)
            fail("problem sending ctor");

        PTestShutdownSubsubChild* c2s1 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s1)
            fail("problem sending ctor");
        PTestShutdownSubsubChild* c2s2 =
            c2->SendPTestShutdownSubsubConstructor(expectParentDeleted);
        if (!c2s2)
            fail("problem sending ctor");

        // make sure the ctors have been processed by the other side;
        // the write end of the socket may temporarily be unwriteable
        if (!SendSync())
            fail("can't synchronize with parent");

        // "crash", but without tripping tinderbox assert/abort
        // detectors
        _exit(0);
    }
}

void
TestShutdownChild::ActorDestroy(ActorDestroyReason why)
{
    fail("hey wait ... we should have crashed!");
}

void
TestShutdownSubChild::ActorDestroy(ActorDestroyReason why)
{
    nsTArray<PTestShutdownSubChild*> broArr;
    Manager()->ManagedPTestShutdownSubChild(broArr);
    if (broArr.Length() == 0)
        fail("manager should still have managees!");

    if (mExpectCrash && AbnormalShutdown != why)
        fail("expected crash!");
    else if (!mExpectCrash && AbnormalShutdown == why)
        fail("wasn't expecting crash!");

    nsTArray<PTestShutdownSubsubChild*> kidsArr;
    ManagedPTestShutdownSubsubChild(kidsArr);
    if (mExpectCrash && 0 == kidsArr.Length())
        fail("expected to *still* have kids");
}

void
TestShutdownSubsubChild::ActorDestroy(ActorDestroyReason why)
{
    nsTArray<PTestShutdownSubsubChild*> broArr;
    Manager()->ManagedPTestShutdownSubsubChild(broArr);
    if (broArr.Length() == 0)
        fail("manager should still have managees!");

    if (mExpectParentDeleted && AncestorDeletion != why)
        fail("expected ParentDeleted == why");
    else if (!mExpectParentDeleted && AncestorDeletion == why)
        fail("wasn't expecting parent delete");
}


} // namespace _ipdltest
} // namespace mozilla
