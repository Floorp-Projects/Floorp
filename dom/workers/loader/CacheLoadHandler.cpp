/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLoadHandler.h"
#include "ScriptResponseHeaderProcessor.h"  // ScriptResponseHeaderProcessor

#include "nsIPrincipal.h"

#include "nsIThreadRetargetableRequest.h"
#include "nsIXPConnect.h"

#include "jsapi.h"
#include "nsNetUtil.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/ServiceWorkerBinding.h"  // ServiceWorkerState
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerScope.h"

#include "mozilla/dom/workerinternals/ScriptLoader.h"  // WorkerScriptLoader

namespace mozilla {
namespace dom {

namespace workerinternals::loader {

NS_IMPL_ISUPPORTS0(CacheCreator)

NS_IMPL_ISUPPORTS(CacheLoadHandler, nsIStreamLoaderObserver)

NS_IMPL_ISUPPORTS0(CachePromiseHandler)

void CachePromiseHandler::ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) {
  AssertIsOnMainThread();
  // May already have been canceled by CacheLoadHandler::Fail from
  // CancelMainThread.
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
             mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
  MOZ_ASSERT_IF(mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel,
                !mLoadInfo.mCachePromise);

  if (mLoadInfo.mCachePromise) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    mLoadInfo.mCachePromise = nullptr;
    mLoader->MaybeExecuteFinishedScripts(mLoadInfo);
  }
}

void CachePromiseHandler::RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) {
  AssertIsOnMainThread();
  // May already have been canceled by CacheLoadHandler::Fail from
  // CancelMainThread.
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
             mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

  mLoadInfo.mCachePromise = nullptr;

  // This will delete the cache object and will call LoadingFinished() with an
  // error for each ongoing operation.
  mLoader->DeleteCache();
}

CacheCreator::CacheCreator(WorkerPrivate* aWorkerPrivate)
    : mCacheName(aWorkerPrivate->ServiceWorkerCacheName()),
      mOriginAttributes(aWorkerPrivate->GetOriginAttributes()) {
  MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  AssertIsOnMainThread();
}

nsresult CacheCreator::CreateCacheStorage(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mCacheStorage);
  MOZ_ASSERT(aPrincipal);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  nsresult rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The JSContext is not in a realm, so CreateSandbox returned an unwrapped
  // global.
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));

  mSandboxGlobalObject = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!mSandboxGlobalObject)) {
    return NS_ERROR_FAILURE;
  }

  // If we're in private browsing mode, don't even try to create the
  // CacheStorage.  Instead, just fail immediately to terminate the
  // ServiceWorker load.
  if (NS_WARN_IF(mOriginAttributes.mPrivateBrowsingId > 0)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Create a CacheStorage bypassing its trusted origin checks.  The
  // ServiceWorker has already performed its own checks before getting
  // to this point.
  ErrorResult error;
  mCacheStorage = CacheStorage::CreateOnMainThread(
      mozilla::dom::cache::CHROME_ONLY_NAMESPACE, mSandboxGlobalObject,
      aPrincipal, true /* force trusted origin */, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

nsresult CacheCreator::Load(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mLoaders.IsEmpty());

  nsresult rv = CreateCacheStorage(aPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult error;
  MOZ_ASSERT(!mCacheName.IsEmpty());
  RefPtr<Promise> promise = mCacheStorage->Open(mCacheName, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  promise->AppendNativeHandler(this);
  return NS_OK;
}

void CacheCreator::FailLoaders(nsresult aRv) {
  AssertIsOnMainThread();

  // Fail() can call LoadingFinished() which may call ExecuteFinishedScripts()
  // which sets mCacheCreator to null, so hold a ref.
  RefPtr<CacheCreator> kungfuDeathGrip = this;

  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    mLoaders[i]->Fail(aRv);
  }

  mLoaders.Clear();
}

void CacheCreator::RejectedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue,
                                    ErrorResult& aRv) {
  AssertIsOnMainThread();
  FailLoaders(NS_ERROR_FAILURE);
}

void CacheCreator::ResolvedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue,
                                    ErrorResult& aRv) {
  AssertIsOnMainThread();

  if (!aValue.isObject()) {
    FailLoaders(NS_ERROR_FAILURE);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  Cache* cache = nullptr;
  nsresult rv = UNWRAP_OBJECT(Cache, &obj, cache);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailLoaders(NS_ERROR_FAILURE);
    return;
  }

  mCache = cache;
  MOZ_DIAGNOSTIC_ASSERT(mCache);

  // If the worker is canceled, CancelMainThread() will have cleared the
  // loaders via DeleteCache().
  for (uint32_t i = 0, len = mLoaders.Length(); i < len; ++i) {
    mLoaders[i]->Load(cache);
  }
}

void CacheCreator::DeleteCache() {
  AssertIsOnMainThread();

  // This is called when the load is canceled which can occur before
  // mCacheStorage is initialized.
  if (mCacheStorage) {
    // It's safe to do this while Cache::Match() and Cache::Put() calls are
    // running.
    RefPtr<Promise> promise = mCacheStorage->Delete(mCacheName, IgnoreErrors());

    // We don't care to know the result of the promise object.
  }

  // Always call this here to ensure the loaders array is cleared.
  FailLoaders(NS_ERROR_FAILURE);
}

CacheLoadHandler::CacheLoadHandler(WorkerPrivate* aWorkerPrivate,
                                   ScriptLoadInfo& aLoadInfo,
                                   bool aIsWorkerScript,
                                   WorkerScriptLoader* aLoader)
    : mLoadInfo(aLoadInfo),
      mLoader(aLoader),
      mWorkerPrivate(aWorkerPrivate),
      mIsWorkerScript(aIsWorkerScript),
      mFailed(false),
      mState(aWorkerPrivate->GetServiceWorkerDescriptor().State()) {
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
  mMainThreadEventTarget = aWorkerPrivate->MainThreadEventTarget();
  MOZ_ASSERT(mMainThreadEventTarget);
  mBaseURI = mLoader->GetBaseURI();
  AssertIsOnMainThread();
}

void CacheLoadHandler::Fail(nsresult aRv) {
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_FAILED(aRv));

  if (mFailed) {
    return;
  }

  mFailed = true;

  if (mPump) {
    MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache);
    mPump->Cancel(aRv);
    mPump = nullptr;
  }

  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cancel;

  // Stop if the load was aborted on the main thread.
  // Can't use Finished() because mCachePromise may still be true.
  if (mLoadInfo.mLoadingFinished) {
    MOZ_ASSERT(!mLoadInfo.mChannel);
    MOZ_ASSERT_IF(mLoadInfo.mCachePromise,
                  mLoadInfo.mCacheStatus == ScriptLoadInfo::WritingToCache ||
                      mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
    return;
  }

  mLoader->LoadingFinished(mLoadInfo, aRv);
}

void CacheLoadHandler::Load(Cache* aCache) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aCache);

  nsCOMPtr<nsIURI> uri;
  nsresult rv =
      NS_NewURI(getter_AddRefs(uri), mLoadInfo.mURL, nullptr, mBaseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  MOZ_ASSERT(mLoadInfo.mFullURL.IsEmpty());
  CopyUTF8toUTF16(spec, mLoadInfo.mFullURL);

  mozilla::dom::RequestOrUSVString request;
  request.SetAsUSVString().ShareOrDependUpon(mLoadInfo.mFullURL);

  mozilla::dom::CacheQueryOptions params;

  // This JSContext will not end up executing JS code because here there are
  // no ReadableStreams involved.
  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  RefPtr<Promise> promise = aCache->Match(jsapi.cx(), request, params, error);
  if (NS_WARN_IF(error.Failed())) {
    Fail(error.StealNSResult());
    return;
  }

  promise->AppendNativeHandler(this);
}

void CacheLoadHandler::RejectedCallback(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue,
                                        ErrorResult& aRv) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);
  Fail(NS_ERROR_FAILURE);
}

void CacheLoadHandler::ResolvedCallback(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue,
                                        ErrorResult& aRv) {
  AssertIsOnMainThread();
  // If we have already called 'Fail', we should not proceed.
  if (mFailed) {
    return;
  }

  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Uncached);

  nsresult rv;

  // The ServiceWorkerScriptCache will store data for any scripts it
  // it knows about.  This is always at least the top level script.
  // Depending on if a previous version of the service worker has
  // been installed or not it may also know about importScripts().  We
  // must handle loading and offlining new importScripts() here, however.
  if (aValue.isUndefined()) {
    // If this is the main script or we're not loading a new service worker
    // then this is an error.  This can happen for internal reasons, like
    // storage was probably wiped without removing the service worker
    // registration.  It can also happen for exposed reasons like the
    // service worker script calling importScripts() after install.
    if (NS_WARN_IF(mIsWorkerScript ||
                   (mState != ServiceWorkerState::Parsed &&
                    mState != ServiceWorkerState::Installing))) {
      Fail(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }

    mLoadInfo.mCacheStatus = ScriptLoadInfo::ToBeCached;
    rv = mLoader->LoadScript(mLoadInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Fail(rv);
    }
    return;
  }

  MOZ_ASSERT(aValue.isObject());

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  mozilla::dom::Response* response = nullptr;
  rv = UNWRAP_OBJECT(Response, &obj, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  InternalHeaders* headers = response->GetInternalHeaders();

  headers->Get("content-security-policy"_ns, mCSPHeaderValue, IgnoreErrors());
  headers->Get("content-security-policy-report-only"_ns,
               mCSPReportOnlyHeaderValue, IgnoreErrors());
  headers->Get("referrer-policy"_ns, mReferrerPolicyHeaderValue,
               IgnoreErrors());

  nsAutoCString coepHeader;
  headers->Get("cross-origin-embedder-policy"_ns, coepHeader, IgnoreErrors());

  nsILoadInfo::CrossOriginEmbedderPolicy coep =
      NS_GetCrossOriginEmbedderPolicyFromHeader(coepHeader);

  rv = ScriptResponseHeaderProcessor::ProcessCrossOriginEmbedderPolicyHeader(
      mWorkerPrivate, coep, mLoader->IsMainScript());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  response->GetBody(getter_AddRefs(inputStream));
  mChannelInfo = response->GetChannelInfo();
  const UniquePtr<PrincipalInfo>& pInfo = response->GetPrincipalInfo();
  if (pInfo) {
    mPrincipalInfo = mozilla::MakeUnique<PrincipalInfo>(*pInfo);
  }

  if (!inputStream) {
    mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;
    nsresult rv = DataReceivedFromCache(
        (uint8_t*)"", 0, mChannelInfo, std::move(mPrincipalInfo),
        mCSPHeaderValue, mCSPReportOnlyHeaderValue, mReferrerPolicyHeaderValue);
    mLoader->OnStreamComplete(mLoadInfo, rv);
    return;
  }

  MOZ_ASSERT(!mPump);
  rv = NS_NewInputStreamPump(getter_AddRefs(mPump), inputStream.forget(),
                             0,     /* default segsize */
                             0,     /* default segcount */
                             false, /* default closeWhenDone */
                             mMainThreadEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Fail(rv);
    return;
  }

  rv = mPump->AsyncRead(loader);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mPump = nullptr;
    Fail(rv);
    return;
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(mPump);
  if (rr) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    rv = rr->RetargetDeliveryTo(sts);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the nsIInputStreamPump to a IO thread.");
    }
  }

  mLoadInfo.mCacheStatus = ScriptLoadInfo::ReadingFromCache;
}

NS_IMETHODIMP
CacheLoadHandler::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext, nsresult aStatus,
                                   uint32_t aStringLen,
                                   const uint8_t* aString) {
  AssertIsOnMainThread();

  mPump = nullptr;

  if (NS_FAILED(aStatus)) {
    MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache ||
               mLoadInfo.mCacheStatus == ScriptLoadInfo::Cancel);
    Fail(aStatus);
    return NS_OK;
  }

  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::ReadingFromCache);
  mLoadInfo.mCacheStatus = ScriptLoadInfo::Cached;

  MOZ_ASSERT(mPrincipalInfo);
  nsresult rv = DataReceivedFromCache(
      aString, aStringLen, mChannelInfo, std::move(mPrincipalInfo),
      mCSPHeaderValue, mCSPReportOnlyHeaderValue, mReferrerPolicyHeaderValue);
  return mLoader->OnStreamComplete(mLoadInfo, rv);
}

nsresult CacheLoadHandler::DataReceivedFromCache(
    const uint8_t* aString, uint32_t aStringLen,
    const mozilla::dom::ChannelInfo& aChannelInfo,
    UniquePtr<PrincipalInfo> aPrincipalInfo, const nsACString& aCSPHeaderValue,
    const nsACString& aCSPReportOnlyHeaderValue,
    const nsACString& aReferrerPolicyHeaderValue) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mLoadInfo.mCacheStatus == ScriptLoadInfo::Cached);

  auto responsePrincipalOrErr = PrincipalInfoToPrincipal(*aPrincipalInfo);
  MOZ_DIAGNOSTIC_ASSERT(responsePrincipalOrErr.isOk());

  nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
  if (!principal) {
    WorkerPrivate* parentWorker = mWorkerPrivate->GetParent();
    MOZ_ASSERT(parentWorker, "Must have a parent!");
    principal = parentWorker->GetPrincipal();
  }

  nsCOMPtr<nsIPrincipal> responsePrincipal = responsePrincipalOrErr.unwrap();

  mLoadInfo.mMutedErrorFlag.emplace(!principal->Subsumes(responsePrincipal));

  // May be null.
  Document* parentDoc = mWorkerPrivate->GetDocument();

  MOZ_ASSERT(mLoadInfo.ScriptTextIsNull());

  nsresult rv;
  if (StaticPrefs::dom_worker_script_loader_utf8_parsing_enabled()) {
    mLoadInfo.InitUTF8Script();
    rv = ScriptLoader::ConvertToUTF8(nullptr, aString, aStringLen, u"UTF-8"_ns,
                                     parentDoc, mLoadInfo.mScript.mUTF8,
                                     mLoadInfo.mScriptLength);
  } else {
    mLoadInfo.InitUTF16Script();
    rv = ScriptLoader::ConvertToUTF16(nullptr, aString, aStringLen, u"UTF-8"_ns,
                                      parentDoc, mLoadInfo.mScript.mUTF16,
                                      mLoadInfo.mScriptLength);
  }
  if (NS_SUCCEEDED(rv) && mLoader->IsMainWorkerScript()) {
    nsCOMPtr<nsIURI> finalURI;
    rv = NS_NewURI(getter_AddRefs(finalURI), mLoadInfo.mFullURL);
    if (NS_SUCCEEDED(rv)) {
      mWorkerPrivate->SetBaseURI(finalURI);
    }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    MOZ_DIAGNOSTIC_ASSERT(principal);

    bool equal = false;
    MOZ_ALWAYS_SUCCEEDS(responsePrincipal->Equals(principal, &equal));
    MOZ_DIAGNOSTIC_ASSERT(equal);

    nsCOMPtr<nsIContentSecurityPolicy> csp;
    if (parentDoc) {
      csp = parentDoc->GetCsp();
    }
    MOZ_DIAGNOSTIC_ASSERT(!csp);
#endif

    mWorkerPrivate->InitChannelInfo(aChannelInfo);

    nsILoadGroup* loadGroup = mWorkerPrivate->GetLoadGroup();
    MOZ_DIAGNOSTIC_ASSERT(loadGroup);

    // Override the principal on the WorkerPrivate.  This is only necessary
    // in order to get a principal with exactly the correct URL.  The fetch
    // referrer logic depends on the WorkerPrivate principal having a URL
    // that matches the worker script URL.  If bug 1340694 is ever fixed
    // this can be removed.
    // XXX: force the partitionedPrincipal to be equal to the response one.
    // This is OK for now because we don't want to expose partitionedPrincipal
    // functionality in ServiceWorkers yet.
    rv = mWorkerPrivate->SetPrincipalsAndCSPOnMainThread(
        responsePrincipal, responsePrincipal, loadGroup, nullptr);
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    rv = mWorkerPrivate->SetCSPFromHeaderValues(aCSPHeaderValue,
                                                aCSPReportOnlyHeaderValue);
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    mWorkerPrivate->UpdateReferrerInfoFromHeader(aReferrerPolicyHeaderValue);
  }

  if (NS_SUCCEEDED(rv)) {
    DataReceived();
  }

  return rv;
}

void CacheLoadHandler::DataReceived() {
  if (mLoader->IsMainWorkerScript()) {
    WorkerPrivate* parent = mWorkerPrivate->GetParent();

    if (parent) {
      // XHR Params Allowed
      mWorkerPrivate->SetXHRParamsAllowed(parent->XHRParamsAllowed());

      // Set Eval and ContentSecurityPolicy
      mWorkerPrivate->SetCSP(parent->GetCSP());
      mWorkerPrivate->SetEvalAllowed(parent->IsEvalAllowed());
    }
  }
}

}  // namespace workerinternals::loader

}  // namespace dom
}  // namespace mozilla
