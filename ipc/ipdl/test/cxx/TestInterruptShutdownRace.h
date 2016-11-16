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

    virtual mozilla::ipc::IPCResult RecvStartDeath() override;

    virtual mozilla::ipc::IPCResult RecvOrphan() override;

protected:
    void StartShuttingDown();

    virtual void ActorDestroy(ActorDestroyReason why) override
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
    virtual mozilla::ipc::IPCResult RecvStart() override;

    virtual mozilla::ipc::IPCResult AnswerExit() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        fail("should have 'crashed'!");
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestInterruptShutdownRace_h
