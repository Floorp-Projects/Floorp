#include "TestOffMainThreadPainting.h"

#include "IPDLUnitTests.h"      // fail etc.
#include "mozilla/Unused.h"
#include <prinrval.h>
#include <prthread.h>

namespace mozilla {
namespace _ipdltest {

TestOffMainThreadPaintingParent::TestOffMainThreadPaintingParent()
 : mAsyncMessages(0),
   mSyncMessages(0)
{
}

TestOffMainThreadPaintingParent::~TestOffMainThreadPaintingParent()
{
}

void
TestOffMainThreadPaintingParent::Main()
{
  ipc::Endpoint<PTestPaintThreadParent> parentPipe;
  ipc::Endpoint<PTestPaintThreadChild> childPipe;
  nsresult rv = PTestPaintThread::CreateEndpoints(
    base::GetCurrentProcId(),
    OtherPid(),
    &parentPipe,
    &childPipe);
  if (NS_FAILED(rv)) {
    fail("create pipes");
  }

  mPaintActor = new TestPaintThreadParent(this);
  if (!mPaintActor->Bind(Move(parentPipe))) {
    fail("bind parent pipe");
  }

  if (!SendStartTest(Move(childPipe))) {
    fail("sending Start");
  }
}

ipc::IPCResult
TestOffMainThreadPaintingParent::RecvFinishedLayout(const uint64_t& aTxnId)
{
  if (!mPaintedTxn || mPaintedTxn.value() != aTxnId) {
    fail("received transaction before receiving paint");
  }
  mPaintedTxn = Nothing();
  mCompletedTxn = Some(aTxnId);
  return IPC_OK();
}

void
TestOffMainThreadPaintingParent::NotifyFinishedPaint(const uint64_t& aTxnId)
{
  if (mCompletedTxn && mCompletedTxn.value() >= aTxnId) {
    fail("received paint after receiving transaction");
  }
  if (mPaintedTxn) {
    fail("painted again before completing previous transaction");
  }
  mPaintedTxn = Some(aTxnId);
}

ipc::IPCResult
TestOffMainThreadPaintingParent::RecvAsyncMessage(const uint64_t& aTxnId)
{
  if (!mCompletedTxn || mCompletedTxn.value() != aTxnId) {
    fail("sync message received out of order");
    return IPC_FAIL_NO_REASON(this);
  }
  mAsyncMessages++;
  return IPC_OK();
}

ipc::IPCResult
TestOffMainThreadPaintingParent::RecvSyncMessage(const uint64_t& aTxnId)
{
  if (!mCompletedTxn || mCompletedTxn.value() != aTxnId) {
    fail("sync message received out of order");
    return IPC_FAIL_NO_REASON(this);
  }
  if (mSyncMessages >= mAsyncMessages) {
    fail("sync message received before async message");
    return IPC_FAIL_NO_REASON(this);
  }
  mSyncMessages++;
  return IPC_OK();
}

ipc::IPCResult
TestOffMainThreadPaintingParent::RecvEndTest()
{
  if (!mCompletedTxn || mCompletedTxn.value() != 1) {
    fail("expected to complete a transaction");
  }
  if (mAsyncMessages != 1) {
    fail("expected to get 1 async message");
  }
  if (mSyncMessages != 1) {
    fail("expected to get 1 sync message");
  }

  passed("ok");

  mPaintActor->Close();
  Close();
  return IPC_OK();
}

void
TestOffMainThreadPaintingParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy != NormalShutdown) {
    fail("child process aborted");
  }
  QuitParent();
}

/**************************
 * PTestLayoutThreadChild *
 **************************/

TestOffMainThreadPaintingChild::TestOffMainThreadPaintingChild()
 : mNextTxnId(1)
{
}

TestOffMainThreadPaintingChild::~TestOffMainThreadPaintingChild()
{
}

ipc::IPCResult 
TestOffMainThreadPaintingChild::RecvStartTest(ipc::Endpoint<PTestPaintThreadChild>&& aEndpoint)
{
  mPaintThread = MakeUnique<base::Thread>("PaintThread");
  if (!mPaintThread->Start()) {
    return IPC_FAIL_NO_REASON(this);
  }

  mPaintActor = new TestPaintThreadChild();
  RefPtr<Runnable> task = NewRunnableMethod<ipc::Endpoint<PTestPaintThreadChild>&&>(
    "TestPaintthreadChild::Bind", mPaintActor, &TestPaintThreadChild::Bind, Move(aEndpoint));
  mPaintThread->message_loop()->PostTask(task.forget());

  IssueTransaction();
  return IPC_OK();
}

void
TestOffMainThreadPaintingChild::ActorDestroy(ActorDestroyReason aWhy)
{
  RefPtr<Runnable> task = NewRunnableMethod(
    "TestPaintThreadChild::Close", mPaintActor, &TestPaintThreadChild::Close);
  mPaintThread->message_loop()->PostTask(task.forget());
  mPaintThread = nullptr;

  QuitChild();
}

void
TestOffMainThreadPaintingChild::ProcessingError(Result aCode, const char* aReason)
{
  MOZ_CRASH("Aborting child due to IPC error");
}

void
TestOffMainThreadPaintingChild::IssueTransaction()
{
  uint64_t txnId = mNextTxnId++;

  // Start painting before we send the message.
  RefPtr<Runnable> task = NewRunnableMethod<uint64_t>(
    "TestPaintThreadChild::BeginPaintingForTxn", mPaintActor, &TestPaintThreadChild::BeginPaintingForTxn, txnId);
  mPaintThread->message_loop()->PostTask(task.forget());

  // Simulate some gecko main thread stuff.
  SendFinishedLayout(txnId);
  SendAsyncMessage(txnId);
  SendSyncMessage(txnId);
  SendEndTest();
}

/**************************
 * PTestPaintThreadParent *
 **************************/

TestPaintThreadParent::TestPaintThreadParent(TestOffMainThreadPaintingParent* aMainBridge)
 : mMainBridge(aMainBridge)
{
}

TestPaintThreadParent::~TestPaintThreadParent()
{
}

bool
TestPaintThreadParent::Bind(ipc::Endpoint<PTestPaintThreadParent>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    return false;
  }

  AddRef();
  return true;
}

ipc::IPCResult
TestPaintThreadParent::RecvFinishedPaint(const uint64_t& aTxnId)
{
  mMainBridge->NotifyFinishedPaint(aTxnId);
  return IPC_OK();
}

void
TestPaintThreadParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

void
TestPaintThreadParent::DeallocPTestPaintThreadParent()
{
  Release();
}

/************************* 
 * PTestPaintThreadChild *
 *************************/

TestPaintThreadChild::TestPaintThreadChild()
 : mCanSend(false)
{
}

TestPaintThreadChild::~TestPaintThreadChild()
{
}

void
TestPaintThreadChild::Bind(ipc::Endpoint<PTestPaintThreadChild>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    MOZ_CRASH("could not bind paint child endpoint");
  }
  AddRef();
  mCanSend = true;
}

void
TestPaintThreadChild::BeginPaintingForTxn(uint64_t aTxnId)
{
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  // Sleep for some time to simulate painting being slow.
  PR_Sleep(PR_MillisecondsToInterval(500));
  SendFinishedPaint(aTxnId);
}

void
TestPaintThreadChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mCanSend = false;
}

void
TestPaintThreadChild::Close()
{
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  if (mCanSend) {
    PTestPaintThreadChild::Close();
  }
}

void
TestPaintThreadChild::DeallocPTestPaintThreadChild()
{
  Release();
}

} // namespace _ipdltest
} // namespace mozilla
