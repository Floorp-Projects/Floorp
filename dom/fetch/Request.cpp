/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Request.h"

#include "js/Value.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/URL.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/ReadableStreamDefaultReader.h"

namespace mozilla::dom {

NS_IMPL_ADDREF_INHERITED(Request, FetchBody<Request>)
NS_IMPL_RELEASE_INHERITED(Request, FetchBody<Request>)

// Can't use _INHERITED macro here because FetchBody<Request> is abstract.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Request)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Request, FetchBody<Request>)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadableStreamBody)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadableStreamReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHeaders)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSignal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Request, FetchBody<Request>)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadableStreamBody)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadableStreamReader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHeaders)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSignal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Request)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(FetchBody<Request>)

Request::Request(nsIGlobalObject* aOwner, SafeRefPtr<InternalRequest> aRequest,
                 AbortSignal* aSignal)
    : FetchBody<Request>(aOwner), mRequest(std::move(aRequest)) {
  MOZ_ASSERT(mRequest->Headers()->Guard() == HeadersGuardEnum::Immutable ||
             mRequest->Headers()->Guard() == HeadersGuardEnum::Request ||
             mRequest->Headers()->Guard() == HeadersGuardEnum::Request_no_cors);
  if (aSignal) {
    // If we don't have a signal as argument, we will create it when required by
    // content, otherwise the Request's signal must follow what has been passed.
    JS::Rooted<JS::Value> reason(RootingCx(), aSignal->RawReason());
    mSignal = new AbortSignal(aOwner, aSignal->Aborted(), reason);
    if (!mSignal->Aborted()) {
      mSignal->Follow(aSignal);
    }
  }
}

Request::~Request() = default;

SafeRefPtr<InternalRequest> Request::GetInternalRequest() {
  return mRequest.clonePtr();
}

namespace {
already_AddRefed<nsIURI> ParseURLFromDocument(Document* aDocument,
                                              const nsAString& aInput,
                                              ErrorResult& aRv) {
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(NS_IsMainThread());

  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString input;
  if (!AppendUTF16toUTF8(aInput, input, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIURI> resolvedURI;
  nsresult rv = NS_NewURI(getter_AddRefs(resolvedURI), input, nullptr,
                          aDocument->GetBaseURI());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(input);
  }
  return resolvedURI.forget();
}
void GetRequestURLFromDocument(Document* aDocument, const nsAString& aInput,
                               nsAString& aRequestURL, nsACString& aURLfragment,
                               ErrorResult& aRv) {
  nsCOMPtr<nsIURI> resolvedURI = ParseURLFromDocument(aDocument, aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  // This fails with URIs with weird protocols, even when they are valid,
  // so we ignore the failure
  nsAutoCString credentials;
  Unused << resolvedURI->GetUserPass(credentials);
  if (!credentials.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(NS_ConvertUTF16toUTF8(aInput));
    return;
  }

  nsCOMPtr<nsIURI> resolvedURIClone;
  aRv = NS_GetURIWithoutRef(resolvedURI, getter_AddRefs(resolvedURIClone));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  nsAutoCString spec;
  aRv = resolvedURIClone->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  CopyUTF8toUTF16(spec, aRequestURL);

  // Get the fragment from nsIURI.
  aRv = resolvedURI->GetRef(aURLfragment);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}
already_AddRefed<nsIURI> ParseURLFromChrome(const nsAString& aInput,
                                            ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString input;
  if (!AppendUTF16toUTF8(aInput, input, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), input);
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(input);
  }
  return uri.forget();
}
void GetRequestURLFromChrome(const nsAString& aInput, nsAString& aRequestURL,
                             nsACString& aURLfragment, ErrorResult& aRv) {
  nsCOMPtr<nsIURI> uri = ParseURLFromChrome(aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  // This fails with URIs with weird protocols, even when they are valid,
  // so we ignore the failure
  nsAutoCString credentials;
  Unused << uri->GetUserPass(credentials);
  if (!credentials.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(NS_ConvertUTF16toUTF8(aInput));
    return;
  }

  nsCOMPtr<nsIURI> uriClone;
  aRv = NS_GetURIWithoutRef(uri, getter_AddRefs(uriClone));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  nsAutoCString spec;
  aRv = uriClone->GetSpec(spec);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  CopyUTF8toUTF16(spec, aRequestURL);

  // Get the fragment from nsIURI.
  aRv = uri->GetRef(aURLfragment);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}
already_AddRefed<URL> ParseURLFromWorker(nsIGlobalObject* aGlobal,
                                         const nsAString& aInput,
                                         ErrorResult& aRv) {
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  NS_ConvertUTF8toUTF16 baseURL(worker->GetLocationInfo().mHref);
  RefPtr<URL> url = URL::Constructor(aGlobal, aInput, baseURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(NS_ConvertUTF16toUTF8(aInput));
  }
  return url.forget();
}
void GetRequestURLFromWorker(nsIGlobalObject* aGlobal, const nsAString& aInput,
                             nsAString& aRequestURL, nsACString& aURLfragment,
                             ErrorResult& aRv) {
  RefPtr<URL> url = ParseURLFromWorker(aGlobal, aInput, aRv);
  if (aRv.Failed()) {
    return;
  }
  nsString username;
  url->GetUsername(username);

  nsString password;
  url->GetPassword(password);

  if (!username.IsEmpty() || !password.IsEmpty()) {
    aRv.ThrowTypeError<MSG_URL_HAS_CREDENTIALS>(NS_ConvertUTF16toUTF8(aInput));
    return;
  }

  // Get the fragment from URL.
  nsAutoString fragment;
  url->GetHash(fragment);

  // Note: URL::GetHash() includes the "#" and we want the fragment with out
  // the hash symbol.
  if (!fragment.IsEmpty()) {
    CopyUTF16toUTF8(Substring(fragment, 1), aURLfragment);
  }

  url->SetHash(u""_ns);
  url->GetHref(aRequestURL);
}

class ReferrerSameOriginChecker final : public WorkerMainThreadRunnable {
 public:
  ReferrerSameOriginChecker(WorkerPrivate* aWorkerPrivate,
                            const nsAString& aReferrerURL, nsresult& aResult)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "Fetch :: Referrer same origin check"_ns),
        mReferrerURL(aReferrerURL),
        mResult(aResult) {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), mReferrerURL))) {
      nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();
      if (principal) {
        mResult = principal->CheckMayLoad(uri,
                                          /* allowIfInheritsPrincipal */ false);
      }
    }
    return true;
  }

 private:
  const nsString mReferrerURL;
  nsresult& mResult;
};

}  // namespace

/*static*/
SafeRefPtr<Request> Request::Constructor(const GlobalObject& aGlobal,
                                         const RequestOrUSVString& aInput,
                                         const RequestInit& aInit,
                                         ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(global, aGlobal.Context(), aInput, aInit, aRv);
}

/*static*/
SafeRefPtr<Request> Request::Constructor(nsIGlobalObject* aGlobal,
                                         JSContext* aCx,
                                         const RequestOrUSVString& aInput,
                                         const RequestInit& aInit,
                                         ErrorResult& aRv) {
  bool hasCopiedBody = false;
  SafeRefPtr<InternalRequest> request;

  RefPtr<AbortSignal> signal;

  if (aInput.IsRequest()) {
    RefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    bool used = inputReq->GetBodyUsed(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    if (used) {
      aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
      return nullptr;
    }

    // The body will be copied when GetRequestConstructorCopy() is executed.
    if (body) {
      hasCopiedBody = true;
    }

    request = inputReq->GetInternalRequest();
    signal = inputReq->GetOrCreateSignal();
  } else {
    // aInput is USVString.
    // We need to get url before we create a InternalRequest.
    nsAutoString input;
    input.Assign(aInput.GetAsUSVString());
    nsAutoString requestURL;
    nsCString fragment;
    if (NS_IsMainThread()) {
      nsCOMPtr<nsPIDOMWindowInner> inner(do_QueryInterface(aGlobal));
      Document* doc = inner ? inner->GetExtantDoc() : nullptr;
      if (doc) {
        GetRequestURLFromDocument(doc, input, requestURL, fragment, aRv);
      } else {
        // If we don't have a document, we must assume that this is a full URL.
        GetRequestURLFromChrome(input, requestURL, fragment, aRv);
      }
    } else {
      GetRequestURLFromWorker(aGlobal, input, requestURL, fragment, aRv);
    }
    if (aRv.Failed()) {
      return nullptr;
    }
    request = MakeSafeRefPtr<InternalRequest>(NS_ConvertUTF16toUTF8(requestURL),
                                              fragment);
  }
  request = request->GetRequestConstructorCopy(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RequestMode fallbackMode = RequestMode::EndGuard_;
  RequestCredentials fallbackCredentials = RequestCredentials::EndGuard_;
  RequestCache fallbackCache = RequestCache::EndGuard_;
  if (aInput.IsUSVString()) {
    fallbackMode = RequestMode::Cors;
    fallbackCredentials = RequestCredentials::Same_origin;
    fallbackCache = RequestCache::Default;
  }

  RequestMode mode =
      aInit.mMode.WasPassed() ? aInit.mMode.Value() : fallbackMode;
  RequestCredentials credentials = aInit.mCredentials.WasPassed()
                                       ? aInit.mCredentials.Value()
                                       : fallbackCredentials;

  if (mode == RequestMode::Navigate) {
    aRv.ThrowTypeError<MSG_INVALID_REQUEST_MODE>("navigate");
    return nullptr;
  }
  if (aInit.IsAnyMemberPresent() && request->Mode() == RequestMode::Navigate) {
    mode = RequestMode::Same_origin;
  }

  if (aInit.IsAnyMemberPresent()) {
    request->SetReferrer(
        NS_LITERAL_STRING_FROM_CSTRING(kFETCH_CLIENT_REFERRER_STR));
    request->SetReferrerPolicy(ReferrerPolicy::_empty);
  }
  if (aInit.mReferrer.WasPassed()) {
    const nsString& referrer = aInit.mReferrer.Value();
    if (referrer.IsEmpty()) {
      request->SetReferrer(u""_ns);
    } else {
      nsAutoString referrerURL;
      if (NS_IsMainThread()) {
        nsCOMPtr<nsPIDOMWindowInner> inner(do_QueryInterface(aGlobal));
        Document* doc = inner ? inner->GetExtantDoc() : nullptr;
        nsCOMPtr<nsIURI> uri;
        if (doc) {
          uri = ParseURLFromDocument(doc, referrer, aRv);
        } else {
          // If we don't have a document, we must assume that this is a full
          // URL.
          uri = ParseURLFromChrome(referrer, aRv);
        }
        if (NS_WARN_IF(aRv.Failed())) {
          aRv.ThrowTypeError<MSG_INVALID_REFERRER_URL>(
              NS_ConvertUTF16toUTF8(referrer));
          return nullptr;
        }
        nsAutoCString spec;
        uri->GetSpec(spec);
        CopyUTF8toUTF16(spec, referrerURL);
        if (!referrerURL.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
          nsCOMPtr<nsIPrincipal> principal = aGlobal->PrincipalOrNull();
          if (principal) {
            nsresult rv =
                principal->CheckMayLoad(uri,
                                        /* allowIfInheritsPrincipal */ false);
            if (NS_FAILED(rv)) {
              referrerURL.AssignLiteral(kFETCH_CLIENT_REFERRER_STR);
            }
          }
        }
      } else {
        RefPtr<URL> url = ParseURLFromWorker(aGlobal, referrer, aRv);
        if (NS_WARN_IF(aRv.Failed())) {
          aRv.ThrowTypeError<MSG_INVALID_REFERRER_URL>(
              NS_ConvertUTF16toUTF8(referrer));
          return nullptr;
        }
        url->GetHref(referrerURL);
        if (!referrerURL.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
          WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
          nsresult rv = NS_OK;
          // ReferrerSameOriginChecker uses a sync loop to get the main thread
          // to perform the same-origin check.  Overall, on Workers this method
          // can create 3 sync loops (two for constructing URLs and one here) so
          // in the future we may want to optimize it all by off-loading all of
          // this work in a single sync loop.
          RefPtr<ReferrerSameOriginChecker> checker =
              new ReferrerSameOriginChecker(worker, referrerURL, rv);
          IgnoredErrorResult error;
          checker->Dispatch(Canceling, error);
          if (error.Failed() || NS_FAILED(rv)) {
            referrerURL.AssignLiteral(kFETCH_CLIENT_REFERRER_STR);
          }
        }
      }
      request->SetReferrer(referrerURL);
    }
  }

  if (aInit.mReferrerPolicy.WasPassed()) {
    request->SetReferrerPolicy(aInit.mReferrerPolicy.Value());
  }

  if (aInit.mSignal.WasPassed()) {
    signal = aInit.mSignal.Value();
  }

  UniquePtr<mozilla::ipc::PrincipalInfo> principalInfo;
  nsILoadInfo::CrossOriginEmbedderPolicy coep =
      nsILoadInfo::EMBEDDER_POLICY_NULL;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (window) {
      nsCOMPtr<Document> doc;
      doc = window->GetExtantDoc();
      if (doc) {
        request->SetEnvironmentReferrerPolicy(doc->GetReferrerPolicy());

        principalInfo.reset(new mozilla::ipc::PrincipalInfo());
        nsresult rv =
            PrincipalToPrincipalInfo(doc->NodePrincipal(), principalInfo.get());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
          return nullptr;
        }
      }
      if (window->GetWindowContext()) {
        coep = window->GetWindowContext()->GetEmbedderPolicy();
      }
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    if (worker) {
      worker->AssertIsOnWorkerThread();
      request->SetEnvironmentReferrerPolicy(worker->GetReferrerPolicy());
      principalInfo =
          MakeUnique<mozilla::ipc::PrincipalInfo>(worker->GetPrincipalInfo());
      coep = worker->GetEmbedderPolicy();
      // For dedicated worker, the response must respect the owner's COEP.
      if (coep == nsILoadInfo::EMBEDDER_POLICY_NULL &&
          worker->IsDedicatedWorker()) {
        coep = worker->GetOwnerEmbedderPolicy();
      }
    }
  }

  request->SetPrincipalInfo(std::move(principalInfo));
  request->SetEmbedderPolicy(coep);

  if (mode != RequestMode::EndGuard_) {
    request->SetMode(mode);
  }

  if (credentials != RequestCredentials::EndGuard_) {
    request->SetCredentialsMode(credentials);
  }

  RequestCache cache =
      aInit.mCache.WasPassed() ? aInit.mCache.Value() : fallbackCache;
  if (cache != RequestCache::EndGuard_) {
    if (cache == RequestCache::Only_if_cached &&
        request->Mode() != RequestMode::Same_origin) {
      nsCString modeString(RequestModeValues::GetString(request->Mode()));
      aRv.ThrowTypeError<MSG_ONLY_IF_CACHED_WITHOUT_SAME_ORIGIN>(modeString);
      return nullptr;
    }
    request->SetCacheMode(cache);
  }

  if (aInit.mRedirect.WasPassed()) {
    request->SetRedirectMode(aInit.mRedirect.Value());
  }

  if (aInit.mIntegrity.WasPassed()) {
    request->SetIntegrity(aInit.mIntegrity.Value());
  }

  if (aInit.mMozErrors.WasPassed() && aInit.mMozErrors.Value()) {
    request->SetMozErrors();
  }

  // Request constructor step 14.
  if (aInit.mMethod.WasPassed()) {
    nsAutoCString method(aInit.mMethod.Value());

    // Step 14.1. Disallow forbidden methods, and anything that is not a HTTP
    // token, since HTTP states that Method may be any of the defined values or
    // a token (extension method).
    nsAutoCString outMethod;
    nsresult rv = FetchUtil::GetValidRequestMethod(method, outMethod);
    if (NS_FAILED(rv)) {
      aRv.ThrowTypeError<MSG_INVALID_REQUEST_METHOD>(method);
      return nullptr;
    }

    // Step 14.2
    request->SetMethod(outMethod);
  }

  RefPtr<InternalHeaders> requestHeaders = request->Headers();

  RefPtr<InternalHeaders> headers;
  if (aInit.mHeaders.WasPassed()) {
    RefPtr<Headers> h = Headers::Create(aGlobal, aInit.mHeaders.Value(), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    headers = h->GetInternalHeaders();
  } else {
    headers = new InternalHeaders(*requestHeaders);
  }

  requestHeaders->Clear();
  // From "Let r be a new Request object associated with request and a new
  // Headers object whose guard is "request"."
  requestHeaders->SetGuard(HeadersGuardEnum::Request, aRv);
  MOZ_ASSERT(!aRv.Failed());

  if (request->Mode() == RequestMode::No_cors) {
    if (!request->HasSimpleMethod()) {
      nsAutoCString method;
      request->GetMethod(method);
      aRv.ThrowTypeError<MSG_INVALID_REQUEST_METHOD>(method);
      return nullptr;
    }

    requestHeaders->SetGuard(HeadersGuardEnum::Request_no_cors, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  requestHeaders->Fill(*headers, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if ((aInit.mBody.WasPassed() && !aInit.mBody.Value().IsNull()) ||
      hasCopiedBody) {
    // HEAD and GET are not allowed to have a body.
    nsAutoCString method;
    request->GetMethod(method);
    // method is guaranteed to be uppercase due to step 14.2 above.
    if (method.EqualsLiteral("HEAD") || method.EqualsLiteral("GET")) {
      aRv.ThrowTypeError("HEAD or GET Request cannot have a body.");
      return nullptr;
    }
  }

  if (aInit.mBody.WasPassed()) {
    const Nullable<fetch::OwningBodyInit>& bodyInitNullable =
        aInit.mBody.Value();
    if (!bodyInitNullable.IsNull()) {
      const fetch::OwningBodyInit& bodyInit = bodyInitNullable.Value();
      nsCOMPtr<nsIInputStream> stream;
      nsAutoCString contentTypeWithCharset;
      uint64_t contentLength = 0;
      aRv = ExtractByteStreamFromBody(bodyInit, getter_AddRefs(stream),
                                      contentTypeWithCharset, contentLength);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      nsCOMPtr<nsIInputStream> temporaryBody = stream;

      if (!contentTypeWithCharset.IsVoid() &&
          !requestHeaders->Has("Content-Type"_ns, aRv)) {
        requestHeaders->Append("Content-Type"_ns, contentTypeWithCharset, aRv);
      }

      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      if (hasCopiedBody) {
        request->SetBody(nullptr, 0);
      }

      request->SetBody(temporaryBody, contentLength);
    }
  }

  auto domRequest =
      MakeSafeRefPtr<Request>(aGlobal, std::move(request), signal);

  if (aInput.IsRequest()) {
    RefPtr<Request> inputReq = &aInput.GetAsRequest();
    nsCOMPtr<nsIInputStream> body;
    inputReq->GetBody(getter_AddRefs(body));
    if (body) {
      inputReq->SetBody(nullptr, 0);
      inputReq->SetBodyUsed(aCx, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }
    }
  }
  return domRequest;
}

SafeRefPtr<Request> Request::Clone(ErrorResult& aRv) {
  bool used = GetBodyUsed(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  if (used) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return nullptr;
  }

  SafeRefPtr<InternalRequest> ir = mRequest->Clone();
  if (!ir) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return MakeSafeRefPtr<Request>(mOwner, std::move(ir), GetOrCreateSignal());
}

Headers* Request::Headers_() {
  if (!mHeaders) {
    mHeaders = new Headers(mOwner, mRequest->Headers());
  }

  return mHeaders;
}

AbortSignal* Request::GetOrCreateSignal() {
  if (!mSignal) {
    mSignal = new AbortSignal(mOwner, false, JS::UndefinedHandleValue);
  }

  return mSignal;
}

AbortSignalImpl* Request::GetSignalImpl() const { return mSignal; }

AbortSignalImpl* Request::GetSignalImplToConsumeBody() const {
  // This is a hack, see Response::GetSignalImplToConsumeBody.
  return nullptr;
}

}  // namespace mozilla::dom
