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
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsICacheInfoChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamLoader.h"
#include "nsIThreadRetargetableRequest.h"

#include "nsIInputStreamPump.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
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

// This class downloads a URL from the network and then it calls
// NetworkFinished() in the CompareManager.
class CompareNetwork final : public nsIStreamLoaderObserver,
                             public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIREQUESTOBSERVER

  CompareNetwork(CompareManager* aManager,
                 ServiceWorkerRegistrationInfo* aRegistration)
    : mManager(aManager)
    , mRegistration(aRegistration)
    , mInternalHeaders(new InternalHeaders())
    , mLoadFlags(nsIChannel::LOAD_BYPASS_SERVICE_WORKER)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL, nsILoadGroup* aLoadGroup);

  void
  Finish(nsresult aStatus,
           const nsACString& aMaxScope = EmptyCString(),
           nsLoadFlags nsLoadFlags = nsIChannel::LOAD_BYPASS_SERVICE_WORKER);

  void
  Abort();

  const nsString& Buffer() const
  {
    AssertIsOnMainThread();
    return mBuffer;
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
  }

  nsresult
  SetPrincipalInfo(nsIChannel* aChannel);

  RefPtr<CompareManager> mManager;
  RefPtr<ServiceWorkerRegistrationInfo> mRegistration;

  nsCOMPtr<nsIChannel> mChannel;
  nsString mBuffer;
  nsString mURL;
  ChannelInfo mChannelInfo;
  RefPtr<InternalHeaders> mInternalHeaders;
  UniquePtr<PrincipalInfo> mPrincipalInfo;

  nsCString mMaxScope;
  nsLoadFlags mLoadFlags;
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
    , mState(WaitingForInitialization)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(Cache* const aCache, const nsAString& aURL);

  void
  Finish(nsresult aStatus, bool aInCache);

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

private:
  ~CompareCache()
  {
    AssertIsOnMainThread();
  }

  void
  ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue);

  RefPtr<CompareManager> mManager;
  nsCOMPtr<nsIInputStreamPump> mPump;

  nsString mURL;
  nsString mBuffer;

  enum {
    WaitingForInitialization,
    WaitingForScript,
    Finished,
  } mState;
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
    , mLoadFlags(nsIChannel::LOAD_BYPASS_SERVICE_WORKER)
    , mState(WaitingForInitialization)
    , mNetworkFinished(false)
    , mCacheFinished(false)
    , mInCache(false)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aRegistration);
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             const nsAString& aCacheName, nsILoadGroup* aLoadGroup);

  void
  NetworkFinished(nsresult aStatus,
                  const nsACString& aMaxScope,
                  nsLoadFlags aLoadFlags)
  {
    AssertIsOnMainThread();

    mNetworkFinished = true;
    mMaxScope = aMaxScope;
    mLoadFlags = aLoadFlags;

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

    if (!mCC || !mInCache) {
      ComparisonFinished(NS_OK, false);
      return;
    }

    ComparisonFinished(NS_OK, mCC->Buffer().Equals(mCN->Buffer()));
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
  ComparisonFinished(nsresult aStatus, bool aIsEqual)
  {
    AssertIsOnMainThread();
    if (mState == Finished) {
      return;
    }

    MOZ_ASSERT(mState == WaitingForScriptOrComparisonResult);

    if (NS_WARN_IF(NS_FAILED(aStatus))) {
      Fail(aStatus);
      return;
    }

    if (aIsEqual) {
      MOZ_ASSERT(mCallback);
      mCallback->ComparisonResult(aStatus,
                                  aIsEqual,
                                  EmptyString(),
                                  mMaxScope,
                                  mLoadFlags);
      Cleanup();
      return;
    }

    // Write to Cache so ScriptLoader reads succeed.
    WriteNetworkBufferToNewCache();
  }

private:
  ~CompareManager()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCC);
    MOZ_ASSERT(!mCN);
  }

  void
  Fail(nsresult aStatus);

  void
  Cleanup();

  nsresult
  FetchScript(const nsAString& aURL, Cache* const aCache = nullptr)
  {
    AssertIsOnMainThread();

    MOZ_DIAGNOSTIC_ASSERT(mState == WaitingForInitialization ||
                          mState == WaitingForScriptOrComparisonResult);

    mCN = new CompareNetwork(this, mRegistration);
    nsresult rv = mCN->Initialize(mPrincipal, aURL, mLoadGroup);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (aCache) {
      mCC = new CompareCache(this);
      rv = mCC->Initialize(aCache, aURL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mCN->Abort();
        return rv;
      }
    }

    return rv;
  }

  void
  ManageOldCache(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForExistingOpen);

    // RAII Cleanup when fails.
    nsresult rv = NS_ERROR_FAILURE;
    auto guard = MakeScopeExit([&] {
        Fail(rv);
    });

    if (NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    MOZ_ASSERT(!mOldCache);
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj) ||
        NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Cache, obj, mOldCache)))) {
      return;
    }

    Optional<RequestOrUSVString> request;
    CacheQueryOptions options;
    ErrorResult error;
    RefPtr<Promise> promise = mOldCache->Keys(request, options, error);
    if (NS_WARN_IF(error.Failed())) {
      rv = error.StealNSResult();
      return;
    }

    mState = WaitingForExistingKeys;
    promise->AppendNativeHandler(this);
    guard.release();
  }

  void
  ManageOldKeys(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForExistingKeys);

    // RAII Cleanup when fails.
    nsresult rv = NS_ERROR_FAILURE;
    auto guard = MakeScopeExit([&] {
        Fail(rv);
    });

    if (NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj)) {
      return;
    }

    uint32_t len = 0;
    if (!JS_GetArrayLength(aCx, obj, &len)) {
      return;
    }

    // Fetch and compare the source scripts.
    mState = WaitingForScriptOrComparisonResult;

    for (uint32_t i = 0; i < len; ++i) {
      JS::Rooted<JS::Value> val(aCx);
      if (NS_WARN_IF(!JS_GetElement(aCx, obj, i, &val)) ||
          NS_WARN_IF(!val.isObject())) {
        return;
      }

      Request* request;
      JS::Rooted<JSObject*> requestObj(aCx, &val.toObject());
      if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Request, requestObj, request)))) {
        return;
      };

      nsString URL;
      request->GetUrl(URL);

      // We still fetch only the main script here, where imported scripts will
      // be handled in following patches.
      if (URL == mURL) {
        rv = FetchScript(URL, mOldCache);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }
      }
    }

    guard.release();
  }

  void
  ManageNewCache(JSContext* aCx, JS::Handle<JS::Value> aValue)
  {
    MOZ_ASSERT(mState == WaitingForOpen);

    // RAII Cleanup when fails.
    nsresult rv = NS_ERROR_FAILURE;
    auto guard = MakeScopeExit([&] {
        Fail(rv);
    });

    if (NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    if (NS_WARN_IF(!obj)) {
      return;
    }

    Cache* cache = nullptr;
    rv = UNWRAP_OBJECT(Cache, obj, cache);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // Just to be safe.
    RefPtr<Cache> kungfuDeathGrip = cache;
    WriteToCache(cache);
    guard.release();
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

    mState = WaitingForOpen;
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
    request.SetAsUSVString().Rebind(mURL.Data(), mURL.Length());

    // For now we have to wait until the Put Promise is fulfilled before we can
    // continue since Cache does not yet support starting a read that is being
    // written to.
    RefPtr<Promise> cachePromise = aCache->Put(request, *response, result);
    if (NS_WARN_IF(result.Failed())) {
      MOZ_ASSERT(!result.IsErrorWithMessage());
      Fail(result.StealNSResult());
      return;
    }

    mState = WaitingForPut;
    cachePromise->AppendNativeHandler(this);
  }

  RefPtr<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<CompareCallback> mCallback;
  JS::PersistentRooted<JSObject*> mSandbox;
  RefPtr<CacheStorage> mCacheStorage;

  RefPtr<CompareNetwork> mCN;
  RefPtr<CompareCache> mCC;

  nsString mURL;
  RefPtr<nsIPrincipal> mPrincipal;
  RefPtr<nsILoadGroup> mLoadGroup;

  // Used for the old cache where saves the old source scripts.
  RefPtr<Cache> mOldCache;

  // Only used if the network script has changed and needs to be cached.
  nsString mNewCacheName;

  nsCString mMaxScope;
  nsLoadFlags mLoadFlags;

  enum {
    WaitingForInitialization,
    WaitingForExistingOpen,
    WaitingForExistingKeys,
    WaitingForScriptOrComparisonResult,
    WaitingForOpen,
    WaitingForPut,
    Finished
  } mState;

  bool mNetworkFinished;
  bool mCacheFinished;
  bool mInCache;
};

NS_IMPL_ISUPPORTS0(CompareManager)

nsresult
CompareNetwork::Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL, nsILoadGroup* aLoadGroup)
{
  MOZ_ASSERT(aPrincipal);
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mURL = aURL;

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Update LoadFlags for propagating to ServiceWorkerInfo.
  mLoadFlags |= mRegistration->GetLoadFlags();
  if (mRegistration->IsLastUpdateCheckTimeOverOneDay()) {
    mLoadFlags |= nsIRequest::LOAD_BYPASS_CACHE;
  }

  // Note that because there is no "serviceworker" RequestContext type, we can
  // use the TYPE_INTERNAL_SCRIPT content policy types when loading a service
  // worker.
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     uri, aPrincipal,
                     nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                     nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER,
                     loadGroup,
                     nullptr, // aCallbacks
                     mLoadFlags);
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

  return NS_OK;
}

void
CompareNetwork::Finish(nsresult aStatus,
                         const nsACString& aMaxScope,
                         nsLoadFlags aLoadFlags)
{
  mManager->NetworkFinished(aStatus, aMaxScope, aLoadFlags);
}

void
CompareNetwork::Abort()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mChannel);
  mChannel->Cancel(NS_BINDING_ABORTED);
  mChannel = nullptr;
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
  if (!ssm) {
    return NS_ERROR_FAILURE;
  }

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
      Finish(NS_ERROR_DOM_SECURITY_ERR);
    } else {
      Finish(aStatus);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIRequest> request;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv);
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  MOZ_ASSERT(httpChannel, "How come we don't have an HTTP channel?");

  bool requestSucceeded;
  rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv);
    return NS_OK;
  }

  if (NS_WARN_IF(!requestSucceeded)) {
    // Get the stringified numeric status code, not statusText which could be
    // something misleading like OK for a 404.
    uint32_t status = 0;
    Unused << httpChannel->GetResponseStatus(&status); // don't care if this fails, use 0.
    nsAutoString statusAsText;
    statusAsText.AppendInt(status);

    ServiceWorkerManager::LocalizeAndReportToAllClients(
      mRegistration->mScope, "ServiceWorkerRegisterNetworkError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(mRegistration->mScope),
        statusAsText, mURL });
    Finish(NS_ERROR_FAILURE);
    return NS_OK;
  }

  // Note: we explicitly don't check for the return value here, because the
  // absence of the header is not an error condition.
  Unused << httpChannel->GetResponseHeader(
      NS_LITERAL_CSTRING("Service-Worker-Allowed"),
      mMaxScope);

  bool isFromCache = false;
  nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(httpChannel));
  if (cacheChannel) {
    cacheChannel->IsFromCache(&isFromCache);
  }

  // [9.2 Update]4.13, If response's cache state is not "local",
  // set registration's last update check time to the current time
  if (!isFromCache) {
    mRegistration->RefreshLastUpdateCheckTime();
  }

  nsAutoCString mimeType;
  rv = httpChannel->GetContentType(mimeType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // We should only end up here if !mResponseHead in the channel.  If headers
    // were received but no content type was specified, we'll be given
    // UNKNOWN_CONTENT_TYPE "application/x-unknown-content-type" and so fall
    // into the next case with its better error message.
    Finish(NS_ERROR_DOM_SECURITY_ERR);
    return rv;
  }

  if (!mimeType.LowerCaseEqualsLiteral("text/javascript") &&
      !mimeType.LowerCaseEqualsLiteral("application/x-javascript") &&
      !mimeType.LowerCaseEqualsLiteral("application/javascript")) {
    ServiceWorkerManager::LocalizeAndReportToAllClients(
      mRegistration->mScope, "ServiceWorkerRegisterMimeTypeError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(mRegistration->mScope),
        NS_ConvertUTF8toUTF16(mimeType), mURL });
    Finish(NS_ERROR_DOM_SECURITY_ERR);
    return rv;
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  rv = ScriptLoader::ConvertToUTF16(httpChannel, aString, aLen,
                                    NS_LITERAL_STRING("UTF-8"), nullptr,
                                    buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  Finish(NS_OK);
  return NS_OK;
}

nsresult
CompareCache::Initialize(Cache* const aCache, const nsAString& aURL)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aCache);
  MOZ_ASSERT(mState == WaitingForInitialization);

  RequestOrUSVString request;
  request.SetAsUSVString().Rebind(aURL.Data(), aURL.Length());
  ErrorResult error;
  CacheQueryOptions params;
  RefPtr<Promise> promise = aCache->Match(request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    mState = Finished;
    return error.StealNSResult();
  }

  // Retrieve the script from aCache.
  mState = WaitingForScript;
  promise->AppendNativeHandler(this);
  return NS_OK;
}

void
CompareCache::Finish(nsresult aStatus, bool aInCache)
{
  mManager->CacheFinished(aStatus, aInCache);
}

void
CompareCache::Abort()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mState != Finished);
  mState = Finished;

  if (mPump) {
    mPump->Cancel(NS_BINDING_ABORTED);
    mPump = nullptr;
  }
}

NS_IMETHODIMP
CompareCache::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                               nsresult aStatus, uint32_t aLen,
                               const uint8_t* aString)
{
  AssertIsOnMainThread();

  if (mState == Finished) {
    return aStatus;
  }

  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    Finish(aStatus, false);
    return aStatus;
  }

  char16_t* buffer = nullptr;
  size_t len = 0;

  nsresult rv = ScriptLoader::ConvertToUTF16(nullptr, aString, aLen,
                                             NS_LITERAL_STRING("UTF-8"),
                                             nullptr, buffer, len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv, false);
    return rv;
  }

  mBuffer.Adopt(buffer, len);

  Finish(NS_OK, true);
  return NS_OK;
}

void
CompareCache::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  switch (mState) {
    case Finished:
      return;
    case WaitingForScript:
      ManageValueResult(aCx, aValue);
      return;
    default:
      MOZ_CRASH("Unacceptable state.");
  }
}

void
CompareCache::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  if (mState != Finished) {
    Finish(NS_ERROR_FAILURE, false);
    return;
  }
}

void
CompareCache::ManageValueResult(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();

  // The cache returns undefined if the object is not stored.
  if (aValue.isUndefined()) {
    Finish(NS_OK, false);
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  if (NS_WARN_IF(!obj)) {
    Finish(NS_ERROR_FAILURE, false);
    return;
  }

  Response* response = nullptr;
  nsresult rv = UNWRAP_OBJECT(Response, obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv, false);
    return;
  }

  MOZ_ASSERT(response->Ok());

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  MOZ_ASSERT(inputStream);

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv, false);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv, false);
    return;
  }

  rv = mPump->AsyncRead(loader, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPump = nullptr;
    Finish(rv, false);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(mPump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPump = nullptr;
      Finish(rv, false);
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

  // RAII Cleanup when fails.
  auto guard = MakeScopeExit([&] { Cleanup(); });

  mURL = aURL;
  mPrincipal = aPrincipal;
  mLoadGroup = aLoadGroup;

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

  // If there is no existing cache, proceed to fetch the script directly.
  if (aCacheName.IsEmpty()) {
    mState = WaitingForScriptOrComparisonResult;
    nsresult rv = FetchScript(aURL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    guard.release();
    return NS_OK;
  }


  // Open the cache saving the old source scripts.
  RefPtr<Promise> promise = mCacheStorage->Open(aCacheName, result);
  if (NS_WARN_IF(result.Failed())) {
    MOZ_ASSERT(!result.IsErrorWithMessage());
    return result.StealNSResult();
  }

  mState = WaitingForExistingOpen;
  promise->AppendNativeHandler(this);

  guard.release();
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

  switch (mState) {
    case Finished:
      return;
    case WaitingForExistingOpen:
      ManageOldCache(aCx, aValue);
      return;
    case WaitingForExistingKeys:
      ManageOldKeys(aCx, aValue);
      return;
    case WaitingForOpen:
      ManageNewCache(aCx, aValue);
      return;
    case WaitingForPut:
      mCallback->ComparisonResult(NS_OK, false /* aIsEqual */,
                                  mNewCacheName, mMaxScope, mLoadFlags);
      Cleanup();
      return;
    default:
      MOZ_CRASH("Unacceptable state.");
  }
}

void
CompareManager::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AssertIsOnMainThread();
  switch (mState) {
    case Finished:
      return;
    case WaitingForExistingOpen:
      NS_WARNING("Could not open the existing cache.");
      break;
    case WaitingForExistingKeys:
      NS_WARNING("Could not get the existing URLs.");
      break;
    case WaitingForOpen:
      NS_WARNING("Could not open cache.");
      break;
    case WaitingForPut:
      NS_WARNING("Could not write to cache.");
      break;
    default:
      MOZ_CRASH("Unacceptable state.");
  }

  Fail(NS_ERROR_FAILURE);
}

void
CompareManager::Fail(nsresult aStatus)
{
  AssertIsOnMainThread();
  mCallback->ComparisonResult(aStatus, false /* aIsEqual */,
                              EmptyString(), EmptyCString(), mLoadFlags);
  Cleanup();
}

void
CompareManager::Cleanup()
{
  AssertIsOnMainThread();

  if (mState != Finished) {
    mState = Finished;

    MOZ_ASSERT(mCallback);
    mCallback = nullptr;

    mCN = nullptr;
    mCC = nullptr;
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
