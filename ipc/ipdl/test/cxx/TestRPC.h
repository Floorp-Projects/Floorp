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

    bool RecvTest1_Start(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest1_InnerEvent(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest2_Start() MOZ_OVERRIDE;
    bool RecvTest2_Msg2() MOZ_OVERRIDE;
    bool RecvTest2_FirstUrgent() MOZ_OVERRIDE;
    bool RecvTest2_SecondUrgent() MOZ_OVERRIDE;
    bool RecvTest3_Start(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest3_InnerEvent(uint32_t* aResult) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestRPCChild :
    public PTestRPCChild
{
public:
    TestRPCChild();
    virtual ~TestRPCChild();

    bool RecvStart() MOZ_OVERRIDE;
    bool RecvTest1_InnerQuery(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest1_NoReenter(uint32_t* aResult) MOZ_OVERRIDE;
    bool RecvTest2_Msg1() MOZ_OVERRIDE;
    bool RecvTest2_Msg3() MOZ_OVERRIDE;
    bool RecvTest3_WakeUp(uint32_t* aResult) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    bool reentered_;
    bool resolved_first_cpow_;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRPC_h
