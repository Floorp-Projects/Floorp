#ifndef mozilla__ipdltest_TestManyChildAllocs_h
#define mozilla__ipdltest_TestManyChildAllocs_h 1

#include "mozilla/_ipdltest/PTestManyChildAllocsParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsChild.h"

#include "mozilla/_ipdltest/PTestManyChildAllocsSubParent.h"
#include "mozilla/_ipdltest/PTestManyChildAllocsSubChild.h"

namespace mozilla {
namespace _ipdltest {

// top-level protocol

class TestManyChildAllocsParent :
    public PTestManyChildAllocsParent
{
public:
    TestManyChildAllocsParent();
    virtual ~TestManyChildAllocsParent();

    void Main();

protected:
    virtual bool RecvDone();
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubParent* __a);
    virtual PTestManyChildAllocsSubParent* AllocPTestManyChildAllocsSub();
};


class TestManyChildAllocsChild :
    public PTestManyChildAllocsChild
{
public:
    TestManyChildAllocsChild();
    virtual ~TestManyChildAllocsChild();

protected:
    virtual bool RecvGo();
    virtual bool DeallocPTestManyChildAllocsSub(PTestManyChildAllocsSubChild* __a);
    virtual PTestManyChildAllocsSubChild* AllocPTestManyChildAllocsSub();
};


// do-nothing sub-protocol actors

class TestManyChildAllocsSubParent :
    public PTestManyChildAllocsSubParent
{
public:
    TestManyChildAllocsSubParent() { }
    virtual ~TestManyChildAllocsSubParent() { }

protected:
    virtual bool RecvHello() { return true; }
};


class TestManyChildAllocsSubChild :
    public PTestManyChildAllocsSubChild
{
public:
    TestManyChildAllocsSubChild() { }
    virtual ~TestManyChildAllocsSubChild() { }
};



} // namepsace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestManyChildAllocs_h
