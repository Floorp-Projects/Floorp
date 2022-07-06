/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#include "nsContentSink.h"
#include "mozilla/Components.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_content.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/LinkStyle.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsIDocShell.h"
#include "nsILoadContext.h"
#include "nsIPrefetchService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIProtocolHandler.h"
#include "nsIHttpChannel.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsViewManager.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindowInner.h"
#include "nsNetCID.h"
#include "nsICookieService.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "mozAutoDocUpdate.h"
#include "nsIWebNavigation.h"
#include "nsGenericHTMLElement.h"
#include "nsIObserverService.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/dom/HTMLDNSPrefetch.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsParserConstants.h"
#include "nsSandboxFlags.h"
#include "Link.h"
#include "HTMLLinkElement.h"
using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

LazyLogModule gContentSinkLogModuleInfo("nscontentsink");

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsContentSink)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsContentSink)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsContentSink)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsContentSink)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsContentSink)
  if (tmp->mDocument) {
    tmp->mDocument->RemoveObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCSSLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCSSLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfoManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsContentSink::nsContentSink()
    : mBackoffCount(0),
      mLastNotificationTime(0),
      mLayoutStarted(0),
      mDynamicLowerValue(0),
      mParsing(0),
      mDroppedTimer(0),
      mDeferredLayoutStart(0),
      mDeferredFlushTags(0),
      mIsDocumentObserver(0),
      mRunsToCompletion(0),
      mIsBlockingOnload(false),
      mDeflectedCount(0),
      mHasPendingEvent(false),
      mCurrentParseEndTime(0),
      mBeginLoadTime(0),
      mLastSampledUserEventTime(0),
      mInMonolithicContainer(0),
      mInNotification(0),
      mUpdatesInNotification(0),
      mPendingSheetCount(0) {
  NS_ASSERTION(!mLayoutStarted, "What?");
  NS_ASSERTION(!mDynamicLowerValue, "What?");
  NS_ASSERTION(!mParsing, "What?");
  NS_ASSERTION(mLastSampledUserEventTime == 0, "What?");
  NS_ASSERTION(mDeflectedCount == 0, "What?");
  NS_ASSERTION(!mDroppedTimer, "What?");
  NS_ASSERTION(mInMonolithicContainer == 0, "What?");
  NS_ASSERTION(mInNotification == 0, "What?");
  NS_ASSERTION(!mDeferredLayoutStart, "What?");
}

nsContentSink::~nsContentSink() {
  if (mDocument) {
    // Remove ourselves just to be safe, though we really should have
    // been removed in DidBuildModel if everything worked right.
    mDocument->RemoveObserver(this);
  }
}

nsresult nsContentSink::Init(Document* aDoc, nsIURI* aURI,
                             nsISupports* aContainer, nsIChannel* aChannel) {
  MOZ_ASSERT(aDoc, "null ptr");
  MOZ_ASSERT(aURI, "null ptr");

  if (!aDoc || !aURI) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;

  mDocumentURI = aURI;
  mDocShell = do_QueryInterface(aContainer);
  mScriptLoader = mDocument->ScriptLoader();

  if (!mRunsToCompletion) {
    if (mDocShell) {
      uint32_t loadType = 0;
      mDocShell->GetLoadType(&loadType);
      mDocument->SetChangeScrollPosWhenScrollingToRef(
          (loadType & nsIDocShell::LOAD_CMD_HISTORY) == 0);
    }

    ProcessHTTPHeaders(aChannel);
  }

  mCSSLoader = aDoc->CSSLoader();

  mNodeInfoManager = aDoc->NodeInfoManager();

  mBackoffCount = StaticPrefs::content_notify_backoffcount();

  if (StaticPrefs::content_sink_enable_perf_mode() != 0) {
    mDynamicLowerValue = StaticPrefs::content_sink_enable_perf_mode() == 1;
    FavorPerformanceHint(!mDynamicLowerValue, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentSink::StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                                nsresult aStatus) {
  MOZ_ASSERT(!mRunsToCompletion, "How come a fragment parser observed sheets?");
  if (aWasDeferred) {
    return NS_OK;
  }
  MOZ_ASSERT(mPendingSheetCount > 0, "How'd that happen?");
  --mPendingSheetCount;

  const bool loadedAllSheets = !mPendingSheetCount;
  if (loadedAllSheets && (mDeferredLayoutStart || mDeferredFlushTags)) {
    if (mDeferredFlushTags) {
      FlushTags();
    }
    if (mDeferredLayoutStart) {
      // We might not have really started layout, since this sheet was still
      // loading.  Do it now.  Probably doesn't matter whether we do this
      // before or after we unblock scripts, but before feels saner.  Note
      // that if mDeferredLayoutStart is true, that means any subclass
      // StartLayout() stuff that needs to happen has already happened, so
      // we don't need to worry about it.
      StartLayout(false);
    }

    // Go ahead and try to scroll to our ref if we have one
    ScrollToRef();
  }

  mScriptLoader->RemoveParserBlockingScriptExecutionBlocker();

  if (loadedAllSheets &&
      mDocument->GetReadyStateEnum() >= Document::READYSTATE_INTERACTIVE) {
    mScriptLoader->DeferCheckpointReached();
  }

  return NS_OK;
}

nsresult nsContentSink::ProcessHTTPHeaders(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannel> httpchannel(do_QueryInterface(aChannel));

  if (!httpchannel) {
    return NS_OK;
  }

  // Note that the only header we care about is the "link" header, since we
  // have all the infrastructure for kicking off stylesheet loads.

  nsAutoCString linkHeader;

  nsresult rv = httpchannel->GetResponseHeader("link"_ns, linkHeader);
  if (NS_SUCCEEDED(rv) && !linkHeader.IsEmpty()) {
    mDocument->SetHeaderData(nsGkAtoms::link,
                             NS_ConvertASCIItoUTF16(linkHeader));

    NS_ASSERTION(!mProcessLinkHeaderEvent.get(),
                 "Already dispatched an event?");

    mProcessLinkHeaderEvent =
        NewNonOwningRunnableMethod("nsContentSink::DoProcessLinkHeader", this,
                                   &nsContentSink::DoProcessLinkHeader);
    rv = NS_DispatchToCurrentThread(mProcessLinkHeaderEvent.get());
    if (NS_FAILED(rv)) {
      mProcessLinkHeaderEvent.Forget();
    }
  }

  return NS_OK;
}

void nsContentSink::DoProcessLinkHeader() {
  nsAutoString value;
  mDocument->GetHeaderData(nsGkAtoms::link, value);
  auto linkHeaders = net::ParseLinkHeader(value);
  for (const auto& linkHeader : linkHeaders) {
    ProcessLinkFromHeader(linkHeader);
  }
}

// check whether the Link header field applies to the context resource
// see <http://tools.ietf.org/html/rfc5988#section-5.2>

bool nsContentSink::LinkContextIsOurDocument(const nsAString& aAnchor) {
  if (aAnchor.IsEmpty()) {
    // anchor parameter not present or empty -> same document reference
    return true;
  }

  nsIURI* docUri = mDocument->GetDocumentURI();

  // the document URI might contain a fragment identifier ("#...')
  // we want to ignore that because it's invisible to the server
  // and just affects the local interpretation in the recipient
  nsCOMPtr<nsIURI> contextUri;
  nsresult rv = NS_GetURIWithoutRef(docUri, getter_AddRefs(contextUri));

  if (NS_FAILED(rv)) {
    // copying failed
    return false;
  }

  // resolve anchor against context
  nsCOMPtr<nsIURI> resolvedUri;
  rv = NS_NewURI(getter_AddRefs(resolvedUri), aAnchor, nullptr, contextUri);

  if (NS_FAILED(rv)) {
    // resolving failed
    return false;
  }

  bool same;
  rv = contextUri->Equals(resolvedUri, &same);
  if (NS_FAILED(rv)) {
    // comparison failed
    return false;
  }

  return same;
}

nsresult nsContentSink::ProcessLinkFromHeader(const net::LinkHeader& aHeader) {
  uint32_t linkTypes = LinkStyle::ParseLinkTypes(aHeader.mRel);

  // The link relation may apply to a different resource, specified
  // in the anchor parameter. For the link relations supported so far,
  // we simply abort if the link applies to a resource different to the
  // one we've loaded
  if (!LinkContextIsOurDocument(aHeader.mAnchor)) {
    return NS_OK;
  }

  if (nsContentUtils::PrefetchPreloadEnabled(mDocShell)) {
    // prefetch href if relation is "next" or "prefetch"
    if ((linkTypes & LinkStyle::eNEXT) || (linkTypes & LinkStyle::ePREFETCH)) {
      PrefetchHref(aHeader.mHref, aHeader.mAs, aHeader.mType, aHeader.mMedia);
    }

    if (!aHeader.mHref.IsEmpty() && (linkTypes & LinkStyle::eDNS_PREFETCH)) {
      PrefetchDNS(aHeader.mHref);
    }

    if (!aHeader.mHref.IsEmpty() && (linkTypes & LinkStyle::ePRECONNECT)) {
      Preconnect(aHeader.mHref, aHeader.mCrossOrigin);
    }

    if (linkTypes & LinkStyle::ePRELOAD) {
      PreloadHref(aHeader.mHref, aHeader.mAs, aHeader.mType, aHeader.mMedia,
                  aHeader.mIntegrity, aHeader.mSrcset, aHeader.mSizes,
                  aHeader.mCrossOrigin, aHeader.mReferrerPolicy);
    }

    if (linkTypes & LinkStyle::eMODULE_PRELOAD) {
      // https://wicg.github.io/import-maps/#wait-for-import-maps
      // Step 1.2: Set document’s acquiring import maps to false.
      // When fetch a modulepreload module script graph.
      mDocument->ScriptLoader()->GetModuleLoader()->SetAcquiringImportMaps(
          false);
    }
  }

  // is it a stylesheet link?
  if (!(linkTypes & LinkStyle::eSTYLESHEET)) {
    return NS_OK;
  }

  bool isAlternate = linkTypes & LinkStyle::eALTERNATE;
  return ProcessStyleLinkFromHeader(aHeader.mHref, isAlternate, aHeader.mTitle,
                                    aHeader.mIntegrity, aHeader.mType,
                                    aHeader.mMedia, aHeader.mReferrerPolicy);
}

nsresult nsContentSink::ProcessStyleLinkFromHeader(
    const nsAString& aHref, bool aAlternate, const nsAString& aTitle,
    const nsAString& aIntegrity, const nsAString& aType,
    const nsAString& aMedia, const nsAString& aReferrerPolicy) {
  if (aAlternate && aTitle.IsEmpty()) {
    // alternates must have title return without error, for now
    return NS_OK;
  }

  nsAutoString mimeType;
  nsAutoString params;
  nsContentUtils::SplitMimeType(aType, mimeType, params);

  // see bug 18817
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    // Unknown stylesheet language
    return NS_OK;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aHref, nullptr,
                          mDocument->GetDocBaseURI());

  if (NS_FAILED(rv)) {
    // The URI is bad, move along, don't propagate the error (for now)
    return NS_OK;
  }

  // Link header is working like a <link> node, so referrerPolicy attr should
  // have higher priority than referrer policy from document.
  ReferrerPolicy policy =
      ReferrerInfo::ReferrerPolicyAttributeFromString(aReferrerPolicy);
  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      ReferrerInfo::CreateFromDocumentAndPolicyOverride(mDocument, policy);

  Loader::SheetInfo info{
      *mDocument,
      nullptr,
      url.forget(),
      nullptr,
      referrerInfo.forget(),
      CORS_NONE,
      aTitle,
      aMedia,
      aIntegrity,
      /* nonce = */ u""_ns,
      aAlternate ? Loader::HasAlternateRel::Yes : Loader::HasAlternateRel::No,
      Loader::IsInline::No,
      Loader::IsExplicitlyEnabled::No,
  };

  auto loadResultOrErr =
      mCSSLoader->LoadStyleLink(info, mRunsToCompletion ? nullptr : this);
  if (loadResultOrErr.isErr()) {
    return loadResultOrErr.unwrapErr();
  }

  if (loadResultOrErr.inspect().ShouldBlock() && !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddParserBlockingScriptExecutionBlocker();
  }

  return NS_OK;
}

void nsContentSink::PrefetchHref(const nsAString& aHref, const nsAString& aAs,
                                 const nsAString& aType,
                                 const nsAString& aMedia) {
  nsCOMPtr<nsIPrefetchService> prefetchService(components::Prefetch::Service());
  if (prefetchService) {
    // construct URI using document charset
    auto encoding = mDocument->GetDocumentCharacterSet();
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref, encoding, mDocument->GetDocBaseURI());
    if (uri) {
      auto referrerInfo = MakeRefPtr<ReferrerInfo>(*mDocument);
      referrerInfo = referrerInfo->CloneWithNewOriginalReferrer(mDocumentURI);

      prefetchService->PrefetchURI(uri, referrerInfo, mDocument, true);
    }
  }
}

void nsContentSink::PreloadHref(const nsAString& aHref, const nsAString& aAs,
                                const nsAString& aType, const nsAString& aMedia,
                                const nsAString& aIntegrity,
                                const nsAString& aSrcset,
                                const nsAString& aSizes, const nsAString& aCORS,
                                const nsAString& aReferrerPolicy) {
  auto encoding = mDocument->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aHref, encoding, mDocument->GetDocBaseURI());
  if (!uri) {
    // URL parsing failed.
    return;
  }

  nsAttrValue asAttr;
  mozilla::net::ParseAsValue(aAs, asAttr);

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsContentUtils::SplitMimeType(aType, mimeType, notUsed);

  auto policyType = mozilla::net::AsValueToContentPolicy(asAttr);
  if (policyType == nsIContentPolicy::TYPE_INVALID ||
      !mozilla::net::CheckPreloadAttrs(asAttr, mimeType, aMedia, mDocument)) {
    // Ignore preload wrong or empty attributes.
    mozilla::net::WarnIgnoredPreload(*mDocument, *uri);
    return;
  }

  mDocument->Preloads().PreloadLinkHeader(uri, aHref, policyType, aAs, aType,
                                          aIntegrity, aSrcset, aSizes, aCORS,
                                          aReferrerPolicy);
}

void nsContentSink::PrefetchDNS(const nsAString& aHref) {
  nsAutoString hostname;
  bool isHttps = false;

  if (StringBeginsWith(aHref, u"//"_ns)) {
    hostname = Substring(aHref, 2);
  } else {
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref);
    if (!uri) {
      return;
    }
    nsresult rv;
    bool isLocalResource = false;
    rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                             &isLocalResource);
    if (NS_SUCCEEDED(rv) && !isLocalResource) {
      nsAutoCString host;
      uri->GetHost(host);
      CopyUTF8toUTF16(host, hostname);
    }
    isHttps = uri->SchemeIs("https");
  }

  if (!hostname.IsEmpty() && HTMLDNSPrefetch::IsAllowed(mDocument)) {
    OriginAttributes oa;
    StoragePrincipalHelper::GetOriginAttributesForNetworkState(mDocument, oa);

    HTMLDNSPrefetch::Prefetch(hostname, isHttps, oa,
                              mDocument->GetChannel()->GetTRRMode(),
                              HTMLDNSPrefetch::Priority::Low);
  }
}

void nsContentSink::Preconnect(const nsAString& aHref,
                               const nsAString& aCrossOrigin) {
  // construct URI using document charset
  auto encoding = mDocument->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aHref, encoding, mDocument->GetDocBaseURI());

  if (uri && mDocument) {
    mDocument->MaybePreconnect(uri,
                               dom::Element::StringToCORSMode(aCrossOrigin));
  }
}

void nsContentSink::ScrollToRef() {
  RefPtr<Document> document = mDocument;
  document->ScrollToRef();
}

void nsContentSink::StartLayout(bool aIgnorePendingSheets) {
  if (mLayoutStarted) {
    // Nothing to do here
    return;
  }

  mDeferredLayoutStart = true;

  if (!aIgnorePendingSheets &&
      (WaitForPendingSheets() || mDocument->HasPendingInitialTranslation())) {
    // Bail out; we'll start layout when the sheets and l10n load
    return;
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_RELEVANT_FOR_JS(
      "Layout", LAYOUT, mDocumentURI->GetSpecOrDefault());

  mDeferredLayoutStart = false;

  if (aIgnorePendingSheets) {
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, "Layout"_ns, mDocument,
        nsContentUtils::eLAYOUT_PROPERTIES, "ForcedLayoutStart");
  }

  // Notify on all our content.  If none of our presshells have started layout
  // yet it'll be a no-op except for updating our data structures, a la
  // UpdateChildCounts() (because we don't want to double-notify on whatever we
  // have right now).  If some of them _have_ started layout, we want to make
  // sure to flush tags instead of just calling UpdateChildCounts() after we
  // loop over the shells.
  FlushTags();

  mLayoutStarted = true;
  mLastNotificationTime = PR_Now();

  mDocument->SetMayStartLayout(true);
  RefPtr<PresShell> presShell = mDocument->GetPresShell();
  // Make sure we don't call Initialize() for a shell that has
  // already called it. This can happen when the layout frame for
  // an iframe is constructed *between* the Embed() call for the
  // docshell in the iframe, and the content sink's call to OpenBody().
  // (Bug 153815)
  if (presShell && !presShell->DidInitialize()) {
    nsresult rv = presShell->Initialize();
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  mDocument->SetScrollToRef(mDocument->GetDocumentURI());
}

void nsContentSink::NotifyAppend(nsIContent* aContainer, uint32_t aStartIndex) {
  mInNotification++;

  {
    // Scope so we call EndUpdate before we decrease mInNotification
    //
    // Note that aContainer->OwnerDoc() may not be mDocument.
    MOZ_AUTO_DOC_UPDATE(aContainer->OwnerDoc(), true);
    MutationObservers::NotifyContentAppended(
        aContainer, aContainer->GetChildAt_Deprecated(aStartIndex));
    mLastNotificationTime = PR_Now();
  }

  mInNotification--;
}

NS_IMETHODIMP
nsContentSink::Notify(nsITimer* timer) {
  if (mParsing) {
    // We shouldn't interfere with our normal DidProcessAToken logic
    mDroppedTimer = true;
    return NS_OK;
  }

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
  } else {
    FlushTags();

    // Now try and scroll to the reference
    // XXX Should we scroll unconditionally for history loads??
    ScrollToRef();
  }

  mNotificationTimer = nullptr;
  return NS_OK;
}

bool nsContentSink::IsTimeToNotify() {
  if (!StaticPrefs::content_notify_ontimer() || !mLayoutStarted ||
      !mBackoffCount || mInMonolithicContainer) {
    return false;
  }

  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
    return false;
  }

  PRTime now = PR_Now();

  int64_t interval = GetNotificationInterval();
  int64_t diff = now - mLastNotificationTime;

  if (diff > interval) {
    mBackoffCount--;
    return true;
  }

  return false;
}

nsresult nsContentSink::WillInterruptImpl() {
  nsresult result = NS_OK;

  SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
             SINK_TRACE_CALLS, ("nsContentSink::WillInterrupt: this=%p", this));
#ifndef SINK_NO_INCREMENTAL
  if (WaitForPendingSheets()) {
    mDeferredFlushTags = true;
  } else if (StaticPrefs::content_notify_ontimer() && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      int64_t now = PR_Now();
      int64_t interval = GetNotificationInterval();
      int64_t diff = now - mLastNotificationTime;

      // If it's already time for us to have a notification
      if (diff > interval || mDroppedTimer) {
        mBackoffCount--;
        SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
                   SINK_TRACE_REFLOW,
                   ("nsContentSink::WillInterrupt: flushing tags since we've "
                    "run out time; backoff count: %d",
                    mBackoffCount));
        result = FlushTags();
        if (mDroppedTimer) {
          ScrollToRef();
          mDroppedTimer = false;
        }
      } else if (!mNotificationTimer) {
        interval -= diff;
        int32_t delay = interval;

        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;

        NS_NewTimerWithCallback(getter_AddRefs(mNotificationTimer), this, delay,
                                nsITimer::TYPE_ONE_SHOT);
        if (mNotificationTimer) {
          SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
                     SINK_TRACE_REFLOW,
                     ("nsContentSink::WillInterrupt: setting up timer with "
                      "delay %d",
                      delay));
        }
      }
    }
  } else {
    SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
               SINK_TRACE_REFLOW,
               ("nsContentSink::WillInterrupt: flushing tags "
                "unconditionally"));
    result = FlushTags();
  }
#endif

  mParsing = false;

  return result;
}

void nsContentSink::WillResumeImpl() {
  SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
             SINK_TRACE_CALLS, ("nsContentSink::WillResume: this=%p", this));

  mParsing = true;
}

nsresult nsContentSink::DidProcessATokenImpl() {
  if (mRunsToCompletion || !mParser) {
    return NS_OK;
  }

  // Get the current user event time
  PresShell* presShell = mDocument->GetPresShell();
  if (!presShell) {
    // If there's no pres shell in the document, return early since
    // we're not laying anything out here.
    return NS_OK;
  }

  // Increase before comparing to gEventProbeRate
  ++mDeflectedCount;

  // Check if there's a pending event
  if (StaticPrefs::content_sink_pending_event_mode() != 0 &&
      !mHasPendingEvent &&
      (mDeflectedCount % StaticPrefs::content_sink_event_probe_rate()) == 0) {
    nsViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    nsCOMPtr<nsIWidget> widget = vm->GetRootWidget();
    mHasPendingEvent = widget && widget->HasPendingInputEvent();
  }

  if (mHasPendingEvent && StaticPrefs::content_sink_pending_event_mode() == 2) {
    return NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  // Have we processed enough tokens to check time?
  if (!mHasPendingEvent &&
      mDeflectedCount <
          uint32_t(mDynamicLowerValue
                       ? StaticPrefs::content_sink_interactive_deflect_count()
                       : StaticPrefs::content_sink_perf_deflect_count())) {
    return NS_OK;
  }

  mDeflectedCount = 0;

  // Check if it's time to return to the main event loop
  if (PR_IntervalToMicroseconds(PR_IntervalNow()) > mCurrentParseEndTime) {
    return NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  return NS_OK;
}

//----------------------------------------------------------------------

void nsContentSink::FavorPerformanceHint(bool perfOverStarvation,
                                         uint32_t starvationDelay) {
  static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell)
    appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
}

void nsContentSink::BeginUpdate(Document* aDocument) {
  // Remember nested updates from updates that we started.
  if (mInNotification > 0 && mUpdatesInNotification < 2) {
    ++mUpdatesInNotification;
  }

  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Since this could result in frame
  // creation, make sure we've flushed everything before we
  // continue.

  if (!mInNotification++) {
    FlushTags();
  }
}

void nsContentSink::EndUpdate(Document* aDocument) {
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Update our notion of how much
  // has been flushed to include any new content if ending
  // this update leaves us not inside a notification.
  if (!--mInNotification) {
    UpdateChildCounts();
  }
}

void nsContentSink::DidBuildModelImpl(bool aTerminated) {
  MOZ_ASSERT(aTerminated ||
                 mDocument->GetReadyStateEnum() == Document::READYSTATE_LOADING,
             "Bad readyState");
  mDocument->SetReadyStateInternal(Document::READYSTATE_INTERACTIVE);

  if (mScriptLoader) {
    mScriptLoader->ParsingComplete(aTerminated);
    if (!mPendingSheetCount) {
      mScriptLoader->DeferCheckpointReached();
    }
  }

  if (!mDocument->HaveFiredDOMTitleChange()) {
    mDocument->NotifyPossibleTitleChange(false);
  }

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(static_cast<LogModule*>(gContentSinkLogModuleInfo),
               SINK_TRACE_REFLOW,
               ("nsContentSink::DidBuildModel: canceling notification "
                "timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = nullptr;
  }
}

void nsContentSink::DropParserAndPerfHint(void) {
  if (!mParser) {
    // Make sure we don't unblock unload too many times
    return;
  }

  // Ref. Bug 49115
  // Do this hack to make sure that the parser
  // doesn't get destroyed, accidently, before
  // the circularity, between sink & parser, is
  // actually broken.
  // Drop our reference to the parser to get rid of a circular
  // reference.
  RefPtr<nsParserBase> kungFuDeathGrip = std::move(mParser);
  mozilla::Unused << kungFuDeathGrip;

  if (mDynamicLowerValue) {
    // Reset the performance hint which was set to FALSE
    // when mDynamicLowerValue was set.
    FavorPerformanceHint(true, 0);
  }

  // Call UnblockOnload only if mRunsToComletion is false and if
  // we have already started loading because it's possible that this function
  // is called (i.e. the parser is terminated) before we start loading due to
  // destroying the window inside unload event callbacks for the previous
  // document.
  if (!mRunsToCompletion && mIsBlockingOnload) {
    mDocument->UnblockOnload(true);
    mIsBlockingOnload = false;
  }
}

bool nsContentSink::IsScriptExecutingImpl() {
  return !!mScriptLoader->GetCurrentScript();
}

nsresult nsContentSink::WillParseImpl(void) {
  if (mRunsToCompletion || !mDocument) {
    return NS_OK;
  }

  PresShell* presShell = mDocument->GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  uint32_t currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  if (StaticPrefs::content_sink_enable_perf_mode() == 0) {
    nsViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    uint32_t lastEventTime;
    vm->GetLastUserEventTime(lastEventTime);

    bool newDynLower = mDocument->IsInBackgroundWindow() ||
                       ((currentTime - mBeginLoadTime) >
                            StaticPrefs::content_sink_initial_perf_time() &&
                        (currentTime - lastEventTime) <
                            StaticPrefs::content_sink_interactive_time());

    if (mDynamicLowerValue != newDynLower) {
      FavorPerformanceHint(!newDynLower, 0);
      mDynamicLowerValue = newDynLower;
    }
  }

  mDeflectedCount = 0;
  mHasPendingEvent = false;

  mCurrentParseEndTime =
      currentTime + (mDynamicLowerValue
                         ? StaticPrefs::content_sink_interactive_parse_time()
                         : StaticPrefs::content_sink_perf_parse_time());

  return NS_OK;
}

void nsContentSink::WillBuildModelImpl() {
  if (!mRunsToCompletion) {
    mDocument->BlockOnload();
    mIsBlockingOnload = true;

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mDocument->ResetScrolledToRefAlready();

  if (mProcessLinkHeaderEvent.get()) {
    mProcessLinkHeaderEvent.Revoke();

    DoProcessLinkHeader();
  }
}

/* static */
void nsContentSink::NotifyDocElementCreated(Document* aDoc) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  auto* win = nsGlobalWindowInner::Cast(aDoc->GetInnerWindow());
  bool fireInitialInsertion = !win || !win->DidFireDocElemInserted();
  if (win) {
    win->SetDidFireDocElemInserted();
  }
  if (fireInitialInsertion) {
    observerService->NotifyObservers(ToSupports(aDoc),
                                     "initial-document-element-inserted", u"");
  }
  observerService->NotifyObservers(ToSupports(aDoc),
                                   "document-element-inserted", u"");

  nsContentUtils::DispatchChromeEvent(aDoc, ToSupports(aDoc),
                                      u"DOMDocElementInserted"_ns,
                                      CanBubble::eYes, Cancelable::eNo);
}

NS_IMETHODIMP
nsContentSink::GetName(nsACString& aName) {
  aName.AssignLiteral("nsContentSink_timer");
  return NS_OK;
}
