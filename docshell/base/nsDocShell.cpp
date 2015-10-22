/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShell.h"

#include <algorithm>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Casting.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/Telemetry.h"
#include "mozilla/unused.h"
#include "URIUtils.h"

#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

#include "nsIDOMStorage.h"
#include "nsIContentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIMozBrowserFrame.h"
#include "nsCURILoader.h"
#include "nsDocShellCID.h"
#include "nsDOMCID.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "nsRect.h"
#include "prenv.h"
#include "nsIDOMWindow.h"
#include "nsIGlobalObject.h"
#include "nsIViewSourceChannel.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScrollableFrame.h"
#include "nsContentPolicyUtils.h" // NS_CheckContentLoadPolicy(...)
#include "nsISeekableStream.h"
#include "nsAutoPtr.h"
#include "nsQueryObject.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScriptChannel.h"
#include "nsITimedChannel.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsIDocShellTreeItem.h"
#include "nsIChannel.h"
#include "IHistory.h"
#include "nsViewSourceHandler.h"
#include "nsWhitespaceTokenizer.h"
#include "nsICookieService.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"
#include "nsPILoadGroupInternal.h"

// Local Includes
#include "nsDocShellLoadInfo.h"
#include "nsCDefaultURIFixup.h"
#include "nsDocShellEnumerator.h"
#include "nsSHistory.h"
#include "nsDocShellEditorData.h"
#include "GeckoProfiler.h"
#include "timeline/JavascriptTimelineMarker.h"

// Helper Classes
#include "nsError.h"
#include "nsEscape.h"

// Interfaces Needed
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsITimer.h"
#include "nsISHistoryInternal.h"
#include "nsIPrincipal.h"
#include "nsNullPrincipal.h"
#include "nsISHEntry.h"
#include "nsIWindowWatcher.h"
#include "nsIPromptFactory.h"
#include "nsITransportSecurityInfo.h"
#include "nsINode.h"
#include "nsINSSErrorsService.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheContainer.h"
#include "nsStreamUtils.h"
#include "nsIController.h"
#include "nsPICommandUpdater.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIWebBrowserChrome3.h"
#include "nsITabChild.h"
#include "nsISiteSecurityService.h"
#include "nsStructuredCloneContainer.h"
#include "nsIStructuredCloneContainer.h"
#ifdef MOZ_PLACES
#include "nsIFaviconService.h"
#include "mozIAsyncFavicons.h"
#endif
#include "nsINetworkPredictor.h"

// Editor-related
#include "nsIEditingSession.h"

#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsPIWindowRoot.h"
#include "nsICachingChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIWyciwygChannel.h"

// For reporting errors with the console service.
// These can go away if error reporting is propagated up past nsDocShell.
#include "nsIScriptError.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

#include "nsFocusManager.h"

#include "nsITextToSubURI.h"

#include "nsIJARChannel.h"

#include "mozilla/Logging.h"

#include "nsISelectionDisplay.h"

#include "nsIGlobalHistory2.h"

#include "nsIFrame.h"
#include "nsSubDocumentFrame.h"

// for embedding
#include "nsIWebBrowserChromeFocus.h"

#if NS_PRINT_PREVIEW
#include "nsIDocumentViewerPrint.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "nsILoadInfo.h"
#include "nsSandboxFlags.h"
#include "nsXULAppAPI.h"
#include "nsDOMNavigationTiming.h"
#include "nsISecurityUITelemetry.h"
#include "nsIAppsService.h"
#include "nsDSURIContentListener.h"
#include "nsDocShellLoadTypes.h"
#include "nsDocShellTransferableHooks.h"
#include "nsICommandManager.h"
#include "nsIDOMNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIHttpChannel.h"
#include "nsIIDNService.h"
#include "nsIInputStreamChannel.h"
#include "nsINestedURI.h"
#include "nsISHContainer.h"
#include "nsISHistory.h"
#include "nsISecureBrowserUI.h"
#include "nsISocketProvider.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsIURIFixup.h"
#include "nsIURILoader.h"
#include "nsIURL.h"
#include "nsIWebBrowserFind.h"
#include "nsIWidget.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsPerformance.h"

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

#include "mozIThirdPartyUtil.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
//#define DEBUG_DOCSHELL_FOCUS
#define DEBUG_PAGE_CACHE
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::workers::ServiceWorkerManager;

// True means sUseErrorPages has been added to
// preferences var cache.
static bool gAddedPreferencesVarCache = false;

bool nsDocShell::sUseErrorPages = false;

// Number of documents currently loading
static int32_t gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static int32_t gDocShellCount = 0;

// Global count of docshells with the private attribute set
static uint32_t gNumberOfPrivateDocShells = 0;

// Global reference to the URI fixup service.
nsIURIFixup* nsDocShell::sURIFixup = 0;

// True means we validate window targets to prevent frameset
// spoofing. Initialize this to a non-bolean value so we know to check
// the pref on the creation of the first docshell.
static uint32_t gValidateOrigin = 0xffffffff;

// Hint for native dispatch of events on how long to delay after
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
// Can be overridden with docshell.event_starvation_delay_hint pref.
#define NS_EVENT_STARVATION_DELAY_HINT 2000

#ifdef DEBUG
static PRLogModuleInfo* gDocShellLog;
#endif
static PRLogModuleInfo* gDocShellLeakLog;

const char kBrandBundleURL[]      = "chrome://branding/locale/brand.properties";
const char kAppstringsBundleURL[] = "chrome://global/locale/appstrings.properties";

static void
FavorPerformanceHint(bool aPerfOverStarvation)
{
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->FavorPerformanceHint(
      aPerfOverStarvation,
      Preferences::GetUint("docshell.event_starvation_delay_hint",
                           NS_EVENT_STARVATION_DELAY_HINT));
  }
}

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

typedef void (*ForEachPingCallback)(void* closure, nsIContent* content,
                                    nsIURI* uri, nsIIOService* ios);

static bool
IsElementAnchor(nsIContent* aContent)
{
  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area);
}

static void
ForEachPing(nsIContent* aContent, ForEachPingCallback aCallback, void* aClosure)
{
  // NOTE: Using nsIDOMHTMLAnchorElement::GetPing isn't really worth it here
  //       since we'd still need to parse the resulting string.  Instead, we
  //       just parse the raw attribute.  It might be nice if the content node
  //       implemented an interface that exposed an enumeration of nsIURIs.

  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  if (!IsElementAnchor(aContent)) {
    return;
  }

  nsCOMPtr<nsIAtom> pingAtom = do_GetAtom("ping");
  if (!pingAtom) {
    return;
  }

  nsAutoString value;
  aContent->GetAttr(kNameSpaceID_None, pingAtom, value);
  if (value.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (!ios) {
    return;
  }

  nsIDocument* doc = aContent->OwnerDoc();

  nsWhitespaceTokenizer tokenizer(value);

  while (tokenizer.hasMoreTokens()) {
    nsCOMPtr<nsIURI> uri, baseURI = aContent->GetBaseURI();
    ios->NewURI(NS_ConvertUTF16toUTF8(tokenizer.nextToken()),
                doc->GetDocumentCharacterSet().get(),
                baseURI, getter_AddRefs(uri));

    // Explicitly not allow loading data: URIs
    bool isDataScheme =
      (NS_SUCCEEDED(uri->SchemeIs("data", &isDataScheme)) && isDataScheme);

    if (!isDataScheme) {
      aCallback(aClosure, aContent, uri, ios);
    }
  }
}

//----------------------------------------------------------------------

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

class nsPingListener final
  : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsPingListener()
  {
  }

  void SetLoadGroup(nsILoadGroup* aLoadGroup) {
    mLoadGroup = aLoadGroup;
  }

  nsresult StartTimeout();

private:
  ~nsPingListener();

  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(nsPingListener, nsIStreamListener, nsIRequestObserver)

nsPingListener::~nsPingListener()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

nsresult
nsPingListener::StartTimeout()
{
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);

  if (timer) {
    nsresult rv = timer->InitWithFuncCallback(OnPingTimeout, mLoadGroup,
                                              PING_TIMEOUT,
                                              nsITimer::TYPE_ONE_SHOT);
    if (NS_SUCCEEDED(rv)) {
      mTimer = timer;
      return NS_OK;
    }
  }

  return NS_ERROR_OUT_OF_MEMORY;
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
                nullptr, // aLoadGroup
                nullptr, // aCallbacks
                nsIRequest::LOAD_NORMAL, // aLoadFlags,
                aIOService);

  if (!chan) {
    return;
  }

  // Don't bother caching the result of this URI load.
  chan->SetLoadFlags(nsIRequest::INHIBIT_CACHING);

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (!httpChan) {
    return;
  }

  // This is needed in order for 3rd-party cookie blocking to work.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(httpChan);
  if (httpInternal) {
    httpInternal->SetDocumentURI(doc->GetDocumentURI());
  }

  httpChan->SetRequestMethod(NS_LITERAL_CSTRING("POST"));

  // Remove extraneous request headers (to reduce request size)
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept"),
                             EmptyCString(), false);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-language"),
                             EmptyCString(), false);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-encoding"),
                             EmptyCString(), false);

  // Always send a Ping-To header.
  nsAutoCString pingTo;
  if (NS_SUCCEEDED(info->target->GetSpec(pingTo))) {
    httpChan->SetRequestHeader(NS_LITERAL_CSTRING("Ping-To"), pingTo, false);
  }

  nsCOMPtr<nsIScriptSecurityManager> sm =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

  if (sm && info->referrer) {
    bool referrerIsSecure;
    uint32_t flags = nsIProtocolHandler::URI_SAFE_TO_LOAD_IN_SECURE_CONTEXT;
    nsresult rv = NS_URIChainHasFlags(info->referrer, flags, &referrerIsSecure);

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
        httpChan->SetRequestHeader(NS_LITERAL_CSTRING("Ping-From"), pingFrom,
                                   false);
      }
    }

    // If the document containing the hyperlink being audited was not retrieved
    // over an encrypted connection and its address does not have the same
    // origin as "ping URL", send a referrer.
    if (!sameOrigin && !referrerIsSecure) {
      httpChan->SetReferrerWithPolicy(info->referrer, info->referrerPolicy);
    }
  }

  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(httpChan);
  if (!uploadChan) {
    return;
  }

  NS_NAMED_LITERAL_CSTRING(uploadData, "PING");

  nsCOMPtr<nsIInputStream> uploadStream;
  NS_NewPostDataStream(getter_AddRefs(uploadStream), false, uploadData);
  if (!uploadStream) {
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
  if (NS_FAILED(pingListener->StartTimeout())) {
    // If we failed to setup the timer, then we should just cancel the channel
    // because we won't be able to ensure that it goes away in a timely manner.
    chan->Cancel(NS_ERROR_ABORT);
    return;
  }
  // if the channel openend successfully, then make the pingListener hold
  // a strong reference to the loadgroup which is released in ::OnStopRequest
  pingListener->SetLoadGroup(loadGroup);
}

// Spec: http://whatwg.org/specs/web-apps/current-work/#ping
static void
DispatchPings(nsIDocShell* aDocShell,
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

static nsDOMPerformanceNavigationType
ConvertLoadTypeToNavigationType(uint32_t aLoadType)
{
  // Not initialized, assume it's normal load.
  if (aLoadType == 0) {
    aLoadType = LOAD_NORMAL;
  }

  auto result = dom::PerformanceNavigation::TYPE_RESERVED;
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_EXTERNAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_REPLACE:
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
    case LOAD_LINK:
    case LOAD_STOP_CONTENT:
    case LOAD_REPLACE_BYPASS_CACHE:
      result = dom::PerformanceNavigation::TYPE_NAVIGATE;
      break;
    case LOAD_HISTORY:
      result = dom::PerformanceNavigation::TYPE_BACK_FORWARD;
      break;
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_CHARSET_CHANGE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      result = dom::PerformanceNavigation::TYPE_RELOAD;
      break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_REFRESH:
    case LOAD_BYPASS_HISTORY:
    case LOAD_ERROR_PAGE:
    case LOAD_PUSHSTATE:
      result = dom::PerformanceNavigation::TYPE_RESERVED;
      break;
    default:
      // NS_NOTREACHED("Unexpected load type value");
      result = dom::PerformanceNavigation::TYPE_RESERVED;
      break;
  }

  return result;
}

static nsISHEntry* GetRootSHEntry(nsISHEntry* aEntry);

static void
IncreasePrivateDocShellCount()
{
  gNumberOfPrivateDocShells++;
  if (gNumberOfPrivateDocShells > 1 ||
      !XRE_IsContentProcess()) {
    return;
  }

  mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
  cc->SendPrivateDocShellsExist(true);
}

static void
DecreasePrivateDocShellCount()
{
  MOZ_ASSERT(gNumberOfPrivateDocShells > 0);
  gNumberOfPrivateDocShells--;
  if (!gNumberOfPrivateDocShells) {
    if (XRE_IsContentProcess()) {
      dom::ContentChild* cc = dom::ContentChild::GetSingleton();
      cc->SendPrivateDocShellsExist(false);
      return;
    }

    nsCOMPtr<nsIObserverService> obsvc = services::GetObserverService();
    if (obsvc) {
      obsvc->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
    }
  }
}

static uint64_t gDocshellIDCounter = 0;

nsDocShell::nsDocShell()
  : nsDocLoader()
  , mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto)
  , mTreeOwner(nullptr)
  , mChromeEventHandler(nullptr)
  , mCharsetReloadState(eCharsetReloadInit)
  , mChildOffset(0)
  , mBusyFlags(BUSY_FLAGS_NONE)
  , mAppType(nsIDocShell::APP_TYPE_UNKNOWN)
  , mLoadType(0)
  , mMarginWidth(-1)
  , mMarginHeight(-1)
  , mItemType(typeContent)
  , mPreviousTransIndex(-1)
  , mLoadedTransIndex(-1)
  , mSandboxFlags(0)
  , mFullscreenAllowed(CHECK_ATTRIBUTES)
  , mOrientationLock(eScreenOrientation_None)
  , mCreated(false)
  , mAllowSubframes(true)
  , mAllowPlugins(true)
  , mAllowJavascript(true)
  , mAllowMetaRedirects(true)
  , mAllowImages(true)
  , mAllowMedia(true)
  , mAllowDNSPrefetch(true)
  , mAllowWindowControl(true)
  , mAllowContentRetargeting(true)
  , mAllowContentRetargetingOnChildren(true)
  , mCreatingDocument(false)
  , mUseErrorPages(false)
  , mObserveErrorPages(true)
  , mAllowAuth(true)
  , mAllowKeywordFixup(false)
  , mIsOffScreenBrowser(false)
  , mIsActive(true)
  , mIsPrerendered(false)
  , mIsAppTab(false)
  , mUseGlobalHistory(false)
  , mInPrivateBrowsing(false)
  , mUseRemoteTabs(false)
  , mDeviceSizeIsPageSize(false)
  , mWindowDraggingAllowed(false)
  , mInFrameSwap(false)
  , mCanExecuteScripts(false)
  , mFiredUnloadEvent(false)
  , mEODForCurrentDocument(false)
  , mURIResultedInDocument(false)
  , mIsBeingDestroyed(false)
  , mIsExecutingOnLoadHandler(false)
  , mIsPrintingOrPP(false)
  , mSavingOldViewer(false)
#ifdef DEBUG
  , mInEnsureScriptEnv(false)
#endif
  , mAffectPrivateSessionLifetime(true)
  , mInvisible(false)
  , mHasLoadedNonBlankURI(false)
  , mDefaultLoadFlags(nsIRequest::LOAD_NORMAL)
  , mBlankTiming(false)
  , mFrameType(eFrameTypeRegular)
  , mOwnOrContainingAppId(nsIScriptSecurityManager::UNKNOWN_APP_ID)
  , mParentCharsetSource(0)
  , mJSRunToCompletionDepth(0)
{
  mHistoryID = ++gDocshellIDCounter;
  if (gDocShellCount++ == 0) {
    NS_ASSERTION(sURIFixup == nullptr,
                 "Huh, sURIFixup not null in first nsDocShell ctor!");

    CallGetService(NS_URIFIXUP_CONTRACTID, &sURIFixup);
  }

#ifdef DEBUG
  if (!gDocShellLog) {
    gDocShellLog = PR_NewLogModule("nsDocShell");
  }
#endif
  if (!gDocShellLeakLog) {
    gDocShellLeakLog = PR_NewLogModule("nsDocShellLeak");
  }
  if (gDocShellLeakLog) {
    MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p created\n", this));
  }

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  ++gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("++DOCSHELL %p == %ld [pid = %d] [id = %llu]\n",
                  (void*)this,
                  gNumberOfDocShells,
                  getpid(),
                  AssertedCast<unsigned long long>(mHistoryID));
  }
#endif
}

nsDocShell::~nsDocShell()
{
  MOZ_ASSERT(!IsObserved());

  Destroy();

  nsCOMPtr<nsISHistoryInternal> shPrivate(do_QueryInterface(mSessionHistory));
  if (shPrivate) {
    shPrivate->SetRootDocShell(nullptr);
  }

  if (--gDocShellCount == 0) {
    NS_IF_RELEASE(sURIFixup);
  }

  if (gDocShellLeakLog) {
    MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p destroyed\n", this));
  }

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  --gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("--DOCSHELL %p == %ld [pid = %d] [id = %llu]\n",
                  (void*)this,
                  gNumberOfDocShells,
                  getpid(),
                  AssertedCast<unsigned long long>(mHistoryID));
  }
#endif
}

nsresult
nsDocShell::Init()
{
  nsresult rv = nsDocLoader::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(mLoadGroup, "Something went wrong!");

  mContentListener = new nsDSURIContentListener(this);
  NS_ENSURE_TRUE(mContentListener, NS_ERROR_OUT_OF_MEMORY);

  rv = mContentListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // We want to hold a strong ref to the loadgroup, so it better hold a weak
  // ref to us...  use an InterfaceRequestorProxy to do this.
  nsCOMPtr<nsIInterfaceRequestor> proxy =
    new InterfaceRequestorProxy(static_cast<nsIInterfaceRequestor*>(this));
  NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);
  mLoadGroup->SetNotificationCallbacks(proxy);

  rv = nsDocLoader::AddDocLoaderAsChildOfRoot(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add as |this| a progress listener to itself.  A little weird, but
  // simpler than reproducing all the listener-notification logic in
  // overrides of the various methods via which nsDocLoader can be
  // notified.   Note that this holds an nsWeakPtr to ourselves, so it's ok.
  return AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                     nsIWebProgress::NOTIFY_STATE_NETWORK);
}

void
nsDocShell::DestroyChildren()
{
  nsCOMPtr<nsIDocShellTreeItem> shell;
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    shell = do_QueryObject(iter.GetNext());
    NS_ASSERTION(shell, "docshell has null child");

    if (shell) {
      shell->SetTreeOwner(nullptr);
    }
  }

  nsDocLoader::DestroyChildren();
}

NS_IMPL_ADDREF_INHERITED(nsDocShell, nsDocLoader)
NS_IMPL_RELEASE_INHERITED(nsDocShell, nsDocLoader)

NS_INTERFACE_MAP_BEGIN(nsDocShell)
  NS_INTERFACE_MAP_ENTRY(nsIDocShell)
  NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
  NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIScrollable)
  NS_INTERFACE_MAP_ENTRY(nsITextScroll)
  NS_INTERFACE_MAP_ENTRY(nsIDocCharset)
  NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIContentViewerContainer)
  NS_INTERFACE_MAP_ENTRY(nsIWebPageDescriptor)
  NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
  NS_INTERFACE_MAP_ENTRY(nsILoadContext)
  NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
  NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
  NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageManager)
  NS_INTERFACE_MAP_ENTRY(nsINetworkInterceptController)
  NS_INTERFACE_MAP_ENTRY(nsIDeprecationWarner)
NS_INTERFACE_MAP_END_INHERITING(nsDocLoader)

NS_IMETHODIMP
nsDocShell::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_PRECONDITION(aSink, "null out param");

  *aSink = nullptr;

  if (aIID.Equals(NS_GET_IID(nsICommandManager))) {
    NS_ENSURE_SUCCESS(EnsureCommandHandler(), NS_ERROR_FAILURE);
    *aSink = mCommandManager;
  } else if (aIID.Equals(NS_GET_IID(nsIURIContentListener))) {
    *aSink = mContentListener;
  } else if ((aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) ||
              aIID.Equals(NS_GET_IID(nsIGlobalObject)) ||
              aIID.Equals(NS_GET_IID(nsPIDOMWindow)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindow)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindowInternal))) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
    return mScriptGlobal->QueryInterface(aIID, aSink);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
    mContentViewer->GetDOMDocument((nsIDOMDocument**)aSink);
    return *aSink ? NS_OK : NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsIDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
    nsCOMPtr<nsIDocument> doc = mContentViewer->GetDocument();
    doc.forget(aSink);
    return *aSink ? NS_OK : NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsIApplicationCacheContainer))) {
    *aSink = nullptr;

    // Return application cache associated with this docshell, if any

    nsCOMPtr<nsIContentViewer> contentViewer;
    GetContentViewer(getter_AddRefs(contentViewer));
    if (!contentViewer) {
      return NS_ERROR_NO_INTERFACE;
    }

    nsCOMPtr<nsIDOMDocument> domDoc;
    contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
    NS_ASSERTION(domDoc, "Should have a document.");
    if (!domDoc) {
      return NS_ERROR_NO_INTERFACE;
    }

#if defined(DEBUG)
    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]: returning app cache container %p",
            this, domDoc.get()));
#endif
    return domDoc->QueryInterface(aIID, aSink);
  } else if (aIID.Equals(NS_GET_IID(nsIPrompt)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the an auth prompter for our window so that the parenting
    // of the dialogs works as it should when using tabs.
    nsIPrompt* prompt;
    rv = wwatch->GetNewPrompter(mScriptGlobal, &prompt);
    NS_ENSURE_SUCCESS(rv, rv);

    *aSink = prompt;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
             aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    return NS_SUCCEEDED(GetAuthPrompt(PROMPT_NORMAL, aIID, aSink)) ?
      NS_OK : NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsISHistory))) {
    nsCOMPtr<nsISHistory> shistory;
    nsresult rv = GetSessionHistory(getter_AddRefs(shistory));
    if (NS_SUCCEEDED(rv) && shistory) {
      shistory.forget(aSink);
      return NS_OK;
    }
    return NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsIWebBrowserFind))) {
    nsresult rv = EnsureFind();
    if (NS_FAILED(rv)) {
      return rv;
    }

    *aSink = mFind;
    NS_ADDREF((nsISupports*)*aSink);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIEditingSession)) &&
             NS_SUCCEEDED(EnsureEditorData())) {
    nsCOMPtr<nsIEditingSession> editingSession;
    mEditorData->GetEditingSession(getter_AddRefs(editingSession));
    if (editingSession) {
      editingSession.forget(aSink);
      return NS_OK;
    }

    return NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsIClipboardDragDropHookList)) &&
             NS_SUCCEEDED(EnsureTransferableHookData())) {
    *aSink = mTransferableHookData;
    NS_ADDREF((nsISupports*)*aSink);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsISelectionDisplay))) {
    nsIPresShell* shell = GetPresShell();
    if (shell) {
      return shell->QueryInterface(aIID, aSink);
    }
  } else if (aIID.Equals(NS_GET_IID(nsIDocShellTreeOwner))) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
    if (NS_SUCCEEDED(rv) && treeOwner) {
      return treeOwner->QueryInterface(aIID, aSink);
    }
  } else if (aIID.Equals(NS_GET_IID(nsITabChild))) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
    if (NS_SUCCEEDED(rv) && treeOwner) {
      nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(treeOwner);
      if (ir) {
        return ir->GetInterface(aIID, aSink);
      }
    }
  } else if (aIID.Equals(NS_GET_IID(nsIContentFrameMessageManager))) {
    nsCOMPtr<nsITabChild> tabChild =
      do_GetInterface(static_cast<nsIDocShell*>(this));
    nsCOMPtr<nsIContentFrameMessageManager> mm;
    if (tabChild) {
      tabChild->GetMessageManager(getter_AddRefs(mm));
    } else {
      nsCOMPtr<nsPIDOMWindow> win = GetWindow();
      if (win) {
        mm = do_QueryInterface(win->GetParentTarget());
      }
    }
    *aSink = mm.get();
  } else {
    return nsDocLoader::GetInterface(aIID, aSink);
  }

  NS_IF_ADDREF(((nsISupports*)*aSink));
  return *aSink ? NS_OK : NS_NOINTERFACE;
}

uint32_t
nsDocShell::ConvertDocShellLoadInfoToLoadType(
    nsDocShellInfoLoadType aDocShellLoadType)
{
  uint32_t loadType = LOAD_NORMAL;

  switch (aDocShellLoadType) {
    case nsIDocShellLoadInfo::loadNormal:
      loadType = LOAD_NORMAL;
      break;
    case nsIDocShellLoadInfo::loadNormalReplace:
      loadType = LOAD_NORMAL_REPLACE;
      break;
    case nsIDocShellLoadInfo::loadNormalExternal:
      loadType = LOAD_NORMAL_EXTERNAL;
      break;
    case nsIDocShellLoadInfo::loadHistory:
      loadType = LOAD_HISTORY;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassCache:
      loadType = LOAD_NORMAL_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassProxy:
      loadType = LOAD_NORMAL_BYPASS_PROXY;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassProxyAndCache:
      loadType = LOAD_NORMAL_BYPASS_PROXY_AND_CACHE;
      break;
    case nsIDocShellLoadInfo::loadNormalAllowMixedContent:
      loadType = LOAD_NORMAL_ALLOW_MIXED_CONTENT;
      break;
    case nsIDocShellLoadInfo::loadReloadNormal:
      loadType = LOAD_RELOAD_NORMAL;
      break;
    case nsIDocShellLoadInfo::loadReloadCharsetChange:
      loadType = LOAD_RELOAD_CHARSET_CHANGE;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassCache:
      loadType = LOAD_RELOAD_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
      loadType = LOAD_RELOAD_BYPASS_PROXY;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
      loadType = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
      break;
    case nsIDocShellLoadInfo::loadLink:
      loadType = LOAD_LINK;
      break;
    case nsIDocShellLoadInfo::loadRefresh:
      loadType = LOAD_REFRESH;
      break;
    case nsIDocShellLoadInfo::loadBypassHistory:
      loadType = LOAD_BYPASS_HISTORY;
      break;
    case nsIDocShellLoadInfo::loadStopContent:
      loadType = LOAD_STOP_CONTENT;
      break;
    case nsIDocShellLoadInfo::loadStopContentAndReplace:
      loadType = LOAD_STOP_CONTENT_AND_REPLACE;
      break;
    case nsIDocShellLoadInfo::loadPushState:
      loadType = LOAD_PUSHSTATE;
      break;
    case nsIDocShellLoadInfo::loadReplaceBypassCache:
      loadType = LOAD_REPLACE_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadMixedContent:
      loadType = LOAD_RELOAD_ALLOW_MIXED_CONTENT;
      break;
    default:
      NS_NOTREACHED("Unexpected nsDocShellInfoLoadType value");
  }

  return loadType;
}

nsDocShellInfoLoadType
nsDocShell::ConvertLoadTypeToDocShellLoadInfo(uint32_t aLoadType)
{
  nsDocShellInfoLoadType docShellLoadType = nsIDocShellLoadInfo::loadNormal;
  switch (aLoadType) {
    case LOAD_NORMAL:
      docShellLoadType = nsIDocShellLoadInfo::loadNormal;
      break;
    case LOAD_NORMAL_REPLACE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalReplace;
      break;
    case LOAD_NORMAL_EXTERNAL:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalExternal;
      break;
    case LOAD_NORMAL_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassCache;
      break;
    case LOAD_NORMAL_BYPASS_PROXY:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxy;
      break;
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxyAndCache;
      break;
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalAllowMixedContent;
      break;
    case LOAD_HISTORY:
      docShellLoadType = nsIDocShellLoadInfo::loadHistory;
      break;
    case LOAD_RELOAD_NORMAL:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadNormal;
      break;
    case LOAD_RELOAD_CHARSET_CHANGE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadCharsetChange;
      break;
    case LOAD_RELOAD_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassCache;
      break;
    case LOAD_RELOAD_BYPASS_PROXY:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxy;
      break;
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;
      break;
    case LOAD_LINK:
      docShellLoadType = nsIDocShellLoadInfo::loadLink;
      break;
    case LOAD_REFRESH:
      docShellLoadType = nsIDocShellLoadInfo::loadRefresh;
      break;
    case LOAD_BYPASS_HISTORY:
    case LOAD_ERROR_PAGE:
      docShellLoadType = nsIDocShellLoadInfo::loadBypassHistory;
      break;
    case LOAD_STOP_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadStopContent;
      break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
      docShellLoadType = nsIDocShellLoadInfo::loadStopContentAndReplace;
      break;
    case LOAD_PUSHSTATE:
      docShellLoadType = nsIDocShellLoadInfo::loadPushState;
      break;
    case LOAD_REPLACE_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReplaceBypassCache;
      break;
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadMixedContent;
      break;
    default:
      NS_NOTREACHED("Unexpected load type value");
  }

  return docShellLoadType;
}

NS_IMETHODIMP
nsDocShell::LoadURI(nsIURI* aURI,
                    nsIDocShellLoadInfo* aLoadInfo,
                    uint32_t aLoadFlags,
                    bool aFirstParty)
{
  NS_PRECONDITION(aLoadInfo || (aLoadFlags & EXTRA_LOAD_FLAGS) == 0,
                  "Unexpected flags");
  NS_PRECONDITION((aLoadFlags & 0xf) == 0, "Should not have these flags set");

  // Note: we allow loads to get through here even if mFiredUnloadEvent is
  // true; that case will get handled in LoadInternal or LoadHistoryEntry,
  // so we pass false as the second parameter to IsNavigationAllowed.
  // However, we don't allow the page to change location *in the middle of*
  // firing beforeunload, so we do need to check if *beforeunload* is currently
  // firing, so we call IsNavigationAllowed rather than just IsPrintingOrPP.
  if (!IsNavigationAllowed(true, false)) {
    return NS_OK; // JS may not handle returning of an error code
  }

  if (DoAppRedirectIfNeeded(aURI, aLoadInfo, aFirstParty)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> referrer;
  nsCOMPtr<nsIURI> originalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIInputStream> postStream;
  nsCOMPtr<nsIInputStream> headersStream;
  nsCOMPtr<nsISupports> owner;
  bool inheritOwner = false;
  bool ownerIsExplicit = false;
  bool sendReferrer = true;
  uint32_t referrerPolicy = mozilla::net::RP_Default;
  bool isSrcdoc = false;
  nsCOMPtr<nsISHEntry> shEntry;
  nsXPIDLString target;
  nsAutoString srcdoc;
  nsCOMPtr<nsIDocShell> sourceDocShell;
  nsCOMPtr<nsIURI> baseURI;

  uint32_t loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);

  NS_ENSURE_ARG(aURI);

  if (!StartupTimeline::HasRecord(StartupTimeline::FIRST_LOAD_URI) &&
      mItemType == typeContent && !NS_IsAboutBlank(aURI)) {
    StartupTimeline::RecordOnce(StartupTimeline::FIRST_LOAD_URI);
  }

  // Extract the info from the DocShellLoadInfo struct...
  if (aLoadInfo) {
    aLoadInfo->GetReferrer(getter_AddRefs(referrer));
    aLoadInfo->GetOriginalURI(getter_AddRefs(originalURI));
    aLoadInfo->GetLoadReplace(&loadReplace);
    nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
    aLoadInfo->GetLoadType(&lt);
    // Get the appropriate loadType from nsIDocShellLoadInfo type
    loadType = ConvertDocShellLoadInfoToLoadType(lt);

    aLoadInfo->GetOwner(getter_AddRefs(owner));
    aLoadInfo->GetInheritOwner(&inheritOwner);
    aLoadInfo->GetOwnerIsExplicit(&ownerIsExplicit);
    aLoadInfo->GetSHEntry(getter_AddRefs(shEntry));
    aLoadInfo->GetTarget(getter_Copies(target));
    aLoadInfo->GetPostDataStream(getter_AddRefs(postStream));
    aLoadInfo->GetHeadersStream(getter_AddRefs(headersStream));
    aLoadInfo->GetSendReferrer(&sendReferrer);
    aLoadInfo->GetReferrerPolicy(&referrerPolicy);
    aLoadInfo->GetIsSrcdocLoad(&isSrcdoc);
    aLoadInfo->GetSrcdocData(srcdoc);
    aLoadInfo->GetSourceDocShell(getter_AddRefs(sourceDocShell));
    aLoadInfo->GetBaseURI(getter_AddRefs(baseURI));
  }

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString uristr;
    aURI->GetAsciiSpec(uristr);
    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]: loading %s with flags 0x%08x",
            this, uristr.get(), aLoadFlags));
  }
#endif

  if (!shEntry &&
      !LOAD_TYPE_HAS_FLAGS(loadType, LOAD_FLAGS_REPLACE_HISTORY)) {
    // First verify if this is a subframe.
    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    GetSameTypeParent(getter_AddRefs(parentAsItem));
    nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(parentAsItem));
    uint32_t parentLoadType;

    if (parentDS && parentDS != static_cast<nsIDocShell*>(this)) {
      /* OK. It is a subframe. Checkout the
       * parent's loadtype. If the parent was loaded thro' a history
       * mechanism, then get the SH entry for the child from the parent.
       * This is done to restore frameset navigation while going back/forward.
       * If the parent was loaded through any other loadType, set the
       * child's loadType too accordingly, so that session history does not
       * get confused.
       */

      // Get the parent's load type
      parentDS->GetLoadType(&parentLoadType);

      // Get the ShEntry for the child from the parent
      nsCOMPtr<nsISHEntry> currentSH;
      bool oshe = false;
      parentDS->GetCurrentSHEntry(getter_AddRefs(currentSH), &oshe);
      bool dynamicallyAddedChild = mDynamicallyCreated;
      if (!dynamicallyAddedChild && !oshe && currentSH) {
        currentSH->HasDynamicallyAddedChild(&dynamicallyAddedChild);
      }
      if (!dynamicallyAddedChild) {
        // Only use the old SHEntry, if we're sure enough that
        // it wasn't originally for some other frame.
        parentDS->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
      }

      // Make some decisions on the child frame's loadType based on the
      // parent's loadType.
      if (!mCurrentURI) {
        // This is a newly created frame. Check for exception cases first.
        // By default the subframe will inherit the parent's loadType.
        if (shEntry && (parentLoadType == LOAD_NORMAL ||
                        parentLoadType == LOAD_LINK   ||
                        parentLoadType == LOAD_NORMAL_EXTERNAL)) {
          // The parent was loaded normally. In this case, this *brand new*
          // child really shouldn't have a SHEntry. If it does, it could be
          // because the parent is replacing an existing frame with a new frame,
          // in the onLoadHandler. We don't want this url to get into session
          // history. Clear off shEntry, and set load type to
          // LOAD_BYPASS_HISTORY.
          bool inOnLoadHandler = false;
          parentDS->GetIsExecutingOnLoadHandler(&inOnLoadHandler);
          if (inOnLoadHandler) {
            loadType = LOAD_NORMAL_REPLACE;
            shEntry = nullptr;
          }
        } else if (parentLoadType == LOAD_REFRESH) {
          // Clear shEntry. For refresh loads, we have to load
          // what comes thro' the pipe, not what's in history.
          shEntry = nullptr;
        } else if ((parentLoadType == LOAD_BYPASS_HISTORY) ||
                   (shEntry &&
                    ((parentLoadType & LOAD_CMD_HISTORY) ||
                     (parentLoadType == LOAD_RELOAD_NORMAL) ||
                     (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE)))) {
          // If the parent url, bypassed history or was loaded from
          // history, pass on the parent's loadType to the new child
          // frame too, so that the child frame will also
          // avoid getting into history.
          loadType = parentLoadType;
        } else if (parentLoadType == LOAD_ERROR_PAGE) {
          // If the parent document is an error page, we don't
          // want to update global/session history. However,
          // this child frame is not an error page.
          loadType = LOAD_BYPASS_HISTORY;
        } else if ((parentLoadType == LOAD_RELOAD_BYPASS_CACHE) ||
                   (parentLoadType == LOAD_RELOAD_BYPASS_PROXY) ||
                   (parentLoadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE)) {
          // the new frame should inherit the parent's load type so that it also
          // bypasses the cache and/or proxy
          loadType = parentLoadType;
        }
      } else {
        // This is a pre-existing subframe. If the load was not originally
        // initiated by session history, (if (!shEntry) condition succeeded) and
        // mCurrentURI is not null, it is possible that a parent's onLoadHandler
        // or even self's onLoadHandler is loading a new page in this child.
        // Check parent's and self's busy flag  and if it is set, we don't want
        // this onLoadHandler load to get in to session history.
        uint32_t parentBusy = BUSY_FLAGS_NONE;
        uint32_t selfBusy = BUSY_FLAGS_NONE;
        parentDS->GetBusyFlags(&parentBusy);
        GetBusyFlags(&selfBusy);
        if (parentBusy & BUSY_FLAGS_BUSY ||
            selfBusy & BUSY_FLAGS_BUSY) {
          loadType = LOAD_NORMAL_REPLACE;
          shEntry = nullptr;
        }
      }
    } // parentDS
    else {
      // This is the root docshell. If we got here while
      // executing an onLoad Handler,this load will not go
      // into session history.
      bool inOnLoadHandler = false;
      GetIsExecutingOnLoadHandler(&inOnLoadHandler);
      if (inOnLoadHandler) {
        loadType = LOAD_NORMAL_REPLACE;
      }
    }
  } // !shEntry

  if (shEntry) {
#ifdef DEBUG
    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]: loading from session history", this));
#endif

    return LoadHistoryEntry(shEntry, loadType);
  }

  // On history navigation via Back/Forward buttons, don't execute
  // automatic JavaScript redirection such as |location.href = ...| or
  // |window.open()|
  //
  // LOAD_NORMAL:        window.open(...) etc.
  // LOAD_STOP_CONTENT:  location.href = ..., location.assign(...)
  if ((loadType == LOAD_NORMAL || loadType == LOAD_STOP_CONTENT) &&
      ShouldBlockLoadingForBackButton()) {
    return NS_OK;
  }

  // Perform the load...

  // We need an owner (a referring principal).
  //
  // If ownerIsExplicit is not set there are 4 possibilities:
  // (1) If the system principal or an expanded principal was passed
  //     in and we're a typeContent docshell, inherit the principal
  //     from the current document instead.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     or if we're being called from system code (eg chrome JS or pure
  //     C++) then inheritOwner should be true and InternalLoad will get
  //     an owner from the current document. If none of these things are
  //     true, then
  // (4) we pass a null owner into the channel, and an owner will be
  //     created later from the channel's internal data.
  //
  // If ownerIsExplicit *is* set, there are 4 possibilities
  // (1) If the system principal or an expanded principal was passed in
  //     and we're a typeContent docshell, return an error.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     then inheritOwner should be true and InternalLoad will get an owner
  //     from the current document. If none of these things are true, then
  // (4) we pass a null owner into the channel, and an owner will be
  //     created later from the channel's internal data.
  //
  // NOTE: This all only works because the only thing the owner is used
  //       for in InternalLoad is data:, javascript:, and about:blank
  //       URIs.  For other URIs this would all be dead wrong!

  if (owner && mItemType != typeChrome) {
    nsCOMPtr<nsIPrincipal> ownerPrincipal = do_QueryInterface(owner);
    if (nsContentUtils::IsSystemPrincipal(ownerPrincipal)) {
      if (ownerIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      owner = nullptr;
      inheritOwner = true;
    } else if (nsContentUtils::IsExpandedPrincipal(ownerPrincipal)) {
      if (ownerIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      // Don't inherit from the current page.  Just do the safe thing
      // and pretend that we were loaded by a nullprincipal.
      owner = nsNullPrincipal::Create();
      inheritOwner = false;
    }
  }
  if (!owner && !inheritOwner && !ownerIsExplicit) {
    // See if there's system or chrome JS code running
    inheritOwner = nsContentUtils::LegacyIsCallerChromeOrNativeCode();
  }

  if (aLoadFlags & LOAD_FLAGS_DISALLOW_INHERIT_OWNER) {
    inheritOwner = false;
    owner = nsNullPrincipal::Create();
  }

  uint32_t flags = 0;

  if (inheritOwner) {
    flags |= INTERNAL_LOAD_FLAGS_INHERIT_OWNER;
  }

  if (!sendReferrer) {
    flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER;
  }

  if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
    flags |= INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  if (aLoadFlags & LOAD_FLAGS_FIRST_LOAD) {
    flags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;
  }

  if (aLoadFlags & LOAD_FLAGS_BYPASS_CLASSIFIER) {
    flags |= INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER;
  }

  if (aLoadFlags & LOAD_FLAGS_FORCE_ALLOW_COOKIES) {
    flags |= INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES;
  }

  if (isSrcdoc) {
    flags |= INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  }

  return InternalLoad(aURI,
                      originalURI,
                      loadReplace,
                      referrer,
                      referrerPolicy,
                      owner,
                      flags,
                      target.get(),
                      nullptr,      // No type hint
                      NullString(), // No forced download
                      postStream,
                      headersStream,
                      loadType,
                      nullptr, // No SHEntry
                      aFirstParty,
                      srcdoc,
                      sourceDocShell,
                      baseURI,
                      nullptr,  // No nsIDocShell
                      nullptr); // No nsIRequest
}

NS_IMETHODIMP
nsDocShell::LoadStream(nsIInputStream* aStream, nsIURI* aURI,
                       const nsACString& aContentType,
                       const nsACString& aContentCharset,
                       nsIDocShellLoadInfo* aLoadInfo)
{
  NS_ENSURE_ARG(aStream);

  mAllowKeywordFixup = false;

  // if the caller doesn't pass in a URI we need to create a dummy URI. necko
  // currently requires a URI in various places during the load. Some consumers
  // do as well.
  nsCOMPtr<nsIURI> uri = aURI;
  if (!uri) {
    // HACK ALERT
    nsresult rv = NS_OK;
    uri = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // Make sure that the URI spec "looks" like a protocol and path...
    // For now, just use a bogus protocol called "internal"
    rv = uri->SetSpec(NS_LITERAL_CSTRING("internal:load-stream"));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  uint32_t loadType = LOAD_NORMAL;
  nsCOMPtr<nsIPrincipal> requestingPrincipal;
  if (aLoadInfo) {
    nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
    (void)aLoadInfo->GetLoadType(&lt);
    // Get the appropriate LoadType from nsIDocShellLoadInfo type
    loadType = ConvertDocShellLoadInfoToLoadType(lt);

    nsCOMPtr<nsISupports> owner;
    aLoadInfo->GetOwner(getter_AddRefs(owner));
    requestingPrincipal = do_QueryInterface(owner);
  }

  NS_ENSURE_SUCCESS(Stop(nsIWebNavigation::STOP_NETWORK), NS_ERROR_FAILURE);

  mLoadType = loadType;

  if (!requestingPrincipal) {
    requestingPrincipal = nsContentUtils::GetSystemPrincipal();
  }

  // build up a channel for this stream.
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                         uri,
                                         aStream,
                                         requestingPrincipal,
                                         nsILoadInfo::SEC_NORMAL,
                                         nsIContentPolicy::TYPE_OTHER,
                                         aContentType,
                                         aContentCharset);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURILoader> uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID));
  NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(DoChannelLoad(channel, uriLoader, false),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::CreateLoadInfo(nsIDocShellLoadInfo** aLoadInfo)
{
  nsDocShellLoadInfo* loadInfo = new nsDocShellLoadInfo();
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr<nsIDocShellLoadInfo> localRef(loadInfo);

  localRef.forget(aLoadInfo);
  return NS_OK;
}

/*
 * Reset state to a new content model within the current document and the
 * document viewer. Called by the document before initiating an out of band
 * document.write().
 */
NS_IMETHODIMP
nsDocShell::PrepareForNewContentModel()
{
  mEODForCurrentDocument = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FirePageHideNotification(bool aIsUnload)
{
  if (mContentViewer && !mFiredUnloadEvent) {
    // Keep an explicit reference since calling PageHide could release
    // mContentViewer
    nsCOMPtr<nsIContentViewer> kungFuDeathGrip(mContentViewer);
    mFiredUnloadEvent = true;

    if (mTiming) {
      mTiming->NotifyUnloadEventStart();
    }

    mContentViewer->PageHide(aIsUnload);

    if (mTiming) {
      mTiming->NotifyUnloadEventEnd();
    }

    nsAutoTArray<nsCOMPtr<nsIDocShell>, 8> kids;
    uint32_t n = mChildList.Length();
    kids.SetCapacity(n);
    for (uint32_t i = 0; i < n; i++) {
      kids.AppendElement(do_QueryInterface(ChildAt(i)));
    }

    n = kids.Length();
    for (uint32_t i = 0; i < n; ++i) {
      if (kids[i]) {
        kids[i]->FirePageHideNotification(aIsUnload);
      }
    }
    // Now make sure our editor, if any, is detached before we go
    // any farther.
    DetachEditorFromWindow();
  }

  return NS_OK;
}

void
nsDocShell::MaybeInitTiming()
{
  if (mTiming && !mBlankTiming) {
    return;
  }

  if (mScriptGlobal && mBlankTiming) {
    nsPIDOMWindow* innerWin = mScriptGlobal->GetCurrentInnerWindow();
    if (innerWin && innerWin->GetPerformance()) {
      mTiming = innerWin->GetPerformance()->GetDOMTiming();
      mBlankTiming = false;
    }
  }

  if (!mTiming) {
    mTiming = new nsDOMNavigationTiming();
  }

  mTiming->NotifyNavigationStart();
}

//
// Bug 13871: Prevent frameset spoofing
//
// This routine answers: 'Is origin's document from same domain as
// target's document?'
//
// file: uris are considered the same domain for the purpose of
// frame navigation regardless of script accessibility (bug 420425)
//
/* static */ bool
nsDocShell::ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem,
                           nsIDocShellTreeItem* aTargetTreeItem)
{
  // We want to bypass this check for chrome callers, but only if there's
  // JS on the stack. System callers still need to do it.
  if (nsContentUtils::GetCurrentJSContext() &&
      nsContentUtils::IsCallerChrome()) {
    return true;
  }

  MOZ_ASSERT(aOriginTreeItem && aTargetTreeItem, "need two docshells");

  // Get origin document principal
  nsCOMPtr<nsIDocument> originDocument = aOriginTreeItem->GetDocument();
  NS_ENSURE_TRUE(originDocument, false);

  // Get target principal
  nsCOMPtr<nsIDocument> targetDocument = aTargetTreeItem->GetDocument();
  NS_ENSURE_TRUE(targetDocument, false);

  bool equal;
  nsresult rv = originDocument->NodePrincipal()->Equals(
    targetDocument->NodePrincipal(), &equal);
  if (NS_SUCCEEDED(rv) && equal) {
    return true;
  }

  // Not strictly equal, special case if both are file: uris
  bool originIsFile = false;
  bool targetIsFile = false;
  nsCOMPtr<nsIURI> originURI;
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> innerOriginURI;
  nsCOMPtr<nsIURI> innerTargetURI;

  rv = originDocument->NodePrincipal()->GetURI(getter_AddRefs(originURI));
  if (NS_SUCCEEDED(rv) && originURI) {
    innerOriginURI = NS_GetInnermostURI(originURI);
  }

  rv = targetDocument->NodePrincipal()->GetURI(getter_AddRefs(targetURI));
  if (NS_SUCCEEDED(rv) && targetURI) {
    innerTargetURI = NS_GetInnermostURI(targetURI);
  }

  return innerOriginURI && innerTargetURI &&
         NS_SUCCEEDED(innerOriginURI->SchemeIs("file", &originIsFile)) &&
         NS_SUCCEEDED(innerTargetURI->SchemeIs("file", &targetIsFile)) &&
         originIsFile && targetIsFile;
}

nsresult
nsDocShell::GetEldestPresContext(nsPresContext** aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  *aPresContext = nullptr;

  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  while (viewer) {
    nsCOMPtr<nsIContentViewer> prevViewer;
    viewer->GetPreviousViewer(getter_AddRefs(prevViewer));
    if (!prevViewer) {
      return viewer->GetPresContext(aPresContext);
    }
    viewer = prevViewer;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPresContext(nsPresContext** aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  *aPresContext = nullptr;

  if (!mContentViewer) {
    return NS_OK;
  }

  return mContentViewer->GetPresContext(aPresContext);
}

NS_IMETHODIMP_(nsIPresShell*)
nsDocShell::GetPresShell()
{
  RefPtr<nsPresContext> presContext;
  (void)GetPresContext(getter_AddRefs(presContext));
  return presContext ? presContext->GetPresShell() : nullptr;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresShell(nsIPresShell** aPresShell)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aPresShell);
  *aPresShell = nullptr;

  RefPtr<nsPresContext> presContext;
  (void)GetEldestPresContext(getter_AddRefs(presContext));

  if (presContext) {
    NS_IF_ADDREF(*aPresShell = presContext->GetPresShell());
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::GetContentViewer(nsIContentViewer** aContentViewer)
{
  NS_ENSURE_ARG_POINTER(aContentViewer);

  *aContentViewer = mContentViewer;
  NS_IF_ADDREF(*aContentViewer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChromeEventHandler(nsIDOMEventTarget* aChromeEventHandler)
{
  // Weak reference. Don't addref.
  nsCOMPtr<EventTarget> handler = do_QueryInterface(aChromeEventHandler);
  mChromeEventHandler = handler.get();

  if (mScriptGlobal) {
    mScriptGlobal->SetChromeEventHandler(mChromeEventHandler);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChromeEventHandler(nsIDOMEventTarget** aChromeEventHandler)
{
  NS_ENSURE_ARG_POINTER(aChromeEventHandler);
  nsCOMPtr<EventTarget> handler = mChromeEventHandler;
  handler.forget(aChromeEventHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurrentURI(nsIURI* aURI)
{
  // Note that securityUI will set STATE_IS_INSECURE, even if
  // the scheme of |aURI| is "https".
  SetCurrentURI(aURI, nullptr, true, 0);
  return NS_OK;
}

bool
nsDocShell::SetCurrentURI(nsIURI* aURI, nsIRequest* aRequest,
                          bool aFireOnLocationChange, uint32_t aLocationFlags)
{
  if (gDocShellLeakLog && MOZ_LOG_TEST(gDocShellLeakLog, LogLevel::Debug)) {
    nsAutoCString spec;
    if (aURI) {
      aURI->GetSpec(spec);
    }
    PR_LogPrint("DOCSHELL %p SetCurrentURI %s\n", this, spec.get());
  }

  // We don't want to send a location change when we're displaying an error
  // page, and we don't want to change our idea of "current URI" either
  if (mLoadType == LOAD_ERROR_PAGE) {
    return false;
  }

  mCurrentURI = NS_TryToMakeImmutable(aURI);

  if (!NS_IsAboutBlank(mCurrentURI)) {
    mHasLoadedNonBlankURI = true;
  }

  bool isRoot = false;  // Is this the root docshell
  bool isSubFrame = false;  // Is this a subframe navigation?

  nsCOMPtr<nsIDocShellTreeItem> root;

  GetSameTypeRootTreeItem(getter_AddRefs(root));
  if (root.get() == static_cast<nsIDocShellTreeItem*>(this)) {
    // This is the root docshell
    isRoot = true;
  }
  if (mLSHE) {
    mLSHE->GetIsSubFrame(&isSubFrame);
  }

  if (!isSubFrame && !isRoot) {
    /*
     * We don't want to send OnLocationChange notifications when
     * a subframe is being loaded for the first time, while
     * visiting a frameset page
     */
    return false;
  }

  if (aFireOnLocationChange) {
    FireOnLocationChange(this, aRequest, aURI, aLocationFlags);
  }
  return !aFireOnLocationChange;
}

NS_IMETHODIMP
nsDocShell::GetCharset(nsACString& aCharset)
{
  aCharset.Truncate();

  nsIPresShell* presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  nsIDocument* doc = presShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  aCharset = doc->GetDocumentCharacterSet();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GatherCharsetMenuTelemetry()
{
  nsCOMPtr<nsIContentViewer> viewer;
  GetContentViewer(getter_AddRefs(viewer));
  if (!viewer) {
    return NS_OK;
  }

  nsIDocument* doc = viewer->GetDocument();
  if (!doc || doc->WillIgnoreCharsetOverride()) {
    return NS_OK;
  }

  Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_USED, true);

  bool isFileURL = false;
  nsIURI* url = doc->GetOriginalURI();
  if (url) {
    url->SchemeIs("file", &isFileURL);
  }

  int32_t charsetSource = doc->GetDocumentCharacterSetSource();
  switch (charsetSource) {
    case kCharsetFromTopLevelDomain:
      // Unlabeled doc on a domain that we map to a fallback encoding
      Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 7);
      break;
    case kCharsetFromFallback:
    case kCharsetFromDocTypeDefault:
    case kCharsetFromCache:
    case kCharsetFromParentFrame:
    case kCharsetFromHintPrevDoc:
      // Changing charset on an unlabeled doc.
      if (isFileURL) {
        Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 0);
      } else {
        Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 1);
      }
      break;
    case kCharsetFromAutoDetection:
      // Changing charset on unlabeled doc where chardet fired
      if (isFileURL) {
        Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 2);
      } else {
        Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 3);
      }
      break;
    case kCharsetFromMetaPrescan:
    case kCharsetFromMetaTag:
    case kCharsetFromChannel:
      // Changing charset on a doc that had a charset label.
      Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 4);
      break;
    case kCharsetFromParentForced:
    case kCharsetFromUserForced:
      // Changing charset on a document that already had an override.
      Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 5);
      break;
    case kCharsetFromIrreversibleAutoDetection:
    case kCharsetFromOtherComponent:
    case kCharsetFromByteOrderMark:
    case kCharsetUninitialized:
    default:
      // Bug. This isn't supposed to happen.
      Telemetry::Accumulate(Telemetry::CHARSET_OVERRIDE_SITUATION, 6);
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCharset(const nsACString& aCharset)
{
  // set the charset override
  return SetForcedCharset(aCharset);
}

NS_IMETHODIMP
nsDocShell::SetForcedCharset(const nsACString& aCharset)
{
  if (aCharset.IsEmpty()) {
    mForcedCharset.Truncate();
    return NS_OK;
  }
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabel(aCharset, encoding)) {
    // Reject unknown labels
    return NS_ERROR_INVALID_ARG;
  }
  if (!EncodingUtils::IsAsciiCompatible(encoding)) {
    // Reject XSS hazards
    return NS_ERROR_INVALID_ARG;
  }
  mForcedCharset = encoding;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetForcedCharset(nsACString& aResult)
{
  aResult = mForcedCharset;
  return NS_OK;
}

void
nsDocShell::SetParentCharset(const nsACString& aCharset,
                             int32_t aCharsetSource,
                             nsIPrincipal* aPrincipal)
{
  mParentCharset = aCharset;
  mParentCharsetSource = aCharsetSource;
  mParentCharsetPrincipal = aPrincipal;
}

void
nsDocShell::GetParentCharset(nsACString& aCharset,
                             int32_t* aCharsetSource,
                             nsIPrincipal** aPrincipal)
{
  aCharset = mParentCharset;
  *aCharsetSource = mParentCharsetSource;
  NS_IF_ADDREF(*aPrincipal = mParentCharsetPrincipal);
}

NS_IMETHODIMP
nsDocShell::GetChannelIsUnsafe(bool* aUnsafe)
{
  *aUnsafe = false;

  nsIChannel* channel = GetCurrentDocChannel();
  if (!channel) {
    return NS_OK;
  }

  nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(channel);
  if (!jarChannel) {
    return NS_OK;
  }

  return jarChannel->GetIsUnsafe(aUnsafe);
}

NS_IMETHODIMP
nsDocShell::GetHasMixedActiveContentLoaded(bool* aHasMixedActiveContentLoaded)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasMixedActiveContentLoaded = doc && doc->GetHasMixedActiveContentLoaded();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedActiveContentBlocked(bool* aHasMixedActiveContentBlocked)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasMixedActiveContentBlocked =
    doc && doc->GetHasMixedActiveContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedDisplayContentLoaded(bool* aHasMixedDisplayContentLoaded)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasMixedDisplayContentLoaded =
    doc && doc->GetHasMixedDisplayContentLoaded();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedDisplayContentBlocked(
  bool* aHasMixedDisplayContentBlocked)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasMixedDisplayContentBlocked =
    doc && doc->GetHasMixedDisplayContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasTrackingContentBlocked(bool* aHasTrackingContentBlocked)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasTrackingContentBlocked = doc && doc->GetHasTrackingContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasTrackingContentLoaded(bool* aHasTrackingContentLoaded)
{
  nsCOMPtr<nsIDocument> doc(GetDocument());
  *aHasTrackingContentLoaded = doc && doc->GetHasTrackingContentLoaded();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowPlugins(bool* aAllowPlugins)
{
  NS_ENSURE_ARG_POINTER(aAllowPlugins);

  *aAllowPlugins = mAllowPlugins;
  if (!mAllowPlugins) {
    return NS_OK;
  }

  bool unsafe;
  *aAllowPlugins = NS_SUCCEEDED(GetChannelIsUnsafe(&unsafe)) && !unsafe;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowPlugins(bool aAllowPlugins)
{
  mAllowPlugins = aAllowPlugins;
  // XXX should enable or disable a plugin host
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowJavascript(bool* aAllowJavascript)
{
  NS_ENSURE_ARG_POINTER(aAllowJavascript);

  *aAllowJavascript = mAllowJavascript;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowJavascript(bool aAllowJavascript)
{
  mAllowJavascript = aAllowJavascript;
  RecomputeCanExecuteScripts();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUsePrivateBrowsing(bool* aUsePrivateBrowsing)
{
  NS_ENSURE_ARG_POINTER(aUsePrivateBrowsing);

  *aUsePrivateBrowsing = mInPrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUsePrivateBrowsing(bool aUsePrivateBrowsing)
{
  nsContentUtils::ReportToConsoleNonLocalized(
    NS_LITERAL_STRING("Only internal code is allowed to set the usePrivateBrowsing attribute"),
    nsIScriptError::warningFlag,
    NS_LITERAL_CSTRING("Internal API Used"),
    mContentViewer ? mContentViewer->GetDocument() : nullptr);

  return SetPrivateBrowsing(aUsePrivateBrowsing);
}

NS_IMETHODIMP
nsDocShell::SetPrivateBrowsing(bool aUsePrivateBrowsing)
{
  bool changed = aUsePrivateBrowsing != mInPrivateBrowsing;
  if (changed) {
    mInPrivateBrowsing = aUsePrivateBrowsing;
    if (mAffectPrivateSessionLifetime) {
      if (aUsePrivateBrowsing) {
        IncreasePrivateDocShellCount();
      } else {
        DecreasePrivateDocShellCount();
      }
    }
  }

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsILoadContext> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->SetPrivateBrowsing(aUsePrivateBrowsing);
    }
  }

  if (changed) {
    nsTObserverArray<nsWeakPtr>::ForwardIterator iter(mPrivacyObservers);
    while (iter.HasMore()) {
      nsWeakPtr ref = iter.GetNext();
      nsCOMPtr<nsIPrivacyTransitionObserver> obs = do_QueryReferent(ref);
      if (!obs) {
        mPrivacyObservers.RemoveElement(ref);
      } else {
        obs->PrivateModeChanged(aUsePrivateBrowsing);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasLoadedNonBlankURI(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mHasLoadedNonBlankURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseRemoteTabs(bool* aUseRemoteTabs)
{
  NS_ENSURE_ARG_POINTER(aUseRemoteTabs);

  *aUseRemoteTabs = mUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetRemoteTabs(bool aUseRemoteTabs)
{
#ifdef MOZ_CRASHREPORTER
  if (aUseRemoteTabs) {
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("DOMIPCEnabled"),
                                       NS_LITERAL_CSTRING("1"));
  }
#endif

  mUseRemoteTabs = aUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAffectPrivateSessionLifetime(bool aAffectLifetime)
{
  bool change = aAffectLifetime != mAffectPrivateSessionLifetime;
  if (change && mInPrivateBrowsing) {
    if (aAffectLifetime) {
      IncreasePrivateDocShellCount();
    } else {
      DecreasePrivateDocShellCount();
    }
  }
  mAffectPrivateSessionLifetime = aAffectLifetime;

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->SetAffectPrivateSessionLifetime(aAffectLifetime);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAffectPrivateSessionLifetime(bool* aAffectLifetime)
{
  *aAffectLifetime = mAffectPrivateSessionLifetime;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddWeakPrivacyTransitionObserver(
    nsIPrivacyTransitionObserver* aObserver)
{
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mPrivacyObservers.AppendElement(weakObs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::AddWeakReflowObserver(nsIReflowObserver* aObserver)
{
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_FAILURE;
  }
  return mReflowObservers.AppendElement(weakObs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::RemoveWeakReflowObserver(nsIReflowObserver* aObserver)
{
  nsWeakPtr obs = do_GetWeakReference(aObserver);
  return mReflowObservers.RemoveElement(obs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::NotifyReflowObservers(bool aInterruptible,
                                  DOMHighResTimeStamp aStart,
                                  DOMHighResTimeStamp aEnd)
{
  nsTObserverArray<nsWeakPtr>::ForwardIterator iter(mReflowObservers);
  while (iter.HasMore()) {
    nsWeakPtr ref = iter.GetNext();
    nsCOMPtr<nsIReflowObserver> obs = do_QueryReferent(ref);
    if (!obs) {
      mReflowObservers.RemoveElement(ref);
    } else if (aInterruptible) {
      obs->ReflowInterruptible(aStart, aEnd);
    } else {
      obs->Reflow(aStart, aEnd);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowMetaRedirects(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = mAllowMetaRedirects;
  if (!mAllowMetaRedirects) {
    return NS_OK;
  }

  bool unsafe;
  *aReturn = NS_SUCCEEDED(GetChannelIsUnsafe(&unsafe)) && !unsafe;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowMetaRedirects(bool aValue)
{
  mAllowMetaRedirects = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowSubframes(bool* aAllowSubframes)
{
  NS_ENSURE_ARG_POINTER(aAllowSubframes);

  *aAllowSubframes = mAllowSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowSubframes(bool aAllowSubframes)
{
  mAllowSubframes = aAllowSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowImages(bool* aAllowImages)
{
  NS_ENSURE_ARG_POINTER(aAllowImages);

  *aAllowImages = mAllowImages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowImages(bool aAllowImages)
{
  mAllowImages = aAllowImages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowMedia(bool* aAllowMedia)
{
  *aAllowMedia = mAllowMedia;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowMedia(bool aAllowMedia)
{
  mAllowMedia = aAllowMedia;

  // Mute or unmute audio contexts attached to the inner window.
  if (mScriptGlobal) {
    nsPIDOMWindow* innerWin = mScriptGlobal->GetCurrentInnerWindow();
    if (innerWin) {
      if (aAllowMedia) {
        innerWin->UnmuteAudioContexts();
      } else {
        innerWin->MuteAudioContexts();
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowDNSPrefetch(bool* aAllowDNSPrefetch)
{
  *aAllowDNSPrefetch = mAllowDNSPrefetch;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowDNSPrefetch(bool aAllowDNSPrefetch)
{
  mAllowDNSPrefetch = aAllowDNSPrefetch;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowWindowControl(bool* aAllowWindowControl)
{
  *aAllowWindowControl = mAllowWindowControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowWindowControl(bool aAllowWindowControl)
{
  mAllowWindowControl = aAllowWindowControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowContentRetargeting(bool* aAllowContentRetargeting)
{
  *aAllowContentRetargeting = mAllowContentRetargeting;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowContentRetargeting(bool aAllowContentRetargeting)
{
  mAllowContentRetargetingOnChildren = aAllowContentRetargeting;
  mAllowContentRetargeting = aAllowContentRetargeting;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowContentRetargetingOnChildren(
    bool* aAllowContentRetargetingOnChildren)
{
  *aAllowContentRetargetingOnChildren = mAllowContentRetargetingOnChildren;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowContentRetargetingOnChildren(
    bool aAllowContentRetargetingOnChildren)
{
  mAllowContentRetargetingOnChildren = aAllowContentRetargetingOnChildren;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetFullscreenAllowed(bool* aFullscreenAllowed)
{
  NS_ENSURE_ARG_POINTER(aFullscreenAllowed);

  // Browsers and apps have their mFullscreenAllowed retrieved from their
  // corresponding iframe in their parent upon creation.
  if (mFullscreenAllowed != CHECK_ATTRIBUTES) {
    *aFullscreenAllowed = (mFullscreenAllowed == PARENT_ALLOWS);
    return NS_OK;
  }

  // Assume false until we determine otherwise...
  *aFullscreenAllowed = false;

  nsCOMPtr<nsPIDOMWindow> win = GetWindow();
  if (!win) {
    return NS_OK;
  }
  nsCOMPtr<Element> frameElement = win->GetFrameElementInternal();
  if (frameElement && !frameElement->IsXULElement()) {
    // We do not allow document inside any containing element other
    // than iframe to enter fullscreen.
    if (!frameElement->IsHTMLElement(nsGkAtoms::iframe)) {
      return NS_OK;
    }
    // If any ancestor iframe does not have allowfullscreen attribute
    // set, then fullscreen is not allowed.
    if (!frameElement->HasAttr(kNameSpaceID_None,
                               nsGkAtoms::allowfullscreen) &&
        !frameElement->HasAttr(kNameSpaceID_None,
                               nsGkAtoms::mozallowfullscreen)) {
      return NS_OK;
    }
  }

  // If we have no parent then we're the root docshell; no ancestor of the
  // original docshell doesn't have a allowfullscreen attribute, so
  // report fullscreen as allowed.
  RefPtr<nsDocShell> parent = GetParentDocshell();
  if (!parent) {
    *aFullscreenAllowed = true;
    return NS_OK;
  }

  // Otherwise, we have a parent, continue the checking for
  // mozFullscreenAllowed in the parent docshell's ancestors.
  return parent->GetFullscreenAllowed(aFullscreenAllowed);
}

NS_IMETHODIMP
nsDocShell::SetFullscreenAllowed(bool aFullscreenAllowed)
{
  if (!nsIDocShell::GetIsBrowserOrApp()) {
    // Only allow setting of fullscreenAllowed on content/process boundaries.
    // At non-boundaries the fullscreenAllowed attribute is calculated based on
    // whether all enclosing frames have the "mozFullscreenAllowed" attribute
    // set to "true". fullscreenAllowed is set at the process boundaries to
    // propagate the value of the parent's "mozFullscreenAllowed" attribute
    // across process boundaries.
    return NS_ERROR_UNEXPECTED;
  }
  mFullscreenAllowed = (aFullscreenAllowed ? PARENT_ALLOWS : PARENT_PROHIBITS);
  return NS_OK;
}

ScreenOrientationInternal
nsDocShell::OrientationLock()
{
  return mOrientationLock;
}

void
nsDocShell::SetOrientationLock(ScreenOrientationInternal aOrientationLock)
{
  mOrientationLock = aOrientationLock;
}

NS_IMETHODIMP
nsDocShell::GetMayEnableCharacterEncodingMenu(
    bool* aMayEnableCharacterEncodingMenu)
{
  *aMayEnableCharacterEncodingMenu = false;
  if (!mContentViewer) {
    return NS_OK;
  }
  nsIDocument* doc = mContentViewer->GetDocument();
  if (!doc) {
    return NS_OK;
  }
  if (doc->WillIgnoreCharsetOverride()) {
    return NS_OK;
  }

  *aMayEnableCharacterEncodingMenu = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocShellEnumerator(int32_t aItemType, int32_t aDirection,
                                  nsISimpleEnumerator** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  RefPtr<nsDocShellEnumerator> docShellEnum;
  if (aDirection == ENUMERATE_FORWARDS) {
    docShellEnum = new nsDocShellForwardsEnumerator;
  } else {
    docShellEnum = new nsDocShellBackwardsEnumerator;
  }

  if (!docShellEnum) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = docShellEnum->SetEnumDocShellType(aItemType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = docShellEnum->SetEnumerationRootItem((nsIDocShellTreeItem*)this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = docShellEnum->First();
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = docShellEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator),
                                    (void**)aResult);

  return rv;
}

NS_IMETHODIMP
nsDocShell::GetAppType(uint32_t* aAppType)
{
  *aAppType = mAppType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAppType(uint32_t aAppType)
{
  mAppType = aAppType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowAuth(bool* aAllowAuth)
{
  *aAllowAuth = mAllowAuth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowAuth(bool aAllowAuth)
{
  mAllowAuth = aAllowAuth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetZoom(float* aZoom)
{
  NS_ENSURE_ARG_POINTER(aZoom);
  *aZoom = 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetZoom(float aZoom)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetMarginWidth(int32_t* aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);

  *aWidth = mMarginWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginWidth(int32_t aWidth)
{
  mMarginWidth = aWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMarginHeight(int32_t* aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);

  *aHeight = mMarginHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginHeight(int32_t aHeight)
{
  mMarginHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetBusyFlags(uint32_t* aBusyFlags)
{
  NS_ENSURE_ARG_POINTER(aBusyFlags);

  *aBusyFlags = mBusyFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::TabToTreeOwner(bool aForward, bool aForDocumentNavigation, bool* aTookFocus)
{
  NS_ENSURE_ARG_POINTER(aTookFocus);

  nsCOMPtr<nsIWebBrowserChromeFocus> chromeFocus = do_GetInterface(mTreeOwner);
  if (chromeFocus) {
    if (aForward) {
      *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusNextElement(aForDocumentNavigation));
    } else {
      *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusPrevElement(aForDocumentNavigation));
    }
  } else {
    *aTookFocus = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSecurityUI(nsISecureBrowserUI** aSecurityUI)
{
  NS_IF_ADDREF(*aSecurityUI = mSecurityUI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSecurityUI(nsISecureBrowserUI* aSecurityUI)
{
  mSecurityUI = aSecurityUI;
  mSecurityUI->SetDocShell(this);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseErrorPages(bool* aUseErrorPages)
{
  *aUseErrorPages = UseErrorPages();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseErrorPages(bool aUseErrorPages)
{
  // If mUseErrorPages is set explicitly, stop using sUseErrorPages.
  if (mObserveErrorPages) {
    mObserveErrorPages = false;
  }
  mUseErrorPages = aUseErrorPages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPreviousTransIndex(int32_t* aPreviousTransIndex)
{
  *aPreviousTransIndex = mPreviousTransIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadedTransIndex(int32_t* aLoadedTransIndex)
{
  *aLoadedTransIndex = mLoadedTransIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::HistoryPurged(int32_t aNumEntries)
{
  // These indices are used for fastback cache eviction, to determine
  // which session history entries are candidates for content viewer
  // eviction.  We need to adjust by the number of entries that we
  // just purged from history, so that we look at the right session history
  // entries during eviction.
  mPreviousTransIndex = std::max(-1, mPreviousTransIndex - aNumEntries);
  mLoadedTransIndex = std::max(0, mLoadedTransIndex - aNumEntries);

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->HistoryPurged(aNumEntries);
    }
  }

  return NS_OK;
}

nsresult
nsDocShell::HistoryTransactionRemoved(int32_t aIndex)
{
  // These indices are used for fastback cache eviction, to determine
  // which session history entries are candidates for content viewer
  // eviction.  We need to adjust by the number of entries that we
  // just purged from history, so that we look at the right session history
  // entries during eviction.
  if (aIndex == mPreviousTransIndex) {
    mPreviousTransIndex = -1;
  } else if (aIndex < mPreviousTransIndex) {
    --mPreviousTransIndex;
  }
  if (mLoadedTransIndex == aIndex) {
    mLoadedTransIndex = 0;
  } else if (aIndex < mLoadedTransIndex) {
    --mLoadedTransIndex;
  }

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      static_cast<nsDocShell*>(shell.get())->HistoryTransactionRemoved(aIndex);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetRecordProfileTimelineMarkers(bool aValue)
{
  bool currentValue = nsIDocShell::GetRecordProfileTimelineMarkers();
  if (currentValue != aValue) {
    if (aValue) {
      TimelineConsumers::AddConsumer(this);
      UseEntryScriptProfiling();
    } else {
      TimelineConsumers::RemoveConsumer(this);
      UnuseEntryScriptProfiling();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRecordProfileTimelineMarkers(bool* aValue)
{
  *aValue = IsObserved();
  return NS_OK;
}

nsresult
nsDocShell::PopProfileTimelineMarkers(
    JSContext* aCx,
    JS::MutableHandle<JS::Value> aOut)
{
  nsTArray<dom::ProfileTimelineMarker> store;
  SequenceRooter<dom::ProfileTimelineMarker> rooter(aCx, &store);

  if (IsObserved()) {
    mObserved->PopMarkers(aCx, store);
  }

  if (!ToJSValue(aCx, store, aOut)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsDocShell::Now(DOMHighResTimeStamp* aWhen)
{
  bool ignore;
  *aWhen =
    (TimeStamp::Now() - TimeStamp::ProcessCreation(ignore)).ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetWindowDraggingAllowed(bool aValue)
{
  RefPtr<nsDocShell> parent = GetParentDocshell();
  if (!aValue && mItemType == typeChrome && !parent) {
    // Window dragging is always allowed for top level
    // chrome docshells.
    return NS_ERROR_FAILURE;
  }
  mWindowDraggingAllowed = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetWindowDraggingAllowed(bool* aValue)
{
  // window dragging regions in CSS (-moz-window-drag:drag)
  // can be slow. Default behavior is to only allow it for
  // chrome top level windows.
  RefPtr<nsDocShell> parent = GetParentDocshell();
  if (mItemType == typeChrome && !parent) {
    // Top level chrome window
    *aValue = true;
  } else {
    *aValue = mWindowDraggingAllowed;
  }
  return NS_OK;
}

nsIDOMStorageManager*
nsDocShell::TopSessionStorageManager()
{
  nsresult rv;

  nsCOMPtr<nsIDocShellTreeItem> topItem;
  rv = GetSameTypeRootTreeItem(getter_AddRefs(topItem));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  if (!topItem) {
    return nullptr;
  }

  nsDocShell* topDocShell = static_cast<nsDocShell*>(topItem.get());
  if (topDocShell != this) {
    return topDocShell->TopSessionStorageManager();
  }

  if (!mSessionStorageManager) {
    mSessionStorageManager =
      do_CreateInstance("@mozilla.org/dom/sessionStorage-manager;1");
  }

  return mSessionStorageManager;
}

NS_IMETHODIMP
nsDocShell::GetSessionStorageForPrincipal(nsIPrincipal* aPrincipal,
                                          const nsAString& aDocumentURI,
                                          bool aCreate,
                                          nsIDOMStorage** aStorage)
{
  nsCOMPtr<nsIDOMStorageManager> manager = TopSessionStorageManager();
  if (!manager) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(GetAsSupports(this));

  if (aCreate) {
    return manager->CreateStorage(domWin, aPrincipal, aDocumentURI,
                                  mInPrivateBrowsing, aStorage);
  }

  return manager->GetStorage(domWin, aPrincipal, mInPrivateBrowsing, aStorage);
}

nsresult
nsDocShell::AddSessionStorage(nsIPrincipal* aPrincipal, nsIDOMStorage* aStorage)
{
  RefPtr<DOMStorage> storage = static_cast<DOMStorage*>(aStorage);
  if (!storage) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIPrincipal* storagePrincipal = storage->GetPrincipal();
  if (storagePrincipal != aPrincipal) {
    NS_ERROR("Wanting to add a sessionStorage for different principal");
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIDOMStorageManager> manager = TopSessionStorageManager();
  if (!manager) {
    return NS_ERROR_UNEXPECTED;
  }

  return manager->CloneStorage(aStorage);
}

NS_IMETHODIMP
nsDocShell::GetCurrentDocumentChannel(nsIChannel** aResult)
{
  NS_IF_ADDREF(*aResult = GetCurrentDocChannel());
  return NS_OK;
}

nsIChannel*
nsDocShell::GetCurrentDocChannel()
{
  if (mContentViewer) {
    nsIDocument* doc = mContentViewer->GetDocument();
    if (doc) {
      return doc->GetChannel();
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsDocShell::AddWeakScrollObserver(nsIScrollObserver* aObserver)
{
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_FAILURE;
  }
  return mScrollObservers.AppendElement(weakObs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::RemoveWeakScrollObserver(nsIScrollObserver* aObserver)
{
  nsWeakPtr obs = do_GetWeakReference(aObserver);
  return mScrollObservers.RemoveElement(obs) ? NS_OK : NS_ERROR_FAILURE;
}

void
nsDocShell::NotifyAsyncPanZoomStarted()
{
  nsTObserverArray<nsWeakPtr>::ForwardIterator iter(mScrollObservers);
  while (iter.HasMore()) {
    nsWeakPtr ref = iter.GetNext();
    nsCOMPtr<nsIScrollObserver> obs = do_QueryReferent(ref);
    if (obs) {
      obs->AsyncPanZoomStarted();
    } else {
      mScrollObservers.RemoveElement(ref);
    }
  }

  // Also notify child docshell
  for (uint32_t i = 0; i < mChildList.Length(); ++i) {
    nsCOMPtr<nsIDocShell> kid = do_QueryInterface(ChildAt(i));
    if (kid) {
      nsDocShell* docShell = static_cast<nsDocShell*>(kid.get());
      docShell->NotifyAsyncPanZoomStarted();
    }
  }
}

void
nsDocShell::NotifyAsyncPanZoomStopped()
{
  nsTObserverArray<nsWeakPtr>::ForwardIterator iter(mScrollObservers);
  while (iter.HasMore()) {
    nsWeakPtr ref = iter.GetNext();
    nsCOMPtr<nsIScrollObserver> obs = do_QueryReferent(ref);
    if (obs) {
      obs->AsyncPanZoomStopped();
    } else {
      mScrollObservers.RemoveElement(ref);
    }
  }

  // Also notify child docshell
  for (uint32_t i = 0; i < mChildList.Length(); ++i) {
    nsCOMPtr<nsIDocShell> kid = do_QueryInterface(ChildAt(i));
    if (kid) {
      nsDocShell* docShell = static_cast<nsDocShell*>(kid.get());
      docShell->NotifyAsyncPanZoomStopped();
    }
  }
}

NS_IMETHODIMP
nsDocShell::NotifyScrollObservers()
{
  nsTObserverArray<nsWeakPtr>::ForwardIterator iter(mScrollObservers);
  while (iter.HasMore()) {
    nsWeakPtr ref = iter.GetNext();
    nsCOMPtr<nsIScrollObserver> obs = do_QueryReferent(ref);
    if (obs) {
      obs->ScrollPositionChanged();
    } else {
      mScrollObservers.RemoveElement(ref);
    }
  }
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellTreeItem
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetName(const nsAString& aName)
{
  mName = aName;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::NameEquals(const char16_t* aName, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mName.Equals(aName);
  return NS_OK;
}

/* virtual */ int32_t
nsDocShell::ItemType()
{
  return mItemType;
}

NS_IMETHODIMP
nsDocShell::GetItemType(int32_t* aItemType)
{
  NS_ENSURE_ARG_POINTER(aItemType);

  *aItemType = ItemType();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetItemType(int32_t aItemType)
{
  NS_ENSURE_ARG((aItemType == typeChrome) || (typeContent == aItemType));

  // Only allow setting the type on root docshells.  Those would be the ones
  // that have the docloader service as mParent or have no mParent at all.
  nsCOMPtr<nsIDocumentLoader> docLoaderService =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(docLoaderService, NS_ERROR_UNEXPECTED);

  NS_ENSURE_STATE(!mParent || mParent == docLoaderService);

  mItemType = aItemType;

  // disable auth prompting for anything but content
  mAllowAuth = mItemType == typeContent;

  RefPtr<nsPresContext> presContext = nullptr;
  GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    presContext->UpdateIsChrome();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParent(nsIDocShellTreeItem** aParent)
{
  if (!mParent) {
    *aParent = nullptr;
  } else {
    CallQueryInterface(mParent, aParent);
  }
  // Note that in the case when the parent is not an nsIDocShellTreeItem we
  // don't want to throw; we just want to return null.
  return NS_OK;
}

already_AddRefed<nsDocShell>
nsDocShell::GetParentDocshell()
{
  nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(GetAsSupports(mParent));
  return docshell.forget().downcast<nsDocShell>();
}

void
nsDocShell::RecomputeCanExecuteScripts()
{
  bool old = mCanExecuteScripts;
  RefPtr<nsDocShell> parent = GetParentDocshell();

  // If we have no tree owner, that means that we've been detached from the
  // docshell tree (this is distinct from having no parent dochshell, which
  // is the case for root docshells). It would be nice to simply disallow
  // script in detached docshells, but bug 986542 demonstrates that this
  // behavior breaks at least one website.
  //
  // So instead, we use our previous value, unless mAllowJavascript has been
  // explicitly set to false.
  if (!mTreeOwner) {
    mCanExecuteScripts = mCanExecuteScripts && mAllowJavascript;
    // If scripting has been explicitly disabled on our docshell, we're done.
  } else if (!mAllowJavascript) {
    mCanExecuteScripts = false;
    // If we have a parent, inherit.
  } else if (parent) {
    mCanExecuteScripts = parent->mCanExecuteScripts;
    // Otherwise, we're the root of the tree, and we haven't explicitly disabled
    // script. Allow.
  } else {
    mCanExecuteScripts = true;
  }

  // Inform our active DOM window.
  //
  // This will pass the outer, which will be in the scope of the active inner.
  if (mScriptGlobal && mScriptGlobal->GetGlobalJSObject()) {
    xpc::Scriptability& scriptability =
      xpc::Scriptability::Get(mScriptGlobal->GetGlobalJSObject());
    scriptability.SetDocShellAllowsScript(mCanExecuteScripts);
  }

  // If our value has changed, our children might be affected. Recompute their
  // value as well.
  if (old != mCanExecuteScripts) {
    nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
    while (iter.HasMore()) {
      static_cast<nsDocShell*>(iter.GetNext())->RecomputeCanExecuteScripts();
    }
  }
}

nsresult
nsDocShell::SetDocLoaderParent(nsDocLoader* aParent)
{
  bool wasFrame = IsFrame();

  nsDocLoader::SetDocLoaderParent(aParent);

  nsCOMPtr<nsISupportsPriority> priorityGroup = do_QueryInterface(mLoadGroup);
  if (wasFrame != IsFrame() && priorityGroup) {
    priorityGroup->AdjustPriority(wasFrame ? -1 : 1);
  }

  // Curse ambiguous nsISupports inheritance!
  nsISupports* parent = GetAsSupports(aParent);

  // If parent is another docshell, we inherit all their flags for
  // allowing plugins, scripting etc.
  bool value;
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(parent));
  if (parentAsDocShell) {
    if (mAllowPlugins && NS_SUCCEEDED(parentAsDocShell->GetAllowPlugins(&value))) {
      SetAllowPlugins(value);
    }
    if (mAllowJavascript && NS_SUCCEEDED(parentAsDocShell->GetAllowJavascript(&value))) {
      SetAllowJavascript(value);
    }
    if (mAllowMetaRedirects && NS_SUCCEEDED(parentAsDocShell->GetAllowMetaRedirects(&value))) {
      SetAllowMetaRedirects(value);
    }
    if (mAllowSubframes && NS_SUCCEEDED(parentAsDocShell->GetAllowSubframes(&value))) {
      SetAllowSubframes(value);
    }
    if (mAllowImages && NS_SUCCEEDED(parentAsDocShell->GetAllowImages(&value))) {
      SetAllowImages(value);
    }
    SetAllowMedia(parentAsDocShell->GetAllowMedia() && mAllowMedia);
    if (mAllowWindowControl && NS_SUCCEEDED(parentAsDocShell->GetAllowWindowControl(&value))) {
      SetAllowWindowControl(value);
    }
    SetAllowContentRetargeting(mAllowContentRetargeting &&
      parentAsDocShell->GetAllowContentRetargetingOnChildren());
    if (NS_SUCCEEDED(parentAsDocShell->GetIsActive(&value))) {
      SetIsActive(value);
    }
    if (parentAsDocShell->GetIsPrerendered()) {
      SetIsPrerendered(true);
    }
    if (NS_FAILED(parentAsDocShell->GetAllowDNSPrefetch(&value))) {
      value = false;
    }
    SetAllowDNSPrefetch(mAllowDNSPrefetch && value);
    value = parentAsDocShell->GetAffectPrivateSessionLifetime();
    SetAffectPrivateSessionLifetime(value);
    uint32_t flags;
    if (NS_SUCCEEDED(parentAsDocShell->GetDefaultLoadFlags(&flags))) {
      SetDefaultLoadFlags(flags);
    }
  }

  nsCOMPtr<nsILoadContext> parentAsLoadContext(do_QueryInterface(parent));
  if (parentAsLoadContext &&
      NS_SUCCEEDED(parentAsLoadContext->GetUsePrivateBrowsing(&value))) {
    SetPrivateBrowsing(value);
  }

  nsCOMPtr<nsIURIContentListener> parentURIListener(do_GetInterface(parent));
  if (parentURIListener) {
    mContentListener->SetParentContentListener(parentURIListener);
  }

  // Our parent has changed. Recompute scriptability.
  RecomputeCanExecuteScripts();

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParent(nsIDocShellTreeItem** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nullptr;

  if (nsIDocShell::GetIsBrowserOrApp()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent =
    do_QueryInterface(GetAsSupports(mParent));
  if (!parent) {
    return NS_OK;
  }

  if (parent->ItemType() == mItemType) {
    parent.swap(*aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParentIgnoreBrowserAndAppBoundaries(nsIDocShell** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nullptr;

  nsCOMPtr<nsIDocShellTreeItem> parent =
    do_QueryInterface(GetAsSupports(mParent));
  if (!parent) {
    return NS_OK;
  }

  if (parent->ItemType() == mItemType) {
    nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parent);
    parentDS.forget(aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
{
  NS_ENSURE_ARG_POINTER(aRootTreeItem);

  RefPtr<nsDocShell> root = this;
  RefPtr<nsDocShell> parent = root->GetParentDocshell();
  while (parent) {
    root = parent;
    parent = root->GetParentDocshell();
  }

  root.forget(aRootTreeItem);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeRootTreeItem(nsIDocShellTreeItem** aRootTreeItem)
{
  NS_ENSURE_ARG_POINTER(aRootTreeItem);
  *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)),
                    NS_ERROR_FAILURE);
  while (parent) {
    *aRootTreeItem = parent;
    NS_ENSURE_SUCCESS(
      (*aRootTreeItem)->GetSameTypeParent(getter_AddRefs(parent)),
      NS_ERROR_FAILURE);
  }
  NS_ADDREF(*aRootTreeItem);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeRootTreeItemIgnoreBrowserAndAppBoundaries(nsIDocShell ** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = static_cast<nsIDocShell *>(this);

    nsCOMPtr<nsIDocShell> parent;
    NS_ENSURE_SUCCESS(GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent)),
                      NS_ERROR_FAILURE);
    while (parent) {
      *aRootTreeItem = parent;
      NS_ENSURE_SUCCESS((*aRootTreeItem)->
        GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent)),
        NS_ERROR_FAILURE);
    }
    NS_ADDREF(*aRootTreeItem);
    return NS_OK;
}

/* static */
bool
nsDocShell::CanAccessItem(nsIDocShellTreeItem* aTargetItem,
                          nsIDocShellTreeItem* aAccessingItem,
                          bool aConsiderOpener)
{
  NS_PRECONDITION(aTargetItem, "Must have target item!");

  if (!gValidateOrigin || !aAccessingItem) {
    // Good to go
    return true;
  }

  // XXXbz should we care if aAccessingItem or the document therein is
  // chrome?  Should those get extra privileges?

  // For historical context, see:
  //
  // Bug 13871:  Prevent frameset spoofing
  // Bug 103638: Targets with same name in different windows open in wrong
  //             window with javascript
  // Bug 408052: Adopt "ancestor" frame navigation policy

  // Now do a security check.
  //
  // Disallow navigation if the two frames are not part of the same app, or if
  // they have different is-in-browser-element states.
  //
  // Allow navigation if
  //  1) aAccessingItem can script aTargetItem or one of its ancestors in
  //     the frame hierarchy or
  //  2) aTargetItem is a top-level frame and aAccessingItem is its descendant
  //  3) aTargetItem is a top-level frame and aAccessingItem can target
  //     its opener per rule (1) or (2).

  if (aTargetItem == aAccessingItem) {
    // A frame is allowed to navigate itself.
    return true;
  }

  nsCOMPtr<nsIDocShell> targetDS = do_QueryInterface(aTargetItem);
  nsCOMPtr<nsIDocShell> accessingDS = do_QueryInterface(aAccessingItem);
  if (!!targetDS != !!accessingDS) {
    // We must be able to convert both or neither to nsIDocShell.
    return false;
  }

  if (targetDS && accessingDS &&
      (targetDS->GetIsInBrowserElement() !=
         accessingDS->GetIsInBrowserElement() ||
       targetDS->GetAppId() != accessingDS->GetAppId())) {
    return false;
  }

  // A private document can't access a non-private one, and vice versa.
  if (aTargetItem->GetDocument()->GetLoadContext()->UsePrivateBrowsing() !=
      aAccessingItem->GetDocument()->GetLoadContext()->UsePrivateBrowsing())
  {
    return false;
  }


  nsCOMPtr<nsIDocShellTreeItem> accessingRoot;
  aAccessingItem->GetSameTypeRootTreeItem(getter_AddRefs(accessingRoot));

  if (aTargetItem == accessingRoot) {
    // A frame can navigate its root.
    return true;
  }

  // Check if aAccessingItem can navigate one of aTargetItem's ancestors.
  nsCOMPtr<nsIDocShellTreeItem> target = aTargetItem;
  do {
    if (ValidateOrigin(aAccessingItem, target)) {
      return true;
    }

    nsCOMPtr<nsIDocShellTreeItem> parent;
    target->GetSameTypeParent(getter_AddRefs(parent));
    parent.swap(target);
  } while (target);

  nsCOMPtr<nsIDocShellTreeItem> targetRoot;
  aTargetItem->GetSameTypeRootTreeItem(getter_AddRefs(targetRoot));

  if (aTargetItem != targetRoot) {
    // target is a subframe, not in accessor's frame hierarchy, and all its
    // ancestors have origins different from that of the accessor. Don't
    // allow access.
    return false;
  }

  if (!aConsiderOpener) {
    // All done here
    return false;
  }

  nsCOMPtr<nsIDOMWindow> targetWindow = aTargetItem->GetWindow();
  if (!targetWindow) {
    NS_ERROR("This should not happen, really");
    return false;
  }

  nsCOMPtr<nsIDOMWindow> targetOpener;
  targetWindow->GetOpener(getter_AddRefs(targetOpener));
  nsCOMPtr<nsIWebNavigation> openerWebNav(do_GetInterface(targetOpener));
  nsCOMPtr<nsIDocShellTreeItem> openerItem(do_QueryInterface(openerWebNav));

  if (!openerItem) {
    return false;
  }

  return CanAccessItem(openerItem, aAccessingItem, false);
}

static bool
ItemIsActive(nsIDocShellTreeItem* aItem)
{
  if (nsCOMPtr<nsIDOMWindow> window = aItem->GetWindow()) {
    auto* win = static_cast<nsGlobalWindow*>(window.get());
    MOZ_ASSERT(win->IsOuterWindow());
    if (!win->GetClosedOuter()) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
nsDocShell::FindItemWithName(const char16_t* aName,
                             nsISupports* aRequestor,
                             nsIDocShellTreeItem* aOriginalRequestor,
                             nsIDocShellTreeItem** aResult)
{
  NS_ENSURE_ARG(aName);
  NS_ENSURE_ARG_POINTER(aResult);

  // If we don't find one, we return NS_OK and a null result
  *aResult = nullptr;

  if (!*aName) {
    return NS_OK;
  }

  if (aRequestor) {
    // If aRequestor is not null we don't need to check special names, so
    // just hand straight off to the search by actual name function.
    return DoFindItemWithName(aName, aRequestor, aOriginalRequestor, aResult);
  } else {
    // This is the entry point into the target-finding algorithm.  Check
    // for special names.  This should only be done once, hence the check
    // for a null aRequestor.

    nsCOMPtr<nsIDocShellTreeItem> foundItem;
    nsDependentString name(aName);
    if (name.LowerCaseEqualsLiteral("_self")) {
      foundItem = this;
    } else if (name.LowerCaseEqualsLiteral("_blank")) {
      // Just return null.  Caller must handle creating a new window with
      // a blank name himself.
      return NS_OK;
    } else if (name.LowerCaseEqualsLiteral("_parent")) {
      GetSameTypeParent(getter_AddRefs(foundItem));
      if (!foundItem) {
        foundItem = this;
      }
    } else if (name.LowerCaseEqualsLiteral("_top")) {
      GetSameTypeRootTreeItem(getter_AddRefs(foundItem));
      NS_ASSERTION(foundItem, "Must have this; worst case it's us!");
    }
    // _main is an IE target which should be case-insensitive but isn't
    // see bug 217886 for details
    else if (name.LowerCaseEqualsLiteral("_content") ||
             name.EqualsLiteral("_main")) {
      // Must pass our same type root as requestor to the
      // treeowner to make sure things work right.
      nsCOMPtr<nsIDocShellTreeItem> root;
      GetSameTypeRootTreeItem(getter_AddRefs(root));
      if (mTreeOwner) {
        NS_ASSERTION(root, "Must have this; worst case it's us!");
        mTreeOwner->FindItemWithName(aName, root, aOriginalRequestor,
                                     getter_AddRefs(foundItem));
      }
#ifdef DEBUG
      else {
        NS_ERROR("Someone isn't setting up the tree owner.  "
                 "You might like to try that.  "
                 "Things will.....you know, work.");
        // Note: _content should always exist.  If we don't have one
        // hanging off the treeowner, just create a named window....
        // so don't return here, in case we did that and can now find
        // it.
        // XXXbz should we be using |root| instead of creating
        // a new window?
      }
#endif
    } else {
      // Do the search for item by an actual name.
      DoFindItemWithName(aName, aRequestor, aOriginalRequestor,
                         getter_AddRefs(foundItem));
    }

    if (foundItem && !CanAccessItem(foundItem, aOriginalRequestor)) {
      foundItem = nullptr;
    }

    // DoFindItemWithName only returns active items and we don't check if
    // the item is active for the special cases.
    if (foundItem) {
      foundItem.swap(*aResult);
    }
    return NS_OK;
  }
}

nsresult
nsDocShell::DoFindItemWithName(const char16_t* aName,
                               nsISupports* aRequestor,
                               nsIDocShellTreeItem* aOriginalRequestor,
                               nsIDocShellTreeItem** aResult)
{
  // First we check our name.
  if (mName.Equals(aName) && ItemIsActive(this) &&
      CanAccessItem(this, aOriginalRequestor)) {
    NS_ADDREF(*aResult = this);
    return NS_OK;
  }

  // This QI may fail, but the places where we want to compare, comparing
  // against nullptr serves the same purpose.
  nsCOMPtr<nsIDocShellTreeItem> reqAsTreeItem(do_QueryInterface(aRequestor));

  // Second we check our children making sure not to ask a child if
  // it is the aRequestor.
#ifdef DEBUG
  nsresult rv =
#endif
  FindChildWithName(aName, true, true, reqAsTreeItem, aOriginalRequestor,
                    aResult);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "FindChildWithName should not be failing here.");
  if (*aResult) {
    return NS_OK;
  }

  // Third if we have a parent and it isn't the requestor then we
  // should ask it to do the search.  If it is the requestor we
  // should just stop here and let the parent do the rest.  If we
  // don't have a parent, then we should ask the
  // docShellTreeOwner to do the search.
  nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem =
    do_QueryInterface(GetAsSupports(mParent));
  if (parentAsTreeItem) {
    if (parentAsTreeItem == reqAsTreeItem) {
      return NS_OK;
    }

    if (parentAsTreeItem->ItemType() == mItemType) {
      return parentAsTreeItem->FindItemWithName(
        aName,
        static_cast<nsIDocShellTreeItem*>(this),
        aOriginalRequestor,
        aResult);
    }
  }

  // If the parent is null or not of the same type fall through and ask tree
  // owner.

  // This may fail, but comparing against null serves the same purpose
  nsCOMPtr<nsIDocShellTreeOwner> reqAsTreeOwner(do_QueryInterface(aRequestor));
  if (mTreeOwner && mTreeOwner != reqAsTreeOwner) {
    return mTreeOwner->FindItemWithName(aName, this, aOriginalRequestor,
                                        aResult);
  }

  return NS_OK;
}

bool
nsDocShell::IsSandboxedFrom(nsIDocShell* aTargetDocShell)
{
  // If no target then not sandboxed.
  if (!aTargetDocShell) {
    return false;
  }

  // We cannot be sandboxed from ourselves.
  if (aTargetDocShell == this) {
    return false;
  }

  // Default the sandbox flags to our flags, so that if we can't retrieve the
  // active document, we will still enforce our own.
  uint32_t sandboxFlags = mSandboxFlags;
  if (mContentViewer) {
    nsCOMPtr<nsIDocument> doc = mContentViewer->GetDocument();
    if (doc) {
      sandboxFlags = doc->GetSandboxFlags();
    }
  }

  // If no flags, we are not sandboxed at all.
  if (!sandboxFlags) {
    return false;
  }

  // If aTargetDocShell has an ancestor, it is not top level.
  nsCOMPtr<nsIDocShellTreeItem> ancestorOfTarget;
  aTargetDocShell->GetSameTypeParent(getter_AddRefs(ancestorOfTarget));
  if (ancestorOfTarget) {
    do {
      // We are not sandboxed if we are an ancestor of target.
      if (ancestorOfTarget == this) {
        return false;
      }
      nsCOMPtr<nsIDocShellTreeItem> tempTreeItem;
      ancestorOfTarget->GetSameTypeParent(getter_AddRefs(tempTreeItem));
      tempTreeItem.swap(ancestorOfTarget);
    } while (ancestorOfTarget);

    // Otherwise, we are sandboxed from aTargetDocShell.
    return true;
  }

  // aTargetDocShell is top level, are we the "one permitted sandboxed
  // navigator", i.e. did we open aTargetDocShell?
  nsCOMPtr<nsIDocShell> permittedNavigator;
  aTargetDocShell->GetOnePermittedSandboxedNavigator(
    getter_AddRefs(permittedNavigator));
  if (permittedNavigator == this) {
    return false;
  }

  // If SANDBOXED_TOPLEVEL_NAVIGATION flag is not on, we are not sandboxed
  // from our top.
  if (!(sandboxFlags & SANDBOXED_TOPLEVEL_NAVIGATION)) {
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
    GetSameTypeRootTreeItem(getter_AddRefs(rootTreeItem));
    if (SameCOMIdentity(aTargetDocShell, rootTreeItem)) {
      return false;
    }
  }

  // Otherwise, we are sandboxed from aTargetDocShell.
  return true;
}

NS_IMETHODIMP
nsDocShell::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner)
{
  NS_ENSURE_ARG_POINTER(aTreeOwner);

  *aTreeOwner = mTreeOwner;
  NS_IF_ADDREF(*aTreeOwner);
  return NS_OK;
}

#ifdef DEBUG_DOCSHELL_FOCUS
static void
PrintDocTree(nsIDocShellTreeItem* aParentNode, int aLevel)
{
  for (int32_t i = 0; i < aLevel; i++) {
    printf("  ");
  }

  int32_t childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentNode));
  int32_t type = aParentNode->ItemType();
  nsCOMPtr<nsIPresShell> presShell = parentAsDocShell->GetPresShell();
  RefPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsIDocument* doc = presShell->GetDocument();

  nsCOMPtr<nsIDOMWindow> domwin(doc->GetWindow());

  nsCOMPtr<nsIWidget> widget;
  nsViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  dom::Element* rootElement = doc->GetRootElement();

  printf("DS %p  Ty %s  Doc %p DW %p EM %p CN %p\n",
         (void*)parentAsDocShell.get(),
         type == nsIDocShellTreeItem::typeChrome ? "Chr" : "Con",
         (void*)doc, (void*)domwin.get(),
         (void*)presContext->EventStateManager(), (void*)rootElement);

  if (childWebshellCount > 0) {
    for (int32_t i = 0; i < childWebshellCount; i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      PrintDocTree(child, aLevel + 1);
    }
  }
}

static void
PrintDocTree(nsIDocShellTreeItem* aParentNode)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  aParentNode->GetParent(getter_AddRefs(parentItem));
  while (parentItem) {
    nsCOMPtr<nsIDocShellTreeItem> tmp;
    parentItem->GetParent(getter_AddRefs(tmp));
    if (!tmp) {
      break;
    }
    parentItem = tmp;
  }

  if (!parentItem) {
    parentItem = aParentNode;
  }

  PrintDocTree(parentItem, 0);
}
#endif

NS_IMETHODIMP
nsDocShell::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner)
{
#ifdef DEBUG_DOCSHELL_FOCUS
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(aTreeOwner));
  if (item) {
    PrintDocTree(item);
  }
#endif

  // Don't automatically set the progress based on the tree owner for frames
  if (!IsFrame()) {
    nsCOMPtr<nsIWebProgress> webProgress =
      do_QueryInterface(GetAsSupports(this));

    if (webProgress) {
      nsCOMPtr<nsIWebProgressListener> oldListener =
        do_QueryInterface(mTreeOwner);
      nsCOMPtr<nsIWebProgressListener> newListener =
        do_QueryInterface(aTreeOwner);

      if (oldListener) {
        webProgress->RemoveProgressListener(oldListener);
      }

      if (newListener) {
        webProgress->AddProgressListener(newListener,
                                         nsIWebProgress::NOTIFY_ALL);
      }
    }
  }

  mTreeOwner = aTreeOwner;  // Weak reference per API

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShellTreeItem> child = do_QueryObject(iter.GetNext());
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    if (child->ItemType() == mItemType) {
      child->SetTreeOwner(aTreeOwner);
    }
  }

  // Our tree owner has changed. Recompute scriptability.
  //
  // Note that this is near-redundant with the recomputation in
  // SetDocLoaderParent(), but not so for the root DocShell, where the call to
  // SetTreeOwner() happens after the initial AddDocLoaderAsChildOfRoot(),
  // and we never set another parent. Given that this is neither expensive nor
  // performance-critical, let's be safe and unconditionally recompute this
  // state whenever dependent state changes.
  RecomputeCanExecuteScripts();

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChildOffset(uint32_t aChildOffset)
{
  mChildOffset = aChildOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHistoryID(uint64_t* aID)
{
  *aID = mHistoryID;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsInUnload(bool* aIsInUnload)
{
  *aIsInUnload = mFiredUnloadEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildCount(int32_t* aChildCount)
{
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = mChildList.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddChild(nsIDocShellTreeItem* aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);

  RefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
  NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);

  // Make sure we're not creating a loop in the docshell tree
  nsDocLoader* ancestor = this;
  do {
    if (childAsDocLoader == ancestor) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    ancestor = ancestor->GetParent();
  } while (ancestor);

  // Make sure to remove the child from its current parent.
  nsDocLoader* childsParent = childAsDocLoader->GetParent();
  if (childsParent) {
    childsParent->RemoveChildLoader(childAsDocLoader);
  }

  // Make sure to clear the treeowner in case this child is a different type
  // from us.
  aChild->SetTreeOwner(nullptr);

  nsresult res = AddChildLoader(childAsDocLoader);
  NS_ENSURE_SUCCESS(res, res);
  NS_ASSERTION(!mChildList.IsEmpty(),
               "child list must not be empty after a successful add");

  nsCOMPtr<nsIDocShell> childDocShell = do_QueryInterface(aChild);
  bool dynamic = false;
  childDocShell->GetCreatedDynamically(&dynamic);
  if (!dynamic) {
    nsCOMPtr<nsISHEntry> currentSH;
    bool oshe = false;
    GetCurrentSHEntry(getter_AddRefs(currentSH), &oshe);
    if (currentSH) {
      currentSH->HasDynamicallyAddedChild(&dynamic);
    }
  }
  childDocShell->SetChildOffset(dynamic ? -1 : mChildList.Length() - 1);

  /* Set the child's global history if the parent has one */
  if (mUseGlobalHistory) {
    childDocShell->SetUseGlobalHistory(true);
  }

  if (aChild->ItemType() != mItemType) {
    return NS_OK;
  }

  aChild->SetTreeOwner(mTreeOwner);

  nsCOMPtr<nsIDocShell> childAsDocShell(do_QueryInterface(aChild));
  if (!childAsDocShell) {
    return NS_OK;
  }

  // charset, style-disabling, and zoom will be inherited in SetupNewViewer()

  // Now take this document's charset and set the child's parentCharset field
  // to it. We'll later use that field, in the loading process, for the
  // charset choosing algorithm.
  // If we fail, at any point, we just return NS_OK.
  // This code has some performance impact. But this will be reduced when
  // the current charset will finally be stored as an Atom, avoiding the
  // alias resolution extra look-up.

  // we are NOT going to propagate the charset is this Chrome's docshell
  if (mItemType == nsIDocShellTreeItem::typeChrome) {
    return NS_OK;
  }

  // get the parent's current charset
  if (!mContentViewer) {
    return NS_OK;
  }
  nsIDocument* doc = mContentViewer->GetDocument();
  if (!doc) {
    return NS_OK;
  }

  bool isWyciwyg = false;

  if (mCurrentURI) {
    // Check if the url is wyciwyg
    mCurrentURI->SchemeIs("wyciwyg", &isWyciwyg);
  }

  if (!isWyciwyg) {
    // If this docshell is loaded from a wyciwyg: URI, don't
    // advertise our charset since it does not in any way reflect
    // the actual source charset, which is what we're trying to
    // expose here.

    const nsACString& parentCS = doc->GetDocumentCharacterSet();
    int32_t charsetSource = doc->GetDocumentCharacterSetSource();
    // set the child's parentCharset
    childAsDocShell->SetParentCharset(parentCS,
                                      charsetSource,
                                      doc->NodePrincipal());
  }

  // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n",
  //        NS_LossyConvertUTF16toASCII(parentCS).get(), mItemType);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveChild(nsIDocShellTreeItem* aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);

  RefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
  NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);

  nsresult rv = RemoveChildLoader(childAsDocLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  aChild->SetTreeOwner(nullptr);

  return nsDocLoader::AddDocLoaderAsChildOfRoot(childAsDocLoader);
}

NS_IMETHODIMP
nsDocShell::GetChildAt(int32_t aIndex, nsIDocShellTreeItem** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);

#ifdef DEBUG
  if (aIndex < 0) {
    NS_WARNING("Negative index passed to GetChildAt");
  } else if (static_cast<uint32_t>(aIndex) >= mChildList.Length()) {
    NS_WARNING("Too large an index passed to GetChildAt");
  }
#endif

  nsIDocumentLoader* child = ChildAt(aIndex);
  NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

  return CallQueryInterface(child, aChild);
}

NS_IMETHODIMP
nsDocShell::FindChildWithName(const char16_t* aName,
                              bool aRecurse, bool aSameType,
                              nsIDocShellTreeItem* aRequestor,
                              nsIDocShellTreeItem* aOriginalRequestor,
                              nsIDocShellTreeItem** aResult)
{
  NS_ENSURE_ARG(aName);
  NS_ENSURE_ARG_POINTER(aResult);

  // if we don't find one, we return NS_OK and a null result
  *aResult = nullptr;

  if (!*aName) {
    return NS_OK;
  }

  nsXPIDLString childName;
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShellTreeItem> child = do_QueryObject(iter.GetNext());
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
    int32_t childType = child->ItemType();

    if (aSameType && (childType != mItemType)) {
      continue;
    }

    bool childNameEquals = false;
    child->NameEquals(aName, &childNameEquals);
    if (childNameEquals && ItemIsActive(child) &&
        CanAccessItem(child, aOriginalRequestor)) {
      child.swap(*aResult);
      break;
    }

    // Only ask it to check children if it is same type
    if (childType != mItemType) {
      continue;
    }

    // Only ask the child if it isn't the requestor
    if (aRecurse && (aRequestor != child)) {
      // See if child contains the shell with the given name
#ifdef DEBUG
      nsresult rv =
#endif
      child->FindChildWithName(aName, true, aSameType,
                               static_cast<nsIDocShellTreeItem*>(this),
                               aOriginalRequestor, aResult);
      NS_ASSERTION(NS_SUCCEEDED(rv), "FindChildWithName should not fail here");
      if (*aResult) {
        // found it
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildSHEntry(int32_t aChildOffset, nsISHEntry** aResult)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  // A nsISHEntry for a child is *only* available when the parent is in
  // the progress of loading a document too...

  if (mLSHE) {
    /* Before looking for the subframe's url, check
     * the expiration status of the parent. If the parent
     * has expired from cache, then subframes will not be
     * loaded from history in certain situations.
     */
    bool parentExpired = false;
    mLSHE->GetExpirationStatus(&parentExpired);

    /* Get the parent's Load Type so that it can be set on the child too.
     * By default give a loadHistory value
     */
    uint32_t loadType = nsIDocShellLoadInfo::loadHistory;
    mLSHE->GetLoadType(&loadType);
    // If the user did a shift-reload on this frameset page,
    // we don't want to load the subframes from history.
    if (loadType == nsIDocShellLoadInfo::loadReloadBypassCache ||
        loadType == nsIDocShellLoadInfo::loadReloadBypassProxy ||
        loadType == nsIDocShellLoadInfo::loadReloadBypassProxyAndCache ||
        loadType == nsIDocShellLoadInfo::loadRefresh) {
      return rv;
    }

    /* If the user pressed reload and the parent frame has expired
     *  from cache, we do not want to load the child frame from history.
     */
    if (parentExpired && (loadType == nsIDocShellLoadInfo::loadReloadNormal)) {
      // The parent has expired. Return null.
      *aResult = nullptr;
      return rv;
    }

    nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE));
    if (container) {
      // Get the child subframe from session history.
      rv = container->GetChildAt(aChildOffset, aResult);
      if (*aResult) {
        (*aResult)->SetLoadType(loadType);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::AddChildSHEntry(nsISHEntry* aCloneRef, nsISHEntry* aNewEntry,
                            int32_t aChildOffset, uint32_t aLoadType,
                            bool aCloneChildren)
{
  nsresult rv = NS_OK;

  if (mLSHE && aLoadType != LOAD_PUSHSTATE) {
    /* You get here if you are currently building a
     * hierarchy ie.,you just visited a frameset page
     */
    nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE, &rv));
    if (container) {
      if (NS_FAILED(container->ReplaceChild(aNewEntry))) {
        rv = container->AddChild(aNewEntry, aChildOffset);
      }
    }
  } else if (!aCloneRef) {
    /* This is an initial load in some subframe.  Just append it if we can */
    nsCOMPtr<nsISHContainer> container(do_QueryInterface(mOSHE, &rv));
    if (container) {
      rv = container->AddChild(aNewEntry, aChildOffset);
    }
  } else {
    rv = AddChildSHEntryInternal(aCloneRef, aNewEntry, aChildOffset,
                                 aLoadType, aCloneChildren);
  }
  return rv;
}

nsresult
nsDocShell::AddChildSHEntryInternal(nsISHEntry* aCloneRef,
                                    nsISHEntry* aNewEntry,
                                    int32_t aChildOffset,
                                    uint32_t aLoadType,
                                    bool aCloneChildren)
{
  nsresult rv = NS_OK;
  if (mSessionHistory) {
    /* You are currently in the rootDocShell.
     * You will get here when a subframe has a new url
     * to load and you have walked up the tree all the
     * way to the top to clone the current SHEntry hierarchy
     * and replace the subframe where a new url was loaded with
     * a new entry.
     */
    int32_t index = -1;
    nsCOMPtr<nsISHEntry> currentHE;
    mSessionHistory->GetIndex(&index);
    if (index < 0) {
      return NS_ERROR_FAILURE;
    }

    rv = mSessionHistory->GetEntryAtIndex(index, false,
                                          getter_AddRefs(currentHE));
    NS_ENSURE_TRUE(currentHE, NS_ERROR_FAILURE);

    nsCOMPtr<nsISHEntry> currentEntry(do_QueryInterface(currentHE));
    if (currentEntry) {
      uint32_t cloneID = 0;
      nsCOMPtr<nsISHEntry> nextEntry;
      aCloneRef->GetID(&cloneID);
      rv = CloneAndReplace(currentEntry, this, cloneID, aNewEntry,
                           aCloneChildren, getter_AddRefs(nextEntry));

      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsISHistoryInternal> shPrivate =
          do_QueryInterface(mSessionHistory);
        NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
        rv = shPrivate->AddEntry(nextEntry, true);
      }
    }
  } else {
    /* Just pass this along */
    nsCOMPtr<nsIDocShell> parent =
      do_QueryInterface(GetAsSupports(mParent), &rv);
    if (parent) {
      rv = static_cast<nsDocShell*>(parent.get())->AddChildSHEntryInternal(
        aCloneRef, aNewEntry, aChildOffset, aLoadType, aCloneChildren);
    }
  }
  return rv;
}

nsresult
nsDocShell::AddChildSHEntryToParent(nsISHEntry* aNewEntry, int32_t aChildOffset,
                                    bool aCloneChildren)
{
  /* You will get here when you are in a subframe and
   * a new url has been loaded on you.
   * The mOSHE in this subframe will be the previous url's
   * mOSHE. This mOSHE will be used as the identification
   * for this subframe in the  CloneAndReplace function.
   */

  // In this case, we will end up calling AddEntry, which increases the
  // current index by 1
  nsCOMPtr<nsISHistory> rootSH;
  GetRootSessionHistory(getter_AddRefs(rootSH));
  if (rootSH) {
    rootSH->GetIndex(&mPreviousTransIndex);
  }

  nsresult rv;
  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(GetAsSupports(mParent), &rv);
  if (parent) {
    rv = parent->AddChildSHEntry(mOSHE, aNewEntry, aChildOffset, mLoadType,
                                 aCloneChildren);
  }

  if (rootSH) {
    rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
    printf("Previous index: %d, Loaded index: %d\n\n", mPreviousTransIndex,
           mLoadedTransIndex);
#endif
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::SetUseGlobalHistory(bool aUseGlobalHistory)
{
  nsresult rv;

  mUseGlobalHistory = aUseGlobalHistory;

  if (!aUseGlobalHistory) {
    mGlobalHistory = nullptr;
    return NS_OK;
  }

  // No need to initialize mGlobalHistory if IHistory is available.
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
    return NS_OK;
  }

  if (mGlobalHistory) {
    return NS_OK;
  }

  mGlobalHistory = do_GetService(NS_GLOBALHISTORY2_CONTRACTID, &rv);
  return rv;
}

NS_IMETHODIMP
nsDocShell::GetUseGlobalHistory(bool* aUseGlobalHistory)
{
  *aUseGlobalHistory = mUseGlobalHistory;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveFromSessionHistory()
{
  nsCOMPtr<nsISHistoryInternal> internalHistory;
  nsCOMPtr<nsISHistory> sessionHistory;
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetSameTypeRootTreeItem(getter_AddRefs(root));
  if (root) {
    nsCOMPtr<nsIWebNavigation> rootAsWebnav = do_QueryInterface(root);
    if (rootAsWebnav) {
      rootAsWebnav->GetSessionHistory(getter_AddRefs(sessionHistory));
      internalHistory = do_QueryInterface(sessionHistory);
    }
  }
  if (!internalHistory) {
    return NS_OK;
  }

  int32_t index = 0;
  sessionHistory->GetIndex(&index);
  nsAutoTArray<uint64_t, 16> ids;
  ids.AppendElement(mHistoryID);
  internalHistory->RemoveEntries(ids, index);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCreatedDynamically(bool aDynamic)
{
  mDynamicallyCreated = aDynamic;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCreatedDynamically(bool* aDynamic)
{
  *aDynamic = mDynamicallyCreated;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentSHEntry(nsISHEntry** aEntry, bool* aOSHE)
{
  *aOSHE = false;
  *aEntry = nullptr;
  if (mLSHE) {
    NS_ADDREF(*aEntry = mLSHE);
  } else if (mOSHE) {
    NS_ADDREF(*aEntry = mOSHE);
    *aOSHE = true;
  }
  return NS_OK;
}

nsIScriptGlobalObject*
nsDocShell::GetScriptGlobalObject()
{
  NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), nullptr);
  return mScriptGlobal;
}

nsIDocument*
nsDocShell::GetDocument()
{
  NS_ENSURE_SUCCESS(EnsureContentViewer(), nullptr);
  return mContentViewer->GetDocument();
}

nsPIDOMWindow*
nsDocShell::GetWindow()
{
  if (NS_FAILED(EnsureScriptEnvironment())) {
    return nullptr;
  }
  return mScriptGlobal;
}

NS_IMETHODIMP
nsDocShell::SetDeviceSizeIsPageSize(bool aValue)
{
  if (mDeviceSizeIsPageSize != aValue) {
    mDeviceSizeIsPageSize = aValue;
    RefPtr<nsPresContext> presContext;
    GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      presContext->MediaFeatureValuesChanged(nsRestyleHint(0));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDeviceSizeIsPageSize(bool* aValue)
{
  *aValue = mDeviceSizeIsPageSize;
  return NS_OK;
}

void
nsDocShell::ClearFrameHistory(nsISHEntry* aEntry)
{
  nsCOMPtr<nsISHContainer> shcontainer = do_QueryInterface(aEntry);
  nsCOMPtr<nsISHistory> rootSH;
  GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsISHistoryInternal> history = do_QueryInterface(rootSH);
  if (!history || !shcontainer) {
    return;
  }

  int32_t count = 0;
  shcontainer->GetChildCount(&count);
  nsAutoTArray<uint64_t, 16> ids;
  for (int32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    shcontainer->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      uint64_t id = 0;
      child->GetDocshellID(&id);
      ids.AppendElement(id);
    }
  }
  int32_t index = 0;
  rootSH->GetIndex(&index);
  history->RemoveEntries(ids, index);
}

//-------------------------------------
//-- Helper Method for Print discovery
//-------------------------------------
bool
nsDocShell::IsPrintingOrPP(bool aDisplayErrorDialog)
{
  if (mIsPrintingOrPP && aDisplayErrorDialog) {
    DisplayLoadError(NS_ERROR_DOCUMENT_IS_PRINTMODE, nullptr, nullptr, nullptr);
  }

  return mIsPrintingOrPP;
}

bool
nsDocShell::IsNavigationAllowed(bool aDisplayPrintErrorDialog,
                                bool aCheckIfUnloadFired)
{
  bool isAllowed = !IsPrintingOrPP(aDisplayPrintErrorDialog) &&
                   (!aCheckIfUnloadFired || !mFiredUnloadEvent);
  if (!isAllowed) {
    return false;
  }
  if (!mContentViewer) {
    return true;
  }
  bool firingBeforeUnload;
  mContentViewer->GetBeforeUnloadFiring(&firingBeforeUnload);
  return !firingBeforeUnload;
}

//*****************************************************************************
// nsDocShell::nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetCanGoBack(bool* aCanGoBack)
{
  if (!IsNavigationAllowed(false)) {
    *aCanGoBack = false;
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
  rv = webnav->GetCanGoBack(aCanGoBack);
  return rv;
}

NS_IMETHODIMP
nsDocShell::GetCanGoForward(bool* aCanGoForward)
{
  if (!IsNavigationAllowed(false)) {
    *aCanGoForward = false;
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
  rv = webnav->GetCanGoForward(aCanGoForward);
  return rv;
}

NS_IMETHODIMP
nsDocShell::GoBack()
{
  if (!IsNavigationAllowed()) {
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
  rv = webnav->GoBack();
  return rv;
}

NS_IMETHODIMP
nsDocShell::GoForward()
{
  if (!IsNavigationAllowed()) {
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
  rv = webnav->GoForward();
  return rv;
}

NS_IMETHODIMP
nsDocShell::GotoIndex(int32_t aIndex)
{
  if (!IsNavigationAllowed()) {
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(rootSH));
  NS_ENSURE_TRUE(webnav, NS_ERROR_FAILURE);
  rv = webnav->GotoIndex(aIndex);
  return rv;
}

NS_IMETHODIMP
nsDocShell::LoadURI(const char16_t* aURI,
                    uint32_t aLoadFlags,
                    nsIURI* aReferringURI,
                    nsIInputStream* aPostStream,
                    nsIInputStream* aHeaderStream)
{
  return LoadURIWithOptions(aURI, aLoadFlags, aReferringURI,
                            mozilla::net::RP_Default, aPostStream,
                            aHeaderStream, nullptr);
}

NS_IMETHODIMP
nsDocShell::LoadURIWithOptions(const char16_t* aURI,
                               uint32_t aLoadFlags,
                               nsIURI* aReferringURI,
                               uint32_t aReferrerPolicy,
                               nsIInputStream* aPostStream,
                               nsIInputStream* aHeaderStream,
                               nsIURI* aBaseURI)
{
  NS_ASSERTION((aLoadFlags & 0xf) == 0, "Unexpected flags");

  if (!IsNavigationAllowed()) {
    return NS_OK; // JS may not handle returning of an error code
  }
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIInputStream> postStream(aPostStream);
  nsresult rv = NS_OK;

  // Create a URI from our string; if that succeeds, we want to
  // change aLoadFlags to not include the ALLOW_THIRD_PARTY_FIXUP
  // flag.

  NS_ConvertUTF16toUTF8 uriString(aURI);
  // Cleanup the empty spaces that might be on each end.
  uriString.Trim(" ");
  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripChars("\r\n");
  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

  rv = NS_NewURI(getter_AddRefs(uri), uriString);
  if (uri) {
    aLoadFlags &= ~LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  if (sURIFixup) {
    // Call the fixup object.  This will clobber the rv from NS_NewURI
    // above, but that's fine with us.  Note that we need to do this even
    // if NS_NewURI returned a URI, because fixup handles nested URIs, etc
    // (things like view-source:mozilla.org for example).
    uint32_t fixupFlags = 0;
    if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
      fixupFlags |= nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    }
    if (aLoadFlags & LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
      fixupFlags |= nsIURIFixup::FIXUP_FLAG_FIX_SCHEME_TYPOS;
    }
    nsCOMPtr<nsIInputStream> fixupStream;
    rv = sURIFixup->GetFixupURIInfo(uriString, fixupFlags,
                                    getter_AddRefs(fixupStream),
                                    getter_AddRefs(fixupInfo));

    if (NS_SUCCEEDED(rv)) {
      fixupInfo->GetPreferredURI(getter_AddRefs(uri));
      fixupInfo->SetConsumer(GetAsSupports(this));
    }

    if (fixupStream) {
      // GetFixupURIInfo only returns a post data stream if it succeeded
      // and changed the URI, in which case we should override the
      // passed-in post data.
      postStream = fixupStream;
    }

    if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
      nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
      if (serv) {
        serv->NotifyObservers(fixupInfo, "keyword-uri-fixup", aURI);
      }
    }
  }
  // else no fixup service so just use the URI we created and see
  // what happens

  if (NS_ERROR_MALFORMED_URI == rv) {
    DisplayLoadError(rv, uri, aURI, nullptr);
  }

  if (NS_FAILED(rv) || !uri) {
    return NS_ERROR_FAILURE;
  }

  PopupControlState popupState;
  if (aLoadFlags & LOAD_FLAGS_ALLOW_POPUPS) {
    popupState = openAllowed;
    aLoadFlags &= ~LOAD_FLAGS_ALLOW_POPUPS;
  } else {
    popupState = openOverridden;
  }
  nsAutoPopupStatePusher statePusher(popupState);

  // Don't pass certain flags that aren't needed and end up confusing
  // ConvertLoadTypeToDocShellLoadInfo.  We do need to ensure that they are
  // passed to LoadURI though, since it uses them.
  uint32_t extraFlags = (aLoadFlags & EXTRA_LOAD_FLAGS);
  aLoadFlags &= ~EXTRA_LOAD_FLAGS;

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  rv = CreateLoadInfo(getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  /*
   * If the user "Disables Protection on This Page", we have to make sure to
   * remember the users decision when opening links in child tabs [Bug 906190]
   */
  uint32_t loadType;
  if (aLoadFlags & LOAD_FLAGS_ALLOW_MIXED_CONTENT) {
    loadType = MAKE_LOAD_TYPE(LOAD_NORMAL_ALLOW_MIXED_CONTENT, aLoadFlags);
  } else {
    loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);
  }

  loadInfo->SetLoadType(ConvertLoadTypeToDocShellLoadInfo(loadType));
  loadInfo->SetPostDataStream(postStream);
  loadInfo->SetReferrer(aReferringURI);
  loadInfo->SetReferrerPolicy(aReferrerPolicy);
  loadInfo->SetHeadersStream(aHeaderStream);
  loadInfo->SetBaseURI(aBaseURI);

  if (fixupInfo) {
    nsAutoString searchProvider, keyword;
    fixupInfo->GetKeywordProviderName(searchProvider);
    fixupInfo->GetKeywordAsSent(keyword);
    MaybeNotifyKeywordSearchLoading(searchProvider, keyword);
  }

  rv = LoadURI(uri, loadInfo, extraFlags, true);

  // Save URI string in case it's needed later when
  // sending to search engine service in EndPageLoad()
  mOriginalUriString = uriString;

  return rv;
}

NS_IMETHODIMP
nsDocShell::DisplayLoadError(nsresult aError, nsIURI* aURI,
                             const char16_t* aURL,
                             nsIChannel* aFailedChannel)
{
  // Get prompt and string bundle servcies
  nsCOMPtr<nsIPrompt> prompter;
  nsCOMPtr<nsIStringBundle> stringBundle;
  GetPromptAndStringBundle(getter_AddRefs(prompter),
                           getter_AddRefs(stringBundle));

  NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  nsAutoString error;
  const uint32_t kMaxFormatStrArgs = 3;
  nsAutoString formatStrs[kMaxFormatStrArgs];
  uint32_t formatStrCount = 0;
  bool addHostPort = false;
  nsresult rv = NS_OK;
  nsAutoString messageStr;
  nsAutoCString cssClass;
  nsAutoCString errorPage;

  errorPage.AssignLiteral("neterror");

  // Turn the error code into a human readable error message.
  if (NS_ERROR_UNKNOWN_PROTOCOL == aError) {
    NS_ENSURE_ARG_POINTER(aURI);

    // Extract the schemes into a comma delimited list.
    nsAutoCString scheme;
    aURI->GetScheme(scheme);
    CopyASCIItoUTF16(scheme, formatStrs[0]);
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(aURI);
    while (nestedURI) {
      nsCOMPtr<nsIURI> tempURI;
      nsresult rv2;
      rv2 = nestedURI->GetInnerURI(getter_AddRefs(tempURI));
      if (NS_SUCCEEDED(rv2) && tempURI) {
        tempURI->GetScheme(scheme);
        formatStrs[0].AppendLiteral(", ");
        AppendASCIItoUTF16(scheme, formatStrs[0]);
      }
      nestedURI = do_QueryInterface(tempURI);
    }
    formatStrCount = 1;
    error.AssignLiteral("unknownProtocolFound");
  } else if (NS_ERROR_FILE_NOT_FOUND == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    error.AssignLiteral("fileNotFound");
  } else if (NS_ERROR_UNKNOWN_HOST == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    // Get the host
    nsAutoCString host;
    nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(aURI);
    innermostURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;
    error.AssignLiteral("dnsNotFound");
  } else if (NS_ERROR_CONNECTION_REFUSED == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error.AssignLiteral("connectionFailure");
  } else if (NS_ERROR_NET_INTERRUPT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error.AssignLiteral("netInterrupt");
  } else if (NS_ERROR_NET_TIMEOUT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    // Get the host
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;
    error.AssignLiteral("netTimeout");
  } else if (NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION == aError ||
             NS_ERROR_CSP_FORM_ACTION_VIOLATION == aError) {
    // CSP error
    cssClass.AssignLiteral("neterror");
    error.AssignLiteral("cspBlocked");
  } else if (NS_ERROR_GET_MODULE(aError) == NS_ERROR_MODULE_SECURITY) {
    nsCOMPtr<nsINSSErrorsService> nsserr =
      do_GetService(NS_NSS_ERRORS_SERVICE_CONTRACTID);

    uint32_t errorClass;
    if (!nsserr || NS_FAILED(nsserr->GetErrorClass(aError, &errorClass))) {
      errorClass = nsINSSErrorsService::ERROR_CLASS_SSL_PROTOCOL;
    }

    nsCOMPtr<nsISupports> securityInfo;
    nsCOMPtr<nsITransportSecurityInfo> tsi;
    if (aFailedChannel) {
      aFailedChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
    }
    tsi = do_QueryInterface(securityInfo);
    if (tsi) {
      uint32_t securityState;
      tsi->GetSecurityState(&securityState);
      if (securityState & nsIWebProgressListener::STATE_USES_SSL_3) {
        error.AssignLiteral("sslv3Used");
        addHostPort = true;
      } else if (securityState & nsIWebProgressListener::STATE_USES_WEAK_CRYPTO) {
        error.AssignLiteral("weakCryptoUsed");
        addHostPort = true;
      } else {
        // Usually we should have aFailedChannel and get a detailed message
        tsi->GetErrorMessage(getter_Copies(messageStr));
      }
    } else {
      // No channel, let's obtain the generic error message
      if (nsserr) {
        nsserr->GetErrorMessage(aError, messageStr);
      }
    }
    if (!messageStr.IsEmpty()) {
      if (errorClass == nsINSSErrorsService::ERROR_CLASS_BAD_CERT) {
        error.AssignLiteral("nssBadCert");

        // If this is an HTTP Strict Transport Security host or a pinned host
        // and the certificate is bad, don't allow overrides (RFC 6797 section
        // 12.1, HPKP draft spec section 2.6).
        uint32_t flags =
          mInPrivateBrowsing ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
        bool isStsHost = false;
        bool isPinnedHost = false;
        if (XRE_IsParentProcess()) {
          nsCOMPtr<nsISiteSecurityService> sss =
            do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI,
                                flags, &isStsHost);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HPKP, aURI,
                                flags, &isPinnedHost);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          mozilla::dom::ContentChild* cc =
            mozilla::dom::ContentChild::GetSingleton();
          mozilla::ipc::URIParams uri;
          SerializeURI(aURI, uri);
          cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, flags,
                              &isStsHost);
          cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HPKP, uri, flags,
                              &isPinnedHost);
        }

        if (Preferences::GetBool(
              "browser.xul.error_pages.expert_bad_cert", false)) {
          cssClass.AssignLiteral("expertBadCert");
        }

        // HSTS/pinning takes precedence over the expert bad cert pref. We
        // never want to show the "Add Exception" button for these sites.
        // In the future we should differentiate between an HSTS host and a
        // pinned host and display a more informative message to the user.
        if (isStsHost || isPinnedHost) {
          cssClass.AssignLiteral("badStsCert");
        }

        uint32_t bucketId;
        if (isStsHost) {
          // measuring STS separately allows us to measure click through
          // rates easily
          bucketId = nsISecurityUITelemetry::WARNING_BAD_CERT_TOP_STS;
        } else {
          bucketId = nsISecurityUITelemetry::WARNING_BAD_CERT_TOP;
        }

        // See if an alternate cert error page is registered
        nsAdoptingCString alternateErrorPage =
          Preferences::GetCString("security.alternate_certificate_error_page");
        if (alternateErrorPage) {
          errorPage.Assign(alternateErrorPage);
        }

        if (!IsFrame() && errorPage.EqualsIgnoreCase("certerror")) {
          Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI, bucketId);
        }

      } else {
        error.AssignLiteral("nssFailure2");
      }
    }
  } else if (NS_ERROR_PHISHING_URI == aError ||
             NS_ERROR_MALWARE_URI == aError ||
             NS_ERROR_UNWANTED_URI == aError) {
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;

    // Malware and phishing detectors may want to use an alternate error
    // page, but if the pref's not set, we'll fall back on the standard page
    nsAdoptingCString alternateErrorPage =
      Preferences::GetCString("urlclassifier.alternate_error_page");
    if (alternateErrorPage) {
      errorPage.Assign(alternateErrorPage);
    }

    uint32_t bucketId;
    if (NS_ERROR_PHISHING_URI == aError) {
      error.AssignLiteral("phishingBlocked");
      bucketId = IsFrame() ? nsISecurityUITelemetry::WARNING_PHISHING_PAGE_FRAME
                           : nsISecurityUITelemetry::WARNING_PHISHING_PAGE_TOP;
    } else if (NS_ERROR_MALWARE_URI == aError) {
      error.AssignLiteral("malwareBlocked");
      bucketId = IsFrame() ? nsISecurityUITelemetry::WARNING_MALWARE_PAGE_FRAME
                           : nsISecurityUITelemetry::WARNING_MALWARE_PAGE_TOP;
    } else {
      error.AssignLiteral("unwantedBlocked");
      bucketId = IsFrame() ? nsISecurityUITelemetry::WARNING_UNWANTED_PAGE_FRAME
                           : nsISecurityUITelemetry::WARNING_UNWANTED_PAGE_TOP;
    }

    if (errorPage.EqualsIgnoreCase("blocked")) {
      Telemetry::Accumulate(Telemetry::SECURITY_UI, bucketId);
    }

    cssClass.AssignLiteral("blacklist");
  } else if (NS_ERROR_CONTENT_CRASHED == aError) {
    errorPage.AssignLiteral("tabcrashed");
    error.AssignLiteral("tabcrashed");

    nsCOMPtr<EventTarget> handler = mChromeEventHandler;
    if (handler) {
      nsCOMPtr<Element> element = do_QueryInterface(handler);
      element->GetAttribute(NS_LITERAL_STRING("crashedPageTitle"), messageStr);
    }

    // DisplayLoadError requires a non-empty messageStr to proceed and call
    // LoadErrorPage. If the page doesn't have a title, we will use a blank
    // space which will be trimmed and thus treated as empty by the front-end.
    if (messageStr.IsEmpty()) {
      messageStr.AssignLiteral(MOZ_UTF16(" "));
    }
  } else {
    // Errors requiring simple formatting
    switch (aError) {
      case NS_ERROR_MALFORMED_URI:
        // URI is malformed
        error.AssignLiteral("malformedURI");
        break;
      case NS_ERROR_REDIRECT_LOOP:
        // Doc failed to load because the server generated too many redirects
        error.AssignLiteral("redirectLoop");
        break;
      case NS_ERROR_UNKNOWN_SOCKET_TYPE:
        // Doc failed to load because PSM is not installed
        error.AssignLiteral("unknownSocketType");
        break;
      case NS_ERROR_NET_RESET:
        // Doc failed to load because the server kept reseting the connection
        // before we could read any data from it
        error.AssignLiteral("netReset");
        break;
      case NS_ERROR_DOCUMENT_NOT_CACHED:
        // Doc failed to load because the cache does not contain a copy of
        // the document.
        error.AssignLiteral("notCached");
        break;
      case NS_ERROR_OFFLINE:
        // Doc failed to load because we are offline.
        error.AssignLiteral("netOffline");
        break;
      case NS_ERROR_DOCUMENT_IS_PRINTMODE:
        // Doc navigation attempted while Printing or Print Preview
        error.AssignLiteral("isprinting");
        break;
      case NS_ERROR_PORT_ACCESS_NOT_ALLOWED:
        // Port blocked for security reasons
        addHostPort = true;
        error.AssignLiteral("deniedPortAccess");
        break;
      case NS_ERROR_UNKNOWN_PROXY_HOST:
        // Proxy hostname could not be resolved.
        error.AssignLiteral("proxyResolveFailure");
        break;
      case NS_ERROR_PROXY_CONNECTION_REFUSED:
        // Proxy connection was refused.
        error.AssignLiteral("proxyConnectFailure");
        break;
      case NS_ERROR_INVALID_CONTENT_ENCODING:
        // Bad Content Encoding.
        error.AssignLiteral("contentEncodingError");
        break;
      case NS_ERROR_REMOTE_XUL:
        error.AssignLiteral("remoteXUL");
        break;
      case NS_ERROR_UNSAFE_CONTENT_TYPE:
        // Channel refused to load from an unrecognized content type.
        error.AssignLiteral("unsafeContentType");
        break;
      case NS_ERROR_CORRUPTED_CONTENT:
        // Broken Content Detected. e.g. Content-MD5 check failure.
        error.AssignLiteral("corruptedContentError");
        break;
      case NS_ERROR_INTERCEPTION_FAILED:
      case NS_ERROR_OPAQUE_INTERCEPTION_DISABLED:
      case NS_ERROR_BAD_OPAQUE_INTERCEPTION_REQUEST_MODE:
      case NS_ERROR_INTERCEPTED_ERROR_RESPONSE:
      case NS_ERROR_INTERCEPTED_USED_RESPONSE:
      case NS_ERROR_CLIENT_REQUEST_OPAQUE_INTERCEPTION:
      case NS_ERROR_BAD_OPAQUE_REDIRECT_INTERCEPTION:
      case NS_ERROR_INTERCEPTION_CANCELED:
      case NS_ERROR_REJECTED_RESPONSE_INTERCEPTION:
        // ServiceWorker intercepted request, but something went wrong.
        nsContentUtils::MaybeReportInterceptionErrorToConsole(GetDocument(),
                                                              aError);
        error.AssignLiteral("corruptedContentError");
        break;
      default:
        break;
    }
  }

  // Test if the error should be displayed
  if (error.IsEmpty()) {
    return NS_OK;
  }

  // Test if the error needs to be formatted
  if (!messageStr.IsEmpty()) {
    // already obtained message
  } else {
    if (addHostPort) {
      // Build up the host:port string.
      nsAutoCString hostport;
      if (aURI) {
        aURI->GetHostPort(hostport);
      } else {
        hostport.Assign('?');
      }
      CopyUTF8toUTF16(hostport, formatStrs[formatStrCount++]);
    }

    nsAutoCString spec;
    rv = NS_ERROR_NOT_AVAILABLE;
    if (aURI) {
      // displaying "file://" is aesthetically unpleasing and could even be
      // confusing to the user
      bool isFileURI = false;
      rv = aURI->SchemeIs("file", &isFileURI);
      if (NS_SUCCEEDED(rv) && isFileURI) {
        aURI->GetPath(spec);
      } else {
        aURI->GetSpec(spec);
      }

      nsAutoCString charset;
      // unescape and convert from origin charset
      aURI->GetOriginCharset(charset);
      nsCOMPtr<nsITextToSubURI> textToSubURI(
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) {
        rv = textToSubURI->UnEscapeURIForUI(charset, spec,
                                            formatStrs[formatStrCount]);
      }
    } else {
      spec.Assign('?');
    }
    if (NS_FAILED(rv)) {
      CopyUTF8toUTF16(spec, formatStrs[formatStrCount]);
    }
    rv = NS_OK;
    ++formatStrCount;

    const char16_t* strs[kMaxFormatStrArgs];
    for (uint32_t i = 0; i < formatStrCount; i++) {
      strs[i] = formatStrs[i].get();
    }
    nsXPIDLString str;
    rv = stringBundle->FormatStringFromName(error.get(), strs, formatStrCount,
                                            getter_Copies(str));
    NS_ENSURE_SUCCESS(rv, rv);
    messageStr.Assign(str.get());
  }

  // Display the error as a page or an alert prompt
  NS_ENSURE_FALSE(messageStr.IsEmpty(), NS_ERROR_FAILURE);

  if (NS_ERROR_NET_INTERRUPT == aError || NS_ERROR_NET_RESET == aError) {
    bool isSecureURI = false;
    rv = aURI->SchemeIs("https", &isSecureURI);
    if (NS_SUCCEEDED(rv) && isSecureURI) {
      // Maybe TLS intolerant. Treat this as an SSL error.
      error.AssignLiteral("nssFailure2");
    }
  }

  if (UseErrorPages()) {
    // Display an error page
    LoadErrorPage(aURI, aURL, errorPage.get(), error.get(),
                  messageStr.get(), cssClass.get(), aFailedChannel);
  } else {
    // The prompter reqires that our private window has a document (or it
    // asserts). Satisfy that assertion now since GetDoc will force
    // creation of one if it hasn't already been created.
    if (mScriptGlobal) {
      unused << mScriptGlobal->GetDoc();
    }

    // Display a message box
    prompter->Alert(nullptr, messageStr.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::LoadErrorPage(nsIURI* aURI, const char16_t* aURL,
                          const char* aErrorPage,
                          const char16_t* aErrorType,
                          const char16_t* aDescription,
                          const char* aCSSClass,
                          nsIChannel* aFailedChannel)
{
#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString spec;
    aURI->GetSpec(spec);

    nsAutoCString chanName;
    if (aFailedChannel) {
      aFailedChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]::LoadErrorPage(\"%s\", \"%s\", {...}, [%s])\n", this,
            spec.get(), NS_ConvertUTF16toUTF8(aURL).get(), chanName.get()));
  }
#endif
  mFailedChannel = aFailedChannel;
  mFailedURI = aURI;
  mFailedLoadType = mLoadType;

  if (mLSHE) {
    // Abandon mLSHE's BFCache entry and create a new one.  This way, if
    // we go back or forward to another SHEntry with the same doc
    // identifier, the error page won't persist.
    mLSHE->AbandonBFCacheEntry();
  }

  nsAutoCString url;
  nsAutoCString charset;
  if (aURI) {
    nsresult rv = aURI->GetSpec(url);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aURI->GetOriginCharset(charset);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aURL) {
    CopyUTF16toUTF8(aURL, url);
  } else {
    return NS_ERROR_INVALID_POINTER;
  }

  // Create a URL to pass all the error information through to the page.

#undef SAFE_ESCAPE
#define SAFE_ESCAPE(cstring, escArg1, escArg2)                                 \
  {                                                                            \
    char* s = nsEscape(escArg1, escArg2);                                      \
    if (!s)                                                                    \
      return NS_ERROR_OUT_OF_MEMORY;                                           \
    cstring.Adopt(s);                                                          \
  }

  nsCString escapedUrl, escapedCharset, escapedError, escapedDescription,
    escapedCSSClass;
  SAFE_ESCAPE(escapedUrl, url.get(), url_Path);
  SAFE_ESCAPE(escapedCharset, charset.get(), url_Path);
  SAFE_ESCAPE(escapedError,
              NS_ConvertUTF16toUTF8(aErrorType).get(), url_Path);
  SAFE_ESCAPE(escapedDescription,
              NS_ConvertUTF16toUTF8(aDescription).get(), url_Path);
  if (aCSSClass) {
    SAFE_ESCAPE(escapedCSSClass, aCSSClass, url_Path);
  }
  nsCString errorPageUrl("about:");
  errorPageUrl.AppendASCII(aErrorPage);
  errorPageUrl.AppendLiteral("?e=");

  errorPageUrl.AppendASCII(escapedError.get());
  errorPageUrl.AppendLiteral("&u=");
  errorPageUrl.AppendASCII(escapedUrl.get());
  if (!escapedCSSClass.IsEmpty()) {
    errorPageUrl.AppendLiteral("&s=");
    errorPageUrl.AppendASCII(escapedCSSClass.get());
  }
  errorPageUrl.AppendLiteral("&c=");
  errorPageUrl.AppendASCII(escapedCharset.get());

  nsAutoCString frameType(FrameTypeToString(mFrameType));
  errorPageUrl.AppendLiteral("&f=");
  errorPageUrl.AppendASCII(frameType.get());

  // Append the manifest URL if the error comes from an app.
  nsString manifestURL;
  nsresult rv = GetAppManifestURL(manifestURL);
  if (manifestURL.Length() > 0) {
    nsCString manifestParam;
    SAFE_ESCAPE(manifestParam,
                NS_ConvertUTF16toUTF8(manifestURL).get(),
                url_Path);
    errorPageUrl.AppendLiteral("&m=");
    errorPageUrl.AppendASCII(manifestParam.get());
  }

  // netError.xhtml's getDescription only handles the "d" parameter at the
  // end of the URL, so append it last.
  errorPageUrl.AppendLiteral("&d=");
  errorPageUrl.AppendASCII(escapedDescription.get());

  nsCOMPtr<nsIURI> errorPageURI;
  rv = NS_NewURI(getter_AddRefs(errorPageURI), errorPageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return InternalLoad(errorPageURI, nullptr, false, nullptr,
                      mozilla::net::RP_Default,
                      nullptr, INTERNAL_LOAD_FLAGS_INHERIT_OWNER, nullptr,
                      nullptr, NullString(), nullptr, nullptr, LOAD_ERROR_PAGE,
                      nullptr, true, NullString(), this, nullptr, nullptr,
                      nullptr);
}

NS_IMETHODIMP
nsDocShell::Reload(uint32_t aReloadFlags)
{
  if (!IsNavigationAllowed()) {
    return NS_OK; // JS may not handle returning of an error code
  }
  nsresult rv;
  NS_ASSERTION(((aReloadFlags & 0xf) == 0),
               "Reload command not updated to use load flags!");
  NS_ASSERTION((aReloadFlags & EXTRA_LOAD_FLAGS) == 0,
               "Don't pass these flags to Reload");

  uint32_t loadType = MAKE_LOAD_TYPE(LOAD_RELOAD_NORMAL, aReloadFlags);
  NS_ENSURE_TRUE(IsValidLoadType(loadType), NS_ERROR_INVALID_ARG);

  // Send notifications to the HistoryListener if any, about the impending
  // reload
  nsCOMPtr<nsISHistory> rootSH;
  rv = GetRootSessionHistory(getter_AddRefs(rootSH));
  nsCOMPtr<nsISHistoryInternal> shistInt(do_QueryInterface(rootSH));
  bool canReload = true;
  if (rootSH) {
    shistInt->NotifyOnHistoryReload(mCurrentURI, aReloadFlags, &canReload);
  }

  if (!canReload) {
    return NS_OK;
  }

  /* If you change this part of code, make sure bug 45297 does not re-occur */
  if (mOSHE) {
    rv = LoadHistoryEntry(mOSHE, loadType);
  } else if (mLSHE) { // In case a reload happened before the current load is done
    rv = LoadHistoryEntry(mLSHE, loadType);
  } else {
    nsCOMPtr<nsIDocument> doc(GetDocument());

    // Do not inherit owner from document
    uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;
    nsAutoString srcdoc;
    nsIPrincipal* principal = nullptr;
    nsAutoString contentTypeHint;
    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIURI> originalURI;
    bool loadReplace = false;
    if (doc) {
      principal = doc->NodePrincipal();
      doc->GetContentType(contentTypeHint);

      if (doc->IsSrcdocDocument()) {
        doc->GetSrcdocData(srcdoc);
        flags |= INTERNAL_LOAD_FLAGS_IS_SRCDOC;
        baseURI = doc->GetBaseURI();
      }
      nsCOMPtr<nsIChannel> chan = doc->GetChannel();
      if (chan) {
        uint32_t loadFlags;
        chan->GetLoadFlags(&loadFlags);
        loadReplace = loadFlags & nsIChannel::LOAD_REPLACE;
        nsCOMPtr<nsIHttpChannel> httpChan(do_QueryInterface(chan));
        if (httpChan) {
          httpChan->GetOriginalURI(getter_AddRefs(originalURI));
        }
      }
    }

    rv = InternalLoad(mCurrentURI,
                      originalURI,
                      loadReplace,
                      mReferrerURI,
                      mReferrerPolicy,
                      principal,
                      flags,
                      nullptr,         // No window target
                      NS_LossyConvertUTF16toASCII(contentTypeHint).get(),
                      NullString(),    // No forced download
                      nullptr,         // No post data
                      nullptr,         // No headers data
                      loadType,        // Load type
                      nullptr,         // No SHEntry
                      true,
                      srcdoc,          // srcdoc argument for iframe
                      this,            // For reloads we are the source
                      baseURI,
                      nullptr,         // No nsIDocShell
                      nullptr);        // No nsIRequest
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::Stop(uint32_t aStopFlags)
{
  // Revoke any pending event related to content viewer restoration
  mRestorePresentationEvent.Revoke();

  if (mLoadType == LOAD_ERROR_PAGE) {
    if (mLSHE) {
      // Since error page loads never unset mLSHE, do so now
      SetHistoryEntry(&mOSHE, mLSHE);
      SetHistoryEntry(&mLSHE, nullptr);
    }

    mFailedChannel = nullptr;
    mFailedURI = nullptr;
  }

  if (nsIWebNavigation::STOP_CONTENT & aStopFlags) {
    // Stop the document loading
    if (mContentViewer) {
      nsCOMPtr<nsIContentViewer> cv = mContentViewer;
      cv->Stop();
    }
  }

  if (nsIWebNavigation::STOP_NETWORK & aStopFlags) {
    // Suspend any timers that were set for this loader.  We'll clear
    // them out for good in CreateContentViewer.
    if (mRefreshURIList) {
      SuspendRefreshURIs();
      mSavedRefreshURIList.swap(mRefreshURIList);
      mRefreshURIList = nullptr;
    }

    // XXXbz We could also pass |this| to nsIURILoader::Stop.  That will
    // just call Stop() on us as an nsIDocumentLoader... We need fewer
    // redundant apis!
    Stop();
  }

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryObject(iter.GetNext()));
    if (shellAsNav) {
      shellAsNav->Stop(aStopFlags);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocument(nsIDOMDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);

  return mContentViewer->GetDOMDocument(aDocument);
}

NS_IMETHODIMP
nsDocShell::GetCurrentURI(nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  if (mCurrentURI) {
    return NS_EnsureSafeToReturn(mCurrentURI, aURI);
  }

  *aURI = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetReferringURI(nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = mReferrerURI;
  NS_IF_ADDREF(*aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSessionHistory(nsISHistory* aSessionHistory)
{
  NS_ENSURE_TRUE(aSessionHistory, NS_ERROR_FAILURE);
  // make sure that we are the root docshell and
  // set a handle to root docshell in SH.

  nsCOMPtr<nsIDocShellTreeItem> root;
  /* Get the root docshell. If *this* is the root docshell
   * then save a handle to *this* in SH. SH needs it to do
   * traversions thro' its entries
   */
  GetSameTypeRootTreeItem(getter_AddRefs(root));
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);
  if (root.get() == static_cast<nsIDocShellTreeItem*>(this)) {
    mSessionHistory = aSessionHistory;
    nsCOMPtr<nsISHistoryInternal> shPrivate =
      do_QueryInterface(mSessionHistory);
    NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
    shPrivate->SetRootDocShell(this);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetSessionHistory(nsISHistory** aSessionHistory)
{
  NS_ENSURE_ARG_POINTER(aSessionHistory);
  *aSessionHistory = mSessionHistory;
  NS_IF_ADDREF(*aSessionHistory);
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebPageDescriptor
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::LoadPage(nsISupports* aPageDescriptor, uint32_t aDisplayType)
{
  nsCOMPtr<nsISHEntry> shEntryIn(do_QueryInterface(aPageDescriptor));

  // Currently, the opaque 'page descriptor' is an nsISHEntry...
  if (!shEntryIn) {
    return NS_ERROR_INVALID_POINTER;
  }

  // Now clone shEntryIn, since we might end up modifying it later on, and we
  // want a page descriptor to be reusable.
  nsCOMPtr<nsISHEntry> shEntry;
  nsresult rv = shEntryIn->Clone(getter_AddRefs(shEntry));
  NS_ENSURE_SUCCESS(rv, rv);

  // Give our cloned shEntry a new bfcache entry so this load is independent
  // of all other loads.  (This is important, in particular, for bugs 582795
  // and 585298.)
  rv = shEntry->AbandonBFCacheEntry();
  NS_ENSURE_SUCCESS(rv, rv);

  //
  // load the page as view-source
  //
  if (nsIWebPageDescriptor::DISPLAY_AS_SOURCE == aDisplayType) {
    nsCOMPtr<nsIURI> oldUri, newUri;
    nsCString spec, newSpec;

    // Create a new view-source URI and replace the original.
    rv = shEntry->GetURI(getter_AddRefs(oldUri));
    if (NS_FAILED(rv)) {
      return rv;
    }

    oldUri->GetSpec(spec);
    newSpec.AppendLiteral("view-source:");
    newSpec.Append(spec);

    rv = NS_NewURI(getter_AddRefs(newUri), newSpec);
    if (NS_FAILED(rv)) {
      return rv;
    }
    shEntry->SetURI(newUri);
    shEntry->SetOriginalURI(nullptr);
  }

  rv = LoadHistoryEntry(shEntry, LOAD_HISTORY);
  return rv;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDescriptor(nsISupports** aPageDescriptor)
{
  NS_PRECONDITION(aPageDescriptor, "Null out param?");

  *aPageDescriptor = nullptr;

  nsISHEntry* src = mOSHE ? mOSHE : mLSHE;
  if (src) {
    nsCOMPtr<nsISHEntry> dest;

    nsresult rv = src->Clone(getter_AddRefs(dest));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // null out inappropriate cloned attributes...
    dest->SetParent(nullptr);
    dest->SetIsSubFrame(false);

    return CallQueryInterface(dest, aPageDescriptor);
  }

  return NS_ERROR_NOT_AVAILABLE;
}

//*****************************************************************************
// nsDocShell::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::InitWindow(nativeWindow aParentNativeWindow,
                       nsIWidget* aParentWidget, int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight)
{
  SetParentWidget(aParentWidget);
  SetPositionAndSize(aX, aY, aWidth, aHeight, false);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Create()
{
  if (mCreated) {
    // We've already been created
    return NS_OK;
  }

  NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
               "Unexpected item type in docshell");

  NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);
  mCreated = true;

  if (gValidateOrigin == 0xffffffff) {
    // Check pref to see if we should prevent frameset spoofing
    gValidateOrigin =
      Preferences::GetBool("browser.frame.validate_origin", true);
  }

  // Should we use XUL error pages instead of alerts if possible?
  mUseErrorPages =
    Preferences::GetBool("browser.xul.error_pages.enabled", mUseErrorPages);

  if (!gAddedPreferencesVarCache) {
    Preferences::AddBoolVarCache(&sUseErrorPages,
                                 "browser.xul.error_pages.enabled",
                                 mUseErrorPages);
    gAddedPreferencesVarCache = true;
  }

  mDeviceSizeIsPageSize =
    Preferences::GetBool("docshell.device_size_is_page_size",
                         mDeviceSizeIsPageSize);

  nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
  if (serv) {
    const char* msg = mItemType == typeContent ?
      NS_WEBNAVIGATION_CREATE : NS_CHROME_WEBNAVIGATION_CREATE;
    serv->NotifyObservers(GetAsSupports(this), msg, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Destroy()
{
  NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
               "Unexpected item type in docshell");

  if (!mIsBeingDestroyed) {
    nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
    if (serv) {
      const char* msg = mItemType == typeContent ?
        NS_WEBNAVIGATION_DESTROY : NS_CHROME_WEBNAVIGATION_DESTROY;
      serv->NotifyObservers(GetAsSupports(this), msg, nullptr);
    }
  }

  mIsBeingDestroyed = true;

  // Make sure we don't record profile timeline markers anymore
  SetRecordProfileTimelineMarkers(false);

  // Remove our pref observers
  if (mObserveErrorPages) {
    mObserveErrorPages = false;
  }

  // Make sure to blow away our mLoadingURI just in case.  No loads
  // from inside this pagehide.
  mLoadingURI = nullptr;

  // Fire unload event before we blow anything away.
  (void)FirePageHideNotification(true);

  // Clear pointers to any detached nsEditorData that's lying
  // around in shistory entries. Breaks cycle. See bug 430921.
  if (mOSHE) {
    mOSHE->SetEditorData(nullptr);
  }
  if (mLSHE) {
    mLSHE->SetEditorData(nullptr);
  }

  // Note: mContentListener can be null if Init() failed and we're being
  // called from the destructor.
  if (mContentListener) {
    mContentListener->DropDocShellReference();
    mContentListener->SetParentContentListener(nullptr);
    // Note that we do NOT set mContentListener to null here; that
    // way if someone tries to do a load in us after this point
    // the nsDSURIContentListener will block it.  All of which
    // means that we should do this before calling Stop(), of
    // course.
  }

  // Stop any URLs that are currently being loaded...
  Stop(nsIWebNavigation::STOP_ALL);

  mEditorData = nullptr;

  mTransferableHookData = nullptr;

  // Save the state of the current document, before destroying the window.
  // This is needed to capture the state of a frameset when the new document
  // causes the frameset to be destroyed...
  PersistLayoutHistoryState();

  // Remove this docshell from its parent's child list
  nsCOMPtr<nsIDocShellTreeItem> docShellParentAsItem =
    do_QueryInterface(GetAsSupports(mParent));
  if (docShellParentAsItem) {
    docShellParentAsItem->RemoveChild(this);
  }

  if (mContentViewer) {
    mContentViewer->Close(nullptr);
    mContentViewer->Destroy();
    mContentViewer = nullptr;
  }

  nsDocLoader::Destroy();

  mParentWidget = nullptr;
  mCurrentURI = nullptr;

  if (mScriptGlobal) {
    mScriptGlobal->DetachFromDocShell();
    mScriptGlobal = nullptr;
  }

  if (mSessionHistory) {
    // We want to destroy these content viewers now rather than
    // letting their destruction wait for the session history
    // entries to get garbage collected.  (Bug 488394)
    nsCOMPtr<nsISHistoryInternal> shPrivate =
      do_QueryInterface(mSessionHistory);
    if (shPrivate) {
      shPrivate->EvictAllContentViewers();
    }
    mSessionHistory = nullptr;
  }

  SetTreeOwner(nullptr);

  mOnePermittedSandboxedNavigator = nullptr;

  // required to break ref cycle
  mSecurityUI = nullptr;

  // Cancel any timers that were set for this docshell; this is needed
  // to break the cycle between us and the timers.
  CancelRefreshURITimers();

  if (mInPrivateBrowsing) {
    mInPrivateBrowsing = false;
    if (mAffectPrivateSessionLifetime) {
      DecreasePrivateDocShellCount();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUnscaledDevicePixelsPerCSSPixel(double* aScale)
{
  if (mParentWidget) {
    *aScale = mParentWidget->GetDefaultScale().scale;
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> ownerWindow(do_QueryInterface(mTreeOwner));
  if (ownerWindow) {
    return ownerWindow->GetUnscaledDevicePixelsPerCSSPixel(aScale);
  }

  *aScale = 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPosition(int32_t aX, int32_t aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

  if (mContentViewer) {
    NS_ENSURE_SUCCESS(mContentViewer->Move(aX, aY), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPosition(int32_t* aX, int32_t* aY)
{
  return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP
nsDocShell::SetSize(int32_t aWidth, int32_t aHeight, bool aRepaint)
{
  int32_t x = 0, y = 0;
  GetPosition(&x, &y);
  return SetPositionAndSize(x, y, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
nsDocShell::GetSize(int32_t* aWidth, int32_t* aHeight)
{
  return GetPositionAndSize(nullptr, nullptr, aWidth, aHeight);
}

NS_IMETHODIMP
nsDocShell::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aWidth,
                               int32_t aHeight, bool aFRepaint)
{
  mBounds.x = aX;
  mBounds.y = aY;
  mBounds.width = aWidth;
  mBounds.height = aHeight;

  // Hold strong ref, since SetBounds can make us null out mContentViewer
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  if (viewer) {
    // XXX Border figured in here or is that handled elsewhere?
    NS_ENSURE_SUCCESS(viewer->SetBounds(mBounds), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                               int32_t* aHeight)
{
  if (mParentWidget) {
    // ensure size is up-to-date if window has changed resolution
    nsIntRect r;
    mParentWidget->GetClientBounds(r);
    SetPositionAndSize(mBounds.x, mBounds.y, r.width, r.height, false);
  }

  // We should really consider just getting this information from
  // our window instead of duplicating the storage and code...
  if (aWidth || aHeight) {
    // Caller wants to know our size; make sure to give them up to
    // date information.
    nsCOMPtr<nsIDocument> doc(do_GetInterface(GetAsSupports(mParent)));
    if (doc) {
      doc->FlushPendingNotifications(Flush_Layout);
    }
  }

  DoGetPositionAndSize(aX, aY, aWidth, aHeight);
  return NS_OK;
}

void
nsDocShell::DoGetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                                 int32_t* aHeight)
{
  if (aX) {
    *aX = mBounds.x;
  }
  if (aY) {
    *aY = mBounds.y;
  }
  if (aWidth) {
    *aWidth = mBounds.width;
  }
  if (aHeight) {
    *aHeight = mBounds.height;
  }
}

NS_IMETHODIMP
nsDocShell::Repaint(bool aForce)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsViewManager* viewManager = presShell->GetViewManager();
  NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

  viewManager->InvalidateAllViews();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentWidget(nsIWidget** aParentWidget)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);

  *aParentWidget = mParentWidget;
  NS_IF_ADDREF(*aParentWidget);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentWidget(nsIWidget* aParentWidget)
{
  mParentWidget = aParentWidget;

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aParentNativeWindow);

  if (mParentWidget) {
    *aParentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    *aParentNativeWindow = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetNativeHandle(nsAString& aNativeHandle)
{
  // the nativeHandle should be accessed from nsIXULWindow
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetVisibility(bool* aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

  *aVisibility = false;

  if (!mContentViewer) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  // get the view manager
  nsViewManager* vm = presShell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  // get the root view
  nsView* view = vm->GetRootView(); // views are not ref counted
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  // if our root view is hidden, we are not visible
  if (view->GetVisibility() == nsViewVisibility_kHide) {
    return NS_OK;
  }

  // otherwise, we must walk up the document and view trees checking
  // for a hidden view, unless we're an off screen browser, which
  // would make this test meaningless.

  RefPtr<nsDocShell> docShell = this;
  RefPtr<nsDocShell> parentItem = docShell->GetParentDocshell();
  while (parentItem) {
    presShell = docShell->GetPresShell();

    nsCOMPtr<nsIPresShell> pPresShell = parentItem->GetPresShell();

    // Null-check for crash in bug 267804
    if (!pPresShell) {
      NS_NOTREACHED("parent docshell has null pres shell");
      return NS_OK;
    }

    vm = presShell->GetViewManager();
    if (vm) {
      view = vm->GetRootView();
    }

    if (view) {
      view = view->GetParent(); // anonymous inner view
      if (view) {
        view = view->GetParent(); // subdocumentframe's view
      }
    }

    nsIFrame* frame = view ? view->GetFrame() : nullptr;
    bool isDocShellOffScreen = false;
    docShell->GetIsOffScreenBrowser(&isDocShellOffScreen);
    if (frame &&
        !frame->IsVisibleConsideringAncestors(
          nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) &&
        !isDocShellOffScreen) {
      return NS_OK;
    }

    docShell = parentItem;
    parentItem = docShell->GetParentDocshell();
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(mTreeOwner));
  if (!treeOwnerAsWin) {
    *aVisibility = true;
    return NS_OK;
  }

  // Check with the tree owner as well to give embedders a chance to
  // expose visibility as well.
  return treeOwnerAsWin->GetVisibility(aVisibility);
}

NS_IMETHODIMP
nsDocShell::SetIsOffScreenBrowser(bool aIsOffScreen)
{
  mIsOffScreenBrowser = aIsOffScreen;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsOffScreenBrowser(bool* aIsOffScreen)
{
  *aIsOffScreen = mIsOffScreenBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsActive(bool aIsActive)
{
  // We disallow setting active on chrome docshells.
  if (mItemType == nsIDocShellTreeItem::typeChrome) {
    return NS_ERROR_INVALID_ARG;
  }

  // Keep track ourselves.
  mIsActive = aIsActive;

  // Tell the PresShell about it.
  nsCOMPtr<nsIPresShell> pshell = GetPresShell();
  if (pshell) {
    pshell->SetIsActive(aIsActive);
  }

  // Tell the window about it
  if (mScriptGlobal) {
    mScriptGlobal->SetIsBackground(!aIsActive);
    if (nsCOMPtr<nsIDocument> doc = mScriptGlobal->GetExtantDoc()) {
      // Update orientation when the top-level browsing context becomes active.
      // We make an exception for apps because they currently rely on
      // orientation locks persisting across browsing contexts.
      if (aIsActive && !GetIsApp()) {
        nsCOMPtr<nsIDocShellTreeItem> parent;
        GetSameTypeParent(getter_AddRefs(parent));
        if (!parent) {
          // We only care about the top-level browsing context.
          uint16_t orientation = OrientationLock();
          ScreenOrientation::UpdateActiveOrientationLock(orientation);
        }
      }

      doc->PostVisibilityUpdateEvent();
    }
  }

  // Recursively tell all of our children, but don't tell <iframe mozbrowser>
  // children; they handle their state separately.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> docshell = do_QueryObject(iter.GetNext());
    if (!docshell) {
      continue;
    }

    if (!docshell->GetIsBrowserOrApp()) {
      docshell->SetIsActive(aIsActive);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsActive(bool* aIsActive)
{
  *aIsActive = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsPrerendered(bool aPrerendered)
{
  MOZ_ASSERT(!aPrerendered || !mIsPrerendered,
             "SetIsPrerendered(true) called on already prerendered docshell");
  mIsPrerendered = aPrerendered;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsPrerendered(bool* aIsPrerendered)
{
  *aIsPrerendered = mIsPrerendered;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsAppTab(bool aIsAppTab)
{
  mIsAppTab = aIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsAppTab(bool* aIsAppTab)
{
  *aIsAppTab = mIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSandboxFlags(uint32_t aSandboxFlags)
{
  mSandboxFlags = aSandboxFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSandboxFlags(uint32_t* aSandboxFlags)
{
  *aSandboxFlags = mSandboxFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetOnePermittedSandboxedNavigator(nsIDocShell* aSandboxedNavigator)
{
  if (mOnePermittedSandboxedNavigator) {
    NS_ERROR("One Permitted Sandboxed Navigator should only be set once.");
    return NS_OK;
  }

  mOnePermittedSandboxedNavigator = do_GetWeakReference(aSandboxedNavigator);
  NS_ASSERTION(mOnePermittedSandboxedNavigator,
               "One Permitted Sandboxed Navigator must support weak references.");

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetOnePermittedSandboxedNavigator(nsIDocShell** aSandboxedNavigator)
{
  NS_ENSURE_ARG_POINTER(aSandboxedNavigator);
  nsCOMPtr<nsIDocShell> permittedNavigator =
    do_QueryReferent(mOnePermittedSandboxedNavigator);
  permittedNavigator.forget(aSandboxedNavigator);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDefaultLoadFlags(uint32_t aDefaultLoadFlags)
{
  mDefaultLoadFlags = aDefaultLoadFlags;

  // Tell the load group to set these flags all requests in the group
  if (mLoadGroup) {
    mLoadGroup->SetDefaultLoadFlags(aDefaultLoadFlags);
  } else {
    NS_WARNING("nsDocShell::SetDefaultLoadFlags has no loadGroup to propagate the flags to");
  }

  // Recursively tell all of our children.  We *do not* skip
  // <iframe mozbrowser> children - if someone sticks custom flags in this
  // docShell then they too get the same flags.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> docshell = do_QueryObject(iter.GetNext());
    if (!docshell) {
      continue;
    }
    docshell->SetDefaultLoadFlags(aDefaultLoadFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDefaultLoadFlags(uint32_t* aDefaultLoadFlags)
{
  *aDefaultLoadFlags = mDefaultLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMixedContentChannel(nsIChannel* aMixedContentChannel)
{
#ifdef DEBUG
  // if the channel is non-null
  if (aMixedContentChannel) {
    // Get the root docshell.
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    NS_WARN_IF_FALSE(root.get() == static_cast<nsIDocShellTreeItem*>(this),
                     "Setting mMixedContentChannel on a docshell that is not "
                     "the root docshell");
  }
#endif
  mMixedContentChannel = aMixedContentChannel;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetFailedChannel(nsIChannel** aFailedChannel)
{
  NS_ENSURE_ARG_POINTER(aFailedChannel);
  nsIDocument* doc = GetDocument();
  if (!doc) {
    *aFailedChannel = nullptr;
    return NS_OK;
  }
  NS_IF_ADDREF(*aFailedChannel = doc->GetFailedChannel());
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMixedContentChannel(nsIChannel** aMixedContentChannel)
{
  NS_ENSURE_ARG_POINTER(aMixedContentChannel);
  NS_IF_ADDREF(*aMixedContentChannel = mMixedContentChannel);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowMixedContentAndConnectionData(bool* aRootHasSecureConnection,
                                                  bool* aAllowMixedContent,
                                                  bool* aIsRootDocShell)
{
  *aRootHasSecureConnection = true;
  *aAllowMixedContent = false;
  *aIsRootDocShell = false;

  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
  NS_ASSERTION(sameTypeRoot,
               "No document shell root tree item from document shell tree item!");
  *aIsRootDocShell =
    sameTypeRoot.get() == static_cast<nsIDocShellTreeItem*>(this);

  // now get the document from sameTypeRoot
  nsCOMPtr<nsIDocument> rootDoc = sameTypeRoot->GetDocument();
  if (rootDoc) {
    nsCOMPtr<nsIPrincipal> rootPrincipal = rootDoc->NodePrincipal();

    // For things with system principal (e.g. scratchpad) there is no uri
    // aRootHasSecureConnection should be false.
    nsCOMPtr<nsIURI> rootUri;
    if (nsContentUtils::IsSystemPrincipal(rootPrincipal) ||
        NS_FAILED(rootPrincipal->GetURI(getter_AddRefs(rootUri))) || !rootUri ||
        NS_FAILED(rootUri->SchemeIs("https", aRootHasSecureConnection))) {
      *aRootHasSecureConnection = false;
    }

    // Check the root doc's channel against the root docShell's
    // mMixedContentChannel to see if they are the same. If they are the same,
    // the user has overriden the block.
    nsCOMPtr<nsIDocShell> rootDocShell = do_QueryInterface(sameTypeRoot);
    nsCOMPtr<nsIChannel> mixedChannel;
    rootDocShell->GetMixedContentChannel(getter_AddRefs(mixedChannel));
    *aAllowMixedContent =
      mixedChannel && (mixedChannel == rootDoc->GetChannel());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetVisibility(bool aVisibility)
{
  // Show()/Hide() may change mContentViewer.
  nsCOMPtr<nsIContentViewer> cv = mContentViewer;
  if (!cv) {
    return NS_OK;
  }
  if (aVisibility) {
    cv->Show();
  } else {
    cv->Hide();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEnabled(bool* aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = true;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::SetEnabled(bool aEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::SetFocus()
{
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMainWidget(nsIWidget** aMainWidget)
{
  // We don't create our own widget, so simply return the parent one.
  return GetParentWidget(aMainWidget);
}

NS_IMETHODIMP
nsDocShell::GetTitle(char16_t** aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);

  *aTitle = ToNewUnicode(mTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTitle(const char16_t* aTitle)
{
  // Store local title
  mTitle = aTitle;

  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetSameTypeParent(getter_AddRefs(parent));

  // When title is set on the top object it should then be passed to the
  // tree owner.
  if (!parent) {
    nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(mTreeOwner));
    if (treeOwnerAsWin) {
      treeOwnerAsWin->SetTitle(aTitle);
    }
  }

  if (mCurrentURI && mLoadType != LOAD_ERROR_PAGE && mUseGlobalHistory &&
      !mInPrivateBrowsing) {
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
      history->SetURITitle(mCurrentURI, mTitle);
    } else if (mGlobalHistory) {
      mGlobalHistory->SetPageTitle(mCurrentURI, nsString(mTitle));
    }
  }

  // Update SessionHistory with the document's title.
  if (mOSHE && mLoadType != LOAD_BYPASS_HISTORY &&
      mLoadType != LOAD_ERROR_PAGE) {
    mOSHE->SetTitle(mTitle);
  }

  return NS_OK;
}

nsresult
nsDocShell::GetCurScrollPos(int32_t aScrollOrientation, int32_t* aCurPos)
{
  NS_ENSURE_ARG_POINTER(aCurPos);

  nsIScrollableFrame* sf = GetRootScrollFrame();
  if (!sf) {
    return NS_ERROR_FAILURE;
  }

  nsPoint pt = sf->GetScrollPosition();

  switch (aScrollOrientation) {
    case ScrollOrientation_X:
      *aCurPos = pt.x;
      return NS_OK;

    case ScrollOrientation_Y:
      *aCurPos = pt.y;
      return NS_OK;

    default:
      NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
  }
}

nsresult
nsDocShell::SetCurScrollPosEx(int32_t aCurHorizontalPos,
                              int32_t aCurVerticalPos)
{
  nsIScrollableFrame* sf = GetRootScrollFrame();
  NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

  sf->ScrollTo(nsPoint(aCurHorizontalPos, aCurVerticalPos),
               nsIScrollableFrame::INSTANT);
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetDefaultScrollbarPreferences(int32_t aScrollOrientation,
                                           int32_t* aScrollbarPref)
{
  NS_ENSURE_ARG_POINTER(aScrollbarPref);
  switch (aScrollOrientation) {
    case ScrollOrientation_X:
      *aScrollbarPref = mDefaultScrollbarPref.x;
      return NS_OK;

    case ScrollOrientation_Y:
      *aScrollbarPref = mDefaultScrollbarPref.y;
      return NS_OK;

    default:
      NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::SetDefaultScrollbarPreferences(int32_t aScrollOrientation,
                                           int32_t aScrollbarPref)
{
  switch (aScrollOrientation) {
    case ScrollOrientation_X:
      mDefaultScrollbarPref.x = aScrollbarPref;
      return NS_OK;

    case ScrollOrientation_Y:
      mDefaultScrollbarPref.y = aScrollbarPref;
      return NS_OK;

    default:
      NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetScrollbarVisibility(bool* aVerticalVisible,
                                   bool* aHorizontalVisible)
{
  nsIScrollableFrame* sf = GetRootScrollFrame();
  NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

  uint32_t scrollbarVisibility = sf->GetScrollbarVisibility();
  if (aVerticalVisible) {
    *aVerticalVisible =
      (scrollbarVisibility & nsIScrollableFrame::VERTICAL) != 0;
  }
  if (aHorizontalVisible) {
    *aHorizontalVisible =
      (scrollbarVisibility & nsIScrollableFrame::HORIZONTAL) != 0;
  }

  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::ScrollByLines(int32_t aNumLines)
{
  nsIScrollableFrame* sf = GetRootScrollFrame();
  NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

  sf->ScrollBy(nsIntPoint(0, aNumLines), nsIScrollableFrame::LINES,
               nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ScrollByPages(int32_t aNumPages)
{
  nsIScrollableFrame* sf = GetRootScrollFrame();
  NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

  sf->ScrollBy(nsIntPoint(0, aNumPages), nsIScrollableFrame::PAGES,
               nsIScrollableFrame::SMOOTH);
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI* aURI, int32_t aDelay, bool aRepeat,
                       bool aMetaRefresh)
{
  NS_ENSURE_ARG(aURI);

  /* Check if Meta refresh/redirects are permitted. Some
   * embedded applications may not want to do this.
   * Must do this before sending out NOTIFY_REFRESH events
   * because listeners may have side effects (e.g. displaying a
   * button to manually trigger the refresh later).
   */
  bool allowRedirects = true;
  GetAllowMetaRedirects(&allowRedirects);
  if (!allowRedirects) {
    return NS_OK;
  }

  // If any web progress listeners are listening for NOTIFY_REFRESH events,
  // give them a chance to block this refresh.
  bool sameURI;
  nsresult rv = aURI->Equals(mCurrentURI, &sameURI);
  if (NS_FAILED(rv)) {
    sameURI = false;
  }
  if (!RefreshAttempted(this, aURI, aDelay, sameURI)) {
    return NS_OK;
  }

  nsRefreshTimer* refreshTimer = new nsRefreshTimer();
  NS_ENSURE_TRUE(refreshTimer, NS_ERROR_OUT_OF_MEMORY);
  uint32_t busyFlags = 0;
  GetBusyFlags(&busyFlags);

  nsCOMPtr<nsISupports> dataRef = refreshTimer;  // Get the ref count to 1

  refreshTimer->mDocShell = this;
  refreshTimer->mURI = aURI;
  refreshTimer->mDelay = aDelay;
  refreshTimer->mRepeat = aRepeat;
  refreshTimer->mMetaRefresh = aMetaRefresh;

  if (!mRefreshURIList) {
    NS_ENSURE_SUCCESS(NS_NewISupportsArray(getter_AddRefs(mRefreshURIList)),
                      NS_ERROR_FAILURE);
  }

  if (busyFlags & BUSY_FLAGS_BUSY) {
    // We are busy loading another page. Don't create the
    // timer right now. Instead queue up the request and trigger the
    // timer in EndPageLoad().
    mRefreshURIList->AppendElement(refreshTimer);
  } else {
    // There is no page loading going on right now.  Create the
    // timer and fire it right away.
    nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

    mRefreshURIList->AppendElement(timer);  // owning timer ref
    timer->InitWithCallback(refreshTimer, aDelay, nsITimer::TYPE_ONE_SHOT);
  }
  return NS_OK;
}

nsresult
nsDocShell::ForceRefreshURIFromTimer(nsIURI* aURI,
                                     int32_t aDelay,
                                     bool aMetaRefresh,
                                     nsITimer* aTimer)
{
  NS_PRECONDITION(aTimer, "Must have a timer here");

  // Remove aTimer from mRefreshURIList if needed
  if (mRefreshURIList) {
    uint32_t n = 0;
    mRefreshURIList->Count(&n);

    for (uint32_t i = 0; i < n; ++i) {
      nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
      if (timer == aTimer) {
        mRefreshURIList->RemoveElementAt(i);
        break;
      }
    }
  }

  return ForceRefreshURI(aURI, aDelay, aMetaRefresh);
}

bool
nsDocShell::DoAppRedirectIfNeeded(nsIURI* aURI,
                                  nsIDocShellLoadInfo* aLoadInfo,
                                  bool aFirstParty)
{
  uint32_t appId = nsIDocShell::GetAppId();

  if (appId != nsIScriptSecurityManager::NO_APP_ID &&
      appId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    nsCOMPtr<nsIAppsService> appsService =
      do_GetService(APPS_SERVICE_CONTRACTID);
    NS_ASSERTION(appsService, "No AppsService available");
    nsCOMPtr<nsIURI> redirect;
    nsresult rv = appsService->GetRedirect(appId, aURI, getter_AddRefs(redirect));
    if (NS_SUCCEEDED(rv) && redirect) {
      rv = LoadURI(redirect, aLoadInfo, nsIWebNavigation::LOAD_FLAGS_NONE,
                   aFirstParty);
      if (NS_SUCCEEDED(rv)) {
        return true;
      }
    }
  }

  return false;
}

NS_IMETHODIMP
nsDocShell::ForceRefreshURI(nsIURI* aURI, int32_t aDelay, bool aMetaRefresh)
{
  NS_ENSURE_ARG(aURI);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);

  /* We do need to pass in a referrer, but we don't want it to
   * be sent to the server.
   */
  loadInfo->SetSendReferrer(false);

  /* for most refreshes the current URI is an appropriate
   * internal referrer
   */
  loadInfo->SetReferrer(mCurrentURI);

  /* Don't ever "guess" on which owner to use to avoid picking
   * the current owner.
   */
  loadInfo->SetOwnerIsExplicit(true);

  /* Check if this META refresh causes a redirection
   * to another site.
   */
  bool equalUri = false;
  nsresult rv = aURI->Equals(mCurrentURI, &equalUri);
  if (NS_SUCCEEDED(rv) && (!equalUri) && aMetaRefresh &&
      aDelay <= REFRESH_REDIRECT_TIMER) {
    /* It is a META refresh based redirection within the threshold time
     * we have in mind (15000 ms as defined by REFRESH_REDIRECT_TIMER).
     * Pass a REPLACE flag to LoadURI().
     */
    loadInfo->SetLoadType(nsIDocShellLoadInfo::loadNormalReplace);

    /* for redirects we mimic HTTP, which passes the
     *  original referrer
     */
    nsCOMPtr<nsIURI> internalReferrer;
    GetReferringURI(getter_AddRefs(internalReferrer));
    if (internalReferrer) {
      loadInfo->SetReferrer(internalReferrer);
    }
  } else {
    loadInfo->SetLoadType(nsIDocShellLoadInfo::loadRefresh);
  }

  /*
   * LoadURI(...) will cancel all refresh timers... This causes the
   * Timer and its refreshData instance to be released...
   */
  LoadURI(aURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, true);

  return NS_OK;
}

nsresult
nsDocShell::SetupRefreshURIFromHeader(nsIURI* aBaseURI,
                                      nsIPrincipal* aPrincipal,
                                      const nsACString& aHeader)
{
  // Refresh headers are parsed with the following format in mind
  // <META HTTP-EQUIV=REFRESH CONTENT="5; URL=http://uri">
  // By the time we are here, the following is true:
  // header = "REFRESH"
  // content = "5; URL=http://uri" // note the URL attribute is
  // optional, if it is absent, the currently loaded url is used.
  // Also note that the seconds and URL separator can be either
  // a ';' or a ','. The ',' separator should be illegal but CNN
  // is using it.
  //
  // We need to handle the following strings, where
  //  - X is a set of digits
  //  - URI is either a relative or absolute URI
  //
  // Note that URI should start with "url=" but we allow omission
  //
  // "" || ";" || ","
  //  empty string. use the currently loaded URI
  //  and refresh immediately.
  // "X" || "X;" || "X,"
  //  Refresh the currently loaded URI in X seconds.
  // "X; URI" || "X, URI"
  //  Refresh using URI as the destination in X seconds.
  // "URI" || "; URI" || ", URI"
  //  Refresh immediately using URI as the destination.
  //
  // Currently, anything immediately following the URI, if
  // separated by any char in the set "'\"\t\r\n " will be
  // ignored. So "10; url=go.html ; foo=bar" will work,
  // and so will "10; url='go.html'; foo=bar". However,
  // "10; url=go.html; foo=bar" will result in the uri
  // "go.html;" since ';' and ',' are valid uri characters.
  //
  // Note that we need to remove any tokens wrapping the URI.
  // These tokens currently include spaces, double and single
  // quotes.

  // when done, seconds is 0 or the given number of seconds
  //            uriAttrib is empty or the URI specified
  MOZ_ASSERT(aPrincipal);

  nsAutoCString uriAttrib;
  int32_t seconds = 0;
  bool specifiesSeconds = false;

  nsACString::const_iterator iter, tokenStart, doneIterating;

  aHeader.BeginReading(iter);
  aHeader.EndReading(doneIterating);

  // skip leading whitespace
  while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter)) {
    ++iter;
  }

  tokenStart = iter;

  // skip leading + and -
  if (iter != doneIterating && (*iter == '-' || *iter == '+')) {
    ++iter;
  }

  // parse number
  while (iter != doneIterating && (*iter >= '0' && *iter <= '9')) {
    seconds = seconds * 10 + (*iter - '0');
    specifiesSeconds = true;
    ++iter;
  }

  if (iter != doneIterating) {
    // if we started with a '-', number is negative
    if (*tokenStart == '-') {
      seconds = -seconds;
    }

    // skip to next ';' or ','
    nsACString::const_iterator iterAfterDigit = iter;
    while (iter != doneIterating && !(*iter == ';' || *iter == ',')) {
      if (specifiesSeconds) {
        // Non-whitespace characters here mean that the string is
        // malformed but tolerate sites that specify a decimal point,
        // even though meta refresh only works on whole seconds.
        if (iter == iterAfterDigit &&
            !nsCRT::IsAsciiSpace(*iter) && *iter != '.') {
          // The characters between the seconds and the next
          // section are just garbage!
          //   e.g. content="2a0z+,URL=http://www.mozilla.org/"
          // Just ignore this redirect.
          return NS_ERROR_FAILURE;
        } else if (nsCRT::IsAsciiSpace(*iter)) {
          // We've had at least one whitespace so tolerate the mistake
          // and drop through.
          // e.g. content="10 foo"
          ++iter;
          break;
        }
      }
      ++iter;
    }

    // skip any remaining whitespace
    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }

    // skip ';' or ','
    if (iter != doneIterating && (*iter == ';' || *iter == ',')) {
      ++iter;
    }

    // skip whitespace
    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }
  }

  // possible start of URI
  tokenStart = iter;

  // skip "url = " to real start of URI
  if (iter != doneIterating && (*iter == 'u' || *iter == 'U')) {
    ++iter;
    if (iter != doneIterating && (*iter == 'r' || *iter == 'R')) {
      ++iter;
      if (iter != doneIterating && (*iter == 'l' || *iter == 'L')) {
        ++iter;

        // skip whitespace
        while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter)) {
          ++iter;
        }

        if (iter != doneIterating && *iter == '=') {
          ++iter;

          // skip whitespace
          while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter)) {
            ++iter;
          }

          // found real start of URI
          tokenStart = iter;
        }
      }
    }
  }

  // skip a leading '"' or '\''.

  bool isQuotedURI = false;
  if (tokenStart != doneIterating &&
      (*tokenStart == '"' || *tokenStart == '\'')) {
    isQuotedURI = true;
    ++tokenStart;
  }

  // set iter to start of URI
  iter = tokenStart;

  // tokenStart here points to the beginning of URI

  // grab the rest of the URI
  while (iter != doneIterating) {
    if (isQuotedURI && (*iter == '"' || *iter == '\'')) {
      break;
    }
    ++iter;
  }

  // move iter one back if the last character is a '"' or '\''
  if (iter != tokenStart && isQuotedURI) {
    --iter;
    if (!(*iter == '"' || *iter == '\'')) {
      ++iter;
    }
  }

  // URI is whatever's contained from tokenStart to iter.
  // note: if tokenStart == doneIterating, so is iter.

  nsresult rv = NS_OK;

  nsCOMPtr<nsIURI> uri;
  bool specifiesURI = false;
  if (tokenStart == iter) {
    uri = aBaseURI;
  } else {
    uriAttrib = Substring(tokenStart, iter);
    // NS_NewURI takes care of any whitespace surrounding the URL
    rv = NS_NewURI(getter_AddRefs(uri), uriAttrib, nullptr, aBaseURI);
    specifiesURI = true;
  }

  // No URI or seconds were specified
  if (!specifiesSeconds && !specifiesURI) {
    // Do nothing because the alternative is to spin around in a refresh
    // loop forever!
    return NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIScriptSecurityManager> securityManager(
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
      rv = securityManager->CheckLoadURIWithPrincipal(
        aPrincipal, uri,
        nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT);

      if (NS_SUCCEEDED(rv)) {
        bool isjs = true;
        rv = NS_URIChainHasFlags(
          uri, nsIProtocolHandler::URI_OPENING_EXECUTES_SCRIPT, &isjs);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isjs) {
          return NS_ERROR_FAILURE;
        }
      }

      if (NS_SUCCEEDED(rv)) {
        // Since we can't travel back in time yet, just pretend
        // negative numbers do nothing at all.
        if (seconds < 0) {
          return NS_ERROR_FAILURE;
        }

        rv = RefreshURI(uri, seconds * 1000, false, true);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::SetupRefreshURI(nsIChannel* aChannel)
{
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel, &rv));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString refreshHeader;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("refresh"),
                                        refreshHeader);

    if (!refreshHeader.IsEmpty()) {
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPrincipal> principal;
      rv = secMan->GetChannelResultPrincipal(aChannel,
                                             getter_AddRefs(principal));
      NS_ENSURE_SUCCESS(rv, rv);

      SetupReferrerFromChannel(aChannel);
      rv = SetupRefreshURIFromHeader(mCurrentURI, principal, refreshHeader);
      if (NS_SUCCEEDED(rv)) {
        return NS_REFRESHURI_HEADER_FOUND;
      }
    }
  }
  return rv;
}

static void
DoCancelRefreshURITimers(nsISupportsArray* aTimerList)
{
  if (!aTimerList) {
    return;
  }

  uint32_t n = 0;
  aTimerList->Count(&n);

  while (n) {
    nsCOMPtr<nsITimer> timer(do_QueryElementAt(aTimerList, --n));

    aTimerList->RemoveElementAt(n);  // bye bye owning timer ref

    if (timer) {
      timer->Cancel();
    }
  }
}

NS_IMETHODIMP
nsDocShell::CancelRefreshURITimers()
{
  DoCancelRefreshURITimers(mRefreshURIList);
  DoCancelRefreshURITimers(mSavedRefreshURIList);
  mRefreshURIList = nullptr;
  mSavedRefreshURIList = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRefreshPending(bool* aResult)
{
  if (!mRefreshURIList) {
    *aResult = false;
    return NS_OK;
  }

  uint32_t count;
  nsresult rv = mRefreshURIList->Count(&count);
  if (NS_SUCCEEDED(rv)) {
    *aResult = (count != 0);
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::SuspendRefreshURIs()
{
  if (mRefreshURIList) {
    uint32_t n = 0;
    mRefreshURIList->Count(&n);

    for (uint32_t i = 0; i < n; ++i) {
      nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
      if (!timer) {
        continue; // this must be a nsRefreshURI already
      }

      // Replace this timer object with a nsRefreshTimer object.
      nsCOMPtr<nsITimerCallback> callback;
      timer->GetCallback(getter_AddRefs(callback));

      timer->Cancel();

      nsCOMPtr<nsITimerCallback> rt = do_QueryInterface(callback);
      NS_ASSERTION(rt,
                   "RefreshURIList timer callbacks should only be RefreshTimer objects");

      mRefreshURIList->ReplaceElementAt(rt, i);
    }
  }

  // Suspend refresh URIs for our child shells as well.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->SuspendRefreshURIs();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ResumeRefreshURIs()
{
  RefreshURIFromQueue();

  // Resume refresh URIs for our child shells as well.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->ResumeRefreshURIs();
    }
  }

  return NS_OK;
}

nsresult
nsDocShell::RefreshURIFromQueue()
{
  if (!mRefreshURIList) {
    return NS_OK;
  }
  uint32_t n = 0;
  mRefreshURIList->Count(&n);

  while (n) {
    nsCOMPtr<nsISupports> element;
    mRefreshURIList->GetElementAt(--n, getter_AddRefs(element));
    nsCOMPtr<nsITimerCallback> refreshInfo(do_QueryInterface(element));

    if (refreshInfo) {
      // This is the nsRefreshTimer object, waiting to be
      // setup in a timer object and fired.
      // Create the timer and  trigger it.
      uint32_t delay =
        static_cast<nsRefreshTimer*>(
          static_cast<nsITimerCallback*>(refreshInfo))->GetDelay();
      nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
      if (timer) {
        // Replace the nsRefreshTimer element in the queue with
        // its corresponding timer object, so that in case another
        // load comes through before the timer can go off, the timer will
        // get cancelled in CancelRefreshURITimer()
        mRefreshURIList->ReplaceElementAt(timer, n);
        timer->InitWithCallback(refreshInfo, delay, nsITimer::TYPE_ONE_SHOT);
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIContentViewerContainer
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::Embed(nsIContentViewer* aContentViewer,
                  const char* aCommand, nsISupports* aExtraInfo)
{
  // Save the LayoutHistoryState of the previous document, before
  // setting up new document
  PersistLayoutHistoryState();

  nsresult rv = SetupNewViewer(aContentViewer);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we are loading a wyciwyg url from history, change the base URI for
  // the document to the original http url that created the document.write().
  // This makes sure that all relative urls in a document.written page loaded
  // via history work properly.
  if (mCurrentURI &&
      (mLoadType & LOAD_CMD_HISTORY ||
       mLoadType == LOAD_RELOAD_NORMAL ||
       mLoadType == LOAD_RELOAD_CHARSET_CHANGE)) {
    bool isWyciwyg = false;
    // Check if the url is wyciwyg
    rv = mCurrentURI->SchemeIs("wyciwyg", &isWyciwyg);
    if (isWyciwyg && NS_SUCCEEDED(rv)) {
      SetBaseUrlForWyciwyg(aContentViewer);
    }
  }
  // XXX What if SetupNewViewer fails?
  if (mLSHE) {
    // Restore the editing state, if it's stored in session history.
    if (mLSHE->HasDetachedEditor()) {
      ReattachEditorToWindow(mLSHE);
    }
    // Set history.state
    SetDocCurrentStateObj(mLSHE);

    SetHistoryEntry(&mOSHE, mLSHE);
  }

  bool updateHistory = true;

  // Determine if this type of load should update history
  switch (mLoadType) {
    case LOAD_NORMAL_REPLACE:
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_REPLACE_BYPASS_CACHE:
      updateHistory = false;
      break;
    default:
      break;
  }

  if (!updateHistory) {
    SetLayoutHistoryState(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsPrinting(bool aIsPrinting)
{
  mIsPrintingOrPP = aIsPrinting;
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::OnProgressChange(nsIWebProgress* aProgress,
                             nsIRequest* aRequest,
                             int32_t aCurSelfProgress,
                             int32_t aMaxSelfProgress,
                             int32_t aCurTotalProgress,
                             int32_t aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStateChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                          uint32_t aStateFlags, nsresult aStatus)
{
  if ((~aStateFlags & (STATE_START | STATE_IS_NETWORK)) == 0) {
    // Save timing statistics.
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    nsAutoCString aURI;
    uri->GetAsciiSpec(aURI);

    nsCOMPtr<nsIWyciwygChannel> wcwgChannel(do_QueryInterface(aRequest));
    nsCOMPtr<nsIWebProgress> webProgress =
      do_QueryInterface(GetAsSupports(this));

    // We don't update navigation timing for wyciwyg channels
    if (this == aProgress && !wcwgChannel) {
      MaybeInitTiming();
      mTiming->NotifyFetchStart(uri,
                                ConvertLoadTypeToNavigationType(mLoadType));
    }

    // Was the wyciwyg document loaded on this docshell?
    if (wcwgChannel && !mLSHE && (mItemType == typeContent) &&
        aProgress == webProgress.get()) {
      bool equalUri = true;
      // Store the wyciwyg url in session history, only if it is
      // being loaded fresh for the first time. We don't want
      // multiple entries for successive loads
      if (mCurrentURI &&
          NS_SUCCEEDED(uri->Equals(mCurrentURI, &equalUri)) &&
          !equalUri) {
        nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
        GetSameTypeParent(getter_AddRefs(parentAsItem));
        nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(parentAsItem));
        bool inOnLoadHandler = false;
        if (parentDS) {
          parentDS->GetIsExecutingOnLoadHandler(&inOnLoadHandler);
        }
        if (inOnLoadHandler) {
          // We're handling parent's load event listener, which causes
          // document.write in a subdocument.
          // Need to clear the session history for all child
          // docshells so that we can handle them like they would
          // all be added dynamically.
          nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
          if (parent) {
            bool oshe = false;
            nsCOMPtr<nsISHEntry> entry;
            parent->GetCurrentSHEntry(getter_AddRefs(entry), &oshe);
            static_cast<nsDocShell*>(parent.get())->ClearFrameHistory(entry);
          }
        }

        // This is a document.write(). Get the made-up url
        // from the channel and store it in session history.
        // Pass false for aCloneChildren, since we're creating
        // a new DOM here.
        AddToSessionHistory(uri, wcwgChannel, nullptr, false,
                            getter_AddRefs(mLSHE));
        SetCurrentURI(uri, aRequest, true, 0);
        // Save history state of the previous page
        PersistLayoutHistoryState();
        // We'll never get an Embed() for this load, so just go ahead
        // and SetHistoryEntry now.
        SetHistoryEntry(&mOSHE, mLSHE);
      }
    }
    // Page has begun to load
    mBusyFlags = BUSY_FLAGS_BUSY | BUSY_FLAGS_BEFORE_PAGE_LOAD;

    if ((aStateFlags & STATE_RESTORING) == 0) {
      // Show the progress cursor if the pref is set
      if (Preferences::GetBool("ui.use_activity_cursor", false)) {
        nsCOMPtr<nsIWidget> mainWidget;
        GetMainWidget(getter_AddRefs(mainWidget));
        if (mainWidget) {
          mainWidget->SetCursor(eCursor_spinning);
        }
      }
    }
  } else if ((~aStateFlags & (STATE_TRANSFERRING | STATE_IS_DOCUMENT)) == 0) {
    // Page is loading
    mBusyFlags = BUSY_FLAGS_BUSY | BUSY_FLAGS_PAGE_LOADING;
  } else if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_NETWORK)) {
    // Page has finished loading
    mBusyFlags = BUSY_FLAGS_NONE;

    // Hide the progress cursor if the pref is set
    if (Preferences::GetBool("ui.use_activity_cursor", false)) {
      nsCOMPtr<nsIWidget> mainWidget;
      GetMainWidget(getter_AddRefs(mainWidget));
      if (mainWidget) {
        mainWidget->SetCursor(eCursor_standard);
      }
    }
  }
  if ((~aStateFlags & (STATE_IS_DOCUMENT | STATE_STOP)) == 0) {
    nsCOMPtr<nsIWebProgress> webProgress =
      do_QueryInterface(GetAsSupports(this));
    // Is the document stop notification for this document?
    if (aProgress == webProgress.get()) {
      nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
      EndPageLoad(aProgress, channel, aStatus);
    }
  }
  // note that redirect state changes will go through here as well, but it
  // is better to handle those in OnRedirectStateChange where more
  // information is available.
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnLocationChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                             nsIURI* aURI, uint32_t aFlags)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

void
nsDocShell::OnRedirectStateChange(nsIChannel* aOldChannel,
                                  nsIChannel* aNewChannel,
                                  uint32_t aRedirectFlags,
                                  uint32_t aStateFlags)
{
  NS_ASSERTION(aStateFlags & STATE_REDIRECTING,
               "Calling OnRedirectStateChange when there is no redirect");
  if (!(aStateFlags & STATE_IS_DOCUMENT)) {
    return;  // not a toplevel document
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));
  if (!oldURI || !newURI) {
    return;
  }

  if (DoAppRedirectIfNeeded(newURI, nullptr, false)) {
    return;
  }

  // Below a URI visit is saved (see AddURIVisit method doc).
  // The visit chain looks something like:
  //   ...
  //   Site N - 1
  //                =>  Site N
  //   (redirect to =>) Site N + 1 (we are here!)

  // Get N - 1 and transition type
  nsCOMPtr<nsIURI> previousURI;
  uint32_t previousFlags = 0;
  ExtractLastVisit(aOldChannel, getter_AddRefs(previousURI), &previousFlags);

  if (aRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL ||
      ChannelIsPost(aOldChannel)) {
    // 1. Internal redirects are ignored because they are specific to the
    //    channel implementation.
    // 2. POSTs are not saved by global history.
    //
    // Regardless, we need to propagate the previous visit to the new
    // channel.
    SaveLastVisit(aNewChannel, previousURI, previousFlags);
  } else {
    nsCOMPtr<nsIURI> referrer;
    // Treat referrer as null if there is an error getting it.
    (void)NS_GetReferrerFromChannel(aOldChannel, getter_AddRefs(referrer));

    // Get the HTTP response code, if available.
    uint32_t responseStatus = 0;
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aOldChannel);
    if (httpChannel) {
      (void)httpChannel->GetResponseStatus(&responseStatus);
    }

    // Add visit N -1 => N
    AddURIVisit(oldURI, referrer, previousURI, previousFlags, responseStatus);

    // Since N + 1 could be the final destination, we will not save N => N + 1
    // here.  OnNewURI will do that, so we will cache it.
    SaveLastVisit(aNewChannel, oldURI, aRedirectFlags);
  }

  // check if the new load should go through the application cache.
  nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
    do_QueryInterface(aNewChannel);
  if (appCacheChannel) {
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
      // Permission will be checked in the parent process.
      appCacheChannel->SetChooseApplicationCache(true);
    } else {
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

      if (secMan) {
        nsCOMPtr<nsIPrincipal> principal;
        secMan->GetDocShellCodebasePrincipal(newURI, this,
                                             getter_AddRefs(principal));
        appCacheChannel->SetChooseApplicationCache(
          NS_ShouldCheckAppCache(principal, mInPrivateBrowsing));
      }
    }
  }

  if (!(aRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL) &&
      mLoadType & (LOAD_CMD_RELOAD | LOAD_CMD_HISTORY)) {
    mLoadType = LOAD_NORMAL_REPLACE;
    SetHistoryEntry(&mLSHE, nullptr);
  }
}

NS_IMETHODIMP
nsDocShell::OnStatusChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           nsresult aStatus, const char16_t* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnSecurityChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest, uint32_t aState)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsresult
nsDocShell::EndPageLoad(nsIWebProgress* aProgress,
                        nsIChannel* aChannel, nsresult aStatus)
{
  if (!aChannel) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIURI> url;
  nsresult rv = aChannel->GetURI(getter_AddRefs(url));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsITimedChannel> timingChannel = do_QueryInterface(aChannel);
  if (timingChannel) {
    TimeStamp channelCreationTime;
    rv = timingChannel->GetChannelCreation(&channelCreationTime);
    if (NS_SUCCEEDED(rv) && !channelCreationTime.IsNull()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TOTAL_CONTENT_PAGE_LOAD_TIME,
                                     channelCreationTime);
      nsCOMPtr<nsPILoadGroupInternal> internalLoadGroup =
        do_QueryInterface(mLoadGroup);
      if (internalLoadGroup) {
        internalLoadGroup->OnEndPageLoad(aChannel);
      }
    }
  }

  // Timing is picked up by the window, we don't need it anymore
  mTiming = nullptr;

  // clean up reload state for meta charset
  if (eCharsetReloadRequested == mCharsetReloadState) {
    mCharsetReloadState = eCharsetReloadStopOrigional;
  } else {
    mCharsetReloadState = eCharsetReloadInit;
  }

  // Save a pointer to the currently-loading history entry.
  // nsDocShell::EndPageLoad will clear mLSHE, but we may need this history
  // entry further down in this method.
  nsCOMPtr<nsISHEntry> loadingSHE = mLSHE;

  //
  // one of many safeguards that prevent death and destruction if
  // someone is so very very rude as to bring this window down
  // during this load handler.
  //
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

  // Notify the ContentViewer that the Document has finished loading.  This
  // will cause any OnLoad(...) and PopState(...) handlers to fire.
  if (!mEODForCurrentDocument && mContentViewer) {
    mIsExecutingOnLoadHandler = true;
    mContentViewer->LoadComplete(aStatus);
    mIsExecutingOnLoadHandler = false;

    mEODForCurrentDocument = true;

    // If all documents have completed their loading
    // favor native event dispatch priorities
    // over performance
    if (--gNumberOfDocumentsLoading == 0) {
      // Hint to use normal native event dispatch priorities
      FavorPerformanceHint(false);
    }
  }
  /* Check if the httpChannel has any cache-control related response headers,
   * like no-store, no-cache. If so, update SHEntry so that
   * when a user goes back/forward to this page, we appropriately do
   * form value restoration or load from server.
   */
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (!httpChannel) {
    // HttpChannel could be hiding underneath a Multipart channel.
    GetHttpChannel(aChannel, getter_AddRefs(httpChannel));
  }

  if (httpChannel) {
    // figure out if SH should be saving layout state.
    bool discardLayoutState = ShouldDiscardLayoutState(httpChannel);
    if (mLSHE && discardLayoutState && (mLoadType & LOAD_CMD_NORMAL) &&
        (mLoadType != LOAD_BYPASS_HISTORY) && (mLoadType != LOAD_ERROR_PAGE)) {
      mLSHE->SetSaveLayoutStateFlag(false);
    }
  }

  // Clear mLSHE after calling the onLoadHandlers. This way, if the
  // onLoadHandler tries to load something different in
  // itself or one of its children, we can deal with it appropriately.
  if (mLSHE) {
    mLSHE->SetLoadType(nsIDocShellLoadInfo::loadHistory);

    // Clear the mLSHE reference to indicate document loading is done one
    // way or another.
    SetHistoryEntry(&mLSHE, nullptr);
  }
  // if there's a refresh header in the channel, this method
  // will set it up for us.
  RefreshURIFromQueue();

  // Test whether this is the top frame or a subframe
  bool isTopFrame = true;
  nsCOMPtr<nsIDocShellTreeItem> targetParentTreeItem;
  rv = GetSameTypeParent(getter_AddRefs(targetParentTreeItem));
  if (NS_SUCCEEDED(rv) && targetParentTreeItem) {
    isTopFrame = false;
  }

  //
  // If the page load failed, then deal with the error condition...
  // Errors are handled as follows:
  //   1. Check to see if it's a file not found error or bad content
  //      encoding error.
  //   2. Send the URI to a keyword server (if enabled)
  //   3. If the error was DNS failure, then add www and .com to the URI
  //      (if appropriate).
  //   4. Throw an error dialog box...
  //
  if (url && NS_FAILED(aStatus)) {
    if (aStatus == NS_ERROR_FILE_NOT_FOUND ||
        aStatus == NS_ERROR_CORRUPTED_CONTENT ||
        aStatus == NS_ERROR_INVALID_CONTENT_ENCODING) {
      DisplayLoadError(aStatus, url, nullptr, aChannel);
      return NS_OK;
    }

    // Handle iframe document not loading error because source was
    // a tracking URL. We make a note of this iframe node by including
    // it in a dedicated array of blocked tracking nodes under its parent
    // document. (document of parent window of blocked document)
    if (isTopFrame == false && aStatus == NS_ERROR_TRACKING_URI) {
      // frameElement is our nsIContent to be annotated
      nsCOMPtr<nsIDOMElement> frameElement;
      nsPIDOMWindow* thisWindow = GetWindow();
      if (!thisWindow) {
        return NS_OK;
      }

      thisWindow->GetFrameElement(getter_AddRefs(frameElement));
      if (!frameElement) {
        return NS_OK;
      }

      // Parent window
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      GetSameTypeParent(getter_AddRefs(parentItem));
      if (!parentItem) {
        return NS_OK;
      }

      nsCOMPtr<nsIDocument> parentDoc;
      parentDoc = parentItem->GetDocument();
      if (!parentDoc) {
        return NS_OK;
      }

      nsCOMPtr<nsIContent> cont = do_QueryInterface(frameElement);
      parentDoc->AddBlockedTrackingNode(cont);

      return NS_OK;
    }

    if (sURIFixup) {
      //
      // Try and make an alternative URI from the old one
      //
      nsCOMPtr<nsIURI> newURI;
      nsCOMPtr<nsIInputStream> newPostData;

      nsAutoCString oldSpec;
      url->GetSpec(oldSpec);

      //
      // First try keyword fixup
      //
      nsAutoString keywordProviderName, keywordAsSent;
      if (aStatus == NS_ERROR_UNKNOWN_HOST && mAllowKeywordFixup) {
        bool keywordsEnabled = Preferences::GetBool("keyword.enabled", false);

        nsAutoCString host;
        url->GetHost(host);

        nsAutoCString scheme;
        url->GetScheme(scheme);

        int32_t dotLoc = host.FindChar('.');

        // we should only perform a keyword search under the following
        // conditions:
        // (0) Pref keyword.enabled is true
        // (1) the url scheme is http (or https)
        // (2) the url does not have a protocol scheme
        // If we don't enforce such a policy, then we end up doing
        // keyword searchs on urls we don't intend like imap, file,
        // mailbox, etc. This could lead to a security problem where we
        // send data to the keyword server that we shouldn't be.
        // Someone needs to clean up keywords in general so we can
        // determine on a per url basis if we want keywords
        // enabled...this is just a bandaid...
        if (keywordsEnabled && !scheme.IsEmpty() &&
            (scheme.Find("http") != 0)) {
          keywordsEnabled = false;
        }

        if (keywordsEnabled && (kNotFound == dotLoc)) {
          nsCOMPtr<nsIURIFixupInfo> info;
          // only send non-qualified hosts to the keyword server
          if (!mOriginalUriString.IsEmpty()) {
            sURIFixup->KeywordToURI(mOriginalUriString,
                                    getter_AddRefs(newPostData),
                                    getter_AddRefs(info));
          } else {
            //
            // If this string was passed through nsStandardURL by
            // chance, then it may have been converted from UTF-8 to
            // ACE, which would result in a completely bogus keyword
            // query.  Here we try to recover the original Unicode
            // value, but this is not 100% correct since the value may
            // have been normalized per the IDN normalization rules.
            //
            // Since we don't have access to the exact original string
            // that was entered by the user, this will just have to do.
            bool isACE;
            nsAutoCString utf8Host;
            nsCOMPtr<nsIIDNService> idnSrv =
              do_GetService(NS_IDNSERVICE_CONTRACTID);
            if (idnSrv &&
                NS_SUCCEEDED(idnSrv->IsACE(host, &isACE)) && isACE &&
                NS_SUCCEEDED(idnSrv->ConvertACEtoUTF8(host, utf8Host))) {
              sURIFixup->KeywordToURI(utf8Host,
                                      getter_AddRefs(newPostData),
                                      getter_AddRefs(info));
            } else {
              sURIFixup->KeywordToURI(host,
                                      getter_AddRefs(newPostData),
                                      getter_AddRefs(info));
            }
          }

          info->GetPreferredURI(getter_AddRefs(newURI));
          if (newURI) {
            info->GetKeywordAsSent(keywordAsSent);
            info->GetKeywordProviderName(keywordProviderName);
          }
        } // end keywordsEnabled
      }

      //
      // Now try change the address, e.g. turn http://foo into
      // http://www.foo.com
      //
      if (aStatus == NS_ERROR_UNKNOWN_HOST ||
          aStatus == NS_ERROR_NET_RESET) {
        bool doCreateAlternate = true;

        // Skip fixup for anything except a normal document load
        // operation on the topframe.

        if (mLoadType != LOAD_NORMAL || !isTopFrame) {
          doCreateAlternate = false;
        } else {
          // Test if keyword lookup produced a new URI or not
          if (newURI) {
            bool sameURI = false;
            url->Equals(newURI, &sameURI);
            if (!sameURI) {
              // Keyword lookup made a new URI so no need to try
              // an alternate one.
              doCreateAlternate = false;
            }
          }
        }
        if (doCreateAlternate) {
          newURI = nullptr;
          newPostData = nullptr;
          keywordProviderName.Truncate();
          keywordAsSent.Truncate();
          sURIFixup->CreateFixupURI(oldSpec,
                                    nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
                                    getter_AddRefs(newPostData),
                                    getter_AddRefs(newURI));
        }
      }

      // Did we make a new URI that is different to the old one? If so
      // load it.
      //
      if (newURI) {
        // Make sure the new URI is different from the old one,
        // otherwise there's little point trying to load it again.
        bool sameURI = false;
        url->Equals(newURI, &sameURI);
        if (!sameURI) {
          nsAutoCString newSpec;
          newURI->GetSpec(newSpec);
          NS_ConvertUTF8toUTF16 newSpecW(newSpec);

          // This notification is meant for Firefox Health Report so it
          // can increment counts from the search engine
          MaybeNotifyKeywordSearchLoading(keywordProviderName, keywordAsSent);

          return LoadURI(newSpecW.get(),  // URI string
                         LOAD_FLAGS_NONE, // Load flags
                         nullptr,         // Referring URI
                         newPostData,     // Post data stream
                         nullptr);        // Headers stream
        }
      }
    }

    // Well, fixup didn't work :-(
    // It is time to throw an error dialog box, and be done with it...

    // Errors to be shown only on top-level frames
    if ((aStatus == NS_ERROR_UNKNOWN_HOST ||
         aStatus == NS_ERROR_CONNECTION_REFUSED ||
         aStatus == NS_ERROR_UNKNOWN_PROXY_HOST ||
         aStatus == NS_ERROR_PROXY_CONNECTION_REFUSED) &&
        (isTopFrame || UseErrorPages())) {
      DisplayLoadError(aStatus, url, nullptr, aChannel);
    } else if (aStatus == NS_ERROR_NET_TIMEOUT ||
               aStatus == NS_ERROR_REDIRECT_LOOP ||
               aStatus == NS_ERROR_UNKNOWN_SOCKET_TYPE ||
               aStatus == NS_ERROR_NET_INTERRUPT ||
               aStatus == NS_ERROR_NET_RESET ||
               aStatus == NS_ERROR_OFFLINE ||
               aStatus == NS_ERROR_MALWARE_URI ||
               aStatus == NS_ERROR_PHISHING_URI ||
               aStatus == NS_ERROR_UNWANTED_URI ||
               aStatus == NS_ERROR_UNSAFE_CONTENT_TYPE ||
               aStatus == NS_ERROR_REMOTE_XUL ||
               aStatus == NS_ERROR_INTERCEPTION_FAILED ||
               aStatus == NS_ERROR_OPAQUE_INTERCEPTION_DISABLED ||
               aStatus == NS_ERROR_BAD_OPAQUE_INTERCEPTION_REQUEST_MODE ||
               aStatus == NS_ERROR_INTERCEPTED_ERROR_RESPONSE ||
               aStatus == NS_ERROR_INTERCEPTED_USED_RESPONSE ||
               aStatus == NS_ERROR_CLIENT_REQUEST_OPAQUE_INTERCEPTION ||
               aStatus == NS_ERROR_INTERCEPTION_CANCELED ||
               aStatus == NS_ERROR_REJECTED_RESPONSE_INTERCEPTION ||
               NS_ERROR_GET_MODULE(aStatus) == NS_ERROR_MODULE_SECURITY) {
      // Errors to be shown for any frame
      DisplayLoadError(aStatus, url, nullptr, aChannel);
    } else if (aStatus == NS_ERROR_DOCUMENT_NOT_CACHED) {
      // Non-caching channels will simply return NS_ERROR_OFFLINE.
      // Caching channels would have to look at their flags to work
      // out which error to return. Or we can fix up the error here.
      if (!(mLoadType & LOAD_CMD_HISTORY)) {
        aStatus = NS_ERROR_OFFLINE;
      }
      DisplayLoadError(aStatus, url, nullptr, aChannel);
    }
  } else if (url && NS_SUCCEEDED(aStatus)) {
    // If we have a host
    mozilla::net::PredictorLearnRedirect(url, aChannel, this);
  }

  return NS_OK;
}

//*****************************************************************************
// nsDocShell: Content Viewer Management
//*****************************************************************************

nsresult
nsDocShell::EnsureContentViewer()
{
  if (mContentViewer) {
    return NS_OK;
  }
  if (mIsBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> baseURI;
  nsIPrincipal* principal = GetInheritedPrincipal(false);
  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  GetSameTypeParent(getter_AddRefs(parentItem));
  if (parentItem) {
    nsCOMPtr<nsPIDOMWindow> domWin = GetWindow();
    if (domWin) {
      nsCOMPtr<Element> parentElement = domWin->GetFrameElementInternal();
      if (parentElement) {
        baseURI = parentElement->GetBaseURI();
      }
    }
  }

  nsresult rv = CreateAboutBlankContentViewer(principal, baseURI);

  NS_ENSURE_STATE(mContentViewer);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> doc(GetDocument());
    NS_ASSERTION(doc,
                 "Should have doc if CreateAboutBlankContentViewer "
                 "succeeded!");

    doc->SetIsInitialDocument(true);
  }

  return rv;
}

nsresult
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal* aPrincipal,
                                          nsIURI* aBaseURI,
                                          bool aTryToSaveOldPresentation)
{
  nsCOMPtr<nsIDocument> blankDoc;
  nsCOMPtr<nsIContentViewer> viewer;
  nsresult rv = NS_ERROR_FAILURE;

  /* mCreatingDocument should never be true at this point. However, it's
     a theoretical possibility. We want to know about it and make it stop,
     and this sounds like a job for an assertion. */
  NS_ASSERTION(!mCreatingDocument,
               "infinite(?) loop creating document averted");
  if (mCreatingDocument) {
    return NS_ERROR_FAILURE;
  }

  AutoRestore<bool> creatingDocument(mCreatingDocument);
  mCreatingDocument = true;

  // mContentViewer->PermitUnload may release |this| docshell.
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

  // Make sure timing is created.  But first record whether we had it
  // already, so we don't clobber the timing for an in-progress load.
  bool hadTiming = mTiming;
  MaybeInitTiming();
  if (mContentViewer) {
    // We've got a content viewer already. Make sure the user
    // permits us to discard the current document and replace it
    // with about:blank. And also ensure we fire the unload events
    // in the current document.

    // Unload gets fired first for
    // document loaded from the session history.
    mTiming->NotifyBeforeUnload();

    bool okToUnload;
    rv = mContentViewer->PermitUnload(false, &okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the load.
      return NS_ERROR_FAILURE;
    }

    mSavingOldViewer = aTryToSaveOldPresentation &&
                       CanSavePresentation(LOAD_NORMAL, nullptr, nullptr);

    if (mTiming) {
      mTiming->NotifyUnloadAccepted(mCurrentURI);
    }

    // Make sure to blow away our mLoadingURI just in case.  No loads
    // from inside this pagehide.
    mLoadingURI = nullptr;

    // Stop any in-progress loading, so that we don't accidentally trigger any
    // PageShow notifications from Embed() interrupting our loading below.
    Stop();

    // Notify the current document that it is about to be unloaded!!
    //
    // It is important to fire the unload() notification *before* any state
    // is changed within the DocShell - otherwise, javascript will get the
    // wrong information :-(
    //
    (void)FirePageHideNotification(!mSavingOldViewer);
  }

  // Now make sure we don't think we're in the middle of firing unload after
  // this point.  This will make us fire unload when the about:blank document
  // unloads... but that's ok, more or less.  Would be nice if it fired load
  // too, of course.
  mFiredUnloadEvent = false;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory =
    nsContentUtils::FindInternalContentViewer(NS_LITERAL_CSTRING("text/html"));

  if (docFactory) {
    nsCOMPtr<nsIPrincipal> principal;
    if (mSandboxFlags & SANDBOXED_ORIGIN) {
      principal = nsNullPrincipal::CreateWithInheritedAttributes(aPrincipal);
      NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);
    } else {
      principal = aPrincipal;
    }
    // generate (about:blank) document to load
    docFactory->CreateBlankDocument(mLoadGroup, principal,
                                    getter_AddRefs(blankDoc));
    if (blankDoc) {
      // Hack: set the base URI manually, since this document never
      // got Reset() with a channel.
      blankDoc->SetBaseURI(aBaseURI);

      blankDoc->SetContainer(this);

      // Copy our sandbox flags to the document. These are immutable
      // after being set here.
      blankDoc->SetSandboxFlags(mSandboxFlags);

      // create a content viewer for us and the new document
      docFactory->CreateInstanceForDocument(
        NS_ISUPPORTS_CAST(nsIDocShell*, this), blankDoc, "view",
        getter_AddRefs(viewer));

      // hook 'em up
      if (viewer) {
        viewer->SetContainer(this);
        rv = Embed(viewer, "", 0);
        NS_ENSURE_SUCCESS(rv, rv);

        SetCurrentURI(blankDoc->GetDocumentURI(), nullptr, true, 0);
        rv = mIsBeingDestroyed ? NS_ERROR_NOT_AVAILABLE : NS_OK;
      }
    }
  }

  // The transient about:blank viewer doesn't have a session history entry.
  SetHistoryEntry(&mOSHE, nullptr);

  // Clear out our mTiming like we would in EndPageLoad, if we didn't
  // have one before entering this function.
  if (!hadTiming) {
    mTiming = nullptr;
    mBlankTiming = true;
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal* aPrincipal)
{
  return CreateAboutBlankContentViewer(aPrincipal, nullptr);
}

bool
nsDocShell::CanSavePresentation(uint32_t aLoadType,
                                nsIRequest* aNewRequest,
                                nsIDocument* aNewDocument)
{
  if (!mOSHE) {
    return false;  // no entry to save into
  }

  nsCOMPtr<nsIContentViewer> viewer;
  mOSHE->GetContentViewer(getter_AddRefs(viewer));
  if (viewer) {
    NS_WARNING("mOSHE already has a content viewer!");
    return false;
  }

  // Only save presentation for "normal" loads and link loads.  Anything else
  // probably wants to refetch the page, so caching the old presentation
  // would be incorrect.
  if (aLoadType != LOAD_NORMAL &&
      aLoadType != LOAD_HISTORY &&
      aLoadType != LOAD_LINK &&
      aLoadType != LOAD_STOP_CONTENT &&
      aLoadType != LOAD_STOP_CONTENT_AND_REPLACE &&
      aLoadType != LOAD_ERROR_PAGE) {
    return false;
  }

  // If the session history entry has the saveLayoutState flag set to false,
  // then we should not cache the presentation.
  bool canSaveState;
  mOSHE->GetSaveLayoutStateFlag(&canSaveState);
  if (!canSaveState) {
    return false;
  }

  // If the document is not done loading, don't cache it.
  if (!mScriptGlobal || mScriptGlobal->IsLoading()) {
    return false;
  }

  if (mScriptGlobal->WouldReuseInnerWindow(aNewDocument)) {
    return false;
  }

  // Avoid doing the work of saving the presentation state in the case where
  // the content viewer cache is disabled.
  if (nsSHistory::GetMaxTotalViewers() == 0) {
    return false;
  }

  // Don't cache the content viewer if we're in a subframe and the subframe
  // pref is disabled.
  bool cacheFrames =
    Preferences::GetBool("browser.sessionhistory.cache_subframes", false);
  if (!cacheFrames) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetSameTypeParent(getter_AddRefs(root));
    if (root && root != this) {
      return false;  // this is a subframe load
    }
  }

  // If the document does not want its presentation cached, then don't.
  nsCOMPtr<nsIDocument> doc = mScriptGlobal->GetExtantDoc();
  return doc && doc->CanSavePresentation(aNewRequest);
}

void
nsDocShell::ReattachEditorToWindow(nsISHEntry* aSHEntry)
{
  NS_ASSERTION(!mEditorData,
               "Why reattach an editor when we already have one?");
  NS_ASSERTION(aSHEntry && aSHEntry->HasDetachedEditor(),
               "Reattaching when there's not a detached editor.");

  if (mEditorData || !aSHEntry) {
    return;
  }

  mEditorData = aSHEntry->ForgetEditorData();
  if (mEditorData) {
#ifdef DEBUG
    nsresult rv =
#endif
      mEditorData->ReattachToWindow(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to reattach editing session");
  }
}

void
nsDocShell::DetachEditorFromWindow()
{
  if (!mEditorData || mEditorData->WaitingForLoad()) {
    // If there's nothing to detach, or if the editor data is actually set
    // up for the _new_ page that's coming in, don't detach.
    return;
  }

  NS_ASSERTION(!mOSHE || !mOSHE->HasDetachedEditor(),
               "Detaching editor when it's already detached.");

  nsresult res = mEditorData->DetachFromWindow();
  NS_ASSERTION(NS_SUCCEEDED(res), "Failed to detach editor");

  if (NS_SUCCEEDED(res)) {
    // Make mOSHE hold the owning ref to the editor data.
    if (mOSHE) {
      mOSHE->SetEditorData(mEditorData.forget());
    } else {
      mEditorData = nullptr;
    }
  }

#ifdef DEBUG
  {
    bool isEditable;
    GetEditable(&isEditable);
    NS_ASSERTION(!isEditable,
                 "Window is still editable after detaching editor.");
  }
#endif // DEBUG
}

nsresult
nsDocShell::CaptureState()
{
  if (!mOSHE || mOSHE == mLSHE) {
    // No entry to save into, or we're replacing the existing entry.
    return NS_ERROR_FAILURE;
  }

  if (!mScriptGlobal) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> windowState = mScriptGlobal->SaveWindowState();
  NS_ENSURE_TRUE(windowState, NS_ERROR_FAILURE);

#ifdef DEBUG_PAGE_CACHE
  nsCOMPtr<nsIURI> uri;
  mOSHE->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  if (uri) {
    uri->GetSpec(spec);
  }
  printf("Saving presentation into session history\n");
  printf("  SH URI: %s\n", spec.get());
#endif

  nsresult rv = mOSHE->SetWindowState(windowState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Suspend refresh URIs and save off the timer queue
  rv = mOSHE->SetRefreshURIList(mSavedRefreshURIList);
  NS_ENSURE_SUCCESS(rv, rv);

  // Capture the current content viewer bounds.
  if (mContentViewer) {
    nsIntRect bounds;
    mContentViewer->GetBounds(bounds);
    rv = mOSHE->SetViewerBounds(bounds);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Capture the docshell hierarchy.
  mOSHE->ClearChildShells();

  uint32_t childCount = mChildList.Length();
  for (uint32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childShell = do_QueryInterface(ChildAt(i));
    NS_ASSERTION(childShell, "null child shell");

    mOSHE->AddChildShell(childShell);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RestorePresentationEvent::Run()
{
  if (mDocShell && NS_FAILED(mDocShell->RestoreFromHistory())) {
    NS_WARNING("RestoreFromHistory failed");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::BeginRestore(nsIContentViewer* aContentViewer, bool aTop)
{
  nsresult rv;
  if (!aContentViewer) {
    rv = EnsureContentViewer();
    NS_ENSURE_SUCCESS(rv, rv);

    aContentViewer = mContentViewer;
  }

  // Dispatch events for restoring the presentation.  We try to simulate
  // the progress notifications loading the document would cause, so we add
  // the document's channel to the loadgroup to initiate stateChange
  // notifications.

  nsCOMPtr<nsIDOMDocument> domDoc;
  aContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    nsIChannel* channel = doc->GetChannel();
    if (channel) {
      mEODForCurrentDocument = false;
      mIsRestoringDocument = true;
      mLoadGroup->AddRequest(channel, nullptr);
      mIsRestoringDocument = false;
    }
  }

  if (!aTop) {
    // This point corresponds to us having gotten OnStartRequest or
    // STATE_START, so do the same thing that CreateContentViewer does at
    // this point to ensure that unload/pagehide events for this document
    // will fire when it's unloaded again.
    mFiredUnloadEvent = false;

    // For non-top frames, there is no notion of making sure that the
    // previous document is in the domwindow when STATE_START notifications
    // happen.  We can just call BeginRestore for all of the child shells
    // now.
    rv = BeginRestoreChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsDocShell::BeginRestoreChildren()
{
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> child = do_QueryObject(iter.GetNext());
    if (child) {
      nsresult rv = child->BeginRestore(nullptr, false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FinishRestore()
{
  // First we call finishRestore() on our children.  In the simulated load,
  // all of the child frames finish loading before the main document.

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> child = do_QueryObject(iter.GetNext());
    if (child) {
      child->FinishRestore();
    }
  }

  if (mOSHE && mOSHE->HasDetachedEditor()) {
    ReattachEditorToWindow(mOSHE);
  }

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (doc) {
    // Finally, we remove the request from the loadgroup.  This will
    // cause onStateChange(STATE_STOP) to fire, which will fire the
    // pageshow event to the chrome.

    nsIChannel* channel = doc->GetChannel();
    if (channel) {
      mIsRestoringDocument = true;
      mLoadGroup->RemoveRequest(channel, nullptr, NS_OK);
      mIsRestoringDocument = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRestoringDocument(bool* aRestoring)
{
  *aRestoring = mIsRestoringDocument;
  return NS_OK;
}

nsresult
nsDocShell::RestorePresentation(nsISHEntry* aSHEntry, bool* aRestoring)
{
  NS_ASSERTION(mLoadType & LOAD_CMD_HISTORY,
               "RestorePresentation should only be called for history loads");

  nsCOMPtr<nsIContentViewer> viewer;
  aSHEntry->GetContentViewer(getter_AddRefs(viewer));

#ifdef DEBUG_PAGE_CACHE
  nsCOMPtr<nsIURI> uri;
  aSHEntry->GetURI(getter_AddRefs(uri));

  nsAutoCString spec;
  if (uri) {
    uri->GetSpec(spec);
  }
#endif

  *aRestoring = false;

  if (!viewer) {
#ifdef DEBUG_PAGE_CACHE
    printf("no saved presentation for uri: %s\n", spec.get());
#endif
    return NS_OK;
  }

  // We need to make sure the content viewer's container is this docshell.
  // In subframe navigation, it's possible for the docshell that the
  // content viewer was originally loaded into to be replaced with a
  // different one.  We don't currently support restoring the presentation
  // in that case.

  nsCOMPtr<nsIDocShell> container;
  viewer->GetContainer(getter_AddRefs(container));
  if (!::SameCOMIdentity(container, GetAsSupports(this))) {
#ifdef DEBUG_PAGE_CACHE
    printf("No valid container, clearing presentation\n");
#endif
    aSHEntry->SetContentViewer(nullptr);
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mContentViewer != viewer, "Restoring existing presentation");

#ifdef DEBUG_PAGE_CACHE
  printf("restoring presentation from session history: %s\n", spec.get());
#endif

  SetHistoryEntry(&mLSHE, aSHEntry);

  // Post an event that will remove the request after we've returned
  // to the event loop.  This mimics the way it is called by nsIChannel
  // implementations.

  // Revoke any pending restore (just in case)
  NS_ASSERTION(!mRestorePresentationEvent.IsPending(),
               "should only have one RestorePresentationEvent");
  mRestorePresentationEvent.Revoke();

  RefPtr<RestorePresentationEvent> evt = new RestorePresentationEvent(this);
  nsresult rv = NS_DispatchToCurrentThread(evt);
  if (NS_SUCCEEDED(rv)) {
    mRestorePresentationEvent = evt.get();
    // The rest of the restore processing will happen on our event
    // callback.
    *aRestoring = true;
  }

  return rv;
}

namespace {
class MOZ_STACK_CLASS PresentationEventForgetter
{
public:
  explicit PresentationEventForgetter(
        nsRevocableEventPtr<nsDocShell::RestorePresentationEvent>&
          aRestorePresentationEvent)
    : mRestorePresentationEvent(aRestorePresentationEvent)
    , mEvent(aRestorePresentationEvent.get())
  {
  }

  ~PresentationEventForgetter()
  {
    Forget();
  }

  void Forget()
  {
    if (mRestorePresentationEvent.get() == mEvent) {
      mRestorePresentationEvent.Forget();
      mEvent = nullptr;
    }
  }

private:
  nsRevocableEventPtr<nsDocShell::RestorePresentationEvent>&
    mRestorePresentationEvent;
  RefPtr<nsDocShell::RestorePresentationEvent> mEvent;
};

} // namespace

nsresult
nsDocShell::RestoreFromHistory()
{
  MOZ_ASSERT(mRestorePresentationEvent.IsPending());
  PresentationEventForgetter forgetter(mRestorePresentationEvent);

  // This section of code follows the same ordering as CreateContentViewer.
  if (!mLSHE) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentViewer> viewer;
  mLSHE->GetContentViewer(getter_AddRefs(viewer));
  if (!viewer) {
    return NS_ERROR_FAILURE;
  }

  if (mSavingOldViewer) {
    // We determined that it was safe to cache the document presentation
    // at the time we initiated the new load.  We need to check whether
    // it's still safe to do so, since there may have been DOM mutations
    // or new requests initiated.
    nsCOMPtr<nsIDOMDocument> domDoc;
    viewer->GetDOMDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    nsIRequest* request = nullptr;
    if (doc) {
      request = doc->GetChannel();
    }
    mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
  }

  nsCOMPtr<nsIContentViewer> oldCv(mContentViewer);
  nsCOMPtr<nsIContentViewer> newCv(viewer);
  int32_t minFontSize = 0;
  float textZoom = 1.0f;
  float pageZoom = 1.0f;
  bool styleDisabled = false;
  if (oldCv && newCv) {
    oldCv->GetMinFontSize(&minFontSize);
    oldCv->GetTextZoom(&textZoom);
    oldCv->GetFullZoom(&pageZoom);
    oldCv->GetAuthorStyleDisabled(&styleDisabled);
  }

  // Protect against mLSHE going away via a load triggered from
  // pagehide or unload.
  nsCOMPtr<nsISHEntry> origLSHE = mLSHE;

  // Make sure to blow away our mLoadingURI just in case.  No loads
  // from inside this pagehide.
  mLoadingURI = nullptr;

  // Notify the old content viewer that it's being hidden.
  FirePageHideNotification(!mSavingOldViewer);

  // If mLSHE was changed as a result of the pagehide event, then
  // something else was loaded.  Don't finish restoring.
  if (mLSHE != origLSHE) {
    return NS_OK;
  }

  // Add the request to our load group.  We do this before swapping out
  // the content viewers so that consumers of STATE_START can access
  // the old document.  We only deal with the toplevel load at this time --
  // to be consistent with normal document loading, subframes cannot start
  // loading until after data arrives, which is after STATE_START completes.

  RefPtr<RestorePresentationEvent> currentPresentationRestoration =
    mRestorePresentationEvent.get();
  Stop();
  // Make sure we're still restoring the same presentation.
  // If we aren't, docshell is in process doing another load already.
  NS_ENSURE_STATE(currentPresentationRestoration ==
                  mRestorePresentationEvent.get());
  BeginRestore(viewer, true);
  NS_ENSURE_STATE(currentPresentationRestoration ==
                  mRestorePresentationEvent.get());
  forgetter.Forget();

  // Set mFiredUnloadEvent = false so that the unload handler for the
  // *new* document will fire.
  mFiredUnloadEvent = false;

  mURIResultedInDocument = true;
  nsCOMPtr<nsISHistory> rootSH;
  GetRootSessionHistory(getter_AddRefs(rootSH));
  if (rootSH) {
    nsCOMPtr<nsISHistoryInternal> hist = do_QueryInterface(rootSH);
    rootSH->GetIndex(&mPreviousTransIndex);
    hist->UpdateIndex();
    rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
    printf("Previous index: %d, Loaded index: %d\n\n", mPreviousTransIndex,
           mLoadedTransIndex);
#endif
  }

  // Rather than call Embed(), we will retrieve the viewer from the session
  // history entry and swap it in.
  // XXX can we refactor this so that we can just call Embed()?
  PersistLayoutHistoryState();
  nsresult rv;
  if (mContentViewer) {
    if (mSavingOldViewer && NS_FAILED(CaptureState())) {
      if (mOSHE) {
        mOSHE->SyncPresentationState();
      }
      mSavingOldViewer = false;
    }
  }

  mSavedRefreshURIList = nullptr;

  // In cases where we use a transient about:blank viewer between loads,
  // we never show the transient viewer, so _its_ previous viewer is never
  // unhooked from the view hierarchy.  Destroy any such previous viewer now,
  // before we grab the root view sibling, so that we don't grab a view
  // that's about to go away.

  if (mContentViewer) {
    nsCOMPtr<nsIContentViewer> previousViewer;
    mContentViewer->GetPreviousViewer(getter_AddRefs(previousViewer));
    if (previousViewer) {
      mContentViewer->SetPreviousViewer(nullptr);
      previousViewer->Destroy();
    }
  }

  // Save off the root view's parent and sibling so that we can insert the
  // new content viewer's root view at the same position.  Also save the
  // bounds of the root view's widget.

  nsView* rootViewSibling = nullptr;
  nsView* rootViewParent = nullptr;
  nsIntRect newBounds(0, 0, 0, 0);

  nsCOMPtr<nsIPresShell> oldPresShell = GetPresShell();
  if (oldPresShell) {
    nsViewManager* vm = oldPresShell->GetViewManager();
    if (vm) {
      nsView* oldRootView = vm->GetRootView();

      if (oldRootView) {
        rootViewSibling = oldRootView->GetNextSibling();
        rootViewParent = oldRootView->GetParent();

        mContentViewer->GetBounds(newBounds);
      }
    }
  }

  nsCOMPtr<nsIContent> container;
  nsCOMPtr<nsIDocument> sibling;
  if (rootViewParent && rootViewParent->GetParent()) {
    nsIFrame* frame = rootViewParent->GetParent()->GetFrame();
    container = frame ? frame->GetContent() : nullptr;
  }
  if (rootViewSibling) {
    nsIFrame* frame = rootViewSibling->GetFrame();
    sibling =
      frame ? frame->PresContext()->PresShell()->GetDocument() : nullptr;
  }

  // Transfer ownership to mContentViewer.  By ensuring that either the
  // docshell or the session history, but not both, have references to the
  // content viewer, we prevent the viewer from being torn down after
  // Destroy() is called.

  if (mContentViewer) {
    mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nullptr);
    viewer->SetPreviousViewer(mContentViewer);
  }
  if (mOSHE && (!mContentViewer || !mSavingOldViewer)) {
    // We don't plan to save a viewer in mOSHE; tell it to drop
    // any other state it's holding.
    mOSHE->SyncPresentationState();
  }

  // Order the mContentViewer setup just like Embed does.
  mContentViewer = nullptr;

  // Now that we're about to switch documents, forget all of our children.
  // Note that we cached them as needed up in CaptureState above.
  DestroyChildren();

  mContentViewer.swap(viewer);

  // Grab all of the related presentation from the SHEntry now.
  // Clearing the viewer from the SHEntry will clear all of this state.
  nsCOMPtr<nsISupports> windowState;
  mLSHE->GetWindowState(getter_AddRefs(windowState));
  mLSHE->SetWindowState(nullptr);

  bool sticky;
  mLSHE->GetSticky(&sticky);

  nsCOMPtr<nsIDOMDocument> domDoc;
  mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));

  nsCOMArray<nsIDocShellTreeItem> childShells;
  int32_t i = 0;
  nsCOMPtr<nsIDocShellTreeItem> child;
  while (NS_SUCCEEDED(mLSHE->ChildShellAt(i++, getter_AddRefs(child))) &&
         child) {
    childShells.AppendObject(child);
  }

  // get the previous content viewer size
  nsIntRect oldBounds(0, 0, 0, 0);
  mLSHE->GetViewerBounds(oldBounds);

  // Restore the refresh URI list.  The refresh timers will be restarted
  // when EndPageLoad() is called.
  nsCOMPtr<nsISupportsArray> refreshURIList;
  mLSHE->GetRefreshURIList(getter_AddRefs(refreshURIList));

  // Reattach to the window object.
  mIsRestoringDocument = true; // for MediaDocument::BecomeInteractive
  rv = mContentViewer->Open(windowState, mLSHE);
  mIsRestoringDocument = false;

  // Hack to keep nsDocShellEditorData alive across the
  // SetContentViewer(nullptr) call below.
  nsAutoPtr<nsDocShellEditorData> data(mLSHE->ForgetEditorData());

  // Now remove it from the cached presentation.
  mLSHE->SetContentViewer(nullptr);
  mEODForCurrentDocument = false;

  mLSHE->SetEditorData(data.forget());

#ifdef DEBUG
  {
    nsCOMPtr<nsISupportsArray> refreshURIs;
    mLSHE->GetRefreshURIList(getter_AddRefs(refreshURIs));
    nsCOMPtr<nsIDocShellTreeItem> childShell;
    mLSHE->ChildShellAt(0, getter_AddRefs(childShell));
    NS_ASSERTION(!refreshURIs && !childShell,
                 "SHEntry should have cleared presentation state");
  }
#endif

  // Restore the sticky state of the viewer.  The viewer has set this state
  // on the history entry in Destroy() just before marking itself non-sticky,
  // to avoid teardown of the presentation.
  mContentViewer->SetSticky(sticky);

  NS_ENSURE_SUCCESS(rv, rv);

  // mLSHE is now our currently-loaded document.
  SetHistoryEntry(&mOSHE, mLSHE);

  // XXX special wyciwyg handling in Embed()?

  // We aren't going to restore any items from the LayoutHistoryState,
  // but we don't want them to stay around in case the page is reloaded.
  SetLayoutHistoryState(nullptr);

  // This is the end of our Embed() replacement

  mSavingOldViewer = false;
  mEODForCurrentDocument = false;

  // Tell the event loop to favor plevents over user events, see comments
  // in CreateContentViewer.
  if (++gNumberOfDocumentsLoading == 1) {
    FavorPerformanceHint(true);
  }

  if (oldCv && newCv) {
    newCv->SetMinFontSize(minFontSize);
    newCv->SetTextZoom(textZoom);
    newCv->SetFullZoom(pageZoom);
    newCv->SetAuthorStyleDisabled(styleDisabled);
  }

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
  uint32_t parentSuspendCount = 0;
  if (document) {
    RefPtr<nsDocShell> parent = GetParentDocshell();
    if (parent) {
      nsCOMPtr<nsIDocument> d = parent->GetDocument();
      if (d) {
        if (d->EventHandlingSuppressed()) {
          document->SuppressEventHandling(nsIDocument::eEvents,
                                          d->EventHandlingSuppressed());
        }

        // Ick, it'd be nicer to not rewalk all of the subdocs here.
        if (d->AnimationsPaused()) {
          document->SuppressEventHandling(nsIDocument::eAnimationsOnly,
                                          d->AnimationsPaused());
        }

        nsCOMPtr<nsPIDOMWindow> parentWindow = d->GetWindow();
        if (parentWindow) {
          parentSuspendCount = parentWindow->TimeoutSuspendCount();
        }
      }
    }

    // Use the uri from the mLSHE we had when we entered this function
    // (which need not match the document's URI if anchors are involved),
    // since that's the history entry we're loading.  Note that if we use
    // origLSHE we don't have to worry about whether the entry in question
    // is still mLSHE or whether it's now mOSHE.
    nsCOMPtr<nsIURI> uri;
    origLSHE->GetURI(getter_AddRefs(uri));
    SetCurrentURI(uri, document->GetChannel(), true, 0);
  }

  // This is the end of our CreateContentViewer() replacement.
  // Now we simulate a load.  First, we restore the state of the javascript
  // window object.
  nsCOMPtr<nsPIDOMWindow> privWin = GetWindow();
  NS_ASSERTION(privWin, "could not get nsPIDOMWindow interface");

  rv = privWin->RestoreWindowState(windowState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, dispatch a title change event which would happen as the
  // <head> is parsed.
  document->NotifyPossibleTitleChange(false);

  // Now we simulate appending child docshells for subframes.
  for (i = 0; i < childShells.Count(); ++i) {
    nsIDocShellTreeItem* childItem = childShells.ObjectAt(i);
    nsCOMPtr<nsIDocShell> childShell = do_QueryInterface(childItem);

    // Make sure to not clobber the state of the child.  Since AddChild
    // always clobbers it, save it off first.
    bool allowPlugins;
    childShell->GetAllowPlugins(&allowPlugins);

    bool allowJavascript;
    childShell->GetAllowJavascript(&allowJavascript);

    bool allowRedirects;
    childShell->GetAllowMetaRedirects(&allowRedirects);

    bool allowSubframes;
    childShell->GetAllowSubframes(&allowSubframes);

    bool allowImages;
    childShell->GetAllowImages(&allowImages);

    bool allowMedia = childShell->GetAllowMedia();

    bool allowDNSPrefetch;
    childShell->GetAllowDNSPrefetch(&allowDNSPrefetch);

    bool allowContentRetargeting = childShell->GetAllowContentRetargeting();
    bool allowContentRetargetingOnChildren =
      childShell->GetAllowContentRetargetingOnChildren();

    uint32_t defaultLoadFlags;
    childShell->GetDefaultLoadFlags(&defaultLoadFlags);

    // this.AddChild(child) calls child.SetDocLoaderParent(this), meaning that
    // the child inherits our state. Among other things, this means that the
    // child inherits our mIsActive, mIsPrerendered and mInPrivateBrowsing,
    // which is what we want.
    AddChild(childItem);

    childShell->SetAllowPlugins(allowPlugins);
    childShell->SetAllowJavascript(allowJavascript);
    childShell->SetAllowMetaRedirects(allowRedirects);
    childShell->SetAllowSubframes(allowSubframes);
    childShell->SetAllowImages(allowImages);
    childShell->SetAllowMedia(allowMedia);
    childShell->SetAllowDNSPrefetch(allowDNSPrefetch);
    childShell->SetAllowContentRetargeting(allowContentRetargeting);
    childShell->SetAllowContentRetargetingOnChildren(
      allowContentRetargetingOnChildren);
    childShell->SetDefaultLoadFlags(defaultLoadFlags);

    rv = childShell->BeginRestore(nullptr, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell();

  // We may be displayed on a different monitor (or in a different
  // HiDPI mode) than when we got into the history list.  So we need
  // to check if this has happened. See bug 838239.

  // Because the prescontext normally handles resolution changes via
  // a runnable (see nsPresContext::UIResolutionChanged), its device
  // context won't be -immediately- updated as a result of calling
  // shell->BackingScaleFactorChanged().

  // But we depend on that device context when adjusting the view size
  // via mContentViewer->SetBounds(newBounds) below. So we need to
  // explicitly tell it to check for changed resolution here.
  if (shell && shell->GetPresContext()->DeviceContext()->CheckDPIChange()) {
    shell->BackingScaleFactorChanged();
  }

  nsViewManager* newVM = shell ? shell->GetViewManager() : nullptr;
  nsView* newRootView = newVM ? newVM->GetRootView() : nullptr;

  // Insert the new root view at the correct location in the view tree.
  if (container) {
    nsSubDocumentFrame* subDocFrame =
      do_QueryFrame(container->GetPrimaryFrame());
    rootViewParent = subDocFrame ? subDocFrame->EnsureInnerView() : nullptr;
  }
  if (sibling &&
      sibling->GetShell() &&
      sibling->GetShell()->GetViewManager()) {
    rootViewSibling = sibling->GetShell()->GetViewManager()->GetRootView();
  } else {
    rootViewSibling = nullptr;
  }
  if (rootViewParent && newRootView &&
      newRootView->GetParent() != rootViewParent) {
    nsViewManager* parentVM = rootViewParent->GetViewManager();
    if (parentVM) {
      // InsertChild(parent, child, sib, true) inserts the child after
      // sib in content order, which is before sib in view order. BUT
      // when sib is null it inserts at the end of the the document
      // order, i.e., first in view order.  But when oldRootSibling is
      // null, the old root as at the end of the view list --- last in
      // content order --- and we want to call InsertChild(parent, child,
      // nullptr, false) in that case.
      parentVM->InsertChild(rootViewParent, newRootView,
                            rootViewSibling,
                            rootViewSibling ? true : false);

      NS_ASSERTION(newRootView->GetNextSibling() == rootViewSibling,
                   "error in InsertChild");
    }
  }

  // If parent is suspended, increase suspension count.
  // This can't be done as early as event suppression since this
  // depends on docshell tree.
  if (parentSuspendCount) {
    privWin->SuspendTimeouts(parentSuspendCount, false);
  }

  // Now that all of the child docshells have been put into place, we can
  // restart the timers for the window and all of the child frames.
  privWin->ResumeTimeouts();

  // Restore the refresh URI list.  The refresh timers will be restarted
  // when EndPageLoad() is called.
  mRefreshURIList = refreshURIList;

  // Meta-refresh timers have been restarted for this shell, but not
  // for our children.  Walk the child shells and restart their timers.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> child = do_QueryObject(iter.GetNext());
    if (child) {
      child->ResumeRefreshURIs();
    }
  }

  // Make sure this presentation is the same size as the previous
  // presentation.  If this is not the same size we showed it at last time,
  // then we need to resize the widget.

  // XXXbryner   This interacts poorly with Firefox's infobar.  If the old
  // presentation had the infobar visible, then we will resize the new
  // presentation to that smaller size.  However, firing the locationchanged
  // event will hide the infobar, which will immediately resize the window
  // back to the larger size.  A future optimization might be to restore
  // the presentation at the "wrong" size, then fire the locationchanged
  // event and check whether the docshell's new size is the same as the
  // cached viewer size (skipping the resize if they are equal).

  if (newRootView) {
    if (!newBounds.IsEmpty() && !newBounds.IsEqualEdges(oldBounds)) {
#ifdef DEBUG_PAGE_CACHE
      printf("resize widget(%d, %d, %d, %d)\n", newBounds.x,
             newBounds.y, newBounds.width, newBounds.height);
#endif
      mContentViewer->SetBounds(newBounds);
    } else {
      nsIScrollableFrame* rootScrollFrame =
        shell->GetRootScrollFrameAsScrollableExternal();
      if (rootScrollFrame) {
        rootScrollFrame->PostScrolledAreaEventForCurrentArea();
      }
    }
  }

  // The FinishRestore call below can kill these, null them out so we don't
  // have invalid pointer lying around.
  newRootView = rootViewSibling = rootViewParent = nullptr;
  newVM = nullptr;

  // Simulate the completion of the load.
  nsDocShell::FinishRestore();

  // Restart plugins, and paint the content.
  if (shell) {
    shell->Thaw();
  }

  return privWin->FireDelayedDOMEvents();
}

nsresult
nsDocShell::CreateContentViewer(const nsACString& aContentType,
                                nsIRequest* aRequest,
                                nsIStreamListener** aContentHandler)
{
  *aContentHandler = nullptr;

  // Can we check the content type of the current content viewer
  // and reuse it without destroying it and re-creating it?

  NS_ASSERTION(mLoadGroup, "Someone ignored return from Init()?");

  // Instantiate the content viewer object
  nsCOMPtr<nsIContentViewer> viewer;
  nsresult rv = NewContentViewerObj(aContentType, aRequest, mLoadGroup,
                                    aContentHandler, getter_AddRefs(viewer));

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Notify the current document that it is about to be unloaded!!
  //
  // It is important to fire the unload() notification *before* any state
  // is changed within the DocShell - otherwise, javascript will get the
  // wrong information :-(
  //

  if (mSavingOldViewer) {
    // We determined that it was safe to cache the document presentation
    // at the time we initiated the new load.  We need to check whether
    // it's still safe to do so, since there may have been DOM mutations
    // or new requests initiated.
    nsCOMPtr<nsIDOMDocument> domDoc;
    viewer->GetDOMDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    mSavingOldViewer = CanSavePresentation(mLoadType, aRequest, doc);
  }

  NS_ASSERTION(!mLoadingURI, "Re-entering unload?");

  nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(aRequest);
  if (aOpenedChannel) {
    aOpenedChannel->GetURI(getter_AddRefs(mLoadingURI));
  }
  FirePageHideNotification(!mSavingOldViewer);
  mLoadingURI = nullptr;

  // Set mFiredUnloadEvent = false so that the unload handler for the
  // *new* document will fire.
  mFiredUnloadEvent = false;

  // we've created a new document so go ahead and call
  // OnLoadingSite(), but don't fire OnLocationChange()
  // notifications before we've called Embed(). See bug 284993.
  mURIResultedInDocument = true;

  if (mLoadType == LOAD_ERROR_PAGE) {
    // We need to set the SH entry and our current URI here and not
    // at the moment we load the page. We want the same behavior
    // of Stop() as for a normal page load. See bug 514232 for details.

    // Revert mLoadType to load type to state the page load failed,
    // following function calls need it.
    mLoadType = mFailedLoadType;

    nsCOMPtr<nsIChannel> failedChannel = mFailedChannel;

    nsIDocument* doc = viewer->GetDocument();
    if (doc) {
      doc->SetFailedChannel(failedChannel);
    }

    // Make sure we have a URI to set currentURI.
    nsCOMPtr<nsIURI> failedURI;
    if (failedChannel) {
      NS_GetFinalChannelURI(failedChannel, getter_AddRefs(failedURI));
    }

    if (!failedURI) {
      failedURI = mFailedURI;
    }
    if (!failedURI) {
      // We need a URI object to store a session history entry, so make up a URI
      NS_NewURI(getter_AddRefs(failedURI), "about:blank");
    }

    // When we don't have failedURI, something wrong will happen. See
    // bug 291876.
    MOZ_ASSERT(failedURI, "We don't have a URI for history APIs.");

    mFailedChannel = nullptr;
    mFailedURI = nullptr;

    // Create an shistory entry for the old load.
    if (failedURI) {
      bool errorOnLocationChangeNeeded = OnNewURI(
        failedURI, failedChannel, nullptr, mLoadType, false, false, false);

      if (errorOnLocationChangeNeeded) {
        FireOnLocationChange(this, failedChannel, failedURI,
                             LOCATION_CHANGE_ERROR_PAGE);
      }
    }

    // Be sure to have a correct mLSHE, it may have been cleared by
    // EndPageLoad. See bug 302115.
    if (mSessionHistory && !mLSHE) {
      int32_t idx;
      mSessionHistory->GetRequestedIndex(&idx);
      if (idx == -1) {
        mSessionHistory->GetIndex(&idx);
      }
      mSessionHistory->GetEntryAtIndex(idx, false, getter_AddRefs(mLSHE));
    }

    mLoadType = LOAD_ERROR_PAGE;
  }

  bool onLocationChangeNeeded = OnLoadingSite(aOpenedChannel, false);

  // let's try resetting the load group if we need to...
  nsCOMPtr<nsILoadGroup> currentLoadGroup;
  NS_ENSURE_SUCCESS(
    aOpenedChannel->GetLoadGroup(getter_AddRefs(currentLoadGroup)),
    NS_ERROR_FAILURE);

  if (currentLoadGroup != mLoadGroup) {
    nsLoadFlags loadFlags = 0;

    // Cancel any URIs that are currently loading...
    // XXX: Need to do this eventually      Stop();
    //
    // Retarget the document to this loadgroup...
    //
    /* First attach the channel to the right loadgroup
     * and then remove from the old loadgroup. This
     * puts the notifications in the right order and
     * we don't null-out mLSHE in OnStateChange() for
     * all redirected urls
     */
    aOpenedChannel->SetLoadGroup(mLoadGroup);

    // Mark the channel as being a document URI...
    aOpenedChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI;

    aOpenedChannel->SetLoadFlags(loadFlags);

    mLoadGroup->AddRequest(aRequest, nullptr);
    if (currentLoadGroup) {
      currentLoadGroup->RemoveRequest(aRequest, nullptr, NS_BINDING_RETARGETED);
    }

    // Update the notification callbacks, so that progress and
    // status information are sent to the right docshell...
    aOpenedChannel->SetNotificationCallbacks(this);
  }

  NS_ENSURE_SUCCESS(Embed(viewer, "", nullptr), NS_ERROR_FAILURE);

  mSavedRefreshURIList = nullptr;
  mSavingOldViewer = false;
  mEODForCurrentDocument = false;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(aRequest));
  if (multiPartChannel) {
    nsCOMPtr<nsIPresShell> shell = GetPresShell();
    if (NS_SUCCEEDED(rv) && shell) {
      nsIDocument* doc = shell->GetDocument();
      if (doc) {
        uint32_t partID;
        multiPartChannel->GetPartID(&partID);
        doc->SetPartID(partID);
      }
    }
  }

  // Give hint to native plevent dispatch mechanism. If a document
  // is loading the native plevent dispatch mechanism should favor
  // performance over normal native event dispatch priorities.
  if (++gNumberOfDocumentsLoading == 1) {
    // Hint to favor performance for the plevent notification mechanism.
    // We want the pages to load as fast as possible even if its means
    // native messages might be starved.
    FavorPerformanceHint(true);
  }

  if (onLocationChangeNeeded) {
    FireOnLocationChange(this, aRequest, mCurrentURI, 0);
  }

  return NS_OK;
}

nsresult
nsDocShell::NewContentViewerObj(const nsACString& aContentType,
                                nsIRequest* aRequest, nsILoadGroup* aLoadGroup,
                                nsIStreamListener** aContentHandler,
                                nsIContentViewer** aViewer)
{
  nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(aRequest);

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    nsContentUtils::FindInternalContentViewer(aContentType);
  if (!docLoaderFactory) {
    return NS_ERROR_FAILURE;
  }

  // Now create an instance of the content viewer nsLayoutDLF makes the
  // determination if it should be a "view-source" instead of "view"
  nsresult rv = docLoaderFactory->CreateInstance("view",
                                                 aOpenedChannel,
                                                 aLoadGroup, aContentType,
                                                 this,
                                                 nullptr,
                                                 aContentHandler,
                                                 aViewer);
  NS_ENSURE_SUCCESS(rv, rv);

  (*aViewer)->SetContainer(this);
  return NS_OK;
}

nsresult
nsDocShell::SetupNewViewer(nsIContentViewer* aNewViewer)
{
  //
  // Copy content viewer state from previous or parent content viewer.
  //
  // The following logic is mirrored in nsHTMLDocument::StartDocumentLoad!
  //
  // Do NOT to maintain a reference to the old content viewer outside
  // of this "copying" block, or it will not be destroyed until the end of
  // this routine and all <SCRIPT>s and event handlers fail! (bug 20315)
  //
  // In this block of code, if we get an error result, we return it
  // but if we get a null pointer, that's perfectly legal for parent
  // and parentContentViewer.
  //

  int32_t x = 0;
  int32_t y = 0;
  int32_t cx = 0;
  int32_t cy = 0;

  // This will get the size from the current content viewer or from the
  // Init settings
  DoGetPositionAndSize(&x, &y, &cx, &cy);

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parentAsItem)),
                    NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));

  nsAutoCString forceCharset;
  nsAutoCString hintCharset;
  int32_t hintCharsetSource;
  int32_t minFontSize;
  float textZoom;
  float pageZoom;
  bool styleDisabled;
  // |newMUDV| also serves as a flag to set the data from the above vars
  nsCOMPtr<nsIContentViewer> newCv;

  if (mContentViewer || parent) {
    nsCOMPtr<nsIContentViewer> oldCv;
    if (mContentViewer) {
      // Get any interesting state from old content viewer
      // XXX: it would be far better to just reuse the document viewer ,
      //      since we know we're just displaying the same document as before
      oldCv = mContentViewer;

      // Tell the old content viewer to hibernate in session history when
      // it is destroyed.

      if (mSavingOldViewer && NS_FAILED(CaptureState())) {
        if (mOSHE) {
          mOSHE->SyncPresentationState();
        }
        mSavingOldViewer = false;
      }
    } else {
      // No old content viewer, so get state from parent's content viewer
      parent->GetContentViewer(getter_AddRefs(oldCv));
    }

    if (oldCv) {
      newCv = aNewViewer;
      if (newCv) {
        NS_ENSURE_SUCCESS(oldCv->GetForceCharacterSet(forceCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetHintCharacterSet(hintCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetHintCharacterSetSource(&hintCharsetSource),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetMinFontSize(&minFontSize),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetTextZoom(&textZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetFullZoom(&pageZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetAuthorStyleDisabled(&styleDisabled),
                          NS_ERROR_FAILURE);
      }
    }
  }

  nscolor bgcolor = NS_RGBA(0, 0, 0, 0);
  // Ensure that the content viewer is destroyed *after* the GC - bug 71515
  nsCOMPtr<nsIContentViewer> kungfuDeathGrip = mContentViewer;
  if (mContentViewer) {
    // Stop any activity that may be happening in the old document before
    // releasing it...
    mContentViewer->Stop();

    // Try to extract the canvas background color from the old
    // presentation shell, so we can use it for the next document.
    nsCOMPtr<nsIPresShell> shell;
    mContentViewer->GetPresShell(getter_AddRefs(shell));

    if (shell) {
      bgcolor = shell->GetCanvasBackground();
    }

    mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nullptr);
    aNewViewer->SetPreviousViewer(mContentViewer);
  }
  if (mOSHE && (!mContentViewer || !mSavingOldViewer)) {
    // We don't plan to save a viewer in mOSHE; tell it to drop
    // any other state it's holding.
    mOSHE->SyncPresentationState();
  }

  mContentViewer = nullptr;

  // Now that we're about to switch documents, forget all of our children.
  // Note that we cached them as needed up in CaptureState above.
  DestroyChildren();

  mContentViewer = aNewViewer;

  nsCOMPtr<nsIWidget> widget;
  NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);

  nsIntRect bounds(x, y, cx, cy);

  mContentViewer->SetNavigationTiming(mTiming);

  if (NS_FAILED(mContentViewer->Init(widget, bounds))) {
    mContentViewer = nullptr;
    NS_WARNING("ContentViewer Initialization failed");
    return NS_ERROR_FAILURE;
  }

  // If we have old state to copy, set the old state onto the new content
  // viewer
  if (newCv) {
    NS_ENSURE_SUCCESS(newCv->SetForceCharacterSet(forceCharset),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetHintCharacterSet(hintCharset),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetHintCharacterSetSource(hintCharsetSource),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetMinFontSize(minFontSize),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetTextZoom(textZoom),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetFullZoom(pageZoom),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetAuthorStyleDisabled(styleDisabled),
                      NS_ERROR_FAILURE);
  }

  // Stuff the bgcolor from the old pres shell into the new
  // pres shell. This improves page load continuity.
  nsCOMPtr<nsIPresShell> shell;
  mContentViewer->GetPresShell(getter_AddRefs(shell));

  if (shell) {
    shell->SetCanvasBackground(bgcolor);
  }

  // XXX: It looks like the LayoutState gets restored again in Embed()
  //      right after the call to SetupNewViewer(...)

  // We don't show the mContentViewer yet, since we want to draw the old page
  // until we have enough of the new page to show.  Just return with the new
  // viewer still set to hidden.

  return NS_OK;
}

nsresult
nsDocShell::SetDocCurrentStateObj(nsISHEntry* aShEntry)
{
  NS_ENSURE_STATE(mContentViewer);
  nsCOMPtr<nsIDocument> document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIStructuredCloneContainer> scContainer;
  if (aShEntry) {
    nsresult rv = aShEntry->GetStateData(getter_AddRefs(scContainer));
    NS_ENSURE_SUCCESS(rv, rv);

    // If aShEntry is null, just set the document's state object to null.
  }

  // It's OK for scContainer too be null here; that just means there's no
  // state data associated with this history entry.
  document->SetStateObject(scContainer);

  return NS_OK;
}

nsresult
nsDocShell::CheckLoadingPermissions()
{
  // This method checks whether the caller may load content into
  // this docshell. Even though we've done our best to hide windows
  // from code that doesn't have the right to access them, it's
  // still possible for an evil site to open a window and access
  // frames in the new window through window.frames[] (which is
  // allAccess for historic reasons), so we still need to do this
  // check on load.
  nsresult rv = NS_OK;

  if (!gValidateOrigin || !IsFrame()) {
    // Origin validation was turned off, or we're not a frame.
    // Permit all loads.

    return rv;
  }

  // Note - The check for a current JSContext here isn't necessarily sensical.
  // It's just designed to preserve the old semantics during a mass-conversion
  // patch.
  if (!nsContentUtils::GetCurrentJSContext()) {
    return NS_OK;
  }

  // Check if the caller is from the same origin as this docshell,
  // or any of its ancestors.
  nsCOMPtr<nsIDocShellTreeItem> item(this);
  do {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_GetInterface(item);
    nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(sgo));

    nsIPrincipal* p;
    if (!sop || !(p = sop->GetPrincipal())) {
      return NS_ERROR_UNEXPECTED;
    }

    if (nsContentUtils::SubjectPrincipal()->Subsumes(p)) {
      // Same origin, permit load
      return NS_OK;
    }

    nsCOMPtr<nsIDocShellTreeItem> tmp;
    item->GetSameTypeParent(getter_AddRefs(tmp));
    item.swap(tmp);
  } while (item);

  return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************

namespace {

#ifdef MOZ_PLACES
// Callback used by CopyFavicon to inform the favicon service that one URI
// (mNewURI) has the same favicon URI (OnComplete's aFaviconURI) as another.
class nsCopyFaviconCallback final : public nsIFaviconDataCallback
{
public:
  NS_DECL_ISUPPORTS

  nsCopyFaviconCallback(mozIAsyncFavicons* aSvc,
                        nsIURI* aNewURI,
                        bool aInPrivateBrowsing)
    : mSvc(aSvc)
    , mNewURI(aNewURI)
    , mInPrivateBrowsing(aInPrivateBrowsing)
  {
  }

  NS_IMETHODIMP
  OnComplete(nsIURI* aFaviconURI, uint32_t aDataLen,
             const uint8_t* aData, const nsACString& aMimeType) override
  {
    // Continue only if there is an associated favicon.
    if (!aFaviconURI) {
      return NS_OK;
    }

    MOZ_ASSERT(aDataLen == 0,
               "We weren't expecting the callback to deliver data.");

    return mSvc->SetAndFetchFaviconForPage(
      mNewURI, aFaviconURI, false,
      mInPrivateBrowsing ? nsIFaviconService::FAVICON_LOAD_PRIVATE :
                           nsIFaviconService::FAVICON_LOAD_NON_PRIVATE,
      nullptr);
  }

private:
  ~nsCopyFaviconCallback() {}

  nsCOMPtr<mozIAsyncFavicons> mSvc;
  nsCOMPtr<nsIURI> mNewURI;
  bool mInPrivateBrowsing;
};

NS_IMPL_ISUPPORTS(nsCopyFaviconCallback, nsIFaviconDataCallback)
#endif

} // namespace

void
nsDocShell::CopyFavicon(nsIURI* aOldURI,
                        nsIURI* aNewURI,
                        bool aInPrivateBrowsing)
{
  if (XRE_IsContentProcess()) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (contentChild) {
      mozilla::ipc::URIParams oldURI, newURI;
      SerializeURI(aOldURI, oldURI);
      SerializeURI(aNewURI, newURI);
      contentChild->SendCopyFavicon(oldURI, newURI, aInPrivateBrowsing);
    }
    return;
  }

#ifdef MOZ_PLACES
  nsCOMPtr<mozIAsyncFavicons> favSvc =
    do_GetService("@mozilla.org/browser/favicon-service;1");
  if (favSvc) {
    nsCOMPtr<nsIFaviconDataCallback> callback =
      new nsCopyFaviconCallback(favSvc, aNewURI, aInPrivateBrowsing);
    favSvc->GetFaviconURLForPage(aOldURI, callback);
  }
#endif
}

class InternalLoadEvent : public nsRunnable
{
public:
  InternalLoadEvent(nsDocShell* aDocShell, nsIURI* aURI,
                    nsIURI* aOriginalURI, bool aLoadReplace,
                    nsIURI* aReferrer, uint32_t aReferrerPolicy,
                    nsISupports* aOwner, uint32_t aFlags,
                    const char* aTypeHint, nsIInputStream* aPostData,
                    nsIInputStream* aHeadersData, uint32_t aLoadType,
                    nsISHEntry* aSHEntry, bool aFirstParty,
                    const nsAString& aSrcdoc, nsIDocShell* aSourceDocShell,
                    nsIURI* aBaseURI)
    : mSrcdoc(aSrcdoc)
    , mDocShell(aDocShell)
    , mURI(aURI)
    , mOriginalURI(aOriginalURI)
    , mLoadReplace(aLoadReplace)
    , mReferrer(aReferrer)
    , mReferrerPolicy(aReferrerPolicy)
    , mOwner(aOwner)
    , mPostData(aPostData)
    , mHeadersData(aHeadersData)
    , mSHEntry(aSHEntry)
    , mFlags(aFlags)
    , mLoadType(aLoadType)
    , mFirstParty(aFirstParty)
    , mSourceDocShell(aSourceDocShell)
    , mBaseURI(aBaseURI)
  {
    // Make sure to keep null things null as needed
    if (aTypeHint) {
      mTypeHint = aTypeHint;
    }
  }

  NS_IMETHOD
  Run()
  {
    return mDocShell->InternalLoad(mURI, mOriginalURI,
                                   mLoadReplace,
                                   mReferrer,
                                   mReferrerPolicy,
                                   mOwner, mFlags,
                                   nullptr, mTypeHint.get(),
                                   NullString(), mPostData, mHeadersData,
                                   mLoadType, mSHEntry, mFirstParty,
                                   mSrcdoc, mSourceDocShell, mBaseURI,
                                   nullptr, nullptr);
  }

private:
  // Use IDL strings so .get() returns null by default
  nsXPIDLString mWindowTarget;
  nsXPIDLCString mTypeHint;
  nsString mSrcdoc;

  RefPtr<nsDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  bool mLoadReplace;
  nsCOMPtr<nsIURI> mReferrer;
  uint32_t mReferrerPolicy;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIInputStream> mPostData;
  nsCOMPtr<nsIInputStream> mHeadersData;
  nsCOMPtr<nsISHEntry> mSHEntry;
  uint32_t mFlags;
  uint32_t mLoadType;
  bool mFirstParty;
  nsCOMPtr<nsIDocShell> mSourceDocShell;
  nsCOMPtr<nsIURI> mBaseURI;
};

/**
 * Returns true if we started an asynchronous load (i.e., from the network), but
 * the document we're loading there hasn't yet become this docshell's active
 * document.
 *
 * When JustStartedNetworkLoad is true, you should be careful about modifying
 * mLoadType and mLSHE.  These are both set when the asynchronous load first
 * starts, and the load expects that, when it eventually runs InternalLoad,
 * mLoadType and mLSHE will have their original values.
 */
bool
nsDocShell::JustStartedNetworkLoad()
{
  return mDocumentRequest && mDocumentRequest != GetCurrentDocChannel();
}

nsresult
nsDocShell::CreatePrincipalFromReferrer(nsIURI* aReferrer,
                                        nsIPrincipal** aResult)
{
  OriginAttributes attrs = GetOriginAttributes();
  nsCOMPtr<nsIPrincipal> prin =
    BasePrincipal::CreateCodebasePrincipal(aReferrer, attrs);
  prin.forget(aResult);

  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::InternalLoad(nsIURI* aURI,
                         nsIURI* aOriginalURI,
                         bool aLoadReplace,
                         nsIURI* aReferrer,
                         uint32_t aReferrerPolicy,
                         nsISupports* aOwner,
                         uint32_t aFlags,
                         const char16_t* aWindowTarget,
                         const char* aTypeHint,
                         const nsAString& aFileName,
                         nsIInputStream* aPostData,
                         nsIInputStream* aHeadersData,
                         uint32_t aLoadType,
                         nsISHEntry* aSHEntry,
                         bool aFirstParty,
                         const nsAString& aSrcdoc,
                         nsIDocShell* aSourceDocShell,
                         nsIURI* aBaseURI,
                         nsIDocShell** aDocShell,
                         nsIRequest** aRequest)
{
  nsresult rv = NS_OK;
  mOriginalUriString.Truncate();

  if (gDocShellLeakLog && MOZ_LOG_TEST(gDocShellLeakLog, LogLevel::Debug)) {
    nsAutoCString spec;
    if (aURI) {
      aURI->GetSpec(spec);
    }
    PR_LogPrint("DOCSHELL %p InternalLoad %s\n", this, spec.get());
  }
  // Initialize aDocShell/aRequest
  if (aDocShell) {
    *aDocShell = nullptr;
  }
  if (aRequest) {
    *aRequest = nullptr;
  }

  if (!aURI) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_ENSURE_TRUE(IsValidLoadType(aLoadType), NS_ERROR_INVALID_ARG);

  NS_ENSURE_TRUE(!mIsBeingDestroyed, NS_ERROR_NOT_AVAILABLE);

  // wyciwyg urls can only be loaded through history. Any normal load of
  // wyciwyg through docshell is  illegal. Disallow such loads.
  if (aLoadType & LOAD_CMD_NORMAL) {
    bool isWyciwyg = false;
    rv = aURI->SchemeIs("wyciwyg", &isWyciwyg);
    if ((isWyciwyg && NS_SUCCEEDED(rv)) || NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  bool isJavaScript = false;
  if (NS_FAILED(aURI->SchemeIs("javascript", &isJavaScript))) {
    isJavaScript = false;
  }

  //
  // First, notify any nsIContentPolicy listeners about the document load.
  // Only abort the load if a content policy listener explicitly vetos it!
  //
  nsCOMPtr<Element> requestingElement;
  // Use nsPIDOMWindow since we _want_ to cross the chrome boundary if needed
  if (mScriptGlobal) {
    requestingElement = mScriptGlobal->GetFrameElementInternal();
  }

  RefPtr<nsGlobalWindow> MMADeathGrip = mScriptGlobal;

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  uint32_t contentType;
  bool isNewDocShell = false;
  bool isTargetTopLevelDocShell = false;
  nsCOMPtr<nsIDocShell> targetDocShell;
  if (aWindowTarget && *aWindowTarget) {
    // Locate the target DocShell.
    nsCOMPtr<nsIDocShellTreeItem> targetItem;
    rv = FindItemWithName(aWindowTarget, nullptr, this,
                          getter_AddRefs(targetItem));
    NS_ENSURE_SUCCESS(rv, rv);

    targetDocShell = do_QueryInterface(targetItem);
    // If the targetDocShell doesn't exist, then this is a new docShell
    // and we should consider this a TYPE_DOCUMENT load
    isNewDocShell = !targetDocShell;

    // If the targetDocShell and the rootDocShell are the same, then the
    // targetDocShell is the top level document and hence we should
    // consider this TYPE_DOCUMENT
    if (targetDocShell) {
      nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
      targetDocShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
      NS_ASSERTION(sameTypeRoot,
                   "No document shell root tree item from targetDocShell!");
      nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(sameTypeRoot);
      NS_ASSERTION(rootShell,
                   "No root docshell from document shell root tree item.");

      if (targetDocShell == rootShell) {
        isTargetTopLevelDocShell = true;
      }
    }
  }
  if (IsFrame() && !isNewDocShell && !isTargetTopLevelDocShell) {
    NS_ASSERTION(requestingElement, "A frame but no DOM element!?");
    contentType = requestingElement->IsHTMLElement(nsGkAtoms::iframe) ?
      nsIContentPolicy::TYPE_INTERNAL_IFRAME : nsIContentPolicy::TYPE_INTERNAL_FRAME;
  } else {
    contentType = nsIContentPolicy::TYPE_DOCUMENT;
  }

  nsISupports* context = requestingElement;
  if (!context) {
    context = ToSupports(mScriptGlobal);
  }

  // XXXbz would be nice to know the loading principal here... but we don't
  nsCOMPtr<nsIPrincipal> loadingPrincipal = do_QueryInterface(aOwner);
  if (!loadingPrincipal && aReferrer) {
    rv =
      CreatePrincipalFromReferrer(aReferrer, getter_AddRefs(loadingPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = NS_CheckContentLoadPolicy(contentType,
                                 aURI,
                                 loadingPrincipal,
                                 context,
                                 EmptyCString(),  // mime guess
                                 nullptr,  // extra
                                 &shouldLoad);

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    if (NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) {
      return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
    }

    return NS_ERROR_CONTENT_BLOCKED;
  }

  nsCOMPtr<nsISupports> owner(aOwner);
  //
  // Get an owner from the current document if necessary.  Note that we only
  // do this for URIs that inherit a security context and local file URIs;
  // in particular we do NOT do this for about:blank.  This way, random
  // about:blank loads that have no owner (which basically means they were
  // done by someone from chrome manually messing with our nsIWebNavigation
  // or by C++ setting document.location) don't get a funky principal.  If
  // callers want something interesting to happen with the about:blank
  // principal in this case, they should pass an owner in.
  //
  {
    bool inherits;
    // One more twist: Don't inherit the owner for external loads.
    if (aLoadType != LOAD_NORMAL_EXTERNAL && !owner &&
        (aFlags & INTERNAL_LOAD_FLAGS_INHERIT_OWNER) &&
        NS_SUCCEEDED(nsContentUtils::URIInheritsSecurityContext(aURI,
                                                                &inherits)) &&
        inherits) {
      owner = GetInheritedPrincipal(true);
    }
  }

  // Don't allow loads that would inherit our security context
  // if this document came from an unsafe channel.
  {
    bool willInherit;
    // This condition needs to match the one in
    // nsContentUtils::ChannelShouldInheritPrincipal.
    // Except we reverse the rv check to be safe in case
    // nsContentUtils::URIInheritsSecurityContext fails here and
    // succeeds there.
    rv = nsContentUtils::URIInheritsSecurityContext(aURI, &willInherit);
    if (NS_FAILED(rv) || willInherit || NS_IsAboutBlank(aURI)) {
      nsCOMPtr<nsIDocShellTreeItem> treeItem = this;
      do {
        nsCOMPtr<nsIDocShell> itemDocShell = do_QueryInterface(treeItem);
        bool isUnsafe;
        if (itemDocShell &&
            NS_SUCCEEDED(itemDocShell->GetChannelIsUnsafe(&isUnsafe)) &&
            isUnsafe) {
          return NS_ERROR_DOM_SECURITY_ERR;
        }

        nsCOMPtr<nsIDocShellTreeItem> parent;
        treeItem->GetSameTypeParent(getter_AddRefs(parent));
        parent.swap(treeItem);
      } while (treeItem);
    }
  }

  //
  // Resolve the window target before going any further...
  // If the load has been targeted to another DocShell, then transfer the
  // load to it...
  //
  if (aWindowTarget && *aWindowTarget) {
    // We've already done our owner-inheriting.  Mask out that bit, so we
    // don't try inheriting an owner from the target window if we came up
    // with a null owner above.
    aFlags = aFlags & ~INTERNAL_LOAD_FLAGS_INHERIT_OWNER;

    bool isNewWindow = false;
    if (!targetDocShell) {
      // If the docshell's document is sandboxed, only open a new window
      // if the document's SANDBOXED_AUXILLARY_NAVIGATION flag is not set.
      // (i.e. if allow-popups is specified)
      NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
      nsIDocument* doc = mContentViewer->GetDocument();
      uint32_t sandboxFlags = 0;

      if (doc) {
        sandboxFlags = doc->GetSandboxFlags();
        if (sandboxFlags & SANDBOXED_AUXILIARY_NAVIGATION) {
          return NS_ERROR_DOM_INVALID_ACCESS_ERR;
        }
      }

      nsCOMPtr<nsPIDOMWindow> win = GetWindow();
      NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

      nsDependentString name(aWindowTarget);
      nsCOMPtr<nsIDOMWindow> newWin;
      nsAutoCString spec;
      if (aURI) {
        aURI->GetSpec(spec);
      }
      rv = win->OpenNoNavigate(NS_ConvertUTF8toUTF16(spec),
                               name,  // window name
                               EmptyString(), // Features
                               getter_AddRefs(newWin));

      // In some cases the Open call doesn't actually result in a new
      // window being opened.  We can detect these cases by examining the
      // document in |newWin|, if any.
      nsCOMPtr<nsPIDOMWindow> piNewWin = do_QueryInterface(newWin);
      if (piNewWin) {
        nsCOMPtr<nsIDocument> newDoc = piNewWin->GetExtantDoc();
        if (!newDoc || newDoc->IsInitialDocument()) {
          isNewWindow = true;
          aFlags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;

          // set opener object to null for noreferrer
          if (aFlags & INTERNAL_LOAD_FLAGS_NO_OPENER) {
            piNewWin->SetOpenerWindow(nullptr, false);
          }
        }
      }

      nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(newWin);
      targetDocShell = do_QueryInterface(webNav);
    }

    //
    // Transfer the load to the target DocShell...  Pass nullptr as the
    // window target name from to prevent recursive retargeting!
    //
    if (NS_SUCCEEDED(rv) && targetDocShell) {
      rv = targetDocShell->InternalLoad(aURI,
                                        aOriginalURI,
                                        aLoadReplace,
                                        aReferrer,
                                        aReferrerPolicy,
                                        owner,
                                        aFlags,
                                        nullptr,         // No window target
                                        aTypeHint,
                                        NullString(),    // No forced download
                                        aPostData,
                                        aHeadersData,
                                        aLoadType,
                                        aSHEntry,
                                        aFirstParty,
                                        aSrcdoc,
                                        aSourceDocShell,
                                        aBaseURI,
                                        aDocShell,
                                        aRequest);
      if (rv == NS_ERROR_NO_CONTENT) {
        // XXXbz except we never reach this code!
        if (isNewWindow) {
          //
          // At this point, a new window has been created, but the
          // URI did not have any data associated with it...
          //
          // So, the best we can do, is to tear down the new window
          // that was just created!
          //
          nsCOMPtr<nsIDOMWindow> domWin = targetDocShell->GetWindow();
          if (domWin) {
            domWin->Close();
          }
        }
        //
        // NS_ERROR_NO_CONTENT should not be returned to the
        // caller... This is an internal error code indicating that
        // the URI had no data associated with it - probably a
        // helper-app style protocol (ie. mailto://)
        //
        rv = NS_OK;
      } else if (isNewWindow) {
        // XXX: Once new windows are created hidden, the new
        //      window will need to be made visible...  For now,
        //      do nothing.
      }
    }

    // Else we ran out of memory, or were a popup and got blocked,
    // or something.

    return rv;
  }

  //
  // Load is being targetted at this docshell so return an error if the
  // docshell is in the process of being destroyed.
  //
  if (mIsBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_STATE(!HasUnloadedParent());

  rv = CheckLoadingPermissions();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mFiredUnloadEvent) {
    if (IsOKToLoadURI(aURI)) {
      NS_PRECONDITION(!aWindowTarget || !*aWindowTarget,
                      "Shouldn't have a window target here!");

      // If this is a replace load, make whatever load triggered
      // the unload event also a replace load, so we don't
      // create extra history entries.
      if (LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {
        mLoadType = LOAD_NORMAL_REPLACE;
      }

      // Do this asynchronously
      nsCOMPtr<nsIRunnable> ev =
        new InternalLoadEvent(this, aURI, aOriginalURI, aLoadReplace,
                              aReferrer, aReferrerPolicy, aOwner, aFlags,
                              aTypeHint, aPostData, aHeadersData,
                              aLoadType, aSHEntry, aFirstParty, aSrcdoc,
                              aSourceDocShell, aBaseURI);
      return NS_DispatchToCurrentThread(ev);
    }

    // Just ignore this load attempt
    return NS_OK;
  }

  // If a source docshell has been passed, check to see if we are sandboxed
  // from it as the result of an iframe or CSP sandbox.
  if (aSourceDocShell && aSourceDocShell->IsSandboxedFrom(this)) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  // If this docshell is owned by a frameloader, make sure to cancel
  // possible frameloader initialization before loading a new page.
  nsCOMPtr<nsIDocShellTreeItem> parent = GetParentDocshell();
  if (parent) {
    nsCOMPtr<nsIDocument> doc = do_GetInterface(parent);
    if (doc) {
      doc->TryCancelFrameLoaderInitialization(this);
    }
  }

  // Before going any further vet loads initiated by external programs.
  if (aLoadType == LOAD_NORMAL_EXTERNAL) {
    // Disallow external chrome: loads targetted at content windows
    bool isChrome = false;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome) {
      NS_WARNING("blocked external chrome: url -- use '--chrome' option");
      return NS_ERROR_FAILURE;
    }

    // clear the decks to prevent context bleed-through (bug 298255)
    rv = CreateAboutBlankContentViewer(nullptr, nullptr);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    // reset loadType so we don't have to add lots of tests for
    // LOAD_NORMAL_EXTERNAL after this point
    aLoadType = LOAD_NORMAL;
  }

  mAllowKeywordFixup =
    (aFlags & INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) != 0;
  mURIResultedInDocument = false;  // reset the clock...

  if (aLoadType == LOAD_NORMAL ||
      aLoadType == LOAD_STOP_CONTENT ||
      LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_REPLACE_HISTORY) ||
      aLoadType == LOAD_HISTORY ||
      aLoadType == LOAD_LINK) {
    nsCOMPtr<nsIURI> currentURI = mCurrentURI;
    // Split currentURI and aURI on the '#' character.  Make sure we read
    // the return values of SplitURIAtHash; if it fails, we don't want to
    // allow a short-circuited navigation.
    nsAutoCString curBeforeHash, curHash, newBeforeHash, newHash;
    nsresult splitRv1, splitRv2;
    splitRv1 = currentURI ?
      nsContentUtils::SplitURIAtHash(currentURI, curBeforeHash, curHash) :
      NS_ERROR_FAILURE;
    splitRv2 = nsContentUtils::SplitURIAtHash(aURI, newBeforeHash, newHash);

    bool sameExceptHashes = NS_SUCCEEDED(splitRv1) &&
                            NS_SUCCEEDED(splitRv2) &&
                            curBeforeHash.Equals(newBeforeHash);

    if (!sameExceptHashes && sURIFixup && currentURI &&
        NS_SUCCEEDED(splitRv2)) {
      // Maybe aURI came from the exposable form of currentURI?
      nsCOMPtr<nsIURI> currentExposableURI;
      rv = sURIFixup->CreateExposableURI(currentURI,
                                         getter_AddRefs(currentExposableURI));
      NS_ENSURE_SUCCESS(rv, rv);
      splitRv1 = nsContentUtils::SplitURIAtHash(currentExposableURI,
                                                curBeforeHash, curHash);
      sameExceptHashes =
        NS_SUCCEEDED(splitRv1) && curBeforeHash.Equals(newBeforeHash);
    }

    bool historyNavBetweenSameDoc = false;
    if (mOSHE && aSHEntry) {
      // We're doing a history load.

      mOSHE->SharesDocumentWith(aSHEntry, &historyNavBetweenSameDoc);

#ifdef DEBUG
      if (historyNavBetweenSameDoc) {
        nsCOMPtr<nsIInputStream> currentPostData;
        mOSHE->GetPostData(getter_AddRefs(currentPostData));
        NS_ASSERTION(currentPostData == aPostData,
                     "Different POST data for entries for the same page?");
      }
#endif
    }

    // A short-circuited load happens when we navigate between two SHEntries
    // for the same document.  We do a short-circuited load under two
    // circumstances.  Either
    //
    //  a) we're navigating between two different SHEntries which share a
    //     document, or
    //
    //  b) we're navigating to a new shentry whose URI differs from the
    //     current URI only in its hash, the new hash is non-empty, and
    //     we're not doing a POST.
    //
    // The restriction tha the SHEntries in (a) must be different ensures
    // that history.go(0) and the like trigger full refreshes, rather than
    // short-circuited loads.
    bool doShortCircuitedLoad =
      (historyNavBetweenSameDoc && mOSHE != aSHEntry) ||
      (!aSHEntry && !aPostData &&
       sameExceptHashes && !newHash.IsEmpty());

    if (doShortCircuitedLoad) {
      // Save the position of the scrollers.
      nscoord cx = 0, cy = 0;
      GetCurScrollPos(ScrollOrientation_X, &cx);
      GetCurScrollPos(ScrollOrientation_Y, &cy);

      // Reset mLoadType to its original value once we exit this block,
      // because this short-circuited load might have started after a
      // normal, network load, and we don't want to clobber its load type.
      // See bug 737307.
      AutoRestore<uint32_t> loadTypeResetter(mLoadType);

      // If a non-short-circuit load (i.e., a network load) is pending,
      // make this a replacement load, so that we don't add a SHEntry here
      // and the network load goes into the SHEntry it expects to.
      if (JustStartedNetworkLoad() && (aLoadType & LOAD_CMD_NORMAL)) {
        mLoadType = LOAD_NORMAL_REPLACE;
      } else {
        mLoadType = aLoadType;
      }

      mURIResultedInDocument = true;

      nsCOMPtr<nsISHEntry> oldLSHE = mLSHE;

      /* we need to assign mLSHE to aSHEntry right here, so that on History
       * loads, SetCurrentURI() called from OnNewURI() will send proper
       * onLocationChange() notifications to the browser to update
       * back/forward buttons.
       */
      SetHistoryEntry(&mLSHE, aSHEntry);

      // Set the doc's URI according to the new history entry's URI.
      nsCOMPtr<nsIDocument> doc = GetDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
      doc->SetDocumentURI(aURI);

      /* This is a anchor traversal with in the same page.
       * call OnNewURI() so that, this traversal will be
       * recorded in session and global history.
       */
      nsCOMPtr<nsISupports> owner;
      if (mOSHE) {
        mOSHE->GetOwner(getter_AddRefs(owner));
      }
      // Pass true for aCloneSHChildren, since we're not
      // changing documents here, so all of our subframes are
      // still relevant to the new session history entry.
      //
      // It also makes OnNewURI(...) set LOCATION_CHANGE_SAME_DOCUMENT
      // flag on firing onLocationChange(...).
      // Anyway, aCloneSHChildren param is simply reflecting
      // doShortCircuitedLoad in this scope.
      OnNewURI(aURI, nullptr, owner, mLoadType, true, true, true);

      nsCOMPtr<nsIInputStream> postData;
      nsCOMPtr<nsISupports> cacheKey;

      if (mOSHE) {
        /* save current position of scroller(s) (bug 59774) */
        mOSHE->SetScrollPosition(cx, cy);
        // Get the postdata and page ident from the current page, if
        // the new load is being done via normal means.  Note that
        // "normal means" can be checked for just by checking for
        // LOAD_CMD_NORMAL, given the loadType and allowScroll check
        // above -- it filters out some LOAD_CMD_NORMAL cases that we
        // wouldn't want here.
        if (aLoadType & LOAD_CMD_NORMAL) {
          mOSHE->GetPostData(getter_AddRefs(postData));
          mOSHE->GetCacheKey(getter_AddRefs(cacheKey));

          // Link our new SHEntry to the old SHEntry's back/forward
          // cache data, since the two SHEntries correspond to the
          // same document.
          if (mLSHE) {
            mLSHE->AdoptBFCacheEntry(mOSHE);
          }
        }
      }

      /* Assign mOSHE to mLSHE. This will either be a new entry created
       * by OnNewURI() for normal loads or aSHEntry for history loads.
       */
      if (mLSHE) {
        SetHistoryEntry(&mOSHE, mLSHE);
        // Save the postData obtained from the previous page
        // in to the session history entry created for the
        // anchor page, so that any history load of the anchor
        // page will restore the appropriate postData.
        if (postData) {
          mOSHE->SetPostData(postData);
        }

        // Make sure we won't just repost without hitting the
        // cache first
        if (cacheKey) {
          mOSHE->SetCacheKey(cacheKey);
        }
      }

      /* Restore the original LSHE if we were loading something
       * while short-circuited load was initiated.
       */
      SetHistoryEntry(&mLSHE, oldLSHE);
      /* Set the title for the SH entry for this target url. so that
       * SH menus in go/back/forward buttons won't be empty for this.
       */
      if (mSessionHistory) {
        int32_t index = -1;
        mSessionHistory->GetIndex(&index);
        nsCOMPtr<nsISHEntry> shEntry;
        mSessionHistory->GetEntryAtIndex(index, false, getter_AddRefs(shEntry));
        NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);
        shEntry->SetTitle(mTitle);
      }

      /* Set the title for the Global History entry for this anchor url.
       */
      if (mUseGlobalHistory && !mInPrivateBrowsing) {
        nsCOMPtr<IHistory> history = services::GetHistoryService();
        if (history) {
          history->SetURITitle(aURI, mTitle);
        } else if (mGlobalHistory) {
          mGlobalHistory->SetPageTitle(aURI, mTitle);
        }
      }

      SetDocCurrentStateObj(mOSHE);

      // Inform the favicon service that the favicon for oldURI also
      // applies to aURI.
      CopyFavicon(currentURI, aURI, mInPrivateBrowsing);

      RefPtr<nsGlobalWindow> win = mScriptGlobal ?
        mScriptGlobal->GetCurrentInnerWindowInternal() : nullptr;

      // ScrollToAnchor doesn't necessarily cause us to scroll the window;
      // the function decides whether a scroll is appropriate based on the
      // arguments it receives.  But even if we don't end up scrolling,
      // ScrollToAnchor performs other important tasks, such as informing
      // the presShell that we have a new hash.  See bug 680257.
      rv = ScrollToAnchor(curHash, newHash, aLoadType);
      NS_ENSURE_SUCCESS(rv, rv);

      /* restore previous position of scroller(s), if we're moving
       * back in history (bug 59774)
       */
      nscoord bx, by;
      bool needsScrollPosUpdate = false;
      if (mOSHE && (aLoadType == LOAD_HISTORY ||
                    aLoadType == LOAD_RELOAD_NORMAL)) {
        needsScrollPosUpdate = true;
        mOSHE->GetScrollPosition(&bx, &by);
      }

      // Dispatch the popstate and hashchange events, as appropriate.
      //
      // The event dispatch below can cause us to re-enter script and
      // destroy the docshell, nulling out mScriptGlobal. Hold a stack
      // reference to avoid null derefs. See bug 914521.
      if (win) {
        // Fire a hashchange event URIs differ, and only in their hashes.
        bool doHashchange = sameExceptHashes && !curHash.Equals(newHash);

        if (historyNavBetweenSameDoc || doHashchange) {
          win->DispatchSyncPopState();
        }

        if (needsScrollPosUpdate && win->HasActiveDocument()) {
          SetCurScrollPosEx(bx, by);
        }

        if (doHashchange) {
          // Note that currentURI hasn't changed because it's on the
          // stack, so we can just use it directly as the old URI.
          win->DispatchAsyncHashchange(currentURI, aURI);
        }
      }

      return NS_OK;
    }
  }

  // Check if the webbrowser chrome wants the load to proceed; this can be
  // used to cancel attempts to load URIs in the wrong process.
  nsCOMPtr<nsIWebBrowserChrome3> browserChrome3 = do_GetInterface(mTreeOwner);
  if (browserChrome3) {
    bool shouldLoad;
    rv = browserChrome3->ShouldLoadURI(this, aURI, aReferrer, &shouldLoad);
    if (NS_SUCCEEDED(rv) && !shouldLoad) {
      return NS_OK;
    }
  }

  // mContentViewer->PermitUnload can destroy |this| docShell, which
  // causes the next call of CanSavePresentation to crash.
  // Hold onto |this| until we return, to prevent a crash from happening.
  // (bug#331040)
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

  // Don't init timing for javascript:, since it generally doesn't
  // actually start a load or anything.  If it does, we'll init
  // timing then, from OnStateChange.

  // XXXbz mTiming should know what channel it's for, so we don't
  // need this hackery.  Note that this is still broken in cases
  // when we're loading something that's not javascript: and the
  // beforeunload handler denies the load.  That will screw up
  // timing for the next load!
  if (!isJavaScript) {
    MaybeInitTiming();
  }
  bool timeBeforeUnload = aFileName.IsVoid();
  if (mTiming && timeBeforeUnload) {
    mTiming->NotifyBeforeUnload();
  }
  // Check if the page doesn't want to be unloaded. The javascript:
  // protocol handler deals with this for javascript: URLs.
  if (!isJavaScript && aFileName.IsVoid() && mContentViewer) {
    bool okToUnload;
    rv = mContentViewer->PermitUnload(false, &okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the
      // load.
      return NS_OK;
    }
  }

  if (mTiming && timeBeforeUnload) {
    mTiming->NotifyUnloadAccepted(mCurrentURI);
  }

  // Whenever a top-level browsing context is navigated, the user agent MUST
  // lock the orientation of the document to the document's default
  // orientation. We don't explicitly check for a top-level browsing context
  // here because orientation is only set on top-level browsing contexts.
  // We make an exception for apps because they currently rely on
  // orientation locks persisting across browsing contexts.
  if (OrientationLock() != eScreenOrientation_None && !GetIsApp()) {
#ifdef DEBUG
    nsCOMPtr<nsIDocShellTreeItem> parent;
    GetSameTypeParent(getter_AddRefs(parent));
    MOZ_ASSERT(!parent);
#endif
    SetOrientationLock(eScreenOrientation_None);
    if (mIsActive) {
      ScreenOrientation::UpdateActiveOrientationLock(eScreenOrientation_None);
    }
  }

  // Check for saving the presentation here, before calling Stop().
  // This is necessary so that we can catch any pending requests.
  // Since the new request has not been created yet, we pass null for the
  // new request parameter.
  // Also pass nullptr for the document, since it doesn't affect the return
  // value for our purposes here.
  bool savePresentation = CanSavePresentation(aLoadType, nullptr, nullptr);

  // Don't stop current network activity for javascript: URL's since
  // they might not result in any data, and thus nothing should be
  // stopped in those cases. In the case where they do result in
  // data, the javascript: URL channel takes care of stopping
  // current network activity.
  if (!isJavaScript && aFileName.IsVoid()) {
    // Stop any current network activity.
    // Also stop content if this is a zombie doc. otherwise
    // the onload will be delayed by other loads initiated in the
    // background by the first document that
    // didn't fully load before the next load was initiated.
    // If not a zombie, don't stop content until data
    // starts arriving from the new URI...

    nsCOMPtr<nsIContentViewer> zombieViewer;
    if (mContentViewer) {
      mContentViewer->GetPreviousViewer(getter_AddRefs(zombieViewer));
    }

    if (zombieViewer ||
        LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_STOP_CONTENT)) {
      rv = Stop(nsIWebNavigation::STOP_ALL);
    } else {
      rv = Stop(nsIWebNavigation::STOP_NETWORK);
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mLoadType = aLoadType;

  // mLSHE should be assigned to aSHEntry, only after Stop() has
  // been called. But when loading an error page, do not clear the
  // mLSHE for the real page.
  if (mLoadType != LOAD_ERROR_PAGE) {
    SetHistoryEntry(&mLSHE, aSHEntry);
  }

  mSavingOldViewer = savePresentation;

  // If we have a saved content viewer in history, restore and show it now.
  if (aSHEntry && (mLoadType & LOAD_CMD_HISTORY)) {
    // Make sure our history ID points to the same ID as
    // SHEntry's docshell ID.
    aSHEntry->GetDocshellID(&mHistoryID);

    // It's possible that the previous viewer of mContentViewer is the
    // viewer that will end up in aSHEntry when it gets closed.  If that's
    // the case, we need to go ahead and force it into its shentry so we
    // can restore it.
    if (mContentViewer) {
      nsCOMPtr<nsIContentViewer> prevViewer;
      mContentViewer->GetPreviousViewer(getter_AddRefs(prevViewer));
      if (prevViewer) {
#ifdef DEBUG
        nsCOMPtr<nsIContentViewer> prevPrevViewer;
        prevViewer->GetPreviousViewer(getter_AddRefs(prevPrevViewer));
        NS_ASSERTION(!prevPrevViewer, "Should never have viewer chain here");
#endif
        nsCOMPtr<nsISHEntry> viewerEntry;
        prevViewer->GetHistoryEntry(getter_AddRefs(viewerEntry));
        if (viewerEntry == aSHEntry) {
          // Make sure this viewer ends up in the right place
          mContentViewer->SetPreviousViewer(nullptr);
          prevViewer->Destroy();
        }
      }
    }
    nsCOMPtr<nsISHEntry> oldEntry = mOSHE;
    bool restoring;
    rv = RestorePresentation(aSHEntry, &restoring);
    if (restoring) {
      return rv;
    }

    // We failed to restore the presentation, so clean up.
    // Both the old and new history entries could potentially be in
    // an inconsistent state.
    if (NS_FAILED(rv)) {
      if (oldEntry) {
        oldEntry->SyncPresentationState();
      }

      aSHEntry->SyncPresentationState();
    }
  }

  nsAutoString srcdoc;
  if (aFlags & INTERNAL_LOAD_FLAGS_IS_SRCDOC) {
    srcdoc = aSrcdoc;
  } else {
    srcdoc = NullString();
  }

  net::PredictorLearn(aURI, nullptr,
                      nsINetworkPredictor::LEARN_LOAD_TOPLEVEL, this);
  net::PredictorPredict(aURI, nullptr,
                        nsINetworkPredictor::PREDICT_LOAD, this, nullptr);

  nsCOMPtr<nsIRequest> req;
  rv = DoURILoad(aURI, aOriginalURI, aLoadReplace, aReferrer,
                 !(aFlags & INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER),
                 aReferrerPolicy,
                 owner, aTypeHint, aFileName, aPostData, aHeadersData,
                 aFirstParty, aDocShell, getter_AddRefs(req),
                 (aFlags & INTERNAL_LOAD_FLAGS_FIRST_LOAD) != 0,
                 (aFlags & INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER) != 0,
                 (aFlags & INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES) != 0,
                 srcdoc, aBaseURI, contentType);
  if (req && aRequest) {
    NS_ADDREF(*aRequest = req);
  }

  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIChannel> chan(do_QueryInterface(req));
    DisplayLoadError(rv, aURI, nullptr, chan);
  }

  return rv;
}

nsIPrincipal*
nsDocShell::GetInheritedPrincipal(bool aConsiderCurrentDocument)
{
  nsCOMPtr<nsIDocument> document;
  bool inheritedFromCurrent = false;

  if (aConsiderCurrentDocument && mContentViewer) {
    document = mContentViewer->GetDocument();
    inheritedFromCurrent = true;
  }

  if (!document) {
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    GetSameTypeParent(getter_AddRefs(parentItem));
    if (parentItem) {
      document = parentItem->GetDocument();
    }
  }

  if (!document) {
    if (!aConsiderCurrentDocument) {
      return nullptr;
    }

    // Make sure we end up with _something_ as the principal no matter
    // what.If this fails, we'll just get a null docViewer and bail.
    EnsureContentViewer();
    if (!mContentViewer) {
      return nullptr;
    }
    document = mContentViewer->GetDocument();
  }

  //-- Get the document's principal
  if (document) {
    nsIPrincipal* docPrincipal = document->NodePrincipal();

    // Don't allow loads in typeContent docShells to inherit the system
    // principal from existing documents.
    if (inheritedFromCurrent &&
        mItemType == typeContent &&
        nsContentUtils::IsSystemPrincipal(docPrincipal)) {
      return nullptr;
    }

    return docPrincipal;
  }

  return nullptr;
}

nsresult
nsDocShell::DoURILoad(nsIURI* aURI,
                      nsIURI* aOriginalURI,
                      bool aLoadReplace,
                      nsIURI* aReferrerURI,
                      bool aSendReferrer,
                      uint32_t aReferrerPolicy,
                      nsISupports* aOwner,
                      const char* aTypeHint,
                      const nsAString& aFileName,
                      nsIInputStream* aPostData,
                      nsIInputStream* aHeadersData,
                      bool aFirstParty,
                      nsIDocShell** aDocShell,
                      nsIRequest** aRequest,
                      bool aIsNewWindowTarget,
                      bool aBypassClassifier,
                      bool aForceAllowCookies,
                      const nsAString& aSrcdoc,
                      nsIURI* aBaseURI,
                      nsContentPolicyType aContentPolicyType)
{
  nsresult rv;
  nsCOMPtr<nsIURILoader> uriLoader;

  uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsLoadFlags loadFlags = mDefaultLoadFlags;
  if (aFirstParty) {
    // tag first party URL loads
    loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
  }

  if (mLoadType == LOAD_ERROR_PAGE) {
    // Error pages are LOAD_BACKGROUND
    loadFlags |= nsIChannel::LOAD_BACKGROUND;
  }

  if (IsFrame()) {
    // Only allow view-source scheme in top-level docshells. view-source is
    // the only scheme to which this applies at the moment due to potential
    // timing attacks to read data from cross-origin iframes. If this widens
    // we should add a protocol flag for whether the scheme is allowed in
    // frames and use something like nsNetUtil::NS_URIChainHasFlags.
    nsCOMPtr<nsIURI> tempURI = aURI;
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(tempURI);
    while (nestedURI) {
      // view-source should always be an nsINestedURI, loop and check the
      // scheme on this and all inner URIs that are also nested URIs.
      bool isViewSource = false;
      rv = tempURI->SchemeIs("view-source", &isViewSource);
      if (NS_FAILED(rv) || isViewSource) {
        return NS_ERROR_UNKNOWN_PROTOCOL;
      }
      nestedURI->GetInnerURI(getter_AddRefs(tempURI));
      nestedURI = do_QueryInterface(tempURI);
    }
  }

  // For mozWidget, display a load error if we navigate to a page which is not
  // claimed in |widgetPages|.
  if (mScriptGlobal) {
    // When we go to display a load error for an invalid mozWidget page, we will
    // try to load an about:neterror page, which is also an invalid mozWidget
    // page. To avoid recursion, we skip this check if aURI's scheme is "about".

    // The goal is to prevent leaking sensitive information of an invalid page of
    // an app, so allowing about:blank would not be conflict to the goal.
    bool isAbout = false;
    rv = aURI->SchemeIs("about", &isAbout);
    if (NS_SUCCEEDED(rv) && !isAbout &&
        nsIDocShell::GetIsApp()) {
      nsCOMPtr<Element> frameElement = mScriptGlobal->GetFrameElementInternal();
      if (frameElement) {
        nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(frameElement);
        // |GetReallyIsApp| indicates the browser frame is a valid app or widget.
        // Here we prevent navigating to an app or widget which loses its validity
        // by loading invalid page or other way.
        if (browserFrame && !browserFrame->GetReallyIsApp()) {
          nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
          if (serv) {
              serv->NotifyObservers(GetDocument(), "invalid-widget", nullptr);
          }
          return NS_ERROR_MALFORMED_URI;
        }
      }
    }
  }

  // open a channel for the url
  nsCOMPtr<nsIChannel> channel;

  bool isSrcdoc = !aSrcdoc.IsVoid();

  nsCOMPtr<nsINode> requestingNode;
  if (mScriptGlobal) {
    requestingNode = mScriptGlobal->GetFrameElementInternal();
    if (!requestingNode) {
      requestingNode = mScriptGlobal->GetExtantDoc();
    }
  }

  bool isSandBoxed = mSandboxFlags & SANDBOXED_ORIGIN;
  // only inherit if we have a triggeringPrincipal
  bool inherit = false;

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = do_QueryInterface(aOwner);
  if (triggeringPrincipal) {
    inherit = nsContentUtils::ChannelShouldInheritPrincipal(
      triggeringPrincipal,
      aURI,
      true, // aInheritForAboutBlank
      isSrcdoc);
  } else if (!triggeringPrincipal && aReferrerURI) {
    rv = CreatePrincipalFromReferrer(aReferrerURI,
                                     getter_AddRefs(triggeringPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  }

  nsSecurityFlags securityFlags = nsILoadInfo::SEC_NORMAL;
  if (inherit) {
    securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }
  if (isSandBoxed) {
    securityFlags |= nsILoadInfo::SEC_SANDBOXED;
  }

  if (!isSrcdoc) {
    rv = NS_NewChannelInternal(getter_AddRefs(channel),
                               aURI,
                               requestingNode,
                               requestingNode
                                 ? requestingNode->NodePrincipal()
                                 : triggeringPrincipal.get(),
                                triggeringPrincipal,
                                securityFlags,
                                aContentPolicyType,
                                nullptr,   // loadGroup
                                static_cast<nsIInterfaceRequestor*>(this),
                                loadFlags);

    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_UNKNOWN_PROTOCOL) {
        // This is a uri with a protocol scheme we don't know how
        // to handle.  Embedders might still be interested in
        // handling the load, though, so we fire a notification
        // before throwing the load away.
        bool abort = false;
        nsresult rv2 = mContentListener->OnStartURIOpen(aURI, &abort);
        if (NS_SUCCEEDED(rv2) && abort) {
          // Hey, they're handling the load for us!  How convenient!
          return NS_OK;
        }
      }
      return rv;
    }
    if (aBaseURI) {
        nsCOMPtr<nsIViewSourceChannel> vsc = do_QueryInterface(channel);
        if (vsc) {
            vsc->SetBaseURI(aBaseURI);
        }
    }
  } else {
    nsAutoCString scheme;
    rv = aURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    bool isViewSource;
    aURI->SchemeIs("view-source", &isViewSource);

    if (isViewSource) {
      nsViewSourceHandler* vsh = nsViewSourceHandler::GetInstance();
      NS_ENSURE_TRUE(vsh, NS_ERROR_FAILURE);

      rv = vsh->NewSrcdocChannel(aURI, aBaseURI, aSrcdoc,
                                 requestingNode,
                                 requestingNode
                                   ? requestingNode->NodePrincipal()
                                   : triggeringPrincipal.get(),
                                 triggeringPrincipal,
                                 securityFlags,
                                 aContentPolicyType,
                                 getter_AddRefs(channel));
    } else {
      rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel),
                                            aURI,
                                            aSrcdoc,
                                            NS_LITERAL_CSTRING("text/html"),
                                            requestingNode,
                                            requestingNode ?
                                              requestingNode->NodePrincipal() :
                                              triggeringPrincipal.get(),
                                            triggeringPrincipal,
                                            securityFlags,
                                            aContentPolicyType,
                                            true);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIInputStreamChannel> isc = do_QueryInterface(channel);
      MOZ_ASSERT(isc);
      isc->SetBaseURI(aBaseURI);
    }
  }

  nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
    do_QueryInterface(channel);
  if (appCacheChannel) {
    // Any document load should not inherit application cache.
    appCacheChannel->SetInheritApplicationCache(false);

    // Loads with the correct permissions should check for a matching
    // application cache.
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
      // Permission will be checked in the parent process
      appCacheChannel->SetChooseApplicationCache(true);
    } else {
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

      if (secMan) {
        nsCOMPtr<nsIPrincipal> principal;
        secMan->GetDocShellCodebasePrincipal(aURI, this,
                                             getter_AddRefs(principal));
        appCacheChannel->SetChooseApplicationCache(
          NS_ShouldCheckAppCache(principal, mInPrivateBrowsing));
      }
    }
  }

  // Make sure to give the caller a channel if we managed to create one
  // This is important for correct error page/session history interaction
  if (aRequest) {
    NS_ADDREF(*aRequest = channel);
  }

  if (aOriginalURI) {
    channel->SetOriginalURI(aOriginalURI);
    if (aLoadReplace) {
      uint32_t loadFlags;
      channel->GetLoadFlags(&loadFlags);
      NS_ENSURE_SUCCESS(rv, rv);
      channel->SetLoadFlags(loadFlags | nsIChannel::LOAD_REPLACE);
    }
  } else {
    channel->SetOriginalURI(aURI);
  }

  if (aTypeHint && *aTypeHint) {
    channel->SetContentType(nsDependentCString(aTypeHint));
    mContentTypeHint = aTypeHint;
  } else {
    mContentTypeHint.Truncate();
  }

  if (!aFileName.IsVoid()) {
    rv = channel->SetContentDisposition(nsIChannel::DISPOSITION_ATTACHMENT);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aFileName.IsEmpty()) {
      rv = channel->SetContentDispositionFilename(aFileName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (mLoadType == LOAD_NORMAL_ALLOW_MIXED_CONTENT ||
      mLoadType == LOAD_RELOAD_ALLOW_MIXED_CONTENT) {
    rv = SetMixedContentChannel(channel);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mMixedContentChannel) {
    /*
     * If the user "Disables Protection on This Page", we call
     * SetMixedContentChannel for the first time, otherwise
     * mMixedContentChannel is still null.
     * Later, if the new channel passes a same orign check, we remember the
     * users decision by calling SetMixedContentChannel using the new channel.
     * This way, the user does not have to click the disable protection button
     * over and over for browsing the same site.
     */
    rv = nsContentUtils::CheckSameOrigin(mMixedContentChannel, channel);
    if (NS_FAILED(rv) || NS_FAILED(SetMixedContentChannel(channel))) {
      SetMixedContentChannel(nullptr);
    }
  }

  // hack
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal(
    do_QueryInterface(channel));
  if (httpChannelInternal) {
    if (aForceAllowCookies) {
      httpChannelInternal->SetThirdPartyFlags(
        nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
    }
    if (aFirstParty) {
      httpChannelInternal->SetDocumentURI(aURI);
    } else {
      httpChannelInternal->SetDocumentURI(aReferrerURI);
    }
    httpChannelInternal->SetRedirectMode(
      nsIHttpChannelInternal::REDIRECT_MODE_MANUAL);
  }

  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(channel));
  if (props) {
    // save true referrer for those who need it (e.g. xpinstall whitelisting)
    // Currently only http and ftp channels support this.
    props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                  aReferrerURI);
  }

  nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(channel));
  nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(channel));

  
  /* Get the cache Key from SH */
  nsCOMPtr<nsISupports> cacheKey;
  if (mLSHE) {
    mLSHE->GetCacheKey(getter_AddRefs(cacheKey));
  } else if (mOSHE) {  // for reload cases
    mOSHE->GetCacheKey(getter_AddRefs(cacheKey));
  }
  
  if (uploadChannel) {
    // figure out if we need to set the post data stream on the channel...
    // right now, this is only done for http channels.....
    if (aPostData) {
      // XXX it's a bit of a hack to rewind the postdata stream here but
      // it has to be done in case the post data is being reused multiple
      // times.
      nsCOMPtr<nsISeekableStream> postDataSeekable =
        do_QueryInterface(aPostData);
      if (postDataSeekable) {
        rv = postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // we really need to have a content type associated with this stream!!
      uploadChannel->SetUploadStream(aPostData, EmptyCString(), -1);
      /* If there is a valid postdata *and* it is a History Load,
       * set up the cache key on the channel, to retrieve the
       * data *only* from the cache. If it is a normal reload, the
       * cache is free to go to the server for updated postdata.
       */
      if (cacheChannel && cacheKey) {
        if (mLoadType == LOAD_HISTORY ||
            mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
          cacheChannel->SetCacheKey(cacheKey);
          uint32_t loadFlags;
          if (NS_SUCCEEDED(channel->GetLoadFlags(&loadFlags))) {
            channel->SetLoadFlags(
              loadFlags | nsICachingChannel::LOAD_ONLY_FROM_CACHE);
          }
        } else if (mLoadType == LOAD_RELOAD_NORMAL) {
          cacheChannel->SetCacheKey(cacheKey);
        }
      }
    } else {
      /* If there is no postdata, set the cache key on the channel, and
       * do not set the LOAD_ONLY_FROM_CACHE flag, so that the channel
       * will be free to get it from net if it is not found in cache.
       * New cache may use it creatively on CGI pages with GET
       * method and even on those that say "no-cache"
       */
      if (mLoadType == LOAD_HISTORY ||
          mLoadType == LOAD_RELOAD_NORMAL ||
          mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
        if (cacheChannel && cacheKey) {
          cacheChannel->SetCacheKey(cacheKey);
        }
      }
    }
  }
  
  if (httpChannel) {
    if (aHeadersData) {
      rv = AddHeadersToChannel(aHeadersData, httpChannel);
    }
    // Set the referrer explicitly
    if (aReferrerURI && aSendReferrer) {
      // Referrer is currenly only set for link clicks here.
      httpChannel->SetReferrerWithPolicy(aReferrerURI, aReferrerPolicy);
    }
  }

  nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(channel);
  if (scriptChannel) {
    // Allow execution against our context if the principals match
    scriptChannel->SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
  }

  if (aIsNewWindowTarget) {
    nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(channel);
    if (props) {
      props->SetPropertyAsBool(NS_LITERAL_STRING("docshell.newWindowTarget"),
                               true);
    }
  }

  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(channel));
  if (timedChannel) {
    timedChannel->SetTimingEnabled(true);
    if (IsFrame()) {
      timedChannel->SetInitiatorType(NS_LITERAL_STRING("subdocument"));
    }
  }

  rv = DoChannelLoad(channel, uriLoader, aBypassClassifier);

  //
  // If the channel load failed, we failed and nsIWebProgress just ain't
  // gonna happen.
  //
  if (NS_SUCCEEDED(rv)) {
    if (aDocShell) {
      *aDocShell = this;
      NS_ADDREF(*aDocShell);
    }
  }

  return rv;
}

static NS_METHOD
AppendSegmentToString(nsIInputStream* aIn,
                      void* aClosure,
                      const char* aFromRawSegment,
                      uint32_t aToOffset,
                      uint32_t aCount,
                      uint32_t* aWriteCount)
{
  // aFromSegment now contains aCount bytes of data.

  nsAutoCString* buf = static_cast<nsAutoCString*>(aClosure);
  buf->Append(aFromRawSegment, aCount);

  // Indicate that we have consumed all of aFromSegment
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
nsDocShell::AddHeadersToChannel(nsIInputStream* aHeadersData,
                                nsIChannel* aGenericChannel)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aGenericChannel);
  NS_ENSURE_STATE(httpChannel);

  uint32_t numRead;
  nsAutoCString headersString;
  nsresult rv = aHeadersData->ReadSegments(AppendSegmentToString,
                                           &headersString,
                                           UINT32_MAX,
                                           &numRead);
  NS_ENSURE_SUCCESS(rv, rv);

  // used during the manipulation of the String from the InputStream
  nsAutoCString headerName;
  nsAutoCString headerValue;
  int32_t crlf;
  int32_t colon;

  //
  // Iterate over the headersString: for each "\r\n" delimited chunk,
  // add the value as a header to the nsIHttpChannel
  //

  static const char kWhitespace[] = "\b\t\r\n ";
  while (true) {
    crlf = headersString.Find("\r\n");
    if (crlf == kNotFound) {
      return NS_OK;
    }

    const nsCSubstring& oneHeader = StringHead(headersString, crlf);

    colon = oneHeader.FindChar(':');
    if (colon == kNotFound) {
      return NS_ERROR_UNEXPECTED;
    }

    headerName = StringHead(oneHeader, colon);
    headerValue = Substring(oneHeader, colon + 1);

    headerName.Trim(kWhitespace);
    headerValue.Trim(kWhitespace);

    headersString.Cut(0, crlf + 2);

    //
    // FINALLY: we can set the header!
    //

    rv = httpChannel->SetRequestHeader(headerName, headerValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_NOTREACHED("oops");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDocShell::DoChannelLoad(nsIChannel* aChannel,
                          nsIURILoader* aURILoader,
                          bool aBypassClassifier)
{
  nsresult rv;
  // Mark the channel as being a document URI and allow content sniffing...
  nsLoadFlags loadFlags = 0;
  (void)aChannel->GetLoadFlags(&loadFlags);
  loadFlags |= nsIChannel::LOAD_DOCUMENT_URI |
               nsIChannel::LOAD_CALL_CONTENT_SNIFFERS;

  // Load attributes depend on load type...
  switch (mLoadType) {
    case LOAD_HISTORY: {
      // Only send VALIDATE_NEVER if mLSHE's URI was never changed via
      // push/replaceState (bug 669671).
      bool uriModified = false;
      if (mLSHE) {
        mLSHE->GetURIWasModified(&uriModified);
      }

      if (!uriModified) {
        loadFlags |= nsIRequest::VALIDATE_NEVER;
      }
      break;
    }

    case LOAD_RELOAD_CHARSET_CHANGE:
      loadFlags |= nsIRequest::LOAD_FROM_CACHE;
      break;

    case LOAD_RELOAD_NORMAL:
    case LOAD_REFRESH:
      loadFlags |= nsIRequest::VALIDATE_ALWAYS;
      break;

    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
    case LOAD_REPLACE_BYPASS_CACHE:
      loadFlags |= nsIRequest::LOAD_BYPASS_CACHE |
                   nsIRequest::LOAD_FRESH_CONNECTION;
      break;

    case LOAD_NORMAL:
    case LOAD_LINK:
      // Set cache checking flags
      switch (Preferences::GetInt("browser.cache.check_doc_frequency", -1)) {
        case 0:
          loadFlags |= nsIRequest::VALIDATE_ONCE_PER_SESSION;
          break;
        case 1:
          loadFlags |= nsIRequest::VALIDATE_ALWAYS;
          break;
        case 2:
          loadFlags |= nsIRequest::VALIDATE_NEVER;
          break;
      }
      break;
  }

  if (!aBypassClassifier) {
    loadFlags |= nsIChannel::LOAD_CLASSIFY_URI;
  }

  // If the user pressed shift-reload, then do not allow ServiceWorker
  // interception to occur. See step 12.1 of the SW HandleFetch algorithm.
  if (IsForceReloadType(mLoadType)) {
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  }

  (void)aChannel->SetLoadFlags(loadFlags);

  uint32_t openFlags = 0;
  if (mLoadType == LOAD_LINK) {
    openFlags |= nsIURILoader::IS_CONTENT_PREFERRED;
  }
  if (!mAllowContentRetargeting) {
    openFlags |= nsIURILoader::DONT_RETARGET;
  }
  rv = aURILoader->OpenURI(aChannel, openFlags, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDocShell::ScrollToAnchor(nsACString& aCurHash, nsACString& aNewHash,
                           uint32_t aLoadType)
{
  if (!mCurrentURI) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    // If we failed to get the shell, or if there is no shell,
    // nothing left to do here.
    return NS_OK;
  }

  nsIScrollableFrame* rootScroll = shell->GetRootScrollFrameAsScrollable();
  if (rootScroll) {
    rootScroll->ClearDidHistoryRestore();
  }

  // If we have no new anchor, we do not want to scroll, unless there is a
  // current anchor and we are doing a history load.  So return if we have no
  // new anchor, and there is no current anchor or the load is not a history
  // load.
  if ((aCurHash.IsEmpty() || aLoadType != LOAD_HISTORY) &&
      aNewHash.IsEmpty()) {
    return NS_OK;
  }

  // Take the '#' off aNewHash to get the ref name.  (aNewHash might be empty,
  // but that's fine.)
  nsDependentCSubstring newHashName(aNewHash, 1);

  // Both the new and current URIs refer to the same page. We can now
  // browse to the hash stored in the new URI.

  if (!newHashName.IsEmpty()) {
    // anchor is there, but if it's a load from history,
    // we don't have any anchor jumping to do
    bool scroll = aLoadType != LOAD_HISTORY &&
                  aLoadType != LOAD_RELOAD_NORMAL;

    char* str = ToNewCString(newHashName);
    if (!str) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // nsUnescape modifies the string that is passed into it.
    nsUnescape(str);

    // We assume that the bytes are in UTF-8, as it says in the
    // spec:
    // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1

    // We try the UTF-8 string first, and then try the document's
    // charset (see below).  If the string is not UTF-8,
    // conversion will fail and give us an empty Unicode string.
    // In that case, we should just fall through to using the
    // page's charset.
    nsresult rv = NS_ERROR_FAILURE;
    NS_ConvertUTF8toUTF16 uStr(str);
    if (!uStr.IsEmpty()) {
      rv = shell->GoToAnchor(NS_ConvertUTF8toUTF16(str), scroll,
                             nsIPresShell::SCROLL_SMOOTH_AUTO);
    }
    free(str);

    // Above will fail if the anchor name is not UTF-8.  Need to
    // convert from document charset to unicode.
    if (NS_FAILED(rv)) {
      // Get a document charset
      NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
      nsIDocument* doc = mContentViewer->GetDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
      const nsACString& aCharset = doc->GetDocumentCharacterSet();

      nsCOMPtr<nsITextToSubURI> textToSubURI =
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      // Unescape and convert to unicode
      nsXPIDLString uStr;

      rv = textToSubURI->UnEscapeAndConvert(PromiseFlatCString(aCharset).get(),
                                            PromiseFlatCString(newHashName).get(),
                                            getter_Copies(uStr));
      NS_ENSURE_SUCCESS(rv, rv);

      // Ignore return value of GoToAnchor, since it will return an error
      // if there is no such anchor in the document, which is actually a
      // success condition for us (we want to update the session history
      // with the new URI no matter whether we actually scrolled
      // somewhere).
      //
      // When newHashName contains "%00", unescaped string may be empty.
      // And GoToAnchor asserts if we ask it to scroll to an empty ref.
      shell->GoToAnchor(uStr, scroll && !uStr.IsEmpty(),
                        nsIPresShell::SCROLL_SMOOTH_AUTO);
    }
  } else {
    // Tell the shell it's at an anchor, without scrolling.
    shell->GoToAnchor(EmptyString(), false);

    // An empty anchor was found, but if it's a load from history,
    // we don't have to jump to the top of the page. Scrollbar
    // position will be restored by the caller, based on positions
    // stored in session history.
    if (aLoadType == LOAD_HISTORY || aLoadType == LOAD_RELOAD_NORMAL) {
      return NS_OK;
    }
    // An empty anchor. Scroll to the top of the page.  Ignore the
    // return value; failure to scroll here (e.g. if there is no
    // root scrollframe) is not grounds for canceling the load!
    SetCurScrollPosEx(0, 0);
  }

  return NS_OK;
}

void
nsDocShell::SetupReferrerFromChannel(nsIChannel* aChannel)
{
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (httpChannel) {
    nsCOMPtr<nsIURI> referrer;
    nsresult rv = httpChannel->GetReferrer(getter_AddRefs(referrer));
    if (NS_SUCCEEDED(rv)) {
      SetReferrerURI(referrer);
    }
    uint32_t referrerPolicy;
    rv = httpChannel->GetReferrerPolicy(&referrerPolicy);
    if (NS_SUCCEEDED(rv)) {
      SetReferrerPolicy(referrerPolicy);
    }
  }
}

bool
nsDocShell::OnNewURI(nsIURI* aURI, nsIChannel* aChannel, nsISupports* aOwner,
                     uint32_t aLoadType, bool aFireOnLocationChange,
                     bool aAddToGlobalHistory, bool aCloneSHChildren)
{
  NS_PRECONDITION(aURI, "uri is null");
  NS_PRECONDITION(!aChannel || !aOwner, "Shouldn't have both set");

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString spec;
    aURI->GetSpec(spec);

    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]::OnNewURI(\"%s\", [%s], 0x%x)\n", this, spec.get(),
            chanName.get(), aLoadType));
  }
#endif

  bool equalUri = false;

  // Get the post data and the HTTP response code from the channel.
  uint32_t responseStatus = 0;
  nsCOMPtr<nsIInputStream> inputStream;
  if (aChannel) {
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

    // Check if the HTTPChannel is hiding under a multiPartChannel
    if (!httpChannel) {
      GetHttpChannel(aChannel, getter_AddRefs(httpChannel));
    }

    if (httpChannel) {
      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      if (uploadChannel) {
        uploadChannel->GetUploadStream(getter_AddRefs(inputStream));
      }

      // If the response status indicates an error, unlink this session
      // history entry from any entries sharing its document.
      nsresult rv = httpChannel->GetResponseStatus(&responseStatus);
      if (mLSHE && NS_SUCCEEDED(rv) && responseStatus >= 400) {
        mLSHE->AbandonBFCacheEntry();
      }
    }
  }

  // Determine if this type of load should update history.
  bool updateGHistory = !(aLoadType == LOAD_BYPASS_HISTORY ||
                          aLoadType == LOAD_ERROR_PAGE ||
                          aLoadType & LOAD_CMD_HISTORY);

  // We don't update session history on reload unless we're loading
  // an iframe in shift-reload case.
  bool updateSHistory = updateGHistory &&
                        (!(aLoadType & LOAD_CMD_RELOAD) ||
                         (IsForceReloadType(aLoadType) && IsFrame()));

  // Create SH Entry (mLSHE) only if there is a SessionHistory object in the
  // current frame or in the root docshell.
  nsCOMPtr<nsISHistory> rootSH = mSessionHistory;
  if (!rootSH) {
    // Get the handle to SH from the root docshell
    GetRootSessionHistory(getter_AddRefs(rootSH));
    if (!rootSH) {
      updateSHistory = false;
      updateGHistory = false; // XXX Why global history too?
    }
  }

  // Check if the url to be loaded is the same as the one already loaded.
  if (mCurrentURI) {
    aURI->Equals(mCurrentURI, &equalUri);
  }

#ifdef DEBUG
  bool shAvailable = (rootSH != nullptr);

  // XXX This log message is almost useless because |updateSHistory|
  //     and |updateGHistory| are not correct at this point.

  MOZ_LOG(gDocShellLog, LogLevel::Debug,
         ("  shAvailable=%i updateSHistory=%i updateGHistory=%i"
          " equalURI=%i\n",
          shAvailable, updateSHistory, updateGHistory, equalUri));

  if (shAvailable && mCurrentURI && !mOSHE && aLoadType != LOAD_ERROR_PAGE) {
    NS_ASSERTION(NS_IsAboutBlank(mCurrentURI),
                 "no SHEntry for a non-transient viewer?");
  }
#endif

  /* If the url to be loaded is the same as the one already there,
   * and the original loadType is LOAD_NORMAL, LOAD_LINK, or
   * LOAD_STOP_CONTENT, set loadType to LOAD_NORMAL_REPLACE so that
   * AddToSessionHistory() won't mess with the current SHEntry and
   * if this page has any frame children, it also will be handled
   * properly. see bug 83684
   *
   * NB: If mOSHE is null but we have a current URI, then it means
   * that we must be at the transient about:blank content viewer
   * (asserted above) and we should let the normal load continue,
   * since there's nothing to replace.
   *
   * XXX Hopefully changing the loadType at this time will not hurt
   *  anywhere. The other way to take care of sequentially repeating
   *  frameset pages is to add new methods to nsIDocShellTreeItem.
   * Hopefully I don't have to do that.
   */
  if (equalUri &&
      mOSHE &&
      (mLoadType == LOAD_NORMAL ||
       mLoadType == LOAD_LINK ||
       mLoadType == LOAD_STOP_CONTENT) &&
      !inputStream) {
    mLoadType = LOAD_NORMAL_REPLACE;
  }

  // If this is a refresh to the currently loaded url, we don't
  // have to update session or global history.
  if (mLoadType == LOAD_REFRESH && !inputStream && equalUri) {
    SetHistoryEntry(&mLSHE, mOSHE);
  }

  /* If the user pressed shift-reload, cache will create a new cache key
   * for the page. Save the new cacheKey in Session History.
   * see bug 90098
   */
  if (aChannel && IsForceReloadType(aLoadType)) {
    MOZ_ASSERT(!updateSHistory || IsFrame(),
               "We shouldn't be updating session history for forced"
               " reloads unless we're in a newly created iframe!");

    nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(aChannel));
    nsCOMPtr<nsISupports> cacheKey;
    // Get the Cache Key and store it in SH.
    if (cacheChannel) {
      cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
    }
    // If we already have a loading history entry, store the new cache key
    // in it.  Otherwise, since we're doing a reload and won't be updating
    // our history entry, store the cache key in our current history entry.
    if (mLSHE) {
      mLSHE->SetCacheKey(cacheKey);
    } else if (mOSHE) {
      mOSHE->SetCacheKey(cacheKey);
    }

    // Since we're force-reloading, clear all the sub frame history.
    ClearFrameHistory(mLSHE);
    ClearFrameHistory(mOSHE);
  }

  if (aLoadType == LOAD_RELOAD_NORMAL) {
    nsCOMPtr<nsISHEntry> currentSH;
    bool oshe = false;
    GetCurrentSHEntry(getter_AddRefs(currentSH), &oshe);
    bool dynamicallyAddedChild = false;
    if (currentSH) {
      currentSH->HasDynamicallyAddedChild(&dynamicallyAddedChild);
    }
    if (dynamicallyAddedChild) {
      ClearFrameHistory(currentSH);
    }
  }

  if (aLoadType == LOAD_REFRESH) {
    ClearFrameHistory(mLSHE);
    ClearFrameHistory(mOSHE);
  }

  if (updateSHistory) {
    // Update session history if necessary...
    if (!mLSHE && (mItemType == typeContent) && mURIResultedInDocument) {
      /* This is  a fresh page getting loaded for the first time
       *.Create a Entry for it and add it to SH, if this is the
       * rootDocShell
       */
      (void)AddToSessionHistory(aURI, aChannel, aOwner, aCloneSHChildren,
                                getter_AddRefs(mLSHE));
    }
  }

  // If this is a POST request, we do not want to include this in global
  // history.
  if (updateGHistory && aAddToGlobalHistory && !ChannelIsPost(aChannel)) {
    nsCOMPtr<nsIURI> previousURI;
    uint32_t previousFlags = 0;

    if (aLoadType & LOAD_CMD_RELOAD) {
      // On a reload request, we don't set redirecting flags.
      previousURI = aURI;
    } else {
      ExtractLastVisit(aChannel, getter_AddRefs(previousURI), &previousFlags);
    }

    // Note: We don't use |referrer| when our global history is
    //       based on IHistory.
    nsCOMPtr<nsIURI> referrer;
    // Treat referrer as null if there is an error getting it.
    (void)NS_GetReferrerFromChannel(aChannel, getter_AddRefs(referrer));

    AddURIVisit(aURI, referrer, previousURI, previousFlags, responseStatus);
  }

  // If this was a history load or a refresh,
  // update the index in SH.
  if (rootSH && (mLoadType & (LOAD_CMD_HISTORY | LOAD_CMD_RELOAD))) {
    nsCOMPtr<nsISHistoryInternal> shInternal(do_QueryInterface(rootSH));
    if (shInternal) {
      rootSH->GetIndex(&mPreviousTransIndex);
      shInternal->UpdateIndex();
      rootSH->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
      printf("Previous index: %d, Loaded index: %d\n\n",
             mPreviousTransIndex, mLoadedTransIndex);
#endif
    }
  }

  // aCloneSHChildren exactly means "we are not loading a new document".
  uint32_t locationFlags =
    aCloneSHChildren ? uint32_t(LOCATION_CHANGE_SAME_DOCUMENT) : 0;

  bool onLocationChangeNeeded = SetCurrentURI(aURI, aChannel,
                                              aFireOnLocationChange,
                                              locationFlags);
  // Make sure to store the referrer from the channel, if any
  SetupReferrerFromChannel(aChannel);
  return onLocationChangeNeeded;
}

bool
nsDocShell::OnLoadingSite(nsIChannel* aChannel, bool aFireOnLocationChange,
                          bool aAddToGlobalHistory)
{
  nsCOMPtr<nsIURI> uri;
  // If this a redirect, use the final url (uri)
  // else use the original url
  //
  // Note that this should match what documents do (see nsDocument::Reset).
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, false);

  // Pass false for aCloneSHChildren, since we're loading a new page here.
  return OnNewURI(uri, aChannel, nullptr, mLoadType, aFireOnLocationChange,
                  aAddToGlobalHistory, false);
}

void
nsDocShell::SetReferrerURI(nsIURI* aURI)
{
  mReferrerURI = aURI;  // This assigment addrefs
}

void
nsDocShell::SetReferrerPolicy(uint32_t aReferrerPolicy)
{
  mReferrerPolicy = aReferrerPolicy;
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::AddState(JS::Handle<JS::Value> aData, const nsAString& aTitle,
                     const nsAString& aURL, bool aReplace, JSContext* aCx)
{
  // Implements History.pushState and History.replaceState

  // Here's what we do, roughly in the order specified by HTML5:
  // 1. Serialize aData using structured clone.
  // 2. If the third argument is present,
  //     a. Resolve the url, relative to the first script's base URL
  //     b. If (a) fails, raise a SECURITY_ERR
  //     c. Compare the resulting absolute URL to the document's address.  If
  //        any part of the URLs difer other than the <path>, <query>, and
  //        <fragment> components, raise a SECURITY_ERR and abort.
  // 3. If !aReplace:
  //     Remove from the session history all entries after the current entry,
  //     as we would after a regular navigation, and save the current
  //     entry's scroll position (bug 590573).
  // 4. As apropriate, either add a state object entry to the session history
  //    after the current entry with the following properties, or modify the
  //    current session history entry to set
  //      a. cloned data as the state object,
  //      b. if the third argument was present, the absolute URL found in
  //         step 2
  //    Also clear the new history entry's POST data (see bug 580069).
  // 5. If aReplace is false (i.e. we're doing a pushState instead of a
  //    replaceState), notify bfcache that we've navigated to a new page.
  // 6. If the third argument is present, set the document's current address
  //    to the absolute URL found in step 2.
  //
  // It's important that this function not run arbitrary scripts after step 1
  // and before completing step 5.  For example, if a script called
  // history.back() before we completed step 5, bfcache might destroy an
  // active content viewer.  Since EvictOutOfRangeContentViewers at the end of
  // step 5 might run script, we can't just put a script blocker around the
  // critical section.
  //
  // Note that we completely ignore the aTitle parameter.

  nsresult rv;

  // Don't clobber the load type of an existing network load.
  AutoRestore<uint32_t> loadTypeResetter(mLoadType);

  // pushState effectively becomes replaceState when we've started a network
  // load but haven't adopted its document yet.  This mirrors what we do with
  // changes to the hash at this stage of the game.
  if (JustStartedNetworkLoad()) {
    aReplace = true;
  }

  nsCOMPtr<nsIDocument> document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  // Step 1: Serialize aData using structured clone.
  nsCOMPtr<nsIStructuredCloneContainer> scContainer;

  // scContainer->Init might cause arbitrary JS to run, and this code might
  // navigate the page we're on, potentially to a different origin! (bug
  // 634834)  To protect against this, we abort if our principal changes due
  // to the InitFromJSVal() call.
  {
    nsCOMPtr<nsIDocument> origDocument = GetDocument();
    if (!origDocument) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    nsCOMPtr<nsIPrincipal> origPrincipal = origDocument->NodePrincipal();

    scContainer = new nsStructuredCloneContainer();
    rv = scContainer->InitFromJSVal(aData, aCx);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> newDocument = GetDocument();
    if (!newDocument) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    nsCOMPtr<nsIPrincipal> newPrincipal = newDocument->NodePrincipal();

    bool principalsEqual = false;
    origPrincipal->Equals(newPrincipal, &principalsEqual);
    NS_ENSURE_TRUE(principalsEqual, NS_ERROR_DOM_SECURITY_ERR);
  }

  // Check that the state object isn't too long.
  // Default max length: 640k bytes.
  int32_t maxStateObjSize =
    Preferences::GetInt("browser.history.maxStateObjectSize", 0xA0000);
  if (maxStateObjSize < 0) {
    maxStateObjSize = 0;
  }

  uint64_t scSize;
  rv = scContainer->GetSerializedNBytes(&scSize);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(scSize <= (uint32_t)maxStateObjSize, NS_ERROR_ILLEGAL_VALUE);

  // Step 2: Resolve aURL
  bool equalURIs = true;
  nsCOMPtr<nsIURI> currentURI;
  if (sURIFixup && mCurrentURI) {
    rv = sURIFixup->CreateExposableURI(mCurrentURI, getter_AddRefs(currentURI));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    currentURI = mCurrentURI;
  }
  nsCOMPtr<nsIURI> oldURI = currentURI;
  nsCOMPtr<nsIURI> newURI;
  if (aURL.Length() == 0) {
    newURI = currentURI;
  } else {
    // 2a: Resolve aURL relative to mURI

    nsIURI* docBaseURI = document->GetDocBaseURI();
    if (!docBaseURI) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString spec;
    docBaseURI->GetSpec(spec);

    nsAutoCString charset;
    rv = docBaseURI->GetOriginCharset(charset);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    rv = NS_NewURI(getter_AddRefs(newURI), aURL, charset.get(), docBaseURI);

    // 2b: If 2a fails, raise a SECURITY_ERR
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // 2c: Same-origin check.
    if (!nsContentUtils::URIIsLocalFile(newURI)) {
      // In addition to checking that the security manager says that
      // the new URI has the same origin as our current URI, we also
      // check that the two URIs have the same userpass. (The
      // security manager says that |http://foo.com| and
      // |http://me@foo.com| have the same origin.)  currentURI
      // won't contain the password part of the userpass, so this
      // means that it's never valid to specify a password in a
      // pushState or replaceState URI.

      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
      NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

      // It's very important that we check that newURI is of the same
      // origin as currentURI, not docBaseURI, because a page can
      // set docBaseURI arbitrarily to any domain.
      nsAutoCString currentUserPass, newUserPass;
      NS_ENSURE_SUCCESS(currentURI->GetUserPass(currentUserPass),
                        NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(newURI->GetUserPass(newUserPass), NS_ERROR_FAILURE);
      if (NS_FAILED(secMan->CheckSameOriginURI(currentURI, newURI, true)) ||
          !currentUserPass.Equals(newUserPass)) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
    } else {
      // It's a file:// URI
      nsCOMPtr<nsIScriptObjectPrincipal> docScriptObj =
        do_QueryInterface(document);

      if (!docScriptObj) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }

      nsCOMPtr<nsIPrincipal> principal = docScriptObj->GetPrincipal();

      if (!principal ||
          NS_FAILED(principal->CheckMayLoad(newURI, true, false))) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
    }

    if (currentURI) {
      currentURI->Equals(newURI, &equalURIs);
    } else {
      equalURIs = false;
    }

  } // end of same-origin check

  // Step 3: Create a new entry in the session history. This will erase
  // all SHEntries after the new entry and make this entry the current
  // one.  This operation may modify mOSHE, which we need later, so we
  // keep a reference here.
  NS_ENSURE_TRUE(mOSHE, NS_ERROR_FAILURE);
  nsCOMPtr<nsISHEntry> oldOSHE = mOSHE;

  mLoadType = LOAD_PUSHSTATE;

  nsCOMPtr<nsISHEntry> newSHEntry;
  if (!aReplace) {
    // Save the current scroll position (bug 590573).
    nscoord cx = 0, cy = 0;
    GetCurScrollPos(ScrollOrientation_X, &cx);
    GetCurScrollPos(ScrollOrientation_Y, &cy);
    mOSHE->SetScrollPosition(cx, cy);

    // Since we're not changing which page we have loaded, pass
    // true for aCloneChildren.
    rv = AddToSessionHistory(newURI, nullptr, nullptr, true,
                             getter_AddRefs(newSHEntry));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(newSHEntry, NS_ERROR_FAILURE);

    // Link the new SHEntry to the old SHEntry's BFCache entry, since the
    // two entries correspond to the same document.
    NS_ENSURE_SUCCESS(newSHEntry->AdoptBFCacheEntry(oldOSHE), NS_ERROR_FAILURE);

    // Set the new SHEntry's title (bug 655273).
    nsString title;
    mOSHE->GetTitle(getter_Copies(title));
    newSHEntry->SetTitle(title);

    // AddToSessionHistory may not modify mOSHE.  In case it doesn't,
    // we'll just set mOSHE here.
    mOSHE = newSHEntry;

  } else {
    newSHEntry = mOSHE;
    newSHEntry->SetURI(newURI);
    newSHEntry->SetOriginalURI(newURI);
    newSHEntry->SetLoadReplace(false);
  }

  // Step 4: Modify new/original session history entry and clear its POST
  // data, if there is any.
  newSHEntry->SetStateData(scContainer);
  newSHEntry->SetPostData(nullptr);

  // If this push/replaceState changed the document's current URI and the new
  // URI differs from the old URI in more than the hash, or if the old
  // SHEntry's URI was modified in this way by a push/replaceState call
  // set URIWasModified to true for the current SHEntry (bug 669671).
  bool sameExceptHashes = true, oldURIWasModified = false;
  newURI->EqualsExceptRef(currentURI, &sameExceptHashes);
  oldOSHE->GetURIWasModified(&oldURIWasModified);
  newSHEntry->SetURIWasModified(!sameExceptHashes || oldURIWasModified);

  // Step 5: If aReplace is false, indicating that we're doing a pushState
  // rather than a replaceState, notify bfcache that we've added a page to
  // the history so it can evict content viewers if appropriate. Otherwise
  // call ReplaceEntry so that we notify nsIHistoryListeners that an entry
  // was replaced.
  nsCOMPtr<nsISHistory> rootSH;
  GetRootSessionHistory(getter_AddRefs(rootSH));
  NS_ENSURE_TRUE(rootSH, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISHistoryInternal> internalSH = do_QueryInterface(rootSH);
  NS_ENSURE_TRUE(internalSH, NS_ERROR_UNEXPECTED);

  if (!aReplace) {
    int32_t curIndex = -1;
    rv = rootSH->GetIndex(&curIndex);
    if (NS_SUCCEEDED(rv) && curIndex > -1) {
      internalSH->EvictOutOfRangeContentViewers(curIndex);
    }
  } else {
    nsCOMPtr<nsISHEntry> rootSHEntry = GetRootSHEntry(newSHEntry);

    int32_t index = -1;
    rv = rootSH->GetIndexOfEntry(rootSHEntry, &index);
    if (NS_SUCCEEDED(rv) && index > -1) {
      internalSH->ReplaceEntry(index, rootSHEntry);
    }
  }

  // Step 6: If the document's URI changed, update document's URI and update
  // global history.
  //
  // We need to call FireOnLocationChange so that the browser's address bar
  // gets updated and the back button is enabled, but we only need to
  // explicitly call FireOnLocationChange if we're not calling SetCurrentURI,
  // since SetCurrentURI will call FireOnLocationChange for us.
  //
  // Both SetCurrentURI(...) and FireDummyOnLocationChange() pass
  // nullptr for aRequest param to FireOnLocationChange(...). Such an update
  // notification is allowed only when we know docshell is not loading a new
  // document and it requires LOCATION_CHANGE_SAME_DOCUMENT flag. Otherwise,
  // FireOnLocationChange(...) breaks security UI.
  if (!equalURIs) {
    document->SetDocumentURI(newURI);
    // We can't trust SetCurrentURI to do always fire locationchange events
    // when we expect it to, so we hack around that by doing it ourselves...
    SetCurrentURI(newURI, nullptr, false, LOCATION_CHANGE_SAME_DOCUMENT);
    if (mLoadType != LOAD_ERROR_PAGE) {
      FireDummyOnLocationChange();
    }

    AddURIVisit(newURI, oldURI, oldURI, 0);

    // AddURIVisit doesn't set the title for the new URI in global history,
    // so do that here.
    if (mUseGlobalHistory && !mInPrivateBrowsing) {
      nsCOMPtr<IHistory> history = services::GetHistoryService();
      if (history) {
        history->SetURITitle(newURI, mTitle);
      } else if (mGlobalHistory) {
        mGlobalHistory->SetPageTitle(newURI, mTitle);
      }
    }

    // Inform the favicon service that our old favicon applies to this new
    // URI.
    CopyFavicon(oldURI, newURI, mInPrivateBrowsing);
  } else {
    FireDummyOnLocationChange();
  }
  document->SetStateObject(scContainer);

  return NS_OK;
}

bool
nsDocShell::ShouldAddToSessionHistory(nsIURI* aURI)
{
  // I believe none of the about: urls should go in the history. But then
  // that could just be me... If the intent is only deny about:blank then we
  // should just do a spec compare, rather than two gets of the scheme and
  // then the path.  -Gagan
  nsresult rv;
  nsAutoCString buf;

  rv = aURI->GetScheme(buf);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (buf.EqualsLiteral("about")) {
    rv = aURI->GetPath(buf);
    if (NS_FAILED(rv)) {
      return false;
    }

    if (buf.EqualsLiteral("blank") || buf.EqualsLiteral("newtab")) {
      return false;
    }
  }

  return true;
}

nsresult
nsDocShell::AddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel,
                                nsISupports* aOwner, bool aCloneChildren,
                                nsISHEntry** aNewEntry)
{
  NS_PRECONDITION(aURI, "uri is null");
  NS_PRECONDITION(!aChannel || !aOwner, "Shouldn't have both set");

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString spec;
    aURI->GetSpec(spec);

    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]::AddToSessionHistory(\"%s\", [%s])\n",
            this, spec.get(), chanName.get()));
  }
#endif

  nsresult rv = NS_OK;
  nsCOMPtr<nsISHEntry> entry;
  bool shouldPersist;

  shouldPersist = ShouldAddToSessionHistory(aURI);

  // Get a handle to the root docshell
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetSameTypeRootTreeItem(getter_AddRefs(root));
  /*
   * If this is a LOAD_FLAGS_REPLACE_HISTORY in a subframe, we use
   * the existing SH entry in the page and replace the url and
   * other vitalities.
   */
  if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY) &&
      root != static_cast<nsIDocShellTreeItem*>(this)) {
    // This is a subframe
    entry = mOSHE;
    nsCOMPtr<nsISHContainer> shContainer(do_QueryInterface(entry));
    if (shContainer) {
      int32_t childCount = 0;
      shContainer->GetChildCount(&childCount);
      // Remove all children of this entry
      for (int32_t i = childCount - 1; i >= 0; i--) {
        nsCOMPtr<nsISHEntry> child;
        shContainer->GetChildAt(i, getter_AddRefs(child));
        shContainer->RemoveChild(child);
      }
      entry->AbandonBFCacheEntry();
    }
  }

  // Create a new entry if necessary.
  if (!entry) {
    entry = do_CreateInstance(NS_SHENTRY_CONTRACTID);

    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Get the post data & referrer
  nsCOMPtr<nsIInputStream> inputStream;
  nsCOMPtr<nsIURI> originalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIURI> referrerURI;
  uint32_t referrerPolicy = mozilla::net::RP_Default;
  nsCOMPtr<nsISupports> cacheKey;
  nsCOMPtr<nsISupports> owner = aOwner;
  bool expired = false;
  bool discardLayoutState = false;
  nsCOMPtr<nsICacheInfoChannel> cacheChannel;
  if (aChannel) {
    cacheChannel = do_QueryInterface(aChannel);

    /* If there is a caching channel, get the Cache Key and store it
     * in SH.
     */
    if (cacheChannel) {
      cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
    }
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

    // Check if the httpChannel is hiding under a multipartChannel
    if (!httpChannel) {
      GetHttpChannel(aChannel, getter_AddRefs(httpChannel));
    }
    if (httpChannel) {
      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      if (uploadChannel) {
        uploadChannel->GetUploadStream(getter_AddRefs(inputStream));
      }
      httpChannel->GetOriginalURI(getter_AddRefs(originalURI));
      uint32_t loadFlags;
      aChannel->GetLoadFlags(&loadFlags);
      loadReplace = loadFlags & nsIChannel::LOAD_REPLACE;
      httpChannel->GetReferrer(getter_AddRefs(referrerURI));
      httpChannel->GetReferrerPolicy(&referrerPolicy);

      discardLayoutState = ShouldDiscardLayoutState(httpChannel);
    }
    aChannel->GetOwner(getter_AddRefs(owner));
    if (!owner) {
      nsCOMPtr<nsILoadInfo> loadInfo;
      aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
      if (loadInfo) {
        // For now keep storing just the principal in the SHEntry.
        if (loadInfo->GetLoadingSandboxed()) {
          owner = nsNullPrincipal::CreateWithInheritedAttributes(
            loadInfo->LoadingPrincipal());
          NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
        } else if (loadInfo->GetForceInheritPrincipal()) {
          owner = loadInfo->TriggeringPrincipal();
        }
      }
    }
  }

  // Title is set in nsDocShell::SetTitle()
  entry->Create(aURI,              // uri
                EmptyString(),     // Title
                inputStream,       // Post data stream
                nullptr,           // LayoutHistory state
                cacheKey,          // CacheKey
                mContentTypeHint,  // Content-type
                owner,             // Channel or provided owner
                mHistoryID,
                mDynamicallyCreated);

  entry->SetOriginalURI(originalURI);
  entry->SetLoadReplace(loadReplace);
  entry->SetReferrerURI(referrerURI);
  entry->SetReferrerPolicy(referrerPolicy);
  nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(aChannel);
  if (inStrmChan) {
    bool isSrcdocChannel;
    inStrmChan->GetIsSrcdocChannel(&isSrcdocChannel);
    if (isSrcdocChannel) {
      nsAutoString srcdoc;
      inStrmChan->GetSrcdocData(srcdoc);
      entry->SetSrcdocData(srcdoc);
      nsCOMPtr<nsIURI> baseURI;
      inStrmChan->GetBaseURI(getter_AddRefs(baseURI));
      entry->SetBaseURI(baseURI);
    }
  }
  /* If cache got a 'no-store', ask SH not to store
   * HistoryLayoutState. By default, SH will set this
   * flag to true and save HistoryLayoutState.
   */
  if (discardLayoutState) {
    entry->SetSaveLayoutStateFlag(false);
  }
  if (cacheChannel) {
    // Check if the page has expired from cache
    uint32_t expTime = 0;
    cacheChannel->GetCacheTokenExpirationTime(&expTime);
    uint32_t now = PRTimeToSeconds(PR_Now());
    if (expTime <= now) {
      expired = true;
    }
  }
  if (expired) {
    entry->SetExpirationStatus(true);
  }

  if (root == static_cast<nsIDocShellTreeItem*>(this) && mSessionHistory) {
    // If we need to clone our children onto the new session
    // history entry, do so now.
    if (aCloneChildren && mOSHE) {
      uint32_t cloneID;
      mOSHE->GetID(&cloneID);
      nsCOMPtr<nsISHEntry> newEntry;
      CloneAndReplace(mOSHE, this, cloneID, entry, true,
                      getter_AddRefs(newEntry));
      NS_ASSERTION(entry == newEntry,
                   "The new session history should be in the new entry");
    }

    // This is the root docshell
    if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {
      // Replace current entry in session history.
      int32_t index = 0;
      mSessionHistory->GetIndex(&index);
      nsCOMPtr<nsISHistoryInternal> shPrivate =
        do_QueryInterface(mSessionHistory);
      // Replace the current entry with the new entry
      if (shPrivate) {
        rv = shPrivate->ReplaceEntry(index, entry);
      }
    } else {
      // Add to session history
      nsCOMPtr<nsISHistoryInternal> shPrivate =
        do_QueryInterface(mSessionHistory);
      NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
      mSessionHistory->GetIndex(&mPreviousTransIndex);
      rv = shPrivate->AddEntry(entry, shouldPersist);
      mSessionHistory->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
      printf("Previous index: %d, Loaded index: %d\n\n",
             mPreviousTransIndex, mLoadedTransIndex);
#endif
    }
  } else {
    // This is a subframe.
    if (!mOSHE || !LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {
      rv = AddChildSHEntryToParent(entry, mChildOffset, aCloneChildren);
    }
  }

  // Return the new SH entry...
  if (aNewEntry) {
    *aNewEntry = nullptr;
    if (NS_SUCCEEDED(rv)) {
      entry.forget(aNewEntry);
    }
  }

  return rv;
}

nsresult
nsDocShell::LoadHistoryEntry(nsISHEntry* aEntry, uint32_t aLoadType)
{
  if (!IsNavigationAllowed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> originalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIInputStream> postData;
  nsCOMPtr<nsIURI> referrerURI;
  uint32_t referrerPolicy;
  nsAutoCString contentType;
  nsCOMPtr<nsISupports> owner;

  NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetOriginalURI(getter_AddRefs(originalURI)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetLoadReplace(&loadReplace),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetReferrerURI(getter_AddRefs(referrerURI)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetReferrerPolicy(&referrerPolicy),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetContentType(contentType), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetOwner(getter_AddRefs(owner)), NS_ERROR_FAILURE);

  // Calling CreateAboutBlankContentViewer can set mOSHE to null, and if
  // that's the only thing holding a ref to aEntry that will cause aEntry to
  // die while we're loading it.  So hold a strong ref to aEntry here, just
  // in case.
  nsCOMPtr<nsISHEntry> kungFuDeathGrip(aEntry);
  bool isJS;
  nsresult rv = uri->SchemeIs("javascript", &isJS);
  if (NS_FAILED(rv) || isJS) {
    // We're loading a URL that will execute script from inside asyncOpen.
    // Replace the current document with about:blank now to prevent
    // anything from the current document from leaking into any JavaScript
    // code in the URL.
    nsCOMPtr<nsIPrincipal> prin = do_QueryInterface(owner);
    // Don't cache the presentation if we're going to just reload the
    // current entry. Caching would lead to trying to save the different
    // content viewers in the same nsISHEntry object.
    rv = CreateAboutBlankContentViewer(prin, nullptr, aEntry != mOSHE);

    if (NS_FAILED(rv)) {
      // The creation of the intermittent about:blank content
      // viewer failed for some reason (potentially because the
      // user prevented it). Interrupt the history load.
      return NS_OK;
    }

    if (!owner) {
      // Ensure that we have an owner.  Otherwise javascript: URIs will
      // pick it up from the about:blank page we just loaded, and we
      // don't really want even that in this case.
      owner = nsNullPrincipal::Create();
      NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  /* If there is a valid postdata *and* the user pressed
   * reload or shift-reload, take user's permission before we
   * repost the data to the server.
   */
  if ((aLoadType & LOAD_CMD_RELOAD) && postData) {
    bool repost;
    rv = ConfirmRepost(&repost);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // If the user pressed cancel in the dialog, return.  We're done here.
    if (!repost) {
      return NS_BINDING_ABORTED;
    }
  }

  // Do not inherit owner from document (security-critical!);
  uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;

  nsAutoString srcdoc;
  bool isSrcdoc;
  nsCOMPtr<nsIURI> baseURI;
  aEntry->GetIsSrcdocEntry(&isSrcdoc);
  if (isSrcdoc) {
    aEntry->GetSrcdocData(srcdoc);
    aEntry->GetBaseURI(getter_AddRefs(baseURI));
    flags |= INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  } else {
    srcdoc = NullString();
  }

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  rv = InternalLoad(uri,
                    originalURI,
                    loadReplace,
                    referrerURI,
                    referrerPolicy,
                    owner,
                    flags,
                    nullptr,            // No window target
                    contentType.get(),  // Type hint
                    NullString(),       // No forced file download
                    postData,           // Post data stream
                    nullptr,            // No headers stream
                    aLoadType,          // Load type
                    aEntry,             // SHEntry
                    true,
                    srcdoc,
                    nullptr,            // Source docshell, see comment above
                    baseURI,
                    nullptr,            // No nsIDocShell
                    nullptr);           // No nsIRequest
  return rv;
}

NS_IMETHODIMP
nsDocShell::GetShouldSaveLayoutState(bool* aShould)
{
  *aShould = false;
  if (mOSHE) {
    // Don't capture historystate and save it in history
    // if the page asked not to do so.
    mOSHE->GetSaveLayoutStateFlag(aShould);
  }

  return NS_OK;
}

nsresult
nsDocShell::PersistLayoutHistoryState()
{
  nsresult rv = NS_OK;

  if (mOSHE) {
    nsCOMPtr<nsIPresShell> shell = GetPresShell();
    if (shell) {
      nsCOMPtr<nsILayoutHistoryState> layoutState;
      rv = shell->CaptureHistoryState(getter_AddRefs(layoutState));
    }
  }

  return rv;
}

/* static */ nsresult
nsDocShell::WalkHistoryEntries(nsISHEntry* aRootEntry,
                               nsDocShell* aRootShell,
                               WalkHistoryEntriesFunc aCallback,
                               void* aData)
{
  NS_ENSURE_TRUE(aRootEntry, NS_ERROR_FAILURE);

  nsCOMPtr<nsISHContainer> container(do_QueryInterface(aRootEntry));
  if (!container) {
    return NS_ERROR_FAILURE;
  }

  int32_t childCount;
  container->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; i++) {
    nsCOMPtr<nsISHEntry> childEntry;
    container->GetChildAt(i, getter_AddRefs(childEntry));
    if (!childEntry) {
      // childEntry can be null for valid reasons, for example if the
      // docshell at index i never loaded anything useful.
      // Remember to clone also nulls in the child array (bug 464064).
      aCallback(nullptr, nullptr, i, aData);
      continue;
    }

    nsDocShell* childShell = nullptr;
    if (aRootShell) {
      // Walk the children of aRootShell and see if one of them
      // has srcChild as a SHEntry.
      nsTObserverArray<nsDocLoader*>::ForwardIterator iter(
        aRootShell->mChildList);
      while (iter.HasMore()) {
        nsDocShell* child = static_cast<nsDocShell*>(iter.GetNext());

        if (child->HasHistoryEntry(childEntry)) {
          childShell = child;
          break;
        }
      }
    }
    nsresult rv = aCallback(childEntry, childShell, i, aData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// callback data for WalkHistoryEntries
struct MOZ_STACK_CLASS CloneAndReplaceData
{
  CloneAndReplaceData(uint32_t aCloneID, nsISHEntry* aReplaceEntry,
                      bool aCloneChildren, nsISHEntry* aDestTreeParent)
    : cloneID(aCloneID)
    , cloneChildren(aCloneChildren)
    , replaceEntry(aReplaceEntry)
    , destTreeParent(aDestTreeParent)
  {
  }

  uint32_t cloneID;
  bool cloneChildren;
  nsISHEntry* replaceEntry;
  nsISHEntry* destTreeParent;
  nsCOMPtr<nsISHEntry> resultEntry;
};

/* static */ nsresult
nsDocShell::CloneAndReplaceChild(nsISHEntry* aEntry, nsDocShell* aShell,
                                 int32_t aEntryIndex, void* aData)
{
  nsCOMPtr<nsISHEntry> dest;

  CloneAndReplaceData* data = static_cast<CloneAndReplaceData*>(aData);
  uint32_t cloneID = data->cloneID;
  nsISHEntry* replaceEntry = data->replaceEntry;

  nsCOMPtr<nsISHContainer> container = do_QueryInterface(data->destTreeParent);
  if (!aEntry) {
    if (container) {
      container->AddChild(nullptr, aEntryIndex);
    }
    return NS_OK;
  }

  uint32_t srcID;
  aEntry->GetID(&srcID);

  nsresult rv = NS_OK;
  if (srcID == cloneID) {
    // Replace the entry
    dest = replaceEntry;
  } else {
    // Clone the SHEntry...
    rv = aEntry->Clone(getter_AddRefs(dest));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  dest->SetIsSubFrame(true);

  if (srcID != cloneID || data->cloneChildren) {
    // Walk the children
    CloneAndReplaceData childData(cloneID, replaceEntry,
                                  data->cloneChildren, dest);
    rv = WalkHistoryEntries(aEntry, aShell,
                            CloneAndReplaceChild, &childData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (srcID != cloneID && aShell) {
    aShell->SwapHistoryEntries(aEntry, dest);
  }

  if (container) {
    container->AddChild(dest, aEntryIndex);
  }

  data->resultEntry = dest;
  return rv;
}

/* static */ nsresult
nsDocShell::CloneAndReplace(nsISHEntry* aSrcEntry,
                            nsDocShell* aSrcShell,
                            uint32_t aCloneID,
                            nsISHEntry* aReplaceEntry,
                            bool aCloneChildren,
                            nsISHEntry** aResultEntry)
{
  NS_ENSURE_ARG_POINTER(aResultEntry);
  NS_ENSURE_TRUE(aReplaceEntry, NS_ERROR_FAILURE);

  CloneAndReplaceData data(aCloneID, aReplaceEntry, aCloneChildren, nullptr);
  nsresult rv = CloneAndReplaceChild(aSrcEntry, aSrcShell, 0, &data);

  data.resultEntry.swap(*aResultEntry);
  return rv;
}

void
nsDocShell::SwapHistoryEntries(nsISHEntry* aOldEntry, nsISHEntry* aNewEntry)
{
  if (aOldEntry == mOSHE) {
    mOSHE = aNewEntry;
  }

  if (aOldEntry == mLSHE) {
    mLSHE = aNewEntry;
  }
}

struct SwapEntriesData
{
  nsDocShell* ignoreShell;     // constant; the shell to ignore
  nsISHEntry* destTreeRoot;    // constant; the root of the dest tree
  nsISHEntry* destTreeParent;  // constant; the node under destTreeRoot
                               // whose children will correspond to aEntry
};

nsresult
nsDocShell::SetChildHistoryEntry(nsISHEntry* aEntry, nsDocShell* aShell,
                                 int32_t aEntryIndex, void* aData)
{
  SwapEntriesData* data = static_cast<SwapEntriesData*>(aData);
  nsDocShell* ignoreShell = data->ignoreShell;

  if (!aShell || aShell == ignoreShell) {
    return NS_OK;
  }

  nsISHEntry* destTreeRoot = data->destTreeRoot;

  nsCOMPtr<nsISHEntry> destEntry;
  nsCOMPtr<nsISHContainer> container = do_QueryInterface(data->destTreeParent);

  if (container) {
    // aEntry is a clone of some child of destTreeParent, but since the
    // trees aren't necessarily in sync, we'll have to locate it.
    // Note that we could set aShell's entry to null if we don't find a
    // corresponding entry under destTreeParent.

    uint32_t targetID, id;
    aEntry->GetID(&targetID);

    // First look at the given index, since this is the common case.
    nsCOMPtr<nsISHEntry> entry;
    container->GetChildAt(aEntryIndex, getter_AddRefs(entry));
    if (entry && NS_SUCCEEDED(entry->GetID(&id)) && id == targetID) {
      destEntry.swap(entry);
    } else {
      int32_t childCount;
      container->GetChildCount(&childCount);
      for (int32_t i = 0; i < childCount; ++i) {
        container->GetChildAt(i, getter_AddRefs(entry));
        if (!entry) {
          continue;
        }

        entry->GetID(&id);
        if (id == targetID) {
          destEntry.swap(entry);
          break;
        }
      }
    }
  } else {
    destEntry = destTreeRoot;
  }

  aShell->SwapHistoryEntries(aEntry, destEntry);

  // Now handle the children of aEntry.
  SwapEntriesData childData = { ignoreShell, destTreeRoot, destEntry };
  return WalkHistoryEntries(aEntry, aShell, SetChildHistoryEntry, &childData);
}

static nsISHEntry*
GetRootSHEntry(nsISHEntry* aEntry)
{
  nsCOMPtr<nsISHEntry> rootEntry = aEntry;
  nsISHEntry* result = nullptr;
  while (rootEntry) {
    result = rootEntry;
    result->GetParent(getter_AddRefs(rootEntry));
  }

  return result;
}

void
nsDocShell::SetHistoryEntry(nsCOMPtr<nsISHEntry>* aPtr, nsISHEntry* aEntry)
{
  // We need to sync up the docshell and session history trees for
  // subframe navigation.  If the load was in a subframe, we forward up to
  // the root docshell, which will then recursively sync up all docshells
  // to their corresponding entries in the new session history tree.
  // If we don't do this, then we can cache a content viewer on the wrong
  // cloned entry, and subsequently restore it at the wrong time.

  nsISHEntry* newRootEntry = GetRootSHEntry(aEntry);
  if (newRootEntry) {
    // newRootEntry is now the new root entry.
    // Find the old root entry as well.

    // Need a strong ref. on |oldRootEntry| so it isn't destroyed when
    // SetChildHistoryEntry() does SwapHistoryEntries() (bug 304639).
    nsCOMPtr<nsISHEntry> oldRootEntry = GetRootSHEntry(*aPtr);
    if (oldRootEntry) {
      nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
      GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));
      nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(rootAsItem);
      if (rootShell) { // if we're the root just set it, nothing to swap
        SwapEntriesData data = { this, newRootEntry };
        nsIDocShell* rootIDocShell = static_cast<nsIDocShell*>(rootShell);
        nsDocShell* rootDocShell = static_cast<nsDocShell*>(rootIDocShell);

#ifdef DEBUG
        nsresult rv =
#endif
        SetChildHistoryEntry(oldRootEntry, rootDocShell, 0, &data);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetChildHistoryEntry failed");
      }
    }
  }

  *aPtr = aEntry;
}

nsresult
nsDocShell::GetRootSessionHistory(nsISHistory** aReturn)
{
  nsresult rv;

  nsCOMPtr<nsIDocShellTreeItem> root;
  // Get the root docshell
  rv = GetSameTypeRootTreeItem(getter_AddRefs(root));
  // QI to nsIWebNavigation
  nsCOMPtr<nsIWebNavigation> rootAsWebnav(do_QueryInterface(root));
  if (rootAsWebnav) {
    // Get the handle to SH from the root docshell
    rv = rootAsWebnav->GetSessionHistory(aReturn);
  }
  return rv;
}

nsresult
nsDocShell::GetHttpChannel(nsIChannel* aChannel, nsIHttpChannel** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChannel) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(aChannel));
  if (multiPartChannel) {
    nsCOMPtr<nsIChannel> baseChannel;
    multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(baseChannel));
    *aReturn = httpChannel;
    NS_IF_ADDREF(*aReturn);
  }
  return NS_OK;
}

bool
nsDocShell::ShouldDiscardLayoutState(nsIHttpChannel* aChannel)
{
  // By default layout State will be saved.
  if (!aChannel) {
    return false;
  }

  // figure out if SH should be saving layout state
  nsCOMPtr<nsISupports> securityInfo;
  bool noStore = false, noCache = false;
  aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  aChannel->IsNoStoreResponse(&noStore);
  aChannel->IsNoCacheResponse(&noCache);

  return (noStore || (noCache && securityInfo));
}

NS_IMETHODIMP
nsDocShell::GetEditor(nsIEditor** aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  if (!mEditorData) {
    *aEditor = nullptr;
    return NS_OK;
  }

  return mEditorData->GetEditor(aEditor);
}

NS_IMETHODIMP
nsDocShell::SetEditor(nsIEditor* aEditor)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return mEditorData->SetEditor(aEditor);
}

NS_IMETHODIMP
nsDocShell::GetEditable(bool* aEditable)
{
  NS_ENSURE_ARG_POINTER(aEditable);
  *aEditable = mEditorData && mEditorData->GetEditable();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasEditingSession(bool* aHasEditingSession)
{
  NS_ENSURE_ARG_POINTER(aHasEditingSession);

  if (mEditorData) {
    nsCOMPtr<nsIEditingSession> editingSession;
    mEditorData->GetEditingSession(getter_AddRefs(editingSession));
    *aHasEditingSession = (editingSession.get() != nullptr);
  } else {
    *aHasEditingSession = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::MakeEditable(bool aInWaitForUriLoad)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return mEditorData->MakeEditable(aInWaitForUriLoad);
}

bool
nsDocShell::ChannelIsPost(nsIChannel* aChannel)
{
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (!httpChannel) {
    return false;
  }

  nsAutoCString method;
  httpChannel->GetRequestMethod(method);
  return method.EqualsLiteral("POST");
}

void
nsDocShell::ExtractLastVisit(nsIChannel* aChannel,
                             nsIURI** aURI,
                             uint32_t* aChannelRedirectFlags)
{
  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(aChannel));
  if (!props) {
    return;
  }

  nsresult rv = props->GetPropertyAsInterface(
    NS_LITERAL_STRING("docshell.previousURI"),
    NS_GET_IID(nsIURI),
    reinterpret_cast<void**>(aURI));

  if (NS_FAILED(rv)) {
    // There is no last visit for this channel, so this must be the first
    // link.  Link the visit to the referrer of this request, if any.
    // Treat referrer as null if there is an error getting it.
    (void)NS_GetReferrerFromChannel(aChannel, aURI);
  } else {
    rv = props->GetPropertyAsUint32(NS_LITERAL_STRING("docshell.previousFlags"),
                                    aChannelRedirectFlags);

    NS_WARN_IF_FALSE(
      NS_SUCCEEDED(rv),
      "Could not fetch previous flags, URI will be treated like referrer");
  }
}

void
nsDocShell::SaveLastVisit(nsIChannel* aChannel,
                          nsIURI* aURI,
                          uint32_t aChannelRedirectFlags)
{
  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(aChannel));
  if (!props || !aURI) {
    return;
  }

  props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.previousURI"),
                                aURI);
  props->SetPropertyAsUint32(NS_LITERAL_STRING("docshell.previousFlags"),
                             aChannelRedirectFlags);
}

void
nsDocShell::AddURIVisit(nsIURI* aURI,
                        nsIURI* aReferrerURI,
                        nsIURI* aPreviousURI,
                        uint32_t aChannelRedirectFlags,
                        uint32_t aResponseStatus)
{
  MOZ_ASSERT(aURI, "Visited URI is null!");
  MOZ_ASSERT(mLoadType != LOAD_ERROR_PAGE &&
             mLoadType != LOAD_BYPASS_HISTORY,
             "Do not add error or bypass pages to global history");

  // Only content-type docshells save URI visits.  Also don't do
  // anything here if we're not supposed to use global history.
  if (mItemType != typeContent || !mUseGlobalHistory || mInPrivateBrowsing) {
    return;
  }

  nsCOMPtr<IHistory> history = services::GetHistoryService();

  if (history) {
    uint32_t visitURIFlags = 0;

    if (!IsFrame()) {
      visitURIFlags |= IHistory::TOP_LEVEL;
    }

    if (aChannelRedirectFlags & nsIChannelEventSink::REDIRECT_TEMPORARY) {
      visitURIFlags |= IHistory::REDIRECT_TEMPORARY;
    } else if (aChannelRedirectFlags & nsIChannelEventSink::REDIRECT_PERMANENT) {
      visitURIFlags |= IHistory::REDIRECT_PERMANENT;
    }

    if (aResponseStatus >= 300 && aResponseStatus < 400) {
      visitURIFlags |= IHistory::REDIRECT_SOURCE;
    }
    // Errors 400-501 and 505 are considered unrecoverable, in the sense a
    // simple retry attempt by the user is unlikely to solve them.
    // 408 is special cased, since may actually indicate a temporary
    // connection problem.
    else if (aResponseStatus != 408 &&
             ((aResponseStatus >= 400 && aResponseStatus <= 501) ||
              aResponseStatus == 505)) {
      visitURIFlags |= IHistory::UNRECOVERABLE_ERROR;
    }

    (void)history->VisitURI(aURI, aPreviousURI, visitURIFlags);
  } else if (mGlobalHistory) {
    // Falls back to sync global history interface.
    (void)mGlobalHistory->AddURI(aURI,
                                 !!aChannelRedirectFlags,
                                 !IsFrame(),
                                 aReferrerURI);
  }
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::SetLoadType(uint32_t aLoadType)
{
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadType(uint32_t* aLoadType)
{
  *aLoadType = mLoadType;
  return NS_OK;
}

nsresult
nsDocShell::ConfirmRepost(bool* aRepost)
{
  nsCOMPtr<nsIPrompt> prompter;
  CallGetInterface(this, static_cast<nsIPrompt**>(getter_AddRefs(prompter)));
  if (!prompter) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (!stringBundleService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> appBundle;
  nsresult rv = stringBundleService->CreateBundle(kAppstringsBundleURL,
                                                  getter_AddRefs(appBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> brandBundle;
  rv = stringBundleService->CreateBundle(kBrandBundleURL,
                                         getter_AddRefs(brandBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(prompter && brandBundle && appBundle,
               "Unable to set up repost prompter.");

  nsXPIDLString brandName;
  rv = brandBundle->GetStringFromName(MOZ_UTF16("brandShortName"),
                                      getter_Copies(brandName));

  nsXPIDLString msgString, button0Title;
  if (NS_FAILED(rv)) { // No brand, use the generic version.
    rv = appBundle->GetStringFromName(MOZ_UTF16("confirmRepostPrompt"),
                                      getter_Copies(msgString));
  } else {
    // Brand available - if the app has an override file with formatting, the
    // app name will be included. Without an override, the prompt will look
    // like the generic version.
    const char16_t* formatStrings[] = { brandName.get() };
    rv = appBundle->FormatStringFromName(MOZ_UTF16("confirmRepostPrompt"),
                                         formatStrings,
                                         ArrayLength(formatStrings),
                                         getter_Copies(msgString));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = appBundle->GetStringFromName(MOZ_UTF16("resendButton.label"),
                                    getter_Copies(button0Title));
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t buttonPressed;
  // The actual value here is irrelevant, but we can't pass an invalid
  // bool through XPConnect.
  bool checkState = false;
  rv = prompter->ConfirmEx(
    nullptr, msgString.get(),
    (nsIPrompt::BUTTON_POS_0 * nsIPrompt::BUTTON_TITLE_IS_STRING) +
      (nsIPrompt::BUTTON_POS_1 * nsIPrompt::BUTTON_TITLE_CANCEL),
    button0Title.get(), nullptr, nullptr, nullptr, &checkState, &buttonPressed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aRepost = (buttonPressed == 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPromptAndStringBundle(nsIPrompt** aPrompt,
                                     nsIStringBundle** aStringBundle)
{
  NS_ENSURE_SUCCESS(GetInterface(NS_GET_IID(nsIPrompt), (void**)aPrompt),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_TRUE(stringBundleService, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(
    stringBundleService->CreateBundle(kAppstringsBundleURL, aStringBundle),
    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent,
                           int32_t* aOffset)
{
  NS_ENSURE_ARG_POINTER(aChild || aParent);

  nsCOMPtr<nsIDOMNodeList> childNodes;
  NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(childNodes, NS_ERROR_FAILURE);

  int32_t i = 0;

  for (; true; i++) {
    nsCOMPtr<nsIDOMNode> childNode;
    NS_ENSURE_SUCCESS(childNodes->Item(i, getter_AddRefs(childNode)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(childNode, NS_ERROR_FAILURE);

    if (childNode.get() == aChild) {
      *aOffset = i;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsIScrollableFrame*
nsDocShell::GetRootScrollFrame()
{
  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, nullptr);

  return shell->GetRootScrollFrameAsScrollableExternal();
}

NS_IMETHODIMP
nsDocShell::EnsureScriptEnvironment()
{
  if (mScriptGlobal) {
    return NS_OK;
  }

  if (mIsBeingDestroyed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

#ifdef DEBUG
  NS_ASSERTION(!mInEnsureScriptEnv,
               "Infinite loop! Calling EnsureScriptEnvironment() from "
               "within EnsureScriptEnvironment()!");

  // Yeah, this isn't re-entrant safe, but that's ok since if we
  // re-enter this method, we'll infinitely loop...
  AutoRestore<bool> boolSetter(mInEnsureScriptEnv);
  mInEnsureScriptEnv = true;
#endif

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  NS_ENSURE_TRUE(browserChrome, NS_ERROR_NOT_AVAILABLE);

  uint32_t chromeFlags;
  browserChrome->GetChromeFlags(&chromeFlags);

  bool isModalContentWindow =
    (mItemType == typeContent) &&
    (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL_CONTENT_WINDOW);
  // There can be various other content docshells associated with the
  // top-level window, like sidebars. Make sure that we only create an
  // nsGlobalModalWindow for the primary content shell.
  if (isModalContentWindow) {
    nsCOMPtr<nsIDocShellTreeItem> primaryItem;
    nsresult rv =
      mTreeOwner->GetPrimaryContentShell(getter_AddRefs(primaryItem));
    NS_ENSURE_SUCCESS(rv, rv);
    isModalContentWindow = (primaryItem == this);
  }

  // If our window is modal and we're not opened as chrome, make
  // this window a modal content window.
  mScriptGlobal =
    NS_NewScriptGlobalObject(mItemType == typeChrome, isModalContentWindow);
  MOZ_ASSERT(mScriptGlobal);

  mScriptGlobal->SetDocShell(this);

  // Ensure the script object is set up to run script.
  return mScriptGlobal->EnsureScriptEnvironment();
}

NS_IMETHODIMP
nsDocShell::EnsureEditorData()
{
  bool openDocHasDetachedEditor = mOSHE && mOSHE->HasDetachedEditor();
  if (!mEditorData && !mIsBeingDestroyed && !openDocHasDetachedEditor) {
    // We shouldn't recreate the editor data if it already exists, or
    // we're shutting down, or we already have a detached editor data
    // stored in the session history. We should only have one editordata
    // per docshell.
    mEditorData = new nsDocShellEditorData(this);
  }

  return mEditorData ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsDocShell::EnsureTransferableHookData()
{
  if (!mTransferableHookData) {
    mTransferableHookData = new nsTransferableHookData();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::EnsureFind()
{
  nsresult rv;
  if (!mFind) {
    mFind = do_CreateInstance("@mozilla.org/embedcomp/find;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // we promise that the nsIWebBrowserFind that we return has been set
  // up to point to the focused, or content window, so we have to
  // set that up each time.

  nsIScriptGlobalObject* scriptGO = GetScriptGlobalObject();
  NS_ENSURE_TRUE(scriptGO, NS_ERROR_UNEXPECTED);

  // default to our window
  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(scriptGO);
  nsCOMPtr<nsPIDOMWindow> windowToSearch;
  nsFocusManager::GetFocusedDescendant(ourWindow, true,
                                       getter_AddRefs(windowToSearch));

  nsCOMPtr<nsIWebBrowserFindInFrames> findInFrames = do_QueryInterface(mFind);
  if (!findInFrames) {
    return NS_ERROR_NO_INTERFACE;
  }

  rv = findInFrames->SetRootSearchFrame(ourWindow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = findInFrames->SetCurrentSearchFrame(windowToSearch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

bool
nsDocShell::IsFrame()
{
  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetSameTypeParent(getter_AddRefs(parent));
  return !!parent;
}

NS_IMETHODIMP
nsDocShell::IsBeingDestroyed(bool* aDoomed)
{
  NS_ENSURE_ARG(aDoomed);
  *aDoomed = mIsBeingDestroyed;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsExecutingOnLoadHandler(bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = mIsExecutingOnLoadHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLayoutHistoryState(nsILayoutHistoryState** aLayoutHistoryState)
{
  if (mOSHE) {
    mOSHE->GetLayoutHistoryState(aLayoutHistoryState);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLayoutHistoryState(nsILayoutHistoryState* aLayoutHistoryState)
{
  if (mOSHE) {
    mOSHE->SetLayoutHistoryState(aLayoutHistoryState);
  }
  return NS_OK;
}

nsRefreshTimer::nsRefreshTimer()
  : mDelay(0), mRepeat(false), mMetaRefresh(false)
{
}

nsRefreshTimer::~nsRefreshTimer()
{
}

NS_IMPL_ADDREF(nsRefreshTimer)
NS_IMPL_RELEASE(nsRefreshTimer)

NS_INTERFACE_MAP_BEGIN(nsRefreshTimer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP
nsRefreshTimer::Notify(nsITimer* aTimer)
{
  NS_ASSERTION(mDocShell, "DocShell is somehow null");

  if (mDocShell && aTimer) {
    // Get the delay count to determine load type
    uint32_t delay = 0;
    aTimer->GetDelay(&delay);
    mDocShell->ForceRefreshURIFromTimer(mURI, delay, mMetaRefresh, aTimer);
  }
  return NS_OK;
}

nsDocShell::InterfaceRequestorProxy::InterfaceRequestorProxy(
    nsIInterfaceRequestor* aRequestor)
{
  if (aRequestor) {
    mWeakPtr = do_GetWeakReference(aRequestor);
  }
}

nsDocShell::InterfaceRequestorProxy::~InterfaceRequestorProxy()
{
  mWeakPtr = nullptr;
}

NS_IMPL_ISUPPORTS(nsDocShell::InterfaceRequestorProxy, nsIInterfaceRequestor)

NS_IMETHODIMP
nsDocShell::InterfaceRequestorProxy::GetInterface(const nsIID& aIID,
                                                  void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  nsCOMPtr<nsIInterfaceRequestor> ifReq = do_QueryReferent(mWeakPtr);
  if (ifReq) {
    return ifReq->GetInterface(aIID, aSink);
  }
  *aSink = nullptr;
  return NS_NOINTERFACE;
}

nsresult
nsDocShell::SetBaseUrlForWyciwyg(nsIContentViewer* aContentViewer)
{
  if (!aContentViewer) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  if (sURIFixup) {
    rv = sURIFixup->CreateExposableURI(mCurrentURI, getter_AddRefs(baseURI));
  }

  // Get the current document and set the base uri
  if (baseURI) {
    nsIDocument* document = aContentViewer->GetDocument();
    if (document) {
      rv = document->SetBaseURI(baseURI);
    }
  }
  return rv;
}

//*****************************************************************************
// nsDocShell::nsIAuthPromptProvider
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetAuthPrompt(uint32_t aPromptReason, const nsIID& aIID,
                          void** aResult)
{
  // a priority prompt request will override a false mAllowAuth setting
  bool priorityPrompt = (aPromptReason == PROMPT_PROXY);

  if (!mAllowAuth && !priorityPrompt) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // we're either allowing auth, or it's a proxy request
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureScriptEnvironment();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.

  return wwatch->GetPrompt(mScriptGlobal, aIID,
                           reinterpret_cast<void**>(aResult));
}

//*****************************************************************************
// nsDocShell::nsILoadContext
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetAssociatedWindow(nsIDOMWindow** aWindow)
{
  CallGetInterface(this, aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopWindow(nsIDOMWindow** aWindow)
{
  nsCOMPtr<nsIDOMWindow> win = GetWindow();
  if (win) {
    win->GetTop(aWindow);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopFrameElement(nsIDOMElement** aElement)
{
  *aElement = nullptr;
  nsCOMPtr<nsIDOMWindow> win = GetWindow();
  if (!win) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> top;
  win->GetScriptableTop(getter_AddRefs(top));
  NS_ENSURE_TRUE(top, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindow> piTop = do_QueryInterface(top);
  NS_ENSURE_TRUE(piTop, NS_ERROR_FAILURE);

  // GetFrameElementInternal, /not/ GetScriptableFrameElement -- if |top| is
  // inside <iframe mozbrowser>, we want to return the iframe, not null.
  // And we want to cross the content/chrome boundary.
  nsCOMPtr<nsIDOMElement> elt =
    do_QueryInterface(piTop->GetFrameElementInternal());
  elt.forget(aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetNestedFrameId(uint64_t* aId)
{
  *aId = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::IsAppOfType(uint32_t aAppType, bool* aIsOfType)
{
  RefPtr<nsDocShell> shell = this;
  while (shell) {
    uint32_t type;
    shell->GetAppType(&type);
    if (type == aAppType) {
      *aIsOfType = true;
      return NS_OK;
    }
    shell = shell->GetParentDocshell();
  }

  *aIsOfType = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsContent(bool* aIsContent)
{
  *aIsContent = (mItemType == typeContent);
  return NS_OK;
}

bool
nsDocShell::IsOKToLoadURI(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Must have a URI!");

  if (!mFiredUnloadEvent) {
    return true;
  }

  if (!mLoadingURI) {
    return false;
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  return secMan &&
         NS_SUCCEEDED(secMan->CheckSameOriginURI(aURI, mLoadingURI, false));
}

//
// Routines for selection and clipboard
//
nsresult
nsDocShell::GetControllerForCommand(const char* aCommand,
                                    nsIController** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIWindowRoot> root = mScriptGlobal->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllerForCommand(aCommand, aResult);
}

NS_IMETHODIMP
nsDocShell::IsCommandEnabled(const char* aCommand, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (controller) {
    rv = controller->IsCommandEnabled(aCommand, aResult);
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::DoCommand(const char* aCommand)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (controller) {
    rv = controller->DoCommand(aCommand);
  }

  return rv;
}

nsresult
nsDocShell::EnsureCommandHandler()
{
  if (!mCommandManager) {
    nsCOMPtr<nsPICommandUpdater> commandUpdater =
      do_CreateInstance("@mozilla.org/embedcomp/command-manager;1");
    if (!commandUpdater) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMWindow> domWindow = GetWindow();
    nsresult rv = commandUpdater->Init(domWindow);
    if (NS_SUCCEEDED(rv)) {
      mCommandManager = do_QueryInterface(commandUpdater);
    }
  }

  return mCommandManager ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::CanCutSelection(bool* aResult)
{
  return IsCommandEnabled("cmd_cut", aResult);
}

NS_IMETHODIMP
nsDocShell::CanCopySelection(bool* aResult)
{
  return IsCommandEnabled("cmd_copy", aResult);
}

NS_IMETHODIMP
nsDocShell::CanCopyLinkLocation(bool* aResult)
{
  return IsCommandEnabled("cmd_copyLink", aResult);
}

NS_IMETHODIMP
nsDocShell::CanCopyImageLocation(bool* aResult)
{
  return IsCommandEnabled("cmd_copyImageLocation", aResult);
}

NS_IMETHODIMP
nsDocShell::CanCopyImageContents(bool* aResult)
{
  return IsCommandEnabled("cmd_copyImageContents", aResult);
}

NS_IMETHODIMP
nsDocShell::CanPaste(bool* aResult)
{
  return IsCommandEnabled("cmd_paste", aResult);
}

NS_IMETHODIMP
nsDocShell::CutSelection(void)
{
  return DoCommand("cmd_cut");
}

NS_IMETHODIMP
nsDocShell::CopySelection(void)
{
  return DoCommand("cmd_copy");
}

NS_IMETHODIMP
nsDocShell::CopyLinkLocation(void)
{
  return DoCommand("cmd_copyLink");
}

NS_IMETHODIMP
nsDocShell::CopyImageLocation(void)
{
  return DoCommand("cmd_copyImageLocation");
}

NS_IMETHODIMP
nsDocShell::CopyImageContents(void)
{
  return DoCommand("cmd_copyImageContents");
}

NS_IMETHODIMP
nsDocShell::Paste(void)
{
  return DoCommand("cmd_paste");
}

NS_IMETHODIMP
nsDocShell::SelectAll(void)
{
  return DoCommand("cmd_selectAll");
}

//
// SelectNone
//
// Collapses the current selection, insertion point ends up at beginning
// of previous selection.
//
NS_IMETHODIMP
nsDocShell::SelectNone(void)
{
  return DoCommand("cmd_selectNone");
}

// link handling

class OnLinkClickEvent : public nsRunnable
{
public:
  OnLinkClickEvent(nsDocShell* aHandler, nsIContent* aContent,
                   nsIURI* aURI,
                   const char16_t* aTargetSpec,
                   const nsAString& aFileName,
                   nsIInputStream* aPostDataStream,
                   nsIInputStream* aHeadersDataStream,
                   bool aIsTrusted);

  NS_IMETHOD Run()
  {
    nsAutoPopupStatePusher popupStatePusher(mPopupState);

    AutoJSAPI jsapi;
    if (mIsTrusted || jsapi.Init(mContent->OwnerDoc()->GetScopeObject())) {
      mHandler->OnLinkClickSync(mContent, mURI,
                                mTargetSpec.get(), mFileName,
                                mPostDataStream, mHeadersDataStream,
                                nullptr, nullptr);
    }
    return NS_OK;
  }

private:
  RefPtr<nsDocShell> mHandler;
  nsCOMPtr<nsIURI> mURI;
  nsString mTargetSpec;
  nsString mFileName;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  nsCOMPtr<nsIInputStream> mHeadersDataStream;
  nsCOMPtr<nsIContent> mContent;
  PopupControlState mPopupState;
  bool mIsTrusted;
};

OnLinkClickEvent::OnLinkClickEvent(nsDocShell* aHandler,
                                   nsIContent* aContent,
                                   nsIURI* aURI,
                                   const char16_t* aTargetSpec,
                                   const nsAString& aFileName,
                                   nsIInputStream* aPostDataStream,
                                   nsIInputStream* aHeadersDataStream,
                                   bool aIsTrusted)
  : mHandler(aHandler)
  , mURI(aURI)
  , mTargetSpec(aTargetSpec)
  , mFileName(aFileName)
  , mPostDataStream(aPostDataStream)
  , mHeadersDataStream(aHeadersDataStream)
  , mContent(aContent)
  , mPopupState(mHandler->mScriptGlobal->GetPopupControlState())
  , mIsTrusted(aIsTrusted)
{
}

NS_IMETHODIMP
nsDocShell::OnLinkClick(nsIContent* aContent,
                        nsIURI* aURI,
                        const char16_t* aTargetSpec,
                        const nsAString& aFileName,
                        nsIInputStream* aPostDataStream,
                        nsIInputStream* aHeadersDataStream,
                        bool aIsTrusted)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  if (!IsNavigationAllowed() || !IsOKToLoadURI(aURI)) {
    return NS_OK;
  }

  // On history navigation through Back/Forward buttons, don't execute
  // automatic JavaScript redirection such as |anchorElement.click()| or
  // |formElement.submit()|.
  //
  // XXX |formElement.submit()| bypasses this checkpoint because it calls
  //     nsDocShell::OnLinkClickSync(...) instead.
  if (ShouldBlockLoadingForBackButton()) {
    return NS_OK;
  }

  if (aContent->IsEditable()) {
    return NS_OK;
  }

  nsresult rv = NS_ERROR_FAILURE;
  nsAutoString target;

  nsCOMPtr<nsIWebBrowserChrome3> browserChrome3 = do_GetInterface(mTreeOwner);
  if (browserChrome3) {
    nsCOMPtr<nsIDOMNode> linkNode = do_QueryInterface(aContent);
    nsAutoString oldTarget(aTargetSpec);
    rv = browserChrome3->OnBeforeLinkTraversal(oldTarget, aURI,
                                               linkNode, mIsAppTab, target);
  }

  if (NS_FAILED(rv)) {
    target = aTargetSpec;
  }

  nsCOMPtr<nsIRunnable> ev =
    new OnLinkClickEvent(this, aContent, aURI, target.get(), aFileName,
                         aPostDataStream, aHeadersDataStream, aIsTrusted);
  return NS_DispatchToCurrentThread(ev);
}

NS_IMETHODIMP
nsDocShell::OnLinkClickSync(nsIContent* aContent,
                            nsIURI* aURI,
                            const char16_t* aTargetSpec,
                            const nsAString& aFileName,
                            nsIInputStream* aPostDataStream,
                            nsIInputStream* aHeadersDataStream,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest)
{
  // Initialize the DocShell / Request
  if (aDocShell) {
    *aDocShell = nullptr;
  }
  if (aRequest) {
    *aRequest = nullptr;
  }

  if (!IsNavigationAllowed() || !IsOKToLoadURI(aURI)) {
    return NS_OK;
  }

  // XXX When the linking node was HTMLFormElement, it is synchronous event.
  //     That is, the caller of this method is not |OnLinkClickEvent::Run()|
  //     but |HTMLFormElement::SubmitSubmission(...)|.
  if (aContent->IsHTMLElement(nsGkAtoms::form) &&
      ShouldBlockLoadingForBackButton()) {
    return NS_OK;
  }

  if (aContent->IsEditable()) {
    return NS_OK;
  }

  {
    // defer to an external protocol handler if necessary...
    nsCOMPtr<nsIExternalProtocolService> extProtService =
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
    if (extProtService) {
      nsAutoCString scheme;
      aURI->GetScheme(scheme);
      if (!scheme.IsEmpty()) {
        // if the URL scheme does not correspond to an exposed protocol, then we
        // need to hand this link click over to the external protocol handler.
        bool isExposed;
        nsresult rv =
          extProtService->IsExposedProtocol(scheme.get(), &isExposed);
        if (NS_SUCCEEDED(rv) && !isExposed) {
          return extProtService->LoadURI(aURI, this);
        }
      }
    }
  }

  uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;
  if (IsElementAnchor(aContent)) {
    MOZ_ASSERT(aContent->IsHTMLElement());
    nsAutoString referrer;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, referrer);
    nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(referrer);
    while (tok.hasMoreTokens()) {
      if (tok.nextToken().LowerCaseEqualsLiteral("noreferrer")) {
        flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER |
                 INTERNAL_LOAD_FLAGS_NO_OPENER;
        break;
      }
    }
  }

  // Get the owner document of the link that was clicked, this will be
  // the document that the link is in, or the last document that the
  // link was in. From that document, we'll get the URI to use as the
  // referer, since the current URI in this docshell may be a
  // new document that we're in the process of loading.
  nsCOMPtr<nsIDocument> refererDoc = aContent->OwnerDoc();
  NS_ENSURE_TRUE(refererDoc, NS_ERROR_UNEXPECTED);

  // Now check that the refererDoc's inner window is the current inner
  // window for mScriptGlobal.  If it's not, then we don't want to
  // follow this link.
  nsPIDOMWindow* refererInner = refererDoc->GetInnerWindow();
  NS_ENSURE_TRUE(refererInner, NS_ERROR_UNEXPECTED);
  if (!mScriptGlobal ||
      mScriptGlobal->GetCurrentInnerWindow() != refererInner) {
    // We're no longer the current inner window
    return NS_OK;
  }

  nsCOMPtr<nsIURI> referer = refererDoc->GetDocumentURI();
  uint32_t refererPolicy = refererDoc->GetReferrerPolicy();

  // get referrer attribute from clicked link and parse it
  // if per element referrer is enabled, the element referrer overrules
  // the document wide referrer
  if (IsElementAnchor(aContent)) {
    net::ReferrerPolicy refPolEnum = aContent->AsElement()->GetReferrerPolicy();
    if (refPolEnum != net::RP_Unset) {
      refererPolicy = refPolEnum;
    }
  }

  // referer could be null here in some odd cases, but that's ok,
  // we'll just load the link w/o sending a referer in those cases.

  nsAutoString target(aTargetSpec);

  // If this is an anchor element, grab its type property to use as a hint
  nsAutoString typeHint;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(aContent));
  if (anchor) {
    anchor->GetType(typeHint);
    NS_ConvertUTF16toUTF8 utf8Hint(typeHint);
    nsAutoCString type, dummy;
    NS_ParseRequestContentType(utf8Hint, type, dummy);
    CopyUTF8toUTF16(type, typeHint);
  }

  // Clone the URI now, in case a content policy or something messes
  // with it under InternalLoad; we do _not_ want to change the URI
  // our caller passed in.
  nsCOMPtr<nsIURI> clonedURI;
  aURI->Clone(getter_AddRefs(clonedURI));
  if (!clonedURI) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = InternalLoad(clonedURI,                 // New URI
                             nullptr,                   // Original URI
                             false,                     // LoadReplace
                             referer,                   // Referer URI
                             refererPolicy,             // Referer policy
                             aContent->NodePrincipal(), // Owner is our node's
                                                        // principal
                             flags,
                             target.get(),              // Window target
                             NS_LossyConvertUTF16toASCII(typeHint).get(),
                             aFileName,                 // Download as file
                             aPostDataStream,           // Post data stream
                             aHeadersDataStream,        // Headers stream
                             LOAD_LINK,                 // Load type
                             nullptr,                   // No SHEntry
                             true,                      // first party site
                             NullString(),              // No srcdoc
                             this,                      // We are the source
                             nullptr,                   // baseURI not needed
                             aDocShell,                 // DocShell out-param
                             aRequest);                 // Request out-param
  if (NS_SUCCEEDED(rv)) {
    DispatchPings(this, aContent, aURI, referer, refererPolicy);
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::OnOverLink(nsIContent* aContent,
                       nsIURI* aURI,
                       const char16_t* aTargetSpec)
{
  if (aContent->IsEditable()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebBrowserChrome2> browserChrome2 = do_GetInterface(mTreeOwner);
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  if (!browserChrome2) {
    browserChrome = do_GetInterface(mTreeOwner);
    if (!browserChrome) {
      return rv;
    }
  }

  nsCOMPtr<nsITextToSubURI> textToSubURI =
    do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // use url origin charset to unescape the URL
  nsAutoCString charset;
  rv = aURI->GetOriginCharset(charset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString uStr;
  rv = textToSubURI->UnEscapeURIForUI(charset, spec, uStr);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::net::PredictorPredict(aURI, mCurrentURI,
                                 nsINetworkPredictor::PREDICT_LINK,
                                 this, nullptr);

  if (browserChrome2) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
    rv = browserChrome2->SetStatusWithContext(nsIWebBrowserChrome::STATUS_LINK,
                                              uStr, element);
  } else {
    rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, uStr.get());
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::OnLeaveLink()
{
  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  nsresult rv = NS_ERROR_FAILURE;

  if (browserChrome) {
    rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                                  EmptyString().get());
  }
  return rv;
}

bool
nsDocShell::ShouldBlockLoadingForBackButton()
{
  if (!(mLoadType & LOAD_CMD_HISTORY) ||
      EventStateManager::IsHandlingUserInput() ||
      !Preferences::GetBool("accessibility.blockjsredirection")) {
    return false;
  }

  bool canGoForward = false;
  GetCanGoForward(&canGoForward);
  return canGoForward;
}

bool
nsDocShell::PluginsAllowedInCurrentDoc()
{
  bool pluginsAllowed = false;

  if (!mContentViewer) {
    return false;
  }

  nsIDocument* doc = mContentViewer->GetDocument();
  if (!doc) {
    return false;
  }

  doc->GetAllowPlugins(&pluginsAllowed);
  return pluginsAllowed;
}

//----------------------------------------------------------------------
// Web Shell Services API

// This functions is only called when a new charset is detected in loading a
// document. Its name should be changed to "CharsetReloadDocument"
NS_IMETHODIMP
nsDocShell::ReloadDocument(const char* aCharset, int32_t aSource)
{
  // XXX hack. keep the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv) {
    int32_t hint;
    cv->GetHintCharacterSetSource(&hint);
    if (aSource > hint) {
      nsCString charset(aCharset);
      cv->SetHintCharacterSet(charset);
      cv->SetHintCharacterSetSource(aSource);
      if (eCharsetReloadRequested != mCharsetReloadState) {
        mCharsetReloadState = eCharsetReloadRequested;
        return Reload(LOAD_FLAGS_CHARSET_CHANGE);
      }
    }
  }
  // return failure if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}

NS_IMETHODIMP
nsDocShell::StopDocumentLoad(void)
{
  if (eCharsetReloadRequested != mCharsetReloadState) {
    Stop(nsIWebNavigation::STOP_ALL);
    return NS_OK;
  }
  // return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}

NS_IMETHODIMP
nsDocShell::GetPrintPreview(nsIWebBrowserPrint** aPrintPreview)
{
  *aPrintPreview = nullptr;
#if NS_PRINT_PREVIEW
  nsCOMPtr<nsIDocumentViewerPrint> print = do_QueryInterface(mContentViewer);
  if (!print || !print->IsInitializedForPrintPreview()) {
    Stop(nsIWebNavigation::STOP_ALL);
    nsCOMPtr<nsIPrincipal> principal = nsNullPrincipal::Create();
    NS_ENSURE_STATE(principal);
    nsresult rv = CreateAboutBlankContentViewer(principal, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
    print = do_QueryInterface(mContentViewer);
    NS_ENSURE_STATE(print);
    print->InitializeForPrintPreview();
  }
  nsCOMPtr<nsIWebBrowserPrint> result = do_QueryInterface(print);
  result.forget(aPrintPreview);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#ifdef DEBUG
unsigned long nsDocShell::gNumberOfDocShells = 0;
#endif

NS_IMETHODIMP
nsDocShell::GetCanExecuteScripts(bool* aResult)
{
  *aResult = mCanExecuteScripts;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsApp(uint32_t aOwnAppId)
{
  mOwnOrContainingAppId = aOwnAppId;
  if (aOwnAppId != nsIScriptSecurityManager::NO_APP_ID &&
      aOwnAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    mFrameType = eFrameTypeApp;
  } else {
    mFrameType = eFrameTypeRegular;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsBrowserInsideApp(uint32_t aContainingAppId)
{
  mOwnOrContainingAppId = aContainingAppId;
  mFrameType = eFrameTypeBrowser;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsBrowserElement(bool* aIsBrowser)
{
  *aIsBrowser = (mFrameType == eFrameTypeBrowser);
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsApp(bool* aIsApp)
{
  *aIsApp = (mFrameType == eFrameTypeApp);
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsBrowserOrApp(bool* aIsBrowserOrApp)
{
  switch (mFrameType) {
    case eFrameTypeRegular:
      *aIsBrowserOrApp = false;
      break;
    case eFrameTypeBrowser:
    case eFrameTypeApp:
      *aIsBrowserOrApp = true;
      break;
  }

  return NS_OK;
}

nsDocShell::FrameType
nsDocShell::GetInheritedFrameType()
{
  if (mFrameType != eFrameTypeRegular) {
    return mFrameType;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  GetSameTypeParent(getter_AddRefs(parentAsItem));

  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
  if (!parent) {
    return eFrameTypeRegular;
  }

  return static_cast<nsDocShell*>(parent.get())->GetInheritedFrameType();
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  *aIsInBrowserElement = (GetInheritedFrameType() == eFrameTypeBrowser);
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsInBrowserOrApp(bool* aIsInBrowserOrApp)
{
  switch (GetInheritedFrameType()) {
    case eFrameTypeRegular:
      *aIsInBrowserOrApp = false;
      break;
    case eFrameTypeBrowser:
    case eFrameTypeApp:
      *aIsInBrowserOrApp = true;
      break;
  }

  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetAppId(uint32_t* aAppId)
{
  if (mOwnOrContainingAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    *aAppId = mOwnOrContainingAppId;
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> parent;
  GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent));

  if (!parent) {
    *aAppId = nsIScriptSecurityManager::NO_APP_ID;
    return NS_OK;
  }

  return parent->GetAppId(aAppId);
}

OriginAttributes
nsDocShell::GetOriginAttributes()
{
  OriginAttributes attrs;
  RefPtr<nsDocShell> parent = GetParentDocshell();
  if (parent) {
    attrs.InheritFromDocShellParent(parent->GetOriginAttributes());
  }

  if (mOwnOrContainingAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    attrs.mAppId = mOwnOrContainingAppId;
  }

  if (mFrameType == eFrameTypeBrowser) {
    attrs.mInBrowser = true;
  }

  return attrs;
}

NS_IMETHODIMP
nsDocShell::GetOriginAttributes(JS::MutableHandle<JS::Value> aVal)
{
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  MOZ_ASSERT(cx);

  OriginAttributes attrs = GetOriginAttributes();
  bool ok = ToJSValue(cx, attrs, aVal);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAppManifestURL(nsAString& aAppManifestURL)
{
  uint32_t appId = nsIDocShell::GetAppId();
  if (appId != nsIScriptSecurityManager::NO_APP_ID &&
      appId != nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    nsCOMPtr<nsIAppsService> appsService =
      do_GetService(APPS_SERVICE_CONTRACTID);
    NS_ASSERTION(appsService, "No AppsService available");
    appsService->GetManifestURLByLocalId(appId, aAppManifestURL);
  } else {
    aAppManifestURL.SetLength(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAsyncPanZoomEnabled(bool* aOut)
{
  if (nsIPresShell* presShell = GetPresShell()) {
    *aOut = presShell->AsyncPanZoomEnabled();
    return NS_OK;
  }

  // If we don't have a presShell, fall back to the default platform value of
  // whether or not APZ is enabled.
  *aOut = gfxPlatform::AsyncPanZoomEnabled();
  return NS_OK;
}

bool
nsDocShell::HasUnloadedParent()
{
  RefPtr<nsDocShell> parent = GetParentDocshell();
  while (parent) {
    bool inUnload = false;
    parent->GetIsInUnload(&inUnload);
    if (inUnload) {
      return true;
    }
    parent = parent->GetParentDocshell();
  }
  return false;
}

bool
nsDocShell::IsInvisible()
{
  return mInvisible;
}

void
nsDocShell::SetInvisible(bool aInvisible)
{
  mInvisible = aInvisible;
}

void
nsDocShell::SetOpener(nsITabParent* aOpener)
{
  mOpener = do_GetWeakReference(aOpener);
}

nsITabParent*
nsDocShell::GetOpener()
{
  nsCOMPtr<nsITabParent> opener(do_QueryReferent(mOpener));
  return opener;
}

void
nsDocShell::NotifyJSRunToCompletionStart(const char* aReason,
                                         const char16_t* aFunctionName,
                                         const char16_t* aFilename,
                                         const uint32_t aLineNumber)
{
  bool timelineOn = nsIDocShell::GetRecordProfileTimelineMarkers();

  // If first start, mark interval start.
  if (timelineOn && mJSRunToCompletionDepth == 0) {
    UniquePtr<TimelineMarker> marker = MakeUnique<JavascriptTimelineMarker>(
      aReason, aFunctionName, aFilename, aLineNumber, MarkerTracingType::START);
    TimelineConsumers::AddMarkerForDocShell(this, Move(marker));
  }
  mJSRunToCompletionDepth++;
}

void
nsDocShell::NotifyJSRunToCompletionStop()
{
  bool timelineOn = nsIDocShell::GetRecordProfileTimelineMarkers();

  // If last stop, mark interval end.
  mJSRunToCompletionDepth--;
  if (timelineOn && mJSRunToCompletionDepth == 0) {
    TimelineConsumers::AddMarkerForDocShell(this, "Javascript", MarkerTracingType::END);
  }
}

void
nsDocShell::MaybeNotifyKeywordSearchLoading(const nsString& aProvider,
                                            const nsString& aKeyword)
{
  if (aProvider.IsEmpty()) {
    return;
  }

  if (XRE_IsContentProcess()) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (contentChild) {
      contentChild->SendNotifyKeywordSearchLoading(aProvider, aKeyword);
    }
    return;
  }

#ifdef MOZ_TOOLKIT_SEARCH
  nsCOMPtr<nsIBrowserSearchService> searchSvc =
    do_GetService("@mozilla.org/browser/search-service;1");
  if (searchSvc) {
    nsCOMPtr<nsISearchEngine> searchEngine;
    searchSvc->GetEngineByName(aProvider, getter_AddRefs(searchEngine));
    if (searchEngine) {
      nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
      if (obsSvc) {
        // Note that "keyword-search" refers to a search via the url
        // bar, not a bookmarks keyword search.
        obsSvc->NotifyObservers(searchEngine, "keyword-search", aKeyword.get());
      }
    }
  }
#endif
}

NS_IMETHODIMP
nsDocShell::ShouldPrepareForIntercept(nsIURI* aURI, bool aIsNonSubresourceRequest,
                                      bool* aShouldIntercept)
{
  *aShouldIntercept = false;
  // Preffed off.
  if (!nsContentUtils::ServiceWorkerInterceptionEnabled()) {
    return NS_OK;
  }

  // No in private browsing
  if (mInPrivateBrowsing) {
    return NS_OK;
  }

  if (mSandboxFlags) {
    // If we're sandboxed, don't intercept.
    return NS_OK;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_OK;
  }

  nsresult result;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID, &result);
  NS_ENSURE_SUCCESS(result, result);

  if (mCurrentURI &&
      nsContentUtils::CookiesBehavior() == nsICookieService::BEHAVIOR_REJECT_FOREIGN) {
    nsAutoCString uriSpec;
    mCurrentURI->GetSpec(uriSpec);
    if (!(uriSpec.EqualsLiteral("about:blank"))) {
      // Reject the interception of third-party iframes if the cookie behaviour
      // is set to reject all third-party cookies (1). In case that this pref
      // is not set or can't be read, we default to allow all cookies (0) as
      // this is the default value in all.js.
      bool isThirdPartyURI = true;
      result = thirdPartyUtil->IsThirdPartyURI(mCurrentURI, aURI,
                                               &isThirdPartyURI);
      if (NS_FAILED(result)) {
          return result;
      }

      if (isThirdPartyURI) {
        return NS_OK;
      }
    }
  }

  if (aIsNonSubresourceRequest) {
    OriginAttributes attrs(GetAppId(), GetIsInBrowserElement());
    *aShouldIntercept = swm->IsAvailable(attrs, aURI);
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ErrorResult rv;
  *aShouldIntercept = swm->IsControlled(doc, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

namespace {

class FetchEventDispatcher final : public nsIFetchEventDispatcher
{
public:
  FetchEventDispatcher(nsIInterceptedChannel* aChannel,
                       nsIRunnable* aContinueRunnable)
    : mChannel(aChannel)
    , mContinueRunnable(aContinueRunnable)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFETCHEVENTDISPATCHER

private:
  ~FetchEventDispatcher()
  {
  }

  nsCOMPtr<nsIInterceptedChannel> mChannel;
  nsCOMPtr<nsIRunnable> mContinueRunnable;
};

NS_IMPL_ISUPPORTS(FetchEventDispatcher, nsIFetchEventDispatcher)

NS_IMETHODIMP
FetchEventDispatcher::Dispatch()
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    mChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
    return NS_OK;
  }

  ErrorResult error;
  swm->DispatchPreparedFetchEvent(mChannel, mContinueRunnable, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

}

NS_IMETHODIMP
nsDocShell::ChannelIntercepted(nsIInterceptedChannel* aChannel,
                               nsIFetchEventDispatcher** aFetchDispatcher)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    aChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = aChannel->GetChannel(getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc;

  bool isSubresourceLoad = !nsContentUtils::IsNonSubresourceRequest(channel);
  if (isSubresourceLoad) {
    doc = GetDocument();
    if (!doc) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  bool isReload = mLoadType & LOAD_CMD_RELOAD;

  OriginAttributes attrs(GetAppId(), GetIsInBrowserElement());

  ErrorResult error;
  nsCOMPtr<nsIRunnable> runnable =
    swm->PrepareFetchEvent(attrs, doc, aChannel, isReload, isSubresourceLoad, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  MOZ_ASSERT(runnable);
  RefPtr<FetchEventDispatcher> dispatcher =
    new FetchEventDispatcher(aChannel, runnable);
  dispatcher.forget(aFetchDispatcher);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPaymentRequestId(const nsAString& aPaymentRequestId)
{
  mPaymentRequestId = aPaymentRequestId;
  return NS_OK;
}

nsString
nsDocShell::GetInheritedPaymentRequestId()
{
  if (!mPaymentRequestId.IsEmpty()) {
    return mPaymentRequestId;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  GetSameTypeParent(getter_AddRefs(parentAsItem));

  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
  if (!parent) {
    return mPaymentRequestId;
  }
  return static_cast<nsDocShell*>(parent.get())->GetInheritedPaymentRequestId();
}

NS_IMETHODIMP
nsDocShell::GetPaymentRequestId(nsAString& aPaymentRequestId)
{
  aPaymentRequestId = GetInheritedPaymentRequestId();
  return NS_OK;
}

bool
nsDocShell::InFrameSwap()
{
  RefPtr<nsDocShell> shell = this;
  do {
    if (shell->mInFrameSwap) {
      return true;
    }
    shell = shell->GetParentDocshell();
  } while (shell);
  return false;
}

NS_IMETHODIMP
nsDocShell::IssueWarning(uint32_t aWarning, bool aAsError)
{
  nsCOMPtr<nsIDocument> doc = mContentViewer->GetDocument();
  if (doc) {
    doc->WarnOnceAbout(nsIDocument::DeprecatedOperations(aWarning), aAsError);
  }
  return NS_OK;
}
