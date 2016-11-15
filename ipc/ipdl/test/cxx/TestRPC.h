#ifndef mozilla__ipdltest_TestRPC_h
#define mozilla__ipdltest_TestRPC_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRPCParent.h"
#include "mozilla/_ipdltest/PTestRPCChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRPCParent :
    public PTestRPCParent
{
public:
    TestRPCParent();
    virtual ~TestRPCParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

    mozilla::ipc::IPCResult RecvTest1_Start(uint32_t* aResult) override;
    mozilla::ipc::IPCResult RecvTest1_InnerEvent(uint32_t* aResult) override;
    mozilla::ipc::IPCResult RecvTest2_Start() override;
    mozilla::ipc::IPCResult RecvTest2_OutOfOrder() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        if (!reentered_)
            fail("never processed raced RPC call!");
        if (!resolved_first_cpow_)
            fail("never resolved first CPOW!");
        passed("ok");
        QuitParent();
    }

private:
    bool reentered_;
    bool resolved_first_cpow_;
};


class TestRPCChild :
    public PTestRPCChild
{
public:
    TestRPCChild();
    virtual ~TestRPCChild();

    mozilla::ipc::IPCResult RecvStart() override;
    mozilla::ipc::IPCResult RecvTest1_InnerQuery(uint32_t* aResult) override;
    mozilla::ipc::IPCResult RecvTest1_NoReenter(uint32_t* aResult) override;
    mozilla::ipc::IPCResult RecvTest2_FirstUrgent() override;
    mozilla::ipc::IPCResult RecvTest2_SecondUrgent() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPC_h
