#include "TestCrashCleanup.h"

#include "base/task.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

namespace {

// NB: this test does its own shutdown, rather than going through
// QuitParent(), because it's testing degenerate edge cases

void DeleteSubprocess(Mutex* mutex, CondVar* cvar)
{
    MutexAutoLock lock(*mutex);

    delete gSubprocess;
    gSubprocess = nullptr;

    cvar->Notify();
}

void DeleteTheWorld()
{
    delete static_cast<TestCrashCleanupParent*>(gParentActor);
    gParentActor = nullptr;

    // needs to be synchronous to avoid affecting event ordering on
    // the main thread
    Mutex mutex("TestCrashCleanup.DeleteTheWorld.mutex");
    CondVar cvar(mutex, "TestCrashCleanup.DeleteTheWorld.cvar");

    MutexAutoLock lock(mutex);

    XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction(DeleteSubprocess, &mutex, &cvar));

    cvar.Wait();
}

void Done()
{
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell (do_GetService(kAppShellCID));
  appShell->Exit();

  passed(__FILE__);
}

} // namespace <anon>

TestCrashCleanupParent::TestCrashCleanupParent() : mCleanedUp(false)
{
    MOZ_COUNT_CTOR(TestCrashCleanupParent);
}

TestCrashCleanupParent::~TestCrashCleanupParent()
{
    MOZ_COUNT_DTOR(TestCrashCleanupParent);

    if (!mCleanedUp)
        fail("should have been ActorDestroy()d!");
}

void
TestCrashCleanupParent::Main()
{
    // NB: has to be enqueued before IO thread's error notification
    MessageLoop::current()->PostTask(
        NewRunnableFunction(DeleteTheWorld));

    if (CallDIEDIEDIE())
        fail("expected an error!");

    Close();

    MessageLoop::current()->PostTask(NewRunnableFunction(Done));
}


//-----------------------------------------------------------------------------
// child

TestCrashCleanupChild::TestCrashCleanupChild()
{
    MOZ_COUNT_CTOR(TestCrashCleanupChild);
}

TestCrashCleanupChild::~TestCrashCleanupChild()
{
    MOZ_COUNT_DTOR(TestCrashCleanupChild);
}

bool
TestCrashCleanupChild::AnswerDIEDIEDIE()
{
    _exit(0);
    NS_RUNTIMEABORT("unreached");
    return false;
}


} // namespace _ipdltest
} // namespace mozilla
