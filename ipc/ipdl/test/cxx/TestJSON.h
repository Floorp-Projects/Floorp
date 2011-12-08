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
    NS_OVERRIDE
    virtual bool
    RecvTest(const JSONVariant& i,
             JSONVariant* o);

    NS_OVERRIDE
    virtual PTestHandleParent* AllocPTestHandle()
    {
        return mKid = new TestHandleParent();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestHandle(PTestHandleParent* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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
    NS_OVERRIDE
    virtual bool
    RecvStart();

    NS_OVERRIDE
    virtual PTestHandleChild* AllocPTestHandle()
    {
        return mKid = new TestHandleChild();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestHandle(PTestHandleChild* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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
