/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerScriptCache.h"
#include "mozilla/unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"

#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "Workers.h"
#include "nsStringStream.h"

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;

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

// This class downloads a URL from the network and then it calls
// NetworkFinished() in the CompareManager.
class CompareNetwork final : public nsIStreamLoaderObserver,
                             public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIREQUESTOBSERVER

  explicit CompareNetwork(CompareManager* aManager)
    : mManager(aManager)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL, nsILoadGroup* aLoadGroup)
  {
    MOZ_ASSERT(aPrincipal);
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), aPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Note that because there is no "serviceworker" RequestContext type, we can
    // use the external TYPE_SCRIPT content policy types when loading a service
    // worker.
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       uri, aPrincipal,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_SCRIPT,
                       loadGroup,
                       nullptr, // aCallbacks
                       nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsLoadFlags flags;
    rv = mChannel->GetLoadFlags(&flags);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    flags |= nsIRequest::LOAD_BYPASS_CACHE;
    rv = mChannel->SetLoadFlags(flags);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
    if (httpChannel) {
      // Spec says no redirects allowed for SW scripts.
      httpChannel->SetRedirectionLimit(0);
    }

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this, this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mChannel->AsyncOpen(loader, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  void
  Abort()
  {
    AssertIsOnMainThread();

    MOZ_ASSERT(mChannel);
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nullptr;
  }

  const nsString& Buffer() const
  {
    AssertIsOnMainThread();
    return mBuffer;
  }

private:
  ~CompareNetwork()
  {
    AssertIsOnMainThread();
  }

  nsRefPtr<CompareManager> mManager;
  nsCOMPtr<nsIChannel> mChannel;
  nsString mBuffer;
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

  explicit CompareCache(CompareManager* aManager)
    : mManager(aManager)
    , mState(WaitingForCache)
    , mAborted(false)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             const nsAString& aCacheName);

  void
  Abort()
  {
    AssertIsOnMainThread();

    MOZ_ASSERT(!mAborted);
    mAborted = true;

    if (mPump) {
      mPump->Cancel(NS_BINDING_ABORTED);
      mPump = nullptr;
    }
  }

  // This class manages 2 promises: 1 is to retrieve cache object, and 2 is for
  // the value from the cache. For this reason we have mState to know what
  // reject/resolve callback we are handling.

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    AssertIsOnMainThread();

    if (mAborted) {
      return;
    }

    if (mState == WaitingForCache) {
      ManageCacheResult(aCx, aValue);
      return;
    }

    MOZ_ASSERT(mState == WaitingForValue);
    ManageValueResult(aCx, aValue);
  }

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

private:
  ~CompareCache()
  {
    AssertIsOnMainThread();
  }

  void
  ManageCacheResult(JSContext* aCx, JS::Handle<JS::Value> aValue);

  void
  ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue);

  nsRefPtr<CompareManager> mManager;
  nsCOMPtr<nsIInputStreamPump> mPump;

  nsString mURL;
  nsString mBuffer;

  enum {
    WaitingForCache,
    WaitingForValue
  } mState;

  bool mAborted;
};

NS_IMPL_ISUPPORTS(CompareCache, nsIStreamLoaderObserver)

class CompareManager final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  explicit CompareManager(CompareCallback* aCallback)
    : mCallback(aCallback)
    , mState(WaitingForOpen)
    , mNetworkFinished(false)
    , mCacheFinished(false)
    , mInCache(false)
  {
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             const nsAString& aCacheName, nsILoadGroup* aLoadGroup)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPrincipal);

    mURL = aURL;

    // Always create a CacheStorage since we want to write the network entry to
    // the cache even if there isn't an existing one.
    AutoJSAPI jsapi;
    jsapi.Init();
    ErrorResult result;
    mSandbox.init(jsapi.cx());
    mCacheStorage = CreateCacheStorage(jsapi.cx(), aPrincipal, result, &mSandbox);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      return result.StealNSResult();
    }

    mCN = new CompareNetwork(this);
    nsresult rv = mCN->Initialize(aPrincipal, aURL, aLoadGroup);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!aCacheName.IsEmpty()) {
      mCC = new CompareCache(this);
      rv = mCC->Initialize(aPrincipal, aURL, aCacheName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mCN->Abort();
        return rv;
      }
    }

    return NS_OK;
  }

  const nsAString&
  URL() const
  {
    AssertIsOnMainThread();
    return mURL;
  }

  void
  SetMaxScope(const nsACString& aMaxScope)
  {
    MOZ_ASSERT(!mNetworkFinished);
    mMaxScope = aMaxScope;
  }

  void
  NetworkFinished(nsresult aStatus)
  {
    AssertIsOnMainThread();

    mNetworkFinished = true;

    if (NS_WARN_IF(NS_FAILED(aStatus))) {
      if (mCC) {
        mCC->Abort();
      }

      ComparisonFinished(aStatus, false);
      return;
    }

    MaybeCompare();
  }

  void
  CacheFinished(nsresult aStatus, bool aInCache)
  {
    AssertIsOnMainThread();

    mCacheFinished = true;
    mInCache = aInCache;

    if (NS_WARN_IF(NS_FAILED(aStatus))) {
      if (mCN) {
        mCN->Abort();
      }

      ComparisonFinished(aStatus, false);
      return;
    }

    MaybeCompare();
  }

  void
  MaybeCompare()
  {
    AssertIsOnMainThread();

    if (!mNetworkFinished || (mCC && !mCacheFinished)) {
      return;
    }

    if (NS_WARN_IF(!mCC || !mInCache)) {
      ComparisonFinished(NS_OK, false);
      return;
    }

    ComparisonFinished(NS_OK, mCC->Buffer().Equals(mCN->Buffer()));
  }

  // This class manages 2 promises: 1 is to retrieve Cache object, and 2 is to
  // Put the value in the cache. For this reason we have mState to know what
  // callback we are handling.
  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);

    if (mState == WaitingForOpen) {
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
      nsRefPtr<Cache> kungfuDeathGrip = cache;
      WriteToCache(cache);
      return;
    }

    MOZ_ASSERT(mState == WaitingForPut);
    mCallback->ComparisonResult(NS_OK, false /* aIsEqual */,
                                mNewCacheName, mMaxScope);
    Cleanup();
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    AssertIsOnMainThread();
    if (mState == WaitingForOpen) {
      NS_WARNING("Could not open cache.");
    } else {
      NS_WARNING("Could not write to cache.");
    }
    Fail(NS_ERROR_FAILURE);
  }

  CacheStorage*
  CacheStorage_()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCacheStorage);
    return mCacheStorage;
  }

  void
  InitChannelInfo(nsIChannel* aChannel)
  {
    mChannelInfo.InitFromChannel(aChannel);
  }

  nsresult
  SetPrincipalInfo(nsIChannel* aChannel)
  {
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(ssm, "Should never be null!");

    nsCOMPtr<nsIPrincipal> channelPrincipal;
    nsresult rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(channelPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    UniquePtr<mozilla::ipc::PrincipalInfo> principalInfo(new mozilla::ipc::PrincipalInfo());
    rv = PrincipalToPrincipalInfo(channelPrincipal, principalInfo.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPrincipalInfo = Move(principalInfo);
    return NS_OK;
  }

private:
  ~CompareManager()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCC);
    MOZ_ASSERT(!mCN);
  }

  void
  Fail(nsresult aStatus)
  {
    AssertIsOnMainThread();
    mCallback->ComparisonResult(aStatus, false /* aIsEqual */,
                                EmptyString(), EmptyCString());
    Cleanup();
  }

  void
  Cleanup()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);
    mCallback = nullptr;
    mCN = nullptr;
    mCC = nullptr;
  }

  void
  ComparisonFinished(nsresult aStatus, bool aIsEqual)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);

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
    WriteNetworkBufferToNewCache();
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

    nsRefPtr<Promise> cacheOpenPromise = mCacheStorage->Open(mNewCacheName, result);
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
    MOZ_ASSERT(mState == WaitingForOpen);

    ErrorResult result;
    nsCOMPtr<nsIInputStream> body;
    result = NS_NewStringInputStream(getter_AddRefs(body), mCN->Buffer());
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    nsRefPtr<InternalResponse> ir =
      new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    ir->SetBody(body);

    ir->InitChannelInfo(mChannelInfo);
    if (mPrincipalInfo) {
      ir->SetPrincipalInfo(Move(mPrincipalInfo));
    }

    nsRefPtr<Response> response = new Response(aCache->GetGlobalObject(), ir);

    RequestOrUSVString request;
    request.SetAsUSVString().Rebind(URL().Data(), URL().Length());

    // For now we have to wait until the Put Promise is fulfilled before we can
    // continue since Cache does not yet support starting a read that is being
    // written to.
    nsRefPtr<Promise> cachePromise = aCache->Put(request, *response, result);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    mState = WaitingForPut;
    cachePromise->AppendNativeHandler(this);
  }

  nsRefPtr<CompareCallback> mCallback;
  JS::PersistentRooted<JSObject*> mSandbox;
  nsRefPtr<CacheStorage> mCacheStorage;

  nsRefPtr<CompareNetwork> mCN;
  nsRefPtr<CompareCache> mCC;

  nsString mURL;
  // Only used if the network script has changed and needs to be cached.
  nsString mNewCacheName;

  ChannelInfo mChannelInfo;

  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;

  nsCString mMaxScope;

  enum {
    WaitingForOpen,
    WaitingForPut
  } mState;

  bool mNetworkFinished;
  bool mCacheFinished;
  bool mInCache;
};

NS_IMPL_ISUPPORTS0(CompareManager)

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

  mManager->InitChannelInfo(mChannel);
  nsresult rv = mManager->SetPrincipalInfo(mChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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
      mManager->NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
    } else {
      mManager->NetworkFinished(aStatus);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIRequest> request;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->NetworkFinished(rv);
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mManager->NetworkFinished(rv);
      return NS_OK;
    }

    if (NS_WARN_IF(!requestSucceeded)) {
      mManager->NetworkFinished(NS_ERROR_FAILURE);
      return NS_OK;
    }

    nsAutoCString maxScope;
    // Note: we explicitly don't check for the return value here, because the
    // absence of the header is not an error condition.
    unused << httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Service-Worker-Allowed"),
                                             maxScope);

    mManager->SetMaxScope(maxScope);

    nsAutoCString mimeType;
    rv = httpChannel->GetContentType(mimeType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mManager->NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
      return rv;
    }

    if (!mimeType.LowerCaseEqualsLiteral("text/javascript") &&
        !mimeType.LowerCaseEqualsLiteral("application/x-javascript") &&
        !mimeType.LowerCaseEqualsLiteral("application/javascript")) {
      mManager->NetworkFinished(NS_ERROR_DOM_SECURITY_ERR);
      return rv;
    }
  }
  else {
    // The only supported request schemes are http, https, and app.
    // Above, we check to ensure that the request is http or https
    // based on the channel qi.  Here we test the scheme to ensure
    // that it is app.  Otherwise, bail.
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (NS_WARN_IF(!channel)) {
      mManager->NetworkFinished(NS_ERROR_FAILURE);
      return NS_OK;
    }
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mManager->NetworkFinished(rv);
      return NS_OK;
    }

    nsAutoCString scheme;
    rv = uri->GetScheme(scheme);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mManager->NetworkFinished(rv);
      return NS_OK;
    }

    if (NS_WARN_IF(!scheme.LowerCaseEqualsLiteral("app"))) {
      mManager->NetworkFinished(NS_ERROR_FAILURE);
      return NS_OK;      
    }
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  rv = nsScriptLoader::ConvertToUTF16(httpChannel, aString, aLen,
                                      NS_LITERAL_STRING("UTF-8"), nullptr,
                                      buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->NetworkFinished(rv);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  mManager->NetworkFinished(NS_OK);
  return NS_OK;
}

nsresult
CompareCache::Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
                         const nsAString& aCacheName)
{
  MOZ_ASSERT(aPrincipal);
  AssertIsOnMainThread();

  mURL = aURL;

  ErrorResult rv;

  nsRefPtr<Promise> promise = mManager->CacheStorage_()->Open(aCacheName, rv);
  if (NS_WARN_IF(rv.Failed())) {
    MOZ_ASSERT(!rv.IsErrorWithMessage());
    return rv.StealNSResult();
  }

  promise->AppendNativeHandler(this);
  return NS_OK;
}

NS_IMETHODIMP
CompareCache::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                               nsresult aStatus, uint32_t aLen,
                               const uint8_t* aString)
{
  AssertIsOnMainThread();

  if (mAborted) {
    return aStatus;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    mManager->CacheFinished(aStatus, false);
    return aStatus;
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  nsresult rv = nsScriptLoader::ConvertToUTF16(nullptr, aString, aLen,
                                               NS_LITERAL_STRING("UTF-8"),
                                               nullptr, buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->CacheFinished(rv, false);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  mManager->CacheFinished(NS_OK, true);
  return NS_OK;
}

void
CompareCache::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (mAborted) {
    return;
  }

  mManager->CacheFinished(NS_ERROR_FAILURE, false);
}

void
CompareCache::ManageCacheResult(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (NS_WARN_IF(!aValue.isObject())) {
    mManager->CacheFinished(NS_ERROR_FAILURE, false);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  if (NS_WARN_IF(!obj)) {
    mManager->CacheFinished(NS_ERROR_FAILURE, false);
    return;
  }

  Cache* cache = nullptr;
  nsresult rv = UNWRAP_OBJECT(Cache, obj, cache);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->CacheFinished(rv, false);
    return;
  }

  RequestOrUSVString request;
  request.SetAsUSVString().Rebind(mURL.Data(), mURL.Length());
  ErrorResult error;
  CacheQueryOptions params;
  nsRefPtr<Promise> promise = cache->Match(request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    mManager->CacheFinished(error.StealNSResult(), false);
    return;
  }

  promise->AppendNativeHandler(this);
  mState = WaitingForValue;
}

void
CompareCache::ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  // The cache returns undefined if the object is not stored.
  if (aValue.isUndefined()) {
    mManager->CacheFinished(NS_OK, false);
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  if (NS_WARN_IF(!obj)) {
    mManager->CacheFinished(NS_ERROR_FAILURE, false);
    return;
  }

  Response* response = nullptr;
  nsresult rv = UNWRAP_OBJECT(Response, obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->CacheFinished(rv, false);
    return;
  }

  MOZ_ASSERT(response->Ok());

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  MOZ_ASSERT(inputStream);

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->CacheFinished(rv, false);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mManager->CacheFinished(rv, false);
    return;
  }

  rv = mPump->AsyncRead(loader, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPump = nullptr;
    mManager->CacheFinished(rv, false);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(mPump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPump = nullptr;
      mManager->CacheFinished(rv, false);
      return;
    }
  }
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
  nsRefPtr<CacheStorage> cacheStorage = CreateCacheStorage(jsapi.cx(), aPrincipal, rv, &sandboxObject);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // We use the ServiceWorker scope as key for the cacheStorage.
  nsRefPtr<Promise> promise =
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
  aName.AssignASCII(chars, NSID_LENGTH);

  return NS_OK;
}

nsresult
Compare(nsIPrincipal* aPrincipal, const nsAString& aCacheName,
        const nsAString& aURL, CompareCallback* aCallback,
        nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aURL.IsEmpty());
  MOZ_ASSERT(aCallback);

  nsRefPtr<CompareManager> cm = new CompareManager(aCallback);

  nsresult rv = cm->Initialize(aPrincipal, aURL, aCacheName, aLoadGroup);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace serviceWorkerScriptCache

END_WORKERS_NAMESPACE
