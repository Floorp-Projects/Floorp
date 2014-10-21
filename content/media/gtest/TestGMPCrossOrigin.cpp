/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"
#include "GMPDecryptorProxy.h"
#include "GMPService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Atomics.h"
#include "nsNSSComponent.h"

using namespace std;

using namespace mozilla;
using namespace mozilla::gmp;

struct GMPTestRunner
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPTestRunner)

  void DoTest(void (GMPTestRunner::*aTestMethod)());
  void RunTestGMPTestCodec();
  void RunTestGMPCrossOrigin();

private:
  ~GMPTestRunner() { }
};

void
GMPTestRunner::RunTestGMPTestCodec()
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();

  GMPVideoHost* host = nullptr;
  GMPVideoDecoderProxy* decoder = nullptr;
  GMPVideoDecoderProxy* decoder2 = nullptr;
  GMPVideoEncoderProxy* encoder = nullptr;

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));

  service->GetGMPVideoDecoder(&tags, NS_LITERAL_CSTRING("o"), &host, &decoder2);
  service->GetGMPVideoDecoder(&tags, NS_LITERAL_CSTRING(""), &host, &decoder);

  service->GetGMPVideoEncoder(&tags, NS_LITERAL_CSTRING(""), &host, &encoder);

  EXPECT_TRUE(host);
  EXPECT_TRUE(decoder);
  EXPECT_TRUE(decoder2);
  EXPECT_TRUE(encoder);

  if (decoder) decoder->Close();
  if (decoder2) decoder2->Close();
  if (encoder) encoder->Close();
}

void
GMPTestRunner::RunTestGMPCrossOrigin()
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();

  GMPVideoHost* host = nullptr;
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));

  GMPVideoDecoderProxy* decoder1 = nullptr;
  GMPVideoDecoderProxy* decoder2 = nullptr;
  GMPVideoEncoderProxy* encoder1 = nullptr;
  GMPVideoEncoderProxy* encoder2 = nullptr;

  service->GetGMPVideoDecoder(&tags, NS_LITERAL_CSTRING("origin1"), &host, &decoder1);
  service->GetGMPVideoDecoder(&tags, NS_LITERAL_CSTRING("origin2"), &host, &decoder2);
  EXPECT_TRUE(!!decoder1 && !!decoder2 &&
              decoder1->ParentID() != decoder2->ParentID());

  service->GetGMPVideoEncoder(&tags, NS_LITERAL_CSTRING("origin1"), &host, &encoder1);
  service->GetGMPVideoEncoder(&tags, NS_LITERAL_CSTRING("origin2"), &host, &encoder2);
  EXPECT_TRUE(!!encoder1 && !!encoder2 &&
              encoder1->ParentID() != encoder2->ParentID());

  if (decoder2) decoder2->Close();
  if (encoder2) encoder2->Close();

  service->GetGMPVideoDecoder(&tags, NS_LITERAL_CSTRING("origin1"), &host, &decoder2);
  EXPECT_TRUE(!!decoder1 && !!decoder2 &&
              decoder1->ParentID() == decoder2->ParentID());

  service->GetGMPVideoEncoder(&tags, NS_LITERAL_CSTRING("origin1"), &host, &encoder2);
  EXPECT_TRUE(!!encoder1 && !!encoder2 &&
              encoder1->ParentID() == encoder2->ParentID());

  if (decoder1) decoder1->Close();
  if (decoder2) decoder2->Close();
  if (encoder1) encoder1->Close();
  if (encoder2) encoder2->Close();
}

class NotifyObserversTask : public nsRunnable {
public:
  NotifyObserversTask(const char* aTopic)
    : mTopic(aTopic)
  {}
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(nullptr, mTopic, nullptr);
    }
    return NS_OK;
  }
  const char* mTopic;
};

class ClearGMPStorageTask : public nsIRunnable
                          , public nsIObserver {
public:
  ClearGMPStorageTask(nsIRunnable* Continuation,
                      nsIThread* aTarget)
    : mContinuation(Continuation)
    , mTarget(aTarget)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Run() MOZ_OVERRIDE {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    EXPECT_TRUE(observerService);
    observerService->AddObserver(this, "gmp-clear-storage-complete", false);
    if (observerService) {
      observerService->NotifyObservers(nullptr, "gmp-clear-storage", nullptr);
    }
    return NS_OK;
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aSomeData) MOZ_OVERRIDE
  {
    if (!strcmp(aTopic, "gmp-clear-storage-complete")) {
      nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
      EXPECT_TRUE(observerService);
      observerService->RemoveObserver(this, "gmp-clear-storage-complete");
      mTarget->Dispatch(mContinuation, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
  }

private:
  virtual ~ClearGMPStorageTask() {}
  nsRefPtr<nsIRunnable> mContinuation;
  nsCOMPtr<nsIThread> mTarget;
};

NS_IMPL_ISUPPORTS(ClearGMPStorageTask, nsIRunnable, nsIObserver)

static void
ClearGMPStorage(nsIRunnable* aContinuation,
                nsIThread* aTarget)
{
  nsRefPtr<ClearGMPStorageTask> task(new ClearGMPStorageTask(aContinuation, aTarget));
  NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
}

static void
SimulatePBModeExit()
{
  NS_DispatchToMainThread(new NotifyObserversTask("last-pb-context-exited"), NS_DISPATCH_SYNC);
}

static nsCString
GetNodeId(const nsAString& aOrigin,
          const nsAString& aTopLevelOrigin,
          bool aInPBMode)
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  EXPECT_TRUE(service);
  nsCString nodeId;
  nsresult rv = service->GetNodeId(aOrigin,
                                   aTopLevelOrigin,
                                   aInPBMode,
                                   nodeId);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  return nodeId;
}

static bool
IsGMPStorageIsEmpty()
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  MOZ_ASSERT(service);
  nsCOMPtr<nsIFile> storage;
  nsresult rv = service->GetStorageDir(getter_AddRefs(storage));
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  bool exists = false;
  if (storage) {
    storage->Exists(&exists);
  }
  return !exists;
}

static void
AssertIsOnGMPThread()
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  MOZ_ASSERT(service);
  nsCOMPtr<nsIThread> thread;
  service->GetThread(getter_AddRefs(thread));
  MOZ_ASSERT(thread);
  nsCOMPtr<nsIThread> currentThread;
  DebugOnly<nsresult> rv = NS_GetCurrentThread(getter_AddRefs(currentThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(currentThread == thread);
}

static already_AddRefed<nsIThread>
GetGMPThread()
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  nsCOMPtr<nsIThread> thread;
  EXPECT_TRUE(NS_SUCCEEDED(service->GetThread(getter_AddRefs(thread))));
  return thread.forget();
}
class GMPStorageTest : public GMPDecryptorProxyCallback
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPStorageTest)

  void DoTest(void (GMPStorageTest::*aTestMethod)()) {
    EnsureNSSInitializedChromeOrContent();
    nsCOMPtr<nsIThread> thread(GetGMPThread());
    ClearGMPStorage(NS_NewRunnableMethod(this, aTestMethod), thread);
    AwaitFinished();
  }

  GMPStorageTest()
    : mDecryptor(nullptr)
    , mMonitor("GMPStorageTest")
    , mFinished(false)
  {
  }

  void
  Update(const nsCString& aMessage)
  {
    nsTArray<uint8_t> msg;
    msg.AppendElements(aMessage.get(), aMessage.Length());
    mDecryptor->UpdateSession(1, NS_LITERAL_CSTRING("fake-session-id"), msg);
  }

  void TestGetNodeId()
  {
    AssertIsOnGMPThread();

    EXPECT_TRUE(IsGMPStorageIsEmpty());

    const nsString origin1 = NS_LITERAL_STRING("example1.com");
    const nsString origin2 = NS_LITERAL_STRING("example2.org");

    nsCString PBnodeId1 = GetNodeId(origin1, origin2, true);
    nsCString PBnodeId2 = GetNodeId(origin1, origin2, true);

    // Node ids for the same origins should be the same in PB mode.
    EXPECT_TRUE(PBnodeId1.Equals(PBnodeId2));

    nsCString PBnodeId3 = GetNodeId(origin2, origin1, true);

    // Node ids with origin and top level origin swapped should be different.
    EXPECT_TRUE(!PBnodeId3.Equals(PBnodeId1));

    // Getting node ids in PB mode should not result in the node id being stored.
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    nsCString nodeId1 = GetNodeId(origin1, origin2, false);
    nsCString nodeId2 = GetNodeId(origin1, origin2, false);

    // NodeIds for the same origin pair in non-pb mode should be the same.
    EXPECT_TRUE(nodeId1.Equals(nodeId2));

    // Node ids for a given origin pair should be different for the PB origins should be the same in PB mode.
    EXPECT_TRUE(!PBnodeId1.Equals(nodeId1));
    EXPECT_TRUE(!PBnodeId2.Equals(nodeId2));

    nsCOMPtr<nsIThread> thread(GetGMPThread());
    ClearGMPStorage(NS_NewRunnableMethodWithArg<nsCString>(
      this, &GMPStorageTest::TestGetNodeId_Continuation, nodeId1), thread);
  }

  void TestGetNodeId_Continuation(nsCString aNodeId1) {
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Once we clear storage, the node ids generated for the same origin-pair
    // should be different.
    const nsString origin1 = NS_LITERAL_STRING("example1.com");
    const nsString origin2 = NS_LITERAL_STRING("example2.org");
    nsCString nodeId3 = GetNodeId(origin1, origin2, false);
    EXPECT_TRUE(!aNodeId1.Equals(nodeId3));

    SetFinished();
  }

  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode) {
    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    EXPECT_TRUE(service);

    const nsCString nodeId = GetNodeId(aOrigin,
                                       aTopLevelOrigin,
                                       aInPBMode);
    EXPECT_TRUE(!nodeId.IsEmpty());

    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    nsresult rv = service->GetGMPDecryptor(&tags, nodeId, &mDecryptor);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    EXPECT_TRUE(!!mDecryptor);

    if (mDecryptor) {
      mDecryptor->Init(this);
    }
  }

  void TestBasicStorage() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();

    CreateDecryptor(NS_LITERAL_STRING("example1.com"),
                    NS_LITERAL_STRING("example2.com"),
                    false);

    // Send a message to the fake GMP for it to run its own tests internally.
    // It sends us a "test-storage complete" message when its passed, or
    // some other message if its tests fail.
    Expect(NS_LITERAL_CSTRING("test-storage complete"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("test-storage"));
  }

  void TestCrossOriginStorage() {
    EXPECT_TRUE(!mDecryptor);

    // Open decryptor on one, origin, write a record, and test that that
    // record can't be read on another origin.
    CreateDecryptor(NS_LITERAL_STRING("example3.com"),
                    NS_LITERAL_STRING("example4.com"),
                    false);

    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    auto t = time(0);
    nsCString response("stored crossOriginTestRecordId ");
    response.AppendInt((int64_t)t);
    Expect(response, NS_NewRunnableMethod(this,
      &GMPStorageTest::TestCrossOriginStorage_RecordStoredContinuation));

    nsCString update("store crossOriginTestRecordId ");
    update.AppendInt((int64_t)t);
    Update(update);
  }

  void TestCrossOriginStorage_RecordStoredContinuation() {
    // Close the old decryptor, and create a new one on a different origin,
    // and try to read the record.
    Shutdown();

    CreateDecryptor(NS_LITERAL_STRING("example5.com"),
                    NS_LITERAL_STRING("example6.com"),
                    false);

    Expect(NS_LITERAL_CSTRING("retrieve crossOriginTestRecordId succeeded (length 0 bytes)"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("retrieve crossOriginTestRecordId"));
  }

  void TestPBStorage() {

    // Open decryptor on one, origin, write a record, close decryptor,
    // open another, and test that record can be read, close decryptor,
    // then send pb-last-context-closed notification, then open decryptor
    // and check that it can't read that data; it should have been purged.
    CreateDecryptor(NS_LITERAL_STRING("pb1.com"),
                    NS_LITERAL_STRING("pb2.com"),
                    true);

    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    nsCString response("stored pbdata test-pb-data");
    Expect(response, NS_NewRunnableMethod(this,
      &GMPStorageTest::TestPBStorage_RecordStoredContinuation));

    nsCString update("store pbdata test-pb-data");
    Update(update);
  }

  void TestPBStorage_RecordStoredContinuation() {
    Shutdown();

    CreateDecryptor(NS_LITERAL_STRING("pb1.com"),
                    NS_LITERAL_STRING("pb2.com"),
                    true);

    Expect(NS_LITERAL_CSTRING("retrieve pbdata succeeded (length 12 bytes)"),
           NS_NewRunnableMethod(this,
              &GMPStorageTest::TestPBStorage_RecordRetrievedContinuation));
    Update(NS_LITERAL_CSTRING("retrieve pbdata"));
  }

  void TestPBStorage_RecordRetrievedContinuation() {
    Shutdown();
    SimulatePBModeExit();

    CreateDecryptor(NS_LITERAL_STRING("pb1.com"),
                    NS_LITERAL_STRING("pb2.com"),
                    true);

    Expect(NS_LITERAL_CSTRING("retrieve pbdata succeeded (length 0 bytes)"),
           NS_NewRunnableMethod(this,
              &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("retrieve pbdata"));

  }

  void Expect(const nsCString& aMessage, nsIRunnable* aContinuation) {
    mExpected.AppendElement(ExpectedMessage(aMessage, aContinuation));
  }

  void AwaitFinished() {
    while (!mFinished) {
      NS_ProcessNextEvent(nullptr, true);
    }
    mFinished = false;
  }

  void Shutdown() {
    if (mDecryptor) {
      mDecryptor->Close();
      mDecryptor = nullptr;
    }
  }

  void Dummy() {
  }

  void SetFinished() {
    mFinished = true;
    Shutdown();
    NS_DispatchToMainThread(NS_NewRunnableMethod(this, &GMPStorageTest::Dummy));
  }

  virtual void SessionMessage(const nsCString& aSessionId,
                              const nsTArray<uint8_t>& aMessage,
                              const nsCString& aDestinationURL) MOZ_OVERRIDE
  {
    MonitorAutoLock mon(mMonitor);

    nsCString msg((const char*)aMessage.Elements(), aMessage.Length());
    EXPECT_TRUE(mExpected.Length() > 0);
    bool matches = mExpected[0].mMessage.Equals(msg);
    EXPECT_TRUE(matches);
    if (mExpected.Length() > 0 && matches) {
      nsRefPtr<nsIRunnable> continuation = mExpected[0].mContinuation;
      mExpected.RemoveElementAt(0);
      if (continuation) {
        NS_DispatchToCurrentThread(continuation);
      }
    }
  }

  virtual void ResolveNewSessionPromise(uint32_t aPromiseId,
                                        const nsCString& aSessionId) MOZ_OVERRIDE { }
  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) MOZ_OVERRIDE {}
  virtual void ResolvePromise(uint32_t aPromiseId) MOZ_OVERRIDE {}
  virtual void RejectPromise(uint32_t aPromiseId,
                             nsresult aException,
                             const nsCString& aSessionId) MOZ_OVERRIDE { }
  virtual void ExpirationChange(const nsCString& aSessionId,
                                GMPTimestamp aExpiryTime) MOZ_OVERRIDE {}
  virtual void SessionClosed(const nsCString& aSessionId) MOZ_OVERRIDE {}
  virtual void SessionError(const nsCString& aSessionId,
                            nsresult aException,
                            uint32_t aSystemCode,
                            const nsCString& aMessage) MOZ_OVERRIDE {}
  virtual void KeyIdUsable(const nsCString& aSessionId,
                           const nsTArray<uint8_t>& aKeyId) MOZ_OVERRIDE { }
  virtual void KeyIdNotUsable(const nsCString& aSessionId,
                              const nsTArray<uint8_t>& aKeyId) MOZ_OVERRIDE {}
  virtual void SetCaps(uint64_t aCaps) MOZ_OVERRIDE {}
  virtual void Decrypted(uint32_t aId,
                         GMPErr aResult,
                         const nsTArray<uint8_t>& aDecryptedData) MOZ_OVERRIDE { }
  virtual void Terminated() MOZ_OVERRIDE { }

private:
  ~GMPStorageTest() { }

  struct ExpectedMessage {
    ExpectedMessage(const nsCString& aMessage, nsIRunnable* aContinuation)
      : mMessage(aMessage)
      , mContinuation(aContinuation)
    {}
    nsCString mMessage;
    nsRefPtr<nsIRunnable> mContinuation;
  };

  nsTArray<ExpectedMessage> mExpected;

  GMPDecryptorProxy* mDecryptor;
  Monitor mMonitor;
  Atomic<bool> mFinished;
};

void
GMPTestRunner::DoTest(void (GMPTestRunner::*aTestMethod)())
{
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  nsCOMPtr<nsIThread> thread;

  service->GetThread(getter_AddRefs(thread));
  thread->Dispatch(NS_NewRunnableMethod(this, aTestMethod), NS_DISPATCH_SYNC);
}

TEST(GeckoMediaPlugins, GMPTestCodec) {
  nsRefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec);
}

TEST(GeckoMediaPlugins, GMPCrossOrigin) {
  nsRefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin);
}

TEST(GeckoMediaPlugins, GMPStorageGetNodeId) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestGetNodeId);
}

TEST(GeckoMediaPlugins, GMPStorageBasic) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestBasicStorage);
}

TEST(GeckoMediaPlugins, GMPStorageCrossOrigin) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestCrossOriginStorage);
}

TEST(GeckoMediaPlugins, GMPStoragePrivateBrowsing) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestPBStorage);
}
