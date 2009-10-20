#ifndef mozilla_ipdltest_TestDesc_h
#define mozilla_ipdltest_TestDesc_h

#include "mozilla/_ipdltest/PTestDescParent.h"
#include "mozilla/_ipdltest/PTestDescChild.h"

#include "mozilla/_ipdltest/PTestDescSubParent.h"
#include "mozilla/_ipdltest/PTestDescSubChild.h"

#include "mozilla/_ipdltest/PTestDescSubsubParent.h"
#include "mozilla/_ipdltest/PTestDescSubsubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Top-level
//
class TestDescParent :
    public PTestDescParent
{
public:
    TestDescParent() { }
    virtual ~TestDescParent() { }

    void Main();

    virtual bool RecvOk(PTestDescSubsubParent* a);

protected:
    virtual PTestDescSubParent* AllocPTestDescSub();
    virtual bool DeallocPTestDescSub(PTestDescSubParent* actor);
};


class TestDescChild :
    public PTestDescChild
{
public:
    TestDescChild() { }
    virtual ~TestDescChild() { }

protected:
    virtual PTestDescSubChild* AllocPTestDescSub();
    virtual bool DeallocPTestDescSub(PTestDescSubChild* actor);
    virtual bool RecvTest(PTestDescSubsubChild* a);
};


//-----------------------------------------------------------------------------
// First descendent
//
class TestDescSubParent :
    public PTestDescSubParent
{
public:
    TestDescSubParent() { }
    virtual ~TestDescSubParent() { }

protected:
    virtual PTestDescSubsubParent* AllocPTestDescSubsub();
    virtual bool DeallocPTestDescSubsub(PTestDescSubsubParent* actor);
};


class TestDescSubChild :
    public PTestDescSubChild
{
public:
    TestDescSubChild() { }
    virtual ~TestDescSubChild() { }

protected:
    virtual PTestDescSubsubChild* AllocPTestDescSubsub();
    virtual bool DeallocPTestDescSubsub(PTestDescSubsubChild* actor);
};


//-----------------------------------------------------------------------------
// Grand-descendent
//
class TestDescSubsubParent :
    public PTestDescSubsubParent
{
public:
    TestDescSubsubParent() { }
    virtual ~TestDescSubsubParent() { }
};

class TestDescSubsubChild :
    public PTestDescSubsubChild
{
public:
    TestDescSubsubChild() { }
    virtual ~TestDescSubsubChild() { }
};


}
}

#endif // ifndef mozilla_ipdltest_TestDesc_h
