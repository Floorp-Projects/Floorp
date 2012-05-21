/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

/* This must occur *after* base/basictypes.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

// Local Includes
#include "nsGlobalWindow.h"
#include "Navigator.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsPerformance.h"
#include "nsDOMNavigationTiming.h"
#include "nsBarProps.h"
#include "nsDOMStorage.h"
#include "nsDOMOfflineResourceList.h"
#include "nsDOMError.h"

#ifdef XP_WIN
#ifdef GetClassName
#undef GetClassName
#endif // GetClassName
#endif // XP_WIN

// Helper Classes
#include "nsXPIDLString.h"
#include "nsJSUtils.h"
#include "prmem.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jsdbgapi.h"           // for JS_ClearWatchPointsForObject
#include "jsfriendapi.h"        // for JS_GetGlobalForFrame
#include "jswrapper.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSEnvironment.h"
#include "nsCharSeparatedTokenizer.h" // for Accept-Language parsing
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"

// Other Classes
#include "nsEventListenerManager.h"
#include "nsEscape.h"
#include "nsStyleCoord.h"
#include "nsMimeTypeArray.h"
#include "nsNetUtil.h"
#include "nsICachingChannel.h"
#include "nsPluginArray.h"
#include "nsIPluginHost.h"
#include "nsPluginHost.h"
#include "nsIPluginInstanceOwner.h"
#include "nsGeolocation.h"
#include "nsDesktopNotification.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCycleCollector.h"
#include "nsCCUncollectableMarker.h"
#include "nsAutoJSValHolder.h"
#include "nsDOMMediaQueryList.h"
#include "mozilla/dom/workers/Workers.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsDeviceSensors.h"
#include "nsIContent.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIEditorDocShell.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#ifndef MOZ_DISABLE_DOMCRYPTO
#include "nsIDOMCrypto.h"
#endif
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMessageEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMPopStateEvent.h"
#include "nsIDOMHashChangeEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMDesktopNotification.h"
#include "nsPIDOMStorage.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsThreadUtils.h"
#include "nsEventStateManager.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIProgrammingLanguage.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIPrompt.h"
#include "nsIPromptService.h"
#include "nsIPromptFactory.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWebContentHandlerRegistrar.h"
#include "nsIWindowMediator.h"  // For window.find()
#include "nsComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsIJSNativeInitializer.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
#include "nsBlobProtocolHandler.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIURIFixup.h"
#include "mozilla/FunctionTimer.h"
#include "nsCDefaultURIFixup.h"
#include "nsEventDispatcher.h"
#include "nsIObserverService.h"
#include "nsIXULAppInfo.h"
#include "nsNetUtil.h"
#include "nsFocusManager.h"
#include "nsIXULWindow.h"
#include "nsEventStateManager.h"
#include "nsITimedChannel.h"
#include "nsICookiePermission.h"
#include "nsServiceManagerUtils.h"
#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#include "nsIDOMXULControlElement.h"
#include "nsMenuPopupFrame.h"
#endif

#include "xpcprivate.h"

#ifdef NS_PRINTING
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"
#include "nsIScriptRuntime.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsBindingManager.h"
#include "nsIXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

#include "nsIDragService.h"
#include "mozilla/dom/Element.h"
#include "nsFrameLoader.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"

#include "mozilla/FunctionTimer.h"
#include "mozIThirdPartyUtil.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prenv.h"

#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"

#include "mozilla/dom/StructuredCloneTags.h"

#include "nsRefreshDriver.h"
#include "mozAutoDocUpdate.h"

#include "mozilla/Telemetry.h"
#include "nsLocation.h"
#include "nsWrapperCacheInlines.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIAppsService.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gDOMLeakPRLog;
#endif

static const char kStorageEnabled[] = "dom.storage.enabled";

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::TimeStamp;
using mozilla::TimeDuration;

nsGlobalWindow::WindowByIdTable *nsGlobalWindow::sWindowsById = nsnull;
bool nsGlobalWindow::sWarnedAboutWindowInternal = false;

static nsIEntropyCollector *gEntropyCollector          = nsnull;
static PRInt32              gRefCnt                    = 0;
static PRInt32              gOpenPopupSpamCount        = 0;
static PopupControlState    gPopupControlState         = openAbused;
static PRInt32              gRunningTimeoutDepth       = 0;
static bool                 gMouseDown                 = false;
static bool                 gDragServiceDisabled       = false;
static FILE                *gDumpFile                  = nsnull;
static PRUint64             gNextWindowID              = 0;
static PRUint32             gSerialCounter             = 0;
static PRUint32             gTimeoutsRecentlySet       = 0;
static TimeStamp            gLastRecordedRecentTimeouts;
#define STATISTICS_INTERVAL (30 * PR_MSEC_PER_SEC)

#ifdef DEBUG_jst
PRInt32 gTimeoutCnt                                    = 0;
#endif

#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
static bool                 gDOMWindowDumpEnabled      = false;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"

// The default shortest interval/timeout we permit
#define DEFAULT_MIN_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
static PRInt32 gMinTimeoutValue;
static PRInt32 gMinBackgroundTimeoutValue;
inline PRInt32
nsGlobalWindow::DOMMinTimeoutValue() const {
  bool isBackground = !mOuterWindow || mOuterWindow->IsBackground();
  return
    NS_MAX(isBackground ? gMinBackgroundTimeoutValue : gMinTimeoutValue, 0);
}

// The number of nested timeouts before we start clamping. HTML5 says 1, WebKit
// uses 5.
#define DOM_CLAMP_TIMEOUT_NESTING_LEVEL 5

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE    DELAY_INTERVAL_LIMIT

#define FORWARD_TO_OUTER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return outer->method args;                                                \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return;                                                                 \
    }                                                                         \
    outer->method args;                                                       \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_CHROME(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalChromeWindow *)outer)->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_CHROME(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalChromeWindow *)mInnerWindow)->method args;               \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(method, args, err_rval)         \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalModalWindow *)outer)->method args;                       \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_MODAL_CONTENT_WINDOW(method, args, err_rval)         \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalModalWindow*)GetCurrentInnerWindowInternal())->method args; \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return;                                                                 \
    }                                                                         \
    GetCurrentInnerWindowInternal()->method args;                             \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

// Same as FORWARD_TO_INNER, but this will create a fresh inner if an
// inner doesn't already exists.
#define FORWARD_TO_INNER_CREATE(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      if (mIsClosed) {                                                        \
        return err_rval;                                                      \
      }                                                                       \
      nsCOMPtr<nsIDOMDocument> doc;                                           \
      nsresult fwdic_nr = GetDocument(getter_AddRefs(doc));                   \
      NS_ENSURE_SUCCESS(fwdic_nr, err_rval);                                  \
      if (!mInnerWindow) {                                                    \
        return err_rval;                                                      \
      }                                                                       \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

// CIDs
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";
#ifndef MOZ_DISABLE_DOMCRYPTO
static const char kCryptoContractID[] = NS_CRYPTO_CONTRACTID;
static const char kPkcs11ContractID[] = NS_PKCS11_CONTRACTID;
#endif
static const char sPopStatePrefStr[] = "browser.history.allowPopState";

/**
 * An object implementing the window.URL property.
 */
class nsDOMMozURLProperty : public nsIDOMMozURLProperty
{
public:
  nsDOMMozURLProperty(nsGlobalWindow* aWindow)
    : mWindow(aWindow)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZURLPROPERTY

  void ClearWindowReference() {
    mWindow = nsnull;
  }
private:
  nsGlobalWindow* mWindow;
};

DOMCI_DATA(MozURLProperty, nsDOMMozURLProperty)
NS_IMPL_ADDREF(nsDOMMozURLProperty)
NS_IMPL_RELEASE(nsDOMMozURLProperty)
NS_INTERFACE_MAP_BEGIN(nsDOMMozURLProperty)
    NS_INTERFACE_MAP_ENTRY(nsIDOMMozURLProperty)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozURLProperty)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozURLProperty)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMMozURLProperty::CreateObjectURL(nsIDOMBlob* aBlob, nsAString& aURL)
{
  NS_PRECONDITION(!mWindow || mWindow->IsInnerWindow(),
                  "Should be inner window");

  NS_ENSURE_STATE(mWindow && mWindow->mDoc);
  NS_ENSURE_ARG_POINTER(aBlob);

  nsIDocument* doc = mWindow->mDoc;

  nsresult rv = aBlob->GetInternalUrl(doc->NodePrincipal(), aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  doc->RegisterFileDataUri(NS_LossyConvertUTF16toASCII(aURL));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozURLProperty::RevokeObjectURL(const nsAString& aURL)
{
  NS_PRECONDITION(!mWindow || mWindow->IsInnerWindow(),
                  "Should be inner window");

  NS_ENSURE_STATE(mWindow);

  NS_LossyConvertUTF16toASCII asciiurl(aURL);

  nsIPrincipal* winPrincipal = mWindow->GetPrincipal();
  if (!winPrincipal) {
    return NS_OK;
  }

  nsIPrincipal* principal =
    nsBlobProtocolHandler::GetFileDataEntryPrincipal(asciiurl);
  bool subsumes;
  if (principal && winPrincipal &&
      NS_SUCCEEDED(winPrincipal->Subsumes(principal, &subsumes)) &&
      subsumes) {
    if (mWindow->mDoc) {
      mWindow->mDoc->UnregisterFileDataUri(asciiurl);
    }
    nsBlobProtocolHandler::RemoveFileDataEntry(asciiurl);
  }

  return NS_OK;
}

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver : public nsIObserver {
public:
  nsGlobalWindowObserver(nsGlobalWindow* aWindow) : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
  {
    if (!mWindow)
      return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nsnull; }
private:
  nsGlobalWindow* mWindow;
};

NS_IMPL_ISUPPORTS1(nsGlobalWindowObserver, nsIObserver)

nsTimeout::nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    ++gTimeoutCnt;
  }
#endif

  memset(this, 0, sizeof(*this));

  MOZ_COUNT_CTOR(nsTimeout);
}

nsTimeout::~nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    --gTimeoutCnt;
  }
#endif

  MOZ_COUNT_DTOR(nsTimeout);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NATIVE_0(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsTimeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWindow,
                                                       nsIScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTimeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTimeout, Release)

nsPIDOMWindow::nsPIDOMWindow(nsPIDOMWindow *aOuterWindow)
: mFrameElement(nsnull), mDocShell(nsnull), mModalStateDepth(0),
  mRunningTimeout(nsnull), mMutationBits(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nsnull),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mIsModalContentWindow(false),
  mIsActive(false), mIsBackground(false),
  mInnerWindow(nsnull), mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(++gNextWindowID), mHasNotifiedGlobalCreated(false)
 {}

nsPIDOMWindow::~nsPIDOMWindow() {}

//*****************************************************************************
// nsOuterWindowProxy: Outer Window Proxy
//*****************************************************************************

class nsOuterWindowProxy : public js::Wrapper
{
public:
  nsOuterWindowProxy() : js::Wrapper(0) {}

  virtual bool isOuterWindow() {
    return true;
  }
  JSString *obj_toString(JSContext *cx, JSObject *wrapper);
  void finalize(JSFreeOp *fop, JSObject *proxy);

  static nsOuterWindowProxy singleton;
};


JSString *
nsOuterWindowProxy::obj_toString(JSContext *cx, JSObject *proxy)
{
    JS_ASSERT(js::IsProxy(proxy));

    return JS_NewStringCopyZ(cx, "[object Window]");
}

void
nsOuterWindowProxy::finalize(JSFreeOp *fop, JSObject *proxy)
{
  nsISupports *global =
    static_cast<nsISupports*>(js::GetProxyExtra(proxy, 0).toPrivate());
  if (global) {
    nsWrapperCache *cache;
    CallQueryInterface(global, &cache);
    cache->ClearWrapper();
  }
}

nsOuterWindowProxy
nsOuterWindowProxy::singleton;

static JSObject*
NewOuterWindowProxy(JSContext *cx, JSObject *parent)
{
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, parent)) {
    return nsnull;
  }

  JSObject *obj = js::Wrapper::New(cx, parent, js::GetObjectProto(parent), parent,
                                   &nsOuterWindowProxy::singleton);
  NS_ASSERTION(js::GetObjectClass(obj)->ext.innerObject, "bad class");
  return obj;
}

//*****************************************************************************
//***    nsGlobalWindow: Object Management
//*****************************************************************************

nsGlobalWindow::nsGlobalWindow(nsGlobalWindow *aOuterWindow)
  : nsPIDOMWindow(aOuterWindow),
    mIsFrozen(false),
    mFullScreen(false),
    mIsClosed(false), 
    mInClose(false), 
    mHavePendingClose(false),
    mHadOriginalOpener(false),
    mIsPopupSpam(false),
    mBlockScriptedClosingFlag(false),
    mFireOfflineStatusChangeEventOnThaw(false),
    mCreatingInnerWindow(false),
    mIsChrome(false),
    mCleanMessageManager(false),
    mNeedsFocus(true),
    mHasFocus(false),
#if defined(XP_MACOSX)
    mShowAccelerators(false),
    mShowFocusRings(false),
#else
    mShowAccelerators(true),
    mShowFocusRings(true),
#endif
    mShowFocusRingForContent(false),
    mFocusByKeyOccurred(false),
    mNotifiedIDDestroyed(false),
    mIsApp(TriState_Unknown),
    mTimeoutInsertionPoint(nsnull),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mJSObject(nsnull),
    mTimeoutsSuspendDepth(0),
    mFocusMethod(0),
    mSerial(0),
#ifdef DEBUG
    mSetOpenerWindowCalled(false),
#endif
    mCleanedUp(false),
    mCallCleanUpAfterModalDialogCloses(false),
    mDialogAbuseCount(0),
    mDialogDisabled(false)
{
  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  // Initialize timeout storage
  PR_INIT_CLIST(&mTimeouts);

  if (aOuterWindow) {
    // |this| is an inner window, add this inner window to the outer
    // window list of inners.
    PR_INSERT_AFTER(this, aOuterWindow);

    mObserver = new nsGlobalWindowObserver(this);
    if (mObserver) {
      NS_ADDREF(mObserver);
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        // Watch for online/offline status changes so we can fire events. Use
        // a strong reference.
        os->AddObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                        false);

        // Watch for dom-storage2-changed so we can fire storage
        // events. Use a strong reference.
        os->AddObserver(mObserver, "dom-storage2-changed", false);
      }
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window, so freeze this
    // outer window here.
    Freeze();

    mObserver = nsnull;
    SetIsDOMBinding();
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  gRefCnt++;

  if (gRefCnt == 1) {
#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
    Preferences::AddBoolVarCache(&gDOMWindowDumpEnabled,
                                 "browser.dom.window.dump.enabled");
#endif
    Preferences::AddIntVarCache(&gMinTimeoutValue,
                                "dom.min_timeout_value",
                                DEFAULT_MIN_TIMEOUT_VALUE);
    Preferences::AddIntVarCache(&gMinBackgroundTimeoutValue,
                                "dom.min_background_timeout_value",
                                DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE);
  }

  if (gDumpFile == nsnull) {
    const nsAdoptingCString& fname =
      Preferences::GetCString("browser.dom.window.dump.file");
    if (!fname.IsEmpty()) {
      // if this fails to open, Dump() knows to just go to stdout
      // on null.
      gDumpFile = fopen(fname, "wb+");
    } else {
      gDumpFile = stdout;
    }
  }

  mSerial = ++gSerialCounter;

#ifdef DEBUG
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf("++DOMWINDOW == %d (%p) [serial = %d] [outer = %p]\n", gRefCnt,
           static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
           gSerialCounter, static_cast<void*>(aOuterWindow));
  }
#endif

#ifdef PR_LOGGING
  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p created outer=%p", this, aOuterWindow));
#endif

  NS_ASSERTION(sWindowsById, "Windows hash table must be created!");
  NS_ASSERTION(!sWindowsById->Get(mWindowID),
               "This window shouldn't be in the hash table yet!");
  sWindowsById->Put(mWindowID, this);

  mEventTargetObjects.Init();
}

/* static */
void
nsGlobalWindow::Init()
{
  CallGetService(NS_ENTROPYCOLLECTOR_CONTRACTID, &gEntropyCollector);
  NS_ASSERTION(gEntropyCollector,
               "gEntropyCollector should have been initialized!");

#ifdef PR_LOGGING
  gDOMLeakPRLog = PR_NewLogModule("DOMLeak");
  NS_ASSERTION(gDOMLeakPRLog, "gDOMLeakPRLog should have been initialized!");
#endif

  sWindowsById = new WindowByIdTable();
  sWindowsById->Init();
}

static PLDHashOperator
DisconnectEventTargetObjects(nsPtrHashKey<nsDOMEventTargetHelper>* aKey,
                             void* aClosure)
{
  nsRefPtr<nsDOMEventTargetHelper> target = aKey->GetKey();
  target->DisconnectFromOwner();
  return PL_DHASH_NEXT;
}

nsGlobalWindow::~nsGlobalWindow()
{
  mEventTargetObjects.EnumerateEntries(DisconnectEventTargetObjects, nsnull);
  mEventTargetObjects.Clear();

  // We have to check if sWindowsById isn't null because ::Shutdown might have
  // been called.
  if (sWindowsById) {
    NS_ASSERTION(sWindowsById->Get(mWindowID),
                 "This window should be in the hash table");
    sWindowsById->Remove(mWindowID);
  }

  --gRefCnt;

#ifdef DEBUG
  if (!PR_GetEnv("MOZ_QUIET")) {
    nsCAutoString url;
    if (mLastOpenedURI) {
      mLastOpenedURI->GetSpec(url);
    }

    printf("--DOMWINDOW == %d (%p) [serial = %d] [outer = %p] [url = %s]\n",
           gRefCnt, static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
           mSerial, static_cast<void*>(mOuterWindow.get()), url.get());
  }
#endif

#ifdef PR_LOGGING
  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p destroyed", this));
#endif

  if (IsOuterWindow()) {
    JSObject *proxy = GetWrapperPreserveColor();
    if (proxy) {
      js::SetProxyExtra(proxy, 0, js::PrivateValue(NULL));
    }

    // An outer window is destroyed with inner windows still possibly
    // alive, iterate through the inner windows and null out their
    // back pointer to this outer, and pull them out of the list of
    // inner windows.

    nsGlobalWindow *w;
    while ((w = (nsGlobalWindow *)PR_LIST_HEAD(this)) != this) {
      PR_REMOVE_AND_INIT_LINK(w);
    }
  } else {
    Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                          mMutationBits ? 1 : 0);

    if (mListenerManager) {
      mListenerManager->Disconnect();
      mListenerManager = nsnull;
    }

    // An inner window is destroyed, pull it out of the outer window's
    // list if inner windows.

    PR_REMOVE_LINK(this);

    // If our outer window's inner window is this window, null out the
    // outer window's reference to this window that's being deleted.
    nsGlobalWindow *outer = GetOuterWindowInternal();
    if (outer) {
      outer->MaybeClearInnerWindow(this);
    }
  }

  mDocument = nsnull;           // Forces Release
  mDoc = nsnull;

  NS_ASSERTION(!mArguments, "mArguments wasn't cleaned up properly!");

  CleanUp(true);

#ifdef DEBUG
  nsCycleCollector_DEBUG_wasFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif

  if (mURLProperty) {
    mURLProperty->ClearWindowReference();
  }

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->RemoveWindowAsListener(this);

  nsLayoutStatics::Release();
}

void
nsGlobalWindow::AddEventTargetObject(nsDOMEventTargetHelper* aObject)
{
  mEventTargetObjects.PutEntry(aObject);
}

void
nsGlobalWindow::RemoveEventTargetObject(nsDOMEventTargetHelper* aObject)
{
  mEventTargetObjects.RemoveEntry(aObject);
}

// static
void
nsGlobalWindow::ShutDown()
{
  if (gDumpFile && gDumpFile != stdout) {
    fclose(gDumpFile);
  }
  gDumpFile = nsnull;

  NS_IF_RELEASE(gEntropyCollector);

  delete sWindowsById;
  sWindowsById = nsnull;
}

// static
void
nsGlobalWindow::CleanupCachedXBLHandlers(nsGlobalWindow* aWindow)
{
  if (aWindow->mCachedXBLPrototypeHandlers.IsInitialized() &&
      aWindow->mCachedXBLPrototypeHandlers.Count() > 0) {
    aWindow->mCachedXBLPrototypeHandlers.Clear();

    nsISupports* supports;
    aWindow->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                            reinterpret_cast<void**>(&supports));
    NS_ASSERTION(supports, "Failed to QI to nsCycleCollectionISupports?!");

    nsContentUtils::DropJSObjects(supports);
  }
}

void
nsGlobalWindow::MaybeForgiveSpamCount()
{
  if (IsOuterWindow() &&
      IsPopupSpamWindow())
  {
    SetPopupSpamWindow(false);
    --gOpenPopupSpamCount;
    NS_ASSERTION(gOpenPopupSpamCount >= 0,
                 "Unbalanced decrement of gOpenPopupSpamCount");
  }
}

void
nsGlobalWindow::CleanUp(bool aIgnoreModalDialog)
{
  if (IsOuterWindow() && !aIgnoreModalDialog) {
    nsGlobalWindow* inner = GetCurrentInnerWindowInternal();
    nsCOMPtr<nsIDOMModalContentWindow> dlg(do_QueryObject(inner));
    if (dlg) {
      // The window we're trying to clean up is the outer window of a
      // modal dialog.  Defer cleanup until the window closes, and let
      // ShowModalDialog take care of calling CleanUp.
      mCallCleanUpAfterModalDialogCloses = true;
      return;
    }
  }

  // Guarantee idempotence.
  if (mCleanedUp)
    return;
  mCleanedUp = true;
  
  mEventTargetObjects.EnumerateEntries(DisconnectEventTargetObjects, nsnull);
  mEventTargetObjects.Clear();

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, "dom-storage2-changed");
    }

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
    NS_RELEASE(mObserver);
  }

  mNavigator = nsnull;
  mScreen = nsnull;
  mMenubar = nsnull;
  mToolbar = nsnull;
  mLocationbar = nsnull;
  mPersonalbar = nsnull;
  mStatusbar = nsnull;
  mScrollbars = nsnull;
  mLocation = nsnull;
  mHistory = nsnull;
  mFrames = nsnull;
  mWindowUtils = nsnull;
  mApplicationCache = nsnull;
  mIndexedDB = nsnull;

  mPerformance = nsnull;

  ClearControllers();

  mOpener = nsnull;             // Forces Release
  if (mContext) {
#ifdef DEBUG
    nsCycleCollector_DEBUG_shouldBeFreed(mContext);
#endif
    mContext = nsnull;            // Forces Release
  }
  mChromeEventHandler = nsnull; // Forces Release
  mParentTarget = nsnull;

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  if (inner) {
    inner->CleanUp(aIgnoreModalDialog);
  }

  if (mCleanMessageManager) {
    NS_ABORT_IF_FALSE(mIsChrome, "only chrome should have msg manager cleaned");
    nsGlobalChromeWindow *asChrome = static_cast<nsGlobalChromeWindow*>(this);
    if (asChrome->mMessageManager) {
      static_cast<nsFrameMessageManager*>(
        asChrome->mMessageManager.get())->Disconnect();
    }
  }

  mInnerWindowHolder = nsnull;
  mArguments = nsnull;
  mArgumentsLast = nsnull;
  mArgumentsOrigin = nsnull;

  CleanupCachedXBLHandlers(this);

#ifdef DEBUG
  nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
}

void
nsGlobalWindow::ClearControllers()
{
  if (mControllers) {
    PRUint32 count;
    mControllers->GetControllerCount(&count);

    while (count--) {
      nsCOMPtr<nsIController> controller;
      mControllers->GetControllerAt(count, getter_AddRefs(controller));

      nsCOMPtr<nsIControllerContext> context = do_QueryInterface(controller);
      if (context)
        context->SetCommandContext(nsnull);
    }

    mControllers = nsnull;
  }
}

void
nsGlobalWindow::FreeInnerObjects()
{
  NS_ASSERTION(IsInnerWindow(), "Don't free inner objects on an outer window");

  // Make sure that this is called before we null out the document and
  // other members that the window destroyed observers could
  // re-create.
  NotifyDOMWindowDestroyed(this);

  // Kill all of the workers for this window.
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = scx ? scx->GetNativeContext() : nsnull;
  mozilla::dom::workers::CancelWorkersForWindow(cx, this);

  // Close all IndexedDB databases for this window.
  indexedDB::IndexedDatabaseManager* idbManager =
    indexedDB::IndexedDatabaseManager::Get();
  if (idbManager) {
    idbManager->AbortCloseDatabasesForWindow(this);
  }

  ClearAllTimeouts();

  mChromeEventHandler = nsnull;

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nsnull;
  }

  mLocation = nsnull;
  mHistory = nsnull;

  if (mNavigator) {
    mNavigator->Invalidate();
    mNavigator = nsnull;
  }

  if (mScreen) {
    mScreen = nsnull;
  }

  if (mDocument) {
    NS_ASSERTION(mDoc, "Why is mDoc null?");

    // Remember the document's principal.
    mDocumentPrincipal = mDoc->NodePrincipal();
  }

#ifdef DEBUG
  if (mDocument)
    nsCycleCollector_DEBUG_shouldBeFreed(nsCOMPtr<nsISupports>(do_QueryInterface(mDocument)));
#endif

  // Remove our reference to the document and the document principal.
  mDocument = nsnull;
  mDoc = nsnull;
  mFocusedNode = nsnull;

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->Disconnect();
    mApplicationCache = nsnull;
  }

  mIndexedDB = nsnull;

  NotifyWindowIDDestroyed("inner-window-destroyed");

  CleanupCachedXBLHandlers(this);

#ifdef DEBUG
  nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

#define OUTER_WINDOW_ONLY                                                     \
  if (IsOuterWindow()) {

#define END_OUTER_WINDOW_ONLY                                                 \
    foundInterface = 0;                                                       \
  } else

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindow)

DOMCI_DATA(Window, nsGlobalWindow)

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindow)
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSWindow)
  if (aIID.Equals(NS_GET_IID(nsIDOMWindowInternal))) {
    foundInterface = static_cast<nsIDOMWindowInternal*>(this);
    if (!sWarnedAboutWindowInternal) {
      sWarnedAboutWindowInternal = true;
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      "Extensions", mDoc,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "nsIDOMWindowInternalWarning");
    }
  } else
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageIndexedDB)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowPerformance)
  NS_INTERFACE_MAP_ENTRY(nsITouchEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIInlineEventHandlers)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Window)
  OUTER_WINDOW_ONLY
    NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  END_OUTER_WINDOW_ONLY
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGlobalWindow)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGlobalWindow)

static PLDHashOperator
MarkXBLHandlers(nsXBLPrototypeHandler* aKey, JSObject* aData, void* aClosure)
{
  xpc_UnmarkGrayObject(aData);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGlobalWindow)
  if (tmp->IsBlackForCC()) {
    if (tmp->mCachedXBLPrototypeHandlers.IsInitialized()) {
      tmp->mCachedXBLPrototypeHandlers.EnumerateRead(MarkXBLHandlers, nsnull);
    }
    nsEventListenerManager* elm = tmp->GetListenerManager(false);
    if (elm) {
      elm->UnmarkGrayJSListeners();
    }
    tmp->UnmarkGrayTimers();
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGlobalWindow)
  return tmp->IsBlackForCC();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGlobalWindow)
  return tmp->IsBlackForCC();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGlobalWindow)
  if (!cb.WantAllTraces() && tmp->IsBlackForCC()) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInnerWindowHolder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOuterWindow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOpenerScriptPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mListenerManager,
                                                  nsEventListenerManager)

  for (nsTimeout* timeout = tmp->FirstTimeout();
       tmp->IsTimeout(timeout);
       timeout = timeout->Next()) {
    cb.NoteNativeChild(timeout, &NS_CYCLE_COLLECTION_NAME(nsTimeout));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDoc)

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFrameElement)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFocusedNode)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mPendingStorageEvents)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindow)
  nsGlobalWindow::CleanupCachedXBLHandlers(tmp);

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInnerWindowHolder)
  if (tmp->mOuterWindow) {
    static_cast<nsGlobalWindow*>(tmp->mOuterWindow.get())->MaybeClearInnerWindow(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOuterWindow)
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOpenerScriptPrincipal)
  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListenerManager)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDoc)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFrameElement)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFocusedNode)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mPendingStorageEvents)

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

struct TraceData
{
  TraceData(TraceCallback& aCallback, void* aClosure) :
    callback(aCallback), closure(aClosure) {}

  TraceCallback& callback;
  void* closure;
};

static PLDHashOperator
TraceXBLHandlers(nsXBLPrototypeHandler* aKey, JSObject* aData, void* aClosure)
{
  TraceData* data = static_cast<TraceData*>(aClosure);
  data->callback(aData, "Cached XBL prototype handler", data->closure);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindow)
  if (tmp->mCachedXBLPrototypeHandlers.IsInitialized()) {
    TraceData data(aCallback, aClosure);
    tmp->mCachedXBLPrototypeHandlers.EnumerateRead(TraceXBLHandlers, &data);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool
nsGlobalWindow::IsBlackForCC()
{
  return
    (mDoc &&
     nsCCUncollectableMarker::InGeneration(mDoc->GetMarkedCCGeneration())) ||
    (nsCCUncollectableMarker::sGeneration && IsBlack());
}

void
nsGlobalWindow::UnmarkGrayTimers()
{
  for (nsTimeout* timeout = FirstTimeout();
       timeout && IsTimeout(timeout);
       timeout = timeout->Next()) {
    if (timeout->mScriptHandler) {
      JSObject* o = timeout->mScriptHandler->GetScriptObject();
      xpc_UnmarkGrayObject(o);
    }
  }
}

//*****************************************************************************
// nsGlobalWindow::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindow::EnsureScriptEnvironment()
{
  FORWARD_TO_OUTER(EnsureScriptEnvironment, (), NS_ERROR_NOT_INITIALIZED);

  if (mJSObject) {
    return NS_OK;
  }

  NS_ASSERTION(!GetCurrentInnerWindowInternal(),
               "mJSObject is null, but we have an inner window?");

  nsCOMPtr<nsIScriptRuntime> scriptRuntime;
  nsresult rv = NS_GetJSRuntime(getter_AddRefs(scriptRuntime));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptContext> context = scriptRuntime->CreateContext();

  NS_ASSERTION(!mContext, "Will overwrite mContext!");

  // should probably assert the context is clean???
  context->WillInitializeContext();

  // We need point the context to the global window before initializing it
  // so that it can make various decisions properly.
  context->SetGlobalObject(this);

  rv = context->InitContext();
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsFrame()) {
    // This window is a [i]frame, don't bother GC'ing when the
    // frame's context is destroyed since a GC will happen when the
    // frameset or host document is destroyed anyway.

    context->SetGCOnDestruction(false);
  }

  mContext = context;
  return NS_OK;
}

nsIScriptContext *
nsGlobalWindow::GetScriptContext()
{
  FORWARD_TO_OUTER(GetScriptContext, (), nsnull);
  return mContext;
}

nsIScriptContext *
nsGlobalWindow::GetContext()
{
  FORWARD_TO_OUTER(GetContext, (), nsnull);

  // check GetContext is indeed identical to GetScriptContext()
  NS_ASSERTION(mContext == GetScriptContext(),
               "GetContext confused?");
  return mContext;
}

JSObject *
nsGlobalWindow::GetGlobalJSObject()
{
  return FastGetGlobalJSObject();
}

bool
nsGlobalWindow::WouldReuseInnerWindow(nsIDocument *aNewDocument)
{
  // We reuse the inner window when:
  // a. We are currently at our original document.
  // b. At least one of the following conditions are true:
  // -- We are not currently a content window (i.e., we're currently a chrome
  //    window).
  // -- The new document is the same as the old document. This means that we're
  //    getting called from document.open().
  // -- The new document has the same origin as what we have loaded right now.

  if (!mDoc || !aNewDocument) {
    return false;
  }

  if (!mDoc->IsInitialDocument()) {
    return false;
  }
  
  NS_ASSERTION(NS_IsAboutBlank(mDoc->GetDocumentURI()),
               "How'd this happen?");
  
  // Great, we're the original document, check for one of the other
  // conditions.
  if (mDoc == aNewDocument) {
    return true;
  }

  bool equal;
  if (NS_SUCCEEDED(mDoc->NodePrincipal()->Equals(aNewDocument->NodePrincipal(),
                                                 &equal)) &&
      equal) {
    // The origin is the same.
    return true;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

  if (treeItem) {
    PRInt32 itemType = nsIDocShellTreeItem::typeContent;
    treeItem->GetItemType(&itemType);

    // If we're a chrome window, then we want to reuse the inner window.
    return itemType == nsIDocShellTreeItem::typeChrome;
  }

  // No treeItem: don't reuse the current inner window.
  return false;
}

void
nsGlobalWindow::SetOpenerScriptPrincipal(nsIPrincipal* aPrincipal)
{
  FORWARD_TO_OUTER_VOID(SetOpenerScriptPrincipal, (aPrincipal));

  if (mDoc) {
    if (!mDoc->IsInitialDocument()) {
      // We have a document already, and it's not the original one.  Bail out.
      // Do NOT set mOpenerScriptPrincipal in this case, just to be safe.
      return;
    }

#ifdef DEBUG
    // We better have an about:blank document loaded at this point.  Otherwise,
    // something is really weird.
    nsCOMPtr<nsIURI> uri;
    mDoc->NodePrincipal()->GetURI(getter_AddRefs(uri));
    NS_ASSERTION(uri && NS_IsAboutBlank(uri) &&
                 NS_IsAboutBlank(mDoc->GetDocumentURI()),
                 "Unexpected original document");
#endif

    GetDocShell()->CreateAboutBlankContentViewer(aPrincipal);
    mDoc->SetIsInitialDocument(true);

    nsCOMPtr<nsIPresShell> shell;
    GetDocShell()->GetPresShell(getter_AddRefs(shell));

    if (shell && !shell->DidInitialReflow()) {
      // Ensure that if someone plays with this document they will get
      // layout happening.
      nsRect r = shell->GetPresContext()->GetVisibleArea();
      shell->InitialReflow(r.width, r.height);
    }
  }
}

nsIPrincipal*
nsGlobalWindow::GetOpenerScriptPrincipal()
{
  FORWARD_TO_OUTER(GetOpenerScriptPrincipal, (), nsnull);

  return mOpenerScriptPrincipal;
}

PopupControlState
PushPopupControlState(PopupControlState aState, bool aForce)
{
  PopupControlState oldState = gPopupControlState;

  if (aState < gPopupControlState || aForce) {
    gPopupControlState = aState;
  }

  return oldState;
}

void
PopPopupControlState(PopupControlState aState)
{
  gPopupControlState = aState;
}

PopupControlState
nsGlobalWindow::PushPopupControlState(PopupControlState aState,
                                      bool aForce) const
{
  return ::PushPopupControlState(aState, aForce);
}

void
nsGlobalWindow::PopPopupControlState(PopupControlState aState) const
{
  ::PopPopupControlState(aState);
}

PopupControlState
nsGlobalWindow::GetPopupControlState() const
{
  return gPopupControlState;
}

#define WINDOWSTATEHOLDER_IID \
{0x0b917c3e, 0xbd50, 0x4683, {0xaf, 0xc9, 0xc7, 0x81, 0x07, 0xae, 0x33, 0x26}}

class WindowStateHolder : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  WindowStateHolder(nsGlobalWindow *aWindow,
                    nsIXPConnectJSObjectHolder *aHolder,
                    nsIXPConnectJSObjectHolder *aOuterProto,
                    nsIXPConnectJSObjectHolder *aOuterRealProto);

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }
  nsIXPConnectJSObjectHolder *GetInnerWindowHolder()
  { return mInnerWindowHolder; }

  nsIXPConnectJSObjectHolder* GetOuterProto() { return mOuterProto; }
  nsIXPConnectJSObjectHolder* GetOuterRealProto() { return mOuterRealProto; }

  void DidRestoreWindow()
  {
    mInnerWindow = nsnull;

    mInnerWindowHolder = nsnull;
    mOuterProto = nsnull;
    mOuterRealProto = nsnull;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindow *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  nsCOMPtr<nsIXPConnectJSObjectHolder> mInnerWindowHolder;
  nsCOMPtr<nsIXPConnectJSObjectHolder> mOuterProto;
  nsCOMPtr<nsIXPConnectJSObjectHolder> mOuterRealProto;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow *aWindow,
                                     nsIXPConnectJSObjectHolder *aHolder,
                                     nsIXPConnectJSObjectHolder *aOuterProto,
                                     nsIXPConnectJSObjectHolder *aOuterRealProto)
  : mInnerWindow(aWindow),
    mOuterProto(aOuterProto),
    mOuterRealProto(aOuterRealProto)
{
  NS_PRECONDITION(aWindow, "null window");
  NS_PRECONDITION(aWindow->IsInnerWindow(), "Saving an outer window");

  mInnerWindowHolder = aHolder;

  aWindow->SuspendTimeouts();
}

WindowStateHolder::~WindowStateHolder()
{
  if (mInnerWindow) {
    // This window was left in the bfcache and is now going away. We need to
    // free it up.
    // Note that FreeInnerObjects may already have been called on the
    // inner window if its outer has already had SetDocShell(null)
    // called.
    mInnerWindow->FreeInnerObjects();
  }
}

NS_IMPL_ISUPPORTS1(WindowStateHolder, WindowStateHolder)

nsresult
nsGlobalWindow::CreateOuterObject(nsGlobalWindow* aNewInner)
{
  JSContext* cx = mContext->GetNativeContext();

  if (IsChromeWindow()) {
    // Always enable E4X for XUL and other chrome content -- there is no
    // need to preserve the <!-- script hiding hack from JS-in-HTML daze
    // (introduced in 1995 for graceful script degradation in Netscape 1,
    // Mosaic, and other pre-JS browsers).
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_XML);
  }

  JSObject* outer = NewOuterWindowProxy(cx, aNewInner->FastGetGlobalJSObject());
  if (!outer) {
    return NS_ERROR_FAILURE;
  }

  js::SetProxyExtra(outer, 0,
    js::PrivateValue(static_cast<nsIScriptGlobalObject*>(this)));

  return SetOuterObject(cx, outer);
}

nsresult
nsGlobalWindow::SetOuterObject(JSContext* aCx, JSObject* aOuterObject)
{
  // Force our context's global object to be the outer.
  // NB: JS_SetGlobalObject sets aCx->compartment.
  JS_SetGlobalObject(aCx, aOuterObject);

  // Set up the prototype for the outer object.
  JSObject* inner = JS_GetParent(aOuterObject);
  JS_SetPrototype(aCx, aOuterObject, JS_GetPrototype(inner));

  return NS_OK;
}

nsresult
nsGlobalWindow::SetNewDocument(nsIDocument* aDocument,
                               nsISupports* aState,
                               bool aForceReuseInnerWindow)
{
  NS_TIME_FUNCTION;

  NS_PRECONDITION(mDocumentPrincipal == nsnull,
                  "mDocumentPrincipal prematurely set!");

  if (!aDocument) {
    NS_ERROR("SetNewDocument(null) called!");

    return NS_ERROR_INVALID_ARG;
  }

  if (IsInnerWindow()) {
    if (!mOuterWindow) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // Refuse to set a new document if the call came from an inner
    // window that's not the current inner window.
    if (mOuterWindow->GetCurrentInnerWindow() != this) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return GetOuterWindowInternal()->SetNewDocument(aDocument, aState,
                                                    aForceReuseInnerWindow);
  }

  NS_PRECONDITION(IsOuterWindow(), "Must only be called on outer windows");

  if (IsFrozen()) {
    // This outer is now getting its first inner, thaw the outer now
    // that it's ready and is getting an inner window.

    Thaw();
  }

  NS_ASSERTION(!GetCurrentInnerWindow() ||
               GetCurrentInnerWindow()->GetExtantDocument() == mDocument,
               "Uh, mDocument doesn't match the current inner window "
               "document!");

  bool wouldReuseInnerWindow = WouldReuseInnerWindow(aDocument);
  if (aForceReuseInnerWindow &&
      !wouldReuseInnerWindow &&
      mDoc &&
      mDoc->NodePrincipal() != aDocument->NodePrincipal()) {
    NS_ERROR("Attempted forced inner window reuse while changing principal");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDocument> oldDoc(do_QueryInterface(mDocument));

  nsIScriptContext *scx = GetContextInternal();
  NS_ENSURE_TRUE(scx, NS_ERROR_NOT_INITIALIZED);

  JSContext *cx = scx->GetNativeContext();
#ifndef MOZ_DISABLE_DOMCRYPTO
  // clear smartcard events, our document has gone away.
  if (mCrypto) {
    mCrypto->SetEnableSmartCardEvents(false);
  }
#endif
  if (!mDocument) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsIDOMWindow* privateRoot = nsGlobalWindow::GetPrivateRoot();

    if (privateRoot == static_cast<nsIDOMWindow*>(this)) {
      nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
      if (xblService) {
        xblService->AttachGlobalKeyHandler(mChromeEventHandler);
      }
    }
  }

  /* No mDocShell means we're already been partially closed down.  When that
     happens, setting status isn't a big requirement, so don't. (Doesn't happen
     under normal circumstances, but bug 49615 describes a case.) */

  nsContentUtils::AddScriptRunner(
    NS_NewRunnableMethod(this, &nsGlobalWindow::ClearStatus));

  bool reUseInnerWindow = aForceReuseInnerWindow || wouldReuseInnerWindow;

  nsresult rv = NS_OK;

  // Set mDocument even if this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.
  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  mContext->WillInitializeContext();

  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

  nsRefPtr<nsGlobalWindow> newInnerWindow;
  bool createdInnerWindow = false;

  bool thisChrome = IsChromeWindow();

  bool isChrome = false;

  nsCxPusher cxPusher;
  if (!cxPusher.Push(cx)) {
    return NS_ERROR_FAILURE;
  }

  XPCAutoRequest ar(cx);

  nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
  NS_ASSERTION(!aState || wsh, "What kind of weird state are you giving me here?");

  if (reUseInnerWindow) {
    // We're reusing the current inner window.
    NS_ASSERTION(!currentInner->IsFrozen(),
                 "We should never be reusing a shared inner window");
    newInnerWindow = currentInner;

    if (aDocument != oldDoc) {
      xpc_UnmarkGrayObject(currentInner->mJSObject);
      nsWindowSH::InvalidateGlobalScopePolluter(cx, currentInner->mJSObject);
    }

    // We're reusing the inner window, but this still counts as a navigation,
    // so all expandos and such defined on the outer window should go away. Force
    // all Xray wrappers to be recomputed.
    xpc_UnmarkGrayObject(mJSObject);
    if (!JS_RefreshCrossCompartmentWrappers(cx, mJSObject)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    if (aState) {
      newInnerWindow = wsh->GetInnerWindow();
      mInnerWindowHolder = wsh->GetInnerWindowHolder();

      NS_ASSERTION(newInnerWindow, "Got a state without inner window");
    } else if (thisChrome) {
      newInnerWindow = new nsGlobalChromeWindow(this);
      isChrome = true;
    } else if (mIsModalContentWindow) {
      newInnerWindow = new nsGlobalModalWindow(this);
    } else {
      newInnerWindow = new nsGlobalWindow(this);
    }

    if (!aState) {
      // This is redundant if we're restoring from a previous inner window.
      nsIScriptGlobalObject *sgo =
        (nsIScriptGlobalObject *)newInnerWindow.get();

      // Freeze the outer window and null out the inner window so
      // that initializing classes on the new inner doesn't end up
      // reaching into the old inner window for classes etc.
      //
      // [This happens with Object.prototype when XPConnect creates
      // a temporary global while initializing classes; the reason
      // being that xpconnect creates the temp global w/o a parent
      // and proto, which makes the JS engine look up classes in
      // cx->globalObject, i.e. this outer window].

      mInnerWindow = nsnull;

      Freeze();
      mCreatingInnerWindow = true;
      // Every script context we are initialized with must create a
      // new global.
      nsCOMPtr<nsIXPConnectJSObjectHolder> &holder = mInnerWindowHolder;
      rv = mContext->CreateNativeGlobalForInner(sgo, isChrome,
                                                aDocument->NodePrincipal(),
                                                &newInnerWindow->mJSObject,
                                                getter_AddRefs(holder));
      NS_ASSERTION(NS_SUCCEEDED(rv) && newInnerWindow->mJSObject && holder,
                   "Failed to get script global and holder");

      mCreatingInnerWindow = false;
      createdInnerWindow = true;
      Thaw();

      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (currentInner && currentInner->mJSObject) {
      if (oldDoc == aDocument) {
        // Move the navigator from the old inner window to the new one since
        // this is a document.write. This is safe from a same-origin point of
        // view because document.write can only be used by the same origin.
        newInnerWindow->mNavigator = currentInner->mNavigator;
        currentInner->mNavigator = nsnull;
        if (newInnerWindow->mNavigator) {
          newInnerWindow->mNavigator->SetWindow(newInnerWindow);
        }
      }

      // Don't free objects on our current inner window if it's going to be
      // held in the bfcache.
      if (!currentInner->IsFrozen()) {
        currentInner->FreeInnerObjects();
      }
    }

    mInnerWindow = newInnerWindow;

    if (!mJSObject) {
      CreateOuterObject(newInnerWindow);
      mContext->DidInitializeContext();

      mJSObject = mContext->GetNativeGlobal();
      SetWrapper(mJSObject);
    } else {
      JSObject *outerObject = NewOuterWindowProxy(cx, xpc_UnmarkGrayObject(newInnerWindow->mJSObject));
      if (!outerObject) {
        NS_ERROR("out of memory");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyExtra(mJSObject, 0, js::PrivateValue(NULL));

      outerObject = JS_TransplantObject(cx, mJSObject, outerObject);
      if (!outerObject) {
        NS_ERROR("unable to transplant wrappers, probably OOM");
        return NS_ERROR_FAILURE;
      }

      nsIScriptGlobalObject *global = static_cast<nsIScriptGlobalObject*>(this);
      js::SetProxyExtra(outerObject, 0, js::PrivateValue(global));

      mJSObject = outerObject;
      SetWrapper(mJSObject);

      {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, mJSObject)) {
          NS_ERROR("unable to enter a compartment");
          return NS_ERROR_FAILURE;
        }

        JS_SetParent(cx, mJSObject, newInnerWindow->mJSObject);

        SetOuterObject(cx, mJSObject);

        JSCompartment *compartment = js::GetObjectCompartment(mJSObject);
        xpc::CompartmentPrivate *priv =
          static_cast<xpc::CompartmentPrivate*>(JS_GetCompartmentPrivate(compartment));
        if (priv && priv->waiverWrapperMap) {
          NS_ASSERTION(!JS_IsExceptionPending(cx),
                       "We might overwrite a pending exception!");
          priv->waiverWrapperMap->Reparent(cx, newInnerWindow->mJSObject);
        }
      }
    }

    // Enter the new global's compartment.
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, mJSObject)) {
      NS_ERROR("unable to enter a compartment");
      return NS_ERROR_FAILURE;
    }

    // If we created a new inner window above, we need to do the last little bit
    // of initialization now that the dust has settled.
    if (createdInnerWindow) {
      nsIXPConnect *xpc = nsContentUtils::XPConnect();
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      nsresult rv = xpc->GetWrappedNativeOfJSObject(cx, newInnerWindow->mJSObject,
                                                    getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ABORT_IF_FALSE(wrapper, "bad wrapper");
      rv = wrapper->FinishInitForWrappedGlobal();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX Not sure if this is needed.
    if (aState) {
      JSObject *proto;
      if (nsIXPConnectJSObjectHolder *holder = wsh->GetOuterRealProto()) {
        holder->GetJSObject(&proto);
      } else {
        proto = nsnull;
      }

      if (!JS_SetPrototype(cx, mJSObject, xpc_UnmarkGrayObject(proto))) {
        NS_ERROR("can't set prototype");
        return NS_ERROR_FAILURE;
      }
    } else {
      if (!JS_DefineProperty(cx, newInnerWindow->mJSObject, "window",
                             OBJECT_TO_JSVAL(mJSObject),
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
        NS_ERROR("can't create the 'window' property");
        return NS_ERROR_FAILURE;
      }
    }
  }

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, mJSObject)) {
    NS_ERROR("unable to enter a compartment");
    return NS_ERROR_FAILURE;
  }

  if (!aState && !reUseInnerWindow) {
    // Loading a new page and creating a new inner window, *not*
    // restoring from session history.

    // Now that both the the inner and outer windows are initialized
    // let the script context do its magic to hook them together.
#ifdef DEBUG
    JSObject* newInnerJSObject = newInnerWindow->FastGetGlobalJSObject();
#endif

    // Now that we're connecting the outer global to the inner one,
    // we must have transplanted it. The JS engine tries to maintain
    // the global object's compartment as its default compartment,
    // so update that now since it might have changed.
    JS_SetGlobalObject(cx, mJSObject);
    NS_ASSERTION(JS_GetPrototype(mJSObject) ==
                 JS_GetPrototype(newInnerJSObject),
                 "outer and inner globals should have the same prototype");

    nsCOMPtr<nsIContent> frame = do_QueryInterface(GetFrameElementInternal());
    if (frame) {
      nsPIDOMWindow* parentWindow = frame->OwnerDoc()->GetWindow();
      if (parentWindow && parentWindow->TimeoutSuspendCount()) {
        SuspendTimeouts(parentWindow->TimeoutSuspendCount());
      }
    }
  }

  // Add an extra ref in case we release mContext during GC.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(mContext);

  // Now that the prototype is all set up, install the global scope
  // polluter. This must happen after the above prototype fixup. If
  // the GSP was to be installed on the inner window's real
  // prototype (as it would be if this was done before the prototype
  // fixup above) we would end up holding the GSP alive (through
  // XPConnect's internal marking of wrapper prototypes) as long as
  // the inner window was around, and if the GSP had properties on
  // it that held an element alive we'd hold the document alive,
  // which could hold event handlers alive, which hold the context
  // alive etc.

  if ((!reUseInnerWindow || aDocument != oldDoc) && !aState) {
    nsCOMPtr<nsIHTMLDocument> html_doc(do_QueryInterface(mDocument));
    nsWindowSH::InstallGlobalScopePolluter(cx, newInnerWindow->mJSObject,
                                           html_doc);
  }

  if (aDocument) {
    aDocument->SetScriptGlobalObject(newInnerWindow);
  }

  if (!aState) {
    if (reUseInnerWindow) {
      if (newInnerWindow->mDoc != aDocument) {
        newInnerWindow->mDocument = do_QueryInterface(aDocument);
        newInnerWindow->mDoc = aDocument;

        // We're reusing the inner window for a new document. In this
        // case we don't clear the inner window's scope, but we must
        // make sure the cached document property gets updated.

        // XXXmarkh - tell other languages about this?
        ::JS_DeleteProperty(cx, currentInner->mJSObject, "document");
      }
    } else {
      rv = newInnerWindow->InnerSetNewDocument(aDocument);
      NS_ENSURE_SUCCESS(rv, rv);

      // Initialize DOM classes etc on the inner window.
      rv = mContext->InitClasses(newInnerWindow->mJSObject);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mArguments) {
      newInnerWindow->DefineArgumentsProperty(mArguments);
      newInnerWindow->mArguments = mArguments;
      newInnerWindow->mArgumentsOrigin = mArgumentsOrigin;

      mArguments = nsnull;
      mArgumentsOrigin = nsnull;
    }

    // Give the new inner window our chrome event handler (since it
    // doesn't have one).
    newInnerWindow->mChromeEventHandler = mChromeEventHandler;
  }

  mContext->GC(js::gcreason::SET_NEW_DOCUMENT);
  mContext->DidInitializeContext();

  if (newInnerWindow && !newInnerWindow->mHasNotifiedGlobalCreated && mDoc) {
    // We should probably notify. However if this is the, arguably bad,
    // situation when we're creating a temporary non-chrome-about-blank
    // document in a chrome docshell, don't notify just yet. Instead wait
    // until we have a real chrome doc.
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));
    PRInt32 itemType = nsIDocShellTreeItem::typeContent;
    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (itemType != nsIDocShellTreeItem::typeChrome ||
        nsContentUtils::IsSystemPrincipal(mDoc->NodePrincipal())) {
      newInnerWindow->mHasNotifiedGlobalCreated = true;
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsGlobalWindow::DispatchDOMWindowCreated));
    }
  }

  return NS_OK;
}

void
nsGlobalWindow::DispatchDOMWindowCreated()
{
  if (!mDoc || !mDocument) {
    return;
  }

  // Fire DOMWindowCreated at chrome event listeners
  nsContentUtils::DispatchChromeEvent(mDoc, mDocument, NS_LITERAL_STRING("DOMWindowCreated"),
                                      true /* bubbles */,
                                      false /* not cancellable */);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    nsAutoString origin;
    nsIPrincipal* principal = mDoc->NodePrincipal();
    nsContentUtils::GetUTFOrigin(principal, origin);
    observerService->
      NotifyObservers(static_cast<nsIDOMWindow*>(this),
                      nsContentUtils::IsSystemPrincipal(principal) ?
                        "chrome-document-global-created" :
                        "content-document-global-created",
                      origin.get());
  }
}

void
nsGlobalWindow::ClearStatus()
{
  SetStatus(EmptyString());
  SetDefaultStatus(EmptyString());
}

nsresult
nsGlobalWindow::InnerSetNewDocument(nsIDocument* aDocument)
{
  NS_PRECONDITION(IsInnerWindow(), "Must only be called on inner windows");

#ifdef PR_LOGGING
  if (aDocument && gDOMLeakPRLog &&
      PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    nsCAutoString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOMWINDOW %p SetNewDocument %s", this, spec.get());
  }
#endif

  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;
  mFocusedNode = nsnull;
  mLocalStorage = nsnull;
  mSessionStorage = nsnull;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                        mMutationBits ? 1 : 0);

  // Clear our mutation bitfield.
  mMutationBits = 0;

  return NS_OK;
}

void
nsGlobalWindow::SetDocShell(nsIDocShell* aDocShell)
{
  NS_ASSERTION(IsOuterWindow(), "Uh, SetDocShell() called on inner window!");

  if (aDocShell == mDocShell)
    return;

  // SetDocShell(nsnull) means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // (mJSObject) so that it can be retrieved later (until it is
  // finalized by the JS GC).

  if (!aDocShell) {
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mTimeouts),
                 "Uh, outer window holds timeouts!");

    // Call FreeInnerObjects on all inner windows, not just the current
    // one, since some could be held by WindowStateHolder objects that
    // are GC-owned.
    for (nsRefPtr<nsGlobalWindow> inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
         inner != this;
         inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
      NS_ASSERTION(!inner->mOuterWindow || inner->mOuterWindow == this,
                   "bad outer window pointer");
      inner->FreeInnerObjects();
    }

    // Make sure that this is called before we null out the document.
    NotifyDOMWindowDestroyed(this);

    NotifyWindowIDDestroyed("outer-window-destroyed");

    nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

    if (currentInner) {
      NS_ASSERTION(mDoc, "Must have doc!");
      
      // Remember the document's principal.
      mDocumentPrincipal = mDoc->NodePrincipal();

      // Release our document reference
      mDocument = nsnull;
      mDoc = nsnull;
      mFocusedNode = nsnull;
    }

    ClearControllers();

    mChromeEventHandler = nsnull; // force release now

    if (mArguments) { 
      // We got no new document after someone called
      // SetArguments(), drop our reference to the arguments.
      mArguments = nsnull;
      mArgumentsLast = nsnull;
      mArgumentsOrigin = nsnull;
    }

    if (mContext) {
      mContext->GC(js::gcreason::SET_DOC_SHELL);
      mContext = nsnull;
    }

#ifdef DEBUG
    nsCycleCollector_DEBUG_shouldBeFreed(mContext);
    nsCycleCollector_DEBUG_shouldBeFreed(static_cast<nsIScriptGlobalObject*>(this));
#endif
  }

  mDocShell = aDocShell;        // Weak Reference

  NS_ASSERTION(!mNavigator, "Non-null mNavigator in outer window!");

  if (mFrames)
    mFrames->SetDocShell(aDocShell);

  if (!mDocShell) {
    MaybeForgiveSpamCount();
    CleanUp(false);
  } else {
    // Get our enclosing chrome shell and retrieve its global window impl, so
    // that we can do some forwarding to the chrome document.
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
    mDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
    mChromeEventHandler = do_QueryInterface(chromeEventHandler);
    if (!mChromeEventHandler) {
      // We have no chrome event handler. If we have a parent,
      // get our chrome event handler from the parent. If
      // we don't have a parent, then we need to make a new
      // window root object that will function as a chrome event
      // handler and receive all events that occur anywhere inside
      // our window.
      nsCOMPtr<nsIDOMWindow> parentWindow;
      GetParent(getter_AddRefs(parentWindow));
      if (parentWindow.get() != static_cast<nsIDOMWindow*>(this)) {
        nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(parentWindow));
        mChromeEventHandler = piWindow->GetChromeEventHandler();
      }
      else NS_NewWindowRoot(this, getter_AddRefs(mChromeEventHandler));
    }

    bool docShellActive;
    mDocShell->GetIsActive(&docShellActive);
    mIsBackground = !docShellActive;
  }
}

void
nsGlobalWindow::SetOpenerWindow(nsIDOMWindow* aOpener,
                                bool aOriginalOpener)
{
  FORWARD_TO_OUTER_VOID(SetOpenerWindow, (aOpener, aOriginalOpener));

  NS_ASSERTION(!aOriginalOpener || !mSetOpenerWindowCalled,
               "aOriginalOpener is true, but not first call to "
               "SetOpenerWindow!");
  NS_ASSERTION(aOpener || !aOriginalOpener,
               "Shouldn't set mHadOriginalOpener if aOpener is null");

  mOpener = do_GetWeakReference(aOpener);
  NS_ASSERTION(mOpener || !aOpener, "Opener must support weak references!");

  if (aOriginalOpener) {
    mHadOriginalOpener = true;
  }

#ifdef DEBUG
  mSetOpenerWindowCalled = true;
#endif
}

static
already_AddRefed<nsIDOMEventTarget>
TryGetTabChildGlobalAsEventTarget(nsISupports *aFrom)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(aFrom);
  if (!frameLoaderOwner) {
    return NULL;
  }

  nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  if (!frameLoader) {
    return NULL;
  }

  nsCOMPtr<nsIDOMEventTarget> eventTarget =
    frameLoader->GetTabChildGlobalAsEventTarget();
  return eventTarget.forget();
}

void
nsGlobalWindow::UpdateParentTarget()
{
  // Try to get our frame element's tab child global (its in-process message
  // manager).  If that fails, fall back to the chrome event handler's tab
  // child global, and if it doesn't have one, just use the chrome event
  // handler itself.

  nsCOMPtr<nsIDOMElement> frameElement = GetFrameElementInternal();
  nsCOMPtr<nsIDOMEventTarget> eventTarget =
    TryGetTabChildGlobalAsEventTarget(frameElement);

  if (!eventTarget) {
    eventTarget = TryGetTabChildGlobalAsEventTarget(mChromeEventHandler);
  }

  if (!eventTarget) {
    eventTarget = mChromeEventHandler;
  }

  mParentTarget = eventTarget;
}

bool
nsGlobalWindow::GetIsTabModalPromptAllowed()
{
  bool allowTabModal = true;
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    cv->GetIsTabModalPromptAllowed(&allowTabModal);
  }

  return allowTabModal;
}

nsIDOMEventTarget*
nsGlobalWindow::GetTargetForDOMEvent()
{
  return static_cast<nsIDOMEventTarget*>(GetOuterWindowInternal());
}

nsIDOMEventTarget*
nsGlobalWindow::GetTargetForEventTargetChain()
{
  return IsInnerWindow() ?
    this : static_cast<nsIDOMEventTarget*>(GetCurrentInnerWindowInternal());
}

nsresult
nsGlobalWindow::WillHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

JSContext*
nsGlobalWindow::GetJSContextForEventHandlers()
{
  return nsnull;
}

nsresult
nsGlobalWindow::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PreHandleEvent is used on outer window!?");
  static PRUint32 count = 0;
  PRUint32 msg = aVisitor.mEvent->message;

  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  if ((msg == NS_MOUSE_MOVE) && gEntropyCollector) {
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case. Means we get a little
    //more entropy.
    if (count++ % 100 == 0) {
      //Since the high bits seem to be zero's most of the time,
      //let's only take the lowest half of the point structure.
      PRInt16 myCoord[2];

      myCoord[0] = aVisitor.mEvent->refPoint.x;
      myCoord[1] = aVisitor.mEvent->refPoint.y;
      gEntropyCollector->RandomUpdate((void*)myCoord, sizeof(myCoord));
      gEntropyCollector->RandomUpdate((void*)&(aVisitor.mEvent->time),
                                      sizeof(PRUint32));
    }
  } else if (msg == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = true;
  } else if (msg == NS_MOUSE_BUTTON_DOWN &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    gMouseDown = true;
  } else if ((msg == NS_MOUSE_BUTTON_UP ||
              msg == NS_DRAGDROP_END) &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    gMouseDown = false;
    if (gDragServiceDisabled) {
      nsCOMPtr<nsIDragService> ds =
        do_GetService("@mozilla.org/widget/dragservice;1");
      if (ds) {
        gDragServiceDisabled = false;
        ds->Unsuppress();
      }
    }
  }

  aVisitor.mParentTarget = GetParentTarget();
  return NS_OK;
}

bool
nsGlobalWindow::DialogOpenAttempted()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ERROR("DialogOpenAttempted() called without a top window?");

    return false;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (!topWindow ||
      topWindow->mLastDialogQuitTime.IsNull() ||
      nsContentUtils::CallerHasUniversalXPConnect()) {
    return false;
  }

  TimeDuration dialogDuration(TimeStamp::Now() -
                              topWindow->mLastDialogQuitTime);

  if (dialogDuration.ToSeconds() <
        Preferences::GetInt("dom.successive_dialog_time_limit",
                            SUCCESSIVE_DIALOG_TIME_LIMIT)) {
    topWindow->mDialogAbuseCount++;

    return (topWindow->GetPopupControlState() > openAllowed ||
            topWindow->mDialogAbuseCount > MAX_DIALOG_COUNT);
  }

  topWindow->mDialogAbuseCount = 0;

  return false;
}

bool
nsGlobalWindow::AreDialogsBlocked()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ASSERTION(!mDocShell, "AreDialogsBlocked() called without a top window?");

    return true;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();

  return !topWindow ||
         (topWindow->mDialogDisabled &&
          (topWindow->GetPopupControlState() > openAllowed ||
           topWindow->mDialogAbuseCount >= MAX_DIALOG_COUNT));
}

bool
nsGlobalWindow::ConfirmDialogAllowed()
{
  FORWARD_TO_OUTER(ConfirmDialogAllowed, (), false);

  NS_ENSURE_TRUE(mDocShell, false);
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1");

  if (!DialogOpenAttempted() || !promptSvc) {
    return true;
  }

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  bool disableDialog = false;
  nsXPIDLString label, title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogLabel", label);
  nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                     "ScriptDialogPreventTitle", title);
  promptSvc->Confirm(this, title.get(), label.get(), &disableDialog);
  if (disableDialog) {
    PreventFurtherDialogs();
    return false;
  }

  return true;
}

void
nsGlobalWindow::PreventFurtherDialogs()
{
  nsGlobalWindow *topWindow = GetTop();
  if (!topWindow) {
    NS_ERROR("PreventFurtherDialogs() called without a top window?");

    return;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();

  if (topWindow)
    topWindow->mDialogDisabled = true;
}

nsresult
nsGlobalWindow::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PostHandleEvent is used on outer window!?");

  // Return early if there is nothing to do.
  switch (aVisitor.mEvent->message) {
    case NS_RESIZE_EVENT:
    case NS_PAGE_UNLOAD:
    case NS_LOAD:
      break;
    default:
      return NS_OK;
  }

  /* mChromeEventHandler and mContext go dangling in the middle of this
   function under some circumstances (events that destroy the window)
   without this addref. */
  nsCOMPtr<nsIDOMEventTarget> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());

  if (aVisitor.mEvent->message == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = false;
  } else if (aVisitor.mEvent->message == NS_PAGE_UNLOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDocument) {
      NS_ASSERTION(mDoc, "Must have doc");
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->message == NS_LOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = true;

    nsCOMPtr<nsIContent> content(do_QueryInterface(GetFrameElementInternal()));
    nsCOMPtr<nsIDocShellTreeItem> treeItem =
      do_QueryInterface(GetDocShell());

    PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (content && GetParentInternal() &&
        itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_LOAD);
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.
      nsEventDispatcher::Dispatch(content, nsnull, &event, nsnull, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::DispatchDOMEvent(nsEvent* aEvent,
                                 nsIDOMEvent* aDOMEvent,
                                 nsPresContext* aPresContext,
                                 nsEventStatus* aEventStatus)
{
  return
    nsEventDispatcher::DispatchDOMEvent(static_cast<nsPIDOMWindow*>(this),
                                       aEvent, aDOMEvent, aPresContext,
                                       aEventStatus);
}

void
nsGlobalWindow::OnFinalize(JSObject* aObject)
{
  if (aObject == mJSObject) {
    mJSObject = NULL;
  }
}

void
nsGlobalWindow::SetScriptsEnabled(bool aEnabled, bool aFireTimeouts)
{
  FORWARD_TO_INNER_VOID(SetScriptsEnabled, (aEnabled, aFireTimeouts));

  if (aEnabled && aFireTimeouts) {
    // Scripts are enabled (again?) on this context, run timeouts that
    // fired on this context while scripts were disabled.
    void (nsGlobalWindow::*run)() = &nsGlobalWindow::RunTimeout;
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(this, run));
  }
}

nsresult
nsGlobalWindow::SetArguments(nsIArray *aArguments, nsIPrincipal *aOrigin)
{
  FORWARD_TO_OUTER(SetArguments, (aArguments, aOrigin),
                   NS_ERROR_NOT_INITIALIZED);

  // Hold on to the arguments so that we can re-set them once the next
  // document is loaded.
  mArguments = aArguments;
  mArgumentsOrigin = aOrigin;

  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

  if (!mIsModalContentWindow) {
    mArgumentsLast = aArguments;
  } else if (currentInner) {
    // SetArguments() is being called on a modal content window that
    // already has an inner window. This can happen when loading
    // javascript: URIs as modal content dialogs. In this case, we'll
    // set up the dialog window, both inner and outer, before we call
    // SetArguments() on the window, so to deal with that, make sure
    // here that the arguments are propagated to the inner window.

    currentInner->mArguments = aArguments;
    currentInner->mArgumentsOrigin = aOrigin;
  }

  return currentInner ?
    currentInner->DefineArgumentsProperty(aArguments) : NS_OK;
}

nsresult
nsGlobalWindow::DefineArgumentsProperty(nsIArray *aArguments)
{
  JSContext *cx;
  nsIScriptContext *ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx &&
                 (cx = ctx->GetNativeContext()),
                 NS_ERROR_NOT_INITIALIZED);

  if (mIsModalContentWindow) {
    // Modal content windows don't have an "arguments" property, they
    // have a "dialogArguments" property which is handled
    // separately. See nsWindowSH::NewResolve().

    return NS_OK;
  }

  return GetContextInternal()->SetProperty(mJSObject, "arguments", aArguments);
}

//*****************************************************************************
// nsGlobalWindow::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
nsGlobalWindow::GetPrincipal()
{
  if (mDoc) {
    // If we have a document, get the principal from the document
    return mDoc->NodePrincipal();
  }

  if (mDocumentPrincipal) {
    return mDocumentPrincipal;
  }

  // If we don't have a principal and we don't have a document we
  // ask the parent window for the principal. This can happen when
  // loading a frameset that has a <frame src="javascript:xxx">, in
  // that case the global window is used in JS before we've loaded
  // a document into the window.

  nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
    do_QueryInterface(GetParentInternal());

  if (objPrincipal) {
    return objPrincipal->GetPrincipal();
  }

  return nsnull;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindow
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocument** aDocument)
{
  // This method *should* forward calls to the outer window, but since
  // there's nothing here that *depends* on anything in the outer
  // (GetDocShell() eliminates that dependency), we won't do that to
  // avoid the extra virtual function call.

  // lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so. Note that domdoc here is the same
  // thing as our mDocument, but we don't have to explicitly set the
  // member variable because the docshell has already called
  // SetNewDocument().
  nsIDocShell *docShell;
  if (!mDocument && (docShell = GetDocShell()))
    nsCOMPtr<nsIDOMDocument> domdoc(do_GetInterface(docShell));

  NS_IF_ADDREF(*aDocument = mDocument);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetWindow(nsIDOMWindow** aWindow)
{
  FORWARD_TO_OUTER(GetWindow, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = static_cast<nsIDOMWindow*>(this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelf(nsIDOMWindow** aWindow)
{
  FORWARD_TO_OUTER(GetSelf, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = static_cast<nsIDOMWindow*>(this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetNavigator(nsIDOMNavigator** aNavigator)
{
  FORWARD_TO_INNER(GetNavigator, (aNavigator), NS_ERROR_NOT_INITIALIZED);

  *aNavigator = nsnull;

  if (!mNavigator) {
    mNavigator = new Navigator(this);
  }

  NS_ADDREF(*aNavigator = mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreen(nsIDOMScreen** aScreen)
{
  FORWARD_TO_INNER(GetScreen, (aScreen), NS_ERROR_NOT_INITIALIZED);

  *aScreen = nsnull;

  if (!mScreen) {
    mScreen = nsScreen::Create(this);
    if (!mScreen) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  NS_IF_ADDREF(*aScreen = mScreen);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetHistory(nsIDOMHistory** aHistory)
{
  FORWARD_TO_INNER(GetHistory, (aHistory), NS_ERROR_NOT_INITIALIZED);

  *aHistory = nsnull;

  if (!mHistory) {
    mHistory = new nsHistory(this);
    if (!mHistory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aHistory = mHistory);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPerformance(nsIDOMPerformance** aPerformance)
{
  FORWARD_TO_INNER(GetPerformance, (aPerformance), NS_ERROR_NOT_INITIALIZED);

  *aPerformance = nsnull;

  if (nsGlobalWindow::HasPerformanceSupport()) {
    if (!mPerformance) {
      if (!mDoc) {
        return NS_OK;
      }
      nsRefPtr<nsDOMNavigationTiming> timing = mDoc->GetNavigationTiming();
      nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(mDoc->GetChannel()));
      bool timingEnabled = false;
      if (!timedChannel ||
          !NS_SUCCEEDED(timedChannel->GetTimingEnabled(&timingEnabled)) ||
          !timingEnabled) {
        timedChannel = nsnull;
      }
      if (timing) {
        mPerformance = new nsPerformance(timing, timedChannel);
      }
    }
    NS_IF_ADDREF(*aPerformance = mPerformance);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetParent(nsIDOMWindow** aParent)
{
  FORWARD_TO_OUTER(GetParent, (aParent), NS_ERROR_NOT_INITIALIZED);

  *aParent = nsnull;
  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(parent));
    NS_ENSURE_SUCCESS(CallQueryInterface(globalObject.get(), aParent),
                      NS_ERROR_FAILURE);
  }
  else {
    *aParent = static_cast<nsIDOMWindow*>(this);
    NS_ADDREF(*aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetTop(nsIDOMWindow** aTop)
{
  FORWARD_TO_OUTER(GetTop, (aTop), NS_ERROR_NOT_INITIALIZED);

  *aTop = nsnull;
  if (mDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));

    if (root) {
      nsCOMPtr<nsIDOMWindow> top(do_GetInterface(root));
      top.swap(*aTop);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetContent(nsIDOMWindow** aContent)
{
  FORWARD_TO_OUTER(GetContent, (aContent), NS_ERROR_NOT_INITIALIZED);

  *aContent = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;

  if (!nsContentUtils::IsCallerChrome()) {
    // If we're called by non-chrome code, make sure we don't return
    // the primary content window if the calling tab is hidden. In
    // such a case we return the same-type root in the hidden tab,
    // which is "good enough", for now.
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));

    if (baseWin) {
      bool visible = false;
      baseWin->GetVisibility(&visible);

      if (!visible) {
        nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

        treeItem->GetSameTypeRootTreeItem(getter_AddRefs(primaryContent));
      }
    }
  }

  if (!primaryContent) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(primaryContent));
  NS_IF_ADDREF(*aContent = domWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPrompter(nsIPrompt** aPrompt)
{
  FORWARD_TO_OUTER(GetPrompter, (aPrompt), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMenubar(nsIDOMBarProp** aMenubar)
{
  FORWARD_TO_OUTER(GetMenubar, (aMenubar), NS_ERROR_NOT_INITIALIZED);

  *aMenubar = nsnull;

  if (!mMenubar) {
    mMenubar = new nsMenubarProp(this);
    if (!mMenubar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aMenubar = mMenubar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetToolbar(nsIDOMBarProp** aToolbar)
{
  FORWARD_TO_OUTER(GetToolbar, (aToolbar), NS_ERROR_NOT_INITIALIZED);

  *aToolbar = nsnull;

  if (!mToolbar) {
    mToolbar = new nsToolbarProp(this);
    if (!mToolbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aToolbar = mToolbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocationbar(nsIDOMBarProp** aLocationbar)
{
  FORWARD_TO_OUTER(GetLocationbar, (aLocationbar), NS_ERROR_NOT_INITIALIZED);

  *aLocationbar = nsnull;

  if (!mLocationbar) {
    mLocationbar = new nsLocationbarProp(this);
    if (!mLocationbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aLocationbar = mLocationbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
{
  FORWARD_TO_OUTER(GetPersonalbar, (aPersonalbar), NS_ERROR_NOT_INITIALIZED);

  *aPersonalbar = nsnull;

  if (!mPersonalbar) {
    mPersonalbar = new nsPersonalbarProp(this);
    if (!mPersonalbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aPersonalbar = mPersonalbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatusbar(nsIDOMBarProp** aStatusbar)
{
  FORWARD_TO_OUTER(GetStatusbar, (aStatusbar), NS_ERROR_NOT_INITIALIZED);

  *aStatusbar = nsnull;

  if (!mStatusbar) {
    mStatusbar = new nsStatusbarProp(this);
    if (!mStatusbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aStatusbar = mStatusbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  FORWARD_TO_OUTER(GetScrollbars, (aScrollbars), NS_ERROR_NOT_INITIALIZED);

  *aScrollbars = nsnull;

  if (!mScrollbars) {
    mScrollbars = new nsScrollbarsProp(this);
    if (!mScrollbars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aScrollbars = mScrollbars);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetClosed(bool* aClosed)
{
  FORWARD_TO_OUTER(GetClosed, (aClosed), NS_ERROR_NOT_INITIALIZED);

  // If someone called close(), or if we don't have a docshell, we're
  // closed.
  *aClosed = mIsClosed || !mDocShell;

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindowCollection** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = nsnull;

  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
    if (!mFrames) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aFrames = static_cast<nsIDOMWindowCollection *>(mFrames);
  NS_IF_ADDREF(*aFrames);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetApplicationCache(nsIDOMOfflineResourceList **aApplicationCache)
{
  FORWARD_TO_INNER(GetApplicationCache, (aApplicationCache), NS_ERROR_UNEXPECTED);

  NS_ENSURE_ARG_POINTER(aApplicationCache);

  if (!mApplicationCache) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetDocShell()));
    if (!webNav) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = webNav->GetCurrentURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    nsCOMPtr<nsIURI> manifestURI;
    nsContentUtils::GetOfflineAppManifest(doc, getter_AddRefs(manifestURI));

    nsRefPtr<nsDOMOfflineResourceList> applicationCache =
      new nsDOMOfflineResourceList(manifestURI, uri, this);
    NS_ENSURE_TRUE(applicationCache, NS_ERROR_OUT_OF_MEMORY);

    applicationCache->Init();

    mApplicationCache = applicationCache;
  }

  NS_IF_ADDREF(*aApplicationCache = mApplicationCache);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetCrypto(nsIDOMCrypto** aCrypto)
{
#ifdef MOZ_DISABLE_DOMCRYPTO
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  FORWARD_TO_OUTER(GetCrypto, (aCrypto), NS_ERROR_NOT_INITIALIZED);

  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoContractID);
  }

  NS_IF_ADDREF(*aCrypto = mCrypto);

  return NS_OK;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  *aPkcs11 = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetControllers(nsIControllers** aResult)
{
  FORWARD_TO_OUTER(GetControllers, (aResult), NS_ERROR_NOT_INITIALIZED);

  if (!mControllers) {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add in the default controller
    nsCOMPtr<nsIController> controller = do_CreateInstance(
                               NS_WINDOWCONTROLLER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mControllers->InsertControllerAt(0, controller);
    nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller);
    if (!controllerContext) return NS_ERROR_FAILURE;

    controllerContext->SetCommandContext(static_cast<nsIDOMWindow*>(this));
  }

  *aResult = mControllers;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOpener(nsIDOMWindow** aOpener)
{
  FORWARD_TO_OUTER(GetOpener, (aOpener), NS_ERROR_NOT_INITIALIZED);

  *aOpener = nsnull;

  nsCOMPtr<nsPIDOMWindow> opener = do_QueryReferent(mOpener);
  if (!opener) {
    return NS_OK;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::IsCallerTrustedForRead()) {
    NS_ADDREF(*aOpener = opener);
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> openerPwin(do_QueryInterface(opener));
  if (!openerPwin) {
    return NS_OK;
  }

  // First, ensure that we're not handing back a chrome window.
  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(openerPwin.get());
  if (win->IsChromeWindow()) {
    return NS_OK;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(openerPwin->GetDocShell());

  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    docShellAsItem->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      PRUint32 appType;
      nsresult rv = openerRootDocShell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
        *aOpener = opener;
      }
    }
  }

  NS_IF_ADDREF(*aOpener);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetOpener(nsIDOMWindow* aOpener)
{
  // check if we were called from a privileged chrome script.
  // If not, opener is settable only to null.
  if (aOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_OK;
  }

  SetOpenerWindow(aOpener, false);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatus(nsAString& aStatus)
{
  FORWARD_TO_OUTER(GetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetStatus(const nsAString& aStatus)
{
  FORWARD_TO_OUTER(SetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.status by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mStatus = aStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if(browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT,
                             PromiseFlatString(aStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetDefaultStatus(nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(GetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  aDefaultStatus = mDefaultStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetDefaultStatus(const nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(SetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.defaultStatus by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mDefaultStatus = aDefaultStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if (browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT_DEFAULT,
                             PromiseFlatString(aDefaultStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetName(nsAString& aName)
{
  FORWARD_TO_OUTER(GetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    docShellAsItem->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetName(const nsAString& aName)
{
  FORWARD_TO_OUTER(SetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsresult result = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    result = docShellAsItem->SetName(PromiseFlatString(aName).get());
  return result;
}

// Helper functions used by many methods below.
PRInt32
nsGlobalWindow::DevToCSSIntPixels(PRInt32 px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->DevPixelsToIntCSSPixels(px);
}

PRInt32
nsGlobalWindow::CSSToDevIntPixels(PRInt32 px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->CSSPixelsToDevPixels(px);
}

nsIntSize
nsGlobalWindow::DevToCSSIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
      presContext->DevPixelsToIntCSSPixels(px.width),
      presContext->DevPixelsToIntCSSPixels(px.height));
}

nsIntSize
nsGlobalWindow::CSSToDevIntPixels(nsIntSize px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return nsIntSize(
    presContext->CSSPixelsToDevPixels(px.width),
    presContext->CSSPixelsToDevPixels(px.height));
}


NS_IMETHODIMP
nsGlobalWindow::GetInnerWidth(PRInt32* aInnerWidth)
{
  FORWARD_TO_OUTER(GetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  EnsureSizeUpToDate();

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsRect shellArea = presContext->GetVisibleArea();
    *aInnerWidth = nsPresContext::AppUnitsToIntCSSPixels(shellArea.width);
  } else {
    *aInnerWidth = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerWidth(PRInt32 aInnerWidth)
{
  FORWARD_TO_OUTER(SetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerWidth by exiting early
   */
  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aInnerWidth, nsnull),
                    NS_ERROR_FAILURE);


  nsRefPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nscoord height = 0;
    nscoord width  = 0;

    nsRefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    height = shellArea.height;
    width  = nsPresContext::CSSPixelsToAppUnits(aInnerWidth);
    return SetCSSViewportWidthAndHeight(width, height);
  }
  else
  {
    PRInt32 height = 0;
    PRInt32 width  = 0;

    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
    docShellAsWin->GetSize(&width, &height);
    width  = CSSToDevIntPixels(aInnerWidth);
    return SetDocShellWidthAndHeight(width, height);
  }
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerHeight(PRInt32* aInnerHeight)
{
  FORWARD_TO_OUTER(GetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  EnsureSizeUpToDate();

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsRect shellArea = presContext->GetVisibleArea();
    *aInnerHeight = nsPresContext::AppUnitsToIntCSSPixels(shellArea.height);
  } else {
    *aInnerHeight = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerHeight(PRInt32 aInnerHeight)
{
  FORWARD_TO_OUTER(SetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerHeight by exiting early
   */
  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aInnerHeight),
                    NS_ERROR_FAILURE);

  nsRefPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nscoord height = 0;
    nscoord width  = 0;

    nsRefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    width = shellArea.width;
    height  = nsPresContext::CSSPixelsToAppUnits(aInnerHeight);
    return SetCSSViewportWidthAndHeight(width, height);
  }
  else
  {
    PRInt32 height = 0;
    PRInt32 width  = 0;

    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
    docShellAsWin->GetSize(&width, &height);
    height  = CSSToDevIntPixels(aInnerHeight);
    return SetDocShellWidthAndHeight(width, height);
  }
}

nsresult
nsGlobalWindow::GetOuterSize(nsIntSize* aSizeCSSPixels)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow *>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  nsIntSize sizeDevPixels;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&sizeDevPixels.width,
                                            &sizeDevPixels.height),
                    NS_ERROR_FAILURE);

  *aSizeCSSPixels = DevToCSSIntPixels(sizeDevPixels);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterWidth(PRInt32* aOuterWidth)
{
  FORWARD_TO_OUTER(GetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterWidth = sizeCSSPixels.width;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterHeight(PRInt32* aOuterHeight)
{
  FORWARD_TO_OUTER(GetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterHeight = sizeCSSPixels.height;
  return NS_OK;
}

nsresult
nsGlobalWindow::SetOuterSize(PRInt32 aLengthCSSPixels, bool aIsWidth)
{
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.outerWidth by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(
                        aIsWidth ? &aLengthCSSPixels : nsnull,
                        aIsWidth ? nsnull : &aLengthCSSPixels),
                    NS_ERROR_FAILURE);

  PRInt32 width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  PRInt32 lengthDevPixels = CSSToDevIntPixels(aLengthCSSPixels);
  if (aIsWidth) {
    width = lengthDevPixels;
  } else {
    height = lengthDevPixels;
  }
  return treeOwnerAsWin->SetSize(width, height, true);    
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterWidth(PRInt32 aOuterWidth)
{
  FORWARD_TO_OUTER(SetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterWidth, true);
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterHeight(PRInt32 aOuterHeight)
{
  FORWARD_TO_OUTER(SetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterHeight, false);
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenX(PRInt32* aScreenX)
{
  FORWARD_TO_OUTER(GetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenX = DevToCSSIntPixels(x);
  return NS_OK;
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
  if (!mDocShell)
    return nsRect();

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow*>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return nsRect();
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame)
    return nsRect();

  return rootFrame->GetScreenRectInAppUnits();
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenX(float* aScreenX)
{
  FORWARD_TO_OUTER(GetMozInnerScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsRect r = GetInnerScreenRect();
  *aScreenX = nsPresContext::AppUnitsToFloatCSSPixels(r.x);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenY(float* aScreenY)
{
  FORWARD_TO_OUTER(GetMozInnerScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsRect r = GetInnerScreenRect();
  *aScreenY = nsPresContext::AppUnitsToFloatCSSPixels(r.y);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozPaintCount(PRUint64* aResult)
{
  FORWARD_TO_OUTER(GetMozPaintCount, (aResult), NS_ERROR_NOT_INITIALIZED);

  *aResult = 0;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return NS_OK;

  *aResult = presShell->GetPaintCount();
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MozRequestAnimationFrame(nsIFrameRequestCallback* aCallback,
                                         PRInt32 *aHandle)
{
  FORWARD_TO_INNER(MozRequestAnimationFrame, (aCallback, aHandle),
                   NS_ERROR_NOT_INITIALIZED);

  if (!mDoc) {
    return NS_OK;
  }

  if (!aCallback) {
    mDoc->WarnOnceAbout(nsIDocument::eMozBeforePaint);
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  if (mJSObject)
    js::NotifyAnimationActivity(mJSObject);

  return mDoc->ScheduleFrameRequestCallback(aCallback, aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::MozCancelRequestAnimationFrame(PRInt32 aHandle)
{
  return MozCancelAnimationFrame(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::MozCancelAnimationFrame(PRInt32 aHandle)
{
  FORWARD_TO_INNER(MozCancelAnimationFrame, (aHandle),
                   NS_ERROR_NOT_INITIALIZED);

  if (!mDoc) {
    return NS_OK;
  }

  mDoc->CancelFrameRequestCallback(aHandle);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozAnimationStartTime(PRInt64 *aTime)
{
  FORWARD_TO_INNER(GetMozAnimationStartTime, (aTime), NS_ERROR_NOT_INITIALIZED);

  if (mDoc) {
    nsIPresShell* presShell = mDoc->GetShell();
    if (presShell) {
      *aTime = presShell->GetPresContext()->RefreshDriver()->
        MostRecentRefreshEpochTime() / PR_USEC_PER_MSEC;
      return NS_OK;
    }
  }

  // If all else fails, just be compatible with Date.now()
  *aTime = JS_Now() / PR_USEC_PER_MSEC;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MatchMedia(const nsAString& aMediaQueryList,
                           nsIDOMMediaQueryList** aResult)
{
  // FIXME: This whole forward-to-outer and then get a pres
  // shell/context off the docshell dance is sort of silly; it'd make
  // more sense to forward to the inner, but it's what everyone else
  // (GetSelection, GetScrollXY, etc.) does around here.
  FORWARD_TO_OUTER(MatchMedia, (aMediaQueryList, aResult),
                   NS_ERROR_NOT_INITIALIZED);

  *aResult = nsnull;

  if (!mDocShell)
    return NS_OK;

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (!presContext)
    return NS_OK;

  presContext->MatchMedia(aMediaQueryList, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenX(PRInt32 aScreenX)
{
  FORWARD_TO_OUTER(SetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenX by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aScreenX, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  x = CSSToDevIntPixels(aScreenX);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenY(PRInt32* aScreenY)
{
  FORWARD_TO_OUTER(GetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenY = DevToCSSIntPixels(y);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenY(PRInt32 aScreenY)
{
  FORWARD_TO_OUTER(SetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenY by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(nsnull, &aScreenY),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  y = CSSToDevIntPixels(aScreenY);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
nsresult
nsGlobalWindow::CheckSecurityWidthAndHeight(PRInt32* aWidth, PRInt32* aHeight)
{
#ifdef MOZ_XUL
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    // if attempting to resize the window, hide any open popups
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsContentUtils::HidePopupsInDocument(doc);
  }
#endif

  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (!nsContentUtils::IsCallerTrustedForWrite()) {
      //sec check failed
      if (aWidth && *aWidth < 100) {
        *aWidth = 100;
      }
      if (aHeight && *aHeight < 100) {
        *aHeight = 100;
      }
    }
  }

  return NS_OK;
}

// NOTE: Arguments to this function should have values in device pixels
nsresult
nsGlobalWindow::SetDocShellWidthAndHeight(PRInt32 aInnerWidth, PRInt32 aInnerHeight)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, aInnerWidth, aInnerHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

// NOTE: Arguments to this function should have values in app units
nsresult
nsGlobalWindow::SetCSSViewportWidthAndHeight(nscoord aInnerWidth, nscoord aInnerHeight)
{
  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  nsRect shellArea = presContext->GetVisibleArea();
  shellArea.height = aInnerHeight;
  shellArea.width = aInnerWidth;

  presContext->SetVisibleArea(shellArea);
  return NS_OK;
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
nsresult
nsGlobalWindow::CheckSecurityLeftAndTop(PRInt32* aLeft, PRInt32* aTop)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!nsContentUtils::IsCallerTrustedForWrite()) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsContentUtils::HidePopupsInDocument(doc);
#endif

    nsGlobalWindow* rootWindow =
      static_cast<nsGlobalWindow*>(GetPrivateRoot());
    if (rootWindow) {
      rootWindow->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIBaseWindow> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));

    nsCOMPtr<nsIDOMScreen> screen;
    GetScreen(getter_AddRefs(screen));

    if (treeOwner && screen) {
      PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
      PRInt32 winLeft, winTop, winWidth, winHeight;

      // Get the window size
      treeOwner->GetPositionAndSize(&winLeft, &winTop, &winWidth, &winHeight);

      // convert those values to CSS pixels
      // XXX four separate retrievals of the prescontext
      winLeft   = DevToCSSIntPixels(winLeft);
      winTop    = DevToCSSIntPixels(winTop);
      winWidth  = DevToCSSIntPixels(winWidth);
      winHeight = DevToCSSIntPixels(winHeight);

      // Get the screen dimensions
      // XXX This should use nsIScreenManager once it's fully fleshed out.
      screen->GetAvailLeft(&screenLeft);
      screen->GetAvailWidth(&screenWidth);
      screen->GetAvailHeight(&screenHeight);
#if defined(XP_MACOSX)
      /* The mac's coordinate system is different from the assumed Windows'
         system. It offsets by the height of the menubar so that a window
         placed at (0,0) will be entirely visible. Unfortunately that
         correction is made elsewhere (in Widget) and the meaning of
         the Avail... coordinates is overloaded. Here we allow a window
         to be placed at (0,0) because it does make sense to do so.
      */
      screen->GetTop(&screenTop);
#else
      screen->GetAvailTop(&screenTop);
#endif

      if (aLeft) {
        if (screenLeft+screenWidth < *aLeft+winWidth)
          *aLeft = screenLeft+screenWidth - winWidth;
        if (screenLeft > *aLeft)
          *aLeft = screenLeft;
      }
      if (aTop) {
        if (screenTop+screenHeight < *aTop+winHeight)
          *aTop = screenTop+screenHeight - winHeight;
        if (screenTop > *aTop)
          *aTop = screenTop;
      }
    } else {
      if (aLeft)
        *aLeft = 0;
      if (aTop)
        *aTop = 0;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPageXOffset(PRInt32* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP
nsGlobalWindow::GetPageYOffset(PRInt32* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

nsresult
nsGlobalWindow::GetScrollMaxXY(PRInt32* aScrollMaxX, PRInt32* aScrollMaxY)
{
  FORWARD_TO_OUTER(GetScrollMaxXY, (aScrollMaxX, aScrollMaxY),
                   NS_ERROR_NOT_INITIALIZED);

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf)
    return NS_OK;

  nsRect scrollRange = sf->GetScrollRange();

  if (aScrollMaxX)
    *aScrollMaxX = NS_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.XMost())));
  if (aScrollMaxY)
    *aScrollMaxY = NS_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.YMost())));

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxX(PRInt32* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  *aScrollMaxX = 0;
  return GetScrollMaxXY(aScrollMaxX, nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxY(PRInt32* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  *aScrollMaxY = 0;
  return GetScrollMaxXY(nsnull, aScrollMaxY);
}

nsresult
nsGlobalWindow::GetScrollXY(PRInt32* aScrollX, PRInt32* aScrollY,
                            bool aDoFlush)
{
  FORWARD_TO_OUTER(GetScrollXY, (aScrollX, aScrollY, aDoFlush),
                   NS_ERROR_NOT_INITIALIZED);

  if (aDoFlush) {
    FlushPendingNotifications(Flush_Layout);
  } else {
    EnsureSizeUpToDate();
  }

  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf)
    return NS_OK;

  nsPoint scrollPos = sf->GetScrollPosition();
  if (scrollPos != nsPoint(0,0) && !aDoFlush) {
    // Oh, well.  This is the expensive case -- the window is scrolled and we
    // didn't actually flush yet.  Repeat, but with a flush, since the content
    // may get shorter and hence our scroll position may decrease.
    return GetScrollXY(aScrollX, aScrollY, true);
  }

  if (aScrollX)
    *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  if (aScrollY)
    *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  *aScrollX = 0;
  return GetScrollXY(aScrollX, nsnull, false);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  *aScrollY = 0;
  return GetScrollXY(nsnull, aScrollY, false);
}

NS_IMETHODIMP
nsGlobalWindow::GetLength(PRUint32* aLength)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  if (NS_SUCCEEDED(GetFrames(getter_AddRefs(frames))) && frames) {
    return frames->GetLength(aLength);
  }
  return NS_ERROR_FAILURE;
}

bool
nsGlobalWindow::DispatchCustomEvent(const char *aEventName)
{
  bool defaultActionEnabled = true;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  nsContentUtils::DispatchTrustedEvent(doc,
                                       GetOuterWindow(),
                                       NS_ConvertASCIItoUTF16(aEventName),
                                       true, true, &defaultActionEnabled);

  return defaultActionEnabled;
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsIDocShellTreeItem *callerItem = nsnull;

  if (cx) {
    nsCOMPtr<nsIWebNavigation> callerWebNav =
      do_GetInterface(nsJSUtils::GetDynamicScriptGlobal(cx));

    if (callerWebNav) {
      CallQueryInterface(callerWebNav, &callerItem);
    }
  }

  return callerItem;
}

bool
nsGlobalWindow::WindowExists(const nsAString& aName,
                             bool aLookForCallerOnJSStack)
{
  NS_PRECONDITION(IsOuterWindow(), "Must be outer window");
  NS_PRECONDITION(mDocShell, "Must have docshell");

  nsCOMPtr<nsIDocShellTreeItem> caller;
  if (aLookForCallerOnJSStack) {
    caller = GetCallerDocShellTreeItem();
  }

  nsCOMPtr<nsIDocShellTreeItem> docShell = do_QueryInterface(mDocShell);
  NS_ASSERTION(docShell,
               "Docshell doesn't implement nsIDocShellTreeItem?");

  if (!caller) {
    caller = docShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  docShell->FindItemWithName(PromiseFlatString(aName).get(), nsnull, caller,
                             getter_AddRefs(namedItem));
  return namedItem != nsnull;
}

already_AddRefed<nsIWidget>
nsGlobalWindow::GetMainWidget()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  nsIWidget *widget = nsnull;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(&widget);
  }

  return widget;
}

nsIWidget*
nsGlobalWindow::GetNearestWidget()
{
  nsIDocShell* docShell = GetDocShell();
  NS_ENSURE_TRUE(docShell, nsnull);
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, nsnull);
  nsIFrame* rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nsnull);
  return rootFrame->GetView()->GetNearestWidget(nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::SetFullScreen(bool aFullScreen)
{
  return SetFullScreenInternal(aFullScreen, true);
}

nsresult
nsGlobalWindow::SetFullScreenInternal(bool aFullScreen, bool aRequireTrust)
{
  FORWARD_TO_OUTER(SetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  bool rootWinFullScreen;
  GetFullScreen(&rootWinFullScreen);
  // Only chrome can change our fullScreen mode, unless we're running in
  // untrusted mode.
  if (aFullScreen == rootWinFullScreen || 
      (aRequireTrust && !nsContentUtils::IsCallerTrustedForWrite())) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(rootItem);
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != treeItem)
    return window->SetFullScreenInternal(aFullScreen, aRequireTrust);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  if (itemType != nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_FAILURE;

  // If we are already in full screen mode, just return.
  if (mFullScreen == aFullScreen)
    return NS_OK;

  // dispatch a "fullscreen" DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  if (!DispatchCustomEvent("fullscreen")) {
    return NS_OK;
  }

  // Prevent chrome documents which are still loading from resizing
  // the window after we set fullscreen mode.
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwnerAsWin));
  if (aFullScreen && xulWin) {
    xulWin->SetIntrinsicallySized(false);
  }

  // Set this before so if widget sends an event indicating its
  // gone full screen, the state trap above works.
  mFullScreen = aFullScreen;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  if (widget)
    widget->MakeFullScreen(aFullScreen);

  if (!mFullScreen) {
    // Force exit from DOM full-screen mode. This is so that if we're in
    // DOM full-screen mode and the user exits full-screen mode with
    // the browser full-screen mode toggle keyboard-shortcut, we'll detect
    // that and leave DOM API full-screen mode too.
    nsIDocument::ExitFullScreen(false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFullScreen(bool* aFullScreen)
{
  FORWARD_TO_OUTER(GetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  if (treeItem) {
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
    if (rootItem != treeItem) {
      nsCOMPtr<nsIDOMWindow> window = do_GetInterface(rootItem);
      if (window)
        return window->GetFullScreen(aFullScreen);
    }
  }

  // We are the root window, or something went wrong. Return our internal value.
  *aFullScreen = mFullScreen;
  return NS_OK;
}

bool
nsGlobalWindow::DOMWindowDumpEnabled()
{
#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  // In optimized builds we check a pref that controls if we should
  // enable output from dump() or not, in debug builds it's always
  // enabled.
  return gDOMWindowDumpEnabled;
#else
  return true;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::Dump(const nsAString& aStr)
{
  if (!DOMWindowDumpEnabled()) {
    return NS_OK;
  }

  char *cstr = ToNewUTF8String(aStr);

#if defined(XP_MACOSX)
  // have to convert \r to \n so that printing to the console works
  char *c = cstr, *cEnd = cstr + strlen(cstr);
  while (c < cEnd) {
    if (*c == '\r')
      *c = '\n';
    c++;
  }
#endif

  if (cstr) {
#ifdef ANDROID
    __android_log_write(ANDROID_LOG_INFO, "GeckoDump", cstr);
#endif
    FILE *fp = gDumpFile ? gDumpFile : stdout;
    fputs(cstr, fp);
    fflush(fp);
    nsMemory::Free(cstr);
  }

  return NS_OK;
}

void
nsGlobalWindow::EnsureReflowFlushAndPaint()
{
  NS_ASSERTION(IsOuterWindow(), "EnsureReflowFlushAndPaint() must be called on"
               "the outer window");
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  if (!mDocShell)
    return;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return;

  // Flush pending reflows.
  if (mDoc) {
    mDoc->FlushPendingNotifications(Flush_Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

NS_IMETHODIMP
nsGlobalWindow::GetTextZoom(float *aZoom)
{
  FORWARD_TO_OUTER(GetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer) {
      return markupViewer->GetTextZoom(aZoom);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::SetTextZoom(float aZoom)
{
  FORWARD_TO_OUTER(SetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer)
      return markupViewer->SetTextZoom(aZoom);
  }
  return NS_ERROR_FAILURE;
}

// static
void
nsGlobalWindow::MakeScriptDialogTitle(nsAString &aOutTitle)
{
  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsresult rv = NS_OK;
  NS_ASSERTION(nsContentUtils::GetSecurityManager(),
    "Global Window has no security manager!");
  if (nsContentUtils::GetSecurityManager()) {
    nsCOMPtr<nsIPrincipal> principal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSubjectPrincipal(getter_AddRefs(principal));

    if (NS_SUCCEEDED(rv) && principal) {
      nsCOMPtr<nsIURI> uri;
      rv = principal->GetURI(getter_AddRefs(uri));

      if (NS_SUCCEEDED(rv) && uri) {
        // remove user:pass for privacy and spoof prevention

        nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID));
        if (fixup) {
          nsCOMPtr<nsIURI> fixedURI;
          rv = fixup->CreateExposableURI(uri, getter_AddRefs(fixedURI));
          if (NS_SUCCEEDED(rv) && fixedURI) {
            nsCAutoString host;
            fixedURI->GetHost(host);

            if (!host.IsEmpty()) {
              // if this URI has a host we'll show it. For other
              // schemes (e.g. file:) we fall back to the localized
              // generic string

              nsCAutoString prepath;
              fixedURI->GetPrePath(prepath);

              NS_ConvertUTF8toUTF16 ucsPrePath(prepath);
              const PRUnichar *formatStrings[] = { ucsPrePath.get() };
              nsXPIDLString tempString;
              nsContentUtils::FormatLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                                    "ScriptDlgHeading",
                                                    formatStrings,
                                                    tempString);
              aOutTitle = tempString;
            }
          }
        }
      }
    }
    else { // failed to get subject principal
      NS_WARNING("No script principal? Who is calling alert/confirm/prompt?!");
    }
  }

  if (aOutTitle.IsEmpty()) {
    // We didn't find a host so use the generic heading
    nsXPIDLString tempString;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDlgGenericHeading",
                                       tempString);
    aOutTitle = tempString;
  }

  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgGenericHeading string from string bundle");
    aOutTitle.AssignLiteral("[Script]");
  }
}

bool
nsGlobalWindow::CanMoveResizeWindows()
{
  // When called from chrome, we can avoid the following checks.
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    // Don't allow scripts to move or resize windows that were not opened by a
    // script.
    if (!mHadOriginalOpener) {
      return false;
    }

    if (!CanSetProperty("dom.disable_window_move_resize")) {
      return false;
    }

    // Ignore the request if we have more than one tab in the window.
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    if (treeOwner) {
      PRUint32 itemCount;
      if (NS_SUCCEEDED(treeOwner->GetTargetableShellCount(&itemCount)) &&
          itemCount > 1) {
        return false;
      }
    }
  }

  if (mDocShell) {
    bool allow;
    nsresult rv = mDocShell->GetAllowWindowControl(&allow);
    if (NS_SUCCEEDED(rv) && !allow)
      return false;
  }

  if (gMouseDown && !gDragServiceDisabled) {
    nsCOMPtr<nsIDragService> ds =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (ds) {
      gDragServiceDisabled = true;
      ds->Suppress();
    }
  }
  return true;
}

NS_IMETHODIMP
nsGlobalWindow::Alert(const nsAString& aString)
{
  FORWARD_TO_OUTER(Alert, (aString), NS_ERROR_NOT_INITIALIZED);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the
  // popup state we push next
  bool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  // Special handling for alert(null) in JS for backwards
  // compatibility.

  NS_NAMED_LITERAL_STRING(null_str, "null");

  const nsAString *str = DOMStringIsNull(aString) ? &null_str : &aString;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  nsContentUtils::StripNullChars(*str, final);

  // Check if we're being called at a point where we can't use tab-modal
  // prompts, because something doesn't want reentrancy.
  bool allowTabModal = GetIsTabModalPromptAllowed();

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrompt> prompt;
  rv = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                            reinterpret_cast<void**>(&prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt);
  if (promptBag)
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), allowTabModal);

  nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                             GetCurrentInnerWindowInternal()->mDoc :
                             nsnull);
  if (shouldEnableDisableDialog) {
    bool disallowDialog = false;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = prompt->AlertCheck(title.get(), final.get(), label.get(),
                            &disallowDialog);
    if (disallowDialog)
      PreventFurtherDialogs();
  } else {
    rv = prompt->Alert(title.get(), final.get());
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Confirm(const nsAString& aString, bool* aReturn)
{
  FORWARD_TO_OUTER(Confirm, (aString, aReturn), NS_ERROR_NOT_INITIALIZED);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the popup state
  // we push next
  bool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  *aReturn = false;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  nsContentUtils::StripNullChars(aString, final);

  // Check if we're being called at a point where we can't use tab-modal
  // prompts, because something doesn't want reentrancy.
  bool allowTabModal = GetIsTabModalPromptAllowed();

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrompt> prompt;
  rv = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                            reinterpret_cast<void**>(&prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt);
  if (promptBag)
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), allowTabModal);

  nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                             GetCurrentInnerWindowInternal()->mDoc :
                             nsnull);
  if (shouldEnableDisableDialog) {
    bool disallowDialog = false;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = prompt->ConfirmCheck(title.get(), final.get(), label.get(),
                              &disallowDialog, aReturn);
    if (disallowDialog)
      PreventFurtherDialogs();
  } else {
    rv = prompt->Confirm(title.get(), final.get(), aReturn);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       nsAString& aReturn)
{
  FORWARD_TO_OUTER(Prompt, (aMessage, aInitial, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  SetDOMStringToNull(aReturn);

  if (AreDialogsBlocked())
    return NS_ERROR_NOT_AVAILABLE;

  // We have to capture this now so as not to get confused with the popup state
  // we push next
  bool shouldEnableDisableDialog = DialogOpenAttempted();

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, true);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);
  
  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString fixedMessage, fixedInitial;
  nsContentUtils::StripNullChars(aMessage, fixedMessage);
  nsContentUtils::StripNullChars(aInitial, fixedInitial);

  // Check if we're being called at a point where we can't use tab-modal
  // prompts, because something doesn't want reentrancy.
  bool allowTabModal = GetIsTabModalPromptAllowed();

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrompt> prompt;
  rv = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                            reinterpret_cast<void**>(&prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt);
  if (promptBag)
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), allowTabModal);

  // Pass in the default value, if any.
  PRUnichar *inoutValue = ToNewUnicode(fixedInitial);
  bool disallowDialog = false;

  nsXPIDLString label;
  if (shouldEnableDisableDialog) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);
  }

  nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                             GetCurrentInnerWindowInternal()->mDoc :
                             nsnull);
  bool ok;
  rv = prompt->Prompt(title.get(), fixedMessage.get(),
                      &inoutValue, label.get(), &disallowDialog, &ok);

  if (disallowDialog) {
    PreventFurtherDialogs();
  }

  NS_ENSURE_SUCCESS(rv, rv);

  nsAdoptingString outValue(inoutValue);

  if (ok && outValue) {
    aReturn.Assign(outValue);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Focus()
{
  FORWARD_TO_OUTER(Focus, (), NS_ERROR_NOT_INITIALIZED);

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return NS_OK;

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);

  bool isVisible = false;
  if (baseWin) {
    baseWin->GetVisibility(&isVisible);
  }

  if (!isVisible) {
    // A hidden tab is being focused, ignore this call.
    return NS_OK;
  }

  nsIDOMWindow *caller = nsContentUtils::GetWindowFromCaller();
  nsCOMPtr<nsIDOMWindow> opener;
  GetOpener(getter_AddRefs(opener));

  // Enforce dom.disable_window_flip (for non-chrome), but still allow the
  // window which opened us to raise us at times when popups are allowed
  // (bugs 355482 and 369306).
  bool canFocus = CanSetProperty("dom.disable_window_flip") ||
                    (opener == caller &&
                     RevisePopupAbuseLevel(gPopupControlState) < openAbused);

  nsCOMPtr<nsIDOMWindow> activeWindow;
  fm->GetActiveWindow(getter_AddRefs(activeWindow));

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "What happened?");
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
  bool isActive = (rootWin == activeWindow);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin && (canFocus || isActive)) {
    bool isEnabled = true;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return NS_OK;
    }

    // XXXndeakin not sure what this is for or if it should go somewhere else
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  // Don't look for a presshell if we're a root chrome window that's got
  // about:blank loaded.  We don't want to focus our widget in that case.
  // XXXbz should we really be checking for IsInitialDocument() instead?
  bool lookForPresShell = true;
  PRInt32 itemType = nsIDocShellTreeItem::typeContent;
  treeItem->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeChrome &&
      GetPrivateRoot() == static_cast<nsIDOMWindow*>(this) &&
      mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    NS_ASSERTION(doc, "Bogus doc?");
    nsIURI* ourURI = doc->GetDocumentURI();
    if (ourURI) {
      lookForPresShell = !NS_IsAboutBlank(ourURI);
    }
  }

  if (lookForPresShell) {
    mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
  }

  nsCOMPtr<nsIDocShellTreeItem> parentDsti;
  treeItem->GetParent(getter_AddRefs(parentDsti));

  // set the parent's current focus to the frame containing this window.
  nsCOMPtr<nsIDOMWindow> parent(do_GetInterface(parentDsti));
  if (parent) {
    nsCOMPtr<nsIDOMDocument> parentdomdoc;
    parent->GetDocument(getter_AddRefs(parentdomdoc));

    nsCOMPtr<nsIDocument> parentdoc = do_QueryInterface(parentdomdoc);
    if (!parentdoc)
      return NS_OK;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    nsIContent* frame = parentdoc->FindContentForSubDocument(doc);
    nsCOMPtr<nsIDOMElement> frameElement = do_QueryInterface(frame);
    if (frameElement) {
      PRUint32 flags = nsIFocusManager::FLAG_NOSCROLL;
      if (canFocus)
        flags |= nsIFocusManager::FLAG_RAISE;
      return fm->SetFocus(frameElement, flags);
    }
  }
  else if (canFocus) {
    // if there is no parent, this must be a toplevel window, so raise the
    // window if canFocus is true
    return fm->SetActiveWindow(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Blur()
{
  FORWARD_TO_OUTER(Blur, (), NS_ERROR_NOT_INITIALIZED);

  // If dom.disable_window_flip == true, then content should not be allowed
  // to call this function (this would allow popunders, bug 369306)
  if (!CanSetProperty("dom.disable_window_flip")) {
    return NS_OK;
  }

  // If embedding apps don't implement nsIEmbeddingSiteWindow2, we
  // shouldn't throw exceptions to web content.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIEmbeddingSiteWindow2> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nsnull.
    rv = siteWindow->Blur();

    // if the root is focused, clear the focus
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (fm && mDocument) {
      nsCOMPtr<nsIDOMElement> element;
      fm->GetFocusedElementForWindow(this, false, nsnull, getter_AddRefs(element));
      nsCOMPtr<nsIContent> content = do_QueryInterface(element);
      if (content == doc->GetRootElement())
        fm->ClearFocus(this);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Back()
{
  FORWARD_TO_OUTER(Back, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoBack();
}

NS_IMETHODIMP
nsGlobalWindow::Forward()
{
  FORWARD_TO_OUTER(Forward, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoForward();
}

NS_IMETHODIMP
nsGlobalWindow::Home()
{
  FORWARD_TO_OUTER(Home, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_OK;

  nsAdoptingString homeURL =
    Preferences::GetLocalizedString(PREF_BROWSER_STARTUP_HOMEPAGE);

  if (homeURL.IsEmpty()) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n", DEFAULT_HOME_PAGE);
#endif
    CopyASCIItoUTF16(DEFAULT_HOME_PAGE, homeURL);
  }

#ifdef MOZ_PHOENIX
  {
    // Firefox lets the user specify multiple home pages to open in
    // individual tabs by separating them with '|'. Since we don't
    // have the machinery in place to easily open new tabs from here,
    // simply truncate the homeURL at the first '|' character to
    // prevent any possibilities of leaking the users list of home
    // pages to the first home page.
    //
    // Once bug https://bugzilla.mozilla.org/show_bug.cgi?id=221445 is
    // fixed we can revisit this.
    PRInt32 firstPipe = homeURL.FindChar('|');

    if (firstPipe > 0) {
      homeURL.Truncate(firstPipe);
    }
  }
#endif

  nsresult rv;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  rv = webNav->LoadURI(homeURL.get(),
                       nsIWebNavigation::LOAD_FLAGS_NONE,
                       nsnull,
                       nsnull,
                       nsnull);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Stop()
{
  FORWARD_TO_OUTER(Stop, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav)
    return NS_OK;

  return webNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_IMETHODIMP
nsGlobalWindow::Print()
{
#ifdef NS_PRINTING
  FORWARD_TO_OUTER(Print, (), NS_ERROR_NOT_INITIALIZED);

  if (Preferences::GetBool("dom.disable_window_print", false))
    return NS_ERROR_NOT_AVAILABLE;

  if (AreDialogsBlocked() || !ConfirmDialogAllowed())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {
    nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                               GetCurrentInnerWindowInternal()->mDoc :
                               nsnull);

    nsCOMPtr<nsIPrintSettingsService> printSettingsService = 
      do_GetService("@mozilla.org/gfx/printsettings-service;1");

    nsCOMPtr<nsIPrintSettings> printSettings;
    if (printSettingsService) {
      bool printSettingsAreGlobal =
        Preferences::GetBool("print.use_global_printsettings", false);

      if (printSettingsAreGlobal) {
        printSettingsService->GetGlobalPrintSettings(getter_AddRefs(printSettings));

        nsXPIDLString printerName;
        printSettings->GetPrinterName(getter_Copies(printerName));
        if (printerName.IsEmpty()) {
          printSettingsService->GetDefaultPrinterName(getter_Copies(printerName));
          printSettings->SetPrinterName(printerName);
        }
        printSettingsService->InitPrintSettingsFromPrinter(printerName, printSettings);
        printSettingsService->InitPrintSettingsFromPrefs(printSettings, 
                                                         true, 
                                                         nsIPrintSettings::kInitSaveAll);
      } else {
        printSettingsService->GetNewPrintSettings(getter_AddRefs(printSettings));
      }

      nsCOMPtr<nsIDOMWindow> callerWin = EnterModalState();
      webBrowserPrint->Print(printSettings, nsnull);
      LeaveModalState(callerWin);

      bool savePrintSettings =
        Preferences::GetBool("print.save_print_settings", false);
      if (printSettingsAreGlobal && savePrintSettings) {
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   true,
                                   nsIPrintSettings::kInitSaveAll);
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   false,
                                   nsIPrintSettings::kInitSavePrinterName);
      }
    } else {
      webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
      webBrowserPrint->Print(printSettings, nsnull);
    }
  }
#endif //NS_PRINTING

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  FORWARD_TO_OUTER(MoveTo, (aXPos, aYPos), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aXPos, &aYPos),
                    NS_ERROR_FAILURE);

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize devPos(CSSToDevIntPixels(nsIntSize(aXPos, aYPos)));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(devPos.width, devPos.height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
  FORWARD_TO_OUTER(MoveBy, (aXDif, aYDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  // To do this correctly we have to convert what we get from GetPosition
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetPosition.

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y), NS_ERROR_FAILURE);

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize cssPos(DevToCSSIntPixels(nsIntSize(x, y)));

  cssPos.width += aXDif;
  cssPos.height += aYDif;
  
  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&cssPos.width,
                                            &cssPos.height),
                    NS_ERROR_FAILURE);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(newDevPos.width,
                                                newDevPos.height),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  FORWARD_TO_OUTER(ResizeTo, (aWidth, aHeight), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);
  
  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aWidth, &aHeight),
                    NS_ERROR_FAILURE);

  nsIntSize devSz(CSSToDevIntPixels(nsIntSize(aWidth, aHeight)));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(devSz.width, devSz.height, true),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
  FORWARD_TO_OUTER(ResizeBy, (aWidthDif, aHeightDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  // To do this correctly we have to convert what we got from GetSize
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetSize.

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));

  cssSize.width += aWidthDif;
  cssSize.height += aHeightDif;

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&cssSize.width,
                                                &cssSize.height),
                    NS_ERROR_FAILURE);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(newDevSize.width,
                                            newDevSize.height,
                                            true),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SizeToContent()
{
  FORWARD_TO_OUTER(SizeToContent, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell) {
    return NS_OK;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return NS_OK;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  NS_ENSURE_TRUE(markupViewer, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(markupViewer->SizeToContent(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetWindowRoot(nsIDOMEventTarget **aWindowRoot)
{
  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return CallQueryInterface(root, aWindowRoot);
}

already_AddRefed<nsPIWindowRoot>
nsGlobalWindow::GetTopWindowRoot()
{
  nsIDOMWindow *rootWindow = GetPrivateRoot();
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
  if (!piWin)
    return nsnull;

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

NS_IMETHODIMP
nsGlobalWindow::Scroll(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP
nsGlobalWindow::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and substracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const PRInt32 maxpx = nsPresContext::AppUnitsToIntCSSPixels(0x7fffffff) - 4;

    if (aXScroll > maxpx) {
      aXScroll = maxpx;
    }

    if (aYScroll > maxpx) {
      aYScroll = maxpx;
    }
    sf->ScrollToCSSPixels(nsIntPoint(aXScroll, aYScroll));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    nsPoint scrollPos = sf->GetScrollPosition();
    // It seems like it would make more sense for ScrollBy to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    return ScrollTo(nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x) + aXScrollDif,
                    nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y) + aYScrollDif);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByLines(PRInt32 numLines)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByLines to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    sf->ScrollBy(nsIntPoint(0, numLines), nsIScrollableFrame::LINES,
                 nsIScrollableFrame::INSTANT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByPages(PRInt32 numPages)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByPages to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    sf->ScrollBy(nsIntPoint(0, numPages), nsIScrollableFrame::PAGES,
                 nsIScrollableFrame::INSTANT);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ClearTimeout(PRInt32 aHandle)
{
  if (aHandle <= 0) {
    return NS_OK;
  }

  return ClearTimeoutOrInterval(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::ClearInterval(PRInt32 aHandle)
{
  if (aHandle <= 0) {
    return NS_OK;
  }

  return ClearTimeoutOrInterval(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::SetTimeout(PRInt32 *_retval)
{
  return SetTimeoutOrInterval(false, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetInterval(PRInt32 *_retval)
{
  return SetTimeoutOrInterval(true, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetResizable(bool aResizable)
{
  // nop

  return NS_OK;
}

static void
ReportUseOfDeprecatedMethod(nsGlobalWindow* aWindow, const char* aWarning)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "DOM Events", doc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aWarning);
}

NS_IMETHODIMP
nsGlobalWindow::CaptureEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfCaptureEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ReleaseEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfReleaseEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::RouteEvent(nsIDOMEvent* aEvt)
{
  ReportUseOfDeprecatedMethod(this, "UseOfRouteEventWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::EnableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::DisableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

static
bool IsPopupBlocked(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIPopupWindowManager> pm =
    do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);

  if (!pm) {
    return false;
  }

  bool blocked = true;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));

  if (doc) {
    PRUint32 permission = nsIPopupWindowManager::ALLOW_POPUP;
    pm->TestPermission(doc->GetDocumentURI(), &permission);
    blocked = (permission == nsIPopupWindowManager::DENY_POPUP);
  }
  return blocked;
}

/* static */
void 
nsGlobalWindow::FirePopupBlockedEvent(nsIDOMDocument* aDoc,
                                      nsIDOMWindow *aRequestingWindow, nsIURI *aPopupURI,
                                      const nsAString &aPopupWindowName,
                                      const nsAString &aPopupWindowFeatures)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about
    // blocked popups.
    nsCOMPtr<nsIDOMEvent> event;
    aDoc->CreateEvent(NS_LITERAL_STRING("PopupBlockedEvents"),
                      getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMPopupBlockedEvent> pbev(do_QueryInterface(event));
      pbev->InitPopupBlockedEvent(NS_LITERAL_STRING("DOMPopupBlocked"),
                                  true, true, aRequestingWindow,
                                  aPopupURI, aPopupWindowName,
                                  aPopupWindowFeatures);
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
      privateEvent->SetTrusted(true);

      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      bool defaultActionEnabled;
      targ->DispatchEvent(event, &defaultActionEnabled);
    }
  }
}

void FirePopupWindowEvent(nsIDOMDocument* aDoc)
{
  // Fire a "PopupWindow" event
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  nsContentUtils::DispatchTrustedEvent(doc, aDoc,
                                       NS_LITERAL_STRING("PopupWindow"),
                                       true, true);
}

// static
bool
nsGlobalWindow::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::IsCallerTrustedForWrite()) {
    return true;
  }

  // If the pref is set to true, we can not set the property
  // and vice versa.
  return !Preferences::GetBool(aPrefName, true);
}

bool
nsGlobalWindow::PopupWhitelisted()
{
  if (!IsPopupBlocked(mDocument))
    return true;

  nsCOMPtr<nsIDOMWindow> parent;

  if (NS_FAILED(GetParent(getter_AddRefs(parent))) ||
      parent == static_cast<nsIDOMWindow*>(this))
  {
    return false;
  }

  return static_cast<nsGlobalWindow*>
                    (static_cast<nsIDOMWindow*>
                                (parent.get()))->PopupWhitelisted();
}

/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers. The window.open code uses this
 * routine to determine whether to allow the new window.
 * Returns a value from the PopupControlState enum.
 */
PopupControlState
nsGlobalWindow::RevisePopupAbuseLevel(PopupControlState aControl)
{
  FORWARD_TO_OUTER(RevisePopupAbuseLevel, (aControl), aControl);

  NS_ASSERTION(mDocShell, "Must have docshell");
  
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mDocShell));

  NS_ASSERTION(item, "Docshell doesn't implement nsIDocShellTreeItem?");

  PRInt32 type = nsIDocShellTreeItem::typeChrome;
  item->GetItemType(&type);
  if (type != nsIDocShellTreeItem::typeContent)
    return openAllowed;

  PopupControlState abuse = aControl;
  switch (abuse) {
  case openControlled:
  case openAbused:
  case openOverridden:
    if (PopupWhitelisted())
      abuse = PopupControlState(abuse - 1);
  case openAllowed: break;
  default:
    NS_WARNING("Strange PopupControlState!");
  }

  // limit the number of simultaneously open popups
  if (abuse == openAbused || abuse == openControlled) {
    PRInt32 popupMax = Preferences::GetInt("dom.popup_maximum", -1);
    if (popupMax >= 0 && gOpenPopupSpamCount >= popupMax)
      abuse = openOverridden;
  }

  return abuse;
}

/* If a window open is blocked, fire the appropriate DOM events.
   aBlocked signifies we just blocked a popup.
   aWindow signifies we just opened what is probably a popup.
*/
void
nsGlobalWindow::FireAbuseEvents(bool aBlocked, bool aWindow,
                                const nsAString &aPopupURL,
                                const nsAString &aPopupWindowName,
                                const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsIDOMWindow> topWindow;
  GetTop(getter_AddRefs(topWindow));
  if (!topWindow)
    return;

  nsCOMPtr<nsIDOMDocument> topDoc;
  topWindow->GetDocument(getter_AddRefs(topDoc));

  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = 0;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsCOMPtr<nsIDOMWindow> contextWindow;

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);
    if (currentCX) {
      contextWindow = do_QueryInterface(currentCX->GetGlobalObject());
    }
  }
  if (!contextWindow)
    contextWindow = static_cast<nsIDOMWindow*>(this);

  nsCOMPtr<nsIDOMDocument> domdoc;
  contextWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (doc)
    baseURL = doc->GetDocBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  if (aBlocked)
    FirePopupBlockedEvent(topDoc, this, popupURI, aPopupWindowName,
                          aPopupWindowFeatures);
  if (aWindow)
    FirePopupWindowEvent(topDoc);
}

NS_IMETHODIMP
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nsnull,            // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::OpenJS(const nsAString& aUrl, const nsAString& aName,
                       const nsAString& aOptions, nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      false,          // aCalledNoScript
                      true,           // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nsContentUtils::GetCurrentJSContext(), // aJSCallerContext
                      _retval);
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions,
                           nsISupports* aExtraArgument, nsIDOMWindow** _retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      true,                    // aDialog
                      false,                   // aContentModal
                      true,                    // aCalledNoScript
                      false,                   // aDoJSFixups
                      nsnull, aExtraArgument,     // Arguments
                      GetPrincipal(),             // aCalleePrincipal
                      nsnull,                     // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions, nsIDOMWindow** _retval)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAXPCNativeCallContext *ncc = nsnull;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  // XXX - need to get this as nsISupports?
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // Strip the url, name and options from the args seen by scripts.
  PRUint32 argOffset = argc < 3 ? argc : 3;
  nsCOMPtr<nsIJSArgArray> argvArray;
  rv = NS_CreateJSArgv(cx, argc - argOffset, argv + argOffset,
                       getter_AddRefs(argvArray));
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenInternal(aUrl, aName, aOptions,
                      true,             // aDialog
                      false,            // aContentModal
                      false,            // aCalledNoScript
                      false,            // aDoJSFixups
                      argvArray, nsnull,   // Arguments
                      GetPrincipal(),      // aCalleePrincipal
                      cx,                  // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindow** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = this;
  NS_ADDREF(*aFrames);

  FlushPendingNotifications(Flush_ContentAndNotify);

  return NS_OK;
}

JSObject* nsGlobalWindow::CallerGlobal()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    NS_ERROR("Please don't call this method from C++!");

    return nsnull;
  }

  return JS_GetScriptedGlobal(cx);
}

nsGlobalWindow*
nsGlobalWindow::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    NS_ERROR("Please don't call this method from C++!");

    return nsnull;
  }

  JSObject *scope = CallerGlobal();

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, scope))
    return nsnull;

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, scope, getter_AddRefs(wrapper));
  if (!wrapper)
    return nsnull;

  // The calling window must be holding a reference, so we can just return a
  // raw pointer here and let the QI's addref be balanced by the nsCOMPtr
  // destructor's release.
  nsCOMPtr<nsPIDOMWindow> win = do_QueryWrappedNative(wrapper);
  if (!win)
    return GetCurrentInnerWindowInternal();
  return static_cast<nsGlobalWindow*>(win.get());
}

/**
 * Class used to represent events generated by calls to Window.postMessage,
 * which asynchronously creates and dispatches events.
 */
class PostMessageEvent : public nsRunnable
{
  public:
    NS_DECL_NSIRUNNABLE

    PostMessageEvent(nsGlobalWindow* aSource,
                     const nsAString& aCallerOrigin,
                     nsGlobalWindow* aTargetWindow,
                     nsIURI* aProvidedOrigin,
                     bool aTrustedCaller)
    : mSource(aSource),
      mCallerOrigin(aCallerOrigin),
      mMessage(nsnull),
      mMessageLen(0),
      mTargetWindow(aTargetWindow),
      mProvidedOrigin(aProvidedOrigin),
      mTrustedCaller(aTrustedCaller)
    {
      MOZ_COUNT_CTOR(PostMessageEvent);
    }
    
    ~PostMessageEvent()
    {
      NS_ASSERTION(!mMessage, "Message should have been deserialized!");
      MOZ_COUNT_DTOR(PostMessageEvent);
    }

    void SetJSData(JSAutoStructuredCloneBuffer& aBuffer)
    {
      NS_ASSERTION(!mMessage && mMessageLen == 0, "Don't call twice!");
      aBuffer.steal(&mMessage, &mMessageLen);
    }

    bool StoreISupports(nsISupports* aSupports)
    {
      mSupportsArray.AppendElement(aSupports);
      return true;
    }

  private:
    nsRefPtr<nsGlobalWindow> mSource;
    nsString mCallerOrigin;
    uint64_t* mMessage;
    size_t mMessageLen;
    nsRefPtr<nsGlobalWindow> mTargetWindow;
    nsCOMPtr<nsIURI> mProvidedOrigin;
    bool mTrustedCaller;
    nsTArray<nsCOMPtr<nsISupports> > mSupportsArray;
};

namespace {

struct StructuredCloneInfo {
  PostMessageEvent* event;
  bool subsumes;
};

static JSObject*
PostMessageReadStructuredClone(JSContext* cx,
                               JSStructuredCloneReader* reader,
                               uint32 tag,
                               uint32 data,
                               void* closure)
{
  NS_ASSERTION(closure, "Must have closure!");

  if (tag == SCTAG_DOM_BLOB || tag == SCTAG_DOM_FILELIST) {
    NS_ASSERTION(!data, "Data should be empty");

    nsISupports* supports;
    if (JS_ReadBytes(reader, &supports, sizeof(supports))) {
      JSObject* global = JS_GetGlobalForScopeChain(cx);
      if (global) {
        jsval val;
        nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
        if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, global, supports,
                                                    &val,
                                                    getter_AddRefs(wrapper)))) {
          return JSVAL_TO_OBJECT(val);
        }
      }
    }
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->read(cx, reader, tag, data, nsnull);
  }

  return JS_FALSE;
}

static JSBool
PostMessageWriteStructuredClone(JSContext* cx,
                                JSStructuredCloneWriter* writer,
                                JSObject* obj,
                                void *closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrappedNative));
  if (wrappedNative) {
    PRUint32 scTag = 0;
    nsISupports* supports = wrappedNative->Native();

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(supports);
    if (blob && scInfo->subsumes)
      scTag = SCTAG_DOM_BLOB;

    nsCOMPtr<nsIDOMFileList> list = do_QueryInterface(supports);
    if (list && scInfo->subsumes)
      scTag = SCTAG_DOM_FILELIST;

    if (scTag)
      return JS_WriteUint32Pair(writer, scTag, 0) &&
             JS_WriteBytes(writer, &supports, sizeof(supports)) &&
             scInfo->event->StoreISupports(supports);
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->write(cx, writer, obj, nsnull);
  }

  return JS_FALSE;
}

JSStructuredCloneCallbacks kPostMessageCallbacks = {
  PostMessageReadStructuredClone,
  PostMessageWriteStructuredClone,
  nsnull
};

} // anonymous namespace

NS_IMETHODIMP
PostMessageEvent::Run()
{
  NS_ABORT_IF_FALSE(mTargetWindow->IsOuterWindow(),
                    "should have been passed an outer window!");
  NS_ABORT_IF_FALSE(!mSource || mSource->IsOuterWindow(),
                    "should have been passed an outer window!");

  // Get the JSContext for the target window
  JSContext* cx = nsnull;
  nsIScriptContext* scriptContext = mTargetWindow->GetContext();
  if (scriptContext) {
    cx = scriptContext->GetNativeContext();
  }

  if (!cx) {
    // This can happen if mTargetWindow has been closed.  To avoid leaking,
    // we need to find a JSContext.
    nsIThreadJSContextStack* cxStack = nsContentUtils::ThreadJSContextStack();
    if (cxStack) {
      cx = cxStack->GetSafeJSContext();
    }

    if (!cx) {
      NS_WARNING("Cannot find a JSContext!  Leaking PostMessage buffer.");
      return NS_ERROR_FAILURE;
    }
  }

  // If we bailed before this point we're going to leak mMessage, but
  // that's probably better than crashing.

  // Ensure that the buffer is freed even if we fail to post the message
  JSAutoStructuredCloneBuffer buffer;
  buffer.adopt(mMessage, mMessageLen);
  mMessage = nsnull;
  mMessageLen = 0;

  nsRefPtr<nsGlobalWindow> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsClosedOrClosing())
    return NS_OK;

  NS_ABORT_IF_FALSE(targetWindow->IsInnerWindow(),
                    "we ordered an inner window!");

  // Ensure that any origin which might have been provided is the origin of this
  // window's document.  Note that we do this *now* instead of when postMessage
  // is called because the target window might have been navigated to a
  // different location between then and now.  If this check happened when
  // postMessage was called, it would be fairly easy for a malicious webpage to
  // intercept messages intended for another site by carefully timing navigation
  // of the target window so it changed location after postMessage but before
  // now.
  if (mProvidedOrigin) {
    // Get the target's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the target's
    // document.
    nsIPrincipal* targetPrin = targetWindow->GetPrincipal();
    if (!targetPrin)
      return NS_OK;
    nsCOMPtr<nsIURI> targetURI;
    if (NS_FAILED(targetPrin->GetURI(getter_AddRefs(targetURI))))
      return NS_OK;
    if (!targetURI) {
      targetURI = targetWindow->mDoc->GetDocumentURI();
      if (!targetURI)
        return NS_OK;
    }

    // Note: This is contrary to the spec with respect to file: URLs, which
    //       the spec groups into a single origin, but given we intentionally
    //       don't do that in other places it seems better to hold the line for
    //       now.  Long-term, we want HTML5 to address this so that we can
    //       be compliant while being safer.
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    nsresult rv =
      ssm->CheckSameOriginURI(mProvidedOrigin, targetURI, true);
    if (NS_FAILED(rv))
      return NS_OK;
  }

  // Deserialize the structured clone data
  jsval messageData;
  {
    JSAutoRequest ar(cx);
    StructuredCloneInfo scInfo;
    scInfo.event = this;

    if (!buffer.read(cx, &messageData, &kPostMessageCallbacks, &scInfo))
      return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  // Create the event
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(targetWindow->mDocument);
  if (!domDoc)
    return NS_OK;
  nsCOMPtr<nsIDOMEvent> event;
  domDoc->CreateEvent(NS_LITERAL_STRING("MessageEvent"),
                      getter_AddRefs(event));
  if (!event)
    return NS_OK;

  nsCOMPtr<nsIDOMMessageEvent> message = do_QueryInterface(event);
  nsresult rv = message->InitMessageEvent(NS_LITERAL_STRING("message"),
                                          false /* non-bubbling */,
                                          true /* cancelable */,
                                          messageData,
                                          mCallerOrigin,
                                          EmptyString(),
                                          mSource);
  if (NS_FAILED(rv))
    return NS_OK;


  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  nsIPresShell *shell = targetWindow->mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell)
    presContext = shell->GetPresContext();

  nsCOMPtr<nsIPrivateDOMEvent> privEvent = do_QueryInterface(message);
  privEvent->SetTrusted(mTrustedCaller);
  nsEvent *internalEvent = privEvent->GetInternalNSEvent();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEventDispatcher::Dispatch(static_cast<nsPIDOMWindow*>(mTargetWindow),
                              presContext,
                              internalEvent,
                              message,
                              &status);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::PostMessageMoz(const jsval& aMessage,
                               const nsAString& aOrigin,
                               JSContext* aCx)
{
  FORWARD_TO_OUTER(PostMessageMoz, (aMessage, aOrigin, aCx),
                   NS_ERROR_NOT_INITIALIZED);

  //
  // Window.postMessage is an intentional subversion of the same-origin policy.
  // As such, this code must be particularly careful in the information it
  // exposes to calling code.
  //
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/section-crossDocumentMessages.html
  //

  // First, get the caller's window
  nsRefPtr<nsGlobalWindow> callerInnerWin = CallerInnerWindow();
  nsIPrincipal* callerPrin;
  if (callerInnerWin) {
    NS_ABORT_IF_FALSE(callerInnerWin->IsInnerWindow(),
                      "should have gotten an inner window here");

    // Compute the caller's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the caller's
    // document.  We must get this now instead of when the event is created and
    // dispatched, because ultimately it is the identity of the calling window
    // *now* that determines who sent the message (and not an identity which might
    // have changed due to intervening navigations).
    callerPrin = callerInnerWin->GetPrincipal();
  }
  else {
    // In case the global is not a window, it can be a sandbox, and the sandbox's
    // principal can be used for the security check.
    JSObject *global = CallerGlobal();
    NS_ASSERTION(global, "Why is there no global object?");
    JSCompartment *compartment = js::GetObjectCompartment(global);
    callerPrin = xpc::GetCompartmentPrincipal(compartment);
  }
  if (!callerPrin)
    return NS_OK;

  nsCOMPtr<nsIURI> callerOuterURI;
  if (NS_FAILED(callerPrin->GetURI(getter_AddRefs(callerOuterURI))))
    return NS_OK;

  nsAutoString origin;
  if (callerOuterURI) {
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerPrin, origin);
  }
  else if (callerInnerWin) {
    // otherwise use the URI of the document to generate origin
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(callerInnerWin->GetExtantDocument());
    if (!doc)
      return NS_OK;
    callerOuterURI = doc->GetDocumentURI();
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerOuterURI, origin);
  }
  else {
    // in case of a sandbox with a system principal origin can be empty
    if (!nsContentUtils::IsSystemPrincipal(callerPrin))
      return NS_OK;
  }

  // Convert the provided origin string into a URI for comparison purposes.
  // "*" indicates no specific origin is required.
  nsCOMPtr<nsIURI> providedOrigin;
  if (!aOrigin.EqualsASCII("*")) {
    if (NS_FAILED(NS_NewURI(getter_AddRefs(providedOrigin), aOrigin)))
      return NS_ERROR_DOM_SYNTAX_ERR;
    if (NS_FAILED(providedOrigin->SetUserPass(EmptyCString())) ||
        NS_FAILED(providedOrigin->SetPath(EmptyCString())))
      return NS_OK;
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  nsRefPtr<PostMessageEvent> event =
    new PostMessageEvent(nsContentUtils::IsCallerChrome() || !callerInnerWin
                         ? nsnull
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         this,
                         providedOrigin,
                         nsContentUtils::IsCallerTrustedForWrite());

  // We *must* clone the data here, or the jsval could be modified
  // by script
  JSAutoStructuredCloneBuffer buffer;
  StructuredCloneInfo scInfo;
  scInfo.event = event;

  nsIPrincipal* principal = GetPrincipal();
  if (NS_FAILED(callerPrin->Subsumes(principal, &scInfo.subsumes)))
    return NS_ERROR_DOM_DATA_CLONE_ERR;

  if (!buffer.write(aCx, aMessage, &kPostMessageCallbacks, &scInfo))
    return NS_ERROR_DOM_DATA_CLONE_ERR;

  event->SetJSData(buffer);

  return NS_DispatchToCurrentThread(event);
}

class nsCloseEvent : public nsRunnable {

  nsRefPtr<nsGlobalWindow> mWindow;

  nsCloseEvent(nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {}

public:

  static nsresult
  PostCloseEvent(nsGlobalWindow* aWindow) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(aWindow);
    nsresult rv = NS_DispatchToCurrentThread(ev);
    if (NS_SUCCEEDED(rv))
      aWindow->MaybeForgiveSpamCount();
    return rv;
  }

  NS_IMETHOD Run() {
    if (mWindow)
      mWindow->ReallyCloseWindow();
    return NS_OK;
  }

};

bool
nsGlobalWindow::CanClose()
{
  if (!mDocShell)
    return true;

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    bool canClose;
    nsresult rv = cv->PermitUnload(false, &canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;

    rv = cv->RequestWindowClose(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;
  }

  return true;
}

NS_IMETHODIMP
nsGlobalWindow::Close()
{
  FORWARD_TO_OUTER(Close, (), NS_ERROR_NOT_INITIALIZED);

  if (IsFrame() || !mDocShell || IsInModalState()) {
    // window.close() is called on a frame in a frameset, on a window
    // that's already closed, or on a window for which there's
    // currently a modal dialog open. Ignore such calls.

    return NS_OK;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return NS_OK;
  }

  if (mBlockScriptedClosingFlag)
  {
    // A script's popup has been blocked and we don't want
    // the window to be closed directly after this event,
    // so the user can see that there was a blocked popup.
    return NS_OK;
  }

  // Don't allow scripts from content to close windows
  // that were not opened by script
  if (!mHadOriginalOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    bool allowClose =
      Preferences::GetBool("dom.allow_scripts_to_close_windows", true);
    if (!allowClose) {
      // We're blocking the close operation
      // report localized error msg in JS console
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag,
          "DOM Window", mDoc,  // Better name for the category?
          nsContentUtils::eDOM_PROPERTIES,
          "WindowCloseBlockedWarning");

      return NS_OK;
    }
  }

  if (!mInClose && !mIsClosed && !CanClose())
    return NS_OK;

  // Fire a DOM event notifying listeners that this window is about to
  // be closed. The tab UI code may choose to cancel the default
  // action for this event, if so, we won't actually close the window
  // (since the tab UI code will close the tab in stead). Sure, this
  // could be abused by content code, but do we care? I don't think
  // so...

  bool wasInClose = mInClose;
  mInClose = true;

  if (!DispatchCustomEvent("DOMWindowClose")) {
    // Someone chose to prevent the default action for this event, if
    // so, let's not close this window after all...

    mInClose = wasInClose;
    return NS_OK;
  }

  return FinalClose();
}

nsresult
nsGlobalWindow::ForceClose()
{
  if (IsFrame() || !mDocShell) {
    // This may be a frame in a frameset, or a window that's already closed.
    // Ignore such calls.

    return NS_OK;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return NS_OK;
  }

  mInClose = true;

  DispatchCustomEvent("DOMWindowClose");

  return FinalClose();
}

nsresult
nsGlobalWindow::FinalClose()
{
  // Flag that we were closed.
  mIsClosed = true;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService(sJSStackContractID);

  JSContext *cx = nsnull;

  if (stack) {
    stack->Peek(&cx);
  }

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);

    if (currentCX && currentCX == GetContextInternal()) {
      currentCX->SetTerminationFunction(CloseWindow, this);
      mHavePendingClose = true;
      return NS_OK;
    }
  }

  // We may have plugins on the page that have issued this close from their
  // event loop and because we currently destroy the plugin window with
  // frames, we crash. So, if we are called from Javascript, post an event
  // to really close the window.
  if (nsContentUtils::IsCallerChrome() ||
      NS_FAILED(nsCloseEvent::PostCloseEvent(this))) {
    ReallyCloseWindow();
  } else {
    mHavePendingClose = true;
  }

  return NS_OK;
}


void
nsGlobalWindow::ReallyCloseWindow()
{
  FORWARD_TO_OUTER_VOID(ReallyCloseWindow, ());

  // Make sure we never reenter this method.
  mHavePendingClose = true;

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {

    // but if we're a browser window we could be in some nasty
    // self-destroying cascade that we should mostly ignore

    nsCOMPtr<nsIDocShellTreeItem> docItem(do_QueryInterface(mDocShell));
    if (docItem) {
      nsCOMPtr<nsIBrowserDOMWindow> bwin;
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      docItem->GetRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDOMWindow> rootWin(do_GetInterface(rootItem));
      nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(rootWin));
      if (chromeWin)
        chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

      if (rootWin) {
        /* Normally we destroy the entire window, but not if
           this DOM window belongs to a tabbed browser and doesn't
           correspond to a tab. This allows a well-behaved tab
           to destroy the container as it should but is a final measure
           to prevent an errant tab from doing so when it shouldn't.
           This works because we reach this code when we shouldn't only
           in the particular circumstance that we belong to a tab
           that has just been closed (and is therefore already missing
           from the list of browsers) (and has an unload handler
           that closes the window). */
        // XXXbz now that we have mHavePendingClose, is this needed?
        bool isTab = false;
        if (rootWin == this ||
            !bwin || (bwin->IsTabContentWindow(GetOuterWindowInternal(),
                                               &isTab), isTab))
          treeOwnerAsWin->Destroy();
      }
    }

    CleanUp(false);
  }
}

nsIDOMWindow *
nsGlobalWindow::EnterModalState()
{
  nsGlobalWindow* topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");

    return nsnull;
  }

  // If there is an active ESM in this window, clear it. Otherwise, this can
  // cause a problem if a modal state is entered during a mouseup event.
  nsEventStateManager* activeESM =
    static_cast<nsEventStateManager*>(nsEventStateManager::GetActiveEventStateManager());
  if (activeESM && activeESM->GetPresContext()) {
    nsIPresShell* activeShell = activeESM->GetPresContext()->GetPresShell();
    if (activeShell && (
        nsContentUtils::ContentIsCrossDocDescendantOf(activeShell->GetDocument(), mDoc) ||
        nsContentUtils::ContentIsCrossDocDescendantOf(mDoc, activeShell->GetDocument()))) {
      nsEventStateManager::ClearGlobalActiveContent(activeESM);

      activeShell->SetCapturingContent(nsnull, 0);

      if (activeShell) {
        nsRefPtr<nsFrameSelection> frameSelection = activeShell->FrameSelection();
        frameSelection->SetMouseDownState(false);
      }
    }
  }

  if (topWin->mModalStateDepth == 0) {
    NS_ASSERTION(!mSuspendedDoc, "Shouldn't have mSuspendedDoc here!");

    mSuspendedDoc = do_QueryInterface(topWin->GetExtantDocument());
    if (mSuspendedDoc && mSuspendedDoc->EventHandlingSuppressed()) {
      mSuspendedDoc->SuppressEventHandling();
    } else {
      mSuspendedDoc = nsnull;
    }
  }
  topWin->mModalStateDepth++;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();

  nsCOMPtr<nsIDOMWindow> callerWin;
  nsIScriptContext *scx;
  if (cx && (scx = GetScriptContextFromJSContext(cx))) {
    scx->EnterModalState();
    callerWin = do_QueryInterface(nsJSUtils::GetDynamicScriptGlobal(cx));
  }

  if (mContext) {
    mContext->EnterModalState();
  }

  return callerWin;
}

// static
void
nsGlobalWindow::RunPendingTimeoutsRecursive(nsGlobalWindow *aTopWindow,
                                            nsGlobalWindow *aWindow)
{
  nsGlobalWindow *inner;

  // Return early if we're frozen or have no inner window.
  if (!(inner = aWindow->GetCurrentInnerWindowInternal()) ||
      inner->IsFrozen()) {
    return;
  }

  inner->RunTimeout(nsnull);

  // Check again if we're frozen since running pending timeouts
  // could've frozen us.
  if (inner->IsFrozen()) {
    return;
  }

  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));

  if (!frames) {
    return;
  }

  PRUint32 i, length;
  if (NS_FAILED(frames->GetLength(&length)) || !length) {
    return;
  }

  for (i = 0; i < length && aTopWindow->mModalStateDepth == 0; i++) {
    nsCOMPtr<nsIDOMWindow> child;
    frames->Item(i, getter_AddRefs(child));

    if (!child) {
      return;
    }

    nsGlobalWindow *childWin =
      static_cast<nsGlobalWindow *>
                 (static_cast<nsIDOMWindow *>
                             (child.get()));

    RunPendingTimeoutsRecursive(aTopWindow, childWin);
  }
}

class nsPendingTimeoutRunner : public nsRunnable
{
public:
  nsPendingTimeoutRunner(nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {
    NS_ASSERTION(mWindow, "mWindow is null.");
  }

  NS_IMETHOD Run()
  {
    nsGlobalWindow::RunPendingTimeoutsRecursive(mWindow, mWindow);

    return NS_OK;
  }

private:
  nsRefPtr<nsGlobalWindow> mWindow;
};

void
nsGlobalWindow::LeaveModalState(nsIDOMWindow *aCallerWin)
{
  nsGlobalWindow *topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");
    return;
  }

  topWin->mModalStateDepth--;

  if (topWin->mModalStateDepth == 0) {
    nsCOMPtr<nsIRunnable> runner = new nsPendingTimeoutRunner(topWin);
    if (NS_FAILED(NS_DispatchToCurrentThread(runner)))
      NS_WARNING("failed to dispatch pending timeout runnable");

    if (mSuspendedDoc) {
      nsCOMPtr<nsIDocument> currentDoc =
        do_QueryInterface(topWin->GetExtantDocument());
      mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(currentDoc == mSuspendedDoc);
      mSuspendedDoc = nsnull;
    }
  }

  if (aCallerWin) {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aCallerWin));
    nsIScriptContext *scx = sgo->GetContext();
    if (scx)
      scx->LeaveModalState();
  }

  if (mContext) {
    mContext->LeaveModalState();
  }

  // Remember the time of the last dialog quit.
  nsGlobalWindow *inner = topWin->GetCurrentInnerWindowInternal();
  if (inner)
    inner->mLastDialogQuitTime = TimeStamp::Now();
}

bool
nsGlobalWindow::IsInModalState()
{
  nsGlobalWindow *topWin = GetTop();

  if (!topWin) {
    NS_ERROR("Uh, IsInModalState() called w/o a reachable top window?");

    return false;
  }

  return topWin->mModalStateDepth != 0;
}

// static
void
nsGlobalWindow::NotifyDOMWindowDestroyed(nsGlobalWindow* aWindow) {
  nsCOMPtr<nsIObserverService> observerService =
    services::GetObserverService();
  if (observerService) {
    observerService->
      NotifyObservers(static_cast<nsIScriptGlobalObject*>(aWindow),
                      DOM_WINDOW_DESTROYED_TOPIC, nsnull);
  }
}

class WindowDestroyedEvent : public nsRunnable
{
public:
  WindowDestroyedEvent(nsPIDOMWindow* aWindow, PRUint64 aID,
                       const char* aTopic) :
    mID(aID), mTopic(aTopic)
  {
    mWindow = do_GetWeakReference(aWindow);
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      nsCOMPtr<nsISupportsPRUint64> wrapper =
        do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
      if (wrapper) {
        wrapper->SetData(mID);
        observerService->NotifyObservers(wrapper, mTopic.get(), nsnull);
      }
    }

    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    if (window) {
      nsGlobalWindow* currentInner = 
        window->IsInnerWindow() ? static_cast<nsGlobalWindow*>(window.get()) :
                                  static_cast<nsGlobalWindow*>(window->GetCurrentInnerWindow());
      NS_ENSURE_TRUE(currentInner, NS_OK);

      JSObject* obj = currentInner->FastGetGlobalJSObject();
      if (obj) {
        JSContext* cx =
          nsContentUtils::ThreadJSContextStack()->GetSafeJSContext();

        JSAutoRequest ar(cx);

        js::NukeChromeCrossCompartmentWrappersForGlobal(cx, obj,
                                                        window->IsInnerWindow() ? js::DontNukeForGlobalObject :
                                                                                  js::NukeForGlobalObject);
      }
    }

    return NS_OK;
  }

private:
  PRUint64 mID;
  nsCString mTopic;
  nsWeakPtr mWindow;
};

void
nsGlobalWindow::NotifyWindowIDDestroyed(const char* aTopic)
{
  nsRefPtr<nsIRunnable> runnable = new WindowDestroyedEvent(this, mWindowID, aTopic);
  nsresult rv = NS_DispatchToCurrentThread(runnable);
  if (NS_SUCCEEDED(rv)) {
    mNotifiedIDDestroyed = true;
  }
}

// static
void
nsGlobalWindow::NotifyDOMWindowFrozen(nsGlobalWindow* aWindow) {
  if (aWindow && aWindow->IsInnerWindow()) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->
        NotifyObservers(static_cast<nsIScriptGlobalObject*>(aWindow),
                        DOM_WINDOW_FROZEN_TOPIC, nsnull);
    }
  }
}

// static
void
nsGlobalWindow::NotifyDOMWindowThawed(nsGlobalWindow* aWindow) {
  if (aWindow && aWindow->IsInnerWindow()) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->
        NotifyObservers(static_cast<nsIScriptGlobalObject*>(aWindow),
                        DOM_WINDOW_THAWED_TOPIC, nsnull);
    }
  }
}

JSObject*
nsGlobalWindow::GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey)
{
  JSObject* handler = nsnull;
  if (mCachedXBLPrototypeHandlers.IsInitialized()) {
    mCachedXBLPrototypeHandlers.Get(aKey, &handler);
  }
  return handler;
}

void
nsGlobalWindow::CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                         nsScriptObjectHolder<JSObject>& aHandler)
{
  if (!mCachedXBLPrototypeHandlers.IsInitialized()) {
    mCachedXBLPrototypeHandlers.Init();
  }

  if (!mCachedXBLPrototypeHandlers.Count()) {
    // Can't use macros to get the participant because nsGlobalChromeWindow also
    // runs through this code. Use QueryInterface to get the correct objects.
    nsXPCOMCycleCollectionParticipant* participant;
    CallQueryInterface(this, &participant);
    NS_ASSERTION(participant,
                 "Failed to QI to nsXPCOMCycleCollectionParticipant!");

    nsISupports* thisSupports;
    QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                   reinterpret_cast<void**>(&thisSupports));
    NS_ASSERTION(thisSupports, "Failed to QI to nsCycleCollectionISupports!");

    nsresult rv = nsContentUtils::HoldJSObjects(thisSupports, participant);
    if (NS_FAILED(rv)) {
      NS_ERROR("nsContentUtils::HoldJSObjects failed!");
      return;
    }
  }

  mCachedXBLPrototypeHandlers.Put(aKey, aHandler.get());
}

NS_IMETHODIMP
nsGlobalWindow::GetFrameElement(nsIDOMElement** aFrameElement)
{
  FORWARD_TO_OUTER(GetFrameElement, (aFrameElement), NS_ERROR_NOT_INITIALIZED);

  *aFrameElement = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> docShellTI(do_QueryInterface(mDocShell));

  if (!docShellTI) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellTI->GetSameTypeParent(getter_AddRefs(parent));

  if (!parent || parent == docShellTI) {
    // We're at a chrome boundary, don't expose the chrome iframe
    // element to content code.

    return NS_OK;
  }

  *aFrameElement = mFrameElement;
  NS_IF_ADDREF(*aFrameElement);

  return NS_OK;
}

// Helper for converting window.showModalDialog() options (list of ';'
// separated name (:|=) value pairs) to a format that's parsable by
// our normal window opening code.

void
ConvertDialogOptions(const nsAString& aOptions, nsAString& aResult)
{
  nsAString::const_iterator end;
  aOptions.EndReading(end);

  nsAString::const_iterator iter;
  aOptions.BeginReading(iter);

  while (iter != end) {
    // Skip whitespace.
    while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
      ++iter;
    }

    nsAString::const_iterator name_start = iter;

    // Skip characters until we find whitespace, ';', ':', or '='
    while (iter != end && !nsCRT::IsAsciiSpace(*iter) &&
           *iter != ';' &&
           *iter != ':' &&
           *iter != '=') {
      ++iter;
    }

    nsAString::const_iterator name_end = iter;

    // Skip whitespace.
    while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
      ++iter;
    }

    if (*iter == ';') {
      // No value found, skip the ';' and keep going.
      ++iter;

      continue;
    }

    nsAString::const_iterator value_start = iter;
    nsAString::const_iterator value_end = iter;

    if (*iter == ':' || *iter == '=') {
      // We found name followed by ':' or '='. Look for a value.

      iter++; // Skip the ':' or '='

      // Skip whitespace.
      while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
        ++iter;
      }

      value_start = iter;

      // Skip until we find whitespace, or ';'.
      while (iter != end && !nsCRT::IsAsciiSpace(*iter) &&
             *iter != ';') {
        ++iter;
      }

      value_end = iter;

      // Skip whitespace.
      while (nsCRT::IsAsciiSpace(*iter) && iter != end) {
        ++iter;
      }
    }

    const nsDependentSubstring& name = Substring(name_start, name_end);
    const nsDependentSubstring& value = Substring(value_start, value_end);

    if (name.LowerCaseEqualsLiteral("center")) {
      if (value.LowerCaseEqualsLiteral("on")  ||
          value.LowerCaseEqualsLiteral("yes") ||
          value.LowerCaseEqualsLiteral("1")) {
        aResult.AppendLiteral(",centerscreen=1");
      }
    } else if (name.LowerCaseEqualsLiteral("dialogwidth")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",width=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogheight")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",height=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogtop")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",top=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("dialogleft")) {
      if (!value.IsEmpty()) {
        aResult.AppendLiteral(",left=");
        aResult.Append(value);
      }
    } else if (name.LowerCaseEqualsLiteral("resizable")) {
      if (value.LowerCaseEqualsLiteral("on")  ||
          value.LowerCaseEqualsLiteral("yes") ||
          value.LowerCaseEqualsLiteral("1")) {
        aResult.AppendLiteral(",resizable=1");
      }
    } else if (name.LowerCaseEqualsLiteral("scroll")) {
      if (value.LowerCaseEqualsLiteral("off")  ||
          value.LowerCaseEqualsLiteral("no") ||
          value.LowerCaseEqualsLiteral("0")) {
        aResult.AppendLiteral(",scrollbars=0");
      }
    }

    if (iter == end) {
      break;
    }

    iter++;
  }
}

NS_IMETHODIMP
nsGlobalWindow::ShowModalDialog(const nsAString& aURI, nsIVariant *aArgs,
                                const nsAString& aOptions,
                                nsIVariant **aRetVal)
{
  FORWARD_TO_OUTER(ShowModalDialog, (aURI, aArgs, aOptions, aRetVal),
                   NS_ERROR_NOT_INITIALIZED);

  *aRetVal = nsnull;

  if (Preferences::GetBool("dom.disable_window_showModalDialog", false))
    return NS_ERROR_NOT_AVAILABLE;

  // Before bringing up the window/dialog, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  if (AreDialogsBlocked() || !ConfirmDialogAllowed())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIDOMWindow> dlgWin;
  nsAutoString options(NS_LITERAL_STRING("-moz-internal-modal=1,status=1"));

  ConvertDialogOptions(aOptions, options);

  options.AppendLiteral(",scrollbars=1,centerscreen=1,resizable=0");

  nsCOMPtr<nsIDOMWindow> callerWin = EnterModalState();
  PRUint32 oldMicroTaskLevel = nsContentUtils::MicroTaskLevel();
  nsContentUtils::SetMicroTaskLevel(0);
  nsresult rv = OpenInternal(aURI, EmptyString(), options,
                             false,          // aDialog
                             true,           // aContentModal
                             true,           // aCalledNoScript
                             true,           // aDoJSFixups
                             nsnull, aArgs,     // args
                             GetPrincipal(),    // aCalleePrincipal
                             nsnull,            // aJSCallerContext
                             getter_AddRefs(dlgWin));
  nsContentUtils::SetMicroTaskLevel(oldMicroTaskLevel);
  LeaveModalState(callerWin);

  NS_ENSURE_SUCCESS(rv, rv);
  
  if (dlgWin) {
    nsCOMPtr<nsIPrincipal> subjectPrincipal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    if (NS_FAILED(rv)) {
      return rv;
    }

    bool canAccess = true;

    if (subjectPrincipal) {
      nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
        do_QueryInterface(dlgWin);
      nsCOMPtr<nsIPrincipal> dialogPrincipal;

      if (objPrincipal) {
        dialogPrincipal = objPrincipal->GetPrincipal();

        rv = subjectPrincipal->Subsumes(dialogPrincipal, &canAccess);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Uh, not sure what kind of dialog this is. Prevent access to
        // be on the safe side...

        canAccess = false;
      }
    }

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(dlgWin));

    if (canAccess) {
      nsPIDOMWindow *inner = win->GetCurrentInnerWindow();

      nsCOMPtr<nsIDOMModalContentWindow> dlgInner(do_QueryInterface(inner));

      if (dlgInner) {
        dlgInner->GetReturnValue(aRetVal);
      }
    }

    nsRefPtr<nsGlobalWindow> winInternal =
      static_cast<nsGlobalWindow*>(win.get());
    if (winInternal->mCallCleanUpAfterModalDialogCloses) {
      winInternal->mCallCleanUpAfterModalDialogCloses = false;
      winInternal->CleanUp(true);
    }
  }
  
  return NS_OK;
}

class CommandDispatcher : public nsRunnable
{
public:
  CommandDispatcher(nsIDOMXULCommandDispatcher* aDispatcher,
                    const nsAString& aAction)
  : mDispatcher(aDispatcher), mAction(aAction) {}

  NS_IMETHOD Run()
  {
    return mDispatcher->UpdateCommands(mAction);
  }

  nsCOMPtr<nsIDOMXULCommandDispatcher> mDispatcher;
  nsString                             mAction;
};

NS_IMETHODIMP
nsGlobalWindow::UpdateCommands(const nsAString& anAction)
{
  nsPIDOMWindow *rootWindow = nsGlobalWindow::GetPrivateRoot();
  if (!rootWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(rootWindow->GetExtantDocument());
  // See if we contain a XUL document.
  if (xulDoc) {
    // Retrieve the command dispatcher and call updateCommands on it.
    nsCOMPtr<nsIDOMXULCommandDispatcher> xulCommandDispatcher;
    xulDoc->GetCommandDispatcher(getter_AddRefs(xulCommandDispatcher));
    if (xulCommandDispatcher) {
      nsContentUtils::AddScriptRunner(new CommandDispatcher(xulCommandDispatcher,
                                                            anAction));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelection(nsISelection** aSelection)
{
  FORWARD_TO_OUTER(GetSelection, (aSelection), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aSelection);
  *aSelection = nsnull;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return NS_OK;
    
  *aSelection = presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  
  NS_IF_ADDREF(*aSelection);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Find(const nsAString& aStr, bool aCaseSensitive,
                     bool aBackwards, bool aWrapAround, bool aWholeWord,
                     bool aSearchInFrames, bool aShowDialog,
                     bool *aDidFind)
{
  if (Preferences::GetBool("dom.disable_window_find", false))
    return NS_ERROR_NOT_AVAILABLE;

  FORWARD_TO_OUTER(Find, (aStr, aCaseSensitive, aBackwards, aWrapAround,
                          aWholeWord, aSearchInFrames, aShowDialog, aDidFind),
                   NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  *aDidFind = false;

  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(finder, NS_ERROR_FAILURE);

  // Set the options of the search
  rv = finder->SetSearchString(PromiseFlatString(aStr).get());
  NS_ENSURE_SUCCESS(rv, rv);
  finder->SetMatchCase(aCaseSensitive);
  finder->SetFindBackwards(aBackwards);
  finder->SetWrapFind(aWrapAround);
  finder->SetEntireWord(aWholeWord);
  finder->SetSearchFrames(aSearchInFrames);

  // the nsIWebBrowserFind is initialized to use this window
  // as the search root, but uses focus to set the current search
  // frame. If we're being called from JS (as here), this window
  // should be the current search frame.
  nsCOMPtr<nsIWebBrowserFindInFrames> framesFinder(do_QueryInterface(finder));
  if (framesFinder) {
    framesFinder->SetRootSearchFrame(this);   // paranoia
    framesFinder->SetCurrentSearchFrame(this);
  }
  
  // The Find API does not accept empty strings. Launch the Find Dialog.
  if (aStr.IsEmpty() || aShowDialog) {
    // See if the find dialog is already up using nsIWindowMediator
    nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

    nsCOMPtr<nsIDOMWindow> findDialog;

    if (windowMediator) {
      windowMediator->GetMostRecentWindow(NS_LITERAL_STRING("findInPage").get(),
                                          getter_AddRefs(findDialog));
    }

    if (findDialog) {
      // The Find dialog is already open, bring it to the top.
      rv = findDialog->Focus();
    } else { // Open a Find dialog
      if (finder) {
        nsCOMPtr<nsIDOMWindow> dialog;
        rv = OpenDialog(NS_LITERAL_STRING("chrome://global/content/finddialog.xul"),
                        NS_LITERAL_STRING("_blank"),
                        NS_LITERAL_STRING("chrome, resizable=no, dependent=yes"),
                        finder, getter_AddRefs(dialog));
      }
    }
  } else {
    // Launch the search with the passed in search string
    rv = finder->FindNext(aDidFind);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiBase64String, aBinaryData);
}

NS_IMETHODIMP
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    bool aUseCapture)
{
  nsRefPtr<nsEventListenerManager> elm = GetListenerManager(false);
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsGlobalWindow)

NS_IMETHODIMP
nsGlobalWindow::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, aRetVal), NS_OK);

  if (!mDoc) {
    return NS_ERROR_FAILURE;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(GetOuterWindow(), nsnull, aEvent,
                                        presContext, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener *aListener,
                                 bool aUseCapture, bool aWantsUntrusted,
                                 PRUint8 aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  if (IsOuterWindow() && mInnerWindow &&
      !nsContentUtils::CanCallerAccess(mInnerWindow)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!aWantsUntrusted &&
      (aOptionalArgc < 2 && !nsContentUtils::IsChromeDoc(mDoc))) {
    aWantsUntrusted = true;
  }

  nsEventListenerManager* manager = GetListenerManager(true);
  NS_ENSURE_STATE(manager);
  manager->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::AddSystemEventListener(const nsAString& aType,
                                       nsIDOMEventListener *aListener,
                                       bool aUseCapture,
                                       bool aWantsUntrusted,
                                       PRUint8 aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  if (IsOuterWindow() && mInnerWindow &&
      !nsContentUtils::CanCallerAccess(mInnerWindow)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!aWantsUntrusted &&
      (aOptionalArgc < 2 && !nsContentUtils::IsChromeDoc(mDoc))) {
    aWantsUntrusted = true;
  }

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

nsEventListenerManager*
nsGlobalWindow::GetListenerManager(bool aCreateIfNotFound)
{
  FORWARD_TO_INNER_CREATE(GetListenerManager, (aCreateIfNotFound), nsnull);

  if (!mListenerManager && aCreateIfNotFound) {
    mListenerManager =
      new nsEventListenerManager(static_cast<nsIDOMEventTarget*>(this));
  }

  return mListenerManager;
}

nsIScriptContext*
nsGlobalWindow::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = NS_ERROR_UNEXPECTED;
  if (IsInnerWindow()) {
    nsPIDOMWindow* outer = GetOuterWindow();
    NS_ENSURE_TRUE(outer && outer->GetCurrentInnerWindow() == this, nsnull);
  }

  nsIScriptContext* scx;
  if ((scx = GetContext())) {
    *aRv = NS_OK;
    return scx;
  }
  return nsnull;
}

//*****************************************************************************
// nsGlobalWindow::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindow*
nsGlobalWindow::GetPrivateParent()
{
  FORWARD_TO_OUTER(GetPrivateParent, (), nsnull);

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (static_cast<nsIDOMWindow *>(this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nsnull;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetDocument();
    if (!doc)
      return nsnull;             // This is ok, just means a null parent.

    nsIScriptGlobalObject *globalObject = doc->GetScriptGlobalObject();
    if (!globalObject)
      return nsnull;             // This is ok, just means a null parent.

    parent = do_QueryInterface(globalObject);
  }

  if (parent) {
    return static_cast<nsGlobalWindow *>
                      (static_cast<nsIDOMWindow*>(parent.get()));
  }

  return nsnull;
}

nsPIDOMWindow*
nsGlobalWindow::GetPrivateRoot()
{
  FORWARD_TO_OUTER(GetPrivateRoot, (), nsnull);

  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  NS_ASSERTION(ptop, "cannot get ptop");
  if (!ptop)
    return nsnull;

  nsIDocShell *docShell = ptop->GetDocShell();

  // Get the chrome event handler from the doc shell, since we only
  // want to deal with XUL chrome handlers and not the new kind of
  // window root handler.
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;
  docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetDocument();
    if (doc) {
      nsIDOMWindow *parent = doc->GetWindow();
      if (parent) {
        parent->GetTop(getter_AddRefs(top));
      }
    }
  }

  return static_cast<nsGlobalWindow *>
                    (static_cast<nsIDOMWindow *>(top));
}


NS_IMETHODIMP
nsGlobalWindow::GetLocation(nsIDOMLocation ** aLocation)
{
  FORWARD_TO_INNER(GetLocation, (aLocation), NS_ERROR_NOT_INITIALIZED);

  *aLocation = nsnull;

  nsIDocShell *docShell = GetDocShell();
  if (!mLocation && docShell) {
    mLocation = new nsLocation(docShell);
    if (!mLocation) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aLocation = mLocation);

  return NS_OK;
}

void
nsGlobalWindow::ActivateOrDeactivate(bool aActivate)
{
  // Set / unset mIsActive on the top level window, which is used for the
  // :-moz-window-inactive pseudoclass.
  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  if (!mainWidget)
    return;

  // Get the top level widget (if the main widget is a sheet, this will
  // be the sheet's top (non-sheet) parent).
  nsCOMPtr<nsIWidget> topLevelWidget = mainWidget->GetSheetWindowParent();
  if (!topLevelWidget) {
    topLevelWidget = mainWidget;
  }

  // Get the top level widget's nsGlobalWindow
  nsCOMPtr<nsIDOMWindow> topLevelWindow;
  if (topLevelWidget == mainWidget) {
    topLevelWindow = static_cast<nsIDOMWindow*>(this);
  } else {
    // This is a workaround for the following problem:
    // When a window with an open sheet loses focus, only the sheet window
    // receives the NS_DEACTIVATE event. However, it's not the sheet that
    // should lose the active styling, but the containing top level window.
    void* clientData;
    topLevelWidget->GetClientData(clientData); // clientData is nsXULWindow
    nsISupports* data = static_cast<nsISupports*>(clientData);
    nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(data));
    topLevelWindow = do_GetInterface(req);
  }
  if (topLevelWindow) {
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(topLevelWindow));
    piWin->SetActive(aActivate);
  }
}

static bool
NotifyDocumentTree(nsIDocument* aDocument, void* aData)
{
  aDocument->EnumerateSubDocuments(NotifyDocumentTree, nsnull);
  aDocument->DocumentStatesChanged(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
  return true;
}

void
nsGlobalWindow::SetActive(bool aActive)
{
  nsPIDOMWindow::SetActive(aActive);
  NotifyDocumentTree(mDoc, nsnull);
}

void nsGlobalWindow::SetIsBackground(bool aIsBackground)
{
  bool resetTimers = (!aIsBackground && IsBackground());
  nsPIDOMWindow::SetIsBackground(aIsBackground);
  if (resetTimers) {
    ResetTimersForNonBackgroundWindow();
  }
}

void nsGlobalWindow::MaybeUpdateTouchState()
{
  FORWARD_TO_INNER_VOID(MaybeUpdateTouchState, ());

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();

  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));

  if(this == focusedWindow) {
    UpdateTouchState();
  }

  if (mMayHaveTouchEventListener) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

    if (observerService) {
      observerService->NotifyObservers(static_cast<nsIDOMWindow*>(this),
                                       DOM_TOUCH_LISTENER_ADDED,
                                       nsnull);
    }
  }
}

void nsGlobalWindow::UpdateTouchState()
{
  FORWARD_TO_INNER_VOID(UpdateTouchState, ());

  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  if (!mainWidget) {
    return;
  }

  if (mMayHaveTouchEventListener) {
    mainWidget->RegisterTouchWindow();
  } else {
    mainWidget->UnregisterTouchWindow();
  }
}

void
nsGlobalWindow::SetChromeEventHandler(nsIDOMEventTarget* aChromeEventHandler)
{
  SetChromeEventHandlerInternal(aChromeEventHandler);
  if (IsOuterWindow()) {
    // update the chrome event handler on all our inner windows
    for (nsGlobalWindow *inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
         inner != this;
         inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
      NS_ASSERTION(!inner->mOuterWindow || inner->mOuterWindow == this,
                   "bad outer window pointer");
      inner->SetChromeEventHandlerInternal(aChromeEventHandler);
    }
  } else if (mOuterWindow) {
    // Need the cast to be able to call the protected method on a
    // superclass. We could make the method public instead, but it's really
    // better this way.
    static_cast<nsGlobalWindow*>(mOuterWindow.get())->
      SetChromeEventHandlerInternal(aChromeEventHandler);
  }
}

static bool IsLink(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aContent);
  return (anchor || (aContent &&
                     aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                           nsGkAtoms::simple, eCaseMatters)));
}

void
nsGlobalWindow::SetFocusedNode(nsIContent* aNode,
                               PRUint32 aFocusMethod,
                               bool aNeedsFocus)
{
  FORWARD_TO_INNER_VOID(SetFocusedNode, (aNode, aFocusMethod, aNeedsFocus));

  if (aNode && aNode->GetCurrentDoc() != mDoc) {
    NS_WARNING("Trying to set focus to a node from a wrong document");
    return;
  }

  if (mCleanedUp) {
    NS_ASSERTION(!aNode, "Trying to focus cleaned up window!");
    aNode = nsnull;
    aNeedsFocus = false;
  }
  if (mFocusedNode != aNode) {
    UpdateCanvasFocus(false, aNode);
    mFocusedNode = aNode;
    mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;
    mShowFocusRingForContent = false;
  }

  if (mFocusedNode) {
    // if a node was focused by a keypress, turn on focus rings for the
    // window.
    if (mFocusMethod & nsIFocusManager::FLAG_BYKEY) {
      mFocusByKeyOccurred = true;
    } else if (
      // otherwise, we set mShowFocusRingForContent, as we don't want this to
      // be permanent for the window. On Windows, focus rings are only shown
      // when the FLAG_SHOWRING flag is used. On other platforms, focus rings
      // are only hidden for clicks on links.
#ifndef XP_WIN
      !(mFocusMethod & nsIFocusManager::FLAG_BYMOUSE) || !IsLink(aNode) ||
#endif
      aFocusMethod & nsIFocusManager::FLAG_SHOWRING) {
        mShowFocusRingForContent = true;
    }
  }

  if (aNeedsFocus)
    mNeedsFocus = aNeedsFocus;
}

PRUint32
nsGlobalWindow::GetFocusMethod()
{
  FORWARD_TO_INNER(GetFocusMethod, (), 0);

  return mFocusMethod;
}

bool
nsGlobalWindow::ShouldShowFocusRing()
{
  FORWARD_TO_INNER(ShouldShowFocusRing, (), false);

  return mShowFocusRings || mShowFocusRingForContent || mFocusByKeyOccurred;
}

void
nsGlobalWindow::SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                      UIStateChangeType aShowFocusRings)
{
  FORWARD_TO_INNER_VOID(SetKeyboardIndicators, (aShowAccelerators, aShowFocusRings));

  bool oldShouldShowFocusRing = ShouldShowFocusRing();

  // only change the flags that have been modified
  if (aShowAccelerators != UIStateChangeType_NoChange)
    mShowAccelerators = aShowAccelerators == UIStateChangeType_Set;
  if (aShowFocusRings != UIStateChangeType_NoChange)
    mShowFocusRings = aShowFocusRings == UIStateChangeType_Set;

  // propagate the indicators to child windows
  nsCOMPtr<nsIDocShellTreeNode> node = do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      nsCOMPtr<nsPIDOMWindow> childWindow = do_GetInterface(childShell);
      if (childWindow) {
        childWindow->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
      }
    }
  }

  bool newShouldShowFocusRing = ShouldShowFocusRing();
  if (mHasFocus && mFocusedNode &&
      oldShouldShowFocusRing != newShouldShowFocusRing &&
      mFocusedNode->IsElement()) {
    // Update mFocusedNode's state.
    if (newShouldShowFocusRing) {
      mFocusedNode->AsElement()->AddStates(NS_EVENT_STATE_FOCUSRING);
    } else {
      mFocusedNode->AsElement()->RemoveStates(NS_EVENT_STATE_FOCUSRING);
    }
  }
}

void
nsGlobalWindow::GetKeyboardIndicators(bool* aShowAccelerators,
                                      bool* aShowFocusRings)
{
  FORWARD_TO_INNER_VOID(GetKeyboardIndicators, (aShowAccelerators, aShowFocusRings));

  *aShowAccelerators = mShowAccelerators;
  *aShowFocusRings = mShowFocusRings;
}

bool
nsGlobalWindow::TakeFocus(bool aFocus, PRUint32 aFocusMethod)
{
  FORWARD_TO_INNER(TakeFocus, (aFocus, aFocusMethod), false);

  if (mCleanedUp) {
    return false;
  }
  
  if (aFocus)
    mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;

  if (mHasFocus != aFocus) {
    mHasFocus = aFocus;
    UpdateCanvasFocus(true, mFocusedNode);
  }

  // if mNeedsFocus is true, then the document has not yet received a
  // document-level focus event. If there is a root content node, then return
  // true to tell the calling focus manager that a focus event is expected. If
  // there is no root content node, the document hasn't loaded enough yet, or
  // there isn't one and there is no point in firing a focus event.
  if (aFocus && mNeedsFocus && mDoc && mDoc->GetRootElement() != nsnull) {
    mNeedsFocus = false;
    return true;
  }

  mNeedsFocus = false;
  return false;
}

void
nsGlobalWindow::SetReadyForFocus()
{
  FORWARD_TO_INNER_VOID(SetReadyForFocus, ());

  bool oldNeedsFocus = mNeedsFocus;
  mNeedsFocus = false;

  // update whether focus rings need to be shown using the state from the
  // root window
  nsPIDOMWindow* root = GetPrivateRoot();
  if (root) {
    bool showAccelerators, showFocusRings;
    root->GetKeyboardIndicators(&showAccelerators, &showFocusRings);
    mShowFocusRings = showFocusRings;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->WindowShown(this, oldNeedsFocus);
}

void
nsGlobalWindow::PageHidden()
{
  FORWARD_TO_INNER_VOID(PageHidden, ());

  // the window is being hidden, so tell the focus manager that the frame is
  // no longer valid. Use the persisted field to determine if the document
  // is being destroyed.

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->WindowHidden(this);

  mNeedsFocus = true;
}

class HashchangeCallback : public nsRunnable
{
public:
  HashchangeCallback(const nsAString &aOldURL,
                     const nsAString &aNewURL,
                     nsGlobalWindow* aWindow)
    : mWindow(aWindow)
  {
    mOldURL.Assign(aOldURL);
    mNewURL.Assign(aNewURL);
  }

  NS_IMETHOD Run()
  {
    NS_PRECONDITION(NS_IsMainThread(), "Should be called on the main thread.");
    return mWindow->FireHashchange(mOldURL, mNewURL);
  }

private:
  nsString mOldURL;
  nsString mNewURL;
  nsRefPtr<nsGlobalWindow> mWindow;
};

nsresult
nsGlobalWindow::DispatchAsyncHashchange(nsIURI *aOldURI, nsIURI *aNewURI)
{
  FORWARD_TO_INNER(DispatchAsyncHashchange, (aOldURI, aNewURI), NS_OK);

  // Make sure that aOldURI and aNewURI are identical up to the '#', and that
  // their hashes are different.
  nsCAutoString oldBeforeHash, oldHash, newBeforeHash, newHash;
  nsContentUtils::SplitURIAtHash(aOldURI, oldBeforeHash, oldHash);
  nsContentUtils::SplitURIAtHash(aNewURI, newBeforeHash, newHash);

  NS_ENSURE_STATE(oldBeforeHash.Equals(newBeforeHash));
  NS_ENSURE_STATE(!oldHash.Equals(newHash));

  nsCAutoString oldSpec, newSpec;
  aOldURI->GetSpec(oldSpec);
  aNewURI->GetSpec(newSpec);

  NS_ConvertUTF8toUTF16 oldWideSpec(oldSpec);
  NS_ConvertUTF8toUTF16 newWideSpec(newSpec);

  nsCOMPtr<nsIRunnable> callback =
    new HashchangeCallback(oldWideSpec, newWideSpec, this);
  return NS_DispatchToMainThread(callback);
}

nsresult
nsGlobalWindow::FireHashchange(const nsAString &aOldURL,
                               const nsAString &aNewURL)
{
  NS_ENSURE_TRUE(IsInnerWindow(), NS_ERROR_FAILURE);

  // Don't do anything if the window is frozen.
  if (IsFrozen())
    return NS_OK;

  // Get a presentation shell for use in creating the hashchange event.
  NS_ENSURE_STATE(mDoc);

  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  // Create a new hashchange event.
  nsCOMPtr<nsIDOMEvent> domEvent;
  nsresult rv =
    nsEventDispatcher::CreateEvent(presContext, nsnull,
                                   NS_LITERAL_STRING("hashchangeevent"),
                                   getter_AddRefs(domEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(privateEvent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMHashChangeEvent> hashchangeEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(hashchangeEvent, NS_ERROR_UNEXPECTED);

  // The hashchange event bubbles and isn't cancellable.
  rv = hashchangeEvent->InitHashChangeEvent(NS_LITERAL_STRING("hashchange"),
                                            true, false,
                                            aOldURL, aNewURL);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateEvent->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  return DispatchEvent(hashchangeEvent, &dummy);
}

nsresult
nsGlobalWindow::DispatchSyncPopState()
{
  FORWARD_TO_INNER(DispatchSyncPopState, (), NS_OK);

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be safe to run script here.");

  // Check that PopState hasn't been pref'ed off.
  if (!Preferences::GetBool(sPopStatePrefStr, false)) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // Bail if the window is frozen.
  if (IsFrozen()) {
    return NS_OK;
  }

  // Get the document's pending state object -- it contains the data we're
  // going to send along with the popstate event.  The object is serialized
  // using structured clone.
  nsCOMPtr<nsIVariant> stateObj;
  rv = mDoc->GetStateObject(getter_AddRefs(stateObj));
  NS_ENSURE_SUCCESS(rv, rv);

  // Obtain a presentation shell for use in creating a popstate event.
  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  // Create a new popstate event
  nsCOMPtr<nsIDOMEvent> domEvent;
  rv = nsEventDispatcher::CreateEvent(presContext, nsnull,
                                      NS_LITERAL_STRING("popstateevent"),
                                      getter_AddRefs(domEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(privateEvent, NS_ERROR_FAILURE);

  // Initialize the popstate event, which does bubble but isn't cancellable.
  nsCOMPtr<nsIDOMPopStateEvent> popstateEvent = do_QueryInterface(domEvent);
  rv = popstateEvent->InitPopStateEvent(NS_LITERAL_STRING("popstate"),
                                        true, false,
                                        stateObj);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = privateEvent->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> outerWindow =
    do_QueryInterface(GetOuterWindow());
  NS_ENSURE_TRUE(outerWindow, NS_ERROR_UNEXPECTED);

  rv = privateEvent->SetTarget(outerWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy; // default action
  return DispatchEvent(popstateEvent, &dummy);
}

// Find an nsICanvasFrame under aFrame.  Only search the principal
// child lists.  aFrame must be non-null.
static nsCanvasFrame* FindCanvasFrame(nsIFrame* aFrame)
{
    nsCanvasFrame* canvasFrame = do_QueryFrame(aFrame);
    if (canvasFrame) {
        return canvasFrame;
    }

    nsIFrame* kid = aFrame->GetFirstPrincipalChild();
    while (kid) {
        canvasFrame = FindCanvasFrame(kid);
        if (canvasFrame) {
            return canvasFrame;
        }
        kid = kid->GetNextSibling();
    }

    return nsnull;
}

//-------------------------------------------------------
// Tells the HTMLFrame/CanvasFrame that is now has focus
void
nsGlobalWindow::UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent)
{
  // this is called from the inner window so use GetDocShell
  nsIDocShell* docShell = GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIEditorDocShell> editorDocShell = do_QueryInterface(docShell);
  if (editorDocShell) {
    bool editable;
    editorDocShell->GetEditable(&editable);
    if (editable)
      return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell || !mDocument)
    return;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  Element *rootElement = doc->GetRootElement();
  if (rootElement) {
      if ((mHasFocus || aFocusChanged) &&
          (mFocusedNode == rootElement || aNewContent == rootElement)) {
          nsIFrame* frame = rootElement->GetPrimaryFrame();
          if (frame) {
              frame = frame->GetParent();
              nsCanvasFrame* canvasFrame = do_QueryFrame(frame);
              if (canvasFrame) {
                  canvasFrame->SetHasFocus(mHasFocus && rootElement == aNewContent);
              }
          }
      }
  } else {
      // Look for the frame the hard way
      nsIFrame* frame = presShell->GetRootFrame();
      if (frame) {
          nsCanvasFrame* canvasFrame = FindCanvasFrame(frame);
          if (canvasFrame) {
              canvasFrame->SetHasFocus(false);
          }
      }      
  }
}

NS_IMETHODIMP
nsGlobalWindow::GetComputedStyle(nsIDOMElement* aElt,
                                 const nsAString& aPseudoElt,
                                 nsIDOMCSSStyleDeclaration** aReturn)
{
  FORWARD_TO_OUTER(GetComputedStyle, (aElt, aPseudoElt, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!aElt) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  nsRefPtr<nsComputedDOMStyle> compStyle;
  nsresult rv = NS_NewComputedDOMStyle(aElt, aPseudoElt, presShell,
                                       getter_AddRefs(compStyle));
  NS_ENSURE_SUCCESS(rv, rv);

  *aReturn = compStyle.forget().get();

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetSessionStorage(nsIDOMStorage ** aSessionStorage)
{
  FORWARD_TO_INNER(GetSessionStorage, (aSessionStorage), NS_ERROR_UNEXPECTED);

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell) {
    *aSessionStorage = nsnull;
    return NS_OK;
  }

  if (!Preferences::GetBool(kStorageEnabled)) {
    *aSessionStorage = nsnull;
    return NS_OK;
  }

  if (mSessionStorage) {
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p has %p sessionStorage", this, mSessionStorage.get());
    }
#endif
    nsCOMPtr<nsPIDOMStorage> piStorage = do_QueryInterface(mSessionStorage);
    if (piStorage) {
      bool canAccess = piStorage->CanAccess(principal);
      NS_ASSERTION(canAccess,
                   "window %x owned sessionStorage "
                   "that could not be accessed!");
      if (!canAccess) {
          mSessionStorage = nsnull;
      }
    }
  }

  if (!mSessionStorage) {
    *aSessionStorage = nsnull;

    nsString documentURI;
    if (mDocument) {
      mDocument->GetDocumentURI(documentURI);
    }

    nsresult rv = docShell->GetSessionStorageForPrincipal(principal,
                                                          documentURI,
                                                          true,
                                                          getter_AddRefs(mSessionStorage));
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p tried to get a new sessionStorage %p", this, mSessionStorage.get());
    }
#endif

    if (!mSessionStorage) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p returns %p sessionStorage", this, mSessionStorage.get());
    }
#endif

  NS_ADDREF(*aSessionStorage = mSessionStorage);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocalStorage(nsIDOMStorage ** aLocalStorage)
{
  FORWARD_TO_INNER(GetLocalStorage, (aLocalStorage), NS_ERROR_UNEXPECTED);

  NS_ENSURE_ARG(aLocalStorage);

  if (!Preferences::GetBool(kStorageEnabled)) {
    *aLocalStorage = nsnull;
    return NS_OK;
  }

  if (!mLocalStorage) {
    *aLocalStorage = nsnull;

    nsresult rv;

    bool unused;
    if (!nsDOMStorage::CanUseStorage(&unused))
      return NS_ERROR_DOM_SECURITY_ERR;

    nsIPrincipal *principal = GetPrincipal();
    if (!principal)
      return NS_OK;

    nsCOMPtr<nsIDOMStorageManager> storageManager =
      do_GetService("@mozilla.org/dom/storagemanager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString documentURI;
    if (mDocument) {
      mDocument->GetDocumentURI(documentURI);
    }

    rv = storageManager->GetLocalStorageForPrincipal(principal,
                                                     documentURI,
                                                     getter_AddRefs(mLocalStorage));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aLocalStorage = mLocalStorage);
  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMStorageIndexedDB
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetMozIndexedDB(nsIIDBFactory** _retval)
{
  if (!mIndexedDB) {
    if (!IsChromeWindow()) {
      nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID);
      NS_ENSURE_TRUE(thirdPartyUtil, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      bool isThirdParty;
      nsresult rv = thirdPartyUtil->IsThirdPartyWindow(this, nsnull,
                                                       &isThirdParty);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      if (isThirdParty) {
        NS_WARNING("IndexedDB is not permitted in a third-party window.");
        *_retval = nsnull;
        return NS_OK;
      }
    }

    mIndexedDB = indexedDB::IDBFactory::Create(this);
    NS_ENSURE_TRUE(mIndexedDB, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  nsCOMPtr<nsIIDBFactory> request(mIndexedDB);
  request.forget(_retval);
  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetInterface(const nsIID & aIID, void **aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDocCharset))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(mDocShell));
      if (docCharset) {
        NS_WARNING("Using deprecated nsIDocCharset: use nsIDocShell.GetCharset() instead ");
        *aSink = docCharset;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
      if (webNav) {
        *aSink = webNav;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        if (webBrowserPrint) {
          *aSink = webBrowserPrint;
          NS_ADDREF(((nsISupports *) *aSink));
        }
      }
    }
  }
#endif
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (!mWindowUtils) {
      mWindowUtils = new nsDOMWindowUtils(this);
    }

    *aSink = mWindowUtils;
    NS_ADDREF(((nsISupports *) *aSink));
  }
  else {
    return QueryInterface(aIID, aSink);
  }

  return *aSink ? NS_OK : NS_ERROR_NO_INTERFACE;
}

void
nsGlobalWindow::FireOfflineStatusEvent()
{
  if (!mDoc)
    return;
  nsAutoString name;
  if (NS_IsOffline()) {
    name.AssignLiteral("offline");
  } else {
    name.AssignLiteral("online");
  }
  // The event is fired at the body element, or if there is no body element,
  // at the document.
  nsCOMPtr<nsISupports> eventTarget = mDoc.get();
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDoc);
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> body;
    htmlDoc->GetBody(getter_AddRefs(body));
    if (body) {
      eventTarget = body;
    }
  }
  else {
    nsCOMPtr<nsIDOMElement> documentElement;
    mDocument->GetDocumentElement(getter_AddRefs(documentElement));
    if(documentElement) {        
      eventTarget = documentElement;
    }
  }
  nsContentUtils::DispatchTrustedEvent(mDoc, eventTarget, name, true, false);
}

nsresult
nsGlobalWindow::Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    if (IsFrozen()) {
      // if an even number of notifications arrive while we're frozen,
      // we don't need to fire.
      mFireOfflineStatusChangeEventOnThaw = !mFireOfflineStatusChangeEventOnThaw;
    } else {
      FireOfflineStatusEvent();
    }
    return NS_OK;
  }

  if (IsInnerWindow() && !nsCRT::strcmp(aTopic, "dom-storage2-changed")) {
    nsIPrincipal *principal;
    nsresult rv;

    nsCOMPtr<nsIDOMStorageEvent> event = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMStorage> changingStorage;
    rv = event->GetStorageArea(getter_AddRefs(changingStorage));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(changingStorage);
    nsPIDOMStorage::nsDOMStorageType storageType = pistorage->StorageType();

    bool fireMozStorageChanged = false;
    principal = GetPrincipal();
    switch (storageType)
    {
    case nsPIDOMStorage::SessionStorage:
    {
      nsCOMPtr<nsIDOMStorage> storage = mSessionStorage;
      if (!storage) {
        nsIDocShell* docShell = GetDocShell();
        if (principal && docShell) {
          // No need to pass documentURI here, it's only needed when we want
          // to create a new storage, the third paramater would be true
          docShell->GetSessionStorageForPrincipal(principal,
                                                  EmptyString(),
                                                  false,
                                                  getter_AddRefs(storage));
        }
      }

      if (!pistorage->IsForkOf(storage)) {
        // This storage event is coming from a different doc shell,
        // i.e. it is a clone, ignore this event.
        return NS_OK;
      }

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
        PR_LogPrint("nsGlobalWindow %p with sessionStorage %p passing event from %p", this, mSessionStorage.get(), pistorage.get());
      }
#endif

      fireMozStorageChanged = SameCOMIdentity(mSessionStorage, changingStorage);
      break;
    }
    case nsPIDOMStorage::LocalStorage:
    {
      // Allow event fire only for the same principal storages
      // XXX We have to use EqualsIgnoreDomain after bug 495337 lands
      nsIPrincipal *storagePrincipal = pistorage->Principal();
      bool equals;

      rv = storagePrincipal->Equals(principal, &equals);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!equals)
        return NS_OK;

      fireMozStorageChanged = SameCOMIdentity(mLocalStorage, changingStorage);
      break;
    }
    default:
      return NS_OK;
    }

    // Clone the storage event included in the observer notification. We want
    // to dispatch clones rather than the original event.
    rv = CloneStorageEvent(fireMozStorageChanged ?
                           NS_LITERAL_STRING("MozStorageChanged") :
                           NS_LITERAL_STRING("storage"),
                           event);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    privateEvent->SetTrusted(true);

    if (fireMozStorageChanged) {
      nsEvent *internalEvent = privateEvent->GetInternalNSEvent();
      internalEvent->flags |= NS_EVENT_FLAG_ONLY_CHROME_DISPATCH;
    }

    if (IsFrozen()) {
      // This window is frozen, rather than firing the events here,
      // store the domain in which the change happened and fire the
      // events if we're ever thawed.

      mPendingStorageEvents.AppendObject(event);
      return NS_OK;
    }

    bool defaultActionEnabled;
    DispatchEvent((nsIDOMStorageEvent *)event, &defaultActionEnabled);

    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "offline-cache-update-added")) {
    if (mApplicationCache)
      return NS_OK;

    // Instantiate the application object now. It observes update belonging to
    // this window's document and correctly updates the applicationCache object
    // state.
    nsCOMPtr<nsIDOMOfflineResourceList> applicationCache;
    GetApplicationCache(getter_AddRefs(applicationCache));
    nsCOMPtr<nsIObserver> observer = do_QueryInterface(applicationCache);
    if (observer)
      observer->Observe(aSubject, aTopic, aData);

    return NS_OK;
  }

  NS_WARNING("unrecognized topic in nsGlobalWindow::Observe");
  return NS_ERROR_FAILURE;
}

nsresult
nsGlobalWindow::CloneStorageEvent(const nsAString& aType,
                                  nsCOMPtr<nsIDOMStorageEvent>& aEvent)
{
  nsresult rv;

  bool canBubble;
  bool cancelable;
  nsAutoString key;
  nsAutoString oldValue;
  nsAutoString newValue;
  nsAutoString url;
  nsCOMPtr<nsIDOMStorage> storageArea;

  nsCOMPtr<nsIDOMEvent> domEvent = do_QueryInterface(aEvent, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  domEvent->GetBubbles(&canBubble);
  domEvent->GetCancelable(&cancelable);

  aEvent->GetKey(key);
  aEvent->GetOldValue(oldValue);
  aEvent->GetNewValue(newValue);
  aEvent->GetUrl(url);
  aEvent->GetStorageArea(getter_AddRefs(storageArea));

  aEvent = new nsDOMStorageEvent();
  return aEvent->InitStorageEvent(aType, canBubble, cancelable,
                                  key, oldValue, newValue,
                                  url, storageArea);
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

  for (PRInt32 i = 0; i < mPendingStorageEvents.Count(); ++i) {
    Observe(mPendingStorageEvents[i], "dom-storage2-changed", nsnull);
  }

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->FirePendingEvents();
  }

  if (mFireOfflineStatusChangeEventOnThaw) {
    mFireOfflineStatusChangeEventOnThaw = false;
    FireOfflineStatusEvent();
  }

  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));
        win->FireDelayedDOMEvents();
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow: Window Control Functions
//*****************************************************************************

nsIDOMWindow *
nsGlobalWindow::GetParentInternal()
{
  FORWARD_TO_OUTER(GetParentInternal, (), nsnull);

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (parent && parent != static_cast<nsIDOMWindow *>(this)) {
    return parent;
  }

  return NULL;
}

// static
void
nsGlobalWindow::CloseBlockScriptTerminationFunc(nsISupports *aRef)
{
  nsGlobalWindow* pwin = static_cast<nsGlobalWindow*>
                                    (static_cast<nsPIDOMWindow*>(aRef));
  pwin->mBlockScriptedClosingFlag = false;
}

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, bool aDialog,
                             bool aContentModal, bool aCalledNoScript,
                             bool aDoJSFixups, nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsIPrincipal *aCalleePrincipal,
                             JSContext *aJSCallerContext,
                             nsIDOMWindow **aReturn)
{
  FORWARD_TO_OUTER(OpenInternal, (aUrl, aName, aOptions, aDialog,
                                  aContentModal, aCalledNoScript, aDoJSFixups,
                                  argv, aExtraArgument, aCalleePrincipal,
                                  aJSCallerContext, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

#ifdef NS_DEBUG
  PRUint32 argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif
  NS_PRECONDITION(!aExtraArgument || (!argv && argc == 0),
                  "Can't pass in arguments both ways");
  NS_PRECONDITION(!aCalledNoScript || (!argv && argc == 0),
                  "Can't pass JS args when called via the noscript methods");
  NS_PRECONDITION(!aJSCallerContext || !aCalledNoScript,
                  "Shouldn't have caller context when called noscript");

  *aReturn = nsnull;

  nsCOMPtr<nsIWebBrowserChrome> chrome;
  GetWebBrowserChrome(getter_AddRefs(chrome));
  if (!chrome) {
    // No chrome means we don't want to go through with this open call
    // -- see nsIWindowWatcher.idl
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mDocShell, "Must have docshell here");

  const bool checkForPopup = !nsContentUtils::IsCallerChrome() &&
    !aDialog && !WindowExists(aName, !aCalledNoScript);

  // Note: it's very important that this be an nsXPIDLCString, since we want
  // .get() on it to return nsnull until we write stuff to it.  The window
  // watcher expects a null URL string if there is no URL to load.
  nsXPIDLCString url;
  nsresult rv = NS_OK;

  // It's important to do this security check before determining whether this
  // window opening should be blocked, to ensure that we don't FireAbuseEvents
  // for a window opening that wouldn't have succeeded in the first place.
  if (!aUrl.IsEmpty()) {
    AppendUTF16toUTF8(aUrl, url);

    /* Check whether the URI is allowed, but not for dialogs --
       see bug 56851. The security of this function depends on
       window.openDialog being inaccessible from web scripts */
    if (url.get() && !aDialog)
      rv = SecurityCheckURL(url.get());
  }

  if (NS_FAILED(rv))
    return rv;

  PopupControlState abuseLevel = gPopupControlState;
  if (checkForPopup) {
    abuseLevel = RevisePopupAbuseLevel(abuseLevel);
    if (abuseLevel >= openAbused) {
      if (aJSCallerContext) {
        // If script in some other window is doing a window.open on us and
        // it's being blocked, then it's OK to close us afterwards, probably.
        // But if we're doing a window.open on ourselves and block the popup,
        // prevent this window from closing until after this script terminates
        // so that whatever popup blocker UI the app has will be visible.
        if (mContext == GetScriptContextFromJSContext(aJSCallerContext)) {
          mBlockScriptedClosingFlag = true;
          mContext->SetTerminationFunction(CloseBlockScriptTerminationFunc,
                                           this);
        }
      }

      FireAbuseEvents(true, false, aUrl, aName, aOptions);
      return aDoJSFixups ? NS_OK : NS_ERROR_FAILURE;
    }
  }    

  nsCOMPtr<nsIDOMWindow> domReturn;

  nsCOMPtr<nsIWindowWatcher> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(wwatch, rv);

  NS_ConvertUTF16toUTF8 options(aOptions);
  NS_ConvertUTF16toUTF8 name(aName);

  const char *options_ptr = aOptions.IsEmpty() ? nsnull : options.get();
  const char *name_ptr = aName.IsEmpty() ? nsnull : name.get();

  {
    // Reset popup state while opening a window to prevent the
    // current state from being active the whole time a modal
    // dialog is open.
    nsAutoPopupStatePusher popupStatePusher(openAbused, true);

    if (!aCalledNoScript) {
      nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
      NS_ASSERTION(pwwatch,
                   "Unable to open windows from JS because window watcher "
                   "is broken");
      NS_ENSURE_TRUE(pwwatch, NS_ERROR_UNEXPECTED);
        
      rv = pwwatch->OpenWindowJS(this, url.get(), name_ptr, options_ptr,
                                 aDialog, argv,
                                 getter_AddRefs(domReturn));
    } else {
      // Push a null JSContext here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindow::OpenWindow and nsIWindowWatcher::OpenWindow.
      nsCOMPtr<nsIJSContextStack> stack;

      if (!aContentModal) {
        stack = do_GetService(sJSStackContractID);
      }

      if (stack) {
        rv = stack->Push(nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
        
      rv = wwatch->OpenWindow(this, url.get(), name_ptr, options_ptr,
                              aExtraArgument, getter_AddRefs(domReturn));

      if (stack) {
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(!cx, "Unexpected JSContext popped!");
      }
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // success!

  NS_ENSURE_TRUE(domReturn, NS_OK);
  domReturn.swap(*aReturn);

  if (aDoJSFixups) {      
    nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*aReturn));
    if (!chrome_win) {
      // A new non-chrome window was created from a call to
      // window.open() from JavaScript, make sure there's a document in
      // the new window. We do this by simply asking the new window for
      // its document, this will synchronously create an empty document
      // if there is no document in the window.
      // XXXbz should this just use EnsureInnerWindow()?
#ifdef DEBUG_jst
      {
        nsCOMPtr<nsPIDOMWindow> pidomwin(do_QueryInterface(*aReturn));

        nsIDOMDocument *temp = pidomwin->GetExtantDocument();

        NS_ASSERTION(temp, "No document in new window!!!");
      }
#endif

      nsCOMPtr<nsIDOMDocument> doc;
      (*aReturn)->GetDocument(getter_AddRefs(doc));
    }
  }
    
  if (checkForPopup) {
    if (abuseLevel >= openControlled) {
      nsGlobalWindow *opened = static_cast<nsGlobalWindow *>(*aReturn);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(true);
        ++gOpenPopupSpamCount;
      }
    }
    if (abuseLevel >= openAbused)
      FireAbuseEvents(false, true, aUrl, aName, aOptions);
  }

  return rv;
}

// static
void
nsGlobalWindow::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

  nsGlobalWindow* globalWin =
    static_cast<nsGlobalWindow *>
               (static_cast<nsPIDOMWindow*>(win));

  // Need to post an event for closing, otherwise window and 
  // presshell etc. may get destroyed while creating frames, bug 338897.
  nsCloseEvent::PostCloseEvent(globalWin);
  // else if OOM, better not to close. That might cause a crash.
}

//*****************************************************************************
// nsGlobalWindow: Timeout Functions
//*****************************************************************************

PRUint32 sNestingLevel;

nsresult
nsGlobalWindow::SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                     PRInt32 interval,
                                     bool aIsInterval, PRInt32 *aReturn)
{
  FORWARD_TO_INNER(SetTimeoutOrInterval, (aHandler, interval, aIsInterval, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  // If we don't have a document (we could have been unloaded since
  // the call to setTimeout was made), do nothing.
  if (!mDocument) {
    return NS_OK;
  }

  // Disallow negative intervals.  If aIsInterval also disallow 0,
  // because we use that as a "don't repeat" flag.
  interval = NS_MAX(aIsInterval ? 1 : 0, interval);

  // Make sure we don't proceed with an interval larger than our timer
  // code can handle. (Note: we already forced |interval| to be non-negative,
  // so the PRUint32 cast (to avoid compiler warnings) is ok.)
  PRUint32 maxTimeoutMs = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  if (static_cast<PRUint32>(interval) > maxTimeoutMs) {
    interval = maxTimeoutMs;
  }

  nsRefPtr<nsTimeout> timeout = new nsTimeout();
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = interval;
  timeout->mScriptHandler = aHandler;

  // Now clamp the actual interval we will use for the timer based on
  PRUint32 nestingLevel = sNestingLevel + 1;
  PRInt32 realInterval = interval;
  if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL) {
    // Don't allow timeouts less than DOMMinTimeoutValue() from
    // now...
    realInterval = NS_MAX(realInterval, DOMMinTimeoutValue());
  }

  // Get principal of currently executing code, save for execution of timeout.
  // If our principals subsume the subject principal then use the subject
  // principal. Otherwise, use our principal to avoid running script in
  // elevated principals.

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv;
  rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  bool subsumes = false;
  nsCOMPtr<nsIPrincipal> ourPrincipal = GetPrincipal();

  // Note the direction of this test: We don't allow setTimeouts running with
  // chrome privileges on content windows, but we do allow setTimeouts running
  // with content privileges on chrome windows (where they can't do very much,
  // of course).
  rv = ourPrincipal->Subsumes(subjectPrincipal, &subsumes);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  if (subsumes) {
    timeout->mPrincipal = subjectPrincipal;
  } else {
    timeout->mPrincipal = ourPrincipal;
  }

  ++gTimeoutsRecentlySet;
  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);

  if (!IsFrozen() && !mTimeoutsSuspendDepth) {
    // If we're not currently frozen, then we set timeout->mWhen to be the
    // actual firing time of the timer (i.e., now + delta). We also actually
    // create a timer and fire it off.

    timeout->mWhen = TimeStamp::Now() + delta;

    timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsRefPtr<nsTimeout> copy = timeout;

    rv = timeout->mTimer->InitWithFuncCallback(TimerCallback, timeout,
                                               realInterval,
                                               nsITimer::TYPE_ONE_SHOT);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // The timeout is now also held in the timer's closure.
    copy.forget();
  } else {
    // If we are frozen, however, then we instead simply set
    // timeout->mTimeRemaining to be the "time remaining" in the timeout (i.e.,
    // the interval itself). We don't create a timer for it, since that will
    // happen when we are thawed and the timeout will then get a timer and run
    // to completion.

    timeout->mTimeRemaining = delta;
  }

  timeout->mWindow = this;

  if (!aIsInterval) {
    timeout->mNestingLevel = nestingLevel;
  }

  // No popups from timeouts by default
  timeout->mPopupState = openAbused;

  if (gRunningTimeoutDepth == 0 && gPopupControlState < openAbused) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    PRInt32 delay =
      Preferences::GetInt("dom.disable_open_click_delay");

    // This is checking |interval|, not realInterval, on purpose,
    // because our lower bound for |realInterval| could be pretty high
    // in some cases.
    if (interval <= delay) {
      timeout->mPopupState = gPopupControlState;
    }
  }

  InsertTimeoutIntoList(timeout);

  timeout->mPublicId = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->mPublicId;

  return NS_OK;

}

nsresult
nsGlobalWindow::SetTimeoutOrInterval(bool aIsInterval, PRInt32 *aReturn)
{
  // This needs to forward to the inner window, but since the current
  // inner may not be the inner in the calling scope, we need to treat
  // this specially here as we don't want timeouts registered in a
  // dying inner window to get registered and run on the current inner
  // window. To get this right, we need to forward this call to the
  // inner window that's calling window.setTimeout().

  if (IsOuterWindow()) {
    nsGlobalWindow* callerInner = CallerInnerWindow();
    NS_ENSURE_TRUE(callerInner, NS_ERROR_NOT_AVAILABLE);

    // If the caller and the callee share the same outer window,
    // forward to the callee inner. Else, we forward to the current
    // inner (e.g. someone is calling setTimeout() on a reference to
    // some other window).

    if (callerInner->GetOuterWindow() == this &&
        callerInner->IsInnerWindow()) {
      return callerInner->SetTimeoutOrInterval(aIsInterval, aReturn);
    }

    FORWARD_TO_INNER(SetTimeoutOrInterval, (aIsInterval, aReturn),
                     NS_ERROR_NOT_INITIALIZED);
  }

  PRInt32 interval = 0;
  bool isInterval = aIsInterval;
  nsCOMPtr<nsIScriptTimeoutHandler> handler;
  nsresult rv = NS_CreateJSTimeoutHandler(this,
                                          &isInterval,
                                          &interval,
                                          getter_AddRefs(handler));
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_DOM_TYPE_ERR) ? NS_OK : rv;

  return SetTimeoutOrInterval(handler, interval, isInterval, aReturn);
}

// static
void
nsGlobalWindow::RunTimeout(nsTimeout *aTimeout)
{
  // If a modal dialog is open for this window, return early. Pending
  // timeouts will run when the modal dialog is dismissed.
  if (IsInModalState() || mTimeoutsSuspendDepth) {
    return;
  }

  NS_TIME_FUNCTION;

  NS_ASSERTION(IsInnerWindow(), "Timeout running on outer window!");
  NS_ASSERTION(!IsFrozen(), "Timeout running on a window in the bfcache!");

  nsTimeout *nextTimeout, *timeout;
  nsTimeout *last_expired_timeout, *last_insertion_point;
  nsTimeout dummy_timeout;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);

  // A native timer has gone off. See which of our timeouts need
  // servicing
  TimeStamp now = TimeStamp::Now();
  TimeStamp deadline;

  if (aTimeout && aTimeout->mWhen > now) {
    // The OS timer fired early (yikes!), and possibly out of order
    // too. Set |deadline| to be the time when the OS timer *should*
    // have fired so that any timers that *should* have fired before
    // aTimeout *will* be fired now. This happens most of the time on
    // Win2k.

    deadline = aTimeout->mWhen;
  } else {
    deadline = now;
  }

  // The timeout list is kept in deadline order. Discover the latest
  // timeout whose deadline has expired. On some platforms, native
  // timeout events fire "early", so we need to test the timer as well
  // as the deadline.
  last_expired_timeout = nsnull;
  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = timeout->Next()) {
    if (((timeout == aTimeout) || (timeout->mWhen <= deadline)) &&
        (timeout->mFiringDepth == 0)) {
      // Mark any timeouts that are on the list to be fired with the
      // firing depth so that we can reentrantly run timeouts
      timeout->mFiringDepth = firingDepth;
      last_expired_timeout = timeout;
    }
  }

  // Maybe the timeout that the event was fired for has been deleted
  // and there are no others timeouts with deadlines that make them
  // eligible for execution yet. Go away.
  if (!last_expired_timeout) {
    return;
  }

  // Record telemetry information about timers set recently.
  TimeDuration recordingInterval = TimeDuration::FromMilliseconds(STATISTICS_INTERVAL);
  if (gLastRecordedRecentTimeouts.IsNull() ||
      now - gLastRecordedRecentTimeouts > recordingInterval) {
    PRUint32 count = gTimeoutsRecentlySet;
    gTimeoutsRecentlySet = 0;
    Telemetry::Accumulate(Telemetry::DOM_TIMERS_RECENTLY_SET, count);
    gLastRecordedRecentTimeouts = now;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  dummy_timeout.mFiringDepth = firingDepth;
  dummy_timeout.mWhen = now;
  PR_INSERT_AFTER(&dummy_timeout, last_expired_timeout);

  // Don't let ClearWindowTimeouts throw away our stack-allocated
  // dummy timeout.
  dummy_timeout.AddRef();
  dummy_timeout.AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  // If we ever start setting mTimeoutInsertionPoint to a non-dummy timeout,
  // the logic in ResetTimersForNonBackgroundWindow will need to change.
  mTimeoutInsertionPoint = &dummy_timeout;

  Telemetry::AutoCounter<Telemetry::DOM_TIMERS_FIRED_PER_NATIVE_TIMEOUT> timeoutsRan;

  for (timeout = FirstTimeout();
       timeout != &dummy_timeout && !IsFrozen();
       timeout = nextTimeout) {
    nextTimeout = timeout->Next();

    if (timeout->mFiringDepth != firingDepth) {
      // We skip the timeout since it's on the list to run at another
      // depth.

      continue;
    }

    if (mTimeoutsSuspendDepth) {
      // Some timer did suspend us. Make sure the
      // rest of the timers get executed later.
      timeout->mFiringDepth = 0;
      continue;
    }

    // The timeout is on the list to run at this depth, go ahead and
    // process it.

    // Get the script context (a strong ref to prevent it going away)
    // for this timeout and ensure the script language is enabled.
    nsCOMPtr<nsIScriptContext> scx = GetContextInternal();

    if (!scx) {
      // No context means this window was closed or never properly
      // initialized for this language.
      continue;
    }

    // The "scripts disabled" concept is still a little vague wrt
    // multiple languages.  Prepare for the day when languages can be
    // disabled independently of the other languages...
    if (!scx->GetScriptsEnabled()) {
      // Scripts were enabled once in this window (unless aTimeout ==
      // nsnull) but now scripts are disabled (we might be in
      // print-preview, for instance), this means we shouldn't run any
      // timeouts at this point.
      //
      // If scripts are enabled for this language in this window again
      // we'll fire the timeouts that are due at that point.
      continue;
    }

    // This timeout is good to run
    nsTimeout *last_running_timeout = mRunningTimeout;
    mRunningTimeout = timeout;
    timeout->mRunning = true;
    ++timeoutsRan;

    // Push this timeout's popup control state, which should only be
    // eabled the first time a timeout fires that was created while
    // popups were enabled and with a delay less than
    // "dom.disable_open_click_delay".
    nsAutoPopupStatePusher popupStatePusher(timeout->mPopupState);

    // Clear the timeout's popup state, if any, to prevent interval
    // timeouts from repeatedly opening poups.
    timeout->mPopupState = openAbused;

    // Hold on to the timeout in case mExpr or mFunObj releases its
    // doc.
    timeout->AddRef();

    ++gRunningTimeoutDepth;
    ++mTimeoutFiringDepth;

    bool trackNestingLevel = !timeout->mIsInterval;
    PRUint32 nestingLevel;
    if (trackNestingLevel) {
      nestingLevel = sNestingLevel;
      sNestingLevel = timeout->mNestingLevel;
    }

    nsCOMPtr<nsIScriptTimeoutHandler> handler(timeout->mScriptHandler);
    JSObject* scriptObject = handler->GetScriptObject();
    if (!scriptObject) {
      // Evaluate the timeout expression.
      const PRUnichar *script = handler->GetHandlerText();
      NS_ASSERTION(script, "timeout has no script nor handler text!");

      const char *filename = nsnull;
      PRUint32 lineNo = 0;
      handler->GetLocation(&filename, &lineNo);

      NS_TIME_FUNCTION_MARK("(file: %s, line: %d)", filename, lineNo);

      bool is_undefined;
      scx->EvaluateString(nsDependentString(script), FastGetGlobalJSObject(),
                          timeout->mPrincipal, timeout->mPrincipal,
                          filename, lineNo, JSVERSION_DEFAULT, nsnull,
                          &is_undefined);
    } else {
      nsCOMPtr<nsIVariant> dummy;
      nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow *>(this));
      scx->CallEventHandler(me, FastGetGlobalJSObject(),
                            scriptObject, handler->GetArgv(),
                            // XXXmarkh - consider allowing CallEventHandler to
                            // accept nsnull?
                            getter_AddRefs(dummy));

    }
    handler = nsnull; // drop reference before dropping timeout refs.

    if (trackNestingLevel) {
      sNestingLevel = nestingLevel;
    }

    --mTimeoutFiringDepth;
    --gRunningTimeoutDepth;

    mRunningTimeout = last_running_timeout;
    timeout->mRunning = false;

    // We ignore any failures from calling EvaluateString() or
    // CallEventHandler() on the context here since we're in a loop
    // where we're likely to be running timeouts whose OS timers
    // didn't fire in time and we don't want to not fire those timers
    // now just because execution of one timer failed. We can't
    // propagate the error to anyone who cares about it from this
    // point anyway, and the script context should have already reported
    // the script error in the usual way - so we just drop it.

    // If all timeouts were cleared and |timeout != aTimeout| then
    // |timeout| may be the last reference to the timeout so check if
    // it was cleared before releasing it.
    bool timeout_was_cleared = timeout->mCleared;

    timeout->Release();

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.

      mTimeoutInsertionPoint = last_insertion_point;

      return;
    }

    bool isInterval = false;

    // If we have a regular interval timer, we re-schedule the
    // timeout, accounting for clock drift.
    if (timeout->mIsInterval) {
      // Compute time to next timeout for interval timer.
      // Make sure nextInterval is at least DOMMinTimeoutValue().
      TimeDuration nextInterval =
        TimeDuration::FromMilliseconds(NS_MAX(timeout->mInterval,
                                              PRUint32(DOMMinTimeoutValue())));

      // If we're running pending timeouts because they've been temporarily
      // disabled (!aTimeout), set the next interval to be relative to "now",
      // and not to when the timeout that was pending should have fired.
      TimeStamp firingTime;
      if (!aTimeout)
        firingTime = now + nextInterval;
      else
        firingTime = timeout->mWhen + nextInterval;

      TimeStamp currentNow = TimeStamp::Now();
      TimeDuration delay = firingTime - currentNow;

      // And make sure delay is nonnegative; that might happen if the timer
      // thread is firing our timers somewhat early or if they're taking a long
      // time to run the callback.
      if (delay < TimeDuration(0)) {
        delay = TimeDuration(0);
      }

      if (timeout->mTimer) {
        timeout->mWhen = currentNow + delay; // firingTime unless delay got
                                             // clamped, in which case it's
                                             // currentNow.

        // Reschedule the OS timer. Don't bother returning any error
        // codes if this fails since the callers of this method
        // doesn't care about them nobody who cares about them
        // anyways.

        // Make sure to cast the unsigned PR_USEC_PER_MSEC to signed
        // PRTime to make the division do the right thing on 64-bit
        // platforms whether delay is positive or negative (which we
        // know is always positive here, but cast anyways for
        // consistency).
        nsresult rv = timeout->mTimer->
          InitWithFuncCallback(TimerCallback, timeout,
                               delay.ToMilliseconds(),
                               nsITimer::TYPE_ONE_SHOT);

        if (NS_FAILED(rv)) {
          NS_ERROR("Error initializing timer for DOM timeout!");

          // We failed to initialize the new OS timer, this timer does
          // us no good here so we just cancel it (just in case) and
          // null out the pointer to the OS timer, this will release the
          // OS timer. As we continue executing the code below we'll end
          // up deleting the timeout since it's not an interval timeout
          // any more (since timeout->mTimer == nsnull).
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;

          // Now that the OS timer no longer has a reference to the
          // timeout we need to drop that reference.
          timeout->Release();
        }
      } else {
        NS_ASSERTION(IsFrozen() || mTimeoutsSuspendDepth,
                     "How'd our timer end up null if we're not frozen or "
                     "suspended?");

        timeout->mTimeRemaining = delay;
        isInterval = true;
      }
    }

    if (timeout->mTimer) {
      if (timeout->mIsInterval) {
        isInterval = true;
      } else {
        // The timeout still has an OS timer, and it's not an
        // interval, that means that the OS timer could still fire (if
        // it didn't already, i.e. aTimeout == timeout), cancel the OS
        // timer and release its reference to the timeout.
        timeout->mTimer->Cancel();
        timeout->mTimer = nsnull;

        timeout->Release();
      }
    }

    // Running a timeout can cause another timeout to be deleted, so
    // we need to reset the pointer to the following timeout.
    nextTimeout = timeout->Next();

    PR_REMOVE_LINK(timeout);

    if (isInterval) {
      // Reschedule an interval timeout. Insert interval timeout
      // onto list sorted in deadline order.
      // AddRefs timeout.
      InsertTimeoutIntoList(timeout);
    }

    // Release the timeout struct since it's possibly out of the list
    timeout->Release();
  }

  // Take the dummy timeout off the head of the list
  PR_REMOVE_LINK(&dummy_timeout);

  mTimeoutInsertionPoint = last_insertion_point;
}

nsrefcnt
nsTimeout::Release()
{
  if (--mRefCnt > 0)
    return mRefCnt;

  // language specific cleanup done as mScriptHandler destructs...

  // Kill the timer if it is still alive.
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  delete this;
  return 0;
}

nsrefcnt
nsTimeout::AddRef()
{
  return ++mRefCnt;
}


nsresult
nsGlobalWindow::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (aTimerID), NS_ERROR_NOT_INITIALIZED);

  PRUint32 public_id = (PRUint32)aTimerID;
  nsTimeout *timeout;

  for (timeout = FirstTimeout();
       IsTimeout(timeout);
       timeout = timeout->Next()) {
    if (timeout->mPublicId == public_id) {
      if (timeout->mRunning) {
        /* We're running from inside the timeout. Mark this
           timeout for deferred deletion by the code in
           RunTimeout() */
        timeout->mIsInterval = false;
      }
      else {
        /* Delete the timeout from the pending timeout list */
        PR_REMOVE_LINK(timeout);

        if (timeout->mTimer) {
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;
          timeout->Release();
        }
        timeout->Release();
      }
      break;
    }
  }

  return NS_OK;
}

nsresult nsGlobalWindow::ResetTimersForNonBackgroundWindow()
{
  FORWARD_TO_INNER(ResetTimersForNonBackgroundWindow, (),
                   NS_ERROR_NOT_INITIALIZED);

  if (IsFrozen() || mTimeoutsSuspendDepth) {
    return NS_OK;
  }

  TimeStamp now = TimeStamp::Now();

  // If mTimeoutInsertionPoint is non-null, we're in the middle of firing
  // timers and the timers we're planning to fire all come before
  // mTimeoutInsertionPoint; mTimeoutInsertionPoint itself is a dummy timeout
  // with an mWhen that may be semi-bogus.  In that case, we don't need to do
  // anything with mTimeoutInsertionPoint or anything before it, so should
  // start at the timer after mTimeoutInsertionPoint, if there is one.
  // Otherwise, start at the beginning of the list.
  for (nsTimeout *timeout = mTimeoutInsertionPoint ?
         mTimeoutInsertionPoint->Next() : FirstTimeout();
       IsTimeout(timeout); ) {
    // It's important that this check be <= so that we guarantee that
    // taking NS_MAX with |now| won't make a quantity equal to
    // timeout->mWhen below.
    if (timeout->mWhen <= now) {
      timeout = timeout->Next();
      continue;
    }

    if (timeout->mWhen - now >
        TimeDuration::FromMilliseconds(gMinBackgroundTimeoutValue)) {
      // No need to loop further.  Timeouts are sorted in mWhen order
      // and the ones after this point were all set up for at least
      // gMinBackgroundTimeoutValue ms and hence were not clamped.
      break;
    }

    /* We switched from background. Re-init the timer appropriately */
    // Compute the interval the timer should have had if it had not been set in a
    // background window
    TimeDuration interval =
      TimeDuration::FromMilliseconds(NS_MAX(timeout->mInterval,
                                            PRUint32(DOMMinTimeoutValue())));
    PRUint32 oldIntervalMillisecs = 0;
    timeout->mTimer->GetDelay(&oldIntervalMillisecs);
    TimeDuration oldInterval = TimeDuration::FromMilliseconds(oldIntervalMillisecs);
    if (oldInterval > interval) {
      // unclamp
      TimeStamp firingTime =
        NS_MAX(timeout->mWhen - oldInterval + interval, now);

      NS_ASSERTION(firingTime < timeout->mWhen,
                   "Our firing time should strictly decrease!");

      TimeDuration delay = firingTime - now;
      timeout->mWhen = firingTime;

      // Since we reset mWhen we need to move |timeout| to the right
      // place in the list so that it remains sorted by mWhen.
      
      // Get the pointer to the next timeout now, before we move the
      // current timeout in the list.
      nsTimeout* nextTimeout = timeout->Next();

      // It is safe to remove and re-insert because mWhen is now
      // strictly smaller than it used to be, so we know we'll insert
      // |timeout| before nextTimeout.
      NS_ASSERTION(!IsTimeout(nextTimeout) ||
                   timeout->mWhen < nextTimeout->mWhen, "How did that happen?");
      PR_REMOVE_LINK(timeout);
      // InsertTimeoutIntoList will addref |timeout| and reset
      // mFiringDepth.  Make sure to undo that after calling it.
      PRUint32 firingDepth = timeout->mFiringDepth;
      InsertTimeoutIntoList(timeout);
      timeout->mFiringDepth = firingDepth;
      timeout->Release();

      nsresult rv =
        timeout->mTimer->InitWithFuncCallback(TimerCallback,
                                              timeout,
                                              delay.ToMilliseconds(),
                                              nsITimer::TYPE_ONE_SHOT);

      if (NS_FAILED(rv)) {
        NS_WARNING("Error resetting non background timer for DOM timeout!");
        return rv;
      }

      timeout = nextTimeout;
    } else {
      timeout = timeout->Next();
    }
  }

  return NS_OK;
}

void
nsGlobalWindow::ClearAllTimeouts()
{
  nsTimeout *timeout, *nextTimeout;

  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = nextTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nsnull;

    nextTimeout = timeout->Next();

    if (timeout->mTimer) {
      timeout->mTimer->Cancel();
      timeout->mTimer = nsnull;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      timeout->Release();
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    timeout->mCleared = true;

    // Drop the count since we're removing it from the list.
    timeout->Release();
  }

  // Clear out our list
  PR_INIT_CLIST(&mTimeouts);
}

void
nsGlobalWindow::InsertTimeoutIntoList(nsTimeout *aTimeout)
{
  NS_ASSERTION(IsInnerWindow(),
               "InsertTimeoutIntoList() called on outer window!");

  // Start at mLastTimeout and go backwards.  Don't go further than
  // mTimeoutInsertionPoint, though.  This optimizes for the common case of
  // insertion at the end.
  nsTimeout* prevSibling;
  for (prevSibling = LastTimeout();
       IsTimeout(prevSibling) && prevSibling != mTimeoutInsertionPoint &&
         // This condition needs to match the one in SetTimeoutOrInterval that
         // determines whether to set mWhen or mTimeRemaining.
         ((IsFrozen() || mTimeoutsSuspendDepth) ?
          prevSibling->mTimeRemaining > aTimeout->mTimeRemaining :
          prevSibling->mWhen > aTimeout->mWhen);
       prevSibling = prevSibling->Prev()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  PR_INSERT_AFTER(aTimeout, prevSibling);

  aTimeout->mFiringDepth = 0;

  // Increment the timeout's reference count since it's now held on to
  // by the list
  aTimeout->AddRef();
}

// static
void
nsGlobalWindow::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsRefPtr<nsTimeout> timeout = (nsTimeout *)aClosure;

  timeout->mWindow->RunTimeout(timeout);
}

//*****************************************************************************
// nsGlobalWindow: Helper Functions
//*****************************************************************************

nsresult
nsGlobalWindow::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!docShellAsItem) {
    *aTreeOwner = nsnull;

    return NS_OK;
  }

  return docShellAsItem->GetTreeOwner(aTreeOwner);
}

nsresult
nsGlobalWindow::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (docShellAsItem) {
    docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  if (!treeOwner) {
    *aTreeOwner = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(treeOwner, aTreeOwner);
}

nsresult
nsGlobalWindow::GetWebBrowserChrome(nsIWebBrowserChrome **aBrowserChrome)
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  NS_IF_ADDREF(*aBrowserChrome = browserChrome);

  return NS_OK;
}

nsIScrollableFrame *
nsGlobalWindow::GetScrollFrame()
{
  FORWARD_TO_OUTER(GetScrollFrame, (), nsnull);

  if (!mDocShell) {
    return nsnull;
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    return presShell->GetRootScrollFrameAsScrollable();
  }
  return nsnull;
}

nsresult
nsGlobalWindow::BuildURIfromBase(const char *aURL, nsIURI **aBuiltURI,
                                 bool *aFreeSecurityPass,
                                 JSContext **aCXused)
{
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = nsnull;

  *aBuiltURI = nsnull;
  *aFreeSecurityPass = false;
  if (aCXused)
    *aCXused = nsnull;

  // get JSContext
  NS_ASSERTION(scx, "opening window missing its context");
  NS_ASSERTION(mDocument, "opening window missing its document");
  if (!scx || !mDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMChromeWindow> chrome_win = do_QueryObject(this);

  if (nsContentUtils::IsCallerChrome() && !chrome_win) {
    // If open() is called from chrome on a non-chrome window, we'll
    // use the context from the window on which open() is being called
    // to prevent giving chrome priveleges to new windows opened in
    // such a way. This also makes us get the appropriate base URI for
    // the below URI resolution code.

    cx = scx->GetNativeContext();
  } else {
    // get the JSContext from the call stack
    nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
    if (stack)
      stack->Peek(&cx);
  }

  /* resolve the URI, which could be relative to the calling window
     (note the algorithm to get the base URI should match the one
     used to actually kick off the load in nsWindowWatcher.cpp). */
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  nsIURI* baseURI = nsnull;
  nsCOMPtr<nsIURI> uriToLoad;
  nsCOMPtr<nsIDOMWindow> sourceWindow;

  if (cx) {
    nsIScriptContext *scriptcx = nsJSUtils::GetDynamicScriptContext(cx);
    if (scriptcx)
      sourceWindow = do_QueryInterface(scriptcx->GetGlobalObject());
  }

  if (!sourceWindow) {
    sourceWindow = do_QueryInterface(NS_ISUPPORTS_CAST(nsIDOMWindow *, this));
    *aFreeSecurityPass = true;
  }

  if (sourceWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    sourceWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc) {
      baseURI = doc->GetDocBaseURI();
      charset = doc->GetDocumentCharacterSet();
    }
  }

  if (aCXused)
    *aCXused = cx;
  return NS_NewURI(aBuiltURI, nsDependentCString(aURL), charset.get(), baseURI);
}

nsresult
nsGlobalWindow::SecurityCheckURL(const char *aURL)
{
  JSContext       *cx;
  bool             freePass;
  nsCOMPtr<nsIURI> uri;

  if (NS_FAILED(BuildURIfromBase(aURL, getter_AddRefs(uri), &freePass, &cx)))
    return NS_ERROR_FAILURE;

  if (!freePass && NS_FAILED(nsContentUtils::GetSecurityManager()->
        CheckLoadURIFromScript(cx, uri)))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void
nsGlobalWindow::FlushPendingNotifications(mozFlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindow::EnsureSizeUpToDate()
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindow *parent =
    static_cast<nsGlobalWindow *>(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }
}

nsresult
nsGlobalWindow::SaveWindowState(nsISupports **aState)
{
  NS_PRECONDITION(IsOuterWindow(), "Can't save the inner window's state");

  *aState = nsnull;

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother saving state.
    return NS_OK;
  }

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();
  NS_ASSERTION(inner, "No inner window to save");

  // Don't do anything else to this inner window! After this point, all
  // calls to SetTimeoutOrInterval will create entries in the timeout
  // list that will only run after this window has come out of the bfcache.
  // Also, while we're frozen, we won't dispatch online/offline events
  // to the page.
  inner->Freeze();

  // Remember the outer window's prototype.
  JSContext *cx = mContext->GetNativeContext();
  JSAutoRequest req(cx);

  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIClassInfo> ci =
    do_QueryInterface((nsIScriptGlobalObject *)this);
  nsCOMPtr<nsIXPConnectJSObjectHolder> proto;
  nsresult rv = xpc->GetWrappedNativePrototype(cx, mJSObject, ci,
                                               getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *realProto = JS_GetPrototype(mJSObject);
  nsCOMPtr<nsIXPConnectJSObjectHolder> realProtoHolder;
  if (realProto) {
    rv = xpc->HoldObject(cx, realProto, getter_AddRefs(realProtoHolder));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner,
                                                      mInnerWindowHolder,
                                                      proto,
                                                      realProtoHolder);
  NS_ENSURE_TRUE(state, NS_ERROR_OUT_OF_MEMORY);

  JSObject *wnProto;
  proto->GetJSObject(&wnProto);
  if (!JS_SetPrototype(cx, mJSObject, wnProto)) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  state.swap(*aState);
  return NS_OK;
}

nsresult
nsGlobalWindow::RestoreWindowState(nsISupports *aState)
{
  NS_ASSERTION(IsOuterWindow(), "Cannot restore an inner window");

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother restoring state.
    return NS_OK;
  }

  nsCOMPtr<WindowStateHolder> holder = do_QueryInterface(aState);
  NS_ENSURE_TRUE(holder, NS_ERROR_FAILURE);

#ifdef DEBUG_PAGE_CACHE
  printf("restoring window state, state = %p\n", (void*)holder);
#endif

  // And we're ready to go!
  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  // if a link is focused, refocus with the FLAG_SHOWRING flag set. This makes
  // it easy to tell which link was last clicked when going back a page.
  nsIContent* focusedNode = inner->GetFocusedNode();
  if (IsLink(focusedNode)) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> focusedElement(do_QueryInterface(focusedNode));
      fm->SetFocus(focusedElement, nsIFocusManager::FLAG_NOSCROLL |
                                   nsIFocusManager::FLAG_SHOWRING);
    }
  }

  inner->Thaw();

  holder->DidRestoreWindow();

  return NS_OK;
}

void
nsGlobalWindow::SuspendTimeouts(PRUint32 aIncrease,
                                bool aFreezeChildren)
{
  FORWARD_TO_INNER_VOID(SuspendTimeouts, (aIncrease, aFreezeChildren));

  bool suspended = (mTimeoutsSuspendDepth != 0);
  mTimeoutsSuspendDepth += aIncrease;

  if (!suspended) {
    nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
    if (ac) {
      for (PRUint32 i = 0; i < mEnabledSensors.Length(); i++)
        ac->RemoveWindowListener(mEnabledSensors[i], this);
    }

    // Suspend all of the workers for this window.
    nsIScriptContext *scx = GetContextInternal();
    JSContext *cx = scx ? scx->GetNativeContext() : nsnull;
    mozilla::dom::workers::SuspendWorkersForWindow(cx, this);

    TimeStamp now = TimeStamp::Now();
    for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
      // Set mTimeRemaining to be the time remaining for this timer.
      if (t->mWhen > now)
        t->mTimeRemaining = t->mWhen - now;
      else
        t->mTimeRemaining = TimeDuration(0);
  
      // Drop the XPCOM timer; we'll reschedule when restoring the state.
      if (t->mTimer) {
        t->mTimer->Cancel();
        t->mTimer = nsnull;
  
        // Drop the reference that the timer's closure had on this timeout, we'll
        // add it back in ResumeTimeouts. Note that it shouldn't matter that we're
        // passing null for the context, since this shouldn't actually release this
        // timeout.
        t->Release();
      }
    }
  }

  // Suspend our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node(do_QueryInterface(GetDocShell()));
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));
        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

        // This is a bit hackish. Only freeze/suspend windows which are truly our
        // subwindows.
        nsCOMPtr<nsIContent> frame = do_QueryInterface(pWin->GetFrameElementInternal());
        if (!mDoc || !frame || mDoc != frame->OwnerDoc() || !inner) {
          continue;
        }

        win->SuspendTimeouts(aIncrease, aFreezeChildren);

        if (inner && aFreezeChildren) {
          inner->Freeze();
        }
      }
    }
  }
}

nsresult
nsGlobalWindow::ResumeTimeouts(bool aThawChildren)
{
  FORWARD_TO_INNER(ResumeTimeouts, (), NS_ERROR_NOT_INITIALIZED);

  NS_ASSERTION(mTimeoutsSuspendDepth, "Mismatched calls to ResumeTimeouts!");
  --mTimeoutsSuspendDepth;
  bool shouldResume = (mTimeoutsSuspendDepth == 0);
  nsresult rv;

  if (shouldResume) {
    nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
    if (ac) {
      for (PRUint32 i = 0; i < mEnabledSensors.Length(); i++)
        ac->AddWindowListener(mEnabledSensors[i], this);
    }

    // Resume all of the workers for this window.
    nsIScriptContext *scx = GetContextInternal();
    JSContext *cx = scx ? scx->GetNativeContext() : nsnull;
    mozilla::dom::workers::ResumeWorkersForWindow(cx, this);

    // Restore all of the timeouts, using the stored time remaining
    // (stored in timeout->mTimeRemaining).

    TimeStamp now = TimeStamp::Now();

#ifdef DEBUG
    bool _seenDummyTimeout = false;
#endif

    for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
      // There's a chance we're being called with RunTimeout on the stack in which
      // case we have a dummy timeout in the list that *must not* be resumed. It
      // can be identified by a null mWindow.
      if (!t->mWindow) {
#ifdef DEBUG
        NS_ASSERTION(!_seenDummyTimeout, "More than one dummy timeout?!");
        _seenDummyTimeout = true;
#endif
        continue;
      }

      // XXXbz the combination of the way |delay| and |t->mWhen| are set here
      // makes no sense.  Are we trying to impose that min timeout value or
      // not???
      PRUint32 delay =
        NS_MAX(PRInt32(t->mTimeRemaining.ToMilliseconds()),
               DOMMinTimeoutValue());

      // Set mWhen back to the time when the timer is supposed to
      // fire.
      t->mWhen = now + t->mTimeRemaining;

      t->mTimer = do_CreateInstance("@mozilla.org/timer;1");
      NS_ENSURE_TRUE(t->mTimer, NS_ERROR_OUT_OF_MEMORY);

      rv = t->mTimer->InitWithFuncCallback(TimerCallback, t, delay,
                                           nsITimer::TYPE_ONE_SHOT);
      if (NS_FAILED(rv)) {
        t->mTimer = nsnull;
        return rv;
      }

      // Add a reference for the new timer's closure.
      t->AddRef();
    }
  }

  // Resume our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          static_cast<nsGlobalWindow*>
                     (static_cast<nsPIDOMWindow*>(pWin));

        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

        // This is a bit hackish. Only thaw/resume windows which are truly our
        // subwindows.
        nsCOMPtr<nsIContent> frame = do_QueryInterface(pWin->GetFrameElementInternal());
        if (!mDoc || !frame || mDoc != frame->OwnerDoc() || !inner) {
          continue;
        }

        if (inner && aThawChildren) {
          inner->Thaw();
        }

        rv = win->ResumeTimeouts(aThawChildren);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

PRUint32
nsGlobalWindow::TimeoutSuspendCount()
{
  FORWARD_TO_INNER(TimeoutSuspendCount, (), 0);
  return mTimeoutsSuspendDepth;
}

void
nsGlobalWindow::EnableDeviceSensor(PRUint32 aType)
{
  bool alreadyEnabled = false;
  for (PRUint32 i = 0; i < mEnabledSensors.Length(); i++) {
    if (mEnabledSensors[i] == aType) {
      alreadyEnabled = true;
      break;
    }
  }

  if (alreadyEnabled)
    return;

  mEnabledSensors.AppendElement(aType);

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->AddWindowListener(aType, this);
}

void
nsGlobalWindow::DisableDeviceSensor(PRUint32 aType)
{
  PRInt32 doomedElement = -1;
  for (PRUint32 i = 0; i < mEnabledSensors.Length(); i++) {
    if (mEnabledSensors[i] == aType) {
      doomedElement = i;
      break;
    }
  }

  if (doomedElement == -1)
    return;

  mEnabledSensors.RemoveElementAt(doomedElement);

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->RemoveWindowListener(aType, this);
}

NS_IMETHODIMP
nsGlobalWindow::GetURL(nsIDOMMozURLProperty** aURL)
{
  FORWARD_TO_INNER(GetURL, (aURL), NS_ERROR_UNEXPECTED);

  if (!mURLProperty) {
    mURLProperty = new nsDOMMozURLProperty(this);
  }

  NS_ADDREF(*aURL = mURLProperty);

  return NS_OK;
}

// static
bool
nsGlobalWindow::HasIndexedDBSupport()
{
  return Preferences::GetBool("indexedDB.feature.enabled", true);
}

// static
bool
nsGlobalWindow::HasPerformanceSupport() 
{
  return Preferences::GetBool("dom.enable_performance", false);
}

void
nsGlobalWindow::SizeOfIncludingThis(nsWindowSizes* aWindowSizes) const
{
  aWindowSizes->mDOM += aWindowSizes->mMallocSizeOf(this);

  if (IsInnerWindow()) {
    nsEventListenerManager* elm =
      const_cast<nsGlobalWindow*>(this)->GetListenerManager(false);
    if (elm) {
      aWindowSizes->mDOM +=
        elm->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf);
    }
    if (mDoc) {
      mDoc->DocSizeOfIncludingThis(aWindowSizes);
    }
  }

  aWindowSizes->mDOM +=
    mNavigator ?
      mNavigator->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf) : 0;
}

void
nsGlobalWindow::SetIsApp(bool aValue)
{
  FORWARD_TO_OUTER_VOID(SetIsApp, (aValue));

  // You shouldn't call SetIsApp() more than once.
  MOZ_ASSERT(mIsApp == TriState_Unknown);

  mIsApp = aValue ? TriState_True : TriState_False;
}

bool
nsGlobalWindow::IsPartOfApp()
{
  FORWARD_TO_OUTER(IsPartOfApp, (), TriState_False);

  // We go trough all window parents until we find one with |mIsApp| set to
  // something. If none is found, we are not part of an application.
  for (nsGlobalWindow* w = this; w;
       w = static_cast<nsGlobalWindow*>(w->GetParentInternal())) {
    if (w->mIsApp == TriState_True) {
      // The window should be part of an application.
      MOZ_ASSERT(w->mApp);
      return true;
    } else if (w->mIsApp == TriState_False) {
      return false;
    }
  }

  return false;
}

nsresult
nsGlobalWindow::SetApp(const nsAString& aManifestURL)
{
  // SetIsApp(true) should be called before calling SetApp().
  if (mIsApp != TriState_True) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  if (!appsService) {
    NS_ERROR("Apps Service is not available!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIDOMApplication> app;
  appsService->GetAppByManifestURL(aManifestURL, getter_AddRefs(app));
  if (!app) {
    NS_WARNING("No application found with the specified manifest URL");
    return NS_ERROR_FAILURE;
  }

  mApp = app.forget();

  return NS_OK;
}

// nsGlobalChromeWindow implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalChromeWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBrowserDOMWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mBrowserDOMWindow)
  if (tmp->mMessageManager) {
    static_cast<nsFrameMessageManager*>(
      tmp->mMessageManager.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMessageManager)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_DATA(ChromeWindow, nsGlobalChromeWindow)

// QueryInterface implementation for nsGlobalChromeWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalChromeWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMChromeWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ChromeWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)

NS_IMETHODIMP
nsGlobalChromeWindow::GetWindowState(PRUint16* aWindowState)
{
  *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  PRInt32 aMode = 0;

  if (widget) {
    nsresult rv = widget->GetSizeMode(&aMode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (aMode) {
    case nsSizeMode_Minimized:
      *aWindowState = nsIDOMChromeWindow::STATE_MINIMIZED;
      break;
    case nsSizeMode_Maximized:
      *aWindowState = nsIDOMChromeWindow::STATE_MAXIMIZED;
      break;
    case nsSizeMode_Fullscreen:
      *aWindowState = nsIDOMChromeWindow::STATE_FULLSCREEN;
      break;
    case nsSizeMode_Normal:
      *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;
      break;
    default:
      NS_WARNING("Illegal window state for this chrome window");
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Maximize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Maximized);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget)
    rv = widget->SetSizeMode(nsSizeMode_Minimized);

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Restore()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Normal);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttention()
{
  return GetAttentionWithCycleCount(-1);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttentionWithCycleCount(PRInt32 aCycleCount)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->GetAttention(aCycleCount);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::BeginWindowMove(nsIDOMEvent *aMouseDownEvent, nsIDOMElement* aPanel)
{
  nsCOMPtr<nsIWidget> widget;

  // if a panel was supplied, use its widget instead.
#ifdef MOZ_XUL
  if (aPanel) {
    nsCOMPtr<nsIContent> panel = do_QueryInterface(aPanel);
    NS_ENSURE_TRUE(panel, NS_ERROR_FAILURE);

    nsIFrame* frame = panel->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame && frame->GetType() == nsGkAtoms::menuPopupFrame, NS_OK);

    widget = (static_cast<nsMenuPopupFrame*>(frame))->GetWidget();
  }
  else {
#endif
    widget = GetMainWidget();
#ifdef MOZ_XUL
  }
#endif

  if (!widget) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrivateDOMEvent> privEvent = do_QueryInterface(aMouseDownEvent);
  NS_ENSURE_TRUE(privEvent, NS_ERROR_FAILURE);
  nsEvent *internalEvent = privEvent->GetInternalNSEvent();
  NS_ENSURE_TRUE(internalEvent &&
                 internalEvent->eventStructType == NS_MOUSE_EVENT,
                 NS_ERROR_FAILURE);
  nsMouseEvent *mouseEvent = static_cast<nsMouseEvent*>(internalEvent);

  return widget->BeginMoveDrag(mouseEvent);
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP
nsGlobalChromeWindow::SetCursor(const nsAString& aCursor)
{
  FORWARD_TO_OUTER_CHROME(SetCursor, (aCursor), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  PRInt32 cursor;

  // use C strings to keep the code/data size down
  NS_ConvertUTF16toUTF8 cursorString(aCursor);

  if (cursorString.Equals("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aCursor);
    if (eCSSKeyword_UNKNOWN == keyword ||
        !nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
      // XXX remove the following three values (leave return NS_OK) after 1.8
      // XXX since they should have been -moz- prefixed (covered by FindKeyword).
      // XXX (also remove |cursorString| at that point?).
      if (cursorString.Equals("grab"))
        cursor = NS_STYLE_CURSOR_GRAB;
      else if (cursorString.Equals("grabbing"))
        cursor = NS_STYLE_CURSOR_GRABBING;
      else if (cursorString.Equals("spinning"))
        cursor = NS_STYLE_CURSOR_SPINNING;
      else
        return NS_OK;
    }
  }

  nsRefPtr<nsPresContext> presContext;
  if (mDocShell) {
    mDocShell->GetPresContext(getter_AddRefs(presContext));
  }

  if (presContext) {
    // Need root widget.
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsIViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    nsIView* rootView = vm->GetRootView();
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    nsIWidget* widget = rootView->GetNearestWidget(nsnull);
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    // Call esm and set cursor.
    rv = presContext->EventStateManager()->SetCursor(cursor, nsnull,
                                                     false, 0.0f, 0.0f,
                                                     widget, true);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(GetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aBrowserWindow);

  *aBrowserWindow = mBrowserDOMWindow;
  NS_IF_ADDREF(*aBrowserWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow *aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(SetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  mBrowserDOMWindow = aBrowserWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::NotifyDefaultButtonLoaded(nsIDOMElement* aDefaultButton)
{
#ifdef MOZ_XUL
  NS_ENSURE_ARG(aDefaultButton);

  // Don't snap to a disabled button.
  nsCOMPtr<nsIDOMXULControlElement> xulControl =
                                      do_QueryInterface(aDefaultButton);
  NS_ENSURE_TRUE(xulControl, NS_ERROR_FAILURE);
  bool disabled;
  nsresult rv = xulControl->GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (disabled)
    return NS_OK;

  // Get the button rect in screen coordinates.
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDefaultButton));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  nsIFrame *frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
  nsIntRect buttonRect = frame->GetScreenRect();

  // Get the widget rect in screen coordinates.
  nsIWidget *widget = GetNearestWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  nsIntRect widgetRect;
  rv = widget->GetScreenBounds(widgetRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the buttonRect coordinates from screen to the widget.
  buttonRect -= widgetRect.TopLeft();
  rv = widget->OnDefaultButtonLoaded(buttonRect);
  if (rv == NS_ERROR_NOT_IMPLEMENTED)
    return NS_OK;
  return rv;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetMessageManager(nsIChromeFrameMessageManager** aManager)
{
  FORWARD_TO_INNER_CHROME(GetMessageManager, (aManager), NS_ERROR_FAILURE);
  if (!mMessageManager) {
    nsIScriptContext* scx = GetContextInternal();
    NS_ENSURE_STATE(scx);
    JSContext* cx = scx->GetNativeContext();
    NS_ENSURE_STATE(cx);
    nsCOMPtr<nsIChromeFrameMessageManager> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    mMessageManager =
      new nsFrameMessageManager(true,
                                nsnull,
                                nsnull,
                                nsnull,
                                nsnull,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                cx);
    NS_ENSURE_TRUE(mMessageManager, NS_ERROR_OUT_OF_MEMORY);
  }
  CallQueryInterface(mMessageManager, aManager);
  return NS_OK;
}

// nsGlobalModalWindow implementation

// QueryInterface implementation for nsGlobalModalWindow
NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalModalWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalModalWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mReturnValue)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(ModalContentWindow, nsGlobalModalWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalModalWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMModalContentWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ModalContentWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalModalWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalModalWindow, nsGlobalWindow)


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGlobalModalWindow,
                                                nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mReturnValue)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_IMETHODIMP
nsGlobalModalWindow::GetDialogArguments(nsIArray **aArguments)
{
  FORWARD_TO_INNER_MODAL_CONTENT_WINDOW(GetDialogArguments, (aArguments),
                                        NS_ERROR_NOT_INITIALIZED);

  bool subsumes = false;
  nsIPrincipal *self = GetPrincipal();
  if (self && NS_SUCCEEDED(self->Subsumes(mArgumentsOrigin, &subsumes)) &&
      subsumes) {
    NS_IF_ADDREF(*aArguments = mArguments);
  } else {
    *aArguments = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalModalWindow::GetReturnValue(nsIVariant **aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetReturnValue, (aRetVal), NS_OK);

  NS_IF_ADDREF(*aRetVal = mReturnValue);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalModalWindow::SetReturnValue(nsIVariant *aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(SetReturnValue, (aRetVal), NS_OK);

  mReturnValue = aRetVal;

  return NS_OK;
}

nsresult
nsGlobalModalWindow::SetNewDocument(nsIDocument *aDocument,
                                    nsISupports *aState,
                                    bool aForceReuseInnerWindow)
{
  // If we're loading a new document into a modal dialog, clear the
  // return value that was set, if any, by the current document.
  if (aDocument) {
    mReturnValue = nsnull;
  }

  return nsGlobalWindow::SetNewDocument(aDocument, aState,
                                        aForceReuseInnerWindow);
}

void
nsGlobalWindow::SetHasAudioAvailableEventListeners()
{
  if (mDoc) {
    mDoc->NotifyAudioAvailableListener();
  }
}

#define EVENT(name_, id_, type_, struct_)                                    \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             jsval *vp) {                    \
    nsEventListenerManager *elm = GetListenerManager(false);              \
    if (elm) {                                                               \
      elm->GetJSEventListener(nsGkAtoms::on##name_, vp);                     \
    } else {                                                                 \
      *vp = JSVAL_NULL;                                                      \
    }                                                                        \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             const jsval &v) {               \
    nsEventListenerManager *elm = GetListenerManager(true);               \
    if (!elm) {                                                              \
      return NS_ERROR_OUT_OF_MEMORY;                                         \
    }                                                                        \
                                                                             \
    JSObject *obj = mJSObject;                                               \
    if (!obj) {                                                              \
      return NS_ERROR_UNEXPECTED;                                            \
    }                                                                        \
    return elm->SetJSEventListenerToJsval(nsGkAtoms::on##name_, cx, obj, v); \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "nsEventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef EVENT

