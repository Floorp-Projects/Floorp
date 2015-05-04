/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerScriptCache.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/cache/Cache.h"
#include "nsIThreadRetargetableRequest.h"

#include "nsIPrincipal.h"
#include "Workers.h"

using mozilla::dom::cache::Cache;
using mozilla::dom::cache::CacheStorage;

BEGIN_WORKERS_NAMESPACE

namespace serviceWorkerScriptCache {

namespace {

already_AddRefed<CacheStorage>
CreateCacheStorage(nsIPrincipal* aPrincipal, ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");

  AutoJSAPI jsapi;
  jsapi.Init();
  nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
  aRv = xpc->CreateSandbox(jsapi.cx(), aPrincipal, getter_AddRefs(sandbox));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> sandboxGlobalObject =
    xpc::NativeGlobal(sandbox->GetJSObject());
  if (!sandboxGlobalObject) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return CacheStorage::CreateOnMainThread(cache::CHROME_ONLY_NAMESPACE,
                                          sandboxGlobalObject,
                                          aPrincipal, aRv);
}

class CompareManager;

// This class downloads a URL from the network and then it calls
// NetworkFinished() in the CompareManager.
class CompareNetwork final : public nsIStreamLoaderObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  explicit CompareNetwork(CompareManager* aManager)
    : mManager(aManager)
  {
    MOZ_ASSERT(aManager);
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL)
  {
    MOZ_ASSERT(aPrincipal);
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       uri, aPrincipal,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_SCRIPT); // FIXME(nsm): TYPE_SERVICEWORKER
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

    // Don't let serviceworker intercept.
    nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(mChannel);
    if (internalChannel) {
      internalChannel->ForceNoIntercept();
    }

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
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

NS_IMPL_ISUPPORTS(CompareNetwork, nsIStreamLoaderObserver)

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
             const nsAString& aCacheName)
  {
    MOZ_ASSERT(aPrincipal);
    AssertIsOnMainThread();

    mURL = aURL;

    ErrorResult rv;
    nsRefPtr<CacheStorage> cacheStorage = CreateCacheStorage(aPrincipal, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    nsRefPtr<Promise> promise = cacheStorage->Open(aCacheName, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    promise->AppendNativeHandler(this);
    return NS_OK;
  }

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

class CompareManager final
{
public:
  NS_INLINE_DECL_REFCOUNTING(CompareManager)

  explicit CompareManager(CompareCallback* aCallback)
    : mCallback(aCallback)
    , mNetworkFinished(false)
    , mCacheFinished(false)
    , mInCache(false)
  {
    AssertIsOnMainThread();
  }

  nsresult
  Initialize(nsIPrincipal* aPrincipal, const nsAString& aURL,
             const nsAString& aCacheName)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aPrincipal);

    mCN = new CompareNetwork(this);
    nsresult rv = mCN->Initialize(aPrincipal, aURL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!aCacheName.IsEmpty()) {
      mCC = new CompareCache(this);
      mCC->Initialize(aPrincipal, aURL, aCacheName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mCN->Abort();
        return rv;
      }
    }

    return NS_OK;
  }

  void
  NetworkFinished(nsresult aStatus)
  {
    AssertIsOnMainThread();

    mNetworkFinished = true;

    if (NS_FAILED(aStatus)) {
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

    if (NS_FAILED(aStatus)) {
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

private:
  ~CompareManager()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mCC);
    MOZ_ASSERT(!mCN);
  }

  void
  ComparisonFinished(nsresult aStatus, bool aIsEqual)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mCallback);

    mCallback->ComparisonResult(aStatus, aIsEqual);
    mCallback = nullptr;
    mCN = nullptr;
    mCC = nullptr;
  }

  nsRefPtr<CompareCallback> mCallback;

  nsRefPtr<CompareNetwork> mCN;
  nsRefPtr<CompareCache> mCC;

  bool mNetworkFinished;
  bool mCacheFinished;
  bool mInCache;
};

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
    mManager->NetworkFinished(aStatus);
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

    if (!requestSucceeded) {
      mManager->NetworkFinished(NS_ERROR_FAILURE);
      return NS_OK;
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

    if (!scheme.LowerCaseEqualsLiteral("app")) {
      mManager->NetworkFinished(NS_ERROR_FAILURE);
      return NS_OK;      
    }
  }

  // FIXME(nsm): "Extract mime type..."

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

} // anonymous namespace

nsresult
PurgeCache(nsIPrincipal* aPrincipal, const nsAString& aCacheName)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (aCacheName.IsEmpty()) {
    return NS_OK;
  }

  ErrorResult rv;
  nsRefPtr<CacheStorage> cacheStorage = CreateCacheStorage(aPrincipal, rv);
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
        const nsAString& aURL, CompareCallback* aCallback)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(!aURL.IsEmpty());
  MOZ_ASSERT(aCallback);

  nsRefPtr<CompareManager> cm = new CompareManager(aCallback);

  nsresult rv = cm->Initialize(aPrincipal, aURL, aCacheName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // serviceWorkerScriptCache namespace

END_WORKERS_NAMESPACE
