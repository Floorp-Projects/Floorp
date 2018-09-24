/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPingListener.h"

#include "mozilla/Preferences.h"

#include "mozilla/dom/DocGroup.h"

#include "nsIDocument.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInputStream.h"
#include "nsIProtocolHandler.h"
#include "nsIUploadChannel2.h"

#include "nsDocument.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsWhitespaceTokenizer.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsPingListener, nsIStreamListener, nsIRequestObserver)

//*****************************************************************************
// <a ping> support
//*****************************************************************************

#define PREF_PINGS_ENABLED           "browser.send_pings"
#define PREF_PINGS_MAX_PER_LINK      "browser.send_pings.max_per_link"
#define PREF_PINGS_REQUIRE_SAME_HOST "browser.send_pings.require_same_host"

// Check prefs to see if pings are enabled and if so what restrictions might
// be applied.
//
// @param maxPerLink
//   This parameter returns the number of pings that are allowed per link click
//
// @param requireSameHost
//   This parameter returns true if pings are restricted to the same host as
//   the document in which the click occurs.  If the same host restriction is
//   imposed, then we still allow for pings to cross over to different
//   protocols and ports for flexibility and because it is not possible to send
//   a ping via FTP.
//
// @returns
//   true if pings are enabled and false otherwise.
//
static bool
PingsEnabled(int32_t* aMaxPerLink, bool* aRequireSameHost)
{
  bool allow = Preferences::GetBool(PREF_PINGS_ENABLED, false);

  *aMaxPerLink = 1;
  *aRequireSameHost = true;

  if (allow) {
    Preferences::GetInt(PREF_PINGS_MAX_PER_LINK, aMaxPerLink);
    Preferences::GetBool(PREF_PINGS_REQUIRE_SAME_HOST, aRequireSameHost);
  }

  return allow;
}

// We wait this many milliseconds before killing the ping channel...
#define PING_TIMEOUT 10000

static void
OnPingTimeout(nsITimer* aTimer, void* aClosure)
{
  nsILoadGroup* loadGroup = static_cast<nsILoadGroup*>(aClosure);
  if (loadGroup) {
    loadGroup->Cancel(NS_ERROR_ABORT);
  }
}

struct MOZ_STACK_CLASS SendPingInfo
{
  int32_t numPings;
  int32_t maxPings;
  bool requireSameHost;
  nsIURI* target;
  nsIURI* referrer;
  nsIDocShell* docShell;
  uint32_t referrerPolicy;
};

static void
SendPing(void* aClosure, nsIContent* aContent, nsIURI* aURI,
         nsIIOService* aIOService)
{
  SendPingInfo* info = static_cast<SendPingInfo*>(aClosure);
  if (info->maxPings > -1 && info->numPings >= info->maxPings) {
    return;
  }

  nsIDocument* doc = aContent->OwnerDoc();

  nsCOMPtr<nsIChannel> chan;
  NS_NewChannel(getter_AddRefs(chan),
                aURI,
                doc,
                info->requireSameHost
                  ? nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED
                  : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                nsIContentPolicy::TYPE_PING,
                nullptr,   // PerformanceStorage
                nullptr, // aLoadGroup
                nullptr, // aCallbacks
                nsIRequest::LOAD_NORMAL, // aLoadFlags,
                aIOService);

  if (!chan) {
    return;
  }

  // Don't bother caching the result of this URI load, but do not exempt
  // it from Safe Browsing.
  chan->SetLoadFlags(nsIRequest::INHIBIT_CACHING | nsIChannel::LOAD_CLASSIFY_URI);

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (!httpChan) {
    return;
  }

  // This is needed in order for 3rd-party cookie blocking to work.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(httpChan);
  nsresult rv;
  if (httpInternal) {
    rv = httpInternal->SetDocumentURI(doc->GetDocumentURI());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  rv = httpChan->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Remove extraneous request headers (to reduce request size)
  rv = httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept"),
                                  EmptyCString(), false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-language"),
                                  EmptyCString(), false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-encoding"),
                                  EmptyCString(), false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Always send a Ping-To header.
  nsAutoCString pingTo;
  if (NS_SUCCEEDED(info->target->GetSpec(pingTo))) {
    rv = httpChan->SetRequestHeader(NS_LITERAL_CSTRING("Ping-To"), pingTo, false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  nsCOMPtr<nsIScriptSecurityManager> sm =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

  if (sm && info->referrer) {
    bool referrerIsSecure;
    uint32_t flags = nsIProtocolHandler::URI_IS_POTENTIALLY_TRUSTWORTHY;
    rv = NS_URIChainHasFlags(info->referrer, flags, &referrerIsSecure);

    // Default to sending less data if NS_URIChainHasFlags() fails.
    referrerIsSecure = NS_FAILED(rv) || referrerIsSecure;

    bool sameOrigin =
      NS_SUCCEEDED(sm->CheckSameOriginURI(info->referrer, aURI, false));

    // If both the address of the document containing the hyperlink being
    // audited and "ping URL" have the same origin or the document containing
    // the hyperlink being audited was not retrieved over an encrypted
    // connection, send a Ping-From header.
    if (sameOrigin || !referrerIsSecure) {
      nsAutoCString pingFrom;
      if (NS_SUCCEEDED(info->referrer->GetSpec(pingFrom))) {
        rv = httpChan->SetRequestHeader(NS_LITERAL_CSTRING("Ping-From"),
                                        pingFrom, false);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }

    // If the document containing the hyperlink being audited was not retrieved
    // over an encrypted connection and its address does not have the same
    // origin as "ping URL", send a referrer.
    if (!sameOrigin && !referrerIsSecure) {
      rv = httpChan->SetReferrerWithPolicy(info->referrer, info->referrerPolicy);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(httpChan);
  if (!uploadChan) {
    return;
  }

  NS_NAMED_LITERAL_CSTRING(uploadData, "PING");

  nsCOMPtr<nsIInputStream> uploadStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(uploadStream), uploadData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uploadChan->ExplicitSetUploadStream(uploadStream,
                                      NS_LITERAL_CSTRING("text/ping"),
                                      uploadData.Length(),
                                      NS_LITERAL_CSTRING("POST"), false);

  // The channel needs to have a loadgroup associated with it, so that we can
  // cancel the channel and any redirected channels it may create.
  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  if (!loadGroup) {
    return;
  }
  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryInterface(info->docShell);
  loadGroup->SetNotificationCallbacks(callbacks);
  chan->SetLoadGroup(loadGroup);

  RefPtr<nsPingListener> pingListener = new nsPingListener();
  chan->AsyncOpen2(pingListener);

  // Even if AsyncOpen failed, we still count this as a successful ping.  It's
  // possible that AsyncOpen may have failed after triggering some background
  // process that may have written something to the network.
  info->numPings++;

  // Prevent ping requests from stalling and never being garbage collected...
  if (NS_FAILED(pingListener->StartTimeout(doc->GetDocGroup()))) {
    // If we failed to setup the timer, then we should just cancel the channel
    // because we won't be able to ensure that it goes away in a timely manner.
    chan->Cancel(NS_ERROR_ABORT);
    return;
  }
  // if the channel openend successfully, then make the pingListener hold
  // a strong reference to the loadgroup which is released in ::OnStopRequest
  pingListener->SetLoadGroup(loadGroup);
}

typedef void (*ForEachPingCallback)(void* closure, nsIContent* content,
                                    nsIURI* uri, nsIIOService* ios);

static void
ForEachPing(nsIContent* aContent, ForEachPingCallback aCallback, void* aClosure)
{
  // NOTE: Using nsIDOMHTMLAnchorElement::GetPing isn't really worth it here
  //       since we'd still need to parse the resulting string.  Instead, we
  //       just parse the raw attribute.  It might be nice if the content node
  //       implemented an interface that exposed an enumeration of nsIURIs.

  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  if (!aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area)) {
    return;
  }

  nsAutoString value;
  aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::ping, value);
  if (value.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (!ios) {
    return;
  }

  nsIDocument* doc = aContent->OwnerDoc();
  nsAutoCString charset;
  doc->GetDocumentCharacterSet()->Name(charset);

  nsWhitespaceTokenizer tokenizer(value);

  while (tokenizer.hasMoreTokens()) {
    nsCOMPtr<nsIURI> uri, baseURI = aContent->GetBaseURI();
    ios->NewURI(NS_ConvertUTF16toUTF8(tokenizer.nextToken()),
                charset.get(), baseURI, getter_AddRefs(uri));
    // if we can't generate a valid URI, then there is nothing to do
    if (!uri) {
      continue;
    }
    // Explicitly not allow loading data: URIs
    bool isDataScheme =
      (NS_SUCCEEDED(uri->SchemeIs("data", &isDataScheme)) && isDataScheme);

    if (!isDataScheme) {
      aCallback(aClosure, aContent, uri, ios);
    }
  }
}

// Spec: http://whatwg.org/specs/web-apps/current-work/#ping
/*static*/ void
nsPingListener::DispatchPings(nsIDocShell* aDocShell,
                              nsIContent* aContent,
                              nsIURI* aTarget,
                              nsIURI* aReferrer,
                              uint32_t aReferrerPolicy)
{
  SendPingInfo info;

  if (!PingsEnabled(&info.maxPings, &info.requireSameHost)) {
    return;
  }
  if (info.maxPings == 0) {
    return;
  }

  info.numPings = 0;
  info.target = aTarget;
  info.referrer = aReferrer;
  info.referrerPolicy = aReferrerPolicy;
  info.docShell = aDocShell;

  ForEachPing(aContent, SendPing, &info);
}

nsPingListener::~nsPingListener()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

nsresult
nsPingListener::StartTimeout(DocGroup* aDocGroup)
{
  NS_ENSURE_ARG(aDocGroup);

  return NS_NewTimerWithFuncCallback(getter_AddRefs(mTimer),
                                     OnPingTimeout,
                                     mLoadGroup,
                                     PING_TIMEOUT,
                                     nsITimer::TYPE_ONE_SHOT,
                                     "nsPingListener::StartTimeout",
                                     aDocGroup->EventTargetFor(TaskCategory::Network));
}

NS_IMETHODIMP
nsPingListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPingListener::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                nsIInputStream* aStream, uint64_t aOffset,
                                uint32_t aCount)
{
  uint32_t result;
  return aStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &result);
}

NS_IMETHODIMP
nsPingListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                              nsresult aStatus)
{
  mLoadGroup = nullptr;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  return NS_OK;
}
