#ifndef mozilla__ipdltest_TestRPCShutdownRace_h
#define mozilla__ipdltest_TestRPCShutdownRace_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRPCShutdownRaceParent.h"
#include "mozilla/_ipdltest/PTestRPCShutdownRaceChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRPCShutdownRaceParent :
    public PTestRPCShutdownRaceParent
{
public:
    TestRPCShutdownRaceParent();
    virtual ~TestRPCShutdownRaceParent();

    static bool RunTestInProcesses() { return true; }
    // FIXME/bug 703323 Could work if modified
    static bool RunTestInThreads() { return false; }

    void Main();

    NS_OVERRIDE
    virtual bool RecvStartDeath();

    NS_OVERRIDE
    virtual bool RecvOrphan();

protected:
    void StartShuttingDown();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
    }
};


class TestRPCShutdownRaceChild :
    public PTestRPCShutdownRaceChild
{
public:
    TestRPCShutdownRaceChild();
    virtual ~TestRPCShutdownRaceChild();

protected:
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual bool AnswerExit();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        fail("should have 'crashed'!");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPCShutdownRace_h
