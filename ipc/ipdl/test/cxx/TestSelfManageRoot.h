#ifndef mozilla__ipdltest_TestSelfManageRoot_h
#define mozilla__ipdltest_TestSelfManageRoot_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestSelfManageRootParent.h"
#include "mozilla/_ipdltest/PTestSelfManageRootChild.h"
#include "mozilla/_ipdltest/PTestSelfManageParent.h"
#include "mozilla/_ipdltest/PTestSelfManageChild.h"


namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side

class TestSelfManageParent :
    public PTestSelfManageParent
{
public:
    TestSelfManageParent() {
        MOZ_COUNT_CTOR(TestSelfManageParent);
    }
    virtual ~TestSelfManageParent() {
        MOZ_COUNT_DTOR(TestSelfManageParent);
    }

    ActorDestroyReason mWhy;

protected:    
    virtual PTestSelfManageParent* AllocPTestSelfManage() MOZ_OVERRIDE {
        return new TestSelfManageParent();
    }

    virtual bool DeallocPTestSelfManage(PTestSelfManageParent* a) MOZ_OVERRIDE {
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE {
        mWhy = why;
    }
};

class TestSelfManageRootParent :
    public PTestSelfManageRootParent
{
public:
    TestSelfManageRootParent() {
        MOZ_COUNT_CTOR(TestSelfManageRootParent);
    }
    virtual ~TestSelfManageRootParent() {
        MOZ_COUNT_DTOR(TestSelfManageRootParent);
    }

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:    
    virtual PTestSelfManageParent* AllocPTestSelfManage() MOZ_OVERRIDE {
        return new TestSelfManageParent();
    }

    virtual bool DeallocPTestSelfManage(PTestSelfManageParent* a) MOZ_OVERRIDE {
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }
};

//-----------------------------------------------------------------------------
// Child side

class TestSelfManageChild :
    public PTestSelfManageChild
{
public:
    TestSelfManageChild() {
        MOZ_COUNT_CTOR(TestSelfManageChild);
    }
    virtual ~TestSelfManageChild() {
        MOZ_COUNT_DTOR(TestSelfManageChild);
    }

protected:    
    virtual PTestSelfManageChild* AllocPTestSelfManage() MOZ_OVERRIDE {
        return new TestSelfManageChild();
    }

    virtual bool DeallocPTestSelfManage(PTestSelfManageChild* a) MOZ_OVERRIDE {
        delete a;
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE { }
};

class TestSelfManageRootChild :
    public PTestSelfManageRootChild
{
public:
    TestSelfManageRootChild() {
        MOZ_COUNT_CTOR(TestSelfManageRootChild);
    }
    virtual ~TestSelfManageRootChild() {
        MOZ_COUNT_DTOR(TestSelfManageRootChild);
    }

    void Main();

protected:    
    virtual PTestSelfManageChild* AllocPTestSelfManage() MOZ_OVERRIDE {
        return new TestSelfManageChild();
    }

    virtual bool DeallocPTestSelfManage(PTestSelfManageChild* a) MOZ_OVERRIDE {
        delete a;
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        QuitChild();
    }
};

} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSelfManageRoot_h
