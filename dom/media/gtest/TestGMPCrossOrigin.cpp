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

#if defined(XP_WIN)
#include "mozilla/WindowsVersion.h"
#endif

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
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
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

  NS_IMETHOD Run() MOZ_OVERRIDE {
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
                     const char16_t* aSomeData) MOZ_OVERRIDE
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
  nsRefPtr<nsIRunnable> mShutdownTask;
  nsRefPtr<nsIRunnable> mContinuation;
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

  NS_IMETHOD Run() MOZ_OVERRIDE {
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

    mNodeId = GetNodeId(aOrigin, aTopLevelOrigin, aInPBMode);
    EXPECT_TRUE(!mNodeId.IsEmpty());

    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    nsresult rv = service->GetGMPDecryptor(&tags, mNodeId, &mDecryptor);
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
    CreateDecryptor(NS_LITERAL_STRING("example1.com"),
                    NS_LITERAL_STRING("example2.com"),
                    false);

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestForgetThisSite_AnotherSite);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);
    Update(NS_LITERAL_CSTRING("test-storage"));
  }

  void TestForgetThisSite_AnotherSite() {
    Shutdown();

    // Generate storage data for another site.
    CreateDecryptor(NS_LITERAL_STRING("example3.com"),
                    NS_LITERAL_STRING("example4.com"),
                    false);

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestForgetThisSite_CollectSiteInfo);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);
    Update(NS_LITERAL_CSTRING("test-storage"));
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
        new NodeInfo(NS_LITERAL_CSTRING("example1.com")));
    // Collect nodeIds that are expected to remain for later comparison.
    EnumerateGMPStorageDir(NS_LITERAL_CSTRING("id"), NodeIdCollector(siteInfo));
    // Invoke "Forget this site" on the main thread.
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<nsAutoPtr<NodeInfo>>(
        this, &GMPStorageTest::TestForgetThisSite_Forget, siteInfo));
  }

  void TestForgetThisSite_Forget(nsAutoPtr<NodeInfo> aSiteInfo) {
    nsRefPtr<GeckoMediaPluginService> service =
        GeckoMediaPluginService::GetGeckoMediaPluginService();
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
    CreateDecryptor(NS_LITERAL_STRING("example1.com"),
                    NS_LITERAL_STRING("example2.com"),
                    false);

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory1_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);
    Update(NS_LITERAL_CSTRING("test-storage"));

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
    CreateDecryptor(NS_LITERAL_STRING("example1.com"),
                    NS_LITERAL_STRING("example2.com"),
                    false);

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory2_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);
    Update(NS_LITERAL_CSTRING("test-storage"));

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
    CreateDecryptor(NS_LITERAL_STRING("example1.com"),
                    NS_LITERAL_STRING("example2.com"),
                    false);

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
        this, &GMPStorageTest::TestClearRecentHistory3_Clear);
    Expect(NS_LITERAL_CSTRING("test-storage complete"), r);
    Update(NS_LITERAL_CSTRING("test-storage"));

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

  void CreateAsyncShutdownTimeoutGMP(const nsAString& aOrigin1,
                                     const nsAString& aOrigin2) {
    CreateDecryptor(aOrigin1, aOrigin2, false);
    Update(NS_LITERAL_CSTRING("shutdown-mode timeout"));
    Shutdown();
  }

  void TestAsyncShutdownTimeout() {
    // Create decryptors that timeout in their async shutdown.
    // If the gtest hangs on shutdown, test fails!
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("example7.com"),
                                  NS_LITERAL_STRING("example8.com"));
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("example9.com"),
                                  NS_LITERAL_STRING("example10.com"));
    CreateAsyncShutdownTimeoutGMP(NS_LITERAL_STRING("example11.com"),
                                  NS_LITERAL_STRING("example12.com"));
    SetFinished();
  };

  void TestAsyncShutdownStorage() {
    // Test that a GMP can write to storage during shutdown, and retrieve
    // that written data in a subsequent session.
    CreateDecryptor(NS_LITERAL_STRING("example13.com"),
                    NS_LITERAL_STRING("example14.com"),
                    false);

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

    Update(update);
  }

  void TestAsyncShutdownStorage_ReceivedShutdownToken(const nsCString& aToken) {
    ShutdownThen(NS_NewRunnableMethodWithArg<nsCString>(this,
      &GMPStorageTest::TestAsyncShutdownStorage_AsyncShutdownComplete, aToken));
  }

  void TestAsyncShutdownStorage_AsyncShutdownComplete(const nsCString& aToken) {
    // Create a new instance of the plugin, retrieve the token written
    // during shutdown and verify it is correct.
    CreateDecryptor(NS_LITERAL_STRING("example13.com"),
                    NS_LITERAL_STRING("example14.com"),
                    false);
    nsCString response("retrieved shutdown-token ");
    response.Append(aToken);
    Expect(response,
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("retrieve-shutdown-token"));
  }

#if defined(XP_WIN)
  void TestOutputProtection() {
    Shutdown();

    CreateDecryptor(NS_LITERAL_STRING("example15.com"),
                    NS_LITERAL_STRING("example16.com"),
                    false);

    Expect(NS_LITERAL_CSTRING("OP tests completed"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("test-op-apis"));
  }
#endif

  void TestPluginVoucher() {
    CreateDecryptor(NS_LITERAL_STRING("example17.com"),
                    NS_LITERAL_STRING("example18.com"),
                    false);
    Expect(NS_LITERAL_CSTRING("retrieved plugin-voucher: gmp-fake placeholder voucher"),
           NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));
    Update(NS_LITERAL_CSTRING("retrieve-plugin-voucher"));
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
    CreateDecryptor(NS_LITERAL_STRING("foo.com"),
                    NS_LITERAL_STRING("bar.com"),
                    aPrivateBrowsing);

    // Create a number of records of different names.
    const uint32_t num = 100;
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

      nsAutoCString update;
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
      Update(update);
    }
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

    CreateDecryptor(NS_LITERAL_STRING("fuz.com"),
                    NS_LITERAL_STRING("baz.com"),
                    false);

    nsCString response("stored ");
    response.Append(longRecordName);
    response.AppendLiteral(" ");
    response.Append(data);
    Expect(response, NS_NewRunnableMethod(this, &GMPStorageTest::SetFinished));

    nsCString update("store ");
    update.Append(longRecordName);
    update.AppendLiteral(" ");
    update.Append(data);
    Update(update);
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
                              const nsTArray<uint8_t>& aMessage) MOZ_OVERRIDE
  {
    MonitorAutoLock mon(mMonitor);

    nsCString msg((const char*)aMessage.Elements(), aMessage.Length());
    EXPECT_TRUE(mExpected.Length() > 0);
    bool matches = mExpected[0].mMessage.Equals(msg);
    EXPECT_STREQ(mExpected[0].mMessage.get(), msg.get());
    if (mExpected.Length() > 0 && matches) {
      nsRefPtr<nsIRunnable> continuation = mExpected[0].mContinuation;
      mExpected.RemoveElementAt(0);
      if (continuation) {
        NS_DispatchToCurrentThread(continuation);
      }
    }
  }

  virtual void SetSessionId(uint32_t aCreateSessionToken,
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
  virtual void KeyStatusChanged(const nsCString& aSessionId,
                                const nsTArray<uint8_t>& aKeyId,
                                GMPMediaKeyStatus aStatus) MOZ_OVERRIDE { }
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
  nsCString mNodeId;
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
