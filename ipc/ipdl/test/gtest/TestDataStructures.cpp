/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test many combinations of data structures (both in argument and return
 * position) to ensure they are transmitted correctly.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestDataStructuresChild.h"
#include "mozilla/_ipdltest/PTestDataStructuresParent.h"
#include "mozilla/_ipdltest/PTestDataStructuresSubChild.h"
#include "mozilla/_ipdltest/PTestDataStructuresSubParent.h"

#include "mozilla/Unused.h"

using namespace mozilla::ipc;

using RegionArray = nsTArray<nsIntRegion>;

namespace mozilla::_ipdltest {

static const uint32_t nactors = 10;

class TestDataStructuresSubParent : public PTestDataStructuresSubParent {
  NS_INLINE_DECL_REFCOUNTING(TestDataStructuresSubParent, override)

 public:
  explicit TestDataStructuresSubParent(uint32_t i) : mI(i) {}
  uint32_t mI;

 private:
  ~TestDataStructuresSubParent() = default;
};

class TestDataStructuresSubChild : public PTestDataStructuresSubChild {
  NS_INLINE_DECL_REFCOUNTING(TestDataStructuresSubChild, override)

 public:
  explicit TestDataStructuresSubChild(uint32_t i) : mI(i) {}
  uint32_t mI;

 private:
  ~TestDataStructuresSubChild() = default;
};

inline static TestDataStructuresSubParent& Cast(
    PTestDataStructuresSubParent* a) {
  return *static_cast<TestDataStructuresSubParent*>(a);
}

class TestDataStructuresParent : public PTestDataStructuresParent {
  NS_INLINE_DECL_REFCOUNTING(TestDataStructuresParent, override)

 public:
  nsTArray<NotNull<
      SideVariant<PTestDataStructuresSubParent*, PTestDataStructuresSubChild*>>>
      kids;

 private:
  IPCResult RecvTestArrayOfInt(nsTArray<int>&& ia,
                               nsTArray<int>* oa) final override {
    EXPECT_EQ(5u, ia.Length());
    for (int i = 0; i < 5; ++i) EXPECT_EQ(i, ia[i]);

    *oa = std::move(ia);

    return IPC_OK();
  }

  IPCResult RecvTestArrayOfActor(
      nsTArray<NotNull<PTestDataStructuresSubParent*>>&& i1,
      nsTArray<NotNull<PTestDataStructuresSubParent*>>* o1) final override {
    EXPECT_EQ(nactors, i1.Length());
    for (uint32_t i = 0; i < i1.Length(); ++i) EXPECT_EQ(i, Cast(i1[i]).mI);
    *o1 = std::move(i1);
    return IPC_OK();
  }

  IPCResult RecvTestUnion(const IntDouble& i1, const IntDouble& i2,
                          IntDouble* o1, IntDouble* o2) final override {
    EXPECT_EQ(42, i1.get_int());
    EXPECT_EQ(4.0, i2.get_double());

    *o1 = i1;
    *o2 = i2;

    return IPC_OK();
  }

  IPCResult RecvTestArrayOfUnion(nsTArray<IntDouble>&& i1,
                                 nsTArray<IntDouble>* o1) final override {
    EXPECT_EQ(4u, i1.Length());
    EXPECT_EQ(1, i1[0].get_int());
    EXPECT_EQ(2.0, i1[1].get_double());
    EXPECT_EQ(3, i1[2].get_int());
    EXPECT_EQ(4.0, i1[3].get_double());

    *o1 = std::move(i1);

    return IPC_OK();
  }

  IPCResult RecvTestUnionWithArray(const IntDoubleArrays& i1,
                                   const IntDoubleArrays& i2,
                                   const IntDoubleArrays& i3,
                                   IntDoubleArrays* o1, IntDoubleArrays* o2,
                                   IntDoubleArrays* o3) final override {
    EXPECT_EQ(42, i1.get_int());

    const nsTArray<int>& i2a = i2.get_ArrayOfint();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    const nsTArray<double>& i3a = i3.get_ArrayOfdouble();
    EXPECT_EQ(3u, i3a.Length());
    EXPECT_EQ(1.0, i3a[0]);
    EXPECT_EQ(2.0, i3a[1]);
    EXPECT_EQ(3.0, i3a[2]);

    *o1 = i1;
    *o2 = i2a;
    *o3 = i3a;

    return IPC_OK();
  }

  IPCResult RecvTestArrayOfUnionWithArray(
      nsTArray<IntDoubleArrays>&& i1,
      nsTArray<IntDoubleArrays>* o1) final override {
    EXPECT_EQ(3u, i1.Length());

    IntDoubleArrays id1(i1[0]);
    EXPECT_EQ(42, id1.get_int());

    nsTArray<int> i2a = i1[1].get_ArrayOfint().Clone();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    nsTArray<double> i3a = i1[2].get_ArrayOfdouble().Clone();
    EXPECT_EQ(3u, i3a.Length());
    EXPECT_EQ(1.0, i3a[0]);
    EXPECT_EQ(2.0, i3a[1]);
    EXPECT_EQ(3.0, i3a[2]);

    o1->AppendElement(id1);
    o1->AppendElement(IntDoubleArrays(i2a));
    o1->AppendElement(IntDoubleArrays(i3a));

    return IPC_OK();
  }

  IPCResult RecvTestStructWithActor(const ActorWrapper& i1,
                                    ActorWrapper* o1) final override {
    EXPECT_FALSE(i1.actor().IsChild()) << "child side should be empty";

    EXPECT_EQ(i1.actor(), kids[0])
        << "should have got back same actor on parent side";

    o1->actor() = kids[0];
    return IPC_OK();
  }

  IPCResult RecvTestUnionWithActors(const Actors& i1, const Actors& i2,
                                    const Actors& i3, Actors* o1, Actors* o2,
                                    Actors* o3) final override {
    EXPECT_EQ(42, i1.get_int());

    nsTArray<int> i2a = i2.get_ArrayOfint().Clone();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    const auto& a = i3.get_ArrayOfPTestDataStructuresSub();
    EXPECT_EQ(a.Length(), kids.Length());
    for (size_t i = 0; i < a.Length(); ++i) {
      EXPECT_EQ(a[i], kids[i]);
    }

    *o1 = 42;
    *o2 = i2a;
    *o3 = kids.Clone();

    return IPC_OK();
  }

  IPCResult RecvTestArrayOfUnionWithActors(
      nsTArray<Actors>&& i1, nsTArray<Actors>* o1) final override {
    EXPECT_EQ(3u, i1.Length());
    EXPECT_EQ(42, i1[0].get_int());

    const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    EXPECT_EQ(kids, i1[2].get_ArrayOfPTestDataStructuresSub());

    *o1 = std::move(i1);

    return IPC_OK();
  }

  IPCResult RecvTestUnions(const Unions& i1, const Unions& i2, const Unions& i3,
                           const Unions& i4, Unions* o1, Unions* o2, Unions* o3,
                           Unions* o4) final override {
    EXPECT_EQ(42, i1.get_int());

    const nsTArray<int>& i2a = i2.get_ArrayOfint();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    EXPECT_EQ(kids, i3.get_ArrayOfPTestDataStructuresSub());

    const auto& i4a =
        i4.get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub();
    EXPECT_EQ(kids, i4a);

    *o1 = i1;
    *o2 = i2;
    *o3 = i3;
    *o4 = i4;

    return IPC_OK();
  }

  IPCResult RecvTestArrayOfUnions(nsTArray<Unions>&& i1,
                                  nsTArray<Unions>* o1) final override {
    EXPECT_EQ(42, i1[0].get_int());

    const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
    EXPECT_EQ(3u, i2a.Length());
    EXPECT_EQ(1, i2a[0]);
    EXPECT_EQ(2, i2a[1]);
    EXPECT_EQ(3, i2a[2]);

    EXPECT_EQ(kids, i1[2].get_ArrayOfPTestDataStructuresSub());

    const auto& i4a =
        i1[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub();
    EXPECT_EQ(kids, i4a);

    *o1 = std::move(i1);

    return IPC_OK();
  }

  IPCResult RecvTestStruct(const SIntDouble& i, SIntDouble* o) final override {
    EXPECT_EQ(1, i.i());
    EXPECT_EQ(2.0, i.d());
    *o = i;
    return IPC_OK();
  }

  IPCResult RecvTestStructWithArrays(const SIntDoubleArrays& i,
                                     SIntDoubleArrays* o) final override {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    nsTArray<double> ad;
    ad.AppendElement(.5);
    ad.AppendElement(1.0);
    ad.AppendElement(2.0);

    EXPECT_EQ(42, i.i());
    EXPECT_EQ(ai, i.ai());
    EXPECT_EQ(ad, i.ad());

    *o = i;

    return IPC_OK();
  }

  IPCResult RecvTestStructWithActors(const SActors& i,
                                     SActors* o) final override {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    EXPECT_EQ(42, i.i());
    EXPECT_EQ(ai, i.ai());
    EXPECT_EQ(kids, i.ap());

    *o = i;

    return IPC_OK();
  }

  IPCResult RecvTestStructs(const Structs& i, Structs* o) final override {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    EXPECT_EQ(42, i.i());
    EXPECT_EQ(ai, i.ai());
    EXPECT_EQ(kids, i.ap());

    const SActors& ia = i.aa()[0];
    EXPECT_EQ(42, ia.i());
    EXPECT_EQ(ai, ia.ai());
    EXPECT_EQ(kids, ia.ap());

    *o = i;

    return IPC_OK();
  }

  IPCResult RecvTestUnionWithStructs(
      const WithStructs& i1, const WithStructs& i2, const WithStructs& i3,
      const WithStructs& i4, const WithStructs& i5, WithStructs* o1,
      WithStructs* o2, WithStructs* o3, WithStructs* o4,
      WithStructs* o5) final override {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    EXPECT_EQ(i1, int(42));
    EXPECT_EQ(i2.get_ArrayOfint(), ai);
    EXPECT_EQ(i3.get_ArrayOfPTestDataStructuresSub(), kids);

    const SActors& ia = i4.get_ArrayOfSActors()[0];
    EXPECT_EQ(42, ia.i());
    EXPECT_EQ(ai, ia.ai());
    EXPECT_EQ(kids, ia.ap());

    const Structs& is = i5.get_ArrayOfStructs()[0];
    EXPECT_EQ(42, is.i());
    EXPECT_EQ(ai, is.ai());
    EXPECT_EQ(kids, is.ap());

    const SActors& isa = is.aa()[0];
    EXPECT_EQ(42, isa.i());
    EXPECT_EQ(ai, isa.ai());
    EXPECT_EQ(kids, isa.ap());

    *o1 = i1;
    *o2 = i2;
    *o3 = i3;
    *o4 = i4;
    *o5 = i5;

    return IPC_OK();
  }

  IPCResult RecvTestStructWithUnions(const WithUnions& i,
                                     WithUnions* o) final override {
    EXPECT_EQ(i.i(), 42);

    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);
    EXPECT_EQ(ai, i.ai());

    EXPECT_EQ(i.ap(), kids);

    EXPECT_EQ(kids, i.aa()[0].get_ArrayOfPTestDataStructuresSub());

    const nsTArray<Unions>& iau = i.au();
    EXPECT_EQ(iau[0], 42);
    EXPECT_EQ(ai, iau[1].get_ArrayOfint());
    EXPECT_EQ(kids, iau[2].get_ArrayOfPTestDataStructuresSub());
    EXPECT_EQ(
        kids,
        iau[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub());

    *o = i;

    return IPC_OK();
  }

  IPCResult RecvTestUnionWithCxx(nsTArray<Op>&& sa) final override {
    EXPECT_EQ(sa.Length(), (size_t)1);
    EXPECT_EQ(Op::TSetAttrs, sa[0].type());
    return IPC_OK();
  }

  IPCResult RecvTestNsIntRegion(RegionArray&& ra) final override {
    for (RegionArray::index_type i = 0; i < ra.Length(); ++i) {
      // if |ra| has been realloc()d and given a different allocator
      // chunk, this loop will nondeterministically crash or iloop.
      for (auto iter = ra[i].RectIter(); !iter.Done(); iter.Next()) {
        Unused << iter.Get();
      }
    }
    return IPC_OK();
  }

  ~TestDataStructuresParent() = default;
};

class TestDataStructuresChild : public PTestDataStructuresChild {
  NS_INLINE_DECL_REFCOUNTING(TestDataStructuresChild, override)

 private:
  nsTArray<NotNull<
      SideVariant<PTestDataStructuresSubParent*, PTestDataStructuresSubChild*>>>
      kids;
  nsTArray<NotNull<PTestDataStructuresSubChild*>> kidsDirect;

  already_AddRefed<PTestDataStructuresSubChild>
  AllocPTestDataStructuresSubChild(const int& i) final override {
    auto child = MakeRefPtr<TestDataStructuresSubChild>(i);
    kids.AppendElement(WrapNotNull(child));
    kidsDirect.AppendElement(WrapNotNull(child));
    return child.forget();
  }

  IPCResult RecvStart() final override {
    TestArrayOfInt();
    TestArrayOfActor();
    TestUnion();
    TestArrayOfUnion();
    TestUnionWithArray();
    TestArrayOfUnionWithArray();
    TestStructWithActor();
    TestUnionWithActors();
    TestArrayOfUnionWithActors();
    TestUnions();
    TestArrayOfUnions();
    TestStruct();
    TestStructWithArrays();
    TestStructWithActors();
    TestStructs();
    TestUnionWithStructs();
    TestStructWithUnions();
    TestUnionWithCxx();
    TestNsIntRegion();

    auto actors = kidsDirect.Clone();
    for (auto& actor : actors) {
      EXPECT_TRUE(PTestDataStructuresSubChild::Send__delete__(actor));
    }

    Close();

    return IPC_OK();
  }

  void TestArrayOfInt() {
    nsTArray<int> ia;

    for (int i = 0; i < 5; ++i) ia.AppendElement(i);

    nsTArray<int> oa;
    EXPECT_TRUE(SendTestArrayOfInt(ia, &oa));

    EXPECT_EQ(ia, oa);
  }

  void TestArrayOfActor() {
    nsTArray<NotNull<PTestDataStructuresSubChild*>> oa;
    EXPECT_TRUE(SendTestArrayOfActor(kidsDirect, &oa));
    EXPECT_EQ(kidsDirect, oa);
  }

  void TestUnion() {
    int i1i = 42;
    double i2d = 4.0;
    IntDouble i1(i1i);
    IntDouble i2(i2d);
    IntDouble o1, o2;

    SendTestUnion(i1, i2, &o1, &o2);

    EXPECT_EQ(i1i, o1.get_int());
    EXPECT_EQ(i2d, o2.get_double());
  }

  void TestArrayOfUnion() {
    nsTArray<IntDouble> i1;
    i1.AppendElement(IntDouble(int(1)));
    i1.AppendElement(IntDouble(2.0));
    i1.AppendElement(IntDouble(int(3)));
    i1.AppendElement(IntDouble(4.0));

    nsTArray<IntDouble> o1;
    EXPECT_TRUE(SendTestArrayOfUnion(i1, &o1));

    // TODO Union::operator==()
    EXPECT_EQ(i1.Length(), o1.Length());
    EXPECT_EQ(1, o1[0].get_int());
    EXPECT_EQ(2.0, o1[1].get_double());
    EXPECT_EQ(3, o1[2].get_int());
    EXPECT_EQ(4.0, o1[3].get_double());
  }

  void TestUnionWithArray() {
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
    EXPECT_TRUE(SendTestUnionWithArray(i1, IntDoubleArrays(i2),
                                       IntDoubleArrays(i3), &o1, &o2, &o3));

    EXPECT_EQ(42, o1.get_int());
    EXPECT_EQ(i2, o2.get_ArrayOfint());
    EXPECT_EQ(i3, o3.get_ArrayOfdouble());
  }

  void TestArrayOfUnionWithArray() {
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
    EXPECT_TRUE(SendTestArrayOfUnionWithArray(i1, &o1));

    EXPECT_EQ(3u, o1.Length());
    IntDoubleArrays od1(o1[0]);
    nsTArray<int> od2 = o1[1].get_ArrayOfint().Clone();
    nsTArray<double> od3 = o1[2].get_ArrayOfdouble().Clone();

    EXPECT_EQ(42, od1.get_int());
    EXPECT_EQ(id2, od2);
    EXPECT_EQ(id3, od3);
  }

  void TestStructWithActor() {
    ActorWrapper iaw(kidsDirect[0]);

    ActorWrapper oaw;
    EXPECT_TRUE(SendTestStructWithActor(iaw, &oaw));

    EXPECT_TRUE(oaw.actor().IsChild());
    EXPECT_EQ(oaw.actor().AsChild(), kidsDirect[0]);
  }

  void TestUnionWithActors() {
    Actors i1(42);
    nsTArray<int> i2a;
    i2a.AppendElement(1);
    i2a.AppendElement(2);
    i2a.AppendElement(3);

    Actors o1, o2, o3;
    EXPECT_TRUE(
        SendTestUnionWithActors(i1, Actors(i2a), Actors(kids), &o1, &o2, &o3));

    EXPECT_EQ(42, o1.get_int());
    EXPECT_EQ(i2a, o2.get_ArrayOfint());
    EXPECT_EQ(kids, o3.get_ArrayOfPTestDataStructuresSub());
  }

  void TestArrayOfUnionWithActors() {
    Actors i1e(42);
    nsTArray<int> i2a;
    i2a.AppendElement(1);
    i2a.AppendElement(2);
    i2a.AppendElement(3);

    nsTArray<Actors> i1;
    i1.AppendElement(i1e);
    i1.AppendElement(i2a);
    i1.AppendElement(kids);

    nsTArray<Actors> o1;
    EXPECT_TRUE(SendTestArrayOfUnionWithActors(i1, &o1));

    EXPECT_EQ(3u, o1.Length());
    EXPECT_EQ(42, o1[0].get_int());
    EXPECT_EQ(i2a, o1[1].get_ArrayOfint());
    EXPECT_EQ(kids, o1[2].get_ArrayOfPTestDataStructuresSub());
  }

  void TestUnions() {
    Unions i1(int(42));

    nsTArray<int> i2a;
    i2a.AppendElement(1);
    i2a.AppendElement(2);
    i2a.AppendElement(3);

    nsTArray<Actors> i4a;
    i4a.AppendElement(kids);

    Unions o1, o2, o3, o4;
    EXPECT_TRUE(SendTestUnions(i1, Unions(i2a), Unions(kids), Unions(i4a), &o1,
                               &o2, &o3, &o4));

    EXPECT_EQ(42, o1.get_int());
    EXPECT_EQ(i2a, o2.get_ArrayOfint());
    EXPECT_EQ(kids, o3.get_ArrayOfPTestDataStructuresSub());
    EXPECT_EQ(kids,
              o4.get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub());
  }

  void TestArrayOfUnions() {
    Unions i1a(int(42));

    nsTArray<int> i2a;
    i2a.AppendElement(1);
    i2a.AppendElement(2);
    i2a.AppendElement(3);

    nsTArray<Actors> i4a;
    i4a.AppendElement(kids);

    nsTArray<Unions> i1;
    i1.AppendElement(i1a);
    i1.AppendElement(Unions(i2a));
    i1.AppendElement(Unions(kids));
    i1.AppendElement(Unions(i4a));

    nsTArray<Unions> o1;
    EXPECT_TRUE(SendTestArrayOfUnions(i1, &o1));

    EXPECT_EQ(4u, o1.Length());
    EXPECT_EQ(42, o1[0].get_int());
    EXPECT_EQ(i2a, o1[1].get_ArrayOfint());
    EXPECT_EQ(kids, o1[2].get_ArrayOfPTestDataStructuresSub());
    EXPECT_EQ(kids,
              o1[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub());
  }

  void TestStruct() {
    SIntDouble i(1, 2.0);
    SIntDouble o;

    EXPECT_TRUE(SendTestStruct(i, &o));

    EXPECT_EQ(o.i(), 1);
    EXPECT_EQ(o.d(), 2.0);
  }

  void TestStructWithArrays() {
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

    EXPECT_TRUE(SendTestStructWithArrays(i, &o));

    EXPECT_EQ(42, o.i());
    EXPECT_EQ(ai, o.ai());
    EXPECT_EQ(ad, o.ad());
  }

  void TestStructWithActors() {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    SActors i;
    i.i() = 42;
    i.ai() = ai.Clone();
    i.ap() = kids.Clone();

    SActors o;
    EXPECT_TRUE(SendTestStructWithActors(i, &o));

    EXPECT_EQ(42, o.i());
    EXPECT_EQ(ai, o.ai());
    EXPECT_EQ(kids, o.ap());
  }

  void TestStructs() {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    SActors ia;
    ia.i() = 42;
    ia.ai() = ai.Clone();
    ia.ap() = kids.Clone();
    nsTArray<SActors> aa;
    aa.AppendElement(ia);

    Structs i;
    i.i() = 42;
    i.ai() = ai.Clone();
    i.ap() = kids.Clone();
    i.aa() = aa.Clone();

    Structs o;
    EXPECT_TRUE(SendTestStructs(i, &o));

    EXPECT_EQ(42, o.i());
    EXPECT_EQ(ai, o.ai());
    EXPECT_EQ(kids, o.ap());

    const SActors& os = o.aa()[0];
    EXPECT_EQ(42, os.i());
    EXPECT_EQ(ai, os.ai());
    EXPECT_EQ(kids, os.ap());
  }

  void TestUnionWithStructs() {
    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);

    SActors ia;
    ia.i() = 42;
    ia.ai() = ai.Clone();
    ia.ap() = kids.Clone();
    nsTArray<SActors> iaa;
    iaa.AppendElement(ia);

    Structs is;
    is.i() = 42;
    is.ai() = ai.Clone();
    is.ap() = kids.Clone();
    is.aa() = iaa.Clone();
    nsTArray<Structs> isa;
    isa.AppendElement(is);

    WithStructs o1, o2, o3, o4, o5;
    EXPECT_TRUE(SendTestUnionWithStructs(
        WithStructs(42), WithStructs(ai), WithStructs(kids), WithStructs(iaa),
        WithStructs(isa), &o1, &o2, &o3, &o4, &o5));

    EXPECT_EQ(o1, int(42));
    EXPECT_EQ(o2.get_ArrayOfint(), ai);
    EXPECT_EQ(o3.get_ArrayOfPTestDataStructuresSub(), kids);

    const SActors& oa = o4.get_ArrayOfSActors()[0];
    EXPECT_EQ(42, oa.i());
    EXPECT_EQ(ai, oa.ai());
    EXPECT_EQ(kids, oa.ap());

    const Structs& os = o5.get_ArrayOfStructs()[0];
    EXPECT_EQ(42, os.i());
    EXPECT_EQ(ai, os.ai());
    EXPECT_EQ(kids, os.ap());

    const SActors& osa = os.aa()[0];
    EXPECT_EQ(42, osa.i());
    EXPECT_EQ(ai, osa.ai());
    EXPECT_EQ(kids, osa.ap());
  }

  void TestStructWithUnions() {
    WithUnions i;

    i.i() = 42;

    nsTArray<int> ai;
    ai.AppendElement(1);
    ai.AppendElement(2);
    ai.AppendElement(3);
    i.ai() = ai.Clone();

    i.ap() = kids.Clone();

    nsTArray<Actors> iaa;
    iaa.AppendElement(kids);
    i.aa() = iaa.Clone();

    nsTArray<Unions> iau;
    iau.AppendElement(int(42));
    iau.AppendElement(ai);
    iau.AppendElement(kids);
    iau.AppendElement(iaa);
    i.au() = iau.Clone();

    WithUnions o;
    EXPECT_TRUE(SendTestStructWithUnions(i, &o));

    EXPECT_EQ(42, o.i());
    EXPECT_EQ(o.ai(), ai);
    EXPECT_EQ(o.ap(), kids);

    const Actors& oaa = o.aa()[0];
    EXPECT_EQ(oaa.get_ArrayOfPTestDataStructuresSub(), kids);

    const nsTArray<Unions>& oau = o.au();
    EXPECT_EQ(oau[0], 42);
    EXPECT_EQ(oau[1].get_ArrayOfint(), ai);
    EXPECT_EQ(oau[2].get_ArrayOfPTestDataStructuresSub(), kids);
    EXPECT_EQ(oau[3].get_ArrayOfActors()[0].get_ArrayOfPTestDataStructuresSub(),
              kids);
  }

  void TestUnionWithCxx() {
    Attrs attrs;
    attrs.common() = CommonAttrs(true);
    attrs.specific() = BarAttrs(1.0f);

    nsTArray<Op> ops;
    ops.AppendElement(SetAttrs(kids[0], attrs));

    EXPECT_TRUE(SendTestUnionWithCxx(ops));
  }

  void TestNsIntRegion() {
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

    EXPECT_TRUE(SendTestNsIntRegion(ra));
  }

  ~TestDataStructuresChild() = default;
};

IPDL_TEST(TestDataStructures) {
  for (uint32_t i = 0; i < nactors; ++i) {
    auto sub = MakeRefPtr<TestDataStructuresSubParent>(i);
    EXPECT_TRUE(mActor->SendPTestDataStructuresSubConstructor(sub, i))
        << "can't alloc actor";
    mActor->kids.AppendElement(WrapNotNull(sub));
  }

  EXPECT_TRUE(mActor->SendStart()) << "can't send Start()";
}

}  // namespace mozilla::_ipdltest
