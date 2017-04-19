/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerScriptCache.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsICacheInfoChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"

#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "ServiceWorkerManager.h"
#include "Workers.h"
#include "nsStringStream.h"

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;
using mozilla::ipc::PrincipalInfo;

BEGIN_WORKERS_NAMESPACE

namespace serviceWorkerScriptCache {

namespace {

// XXX A sandbox nsIGlobalObject does not preserve its reflector, so |aSandbox|
// must be kept alive as long as the CacheStorage if you want to ensure that
// the CacheStorage will continue to work. Failures will manifest as errors
// like "JavaScript error: , line 0: TypeError: The expression cannot be
// converted to return the specified type."
already_AddRefed<CacheStorage>
CreateCacheStorage(JSContext* aCx, nsIPrincipal* aPrincipal, ErrorResult& aRv,
                   JS::MutableHandle<JSObject*> aSandbox)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  aRv = xpc->CreateSandbox(aCx, aPrincipal, aSandbox.address());
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> sandboxGlobalObject = xpc::NativeGlobal(aSandbox);
  if (!sandboxGlobalObject) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // We assume private browsing is not enabled here.  The ScriptLoader
  // explicitly fails for private browsing so there should never be
  // a service worker running in private browsing mode.  Therefore if
  // we are purging scripts or running a comparison algorithm we cannot
  // be in private browing.
  //
  // Also, bypass the CacheStorage trusted origin checks.  The ServiceWorker
  // has validated the origin prior to this point.  All the information
  // to revalidate is not available now.
  return CacheStorage::CreateOnMainThread(cache::CHROME_ONLY_NAMESPACE,
                                          sandboxGlobalObject, aPrincipal,
                                          false /* private browsing */,
                                          true /* force trusted origin */,
                                          aRv);
}

class CompareManager;
class CompareCache;

// This class downloads a URL from the network, compare the downloaded script
// with an existing cache if provided, and report to CompareManager via calling
// ComparisonFinished().
class CompareNetwork final : public nsIStreamLoaderObserver,
                             public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIREQUESTOBSERVER

  explicit CompareNetwork(CompareManager* aManager)
    : mManager(aManager)
    , mIsMainScript(true)
    , mInternalHeaders(new InternalHeaders())
    , mState(WaitingForInitialization)
    , mNetworkResult(NS_OK)
    , mCacheResult(NS_OK)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal,
             const nsAString& aURL,
             bool aIsMainScript,
             nsILoadGroup* aLoadGroup,
             Cache* const aCache);

  void
  Abort();

  void
  NetworkFinished(nsresult aRv);

  void
  CacheFinished(nsresult aRv);

  const nsString& Buffer() const
  {
    AssertIsOnMainThread();
    return mBuffer;
  }

  const nsString&
  URL() const
  {
    AssertIsOnMainThread();
    return mURL;
  }

  const ChannelInfo&
  GetChannelInfo() const
  {
    return mChannelInfo;
  }

  already_AddRefed<InternalHeaders>
  GetInternalHeaders() const
  {
    RefPtr<InternalHeaders> internalHeaders = mInternalHeaders;
    return internalHeaders.forget();
  }

  UniquePtr<PrincipalInfo>
  TakePrincipalInfo()
  {
    return Move(mPrincipalInfo);
  }

private:
  ~CompareNetwork()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCC);
  }

  void
  Finished();

  nsresult
  SetPrincipalInfo(nsIChannel* aChannel);

  RefPtr<CompareManager> mManager;
  RefPtr<CompareCache> mCC;
  nsCOMPtr<nsIChannel> mChannel;
  nsString mBuffer;

  nsString mURL;
  bool mIsMainScript;
  ChannelInfo mChannelInfo;
  RefPtr<InternalHeaders> mInternalHeaders;
  UniquePtr<PrincipalInfo> mPrincipalInfo;

  enum {
    WaitingForInitialization,
    WaitingForBothFinished,
    WaitingForNetworkFinished,
    WaitingForCacheFinished,
    Redundant
  } mState;

  nsresult mNetworkResult;
  nsresult mCacheResult;
};

NS_IMPL_ISUPPORTS(CompareNetwork, nsIStreamLoaderObserver,
                  nsIRequestObserver)

// This class gets a cached Response from the CacheStorage and then it calls
// CacheFinished() in the CompareManager.
class CompareCache final : public PromiseNativeHandler
                         , public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  explicit CompareCache(CompareNetwork* aCN)
    : mCN(aCN)
    , mState(WaitingForInitialization)
    , mInCache(false)
  {
    MOZ_ASSERT(aCN);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             Cache* const aCache);

  void
  Abort();

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  const nsString& Buffer() const
  {
    AssertIsOnMainThread();
    return mBuffer;
  }

  const nsString& URL() const
  {
    AssertIsOnMainThread();
    return mURL;
  }

  bool
  InCache()
  {
    return mInCache;
  }

private:
  ~CompareCache()
  {
    AssertIsOnMainThread();
  }

  void
  Finished(nsresult aStatus, bool aInCache);

  void
  ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue);

  RefPtr<CompareNetwork> mCN;
  nsCOMPtr<nsIInputStreamPump> mPump;

  nsString mURL;
  nsString mBuffer;

  enum {
    WaitingForInitialization,
    WaitingForValue,
    Redundant
  } mState;

  bool mInCache;
};

NS_IMPL_ISUPPORTS(CompareCache, nsIStreamLoaderObserver)

class CompareManager final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  explicit CompareManager(ServiceWorkerRegistrationInfo* aRegistration,
                          CompareCallback* aCallback)
    : mRegistration(aRegistration)
    , mCallback(aCallback)
    , mState(WaitingForInitialization)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aRegistration);
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             const nsAString& aCacheName, nsILoadGroup* aLoadGroup);

  const nsString&
  URL() const
  {
    AssertIsOnMainThread();
    return mURL;
  }

  void
  SetMaxScope(const nsACString& aMaxScope)
  {
    mMaxScope = aMaxScope;
  }

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetRegistration()
  {
    RefPtr<ServiceWorkerRegistrationInfo> copy = mRegistration.get();
    return copy.forget();
  }

  void
  SaveLoadFlags(nsLoadFlags aLoadFlags)
  {
    mCallback->SaveLoadFlags(aLoadFlags);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  CacheStorage*
  CacheStorage_()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCacheStorage);
    return mCacheStorage;
  }

  void
  ComparisonFinished(nsresult aStatus, bool aIsEqual = false)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mState == WaitingForScriptOrComparisonResult);

    if (NS_WARN_IF(NS_FAILED(aStatus))) {
      Fail(aStatus);
      return;
    }

    if (aIsEqual) {
      mCallback->ComparisonResult(aStatus, aIsEqual, EmptyString(), mMaxScope);
      Cleanup();
      return;
    }

    // Write to Cache so ScriptLoader reads succeed.
    mState = WaitingForOpen;
    WriteNetworkBufferToNewCache();
  }

private:
  ~CompareManager()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCN);
  }

  void
  Fail(nsresult aStatus);

  void
  Cleanup();

  void
  FetchScript(const nsAString& aURL,
              bool aIsMainScript,
              Cache* const aCache)
  {
    mCN = new CompareNetwork(this);
    nsresult rv = mCN->Initialize(mPrincipal,
                                  aURL,
                                  aIsMainScript,
                                  mLoadGroup,
                                  aCache);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(rv);
    }
  }

  void
  ManageOldCache(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForExistingOpen);

    if (NS_WARN_IF(!aValue.isObject())) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    MOZ_ASSERT(!mOldCache);
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj) ||
        NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Cache, obj, mOldCache)))) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    Optional<RequestOrUSVString> request;
    CacheQueryOptions options;
    ErrorResult error;
    RefPtr<Promise> promise = mOldCache->Keys(request, options, error);
    if (NS_WARN_IF(error.Failed())) {
      Fail(error.StealNSResult());
      return;
    }

    mState = WaitingForExistingKeys;
    promise->AppendNativeHandler(this);
    return;
  }

  void
  ManageOldKeys(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForExistingKeys);

    if (NS_WARN_IF(!aValue.isObject())) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj)) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    uint32_t len = 0;
    if (!JS_GetArrayLength(aCx, obj, &len)) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    // Populate the script URLs.
    for (uint32_t i = 0; i < len; ++i) {
      JS::Rooted<JS::Value> val(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, obj, i, &val)) ||
          NS_WARN_IF(!val.isObject())) {
        Fail(NS_ERROR_FAILURE);
        return;
      }

      Request* request;
      JS::Rooted<JSObject*> requestObj(aCx, &val.toObject());
      if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Request,
                requestObj,
                request)))) {
        Fail(NS_ERROR_FAILURE);
        continue;
      };

      nsString URL;
      request->GetUrl(URL);
    }

    mState = WaitingForScriptOrComparisonResult;
    FetchScript(mURL, true /* aIsMainScript */, mOldCache);
    return;
  }

  void
  ManageNewCache(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForOpen);

    if (NS_WARN_IF(!aValue.isObject())) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj)) {
      Fail(NS_ERROR_FAILURE);
      return;
    }

    Cache* cache = nullptr;
    nsresult rv = UNWRAP_OBJECT(Cache, obj, cache);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(rv);
      return;
    }

    // Just to be safe.
    RefPtr<Cache> kungfuDeathGrip = cache;
    mState = WaitingForPut;
    WriteToCache(cache);
    return;
  }

  void
  WriteNetworkBufferToNewCache()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCN);
    MOZ_ASSERT(mCacheStorage);
    MOZ_ASSERT(mNewCacheName.IsEmpty());

    ErrorResult result;
    result = serviceWorkerScriptCache::GenerateCacheName(mNewCacheName);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    RefPtr<Promise> cacheOpenPromise = mCacheStorage->Open(mNewCacheName, result);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    cacheOpenPromise->AppendNativeHandler(this);
  }

  void
  WriteToCache(Cache* aCache)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aCache);
    MOZ_ASSERT(mState == WaitingForPut);

    ErrorResult result;
    nsCOMPtr<nsIInputStream> body;
    result = NS_NewCStringInputStream(getter_AddRefs(body),
                                      NS_ConvertUTF16toUTF8(mCN->Buffer()));
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    RefPtr<InternalResponse> ir =
      new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    ir->SetBody(body, mCN->Buffer().Length());

    ir->InitChannelInfo(mCN->GetChannelInfo());
    UniquePtr<PrincipalInfo> principalInfo = mCN->TakePrincipalInfo();
    if (principalInfo) {
      ir->SetPrincipalInfo(Move(principalInfo));
    }

    IgnoredErrorResult ignored;
    RefPtr<InternalHeaders> internalHeaders = mCN->GetInternalHeaders();
    ir->Headers()->Fill(*(internalHeaders.get()), ignored);

    RefPtr<Response> response = new Response(aCache->GetGlobalObject(), ir);

    RequestOrUSVString request;
    request.SetAsUSVString().Rebind(URL().Data(), URL().Length());

    // For now we have to wait until the Put Promise is fulfilled before we can
    // continue since Cache does not yet support starting a read that is being
    // written to.
    RefPtr<Promise> cachePromise = aCache->Put(request, *response, result);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    cachePromise->AppendNativeHandler(this);
  }

  RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<CompareCallback> mCallback;
  JS::PersistentRooted<JSObject*> mSandbox;
  RefPtr<CacheStorage> mCacheStorage;

  RefPtr<CompareNetwork> mCN;

  nsString mURL;
  RefPtr<nsIPrincipal> mPrincipal;
  RefPtr<nsILoadGroup> mLoadGroup;

  // Used for the old cache where saves the old source scripts.
  nsString mOldCacheName;
  RefPtr<Cache> mOldCache;

  // Only used if the network script has changed and needs to be cached.
  nsString mNewCacheName;

  nsCString mMaxScope;

  enum {
    WaitingForInitialization,
    WaitingForExistingOpen,
    WaitingForExistingKeys,
    WaitingForScriptOrComparisonResult,
    WaitingForOpen,
    WaitingForPut,
    Redundant
  } mState;
};

NS_IMPL_ISUPPORTS0(CompareManager)

nsresult
CompareNetwork::Initialize(nsIPrincipal* aPrincipal,
                           const nsAString& aURL,
                           bool aIsMainScript,
                           nsILoadGroup* aLoadGroup,
                           Cache* const aCache)
{
  MOZ_ASSERT(aPrincipal);
  AssertIsOnMainThread();

  mURL = aURL;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mIsMainScript = aIsMainScript;

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsLoadFlags flags = nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    mManager->GetRegistration();
  flags |= registration->GetLoadFlags();
  if (registration->IsLastUpdateCheckTimeOverOneDay()) {
    flags |= nsIRequest::LOAD_BYPASS_CACHE;
  }

  // Save the load flags for propagating to ServiceWorkerInfo.
  mManager->SaveLoadFlags(flags);

  // Note that because there is no "serviceworker" RequestContext type, we can
  // use the TYPE_INTERNAL_SCRIPT content policy types when loading a service
  // worker.
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     uri, aPrincipal,
                     nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                     nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER,
                     loadGroup,
                     nullptr, // aCallbacks
                     flags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (httpChannel) {
    // Spec says no redirects allowed for SW scripts.
    rv = httpChannel->SetRedirectionLimit(0);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Service-Worker"),
                                       NS_LITERAL_CSTRING("script"),
                                       /* merge */ false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mChannel->AsyncOpen2(loader);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If we do have an existing cache to compare with.
  if (aCache) {
    mCC = new CompareCache(this);
    rv = mCC->Initialize(aPrincipal, aURL, aCache);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mManager->ComparisonFinished(rv);
      return rv;
    }

    mState = WaitingForBothFinished;
    return NS_OK;
  }

  mState = WaitingForNetworkFinished;
  return NS_OK;
}

 void
CompareNetwork::Finished()
{
  MOZ_ASSERT(mState != Redundant);
  mState = Redundant;

  bool same = true;
  nsresult rv = NS_OK;

  // mNetworkResult is prior to mCacheResult, since it's needed for reporting
  // various error to the web contenet.
  if (NS_FAILED(mNetworkResult)) {
    rv = mNetworkResult;
  } else if (mCC && NS_FAILED(mCacheResult)) {
    rv = mCacheResult;
  } else { // Both passed.
    same = mCC &&
           mCC->InCache() &&
           mCC->Buffer().Equals(mBuffer);
  }

  mManager->ComparisonFinished(rv, same);

  mCC = nullptr;
}

void
CompareNetwork::NetworkFinished(nsresult aRv)
{
  MOZ_ASSERT(mState == WaitingForBothFinished ||
             mState == WaitingForNetworkFinished);

  mNetworkResult = aRv;

  if (mState == WaitingForBothFinished) {
    mState = WaitingForCacheFinished;
    return;
  }

  if (mState == WaitingForNetworkFinished) {
    Finished();
    return;
  }
}

void
CompareNetwork::CacheFinished(nsresult aRv)
{
  MOZ_ASSERT(mState == WaitingForBothFinished ||
             mState == WaitingForCacheFinished);

  mCacheResult = aRv;

  if (mState == WaitingForBothFinished) {
    mState = WaitingForNetworkFinished;
    return;
  }

  if (mState == WaitingForCacheFinished) {
    Finished();
    return;
  }
}

void
CompareNetwork::Abort()
{
  AssertIsOnMainThread();

  if (mState != Redundant) {
    mState = Redundant;

    MOZ_ASSERT(mChannel);
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;

    if (mCC) {
      mCC->Abort();
      mCC = nullptr;
    }
  }
}

NS_IMETHODIMP
CompareNetwork::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  AssertIsOnMainThread();

  // If no channel, Abort() has been called.
  if (!mChannel) {
    return NS_OK;
  }

#ifdef DEBUG
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  MOZ_ASSERT(channel == mChannel);
#endif

  MOZ_ASSERT(!mChannelInfo.IsInitialized());
  mChannelInfo.InitFromChannel(mChannel);

  nsresult rv = SetPrincipalInfo(mChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInternalHeaders->FillResponseHeaders(mChannel);

  return NS_OK;
}

nsresult
CompareNetwork::SetPrincipalInfo(nsIChannel* aChannel)
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ASSERTION(ssm, "Should never be null!");

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  UniquePtr<PrincipalInfo> principalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(channelPrincipal, principalInfo.get());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrincipalInfo = Move(principalInfo);
  return NS_OK;
}

NS_IMETHODIMP
CompareNetwork::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                              nsresult aStatusCode)
{
  // Nothing to do here!
  return NS_OK;
}

NS_IMETHODIMP
CompareNetwork::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                                 nsresult aStatus, uint32_t aLen,
                                 const uint8_t* aString)
{
  AssertIsOnMainThread();

  // If no channel, Abort() has been called.
  if (!mChannel) {
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    if (aStatus == NS_ERROR_REDIRECT_LOOP) {
      NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
    } else {
      NetworkFinished(aStatus);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIRequest> request;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NetworkFinished(rv);
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  MOZ_ASSERT(httpChannel, "How come we don't have an HTTP channel?");

  bool requestSucceeded;
  rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NetworkFinished(rv);
    return NS_OK;
  }

  if (NS_WARN_IF(!requestSucceeded)) {
    // Get the stringified numeric status code, not statusText which could be
    // something misleading like OK for a 404.
    uint32_t status = 0;
    Unused << httpChannel->GetResponseStatus(&status); // don't care if this fails, use 0.
    nsAutoString statusAsText;
    statusAsText.AppendInt(status);

    RefPtr<ServiceWorkerRegistrationInfo> registration = mManager->GetRegistration();
    ServiceWorkerManager::LocalizeAndReportToAllClients(
      registration->mScope, "ServiceWorkerRegisterNetworkError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(registration->mScope),
        statusAsText, mManager->URL() });
    NetworkFinished(NS_ERROR_FAILURE);
    return NS_OK;
  }

  if (mIsMainScript) {
    nsAutoCString maxScope;
    // Note: we explicitly don't check for the return value here, because the
    // absence of the header is not an error condition.
    Unused << httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Service-Worker-Allowed"),
        maxScope);

    mManager->SetMaxScope(maxScope);
  }

  bool isFromCache = false;
  nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(httpChannel));
  if (cacheChannel) {
    cacheChannel->IsFromCache(&isFromCache);
  }

  // [9.2 Update]4.13, If response's cache state is not "local",
  // set registration's last update check time to the current time
  if (!isFromCache) {
    RefPtr<ServiceWorkerRegistrationInfo> registration =
      mManager->GetRegistration();
    registration->RefreshLastUpdateCheckTime();
  }

  nsAutoCString mimeType;
  rv = httpChannel->GetContentType(mimeType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // We should only end up here if !mResponseHead in the channel.  If headers
    // were received but no content type was specified, we'll be given
    // UNKNOWN_CONTENT_TYPE "application/x-unknown-content-type" and so fall
    // into the next case with its better error message.
    NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
    return rv;
  }

  if (!mimeType.LowerCaseEqualsLiteral("text/javascript") &&
      !mimeType.LowerCaseEqualsLiteral("application/x-javascript") &&
      !mimeType.LowerCaseEqualsLiteral("application/javascript")) {
    RefPtr<ServiceWorkerRegistrationInfo> registration = mManager->GetRegistration();
    ServiceWorkerManager::LocalizeAndReportToAllClients(
      registration->mScope, "ServiceWorkerRegisterMimeTypeError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(registration->mScope),
        NS_ConvertUTF8toUTF16(mimeType), mManager->URL() });
    NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
    return rv;
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  rv = nsScriptLoader::ConvertToUTF16(httpChannel, aString, aLen,
                                      NS_LITERAL_STRING("UTF-8"), nullptr,
                                      buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NetworkFinished(rv);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  NetworkFinished(NS_OK);
  return NS_OK;
}

nsresult
CompareCache::Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
                         Cache* const aCache)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCache);
  MOZ_ASSERT(mState == WaitingForInitialization);
  AssertIsOnMainThread();

  mURL = aURL;

  RequestOrUSVString request;
  request.SetAsUSVString().Rebind(mURL.Data(), mURL.Length());
  ErrorResult error;
  CacheQueryOptions params;
  RefPtr<Promise> promise = aCache->Match(request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    Finished(error.StealNSResult(), false);
    return error.StealNSResult();
  }

  mState = WaitingForValue;
  promise->AppendNativeHandler(this);
  return NS_OK;
}

void
CompareCache::Finished(nsresult aStatus, bool aInCache)
{
  if (mState != Redundant) {
    mState = Redundant;
    mInCache = aInCache;
    mCN->CacheFinished(aStatus);
  }
}

void
CompareCache::Abort()
{
  AssertIsOnMainThread();

  if (mState != Redundant) {
    mState = Redundant;

    if (mPump) {
      mPump->Cancel(NS_BINDING_ABORTED);
      mPump = nullptr;
    }

    mCN = nullptr;
  }
}

NS_IMETHODIMP
CompareCache::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                               nsresult aStatus, uint32_t aLen,
                               const uint8_t* aString)
{
  AssertIsOnMainThread();

  if (mState == Redundant) {
    return aStatus;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    Finished(aStatus, false);
    return aStatus;
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  nsresult rv = nsScriptLoader::ConvertToUTF16(nullptr, aString, aLen,
                                               NS_LITERAL_STRING("UTF-8"),
                                               nullptr, buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finished(rv, false);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  Finished(NS_OK, true);
  return NS_OK;
}

// This class manages only 1 promise: For the value from the cache.
void
CompareCache::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (mState == Redundant) {
    return;
  }

  MOZ_ASSERT(mState == WaitingForValue);
  ManageValueResult(aCx, aValue);
}

void
CompareCache::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (mState == Redundant) {
    return;
  }

  Finished(NS_ERROR_FAILURE, false);
}

void
CompareCache::ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  // The cache returns undefined if the object is not stored.
  if (aValue.isUndefined()) {
    Finished(NS_OK, false);
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  if (NS_WARN_IF(!obj)) {
    Finished(NS_ERROR_FAILURE, false);
    return;
  }

  Response* response = nullptr;
  nsresult rv = UNWRAP_OBJECT(Response, obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finished(rv, false);
    return;
  }

  MOZ_ASSERT(response->Ok());

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  MOZ_ASSERT(inputStream);

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finished(rv, false);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finished(rv, false);
    return;
  }

  rv = mPump->AsyncRead(loader, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPump = nullptr;
    Finished(rv, false);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(mPump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPump = nullptr;
      Finished(rv, false);
      return;
    }
  }
}

nsresult
CompareManager::Initialize(nsIPrincipal* aPrincipal,
                           const nsAString& aURL,
                           const nsAString& aCacheName,
                           nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(mState == WaitingForInitialization);

  mURL = aURL;
  mPrincipal = aPrincipal;
  mLoadGroup = aLoadGroup;
  mOldCacheName = aCacheName;

  // Always create a CacheStorage since we want to write the network entry to
  // the cache even if there isn't an existing one.
  AutoJSAPI jsapi;
  jsapi.Init();
  ErrorResult result;
  mSandbox.init(jsapi.cx());
  mCacheStorage = CreateCacheStorage(jsapi.cx(), aPrincipal, result, &mSandbox);
  if (NS_WARN_IF(result.Failed())) {
    MOZ_ASSERT(!result.IsErrorWithMessage());
    Cleanup();
    return result.StealNSResult();
  }

  // Open the cache saving the old source scripts.
  if (!mOldCacheName.IsEmpty()) {
    RefPtr<Promise> promise = mCacheStorage->Open(mOldCacheName, result);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      return result.StealNSResult();
    }

    mState = WaitingForExistingOpen;
    promise->AppendNativeHandler(this);
    return NS_OK;
  }

  // Go fetch the script directly without comparison.
  mState = WaitingForScriptOrComparisonResult;
  FetchScript(mURL, true /* aIsMainScript */, nullptr);
  return NS_OK;
}

// This class manages 4 promises if needed:
// 1. Retrieve the Cache object by a given CacheName of OldCache.
// 2. Retrieve the URLs saved in OldCache.
// 3. Retrieve the Cache object of the NewCache for the newly created SW.
// 4. Put the value in the cache.
// For this reason we have mState to know what callback we are handling.
void
CompareManager::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mCallback);

  if (mState == WaitingForExistingOpen) {
    ManageOldCache(aCx, aValue);
    return;
  }

  if (mState == WaitingForExistingKeys) {
    ManageOldKeys(aCx, aValue);
    return;
  }

  if (mState == WaitingForOpen) {
    ManageNewCache(aCx, aValue);
    return;
  }

  MOZ_ASSERT(mState == WaitingForPut);
  mCallback->ComparisonResult(NS_OK, false /* aIsEqual */,
                              mNewCacheName, mMaxScope);
  Cleanup();
}

void
CompareManager::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  if (mState == WaitingForExistingKeys) {
    NS_WARNING("Could not get the existing URLs.");
  } else if (mState == WaitingForExistingKeys){
    NS_WARNING("Could not get the existing URLs.");
  } else if (mState == WaitingForOpen) {
    NS_WARNING("Could not open cache.");
  } else {
    NS_WARNING("Could not write to cache.");
  }
  Fail(NS_ERROR_FAILURE);
}

void
CompareManager::Fail(nsresult aStatus)
{
  AssertIsOnMainThread();
  mCallback->ComparisonResult(aStatus, false /* aIsEqual */,
                              EmptyString(), EmptyCString());
  Cleanup();
}

void
CompareManager::Cleanup()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mCallback);
  mCallback = nullptr;
  mCN = nullptr;

  mState = Redundant;
}

} // namespace

nsresult
PurgeCache(nsIPrincipal* aPrincipal, const nsAString& aCacheName)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (aCacheName.IsEmpty()) {
    return NS_OK;
  }

  AutoJSAPI jsapi;
  jsapi.Init();
  ErrorResult rv;
  JS::Rooted<JSObject*> sandboxObject(jsapi.cx());
  RefPtr<CacheStorage> cacheStorage = CreateCacheStorage(jsapi.cx(), aPrincipal, rv, &sandboxObject);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // We use the ServiceWorker scope as key for the cacheStorage.
  RefPtr<Promise> promise =
    cacheStorage->Delete(aCacheName, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // We don't actually care about the result of the delete operation.
  return NS_OK;
}

nsresult
GenerateCacheName(nsAString& aName)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsID id;
  rv = uuidGenerator->GenerateUUIDInPlace(&id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);

  // NSID_LENGTH counts the null terminator.
  aName.AssignASCII(chars, NSID_LENGTH - 1);

  return NS_OK;
}

nsresult
Compare(ServiceWorkerRegistrationInfo* aRegistration,
        nsIPrincipal* aPrincipal, const nsAString& aCacheName,
        const nsAString& aURL, CompareCallback* aCallback,
        nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRegistration);
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aURL.IsEmpty());
  MOZ_ASSERT(aCallback);

  RefPtr<CompareManager> cm = new CompareManager(aRegistration, aCallback);

  nsresult rv = cm->Initialize(aPrincipal, aURL, aCacheName, aLoadGroup);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace serviceWorkerScriptCache

END_WORKERS_NAMESPACE
