#include <algorithm>            // std::equal

#include "TestJSON.h"

#include "IPDLUnitTests.h"      // fail etc.

#define test_assert(_cond, _msg) \
    if (!(_cond)) fail(_msg)

namespace mozilla {
namespace _ipdltest {

// XXX move me into nsTArray
template<typename T>
bool
operator==(const nsTArray<T>& a, const nsTArray<T>& b)
{
    return (a.Length() == b.Length() && 
            std::equal(a.Elements(), a.Elements() + a.Length(),
                       b.Elements()));
}

bool
operator==(const Key& a, const Key& b)
{
    return a.get_nsString() == b.get_nsString();
}

bool operator==(const JSONVariant& a, const JSONVariant& b);

bool
operator==(const KeyValue& a, const KeyValue& b)
{
    return a.key() == b.key() && a.value() == b.value();
}

// XXX start generating me in IPDL code
bool
operator==(const JSONVariant& a, const JSONVariant& b)
{
    typedef JSONVariant t;

    if (a.type() != b.type())
        return false;

    switch(a.type()) {
    case t::Tvoid_t:
    case t::Tnull_t:
        return true;
    case t::Tbool:
        return a.get_bool() == b.get_bool();
    case t::Tint:
        return a.get_int() == b.get_int();
    case t::Tdouble:
        return a.get_double() == b.get_double();
    case t::TnsString:
        return a.get_nsString() == b.get_nsString();
    case t::TPTestHandleParent:
        return a.get_PTestHandleParent() == b.get_PTestHandleParent();
    case t::TPTestHandleChild:
        return a.get_PTestHandleChild() == b.get_PTestHandleChild();
    case t::TArrayOfKeyValue:
        return a.get_ArrayOfKeyValue() == b.get_ArrayOfKeyValue();
    case t::TArrayOfJSONVariant:
        return a.get_ArrayOfJSONVariant() == b.get_ArrayOfJSONVariant();
    default:
        NS_RUNTIMEABORT("unreached");
        return false;
    }
}

static nsString
String(const char* const str)
{
    return NS_ConvertUTF8toUTF16(str);
}

static nsTArray<JSONVariant>
Array123()
{
    nsTArray<JSONVariant> a123;
    a123.AppendElement(1);  a123.AppendElement(2);  a123.AppendElement(3);

    test_assert(a123 == a123, "operator== is broken");

    return a123;
}

template<class HandleT>
JSONVariant
MakeTestVariant(HandleT* handle)
{
    // In JS syntax:
    //
    //   return [
    //     undefined, null, true, 1.25, "test string",
    //     handle,
    //     [ 1, 2, 3 ],
    //     { "undefined" : undefined,
    //       "null"      : null,
    //       "true"      : true,
    //       "1.25"      : 1.25,
    //       "string"    : "string"
    //       "handle"    : handle,
    //       "array"     : [ 1, 2, 3 ]
    //     }
    //   ]
    //
    nsTArray<JSONVariant> outer;

    outer.AppendElement(void_t());
    outer.AppendElement(null_t());
    outer.AppendElement(true);
    outer.AppendElement(1.25);
    outer.AppendElement(String("test string"));

    outer.AppendElement(handle);

    outer.AppendElement(Array123());

    nsTArray<KeyValue> obj;
    obj.AppendElement(KeyValue(String("undefined"), void_t()));
    obj.AppendElement(KeyValue(String("null"), null_t()));
    obj.AppendElement(KeyValue(String("true"), true));
    obj.AppendElement(KeyValue(String("1.25"), 1.25));
    obj.AppendElement(KeyValue(String("string"), String("value")));
    obj.AppendElement(KeyValue(String("handle"), handle));
    obj.AppendElement(KeyValue(String("array"), Array123()));

    outer.AppendElement(obj);

    test_assert(outer == outer, "operator== is broken");

    return JSONVariant(outer);
}

//-----------------------------------------------------------------------------
// parent

void
TestJSONParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}


bool
TestJSONParent::RecvTest(const JSONVariant& i,
                         JSONVariant* o)
{
    test_assert(i == i, "operator== is broken");
    test_assert(i == MakeTestVariant(mKid), "inparam mangled en route");

    *o = i;

    test_assert(i == *o, "operator= is broken");

    return true;
}


//-----------------------------------------------------------------------------
// child

bool
TestJSONChild::RecvStart()
{
    if (!SendPTestHandleConstructor())
        fail("sending Handle ctor");

    JSONVariant i(MakeTestVariant(mKid));
    test_assert(i == i, "operator== is broken");
    test_assert(i == MakeTestVariant(mKid), "copy ctor is broken");

    JSONVariant o;
    if (!SendTest(i, &o))
        fail("sending Test");

    test_assert(i == o, "round-trip mangled input data");
    test_assert(o == MakeTestVariant(mKid), "outparam mangled en route");

    Close();
    return true;
}


} // namespace _ipdltest
} // namespace mozilla
