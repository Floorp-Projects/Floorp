#include "TestSelfManageRoot.h"

#include "IPDLUnitTests.h"      // fail etc.

#define ASSERT(c)                               \
    do {                                        \
        if (!(c))                               \
            fail(#c);                           \
    } while (0)

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

void
TestSelfManageRootParent::Main()
{
    TestSelfManageParent* a =
        static_cast<TestSelfManageParent*>(SendPTestSelfManageConstructor());
    if (!a)
        fail("constructing PTestSelfManage");

    ASSERT(1 == ManagedPTestSelfManageParent().Length());

    TestSelfManageParent* aa =
        static_cast<TestSelfManageParent*>(a->SendPTestSelfManageConstructor());
    if (!aa)
        fail("constructing PTestSelfManage");

    ASSERT(1 == ManagedPTestSelfManageParent().Length() &&
           1 == a->ManagedPTestSelfManageParent().Length());

    if (!PTestSelfManageParent::Send__delete__(aa))
        fail("destroying PTestSelfManage");
    ASSERT(Deletion == aa->mWhy &&
           1 == ManagedPTestSelfManageParent().Length() &&
           0 == a->ManagedPTestSelfManageParent().Length());
    delete aa;

    aa =
        static_cast<TestSelfManageParent*>(a->SendPTestSelfManageConstructor());
    if (!aa)
        fail("constructing PTestSelfManage");

    ASSERT(1 == ManagedPTestSelfManageParent().Length() &&
           1 == a->ManagedPTestSelfManageParent().Length());

    if (!PTestSelfManageParent::Send__delete__(a))
        fail("destroying PTestSelfManage");
    ASSERT(Deletion == a->mWhy &&
           AncestorDeletion == aa->mWhy &&
           0 == ManagedPTestSelfManageParent().Length() &&
           0 == a->ManagedPTestSelfManageParent().Length());
    delete a;
    delete aa;

    Close();
}

} // namespace _ipdltest
} // namespace mozilla
