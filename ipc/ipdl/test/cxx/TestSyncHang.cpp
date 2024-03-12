#include "TestSyncHang.h"
#include "base/task.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "IPDLUnitTests.h"  // fail etc.

using std::string;
using std::vector;

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

mozilla::ipc::GeckoChildProcessHost* gSyncHangSubprocess;

TestSyncHangParent::TestSyncHangParent() { MOZ_COUNT_CTOR(TestSyncHangParent); }

TestSyncHangParent::~TestSyncHangParent() {
  MOZ_COUNT_DTOR(TestSyncHangParent);
}

void DeleteSyncHangSubprocess(MessageLoop* uiLoop) {
  gSyncHangSubprocess->Destroy();
  gSyncHangSubprocess = nullptr;
}

void DeferredSyncHangParentShutdown() {
  // ping to DeleteSubprocess
  XRE_GetIOMessageLoop()->PostTask(
      NewRunnableFunction("DeleteSyncHangSubprocess", DeleteSyncHangSubprocess,
                          MessageLoop::current()));
}

void TestSyncHangParent::Main() {
  vector<string> args;
  args.push_back("fake/path");
  gSyncHangSubprocess =
      new mozilla::ipc::GeckoChildProcessHost(GeckoProcessType_Plugin);
  bool launched = gSyncHangSubprocess->SyncLaunch(args, 2);
  if (launched)
    fail("Calling SyncLaunch with an invalid path should return false");

  MessageLoop::current()->PostTask(NewRunnableFunction(
      "DeferredSyncHangParentShutdown", DeferredSyncHangParentShutdown));
  Close();
}

//-----------------------------------------------------------------------------
// child

TestSyncHangChild::TestSyncHangChild() { MOZ_COUNT_CTOR(TestSyncHangChild); }

TestSyncHangChild::~TestSyncHangChild() { MOZ_COUNT_DTOR(TestSyncHangChild); }

mozilla::ipc::IPCResult TestSyncHangChild::RecvUnusedMessage() {
  return IPC_OK();
}

}  // namespace _ipdltest
}  // namespace mozilla
