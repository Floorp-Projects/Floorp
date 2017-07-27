/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "GMPTestMonitor.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"
#include "GMPDecryptorProxy.h"
#include "GMPServiceParent.h"
#include "MediaPrefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Atomics.h"
#include "nsNSSComponent.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h" // For MediaKeyStatus
#include "mozilla/dom/MediaKeyMessageEventBinding.h" // For MediaKeyMessageType

using namespace std;

using namespace mozilla;
using namespace mozilla::gmp;

struct GMPTestRunner
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPTestRunner)

  GMPTestRunner() { MediaPrefs::GetSingleton(); }
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
         nsresult (NS_STDCALL GeckoMediaPluginService::*Getter)(GMPCrashHelper*,
                                                                nsTArray<nsCString>*,
                                                                const nsACString&,
                                                                UniquePtr<Base>&&)>
class RunTestGMPVideoCodec : public Base
{
public:
  void Done(T* aGMP, GMPVideoHost* aHost) override
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
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    return ((*service).*Getter)(nullptr, &tags, aNodeId, Move(aCallback));
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
  void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost) override
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
    void Done(typename Base::GMPCodecType* aGMP, GMPVideoHost* aHost) override
    {
      EXPECT_TRUE(aGMP);
      if (aGMP) {
        EXPECT_TRUE(mGMP &&
                    (mGMP->GetPluginId() == aGMP->GetPluginId()) == mShouldBeEqual);
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
  RefPtr<GeckoMediaPluginService> service =
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
 * Enumerate files under $profileDir/gmp/$platform/gmp-fake/$aDir/ (non-recursive).
 */
template<typename T>
static nsresult
EnumerateGMPStorageDir(const nsACString& aDir, T&& aDirIter)
{
  RefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  MOZ_ASSERT(service);

  // $profileDir/gmp/$platform/
  nsCOMPtr<nsIFile> path;
  nsresult rv = service->GetStorageDir(getter_AddRefs(path));
  if (NS_FAILED(rv)) {
    return rv;
  }


  // $profileDir/gmp/$platform/gmp-fake/
  rv = path->Append(NS_LITERAL_STRING("gmp-fake"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // $profileDir/gmp/$platform/gmp-fake/$aDir/
  rv = path->AppendNative(aDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return EnumerateDir(path, aDirIter);
}

class GMPShutdownObserver : public nsIRunnable
                          , public nsIObserver {
public:
  GMPShutdownObserver(already_AddRefed<nsIRunnable> aShutdownTask,
                      already_AddRefed<nsIRunnable> Continuation,
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

class NotifyObserversTask : public Runnable {
public:
  explicit NotifyObserversTask(const char* aTopic)
    : mozilla::Runnable("NotifyObserversTask")
    , mTopic(aTopic)
  {}
  NS_IMETHOD Run() override {
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
  ClearGMPStorageTask(already_AddRefed<nsIRunnable> Continuation,
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
ClearGMPStorage(already_AddRefed<nsIRunnable> aContinuation,
                nsIThread* aTarget, PRTime aSince = -1)
{
  RefPtr<ClearGMPStorageTask> task(
    new ClearGMPStorageTask(Move(aContinuation), aTarget, aSince));
  SystemGroup::Dispatch(TaskCategory::Other, task.forget());
}

static void
SimulatePBModeExit()
{
  // SystemGroup::EventTargetFor() doesn't support NS_DISPATCH_SYNC.
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
  RefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  EXPECT_TRUE(service);
  nsCString nodeId;
  nsresult result;
  UniquePtr<GetNodeIdCallback> callback(new TestGetNodeIdCallback(nodeId,
                                                                  result));

  OriginAttributes attrs;
  attrs.mPrivateBrowsingId = aInPBMode ? 1 : 0;

  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  nsAutoString origin;
  origin.Assign(aOrigin);
  origin.Append(NS_ConvertUTF8toUTF16(suffix));

  nsAutoString topLevelOrigin;
  topLevelOrigin.Assign(aTopLevelOrigin);
  topLevelOrigin.Append(NS_ConvertUTF8toUTF16(suffix));

  // We rely on the fact that the GetNodeId implementation for
  // GeckoMediaPluginServiceParent is synchronous.
  nsresult rv = service->GetNodeId(origin,
                                   topLevelOrigin,
                                   NS_LITERAL_STRING("gmp-fake"),
                                   Move(callback));
  EXPECT_TRUE(NS_SUCCEEDED(rv) && NS_SUCCEEDED(result));
  return nodeId;
}

static bool
IsGMPStorageIsEmpty()
{
  RefPtr<GeckoMediaPluginServiceParent> service =
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
  RefPtr<GeckoMediaPluginService> service =
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
    ClearGMPStorage(
      NewRunnableMethod("GMPStorageTest::DoTest", this, aTestMethod), thread);
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
    ClearGMPStorage(
      NewRunnableMethod<nsCString>("GMPStorageTest::TestGetNodeId_Continuation",
                                   this,
                                   &GMPStorageTest::TestGetNodeId_Continuation,
                                   nodeId1),
      thread);
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
    explicit CreateDecryptorDone(GMPStorageTest* aRunner)
      : mRunner(aRunner)
    {
    }

    void Done(GMPDecryptorProxy* aDecryptor) override
    {
      mRunner->mDecryptor = aDecryptor;
      EXPECT_TRUE(!!mRunner->mDecryptor);

      if (mRunner->mDecryptor) {
        mRunner->mDecryptor->Init(mRunner, false, true);
      }
    }

  private:
    RefPtr<GMPStorageTest> mRunner;
  };

  void CreateDecryptor(const nsCString& aNodeId,
                       const nsCString& aUpdate)
  {
    nsTArray<nsCString> updates;
    updates.AppendElement(aUpdate);
    nsCOMPtr<nsIRunnable> continuation(new Updates(this, Move(updates)));
    CreateDecryptor(aNodeId, continuation);
  }

  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       const nsCString& aUpdate)
  {
    nsTArray<nsCString> updates;
    updates.AppendElement(aUpdate);
    CreateDecryptor(aOrigin, aTopLevelOrigin, aInPBMode, Move(updates));
  }
  class Updates : public Runnable
  {
  public:
    Updates(GMPStorageTest* aRunner, nsTArray<nsCString>&& aUpdates)
      : mozilla::Runnable("GMPStorageTest::Updates")
      , mRunner(aRunner)
      , mUpdates(Move(aUpdates))
    {
    }

    NS_IMETHOD Run() override
    {
      for (auto& update : mUpdates) {
        mRunner->Update(update);
      }
      return NS_OK;
    }

  private:
    RefPtr<GMPStorageTest> mRunner;
    nsTArray<nsCString> mUpdates;
  };
  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       nsTArray<nsCString>&& aUpdates) {
    nsCOMPtr<nsIRunnable> updates(new Updates(this, Move(aUpdates)));
    CreateDecryptor(GetNodeId(aOrigin, aTopLevelOrigin, aInPBMode), updates);
  }

  void CreateDecryptor(const nsCString& aNodeId,
                       nsIRunnable* aContinuation) {
    RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    EXPECT_TRUE(service);

    mNodeId = aNodeId;
    EXPECT_TRUE(!mNodeId.IsEmpty());

    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    UniquePtr<GetGMPDecryptorCallback> callback(
      new CreateDecryptorDone(this));

    // Continue after the OnSetDecryptorId message, so that we don't
    // get warnings in the async shutdown tests due to receiving the
    // SetDecryptorId message after we've started shutdown.
    mSetDecryptorIdContinuation = aContinuation;

    nsresult rv =
      service->GetGMPDecryptor(nullptr, &tags, mNodeId, Move(callback));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  }

  void TestBasicStorage() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();

    // Send a message to the fake GMP for it to run its own tests internally.
    // It sends us a "test-storage complete" message when its passed, or
    // some other message if its tests fail.
    Expect(NS_LITERAL_CSTRING("test-storage complete"),
           NewRunnableMethod("GMPStorageTest::SetFinished",
                             this,
                             &GMPStorageTest::SetFinished));

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
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestForgetThisSite_AnotherSite",
                        this,
                        &GMPStorageTest::TestForgetThisSite_AnotherSite);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r.forget());

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  void TestForgetThisSite_AnotherSite() {
    Shutdown();

    // Generate storage data for another site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestForgetThisSite_CollectSiteInfo",
                        this,
                        &GMPStorageTest::TestForgetThisSite_CollectSiteInfo);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r.forget());

    CreateDecryptor(NS_LITERAL_STRING("http://example3.com"),
                    NS_LITERAL_STRING("http://example4.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  struct NodeInfo {
    explicit NodeInfo(const nsACString& aSite,
                      const mozilla::OriginAttributesPattern& aPattern)
      : siteToForget(aSite)
      , mPattern(aPattern)
    { }
    nsCString siteToForget;
    mozilla::OriginAttributesPattern mPattern;
    nsTArray<nsCString> expectedRemainingNodeIds;
  };

  class NodeIdCollector {
  public:
    explicit NodeIdCollector(NodeInfo* aInfo) : mNodeInfo(aInfo) {}
    void operator()(nsIFile* aFile) {
      nsCString salt;
      nsresult rv = ReadSalt(aFile, salt);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      if (!MatchOrigin(aFile, mNodeInfo->siteToForget, mNodeInfo->mPattern)) {
        mNodeInfo->expectedRemainingNodeIds.AppendElement(salt);
      }
    }
  private:
    NodeInfo* mNodeInfo;
  };

  void TestForgetThisSite_CollectSiteInfo() {
    mozilla::OriginAttributesPattern pattern;

    UniquePtr<NodeInfo> siteInfo(
        new NodeInfo(NS_LITERAL_CSTRING("http://example1.com"),
                     pattern));
    // Collect nodeIds that are expected to remain for later comparison.
    EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"),
                           NodeIdCollector(siteInfo.get()));
    // Invoke "Forget this site" on the main thread.
    SystemGroup::Dispatch(TaskCategory::Other,
                          NewRunnableMethod<UniquePtr<NodeInfo>&&>(
                            "GMPStorageTest::TestForgetThisSite_Forget",
                            this,
                            &GMPStorageTest::TestForgetThisSite_Forget,
                            Move(siteInfo)));
  }

  void TestForgetThisSite_Forget(UniquePtr<NodeInfo>&& aSiteInfo) {
    RefPtr<GeckoMediaPluginServiceParent> service =
        GeckoMediaPluginServiceParent::GetSingleton();
    service->ForgetThisSiteNative(NS_ConvertUTF8toUTF16(aSiteInfo->siteToForget),
                                  aSiteInfo->mPattern);

    nsCOMPtr<nsIThread> thread;
    service->GetThread(getter_AddRefs(thread));

    nsCOMPtr<nsIRunnable> r = NewRunnableMethod<UniquePtr<NodeInfo>&&>(
      "GMPStorageTest::TestForgetThisSite_Verify",
      this,
      &GMPStorageTest::TestForgetThisSite_Verify,
      Move(aSiteInfo));
    thread->Dispatch(r, NS_DISPATCH_NORMAL);

    nsCOMPtr<nsIRunnable> f = NewRunnableMethod(
      "GMPStorageTest::SetFinished", this, &GMPStorageTest::SetFinished);
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
      EXPECT_FALSE(MatchOrigin(aFile, mNodeInfo->siteToForget, mNodeInfo->mPattern));
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

  void TestForgetThisSite_Verify(UniquePtr<NodeInfo>&& aSiteInfo) {
    nsresult rv = EnumerateGMPStorageDir(
        NS_LITERAL_CSTRING("id"), NodeIdVerifier(aSiteInfo.get()));
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    rv = EnumerateGMPStorageDir(
        NS_LITERAL_CSTRING("storage"), StorageVerifier(aSiteInfo.get()));
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  }

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/$platform/gmp-fake/id/.
   * 3. Pass |t| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/$platform/gmp-fake/id/ and
   *    $profileDir/gmp/$platform/gmp-fake/storage are removed.
   */
  void TestClearRecentHistory1() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory1_Clear",
                        this,
                        &GMPStorageTest::TestClearRecentHistory1_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r.forget());

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
}

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/$platform/gmp-fake/storage/.
   * 3. Pass |t| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/$platform/gmp-fake/id/ and
   *    $profileDir/gmp/$platform/gmp-fake/storage are removed.
   */
  void TestClearRecentHistory2() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory2_Clear",
                        this,
                        &GMPStorageTest::TestClearRecentHistory2_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r.forget());

    CreateDecryptor(NS_LITERAL_STRING("http://example1.com"),
                    NS_LITERAL_STRING("http://example2.com"),
                    false,
                    NS_LITERAL_CSTRING("test-storage"));
  }

  /**
   * 1. Generate some storage data.
   * 2. Find the max mtime |t| in $profileDir/gmp/$platform/gmp-fake/storage/.
   * 3. Pass |t+1| to clear recent history.
   * 4. Check if all directories in $profileDir/gmp/$platform/gmp-fake/id/ and
   *    $profileDir/gmp/$platform/gmp-fake/storage remain unchanged.
   */
  void TestClearRecentHistory3() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsGMPStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory3_Clear",
                        this,
                        &GMPStorageTest::TestClearRecentHistory3_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r.forget());

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

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory_CheckEmpty",
                        this,
                        &GMPStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r.forget(), t, f.GetResult());
  }

  void TestClearRecentHistory2_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory_CheckEmpty",
                        this,
                        &GMPStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r.forget(), t, f.GetResult());
  }

  void TestClearRecentHistory3_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("GMPStorageTest::TestClearRecentHistory_CheckNonEmpty",
                        this,
                        &GMPStorageTest::TestClearRecentHistory_CheckNonEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearGMPStorage(r.forget(), t, f.GetResult() + 1);
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
    // There should be no files under $profileDir/gmp/$platform/gmp-fake/id/
    EXPECT_EQ(c1.GetCount(), 0);

    FileCounter c2;
    rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be no files under $profileDir/gmp/$platform/gmp-fake/storage/
    EXPECT_EQ(c2.GetCount(), 0);

    SetFinished();
  }

  void TestClearRecentHistory_CheckNonEmpty() {
    FileCounter c1;
    nsresult rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), c1);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/$platform/gmp-fake/id/
    EXPECT_EQ(c1.GetCount(), 1);

    FileCounter c2;
    rv = EnumerateGMPStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/$platform/gmp-fake/storage/
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
    Expect(response,
           NewRunnableMethod(
             "GMPStorageTest::TestCrossOriginStorage_RecordStoredContinuation",
             this,
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

    Expect(NS_LITERAL_CSTRING(
             "retrieve crossOriginTestRecordId succeeded (length 0 bytes)"),
           NewRunnableMethod("GMPStorageTest::SetFinished",
                             this,
                             &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example5.com"),
                    NS_LITERAL_STRING("http://example6.com"),
                    false,
                    NS_LITERAL_CSTRING("retrieve crossOriginTestRecordId"));
  }

  void TestPBStorage() {
    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    nsCString response("stored pbdata test-pb-data");
    Expect(response,
           NewRunnableMethod(
             "GMPStorageTest::TestPBStorage_RecordStoredContinuation",
             this,
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
           NewRunnableMethod(
             "GMPStorageTest::TestPBStorage_RecordRetrievedContinuation",
             this,
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
           NewRunnableMethod("GMPStorageTest::SetFinished",
                             this,
                             &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("retrieve pbdata"));
  }

#if defined(XP_WIN)
  void TestOutputProtection() {
    Shutdown();

    Expect(NS_LITERAL_CSTRING("OP tests completed"),
           NewRunnableMethod("GMPStorageTest::SetFinished",
                             this, &GMPStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://example15.com"),
                    NS_LITERAL_STRING("http://example16.com"),
                    false,
                    NS_LITERAL_CSTRING("test-op-apis"));
  }
#endif

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
    Expect(response,
           NewRunnableMethod("GMPStorageTest::SetFinished",
                             this,
                             &GMPStorageTest::SetFinished));

    nsCString update("store ");
    update.Append(longRecordName);
    update.AppendLiteral(" ");
    update.Append(data);
    CreateDecryptor(NS_LITERAL_STRING("http://fuz.com"),
                    NS_LITERAL_STRING("http://baz.com"),
                    false,
                    update);
  }

  void Expect(const nsCString& aMessage, already_AddRefed<nsIRunnable> aContinuation) {
    mExpected.AppendElement(ExpectedMessage(aMessage, Move(aContinuation)));
  }

  void AwaitFinished() {
    mozilla::SpinEventLoopUntil([&]() -> bool { return mFinished; });
    mFinished = false;
  }

  void ShutdownThen(already_AddRefed<nsIRunnable> aContinuation) {
    EXPECT_TRUE(!!mDecryptor);
    if (!mDecryptor) {
      return;
    }
    EXPECT_FALSE(mNodeId.IsEmpty());
    RefPtr<GMPShutdownObserver> task(new GMPShutdownObserver(
      NewRunnableMethod(
        "GMPStorageTest::Shutdown", this, &GMPStorageTest::Shutdown),
      Move(aContinuation),
      mNodeId));
    SystemGroup::Dispatch(TaskCategory::Other, task.forget());
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
    nsCOMPtr<nsIRunnable> task =
      NewRunnableMethod("GMPStorageTest::Dummy", this, &GMPStorageTest::Dummy);
    SystemGroup::Dispatch(TaskCategory::Other, task.forget());
  }

  void SessionMessage(const nsCString& aSessionId,
                      mozilla::dom::MediaKeyMessageType aMessageType,
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

  void SetDecryptorId(uint32_t aId) override
  {
    if (!mSetDecryptorIdContinuation) {
      return;
    }
    nsCOMPtr<nsIThread> thread(GetGMPThread());
    thread->Dispatch(mSetDecryptorIdContinuation, NS_DISPATCH_NORMAL);
    mSetDecryptorIdContinuation = nullptr;
  }

  void SetSessionId(uint32_t aCreateSessionToken,
                    const nsCString& aSessionId) override { }
  void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                 bool aSuccess) override {}
  void ResolvePromise(uint32_t aPromiseId) override {}
  void RejectPromise(uint32_t aPromiseId,
                     nsresult aException,
                     const nsCString& aSessionId) override { }
  void ExpirationChange(const nsCString& aSessionId,
                        UnixTime aExpiryTime) override {}
  void SessionClosed(const nsCString& aSessionId) override {}
  void SessionError(const nsCString& aSessionId,
                    nsresult aException,
                    uint32_t aSystemCode,
                    const nsCString& aMessage) override {}
  void Decrypted(uint32_t aId,
                 mozilla::DecryptStatus aResult,
                 const nsTArray<uint8_t>& aDecryptedData) override { }

  void BatchedKeyStatusChanged(const nsCString& aSessionId,
                               const nsTArray<CDMKeyInfo>& aKeyInfos) override { }

  void Terminated() override {
    if (mDecryptor) {
      mDecryptor->Close();
      mDecryptor = nullptr;
    }
  }

private:
  ~GMPStorageTest() { }

  struct ExpectedMessage {
    ExpectedMessage(const nsCString& aMessage, already_AddRefed<nsIRunnable> aContinuation)
      : mMessage(aMessage)
      , mContinuation(aContinuation)
    {}
    nsCString mMessage;
    nsCOMPtr<nsIRunnable> mContinuation;
  };

  nsTArray<ExpectedMessage> mExpected;

  RefPtr<nsIRunnable> mSetDecryptorIdContinuation;

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
  thread->Dispatch(NewRunnableMethod<GMPTestMonitor&>(
                     "GMPTestRunner::DoTest", this, aTestMethod, monitor),
                   NS_DISPATCH_NORMAL);
  monitor.AwaitFinished();
}

TEST(GeckoMediaPlugins, GMPTestCodec) {
  RefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec1);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec2);
  runner->DoTest(&GMPTestRunner::RunTestGMPTestCodec3);
}

TEST(GeckoMediaPlugins, GMPCrossOrigin) {
  RefPtr<GMPTestRunner> runner = new GMPTestRunner();
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin1);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin2);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin3);
  runner->DoTest(&GMPTestRunner::RunTestGMPCrossOrigin4);
}

TEST(GeckoMediaPlugins, GMPStorageGetNodeId) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestGetNodeId);
}

TEST(GeckoMediaPlugins, GMPStorageBasic) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestBasicStorage);
}

TEST(GeckoMediaPlugins, GMPStorageForgetThisSite) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestForgetThisSite);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory1) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory1);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory2) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory2);
}

TEST(GeckoMediaPlugins, GMPStorageClearRecentHistory3) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestClearRecentHistory3);
}

TEST(GeckoMediaPlugins, GMPStorageCrossOrigin) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestCrossOriginStorage);
}

TEST(GeckoMediaPlugins, GMPStoragePrivateBrowsing) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestPBStorage);
}

#if defined(XP_WIN)
TEST(GeckoMediaPlugins, GMPOutputProtection) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestOutputProtection);
}
#endif

TEST(GeckoMediaPlugins, GMPStorageLongRecordNames) {
  RefPtr<GMPStorageTest> runner = new GMPStorageTest();
  runner->DoTest(&GMPStorageTest::TestLongRecordNames);
}
