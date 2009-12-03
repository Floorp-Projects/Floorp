#ifndef mozilla__ipdltest_TestArrays_h
#define mozilla__ipdltest_TestArrays_h 1


#include "mozilla/_ipdltest/PTestArraysParent.h"
#include "mozilla/_ipdltest/PTestArraysChild.h"

#include "mozilla/_ipdltest/PTestArraysSubParent.h"
#include "mozilla/_ipdltest/PTestArraysSubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Subprotocol actors

class TestArraysSub :
        public PTestArraysSubParent,
        public PTestArraysSubChild
{
public:
    TestArraysSub(uint32 i) : mI(i)
    { }
    virtual ~TestArraysSub()
    { }
    uint32 mI;
};

//-----------------------------------------------------------------------------
// Main actors

class TestArraysParent :
    public PTestArraysParent
{
public:
    TestArraysParent();
    virtual ~TestArraysParent();

    void Main();

protected:
    virtual PTestArraysSubParent* AllocPTestArraysSub(const int& i)
    {
        PTestArraysSubParent* actor = new TestArraysSub(i);
        mKids.AppendElement(actor);
        return actor;
    }

    virtual bool DeallocPTestArraysSub(PTestArraysSubParent* actor);

    virtual bool RecvTest1(
            const nsTArray<int>& i1,
            nsTArray<int>* o1);

    virtual bool RecvTest2(
            const nsTArray<PTestArraysSubParent*>& i1,
            nsTArray<PTestArraysSubParent*>* o1);

    virtual bool RecvTest3(
            const IntDouble& i1,
            const IntDouble& i2,
            IntDouble* o1,
            IntDouble* o2);

    virtual bool RecvTest4(
            const nsTArray<IntDouble>& i1,
            nsTArray<IntDouble>* o1);

    virtual bool RecvTest5(
            const IntDoubleArrays& i1,
            const IntDoubleArrays& i2,
            const IntDoubleArrays& i3,
            IntDoubleArrays* o1,
            IntDoubleArrays* o2,
            IntDoubleArrays* o3);

    virtual bool RecvTest6(
            const nsTArray<IntDoubleArrays>& i1,
            nsTArray<IntDoubleArrays>* o1);

    virtual bool RecvTest7(
            const Actors& i1,
            const Actors& i2,
            const Actors& i3,
            Actors* o1,
            Actors* o2,
            Actors* o3);

    virtual bool RecvTest8(
            const nsTArray<Actors>& i1,
            nsTArray<Actors>* o1);
    virtual bool RecvTest9(
            const Unions& i1,
            const Unions& i2,
            const Unions& i3,
            const Unions& i4,
            Unions* o1,
            Unions* o2,
            Unions* o3,
            Unions* o4);
    virtual bool RecvTest10(
            const nsTArray<Unions>& i1,
            nsTArray<Unions>* o1);

private:
    nsTArray<PTestArraysSubParent*> mKids;
};


class TestArraysChild :
    public PTestArraysChild
{
public:
    TestArraysChild();
    virtual ~TestArraysChild();

protected:
    virtual PTestArraysSubChild* AllocPTestArraysSub(const int& i)
    {
        PTestArraysSubChild* actor = new TestArraysSub(i);
        mKids.AppendElement(actor);
        return actor;
    }
    virtual bool DeallocPTestArraysSub(PTestArraysSubChild* actor)
    {
        delete actor;
        return true;
    }

    virtual bool RecvStart();

private:
    void Test1();
    void Test2();
    void Test3();
    void Test4();
    void Test5();
    void Test6();
    void Test7();
    void Test8();
    void Test9();
    void Test10();

    nsTArray<PTestArraysSubChild*> mKids;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestArrays_h
