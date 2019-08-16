/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShell.h"

#include <algorithm>

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>  // for getpid()
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Casting.h"
#include "mozilla/Components.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/WidgetUtils.h"

#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ClientHandle.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/PerformanceNavigation.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ServiceWorkerInterceptController.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"

#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "ReferrerInfo.h"

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
#include "nsIConsoleReportCollector.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContentViewer.h"
#include "nsIController.h"
#include "nsICookieService.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDOMWindow.h"
#include "nsIEditingSession.h"
#include "nsIExternalProtocolService.h"
#include "nsIFormPOSTActionChannel.h"
#include "nsIFrame.h"
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
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollObserver.h"
#include "nsISecureBrowserUI.h"
#include "nsISecurityUITelemetry.h"
#include "nsISeekableStream.h"
#include "nsISelectionDisplay.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsISiteSecurityService.h"
#include "nsISocketProvider.h"
#include "nsIStringBundle.h"
#include "nsIStructuredCloneContainer.h"
#include "nsISupportsPrimitives.h"
#include "nsIBrowserChild.h"
#include "nsITextToSubURI.h"
#include "nsITimedChannel.h"
#include "nsITimer.h"
#include "nsITransportSecurityInfo.h"
#include "nsIUploadChannel.h"
#include "nsIURIFixup.h"
#include "nsIURILoader.h"
#include "nsIURIMutator.h"
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

#include "nsCommandManager.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"

#include "IHistory.h"
#include "IUrlClassifierUITelemetry.h"

#include "mozIThirdPartyUtil.h"

#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsCExternalHandlerService.h"
#include "nsContentDLF.h"
#include "nsContentPolicyUtils.h"  // NS_CheckContentLoadPolicy(...)
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsCURILoader.h"
#include "nsDocShellCID.h"
#include "nsDocShellEditorData.h"
#include "nsDocShellEnumerator.h"
#include "nsDocShellLoadState.h"
#include "nsDocShellLoadTypes.h"
#include "nsDOMCID.h"
#include "nsDOMNavigationTiming.h"
#include "nsDSURIContentListener.h"
#include "nsEditingSession.h"
#include "nsError.h"
#include "nsEscape.h"
#include "nsFocusManager.h"
#include "nsGlobalWindow.h"
#include "nsISearchService.h"
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
#include "nsSHistory.h"
#include "nsStructuredCloneContainer.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsViewSourceHandler.h"
#include "nsWebBrowserFind.h"
#include "nsWhitespaceTokenizer.h"
#include "nsWidgetsCID.h"
#include "nsXULAppAPI.h"

#include "GeckoProfiler.h"
#include "mozilla/NullPrincipal.h"
#include "Navigator.h"
#include "prenv.h"
#include "URIUtils.h"

#include "timeline/JavascriptTimelineMarker.h"

#ifdef MOZ_PLACES
#  include "nsIFaviconService.h"
#  include "mozIPlacesPendingOperation.h"
#endif

#if NS_PRINT_PREVIEW
#  include "nsIDocumentViewerPrint.h"
#  include "nsIWebBrowserPrint.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

// Threshold value in ms for META refresh based redirects
#define REFRESH_REDIRECT_TIMER 15000

// Hint for native dispatch of events on how long to delay after
// all documents have loaded in milliseconds before favoring normal
// native event dispatch priorites over performance
// Can be overridden with docshell.event_starvation_delay_hint pref.
#define NS_EVENT_STARVATION_DELAY_HINT 2000

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

// Number of documents currently loading
static int32_t gNumberOfDocumentsLoading = 0;

// Global count of existing docshells.
static int32_t gDocShellCount = 0;

// Global count of docshells with the private attribute set
static uint32_t gNumberOfPrivateDocShells = 0;

#ifdef DEBUG
static mozilla::LazyLogModule gDocShellLog("nsDocShell");
#endif
static mozilla::LazyLogModule gDocShellLeakLog("nsDocShellLeak");
extern mozilla::LazyLogModule gPageCacheLog;

const char kBrandBundleURL[] = "chrome://branding/locale/brand.properties";
const char kAppstringsBundleURL[] =
    "chrome://global/locale/appstrings.properties";

// Global reference to the URI fixup service.
nsIURIFixup* nsDocShell::sURIFixup = nullptr;

static void FavorPerformanceHint(bool aPerfOverStarvation) {
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->FavorPerformanceHint(
        aPerfOverStarvation,
        Preferences::GetUint("docshell.event_starvation_delay_hint",
                             NS_EVENT_STARVATION_DELAY_HINT));
  }
}

static void IncreasePrivateDocShellCount() {
  gNumberOfPrivateDocShells++;
  if (gNumberOfPrivateDocShells > 1 || !XRE_IsContentProcess()) {
    return;
  }

  mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();
  cc->SendPrivateDocShellsExist(true);
}

static void DecreasePrivateDocShellCount() {
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

nsDocShell::nsDocShell(BrowsingContext* aBrowsingContext,
                       uint64_t aContentWindowID)
    : nsDocLoader(),
      mContentWindowID(aContentWindowID),
      mBrowsingContext(aBrowsingContext),
      mForcedCharset(nullptr),
      mParentCharset(nullptr),
      mTreeOwner(nullptr),
      mDefaultScrollbarPref(Scrollbar_Auto, Scrollbar_Auto),
      mCharsetReloadState(eCharsetReloadInit),
      mOrientationLock(hal::eScreenOrientation_None),
      mParentCharsetSource(0),
      mMarginWidth(-1),
      mMarginHeight(-1),
      mItemType(aBrowsingContext->IsContent() ? typeContent : typeChrome),
      mPreviousEntryIndex(-1),
      mLoadedEntryIndex(-1),
      mChildOffset(0),
      mSandboxFlags(0),
      mBusyFlags(BUSY_FLAGS_NONE),
      mAppType(nsIDocShell::APP_TYPE_UNKNOWN),
      mLoadType(0),
      mDefaultLoadFlags(nsIRequest::LOAD_NORMAL),
      mFailedLoadType(0),
      mFrameType(FRAME_TYPE_REGULAR),
      mPrivateBrowsingId(0),
      mDisplayMode(nsIDocShell::DISPLAY_MODE_BROWSER),
      mJSRunToCompletionDepth(0),
      mTouchEventsOverride(nsIDocShell::TOUCHEVENTS_OVERRIDE_NONE),
      mMetaViewportOverride(nsIDocShell::META_VIEWPORT_OVERRIDE_NONE),
      mFullscreenAllowed(CHECK_ATTRIBUTES),
      mCreatingDocument(false),
#ifdef DEBUG
      mInEnsureScriptEnv(false),
#endif
      mCreated(false),
      mAllowSubframes(true),
      mAllowPlugins(true),
      mAllowJavascript(true),
      mAllowMetaRedirects(true),
      mAllowImages(true),
      mAllowMedia(true),
      mAllowDNSPrefetch(true),
      mAllowWindowControl(true),
      mAllowContentRetargeting(true),
      mAllowContentRetargetingOnChildren(true),
      mUseErrorPages(false),
      mUseStrictSecurityChecks(false),
      mObserveErrorPages(true),
      mCSSErrorReportingEnabled(false),
      mAllowAuth(mItemType == typeContent),
      mAllowKeywordFixup(false),
      mIsOffScreenBrowser(false),
      mIsActive(true),
      mDisableMetaRefreshWhenInactive(false),
      mIsAppTab(false),
      mUseGlobalHistory(false),
      mUseRemoteTabs(false),
      mUseRemoteSubframes(false),
      mUseTrackingProtection(false),
      mDeviceSizeIsPageSize(false),
      mWindowDraggingAllowed(false),
      mInFrameSwap(false),
      mInheritPrivateBrowsingId(true),
      mCanExecuteScripts(false),
      mFiredUnloadEvent(false),
      mEODForCurrentDocument(false),
      mURIResultedInDocument(false),
      mIsBeingDestroyed(false),
      mIsExecutingOnLoadHandler(false),
      mIsPrintingOrPP(false),
      mSavingOldViewer(false),
      mDynamicallyCreated(false),
      mAffectPrivateSessionLifetime(true),
      mInvisible(false),
      mHasLoadedNonBlankURI(false),
      mBlankTiming(false),
      mTitleValidForCurrentURI(false),
      mIsFrame(false),
      mSkipBrowsingContextDetachOnDestroy(false),
      mWatchedByDevtools(false),
      mIsNavigating(false) {
  mHistoryID.m0 = 0;
  mHistoryID.m1 = 0;
  mHistoryID.m2 = 0;
  AssertOriginAttributesMatchPrivateBrowsing();

  nsContentUtils::GenerateUUIDInPlace(mHistoryID);

  // If no outer window ID was provided, generate a new one.
  if (aContentWindowID == 0) {
    mContentWindowID = nsContentUtils::GenerateWindowId();
  }

  if (gDocShellCount++ == 0) {
    NS_ASSERTION(sURIFixup == nullptr,
                 "Huh, sURIFixup not null in first nsDocShell ctor!");

    nsCOMPtr<nsIURIFixup> uriFixup = components::URIFixup::Service();
    uriFixup.forget(&sURIFixup);
  }

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p created\n", this));

#ifdef DEBUG
  // We're counting the number of |nsDocShells| to help find leaks
  ++gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("++DOCSHELL %p == %ld [pid = %d] [id = %s]\n", (void*)this,
                  gNumberOfDocShells, getpid(),
                  nsIDToCString(mHistoryID).get());
  }
#endif
}

nsDocShell::~nsDocShell() {
  MOZ_ASSERT(!mObserved);

  // Avoid notifying observers while we're in the dtor.
  mIsBeingDestroyed = true;

#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_pages(mHistoryID);
#endif

  Destroy();

  if (mSessionHistory) {
    mSessionHistory->LegacySHistory()->ClearRootDocShell();
  }

  if (--gDocShellCount == 0) {
    NS_IF_RELEASE(sURIFixup);
  }

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug, ("DOCSHELL %p destroyed\n", this));

#ifdef DEBUG
  nsAutoCString url;
  if (mLastOpenedURI) {
    url = mLastOpenedURI->GetSpecOrDefault();

    // Data URLs can be very long, so truncate to avoid flooding the log.
    const uint32_t maxURLLength = 1000;
    if (url.Length() > maxURLLength) {
      url.Truncate(maxURLLength);
    }
  }
  // We're counting the number of |nsDocShells| to help find leaks
  --gNumberOfDocShells;
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("--DOCSHELL %p == %ld [pid = %d] [id = %s] [url = %s]\n",
                  (void*)this, gNumberOfDocShells, getpid(),
                  nsIDToCString(mHistoryID).get(), url.get());
  }
#endif
}

/* static */
already_AddRefed<nsDocShell> nsDocShell::Create(
    BrowsingContext* aBrowsingContext, uint64_t aContentWindowID) {
  MOZ_ASSERT(aBrowsingContext, "DocShell without a BrowsingContext!");

  nsresult rv;
  RefPtr<nsDocShell> ds = new nsDocShell(aBrowsingContext, aContentWindowID);

  // Initialize the underlying nsDocLoader.
  rv = ds->nsDocLoader::Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // Create our ContentListener
  ds->mContentListener = new nsDSURIContentListener(ds);
  rv = ds->mContentListener->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // If parent intercept is not enabled then we must forward to
  // the network controller from docshell.  We also enable if we're
  // in the parent process in order to support non-e10s configurations.
  // Note: This check is duplicated in SharedWorkerInterfaceRequestor's
  // constructor.
  if (!ServiceWorkerParentInterceptEnabled() || XRE_IsParentProcess()) {
    ds->mInterceptController = new ServiceWorkerInterceptController();
  }

  // We want to hold a strong ref to the loadgroup, so it better hold a weak
  // ref to us...  use an InterfaceRequestorProxy to do this.
  nsCOMPtr<nsIInterfaceRequestor> proxy = new InterfaceRequestorProxy(ds);
  ds->mLoadGroup->SetNotificationCallbacks(proxy);

  // XXX(nika): We have our BrowsingContext, so we might be able to skip this.
  // It could be nice to directly set up our DocLoader tree?
  rv = nsDocLoader::AddDocLoaderAsChildOfRoot(ds);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // Add |ds| as a progress listener to itself.  A little weird, but simpler
  // than reproducing all the listener-notification logic in overrides of the
  // various methods via which nsDocLoader can be notified.   Note that this
  // holds an nsWeakPtr to |ds|, so it's ok.
  rv = ds->AddProgressListener(ds, nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                       nsIWebProgress::NOTIFY_STATE_NETWORK);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  // If our parent is present in this process, set up our parent now.
  RefPtr<BrowsingContext> parent = aBrowsingContext->GetParent();
  if (parent && parent->GetDocShell()) {
    parent->GetDocShell()->AddChild(ds);
  }

  // Make |ds| the primary DocShell for the given context.
  aBrowsingContext->SetDocShell(ds);

  return ds.forget();
}

void nsDocShell::DestroyChildren() {
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

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsDocShell, nsDocLoader,
                                   mSessionStorageManager, mScriptGlobal,
                                   mInitialClientSource, mSessionHistory,
                                   mBrowsingContext, mChromeEventHandler)

NS_IMPL_ADDREF_INHERITED(nsDocShell, nsDocLoader)
NS_IMPL_RELEASE_INHERITED(nsDocShell, nsDocLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDocShell)
  NS_INTERFACE_MAP_ENTRY(nsIDocShell)
  NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
  NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIScrollable)
  NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIWebPageDescriptor)
  NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
  NS_INTERFACE_MAP_ENTRY(nsILoadContext)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageManager)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsINetworkInterceptController,
                                     mInterceptController)
  NS_INTERFACE_MAP_ENTRY(nsIDeprecationWarner)
NS_INTERFACE_MAP_END_INHERITING(nsDocLoader)

NS_IMETHODIMP
nsDocShell::GetInterface(const nsIID& aIID, void** aSink) {
  MOZ_ASSERT(aSink, "null out param");

  *aSink = nullptr;

  if (aIID.Equals(NS_GET_IID(nsICommandManager))) {
    NS_ENSURE_SUCCESS(EnsureCommandHandler(), NS_ERROR_FAILURE);
    *aSink = static_cast<nsICommandManager*>(mCommandManager.get());
  } else if (aIID.Equals(NS_GET_IID(nsIURIContentListener))) {
    *aSink = mContentListener;
  } else if ((aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)) ||
              aIID.Equals(NS_GET_IID(nsIGlobalObject)) ||
              aIID.Equals(NS_GET_IID(nsPIDOMWindowOuter)) ||
              aIID.Equals(NS_GET_IID(mozIDOMWindowProxy)) ||
              aIID.Equals(NS_GET_IID(nsIDOMWindow))) &&
             NS_SUCCEEDED(EnsureScriptEnvironment())) {
    return mScriptGlobal->QueryInterface(aIID, aSink);
  } else if (aIID.Equals(NS_GET_IID(Document)) &&
             NS_SUCCEEDED(EnsureContentViewer())) {
    RefPtr<Document> doc = mContentViewer->GetDocument();
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

    RefPtr<Document> doc = contentViewer->GetDocument();
    NS_ASSERTION(doc, "Should have a document.");
    if (!doc) {
      return NS_ERROR_NO_INTERFACE;
    }

#if defined(DEBUG)
    MOZ_LOG(
        gDocShellLog, LogLevel::Debug,
        ("nsDocShell[%p]: returning app cache container %p", this, doc.get()));
#endif
    return doc->QueryInterface(aIID, aSink);
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
    return NS_SUCCEEDED(GetAuthPrompt(PROMPT_NORMAL, aIID, aSink))
               ? NS_OK
               : NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsISHistory))) {
    RefPtr<ChildSHistory> shistory = GetSessionHistory();
    if (shistory) {
      // XXX(nika): Stop exposing nsISHistory through GetInterface.
      nsCOMPtr<nsISHistory> legacy = shistory->LegacySHistory();
      legacy.forget(aSink);
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
  } else if (aIID.Equals(NS_GET_IID(nsISelectionDisplay))) {
    if (PresShell* presShell = GetPresShell()) {
      return presShell->QueryInterface(aIID, aSink);
    }
  } else if (aIID.Equals(NS_GET_IID(nsIDocShellTreeOwner))) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    nsresult rv = GetTreeOwner(getter_AddRefs(treeOwner));
    if (NS_SUCCEEDED(rv) && treeOwner) {
      return treeOwner->QueryInterface(aIID, aSink);
    }
  } else if (aIID.Equals(NS_GET_IID(nsIBrowserChild))) {
    *aSink = GetBrowserChild().take();
    return *aSink ? NS_OK : NS_ERROR_FAILURE;
  } else {
    return nsDocLoader::GetInterface(aIID, aSink);
  }

  NS_IF_ADDREF(((nsISupports*)*aSink));
  return *aSink ? NS_OK : NS_NOINTERFACE;
}

NS_IMETHODIMP
nsDocShell::SetCancelContentJSEpoch(int32_t aEpoch) {
  // Note: this gets called fairly early (before a pageload actually starts).
  // We could probably defer this even longer.
  nsCOMPtr<nsIBrowserChild> browserChild = GetBrowserChild();
  static_cast<BrowserChild*>(browserChild.get())
      ->SetCancelContentJSEpoch(aEpoch);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::LoadURI(nsDocShellLoadState* aLoadState) {
  MOZ_ASSERT(aLoadState, "Must have a valid load state!");
  MOZ_ASSERT(
      (aLoadState->LoadFlags() & INTERNAL_LOAD_FLAGS_LOADURI_SETUP_FLAGS) == 0,
      "Should not have these flags set");

  if (!aLoadState->TriggeringPrincipal()) {
#ifndef ANDROID
    MOZ_ASSERT(false, "LoadURI must have a triggering principal");
#endif
    if (mUseStrictSecurityChecks) {
      return NS_ERROR_FAILURE;
    }
  }

  // Note: we allow loads to get through here even if mFiredUnloadEvent is
  // true; that case will get handled in LoadInternal or LoadHistoryEntry,
  // so we pass false as the second parameter to IsNavigationAllowed.
  // However, we don't allow the page to change location *in the middle of*
  // firing beforeunload, so we do need to check if *beforeunload* is currently
  // firing, so we call IsNavigationAllowed rather than just IsPrintingOrPP.
  if (!IsNavigationAllowed(true, false)) {
    return NS_OK;  // JS may not handle returning of an error code
  }

  if (!StartupTimeline::HasRecord(StartupTimeline::FIRST_LOAD_URI) &&
      mItemType == typeContent && !NS_IsAboutBlank(aLoadState->URI())) {
    StartupTimeline::RecordOnce(StartupTimeline::FIRST_LOAD_URI);
  }

  // LoadType used to be set to a default value here, if no LoadInfo/LoadState
  // object was passed in. That functionality has been removed as of bug
  // 1492648. LoadType should now be set up by the caller at the time they
  // create their nsDocShellLoadState object to pass into LoadURI.

  MOZ_LOG(
      gDocShellLeakLog, LogLevel::Debug,
      ("nsDocShell[%p]: loading %s with flags 0x%08x", this,
       aLoadState->URI()->GetSpecOrDefault().get(), aLoadState->LoadFlags()));

  if (!aLoadState->SHEntry() &&
      !LOAD_TYPE_HAS_FLAGS(aLoadState->LoadType(),
                           LOAD_FLAGS_REPLACE_HISTORY)) {
    // This is possibly a subframe, so handle it accordingly.
    //
    // If history exists, it will be loaded into the aLoadState object, and the
    // LoadType will be changed.
    MaybeHandleSubframeHistory(aLoadState);
  }

  if (aLoadState->SHEntry()) {
#ifdef DEBUG
    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]: loading from session history", this));
#endif

    return LoadHistoryEntry(aLoadState->SHEntry(), aLoadState->LoadType());
  }

  // On history navigation via Back/Forward buttons, don't execute
  // automatic JavaScript redirection such as |location.href = ...| or
  // |window.open()|
  //
  // LOAD_NORMAL:        window.open(...) etc.
  // LOAD_STOP_CONTENT:  location.href = ..., location.assign(...)
  if ((aLoadState->LoadType() == LOAD_NORMAL ||
       aLoadState->LoadType() == LOAD_STOP_CONTENT) &&
      ShouldBlockLoadingForBackButton()) {
    return NS_OK;
  }

  // Set up the inheriting principal in LoadState.
  nsresult rv =
      aLoadState->SetupInheritingPrincipal(mItemType, mOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aLoadState->SetupTriggeringPrincipal(mOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  aLoadState->CalculateLoadURIFlags();

  MOZ_ASSERT(aLoadState->TypeHint().IsVoid(),
             "Typehint should be null when calling InternalLoad from LoadURI");
  MOZ_ASSERT(aLoadState->FileName().IsVoid(),
             "FileName should be null when calling InternalLoad from LoadURI");
  MOZ_ASSERT(aLoadState->SHEntry() == nullptr,
             "SHEntry should be null when calling InternalLoad from LoadURI");

  return InternalLoad(aLoadState,
                      nullptr,   // no nsIDocShell
                      nullptr);  // no nsIRequest
}

void nsDocShell::MaybeHandleSubframeHistory(nsDocShellLoadState* aLoadState) {
  // First, verify if this is a subframe.
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  GetInProcessSameTypeParent(getter_AddRefs(parentAsItem));
  nsCOMPtr<nsIDocShell> parentDS(do_QueryInterface(parentAsItem));

  if (!parentDS || parentDS == static_cast<nsIDocShell*>(this)) {
    // This is the root docshell. If we got here while
    // executing an onLoad Handler,this load will not go
    // into session history.
    bool inOnLoadHandler = false;
    GetIsExecutingOnLoadHandler(&inOnLoadHandler);
    if (inOnLoadHandler) {
      aLoadState->SetLoadType(LOAD_NORMAL_REPLACE);
    }
    return;
  }

  /* OK. It is a subframe. Checkout the parent's loadtype. If the parent was
   * loaded through a history mechanism, then get the SH entry for the child
   * from the parent. This is done to restore frameset navigation while going
   * back/forward. If the parent was loaded through any other loadType, set the
   * child's loadType too accordingly, so that session history does not get
   * confused.
   */

  // Get the parent's load type
  uint32_t parentLoadType;
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
    nsCOMPtr<nsISHEntry> shEntry;
    parentDS->GetChildSHEntry(mChildOffset, getter_AddRefs(shEntry));
    aLoadState->SetSHEntry(shEntry);
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

  if (mCurrentURI && (!NS_IsAboutBlank(mCurrentURI) || currentChildEntry)) {
    // This is a pre-existing subframe. If
    // 1. The load of this frame was not originally initiated by session
    //    history directly (i.e. (!shEntry) condition succeeded, but it can
    //    still be a history load on parent which causes this frame being
    //    loaded), which we checked with the above assert, and
    // 2. mCurrentURI is not null, nor the initial about:blank,
    // it is possible that a parent's onLoadHandler or even self's
    // onLoadHandler is loading a new page in this child. Check parent's and
    // self's busy flag and if it is set, we don't want this onLoadHandler
    // load to get in to session history.
    BusyFlags parentBusy = parentDS->GetBusyFlags();
    BusyFlags selfBusy = GetBusyFlags();

    if (parentBusy & BUSY_FLAGS_BUSY || selfBusy & BUSY_FLAGS_BUSY) {
      aLoadState->SetLoadType(LOAD_NORMAL_REPLACE);
      aLoadState->SetSHEntry(nullptr);
    }
    return;
  }

  // This is a newly created frame. Check for exception cases first.
  // By default the subframe will inherit the parent's loadType.
  if (aLoadState->SHEntry() &&
      (parentLoadType == LOAD_NORMAL || parentLoadType == LOAD_LINK ||
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
      aLoadState->SetLoadType(LOAD_NORMAL_REPLACE);
      aLoadState->SetSHEntry(nullptr);
    }
  } else if (parentLoadType == LOAD_REFRESH) {
    // Clear shEntry. For refresh loads, we have to load
    // what comes through the pipe, not what's in history.
    aLoadState->SetSHEntry(nullptr);
  } else if ((parentLoadType == LOAD_BYPASS_HISTORY) ||
             (aLoadState->SHEntry() &&
              ((parentLoadType & LOAD_CMD_HISTORY) ||
               (parentLoadType == LOAD_RELOAD_NORMAL) ||
               (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE) ||
               (parentLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE) ||
               (parentLoadType ==
                LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE)))) {
    // If the parent url, bypassed history or was loaded from
    // history, pass on the parent's loadType to the new child
    // frame too, so that the child frame will also
    // avoid getting into history.
    aLoadState->SetLoadType(parentLoadType);
  } else if (parentLoadType == LOAD_ERROR_PAGE) {
    // If the parent document is an error page, we don't
    // want to update global/session history. However,
    // this child frame is not an error page.
    aLoadState->SetLoadType(LOAD_BYPASS_HISTORY);
  } else if ((parentLoadType == LOAD_RELOAD_BYPASS_CACHE) ||
             (parentLoadType == LOAD_RELOAD_BYPASS_PROXY) ||
             (parentLoadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE)) {
    // the new frame should inherit the parent's load type so that it also
    // bypasses the cache and/or proxy
    aLoadState->SetLoadType(parentLoadType);
  }
}

/*
 * Reset state to a new content model within the current document and the
 * document viewer. Called by the document before initiating an out of band
 * document.write().
 */
NS_IMETHODIMP
nsDocShell::PrepareForNewContentModel() {
  mEODForCurrentDocument = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::FirePageHideNotification(bool aIsUnload) {
  FirePageHideNotificationInternal(aIsUnload, false);
  return NS_OK;
}

void nsDocShell::FirePageHideNotificationInternal(
    bool aIsUnload, bool aSkipCheckingDynEntries) {
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
      RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
      if (rootSH && mOSHE) {
        int32_t index = rootSH->Index();
        rootSH->LegacySHistory()->RemoveDynEntries(index, mOSHE);
      }
    }

    // Now make sure our editor, if any, is detached before we go
    // any farther.
    DetachEditorFromWindow();
  }
}

nsresult nsDocShell::DispatchToTabGroup(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  // Hold the ref so we won't forget to release it.
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    // Window should only be unavailable after destroyed.
    MOZ_ASSERT(mIsBeingDestroyed);
    return NS_ERROR_FAILURE;
  }

  if (win->GetDocGroup()) {
    return win->GetDocGroup()->Dispatch(aCategory, runnable.forget());
  }
  RefPtr<mozilla::dom::TabGroup> tabGroup = win->TabGroup();
  return tabGroup->Dispatch(aCategory, runnable.forget());
}

NS_IMETHODIMP
nsDocShell::DispatchLocationChangeEvent() {
  return DispatchToTabGroup(
      TaskCategory::Other,
      NewRunnableMethod("nsDocShell::FireDummyOnLocationChange", this,
                        &nsDocShell::FireDummyOnLocationChange));
}

NS_IMETHODIMP
nsDocShell::StartDelayedAutoplayMediaComponents() {
  RefPtr<nsPIDOMWindowOuter> outerWindow = GetWindow();
  if (outerWindow) {
    outerWindow->SetMediaSuspend(nsISuspendedTypes::NONE_SUSPENDED);
  }
  return NS_OK;
}

bool nsDocShell::MaybeInitTiming() {
  if (mTiming && !mBlankTiming) {
    return false;
  }

  bool canBeReset = false;

  if (mScriptGlobal && mBlankTiming) {
    nsPIDOMWindowInner* innerWin = mScriptGlobal->GetCurrentInnerWindow();
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

void nsDocShell::MaybeResetInitTiming(bool aReset) {
  if (aReset) {
    mTiming = nullptr;
  }
}

nsDOMNavigationTiming* nsDocShell::GetNavigationTiming() const {
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
/* static */
bool nsDocShell::ValidateOrigin(nsIDocShellTreeItem* aOriginTreeItem,
                                nsIDocShellTreeItem* aTargetTreeItem) {
  // We want to bypass this check for chrome callers, but only if there's
  // JS on the stack. System callers still need to do it.
  if (nsContentUtils::GetCurrentJSContext() &&
      nsContentUtils::IsCallerChrome()) {
    return true;
  }

  MOZ_ASSERT(aOriginTreeItem && aTargetTreeItem, "need two docshells");

  // Get origin document principal
  RefPtr<Document> originDocument = aOriginTreeItem->GetDocument();
  NS_ENSURE_TRUE(originDocument, false);

  // Get target principal
  RefPtr<Document> targetDocument = aTargetTreeItem->GetDocument();
  NS_ENSURE_TRUE(targetDocument, false);

  bool equal;
  nsresult rv = originDocument->NodePrincipal()->Equals(
      targetDocument->NodePrincipal(), &equal);
  if (NS_SUCCEEDED(rv) && equal) {
    return true;
  }

  // Not strictly equal, special case if both are file: uris
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

  return innerOriginURI && innerTargetURI && SchemeIsFile(innerOriginURI) &&
         SchemeIsFile(innerTargetURI);
}

nsPresContext* nsDocShell::GetEldestPresContext() {
  nsIContentViewer* viewer = mContentViewer;
  while (viewer) {
    nsIContentViewer* prevViewer = viewer->GetPreviousViewer();
    if (!prevViewer) {
      return viewer->GetPresContext();
    }
    viewer = prevViewer;
  }

  return nullptr;
}

nsPresContext* nsDocShell::GetPresContext() {
  if (!mContentViewer) {
    return nullptr;
  }

  return mContentViewer->GetPresContext();
}

PresShell* nsDocShell::GetPresShell() {
  nsPresContext* presContext = GetPresContext();
  return presContext ? presContext->GetPresShell() : nullptr;
}

PresShell* nsDocShell::GetEldestPresShell() {
  nsPresContext* presContext = GetEldestPresContext();

  if (presContext) {
    return presContext->GetPresShell();
  }

  return nullptr;
}

NS_IMETHODIMP
nsDocShell::GetContentViewer(nsIContentViewer** aContentViewer) {
  NS_ENSURE_ARG_POINTER(aContentViewer);

  *aContentViewer = mContentViewer;
  NS_IF_ADDREF(*aContentViewer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetOuterWindowID(uint64_t* aWindowID) {
  *aWindowID = mContentWindowID;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetChromeEventHandler(EventTarget* aChromeEventHandler) {
  mChromeEventHandler = aChromeEventHandler;

  if (mScriptGlobal) {
    mScriptGlobal->SetChromeEventHandler(mChromeEventHandler);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetChromeEventHandler(EventTarget** aChromeEventHandler) {
  NS_ENSURE_ARG_POINTER(aChromeEventHandler);
  RefPtr<EventTarget> handler = mChromeEventHandler;
  handler.forget(aChromeEventHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurrentURI(nsIURI* aURI) {
  // Note that securityUI will set STATE_IS_INSECURE, even if
  // the scheme of |aURI| is "https".
  SetCurrentURI(aURI, nullptr, true, 0);
  return NS_OK;
}

bool nsDocShell::SetCurrentURI(nsIURI* aURI, nsIRequest* aRequest,
                               bool aFireOnLocationChange,
                               uint32_t aLocationFlags) {
  MOZ_ASSERT(!mIsBeingDestroyed);

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug,
          ("DOCSHELL %p SetCurrentURI %s\n", this,
           aURI ? aURI->GetSpecOrDefault().get() : ""));

  // We don't want to send a location change when we're displaying an error
  // page, and we don't want to change our idea of "current URI" either
  if (mLoadType == LOAD_ERROR_PAGE) {
    return false;
  }

  bool uriIsEqual = false;
  if (!mCurrentURI || !aURI ||
      NS_FAILED(mCurrentURI->Equals(aURI, &uriIsEqual)) || !uriIsEqual) {
    mTitleValidForCurrentURI = false;
  }

  mCurrentURI = aURI;

#ifdef DEBUG
  mLastOpenedURI = aURI;
#endif

  if (!NS_IsAboutBlank(mCurrentURI)) {
    mHasLoadedNonBlankURI = true;
  }

  bool isRoot = false;      // Is this the root docshell
  bool isSubFrame = false;  // Is this a subframe navigation?

  nsCOMPtr<nsIDocShellTreeItem> root;

  GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  if (root.get() == static_cast<nsIDocShellTreeItem*>(this)) {
    // This is the root docshell
    isRoot = true;
  }
  if (mLSHE) {
    isSubFrame = mLSHE->GetIsSubFrame();
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
nsDocShell::GetCharset(nsACString& aCharset) {
  aCharset.Truncate();

  PresShell* presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  Document* doc = presShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  doc->GetDocumentCharacterSet()->Name(aCharset);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GatherCharsetMenuTelemetry() {
  nsCOMPtr<nsIContentViewer> viewer;
  GetContentViewer(getter_AddRefs(viewer));
  if (!viewer) {
    return NS_OK;
  }

  Document* doc = viewer->GetDocument();
  if (!doc || doc->WillIgnoreCharsetOverride()) {
    return NS_OK;
  }

  Telemetry::ScalarSet(Telemetry::ScalarID::ENCODING_OVERRIDE_USED, true);

  nsIURI* url = doc->GetOriginalURI();
  bool isFileURL = url && SchemeIsFile(url);

  int32_t charsetSource = doc->GetDocumentCharacterSetSource();
  switch (charsetSource) {
    case kCharsetFromTopLevelDomain:
      // Unlabeled doc on a domain that we map to a fallback encoding
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::RemoteTld);
      break;
    case kCharsetFromFallback:
    case kCharsetFromDocTypeDefault:
    case kCharsetFromCache:
    case kCharsetFromParentFrame:
      // Changing charset on an unlabeled doc.
      if (isFileURL) {
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::Local);
      } else {
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::RemoteNonTld);
      }
      break;
    case kCharsetFromAutoDetection:
      // Changing charset on unlabeled doc where chardet fired
      if (isFileURL) {
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::LocalChardet);
      } else {
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::RemoteChardet);
      }
      break;
    case kCharsetFromMetaPrescan:
    case kCharsetFromMetaTag:
    case kCharsetFromChannel:
      // Changing charset on a doc that had a charset label.
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::Labeled);
      break;
    case kCharsetFromParentForced:
    case kCharsetFromUserForced:
      // Changing charset on a document that already had an override.
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::AlreadyOverridden);
      break;
    case kCharsetFromIrreversibleAutoDetection:
    case kCharsetFromOtherComponent:
    case kCharsetFromByteOrderMark:
    case kCharsetUninitialized:
    default:
      // Bug. This isn't supposed to happen.
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_ENCODING_OVERRIDE_SITUATION::Bug);
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCharset(const nsACString& aCharset) {
  // set the charset override
  return SetForcedCharset(aCharset);
}

NS_IMETHODIMP
nsDocShell::SetForcedCharset(const nsACString& aCharset) {
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
nsDocShell::GetForcedCharset(nsACString& aResult) {
  if (mForcedCharset) {
    mForcedCharset->Name(aResult);
  } else {
    aResult.Truncate();
  }
  return NS_OK;
}

void nsDocShell::SetParentCharset(const Encoding*& aCharset,
                                  int32_t aCharsetSource,
                                  nsIPrincipal* aPrincipal) {
  mParentCharset = aCharset;
  mParentCharsetSource = aCharsetSource;
  mParentCharsetPrincipal = aPrincipal;
}

void nsDocShell::GetParentCharset(const Encoding*& aCharset,
                                  int32_t* aCharsetSource,
                                  nsIPrincipal** aPrincipal) {
  aCharset = mParentCharset;
  *aCharsetSource = mParentCharsetSource;
  NS_IF_ADDREF(*aPrincipal = mParentCharsetPrincipal);
}

NS_IMETHODIMP
nsDocShell::GetHasMixedActiveContentLoaded(bool* aHasMixedActiveContentLoaded) {
  RefPtr<Document> doc(GetDocument());
  *aHasMixedActiveContentLoaded = doc && doc->GetHasMixedActiveContentLoaded();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedActiveContentBlocked(
    bool* aHasMixedActiveContentBlocked) {
  RefPtr<Document> doc(GetDocument());
  *aHasMixedActiveContentBlocked =
      doc && doc->GetHasMixedActiveContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedDisplayContentLoaded(
    bool* aHasMixedDisplayContentLoaded) {
  RefPtr<Document> doc(GetDocument());
  *aHasMixedDisplayContentLoaded =
      doc && doc->GetHasMixedDisplayContentLoaded();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasMixedDisplayContentBlocked(
    bool* aHasMixedDisplayContentBlocked) {
  RefPtr<Document> doc(GetDocument());
  *aHasMixedDisplayContentBlocked =
      doc && doc->GetHasMixedDisplayContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasTrackingContentBlocked(bool* aHasTrackingContentBlocked) {
  RefPtr<Document> doc(GetDocument());
  *aHasTrackingContentBlocked = doc && doc->GetHasTrackingContentBlocked();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowPlugins(bool* aAllowPlugins) {
  NS_ENSURE_ARG_POINTER(aAllowPlugins);

  *aAllowPlugins = mAllowPlugins;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowPlugins(bool aAllowPlugins) {
  mAllowPlugins = aAllowPlugins;
  // XXX should enable or disable a plugin host
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowJavascript(bool* aAllowJavascript) {
  NS_ENSURE_ARG_POINTER(aAllowJavascript);

  *aAllowJavascript = mAllowJavascript;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCssErrorReportingEnabled(bool* aEnabled) {
  MOZ_ASSERT(aEnabled);
  *aEnabled = mCSSErrorReportingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCssErrorReportingEnabled(bool aEnabled) {
  mCSSErrorReportingEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowJavascript(bool aAllowJavascript) {
  mAllowJavascript = aAllowJavascript;
  RecomputeCanExecuteScripts();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUsePrivateBrowsing(bool* aUsePrivateBrowsing) {
  NS_ENSURE_ARG_POINTER(aUsePrivateBrowsing);
  AssertOriginAttributesMatchPrivateBrowsing();
  *aUsePrivateBrowsing = mPrivateBrowsingId > 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUsePrivateBrowsing(bool aUsePrivateBrowsing) {
  if (!CanSetOriginAttributes()) {
    bool changed = aUsePrivateBrowsing != (mPrivateBrowsingId > 0);

    return changed ? NS_ERROR_FAILURE : NS_OK;
  }

  return SetPrivateBrowsing(aUsePrivateBrowsing);
}

NS_IMETHODIMP
nsDocShell::SetPrivateBrowsing(bool aUsePrivateBrowsing) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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
nsDocShell::GetHasLoadedNonBlankURI(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mHasLoadedNonBlankURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseRemoteTabs(bool* aUseRemoteTabs) {
  NS_ENSURE_ARG_POINTER(aUseRemoteTabs);

  *aUseRemoteTabs = mUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetRemoteTabs(bool aUseRemoteTabs) {
  if (aUseRemoteTabs) {
    CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::DOMIPCEnabled,
                                       true);
  }

  // Don't allow non-remote tabs with remote subframes.
  if (NS_WARN_IF(!aUseRemoteTabs && mUseRemoteSubframes)) {
    return NS_ERROR_UNEXPECTED;
  }

  mUseRemoteTabs = aUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseRemoteSubframes(bool* aUseRemoteSubframes) {
  NS_ENSURE_ARG_POINTER(aUseRemoteSubframes);

  *aUseRemoteSubframes = mUseRemoteSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetRemoteSubframes(bool aUseRemoteSubframes) {
  static bool annotated = false;

  if (aUseRemoteSubframes && !annotated) {
    annotated = true;
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::DOMFissionEnabled, true);
  }

  // Don't allow non-remote tabs with remote subframes.
  if (NS_WARN_IF(aUseRemoteSubframes && !mUseRemoteTabs)) {
    return NS_ERROR_UNEXPECTED;
  }

  mUseRemoteSubframes = aUseRemoteSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAffectPrivateSessionLifetime(bool aAffectLifetime) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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
nsDocShell::GetAffectPrivateSessionLifetime(bool* aAffectLifetime) {
  *aAffectLifetime = mAffectPrivateSessionLifetime;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddWeakPrivacyTransitionObserver(
    nsIPrivacyTransitionObserver* aObserver) {
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mPrivacyObservers.AppendElement(weakObs);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddWeakReflowObserver(nsIReflowObserver* aObserver) {
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_FAILURE;
  }
  mReflowObservers.AppendElement(weakObs);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveWeakReflowObserver(nsIReflowObserver* aObserver) {
  nsWeakPtr obs = do_GetWeakReference(aObserver);
  return mReflowObservers.RemoveElement(obs) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::NotifyReflowObservers(bool aInterruptible,
                                  DOMHighResTimeStamp aStart,
                                  DOMHighResTimeStamp aEnd) {
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
nsDocShell::GetAllowMetaRedirects(bool* aReturn) {
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = mAllowMetaRedirects;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowMetaRedirects(bool aValue) {
  mAllowMetaRedirects = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowSubframes(bool* aAllowSubframes) {
  NS_ENSURE_ARG_POINTER(aAllowSubframes);

  *aAllowSubframes = mAllowSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowSubframes(bool aAllowSubframes) {
  mAllowSubframes = aAllowSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowImages(bool* aAllowImages) {
  NS_ENSURE_ARG_POINTER(aAllowImages);

  *aAllowImages = mAllowImages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowImages(bool aAllowImages) {
  mAllowImages = aAllowImages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowMedia(bool* aAllowMedia) {
  *aAllowMedia = mAllowMedia;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowMedia(bool aAllowMedia) {
  mAllowMedia = aAllowMedia;

  // Mute or unmute audio contexts attached to the inner window.
  if (mScriptGlobal) {
    if (nsPIDOMWindowInner* innerWin = mScriptGlobal->GetCurrentInnerWindow()) {
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
nsDocShell::GetAllowDNSPrefetch(bool* aAllowDNSPrefetch) {
  *aAllowDNSPrefetch = mAllowDNSPrefetch;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowDNSPrefetch(bool aAllowDNSPrefetch) {
  mAllowDNSPrefetch = aAllowDNSPrefetch;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowWindowControl(bool* aAllowWindowControl) {
  *aAllowWindowControl = mAllowWindowControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowWindowControl(bool aAllowWindowControl) {
  mAllowWindowControl = aAllowWindowControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowContentRetargeting(bool* aAllowContentRetargeting) {
  *aAllowContentRetargeting = mAllowContentRetargeting;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowContentRetargeting(bool aAllowContentRetargeting) {
  mAllowContentRetargetingOnChildren = aAllowContentRetargeting;
  mAllowContentRetargeting = aAllowContentRetargeting;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowContentRetargetingOnChildren(
    bool* aAllowContentRetargetingOnChildren) {
  *aAllowContentRetargetingOnChildren = mAllowContentRetargetingOnChildren;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowContentRetargetingOnChildren(
    bool aAllowContentRetargetingOnChildren) {
  mAllowContentRetargetingOnChildren = aAllowContentRetargetingOnChildren;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetInheritPrivateBrowsingId(bool* aInheritPrivateBrowsingId) {
  *aInheritPrivateBrowsingId = mInheritPrivateBrowsingId;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetInheritPrivateBrowsingId(bool aInheritPrivateBrowsingId) {
  mInheritPrivateBrowsingId = aInheritPrivateBrowsingId;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetFullscreenAllowed(bool* aFullscreenAllowed) {
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
  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
  if (!parent) {
    *aFullscreenAllowed = true;
    return NS_OK;
  }

  // Otherwise, we have a parent, continue the checking for
  // mozFullscreenAllowed in the parent docshell's ancestors.
  return parent->GetFullscreenAllowed(aFullscreenAllowed);
}

NS_IMETHODIMP
nsDocShell::SetFullscreenAllowed(bool aFullscreenAllowed) {
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

hal::ScreenOrientation nsDocShell::OrientationLock() {
  return mOrientationLock;
}

void nsDocShell::SetOrientationLock(hal::ScreenOrientation aOrientationLock) {
  mOrientationLock = aOrientationLock;
}

NS_IMETHODIMP
nsDocShell::GetMayEnableCharacterEncodingMenu(
    bool* aMayEnableCharacterEncodingMenu) {
  *aMayEnableCharacterEncodingMenu = false;
  if (!mContentViewer) {
    return NS_OK;
  }
  Document* doc = mContentViewer->GetDocument();
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
nsDocShell::GetCharsetAutodetected(bool* aCharsetAutodetected) {
  *aCharsetAutodetected = false;
  if (!mContentViewer) {
    return NS_OK;
  }
  Document* doc = mContentViewer->GetDocument();
  if (!doc) {
    return NS_OK;
  }
  int32_t source = doc->GetDocumentCharacterSetSource();

  if (source == kCharsetFromAutoDetection ||
      source == kCharsetFromUserForcedAutoDetection) {
    *aCharsetAutodetected = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDocShellEnumerator(int32_t aItemType,
                                  DocShellEnumeratorDirection aDirection,
                                  nsISimpleEnumerator** aResult) {
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
nsDocShell::GetAppType(AppType* aAppType) {
  *aAppType = mAppType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAppType(AppType aAppType) {
  mAppType = aAppType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowAuth(bool* aAllowAuth) {
  *aAllowAuth = mAllowAuth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetAllowAuth(bool aAllowAuth) {
  mAllowAuth = aAllowAuth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetZoom(float* aZoom) {
  NS_ENSURE_ARG_POINTER(aZoom);
  *aZoom = 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetZoom(float aZoom) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsDocShell::GetMarginWidth(int32_t* aWidth) {
  NS_ENSURE_ARG_POINTER(aWidth);

  *aWidth = mMarginWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginWidth(int32_t aWidth) {
  mMarginWidth = aWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMarginHeight(int32_t* aHeight) {
  NS_ENSURE_ARG_POINTER(aHeight);

  *aHeight = mMarginHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMarginHeight(int32_t aHeight) {
  mMarginHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetBusyFlags(BusyFlags* aBusyFlags) {
  NS_ENSURE_ARG_POINTER(aBusyFlags);

  *aBusyFlags = mBusyFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::TabToTreeOwner(bool aForward, bool aForDocumentNavigation,
                           bool* aTookFocus) {
  NS_ENSURE_ARG_POINTER(aTookFocus);

  nsCOMPtr<nsIWebBrowserChromeFocus> chromeFocus = do_GetInterface(mTreeOwner);
  if (chromeFocus) {
    if (aForward) {
      *aTookFocus =
          NS_SUCCEEDED(chromeFocus->FocusNextElement(aForDocumentNavigation));
    } else {
      *aTookFocus =
          NS_SUCCEEDED(chromeFocus->FocusPrevElement(aForDocumentNavigation));
    }
  } else {
    *aTookFocus = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSecurityUI(nsISecureBrowserUI** aSecurityUI) {
  NS_IF_ADDREF(*aSecurityUI = mSecurityUI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSecurityUI(nsISecureBrowserUI* aSecurityUI) {
  MOZ_ASSERT(!mIsBeingDestroyed);

  mSecurityUI = aSecurityUI;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadURIDelegate(nsILoadURIDelegate** aLoadURIDelegate) {
  NS_IF_ADDREF(*aLoadURIDelegate = mLoadURIDelegate);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLoadURIDelegate(nsILoadURIDelegate* aLoadURIDelegate) {
  mLoadURIDelegate = aLoadURIDelegate;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseErrorPages(bool* aUseErrorPages) {
  *aUseErrorPages = UseErrorPages();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseErrorPages(bool aUseErrorPages) {
  // If mUseErrorPages is set explicitly, stop using the
  // browser.xul.error_pages_enabled pref.
  if (mObserveErrorPages) {
    mObserveErrorPages = false;
  }
  mUseErrorPages = aUseErrorPages;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPreviousEntryIndex(int32_t* aPreviousEntryIndex) {
  *aPreviousEntryIndex = mPreviousEntryIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadedEntryIndex(int32_t* aLoadedEntryIndex) {
  *aLoadedEntryIndex = mLoadedEntryIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::HistoryPurged(int32_t aNumEntries) {
  // These indices are used for fastback cache eviction, to determine
  // which session history entries are candidates for content viewer
  // eviction.  We need to adjust by the number of entries that we
  // just purged from history, so that we look at the right session history
  // entries during eviction.
  mPreviousEntryIndex = std::max(-1, mPreviousEntryIndex - aNumEntries);
  mLoadedEntryIndex = std::max(0, mLoadedEntryIndex - aNumEntries);

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      shell->HistoryPurged(aNumEntries);
    }
  }

  return NS_OK;
}

nsresult nsDocShell::HistoryEntryRemoved(int32_t aIndex) {
  // These indices are used for fastback cache eviction, to determine
  // which session history entries are candidates for content viewer
  // eviction.  We need to adjust by the number of entries that we
  // just purged from history, so that we look at the right session history
  // entries during eviction.
  if (aIndex == mPreviousEntryIndex) {
    mPreviousEntryIndex = -1;
  } else if (aIndex < mPreviousEntryIndex) {
    --mPreviousEntryIndex;
  }
  if (mLoadedEntryIndex == aIndex) {
    mLoadedEntryIndex = 0;
  } else if (aIndex < mLoadedEntryIndex) {
    --mLoadedEntryIndex;
  }

  nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
  while (iter.HasMore()) {
    nsCOMPtr<nsIDocShell> shell = do_QueryObject(iter.GetNext());
    if (shell) {
      static_cast<nsDocShell*>(shell.get())->HistoryEntryRemoved(aIndex);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetRecordProfileTimelineMarkers(bool aValue) {
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
nsDocShell::GetRecordProfileTimelineMarkers(bool* aValue) {
  *aValue = !!mObserved;
  return NS_OK;
}

nsresult nsDocShell::PopProfileTimelineMarkers(
    JSContext* aCx, JS::MutableHandle<JS::Value> aOut) {
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

nsresult nsDocShell::Now(DOMHighResTimeStamp* aWhen) {
  *aWhen = (TimeStamp::Now() - TimeStamp::ProcessCreation()).ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetWindowDraggingAllowed(bool aValue) {
  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
  if (!aValue && mItemType == typeChrome && !parent) {
    // Window dragging is always allowed for top level
    // chrome docshells.
    return NS_ERROR_FAILURE;
  }
  mWindowDraggingAllowed = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetWindowDraggingAllowed(bool* aValue) {
  // window dragging regions in CSS (-moz-window-drag:drag)
  // can be slow. Default behavior is to only allow it for
  // chrome top level windows.
  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
  if (mItemType == typeChrome && !parent) {
    // Top level chrome window
    *aValue = true;
  } else {
    *aValue = mWindowDraggingAllowed;
  }
  return NS_OK;
}

nsIDOMStorageManager* nsDocShell::TopSessionStorageManager() {
  nsresult rv;

  nsCOMPtr<nsIDocShellTreeItem> topItem;
  rv = GetInProcessSameTypeRootTreeItem(getter_AddRefs(topItem));
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
    mSessionStorageManager = new SessionStorageManager();
  }

  return mSessionStorageManager;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDocumentChannel(nsIChannel** aResult) {
  NS_IF_ADDREF(*aResult = GetCurrentDocChannel());
  return NS_OK;
}

nsIChannel* nsDocShell::GetCurrentDocChannel() {
  if (mContentViewer) {
    Document* doc = mContentViewer->GetDocument();
    if (doc) {
      return doc->GetChannel();
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsDocShell::AddWeakScrollObserver(nsIScrollObserver* aObserver) {
  nsWeakPtr weakObs = do_GetWeakReference(aObserver);
  if (!weakObs) {
    return NS_ERROR_FAILURE;
  }
  mScrollObservers.AppendElement(weakObs);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveWeakScrollObserver(nsIScrollObserver* aObserver) {
  nsWeakPtr obs = do_GetWeakReference(aObserver);
  return mScrollObservers.RemoveElement(obs) ? NS_OK : NS_ERROR_FAILURE;
}

void nsDocShell::NotifyAsyncPanZoomStarted() {
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

void nsDocShell::NotifyAsyncPanZoomStopped() {
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
nsDocShell::NotifyScrollObservers() {
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
nsDocShell::GetName(nsAString& aName) {
  aName = mBrowsingContext->Name();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetName(const nsAString& aName) {
  mBrowsingContext->SetName(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::NameEquals(const nsAString& aName, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mBrowsingContext->NameEquals(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCustomUserAgent(nsAString& aCustomUserAgent) {
  aCustomUserAgent = mCustomUserAgent;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCustomUserAgent(const nsAString& aCustomUserAgent) {
  mCustomUserAgent = aCustomUserAgent;
  RefPtr<nsGlobalWindowInner> win =
      mScriptGlobal ? mScriptGlobal->GetCurrentInnerWindowInternal() : nullptr;
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
nsDocShell::GetTouchEventsOverride(TouchEventsOverride* aTouchEventsOverride) {
  *aTouchEventsOverride = mTouchEventsOverride;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTouchEventsOverride(TouchEventsOverride aTouchEventsOverride) {
  // We don't have a way to verify this coming from Javascript, so this check is
  // still needed.
  if (!(aTouchEventsOverride == TOUCHEVENTS_OVERRIDE_NONE ||
        aTouchEventsOverride == TOUCHEVENTS_OVERRIDE_ENABLED ||
        aTouchEventsOverride == TOUCHEVENTS_OVERRIDE_DISABLED)) {
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

NS_IMETHODIMP
nsDocShell::GetMetaViewportOverride(
    MetaViewportOverride* aMetaViewportOverride) {
  NS_ENSURE_ARG_POINTER(aMetaViewportOverride);

  *aMetaViewportOverride = mMetaViewportOverride;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMetaViewportOverride(
    MetaViewportOverride aMetaViewportOverride) {
  // We don't have a way to verify this coming from Javascript, so this check is
  // still needed.
  if (!(aMetaViewportOverride == META_VIEWPORT_OVERRIDE_NONE ||
        aMetaViewportOverride == META_VIEWPORT_OVERRIDE_ENABLED ||
        aMetaViewportOverride == META_VIEWPORT_OVERRIDE_DISABLED)) {
    return NS_ERROR_INVALID_ARG;
  }

  mMetaViewportOverride = aMetaViewportOverride;

  // Inform our presShell that it needs to re-check its need for a viewport
  // override.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->UpdateViewportOverridden(true);
  }

  return NS_OK;
}

/* virtual */
int32_t nsDocShell::ItemType() { return mItemType; }

NS_IMETHODIMP
nsDocShell::GetItemType(int32_t* aItemType) {
  NS_ENSURE_ARG_POINTER(aItemType);

  MOZ_DIAGNOSTIC_ASSERT(
      (mBrowsingContext->IsContent() ? typeContent : typeChrome) == mItemType);
  *aItemType = mItemType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetInProcessParent(nsIDocShellTreeItem** aParent) {
  if (!mParent) {
    *aParent = nullptr;
  } else {
    CallQueryInterface(mParent, aParent);
  }
  // Note that in the case when the parent is not an nsIDocShellTreeItem we
  // don't want to throw; we just want to return null.
  return NS_OK;
}

// With Fission, related nsDocShell objects may exist in a different process. In
// that case, this method will return `nullptr`, despite a parent nsDocShell
// object existing.
//
// Prefer using `BrowsingContext::Parent()`, which will succeed even if the
// parent entry is not in the current process, and handle the case where the
// parent nsDocShell is inaccessible.
already_AddRefed<nsDocShell> nsDocShell::GetInProcessParentDocshell() {
  nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(GetAsSupports(mParent));
  return docshell.forget().downcast<nsDocShell>();
}

void nsDocShell::MaybeCreateInitialClientSource(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(!mIsBeingDestroyed);

  // If there is an existing document then there is no need to create
  // a client for a future initial about:blank document.
  if (mScriptGlobal && mScriptGlobal->GetCurrentInnerWindowInternal() &&
      mScriptGlobal->GetCurrentInnerWindowInternal()->GetExtantDoc()) {
    MOZ_DIAGNOSTIC_ASSERT(mScriptGlobal->GetCurrentInnerWindowInternal()
                              ->GetClientInfo()
                              .isSome());
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

  nsIPrincipal* principal =
      aPrincipal ? aPrincipal : GetInheritedPrincipal(false);

  // Sometimes there is no principal available when we are called from
  // CreateAboutBlankContentViewer.  For example, sometimes the principal
  // is only extracted from the load context after the document is created
  // in Document::ResetToURI().  Ideally we would do something similar
  // here, but for now lets just avoid the issue by not preallocating the
  // client.
  if (!principal) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    return;
  }

  mInitialClientSource = ClientManager::CreateSource(
      ClientType::Window, win->EventTargetFor(TaskCategory::Other), principal);
  MOZ_DIAGNOSTIC_ASSERT(mInitialClientSource);

  // Mark the initial client as execution ready, but owned by the docshell.
  // If the client is actually used this will cause ClientSource to force
  // the creation of the initial about:blank by calling
  // nsDocShell::GetDocument().
  mInitialClientSource->DocShellExecutionReady(this);

  // Next, check to see if the parent is controlled.
  nsCOMPtr<nsIDocShell> parent = GetInProcessParentDocshell();
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
  if (controller.isNothing() ||
      !ServiceWorkerAllowedToControlWindow(principal, uri)) {
    return;
  }

  mInitialClientSource->InheritController(controller.ref());
}

Maybe<ClientInfo> nsDocShell::GetInitialClientInfo() const {
  if (mInitialClientSource) {
    Maybe<ClientInfo> result;
    result.emplace(mInitialClientSource->Info());
    return result;
  }

  nsGlobalWindowInner* innerWindow =
      mScriptGlobal ? mScriptGlobal->GetCurrentInnerWindowInternal() : nullptr;
  Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;

  if (!doc || !doc->IsInitialDocument()) {
    return Maybe<ClientInfo>();
  }

  return innerWindow->GetClientInfo();
}

void nsDocShell::RecomputeCanExecuteScripts() {
  bool old = mCanExecuteScripts;
  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();

  // If we have no tree owner, that means that we've been detached from the
  // docshell tree (this is distinct from having no parent docshell, which
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

nsresult nsDocShell::SetDocLoaderParent(nsDocLoader* aParent) {
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
    if (mAllowPlugins &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowPlugins(&value))) {
      SetAllowPlugins(value);
    }
    if (mAllowJavascript &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowJavascript(&value))) {
      SetAllowJavascript(value);
    }
    if (mAllowMetaRedirects &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowMetaRedirects(&value))) {
      SetAllowMetaRedirects(value);
    }
    if (mAllowSubframes &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowSubframes(&value))) {
      SetAllowSubframes(value);
    }
    if (mAllowImages &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowImages(&value))) {
      SetAllowImages(value);
    }
    SetAllowMedia(parentAsDocShell->GetAllowMedia() && mAllowMedia);
    if (mAllowWindowControl &&
        NS_SUCCEEDED(parentAsDocShell->GetAllowWindowControl(&value))) {
      SetAllowWindowControl(value);
    }
    SetAllowContentRetargeting(
        mAllowContentRetargeting &&
        parentAsDocShell->GetAllowContentRetargetingOnChildren());
    if (NS_SUCCEEDED(parentAsDocShell->GetIsActive(&value))) {
      SetIsActive(value);
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

    SetTouchEventsOverride(parentAsDocShell->GetTouchEventsOverride());

    // We don't need to inherit metaViewportOverride, because the viewport
    // is only relevant for the outermost nsDocShell, not for any iframes
    // like this that might be embedded within it.
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

  // Inform windows when they're being removed from their parent.
  if (!aParent) {
    MaybeClearStorageAccessFlag();
  }

  NS_ASSERTION(mInheritPrivateBrowsingId || wasPrivate == UsePrivateBrowsing(),
               "Private browsing state changed while inheritance was disabled");

  return NS_OK;
}

void nsDocShell::MaybeClearStorageAccessFlag() {
  if (mScriptGlobal) {
    // Tell our window that the parent has now changed.
    mScriptGlobal->ParentWindowChanged();

    // Tell all of our children about the change recursively as well.
    nsTObserverArray<nsDocLoader*>::ForwardIterator iter(mChildList);
    while (iter.HasMore()) {
      nsCOMPtr<nsIDocShell> child = do_QueryObject(iter.GetNext());
      if (child) {
        static_cast<nsDocShell*>(child.get())->MaybeClearStorageAccessFlag();
      }
    }
  }
}

NS_IMETHODIMP
nsDocShell::GetInProcessSameTypeParent(nsIDocShellTreeItem** aParent) {
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
nsDocShell::GetSameTypeParentIgnoreBrowserBoundaries(nsIDocShell** aParent) {
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
nsDocShell::GetInProcessRootTreeItem(nsIDocShellTreeItem** aRootTreeItem) {
  NS_ENSURE_ARG_POINTER(aRootTreeItem);

  RefPtr<nsDocShell> root = this;
  RefPtr<nsDocShell> parent = root->GetInProcessParentDocshell();
  while (parent) {
    root = parent;
    parent = root->GetInProcessParentDocshell();
  }

  root.forget(aRootTreeItem);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetInProcessSameTypeRootTreeItem(
    nsIDocShellTreeItem** aRootTreeItem) {
  NS_ENSURE_ARG_POINTER(aRootTreeItem);
  *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  NS_ENSURE_SUCCESS(GetInProcessSameTypeParent(getter_AddRefs(parent)),
                    NS_ERROR_FAILURE);
  while (parent) {
    *aRootTreeItem = parent;
    NS_ENSURE_SUCCESS(
        (*aRootTreeItem)->GetInProcessSameTypeParent(getter_AddRefs(parent)),
        NS_ERROR_FAILURE);
  }
  NS_ADDREF(*aRootTreeItem);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSameTypeRootTreeItemIgnoreBrowserBoundaries(
    nsIDocShell** aRootTreeItem) {
  NS_ENSURE_ARG_POINTER(aRootTreeItem);
  *aRootTreeItem = static_cast<nsIDocShell*>(this);

  nsCOMPtr<nsIDocShell> parent;
  NS_ENSURE_SUCCESS(
      GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent)),
      NS_ERROR_FAILURE);
  while (parent) {
    *aRootTreeItem = parent;
    NS_ENSURE_SUCCESS(
        (*aRootTreeItem)
            ->GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent)),
        NS_ERROR_FAILURE);
  }
  NS_ADDREF(*aRootTreeItem);
  return NS_OK;
}

/* static */
bool nsDocShell::CanAccessItem(nsIDocShellTreeItem* aTargetItem,
                               nsIDocShellTreeItem* aAccessingItem,
                               bool aConsiderOpener) {
  MOZ_ASSERT(aTargetItem, "Must have target item!");

  if (!aAccessingItem) {
    // Good to go
    return true;
  }

  nsCOMPtr<nsIDocShell> targetDS = do_QueryInterface(aTargetItem);
  nsCOMPtr<nsIDocShell> accessingDS = do_QueryInterface(aAccessingItem);
  if (!targetDS || !accessingDS) {
    // We must be able to convert both to nsIDocShell.
    return false;
  }

  return Cast(accessingDS)
      ->mBrowsingContext->CanAccess(Cast(targetDS)->mBrowsingContext,
                                    aConsiderOpener);
}

static bool ItemIsActive(nsIDocShellTreeItem* aItem) {
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
                             nsIDocShellTreeItem** aResult) {
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
      GetInProcessSameTypeParent(getter_AddRefs(foundItem));
      if (!foundItem) {
        foundItem = this;
      }
    } else if (aName.LowerCaseEqualsLiteral("_top")) {
      GetInProcessSameTypeRootTreeItem(getter_AddRefs(foundItem));
      NS_ASSERTION(foundItem, "Must have this; worst case it's us!");
    } else {
      // Do the search for item by an actual name.
      DoFindItemWithName(aName, aRequestor, aOriginalRequestor, aSkipTabGroup,
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

void nsDocShell::AssertOriginAttributesMatchPrivateBrowsing() {
  // Chrome docshells must not have a private browsing OriginAttribute
  // Content docshells must maintain the equality:
  // mOriginAttributes.mPrivateBrowsingId == mPrivateBrowsingId
  if (mItemType == typeChrome) {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId == 0);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(mOriginAttributes.mPrivateBrowsingId ==
                          mPrivateBrowsingId);
  }
}

nsresult nsDocShell::DoFindItemWithName(const nsAString& aName,
                                        nsIDocShellTreeItem* aRequestor,
                                        nsIDocShellTreeItem* aOriginalRequestor,
                                        bool aSkipTabGroup,
                                        nsIDocShellTreeItem** aResult) {
  // First we check our name.
  if (mBrowsingContext->NameEquals(aName) && ItemIsActive(this) &&
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
    // NOTE: Could use GetInProcessSameTypeParent if the issues described in
    // bug 1310344 are fixed.
    if (!GetIsMozBrowser() && parentAsTreeItem->ItemType() == mItemType) {
      return parentAsTreeItem->FindItemWithName(aName, this, aOriginalRequestor,
                                                /* aSkipTabGroup = */ false,
                                                aResult);
    }
  }

  // If we have a null parent or the parent is not of the same type, we need to
  // give up on finding it in our tree, and start looking in our TabGroup.
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (window && !aSkipTabGroup) {
    RefPtr<mozilla::dom::TabGroup> tabGroup = window->TabGroup();
    tabGroup->FindItemWithName(aName, this, aOriginalRequestor, aResult);
  }

  return NS_OK;
}

bool nsDocShell::IsSandboxedFrom(BrowsingContext* aTargetBC) {
  // If no target then not sandboxed.
  if (!aTargetBC) {
    return false;
  }

  // We cannot be sandboxed from ourselves.
  if (aTargetBC == mBrowsingContext) {
    return false;
  }

  // Default the sandbox flags to our flags, so that if we can't retrieve the
  // active document, we will still enforce our own.
  uint32_t sandboxFlags = mSandboxFlags;
  if (mContentViewer) {
    RefPtr<Document> doc = mContentViewer->GetDocument();
    if (doc) {
      sandboxFlags = doc->GetSandboxFlags();
    }
  }

  // If no flags, we are not sandboxed at all.
  if (!sandboxFlags) {
    return false;
  }

  // If aTargetBC has an ancestor, it is not top level.
  RefPtr<BrowsingContext> ancestorOfTarget(aTargetBC->GetParent());
  if (ancestorOfTarget) {
    do {
      // We are not sandboxed if we are an ancestor of target.
      if (ancestorOfTarget == mBrowsingContext) {
        return false;
      }
      ancestorOfTarget = ancestorOfTarget->GetParent();
    } while (ancestorOfTarget);

    // Otherwise, we are sandboxed from aTargetBC.
    return true;
  }

  // aTargetBC is top level, are we the "one permitted sandboxed
  // navigator", i.e. did we open aTargetBC?
  RefPtr<BrowsingContext> permittedNavigator(
      aTargetBC->GetOnePermittedSandboxedNavigator());
  if (permittedNavigator == mBrowsingContext) {
    return false;
  }

  // If SANDBOXED_TOPLEVEL_NAVIGATION flag is not on, we are not sandboxed
  // from our top.
  if (!(sandboxFlags & SANDBOXED_TOPLEVEL_NAVIGATION) &&
      aTargetBC == mBrowsingContext->Top()) {
    return false;
  }

  // Otherwise, we are sandboxed from aTargetBC.
  return true;
}

NS_IMETHODIMP
nsDocShell::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner) {
  NS_ENSURE_ARG_POINTER(aTreeOwner);

  *aTreeOwner = mTreeOwner;
  NS_IF_ADDREF(*aTreeOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner) {
  if (mIsBeingDestroyed && aTreeOwner) {
    return NS_ERROR_FAILURE;
  }

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

  // If we're in the content process and have had a TreeOwner set on us, extract
  // our BrowserChild actor. If we've already had our BrowserChild set, assert
  // that it hasn't changed.
  if (mTreeOwner && XRE_IsContentProcess()) {
    nsCOMPtr<nsIBrowserChild> newBrowserChild = do_GetInterface(mTreeOwner);
    MOZ_ASSERT(newBrowserChild,
               "No BrowserChild actor for tree owner in Content!");

    if (mBrowserChild) {
      nsCOMPtr<nsIBrowserChild> oldBrowserChild =
          do_QueryReferent(mBrowserChild);
      MOZ_RELEASE_ASSERT(
          oldBrowserChild == newBrowserChild,
          "Cannot cahnge BrowserChild during nsDocShell lifetime!");
    } else {
      mBrowserChild = do_GetWeakReference(newBrowserChild);
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

void nsDocShell::SetChildOffset(int32_t aChildOffset) {
  mChildOffset = aChildOffset;
}

int32_t nsDocShell::GetChildOffset() { return mChildOffset; }

NS_IMETHODIMP
nsDocShell::GetHistoryID(nsID** aID) {
  *aID = mHistoryID.Clone();
  return NS_OK;
}

const nsID nsDocShell::HistoryID() { return mHistoryID; }

NS_IMETHODIMP
nsDocShell::GetIsInUnload(bool* aIsInUnload) {
  *aIsInUnload = mFiredUnloadEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetInProcessChildCount(int32_t* aChildCount) {
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = mChildList.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::AddChild(nsIDocShellTreeItem* aChild) {
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

  Cast(childDocShell)->SetRemoteTabs(mUseRemoteTabs);
  Cast(childDocShell)->SetRemoteSubframes(mUseRemoteSubframes);

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
  Document* doc = mContentViewer->GetDocument();
  if (!doc) {
    return NS_OK;
  }

  const Encoding* parentCS = doc->GetDocumentCharacterSet();
  int32_t charsetSource = doc->GetDocumentCharacterSetSource();
  // set the child's parentCharset
  childAsDocShell->SetParentCharset(parentCS, charsetSource,
                                    doc->NodePrincipal());

  // printf("### 1 >>> Adding child. Parent CS = %s. ItemType = %d.\n",
  //        NS_LossyConvertUTF16toASCII(parentCS).get(), mItemType);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveChild(nsIDocShellTreeItem* aChild) {
  NS_ENSURE_ARG_POINTER(aChild);

  RefPtr<nsDocLoader> childAsDocLoader = GetAsDocLoader(aChild);
  NS_ENSURE_TRUE(childAsDocLoader, NS_ERROR_UNEXPECTED);

  nsresult rv = RemoveChildLoader(childAsDocLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  aChild->SetTreeOwner(nullptr);

  return nsDocLoader::AddDocLoaderAsChildOfRoot(childAsDocLoader);
}

NS_IMETHODIMP
nsDocShell::GetInProcessChildAt(int32_t aIndex, nsIDocShellTreeItem** aChild) {
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
nsDocShell::FindChildWithName(const nsAString& aName, bool aRecurse,
                              bool aSameType, nsIDocShellTreeItem* aRequestor,
                              nsIDocShellTreeItem* aOriginalRequestor,
                              nsIDocShellTreeItem** aResult) {
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
nsDocShell::GetChildSHEntry(int32_t aChildOffset, nsISHEntry** aResult) {
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
    bool parentExpired = mLSHE->GetExpirationStatus();

    /* Get the parent's Load Type so that it can be set on the child too.
     * By default give a loadHistory value
     */
    uint32_t loadType = mLSHE->GetLoadType();
    // If the user did a shift-reload on this frameset page,
    // we don't want to load the subframes from history.
    if (IsForceReloadType(loadType) || loadType == LOAD_REFRESH) {
      return rv;
    }

    /* If the user pressed reload and the parent frame has expired
     *  from cache, we do not want to load the child frame from history.
     */
    if (parentExpired && (loadType == LOAD_RELOAD_NORMAL)) {
      // The parent has expired. Return null.
      *aResult = nullptr;
      return rv;
    }

    // Get the child subframe from session history.
    rv = mLSHE->GetChildAt(aChildOffset, aResult);
    if (*aResult) {
      (*aResult)->SetLoadType(loadType);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsDocShell::AddChildSHEntry(nsISHEntry* aCloneRef, nsISHEntry* aNewEntry,
                            int32_t aChildOffset, uint32_t aLoadType,
                            bool aCloneChildren) {
  nsresult rv = NS_OK;

  if (mLSHE && aLoadType != LOAD_PUSHSTATE) {
    /* You get here if you are currently building a
     * hierarchy ie.,you just visited a frameset page
     */
    if (NS_FAILED(mLSHE->ReplaceChild(aNewEntry))) {
      rv = mLSHE->AddChild(aNewEntry, aChildOffset);
    }
  } else if (!aCloneRef) {
    /* This is an initial load in some subframe.  Just append it if we can */
    if (mOSHE) {
      rv = mOSHE->AddChild(aNewEntry, aChildOffset);
    }
  } else {
    rv = AddChildSHEntryInternal(aCloneRef, aNewEntry, aChildOffset, aLoadType,
                                 aCloneChildren);
  }
  return rv;
}

nsresult nsDocShell::AddChildSHEntryInternal(nsISHEntry* aCloneRef,
                                             nsISHEntry* aNewEntry,
                                             int32_t aChildOffset,
                                             uint32_t aLoadType,
                                             bool aCloneChildren) {
  nsresult rv = NS_OK;
  if (mSessionHistory) {
    /* You are currently in the rootDocShell.
     * You will get here when a subframe has a new url
     * to load and you have walked up the tree all the
     * way to the top to clone the current SHEntry hierarchy
     * and replace the subframe where a new url was loaded with
     * a new entry.
     */
    nsCOMPtr<nsISHEntry> currentHE;
    int32_t index = mSessionHistory->Index();
    if (index < 0) {
      return NS_ERROR_FAILURE;
    }

    rv = mSessionHistory->LegacySHistory()->GetEntryAtIndex(
        index, getter_AddRefs(currentHE));
    NS_ENSURE_TRUE(currentHE, NS_ERROR_FAILURE);

    nsCOMPtr<nsISHEntry> currentEntry(currentHE);
    if (currentEntry) {
      nsCOMPtr<nsISHEntry> nextEntry;
      uint32_t cloneID = aCloneRef->GetID();
      rv = nsSHistory::CloneAndReplace(currentEntry, this, cloneID, aNewEntry,
                                       aCloneChildren,
                                       getter_AddRefs(nextEntry));

      if (NS_SUCCEEDED(rv)) {
        rv = mSessionHistory->LegacySHistory()->AddEntry(nextEntry, true);
      }
    }
  } else {
    /* Just pass this along */
    nsCOMPtr<nsIDocShell> parent =
        do_QueryInterface(GetAsSupports(mParent), &rv);
    if (parent) {
      rv = static_cast<nsDocShell*>(parent.get())
               ->AddChildSHEntryInternal(aCloneRef, aNewEntry, aChildOffset,
                                         aLoadType, aCloneChildren);
    }
  }
  return rv;
}

nsresult nsDocShell::AddChildSHEntryToParent(nsISHEntry* aNewEntry,
                                             int32_t aChildOffset,
                                             bool aCloneChildren) {
  /* You will get here when you are in a subframe and
   * a new url has been loaded on you.
   * The mOSHE in this subframe will be the previous url's
   * mOSHE. This mOSHE will be used as the identification
   * for this subframe in the  CloneAndReplace function.
   */

  // In this case, we will end up calling AddEntry, which increases the
  // current index by 1
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (rootSH) {
    mPreviousEntryIndex = rootSH->Index();
  }

  nsresult rv;
  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(GetAsSupports(mParent), &rv);
  if (parent) {
    rv = parent->AddChildSHEntry(mOSHE, aNewEntry, aChildOffset, mLoadType,
                                 aCloneChildren);
  }

  if (rootSH) {
    mLoadedEntryIndex = rootSH->Index();

    if (MOZ_UNLIKELY(MOZ_LOG_TEST(gPageCacheLog, LogLevel::Verbose))) {
      MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
              ("Previous index: %d, Loaded index: %d", mPreviousEntryIndex,
               mLoadedEntryIndex));
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::SetUseGlobalHistory(bool aUseGlobalHistory) {
  mUseGlobalHistory = aUseGlobalHistory;
  if (!aUseGlobalHistory) {
    return NS_OK;
  }

  nsCOMPtr<IHistory> history = services::GetHistoryService();
  return history ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetUseGlobalHistory(bool* aUseGlobalHistory) {
  *aUseGlobalHistory = mUseGlobalHistory;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::RemoveFromSessionHistory() {
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  nsCOMPtr<nsIWebNavigation> rootAsWebnav = do_QueryInterface(root);
  if (!rootAsWebnav) {
    return NS_OK;
  }
  RefPtr<ChildSHistory> sessionHistory = rootAsWebnav->GetSessionHistory();
  if (!sessionHistory) {
    return NS_OK;
  }
  int32_t index = sessionHistory->Index();
  AutoTArray<nsID, 16> ids({mHistoryID});
  sessionHistory->LegacySHistory()->RemoveEntries(ids, index);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCreatedDynamically(bool aDynamic) {
  mDynamicallyCreated = aDynamic;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCreatedDynamically(bool* aDynamic) {
  *aDynamic = mDynamicallyCreated;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentSHEntry(nsISHEntry** aEntry, bool* aOSHE) {
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

nsIScriptGlobalObject* nsDocShell::GetScriptGlobalObject() {
  NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), nullptr);
  return mScriptGlobal;
}

Document* nsDocShell::GetDocument() {
  NS_ENSURE_SUCCESS(EnsureContentViewer(), nullptr);
  return mContentViewer->GetDocument();
}

nsPIDOMWindowOuter* nsDocShell::GetWindow() {
  if (NS_FAILED(EnsureScriptEnvironment())) {
    return nullptr;
  }
  return mScriptGlobal;
}

NS_IMETHODIMP
nsDocShell::GetDomWindow(mozIDOMWindowProxy** aWindow) {
  NS_ENSURE_ARG_POINTER(aWindow);

  nsresult rv = EnsureScriptEnvironment();
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsGlobalWindowOuter> window = mScriptGlobal;
  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMessageManager(ContentFrameMessageManager** aMessageManager) {
  RefPtr<ContentFrameMessageManager> mm;
  if (RefPtr<BrowserChild> browserChild = BrowserChild::GetFrom(this)) {
    mm = browserChild->GetMessageManager();
  } else if (nsPIDOMWindowOuter* win = GetWindow()) {
    mm = win->GetMessageManager();
  }
  mm.forget(aMessageManager);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetContentBlockingLog(Promise** aPromise) {
  NS_ENSURE_ARG_POINTER(aPromise);

  if (!mContentViewer) {
    *aPromise = nullptr;
    return NS_ERROR_FAILURE;
  }

  Document* doc = mContentViewer->GetDocument();
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(doc->GetOwnerGlobal(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  promise->MaybeResolve(
      NS_ConvertUTF8toUTF16(doc->GetContentBlockingLog()->Stringify()));
  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsNavigating(bool* aOut) {
  *aOut = mIsNavigating;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDeviceSizeIsPageSize(bool aValue) {
  if (mDeviceSizeIsPageSize != aValue) {
    mDeviceSizeIsPageSize = aValue;
    RefPtr<nsPresContext> presContext = GetPresContext();
    if (presContext) {
      presContext->MediaFeatureValuesChanged(
          {MediaFeatureChangeReason::DeviceSizeIsPageSizeChange});
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetDeviceSizeIsPageSize(bool* aValue) {
  *aValue = mDeviceSizeIsPageSize;
  return NS_OK;
}

void nsDocShell::ClearFrameHistory(nsISHEntry* aEntry) {
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (!rootSH || !aEntry) {
    return;
  }

  int32_t count = aEntry->GetChildCount();
  AutoTArray<nsID, 16> ids;
  for (int32_t i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    aEntry->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      ids.AppendElement(child->DocshellID());
    }
  }
  int32_t index = rootSH->Index();
  rootSH->LegacySHistory()->RemoveEntries(ids, index);
}

//-------------------------------------
//-- Helper Method for Print discovery
//-------------------------------------
bool nsDocShell::IsPrintingOrPP(bool aDisplayErrorDialog) {
  if (mIsPrintingOrPP && aDisplayErrorDialog) {
    DisplayLoadError(NS_ERROR_DOCUMENT_IS_PRINTMODE, nullptr, nullptr, nullptr);
  }

  return mIsPrintingOrPP;
}

bool nsDocShell::IsNavigationAllowed(bool aDisplayPrintErrorDialog,
                                     bool aCheckIfUnloadFired) {
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
nsDocShell::GetCanGoBack(bool* aCanGoBack) {
  *aCanGoBack = false;
  if (!IsNavigationAllowed(false)) {
    return NS_OK;  // JS may not handle returning of an error code
  }
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (rootSH) {
    *aCanGoBack = rootSH->CanGo(-1);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetCanGoForward(bool* aCanGoForward) {
  *aCanGoForward = false;
  if (!IsNavigationAllowed(false)) {
    return NS_OK;  // JS may not handle returning of an error code
  }
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (rootSH) {
    *aCanGoForward = rootSH->CanGo(1);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GoBack() {
  if (!IsNavigationAllowed()) {
    return NS_OK;  // JS may not handle returning of an error code
  }

  auto cleanupIsNavigating = MakeScopeExit([&]() { mIsNavigating = false; });
  mIsNavigating = true;

  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
  ErrorResult rv;
  rootSH->Go(-1, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDocShell::GoForward() {
  if (!IsNavigationAllowed()) {
    return NS_OK;  // JS may not handle returning of an error code
  }

  auto cleanupIsNavigating = MakeScopeExit([&]() { mIsNavigating = false; });
  mIsNavigating = true;

  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
  ErrorResult rv;
  rootSH->Go(1, rv);
  return rv.StealNSResult();
}

// XXX(nika): We may want to stop exposing this API in the child process? Going
// to a specific index from multiple different processes could definitely race.
NS_IMETHODIMP
nsDocShell::GotoIndex(int32_t aIndex) {
  if (!IsNavigationAllowed()) {
    return NS_OK;  // JS may not handle returning of an error code
  }

  auto cleanupIsNavigating = MakeScopeExit([&]() { mIsNavigating = false; });
  mIsNavigating = true;

  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  NS_ENSURE_TRUE(rootSH, NS_ERROR_FAILURE);
  return rootSH->LegacySHistory()->GotoIndex(aIndex);
}

nsresult nsDocShell::LoadURI(const nsAString& aURI,
                             const LoadURIOptions& aLoadURIOptions) {
  uint32_t loadFlags = aLoadURIOptions.mLoadFlags;

  NS_ASSERTION((loadFlags & INTERNAL_LOAD_FLAGS_LOADURI_SETUP_FLAGS) == 0,
               "Unexpected flags");

  if (!IsNavigationAllowed()) {
    return NS_OK;  // JS may not handle returning of an error code
  }

  auto cleanupIsNavigating = MakeScopeExit([&]() { mIsNavigating = false; });
  mIsNavigating = true;

  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIInputStream> postData(aLoadURIOptions.mPostData);
  nsresult rv = NS_OK;

  NS_ConvertUTF16toUTF8 uriString(aURI);
  // Cleanup the empty spaces that might be on each end.
  uriString.Trim(" ");
  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripCRLF();
  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

  if (mUseStrictSecurityChecks && !aLoadURIOptions.mTriggeringPrincipal) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  if (sURIFixup) {
    uint32_t fixupFlags;
    rv = sURIFixup->WebNavigationFlagsToFixupFlags(uriString, loadFlags,
                                                   &fixupFlags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    // If we don't allow keyword lookups for this URL string, make sure to
    // update loadFlags to indicate this as well.
    if (!(fixupFlags & nsIURIFixup::FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
      loadFlags &= ~LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
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
      postData = fixupStream;
    }

    if (loadFlags & LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
      nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
      if (serv) {
        serv->NotifyObservers(fixupInfo, "keyword-uri-fixup",
                              PromiseFlatString(aURI).get());
      }
    }
  } else {
    // No fixup service so just create a URI and see what happens...
    rv = NS_NewURI(getter_AddRefs(uri), uriString);
    loadFlags &= ~LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  if (NS_ERROR_MALFORMED_URI == rv) {
    if (DisplayLoadError(rv, uri, PromiseFlatString(aURI).get(), nullptr) &&
        (loadFlags & LOAD_FLAGS_ERROR_LOAD_CHANGES_RV) != 0) {
      return NS_ERROR_LOAD_SHOWED_ERRORPAGE;
    }
  }

  if (NS_FAILED(rv) || !uri) {
    return NS_ERROR_FAILURE;
  }

  PopupBlocker::PopupControlState popupState;
  if (loadFlags & LOAD_FLAGS_ALLOW_POPUPS) {
    popupState = PopupBlocker::openAllowed;
    loadFlags &= ~LOAD_FLAGS_ALLOW_POPUPS;
  } else {
    popupState = PopupBlocker::openOverridden;
  }
  AutoPopupStatePusher statePusher(popupState);

  bool forceAllowDataURI = loadFlags & LOAD_FLAGS_FORCE_ALLOW_DATA_URI;

  // Don't pass certain flags that aren't needed and end up confusing
  // ConvertLoadTypeToDocShellInfoLoadType.  We do need to ensure that they are
  // passed to LoadURI though, since it uses them.
  uint32_t extraFlags = (loadFlags & EXTRA_LOAD_FLAGS);
  loadFlags &= ~EXTRA_LOAD_FLAGS;

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(uri);
  loadState->SetReferrerInfo(aLoadURIOptions.mReferrerInfo);

  /*
   * If the user "Disables Protection on This Page", we have to make sure to
   * remember the users decision when opening links in child tabs [Bug 906190]
   */
  if (loadFlags & LOAD_FLAGS_ALLOW_MIXED_CONTENT) {
    loadState->SetLoadType(
        MAKE_LOAD_TYPE(LOAD_NORMAL_ALLOW_MIXED_CONTENT, loadFlags));
  } else {
    loadState->SetLoadType(MAKE_LOAD_TYPE(LOAD_NORMAL, loadFlags));
  }

  loadState->SetLoadFlags(extraFlags);
  loadState->SetFirstParty(true);
  loadState->SetPostDataStream(postData);
  loadState->SetHeadersStream(aLoadURIOptions.mHeaders);
  loadState->SetBaseURI(aLoadURIOptions.mBaseURI);
  loadState->SetTriggeringPrincipal(aLoadURIOptions.mTriggeringPrincipal);
  loadState->SetCsp(aLoadURIOptions.mCsp);
  loadState->SetForceAllowDataURI(forceAllowDataURI);

  if (fixupInfo) {
    nsAutoString searchProvider, keyword;
    fixupInfo->GetKeywordProviderName(searchProvider);
    fixupInfo->GetKeywordAsSent(keyword);
    MaybeNotifyKeywordSearchLoading(searchProvider, keyword);
  }

  rv = LoadURI(loadState);

  // Save URI string in case it's needed later when
  // sending to search engine service in EndPageLoad()
  mOriginalUriString = uriString;

  return rv;
}

NS_IMETHODIMP
nsDocShell::LoadURIFromScript(const nsAString& aURI,
                              JS::Handle<JS::Value> aLoadURIOptions,
                              JSContext* aCx) {
  // generate dictionary for aLoadURIOptions and forward call
  LoadURIOptions loadURIOptions;
  if (!loadURIOptions.Init(aCx, aLoadURIOptions)) {
    return NS_ERROR_INVALID_ARG;
  }
  return LoadURI(aURI, loadURIOptions);
}

NS_IMETHODIMP
nsDocShell::DisplayLoadError(nsresult aError, nsIURI* aURI,
                             const char16_t* aURL, nsIChannel* aFailedChannel,
                             bool* aDisplayedErrorPage) {
  *aDisplayedErrorPage = false;
  // Get prompt and string bundle services
  nsCOMPtr<nsIPrompt> prompter;
  nsCOMPtr<nsIStringBundle> stringBundle;
  GetPromptAndStringBundle(getter_AddRefs(prompter),
                           getter_AddRefs(stringBundle));

  NS_ENSURE_TRUE(stringBundle, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  const char* error = nullptr;
  // The key used to select the appropriate error message from the properties
  // file.
  const char* errorDescriptionID = nullptr;
  AutoTArray<nsString, 3> formatStrs;
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
    CopyASCIItoUTF16(scheme, *formatStrs.AppendElement());
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
    CopyUTF8toUTF16(host, *formatStrs.AppendElement());
    errorDescriptionID = "dnsNotFound2";
    error = "dnsNotFound";
  } else if (NS_ERROR_CONNECTION_REFUSED == aError ||
             NS_ERROR_PROXY_BAD_GATEWAY == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error = "connectionFailure";
  } else if (NS_ERROR_NET_INTERRUPT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    addHostPort = true;
    error = "netInterrupt";
  } else if (NS_ERROR_NET_TIMEOUT == aError ||
             NS_ERROR_PROXY_GATEWAY_TIMEOUT == aError) {
    NS_ENSURE_ARG_POINTER(aURI);
    // Get the host
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, *formatStrs.AppendElement());
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
      } else if (securityState &
                 nsIWebProgressListener::STATE_USES_WEAK_CRYPTO) {
        error = "weakCryptoUsed";
        addHostPort = true;
      }
    } else {
      // No channel, let's obtain the generic error message
      if (nsserr) {
        nsserr->GetErrorMessage(aError, messageStr);
      }
    }
    // We don't have a message string here anymore but DisplayLoadError
    // requires a non-empty messageStr.
    messageStr.Truncate();
    messageStr.AssignLiteral(u" ");
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
        rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, flags,
                              mOriginAttributes, nullptr, nullptr, &isStsHost);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HPKP, aURI, flags,
                              mOriginAttributes, nullptr, nullptr,
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

      if (Preferences::GetBool("browser.xul.error_pages.expert_bad_cert",
                               false)) {
        cssClass.AssignLiteral("expertBadCert");
      }

      // HSTS/pinning takes precedence over the expert bad cert pref. We
      // never want to show the "Add Exception" button for these sites.
      // In the future we should differentiate between an HSTS host and a
      // pinned host and display a more informative message to the user.
      if (isStsHost || isPinnedHost) {
        cssClass.AssignLiteral("badStsCert");
      }

      // See if an alternate cert error page is registered
      nsAutoCString alternateErrorPage;
      nsresult rv = Preferences::GetCString(
          "security.alternate_certificate_error_page", alternateErrorPage);
      if (NS_SUCCEEDED(rv)) {
        errorPage.Assign(alternateErrorPage);
      }
    } else {
      error = "nssFailure2";
    }
  } else if (NS_ERROR_PHISHING_URI == aError ||
             NS_ERROR_MALWARE_URI == aError ||
             NS_ERROR_UNWANTED_URI == aError ||
             NS_ERROR_HARMFUL_URI == aError) {
    nsAutoCString host;
    aURI->GetHost(host);
    CopyUTF8toUTF16(host, *formatStrs.AppendElement());

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
      bucketId = IsFrame()
                     ? IUrlClassifierUITelemetry::WARNING_PHISHING_PAGE_FRAME
                     : IUrlClassifierUITelemetry::WARNING_PHISHING_PAGE_TOP;
    } else if (NS_ERROR_MALWARE_URI == aError) {
      sendTelemetry = true;
      error = "malwareBlocked";
      bucketId = IsFrame()
                     ? IUrlClassifierUITelemetry::WARNING_MALWARE_PAGE_FRAME
                     : IUrlClassifierUITelemetry::WARNING_MALWARE_PAGE_TOP;
    } else if (NS_ERROR_UNWANTED_URI == aError) {
      sendTelemetry = true;
      error = "unwantedBlocked";
      bucketId = IsFrame()
                     ? IUrlClassifierUITelemetry::WARNING_UNWANTED_PAGE_FRAME
                     : IUrlClassifierUITelemetry::WARNING_UNWANTED_PAGE_TOP;
    } else if (NS_ERROR_HARMFUL_URI == aError) {
      sendTelemetry = true;
      error = "harmfulBlocked";
      bucketId = IsFrame()
                     ? IUrlClassifierUITelemetry::WARNING_HARMFUL_PAGE_FRAME
                     : IUrlClassifierUITelemetry::WARNING_HARMFUL_PAGE_TOP;
    }

    if (sendTelemetry && errorPage.EqualsIgnoreCase("blocked")) {
      Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UI_EVENTS, bucketId);
    }

    cssClass.AssignLiteral("blacklist");
  } else if (NS_ERROR_CONTENT_CRASHED == aError) {
    errorPage.AssignLiteral("tabcrashed");
    error = "tabcrashed";

    RefPtr<EventTarget> handler = mChromeEventHandler;
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
  } else if (NS_ERROR_FRAME_CRASHED == aError) {
    errorPage.AssignLiteral("framecrashed");
    error = "framecrashed";
    messageStr.AssignLiteral(u" ");
  } else if (NS_ERROR_BUILDID_MISMATCH == aError) {
    errorPage.AssignLiteral("restartrequired");
    error = "restartrequired";

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
      case NS_ERROR_PROXY_AUTHENTICATION_FAILED:
      case NS_ERROR_TOO_MANY_REQUESTS:
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
      case NS_ERROR_BLOCKED_BY_POLICY:
        // Page blocked by policy
        error = "blockedByPolicy";
        break;
      case NS_ERROR_NET_HTTP2_SENT_GOAWAY:
        // HTTP/2 stack detected a protocol error
        error = "networkProtocolError";
        break;
      default:
        break;
    }
  }

  if (mLoadURIDelegate) {
    nsCOMPtr<nsIURI> errorPageURI;
    rv = mLoadURIDelegate->HandleLoadError(aURI, aError,
                                           NS_ERROR_GET_MODULE(aError),
                                           getter_AddRefs(errorPageURI));
    if (NS_FAILED(rv)) {
      *aDisplayedErrorPage = false;
      return NS_OK;
    }

    if (errorPageURI) {
      *aDisplayedErrorPage =
          NS_SUCCEEDED(LoadErrorPage(errorPageURI, aURI, aFailedChannel));
      return NS_OK;
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
      CopyUTF8toUTF16(hostport, *formatStrs.AppendElement());
    }

    nsAutoCString spec;
    rv = NS_ERROR_NOT_AVAILABLE;
    auto& nextFormatStr = *formatStrs.AppendElement();
    if (aURI) {
      // displaying "file://" is aesthetically unpleasing and could even be
      // confusing to the user
      if (SchemeIsFile(aURI)) {
        aURI->GetPathQueryRef(spec);
      } else {
        aURI->GetSpec(spec);
      }

      nsCOMPtr<nsITextToSubURI> textToSubURI(
          do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) {
        rv = textToSubURI->UnEscapeURIForUI(NS_LITERAL_CSTRING("UTF-8"), spec,
                                            nextFormatStr);
      }
    } else {
      spec.Assign('?');
    }
    if (NS_FAILED(rv)) {
      CopyUTF8toUTF16(spec, nextFormatStr);
    }
    rv = NS_OK;

    nsAutoString str;
    rv =
        stringBundle->FormatStringFromName(errorDescriptionID, formatStrs, str);
    NS_ENSURE_SUCCESS(rv, rv);
    messageStr.Assign(str);
  }

  // Display the error as a page or an alert prompt
  NS_ENSURE_FALSE(messageStr.IsEmpty(), NS_ERROR_FAILURE);

  if ((NS_ERROR_NET_INTERRUPT == aError || NS_ERROR_NET_RESET == aError) &&
      SchemeIsHTTPS(aURI)) {
    // Maybe TLS intolerant. Treat this as an SSL error.
    error = "nssFailure2";
  }

  if (UseErrorPages()) {
    // Display an error page
    nsresult loadedPage =
        LoadErrorPage(aURI, aURL, errorPage.get(), error, messageStr.get(),
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

nsresult nsDocShell::LoadErrorPage(nsIURI* aURI, const char16_t* aURL,
                                   const char* aErrorPage,
                                   const char* aErrorType,
                                   const char16_t* aDescription,
                                   const char* aCSSClass,
                                   nsIChannel* aFailedChannel) {
  MOZ_ASSERT(!mIsBeingDestroyed);

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aFailedChannel) {
      aFailedChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]::LoadErrorPage(\"%s\", \"%s\", {...}, [%s])\n",
             this, aURI ? aURI->GetSpecOrDefault().get() : "",
             NS_ConvertUTF16toUTF8(aURL).get(), chanName.get()));
  }
#endif

  nsAutoCString url;
  if (aURI) {
    nsresult rv = aURI->GetSpec(url);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aURL) {
    CopyUTF16toUTF8(MakeStringSpan(aURL), url);
  } else {
    return NS_ERROR_INVALID_POINTER;
  }

  // Create a URL to pass all the error information through to the page.

#undef SAFE_ESCAPE
#define SAFE_ESCAPE(output, input, params)             \
  if (NS_WARN_IF(!NS_Escape(input, output, params))) { \
    return NS_ERROR_OUT_OF_MEMORY;                     \
  }

  nsCString escapedUrl, escapedError, escapedDescription, escapedCSSClass;
  SAFE_ESCAPE(escapedUrl, url, url_Path);
  SAFE_ESCAPE(escapedError, nsDependentCString(aErrorType), url_Path);
  SAFE_ESCAPE(escapedDescription, NS_ConvertUTF16toUTF8(aDescription),
              url_Path);
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

  return LoadErrorPage(errorPageURI, aURI, aFailedChannel);
}

nsresult nsDocShell::LoadErrorPage(nsIURI* aErrorURI, nsIURI* aFailedURI,
                                   nsIChannel* aFailedChannel) {
  mFailedChannel = aFailedChannel;
  mFailedURI = aFailedURI;
  mFailedLoadType = mLoadType;

  if (mLSHE) {
    // Abandon mLSHE's BFCache entry and create a new one.  This way, if
    // we go back or forward to another SHEntry with the same doc
    // identifier, the error page won't persist.
    mLSHE->AbandonBFCacheEntry();
  }

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(aErrorURI);
  loadState->SetTriggeringPrincipal(nsContentUtils::GetSystemPrincipal());
  loadState->SetLoadType(LOAD_ERROR_PAGE);
  loadState->SetFirstParty(true);
  loadState->SetSourceDocShell(this);

  return InternalLoad(loadState, nullptr, nullptr);
}

NS_IMETHODIMP
nsDocShell::Reload(uint32_t aReloadFlags) {
  if (!IsNavigationAllowed()) {
    return NS_OK;  // JS may not handle returning of an error code
  }
  nsresult rv;
  NS_ASSERTION(((aReloadFlags & INTERNAL_LOAD_FLAGS_LOADURI_SETUP_FLAGS) == 0),
               "Reload command not updated to use load flags!");
  NS_ASSERTION((aReloadFlags & EXTRA_LOAD_FLAGS) == 0,
               "Don't pass these flags to Reload");

  uint32_t loadType = MAKE_LOAD_TYPE(LOAD_RELOAD_NORMAL, aReloadFlags);
  NS_ENSURE_TRUE(IsValidLoadType(loadType), NS_ERROR_INVALID_ARG);

  // Send notifications to the HistoryListener if any, about the impending
  // reload
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  bool canReload = true;
  if (rootSH) {
    rootSH->LegacySHistory()->NotifyOnHistoryReload(&canReload);
  }

  if (!canReload) {
    return NS_OK;
  }

  /* If you change this part of code, make sure bug 45297 does not re-occur */
  if (mOSHE) {
    rv = LoadHistoryEntry(mOSHE, loadType);
  } else if (mLSHE) {  // In case a reload happened before the current load is
                       // done
    rv = LoadHistoryEntry(mLSHE, loadType);
  } else {
    RefPtr<Document> doc(GetDocument());

    if (!doc) {
      return NS_OK;
    }

    // Do not inherit owner from document
    uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;
    nsAutoString srcdoc;
    nsIURI* baseURI = nullptr;
    nsCOMPtr<nsIURI> originalURI;
    nsCOMPtr<nsIURI> resultPrincipalURI;
    bool loadReplace = false;

    nsIPrincipal* triggeringPrincipal = doc->NodePrincipal();
    nsCOMPtr<nsIContentSecurityPolicy> csp = doc->GetCsp();

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

      nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();
      loadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
    }

    MOZ_ASSERT(triggeringPrincipal, "Need a valid triggeringPrincipal");
    if (mUseStrictSecurityChecks && !triggeringPrincipal) {
      return NS_ERROR_FAILURE;
    }

    // Stack variables to ensure changes to the member variables don't affect to
    // the call.
    nsCOMPtr<nsIURI> currentURI = mCurrentURI;

    // Reload always rewrites result principal URI.
    Maybe<nsCOMPtr<nsIURI>> emplacedResultPrincipalURI;
    emplacedResultPrincipalURI.emplace(std::move(resultPrincipalURI));

    RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(currentURI);
    loadState->SetReferrerInfo(mReferrerInfo);
    loadState->SetOriginalURI(originalURI);
    loadState->SetMaybeResultPrincipalURI(emplacedResultPrincipalURI);
    loadState->SetLoadReplace(loadReplace);
    loadState->SetTriggeringPrincipal(triggeringPrincipal);
    loadState->SetPrincipalToInherit(triggeringPrincipal);
    loadState->SetCsp(csp);
    loadState->SetLoadFlags(flags);
    loadState->SetTypeHint(NS_ConvertUTF16toUTF8(contentTypeHint));
    loadState->SetLoadType(loadType);
    loadState->SetFirstParty(true);
    loadState->SetSrcdocData(srcdoc);
    loadState->SetSourceDocShell(this);
    loadState->SetBaseURI(baseURI);
    rv = InternalLoad(loadState, nullptr, nullptr);
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::Stop(uint32_t aStopFlags) {
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
nsDocShell::GetDocument(Document** aDocument) {
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_SUCCESS(EnsureContentViewer(), NS_ERROR_FAILURE);

  RefPtr<Document> doc = mContentViewer->GetDocument();
  if (!doc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  doc.forget(aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentURI(nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> uri = mCurrentURI;
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::InitSessionHistory() {
  MOZ_ASSERT(!mIsBeingDestroyed);

  // Make sure that we are the root DocShell, and set a handle to root docshell
  // in the session history.
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  if (root != this) {
    return NS_ERROR_FAILURE;
  }

  mSessionHistory = new ChildSHistory(this);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSessionHistoryXPCOM(nsISupports** aSessionHistory) {
  NS_ENSURE_ARG_POINTER(aSessionHistory);
  RefPtr<ChildSHistory> shistory = mSessionHistory;
  shistory.forget(aSessionHistory);
  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIWebPageDescriptor
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::LoadPage(nsISupports* aPageDescriptor, uint32_t aDisplayType) {
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
    nsCString spec, newSpec;

    // Create a new view-source URI and replace the original.
    nsCOMPtr<nsIURI> oldUri = shEntry->GetURI();

    oldUri->GetSpec(spec);
    newSpec.AppendLiteral("view-source:");
    newSpec.Append(spec);

    nsCOMPtr<nsIURI> newUri;
    rv = NS_NewURI(getter_AddRefs(newUri), newSpec);
    if (NS_FAILED(rv)) {
      return rv;
    }
    shEntry->SetURI(newUri);
    shEntry->SetOriginalURI(nullptr);
    shEntry->SetResultPrincipalURI(nullptr);
    // shEntry's current triggering principal is whoever loaded that page
    // initially. But now we're doing another load of the page, via an API that
    // is only exposed to system code.  The triggering principal for this load
    // should be the system principal.
    shEntry->SetTriggeringPrincipal(nsContentUtils::GetSystemPrincipal());
  }

  rv = LoadHistoryEntry(shEntry, LOAD_HISTORY);
  return rv;
}

NS_IMETHODIMP
nsDocShell::GetCurrentDescriptor(nsISupports** aPageDescriptor) {
  MOZ_ASSERT(aPageDescriptor, "Null out param?");

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
                       int32_t aWidth, int32_t aHeight) {
  SetParentWidget(aParentWidget);
  SetPositionAndSize(aX, aY, aWidth, aHeight, 0);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Create() {
  if (mCreated) {
    // We've already been created
    return NS_OK;
  }

  NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
               "Unexpected item type in docshell");

  NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);
  mCreated = true;

  mUseStrictSecurityChecks = Preferences::GetBool(
      "security.strict_security_checks.enabled", mUseStrictSecurityChecks);

  // Should we use XUL error pages instead of alerts if possible?
  mUseErrorPages = StaticPrefs::browser_xul_error_pages_enabled();

  mDisableMetaRefreshWhenInactive =
      Preferences::GetBool("browser.meta_refresh_when_inactive.disabled",
                           mDisableMetaRefreshWhenInactive);

  mDeviceSizeIsPageSize = Preferences::GetBool(
      "docshell.device_size_is_page_size", mDeviceSizeIsPageSize);

  nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
  if (serv) {
    const char* msg = mItemType == typeContent ? NS_WEBNAVIGATION_CREATE
                                               : NS_CHROME_WEBNAVIGATION_CREATE;
    serv->NotifyObservers(GetAsSupports(this), msg, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::Destroy() {
  // XXX: We allow this function to be called just once.  If you are going to
  // reset new variables in this function, please make sure the variables will
  // never be re-initialized.  Adding assertions to check |mIsBeingDestroyed|
  // in the setter functions for the variables would be enough.
  if (mIsBeingDestroyed) {
    return NS_ERROR_DOCSHELL_DYING;
  }

  NS_ASSERTION(mItemType == typeContent || mItemType == typeChrome,
               "Unexpected item type in docshell");

  AssertOriginAttributesMatchPrivateBrowsing();

  nsCOMPtr<nsIObserverService> serv = services::GetObserverService();
  if (serv) {
    const char* msg = mItemType == typeContent
                          ? NS_WEBNAVIGATION_DESTROY
                          : NS_CHROME_WEBNAVIGATION_DESTROY;
    serv->NotifyObservers(GetAsSupports(this), msg, nullptr);
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
    mSessionHistory->EvictLocalContentViewers();
    mSessionHistory = nullptr;
  }

  // Either `Detach` our BrowsingContext if this window is closing, or prepare
  // the BrowsingContext for the switch to continue.
  if (mSkipBrowsingContextDetachOnDestroy) {
    mBrowsingContext->PrepareForProcessChange();
  } else {
    mBrowsingContext->Detach();
  }

  SetTreeOwner(nullptr);

  mBrowserChild = nullptr;

  mChromeEventHandler = nullptr;

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
nsDocShell::GetUnscaledDevicePixelsPerCSSPixel(double* aScale) {
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
nsDocShell::GetDevicePixelsPerDesktopPixel(double* aScale) {
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
nsDocShell::SetPosition(int32_t aX, int32_t aY) {
  mBounds.MoveTo(aX, aY);

  if (mContentViewer) {
    NS_ENSURE_SUCCESS(mContentViewer->Move(aX, aY), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetPositionDesktopPix(int32_t aX, int32_t aY) {
  nsCOMPtr<nsIBaseWindow> ownerWindow(do_QueryInterface(mTreeOwner));
  if (ownerWindow) {
    return ownerWindow->SetPositionDesktopPix(aX, aY);
  }

  double scale = 1.0;
  GetDevicePixelsPerDesktopPixel(&scale);
  return SetPosition(NSToIntRound(aX * scale), NSToIntRound(aY * scale));
}

NS_IMETHODIMP
nsDocShell::GetPosition(int32_t* aX, int32_t* aY) {
  return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP
nsDocShell::SetSize(int32_t aWidth, int32_t aHeight, bool aRepaint) {
  int32_t x = 0, y = 0;
  GetPosition(&x, &y);
  return SetPositionAndSize(x, y, aWidth, aHeight,
                            aRepaint ? nsIBaseWindow::eRepaint : 0);
}

NS_IMETHODIMP
nsDocShell::GetSize(int32_t* aWidth, int32_t* aHeight) {
  return GetPositionAndSize(nullptr, nullptr, aWidth, aHeight);
}

NS_IMETHODIMP
nsDocShell::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aWidth,
                               int32_t aHeight, uint32_t aFlags) {
  mBounds.SetRect(aX, aY, aWidth, aHeight);

  // Hold strong ref, since SetBounds can make us null out mContentViewer
  nsCOMPtr<nsIContentViewer> viewer = mContentViewer;
  if (viewer) {
    uint32_t cvflags = (aFlags & nsIBaseWindow::eDelayResize)
                           ? nsIContentViewer::eDelayResize
                           : 0;
    // XXX Border figured in here or is that handled elsewhere?
    nsresult rv = viewer->SetBoundsWithFlags(mBounds, cvflags);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                               int32_t* aHeight) {
  if (mParentWidget) {
    // ensure size is up-to-date if window has changed resolution
    LayoutDeviceIntRect r = mParentWidget->GetClientBounds();
    SetPositionAndSize(mBounds.X(), mBounds.Y(), r.Width(), r.Height(), 0);
  }

  // We should really consider just getting this information from
  // our window instead of duplicating the storage and code...
  if (aWidth || aHeight) {
    // Caller wants to know our size; make sure to give them up to
    // date information.
    RefPtr<Document> doc(do_GetInterface(GetAsSupports(mParent)));
    if (doc) {
      doc->FlushPendingNotifications(FlushType::Layout);
    }
  }

  DoGetPositionAndSize(aX, aY, aWidth, aHeight);
  return NS_OK;
}

void nsDocShell::DoGetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aWidth,
                                      int32_t* aHeight) {
  if (aX) {
    *aX = mBounds.X();
  }
  if (aY) {
    *aY = mBounds.Y();
  }
  if (aWidth) {
    *aWidth = mBounds.Width();
  }
  if (aHeight) {
    *aHeight = mBounds.Height();
  }
}

NS_IMETHODIMP
nsDocShell::Repaint(bool aForce) {
  PresShell* presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  RefPtr<nsViewManager> viewManager = presShell->GetViewManager();
  NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

  viewManager->InvalidateAllViews();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentWidget(nsIWidget** aParentWidget) {
  NS_ENSURE_ARG_POINTER(aParentWidget);

  *aParentWidget = mParentWidget;
  NS_IF_ADDREF(*aParentWidget);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentWidget(nsIWidget* aParentWidget) {
  MOZ_ASSERT(!mIsBeingDestroyed);
  mParentWidget = aParentWidget;

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetParentNativeWindow(nativeWindow* aParentNativeWindow) {
  NS_ENSURE_ARG_POINTER(aParentNativeWindow);

  if (mParentWidget) {
    *aParentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    *aParentNativeWindow = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetParentNativeWindow(nativeWindow aParentNativeWindow) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetNativeHandle(nsAString& aNativeHandle) {
  // the nativeHandle should be accessed from nsIXULWindow
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::GetVisibility(bool* aVisibility) {
  NS_ENSURE_ARG_POINTER(aVisibility);

  *aVisibility = false;

  if (!mContentViewer) {
    return NS_OK;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_OK;
  }

  // get the view manager
  nsViewManager* vm = presShell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  // get the root view
  nsView* view = vm->GetRootView();  // views are not ref counted
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  // if our root view is hidden, we are not visible
  if (view->GetVisibility() == nsViewVisibility_kHide) {
    return NS_OK;
  }

  // otherwise, we must walk up the document and view trees checking
  // for a hidden view, unless we're an off screen browser, which
  // would make this test meaningless.

  RefPtr<nsDocShell> docShell = this;
  RefPtr<nsDocShell> parentItem = docShell->GetInProcessParentDocshell();
  while (parentItem) {
    // Null-check for crash in bug 267804
    if (!parentItem->GetPresShell()) {
      MOZ_ASSERT_UNREACHABLE("parent docshell has null pres shell");
      return NS_OK;
    }

    vm = docShell->GetPresShell()->GetViewManager();
    if (vm) {
      view = vm->GetRootView();
    }

    if (view) {
      view = view->GetParent();  // anonymous inner view
      if (view) {
        view = view->GetParent();  // subdocumentframe's view
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
    parentItem = docShell->GetInProcessParentDocshell();
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
nsDocShell::SetIsOffScreenBrowser(bool aIsOffScreen) {
  mIsOffScreenBrowser = aIsOffScreen;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsOffScreenBrowser(bool* aIsOffScreen) {
  *aIsOffScreen = mIsOffScreenBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsActive(bool aIsActive) {
  // Keep track ourselves.
  mIsActive = aIsActive;

  // Tell the PresShell about it.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->SetIsActive(aIsActive);
  }

  // Tell the window about it
  if (mScriptGlobal) {
    mScriptGlobal->SetIsBackground(!aIsActive);
    if (RefPtr<Document> doc = mScriptGlobal->GetExtantDoc()) {
      // Update orientation when the top-level browsing context becomes active.
      if (aIsActive) {
        nsCOMPtr<nsIDocShellTreeItem> parent;
        GetInProcessSameTypeParent(getter_AddRefs(parent));
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
    Document* doc = mContentViewer->GetDocument();
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
nsDocShell::GetIsActive(bool* aIsActive) {
  *aIsActive = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetIsAppTab(bool aIsAppTab) {
  mIsAppTab = aIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsAppTab(bool* aIsAppTab) {
  *aIsAppTab = mIsAppTab;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetSandboxFlags(uint32_t aSandboxFlags) {
  mSandboxFlags = aSandboxFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetSandboxFlags(uint32_t* aSandboxFlags) {
  *aSandboxFlags = mSandboxFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDefaultLoadFlags(uint32_t aDefaultLoadFlags) {
  mDefaultLoadFlags = aDefaultLoadFlags;

  // Tell the load group to set these flags all requests in the group
  if (mLoadGroup) {
    mLoadGroup->SetDefaultLoadFlags(aDefaultLoadFlags);
  } else {
    NS_WARNING(
        "nsDocShell::SetDefaultLoadFlags has no loadGroup to propagate the "
        "flags to");
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
nsDocShell::GetDefaultLoadFlags(uint32_t* aDefaultLoadFlags) {
  *aDefaultLoadFlags = mDefaultLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetMixedContentChannel(nsIChannel* aMixedContentChannel) {
#ifdef DEBUG
  // if the channel is non-null
  if (aMixedContentChannel) {
    // Get the root docshell.
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
    NS_WARNING_ASSERTION(root.get() == static_cast<nsIDocShellTreeItem*>(this),
                         "Setting mMixedContentChannel on a docshell that is "
                         "not the root docshell");
  }
#endif
  mMixedContentChannel = aMixedContentChannel;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetFailedChannel(nsIChannel** aFailedChannel) {
  NS_ENSURE_ARG_POINTER(aFailedChannel);
  Document* doc = GetDocument();
  if (!doc) {
    *aFailedChannel = nullptr;
    return NS_OK;
  }
  NS_IF_ADDREF(*aFailedChannel = doc->GetFailedChannel());
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetMixedContentChannel(nsIChannel** aMixedContentChannel) {
  NS_ENSURE_ARG_POINTER(aMixedContentChannel);
  NS_IF_ADDREF(*aMixedContentChannel = mMixedContentChannel);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetAllowMixedContentAndConnectionData(
    bool* aRootHasSecureConnection, bool* aAllowMixedContent,
    bool* aIsRootDocShell) {
  *aRootHasSecureConnection = true;
  *aAllowMixedContent = false;
  *aIsRootDocShell = false;

  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  GetInProcessSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
  NS_ASSERTION(
      sameTypeRoot,
      "No document shell root tree item from document shell tree item!");
  *aIsRootDocShell =
      sameTypeRoot.get() == static_cast<nsIDocShellTreeItem*>(this);

  // now get the document from sameTypeRoot
  RefPtr<Document> rootDoc = sameTypeRoot->GetDocument();
  if (rootDoc) {
    nsCOMPtr<nsIPrincipal> rootPrincipal = rootDoc->NodePrincipal();

    // For things with system principal (e.g. scratchpad) there is no uri
    // aRootHasSecureConnection should be false.
    nsCOMPtr<nsIURI> rootUri = rootPrincipal->GetURI();
    if (nsContentUtils::IsSystemPrincipal(rootPrincipal) || !rootUri ||
        !SchemeIsHTTPS(rootUri)) {
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
nsDocShell::SetVisibility(bool aVisibility) {
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
nsDocShell::GetEnabled(bool* aEnabled) {
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = true;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocShell::SetEnabled(bool aEnabled) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsDocShell::SetFocus() { return NS_OK; }

NS_IMETHODIMP
nsDocShell::GetMainWidget(nsIWidget** aMainWidget) {
  // We don't create our own widget, so simply return the parent one.
  return GetParentWidget(aMainWidget);
}

NS_IMETHODIMP
nsDocShell::GetTitle(nsAString& aTitle) {
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetTitle(const nsAString& aTitle) {
  // Avoid unnecessary updates of the title if the URI and the title haven't
  // changed.
  if (mTitleValidForCurrentURI && mTitle == aTitle) {
    return NS_OK;
  }

  // Store local title
  mTitle = aTitle;
  mTitleValidForCurrentURI = true;

  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetInProcessSameTypeParent(getter_AddRefs(parent));

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

nsPoint nsDocShell::GetCurScrollPos() {
  nsPoint scrollPos;
  if (nsIScrollableFrame* sf = GetRootScrollFrame()) {
    scrollPos = sf->GetVisualViewportOffset();
  }
  return scrollPos;
}

nsresult nsDocShell::SetCurScrollPosEx(int32_t aCurHorizontalPos,
                                       int32_t aCurVerticalPos) {
  nsIScrollableFrame* sf = GetRootScrollFrame();
  NS_ENSURE_TRUE(sf, NS_ERROR_FAILURE);

  ScrollMode scrollMode =
      sf->IsSmoothScroll() ? ScrollMode::SmoothMsd : ScrollMode::Instant;

  nsPoint targetPos(aCurHorizontalPos, aCurVerticalPos);
  sf->ScrollTo(targetPos, scrollMode);

  // Set the visual viewport offset as well.

  RefPtr<PresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  // Only the root content document can have a distinct visual viewport offset.
  if (!presContext->IsRootContentDocument()) {
    return NS_OK;
  }

  // Not on a platform with a distinct visual viewport - don't bother setting
  // the visual viewport offset.
  if (!presShell->IsVisualViewportSizeSet()) {
    return NS_OK;
  }

  presShell->ScrollToVisual(targetPos, layers::FrameMetrics::eMainThread,
                            scrollMode);

  return NS_OK;
}

//*****************************************************************************
// nsDocShell::nsIScrollable
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetDefaultScrollbarPreferences(int32_t aScrollOrientation,
                                           int32_t* aScrollbarPref) {
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
                                           int32_t aScrollbarPref) {
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
                                   bool* aHorizontalVisible) {
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
// nsDocShell::nsIRefreshURI
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::RefreshURI(nsIURI* aURI, nsIPrincipal* aPrincipal, int32_t aDelay,
                       bool aRepeat, bool aMetaRefresh) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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

  BusyFlags busyFlags = GetBusyFlags();

  if (!mRefreshURIList) {
    mRefreshURIList = nsArray::Create();
  }

  if (busyFlags & BUSY_FLAGS_BUSY ||
      (!mIsActive && mDisableMetaRefreshWhenInactive)) {
    // We don't  want to create the timer right now. Instead queue up the
    // request and trigger the timer in EndPageLoad() or whenever we become
    // active.
    mRefreshURIList->AppendElement(refreshTimer);
  } else {
    // There is no page loading going on right now.  Create the
    // timer and fire it right away.
    nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
    NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

    nsCOMPtr<nsITimer> timer;
    MOZ_TRY_VAR(timer,
                NS_NewTimerWithCallback(
                    refreshTimer, aDelay, nsITimer::TYPE_ONE_SHOT,
                    win->TabGroup()->EventTargetFor(TaskCategory::Network)));

    mRefreshURIList->AppendElement(timer);  // owning timer ref
  }
  return NS_OK;
}

nsresult nsDocShell::ForceRefreshURIFromTimer(nsIURI* aURI,
                                              nsIPrincipal* aPrincipal,
                                              int32_t aDelay, bool aMetaRefresh,
                                              nsITimer* aTimer) {
  MOZ_ASSERT(aTimer, "Must have a timer here");

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
nsDocShell::ForceRefreshURI(nsIURI* aURI, nsIPrincipal* aPrincipal,
                            int32_t aDelay, bool aMetaRefresh) {
  NS_ENSURE_ARG(aURI);

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(aURI);
  loadState->SetOriginalURI(mCurrentURI);
  loadState->SetResultPrincipalURI(aURI);
  loadState->SetResultPrincipalURIIsSome(true);
  loadState->SetKeepResultPrincipalURIIfSet(true);

  // Set the triggering pricipal to aPrincipal if available, or current
  // document's principal otherwise.
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  RefPtr<Document> doc = GetDocument();
  if (!principal) {
    if (!doc) {
      return NS_ERROR_FAILURE;
    }
    principal = doc->NodePrincipal();
  }
  loadState->SetTriggeringPrincipal(principal);
  if (doc) {
    loadState->SetCsp(doc->GetCsp());
  }

  loadState->SetPrincipalIsExplicit(true);

  /* Check if this META refresh causes a redirection
   * to another site.
   */
  bool equalUri = false;
  nsresult rv = aURI->Equals(mCurrentURI, &equalUri);

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (NS_SUCCEEDED(rv) && (!equalUri) && aMetaRefresh &&
      aDelay <= REFRESH_REDIRECT_TIMER) {
    /* It is a META refresh based redirection within the threshold time
     * we have in mind (15000 ms as defined by REFRESH_REDIRECT_TIMER).
     * Pass a REPLACE flag to LoadURI().
     */
    loadState->SetLoadType(LOAD_NORMAL_REPLACE);

    /* For redirects we mimic HTTP, which passes the
     * original referrer.
     * We will pass in referrer but will not send to server
     */
    if (mReferrerInfo) {
      referrerInfo = static_cast<ReferrerInfo*>(mReferrerInfo.get())
                         ->CloneWithNewSendReferrer(false);
    }
  } else {
    loadState->SetLoadType(LOAD_REFRESH);
    /* We do need to pass in a referrer, but we don't want it to
     * be sent to the server.
     * For most refreshes the current URI is an appropriate
     * internal referrer.
     */
    referrerInfo = new ReferrerInfo(mCurrentURI, mozilla::net::RP_Unset, false);
  }

  loadState->SetReferrerInfo(referrerInfo);
  loadState->SetLoadFlags(
      nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL);
  loadState->SetFirstParty(true);

  /*
   * LoadURI(...) will cancel all refresh timers... This causes the
   * Timer and its refreshData instance to be released...
   */
  LoadURI(loadState);

  return NS_OK;
}

nsresult nsDocShell::SetupRefreshURIFromHeader(nsIURI* aBaseURI,
                                               nsIPrincipal* aPrincipal,
                                               const nsACString& aHeader) {
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
        if (iter == iterAfterDigit && !nsCRT::IsAsciiSpace(*iter) &&
            *iter != '.') {
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
nsDocShell::SetupRefreshURI(nsIChannel* aChannel) {
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

      SetupReferrerInfoFromChannel(aChannel);
      rv = SetupRefreshURIFromHeader(mCurrentURI, principal, refreshHeader);
      if (NS_SUCCEEDED(rv)) {
        return NS_REFRESHURI_HEADER_FOUND;
      }
    }
  }
  return rv;
}

static void DoCancelRefreshURITimers(nsIMutableArray* aTimerList) {
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
nsDocShell::CancelRefreshURITimers() {
  DoCancelRefreshURITimers(mRefreshURIList);
  DoCancelRefreshURITimers(mSavedRefreshURIList);
  mRefreshURIList = nullptr;
  mSavedRefreshURIList = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetRefreshPending(bool* aResult) {
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
nsDocShell::SuspendRefreshURIs() {
  if (mRefreshURIList) {
    uint32_t n = 0;
    mRefreshURIList->GetLength(&n);

    for (uint32_t i = 0; i < n; ++i) {
      nsCOMPtr<nsITimer> timer = do_QueryElementAt(mRefreshURIList, i);
      if (!timer) {
        continue;  // this must be a nsRefreshURI already
      }

      // Replace this timer object with a nsRefreshTimer object.
      nsCOMPtr<nsITimerCallback> callback;
      timer->GetCallback(getter_AddRefs(callback));

      timer->Cancel();

      mRefreshURIList->ReplaceElementAt(callback, i);
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
nsDocShell::ResumeRefreshURIs() {
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

nsresult nsDocShell::RefreshURIFromQueue() {
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
      uint32_t delay = static_cast<nsRefreshTimer*>(
                           static_cast<nsITimerCallback*>(refreshInfo))
                           ->GetDelay();
      nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
      if (win) {
        nsCOMPtr<nsITimer> timer;
        NS_NewTimerWithCallback(
            getter_AddRefs(timer), refreshInfo, delay, nsITimer::TYPE_ONE_SHOT,
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

nsresult nsDocShell::Embed(nsIContentViewer* aContentViewer,
                           WindowGlobalChild* aWindowActor) {
  // Save the LayoutHistoryState of the previous document, before
  // setting up new document
  PersistLayoutHistoryState();

  nsresult rv = SetupNewViewer(aContentViewer, aWindowActor);
  NS_ENSURE_SUCCESS(rv, rv);

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

//*****************************************************************************
// nsDocShell::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::OnProgressChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                             int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                             int32_t aCurTotalProgress,
                             int32_t aMaxTotalProgress) {
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnStateChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                          uint32_t aStateFlags, nsresult aStatus) {
  if ((~aStateFlags & (STATE_START | STATE_IS_NETWORK)) == 0) {
    // Save timing statistics.
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    nsAutoCString aURI;
    uri->GetAsciiSpec(aURI);

    if (this == aProgress) {
      mozilla::Unused << MaybeInitTiming();
      mTiming->NotifyFetchStart(uri,
                                ConvertLoadTypeToNavigationType(mLoadType));
    }

    // Page has begun to load
    mBusyFlags = (BusyFlags)(BUSY_FLAGS_BUSY | BUSY_FLAGS_BEFORE_PAGE_LOAD);

    if ((aStateFlags & STATE_RESTORING) == 0) {
      // Show the progress cursor if the pref is set
      if (StaticPrefs::ui_use_activity_cursor()) {
        nsCOMPtr<nsIWidget> mainWidget;
        GetMainWidget(getter_AddRefs(mainWidget));
        if (mainWidget) {
          mainWidget->SetCursor(eCursor_spinning, nullptr, 0, 0);
        }
      }
    }
  } else if ((~aStateFlags & (STATE_TRANSFERRING | STATE_IS_DOCUMENT)) == 0) {
    // Page is loading
    mBusyFlags = (BusyFlags)(BUSY_FLAGS_BUSY | BUSY_FLAGS_PAGE_LOADING);
  } else if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_NETWORK)) {
    // Page has finished loading
    mBusyFlags = BUSY_FLAGS_NONE;

    // Hide the progress cursor if the pref is set
    if (StaticPrefs::ui_use_activity_cursor()) {
      nsCOMPtr<nsIWidget> mainWidget;
      GetMainWidget(getter_AddRefs(mainWidget));
      if (mainWidget) {
        mainWidget->SetCursor(eCursor_standard, nullptr, 0, 0);
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
                             nsIURI* aURI, uint32_t aFlags) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

void nsDocShell::OnRedirectStateChange(nsIChannel* aOldChannel,
                                       nsIChannel* aNewChannel,
                                       uint32_t aRedirectFlags,
                                       uint32_t aStateFlags) {
  NS_ASSERTION(aStateFlags & STATE_REDIRECTING,
               "Calling OnRedirectStateChange when there is no redirect");

  // If mixed content is allowed for the old channel, we forward
  // the permission to the new channel if it has the same origin
  // as the old one.
  if (mMixedContentChannel && mMixedContentChannel == aOldChannel) {
    nsresult rv =
        nsContentUtils::CheckSameOrigin(mMixedContentChannel, aNewChannel);
    if (NS_SUCCEEDED(rv)) {
      SetMixedContentChannel(aNewChannel);  // Same origin: forward permission.
    } else {
      SetMixedContentChannel(
          nullptr);  // Different origin: clear mMixedContentChannel.
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
    // Get the HTTP response code, if available.
    uint32_t responseStatus = 0;
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aOldChannel);
    if (httpChannel) {
      Unused << httpChannel->GetResponseStatus(&responseStatus);
    }

    // Add visit N -1 => N
    AddURIVisit(oldURI, previousURI, previousFlags, responseStatus);

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
        secMan->GetDocShellContentPrincipal(newURI, this,
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
nsDocShell::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                           nsresult aStatus, const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                             uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                   nsIRequest* aRequest, uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsresult nsDocShell::EndPageLoad(nsIWebProgress* aProgress,
                                 nsIChannel* aChannel, nsresult aStatus) {
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
  mozilla::Unused << loadingSHE;  // XXX: Not sure if we need this anymore

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
    nsCOMPtr<nsIContentViewer> contentViewer = mContentViewer;
    contentViewer->LoadComplete(aStatus);
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
    mLSHE->SetLoadType(LOAD_HISTORY);

    // Clear the mLSHE reference to indicate document loading is done one
    // way or another.
    SetHistoryEntry(&mLSHE, nullptr);
  }
  // if there's a refresh header in the channel, this method
  // will set it up for us.
  if (mIsActive || !mDisableMetaRefreshWhenInactive) RefreshURIFromQueue();

  // Test whether this is the top frame or a subframe
  bool isTopFrame = true;
  nsCOMPtr<nsIDocShellTreeItem> targetParentTreeItem;
  rv = GetInProcessSameTypeParent(getter_AddRefs(targetParentTreeItem));
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
    if (!isTopFrame &&
        UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(aStatus)) {
      // frameElement is our nsIContent to be annotated
      RefPtr<Element> frameElement;
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
      GetInProcessSameTypeParent(getter_AddRefs(parentItem));
      if (!parentItem) {
        return NS_OK;
      }

      RefPtr<Document> parentDoc;
      parentDoc = parentItem->GetDocument();
      if (!parentDoc) {
        return NS_OK;
      }

      parentDoc->AddBlockedNodeByClassifier(frameElement);

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
            if (idnSrv && NS_SUCCEEDED(idnSrv->IsACE(host, &isACE)) && isACE &&
                NS_SUCCEEDED(idnSrv->ConvertACEtoUTF8(host, utf8Host))) {
              sURIFixup->KeywordToURI(utf8Host, getter_AddRefs(newPostData),
                                      getter_AddRefs(info));
            } else {
              sURIFixup->KeywordToURI(host, getter_AddRefs(newPostData),
                                      getter_AddRefs(info));
            }
          }

          info->GetPreferredURI(getter_AddRefs(newURI));
          if (newURI) {
            info->GetKeywordAsSent(keywordAsSent);
            info->GetKeywordProviderName(keywordProviderName);
          }
        }  // end keywordsEnabled
      }

      //
      // Now try change the address, e.g. turn http://foo into
      // http://www.foo.com
      //
      if (aStatus == NS_ERROR_UNKNOWN_HOST || aStatus == NS_ERROR_NET_RESET) {
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
          sURIFixup->CreateFixupURI(
              oldSpec, nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
              getter_AddRefs(newPostData), getter_AddRefs(newURI));
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

          nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
          MOZ_ASSERT(loadInfo, "loadInfo is required on all channels");
          nsCOMPtr<nsIPrincipal> triggeringPrincipal =
              loadInfo->TriggeringPrincipal();

          LoadURIOptions loadURIOptions;
          loadURIOptions.mTriggeringPrincipal = triggeringPrincipal;
          loadURIOptions.mCsp = loadInfo->GetCspToInherit();
          loadURIOptions.mPostData = newPostData;
          return LoadURI(newSpecW, loadURIOptions);
        }
      }
    }

    // Well, fixup didn't work :-(
    // It is time to throw an error dialog box, and be done with it...

    // Errors to be shown only on top-level frames
    if ((aStatus == NS_ERROR_UNKNOWN_HOST ||
         aStatus == NS_ERROR_CONNECTION_REFUSED ||
         aStatus == NS_ERROR_UNKNOWN_PROXY_HOST ||
         aStatus == NS_ERROR_PROXY_CONNECTION_REFUSED ||
         aStatus == NS_ERROR_PROXY_AUTHENTICATION_FAILED ||
         aStatus == NS_ERROR_TOO_MANY_REQUESTS ||
         aStatus == NS_ERROR_BLOCKED_BY_POLICY) &&
        (isTopFrame || UseErrorPages())) {
      DisplayLoadError(aStatus, url, nullptr, aChannel);
    } else if (aStatus == NS_ERROR_NET_TIMEOUT ||
               aStatus == NS_ERROR_PROXY_GATEWAY_TIMEOUT ||
               aStatus == NS_ERROR_REDIRECT_LOOP ||
               aStatus == NS_ERROR_UNKNOWN_SOCKET_TYPE ||
               aStatus == NS_ERROR_NET_INTERRUPT ||
               aStatus == NS_ERROR_NET_RESET ||
               aStatus == NS_ERROR_PROXY_BAD_GATEWAY ||
               aStatus == NS_ERROR_OFFLINE || aStatus == NS_ERROR_MALWARE_URI ||
               aStatus == NS_ERROR_PHISHING_URI ||
               aStatus == NS_ERROR_UNWANTED_URI ||
               aStatus == NS_ERROR_HARMFUL_URI ||
               aStatus == NS_ERROR_UNSAFE_CONTENT_TYPE ||
               aStatus == NS_ERROR_REMOTE_XUL ||
               aStatus == NS_ERROR_INTERCEPTION_FAILED ||
               aStatus == NS_ERROR_NET_INADEQUATE_SECURITY ||
               aStatus == NS_ERROR_NET_HTTP2_SENT_GOAWAY ||
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
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    PredictorLearnRedirect(url, aChannel, loadInfo->GetOriginAttributes());
  }

  return NS_OK;
}

//*****************************************************************************
// nsDocShell: Content Viewer Management
//*****************************************************************************

nsresult nsDocShell::EnsureContentViewer() {
  if (mContentViewer) {
    return NS_OK;
  }
  if (mIsBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentSecurityPolicy> cspToInheritForAboutBlank;
  nsCOMPtr<nsIURI> baseURI;
  nsIPrincipal* principal = GetInheritedPrincipal(false);
  nsIPrincipal* storagePrincipal = GetInheritedPrincipal(false, true);

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  GetInProcessSameTypeParent(getter_AddRefs(parentItem));
  if (parentItem) {
    if (nsCOMPtr<nsPIDOMWindowOuter> domWin = GetWindow()) {
      nsCOMPtr<Element> parentElement = domWin->GetFrameElementInternal();
      if (parentElement) {
        baseURI = parentElement->GetBaseURI();
        cspToInheritForAboutBlank = parentElement->GetCsp();
      }
    }
  }

  nsresult rv = CreateAboutBlankContentViewer(
      principal, storagePrincipal, cspToInheritForAboutBlank, baseURI);

  NS_ENSURE_STATE(mContentViewer);

  if (NS_SUCCEEDED(rv)) {
    RefPtr<Document> doc(GetDocument());
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

nsresult nsDocShell::CreateAboutBlankContentViewer(
    nsIPrincipal* aPrincipal, nsIPrincipal* aStoragePrincipal,
    nsIContentSecurityPolicy* aCSP, nsIURI* aBaseURI,
    bool aTryToSaveOldPresentation, bool aCheckPermitUnload,
    WindowGlobalChild* aActor) {
  RefPtr<Document> blankDoc;
  nsCOMPtr<nsIContentViewer> viewer;
  nsresult rv = NS_ERROR_FAILURE;

  MOZ_ASSERT_IF(aActor, aActor->DocumentPrincipal() == aPrincipal);

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
    // pagehide notification might destroy this docshell.
    if (mIsBeingDestroyed) {
      return NS_ERROR_DOCSHELL_DYING;
    }
  }

  // Now make sure we don't think we're in the middle of firing unload after
  // this point.  This will make us fire unload when the about:blank document
  // unloads... but that's ok, more or less.  Would be nice if it fired load
  // too, of course.
  mFiredUnloadEvent = false;

  nsCOMPtr<nsIDocumentLoaderFactory> docFactory =
      nsContentUtils::FindInternalContentViewer(
          NS_LITERAL_CSTRING("text/html"));

  if (docFactory) {
    nsCOMPtr<nsIPrincipal> principal, storagePrincipal;
    if (mSandboxFlags & SANDBOXED_ORIGIN) {
      if (aPrincipal) {
        principal = NullPrincipal::CreateWithInheritedAttributes(aPrincipal);
      } else {
        principal = NullPrincipal::CreateWithInheritedAttributes(this);
      }
      storagePrincipal = principal;
    } else {
      principal = aPrincipal;
      storagePrincipal = aStoragePrincipal;
    }

    MaybeCreateInitialClientSource(principal);

    // generate (about:blank) document to load
    blankDoc = nsContentDLF::CreateBlankDocument(mLoadGroup, principal,
                                                 storagePrincipal, this);
    if (blankDoc) {
      // Hack: manually set the CSP for the new document
      // Please create an actual copy of the CSP (do not share the same
      // reference) otherwise appending a new policy within the new
      // document will be incorrectly propagated to the opening doc.
      if (aCSP) {
        RefPtr<nsCSPContext> cspToInherit = new nsCSPContext();
        cspToInherit->InitFromOther(static_cast<nsCSPContext*>(aCSP));
        blankDoc->SetCsp(cspToInherit);
      }

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
        rv = Embed(viewer, aActor);
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
nsDocShell::CreateAboutBlankContentViewer(nsIPrincipal* aPrincipal,
                                          nsIPrincipal* aStoragePrincipal,
                                          nsIContentSecurityPolicy* aCSP) {
  return CreateAboutBlankContentViewer(aPrincipal, aStoragePrincipal, aCSP,
                                       nullptr);
}

nsresult nsDocShell::CreateContentViewerForActor(
    WindowGlobalChild* aWindowActor) {
  MOZ_ASSERT(aWindowActor);

  // FIXME: WindowGlobalChild should provide the StoragePrincipal.
  nsresult rv = CreateAboutBlankContentViewer(
      aWindowActor->DocumentPrincipal(), aWindowActor->DocumentPrincipal(),
      /* aCsp */ nullptr,
      /* aBaseURI */ nullptr,
      /* aTryToSaveOldPresentation */ true,
      /* aCheckPermitUnload */ true, aWindowActor);
  if (NS_SUCCEEDED(rv)) {
    RefPtr<Document> doc(GetDocument());
    MOZ_ASSERT(
        doc,
        "Should have a document if CreateAboutBlankContentViewer succeeded");
    MOZ_ASSERT(doc->GetOwnerGlobal() == aWindowActor->WindowGlobal(),
               "New document should be in the same global as our actor");

    // FIXME: We may want to support non-initial documents here.
    doc->SetIsInitialDocument(true);
  }

  return rv;
}

bool nsDocShell::CanSavePresentation(uint32_t aLoadType,
                                     nsIRequest* aNewRequest,
                                     Document* aNewDocument) {
  if (!mOSHE) {
    return false;  // no entry to save into
  }

  nsCOMPtr<nsIContentViewer> viewer = mOSHE->GetContentViewer();
  if (viewer) {
    NS_WARNING("mOSHE already has a content viewer!");
    return false;
  }

  // Only save presentation for "normal" loads and link loads.  Anything else
  // probably wants to refetch the page, so caching the old presentation
  // would be incorrect.
  if (aLoadType != LOAD_NORMAL && aLoadType != LOAD_HISTORY &&
      aLoadType != LOAD_LINK && aLoadType != LOAD_STOP_CONTENT &&
      aLoadType != LOAD_STOP_CONTENT_AND_REPLACE &&
      aLoadType != LOAD_ERROR_PAGE) {
    return false;
  }

  // If the session history entry has the saveLayoutState flag set to false,
  // then we should not cache the presentation.
  if (!mOSHE->GetSaveLayoutStateFlag()) {
    return false;
  }

  // If the document is not done loading, don't cache it.
  if (!mScriptGlobal || mScriptGlobal->IsLoading()) {
    MOZ_LOG(gPageCacheLog, mozilla::LogLevel::Verbose,
            ("Blocked due to document still loading"));
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
  GetInProcessSameTypeParent(getter_AddRefs(root));
  if (root && root != this) {
    return false;  // this is a subframe load
  }

  // If the document does not want its presentation cached, then don't.
  RefPtr<Document> doc = mScriptGlobal->GetExtantDoc();

  uint16_t bfCacheCombo = 0;
  bool canSavePresentation =
      doc->CanSavePresentation(aNewRequest, bfCacheCombo);
  ReportBFCacheComboTelemetry(bfCacheCombo);

  return doc && canSavePresentation;
}

void nsDocShell::ReportBFCacheComboTelemetry(uint16_t aCombo) {
  switch (aCombo) {
    case BFCACHE_SUCCESS:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::BFCache_Success);
      break;
    case UNLOAD:
      Telemetry::AccumulateCategorical(Telemetry::LABELS_BFCACHE_COMBO::Unload);
      break;
    case UNLOAD_REQUEST:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::Unload_Req);
      break;
    case REQUEST:
      Telemetry::AccumulateCategorical(Telemetry::LABELS_BFCACHE_COMBO::Req);
      break;
    case UNLOAD_REQUEST_PEER:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::Unload_Req_Peer);
      break;
    case UNLOAD_REQUEST_PEER_MSE:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::Unload_Req_Peer_MSE);
      break;
    case UNLOAD_REQUEST_MSE:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::Unload_Req_MSE);
      break;
    case SUSPENDED_UNLOAD_REQUEST_PEER:
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_BFCACHE_COMBO::SPD_Unload_Req_Peer);
      break;
    default:
      Telemetry::AccumulateCategorical(Telemetry::LABELS_BFCACHE_COMBO::Other);
      break;
  }
};

void nsDocShell::ReattachEditorToWindow(nsISHEntry* aSHEntry) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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

void nsDocShell::DetachEditorFromWindow() {
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
      MOZ_ASSERT(!mIsBeingDestroyed || !mOSHE->HasDetachedEditor(),
                 "We should not set the editor data again once after we "
                 "detached the editor data during destroying this docshell");
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
#endif  // DEBUG
}

nsresult nsDocShell::CaptureState() {
  if (!mOSHE || mOSHE == mLSHE) {
    // No entry to save into, or we're replacing the existing entry.
    return NS_ERROR_FAILURE;
  }

  if (!mScriptGlobal) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> windowState = mScriptGlobal->SaveWindowState();
  NS_ENSURE_TRUE(windowState, NS_ERROR_FAILURE);

  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gPageCacheLog, LogLevel::Debug))) {
    nsCOMPtr<nsIURI> uri = mOSHE->GetURI();
    nsAutoCString spec;
    if (uri) {
      uri->GetSpec(spec);
    }
    MOZ_LOG(gPageCacheLog, LogLevel::Debug,
            ("Saving presentation into session history, URI: %s", spec.get()));
  }

  mOSHE->SetWindowState(windowState);

  // Suspend refresh URIs and save off the timer queue
  mOSHE->SetRefreshURIList(mSavedRefreshURIList);

  // Capture the current content viewer bounds.
  if (mContentViewer) {
    nsIntRect bounds;
    mContentViewer->GetBounds(bounds);
    mOSHE->SetViewerBounds(bounds);
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
nsDocShell::RestorePresentationEvent::Run() {
  if (mDocShell && NS_FAILED(mDocShell->RestoreFromHistory())) {
    NS_WARNING("RestoreFromHistory failed");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::BeginRestore(nsIContentViewer* aContentViewer, bool aTop) {
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

  RefPtr<Document> doc = aContentViewer->GetDocument();
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

nsresult nsDocShell::BeginRestoreChildren() {
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
nsDocShell::FinishRestore() {
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

  RefPtr<Document> doc = GetDocument();
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
nsDocShell::GetRestoringDocument(bool* aRestoring) {
  *aRestoring = mIsRestoringDocument;
  return NS_OK;
}

nsresult nsDocShell::RestorePresentation(nsISHEntry* aSHEntry,
                                         bool* aRestoring) {
  MOZ_ASSERT(!mIsBeingDestroyed);

  NS_ASSERTION(mLoadType & LOAD_CMD_HISTORY,
               "RestorePresentation should only be called for history loads");

  nsCOMPtr<nsIContentViewer> viewer = aSHEntry->GetContentViewer();

  nsAutoCString spec;
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gPageCacheLog, LogLevel::Debug))) {
    nsCOMPtr<nsIURI> uri = aSHEntry->GetURI();
    if (uri) {
      uri->GetSpec(spec);
    }
  }

  *aRestoring = false;

  if (!viewer) {
    MOZ_LOG(gPageCacheLog, LogLevel::Debug,
            ("no saved presentation for uri: %s", spec.get()));
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
    MOZ_LOG(gPageCacheLog, LogLevel::Debug,
            ("No valid container, clearing presentation"));
    aSHEntry->SetContentViewer(nullptr);
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mContentViewer != viewer, "Restoring existing presentation");

  MOZ_LOG(gPageCacheLog, LogLevel::Debug,
          ("restoring presentation from session history: %s", spec.get()));

  SetHistoryEntry(&mLSHE, aSHEntry);

  // Post an event that will remove the request after we've returned
  // to the event loop.  This mimics the way it is called by nsIChannel
  // implementations.

  // Revoke any pending restore (just in case)
  NS_ASSERTION(!mRestorePresentationEvent.IsPending(),
               "should only have one RestorePresentationEvent");
  mRestorePresentationEvent.Revoke();

  RefPtr<RestorePresentationEvent> evt = new RestorePresentationEvent(this);
  nsresult rv = DispatchToTabGroup(
      TaskCategory::Other, RefPtr<RestorePresentationEvent>(evt).forget());
  if (NS_SUCCEEDED(rv)) {
    mRestorePresentationEvent = evt.get();
    // The rest of the restore processing will happen on our event
    // callback.
    *aRestoring = true;
  }

  return rv;
}

namespace {
class MOZ_STACK_CLASS PresentationEventForgetter {
 public:
  explicit PresentationEventForgetter(
      nsRevocableEventPtr<nsDocShell::RestorePresentationEvent>&
          aRestorePresentationEvent)
      : mRestorePresentationEvent(aRestorePresentationEvent),
        mEvent(aRestorePresentationEvent.get()) {}

  ~PresentationEventForgetter() { Forget(); }

  void Forget() {
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

}  // namespace

bool nsDocShell::SandboxFlagsImplyCookies(const uint32_t& aSandboxFlags) {
  return (aSandboxFlags & (SANDBOXED_ORIGIN | SANDBOXED_SCRIPTS)) == 0;
}

nsresult nsDocShell::RestoreFromHistory() {
  MOZ_ASSERT(mRestorePresentationEvent.IsPending());
  PresentationEventForgetter forgetter(mRestorePresentationEvent);

  // This section of code follows the same ordering as CreateContentViewer.
  if (!mLSHE) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentViewer> viewer = mLSHE->GetContentViewer();
  if (!viewer) {
    return NS_ERROR_FAILURE;
  }

  if (mSavingOldViewer) {
    // We determined that it was safe to cache the document presentation
    // at the time we initiated the new load.  We need to check whether
    // it's still safe to do so, since there may have been DOM mutations
    // or new requests initiated.
    RefPtr<Document> doc = viewer->GetDocument();
    nsIRequest* request = nullptr;
    if (doc) {
      request = doc->GetChannel();
    }
    mSavingOldViewer = CanSavePresentation(mLoadType, request, doc);
  }

  nsCOMPtr<nsIContentViewer> oldCv(mContentViewer);
  nsCOMPtr<nsIContentViewer> newCv(viewer);
  float textZoom = 1.0f;
  float pageZoom = 1.0f;
  float overrideDPPX = 0.0f;

  bool styleDisabled = false;
  if (oldCv && newCv) {
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
  // pagehide notification might destroy this docshell.
  if (mIsBeingDestroyed) {
    return NS_ERROR_DOCSHELL_DYING;
  }

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
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (rootSH) {
    mPreviousEntryIndex = rootSH->Index();
    rootSH->LegacySHistory()->UpdateIndex();
    mLoadedEntryIndex = rootSH->Index();
    MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
            ("Previous index: %d, Loaded index: %d", mPreviousEntryIndex,
             mLoadedEntryIndex));
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
    // Make sure to hold a strong ref to previousViewer here while we
    // drop the reference to it from mContentViewer.
    nsCOMPtr<nsIContentViewer> previousViewer =
        mContentViewer->GetPreviousViewer();
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

  PresShell* oldPresShell = GetPresShell();
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
  RefPtr<Document> sibling;
  if (rootViewParent && rootViewParent->GetParent()) {
    nsIFrame* frame = rootViewParent->GetParent()->GetFrame();
    container = frame ? frame->GetContent() : nullptr;
  }
  if (rootViewSibling) {
    nsIFrame* frame = rootViewSibling->GetFrame();
    sibling = frame ? frame->PresShell()->GetDocument() : nullptr;
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

  // Move the browsing ontext's children to the cache. If we're
  // detaching them, we'll detach them from there.
  mBrowsingContext->CacheChildren();

  // Now that we're about to switch documents, forget all of our children.
  // Note that we cached them as needed up in CaptureState above.
  DestroyChildren();

  mContentViewer.swap(viewer);

  // Grab all of the related presentation from the SHEntry now.
  // Clearing the viewer from the SHEntry will clear all of this state.
  nsCOMPtr<nsISupports> windowState = mLSHE->GetWindowState();
  mLSHE->SetWindowState(nullptr);

  bool sticky = mLSHE->GetSticky();

  RefPtr<Document> document = mContentViewer->GetDocument();

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
  nsCOMPtr<nsIMutableArray> refreshURIList = mLSHE->GetRefreshURIList();

  // Reattach to the window object.
  mIsRestoringDocument = true;  // for MediaDocument::BecomeInteractive
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
    nsCOMPtr<nsIMutableArray> refreshURIs = mLSHE->GetRefreshURIList();
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
    newCv->SetTextZoom(textZoom);
    newCv->SetFullZoom(pageZoom);
    newCv->SetOverrideDPPX(overrideDPPX);
    newCv->SetAuthorStyleDisabled(styleDisabled);
  }

  if (document) {
    RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
    if (parent) {
      RefPtr<Document> d = parent->GetDocument();
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
    nsCOMPtr<nsIURI> uri = origLSHE->GetURI();
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

  BrowsingContext::Children contexts(childShells.Count());
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
    // child inherits our mIsActive mPrivateBrowsingId, which is what we want.
    AddChild(childItem);

    contexts.AppendElement(childShell->GetBrowsingContext());

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

  if (!contexts.IsEmpty()) {
    mBrowsingContext->RestoreChildren(std::move(contexts));
  }

  // Make sure to restore the window state after adding the child shells back
  // to the tree.  This is necessary for Thaw() and Resume() to propagate
  // properly.
  rv = privWin->RestoreWindowState(windowState);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<PresShell> presShell = GetPresShell();

  // We may be displayed on a different monitor (or in a different
  // HiDPI mode) than when we got into the history list.  So we need
  // to check if this has happened. See bug 838239.

  // Because the prescontext normally handles resolution changes via
  // a runnable (see nsPresContext::UIResolutionChanged), its device
  // context won't be -immediately- updated as a result of calling
  // presShell->BackingScaleFactorChanged().

  // But we depend on that device context when adjusting the view size
  // via mContentViewer->SetBounds(newBounds) below. So we need to
  // explicitly tell it to check for changed resolution here.
  if (presShell &&
      presShell->GetPresContext()->DeviceContext()->CheckDPIChange()) {
    presShell->BackingScaleFactorChanged();
  }

  nsViewManager* newVM = presShell ? presShell->GetViewManager() : nullptr;
  nsView* newRootView = newVM ? newVM->GetRootView() : nullptr;

  // Insert the new root view at the correct location in the view tree.
  if (container) {
    nsSubDocumentFrame* subDocFrame =
        do_QueryFrame(container->GetPrimaryFrame());
    rootViewParent = subDocFrame ? subDocFrame->EnsureInnerView() : nullptr;
  } else {
    rootViewParent = nullptr;
  }
  if (sibling && sibling->GetPresShell() &&
      sibling->GetPresShell()->GetViewManager()) {
    rootViewSibling = sibling->GetPresShell()->GetViewManager()->GetRootView();
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
      parentVM->InsertChild(rootViewParent, newRootView, rootViewSibling,
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

  // Now that we have found the inner window of the page restored
  // from the history, we have to  make sure that
  // performance.navigation.type is 2.
  privWinInner->GetPerformance()->GetDOMTiming()->NotifyRestoreStart();

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
      MOZ_LOG(gPageCacheLog, LogLevel::Debug,
              ("resize widget(%d, %d, %d, %d)", newBounds.x, newBounds.y,
               newBounds.width, newBounds.height));
      mContentViewer->SetBounds(newBounds);
    } else {
      nsIScrollableFrame* rootScrollFrame =
          presShell->GetRootScrollFrameAsScrollable();
      if (rootScrollFrame) {
        rootScrollFrame->PostScrolledAreaEventForCurrentArea();
      }
    }
  }

  // The FinishRestore call below can kill these, null them out so we don't
  // have invalid pointer lying around.
  newRootView = rootViewSibling = rootViewParent = nullptr;
  newVM = nullptr;

  // If the IsUnderHiddenEmbedderElement() state has been changed, we need to
  // update it.
  if (oldPresShell && presShell &&
      presShell->IsUnderHiddenEmbedderElement() !=
          oldPresShell->IsUnderHiddenEmbedderElement()) {
    presShell->SetIsUnderHiddenEmbedderElement(
        oldPresShell->IsUnderHiddenEmbedderElement());
  }

  // Simulate the completion of the load.
  nsDocShell::FinishRestore();

  // Restart plugins, and paint the content.
  if (presShell) {
    presShell->Thaw();
  }

  return privWin->FireDelayedDOMEvents();
}

nsresult nsDocShell::CreateContentViewer(const nsACString& aContentType,
                                         nsIRequest* aRequest,
                                         nsIStreamListener** aContentHandler) {
  if (DocGroup::TryToLoadIframesInBackground()) {
    ResetToFirstLoad();
  }

  *aContentHandler = nullptr;

  if (!mTreeOwner || mIsBeingDestroyed) {
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
    RefPtr<Document> doc = viewer->GetDocument();
    mSavingOldViewer = CanSavePresentation(mLoadType, aRequest, doc);
  }

  NS_ASSERTION(!mLoadingURI, "Re-entering unload?");

  nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(aRequest);
  if (aOpenedChannel) {
    aOpenedChannel->GetURI(getter_AddRefs(mLoadingURI));
  }
  FirePageHideNotification(!mSavingOldViewer);
  if (mIsBeingDestroyed) {
    // Force to stop the newly created orphaned viewer.
    viewer->Stop();
    return NS_ERROR_DOCSHELL_DYING;
  }
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

    Document* doc = viewer->GetDocument();
    if (doc) {
      doc->SetFailedChannel(failedChannel);
    }

    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    if (failedChannel) {
      // Make sure we have a URI to set currentURI.
      NS_GetFinalChannelURI(failedChannel, getter_AddRefs(failedURI));
    } else {
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
      errorOnLocationChangeNeeded =
          OnNewURI(failedURI, failedChannel, triggeringPrincipal, nullptr,
                   nullptr, mLoadType, nullptr, false, false, false);
    }

    // Be sure to have a correct mLSHE, it may have been cleared by
    // EndPageLoad. See bug 302115.
    if (mSessionHistory && !mLSHE) {
      int32_t idx = mSessionHistory->LegacySHistory()->GetRequestedIndex();
      if (idx == -1) {
        idx = mSessionHistory->Index();
      }
      mSessionHistory->LegacySHistory()->GetEntryAtIndex(idx,
                                                         getter_AddRefs(mLSHE));
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

  if (DocGroup::TryToLoadIframesInBackground()) {
    if ((!mContentViewer || GetDocument()->IsInitialDocument()) && IsFrame()) {
      // At this point, we know we just created a new iframe document based on
      // the response from the server, and we check if it's a cross-domain
      // iframe

      RefPtr<Document> newDoc = viewer->GetDocument();

      RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
      nsCOMPtr<nsIPrincipal> parentPrincipal =
          parent->GetDocument()->NodePrincipal();
      nsCOMPtr<nsIPrincipal> thisPrincipal = newDoc->NodePrincipal();

      SiteIdentifier parentSite;
      SiteIdentifier thisSite;

      nsresult rv =
          BasePrincipal::Cast(parentPrincipal)->GetSiteIdentifier(parentSite);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = BasePrincipal::Cast(thisPrincipal)->GetSiteIdentifier(thisSite);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!parentSite.Equals(thisSite)) {
#ifdef MOZ_GECKO_PROFILER
        nsCOMPtr<nsIURI> prinURI;
        thisPrincipal->GetURI(getter_AddRefs(prinURI));
        nsPrintfCString marker("Iframe loaded in background: %s",
                               prinURI->GetSpecOrDefault().get());
        TimeStamp now = TimeStamp::Now();
        profiler_add_text_marker("Background Iframe", marker,
                                 JS::ProfilingCategoryPair::DOM, now, now,
                                 Nothing(), Nothing());
#endif
        SetBackgroundLoadIframe();
      }
    }
  }

  NS_ENSURE_SUCCESS(Embed(viewer), NS_ERROR_FAILURE);

  if (TreatAsBackgroundLoad()) {
    nsCOMPtr<nsIRunnable> triggerParentCheckDocShell =
        NewRunnableMethod("nsDocShell::TriggerParentCheckDocShellIsEmpty", this,
                          &nsDocShell::TriggerParentCheckDocShellIsEmpty);
    nsresult rv = NS_DispatchToCurrentThread(triggerParentCheckDocShell);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mSavedRefreshURIList = nullptr;
  mSavingOldViewer = false;
  mEODForCurrentDocument = false;

  // if this document is part of a multipart document,
  // the ID can be used to distinguish it from the other parts.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(aRequest));
  if (multiPartChannel) {
    if (PresShell* presShell = GetPresShell()) {
      if (Document* doc = presShell->GetDocument()) {
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

  if (errorOnLocationChangeNeeded) {
    FireOnLocationChange(this, failedChannel, failedURI,
                         LOCATION_CHANGE_ERROR_PAGE);
  } else if (onLocationChangeNeeded) {
    uint32_t locationFlags =
        (mLoadType & LOAD_CMD_RELOAD) ? uint32_t(LOCATION_CHANGE_RELOAD) : 0;
    FireOnLocationChange(this, aRequest, mCurrentURI, locationFlags);
  }

  return NS_OK;
}

nsresult nsDocShell::NewContentViewerObj(const nsACString& aContentType,
                                         nsIRequest* aRequest,
                                         nsILoadGroup* aLoadGroup,
                                         nsIStreamListener** aContentHandler,
                                         nsIContentViewer** aViewer) {
  nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(aRequest);

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
      nsContentUtils::FindInternalContentViewer(aContentType);
  if (!docLoaderFactory) {
    return NS_ERROR_FAILURE;
  }

  // Now create an instance of the content viewer nsLayoutDLF makes the
  // determination if it should be a "view-source" instead of "view"
  nsresult rv = docLoaderFactory->CreateInstance(
      "view", aOpenedChannel, aLoadGroup, aContentType, this, nullptr,
      aContentHandler, aViewer);
  NS_ENSURE_SUCCESS(rv, rv);

  (*aViewer)->SetContainer(this);
  return NS_OK;
}

nsresult nsDocShell::SetupNewViewer(nsIContentViewer* aNewViewer,
                                    WindowGlobalChild* aWindowActor) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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
  NS_ENSURE_SUCCESS(GetInProcessSameTypeParent(getter_AddRefs(parentAsItem)),
                    NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));

  const Encoding* forceCharset = nullptr;
  const Encoding* hintCharset = nullptr;
  int32_t hintCharsetSource = kCharsetUninitialized;
  float textZoom = 1.0;
  float pageZoom = 1.0;
  float overrideDPPX = 1.0;
  bool styleDisabled = false;
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
        NS_ENSURE_SUCCESS(oldCv->GetTextZoom(&textZoom), NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetFullZoom(&pageZoom), NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetOverrideDPPX(&overrideDPPX),
                          NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(oldCv->GetAuthorStyleDisabled(&styleDisabled),
                          NS_ERROR_FAILURE);
      }
    }
  }

  nscolor bgcolor = NS_RGBA(0, 0, 0, 0);
  bool isActive = false;
  // Ensure that the content viewer is destroyed *after* the GC - bug 71515
  nsCOMPtr<nsIContentViewer> contentViewer = mContentViewer;
  if (contentViewer) {
    // Stop any activity that may be happening in the old document before
    // releasing it...
    contentViewer->Stop();

    // Try to extract the canvas background color from the old
    // presentation shell, so we can use it for the next document.
    if (PresShell* presShell = contentViewer->GetPresShell()) {
      bgcolor = presShell->GetCanvasBackground();
      isActive = presShell->IsActive();
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

  // Move the browsing ontext's children to the cache. If we're
  // detaching them, we'll detach them from there.
  mBrowsingContext->CacheChildren();

  // Now that we're about to switch documents, forget all of our children.
  // Note that we cached them as needed up in CaptureState above.
  DestroyChildren();

  mContentViewer = aNewViewer;

  nsCOMPtr<nsIWidget> widget;
  NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(widget)), NS_ERROR_FAILURE);

  nsIntRect bounds(x, y, cx, cy);

  mContentViewer->SetNavigationTiming(mTiming);

  if (NS_FAILED(mContentViewer->Init(widget, bounds, aWindowActor))) {
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
    NS_ENSURE_SUCCESS(newCv->SetTextZoom(textZoom), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetFullZoom(pageZoom), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetOverrideDPPX(overrideDPPX), NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(newCv->SetAuthorStyleDisabled(styleDisabled),
                      NS_ERROR_FAILURE);
  }

  // Stuff the bgcolor from the old pres shell into the new
  // pres shell. This improves page load continuity.
  if (RefPtr<PresShell> presShell = mContentViewer->GetPresShell()) {
    presShell->SetCanvasBackground(bgcolor);
    if (isActive) {
      presShell->SetIsActive(isActive);
    }
  }

  // XXX: It looks like the LayoutState gets restored again in Embed()
  //      right after the call to SetupNewViewer(...)

  // We don't show the mContentViewer yet, since we want to draw the old page
  // until we have enough of the new page to show.  Just return with the new
  // viewer still set to hidden.

  return NS_OK;
}

nsresult nsDocShell::SetDocCurrentStateObj(nsISHEntry* aShEntry) {
  NS_ENSURE_STATE(mContentViewer);
  RefPtr<Document> document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIStructuredCloneContainer> scContainer;
  if (aShEntry) {
    scContainer = aShEntry->GetStateData();

    // If aShEntry is null, just set the document's state object to null.
  }

  // It's OK for scContainer too be null here; that just means there's no
  // state data associated with this history entry.
  document->SetStateObject(scContainer);

  return NS_OK;
}

nsresult nsDocShell::CheckLoadingPermissions() {
  // This method checks whether the caller may load content into
  // this docshell. Even though we've done our best to hide windows
  // from code that doesn't have the right to access them, it's
  // still possible for an evil site to open a window and access
  // frames in the new window through window.frames[] (which is
  // allAccess for historic reasons), so we still need to do this
  // check on load.
  nsresult rv = NS_OK;

  if (!IsFrame()) {
    // We're not a frame. Permit all loads.
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
    item->GetInProcessSameTypeParent(getter_AddRefs(tmp));
    item.swap(tmp);
  } while (item);

  return NS_ERROR_DOM_PROP_ACCESS_DENIED;
}

//*****************************************************************************
// nsDocShell: Site Loading
//*****************************************************************************

void nsDocShell::CopyFavicon(nsIURI* aOldURI, nsIURI* aNewURI,
                             nsIPrincipal* aLoadingPrincipal,
                             bool aInPrivateBrowsing) {
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
  nsCOMPtr<nsIFaviconService> favSvc =
      do_GetService("@mozilla.org/browser/favicon-service;1");
  if (favSvc) {
    favSvc->CopyFavicons(aOldURI, aNewURI,
                         aInPrivateBrowsing
                             ? nsIFaviconService::FAVICON_LOAD_PRIVATE
                             : nsIFaviconService::FAVICON_LOAD_NON_PRIVATE,
                         nullptr);
  }
#endif
}

class InternalLoadEvent : public Runnable {
 public:
  InternalLoadEvent(nsDocShell* aDocShell, nsDocShellLoadState* aLoadState)
      : mozilla::Runnable("InternalLoadEvent"),
        mDocShell(aDocShell),
        mLoadState(aLoadState) {
    // For events, both target and filename should be the version of "null" they
    // expect. By the time the event is fired, both window targeting and file
    // downloading have been handled, so we should never have an internal load
    // event that retargets or had a download.
    mLoadState->SetTarget(EmptyString());
    mLoadState->SetFileName(VoidString());
  }

  NS_IMETHOD
  Run() override {
#ifndef ANDROID
    MOZ_ASSERT(mLoadState->TriggeringPrincipal(),
               "InternalLoadEvent: Should always have a principal here");
#endif
    return mDocShell->InternalLoad(mLoadState, nullptr, nullptr);
  }

 private:
  RefPtr<nsDocShell> mDocShell;
  RefPtr<nsDocShellLoadState> mLoadState;
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
bool nsDocShell::JustStartedNetworkLoad() {
  return mDocumentRequest && mDocumentRequest != GetCurrentDocChannel();
}

nsresult nsDocShell::MaybeHandleLoadDelegate(nsDocShellLoadState* aLoadState,
                                             uint32_t aWindowType,
                                             bool* aDidHandleLoad) {
  MOZ_ASSERT(aLoadState);
  MOZ_ASSERT(aDidHandleLoad);
  MOZ_ASSERT(aWindowType == nsIBrowserDOMWindow::OPEN_NEWWINDOW ||
             aWindowType == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW);

  *aDidHandleLoad = false;
  // If we don't have a delegate or we're trying to load the error page, we
  // shouldn't be trying to do sandbox loads.
  if (!mLoadURIDelegate || aLoadState->LoadType() == LOAD_ERROR_PAGE) {
    return NS_OK;
  }

  // Dispatch only load requests for the current or a new window to the
  // delegate, e.g., to allow for GeckoView apps to handle the load event
  // outside of Gecko.
  Document* doc = mContentViewer ? mContentViewer->GetDocument() : nullptr;
  const bool isDocumentAuxSandboxed =
      doc && (doc->GetSandboxFlags() & SANDBOXED_AUXILIARY_NAVIGATION);

  if (aWindowType == nsIBrowserDOMWindow::OPEN_NEWWINDOW &&
      isDocumentAuxSandboxed) {
    // At this point, the load is invalid, and we can consider "handled", so
    // we'll exit from InternalLoad.
    *aDidHandleLoad = true;
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  return mLoadURIDelegate->LoadURI(
      aLoadState->URI(), aWindowType, aLoadState->LoadFlags(),
      aLoadState->TriggeringPrincipal(), aDidHandleLoad);
}

// The contentType will be INTERNAL_(I)FRAME if this docshell is for a
// non-toplevel browsing context in spec terms. (frame, iframe, <object>,
// <embed>, etc)
//
// This return value will be used when we call NS_CheckContentLoadPolicy, and
// later when we call DoURILoad.
uint32_t nsDocShell::DetermineContentType() {
  if (!IsFrame()) {
    return nsIContentPolicy::TYPE_DOCUMENT;
  }

  nsCOMPtr<Element> requestingElement =
      mScriptGlobal->GetFrameElementInternal();
  if (requestingElement) {
    return requestingElement->IsHTMLElement(nsGkAtoms::iframe)
               ? nsIContentPolicy::TYPE_INTERNAL_IFRAME
               : nsIContentPolicy::TYPE_INTERNAL_FRAME;
  }
  // If we have lost our frame element by now, just assume we're
  // an iframe since that's more common.
  return nsIContentPolicy::TYPE_INTERNAL_IFRAME;
}

nsresult nsDocShell::PerformRetargeting(nsDocShellLoadState* aLoadState,
                                        nsIDocShell** aDocShell,
                                        nsIRequest** aRequest) {
  MOZ_ASSERT(aLoadState, "need a load state!");
  MOZ_ASSERT(!aLoadState->Target().IsEmpty(), "should have a target here!");

  nsresult rv;
  nsCOMPtr<nsIDocShell> targetDocShell;

  // Locate the target DocShell.
  nsCOMPtr<nsIDocShellTreeItem> targetItem;
  // Only _self, _parent, and _top are supported in noopener case.  But we
  // have to be careful to not apply that to the noreferrer case.  See bug
  // 1358469.
  bool allowNamedTarget =
      !aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_NO_OPENER) ||
      aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER);
  if (allowNamedTarget ||
      aLoadState->Target().LowerCaseEqualsLiteral("_self") ||
      aLoadState->Target().LowerCaseEqualsLiteral("_parent") ||
      aLoadState->Target().LowerCaseEqualsLiteral("_top")) {
    rv = FindItemWithName(aLoadState->Target(), nullptr, this, false,
                          getter_AddRefs(targetItem));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  targetDocShell = do_QueryInterface(targetItem);
  if (!targetDocShell) {
    // If the targetDocShell doesn't exist, then this is a new docShell
    // and we should consider this a TYPE_DOCUMENT load
    //
    // For example, when target="_blank"

    // If there's no targetDocShell, that means we are about to create a new
    // window. Perform a content policy check before creating the window. Please
    // note for all other docshell loads content policy checks are performed
    // within the contentSecurityManager when the channel is about to be
    // openend.
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
          mScriptGlobal->GetFrameElementInternal();
      requestingContext = requestingElement;
    }

    // Ideally we should use the same loadinfo as within DoURILoad which
    // should match this one when both are applicable.
    nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new LoadInfo(
        mScriptGlobal, aLoadState->TriggeringPrincipal(), requestingContext,
        nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK);

    // Since Content Policy checks are performed within docShell as well as
    // the ContentSecurityManager we need a reliable way to let certain
    // nsIContentPolicy consumers ignore duplicate calls.
    secCheckLoadInfo->SetSkipContentPolicyCheckForWebRequest(true);

    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(aLoadState->URI(), secCheckLoadInfo,
                                   EmptyCString(),  // mime guess
                                   &shouldLoad);

    if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
      if (NS_SUCCEEDED(rv) && shouldLoad == nsIContentPolicy::REJECT_TYPE) {
        return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
      }

      return NS_ERROR_CONTENT_BLOCKED;
    }
  }

  if ((!targetDocShell || targetDocShell == static_cast<nsIDocShell*>(this))) {
    bool handled;
    rv = MaybeHandleLoadDelegate(aLoadState,
                                 targetDocShell
                                     ? nsIBrowserDOMWindow::OPEN_CURRENTWINDOW
                                     : nsIBrowserDOMWindow::OPEN_NEWWINDOW,
                                 &handled);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (handled) {
      return NS_OK;
    }
  }

  //
  // Resolve the window target before going any further...
  // If the load has been targeted to another DocShell, then transfer the
  // load to it...
  //

  // We've already done our owner-inheriting.  Mask out that bit, so we
  // don't try inheriting an owner from the target window if we came up
  // with a null owner above.
  aLoadState->UnsetLoadFlag(INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL);

  if (!targetDocShell) {
    // If the docshell's document is sandboxed, only open a new window
    // if the document's SANDBOXED_AUXILLARY_NAVIGATION flag is not set.
    // (i.e. if allow-popups is specified)
    NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
    Document* doc = mContentViewer->GetDocument();

    const bool isDocumentAuxSandboxed =
        doc && (doc->GetSandboxFlags() & SANDBOXED_AUXILIARY_NAVIGATION);

    if (isDocumentAuxSandboxed) {
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
    NS_ENSURE_TRUE(win, NS_ERROR_NOT_AVAILABLE);

    RefPtr<BrowsingContext> newBC;
    nsAutoCString spec;
    aLoadState->URI()->GetSpec(spec);

    // If we are a noopener load, we just hand the whole thing over to our
    // window.
    if (aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_NO_OPENER)) {
      // Various asserts that we know to hold because NO_OPENER loads can only
      // happen for links.
      MOZ_ASSERT(!aLoadState->LoadReplace());
      MOZ_ASSERT(aLoadState->PrincipalToInherit() ==
                 aLoadState->TriggeringPrincipal());
      MOZ_ASSERT(
          (aLoadState->LoadFlags() & ~INTERNAL_LOAD_FLAGS_IS_USER_TRIGGERED) ==
              INTERNAL_LOAD_FLAGS_NO_OPENER ||
          (aLoadState->LoadFlags() & ~INTERNAL_LOAD_FLAGS_IS_USER_TRIGGERED) ==
              (INTERNAL_LOAD_FLAGS_NO_OPENER |
               INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER));
      MOZ_ASSERT(!aLoadState->PostDataStream());
      MOZ_ASSERT(!aLoadState->HeadersStream());
      // If OnLinkClickSync was invoked inside the onload handler, the load
      // type would be set to LOAD_NORMAL_REPLACE; otherwise it should be
      // LOAD_LINK.
      MOZ_ASSERT(aLoadState->LoadType() == LOAD_LINK ||
                 aLoadState->LoadType() == LOAD_NORMAL_REPLACE);
      MOZ_ASSERT(!aLoadState->SHEntry());
      MOZ_ASSERT(aLoadState->FirstParty());  // Windowwatcher will assume this.

      RefPtr<nsDocShellLoadState> loadState =
          new nsDocShellLoadState(aLoadState->URI());

      // Set up our loadinfo so it will do the load as much like we would have
      // as possible.
      loadState->SetReferrerInfo(aLoadState->GetReferrerInfo());
      loadState->SetOriginalURI(aLoadState->OriginalURI());

      Maybe<nsCOMPtr<nsIURI>> resultPrincipalURI;
      aLoadState->GetMaybeResultPrincipalURI(resultPrincipalURI);

      loadState->SetMaybeResultPrincipalURI(resultPrincipalURI);
      loadState->SetKeepResultPrincipalURIIfSet(
          aLoadState->KeepResultPrincipalURIIfSet());
      // LoadReplace will always be false due to asserts above, skip setting
      // it.
      loadState->SetTriggeringPrincipal(aLoadState->TriggeringPrincipal());
      loadState->SetCsp(aLoadState->Csp());
      loadState->SetInheritPrincipal(
          aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL));
      // Explicit principal because we do not want any guesses as to what the
      // principal to inherit is: it should be aTriggeringPrincipal.
      loadState->SetPrincipalIsExplicit(true);
      loadState->SetLoadType(LOAD_LINK);
      loadState->SetForceAllowDataURI(
          aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI));

      rv = win->Open(NS_ConvertUTF8toUTF16(spec),
                     aLoadState->Target(),  // window name
                     EmptyString(),         // Features
                     loadState,
                     true,  // aForceNoOpener
                     getter_AddRefs(newBC));
      MOZ_ASSERT(!newBC);
      return rv;
    }

    rv = win->OpenNoNavigate(NS_ConvertUTF8toUTF16(spec),
                             aLoadState->Target(),  // window name
                             EmptyString(),         // Features
                             getter_AddRefs(newBC));

    // In some cases the Open call doesn't actually result in a new
    // window being opened.  We can detect these cases by examining the
    // document in |newBC|, if any.
    nsCOMPtr<nsPIDOMWindowOuter> piNewWin =
        newBC ? newBC->GetDOMWindow() : nullptr;
    if (piNewWin) {
      RefPtr<Document> newDoc = piNewWin->GetExtantDoc();
      if (!newDoc || newDoc->IsInitialDocument()) {
        aLoadState->SetLoadFlag(INTERNAL_LOAD_FLAGS_FIRST_LOAD);
      }
    }

    if (newBC) {
      targetDocShell = newBC->GetDocShell();
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(targetDocShell, rv);
  //
  // Transfer the load to the target DocShell... Pass empty string as the
  // window target name from to prevent recursive retargeting!
  //
  nsDocShell* docShell = nsDocShell::Cast(targetDocShell);
  // No window target
  aLoadState->SetTarget(EmptyString());
  // No forced download
  aLoadState->SetFileName(VoidString());
  rv = docShell->InternalLoad(aLoadState, aDocShell, aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  // Switch to target tab if we're currently focused window.
  // Take loadDivertedInBackground into account so the behavior would be
  // the same as how the tab first opened.
  bool isTargetActive = false;
  targetDocShell->GetIsActive(&isTargetActive);
  nsCOMPtr<nsPIDOMWindowOuter> domWin = targetDocShell->GetWindow();
  if (mIsActive && !isTargetActive && domWin &&
      !Preferences::GetBool("browser.tabs.loadDivertedInBackground", false)) {
    nsFocusManager::FocusWindow(domWin);
  }

  // Else we ran out of memory, or were a popup and got blocked,
  // or something.

  return rv;
}

bool nsDocShell::IsSameDocumentNavigation(nsDocShellLoadState* aLoadState,
                                          SameDocumentNavigationState& aState) {
  MOZ_ASSERT(aLoadState);
  if (!(aLoadState->LoadType() == LOAD_NORMAL ||
        aLoadState->LoadType() == LOAD_STOP_CONTENT ||
        LOAD_TYPE_HAS_FLAGS(aLoadState->LoadType(),
                            LOAD_FLAGS_REPLACE_HISTORY) ||
        aLoadState->LoadType() == LOAD_HISTORY ||
        aLoadState->LoadType() == LOAD_LINK)) {
    return false;
  }

  nsCOMPtr<nsIURI> currentURI = mCurrentURI;

  nsresult rvURINew = aLoadState->URI()->GetRef(aState.mNewHash);
  if (NS_SUCCEEDED(rvURINew)) {
    rvURINew = aLoadState->URI()->GetHasRef(&aState.mNewURIHasRef);
  }

  if (currentURI && NS_SUCCEEDED(rvURINew)) {
    nsresult rvURIOld = currentURI->GetRef(aState.mCurrentHash);
    if (NS_SUCCEEDED(rvURIOld)) {
      rvURIOld = currentURI->GetHasRef(&aState.mCurrentURIHasRef);
    }
    if (NS_SUCCEEDED(rvURIOld)) {
      if (NS_FAILED(currentURI->EqualsExceptRef(aLoadState->URI(),
                                                &aState.mSameExceptHashes))) {
        aState.mSameExceptHashes = false;
      }
    }
  }

  if (!aState.mSameExceptHashes && sURIFixup && currentURI &&
      NS_SUCCEEDED(rvURINew)) {
    // Maybe aLoadState->URI() came from the exposable form of currentURI?
    nsCOMPtr<nsIURI> currentExposableURI;
    DebugOnly<nsresult> rv = sURIFixup->CreateExposableURI(
        currentURI, getter_AddRefs(currentExposableURI));
    MOZ_ASSERT(NS_SUCCEEDED(rv), "CreateExposableURI should not fail, ever!");
    nsresult rvURIOld = currentExposableURI->GetRef(aState.mCurrentHash);
    if (NS_SUCCEEDED(rvURIOld)) {
      rvURIOld = currentExposableURI->GetHasRef(&aState.mCurrentURIHasRef);
    }
    if (NS_SUCCEEDED(rvURIOld)) {
      if (NS_FAILED(currentExposableURI->EqualsExceptRef(
              aLoadState->URI(), &aState.mSameExceptHashes))) {
        aState.mSameExceptHashes = false;
      }
    }
  }

  if (mOSHE && aLoadState->SHEntry()) {
    // We're doing a history load.

    mOSHE->SharesDocumentWith(aLoadState->SHEntry(),
                              &aState.mHistoryNavBetweenSameDoc);

#ifdef DEBUG
    if (aState.mHistoryNavBetweenSameDoc) {
      nsCOMPtr<nsIInputStream> currentPostData = mOSHE->GetPostData();
      NS_ASSERTION(currentPostData == aLoadState->PostDataStream(),
                   "Different POST data for entries for the same page?");
    }
#endif
  }

  // A same document navigation happens when we navigate between two SHEntries
  // for the same document. We do a same document navigation under two
  // circumstances. Either
  //
  //  a) we're navigating between two different SHEntries which share a
  //     document, or
  //
  //  b) we're navigating to a new shentry whose URI differs from the
  //     current URI only in its hash, the new hash is non-empty, and
  //     we're not doing a POST.
  //
  // The restriction that the SHEntries in (a) must be different ensures
  // that history.go(0) and the like trigger full refreshes, rather than
  // same document navigations.
  bool doSameDocumentNavigation =
      (aState.mHistoryNavBetweenSameDoc && mOSHE != aLoadState->SHEntry()) ||
      (!aLoadState->SHEntry() && !aLoadState->PostDataStream() &&
       aState.mSameExceptHashes && aState.mNewURIHasRef);

  return doSameDocumentNavigation;
}

nsresult nsDocShell::HandleSameDocumentNavigation(
    nsDocShellLoadState* aLoadState, SameDocumentNavigationState& aState) {
#ifdef DEBUG
  SameDocumentNavigationState state;
  MOZ_ASSERT(IsSameDocumentNavigation(aLoadState, state));
#endif

  nsCOMPtr<nsIURI> currentURI = mCurrentURI;

  // Save the position of the scrollers.
  nsPoint scrollPos = GetCurScrollPos();

  // Reset mLoadType to its original value once we exit this block, because this
  // same document navigation might have started after a normal, network load,
  // and we don't want to clobber its load type. See bug 737307.
  AutoRestore<uint32_t> loadTypeResetter(mLoadType);

  // If a non-same-document-navigation (i.e., a network load) is pending, make
  // this a replacement load, so that we don't add a SHEntry here and the
  // network load goes into the SHEntry it expects to.
  if (JustStartedNetworkLoad() && (aLoadState->LoadType() & LOAD_CMD_NORMAL)) {
    mLoadType = LOAD_NORMAL_REPLACE;
  } else {
    mLoadType = aLoadState->LoadType();
  }

  mURIResultedInDocument = true;

  nsCOMPtr<nsISHEntry> oldLSHE = mLSHE;

  // we need to assign aLoadState->SHEntry() to mLSHE right here, so that on
  // History loads, SetCurrentURI() called from OnNewURI() will send proper
  // onLocationChange() notifications to the browser to update back/forward
  // buttons.
  SetHistoryEntry(&mLSHE, aLoadState->SHEntry());

  // Set the doc's URI according to the new history entry's URI.
  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  doc->SetDocumentURI(aLoadState->URI());

  /* This is a anchor traversal within the same page.
   * call OnNewURI() so that, this traversal will be
   * recorded in session and global history.
   */
  nsCOMPtr<nsIPrincipal> newURITriggeringPrincipal, newURIPrincipalToInherit,
      newURIStoragePrincipalToInherit;
  nsCOMPtr<nsIContentSecurityPolicy> newCsp;
  if (mOSHE) {
    newURITriggeringPrincipal = mOSHE->GetTriggeringPrincipal();
    newURIPrincipalToInherit = mOSHE->GetPrincipalToInherit();
    newURIStoragePrincipalToInherit = mOSHE->GetStoragePrincipalToInherit();
    newCsp = mOSHE->GetCsp();
  } else {
    newURITriggeringPrincipal = aLoadState->TriggeringPrincipal();
    newURIPrincipalToInherit = doc->NodePrincipal();
    newURIStoragePrincipalToInherit = doc->IntrinsicStoragePrincipal();
    newCsp = doc->GetCsp();
  }
  // Pass true for aCloneSHChildren, since we're not
  // changing documents here, so all of our subframes are
  // still relevant to the new session history entry.
  //
  // It also makes OnNewURI(...) set LOCATION_CHANGE_SAME_DOCUMENT
  // flag on firing onLocationChange(...).
  // Anyway, aCloneSHChildren param is simply reflecting
  // doSameDocumentNavigation in this scope.
  OnNewURI(aLoadState->URI(), nullptr, newURITriggeringPrincipal,
           newURIPrincipalToInherit, newURIStoragePrincipalToInherit, mLoadType,
           newCsp, true, true, true);

  nsCOMPtr<nsIInputStream> postData;
  uint32_t cacheKey = 0;

  bool scrollRestorationIsManual = false;
  if (mOSHE) {
    /* save current position of scroller(s) (bug 59774) */
    mOSHE->SetScrollPosition(scrollPos.x, scrollPos.y);
    scrollRestorationIsManual = mOSHE->GetScrollRestorationIsManual();
    // Get the postdata and page ident from the current page, if
    // the new load is being done via normal means.  Note that
    // "normal means" can be checked for just by checking for
    // LOAD_CMD_NORMAL, given the loadType and allowScroll check
    // above -- it filters out some LOAD_CMD_NORMAL cases that we
    // wouldn't want here.
    if (aLoadState->LoadType() & LOAD_CMD_NORMAL) {
      postData = mOSHE->GetPostData();
      cacheKey = mOSHE->GetCacheKey();

      // Link our new SHEntry to the old SHEntry's back/forward
      // cache data, since the two SHEntries correspond to the
      // same document.
      if (mLSHE) {
        if (!aLoadState->SHEntry()) {
          // If we're not doing a history load, scroll restoration
          // should be inherited from the previous session history entry.
          mLSHE->SetScrollRestorationIsManual(scrollRestorationIsManual);
        }
        mLSHE->AdoptBFCacheEntry(mOSHE);
      }
    }
  }

  // If we're doing a history load, use its scroll restoration state.
  if (aLoadState->SHEntry()) {
    scrollRestorationIsManual =
        aLoadState->SHEntry()->GetScrollRestorationIsManual();
  }

  /* Assign mLSHE to mOSHE. This will either be a new entry created
   * by OnNewURI() for normal loads or aLoadState->SHEntry() for history
   * loads.
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
    if (cacheKey != 0) {
      mOSHE->SetCacheKey(cacheKey);
    }
  }

  /* Restore the original LSHE if we were loading something
   * while same document navigation was initiated.
   */
  SetHistoryEntry(&mLSHE, oldLSHE);
  /* Set the title for the SH entry for this target url. so that
   * SH menus in go/back/forward buttons won't be empty for this.
   */
  if (mSessionHistory) {
    int32_t index = mSessionHistory->Index();
    nsCOMPtr<nsISHEntry> shEntry;
    mSessionHistory->LegacySHistory()->GetEntryAtIndex(index,
                                                       getter_AddRefs(shEntry));
    NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);
    shEntry->SetTitle(mTitle);
  }

  /* Set the title for the Global History entry for this anchor url.
   */
  UpdateGlobalHistoryTitle(aLoadState->URI());

  SetDocCurrentStateObj(mOSHE);

  // Inform the favicon service that the favicon for oldURI also
  // applies to aLoadState->URI().
  CopyFavicon(currentURI, aLoadState->URI(), doc->NodePrincipal(),
              UsePrivateBrowsing());

  RefPtr<nsGlobalWindowOuter> scriptGlobal = mScriptGlobal;
  RefPtr<nsGlobalWindowInner> win =
      scriptGlobal ? scriptGlobal->GetCurrentInnerWindowInternal() : nullptr;

  // ScrollToAnchor doesn't necessarily cause us to scroll the window;
  // the function decides whether a scroll is appropriate based on the
  // arguments it receives.  But even if we don't end up scrolling,
  // ScrollToAnchor performs other important tasks, such as informing
  // the presShell that we have a new hash.  See bug 680257.
  nsresult rv = ScrollToAnchor(aState.mCurrentURIHasRef, aState.mNewURIHasRef,
                               aState.mNewHash, aLoadState->LoadType());
  NS_ENSURE_SUCCESS(rv, rv);

  /* restore previous position of scroller(s), if we're moving
   * back in history (bug 59774)
   */
  nscoord bx = 0;
  nscoord by = 0;
  bool needsScrollPosUpdate = false;
  if (mOSHE &&
      (aLoadState->LoadType() == LOAD_HISTORY ||
       aLoadState->LoadType() == LOAD_RELOAD_NORMAL) &&
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
    bool doHashchange = aState.mSameExceptHashes &&
                        (aState.mCurrentURIHasRef != aState.mNewURIHasRef ||
                         !aState.mCurrentHash.Equals(aState.mNewHash));

    if (aState.mHistoryNavBetweenSameDoc || doHashchange) {
      win->DispatchSyncPopState();
    }

    if (needsScrollPosUpdate && win->HasActiveDocument()) {
      SetCurScrollPosEx(bx, by);
    }

    if (doHashchange) {
      // Note that currentURI hasn't changed because it's on the
      // stack, so we can just use it directly as the old URI.
      win->DispatchAsyncHashchange(currentURI, aLoadState->URI());
    }
  }

  return NS_OK;
}

nsresult nsDocShell::InternalLoad(nsDocShellLoadState* aLoadState,
                                  nsIDocShell** aDocShell,
                                  nsIRequest** aRequest) {
  MOZ_ASSERT(aLoadState, "need a load state!");
  MOZ_ASSERT(aLoadState->TriggeringPrincipal(),
             "need a valid TriggeringPrincipal");

  if (mUseStrictSecurityChecks && !aLoadState->TriggeringPrincipal()) {
    return NS_ERROR_FAILURE;
  }

  mOriginalUriString.Truncate();

  MOZ_LOG(gDocShellLeakLog, LogLevel::Debug,
          ("DOCSHELL %p InternalLoad %s\n", this,
           aLoadState->URI()->GetSpecOrDefault().get()));

  // Initialize aDocShell/aRequest
  if (aDocShell) {
    *aDocShell = nullptr;
  }
  if (aRequest) {
    *aRequest = nullptr;
  }

  NS_ENSURE_TRUE(IsValidLoadType(aLoadState->LoadType()), NS_ERROR_INVALID_ARG);

  // Cancel loads coming from Docshells that are being destroyed.
  if (mIsBeingDestroyed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // If we have a target to move to, do that now.
  if (!aLoadState->Target().IsEmpty()) {
    return PerformRetargeting(aLoadState, aDocShell, aRequest);
  }

  // If we don't have a target, we're loading into ourselves, and our load
  // delegate may want to intercept that load.
  SameDocumentNavigationState sameDocumentNavigationState;
  bool sameDocument =
      IsSameDocumentNavigation(aLoadState, sameDocumentNavigationState);
  // LoadDelegate has already had chance to delegate loads which ended up to
  // session history, so no need to re-delegate here, and we don't want fragment
  // navigations to go through load delegate.
  if (!sameDocument && !(aLoadState->LoadType() & LOAD_CMD_HISTORY)) {
    bool handled;
    rv = MaybeHandleLoadDelegate(
        aLoadState, nsIBrowserDOMWindow::OPEN_CURRENTWINDOW, &handled);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (handled) {
      return NS_OK;
    }
  }

  // If a source docshell has been passed, check to see if we are sandboxed
  // from it as the result of an iframe or CSP sandbox.
  if (aLoadState->SourceDocShell() &&
      aLoadState->SourceDocShell()->IsSandboxedFrom(mBrowsingContext)) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  NS_ENSURE_STATE(!HasUnloadedParent());

  rv = CheckLoadingPermissions();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mFiredUnloadEvent) {
    if (IsOKToLoadURI(aLoadState->URI())) {
      MOZ_ASSERT(aLoadState->Target().IsEmpty(),
                 "Shouldn't have a window target here!");

      // If this is a replace load, make whatever load triggered
      // the unload event also a replace load, so we don't
      // create extra history entries.
      if (LOAD_TYPE_HAS_FLAGS(aLoadState->LoadType(),
                              LOAD_FLAGS_REPLACE_HISTORY)) {
        mLoadType = LOAD_NORMAL_REPLACE;
      }

      // Do this asynchronously
      nsCOMPtr<nsIRunnable> ev = new InternalLoadEvent(this, aLoadState);
      return DispatchToTabGroup(TaskCategory::Other, ev.forget());
    }

    // Just ignore this load attempt
    return NS_OK;
  }

  // If we are loading a URI that should inherit a security context (basically
  // javascript: at this point), and the caller has said that principal
  // inheritance is allowed, there are a few possible cases:
  //
  // 1) We are provided with the principal to inherit. In that case, we just use
  //    it.
  //
  // 2) The load is coming from some other application. In this case we don't
  //    want to inherit from whatever document we have loaded now, since the
  //    load is unrelated to it.
  //
  // 3) It's a load from our application, but does not provide an explicit
  //    principal to inherit. In that case, we want to inherit the principal of
  //    our current document, or of our parent document (if any) if we don't
  //    have a current document.
  {
    bool inherits;

    if (aLoadState->LoadType() != LOAD_NORMAL_EXTERNAL &&
        !aLoadState->PrincipalToInherit() &&
        (aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_INHERIT_PRINCIPAL)) &&
        NS_SUCCEEDED(nsContentUtils::URIInheritsSecurityContext(
            aLoadState->URI(), &inherits)) &&
        inherits) {
      aLoadState->SetPrincipalToInherit(GetInheritedPrincipal(true));
    }
    // If principalToInherit is still null (e.g. if some of the conditions of
    // were not satisfied), then no inheritance of any sort will happen: the
    // load will just get a principal based on the URI being loaded.
  }

  // If this docshell is owned by a frameloader, make sure to cancel
  // possible frameloader initialization before loading a new page.
  nsCOMPtr<nsIDocShellTreeItem> parent = GetInProcessParentDocshell();
  if (parent) {
    RefPtr<Document> doc = parent->GetDocument();
    if (doc) {
      doc->TryCancelFrameLoaderInitialization(this);
    }
  }

  bool loadFromExternal = false;

  // Before going any further vet loads initiated by external programs.
  if (aLoadState->LoadType() == LOAD_NORMAL_EXTERNAL) {
    loadFromExternal = true;
    // Disallow external chrome: loads targetted at content windows
    if (SchemeIsChrome(aLoadState->URI())) {
      NS_WARNING("blocked external chrome: url -- use '--chrome' option");
      return NS_ERROR_FAILURE;
    }

    // clear the decks to prevent context bleed-through (bug 298255)
    rv = CreateAboutBlankContentViewer(nullptr, nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    // reset loadType so we don't have to add lots of tests for
    // LOAD_NORMAL_EXTERNAL after this point
    aLoadState->SetLoadType(LOAD_NORMAL);
  }

  mAllowKeywordFixup =
      aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP);
  mURIResultedInDocument = false;  // reset the clock...

  // See if this is actually a load between two history entries for the same
  // document. If the process fails, or if we successfully navigate within the
  // same document, return.
  if (sameDocument) {
    return HandleSameDocumentNavigation(aLoadState,
                                        sameDocumentNavigationState);
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
  bool isJavaScript = SchemeIsJavascript(aLoadState->URI());

  if (!isJavaScript) {
    toBeReset = MaybeInitTiming();
  }
  bool isNotDownload = aLoadState->FileName().IsVoid();
  if (mTiming && isNotDownload) {
    mTiming->NotifyBeforeUnload();
  }
  // Check if the page doesn't want to be unloaded. The javascript:
  // protocol handler deals with this for javascript: URLs.
  if (!isJavaScript && isNotDownload && mContentViewer) {
    bool okToUnload;
    rv = mContentViewer->PermitUnload(&okToUnload);

    if (NS_SUCCEEDED(rv) && !okToUnload) {
      // The user chose not to unload the page, interrupt the
      // load.
      MaybeResetInitTiming(toBeReset);
      return NS_OK;
    }
  }

  if (mTiming && isNotDownload) {
    mTiming->NotifyUnloadAccepted(mCurrentURI);
  }

  // Check if the webbrowser chrome wants the load to proceed; this can be
  // used to cancel attempts to load URIs in the wrong process.
  nsCOMPtr<nsIWebBrowserChrome3> browserChrome3 = do_GetInterface(mTreeOwner);
  if (browserChrome3) {
    bool shouldLoad;
    rv = browserChrome3->ShouldLoadURI(
        this, aLoadState->URI(), aLoadState->GetReferrerInfo(),
        !!aLoadState->PostDataStream(), aLoadState->TriggeringPrincipal(),
        aLoadState->Csp(), &shouldLoad);
    if (NS_SUCCEEDED(rv) && !shouldLoad) {
      return NS_OK;
    }
  }

  // In e10s, in the parent process, we refuse to load anything other than
  // "safe" resources that we ship or trust enough to give "special" URLs.
  if (XRE_IsE10sParentProcess()) {
    nsCOMPtr<nsIURI> uri = aLoadState->URI();
    do {
      bool canLoadInParent = false;
      if (NS_SUCCEEDED(NS_URIChainHasFlags(
              uri, nsIProtocolHandler::URI_IS_UI_RESOURCE, &canLoadInParent)) &&
          canLoadInParent) {
        // We allow UI resources.
        break;
      }
      // For about: and extension-based URIs, which don't get
      // URI_IS_UI_RESOURCE, first remove layers of view-source:, if present.
      while (uri && uri->SchemeIs("view-source")) {
        nsCOMPtr<nsINestedURI> nested = do_QueryInterface(uri);
        if (nested) {
          nested->GetInnerURI(getter_AddRefs(uri));
        } else {
          break;
        }
      }
      // Allow about: URIs, and allow moz-extension ones if we're running
      // extension content in the parent process.
      if (!uri || uri->SchemeIs("about") ||
          (!StaticPrefs::extensions_webextensions_remote() &&
           uri->SchemeIs("moz-extension"))) {
        break;
      }
      nsAutoCString scheme;
      uri->GetScheme(scheme);
      // Allow ext+foo URIs (extension-registered custom protocols). See
      // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/protocol_handlers
      if (StringBeginsWith(scheme, NS_LITERAL_CSTRING("ext+")) &&
          !StaticPrefs::extensions_webextensions_remote()) {
        break;
      }
      // This next bit is... awful. Basically, about:addons used to load the
      // discovery pane remotely. Allow for that, if that's actually the state
      // we're in (which is no longer the default at time of writing, but still
      // tested). https://bugzilla.mozilla.org/show_bug.cgi?id=1565606 covers
      // removing this atrocity.
      nsCOMPtr<nsIWebNavigation> parent(do_QueryInterface(mParent));
      if (parent) {
        nsCOMPtr<nsIURI> parentURL;
        parent->GetCurrentURI(getter_AddRefs(parentURL));
        if (parentURL &&
            parentURL->GetSpecOrDefault().EqualsLiteral("about:addons") &&
            (!Preferences::GetBool("extensions.htmlaboutaddons.enabled",
                                   true) ||
             !Preferences::GetBool(
                 "extensions.htmlaboutaddons.discover.enabled", true))) {
          nsCString discoveryURLString;
          Preferences::GetCString("extensions.webservice.discoverURL",
                                  discoveryURLString);
          nsCOMPtr<nsIURI> discoveryURL;
          NS_NewURI(getter_AddRefs(discoveryURL), discoveryURLString);

          nsAutoCString discoveryPrePath;
          if (discoveryURL) {
            discoveryURL->GetPrePath(discoveryPrePath);
          }

          nsAutoCString requestedPrePath;
          uri->GetPrePath(requestedPrePath);
          // So allow the discovery path to load inside about:addons.
          if (discoveryPrePath.Equals(requestedPrePath)) {
            break;
          }
        }
      }
      // Final exception for some legacy automated tests:
      if (xpc::IsInAutomation() &&
          Preferences::GetBool("security.allow_unsafe_parent_loads", false)) {
        break;
      }
      return NS_ERROR_FAILURE;
    } while (0);
  }

  // Whenever a top-level browsing context is navigated, the user agent MUST
  // lock the orientation of the document to the document's default
  // orientation. We don't explicitly check for a top-level browsing context
  // here because orientation is only set on top-level browsing contexts.
  if (OrientationLock() != hal::eScreenOrientation_None) {
#ifdef DEBUG
    nsCOMPtr<nsIDocShellTreeItem> parent;
    GetInProcessSameTypeParent(getter_AddRefs(parent));
    MOZ_ASSERT(!parent);
#endif
    SetOrientationLock(hal::eScreenOrientation_None);
    if (mIsActive) {
      ScreenOrientation::UpdateActiveOrientationLock(
          hal::eScreenOrientation_None);
    }
  }

  // Check for saving the presentation here, before calling Stop().
  // This is necessary so that we can catch any pending requests.
  // Since the new request has not been created yet, we pass null for the
  // new request parameter.
  // Also pass nullptr for the document, since it doesn't affect the return
  // value for our purposes here.
  bool savePresentation =
      CanSavePresentation(aLoadState->LoadType(), nullptr, nullptr);

  // Don't stop current network activity for javascript: URL's since
  // they might not result in any data, and thus nothing should be
  // stopped in those cases. In the case where they do result in
  // data, the javascript: URL channel takes care of stopping
  // current network activity.
  if (!isJavaScript && isNotDownload) {
    // Stop any current network activity.
    // Also stop content if this is a zombie doc. otherwise
    // the onload will be delayed by other loads initiated in the
    // background by the first document that
    // didn't fully load before the next load was initiated.
    // If not a zombie, don't stop content until data
    // starts arriving from the new URI...

    if ((mContentViewer && mContentViewer->GetPreviousViewer()) ||
        LOAD_TYPE_HAS_FLAGS(aLoadState->LoadType(), LOAD_FLAGS_STOP_CONTENT)) {
      rv = Stop(nsIWebNavigation::STOP_ALL);
    } else {
      rv = Stop(nsIWebNavigation::STOP_NETWORK);
    }

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mLoadType = aLoadState->LoadType();

  // aLoadState->SHEntry() should be assigned to mLSHE, only after Stop() has
  // been called. But when loading an error page, do not clear the
  // mLSHE for the real page.
  if (mLoadType != LOAD_ERROR_PAGE) {
    SetHistoryEntry(&mLSHE, aLoadState->SHEntry());
    if (aLoadState->SHEntry()) {
      // We're making history navigation or a reload. Make sure our history ID
      // points to the same ID as SHEntry's docshell ID.
      mHistoryID = aLoadState->SHEntry()->DocshellID();
    }
  }

  mSavingOldViewer = savePresentation;

  // If we have a saved content viewer in history, restore and show it now.
  if (aLoadState->SHEntry() && (mLoadType & LOAD_CMD_HISTORY)) {
    // https://html.spec.whatwg.org/#history-traversal:
    // To traverse the history
    // "If entry has a different Document object than the current entry, then
    // run the following substeps: Remove any tasks queued by the history
    // traversal task source..."
    // Same document object case was handled already above with
    // HandleSameDocumentNavigation call.
    RefPtr<ChildSHistory> shistory = GetRootSessionHistory();
    if (shistory) {
      shistory->RemovePendingHistoryNavigations();
    }

    // It's possible that the previous viewer of mContentViewer is the
    // viewer that will end up in aLoadState->SHEntry() when it gets closed.  If
    // that's the case, we need to go ahead and force it into its shentry so we
    // can restore it.
    if (mContentViewer) {
      nsCOMPtr<nsIContentViewer> prevViewer =
          mContentViewer->GetPreviousViewer();
      if (prevViewer) {
#ifdef DEBUG
        nsCOMPtr<nsIContentViewer> prevPrevViewer =
            prevViewer->GetPreviousViewer();
        NS_ASSERTION(!prevPrevViewer, "Should never have viewer chain here");
#endif
        nsCOMPtr<nsISHEntry> viewerEntry;
        prevViewer->GetHistoryEntry(getter_AddRefs(viewerEntry));
        if (viewerEntry == aLoadState->SHEntry()) {
          // Make sure this viewer ends up in the right place
          mContentViewer->SetPreviousViewer(nullptr);
          prevViewer->Destroy();
        }
      }
    }
    nsCOMPtr<nsISHEntry> oldEntry = mOSHE;
    bool restoring;
    rv = RestorePresentation(aLoadState->SHEntry(), &restoring);
    if (restoring) {
      Telemetry::Accumulate(Telemetry::BFCACHE_PAGE_RESTORED, true);
      return rv;
    }
    Telemetry::Accumulate(Telemetry::BFCACHE_PAGE_RESTORED, false);

    // We failed to restore the presentation, so clean up.
    // Both the old and new history entries could potentially be in
    // an inconsistent state.
    if (NS_FAILED(rv)) {
      if (oldEntry) {
        oldEntry->SyncPresentationState();
      }

      aLoadState->SHEntry()->SyncPresentationState();
    }
  }

  bool isTopLevelDoc =
      mItemType == typeContent && (!IsFrame() || GetIsMozBrowser());

  OriginAttributes attrs = GetOriginAttributes();
  attrs.SetFirstPartyDomain(isTopLevelDoc, aLoadState->URI());

  PredictorLearn(aLoadState->URI(), nullptr,
                 nsINetworkPredictor::LEARN_LOAD_TOPLEVEL, attrs);
  PredictorPredict(aLoadState->URI(), nullptr,
                   nsINetworkPredictor::PREDICT_LOAD, attrs, nullptr);

  nsCOMPtr<nsIRequest> req;
  rv = DoURILoad(aLoadState, loadFromExternal, aDocShell, getter_AddRefs(req));
  if (req && aRequest) {
    NS_ADDREF(*aRequest = req);
  }

  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIChannel> chan(do_QueryInterface(req));
    if (DisplayLoadError(rv, aLoadState->URI(), nullptr, chan) &&
        aLoadState->HasLoadFlags(LOAD_FLAGS_ERROR_LOAD_CHANGES_RV)) {
      return NS_ERROR_LOAD_SHOWED_ERRORPAGE;
    }

    // We won't report any error if this is an unknown protocol error. The
    // reason behind this is that it will allow enumeration of external
    // protocols if we report an error for each unknown protocol.
    if (NS_ERROR_UNKNOWN_PROTOCOL == rv) {
      return NS_OK;
    }
  }

  return rv;
}

nsIPrincipal* nsDocShell::GetInheritedPrincipal(
    bool aConsiderCurrentDocument, bool aConsiderStoragePrincipal) {
  RefPtr<Document> document;
  bool inheritedFromCurrent = false;

  if (aConsiderCurrentDocument && mContentViewer) {
    document = mContentViewer->GetDocument();
    inheritedFromCurrent = true;
  }

  if (!document) {
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    GetInProcessSameTypeParent(getter_AddRefs(parentItem));
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
    nsIPrincipal* docPrincipal = aConsiderStoragePrincipal
                                     ? document->IntrinsicStoragePrincipal()
                                     : document->NodePrincipal();

    // Don't allow loads in typeContent docShells to inherit the system
    // principal from existing documents.
    if (inheritedFromCurrent && mItemType == typeContent &&
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
static bool IsConsideredSameOriginForUIR(nsIPrincipal* aTriggeringPrincipal,
                                         nsIPrincipal* aResultPrincipal) {
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

  if (!aResultPrincipal->GetIsContentPrincipal()) {
    return false;
  }

  nsCOMPtr<nsIURI> resultURI = aResultPrincipal->GetURI();

  // We know this is a content principal, and content principals require valid
  // URIs, so we shouldn't need to check non-null here.
  if (!SchemeIsHTTP(resultURI)) {
    return false;
  }

  nsresult rv;
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
      BasePrincipal::CreateContentPrincipal(tmpResultURI, tmpOA);

  return aTriggeringPrincipal->Equals(tmpResultPrincipal);
}

nsresult nsDocShell::DoURILoad(nsDocShellLoadState* aLoadState,
                               bool aLoadFromExternal, nsIDocShell** aDocShell,
                               nsIRequest** aRequest) {
  // Double-check that we're still around to load this URI.
  if (mIsBeingDestroyed) {
    // Return NS_OK despite not doing anything to avoid throwing exceptions
    // from nsLocation::SetHref if the unload handler of the existing page
    // tears us down.
    return NS_OK;
  }

  nsCOMPtr<nsIURILoader> uriLoader = components::URILoader::Service();
  if (NS_WARN_IF(!uriLoader)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv;
  uint32_t contentPolicyType = DetermineContentType();

  if (IsFrame()) {
    MOZ_ASSERT(contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_IFRAME ||
                   contentPolicyType == nsIContentPolicy::TYPE_INTERNAL_FRAME,
               "DoURILoad thinks this is a frame and InternalLoad does not");

    if (StaticPrefs::dom_block_external_protocol_in_iframes()) {
      // Only allow URLs able to return data in iframes.
      bool doesNotReturnData = false;
      NS_URIChainHasFlags(aLoadState->URI(),
                          nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                          &doesNotReturnData);
      if (doesNotReturnData) {
        bool popupBlocked = true;

        // Let's consider external protocols as popups and let's check if the
        // page is allowed to open them without abuse regardless of allowed
        // events
        if (PopupBlocker::GetPopupControlState() <= PopupBlocker::openBlocked) {
          nsCOMPtr<nsINode> loadingNode =
              mScriptGlobal->GetFrameElementInternal();
          popupBlocked = !PopupBlocker::TryUsePopupOpeningToken(
              loadingNode ? loadingNode->NodePrincipal() : nullptr);
        } else if (mIsActive &&
                   PopupBlocker::ConsumeTimerTokenForExternalProtocolIframe()) {
          popupBlocked = false;
        } else {
          nsCOMPtr<nsINode> loadingNode =
              mScriptGlobal->GetFrameElementInternal();
          if (loadingNode) {
            popupBlocked = !PopupBlocker::CanShowPopupByPermission(
                loadingNode->NodePrincipal());
          }
        }

        // No error must be returned when iframes are blocked.
        if (popupBlocked) {
          return NS_OK;
        }
      }
    }

    // Only allow view-source scheme in top-level docshells. view-source is
    // the only scheme to which this applies at the moment due to potential
    // timing attacks to read data from cross-origin iframes. If this widens
    // we should add a protocol flag for whether the scheme is allowed in
    // frames and use something like nsNetUtil::NS_URIChainHasFlags.
    nsCOMPtr<nsIURI> tempURI = aLoadState->URI();
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(tempURI);
    while (nestedURI) {
      // view-source should always be an nsINestedURI, loop and check the
      // scheme on this and all inner URIs that are also nested URIs.
      if (SchemeIsViewSource(tempURI)) {
        return NS_ERROR_UNKNOWN_PROTOCOL;
      }
      nestedURI->GetInnerURI(getter_AddRefs(tempURI));
      nestedURI = do_QueryInterface(tempURI);
    }
  } else {
    MOZ_ASSERT(contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT,
               "DoURILoad thinks this is a document and InternalLoad does not");
  }

  // open a channel for the url
  nsCOMPtr<nsIChannel> channel;

  nsAutoString srcdoc;
  bool isSrcdoc = aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_IS_SRCDOC);
  if (isSrcdoc) {
    srcdoc = aLoadState->SrcdocData();
  } else {
    srcdoc = VoidString();
  }

  // If we have a pending channel, use the channel we've already created here.
  // We don't need to set up load flags for our channel, as it has already been
  // created.
  nsCOMPtr<nsIChildChannel> pendingChannel =
      aLoadState->GetPendingRedirectedChannel();
  if (pendingChannel) {
    MOZ_ASSERT(!isSrcdoc, "pending channel for srcdoc load?");

    channel = do_QueryInterface(pendingChannel);
    MOZ_ASSERT(channel, "nsIChildChannel isn't a nsIChannel?");

    // If we have a request outparameter, shove our channel into it.
    if (aRequest) {
      nsCOMPtr<nsIRequest> outRequest = channel;
      outRequest.forget(aRequest);
    }

    rv = OpenInitializedChannel(channel, uriLoader,
                                nsIURILoader::REDIRECTED_CHANNEL);

    // If the channel load failed, we failed and nsIWebProgress just ain't
    // gonna happen.
    if (NS_SUCCEEDED(rv) && aDocShell) {
      nsCOMPtr<nsIDocShell> self = this;
      self.forget(aDocShell);
    }
    return rv;
  }

  // There are two cases we care about:
  // * Top-level load: In this case, loadingNode is null, but loadingWindow
  //   is our mScriptGlobal. We pass null for loadingPrincipal in this case.
  // * Subframe load: loadingWindow is null, but loadingNode is the frame
  //   element for the load. loadingPrincipal is the NodePrincipal of the
  //   frame element.
  nsCOMPtr<nsINode> loadingNode;
  nsCOMPtr<nsPIDOMWindowOuter> loadingWindow;
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  nsCOMPtr<nsISupports> topLevelLoadingContext;

  if (contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT) {
    loadingNode = nullptr;
    loadingPrincipal = nullptr;
    loadingWindow = mScriptGlobal;
    if (XRE_IsContentProcess()) {
      // In e10s the child process doesn't have access to the element that
      // contains the browsing context (because that element is in the chrome
      // process).
      nsCOMPtr<nsIBrowserChild> browserChild = GetBrowserChild();
      topLevelLoadingContext = ToSupports(browserChild);
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
    loadingNode = mScriptGlobal->GetFrameElementInternal();
    if (loadingNode) {
      // If we have a loading node, then use that as our loadingPrincipal.
      loadingPrincipal = loadingNode->NodePrincipal();
#ifdef DEBUG
      // Get the docshell type for requestingElement.
      RefPtr<Document> requestingDoc = loadingNode->OwnerDoc();
      nsCOMPtr<nsIDocShell> elementDocShell = requestingDoc->GetDocShell();
      // requestingElement docshell type = current docshell type.
      MOZ_ASSERT(
          mItemType == elementDocShell->ItemType(),
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
  MOZ_ASSERT(aLoadState->TriggeringPrincipal(),
             "Need a valid triggeringPrincipal");

  if (mUseStrictSecurityChecks && !aLoadState->TriggeringPrincipal()) {
    return NS_ERROR_FAILURE;
  }

  bool isSandBoxed = mSandboxFlags & SANDBOXED_ORIGIN;

  // We want to inherit aLoadState->PrincipalToInherit() when:
  // 1. ChannelShouldInheritPrincipal returns true.
  // 2. aLoadState->URI() is not data: URI, or data: URI is not
  //    configured as unique opaque origin.
  bool inheritAttrs = false, inheritPrincipal = false;

  if (aLoadState->PrincipalToInherit()) {
    inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
        aLoadState->PrincipalToInherit(), aLoadState->URI(),
        true,  // aInheritForAboutBlank
        isSrcdoc);

    bool isURIUniqueOrigin = nsIOService::IsDataURIUniqueOpaqueOrigin() &&
                             SchemeIsData(aLoadState->URI());
    inheritPrincipal = inheritAttrs && !isURIUniqueOrigin;
  }

  nsLoadFlags loadFlags = mDefaultLoadFlags;
  nsSecurityFlags securityFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;

  if (aLoadState->FirstParty()) {
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

  RefPtr<LoadInfo> loadInfo =
      (contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT)
          ? new LoadInfo(loadingWindow, aLoadState->TriggeringPrincipal(),
                         topLevelLoadingContext, securityFlags)
          : new LoadInfo(loadingPrincipal, aLoadState->TriggeringPrincipal(),
                         loadingNode, securityFlags, contentPolicyType);

  if (aLoadState->PrincipalToInherit()) {
    loadInfo->SetPrincipalToInherit(aLoadState->PrincipalToInherit());
  }
  loadInfo->SetLoadTriggeredFromExternal(aLoadFromExternal);
  loadInfo->SetForceAllowDataURI(
      aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_FORCE_ALLOW_DATA_URI));
  loadInfo->SetOriginalFrameSrcLoad(
      aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_ORIGINAL_FRAME_SRC));

  // We have to do this in case our OriginAttributes are different from the
  // OriginAttributes of the parent document. Or in case there isn't a
  // parent document.
  bool isTopLevelDoc = mItemType == typeContent &&
                       (contentPolicyType == nsIContentPolicy::TYPE_DOCUMENT ||
                        GetIsMozBrowser());

  OriginAttributes attrs;

  // Inherit origin attributes from PrincipalToInherit if inheritAttrs is
  // true. Otherwise we just use the origin attributes from docshell.
  if (inheritAttrs) {
    MOZ_ASSERT(aLoadState->PrincipalToInherit(),
               "We should have PrincipalToInherit here.");
    attrs = aLoadState->PrincipalToInherit()->OriginAttributesRef();
    // If firstPartyIsolation is not enabled, then PrincipalToInherit should
    // have the same origin attributes with docshell.
    MOZ_ASSERT_IF(!OriginAttributes::IsFirstPartyEnabled(),
                  attrs == GetOriginAttributes());
  } else {
    attrs = GetOriginAttributes();
    attrs.SetFirstPartyDomain(isTopLevelDoc, aLoadState->URI());
  }

  rv = loadInfo->SetOriginAttributes(attrs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Document loads should set the reload flag on the channel so that it
  // can be exposed on the service worker FetchEvent.
  rv = loadInfo->SetIsDocshellReload(mLoadType & LOAD_CMD_RELOAD);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLoadState->GetIsFromProcessingFrameAttributes()) {
    loadInfo->SetIsFromProcessingFrameAttributes();
  }

  // Propagate the IsFormSubmission flag to the loadInfo.
  if (aLoadState->IsFormSubmission()) {
    loadInfo->SetIsFormSubmission(true);
  }

  nsIURI* baseURI = aLoadState->BaseURI();
  if (!isSrcdoc) {
    rv = NS_NewChannelInternal(
        getter_AddRefs(channel), aLoadState->URI(), loadInfo,
        nullptr,  // PerformanceStorage
        nullptr,  // loadGroup
        static_cast<nsIInterfaceRequestor*>(this), loadFlags);

    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_UNKNOWN_PROTOCOL) {
        // This is a uri with a protocol scheme we don't know how
        // to handle.  Embedders might still be interested in
        // handling the load, though, so we fire a notification
        // before throwing the load away.
        bool abort = false;
        nsresult rv2 =
            mContentListener->OnStartURIOpen(aLoadState->URI(), &abort);
        if (NS_SUCCEEDED(rv2) && abort) {
          // Hey, they're handling the load for us!  How convenient!
          return NS_OK;
        }
      }
      return rv;
    }

    if (baseURI) {
      nsCOMPtr<nsIViewSourceChannel> vsc = do_QueryInterface(channel);
      if (vsc) {
        rv = vsc->SetBaseURI(baseURI);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }
  } else if (SchemeIsViewSource(aLoadState->URI())) {
    nsViewSourceHandler* vsh = nsViewSourceHandler::GetInstance();
    NS_ENSURE_TRUE(vsh, NS_ERROR_FAILURE);

    rv = vsh->NewSrcdocChannel(aLoadState->URI(), baseURI, srcdoc, loadInfo,
                               getter_AddRefs(channel));
  } else {
    rv = NS_NewInputStreamChannelInternal(
        getter_AddRefs(channel), aLoadState->URI(), srcdoc,
        NS_LITERAL_CSTRING("text/html"), loadInfo, true);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIInputStreamChannel> isc = do_QueryInterface(channel);
    MOZ_ASSERT(isc);
    isc->SetBaseURI(baseURI);
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = aLoadState->Csp();
  if (csp) {
    // Navigational requests that are same origin need to be upgraded in case
    // upgrade-insecure-requests is present.
    bool upgradeInsecureRequests = false;
    csp->GetUpgradeInsecureRequests(&upgradeInsecureRequests);
    if (upgradeInsecureRequests) {
      // only upgrade if the navigation is same origin
      nsCOMPtr<nsIPrincipal> resultPrincipal;
      rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
          channel, getter_AddRefs(resultPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);
      if (IsConsideredSameOriginForUIR(aLoadState->TriggeringPrincipal(),
                                       resultPrincipal)) {
        loadInfo->SetUpgradeInsecureRequests();
      }
    }

    // For document loads we store the CSP that potentially needs to
    // be inherited by the new document, e.g. in case we are loading
    // an opaque origin like a data: URI. The actual inheritance
    // check happens within Document::InitCSP().
    // Please create an actual copy of the CSP (do not share the same
    // reference) otherwise a Meta CSP of an opaque origin will
    // incorrectly be propagated to the embedding document.
    RefPtr<nsCSPContext> cspToInherit = new nsCSPContext();
    cspToInherit->InitFromOther(static_cast<nsCSPContext*>(csp.get()));
    loadInfo->SetCSPToInherit(cspToInherit);
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
        secMan->GetDocShellContentPrincipal(aLoadState->URI(), this,
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

  if (aLoadState->OriginalURI()) {
    channel->SetOriginalURI(aLoadState->OriginalURI());
    // The LOAD_REPLACE flag and its handling here will be removed as part
    // of bug 1319110.  For now preserve its restoration here to not break
    // any code expecting it being set specially on redirected channels.
    // If the flag has originally been set to change result of
    // NS_GetFinalChannelURI it won't have any effect and also won't cause
    // any harm.
    if (aLoadState->LoadReplace()) {
      uint32_t loadFlags;
      channel->GetLoadFlags(&loadFlags);
      NS_ENSURE_SUCCESS(rv, rv);
      channel->SetLoadFlags(loadFlags | nsIChannel::LOAD_REPLACE);
    }
  } else {
    channel->SetOriginalURI(aLoadState->URI());
  }

  nsCOMPtr<nsIURI> rpURI;
  loadInfo->GetResultPrincipalURI(getter_AddRefs(rpURI));
  Maybe<nsCOMPtr<nsIURI>> originalResultPrincipalURI;
  aLoadState->GetMaybeResultPrincipalURI(originalResultPrincipalURI);
  if (originalResultPrincipalURI &&
      (!aLoadState->KeepResultPrincipalURIIfSet() || !rpURI)) {
    // Unconditionally override, we want the replay to be equal to what has
    // been captured.
    loadInfo->SetResultPrincipalURI(originalResultPrincipalURI.ref());
  }

  const nsACString& typeHint = aLoadState->TypeHint();
  if (!typeHint.IsVoid()) {
    channel->SetContentType(typeHint);
    mContentTypeHint = typeHint;
  } else {
    mContentTypeHint.Truncate();
  }

  const nsAString& fileName = aLoadState->FileName();
  if (!fileName.IsVoid()) {
    rv = channel->SetContentDisposition(nsIChannel::DISPOSITION_ATTACHMENT);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!fileName.IsEmpty()) {
      rv = channel->SetContentDispositionFilename(fileName);
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
  nsCOMPtr<nsIURI> referrer;
  nsIReferrerInfo* referrerInfo = aLoadState->GetReferrerInfo();
  if (referrerInfo) {
    referrerInfo->GetOriginalReferrer(getter_AddRefs(referrer));
  }
  if (httpChannelInternal) {
    if (aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_FORCE_ALLOW_COOKIES)) {
      rv = httpChannelInternal->SetThirdPartyFlags(
          nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    if (aLoadState->FirstParty()) {
      rv = httpChannelInternal->SetDocumentURI(aLoadState->URI());
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      rv = httpChannelInternal->SetDocumentURI(referrer);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    rv = httpChannelInternal->SetRedirectMode(
        nsIHttpChannelInternal::REDIRECT_MODE_MANUAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  Unused << rv;  // Keep Coverity happy

  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(channel));
  if (props) {
    // save true referrer for those who need it (e.g. xpinstall whitelisting)
    // Currently only http and ftp channels support this.
    props->SetPropertyAsInterface(
        NS_LITERAL_STRING("docshell.internalReferrer"), referrer);
  }

  nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(channel));
  /* Get the cache Key from SH */
  uint32_t cacheKey = 0;
  if (cacheChannel) {
    if (mLSHE) {
      cacheKey = mLSHE->GetCacheKey();
    } else if (mOSHE) {  // for reload cases
      cacheKey = mOSHE->GetCacheKey();
    }
  }

  // figure out if we need to set the post data stream on the channel...
  if (aLoadState->PostDataStream()) {
    nsCOMPtr<nsIFormPOSTActionChannel> postChannel(do_QueryInterface(channel));
    if (postChannel) {
      // XXX it's a bit of a hack to rewind the postdata stream here but
      // it has to be done in case the post data is being reused multiple
      // times.
      nsCOMPtr<nsISeekableStream> postDataSeekable =
          do_QueryInterface(aLoadState->PostDataStream());
      if (postDataSeekable) {
        rv = postDataSeekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // we really need to have a content type associated with this stream!!
      postChannel->SetUploadStream(aLoadState->PostDataStream(), EmptyCString(),
                                   -1);
    }

    /* If there is a valid postdata *and* it is a History Load,
     * set up the cache key on the channel, to retrieve the
     * data *only* from the cache. If it is a normal reload, the
     * cache is free to go to the server for updated postdata.
     */
    if (cacheChannel && cacheKey != 0) {
      if (mLoadType == LOAD_HISTORY ||
          mLoadType == LOAD_RELOAD_CHARSET_CHANGE) {
        cacheChannel->SetCacheKey(cacheKey);
        uint32_t loadFlags;
        if (NS_SUCCEEDED(channel->GetLoadFlags(&loadFlags))) {
          channel->SetLoadFlags(loadFlags |
                                nsICachingChannel::LOAD_ONLY_FROM_CACHE);
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
    if (mLoadType == LOAD_HISTORY || mLoadType == LOAD_RELOAD_NORMAL ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE ||
        mLoadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE) {
      if (cacheChannel && cacheKey != 0) {
        cacheChannel->SetCacheKey(cacheKey);
      }
    }
  }

  if (httpChannel) {
    if (aLoadState->HeadersStream()) {
      rv = AddHeadersToChannel(aLoadState->HeadersStream(), httpChannel);
    }
    // Set the referrer explicitly
    // Referrer is currenly only set for link clicks here.
    if (referrerInfo) {
      rv = httpChannel->SetReferrerInfo(referrerInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  nsCOMPtr<nsIScriptChannel> scriptChannel = do_QueryInterface(channel);
  if (scriptChannel) {
    // Allow execution against our context if the principals match
    scriptChannel->SetExecutionPolicy(nsIScriptChannel::EXECUTE_NORMAL);
  }

  if (aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_FIRST_LOAD)) {
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

  rv = DoChannelLoad(
      channel, uriLoader,
      aLoadState->HasLoadFlags(INTERNAL_LOAD_FLAGS_BYPASS_CLASSIFIER));

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

static nsresult AppendSegmentToString(nsIInputStream* aIn, void* aClosure,
                                      const char* aFromRawSegment,
                                      uint32_t aToOffset, uint32_t aCount,
                                      uint32_t* aWriteCount) {
  // aFromSegment now contains aCount bytes of data.

  nsAutoCString* buf = static_cast<nsAutoCString*>(aClosure);
  buf->Append(aFromRawSegment, aCount);

  // Indicate that we have consumed all of aFromSegment
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult nsDocShell::AddHeadersToChannel(nsIInputStream* aHeadersData,
                                         nsIChannel* aGenericChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aGenericChannel);
  NS_ENSURE_STATE(httpChannel);

  uint32_t numRead;
  nsAutoCString headersString;
  nsresult rv = aHeadersData->ReadSegments(
      AppendSegmentToString, &headersString, UINT32_MAX, &numRead);
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

  MOZ_ASSERT_UNREACHABLE("oops");
  return NS_ERROR_UNEXPECTED;
}

nsresult nsDocShell::DoChannelLoad(nsIChannel* aChannel,
                                   nsIURILoader* aURILoader,
                                   bool aBypassClassifier) {
  // Mark the channel as being a document URI and allow content sniffing...
  nsLoadFlags loadFlags = 0;
  (void)aChannel->GetLoadFlags(&loadFlags);
  loadFlags |=
      nsIChannel::LOAD_DOCUMENT_URI | nsIChannel::LOAD_CALL_CONTENT_SNIFFERS;

  if (SandboxFlagsImplyCookies(mSandboxFlags)) {
    loadFlags |= nsIRequest::LOAD_DOCUMENT_NEEDS_COOKIE;
  }

  if (mSandboxFlags & SANDBOXED_AUXILIARY_NAVIGATION) {
    nsCOMPtr<nsIHttpChannelInternal> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
      httpChannel->SetHasSandboxedAuxiliaryNavigations(true);
    }
  }

  // Load attributes depend on load type...
  switch (mLoadType) {
    case LOAD_HISTORY: {
      // Only send VALIDATE_NEVER if mLSHE's URI was never changed via
      // push/replaceState (bug 669671).
      bool uriModified = false;
      if (mLSHE) {
        uriModified = mLSHE->GetURIWasModified();
      }

      if (!uriModified) {
        loadFlags |= nsIRequest::VALIDATE_NEVER;
      }
      break;
    }

    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
      loadFlags |=
          nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::LOAD_FRESH_CONNECTION;
      MOZ_FALLTHROUGH;

    case LOAD_RELOAD_CHARSET_CHANGE: {
      // Use SetAllowStaleCacheContent (not LOAD_FROM_CACHE flag) since we
      // only want to force cache load for this channel, not the whole
      // loadGroup.
      nsCOMPtr<nsICacheInfoChannel> cachingChannel =
          do_QueryInterface(aChannel);
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
      loadFlags |=
          nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::LOAD_FRESH_CONNECTION;
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

  if (aBypassClassifier) {
    loadFlags |= nsIChannel::LOAD_BYPASS_URL_CLASSIFIER;
  }

  // If the user pressed shift-reload, then do not allow ServiceWorker
  // interception to occur. See step 12.1 of the SW HandleFetch algorithm.
  if (IsForceReloading()) {
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

  return OpenInitializedChannel(aChannel, aURILoader, openFlags);
}

nsresult nsDocShell::OpenInitializedChannel(nsIChannel* aChannel,
                                            nsIURILoader* aURILoader,
                                            uint32_t aOpenFlags) {
  nsresult rv;

  // If anything fails here, make sure to clear our initial ClientSource.
  auto cleanupInitialClient =
      MakeScopeExit([&] { mInitialClientSource.reset(); });

  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

  MaybeCreateInitialClientSource();

  nsCOMPtr<nsILoadInfo> loadInfo;
  aChannel->GetLoadInfo(getter_AddRefs(loadInfo));

  LoadInfo* li = static_cast<LoadInfo*>(loadInfo.get());
  if (loadInfo->GetExternalContentPolicyType() ==
      nsIContentPolicy::TYPE_DOCUMENT) {
    li->UpdateBrowsingContextID(mBrowsingContext->Id());
  } else if (loadInfo->GetExternalContentPolicyType() ==
             nsIContentPolicy::TYPE_SUBDOCUMENT) {
    li->UpdateFrameBrowsingContextID(mBrowsingContext->Id());
  }
  // TODO: more attributes need to be updated on the LoadInfo (bug 1561706)

  // Since we are loading a document we need to make sure the proper reserved
  // and initial client data is stored on the nsILoadInfo.  The
  // ClientChannelHelper does this and ensures that it is propagated properly
  // on redirects.  We pass no reserved client here so that the helper will
  // create the reserved ClientSource if necessary.
  Maybe<ClientInfo> noReservedClient;
  rv = AddClientChannelHelper(aChannel, std::move(noReservedClient),
                              GetInitialClientInfo(),
                              win->EventTargetFor(TaskCategory::Other));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aURILoader->OpenURI(aChannel, aOpenFlags, this);
  NS_ENSURE_SUCCESS(rv, rv);

  // We're about to load a new page and it may take time before necko
  // gives back any data, so main thread might have a chance to process a
  // collector slice
  nsJSContext::MaybeRunNextCollectorSlice(this, JS::GCReason::DOCSHELL);

  // Success.  Keep the initial ClientSource if it exists.
  cleanupInitialClient.release();

  return NS_OK;
}

nsresult nsDocShell::ScrollToAnchor(bool aCurHasRef, bool aNewHasRef,
                                    nsACString& aNewHash, uint32_t aLoadType) {
  if (!mCurrentURI) {
    return NS_OK;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    // If we failed to get the shell, or if there is no shell,
    // nothing left to do here.
    return NS_OK;
  }

  nsIScrollableFrame* rootScroll = presShell->GetRootScrollFrameAsScrollable();
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
    bool scroll = aLoadType != LOAD_HISTORY && aLoadType != LOAD_RELOAD_NORMAL;

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
      rv = presShell->GoToAnchor(uStr, scroll, ScrollFlags::ScrollSmoothAuto);
    }

    if (NS_FAILED(rv)) {
      char* str = ToNewCString(aNewHash);
      if (!str) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsUnescape(str);
      NS_ConvertUTF8toUTF16 utf16Str(str);
      if (!utf16Str.IsEmpty()) {
        rv = presShell->GoToAnchor(utf16Str, scroll,
                                   ScrollFlags::ScrollSmoothAuto);
      }
      free(str);
    }

    // Above will fail if the anchor name is not UTF-8.  Need to
    // convert from document charset to unicode.
    if (NS_FAILED(rv)) {
      // Get a document charset
      NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
      Document* doc = mContentViewer->GetDocument();
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
      presShell->GoToAnchor(uStr, scroll && !uStr.IsEmpty(),
                            ScrollFlags::ScrollSmoothAuto);
    }
  } else {
    // Tell the shell it's at an anchor, without scrolling.
    presShell->GoToAnchor(EmptyString(), false);

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

void nsDocShell::SetupReferrerInfoFromChannel(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (httpChannel) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo = httpChannel->GetReferrerInfo();
    SetReferrerInfo(referrerInfo);
  }
}

bool nsDocShell::OnNewURI(nsIURI* aURI, nsIChannel* aChannel,
                          nsIPrincipal* aTriggeringPrincipal,
                          nsIPrincipal* aPrincipalToInherit,
                          nsIPrincipal* aStoragePrincipalToInherit,
                          uint32_t aLoadType, nsIContentSecurityPolicy* aCsp,
                          bool aFireOnLocationChange, bool aAddToGlobalHistory,
                          bool aCloneSHChildren) {
  MOZ_ASSERT(aURI, "uri is null");
  MOZ_ASSERT(!aChannel || !aTriggeringPrincipal, "Shouldn't have both set");

  MOZ_ASSERT(!aPrincipalToInherit ||
             (aPrincipalToInherit && aTriggeringPrincipal));

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]::OnNewURI(\"%s\", [%s], 0x%x)\n", this,
             aURI->GetSpecOrDefault().get(), chanName.get(), aLoadType));
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
  bool updateGHistory =
      !(aLoadType == LOAD_BYPASS_HISTORY || aLoadType == LOAD_ERROR_PAGE ||
        aLoadType & LOAD_CMD_HISTORY);

  // We don't update session history on reload unless we're loading
  // an iframe in shift-reload case.
  bool updateSHistory =
      updateGHistory && (!(aLoadType & LOAD_CMD_RELOAD) ||
                         (IsForceReloadType(aLoadType) && IsFrame()));

  // Create SH Entry (mLSHE) only if there is a SessionHistory object in the
  // current frame or in the root docshell.
  RefPtr<ChildSHistory> rootSH = mSessionHistory;
  if (!rootSH) {
    // Get the handle to SH from the root docshell
    rootSH = GetRootSessionHistory();
  }
  if (!rootSH) {
    updateSHistory = false;
    updateGHistory = false;  // XXX Why global history too?
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
  if (equalUri && mOSHE &&
      (mLoadType == LOAD_NORMAL || mLoadType == LOAD_LINK ||
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
    uint32_t cacheKey = 0;
    // Get the Cache Key and store it in SH.
    if (cacheChannel) {
      cacheChannel->GetCacheKey(&cacheKey);
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
  // XXX: history.go(0) won't go this path as aLoadType is LOAD_HISTORY in
  // this case. One should re-validate after bug 1331865 fixed.
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
                                aPrincipalToInherit, aStoragePrincipalToInherit,
                                aCsp, aCloneSHChildren, getter_AddRefs(mLSHE));
    }
  } else if (mSessionHistory && mLSHE && mURIResultedInDocument) {
    // Even if we don't add anything to SHistory, ensure the current index
    // points to the same SHEntry as our mLSHE.
    int32_t index = mSessionHistory->LegacySHistory()->GetRequestedIndex();
    if (index == -1) {
      index = mSessionHistory->Index();
    }
    nsCOMPtr<nsISHEntry> currentSH;
    mSessionHistory->LegacySHistory()->GetEntryAtIndex(
        index, getter_AddRefs(currentSH));
    if (currentSH != mLSHE) {
      mSessionHistory->LegacySHistory()->ReplaceEntry(index, mLSHE);
    }
  }

#ifdef MOZ_GECKO_PROFILER
  // We register the page load only if the load updates the history and it's
  // not a refresh. This also registers the iframes in shift-reload case, but
  // it's reasonable to register since we are updating the historyId in that
  // case.
  if (updateSHistory) {
    uint32_t id = 0;
    nsAutoCString spec;
    if (mLSHE) {
      mLSHE->GetID(&id);
    }
    aURI->GetSpec(spec);
    profiler_register_page(mHistoryID, id, spec, IsFrame());
  }
#endif

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

    AddURIVisit(aURI, previousURI, previousFlags, responseStatus);
  }

  // If this was a history load or a refresh, or it was a history load but
  // later changed to LOAD_NORMAL_REPLACE due to redirection, update the index
  // in session history.
  if (rootSH && ((mLoadType & (LOAD_CMD_HISTORY | LOAD_CMD_RELOAD)) ||
                 mLoadType == LOAD_NORMAL_REPLACE)) {
    mPreviousEntryIndex = rootSH->Index();
    rootSH->LegacySHistory()->UpdateIndex();
    mLoadedEntryIndex = rootSH->Index();
    MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
            ("Previous index: %d, Loaded index: %d", mPreviousEntryIndex,
             mLoadedEntryIndex));
  }

  // aCloneSHChildren exactly means "we are not loading a new document".
  uint32_t locationFlags =
      aCloneSHChildren ? uint32_t(LOCATION_CHANGE_SAME_DOCUMENT) : 0;

  bool onLocationChangeNeeded =
      SetCurrentURI(aURI, aChannel, aFireOnLocationChange, locationFlags);
  // Make sure to store the referrer from the channel, if any
  SetupReferrerInfoFromChannel(aChannel);
  return onLocationChangeNeeded;
}

bool nsDocShell::OnLoadingSite(nsIChannel* aChannel, bool aFireOnLocationChange,
                               bool aAddToGlobalHistory) {
  nsCOMPtr<nsIURI> uri;
  // If this a redirect, use the final url (uri)
  // else use the original url
  //
  // Note that this should match what documents do (see Document::Reset).
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, false);

  // Pass false for aCloneSHChildren, since we're loading a new page here.
  return OnNewURI(uri, aChannel, nullptr, nullptr, nullptr, mLoadType, nullptr,
                  aFireOnLocationChange, aAddToGlobalHistory, false);
}

void nsDocShell::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mReferrerInfo = aReferrerInfo;  // This assigment addrefs
}

//*****************************************************************************
// nsDocShell: Session History
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::AddState(JS::Handle<JS::Value> aData, const nsAString& aTitle,
                     const nsAString& aURL, bool aReplace, JSContext* aCx) {
  // Implements History.pushState and History.replaceState

  // Here's what we do, roughly in the order specified by HTML5.  The specific
  // steps we are executing are at
  // <https://html.spec.whatwg.org/multipage/history.html#dom-history-pushstate>
  // and
  // <https://html.spec.whatwg.org/multipage/history.html#url-and-history-update-steps>.
  // This function basically implements #dom-history-pushstate and
  // UpdateURLAndHistory implements #url-and-history-update-steps.
  //
  // A. Serialize aData using structured clone.  This is #dom-history-pushstate
  //    step 5.
  // B. If the third argument is present, #dom-history-pushstate step 7.
  //     7.1. Resolve the url, relative to our document.
  //     7.2. If (a) fails, raise a SECURITY_ERR
  //     7.4. Compare the resulting absolute URL to the document's address.  If
  //          any part of the URLs difer other than the <path>, <query>, and
  //          <fragment> components, raise a SECURITY_ERR and abort.
  // C. If !aReplace, #url-and-history-update-steps steps 2.1-2.3:
  //     Remove from the session history all entries after the current entry,
  //     as we would after a regular navigation, and save the current
  //     entry's scroll position (bug 590573).
  // D. #url-and-history-update-steps step 2.4 or step 3.  As apropriate,
  //    either add a state object entry to the session history after the
  //    current entry with the following properties, or modify the current
  //    session history entry to set
  //      a. cloned data as the state object,
  //      b. if the third argument was present, the absolute URL found in
  //         step 2
  //    Also clear the new history entry's POST data (see bug 580069).
  // E. If aReplace is false (i.e. we're doing a pushState instead of a
  //    replaceState), notify bfcache that we've navigated to a new page.
  // F. If the third argument is present, set the document's current address
  //    to the absolute URL found in step B.  This is
  //    #url-and-history-update-steps step 4.
  //
  // It's important that this function not run arbitrary scripts after step A
  // and before completing step E.  For example, if a script called
  // history.back() before we completed step E, bfcache might destroy an
  // active content viewer.  Since EvictOutOfRangeContentViewers at the end of
  // step E might run script, we can't just put a script blocker around the
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

  RefPtr<Document> document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  // Step A: Serialize aData using structured clone.
  // https://html.spec.whatwg.org/multipage/history.html#dom-history-pushstate
  // step 5.
  nsCOMPtr<nsIStructuredCloneContainer> scContainer;

  // scContainer->Init might cause arbitrary JS to run, and this code might
  // navigate the page we're on, potentially to a different origin! (bug
  // 634834)  To protect against this, we abort if our principal changes due
  // to the InitFromJSVal() call.
  {
    RefPtr<Document> origDocument = GetDocument();
    if (!origDocument) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    nsCOMPtr<nsIPrincipal> origPrincipal = origDocument->NodePrincipal();

    scContainer = new nsStructuredCloneContainer();
    rv = scContainer->InitFromJSVal(aData, aCx);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<Document> newDocument = GetDocument();
    if (!newDocument) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    nsCOMPtr<nsIPrincipal> newPrincipal = newDocument->NodePrincipal();

    bool principalsEqual = false;
    origPrincipal->Equals(newPrincipal, &principalsEqual);
    NS_ENSURE_TRUE(principalsEqual, NS_ERROR_DOM_SECURITY_ERR);
  }

  // Check that the state object isn't too long.
  // Default max length: 2097152 (0x200000) bytes.
  int32_t maxStateObjSize =
      Preferences::GetInt("browser.history.maxStateObjectSize", 2097152);
  if (maxStateObjSize < 0) {
    maxStateObjSize = 0;
  }

  uint64_t scSize;
  rv = scContainer->GetSerializedNBytes(&scSize);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(scSize <= (uint32_t)maxStateObjSize, NS_ERROR_ILLEGAL_VALUE);

  // Step B: Resolve aURL.
  // https://html.spec.whatwg.org/multipage/history.html#dom-history-pushstate
  // step 7.
  bool equalURIs = true;
  nsCOMPtr<nsIURI> currentURI;
  if (sURIFixup && mCurrentURI) {
    rv = sURIFixup->CreateExposableURI(mCurrentURI, getter_AddRefs(currentURI));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    currentURI = mCurrentURI;
  }
  nsCOMPtr<nsIURI> newURI;
  if (aURL.Length() == 0) {
    newURI = currentURI;
  } else {
    // 7.1: Resolve aURL relative to mURI

    nsIURI* docBaseURI = document->GetDocBaseURI();
    if (!docBaseURI) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString spec;
    docBaseURI->GetSpec(spec);

    rv = NS_NewURI(getter_AddRefs(newURI), aURL,
                   document->GetDocumentCharacterSet(), docBaseURI);

    // 7.2: If 2a fails, raise a SECURITY_ERR
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // 7.4 and 7.5: Same-origin check.
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
      bool isPrivateWin =
          document->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId >
          0;
      if (NS_FAILED(secMan->CheckSameOriginURI(currentURI, newURI, true,
                                               isPrivateWin)) ||
          !currentUserPass.Equals(newUserPass)) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
    } else {
      // It's a file:// URI
      nsCOMPtr<nsIPrincipal> principal = document->GetPrincipal();

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

  }  // end of same-origin check

  // Step 8: call "URL and history update steps"
  rv = UpdateURLAndHistory(document, newURI, scContainer, aTitle, aReplace,
                           currentURI, equalURIs);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsDocShell::UpdateURLAndHistory(Document* aDocument, nsIURI* aNewURI,
                                         nsIStructuredCloneContainer* aData,
                                         const nsAString& aTitle, bool aReplace,
                                         nsIURI* aCurrentURI, bool aEqualURIs) {
  // Implements
  // https://html.spec.whatwg.org/multipage/history.html#url-and-history-update-steps

  // Step 2, if aReplace is false: Create a new entry in the session
  // history. This will erase all SHEntries after the new entry and make this
  // entry the current one.  This operation may modify mOSHE, which we need
  // later, so we keep a reference here.
  NS_ENSURE_TRUE(mOSHE || aReplace, NS_ERROR_FAILURE);
  nsCOMPtr<nsISHEntry> oldOSHE = mOSHE;

  mLoadType = LOAD_PUSHSTATE;

  nsCOMPtr<nsISHEntry> newSHEntry;
  if (!aReplace) {
    // Step 2.

    // Step 2.2, "Remove any tasks queued by the history traversal task
    // source..."
    RefPtr<ChildSHistory> shistory = GetRootSessionHistory();
    if (shistory) {
      shistory->RemovePendingHistoryNavigations();
    }

    // Save the current scroll position (bug 590573).  Step 2.3.
    nsPoint scrollPos = GetCurScrollPos();
    mOSHE->SetScrollPosition(scrollPos.x, scrollPos.y);

    bool scrollRestorationIsManual = mOSHE->GetScrollRestorationIsManual();
    nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();

    // Since we're not changing which page we have loaded, pass
    // true for aCloneChildren.
    nsresult rv = AddToSessionHistory(
        aNewURI, nullptr,
        aDocument->NodePrincipal(),  // triggeringPrincipal
        nullptr, nullptr, csp, true, getter_AddRefs(newSHEntry));
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
    mOSHE->GetTitle(title);
    newSHEntry->SetTitle(title);

    // AddToSessionHistory may not modify mOSHE.  In case it doesn't,
    // we'll just set mOSHE here.
    mOSHE = newSHEntry;

#ifdef MOZ_GECKO_PROFILER
    uint32_t id = 0;
    GetOSHEId(&id);
    profiler_register_page(mHistoryID, id, aNewURI->GetSpecOrDefault(),
                           IsFrame());
#endif

  } else {
    // Step 3.
    newSHEntry = mOSHE;

    // Since we're not changing which page we have loaded, pass
    if (!newSHEntry) {
      nsresult rv = AddToSessionHistory(
          aNewURI, nullptr,
          aDocument->NodePrincipal(),  // triggeringPrincipal
          nullptr, nullptr, aDocument->GetCsp(), true,
          getter_AddRefs(newSHEntry));
      NS_ENSURE_SUCCESS(rv, rv);
      mOSHE = newSHEntry;
    }
    newSHEntry->SetURI(aNewURI);
    newSHEntry->SetOriginalURI(aNewURI);
    // Setting the resultPrincipalURI to nullptr is fine here: it will cause
    // NS_GetFinalChannelURI to use the originalURI as the URI, which is aNewURI
    // in our case.  We could also set it to aNewURI, with the same result.
    newSHEntry->SetResultPrincipalURI(nullptr);
    newSHEntry->SetLoadReplace(false);
  }

  // Step 2.4 and 3: Modify new/original session history entry and clear its
  // POST data, if there is any.
  newSHEntry->SetStateData(aData);
  newSHEntry->SetPostData(nullptr);

  // If this push/replaceState changed the document's current URI and the new
  // URI differs from the old URI in more than the hash, or if the old
  // SHEntry's URI was modified in this way by a push/replaceState call
  // set URIWasModified to true for the current SHEntry (bug 669671).
  bool sameExceptHashes = true;
  aNewURI->EqualsExceptRef(aCurrentURI, &sameExceptHashes);
  bool oldURIWasModified = oldOSHE && oldOSHE->GetURIWasModified();
  newSHEntry->SetURIWasModified(!sameExceptHashes || oldURIWasModified);

  // Step E as described at the top of AddState: If aReplace is false,
  // indicating that we're doing a pushState rather than a replaceState, notify
  // bfcache that we've added a page to the history so it can evict content
  // viewers if appropriate. Otherwise call ReplaceEntry so that we notify
  // nsIHistoryListeners that an entry was replaced.  We may not have a root
  // session history if this call is coming from a document.open() in a docshell
  // subtree that disables session history.
  RefPtr<ChildSHistory> rootSH = GetRootSessionHistory();
  if (rootSH) {
    if (!aReplace) {
      int32_t curIndex = rootSH->Index();
      if (curIndex > -1) {
        rootSH->LegacySHistory()->EvictOutOfRangeContentViewers(curIndex);
      }
    } else {
      nsCOMPtr<nsISHEntry> rootSHEntry = nsSHistory::GetRootSHEntry(newSHEntry);

      int32_t index = rootSH->LegacySHistory()->GetIndexOfEntry(rootSHEntry);
      if (index > -1) {
        rootSH->LegacySHistory()->ReplaceEntry(index, rootSHEntry);
      }
    }
  }

  // Step 4: If the document's URI changed, update document's URI and update
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
  //
  // If the docshell is shutting down, don't update the document URI, as we
  // can't load into a docshell that is being destroyed.
  if (!aEqualURIs && !mIsBeingDestroyed) {
    aDocument->SetDocumentURI(aNewURI);
    // We can't trust SetCurrentURI to do always fire locationchange events
    // when we expect it to, so we hack around that by doing it ourselves...
    SetCurrentURI(aNewURI, nullptr, false, LOCATION_CHANGE_SAME_DOCUMENT);
    if (mLoadType != LOAD_ERROR_PAGE) {
      FireDummyOnLocationChange();
    }

    AddURIVisit(aNewURI, aCurrentURI, 0);

    // AddURIVisit doesn't set the title for the new URI in global history,
    // so do that here.
    UpdateGlobalHistoryTitle(aNewURI);

    // Inform the favicon service that our old favicon applies to this new
    // URI.
    CopyFavicon(aCurrentURI, aNewURI, aDocument->NodePrincipal(),
                UsePrivateBrowsing());
  } else {
    FireDummyOnLocationChange();
  }
  aDocument->SetStateObject(aData);

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetCurrentScrollRestorationIsManual(bool* aIsManual) {
  *aIsManual = false;
  if (mOSHE) {
    *aIsManual = mOSHE->GetScrollRestorationIsManual();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetCurrentScrollRestorationIsManual(bool aIsManual) {
  if (mOSHE) {
    mOSHE->SetScrollRestorationIsManual(aIsManual);
  }

  return NS_OK;
}

bool nsDocShell::ShouldAddToSessionHistory(nsIURI* aURI, nsIChannel* aChannel) {
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
      rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
          aChannel, getter_AddRefs(resultPrincipal));
      NS_ENSURE_SUCCESS(rv, false);
      return !nsContentUtils::IsSystemPrincipal(resultPrincipal);
    }
  }

  return true;
}

nsresult nsDocShell::AddToSessionHistory(
    nsIURI* aURI, nsIChannel* aChannel, nsIPrincipal* aTriggeringPrincipal,
    nsIPrincipal* aPrincipalToInherit, nsIPrincipal* aStoragePrincipalToInherit,
    nsIContentSecurityPolicy* aCsp, bool aCloneChildren,
    nsISHEntry** aNewEntry) {
  MOZ_ASSERT(aURI, "uri is null");
  MOZ_ASSERT(!aChannel || !aTriggeringPrincipal, "Shouldn't have both set");

#if defined(DEBUG)
  if (MOZ_LOG_TEST(gDocShellLog, LogLevel::Debug)) {
    nsAutoCString chanName;
    if (aChannel) {
      aChannel->GetName(chanName);
    } else {
      chanName.AssignLiteral("<no channel>");
    }

    MOZ_LOG(gDocShellLog, LogLevel::Debug,
            ("nsDocShell[%p]::AddToSessionHistory(\"%s\", [%s])\n", this,
             aURI->GetSpecOrDefault().get(), chanName.get()));
  }
#endif

  nsresult rv = NS_OK;
  nsCOMPtr<nsISHEntry> entry;

  // Get a handle to the root docshell
  nsCOMPtr<nsIDocShellTreeItem> root;
  GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  /*
   * If this is a LOAD_FLAGS_REPLACE_HISTORY in a subframe, we use
   * the existing SH entry in the page and replace the url and
   * other vitalities.
   */
  if (LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY) &&
      root != static_cast<nsIDocShellTreeItem*>(this)) {
    // This is a subframe
    entry = mOSHE;
    if (entry) {
      int32_t childCount = entry->GetChildCount();
      // Remove all children of this entry
      for (int32_t i = childCount - 1; i >= 0; i--) {
        nsCOMPtr<nsISHEntry> child;
        entry->GetChildAt(i, getter_AddRefs(child));
        entry->RemoveChild(child);
      }
      entry->AbandonBFCacheEntry();
    }
  }

  // Create a new entry if necessary.
  if (!entry) {
    entry = components::SHEntry::Create();

    if (!entry) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Get the post data & referrer
  nsCOMPtr<nsIInputStream> inputStream;
  nsCOMPtr<nsIURI> originalURI;
  nsCOMPtr<nsIURI> resultPrincipalURI;
  bool loadReplace = false;
  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  uint32_t cacheKey = 0;
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> principalToInherit = aPrincipalToInherit;
  nsCOMPtr<nsIPrincipal> storagePrincipalToInherit = aStoragePrincipalToInherit;
  nsCOMPtr<nsIContentSecurityPolicy> csp = aCsp;
  bool expired = false;
  bool discardLayoutState = false;
  nsCOMPtr<nsICacheInfoChannel> cacheChannel;
  if (aChannel) {
    cacheChannel = do_QueryInterface(aChannel);

    /* If there is a caching channel, get the Cache Key and store it
     * in SH.
     */
    if (cacheChannel) {
      cacheChannel->GetCacheKey(&cacheKey);
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
      rv = httpChannel->GetReferrerInfo(getter_AddRefs(referrerInfo));
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      discardLayoutState = ShouldDiscardLayoutState(httpChannel);
    }

    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    if (!triggeringPrincipal) {
      triggeringPrincipal = loadInfo->TriggeringPrincipal();
    }
    if (!csp) {
      csp = loadInfo->GetCspToInherit();
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

    if (!storagePrincipalToInherit) {
      // XXXehsan is it correct to fall back to the principal to inherit in all
      // cases?  For example, what about the cases where we are using the load
      // info's principal to inherit?  Do we need to add a similar concept to
      // load info for storage principal?
      storagePrincipalToInherit = principalToInherit;
    }
  }

  // Title is set in nsDocShell::SetTitle()
  entry->Create(aURI,                 // uri
                EmptyString(),        // Title
                inputStream,          // Post data stream
                nullptr,              // LayoutHistory state
                cacheKey,             // CacheKey
                mContentTypeHint,     // Content-type
                triggeringPrincipal,  // Channel or provided principal
                principalToInherit, storagePrincipalToInherit, csp, mHistoryID,
                mDynamicallyCreated);

  entry->SetOriginalURI(originalURI);
  entry->SetResultPrincipalURI(resultPrincipalURI);
  entry->SetLoadReplace(loadReplace);
  entry->SetReferrerInfo(referrerInfo);
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
      uint32_t cloneID = mOSHE->GetID();
      nsCOMPtr<nsISHEntry> newEntry;
      nsSHistory::CloneAndReplace(mOSHE, this, cloneID, entry, true,
                                  getter_AddRefs(newEntry));
      NS_ASSERTION(entry == newEntry,
                   "The new session history should be in the new entry");
    }

    // This is the root docshell
    bool addToSHistory =
        !LOAD_TYPE_HAS_FLAGS(mLoadType, LOAD_FLAGS_REPLACE_HISTORY);
    if (!addToSHistory) {
      // Replace current entry in session history; If the requested index is
      // valid, it indicates the loading was triggered by a history load, and
      // we should replace the entry at requested index instead.
      int32_t index = mSessionHistory->LegacySHistory()->GetRequestedIndex();
      if (index == -1) {
        index = mSessionHistory->Index();
      }

      // Replace the current entry with the new entry
      if (index >= 0) {
        rv = mSessionHistory->LegacySHistory()->ReplaceEntry(index, entry);
      } else {
        // If we're trying to replace an inexistant shistory entry, append.
        addToSHistory = true;
      }
    }

    if (addToSHistory) {
      // Add to session history
      mPreviousEntryIndex = mSessionHistory->Index();

      bool shouldPersist = ShouldAddToSessionHistory(aURI, aChannel);
      rv = mSessionHistory->LegacySHistory()->AddEntry(entry, shouldPersist);
      mLoadedEntryIndex = mSessionHistory->Index();
      MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
              ("Previous index: %d, Loaded index: %d", mPreviousEntryIndex,
               mLoadedEntryIndex));
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

nsresult nsDocShell::LoadHistoryEntry(nsISHEntry* aEntry, uint32_t aLoadType) {
  if (!IsNavigationAllowed()) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(aEntry, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri = aEntry->GetURI();
  nsCOMPtr<nsIURI> originalURI = aEntry->GetOriginalURI();
  nsCOMPtr<nsIURI> resultPrincipalURI = aEntry->GetResultPrincipalURI();
  bool loadReplace = aEntry->GetLoadReplace();
  nsCOMPtr<nsIInputStream> postData = aEntry->GetPostData();
  nsAutoCString contentType;
  aEntry->GetContentType(contentType);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aEntry->GetTriggeringPrincipal();
  nsCOMPtr<nsIPrincipal> principalToInherit = aEntry->GetPrincipalToInherit();
  nsCOMPtr<nsIPrincipal> storagePrincipalToInherit =
      aEntry->GetStoragePrincipalToInherit();
  nsCOMPtr<nsIContentSecurityPolicy> csp = aEntry->GetCsp();
  nsCOMPtr<nsIReferrerInfo> referrerInfo = aEntry->GetReferrerInfo();

  // Calling CreateAboutBlankContentViewer can set mOSHE to null, and if
  // that's the only thing holding a ref to aEntry that will cause aEntry to
  // die while we're loading it.  So hold a strong ref to aEntry here, just
  // in case.
  nsCOMPtr<nsISHEntry> kungFuDeathGrip(aEntry);
  nsresult rv;
  if (SchemeIsJavascript(uri)) {
    // We're loading a URL that will execute script from inside asyncOpen.
    // Replace the current document with about:blank now to prevent
    // anything from the current document from leaking into any JavaScript
    // code in the URL.
    // Don't cache the presentation if we're going to just reload the
    // current entry. Caching would lead to trying to save the different
    // content viewers in the same nsISHEntry object.
    rv = CreateAboutBlankContentViewer(principalToInherit,
                                       storagePrincipalToInherit, nullptr,
                                       nullptr, aEntry != mOSHE);

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
  nsCOMPtr<nsIURI> baseURI;
  if (aEntry->GetIsSrcdocEntry()) {
    aEntry->GetSrcdocData(srcdoc);
    baseURI = aEntry->GetBaseURI();
    flags |= INTERNAL_LOAD_FLAGS_IS_SRCDOC;
  } else {
    srcdoc = VoidString();
  }

  // If there is no valid triggeringPrincipal, we deny the load
  MOZ_ASSERT(triggeringPrincipal,
             "need a valid triggeringPrincipal to load from history");
  if (!triggeringPrincipal) {
    return NS_ERROR_FAILURE;
  }

  // Passing nullptr as aSourceDocShell gives the same behaviour as before
  // aSourceDocShell was introduced. According to spec we should be passing
  // the source browsing context that was used when the history entry was
  // first created. bug 947716 has been created to address this issue.
  Maybe<nsCOMPtr<nsIURI>> emplacedResultPrincipalURI;
  emplacedResultPrincipalURI.emplace(std::move(resultPrincipalURI));

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(uri);
  loadState->SetReferrerInfo(referrerInfo);
  loadState->SetOriginalURI(originalURI);
  loadState->SetMaybeResultPrincipalURI(emplacedResultPrincipalURI);
  loadState->SetLoadReplace(loadReplace);
  loadState->SetTriggeringPrincipal(triggeringPrincipal);
  loadState->SetPrincipalToInherit(principalToInherit);
  loadState->SetLoadFlags(flags);
  loadState->SetTypeHint(contentType);
  loadState->SetPostDataStream(postData);
  loadState->SetLoadType(aLoadType);
  loadState->SetSHEntry(aEntry);
  loadState->SetFirstParty(true);
  loadState->SetSrcdocData(srcdoc);
  loadState->SetBaseURI(baseURI);
  loadState->SetCsp(csp);

  rv = InternalLoad(loadState,
                    nullptr,   // No nsIDocShell
                    nullptr);  // No nsIRequest
  return rv;
}

nsresult nsDocShell::PersistLayoutHistoryState() {
  nsresult rv = NS_OK;

  if (mOSHE) {
    bool scrollRestorationIsManual = mOSHE->GetScrollRestorationIsManual();
    nsCOMPtr<nsILayoutHistoryState> layoutState;
    if (RefPtr<PresShell> presShell = GetPresShell()) {
      rv = presShell->CaptureHistoryState(getter_AddRefs(layoutState));
    } else if (scrollRestorationIsManual) {
      // Even if we don't have layout anymore, we may want to reset the
      // current scroll state in layout history.
      GetLayoutHistoryState(getter_AddRefs(layoutState));
    }

    if (scrollRestorationIsManual && layoutState) {
      layoutState->ResetScrollState();
    }
  }

  return rv;
}

void nsDocShell::SwapHistoryEntries(nsISHEntry* aOldEntry,
                                    nsISHEntry* aNewEntry) {
  if (aOldEntry == mOSHE) {
    mOSHE = aNewEntry;
  }

  if (aOldEntry == mLSHE) {
    mLSHE = aNewEntry;
  }
}

void nsDocShell::SetHistoryEntry(nsCOMPtr<nsISHEntry>* aPtr,
                                 nsISHEntry* aEntry) {
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
      GetInProcessSameTypeRootTreeItem(getter_AddRefs(rootAsItem));
      nsCOMPtr<nsIDocShell> rootShell = do_QueryInterface(rootAsItem);
      if (rootShell) {  // if we're the root just set it, nothing to swap
        nsSHistory::SwapEntriesData data = {this, newRootEntry};
        nsIDocShell* rootIDocShell = static_cast<nsIDocShell*>(rootShell);
        nsDocShell* rootDocShell = static_cast<nsDocShell*>(rootIDocShell);

#ifdef DEBUG
        nsresult rv =
#endif
            nsSHistory::SetChildHistoryEntry(oldRootEntry, rootDocShell, 0,
                                             &data);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetChildHistoryEntry failed");
      }
    }
  }

  *aPtr = aEntry;
}

already_AddRefed<ChildSHistory> nsDocShell::GetRootSessionHistory() {
  nsCOMPtr<nsIDocShellTreeItem> root;
  nsresult rv = GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  nsCOMPtr<nsIWebNavigation> webnav = do_QueryInterface(root);
  if (!webnav) {
    return nullptr;
  }
  return webnav->GetSessionHistory();
}

nsresult nsDocShell::GetHttpChannel(nsIChannel* aChannel,
                                    nsIHttpChannel** aReturn) {
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

bool nsDocShell::ShouldDiscardLayoutState(nsIHttpChannel* aChannel) {
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
nsDocShell::GetEditor(nsIEditor** aEditor) {
  NS_ENSURE_ARG_POINTER(aEditor);
  RefPtr<HTMLEditor> htmlEditor = GetHTMLEditorInternal();
  htmlEditor.forget(aEditor);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetEditor(nsIEditor* aEditor) {
  HTMLEditor* htmlEditor = aEditor ? aEditor->AsHTMLEditor() : nullptr;
  // If TextEditor comes, throw an error.
  if (aEditor && !htmlEditor) {
    return NS_ERROR_INVALID_ARG;
  }
  return SetHTMLEditorInternal(htmlEditor);
}

HTMLEditor* nsDocShell::GetHTMLEditorInternal() {
  return mEditorData ? mEditorData->GetHTMLEditor() : nullptr;
}

nsresult nsDocShell::SetHTMLEditorInternal(HTMLEditor* aHTMLEditor) {
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
nsDocShell::GetEditable(bool* aEditable) {
  NS_ENSURE_ARG_POINTER(aEditable);
  *aEditable = mEditorData && mEditorData->GetEditable();
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetHasEditingSession(bool* aHasEditingSession) {
  NS_ENSURE_ARG_POINTER(aHasEditingSession);

  if (mEditorData) {
    *aHasEditingSession = !!mEditorData->GetEditingSession();
  } else {
    *aHasEditingSession = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::MakeEditable(bool aInWaitForUriLoad) {
  nsresult rv = EnsureEditorData();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return mEditorData->MakeEditable(aInWaitForUriLoad);
}

bool nsDocShell::ChannelIsPost(nsIChannel* aChannel) {
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (!httpChannel) {
    return false;
  }

  nsAutoCString method;
  Unused << httpChannel->GetRequestMethod(method);
  return method.EqualsLiteral("POST");
}

void nsDocShell::ExtractLastVisit(nsIChannel* aChannel, nsIURI** aURI,
                                  uint32_t* aChannelRedirectFlags) {
  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(aChannel));
  if (!props) {
    return;
  }

  nsresult rv = props->GetPropertyAsInterface(
      NS_LITERAL_STRING("docshell.previousURI"), NS_GET_IID(nsIURI),
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

void nsDocShell::SaveLastVisit(nsIChannel* aChannel, nsIURI* aURI,
                               uint32_t aChannelRedirectFlags) {
  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(aChannel));
  if (!props || !aURI) {
    return;
  }

  props->SetPropertyAsInterface(NS_LITERAL_STRING("docshell.previousURI"),
                                aURI);
  props->SetPropertyAsUint32(NS_LITERAL_STRING("docshell.previousFlags"),
                             aChannelRedirectFlags);
}

void nsDocShell::AddURIVisit(nsIURI* aURI, nsIURI* aPreviousURI,
                             uint32_t aChannelRedirectFlags,
                             uint32_t aResponseStatus) {
  MOZ_ASSERT(aURI, "Visited URI is null!");
  MOZ_ASSERT(mLoadType != LOAD_ERROR_PAGE && mLoadType != LOAD_BYPASS_HISTORY,
             "Do not add error or bypass pages to global history");

  // Only content-type docshells save URI visits.  Also don't do
  // anything here if we're not supposed to use global history.
  if (mItemType != typeContent || !mUseGlobalHistory || UsePrivateBrowsing()) {
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
    } else if (aChannelRedirectFlags &
               nsIChannelEventSink::REDIRECT_PERMANENT) {
      visitURIFlags |= IHistory::REDIRECT_PERMANENT;
    } else {
      MOZ_ASSERT(!aChannelRedirectFlags,
                 "One of REDIRECT_TEMPORARY or REDIRECT_PERMANENT must be set "
                 "if any flags in aChannelRedirectFlags is set.");
    }

    if (aResponseStatus >= 300 && aResponseStatus < 400) {
      visitURIFlags |= IHistory::REDIRECT_SOURCE;
      if (aResponseStatus == 301 || aResponseStatus == 308) {
        visitURIFlags |= IHistory::REDIRECT_SOURCE_PERMANENT;
      }
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

    nsPIDOMWindowOuter* outer = GetWindow();
    nsCOMPtr<nsIWidget> widget = widget::WidgetUtils::DOMWindowToWidget(outer);
    (void)history->VisitURI(widget, aURI, aPreviousURI, visitURIFlags);
  }
}

//*****************************************************************************
// nsDocShell: Helper Routines
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::SetLoadType(uint32_t aLoadType) {
  mLoadType = aLoadType;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLoadType(uint32_t* aLoadType) {
  *aLoadType = mLoadType;
  return NS_OK;
}

nsresult nsDocShell::ConfirmRepost(bool* aRepost) {
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

  AutoTArray<nsString, 1> formatStrings;
  rv = brandBundle->GetStringFromName("brandShortName",
                                      *formatStrings.AppendElement());

  nsAutoString msgString, button0Title;
  if (NS_FAILED(rv)) {  // No brand, use the generic version.
    rv = appBundle->GetStringFromName("confirmRepostPrompt", msgString);
  } else {
    // Brand available - if the app has an override file with formatting, the
    // app name will be included. Without an override, the prompt will look
    // like the generic version.
    rv = appBundle->FormatStringFromName("confirmRepostPrompt", formatStrings,
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
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag =
          do_QueryInterface(prompter)) {
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
      button0Title.get(), nullptr, nullptr, nullptr, &checkState,
      &buttonPressed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aRepost = (buttonPressed == 0);
  return NS_OK;
}

nsresult nsDocShell::GetPromptAndStringBundle(nsIPrompt** aPrompt,
                                              nsIStringBundle** aStringBundle) {
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

nsIScrollableFrame* nsDocShell::GetRootScrollFrame() {
  PresShell* presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);

  return presShell->GetRootScrollFrameAsScrollable();
}

nsresult nsDocShell::EnsureScriptEnvironment() {
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
  mScriptGlobal = nsGlobalWindowOuter::Create(this, mItemType == typeChrome);
  MOZ_ASSERT(mScriptGlobal);

  // Ensure the script object is set up to run script.
  return mScriptGlobal->EnsureScriptEnvironment();
}

nsresult nsDocShell::EnsureEditorData() {
  MOZ_ASSERT(!mIsBeingDestroyed);

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

nsresult nsDocShell::EnsureFind() {
  if (!mFind) {
    mFind = new nsWebBrowserFind();
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

  nsresult rv = findInFrames->SetRootSearchFrame(ourWindow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = findInFrames->SetCurrentSearchFrame(windowToSearch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

bool nsDocShell::IsFrame() { return mIsFrame; }

NS_IMETHODIMP
nsDocShell::IsBeingDestroyed(bool* aDoomed) {
  NS_ENSURE_ARG(aDoomed);
  *aDoomed = mIsBeingDestroyed;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsExecutingOnLoadHandler(bool* aResult) {
  NS_ENSURE_ARG(aResult);
  *aResult = mIsExecutingOnLoadHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetLayoutHistoryState(nsILayoutHistoryState** aLayoutHistoryState) {
  if (mOSHE) {
    nsCOMPtr<nsILayoutHistoryState> state = mOSHE->GetLayoutHistoryState();
    state.forget(aLayoutHistoryState);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetLayoutHistoryState(nsILayoutHistoryState* aLayoutHistoryState) {
  if (mOSHE) {
    mOSHE->SetLayoutHistoryState(aLayoutHistoryState);
  }
  return NS_OK;
}

nsDocShell::InterfaceRequestorProxy::InterfaceRequestorProxy(
    nsIInterfaceRequestor* aRequestor) {
  if (aRequestor) {
    mWeakPtr = do_GetWeakReference(aRequestor);
  }
}

nsDocShell::InterfaceRequestorProxy::~InterfaceRequestorProxy() {
  mWeakPtr = nullptr;
}

NS_IMPL_ISUPPORTS(nsDocShell::InterfaceRequestorProxy, nsIInterfaceRequestor)

NS_IMETHODIMP
nsDocShell::InterfaceRequestorProxy::GetInterface(const nsIID& aIID,
                                                  void** aSink) {
  NS_ENSURE_ARG_POINTER(aSink);
  nsCOMPtr<nsIInterfaceRequestor> ifReq = do_QueryReferent(mWeakPtr);
  if (ifReq) {
    return ifReq->GetInterface(aIID, aSink);
  }
  *aSink = nullptr;
  return NS_NOINTERFACE;
}

//*****************************************************************************
// nsDocShell::nsIAuthPromptProvider
//*****************************************************************************

NS_IMETHODIMP
nsDocShell::GetAuthPrompt(uint32_t aPromptReason, const nsIID& aIID,
                          void** aResult) {
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
nsDocShell::GetAssociatedWindow(mozIDOMWindowProxy** aWindow) {
  CallGetInterface(this, aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopWindow(mozIDOMWindowProxy** aWindow) {
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (win) {
    win = win->GetInProcessTop();
  }
  win.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetTopFrameElement(Element** aElement) {
  *aElement = nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> win = GetWindow();
  if (!win) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> top = win->GetInProcessScriptableTop();
  NS_ENSURE_TRUE(top, NS_ERROR_FAILURE);

  // GetFrameElementInternal, /not/ GetScriptableFrameElement -- if |top| is
  // inside <iframe mozbrowser>, we want to return the iframe, not null.
  // And we want to cross the content/chrome boundary.
  RefPtr<Element> elt = top->GetFrameElementInternal();
  elt.forget(aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetNestedFrameId(uint64_t* aId) {
  *aId = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetUseTrackingProtection(bool* aUseTrackingProtection) {
  *aUseTrackingProtection = false;

  if (mUseTrackingProtection ||
      StaticPrefs::privacy_trackingprotection_enabled() ||
      (UsePrivateBrowsing() &&
       StaticPrefs::privacy_trackingprotection_pbmode_enabled())) {
    *aUseTrackingProtection = true;
    return NS_OK;
  }

  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
  if (parent) {
    return parent->GetUseTrackingProtection(aUseTrackingProtection);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetUseTrackingProtection(bool aUseTrackingProtection) {
  mUseTrackingProtection = aUseTrackingProtection;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetIsContent(bool* aIsContent) {
  *aIsContent = (mItemType == typeContent);
  return NS_OK;
}

bool nsDocShell::IsOKToLoadURI(nsIURI* aURI) {
  MOZ_ASSERT(aURI, "Must have a URI!");

  if (!mFiredUnloadEvent) {
    return true;
  }

  if (!mLoadingURI) {
    return false;
  }

  bool isPrivateWin = false;
  Document* doc = GetDocument();
  if (doc) {
    isPrivateWin =
        doc->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId > 0;
  }

  nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  return secMan && NS_SUCCEEDED(secMan->CheckSameOriginURI(
                       aURI, mLoadingURI, false, isPrivateWin));
}

//
// Routines for selection and clipboard
//
nsresult nsDocShell::GetControllerForCommand(const char* aCommand,
                                             nsIController** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  NS_ENSURE_TRUE(mScriptGlobal, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIWindowRoot> root = mScriptGlobal->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllerForCommand(aCommand, false /* for any window */,
                                       aResult);
}

NS_IMETHODIMP
nsDocShell::IsCommandEnabled(const char* aCommand, bool* aResult) {
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
nsDocShell::DoCommand(const char* aCommand) {
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIController> controller;
  rv = GetControllerForCommand(aCommand, getter_AddRefs(controller));
  if (controller) {
    rv = controller->DoCommand(aCommand);
  }

  return rv;
}

NS_IMETHODIMP
nsDocShell::DoCommandWithParams(const char* aCommand,
                                nsICommandParams* aParams) {
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

nsresult nsDocShell::EnsureCommandHandler() {
  if (!mCommandManager) {
    if (nsCOMPtr<nsPIDOMWindowOuter> domWindow = GetWindow()) {
      mCommandManager = new nsCommandManager(domWindow);
    }
  }
  return mCommandManager ? NS_OK : NS_ERROR_FAILURE;
}

// link handling

class OnLinkClickEvent : public Runnable {
 public:
  OnLinkClickEvent(nsDocShell* aHandler, nsIContent* aContent, nsIURI* aURI,
                   const nsAString& aTargetSpec, const nsAString& aFileName,
                   nsIInputStream* aPostDataStream,
                   nsIInputStream* aHeadersDataStream, bool aNoOpenerImplied,
                   bool aIsUserTriggered, bool aIsTrusted,
                   nsIPrincipal* aTriggeringPrincipal,
                   nsIContentSecurityPolicy* aCsp);

  NS_IMETHOD Run() override {
    AutoPopupStatePusher popupStatePusher(mPopupState);

    // We need to set up an AutoJSAPI here for the following reason: When we
    // do OnLinkClickSync we'll eventually end up in
    // nsGlobalWindow::OpenInternal which only does popup blocking if
    // !LegacyIsCallerChromeOrNativeCode(). So we need to fake things so that
    // we don't look like native code as far as LegacyIsCallerNativeCode() is
    // concerned.
    AutoJSAPI jsapi;
    if (mIsTrusted || jsapi.Init(mContent->OwnerDoc()->GetScopeObject())) {
      mHandler->OnLinkClickSync(mContent, mURI, mTargetSpec, mFileName,
                                mPostDataStream, mHeadersDataStream,
                                mNoOpenerImplied, nullptr, nullptr,
                                mIsUserTriggered, mTriggeringPrincipal, mCsp);
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
  PopupBlocker::PopupControlState mPopupState;
  bool mNoOpenerImplied;
  bool mIsUserTriggered;
  bool mIsTrusted;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIContentSecurityPolicy> mCsp;
};

OnLinkClickEvent::OnLinkClickEvent(
    nsDocShell* aHandler, nsIContent* aContent, nsIURI* aURI,
    const nsAString& aTargetSpec, const nsAString& aFileName,
    nsIInputStream* aPostDataStream, nsIInputStream* aHeadersDataStream,
    bool aNoOpenerImplied, bool aIsUserTriggered, bool aIsTrusted,
    nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp)
    : mozilla::Runnable("OnLinkClickEvent"),
      mHandler(aHandler),
      mURI(aURI),
      mTargetSpec(aTargetSpec),
      mFileName(aFileName),
      mPostDataStream(aPostDataStream),
      mHeadersDataStream(aHeadersDataStream),
      mContent(aContent),
      mPopupState(PopupBlocker::GetPopupControlState()),
      mNoOpenerImplied(aNoOpenerImplied),
      mIsUserTriggered(aIsUserTriggered),
      mIsTrusted(aIsTrusted),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mCsp(aCsp) {}

nsresult nsDocShell::OnLinkClick(
    nsIContent* aContent, nsIURI* aURI, const nsAString& aTargetSpec,
    const nsAString& aFileName, nsIInputStream* aPostDataStream,
    nsIInputStream* aHeadersDataStream, bool aIsUserTriggered, bool aIsTrusted,
    nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp) {
#ifndef ANDROID
  MOZ_ASSERT(aTriggeringPrincipal, "Need a valid triggeringPrincipal");
#endif
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
    rv = browserChrome3->OnBeforeLinkTraversal(aTargetSpec, aURI, aContent,
                                               mIsAppTab, target);
    if (!aTargetSpec.Equals(target)) {
      noOpenerImplied = true;
    }
  }

  if (NS_FAILED(rv)) {
    target = aTargetSpec;
  }

  nsCOMPtr<nsIRunnable> ev = new OnLinkClickEvent(
      this, aContent, aURI, target, aFileName, aPostDataStream,
      aHeadersDataStream, noOpenerImplied, aIsUserTriggered, aIsTrusted,
      aTriggeringPrincipal, aCsp);
  return DispatchToTabGroup(TaskCategory::UI, ev.forget());
}

static bool IsElementAnchorOrArea(nsIContent* aContent) {
  // Make sure we are dealing with either an <A> or <AREA> element in the HTML
  // or XHTML namespace.
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::a, nsGkAtoms::area);
}

nsresult nsDocShell::OnLinkClickSync(
    nsIContent* aContent, nsIURI* aURI, const nsAString& aTargetSpec,
    const nsAString& aFileName, nsIInputStream* aPostDataStream,
    nsIInputStream* aHeadersDataStream, bool aNoOpenerImplied,
    nsIDocShell** aDocShell, nsIRequest** aRequest, bool aIsUserTriggered,
    nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp) {
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
        // if the URL scheme does not correspond to an exposed protocol, then
        // we need to hand this link click over to the external protocol
        // handler.
        bool isExposed;
        nsresult rv =
            extProtService->IsExposedProtocol(scheme.get(), &isExposed);
        if (NS_SUCCEEDED(rv) && !isExposed) {
          return extProtService->LoadURI(aURI, this);
        }
      }
    }
  }

  // if the triggeringPrincipal is not passed explicitly, then we
  // fall back to using doc->NodePrincipal() as the triggeringPrincipal.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
      aTriggeringPrincipal ? aTriggeringPrincipal : aContent->NodePrincipal();

  nsCOMPtr<nsIContentSecurityPolicy> csp = aCsp;
  if (!csp) {
    // Currently, if no csp is passed explicitly we fall back to querying the
    // CSP from the document.
    csp = aContent->GetCsp();
  }

  uint32_t flags = INTERNAL_LOAD_FLAGS_NONE;
  bool isElementAnchorOrArea = IsElementAnchorOrArea(aContent);
  if (isElementAnchorOrArea) {
    MOZ_ASSERT(aContent->IsHTMLElement());
    nsAutoString relString;
    aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::rel,
                                   relString);
    nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(
        relString);

    bool targetBlank = aTargetSpec.LowerCaseEqualsLiteral("_blank");
    bool explicitOpenerSet = false;

    // The opener behaviour follows a hierarchy, such that if a higher
    // priority behaviour is specified, it always takes priority. That
    // priority is currently: norefrerer > noopener > opener > default

    while (tok.hasMoreTokens()) {
      const nsAString& token = tok.nextToken();
      if (token.LowerCaseEqualsLiteral("noreferrer")) {
        flags |= INTERNAL_LOAD_FLAGS_DONT_SEND_REFERRER |
                 INTERNAL_LOAD_FLAGS_NO_OPENER;
        // noreferrer cannot be overwritten by a 'rel=opener'.
        explicitOpenerSet = true;
        break;
      }

      if (token.LowerCaseEqualsLiteral("noopener")) {
        flags |= INTERNAL_LOAD_FLAGS_NO_OPENER;
        explicitOpenerSet = true;
      }

      if (targetBlank && StaticPrefs::dom_targetBlankNoOpener_enabled() &&
          token.LowerCaseEqualsLiteral("opener") && !explicitOpenerSet) {
        explicitOpenerSet = true;
      }
    }

    if (targetBlank && StaticPrefs::dom_targetBlankNoOpener_enabled() &&
        !explicitOpenerSet &&
        !nsContentUtils::IsSystemPrincipal(triggeringPrincipal)) {
      flags |= INTERNAL_LOAD_FLAGS_NO_OPENER;
    }

    if (aNoOpenerImplied) {
      flags |= INTERNAL_LOAD_FLAGS_NO_OPENER;
    }
  }

  // Get the owner document of the link that was clicked, this will be
  // the document that the link is in, or the last document that the
  // link was in. From that document, we'll get the URI to use as the
  // referrer, since the current URI in this docshell may be a
  // new document that we're in the process of loading.
  RefPtr<Document> referrerDoc = aContent->OwnerDoc();
  NS_ENSURE_TRUE(referrerDoc, NS_ERROR_UNEXPECTED);

  // Now check that the referrerDoc's inner window is the current inner
  // window for mScriptGlobal.  If it's not, then we don't want to
  // follow this link.
  nsPIDOMWindowInner* referrerInner = referrerDoc->GetInnerWindow();
  NS_ENSURE_TRUE(referrerInner, NS_ERROR_UNEXPECTED);
  if (!mScriptGlobal ||
      mScriptGlobal->GetCurrentInnerWindow() != referrerInner) {
    // We're no longer the current inner window
    return NS_OK;
  }

  // referrer could be null here in some odd cases, but that's ok,
  // we'll just load the link w/o sending a referrer in those cases.

  // If this is an anchor element, grab its type property to use as a hint
  nsAutoString typeHint;
  RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::FromNode(aContent);
  if (anchor) {
    anchor->GetType(typeHint);
    NS_ConvertUTF16toUTF8 utf8Hint(typeHint);
    nsAutoCString type, dummy;
    NS_ParseRequestContentType(utf8Hint, type, dummy);
    CopyUTF8toUTF16(type, typeHint);
  }

  // Link click (or form submission) can be triggered inside an onload
  // handler, and we don't want to add history entry in this case.
  bool inOnLoadHandler = false;
  GetIsExecutingOnLoadHandler(&inOnLoadHandler);
  uint32_t loadType = inOnLoadHandler ? LOAD_NORMAL_REPLACE : LOAD_LINK;

  if (aIsUserTriggered) {
    flags |= INTERNAL_LOAD_FLAGS_IS_USER_TRIGGERED;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo = new ReferrerInfo();
  if (isElementAnchorOrArea) {
    referrerInfo->InitWithNode(aContent);
  } else {
    referrerInfo->InitWithDocument(referrerDoc);
  }

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(aURI);
  loadState->SetReferrerInfo(referrerInfo);
  loadState->SetTriggeringPrincipal(triggeringPrincipal);
  loadState->SetPrincipalToInherit(aContent->NodePrincipal());
  loadState->SetCsp(csp);
  loadState->SetLoadFlags(flags);
  loadState->SetTarget(aTargetSpec);
  loadState->SetTypeHint(NS_ConvertUTF16toUTF8(typeHint));
  loadState->SetFileName(aFileName);
  loadState->SetPostDataStream(aPostDataStream);
  loadState->SetHeadersStream(aHeadersDataStream);
  loadState->SetLoadType(loadType);
  loadState->SetFirstParty(true);
  loadState->SetSourceDocShell(this);
  loadState->SetIsFormSubmission(aContent->IsHTMLElement(nsGkAtoms::form));
  nsresult rv = InternalLoad(loadState, aDocShell, aRequest);

  if (NS_SUCCEEDED(rv)) {
    nsPingListener::DispatchPings(this, aContent, aURI, referrerInfo);
  }
  return rv;
}

nsresult nsDocShell::OnOverLink(nsIContent* aContent, nsIURI* aURI,
                                const nsAString& aTargetSpec) {
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
  rv = aURI->GetDisplaySpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 uStr(spec);

  PredictorPredict(aURI, mCurrentURI, nsINetworkPredictor::PREDICT_LINK,
                   aContent->NodePrincipal()->OriginAttributesRef(), nullptr);

  if (browserChrome2) {
    rv = browserChrome2->SetStatusWithContext(nsIWebBrowserChrome::STATUS_LINK,
                                              uStr, aContent);
  } else {
    rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, uStr.get());
  }
  return rv;
}

nsresult nsDocShell::OnLeaveLink() {
  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));
  nsresult rv = NS_ERROR_FAILURE;

  if (browserChrome) {
    rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK,
                                  EmptyString().get());
  }
  return rv;
}

bool nsDocShell::ShouldBlockLoadingForBackButton() {
  if (!(mLoadType & LOAD_CMD_HISTORY) ||
      EventStateManager::IsHandlingUserInput() ||
      !Preferences::GetBool("accessibility.blockjsredirection")) {
    return false;
  }

  bool canGoForward = false;
  GetCanGoForward(&canGoForward);
  return canGoForward;
}

bool nsDocShell::PluginsAllowedInCurrentDoc() {
  if (!mContentViewer) {
    return false;
  }

  Document* doc = mContentViewer->GetDocument();
  if (!doc) {
    return false;
  }

  return doc->GetAllowPlugins();
}

//----------------------------------------------------------------------
// Web Shell Services API

// This functions is only called when a new charset is detected in loading a
// document.
nsresult nsDocShell::CharsetChangeReloadDocument(const char* aCharset,
                                                 int32_t aSource) {
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
            return Reload(LOAD_FLAGS_CHARSET_CHANGE | LOAD_FLAGS_BYPASS_CACHE |
                          LOAD_FLAGS_BYPASS_PROXY);
          case LOAD_RELOAD_BYPASS_CACHE:
            return Reload(LOAD_FLAGS_CHARSET_CHANGE | LOAD_FLAGS_BYPASS_CACHE);
          default:
            return Reload(LOAD_FLAGS_CHARSET_CHANGE);
        }
      }
    }
  }
  // return failure if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}

nsresult nsDocShell::CharsetChangeStopDocumentLoad() {
  if (eCharsetReloadRequested != mCharsetReloadState) {
    Stop(nsIWebNavigation::STOP_ALL);
    return NS_OK;
  }
  // return failer if this request is not accepted due to mCharsetReloadState
  return NS_ERROR_DOCSHELL_REQUEST_REJECTED;
}

void nsDocShell::SetIsPrinting(bool aIsPrinting) {
  mIsPrintingOrPP = aIsPrinting;
}

NS_IMETHODIMP
nsDocShell::InitOrReusePrintPreviewViewer(nsIWebBrowserPrint** aPrintPreview) {
  *aPrintPreview = nullptr;
#if NS_PRINT_PREVIEW
  nsCOMPtr<nsIDocumentViewerPrint> print = do_QueryInterface(mContentViewer);
  if (!print || !print->IsInitializedForPrintPreview()) {
    // XXX: Creating a brand new content viewer to host preview every
    // time we enter here seems overwork. We could skip ahead to where
    // we QI the mContentViewer if the current URI is either about:blank
    // or about:printpreview.
    Stop(nsIWebNavigation::STOP_ALL);
    nsCOMPtr<nsIPrincipal> principal =
        NullPrincipal::CreateWithInheritedAttributes(this);
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:printpreview"));
    // Reuse the null principal for the storage principal.
    // XXXehsan is that the right principal to use here?
    nsresult rv = CreateAboutBlankContentViewer(principal, principal,
                                                /* aCsp = */ nullptr, uri);
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

NS_IMETHODIMP nsDocShell::ExitPrintPreview() {
#if NS_PRINT_PREVIEW
#  ifdef DEBUG
  nsCOMPtr<nsIDocumentViewerPrint> vp = do_QueryInterface(mContentViewer);
  MOZ_ASSERT(vp && vp->IsInitializedForPrintPreview());
#  endif
  nsCOMPtr<nsIWebBrowserPrint> viewer = do_QueryInterface(mContentViewer);
  return viewer->ExitPrintPreview();
#else
  return NS_OK;
#endif
}

#ifdef DEBUG
unsigned long nsDocShell::gNumberOfDocShells = 0;
#endif

NS_IMETHODIMP
nsDocShell::GetCanExecuteScripts(bool* aResult) {
  *aResult = mCanExecuteScripts;
  return NS_OK;
}

/* [infallible] */
NS_IMETHODIMP nsDocShell::SetFrameType(FrameType aFrameType) {
  mFrameType = aFrameType;
  return NS_OK;
}

/* [infallible] */
NS_IMETHODIMP nsDocShell::GetFrameType(FrameType* aFrameType) {
  *aFrameType = mFrameType;
  return NS_OK;
}

/* [infallible] */
NS_IMETHODIMP nsDocShell::GetIsMozBrowser(bool* aIsMozBrowser) {
  *aIsMozBrowser = (mFrameType == FRAME_TYPE_BROWSER);
  return NS_OK;
}

uint32_t nsDocShell::GetInheritedFrameType() {
  if (mFrameType != FRAME_TYPE_REGULAR) {
    return mFrameType;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  GetInProcessSameTypeParent(getter_AddRefs(parentAsItem));

  nsCOMPtr<nsIDocShell> parent = do_QueryInterface(parentAsItem);
  if (!parent) {
    return FRAME_TYPE_REGULAR;
  }

  return static_cast<nsDocShell*>(parent.get())->GetInheritedFrameType();
}

/* [infallible] */
NS_IMETHODIMP nsDocShell::GetIsInMozBrowser(bool* aIsInMozBrowser) {
  *aIsInMozBrowser = (GetInheritedFrameType() == FRAME_TYPE_BROWSER);
  return NS_OK;
}

/* [infallible] */
NS_IMETHODIMP nsDocShell::GetIsTopLevelContentDocShell(
    bool* aIsTopLevelContentDocShell) {
  *aIsTopLevelContentDocShell = false;

  if (mItemType == typeContent) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
    *aIsTopLevelContentDocShell =
        root.get() == static_cast<nsIDocShellTreeItem*>(this);
  }

  return NS_OK;
}

// Implements nsILoadContext.originAttributes
NS_IMETHODIMP
nsDocShell::GetScriptableOriginAttributes(JSContext* aCx,
                                          JS::MutableHandle<JS::Value> aVal) {
  return GetOriginAttributes(aCx, aVal);
}

// Implements nsIDocShell.GetOriginAttributes()
NS_IMETHODIMP
nsDocShell::GetOriginAttributes(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aVal) {
  bool ok = ToJSValue(aCx, mOriginAttributes, aVal);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

bool nsDocShell::CanSetOriginAttributes() {
  MOZ_ASSERT(mChildList.IsEmpty());
  if (!mChildList.IsEmpty()) {
    return false;
  }

  // TODO: Bug 1273058 - mContentViewer should be null when setting origin
  // attributes.
  if (mContentViewer) {
    Document* doc = mContentViewer->GetDocument();
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

bool nsDocShell::ServiceWorkerAllowedToControlWindow(nsIPrincipal* aPrincipal,
                                                     nsIURI* aURI) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aURI);

  if (UsePrivateBrowsing() || mSandboxFlags) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetInProcessSameTypeParent(getter_AddRefs(parent));
  nsPIDOMWindowOuter* parentOuter = parent ? parent->GetWindow() : nullptr;
  nsPIDOMWindowInner* parentInner =
      parentOuter ? parentOuter->GetCurrentInnerWindow() : nullptr;

  StorageAccess storage =
      StorageAllowedForNewWindow(aPrincipal, aURI, parentInner);

  return storage == StorageAccess::eAllow;
}

nsresult nsDocShell::SetOriginAttributes(const OriginAttributes& aAttrs) {
  MOZ_ASSERT(!mIsBeingDestroyed);

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
nsDocShell::SetOriginAttributesBeforeLoading(
    JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx) {
  if (!aOriginAttributes.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  OriginAttributes attrs;
  if (!attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return SetOriginAttributes(attrs);
}

NS_IMETHODIMP
nsDocShell::ResumeRedirectedLoad(uint64_t aIdentifier, int32_t aHistoryIndex) {
  RefPtr<nsDocShell> self = this;
  RefPtr<ChildProcessChannelListener> cpcl =
      ChildProcessChannelListener::GetSingleton();

  // Call into InternalLoad with the pending channel when it is received.
  cpcl->RegisterCallback(
      aIdentifier, [self, aHistoryIndex](nsIChildChannel* aChannel) {
        if (NS_WARN_IF(self->mIsBeingDestroyed)) {
          nsCOMPtr<nsIRequest> request = do_QueryInterface(aChannel);
          if (request) {
            request->Cancel(NS_BINDING_ABORTED);
          }
          return;
        }

        RefPtr<nsDocShellLoadState> loadState;
        nsresult rv = nsDocShellLoadState::CreateFromPendingChannel(
            aChannel, getter_AddRefs(loadState));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        // If we're performing a history load, locate the correct history entry,
        // and set the relevant bits on our loadState.
        if (aHistoryIndex >= 0 && self->mSessionHistory) {
          nsCOMPtr<nsISHistory> legacySHistory =
              self->mSessionHistory->LegacySHistory();

          nsCOMPtr<nsISHEntry> entry;
          rv = legacySHistory->GetEntryAtIndex(aHistoryIndex,
                                               getter_AddRefs(entry));
          if (NS_SUCCEEDED(rv)) {
            legacySHistory->InternalSetRequestedIndex(aHistoryIndex);
            loadState->SetLoadType(LOAD_HISTORY);
            loadState->SetSHEntry(entry);
          }
        }

        self->InternalLoad(loadState, nullptr, nullptr);
      });
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetOriginAttributes(JS::Handle<JS::Value> aOriginAttributes,
                                JSContext* aCx) {
  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return SetOriginAttributes(attrs);
}

NS_IMETHODIMP
nsDocShell::GetOSHEId(uint32_t* aSHEntryId) {
  if (mOSHE) {
    mOSHE->GetID(aSHEntryId);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsDocShell::GetAsyncPanZoomEnabled(bool* aOut) {
  if (PresShell* presShell = GetPresShell()) {
    *aOut = presShell->AsyncPanZoomEnabled();
    return NS_OK;
  }

  // If we don't have a presShell, fall back to the default platform value of
  // whether or not APZ is enabled.
  *aOut = gfxPlatform::AsyncPanZoomEnabled();
  return NS_OK;
}

bool nsDocShell::HasUnloadedParent() {
  RefPtr<nsDocShell> parent = GetInProcessParentDocshell();
  while (parent) {
    bool inUnload = false;
    parent->GetIsInUnload(&inUnload);
    if (inUnload) {
      return true;
    }
    parent = parent->GetInProcessParentDocshell();
  }
  return false;
}

void nsDocShell::UpdateGlobalHistoryTitle(nsIURI* aURI) {
  if (mUseGlobalHistory && !UsePrivateBrowsing()) {
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
      history->SetURITitle(aURI, mTitle);
    }
  }
}

bool nsDocShell::IsInvisible() { return mInvisible; }

void nsDocShell::SetInvisible(bool aInvisible) { mInvisible = aInvisible; }

void nsDocShell::SetOpener(nsIRemoteTab* aOpener) {
  mOpener = do_GetWeakReference(aOpener);
}

nsIRemoteTab* nsDocShell::GetOpener() {
  nsCOMPtr<nsIRemoteTab> opener(do_QueryReferent(mOpener));
  return opener;
}

// The caller owns |aAsyncCause| here.
void nsDocShell::NotifyJSRunToCompletionStart(const char* aReason,
                                              const char16_t* aFunctionName,
                                              const char16_t* aFilename,
                                              const uint32_t aLineNumber,
                                              JS::Handle<JS::Value> aAsyncStack,
                                              const char* aAsyncCause) {
  // If first start, mark interval start.
  if (mJSRunToCompletionDepth == 0) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    if (timelines && timelines->HasConsumer(this)) {
      timelines->AddMarkerForDocShell(
          this, mozilla::MakeUnique<JavascriptTimelineMarker>(
                    aReason, aFunctionName, aFilename, aLineNumber,
                    MarkerTracingType::START, aAsyncStack, aAsyncCause));
    }
  }

  mJSRunToCompletionDepth++;
}

void nsDocShell::NotifyJSRunToCompletionStop() {
  mJSRunToCompletionDepth--;

  // If last stop, mark interval end.
  if (mJSRunToCompletionDepth == 0) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    if (timelines && timelines->HasConsumer(this)) {
      timelines->AddMarkerForDocShell(this, "Javascript",
                                      MarkerTracingType::END);
    }
  }
}

void nsDocShell::MaybeNotifyKeywordSearchLoading(const nsString& aProvider,
                                                 const nsString& aKeyword) {
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

  nsCOMPtr<nsISearchService> searchSvc =
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
}

NS_IMETHODIMP
nsDocShell::ShouldPrepareForIntercept(nsIURI* aURI, nsIChannel* aChannel,
                                      bool* aShouldIntercept) {
  return mInterceptController->ShouldPrepareForIntercept(aURI, aChannel,
                                                         aShouldIntercept);
}

NS_IMETHODIMP
nsDocShell::ChannelIntercepted(nsIInterceptedChannel* aChannel) {
  return mInterceptController->ChannelIntercepted(aChannel);
}

bool nsDocShell::InFrameSwap() {
  RefPtr<nsDocShell> shell = this;
  do {
    if (shell->mInFrameSwap) {
      return true;
    }
    shell = shell->GetInProcessParentDocshell();
  } while (shell);
  return false;
}

UniquePtr<ClientSource> nsDocShell::TakeInitialClientSource() {
  return std::move(mInitialClientSource);
}

NS_IMETHODIMP
nsDocShell::IssueWarning(uint32_t aWarning, bool aAsError) {
  if (mContentViewer) {
    RefPtr<Document> doc = mContentViewer->GetDocument();
    if (doc) {
      doc->WarnOnceAbout(Document::DeprecatedOperations(aWarning), aAsError);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetEditingSession(nsIEditingSession** aEditSession) {
  if (!NS_SUCCEEDED(EnsureEditorData())) {
    return NS_ERROR_FAILURE;
  }

  *aEditSession = do_AddRef(mEditorData->GetEditingSession()).take();
  return *aEditSession ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocShell::GetScriptableBrowserChild(nsIBrowserChild** aBrowserChild) {
  *aBrowserChild = GetBrowserChild().take();
  return *aBrowserChild ? NS_OK : NS_ERROR_FAILURE;
}

already_AddRefed<nsIBrowserChild> nsDocShell::GetBrowserChild() {
  nsCOMPtr<nsIBrowserChild> tc = do_QueryReferent(mBrowserChild);
  return tc.forget();
}

nsCommandManager* nsDocShell::GetCommandManager() {
  NS_ENSURE_SUCCESS(EnsureCommandHandler(), nullptr);
  return mCommandManager;
}

NS_IMETHODIMP
nsDocShell::GetIsOnlyToplevelInTabGroup(bool* aResult) {
  MOZ_ASSERT(aResult);

  nsPIDOMWindowOuter* outer = GetWindow();
  MOZ_ASSERT(outer);

  // If we are not toplevel then we are not the only toplevel window in the
  // tab group.
  if (outer->GetInProcessScriptableParentOrNull()) {
    *aResult = false;
    return NS_OK;
  }

  // If we have any other toplevel windows in our tab group, then we are not
  // the only toplevel window in the tab group.
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
nsDocShell::GetAwaitingLargeAlloc(bool* aResult) {
  MOZ_ASSERT(aResult);
  nsCOMPtr<nsIBrowserChild> browserChild = GetBrowserChild();
  if (!browserChild) {
    *aResult = false;
    return NS_OK;
  }
  *aResult =
      static_cast<BrowserChild*>(browserChild.get())->IsAwaitingLargeAlloc();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsDocShell::GetOriginAttributes(mozilla::OriginAttributes& aAttrs) {
  aAttrs = mOriginAttributes;
}

HTMLEditor* nsIDocShell::GetHTMLEditor() {
  nsDocShell* docShell = static_cast<nsDocShell*>(this);
  return docShell->GetHTMLEditorInternal();
}

nsresult nsIDocShell::SetHTMLEditor(HTMLEditor* aHTMLEditor) {
  nsDocShell* docShell = static_cast<nsDocShell*>(this);
  return docShell->SetHTMLEditorInternal(aHTMLEditor);
}

NS_IMETHODIMP
nsDocShell::GetDisplayMode(DisplayMode* aDisplayMode) {
  *aDisplayMode = mDisplayMode;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetDisplayMode(DisplayMode aDisplayMode) {
  // We don't have a way to verify this coming from Javascript, so this check
  // is still needed.
  if (!(aDisplayMode == nsIDocShell::DISPLAY_MODE_BROWSER ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_STANDALONE ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_FULLSCREEN ||
        aDisplayMode == nsIDocShell::DISPLAY_MODE_MINIMAL_UI)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aDisplayMode != mDisplayMode) {
    mDisplayMode = aDisplayMode;

    RefPtr<nsPresContext> presContext = GetPresContext();
    presContext->MediaFeatureValuesChangedAllDocuments(
        {MediaFeatureChangeReason::DisplayModeChange});
  }

  return NS_OK;
}

#define MATRIX_LENGTH 20

NS_IMETHODIMP
nsDocShell::SetColorMatrix(const nsTArray<float>& aMatrix) {
  if (aMatrix.Length() == MATRIX_LENGTH) {
    mColorMatrix.reset(new gfx::Matrix5x4());
    static_assert(
        MATRIX_LENGTH * sizeof(float) == sizeof(mColorMatrix->components),
        "Size mismatch for our memcpy");
    memcpy(mColorMatrix->components, aMatrix.Elements(),
           sizeof(mColorMatrix->components));
  } else if (aMatrix.Length() == 0) {
    mColorMatrix.reset();
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* frame = presShell->GetRootFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  frame->SchedulePaint();

  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::GetColorMatrix(nsTArray<float>& aMatrix) {
  if (mColorMatrix) {
    aMatrix.SetLength(MATRIX_LENGTH);
    static_assert(
        MATRIX_LENGTH * sizeof(float) == sizeof(mColorMatrix->components),
        "Size mismatch for our memcpy");
    memcpy(aMatrix.Elements(), mColorMatrix->components,
           MATRIX_LENGTH * sizeof(float));
  }

  return NS_OK;
}

#undef MATRIX_LENGTH

bool nsDocShell::IsForceReloading() { return IsForceReloadType(mLoadType); }

NS_IMETHODIMP
nsDocShell::GetBrowsingContextXPCOM(BrowsingContext** aBrowsingContext) {
  *aBrowsingContext = do_AddRef(mBrowsingContext).take();
  return NS_OK;
}

BrowsingContext* nsDocShell::GetBrowsingContext() { return mBrowsingContext; }

bool nsDocShell::GetIsAttemptingToNavigate() {
  // XXXbz the document.open spec says to abort even if there's just a
  // queued navigation task, sort of.  It's not clear whether browsers
  // actually do that, and we didn't use to do it, so for now let's
  // not do that.
  // https://github.com/whatwg/html/issues/3447 tracks the spec side of this.
  if (mDocumentRequest) {
    // There's definitely a navigation in progress.
    return true;
  }

  // javascript: channels have slightly weird behavior: they're LOAD_BACKGROUND
  // until the script runs, which means they're not sending loadgroup
  // notifications and hence not getting set as mDocumentRequest.  Look through
  // our loadgroup for document-level javascript: loads.
  if (!mLoadGroup) {
    return false;
  }

  nsCOMPtr<nsISimpleEnumerator> requests;
  mLoadGroup->GetRequests(getter_AddRefs(requests));
  bool hasMore = false;
  while (NS_SUCCEEDED(requests->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    requests->GetNext(getter_AddRefs(elem));
    nsCOMPtr<nsIScriptChannel> scriptChannel(do_QueryInterface(elem));
    if (!scriptChannel) {
      continue;
    }

    if (scriptChannel->GetIsDocumentLoad()) {
      // This is a javascript: load that might lead to a new document,
      // hence a navigation.
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
nsDocShell::GetWatchedByDevtools(bool* aWatched) {
  NS_ENSURE_ARG(aWatched);
  *aWatched = mWatchedByDevtools;
  return NS_OK;
}

NS_IMETHODIMP
nsDocShell::SetWatchedByDevtools(bool aWatched) {
  mWatchedByDevtools = aWatched;
  return NS_OK;
}
