#include "TestCancel.h"

#include "IPDLUnitTests.h"  // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestCancelParent::TestCancelParent() { MOZ_COUNT_CTOR(TestCancelParent); }

TestCancelParent::~TestCancelParent() { MOZ_COUNT_DTOR(TestCancelParent); }

void TestCancelParent::Main() {
  if (SendTest1_1()) fail("sending Test1_1");

  uint32_t value = 0;
  if (!SendCheckChild(&value)) fail("Test1 CheckChild");

  if (value != 12) fail("Test1 CheckChild reply");
}

mozilla::ipc::IPCResult TestCancelParent::RecvDone1() {
  if (!SendStart2()) fail("sending Start2");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelParent::RecvTest2_1() {
  if (SendTest2_2()) fail("sending Test2_2");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelParent::RecvStart3() {
  if (SendTest3_1()) fail("sending Test3_1");

  uint32_t value = 0;
  if (!SendCheckChild(&value)) fail("Test1 CheckChild");

  if (value != 12) fail("Test1 CheckChild reply");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelParent::RecvTest3_2() {
  GetIPCChannel()->CancelCurrentTransaction();
  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelParent::RecvDone() {
  MessageLoop::current()->PostTask(NewNonOwningRunnableMethod(
      "ipc::IToplevelProtocol::Close", this, &TestCancelParent::Close));
  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelParent::RecvCheckParent(uint32_t* reply) {
  *reply = 12;
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// child

mozilla::ipc::IPCResult TestCancelChild::RecvTest1_1() {
  GetIPCChannel()->CancelCurrentTransaction();

  uint32_t value = 0;
  if (!SendCheckParent(&value)) fail("Test1 CheckParent");

  if (value != 12) fail("Test1 CheckParent reply");

  if (!SendDone1()) fail("Test1 CheckParent");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelChild::RecvStart2() {
  if (!SendTest2_1()) fail("sending Test2_1");

  if (!SendStart3()) fail("sending Start3");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelChild::RecvTest2_2() {
  GetIPCChannel()->CancelCurrentTransaction();
  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelChild::RecvTest3_1() {
  if (SendTest3_2()) fail("sending Test3_2");

  uint32_t value = 0;
  if (!SendCheckParent(&value)) fail("Test1 CheckParent");

  if (value != 12) fail("Test1 CheckParent reply");

  if (!SendDone()) fail("sending Done");

  return IPC_OK();
}

mozilla::ipc::IPCResult TestCancelChild::RecvCheckChild(uint32_t* reply) {
  *reply = 12;
  return IPC_OK();
}

TestCancelChild::TestCancelChild() { MOZ_COUNT_CTOR(TestCancelChild); }

TestCancelChild::~TestCancelChild() { MOZ_COUNT_DTOR(TestCancelChild); }

}  // namespace _ipdltest
}  // namespace mozilla
