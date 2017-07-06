#ifndef mozilla__ipdltest_TestJSON_h
#define mozilla__ipdltest_TestJSON_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestJSONParent.h"
#include "mozilla/_ipdltest/PTestJSONChild.h"

#include "mozilla/_ipdltest/PTestHandleParent.h"
#include "mozilla/_ipdltest/PTestHandleChild.h"

namespace mozilla {
namespace _ipdltest {

class TestHandleParent :
    public PTestHandleParent
{
public:
    TestHandleParent() { }
    virtual ~TestHandleParent() { }

protected:
    virtual void ActorDestroy(ActorDestroyReason why) override {}
};

class TestJSONParent :
    public PTestJSONParent
{
public:
    TestJSONParent() { }
    virtual ~TestJSONParent() { }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual mozilla::ipc::IPCResult
    RecvTest(const JSONVariant& i,
             JSONVariant* o) override;

    virtual PTestHandleParent* AllocPTestHandleParent() override
    {
        return mKid = new TestHandleParent();
    }

    virtual bool DeallocPTestHandleParent(PTestHandleParent* actor) override
    {
        delete actor;
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        passed("ok");
        QuitParent();
    }

    PTestHandleParent* mKid;
};


class TestHandleChild :
    public PTestHandleChild
{
public:
    TestHandleChild() { }
    virtual ~TestHandleChild() { }
};

class TestJSONChild :
    public PTestJSONChild
{
public:
    TestJSONChild() { }
    virtual ~TestJSONChild() { }

protected:
    virtual mozilla::ipc::IPCResult
    RecvStart() override;

    virtual PTestHandleChild* AllocPTestHandleChild() override
    {
        return mKid = new TestHandleChild();
    }

    virtual bool DeallocPTestHandleChild(PTestHandleChild* actor) override
    {
        delete actor;
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) override
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

    PTestHandleChild* mKid;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestJSON_h
