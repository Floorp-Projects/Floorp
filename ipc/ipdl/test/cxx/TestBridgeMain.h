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
    virtual PTestBridgeMainSubParent*
    AllocPTestBridgeMainSubParent(Transport* transport,
                                  ProcessId otherProcess) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
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
    virtual bool RecvHello() MOZ_OVERRIDE;
    virtual bool RecvHelloSync() MOZ_OVERRIDE;
    virtual bool AnswerHelloRpc() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

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
    virtual bool RecvBridgeEm() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
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
    virtual bool RecvPing() MOZ_OVERRIDE;

    virtual PTestBridgeMainSubChild*
    AllocPTestBridgeMainSubChild(Transport* transport,
                                 ProcessId otherProcess) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
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
    virtual bool RecvHi() MOZ_OVERRIDE;
    virtual bool AnswerHiRpc() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    bool mGotHi;
    Transport* mTransport;
};

} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestBridgeMain_h
