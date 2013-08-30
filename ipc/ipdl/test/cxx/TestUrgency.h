#ifndef mozilla__ipdltest_TestUrgency_h
#define mozilla__ipdltest_TestUrgency_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestUrgencyParent.h"
#include "mozilla/_ipdltest/PTestUrgencyChild.h"

namespace mozilla {
namespace _ipdltest {


class TestUrgencyParent :
    public PTestUrgencyParent
{
public:
    TestUrgencyParent();
    virtual ~TestUrgencyParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

    bool RecvTest1(uint32_t *value);
    bool RecvTest2();
    bool RecvTest3(uint32_t *value);
    bool RecvTest4_Begin();
    bool RecvTest4_NestedSync();
    bool RecvFinalTest_Begin();

    bool ShouldContinueFromReplyTimeout() MOZ_OVERRIDE
    {
      return false;
    }
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

private:
    bool inreply_;
};


class TestUrgencyChild :
    public PTestUrgencyChild
{
public:
    TestUrgencyChild();
    virtual ~TestUrgencyChild();

    bool RecvStart();
    bool AnswerReply1(uint32_t *reply);
    bool AnswerReply2(uint32_t *reply);
    bool AnswerTest4_Reenter();
    bool AnswerFinalTest_Hang();

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (AbnormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    uint32_t test_;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestUrgency_h
