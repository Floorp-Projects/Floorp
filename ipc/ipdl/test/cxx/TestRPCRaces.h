#ifndef mozilla__ipdltest_TestRPCRaces_h
#define mozilla__ipdltest_TestRPCRaces_h

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRPCRacesParent.h"
#include "mozilla/_ipdltest/PTestRPCRacesChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRPCRacesParent :
    public PTestRPCRacesParent
{
public:
    TestRPCRacesParent() : mHasReply(false), mChildHasReply(false)
    { }
    virtual ~TestRPCRacesParent() { }

    void Main();

protected:
    NS_OVERRIDE
    virtual bool
    RecvStartRace();

    NS_OVERRIDE
    virtual bool
    AnswerRace(bool* hasRace);

    NS_OVERRIDE
    virtual bool
    AnswerStackFrame();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        if (!(mHasReply && mChildHasReply))
            fail("both sides should have replies!");
        passed("ok");
        QuitParent();
    }

private:
    void OnRaceTime();

    void Test2();

    bool mHasReply;
    bool mChildHasReply;
};


class TestRPCRacesChild :
    public PTestRPCRacesChild
{
public:
    TestRPCRacesChild() : mHasReply(false) { }
    virtual ~TestRPCRacesChild() { }

protected:
    NS_OVERRIDE
    virtual bool
    RecvStart();

    NS_OVERRIDE
    virtual bool
    AnswerRace(bool* hasRace);

    NS_OVERRIDE
    virtual bool
    AnswerStackFrame();

    NS_OVERRIDE
    virtual bool
    RecvWakeup();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    bool mHasReply;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPCRaces_h
