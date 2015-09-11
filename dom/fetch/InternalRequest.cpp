/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalRequest.h"

#include "nsIContentPolicy.h"
#include "nsIDocument.h"
#include "nsStreamUtils.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/workers/Workers.h"

#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

// The global is used to extract the principal.
already_AddRefed<InternalRequest>
InternalRequest::GetRequestConstructorCopy(nsIGlobalObject* aGlobal, ErrorResult& aRv) const
{
  nsRefPtr<InternalRequest> copy = new InternalRequest();
  copy->mURL.Assign(mURL);
  copy->SetMethod(mMethod);
  copy->mHeaders = new InternalHeaders(*mHeaders);
  copy->SetUnsafeRequest();

  copy->mBodyStream = mBodyStream;
  copy->mForceOriginHeader = true;
  // The "client" is not stored in our implementation. Fetch API users should
  // use the appropriate window/document/principal and other Gecko security
  // mechanisms as appropriate.
  copy->mSameOriginDataURL = true;
  copy->mPreserveContentCodings = true;
  // The default referrer is already about:client.

  copy->mContentPolicyType = nsIContentPolicy::TYPE_FETCH;
  copy->mMode = mMode;
  copy->mCredentialsMode = mCredentialsMode;
  copy->mCacheMode = mCacheMode;
  copy->mRedirectMode = mRedirectMode;
  copy->mCreatedByFetchEvent = mCreatedByFetchEvent;
  return copy.forget();
}

already_AddRefed<InternalRequest>
InternalRequest::Clone()
{
  nsRefPtr<InternalRequest> clone = new InternalRequest(*this);

  if (!mBodyStream) {
    return clone.forget();
  }

  nsCOMPtr<nsIInputStream> clonedBody;
  nsCOMPtr<nsIInputStream> replacementBody;

  nsresult rv = NS_CloneInputStream(mBodyStream, getter_AddRefs(clonedBody),
                                    getter_AddRefs(replacementBody));
  if (NS_WARN_IF(NS_FAILED(rv))) { return nullptr; }

  clone->mBodyStream.swap(clonedBody);
  if (replacementBody) {
    mBodyStream.swap(replacementBody);
  }

  return clone.forget();
}

InternalRequest::InternalRequest(const InternalRequest& aOther)
  : mMethod(aOther.mMethod)
  , mURL(aOther.mURL)
  , mHeaders(new InternalHeaders(*aOther.mHeaders))
  , mContentPolicyType(aOther.mContentPolicyType)
  , mReferrer(aOther.mReferrer)
  , mMode(aOther.mMode)
  , mCredentialsMode(aOther.mCredentialsMode)
  , mResponseTainting(aOther.mResponseTainting)
  , mCacheMode(aOther.mCacheMode)
  , mRedirectMode(aOther.mRedirectMode)
  , mAuthenticationFlag(aOther.mAuthenticationFlag)
  , mForceOriginHeader(aOther.mForceOriginHeader)
  , mPreserveContentCodings(aOther.mPreserveContentCodings)
  , mSameOriginDataURL(aOther.mSameOriginDataURL)
  , mSandboxedStorageAreaURLs(aOther.mSandboxedStorageAreaURLs)
  , mSkipServiceWorker(aOther.mSkipServiceWorker)
  , mSynchronous(aOther.mSynchronous)
  , mUnsafeRequest(aOther.mUnsafeRequest)
  , mUseURLCredentials(aOther.mUseURLCredentials)
  , mCreatedByFetchEvent(aOther.mCreatedByFetchEvent)
{
  // NOTE: does not copy body stream... use the fallible Clone() for that
}

InternalRequest::~InternalRequest()
{
}

void
InternalRequest::SetContentPolicyType(nsContentPolicyType aContentPolicyType)
{
  mContentPolicyType = aContentPolicyType;
}

/* static */
RequestContext
InternalRequest::MapContentPolicyTypeToRequestContext(nsContentPolicyType aContentPolicyType)
{
  RequestContext context = RequestContext::Internal;
  switch (aContentPolicyType) {
  case nsIContentPolicy::TYPE_OTHER:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
  case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
    context = RequestContext::Script;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_WORKER:
    context = RequestContext::Worker;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
    context = RequestContext::Sharedworker;
    break;
  case nsIContentPolicy::TYPE_IMAGE:
    context = RequestContext::Image;
    break;
  case nsIContentPolicy::TYPE_STYLESHEET:
    context = RequestContext::Style;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_OBJECT:
    context = RequestContext::Object;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_EMBED:
    context = RequestContext::Embed;
    break;
  case nsIContentPolicy::TYPE_DOCUMENT:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_IFRAME:
    context = RequestContext::Iframe;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_FRAME:
    context = RequestContext::Frame;
    break;
  case nsIContentPolicy::TYPE_REFRESH:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_XBL:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_PING:
    context = RequestContext::Ping;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST:
    context = RequestContext::Xmlhttprequest;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE:
    context = RequestContext::Eventsource;
    break;
  case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
    context = RequestContext::Plugin;
    break;
  case nsIContentPolicy::TYPE_DTD:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_FONT:
    context = RequestContext::Font;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_AUDIO:
    context = RequestContext::Audio;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_VIDEO:
    context = RequestContext::Video;
    break;
  case nsIContentPolicy::TYPE_INTERNAL_TRACK:
    context = RequestContext::Track;
    break;
  case nsIContentPolicy::TYPE_WEBSOCKET:
    context = RequestContext::Internal;
    break;
  case nsIContentPolicy::TYPE_CSP_REPORT:
    context = RequestContext::Cspreport;
    break;
  case nsIContentPolicy::TYPE_XSLT:
    context = RequestContext::Xslt;
    break;
  case nsIContentPolicy::TYPE_BEACON:
    context = RequestContext::Beacon;
    break;
  case nsIContentPolicy::TYPE_FETCH:
    context = RequestContext::Fetch;
    break;
  case nsIContentPolicy::TYPE_IMAGESET:
    context = RequestContext::Imageset;
    break;
  case nsIContentPolicy::TYPE_WEB_MANIFEST:
    context = RequestContext::Manifest;
    break;
  default:
    MOZ_ASSERT(false, "Unhandled nsContentPolicyType value");
    break;
  }
  return context;
}

// static
bool
InternalRequest::IsNavigationContentPolicy(nsContentPolicyType aContentPolicyType)
{
  // https://fetch.spec.whatwg.org/#navigation-request-context
  //
  // A navigation request context is one of "form", "frame", "hyperlink",
  // "iframe", "internal" (as long as context frame type is not "none"),
  // "location", "metarefresh", and "prerender".
  //
  // Note, all of these request types are effectively initiated by nsDocShell.
  //
  // The TYPE_REFRESH is used in some code paths for metarefresh, but will not
  // be seen during the actual load.  Instead the new load gets a normal
  // nsDocShell policy type.  We include it here in case this utility method
  // is called before the load starts.
  return aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT ||
         aContentPolicyType == nsIContentPolicy::TYPE_SUBDOCUMENT ||
         aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_FRAME ||
         aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_IFRAME ||
         aContentPolicyType == nsIContentPolicy::TYPE_REFRESH;
}

// static
bool
InternalRequest::IsWorkerContentPolicy(nsContentPolicyType aContentPolicyType)
{
  // https://fetch.spec.whatwg.org/#worker-request-context
  //
  // A worker request context is one of "serviceworker", "sharedworker", and
  // "worker".
  //
  // Note, service workers are not included here because currently there is
  // no way to generate a Request with a "serviceworker" RequestContext.
  // ServiceWorker scripts cannot be intercepted.
  return aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
         aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER;
}

bool
InternalRequest::IsNavigationRequest() const
{
  return IsNavigationContentPolicy(mContentPolicyType);
}

bool
InternalRequest::IsWorkerRequest() const
{
  return IsWorkerContentPolicy(mContentPolicyType);
}

bool
InternalRequest::IsClientRequest() const
{
  return IsNavigationRequest() || IsWorkerRequest();
}

// static
RequestMode
InternalRequest::MapChannelToRequestMode(nsIChannel* aChannel)
{
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aChannel->GetLoadInfo(getter_AddRefs(loadInfo))));

  // RequestMode deviates from our internal security mode for navigations.
  // While navigations normally allow cross origin we must set a same-origin
  // RequestMode to get the correct service worker interception restrictions
  // in place.
  // TODO: remove the worker override once securityMode is fully implemented (bug 1189945)
  nsContentPolicyType contentPolicy = loadInfo->InternalContentPolicyType();
  if (IsNavigationContentPolicy(contentPolicy) ||
      IsWorkerContentPolicy(contentPolicy)) {
    return RequestMode::Same_origin;
  }

  uint32_t securityMode;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(loadInfo->GetSecurityMode(&securityMode)));

  switch(securityMode) {
    case nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS:
    case nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED:
      return RequestMode::Same_origin;
    case nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS:
    case nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL:
      return RequestMode::No_cors;
    case nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS:
      // TODO: Check additional flag force-preflight after bug 1199693 (bug 1189945)
      return RequestMode::Cors;
    default:
      // TODO: assert never reached after CorsMode flag removed (bug 1189945)
      MOZ_ASSERT(securityMode == nsILoadInfo::SEC_NORMAL);
      break;
  }

  // TODO: remove following code once securityMode is fully implemented (bug 1189945)

  // We only support app:// protocol interception in non-release builds.
#ifndef RELEASE_BUILD
  nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(aChannel);
  if (jarChannel) {
    return RequestMode::No_cors;
  }
#endif

  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aChannel);

  uint32_t corsMode;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(httpChannel->GetCorsMode(&corsMode)));

  // This cast is valid due to static asserts in ServiceWorkerManager.cpp.
  return static_cast<RequestMode>(corsMode);
}

} // namespace dom
} // namespace mozilla
