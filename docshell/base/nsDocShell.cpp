/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 tw=80 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Util.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds (but only for some things)
#define FORCE_PR_LOG 1
#endif

#include "nsIBrowserDOMWindow.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMStorage.h"
#include "nsPIDOMStorage.h"
#include "nsIContentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsCURILoader.h"
#include "nsURILoader.h"
#include "nsDocShellCID.h"
#include "nsLayoutCID.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsNetUtil.h"
#include "nsRect.h"
#include "prprf.h"
#include "prenv.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsPoint.h"
#include "nsGfxCIID.h"
#include "nsIObserverService.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsTextFormatter.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIUploadChannel.h"
#include "nsISecurityEventSink.h"
#include "mozilla/FunctionTimer.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScrollableFrame.h"
#include "nsContentPolicyUtils.h" // NS_CheckContentLoadPolicy(...)
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"
#include "nsISeekableStream.h"
#include "nsAutoPtr.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsDOMJSUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScriptChannel.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsITimedChannel.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsCPrefetchService.h"
#include "nsJSON.h"
#include "IHistory.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Attributes.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  


// Local Includes
#include "nsDocShell.h"
#include "nsDocShellLoadInfo.h"
#include "nsCDefaultURIFixup.h"
#include "nsDocShellEnumerator.h"
#include "nsSHistory.h"
#include "nsDocShellEditorData.h"

// Helper Classes
#include "nsDOMError.h"
#include "nsEscape.h"

// Interfaces Needed
#include "nsIUploadChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIWebProgress.h"
#include "nsILayoutHistoryState.h"
#include "nsITimer.h"
#include "nsISHistoryInternal.h"
#include "nsIPrincipal.h"
#include "nsIFileURL.h"
#include "nsIHistoryEntry.h"
#include "nsISHistoryListener.h"
#include "nsIWindowWatcher.h"
#include "nsIPromptFactory.h"
#include "nsIObserver.h"
#include "nsINestedURI.h"
#include "nsITransportSecurityInfo.h"
#include "nsINSSErrorsService.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIPermissionManager.h"
#include "nsStreamUtils.h"
#include "nsIController.h"
#include "nsPICommandUpdater.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIWebBrowserChrome3.h"
#include "nsITabChild.h"
#include "nsIStrictTransportSecurityService.h"
#include "nsStructuredCloneContainer.h"
#include "nsIStructuredCloneContainer.h"
#include "nsIFaviconService.h"
#include "mozIAsyncFavicons.h"

// Editor-related
#include "nsIEditingSession.h"

#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIDOMDocument.h"
#include "nsICachingChannel.h"
#include "nsICacheVisitor.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIMultiPartChannel.h"
#include "nsIWyciwygChannel.h"

// For reporting errors with the console service.
// These can go away if error reporting is propagated up past nsDocShell.
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

#include "nsFocusManager.h"

#include "nsITextToSubURI.h"

#include "nsIJARChannel.h"

#include "prlog.h"
#include "prmem.h"

#include "nsISelectionDisplay.h"

#include "nsIGlobalHistory2.h"

#include "nsEventStateManager.h"

#include "nsIFrame.h"

// for embedding
#include "nsIWebBrowserChromeFocus.h"

#if NS_PRINT_PREVIEW
#include "nsIDocumentViewerPrint.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsPluginError.h"
#include "nsContentUtils.h"
#include "nsContentErrors.h"
#include "nsIChannelPolicy.h"
#include "nsIContentSecurityPolicy.h"

#include "nsXULAppAPI.h"

#include "nsDOMNavigationTiming.h"
#include "nsITimedChannel.h"
#include "mozilla/StartupTimeline.h"
#include "nsIFrameMessageManager.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
//#define DEBUG_DOCSHELL_FOCUS
#define DEBUG_PAGE_CACHE
#endif

using namespace mozilla;

// Number of documents currently loading
static PRInt32 gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static PRInt32 gDocShellCount = 0;

// Global count of docshells with the private attribute set
static PRUint32 gNumberOfPrivateDocShells = 0;

// Global reference to the URI fixup service.
nsIURIFixup *nsDocShell::sURIFixup = 0;

// True means we validate window targets to prevent frameset
// spoofing. Initialize this to a non-bolean value so we know to check
// the pref on the creation of the first docshell.
static PRUint32 gValidateOrigin = 0xffffffff;

// Hint for native dispatch of events on how long to delay after 
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
#define NS_EVENT_STARVATION_DELAY_HINT 2000

// This is needed for displaying an error message 
// when navigation is attempted on a document when printing
// The value arbitrary as long as it doesn't conflict with
// any of the other values in the errors in DisplayLoadError
#define NS_ERROR_DOCUMENT_IS_PRINTMODE  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GENERAL,2001)

#ifdef PR_LOGGING
#ifdef DEBUG
static PRLogModuleInfo* gDocShellLog;
#endif
static PRLogModuleInfo* gDocShellLeakLog;
#endif

const char kBrandBundleURL[]      = "chrome://branding/locale/brand.properties";
const char kAppstringsBundleURL[] = "chrome://global/locale/appstrings.properties";

static void
FavorPerformanceHint(bool perfOverStarvation, PRUint32 starvationDelay)
{
    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    if (appShell)
        appShell->FavorPerformanceHint(perfOverStarvation, starvationDelay);
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
PingsEnabled(PRInt32 *maxPerLink, bool *requireSameHost)
{
  bool allow = Preferences::GetBool(PREF_PINGS_ENABLED, false);

  *maxPerLink = 1;
  *requireSameHost = true;

  if (allow) {
    Preferences::GetInt(PREF_PINGS_MAX_PER_LINK, maxPerLink);
    Preferences::GetBool(PREF_PINGS_REQUIRE_SAME_HOST, requireSameHost);
  }

  return allow;
}

static bool
CheckPingURI(nsIURI* uri, nsIContent* content)
{
  if (!uri)
    return false;

  // Check with nsIScriptSecurityManager
  nsCOMPtr<nsIScriptSecurityManager> ssmgr =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(ssmgr, false);

  nsresult rv =
    ssmgr->CheckLoadURIWithPrincipal(content->NodePrincipal(), uri,
                                     nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return false;
  }

  // Ignore non-HTTP(S)
  bool match;
  if ((NS_FAILED(uri->SchemeIs("http", &match)) || !match) &&
      (NS_FAILED(uri->SchemeIs("https", &match)) || !match)) {
    return false;
  }

  // Check with contentpolicy
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_PING,
                                 uri,
                                 content->NodePrincipal(),
                                 content,
                                 EmptyCString(), // mime hint
                                 nsnull, //extra
                                 &shouldLoad);
  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad);
}

typedef void (* ForEachPingCallback)(void *closure, nsIContent *content,
                                     nsIURI *uri, nsIIOService *ios);

static void
ForEachPing(nsIContent *content, ForEachPingCallback callback, void *closure)
{
  // NOTE: Using nsIDOMHTMLAnchorElement::GetPing isn't really worth it here
  //       since we'd still need to parse the resulting string.  Instead, we
  //       just parse the raw attribute.  It might be nice if the content node
  //       implemented an interface that exposed an enumeration of nsIURIs.

  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  if (!content->IsHTML())
    return;
  nsIAtom *nameAtom = content->Tag();
  if (!nameAtom->Equals(NS_LITERAL_STRING("a")) &&
      !nameAtom->Equals(NS_LITERAL_STRING("area")))
    return;

  nsCOMPtr<nsIAtom> pingAtom = do_GetAtom("ping");
  if (!pingAtom)
    return;

  nsAutoString value;
  content->GetAttr(kNameSpaceID_None, pingAtom, value);
  if (value.IsEmpty())
    return;

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (!ios)
    return;

  nsIDocument *doc = content->OwnerDoc();

  // value contains relative URIs split on spaces (U+0020)
  const PRUnichar *start = value.BeginReading();
  const PRUnichar *end   = value.EndReading();
  const PRUnichar *iter  = start;
  for (;;) {
    if (iter < end && *iter != ' ') {
      ++iter;
    } else {  // iter is pointing at either end or a space
      while (*start == ' ' && start < iter)
        ++start;
      if (iter != start) {
        nsCOMPtr<nsIURI> uri, baseURI = content->GetBaseURI();
        ios->NewURI(NS_ConvertUTF16toUTF8(Substring(start, iter)),
                    doc->GetDocumentCharacterSet().get(),
                    baseURI, getter_AddRefs(uri));
        if (CheckPingURI(uri, content)) {
          callback(closure, content, uri, ios);
        }
      }
      start = iter = iter + 1;
      if (iter >= end)
        break;
    }
  }
}

//----------------------------------------------------------------------

// We wait this many milliseconds before killing the ping channel...
#define PING_TIMEOUT 10000

static void
OnPingTimeout(nsITimer *timer, void *closure)
{
  nsILoadGroup *loadGroup = static_cast<nsILoadGroup *>(closure);
  loadGroup->Cancel(NS_ERROR_ABORT);
  loadGroup->Release();
}

// Check to see if two URIs have the same host or not
static bool
IsSameHost(nsIURI *uri1, nsIURI *uri2)
{
  nsCAutoString host1, host2;
  uri1->GetAsciiHost(host1);
  uri2->GetAsciiHost(host2);
  return host1.Equals(host2);
}

class nsPingListener MOZ_FINAL : public nsIStreamListener
                               , public nsIInterfaceRequestor
                               , public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  nsPingListener(bool requireSameHost, nsIContent* content)
    : mRequireSameHost(requireSameHost),
      mContent(content)
  {}

private:
  bool mRequireSameHost;
  nsCOMPtr<nsIContent> mContent;
};

NS_IMPL_ISUPPORTS4(nsPingListener, nsIStreamListener, nsIRequestObserver,
                   nsIInterfaceRequestor, nsIChannelEventSink)

NS_IMETHODIMP
nsPingListener::OnStartRequest(nsIRequest *request, nsISupports *context)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPingListener::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                nsIInputStream *stream, PRUint32 offset,
                                PRUint32 count)
{
  PRUint32 result;
  return stream->ReadSegments(NS_DiscardSegment, nsnull, count, &result);
}

NS_IMETHODIMP
nsPingListener::OnStopRequest(nsIRequest *request, nsISupports *context,
                              nsresult status)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPingListener::GetInterface(const nsIID &iid, void **result)
{
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *result = (nsIChannelEventSink *) this;
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsPingListener::AsyncOnChannelRedirect(nsIChannel *oldChan, nsIChannel *newChan,
                                       PRUint32 flags,
                                       nsIAsyncVerifyRedirectCallback *callback)
{
  nsCOMPtr<nsIURI> newURI;
  newChan->GetURI(getter_AddRefs(newURI));

  if (!CheckPingURI(newURI, mContent))
    return NS_ERROR_ABORT;

  if (!mRequireSameHost) {
    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

  // XXXbz should this be using something more like the nsContentUtils
  // same-origin checker?
  nsCOMPtr<nsIURI> oldURI;
  oldChan->GetURI(getter_AddRefs(oldURI));
  NS_ENSURE_STATE(oldURI && newURI);

  if (!IsSameHost(oldURI, newURI))
    return NS_ERROR_ABORT;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

struct SendPingInfo {
  PRInt32 numPings;
  PRInt32 maxPings;
  bool    requireSameHost;
  nsIURI *referrer;
};

static void
SendPing(void *closure, nsIContent *content, nsIURI *uri, nsIIOService *ios)
{
  SendPingInfo *info = static_cast<SendPingInfo *>(closure);
  if (info->numPings >= info->maxPings)
    return;

  if (info->requireSameHost) {
    // Make sure the referrer and the given uri share the same origin.  We
    // only require the same hostname.  The scheme and port may differ.
    if (!IsSameHost(uri, info->referrer))
      return;
  }

  nsIDocument *doc = content->OwnerDoc();

  nsCOMPtr<nsIChannel> chan;
  ios->NewChannelFromURI(uri, getter_AddRefs(chan));
  if (!chan)
    return;

  // Don't bother caching the result of this URI load.
  chan->SetLoadFlags(nsIRequest::INHIBIT_CACHING);

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (!httpChan)
    return;

  // This is needed in order for 3rd-party cookie blocking to work.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(httpChan);
  if (httpInternal)
    httpInternal->SetDocumentURI(doc->GetDocumentURI());

  if (info->referrer)
    httpChan->SetReferrer(info->referrer);

  httpChan->SetRequestMethod(NS_LITERAL_CSTRING("POST"));

  // Remove extraneous request headers (to reduce request size)
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept"),
                             EmptyCString(), false);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-language"),
                             EmptyCString(), false);
  httpChan->SetRequestHeader(NS_LITERAL_CSTRING("accept-encoding"),
                             EmptyCString(), false);

  nsCOMPtr<nsIUploadChannel> uploadChan = do_QueryInterface(httpChan);
  if (!uploadChan)
    return;

  // To avoid sending an unnecessary Content-Type header, we encode the
  // closing portion of the headers in the POST body.
  NS_NAMED_LITERAL_CSTRING(uploadData, "Content-Length: 0\r\n\r\n");

  nsCOMPtr<nsIInputStream> uploadStream;
  NS_NewPostDataStream(getter_AddRefs(uploadStream), false,
                       uploadData, 0);
  if (!uploadStream)
    return;

  uploadChan->SetUploadStream(uploadStream, EmptyCString(), -1);

  // The channel needs to have a loadgroup associated with it, so that we can
  // cancel the channel and any redirected channels it may create.
  nsCOMPtr<nsILoadGroup> loadGroup =
      do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  if (!loadGroup)
    return;
  chan->SetLoadGroup(loadGroup);

  // Construct a listener that merely discards any response.  If successful at
  // opening the channel, then it is not necessary to hold a reference to the
  // channel.  The networking subsystem will take care of that for us.
  nsCOMPtr<nsIStreamListener> listener =
      new nsPingListener(info->requireSameHost, content);
  if (!listener)
    return;

  // Observe redirects as well:
  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryInterface(listener);
  NS_ASSERTION(callbacks, "oops");
  loadGroup->SetNotificationCallbacks(callbacks);

  chan->AsyncOpen(listener, nsnull);

  // Even if AsyncOpen failed, we still count this as a successful ping.  It's
  // possible that AsyncOpen may have failed after triggering some background
  // process that may have written something to the network.
  info->numPings++;

  // Prevent ping requests from stalling and never being garbage collected...
  nsCOMPtr<nsITimer> timer =
      do_CreateInstance(NS_TIMER_CONTRACTID);
  if (timer) {
    nsresult rv = timer->InitWithFuncCallback(OnPingTimeout, loadGroup,
                                              PING_TIMEOUT,
                                              nsITimer::TYPE_ONE_SHOT);
    if (NS_SUCCEEDED(rv)) {
      // When the timer expires, the callback function will release this
      // reference to the loadgroup.
      static_cast<nsILoadGroup *>(loadGroup.get())->AddRef();
      loadGroup = 0;
    }
  }
  
  // If we failed to setup the timer, then we should just cancel the channel
  // because we won't be able to ensure that it goes away in a timely manner.
  if (loadGroup)
    chan->Cancel(NS_ERROR_ABORT);
}

// Spec: http://whatwg.org/specs/web-apps/current-work/#ping
static void
DispatchPings(nsIContent *content, nsIURI *referrer)
{
  SendPingInfo info;

  if (!PingsEnabled(&info.maxPings, &info.requireSameHost))
    return;
  if (info.maxPings == 0)
    return;

  info.numPings = 0;
  info.referrer = referrer;

  ForEachPing(content, SendPing, &info);
}

static nsDOMPerformanceNavigationType
ConvertLoadTypeToNavigationType(PRUint32 aLoadType)
{
  // Not initialized, assume it's normal load.
  if (aLoadType == 0) {
    aLoadType = LOAD_NORMAL;
  }

  nsDOMPerformanceNavigationType result = dom::PerformanceNavigation::TYPE_RESERVED;
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_EXTERNAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_REPLACE:
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

static nsISHEntry* GetRootSHEntry(nsISHEntry *entry);

static void
IncreasePrivateDocShellCount()
{
    gNumberOfPrivateDocShells++;
    if (gNumberOfPrivateDocShells > 1 ||
        XRE_GetProcessType() != GeckoProcessType_Content) {
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
    if (!gNumberOfPrivateDocShells)
    {
        if (XRE_GetProcessType() == GeckoProcessType_Content) {
            mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
            cc->SendPrivateDocShellsExist(false);
            return;
        }

        nsCOMPtr<nsIObserverService> obsvc = mozilla::services::GetObserverService();
        if (obsvc) {
            obsvc->NotifyObservers(nsnull, "last-pb-context-exited", nsnull);
        }
    }
}

//*****************************************************************************
//***    nsDocShell: Object Management
//*****************************************************************************

static PRUint64 gDocshellIDCounter = 0;

// Note: operator new zeros our memory
nsDocShell::nsDocShell():
    nsDocLoader(),
    mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto),
    mTreeOwner(nsnull),
    mChromeEventHandler(nsnull),
    mCharsetReloadState(eCharsetReloadInit),
    mChildOffset(0),
    mBusyFlags(BUSY_FLAGS_NONE),
    mAppType(nsIDocShell::APP_TYPE_UNKNOWN),
    mLoadType(0),
    mMarginWidth(-1),
    mMarginHeight(-1),
    mItemType(typeContent),
    mPreviousTransIndex(-1),
    mLoadedTransIndex(-1),
    mCreated(false),
    mAllowSubframes(true),
    mAllowPlugins(true),
    mAllowJavascript(true),
    mAllowMetaRedirects(true),
    mAllowImages(true),
    mAllowDNSPrefetch(true),
    mAllowWindowControl(true),
    mCreatingDocument(false),
    mUseErrorPages(false),
    mObserveErrorPages(true),
    mAllowAuth(true),
    mAllowKeywordFixup(false),
    mIsOffScreenBrowser(false),
    mIsActive(true),
    mIsAppTab(false),
    mUseGlobalHistory(false),
    mInPrivateBrowsing(false),
    mFiredUnloadEvent(false),
    mEODForCurrentDocument(false),
    mURIResultedInDocument(false),
    mIsBeingDestroyed(false),
    mIsExecutingOnLoadHandler(false),
    mIsPrintingOrPP(false),
    mSavingOldViewer(false),
#ifdef DEBUG
    mInEnsureScriptEnv(false),
#endif
    mParentCharsetSource(0)
{
    mHistoryID = ++gDocshellIDCounter;
    if (gDocShellCount++ == 0) {
        NS_ASSERTION(sURIFixup == nsnull,
                     "Huh, sURIFixup not null in first nsDocShell ctor!");

        CallGetService(NS_URIFIXUP_CONTRACTID, &sURIFixup);
    }

#ifdef PR_LOGGING
#ifdef DEBUG
    if (! gDocShellLog)
        gDocShellLog = PR_NewLogModule("nsDocShell");
#endif
    if (nsnull == gDocShellLeakLog)
        gDocShellLeakLog = PR_NewLogModule("nsDocShellLeak");
    if (gDocShellLeakLog)
        PR_LOG(gDocShellLeakLog, PR_LOG_DEBUG, ("DOCSHELL %p created\n", this));
#endif

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  ++gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
      printf("++DOCSHELL %p == %ld [id = %llu]\n", (void*) this,
             gNumberOfDocShells, mHistoryID);
  }
#endif
}

nsDocShell::~nsDocShell()
{
    Destroy();

    nsCOMPtr<nsISHistoryInternal>
        shPrivate(do_QueryInterface(mSessionHistory));
    if (shPrivate) {
        shPrivate->SetRootDocShell(nsnull);
    }

    if (--gDocShellCount == 0) {
        NS_IF_RELEASE(sURIFixup);
    }

#ifdef PR_LOGGING
    if (gDocShellLeakLog)
        PR_LOG(gDocShellLeakLog, PR_LOG_DEBUG, ("DOCSHELL %p destroyed\n", this));
#endif

#ifdef DEBUG
    // We're counting the number of |nsDocShells| to help find leaks
    --gNumberOfDocShells;
    if (!PR_GetEnv("MOZ_QUIET")) {
        printf("--DOCSHELL %p == %ld [id = %llu]\n", (void*) this,
               gNumberOfDocShells, mHistoryID);
    }
#endif

    if (mInPrivateBrowsing) {
        DecreasePrivateDocShellCount();
    }
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

    mStorages.Init();

    // We want to hold a strong ref to the loadgroup, so it better hold a weak
    // ref to us...  use an InterfaceRequestorProxy to do this.
    nsCOMPtr<InterfaceRequestorProxy> proxy =
        new InterfaceRequestorProxy(static_cast<nsIInterfaceRequestor*>
                                               (this));
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
    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; i++) {
        shell = do_QueryInterface(ChildAt(i));
        NS_ASSERTION(shell, "docshell has null child");

        if (shell) {
            shell->SetTreeOwner(nsnull);
        }
    }

    nsDocLoader::DestroyChildren();
}

//*****************************************************************************
// nsDocShell::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF_INHERITED(nsDocShell, nsDocLoader)
NS_IMPL_RELEASE_INHERITED(nsDocShell, nsDocLoader)

NS_INTERFACE_MAP_BEGIN(nsDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
    NS_INTERFACE_MAP_ENTRY(nsIDocShellHistory)
    NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
    NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
    NS_INTERFACE_MAP_ENTRY(nsIScrollable)
    NS_INTERFACE_MAP_ENTRY(nsITextScroll)
    NS_INTERFACE_MAP_ENTRY(nsIDocCharset)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
    NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerContainer)
    NS_INTERFACE_MAP_ENTRY(nsIEditorDocShell)
    NS_INTERFACE_MAP_ENTRY(nsIWebPageDescriptor)
    NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
    NS_INTERFACE_MAP_ENTRY(nsIObserver)
    NS_INTERFACE_MAP_ENTRY(nsILoadContext)
    NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
    NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
    NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
NS_INTERFACE_MAP_END_INHERITING(nsDocLoader)

///*****************************************************************************
// nsDocShell::nsIInterfaceRequestor
//*****************************************************************************   
NS_IMETHODIMP nsDocShell::GetInterface(const nsIID & aIID, void **aSink)
{
    NS_PRECONDITION(aSink, "null out param");

    *aSink = nsnull;

    if (aIID.Equals(NS_GET_IID(nsICommandManager))) {
        NS_ENSURE_SUCCESS(EnsureCommandHandler(), NS_ERROR_FAILURE);
        *aSink = mCommandManager;
    }
    else if (aIID.Equals(NS_GET_IID(nsIURIContentListener))) {
        *aSink = mContentListener;
    }
    else if (aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        *aSink = mScriptGlobal;
    }
    else if ((aIID.Equals(NS_GET_IID(nsPIDOMWindow)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindow)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindowInternal))) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        return mScriptGlobal->QueryInterface(aIID, aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsIDOMDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
        mContentViewer->GetDOMDocument((nsIDOMDocument **) aSink);
        return *aSink ? NS_OK : NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIDocument)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));
        if (!domDoc)
            return NS_NOINTERFACE;
        return domDoc->QueryInterface(aIID, aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsIApplicationCacheContainer))) {
        *aSink = nsnull;

        // Return application cache associated with this docshell, if any

        nsCOMPtr<nsIContentViewer> contentViewer;
        GetContentViewer(getter_AddRefs(contentViewer));
        if (!contentViewer)
            return NS_ERROR_NO_INTERFACE;

        nsCOMPtr<nsIDOMDocument> domDoc;
        contentViewer->GetDOMDocument(getter_AddRefs(domDoc));
        NS_ASSERTION(domDoc, "Should have a document.");
        if (!domDoc)
            return NS_ERROR_NO_INTERFACE;

#if defined(PR_LOGGING) && defined(DEBUG)
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: returning app cache container %p",
                this, domDoc.get()));
#endif
        return domDoc->QueryInterface(aIID, aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsIPrompt)) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> wwatch =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(mScriptGlobal));

        // Get the an auth prompter for our window so that the parenting
        // of the dialogs works as it should when using tabs.

        nsIPrompt *prompt;
        rv = wwatch->GetNewPrompter(window, &prompt);
        NS_ENSURE_SUCCESS(rv, rv);

        *aSink = prompt;
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
             aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
        return NS_SUCCEEDED(
                GetAuthPrompt(PROMPT_NORMAL, aIID, aSink)) ?
                NS_OK : NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsISHistory))) {
        nsCOMPtr<nsISHistory> shistory;
        nsresult
            rv =
            GetSessionHistory(getter_AddRefs(shistory));
        if (NS_SUCCEEDED(rv) && shistory) {
            *aSink = shistory;
            NS_ADDREF((nsISupports *) * aSink);
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }
    else if (aIID.Equals(NS_GET_IID(nsIWebBrowserFind))) {
        nsresult rv = EnsureFind();
        if (NS_FAILED(rv)) return rv;

        *aSink = mFind;
        NS_ADDREF((nsISupports*)*aSink);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIEditingSession)) && NS_SUCCEEDED(EnsureEditorData())) {
      nsCOMPtr<nsIEditingSession> editingSession;
      mEditorData->GetEditingSession(getter_AddRefs(editingSession));
      if (editingSession)
      {
        *aSink = editingSession;
        NS_ADDREF((nsISupports *)*aSink);
        return NS_OK;
      }  

      return NS_NOINTERFACE;   
    }
    else if (aIID.Equals(NS_GET_IID(nsIClipboardDragDropHookList)) 
            && NS_SUCCEEDED(EnsureTransferableHookData())) {
        *aSink = mTransferableHookData;
        NS_ADDREF((nsISupports *)*aSink);
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsISelectionDisplay))) {
      nsCOMPtr<nsIPresShell> shell;
      nsresult rv = GetPresShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell)
        return shell->QueryInterface(aIID,aSink);    
    }
    else if (aIID.Equals(NS_GET_IID(nsIDocShellTreeOwner))) {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
      if (NS_SUCCEEDED(rv) && treeOwner)
        return treeOwner->QueryInterface(aIID, aSink);
    }
    else if (aIID.Equals(NS_GET_IID(nsITabChild))) {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
      if (NS_SUCCEEDED(rv) && treeOwner) {
        nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(treeOwner);
        if (ir)
          return ir->GetInterface(aIID, aSink);
      }
    }
    else if (aIID.Equals(NS_GET_IID(nsIContentFrameMessageManager))) {
      nsCOMPtr<nsITabChild> tabChild =
        do_GetInterface(static_cast<nsIDocShell*>(this));
      nsCOMPtr<nsIContentFrameMessageManager> mm;
      if (tabChild) {
        tabChild->
          GetMessageManager(getter_AddRefs(mm));
      } else {
        nsCOMPtr<nsPIDOMWindow> win =
          do_GetInterface(static_cast<nsIDocShell*>(this));
        if (win) {
          mm = do_QueryInterface(win->GetParentTarget());
        }
      }
      *aSink = mm.get();
    }
    else {
      return nsDocLoader::GetInterface(aIID, aSink);
    }

    NS_IF_ADDREF(((nsISupports *) * aSink));
    return *aSink ? NS_OK : NS_NOINTERFACE;
}

PRUint32
nsDocShell::
ConvertDocShellLoadInfoToLoadType(nsDocShellInfoLoadType aDocShellLoadType)
{
    PRUint32 loadType = LOAD_NORMAL;

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
    default:
        NS_NOTREACHED("Unexpected nsDocShellInfoLoadType value");
    }

    return loadType;
}


nsDocShellInfoLoadType
nsDocShell::ConvertLoadTypeToDocShellLoadInfo(PRUint32 aLoadType)
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
    default:
        NS_NOTREACHED("Unexpected load type value");
    }

    return docShellLoadType;
}                                                                               

//*****************************************************************************
// nsDocShell::nsIDocShell
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::LoadURI(nsIURI * aURI,
                    nsIDocShellLoadInfo * aLoadInfo,
                    PRUint32 aLoadFlags,
                    bool aFirstParty)
{
    NS_PRECONDITION(aLoadInfo || (aLoadFlags & EXTRA_LOAD_FLAGS) == 0,
                    "Unexpected flags");
    NS_PRECONDITION((aLoadFlags & 0xf) == 0, "Should not have these flags set");
    
    // Note: we allow loads to get through here even if mFiredUnloadEvent is
    // true; that case will get handled in LoadInternal or LoadHistoryEntry.
    if (IsPrintingOrPP()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIInputStream> postStream;
    nsCOMPtr<nsIInputStream> headersStream;
    nsCOMPtr<nsISupports> owner;
    bool inheritOwner = false;
    bool ownerIsExplicit = false;
    bool sendReferrer = true;
    nsCOMPtr<nsISHEntry> shEntry;
    nsXPIDLString target;
    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);    

    NS_ENSURE_ARG(aURI);

    if (!StartupTimeline::HasRecord(StartupTimeline::FIRST_LOAD_URI) &&
        mItemType == typeContent && !NS_IsAboutBlank(aURI)) {
        StartupTimeline::RecordOnce(StartupTimeline::FIRST_LOAD_URI);
    }

    // Extract the info from the DocShellLoadInfo struct...
    if (aLoadInfo) {
        aLoadInfo->GetReferrer(getter_AddRefs(referrer));

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
    }

#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString uristr;
        aURI->GetAsciiSpec(uristr);
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
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
        PRUint32 parentLoadType;

        if (parentDS && parentDS != static_cast<nsIDocShell *>(this)) {
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

            nsCOMPtr<nsIDocShellHistory> parent(do_QueryInterface(parentAsItem));
            if (parent) {
                // Get the ShEntry for the child from the parent
                nsCOMPtr<nsISHEntry> currentSH;
                bool oshe = false;
                parent->GetCurrentSHEntry(getter_AddRefs(currentSH), &oshe);
                bool dynamicallyAddedChild = mDynamicallyCreated;
                if (!dynamicallyAddedChild && !oshe && currentSH) {
                    currentSH->HasDynamicallyAddedChild(&dynamicallyAddedChild);
                }
                if (!dynamicallyAddedChild) {
                    // Only use the old SHEntry, if we're sure enough that
                    // it wasn't originally for some other frame.
                    parent->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
                }

                // Make some decisions on the child frame's loadType based on the 
                // parent's loadType. 
                if (mCurrentURI == nsnull) {
                    // This is a newly created frame. Check for exception cases first. 
                    // By default the subframe will inherit the parent's loadType.
                    if (shEntry && (parentLoadType == LOAD_NORMAL ||
                                    parentLoadType == LOAD_LINK   ||
                                    parentLoadType == LOAD_NORMAL_EXTERNAL)) {
                        // The parent was loaded normally. In this case, this *brand new* child really shouldn't
                        // have a SHEntry. If it does, it could be because the parent is replacing an
                        // existing frame with a new frame, in the onLoadHandler. We don't want this
                        // url to get into session history. Clear off shEntry, and set load type to
                        // LOAD_BYPASS_HISTORY. 
                        bool inOnLoadHandler=false;
                        parentDS->GetIsExecutingOnLoadHandler(&inOnLoadHandler);
                        if (inOnLoadHandler) {
                            loadType = LOAD_NORMAL_REPLACE;
                            shEntry = nsnull;
                        }
                    }   
                    else if (parentLoadType == LOAD_REFRESH) {
                        // Clear shEntry. For refresh loads, we have to load
                        // what comes thro' the pipe, not what's in history.
                        shEntry = nsnull;
                    }
                    else if ((parentLoadType == LOAD_BYPASS_HISTORY) ||
                              (shEntry && 
                               ((parentLoadType & LOAD_CMD_HISTORY) || 
                                (parentLoadType == LOAD_RELOAD_NORMAL) || 
                                (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE)))) {
                        // If the parent url, bypassed history or was loaded from
                        // history, pass on the parent's loadType to the new child 
                        // frame too, so that the child frame will also
                        // avoid getting into history. 
                        loadType = parentLoadType;
                    }
                    else if (parentLoadType == LOAD_ERROR_PAGE) {
                        // If the parent document is an error page, we don't
                        // want to update global/session history. However,
                        // this child frame is not an error page.
                        loadType = LOAD_BYPASS_HISTORY;
                    }
                }
                else {
                    // This is a pre-existing subframe. If the load was not originally initiated
                    // by session history, (if (!shEntry) condition succeeded) and mCurrentURI is not null,
                    // it is possible that a parent's onLoadHandler or even self's onLoadHandler is loading 
                    // a new page in this child. Check parent's and self's busy flag  and if it is set,
                    // we don't want this onLoadHandler load to get in to session history.
                    PRUint32 parentBusy = BUSY_FLAGS_NONE;
                    PRUint32 selfBusy = BUSY_FLAGS_NONE;
                    parentDS->GetBusyFlags(&parentBusy);                    
                    GetBusyFlags(&selfBusy);
                    if (parentBusy & BUSY_FLAGS_BUSY ||
                        selfBusy & BUSY_FLAGS_BUSY) {
                        loadType = LOAD_NORMAL_REPLACE;
                        shEntry = nsnull; 
                    }
                }
            } // parent
        } //parentDS
        else {  
            // This is the root docshell. If we got here while  
            // executing an onLoad Handler,this load will not go 
            // into session history.
            bool inOnLoadHandler=false;
            GetIsExecutingOnLoadHandler(&inOnLoadHandler);
            if (inOnLoadHandler) {
                loadType = LOAD_NORMAL_REPLACE;
            }
        } 
    } // !shEntry

    if (shEntry) {
#ifdef DEBUG
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
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
    // (1) If the system principal was passed in and we're a typeContent
    //     docshell, inherit the principal from the current document
    //     instead.
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
    // (1) If the system principal was passed in and we're a typeContent
    //     docshell, return an error.
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

    nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (owner && mItemType != typeChrome) {
        nsCOMPtr<nsIPrincipal> ownerPrincipal = do_QueryInterface(owner);
        bool isSystem;
        rv = secMan->IsSystemPrincipal(ownerPrincipal, &isSystem);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isSystem) {
            if (ownerIsExplicit) {
                return NS_ERROR_DOM_SECURITY_ERR;
            }
            owner = nsnull;
            inheritOwner = true;
        }
    }
    if (!owner && !inheritOwner && !ownerIsExplicit) {
        // See if there's system or chrome JS code running
        rv = secMan->SubjectPrincipalIsSystem(&inheritOwner);
        if (NS_FAILED(rv)) {
            // Set it back to false
            inheritOwner = false;
        }
    }

    if (aLoadFlags & LOAD_FLAGS_DISALLOW_INHERIT_OWNER) {
        inheritOwner = false;
        owner = do_CreateInstance("@mozilla.org/nullprincipal;1");
    }

    PRUint32 flags = 0;

    if (inheritOwner)
        flags |= INTERNAL_LOAD_FLAGS_INHERIT_OWNER;

    if (!sendReferrer)
        flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER;
            
    if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP)
        flags |= INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;

    if (aLoadFlags & LOAD_FLAGS_FIRST_LOAD)
        flags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;

    if (aLoadFlags & LOAD_FLAGS_BYPASS_CLASSIFIER)
        flags |= INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER;

    if (aLoadFlags & LOAD_FLAGS_FORCE_ALLOW_COOKIES)
        flags |= INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES;

    return InternalLoad(aURI,
                        referrer,
                        owner,
                        flags,
                        target.get(),
                        nsnull,         // No type hint
                        postStream,
                        headersStream,
                        loadType,
                        nsnull,         // No SHEntry
                        aFirstParty,
                        nsnull,         // No nsIDocShell
                        nsnull);        // No nsIRequest
}

NS_IMETHODIMP
nsDocShell::LoadStream(nsIInputStream *aStream, nsIURI * aURI,
                       const nsACString &aContentType,
                       const nsACString &aContentCharset,
                       nsIDocShellLoadInfo * aLoadInfo)
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
        if (NS_FAILED(rv))
            return rv;
        // Make sure that the URI spec "looks" like a protocol and path...
        // For now, just use a bogus protocol called "internal"
        rv = uri->SetSpec(NS_LITERAL_CSTRING("internal:load-stream"));
        if (NS_FAILED(rv))
            return rv;
    }

    PRUint32 loadType = LOAD_NORMAL;
    if (aLoadInfo) {
        nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
        (void) aLoadInfo->GetLoadType(&lt);
        // Get the appropriate LoadType from nsIDocShellLoadInfo type
        loadType = ConvertDocShellLoadInfoToLoadType(lt);
    }

    NS_ENSURE_SUCCESS(Stop(nsIWebNavigation::STOP_NETWORK), NS_ERROR_FAILURE);

    mLoadType = loadType;

    // build up a channel for this stream.
    nsCOMPtr<nsIChannel> channel;
    NS_ENSURE_SUCCESS(NS_NewInputStreamChannel
                      (getter_AddRefs(channel), uri, aStream,
                       aContentType, aContentCharset),
                      NS_ERROR_FAILURE);

    nsCOMPtr<nsIURILoader>
        uriLoader(do_GetService(NS_URI_LOADER_CONTRACTID));
    NS_ENSURE_TRUE(uriLoader, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(DoChannelLoad(channel, uriLoader, false),
                      NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::CreateLoadInfo(nsIDocShellLoadInfo ** aLoadInfo)
{
    nsDocShellLoadInfo *loadInfo = new nsDocShellLoadInfo();
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsIDocShellLoadInfo> localRef(loadInfo);

    *aLoadInfo = localRef;
    NS_ADDREF(*aLoadInfo);
    return NS_OK;
}


/*
 * Reset state to a new content model within the current document and the document
 * viewer.  Called by the document before initiating an out of band document.write().
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
        PRInt32 i, n = mChildList.Count();
        kids.SetCapacity(n);
        for (i = 0; i < n; i++) {
            kids.AppendElement(do_QueryInterface(ChildAt(i)));
        }

        n = kids.Length();
        for (i = 0; i < n; ++i) {
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

nsresult
nsDocShell::MaybeInitTiming()
{
    if (mTiming) {
        return NS_OK;
    }

    if (Preferences::GetBool("dom.enable_performance", false)) {
        mTiming = new nsDOMNavigationTiming();
        mTiming->NotifyNavigationStart();
    }
    return NS_OK;
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
/* static */
bool
nsDocShell::ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem,
                           nsIDocShellTreeItem* aTargetTreeItem)
{
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(securityManager, false);

    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    nsresult rv =
        securityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    NS_ENSURE_SUCCESS(rv, false);

    if (subjectPrincipal) {
        // We're called from JS, check if UniversalXPConnect is
        // enabled.
        bool ubwEnabled = false;
        rv = securityManager->IsCapabilityEnabled("UniversalXPConnect",
                                                  &ubwEnabled);
        NS_ENSURE_SUCCESS(rv, false);

        if (ubwEnabled) {
            return true;
        }
    }

    // Get origin document principal
    nsCOMPtr<nsIDocument> originDocument(do_GetInterface(aOriginTreeItem));
    NS_ENSURE_TRUE(originDocument, false);

    // Get target principal
    nsCOMPtr<nsIDocument> targetDocument(do_GetInterface(aTargetTreeItem));
    NS_ENSURE_TRUE(targetDocument, false);

    bool equal;
    rv = originDocument->NodePrincipal()->
            Equals(targetDocument->NodePrincipal(), &equal);
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
    if (NS_SUCCEEDED(rv) && originURI)
        innerOriginURI = NS_GetInnermostURI(originURI);

    rv = targetDocument->NodePrincipal()->GetURI(getter_AddRefs(targetURI));
    if (NS_SUCCEEDED(rv) && targetURI)
        innerTargetURI = NS_GetInnermostURI(targetURI);

    return innerOriginURI && innerTargetURI &&
        NS_SUCCEEDED(innerOriginURI->SchemeIs("file", &originIsFile)) &&
        NS_SUCCEEDED(innerTargetURI->SchemeIs("file", &targetIsFile)) &&
        originIsFile && targetIsFile;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresContext(nsPresContext** aPresContext)
{
    NS_ENSURE_ARG_POINTER(aPresContext);
    *aPresContext = nsnull;

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
nsDocShell::GetPresContext(nsPresContext ** aPresContext)
{
    NS_ENSURE_ARG_POINTER(aPresContext);
    *aPresContext = nsnull;

    if (!mContentViewer)
      return NS_OK;

    return mContentViewer->GetPresContext(aPresContext);
}

NS_IMETHODIMP
nsDocShell::GetPresShell(nsIPresShell ** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsRefPtr<nsPresContext> presContext;
    (void) GetPresContext(getter_AddRefs(presContext));

    if (presContext) {
        NS_IF_ADDREF(*aPresShell = presContext->GetPresShell());
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetEldestPresShell(nsIPresShell** aPresShell)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPresShell);
    *aPresShell = nsnull;

    nsRefPtr<nsPresContext> presContext;
    (void) GetEldestPresContext(getter_AddRefs(presContext));

    if (presContext) {
        NS_IF_ADDREF(*aPresShell = presContext->GetPresShell());
    }

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetContentViewer(nsIContentViewer ** aContentViewer)
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
    mChromeEventHandler = aChromeEventHandler;

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
    if (win) {
        win->SetChromeEventHandler(aChromeEventHandler);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChromeEventHandler(nsIDOMEventTarget** aChromeEventHandler)
{
    NS_ENSURE_ARG_POINTER(aChromeEventHandler);
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mChromeEventHandler);
    target.swap(*aChromeEventHandler);
    return NS_OK;
}

/* void setCurrentURI (in nsIURI uri); */
NS_IMETHODIMP
nsDocShell::SetCurrentURI(nsIURI *aURI)
{
    // Note that securityUI will set STATE_IS_INSECURE, even if
    // the scheme of |aURI| is "https".
    SetCurrentURI(aURI, nsnull, true, 0);
    return NS_OK;
}

bool
nsDocShell::SetCurrentURI(nsIURI *aURI, nsIRequest *aRequest,
                          bool aFireOnLocationChange, PRUint32 aLocationFlags)
{
#ifdef PR_LOGGING
    if (gDocShellLeakLog && PR_LOG_TEST(gDocShellLeakLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        if (aURI)
            aURI->GetSpec(spec);
        PR_LogPrint("DOCSHELL %p SetCurrentURI %s\n", this, spec.get());
    }
#endif

    // We don't want to send a location change when we're displaying an error
    // page, and we don't want to change our idea of "current URI" either
    if (mLoadType == LOAD_ERROR_PAGE) {
        return false;
    }

    mCurrentURI = NS_TryToMakeImmutable(aURI);
    
    bool isRoot = false;   // Is this the root docshell
    bool isSubFrame = false;  // Is this a subframe navigation?

    nsCOMPtr<nsIDocShellTreeItem> root;

    GetSameTypeRootTreeItem(getter_AddRefs(root));
    if (root.get() == static_cast<nsIDocShellTreeItem *>(this)) 
    {
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
nsDocShell::GetCharset(char** aCharset)
{
    NS_ENSURE_ARG_POINTER(aCharset);
    *aCharset = nsnull; 

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    nsIDocument *doc = presShell->GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
    *aCharset = ToNewCString(doc->GetDocumentCharacterSet());
    if (!*aCharset) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCharset(const char* aCharset)
{
    // set the default charset
    nsCOMPtr<nsIContentViewer> viewer;
    GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV(do_QueryInterface(viewer));
      if (muDV) {
        nsCString charset(aCharset);
        NS_ENSURE_SUCCESS(muDV->SetDefaultCharacterSet(charset),
                          NS_ERROR_FAILURE);
      }
    }

    // set the charset override
    nsCOMPtr<nsIAtom> csAtom = do_GetAtom(aCharset);
    SetForcedCharset(csAtom);

    return NS_OK;
} 

NS_IMETHODIMP nsDocShell::SetForcedCharset(nsIAtom * aCharset)
{
  mForcedCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetForcedCharset(nsIAtom ** aResult)
{
  *aResult = mForcedCharset;
  if (mForcedCharset) NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentCharset(nsIAtom * aCharset)
{
  mParentCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentCharset(nsIAtom ** aResult)
{
  *aResult = mParentCharset;
  if (mParentCharset) NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetParentCharsetSource(PRInt32 aCharsetSource)
{
  mParentCharsetSource = aCharsetSource;
  return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetParentCharsetSource(PRInt32 * aParentCharsetSource)
{
  *aParentCharsetSource = mParentCharsetSource;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChannelIsUnsafe(bool *aUnsafe)
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
nsDocShell::GetAllowPlugins(bool * aAllowPlugins)
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
    //XXX should enable or disable a plugin host
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowJavascript(bool * aAllowJavascript)
{
    NS_ENSURE_ARG_POINTER(aAllowJavascript);

    *aAllowJavascript = mAllowJavascript;
    if (!mAllowJavascript) {
        return NS_OK;
    }

    bool unsafe;
    *aAllowJavascript = NS_SUCCEEDED(GetChannelIsUnsafe(&unsafe)) && !unsafe;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowJavascript(bool aAllowJavascript)
{
    mAllowJavascript = aAllowJavascript;
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
    bool changed = aUsePrivateBrowsing != mInPrivateBrowsing;
    if (changed) {
        mInPrivateBrowsing = aUsePrivateBrowsing;
        if (aUsePrivateBrowsing) {
            IncreasePrivateDocShellCount();
        } else {
            DecreasePrivateDocShellCount();
        }
    }
    
    PRInt32 count = mChildList.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsILoadContext> shell = do_QueryInterface(ChildAt(i));
        if (shell) {
            shell->SetUsePrivateBrowsing(aUsePrivateBrowsing);
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
nsDocShell::AddWeakPrivacyTransitionObserver(nsIPrivacyTransitionObserver* aObserver)
{
    nsWeakPtr weakObs = do_GetWeakReference(aObserver);
    if (!weakObs) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    return mPrivacyObservers.AppendElement(weakObs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocShell::GetAllowMetaRedirects(bool * aReturn)
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

NS_IMETHODIMP nsDocShell::SetAllowMetaRedirects(bool aValue)
{
    mAllowMetaRedirects = aValue;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowSubframes(bool * aAllowSubframes)
{
    NS_ENSURE_ARG_POINTER(aAllowSubframes);

    *aAllowSubframes = mAllowSubframes;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowSubframes(bool aAllowSubframes)
{
    mAllowSubframes = aAllowSubframes;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowImages(bool * aAllowImages)
{
    NS_ENSURE_ARG_POINTER(aAllowImages);

    *aAllowImages = mAllowImages;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowImages(bool aAllowImages)
{
    mAllowImages = aAllowImages;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowDNSPrefetch(bool * aAllowDNSPrefetch)
{
    *aAllowDNSPrefetch = mAllowDNSPrefetch;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowDNSPrefetch(bool aAllowDNSPrefetch)
{
    mAllowDNSPrefetch = aAllowDNSPrefetch;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::GetAllowWindowControl(bool * aAllowWindowControl)
{
    *aAllowWindowControl = mAllowWindowControl;
    return NS_OK;
}

NS_IMETHODIMP nsDocShell::SetAllowWindowControl(bool aAllowWindowControl)
{
    mAllowWindowControl = aAllowWindowControl;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocShellEnumerator(PRInt32 aItemType, PRInt32 aDirection, nsISimpleEnumerator **outEnum)
{
    NS_ENSURE_ARG_POINTER(outEnum);
    *outEnum = nsnull;
    
    nsRefPtr<nsDocShellEnumerator> docShellEnum;
    if (aDirection == ENUMERATE_FORWARDS)
        docShellEnum = new nsDocShellForwardsEnumerator;
    else
        docShellEnum = new nsDocShellBackwardsEnumerator;
    
    if (!docShellEnum) return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = docShellEnum->SetEnumDocShellType(aItemType);
    if (NS_FAILED(rv)) return rv;

    rv = docShellEnum->SetEnumerationRootItem((nsIDocShellTreeItem *)this);
    if (NS_FAILED(rv)) return rv;

    rv = docShellEnum->First();
    if (NS_FAILED(rv)) return rv;

    rv = docShellEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)outEnum);

    return rv;
}

NS_IMETHODIMP
nsDocShell::GetAppType(PRUint32 * aAppType)
{
    *aAppType = mAppType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAppType(PRUint32 aAppType)
{
    mAppType = aAppType;
    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::GetAllowAuth(bool * aAllowAuth)
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
nsDocShell::GetZoom(float *zoom)
{
    NS_ENSURE_ARG_POINTER(zoom);
    *zoom = 1.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetZoom(float zoom)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetMarginWidth(PRInt32 * aWidth)
{
    NS_ENSURE_ARG_POINTER(aWidth);

    *aWidth = mMarginWidth;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginWidth(PRInt32 aWidth)
{
    mMarginWidth = aWidth;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMarginHeight(PRInt32 * aHeight)
{
    NS_ENSURE_ARG_POINTER(aHeight);

    *aHeight = mMarginHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginHeight(PRInt32 aHeight)
{
    mMarginHeight = aHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetBusyFlags(PRUint32 * aBusyFlags)
{
    NS_ENSURE_ARG_POINTER(aBusyFlags);

    *aBusyFlags = mBusyFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::TabToTreeOwner(bool aForward, bool* aTookFocus)
{
    NS_ENSURE_ARG_POINTER(aTookFocus);
    
    nsCOMPtr<nsIWebBrowserChromeFocus> chromeFocus = do_GetInterface(mTreeOwner);
    if (chromeFocus) {
        if (aForward)
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusNextElement());
        else
            *aTookFocus = NS_SUCCEEDED(chromeFocus->FocusPrevElement());
    } else
        *aTookFocus = false;
    
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSecurityUI(nsISecureBrowserUI **aSecurityUI)
{
    NS_IF_ADDREF(*aSecurityUI = mSecurityUI);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSecurityUI(nsISecureBrowserUI *aSecurityUI)
{
    mSecurityUI = aSecurityUI;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseErrorPages(bool *aUseErrorPages)
{
    *aUseErrorPages = mUseErrorPages;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseErrorPages(bool aUseErrorPages)
{
    // If mUseErrorPages is set explicitly, stop observing the pref.
    if (mObserveErrorPages) {
        Preferences::RemoveObserver(this, "browser.xul.error_pages.enabled");
        mObserveErrorPages = false;
    }
    mUseErrorPages = aUseErrorPages;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPreviousTransIndex(PRInt32 *aPreviousTransIndex)
{
    *aPreviousTransIndex = mPreviousTransIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadedTransIndex(PRInt32 *aLoadedTransIndex)
{
    *aLoadedTransIndex = mLoadedTransIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::HistoryPurged(PRInt32 aNumEntries)
{
    // These indices are used for fastback cache eviction, to determine
    // which session history entries are candidates for content viewer
    // eviction.  We need to adjust by the number of entries that we
    // just purged from history, so that we look at the right session history
    // entries during eviction.
    mPreviousTransIndex = NS_MAX(-1, mPreviousTransIndex - aNumEntries);
    mLoadedTransIndex = NS_MAX(0, mLoadedTransIndex - aNumEntries);

    PRInt32 count = mChildList.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell) {
            shell->HistoryPurged(aNumEntries);
        }
    }

    return NS_OK;
}

nsresult
nsDocShell::HistoryTransactionRemoved(PRInt32 aIndex)
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
                            
    PRInt32 count = mChildList.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell) {
            static_cast<nsDocShell*>(shell.get())->
                HistoryTransactionRemoved(aIndex);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSessionStorageForPrincipal(nsIPrincipal* aPrincipal,
                                          const nsAString& aDocumentURI,
                                          bool aCreate,
                                          nsIDOMStorage** aStorage)
{
    NS_ENSURE_ARG_POINTER(aStorage);
    *aStorage = nsnull;

    if (!aPrincipal)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIDocShellTreeItem> topItem;
    rv = GetSameTypeRootTreeItem(getter_AddRefs(topItem));
    if (NS_FAILED(rv))
        return rv;

    if (!topItem)
        return NS_ERROR_FAILURE;

    nsDocShell* topDocShell = static_cast<nsDocShell*>(topItem.get());
    if (topDocShell != this)
        return topDocShell->GetSessionStorageForPrincipal(aPrincipal,
                                                          aDocumentURI,
                                                          aCreate,
                                                          aStorage);

    nsXPIDLCString origin;
    rv = aPrincipal->GetOrigin(getter_Copies(origin));
    if (NS_FAILED(rv))
        return rv;

    if (origin.IsEmpty())
        return NS_OK;

    if (!mStorages.Get(origin, aStorage) && aCreate) {
        nsCOMPtr<nsIDOMStorage> newstorage =
            do_CreateInstance("@mozilla.org/dom/storage;2");
        if (!newstorage)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(newstorage);
        if (!pistorage)
            return NS_ERROR_FAILURE;

        rv = pistorage->InitAsSessionStorage(aPrincipal, aDocumentURI, mInPrivateBrowsing);
        if (NS_FAILED(rv))
            return rv;

        mStorages.Put(origin, newstorage);

        newstorage.swap(*aStorage);
#if defined(PR_LOGGING) && defined(DEBUG)
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: created a new sessionStorage %p",
                this, *aStorage));
#endif
    }
    else if (*aStorage) {
        nsCOMPtr<nsPIDOMStorage> piStorage = do_QueryInterface(*aStorage);
        if (piStorage) {
            nsCOMPtr<nsIPrincipal> storagePrincipal = piStorage->Principal();

            // The origin string used to map items in the hash table is 
            // an implicit security check. That check is double-confirmed 
            // by checking the principal a storage was demanded for 
            // really is the principal for which that storage was originally 
            // created. Originally, the check was hidden in the CanAccess 
            // method but it's implementation has changed.
            bool equals;
            nsresult rv = aPrincipal->EqualsIgnoringDomain(storagePrincipal, &equals);
            NS_ASSERTION(NS_SUCCEEDED(rv) && equals,
                         "GetSessionStorageForPrincipal got a storage "
                         "that could not be accessed!");

            if (NS_FAILED(rv) || !equals) {
                NS_RELEASE(*aStorage);
                return NS_ERROR_DOM_SECURITY_ERR;
            }
        }

#if defined(PR_LOGGING) && defined(DEBUG)
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: returns existing sessionStorage %p",
                this, *aStorage));
#endif
    }

    if (aCreate) {
        // We are asked to create a new storage object. This indicates
        // that a new windows wants it. At this moment we "fork" the existing
        // storage object (what it means is described in the paragraph bellow).
        // We must create a single object per a single window to distinguish
        // a window originating oparations on the storage object to succesfully
        // prevent dispatch of a storage event to this same window that ivoked
        // a change in its storage. We also do this to correctly fill
        // documentURI property in the storage event.
        //
        // The difference between clone and fork is that clone creates
        // a completelly new and independent storage, but fork only creates
        // a new object wrapping the storage implementation and data and
        // the forked storage then behaves completelly the same way as
        // the storage it has been forked of, all such forked storage objects
        // shares their state and data and change on one such object affects
        // all others the same way.
        nsCOMPtr<nsPIDOMStorage> piStorage = do_QueryInterface(*aStorage);
        nsCOMPtr<nsIDOMStorage> fork = piStorage->Fork(aDocumentURI);
#if defined(PR_LOGGING) && defined(DEBUG)
        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]: forked sessionStorage %p to %p",
                this, *aStorage, fork.get()));
#endif
        fork.swap(*aStorage);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSessionStorageForURI(nsIURI* aURI,
                                    const nsAString& aDocumentURI,
                                    nsIDOMStorage** aStorage)
{
    return GetSessionStorageForURI(aURI, aDocumentURI, true, aStorage);
}

nsresult
nsDocShell::GetSessionStorageForURI(nsIURI* aURI,
                                    const nsSubstring& aDocumentURI,
                                    bool aCreate,
                                    nsIDOMStorage** aStorage)
{
    NS_ENSURE_ARG(aURI);
    NS_ENSURE_ARG_POINTER(aStorage);

    *aStorage = nsnull;

    nsresult rv;

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is terrible hack and should go away along with this whole method.
    nsCOMPtr<nsIPrincipal> principal;
    rv = securityManager->GetCodebasePrincipal(aURI, getter_AddRefs(principal));
    if (NS_FAILED(rv))
        return rv;

    return GetSessionStorageForPrincipal(principal, aDocumentURI, aCreate, aStorage);
}

nsresult
nsDocShell::AddSessionStorage(nsIPrincipal* aPrincipal,
                              nsIDOMStorage* aStorage)
{
    NS_ENSURE_ARG_POINTER(aStorage);

    if (!aPrincipal)
        return NS_OK;

    nsCOMPtr<nsIDocShellTreeItem> topItem;
    nsresult rv = GetSameTypeRootTreeItem(getter_AddRefs(topItem));
    if (NS_FAILED(rv))
        return rv;

    if (topItem) {
        nsCOMPtr<nsIDocShell> topDocShell = do_QueryInterface(topItem);
        if (topDocShell == this) {
            nsXPIDLCString origin;
            rv = aPrincipal->GetOrigin(getter_Copies(origin));
            if (NS_FAILED(rv))
                return rv;

            if (origin.IsEmpty())
                return NS_ERROR_FAILURE;

            // Do not replace an existing session storage.
            if (mStorages.GetWeak(origin))
                return NS_ERROR_NOT_AVAILABLE;

#if defined(PR_LOGGING) && defined(DEBUG)
            PR_LOG(gDocShellLog, PR_LOG_DEBUG,
                   ("nsDocShell[%p]: was added a sessionStorage %p",
                    this, aStorage));
#endif
            mStorages.Put(origin, aStorage);
        }
        else {
            return topDocShell->AddSessionStorage(aPrincipal, aStorage);
        }
    }

    return NS_OK;
}

static PLDHashOperator
CloneSessionStorages(nsCStringHashKey::KeyType aKey, nsIDOMStorage* aStorage,
                     void* aUserArg)
{
    nsIDocShell *docShell = static_cast<nsIDocShell*>(aUserArg);
    nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(aStorage);

    if (pistorage) {
        nsCOMPtr<nsIDOMStorage> storage = pistorage->Clone();
        docShell->AddSessionStorage(pistorage->Principal(), storage);
    }

    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsDocShell::CloneSessionStoragesTo(nsIDocShell* aDocShell)
{
    aDocShell->ClearSessionStorages();
    mStorages.EnumerateRead(CloneSessionStorages, aDocShell);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ClearSessionStorages()
{
    mStorages.Clear();
    return NS_OK;
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
    return nsnull;
}

//*****************************************************************************
// nsDocShell::nsIDocShellTreeItem
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetName(PRUnichar ** aName)
{
    NS_ENSURE_ARG_POINTER(aName);
    *aName = ToNewUnicode(mName);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetName(const PRUnichar * aName)
{
    mName = aName;              // this does a copy of aName
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::NameEquals(const PRUnichar *aName, bool *_retval)
{
    NS_ENSURE_ARG_POINTER(aName);
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = mName.Equals(aName);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetItemType(PRInt32 * aItemType)
{
    NS_ENSURE_ARG_POINTER(aItemType);

    *aItemType = mItemType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetItemType(PRInt32 aItemType)
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

    nsRefPtr<nsPresContext> presContext = nsnull;
    GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
        presContext->InvalidateIsChromeCache();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParent(nsIDocShellTreeItem ** aParent)
{
    if (!mParent) {
        *aParent = nsnull;
    } else {
        CallQueryInterface(mParent, aParent);
    }
    // Note that in the case when the parent is not an nsIDocShellTreeItem we
    // don't want to throw; we just want to return null.
    return NS_OK;
}

nsresult
nsDocShell::SetDocLoaderParent(nsDocLoader * aParent)
{
    nsDocLoader::SetDocLoaderParent(aParent);

    // Curse ambiguous nsISupports inheritance!
    nsISupports* parent = GetAsSupports(aParent);

    // If parent is another docshell, we inherit all their flags for
    // allowing plugins, scripting etc.
    bool value;
    nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(parent));
    if (parentAsDocShell)
    {
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowPlugins(&value)))
        {
            SetAllowPlugins(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowJavascript(&value)))
        {
            SetAllowJavascript(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowMetaRedirects(&value)))
        {
            SetAllowMetaRedirects(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowSubframes(&value)))
        {
            SetAllowSubframes(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowImages(&value)))
        {
            SetAllowImages(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetAllowWindowControl(&value)))
        {
            SetAllowWindowControl(value);
        }
        if (NS_SUCCEEDED(parentAsDocShell->GetIsActive(&value)))
        {
            SetIsActive(value);
        }
        if (NS_FAILED(parentAsDocShell->GetAllowDNSPrefetch(&value))) {
            value = false;
        }
        SetAllowDNSPrefetch(value);
    }
    nsCOMPtr<nsILoadContext> parentAsLoadContext(do_QueryInterface(parent));
    if (parentAsLoadContext &&
        NS_SUCCEEDED(parentAsLoadContext->GetUsePrivateBrowsing(&value)))
    {
        SetUsePrivateBrowsing(value);
    }

    nsCOMPtr<nsIURIContentListener> parentURIListener(do_GetInterface(parent));
    if (parentURIListener)
        mContentListener->SetParentContentListener(parentURIListener);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParent(nsIDocShellTreeItem ** aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;

    nsCOMPtr<nsIDocShellTreeItem> parent =
        do_QueryInterface(GetAsSupports(mParent));
    if (!parent)
        return NS_OK;

    PRInt32 parentType;
    NS_ENSURE_SUCCESS(parent->GetItemType(&parentType), NS_ERROR_FAILURE);

    if (parentType == mItemType) {
        parent.swap(*aParent);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRootTreeItem(nsIDocShellTreeItem ** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = static_cast<nsIDocShellTreeItem *>(this);

    nsCOMPtr<nsIDocShellTreeItem> parent;
    NS_ENSURE_SUCCESS(GetParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);
    while (parent) {
        *aRootTreeItem = parent;
        NS_ENSURE_SUCCESS((*aRootTreeItem)->GetParent(getter_AddRefs(parent)),
                          NS_ERROR_FAILURE);
    }
    NS_ADDREF(*aRootTreeItem);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeRootTreeItem(nsIDocShellTreeItem ** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = static_cast<nsIDocShellTreeItem *>(this);

    nsCOMPtr<nsIDocShellTreeItem> parent;
    NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)),
                      NS_ERROR_FAILURE);
    while (parent) {
        *aRootTreeItem = parent;
        NS_ENSURE_SUCCESS((*aRootTreeItem)->
                          GetSameTypeParent(getter_AddRefs(parent)),
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

    // Now do a security check
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

    nsCOMPtr<nsIDOMWindow> targetWindow = do_GetInterface(aTargetItem);
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
ItemIsActive(nsIDocShellTreeItem *aItem)
{
    nsCOMPtr<nsIDOMWindow> window(do_GetInterface(aItem));

    if (window) {
        bool isClosed;

        if (NS_SUCCEEDED(window->GetClosed(&isClosed)) && !isClosed) {
            return true;
        }
    }

    return false;
}

NS_IMETHODIMP
nsDocShell::FindItemWithName(const PRUnichar * aName,
                             nsISupports * aRequestor,
                             nsIDocShellTreeItem * aOriginalRequestor,
                             nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    // If we don't find one, we return NS_OK and a null result
    *_retval = nsnull;

    if (!*aName)
        return NS_OK;

    if (!aRequestor)
    {
        nsCOMPtr<nsIDocShellTreeItem> foundItem;

        // This is the entry point into the target-finding algorithm.  Check
        // for special names.  This should only be done once, hence the check
        // for a null aRequestor.

        nsDependentString name(aName);
        if (name.LowerCaseEqualsLiteral("_self")) {
            foundItem = this;
        }
        else if (name.LowerCaseEqualsLiteral("_blank"))
        {
            // Just return null.  Caller must handle creating a new window with
            // a blank name himself.
            return NS_OK;
        }
        else if (name.LowerCaseEqualsLiteral("_parent"))
        {
            GetSameTypeParent(getter_AddRefs(foundItem));
            if(!foundItem)
                foundItem = this;
        }
        else if (name.LowerCaseEqualsLiteral("_top"))
        {
            GetSameTypeRootTreeItem(getter_AddRefs(foundItem));
            NS_ASSERTION(foundItem, "Must have this; worst case it's us!");
        }
        // _main is an IE target which should be case-insensitive but isn't
        // see bug 217886 for details
        else if (name.LowerCaseEqualsLiteral("_content") ||
                 name.EqualsLiteral("_main"))
        {
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
        }

        if (foundItem && !CanAccessItem(foundItem, aOriginalRequestor)) {
            foundItem = nsnull;
        }

        if (foundItem) {
            // We return foundItem here even if it's not an active
            // item since all the names we've dealt with so far are
            // special cases that we won't bother looking for further.

            foundItem.swap(*_retval);
            return NS_OK;
        }
    }

    // Keep looking
        
    // First we check our name.
    if (mName.Equals(aName) && ItemIsActive(this) &&
        CanAccessItem(this, aOriginalRequestor)) {
        NS_ADDREF(*_retval = this);
        return NS_OK;
    }

    // This QI may fail, but the places where we want to compare, comparing
    // against nsnull serves the same purpose.
    nsCOMPtr<nsIDocShellTreeItem> reqAsTreeItem(do_QueryInterface(aRequestor));

    // Second we check our children making sure not to ask a child if
    // it is the aRequestor.
#ifdef DEBUG
    nsresult rv =
#endif
    FindChildWithName(aName, true, true, reqAsTreeItem,
                      aOriginalRequestor, _retval);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "FindChildWithName should not be failing here.");
    if (*_retval)
        return NS_OK;
        
    // Third if we have a parent and it isn't the requestor then we
    // should ask it to do the search.  If it is the requestor we
    // should just stop here and let the parent do the rest.  If we
    // don't have a parent, then we should ask the
    // docShellTreeOwner to do the search.
    nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem =
        do_QueryInterface(GetAsSupports(mParent));
    if (parentAsTreeItem) {
        if (parentAsTreeItem == reqAsTreeItem)
            return NS_OK;

        PRInt32 parentType;
        parentAsTreeItem->GetItemType(&parentType);
        if (parentType == mItemType) {
            return parentAsTreeItem->
                FindItemWithName(aName,
                                 static_cast<nsIDocShellTreeItem*>
                                            (this),
                                 aOriginalRequestor,
                                 _retval);
        }
    }

    // If the parent is null or not of the same type fall through and ask tree
    // owner.

    // This may fail, but comparing against null serves the same purpose
    nsCOMPtr<nsIDocShellTreeOwner>
        reqAsTreeOwner(do_QueryInterface(aRequestor));

    if (mTreeOwner && mTreeOwner != reqAsTreeOwner) {
        return mTreeOwner->
            FindItemWithName(aName, this, aOriginalRequestor, _retval);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTreeOwner(nsIDocShellTreeOwner ** aTreeOwner)
{
    NS_ENSURE_ARG_POINTER(aTreeOwner);

    *aTreeOwner = mTreeOwner;
    NS_IF_ADDREF(*aTreeOwner);
    return NS_OK;
}

#ifdef DEBUG_DOCSHELL_FOCUS
static void 
PrintDocTree(nsIDocShellTreeItem * aParentNode, int aLevel)
{
  for (PRInt32 i=0;i<aLevel;i++) printf("  ");

  PRInt32 childWebshellCount;
  aParentNode->GetChildCount(&childWebshellCount);
  nsCOMPtr<nsIDocShell> parentAsDocShell(do_QueryInterface(aParentNode));
  PRInt32 type;
  aParentNode->GetItemType(&type);
  nsCOMPtr<nsIPresShell> presShell;
  parentAsDocShell->GetPresShell(getter_AddRefs(presShell));
  nsRefPtr<nsPresContext> presContext;
  parentAsDocShell->GetPresContext(getter_AddRefs(presContext));
  nsIDocument *doc = presShell->GetDocument();

  nsCOMPtr<nsIDOMWindow> domwin(doc->GetWindow());

  nsCOMPtr<nsIWidget> widget;
  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    vm->GetWidget(getter_AddRefs(widget));
  }
  dom::Element* rootElement = doc->GetRootElement();

  printf("DS %p  Ty %s  Doc %p DW %p EM %p CN %p\n",  
    (void*)parentAsDocShell.get(), 
    type==nsIDocShellTreeItem::typeChrome?"Chr":"Con", 
     (void*)doc, (void*)domwin.get(),
     (void*)presContext->EventStateManager(), (void*)rootElement);

  if (childWebshellCount > 0) {
    for (PRInt32 i=0;i<childWebshellCount;i++) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aParentNode->GetChildAt(i, getter_AddRefs(child));
      PrintDocTree(child, aLevel+1);
    }
  }
}

static void 
PrintDocTree(nsIDocShellTreeItem * aParentNode)
{
  NS_ASSERTION(aParentNode, "Pointer is null!");

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  aParentNode->GetParent(getter_AddRefs(parentItem));
  while (parentItem) {
    nsCOMPtr<nsIDocShellTreeItem>tmp;
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
nsDocShell::SetTreeOwner(nsIDocShellTreeOwner * aTreeOwner)
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
            nsCOMPtr<nsIWebProgressListener>
                oldListener(do_QueryInterface(mTreeOwner));
            nsCOMPtr<nsIWebProgressListener>
                newListener(do_QueryInterface(aTreeOwner));

            if (oldListener) {
                webProgress->RemoveProgressListener(oldListener);
            }

            if (newListener) {
                webProgress->AddProgressListener(newListener,
                                                 nsIWebProgress::NOTIFY_ALL);
            }
        }
    }

    mTreeOwner = aTreeOwner;    // Weak reference per API

    PRInt32 i, n = mChildList.Count();
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child = do_QueryInterface(ChildAt(i));
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
        PRInt32 childType = ~mItemType; // Set it to not us in case the get fails
        child->GetItemType(&childType); // We don't care if this fails, if it does we won't set the owner
        if (childType == mItemType)
            child->SetTreeOwner(aTreeOwner);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChildOffset(PRUint32 aChildOffset)
{
    mChildOffset = aChildOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHistoryID(PRUint64* aID)
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

//*****************************************************************************
// nsDocShell::nsIDocShellTreeNode
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetChildCount(PRInt32 * aChildCount)
{
    NS_ENSURE_ARG_POINTER(aChildCount);
    *aChildCount = mChildList.Count();
    return NS_OK;
}



NS_IMETHODIMP
nsDocShell::AddChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    nsRefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
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
    aChild->SetTreeOwner(nsnull);
    
    nsresult res = AddChildLoader(childAsDocLoader);
    NS_ENSURE_SUCCESS(res, res);
    NS_ASSERTION(mChildList.Count() > 0,
                 "child list must not be empty after a successful add");

    nsCOMPtr<nsIDocShellHistory> docshellhistory = do_QueryInterface(aChild);
    bool dynamic = false;
    docshellhistory->GetCreatedDynamically(&dynamic);
    if (!dynamic) {
        nsCOMPtr<nsISHEntry> currentSH;
        bool oshe = false;
        GetCurrentSHEntry(getter_AddRefs(currentSH), &oshe);
        if (currentSH) {
            currentSH->HasDynamicallyAddedChild(&dynamic);
        }
    }
    nsCOMPtr<nsIDocShell> childDocShell = do_QueryInterface(aChild);
    childDocShell->SetChildOffset(dynamic ? -1 : mChildList.Count() - 1);

    /* Set the child's global history if the parent has one */
    if (mUseGlobalHistory) {
        nsCOMPtr<nsIDocShellHistory>
            dsHistoryChild(do_QueryInterface(aChild));
        if (dsHistoryChild)
            dsHistoryChild->SetUseGlobalHistory(true);
    }


    PRInt32 childType = ~mItemType;     // Set it to not us in case the get fails
    aChild->GetItemType(&childType);
    if (childType != mItemType)
        return NS_OK;
    // Everything below here is only done when the child is the same type.


    aChild->SetTreeOwner(mTreeOwner);

    nsCOMPtr<nsIDocShell> childAsDocShell(do_QueryInterface(aChild));
    if (!childAsDocShell)
        return NS_OK;

    // charset, style-disabling, and zoom will be inherited in SetupNewViewer()

    // Now take this document's charset and set the child's parentCharset field
    // to it. We'll later use that field, in the loading process, for the
    // charset choosing algorithm.
    // If we fail, at any point, we just return NS_OK.
    // This code has some performance impact. But this will be reduced when 
    // the current charset will finally be stored as an Atom, avoiding the
    // alias resolution extra look-up.

    // we are NOT going to propagate the charset is this Chrome's docshell
    if (mItemType == nsIDocShellTreeItem::typeChrome)
        return NS_OK;

    // get the parent's current charset
    if (!mContentViewer)
        return NS_OK;
    nsIDocument* doc = mContentViewer->GetDocument();
    if (!doc)
        return NS_OK;
    const nsACString &parentCS = doc->GetDocumentCharacterSet();

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

        // set the child's parentCharset
        nsCOMPtr<nsIAtom> parentCSAtom(do_GetAtom(parentCS));
        res = childAsDocShell->SetParentCharset(parentCSAtom);
        if (NS_FAILED(res))
            return NS_OK;

        PRInt32 charsetSource = doc->GetDocumentCharacterSetSource();

        // set the child's parentCharset
        res = childAsDocShell->SetParentCharsetSource(charsetSource);
        if (NS_FAILED(res))
            return NS_OK;
    }

    // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n", NS_LossyConvertUTF16toASCII(parentCS).get(), mItemType);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveChild(nsIDocShellTreeItem * aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

    nsRefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
    NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);
    
    nsresult rv = RemoveChildLoader(childAsDocLoader);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aChild->SetTreeOwner(nsnull);

    return nsDocLoader::AddDocLoaderAsChildOfRoot(childAsDocLoader);
}

NS_IMETHODIMP
nsDocShell::GetChildAt(PRInt32 aIndex, nsIDocShellTreeItem ** aChild)
{
    NS_ENSURE_ARG_POINTER(aChild);

#ifdef DEBUG
    if (aIndex < 0) {
      NS_WARNING("Negative index passed to GetChildAt");
    }
    else if (aIndex >= mChildList.Count()) {
      NS_WARNING("Too large an index passed to GetChildAt");
    }
#endif

    nsIDocumentLoader* child = SafeChildAt(aIndex);
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);
    
    return CallQueryInterface(child, aChild);
}

NS_IMETHODIMP
nsDocShell::FindChildWithName(const PRUnichar * aName,
                              bool aRecurse, bool aSameType,
                              nsIDocShellTreeItem * aRequestor,
                              nsIDocShellTreeItem * aOriginalRequestor,
                              nsIDocShellTreeItem ** _retval)
{
    NS_ENSURE_ARG(aName);
    NS_ENSURE_ARG_POINTER(_retval);

    *_retval = nsnull;          // if we don't find one, we return NS_OK and a null result 

    if (!*aName)
        return NS_OK;

    nsXPIDLString childName;
    PRInt32 i, n = mChildList.Count();
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child = do_QueryInterface(ChildAt(i));
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);
        PRInt32 childType;
        child->GetItemType(&childType);

        if (aSameType && (childType != mItemType))
            continue;

        bool childNameEquals = false;
        child->NameEquals(aName, &childNameEquals);
        if (childNameEquals && ItemIsActive(child) &&
            CanAccessItem(child, aOriginalRequestor)) {
            child.swap(*_retval);
            break;
        }

        if (childType != mItemType)     //Only ask it to check children if it is same type
            continue;

        if (aRecurse && (aRequestor != child))  // Only ask the child if it isn't the requestor
        {
            // See if child contains the shell with the given name
#ifdef DEBUG
            nsresult rv =
#endif
            child->FindChildWithName(aName, true,
                                     aSameType,
                                     static_cast<nsIDocShellTreeItem*>
                                                (this),
                                     aOriginalRequestor,
                                     _retval);
            NS_ASSERTION(NS_SUCCEEDED(rv),
                         "FindChildWithName should not fail here");
            if (*_retval)           // found it
                return NS_OK;
        }
    }
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIDocShellHistory
//*****************************************************************************   
NS_IMETHODIMP
nsDocShell::GetChildSHEntry(PRInt32 aChildOffset, nsISHEntry ** aResult)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;

    
    // A nsISHEntry for a child is *only* available when the parent is in
    // the progress of loading a document too...
    
    if (mLSHE) {
        /* Before looking for the subframe's url, check
         * the expiration status of the parent. If the parent
         * has expired from cache, then subframes will not be 
         * loaded from history in certain situations.  
         */
        bool parentExpired=false;
        mLSHE->GetExpirationStatus(&parentExpired);
        
        /* Get the parent's Load Type so that it can be set on the child too.
         * By default give a loadHistory value
         */
        PRUint32 loadType = nsIDocShellLoadInfo::loadHistory;
        mLSHE->GetLoadType(&loadType);  
        // If the user did a shift-reload on this frameset page, 
        // we don't want to load the subframes from history.
        if (loadType == nsIDocShellLoadInfo::loadReloadBypassCache ||
            loadType == nsIDocShellLoadInfo::loadReloadBypassProxy ||
            loadType == nsIDocShellLoadInfo::loadReloadBypassProxyAndCache ||
            loadType == nsIDocShellLoadInfo::loadRefresh)
            return rv;
        
        /* If the user pressed reload and the parent frame has expired
         *  from cache, we do not want to load the child frame from history.
         */
        if (parentExpired && (loadType == nsIDocShellLoadInfo::loadReloadNormal)) {
            // The parent has expired. Return null.
            *aResult = nsnull;
            return rv;
        }

        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE));
        if (container) {
            // Get the child subframe from session history.
            rv = container->GetChildAt(aChildOffset, aResult);            
            if (*aResult) 
                (*aResult)->SetLoadType(loadType);            
        }
    }
    return rv;
}

NS_IMETHODIMP
nsDocShell::AddChildSHEntry(nsISHEntry * aCloneRef, nsISHEntry * aNewEntry,
                            PRInt32 aChildOffset, PRUint32 loadType,
                            bool aCloneChildren)
{
    nsresult rv;

    if (mLSHE && loadType != LOAD_PUSHSTATE) {
        /* You get here if you are currently building a 
         * hierarchy ie.,you just visited a frameset page
         */
        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mLSHE, &rv));
        if (container) {
            rv = container->AddChild(aNewEntry, aChildOffset);
        }
    }
    else if (!aCloneRef) {
        /* This is an initial load in some subframe.  Just append it if we can */
        nsCOMPtr<nsISHContainer> container(do_QueryInterface(mOSHE, &rv));
        if (container) {
            rv = container->AddChild(aNewEntry, aChildOffset);
        }
    }
    else if (mSessionHistory) {
        /* You are currently in the rootDocShell.
         * You will get here when a subframe has a new url
         * to load and you have walked up the tree all the 
         * way to the top to clone the current SHEntry hierarchy
         * and replace the subframe where a new url was loaded with
         * a new entry.
         */
        PRInt32 index = -1;
        nsCOMPtr<nsIHistoryEntry> currentHE;
        mSessionHistory->GetIndex(&index);
        if (index < 0)
            return NS_ERROR_FAILURE;

        rv = mSessionHistory->GetEntryAtIndex(index, false,
                                              getter_AddRefs(currentHE));
        NS_ENSURE_TRUE(currentHE, NS_ERROR_FAILURE);

        nsCOMPtr<nsISHEntry> currentEntry(do_QueryInterface(currentHE));
        if (currentEntry) {
            PRUint32 cloneID = 0;
            nsCOMPtr<nsISHEntry> nextEntry;
            aCloneRef->GetID(&cloneID);
            rv = CloneAndReplace(currentEntry, this, cloneID, aNewEntry,
                                 aCloneChildren, getter_AddRefs(nextEntry));

            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsISHistoryInternal>
                    shPrivate(do_QueryInterface(mSessionHistory));
                NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
                rv = shPrivate->AddEntry(nextEntry, true);
            }
        }
    }
    else {
        /* Just pass this along */
        nsCOMPtr<nsIDocShellHistory> parent =
            do_QueryInterface(GetAsSupports(mParent), &rv);
        if (parent) {
            rv = parent->AddChildSHEntry(aCloneRef, aNewEntry, aChildOffset,
                                         loadType, aCloneChildren);
        }          
    }
    return rv;
}

nsresult
nsDocShell::DoAddChildSHEntry(nsISHEntry* aNewEntry, PRInt32 aChildOffset,
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
    nsCOMPtr<nsIDocShellHistory> parent =
        do_QueryInterface(GetAsSupports(mParent), &rv);
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
        mGlobalHistory = nsnull;
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
nsDocShell::GetUseGlobalHistory(bool *aUseGlobalHistory)
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
        nsCOMPtr<nsIWebNavigation> rootAsWebnav =
            do_QueryInterface(root);
        if (rootAsWebnav) {
            rootAsWebnav->GetSessionHistory(getter_AddRefs(sessionHistory));
            internalHistory = do_QueryInterface(sessionHistory);
        }
    }
    if (!internalHistory) {
        return NS_OK;
    }

    PRInt32 index = 0;
    sessionHistory->GetIndex(&index);
    nsAutoTArray<PRUint64, 16> ids;
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
    *aEntry = nsnull;
    if (mLSHE) {
        NS_ADDREF(*aEntry = mLSHE);
    } else if (mOSHE) {
        NS_ADDREF(*aEntry = mOSHE);
        *aOSHE = true;
    }
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

  PRInt32 count = 0;
  shcontainer->GetChildCount(&count);
  nsAutoTArray<PRUint64, 16> ids;
  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    shcontainer->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      PRUint64 id = 0;
      child->GetDocshellID(&id);
      ids.AppendElement(id);
    }
  }
  PRInt32 index = 0;
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
    DisplayLoadError(NS_ERROR_DOCUMENT_IS_PRINTMODE, nsnull, nsnull);
  }

  return mIsPrintingOrPP;
}

bool
nsDocShell::IsNavigationAllowed(bool aDisplayPrintErrorDialog)
{
    return !IsPrintingOrPP(aDisplayPrintErrorDialog) && !mFiredUnloadEvent;
}

//*****************************************************************************
// nsDocShell::nsIWebNavigation
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetCanGoBack(bool * aCanGoBack)
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
nsDocShell::GetCanGoForward(bool * aCanGoForward)
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

NS_IMETHODIMP nsDocShell::GotoIndex(PRInt32 aIndex)
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
nsDocShell::LoadURI(const PRUnichar * aURI,
                    PRUint32 aLoadFlags,
                    nsIURI * aReferringURI,
                    nsIInputStream * aPostStream,
                    nsIInputStream * aHeaderStream)
{
    NS_ASSERTION((aLoadFlags & 0xf) == 0, "Unexpected flags");
    
    if (!IsNavigationAllowed()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsCOMPtr<nsIURI> uri;
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
    
    if (sURIFixup) {
        // Call the fixup object.  This will clobber the rv from NS_NewURI
        // above, but that's fine with us.  Note that we need to do this even
        // if NS_NewURI returned a URI, because fixup handles nested URIs, etc
        // (things like view-source:mozilla.org for example).
        PRUint32 fixupFlags = 0;
        if (aLoadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
          fixupFlags |= nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
        }
        if (aLoadFlags & LOAD_FLAGS_URI_IS_UTF8) {
          fixupFlags |= nsIURIFixup::FIXUP_FLAG_USE_UTF8;
        }
        rv = sURIFixup->CreateFixupURI(uriString, fixupFlags,
                                       getter_AddRefs(uri));
    }
    // else no fixup service so just use the URI we created and see
    // what happens

    if (NS_ERROR_MALFORMED_URI == rv) {
        DisplayLoadError(rv, uri, aURI);
    }

    if (NS_FAILED(rv) || !uri)
        return NS_ERROR_FAILURE;

    PopupControlState popupState;
    if (aLoadFlags & LOAD_FLAGS_ALLOW_POPUPS) {
        popupState = openAllowed;
        aLoadFlags &= ~LOAD_FLAGS_ALLOW_POPUPS;
    } else {
        popupState = openOverridden;
    }
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
    nsAutoPopupStatePusher statePusher(win, popupState);

    // Don't pass certain flags that aren't needed and end up confusing
    // ConvertLoadTypeToDocShellLoadInfo.  We do need to ensure that they are
    // passed to LoadURI though, since it uses them.
    PRUint32 extraFlags = (aLoadFlags & EXTRA_LOAD_FLAGS);
    aLoadFlags &= ~EXTRA_LOAD_FLAGS;

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    rv = CreateLoadInfo(getter_AddRefs(loadInfo));
    if (NS_FAILED(rv)) return rv;
    
    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_NORMAL, aLoadFlags);
    loadInfo->SetLoadType(ConvertLoadTypeToDocShellLoadInfo(loadType));
    loadInfo->SetPostDataStream(aPostStream);
    loadInfo->SetReferrer(aReferringURI);
    loadInfo->SetHeadersStream(aHeaderStream);

    rv = LoadURI(uri, loadInfo, extraFlags, true);

    // Save URI string in case it's needed later when
    // sending to search engine service in EndPageLoad()
    mOriginalUriString = uriString; 

    return rv;
}

NS_IMETHODIMP
nsDocShell::DisplayLoadError(nsresult aError, nsIURI *aURI,
                             const PRUnichar *aURL,
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
    const PRUint32 kMaxFormatStrArgs = 3;
    nsAutoString formatStrs[kMaxFormatStrArgs];
    PRUint32 formatStrCount = 0;
    bool addHostPort = false;
    nsresult rv = NS_OK;
    nsAutoString messageStr;
    nsCAutoString cssClass;
    nsCAutoString errorPage;

    errorPage.AssignLiteral("neterror");

    // Turn the error code into a human readable error message.
    if (NS_ERROR_UNKNOWN_PROTOCOL == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // extract the scheme
        nsCAutoString scheme;
        aURI->GetScheme(scheme);
        CopyASCIItoUTF16(scheme, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("protocolNotFound");
    }
    else if (NS_ERROR_FILE_NOT_FOUND == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        error.AssignLiteral("fileNotFound");
    }
    else if (NS_ERROR_UNKNOWN_HOST == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Get the host
        nsCAutoString host;
        nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(aURI);
        innermostURI->GetHost(host);
        CopyUTF8toUTF16(host, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("dnsNotFound");
    }
    else if(NS_ERROR_CONNECTION_REFUSED == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        addHostPort = true;
        error.AssignLiteral("connectionFailure");
    }
    else if(NS_ERROR_NET_INTERRUPT == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        addHostPort = true;
        error.AssignLiteral("netInterrupt");
    }
    else if (NS_ERROR_NET_TIMEOUT == aError) {
        NS_ENSURE_ARG_POINTER(aURI);
        // Get the host
        nsCAutoString host;
        aURI->GetHost(host);
        CopyUTF8toUTF16(host, formatStrs[0]);
        formatStrCount = 1;
        error.AssignLiteral("netTimeout");
    }
    else if (NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION == aError) {
        // CSP error
        cssClass.AssignLiteral("neterror");
        error.AssignLiteral("cspFrameAncestorBlocked");
    }
    else if (NS_ERROR_GET_MODULE(aError) == NS_ERROR_MODULE_SECURITY) {
        nsCOMPtr<nsINSSErrorsService> nsserr =
            do_GetService(NS_NSS_ERRORS_SERVICE_CONTRACTID);

        PRUint32 errorClass;
        if (!nsserr ||
            NS_FAILED(nsserr->GetErrorClass(aError, &errorClass))) {
          errorClass = nsINSSErrorsService::ERROR_CLASS_SSL_PROTOCOL;
        }

        nsCOMPtr<nsISupports> securityInfo;
        nsCOMPtr<nsITransportSecurityInfo> tsi;
        if (aFailedChannel)
            aFailedChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
        tsi = do_QueryInterface(securityInfo);
        if (tsi) {
            // Usually we should have aFailedChannel and get a detailed message
            tsi->GetErrorMessage(getter_Copies(messageStr));
        }
        else {
            // No channel, let's obtain the generic error message
            if (nsserr) {
                nsserr->GetErrorMessage(aError, messageStr);
            }
        }
        if (!messageStr.IsEmpty()) {
            if (errorClass == nsINSSErrorsService::ERROR_CLASS_BAD_CERT) {
                error.AssignLiteral("nssBadCert");

                // if this is a Strict-Transport-Security host and the cert
                // is bad, don't allow overrides (STS Spec section 7.3).
                nsCOMPtr<nsIStrictTransportSecurityService> stss =
                          do_GetService(NS_STSSERVICE_CONTRACTID, &rv);
                NS_ENSURE_SUCCESS(rv, rv);

                bool isStsHost = false;
                rv = stss->IsStsURI(aURI, &isStsHost);
                NS_ENSURE_SUCCESS(rv, rv);

                if (isStsHost)
                  cssClass.AssignLiteral("badStsCert");

                if (Preferences::GetBool(
                        "browser.xul.error_pages.expert_bad_cert", false)) {
                    cssClass.AssignLiteral("expertBadCert");
                }

                // See if an alternate cert error page is registered
                nsAdoptingCString alternateErrorPage =
                    Preferences::GetCString(
                        "security.alternate_certificate_error_page");
                if (alternateErrorPage)
                    errorPage.Assign(alternateErrorPage);
            } else {
                error.AssignLiteral("nssFailure2");
            }
        }
    } else if (NS_ERROR_PHISHING_URI == aError || NS_ERROR_MALWARE_URI == aError) {
        nsCAutoString host;
        aURI->GetHost(host);
        CopyUTF8toUTF16(host, formatStrs[0]);
        formatStrCount = 1;

        // Malware and phishing detectors may want to use an alternate error
        // page, but if the pref's not set, we'll fall back on the standard page
        nsAdoptingCString alternateErrorPage =
            Preferences::GetCString("urlclassifier.alternate_error_page");
        if (alternateErrorPage)
            errorPage.Assign(alternateErrorPage);

        if (NS_ERROR_PHISHING_URI == aError)
            error.AssignLiteral("phishingBlocked");
        else
            error.AssignLiteral("malwareBlocked");
        cssClass.AssignLiteral("blacklist");
    }
    else {
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
        {
            error.AssignLiteral("remoteXUL");
            break;
        }
        case NS_ERROR_UNSAFE_CONTENT_TYPE:
            // Channel refused to load from an unrecognized content type.
            error.AssignLiteral("unsafeContentType");
            break;
        case NS_ERROR_CORRUPTED_CONTENT:
            // Broken Content Detected. e.g. Content-MD5 check failure.
            error.AssignLiteral("corruptedContentError");
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
    }
    else {
        if (addHostPort) {
            // Build up the host:port string.
            nsCAutoString hostport;
            if (aURI) {
                aURI->GetHostPort(hostport);
            } else {
                hostport.AssignLiteral("?");
            }
            CopyUTF8toUTF16(hostport, formatStrs[formatStrCount++]);
        }

        nsCAutoString spec;
        rv = NS_ERROR_NOT_AVAILABLE;
        if (aURI) {
            // displaying "file://" is aesthetically unpleasing and could even be
            // confusing to the user
            bool isFileURI = false;
            rv = aURI->SchemeIs("file", &isFileURI);
            if (NS_SUCCEEDED(rv) && isFileURI)
                aURI->GetPath(spec);
            else
                aURI->GetSpec(spec);

            nsCAutoString charset;
            // unescape and convert from origin charset
            aURI->GetOriginCharset(charset);
            nsCOMPtr<nsITextToSubURI> textToSubURI(
                do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
            if (NS_SUCCEEDED(rv)) {
                rv = textToSubURI->UnEscapeURIForUI(charset, spec, formatStrs[formatStrCount]);
            }
        } else {
            spec.AssignLiteral("?");
        }
        if (NS_FAILED(rv))
            CopyUTF8toUTF16(spec, formatStrs[formatStrCount]);
        rv = NS_OK;
        ++formatStrCount;

        const PRUnichar *strs[kMaxFormatStrArgs];
        for (PRUint32 i = 0; i < formatStrCount; i++) {
            strs[i] = formatStrs[i].get();
        }
        nsXPIDLString str;
        rv = stringBundle->FormatStringFromName(
            error.get(),
            strs, formatStrCount, getter_Copies(str));
        NS_ENSURE_SUCCESS(rv, rv);
        messageStr.Assign(str.get());
    }

    // Display the error as a page or an alert prompt
    NS_ENSURE_FALSE(messageStr.IsEmpty(), NS_ERROR_FAILURE);
    if (mUseErrorPages) {
        // Display an error page
        LoadErrorPage(aURI, aURL, errorPage.get(), error.get(),
                      messageStr.get(), cssClass.get(), aFailedChannel);
    } 
    else
    {
        // The prompter reqires that our private window has a document (or it
        // asserts). Satisfy that assertion now since GetDocument will force
        // creation of one if it hasn't already been created.
        nsCOMPtr<nsPIDOMWindow> pwin(do_QueryInterface(mScriptGlobal));
        if (pwin) {
            nsCOMPtr<nsIDOMDocument> doc;
            pwin->GetDocument(getter_AddRefs(doc));
        }

        // Display a message box
        prompter->Alert(nsnull, messageStr.get());
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDocShell::LoadErrorPage(nsIURI *aURI, const PRUnichar *aURL,
                          const char *aErrorPage,
                          const PRUnichar *aErrorType,
                          const PRUnichar *aDescription,
                          const char *aCSSClass,
                          nsIChannel* aFailedChannel)
{
#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aFailedChannel)
            aFailedChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
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

    nsCAutoString url;
    nsCAutoString charset;
    if (aURI)
    {
        nsresult rv = aURI->GetSpec(url);
        rv |= aURI->GetOriginCharset(charset);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aURL)
    {
        // We need a URI object to store a session history entry, so make up a URI
        nsresult rv = NS_NewURI(getter_AddRefs(mFailedURI), "about:blank");
        NS_ENSURE_SUCCESS(rv, rv);

        CopyUTF16toUTF8(aURL, url);
    }
    else
    {
        return NS_ERROR_INVALID_POINTER;
    }

    // Create a URL to pass all the error information through to the page.

#undef SAFE_ESCAPE
#define SAFE_ESCAPE(cstring, escArg1, escArg2)  \
    {                                           \
        char* s = nsEscape(escArg1, escArg2);   \
        if (!s)                                 \
            return NS_ERROR_OUT_OF_MEMORY;      \
        cstring.Adopt(s);                       \
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
        errorPageUrl.AppendASCII("&s=");
        errorPageUrl.AppendASCII(escapedCSSClass.get());
    }
    errorPageUrl.AppendLiteral("&c=");
    errorPageUrl.AppendASCII(escapedCharset.get());
    errorPageUrl.AppendLiteral("&d=");
    errorPageUrl.AppendASCII(escapedDescription.get());

    nsCOMPtr<nsIURI> errorPageURI;
    nsresult rv = NS_NewURI(getter_AddRefs(errorPageURI), errorPageUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    return InternalLoad(errorPageURI, nsnull, nsnull,
                        INTERNAL_LOAD_FLAGS_INHERIT_OWNER, nsnull, nsnull,
                        nsnull, nsnull, LOAD_ERROR_PAGE,
                        nsnull, true, nsnull, nsnull);
}


NS_IMETHODIMP
nsDocShell::Reload(PRUint32 aReloadFlags)
{
    if (!IsNavigationAllowed()) {
      return NS_OK; // JS may not handle returning of an error code
    }
    nsresult rv;
    NS_ASSERTION(((aReloadFlags & 0xf) == 0),
                 "Reload command not updated to use load flags!");
    NS_ASSERTION((aReloadFlags & EXTRA_LOAD_FLAGS) == 0,
                 "Don't pass these flags to Reload");

    PRUint32 loadType = MAKE_LOAD_TYPE(LOAD_RELOAD_NORMAL, aReloadFlags);
    NS_ENSURE_TRUE(IsValidLoadType(loadType), NS_ERROR_INVALID_ARG);

    // Send notifications to the HistoryListener if any, about the impending reload
    nsCOMPtr<nsISHistory> rootSH;
    rv = GetRootSessionHistory(getter_AddRefs(rootSH));
    nsCOMPtr<nsISHistoryInternal> shistInt(do_QueryInterface(rootSH));
    bool canReload = true; 
    if (rootSH) {
      nsCOMPtr<nsISHistoryListener> listener;
      shistInt->GetListener(getter_AddRefs(listener));
      if (listener) {
        listener->OnHistoryReload(mCurrentURI, aReloadFlags, &canReload);
      }
    }

    if (!canReload)
      return NS_OK;
    
    /* If you change this part of code, make sure bug 45297 does not re-occur */
    if (mOSHE) {
        rv = LoadHistoryEntry(mOSHE, loadType);
    }
    else if (mLSHE) { // In case a reload happened before the current load is done
        rv = LoadHistoryEntry(mLSHE, loadType);
    }
    else {
        nsCOMPtr<nsIDocument> doc(do_GetInterface(GetAsSupports(this)));

        nsIPrincipal* principal = nsnull;
        nsAutoString contentTypeHint;
        if (doc) {
            principal = doc->NodePrincipal();
            doc->GetContentType(contentTypeHint);
        }

        rv = InternalLoad(mCurrentURI,
                          mReferrerURI,
                          principal,
                          INTERNAL_LOAD_FLAGS_NONE, // Do not inherit owner from document
                          nsnull,         // No window target
                          NS_LossyConvertUTF16toASCII(contentTypeHint).get(),
                          nsnull,         // No post data
                          nsnull,         // No headers data
                          loadType,       // Load type
                          nsnull,         // No SHEntry
                          true,
                          nsnull,         // No nsIDocShell
                          nsnull);        // No nsIRequest
    }
    

    return rv;
}

NS_IMETHODIMP
nsDocShell::Stop(PRUint32 aStopFlags)
{
    // Revoke any pending event related to content viewer restoration
    mRestorePresentationEvent.Revoke();

    if (mLoadType == LOAD_ERROR_PAGE) {
        if (mLSHE) {
            // Since error page loads never unset mLSHE, do so now
            SetHistoryEntry(&mOSHE, mLSHE);
            SetHistoryEntry(&mLSHE, nsnull);
        }

        mFailedChannel = nsnull;
        mFailedURI = nsnull;
    }

    if (nsIWebNavigation::STOP_CONTENT & aStopFlags) {
        // Stop the document loading
        if (mContentViewer)
            mContentViewer->Stop();
    }

    if (nsIWebNavigation::STOP_NETWORK & aStopFlags) {
        // Suspend any timers that were set for this loader.  We'll clear
        // them out for good in CreateContentViewer.
        if (mRefreshURIList) {
            SuspendRefreshURIs();
            mSavedRefreshURIList.swap(mRefreshURIList);
            mRefreshURIList = nsnull;
        }

        // XXXbz We could also pass |this| to nsIURILoader::Stop.  That will
        // just call Stop() on us as an nsIDocumentLoader... We need fewer
        // redundant apis!
        Stop();
    }

    PRInt32 n;
    PRInt32 count = mChildList.Count();
    for (n = 0; n < count; n++) {
        nsCOMPtr<nsIWebNavigation> shellAsNav(do_QueryInterface(ChildAt(n)));
        if (shellAsNav)
            shellAsNav->Stop(aStopFlags);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocument(nsIDOMDocument ** aDocument)
{
    NS_ENSURE_ARG_POINTER(aDocument);
    NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);

    return mContentViewer->GetDOMDocument(aDocument);
}

NS_IMETHODIMP
nsDocShell::GetCurrentURI(nsIURI ** aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    if (mCurrentURI) {
        return NS_EnsureSafeToReturn(mCurrentURI, aURI);
    }

    *aURI = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetReferringURI(nsIURI ** aURI)
{
    NS_ENSURE_ARG_POINTER(aURI);

    *aURI = mReferrerURI;
    NS_IF_ADDREF(*aURI);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSessionHistory(nsISHistory * aSessionHistory)
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
    if (root.get() == static_cast<nsIDocShellTreeItem *>(this)) {
        mSessionHistory = aSessionHistory;
        nsCOMPtr<nsISHistoryInternal>
            shPrivate(do_QueryInterface(mSessionHistory));
        NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
        shPrivate->SetRootDocShell(this);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;

}


NS_IMETHODIMP
nsDocShell::GetSessionHistory(nsISHistory ** aSessionHistory)
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
nsDocShell::LoadPage(nsISupports *aPageDescriptor, PRUint32 aDisplayType)
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
        if (NS_FAILED(rv))
              return rv;

        oldUri->GetSpec(spec);
        newSpec.AppendLiteral("view-source:");
        newSpec.Append(spec);

        rv = NS_NewURI(getter_AddRefs(newUri), newSpec);
        if (NS_FAILED(rv)) {
            return rv;
        }
        shEntry->SetURI(newUri);
    }

    rv = LoadHistoryEntry(shEntry, LOAD_HISTORY);
    return rv;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDescriptor(nsISupports **aPageDescriptor)
{
    NS_PRECONDITION(aPageDescriptor, "Null out param?");

    *aPageDescriptor = nsnull;

    nsISHEntry* src = mOSHE ? mOSHE : mLSHE;
    if (src) {
        nsCOMPtr<nsISHEntry> dest;

        nsresult rv = src->Clone(getter_AddRefs(dest));
        if (NS_FAILED(rv)) {
            return rv;
        }

        // null out inappropriate cloned attributes...
        dest->SetParent(nsnull);
        dest->SetIsSubFrame(false);
        
        return CallQueryInterface(dest, aPageDescriptor);
    }

    return NS_ERROR_NOT_AVAILABLE;
}


//*****************************************************************************
// nsDocShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::InitWindow(nativeWindow parentNativeWindow,
                       nsIWidget * parentWidget, PRInt32 x, PRInt32 y,
                       PRInt32 cx, PRInt32 cy)
{
    SetParentWidget(parentWidget);
    SetPositionAndSize(x, y, cx, cy, false);

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

    mAllowSubframes =
        Preferences::GetBool("browser.frames.enabled", mAllowSubframes);

    if (gValidateOrigin == 0xffffffff) {
        // Check pref to see if we should prevent frameset spoofing
        gValidateOrigin =
            Preferences::GetBool("browser.frame.validate_origin", true);
    }

    // Should we use XUL error pages instead of alerts if possible?
    mUseErrorPages =
        Preferences::GetBool("browser.xul.error_pages.enabled", mUseErrorPages);

    if (mObserveErrorPages) {
        Preferences::AddStrongObserver(this, "browser.xul.error_pages.enabled");
    }

    nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
    if (serv) {
        const char* msg = mItemType == typeContent ?
            NS_WEBNAVIGATION_CREATE : NS_CHROME_WEBNAVIGATION_CREATE;
        serv->NotifyObservers(GetAsSupports(this), msg, nsnull);
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
            serv->NotifyObservers(GetAsSupports(this), msg, nsnull);
        }
    }
    
    mIsBeingDestroyed = true;

    // Remove our pref observers
    if (mObserveErrorPages) {
        Preferences::RemoveObserver(this, "browser.xul.error_pages.enabled");
        mObserveErrorPages = false;
    }

    // Make sure to blow away our mLoadingURI just in case.  No loads
    // from inside this pagehide.
    mLoadingURI = nsnull;

    // Fire unload event before we blow anything away.
    (void) FirePageHideNotification(true);

    // Clear pointers to any detached nsEditorData that's lying
    // around in shistory entries. Breaks cycle. See bug 430921.
    if (mOSHE)
      mOSHE->SetEditorData(nsnull);
    if (mLSHE)
      mLSHE->SetEditorData(nsnull);
      
    // Note: mContentListener can be null if Init() failed and we're being
    // called from the destructor.
    if (mContentListener) {
        mContentListener->DropDocShellreference();
        mContentListener->SetParentContentListener(nsnull);
        // Note that we do NOT set mContentListener to null here; that
        // way if someone tries to do a load in us after this point
        // the nsDSURIContentListener will block it.  All of which
        // means that we should do this before calling Stop(), of
        // course.
    }

    // Stop any URLs that are currently being loaded...
    Stop(nsIWebNavigation::STOP_ALL);

    mEditorData = nsnull;

    mTransferableHookData = nsnull;

    // Save the state of the current document, before destroying the window.
    // This is needed to capture the state of a frameset when the new document
    // causes the frameset to be destroyed...
    PersistLayoutHistoryState();

    // Remove this docshell from its parent's child list
    nsCOMPtr<nsIDocShellTreeItem> docShellParentAsItem =
        do_QueryInterface(GetAsSupports(mParent));
    if (docShellParentAsItem)
        docShellParentAsItem->RemoveChild(this);

    if (mContentViewer) {
        mContentViewer->Close(nsnull);
        mContentViewer->Destroy();
        mContentViewer = nsnull;
    }

    nsDocLoader::Destroy();
    
    mParentWidget = nsnull;
    mCurrentURI = nsnull;

    if (mScriptGlobal) {
        nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
        win->DetachFromDocShell();

        mScriptGlobal = nsnull;
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
        mSessionHistory = nsnull;
    }

    SetTreeOwner(nsnull);

    // required to break ref cycle
    mSecurityUI = nsnull;

    // Cancel any timers that were set for this docshell; this is needed
    // to break the cycle between us and the timers.
    CancelRefreshURITimers();
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPosition(PRInt32 x, PRInt32 y)
{
    mBounds.x = x;
    mBounds.y = y;

    if (mContentViewer)
        NS_ENSURE_SUCCESS(mContentViewer->Move(x, y), NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPosition(PRInt32 * aX, PRInt32 * aY)
{
    PRInt32 dummyHolder;
    return GetPositionAndSize(aX, aY, &dummyHolder, &dummyHolder);
}

NS_IMETHODIMP
nsDocShell::SetSize(PRInt32 aCX, PRInt32 aCY, bool aRepaint)
{
    PRInt32 x = 0, y = 0;
    GetPosition(&x, &y);
    return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP
nsDocShell::GetSize(PRInt32 * aCX, PRInt32 * aCY)
{
    PRInt32 dummyHolder;
    return GetPositionAndSize(&dummyHolder, &dummyHolder, aCX, aCY);
}

NS_IMETHODIMP
nsDocShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
                               PRInt32 cy, bool fRepaint)
{
    mBounds.x = x;
    mBounds.y = y;
    mBounds.width = cx;
    mBounds.height = cy;

    // Hold strong ref, since SetBounds can make us null out mContentViewer
    nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
    if (viewer) {
        //XXX Border figured in here or is that handled elsewhere?
        NS_ENSURE_SUCCESS(viewer->SetBounds(mBounds), NS_ERROR_FAILURE);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(PRInt32 * x, PRInt32 * y, PRInt32 * cx,
                               PRInt32 * cy)
{
    // We should really consider just getting this information from
    // our window instead of duplicating the storage and code...
    if (cx || cy) {
        // Caller wants to know our size; make sure to give them up to
        // date information.
        nsCOMPtr<nsIDocument> doc(do_GetInterface(GetAsSupports(mParent)));
        if (doc) {
            doc->FlushPendingNotifications(Flush_Layout);
        }
    }
    
    DoGetPositionAndSize(x, y, cx, cy);
    return NS_OK;
}

void
nsDocShell::DoGetPositionAndSize(PRInt32 * x, PRInt32 * y, PRInt32 * cx,
                                 PRInt32 * cy)
{    
    if (x)
        *x = mBounds.x;
    if (y)
        *y = mBounds.y;
    if (cx)
        *cx = mBounds.width;
    if (cy)
        *cy = mBounds.height;
}

NS_IMETHODIMP
nsDocShell::Repaint(bool aForce)
{
    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsIViewManager* viewManager = presShell->GetViewManager();
    NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(viewManager->InvalidateAllViews(), NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentWidget(nsIWidget ** parentWidget)
{
    NS_ENSURE_ARG_POINTER(parentWidget);

    *parentWidget = mParentWidget;
    NS_IF_ADDREF(*parentWidget);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentWidget(nsIWidget * aParentWidget)
{
    mParentWidget = aParentWidget;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentNativeWindow(nativeWindow * parentNativeWindow)
{
    NS_ENSURE_ARG_POINTER(parentNativeWindow);

    if (mParentWidget)
        *parentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
    else
        *parentNativeWindow = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetVisibility(bool * aVisibility)
{
    NS_ENSURE_ARG_POINTER(aVisibility);

    *aVisibility = false;

    if (!mContentViewer)
        return NS_OK;

    nsCOMPtr<nsIPresShell> presShell;
    GetPresShell(getter_AddRefs(presShell));
    if (!presShell)
        return NS_OK;

    // get the view manager
    nsIViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    // get the root view
    nsIView *view = vm->GetRootView(); // views are not ref counted
    NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

    // if our root view is hidden, we are not visible
    if (view->GetVisibility() == nsViewVisibility_kHide)
        return NS_OK;

    // otherwise, we must walk up the document and view trees checking
    // for a hidden view, unless we're an off screen browser, which 
    // would make this test meaningless.

    nsCOMPtr<nsIDocShellTreeItem> treeItem = this;
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    treeItem->GetParent(getter_AddRefs(parentItem));
    while (parentItem) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(treeItem));
        docShell->GetPresShell(getter_AddRefs(presShell));

        nsCOMPtr<nsIDocShell> parentDS = do_QueryInterface(parentItem);
        nsCOMPtr<nsIPresShell> pPresShell;
        parentDS->GetPresShell(getter_AddRefs(pPresShell));

        // Null-check for crash in bug 267804
        if (!pPresShell) {
            NS_NOTREACHED("parent docshell has null pres shell");
            return NS_OK;
        }

        nsIContent *shellContent =
            pPresShell->GetDocument()->FindContentForSubDocument(presShell->GetDocument());
        NS_ASSERTION(shellContent, "subshell not in the map");

        nsIFrame* frame = shellContent ? shellContent->GetPrimaryFrame() : nsnull;
        bool isDocShellOffScreen = false;
        docShell->GetIsOffScreenBrowser(&isDocShellOffScreen);
        if (frame &&
            !frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) &&
            !isDocShellOffScreen) {
            return NS_OK;
        }

        treeItem = parentItem;
        treeItem->GetParent(getter_AddRefs(parentItem));
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
nsDocShell::GetIsOffScreenBrowser(bool *aIsOffScreen) 
{
    *aIsOffScreen = mIsOffScreenBrowser;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsActive(bool aIsActive)
{
  // We disallow setting active on chrome docshells.
  if (mItemType == nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_INVALID_ARG;

  // Keep track ourselves.
  mIsActive = aIsActive;

  // Tell the PresShell about it.
  nsCOMPtr<nsIPresShell> pshell;
  GetPresShell(getter_AddRefs(pshell));
  if (pshell)
    pshell->SetIsActive(aIsActive);

  // Tell the window about it
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mScriptGlobal);
  if (win) {
      win->SetIsBackground(!aIsActive);
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(win->GetExtantDocument());
      if (doc) {
          doc->PostVisibilityUpdateEvent();
      }
  }

  // Recursively tell all of our children
  PRInt32 n = mChildList.Count();
  for (PRInt32 i = 0; i < n; ++i) {
      nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(ChildAt(i));
      if (docshell)
        docshell->SetIsActive(aIsActive);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsActive(bool *aIsActive)
{
  *aIsActive = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsAppTab(bool aIsAppTab)
{
  mIsAppTab = aIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsAppTab(bool *aIsAppTab)
{
  *aIsAppTab = mIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetVisibility(bool aVisibility)
{
    if (!mContentViewer)
        return NS_OK;
    if (aVisibility) {
        mContentViewer->Show();
    }
    else {
        mContentViewer->Hide();
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEnabled(bool *aEnabled)
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
nsDocShell::GetMainWidget(nsIWidget ** aMainWidget)
{
    // We don't create our own widget, so simply return the parent one. 
    return GetParentWidget(aMainWidget);
}

NS_IMETHODIMP
nsDocShell::GetTitle(PRUnichar ** aTitle)
{
    NS_ENSURE_ARG_POINTER(aTitle);

    *aTitle = ToNewUnicode(mTitle);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTitle(const PRUnichar * aTitle)
{
    // Store local title
    mTitle = aTitle;

    nsCOMPtr<nsIDocShellTreeItem> parent;
    GetSameTypeParent(getter_AddRefs(parent));

    // When title is set on the top object it should then be passed to the 
    // tree owner.
    if (!parent) {
        nsCOMPtr<nsIBaseWindow>
            treeOwnerAsWin(do_QueryInterface(mTreeOwner));
        if (treeOwnerAsWin)
            treeOwnerAsWin->SetTitle(aTitle);
    }

    if (mCurrentURI && mLoadType != LOAD_ERROR_PAGE && mUseGlobalHistory) {
        nsCOMPtr<IHistory> history = services::GetHistoryService();
        if (history) {
            history->SetURITitle(mCurrentURI, mTitle);
        }
        else if (mGlobalHistory) {
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

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::GetCurScrollPos(PRInt32 scrollOrientation, PRInt32 * curPos)
{
    NS_ENSURE_ARG_POINTER(curPos);

    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    nsPoint pt = sf->GetScrollPosition();

    switch (scrollOrientation) {
    case ScrollOrientation_X:
        *curPos = pt.x;
        return NS_OK;

    case ScrollOrientation_Y:
        *curPos = pt.y;
        return NS_OK;

    default:
        NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
    }
}

NS_IMETHODIMP
nsDocShell::SetCurScrollPos(PRInt32 scrollOrientation, PRInt32 curPos)
{
    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    nsPoint pt = sf->GetScrollPosition();

    switch (scrollOrientation) {
    case ScrollOrientation_X:
        pt.x = curPos;
        break;

    case ScrollOrientation_Y:
        pt.y = curPos;
        break;

    default:
        NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
    }

    sf->ScrollTo(pt, nsIScrollableFrame::INSTANT);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurScrollPosEx(PRInt32 curHorizontalPos, PRInt32 curVerticalPos)
{
    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    sf->ScrollTo(nsPoint(curHorizontalPos, curVerticalPos),
                 nsIScrollableFrame::INSTANT);
    return NS_OK;
}

// XXX This is wrong
NS_IMETHODIMP
nsDocShell::GetScrollRange(PRInt32 scrollOrientation,
                           PRInt32 * minPos, PRInt32 * maxPos)
{
    NS_ENSURE_ARG_POINTER(minPos && maxPos);

    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    nsSize portSize = sf->GetScrollPortRect().Size();
    nsRect range = sf->GetScrollRange();

    switch (scrollOrientation) {
    case ScrollOrientation_X:
        *minPos = range.x;
        *maxPos = range.XMost() + portSize.width;
        return NS_OK;

    case ScrollOrientation_Y:
        *minPos = range.y;
        *maxPos = range.YMost() + portSize.height;
        return NS_OK;

    default:
        NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
    }
}

NS_IMETHODIMP
nsDocShell::SetScrollRange(PRInt32 scrollOrientation,
                           PRInt32 minPos, PRInt32 maxPos)
{
    //XXX First Check
    /*
       Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
       something less than the current thumb position, curPos is set = to maxPos.

       @return NS_OK - Setting or Getting completed successfully.
       NS_ERROR_INVALID_ARG - returned when curPos is not within the
       minPos and maxPos.
     */
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::SetScrollRangeEx(PRInt32 minHorizontalPos,
                             PRInt32 maxHorizontalPos, PRInt32 minVerticalPos,
                             PRInt32 maxVerticalPos)
{
    //XXX First Check
    /*
       Retrieves or Sets the valid ranges for the thumb.  When maxPos is set to 
       something less than the current thumb position, curPos is set = to maxPos.

       @return NS_OK - Setting or Getting completed successfully.
       NS_ERROR_INVALID_ARG - returned when curPos is not within the
       minPos and maxPos.
     */
    return NS_ERROR_FAILURE;
}

// This returns setting for all documents in this docshell
NS_IMETHODIMP
nsDocShell::GetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 * scrollbarPref)
{
    NS_ENSURE_ARG_POINTER(scrollbarPref);
    switch (scrollOrientation) {
    case ScrollOrientation_X:
        *scrollbarPref = mDefaultScrollbarPref.x;
        return NS_OK;

    case ScrollOrientation_Y:
        *scrollbarPref = mDefaultScrollbarPref.y;
        return NS_OK;

    default:
        NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
    }
    return NS_ERROR_FAILURE;
}

// Set scrolling preference for all documents in this shell
//
// There are three possible values stored in the shell:
//  1) nsIScrollable::Scrollbar_Never = no scrollbar
//  2) nsIScrollable::Scrollbar_Auto = scrollbar appears if the document
//     being displayed would normally have scrollbar
//  3) nsIScrollable::Scrollbar_Always = scrollbar always appears
//
// One important client is nsHTMLFrameInnerFrame::CreateWebShell()
NS_IMETHODIMP
nsDocShell::SetDefaultScrollbarPreferences(PRInt32 scrollOrientation,
                                           PRInt32 scrollbarPref)
{
    switch (scrollOrientation) {
    case ScrollOrientation_X:
        mDefaultScrollbarPref.x = scrollbarPref;
        return NS_OK;

    case ScrollOrientation_Y:
        mDefaultScrollbarPref.y = scrollbarPref;
        return NS_OK;

    default:
        NS_ENSURE_TRUE(false, NS_ERROR_INVALID_ARG);
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetScrollbarVisibility(bool * verticalVisible,
                                   bool * horizontalVisible)
{
    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    PRUint32 scrollbarVisibility = sf->GetScrollbarVisibility();
    if (verticalVisible)
        *verticalVisible = (scrollbarVisibility & nsIScrollableFrame::VERTICAL) != 0;
    if (horizontalVisible)
        *horizontalVisible = (scrollbarVisibility & nsIScrollableFrame::HORIZONTAL) != 0;

    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsITextScroll
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::ScrollByLines(PRInt32 numLines)
{
    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    sf->ScrollBy(nsIntPoint(0, numLines), nsIScrollableFrame::LINES,
                 nsIScrollableFrame::SMOOTH);
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ScrollByPages(PRInt32 numPages)
{
    nsIScrollableFrame* sf = GetRootScrollFrame();
    NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

    sf->ScrollBy(nsIntPoint(0, numPages), nsIScrollableFrame::PAGES,
                 nsIScrollableFrame::SMOOTH);
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScriptGlobalObjectOwner
//*****************************************************************************   

nsIScriptGlobalObject*
nsDocShell::GetScriptGlobalObject()
{
    NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), nsnull);

    return mScriptGlobal;
}

//*****************************************************************************
// nsDocShell::nsIRefreshURI
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI * aURI, PRInt32 aDelay, bool aRepeat,
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
    if (!allowRedirects)
        return NS_OK;

    // If any web progress listeners are listening for NOTIFY_REFRESH events,
    // give them a chance to block this refresh.
    bool sameURI;
    nsresult rv = aURI->Equals(mCurrentURI, &sameURI);
    if (NS_FAILED(rv))
        sameURI = false;
    if (!RefreshAttempted(this, aURI, aDelay, sameURI))
        return NS_OK;

    nsRefreshTimer *refreshTimer = new nsRefreshTimer();
    NS_ENSURE_TRUE(refreshTimer, NS_ERROR_OUT_OF_MEMORY);
    PRUint32 busyFlags = 0;
    GetBusyFlags(&busyFlags);

    nsCOMPtr<nsISupports> dataRef = refreshTimer;    // Get the ref count to 1

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
    }
    else {
        // There is no page loading going on right now.  Create the
        // timer and fire it right away.
        nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
        NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

        mRefreshURIList->AppendElement(timer);      // owning timer ref
        timer->InitWithCallback(refreshTimer, aDelay, nsITimer::TYPE_ONE_SHOT);
    }
    return NS_OK;
}

nsresult
nsDocShell::ForceRefreshURIFromTimer(nsIURI * aURI,
                                     PRInt32 aDelay, 
                                     bool aMetaRefresh,
                                     nsITimer* aTimer)
{
    NS_PRECONDITION(aTimer, "Must have a timer here");

    // Remove aTimer from mRefreshURIList if needed
    if (mRefreshURIList) {
        PRUint32 n = 0;
        mRefreshURIList->Count(&n);

        for (PRUint32 i = 0;  i < n; ++i) {
            nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
            if (timer == aTimer) {
                mRefreshURIList->RemoveElementAt(i);
                break;
            }
        }
    }

    return ForceRefreshURI(aURI, aDelay, aMetaRefresh);
}

NS_IMETHODIMP
nsDocShell::ForceRefreshURI(nsIURI * aURI,
                            PRInt32 aDelay, 
                            bool aMetaRefresh)
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
    }
    else {
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
nsDocShell::SetupRefreshURIFromHeader(nsIURI * aBaseURI,
                                      const nsACString & aHeader)
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
    nsCAutoString uriAttrib;
    PRInt32 seconds = 0;
    bool specifiesSeconds = false;

    nsACString::const_iterator iter, tokenStart, doneIterating;

    aHeader.BeginReading(iter);
    aHeader.EndReading(doneIterating);

    // skip leading whitespace
    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
        ++iter;

    tokenStart = iter;

    // skip leading + and -
    if (iter != doneIterating && (*iter == '-' || *iter == '+'))
        ++iter;

    // parse number
    while (iter != doneIterating && (*iter >= '0' && *iter <= '9')) {
        seconds = seconds * 10 + (*iter - '0');
        specifiesSeconds = true;
        ++iter;
    }

    if (iter != doneIterating) {
        // if we started with a '-', number is negative
        if (*tokenStart == '-')
            seconds = -seconds;

        // skip to next ';' or ','
        nsACString::const_iterator iterAfterDigit = iter;
        while (iter != doneIterating && !(*iter == ';' || *iter == ','))
        {
            if (specifiesSeconds)
            {
                // Non-whitespace characters here mean that the string is
                // malformed but tolerate sites that specify a decimal point,
                // even though meta refresh only works on whole seconds.
                if (iter == iterAfterDigit &&
                    !nsCRT::IsAsciiSpace(*iter) && *iter != '.')
                {
                    // The characters between the seconds and the next
                    // section are just garbage!
                    //   e.g. content="2a0z+,URL=http://www.mozilla.org/"
                    // Just ignore this redirect.
                    return NS_ERROR_FAILURE;
                }
                else if (nsCRT::IsAsciiSpace(*iter))
                {
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
        while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
            ++iter;

        // skip ';' or ','
        if (iter != doneIterating && (*iter == ';' || *iter == ',')) {
            ++iter;
        }

        // skip whitespace
        while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
            ++iter;
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
                while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
                    ++iter;

                if (iter != doneIterating && *iter == '=') {
                    ++iter;

                    // skip whitespace
                    while (iter != doneIterating && nsCRT::IsAsciiSpace(*iter))
                        ++iter;

                    // found real start of URI
                    tokenStart = iter;
                }
            }
        }
    }

    // skip a leading '"' or '\''.

    bool isQuotedURI = false;
    if (tokenStart != doneIterating && (*tokenStart == '"' || *tokenStart == '\''))
    {
        isQuotedURI = true;
        ++tokenStart;
    }

    // set iter to start of URI
    iter = tokenStart;

    // tokenStart here points to the beginning of URI

    // grab the rest of the URI
    while (iter != doneIterating)
    {
        if (isQuotedURI && (*iter == '"' || *iter == '\''))
            break;
        ++iter;
    }

    // move iter one back if the last character is a '"' or '\''
    if (iter != tokenStart && isQuotedURI) {
        --iter;
        if (!(*iter == '"' || *iter == '\''))
            ++iter;
    }

    // URI is whatever's contained from tokenStart to iter.
    // note: if tokenStart == doneIterating, so is iter.

    nsresult rv = NS_OK;

    nsCOMPtr<nsIURI> uri;
    bool specifiesURI = false;
    if (tokenStart == iter) {
        uri = aBaseURI;
    }
    else {
        uriAttrib = Substring(tokenStart, iter);
        // NS_NewURI takes care of any whitespace surrounding the URL
        rv = NS_NewURI(getter_AddRefs(uri), uriAttrib, nsnull, aBaseURI);
        specifiesURI = true;
    }

    // No URI or seconds were specified
    if (!specifiesSeconds && !specifiesURI)
    {
        // Do nothing because the alternative is to spin around in a refresh
        // loop forever!
        return NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIScriptSecurityManager>
            securityManager(do_GetService
                            (NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
            rv = securityManager->
                CheckLoadURI(aBaseURI, uri,
                             nsIScriptSecurityManager::
                             LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT);

            if (NS_SUCCEEDED(rv)) {
                bool isjs = true;
                rv = NS_URIChainHasFlags(uri,
                  nsIProtocolHandler::URI_OPENING_EXECUTES_SCRIPT, &isjs);
                NS_ENSURE_SUCCESS(rv, rv);

                if (isjs) {
                    return NS_ERROR_FAILURE;
                }
            }

            if (NS_SUCCEEDED(rv)) {
                // Since we can't travel back in time yet, just pretend
                // negative numbers do nothing at all.
                if (seconds < 0)
                    return NS_ERROR_FAILURE;

                rv = RefreshURI(uri, seconds * 1000, false, true);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP nsDocShell::SetupRefreshURI(nsIChannel * aChannel)
{
    nsresult rv;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsCAutoString refreshHeader;
        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("refresh"),
                                            refreshHeader);

        if (!refreshHeader.IsEmpty()) {
            SetupReferrerFromChannel(aChannel);
            rv = SetupRefreshURIFromHeader(mCurrentURI, refreshHeader);
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
    if (!aTimerList)
        return;

    PRUint32 n=0;
    aTimerList->Count(&n);

    while (n) {
        nsCOMPtr<nsITimer> timer(do_QueryElementAt(aTimerList, --n));

        aTimerList->RemoveElementAt(n);    // bye bye owning timer ref

        if (timer)
            timer->Cancel();        
    }
}

NS_IMETHODIMP
nsDocShell::CancelRefreshURITimers()
{
    DoCancelRefreshURITimers(mRefreshURIList);
    DoCancelRefreshURITimers(mSavedRefreshURIList);
    mRefreshURIList = nsnull;
    mSavedRefreshURIList = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRefreshPending(bool* _retval)
{
    if (!mRefreshURIList) {
        *_retval = false;
        return NS_OK;
    }

    PRUint32 count;
    nsresult rv = mRefreshURIList->Count(&count);
    if (NS_SUCCEEDED(rv))
        *_retval = (count != 0);
    return rv;
}

NS_IMETHODIMP
nsDocShell::SuspendRefreshURIs()
{
    if (mRefreshURIList) {
        PRUint32 n = 0;
        mRefreshURIList->Count(&n);

        for (PRUint32 i = 0;  i < n; ++i) {
            nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
            if (!timer)
                continue;  // this must be a nsRefreshURI already

            // Replace this timer object with a nsRefreshTimer object.
            nsCOMPtr<nsITimerCallback> callback;
            timer->GetCallback(getter_AddRefs(callback));

            timer->Cancel();

            nsCOMPtr<nsITimerCallback> rt = do_QueryInterface(callback);
            NS_ASSERTION(rt, "RefreshURIList timer callbacks should only be RefreshTimer objects");

            mRefreshURIList->ReplaceElementAt(rt, i);
        }
    }

    // Suspend refresh URIs for our child shells as well.
    PRInt32 n = mChildList.Count();

    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell)
            shell->SuspendRefreshURIs();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ResumeRefreshURIs()
{
    RefreshURIFromQueue();

    // Resume refresh URIs for our child shells as well.
    PRInt32 n = mChildList.Count();

    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> shell = do_QueryInterface(ChildAt(i));
        if (shell)
            shell->ResumeRefreshURIs();
    }

    return NS_OK;
}

nsresult
nsDocShell::RefreshURIFromQueue()
{
    if (!mRefreshURIList)
        return NS_OK;
    PRUint32 n = 0;
    mRefreshURIList->Count(&n);

    while (n) {
        nsCOMPtr<nsISupports> element;
        mRefreshURIList->GetElementAt(--n, getter_AddRefs(element));
        nsCOMPtr<nsITimerCallback> refreshInfo(do_QueryInterface(element));

        if (refreshInfo) {   
            // This is the nsRefreshTimer object, waiting to be
            // setup in a timer object and fired.                         
            // Create the timer and  trigger it.
            PRUint32 delay = static_cast<nsRefreshTimer*>(static_cast<nsITimerCallback*>(refreshInfo))->GetDelay();
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
    }  // while
 
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIContentViewerContainer
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::Embed(nsIContentViewer * aContentViewer,
                  const char *aCommand, nsISupports * aExtraInfo)
{
    // Save the LayoutHistoryState of the previous document, before
    // setting up new document
    PersistLayoutHistoryState();

    nsresult rv = SetupNewViewer(aContentViewer);

    // If we are loading a wyciwyg url from history, change the base URI for 
    // the document to the original http url that created the document.write().
    // This makes sure that all relative urls in a document.written page loaded
    // via history work properly.
    if (mCurrentURI &&
       (mLoadType & LOAD_CMD_HISTORY ||
        mLoadType == LOAD_RELOAD_NORMAL ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE)){
        bool isWyciwyg = false;
        // Check if the url is wyciwyg
        rv = mCurrentURI->SchemeIs("wyciwyg", &isWyciwyg);      
        if (isWyciwyg && NS_SUCCEEDED(rv))
            SetBaseUrlForWyciwyg(aContentViewer);
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

    if (!updateHistory)
        SetLayoutHistoryState(nsnull);

    return NS_OK;
}

/* void setIsPrinting (in boolean aIsPrinting); */
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
nsDocShell::OnProgressChange(nsIWebProgress * aProgress,
                             nsIRequest * aRequest,
                             PRInt32 aCurSelfProgress,
                             PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress,
                             PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStateChange(nsIWebProgress * aProgress, nsIRequest * aRequest,
                          PRUint32 aStateFlags, nsresult aStatus)
{
    nsresult rv;

    if ((~aStateFlags & (STATE_START | STATE_IS_NETWORK)) == 0) {
        // Save timing statistics.
        nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        nsCAutoString aURI;
        uri->GetAsciiSpec(aURI);
        if (this == aProgress){
            rv = MaybeInitTiming();
            if (mTiming) {
                mTiming->NotifyFetchStart(uri, ConvertLoadTypeToNavigationType(mLoadType));
            } 
        }

        nsCOMPtr<nsIWyciwygChannel>  wcwgChannel(do_QueryInterface(aRequest));
        nsCOMPtr<nsIWebProgress> webProgress =
            do_QueryInterface(GetAsSupports(this));

        // Was the wyciwyg document loaded on this docshell?
        if (wcwgChannel && !mLSHE && (mItemType == typeContent) && aProgress == webProgress.get()) {
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
                    nsCOMPtr<nsIDocShellHistory> parent =
                        do_QueryInterface(parentAsItem);
                    if (parent) {
                        bool oshe = false;
                        nsCOMPtr<nsISHEntry> entry;
                        parent->GetCurrentSHEntry(getter_AddRefs(entry), &oshe);
                        static_cast<nsDocShell*>(parent.get())->
                            ClearFrameHistory(entry);
                    }
                }

                // This is a document.write(). Get the made-up url
                // from the channel and store it in session history.
                // Pass false for aCloneChildren, since we're creating
                // a new DOM here.
                rv = AddToSessionHistory(uri, wcwgChannel, nsnull, false,
                                         getter_AddRefs(mLSHE));
                SetCurrentURI(uri, aRequest, true, 0);
                // Save history state of the previous page
                rv = PersistLayoutHistoryState();
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
    }
    else if ((~aStateFlags & (STATE_TRANSFERRING | STATE_IS_DOCUMENT)) == 0) {
        // Page is loading
        mBusyFlags = BUSY_FLAGS_BUSY | BUSY_FLAGS_PAGE_LOADING;
    }
    else if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_NETWORK)) {
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
nsDocShell::OnLocationChange(nsIWebProgress * aProgress, nsIRequest * aRequest,
                             nsIURI * aURI, PRUint32 aFlags)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

void
nsDocShell::OnRedirectStateChange(nsIChannel* aOldChannel,
                                  nsIChannel* aNewChannel,
                                  PRUint32 aRedirectFlags,
                                  PRUint32 aStateFlags)
{
    NS_ASSERTION(aStateFlags & STATE_REDIRECTING,
                 "Calling OnRedirectStateChange when there is no redirect");
    if (!(aStateFlags & STATE_IS_DOCUMENT))
        return; // not a toplevel document

    nsCOMPtr<nsIURI> oldURI, newURI;
    aOldChannel->GetURI(getter_AddRefs(oldURI));
    aNewChannel->GetURI(getter_AddRefs(newURI));
    if (!oldURI || !newURI) {
        return;
    }
    // On session restore we get a redirect from page to itself. Don't count it.
    bool equals = false;
    if (mTiming &&
        !(mLoadType == LOAD_HISTORY &&
          NS_SUCCEEDED(newURI->Equals(oldURI, &equals)) && equals)) {
        mTiming->NotifyRedirect(oldURI, newURI);
    }

    // Below a URI visit is saved (see AddURIVisit method doc).
    // The visit chain looks something like:
    //   ...
    //   Site N - 1
    //                =>  Site N
    //   (redirect to =>) Site N + 1 (we are here!)

    // Get N - 1 and transition type
    nsCOMPtr<nsIURI> previousURI;
    PRUint32 previousFlags = 0;
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
    }
    else {
        nsCOMPtr<nsIURI> referrer;
        // Treat referrer as null if there is an error getting it.
        (void)NS_GetReferrerFromChannel(aOldChannel,
                                        getter_AddRefs(referrer));

        // Get the HTTP response code, if available.
        PRUint32 responseStatus = 0;
        nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aOldChannel);
        if (httpChannel) {
            (void)httpChannel->GetResponseStatus(&responseStatus);
        }

        // Add visit N -1 => N
        AddURIVisit(oldURI, referrer, previousURI, previousFlags,
                    responseStatus);

        // Since N + 1 could be the final destination, we will not save N => N + 1
        // here.  OnNewURI will do that, so we will cache it.
        SaveLastVisit(aNewChannel, oldURI, aRedirectFlags);
    }

    // check if the new load should go through the application cache.
    nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
        do_QueryInterface(aNewChannel);
    if (appCacheChannel) {
        // Permission will be checked in the parent process.
        if (GeckoProcessType_Default != XRE_GetProcessType())
            appCacheChannel->SetChooseApplicationCache(true);
        else
            appCacheChannel->SetChooseApplicationCache(ShouldCheckAppCache(newURI));
    }

    if (!(aRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL) && 
        mLoadType & (LOAD_CMD_RELOAD | LOAD_CMD_HISTORY)) {
        mLoadType = LOAD_NORMAL_REPLACE;
        SetHistoryEntry(&mLSHE, nsnull);
    }
}

NS_IMETHODIMP
nsDocShell::OnStatusChange(nsIWebProgress * aWebProgress,
                           nsIRequest * aRequest,
                           nsresult aStatus, const PRUnichar * aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnSecurityChange(nsIWebProgress * aWebProgress,
                             nsIRequest * aRequest, PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}


nsresult
nsDocShell::EndPageLoad(nsIWebProgress * aProgress,
                        nsIChannel * aChannel, nsresult aStatus)
{
    if(!aChannel)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIURI> url;
    nsresult rv = aChannel->GetURI(getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsITimedChannel> timingChannel =
        do_QueryInterface(aChannel);
    if (timingChannel) {
        TimeStamp channelCreationTime;
        rv = timingChannel->GetChannelCreation(&channelCreationTime);
        if (NS_SUCCEEDED(rv) && !channelCreationTime.IsNull())
            Telemetry::AccumulateTimeDelta(
                Telemetry::TOTAL_CONTENT_PAGE_LOAD_TIME,
                channelCreationTime);
    }

    // Timing is picked up by the window, we don't need it anymore
    mTiming = nsnull;

    // clean up reload state for meta charset
    if (eCharsetReloadRequested == mCharsetReloadState)
        mCharsetReloadState = eCharsetReloadStopOrigional;
    else 
        mCharsetReloadState = eCharsetReloadInit;

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
          FavorPerformanceHint(false, NS_EVENT_STARVATION_DELAY_HINT);
        }
    }
    /* Check if the httpChannel has any cache-control related response headers,
     * like no-store, no-cache. If so, update SHEntry so that 
     * when a user goes back/forward to this page, we appropriately do 
     * form value restoration or load from server.
     */
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (!httpChannel) // HttpChannel could be hiding underneath a Multipart channel.    
        GetHttpChannel(aChannel, getter_AddRefs(httpChannel));

    if (httpChannel) {
        // figure out if SH should be saving layout state.
        bool discardLayoutState = ShouldDiscardLayoutState(httpChannel);       
        if (mLSHE && discardLayoutState && (mLoadType & LOAD_CMD_NORMAL) &&
            (mLoadType != LOAD_BYPASS_HISTORY) && (mLoadType != LOAD_ERROR_PAGE))
            mLSHE->SetSaveLayoutStateFlag(false);            
    }

    // Clear mLSHE after calling the onLoadHandlers. This way, if the
    // onLoadHandler tries to load something different in
    // itself or one of its children, we can deal with it appropriately.
    if (mLSHE) {
        mLSHE->SetLoadType(nsIDocShellLoadInfo::loadHistory);

        // Clear the mLSHE reference to indicate document loading is done one
        // way or another.
        SetHistoryEntry(&mLSHE, nsnull);
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
            DisplayLoadError(aStatus, url, nsnull, aChannel);
            return NS_OK;
        }

        if (sURIFixup) {
            //
            // Try and make an alternative URI from the old one
            //
            nsCOMPtr<nsIURI> newURI;

            nsCAutoString oldSpec;
            url->GetSpec(oldSpec);
      
            //
            // First try keyword fixup
            //
            if (aStatus == NS_ERROR_UNKNOWN_HOST && mAllowKeywordFixup) {
                bool keywordsEnabled =
                    Preferences::GetBool("keyword.enabled", false);

                nsCAutoString host;
                url->GetHost(host);

                nsCAutoString scheme;
                url->GetScheme(scheme);

                PRInt32 dotLoc = host.FindChar('.');

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
                    // only send non-qualified hosts to the keyword server
                    if (!mOriginalUriString.IsEmpty()) {
                        sURIFixup->KeywordToURI(mOriginalUriString,
                                                getter_AddRefs(newURI));
                    }
                    else {
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
                        nsCAutoString utf8Host;
                        nsCOMPtr<nsIIDNService> idnSrv =
                            do_GetService(NS_IDNSERVICE_CONTRACTID);
                        if (idnSrv &&
                            NS_SUCCEEDED(idnSrv->IsACE(host, &isACE)) && isACE &&
                            NS_SUCCEEDED(idnSrv->ConvertACEtoUTF8(host, utf8Host)))
                            sURIFixup->KeywordToURI(utf8Host,
                                                    getter_AddRefs(newURI));
                        else
                            sURIFixup->KeywordToURI(host, getter_AddRefs(newURI));
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
                }
                else {
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
                    newURI = nsnull;
                    sURIFixup->CreateFixupURI(oldSpec,
                      nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
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
                    nsCAutoString newSpec;
                    newURI->GetSpec(newSpec);
                    NS_ConvertUTF8toUTF16 newSpecW(newSpec);

                    return LoadURI(newSpecW.get(),  // URI string
                                   LOAD_FLAGS_NONE, // Load flags
                                   nsnull,          // Referring URI
                                   nsnull,          // Post data stream
                                   nsnull);         // Headers stream
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
            (isTopFrame || mUseErrorPages)) {
            DisplayLoadError(aStatus, url, nsnull, aChannel);
        }
        // Errors to be shown for any frame
        else if (aStatus == NS_ERROR_NET_TIMEOUT ||
                 aStatus == NS_ERROR_REDIRECT_LOOP ||
                 aStatus == NS_ERROR_UNKNOWN_SOCKET_TYPE ||
                 aStatus == NS_ERROR_NET_INTERRUPT ||
                 aStatus == NS_ERROR_NET_RESET ||
                 aStatus == NS_ERROR_OFFLINE ||
                 aStatus == NS_ERROR_MALWARE_URI ||
                 aStatus == NS_ERROR_PHISHING_URI ||
                 aStatus == NS_ERROR_UNSAFE_CONTENT_TYPE ||
                 aStatus == NS_ERROR_REMOTE_XUL ||
                 aStatus == NS_ERROR_OFFLINE ||
                 NS_ERROR_GET_MODULE(aStatus) == NS_ERROR_MODULE_SECURITY) {
            DisplayLoadError(aStatus, url, nsnull, aChannel);
        }
        else if (aStatus == NS_ERROR_DOCUMENT_NOT_CACHED) {
            // Non-caching channels will simply return NS_ERROR_OFFLINE.
            // Caching channels would have to look at their flags to work
            // out which error to return. Or we can fix up the error here.
            if (!(mLoadType & LOAD_CMD_HISTORY))
                aStatus = NS_ERROR_OFFLINE;
            DisplayLoadError(aStatus, url, nsnull, aChannel);
        }
  } // if we have a host

  return NS_OK;
}


//*****************************************************************************
// nsDocShell: Content Viewer Management
//*****************************************************************************   

NS_IMETHODIMP
nsDocShell::EnsureContentViewer()
{
    if (mContentViewer)
        return NS_OK;
    if (mIsBeingDestroyed)
        return NS_ERROR_FAILURE;

    NS_TIME_FUNCTION;

    nsIPrincipal* principal = nsnull;
    nsCOMPtr<nsIURI> baseURI;

    nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(mScriptGlobal));
    if (piDOMWindow) {
        principal = piDOMWindow->GetOpenerScriptPrincipal();
    }

    if (!principal) {
        principal = GetInheritedPrincipal(false);
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        GetSameTypeParent(getter_AddRefs(parentItem));
        if (parentItem) {
            nsCOMPtr<nsPIDOMWindow> domWin = do_GetInterface(GetAsSupports(this));
            if (domWin) {
                nsCOMPtr<nsIContent> parentContent =
                    do_QueryInterface(domWin->GetFrameElementInternal());
                if (parentContent) {
                    baseURI = parentContent->GetBaseURI();
                }
            }
        }
    }

    nsresult rv = CreateAboutBlankContentViewer(principal, baseURI);

    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDocument> doc(do_GetInterface(GetAsSupports(this)));
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
  NS_ASSERTION(!mCreatingDocument, "infinite(?) loop creating document averted");
  if (mCreatingDocument)
    return NS_ERROR_FAILURE;

  mCreatingDocument = true;

  // mContentViewer->PermitUnload may release |this| docshell.
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);
  
  if (mContentViewer) {
    // We've got a content viewer already. Make sure the user
    // permits us to discard the current document and replace it
    // with about:blank. And also ensure we fire the unload events
    // in the current document.

    // Make sure timing is created. Unload gets fired first for
    // document loaded from the session history.
    rv = MaybeInitTiming();
    if (mTiming) {
      mTiming->NotifyBeforeUnload();
    }

    bool okToUnload;
    rv = mContentViewer->PermitUnload(false, &okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the load.
      return NS_ERROR_FAILURE;
    }

    mSavingOldViewer = aTryToSaveOldPresentation && 
                       CanSavePresentation(LOAD_NORMAL, nsnull, nsnull);

    if (mTiming) {
      mTiming->NotifyUnloadAccepted(mCurrentURI);
    }

    // Make sure to blow away our mLoadingURI just in case.  No loads
    // from inside this pagehide.
    mLoadingURI = nsnull;
    
    // Stop any in-progress loading, so that we don't accidentally trigger any
    // PageShow notifications from Embed() interrupting our loading below.
    Stop();

    // Notify the current document that it is about to be unloaded!!
    //
    // It is important to fire the unload() notification *before* any state
    // is changed within the DocShell - otherwise, javascript will get the
    // wrong information :-(
    //
    (void) FirePageHideNotification(!mSavingOldViewer);
  }

  // Now make sure we don't think we're in the middle of firing unload after
  // this point.  This will make us fire unload when the about:blank document
  // unloads... but that's ok, more or less.  Would be nice if it fired load
  // too, of course.
  mFiredUnloadEvent = false;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory =
      nsContentUtils::FindInternalContentViewer("text/html");

  if (docFactory) {
    // generate (about:blank) document to load
    docFactory->CreateBlankDocument(mLoadGroup, aPrincipal,
                                    getter_AddRefs(blankDoc));
    if (blankDoc) {
      // Hack: set the base URI manually, since this document never
      // got Reset() with a channel.
      blankDoc->SetBaseURI(aBaseURI);

      blankDoc->SetContainer(static_cast<nsIDocShell *>(this));

      // create a content viewer for us and the new document
      docFactory->CreateInstanceForDocument(NS_ISUPPORTS_CAST(nsIDocShell *, this),
                    blankDoc, "view", getter_AddRefs(viewer));

      // hook 'em up
      if (viewer) {
        viewer->SetContainer(static_cast<nsIContentViewerContainer *>(this));
        Embed(viewer, "", 0);

        SetCurrentURI(blankDoc->GetDocumentURI(), nsnull, true, 0);
        rv = mIsBeingDestroyed ? NS_ERROR_NOT_AVAILABLE : NS_OK;
      }
    }
  }
  mCreatingDocument = false;

  // The transient about:blank viewer doesn't have a session history entry.
  SetHistoryEntry(&mOSHE, nsnull);

  return rv;
}

NS_IMETHODIMP
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal *aPrincipal)
{
    return CreateAboutBlankContentViewer(aPrincipal, nsnull);
}

bool
nsDocShell::CanSavePresentation(PRUint32 aLoadType,
                                nsIRequest *aNewRequest,
                                nsIDocument *aNewDocument)
{
    if (!mOSHE)
        return false; // no entry to save into

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
        aLoadType != LOAD_ERROR_PAGE)
        return false;

    // If the session history entry has the saveLayoutState flag set to false,
    // then we should not cache the presentation.
    bool canSaveState;
    mOSHE->GetSaveLayoutStateFlag(&canSaveState);
    if (!canSaveState)
        return false;

    // If the document is not done loading, don't cache it.
    nsCOMPtr<nsPIDOMWindow> pWin = do_QueryInterface(mScriptGlobal);
    if (!pWin || pWin->IsLoading())
        return false;

    if (pWin->WouldReuseInnerWindow(aNewDocument))
        return false;

    // Avoid doing the work of saving the presentation state in the case where
    // the content viewer cache is disabled.
    if (nsSHistory::GetMaxTotalViewers() == 0)
        return false;

    // Don't cache the content viewer if we're in a subframe and the subframe
    // pref is disabled.
    bool cacheFrames =
        Preferences::GetBool("browser.sessionhistory.cache_subframes",
                             false);
    if (!cacheFrames) {
        nsCOMPtr<nsIDocShellTreeItem> root;
        GetSameTypeParent(getter_AddRefs(root));
        if (root && root != this) {
            return false;  // this is a subframe load
        }
    }

    // If the document does not want its presentation cached, then don't.
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(pWin->GetExtantDocument());
    if (!doc || !doc->CanSavePresentation(aNewRequest))
        return false;

    return true;
}

void
nsDocShell::ReattachEditorToWindow(nsISHEntry *aSHEntry)
{
    NS_ASSERTION(!mEditorData,
                 "Why reattach an editor when we already have one?");
    NS_ASSERTION(aSHEntry && aSHEntry->HasDetachedEditor(),
                 "Reattaching when there's not a detached editor.");

    if (mEditorData || !aSHEntry)
        return;

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
        if (mOSHE)
            mOSHE->SetEditorData(mEditorData.forget());
        else
            mEditorData = nsnull;
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

    nsCOMPtr<nsPIDOMWindow> privWin = do_QueryInterface(mScriptGlobal);
    if (!privWin)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> windowState;
    nsresult rv = privWin->SaveWindowState(getter_AddRefs(windowState));
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PAGE_CACHE
    nsCOMPtr<nsIURI> uri;
    mOSHE->GetURI(getter_AddRefs(uri));
    nsCAutoString spec;
    if (uri)
        uri->GetSpec(spec);
    printf("Saving presentation into session history\n");
    printf("  SH URI: %s\n", spec.get());
#endif

    rv = mOSHE->SetWindowState(windowState);
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

    PRInt32 childCount = mChildList.Count();
    for (PRInt32 i = 0; i < childCount; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> childShell = do_QueryInterface(ChildAt(i));
        NS_ASSERTION(childShell, "null child shell");

        mOSHE->AddChildShell(childShell);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RestorePresentationEvent::Run()
{
    if (mDocShell && NS_FAILED(mDocShell->RestoreFromHistory()))
        NS_WARNING("RestoreFromHistory failed");
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::BeginRestore(nsIContentViewer *aContentViewer, bool aTop)
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
        nsIChannel *channel = doc->GetChannel();
        if (channel) {
            mEODForCurrentDocument = false;
            mIsRestoringDocument = true;
            mLoadGroup->AddRequest(channel, nsnull);
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
    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child) {
            nsresult rv = child->BeginRestore(nsnull, false);
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

    PRInt32 n = mChildList.Count();
    for (PRInt32 i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child) {
            child->FinishRestore();
        }
    }

    if (mOSHE && mOSHE->HasDetachedEditor()) {
      ReattachEditorToWindow(mOSHE);
    }

    nsCOMPtr<nsIDocument> doc = do_GetInterface(GetAsSupports(this));
    if (doc) {
        // Finally, we remove the request from the loadgroup.  This will
        // cause onStateChange(STATE_STOP) to fire, which will fire the
        // pageshow event to the chrome.

        nsIChannel *channel = doc->GetChannel();
        if (channel) {
            mIsRestoringDocument = true;
            mLoadGroup->RemoveRequest(channel, nsnull, NS_OK);
            mIsRestoringDocument = false;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRestoringDocument(bool *aRestoring)
{
    *aRestoring = mIsRestoringDocument;
    return NS_OK;
}

nsresult
nsDocShell::RestorePresentation(nsISHEntry *aSHEntry, bool *aRestoring)
{
    NS_ASSERTION(mLoadType & LOAD_CMD_HISTORY,
                 "RestorePresentation should only be called for history loads");

    nsCOMPtr<nsIContentViewer> viewer;
    aSHEntry->GetContentViewer(getter_AddRefs(viewer));

#ifdef DEBUG_PAGE_CACHE
    nsCOMPtr<nsIURI> uri;
    aSHEntry->GetURI(getter_AddRefs(uri));

    nsCAutoString spec;
    if (uri)
        uri->GetSpec(spec);
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

    nsCOMPtr<nsISupports> container;
    viewer->GetContainer(getter_AddRefs(container));
    if (!::SameCOMIdentity(container, GetAsSupports(this))) {
#ifdef DEBUG_PAGE_CACHE
        printf("No valid container, clearing presentation\n");
#endif
        aSHEntry->SetContentViewer(nsnull);
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(mContentViewer != viewer, "Restoring existing presentation");

#ifdef DEBUG_PAGE_CACHE
    printf("restoring presentation from session history: %s\n", spec.get());
#endif

    SetHistoryEntry(&mLSHE, aSHEntry);

    // Add the request to our load group.  We do this before swapping out
    // the content viewers so that consumers of STATE_START can access
    // the old document.  We only deal with the toplevel load at this time --
    // to be consistent with normal document loading, subframes cannot start
    // loading until after data arrives, which is after STATE_START completes.

    BeginRestore(viewer, true);

    // Post an event that will remove the request after we've returned
    // to the event loop.  This mimics the way it is called by nsIChannel
    // implementations.

    // Revoke any pending restore (just in case)
    NS_ASSERTION(!mRestorePresentationEvent.IsPending(),
        "should only have one RestorePresentationEvent");
    mRestorePresentationEvent.Revoke();

    nsRefPtr<RestorePresentationEvent> evt = new RestorePresentationEvent(this);
    nsresult rv = NS_DispatchToCurrentThread(evt);
    if (NS_SUCCEEDED(rv)) {
        mRestorePresentationEvent = evt.get();
        // The rest of the restore processing will happen on our event
        // callback.
        *aRestoring = true;
    }

    return rv;
}

nsresult
nsDocShell::RestoreFromHistory()
{
    mRestorePresentationEvent.Forget();

    // This section of code follows the same ordering as CreateContentViewer.
    if (!mLSHE)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIContentViewer> viewer;
    mLSHE->GetContentViewer(getter_AddRefs(viewer));
    if (!viewer)
        return NS_ERROR_FAILURE;

    if (mSavingOldViewer) {
        // We determined that it was safe to cache the document presentation
        // at the time we initiated the new load.  We need to check whether
        // it's still safe to do so, since there may have been DOM mutations
        // or new requests initiated.
        nsCOMPtr<nsIDOMDocument> domDoc;
        viewer->GetDOMDocument(getter_AddRefs(domDoc));
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
        nsIRequest *request = nsnull;
        if (doc)
            request = doc->GetChannel();
        mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
    }

    nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV(
        do_QueryInterface(mContentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> newMUDV(
        do_QueryInterface(viewer));
    PRInt32 minFontSize = 0;
    float textZoom = 1.0f;
    float pageZoom = 1.0f;
    bool styleDisabled = false;
    if (oldMUDV && newMUDV) {
        oldMUDV->GetMinFontSize(&minFontSize);
        oldMUDV->GetTextZoom(&textZoom);
        oldMUDV->GetFullZoom(&pageZoom);
        oldMUDV->GetAuthorStyleDisabled(&styleDisabled);
    }

    // Protect against mLSHE going away via a load triggered from
    // pagehide or unload.
    nsCOMPtr<nsISHEntry> origLSHE = mLSHE;

    // Make sure to blow away our mLoadingURI just in case.  No loads
    // from inside this pagehide.
    mLoadingURI = nsnull;
    
    // Notify the old content viewer that it's being hidden.
    FirePageHideNotification(!mSavingOldViewer);

    // If mLSHE was changed as a result of the pagehide event, then
    // something else was loaded.  Don't finish restoring.
    if (mLSHE != origLSHE)
      return NS_OK;

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

    mSavedRefreshURIList = nsnull;

    // In cases where we use a transient about:blank viewer between loads,
    // we never show the transient viewer, so _its_ previous viewer is never
    // unhooked from the view hierarchy.  Destroy any such previous viewer now,
    // before we grab the root view sibling, so that we don't grab a view
    // that's about to go away.

    if (mContentViewer) {
        nsCOMPtr<nsIContentViewer> previousViewer;
        mContentViewer->GetPreviousViewer(getter_AddRefs(previousViewer));
        if (previousViewer) {
            mContentViewer->SetPreviousViewer(nsnull);
            previousViewer->Destroy();
        }
    }

    // Save off the root view's parent and sibling so that we can insert the
    // new content viewer's root view at the same position.  Also save the
    // bounds of the root view's widget.

    nsIView *rootViewSibling = nsnull, *rootViewParent = nsnull;
    nsIntRect newBounds(0, 0, 0, 0);

    nsCOMPtr<nsIPresShell> oldPresShell;
    nsDocShell::GetPresShell(getter_AddRefs(oldPresShell));
    if (oldPresShell) {
        nsIViewManager *vm = oldPresShell->GetViewManager();
        if (vm) {
            nsIView *oldRootView = vm->GetRootView();

            if (oldRootView) {
                rootViewSibling = oldRootView->GetNextSibling();
                rootViewParent = oldRootView->GetParent();

                mContentViewer->GetBounds(newBounds);
            }
        }
    }

    // Transfer ownership to mContentViewer.  By ensuring that either the
    // docshell or the session history, but not both, have references to the
    // content viewer, we prevent the viewer from being torn down after
    // Destroy() is called.

    if (mContentViewer) {
        mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nsnull);
        viewer->SetPreviousViewer(mContentViewer);
    }
    if (mOSHE && (!mContentViewer || !mSavingOldViewer)) {
        // We don't plan to save a viewer in mOSHE; tell it to drop
        // any other state it's holding.
        mOSHE->SyncPresentationState();
    }

    // Order the mContentViewer setup just like Embed does.
    mContentViewer = nsnull;

    // Now that we're about to switch documents, forget all of our children.
    // Note that we cached them as needed up in CaptureState above.
    DestroyChildren();

    mContentViewer.swap(viewer);

    // Grab all of the related presentation from the SHEntry now.
    // Clearing the viewer from the SHEntry will clear all of this state.
    nsCOMPtr<nsISupports> windowState;
    mLSHE->GetWindowState(getter_AddRefs(windowState));
    mLSHE->SetWindowState(nsnull);

    bool sticky;
    mLSHE->GetSticky(&sticky);

    nsCOMPtr<nsIDOMDocument> domDoc;
    mContentViewer->GetDOMDocument(getter_AddRefs(domDoc));

    nsCOMArray<nsIDocShellTreeItem> childShells;
    PRInt32 i = 0;
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
    rv = mContentViewer->Open(windowState, mLSHE);

    // Hack to keep nsDocShellEditorData alive across the
    // SetContentViewer(nsnull) call below.
    nsAutoPtr<nsDocShellEditorData> data(mLSHE->ForgetEditorData());

    // Now remove it from the cached presentation.
    mLSHE->SetContentViewer(nsnull);
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
    SetLayoutHistoryState(nsnull);

    // This is the end of our Embed() replacement

    mSavingOldViewer = false;
    mEODForCurrentDocument = false;

    // Tell the event loop to favor plevents over user events, see comments
    // in CreateContentViewer.
    if (++gNumberOfDocumentsLoading == 1)
        FavorPerformanceHint(true, NS_EVENT_STARVATION_DELAY_HINT);


    if (oldMUDV && newMUDV) {
        newMUDV->SetMinFontSize(minFontSize);
        newMUDV->SetTextZoom(textZoom);
        newMUDV->SetFullZoom(pageZoom);
        newMUDV->SetAuthorStyleDisabled(styleDisabled);
    }

    nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
    PRUint32 parentSuspendCount = 0;
    if (document) {
        nsCOMPtr<nsIDocShellTreeItem> parent;
        GetParent(getter_AddRefs(parent));
        nsCOMPtr<nsIDocument> d = do_GetInterface(parent);
        if (d) {
            if (d->EventHandlingSuppressed()) {
                document->SuppressEventHandling(d->EventHandlingSuppressed());
            }
            nsCOMPtr<nsPIDOMWindow> parentWindow = d->GetWindow();
            if (parentWindow) {
                parentSuspendCount = parentWindow->TimeoutSuspendCount();
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
    nsCOMPtr<nsPIDOMWindow> privWin =
        do_GetInterface(static_cast<nsIInterfaceRequestor*>(this));
    NS_ASSERTION(privWin, "could not get nsPIDOMWindow interface");

    rv = privWin->RestoreWindowState(windowState);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now, dispatch a title change event which would happen as the
    // <head> is parsed.
    document->NotifyPossibleTitleChange(false);

    // Now we simulate appending child docshells for subframes.
    for (i = 0; i < childShells.Count(); ++i) {
        nsIDocShellTreeItem *childItem = childShells.ObjectAt(i);
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

        bool allowDNSPrefetch;
        childShell->GetAllowDNSPrefetch(&allowDNSPrefetch);

        // this.AddChild(child) calls child.SetDocLoaderParent(this), meaning
        // that the child inherits our state. Among other things, this means
        // that the child inherits our mIsActive and mInPrivateBrowsing, which
        // is what we want.
        AddChild(childItem);

        childShell->SetAllowPlugins(allowPlugins);
        childShell->SetAllowJavascript(allowJavascript);
        childShell->SetAllowMetaRedirects(allowRedirects);
        childShell->SetAllowSubframes(allowSubframes);
        childShell->SetAllowImages(allowImages);
        childShell->SetAllowDNSPrefetch(allowDNSPrefetch);

        rv = childShell->BeginRestore(nsnull, false);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresShell> shell;
    nsDocShell::GetPresShell(getter_AddRefs(shell));

    nsIViewManager *newVM = shell ? shell->GetViewManager() : nsnull;
    nsIView *newRootView = newVM ? newVM->GetRootView() : nsnull;

    // Insert the new root view at the correct location in the view tree.
    if (rootViewParent) {
        nsIViewManager *parentVM = rootViewParent->GetViewManager();

        if (parentVM && newRootView) {
            // InsertChild(parent, child, sib, true) inserts the child after
            // sib in content order, which is before sib in view order. BUT
            // when sib is null it inserts at the end of the the document
            // order, i.e., first in view order.  But when oldRootSibling is
            // null, the old root as at the end of the view list --- last in
            // content order --- and we want to call InsertChild(parent, child,
            // nsnull, false) in that case.
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
    PRInt32 n = mChildList.Count();
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShell> child = do_QueryInterface(ChildAt(i));
        if (child)
            child->ResumeRefreshURIs();
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
            nsIScrollableFrame *rootScrollFrame =
              shell->GetRootScrollFrameAsScrollableExternal();
            if (rootScrollFrame) {
                rootScrollFrame->PostScrolledAreaEventForCurrentArea();
            }
        }
    }

    // The FinishRestore call below can kill these, null them out so we don't
    // have invalid pointer lying around.
    newRootView = rootViewSibling = rootViewParent = nsnull;
    newVM = nsnull;

    // Simulate the completion of the load.
    nsDocShell::FinishRestore();

    // Restart plugins, and paint the content.
    if (shell) {
        shell->Thaw();

        newVM = shell->GetViewManager();
        if (newVM) {
            // When we insert the root view above the resulting invalidate is
            // dropped because painting is suppressed in the presshell until we
            // call Thaw. So we issue the invalidate here.
            newRootView = newVM->GetRootView();
            if (newRootView) {
                newVM->InvalidateView(newRootView);
            }
        }
    }

    return privWin->FireDelayedDOMEvents();
}

NS_IMETHODIMP
nsDocShell::CreateContentViewer(const char *aContentType,
                                nsIRequest * request,
                                nsIStreamListener ** aContentHandler)
{
    *aContentHandler = nsnull;

    // Can we check the content type of the current content viewer
    // and reuse it without destroying it and re-creating it?

    NS_ASSERTION(mLoadGroup, "Someone ignored return from Init()?");

    // Instantiate the content viewer object
    nsCOMPtr<nsIContentViewer> viewer;
    nsresult rv = NewContentViewerObj(aContentType, request, mLoadGroup,
                                      aContentHandler, getter_AddRefs(viewer));

    if (NS_FAILED(rv))
        return rv;

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
        mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
    }

    NS_ASSERTION(!mLoadingURI, "Re-entering unload?");
    
    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);
    if (aOpenedChannel) {
        aOpenedChannel->GetURI(getter_AddRefs(mLoadingURI));
    }
    FirePageHideNotification(!mSavingOldViewer);
    mLoadingURI = nsnull;

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

        // Make sure we have a URI to set currentURI.
        nsCOMPtr<nsIURI> failedURI;
        if (failedChannel) {
            NS_GetFinalChannelURI(failedChannel, getter_AddRefs(failedURI));
        }

        if (!failedURI) {
            failedURI = mFailedURI;
        }

        // When we don't have failedURI, something wrong will happen. See
        // bug 291876.
        MOZ_ASSERT(failedURI, "We don't have a URI for history APIs.");

        mFailedChannel = nsnull;
        mFailedURI = nsnull;

        // Create an shistory entry for the old load.
        if (failedURI) {
            bool errorOnLocationChangeNeeded =
                OnNewURI(failedURI, failedChannel, nsnull, mLoadType, false,
                         false, false);

            if (errorOnLocationChangeNeeded) {
                FireOnLocationChange(this, failedChannel, failedURI,
                                     LOCATION_CHANGE_ERROR_PAGE);
            }
        }

        // Be sure to have a correct mLSHE, it may have been cleared by
        // EndPageLoad. See bug 302115.
        if (mSessionHistory && !mLSHE) {
            PRInt32 idx;
            mSessionHistory->GetRequestedIndex(&idx);
            if (idx == -1)
                mSessionHistory->GetIndex(&idx);

            nsCOMPtr<nsIHistoryEntry> entry;
            mSessionHistory->GetEntryAtIndex(idx, false,
                                             getter_AddRefs(entry));
            mLSHE = do_QueryInterface(entry);
        }

        mLoadType = LOAD_ERROR_PAGE;
    }

    bool onLocationChangeNeeded = OnLoadingSite(aOpenedChannel, false);

    // let's try resetting the load group if we need to...
    nsCOMPtr<nsILoadGroup> currentLoadGroup;
    NS_ENSURE_SUCCESS(aOpenedChannel->
                      GetLoadGroup(getter_AddRefs(currentLoadGroup)),
                      NS_ERROR_FAILURE);

    if (currentLoadGroup != mLoadGroup) {
        nsLoadFlags loadFlags = 0;

        //Cancel any URIs that are currently loading...
        /// XXX: Need to do this eventually      Stop();
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

        mLoadGroup->AddRequest(request, nsnull);
        if (currentLoadGroup)
            currentLoadGroup->RemoveRequest(request, nsnull,
                                            NS_BINDING_RETARGETED);

        // Update the notification callbacks, so that progress and
        // status information are sent to the right docshell...
        aOpenedChannel->SetNotificationCallbacks(this);
    }

    NS_ENSURE_SUCCESS(Embed(viewer, "", (nsISupports *) nsnull),
                      NS_ERROR_FAILURE);

    mSavedRefreshURIList = nsnull;
    mSavingOldViewer = false;
    mEODForCurrentDocument = false;

    // if this document is part of a multipart document,
    // the ID can be used to distinguish it from the other parts.
    nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(request));
    if (multiPartChannel) {
      nsCOMPtr<nsIPresShell> shell;
      rv = GetPresShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell) {
        nsIDocument *doc = shell->GetDocument();
        if (doc) {
          PRUint32 partID;
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
      FavorPerformanceHint(true, NS_EVENT_STARVATION_DELAY_HINT);
    }

    if (onLocationChangeNeeded) {
      FireOnLocationChange(this, request, mCurrentURI, 0);
    }
  
    return NS_OK;
}

nsresult
nsDocShell::NewContentViewerObj(const char *aContentType,
                                nsIRequest * request, nsILoadGroup * aLoadGroup,
                                nsIStreamListener ** aContentHandler,
                                nsIContentViewer ** aViewer)
{
    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
        nsContentUtils::FindInternalContentViewer(aContentType);
    if (!docLoaderFactory) {
        return NS_ERROR_FAILURE;
    }

    // Now create an instance of the content viewer
    // nsLayoutDLF makes the determination if it should be a "view-source" instead of "view"
    nsresult rv = docLoaderFactory->CreateInstance("view",
                                                   aOpenedChannel,
                                                   aLoadGroup, aContentType,
                                                   static_cast<nsIContentViewerContainer*>(this),
                                                   nsnull,
                                                   aContentHandler,
                                                   aViewer);
    NS_ENSURE_SUCCESS(rv, rv);

    (*aViewer)->SetContainer(static_cast<nsIContentViewerContainer *>(this));
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetupNewViewer(nsIContentViewer * aNewViewer)
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

    PRInt32 x = 0;
    PRInt32 y = 0;
    PRInt32 cx = 0;
    PRInt32 cy = 0;

    // This will get the size from the current content viewer or from the
    // Init settings
    DoGetPositionAndSize(&x, &y, &cx, &cy);

    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parentAsItem)),
                      NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));

    nsCAutoString defaultCharset;
    nsCAutoString forceCharset;
    nsCAutoString hintCharset;
    PRInt32 hintCharsetSource;
    nsCAutoString prevDocCharset;
    PRInt32 minFontSize;
    float textZoom;
    float pageZoom;
    bool styleDisabled;
    // |newMUDV| also serves as a flag to set the data from the above vars
    nsCOMPtr<nsIMarkupDocumentViewer> newMUDV;

    if (mContentViewer || parent) {
        nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV;
        if (mContentViewer) {
            // Get any interesting state from old content viewer
            // XXX: it would be far better to just reuse the document viewer ,
            //      since we know we're just displaying the same document as before
            oldMUDV = do_QueryInterface(mContentViewer);

            // Tell the old content viewer to hibernate in session history when
            // it is destroyed.

            if (mSavingOldViewer && NS_FAILED(CaptureState())) {
                if (mOSHE) {
                    mOSHE->SyncPresentationState();
                }
                mSavingOldViewer = false;
            }
        }
        else {
            // No old content viewer, so get state from parent's content viewer
            nsCOMPtr<nsIContentViewer> parentContentViewer;
            parent->GetContentViewer(getter_AddRefs(parentContentViewer));
            oldMUDV = do_QueryInterface(parentContentViewer);
        }

        if (oldMUDV) {
            nsresult rv;

            newMUDV = do_QueryInterface(aNewViewer,&rv);
            if (newMUDV) {
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetDefaultCharacterSet(defaultCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetForceCharacterSet(forceCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetHintCharacterSet(hintCharset),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetHintCharacterSetSource(&hintCharsetSource),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetMinFontSize(&minFontSize),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetTextZoom(&textZoom),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetFullZoom(&pageZoom),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetAuthorStyleDisabled(&styleDisabled),
                                  NS_ERROR_FAILURE);
                NS_ENSURE_SUCCESS(oldMUDV->
                                  GetPrevDocCharacterSet(prevDocCharset),
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

        mContentViewer->Close(mSavingOldViewer ? mOSHE.get() : nsnull);
        aNewViewer->SetPreviousViewer(mContentViewer);
    }
    if (mOSHE && (!mContentViewer || !mSavingOldViewer)) {
        // We don't plan to save a viewer in mOSHE; tell it to drop
        // any other state it's holding.
        mOSHE->SyncPresentationState();
    }

    mContentViewer = nsnull;

    // Now that we're about to switch documents, forget all of our children.
    // Note that we cached them as needed up in CaptureState above.
    DestroyChildren();

    mContentViewer = aNewViewer;

    nsCOMPtr<nsIWidget> widget;
    NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);

    nsIntRect bounds(x, y, cx, cy);

    mContentViewer->SetNavigationTiming(mTiming);

    if (NS_FAILED(mContentViewer->Init(widget, bounds))) {
        mContentViewer = nsnull;
        NS_ERROR("ContentViewer Initialization failed");
        return NS_ERROR_FAILURE;
    }

    // If we have old state to copy, set the old state onto the new content
    // viewer
    if (newMUDV) {
        NS_ENSURE_SUCCESS(newMUDV->SetDefaultCharacterSet(defaultCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetForceCharacterSet(forceCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSet(hintCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->
                          SetHintCharacterSetSource(hintCharsetSource),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetPrevDocCharacterSet(prevDocCharset),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetMinFontSize(minFontSize),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetTextZoom(textZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetFullZoom(pageZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(newMUDV->SetAuthorStyleDisabled(styleDisabled),
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
nsDocShell::SetDocCurrentStateObj(nsISHEntry *shEntry)
{
    nsCOMPtr<nsIDocument> document = do_GetInterface(GetAsSupports(this));
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStructuredCloneContainer> scContainer;
    if (shEntry) {
        nsresult rv = shEntry->GetStateData(getter_AddRefs(scContainer));
        NS_ENSURE_SUCCESS(rv, rv);

        // If shEntry is null, just set the document's state object to null.
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
    nsresult rv = NS_OK, sameOrigin = NS_OK;

    if (!gValidateOrigin || !IsFrame()) {
        // Origin validation was turned off, or we're not a frame.
        // Permit all loads.

        return rv;
    }

    // We're a frame. Check that the caller has write permission to
    // the parent before allowing it to load anything into this
    // docshell.

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    bool ubwEnabled = false;
    rv = securityManager->IsCapabilityEnabled("UniversalXPConnect",
                                              &ubwEnabled);
    if (NS_FAILED(rv) || ubwEnabled) {
        return rv;
    }

    nsCOMPtr<nsIPrincipal> subjPrincipal;
    rv = securityManager->GetSubjectPrincipal(getter_AddRefs(subjPrincipal));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && subjPrincipal, rv);

    // Check if the caller is from the same origin as this docshell,
    // or any of its ancestors.
    nsCOMPtr<nsIDocShellTreeItem> item(this);
    do {
        nsCOMPtr<nsIScriptGlobalObject> sgo(do_GetInterface(item));
        nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(sgo));

        nsIPrincipal *p;
        if (!sop || !(p = sop->GetPrincipal())) {
            return NS_ERROR_UNEXPECTED;
        }

        // Compare origins
        bool subsumes;
        sameOrigin = subjPrincipal->Subsumes(p, &subsumes);
        if (NS_SUCCEEDED(sameOrigin)) {
            if (subsumes) {
                // Same origin, permit load

                return sameOrigin;
            }

            sameOrigin = NS_ERROR_DOM_PROP_ACCESS_DENIED;
        }

        nsCOMPtr<nsIDocShellTreeItem> tmp;
        item->GetSameTypeParent(getter_AddRefs(tmp));
        item.swap(tmp);
    } while (item);

    return sameOrigin;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************   
namespace
{

// Callback used by CopyFavicon to inform the favicon service that one URI
// (mNewURI) has the same favicon URI (OnComplete's aFaviconURI) as another.
class nsCopyFaviconCallback MOZ_FINAL : public nsIFaviconDataCallback
{
public:
    NS_DECL_ISUPPORTS

    nsCopyFaviconCallback(nsIURI *aNewURI)
      : mNewURI(aNewURI)
    {
    }

    NS_IMETHODIMP
    OnComplete(nsIURI *aFaviconURI, PRUint32 aDataLen,
               const PRUint8 *aData, const nsACString &aMimeType)
    {
        // Continue only if there is an associated favicon.
        if (!aFaviconURI) {
          return NS_OK;
        }

        NS_ASSERTION(aDataLen == 0,
                     "We weren't expecting the callback to deliver data.");
        nsCOMPtr<mozIAsyncFavicons> favSvc =
            do_GetService("@mozilla.org/browser/favicon-service;1");
        NS_ENSURE_STATE(favSvc);

        return favSvc->SetAndFetchFaviconForPage(mNewURI, aFaviconURI,
                                                 false, nsnull);
    }

private:
    nsCOMPtr<nsIURI> mNewURI;
};

NS_IMPL_ISUPPORTS1(nsCopyFaviconCallback, nsIFaviconDataCallback)

// Tell the favicon service that aNewURI has the same favicon as aOldURI.
void CopyFavicon(nsIURI *aOldURI, nsIURI *aNewURI)
{
    nsCOMPtr<mozIAsyncFavicons> favSvc =
        do_GetService("@mozilla.org/browser/favicon-service;1");
    if (favSvc) {
        nsCOMPtr<nsIFaviconDataCallback> callback =
            new nsCopyFaviconCallback(aNewURI);
        favSvc->GetFaviconURLForPage(aOldURI, callback);
    }
}

} // anonymous namespace

class InternalLoadEvent : public nsRunnable
{
public:
    InternalLoadEvent(nsDocShell* aDocShell, nsIURI * aURI, nsIURI * aReferrer,
                      nsISupports * aOwner, PRUint32 aFlags,
                      const char* aTypeHint, nsIInputStream * aPostData,
                      nsIInputStream * aHeadersData, PRUint32 aLoadType,
                      nsISHEntry * aSHEntry, bool aFirstParty) :
        mDocShell(aDocShell),
        mURI(aURI),
        mReferrer(aReferrer),
        mOwner(aOwner),
        mPostData(aPostData),
        mHeadersData(aHeadersData),
        mSHEntry(aSHEntry),
        mFlags(aFlags),
        mLoadType(aLoadType),
        mFirstParty(aFirstParty)
    {
        // Make sure to keep null things null as needed
        if (aTypeHint) {
            mTypeHint = aTypeHint;
        }
    }
    
    NS_IMETHOD Run() {
        return mDocShell->InternalLoad(mURI, mReferrer, mOwner, mFlags,
                                       nsnull, mTypeHint.get(),
                                       mPostData, mHeadersData, mLoadType,
                                       mSHEntry, mFirstParty, nsnull, nsnull);
    }

private:

    // Use IDL strings so .get() returns null by default
    nsXPIDLString mWindowTarget;
    nsXPIDLCString mTypeHint;

    nsRefPtr<nsDocShell> mDocShell;
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsIURI> mReferrer;
    nsCOMPtr<nsISupports> mOwner;
    nsCOMPtr<nsIInputStream> mPostData;
    nsCOMPtr<nsIInputStream> mHeadersData;
    nsCOMPtr<nsISHEntry> mSHEntry;
    PRUint32 mFlags;
    PRUint32 mLoadType;
    bool mFirstParty;
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
    return mDocumentRequest &&
           mDocumentRequest != GetCurrentDocChannel();
}

NS_IMETHODIMP
nsDocShell::InternalLoad(nsIURI * aURI,
                         nsIURI * aReferrer,
                         nsISupports * aOwner,
                         PRUint32 aFlags,
                         const PRUnichar *aWindowTarget,
                         const char* aTypeHint,
                         nsIInputStream * aPostData,
                         nsIInputStream * aHeadersData,
                         PRUint32 aLoadType,
                         nsISHEntry * aSHEntry,
                         bool aFirstParty,
                         nsIDocShell** aDocShell,
                         nsIRequest** aRequest)
{
    nsresult rv = NS_OK;
    mOriginalUriString.Truncate();

#ifdef PR_LOGGING
    if (gDocShellLeakLog && PR_LOG_TEST(gDocShellLeakLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        if (aURI)
            aURI->GetSpec(spec);
        PR_LogPrint("DOCSHELL %p InternalLoad %s\n", this, spec.get());
    }
#endif
    
    // Initialize aDocShell/aRequest
    if (aDocShell) {
        *aDocShell = nsnull;
    }
    if (aRequest) {
        *aRequest = nsnull;
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
        if ((isWyciwyg && NS_SUCCEEDED(rv)) || NS_FAILED(rv)) 
            return NS_ERROR_FAILURE;
    }

    bool bIsJavascript = false;
    if (NS_FAILED(aURI->SchemeIs("javascript", &bIsJavascript))) {
        bIsJavascript = false;
    }

    //
    // First, notify any nsIContentPolicy listeners about the document load.
    // Only abort the load if a content policy listener explicitly vetos it!
    //
    nsCOMPtr<nsIDOMElement> requestingElement;
    // Use nsPIDOMWindow since we _want_ to cross the chrome boundary if needed
    nsCOMPtr<nsPIDOMWindow> privateWin(do_QueryInterface(mScriptGlobal));
    if (privateWin)
        requestingElement = privateWin->GetFrameElementInternal();

    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
    PRUint32 contentType;
    if (IsFrame()) {
        NS_ASSERTION(requestingElement, "A frame but no DOM element!?");
        contentType = nsIContentPolicy::TYPE_SUBDOCUMENT;
    } else {
        contentType = nsIContentPolicy::TYPE_DOCUMENT;
    }

    nsISupports* context = requestingElement;
    if (!context) {
        context =  mScriptGlobal;
    }

    // XXXbz would be nice to know the loading principal here... but we don't
    nsCOMPtr<nsIPrincipal> loadingPrincipal = do_QueryInterface(aOwner);
    if (!loadingPrincipal && aReferrer) {
        nsCOMPtr<nsIScriptSecurityManager> secMan =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = secMan->GetCodebasePrincipal(aReferrer,
                                          getter_AddRefs(loadingPrincipal));
    }

    rv = NS_CheckContentLoadPolicy(contentType,
                                   aURI,
                                   loadingPrincipal,
                                   context,
                                   EmptyCString(), //mime guess
                                   nsnull,         //extra
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
        // nsContentUtils::SetUpChannelOwner.
        // Except we reverse the rv check to be safe in case
        // nsContentUtils::URIInheritsSecurityContext fails here and
        // succeeds there.
        rv = nsContentUtils::URIInheritsSecurityContext(aURI, &willInherit);
        if (NS_FAILED(rv) || willInherit || NS_IsAboutBlank(aURI)) {
            nsCOMPtr<nsIDocShellTreeItem> treeItem = this;
            do {
                nsCOMPtr<nsIDocShell> itemDocShell =
                    do_QueryInterface(treeItem);
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
        
        // Locate the target DocShell.
        // This may involve creating a new toplevel window - if necessary.
        //
        nsCOMPtr<nsIDocShellTreeItem> targetItem;
        FindItemWithName(aWindowTarget, nsnull, this,
                         getter_AddRefs(targetItem));

        nsCOMPtr<nsIDocShell> targetDocShell = do_QueryInterface(targetItem);
        
        bool isNewWindow = false;
        if (!targetDocShell) {
            nsCOMPtr<nsIDOMWindow> win =
                do_GetInterface(GetAsSupports(this));
            NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

            nsDependentString name(aWindowTarget);
            nsCOMPtr<nsIDOMWindow> newWin;
            rv = win->Open(EmptyString(), // URL to load
                           name,          // window name
                           EmptyString(), // Features
                           getter_AddRefs(newWin));

            // In some cases the Open call doesn't actually result in a new
            // window being opened.  We can detect these cases by examining the
            // document in |newWin|, if any.
            nsCOMPtr<nsPIDOMWindow> piNewWin = do_QueryInterface(newWin);
            if (piNewWin) {
                nsCOMPtr<nsIDocument> newDoc =
                    do_QueryInterface(piNewWin->GetExtantDocument());
                if (!newDoc || newDoc->IsInitialDocument()) {
                    isNewWindow = true;
                    aFlags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;
                }
            }

            nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(newWin);
            targetDocShell = do_QueryInterface(webNav);
        }

        //
        // Transfer the load to the target DocShell...  Pass nsnull as the
        // window target name from to prevent recursive retargeting!
        //
        if (NS_SUCCEEDED(rv) && targetDocShell) {
            rv = targetDocShell->InternalLoad(aURI,
                                              aReferrer,
                                              owner,
                                              aFlags,
                                              nsnull,         // No window target
                                              aTypeHint,
                                              aPostData,
                                              aHeadersData,
                                              aLoadType,
                                              aSHEntry,
                                              aFirstParty,
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
                    nsCOMPtr<nsIDOMWindow> domWin =
                        do_GetInterface(targetDocShell);
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
            }
            else if (isNewWindow) {
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

    rv = CheckLoadingPermissions();
    if (NS_FAILED(rv)) {
        return rv;
    }

    // If this docshell is owned by a frameloader, make sure to cancel
    // possible frameloader initialization before loading a new page.
    nsCOMPtr<nsIDocShellTreeItem> parent;
    GetParent(getter_AddRefs(parent));
    if (parent) {
      nsCOMPtr<nsIDocument> doc = do_GetInterface(parent);
      if (doc) {
        doc->TryCancelFrameLoaderInitialization(this);
      }
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
                new InternalLoadEvent(this, aURI, aReferrer, aOwner, aFlags,
                                      aTypeHint, aPostData, aHeadersData,
                                      aLoadType, aSHEntry, aFirstParty);
            return NS_DispatchToCurrentThread(ev);
        }

        // Just ignore this load attempt
        return NS_OK;
    }

    // Before going any further vet loads initiated by external programs.
    if (aLoadType == LOAD_NORMAL_EXTERNAL) {
        // Disallow external chrome: loads targetted at content windows
        bool isChrome = false;
        if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome) {
            NS_WARNING("blocked external chrome: url -- use '-chrome' option");
            return NS_ERROR_FAILURE;
        }

        // clear the decks to prevent context bleed-through (bug 298255)
        rv = CreateAboutBlankContentViewer(nsnull, nsnull);
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

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

        // Split mCurrentURI and aURI on the '#' character.  Make sure we read
        // the return values of SplitURIAtHash; if it fails, we don't want to
        // allow a short-circuited navigation.
        nsCAutoString curBeforeHash, curHash, newBeforeHash, newHash;
        nsresult splitRv1, splitRv2;
        splitRv1 = mCurrentURI ?
            nsContentUtils::SplitURIAtHash(mCurrentURI,
                                           curBeforeHash, curHash) :
            NS_ERROR_FAILURE;
        splitRv2 = nsContentUtils::SplitURIAtHash(aURI, newBeforeHash, newHash);

        bool sameExceptHashes = NS_SUCCEEDED(splitRv1) &&
                                  NS_SUCCEEDED(splitRv2) &&
                                  curBeforeHash.Equals(newBeforeHash);

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
          (!aSHEntry && aPostData == nsnull &&
           sameExceptHashes && !newHash.IsEmpty());

        if (doShortCircuitedLoad) {
            // Save the current URI; we need it if we fire a hashchange later.
            nsCOMPtr<nsIURI> oldURI = mCurrentURI;

            // Save the position of the scrollers.
            nscoord cx = 0, cy = 0;
            GetCurScrollPos(ScrollOrientation_X, &cx);
            GetCurScrollPos(ScrollOrientation_Y, &cy);

            // ScrollToAnchor doesn't necessarily cause us to scroll the window;
            // the function decides whether a scroll is appropriate based on the
            // arguments it receives.  But even if we don't end up scrolling,
            // ScrollToAnchor performs other important tasks, such as informing
            // the presShell that we have a new hash.  See bug 680257.
            rv = ScrollToAnchor(curHash, newHash, aLoadType);
            NS_ENSURE_SUCCESS(rv, rv);

            // Reset mLoadType to its original value once we exit this block,
            // because this short-circuited load might have started after a
            // normal, network load, and we don't want to clobber its load type.
            // See bug 737307.
            AutoRestore<PRUint32> loadTypeResetter(mLoadType);

            // If a non-short-circuit load (i.e., a network load) is pending,
            // make this a replacement load, so that we don't add a SHEntry here
            // and the network load goes into the SHEntry it expects to.
            if (JustStartedNetworkLoad() && (aLoadType & LOAD_CMD_NORMAL)) {
                mLoadType = LOAD_NORMAL_REPLACE;
            }
            else {
                mLoadType = aLoadType;
            }

            mURIResultedInDocument = true;

            /* we need to assign mLSHE to aSHEntry right here, so that on History loads,
             * SetCurrentURI() called from OnNewURI() will send proper
             * onLocationChange() notifications to the browser to update
             * back/forward buttons.
             */
            SetHistoryEntry(&mLSHE, aSHEntry);

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
            OnNewURI(aURI, nsnull, owner, mLoadType, true, true, true);

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
                    if (mLSHE)
                        mLSHE->AdoptBFCacheEntry(mOSHE);
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
                if (postData)
                    mOSHE->SetPostData(postData);

                // Make sure we won't just repost without hitting the
                // cache first
                if (cacheKey)
                    mOSHE->SetCacheKey(cacheKey);
            }

            /* restore previous position of scroller(s), if we're moving
             * back in history (bug 59774)
             */
            if (mOSHE && (aLoadType == LOAD_HISTORY || aLoadType == LOAD_RELOAD_NORMAL))
            {
                nscoord bx, by;
                mOSHE->GetScrollPosition(&bx, &by);
                SetCurScrollPosEx(bx, by);
            }

            /* Clear out mLSHE so that further anchor visits get
             * recorded in SH and SH won't misbehave. 
             */
            SetHistoryEntry(&mLSHE, nsnull);
            /* Set the title for the SH entry for this target url. so that
             * SH menus in go/back/forward buttons won't be empty for this.
             */
            if (mSessionHistory) {
                PRInt32 index = -1;
                mSessionHistory->GetIndex(&index);
                nsCOMPtr<nsIHistoryEntry> hEntry;
                mSessionHistory->GetEntryAtIndex(index, false,
                                                 getter_AddRefs(hEntry));
                NS_ENSURE_TRUE(hEntry, NS_ERROR_FAILURE);
                nsCOMPtr<nsISHEntry> shEntry(do_QueryInterface(hEntry));
                if (shEntry)
                    shEntry->SetTitle(mTitle);
            }

            /* Set the title for the Global History entry for this anchor url.
             */
            if (mUseGlobalHistory) {
                nsCOMPtr<IHistory> history = services::GetHistoryService();
                if (history) {
                    history->SetURITitle(aURI, mTitle);
                }
                else if (mGlobalHistory) {
                    mGlobalHistory->SetPageTitle(aURI, mTitle);
                }
            }

            // Set the doc's URI according to the new history entry's URI.
            nsCOMPtr<nsIDocument> doc =
              do_GetInterface(GetAsSupports(this));
            NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
            doc->SetDocumentURI(aURI);

            SetDocCurrentStateObj(mOSHE);

            // Dispatch the popstate and hashchange events, as appropriate.
            nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mScriptGlobal);
            if (window) {
                // Fire a hashchange event URIs differ, and only in their hashes.
                bool doHashchange = sameExceptHashes && !curHash.Equals(newHash);

                if (historyNavBetweenSameDoc || doHashchange) {
                  window->DispatchSyncPopState();
                }

                if (doHashchange) {
                  // Make sure to use oldURI here, not mCurrentURI, because by
                  // now, mCurrentURI has changed!
                  window->DispatchAsyncHashchange(oldURI, aURI);
                }
            }

            // Inform the favicon service that the favicon for oldURI also
            // applies to aURI.
            CopyFavicon(oldURI, aURI);

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
    if (!bIsJavascript) {
        MaybeInitTiming();
    }
    if (mTiming) {
      mTiming->NotifyBeforeUnload();
    }
    // Check if the page doesn't want to be unloaded. The javascript:
    // protocol handler deals with this for javascript: URLs.
    if (!bIsJavascript && mContentViewer) {
        bool okToUnload;
        rv = mContentViewer->PermitUnload(false, &okToUnload);

        if (NS_SUCCEEDED(rv) && !okToUnload) {
            // The user chose not to unload the page, interrupt the
            // load.
            return NS_OK;
        }
    }

    if (mTiming) {
      mTiming->NotifyUnloadAccepted(mCurrentURI);
    }

    // Check for saving the presentation here, before calling Stop().
    // This is necessary so that we can catch any pending requests.
    // Since the new request has not been created yet, we pass null for the
    // new request parameter.
    // Also pass nsnull for the document, since it doesn't affect the return
    // value for our purposes here.
    bool savePresentation = CanSavePresentation(aLoadType, nsnull, nsnull);

    // Don't stop current network activity for javascript: URL's since
    // they might not result in any data, and thus nothing should be
    // stopped in those cases. In the case where they do result in
    // data, the javascript: URL channel takes care of stopping
    // current network activity.
    if (!bIsJavascript) {
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

        if (NS_FAILED(rv)) 
            return rv;
    }

    mLoadType = aLoadType;

    // mLSHE should be assigned to aSHEntry, only after Stop() has
    // been called. But when loading an error page, do not clear the
    // mLSHE for the real page.
    if (mLoadType != LOAD_ERROR_PAGE)
        SetHistoryEntry(&mLSHE, aSHEntry);

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
                    mContentViewer->SetPreviousViewer(nsnull);
                    prevViewer->Destroy();
                }
            }
        }
        nsCOMPtr<nsISHEntry> oldEntry = mOSHE;
        bool restoring;
        rv = RestorePresentation(aSHEntry, &restoring);
        if (restoring)
            return rv;

        // We failed to restore the presentation, so clean up.
        // Both the old and new history entries could potentially be in
        // an inconsistent state.
        if (NS_FAILED(rv)) {
            if (oldEntry)
                oldEntry->SyncPresentationState();

            aSHEntry->SyncPresentationState();
        }
    }

    nsCOMPtr<nsIRequest> req;
    rv = DoURILoad(aURI, aReferrer,
                   !(aFlags & INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER),
                   owner, aTypeHint, aPostData, aHeadersData, aFirstParty,
                   aDocShell, getter_AddRefs(req),
                   (aFlags & INTERNAL_LOAD_FLAGS_FIRST_LOAD) != 0,
                   (aFlags & INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER) != 0,
                   (aFlags & INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES) != 0);
    if (req && aRequest)
        NS_ADDREF(*aRequest = req);

    if (NS_FAILED(rv)) {
        nsCOMPtr<nsIChannel> chan(do_QueryInterface(req));
        DisplayLoadError(rv, aURI, nsnull, chan);
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
            document = do_GetInterface(parentItem);
        }
    }

    if (!document) {
        if (!aConsiderCurrentDocument) {
            return nsnull;
        }

        // Make sure we end up with _something_ as the principal no matter
        // what.
        EnsureContentViewer();  // If this fails, we'll just get a null
                                // docViewer and bail.

        if (!mContentViewer)
            return nsnull;
        document = mContentViewer->GetDocument();
    }

    //-- Get the document's principal
    if (document) {
        nsIPrincipal *docPrincipal = document->NodePrincipal();

        // Don't allow loads in typeContent docShells to inherit the system
        // principal from existing documents.
        if (inheritedFromCurrent &&
            mItemType == typeContent &&
            nsContentUtils::IsSystemPrincipal(docPrincipal)) {
            return nsnull;
        }

        return docPrincipal;
    }

    return nsnull;
}

bool
nsDocShell::ShouldCheckAppCache(nsIURI *aURI)
{
    if (mInPrivateBrowsing) {
        return false;
    }

    nsCOMPtr<nsIOfflineCacheUpdateService> offlineService =
        do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
    if (!offlineService) {
        return false;
    }

    bool allowed;
    nsresult rv = offlineService->OfflineAppAllowedForURI(aURI,
                                                          nsnull,
                                                          &allowed);
    return NS_SUCCEEDED(rv) && allowed;
}

nsresult
nsDocShell::DoURILoad(nsIURI * aURI,
                      nsIURI * aReferrerURI,
                      bool aSendReferrer,
                      nsISupports * aOwner,
                      const char * aTypeHint,
                      nsIInputStream * aPostData,
                      nsIInputStream * aHeadersData,
                      bool aFirstParty,
                      nsIDocShell ** aDocShell,
                      nsIRequest ** aRequest,
                      bool aIsNewWindowTarget,
                      bool aBypassClassifier,
                      bool aForceAllowCookies)
{
    nsresult rv;
    nsCOMPtr<nsIURILoader> uriLoader;

    uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL;
    if (aFirstParty) {
        // tag first party URL loads
        loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
    }

    if (mLoadType == LOAD_ERROR_PAGE) {
        // Error pages are LOAD_BACKGROUND
        loadFlags |= nsIChannel::LOAD_BACKGROUND;
    }

    // check for Content Security Policy to pass along with the
    // new channel we are creating
    nsCOMPtr<nsIChannelPolicy> channelPolicy;
    if (IsFrame()) {
        // check the parent docshell for a CSP
        nsCOMPtr<nsIContentSecurityPolicy> csp;
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        GetSameTypeParent(getter_AddRefs(parentItem));
        nsCOMPtr<nsIDocument> doc = do_GetInterface(parentItem);
        if (doc) {
            rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
            NS_ENSURE_SUCCESS(rv, rv);
            if (csp) {
                channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
                channelPolicy->SetContentSecurityPolicy(csp);
                channelPolicy->SetLoadType(nsIContentPolicy::TYPE_SUBDOCUMENT);
            }
        }
    }

    // open a channel for the url
    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel),
                       aURI,
                       nsnull,
                       nsnull,
                       static_cast<nsIInterfaceRequestor *>(this),
                       loadFlags,
                       channelPolicy);
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

    nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
        do_QueryInterface(channel);
    if (appCacheChannel) {
        // Any document load should not inherit application cache.
        appCacheChannel->SetInheritApplicationCache(false);

        // Loads with the correct permissions should check for a matching
        // application cache.
        // Permission will be checked in the parent process
        if (GeckoProcessType_Default != XRE_GetProcessType())
            appCacheChannel->SetChooseApplicationCache(true);
        else
            appCacheChannel->SetChooseApplicationCache(
                ShouldCheckAppCache(aURI));
    }

    // Make sure to give the caller a channel if we managed to create one
    // This is important for correct error page/session history interaction
    if (aRequest)
        NS_ADDREF(*aRequest = channel);

    channel->SetOriginalURI(aURI);
    if (aTypeHint && *aTypeHint) {
        channel->SetContentType(nsDependentCString(aTypeHint));
        mContentTypeHint = aTypeHint;
    }
    else {
        mContentTypeHint.Truncate();
    }
    
    //hack
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal(do_QueryInterface(channel));
    if (httpChannelInternal) {
      if (aForceAllowCookies) {
        httpChannelInternal->SetForceAllowThirdPartyCookie(true);
      } 
      if (aFirstParty) {
        httpChannelInternal->SetDocumentURI(aURI);
      } else {
        httpChannelInternal->SetDocumentURI(aReferrerURI);
      }
    }

    nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(channel));
    if (props)
    {
      // save true referrer for those who need it (e.g. xpinstall whitelisting)
      // Currently only http and ftp channels support this.
      props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                    aReferrerURI);
    }

    //
    // If this is a HTTP channel, then set up the HTTP specific information
    // (ie. POST data, referrer, ...)
    //
    if (httpChannel) {
        nsCOMPtr<nsICachingChannel>  cacheChannel(do_QueryInterface(httpChannel));
        /* Get the cache Key from SH */
        nsCOMPtr<nsISupports> cacheKey;
        if (mLSHE) {
            mLSHE->GetCacheKey(getter_AddRefs(cacheKey));
        }
        else if (mOSHE)          // for reload cases
            mOSHE->GetCacheKey(getter_AddRefs(cacheKey));

        // figure out if we need to set the post data stream on the channel...
        // right now, this is only done for http channels.....
        if (aPostData) {
            // XXX it's a bit of a hack to rewind the postdata stream here but
            // it has to be done in case the post data is being reused multiple
            // times.
            nsCOMPtr<nsISeekableStream>
                postDataSeekable(do_QueryInterface(aPostData));
            if (postDataSeekable) {
                rv = postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
            NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

            // we really need to have a content type associated with this stream!!
            uploadChannel->SetUploadStream(aPostData, EmptyCString(), -1);
            /* If there is a valid postdata *and* it is a History Load,
             * set up the cache key on the channel, to retrieve the
             * data *only* from the cache. If it is a normal reload, the 
             * cache is free to go to the server for updated postdata. 
             */
            if (cacheChannel && cacheKey) {
                if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
                    cacheChannel->SetCacheKey(cacheKey);
                    PRUint32 loadFlags;
                    if (NS_SUCCEEDED(channel->GetLoadFlags(&loadFlags)))
                        channel->SetLoadFlags(loadFlags | nsICachingChannel::LOAD_ONLY_FROM_CACHE);
                }
                else if (mLoadType == LOAD_RELOAD_NORMAL)
                    cacheChannel->SetCacheKey(cacheKey);
            }         
        }
        else {
            /* If there is no postdata, set the cache key on the channel, and
             * do not set the LOAD_ONLY_FROM_CACHE flag, so that the channel
             * will be free to get it from net if it is not found in cache.
             * New cache may use it creatively on CGI pages with GET
             * method and even on those that say "no-cache"
             */
            if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_NORMAL 
                || mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
                if (cacheChannel && cacheKey)
                    cacheChannel->SetCacheKey(cacheKey);
            }
        }
        if (aHeadersData) {
            rv = AddHeadersToChannel(aHeadersData, httpChannel);
        }
        // Set the referrer explicitly
        if (aReferrerURI && aSendReferrer) {
            // Referrer is currenly only set for link clicks here.
            httpChannel->SetReferrer(aReferrerURI);
        }
    }

    nsCOMPtr<nsIPrincipal> ownerPrincipal(do_QueryInterface(aOwner));
    nsContentUtils::SetUpChannelOwner(ownerPrincipal, channel, aURI, true);

    nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(channel);
    if (scriptChannel) {
        // Allow execution against our context if the principals match
        scriptChannel->
            SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
    }

    if (aIsNewWindowTarget) {
        nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(channel);
        if (props) {
            props->SetPropertyAsBool(
                NS_LITERAL_STRING("docshell.newWindowTarget"),
                true);
        }
    }

    if (Preferences::GetBool("dom.enable_performance", false)) {
        nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(channel));
        if (timedChannel) {
            timedChannel->SetTimingEnabled(true);
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
AppendSegmentToString(nsIInputStream *in,
                      void *closure,
                      const char *fromRawSegment,
                      PRUint32 toOffset,
                      PRUint32 count,
                      PRUint32 *writeCount)
{
    // aFromSegment now contains aCount bytes of data.

    nsCAutoString *buf = static_cast<nsCAutoString *>(closure);
    buf->Append(fromRawSegment, count);

    // Indicate that we have consumed all of aFromSegment
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddHeadersToChannel(nsIInputStream *aHeadersData,
                                nsIChannel *aGenericChannel)
{
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aGenericChannel);
    NS_ENSURE_STATE(httpChannel);

    PRUint32 numRead;
    nsCAutoString headersString;
    nsresult rv = aHeadersData->ReadSegments(AppendSegmentToString,
                                             &headersString,
                                             PR_UINT32_MAX,
                                             &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    // used during the manipulation of the String from the InputStream
    nsCAutoString headerName;
    nsCAutoString headerValue;
    PRInt32 crlf;
    PRInt32 colon;

    //
    // Iterate over the headersString: for each "\r\n" delimited chunk,
    // add the value as a header to the nsIHttpChannel
    //

    static const char kWhitespace[] = "\b\t\r\n ";
    while (true) {
        crlf = headersString.Find("\r\n");
        if (crlf == kNotFound)
            return NS_OK;

        const nsCSubstring &oneHeader = StringHead(headersString, crlf);

        colon = oneHeader.FindChar(':');
        if (colon == kNotFound)
            return NS_ERROR_UNEXPECTED;

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

nsresult nsDocShell::DoChannelLoad(nsIChannel * aChannel,
                                   nsIURILoader * aURILoader,
                                   bool aBypassClassifier)
{
    nsresult rv;
    // Mark the channel as being a document URI and allow content sniffing...
    nsLoadFlags loadFlags = 0;
    (void) aChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_DOCUMENT_URI |
                 nsIChannel::LOAD_CALL_CONTENT_SNIFFERS;

    // Load attributes depend on load type...
    switch (mLoadType) {
    case LOAD_HISTORY:
        {
            // Only send VALIDATE_NEVER if mLSHE's URI was never changed via
            // push/replaceState (bug 669671).
            bool uriModified = false;
            if (mLSHE) {
                mLSHE->GetURIWasModified(&uriModified);
            }

            if (!uriModified)
                loadFlags |= nsIRequest::VALIDATE_NEVER;
        }
        break;

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
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
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

    (void) aChannel->SetLoadFlags(loadFlags);

    rv = aURILoader->OpenURI(aChannel,
                             (mLoadType == LOAD_LINK),
                             this);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

nsresult
nsDocShell::ScrollToAnchor(nsACString & aCurHash, nsACString & aNewHash,
                           PRUint32 aLoadType)
{
    if (!mCurrentURI) {
        return NS_OK;
    }

    nsCOMPtr<nsIPresShell> shell;
    nsresult rv = GetPresShell(getter_AddRefs(shell));
    if (NS_FAILED(rv) || !shell) {
        // If we failed to get the shell, or if there is no shell,
        // nothing left to do here.
        return rv;
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

        char *str = ToNewCString(newHashName);
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
        rv = NS_ERROR_FAILURE;
        NS_ConvertUTF8toUTF16 uStr(str);
        if (!uStr.IsEmpty()) {
            rv = shell->GoToAnchor(NS_ConvertUTF8toUTF16(str), scroll);
        }
        nsMemory::Free(str);

        // Above will fail if the anchor name is not UTF-8.  Need to
        // convert from document charset to unicode.
        if (NS_FAILED(rv)) {
                
            // Get a document charset
            NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
            nsIDocument* doc = mContentViewer->GetDocument();
            NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
            const nsACString &aCharset = doc->GetDocumentCharacterSet();

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
            shell->GoToAnchor(uStr, scroll && !uStr.IsEmpty());
        }
    }
    else {

        // Tell the shell it's at an anchor, without scrolling.
        shell->GoToAnchor(EmptyString(), false);
        
        // An empty anchor was found, but if it's a load from history,
        // we don't have to jump to the top of the page. Scrollbar 
        // position will be restored by the caller, based on positions
        // stored in session history.
        if (aLoadType == LOAD_HISTORY || aLoadType == LOAD_RELOAD_NORMAL)
            return NS_OK;
        // An empty anchor. Scroll to the top of the page.  Ignore the
        // return value; failure to scroll here (e.g. if there is no
        // root scrollframe) is not grounds for canceling the load!
        SetCurScrollPosEx(0, 0);
    }

    return NS_OK;
}

void
nsDocShell::SetupReferrerFromChannel(nsIChannel * aChannel)
{
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
        nsCOMPtr<nsIURI> referrer;
        nsresult rv = httpChannel->GetReferrer(getter_AddRefs(referrer));
        if (NS_SUCCEEDED(rv)) {
            SetReferrerURI(referrer);
        }
    }
}

bool
nsDocShell::OnNewURI(nsIURI * aURI, nsIChannel * aChannel, nsISupports* aOwner,
                     PRUint32 aLoadType, bool aFireOnLocationChange,
                     bool aAddToGlobalHistory, bool aCloneSHChildren)
{
    NS_PRECONDITION(aURI, "uri is null");
    NS_PRECONDITION(!aChannel || !aOwner, "Shouldn't have both set");

#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aChannel)
            aChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]::OnNewURI(\"%s\", [%s], 0x%x)\n", this, spec.get(),
                chanName.get(), aLoadType));
    }
#endif

    bool equalUri = false;

    // Get the post data and the HTTP response code from the channel.
    PRUint32 responseStatus = 0;
    nsCOMPtr<nsIInputStream> inputStream;
    if (aChannel) {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));

        // Check if the HTTPChannel is hiding under a multiPartChannel
        if (!httpChannel)  {
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

    // We don't update session history on reload.
    bool updateSHistory = updateGHistory && (!(aLoadType & LOAD_CMD_RELOAD));

    /* Create SH Entry (mLSHE) only if there is a  SessionHistory object (mSessionHistory) in
     * the current frame or in the root docshell
     */
    nsCOMPtr<nsISHistory> rootSH = mSessionHistory;
    if (!rootSH) {
        // Get the handle to SH from the root docshell          
        GetRootSessionHistory(getter_AddRefs(rootSH));
        if (!rootSH) {
            updateSHistory = false;
            updateGHistory = false; // XXX Why global history too?
        }
    }  // rootSH

    // Check if the url to be loaded is the same as the one already loaded.
    if (mCurrentURI)
        aURI->Equals(mCurrentURI, &equalUri);

#ifdef DEBUG
    bool shAvailable = (rootSH != nsnull);

    // XXX This log message is almost useless because |updateSHistory|
    //     and |updateGHistory| are not correct at this point.

    PR_LOG(gDocShellLog, PR_LOG_DEBUG,
           ("  shAvailable=%i updateSHistory=%i updateGHistory=%i"
            " equalURI=%i\n",
            shAvailable, updateSHistory, updateGHistory, equalUri));

    if (shAvailable && mCurrentURI && !mOSHE && aLoadType != LOAD_ERROR_PAGE) {
        NS_ASSERTION(NS_IsAboutBlank(mCurrentURI), "no SHEntry for a non-transient viewer?");
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
        !inputStream)
    {
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
    if (aChannel &&
        (aLoadType == LOAD_RELOAD_BYPASS_CACHE ||
         aLoadType == LOAD_RELOAD_BYPASS_PROXY ||
         aLoadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE)) {
        NS_ASSERTION(!updateSHistory,
                     "We shouldn't be updating session history for forced"
                     " reloads!");
        
        nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aChannel));
        nsCOMPtr<nsISupports>  cacheKey;
        // Get the Cache Key and store it in SH.
        if (cacheChannel)
            cacheChannel->GetCacheKey(getter_AddRefs(cacheKey));
        // If we already have a loading history entry, store the new cache key
        // in it.  Otherwise, since we're doing a reload and won't be updating
        // our history entry, store the cache key in our current history entry.
        if (mLSHE)
            mLSHE->SetCacheKey(cacheKey);
        else if (mOSHE)
            mOSHE->SetCacheKey(cacheKey);

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
            (void) AddToSessionHistory(aURI, aChannel, aOwner, aCloneSHChildren,
                                       getter_AddRefs(mLSHE));
        }
    }

    // If this is a POST request, we do not want to include this in global
    // history.
    if (updateGHistory &&
        aAddToGlobalHistory && 
        !ChannelIsPost(aChannel)) {
        nsCOMPtr<nsIURI> previousURI;
        PRUint32 previousFlags = 0;

        if (aLoadType & LOAD_CMD_RELOAD) {
            // On a reload request, we don't set redirecting flags.
            previousURI = aURI;
        } else {
            ExtractLastVisit(aChannel, getter_AddRefs(previousURI),
                             &previousFlags);
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
    PRUint32 locationFlags = aCloneSHChildren?
                                 PRUint32(LOCATION_CHANGE_SAME_DOCUMENT) : 0;

    bool onLocationChangeNeeded = SetCurrentURI(aURI, aChannel,
                                                aFireOnLocationChange,
                                                locationFlags);
    // Make sure to store the referrer from the channel, if any
    SetupReferrerFromChannel(aChannel);
    return onLocationChangeNeeded;
}

bool
nsDocShell::OnLoadingSite(nsIChannel * aChannel, bool aFireOnLocationChange,
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
    return OnNewURI(uri, aChannel, nsnull, mLoadType, aFireOnLocationChange,
                    aAddToGlobalHistory, false);

}

void
nsDocShell::SetReferrerURI(nsIURI * aURI)
{
    mReferrerURI = aURI;        // This assigment addrefs
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::AddState(nsIVariant *aData, const nsAString& aTitle,
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
    AutoRestore<PRUint32> loadTypeResetter(mLoadType);

    // pushState effectively becomes replaceState when we've started a network
    // load but haven't adopted its document yet.  This mirrors what we do with
    // changes to the hash at this stage of the game.
    if (JustStartedNetworkLoad()) {
        aReplace = true;
    }

    nsCOMPtr<nsIDocument> document = do_GetInterface(GetAsSupports(this));
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    // Step 1: Serialize aData using structured clone.
    nsCOMPtr<nsIStructuredCloneContainer> scContainer;

    // scContainer->Init might cause arbitrary JS to run, and this code might
    // navigate the page we're on, potentially to a different origin! (bug
    // 634834)  To protect against this, we abort if our principal changes due
    // to the InitFromVariant() call.
    {
        nsCOMPtr<nsIDocument> origDocument =
            do_GetInterface(GetAsSupports(this));
        if (!origDocument)
            return NS_ERROR_DOM_SECURITY_ERR;
        nsCOMPtr<nsIPrincipal> origPrincipal = origDocument->NodePrincipal();

        scContainer = new nsStructuredCloneContainer();
        JSContext *cx = aCx;
        if (!cx) {
            cx = nsContentUtils::GetContextFromDocument(document);
        }
        rv = scContainer->InitFromVariant(aData, cx);

        // If we're running in the document's context and the structured clone
        // failed, clear the context's pending exception.  See bug 637116.
        if (NS_FAILED(rv) && !aCx) {
            JS_ClearPendingException(aCx);
        }
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDocument> newDocument =
            do_GetInterface(GetAsSupports(this));
        if (!newDocument)
            return NS_ERROR_DOM_SECURITY_ERR;
        nsCOMPtr<nsIPrincipal> newPrincipal = newDocument->NodePrincipal();

        bool principalsEqual = false;
        origPrincipal->Equals(newPrincipal, &principalsEqual);
        NS_ENSURE_TRUE(principalsEqual, NS_ERROR_DOM_SECURITY_ERR);
    }

    // Check that the state object isn't too long.
    // Default max length: 640k bytes.
    PRInt32 maxStateObjSize =
        Preferences::GetInt("browser.history.maxStateObjectSize", 0xA0000);
    if (maxStateObjSize < 0) {
        maxStateObjSize = 0;
    }

    PRUint64 scSize;
    rv = scContainer->GetSerializedNBytes(&scSize);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(scSize <= (PRUint32)maxStateObjSize,
                   NS_ERROR_ILLEGAL_VALUE);

    // Step 2: Resolve aURL
    bool equalURIs = true;
    nsCOMPtr<nsIURI> oldURI = mCurrentURI;
    nsCOMPtr<nsIURI> newURI;
    if (aURL.Length() == 0) {
        newURI = mCurrentURI;
    }
    else {
        // 2a: Resolve aURL relative to mURI

        nsIURI* docBaseURI = document->GetDocBaseURI();
        if (!docBaseURI)
            return NS_ERROR_FAILURE;

        nsCAutoString spec;
        docBaseURI->GetSpec(spec);

        nsCAutoString charset;
        rv = docBaseURI->GetOriginCharset(charset);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        rv = NS_NewURI(getter_AddRefs(newURI), aURL,
                       charset.get(), docBaseURI);

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
            // |http://me@foo.com| have the same origin.)  mCurrentURI
            // won't contain the password part of the userpass, so this
            // means that it's never valid to specify a password in a
            // pushState or replaceState URI.

            nsCOMPtr<nsIScriptSecurityManager> secMan =
                do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
            NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

            // It's very important that we check that newURI is of the same
            // origin as mCurrentURI, not docBaseURI, because a page can
            // set docBaseURI arbitrarily to any domain.
            nsCAutoString currentUserPass, newUserPass;
            NS_ENSURE_SUCCESS(mCurrentURI->GetUserPass(currentUserPass),
                              NS_ERROR_FAILURE);
            NS_ENSURE_SUCCESS(newURI->GetUserPass(newUserPass),
                              NS_ERROR_FAILURE);
            if (NS_FAILED(secMan->CheckSameOriginURI(mCurrentURI,
                                                     newURI, true)) ||
                !currentUserPass.Equals(newUserPass)) {

                return NS_ERROR_DOM_SECURITY_ERR;
            }
        }
        else {
            // It's a file:// URI
            nsCOMPtr<nsIScriptObjectPrincipal> docScriptObj =
                do_QueryInterface(document);

            if (!docScriptObj) {
                return NS_ERROR_DOM_SECURITY_ERR;
            }

            nsCOMPtr<nsIPrincipal> principal = docScriptObj->GetPrincipal();

            if (!principal ||
                NS_FAILED(principal->CheckMayLoad(newURI, true))) {

                return NS_ERROR_DOM_SECURITY_ERR;
            }
        }

        if (mCurrentURI) {
            mCurrentURI->Equals(newURI, &equalURIs);
        }
        else {
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
        rv = AddToSessionHistory(newURI, nsnull, nsnull, true,
                                 getter_AddRefs(newSHEntry));
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ENSURE_TRUE(newSHEntry, NS_ERROR_FAILURE);

        // Link the new SHEntry to the old SHEntry's BFCache entry, since the
        // two entries correspond to the same document.
        NS_ENSURE_SUCCESS(newSHEntry->AdoptBFCacheEntry(oldOSHE),
                          NS_ERROR_FAILURE);

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
    }

    // Step 4: Modify new/original session history entry and clear its POST
    // data, if there is any.
    newSHEntry->SetStateData(scContainer);
    newSHEntry->SetPostData(nsnull);

    // If this push/replaceState changed the document's current URI and the new
    // URI differs from the old URI in more than the hash, or if the old
    // SHEntry's URI was modified in this way by a push/replaceState call
    // set URIWasModified to true for the current SHEntry (bug 669671).
    bool sameExceptHashes = true, oldURIWasModified = false;
    newURI->EqualsExceptRef(mCurrentURI, &sameExceptHashes);
    oldOSHE->GetURIWasModified(&oldURIWasModified);
    newSHEntry->SetURIWasModified(!sameExceptHashes || oldURIWasModified);

    // Step 5: If aReplace is false, indicating that we're doing a pushState
    // rather than a replaceState, notify bfcache that we've added a page to
    // the history so it can evict content viewers if appropriate.
    if (!aReplace) {
        nsCOMPtr<nsISHistory> rootSH;
        GetRootSessionHistory(getter_AddRefs(rootSH));
        NS_ENSURE_TRUE(rootSH, NS_ERROR_UNEXPECTED);

        nsCOMPtr<nsISHistoryInternal> internalSH =
            do_QueryInterface(rootSH);
        NS_ENSURE_TRUE(internalSH, NS_ERROR_UNEXPECTED);

        PRInt32 curIndex = -1;
        rv = rootSH->GetIndex(&curIndex);
        if (NS_SUCCEEDED(rv) && curIndex > -1) {
            internalSH->EvictOutOfRangeContentViewers(curIndex);
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
    // nsnull for aRequest param to FireOnLocationChange(...). Such an update
    // notification is allowed only when we know docshell is not loading a new
    // document and it requires LOCATION_CHANGE_SAME_DOCUMENT flag. Otherwise,
    // FireOnLocationChange(...) breaks security UI.
    if (!equalURIs) {
        SetCurrentURI(newURI, nsnull, true, LOCATION_CHANGE_SAME_DOCUMENT);
        document->SetDocumentURI(newURI);

        AddURIVisit(newURI, oldURI, oldURI, 0);

        // AddURIVisit doesn't set the title for the new URI in global history,
        // so do that here.
        if (mUseGlobalHistory) {
            nsCOMPtr<IHistory> history = services::GetHistoryService();
            if (history) {
                history->SetURITitle(newURI, mTitle);
            }
            else if (mGlobalHistory) {
                mGlobalHistory->SetPageTitle(newURI, mTitle);
            }
        }

        // Inform the favicon service that our old favicon applies to this new
        // URI.
        CopyFavicon(oldURI, newURI);
    }
    else {
        FireDummyOnLocationChange();
    }
    document->SetStateObject(scContainer);

    return NS_OK;
}

bool
nsDocShell::ShouldAddToSessionHistory(nsIURI * aURI)
{
    // I believe none of the about: urls should go in the history. But then
    // that could just be me... If the intent is only deny about:blank then we
    // should just do a spec compare, rather than two gets of the scheme and
    // then the path.  -Gagan
    nsresult rv;
    nsCAutoString buf, pref;

    rv = aURI->GetScheme(buf);
    if (NS_FAILED(rv))
        return false;

    if (buf.Equals("about")) {
        rv = aURI->GetPath(buf);
        if (NS_FAILED(rv))
            return false;

        if (buf.Equals("blank")) {
            return false;
        }
    }

    rv = aURI->GetSpec(buf);
    NS_ENSURE_SUCCESS(rv, true);

    rv = Preferences::GetDefaultCString("browser.newtab.url", &pref);
    NS_ENSURE_SUCCESS(rv, true);

    return !buf.Equals(pref);
}

nsresult
nsDocShell::AddToSessionHistory(nsIURI * aURI, nsIChannel * aChannel,
                                nsISupports* aOwner, bool aCloneChildren,
                                nsISHEntry ** aNewEntry)
{
    NS_PRECONDITION(aURI, "uri is null");
    NS_PRECONDITION(!aChannel || !aOwner, "Shouldn't have both set");

#if defined(PR_LOGGING) && defined(DEBUG)
    if (PR_LOG_TEST(gDocShellLog, PR_LOG_DEBUG)) {
        nsCAutoString spec;
        aURI->GetSpec(spec);

        nsCAutoString chanName;
        if (aChannel)
            aChannel->GetName(chanName);
        else
            chanName.AssignLiteral("<no channel>");

        PR_LOG(gDocShellLog, PR_LOG_DEBUG,
               ("nsDocShell[%p]::AddToSessionHistory(\"%s\", [%s])\n", this, spec.get(),
                chanName.get()));
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
        root != static_cast<nsIDocShellTreeItem *>(this)) {
        // This is a subframe 
        entry = mOSHE;
        nsCOMPtr<nsISHContainer> shContainer(do_QueryInterface(entry));
        if (shContainer) {
            PRInt32 childCount = 0;
            shContainer->GetChildCount(&childCount);
            // Remove all children of this entry 
            for (PRInt32 i = childCount - 1; i >= 0; i--) {
                nsCOMPtr<nsISHEntry> child;
                shContainer->GetChildAt(i, getter_AddRefs(child));
                shContainer->RemoveChild(child);
            }  // for
        }  // shContainer
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
    nsCOMPtr<nsIURI> referrerURI;
    nsCOMPtr<nsISupports> cacheKey;
    nsCOMPtr<nsISupports> owner = aOwner;
    bool expired = false;
    bool discardLayoutState = false;
    nsCOMPtr<nsICachingChannel> cacheChannel;
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
            httpChannel->GetReferrer(getter_AddRefs(referrerURI));

            discardLayoutState = ShouldDiscardLayoutState(httpChannel);
        }
        aChannel->GetOwner(getter_AddRefs(owner));
    }

    //Title is set in nsDocShell::SetTitle()
    entry->Create(aURI,              // uri
                  EmptyString(),     // Title
                  inputStream,       // Post data stream
                  nsnull,            // LayoutHistory state
                  cacheKey,          // CacheKey
                  mContentTypeHint,  // Content-type
                  owner,             // Channel or provided owner
                  mHistoryID,
                  mDynamicallyCreated);
    entry->SetReferrerURI(referrerURI);
    /* If cache got a 'no-store', ask SH not to store
     * HistoryLayoutState. By default, SH will set this
     * flag to true and save HistoryLayoutState.
     */    
    if (discardLayoutState) {
        entry->SetSaveLayoutStateFlag(false);
    }
    if (cacheChannel) {
        // Check if the page has expired from cache
        PRUint32 expTime = 0;
        cacheChannel->GetCacheTokenExpirationTime(&expTime);
        PRUint32 now = PRTimeToSeconds(PR_Now());
        if (expTime <=  now)
            expired = true;
    }
    if (expired)
        entry->SetExpirationStatus(true);


    if (root == static_cast<nsIDocShellTreeItem *>(this) && mSessionHistory) {
        // If we need to clone our children onto the new session
        // history entry, do so now.
        if (aCloneChildren && mOSHE) {
            PRUint32 cloneID;
            mOSHE->GetID(&cloneID);
            nsCOMPtr<nsISHEntry> newEntry;
            CloneAndReplace(mOSHE, this, cloneID, entry, true, getter_AddRefs(newEntry));
            NS_ASSERTION(entry == newEntry, "The new session history should be in the new entry");
        }

        // This is the root docshell
        if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {            
            // Replace current entry in session history.
            PRInt32  index = 0;   
            mSessionHistory->GetIndex(&index);
            nsCOMPtr<nsISHistoryInternal>   shPrivate(do_QueryInterface(mSessionHistory));
            // Replace the current entry with the new entry
            if (shPrivate)
                rv = shPrivate->ReplaceEntry(index, entry);          
        }
        else {
            // Add to session history
            nsCOMPtr<nsISHistoryInternal>
                shPrivate(do_QueryInterface(mSessionHistory));
            NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
            mSessionHistory->GetIndex(&mPreviousTransIndex);
            rv = shPrivate->AddEntry(entry, shouldPersist);
            mSessionHistory->GetIndex(&mLoadedTransIndex);
#ifdef DEBUG_PAGE_CACHE
            printf("Previous index: %d, Loaded index: %d\n\n",
                   mPreviousTransIndex, mLoadedTransIndex);
#endif
        }
    }
    else {  
        // This is a subframe.
        if (!mOSHE || !LOAD_TYPE_HAS_FLAGS(mLoadType,
                                           LOAD_FLAGS_REPLACE_HISTORY))
            rv = DoAddChildSHEntry(entry, mChildOffset, aCloneChildren);
    }

    // Return the new SH entry...
    if (aNewEntry) {
        *aNewEntry = nsnull;
        if (NS_SUCCEEDED(rv)) {
            *aNewEntry = entry;
            NS_ADDREF(*aNewEntry);
        }
    }

    return rv;
}


NS_IMETHODIMP
nsDocShell::LoadHistoryEntry(nsISHEntry * aEntry, PRUint32 aLoadType)
{
    if (!IsNavigationAllowed()) {
        return NS_OK;
    }
    
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIInputStream> postData;
    nsCOMPtr<nsIURI> referrerURI;
    nsCAutoString contentType;
    nsCOMPtr<nsISupports> owner;

    NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetReferrerURI(getter_AddRefs(referrerURI)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetPostData(getter_AddRefs(postData)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetContentType(contentType), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(aEntry->GetOwner(getter_AddRefs(owner)),
                      NS_ERROR_FAILURE);

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
        rv = CreateAboutBlankContentViewer(prin, nsnull, aEntry != mOSHE);

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
            owner = do_CreateInstance("@mozilla.org/nullprincipal;1");
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
      if (NS_FAILED(rv)) return rv;

      // If the user pressed cancel in the dialog, return.  We're done here.
      if (!repost)
        return NS_BINDING_ABORTED;
    }

    rv = InternalLoad(uri,
                      referrerURI,
                      owner,
                      INTERNAL_LOAD_FLAGS_NONE, // Do not inherit owner from document (security-critical!)
                      nsnull,            // No window target
                      contentType.get(), // Type hint
                      postData,          // Post data stream
                      nsnull,            // No headers stream
                      aLoadType,         // Load type
                      aEntry,            // SHEntry
                      true,
                      nsnull,            // No nsIDocShell
                      nsnull);           // No nsIRequest
    return rv;
}

NS_IMETHODIMP nsDocShell::GetShouldSaveLayoutState(bool* aShould)
{
    *aShould = false;
    if (mOSHE) {
        // Don't capture historystate and save it in history
        // if the page asked not to do so.
        mOSHE->GetSaveLayoutStateFlag(aShould);
    }

    return NS_OK;
}

NS_IMETHODIMP nsDocShell::PersistLayoutHistoryState()
{
    nsresult  rv = NS_OK;
    
    if (mOSHE) {
        nsCOMPtr<nsIPresShell> shell;
        rv = GetPresShell(getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv) && shell) {
            nsCOMPtr<nsILayoutHistoryState> layoutState;
            rv = shell->CaptureHistoryState(getter_AddRefs(layoutState),
                                            true);
        }
    }

    return rv;
}

/* static */ nsresult
nsDocShell::WalkHistoryEntries(nsISHEntry *aRootEntry,
                               nsDocShell *aRootShell,
                               WalkHistoryEntriesFunc aCallback,
                               void *aData)
{
    NS_ENSURE_TRUE(aRootEntry, NS_ERROR_FAILURE);

    nsCOMPtr<nsISHContainer> container(do_QueryInterface(aRootEntry));
    if (!container)
        return NS_ERROR_FAILURE;

    PRInt32 childCount;
    container->GetChildCount(&childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
        nsCOMPtr<nsISHEntry> childEntry;
        container->GetChildAt(i, getter_AddRefs(childEntry));
        if (!childEntry) {
            // childEntry can be null for valid reasons, for example if the
            // docshell at index i never loaded anything useful.
            // Remember to clone also nulls in the child array (bug 464064).
            aCallback(nsnull, nsnull, i, aData);
            continue;
        }

        nsDocShell *childShell = nsnull;
        if (aRootShell) {
            // Walk the children of aRootShell and see if one of them
            // has srcChild as a SHEntry.

            PRInt32 childCount = aRootShell->mChildList.Count();
            for (PRInt32 j = 0; j < childCount; ++j) {
                nsDocShell *child =
                    static_cast<nsDocShell*>(aRootShell->ChildAt(j));

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
struct NS_STACK_CLASS CloneAndReplaceData
{
    CloneAndReplaceData(PRUint32 aCloneID, nsISHEntry *aReplaceEntry,
                        bool aCloneChildren, nsISHEntry *aDestTreeParent)
        : cloneID(aCloneID),
          cloneChildren(aCloneChildren),
          replaceEntry(aReplaceEntry),
          destTreeParent(aDestTreeParent) { }

    PRUint32              cloneID;
    bool                  cloneChildren;
    nsISHEntry           *replaceEntry;
    nsISHEntry           *destTreeParent;
    nsCOMPtr<nsISHEntry>  resultEntry;
};

/* static */ nsresult
nsDocShell::CloneAndReplaceChild(nsISHEntry *aEntry, nsDocShell *aShell,
                                 PRInt32 aEntryIndex, void *aData)
{
    nsCOMPtr<nsISHEntry> dest;

    CloneAndReplaceData *data = static_cast<CloneAndReplaceData*>(aData);
    PRUint32 cloneID = data->cloneID;
    nsISHEntry *replaceEntry = data->replaceEntry;

    nsCOMPtr<nsISHContainer> container =
      do_QueryInterface(data->destTreeParent);
    if (!aEntry) {
        if (container) {
            container->AddChild(nsnull, aEntryIndex);
        }
        return NS_OK;
    }
    
    PRUint32 srcID;
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

    if (container)
        container->AddChild(dest, aEntryIndex);

    data->resultEntry = dest;
    return rv;
}

/* static */ nsresult
nsDocShell::CloneAndReplace(nsISHEntry *aSrcEntry,
                                   nsDocShell *aSrcShell,
                                   PRUint32 aCloneID,
                                   nsISHEntry *aReplaceEntry,
                                   bool aCloneChildren,
                                   nsISHEntry **aResultEntry)
{
    NS_ENSURE_ARG_POINTER(aResultEntry);
    NS_ENSURE_TRUE(aReplaceEntry, NS_ERROR_FAILURE);

    CloneAndReplaceData data(aCloneID, aReplaceEntry, aCloneChildren, nsnull);
    nsresult rv = CloneAndReplaceChild(aSrcEntry, aSrcShell, 0, &data);

    data.resultEntry.swap(*aResultEntry);
    return rv;
}

void
nsDocShell::SwapHistoryEntries(nsISHEntry *aOldEntry, nsISHEntry *aNewEntry)
{
    if (aOldEntry == mOSHE)
        mOSHE = aNewEntry;

    if (aOldEntry == mLSHE)
        mLSHE = aNewEntry;
}


struct SwapEntriesData
{
    nsDocShell *ignoreShell;     // constant; the shell to ignore
    nsISHEntry *destTreeRoot;    // constant; the root of the dest tree
    nsISHEntry *destTreeParent;  // constant; the node under destTreeRoot
                                 // whose children will correspond to aEntry
};


nsresult
nsDocShell::SetChildHistoryEntry(nsISHEntry *aEntry, nsDocShell *aShell,
                                 PRInt32 aEntryIndex, void *aData)
{
    SwapEntriesData *data = static_cast<SwapEntriesData*>(aData);
    nsDocShell *ignoreShell = data->ignoreShell;

    if (!aShell || aShell == ignoreShell)
        return NS_OK;

    nsISHEntry *destTreeRoot = data->destTreeRoot;

    nsCOMPtr<nsISHEntry> destEntry;
    nsCOMPtr<nsISHContainer> container =
        do_QueryInterface(data->destTreeParent);

    if (container) {
        // aEntry is a clone of some child of destTreeParent, but since the
        // trees aren't necessarily in sync, we'll have to locate it.
        // Note that we could set aShell's entry to null if we don't find a
        // corresponding entry under destTreeParent.

        PRUint32 targetID, id;
        aEntry->GetID(&targetID);

        // First look at the given index, since this is the common case.
        nsCOMPtr<nsISHEntry> entry;
        container->GetChildAt(aEntryIndex, getter_AddRefs(entry));
        if (entry && NS_SUCCEEDED(entry->GetID(&id)) && id == targetID) {
            destEntry.swap(entry);
        } else {
            PRInt32 childCount;
            container->GetChildCount(&childCount);
            for (PRInt32 i = 0; i < childCount; ++i) {
                container->GetChildAt(i, getter_AddRefs(entry));
                if (!entry)
                    continue;

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
    return WalkHistoryEntries(aEntry, aShell,
                              SetChildHistoryEntry, &childData);
}


static nsISHEntry*
GetRootSHEntry(nsISHEntry *aEntry)
{
    nsCOMPtr<nsISHEntry> rootEntry = aEntry;
    nsISHEntry *result = nsnull;
    while (rootEntry) {
        result = rootEntry;
        result->GetParent(getter_AddRefs(rootEntry));
    }

    return result;
}


void
nsDocShell::SetHistoryEntry(nsCOMPtr<nsISHEntry> *aPtr, nsISHEntry *aEntry)
{
    // We need to sync up the docshell and session history trees for
    // subframe navigation.  If the load was in a subframe, we forward up to
    // the root docshell, which will then recursively sync up all docshells
    // to their corresponding entries in the new session history tree.
    // If we don't do this, then we can cache a content viewer on the wrong
    // cloned entry, and subsequently restore it at the wrong time.

    nsISHEntry *newRootEntry = GetRootSHEntry(aEntry);
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
                nsIDocShell *rootIDocShell =
                    static_cast<nsIDocShell*>(rootShell);
                nsDocShell *rootDocShell = static_cast<nsDocShell*>
                                                      (rootIDocShell);

#ifdef DEBUG
                nsresult rv =
#endif
                SetChildHistoryEntry(oldRootEntry, rootDocShell,
                                                   0, &data);
                NS_ASSERTION(NS_SUCCEEDED(rv), "SetChildHistoryEntry failed");
            }
        }
    }

    *aPtr = aEntry;
}


nsresult
nsDocShell::GetRootSessionHistory(nsISHistory ** aReturn)
{
    nsresult rv;

    nsCOMPtr<nsIDocShellTreeItem> root;
    //Get the root docshell
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
nsDocShell::GetHttpChannel(nsIChannel * aChannel, nsIHttpChannel ** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);
    if (!aChannel)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIMultiPartChannel>  multiPartChannel(do_QueryInterface(aChannel));
    if (multiPartChannel) {
        nsCOMPtr<nsIChannel> baseChannel;
        multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));
        nsCOMPtr<nsIHttpChannel>  httpChannel(do_QueryInterface(baseChannel));
        *aReturn = httpChannel;
        NS_IF_ADDREF(*aReturn);
    }
    return NS_OK;
}

bool 
nsDocShell::ShouldDiscardLayoutState(nsIHttpChannel * aChannel)
{    
    // By default layout State will be saved. 
    if (!aChannel)
        return false;

    // figure out if SH should be saving layout state 
    nsCOMPtr<nsISupports> securityInfo;
    bool noStore = false, noCache = false;
    aChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
    aChannel->IsNoStoreResponse(&noStore);
    aChannel->IsNoCacheResponse(&noCache);

    return (noStore || (noCache && securityInfo));
}

//*****************************************************************************
// nsDocShell: nsIEditorDocShell
//*****************************************************************************   

NS_IMETHODIMP nsDocShell::GetEditor(nsIEditor * *aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  if (!mEditorData) {
    *aEditor = nsnull;
    return NS_OK;
  }

  return mEditorData->GetEditor(aEditor);
}

NS_IMETHODIMP nsDocShell::SetEditor(nsIEditor * aEditor)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) return rv;

  return mEditorData->SetEditor(aEditor);
}


NS_IMETHODIMP nsDocShell::GetEditable(bool *aEditable)
{
  NS_ENSURE_ARG_POINTER(aEditable);
  *aEditable = mEditorData && mEditorData->GetEditable();
  return NS_OK;
}


NS_IMETHODIMP nsDocShell::GetHasEditingSession(bool *aHasEditingSession)
{
  NS_ENSURE_ARG_POINTER(aHasEditingSession);
  
  if (mEditorData)
  {
    nsCOMPtr<nsIEditingSession> editingSession;
    mEditorData->GetEditingSession(getter_AddRefs(editingSession));
    *aHasEditingSession = (editingSession.get() != nsnull);
  }
  else
  {
    *aHasEditingSession = false;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocShell::MakeEditable(bool inWaitForUriLoad)
{
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) return rv;

  return mEditorData->MakeEditable(inWaitForUriLoad);
}

bool
nsDocShell::ChannelIsPost(nsIChannel* aChannel)
{
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
    if (!httpChannel) {
        return false;
    }

    nsCAutoString method;
    httpChannel->GetRequestMethod(method);
    return method.Equals("POST");
}

void
nsDocShell::ExtractLastVisit(nsIChannel* aChannel,
                             nsIURI** aURI,
                             PRUint32* aChannelRedirectFlags)
{
    nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(aChannel));
    if (!props) {
        return;
    }

    nsresult rv = props->GetPropertyAsInterface(
        NS_LITERAL_STRING("docshell.previousURI"),
        NS_GET_IID(nsIURI),
        reinterpret_cast<void**>(aURI)
    );

    if (NS_FAILED(rv)) {
        // There is no last visit for this channel, so this must be the first
        // link.  Link the visit to the referrer of this request, if any.
        // Treat referrer as null if there is an error getting it.
        (void)NS_GetReferrerFromChannel(aChannel, aURI);
    }
    else {
      rv = props->GetPropertyAsUint32(
          NS_LITERAL_STRING("docshell.previousFlags"),
          aChannelRedirectFlags
      );

      NS_WARN_IF_FALSE(
          NS_SUCCEEDED(rv),
          "Could not fetch previous flags, URI will be treated like referrer"
      );
    }
}

void
nsDocShell::SaveLastVisit(nsIChannel* aChannel,
                          nsIURI* aURI,
                          PRUint32 aChannelRedirectFlags)
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
                        PRUint32 aChannelRedirectFlags,
                        PRUint32 aResponseStatus)
{
    MOZ_ASSERT(aURI, "Visited URI is null!");
    MOZ_ASSERT(mLoadType != LOAD_ERROR_PAGE &&
               mLoadType != LOAD_BYPASS_HISTORY,
               "Do not add error or bypass pages to global history");

    // Only content-type docshells save URI visits.  Also don't do
    // anything here if we're not supposed to use global history.
    if (mItemType != typeContent || !mUseGlobalHistory) {
        return;
    }

    nsCOMPtr<IHistory> history = services::GetHistoryService();

    if (history) {
        PRUint32 visitURIFlags = 0;

        if (!IsFrame()) {
            visitURIFlags |= IHistory::TOP_LEVEL;
        }

        if (aChannelRedirectFlags & nsIChannelEventSink::REDIRECT_TEMPORARY) {
            visitURIFlags |= IHistory::REDIRECT_TEMPORARY;
        }
        else if (aChannelRedirectFlags &
                 nsIChannelEventSink::REDIRECT_PERMANENT) {
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
                 (aResponseStatus >= 400 && aResponseStatus <= 501 ||
                  aResponseStatus == 505)) {
            visitURIFlags |= IHistory::UNRECOVERABLE_ERROR;
        }

        (void)history->VisitURI(aURI, aPreviousURI, visitURIFlags);
    }
    else if (mGlobalHistory) {
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
nsDocShell::SetLoadType(PRUint32 aLoadType)
{
    mLoadType = aLoadType;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadType(PRUint32 * aLoadType)
{
    *aLoadType = mLoadType;
    return NS_OK;
}

nsresult
nsDocShell::ConfirmRepost(bool * aRepost)
{
  nsCOMPtr<nsIPrompt> prompter;
  CallGetInterface(this, static_cast<nsIPrompt**>(getter_AddRefs(prompter)));
  if (!prompter) {
      return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (!stringBundleService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> appBundle;
  nsresult rv = stringBundleService->CreateBundle(kAppstringsBundleURL,
                                                  getter_AddRefs(appBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> brandBundle;
  rv = stringBundleService->CreateBundle(kBrandBundleURL, getter_AddRefs(brandBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(prompter && brandBundle && appBundle,
               "Unable to set up repost prompter.");

  nsXPIDLString brandName;
  rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                      getter_Copies(brandName));

  nsXPIDLString msgString, button0Title;
  if (NS_FAILED(rv)) { // No brand, use the generic version.
    rv = appBundle->GetStringFromName(NS_LITERAL_STRING("confirmRepostPrompt").get(),
                                      getter_Copies(msgString));
  }
  else {
    // Brand available - if the app has an override file with formatting, the app name will
    // be included. Without an override, the prompt will look like the generic version.
    const PRUnichar *formatStrings[] = { brandName.get() };
    rv = appBundle->FormatStringFromName(NS_LITERAL_STRING("confirmRepostPrompt").get(),
                                         formatStrings, ArrayLength(formatStrings),
                                         getter_Copies(msgString));
  }
  if (NS_FAILED(rv)) return rv;

  rv = appBundle->GetStringFromName(NS_LITERAL_STRING("resendButton.label").get(),
                                    getter_Copies(button0Title));
  if (NS_FAILED(rv)) return rv;

  PRInt32 buttonPressed;
  // The actual value here is irrelevant, but we can't pass an invalid
  // bool through XPConnect.
  bool checkState = false;
  rv = prompter->
         ConfirmEx(nsnull, msgString.get(),
                   (nsIPrompt::BUTTON_POS_0 * nsIPrompt::BUTTON_TITLE_IS_STRING) +
                   (nsIPrompt::BUTTON_POS_1 * nsIPrompt::BUTTON_TITLE_CANCEL),
                   button0Title.get(), nsnull, nsnull, nsnull, &checkState, &buttonPressed);
  if (NS_FAILED(rv)) return rv;

  *aRepost = (buttonPressed == 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPromptAndStringBundle(nsIPrompt ** aPrompt,
                                     nsIStringBundle ** aStringBundle)
{
    NS_ENSURE_SUCCESS(GetInterface(NS_GET_IID(nsIPrompt), (void **) aPrompt),
                      NS_ERROR_FAILURE);

    nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(stringBundleService, NS_ERROR_FAILURE);

    NS_ENSURE_SUCCESS(stringBundleService->
                      CreateBundle(kAppstringsBundleURL,
                                   aStringBundle),
                      NS_ERROR_FAILURE);

    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildOffset(nsIDOMNode * aChild, nsIDOMNode * aParent,
                           PRInt32 * aOffset)
{
    NS_ENSURE_ARG_POINTER(aChild || aParent);

    nsCOMPtr<nsIDOMNodeList> childNodes;
    NS_ENSURE_SUCCESS(aParent->GetChildNodes(getter_AddRefs(childNodes)),
                      NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(childNodes, NS_ERROR_FAILURE);

    PRInt32 i = 0;

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

nsIScrollableFrame *
nsDocShell::GetRootScrollFrame()
{
    nsCOMPtr<nsIPresShell> shell;
    NS_ENSURE_SUCCESS(GetPresShell(getter_AddRefs(shell)), nsnull);
    NS_ENSURE_TRUE(shell, nsnull);

    return shell->GetRootScrollFrameAsScrollableExternal();
}

NS_IMETHODIMP
nsDocShell::EnsureScriptEnvironment()
{
    if (mScriptGlobal)
        return NS_OK;

    if (mIsBeingDestroyed) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    NS_TIME_FUNCTION;

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

    PRUint32 chromeFlags;
    browserChrome->GetChromeFlags(&chromeFlags);

    bool isModalContentWindow =
        (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL) &&
        !(chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME);

    // If our window is modal and we're not opened as chrome, make
    // this window a modal content window.
    nsRefPtr<nsGlobalWindow> window =
        NS_NewScriptGlobalObject(mItemType == typeChrome, isModalContentWindow);
    MOZ_ASSERT(window);
    mScriptGlobal = window;

    window->SetDocShell(this);

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
        if (!mTransferableHookData) return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}


NS_IMETHODIMP nsDocShell::EnsureFind()
{
    nsresult rv;
    if (!mFind)
    {
        mFind = do_CreateInstance("@mozilla.org/embedcomp/find;1", &rv);
        if (NS_FAILED(rv)) return rv;
    }
    
    // we promise that the nsIWebBrowserFind that we return has been set
    // up to point to the focused, or content window, so we have to
    // set that up each time.

    nsIScriptGlobalObject* scriptGO = GetScriptGlobalObject();
    NS_ENSURE_TRUE(scriptGO, NS_ERROR_UNEXPECTED);

    // default to our window
    nsCOMPtr<nsIDOMWindow> windowToSearch(do_QueryInterface(mScriptGlobal));

    nsCOMPtr<nsIDocShellTreeItem> root;
    GetRootTreeItem(getter_AddRefs(root));

    // if the active window is the same window that this docshell is in,
    // use the currently focused frame
    nsCOMPtr<nsIDOMWindow> rootWindow = do_GetInterface(root);
    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    if (fm) {
      nsCOMPtr<nsIDOMWindow> activeWindow;
      fm->GetActiveWindow(getter_AddRefs(activeWindow));
      if (activeWindow == rootWindow)
        fm->GetFocusedWindow(getter_AddRefs(windowToSearch));
    }

    nsCOMPtr<nsIWebBrowserFindInFrames> findInFrames = do_QueryInterface(mFind);
    if (!findInFrames) return NS_ERROR_NO_INTERFACE;
    
    rv = findInFrames->SetRootSearchFrame(rootWindow);
    if (NS_FAILED(rv)) return rv;
    rv = findInFrames->SetCurrentSearchFrame(windowToSearch);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

bool
nsDocShell::IsFrame()
{
    nsCOMPtr<nsIDocShellTreeItem> parent =
        do_QueryInterface(GetAsSupports(mParent));
    if (parent) {
        PRInt32 parentType = ~mItemType;        // Not us
        parent->GetItemType(&parentType);
        if (parentType == mItemType)    // This is a frame
            return true;
    }

    return false;
}

/* boolean IsBeingDestroyed (); */
NS_IMETHODIMP 
nsDocShell::IsBeingDestroyed(bool *aDoomed)
{
  NS_ENSURE_ARG(aDoomed);
  *aDoomed = mIsBeingDestroyed;
  return NS_OK;
}


NS_IMETHODIMP 
nsDocShell::GetIsExecutingOnLoadHandler(bool *aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = mIsExecutingOnLoadHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLayoutHistoryState(nsILayoutHistoryState **aLayoutHistoryState)
{
  if (mOSHE)
    mOSHE->GetLayoutHistoryState(aLayoutHistoryState);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLayoutHistoryState(nsILayoutHistoryState *aLayoutHistoryState)
{
  if (mOSHE)
    mOSHE->SetLayoutHistoryState(aLayoutHistoryState);
  return NS_OK;
}

//*****************************************************************************
//***    nsRefreshTimer: Object Management
//*****************************************************************************

nsRefreshTimer::nsRefreshTimer()
    : mDelay(0), mRepeat(false), mMetaRefresh(false)
{
}

nsRefreshTimer::~nsRefreshTimer()
{
}

//*****************************************************************************
// nsRefreshTimer::nsISupports
//*****************************************************************************   

NS_IMPL_THREADSAFE_ADDREF(nsRefreshTimer)
NS_IMPL_THREADSAFE_RELEASE(nsRefreshTimer)

NS_INTERFACE_MAP_BEGIN(nsRefreshTimer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
    NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_THREADSAFE

///*****************************************************************************
// nsRefreshTimer::nsITimerCallback
//******************************************************************************
NS_IMETHODIMP
nsRefreshTimer::Notify(nsITimer * aTimer)
{
    NS_ASSERTION(mDocShell, "DocShell is somehow null");

    if (mDocShell && aTimer) {
        // Get the delay count to determine load type
        PRUint32 delay = 0;
        aTimer->GetDelay(&delay);
        mDocShell->ForceRefreshURIFromTimer(mURI, delay, mMetaRefresh, aTimer);
    }
    return NS_OK;
}

//*****************************************************************************
// nsDocShell::InterfaceRequestorProxy
//*****************************************************************************
nsDocShell::InterfaceRequestorProxy::InterfaceRequestorProxy(nsIInterfaceRequestor* p)
{
    if (p) {
        mWeakPtr = do_GetWeakReference(p);
    }
}
 
nsDocShell::InterfaceRequestorProxy::~InterfaceRequestorProxy()
{
    mWeakPtr = nsnull;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDocShell::InterfaceRequestorProxy, nsIInterfaceRequestor) 
  
NS_IMETHODIMP 
nsDocShell::InterfaceRequestorProxy::GetInterface(const nsIID & aIID, void **aSink)
{
    NS_ENSURE_ARG_POINTER(aSink);
    nsCOMPtr<nsIInterfaceRequestor> ifReq = do_QueryReferent(mWeakPtr);
    if (ifReq) {
        return ifReq->GetInterface(aIID, aSink);
    }
    *aSink = nsnull;
    return NS_NOINTERFACE;
}

nsresult
nsDocShell::SetBaseUrlForWyciwyg(nsIContentViewer * aContentViewer)
{
    if (!aContentViewer)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIURI> baseURI;
    nsresult rv = NS_ERROR_NOT_AVAILABLE;

    if (sURIFixup)
        rv = sURIFixup->CreateExposableURI(mCurrentURI,
                                           getter_AddRefs(baseURI));

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
nsDocShell::GetAuthPrompt(PRUint32 aPromptReason, const nsIID& iid,
                          void** aResult)
{
    // a priority prompt request will override a false mAllowAuth setting
    bool priorityPrompt = (aPromptReason == PROMPT_PROXY);

    if (!mAllowAuth && !priorityPrompt)
        return NS_ERROR_NOT_AVAILABLE;

    // we're either allowing auth, or it's a proxy request
    nsresult rv;
    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = EnsureScriptEnvironment();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(mScriptGlobal));

    // Get the an auth prompter for our window so that the parenting
    // of the dialogs works as it should when using tabs.

    return wwatch->GetPrompt(window, iid,
                             reinterpret_cast<void**>(aResult));
}

//*****************************************************************************
// nsDocShell::nsIObserver
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::Observe(nsISupports *aSubject, const char *aTopic,
                    const PRUnichar *aData)
{
    nsresult rv = NS_OK;
    if (mObserveErrorPages &&
        !nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) &&
        !nsCRT::strcmp(aData,
          NS_LITERAL_STRING("browser.xul.error_pages.enabled").get())) {

        bool tmpbool;
        rv = Preferences::GetBool("browser.xul.error_pages.enabled", &tmpbool);
        if (NS_SUCCEEDED(rv))
            mUseErrorPages = tmpbool;

    } else {
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
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
    nsCOMPtr<nsIDOMWindow> win = do_GetInterface(GetAsSupports(this));
    if (win) {
        win->GetTop(aWindow);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::IsAppOfType(PRUint32 aAppType, bool *aIsOfType)
{
    nsCOMPtr<nsIDocShell> shell = this;
    while (shell) {
        PRUint32 type;
        shell->GetAppType(&type);
        if (type == aAppType) {
            *aIsOfType = true;
            return NS_OK;
        }
        nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(shell);
        nsCOMPtr<nsIDocShellTreeItem> parent;
        item->GetParent(getter_AddRefs(parent));
        shell = do_QueryInterface(parent);
    }

    *aIsOfType = false;
    return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsContent(bool *aIsContent)
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
    return
        secMan &&
        NS_SUCCEEDED(secMan->CheckSameOriginURI(aURI, mLoadingURI, false));
}

//
// Routines for selection and clipboard
//
nsresult
nsDocShell::GetControllerForCommand(const char * inCommand,
                                    nsIController** outController)
{
  NS_ENSURE_ARG_POINTER(outController);
  *outController = nsnull;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mScriptGlobal));
  if (window) {
      nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
      if (root) {
          return root->GetControllerForCommand(inCommand, outController);
      }
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsDocShell::IsCommandEnabled(const char * inCommand, bool* outEnabled)
{
  NS_ENSURE_ARG_POINTER(outEnabled);
  *outEnabled = false;

  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand (inCommand, getter_AddRefs(controller));
  if (controller)
    rv = controller->IsCommandEnabled(inCommand, outEnabled);
  
  return rv;
}

nsresult
nsDocShell::DoCommand(const char * inCommand)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand(inCommand, getter_AddRefs(controller));
  if (controller)
    rv = controller->DoCommand(inCommand);
  
  return rv;
}

nsresult
nsDocShell::EnsureCommandHandler()
{
  if (!mCommandManager)
  {
    nsCOMPtr<nsPICommandUpdater> commandUpdater =
      do_CreateInstance("@mozilla.org/embedcomp/command-manager;1");
    if (!commandUpdater) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCOMPtr<nsIDOMWindow> domWindow =
      do_GetInterface(static_cast<nsIInterfaceRequestor *>(this));

    nsresult rv = commandUpdater->Init(domWindow);
    if (NS_SUCCEEDED(rv))
      mCommandManager = do_QueryInterface(commandUpdater);
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
  return IsCommandEnabled("cmd_copyImageLocation",
                          aResult);
}

NS_IMETHODIMP
nsDocShell::CanCopyImageContents(bool* aResult)
{
  return IsCommandEnabled("cmd_copyImageContents",
                          aResult);
}

NS_IMETHODIMP
nsDocShell::CanPaste(bool* aResult)
{
  return IsCommandEnabled("cmd_paste", aResult);
}

NS_IMETHODIMP
nsDocShell::CutSelection(void)
{
  return DoCommand ( "cmd_cut" );
}

NS_IMETHODIMP
nsDocShell::CopySelection(void)
{
  return DoCommand ( "cmd_copy" );
}

NS_IMETHODIMP
nsDocShell::CopyLinkLocation(void)
{
  return DoCommand ( "cmd_copyLink" );
}

NS_IMETHODIMP
nsDocShell::CopyImageLocation(void)
{
  return DoCommand ( "cmd_copyImageLocation" );
}

NS_IMETHODIMP
nsDocShell::CopyImageContents(void)
{
  return DoCommand ( "cmd_copyImageContents" );
}

NS_IMETHODIMP
nsDocShell::Paste(void)
{
  return DoCommand ( "cmd_paste" );
}

NS_IMETHODIMP
nsDocShell::SelectAll(void)
{
  return DoCommand ( "cmd_selectAll" );
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
  return DoCommand ( "cmd_selectNone" );
}

//----------------------------------------------------------------------

// link handling

class OnLinkClickEvent : public nsRunnable {
public:
  OnLinkClickEvent(nsDocShell* aHandler, nsIContent* aContent,
                   nsIURI* aURI,
                   const PRUnichar* aTargetSpec,
                   nsIInputStream* aPostDataStream, 
                   nsIInputStream* aHeadersDataStream,
                   bool aIsTrusted);

  NS_IMETHOD Run() {
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mHandler->mScriptGlobal));
    nsAutoPopupStatePusher popupStatePusher(window, mPopupState);

    nsCxPusher pusher;
    if (mIsTrusted || pusher.Push(mContent)) {
      mHandler->OnLinkClickSync(mContent, mURI,
                                mTargetSpec.get(), mPostDataStream,
                                mHeadersDataStream,
                                nsnull, nsnull);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsDocShell>     mHandler;
  nsCOMPtr<nsIURI>         mURI;
  nsString                 mTargetSpec;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  nsCOMPtr<nsIInputStream> mHeadersDataStream;
  nsCOMPtr<nsIContent>     mContent;
  PopupControlState        mPopupState;
  bool                     mIsTrusted;
};

OnLinkClickEvent::OnLinkClickEvent(nsDocShell* aHandler,
                                   nsIContent *aContent,
                                   nsIURI* aURI,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream,
                                   nsIInputStream* aHeadersDataStream,
                                   bool aIsTrusted)
  : mHandler(aHandler)
  , mURI(aURI)
  , mTargetSpec(aTargetSpec)
  , mPostDataStream(aPostDataStream)
  , mHeadersDataStream(aHeadersDataStream)
  , mContent(aContent)
  , mIsTrusted(aIsTrusted)
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mHandler->mScriptGlobal));

  mPopupState = window->GetPopupControlState();
}

//----------------------------------------

NS_IMETHODIMP
nsDocShell::OnLinkClick(nsIContent* aContent,
                        nsIURI* aURI,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream,
                        nsIInputStream* aHeadersDataStream,
                        bool aIsTrusted)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

  if (!IsOKToLoadURI(aURI)) {
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
  
  if (NS_FAILED(rv))
    target = aTargetSpec;  

  nsCOMPtr<nsIRunnable> ev =
      new OnLinkClickEvent(this, aContent, aURI, target.get(),
                           aPostDataStream, aHeadersDataStream, aIsTrusted);
  return NS_DispatchToCurrentThread(ev);
}

NS_IMETHODIMP
nsDocShell::OnLinkClickSync(nsIContent *aContent,
                            nsIURI* aURI,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream,
                            nsIInputStream* aHeadersDataStream,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest)
{
  // Initialize the DocShell / Request
  if (aDocShell) {
    *aDocShell = nsnull;
  }
  if (aRequest) {
    *aRequest = nsnull;
  }

  if (!IsOKToLoadURI(aURI)) {
    return NS_OK;
  }

  // XXX When the linking node was HTMLFormElement, it is synchronous event.
  //     That is, the caller of this method is not |OnLinkClickEvent::Run()|
  //     but |nsHTMLFormElement::SubmitSubmission(...)|.
  if (nsGkAtoms::form == aContent->Tag() && ShouldBlockLoadingForBackButton()) {
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
      nsCAutoString scheme;
      aURI->GetScheme(scheme);
      if (!scheme.IsEmpty()) {
        // if the URL scheme does not correspond to an exposed protocol, then we
        // need to hand this link click over to the external protocol handler.
        bool isExposed;
        nsresult rv = extProtService->IsExposedProtocol(scheme.get(), &isExposed);
        if (NS_SUCCEEDED(rv) && !isExposed) {
          return extProtService->LoadURI(aURI, this); 
        }
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
  nsCOMPtr<nsPIDOMWindow> outerWindow = do_QueryInterface(mScriptGlobal);
  if (!outerWindow || outerWindow->GetCurrentInnerWindow() != refererInner) {
      // We're no longer the current inner window
      return NS_OK;
  }

  nsCOMPtr<nsIURI> referer = refererDoc->GetDocumentURI();

  // referer could be null here in some odd cases, but that's ok,
  // we'll just load the link w/o sending a referer in those cases.

  nsAutoString target(aTargetSpec);

  // If this is an anchor element, grab its type property to use as a hint
  nsAutoString typeHint;
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(aContent));
  if (anchor) {
    anchor->GetType(typeHint);
    NS_ConvertUTF16toUTF8 utf8Hint(typeHint);
    nsCAutoString type, dummy;
    NS_ParseContentType(utf8Hint, type, dummy);
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
                             referer,                   // Referer URI
                             aContent->NodePrincipal(), // Owner is our node's
                                                        // principal
                             INTERNAL_LOAD_FLAGS_NONE,
                             target.get(),              // Window target
                             NS_LossyConvertUTF16toASCII(typeHint).get(),
                             aPostDataStream,           // Post data stream
                             aHeadersDataStream,        // Headers stream
                             LOAD_LINK,                 // Load type
                             nsnull,                    // No SHEntry
                             true,                   // first party site
                             aDocShell,                 // DocShell out-param
                             aRequest);                 // Request out-param
  if (NS_SUCCEEDED(rv)) {
    DispatchPings(aContent, referer);
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::OnOverLink(nsIContent* aContent,
                       nsIURI* aURI,
                       const PRUnichar* aTargetSpec)
{
  if (aContent->IsEditable()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebBrowserChrome2> browserChrome2 = do_GetInterface(mTreeOwner);
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  if (!browserChrome2) {
    browserChrome = do_GetInterface(mTreeOwner);
    if (!browserChrome)
      return rv;
  }

  nsCOMPtr<nsITextToSubURI> textToSubURI =
      do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // use url origin charset to unescape the URL
  nsCAutoString charset;
  rv = aURI->GetOriginCharset(charset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString uStr;
  rv = textToSubURI->UnEscapeURIForUI(charset, spec, uStr);    
  NS_ENSURE_SUCCESS(rv, rv);

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

  if (browserChrome)  {
      rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                                    EmptyString().get());
  }
  return rv;
}

bool
nsDocShell::ShouldBlockLoadingForBackButton()
{
  if (!(mLoadType & LOAD_CMD_HISTORY) ||
      nsEventStateManager::IsHandlingUserInput() ||
      !Preferences::GetBool("accessibility.blockjsredirection")) {
    return false;
  }

  bool canGoForward = false;
  GetCanGoForward(&canGoForward);
  return canGoForward;
}

//----------------------------------------------------------------------
// Web Shell Services API

//This functions is only called when a new charset is detected in loading a document. 
//Its name should be changed to "CharsetReloadDocument"
NS_IMETHODIMP
nsDocShell::ReloadDocument(const char* aCharset,
                           PRInt32 aSource)
{

  // XXX hack. keep the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      PRInt32 hint;
      muDV->GetHintCharacterSetSource(&hint);
      if (aSource > hint)
      {
        nsCString charset(aCharset);
        muDV->SetHintCharacterSet(charset);
        muDV->SetHintCharacterSetSource(aSource);
        if(eCharsetReloadRequested != mCharsetReloadState) 
        {
          mCharsetReloadState = eCharsetReloadRequested;
          return Reload(LOAD_FLAGS_CHARSET_CHANGE);
        }
      }
    }
  }
  //return failure if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}


NS_IMETHODIMP
nsDocShell::StopDocumentLoad(void)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    Stop(nsIWebNavigation::STOP_ALL);
    return NS_OK;
  }
  //return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}

NS_IMETHODIMP
nsDocShell::GetPrintPreview(nsIWebBrowserPrint** aPrintPreview)
{
  *aPrintPreview = nsnull;
#if NS_PRINT_PREVIEW
  nsCOMPtr<nsIDocumentViewerPrint> print = do_QueryInterface(mContentViewer);
  if (!print || !print->IsInitializedForPrintPreview()) {
    Stop(nsIWebNavigation::STOP_ALL);
    nsCOMPtr<nsIPrincipal> principal =
      do_CreateInstance("@mozilla.org/nullprincipal;1");
    NS_ENSURE_STATE(principal);
    nsresult rv = CreateAboutBlankContentViewer(principal, nsnull);
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
nsDocShell::GetCanExecuteScripts(bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false; // disallow by default

  nsCOMPtr<nsIDocShell> docshell = this;
  nsCOMPtr<nsIDocShellTreeItem> globalObjTreeItem =
      do_QueryInterface(docshell);

  if (globalObjTreeItem)
  {
      nsCOMPtr<nsIDocShellTreeItem> treeItem(globalObjTreeItem);
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      bool firstPass = true;
      bool lookForParents = false;

      // Walk up the docshell tree to see if any containing docshell disallows scripts
      do
      {
          nsresult rv = docshell->GetAllowJavascript(aResult);
          if (NS_FAILED(rv)) return rv;
          if (!*aResult) {
              nsDocShell* realDocshell = static_cast<nsDocShell*>(docshell.get());
              if (realDocshell->mContentViewer) {
                  nsIDocument* doc = realDocshell->mContentViewer->GetDocument();
                  if (doc && doc->HasFlag(NODE_IS_EDITABLE) &&
                      realDocshell->mEditorData) {
                      nsCOMPtr<nsIEditingSession> editSession;
                      realDocshell->mEditorData->GetEditingSession(getter_AddRefs(editSession));
                      bool jsDisabled = false;
                      if (editSession &&
                          NS_SUCCEEDED(rv = editSession->GetJsAndPluginsDisabled(&jsDisabled))) {
                          if (firstPass) {
                              if (jsDisabled) {
                                  // We have a docshell which has been explicitly set
                                  // to design mode, so we disallow scripts.
                                  return NS_OK;
                              }
                              // The docshell was not explicitly set to design mode,
                              // so it must be so because a parent was explicitly
                              // set to design mode.  We don't need to look at higher
                              // docshells.
                              *aResult = true;
                              break;
                          } else if (lookForParents && jsDisabled) {
                              // If a parent was explicitly set to design mode,
                              // we should allow script execution on the child.
                              *aResult = true;
                              break;
                          }
                          // If the child docshell allows scripting, and the
                          // parent is inside design mode, we don't need to look
                          // further.
                          *aResult = true;
                          return NS_OK;
                      }
                      NS_WARNING("The editing session does not work?");
                      return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
                  }
                  if (firstPass) {
                      // Don't be too hard on docshells on the first pass.
                      // There may be a parent docshell which has been set
                      // to design mode, so look for it.
                      lookForParents = true;
                  } else {
                      // We have a docshell which disallows scripts
                      // and is not editable, so we shouldn't allow
                      // scripts at all.
                      return NS_OK;
                  }
              }
          } else if (lookForParents) {
              // The parent docshell was not explicitly set to design
              // mode, so js on the child docshell was disabled for
              // another reason.  Therefore, we need to disable js.
              *aResult = false;
              return NS_OK;
          }
          firstPass = false;

          treeItem->GetParent(getter_AddRefs(parentItem));
          treeItem.swap(parentItem);
          docshell = do_QueryInterface(treeItem);
#ifdef DEBUG
          if (treeItem && !docshell) {
            NS_ERROR("cannot get a docshell from a treeItem!");
          }
#endif // DEBUG
      } while (treeItem && docshell);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsBrowserFrame(bool *aOut)
{
  NS_ENSURE_ARG_POINTER(aOut);
  *aOut = mIsBrowserFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsBrowserFrame(bool aValue)
{
  // Disallow transitions from browser frame to not-browser-frame.  Once a
  // browser frame, always a browser frame.  (Otherwise, observers of
  // docshell-marked-as-browser-frame would have to distinguish between
  // newly-created browser frames and frames which went from true to false back
  // to true.)
  NS_ENSURE_STATE(!mIsBrowserFrame || aValue);

  bool wasBrowserFrame = mIsBrowserFrame;
  mIsBrowserFrame = aValue;
  if (aValue && !wasBrowserFrame) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(GetAsSupports(this),
                          "docshell-marked-as-browser-frame", NULL);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetContainedInBrowserFrame(bool *aOut)
{
    *aOut = false;

    if (mIsBrowserFrame) {
        *aOut = true;
        return NS_OK;
    }

    nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
    GetSameTypeParent(getter_AddRefs(parentAsItem));

    nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
    if (parent) {
        return parent->GetContainedInBrowserFrame(aOut);
    }

    return NS_OK;
}
