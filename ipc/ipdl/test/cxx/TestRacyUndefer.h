#ifndef mozilla__ipdltest_TestRacyUndefer_h
#define mozilla__ipdltest_TestRacyUndefer_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyUndeferParent.h"
#include "mozilla/_ipdltest/PTestRacyUndeferChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRacyUndeferParent :
    public PTestRacyUndeferParent
{
public:
    TestRacyUndeferParent();
    virtual ~TestRacyUndeferParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    NS_OVERRIDE
    virtual bool AnswerSpam();

    NS_OVERRIDE
    virtual bool AnswerRaceWinTwice();

    NS_OVERRIDE
    virtual bool RecvDone();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestRacyUndeferChild :
    public PTestRacyUndeferChild
{
public:
    TestRacyUndeferChild();
    virtual ~TestRacyUndeferChild();

protected:
    NS_OVERRIDE
    virtual bool RecvStart();

    NS_OVERRIDE
    virtual bool RecvAwakenSpam();
    NS_OVERRIDE
    virtual bool RecvAwakenRaceWinTwice();

    NS_OVERRIDE
    virtual bool AnswerRace();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRacyUndefer_h
