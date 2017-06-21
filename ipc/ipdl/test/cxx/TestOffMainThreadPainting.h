#ifndef mozilla__ipdltest_TestOffMainThreadPainting_h
#define mozilla__ipdltest_TestOffMainThreadPainting_h

#include "mozilla/Maybe.h"
#include "mozilla/_ipdltest/IPDLUnitTests.h"
#include "mozilla/_ipdltest/PTestLayoutThreadChild.h"
#include "mozilla/_ipdltest/PTestLayoutThreadParent.h"
#include "mozilla/_ipdltest/PTestPaintThreadChild.h"
#include "mozilla/_ipdltest/PTestPaintThreadParent.h"
#include "base/thread.h"

namespace mozilla {
namespace _ipdltest {

class TestPaintThreadChild;
class TestPaintThreadParent;

// Analagous to LayerTransactionParent.
class TestOffMainThreadPaintingParent final : public PTestLayoutThreadParent
{
public:
  static bool RunTestInThreads() { return false; }
  static bool RunTestInProcesses() { return true; }

  void Main();

  MOZ_IMPLICIT TestOffMainThreadPaintingParent();
  ~TestOffMainThreadPaintingParent() override;

  ipc::IPCResult RecvFinishedLayout(const uint64_t& aTxnId) override;
  ipc::IPCResult RecvAsyncMessage(const uint64_t& aTxnId) override;
  ipc::IPCResult RecvSyncMessage(const uint64_t& aTxnId) override;
  ipc::IPCResult RecvEndTest() override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

  void NotifyFinishedPaint(const uint64_t& aTxnId);

private:
  RefPtr<TestPaintThreadParent> mPaintActor;
  Maybe<uint64_t> mCompletedTxn;
  Maybe<uint64_t> mPaintedTxn;
  uint32_t mAsyncMessages;
  uint32_t mSyncMessages;
};

// Analagous to LayerTransactionChild.
class TestOffMainThreadPaintingChild final : public PTestLayoutThreadChild
{
public:
  TestOffMainThreadPaintingChild();
  ~TestOffMainThreadPaintingChild() override;

  ipc::IPCResult RecvStartTest(ipc::Endpoint<PTestPaintThreadChild>&& aEndpoint) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

private:
  void IssueTransaction();

private:
  UniquePtr<base::Thread> mPaintThread;
  RefPtr<TestPaintThreadChild> mPaintActor;
  uint64_t mNextTxnId;
};

/****************
 * Paint Actors *
 ****************/

class TestPaintThreadParent final : public PTestPaintThreadParent
{
public:
  explicit TestPaintThreadParent(TestOffMainThreadPaintingParent* aMainBridge);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestPaintThreadParent);

  bool Bind(ipc::Endpoint<PTestPaintThreadParent>&& aEndpoint);
  ipc::IPCResult RecvFinishedPaint(const uint64_t& aTxnId) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPTestPaintThreadParent() override;

private:
  ~TestPaintThreadParent() override;

private:
  TestOffMainThreadPaintingParent* mMainBridge;
};

class TestPaintThreadChild final : public PTestPaintThreadChild
{
public:
  explicit TestPaintThreadChild(MessageChannel* aOtherChannel);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestPaintThreadChild);

  void Bind(ipc::Endpoint<PTestPaintThreadChild>&& aEndpoint);
  void BeginPaintingForTxn(uint64_t aTxnId);
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPTestPaintThreadChild() override;
  void Close();

private:
  ~TestPaintThreadChild() override;

  bool mCanSend;
  MessageChannel* mMainChannel;
};

} // namespace _ipdltest
} // namespace mozilla

#endif // ifndef mozilla__ipdltest_TestOffMainThreadPainting_h
