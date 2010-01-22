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

    void Main();

protected:    
    NS_OVERRIDE
    virtual bool RecvA();

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestRacyRPCRepliesChild :
    public PTestRacyRPCRepliesChild
{
public:
    TestRacyRPCRepliesChild();
    virtual ~TestRacyRPCRepliesChild();

protected:
    NS_OVERRIDE
    virtual bool AnswerR(int* replyNum);

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
