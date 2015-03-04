/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/TypeUtils.h"

#include "mozilla/unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/PCacheTypes.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PFileDescriptorSetChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsURLParsers.h"

namespace {

using mozilla::ErrorResult;

// Utility function to remove the fragment from a URL, check its scheme, and optionally
// provide a URL without the query.  We're not using nsIURL or URL to do this because
// they require going to the main thread.
static void
ProcessURL(nsAString& aUrl, bool* aSchemeValidOut,
           nsAString* aUrlWithoutQueryOut, ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 flatURL(aUrl);
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
                       scheme.LowerCaseEqualsLiteral("https");
  }

  uint32_t queryPos;
  int32_t queryLen;
  uint32_t refPos;
  int32_t refLen;

  aRv = urlParser->ParsePath(url + pathPos, flatURL.Length() - pathPos,
                             nullptr, nullptr,               // ignore filepath
                             &queryPos, &queryLen,
                             &refPos, &refLen);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // TODO: Remove this once Request/Response properly strip the fragment (bug 1110476)
  if (refLen >= 0) {
    // ParsePath gives us ref position relative to the start of the path
    refPos += pathPos;

    aUrl = Substring(aUrl, 0, refPos - 1);
  }

  if (!aUrlWithoutQueryOut) {
    return;
  }

  if (queryLen < 0) {
    *aUrlWithoutQueryOut = aUrl;
    return;
  }

  // ParsePath gives us query position relative to the start of the path
  queryPos += pathPos;

  // We want everything before the query sine we already removed the trailing
  // fragment
  *aUrlWithoutQueryOut = Substring(aUrl, 0, queryPos - 1);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::FileDescriptor;
using mozilla::ipc::FileDescriptorSetChild;
using mozilla::ipc::PFileDescriptorSetChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::OptionalFileDescriptorSet;


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
    nsRefPtr<Request> request = aIn.GetAsRequest().get();

    // Check and set bodyUsed flag immediately because its on Request
    // instead of InternalRequest.
    CheckAndSetBodyUsed(request, aBodyAction, aRv);
    if (aRv.Failed()) { return nullptr; }

    return request->GetInternalRequest();
  }

  return ToInternalRequest(aIn.GetAsUSVString(), aRv);
}

void
TypeUtils::ToPCacheRequest(PCacheRequest& aOut, InternalRequest* aIn,
                           BodyAction aBodyAction,
                           ReferrerAction aReferrerAction,
                           SchemeAction aSchemeAction, ErrorResult& aRv)
{
  MOZ_ASSERT(aIn);

  aIn->GetMethod(aOut.method());

  nsAutoCString url;
  aIn->GetURL(url);
  CopyUTF8toUTF16(url, aOut.url());

  bool schemeValid;
  ProcessURL(aOut.url(), &schemeValid, &aOut.urlWithoutQuery(), aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!schemeValid) {
    if (aSchemeAction == TypeErrorOnInvalidScheme) {
      NS_NAMED_LITERAL_STRING(label, "Request");
      aRv.ThrowTypeError(MSG_INVALID_URL_SCHEME, &label, &aOut.url());
      return;
    }

    if (aSchemeAction == NetworkErrorOnInvalidScheme) {
      aRv.Throw(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }
  }

  if (aReferrerAction == ExpandReferrer) {
    UpdateRequestReferrer(GetGlobalObject(), aIn);
  }
  aIn->GetReferrer(aOut.referrer());

  nsRefPtr<InternalHeaders> headers = aIn->Headers();
  MOZ_ASSERT(headers);
  headers->GetPHeaders(aOut.headers());
  aOut.headersGuard() = headers->Guard();
  aOut.mode() = aIn->Mode();
  aOut.credentials() = aIn->GetCredentialsMode();
  aOut.context() = aIn->ContentPolicyType();

  if (aBodyAction == IgnoreBody) {
    aOut.body() = void_t();
    return;
  }

  // BodyUsed flag is checked and set previously in ToInternalRequest()

  nsCOMPtr<nsIInputStream> stream;
  aIn->GetBody(getter_AddRefs(stream));
  SerializeCacheStream(stream, &aOut.body(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
TypeUtils::ToPCacheResponseWithoutBody(PCacheResponse& aOut,
                                       InternalResponse& aIn, ErrorResult& aRv)
{
  aOut.type() = aIn.Type();

  nsAutoCString url;
  aIn.GetUrl(url);
  CopyUTF8toUTF16(url, aOut.url());

  if (aOut.url() != EmptyString()) {
    // Pass all Response URL schemes through... The spec only requires we take
    // action on invalid schemes for Request objects.
    ProcessURL(aOut.url(), nullptr, nullptr, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  aOut.status() = aIn.GetStatus();
  aOut.statusText() = aIn.GetStatusText();
  nsRefPtr<InternalHeaders> headers = aIn.Headers();
  MOZ_ASSERT(headers);
  headers->GetPHeaders(aOut.headers());
  aOut.headersGuard() = headers->Guard();
  aOut.securityInfo() = aIn.GetSecurityInfo();
}

void
TypeUtils::ToPCacheResponse(PCacheResponse& aOut, Response& aIn, ErrorResult& aRv)
{
  if (aIn.BodyUsed()) {
    aRv.ThrowTypeError(MSG_FETCH_BODY_CONSUMED_ERROR);
    return;
  }

  nsRefPtr<InternalResponse> ir = aIn.GetInternalResponse();
  ToPCacheResponseWithoutBody(aOut, *ir, aRv);

  nsCOMPtr<nsIInputStream> stream;
  aIn.GetBody(getter_AddRefs(stream));
  if (stream) {
    aIn.SetBodyUsed();
  }

  SerializeCacheStream(stream, &aOut.body(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

// static
void
TypeUtils::ToPCacheQueryParams(PCacheQueryParams& aOut,
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
TypeUtils::ToResponse(const PCacheResponse& aIn)
{
  nsRefPtr<InternalResponse> ir;
  switch (aIn.type())
  {
    case ResponseType::Error:
      ir = InternalResponse::NetworkError();
      break;
    case ResponseType::Opaque:
      ir = InternalResponse::OpaqueResponse();
      break;
    case ResponseType::Default:
      ir = new InternalResponse(aIn.status(), aIn.statusText());
      break;
    case ResponseType::Basic:
    {
      nsRefPtr<InternalResponse> inner = new InternalResponse(aIn.status(),
                                                              aIn.statusText());
      ir = InternalResponse::BasicResponse(inner);
      break;
    }
    case ResponseType::Cors:
    {
      nsRefPtr<InternalResponse> inner = new InternalResponse(aIn.status(),
                                                              aIn.statusText());
      ir = InternalResponse::CORSResponse(inner);
      break;
    }
    default:
      MOZ_CRASH("Unexpected ResponseType!");
  }
  MOZ_ASSERT(ir);

  ir->SetUrl(NS_ConvertUTF16toUTF8(aIn.url()));

  nsRefPtr<InternalHeaders> internalHeaders =
    new InternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;
  ir->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_ASSERT(!result.Failed());
  ir->Headers()->Fill(*internalHeaders, result);
  MOZ_ASSERT(!result.Failed());

  ir->SetSecurityInfo(aIn.securityInfo());

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());
  ir->SetBody(stream);

  nsRefPtr<Response> ref = new Response(GetGlobalObject(), ir);
  return ref.forget();
}

already_AddRefed<InternalRequest>
TypeUtils::ToInternalRequest(const PCacheRequest& aIn)
{
  nsRefPtr<InternalRequest> internalRequest = new InternalRequest();

  internalRequest->SetMethod(aIn.method());
  internalRequest->SetURL(NS_ConvertUTF16toUTF8(aIn.url()));
  internalRequest->SetReferrer(aIn.referrer());
  internalRequest->SetMode(aIn.mode());
  internalRequest->SetCredentialsMode(aIn.credentials());
  internalRequest->SetContentPolicyType(aIn.context());

  nsRefPtr<InternalHeaders> internalHeaders =
    new InternalHeaders(aIn.headers(), aIn.headersGuard());
  ErrorResult result;
  internalRequest->Headers()->SetGuard(aIn.headersGuard(), result);
  MOZ_ASSERT(!result.Failed());
  internalRequest->Headers()->Fill(*internalHeaders, result);
  MOZ_ASSERT(!result.Failed());

  nsCOMPtr<nsIInputStream> stream = ReadStream::Create(aIn.body());

  internalRequest->SetBody(stream);

  return internalRequest.forget();
}

already_AddRefed<Request>
TypeUtils::ToRequest(const PCacheRequest& aIn)
{
  nsRefPtr<InternalRequest> internalRequest = ToInternalRequest(aIn);
  nsRefPtr<Request> request = new Request(GetGlobalObject(), internalRequest);
  return request.forget();
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
    aRv.ThrowTypeError(MSG_FETCH_BODY_CONSUMED_ERROR);
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

  nsRefPtr<Request> request = Request::Constructor(global, requestOrString,
                                                   RequestInit(), aRv);
  if (NS_WARN_IF(aRv.Failed())) { return nullptr; }

  return request->GetInternalRequest();
}

void
TypeUtils::SerializeCacheStream(nsIInputStream* aStream,
                                PCacheReadStreamOrVoid* aStreamOut,
                                ErrorResult& aRv)
{
  *aStreamOut = void_t();
  if (!aStream) {
    return;
  }

  nsRefPtr<ReadStream> controlled = do_QueryObject(aStream);
  if (controlled) {
    controlled->Serialize(aStreamOut);
    return;
  }

  // TODO: implement CrossProcessPipe if we cannot directly serialize (bug 1110814)
  nsCOMPtr<nsIIPCSerializableInputStream> serial = do_QueryInterface(aStream);
  if (!serial) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  PCacheReadStream readStream;
  readStream.controlChild() = nullptr;
  readStream.controlParent() = nullptr;

  nsAutoTArray<FileDescriptor, 4> fds;
  SerializeInputStream(aStream, readStream.params(), fds);

  PFileDescriptorSetChild* fdSet = nullptr;
  if (!fds.IsEmpty()) {
    // We should not be serializing until we have an actor ready
    PBackgroundChild* manager = BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(manager);

    fdSet = manager->SendPFileDescriptorSetConstructor(fds[0]);
    for (uint32_t i = 1; i < fds.Length(); ++i) {
      unused << fdSet->SendAddFileDescriptor(fds[i]);
    }
  }

  if (fdSet) {
    readStream.fds() = fdSet;
  } else {
    readStream.fds() = void_t();
  }

  *aStreamOut = readStream;
}

nsIThread*
TypeUtils::GetStreamThread()
{
  AssertOwningThread();

  if (!mStreamThread) {
    // Named threads only allow 16 bytes for their names.  Try to make
    // it meaningful...
    // TODO: use a thread pool or singleton thread here (bug 1119864)
    nsresult rv = NS_NewNamedThread("DOMCacheTypeU",
                                    getter_AddRefs(mStreamThread));
    if (NS_FAILED(rv) || !mStreamThread) {
      MOZ_CRASH("Failed to create DOM Cache serialization thread.");
    }
  }

  return mStreamThread;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
