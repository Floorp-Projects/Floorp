#include "TestDesc.h"

#include "nsIAppShell.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h" // do_GetService()
#include "nsWidgetsCID.h"       // NS_APPSHELL_CID

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent
void
TestDescParent::Main()
{
    PTestDescSubParent* p = SendPTestDescSubConstructor();
    if (!p)
        fail("can't allocate Sub");

    PTestDescSubsubParent* pp = p->SendPTestDescSubsubConstructor();
    if (!pp)
        fail("can't allocate Subsub");

    if (!SendTest(pp))
        fail("can't send Subsub");
}

bool
TestDescParent::RecvOk(PTestDescSubsubParent* a)
{
    if (!a)
        fail("didn't receive Subsub");

    passed("ok");

    static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
    nsCOMPtr<nsIAppShell> appShell (do_GetService(kAppShellCID));
    appShell->Exit();

    return true;
}


PTestDescSubParent*
TestDescParent::AllocPTestDescSub() {
    return new TestDescSubParent();
}
bool
TestDescParent::DeallocPTestDescSub(PTestDescSubParent* actor)
{
    delete actor;
    return true;
}

PTestDescSubsubParent*
TestDescSubParent::AllocPTestDescSubsub()
{
    return new TestDescSubsubParent();
}
bool
TestDescSubParent::DeallocPTestDescSubsub(PTestDescSubsubParent* actor)
{
    delete actor;
    return true;
}


//-----------------------------------------------------------------------------
// child

bool
TestDescChild::RecvTest(PTestDescSubsubChild* a)
{
    if (!a)
        fail("didn't receive Subsub");
    if (!SendOk(a))
        fail("couldn't send Ok()");
    return true;
}

PTestDescSubChild*
TestDescChild::AllocPTestDescSub() {
    return new TestDescSubChild();
}
bool
TestDescChild::DeallocPTestDescSub(PTestDescSubChild* actor)
{
    delete actor;
    return true;
}

PTestDescSubsubChild*
TestDescSubChild::AllocPTestDescSubsub()
{
    return new TestDescSubsubChild();
}
bool
TestDescSubChild::DeallocPTestDescSubsub(PTestDescSubsubChild* actor)
{
    delete actor;
    return true;
}

} // namespace _ipdltest
} // namespace mozilla
