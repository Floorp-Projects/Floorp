#ifndef mozilla__ipdltest_TestSyncHang_h
#define mozilla__ipdltest_TestSyncHang_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSyncHangParent.h"
#include "mozilla/_ipdltest/PTestSyncHangChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSyncHangParent :
    public PTestSyncHangParent
{
public:
    TestSyncHangParent();
    virtual ~TestSyncHangParent();

    static bool RunTestInProcesses() { return true; }
    // FIXME/bug 703323 Could work if modified
    static bool RunTestInThreads() { return false; }

    void Main();

protected:    
    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};


class TestSyncHangChild :
    public PTestSyncHangChild
{
public:
    TestSyncHangChild();
    virtual ~TestSyncHangChild();

protected:
    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSyncHang_h
