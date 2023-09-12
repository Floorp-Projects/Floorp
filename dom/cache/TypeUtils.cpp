/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/TypeUtils.h"

#include <algorithm>
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/CacheStorageBinding.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsCOMPtr.h"
#include "nsHttp.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsPromiseFlatString.h"
#include "nsQueryObject.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsURLParsers.h"

namespace mozilla::dom::cache {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::PBackgroundChild;

namespace {

static bool HasVaryStar(mozilla::dom::InternalHeaders* aHeaders) {
  nsCString varyHeaders;
  ErrorResult rv;
  aHeaders->Get("vary"_ns, varyHeaders, rv);
  MOZ_ALWAYS_TRUE(!rv.Failed());

  for (const nsACString& header :
       nsCCharSeparatedTokenizer(varyHeaders, NS_HTTP_HEADER_SEP).ToRange()) {
    if (header.EqualsLiteral("*")) {
      return true;
    }
  }
  return false;
}

nsTArray<HeadersEntry> ToHeadersEntryList(InternalHeaders* aHeaders) {
  MOZ_DIAGNOSTIC_ASSERT(aHeaders);

  AutoTArray<InternalHeaders::Entry, 16> entryList;
  aHeaders->GetEntries(entryList);

  nsTArray<HeadersEntry> result;
  result.SetCapacity(entryList.Length());
  std::transform(entryList.cbegin(), entryList.cend(), MakeBackInserter(result),
                 [](const auto& entry) {
                   return HeadersEntry(entry.mName, entry.mValue);
                 });

  return result;
}

}  // namespace

SafeRefPtr<InternalRequest> TypeUtils::ToInternalRequest(
    JSContext* aCx, const RequestOrUSVString& aIn, BodyAction aBodyAction,
    ErrorResult& aRv) {
  if (aIn.IsRequest()) {
    Request& request = aIn.GetAsRequest();

    // Check and set bodyUsed flag immediately because its on Request
    // instead of InternalRequest.
    CheckAndSetBodyUsed(aCx, request, aBodyAction, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    return request.GetInternalRequest();
  }

  return ToInternalRequest(aIn.GetAsUSVString(), aRv);
}

SafeRefPtr<InternalRequest> TypeUtils::ToInternalRequest(
    JSContext* aCx, const OwningRequestOrUSVString& aIn, BodyAction aBodyAction,
    ErrorResult& aRv) {
  if (aIn.IsRequest()) {
    Request& request = aIn.GetAsRequest();

    // Check and set bodyUsed flag immediately because its on Request
    // instead of InternalRequest.
    CheckAndSetBodyUsed(aCx, request, aBodyAction, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    return request.GetInternalRequest();
  }

  return ToInternalRequest(aIn.GetAsUSVString(), aRv);
}

void TypeUtils::ToCacheRequest(CacheRequest& aOut, const InternalRequest& aIn,
                               BodyAction aBodyAction,
                               SchemeAction aSchemeAction, ErrorResult& aRv) {
  aIn.GetMethod(aOut.method());
  nsCString url(aIn.GetURLWithoutFragment());
  bool schemeValid;
  ProcessURL(url, &schemeValid, &aOut.urlWithoutQuery(), &aOut.urlQuery(), aRv);
  if (aRv.Failed()) {
    return;
  }
  if (!schemeValid) {
    if (aSchemeAction == TypeErrorOnInvalidScheme) {
      aRv.ThrowTypeError<MSG_INVALID_URL_SCHEME>("Request", url);
      return;
    }
  }
  aOut.urlFragment() = aIn.GetFragment();

  aIn.GetReferrer(aOut.referrer());
  aOut.referrerPolicy() = aIn.ReferrerPolicy_();
  RefPtr<InternalHeaders> headers = aIn.Headers();
  MOZ_DIAGNOSTIC_ASSERT(headers);
  aOut.headers() = ToHeadersEntryList(headers);
  aOut.headersGuard() = headers->Guard();
  aOut.mode() = aIn.Mode();
  aOut.credentials() = aIn.GetCredentialsMode();
  aOut.contentPolicyType() = aIn.ContentPolicyType();
  aOut.requestCache() = aIn.GetCacheMode();
  aOut.requestRedirect() = aIn.GetRedirectMode();

  aOut.integrity() = aIn.GetIntegrity();
  aOut.loadingEmbedderPolicy() = aIn.GetEmbedderPolicy();
  const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      aIn.GetPrincipalInfo();
  if (principalInfo) {
    aOut.principalInfo() = Some(*(principalInfo.get()));
  }

  if (aBodyAction == IgnoreBody) {
    aOut.body() = Nothing();
    return;
  }

  // BodyUsed flag is checked and set previously in ToInternalRequest()

  nsCOMPtr<nsIInputStream> stream;
  aIn.GetBody(getter_AddRefs(stream));
  SerializeCacheStream(stream, &aOut.body(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void TypeUtils::ToCacheResponseWithoutBody(CacheResponse& aOut,
                                           InternalResponse& aIn,
                                           ErrorResult& aRv) {
  aOut.type() = aIn.Type();

  aIn.GetUnfilteredURLList(aOut.urlList());
  AutoTArray<nsCString, 4> urlList;
  aIn.GetURLList(urlList);

  for (uint32_t i = 0; i < aOut.urlList().Length(); i++) {
    MOZ_DIAGNOSTIC_ASSERT(!aOut.urlList()[i].IsEmpty());
    // Pass all Response URL schemes through... The spec only requires we take
    // action on invalid schemes for Request objects.
    ProcessURL(aOut.urlList()[i], nullptr, nullptr, nullptr, aRv);
  }

  aOut.status() = aIn.GetUnfilteredStatus();
  aOut.statusText() = aIn.GetUnfilteredStatusText();
  RefPtr<InternalHeaders> headers = aIn.UnfilteredHeaders();
  MOZ_DIAGNOSTIC_ASSERT(headers);
  if (aIn.Type() != ResponseType::Opaque && HasVaryStar(headers)) {
    aRv.ThrowTypeError("Invalid Response object with a 'Vary: *' header.");
    return;
  }
  aOut.headers() = ToHeadersEntryList(headers);
  aOut.headersGuard() = headers->Guard();
  aOut.securityInfo() = aIn.GetChannelInfo().SecurityInfo();
  if (aIn.GetPrincipalInfo()) {
    aOut.principalInfo() = Some(*aIn.GetPrincipalInfo());
  } else {
    aOut.principalInfo() = Nothing();
  }

  aOut.paddingInfo() = aIn.GetPaddingInfo();
  aOut.paddingSize() = aIn.GetPaddingSize();
}

void TypeUtils::ToCacheResponse(JSContext* aCx, CacheResponse& aOut,
                                Response& aIn, ErrorResult& aRv) {
  if (aIn.BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return;
  }

  SafeRefPtr<InternalResponse> ir = aIn.GetInternalResponse();
  ToCacheResponseWithoutBody(aOut, *ir, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  ir->GetUnfilteredBody(getter_AddRefs(stream));
  if (stream) {
    aIn.SetBodyUsed(aCx, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  SerializeCacheStream(stream, &aOut.body(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

// static
void TypeUtils::ToCacheQueryParams(CacheQueryParams& aOut,
                                   const CacheQueryOptions& aIn) {
  aOut.ignoreSearch() = aIn.mIgnoreSearch;
  aOut.ignoreMethod() = aIn.mIgnoreMethod;
  aOut.ignoreVary() = aIn.mIgnoreVary;
}

// static
void TypeUtils::ToCacheQueryParams(CacheQueryParams& aOut,
                                   const MultiCacheQueryOptions& aIn) {
  ToCacheQueryParams(aOut, static_cast<const CacheQueryOptions&>(aIn));
  aOut.cacheNameSet() = aIn.mCacheName.WasPassed();
  if (aOut.cacheNameSet()) {
    aOut.cacheName() = aIn.mCacheName.Value();
  } else {
    aOut.cacheName() = u""_ns;
  }
}

already_AddRefed<Response> TypeUtils::ToResponse(const CacheResponse& aIn) {
  if (aIn.type() == ResponseType::Error) {
    // We don't bother tracking the internal error code for cached responses...
    RefPtr<Response> r =
        new Response(GetGlobalObject(),
                     InternalResponse::NetworkError(NS_ERROR_FAILURE), nullptr);
    return r.forget();
  }

  SafeRefPtr<InternalResponse> ir =
      MakeSafeRefPtr<InternalResponse>(aIn.status(), aIn.statusText());
  ir->SetURLList(aIn.urlList());

  RefPtr<InternalHeaders> internalHeaders =
      ToInternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;

  // Be careful to fill the headers before setting the guard in order to
  // correctly re-create the original headers.
  ir->Headers()->Fill(*internalHeaders, result);
  MOZ_DIAGNOSTIC_ASSERT(!result.Failed());
  ir->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_DIAGNOSTIC_ASSERT(!result.Failed());

  ir->InitChannelInfo(aIn.securityInfo());
  if (aIn.principalInfo().isSome()) {
    UniquePtr<mozilla::ipc::PrincipalInfo> info(
        new mozilla::ipc::PrincipalInfo(aIn.principalInfo().ref()));
    ir->SetPrincipalInfo(std::move(info));
  }

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());
  ir->SetBody(stream, InternalResponse::UNKNOWN_BODY_SIZE);

  switch (aIn.type()) {
    case ResponseType::Basic:
      ir = ir->BasicResponse();
      break;
    case ResponseType::Cors:
      ir = ir->CORSResponse();
      break;
    case ResponseType::Default:
      break;
    case ResponseType::Opaque:
      ir = ir->OpaqueResponse();
      break;
    case ResponseType::Opaqueredirect:
      ir = ir->OpaqueRedirectResponse();
      break;
    default:
      MOZ_CRASH("Unexpected ResponseType!");
  }
  MOZ_DIAGNOSTIC_ASSERT(ir);

  ir->SetPaddingSize(aIn.paddingSize());

  RefPtr<Response> ref =
      new Response(GetGlobalObject(), std::move(ir), nullptr);
  return ref.forget();
}
SafeRefPtr<InternalRequest> TypeUtils::ToInternalRequest(
    const CacheRequest& aIn) {
  nsAutoCString url(aIn.urlWithoutQuery());
  url.Append(aIn.urlQuery());
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(url, aIn.urlFragment());
  internalRequest->SetMethod(aIn.method());
  internalRequest->SetReferrer(aIn.referrer());
  internalRequest->SetReferrerPolicy(aIn.referrerPolicy());
  internalRequest->SetMode(aIn.mode());
  internalRequest->SetCredentialsMode(aIn.credentials());
  internalRequest->SetContentPolicyType(aIn.contentPolicyType());
  internalRequest->SetCacheMode(aIn.requestCache());
  internalRequest->SetRedirectMode(aIn.requestRedirect());
  internalRequest->SetIntegrity(aIn.integrity());

  RefPtr<InternalHeaders> internalHeaders =
      ToInternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;

  // Be careful to fill the headers before setting the guard in order to
  // correctly re-create the original headers.
  internalRequest->Headers()->Fill(*internalHeaders, result);
  MOZ_DIAGNOSTIC_ASSERT(!result.Failed());

  internalRequest->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_DIAGNOSTIC_ASSERT(!result.Failed());

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());

  internalRequest->SetBody(stream, -1);

  return internalRequest;
}

SafeRefPtr<Request> TypeUtils::ToRequest(const CacheRequest& aIn) {
  return MakeSafeRefPtr<Request>(GetGlobalObject(), ToInternalRequest(aIn),
                                 nullptr);
}

// static
already_AddRefed<InternalHeaders> TypeUtils::ToInternalHeaders(
    const nsTArray<HeadersEntry>& aHeadersEntryList, HeadersGuardEnum aGuard) {
  nsTArray<InternalHeaders::Entry> entryList;
  entryList.SetCapacity(aHeadersEntryList.Length());
  std::transform(aHeadersEntryList.cbegin(), aHeadersEntryList.cend(),
                 MakeBackInserter(entryList), [](const auto& headersEntry) {
                   return InternalHeaders::Entry(headersEntry.name(),
                                                 headersEntry.value());
                 });

  RefPtr<InternalHeaders> ref =
      new InternalHeaders(std::move(entryList), aGuard);
  return ref.forget();
}

// Utility function to remove the fragment from a URL, check its scheme, and
// optionally provide a URL without the query.  We're not using nsIURL or URL to
// do this because they require going to the main thread. static
void TypeUtils::ProcessURL(nsACString& aUrl, bool* aSchemeValidOut,
                           nsACString* aUrlWithoutQueryOut,
                           nsACString* aUrlQueryOut, ErrorResult& aRv) {
  const nsCString& flatURL = PromiseFlatCString(aUrl);
  const char* url = flatURL.get();

  // off the main thread URL parsing using nsStdURLParser.
  nsCOMPtr<nsIURLParser> urlParser = new nsStdURLParser();

  uint32_t pathPos;
  int32_t pathLen;
  uint32_t schemePos;
  int32_t schemeLen;
  aRv = urlParser->ParseURL(url, flatURL.Length(), &schemePos, &schemeLen,
                            nullptr, nullptr,  // ignore authority
                            &pathPos, &pathLen);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (aSchemeValidOut) {
    nsAutoCString scheme(Substring(flatURL, schemePos, schemeLen));
    *aSchemeValidOut =
        scheme.LowerCaseEqualsLiteral("http") ||
        scheme.LowerCaseEqualsLiteral("https") ||
        (StaticPrefs::extensions_backgroundServiceWorker_enabled_AtStartup() &&
         scheme.LowerCaseEqualsLiteral("moz-extension"));
  }

  uint32_t queryPos;
  int32_t queryLen;

  aRv = urlParser->ParsePath(url + pathPos, flatURL.Length() - pathPos, nullptr,
                             nullptr,  // ignore filepath
                             &queryPos, &queryLen, nullptr, nullptr);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!aUrlWithoutQueryOut) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aUrlQueryOut);

  if (queryLen < 0) {
    *aUrlWithoutQueryOut = aUrl;
    aUrlQueryOut->Truncate();
    return;
  }

  // ParsePath gives us query position relative to the start of the path
  queryPos += pathPos;

  *aUrlWithoutQueryOut = Substring(aUrl, 0, queryPos - 1);
  *aUrlQueryOut = Substring(aUrl, queryPos - 1, queryLen + 1);
}

void TypeUtils::CheckAndSetBodyUsed(JSContext* aCx, Request& aRequest,
                                    BodyAction aBodyAction, ErrorResult& aRv) {
  if (aBodyAction == IgnoreBody) {
    return;
  }

  if (aRequest.BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  aRequest.GetBody(getter_AddRefs(stream));
  if (stream) {
    aRequest.SetBodyUsed(aCx, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }
}

SafeRefPtr<InternalRequest> TypeUtils::ToInternalRequest(const nsAString& aIn,
                                                         ErrorResult& aRv) {
  RequestOrUSVString requestOrString;
  requestOrString.SetAsUSVString().ShareOrDependUpon(aIn);

  // Re-create a GlobalObject stack object so we can use webidl Constructors.
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  JSContext* cx = jsapi.cx();
  GlobalObject global(cx, GetGlobalObject()->GetGlobalJSObject());
  MOZ_DIAGNOSTIC_ASSERT(!global.Failed());

  RootedDictionary<RequestInit> requestInit(cx);
  SafeRefPtr<Request> request =
      Request::Constructor(global, requestOrString, requestInit, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return request->GetInternalRequest();
}

void TypeUtils::SerializeCacheStream(nsIInputStream* aStream,
                                     Maybe<CacheReadStream>* aStreamOut,
                                     ErrorResult& aRv) {
  *aStreamOut = Nothing();
  if (!aStream) {
    return;
  }

  RefPtr<ReadStream> controlled = do_QueryObject(aStream);
  if (controlled) {
    controlled->Serialize(aStreamOut, aRv);
    return;
  }

  aStreamOut->emplace(CacheReadStream());
  CacheReadStream& cacheStream = aStreamOut->ref();

  cacheStream.control() = nullptr;

  MOZ_ALWAYS_TRUE(mozilla::ipc::SerializeIPCStream(do_AddRef(aStream),
                                                   cacheStream.stream(),
                                                   /* aAllowLazy */ false));
}

}  // namespace mozilla::dom::cache
