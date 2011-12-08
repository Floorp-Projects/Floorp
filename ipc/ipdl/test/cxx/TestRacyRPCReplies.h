#ifndef mozilla__ipdltest_TestRacyRPCReplies_h
#define mozilla__ipdltest_TestRacyRPCReplies_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestRacyRPCRepliesParent.h"
#include "mozilla/_ipdltest/PTestRacyRPCRepliesChild.h"

namespace mozilla {
namespace _ipdltest {


class TestRacyRPCRepliesParent :
    public PTestRacyRPCRepliesParent
{
public:
    TestRacyRPCRepliesParent();
    virtual ~TestRacyRPCRepliesParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    NS_OVERRIDE
    virtual bool RecvA_();

    NS_OVERRIDE
    virtual bool Answer_R(int* replyNum);

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

private:
    int mReplyNum;
};


class TestRacyRPCRepliesChild :
    public PTestRacyRPCRepliesChild
{
public:
    TestRacyRPCRepliesChild();
    virtual ~TestRacyRPCRepliesChild();

protected:
    NS_OVERRIDE
    virtual bool AnswerR_(int* replyNum);

    NS_OVERRIDE
    virtual bool RecvChildTest();

    NS_OVERRIDE
    virtual bool Recv_A();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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


#endif // ifndef mozilla__ipdltest_TestRacyRPCReplies_h
