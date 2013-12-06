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
    static bool RunTestInThreads() { return true; }

    void Main();

    bool AnswerTest1_Start(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest1_InnerEvent(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest2_Start() MOZ_OVERRIDE;
    bool AnswerTest2_OutOfOrder() MOZ_OVERRIDE;
    bool RecvTest3_Start(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest3_InnerEvent(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest4_Start(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest4_Inner(uint32_t* aResult) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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

    bool RecvStart() MOZ_OVERRIDE;
    bool AnswerTest1_InnerQuery(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest1_NoReenter(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest2_FirstUrgent() MOZ_OVERRIDE;
    bool AnswerTest2_SecondUrgent() MOZ_OVERRIDE;
    bool AnswerTest3_WakeUp(uint32_t* aResult) MOZ_OVERRIDE;
    bool AnswerTest4_WakeUp(uint32_t* aResult) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPC_h
