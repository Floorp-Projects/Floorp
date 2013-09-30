#include "base/thread.h"

#include "TestOpens.h"

#include "IPDLUnitTests.h"      // fail etc.

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestOpensChild>
{
    static void RetainCallee(mozilla::_ipdltest::TestOpensChild* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestOpensChild* obj) { }
};

template<>
struct RunnableMethodTraits<mozilla::_ipdltest2::TestOpensOpenedChild>
{
    static void RetainCallee(mozilla::_ipdltest2::TestOpensOpenedChild* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest2::TestOpensOpenedChild* obj) { }
};

using namespace base;
using namespace mozilla::ipc;

namespace mozilla {
// NB: this is generally bad style, but I am lazy.
using namespace _ipdltest;
using namespace _ipdltest2;

static MessageLoop* gMainThread;

static void
AssertNotMainThread()
{
    if (!gMainThread)
        fail("gMainThread is not initialized");
    if (MessageLoop::current() == gMainThread)
        fail("unexpectedly called on the main thread");
}

//-----------------------------------------------------------------------------
// parent

// Thread on which TestOpensOpenedParent runs
static Thread* gParentThread;

void
TestOpensParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

static void
OpenParent(TestOpensOpenedParent* aParent,
           Transport* aTransport, ProcessHandle aOtherProcess)
{
    AssertNotMainThread();

    // Open the actor on the off-main thread to park it there.
    // Messages will be delivered to this thread's message loop
    // instead of the main thread's.
    if (!aParent->Open(aTransport, aOtherProcess,
                       XRE_GetIOMessageLoop(), ipc::ParentSide))
        fail("opening Parent");
}

PTestOpensOpenedParent*
TestOpensParent::AllocPTestOpensOpenedParent(Transport* transport,
                                             ProcessId otherProcess)
{
    gMainThread = MessageLoop::current();

    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nullptr;
    }

    gParentThread = new Thread("ParentThread");
    if (!gParentThread->Start())
        fail("starting parent thread");

    TestOpensOpenedParent* a = new TestOpensOpenedParent(transport);
    gParentThread->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(OpenParent, a, transport, h));

    return a;
}

void
TestOpensParent::ActorDestroy(ActorDestroyReason why)
{
    // Stops the thread and joins it
    delete gParentThread;

    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    passed("ok");
    QuitParent();
}

bool
TestOpensOpenedParent::RecvHello()
{
    AssertNotMainThread();
    return SendHi();
}

bool
TestOpensOpenedParent::RecvHelloSync()
{
    AssertNotMainThread();
    return true;
}

bool
TestOpensOpenedParent::AnswerHelloRpc()
{
    AssertNotMainThread();
    return CallHiRpc();
}

void
TestOpensOpenedParent::ActorDestroy(ActorDestroyReason why)
{
    AssertNotMainThread();

    if (NormalShutdown != why)
        fail("unexpected destruction!");

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        new DeleteTask<TestOpensOpenedParent>(this));
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<Transport>(mTransport));
}

//-----------------------------------------------------------------------------
// child

static TestOpensChild* gOpensChild;
// Thread on which TestOpensOpenedChild runs
static Thread* gChildThread;

TestOpensChild::TestOpensChild()
{
    gOpensChild = this;
}

bool
TestOpensChild::RecvStart()
{
    if (!PTestOpensOpened::Open(this))
        fail("opening PTestOpensOpened");
    return true;
}

static void
OpenChild(TestOpensOpenedChild* aChild,
           Transport* aTransport, ProcessHandle aOtherProcess)
{
    AssertNotMainThread();

    // Open the actor on the off-main thread to park it there.
    // Messages will be delivered to this thread's message loop
    // instead of the main thread's.
    if (!aChild->Open(aTransport, aOtherProcess,
                      XRE_GetIOMessageLoop(), ipc::ChildSide))
        fail("opening Child");

    // Kick off the unit tests
    if (!aChild->SendHello())
        fail("sending Hello");
}

PTestOpensOpenedChild*
TestOpensChild::AllocPTestOpensOpenedChild(Transport* transport,
                                           ProcessId otherProcess)
{
    gMainThread = MessageLoop::current();

    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nullptr;
    }

    gChildThread = new Thread("ChildThread");
    if (!gChildThread->Start())
        fail("starting child thread");

    TestOpensOpenedChild* a = new TestOpensOpenedChild(transport);
    gChildThread->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(OpenChild, a, transport, h));

    return a;
}

void
TestOpensChild::ActorDestroy(ActorDestroyReason why)
{
    // Stops the thread and joins it
    delete gChildThread;

    if (NormalShutdown != why)
        fail("unexpected destruction!");
    QuitChild();
}

bool
TestOpensOpenedChild::RecvHi()
{
    AssertNotMainThread();

    if (!SendHelloSync())
        fail("sending HelloSync");
    if (!CallHelloRpc())
        fail("calling HelloRpc");
    if (!mGotHi)
        fail("didn't answer HiRpc");

    // Need to close the channel without message-processing frames on
    // the C++ stack
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &TestOpensOpenedChild::Close));
    return true;
}

bool
TestOpensOpenedChild::AnswerHiRpc()
{
    AssertNotMainThread();

    mGotHi = true;              // d00d
    return true;
}

static void
ShutdownTestOpensOpenedChild(TestOpensOpenedChild* child,
                             Transport* transport)
{
    delete child;

    // Now delete the transport, which has to happen after the
    // top-level actor is deleted.
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<Transport>(transport));

    // Kick off main-thread shutdown.
    gMainThread->PostTask(
        FROM_HERE,
        NewRunnableMethod(gOpensChild, &TestOpensChild::Close));
}

void
TestOpensOpenedChild::ActorDestroy(ActorDestroyReason why)
{
    AssertNotMainThread();

    if (NormalShutdown != why)
        fail("unexpected destruction!");  

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.  Defer shutdown to
    // let cleanup finish.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableFunction(ShutdownTestOpensOpenedChild,
                            this, mTransport));
}

} // namespace mozilla
