#ifndef mozilla__ipdltest_TestCancel_h
#define mozilla__ipdltest_TestCancel_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestCancelParent.h"
#include "mozilla/_ipdltest/PTestCancelChild.h"

namespace mozilla {
namespace _ipdltest {


class TestCancelParent :
    public PTestCancelParent
{
public:
    TestCancelParent();
    virtual ~TestCancelParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return false; }

    void Main();

    virtual mozilla::ipc::IPCResult RecvDone1() override;
    virtual mozilla::ipc::IPCResult RecvTest2_1() override;
    virtual mozilla::ipc::IPCResult RecvStart3() override;
    virtual mozilla::ipc::IPCResult RecvTest3_2() override;
    virtual mozilla::ipc::IPCResult RecvDone() override;

    virtual mozilla::ipc::IPCResult RecvCheckParent(uint32_t *reply) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        passed("ok");
        QuitParent();
    }
};


class TestCancelChild :
    public PTestCancelChild
{
public:
    TestCancelChild();
    virtual ~TestCancelChild();

    virtual mozilla::ipc::IPCResult RecvTest1_1() override;
    virtual mozilla::ipc::IPCResult RecvStart2() override;
    virtual mozilla::ipc::IPCResult RecvTest2_2() override;
    virtual mozilla::ipc::IPCResult RecvTest3_1() override;

    virtual mozilla::ipc::IPCResult RecvCheckChild(uint32_t *reply) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestCancel_h
