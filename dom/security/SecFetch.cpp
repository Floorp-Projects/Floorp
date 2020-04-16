/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SecFetch.h"
#include "nsIHttpChannel.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsIReferrerInfo.h"
#include "mozIThirdPartyUtil.h"
#include "nsMixedContentBlocker.h"
#include "nsNetUtil.h"

// Helper function which maps an internal content policy type
// to the corresponding destination for the context of SecFetch.
nsCString MapInternalContentPolicyTypeToDest(nsContentPolicyType aType) {
  switch (aType) {
    case nsIContentPolicy::TYPE_OTHER:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
    case nsIContentPolicy::TYPE_SCRIPT:
      return NS_LITERAL_CSTRING("script");
    case nsIContentPolicy::TYPE_INTERNAL_WORKER:
      return NS_LITERAL_CSTRING("worker");
    case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
      return NS_LITERAL_CSTRING("sharedworker");
    case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
      return NS_LITERAL_CSTRING("serviceworker");
    case nsIContentPolicy::TYPE_INTERNAL_AUDIOWORKLET:
      return NS_LITERAL_CSTRING("audioworklet");
    case nsIContentPolicy::TYPE_INTERNAL_PAINTWORKLET:
      return NS_LITERAL_CSTRING("paintworklet");
    case nsIContentPolicy::TYPE_IMAGESET:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON:
    case nsIContentPolicy::TYPE_IMAGE:
      return NS_LITERAL_CSTRING("image");
    case nsIContentPolicy::TYPE_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET:
    case nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD:
      return NS_LITERAL_CSTRING("style");
    case nsIContentPolicy::TYPE_OBJECT:
    case nsIContentPolicy::TYPE_INTERNAL_OBJECT:
      return NS_LITERAL_CSTRING("object");
    case nsIContentPolicy::TYPE_INTERNAL_EMBED:
      return NS_LITERAL_CSTRING("embed");
    case nsIContentPolicy::TYPE_DOCUMENT:
      return NS_LITERAL_CSTRING("document");
    case nsIContentPolicy::TYPE_SUBDOCUMENT:
    case nsIContentPolicy::TYPE_INTERNAL_IFRAME:
      return NS_LITERAL_CSTRING("iframe");
    case nsIContentPolicy::TYPE_INTERNAL_FRAME:
      return NS_LITERAL_CSTRING("frame");
    case nsIContentPolicy::TYPE_REFRESH:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_XBL:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_PING:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_XMLHTTPREQUEST:
    case nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_OBJECT_SUBREQUEST:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_DTD:
    case nsIContentPolicy::TYPE_INTERNAL_DTD:
    case nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_FONT:
      return NS_LITERAL_CSTRING("font");
    case nsIContentPolicy::TYPE_MEDIA:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_INTERNAL_AUDIO:
      return NS_LITERAL_CSTRING("audio");
    case nsIContentPolicy::TYPE_INTERNAL_VIDEO:
      return NS_LITERAL_CSTRING("video");
    case nsIContentPolicy::TYPE_INTERNAL_TRACK:
      return NS_LITERAL_CSTRING("track");
    case nsIContentPolicy::TYPE_WEBSOCKET:
      return NS_LITERAL_CSTRING("websocket");
    case nsIContentPolicy::TYPE_CSP_REPORT:
      return NS_LITERAL_CSTRING("report");
    case nsIContentPolicy::TYPE_XSLT:
      return NS_LITERAL_CSTRING("xslt");
    case nsIContentPolicy::TYPE_BEACON:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_FETCH:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_WEB_MANIFEST:
      return NS_LITERAL_CSTRING("manifest");
    case nsIContentPolicy::TYPE_SAVEAS_DOWNLOAD:
      return NS_LITERAL_CSTRING("empty");
    case nsIContentPolicy::TYPE_SPECULATIVE:
      return NS_LITERAL_CSTRING("empty");
    default:
      MOZ_CRASH("Unhandled nsContentPolicyType value");
      break;
  }

  return NS_LITERAL_CSTRING("empty");
}

// Helper function to determine whether a request (including involved
// redirects) is same-origin in the context of SecFetch.
bool IsSameOrigin(nsIHttpChannel* aHTTPChannel) {
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aHTTPChannel, getter_AddRefs(channelURI));

  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  bool isPrivateWin = loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  bool isSameOrigin = false;
  nsresult rv = loadInfo->TriggeringPrincipal()->IsSameOrigin(
      channelURI, isPrivateWin, &isSameOrigin);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  // if the initial request is not same-origin, we can return here
  // because we already know it's not a same-origin request
  if (!isSameOrigin) {
    return false;
  }

  // let's further check all the hoops in the redirectChain to
  // ensure all involved redirects are same-origin
  nsCOMPtr<nsIPrincipal> redirectPrincipal;
  for (nsIRedirectHistoryEntry* entry : loadInfo->RedirectChain()) {
    entry->GetPrincipal(getter_AddRefs(redirectPrincipal));
    if (redirectPrincipal) {
      rv = redirectPrincipal->IsSameOrigin(channelURI, isPrivateWin,
                                           &isSameOrigin);
      Unused << NS_WARN_IF(NS_FAILED(rv));
      if (!isSameOrigin) {
        return false;
      }
    }
  }

  // must be a same-origin request
  return true;
}

// Helper function to determine whether a request (including involved
// redirects) is same-site in the context of SecFetch.
bool IsSameSite(nsIChannel* aHTTPChannel) {
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (!thirdPartyUtil) {
    return false;
  }

  nsAutoCString hostDomain;
  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  nsresult rv = loadInfo->TriggeringPrincipal()->GetBaseDomain(hostDomain);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  nsAutoCString channelDomain;
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aHTTPChannel, getter_AddRefs(channelURI));
  rv = thirdPartyUtil->GetBaseDomain(channelURI, channelDomain);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  // if the initial request is not same-site, or not https, we can
  // return here because we already know it's not a same-site request
  if (!hostDomain.Equals(channelDomain) ||
      !loadInfo->TriggeringPrincipal()->SchemeIs("https")) {
    return false;
  }

  // let's further check all the hoops in the redirectChain to
  // ensure all involved redirects are same-site and https
  nsCOMPtr<nsIPrincipal> redirectPrincipal;
  for (nsIRedirectHistoryEntry* entry : loadInfo->RedirectChain()) {
    entry->GetPrincipal(getter_AddRefs(redirectPrincipal));
    if (redirectPrincipal) {
      redirectPrincipal->GetBaseDomain(hostDomain);
      if (!hostDomain.Equals(channelDomain) ||
          !redirectPrincipal->SchemeIs("https")) {
        return false;
      }
    }
  }

  // must be a same-site request
  return true;
}

// Helper function to determine whether a request was triggered
// by the end user in the context of SecFetch.
bool IsUserTriggeredForSecFetchSite(nsIHttpChannel* aHTTPChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  nsContentPolicyType contentType = loadInfo->InternalContentPolicyType();

  // only requests wich result in type "document" are subject to
  // user initiated actions in the context of SecFetch.
  if (contentType != nsIContentPolicy::TYPE_DOCUMENT &&
      contentType != nsIContentPolicy::TYPE_SUBDOCUMENT &&
      contentType != nsIContentPolicy::TYPE_INTERNAL_IFRAME) {
    return false;
  }

  // All web requests have a valid "original" referrer set in the
  // ReferrerInfo which we can use to determine whether a request
  // was triggered by a user or not.
  nsCOMPtr<nsIReferrerInfo> referrerInfo = aHTTPChannel->GetReferrerInfo();
  if (referrerInfo) {
    nsCOMPtr<nsIURI> originalReferrer;
    referrerInfo->GetOriginalReferrer(getter_AddRefs(originalReferrer));
    if (originalReferrer) {
      return false;
    }
  }

  return true;
}

void SecFetch::AddSecFetchDest(nsIHttpChannel* aHTTPChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  nsContentPolicyType contentType = loadInfo->InternalContentPolicyType();
  nsCString dest = MapInternalContentPolicyTypeToDest(contentType);

  nsresult rv = aHTTPChannel->SetRequestHeader(
      NS_LITERAL_CSTRING("Sec-Fetch-Dest"), dest, false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SecFetch::AddSecFetchMode(nsIHttpChannel* aHTTPChannel) {
  nsAutoCString mode("no-cors");

  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  uint32_t securityMode = loadInfo->GetSecurityMode();
  nsContentPolicyType externalType = loadInfo->GetExternalContentPolicyType();

  if (securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS ||
      securityMode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED) {
    mode = NS_LITERAL_CSTRING("same-origin");
  } else if (securityMode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    mode = NS_LITERAL_CSTRING("cors");
  } else {
    // If it's not one of the security modes above, then we ensure it's
    // at least one of the others defined in nsILoadInfo
    MOZ_ASSERT(
        securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS ||
            securityMode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        "unhandled security mode");
  }

  if (externalType == nsIContentPolicy::TYPE_DOCUMENT ||
      externalType == nsIContentPolicy::TYPE_SUBDOCUMENT ||
      externalType == nsIContentPolicy::TYPE_REFRESH ||
      externalType == nsIContentPolicy::TYPE_OBJECT) {
    mode = NS_LITERAL_CSTRING("navigate");
  } else if (externalType == nsIContentPolicy::TYPE_WEBSOCKET) {
    mode = NS_LITERAL_CSTRING("websocket");
  }

  nsresult rv = aHTTPChannel->SetRequestHeader(
      NS_LITERAL_CSTRING("Sec-Fetch-Mode"), mode, false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SecFetch::AddSecFetchSite(nsIHttpChannel* aHTTPChannel) {
  nsAutoCString site("same-origin");

  bool isSameOrigin = IsSameOrigin(aHTTPChannel);
  if (!isSameOrigin) {
    bool isSameSite = IsSameSite(aHTTPChannel);
    if (isSameSite) {
      site = NS_LITERAL_CSTRING("same-site");
    } else {
      site = NS_LITERAL_CSTRING("cross-site");
    }
  }

  if (IsUserTriggeredForSecFetchSite(aHTTPChannel)) {
    site = NS_LITERAL_CSTRING("none");
  }

  nsresult rv = aHTTPChannel->SetRequestHeader(
      NS_LITERAL_CSTRING("Sec-Fetch-Site"), site, false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SecFetch::AddSecFetchUser(nsIHttpChannel* aHTTPChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aHTTPChannel->LoadInfo();
  nsContentPolicyType externalType = loadInfo->GetExternalContentPolicyType();

  // sec-fetch-user only applies to loads of type document or subdocument
  if (externalType != nsIContentPolicy::TYPE_DOCUMENT &&
      externalType != nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return;
  }

  // sec-fetch-user only applies if the request is user triggered
  if (!loadInfo->GetHasValidUserGestureActivation()) {
    return;
  }

  nsAutoCString user("?1");
  nsresult rv = aHTTPChannel->SetRequestHeader(
      NS_LITERAL_CSTRING("Sec-Fetch-User"), user, false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SecFetch::AddSecFetchHeader(nsIHttpChannel* aHTTPChannel) {
  // if sec-fetch-* is prefed off, then there is nothing to do
  if (!StaticPrefs::dom_security_secFetch_enabled()) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aHTTPChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // if we are not dealing with a potentially trustworthy URL, then
  // there is nothing to do here
  if (!nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(uri)) {
    return;
  }

  AddSecFetchDest(aHTTPChannel);
  AddSecFetchMode(aHTTPChannel);
  AddSecFetchSite(aHTTPChannel);
  AddSecFetchUser(aHTTPChannel);
}
