#include "TestRPC.h"

#include "IPDLUnitTests.h"      // fail etc.
#if defined(OS_POSIX)
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestRPCParent::TestRPCParent()
 : reentered_(false),
   resolved_first_cpow_(false)
{
  MOZ_COUNT_CTOR(TestRPCParent);
}

TestRPCParent::~TestRPCParent()
{
  MOZ_COUNT_DTOR(TestRPCParent);
}

void
TestRPCParent::Main()
{
  if (!SendStart())
    fail("sending Start");
}

bool
TestRPCParent::RecvTest1_Start(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_InnerQuery(&result))
    fail("SendTest1_InnerQuery");
  if (result != 300)
    fail("Wrong result (expected 300)");

  *aResult = 100;
  return true;
}

bool
TestRPCParent::RecvTest1_InnerEvent(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_NoReenter(&result))
    fail("SendTest1_NoReenter");
  if (result != 400)
    fail("Wrong result (expected 400)");

  *aResult = 200;
  return true;
}

bool
TestRPCParent::RecvTest2_Start()
{
  // Send a CPOW. During this time, we must NOT process the RPC message, as
  // we could start receiving CPOW replies out-of-order.
  if (!SendTest2_FirstUrgent())
    fail("SendTest2_FirstUrgent");

  MOZ_ASSERT(!reentered_);
  resolved_first_cpow_ = true;
  return true;
}

bool
TestRPCParent::RecvTest2_OutOfOrder()
{
  // Send a CPOW. If this RPC call was initiated while waiting for the first
  // CPOW to resolve, replies will be processed out of order, and we'll crash.
  if (!SendTest2_SecondUrgent())
    fail("SendTest2_SecondUrgent");

  reentered_ = true;
  return true;
}

bool
TestRPCParent::RecvTest3_Start(uint32_t* aResult)
{
  if (!SendTest3_WakeUp(aResult))
    fail("SendTest3_WakeUp");

  return true;
}

bool
TestRPCParent::RecvTest3_InnerEvent(uint32_t* aResult)
{
  *aResult = 200;
  return true;
}

//-----------------------------------------------------------------------------
// child


TestRPCChild::TestRPCChild()
{
    MOZ_COUNT_CTOR(TestRPCChild);
}

TestRPCChild::~TestRPCChild()
{
    MOZ_COUNT_DTOR(TestRPCChild);
}

bool
TestRPCChild::RecvStart()
{
  uint32_t result;
  if (!SendTest1_Start(&result))
    fail("SendTest1_Start");
  if (result != 100)
    fail("Wrong result (expected 100)");

  if (!SendTest2_Start())
    fail("SendTest2_Start");

  if (!SendTest2_OutOfOrder())
    fail("SendTest2_OutOfOrder");

  result = 0;
  if (!SendTest3_Start(&result))
    fail("SendTest3_Start");
  if (result != 200)
    fail("Wrong result (expected 200)");

  Close();
  return true;
}

bool
TestRPCChild::RecvTest1_InnerQuery(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_InnerEvent(&result))
    fail("SendTest1_InnerEvent");
  if (result != 200)
    fail("Wrong result (expected 200)");

  *aResult = 300;
  return true;
}

bool
TestRPCChild::RecvTest1_NoReenter(uint32_t* aResult)
{
  *aResult = 400;
  return true;
}

bool
TestRPCChild::RecvTest2_FirstUrgent()
{
  return true;
}

bool
TestRPCChild::RecvTest2_SecondUrgent()
{
  return true;
}

bool
TestRPCChild::RecvTest3_WakeUp(uint32_t* aResult)
{
  if (!SendTest3_InnerEvent(aResult))
    fail("SendTest3_InnerEvent");

  return true;
}

} // namespace _ipdltest
} // namespace mozilla
