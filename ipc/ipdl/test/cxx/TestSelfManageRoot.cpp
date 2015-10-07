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

    ASSERT(1 == ManagedPTestSelfManageParent().Count());

    TestSelfManageParent* aa =
        static_cast<TestSelfManageParent*>(a->SendPTestSelfManageConstructor());
    if (!aa)
        fail("constructing PTestSelfManage");

    ASSERT(1 == ManagedPTestSelfManageParent().Count() &&
           1 == a->ManagedPTestSelfManageParent().Count());

    if (!PTestSelfManageParent::Send__delete__(aa))
        fail("destroying PTestSelfManage");
    ASSERT(Deletion == aa->mWhy &&
           1 == ManagedPTestSelfManageParent().Count() &&
           0 == a->ManagedPTestSelfManageParent().Count());
    delete aa;

    aa =
        static_cast<TestSelfManageParent*>(a->SendPTestSelfManageConstructor());
    if (!aa)
        fail("constructing PTestSelfManage");

    ASSERT(1 == ManagedPTestSelfManageParent().Count() &&
           1 == a->ManagedPTestSelfManageParent().Count());

    if (!PTestSelfManageParent::Send__delete__(a))
        fail("destroying PTestSelfManage");
    ASSERT(Deletion == a->mWhy &&
           AncestorDeletion == aa->mWhy &&
           0 == ManagedPTestSelfManageParent().Count() &&
           0 == a->ManagedPTestSelfManageParent().Count());
    delete a;
    delete aa;

    Close();
}

} // namespace _ipdltest
} // namespace mozilla
