#ifndef mozilla__ipdltest_TestInterruptShutdownRace_h
#define mozilla__ipdltest_TestInterruptShutdownRace_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestInterruptShutdownRaceParent.h"
#include "mozilla/_ipdltest/PTestInterruptShutdownRaceChild.h"

namespace mozilla {
namespace _ipdltest {


class TestInterruptShutdownRaceParent :
    public PTestInterruptShutdownRaceParent
{
public:
    TestInterruptShutdownRaceParent();
    virtual ~TestInterruptShutdownRaceParent();

    static bool RunTestInProcesses() { return true; }
    // FIXME/bug 703323 Could work if modified
    static bool RunTestInThreads() { return false; }

    void Main();

    virtual bool RecvStartDeath() MOZ_OVERRIDE;

    virtual bool RecvOrphan() MOZ_OVERRIDE;

protected:
    void StartShuttingDown();

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
    }
};


class TestInterruptShutdownRaceChild :
    public PTestInterruptShutdownRaceChild
{
public:
    TestInterruptShutdownRaceChild();
    virtual ~TestInterruptShutdownRaceChild();

protected:
    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual bool AnswerExit() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        fail("should have 'crashed'!");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestInterruptShutdownRace_h
