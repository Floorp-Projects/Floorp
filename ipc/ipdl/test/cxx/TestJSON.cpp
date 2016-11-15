#include "TestJSON.h"

#include "IPDLUnitTests.h"      // fail etc.

#define test_assert(_cond, _msg) \
    if (!(_cond)) fail(_msg)

namespace mozilla {
namespace _ipdltest {

static nsString
String(const char* const str)
{
    return NS_ConvertUTF8toUTF16(str);
}

static void
Array123(InfallibleTArray<JSONVariant>& a123)
{
    a123.AppendElement(1);  a123.AppendElement(2);  a123.AppendElement(3);

    test_assert(a123 == a123, "operator== is broken");
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
    InfallibleTArray<JSONVariant> outer;

    outer.AppendElement(void_t());
    outer.AppendElement(null_t());
    outer.AppendElement(true);
    outer.AppendElement(1.25);
    outer.AppendElement(String("test string"));

    outer.AppendElement(handle);

    InfallibleTArray<JSONVariant> tmp;
    Array123(tmp);
    outer.AppendElement(tmp);

    InfallibleTArray<KeyValue> obj;
    obj.AppendElement(KeyValue(String("undefined"), void_t()));
    obj.AppendElement(KeyValue(String("null"), null_t()));
    obj.AppendElement(KeyValue(String("true"), true));
    obj.AppendElement(KeyValue(String("1.25"), 1.25));
    obj.AppendElement(KeyValue(String("string"), String("value")));
    obj.AppendElement(KeyValue(String("handle"), handle));
    InfallibleTArray<JSONVariant> tmp2;
    Array123(tmp2);
    obj.AppendElement(KeyValue(String("array"), tmp2));

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


mozilla::ipc::IPCResult
TestJSONParent::RecvTest(const JSONVariant& i,
                         JSONVariant* o)
{
    test_assert(i == i, "operator== is broken");
    test_assert(i == MakeTestVariant(mKid), "inparam mangled en route");

    *o = i;

    test_assert(i == *o, "operator= is broken");

    return IPC_OK();
}


//-----------------------------------------------------------------------------
// child

mozilla::ipc::IPCResult
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
    return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
