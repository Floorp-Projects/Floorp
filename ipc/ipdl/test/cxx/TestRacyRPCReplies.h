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
    virtual bool RecvA_() MOZ_OVERRIDE;

    virtual bool Answer_R(int* replyNum) MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
    virtual bool AnswerR_(int* replyNum) MOZ_OVERRIDE;

    virtual bool RecvChildTest() MOZ_OVERRIDE;

    virtual bool Recv_A() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
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
