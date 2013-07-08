#ifndef mozilla__ipdltest_TestDataStructures_h
#define mozilla__ipdltest_TestDataStructures_h 1

#include "mozilla/_ipdltest/IPDLUnitTests.h"

#include "mozilla/_ipdltest/PTestDataStructuresParent.h"
#include "mozilla/_ipdltest/PTestDataStructuresChild.h"

#include "mozilla/_ipdltest/PTestDataStructuresSubParent.h"
#include "mozilla/_ipdltest/PTestDataStructuresSubChild.h"

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Subprotocol actors

class TestDataStructuresSub :
        public PTestDataStructuresSubParent,
        public PTestDataStructuresSubChild
{
public:
    TestDataStructuresSub(uint32_t i) : mI(i)
    { }
    virtual ~TestDataStructuresSub()
    { }
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
      if (Deletion != why)
        fail("unexpected destruction!");
    }
  uint32_t mI;
};

//-----------------------------------------------------------------------------
// Main actors

class TestDataStructuresParent :
    public PTestDataStructuresParent
{
public:
    TestDataStructuresParent();
    virtual ~TestDataStructuresParent();

    static bool RunTestInProcesses() { return true; }
    static bool RunTestInThreads() { return true; }

    void Main();

protected:
    virtual PTestDataStructuresSubParent* AllocPTestDataStructuresSubParent(const int& i) MOZ_OVERRIDE
    {
        PTestDataStructuresSubParent* actor = new TestDataStructuresSub(i);
        mKids.AppendElement(actor);
        return actor;
    }

    virtual bool DeallocPTestDataStructuresSubParent(PTestDataStructuresSubParent* actor) MOZ_OVERRIDE;

    virtual bool RecvTest1(
            const InfallibleTArray<int>& i1,
            InfallibleTArray<int>* o1) MOZ_OVERRIDE;

    virtual bool RecvTest2(
            const InfallibleTArray<PTestDataStructuresSubParent*>& i1,
            InfallibleTArray<PTestDataStructuresSubParent*>* o1) MOZ_OVERRIDE;

    virtual bool RecvTest3(
            const IntDouble& i1,
            const IntDouble& i2,
            IntDouble* o1,
            IntDouble* o2) MOZ_OVERRIDE;

    virtual bool RecvTest4(
            const InfallibleTArray<IntDouble>& i1,
            InfallibleTArray<IntDouble>* o1) MOZ_OVERRIDE;

    virtual bool RecvTest5(
            const IntDoubleArrays& i1,
            const IntDoubleArrays& i2,
            const IntDoubleArrays& i3,
            IntDoubleArrays* o1,
            IntDoubleArrays* o2,
            IntDoubleArrays* o3) MOZ_OVERRIDE;

    virtual bool RecvTest6(
            const InfallibleTArray<IntDoubleArrays>& i1,
            InfallibleTArray<IntDoubleArrays>* o1) MOZ_OVERRIDE;


    virtual bool RecvTest7_0(const ActorWrapper& i1,
                             ActorWrapper* o1) MOZ_OVERRIDE;

    virtual bool RecvTest7(
            const Actors& i1,
            const Actors& i2,
            const Actors& i3,
            Actors* o1,
            Actors* o2,
            Actors* o3) MOZ_OVERRIDE;

    virtual bool RecvTest8(
            const InfallibleTArray<Actors>& i1,
            InfallibleTArray<Actors>* o1) MOZ_OVERRIDE;

    virtual bool RecvTest9(
            const Unions& i1,
            const Unions& i2,
            const Unions& i3,
            const Unions& i4,
            Unions* o1,
            Unions* o2,
            Unions* o3,
            Unions* o4) MOZ_OVERRIDE;

    virtual bool RecvTest10(
            const InfallibleTArray<Unions>& i1,
            InfallibleTArray<Unions>* o1) MOZ_OVERRIDE;

    virtual bool RecvTest11(
            const SIntDouble& i,
            SIntDouble* o) MOZ_OVERRIDE;

    virtual bool RecvTest12(
            const SIntDoubleArrays& i,
            SIntDoubleArrays* o) MOZ_OVERRIDE;

    virtual bool RecvTest13(
            const SActors& i,
            SActors* o) MOZ_OVERRIDE;

    virtual bool RecvTest14(
            const Structs& i,
            Structs* o) MOZ_OVERRIDE;

    virtual bool RecvTest15(
            const WithStructs& i1,
            const WithStructs& i2,
            const WithStructs& i3,
            const WithStructs& i4,
            const WithStructs& i5,
            WithStructs* o1,
            WithStructs* o2,
            WithStructs* o3,
            WithStructs* o4,
            WithStructs* o5) MOZ_OVERRIDE;

    virtual bool RecvTest16(
            const WithUnions& i,
            WithUnions* o) MOZ_OVERRIDE;

    virtual bool RecvTest17(const InfallibleTArray<Op>& sa) MOZ_OVERRIDE;

    virtual bool RecvTest18(const InfallibleTArray<nsIntRegion>& ra) MOZ_OVERRIDE;

    virtual bool RecvDummy(const ShmemUnion& su, ShmemUnion* rsu) MOZ_OVERRIDE
    {
        *rsu = su;
        return true;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");  
        passed("ok");
        QuitParent();
    }

private:
    InfallibleTArray<PTestDataStructuresSubParent*> mKids;
};


class TestDataStructuresChild :
    public PTestDataStructuresChild
{
public:
    TestDataStructuresChild();
    virtual ~TestDataStructuresChild();

protected:
    virtual PTestDataStructuresSubChild* AllocPTestDataStructuresSubChild(const int& i) MOZ_OVERRIDE
    {
        PTestDataStructuresSubChild* actor = new TestDataStructuresSub(i);
        mKids.AppendElement(actor);
        return actor;
    }

    virtual bool DeallocPTestDataStructuresSubChild(PTestDataStructuresSubChild* actor) MOZ_OVERRIDE
    {
        delete actor;
        return true;
    }

    virtual bool RecvStart() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
    {
        if (NormalShutdown != why)
            fail("unexpected destruction!");
        QuitChild();
    }

private:
    void Test1();
    void Test2();
    void Test3();
    void Test4();
    void Test5();
    void Test6();
    void Test7_0();
    void Test7();
    void Test8();
    void Test9();
    void Test10();
    void Test11();
    void Test12();
    void Test13();
    void Test14();
    void Test15();
    void Test16();
    void Test17();
    void Test18();

    InfallibleTArray<PTestDataStructuresSubChild*> mKids;
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestDataStructures_h
