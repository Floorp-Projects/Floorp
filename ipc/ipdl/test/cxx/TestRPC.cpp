#include "TestRPC.h"

#include "IPDLUnitTests.h"      // fail etc.
#include <unistd.h>
#if !defined(OS_POSIX)
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
TestRPCParent::AnswerTest1_Start(uint32_t* aResult)
{
  uint32_t result;
  if (!CallTest1_InnerQuery(&result))
    fail("CallTest1_InnerQuery");
  if (result != 300)
    fail("Wrong result (expected 300)");

  *aResult = 100;
  return true;
}

bool
TestRPCParent::AnswerTest1_InnerEvent(uint32_t* aResult)
{
  uint32_t result;
  if (!CallTest1_NoReenter(&result))
    fail("CallTest1_NoReenter");
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
  if (!CallTest2_FirstUrgent())
    fail("CallTest2_FirstUrgent");

  MOZ_ASSERT(!reentered_);
  resolved_first_cpow_ = true;
  return true;
}

bool
TestRPCParent::AnswerTest2_OutOfOrder()
{
  // Send a CPOW. If this RPC call was initiated while waiting for the first
  // CPOW to resolve, replies will be processed out of order, and we'll crash.
  if (!CallTest2_SecondUrgent())
    fail("CallTest2_SecondUrgent");

  reentered_ = true;
  return true;
}

bool
TestRPCParent::RecvTest3_Start(uint32_t* aResult)
{
  if (!CallTest3_WakeUp(aResult))
    fail("CallTest3_WakeUp");

  return true;
}

bool
TestRPCParent::AnswerTest3_InnerEvent(uint32_t* aResult)
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
  if (!CallTest1_Start(&result))
    fail("CallTest1_Start");
  if (result != 100)
    fail("Wrong result (expected 100)");

  if (!SendTest2_Start())
    fail("SendTest2_Start");

  if (!CallTest2_OutOfOrder())
    fail("CallTest2_OutOfOrder");

  result = 0;
  if (!SendTest3_Start(&result))
    fail("SendTest3_Start");
  if (result != 200)
    fail("Wrong result (expected 200)");

  Close();
  return true;
}

bool
TestRPCChild::AnswerTest1_InnerQuery(uint32_t* aResult)
{
  uint32_t result;
  if (!CallTest1_InnerEvent(&result))
    fail("CallTest1_InnerEvent");
  if (result != 200)
    fail("Wrong result (expected 200)");

  *aResult = 300;
  return true;
}

bool
TestRPCChild::AnswerTest1_NoReenter(uint32_t* aResult)
{
  *aResult = 400;
  return true;
}

bool
TestRPCChild::AnswerTest2_FirstUrgent()
{
  return true;
}

bool
TestRPCChild::AnswerTest2_SecondUrgent()
{
  return true;
}

bool
TestRPCChild::AnswerTest3_WakeUp(uint32_t* aResult)
{
  if (!CallTest3_InnerEvent(aResult))
    fail("CallTest3_InnerEvent");

  return true;
}

} // namespace _ipdltest
} // namespace mozilla
