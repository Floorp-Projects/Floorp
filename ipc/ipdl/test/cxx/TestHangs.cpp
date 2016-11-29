#include "base/process_util.h"

#include "TestHangs.h"

#include "IPDLUnitTests.h"      // fail etc.

using base::KillProcess;

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestHangsParent::TestHangsParent()
  : mDetectedHang(false)
  , mNumAnswerStackFrame(0)
{
    MOZ_COUNT_CTOR(TestHangsParent);
}

TestHangsParent::~TestHangsParent()
{
    MOZ_COUNT_DTOR(TestHangsParent);
}

void
TestHangsParent::Main()
{
    // Here we try to set things up to test the following sequence of events:
    //
    // - subprocess causes an OnMaybeDequeueOne() task to be posted to
    //   this thread
    //
    // - subprocess hangs just long enough for the hang timer to expire
    //
    // - hang-kill code in the parent starts running
    //
    // - subprocess replies to message while hang code runs
    //
    // - reply is processed in OnMaybeDequeueOne() before Close() has
    //   been called or the channel error notification has been posted

    // this tells the subprocess to send us Nonce()
    if (!SendStart())
        fail("sending Start");

    // now we sleep here for a while awaiting the Nonce() message from
    // the child.  since we're not blocked on anything, the IO thread
    // will enqueue an OnMaybeDequeueOne() task to process that
    // message
    //
    // NB: PR_Sleep is exactly what we want, only the current thread
    // sleeping
    PR_Sleep(5000);

    // when we call into this, we'll pull the Nonce() message out of
    // the mPending queue, but that doesn't matter ... the
    // OnMaybeDequeueOne() event will remain
    if (CallStackFrame() && mDetectedHang)
        fail("should have timed out!");

    // the Close() task in the queue will shut us down
}

bool
TestHangsParent::ShouldContinueFromReplyTimeout()
{
    mDetectedHang = true;

    // so we've detected a timeout after 2 ms ... now we cheat and
    // sleep for a long time, to allow the subprocess's reply to come
    // in

    PR_Sleep(5000);

    // reply should be here; we'll post a task to shut things down.
    // This must be after OnMaybeDequeueOne() in the event queue.
    MessageLoop::current()->PostTask(
        NewNonOwningRunnableMethod(this, &TestHangsParent::CleanUp));

    GetIPCChannel()->CloseWithTimeout();

    return false;
}

mozilla::ipc::IPCResult
TestHangsParent::AnswerStackFrame()
{
    ++mNumAnswerStackFrame;

    // XXX This assertion will get deleted as part of bug 1316757.
    MOZ_ASSERT((PTestHangs::HANG != state()) == (mNumAnswerStackFrame == 1));

    if (mNumAnswerStackFrame == 1) {
        if (CallStackFrame()) {
          fail("should have timed out!");
        }
    } else if (mNumAnswerStackFrame == 2) {
        // minimum possible, 2 ms.  We want to detecting a hang to race
        // with the reply coming in, as reliably as possible
        SetReplyTimeoutMs(2);

        if (CallHang())
            fail("should have timed out!");
    } else {
        fail("unexpected state");
    }

    return IPC_OK();
}

void
TestHangsParent::CleanUp()
{
    ipc::ScopedProcessHandle otherProcessHandle;
    if (!base::OpenProcessHandle(OtherPid(), &otherProcessHandle.rwget())) {
        fail("couldn't open child process");
    } else {
        if (!KillProcess(otherProcessHandle, 0, false)) {
            fail("terminating child process");
        }
    }
    Close();
}


//-----------------------------------------------------------------------------
// child

TestHangsChild::TestHangsChild()
{
    MOZ_COUNT_CTOR(TestHangsChild);
}

TestHangsChild::~TestHangsChild()
{
    MOZ_COUNT_DTOR(TestHangsChild);
}

mozilla::ipc::IPCResult
TestHangsChild::AnswerHang()
{
    puts(" (child process is 'hanging' now)");

    // just sleep until we're reasonably confident the 1ms hang
    // detector fired in the parent process and it's sleeping in
    // ShouldContinueFromReplyTimeout()
    PR_Sleep(1000);

    return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
