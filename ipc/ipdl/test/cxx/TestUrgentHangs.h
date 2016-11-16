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

    mozilla::ipc::IPCResult RecvTest1_2() override;
    mozilla::ipc::IPCResult RecvTestInner() override;
    mozilla::ipc::IPCResult RecvTestInnerUrgent() override;

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

    mozilla::ipc::IPCResult RecvTest1_1() override;
    mozilla::ipc::IPCResult RecvTest1_3() override;
    mozilla::ipc::IPCResult RecvTest2() override;
    mozilla::ipc::IPCResult RecvTest3() override;
    mozilla::ipc::IPCResult RecvTest4() override;
    mozilla::ipc::IPCResult RecvTest4_1() override;
    mozilla::ipc::IPCResult RecvTest5() override;
    mozilla::ipc::IPCResult RecvTest5_1() override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestUrgentHangs_h
