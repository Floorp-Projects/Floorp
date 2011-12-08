#ifndef mozilla__ipdltest_TestBridgeMain_h
#define mozilla__ipdltest_TestBridgeMain_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestBridgeMainParent.h"
#include "mozilla/_ipdltest/PTestBridgeMainChild.h"

#include "mozilla/_ipdltest/PTestBridgeSubParent.h"
#include "mozilla/_ipdltest/PTestBridgeSubChild.h"

#include "mozilla/_ipdltest/PTestBridgeMainSubParent.h"
#include "mozilla/_ipdltest/PTestBridgeMainSubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// "Main" process
//
class TestBridgeMainParent :
    public PTestBridgeMainParent
{
public:
    TestBridgeMainParent() {}
    virtual ~TestBridgeMainParent() {}

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    NS_OVERRIDE
    virtual PTestBridgeMainSubParent*
    AllocPTestBridgeMainSub(Transport* transport,
                            ProcessId otherProcess);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);
};

class TestBridgeMainSubParent :
    public PTestBridgeMainSubParent
{
public:
    TestBridgeMainSubParent(Transport* aTransport)
        : mTransport(aTransport)
    {}
    virtual ~TestBridgeMainSubParent() {}

protected:
    NS_OVERRIDE
    virtual bool RecvHello();
    NS_OVERRIDE
    virtual bool RecvHelloSync();
    NS_OVERRIDE
    virtual bool AnswerHelloRpc();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    Transport* mTransport;
};

//-----------------------------------------------------------------------------
// "Sub" process --- child of "main"
//
class TestBridgeSubParent;

class TestBridgeMainChild :
    public PTestBridgeMainChild
{
public:
    TestBridgeMainChild();
    virtual ~TestBridgeMainChild() {}

protected:
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    IPDLUnitTestSubprocess* mSubprocess;
};

class TestBridgeSubParent :
    public PTestBridgeSubParent
{
public:
    TestBridgeSubParent() {}
    virtual ~TestBridgeSubParent() {}

    void Main();

protected:
    NS_OVERRIDE
    virtual bool RecvBridgeEm();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);
};

//-----------------------------------------------------------------------------
// "Subsub" process --- child of "sub"
//
class TestBridgeSubChild :
    public PTestBridgeSubChild
{
public:
    TestBridgeSubChild();
    virtual ~TestBridgeSubChild() {}

protected:
    NS_OVERRIDE
    virtual bool RecvPing();

    NS_OVERRIDE
    virtual PTestBridgeMainSubChild*
    AllocPTestBridgeMainSub(Transport* transport,
                            ProcessId otherProcess);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);
};

class TestBridgeMainSubChild :
    public PTestBridgeMainSubChild
{
public:
    TestBridgeMainSubChild(Transport* aTransport)
        : mGotHi(false)
        , mTransport(aTransport)
    {}
    virtual ~TestBridgeMainSubChild() {}

protected:
    NS_OVERRIDE
    virtual bool RecvHi();
    NS_OVERRIDE
    virtual bool AnswerHiRpc();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why);

    bool mGotHi;
    Transport* mTransport;
};

} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestBridgeMain_h
