/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/RefPtr.h"

#include "ChromiumCDMCallback.h"
#include "GMPTestMonitor.h"
#include "GMPServiceParent.h"
#include "MediaResult.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsNSSComponent.h" //For EnsureNSSInitializedChromeOrContent
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::gmp;

static already_AddRefed<nsIThread>
GetGMPThread()
{
  RefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  nsCOMPtr<nsIThread> thread;
  EXPECT_TRUE(NS_SUCCEEDED(service->GetThread(getter_AddRefs(thread))));
  return thread.forget();
}

static RefPtr<AbstractThread>
GetAbstractGMPThread()
{
  RefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  return service->GetAbstractGMPThread();
}
/**
 * Enumerate files under |aPath| (non-recursive).
 */
template<typename T>
static nsresult
EnumerateDir(nsIFile* aPath, T&& aDirIter)
{
  nsCOMPtr<nsIDirectoryEnumerator> iter;
  nsresult rv = aPath->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIFile> entry;
  while (NS_SUCCEEDED(iter->GetNextFile(getter_AddRefs(entry))) && entry) {
    aDirIter(entry);
  }
  return NS_OK;
}

/**
 * Enumerate files under $profileDir/gmp/$platform/gmp-fake/$aDir/ (non-recursive).
 */
template<typename T>
static nsresult
EnumerateCDMStorageDir(const nsACString& aDir, T&& aDirIter)
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

class ClearCDMStorageTask : public nsIRunnable
                          , public nsIObserver {
public:
  ClearCDMStorageTask(already_AddRefed<nsIRunnable> Continuation,
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
  virtual ~ClearCDMStorageTask() {}
  nsCOMPtr<nsIRunnable> mContinuation;
  nsCOMPtr<nsIThread> mTarget;
  const PRTime mSince;
};

NS_IMPL_ISUPPORTS(ClearCDMStorageTask, nsIRunnable, nsIObserver)

static void
ClearCDMStorage(already_AddRefed<nsIRunnable> aContinuation,
                nsIThread* aTarget, PRTime aSince = -1)
{
  RefPtr<ClearCDMStorageTask> task(
    new ClearCDMStorageTask(Move(aContinuation), aTarget, aSince));
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

static NodeId
GetNodeId(const nsAString& aOrigin,
          const nsAString& aTopLevelOrigin,
          const nsAString & aGmpName,
          bool aInPBMode)
{
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
  return NodeId(origin, topLevelOrigin, aGmpName);
}

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
IsCDMStorageIsEmpty()
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

class CDMStorageTest
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CDMStorageTest)

  void DoTest(void (CDMStorageTest::*aTestMethod)()) {
    EnsureNSSInitializedChromeOrContent();
    nsCOMPtr<nsIThread> thread(GetGMPThread());
    ClearCDMStorage(
      NewRunnableMethod("CDMStorageTest::DoTest", this, aTestMethod), thread);
    AwaitFinished();
  }

  CDMStorageTest()
    : mMonitor("CDMStorageTest")
    , mFinished(false)
  {
  }

  void
  Update(const nsCString& aMessage)
  {
    nsTArray<uint8_t> msg;
    msg.AppendElements(aMessage.get(), aMessage.Length());
    mCDM->UpdateSession(NS_LITERAL_CSTRING("fake-session-id"), 1, msg);
  }

  void TestGetNodeId()
  {
    AssertIsOnGMPThread();

    EXPECT_TRUE(IsCDMStorageIsEmpty());

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
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    nsCString nodeId1 = GetNodeId(origin1, origin2, false);
    nsCString nodeId2 = GetNodeId(origin1, origin2, false);

    // NodeIds for the same origin pair in non-pb mode should be the same.
    EXPECT_TRUE(nodeId1.Equals(nodeId2));

    // Node ids for a given origin pair should be different for the PB origins should be the same in PB mode.
    EXPECT_TRUE(!PBnodeId1.Equals(nodeId1));
    EXPECT_TRUE(!PBnodeId2.Equals(nodeId2));

    nsCOMPtr<nsIThread> thread(GetGMPThread());
    ClearCDMStorage(
      NewRunnableMethod<nsCString>("CDMStorageTest::TestGetNodeId_Continuation",
                                   this,
                                   &CDMStorageTest::TestGetNodeId_Continuation,
                                   nodeId1),
      thread);
  }

  void TestGetNodeId_Continuation(nsCString aNodeId1) {
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    // Once we clear storage, the node ids generated for the same origin-pair
    // should be different.
    const nsString origin1 = NS_LITERAL_STRING("http://example1.com");
    const nsString origin2 = NS_LITERAL_STRING("http://example2.org");
    nsCString nodeId3 = GetNodeId(origin1, origin2, false);
    EXPECT_TRUE(!aNodeId1.Equals(nodeId3));

    SetFinished();
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

  void CreateDecryptor(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPBMode,
                       nsTArray<nsCString>&& aUpdates) {
    CreateDecryptor(GetNodeId(aOrigin, aTopLevelOrigin, NS_LITERAL_STRING("gmp-fake"), aInPBMode), Move(aUpdates));
  }

  void CreateDecryptor(const NodeId& aNodeId,
                       nsTArray<nsCString>&& aUpdates) {
    RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();
    EXPECT_TRUE(service);

    nsTArray<nsCString> tags;
    tags.AppendElement(NS_LITERAL_CSTRING("fake"));

    RefPtr<CDMStorageTest> self = this;
    RefPtr<gmp::GetCDMParentPromise> promise =
          service->GetCDM(aNodeId, Move(tags), nullptr);
    auto thread = GetAbstractGMPThread();
    promise->Then(thread,
                  __func__,
                  [self, aUpdates](RefPtr<gmp::ChromiumCDMParent> cdm) {
                    self->mCDM = cdm;
                    EXPECT_TRUE(!!self->mCDM);
                    self->mCallback.reset(new CallbackProxy(self));
                    nsCString failureReason;
                    self->mCDM->Init(self->mCallback.get(),
                                     false,
                                     true,
                                     GetMainThreadEventTarget(),
                                     failureReason);

                    for (auto& update : aUpdates) {
                      self->Update(update);
                    }
                  },
                  [](MediaResult rv) { EXPECT_TRUE(false); });
  }

  void TestBasicStorage() {
    AssertIsOnGMPThread();
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    RefPtr<GeckoMediaPluginService> service =
      GeckoMediaPluginService::GetGeckoMediaPluginService();

    // Send a message to the fake GMP for it to run its own tests internally.
    // It sends us a "test-storage complete" message when its passed, or
    // some other message if its tests fail.
    Expect(NS_LITERAL_CSTRING("test-storage complete"),
           NewRunnableMethod("CDMStorageTest::SetFinished",
                             this,
                             &CDMStorageTest::SetFinished));

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
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestForgetThisSite_AnotherSite",
                        this,
                        &CDMStorageTest::TestForgetThisSite_AnotherSite);
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
      NewRunnableMethod("CDMStorageTest::TestForgetThisSite_CollectSiteInfo",
                        this,
                        &CDMStorageTest::TestForgetThisSite_CollectSiteInfo);
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
    EnumerateCDMStorageDir(NS_LITERAL_CSTRING("id"),
                           NodeIdCollector(siteInfo.get()));
    // Invoke "Forget this site" on the main thread.
    SystemGroup::Dispatch(TaskCategory::Other,
                          NewRunnableMethod<UniquePtr<NodeInfo>&&>(
                            "CDMStorageTest::TestForgetThisSite_Forget",
                            this,
                            &CDMStorageTest::TestForgetThisSite_Forget,
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
      "CDMStorageTest::TestForgetThisSite_Verify",
      this,
      &CDMStorageTest::TestForgetThisSite_Verify,
      Move(aSiteInfo));
    thread->Dispatch(r, NS_DISPATCH_NORMAL);

    nsCOMPtr<nsIRunnable> f = NewRunnableMethod(
      "CDMStorageTest::SetFinished", this, &CDMStorageTest::SetFinished);
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
    nsresult rv = EnumerateCDMStorageDir(
        NS_LITERAL_CSTRING("id"), NodeIdVerifier(aSiteInfo.get()));
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    rv = EnumerateCDMStorageDir(
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
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory1_Clear",
                        this,
                        &CDMStorageTest::TestClearRecentHistory1_Clear);
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
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory2_Clear",
                        this,
                        &CDMStorageTest::TestClearRecentHistory2_Clear);
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
    EXPECT_TRUE(IsCDMStorageIsEmpty());

    // Generate storage data for some site.
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory3_Clear",
                        this,
                        &CDMStorageTest::TestClearRecentHistory3_Clear);
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
    nsresult rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("id"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory_CheckEmpty",
                        this,
                        &CDMStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearCDMStorage(r.forget(), t, f.GetResult());
  }

  void TestClearRecentHistory2_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory_CheckEmpty",
                        this,
                        &CDMStorageTest::TestClearRecentHistory_CheckEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearCDMStorage(r.forget(), t, f.GetResult());
  }

  void TestClearRecentHistory3_Clear() {
    MaxMTimeFinder f;
    nsresult rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("storage"), f);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod("CDMStorageTest::TestClearRecentHistory_CheckNonEmpty",
                        this,
                        &CDMStorageTest::TestClearRecentHistory_CheckNonEmpty);
    nsCOMPtr<nsIThread> t(GetGMPThread());
    ClearCDMStorage(r.forget(), t, f.GetResult() + 1);
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
    nsresult rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("id"), c1);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be no files under $profileDir/gmp/$platform/gmp-fake/id/
    EXPECT_EQ(c1.GetCount(), 0);

    FileCounter c2;
    rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be no files under $profileDir/gmp/$platform/gmp-fake/storage/
    EXPECT_EQ(c2.GetCount(), 0);

    SetFinished();
  }

  void TestClearRecentHistory_CheckNonEmpty() {
    FileCounter c1;
    nsresult rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("id"), c1);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/$platform/gmp-fake/id/
    EXPECT_EQ(c1.GetCount(), 1);

    FileCounter c2;
    rv = EnumerateCDMStorageDir(NS_LITERAL_CSTRING("storage"), c2);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    // There should be one directory under $profileDir/gmp/$platform/gmp-fake/storage/
    EXPECT_EQ(c2.GetCount(), 1);

    SetFinished();
  }

  void TestCrossOriginStorage() {
    EXPECT_TRUE(!mCDM);

    // Send the decryptor the message "store recordid $time"
    // Wait for the decrytor to send us "stored recordid $time"
    auto t = time(0);
    nsCString response("stored crossOriginTestRecordId ");
    response.AppendInt((int64_t)t);
    Expect(response,
           NewRunnableMethod(
             "CDMStorageTest::TestCrossOriginStorage_RecordStoredContinuation",
             this,
             &CDMStorageTest::TestCrossOriginStorage_RecordStoredContinuation));

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
           NewRunnableMethod("CDMStorageTest::SetFinished",
                             this,
                             &CDMStorageTest::SetFinished));

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
             "CDMStorageTest::TestPBStorage_RecordStoredContinuation",
             this,
             &CDMStorageTest::TestPBStorage_RecordStoredContinuation));

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
             "CDMStorageTest::TestPBStorage_RecordRetrievedContinuation",
             this,
             &CDMStorageTest::TestPBStorage_RecordRetrievedContinuation));

    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("retrieve pbdata"));
  }

  void TestPBStorage_RecordRetrievedContinuation() {
    Shutdown();
    SimulatePBModeExit();

    Expect(NS_LITERAL_CSTRING("retrieve pbdata succeeded (length 0 bytes)"),
           NewRunnableMethod("CDMStorageTest::SetFinished",
                             this,
                             &CDMStorageTest::SetFinished));

    CreateDecryptor(NS_LITERAL_STRING("http://pb1.com"),
                    NS_LITERAL_STRING("http://pb2.com"),
                    true,
                    NS_LITERAL_CSTRING("retrieve pbdata"));
  }

#if defined(XP_WIN)
  void TestOutputProtection() {
    Shutdown();

    Expect(NS_LITERAL_CSTRING("OP tests completed"),
           NewRunnableMethod("CDMStorageTest::SetFinished",
                             this, &CDMStorageTest::SetFinished));

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
           NewRunnableMethod("CDMStorageTest::SetFinished",
                             this,
                             &CDMStorageTest::SetFinished));

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
    EXPECT_TRUE(!!mCDM);
    if (!mCDM) {
      return;
    }
    EXPECT_FALSE(mNodeId.IsEmpty());
    RefPtr<GMPShutdownObserver> task(new GMPShutdownObserver(
      NewRunnableMethod(
        "CDMStorageTest::Shutdown", this, &CDMStorageTest::Shutdown),
      Move(aContinuation),
      mNodeId));
    SystemGroup::Dispatch(TaskCategory::Other, task.forget());
  }

  void Shutdown() {
    if (mCDM) {
      mCDM->Shutdown();
      mCDM = nullptr;
      mNodeId = EmptyCString();
    }
  }

  void Dummy() {
  }

  void SetFinished() {
    mFinished = true;
    Shutdown();
    nsCOMPtr<nsIRunnable> task =
      NewRunnableMethod("CDMStorageTest::Dummy", this, &CDMStorageTest::Dummy);
    SystemGroup::Dispatch(TaskCategory::Other, task.forget());
  }

  void SessionMessage(const nsACString& aSessionId,
                      uint32_t aMessageType,
                      const nsTArray<uint8_t>& aMessage)
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

  void Terminated() {
    if (mCDM) {
      mCDM->Shutdown();
      mCDM = nullptr;
    }
  }

private:
  ~CDMStorageTest() { }

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

  RefPtr<gmp::ChromiumCDMParent> mCDM;
  Monitor mMonitor;
  Atomic<bool> mFinished;
  nsCString mNodeId;

  class CallbackProxy : public ChromiumCDMCallback {
  public:

    explicit CallbackProxy(CDMStorageTest* aRunner)
      : mRunner(aRunner)
    {
    }

    void SetSessionId(uint32_t aPromiseId,
                      const nsCString& aSessionId) override { }

    void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                   bool aSuccessful) override { }

    void ResolvePromiseWithKeyStatus(uint32_t aPromiseId,
                                     uint32_t aKeyStatus) override { }

    void ResolvePromise(uint32_t aPromiseId) override { }

    void RejectPromise(uint32_t aPromiseId,
                       nsresult aError,
                       const nsCString& aErrorMessage) override {  }

    void SessionMessage(const nsACString& aSessionId,
                        uint32_t aMessageType,
                        nsTArray<uint8_t>&& aMessage) override
    {
      mRunner->SessionMessage(aSessionId, aMessageType, Move(aMessage));
    }

    void SessionKeysChange(const nsCString& aSessionId,
                           nsTArray<mozilla::gmp::CDMKeyInformation>&& aKeysInfo) override { }

    void ExpirationChange(const nsCString& aSessionId,
                          double aSecondsSinceEpoch) override { }

    void SessionClosed(const nsCString& aSessionId) override { }

    void LegacySessionError(const nsCString& aSessionId,
                            nsresult aError,
                            uint32_t aSystemCode,
                            const nsCString& aMessage) override { }

    void Terminated() override { mRunner->Terminated(); }

    void Shutdown() override { mRunner->Shutdown(); }

  private:

    // Warning: Weak ref.
    CDMStorageTest* mRunner;
  };

  UniquePtr<CallbackProxy> mCallback;
}; // class CDMStorageTest


TEST(GeckoMediaPlugins, CDMStorageGetNodeId) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestGetNodeId);
}

TEST(GeckoMediaPlugins, CDMStorageBasic) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestBasicStorage);
}

TEST(GeckoMediaPlugins, CDMStorageForgetThisSite) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestForgetThisSite);
}

TEST(GeckoMediaPlugins, CDMStorageClearRecentHistory1) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestClearRecentHistory1);
}

TEST(GeckoMediaPlugins, CDMStorageClearRecentHistory2) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestClearRecentHistory2);
}

TEST(GeckoMediaPlugins, CDMStorageClearRecentHistory3) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestClearRecentHistory3);
}

TEST(GeckoMediaPlugins, CDMStorageCrossOrigin) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestCrossOriginStorage);
}

TEST(GeckoMediaPlugins, CDMStoragePrivateBrowsing) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestPBStorage);
}

#if defined(XP_WIN)
TEST(GeckoMediaPlugins, GMPOutputProtection) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestOutputProtection);
}
#endif

TEST(GeckoMediaPlugins, CDMStorageLongRecordNames) {
  RefPtr<CDMStorageTest> runner = new CDMStorageTest();
  runner->DoTest(&CDMStorageTest::TestLongRecordNames);
}
