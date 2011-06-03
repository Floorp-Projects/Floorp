#include "TestOpens.h"

#include "IPDLUnitTests.h"      // fail etc.

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestOpensOpenedChild>
{
    static void RetainCallee(mozilla::_ipdltest::TestOpensOpenedChild* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestOpensOpenedChild* obj) { }
};

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

void
TestOpensParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

PTestOpensOpenedParent*
TestOpensParent::AllocPTestOpensOpened(Transport* transport,
                                       ProcessId otherProcess)
{
    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nsnull;
    }

    nsAutoPtr<TestOpensOpenedParent> a(new TestOpensOpenedParent(transport));
    if (!a->Open(transport, h, XRE_GetIOMessageLoop(), AsyncChannel::Parent)) {
        return nsnull;
    }
    return a.forget();
}

void
TestOpensParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    passed("ok");
    QuitParent();
}

bool
TestOpensOpenedParent::RecvHello()
{
    return SendHi();
}

bool
TestOpensOpenedParent::RecvHelloSync()
{
    return true;
}

bool
TestOpensOpenedParent::AnswerHelloRpc()
{
    return CallHiRpc();
}

void
TestOpensOpenedParent::ActorDestroy(ActorDestroyReason why)
{
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

PTestOpensOpenedChild*
TestOpensChild::AllocPTestOpensOpened(Transport* transport,
                                      ProcessId otherProcess)
{
    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nsnull;
    }

    nsAutoPtr<TestOpensOpenedChild> a(new TestOpensOpenedChild(transport));
    if (!a->Open(transport, h, XRE_GetIOMessageLoop(), AsyncChannel::Child)) {
        return nsnull;
    }

    if (!a->SendHello())
        fail("sending Hello");

    return a.forget();
}

void
TestOpensChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");
    QuitChild();
}

bool
TestOpensOpenedChild::RecvHi()
{
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
    mGotHi = true;              // d00d
    return true;
}

void
TestOpensOpenedChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  

    gOpensChild->Close();

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        new DeleteTask<TestOpensOpenedChild>(this));
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<Transport>(mTransport));
}

} // namespace _ipdltest
} // namespace mozilla
