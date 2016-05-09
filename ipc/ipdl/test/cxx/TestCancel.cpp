#include "TestCancel.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestCancelParent::TestCancelParent()
{
    MOZ_COUNT_CTOR(TestCancelParent);
}

TestCancelParent::~TestCancelParent()
{
    MOZ_COUNT_DTOR(TestCancelParent);
}

void
TestCancelParent::Main()
{
    if (SendTest1_1())
	fail("sending Test1_1");

    uint32_t value = 0;
    if (!SendCheckChild(&value))
	fail("Test1 CheckChild");

    if (value != 12)
	fail("Test1 CheckChild reply");
}

bool
TestCancelParent::RecvDone1()
{
    if (!SendStart2())
	fail("sending Start2");

    return true;
}

bool
TestCancelParent::RecvTest2_1()
{
    if (SendTest2_2())
	fail("sending Test2_2");

    return true;
}

bool
TestCancelParent::RecvStart3()
{
    if (SendTest3_1())
	fail("sending Test3_1");

    uint32_t value = 0;
    if (!SendCheckChild(&value))
	fail("Test1 CheckChild");

    if (value != 12)
	fail("Test1 CheckChild reply");

    return true;
}

bool
TestCancelParent::RecvTest3_2()
{
    GetIPCChannel()->CancelCurrentTransaction();
    return true;
}

bool
TestCancelParent::RecvDone()
{
    MessageLoop::current()->PostTask(
	NewNonOwningRunnableMethod(this, &TestCancelParent::Close));
    return true;
}

bool
TestCancelParent::RecvCheckParent(uint32_t *reply)
{
    *reply = 12;
    return true;
}

//-----------------------------------------------------------------------------
// child

bool
TestCancelChild::RecvTest1_1()
{
    GetIPCChannel()->CancelCurrentTransaction();

    uint32_t value = 0;
    if (!SendCheckParent(&value))
	fail("Test1 CheckParent");

    if (value != 12)
	fail("Test1 CheckParent reply");

    if (!SendDone1())
	fail("Test1 CheckParent");

    return true;
}

bool
TestCancelChild::RecvStart2()
{
    if (!SendTest2_1())
	fail("sending Test2_1");

    if (!SendStart3())
	fail("sending Start3");

    return true;
}

bool
TestCancelChild::RecvTest2_2()
{
    GetIPCChannel()->CancelCurrentTransaction();
    return true;
}

bool
TestCancelChild::RecvTest3_1()
{
    if (SendTest3_2())
	fail("sending Test3_2");

    uint32_t value = 0;
    if (!SendCheckParent(&value))
	fail("Test1 CheckParent");

    if (value != 12)
	fail("Test1 CheckParent reply");

    if (!SendDone())
	fail("sending Done");

    return true;
}

bool
TestCancelChild::RecvCheckChild(uint32_t *reply)
{
    *reply = 12;
    return true;
}

TestCancelChild::TestCancelChild()
{
    MOZ_COUNT_CTOR(TestCancelChild);
}

TestCancelChild::~TestCancelChild()
{
    MOZ_COUNT_DTOR(TestCancelChild);
}

} // namespace _ipdltest
} // namespace mozilla
