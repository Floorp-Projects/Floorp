#include "TestBridgeMain.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "IPDLUnitTestSubprocess.h"

using namespace std;

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestBridgeMainSubChild>
{
    static void RetainCallee(mozilla::_ipdltest::TestBridgeMainSubChild* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestBridgeMainSubChild* obj) { }
};

namespace mozilla {
namespace _ipdltest {


//-----------------------------------------------------------------------------
// main process
void
TestBridgeMainParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

PTestBridgeMainSubParent*
TestBridgeMainParent::AllocPTestBridgeMainSubParent(Transport* transport,
                                                    ProcessId otherProcess)
{
    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nullptr;
    }

    nsAutoPtr<TestBridgeMainSubParent> a(new TestBridgeMainSubParent(transport));
    if (!a->Open(transport, h, XRE_GetIOMessageLoop(), ipc::ParentSide)) {
        return nullptr;
    }
    return a.forget();
}

void
TestBridgeMainParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    passed("ok");
    QuitParent();
}

bool
TestBridgeMainSubParent::RecvHello()
{
    return SendHi();
}

bool
TestBridgeMainSubParent::RecvHelloSync()
{
    return true;
}

bool
TestBridgeMainSubParent::AnswerHelloRpc()
{
    return CallHiRpc();
}

void
TestBridgeMainSubParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        new DeleteTask<TestBridgeMainSubParent>(this));
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<Transport>(mTransport));
}

//-----------------------------------------------------------------------------
// sub process --- child of main
TestBridgeMainChild* gBridgeMainChild;

TestBridgeMainChild::TestBridgeMainChild()
    : mSubprocess(nullptr)
{
    gBridgeMainChild = this;
}

bool
TestBridgeMainChild::RecvStart()
{
    vector<string> subsubArgs;
    subsubArgs.push_back("TestBridgeSub");

    mSubprocess = new IPDLUnitTestSubprocess();
    if (!mSubprocess->SyncLaunch(subsubArgs))
        fail("problem launching subprocess");

    IPC::Channel* transport = mSubprocess->GetChannel();
    if (!transport)
        fail("no transport");

    TestBridgeSubParent* bsp = new TestBridgeSubParent();
    bsp->Open(transport, mSubprocess->GetChildProcessHandle());

    bsp->Main();
    return true;
}

void
TestBridgeMainChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  
    // NB: this is kosher because QuitChild() joins with the IO thread
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<IPDLUnitTestSubprocess>(mSubprocess));
    QuitChild();
}

void
TestBridgeSubParent::Main()
{
    if (!SendPing())
        fail("sending Ping");
}

bool
TestBridgeSubParent::RecvBridgeEm()
{
    if (!PTestBridgeMainSub::Bridge(gBridgeMainChild, this))
        fail("bridging Main and Sub");
    return true;
}

void
TestBridgeSubParent::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");
    gBridgeMainChild->Close();

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        new DeleteTask<TestBridgeSubParent>(this));
}

//-----------------------------------------------------------------------------
// subsub process --- child of sub

static TestBridgeSubChild* gBridgeSubChild;

TestBridgeSubChild::TestBridgeSubChild()
{
    gBridgeSubChild = this;   
}

bool
TestBridgeSubChild::RecvPing()
{
    if (!SendBridgeEm())
        fail("sending BridgeEm");
    return true;
}

PTestBridgeMainSubChild*
TestBridgeSubChild::AllocPTestBridgeMainSubChild(Transport* transport,
                                                 ProcessId otherProcess)
{
    ProcessHandle h;
    if (!base::OpenProcessHandle(otherProcess, &h)) {
        return nullptr;
    }

    nsAutoPtr<TestBridgeMainSubChild> a(new TestBridgeMainSubChild(transport));
    if (!a->Open(transport, h, XRE_GetIOMessageLoop(), ipc::ChildSide)) {
        return nullptr;
    }

    if (!a->SendHello())
        fail("sending Hello");

    return a.forget();
}

void
TestBridgeSubChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");
    QuitChild();
}

bool
TestBridgeMainSubChild::RecvHi()
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
        NewRunnableMethod(this, &TestBridgeMainSubChild::Close));
    return true;
}

bool
TestBridgeMainSubChild::AnswerHiRpc()
{
    mGotHi = true;              // d00d
    return true;
}

void
TestBridgeMainSubChild::ActorDestroy(ActorDestroyReason why)
{
    if (NormalShutdown != why)
        fail("unexpected destruction!");  

    gBridgeSubChild->Close();

    // ActorDestroy() is just a callback from IPDL-generated code,
    // which needs the top-level actor (this) to stay alive a little
    // longer so other things can be cleaned up.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        new DeleteTask<TestBridgeMainSubChild>(this));
    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        new DeleteTask<Transport>(mTransport));
}

} // namespace mozilla
} // namespace _ipdltest
