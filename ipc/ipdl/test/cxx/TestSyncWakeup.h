#ifndef mozilla__ipdltest_TestSyncWakeup_h
#define mozilla__ipdltest_TestSyncWakeup_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncWakeupParent.h"
#include "mozilla/_ipdltest/PTestSyncWakeupChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSyncWakeupParent :
    public PTestSyncWakeupParent
{
public:
    TestSyncWakeupParent();
    virtual ~TestSyncWakeupParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual mozilla::ipc::IPCResult AnswerStackFrame() override;

    virtual mozilla::ipc::IPCResult RecvSync1() override;

    virtual mozilla::ipc::IPCResult RecvSync2() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        passed("ok");
        QuitParent();
    }
};


class TestSyncWakeupChild :
    public PTestSyncWakeupChild
{
public:
    TestSyncWakeupChild();
    virtual ~TestSyncWakeupChild();

protected:
    virtual mozilla::ipc::IPCResult RecvStart() override;

    virtual mozilla::ipc::IPCResult RecvNote1() override;

    virtual mozilla::ipc::IPCResult AnswerStackFrame() override;

    virtual mozilla::ipc::IPCResult RecvNote2() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    bool mDone;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSyncWakeup_h
