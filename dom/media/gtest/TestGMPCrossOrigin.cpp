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
#include "GMPServiceParent.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Atomics.h"
#include "nsNSSComponent.h"
#include "mozilla/DebugOnly.h"

#if defined(XP_WIN)
#include "mozilla/WindowsVersion.h"
#endif

using namespace std;

using namespace mozilla;
using namespace mozilla::gmp;

class GMPTestMonitor
{
public:
  GMPTestMonitor()
    : mFinished(false)
  {
  }

  void AwaitFinished()
  {
    MOZ_ASSERT(NS_IsMainThread());
    while (!mFinished) {
      NS_ProcessNextEvent(nullptr, true);
    }
    mFinished = false;
  }

private:
  void MarkFinished()
  {
    mFinished = true;
  }

public:
  void SetFinished()
  {
    NS_DispatchToMainThread(NS_NewNonOwningRunnableMethod(this,
                                                          &GMPTestMonitor::MarkFinished));
  }

private:
  bool mFinished;
};

struct GMPTestRunner
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPTestRunner)

  void DoTest(void (GMPTestRunner::*aTestMethod)(GMPTestMonitor&));
  void RunTestGMPTestCodec1(GMPTestMonitor& aMonitor);
  void RunTestGMPTestCodec2(GMPTestMonitor& aMonitor);
  void RunTestGMPTestCodec3(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin1(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin2(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin3(GMPTestMonitor& aMonitor);
  void RunTestGMPCrossOrigin4(GMPTestMonitor& aMonitor);

private:
  ~GMPTestRunner() { }
};

template<class T, class Base,
         nsresult (NS_STDCALL GeckoMediaPluginService::*Getter)(nsTArray<nsCString>*,
                                                                const nsACString&,
                                                                UniquePtr<Base>&&)>
class RunTestGMPVideoCodec : public Base
{
public:
  virtual void Done(T* aGMP, GMPVideoHost* aHost)
  {
    EXPECT_TRUE(aGMP);
    EXPECT_TRUE(aHost);
    if (aGMP) {
      aGMP->Close();
    }
    mMonitor.SetFinished();
  }

  static void Run(GMPTestMonitor& aMonitor, const nsCString& aOrigin)
  {
    UniquePtr<GMPCallbackType> callback(new RunTestGMPVideoCodec(aMonitor));
    Get(aOrigin, Move(callback));
  }

protected:
  typedef T GMPCodecType;
  typedef Base GMPCallbackType;

  explicit RunTestGMPVideoCodec(GMPTestMonitor& aMonitor)
    : mMonitor(aMonitor)
  {
  }

  static nsresult Get(const nsACString& aNodeId, UniquePtr<Base>&& aCallback)
  {
    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("h264"));

    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    return ((*service).*Getter)(&tags, aNodeId, Move(aCallback));
  }

protected:
  GMPTestMonitor& mMonitor;
};

typedef RunTestGMPVideoCodec<GMPVideoDecoderProxy,
                             GetGMPVideoDecoderCallback,
                             &GeckoMediaPluginService::GetGMPVideoDecoder>
  RunTestGMPVideoDecoder;
typedef RunTestGMPVideoCodec<GMPVideoEncoderProxy,
                             GetGMPVideoEncoderCallback,
                             &GeckoMediaPluginService::GetGMPVideoEncoder>
  RunTestGMPVideoEncoder;

void
GMPTestRunner::RunTestGMPTestCodec1(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoDecoder::Run(aMonitor, NS_LITERAL_CSTRING("o"));
}

void
GMPTestRunner::RunTestGMPTestCodec2(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoDecoder::Run(aMonitor, NS_LITERAL_CSTRING(""));
}

void
GMPTestRunner::RunTestGMPTestCodec3(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoEncoder::Run(aMonitor, NS_LITERAL_CSTRING(""));
}

template<class Base>
class RunTestGMPCrossOrigin : public Base
{
public:
  virtual void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost)
  {
    EXPECT_TRUE(aGMP);

    UniquePtr<typename Base::GMPCallbackType> callback(
      new Step2(Base::mMonitor, aGMP, mShouldBeEqual));
    nsresult rv = Base::Get(mOrigin2, Move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      Base::mMonitor.SetFinished();
    }
  }

  static void Run(GMPTestMonitor& aMonitor, const nsCString& aOrigin1,
                  const nsCString& aOrigin2)
  {
    UniquePtr<typename Base::GMPCallbackType> callback(
      new RunTestGMPCrossOrigin<Base>(aMonitor, aOrigin1, aOrigin2));
    nsresult rv = Base::Get(aOrigin1, Move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      aMonitor.SetFinished();
    }
  }

private:
  RunTestGMPCrossOrigin(GMPTestMonitor& aMonitor, const nsCString& aOrigin1,
                        const nsCString& aOrigin2)
    : Base(aMonitor),
      mGMP(nullptr),
      mOrigin2(aOrigin2),
      mShouldBeEqual(aOrigin1.Equals(aOrigin2))
  {
  }

  class Step2 : public Base
  {
  public:
    Step2(GMPTestMonitor& aMonitor,
          typename Base::GMPCodecType* aGMP,
          bool aShouldBeEqual)
      : Base(aMonitor),
        mGMP(aGMP),
        mShouldBeEqual(aShouldBeEqual)
    {
    }
    virtual void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost)
    {
      EXPECT_TRUE(aGMP);
      if (aGMP) {
        EXPECT_TRUE(mGMP &&
                    (mGMP->ParentID() == aGMP->ParentID()) == mShouldBeEqual);
      }
      if (mGMP) {
        mGMP->Close();
      }
      Base::Done(aGMP, aHost);
    }

  private:
    typename Base::GMPCodecType* mGMP;
    bool mShouldBeEqual;
  };

  typename Base::GMPCodecType* mGMP;
  nsCString mOrigin2;
  bool mShouldBeEqual;
};

typedef RunTestGMPCrossOrigin<RunTestGMPVideoDecoder>
  RunTestGMPVideoDecoderCrossOrigin;
typedef RunTestGMPCrossOrigin<RunTestGMPVideoEncoder>
  RunTestGMPVideoEncoderCrossOrigin;

void
GMPTestRunner::RunTestGMPCrossOrigin1(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoDecoderCrossOrigin::Run(
    aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin2"));
}

void
GMPTestRunner::RunTestGMPCrossOrigin2(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoEncoderCrossOrigin::Run(
    aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin2"));
}

void
GMPTestRunner::RunTestGMPCrossOrigin3(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoDecoderCrossOrigin::Run(
    aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin1"));
}

void
GMPTestRunner::RunTestGMPCrossOrigin4(GMPTestMonitor& aMonitor)
{
  RunTestGMPVideoEncoderCrossOrigin::Run(
    aMonitor, NS_LITERAL_CSTRING("origin1"), NS_LITERAL_CSTRING("origin1"));
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

/**
 * Enumerate files under |aPath| (non-recursive).
 */
template<typename T>
static nsresult
EnumerateDir(nsIFile* aPath, T&& aDirIter)
{
  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = aPath->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool hasMore = false;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsIFile> entry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) {
      continue;
    }

    aDirIter(entry);
  }
  return NS_OK;
}

/**
 * Enumerate files under $profileDir/gmp/$aDir/ (non-recursive).
 */
template<typename T>
static nsresult
EnumerateGMPStorageDir(const nsACString& aDir, T&& aDirIter)
{
  nsRefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  MOZ_ASSERT(service);

  // $profileDir/gmp/
  nsCOMPtr<nsIFile> path;
  nsresult rv = service->GetStorageDir(getter_AddRefs(path));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // $profileDir/gmp/$aDir/
  rv = path->AppendNative(aDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return EnumerateDir(path, aDirIter);
}

class GMPShutdownObserver : public nsIRunnable
                          , public nsIObserver {
public:
  GMPShutdownObserver(nsIRunnable* aShutdownTask,
                      nsIRunnable* Continuation,
                      const nsACString& aNodeId)
    : mShutdownTask(aShutdownTask)
    , mContinuation(Continuation)
    , mNodeId(NS_ConvertUTF8toUTF16(aNodeId))
  {}

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    EXPECT_TRUE(observerService);
    observerService->AddObserver(this, "gmp-shutdown", false);

    nsCOMPtr<nsIThread> thread(GetGMPThread());
    thread->Dispatch(mShutdownTask, NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aSomeData) override
  {
    if (!strcmp(aTopic, "gmp-shutdown") &&
        mNodeId.Equals(nsDependentString(aSomeData))) {
      nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
      EXPECT_TRUE(observerService);
      observerService->RemoveObserver(this, "gmp-shutdown");
      nsCOMPtr<nsIThread> thread(GetGMPThread());
      thread->Dispatch(mContinuation, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
  }

private:
  virtual ~GMPShutdownObserver() {}
  nsCOMPtr<nsIRunnable> mShutdownTask;
  nsCOMPtr<nsIRunnable> mContinuation;
  const nsString mNodeId;
};

NS_IMPL_ISUPPORTS(GMPShutdownObserver, nsIRunnable, nsIObserver)

class NotifyObserversTask : public nsRunnable {
public:
  explicit NotifyObserversTask(const char* aTopic)
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
                      nsIThread* aTarget, PRTime aSince)
    : mContinuation(Continuation)
    , mTarget(aTarget)
    , mSince(aSince)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    EXPECT_TRUE(observerService);
    observerService->AddObserver(this, "gmp-clear-storage-complete", false);
    if (observerService) {
      nsAutoString str;
      if (mSince >= 0) {
        str.AppendInt(static_cast<int64_t>(mSince));
      }
      observerService->NotifyObservers(
          nullptr, "browser:purge-session-history", str.Data());
    }
    return NS_OK;
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aSomeData) override
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
  nsCOMPtr<nsIRunnable> mContinuation;
  nsCOMPtr<nsIThread> mTarget;
  const PRTime mSince;
};

NS_IMPL_ISUPPORTS(ClearGMPStorageTask, nsIRunnable, nsIObserver)

static void
ClearGMPStorage(nsIRunnable* aContinuation,
                nsIThread* aTarget, PRTime aSince = -1)
{
  nsRefPtr<ClearGMPStorageTask> task(
      new ClearGMPStorageTask(aContinuation, aTarget, aSince));
  NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
}

static void
SimulatePBModeExit()
{
  NS_DispatchToMainThread(new NotifyObserversTask("last-pb-context-exited"), NS_DISPATCH_SYNC);
}

class TestGetNodeIdCallback : public GetNodeIdCallback
{
public:
  TestGetNodeIdCallback(nsCString& aNodeId, nsresult& aResult)
    : mNodeId(aNodeId),
      mResult(aResult)
  {
  }

  void Done(nsresult aResult, const nsACString& aNodeId)
  {
    mResult = aResult;
    mNodeId = aNodeId;
  }

private:
  nsCString& mNodeId;
  nsresult& mResult;
};

static nsCString
GetNodeId(const nsAString& aOrigin,
          const nsAString& aTopLevelOrigin,
          bool aInPBMode)
{
  nsRefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  EXPECT_TRUE(service);
  nsCString nodeId;
  nsresult result;
  UniquePtr<GetNodeIdCallback> callback(new TestGetNodeIdCallback(nodeId,
                                                                  result));
  // We rely on the fact that the GetNodeId implementation for
  // GeckoMediaPluginServiceParent is synchronous.
  nsresult rv = service->GetNodeId(aOrigin,
                                   aTopLevelOrigin,
                                   aInPBMode,
                                   NS_LITERAL_CSTRING(""),
                                   Move(callback));
  EXPECT_TRUE(NS_SUCCEEDED(rv) && NS_SUCCEEDED(result));
  return nodeId;
}

static bool
IsGMPStorageIsEmpty()
{
  nsRefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
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

    const nsString origin1 = NS_LITERAL_STRING("http://example1.com");
    const nsString origin2 = NS_LITERAL_STRING("http://example2.org");

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
    const nsString origin1 = NS_LITERAL_STRING("http://example1.com");
    const nsString origin2 = NS_LITERAL_STRING("http://example2.org");
    nsCString nodeId3 = GetNodeId(origin1, origin2, false);
    EXPECT_TRUE(!aNodeId1.Equals(nodeId3));

    SetFinished();
  }

  class CreateDecryptorDone : public GetGMPDecryptorCallback
  {
  public:
    CreateDecryptorDone(GMPStorageTest* aRunner, nsIRunnable* aContinuation)
      : mRunner(aRunner),
        mContinuation(aContinuation)
    {
    }

    virtual void Done(GMPDecryptorProxy* aDecryptor) override
    {
      mRunner->mDecryptor = aDecryptor;
      EXPECT_TRUE(!!mRunner->mDecryptor);

      if (mRunner->mDecryptor) {
        mRunner->mDecryptor->Init(mRunner);
      }
      nsCOMPtr<nsIThread> thread(GetGMPThread());
      thread->Dispatch(mContinuation, NS_DISPATCH_NORMAL);
    }

  private:
    nsRefPtr<GMPStorageTest> mRunner;
    nsCOMPtr<nsIRunnable> mContinuation;
  };

  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       const nsCString& aUpdate)
  {
    nsTArray<nsCString> updates;
    updates.AppendElement(aUpdate);
    CreateDecryptor(aOrigin, aTopLevelOrigin, aInPBMode, Move(updates));
  }
  class Updates : public nsRunnable
  {
  public:
    Updates(GMPStorageTest* aRunner, nsTArray<nsCString>&& aUpdates)
      : mRunner(aRunner),
        mUpdates(aUpdates)
    {
    }

    NS_IMETHOD Run()
    {
      for (auto& update : mUpdates) {
        mRunner->Update(update);
      }
      return NS_OK;
    }

  private:
    nsRefPtr<GMPStorageTest> mRunner;
    nsTArray<nsCString> mUpdates;
  };
  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       nsTArray<nsCString>&& aUpdates) {
    nsCOMPtr<nsIRunnable> updates(new Updates(this, Move(aUpdates)));
    CreateDecryptor(aOrigin, aTopLevelOrigin, aInPBMode, updates);
  }
  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       nsIRunnable* aContinuation) {
    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    EXPECT_TRUE(service);

    mNodeId = GetNodeId(aOrigin, aTopLevelOrigin, aInPBMode);
    EXPECT_TRUE(!mNodeId.IsEmpty());

    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    UniquePtr<GetGMPDecryptorCallback> callback(
      new CreateDecryptorDone(this, aContinuation));
    nsresult rv =
      service->GetGMPDecryptor(&tags, mNodeId, Move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  }

  void TestBasicStorage() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    nsRefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();

    // Send a message to the fake GMP for it to run its own tests internally.
    // It sends us a "test-storage complete" message when its passed, or
    // some other message if its tests fail.
    Expect(NS_LITERAL_CSTRING("test-storage complete"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  /**
   * 1. Generate storage data for some sites.
   * 2. Forget about one of the sites.
   * 3. Check if the storage data for the forgotten site are erased correctly.
   * 4. Check if the storage data for other sites remain unchanged.
   */
  void TestForgetThisSite() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestForgetThisSite_AnotherSite);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  void TestForgetThisSite_AnotherSite() {
    Shutdown();

    // Generate storage data for another site.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestForgetThisSite_CollectSiteInfo);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);

    CreateDecryptor(NS_LITERAL_STRING("http://example3.com"),
                    NS_LITERAL_STRING("http://example4.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  struct NodeInfo {
    explicit NodeInfo(const nsACString& aSite) : siteToForget(aSite) {}
    nsCString siteToForget;
    nsTArray<nsCString> expectedRemainingNodeIds;
  };

  class NodeIdCollector {
  public:
    explicit NodeIdCollector(NodeInfo* aInfo) : mNodeInfo(aInfo) {}
    void operator()(nsIFile* aFile) {
      nsCString salt;
      nsresult rv = ReadSalt(aFile, salt);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      if (!MatchOrigin(aFile, mNodeInfo->siteToForget)) {
        mNodeInfo->expectedRemainingNodeIds.AppendElement(salt);
      }
    }
  private:
    NodeInfo* mNodeInfo;
  };

  void TestForgetThisSite_CollectSiteInfo() {
    nsAutoPtr<NodeInfo> siteInfo(
        new NodeInfo(NS_LITERAL_CSTRING("http://example1.com")));
    // Collect nodeIds that are expected to remain for later comparison.
    EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), NodeIdCollector(siteInfo));
    // Invoke "Forget this site" on the main thread.
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<nsAutoPtr<NodeInfo>>(
        this, &GMPStorageTest::TestForgetThisSite_Forget, siteInfo));
  }

  void TestForgetThisSite_Forget(nsAutoPtr<NodeInfo> aSiteInfo) {
    nsRefPtr<GeckoMediaPluginServiceParent> service =
        GeckoMediaPluginServiceParent::GetSingleton();
    service->ForgetThisSite(NS_ConvertUTF8toUTF16(aSiteInfo->siteToForget));

    nsCOMPtr<nsIThread> thread;
    service->GetThread(getter_AddRefs(thread));

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<nsAutoPtr<NodeInfo>>(
        this, &GMPStorageTest::TestForgetThisSite_Verify, aSiteInfo);
    thread->Dispatch(r, NS_DISPATCH_NORMAL);

    nsCOMPtr<nsIRunnable> f = NS_NewRunnableMethod(
        this, &GMPStorageTest::SetFinished);
    thread->Dispatch(f, NS_DISPATCH_NORMAL);
  }

  class NodeIdVerifier {
  public:
    explicit NodeIdVerifier(const NodeInfo* aInfo)
      : mNodeInfo(aInfo)
      , mExpectedRemainingNodeIds(aInfo->expectedRemainingNodeIds) {}
    void operator()(nsIFile* aFile) {
      nsCString salt;
      nsresult rv = ReadSalt(aFile, salt);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      // Shouldn't match the origin if we clear correctly.
      EXPECT_FALSE(MatchOrigin(aFile, mNodeInfo->siteToForget));
      // Check if remaining nodeIDs are as expected.
      EXPECT_TRUE(mExpectedRemainingNodeIds.RemoveElement(salt));
    }
    ~NodeIdVerifier() {
      EXPECT_TRUE(mExpectedRemainingNodeIds.IsEmpty());
    }
  private:
    const NodeInfo* mNodeInfo;
    nsTArray<nsCString> mExpectedRemainingNodeIds;
  };

  class StorageVerifier {
  public:
    explicit StorageVerifier(const NodeInfo* aInfo)
      : mExpectedRemainingNodeIds(aInfo->expectedRemainingNodeIds) {}
    void operator()(nsIFile* aFile) {
      nsCString salt;
      nsresult rv = aFile->GetNativeLeafName(salt);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      EXPECT_TRUE(mExpectedRemainingNodeIds.RemoveElement(salt));
    }
    ~StorageVerifier() {
      EXPECT_TRUE(mExpectedRemainingNodeIds.IsEmpty());
    }
  private:
    nsTArray<nsCString> mExpectedRemainingNodeIds;
  };

  void TestForgetThisSite_Verify(nsAutoPtr<NodeInfo> aSiteInfo) {
    nsresult rv = EnumerateGMPStorageDir(
        NS_LITERAL_CSTRING("id"), NodeIdVerifier(aSiteInfo));
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    rv = EnumerateGMPStorageDir(
        NS_LITERAL_CSTRING("storage"), StorageVerifier(aSiteInfo));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  }

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/id/.
   * 3. Pass |t| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/id/ and
   *    $profileDir/gmp/storage are removed.
   */
  void TestClearRecentHistory1() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory1_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
}

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/storage/.
   * 3. Pass |t| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/id/ and
   *    $profileDir/gmp/storage are removed.
   */
  void TestClearRecentHistory2() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory2_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/storage/.
   * 3. Pass |t+1| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/id/ and
   *    $profileDir/gmp/storage remain unchanged.
   */
  void TestClearRecentHistory3() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory3_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  class MaxMTimeFinder {
  public:
    MaxMTimeFinder() : mMaxTime(0) {}
    void operator()(nsIFile* aFile) {
      PRTime lastModified;
      nsresult rv = aFile->GetLastModifiedTime(&lastModified);
      if (NS_SUCCEEDED(rv) && lastModified > mMaxTime) {
        mMaxTime = lastModified;
      }
      EnumerateDir(aFile, *this);
    }
    PRTime GetResult() const { return mMaxTime; }
  private:
    PRTime mMaxTime;
  };

  void TestClearRecentHistory1_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r, t, f.GetResult());
  }

  void TestClearRecentHistory2_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r, t, f.GetResult());
  }

  void TestClearRecentHistory3_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory_CheckNonEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r, t, f.GetResult() + 1);
  }

  class FileCounter {
  public:
    FileCounter() : mCount(0) {}
    void operator()(nsIFile* aFile) {
      ++mCount;
    }
    int GetCount() const { return mCount; }
  private:
    int mCount;
  };

  void TestClearRecentHistory_CheckEmpty() {
    FileCounter c1;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), c1);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be no files under $profileDir/gmp/id/
    EXPECT_EQ(c1.GetCount(), 0);

    FileCounter c2;
    rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be no files under $profileDir/gmp/storage/
    EXPECT_EQ(c2.GetCount(), 0);

    SetFinished();
  }

  void TestClearRecentHistory_CheckNonEmpty() {
    FileCounter c1;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), c1);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/id/
    EXPECT_EQ(c1.GetCount(), 1);

    FileCounter c2;
    rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/storage/
    EXPECT_EQ(c2.GetCount(), 1);

    SetFinished();
  }

  void TestCrossOriginStorage() {
    EXPECT_TRUE(!mDecryptor);

    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    auto t = time(0);
    nsCString response("stored crossOriginTestRecordId ");
    response.AppendInt((int64_t)t);
    Expect(response, NS_NewRunnableMethod(this,
      &GMPStorageTest::TestCrossOriginStorage_RecordStoredContinuation));

    nsCString update("store crossOriginTestRecordId ");
    update.AppendInt((int64_t)t);

    // Open decryptor on one, origin, write a record, and test that that
    // record can't be read on another origin.
    CreateDecryptor(NS_LITERAL_STRING("http://example3.com"),
                    NS_LITERAL_STRING("http://example4.com"),
                    false,
                    update);
  }

  void TestCrossOriginStorage_RecordStoredContinuation() {
    // Close the old decryptor, and create a new one on a different origin,
    // and try to read the record.
    Shutdown();

    Expect(NS_LITERAL_CSTRING("retrieve crossOriginTestRecordId succeeded (length 0 bytes)"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example5.com"),
                    NS_LITERAL_STRING("http://example6.com"),
                    false,
                    NS_LITERAL_CSTRING("retrieve crossOriginTestRecordId"));
  }

  void TestPBStorage() {
    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    nsCString response("stored pbdata test-pb-data");
    Expect(response, NS_NewRunnableMethod(this,
      &GMPStorageTest::TestPBStorage_RecordStoredContinuation));

    // Open decryptor on one, origin, write a record, close decryptor,
    // open another, and test that record can be read, close decryptor,
    // then send pb-last-context-closed notification, then open decryptor
    // and check that it can't read that data; it should have been purged.
    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("store pbdata test-pb-data"));
  }

  void TestPBStorage_RecordStoredContinuation() {
    Shutdown();

    Expect(NS_LITERAL_CSTRING("retrieve pbdata succeeded (length 12 bytes)"),
           NS_NewRunnableMethod(this,
              &GMPStorageTest::TestPBStorage_RecordRetrievedContinuation));

    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("retrieve pbdata"));
  }

  void TestPBStorage_RecordRetrievedContinuation() {
    Shutdown();
    SimulatePBModeExit();

    Expect(NS_LITERAL_CSTRING("retrieve pbdata succeeded (length 0 bytes)"),
           NS_NewRunnableMethod(this,
              &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("retrieve pbdata"));
  }

  void NextAsyncShutdownTimeoutTest(nsIRunnable* aContinuation)
  {
    if (mDecryptor) {
      Update(NS_LITERAL_CSTRING("shutdown-mode timeout"));
      Shutdown();
    }
    nsCOMPtr<nsIThread> thread(GetGMPThread());
    thread->Dispatch(aContinuation, NS_DISPATCH_NORMAL);
  }

  void CreateAsyncShutdownTimeoutGMP(const nsAString& aOrigin1,
                                     const nsAString& aOrigin2,
                                     void (GMPStorageTest::*aCallback)()) {
    nsCOMPtr<nsIRunnable> continuation(
      NS_NewRunnableMethodWithArg<nsCOMPtr<nsIRunnable>>(
        this,
        &GMPStorageTest::NextAsyncShutdownTimeoutTest,
        NS_NewRunnableMethod(this, aCallback)));

    CreateDecryptor(aOrigin1, aOrigin2, false, continuation);
  }

  void TestAsyncShutdownTimeout() {
    // Create decryptors that timeout in their async shutdown.
    // If the gtest hangs on shutdown, test fails!
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("http://example7.com"),
                                  NS_LITERAL_STRING("http://example8.com"),
                                  &GMPStorageTest::TestAsyncShutdownTimeout2);
  };

  void TestAsyncShutdownTimeout2() {
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("http://example9.com"),
                                  NS_LITERAL_STRING("http://example10.com"),
                                  &GMPStorageTest::TestAsyncShutdownTimeout3);
  };

  void TestAsyncShutdownTimeout3() {
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("http://example11.com"),
                                  NS_LITERAL_STRING("http://example12.com"),
                                  &GMPStorageTest::SetFinished);
  };

  void TestAsyncShutdownStorage() {
    // Instruct the GMP to write a token (the current timestamp, so it's
    // unique) during async shutdown, then shutdown the plugin, re-create
    // it, and check that the token was successfully stored.
    auto t = time(0);
    nsCString update("shutdown-mode token ");
    nsCString token;
    token.AppendInt((int64_t)t);
    update.Append(token);

    // Wait for a response from the GMP, so we know it's had time to receive
    // the token.
    nsCString response("shutdown-token received ");
    response.Append(token);
    Expect(response, NS_NewRunnableMethodWithArg<nsCString>(this,
      &GMPStorageTest::TestAsyncShutdownStorage_ReceivedShutdownToken, token));

    // Test that a GMP can write to storage during shutdown, and retrieve
    // that written data in a subsequent session.
    CreateDecryptor(NS_LITERAL_STRING("http://example13.com"),
                    NS_LITERAL_STRING("http://example14.com"),
                    false,
                    update);
  }

  void TestAsyncShutdownStorage_ReceivedShutdownToken(const nsCString& aToken) {
    ShutdownThen(NS_NewRunnableMethodWithArg<nsCString>(this,
      &GMPStorageTest::TestAsyncShutdownStorage_AsyncShutdownComplete, aToken));
  }

  void TestAsyncShutdownStorage_AsyncShutdownComplete(const nsCString& aToken) {
    // Create a new instance of the plugin, retrieve the token written
    // during shutdown and verify it is correct.
    nsCString response("retrieved shutdown-token ");
    response.Append(aToken);
    Expect(response,
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example13.com"),
                    NS_LITERAL_STRING("http://example14.com"),
                    false,
                    NS_LITERAL_CSTRING("retrieve-shutdown-token"));
  }

#if defined(XP_WIN)
  void TestOutputProtection() {
    Shutdown();

    Expect(NS_LITERAL_CSTRING("OP tests completed"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example15.com"),
                    NS_LITERAL_STRING("http://example16.com"),
                    false,
                    NS_LITERAL_CSTRING("test-op-apis"));
  }
#endif

  void TestPluginVoucher() {
    Expect(NS_LITERAL_CSTRING("retrieved plugin-voucher: gmp-fake placeholder voucher"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example17.com"),
                    NS_LITERAL_STRING("http://example18.com"),
                    false,
                    NS_LITERAL_CSTRING("retrieve-plugin-voucher"));
  }

  void TestGetRecordNamesInMemoryStorage() {
    TestGetRecordNames(true);
  }

  nsCString mRecordNames;

  void AppendIntPadded(nsACString& aString, uint32_t aInt) {
    if (aInt > 0 && aInt < 10) {
      aString.AppendLiteral("0");
    }
    aString.AppendInt(aInt);
  }

  void TestGetRecordNames(bool aPrivateBrowsing) {
    // Create a number of records of different names.
    const uint32_t num = 100;
    nsTArray<nsCString> updates(num);
    for (uint32_t i = 0; i < num; i++) {
      nsAutoCString response;
      response.AppendLiteral("stored data");
      AppendIntPadded(response, i);
      response.AppendLiteral(" test-data");
      AppendIntPadded(response, i);

      if (i != 0) {
        mRecordNames.AppendLiteral(",");
      }
      mRecordNames.AppendLiteral("data");
      AppendIntPadded(mRecordNames, i);

      nsCString& update = *updates.AppendElement();
      update.AppendLiteral("store data");
      AppendIntPadded(update, i);
      update.AppendLiteral(" test-data");
      AppendIntPadded(update, i);

      nsIRunnable* continuation = nullptr;
      if (i + 1 == num) {
        continuation =
          NS_NewRunnableMethod(this, &GMPStorageTest::TestGetRecordNames_QueryNames);
      }
      Expect(response, continuation);
    }

    CreateDecryptor(NS_LITERAL_STRING("http://foo.com"),
                    NS_LITERAL_STRING("http://bar.com"),
                    aPrivateBrowsing,
                    Move(updates));
  }

  void TestGetRecordNames_QueryNames() {
    nsCString response("record-names ");
    response.Append(mRecordNames);
    Expect(response,
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("retrieve-record-names"));
  }

  void GetRecordNamesPersistentStorage() {
    TestGetRecordNames(false);
  }

  void TestLongRecordNames() {
    NS_NAMED_LITERAL_CSTRING(longRecordName,
      "A_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_"
      "long_record_name");

    NS_NAMED_LITERAL_CSTRING(data, "Just_some_arbitrary_data.");

    MOZ_ASSERT(longRecordName.Length() < GMP_MAX_RECORD_NAME_SIZE);
    MOZ_ASSERT(longRecordName.Length() > 260); // Windows MAX_PATH

    nsCString response("stored ");
    response.Append(longRecordName);
    response.AppendLiteral(" ");
    response.Append(data);
    Expect(response, NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    nsCString update("store ");
    update.Append(longRecordName);
    update.AppendLiteral(" ");
    update.Append(data);
    CreateDecryptor(NS_LITERAL_STRING("http://fuz.com"),
                    NS_LITERAL_STRING("http://baz.com"),
                    false,
                    update);
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

  void ShutdownThen(nsIRunnable* aContinuation) {
    EXPECT_TRUE(!!mDecryptor);
    if (!mDecryptor) {
      return;
    }
    EXPECT_FALSE(mNodeId.IsEmpty());
    nsRefPtr<GMPShutdownObserver> task(
      new GMPShutdownObserver(NS_NewRunnableMethod(this, &GMPStorageTest::Shutdown),
                              aContinuation, mNodeId));
    NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
  }

  void Shutdown() {
    if (mDecryptor) {
      mDecryptor->Close();
      mDecryptor = nullptr;
      mNodeId = EmptyCString();
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
                              GMPSessionMessageType aMessageType,
                              const nsTArray<uint8_t>& aMessage) override
  {
    MonitorAutoLock mon(mMonitor);

    nsCString msg((const char*)aMessage.Elements(), aMessage.Length());
    EXPECT_TRUE(mExpected.Length() > 0);
    bool matches = mExpected[0].mMessage.Equals(msg);
    EXPECT_STREQ(mExpected[0].mMessage.get(), msg.get());
    if (mExpected.Length() > 0 && matches) {
      nsCOMPtr<nsIRunnable> continuation = mExpected[0].mContinuation;
      mExpected.RemoveElementAt(0);
      if (continuation) {
        NS_DispatchToCurrentThread(continuation);
      }
    }
  }

  virtual void SetSessionId(uint32_t aCreateSessionToken,
                            const nsCString& aSessionId) override { }
  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) override {}
  virtual void ResolvePromise(uint32_t aPromiseId) override {}
  virtual void RejectPromise(uint32_t aPromiseId,
                             nsresult aException,
                             const nsCString& aSessionId) override { }
  virtual void ExpirationChange(const nsCString& aSessionId,
                                GMPTimestamp aExpiryTime) override {}
  virtual void SessionClosed(const nsCString& aSessionId) override {}
  virtual void SessionError(const nsCString& aSessionId,
                            nsresult aException,
                            uint32_t aSystemCode,
                            const nsCString& aMessage) override {}
  virtual void KeyStatusChanged(const nsCString& aSessionId,
                                const nsTArray<uint8_t>& aKeyId,
                                GMPMediaKeyStatus aStatus) override { }
  virtual void SetCaps(uint64_t aCaps) override {}
  virtual void Decrypted(uint32_t aId,
                         GMPErr aResult,
                         const nsTArray<uint8_t>& aDecryptedData) override { }
  virtual void Terminated() override { }

private:
  ~GMPStorageTest() { }

  struct ExpectedMessage {
    ExpectedMessage(const nsCString& aMessage, nsIRunnable* aContinuation)
      : mMessage(aMessage)
      , mContinuation(aContinuation)
    {}
    nsCString mMessage;
    nsCOMPtr<nsIRunnable> mContinuation;
  };

  nsTArray<ExpectedMessage> mExpected;

  GMPDecryptorProxy* mDecryptor;
  Monitor mMonitor;
  Atomic<bool> mFinished;
  nsCString mNodeId;
};

void
GMPTestRunner::DoTest(void (GMPTestRunner::*aTestMethod)(GMPTestMonitor&))
{
  nsCOMPtr<nsIThread> thread(GetGMPThread());

  GMPTestMonitor monitor;
  thread->Dispatch(NS_NewRunnableMethodWithArg<GMPTestMonitor&>(this,
                                                                aTestMethod,
                                                                monitor),
                   NS_DISPATCH_NORMAL);
  monitor.AwaitFinished();
}

TEST(GeckoMediaPlugins, GMPTestCodec) {
  nsRefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec1);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec2);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec3);
}

TEST(GeckoMediaPlugins, GMPCrossOrigin) {
  nsRefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin1);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin2);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin3);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin4);
}

TEST(GeckoMediaPlugins, GMPStorageGetNodeId) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestGetNodeId);
}

TEST(GeckoMediaPlugins, GMPStorageBasic) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestBasicStorage);
}

TEST(GeckoMediaPlugins, GMPStorageForgetThisSite) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestForgetThisSite);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory1) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory1);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory2) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory2);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory3) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory3);
}

TEST(GeckoMediaPlugins, GMPStorageCrossOrigin) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestCrossOriginStorage);
}

TEST(GeckoMediaPlugins, GMPStoragePrivateBrowsing) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestPBStorage);
}

TEST(GeckoMediaPlugins, GMPStorageAsyncShutdownTimeout) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestAsyncShutdownTimeout);
}

TEST(GeckoMediaPlugins, GMPStorageAsyncShutdownStorage) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestAsyncShutdownStorage);
}

TEST(GeckoMediaPlugins, GMPPluginVoucher) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestPluginVoucher);
}

#if defined(XP_WIN)
TEST(GeckoMediaPlugins, GMPOutputProtection) {
  // Output Protection is not available pre-Vista.
  if (!IsVistaOrLater()) {
    return;
  }

  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestOutputProtection);
}
#endif

TEST(GeckoMediaPlugins, GMPStorageGetRecordNamesInMemoryStorage) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestGetRecordNamesInMemoryStorage);
}

TEST(GeckoMediaPlugins, GMPStorageGetRecordNamesPersistentStorage) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::GetRecordNamesPersistentStorage);
}

TEST(GeckoMediaPlugins, GMPStorageLongRecordNames) {
  nsRefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestLongRecordNames);
}
