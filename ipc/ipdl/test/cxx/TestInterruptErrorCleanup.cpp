#include "TestInterruptErrorCleanup.h"

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
    delete static_cast<TestInterruptErrorCleanupParent*>(gParentActor);
    gParentActor = nullptr;

    // needs to be synchronous to avoid affecting event ordering on
    // the main thread
    Mutex mutex("TestInterruptErrorCleanup.DeleteTheWorld.mutex");
    CondVar cvar(mutex, "TestInterruptErrorCleanup.DeleteTheWorld.cvar");

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

TestInterruptErrorCleanupParent::TestInterruptErrorCleanupParent()
    : mGotProcessingError(false)
{
    MOZ_COUNT_CTOR(TestInterruptErrorCleanupParent);
}

TestInterruptErrorCleanupParent::~TestInterruptErrorCleanupParent()
{
    MOZ_COUNT_DTOR(TestInterruptErrorCleanupParent);
}

void
TestInterruptErrorCleanupParent::Main()
{
    // This test models the following sequence of events
    //
    // (1) Parent: Interrupt out-call
    // (2) Child: crash
    // --[Parent-only hereafter]--
    // (3) Interrupt out-call return false
    // (4) Close()
    // --[event loop]--
    // (5) delete parentActor
    // (6) delete childProcess
    // --[event loop]--
    // (7) Channel::OnError notification
    // --[event loop]--
    // (8) Done, quit
    //
    // See bug 535298 and friends; this seqeunce of events captures
    // three differnent potential errors
    //  - Close()-after-error (semantic error previously)
    //  - use-after-free of parentActor
    //  - use-after-free of channel
    //
    // Because of legacy constraints related to nsNPAPI* code, we need
    // to ensure that this sequence of events can occur without
    // errors/crashes.

    MessageLoop::current()->PostTask(
        NewRunnableFunction(DeleteTheWorld));

    // it's a failure if this *succeeds*
    if (CallError())
        fail("expected an error!");

    if (!mGotProcessingError)
        fail("expected a ProcessingError() notification");

    // it's OK to Close() a channel after an error, because nsNPAPI*
    // wants to do this
    Close();

    // we know that this event *must* be after the MaybeError
    // notification enqueued by AsyncChannel, because that event is
    // enqueued within the same mutex that ends up signaling the
    // wakeup-on-error of |CallError()| above
    MessageLoop::current()->PostTask(NewRunnableFunction(Done));
}

void
TestInterruptErrorCleanupParent::ProcessingError(Result aCode, const char* aReason)
{
    if (aCode != MsgDropped)
        fail("unexpected processing error");
    mGotProcessingError = true;
}

//-----------------------------------------------------------------------------
// child

TestInterruptErrorCleanupChild::TestInterruptErrorCleanupChild()
{
    MOZ_COUNT_CTOR(TestInterruptErrorCleanupChild);
}

TestInterruptErrorCleanupChild::~TestInterruptErrorCleanupChild()
{
    MOZ_COUNT_DTOR(TestInterruptErrorCleanupChild);
}

mozilla::ipc::IPCResult
TestInterruptErrorCleanupChild::AnswerError()
{
    _exit(0);
    NS_RUNTIMEABORT("unreached");
    return IPC_FAIL_NO_REASON(this);
}


} // namespace _ipdltest
} // namespace mozilla
