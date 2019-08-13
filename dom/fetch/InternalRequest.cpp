/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InternalRequest.h"

#include "nsIContentPolicy.h"
#include "mozilla/dom/Document.h"
#include "nsStreamUtils.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/ScriptSettings.h"

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {
// The global is used to extract the principal.
already_AddRefed<InternalRequest> InternalRequest::GetRequestConstructorCopy(
    nsIGlobalObject* aGlobal, ErrorResult& aRv) const {
  MOZ_RELEASE_ASSERT(!mURLList.IsEmpty(),
                     "Internal Request's urlList should not be empty when "
                     "copied from constructor.");
  RefPtr<InternalRequest> copy =
      new InternalRequest(mURLList.LastElement(), mFragment);
  copy->SetMethod(mMethod);
  copy->mHeaders = new InternalHeaders(*mHeaders);
  copy->SetUnsafeRequest();
  copy->mBodyStream = mBodyStream;
  copy->mBodyLength = mBodyLength;
  // The "client" is not stored in our implementation. Fetch API users should
  // use the appropriate window/document/principal and other Gecko security
  // mechanisms as appropriate.
  copy->mSameOriginDataURL = true;
  copy->mPreserveContentCodings = true;
  copy->mReferrer = mReferrer;
  copy->mReferrerPolicy = mReferrerPolicy;
  copy->mEnvironmentReferrerPolicy = mEnvironmentReferrerPolicy;
  copy->mIntegrity = mIntegrity;
  copy->mMozErrors = mMozErrors;

  copy->mContentPolicyType = mContentPolicyTypeOverridden
                                 ? mContentPolicyType
                                 : nsIContentPolicy::TYPE_FETCH;
  copy->mMode = mMode;
  copy->mCredentialsMode = mCredentialsMode;
  copy->mCacheMode = mCacheMode;
  copy->mRedirectMode = mRedirectMode;
  copy->mCreatedByFetchEvent = mCreatedByFetchEvent;
  copy->mContentPolicyTypeOverridden = mContentPolicyTypeOverridden;

  copy->mPreferredAlternativeDataType = mPreferredAlternativeDataType;
  return copy.forget();
}

already_AddRefed<InternalRequest> InternalRequest::Clone() {
  RefPtr<InternalRequest> clone = new InternalRequest(*this);

  if (!mBodyStream) {
    return clone.forget();
  }

  nsCOMPtr<nsIInputStream> clonedBody;
  nsCOMPtr<nsIInputStream> replacementBody;

  nsresult rv = NS_CloneInputStream(mBodyStream, getter_AddRefs(clonedBody),
                                    getter_AddRefs(replacementBody));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  clone->mBodyStream.swap(clonedBody);
  if (replacementBody) {
    mBodyStream.swap(replacementBody);
  }
  return clone.forget();
}
InternalRequest::InternalRequest(const nsACString& aURL,
                                 const nsACString& aFragment)
    : mMethod("GET"),
      mHeaders(new InternalHeaders(HeadersGuardEnum::None)),
      mBodyLength(InternalResponse::UNKNOWN_BODY_SIZE),
      mContentPolicyType(nsIContentPolicy::TYPE_FETCH),
      mReferrer(NS_LITERAL_STRING(kFETCH_CLIENT_REFERRER_STR)),
      mReferrerPolicy(ReferrerPolicy::_empty),
      mEnvironmentReferrerPolicy(net::RP_Unset),
      mMode(RequestMode::No_cors),
      mCredentialsMode(RequestCredentials::Omit),
      mResponseTainting(LoadTainting::Basic),
      mCacheMode(RequestCache::Default),
      mRedirectMode(RequestRedirect::Follow),
      mMozErrors(false),
      mAuthenticationFlag(false),
      mPreserveContentCodings(false)
      // FIXME(nsm): This should be false by default, but will lead to the
      // algorithm never loading data: URLs right now. See Bug 1018872 about
      // how certain contexts will override it to set it to true. Fetch
      // specification does not handle this yet.
      ,
      mSameOriginDataURL(true),
      mSkipServiceWorker(false),
      mSynchronous(false),
      mUnsafeRequest(false),
      mUseURLCredentials(false) {
  MOZ_ASSERT(!aURL.IsEmpty());
  AddURL(aURL, aFragment);
}
InternalRequest::InternalRequest(
    const nsACString& aURL, const nsACString& aFragment,
    const nsACString& aMethod, already_AddRefed<InternalHeaders> aHeaders,
    RequestCache aCacheMode, RequestMode aMode,
    RequestRedirect aRequestRedirect, RequestCredentials aRequestCredentials,
    const nsAString& aReferrer, ReferrerPolicy aReferrerPolicy,
    nsContentPolicyType aContentPolicyType, const nsAString& aIntegrity)
    : mMethod(aMethod),
      mHeaders(aHeaders),
      mBodyLength(InternalResponse::UNKNOWN_BODY_SIZE),
      mContentPolicyType(aContentPolicyType),
      mReferrer(aReferrer),
      mReferrerPolicy(aReferrerPolicy),
      mEnvironmentReferrerPolicy(net::RP_Unset),
      mMode(aMode),
      mCredentialsMode(aRequestCredentials),
      mResponseTainting(LoadTainting::Basic),
      mCacheMode(aCacheMode),
      mRedirectMode(aRequestRedirect),
      mIntegrity(aIntegrity),
      mMozErrors(false),
      mAuthenticationFlag(false),
      mPreserveContentCodings(false)
      // FIXME See the above comment in the default constructor.
      ,
      mSameOriginDataURL(true),
      mSkipServiceWorker(false),
      mSynchronous(false),
      mUnsafeRequest(false),
      mUseURLCredentials(false) {
  MOZ_ASSERT(!aURL.IsEmpty());
  AddURL(aURL, aFragment);
}
InternalRequest::InternalRequest(const InternalRequest& aOther)
    : mMethod(aOther.mMethod),
      mURLList(aOther.mURLList),
      mHeaders(new InternalHeaders(*aOther.mHeaders)),
      mBodyLength(InternalResponse::UNKNOWN_BODY_SIZE),
      mContentPolicyType(aOther.mContentPolicyType),
      mReferrer(aOther.mReferrer),
      mReferrerPolicy(aOther.mReferrerPolicy),
      mEnvironmentReferrerPolicy(aOther.mEnvironmentReferrerPolicy),
      mMode(aOther.mMode),
      mCredentialsMode(aOther.mCredentialsMode),
      mResponseTainting(aOther.mResponseTainting),
      mCacheMode(aOther.mCacheMode),
      mRedirectMode(aOther.mRedirectMode),
      mIntegrity(aOther.mIntegrity),
      mMozErrors(aOther.mMozErrors),
      mFragment(aOther.mFragment),
      mAuthenticationFlag(aOther.mAuthenticationFlag),
      mPreserveContentCodings(aOther.mPreserveContentCodings),
      mSameOriginDataURL(aOther.mSameOriginDataURL),
      mSkipServiceWorker(aOther.mSkipServiceWorker),
      mSynchronous(aOther.mSynchronous),
      mUnsafeRequest(aOther.mUnsafeRequest),
      mUseURLCredentials(aOther.mUseURLCredentials),
      mCreatedByFetchEvent(aOther.mCreatedByFetchEvent),
      mContentPolicyTypeOverridden(aOther.mContentPolicyTypeOverridden) {
  // NOTE: does not copy body stream... use the fallible Clone() for that
}

InternalRequest::~InternalRequest() {}

void InternalRequest::SetContentPolicyType(
    nsContentPolicyType aContentPolicyType) {
  mContentPolicyType = aContentPolicyType;
}

void InternalRequest::OverrideContentPolicyType(
    nsContentPolicyType aContentPolicyType) {
  SetContentPolicyType(aContentPolicyType);
  mContentPolicyTypeOverridden = true;
}

/* static */
RequestDestination InternalRequest::MapContentPolicyTypeToRequestDestination(
    nsContentPolicyType aContentPolicyType) {
  RequestDestination destination = RequestDestination::_empty;
  switch (aContentPolicyType) {
    case nsIContentPolicy::TYPE_OTHER:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
    case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
    case nsIContentPolicy::TYPE_SCRIPT:
      destination = RequestDestination::Script;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_WORKER:
      destination = RequestDestination::Worker;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
      destination = RequestDestination::Sharedworker;
      break;
    case nsIContentPolicy::TYPE_IMAGESET:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON:
    case nsIContentPolicy::TYPE_IMAGE:
      destination = RequestDestination::Image;
      break;
    case nsIContentPolicy::TYPE_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD:
      destination = RequestDestination::Style;
      break;
    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_INTERNAL_OBJECT:
      destination = RequestDestination::Object;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_EMBED:
      destination = RequestDestination::Embed;
      break;
    case nsIContentPolicy::TYPE_DOCUMENT:
    case nsIContentPolicy::TYPE_SUBDOCUMENT:
    case nsIContentPolicy::TYPE_INTERNAL_IFRAME:
      destination = RequestDestination::Document;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_FRAME:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_REFRESH:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_XBL:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_PING:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    case nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_DTD:
    case nsIContentPolicy::TYPE_INTERNAL_DTD:
    case nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_FONT:
      destination = RequestDestination::Font;
      break;
    case nsIContentPolicy::TYPE_MEDIA:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_AUDIO:
      destination = RequestDestination::Audio;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_VIDEO:
      destination = RequestDestination::Video;
      break;
    case nsIContentPolicy::TYPE_INTERNAL_TRACK:
      destination = RequestDestination::Track;
      break;
    case nsIContentPolicy::TYPE_WEBSOCKET:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_CSP_REPORT:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_XSLT:
      destination = RequestDestination::Xslt;
      break;
    case nsIContentPolicy::TYPE_BEACON:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_FETCH:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_WEB_MANIFEST:
      destination = RequestDestination::Manifest;
      break;
    case nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD:
      destination = RequestDestination::_empty;
      break;
    case nsIContentPolicy::TYPE_SPECULATIVE:
      destination = RequestDestination::_empty;
      break;
    default:
      MOZ_ASSERT(false, "Unhandled nsContentPolicyType value");
      break;
  }

  return destination;
}

// static
bool InternalRequest::IsNavigationContentPolicy(
    nsContentPolicyType aContentPolicyType) {
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
bool InternalRequest::IsWorkerContentPolicy(
    nsContentPolicyType aContentPolicyType) {
  // https://fetch.spec.whatwg.org/#worker-request-context
  //
  // A worker request context is one of "serviceworker", "sharedworker", and
  // "worker".
  //
  // Note, service workers are not included here because currently there is
  // no way to generate a Request with a "serviceworker" RequestDestination.
  // ServiceWorker scripts cannot be intercepted.
  return aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
         aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER;
}

bool InternalRequest::IsNavigationRequest() const {
  return IsNavigationContentPolicy(mContentPolicyType);
}

bool InternalRequest::IsWorkerRequest() const {
  return IsWorkerContentPolicy(mContentPolicyType);
}

bool InternalRequest::IsClientRequest() const {
  return IsNavigationRequest() || IsWorkerRequest();
}

// static
RequestMode InternalRequest::MapChannelToRequestMode(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsContentPolicyType contentPolicy = loadInfo->InternalContentPolicyType();
  if (IsNavigationContentPolicy(contentPolicy)) {
    return RequestMode::Navigate;
  }

  // TODO: remove the worker override once securityMode is fully implemented
  // (bug 1189945)
  if (IsWorkerContentPolicy(contentPolicy)) {
    return RequestMode::Same_origin;
  }

  uint32_t securityMode = loadInfo->GetSecurityMode();

  switch (securityMode) {
    case nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS:
    case nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED:
      return RequestMode::Same_origin;
    case nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS:
    case nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL:
      return RequestMode::No_cors;
    case nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS:
      // TODO: Check additional flag force-preflight after bug 1199693 (bug
      // 1189945)
      return RequestMode::Cors;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected security mode!");
      return RequestMode::Same_origin;
  }
}

// static
RequestCredentials InternalRequest::MapChannelToRequestCredentials(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  uint32_t cookiePolicy = loadInfo->GetCookiePolicy();

  if (cookiePolicy == nsILoadInfo::SEC_COOKIES_INCLUDE) {
    return RequestCredentials::Include;
  } else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_OMIT) {
    return RequestCredentials::Omit;
  } else if (cookiePolicy == nsILoadInfo::SEC_COOKIES_SAME_ORIGIN) {
    return RequestCredentials::Same_origin;
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected cookie policy!");
  return RequestCredentials::Same_origin;
}

void InternalRequest::MaybeSkipCacheIfPerformingRevalidation() {
  if (mCacheMode == RequestCache::Default &&
      mHeaders->HasRevalidationHeaders()) {
    mCacheMode = RequestCache::No_store;
  }
}

void InternalRequest::SetPrincipalInfo(
    UniquePtr<mozilla::ipc::PrincipalInfo> aPrincipalInfo) {
  mPrincipalInfo = std::move(aPrincipalInfo);
}

}  // namespace dom
}  // namespace mozilla
