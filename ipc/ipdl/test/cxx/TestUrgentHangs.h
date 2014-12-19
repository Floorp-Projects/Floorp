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
    void FinishTesting();

    bool RecvTest1_2();
    bool RecvTestInner();

    bool ShouldContinueFromReplyTimeout() MOZ_OVERRIDE
    {
      return false;
    }
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        passed("ok");
        QuitParent();
    }
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

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestUrgentHangs_h
