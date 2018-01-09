/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShell.h"

#include <algorithm>

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Casting.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ClientHandle.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/PendingGlobalHistoryEntry.h"
#include "mozilla/dom/PerformanceNavigation.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/ToJSValue.h"

#include "mozilla/dom/workers/ServiceWorkerManager.h"

#include "mozilla/net/ReferrerPolicy.h"

#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIAppShell.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsICachingChannel.h"
#include "nsICaptivePortalService.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIClassOfService.h"
#include "nsICommandManager.h"
#include "nsIConsoleReportCollector.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentViewer.h"
#include "nsIController.h"
#include "nsICookieService.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMStorage.h"
#include "nsIDOMWindow.h"
#include "nsIEditingSession.h"
#include "nsIExternalProtocolService.h"
#include "nsIFormPOSTActionChannel.h"
#include "nsIFrame.h"
#include "nsIGlobalHistory2.h"
#include "nsIGlobalObject.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIDNService.h"
#include "nsIInputStreamChannel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIJARChannel.h"
#include "nsILayoutHistoryState.h"
#include "nsILoadInfo.h"
#include "nsIMultiPartChannel.h"
#include "nsINestedURI.h"
#include "nsINetworkPredictor.h"
#include "nsINode.h"
#include "nsINSSErrorsService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"
#include "nsIPrincipal.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsIPrompt.h"
#include "nsIPromptFactory.h"
#include "nsIReflowObserver.h"
#include "nsIScriptChannel.h"
#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollObserver.h"
#include "nsISecureBrowserUI.h"
#include "nsISecurityUITelemetry.h"
#include "nsISeekableStream.h"
#include "nsISelectionDisplay.h"
#include "nsISHContainer.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsISiteSecurityService.h"
#include "nsISocketProvider.h"
#include "nsIStringBundle.h"
#include "nsIStructuredCloneContainer.h"
#include "nsISupportsPrimitives.h"
#include "nsITabChild.h"
#include "nsITextToSubURI.h"
#include "nsITimedChannel.h"
#include "nsITimer.h"
#include "nsITransportSecurityInfo.h"
#include "nsIUploadChannel.h"
#include "nsIURIFixup.h"
#include "nsIURILoader.h"
#include "nsIURL.h"
#include "nsIViewSourceChannel.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChrome3.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebProgress.h"
#include "nsIWidget.h"
#include "nsIWindowWatcher.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIWyciwygChannel.h"

#include "nsPICommandUpdater.h"
#include "nsPIDOMWindow.h"
#include "nsPILoadGroupInternal.h"
#include "nsPIWindowRoot.h"

#include "IHistory.h"
#include "IUrlClassifierUITelemetry.h"

#include "mozIThirdPartyUtil.h"

#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsCDefaultURIFixup.h"
#include "nsCExternalHandlerService.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h" // NS_CheckContentLoadPolicy(...)
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsCURILoader.h"
#include "nsDocShellCID.h"
#include "nsDocShellEditorData.h"
#include "nsDocShellEnumerator.h"
#include "nsDocShellLoadInfo.h"
#include "nsDocShellLoadTypes.h"
#include "nsDocShellTransferableHooks.h"
#include "nsDOMCID.h"
#include "nsDOMNavigationTiming.h"
#include "nsDSURIContentListener.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsFocusManager.h"
#include "nsGlobalWindow.h"
#include "nsJSEnvironment.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsObjectLoadingContent.h"
#include "nsPingListener.h"
#include "nsPoint.h"
#include "nsQueryObject.h"
#include "nsRect.h"
#include "nsRefreshTimer.h"
#include "nsSandboxFlags.h"
#include "nsIServiceWorkerManager.h"
#include "nsSHistory.h"
#include "nsStructuredCloneContainer.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsViewSourceHandler.h"
#include "nsWhitespaceTokenizer.h"
#include "nsWidgetsCID.h"
#include "nsXULAppAPI.h"

#include "GeckoProfiler.h"
#include "Navigator.h"
#include "NullPrincipal.h"
#include "prenv.h"
#include "URIUtils.h"

#include "timeline/JavascriptTimelineMarker.h"

#ifdef MOZ_PLACES
#include "nsIFaviconService.h"
#include "mozIPlacesPendingOperation.h"
#include "mozIAsyncFavicons.h"
#endif

#if NS_PRINT_PREVIEW
#include "nsIDocumentViewerPrint.h"
#include "nsIWebBrowserPrint.h"
#endif

#ifdef MOZ_TOOLKIT_SEARCH
#include "nsIBrowserSearchService.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::workers::ServiceWorkerManager;

// Threshold value in ms for META refresh based redirects
#define REFRESH_REDIRECT_TIMER 15000

// Hint for native dispatch of events on how long to delay after
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
// Can be overridden with docshell.event_starvation_delay_hint pref.
#define NS_EVENT_STARVATION_DELAY_HINT 2000

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

// True means sUseErrorPages has been added to
// preferences var cache.
static bool gAddedPreferencesVarCache = false;

// Number of documents currently loading
static int32_t gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static int32_t gDocShellCount = 0;

// Global count of docshells with the private attribute set
static uint32_t gNumberOfPrivateDocShells = 0;

// True means we validate window targets to prevent frameset
// spoofing. Initialize this to a non-bolean value so we know to check
// the pref on the creation of the first docshell.
static uint32_t gValidateOrigin = 0xffffffff;

#ifdef DEBUG
static mozilla::LazyLogModule gDocShellLog("nsDocShell");
#endif
static mozilla::LazyLogModule gDocShellLeakLog("nsDocShellLeak");;

const char kBrandBundleURL[]      = "chrome://branding/locale/brand.properties";
const char kAppstringsBundleURL[] = "chrome://global/locale/appstrings.properties";

bool nsDocShell::sUseErrorPages = false;

// Global reference to the URI fixup service.
nsIURIFixup* nsDocShell::sURIFixup = nullptr;

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

nsDocShell::nsDocShell()
  : nsDocLoader()
  , mForcedCharset(nullptr)
  , mParentCharset(nullptr)
  , mTreeOwner(nullptr)
  , mChromeEventHandler(nullptr)
  , mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto)
  , mCharsetReloadState(eCharsetReloadInit)
  , mOrientationLock(eScreenOrientation_None)
  , mParentCharsetSource(0)
  , mMarginWidth(-1)
  , mMarginHeight(-1)
  , mItemType(typeContent)
  , mPreviousTransIndex(-1)
  , mLoadedTransIndex(-1)
  , mChildOffset(0)
  , mSandboxFlags(0)
  , mBusyFlags(BUSY_FLAGS_NONE)
  , mAppType(nsIDocShell::APP_TYPE_UNKNOWN)
  , mLoadType(0)
  , mDefaultLoadFlags(nsIRequest::LOAD_NORMAL)
  , mReferrerPolicy(0)
  , mFailedLoadType(0)
  , mFrameType(FRAME_TYPE_REGULAR)
  , mPrivateBrowsingId(0)
  , mDisplayMode(nsIDocShell::DISPLAY_MODE_BROWSER)
  , mJSRunToCompletionDepth(0)
  , mTouchEventsOverride(nsIDocShell::TOUCHEVENTS_OVERRIDE_NONE)
  , mFullscreenAllowed(CHECK_ATTRIBUTES)
  , mCreatingDocument(false)
#ifdef DEBUG
  , mInEnsureScriptEnv(false)
#endif
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
  , mUseErrorPages(false)
  , mObserveErrorPages(true)
  , mAllowAuth(true)
  , mAllowKeywordFixup(false)
  , mIsOffScreenBrowser(false)
  , mIsActive(true)
  , mDisableMetaRefreshWhenInactive(false)
  , mIsPrerendered(false)
  , mIsAppTab(false)
  , mUseGlobalHistory(false)
  , mUseRemoteTabs(false)
  , mUseTrackingProtection(false)
  , mDeviceSizeIsPageSize(false)
  , mWindowDraggingAllowed(false)
  , mInFrameSwap(false)
  , mInheritPrivateBrowsingId(true)
  , mCanExecuteScripts(false)
  , mFiredUnloadEvent(false)
  , mEODForCurrentDocument(false)
  , mURIResultedInDocument(false)
  , mIsBeingDestroyed(false)
  , mIsExecutingOnLoadHandler(false)
  , mIsPrintingOrPP(false)
  , mSavingOldViewer(false)
  , mDynamicallyCreated(false)
  , mAffectPrivateSessionLifetime(true)
  , mInvisible(false)
  , mHasLoadedNonBlankURI(false)
  , mBlankTiming(false)
{
  AssertOriginAttributesMatchPrivateBrowsing();

  nsContentUtils::GenerateUUIDInPlace(mHistoryID);

  if (gDocShellCount++ == 0) {
    NS_ASSERTION(sURIFixup == nullptr,
                 "Huh, sURIFixup not null in first nsDocShell ctor!");

    CallGetService(NS_URIFIXUP_CONTRACTID, &sURIFixup);
  }

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p created\n", this));

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  ++gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("++DOCSHELL %p == %ld [pid = %d] [id = %s]\n",
                  (void*)this,
                  gNumberOfDocShells,
                  getpid(),
                  nsIDToCString(mHistoryID).get());
  }
#endif
}

nsDocShell::~nsDocShell()
{
  MOZ_ASSERT(!mObserved);

  // Avoid notifying observers while we're in the dtor.
  mIsBeingDestroyed = true;

  Destroy();

  nsCOMPtr<nsISHistoryInternal> shPrivate(do_QueryInterface(mSessionHistory));
  if (shPrivate) {
    shPrivate->SetRootDocShell(nullptr);
  }

  if (--gDocShellCount == 0) {
    NS_IF_RELEASE(sURIFixup);
  }

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p destroyed\n", this));

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  --gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("--DOCSHELL %p == %ld [pid = %d] [id = %s]\n",
                  (void*)this,
                  gNumberOfDocShells,
                  getpid(),
                  nsIDToCString(mHistoryID).get());
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
  rv = mContentListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // We want to hold a strong ref to the loadgroup, so it better hold a weak
  // ref to us...  use an InterfaceRequestorProxy to do this.
  nsCOMPtr<nsIInterfaceRequestor> proxy =
    new InterfaceRequestorProxy(static_cast<nsIInterfaceRequestor*>(this));
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

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsDocShell,
                                   nsDocLoader,
                                   mSessionStorageManager,
                                   mScriptGlobal,
                                   mInitialClientSource)

NS_IMPL_ADDREF_INHERITED(nsDocShell, nsDocLoader)
NS_IMPL_RELEASE_INHERITED(nsDocShell, nsDocLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDocShell)
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
              aIID.Equals(NS_GET_IID(nsPIDOMWindowOuter)) ||
              aIID.Equals(NS_GET_IID(mozIDOMWindowProxy)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindow))) &&
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
    rv = wwatch->GetNewPrompter(mScriptGlobal->AsOuter(), &prompt);
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
  } else if (aIID.Equals(NS_GET_IID(nsIEditingSession))) {
    nsCOMPtr<nsIEditingSession> es;
    GetEditingSession(getter_AddRefs(es));
    es.forget(aSink);
    return *aSink ? NS_OK : NS_NOINTERFACE;
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
    *aSink = GetTabChild().take();
    return *aSink ? NS_OK : NS_ERROR_FAILURE;
  } else if (aIID.Equals(NS_GET_IID(nsIContentFrameMessageManager))) {
    nsCOMPtr<nsITabChild> tabChild =
      do_GetInterface(static_cast<nsIDocShell*>(this));
    nsCOMPtr<nsIContentFrameMessageManager> mm;
    if (tabChild) {
      tabChild->GetMessageManager(getter_AddRefs(mm));
    } else {
      if (nsPIDOMWindowOuter* win = GetWindow()) {
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

  nsCOMPtr<nsIURI> referrer;
  nsCOMPtr<nsIURI> originalURI;
  Maybe<nsCOMPtr<nsIURI>> resultPrincipalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIInputStream> postStream;
  nsCOMPtr<nsIInputStream> headersStream;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  bool inheritPrincipal = false;
  bool principalIsExplicit = false;
  bool sendReferrer = true;
  uint32_t referrerPolicy = mozilla::net::RP_Unset;
  bool isSrcdoc = false;
  nsCOMPtr<nsISHEntry> shEntry;
  nsString target;
  nsAutoString srcdoc;
  bool forceAllowDataURI = false;
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
    GetMaybeResultPrincipalURI(aLoadInfo, resultPrincipalURI);
    aLoadInfo->GetLoadReplace(&loadReplace);
    nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
    aLoadInfo->GetLoadType(&lt);
    // Get the appropriate loadType from nsIDocShellLoadInfo type
    loadType = ConvertDocShellInfoLoadTypeToLoadType(lt);

    aLoadInfo->GetTriggeringPrincipal(getter_AddRefs(triggeringPrincipal));
    aLoadInfo->GetInheritPrincipal(&inheritPrincipal);
    aLoadInfo->GetPrincipalIsExplicit(&principalIsExplicit);
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
    aLoadInfo->GetForceAllowDataURI(&forceAllowDataURI);
  }

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug,
          ("nsDocShell[%p]: loading %s with flags 0x%08x",
           this, aURI->GetSpecOrDefault().get(), aLoadFlags));

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
      // parent's loadType, if the subframe hasn't loaded anything into it.
      //
      // In some cases privileged scripts may try to get the DOMWindow
      // reference of this docshell before the loading starts, causing the
      // initial about:blank content viewer being created and mCurrentURI being
      // set. To handle this case we check if mCurrentURI is about:blank and
      // currentSHEntry is null.
      nsCOMPtr<nsISHEntry> currentChildEntry;
      GetCurrentSHEntry(getter_AddRefs(currentChildEntry), &oshe);
      if (!mCurrentURI || (NS_IsAboutBlank(mCurrentURI) && !currentChildEntry)) {
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
                     (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE) ||
                     (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE) ||
                     (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE)))) {
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
        // This is a pre-existing subframe. If
        // 1. The load of this frame was not originally initiated by session
        //    history directly (i.e. (!shEntry) condition succeeded, but it can
        //    still be a history load on parent which causes this frame being
        //    loaded), and
        // 2. mCurrentURI is not null, nor the initial about:blank,
        // it is possible that a parent's onLoadHandler or even self's
        // onLoadHandler is loading a new page in this child. Check parent's and
        // self's busy flag and if it is set, we don't want this onLoadHandler
        // load to get in to session history.
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

  // We need a principalToInherit.
  //
  // If principalIsExplicit is not set there are 4 possibilities:
  // (1) If the system principal or an expanded principal was passed
  //     in and we're a typeContent docshell, inherit the principal
  //     from the current document instead.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     or if we're being called from system code (eg chrome JS or pure
  //     C++) then inheritPrincipal should be true and InternalLoad will get
  //     a principal from the current document. If none of these things are
  //     true, then
  // (4) we don't pass a principal into the channel, and a principal will be
  //     created later from the channel's internal data.
  //
  // If principalIsExplicit *is* set, there are 4 possibilities
  // (1) If the system principal or an expanded principal was passed in
  //     and we're a typeContent docshell, return an error.
  // (2) In all other cases when the principal passed in is not null,
  //     use that principal.
  // (3) If the caller has allowed inheriting from the current document,
  //     then inheritPrincipal should be true and InternalLoad will get
  //     a principal from the current document. If none of these things are
  //     true, then
  // (4) we dont' pass a principal into the channel, and a principal will be
  //     created later from the channel's internal data.
  nsCOMPtr<nsIPrincipal> principalToInherit = triggeringPrincipal;
  if (principalToInherit && mItemType != typeChrome) {
    if (nsContentUtils::IsSystemPrincipal(principalToInherit)) {
      if (principalIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      principalToInherit = nullptr;
      inheritPrincipal = true;
    } else if (nsContentUtils::IsExpandedPrincipal(principalToInherit)) {
      if (principalIsExplicit) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      // Don't inherit from the current page.  Just do the safe thing
      // and pretend that we were loaded by a nullprincipal.
      //
      // We didn't inherit OriginAttributes here as ExpandedPrincipal doesn't
      // have origin attributes.
      principalToInherit = NullPrincipal::CreateWithInheritedAttributes(this);
      inheritPrincipal = false;
    }
  }
  if (!principalToInherit && !inheritPrincipal && !principalIsExplicit) {
    // See if there's system or chrome JS code running
    inheritPrincipal = nsContentUtils::LegacyIsCallerChromeOrNativeCode();
  }

  if (aLoadFlags & LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL) {
    inheritPrincipal = false;
    // If aFirstParty is true and the pref 'privacy.firstparty.isolate' is
    // enabled, we will set firstPartyDomain on the origin attributes.
    principalToInherit = NullPrincipal::CreateWithInheritedAttributes(this, aFirstParty);
  }

  // If the triggeringPrincipal is not passed explicitly, we first try to create
  // a principal from the referrer, since the referrer URI reflects the web origin
  // that triggered the load. If there is no referrer URI, we fall back to using
  // the SystemPrincipal. It's safe to assume that no provided triggeringPrincipal
  // and no referrer simulate a load that was triggered by the system.
  // It's important to note that this block of code needs to appear *after* the block
  // where we munge the principalToInherit, because otherwise we would never enter
  // code blocks checking if the principalToInherit is null and we will end up with
  // a wrong inheritPrincipal flag.
  if (!triggeringPrincipal) {
    if (referrer) {
      nsresult rv = CreatePrincipalFromReferrer(referrer,
                                                getter_AddRefs(triggeringPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
    }
  }

  uint32_t flags = 0;

  if (inheritPrincipal) {
    flags |= INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL;
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

  if (forceAllowDataURI) {
    flags |= INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
  }

  return InternalLoad(aURI,
                      originalURI,
                      resultPrincipalURI,
                      loadReplace,
                      referrer,
                      referrerPolicy,
                      triggeringPrincipal,
                      principalToInherit,
                      flags,
                      target,
                      nullptr,      // No type hint
                      VoidString(), // No forced download
                      postStream,
                      -1, // XXXbaku
                      headersStream,
                      loadType,
                      nullptr, // No SHEntry
                      aFirstParty,
                      srcdoc,
                      sourceDocShell,
                      baseURI,
                      false,
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
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  if (aLoadInfo) {
    nsDocShellInfoLoadType lt = nsIDocShellLoadInfo::loadNormal;
    (void)aLoadInfo->GetLoadType(&lt);
    // Get the appropriate LoadType from nsIDocShellLoadInfo type
    loadType = ConvertDocShellInfoLoadTypeToLoadType(lt);
    aLoadInfo->GetTriggeringPrincipal(getter_AddRefs(triggeringPrincipal));
  }

  NS_ENSURE_SUCCESS(Stop(nsIWebNavigation::STOP_NETWORK), NS_ERROR_FAILURE);

  mLoadType = loadType;

  if (!triggeringPrincipal) {
    triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  }

  // build up a channel for this stream.
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIInputStream> stream = aStream;
  nsresult rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                         uri,
                                         stream.forget(),
                                         triggeringPrincipal,
                                         nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
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
  FirePageHideNotificationInternal(aIsUnload, false);
  return NS_OK;
}

void
nsDocShell::FirePageHideNotificationInternal(bool aIsUnload,
                                             bool aSkipCheckingDynEntries)
{
  if (mContentViewer && !mFiredUnloadEvent) {
    // Keep an explicit reference since calling PageHide could release
    // mContentViewer
    nsCOMPtr<nsIContentViewer> contentViewer(mContentViewer);
    mFiredUnloadEvent = true;

    if (mTiming) {
      mTiming->NotifyUnloadEventStart();
    }

    contentViewer->PageHide(aIsUnload);

    if (mTiming) {
      mTiming->NotifyUnloadEventEnd();
    }

    AutoTArray<nsCOMPtr<nsIDocShell>, 8> kids;
    uint32_t n = mChildList.Length();
    kids.SetCapacity(n);
    for (uint32_t i = 0; i < n; i++) {
      kids.AppendElement(do_QueryInterface(ChildAt(i)));
    }

    n = kids.Length();
    for (uint32_t i = 0; i < n; ++i) {
      RefPtr<nsDocShell> child = static_cast<nsDocShell*>(kids[i].get());
      if (child) {
        // Skip checking dynamic subframe entries in our children.
        child->FirePageHideNotificationInternal(aIsUnload, true);
      }
    }

    // If the document is unloading, remove all dynamic subframe entries.
    if (aIsUnload && !aSkipCheckingDynEntries) {
      nsCOMPtr<nsISHistory> rootSH;
      GetRootSessionHistory(getter_AddRefs(rootSH));
      nsCOMPtr<nsISHistoryInternal> shPrivate = do_QueryInterface(rootSH);
      nsCOMPtr<nsISHContainer> container(do_QueryInterface(mOSHE));
      if (shPrivate && container) {
        int32_t index = -1;
        rootSH->GetIndex(&index);
        shPrivate->RemoveDynEntries(index, container);
      }
    }

    // Now make sure our editor, if any, is detached before we go
    // any farther.
    DetachEditorFromWindow();
  }
}

nsresult
nsDocShell::DispatchToTabGroup(TaskCategory aCategory,
                               already_AddRefed<nsIRunnable>&& aRunnable)
{
  // Hold the ref so we won't forget to release it.
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    // Window should only be unavailable after destroyed.
    MOZ_ASSERT(mIsBeingDestroyed);
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozilla::dom::TabGroup> tabGroup = win->TabGroup();
  return tabGroup->Dispatch(aCategory, runnable.forget());
}

NS_IMETHODIMP
nsDocShell::DispatchLocationChangeEvent()
{
  return DispatchToTabGroup(
    TaskCategory::Other,
    NewRunnableMethod("nsDocShell::FireDummyOnLocationChange",
                      this,
                      &nsDocShell::FireDummyOnLocationChange));
}

bool
nsDocShell::MaybeInitTiming()
{
  if (mTiming && !mBlankTiming) {
    return false;
  }

  bool canBeReset = false;

  if (mScriptGlobal && mBlankTiming) {
    nsPIDOMWindowInner* innerWin =
      mScriptGlobal->AsOuter()->GetCurrentInnerWindow();
    if (innerWin && innerWin->GetPerformance()) {
      mTiming = innerWin->GetPerformance()->GetDOMTiming();
      mBlankTiming = false;
    }
  }

  if (!mTiming) {
    mTiming = new nsDOMNavigationTiming(this);
    canBeReset = true;
  }

  mTiming->NotifyNavigationStart(
    mIsActive ? nsDOMNavigationTiming::DocShellState::eActive
              : nsDOMNavigationTiming::DocShellState::eInactive);

  return canBeReset;
}

void
nsDocShell::MaybeResetInitTiming(bool aReset)
{
  if (aReset) {
    mTiming = nullptr;
  }
}

nsDOMNavigationTiming*
nsDocShell::GetNavigationTiming() const
{
  return mTiming;
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
  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug,
          ("DOCSHELL %p SetCurrentURI %s\n",
           this, aURI ? aURI->GetSpecOrDefault().get() : ""));

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
  doc->GetDocumentCharacterSet()->Name(aCharset);
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
    mForcedCharset = nullptr;
    return NS_OK;
  }
  const Encoding* encoding = Encoding::ForLabel(aCharset);
  if (!encoding) {
    // Reject unknown labels
    return NS_ERROR_INVALID_ARG;
  }
  if (!encoding->IsAsciiCompatible() && encoding != ISO_2022_JP_ENCODING) {
    // Reject XSS hazards
    return NS_ERROR_INVALID_ARG;
  }
  mForcedCharset = encoding;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetForcedCharset(nsACString& aResult)
{
  if (mForcedCharset) {
    mForcedCharset->Name(aResult);
  } else {
    aResult.Truncate();
  }
  return NS_OK;
}

void
nsDocShell::SetParentCharset(const Encoding*& aCharset,
                             int32_t aCharsetSource,
                             nsIPrincipal* aPrincipal)
{
  mParentCharset = aCharset;
  mParentCharsetSource = aCharsetSource;
  mParentCharsetPrincipal = aPrincipal;
}

void
nsDocShell::GetParentCharset(const Encoding*& aCharset,
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
  AssertOriginAttributesMatchPrivateBrowsing();
  *aUsePrivateBrowsing = mPrivateBrowsingId > 0;
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

  if (!CanSetOriginAttributes()) {
    bool changed = aUsePrivateBrowsing != (mPrivateBrowsingId > 0);

    return changed ? NS_ERROR_FAILURE : NS_OK;
  }

  return SetPrivateBrowsing(aUsePrivateBrowsing);
}

NS_IMETHODIMP
nsDocShell::SetPrivateBrowsing(bool aUsePrivateBrowsing)
{
  bool changed = aUsePrivateBrowsing != (mPrivateBrowsingId > 0);
  if (changed) {
    mPrivateBrowsingId = aUsePrivateBrowsing ? 1 : 0;

    if (mItemType != typeChrome) {
      mOriginAttributes.SyncAttributesWithPrivateBrowsing(aUsePrivateBrowsing);
    }

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

  AssertOriginAttributesMatchPrivateBrowsing();
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
  if (aUseRemoteTabs) {
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("DOMIPCEnabled"),
                                       NS_LITERAL_CSTRING("1"));
  }

  mUseRemoteTabs = aUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAffectPrivateSessionLifetime(bool aAffectLifetime)
{
  bool change = aAffectLifetime != mAffectPrivateSessionLifetime;
  if (change && UsePrivateBrowsing()) {
    AssertOriginAttributesMatchPrivateBrowsing();
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
    if (nsPIDOMWindowInner* innerWin =
        mScriptGlobal->AsOuter()->GetCurrentInnerWindow()) {
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
nsDocShell::GetInheritPrivateBrowsingId(bool* aInheritPrivateBrowsingId)
{
  *aInheritPrivateBrowsingId = mInheritPrivateBrowsingId;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetInheritPrivateBrowsingId(bool aInheritPrivateBrowsingId)
{
  mInheritPrivateBrowsingId = aInheritPrivateBrowsingId;
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

  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    return NS_OK;
  }
  if (nsCOMPtr<Element> frameElement = win->GetFrameElementInternal()) {
    if (frameElement->IsXULElement()) {
      if (frameElement->HasAttr(kNameSpaceID_None,
                                nsGkAtoms::disablefullscreen)) {
        // Document inside this frame is explicitly disabled.
        return NS_OK;
      }
    } else {
      // We do not allow document inside any containing element other
      // than iframe to enter fullscreen.
      if (frameElement->IsHTMLElement(nsGkAtoms::iframe)) {
        // If any ancestor iframe does not have allowfullscreen attribute
        // set, then fullscreen is not allowed.
        if (!frameElement->HasAttr(kNameSpaceID_None,
                                  nsGkAtoms::allowfullscreen) &&
            !frameElement->HasAttr(kNameSpaceID_None,
                                  nsGkAtoms::mozallowfullscreen)) {
          return NS_OK;
        }
      } else if (frameElement->IsHTMLElement(nsGkAtoms::embed)) {
        // Respect allowfullscreen only if this is a rewritten YouTube embed.
        nsCOMPtr<nsIObjectLoadingContent> objectLoadingContent =
          do_QueryInterface(frameElement);
        if (!objectLoadingContent) {
          return NS_OK;
        }
        nsObjectLoadingContent* olc =
          static_cast<nsObjectLoadingContent*>(objectLoadingContent.get());
        if (!olc->IsRewrittenYoutubeEmbed()) {
          return NS_OK;
        }
        // We don't have to check prefixed attributes because Flash does not
        // support them.
        if (!frameElement->HasAttr(kNameSpaceID_None,
                                  nsGkAtoms::allowfullscreen)) {
          return NS_OK;
        }
      } else {
        // neither iframe nor embed
        return NS_OK;
      }
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
  if (!nsIDocShell::GetIsMozBrowser()) {
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
nsDocShell::GetLoadURIDelegate(nsILoadURIDelegate** aLoadURIDelegate)
{
  NS_IF_ADDREF(*aLoadURIDelegate = mLoadURIDelegate);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLoadURIDelegate(nsILoadURIDelegate* aLoadURIDelegate)
{
  mLoadURIDelegate = aLoadURIDelegate;
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
  if (currentValue == aValue) {
    return NS_OK;
  }

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (!timelines) {
    return NS_OK;
  }

  if (aValue) {
    MOZ_ASSERT(!timelines->HasConsumer(this));
    timelines->AddConsumer(this);
    MOZ_ASSERT(timelines->HasConsumer(this));
    UseEntryScriptProfiling();
  } else {
    MOZ_ASSERT(timelines->HasConsumer(this));
    timelines->RemoveConsumer(this);
    MOZ_ASSERT(!timelines->HasConsumer(this));
    UnuseEntryScriptProfiling();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRecordProfileTimelineMarkers(bool* aValue)
{
  *aValue = !!mObserved;
  return NS_OK;
}

nsresult
nsDocShell::PopProfileTimelineMarkers(
    JSContext* aCx,
    JS::MutableHandle<JS::Value> aOut)
{
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (!timelines) {
    return NS_OK;
  }

  nsTArray<dom::ProfileTimelineMarker> store;
  SequenceRooter<dom::ProfileTimelineMarker> rooter(aCx, &store);

  timelines->PopMarkers(this, aCx, store);

  if (!ToJSValue(aCx, store, aOut)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsDocShell::Now(DOMHighResTimeStamp* aWhen)
{
  *aWhen = (TimeStamp::Now() - TimeStamp::ProcessCreation()).ToMilliseconds();
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
nsDocShell::NameEquals(const nsAString& aName, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mName.Equals(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCustomUserAgent(nsAString& aCustomUserAgent)
{
  aCustomUserAgent = mCustomUserAgent;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCustomUserAgent(const nsAString& aCustomUserAgent)
{
  mCustomUserAgent = aCustomUserAgent;
  RefPtr<nsGlobalWindowInner> win = mScriptGlobal ?
    mScriptGlobal->GetCurrentInnerWindowInternal() : nullptr;
  if (win) {
    Navigator* navigator = win->Navigator();
    if (navigator) {
      navigator->ClearUserAgentCache();
    }
  }

  uint32_t childCount = mChildList.Length();
  for (uint32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShell> childShell = do_QueryInterface(ChildAt(i));
    if (childShell) {
      childShell->SetCustomUserAgent(aCustomUserAgent);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTouchEventsOverride(uint32_t* aTouchEventsOverride)
{
  NS_ENSURE_ARG_POINTER(aTouchEventsOverride);

  *aTouchEventsOverride = mTouchEventsOverride;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTouchEventsOverride(uint32_t aTouchEventsOverride)
{
  if (!(aTouchEventsOverride == nsIDocShell::TOUCHEVENTS_OVERRIDE_NONE ||
        aTouchEventsOverride == nsIDocShell::TOUCHEVENTS_OVERRIDE_ENABLED ||
        aTouchEventsOverride == nsIDocShell::TOUCHEVENTS_OVERRIDE_DISABLED)) {
    return NS_ERROR_INVALID_ARG;
  }

  mTouchEventsOverride = aTouchEventsOverride;

  uint32_t childCount = mChildList.Length();
  for (uint32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShell> childShell = do_QueryInterface(ChildAt(i));
    if (childShell) {
      childShell->SetTouchEventsOverride(aTouchEventsOverride);
    }
  }
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
nsDocShell::MaybeCreateInitialClientSource(nsIPrincipal* aPrincipal)
{
  // If there is an existing document then there is no need to create
  // a client for a future initial about:blank document.
  if (mScriptGlobal && mScriptGlobal->GetCurrentInnerWindowInternal() &&
      mScriptGlobal->GetCurrentInnerWindowInternal()->GetExtantDoc()) {
    MOZ_DIAGNOSTIC_ASSERT(
      mScriptGlobal->GetCurrentInnerWindowInternal()->GetClientInfo().isSome());
    MOZ_DIAGNOSTIC_ASSERT(!mInitialClientSource);
    return;
  }

  // Don't recreate the initial client source.  We call this multiple times
  // when DoChannelLoad() is called before CreateAboutBlankContentViewer.
  if (mInitialClientSource) {
    return;
  }

  // Don't pre-allocate the client when we are sandboxed.  The inherited
  // principal does not take sandboxing into account.
  // TODO: Refactor sandboxing principal code out so we can use it here.
  if (!aPrincipal && mSandboxFlags) {
    return;
  }

  nsIPrincipal* principal = aPrincipal ? aPrincipal
                                       : GetInheritedPrincipal(false);

  // Sometimes there is no principal available when we are called from
  // CreateAboutBlankContentViewer.  For example, sometimes the principal
  // is only extracted from the load context after the document is created
  // in nsDocument::ResetToURI().  Ideally we would do something similar
  // here, but for now lets just avoid the issue by not preallocating the
  // client.
  if (!principal) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    return;
  }

  mInitialClientSource =
    ClientManager::CreateSource(ClientType::Window,
                                win->EventTargetFor(TaskCategory::Other),
                                principal);
  MOZ_DIAGNOSTIC_ASSERT(mInitialClientSource);

  // Mark the initial client as execution ready, but owned by the docshell.
  // If the client is actually used this will cause ClientSource to force
  // the creation of the initial about:blank by calling nsDocShell::GetDocument().
  mInitialClientSource->DocShellExecutionReady(this);

  // Next, check to see if the parent is controlled.
  nsCOMPtr<nsIDocShell> parent = GetParentDocshell();
  nsPIDOMWindowOuter* parentOuter = parent ? parent->GetWindow() : nullptr;
  nsPIDOMWindowInner* parentInner =
    parentOuter ? parentOuter->GetCurrentInnerWindow() : nullptr;
  if (!parentInner) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:blank")));

  // We're done if there is no parent controller or if this docshell
  // is not permitted to control for some reason.
  Maybe<ServiceWorkerDescriptor> controller(parentInner->GetController());
  if (controller.isNothing() || !ServiceWorkerAllowedToControlWindow(principal, uri)) {
    return;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    return;
  }

  // If the parent is controlled then propagate that controller to the
  // initial about:blank client as well.  This will set the controller
  // in the ClientManagerService in the parent.
  //
  // Note: If the registration is missing from the SWM we avoid setting
  //       the controller on the client.  We can do this synchronously
  //       for now since SWM is in the child process.  In the future
  //       when SWM is in the parent process we will probably have to
  //       always set the initial client source and then somehow clear
  //       it if we find the registration is acutally gone.  Its also
  //       possible this race only occurs in cases where the resulting
  //       window is no longer exposed.  For example, in theory the SW
  //       should not go away if our parent window is controlled.
  if (!swm->StartControlling(mInitialClientSource->Info(), controller.ref())) {
    return;
  }

  // Also mark the ClientSource as controlled directly in case script
  // immediately accesses navigator.serviceWorker.controller.
  mInitialClientSource->SetController(controller.ref());
}

Maybe<ClientInfo>
nsDocShell::GetInitialClientInfo() const
{
  if (mInitialClientSource) {
    Maybe<ClientInfo> result;
    result.emplace(mInitialClientSource->Info());
    return Move(result);
  }

  nsGlobalWindowInner* innerWindow =
    mScriptGlobal ? mScriptGlobal->GetCurrentInnerWindowInternal() : nullptr;
  nsIDocument* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;

  if (!doc || !doc->IsInitialDocument()) {
    return Maybe<ClientInfo>();
  }

  return innerWindow->GetClientInfo();
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
#ifdef DEBUG
  bool wasPrivate = UsePrivateBrowsing();
#endif

  nsresult rv = nsDocLoader::SetDocLoaderParent(aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsPriority> priorityGroup = do_QueryInterface(mLoadGroup);
  if (wasFrame != IsFrame() && priorityGroup) {
    priorityGroup->AdjustPriority(wasFrame ? -1 : 1);
  }

  // Curse ambiguous nsISupports inheritance!
  nsISupports* parent = GetAsSupports(aParent);

  // If parent is another docshell, we inherit all their flags for
  // allowing plugins, scripting etc.
  bool value;
  nsString customUserAgent;
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
    if (parentAsDocShell->GetIsPrerendered()) {
      SetIsPrerendered();
    }
    if (NS_SUCCEEDED(parentAsDocShell->GetIsActive(&value))) {
      // a prerendered docshell is not active yet
      SetIsActive(value && !mIsPrerendered);
    }
    if (NS_SUCCEEDED(parentAsDocShell->GetCustomUserAgent(customUserAgent)) &&
        !customUserAgent.IsEmpty()) {
      SetCustomUserAgent(customUserAgent);
    }
    if (NS_FAILED(parentAsDocShell->GetAllowDNSPrefetch(&value))) {
      value = false;
    }
    SetAllowDNSPrefetch(mAllowDNSPrefetch && value);
    if (mInheritPrivateBrowsingId) {
      value = parentAsDocShell->GetAffectPrivateSessionLifetime();
      SetAffectPrivateSessionLifetime(value);
    }
    uint32_t flags;
    if (NS_SUCCEEDED(parentAsDocShell->GetDefaultLoadFlags(&flags))) {
      SetDefaultLoadFlags(flags);
    }
    uint32_t touchEventsOverride;
    if (NS_SUCCEEDED(parentAsDocShell->GetTouchEventsOverride(&touchEventsOverride))) {
      SetTouchEventsOverride(touchEventsOverride);
    }
  }

  nsCOMPtr<nsILoadContext> parentAsLoadContext(do_QueryInterface(parent));
  if (parentAsLoadContext && mInheritPrivateBrowsingId &&
      NS_SUCCEEDED(parentAsLoadContext->GetUsePrivateBrowsing(&value))) {
    SetPrivateBrowsing(value);
  }

  nsCOMPtr<nsIURIContentListener> parentURIListener(do_GetInterface(parent));
  if (parentURIListener) {
    mContentListener->SetParentContentListener(parentURIListener);
  }

  // Our parent has changed. Recompute scriptability.
  RecomputeCanExecuteScripts();

  NS_ASSERTION(mInheritPrivateBrowsingId || wasPrivate == UsePrivateBrowsing(),
               "Private browsing state changed while inheritance was disabled");

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeParent(nsIDocShellTreeItem** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nullptr;

  if (nsIDocShell::GetIsMozBrowser()) {
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
nsDocShell::GetSameTypeParentIgnoreBrowserBoundaries(nsIDocShell** aParent)
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
nsDocShell::GetSameTypeRootTreeItemIgnoreBrowserBoundaries(nsIDocShell** aRootTreeItem)
{
    NS_ENSURE_ARG_POINTER(aRootTreeItem);
    *aRootTreeItem = static_cast<nsIDocShell *>(this);

    nsCOMPtr<nsIDocShell> parent;
    NS_ENSURE_SUCCESS(GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent)),
                      NS_ERROR_FAILURE);
    while (parent) {
      *aRootTreeItem = parent;
      NS_ENSURE_SUCCESS((*aRootTreeItem)->
        GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent)),
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
  if (!targetDS || !accessingDS) {
    // We must be able to convert both to nsIDocShell.
    return false;
  }

  if (targetDS->GetIsInIsolatedMozBrowserElement() !=
        accessingDS->GetIsInIsolatedMozBrowserElement()) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> accessingRoot;
  aAccessingItem->GetSameTypeRootTreeItem(getter_AddRefs(accessingRoot));
  nsCOMPtr<nsIDocShell> accessingRootDS = do_QueryInterface(accessingRoot);

  nsCOMPtr<nsIDocShellTreeItem> targetRoot;
  aTargetItem->GetSameTypeRootTreeItem(getter_AddRefs(targetRoot));
  nsCOMPtr<nsIDocShell> targetRootDS = do_QueryInterface(targetRoot);

  OriginAttributes targetOA =
    static_cast<nsDocShell*>(targetDS.get())->GetOriginAttributes();
  OriginAttributes accessingOA =
    static_cast<nsDocShell*>(accessingDS.get())->GetOriginAttributes();

  // When the first party isolation is on, the top-level docShell may not have
  // the firstPartyDomain in its originAttributes, but its document will have
  // it. So we get the firstPartyDomain from the nodePrincipal of the document
  // before we compare the originAttributes.
  if (OriginAttributes::IsFirstPartyEnabled()) {
    if (aAccessingItem->ItemType() == nsIDocShellTreeItem::typeContent &&
        (accessingDS == accessingRootDS || accessingDS->GetIsMozBrowser())) {

      nsCOMPtr<nsIDocument> accessingDoc = aAccessingItem->GetDocument();

      if (accessingDoc) {
        nsCOMPtr<nsIPrincipal> accessingPrincipal = accessingDoc->NodePrincipal();

        accessingOA.mFirstPartyDomain =
          accessingPrincipal->OriginAttributesRef().mFirstPartyDomain;
      }
    }

    if (aTargetItem->ItemType() == nsIDocShellTreeItem::typeContent &&
        (targetDS == targetRootDS || targetDS->GetIsMozBrowser())) {

      nsCOMPtr<nsIDocument> targetDoc = aAccessingItem->GetDocument();

      if (targetDoc) {
        nsCOMPtr<nsIPrincipal> targetPrincipal = targetDoc->NodePrincipal();

        targetOA.mFirstPartyDomain =
          targetPrincipal->OriginAttributesRef().mFirstPartyDomain;
      }
    }
  }

  if (targetOA != accessingOA) {
    return false;
  }

  // A private document can't access a non-private one, and vice versa.
  if (static_cast<nsDocShell*>(targetDS.get())->UsePrivateBrowsing() !=
      static_cast<nsDocShell*>(accessingDS.get())->UsePrivateBrowsing()) {
    return false;
  }

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

  nsCOMPtr<nsPIDOMWindowOuter> targetWindow = aTargetItem->GetWindow();
  if (!targetWindow) {
    NS_ERROR("This should not happen, really");
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> targetOpener = targetWindow->GetOpener();
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
  if (nsCOMPtr<nsPIDOMWindowOuter> window = aItem->GetWindow()) {
    auto* win = nsGlobalWindowOuter::Cast(window);
    if (!win->GetClosedOuter()) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
nsDocShell::FindItemWithName(const nsAString& aName,
                             nsIDocShellTreeItem* aRequestor,
                             nsIDocShellTreeItem* aOriginalRequestor,
                             bool aSkipTabGroup,
                             nsIDocShellTreeItem** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // If we don't find one, we return NS_OK and a null result
  *aResult = nullptr;

  if (aName.IsEmpty()) {
    return NS_OK;
  }

  if (aRequestor) {
    // If aRequestor is not null we don't need to check special names, so
    // just hand straight off to the search by actual name function.
    return DoFindItemWithName(aName, aRequestor, aOriginalRequestor,
                              aSkipTabGroup, aResult);
  } else {
    // This is the entry point into the target-finding algorithm.  Check
    // for special names.  This should only be done once, hence the check
    // for a null aRequestor.

    nsCOMPtr<nsIDocShellTreeItem> foundItem;
    if (aName.LowerCaseEqualsLiteral("_self")) {
      foundItem = this;
    } else if (aName.LowerCaseEqualsLiteral("_blank")) {
      // Just return null.  Caller must handle creating a new window with
      // a blank name himself.
      return NS_OK;
    } else if (aName.LowerCaseEqualsLiteral("_parent")) {
      GetSameTypeParent(getter_AddRefs(foundItem));
      if (!foundItem) {
        foundItem = this;
      }
    } else if (aName.LowerCaseEqualsLiteral("_top")) {
      GetSameTypeRootTreeItem(getter_AddRefs(foundItem));
      NS_ASSERTION(foundItem, "Must have this; worst case it's us!");
    } else {
      // Do the search for item by an actual name.
      DoFindItemWithName(aName, aRequestor, aOriginalRequestor,
                         aSkipTabGroup, getter_AddRefs(foundItem));
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

void
nsDocShell::AssertOriginAttributesMatchPrivateBrowsing() {
  // Chrome docshells must not have a private browsing OriginAttribute
  // Content docshells must maintain the equality:
  // mOriginAttributes.mPrivateBrowsingId == mPrivateBrowsingId
  if (mItemType == typeChrome) {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId == 0);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId == mPrivateBrowsingId);
  }
}

nsresult
nsDocShell::DoFindItemWithName(const nsAString& aName,
                               nsIDocShellTreeItem* aRequestor,
                               nsIDocShellTreeItem* aOriginalRequestor,
                               bool aSkipTabGroup,
                               nsIDocShellTreeItem** aResult)
{
  // First we check our name.
  if (mName.Equals(aName) && ItemIsActive(this) &&
      CanAccessItem(this, aOriginalRequestor)) {
    NS_ADDREF(*aResult = this);
    return NS_OK;
  }

  // Second we check our children making sure not to ask a child if
  // it is the aRequestor.
#ifdef DEBUG
  nsresult rv =
#endif
  FindChildWithName(aName, true, true, aRequestor, aOriginalRequestor,
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
    if (parentAsTreeItem == aRequestor) {
      return NS_OK;
    }

    // If we have a same-type parent, respecting browser and app boundaries.
    // NOTE: Could use GetSameTypeParent if the issues described in bug 1310344 are fixed.
    if (!GetIsMozBrowser() && parentAsTreeItem->ItemType() == mItemType) {
      return parentAsTreeItem->FindItemWithName(
        aName,
        static_cast<nsIDocShellTreeItem*>(this),
        aOriginalRequestor,
        /* aSkipTabGroup = */ false,
        aResult);
    }
  }

  // If we have a null parent or the parent is not of the same type, we need to
  // give up on finding it in our tree, and start looking in our TabGroup.
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (window && !aSkipTabGroup) {
    RefPtr<mozilla::dom::TabGroup> tabGroup = window->TabGroup();
    tabGroup->FindItemWithName(aName, aRequestor, aOriginalRequestor, aResult);
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

  nsCOMPtr<nsPIDOMWindowOuter> domwin(doc->GetWindow());

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
nsDocShell::SetChildOffset(int32_t aChildOffset)
{
  mChildOffset = aChildOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChildOffset(int32_t* aChildOffset)
{
  *aChildOffset = mChildOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHistoryID(nsID** aID)
{
  *aID = mHistoryID.Clone();
  return NS_OK;
}

const nsID
nsDocShell::HistoryID()
{
  return mHistoryID;
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
    nsresult rv = childsParent->RemoveChildLoader(childAsDocLoader);
    NS_ENSURE_SUCCESS(rv, rv);
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

    const Encoding* parentCS = doc->GetDocumentCharacterSet();
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
nsDocShell::FindChildWithName(const nsAString& aName,
                              bool aRecurse, bool aSameType,
                              nsIDocShellTreeItem* aRequestor,
                              nsIDocShellTreeItem* aOriginalRequestor,
                              nsIDocShellTreeItem** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // if we don't find one, we return NS_OK and a null result
  *aResult = nullptr;

  if (aName.IsEmpty()) {
    return NS_OK;
  }

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
      rv = nsSHistory::CloneAndReplace(currentEntry, this, cloneID,
        aNewEntry, aCloneChildren, getter_AddRefs(nextEntry));

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
  AutoTArray<nsID, 16> ids({mHistoryID});
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

nsPIDOMWindowOuter*
nsDocShell::GetWindow()
{
  if (NS_FAILED(EnsureScriptEnvironment())) {
    return nullptr;
  }
  return mScriptGlobal->AsOuter();
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
  AutoTArray<nsID, 16> ids;
  for (int32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    shcontainer->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      ids.AppendElement(child->DocshellID());
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
                    nsIInputStream* aHeaderStream,
                    nsIPrincipal* aTriggeringPrincipal)
{
  return LoadURIWithOptions(aURI, aLoadFlags, aReferringURI,
                            mozilla::net::RP_Unset, aPostStream,
                            aHeaderStream, nullptr, aTriggeringPrincipal);
}

NS_IMETHODIMP
nsDocShell::LoadURIWithOptions(const char16_t* aURI,
                               uint32_t aLoadFlags,
                               nsIURI* aReferringURI,
                               uint32_t aReferrerPolicy,
                               nsIInputStream* aPostStream,
                               nsIInputStream* aHeaderStream,
                               nsIURI* aBaseURI,
                               nsIPrincipal* aTriggeringPrincipal)
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
  uriString.StripCRLF();
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
    if (DisplayLoadError(rv, uri, aURI, nullptr) &&
        (aLoadFlags & LOAD_FLAGS_ERROR_LOAD_CHANGES_RV) != 0) {
      return NS_ERROR_LOAD_SHOWED_ERRORPAGE;
    }
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

  bool forceAllowDataURI =
    aLoadFlags & LOAD_FLAGS_FORCE_ALLOW_DATA_URI;

  // Don't pass certain flags that aren't needed and end up confusing
  // ConvertLoadTypeToDocShellInfoLoadType.  We do need to ensure that they are
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

  loadInfo->SetLoadType(ConvertLoadTypeToDocShellInfoLoadType(loadType));
  loadInfo->SetPostDataStream(postStream);
  loadInfo->SetReferrer(aReferringURI);
  loadInfo->SetReferrerPolicy(aReferrerPolicy);
  loadInfo->SetHeadersStream(aHeaderStream);
  loadInfo->SetBaseURI(aBaseURI);
  loadInfo->SetTriggeringPrincipal(aTriggeringPrincipal);
  loadInfo->SetForceAllowDataURI(forceAllowDataURI);

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
                             nsIChannel* aFailedChannel,
                             bool* aDisplayedErrorPage)
{
  *aDisplayedErrorPage = false;
  // Get prompt and string bundle servcies
  nsCOMPtr<nsIPrompt> prompter;
  nsCOMPtr<nsIStringBundle> stringBundle;
  GetPromptAndStringBundle(getter_AddRefs(prompter),
                           getter_AddRefs(stringBundle));

  NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  const char* error = nullptr;
  // The key used to select the appropriate error message from the properties file.
  const char* errorDescriptionID = nullptr;
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
    error = "unknownProtocolFound";
  } else if (NS_ERROR_FILE_NOT_FOUND == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    error = "fileNotFound";
  } else if (NS_ERROR_FILE_ACCESS_DENIED == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    error = "fileAccessDenied";
  } else if (NS_ERROR_UNKNOWN_HOST == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    // Get the host
    nsAutoCString host;
    nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(aURI);
    innermostURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;
    errorDescriptionID = "dnsNotFound2";
    error = "dnsNotFound";
  } else if (NS_ERROR_CONNECTION_REFUSED == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error = "connectionFailure";
  } else if (NS_ERROR_NET_INTERRUPT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error = "netInterrupt";
  } else if (NS_ERROR_NET_TIMEOUT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    // Get the host
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;
    error = "netTimeout";
  } else if (NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION == aError ||
             NS_ERROR_CSP_FORM_ACTION_VIOLATION == aError) {
    // CSP error
    cssClass.AssignLiteral("neterror");
    error = "cspBlocked";
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
        error = "sslv3Used";
        addHostPort = true;
      } else if (securityState & nsIWebProgressListener::STATE_USES_WEAK_CRYPTO) {
        error = "weakCryptoUsed";
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
        error = "nssBadCert";

        // If this is an HTTP Strict Transport Security host or a pinned host
        // and the certificate is bad, don't allow overrides (RFC 6797 section
        // 12.1, HPKP draft spec section 2.6).
        uint32_t flags =
          UsePrivateBrowsing() ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
        bool isStsHost = false;
        bool isPinnedHost = false;
        if (XRE_IsParentProcess()) {
          nsCOMPtr<nsISiteSecurityService> sss =
            do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI,
                                flags, mOriginAttributes, nullptr, nullptr,
                                &isStsHost);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HPKP, aURI,
                                flags, mOriginAttributes, nullptr, nullptr,
                                &isPinnedHost);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          mozilla::dom::ContentChild* cc =
            mozilla::dom::ContentChild::GetSingleton();
          mozilla::ipc::URIParams uri;
          SerializeURI(aURI, uri);
          cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, flags,
                              mOriginAttributes, &isStsHost);
          cc->SendIsSecureURI(nsISiteSecurityService::HEADER_HPKP, uri, flags,
                              mOriginAttributes, &isPinnedHost);
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
        nsAutoCString alternateErrorPage;
        nsresult rv =
          Preferences::GetCString("security.alternate_certificate_error_page",
                                  alternateErrorPage);
        if (NS_SUCCEEDED(rv)) {
          errorPage.Assign(alternateErrorPage);
        }

        if (!IsFrame() && errorPage.EqualsIgnoreCase("certerror")) {
          Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI, bucketId);
        }

      } else {
        error = "nssFailure2";
      }
    }
  } else if (NS_ERROR_PHISHING_URI == aError ||
             NS_ERROR_MALWARE_URI == aError ||
             NS_ERROR_UNWANTED_URI == aError ||
             NS_ERROR_HARMFUL_URI == aError) {
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, formatStrs[0]);
    formatStrCount = 1;

    // Malware and phishing detectors may want to use an alternate error
    // page, but if the pref's not set, we'll fall back on the standard page
    nsAutoCString alternateErrorPage;
    nsresult rv = Preferences::GetCString("urlclassifier.alternate_error_page",
                                          alternateErrorPage);
    if (NS_SUCCEEDED(rv)) {
      errorPage.Assign(alternateErrorPage);
    }

    uint32_t bucketId;
    bool sendTelemetry = false;
    if (NS_ERROR_PHISHING_URI == aError) {
      sendTelemetry = true;
      error = "deceptiveBlocked";
      bucketId = IsFrame() ? IUrlClassifierUITelemetry::WARNING_PHISHING_PAGE_FRAME
                           : IUrlClassifierUITelemetry::WARNING_PHISHING_PAGE_TOP;
    } else if (NS_ERROR_MALWARE_URI == aError) {
      sendTelemetry = true;
      error = "malwareBlocked";
      bucketId = IsFrame() ? IUrlClassifierUITelemetry::WARNING_MALWARE_PAGE_FRAME
                           : IUrlClassifierUITelemetry::WARNING_MALWARE_PAGE_TOP;
    } else if (NS_ERROR_UNWANTED_URI == aError) {
      sendTelemetry = true;
      error = "unwantedBlocked";
      bucketId = IsFrame() ? IUrlClassifierUITelemetry::WARNING_UNWANTED_PAGE_FRAME
                           : IUrlClassifierUITelemetry::WARNING_UNWANTED_PAGE_TOP;
    } else if (NS_ERROR_HARMFUL_URI == aError) {
      sendTelemetry = true;
      error = "harmfulBlocked";
      bucketId = IsFrame() ? IUrlClassifierUITelemetry::WARNING_HARMFUL_PAGE_FRAME
                           : IUrlClassifierUITelemetry::WARNING_HARMFUL_PAGE_TOP;
    }

    if (sendTelemetry && errorPage.EqualsIgnoreCase("blocked")) {
      Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UI_EVENTS, bucketId);
    }

    cssClass.AssignLiteral("blacklist");
  } else if (NS_ERROR_CONTENT_CRASHED == aError) {
    errorPage.AssignLiteral("tabcrashed");
    error = "tabcrashed";

    nsCOMPtr<EventTarget> handler = mChromeEventHandler;
    if (handler) {
      nsCOMPtr<Element> element = do_QueryInterface(handler);
      element->GetAttribute(NS_LITERAL_STRING("crashedPageTitle"), messageStr);
    }

    // DisplayLoadError requires a non-empty messageStr to proceed and call
    // LoadErrorPage. If the page doesn't have a title, we will use a blank
    // space which will be trimmed and thus treated as empty by the front-end.
    if (messageStr.IsEmpty()) {
      messageStr.AssignLiteral(u" ");
    }
  } else {
    // Errors requiring simple formatting
    switch (aError) {
      case NS_ERROR_MALFORMED_URI:
        // URI is malformed
        error = "malformedURI";
        errorDescriptionID = "malformedURI2";
        break;
      case NS_ERROR_REDIRECT_LOOP:
        // Doc failed to load because the server generated too many redirects
        error = "redirectLoop";
        break;
      case NS_ERROR_UNKNOWN_SOCKET_TYPE:
        // Doc failed to load because PSM is not installed
        error = "unknownSocketType";
        break;
      case NS_ERROR_NET_RESET:
        // Doc failed to load because the server kept reseting the connection
        // before we could read any data from it
        error = "netReset";
        break;
      case NS_ERROR_DOCUMENT_NOT_CACHED:
        // Doc failed to load because the cache does not contain a copy of
        // the document.
        error = "notCached";
        break;
      case NS_ERROR_OFFLINE:
        // Doc failed to load because we are offline.
        error = "netOffline";
        break;
      case NS_ERROR_DOCUMENT_IS_PRINTMODE:
        // Doc navigation attempted while Printing or Print Preview
        error = "isprinting";
        break;
      case NS_ERROR_PORT_ACCESS_NOT_ALLOWED:
        // Port blocked for security reasons
        addHostPort = true;
        error = "deniedPortAccess";
        break;
      case NS_ERROR_UNKNOWN_PROXY_HOST:
        // Proxy hostname could not be resolved.
        error = "proxyResolveFailure";
        break;
      case NS_ERROR_PROXY_CONNECTION_REFUSED:
        // Proxy connection was refused.
        error = "proxyConnectFailure";
        break;
      case NS_ERROR_INVALID_CONTENT_ENCODING:
        // Bad Content Encoding.
        error = "contentEncodingError";
        break;
      case NS_ERROR_REMOTE_XUL:
        error = "remoteXUL";
        break;
      case NS_ERROR_UNSAFE_CONTENT_TYPE:
        // Channel refused to load from an unrecognized content type.
        error = "unsafeContentType";
        break;
      case NS_ERROR_CORRUPTED_CONTENT:
        // Broken Content Detected. e.g. Content-MD5 check failure.
        error = "corruptedContentErrorv2";
        break;
      case NS_ERROR_INTERCEPTION_FAILED:
        // ServiceWorker intercepted request, but something went wrong.
        error = "corruptedContentErrorv2";
        break;
      case NS_ERROR_NET_INADEQUATE_SECURITY:
        // Server negotiated bad TLS for HTTP/2.
        error = "inadequateSecurityError";
        addHostPort = true;
        break;
      default:
        break;
    }
  }

  // Test if the error should be displayed
  if (!error) {
    return NS_OK;
  }

  if (!errorDescriptionID) {
    errorDescriptionID = error;
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
        aURI->GetPathQueryRef(spec);
      } else {
        aURI->GetSpec(spec);
      }

      nsCOMPtr<nsITextToSubURI> textToSubURI(
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) {
        rv = textToSubURI->UnEscapeURIForUI(NS_LITERAL_CSTRING("UTF-8"), spec,
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
    nsAutoString str;
    rv = stringBundle->FormatStringFromName(errorDescriptionID, strs, formatStrCount, str);
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
      error = "nssFailure2";
    }
  }

  if (UseErrorPages()) {
    // Display an error page
    nsresult loadedPage = LoadErrorPage(aURI, aURL, errorPage.get(),
                                        error, messageStr.get(),
                                        cssClass.get(), aFailedChannel);
    *aDisplayedErrorPage = NS_SUCCEEDED(loadedPage);
  } else {
    // The prompter reqires that our private window has a document (or it
    // asserts). Satisfy that assertion now since GetDoc will force
    // creation of one if it hasn't already been created.
    if (mScriptGlobal) {
      Unused << mScriptGlobal->GetDoc();
    }

    // Display a message box
    prompter->Alert(nullptr, messageStr.get());
  }

  return NS_OK;
}

#define PREF_SAFEBROWSING_ALLOWOVERRIDE "browser.safebrowsing.allowOverride"

nsresult
nsDocShell::LoadErrorPage(nsIURI* aURI, const char16_t* aURL,
                          const char* aErrorPage,
                          const char* aErrorType,
                          const char16_t* aDescription,
                          const char* aCSSClass,
                          nsIChannel* aFailedChannel)
{
#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aFailedChannel) {
      aFailedChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
           ("nsDocShell[%p]::LoadErrorPage(\"%s\", \"%s\", {...}, [%s])\n", this,
            aURI ? aURI->GetSpecOrDefault().get() : "",
            NS_ConvertUTF16toUTF8(aURL).get(),
            chanName.get()));
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
  if (aURI) {
    nsresult rv = aURI->GetSpec(url);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aURL) {
    CopyUTF16toUTF8(aURL, url);
  } else {
    return NS_ERROR_INVALID_POINTER;
  }

  // Create a URL to pass all the error information through to the page.

#undef SAFE_ESCAPE
#define SAFE_ESCAPE(output, input, params)                                     \
  if (NS_WARN_IF(!NS_Escape(input, output, params))) {                         \
    return NS_ERROR_OUT_OF_MEMORY;                                             \
  }

  nsCString escapedUrl, escapedError, escapedDescription,
    escapedCSSClass;
  SAFE_ESCAPE(escapedUrl, url, url_Path);
  SAFE_ESCAPE(escapedError, nsDependentCString(aErrorType), url_Path);
  SAFE_ESCAPE(escapedDescription,
              NS_ConvertUTF16toUTF8(aDescription), url_Path);
  if (aCSSClass) {
    nsCString cssClass(aCSSClass);
    SAFE_ESCAPE(escapedCSSClass, cssClass, url_Path);
  }
  nsCString errorPageUrl("about:");
  errorPageUrl.AppendASCII(aErrorPage);
  errorPageUrl.AppendLiteral("?e=");

  errorPageUrl.AppendASCII(escapedError.get());
  errorPageUrl.AppendLiteral("&u=");
  errorPageUrl.AppendASCII(escapedUrl.get());
  if ((strcmp(aErrorPage, "blocked") == 0) &&
      Preferences::GetBool(PREF_SAFEBROWSING_ALLOWOVERRIDE, true)) {
    errorPageUrl.AppendLiteral("&o=1");
  }
  if (!escapedCSSClass.IsEmpty()) {
    errorPageUrl.AppendLiteral("&s=");
    errorPageUrl.AppendASCII(escapedCSSClass.get());
  }
  errorPageUrl.AppendLiteral("&c=UTF-8");

  nsAutoCString frameType(FrameTypeToString(mFrameType));
  errorPageUrl.AppendLiteral("&f=");
  errorPageUrl.AppendASCII(frameType.get());

  nsCOMPtr<nsICaptivePortalService> cps = do_GetService(NS_CAPTIVEPORTAL_CID);
  int32_t cpsState;
  if (cps && NS_SUCCEEDED(cps->GetState(&cpsState)) &&
      cpsState == nsICaptivePortalService::LOCKED_PORTAL) {
    errorPageUrl.AppendLiteral("&captive=true");
  }

  // netError.xhtml's getDescription only handles the "d" parameter at the
  // end of the URL, so append it last.
  errorPageUrl.AppendLiteral("&d=");
  errorPageUrl.AppendASCII(escapedDescription.get());

  nsCOMPtr<nsIURI> errorPageURI;
  nsresult rv = NS_NewURI(getter_AddRefs(errorPageURI), errorPageUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return InternalLoad(errorPageURI, nullptr, Nothing(), false, nullptr,
                      mozilla::net::RP_Unset,
                      nsContentUtils::GetSystemPrincipal(), nullptr,
                      INTERNAL_LOAD_FLAGS_NONE, EmptyString(),
                      nullptr, VoidString(), nullptr, -1, nullptr,
                      LOAD_ERROR_PAGE, nullptr, true, VoidString(), this,
                      nullptr, false, nullptr, nullptr);
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

    if (!doc) {
      return NS_OK;
    }

    // Do not inherit owner from document
    uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;
    nsAutoString srcdoc;
    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIURI> originalURI;
    nsCOMPtr<nsIURI> resultPrincipalURI;
    bool loadReplace = false;

    nsIPrincipal* triggeringPrincipal = doc->NodePrincipal();
    nsAutoString contentTypeHint;
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

      nsCOMPtr<nsILoadInfo> loadInfo = chan->GetLoadInfo();
      if (loadInfo) {
        loadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
      }
    }

    MOZ_ASSERT(triggeringPrincipal, "Need a valid triggeringPrincipal");

    // Stack variables to ensure changes to the member variables don't affect to
    // the call.
    nsCOMPtr<nsIURI> currentURI = mCurrentURI;
    nsCOMPtr<nsIURI> referrerURI = mReferrerURI;
    uint32_t referrerPolicy = mReferrerPolicy;

    // Reload always rewrites result principal URI.
    Maybe<nsCOMPtr<nsIURI>> emplacedResultPrincipalURI;
    emplacedResultPrincipalURI.emplace(Move(resultPrincipalURI));
    rv = InternalLoad(currentURI,
                      originalURI,
                      emplacedResultPrincipalURI,
                      loadReplace,
                      referrerURI,
                      referrerPolicy,
                      triggeringPrincipal,
                      triggeringPrincipal,
                      flags,
                      EmptyString(),   // No window target
                      NS_LossyConvertUTF16toASCII(contentTypeHint).get(),
                      VoidString(),    // No forced download
                      nullptr,         // No post data
                      -1,              // No post data length
                      nullptr,         // No headers data
                      loadType,        // Load type
                      nullptr,         // No SHEntry
                      true,
                      srcdoc,          // srcdoc argument for iframe
                      this,            // For reloads we are the source
                      baseURI,
                      false,
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
    shEntry->SetResultPrincipalURI(nullptr);
    // shEntry's current triggering principal is whoever loaded that page initially.
    // But now we're doing another load of the page, via an API that is only exposed
    // to system code.  The triggering principal for this load should be the system
    // principal.
    shEntry->SetTriggeringPrincipal(nsContentUtils::GetSystemPrincipal());
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
  SetPositionAndSize(aX, aY, aWidth, aHeight, 0);

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

  mDisableMetaRefreshWhenInactive =
    Preferences::GetBool("browser.meta_refresh_when_inactive.disabled",
                         mDisableMetaRefreshWhenInactive);

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

  AssertOriginAttributesMatchPrivateBrowsing();

  if (!mIsBeingDestroyed) {
    nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
    if (serv) {
      const char* msg = mItemType == typeContent ?
        NS_WEBNAVIGATION_DESTROY : NS_CHROME_WEBNAVIGATION_DESTROY;
      serv->NotifyObservers(GetAsSupports(this), msg, nullptr);
    }
  }

  mIsBeingDestroyed = true;

  // Brak the cycle with the initial client, if present.
  mInitialClientSource.reset();

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

  if (UsePrivateBrowsing()) {
    mPrivateBrowsingId = 0;
    mOriginAttributes.SyncAttributesWithPrivateBrowsing(false);
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
nsDocShell::GetDevicePixelsPerDesktopPixel(double* aScale)
{
  if (mParentWidget) {
    *aScale = mParentWidget->GetDesktopToDeviceScale().scale;
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> ownerWindow(do_QueryInterface(mTreeOwner));
  if (ownerWindow) {
    return ownerWindow->GetDevicePixelsPerDesktopPixel(aScale);
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
nsDocShell::SetPositionDesktopPix(int32_t aX, int32_t aY)
{
  nsCOMPtr<nsIBaseWindow> ownerWindow(do_QueryInterface(mTreeOwner));
  if (ownerWindow) {
    return ownerWindow->SetPositionDesktopPix(aX, aY);
  }

  double scale = 1.0;
  GetDevicePixelsPerDesktopPixel(&scale);
  return SetPosition(NSToIntRound(aX * scale), NSToIntRound(aY * scale));
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
  return SetPositionAndSize(x, y, aWidth, aHeight,
                            aRepaint ? nsIBaseWindow::eRepaint : 0);
}

NS_IMETHODIMP
nsDocShell::GetSize(int32_t* aWidth, int32_t* aHeight)
{
  return GetPositionAndSize(nullptr, nullptr, aWidth, aHeight);
}

NS_IMETHODIMP
nsDocShell::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aWidth,
                               int32_t aHeight, uint32_t aFlags)
{
  mBounds.x = aX;
  mBounds.y = aY;
  mBounds.width = aWidth;
  mBounds.height = aHeight;

  // Hold strong ref, since SetBounds can make us null out mContentViewer
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  if (viewer) {
    uint32_t cvflags = (aFlags & nsIBaseWindow::eDelayResize) ?
                           nsIContentViewer::eDelayResize : 0;
    // XXX Border figured in here or is that handled elsewhere?
    nsresult rv = viewer->SetBoundsWithFlags(mBounds, cvflags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                               int32_t* aHeight)
{
  if (mParentWidget) {
    // ensure size is up-to-date if window has changed resolution
    LayoutDeviceIntRect r = mParentWidget->GetClientBounds();
    SetPositionAndSize(mBounds.x, mBounds.y, r.width, r.height, 0);
  }

  // We should really consider just getting this information from
  // our window instead of duplicating the storage and code...
  if (aWidth || aHeight) {
    // Caller wants to know our size; make sure to give them up to
    // date information.
    nsCOMPtr<nsIDocument> doc(do_GetInterface(GetAsSupports(mParent)));
    if (doc) {
      doc->FlushPendingNotifications(FlushType::Layout);
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

  // Clear prerender flag if necessary.
  if (mIsPrerendered && aIsActive) {
    MOZ_ASSERT(mPrerenderGlobalHistory.get());
    mIsPrerendered = false;
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    nsresult rv = NS_OK;
    if (history) {
      rv = mPrerenderGlobalHistory->ApplyChanges(history);
    } else if (mGlobalHistory) {
      rv = mPrerenderGlobalHistory->ApplyChanges(mGlobalHistory);
    }
    mPrerenderGlobalHistory = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
      if (aIsActive) {
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

  // Tell the nsDOMNavigationTiming about it
  RefPtr<nsDOMNavigationTiming> timing = mTiming;
  if (!timing && mContentViewer) {
    nsIDocument* doc = mContentViewer->GetDocument();
    if (doc) {
      timing = doc->GetNavigationTiming();
    }
  }
  if (timing) {
    timing->NotifyDocShellStateChanged(
      aIsActive ? nsDOMNavigationTiming::DocShellState::eActive
                : nsDOMNavigationTiming::DocShellState::eInactive);
  }

  // Recursively tell all of our children, but don't tell <iframe mozbrowser>
  // children; they handle their state separately.
  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> docshell = do_QueryObject(iter.GetNext());
    if (!docshell) {
      continue;
    }

    if (!docshell->GetIsMozBrowser()) {
      docshell->SetIsActive(aIsActive);
    }
  }

  // Restart or stop meta refresh timers if necessary
  if (mDisableMetaRefreshWhenInactive) {
    if (mIsActive) {
      ResumeRefreshURIs();
    } else {
      SuspendRefreshURIs();
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
nsDocShell::SetIsPrerendered()
{
  MOZ_ASSERT(!mIsPrerendered,
             "SetIsPrerendered() called on already prerendered docshell");
  SetIsActive(false);
  mIsPrerendered = true;
  mPrerenderGlobalHistory = mozilla::MakeUnique<PendingGlobalHistoryEntry>();
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
    NS_WARNING_ASSERTION(root.get() == static_cast<nsIDocShellTreeItem*>(this),
                         "Setting mMixedContentChannel on a docshell that is "
                         "not the root docshell");
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
nsDocShell::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTitle(const nsAString& aTitle)
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

  AssertOriginAttributesMatchPrivateBrowsing();
  if (mCurrentURI && mLoadType != LOAD_ERROR_PAGE) {
    UpdateGlobalHistoryTitle(mCurrentURI);
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
nsDocShell::RefreshURI(nsIURI* aURI, nsIPrincipal* aPrincipal,
                       int32_t aDelay, bool aRepeat,
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

  nsCOMPtr<nsITimerCallback> refreshTimer =
    new nsRefreshTimer(this, aURI, aPrincipal, aDelay, aRepeat, aMetaRefresh);

  uint32_t busyFlags = 0;
  GetBusyFlags(&busyFlags);

  if (!mRefreshURIList) {
    mRefreshURIList = nsArray::Create();
  }

  if (busyFlags & BUSY_FLAGS_BUSY || (!mIsActive && mDisableMetaRefreshWhenInactive)) {
    // We don't  want to create the timer right now. Instead queue up the request
    // and trigger the timer in EndPageLoad() or whenever we become active.
    mRefreshURIList->AppendElement(refreshTimer);
  } else {
    // There is no page loading going on right now.  Create the
    // timer and fire it right away.
    nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
    NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

    nsCOMPtr<nsITimer> timer;
    MOZ_TRY_VAR(timer,
                NS_NewTimerWithCallback(refreshTimer, aDelay, nsITimer::TYPE_ONE_SHOT,
                                        win->TabGroup()->EventTargetFor(TaskCategory::Network)));

    mRefreshURIList->AppendElement(timer);  // owning timer ref
  }
  return NS_OK;
}

nsresult
nsDocShell::ForceRefreshURIFromTimer(nsIURI* aURI,
                                     nsIPrincipal* aPrincipal,
                                     int32_t aDelay,
                                     bool aMetaRefresh,
                                     nsITimer* aTimer)
{
  NS_PRECONDITION(aTimer, "Must have a timer here");

  // Remove aTimer from mRefreshURIList if needed
  if (mRefreshURIList) {
    uint32_t n = 0;
    mRefreshURIList->GetLength(&n);

    for (uint32_t i = 0; i < n; ++i) {
      nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
      if (timer == aTimer) {
        mRefreshURIList->RemoveElementAt(i);
        break;
      }
    }
  }

  return ForceRefreshURI(aURI, aPrincipal, aDelay, aMetaRefresh);
}

NS_IMETHODIMP
nsDocShell::ForceRefreshURI(nsIURI* aURI, nsIPrincipal* aPrincipal, int32_t aDelay, bool aMetaRefresh)
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

  /* Don't ever "guess" on which principal to use to avoid picking
   * the current principal.
   */
  loadInfo->SetPrincipalIsExplicit(true);

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

  // If the principal is null, the refresh will have a triggeringPrincipal
  // derived from the referrer URI, or will be set to the system principal
  // if there is no refererrer. See LoadURI()
  if (aPrincipal) {
    loadInfo->SetTriggeringPrincipal(aPrincipal);
  }

  /*
   * LoadURI(...) will cancel all refresh timers... This causes the
   * Timer and its refreshData instance to be released...
   */
  LoadURI(aURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL, true);

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

        rv = RefreshURI(uri, aPrincipal, seconds * 1000, false, true);
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
DoCancelRefreshURITimers(nsIMutableArray* aTimerList)
{
  if (!aTimerList) {
    return;
  }

  uint32_t n = 0;
  aTimerList->GetLength(&n);

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
  nsresult rv = mRefreshURIList->GetLength(&count);
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
    mRefreshURIList->GetLength(&n);

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
  mRefreshURIList->GetLength(&n);

  while (n) {
    nsCOMPtr<nsITimerCallback> refreshInfo =
        do_QueryElementAt(mRefreshURIList, --n);

    if (refreshInfo) {
      // This is the nsRefreshTimer object, waiting to be
      // setup in a timer object and fired.
      // Create the timer and  trigger it.
      uint32_t delay =
        static_cast<nsRefreshTimer*>(
          static_cast<nsITimerCallback*>(refreshInfo))->GetDelay();
      nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
      if (win) {
        nsCOMPtr<nsITimer> timer;
        NS_NewTimerWithCallback(getter_AddRefs(timer),
                                refreshInfo, delay, nsITimer::TYPE_ONE_SHOT,
                                win->TabGroup()->EventTargetFor(TaskCategory::Network));

        if (timer) {
          // Replace the nsRefreshTimer element in the queue with
          // its corresponding timer object, so that in case another
          // load comes through before the timer can go off, the timer will
          // get cancelled in CancelRefreshURITimer()
          mRefreshURIList->ReplaceElementAt(timer, n);
        }
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
       mLoadType == LOAD_RELOAD_CHARSET_CHANGE ||
       mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE ||
       mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE)) {
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
      mozilla::Unused << MaybeInitTiming();
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
        AddToSessionHistory(uri, wcwgChannel, nullptr, nullptr, false,
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
      if (nsContentUtils::UseActivityCursor()) {
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
    if (nsContentUtils::UseActivityCursor()) {
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

  // If mixed content is allowed for the old channel, we forward
  // the permission to the new channel if it has the same origin
  // as the old one.
  if (mMixedContentChannel && mMixedContentChannel == aOldChannel) {
    nsresult rv = nsContentUtils::CheckSameOrigin(mMixedContentChannel, aNewChannel);
    if (NS_SUCCEEDED(rv)) {
      SetMixedContentChannel(aNewChannel); // Same origin: forward permission.
    } else {
      SetMixedContentChannel(nullptr); // Different origin: clear mMixedContentChannel.
    }
  }

  if (!(aStateFlags & STATE_IS_DOCUMENT)) {
    return;  // not a toplevel document
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));
  if (!oldURI || !newURI) {
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
      Unused << httpChannel->GetResponseStatus(&responseStatus);
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
          NS_ShouldCheckAppCache(principal));
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

  // Make sure to discard the initial client if we never created the initial
  // about:blank document.  Do this before possibly returning from the method
  // due to an error.
  mInitialClientSource.reset();

  nsCOMPtr<nsIConsoleReportCollector> reporter = do_QueryInterface(aChannel);
  if (reporter) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
      reporter->FlushConsoleReports(loadGroup);
    } else {
      reporter->FlushConsoleReports(GetDocument());
    }
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
  mozilla::Unused << loadingSHE; // XXX: Not sure if we need this anymore

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
  if (mIsActive || !mDisableMetaRefreshWhenInactive)
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
        aStatus == NS_ERROR_FILE_ACCESS_DENIED ||
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
      nsPIDOMWindowOuter* thisWindow = GetWindow();
      if (!thisWindow) {
        return NS_OK;
      }

      frameElement = thisWindow->GetFrameElement();
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

          if (doCreateAlternate) {
            // Skip doing this if our channel was redirected, because we
            // shouldn't be guessing things about the post-redirect URI.
            nsLoadFlags loadFlags = 0;
            if (NS_FAILED(aChannel->GetLoadFlags(&loadFlags)) ||
                (loadFlags & nsIChannel::LOAD_REPLACE)) {
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

          nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
          nsCOMPtr<nsIPrincipal> triggeringPrincipal = loadInfo
            ? loadInfo->TriggeringPrincipal()
            : nsContentUtils::GetSystemPrincipal();
          return LoadURI(newSpecW.get(),       // URI string
                         LOAD_FLAGS_NONE,      // Load flags
                         nullptr,              // Referring URI
                         newPostData,          // Post data stream
                         nullptr,              // Headers stream
                         triggeringPrincipal); // TriggeringPrincipal
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
               aStatus == NS_ERROR_HARMFUL_URI ||
               aStatus == NS_ERROR_UNSAFE_CONTENT_TYPE ||
               aStatus == NS_ERROR_REMOTE_XUL ||
               aStatus == NS_ERROR_INTERCEPTION_FAILED ||
               aStatus == NS_ERROR_NET_INADEQUATE_SECURITY ||
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
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      mozilla::net::PredictorLearnRedirect(url, aChannel,
                                           loadInfo->GetOriginAttributes());
    }
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
    if (nsCOMPtr<nsPIDOMWindowOuter> domWin = GetWindow()) {
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

    // Documents created using EnsureContentViewer may be transient
    // placeholders created by framescripts before content has a chance to
    // load. In some cases, window.open(..., "noopener") will create such a
    // document (in a new TabGroup) and then synchronously tear it down, firing
    // a "pagehide" event. Doing so violates our assertions about
    // DocGroups. It's easier to silence the assertion here than to avoid
    // creating the extra document.
    doc->IgnoreDocGroupMismatches();
  }

  return rv;
}

nsresult
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal* aPrincipal,
                                          nsIURI* aBaseURI,
                                          bool aTryToSaveOldPresentation,
                                          bool aCheckPermitUnload)
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

  // mContentViewer->PermitUnload may release |this| docshell.
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

  AutoRestore<bool> creatingDocument(mCreatingDocument);
  mCreatingDocument = true;

  if (aPrincipal && !nsContentUtils::IsSystemPrincipal(aPrincipal) &&
      mItemType != typeChrome) {
    MOZ_ASSERT(aPrincipal->OriginAttributesRef() == mOriginAttributes);
  }

  // Make sure timing is created.  But first record whether we had it
  // already, so we don't clobber the timing for an in-progress load.
  bool hadTiming = mTiming;
  bool toBeReset = MaybeInitTiming();
  if (mContentViewer) {
    if (aCheckPermitUnload) {
      // We've got a content viewer already. Make sure the user
      // permits us to discard the current document and replace it
      // with about:blank. And also ensure we fire the unload events
      // in the current document.

      // Unload gets fired first for
      // document loaded from the session history.
      mTiming->NotifyBeforeUnload();

      bool okToUnload;
      rv = mContentViewer->PermitUnload(&okToUnload);

      if (NS_SUCCEEDED(rv) && !okToUnload) {
        // The user chose not to unload the page, interrupt the load.
        MaybeResetInitTiming(toBeReset);
        return NS_ERROR_FAILURE;
      }
      if (mTiming) {
        mTiming->NotifyUnloadAccepted(mCurrentURI);
      }
    }

    mSavingOldViewer = aTryToSaveOldPresentation &&
                       CanSavePresentation(LOAD_NORMAL, nullptr, nullptr);

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
      if (aPrincipal) {
        principal = NullPrincipal::CreateWithInheritedAttributes(aPrincipal);
      } else {
        principal = NullPrincipal::CreateWithInheritedAttributes(this);
      }
    } else {
      principal = aPrincipal;
    }

    MaybeCreateInitialClientSource(principal);

    // generate (about:blank) document to load
    blankDoc = nsContentDLF::CreateBlankDocument(mLoadGroup, principal, this);
    if (blankDoc) {
      // Hack: set the base URI manually, since this document never
      // got Reset() with a channel.
      blankDoc->SetBaseURI(aBaseURI);

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

NS_IMETHODIMP
nsDocShell::ForceCreateAboutBlankContentViewer(nsIPrincipal* aPrincipal)
{
  return CreateAboutBlankContentViewer(aPrincipal, nullptr, true, false);
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

  // Don't cache the content viewer if we're in a subframe.
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetSameTypeParent(getter_AddRefs(root));
  if (root && root != this) {
    return false;  // this is a subframe load
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
  nsresult rv = DispatchToTabGroup(TaskCategory::Other,
                                   RefPtr<RestorePresentationEvent>(evt).forget());
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

bool
nsDocShell::SandboxFlagsImplyCookies(const uint32_t &aSandboxFlags)
{
  return (aSandboxFlags & (SANDBOXED_ORIGIN | SANDBOXED_SCRIPTS)) == 0;
}

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
  float overrideDPPX = 0.0f;

  bool styleDisabled = false;
  if (oldCv && newCv) {
    oldCv->GetMinFontSize(&minFontSize);
    oldCv->GetTextZoom(&textZoom);
    oldCv->GetFullZoom(&pageZoom);
    oldCv->GetOverrideDPPX(&overrideDPPX);
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
      frame ? frame->PresShell()->GetDocument() : nullptr;
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
  nsCOMPtr<nsIMutableArray> refreshURIList;
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
    nsCOMPtr<nsIMutableArray> refreshURIs;
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
    newCv->SetOverrideDPPX(overrideDPPX);
    newCv->SetAuthorStyleDisabled(styleDisabled);
  }

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
  if (document) {
    RefPtr<nsDocShell> parent = GetParentDocshell();
    if (parent) {
      nsCOMPtr<nsIDocument> d = parent->GetDocument();
      if (d) {
        if (d->EventHandlingSuppressed()) {
          document->SuppressEventHandling(d->EventHandlingSuppressed());
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
  nsCOMPtr<nsPIDOMWindowOuter> privWin = GetWindow();
  NS_ASSERTION(privWin, "could not get nsPIDOMWindow interface");

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
    // child inherits our mIsActive, mIsPrerendered and mPrivateBrowsingId,
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

  // Make sure to restore the window state after adding the child shells back
  // to the tree.  This is necessary for Thaw() and Resume() to propagate
  // properly.
  rv = privWin->RestoreWindowState(windowState);
  NS_ENSURE_SUCCESS(rv, rv);

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
  } else {
    rootViewParent = nullptr;
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

  nsCOMPtr<nsPIDOMWindowInner> privWinInner = privWin->GetCurrentInnerWindow();

  // If parent is suspended, increase suspension count.
  // This can't be done as early as event suppression since this
  // depends on docshell tree.
  privWinInner->SyncStateFromParentWindow();

  // Now that all of the child docshells have been put into place, we can
  // restart the timers for the window and all of the child frames.
  privWinInner->Resume();

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
        shell->GetRootScrollFrameAsScrollable();
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

  if (!mTreeOwner) {
    // If we don't have a tree owner, then we're in the process of being
    // destroyed. Rather than continue trying to load something, just give up.
    return NS_ERROR_DOCSHELL_DYING;
  }

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
  bool errorOnLocationChangeNeeded = false;
  nsCOMPtr<nsIChannel> failedChannel = mFailedChannel;
  nsCOMPtr<nsIURI> failedURI;

  if (mLoadType == LOAD_ERROR_PAGE) {
    // We need to set the SH entry and our current URI here and not
    // at the moment we load the page. We want the same behavior
    // of Stop() as for a normal page load. See bug 514232 for details.

    // Revert mLoadType to load type to state the page load failed,
    // following function calls need it.
    mLoadType = mFailedLoadType;


    nsIDocument* doc = viewer->GetDocument();
    if (doc) {
      doc->SetFailedChannel(failedChannel);
    }

    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    if (failedChannel) {
      // Make sure we have a URI to set currentURI.
      NS_GetFinalChannelURI(failedChannel, getter_AddRefs(failedURI));
    }
     else {
       // if there is no failed channel we have to explicitly provide
       // a triggeringPrincipal for the history entry.
       triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
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
      errorOnLocationChangeNeeded = OnNewURI(
        failedURI, failedChannel, triggeringPrincipal,
        nullptr, mLoadType, false, false, false);
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
    if (SandboxFlagsImplyCookies(mSandboxFlags)) {
      loadFlags |= nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE;
    }

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

  if (errorOnLocationChangeNeeded){
    FireOnLocationChange(this, failedChannel, failedURI,
                         LOCATION_CHANGE_ERROR_PAGE);
  } else if (onLocationChangeNeeded) {
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

  const Encoding* forceCharset = nullptr;
  const Encoding* hintCharset = nullptr;
  int32_t hintCharsetSource;
  int32_t minFontSize;
  float textZoom;
  float pageZoom;
  float overrideDPPX;
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
        forceCharset = oldCv->GetForceCharset();
        hintCharset = oldCv->GetHintCharset();
        NS_ENSURE_SUCCESS(oldCv->GetHintCharacterSetSource(&hintCharsetSource),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetMinFontSize(&minFontSize),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetTextZoom(&textZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetFullZoom(&pageZoom),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetOverrideDPPX(&overrideDPPX),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetAuthorStyleDisabled(&styleDisabled),
                          NS_ERROR_FAILURE);
      }
    }
  }

  nscolor bgcolor = NS_RGBA(0, 0, 0, 0);
  // Ensure that the content viewer is destroyed *after* the GC - bug 71515
  nsCOMPtr<nsIContentViewer> contentViewer = mContentViewer;
  if (contentViewer) {
    // Stop any activity that may be happening in the old document before
    // releasing it...
    contentViewer->Stop();

    // Try to extract the canvas background color from the old
    // presentation shell, so we can use it for the next document.
    nsCOMPtr<nsIPresShell> shell;
    contentViewer->GetPresShell(getter_AddRefs(shell));

    if (shell) {
      bgcolor = shell->GetCanvasBackground();
    }

    contentViewer->Close(mSavingOldViewer ? mOSHE.get() : nullptr);
    aNewViewer->SetPreviousViewer(contentViewer);
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
    newCv->SetForceCharset(forceCharset);
    newCv->SetHintCharset(hintCharset);
    NS_ENSURE_SUCCESS(newCv->SetHintCharacterSetSource(hintCharsetSource),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetMinFontSize(minFontSize),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetTextZoom(textZoom),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetFullZoom(pageZoom),
                      NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetOverrideDPPX(overrideDPPX),
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

void
nsDocShell::CopyFavicon(nsIURI* aOldURI,
                        nsIURI* aNewURI,
                        nsIPrincipal* aLoadingPrincipal,
                        bool aInPrivateBrowsing)
{
  if (XRE_IsContentProcess()) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (contentChild) {
      mozilla::ipc::URIParams oldURI, newURI;
      SerializeURI(aOldURI, oldURI);
      SerializeURI(aNewURI, newURI);
      contentChild->SendCopyFavicon(oldURI, newURI,
                                    IPC::Principal(aLoadingPrincipal),
                                    aInPrivateBrowsing);
    }
    return;
  }

#ifdef MOZ_PLACES
  nsCOMPtr<mozIAsyncFavicons> favSvc =
    do_GetService("@mozilla.org/browser/favicon-service;1");
  if (favSvc) {
    favSvc->CopyFavicons(aOldURI, aNewURI,
      aInPrivateBrowsing ? nsIFaviconService::FAVICON_LOAD_PRIVATE
                         : nsIFaviconService::FAVICON_LOAD_NON_PRIVATE, nullptr);
  }
#endif
}

class InternalLoadEvent : public Runnable
{
public:
  InternalLoadEvent(nsDocShell* aDocShell,
                    nsIURI* aURI,
                    nsIURI* aOriginalURI,
                    Maybe<nsCOMPtr<nsIURI>> const& aResultPrincipalURI,
                    bool aLoadReplace,
                    nsIURI* aReferrer, uint32_t aReferrerPolicy,
                    nsIPrincipal* aTriggeringPrincipal,
                    nsIPrincipal* aPrincipalToInherit,
                    uint32_t aFlags,
                    const char* aTypeHint,
                    nsIInputStream* aPostData,
                    int64_t aPostDataLength,
                    nsIInputStream* aHeadersData,
                    uint32_t aLoadType,
                    nsISHEntry* aSHEntry,
                    bool aFirstParty,
                    const nsAString& aSrcdoc,
                    nsIDocShell* aSourceDocShell,
                    nsIURI* aBaseURI,
                    bool aCheckForPrerender)
    : mozilla::Runnable("InternalLoadEvent")
    , mSrcdoc(aSrcdoc)
    , mDocShell(aDocShell)
    , mURI(aURI)
    , mOriginalURI(aOriginalURI)
    , mResultPrincipalURI(aResultPrincipalURI)
    , mLoadReplace(aLoadReplace)
    , mReferrer(aReferrer)
    , mReferrerPolicy(aReferrerPolicy)
    , mTriggeringPrincipal(aTriggeringPrincipal)
    , mPrincipalToInherit(aPrincipalToInherit)
    , mPostData(aPostData)
    , mPostDataLength(aPostDataLength)
    , mHeadersData(aHeadersData)
    , mSHEntry(aSHEntry)
    , mFlags(aFlags)
    , mLoadType(aLoadType)
    , mFirstParty(aFirstParty)
    , mSourceDocShell(aSourceDocShell)
    , mBaseURI(aBaseURI)
    , mCheckForPrerender(aCheckForPrerender)
  {
    // Make sure to keep null things null as needed
    if (aTypeHint) {
      mTypeHint = aTypeHint;
    } else {
      mTypeHint.SetIsVoid(true);
    }
  }

  NS_IMETHOD
  Run() override
  {
    return mDocShell->InternalLoad(mURI, mOriginalURI, mResultPrincipalURI,
                                   mLoadReplace,
                                   mReferrer,
                                   mReferrerPolicy,
                                   mTriggeringPrincipal, mPrincipalToInherit,
                                   mFlags, EmptyString(),
                                   mTypeHint.IsVoid() ? nullptr
                                                      : mTypeHint.get(),
                                   VoidString(), mPostData, mPostDataLength,
                                   mHeadersData, mLoadType, mSHEntry,
                                   mFirstParty, mSrcdoc, mSourceDocShell,
                                   mBaseURI, mCheckForPrerender, nullptr,
                                   nullptr);
  }

private:
  nsCString mTypeHint;
  nsString mSrcdoc;

  RefPtr<nsDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  Maybe<nsCOMPtr<nsIURI>> mResultPrincipalURI;
  bool mLoadReplace;
  nsCOMPtr<nsIURI> mReferrer;
  uint32_t mReferrerPolicy;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipalToInherit;
  nsCOMPtr<nsIInputStream> mPostData;
  int64_t mPostDataLength;
  nsCOMPtr<nsIInputStream> mHeadersData;
  nsCOMPtr<nsISHEntry> mSHEntry;
  uint32_t mFlags;
  uint32_t mLoadType;
  bool mFirstParty;
  nsCOMPtr<nsIDocShell> mSourceDocShell;
  nsCOMPtr<nsIURI> mBaseURI;
  bool mCheckForPrerender;
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
  nsCOMPtr<nsIPrincipal> prin =
    BasePrincipal::CreateCodebasePrincipal(aReferrer, mOriginAttributes);
  prin.forget(aResult);

  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::InternalLoad(nsIURI* aURI,
                         nsIURI* aOriginalURI,
                         Maybe<nsCOMPtr<nsIURI>> const& aResultPrincipalURI,
                         bool aLoadReplace,
                         nsIURI* aReferrer,
                         uint32_t aReferrerPolicy,
                         nsIPrincipal* aTriggeringPrincipal,
                         nsIPrincipal* aPrincipalToInherit,
                         uint32_t aFlags,
                         const nsAString& aWindowTarget,
                         const char* aTypeHint,
                         const nsAString& aFileName,
                         nsIInputStream* aPostData,
                         int64_t aPostDataLength,
                         nsIInputStream* aHeadersData,
                         uint32_t aLoadType,
                         nsISHEntry* aSHEntry,
                         bool aFirstParty,
                         const nsAString& aSrcdoc,
                         nsIDocShell* aSourceDocShell,
                         nsIURI* aBaseURI,
                         bool aCheckForPrerender,
                         nsIDocShell** aDocShell,
                         nsIRequest** aRequest)
{
  MOZ_ASSERT(aTriggeringPrincipal, "need a valid TriggeringPrincipal");

  nsresult rv = NS_OK;
  mOriginalUriString.Truncate();

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug,
          ("DOCSHELL %p InternalLoad %s\n",
           this, aURI ? aURI->GetSpecOrDefault().get() : ""));
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

  rv = EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return rv;
  }

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

  bool isTargetTopLevelDocShell = false;
  nsCOMPtr<nsIDocShell> targetDocShell;
  if (!aWindowTarget.IsEmpty()) {
    // Locate the target DocShell.
    nsCOMPtr<nsIDocShellTreeItem> targetItem;
    // Only _self, _parent, and _top are supported in noopener case.  But we
    // have to be careful to not apply that to the noreferrer case.  See bug
    // 1358469.
    bool allowNamedTarget = !(aFlags & INTERNAL_LOAD_FLAGS_NO_OPENER) ||
                            (aFlags & INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER);
    if (allowNamedTarget ||
        aWindowTarget.LowerCaseEqualsLiteral("_self") ||
        aWindowTarget.LowerCaseEqualsLiteral("_parent") ||
        aWindowTarget.LowerCaseEqualsLiteral("_top")) {
      rv = FindItemWithName(aWindowTarget, nullptr, this, false,
                            getter_AddRefs(targetItem));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    targetDocShell = do_QueryInterface(targetItem);
    if (targetDocShell) {
      // If the targetDocShell and the rootDocShell are the same, then the
      // targetDocShell is the top level document and hence we should
      // consider this TYPE_DOCUMENT
      //
      // For example:
      // 1. target="_top"
      // 2. target="_parent", where this docshell is in the 2nd level of
      //    docshell tree.
      nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
      targetDocShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
      NS_ASSERTION(sameTypeRoot,
                   "No document shell root tree item from targetDocShell!");
      nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(sameTypeRoot);
      NS_ASSERTION(rootShell,
                   "No root docshell from document shell root tree item.");
      isTargetTopLevelDocShell = targetDocShell == rootShell;
    } else {
      // If the targetDocShell doesn't exist, then this is a new docShell
      // and we should consider this a TYPE_DOCUMENT load
      //
      // For example, when target="_blank"
      isTargetTopLevelDocShell = true;
    }
  }

  // The contentType will be INTERNAL_(I)FRAME if:
  // 1. This docshell is for iframe.
  // 2. AND aWindowTarget is not a new window, nor a top-level window.
  //
  // This variable will be used when we call NS_CheckContentLoadPolicy, and
  // later when we call DoURILoad.
  uint32_t contentType;
  if (IsFrame() && !isTargetTopLevelDocShell) {
    nsCOMPtr<Element> requestingElement =
      mScriptGlobal->AsOuter()->GetFrameElementInternal();
    if (requestingElement) {
      contentType = requestingElement->IsHTMLElement(nsGkAtoms::iframe) ?
        nsIContentPolicy::TYPE_INTERNAL_IFRAME : nsIContentPolicy::TYPE_INTERNAL_FRAME;
    } else {
      // If we have lost our frame element by now, just assume we're
      // an iframe since that's more common.
      contentType = nsIContentPolicy::TYPE_INTERNAL_IFRAME;
    }
  } else {
    contentType = nsIContentPolicy::TYPE_DOCUMENT;
    isTargetTopLevelDocShell = true;
  }

  // If there's no targetDocShell, that means we are about to create a new
  // window (or aWindowTarget is empty). Perform a content policy check before
  // creating the window. Please note for all other docshell loads
  // content policy checks are performed within the contentSecurityManager
  // when the channel is about to be openend.
  if (!targetDocShell && !aWindowTarget.IsEmpty()) {
    MOZ_ASSERT(contentType == nsIContentPolicy::TYPE_DOCUMENT,
               "opening a new window requires type to be TYPE_DOCUMENT");

    nsISupports* requestingContext = nullptr;
    if (XRE_IsContentProcess()) {
      // In e10s the child process doesn't have access to the element that
      // contains the browsing context (because that element is in the chrome
      // process). So we just pass mScriptGlobal.
      requestingContext = ToSupports(mScriptGlobal);
    } else {
      // This is for loading non-e10s tabs and toplevel windows of various
      // sorts.
      // For the toplevel window cases, requestingElement will be null.
      nsCOMPtr<Element> requestingElement =
        mScriptGlobal->AsOuter()->GetFrameElementInternal();
      requestingContext = requestingElement;
    }

    // Since Content Policy checks are performed within docShell as well as
    // the ContentSecurityManager we need a reliable way to let certain
    // nsIContentPolicy consumers ignore duplicate calls. Let's use the 'extra'
    // argument to pass a specific identifier.
    nsCOMPtr<nsISupportsString> extraStr =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_NAMED_LITERAL_STRING(msg, "conPolCheckFromDocShell");
    rv = extraStr->SetData(msg);
    NS_ENSURE_SUCCESS(rv, rv);

    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(contentType,
                                   aURI,
                                   // This is a top-level load, so the loading
                                   // principal is null.
                                   nullptr,
                                   aTriggeringPrincipal,
                                   requestingContext,
                                   EmptyCString(),  // mime guess
                                   extraStr,  // extra
                                   &shouldLoad);

    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
      if (NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) {
        return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
      }

      return NS_ERROR_CONTENT_BLOCKED;
    }

    // If HSTS priming was set by nsMixedContentBlocker::ShouldLoad, and we
    // would block due to mixed content, go ahead and block here. If we try to
    // proceed with priming, we will error out later on.
    nsCOMPtr<nsIDocShell> docShell = NS_CP_GetDocShellFromContext(requestingContext);
    // When loading toplevel windows, requestingContext can be null.  We don't
    // really care about HSTS in that situation, though; loads in toplevel
    // windows should all be browser UI.
    if (docShell) {
      nsIDocument* document = docShell->GetDocument();
      NS_ENSURE_TRUE(document, NS_OK);

      HSTSPrimingState state = document->GetHSTSPrimingStateForLocation(aURI);
      if (state == HSTSPrimingState::eHSTS_PRIMING_BLOCK) {
        // HSTS Priming currently disabled for InternalLoad, so we need to clear
        // the location that was added by nsMixedContentBlocker::ShouldLoad
        // Bug 1269815 will address images loaded via InternalLoad
        document->ClearHSTSPrimingLocation(aURI);
        return NS_ERROR_CONTENT_BLOCKED;
      }
    }
  }

  nsCOMPtr<nsIPrincipal> principalToInherit = aPrincipalToInherit;
  //
  // Get a principal from the current document if necessary.  Note that we only
  // do this for URIs that inherit a security context and local file URIs;
  // in particular we do NOT do this for about:blank.  This way, random
  // about:blank loads that have no principal (which basically means they were
  // done by someone from chrome manually messing with our nsIWebNavigation
  // or by C++ setting document.location) don't get a funky principal.  If
  // callers want something interesting to happen with the about:blank
  // principal in this case, they should pass aPrincipalToInherit in.
  //
  {
    bool inherits;
    // One more twist: Don't inherit the principal for external loads.
    if (aLoadType != LOAD_NORMAL_EXTERNAL && !principalToInherit &&
        (aFlags & INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL) &&
         NS_SUCCEEDED(nsContentUtils::URIInheritsSecurityContext(aURI,
                                                                 &inherits)) &&
         inherits) {
      principalToInherit = GetInheritedPrincipal(true);
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

  nsIDocument* doc = mContentViewer ? mContentViewer->GetDocument()
                                    : nullptr;

  const bool isDocumentAuxSandboxed = doc &&
    (doc->GetSandboxFlags() & SANDBOXED_AUXILIARY_NAVIGATION);

  if (aURI && mLoadURIDelegate &&
      (!targetDocShell || targetDocShell == static_cast<nsIDocShell*>(this))) {
    // Dispatch only load requests for the current or a new window to the
    // delegate, e.g., to allow for GeckoView apps to handle the load event
    // outside of Gecko.
    const int where = (aWindowTarget.IsEmpty() || targetDocShell)
                      ? nsIBrowserDOMWindow::OPEN_CURRENTWINDOW
                      : nsIBrowserDOMWindow::OPEN_NEW;

    if (where == nsIBrowserDOMWindow::OPEN_NEW && isDocumentAuxSandboxed) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    if (NS_SUCCEEDED(mLoadURIDelegate->LoadURI(aURI, where, aFlags,
                                               aTriggeringPrincipal))) {
      // The request has been handled, nothing to do here.
      return NS_OK;
    }
  }

  //
  // Resolve the window target before going any further...
  // If the load has been targeted to another DocShell, then transfer the
  // load to it...
  //
  if (!aWindowTarget.IsEmpty()) {
    // We've already done our owner-inheriting.  Mask out that bit, so we
    // don't try inheriting an owner from the target window if we came up
    // with a null owner above.
    aFlags = aFlags & ~INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL;

    bool isNewWindow = false;
    if (!targetDocShell) {
      // If the docshell's document is sandboxed, only open a new window
      // if the document's SANDBOXED_AUXILLARY_NAVIGATION flag is not set.
      // (i.e. if allow-popups is specified)
      NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
      if (isDocumentAuxSandboxed) {
        return NS_ERROR_DOM_INVALID_ACCESS_ERR;
      }

      nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
      NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

      nsCOMPtr<nsPIDOMWindowOuter> newWin;
      nsAutoCString spec;
      if (aURI) {
        aURI->GetSpec(spec);
      }
      // If we are a noopener load, we just hand the whole thing over to our
      // window.
      if (aFlags & INTERNAL_LOAD_FLAGS_NO_OPENER) {
        // Various asserts that we know to hold because NO_OPENER loads can only
        // happen for links.
        MOZ_ASSERT(!aLoadReplace);
        MOZ_ASSERT(aPrincipalToInherit == aTriggeringPrincipal);
        MOZ_ASSERT(aFlags == INTERNAL_LOAD_FLAGS_NO_OPENER ||
                   aFlags == (INTERNAL_LOAD_FLAGS_NO_OPENER |
                              INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER));
        MOZ_ASSERT(!aPostData);
        MOZ_ASSERT(!aHeadersData);
        // If OnLinkClickSync was invoked inside the onload handler, the load
        // type would be set to LOAD_NORMAL_REPLACE; otherwise it should be
        // LOAD_LINK.
        MOZ_ASSERT(aLoadType == LOAD_LINK ||
                   aLoadType == LOAD_NORMAL_REPLACE);
        MOZ_ASSERT(!aSHEntry);
        MOZ_ASSERT(aFirstParty); // Windowwatcher will assume this.

        nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
        rv = CreateLoadInfo(getter_AddRefs(loadInfo));
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Set up our loadinfo so it will do the load as much like we would have
        // as possible.
        loadInfo->SetReferrer(aReferrer);
        loadInfo->SetReferrerPolicy(aReferrerPolicy);
        loadInfo->SetSendReferrer(!(aFlags &
                                    INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER));
        loadInfo->SetOriginalURI(aOriginalURI);
        SetMaybeResultPrincipalURI(loadInfo, aResultPrincipalURI);
        loadInfo->SetLoadReplace(aLoadReplace);
        loadInfo->SetTriggeringPrincipal(aTriggeringPrincipal);
        loadInfo->SetInheritPrincipal(
          aFlags & INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL);
        // Explicit principal because we do not want any guesses as to what the
        // principal to inherit is: it should be aTriggeringPrincipal.
        loadInfo->SetPrincipalIsExplicit(true);
        loadInfo->SetLoadType(ConvertLoadTypeToDocShellInfoLoadType(LOAD_LINK));
        loadInfo->SetForceAllowDataURI(aFlags & INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI);

        rv = win->Open(NS_ConvertUTF8toUTF16(spec),
                       aWindowTarget, // window name
                       EmptyString(), // Features
                       loadInfo,
                       true, // aForceNoOpener
                       getter_AddRefs(newWin));
        MOZ_ASSERT(!newWin);
        return rv;
      }

      rv = win->OpenNoNavigate(NS_ConvertUTF8toUTF16(spec),
                               aWindowTarget,  // window name
                               EmptyString(), // Features
                               getter_AddRefs(newWin));

      // In some cases the Open call doesn't actually result in a new
      // window being opened.  We can detect these cases by examining the
      // document in |newWin|, if any.
      nsCOMPtr<nsPIDOMWindowOuter> piNewWin = do_QueryInterface(newWin);
      if (piNewWin) {
        nsCOMPtr<nsIDocument> newDoc = piNewWin->GetExtantDoc();
        if (!newDoc || newDoc->IsInitialDocument()) {
          isNewWindow = true;
          aFlags |= INTERNAL_LOAD_FLAGS_FIRST_LOAD;
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
                                        aResultPrincipalURI,
                                        aLoadReplace,
                                        aReferrer,
                                        aReferrerPolicy,
                                        aTriggeringPrincipal,
                                        principalToInherit,
                                        aFlags,
                                        EmptyString(),   // No window target
                                        aTypeHint,
                                        VoidString(),    // No forced download
                                        aPostData,
                                        aPostDataLength,
                                        aHeadersData,
                                        aLoadType,
                                        aSHEntry,
                                        aFirstParty,
                                        aSrcdoc,
                                        aSourceDocShell,
                                        aBaseURI,
                                        aCheckForPrerender,
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
          if (nsCOMPtr<nsPIDOMWindowOuter> domWin = targetDocShell->GetWindow()) {
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

      if (NS_SUCCEEDED(rv)) {
        // Switch to target tab if we're currently focused window.
        // Take loadDivertedInBackground into account so the behavior would be
        // the same as how the tab first opened.
        bool isTargetActive = false;
        targetDocShell->GetIsActive(&isTargetActive);
        nsCOMPtr<nsPIDOMWindowOuter> domWin = targetDocShell->GetWindow();
        if (mIsActive && !isTargetActive && domWin &&
            !Preferences::GetBool("browser.tabs.loadDivertedInBackground", false)) {
          if (NS_FAILED(nsContentUtils::DispatchFocusChromeEvent(domWin))) {
            return NS_ERROR_FAILURE;
          }
        }
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
      NS_PRECONDITION(aWindowTarget.IsEmpty(),
                      "Shouldn't have a window target here!");

      // If this is a replace load, make whatever load triggered
      // the unload event also a replace load, so we don't
      // create extra history entries.
      if (LOAD_TYPE_HAS_FLAGS(aLoadType, LOAD_FLAGS_REPLACE_HISTORY)) {
        mLoadType = LOAD_NORMAL_REPLACE;
      }

      // Do this asynchronously
      nsCOMPtr<nsIRunnable> ev =
        new InternalLoadEvent(this, aURI, aOriginalURI, aResultPrincipalURI,
                              aLoadReplace, aReferrer, aReferrerPolicy,
                              aTriggeringPrincipal, principalToInherit,
                              aFlags, aTypeHint, aPostData, aPostDataLength,
                              aHeadersData, aLoadType, aSHEntry, aFirstParty,
                              aSrcdoc, aSourceDocShell, aBaseURI, false);
      return DispatchToTabGroup(TaskCategory::Other, ev.forget());
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
    nsCOMPtr<nsIDocument> doc = parent->GetDocument();
    if (doc) {
      doc->TryCancelFrameLoaderInitialization(this);
    }
  }

  bool loadFromExternal = false;

  // Before going any further vet loads initiated by external programs.
  if (aLoadType == LOAD_NORMAL_EXTERNAL) {
    loadFromExternal = true;
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

    nsAutoCString curHash, newHash;
    bool curURIHasRef = false, newURIHasRef = false;

    nsresult rvURINew = aURI->GetRef(newHash);
    if (NS_SUCCEEDED(rvURINew)) {
      rvURINew = aURI->GetHasRef(&newURIHasRef);
    }

    bool sameExceptHashes = false;
    if (currentURI && NS_SUCCEEDED(rvURINew)) {
      nsresult rvURIOld = currentURI->GetRef(curHash);
      if (NS_SUCCEEDED(rvURIOld)) {
        rvURIOld = currentURI->GetHasRef(&curURIHasRef);
      }
      if (NS_SUCCEEDED(rvURIOld)) {
        if (NS_FAILED(currentURI->EqualsExceptRef(aURI, &sameExceptHashes))) {
          sameExceptHashes = false;
        }
      }
    }

    if (!sameExceptHashes && sURIFixup && currentURI &&
        NS_SUCCEEDED(rvURINew)) {
      // Maybe aURI came from the exposable form of currentURI?
      nsCOMPtr<nsIURI> currentExposableURI;
      rv = sURIFixup->CreateExposableURI(currentURI,
                                         getter_AddRefs(currentExposableURI));
      NS_ENSURE_SUCCESS(rv, rv);
      nsresult rvURIOld = currentExposableURI->GetRef(curHash);
      if (NS_SUCCEEDED(rvURIOld)) {
        rvURIOld = currentExposableURI->GetHasRef(&curURIHasRef);
      }
      if (NS_SUCCEEDED(rvURIOld)) {
        if (NS_FAILED(currentExposableURI->EqualsExceptRef(aURI, &sameExceptHashes))) {
          sameExceptHashes = false;
        }
      }
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
       sameExceptHashes && newURIHasRef);

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
      nsCOMPtr<nsIPrincipal> newURITriggeringPrincipal, newURIPrincipalToInherit;
      if (mOSHE) {
        mOSHE->GetTriggeringPrincipal(getter_AddRefs(newURITriggeringPrincipal));
        mOSHE->GetPrincipalToInherit(getter_AddRefs(newURIPrincipalToInherit));
      } else {
        newURITriggeringPrincipal = aTriggeringPrincipal;
        newURIPrincipalToInherit = doc->NodePrincipal();
      }
      // Pass true for aCloneSHChildren, since we're not
      // changing documents here, so all of our subframes are
      // still relevant to the new session history entry.
      //
      // It also makes OnNewURI(...) set LOCATION_CHANGE_SAME_DOCUMENT
      // flag on firing onLocationChange(...).
      // Anyway, aCloneSHChildren param is simply reflecting
      // doShortCircuitedLoad in this scope.
      OnNewURI(aURI, nullptr, newURITriggeringPrincipal, newURIPrincipalToInherit,
               mLoadType, true, true, true);

      nsCOMPtr<nsIInputStream> postData;
      nsCOMPtr<nsISupports> cacheKey;

      bool scrollRestorationIsManual = false;
      if (mOSHE) {
        /* save current position of scroller(s) (bug 59774) */
        mOSHE->SetScrollPosition(cx, cy);
        mOSHE->GetScrollRestorationIsManual(&scrollRestorationIsManual);
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
            if (!aSHEntry) {
              // If we're not doing a history load, scroll restoration
              // should be inherited from the previous session history entry.
              mLSHE->SetScrollRestorationIsManual(scrollRestorationIsManual);
            }
            mLSHE->AdoptBFCacheEntry(mOSHE);
          }
        }
      }

      // If we're doing a history load, use its scroll restoration state.
      if (aSHEntry) {
        aSHEntry->GetScrollRestorationIsManual(&scrollRestorationIsManual);
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
      UpdateGlobalHistoryTitle(aURI);

      SetDocCurrentStateObj(mOSHE);

      // Inform the favicon service that the favicon for oldURI also
      // applies to aURI.
      CopyFavicon(currentURI, aURI, doc->NodePrincipal(), UsePrivateBrowsing());

      RefPtr<nsGlobalWindowOuter> scriptGlobal = mScriptGlobal;
      RefPtr<nsGlobalWindowInner> win = scriptGlobal ?
        scriptGlobal->GetCurrentInnerWindowInternal() : nullptr;

      // ScrollToAnchor doesn't necessarily cause us to scroll the window;
      // the function decides whether a scroll is appropriate based on the
      // arguments it receives.  But even if we don't end up scrolling,
      // ScrollToAnchor performs other important tasks, such as informing
      // the presShell that we have a new hash.  See bug 680257.
      rv = ScrollToAnchor(curURIHasRef, newURIHasRef, newHash, aLoadType);
      NS_ENSURE_SUCCESS(rv, rv);

      /* restore previous position of scroller(s), if we're moving
       * back in history (bug 59774)
       */
      nscoord bx = 0;
      nscoord by = 0;
      bool needsScrollPosUpdate = false;
      if (mOSHE && (aLoadType == LOAD_HISTORY ||
                    aLoadType == LOAD_RELOAD_NORMAL) &&
          !scrollRestorationIsManual) {
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
        bool doHashchange = sameExceptHashes &&
                            (curURIHasRef != newURIHasRef || !curHash.Equals(newHash));

        if (historyNavBetweenSameDoc || doHashchange) {
          win->DispatchSyncPopState();
        }

        if (needsScrollPosUpdate && win->AsInner()->HasActiveDocument()) {
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

  // mContentViewer->PermitUnload can destroy |this| docShell, which
  // causes the next call of CanSavePresentation to crash.
  // Hold onto |this| until we return, to prevent a crash from happening.
  // (bug#331040)
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(this);

  // Don't init timing for javascript:, since it generally doesn't
  // actually start a load or anything.  If it does, we'll init
  // timing then, from OnStateChange.

  // XXXbz mTiming should know what channel it's for, so we don't
  // need this hackery.
  bool toBeReset = false;
  if (!isJavaScript) {
    toBeReset = MaybeInitTiming();
  }
  bool timeBeforeUnload = aFileName.IsVoid();
  if (mTiming && timeBeforeUnload) {
    mTiming->NotifyBeforeUnload();
  }
  // Check if the page doesn't want to be unloaded. The javascript:
  // protocol handler deals with this for javascript: URLs.
  if (!isJavaScript && aFileName.IsVoid() && mContentViewer) {
    bool okToUnload;
    rv = mContentViewer->PermitUnload(&okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the
      // load.
      MaybeResetInitTiming(toBeReset);
      return NS_OK;
    }
  }

  if (mTiming && timeBeforeUnload) {
    mTiming->NotifyUnloadAccepted(mCurrentURI);
  }

  // Check if the webbrowser chrome wants the load to proceed; this can be
  // used to cancel attempts to load URIs in the wrong process.
  nsCOMPtr<nsIWebBrowserChrome3> browserChrome3 = do_GetInterface(mTreeOwner);
  if (browserChrome3) {
    bool shouldLoad;
    rv = browserChrome3->ShouldLoadURI(this, aURI, aReferrer, !!aPostData,
                                       aTriggeringPrincipal, &shouldLoad);
    if (NS_SUCCEEDED(rv) && !shouldLoad) {
      return NS_OK;
    }
  }

  if (browserChrome3 && aCheckForPrerender) {
    nsCOMPtr<nsIRunnable> ev =
      new InternalLoadEvent(this, aURI, aOriginalURI, aResultPrincipalURI,
                            aLoadReplace, aReferrer, aReferrerPolicy,
                            aTriggeringPrincipal, principalToInherit,
                            aFlags, aTypeHint, aPostData, aPostDataLength,
                            aHeadersData, aLoadType, aSHEntry, aFirstParty,
                            aSrcdoc, aSourceDocShell, aBaseURI, false);
    // We don't need any success handler since in that case
    // OnPartialSHistoryDeactive would be called, and it would ensure
    // docshell loads about:blank.
    bool shouldSwitch = false;
    rv = browserChrome3->ShouldSwitchToPrerenderedDocument(
      aURI, mCurrentURI, nullptr, ev, &shouldSwitch);
    if (NS_SUCCEEDED(rv) && shouldSwitch) {
      return NS_OK;
    }
  }

  // Whenever a top-level browsing context is navigated, the user agent MUST
  // lock the orientation of the document to the document's default
  // orientation. We don't explicitly check for a top-level browsing context
  // here because orientation is only set on top-level browsing contexts.
  if (OrientationLock() != eScreenOrientation_None) {
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
    if (aSHEntry) {
      // We're making history navigation or a reload. Make sure our history ID
      // points to the same ID as SHEntry's docshell ID.
      mHistoryID = aSHEntry->DocshellID();
    }
  }

  mSavingOldViewer = savePresentation;

  // If we have a saved content viewer in history, restore and show it now.
  if (aSHEntry && (mLoadType & LOAD_CMD_HISTORY)) {
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
    srcdoc = VoidString();
  }

  bool isTopLevelDoc = mItemType == typeContent &&
                       (isTargetTopLevelDocShell ||
                        GetIsMozBrowser());

  OriginAttributes attrs = GetOriginAttributes();
  attrs.SetFirstPartyDomain(isTopLevelDoc, aURI);

  net::PredictorLearn(aURI, nullptr,
                      nsINetworkPredictor::LEARN_LOAD_TOPLEVEL, attrs);
  net::PredictorPredict(aURI, nullptr,
                        nsINetworkPredictor::PREDICT_LOAD, attrs, nullptr);

  nsCOMPtr<nsIRequest> req;
  rv = DoURILoad(aURI, aOriginalURI, aResultPrincipalURI, aLoadReplace,
                 loadFromExternal,
                 (aFlags & INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI),
                 aReferrer,
                 !(aFlags & INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER),
                 aReferrerPolicy,
                 aTriggeringPrincipal, principalToInherit, aTypeHint,
                 aFileName, aPostData, aPostDataLength, aHeadersData,
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
    if (DisplayLoadError(rv, aURI, nullptr, chan) &&
        (aFlags & LOAD_FLAGS_ERROR_LOAD_CHANGES_RV) != 0) {
      return NS_ERROR_LOAD_SHOWED_ERRORPAGE;
    }
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

// CSPs upgrade-insecure-requests directive applies to same origin top level
// navigations. Using the SOP would return false for the case when an https
// page triggers and http page to load, even though that http page would be
// upgraded to https later. Hence we have to use that custom function instead
// of simply calling aTriggeringPrincipal->Equals(aResultPrincipal).
static bool
IsConsideredSameOriginForUIR(nsIPrincipal* aTriggeringPrincipal,
                             nsIPrincipal* aResultPrincipal)
{
  MOZ_ASSERT(aTriggeringPrincipal);
  MOZ_ASSERT(aResultPrincipal);

  // we only have to make sure that the following truth table holds:
  // aTriggeringPrincipal         | aResultPrincipal             | Result
  // ----------------------------------------------------------------
  // http://example.com/foo.html  | http://example.com/bar.html  | true
  // https://example.com/foo.html | https://example.com/bar.html | true
  // https://example.com/foo.html | http://example.com/bar.html  | true
  if (aTriggeringPrincipal->Equals(aResultPrincipal)) {
    return true;
  }

  if (!aResultPrincipal->GetIsCodebasePrincipal()) {
    return false;
  }

  nsCOMPtr<nsIURI> resultURI;
  nsresult rv = aResultPrincipal->GetURI(getter_AddRefs(resultURI));
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoCString resultScheme;
  rv = resultURI->GetScheme(resultScheme);
  NS_ENSURE_SUCCESS(rv, false);
  if (!resultScheme.EqualsLiteral("http")) {
    return false;
  }

  nsAutoCString tmpResultSpec;
  rv = resultURI->GetSpec(tmpResultSpec);
  NS_ENSURE_SUCCESS(rv, false);
  // replace http with https
  tmpResultSpec.ReplaceLiteral(0, 4, "https");

  nsCOMPtr<nsIURI> tmpResultURI;
  rv = NS_NewURI(getter_AddRefs(tmpResultURI), tmpResultSpec);
  NS_ENSURE_SUCCESS(rv, false);

  mozilla::OriginAttributes tmpOA =
    BasePrincipal::Cast(aResultPrincipal)->OriginAttributesRef();

  nsCOMPtr<nsIPrincipal> tmpResultPrincipal =
    BasePrincipal::CreateCodebasePrincipal(tmpResultURI, tmpOA);

  return aTriggeringPrincipal->Equals(tmpResultPrincipal);
}

nsresult
nsDocShell::DoURILoad(nsIURI* aURI,
                      nsIURI* aOriginalURI,
                      Maybe<nsCOMPtr<nsIURI>> const& aResultPrincipalURI,
                      bool aLoadReplace,
                      bool aLoadFromExternal,
                      bool aForceAllowDataURI,
                      nsIURI* aReferrerURI,
                      bool aSendReferrer,
                      uint32_t aReferrerPolicy,
                      nsIPrincipal* aTriggeringPrincipal,
                      nsIPrincipal* aPrincipalToInherit,
                      const char* aTypeHint,
                      const nsAString& aFileName,
                      nsIInputStream* aPostData,
                      int64_t aPostDataLength,
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
  // Double-check that we're still around to load this URI.
  if (mIsBeingDestroyed) {
    // Return NS_OK despite not doing anything to avoid throwing exceptions from
    // nsLocation::SetHref if the unload handler of the existing page tears us
    // down.
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (IsFrame()) {

    MOZ_ASSERT(aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_IFRAME ||
               aContentPolicyType == nsIContentPolicy::TYPE_INTERNAL_FRAME,
               "DoURILoad thinks this is a frame and InternalLoad does not");

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
  } else {
    MOZ_ASSERT(aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT,
    "DoURILoad thinks this is a document and InternalLoad does not");
  }

  // open a channel for the url
  nsCOMPtr<nsIChannel> channel;

  bool isSrcdoc = !aSrcdoc.IsVoid();

  // There are two cases we care about:
  // * Top-level load: In this case, loadingNode is null, but loadingWindow
  //   is our mScriptGlobal. We pass null for loadingPrincipal in this case.
  // * Subframe load: loadingWindow is null, but loadingNode is the frame
  //   element for the load. loadingPrincipal is the NodePrincipal of the frame
  //   element.
  nsCOMPtr<nsINode> loadingNode;
  nsCOMPtr<nsPIDOMWindowOuter> loadingWindow;
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  nsCOMPtr<nsISupports> topLevelLoadingContext;

  if (aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT) {
    loadingNode = nullptr;
    loadingPrincipal = nullptr;
    loadingWindow = mScriptGlobal->AsOuter();
    if (XRE_IsContentProcess()) {
      // In e10s the child process doesn't have access to the element that
      // contains the browsing context (because that element is in the chrome
      // process).
      nsCOMPtr<nsITabChild> tabChild = GetTabChild();
      topLevelLoadingContext = ToSupports(tabChild);
    } else {
      // This is for loading non-e10s tabs and toplevel windows of various
      // sorts.
      // For the toplevel window cases, requestingElement will be null.
      nsCOMPtr<Element> requestingElement =
        loadingWindow->GetFrameElementInternal();
      topLevelLoadingContext = requestingElement;
    }
  } else {
    loadingWindow = nullptr;
    loadingNode = mScriptGlobal->AsOuter()->GetFrameElementInternal();
    if (loadingNode) {
      // If we have a loading node, then use that as our loadingPrincipal.
      loadingPrincipal = loadingNode->NodePrincipal();
#ifdef DEBUG
      // Get the docshell type for requestingElement.
      nsCOMPtr<nsIDocument> requestingDoc = loadingNode->OwnerDoc();
      nsCOMPtr<nsIDocShell> elementDocShell = requestingDoc->GetDocShell();
      // requestingElement docshell type = current docshell type.
      MOZ_ASSERT(mItemType == elementDocShell->ItemType(),
                "subframes should have the same docshell type as their parent");
#endif
    } else {
      // If this isn't a top-level load and mScriptGlobal's frame element is
      // null, then the element got removed from the DOM while we were trying
      // to load this resource. This docshell is scheduled for destruction
      // already, so bail out here.
      return NS_OK;
    }
  }

  // Getting the right triggeringPrincipal needs to be updated and is only
  // ready for use once bug 1182569 landed. Until then, we cannot rely on
  // the triggeringPrincipal for TYPE_DOCUMENT loads.
  MOZ_ASSERT(aTriggeringPrincipal, "Need a valid triggeringPrincipal");

  bool isSandBoxed = mSandboxFlags & SANDBOXED_ORIGIN;

  // We want to inherit aPrincipalToInherit when:
  // 1. ChannelShouldInheritPrincipal returns true.
  // 2. aURI is not data: URI, or data: URI is not configured as unique opaque
  //    origin.
  bool inheritAttrs = false, inheritPrincipal = false;

  if (aPrincipalToInherit) {
    inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
      aPrincipalToInherit,
      aURI,
      true, // aInheritForAboutBlank
      isSrcdoc);

    bool isData;
    bool isURIUniqueOrigin = nsIOService::IsDataURIUniqueOpaqueOrigin() &&
                             NS_SUCCEEDED(aURI->SchemeIs("data", &isData)) &&
                             isData;
    inheritPrincipal = inheritAttrs && !isURIUniqueOrigin;
  }

  nsLoadFlags loadFlags = mDefaultLoadFlags;
  nsSecurityFlags securityFlags =
    nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;

  if (aFirstParty) {
    // tag first party URL loads
    loadFlags |= nsIChannel::LOAD_INITIAL_DOCUMENT_URI;
  }

  if (mLoadType == LOAD_ERROR_PAGE) {
    // Error pages are LOAD_BACKGROUND
    loadFlags |= nsIChannel::LOAD_BACKGROUND;
    securityFlags |= nsILoadInfo::SEC_LOAD_ERROR_PAGE;
  }

  if (inheritPrincipal) {
    securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }
  if (isSandBoxed) {
    securityFlags |= nsILoadInfo::SEC_SANDBOXED;
  }

  nsCOMPtr<nsILoadInfo> loadInfo =
    (aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT) ?
      new LoadInfo(loadingWindow, aTriggeringPrincipal, topLevelLoadingContext,
                   securityFlags) :
      new LoadInfo(loadingPrincipal, aTriggeringPrincipal, loadingNode,
                   securityFlags, aContentPolicyType);

  if (aPrincipalToInherit) {
    loadInfo->SetPrincipalToInherit(aPrincipalToInherit);
  }
  loadInfo->SetLoadTriggeredFromExternal(aLoadFromExternal);
  loadInfo->SetForceAllowDataURI(aForceAllowDataURI);

  // We have to do this in case our OriginAttributes are different from the
  // OriginAttributes of the parent document. Or in case there isn't a
  // parent document.
  bool isTopLevelDoc = mItemType == typeContent &&
                       (aContentPolicyType == nsIContentPolicy::TYPE_DOCUMENT ||
                        GetIsMozBrowser());

  OriginAttributes attrs;

  // Inherit origin attributes from aPrincipalToInherit if inheritAttrs is true.
  // Otherwise we just use the origin attributes from docshell.
  if (inheritAttrs) {
    MOZ_ASSERT(aPrincipalToInherit, "We should have aPrincipalToInherit here.");
    attrs = aPrincipalToInherit->OriginAttributesRef();
    // If firstPartyIsolation is not enabled, then PrincipalToInherit should
    // have the same origin attributes with docshell.
    MOZ_ASSERT_IF(!OriginAttributes::IsFirstPartyEnabled(), attrs == GetOriginAttributes());
  } else {
    attrs = GetOriginAttributes();
    attrs.SetFirstPartyDomain(isTopLevelDoc, aURI);
  }

  rv = loadInfo->SetOriginAttributes(attrs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isSrcdoc) {
    rv = NS_NewChannelInternal(getter_AddRefs(channel),
                               aURI,
                               loadInfo,
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
            rv = vsc->SetBaseURI(aBaseURI);
            MOZ_ASSERT(NS_SUCCEEDED(rv));
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
                                 loadInfo, getter_AddRefs(channel));
    } else {
      rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel),
                                            aURI,
                                            aSrcdoc,
                                            NS_LITERAL_CSTRING("text/html"),
                                            loadInfo,
                                            true);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIInputStreamChannel> isc = do_QueryInterface(channel);
      MOZ_ASSERT(isc);
      isc->SetBaseURI(aBaseURI);
    }
  }

  // Navigational requests that are same origin need to be upgraded in case
  // upgrade-insecure-requests is present. Please note that in that case
  // the triggeringPrincipal is holding the CSP that potentially
  // holds upgrade-insecure-requests.
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  aTriggeringPrincipal->GetCsp(getter_AddRefs(csp));
  if (csp) {
    bool upgradeInsecureRequests = false;
    csp->GetUpgradeInsecureRequests(&upgradeInsecureRequests);
    if (upgradeInsecureRequests) {
      // only upgrade if the navigation is same origin
      nsCOMPtr<nsIPrincipal> resultPrincipal;
      rv = nsContentUtils::GetSecurityManager()->
             GetChannelResultPrincipal(channel,
                                       getter_AddRefs(resultPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);
      if (IsConsideredSameOriginForUIR(aTriggeringPrincipal, resultPrincipal)) {
        static_cast<mozilla::LoadInfo*>(loadInfo.get())->SetUpgradeInsecureRequests();
      }
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
          NS_ShouldCheckAppCache(principal));
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
    // The LOAD_REPLACE flag and its handling here will be removed as part
    // of bug 1319110.  For now preserve its restoration here to not break
    // any code expecting it being set specially on redirected channels.
    // If the flag has originally been set to change result of
    // NS_GetFinalChannelURI it won't have any effect and also won't cause
    // any harm.
    if (aLoadReplace) {
      uint32_t loadFlags;
      channel->GetLoadFlags(&loadFlags);
      NS_ENSURE_SUCCESS(rv, rv);
      channel->SetLoadFlags(loadFlags | nsIChannel::LOAD_REPLACE);
    }
  } else {
    channel->SetOriginalURI(aURI);
  }

  if (aResultPrincipalURI) {
    // Unconditionally override, we want the replay to be equal to what has
    // been captured.
    loadInfo->SetResultPrincipalURI(aResultPrincipalURI.ref());
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
      rv = httpChannelInternal->SetThirdPartyFlags(
        nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    if (aFirstParty) {
      rv = httpChannelInternal->SetDocumentURI(aURI);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      rv = httpChannelInternal->SetDocumentURI(aReferrerURI);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    rv = httpChannelInternal->SetRedirectMode(
      nsIHttpChannelInternal::REDIRECT_MODE_MANUAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(channel));
  if (props) {
    // save true referrer for those who need it (e.g. xpinstall whitelisting)
    // Currently only http and ftp channels support this.
    props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                  aReferrerURI);
  }

  nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(channel));
  /* Get the cache Key from SH */
  nsCOMPtr<nsISupports> cacheKey;
  if (cacheChannel) {
    if (mLSHE) {
      mLSHE->GetCacheKey(getter_AddRefs(cacheKey));
    } else if (mOSHE) {  // for reload cases
      mOSHE->GetCacheKey(getter_AddRefs(cacheKey));
    }
  }

  // figure out if we need to set the post data stream on the channel...
  if (aPostData) {
    nsCOMPtr<nsIFormPOSTActionChannel> postChannel(do_QueryInterface(channel));
    if (postChannel) {
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
      postChannel->SetUploadStream(aPostData, EmptyCString(), aPostDataLength);
    }

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
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE) {
      if (cacheChannel && cacheKey) {
        cacheChannel->SetCacheKey(cacheKey);
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
      rv = httpChannel->SetReferrerWithPolicy(aReferrerURI, aReferrerPolicy);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
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

    nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
    if (IsFrame() && win) {
      nsCOMPtr<Element> frameElement = win->GetFrameElementInternal();
      if (frameElement) {
        timedChannel->SetInitiatorType(frameElement->LocalName());
      }
    }
  }

  // Mark the http channel as UrgentStart for top level document loading
  // in active tab.
  if (mIsActive || (mLoadType & (LOAD_CMD_NORMAL | LOAD_CMD_HISTORY))) {
    if (httpChannel && isTopLevelDoc) {
      nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));
      if (cos) {
        cos->AddClassFlags(nsIClassOfService::UrgentStart);
      }
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

static nsresult
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

    const nsACString& oneHeader = StringHead(headersString, crlf);

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

  if (SandboxFlagsImplyCookies(mSandboxFlags)) {
    loadFlags |= nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE;
  }
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

    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
      loadFlags |= nsIRequest::LOAD_BYPASS_CACHE |
        nsIRequest::LOAD_FRESH_CONNECTION;
      MOZ_FALLTHROUGH;

    case LOAD_RELOAD_CHARSET_CHANGE: {
      // Use SetAllowStaleCacheContent (not LOAD_FROM_CACHE flag) since we only want
      // to force cache load for this channel, not the whole loadGroup.
      nsCOMPtr<nsICacheInfoChannel> cachingChannel = do_QueryInterface(aChannel);
      if (cachingChannel) {
        cachingChannel->SetAllowStaleCacheContent(true);
      }
      break;
    }

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

  // If anything fails here, make sure to clear our initial ClientSource.
  auto cleanupInitialClient = MakeScopeExit([&] {
    mInitialClientSource.reset();
  });

  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

  MaybeCreateInitialClientSource();

  // Since we are loading a document we need to make sure the proper reserved
  // and initial client data is stored on the nsILoadInfo.  The
  // ClientChannelHelper does this and ensures that it is propagated properly
  // on redirects.  We pass no reserved client here so that the helper will
  // create the reserved ClientSource if necessary.
  Maybe<ClientInfo> noReservedClient;
  rv = AddClientChannelHelper(aChannel,
                              Move(noReservedClient),
                              GetInitialClientInfo(),
                              win->EventTargetFor(TaskCategory::Other));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aURILoader->OpenURI(aChannel, openFlags, this);
  NS_ENSURE_SUCCESS(rv, rv);

  // We're about to load a new page and it may take time before necko
  // gives back any data, so main thread might have a chance to process a
  // collector slice
  nsJSContext::MaybeRunNextCollectorSlice(this, JS::gcreason::DOCSHELL);

  // Success.  Keep the initial ClientSource if it exists.
  cleanupInitialClient.release();

  return NS_OK;
}

nsresult
nsDocShell::ScrollToAnchor(bool aCurHasRef, bool aNewHasRef,
                           nsACString& aNewHash, uint32_t aLoadType)
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
  if ((!aCurHasRef || aLoadType != LOAD_HISTORY) && !aNewHasRef) {
    return NS_OK;
  }

  // Both the new and current URIs refer to the same page. We can now
  // browse to the hash stored in the new URI.

  if (!aNewHash.IsEmpty()) {
    // anchor is there, but if it's a load from history,
    // we don't have any anchor jumping to do
    bool scroll = aLoadType != LOAD_HISTORY &&
                  aLoadType != LOAD_RELOAD_NORMAL;

    // We assume that the bytes are in UTF-8, as it says in the
    // spec:
    // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1

    // We try the UTF-8 string first, and then try the document's
    // charset (see below).  If the string is not UTF-8,
    // conversion will fail and give us an empty Unicode string.
    // In that case, we should just fall through to using the
    // page's charset.
    nsresult rv = NS_ERROR_FAILURE;
    NS_ConvertUTF8toUTF16 uStr(aNewHash);
    if (!uStr.IsEmpty()) {
      rv = shell->GoToAnchor(uStr, scroll,
                             nsIPresShell::SCROLL_SMOOTH_AUTO);
    }

    // Above will fail if the anchor name is not UTF-8.  Need to
    // convert from document charset to unicode.
    if (NS_FAILED(rv)) {
      // Get a document charset
      NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
      nsIDocument* doc = mContentViewer->GetDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
      nsAutoCString charset;
      doc->GetDocumentCharacterSet()->Name(charset);

      nsCOMPtr<nsITextToSubURI> textToSubURI =
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      // Unescape and convert to unicode
      nsAutoString uStr;

      rv = textToSubURI->UnEscapeAndConvert(charset, aNewHash, uStr);
      NS_ENSURE_SUCCESS(rv, rv);

      // Ignore return value of GoToAnchor, since it will return an error
      // if there is no such anchor in the document, which is actually a
      // success condition for us (we want to update the session history
      // with the new URI no matter whether we actually scrolled
      // somewhere).
      //
      // When aNewHash contains "%00", unescaped string may be empty.
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
nsDocShell::OnNewURI(nsIURI* aURI, nsIChannel* aChannel,
                     nsIPrincipal* aTriggeringPrincipal,
                     nsIPrincipal* aPrincipalToInherit,
                     uint32_t aLoadType, bool aFireOnLocationChange,
                     bool aAddToGlobalHistory, bool aCloneSHChildren)
{
  NS_PRECONDITION(aURI, "uri is null");
  NS_PRECONDITION(!aChannel || !aTriggeringPrincipal, "Shouldn't have both set");

  MOZ_ASSERT(!aPrincipalToInherit || (aPrincipalToInherit && aTriggeringPrincipal));

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]::OnNewURI(\"%s\", [%s], 0x%x)\n",
             this, aURI->GetSpecOrDefault().get(), chanName.get(), aLoadType));
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
    // XXX mCurrentURI can be changed from any caller regardless what actual
    // loaded document is, so testing mCurrentURI isn't really a reliable way.
    // Session restore is one example which changes current URI in order to
    // show address before loading. See bug 1301399.
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

  // Clear subframe history on refresh.
  // XXX: history.go(0) won't go this path as aLoadType is LOAD_HISTORY in this
  // case. One should re-validate after bug 1331865 fixed.
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
      (void)AddToSessionHistory(aURI, aChannel, aTriggeringPrincipal,
                                aPrincipalToInherit, aCloneSHChildren,
                                getter_AddRefs(mLSHE));
    }
  } else if (mSessionHistory && mLSHE && mURIResultedInDocument) {
    // Even if we don't add anything to SHistory, ensure the current index
    // points to the same SHEntry as our mLSHE.
    int32_t index = 0;
    mSessionHistory->GetRequestedIndex(&index);
    if (index == -1) {
      mSessionHistory->GetIndex(&index);
    }
    nsCOMPtr<nsISHEntry> currentSH;
    mSessionHistory->GetEntryAtIndex(index, false, getter_AddRefs(currentSH));
    if (currentSH != mLSHE) {
      nsCOMPtr<nsISHistoryInternal> shPrivate =
        do_QueryInterface(mSessionHistory);
      shPrivate->ReplaceEntry(index, mLSHE);
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

  // If this was a history load or a refresh, or it was a history load but
  // later changed to LOAD_NORMAL_REPLACE due to redirection, update the index
  // in session history.
  if (rootSH &&
       ((mLoadType & (LOAD_CMD_HISTORY | LOAD_CMD_RELOAD)) ||
         mLoadType == LOAD_NORMAL_REPLACE)) {
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
  return OnNewURI(uri, aChannel, nullptr, nullptr, mLoadType, aFireOnLocationChange,
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

    rv = NS_NewURI(getter_AddRefs(newURI), aURL,
                   document->GetDocumentCharacterSet(), docBaseURI);

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

    bool scrollRestorationIsManual = false;
    mOSHE->GetScrollRestorationIsManual(&scrollRestorationIsManual);

    // Since we're not changing which page we have loaded, pass
    // true for aCloneChildren.
    rv = AddToSessionHistory(newURI, nullptr,
                             document->NodePrincipal(), // triggeringPrincipal
                             nullptr, true,
                             getter_AddRefs(newSHEntry));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(newSHEntry, NS_ERROR_FAILURE);

    // Session history entries created by pushState inherit scroll restoration
    // mode from the current entry.
    newSHEntry->SetScrollRestorationIsManual(scrollRestorationIsManual);

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
    nsCOMPtr<nsISHEntry> rootSHEntry = nsSHistory::GetRootSHEntry(newSHEntry);

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
    UpdateGlobalHistoryTitle(newURI);

    // Inform the favicon service that our old favicon applies to this new
    // URI.
    CopyFavicon(oldURI, newURI, document->NodePrincipal(), UsePrivateBrowsing());
  } else {
    FireDummyOnLocationChange();
  }
  document->SetStateObject(scContainer);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentScrollRestorationIsManual(bool* aIsManual)
{
  *aIsManual = false;
  if (mOSHE) {
    mOSHE->GetScrollRestorationIsManual(aIsManual);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurrentScrollRestorationIsManual(bool aIsManual)
{
  if (mOSHE) {
    mOSHE->SetScrollRestorationIsManual(aIsManual);
  }

  return NS_OK;
}

bool
nsDocShell::ShouldAddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel)
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
    rv = aURI->GetPathQueryRef(buf);
    if (NS_FAILED(rv)) {
      return false;
    }

    if (buf.EqualsLiteral("blank")) {
      return false;
    }
    // We only want to add about:newtab if it's not privileged:
    if (buf.EqualsLiteral("newtab")) {
      NS_ENSURE_TRUE(aChannel, false);
      nsCOMPtr<nsIPrincipal> resultPrincipal;
      rv = nsContentUtils::GetSecurityManager()->
             GetChannelResultPrincipal(aChannel,
                                       getter_AddRefs(resultPrincipal));
      NS_ENSURE_SUCCESS(rv, false);
      return !nsContentUtils::IsSystemPrincipal(resultPrincipal);
    }
  }

  return true;
}

nsresult
nsDocShell::AddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel,
                                nsIPrincipal* aTriggeringPrincipal,
                                nsIPrincipal* aPrincipalToInherit,
                                bool aCloneChildren,
                                nsISHEntry** aNewEntry)
{
  NS_PRECONDITION(aURI, "uri is null");
  NS_PRECONDITION(!aChannel || !aTriggeringPrincipal, "Shouldn't have both set");

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]::AddToSessionHistory(\"%s\", [%s])\n",
             this, aURI->GetSpecOrDefault().get(), chanName.get()));
  }
#endif

  nsresult rv = NS_OK;
  nsCOMPtr<nsISHEntry> entry;

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
  nsCOMPtr<nsIURI> resultPrincipalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIURI> referrerURI;
  uint32_t referrerPolicy = mozilla::net::RP_Unset;
  nsCOMPtr<nsISupports> cacheKey;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> principalToInherit = aPrincipalToInherit;
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
      rv = httpChannel->GetReferrer(getter_AddRefs(referrerURI));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      rv = httpChannel->GetReferrerPolicy(&referrerPolicy);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      discardLayoutState = ShouldDiscardLayoutState(httpChannel);
    }

    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo) {
      if (!triggeringPrincipal) {
        triggeringPrincipal = loadInfo->TriggeringPrincipal();
      }

      loadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));

      // For now keep storing just the principal in the SHEntry.
      if (!principalToInherit) {
        if (loadInfo->GetLoadingSandboxed()) {
          if (loadInfo->LoadingPrincipal()) {
            principalToInherit = NullPrincipal::CreateWithInheritedAttributes(
            loadInfo->LoadingPrincipal());
          } else {
            // get the OriginAttributes
            OriginAttributes attrs;
            loadInfo->GetOriginAttributes(&attrs);
            principalToInherit = NullPrincipal::Create(attrs);
          }
        } else {
          principalToInherit = loadInfo->PrincipalToInherit();
        }
      }
    }
  }

  // Title is set in nsDocShell::SetTitle()
  entry->Create(aURI,                // uri
                EmptyString(),       // Title
                inputStream,         // Post data stream
                nullptr,             // LayoutHistory state
                cacheKey,            // CacheKey
                mContentTypeHint,    // Content-type
                triggeringPrincipal, // Channel or provided principal
                principalToInherit,
                mHistoryID,
                mDynamicallyCreated);

  entry->SetOriginalURI(originalURI);
  entry->SetResultPrincipalURI(resultPrincipalURI);
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
      nsSHistory::CloneAndReplace(mOSHE, this, cloneID, entry, true,
                                  getter_AddRefs(newEntry));
      NS_ASSERTION(entry == newEntry,
                   "The new session history should be in the new entry");
    }

    // This is the root docshell
    bool addToSHistory = !LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY);
    if (!addToSHistory) {
      // Replace current entry in session history; If the requested index is
      // valid, it indicates the loading was triggered by a history load, and
      // we should replace the entry at requested index instead.
      int32_t index = 0;
      mSessionHistory->GetRequestedIndex(&index);
      if (index == -1) {
        mSessionHistory->GetIndex(&index);
      }
      nsCOMPtr<nsISHistoryInternal> shPrivate =
        do_QueryInterface(mSessionHistory);
      // Replace the current entry with the new entry
      if (index >= 0) {
        if (shPrivate) {
          rv = shPrivate->ReplaceEntry(index, entry);
        }
      } else {
        // If we're trying to replace an inexistant shistory entry, append.
        addToSHistory = true;
      }
    }

    if (addToSHistory) {
      // Add to session history
      nsCOMPtr<nsISHistoryInternal> shPrivate =
        do_QueryInterface(mSessionHistory);
      NS_ENSURE_TRUE(shPrivate, NS_ERROR_FAILURE);
      mSessionHistory->GetIndex(&mPreviousTransIndex);

      bool shouldPersist = ShouldAddToSessionHistory(aURI, aChannel);
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
  nsCOMPtr<nsIURI> resultPrincipalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIInputStream> postData;
  nsCOMPtr<nsIURI> referrerURI;
  uint32_t referrerPolicy;
  nsAutoCString contentType;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  nsCOMPtr<nsIPrincipal> principalToInherit;

  NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(aEntry->GetURI(getter_AddRefs(uri)), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetOriginalURI(getter_AddRefs(originalURI)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI)),
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
  NS_ENSURE_SUCCESS(aEntry->GetTriggeringPrincipal(getter_AddRefs(triggeringPrincipal)),
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(aEntry->GetPrincipalToInherit(getter_AddRefs(principalToInherit)),
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
    // Don't cache the presentation if we're going to just reload the
    // current entry. Caching would lead to trying to save the different
    // content viewers in the same nsISHEntry object.
    rv = CreateAboutBlankContentViewer(principalToInherit, nullptr,
                                       aEntry != mOSHE);

    if (NS_FAILED(rv)) {
      // The creation of the intermittent about:blank content
      // viewer failed for some reason (potentially because the
      // user prevented it). Interrupt the history load.
      return NS_OK;
    }

    if (!triggeringPrincipal) {
      // Ensure that we have a triggeringPrincipal.  Otherwise javascript:
      // URIs will pick it up from the about:blank page we just loaded,
      // and we don't really want even that in this case.
      triggeringPrincipal = NullPrincipal::CreateWithInheritedAttributes(this);
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

  // Do not inherit principal from document (security-critical!);
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
    srcdoc = VoidString();
  }

  // If there is no valid triggeringPrincipal, we deny the load
  MOZ_ASSERT(triggeringPrincipal, "need a valid triggeringPrincipal to load from history");
  if (!triggeringPrincipal) {
    return NS_ERROR_FAILURE;
  }

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  Maybe<nsCOMPtr<nsIURI>> emplacedResultPrincipalURI;
  emplacedResultPrincipalURI.emplace(Move(resultPrincipalURI));
  rv = InternalLoad(uri,
                    originalURI,
                    emplacedResultPrincipalURI,
                    loadReplace,
                    referrerURI,
                    referrerPolicy,
                    triggeringPrincipal,
                    principalToInherit,
                    flags,
                    EmptyString(),      // No window target
                    contentType.get(),  // Type hint
                    VoidString(),       // No forced file download
                    postData,           // Post data stream
                    -1,                 // Post data stream length
                    nullptr,            // No headers stream
                    aLoadType,          // Load type
                    aEntry,             // SHEntry
                    true,
                    srcdoc,
                    nullptr,            // Source docshell, see comment above
                    baseURI,
                    false,
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
    bool scrollRestorationIsManual = false;
    mOSHE->GetScrollRestorationIsManual(&scrollRestorationIsManual);

    nsCOMPtr<nsIPresShell> shell = GetPresShell();
    nsCOMPtr<nsILayoutHistoryState> layoutState;
    if (shell) {
      rv = shell->CaptureHistoryState(getter_AddRefs(layoutState));
    } else if (scrollRestorationIsManual) {
      // Even if we don't have layout anymore, we may want to reset the current
      // scroll state in layout history.
      GetLayoutHistoryState(getter_AddRefs(layoutState));
    }

    if (scrollRestorationIsManual && layoutState) {
      layoutState->ResetScrollState();
    }
  }

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

void
nsDocShell::SetHistoryEntry(nsCOMPtr<nsISHEntry>* aPtr, nsISHEntry* aEntry)
{
  // We need to sync up the docshell and session history trees for
  // subframe navigation.  If the load was in a subframe, we forward up to
  // the root docshell, which will then recursively sync up all docshells
  // to their corresponding entries in the new session history tree.
  // If we don't do this, then we can cache a content viewer on the wrong
  // cloned entry, and subsequently restore it at the wrong time.

  nsISHEntry* newRootEntry = nsSHistory::GetRootSHEntry(aEntry);
  if (newRootEntry) {
    // newRootEntry is now the new root entry.
    // Find the old root entry as well.

    // Need a strong ref. on |oldRootEntry| so it isn't destroyed when
    // SetChildHistoryEntry() does SwapHistoryEntries() (bug 304639).
    nsCOMPtr<nsISHEntry> oldRootEntry = nsSHistory::GetRootSHEntry(*aPtr);
    if (oldRootEntry) {
      nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
      GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));
      nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(rootAsItem);
      if (rootShell) { // if we're the root just set it, nothing to swap
        nsSHistory::SwapEntriesData data = { this, newRootEntry };
        nsIDocShell* rootIDocShell = static_cast<nsIDocShell*>(rootShell);
        nsDocShell* rootDocShell = static_cast<nsDocShell*>(rootIDocShell);

#ifdef DEBUG
        nsresult rv =
#endif
        nsSHistory::SetChildHistoryEntry(oldRootEntry, rootDocShell, 0, &data);
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
  bool noStore = false;
  Unused << aChannel->IsNoStoreResponse(&noStore);
  return noStore;
}

NS_IMETHODIMP
nsDocShell::GetEditor(nsIEditor** aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  RefPtr<HTMLEditor> htmlEditor = GetHTMLEditorInternal();
  htmlEditor.forget(aEditor);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetEditor(nsIEditor* aEditor)
{
  HTMLEditor* htmlEditor = aEditor ? aEditor->AsHTMLEditor() : nullptr;
  // If TextEditor comes, throw an error.
  if (aEditor && !htmlEditor) {
    return NS_ERROR_INVALID_ARG;
  }
  return SetHTMLEditorInternal(htmlEditor);
}

HTMLEditor*
nsDocShell::GetHTMLEditorInternal()
{
  return mEditorData ? mEditorData->GetHTMLEditor() : nullptr;
}

nsresult
nsDocShell::SetHTMLEditorInternal(HTMLEditor* aHTMLEditor)
{
  if (!aHTMLEditor && !mEditorData) {
    return NS_OK;
  }

  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return mEditorData->SetHTMLEditor(aHTMLEditor);
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
  Unused << httpChannel->GetRequestMethod(method);
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

    NS_WARNING_ASSERTION(
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
  if (mItemType != typeContent || !mUseGlobalHistory || UsePrivateBrowsing()) {
    return;
  }

  nsCOMPtr<IHistory> history = services::GetHistoryService();

  if (mPrerenderGlobalHistory || history) {
    uint32_t visitURIFlags = 0;

    if (!IsFrame()) {
      visitURIFlags |= IHistory::TOP_LEVEL;
    }

    if (aChannelRedirectFlags & nsIChannelEventSink::REDIRECT_TEMPORARY) {
      visitURIFlags |= IHistory::REDIRECT_TEMPORARY;
    } else if (aChannelRedirectFlags & nsIChannelEventSink::REDIRECT_PERMANENT) {
      visitURIFlags |= IHistory::REDIRECT_PERMANENT;
    } else {
      MOZ_ASSERT(!aChannelRedirectFlags,
                 "One of REDIRECT_TEMPORARY or REDIRECT_PERMANENT must be set "
                 "if any flags in aChannelRedirectFlags is set.");
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

    if (mPrerenderGlobalHistory) {
      mPrerenderGlobalHistory->VisitURI(aURI,
                                        aPreviousURI,
                                        aReferrerURI,
                                        visitURIFlags);
    } else {
      (void)history->VisitURI(aURI, aPreviousURI, visitURIFlags);
    }
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

  nsAutoString brandName;
  rv = brandBundle->GetStringFromName("brandShortName", brandName);

  nsAutoString msgString, button0Title;
  if (NS_FAILED(rv)) { // No brand, use the generic version.
    rv = appBundle->GetStringFromName("confirmRepostPrompt", msgString);
  } else {
    // Brand available - if the app has an override file with formatting, the
    // app name will be included. Without an override, the prompt will look
    // like the generic version.
    const char16_t* formatStrings[] = { brandName.get() };
    rv = appBundle->FormatStringFromName("confirmRepostPrompt",
                                         formatStrings,
                                         ArrayLength(formatStrings),
                                         msgString);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = appBundle->GetStringFromName("resendButton.label", button0Title);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Make the repost prompt tab modal to prevent malicious pages from locking
  // up the browser, see bug 1412559 for an example.
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompter)) {
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), true);
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

nsresult
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

nsIScrollableFrame*
nsDocShell::GetRootScrollFrame()
{
  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, nullptr);

  return shell->GetRootScrollFrameAsScrollable();
}

nsresult
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

  // If our window is modal and we're not opened as chrome, make
  // this window a modal content window.
  mScriptGlobal = NS_NewScriptGlobalObject(mItemType == typeChrome);
  MOZ_ASSERT(mScriptGlobal);

  mScriptGlobal->SetDocShell(this);

  // Ensure the script object is set up to run script.
  return mScriptGlobal->EnsureScriptEnvironment();
}

nsresult
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

nsresult
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
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = do_QueryInterface(scriptGO);
  nsCOMPtr<nsPIDOMWindowOuter> windowToSearch;
  nsFocusManager::GetFocusedDescendant(ourWindow,
                                       nsFocusManager::eIncludeAllDescendants,
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
      document->SetBaseURI(baseURI);
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

  return wwatch->GetPrompt(mScriptGlobal->AsOuter(), aIID,
                           reinterpret_cast<void**>(aResult));
}

//*****************************************************************************
// nsDocShell::nsILoadContext
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetAssociatedWindow(mozIDOMWindowProxy** aWindow)
{
  CallGetInterface(this, aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopWindow(mozIDOMWindowProxy** aWindow)
{
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (win) {
    win = win->GetTop();
  }
  win.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopFrameElement(nsIDOMElement** aElement)
{
  *aElement = nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> top = win->GetScriptableTop();
  NS_ENSURE_TRUE(top, NS_ERROR_FAILURE);

  // GetFrameElementInternal, /not/ GetScriptableFrameElement -- if |top| is
  // inside <iframe mozbrowser>, we want to return the iframe, not null.
  // And we want to cross the content/chrome boundary.
  nsCOMPtr<nsIDOMElement> elt =
    do_QueryInterface(top->GetFrameElementInternal());
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
nsDocShell::GetUseTrackingProtection(bool* aUseTrackingProtection)
{
  *aUseTrackingProtection  = false;

  static bool sTPEnabled = false;
  static bool sTPInPBEnabled = false;
  static bool sPrefsInit = false;

  if (!sPrefsInit) {
    sPrefsInit = true;
    Preferences::AddBoolVarCache(&sTPEnabled,
      "privacy.trackingprotection.enabled", false);
    Preferences::AddBoolVarCache(&sTPInPBEnabled,
      "privacy.trackingprotection.pbmode.enabled", false);
  }

  if (mUseTrackingProtection || sTPEnabled ||
      (UsePrivateBrowsing() && sTPInPBEnabled)) {
    *aUseTrackingProtection = true;
    return NS_OK;
  }

  RefPtr<nsDocShell> parent = GetParentDocshell();
  if (parent) {
    return parent->GetUseTrackingProtection(aUseTrackingProtection);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseTrackingProtection(bool aUseTrackingProtection)
{
  mUseTrackingProtection = aUseTrackingProtection;
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

  return root->GetControllerForCommand(aCommand, false /* for any window */,
                                       aResult);
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

NS_IMETHODIMP
nsDocShell::DoCommandWithParams(const char* aCommand, nsICommandParams* aParams)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsICommandController> commandController =
    do_QueryInterface(controller, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return commandController->DoCommandWithParams(aCommand, aParams);
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

    nsCOMPtr<nsPIDOMWindowOuter> domWindow = GetWindow();
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

class OnLinkClickEvent : public Runnable
{
public:
  OnLinkClickEvent(nsDocShell* aHandler, nsIContent* aContent,
                   nsIURI* aURI,
                   const char16_t* aTargetSpec,
                   const nsAString& aFileName,
                   nsIInputStream* aPostDataStream,
                   int64_t aPostDataStreamLength,
                   nsIInputStream* aHeadersDataStream,
                   bool aNoOpenerImplied,
                   bool aIsTrusted,
                   nsIPrincipal* aTriggeringPrincipal);

  NS_IMETHOD Run() override
  {
    nsAutoPopupStatePusher popupStatePusher(mPopupState);

    // We need to set up an AutoJSAPI here for the following reason: When we do
    // OnLinkClickSync we'll eventually end up in nsGlobalWindow::OpenInternal
    // which only does popup blocking if !LegacyIsCallerChromeOrNativeCode().
    // So we need to fake things so that we don't look like native code as far
    // as LegacyIsCallerNativeCode() is concerned.
    AutoJSAPI jsapi;
    if (mIsTrusted || jsapi.Init(mContent->OwnerDoc()->GetScopeObject())) {
      mHandler->OnLinkClickSync(mContent, mURI,
                                mTargetSpec.get(), mFileName,
                                mPostDataStream, mPostDataStreamLength,
                                mHeadersDataStream, mNoOpenerImplied,
                                nullptr, nullptr, mTriggeringPrincipal);
    }
    return NS_OK;
  }

private:
  RefPtr<nsDocShell> mHandler;
  nsCOMPtr<nsIURI> mURI;
  nsString mTargetSpec;
  nsString mFileName;
  nsCOMPtr<nsIInputStream> mPostDataStream;
  int64_t mPostDataStreamLength;
  nsCOMPtr<nsIInputStream> mHeadersDataStream;
  nsCOMPtr<nsIContent> mContent;
  PopupControlState mPopupState;
  bool mNoOpenerImplied;
  bool mIsTrusted;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
};

OnLinkClickEvent::OnLinkClickEvent(nsDocShell* aHandler,
                                   nsIContent* aContent,
                                   nsIURI* aURI,
                                   const char16_t* aTargetSpec,
                                   const nsAString& aFileName,
                                   nsIInputStream* aPostDataStream,
                                   int64_t aPostDataStreamLength,
                                   nsIInputStream* aHeadersDataStream,
                                   bool aNoOpenerImplied,
                                   bool aIsTrusted,
                                   nsIPrincipal* aTriggeringPrincipal)
  : mozilla::Runnable("OnLinkClickEvent")
  , mHandler(aHandler)
  , mURI(aURI)
  , mTargetSpec(aTargetSpec)
  , mFileName(aFileName)
  , mPostDataStream(aPostDataStream)
  , mPostDataStreamLength(aPostDataStreamLength)
  , mHeadersDataStream(aHeadersDataStream)
  , mContent(aContent)
  , mPopupState(mHandler->mScriptGlobal->GetPopupControlState())
  , mNoOpenerImplied(aNoOpenerImplied)
  , mIsTrusted(aIsTrusted)
  , mTriggeringPrincipal(aTriggeringPrincipal)
{
}

NS_IMETHODIMP
nsDocShell::OnLinkClick(nsIContent* aContent,
                        nsIURI* aURI,
                        const char16_t* aTargetSpec,
                        const nsAString& aFileName,
                        nsIInputStream* aPostDataStream,
                        int64_t aPostDataStreamLength,
                        nsIInputStream* aHeadersDataStream,
                        bool aIsTrusted,
                        nsIPrincipal* aTriggeringPrincipal)
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
  bool noOpenerImplied = false;
  if (browserChrome3) {
    nsCOMPtr<nsIDOMNode> linkNode = do_QueryInterface(aContent);
    nsAutoString oldTarget(aTargetSpec);
    rv = browserChrome3->OnBeforeLinkTraversal(oldTarget, aURI,
                                               linkNode, mIsAppTab, target);
    if (!oldTarget.Equals(target)) {
      noOpenerImplied = true;
    }
  }

  if (NS_FAILED(rv)) {
    target = aTargetSpec;
  }

  nsCOMPtr<nsIRunnable> ev =
    new OnLinkClickEvent(this, aContent, aURI, target.get(), aFileName,
                         aPostDataStream, aPostDataStreamLength,
                         aHeadersDataStream, noOpenerImplied,
                         aIsTrusted, aTriggeringPrincipal);
  return DispatchToTabGroup(TaskCategory::UI, ev.forget());
}

static bool
IsElementAnchorOrArea(nsIContent* aContent)
{
  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area);
}

NS_IMETHODIMP
nsDocShell::OnLinkClickSync(nsIContent* aContent,
                            nsIURI* aURI,
                            const char16_t* aTargetSpec,
                            const nsAString& aFileName,
                            nsIInputStream* aPostDataStream,
                            int64_t aPostDataStreamLength,
                            nsIInputStream* aHeadersDataStream,
                            bool aNoOpenerImplied,
                            nsIDocShell** aDocShell,
                            nsIRequest** aRequest,
                            nsIPrincipal* aTriggeringPrincipal)
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
  if (IsElementAnchorOrArea(aContent)) {
    MOZ_ASSERT(aContent->IsHTMLElement());
    nsAutoString referrer;
    aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, referrer);
    nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(referrer);
    while (tok.hasMoreTokens()) {
      const nsAString& token = tok.nextToken();
      if (token.LowerCaseEqualsLiteral("noreferrer")) {
        flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER |
                 INTERNAL_LOAD_FLAGS_NO_OPENER;
        // We now have all the flags we could possibly have, so just stop.
        break;
      }
      if (token.LowerCaseEqualsLiteral("noopener")) {
        flags |= INTERNAL_LOAD_FLAGS_NO_OPENER;
      }
    }
    if (aNoOpenerImplied) {
      flags |= INTERNAL_LOAD_FLAGS_NO_OPENER;
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
  nsPIDOMWindowInner* refererInner = refererDoc->GetInnerWindow();
  NS_ENSURE_TRUE(refererInner, NS_ERROR_UNEXPECTED);
  if (!mScriptGlobal ||
      mScriptGlobal->AsOuter()->GetCurrentInnerWindow() != refererInner) {
    // We're no longer the current inner window
    return NS_OK;
  }

  nsCOMPtr<nsIURI> referer = refererDoc->GetDocumentURI();
  uint32_t refererPolicy = refererDoc->GetReferrerPolicy();

  // get referrer attribute from clicked link and parse it
  // if per element referrer is enabled, the element referrer overrules
  // the document wide referrer
  if (IsElementAnchorOrArea(aContent)) {
    net::ReferrerPolicy refPolEnum = aContent->AsElement()->GetReferrerPolicyAsEnum();
    if (refPolEnum != net::RP_Unset) {
      refererPolicy = refPolEnum;
    }
  }

  // referer could be null here in some odd cases, but that's ok,
  // we'll just load the link w/o sending a referer in those cases.

  nsAutoString target(aTargetSpec);

  // If this is an anchor element, grab its type property to use as a hint
  nsAutoString typeHint;
  RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::FromContent(aContent);
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

  // if the triggeringPrincipal is not passed explicitly, then we
  // fall back to using doc->NodePrincipal() as the triggeringPrincipal.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
    aTriggeringPrincipal ? aTriggeringPrincipal
                         : aContent->NodePrincipal();

  // Link click (or form submission) can be triggered inside an onload handler,
  // and we don't want to add history entry in this case.
  bool inOnLoadHandler = false;
  GetIsExecutingOnLoadHandler(&inOnLoadHandler);
  uint32_t loadType = inOnLoadHandler ? LOAD_NORMAL_REPLACE : LOAD_LINK;

  nsresult rv = InternalLoad(clonedURI,                 // New URI
                             nullptr,                   // Original URI
                             Nothing(),                 // Let the protocol handler assign it
                             false,                     // LoadReplace
                             referer,                   // Referer URI
                             refererPolicy,             // Referer policy
                             triggeringPrincipal,
                             aContent->NodePrincipal(),
                             flags,
                             target,                    // Window target
                             NS_LossyConvertUTF16toASCII(typeHint).get(),
                             aFileName,                 // Download as file
                             aPostDataStream,           // Post data stream
                             aPostDataStreamLength,     // Post data stream length
                             aHeadersDataStream,        // Headers stream
                             loadType,                  // Load type
                             nullptr,                   // No SHEntry
                             true,                      // first party site
                             VoidString(),              // No srcdoc
                             this,                      // We are the source
                             nullptr,                   // baseURI not needed
                             true,                      // Check for prerendered doc
                             aDocShell,                 // DocShell out-param
                             aRequest);                 // Request out-param
  if (NS_SUCCEEDED(rv)) {
    nsPingListener::DispatchPings(this, aContent, aURI, referer, refererPolicy);
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

  nsAutoCString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 uStr(spec);

  mozilla::net::PredictorPredict(aURI, mCurrentURI,
                                 nsINetworkPredictor::PREDICT_LINK,
                                 aContent->NodePrincipal()->OriginAttributesRef(),
                                 nullptr);

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

  if (!mContentViewer) {
    return false;
  }

  nsIDocument* doc = mContentViewer->GetDocument();
  if (!doc) {
    return false;
  }

  return doc->GetAllowPlugins();
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
        switch (mLoadType) {
        case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
          return Reload(LOAD_FLAGS_CHARSET_CHANGE |
                        LOAD_FLAGS_BYPASS_CACHE |
                        LOAD_FLAGS_BYPASS_PROXY);
        case LOAD_RELOAD_BYPASS_CACHE:
          return Reload(LOAD_FLAGS_CHARSET_CHANGE |
                        LOAD_FLAGS_BYPASS_CACHE);
        default:
          return Reload(LOAD_FLAGS_CHARSET_CHANGE);
        }
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
    // XXX: Creating a brand new content viewer to host preview every
    // time we enter here seems overwork. We could skip ahead to where
    // we QI the mContentViewer if the current URI is either about:blank
    // or about:printpreview.
    Stop(nsIWebNavigation::STOP_ALL);
    nsCOMPtr<nsIPrincipal> principal = NullPrincipal::CreateWithInheritedAttributes(this);
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:printpreview"));
    nsresult rv = CreateAboutBlankContentViewer(principal, uri);
    NS_ENSURE_SUCCESS(rv, rv);
    // Here we manually set current URI since we have just created a
    // brand new content viewer (about:blank) to host preview.
    SetCurrentURI(uri, nullptr, true, 0);
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

/* [infallible] */ NS_IMETHODIMP
nsDocShell::SetFrameType(uint32_t aFrameType)
{
  mFrameType = aFrameType;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetFrameType(uint32_t* aFrameType)
{
  *aFrameType = mFrameType;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsMozBrowser(bool* aIsMozBrowser)
{
  *aIsMozBrowser = (mFrameType == FRAME_TYPE_BROWSER);
  return NS_OK;
}

uint32_t
nsDocShell::GetInheritedFrameType()
{
  if (mFrameType != FRAME_TYPE_REGULAR) {
    return mFrameType;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  GetSameTypeParent(getter_AddRefs(parentAsItem));

  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
  if (!parent) {
    return FRAME_TYPE_REGULAR;
  }

  return static_cast<nsDocShell*>(parent.get())->GetInheritedFrameType();
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsIsolatedMozBrowserElement(bool* aIsIsolatedMozBrowserElement)
{
  bool result = mFrameType == FRAME_TYPE_BROWSER &&
                mOriginAttributes.mInIsolatedMozBrowser;
  *aIsIsolatedMozBrowserElement = result;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsInIsolatedMozBrowserElement(bool* aIsInIsolatedMozBrowserElement)
{
  MOZ_ASSERT(!mOriginAttributes.mInIsolatedMozBrowser ||
             (GetInheritedFrameType() == FRAME_TYPE_BROWSER),
             "Isolated mozbrowser should only be true inside browser frames");
  bool result = (GetInheritedFrameType() == FRAME_TYPE_BROWSER) &&
                mOriginAttributes.mInIsolatedMozBrowser;
  *aIsInIsolatedMozBrowserElement = result;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsInMozBrowser(bool* aIsInMozBrowser)
{
  *aIsInMozBrowser = (GetInheritedFrameType() == FRAME_TYPE_BROWSER);
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsDocShell::GetIsTopLevelContentDocShell(bool* aIsTopLevelContentDocShell)
{
  *aIsTopLevelContentDocShell = false;

  if (mItemType == typeContent) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetSameTypeRootTreeItem(getter_AddRefs(root));
    *aIsTopLevelContentDocShell = root.get() == static_cast<nsIDocShellTreeItem*>(this);
  }

  return NS_OK;
}

// Implements nsILoadContext.originAttributes
NS_IMETHODIMP
nsDocShell::GetScriptableOriginAttributes(JS::MutableHandle<JS::Value> aVal)
{
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  MOZ_ASSERT(cx);

  return GetOriginAttributes(cx, aVal);
}

// Implements nsIDocShell.GetOriginAttributes()
NS_IMETHODIMP
nsDocShell::GetOriginAttributes(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aVal)
{
  bool ok = ToJSValue(aCx, mOriginAttributes, aVal);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

bool
nsDocShell::CanSetOriginAttributes()
{
  MOZ_ASSERT(mChildList.IsEmpty());
  if (!mChildList.IsEmpty()) {
    return false;
  }

  // TODO: Bug 1273058 - mContentViewer should be null when setting origin
  // attributes.
  if (mContentViewer) {
    nsIDocument* doc = mContentViewer->GetDocument();
    if (doc) {
      nsIURI* uri = doc->GetDocumentURI();
      if (!uri) {
        return false;
      }
      nsCString uriSpec = uri->GetSpecOrDefault();
      MOZ_ASSERT(uriSpec.EqualsLiteral("about:blank"));
      if (!uriSpec.EqualsLiteral("about:blank")) {
        return false;
      }
    }
  }

  return true;
}

bool
nsDocShell::ServiceWorkerAllowedToControlWindow(nsIPrincipal* aPrincipal,
                                                nsIURI* aURI)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aURI);

  if (UsePrivateBrowsing() || mSandboxFlags) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetSameTypeParent(getter_AddRefs(parent));
  nsPIDOMWindowOuter* parentOuter = parent ? parent->GetWindow() : nullptr;
  nsPIDOMWindowInner* parentInner =
    parentOuter ? parentOuter->GetCurrentInnerWindow() : nullptr;

  nsContentUtils::StorageAccess storage =
    nsContentUtils::StorageAllowedForNewWindow(aPrincipal, aURI, parentInner);

  return storage == nsContentUtils::StorageAccess::eAllow;
}

nsresult
nsDocShell::SetOriginAttributes(const OriginAttributes& aAttrs)
{
  if (!CanSetOriginAttributes()) {
    return NS_ERROR_FAILURE;
  }

  AssertOriginAttributesMatchPrivateBrowsing();
  mOriginAttributes = aAttrs;

  bool isPrivate = mOriginAttributes.mPrivateBrowsingId > 0;
  // Chrome docshell can not contain OriginAttributes.mPrivateBrowsingId
  if (mItemType == typeChrome && isPrivate) {
    mOriginAttributes.mPrivateBrowsingId = 0;
  }

  SetPrivateBrowsing(isPrivate);
  AssertOriginAttributesMatchPrivateBrowsing();

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetOriginAttributesBeforeLoading(JS::Handle<JS::Value> aOriginAttributes)
{
  if (!aOriginAttributes.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(&aOriginAttributes.toObject())) {
    return NS_ERROR_UNEXPECTED;
  }

  JSContext* cx = jsapi.cx();
  if (NS_WARN_IF(!cx)) {
    return NS_ERROR_FAILURE;
  }

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(cx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return SetOriginAttributes(attrs);
}

NS_IMETHODIMP
nsDocShell::SetOriginAttributes(JS::Handle<JS::Value> aOriginAttributes,
                                JSContext* aCx)
{
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return SetOriginAttributes(attrs);
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

void
nsDocShell::UpdateGlobalHistoryTitle(nsIURI* aURI)
{
  if (mUseGlobalHistory && !UsePrivateBrowsing()) {
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (mPrerenderGlobalHistory) {
      mPrerenderGlobalHistory->SetURITitle(aURI, mTitle);
    } else if (history) {
      history->SetURITitle(aURI, mTitle);
    } else if (mGlobalHistory) {
      mGlobalHistory->SetPageTitle(aURI, nsString(mTitle));
    }
  }
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

// The caller owns |aAsyncCause| here.
void
nsDocShell::NotifyJSRunToCompletionStart(const char* aReason,
                                         const char16_t* aFunctionName,
                                         const char16_t* aFilename,
                                         const uint32_t aLineNumber,
                                         JS::Handle<JS::Value> aAsyncStack,
                                         const char* aAsyncCause)
{
  // If first start, mark interval start.
  if (mJSRunToCompletionDepth == 0) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    if (timelines && timelines->HasConsumer(this)) {
      timelines->AddMarkerForDocShell(this, Move(
        mozilla::MakeUnique<JavascriptTimelineMarker>(
          aReason, aFunctionName, aFilename, aLineNumber, MarkerTracingType::START,
          aAsyncStack, aAsyncCause)));
    }
  }

  mJSRunToCompletionDepth++;
}

void
nsDocShell::NotifyJSRunToCompletionStop()
{
  mJSRunToCompletionDepth--;

  // If last stop, mark interval end.
  if (mJSRunToCompletionDepth == 0) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    if (timelines && timelines->HasConsumer(this)) {
      timelines->AddMarkerForDocShell(this, "Javascript", MarkerTracingType::END);
    }
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

  // For subresource requests we base our decision solely on the client's
  // controller value.  Any settings that would have blocked service worker
  // access should have been set before the initial navigation created the
  // window.
  if (!aIsNonSubresourceRequest) {
    nsCOMPtr<nsIDocument> doc = GetDocument();
    if (!doc) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    ErrorResult rv;
    *aShouldIntercept = doc->GetController().isSome();
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(aURI, mOriginAttributes);

  // For navigations, first check to see if we are allowed to control a
  // window with the given URL.
  if (!ServiceWorkerAllowedToControlWindow(principal, aURI)) {
    return NS_OK;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_OK;
  }

  // We're allowed to control a window, so check with the ServiceWorkerManager
  // for a matching service worker.
  *aShouldIntercept = swm->IsAvailable(principal, aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::ChannelIntercepted(nsIInterceptedChannel* aChannel)
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    aChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
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

  ErrorResult error;
  swm->DispatchFetchEvent(mOriginAttributes, doc, aChannel, isReload,
                          isSubresourceLoad, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

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

UniquePtr<ClientSource>
nsDocShell::TakeInitialClientSource()
{
  return Move(mInitialClientSource);
}

NS_IMETHODIMP
nsDocShell::IssueWarning(uint32_t aWarning, bool aAsError)
{
  if (mContentViewer) {
    nsCOMPtr<nsIDocument> doc = mContentViewer->GetDocument();
    if (doc) {
      doc->WarnOnceAbout(nsIDocument::DeprecatedOperations(aWarning), aAsError);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEditingSession(nsIEditingSession** aEditSession)
{
  if (!NS_SUCCEEDED(EnsureEditorData())) {
    return NS_ERROR_FAILURE;
  }

  mEditorData->GetEditingSession(aEditSession);
  return *aEditSession ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetScriptableTabChild(nsITabChild** aTabChild)
{
  *aTabChild = GetTabChild().take();
  return *aTabChild ? NS_OK : NS_ERROR_FAILURE;
}

already_AddRefed<nsITabChild>
nsDocShell::GetTabChild()
{
  nsCOMPtr<nsIDocShellTreeOwner> owner(mTreeOwner);
  nsCOMPtr<nsITabChild> tc = do_GetInterface(owner);
  return tc.forget();
}

nsICommandManager*
nsDocShell::GetCommandManager()
{
  NS_ENSURE_SUCCESS(EnsureCommandHandler(), nullptr);
  return mCommandManager;
}

NS_IMETHODIMP
nsDocShell::GetIsOnlyToplevelInTabGroup(bool* aResult)
{
  MOZ_ASSERT(aResult);

  nsPIDOMWindowOuter* outer = GetWindow();
  MOZ_ASSERT(outer);

  // If we are not toplevel then we are not the only toplevel window in the tab
  // group.
  if (outer->GetScriptableParentOrNull()) {
    *aResult = false;
    return NS_OK;
  }

  // If we have any other toplevel windows in our tab group, then we are not the
  // only toplevel window in the tab group.
  nsTArray<nsPIDOMWindowOuter*> toplevelWindows =
    outer->TabGroup()->GetTopLevelWindows();
  if (toplevelWindows.Length() > 1) {
    *aResult = false;
    return NS_OK;
  }
  MOZ_ASSERT(toplevelWindows.Length() == 1);
  MOZ_ASSERT(toplevelWindows[0] == outer);

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAwaitingLargeAlloc(bool* aResult)
{
  MOZ_ASSERT(aResult);
  nsCOMPtr<nsITabChild> tabChild = GetTabChild();
  if (!tabChild) {
    *aResult = false;
    return NS_OK;
  }
  *aResult = static_cast<TabChild*>(tabChild.get())->IsAwaitingLargeAlloc();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsDocShell::GetOriginAttributes(mozilla::OriginAttributes& aAttrs)
{
  aAttrs = mOriginAttributes;
}

HTMLEditor*
nsIDocShell::GetHTMLEditor()
{
  nsDocShell* docShell = static_cast<nsDocShell*>(this);
  return docShell->GetHTMLEditorInternal();
}

nsresult
nsIDocShell::SetHTMLEditor(HTMLEditor* aHTMLEditor)
{
  nsDocShell* docShell = static_cast<nsDocShell*>(this);
  return docShell->SetHTMLEditorInternal(aHTMLEditor);
}

NS_IMETHODIMP
nsDocShell::GetDisplayMode(uint32_t* aDisplayMode)
{
  NS_ENSURE_ARG_POINTER(aDisplayMode);
  *aDisplayMode = mDisplayMode;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDisplayMode(uint32_t aDisplayMode)
{
  if (!(aDisplayMode == nsIDocShell::DISPLAY_MODE_BROWSER ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_STANDALONE ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_FULLSCREEN ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_MINIMAL_UI)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aDisplayMode != mDisplayMode) {
    mDisplayMode = aDisplayMode;

    RefPtr<nsPresContext> presContext;
    if (NS_SUCCEEDED(GetPresContext(getter_AddRefs(presContext)))) {
      presContext->MediaFeatureValuesChangedAllDocuments(nsRestyleHint(0));
    }
  }

  return NS_OK;
}
