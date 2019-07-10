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

class TestDataStructuresSub : public PTestDataStructuresSubParent,
                              public PTestDataStructuresSubChild {
 public:
  explicit TestDataStructuresSub(uint32_t i) : mI(i) {}
  virtual ~TestDataStructuresSub() {}
  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (Deletion != why) fail("unexpected destruction!");
  }
  uint32_t mI;
};

//-----------------------------------------------------------------------------
// Main actors

class TestDataStructuresParent : public PTestDataStructuresParent {
  friend class PTestDataStructuresParent;

 public:
  TestDataStructuresParent();
  virtual ~TestDataStructuresParent();

  static bool RunTestInProcesses() { return true; }
  static bool RunTestInThreads() { return true; }

  void Main();

 protected:
  PTestDataStructuresSubParent* AllocPTestDataStructuresSubParent(
      const int& i) {
    PTestDataStructuresSubParent* actor = new TestDataStructuresSub(i);
    mKids.AppendElement(actor);
    return actor;
  }

  bool DeallocPTestDataStructuresSubParent(PTestDataStructuresSubParent* actor);

  mozilla::ipc::IPCResult RecvTest1(nsTArray<int>&& i1, nsTArray<int>* o1);

  mozilla::ipc::IPCResult RecvTest2(
      nsTArray<PTestDataStructuresSubParent*>&& i1,
      nsTArray<PTestDataStructuresSubParent*>* o1);

  mozilla::ipc::IPCResult RecvTest3(const IntDouble& i1, const IntDouble& i2,
                                    IntDouble* o1, IntDouble* o2);

  mozilla::ipc::IPCResult RecvTest4(nsTArray<IntDouble>&& i1,
                                    nsTArray<IntDouble>* o1);

  mozilla::ipc::IPCResult RecvTest5(const IntDoubleArrays& i1,
                                    const IntDoubleArrays& i2,
                                    const IntDoubleArrays& i3,
                                    IntDoubleArrays* o1, IntDoubleArrays* o2,
                                    IntDoubleArrays* o3);

  mozilla::ipc::IPCResult RecvTest6(nsTArray<IntDoubleArrays>&& i1,
                                    nsTArray<IntDoubleArrays>* o1);

  mozilla::ipc::IPCResult RecvTest7_0(const ActorWrapper& i1, ActorWrapper* o1);

  mozilla::ipc::IPCResult RecvTest7(const Actors& i1, const Actors& i2,
                                    const Actors& i3, Actors* o1, Actors* o2,
                                    Actors* o3);

  mozilla::ipc::IPCResult RecvTest8(nsTArray<Actors>&& i1,
                                    nsTArray<Actors>* o1);

  mozilla::ipc::IPCResult RecvTest9(const Unions& i1, const Unions& i2,
                                    const Unions& i3, const Unions& i4,
                                    Unions* o1, Unions* o2, Unions* o3,
                                    Unions* o4);

  mozilla::ipc::IPCResult RecvTest10(nsTArray<Unions>&& i1,
                                     nsTArray<Unions>* o1);

  mozilla::ipc::IPCResult RecvTest11(const SIntDouble& i, SIntDouble* o);

  mozilla::ipc::IPCResult RecvTest12(const SIntDoubleArrays& i,
                                     SIntDoubleArrays* o);

  mozilla::ipc::IPCResult RecvTest13(const SActors& i, SActors* o);

  mozilla::ipc::IPCResult RecvTest14(const Structs& i, Structs* o);

  mozilla::ipc::IPCResult RecvTest15(
      const WithStructs& i1, const WithStructs& i2, const WithStructs& i3,
      const WithStructs& i4, const WithStructs& i5, WithStructs* o1,
      WithStructs* o2, WithStructs* o3, WithStructs* o4, WithStructs* o5);

  mozilla::ipc::IPCResult RecvTest16(const WithUnions& i, WithUnions* o);

  mozilla::ipc::IPCResult RecvTest17(nsTArray<Op>&& sa);

  mozilla::ipc::IPCResult RecvTest18(nsTArray<nsIntRegion>&& ra);

  mozilla::ipc::IPCResult RecvDummy(const ShmemUnion& su, ShmemUnion* rsu) {
    *rsu = su;
    return IPC_OK();
  }

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
    passed("ok");
    QuitParent();
  }

 private:
  nsTArray<PTestDataStructuresSubParent*> mKids;
};

class TestDataStructuresChild : public PTestDataStructuresChild {
  friend class PTestDataStructuresChild;

 public:
  TestDataStructuresChild();
  virtual ~TestDataStructuresChild();

 protected:
  PTestDataStructuresSubChild* AllocPTestDataStructuresSubChild(const int& i) {
    PTestDataStructuresSubChild* actor = new TestDataStructuresSub(i);
    mKids.AppendElement(actor);
    return actor;
  }

  bool DeallocPTestDataStructuresSubChild(PTestDataStructuresSubChild* actor) {
    delete actor;
    return true;
  }

  mozilla::ipc::IPCResult RecvStart();

  virtual void ActorDestroy(ActorDestroyReason why) override {
    if (NormalShutdown != why) fail("unexpected destruction!");
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

  nsTArray<PTestDataStructuresSubChild*> mKids;
};

}  // namespace _ipdltest
}  // namespace mozilla

#endif  // ifndef mozilla__ipdltest_TestDataStructures_h
