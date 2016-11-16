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

mozilla::ipc::IPCResult
TestRPCParent::RecvTest1_Start(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_InnerQuery(&result))
    fail("SendTest1_InnerQuery");
  if (result != 300)
    fail("Wrong result (expected 300)");

  *aResult = 100;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCParent::RecvTest1_InnerEvent(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_NoReenter(&result))
    fail("SendTest1_NoReenter");
  if (result != 400)
    fail("Wrong result (expected 400)");

  *aResult = 200;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCParent::RecvTest2_Start()
{
  // Send a CPOW. During this time, we must NOT process the RPC message, as
  // we could start receiving CPOW replies out-of-order.
  if (!SendTest2_FirstUrgent())
    fail("SendTest2_FirstUrgent");

  MOZ_ASSERT(!reentered_);
  resolved_first_cpow_ = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCParent::RecvTest2_OutOfOrder()
{
  // Send a CPOW. If this RPC call was initiated while waiting for the first
  // CPOW to resolve, replies will be processed out of order, and we'll crash.
  if (!SendTest2_SecondUrgent())
    fail("SendTest2_SecondUrgent");

  reentered_ = true;
  return IPC_OK();
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

mozilla::ipc::IPCResult
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

  Close();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCChild::RecvTest1_InnerQuery(uint32_t* aResult)
{
  uint32_t result;
  if (!SendTest1_InnerEvent(&result))
    fail("SendTest1_InnerEvent");
  if (result != 200)
    fail("Wrong result (expected 200)");

  *aResult = 300;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCChild::RecvTest1_NoReenter(uint32_t* aResult)
{
  *aResult = 400;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCChild::RecvTest2_FirstUrgent()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
TestRPCChild::RecvTest2_SecondUrgent()
{
  return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
