#include "TestDataStructures.h"

#include "mozilla/Unused.h"

#include "IPDLUnitTests.h"  // fail etc.

typedef nsTArray<nsIntRegion> RegionArray;

namespace mozilla {
namespace _ipdltest {

static const uint32_t nactors = 10;

#define test_assert(_cond, _msg) \
  if (!(_cond)) fail(_msg)

template <typename T>
static void assert_arrays_equal(const nsTArray<T>& a, const nsTArray<T>& b) {
  test_assert(a == b, "arrays equal");
}

inline static TestDataStructuresSub& Cast(PTestDataStructuresSubParent* a) {
  return *static_cast<TestDataStructuresSub*>(a);
}

//-----------------------------------------------------------------------------
// parent

TestDataStructuresParent::TestDataStructuresParent() {
  MOZ_COUNT_CTOR(TestDataStructuresParent);
}

TestDataStructuresParent::~TestDataStructuresParent() {
  MOZ_COUNT_DTOR(TestDataStructuresParent);
}

void TestDataStructuresParent::Main() {
  for (uint32_t i = 0; i < nactors; ++i)
    if (!SendPTestDataStructuresSubConstructor(i)) fail("can't alloc actor");

  if (!SendStart()) fail("can't send Start()");
}

bool TestDataStructuresParent::DeallocPTestDataStructuresSubParent(
    PTestDataStructuresSubParent* actor) {
  test_assert(Cast(actor).mI == Cast(mKids[0]).mI, "dtor sent to wrong actor");
  mKids.RemoveElementAt(0);
  delete actor;
  if (mKids.Length() > 0) return true;

  return true;
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest1(nsTArray<int>&& ia,
                                                            nsTArray<int>* oa) {
  test_assert(5 == ia.Length(), "wrong length");
  for (int i = 0; i < 5; ++i) test_assert(i == ia[i], "wrong value");

  *oa = ia;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest2(
    nsTArray<PTestDataStructuresSubParent*>&& i1,
    nsTArray<PTestDataStructuresSubParent*>* o1) {
  test_assert(nactors == i1.Length(), "wrong #actors");
  for (uint32_t i = 0; i < i1.Length(); ++i)
    test_assert(i == Cast(i1[i]).mI, "wrong mI value");
  *o1 = i1;
  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest3(const IntDouble& i1,
                                                            const IntDouble& i2,
                                                            IntDouble* o1,
                                                            IntDouble* o2) {
  test_assert(42 == i1.get_int(), "wrong value");
  test_assert(4.0 == i2.get_double(), "wrong value");

  *o1 = i1;
  *o2 = i2;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest4(
    nsTArray<IntDouble>&& i1, nsTArray<IntDouble>* o1) {
  test_assert(4 == i1.Length(), "wrong length");
  test_assert(1 == i1[0].get_int(), "wrong value");
  test_assert(2.0 == i1[1].get_double(), "wrong value");
  test_assert(3 == i1[2].get_int(), "wrong value");
  test_assert(4.0 == i1[3].get_double(), "wrong value");

  *o1 = i1;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest5(
    const IntDoubleArrays& i1, const IntDoubleArrays& i2,
    const IntDoubleArrays& i3, IntDoubleArrays* o1, IntDoubleArrays* o2,
    IntDoubleArrays* o3) {
  test_assert(42 == i1.get_int(), "wrong value");

  const nsTArray<int>& i2a = i2.get_ArrayOfint();
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  const nsTArray<double>& i3a = i3.get_ArrayOfdouble();
  test_assert(3 == i3a.Length(), "wrong length");
  test_assert(1.0 == i3a[0], "wrong value");
  test_assert(2.0 == i3a[1], "wrong value");
  test_assert(3.0 == i3a[2], "wrong value");

  *o1 = i1;
  *o2 = i2a;
  *o3 = i3a;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest7_0(
    const ActorWrapper& i1, ActorWrapper* o1) {
  if (i1.actorChild() != nullptr)
    fail("child side actor should always be null");

  if (i1.actorParent() != mKids[0])
    fail("should have got back same actor on parent side");

  o1->actorParent() = mKids[0];
  // malicious behavior
  o1->actorChild() =
      reinterpret_cast<PTestDataStructuresSubChild*>(uintptr_t(0xdeadbeef));
  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest6(
    nsTArray<IntDoubleArrays>&& i1, nsTArray<IntDoubleArrays>* o1) {
  test_assert(3 == i1.Length(), "wrong length");

  IntDoubleArrays id1(i1[0]);
  test_assert(42 == id1.get_int(), "wrong value");

  nsTArray<int> i2a(i1[1].get_ArrayOfint());
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  nsTArray<double> i3a(i1[2].get_ArrayOfdouble());
  test_assert(3 == i3a.Length(), "wrong length");
  test_assert(1.0 == i3a[0], "wrong value");
  test_assert(2.0 == i3a[1], "wrong value");
  test_assert(3.0 == i3a[2], "wrong value");

  o1->AppendElement(id1);
  o1->AppendElement(IntDoubleArrays(i2a));
  o1->AppendElement(IntDoubleArrays(i3a));

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest7(
    const Actors& i1, const Actors& i2, const Actors& i3, Actors* o1,
    Actors* o2, Actors* o3) {
  test_assert(42 == i1.get_int(), "wrong value");

  nsTArray<int> i2a(i2.get_ArrayOfint());
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  assert_arrays_equal(mKids, i3.get_ArrayOfPTestDataStructuresSubParent());

  *o1 = 42;
  *o2 = i2a;
  *o3 = mKids;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest8(
    nsTArray<Actors>&& i1, nsTArray<Actors>* o1) {
  test_assert(3 == i1.Length(), "wrong length");
  test_assert(42 == i1[0].get_int(), "wrong value");

  const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  assert_arrays_equal(mKids, i1[2].get_ArrayOfPTestDataStructuresSubParent());

  *o1 = i1;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest9(
    const Unions& i1, const Unions& i2, const Unions& i3, const Unions& i4,
    Unions* o1, Unions* o2, Unions* o3, Unions* o4) {
  test_assert(42 == i1.get_int(), "wrong value");

  const nsTArray<int>& i2a = i2.get_ArrayOfint();
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  assert_arrays_equal(mKids, i3.get_ArrayOfPTestDataStructuresSubParent());

  const nsTArray<PTestDataStructuresSubParent*>& i4a =
      i4.get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubParent();
  assert_arrays_equal(mKids, i4a);

  *o1 = i1;
  *o2 = i2;
  *o3 = i3;
  *o4 = i4;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest10(
    nsTArray<Unions>&& i1, nsTArray<Unions>* o1) {
  test_assert(42 == i1[0].get_int(), "wrong value");

  const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
  test_assert(3 == i2a.Length(), "wrong length");
  test_assert(1 == i2a[0], "wrong value");
  test_assert(2 == i2a[1], "wrong value");
  test_assert(3 == i2a[2], "wrong value");

  assert_arrays_equal(mKids, i1[2].get_ArrayOfPTestDataStructuresSubParent());

  const nsTArray<PTestDataStructuresSubParent*>& i4a =
      i1[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubParent();
  assert_arrays_equal(mKids, i4a);

  *o1 = i1;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest11(
    const SIntDouble& i, SIntDouble* o) {
  test_assert(1 == i.i(), "wrong value");
  test_assert(2.0 == i.d(), "wrong value");
  *o = i;
  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest12(
    const SIntDoubleArrays& i, SIntDoubleArrays* o) {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  nsTArray<double> ad;
  ad.AppendElement(.5);
  ad.AppendElement(1.0);
  ad.AppendElement(2.0);

  test_assert(42 == i.i(), "wrong value");
  assert_arrays_equal(ai, i.ai());
  assert_arrays_equal(ad, i.ad());

  *o = i;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest13(const SActors& i,
                                                             SActors* o) {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  test_assert(42 == i.i(), "wrong value");
  assert_arrays_equal(ai, i.ai());
  assert_arrays_equal(mKids, i.apParent());

  *o = i;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest14(const Structs& i,
                                                             Structs* o) {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  test_assert(42 == i.i(), "wrong value");
  assert_arrays_equal(ai, i.ai());
  assert_arrays_equal(mKids, i.apParent());

  const SActors& ia = i.aa()[0];
  test_assert(42 == ia.i(), "wrong value");
  assert_arrays_equal(ai, ia.ai());
  assert_arrays_equal(mKids, ia.apParent());

  *o = i;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest15(
    const WithStructs& i1, const WithStructs& i2, const WithStructs& i3,
    const WithStructs& i4, const WithStructs& i5, WithStructs* o1,
    WithStructs* o2, WithStructs* o3, WithStructs* o4, WithStructs* o5) {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  test_assert(i1 == int(42), "wrong value");
  assert_arrays_equal(i2.get_ArrayOfint(), ai);
  assert_arrays_equal(i3.get_ArrayOfPTestDataStructuresSubParent(), mKids);

  const SActors& ia = i4.get_ArrayOfSActors()[0];
  test_assert(42 == ia.i(), "wrong value");
  assert_arrays_equal(ai, ia.ai());
  assert_arrays_equal(mKids, ia.apParent());

  const Structs& is = i5.get_ArrayOfStructs()[0];
  test_assert(42 == is.i(), "wrong value");
  assert_arrays_equal(ai, is.ai());
  assert_arrays_equal(mKids, is.apParent());

  const SActors& isa = is.aa()[0];
  test_assert(42 == isa.i(), "wrong value");
  assert_arrays_equal(ai, isa.ai());
  assert_arrays_equal(mKids, isa.apParent());

  *o1 = i1;
  *o2 = i2;
  *o3 = i3;
  *o4 = i4;
  *o5 = i5;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest16(
    const WithUnions& i, WithUnions* o) {
  test_assert(i.i() == 42, "wrong value");

  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);
  assert_arrays_equal(ai, i.ai());

  assert_arrays_equal(i.apParent(), mKids);

  assert_arrays_equal(mKids,
                      i.aa()[0].get_ArrayOfPTestDataStructuresSubParent());

  const nsTArray<Unions>& iau = i.au();
  test_assert(iau[0] == 42, "wrong value");
  assert_arrays_equal(ai, iau[1].get_ArrayOfint());
  assert_arrays_equal(mKids, iau[2].get_ArrayOfPTestDataStructuresSubParent());
  assert_arrays_equal(
      mKids,
      iau[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubParent());

  *o = i;

  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest17(
    nsTArray<Op>&& sa) {
  test_assert(sa.Length() == 1 && Op::TSetAttrs == sa[0].type(), "wrong value");
  return IPC_OK();
}

mozilla::ipc::IPCResult TestDataStructuresParent::RecvTest18(RegionArray&& ra) {
  for (RegionArray::index_type i = 0; i < ra.Length(); ++i) {
    // if |ra| has been realloc()d and given a different allocator
    // chunk, this loop will nondeterministically crash or iloop.
    for (auto iter = ra[i].RectIter(); !iter.Done(); iter.Next()) {
      Unused << iter.Get();
    }
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// child

TestDataStructuresChild::TestDataStructuresChild() {
  MOZ_COUNT_CTOR(TestDataStructuresChild);
}

TestDataStructuresChild::~TestDataStructuresChild() {
  MOZ_COUNT_DTOR(TestDataStructuresChild);
}

mozilla::ipc::IPCResult TestDataStructuresChild::RecvStart() {
  puts("[TestDataStructuresChild] starting");

  Test1();
  Test2();
  Test3();
  Test4();
  Test5();
  Test6();
  Test7_0();
  Test7();
  Test8();
  Test9();
  Test10();
  Test11();
  Test12();
  Test13();
  Test14();
  Test15();
  Test16();
  Test17();
  if (OtherPid() != base::GetCurrentProcId()) {
    // FIXME/bug 703317 allocation of nsIntRegion uses a global
    // region pool which breaks threads
    Test18();
  }

  for (uint32_t i = 0; i < nactors; ++i)
    if (!PTestDataStructuresSubChild::Send__delete__(mKids[i]))
      fail("can't send dtor");

  Close();

  return IPC_OK();
}

void TestDataStructuresChild::Test1() {
  nsTArray<int> ia;

  for (int i = 0; i < 5; ++i) ia.AppendElement(i);

  nsTArray<int> oa;
  if (!SendTest1(ia, &oa)) fail("can't send Test1");

  assert_arrays_equal(ia, oa);

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test2() {
  nsTArray<PTestDataStructuresSubChild*> oa;
  if (!SendTest2(mKids, &oa)) fail("can't send Test2");
  assert_arrays_equal(mKids, oa);

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test3() {
  int i1i = 42;
  double i2d = 4.0;
  IntDouble i1(i1i);
  IntDouble i2(i2d);
  IntDouble o1, o2;

  SendTest3(i1, i2, &o1, &o2);

  test_assert(i1i == o1.get_int(), "wrong value");
  test_assert(i2d == o2.get_double(), "wrong value");

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test4() {
  nsTArray<IntDouble> i1;
  i1.AppendElement(IntDouble(int(1)));
  i1.AppendElement(IntDouble(2.0));
  i1.AppendElement(IntDouble(int(3)));
  i1.AppendElement(IntDouble(4.0));

  nsTArray<IntDouble> o1;
  if (!SendTest4(i1, &o1)) fail("can't send Test4");

  // TODO Union::operator==()
  test_assert(i1.Length() == o1.Length(), "wrong length");
  test_assert(1 == o1[0].get_int(), "wrong value");
  test_assert(2.0 == o1[1].get_double(), "wrong value");
  test_assert(3 == o1[2].get_int(), "wrong value");
  test_assert(4.0 == o1[3].get_double(), "wrong value");

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test5() {
  IntDoubleArrays i1(int(42));
  nsTArray<int> i2;
  i2.AppendElement(1);
  i2.AppendElement(2);
  i2.AppendElement(3);
  nsTArray<double> i3;
  i3.AppendElement(1.0);
  i3.AppendElement(2.0);
  i3.AppendElement(3.0);

  IntDoubleArrays o1, o2, o3;
  if (!SendTest5(i1, IntDoubleArrays(i2), IntDoubleArrays(i3), &o1, &o2, &o3))
    fail("can't send Test5");

  test_assert(42 == o1.get_int(), "wrong value");
  assert_arrays_equal(i2, o2.get_ArrayOfint());
  assert_arrays_equal(i3, o3.get_ArrayOfdouble());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test6() {
  IntDoubleArrays id1(int(42));
  nsTArray<int> id2;
  id2.AppendElement(1);
  id2.AppendElement(2);
  id2.AppendElement(3);
  nsTArray<double> id3;
  id3.AppendElement(1.0);
  id3.AppendElement(2.0);
  id3.AppendElement(3.0);

  nsTArray<IntDoubleArrays> i1;
  i1.AppendElement(id1);
  i1.AppendElement(IntDoubleArrays(id2));
  i1.AppendElement(IntDoubleArrays(id3));

  nsTArray<IntDoubleArrays> o1;
  if (!SendTest6(i1, &o1)) fail("can't send Test6");

  test_assert(3 == o1.Length(), "wrong length");
  IntDoubleArrays od1(o1[0]);
  nsTArray<int> od2(o1[1].get_ArrayOfint());
  nsTArray<double> od3(o1[2].get_ArrayOfdouble());

  test_assert(42 == od1.get_int(), "wrong value");
  assert_arrays_equal(id2, od2);
  assert_arrays_equal(id3, od3);

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test7_0() {
  ActorWrapper iaw;
  if (iaw.actorChild() != nullptr || iaw.actorParent() != nullptr)
    fail("actor members should be null initially");

  iaw.actorChild() = mKids[0];
  if (iaw.actorParent() != nullptr)
    fail("parent should be null on child side after set");

  ActorWrapper oaw;
  if (!SendTest7_0(iaw, &oaw)) fail("sending Test7_0");

  if (oaw.actorParent() != nullptr)
    fail(
        "parent accessor on actor-struct members should always be null in "
        "child");

  if (oaw.actorChild() != mKids[0])
    fail("should have got back same child-side actor");
}

void TestDataStructuresChild::Test7() {
  Actors i1(42);
  nsTArray<int> i2a;
  i2a.AppendElement(1);
  i2a.AppendElement(2);
  i2a.AppendElement(3);

  Actors o1, o2, o3;
  if (!SendTest7(i1, Actors(i2a), Actors(mKids), &o1, &o2, &o3))
    fail("can't send Test7");

  test_assert(42 == o1.get_int(), "wrong value");
  assert_arrays_equal(i2a, o2.get_ArrayOfint());
  assert_arrays_equal(mKids, o3.get_ArrayOfPTestDataStructuresSubChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test8() {
  Actors i1e(42);
  nsTArray<int> i2a;
  i2a.AppendElement(1);
  i2a.AppendElement(2);
  i2a.AppendElement(3);

  nsTArray<Actors> i1;
  i1.AppendElement(i1e);
  i1.AppendElement(i2a);
  i1.AppendElement(mKids);

  nsTArray<Actors> o1;
  if (!SendTest8(i1, &o1)) fail("can't send Test8");

  test_assert(3 == o1.Length(), "wrong length");
  test_assert(42 == o1[0].get_int(), "wrong value");
  assert_arrays_equal(i2a, o1[1].get_ArrayOfint());
  assert_arrays_equal(mKids, o1[2].get_ArrayOfPTestDataStructuresSubChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test9() {
  Unions i1(int(42));

  nsTArray<int> i2a;
  i2a.AppendElement(1);
  i2a.AppendElement(2);
  i2a.AppendElement(3);

  nsTArray<Actors> i4a;
  i4a.AppendElement(mKids);

  Unions o1, o2, o3, o4;
  if (!SendTest9(i1, Unions(i2a), Unions(mKids), Unions(i4a), &o1, &o2, &o3,
                 &o4))
    fail("can't send Test9");

  test_assert(42 == o1.get_int(), "wrong value");
  assert_arrays_equal(i2a, o2.get_ArrayOfint());
  assert_arrays_equal(mKids, o3.get_ArrayOfPTestDataStructuresSubChild());
  assert_arrays_equal(
      mKids,
      o4.get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test10() {
  Unions i1a(int(42));

  nsTArray<int> i2a;
  i2a.AppendElement(1);
  i2a.AppendElement(2);
  i2a.AppendElement(3);

  nsTArray<Actors> i4a;
  i4a.AppendElement(mKids);

  nsTArray<Unions> i1;
  i1.AppendElement(i1a);
  i1.AppendElement(Unions(i2a));
  i1.AppendElement(Unions(mKids));
  i1.AppendElement(Unions(i4a));

  nsTArray<Unions> o1;
  if (!SendTest10(i1, &o1)) fail("can't send Test10");

  test_assert(4 == o1.Length(), "wrong length");
  test_assert(42 == o1[0].get_int(), "wrong value");
  assert_arrays_equal(i2a, o1[1].get_ArrayOfint());
  assert_arrays_equal(mKids, o1[2].get_ArrayOfPTestDataStructuresSubChild());
  assert_arrays_equal(
      mKids,
      o1[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test11() {
  SIntDouble i(1, 2.0);
  SIntDouble o;

  if (!SendTest11(i, &o)) fail("sending Test11");

  test_assert(1 == o.i() && 2.0 == o.d(), "wrong values");

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test12() {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  nsTArray<double> ad;
  ad.AppendElement(.5);
  ad.AppendElement(1.0);
  ad.AppendElement(2.0);

  SIntDoubleArrays i(42, ai, ad);
  SIntDoubleArrays o;

  if (!SendTest12(i, &o)) fail("sending Test12");

  test_assert(42 == o.i(), "wrong value");
  assert_arrays_equal(ai, o.ai());
  assert_arrays_equal(ad, o.ad());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test13() {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  SActors i;
  i.i() = 42;
  i.ai() = ai;
  i.apChild() = mKids;

  SActors o;
  if (!SendTest13(i, &o)) fail("can't send Test13");

  test_assert(42 == o.i(), "wrong value");
  assert_arrays_equal(ai, o.ai());
  assert_arrays_equal(mKids, o.apChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test14() {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  SActors ia;
  ia.i() = 42;
  ia.ai() = ai;
  ia.apChild() = mKids;
  nsTArray<SActors> aa;
  aa.AppendElement(ia);

  Structs i;
  i.i() = 42;
  i.ai() = ai;
  i.apChild() = mKids;
  i.aa() = aa;

  Structs o;
  if (!SendTest14(i, &o)) fail("can't send Test14");

  test_assert(42 == o.i(), "wrong value");
  assert_arrays_equal(ai, o.ai());
  assert_arrays_equal(mKids, o.apChild());

  const SActors& os = o.aa()[0];
  test_assert(42 == os.i(), "wrong value");
  assert_arrays_equal(ai, os.ai());
  assert_arrays_equal(mKids, os.apChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test15() {
  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);

  SActors ia;
  ia.i() = 42;
  ia.ai() = ai;
  ia.apChild() = mKids;
  nsTArray<SActors> iaa;
  iaa.AppendElement(ia);

  Structs is;
  is.i() = 42;
  is.ai() = ai;
  is.apChild() = mKids;
  is.aa() = iaa;
  nsTArray<Structs> isa;
  isa.AppendElement(is);

  WithStructs o1, o2, o3, o4, o5;
  if (!SendTest15(WithStructs(42), WithStructs(ai), WithStructs(mKids),
                  WithStructs(iaa), WithStructs(isa), &o1, &o2, &o3, &o4, &o5))
    fail("sending Test15");

  test_assert(o1 == int(42), "wrong value");
  assert_arrays_equal(o2.get_ArrayOfint(), ai);
  assert_arrays_equal(o3.get_ArrayOfPTestDataStructuresSubChild(), mKids);

  const SActors& oa = o4.get_ArrayOfSActors()[0];
  test_assert(42 == oa.i(), "wrong value");
  assert_arrays_equal(ai, oa.ai());
  assert_arrays_equal(mKids, oa.apChild());

  const Structs& os = o5.get_ArrayOfStructs()[0];
  test_assert(42 == os.i(), "wrong value");
  assert_arrays_equal(ai, os.ai());
  assert_arrays_equal(mKids, os.apChild());

  const SActors& osa = os.aa()[0];
  test_assert(42 == osa.i(), "wrong value");
  assert_arrays_equal(ai, osa.ai());
  assert_arrays_equal(mKids, osa.apChild());

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test16() {
  WithUnions i;

  i.i() = 42;

  nsTArray<int> ai;
  ai.AppendElement(1);
  ai.AppendElement(2);
  ai.AppendElement(3);
  i.ai() = ai;

  i.apChild() = mKids;

  nsTArray<Actors> iaa;
  iaa.AppendElement(mKids);
  i.aa() = iaa;

  nsTArray<Unions> iau;
  iau.AppendElement(int(42));
  iau.AppendElement(ai);
  iau.AppendElement(mKids);
  iau.AppendElement(iaa);
  i.au() = iau;

  WithUnions o;
  if (!SendTest16(i, &o)) fail("sending Test16");

  test_assert(42 == o.i(), "wrong value");
  assert_arrays_equal(o.ai(), ai);
  assert_arrays_equal(o.apChild(), mKids);

  const Actors& oaa = o.aa()[0];
  assert_arrays_equal(oaa.get_ArrayOfPTestDataStructuresSubChild(), mKids);

  const nsTArray<Unions>& oau = o.au();
  test_assert(oau[0] == 42, "wrong value");
  assert_arrays_equal(oau[1].get_ArrayOfint(), ai);
  assert_arrays_equal(oau[2].get_ArrayOfPTestDataStructuresSubChild(), mKids);
  assert_arrays_equal(
      oau[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSubChild(),
      mKids);

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test17() {
  Attrs attrs;
  attrs.common() = CommonAttrs(true);
  attrs.specific() = BarAttrs(1.0f);

  nsTArray<Op> ops;
  ops.AppendElement(SetAttrs(nullptr, mKids[0], attrs));

  if (!SendTest17(ops)) fail("sending Test17");

  printf("  passed %s\n", __FUNCTION__);
}

void TestDataStructuresChild::Test18() {
  const int nelements = 1000;
  RegionArray ra;
  // big enough to hopefully force a realloc to a different chunk of
  // memory on the receiving side, if the workaround isn't working
  // correctly.  But SetCapacity() here because we don't want to
  // crash on the sending side.
  ra.SetCapacity(nelements);
  for (int i = 0; i < nelements; ++i) {
    nsIntRegion r;
    r.Or(nsIntRect(0, 0, 10, 10), nsIntRect(10, 10, 10, 10));
    ra.AppendElement(r);
  }

  if (!SendTest18(ra)) fail("sending Test18");

  printf("  passed %s\n", __FUNCTION__);
}

}  // namespace _ipdltest
}  // namespace mozilla
