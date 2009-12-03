#include "TestArrays.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

static const uint32 nactors = 10;

#define test_assert(_cond, _msg) \
    if (!(_cond)) fail(_msg)

template<typename T>
static void
assert_arrays_equal(nsTArray<T> a, nsTArray<T> b)
{
    test_assert(a.Length() == b.Length(), "Length()s different");
    for (uint32 i = 0; i < a.Length(); ++i)
        test_assert(a[i] == b[i], "values different");
}

inline static TestArraysSub&
Cast(PTestArraysSubParent* a)
{
    return *static_cast<TestArraysSub*>(a);
}

inline static TestArraysSub&
Cast(PTestArraysSubChild* a)
{
    return *static_cast<TestArraysSub*>(a);
}

//-----------------------------------------------------------------------------
// parent

TestArraysParent::TestArraysParent()
{
    MOZ_COUNT_CTOR(TestArraysParent);
}

TestArraysParent::~TestArraysParent()
{
    MOZ_COUNT_DTOR(TestArraysParent);
}

void
TestArraysParent::Main()
{
    for (uint32 i = 0; i < nactors; ++i)
        if (!SendPTestArraysSubConstructor(i))
            fail("can't alloc actor");

    if (!SendStart())
        fail("can't send Start()");
}

bool
TestArraysParent::DeallocPTestArraysSub(PTestArraysSubParent* actor)
{
    test_assert(Cast(actor).mI == Cast(mKids[0]).mI,
                "dtor sent to wrong actor");
    mKids.RemoveElementAt(0);
    delete actor;
    if (mKids.Length() > 0)
        return true;

    return true;
}

bool TestArraysParent::RecvTest1(
        const nsTArray<int>& ia,
        nsTArray<int>* oa)
{
    test_assert(5 == ia.Length(), "wrong length");
    for (int i = 0; i < 5; ++i)
        test_assert(i == ia[i], "wrong value");

    *oa = ia;

    return true;
}

bool TestArraysParent::RecvTest2(
        const nsTArray<PTestArraysSubParent*>& i1,
        nsTArray<PTestArraysSubParent*>* o1)
{
    test_assert(nactors == i1.Length(), "wrong #actors");
    for (uint32 i = 0; i < i1.Length(); ++i)
        test_assert(i == Cast(i1[i]).mI, "wrong mI value");
    *o1 = i1;
    return true;
}

bool TestArraysParent::RecvTest3(
        const IntDouble& i1,
        const IntDouble& i2,
        IntDouble* o1,
        IntDouble* o2)
{   
    test_assert(42 == i1.get_int(), "wrong value");
    test_assert(4.0 == i2.get_double(), "wrong value");

    *o1 = i1;
    *o2 = i2;

    return true;
}

bool TestArraysParent::RecvTest4(
        const nsTArray<IntDouble>& i1,
        nsTArray<IntDouble>* o1)
{
    test_assert(4 == i1.Length(), "wrong length");
    test_assert(1 == i1[0].get_int(), "wrong value");
    test_assert(2.0 == i1[1].get_double(), "wrong value");
    test_assert(3 == i1[2].get_int(), "wrong value");
    test_assert(4.0 == i1[3].get_double(), "wrong value");    

    *o1 = i1;

    return true;
}

bool TestArraysParent::RecvTest5(
        const IntDoubleArrays& i1,
        const IntDoubleArrays& i2,
        const IntDoubleArrays& i3,
        IntDoubleArrays* o1,
        IntDoubleArrays* o2,
        IntDoubleArrays* o3)
{
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

    return true;
}

bool TestArraysParent::RecvTest6(
        const nsTArray<IntDoubleArrays>& i1,
        nsTArray<IntDoubleArrays>* o1)
{
    test_assert(3 == i1.Length(), "wrong length");

    IntDoubleArrays id1(i1[0]);
    test_assert(42 == id1.get_int(), "wrong value");

    nsTArray<int> i2a = i1[1].get_ArrayOfint();
    test_assert(3 == i2a.Length(), "wrong length");
    test_assert(1 == i2a[0], "wrong value");
    test_assert(2 == i2a[1], "wrong value");
    test_assert(3 == i2a[2], "wrong value");

    nsTArray<double> i3a = i1[2].get_ArrayOfdouble();
    test_assert(3 == i3a.Length(), "wrong length");
    test_assert(1.0 == i3a[0], "wrong value");
    test_assert(2.0 == i3a[1], "wrong value");
    test_assert(3.0 == i3a[2], "wrong value");

    o1->AppendElement(id1);
    o1->AppendElement(IntDoubleArrays(i2a));
    o1->AppendElement(IntDoubleArrays(i3a));

    return true;
}

bool TestArraysParent::RecvTest7(
        const Actors& i1,
        const Actors& i2,
        const Actors& i3,
        Actors* o1,
        Actors* o2,
        Actors* o3)
{
    test_assert(42 == i1.get_int(), "wrong value");

    nsTArray<int> i2a(i2.get_ArrayOfint());
    test_assert(3 == i2a.Length(), "wrong length");
    test_assert(1 == i2a[0], "wrong value");
    test_assert(2 == i2a[1], "wrong value");
    test_assert(3 == i2a[2], "wrong value");

    assert_arrays_equal(mKids, i3.get_ArrayOfPTestArraysSubParent());

    *o1 = 42;
    *o2 = i2a;
    *o3 = mKids;

    return true;
}

bool TestArraysParent::RecvTest8(
        const nsTArray<Actors>& i1,
        nsTArray<Actors>* o1)
{
    test_assert(3 == i1.Length(), "wrong length");
    test_assert(42 == i1[0].get_int(), "wrong value");

    const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
    test_assert(3 == i2a.Length(), "wrong length");
    test_assert(1 == i2a[0], "wrong value");
    test_assert(2 == i2a[1], "wrong value");
    test_assert(3 == i2a[2], "wrong value");

    assert_arrays_equal(mKids, i1[2].get_ArrayOfPTestArraysSubParent());

    *o1 = i1;

    return true;
}

bool TestArraysParent::RecvTest9(
        const Unions& i1,
        const Unions& i2,
        const Unions& i3,
        const Unions& i4,
        Unions* o1,
        Unions* o2,
        Unions* o3,
        Unions* o4)
{
    test_assert(42 == i1.get_int(), "wrong value");

    const nsTArray<int>& i2a = i2.get_ArrayOfint();
    test_assert(3 == i2a.Length(), "wrong length");
    test_assert(1 == i2a[0], "wrong value");
    test_assert(2 == i2a[1], "wrong value");
    test_assert(3 == i2a[2], "wrong value");

    assert_arrays_equal(mKids, i3.get_ArrayOfPTestArraysSubParent());

    const nsTArray<PTestArraysSubParent*>& i4a = 
        i4.get_ArrayOfActors()[0].get_ArrayOfPTestArraysSubParent();
    assert_arrays_equal(mKids, i4a);

    *o1 = i1;
    *o2 = i2;
    *o3 = i3;
    *o4 = i4;

    return true;
}

bool TestArraysParent::RecvTest10(
        const nsTArray<Unions>& i1,
        nsTArray<Unions>* o1)
{
    test_assert(42 == i1[0].get_int(), "wrong value");

    const nsTArray<int>& i2a = i1[1].get_ArrayOfint();
    test_assert(3 == i2a.Length(), "wrong length");
    test_assert(1 == i2a[0], "wrong value");
    test_assert(2 == i2a[1], "wrong value");
    test_assert(3 == i2a[2], "wrong value");

    assert_arrays_equal(mKids, i1[2].get_ArrayOfPTestArraysSubParent());

    const nsTArray<PTestArraysSubParent*>& i4a = 
        i1[3].get_ArrayOfActors()[0].get_ArrayOfPTestArraysSubParent();
    assert_arrays_equal(mKids, i4a);

    *o1 = i1;

    return true;
}


//-----------------------------------------------------------------------------
// child

TestArraysChild::TestArraysChild()
{
    MOZ_COUNT_CTOR(TestArraysChild);
}

TestArraysChild::~TestArraysChild()
{
    MOZ_COUNT_DTOR(TestArraysChild);
}

bool
TestArraysChild::RecvStart()
{
    puts("[TestArraysChild] starting");

    Test1();
    Test2();
    Test3();
    Test4();
    Test5();
    Test6();
    Test7();
    Test8();
    Test9();
    Test10();

    for (uint32 i = 0; i < nactors; ++i)
        if (!PTestArraysSubChild::Send__delete__(mKids[i]))
            fail("can't send dtor");

    Close();

    return true;
}

void
TestArraysChild::Test1()
{
    nsTArray<int> ia;

    for (int i = 0; i < 5; ++i)
        ia.AppendElement(i);

    nsTArray<int> oa;
    if (!SendTest1(ia, &oa))
        fail("can't send Test1");

    assert_arrays_equal(ia, oa);

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test2()
{
    nsTArray<PTestArraysSubChild*> oa;
    if (!SendTest2(mKids, &oa))
        fail("can't send Test2");
    assert_arrays_equal(mKids, oa);

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test3()
{
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

void
TestArraysChild::Test4()
{
    nsTArray<IntDouble> i1;
    i1.AppendElement(IntDouble(int(1)));
    i1.AppendElement(IntDouble(2.0));
    i1.AppendElement(IntDouble(int(3)));
    i1.AppendElement(IntDouble(4.0));

    nsTArray<IntDouble> o1;
    if (!SendTest4(i1, &o1))
        fail("can't send Test4");

    // TODO Union::operator==()
    test_assert(i1.Length() == o1.Length(), "wrong length");
    test_assert(1 == o1[0].get_int(), "wrong value");
    test_assert(2.0 == o1[1].get_double(), "wrong value");
    test_assert(3 == o1[2].get_int(), "wrong value");
    test_assert(4.0 == o1[3].get_double(), "wrong value");

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test5()
{
    IntDoubleArrays i1(int(42));
    nsTArray<int> i2;
    i2.AppendElement(1); i2.AppendElement(2); i2.AppendElement(3);
    nsTArray<double> i3;
    i3.AppendElement(1.0); i3.AppendElement(2.0); i3.AppendElement(3.0);

    IntDoubleArrays o1, o2, o3;
    if (!SendTest5(i1, IntDoubleArrays(i2), IntDoubleArrays(i3),
                   &o1, &o2, &o3))
        fail("can't send Test5");

    test_assert(42 == o1.get_int(), "wrong value");
    assert_arrays_equal(i2, o2.get_ArrayOfint());
    assert_arrays_equal(i3, o3.get_ArrayOfdouble());

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test6()
{
    IntDoubleArrays id1(int(42));
    nsTArray<int> id2;
    id2.AppendElement(1); id2.AppendElement(2); id2.AppendElement(3);
    nsTArray<double> id3;
    id3.AppendElement(1.0); id3.AppendElement(2.0); id3.AppendElement(3.0);

    nsTArray<IntDoubleArrays> i1;
    i1.AppendElement(id1);
    i1.AppendElement(IntDoubleArrays(id2));
    i1.AppendElement(IntDoubleArrays(id3));

    nsTArray<IntDoubleArrays> o1;
    if (!SendTest6(i1, &o1))
        fail("can't send Test6");

    test_assert(3 == o1.Length(), "wrong length");
    IntDoubleArrays od1(o1[0]);
    nsTArray<int> od2 = o1[1].get_ArrayOfint();
    nsTArray<double> od3 = o1[2].get_ArrayOfdouble();

    test_assert(42 == od1.get_int(), "wrong value");
    assert_arrays_equal(id2, od2);
    assert_arrays_equal(id3, od3);

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test7()
{
    Actors i1(42);
    nsTArray<int> i2a;
    i2a.AppendElement(1);  i2a.AppendElement(2);  i2a.AppendElement(3);

    Actors o1, o2, o3;
    if (!SendTest7(i1, Actors(i2a), Actors(mKids), &o1, &o2, &o3))
        fail("can't send Test7");

    test_assert(42 == o1.get_int(), "wrong value");
    assert_arrays_equal(i2a, o2.get_ArrayOfint());
    assert_arrays_equal(mKids, o3.get_ArrayOfPTestArraysSubChild());

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test8()
{
    Actors i1e(42);
    nsTArray<int> i2a;
    i2a.AppendElement(1);  i2a.AppendElement(2);  i2a.AppendElement(3);

    nsTArray<Actors> i1;
    i1.AppendElement(i1e);
    i1.AppendElement(i2a);
    i1.AppendElement(mKids);

    nsTArray<Actors> o1;
    if (!SendTest8(i1, &o1))
        fail("can't send Test8");

    test_assert(3 == o1.Length(), "wrong length");
    test_assert(42 == o1[0].get_int(), "wrong value");
    assert_arrays_equal(i2a, o1[1].get_ArrayOfint());
    assert_arrays_equal(mKids, o1[2].get_ArrayOfPTestArraysSubChild());

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test9()
{
    Unions i1(int(42));

    nsTArray<int> i2a;
    i2a.AppendElement(1);
    i2a.AppendElement(2);
    i2a.AppendElement(3);

    nsTArray<Actors> i4a;
    i4a.AppendElement(mKids);

    Unions o1, o2, o3, o4;
    if (!SendTest9(i1, Unions(i2a), Unions(mKids), Unions(i4a),
                   &o1, &o2, &o3, &o4))
        fail("can't send Test9");

    test_assert(42 == o1.get_int(), "wrong value");
    assert_arrays_equal(i2a, o2.get_ArrayOfint());
    assert_arrays_equal(mKids, o3.get_ArrayOfPTestArraysSubChild());
    assert_arrays_equal(
        mKids,
        o4.get_ArrayOfActors()[0].get_ArrayOfPTestArraysSubChild());

    printf("  passed %s\n", __FUNCTION__);
}

void
TestArraysChild::Test10()
{
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
    if (!SendTest10(i1, &o1))
        fail("can't send Test10");

    test_assert(4 == o1.Length(), "wrong length");
    test_assert(42 == o1[0].get_int(), "wrong value");
    assert_arrays_equal(i2a, o1[1].get_ArrayOfint());
    assert_arrays_equal(mKids, o1[2].get_ArrayOfPTestArraysSubChild());
    assert_arrays_equal(
        mKids,
        o1[3].get_ArrayOfActors()[0].get_ArrayOfPTestArraysSubChild());

    printf("  passed %s\n", __FUNCTION__);
}


} // namespace _ipdltest
} // namespace mozilla
