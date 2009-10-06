#include "TestSanity.h"

#include "nsIAppShell.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h" // do_GetService()
#include "nsWidgetsCID.h"       // NS_APPSHELL_CID

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestSanityParent::TestSanityParent()
{
    MOZ_COUNT_CTOR(TestSanityParent);
}

TestSanityParent::~TestSanityParent()
{
    MOZ_COUNT_DTOR(TestSanityParent);
}

void
TestSanityParent::Main()
{
    if (!SendPing(0))
        fail("sending Ping");
}


bool
TestSanityParent::RecvPong(const int& one)
{
    if (1 != one)
        fail("invalid argument `%d', should have been `1'", one);

    passed("sent ping/received pong");

    static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
    nsCOMPtr<nsIAppShell> appShell (do_GetService(kAppShellCID));
    appShell->Exit();

    return true;
}

bool
TestSanityParent::RecvUNREACHED()
{
    fail("unreached");
    return false;               // not reached
}


//-----------------------------------------------------------------------------
// child

TestSanityChild::TestSanityChild()
{
    MOZ_COUNT_CTOR(TestSanityChild);
}

TestSanityChild::~TestSanityChild()
{
    MOZ_COUNT_DTOR(TestSanityChild);
}

bool
TestSanityChild::RecvPing(const int& zero)
{
    if (0 != zero)
        fail("invalid argument `%d', should have been `0'", zero);

    if (!SendPong(1))
        fail("sending Pong");
    return true;
}

bool
TestSanityChild::RecvUNREACHED()
{
    fail("unreached");
    return false;               // not reached
}


} // namespace _ipdltest
} // namespace mozilla
