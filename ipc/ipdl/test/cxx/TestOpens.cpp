#include "base/task.h"
#include "base/thread.h"

#include "TestOpens.h"

#include "IPDLUnitTests.h"      // fail etc.

using namespace mozilla::ipc;

using base::ProcessHandle;
using base::Thread;

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
           Transport* aTransport, base::ProcessId aOtherPid)
{
    AssertNotMainThread();

    // Open the actor on the off-main thread to park it there.
    // Messages will be delivered to this thread's message loop
    // instead of the main thread's.
    if (!aParent->Open(aTransport, aOtherPid,
                       XRE_GetIOMessageLoop(), ipc::ParentSide))
        fail("opening Parent");
}

PTestOpensOpenedParent*
TestOpensParent::AllocPTestOpensOpenedParent(Transport* transport,
                                             ProcessId otherPid)
{
    gMainThread = MessageLoop::current();

    gParentThread = new Thread("ParentThread");
    if (!gParentThread->Start())
        fail("starting parent thread");

    TestOpensOpenedParent* a = new TestOpensOpenedParent(transport);
    gParentThread->message_loop()->PostTask(
        NewRunnableFunction(OpenParent, a, transport, otherPid));

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

static void
ShutdownTestOpensOpenedParent(TestOpensOpenedParent* parent,
                              Transport* transport)
{
    delete parent;
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
    gParentThread->message_loop()->PostTask(
        NewRunnableFunction(ShutdownTestOpensOpenedParent,
                            this, mTransport));
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
           Transport* aTransport, base::ProcessId aOtherPid)
{
    AssertNotMainThread();

    // Open the actor on the off-main thread to park it there.
    // Messages will be delivered to this thread's message loop
    // instead of the main thread's.
    if (!aChild->Open(aTransport, aOtherPid,
                      XRE_GetIOMessageLoop(), ipc::ChildSide))
        fail("opening Child");

    // Kick off the unit tests
    if (!aChild->SendHello())
        fail("sending Hello");
}

PTestOpensOpenedChild*
TestOpensChild::AllocPTestOpensOpenedChild(Transport* transport,
                                           ProcessId otherPid)
{
    gMainThread = MessageLoop::current();

    gChildThread = new Thread("ChildThread");
    if (!gChildThread->Start())
        fail("starting child thread");

    TestOpensOpenedChild* a = new TestOpensOpenedChild(transport);
    gChildThread->message_loop()->PostTask(
        NewRunnableFunction(OpenChild, a, transport, otherPid));

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
        NewNonOwningRunnableMethod(this, &TestOpensOpenedChild::Close));
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

    // Kick off main-thread shutdown.
    gMainThread->PostTask(
        NewNonOwningRunnableMethod(gOpensChild, &TestOpensChild::Close));
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
    gChildThread->message_loop()->PostTask(
        NewRunnableFunction(ShutdownTestOpensOpenedChild,
                            this, mTransport));
}

} // namespace mozilla
