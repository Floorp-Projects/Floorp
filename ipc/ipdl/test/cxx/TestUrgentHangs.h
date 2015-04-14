#ifndef mozilla__ipdltest_TestUrgentHangs_h
#define mozilla__ipdltest_TestUrgentHangs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestUrgentHangsParent.h"
#include "mozilla/_ipdltest/PTestUrgentHangsChild.h"

namespace mozilla {
namespace _ipdltest {


class TestUrgentHangsParent :
    public PTestUrgentHangsParent
{
public:
    TestUrgentHangsParent();
    virtual ~TestUrgentHangsParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();
    void SecondStage();
    void ThirdStage();

    bool RecvTest1_2();
    bool RecvTestInner();
    bool RecvTestInnerUrgent();

    bool ShouldContinueFromReplyTimeout() override
    {
      return false;
    }
    virtual void ActorDestroy(ActorDestroyReason why) override
    {
	if (mInnerCount != 1) {
	    fail("wrong mInnerCount");
	}
	if (mInnerUrgentCount != 2) {
	    fail("wrong mInnerUrgentCount");
	}
        passed("ok");
        QuitParent();
    }

private:
    size_t mInnerCount, mInnerUrgentCount;
};


class TestUrgentHangsChild :
    public PTestUrgentHangsChild
{
public:
    TestUrgentHangsChild();
    virtual ~TestUrgentHangsChild();

    bool RecvTest1_1();
    bool RecvTest1_3();
    bool RecvTest2();
    bool RecvTest3();
    bool RecvTest4();
    bool RecvTest4_1();
    bool RecvTest5();
    bool RecvTest5_1();

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestUrgentHangs_h
