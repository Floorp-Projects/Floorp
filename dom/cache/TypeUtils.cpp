/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/TypeUtils.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PFileDescriptorSetChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/SendStream.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsQueryObject.h"
#include "nsPromiseFlatString.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsURLParsers.h"
#include "nsCRT.h"
#include "nsHttp.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::AutoIPCStream;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::PFileDescriptorSetChild;

namespace {

static bool
HasVaryStar(mozilla::dom::InternalHeaders* aHeaders)
{
  AutoTArray<nsCString, 16> varyHeaders;
  ErrorResult rv;
  aHeaders->GetAll(NS_LITERAL_CSTRING("vary"), varyHeaders, rv);
  MOZ_ALWAYS_TRUE(!rv.Failed());

  for (uint32_t i = 0; i < varyHeaders.Length(); ++i) {
    nsAutoCString varyValue(varyHeaders[i]);
    char* rawBuffer = varyValue.BeginWriting();
    char* token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer);
    for (; token;
         token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer)) {
      nsDependentCString header(token);
      if (header.EqualsLiteral("*")) {
        return true;
      }
    }
  }
  return false;
}

void
ToHeadersEntryList(nsTArray<HeadersEntry>& aOut, InternalHeaders* aHeaders)
{
  MOZ_ASSERT(aHeaders);

  AutoTArray<InternalHeaders::Entry, 16> entryList;
  aHeaders->GetEntries(entryList);

  for (uint32_t i = 0; i < entryList.Length(); ++i) {
    InternalHeaders::Entry& entry = entryList[i];
    aOut.AppendElement(HeadersEntry(entry.mName, entry.mValue));
  }
}

} // namespace

already_AddRefed<InternalRequest>
TypeUtils::ToInternalRequest(const RequestOrUSVString& aIn,
                             BodyAction aBodyAction, ErrorResult& aRv)
{
  if (aIn.IsRequest()) {
    Request& request = aIn.GetAsRequest();

    // Check and set bodyUsed flag immediately because its on Request
    // instead of InternalRequest.
    CheckAndSetBodyUsed(&request, aBodyAction, aRv);
    if (aRv.Failed()) { return nullptr; }

    return request.GetInternalRequest();
  }

  return ToInternalRequest(aIn.GetAsUSVString(), aRv);
}

already_AddRefed<InternalRequest>
TypeUtils::ToInternalRequest(const OwningRequestOrUSVString& aIn,
                             BodyAction aBodyAction, ErrorResult& aRv)
{

  if (aIn.IsRequest()) {
    RefPtr<Request> request = aIn.GetAsRequest().get();

    // Check and set bodyUsed flag immediately because its on Request
    // instead of InternalRequest.
    CheckAndSetBodyUsed(request, aBodyAction, aRv);
    if (aRv.Failed()) { return nullptr; }

    return request->GetInternalRequest();
  }

  return ToInternalRequest(aIn.GetAsUSVString(), aRv);
}

void
TypeUtils::ToCacheRequest(CacheRequest& aOut, InternalRequest* aIn,
                          BodyAction aBodyAction, SchemeAction aSchemeAction,
                          nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(aIn);

  aIn->GetMethod(aOut.method());

  nsAutoCString url;
  aIn->GetURL(url);

  bool schemeValid;
  ProcessURL(url, &schemeValid, &aOut.urlWithoutQuery(), &aOut.urlQuery(), aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!schemeValid) {
    if (aSchemeAction == TypeErrorOnInvalidScheme) {
      NS_ConvertUTF8toUTF16 urlUTF16(url);
      aRv.ThrowTypeError<MSG_INVALID_URL_SCHEME>(NS_LITERAL_STRING("Request"),
                                                 urlUTF16);
      return;
    }
  }

  aIn->GetReferrer(aOut.referrer());
  aOut.referrerPolicy() = aIn->ReferrerPolicy_();

  RefPtr<InternalHeaders> headers = aIn->Headers();
  MOZ_ASSERT(headers);
  ToHeadersEntryList(aOut.headers(), headers);
  aOut.headersGuard() = headers->Guard();
  aOut.mode() = aIn->Mode();
  aOut.credentials() = aIn->GetCredentialsMode();
  aOut.contentPolicyType() = aIn->ContentPolicyType();
  aOut.requestCache() = aIn->GetCacheMode();
  aOut.requestRedirect() = aIn->GetRedirectMode();

  if (aBodyAction == IgnoreBody) {
    aOut.body() = void_t();
    return;
  }

  // BodyUsed flag is checked and set previously in ToInternalRequest()

  nsCOMPtr<nsIInputStream> stream;
  aIn->GetBody(getter_AddRefs(stream));
  SerializeCacheStream(stream, &aOut.body(), aStreamCleanupList, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
TypeUtils::ToCacheResponseWithoutBody(CacheResponse& aOut,
                                      InternalResponse& aIn, ErrorResult& aRv)
{
  aOut.type() = aIn.Type();

  aIn.GetUnfilteredURLList(aOut.urlList());
  AutoTArray<nsCString, 4> urlList;
  aIn.GetURLList(urlList);

  for (uint32_t i = 0; i < aOut.urlList().Length(); i++) {
    MOZ_ASSERT(!aOut.urlList()[i].IsEmpty());
    // Pass all Response URL schemes through... The spec only requires we take
    // action on invalid schemes for Request objects.
    ProcessURL(aOut.urlList()[i], nullptr, nullptr, nullptr, aRv);
  }

  aOut.status() = aIn.GetUnfilteredStatus();
  aOut.statusText() = aIn.GetUnfilteredStatusText();
  RefPtr<InternalHeaders> headers = aIn.UnfilteredHeaders();
  MOZ_ASSERT(headers);
  if (HasVaryStar(headers)) {
    aRv.ThrowTypeError<MSG_RESPONSE_HAS_VARY_STAR>();
    return;
  }
  ToHeadersEntryList(aOut.headers(), headers);
  aOut.headersGuard() = headers->Guard();
  aOut.channelInfo() = aIn.GetChannelInfo().AsIPCChannelInfo();
  if (aIn.GetPrincipalInfo()) {
    aOut.principalInfo() = *aIn.GetPrincipalInfo();
  } else {
    aOut.principalInfo() = void_t();
  }
}

void
TypeUtils::ToCacheResponse(CacheResponse& aOut, Response& aIn,
                           nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList,
                           ErrorResult& aRv)
{
  if (aIn.BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return;
  }

  RefPtr<InternalResponse> ir = aIn.GetInternalResponse();
  ToCacheResponseWithoutBody(aOut, *ir, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  ir->GetUnfilteredBody(getter_AddRefs(stream));
  if (stream) {
    aIn.SetBodyUsed();
  }

  SerializeCacheStream(stream, &aOut.body(), aStreamCleanupList, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

// static
void
TypeUtils::ToCacheQueryParams(CacheQueryParams& aOut,
                              const CacheQueryOptions& aIn)
{
  aOut.ignoreSearch() = aIn.mIgnoreSearch;
  aOut.ignoreMethod() = aIn.mIgnoreMethod;
  aOut.ignoreVary() = aIn.mIgnoreVary;
  aOut.cacheNameSet() = aIn.mCacheName.WasPassed();
  if (aOut.cacheNameSet()) {
    aOut.cacheName() = aIn.mCacheName.Value();
  } else {
    aOut.cacheName() = NS_LITERAL_STRING("");
  }
}

already_AddRefed<Response>
TypeUtils::ToResponse(const CacheResponse& aIn)
{
  if (aIn.type() == ResponseType::Error) {
    RefPtr<InternalResponse> error = InternalResponse::NetworkError();
    RefPtr<Response> r = new Response(GetGlobalObject(), error);
    return r.forget();
  }

  RefPtr<InternalResponse> ir = new InternalResponse(aIn.status(),
                                                       aIn.statusText());
  ir->SetURLList(aIn.urlList());

  RefPtr<InternalHeaders> internalHeaders =
    ToInternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;
  ir->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_ASSERT(!result.Failed());
  ir->Headers()->Fill(*internalHeaders, result);
  MOZ_ASSERT(!result.Failed());

  ir->InitChannelInfo(aIn.channelInfo());
  if (aIn.principalInfo().type() == mozilla::ipc::OptionalPrincipalInfo::TPrincipalInfo) {
    UniquePtr<mozilla::ipc::PrincipalInfo> info(new mozilla::ipc::PrincipalInfo(aIn.principalInfo().get_PrincipalInfo()));
    ir->SetPrincipalInfo(Move(info));
  }

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());
  ir->SetBody(stream, InternalResponse::UNKNOWN_BODY_SIZE);

  switch (aIn.type())
  {
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
  MOZ_ASSERT(ir);

  RefPtr<Response> ref = new Response(GetGlobalObject(), ir);
  return ref.forget();
}

already_AddRefed<InternalRequest>
TypeUtils::ToInternalRequest(const CacheRequest& aIn)
{
  nsAutoCString url(aIn.urlWithoutQuery());
  url.Append(aIn.urlQuery());

  RefPtr<InternalRequest> internalRequest = new InternalRequest(url);

  internalRequest->SetMethod(aIn.method());

  internalRequest->SetReferrer(aIn.referrer());
  internalRequest->SetReferrerPolicy(aIn.referrerPolicy());
  internalRequest->SetMode(aIn.mode());
  internalRequest->SetCredentialsMode(aIn.credentials());
  internalRequest->SetContentPolicyType(aIn.contentPolicyType());
  internalRequest->SetCacheMode(aIn.requestCache());
  internalRequest->SetRedirectMode(aIn.requestRedirect());

  RefPtr<InternalHeaders> internalHeaders =
    ToInternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;

  // Be careful to fill the headers before setting the guard in order to
  // correctly re-create the original headers.
  internalRequest->Headers()->Fill(*internalHeaders, result);
  MOZ_ASSERT(!result.Failed());

  internalRequest->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_ASSERT(!result.Failed());

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());

  internalRequest->SetBody(stream);

  return internalRequest.forget();
}

already_AddRefed<Request>
TypeUtils::ToRequest(const CacheRequest& aIn)
{
  RefPtr<InternalRequest> internalRequest = ToInternalRequest(aIn);
  RefPtr<Request> request = new Request(GetGlobalObject(), internalRequest);
  return request.forget();
}

// static
already_AddRefed<InternalHeaders>
TypeUtils::ToInternalHeaders(const nsTArray<HeadersEntry>& aHeadersEntryList,
                             HeadersGuardEnum aGuard)
{
  nsTArray<InternalHeaders::Entry> entryList(aHeadersEntryList.Length());

  for (uint32_t i = 0; i < aHeadersEntryList.Length(); ++i) {
    const HeadersEntry& headersEntry = aHeadersEntryList[i];
    entryList.AppendElement(InternalHeaders::Entry(headersEntry.name(),
                                                   headersEntry.value()));
  }

  RefPtr<InternalHeaders> ref = new InternalHeaders(Move(entryList), aGuard);
  return ref.forget();
}

// Utility function to remove the fragment from a URL, check its scheme, and optionally
// provide a URL without the query.  We're not using nsIURL or URL to do this because
// they require going to the main thread.
// static
void
TypeUtils::ProcessURL(nsACString& aUrl, bool* aSchemeValidOut,
                      nsACString* aUrlWithoutQueryOut,nsACString* aUrlQueryOut,
                      ErrorResult& aRv)
{
  const nsAFlatCString& flatURL = PromiseFlatCString(aUrl);
  const char* url = flatURL.get();

  // off the main thread URL parsing using nsStdURLParser.
  nsCOMPtr<nsIURLParser> urlParser = new nsStdURLParser();

  uint32_t pathPos;
  int32_t pathLen;
  uint32_t schemePos;
  int32_t schemeLen;
  aRv = urlParser->ParseURL(url, flatURL.Length(), &schemePos, &schemeLen,
                            nullptr, nullptr,       // ignore authority
                            &pathPos, &pathLen);
  if (NS_WARN_IF(aRv.Failed())) { return; }

  if (aSchemeValidOut) {
    nsAutoCString scheme(Substring(flatURL, schemePos, schemeLen));
    *aSchemeValidOut = scheme.LowerCaseEqualsLiteral("http") ||
                       scheme.LowerCaseEqualsLiteral("https") ||
                       scheme.LowerCaseEqualsLiteral("app");
  }

  uint32_t queryPos;
  int32_t queryLen;

  aRv = urlParser->ParsePath(url + pathPos, flatURL.Length() - pathPos,
                             nullptr, nullptr,               // ignore filepath
                             &queryPos, &queryLen,
                             nullptr, nullptr);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!aUrlWithoutQueryOut) {
    return;
  }

  MOZ_ASSERT(aUrlQueryOut);

  if (queryLen < 0) {
    *aUrlWithoutQueryOut = aUrl;
    *aUrlQueryOut = EmptyCString();
    return;
  }

  // ParsePath gives us query position relative to the start of the path
  queryPos += pathPos;

  *aUrlWithoutQueryOut = Substring(aUrl, 0, queryPos - 1);
  *aUrlQueryOut = Substring(aUrl, queryPos - 1, queryLen + 1);
}

void
TypeUtils::CheckAndSetBodyUsed(Request* aRequest, BodyAction aBodyAction,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(aRequest);

  if (aBodyAction == IgnoreBody) {
    return;
  }

  if (aRequest->BodyUsed()) {
    aRv.ThrowTypeError<MSG_FETCH_BODY_CONSUMED_ERROR>();
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  aRequest->GetBody(getter_AddRefs(stream));
  if (stream) {
    aRequest->SetBodyUsed();
  }
}

already_AddRefed<InternalRequest>
TypeUtils::ToInternalRequest(const nsAString& aIn, ErrorResult& aRv)
{
  RequestOrUSVString requestOrString;
  requestOrString.SetAsUSVString().Rebind(aIn.Data(), aIn.Length());

  // Re-create a GlobalObject stack object so we can use webidl Constructors.
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  JSContext* cx = jsapi.cx();
  GlobalObject global(cx, GetGlobalObject()->GetGlobalJSObject());
  MOZ_ASSERT(!global.Failed());

  RefPtr<Request> request = Request::Constructor(global, requestOrString,
                                                   RequestInit(), aRv);
  if (NS_WARN_IF(aRv.Failed())) { return nullptr; }

  return request->GetInternalRequest();
}

void
TypeUtils::SerializeCacheStream(nsIInputStream* aStream,
                                CacheReadStreamOrVoid* aStreamOut,
                                nsTArray<UniquePtr<AutoIPCStream>>& aStreamCleanupList,
                                ErrorResult& aRv)
{
  *aStreamOut = void_t();
  if (!aStream) {
    return;
  }

  RefPtr<ReadStream> controlled = do_QueryObject(aStream);
  if (controlled) {
    controlled->Serialize(aStreamOut, aStreamCleanupList, aRv);
    return;
  }

  *aStreamOut = CacheReadStream();
  CacheReadStream& cacheStream = aStreamOut->get_CacheReadStream();

  cacheStream.controlChild() = nullptr;
  cacheStream.controlParent() = nullptr;

  UniquePtr<AutoIPCStream> autoStream(new AutoIPCStream(cacheStream.stream()));
  autoStream->Serialize(aStream, GetIPCManager());

  aStreamCleanupList.AppendElement(Move(autoStream));
}

} // namespace cache
} // namespace dom
} // namespace mozilla
