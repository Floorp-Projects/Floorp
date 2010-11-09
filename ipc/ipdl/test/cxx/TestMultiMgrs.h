#ifndef mozilla__ipdltest_TestMultiMgrs_h
#define mozilla__ipdltest_TestMultiMgrs_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestMultiMgrsParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsBottomChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsLeftChild.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightParent.h"
#include "mozilla/_ipdltest/PTestMultiMgrsRightChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent side
//

class TestMultiMgrsBottomParent :
    public PTestMultiMgrsBottomParent
{
public:
    TestMultiMgrsBottomParent() { }
    virtual ~TestMultiMgrsBottomParent() { }
};

class TestMultiMgrsLeftParent :
    public PTestMultiMgrsLeftParent
{
public:
    TestMultiMgrsLeftParent() { }
    virtual ~TestMultiMgrsLeftParent() { }

    bool HasChild(TestMultiMgrsBottomParent* c)
    {
        return ManagedPTestMultiMgrsBottomParent().Contains(c);
    }

protected:
    NS_OVERRIDE
    virtual PTestMultiMgrsBottomParent* AllocPTestMultiMgrsBottom()
    {
        return new TestMultiMgrsBottomParent();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsBottom(PTestMultiMgrsBottomParent* actor)
    {
        delete actor;
        return true;
    }
};

class TestMultiMgrsRightParent :
    public PTestMultiMgrsRightParent
{
public:
    TestMultiMgrsRightParent() { }
    virtual ~TestMultiMgrsRightParent() { }

    bool HasChild(TestMultiMgrsBottomParent* c)
    {
        return ManagedPTestMultiMgrsBottomParent().Contains(c);
    }

protected:
    NS_OVERRIDE
    virtual PTestMultiMgrsBottomParent* AllocPTestMultiMgrsBottom()
    {
        return new TestMultiMgrsBottomParent();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsBottom(PTestMultiMgrsBottomParent* actor)
    {
        delete actor;
        return true;
    }
};

class TestMultiMgrsParent :
    public PTestMultiMgrsParent
{
public:
    TestMultiMgrsParent() { }
    virtual ~TestMultiMgrsParent() { }

    void Main();

protected:
    NS_OVERRIDE
    virtual bool RecvOK();

    NS_OVERRIDE
    virtual PTestMultiMgrsLeftParent* AllocPTestMultiMgrsLeft()
    {
        return new TestMultiMgrsLeftParent();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsLeft(PTestMultiMgrsLeftParent* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual PTestMultiMgrsRightParent* AllocPTestMultiMgrsRight()
    {
        return new TestMultiMgrsRightParent();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsRight(PTestMultiMgrsRightParent* actor)
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
};

//-----------------------------------------------------------------------------
// Child side
//

class TestMultiMgrsBottomChild :
    public PTestMultiMgrsBottomChild
{
public:
    TestMultiMgrsBottomChild() { }
    virtual ~TestMultiMgrsBottomChild() { }
};

class TestMultiMgrsLeftChild :
    public PTestMultiMgrsLeftChild
{
public:
    TestMultiMgrsLeftChild() { }
    virtual ~TestMultiMgrsLeftChild() { }

    bool HasChild(PTestMultiMgrsBottomChild* c)
    {
        return ManagedPTestMultiMgrsBottomChild().Contains(c);
    }

protected:
    NS_OVERRIDE
    virtual bool RecvPTestMultiMgrsBottomConstructor(PTestMultiMgrsBottomChild* actor);

    NS_OVERRIDE
    virtual PTestMultiMgrsBottomChild* AllocPTestMultiMgrsBottom()
    {
        return new TestMultiMgrsBottomChild();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsBottom(PTestMultiMgrsBottomChild* actor)
    {
        delete actor;
        return true;
    }
};

class TestMultiMgrsRightChild :
    public PTestMultiMgrsRightChild
{
public:
    TestMultiMgrsRightChild() { }
    virtual ~TestMultiMgrsRightChild() { }

    bool HasChild(PTestMultiMgrsBottomChild* c)
    {
        return ManagedPTestMultiMgrsBottomChild().Contains(c);
    }

protected:
    NS_OVERRIDE
    virtual bool RecvPTestMultiMgrsBottomConstructor(PTestMultiMgrsBottomChild* actor);

    NS_OVERRIDE
    virtual PTestMultiMgrsBottomChild* AllocPTestMultiMgrsBottom()
    {
        return new TestMultiMgrsBottomChild();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsBottom(PTestMultiMgrsBottomChild* actor)
    {
        delete actor;
        return true;
    }
};

class TestMultiMgrsChild :
    public PTestMultiMgrsChild
{
public:
    TestMultiMgrsChild() { }
    virtual ~TestMultiMgrsChild() { }

    void Main();

    PTestMultiMgrsBottomChild* mBottomL;
    PTestMultiMgrsBottomChild* mBottomR;

protected:
    NS_OVERRIDE
    virtual bool RecvCheck();

    NS_OVERRIDE
    virtual PTestMultiMgrsLeftChild* AllocPTestMultiMgrsLeft()
    {
        return new TestMultiMgrsLeftChild();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsLeft(PTestMultiMgrsLeftChild* actor)
    {
        delete actor;
        return true;
    }

    NS_OVERRIDE
    virtual PTestMultiMgrsRightChild* AllocPTestMultiMgrsRight()
    {
        return new TestMultiMgrsRightChild();
    }

    NS_OVERRIDE
    virtual bool DeallocPTestMultiMgrsRight(PTestMultiMgrsRightChild* actor)
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
        QuitChild();
    }
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestMultiMgrs_h
