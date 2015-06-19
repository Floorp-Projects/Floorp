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

    virtual bool RecvDone1() override;
    virtual bool RecvTest2_1() override;
    virtual bool RecvStart3() override;
    virtual bool RecvTest3_2() override;
    virtual bool RecvDone() override;

    virtual bool RecvCheckParent(uint32_t *reply) override;

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

    virtual bool RecvTest1_1() override;
    virtual bool RecvStart2() override;
    virtual bool RecvTest2_2() override;
    virtual bool RecvTest3_1() override;

    virtual bool RecvCheckChild(uint32_t *reply) override;

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestCancel_h
