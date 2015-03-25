#ifndef mozilla__ipdltest_TestRacyInterruptReplies_h
#define mozilla__ipdltest_TestRacyInterruptReplies_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyInterruptRepliesParent.h"
#include "mozilla/_ipdltest/PTestRacyInterruptRepliesChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRacyInterruptRepliesParent :
    public PTestRacyInterruptRepliesParent
{
public:
    TestRacyInterruptRepliesParent();
    virtual ~TestRacyInterruptRepliesParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual bool RecvA_() override;

    virtual bool Answer_R(int* replyNum) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

private:
    int mReplyNum;
};


class TestRacyInterruptRepliesChild :
    public PTestRacyInterruptRepliesChild
{
public:
    TestRacyInterruptRepliesChild();
    virtual ~TestRacyInterruptRepliesChild();

protected:
    virtual bool AnswerR_(int* replyNum) override;

    virtual bool RecvChildTest() override;

    virtual bool Recv_A() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    int mReplyNum;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestRacyInterruptReplies_h
