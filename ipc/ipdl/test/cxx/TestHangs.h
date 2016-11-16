#ifndef mozilla__ipdltest_TestHangs_h
#define mozilla__ipdltest_TestHangs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestHangsParent.h"
#include "mozilla/_ipdltest/PTestHangsChild.h"

namespace mozilla {
namespace _ipdltest {


class TestHangsParent :
    public PTestHangsParent
{
public:
    TestHangsParent();
    virtual ~TestHangsParent();

    static bool RunTestInProcesses() { return true; }

    // FIXME/bug 703320 Disabled because parent kills child proc, not
    //                  clear how that should work in threads.
    static bool RunTestInThreads() { return false; }

    void Main();

protected:
    virtual bool ShouldContinueFromReplyTimeout() override;

    virtual mozilla::ipc::IPCResult RecvNonce() override {
        return IPC_OK();
    }

    virtual mozilla::ipc::IPCResult AnswerStackFrame() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

    void CleanUp();

    bool mDetectedHang;
};


class TestHangsChild :
    public PTestHangsChild
{
public:
    TestHangsChild();
    virtual ~TestHangsChild();

protected:
    virtual mozilla::ipc::IPCResult RecvStart() override {
        if (!SendNonce())
            fail("sending Nonce");
        return IPC_OK();
    }

    virtual mozilla::ipc::IPCResult AnswerStackFrame() override
    {
        if (CallStackFrame())
            fail("should have failed");
        return IPC_OK();
    }

    virtual mozilla::ipc::IPCResult AnswerHang() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestHangs_h
