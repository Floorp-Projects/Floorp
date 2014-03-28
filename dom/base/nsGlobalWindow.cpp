/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindow.h"

#include <algorithm>

#include "mozilla/MemoryReporting.h"

// Local Includes
#include "Navigator.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsPerformance.h"
#include "nsDOMNavigationTiming.h"
#include "nsIDOMStorage.h"
#include "nsIDOMStorageManager.h"
#include "DOMStorage.h"
#include "nsDOMOfflineResourceList.h"
#include "nsError.h"
#include "nsIIdleService.h"
#include "nsISizeOfEventTarget.h"
#include "nsDOMJSUtils.h"
#include "nsArrayUtils.h"
#include "nsIDOMWindowCollection.h"
#include "nsDOMWindowList.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIPermissionManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIController.h"
#include "nsScriptNameSpaceManager.h"
#include "nsWindowMemoryReporter.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "js/OldDebugAPI.h"     // for JS_ClearWatchPointsForObject
#include "jswrapper.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSEnvironment.h"
#include "ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/unused.h"

// Other Classes
#include "mozilla/dom/BarProps.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/dom/MessagePortList.h"
#include "nsJSPrincipals.h"
#include "mozilla/Attributes.h"
#include "mozilla/Debug.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "AudioChannelService.h"
#include "MessageEvent.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsIBaseWindow.h"
#include "nsIDeviceSensors.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "Crypto.h"
#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "nsIDOMCryptoLegacy.h"
#endif
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMPopStateEvent.h"
#include "nsIDOMHashChangeEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsPIDOMStorage.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsThreadUtils.h"
#include "nsILoadContext.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIPrompt.h"
#include "nsIPromptService.h"
#include "nsIPromptFactory.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWindowMediator.h"  // For window.find()
#include "nsComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsIScriptError.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsCSSProps.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIURIFixup.h"
#ifndef DEBUG
#include "nsIAppStartup.h"
#include "nsToolkitCompsCID.h"
#endif
#include "nsCDefaultURIFixup.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"
#include "nsIXULWindow.h"
#include "nsITimedChannel.h"
#include "nsServiceManagerUtils.h"
#ifdef MOZ_XUL
#include "nsIDOMXULControlElement.h"
#include "nsMenuPopupFrame.h"
#endif
#include "nsIDOMCustomEvent.h"
#include "nsIFrameRequestCallback.h"
#include "nsIJARChannel.h"

#include "xpcprivate.h"

#ifdef NS_PRINTING
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsBindingManager.h"
#include "nsXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

#include "nsIDragService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Selection.h"
#include "nsFrameLoader.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"
#include "GeneratedEvents.h"
#include "GeneratedEventClasses.h"
#include "mozIThirdPartyUtil.h"
#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prenv.h"
#include "prprf.h"

#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/quota/QuotaManager.h"

#include "mozilla/dom/StructuredCloneTags.h"

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadService.h"
#endif

#include "nsRefreshDriver.h"

#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "nsLocation.h"
#include "nsHTMLDocument.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "prrng.h"
#include "nsSandboxFlags.h"
#include "TimeChangeObserver.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/BrowserElementDictionariesBinding.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsITabChild.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/ScriptSettings.h"
#ifdef HAVE_SIDEBAR
#include "mozilla/dom/ExternalBinding.h"
#endif

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesis.h"
#endif

#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif

#ifdef MOZ_B2G
#include "nsPISocketTransportService.h"
#endif

// Apple system headers seem to have a check() macro.  <sigh>
#ifdef check
class nsIScriptTimeoutHandler;
#undef check
#endif // check
#include "AccessCheck.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gDOMLeakPRLog;
#endif

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif

static const char kStorageEnabled[] = "dom.storage.enabled";

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using mozilla::TimeStamp;
using mozilla::TimeDuration;

nsGlobalWindow::WindowByIdTable *nsGlobalWindow::sWindowsById = nullptr;
bool nsGlobalWindow::sWarnedAboutWindowInternal = false;
bool nsGlobalWindow::sIdleObserversAPIFuzzTimeDisabled = false;

static nsIEntropyCollector *gEntropyCollector          = nullptr;
static int32_t              gRefCnt                    = 0;
static int32_t              gOpenPopupSpamCount        = 0;
static PopupControlState    gPopupControlState         = openAbused;
static int32_t              gRunningTimeoutDepth       = 0;
static bool                 gMouseDown                 = false;
static bool                 gDragServiceDisabled       = false;
static FILE                *gDumpFile                  = nullptr;
static uint64_t             gNextWindowID              = 0;
static uint32_t             gSerialCounter             = 0;
static uint32_t             gTimeoutsRecentlySet       = 0;
static TimeStamp            gLastRecordedRecentTimeouts;
#define STATISTICS_INTERVAL (30 * PR_MSEC_PER_SEC)

#ifdef DEBUG_jst
int32_t gTimeoutCnt                                    = 0;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"

// The default shortest interval/timeout we permit
#define DEFAULT_MIN_TIMEOUT_VALUE 4 // 4ms
#define DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE 1000 // 1000ms
static int32_t gMinTimeoutValue;
static int32_t gMinBackgroundTimeoutValue;
inline int32_t
nsGlobalWindow::DOMMinTimeoutValue() const {
  bool isBackground = !mOuterWindow || mOuterWindow->IsBackground();
  return
    std::max(isBackground ? gMinBackgroundTimeoutValue : gMinTimeoutValue, 0);
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
    if (!HasActiveDocument()) {                                                \
      NS_WARNING(outer ?                                                      \
                 "Inner window does not have active document." :              \
                 "No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return outer->method args;                                                \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_OR_THROW(method, args, errorresult, err_rval)        \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!HasActiveDocument()) {                                               \
      if (!outer) {                                                           \
        NS_WARNING("No outer window available!");                             \
        errorresult.Throw(NS_ERROR_NOT_INITIALIZED);                          \
      } else {                                                                \
        errorresult.Throw(NS_ERROR_XPC_SECURITY_MANAGER_VETO);                \
      }                                                                       \
    } else {                                                                  \
      return outer->method args;                                              \
    }                                                                         \
    return err_rval;                                                          \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!HasActiveDocument()) {                                               \
      NS_WARNING(outer ?                                                      \
                 "Inner window does not have active document." :              \
                 "No outer window available!");                               \
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
    if (!HasActiveDocument()) {                                               \
      NS_WARNING(outer ?                                                      \
                 "Inner window does not have active document." :              \
                 "No outer window available!");                               \
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
    if (!HasActiveDocument()) {                                               \
      NS_WARNING(outer ?                                                      \
                 "Inner window does not have active document." :              \
                 "No outer window available!");                               \
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

#define FORWARD_TO_INNER_OR_THROW(method, args, errorresult, err_rval)        \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      errorresult.Throw(NS_ERROR_NOT_INITIALIZED);                            \
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

static const char sPopStatePrefStr[] = "browser.history.allowPopState";

#define NETWORK_UPLOAD_EVENT_NAME     NS_LITERAL_STRING("moznetworkupload")
#define NETWORK_DOWNLOAD_EVENT_NAME   NS_LITERAL_STRING("moznetworkdownload")

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver MOZ_FINAL : public nsIObserver,
                                         public nsIInterfaceRequestor
{
public:
  nsGlobalWindowObserver(nsGlobalWindow* aWindow) : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
  {
    if (!mWindow)
      return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nullptr; }
  NS_IMETHODIMP GetInterface(const nsIID& aIID, void** aResult)
  {
    if (mWindow && aIID.Equals(NS_GET_IID(nsIDOMWindow)) && mWindow) {
      return mWindow->QueryInterface(aIID, aResult);
    }
    return NS_NOINTERFACE;
  }

private:
  nsGlobalWindow* mWindow;
};

NS_IMPL_ISUPPORTS2(nsGlobalWindowObserver, nsIObserver, nsIInterfaceRequestor)

nsTimeout::nsTimeout()
  : mCleared(false),
    mRunning(false),
    mIsInterval(false),
    mPublicId(0),
    mInterval(0),
    mFiringDepth(0),
    mNestingLevel(0),
    mPopupState(openAllowed)
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    ++gTimeoutCnt;
  }
#endif

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

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  MOZ_COUNT_DTOR(nsTimeout);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTimeout)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTimeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTimeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTimeout, Release)

// Return true if this timeout has a refcount of 1. This is used to check
// that dummy_timeout doesn't leak from nsGlobalWindow::RunTimeout.
bool
nsTimeout::HasRefCntOne()
{
  return mRefCnt.get() == 1;
}

nsPIDOMWindow::nsPIDOMWindow(nsPIDOMWindow *aOuterWindow)
: mFrameElement(nullptr), mDocShell(nullptr), mModalStateDepth(0),
  mRunningTimeout(nullptr), mMutationBits(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nullptr),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mMayHavePointerEnterLeaveEventListener(false),
  mIsModalContentWindow(false),
  mIsActive(false), mIsBackground(false),
  mAudioMuted(false), mAudioVolume(1.0),
  mInnerWindow(nullptr), mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(++gNextWindowID), mHasNotifiedGlobalCreated(false),
  mMarkedCCGeneration(0)
 {}

nsPIDOMWindow::~nsPIDOMWindow() {}

// DialogValueHolder CC goop.
NS_IMPL_CYCLE_COLLECTION_1(DialogValueHolder, mValue)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DialogValueHolder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DialogValueHolder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DialogValueHolder)

//*****************************************************************************
// nsOuterWindowProxy: Outer Window Proxy
//*****************************************************************************

class nsOuterWindowProxy : public js::Wrapper
{
public:
  nsOuterWindowProxy() : js::Wrapper(0) { }

  virtual bool finalizeInBackground(JS::Value priv) {
    return false;
  }

  virtual const char *className(JSContext *cx,
                                JS::Handle<JSObject*> wrapper) MOZ_OVERRIDE;
  virtual void finalize(JSFreeOp *fop, JSObject *proxy) MOZ_OVERRIDE;

  // Fundamental traps
  virtual bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
                           MOZ_OVERRIDE;
  virtual bool preventExtensions(JSContext *cx,
                                 JS::Handle<JSObject*> proxy) MOZ_OVERRIDE;
  virtual bool getPropertyDescriptor(JSContext* cx,
                                     JS::Handle<JSObject*> proxy,
                                     JS::Handle<jsid> id,
                                     JS::MutableHandle<JSPropertyDescriptor> desc,
                                     unsigned flags) MOZ_OVERRIDE;
  virtual bool getOwnPropertyDescriptor(JSContext* cx,
                                        JS::Handle<JSObject*> proxy,
                                        JS::Handle<jsid> id,
                                        JS::MutableHandle<JSPropertyDescriptor> desc,
                                        unsigned flags) MOZ_OVERRIDE;
  virtual bool defineProperty(JSContext* cx,
                              JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id,
                              JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
  virtual bool getOwnPropertyNames(JSContext *cx,
                                   JS::Handle<JSObject*> proxy,
                                   JS::AutoIdVector &props) MOZ_OVERRIDE;
  virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id,
                       bool *bp) MOZ_OVERRIDE;
  virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::AutoIdVector &props) MOZ_OVERRIDE;

  virtual bool watch(JSContext *cx, JS::Handle<JSObject*> proxy,
                     JS::Handle<jsid> id, JS::Handle<JSObject*> callable) MOZ_OVERRIDE;
  virtual bool unwatch(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id) MOZ_OVERRIDE;

  // Derived traps
  virtual bool has(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<jsid> id, bool *bp) MOZ_OVERRIDE;
  virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id, bool *bp) MOZ_OVERRIDE;
  virtual bool get(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<JSObject*> receiver,
                   JS::Handle<jsid> id,
                   JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;
  virtual bool set(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<JSObject*> receiver,
                   JS::Handle<jsid> id,
                   bool strict,
                   JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;
  virtual bool keys(JSContext *cx, JS::Handle<JSObject*> proxy,
                    JS::AutoIdVector &props) MOZ_OVERRIDE;
  virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> proxy,
                       unsigned flags,
                       JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;

  static nsOuterWindowProxy singleton;

protected:
  nsGlobalWindow* GetWindow(JSObject *proxy)
  {
    return nsGlobalWindow::FromSupports(
      static_cast<nsISupports*>(js::GetProxyExtra(proxy, 0).toPrivate()));
  }

  // False return value means we threw an exception.  True return value
  // but false "found" means we didn't have a subframe at that index.
  bool GetSubframeWindow(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp,
                         bool &found);

  // Returns a non-null window only if id is an index and we have a
  // window at that index.
  already_AddRefed<nsIDOMWindow> GetSubframeWindow(JSContext *cx,
                                                   JS::Handle<JSObject*> proxy,
                                                   JS::Handle<jsid> id);

  bool AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                  JS::AutoIdVector &props);
};

const js::Class OuterWindowProxyClass =
    PROXY_CLASS_WITH_EXT(
        "Proxy",
        0, /* additional slots */
        0, /* additional class flags */
        nullptr, /* call */
        nullptr, /* construct */
        PROXY_MAKE_EXT(
            nullptr, /* outerObject */
            js::proxy_innerObject,
            nullptr, /* iteratorObject */
            false   /* isWrappedNative */
        ));

bool
nsOuterWindowProxy::isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy,
                                 bool *extensible)
{
  // If [[Extensible]] could be false, then navigating a window could navigate
  // to a window that's [[Extensible]] after being at one that wasn't: an
  // invariant violation.  So always report true for this.
  *extensible = true;
  return true;
}

bool
nsOuterWindowProxy::preventExtensions(JSContext *cx,
                                      JS::Handle<JSObject*> proxy)
{
  // See above.
  JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                       JSMSG_CANT_CHANGE_EXTENSIBILITY);
  return false;
}

const char *
nsOuterWindowProxy::className(JSContext *cx, JS::Handle<JSObject*> proxy)
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "Window";
}

void
nsOuterWindowProxy::finalize(JSFreeOp *fop, JSObject *proxy)
{
  nsGlobalWindow* global = GetWindow(proxy);
  if (global) {
    global->ClearWrapper();

    // Ideally we would use OnFinalize here, but it's possible that
    // EnsureScriptEnvironment will later be called on the window, and we don't
    // want to create a new script object in that case. Therefore, we need to
    // write a non-null value that will reliably crash when dereferenced.
    global->PoisonOuterWindowProxy(proxy);
  }
}

bool
nsOuterWindowProxy::getPropertyDescriptor(JSContext* cx,
                                          JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc,
                                          unsigned flags)
{
  // The only thing we can do differently from js::Wrapper is shadow stuff with
  // our indexed properties, so we can just try getOwnPropertyDescriptor and if
  // that gives us nothing call on through to js::Wrapper.
  desc.object().set(nullptr);
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc, flags)) {
    return false;
  }

  if (desc.object()) {
    return true;
  }

  return js::Wrapper::getPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
nsOuterWindowProxy::getOwnPropertyDescriptor(JSContext* cx,
                                             JS::Handle<JSObject*> proxy,
                                             JS::Handle<jsid> id,
                                             JS::MutableHandle<JSPropertyDescriptor> desc,
                                             unsigned flags)
{
  bool found;
  if (!GetSubframeWindow(cx, proxy, id, desc.value(), found)) {
    return false;
  }
  if (found) {
    FillPropertyDescriptor(desc, proxy, true);
    return true;
  }
  // else fall through to js::Wrapper

  return js::Wrapper::getOwnPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
nsOuterWindowProxy::defineProperty(JSContext* cx,
                                   JS::Handle<JSObject*> proxy,
                                   JS::Handle<jsid> id,
                                   JS::MutableHandle<JSPropertyDescriptor> desc)
{
  int32_t index = GetArrayIndexFromId(cx, id);
  if (IsArrayIndex(index)) {
    // Spec says to Reject whether this is a supported index or not,
    // since we have no indexed setter or indexed creator.  That means
    // throwing in strict mode (FIXME: Bug 828137), doing nothing in
    // non-strict mode.
    return true;
  }

  return js::Wrapper::defineProperty(cx, proxy, id, desc);
}

bool
nsOuterWindowProxy::getOwnPropertyNames(JSContext *cx,
                                        JS::Handle<JSObject*> proxy,
                                        JS::AutoIdVector &props)
{
  // Just our indexed stuff followed by our "normal" own property names.
  if (!AppendIndexedPropertyNames(cx, proxy, props)) {
    return false;
  }

  JS::AutoIdVector innerProps(cx);
  if (!js::Wrapper::getOwnPropertyNames(cx, proxy, innerProps)) {
    return false;
  }
  return js::AppendUnique(cx, props, innerProps);
}

bool
nsOuterWindowProxy::delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id, bool *bp)
{
  if (nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id)) {
    // Reject (which means throw if strict, else return false) the delete.
    // Except we don't even know whether we're strict.  See bug 803157.
    *bp = false;
    return true;
  }

  int32_t index = GetArrayIndexFromId(cx, id);
  if (IsArrayIndex(index)) {
    // Indexed, but not supported.  Spec says return true.
    *bp = true;
    return true;
  }

  return js::Wrapper::delete_(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::enumerate(JSContext *cx, JS::Handle<JSObject*> proxy,
                              JS::AutoIdVector &props)
{
  // Just our indexed stuff followed by our "normal" own property names.
  if (!AppendIndexedPropertyNames(cx, proxy, props)) {
    return false;
  }

  JS::AutoIdVector innerProps(cx);
  if (!js::Wrapper::enumerate(cx, proxy, innerProps)) {
    return false;
  }
  return js::AppendUnique(cx, props, innerProps);
}

bool
nsOuterWindowProxy::has(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<jsid> id, bool *bp)
{
  if (nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::has(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                           JS::Handle<jsid> id, bool *bp)
{
  if (nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::hasOwn(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::get(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<JSObject*> receiver,
                        JS::Handle<jsid> id,
                        JS::MutableHandle<JS::Value> vp)
{
  if (id == nsDOMClassInfo::sWrappedJSObject_id &&
      xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
    vp.set(JS::ObjectValue(*proxy));
    return true;
  }

  bool found;
  if (!GetSubframeWindow(cx, proxy, id, vp, found)) {
    return false;
  }
  if (found) {
    return true;
  }
  // Else fall through to js::Wrapper

  return js::Wrapper::get(cx, proxy, receiver, id, vp);
}

bool
nsOuterWindowProxy::set(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<JSObject*> receiver,
                        JS::Handle<jsid> id,
                        bool strict,
                        JS::MutableHandle<JS::Value> vp)
{
  int32_t index = GetArrayIndexFromId(cx, id);
  if (IsArrayIndex(index)) {
    // Reject (which means throw if and only if strict) the set.
    if (strict) {
      // XXXbz This needs to throw, but see bug 828137.
    }
    return true;
  }

  return js::Wrapper::set(cx, proxy, receiver, id, strict, vp);
}

bool
nsOuterWindowProxy::keys(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::AutoIdVector &props)
{
  // BaseProxyHandler::keys seems to do what we want here: call
  // getOwnPropertyNames and then filter out the non-enumerable properties.
  return js::BaseProxyHandler::keys(cx, proxy, props);
}

bool
nsOuterWindowProxy::iterate(JSContext *cx, JS::Handle<JSObject*> proxy,
                            unsigned flags, JS::MutableHandle<JS::Value> vp)
{
  // BaseProxyHandler::iterate seems to do what we want here: fall
  // back on the property names returned from keys() and enumerate().
  return js::BaseProxyHandler::iterate(cx, proxy, flags, vp);
}

bool
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id,
                                      JS::MutableHandle<JS::Value> vp,
                                      bool& found)
{
  nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id);
  if (!frame) {
    found = false;
    return true;
  }

  found = true;
  // Just return the window's global
  nsGlobalWindow* global = static_cast<nsGlobalWindow*>(frame.get());
  global->EnsureInnerWindow();
  JSObject* obj = global->FastGetGlobalJSObject();
  // This null check fixes a hard-to-reproduce crash that occurs when we
  // get here when we're mid-call to nsDocShell::Destroy. See bug 640904
  // comment 105.
  if (MOZ_UNLIKELY(!obj)) {
    return xpc::Throw(cx, NS_ERROR_FAILURE);
  }

  vp.setObject(*obj);
  return JS_WrapValue(cx, vp);
}

already_AddRefed<nsIDOMWindow>
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id)
{
  int32_t index = GetArrayIndexFromId(cx, id);
  if (!IsArrayIndex(index)) {
    return nullptr;
  }

  nsGlobalWindow* win = GetWindow(proxy);
  bool unused;
  return win->IndexedGetter(index, unused);
}

bool
nsOuterWindowProxy::AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                               JS::AutoIdVector &props)
{
  uint32_t length = GetWindow(proxy)->Length();
  MOZ_ASSERT(int32_t(length) >= 0);
  if (!props.reserve(props.length() + length)) {
    return false;
  }
  for (int32_t i = 0; i < int32_t(length); ++i) {
    props.append(INT_TO_JSID(i));
  }

  return true;
}

bool
nsOuterWindowProxy::watch(JSContext *cx, JS::Handle<JSObject*> proxy,
                          JS::Handle<jsid> id, JS::Handle<JSObject*> callable)
{
  return js::WatchGuts(cx, proxy, id, callable);
}

bool
nsOuterWindowProxy::unwatch(JSContext *cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id)
{
  return js::UnwatchGuts(cx, proxy, id);
}

nsOuterWindowProxy
nsOuterWindowProxy::singleton;

class nsChromeOuterWindowProxy : public nsOuterWindowProxy
{
public:
  nsChromeOuterWindowProxy() : nsOuterWindowProxy() {}

  virtual const char *className(JSContext *cx, JS::Handle<JSObject*> wrapper) MOZ_OVERRIDE;

  static nsChromeOuterWindowProxy singleton;
};

const char *
nsChromeOuterWindowProxy::className(JSContext *cx,
                                    JS::Handle<JSObject*> proxy)
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "ChromeWindow";
}

nsChromeOuterWindowProxy
nsChromeOuterWindowProxy::singleton;

static JSObject*
NewOuterWindowProxy(JSContext *cx, JS::Handle<JSObject*> parent, bool isChrome)
{
  JSAutoCompartment ac(cx, parent);
  js::WrapperOptions options;
  options.setClass(&OuterWindowProxyClass);
  options.setSingleton(true);
  JSObject *obj = js::Wrapper::New(cx, parent, parent,
                                   isChrome ? &nsChromeOuterWindowProxy::singleton
                                            : &nsOuterWindowProxy::singleton,
                                   &options);

  NS_ASSERTION(js::GetObjectClass(obj)->ext.innerObject, "bad class");
  return obj;
}

//*****************************************************************************
//***    nsGlobalWindow: Object Management
//*****************************************************************************

nsGlobalWindow::nsGlobalWindow(nsGlobalWindow *aOuterWindow)
  : nsPIDOMWindow(aOuterWindow),
    mIdleFuzzFactor(0),
    mIdleCallbackIndex(-1),
    mCurrentlyIdle(false),
    mAddActiveEventFuzzTime(true),
    mIsFrozen(false),
    mFullScreen(false),
    mIsClosed(false),
    mInClose(false),
    mHavePendingClose(false),
    mHadOriginalOpener(false),
    mIsPopupSpam(false),
    mBlockScriptedClosingFlag(false),
    mFireOfflineStatusChangeEventOnThaw(false),
    mNotifyIdleObserversIdleOnThaw(false),
    mNotifyIdleObserversActiveOnThaw(false),
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
    mInnerObjectsFreed(false),
    mHasGamepad(false),
#ifdef MOZ_GAMEPAD
    mHasSeenGamepadInput(false),
#endif
    mNotifiedIDDestroyed(false),
    mAllowScriptsToClose(false),
    mTimeoutInsertionPoint(nullptr),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mTimeoutsSuspendDepth(0),
    mFocusMethod(0),
    mSerial(0),
#ifdef DEBUG
    mSetOpenerWindowCalled(false),
#endif
#ifdef MOZ_B2G
    mNetworkUploadObserverEnabled(false),
    mNetworkDownloadObserverEnabled(false),
#endif
    mCleanedUp(false),
    mDialogAbuseCount(0),
    mAreDialogsEnabled(true)
{
  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

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

    mObserver = nullptr;
    SetIsDOMBinding();
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  gRefCnt++;

  if (gRefCnt == 1) {
    Preferences::AddIntVarCache(&gMinTimeoutValue,
                                "dom.min_timeout_value",
                                DEFAULT_MIN_TIMEOUT_VALUE);
    Preferences::AddIntVarCache(&gMinBackgroundTimeoutValue,
                                "dom.min_background_timeout_value",
                                DEFAULT_MIN_BACKGROUND_TIMEOUT_VALUE);
    Preferences::AddBoolVarCache(&sIdleObserversAPIFuzzTimeDisabled, 
                                 "dom.idle-observers-api.fuzz_time.disabled",
                                 false);
  }

  if (gDumpFile == nullptr) {
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
    printf_stderr("++DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p]\n",
                  gRefCnt,
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  gSerialCounter,
                  static_cast<void*>(ToCanonicalSupports(aOuterWindow)));
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
  // We seem to see crashes in release builds because of null |sWindowsById|.
  if (sWindowsById) {
    sWindowsById->Put(mWindowID, this);
  }
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
}

static PLDHashOperator
DisconnectEventTargetObjects(nsPtrHashKey<DOMEventTargetHelper>* aKey,
                             void* aClosure)
{
  nsRefPtr<DOMEventTargetHelper> target = aKey->GetKey();
  target->DisconnectFromOwner();
  return PL_DHASH_NEXT;
}

nsGlobalWindow::~nsGlobalWindow()
{
  mEventTargetObjects.EnumerateEntries(DisconnectEventTargetObjects, nullptr);
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
    nsAutoCString url;
    if (mLastOpenedURI) {
      mLastOpenedURI->GetSpec(url);

      // Data URLs can be very long, so truncate to avoid flooding the log.
      const uint32_t maxURLLength = 1000;
      if (url.Length() > maxURLLength) {
        url.Truncate(maxURLLength);
      }
    }

    nsGlobalWindow* outer = static_cast<nsGlobalWindow*>(mOuterWindow.get());
    printf_stderr("--DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p] [url = %s]\n",
                  gRefCnt,
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  static_cast<void*>(ToCanonicalSupports(outer)),
                  url.get());
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
      js::SetProxyExtra(proxy, 0, js::PrivateValue(nullptr));
    }

    // An outer window is destroyed with inner windows still possibly
    // alive, iterate through the inner windows and null out their
    // back pointer to this outer, and pull them out of the list of
    // inner windows.

    nsGlobalWindow *w;
    while ((w = (nsGlobalWindow *)PR_LIST_HEAD(this)) != this) {
      PR_REMOVE_AND_INIT_LINK(w);
    }

    DropOuterWindowDocs();
  } else {
    Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                          mMutationBits ? 1 : 0);

    if (mListenerManager) {
      mListenerManager->Disconnect();
      mListenerManager = nullptr;
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

  // Outer windows are always supposed to call CleanUp before letting themselves
  // be destroyed. And while CleanUp generally seems to be intended to clean up
  // outers, we've historically called it for both. Changing this would probably
  // involve auditing all of the references that inners and outers can have, and
  // separating the handling into CleanUp() and FreeInnerObjects.
  if (IsInnerWindow()) {
    CleanUp();
  } else {
    MOZ_ASSERT(mCleanedUp);
  }

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->RemoveWindowAsListener(this);

  nsLayoutStatics::Release();
}

void
nsGlobalWindow::AddEventTargetObject(DOMEventTargetHelper* aObject)
{
  mEventTargetObjects.PutEntry(aObject);
}

void
nsGlobalWindow::RemoveEventTargetObject(DOMEventTargetHelper* aObject)
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
  gDumpFile = nullptr;

  NS_IF_RELEASE(gEntropyCollector);

  delete sWindowsById;
  sWindowsById = nullptr;
}

// static
void
nsGlobalWindow::CleanupCachedXBLHandlers(nsGlobalWindow* aWindow)
{
  if (aWindow->mCachedXBLPrototypeHandlers &&
      aWindow->mCachedXBLPrototypeHandlers->Count() > 0) {
    aWindow->mCachedXBLPrototypeHandlers->Clear();
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
nsGlobalWindow::DropOuterWindowDocs()
{
  MOZ_ASSERT(IsOuterWindow());
  MOZ_ASSERT_IF(mDoc, !mDoc->EventHandlingSuppressed());
  mDoc = nullptr;
  mSuspendedDoc = nullptr;
}

void
nsGlobalWindow::CleanUp()
{
  // Guarantee idempotence.
  if (mCleanedUp)
    return;
  mCleanedUp = true;

  mEventTargetObjects.EnumerateEntries(DisconnectEventTargetObjects, nullptr);
  mEventTargetObjects.Clear();

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, "dom-storage2-changed");
    }

#ifdef MOZ_B2G
    DisableNetworkEvent(NS_NETWORK_UPLOAD_EVENT);
    DisableNetworkEvent(NS_NETWORK_DOWNLOAD_EVENT);
#endif // MOZ_B2G

    if (mIdleService) {
      mIdleService->RemoveIdleObserver(mObserver, MIN_IDLE_NOTIFICATION_TIME_S);
    }

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
    NS_RELEASE(mObserver);
  }

  if (mNavigator) {
    mNavigator->Invalidate();
    mNavigator = nullptr;
  }

  mScreen = nullptr;
  mMenubar = nullptr;
  mToolbar = nullptr;
  mLocationbar = nullptr;
  mPersonalbar = nullptr;
  mStatusbar = nullptr;
  mScrollbars = nullptr;
  mLocation = nullptr;
  mHistory = nullptr;
  mFrames = nullptr;
  mWindowUtils = nullptr;
  mApplicationCache = nullptr;
  mIndexedDB = nullptr;

  mConsole = nullptr;

  mExternal = nullptr;

  mPerformance = nullptr;

#ifdef MOZ_WEBSPEECH
  mSpeechSynthesis = nullptr;
#endif

  ClearControllers();

  mOpener = nullptr;             // Forces Release
  if (mContext) {
    mContext = nullptr;            // Forces Release
  }
  mChromeEventHandler = nullptr; // Forces Release
  mParentTarget = nullptr;

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  if (inner) {
    inner->CleanUp();
  }

  DisableGamepadUpdates();
  mHasGamepad = false;

  if (mCleanMessageManager) {
    NS_ABORT_IF_FALSE(mIsChrome, "only chrome should have msg manager cleaned");
    nsGlobalChromeWindow *asChrome = static_cast<nsGlobalChromeWindow*>(this);
    if (asChrome->mMessageManager) {
      static_cast<nsFrameMessageManager*>(
        asChrome->mMessageManager.get())->Disconnect();
    }
  }

  mArguments = nullptr;
  mDialogArguments = nullptr;

  CleanupCachedXBLHandlers(this);

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = nullptr;
  }

  DisableTimeChangeNotifications();
}

void
nsGlobalWindow::ClearControllers()
{
  if (mControllers) {
    uint32_t count;
    mControllers->GetControllerCount(&count);

    while (count--) {
      nsCOMPtr<nsIController> controller;
      mControllers->GetControllerAt(count, getter_AddRefs(controller));

      nsCOMPtr<nsIControllerContext> context = do_QueryInterface(controller);
      if (context)
        context->SetCommandContext(nullptr);
    }

    mControllers = nullptr;
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

  mInnerObjectsFreed = true;

  // Kill all of the workers for this window.
  mozilla::dom::workers::CancelWorkersForWindow(this);

  // Close all offline storages for this window.
  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  if (quotaManager) {
    quotaManager->AbortCloseStoragesForWindow(this);
  }

  ClearAllTimeouts();

  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = nullptr;
  }

  mIdleObservers.Clear();

  mChromeEventHandler = nullptr;

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }

  mLocation = nullptr;
  mHistory = nullptr;

  if (mNavigator) {
    mNavigator->OnNavigation();
    mNavigator->Invalidate();
    mNavigator = nullptr;
  }

  if (mScreen) {
    mScreen = nullptr;
  }

  if (mDoc) {
    // Remember the document's principal and URI.
    mDocumentPrincipal = mDoc->NodePrincipal();
    mDocumentURI = mDoc->GetDocumentURI();
    mDocBaseURI = mDoc->GetDocBaseURI();

    while (mDoc->EventHandlingSuppressed()) {
      mDoc->UnsuppressEventHandlingAndFireEvents(nsIDocument::eEvents, false);
    }

    // Note: we don't have to worry about eAnimationsOnly suppressions because
    // they won't leak.
  }

  // Remove our reference to the document and the document principal.
  mFocusedNode = nullptr;

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->Disconnect();
    mApplicationCache = nullptr;
  }

  mIndexedDB = nullptr;

  NotifyWindowIDDestroyed("inner-window-destroyed");

  CleanupCachedXBLHandlers(this);

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

#ifdef MOZ_GAMEPAD
  mGamepads.Clear();
#endif
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

DOMCI_DATA(Window, nsGlobalWindow)

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindow)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
#ifdef MOZ_B2G
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowB2G)
#endif // MOZ_B2G
#ifdef MOZ_WEBSPEECH
  NS_INTERFACE_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif // MOZ_B2G
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSWindow)
  if (aIID.Equals(NS_GET_IID(nsIDOMWindowInternal))) {
    foundInterface = static_cast<nsIDOMWindowInternal*>(this);
    if (!sWarnedAboutWindowInternal) {
      sWarnedAboutWindowInternal = true;
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("Extensions"), mDoc,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "nsIDOMWindowInternalWarning");
    }
  } else
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowPerformance)
  NS_INTERFACE_MAP_ENTRY(nsITouchEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIInlineEventHandlers)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Window)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGlobalWindow)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGlobalWindow)

static PLDHashOperator
MarkXBLHandlers(nsXBLPrototypeHandler* aKey, JS::Heap<JSObject*>& aData, void* aClosure)
{
  JS::ExposeObjectToActiveJS(aData);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGlobalWindow)
  if (tmp->IsBlackForCC(false)) {
    if (tmp->mCachedXBLPrototypeHandlers) {
      tmp->mCachedXBLPrototypeHandlers->Enumerate(MarkXBLHandlers, nullptr);
    }
    if (EventListenerManager* elm = tmp->GetExistingListenerManager()) {
      elm->MarkForCC();
    }
    tmp->UnmarkGrayTimers();
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGlobalWindow)
  return tmp->IsBlackForCC(true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGlobalWindow)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            IdleObserverHolder& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mIdleObserver.get(), aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindow)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsGlobalWindow)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    PR_snprintf(name, sizeof(name), "nsGlobalWindow #%ld", tmp->mWindowID);
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGlobalWindow, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDialogArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReturnValue)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSpeechSynthesis)
#endif

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOuterWindow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)

  for (nsTimeout* timeout = tmp->mTimeouts.getFirst();
       timeout;
       timeout = timeout->getNext()) {
    cb.NoteNativeChild(timeout, NS_CYCLE_COLLECTION_PARTICIPANT(nsTimeout));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingStorageEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleObservers)

#ifdef MOZ_GAMEPAD
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepads)
#endif

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusedNode)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMenubar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mToolbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocationbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPersonalbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStatusbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScrollbars)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExternal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindow)
  nsGlobalWindow::CleanupCachedXBLHandlers(tmp);

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDialogArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReturnValue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechSynthesis)
#endif

  if (tmp->mOuterWindow) {
    static_cast<nsGlobalWindow*>(tmp->mOuterWindow.get())->MaybeClearInnerWindow(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOuterWindow)
  }

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessionStorage)
  if (tmp->mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(tmp->mApplicationCache.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplicationCache)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingStorageEvents)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

#ifdef MOZ_GAMEPAD
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGamepads)
#endif

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFocusedNode)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMenubar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mToolbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocationbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPersonalbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStatusbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScrollbars)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExternal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

#ifdef DEBUG
void
nsGlobalWindow::RiskyUnlink()
{
  NS_CYCLE_COLLECTION_INNERNAME.Unlink(this);
}
#endif

struct TraceData
{
  const TraceCallbacks& callbacks;
  void* closure;
};

static PLDHashOperator
TraceXBLHandlers(nsXBLPrototypeHandler* aKey, JS::Heap<JSObject*>& aData, void* aClosure)
{
  TraceData* data = static_cast<TraceData*>(aClosure);
  data->callbacks.Trace(&aData, "Cached XBL prototype handler", data->closure);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindow)
  if (tmp->mCachedXBLPrototypeHandlers) {
    TraceData data = { aCallbacks, aClosure };
    tmp->mCachedXBLPrototypeHandlers->Enumerate(TraceXBLHandlers, &data);
  }
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool
nsGlobalWindow::IsBlackForCC(bool aTracingNeeded)
{
  if (!nsCCUncollectableMarker::sGeneration) {
    return false;
  }

  return (nsCCUncollectableMarker::InGeneration(GetMarkedCCGeneration()) ||
          IsBlack()) &&
         (!aTracingNeeded ||
          HasNothingToTrace(static_cast<nsIDOMEventTarget*>(this)));
}

void
nsGlobalWindow::UnmarkGrayTimers()
{
  for (nsTimeout* timeout = mTimeouts.getFirst();
       timeout;
       timeout = timeout->getNext()) {
    if (timeout->mScriptHandler) {
      Function* f = timeout->mScriptHandler->GetCallback();
      if (f) {
        // Callable() already does xpc_UnmarkGrayObject.
        DebugOnly<JS::Handle<JSObject*> > o = f->Callable();
        MOZ_ASSERT(!xpc_IsGrayGCThing(o.value), "Should have been unmarked");
      }
    }
  }
}

//*****************************************************************************
// nsGlobalWindow::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindow::EnsureScriptEnvironment()
{
  nsGlobalWindow* outer = GetOuterWindowInternal();
  if (!outer) {
    NS_WARNING("No outer window available!");
    return NS_ERROR_FAILURE;
  }

  if (outer->GetWrapperPreserveColor()) {
    return NS_OK;
  }

  NS_ASSERTION(!outer->GetCurrentInnerWindowInternal(),
               "No cached wrapper, but we have an inner window?");

  // If this window is a [i]frame, don't bother GC'ing when the frame's context
  // is destroyed since a GC will happen when the frameset or host document is
  // destroyed anyway.
  nsCOMPtr<nsIScriptContext> context = new nsJSContext(!IsFrame(), outer);

  NS_ASSERTION(!outer->mContext, "Will overwrite mContext!");

  // should probably assert the context is clean???
  context->WillInitializeContext();

  nsresult rv = context->InitContext();
  NS_ENSURE_SUCCESS(rv, rv);

  outer->mContext = context;
  return NS_OK;
}

nsIScriptContext *
nsGlobalWindow::GetScriptContext()
{
  nsGlobalWindow* outer = GetOuterWindowInternal();
  return outer ? outer->mContext : nullptr;
}

JSObject *
nsGlobalWindow::GetGlobalJSObject()
{
  return FastGetGlobalJSObject();
}

void
nsGlobalWindow::TraceGlobalJSObject(JSTracer* aTrc)
{
  TraceWrapper(aTrc, "active window global");
}

/* static */
JSObject*
nsGlobalWindow::OuterObject(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  nsGlobalWindow* origWin;
  UNWRAP_OBJECT(Window, aObj, origWin);
  nsGlobalWindow* win = origWin->GetOuterWindowInternal();

  if (!win) {
    // If we no longer have an outer window. No code should ever be
    // running on a window w/o an outer, which means this hook should
    // never be called when we have no outer. But just in case, return
    // null to prevent leaking an inner window to code in a different
    // window.
    NS_WARNING("nsGlobalWindow::OuterObject shouldn't fail!");
    return nullptr;
  }

  JS::Rooted<JSObject*> winObj(aCx, win->FastGetGlobalJSObject());
  MOZ_ASSERT(winObj);

  // Note that while |wrapper| is same-compartment with cx, the outer window
  // might not be. If we're running script in an inactive scope and evalute
  // |this|, the outer window is actually a cross-compartment wrapper. So we
  // need to wrap here.
  if (!JS_WrapObject(aCx, &winObj)) {
    NS_WARNING("nsGlobalWindow::OuterObject shouldn't fail!");
    return nullptr;
  }

  return winObj;
}

bool
nsGlobalWindow::WouldReuseInnerWindow(nsIDocument *aNewDocument)
{
  // We reuse the inner window when:
  // a. We are currently at our original document.
  // b. At least one of the following conditions are true:
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

  return false;
}

void
nsGlobalWindow::SetInitialPrincipalToSubject()
{
  FORWARD_TO_OUTER_VOID(SetInitialPrincipalToSubject, ());

  // First, grab the subject principal.
  nsCOMPtr<nsIPrincipal> newWindowPrincipal = nsContentUtils::GetSubjectPrincipal();
  if (!newWindowPrincipal) {
    newWindowPrincipal = nsContentUtils::GetSystemPrincipal();
  }

  // Now, if we're about to use the system principal or an nsExpandedPrincipal,
  // make sure we're not using it for a content docshell.
  if (nsContentUtils::IsSystemOrExpandedPrincipal(newWindowPrincipal) &&
      GetDocShell()->ItemType() != nsIDocShellTreeItem::typeChrome) {
    newWindowPrincipal = nullptr;
  }

  // If there's an existing document, bail if it either:
  if (mDoc) {
    // (a) is not an initial about:blank document, or
    if (!mDoc->IsInitialDocument())
      return;
    // (b) already has the correct principal.
    if (mDoc->NodePrincipal() == newWindowPrincipal)
      return;

#ifdef DEBUG
    // If we have a document loaded at this point, it had better be about:blank.
    // Otherwise, something is really weird.
    nsCOMPtr<nsIURI> uri;
    mDoc->NodePrincipal()->GetURI(getter_AddRefs(uri));
    NS_ASSERTION(uri && NS_IsAboutBlank(uri) &&
                 NS_IsAboutBlank(mDoc->GetDocumentURI()),
                 "Unexpected original document");
#endif
  }

  GetDocShell()->CreateAboutBlankContentViewer(newWindowPrincipal);
  mDoc->SetIsInitialDocument(true);

  nsCOMPtr<nsIPresShell> shell = GetDocShell()->GetPresShell();

  if (shell && !shell->DidInitialize()) {
    // Ensure that if someone plays with this document they will get
    // layout happening.
    nsRect r = shell->GetPresContext()->GetVisibleArea();
    shell->Initialize(r.width, r.height);
  }
}

PopupControlState
PushPopupControlState(PopupControlState aState, bool aForce)
{
  MOZ_ASSERT(NS_IsMainThread());

  PopupControlState oldState = gPopupControlState;

  if (aState < gPopupControlState || aForce) {
    gPopupControlState = aState;
  }

  return oldState;
}

void
PopPopupControlState(PopupControlState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());
  return gPopupControlState;
}

#define WINDOWSTATEHOLDER_IID \
{0x0b917c3e, 0xbd50, 0x4683, {0xaf, 0xc9, 0xc7, 0x81, 0x07, 0xae, 0x33, 0x26}}

class WindowStateHolder MOZ_FINAL : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  WindowStateHolder(nsGlobalWindow *aWindow);

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }

  void DidRestoreWindow()
  {
    mInnerWindow = nullptr;
  }

protected:
  ~WindowStateHolder();

  nsRefPtr<nsGlobalWindow> mInnerWindow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow* aWindow)
  : mInnerWindow(aWindow)
{
  NS_PRECONDITION(aWindow, "null window");
  NS_PRECONDITION(aWindow->IsInnerWindow(), "Saving an outer window");

  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  mInnerWindow->PreserveWrapper(ToSupports(mInnerWindow));

  aWindow->SuspendTimeouts();

  // When a global goes into the bfcache, we disable script.
  xpc::Scriptability::Get(aWindow->GetWrapperPreserveColor()).SetDocShellAllowsScript(false);
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

// We need certain special behavior for remote XUL whitelisted domains, but we
// don't want that behavior to take effect in automation, because we whitelist
// all the mochitest domains. So we need to check a pref here.
static bool
TreatAsRemoteXUL(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(aPrincipal));
  return nsContentUtils::AllowXULXBLForPrincipal(aPrincipal) &&
         !Preferences::GetBool("dom.use_xbl_scopes_for_remote_xul", false);
}

/**
 * Create a new global object that will be used for an inner window.
 * Return the native global and an nsISupports 'holder' that can be used
 * to manage the lifetime of it.
 */
static nsresult
CreateNativeGlobalForInner(JSContext* aCx,
                           nsGlobalWindow* aNewInner,
                           nsIURI* aURI,
                           nsIPrincipal* aPrincipal,
                           JS::MutableHandle<JSObject*> aGlobal)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aNewInner);
  MOZ_ASSERT(aNewInner->IsInnerWindow());
  MOZ_ASSERT(aPrincipal);

  // DOMWindow with nsEP is not supported, we have to make sure
  // no one creates one accidentally.
  nsCOMPtr<nsIExpandedPrincipal> nsEP = do_QueryInterface(aPrincipal);
  MOZ_RELEASE_ASSERT(!nsEP, "DOMWindow with nsEP is not supported");

  nsGlobalWindow *top = nullptr;
  if (aNewInner->GetOuterWindow()) {
    top = aNewInner->GetTop();
  }
  JS::CompartmentOptions options;
  if (top) {
    if (top->GetGlobalJSObject()) {
      options.setSameZoneAs(top->GetGlobalJSObject());
    }
  }

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  // Determine if we need the Components object.
  bool needComponents = nsContentUtils::IsSystemPrincipal(aPrincipal) ||
                        TreatAsRemoteXUL(aPrincipal);
  uint32_t flags = needComponents ? 0 : nsIXPConnect::OMIT_COMPONENTS_OBJECT;
  flags |= nsIXPConnect::DONT_FIRE_ONNEWGLOBALHOOK;

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  nsresult rv = xpc->InitClassesWithNewWrappedGlobal(
    aCx, ToSupports(aNewInner),
    aPrincipal, flags, options, getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  aGlobal.set(holder->GetJSObject());

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aNewInner->GetWrapperPreserveColor() == aGlobal);
  xpc::SetLocationForGlobal(aGlobal, aURI);

  return NS_OK;
}

nsresult
nsGlobalWindow::SetNewDocument(nsIDocument* aDocument,
                               nsISupports* aState,
                               bool aForceReuseInnerWindow)
{
  NS_PRECONDITION(mDocumentPrincipal == nullptr,
                  "mDocumentPrincipal prematurely set!");
  MOZ_ASSERT(aDocument);

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
               GetCurrentInnerWindow()->GetExtantDoc() == mDoc,
               "Uh, mDoc doesn't match the current inner window "
               "document!");

  bool wouldReuseInnerWindow = WouldReuseInnerWindow(aDocument);
  if (aForceReuseInnerWindow &&
      !wouldReuseInnerWindow &&
      mDoc &&
      mDoc->NodePrincipal() != aDocument->NodePrincipal()) {
    NS_ERROR("Attempted forced inner window reuse while changing principal");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDocument> oldDoc = mDoc;

  nsIScriptContext *scx = GetContextInternal();
  NS_ENSURE_TRUE(scx, NS_ERROR_NOT_INITIALIZED);

  JSContext *cx = scx->GetNativeContext();

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  // clear smartcard events, our document has gone away.
  if (mCrypto && XRE_GetProcessType() != GeckoProcessType_Content) {
    nsresult rv = mCrypto->SetEnableSmartCardEvents(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  if (!mDoc) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsIDOMWindow* privateRoot = nsGlobalWindow::GetPrivateRoot();

    if (privateRoot == static_cast<nsIDOMWindow*>(this)) {
      nsXBLService::AttachGlobalKeyHandler(mChromeEventHandler);
    }
  }

  /* No mDocShell means we're already been partially closed down.  When that
     happens, setting status isn't a big requirement, so don't. (Doesn't happen
     under normal circumstances, but bug 49615 describes a case.) */

  nsContentUtils::AddScriptRunner(
    NS_NewRunnableMethod(this, &nsGlobalWindow::ClearStatus));

  // Sometimes, WouldReuseInnerWindow() returns true even if there's no inner
  // window (see bug 776497). Be safe.
  bool reUseInnerWindow = (aForceReuseInnerWindow || wouldReuseInnerWindow) &&
                          GetCurrentInnerWindowInternal();

  nsresult rv = NS_OK;

  // Set mDoc even if this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.
  mDoc = aDocument;
  if (IsInnerWindow() && IsDOMBinding()) {
    WindowBinding::ClearCachedDocumentValue(cx, this);
  }

  // Take this opportunity to clear mSuspendedDoc. Our old inner window is now
  // responsible for unsuspending it.
  mSuspendedDoc = nullptr;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  mContext->WillInitializeContext();

  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

  if (currentInner && currentInner->mNavigator) {
    currentInner->mNavigator->OnNavigation();
  }

  nsRefPtr<nsGlobalWindow> newInnerWindow;
  bool createdInnerWindow = false;

  bool thisChrome = IsChromeWindow();

  nsCxPusher cxPusher;
  cxPusher.Push(cx);

  // Check if we're near the stack limit before we get anywhere near the
  // transplanting code.
  JS_CHECK_RECURSION(cx, return NS_ERROR_FAILURE);

  nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
  NS_ASSERTION(!aState || wsh, "What kind of weird state are you giving me here?");

  JS::Rooted<JSObject*> newInnerGlobal(cx);
  if (reUseInnerWindow) {
    // We're reusing the current inner window.
    NS_ASSERTION(!currentInner->IsFrozen(),
                 "We should never be reusing a shared inner window");
    newInnerWindow = currentInner;
    newInnerGlobal = currentInner->GetWrapperPreserveColor();

    if (aDocument != oldDoc) {
      JS::ExposeObjectToActiveJS(newInnerGlobal);
    }

    // We're reusing the inner window, but this still counts as a navigation,
    // so all expandos and such defined on the outer window should go away. Force
    // all Xray wrappers to be recomputed.
    JS::Rooted<JSObject*> rootedObject(cx, GetWrapperPreserveColor());
    JS::ExposeObjectToActiveJS(rootedObject);
    if (!JS_RefreshCrossCompartmentWrappers(cx, rootedObject)) {
      return NS_ERROR_FAILURE;
    }

    // Inner windows are only reused for same-origin principals, but the principals
    // don't necessarily match exactly. Update the principal on the compartment to
    // match the new document.
    // NB: We don't just call currentInner->RefreshCompartmentPrincipals() here
    // because we haven't yet set its mDoc to aDocument.
    JSCompartment *compartment = js::GetObjectCompartment(newInnerGlobal);
#ifdef DEBUG
    bool sameOrigin = false;
    nsIPrincipal *existing =
      nsJSPrincipals::get(JS_GetCompartmentPrincipals(compartment));
    aDocument->NodePrincipal()->Equals(existing, &sameOrigin);
    MOZ_ASSERT(sameOrigin);
#endif
    JS_SetCompartmentPrincipals(compartment,
                                nsJSPrincipals::get(aDocument->NodePrincipal()));
  } else {
    if (aState) {
      newInnerWindow = wsh->GetInnerWindow();
      newInnerGlobal = newInnerWindow->GetWrapperPreserveColor();
    } else {
      if (thisChrome) {
        newInnerWindow = new nsGlobalChromeWindow(this);
      } else if (mIsModalContentWindow) {
        newInnerWindow = new nsGlobalModalWindow(this);
      } else {
        newInnerWindow = new nsGlobalWindow(this);
      }

      // Freeze the outer window and null out the inner window so
      // that initializing classes on the new inner doesn't end up
      // reaching into the old inner window for classes etc.
      //
      // [This happens with Object.prototype when XPConnect creates
      // a temporary global while initializing classes; the reason
      // being that xpconnect creates the temp global w/o a parent
      // and proto, which makes the JS engine look up classes in
      // cx->globalObject, i.e. this outer window].

      mInnerWindow = nullptr;

      Freeze();
      mCreatingInnerWindow = true;
      // Every script context we are initialized with must create a
      // new global.
      rv = CreateNativeGlobalForInner(cx, newInnerWindow,
                                      aDocument->GetDocumentURI(),
                                      aDocument->NodePrincipal(),
                                      &newInnerGlobal);
      NS_ASSERTION(NS_SUCCEEDED(rv) && newInnerGlobal &&
                   newInnerWindow->GetWrapperPreserveColor() == newInnerGlobal,
                   "Failed to get script global");

      mCreatingInnerWindow = false;
      createdInnerWindow = true;
      Thaw();

      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (currentInner && currentInner->GetWrapperPreserveColor()) {
      if (oldDoc == aDocument) {
        // Move the navigator from the old inner window to the new one since
        // this is a document.write. This is safe from a same-origin point of
        // view because document.write can only be used by the same origin.
        newInnerWindow->mNavigator = currentInner->mNavigator;
        currentInner->mNavigator = nullptr;
        if (newInnerWindow->mNavigator) {
          newInnerWindow->mNavigator->SetWindow(newInnerWindow);
        }

        // Make a copy of the old window's performance object on document.open.
        // Note that we have to force eager creation of it here, because we need
        // to grab the current document channel and whatnot before that changes.
        currentInner->CreatePerformanceObjectIfNeeded();
        if (currentInner->mPerformance) {
          newInnerWindow->mPerformance =
            new nsPerformance(newInnerWindow,
                              currentInner->mPerformance->GetDOMTiming(),
                              currentInner->mPerformance->GetChannel(),
                              currentInner->mPerformance->GetParentPerformance());
        }
      }

      // Don't free objects on our current inner window if it's going to be
      // held in the bfcache.
      if (!currentInner->IsFrozen()) {
        currentInner->FreeInnerObjects();
      }
    }

    mInnerWindow = newInnerWindow;

    if (!GetWrapperPreserveColor()) {
      JS::Rooted<JSObject*> outer(cx,
        NewOuterWindowProxy(cx, newInnerGlobal, thisChrome));
      NS_ENSURE_TRUE(outer, NS_ERROR_FAILURE);

      js::SetProxyExtra(outer, 0, js::PrivateValue(ToSupports(this)));

      // Inform the nsJSContext, which is the canonical holder of the outer.
      mContext->SetWindowProxy(outer);
      mContext->DidInitializeContext();

      SetWrapper(mContext->GetWindowProxy());
    } else {
      JS::ExposeObjectToActiveJS(newInnerGlobal);
      JS::Rooted<JSObject*> outerObject(cx,
        NewOuterWindowProxy(cx, newInnerGlobal, thisChrome));
      if (!outerObject) {
        NS_ERROR("out of memory");
        return NS_ERROR_FAILURE;
      }

      JS::Rooted<JSObject*> obj(cx, GetWrapperPreserveColor());

      js::SetProxyExtra(obj, 0, js::PrivateValue(nullptr));

      outerObject = xpc::TransplantObject(cx, obj, outerObject);
      if (!outerObject) {
        NS_ERROR("unable to transplant wrappers, probably OOM");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyExtra(outerObject, 0, js::PrivateValue(ToSupports(this)));

      SetWrapper(outerObject);

      {
        JSAutoCompartment ac(cx, outerObject);

        JS_SetParent(cx, outerObject, newInnerGlobal);

        // Inform the nsJSContext, which is the canonical holder of the outer.
        mContext->SetWindowProxy(outerObject);

        NS_ASSERTION(!JS_IsExceptionPending(cx),
                     "We might overwrite a pending exception!");
        XPCWrappedNativeScope* scope = xpc::GetObjectScope(outerObject);
        if (scope->mWaiverWrapperMap) {
          scope->mWaiverWrapperMap->Reparent(cx, newInnerGlobal);
        }
      }
    }

    // Enter the new global's compartment.
    JSAutoCompartment ac(cx, GetWrapperPreserveColor());

    // Set scriptability based on the state of the docshell.
    bool allow = GetDocShell()->GetCanExecuteScripts();
    xpc::Scriptability::Get(GetWrapperPreserveColor()).SetDocShellAllowsScript(allow);

    // If we created a new inner window above, we need to do the last little bit
    // of initialization now that the dust has settled.
    if (createdInnerWindow) {
      nsIXPConnect *xpc = nsContentUtils::XPConnect();
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      nsresult rv = xpc->GetWrappedNativeOfJSObject(cx, newInnerGlobal,
                                                    getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ABORT_IF_FALSE(wrapper, "bad wrapper");
      rv = wrapper->FinishInitForWrappedGlobal();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!aState) {
      JS::Rooted<JSObject*> rootedWrapper(cx, GetWrapperPreserveColor());
      if (!JS_DefineProperty(cx, newInnerGlobal, "window", rootedWrapper,
                             JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
                             JS_PropertyStub, JS_StrictPropertyStub)) {
        NS_ERROR("can't create the 'window' property");
        return NS_ERROR_FAILURE;
      }
    }
  }

  JSAutoCompartment ac(cx, GetWrapperPreserveColor());

  if (!aState && !reUseInnerWindow) {
    // Loading a new page and creating a new inner window, *not*
    // restoring from session history.

    // Now that both the the inner and outer windows are initialized
    // let the script context do its magic to hook them together.
    MOZ_ASSERT(mContext->GetWindowProxy() == GetWrapperPreserveColor());
#ifdef DEBUG
    JS::Rooted<JSObject*> rootedJSObject(cx, GetWrapperPreserveColor());
    JS::Rooted<JSObject*> proto1(cx), proto2(cx);
    JS_GetPrototype(cx, rootedJSObject, &proto1);
    JS_GetPrototype(cx, newInnerGlobal, &proto2);
    NS_ASSERTION(proto1 == proto2,
                 "outer and inner globals should have the same prototype");
#endif

    nsCOMPtr<Element> frame = GetFrameElementInternal();
    if (frame) {
      nsPIDOMWindow* parentWindow = frame->OwnerDoc()->GetWindow();
      if (parentWindow && parentWindow->TimeoutSuspendCount()) {
        SuspendTimeouts(parentWindow->TimeoutSuspendCount());
      }
    }
  }

  // Add an extra ref in case we release mContext during GC.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(mContext);

  aDocument->SetScriptGlobalObject(newInnerWindow);

  if (!aState) {
    if (reUseInnerWindow) {
      if (newInnerWindow->mDoc != aDocument) {
        newInnerWindow->mDoc = aDocument;

        if (newInnerWindow->IsDOMBinding()) {
          WindowBinding::ClearCachedDocumentValue(cx, newInnerWindow);
        } else {
          // We're reusing the inner window for a new document. In this
          // case we don't clear the inner window's scope, but we must
          // make sure the cached document property gets updated.

          JS::Rooted<JSObject*> obj(cx,
                                    currentInner->GetWrapperPreserveColor());
          ::JS_DeleteProperty(cx, obj, "document");
        }
      }
    } else {
      newInnerWindow->InnerSetNewDocument(cx, aDocument);

      // Initialize DOM classes etc on the inner window.
      JS::Rooted<JSObject*> obj(cx, newInnerGlobal);
      rv = mContext->InitClasses(obj);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // If the document comes from a JAR, check if the channel was determined
    // to be unsafe. If so, permanently disable script on the compartment by
    // calling Block() and throwing away the key.
    nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(aDocument->GetChannel());
    if (jarChannel && jarChannel->GetIsUnsafe()) {
      xpc::Scriptability::Get(newInnerGlobal).Block();
    }

    if (mArguments) {
      newInnerWindow->DefineArgumentsProperty(mArguments);
      mArguments = nullptr;
    }

    // Give the new inner window our chrome event handler (since it
    // doesn't have one).
    newInnerWindow->mChromeEventHandler = mChromeEventHandler;
  }

  mContext->GC(JS::gcreason::SET_NEW_DOCUMENT);
  mContext->DidInitializeContext();

  // We wait to fire the debugger hook until the window is all set up and hooked
  // up with the outer. See bug 969156.
  if (createdInnerWindow) {
    JS::Rooted<JSObject*> global(cx, newInnerWindow->GetWrapper());
    JS_FireOnNewGlobalObject(cx, global);
  }

  if (newInnerWindow && !newInnerWindow->mHasNotifiedGlobalCreated && mDoc) {
    // We should probably notify. However if this is the, arguably bad,
    // situation when we're creating a temporary non-chrome-about-blank
    // document in a chrome docshell, don't notify just yet. Instead wait
    // until we have a real chrome doc.
    if (!mDocShell ||
        mDocShell->ItemType() != nsIDocShellTreeItem::typeChrome ||
        nsContentUtils::IsSystemPrincipal(mDoc->NodePrincipal())) {
      newInnerWindow->mHasNotifiedGlobalCreated = true;
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsGlobalWindow::DispatchDOMWindowCreated));
    }
  }

  PreloadLocalStorage();

  return NS_OK;
}

void
nsGlobalWindow::PreloadLocalStorage()
{
  if (!Preferences::GetBool(kStorageEnabled)) {
    return;
  }

  if (IsChromeWindow()) {
    return;
  }

  nsIPrincipal* principal = GetPrincipal();
  if (!principal) {
    return;
  }

  nsresult rv;

  nsCOMPtr<nsIDOMStorageManager> storageManager =
    do_GetService("@mozilla.org/dom/localStorage-manager;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  storageManager->PrecacheStorage(principal);
}

void
nsGlobalWindow::DispatchDOMWindowCreated()
{
  if (!mDoc) {
    return;
  }

  // Fire DOMWindowCreated at chrome event listeners
  nsContentUtils::DispatchChromeEvent(mDoc, mDoc, NS_LITERAL_STRING("DOMWindowCreated"),
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
}

void
nsGlobalWindow::InnerSetNewDocument(JSContext* aCx, nsIDocument* aDocument)
{
  NS_PRECONDITION(IsInnerWindow(), "Must only be called on inner windows");
  MOZ_ASSERT(aDocument);

#ifdef PR_LOGGING
  if (gDOMLeakPRLog && PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    nsAutoCString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOMWINDOW %p SetNewDocument %s", this, spec.get());
  }
#endif

  mDoc = aDocument;
  if (IsDOMBinding()) {
    WindowBinding::ClearCachedDocumentValue(aCx, this);
  }
  mFocusedNode = nullptr;
  mLocalStorage = nullptr;
  mSessionStorage = nullptr;

#ifdef DEBUG
  mLastOpenedURI = aDocument->GetDocumentURI();
#endif

  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                        mMutationBits ? 1 : 0);

  // Clear our mutation bitfield.
  mMutationBits = 0;
}

void
nsGlobalWindow::SetDocShell(nsIDocShell* aDocShell)
{
  NS_ASSERTION(IsOuterWindow(), "Uh, SetDocShell() called on inner window!");
  MOZ_ASSERT(aDocShell);

  if (aDocShell == mDocShell) {
    return;
  }

  mDocShell = aDocShell; // Weak Reference

  NS_ASSERTION(!mNavigator, "Non-null mNavigator in outer window!");

  if (mFrames) {
    mFrames->SetDocShell(aDocShell);
  }

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
    else {
      mChromeEventHandler = NS_NewWindowRoot(this);
    }
  }

  bool docShellActive;
  mDocShell->GetIsActive(&docShellActive);
  mIsBackground = !docShellActive;
}

void
nsGlobalWindow::DetachFromDocShell()
{
  NS_ASSERTION(IsOuterWindow(), "Uh, DetachFromDocShell() called on inner window!");

  // DetachFromDocShell means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // so that it can be retrieved later (until it is finalized by the JS GC).

  NS_ASSERTION(mTimeouts.isEmpty(), "Uh, outer window holds timeouts!");

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

    // Remember the document's principal and URI.
    mDocumentPrincipal = mDoc->NodePrincipal();
    mDocumentURI = mDoc->GetDocumentURI();
    mDocBaseURI = mDoc->GetDocBaseURI();

    // Release our document reference
    DropOuterWindowDocs();
    mFocusedNode = nullptr;
  }

  ClearControllers();

  mChromeEventHandler = nullptr; // force release now

  if (mContext) {
    mContext->GC(JS::gcreason::SET_DOC_SHELL);
    mContext = nullptr;
  }

  mDocShell = nullptr; // Weak Reference

  NS_ASSERTION(!mNavigator, "Non-null mNavigator in outer window!");

  if (mFrames) {
    mFrames->SetDocShell(nullptr);
  }

  MaybeForgiveSpamCount();
  CleanUp();
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
already_AddRefed<EventTarget>
TryGetTabChildGlobalAsEventTarget(nsISupports *aFrom)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(aFrom);
  if (!frameLoaderOwner) {
    return nullptr;
  }

  nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  if (!frameLoader) {
    return nullptr;
  }

  nsCOMPtr<EventTarget> target = frameLoader->GetTabChildGlobalAsEventTarget();
  return target.forget();
}

void
nsGlobalWindow::UpdateParentTarget()
{
  // Try to get our frame element's tab child global (its in-process message
  // manager).  If that fails, fall back to the chrome event handler's tab
  // child global, and if it doesn't have one, just use the chrome event
  // handler itself.

  nsCOMPtr<Element> frameElement = GetFrameElementInternal();
  nsCOMPtr<EventTarget> eventTarget =
    TryGetTabChildGlobalAsEventTarget(frameElement);

  if (!eventTarget) {
    nsGlobalWindow* topWin = GetScriptableTop();
    if (topWin) {
      frameElement = topWin->GetFrameElementInternal();
      eventTarget = TryGetTabChildGlobalAsEventTarget(frameElement);
    }
  }

  if (!eventTarget) {
    eventTarget = TryGetTabChildGlobalAsEventTarget(mChromeEventHandler);
  }

  if (!eventTarget) {
    eventTarget = mChromeEventHandler;
  }

  mParentTarget = eventTarget;
}

EventTarget*
nsGlobalWindow::GetTargetForDOMEvent()
{
  return GetOuterWindowInternal();
}

EventTarget*
nsGlobalWindow::GetTargetForEventTargetChain()
{
  return IsInnerWindow() ? this : GetCurrentInnerWindowInternal();
}

nsresult
nsGlobalWindow::WillHandleEvent(EventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

JSContext*
nsGlobalWindow::GetJSContextForEventHandlers()
{
  return nullptr;
}

nsresult
nsGlobalWindow::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PreHandleEvent is used on outer window!?");
  static uint32_t count = 0;
  uint32_t msg = aVisitor.mEvent->message;

  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  if ((msg == NS_MOUSE_MOVE) && gEntropyCollector) {
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case. Means we get a little
    //more entropy.
    if (count++ % 100 == 0) {
      //Since the high bits seem to be zero's most of the time,
      //let's only take the lowest half of the point structure.
      int16_t myCoord[2];

      myCoord[0] = aVisitor.mEvent->refPoint.x;
      myCoord[1] = aVisitor.mEvent->refPoint.y;
      gEntropyCollector->RandomUpdate((void*)myCoord, sizeof(myCoord));
      gEntropyCollector->RandomUpdate((void*)&(aVisitor.mEvent->time),
                                      sizeof(uint32_t));
    }
  } else if (msg == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = true;
  } else if (msg == NS_MOUSE_BUTTON_DOWN &&
             aVisitor.mEvent->mFlags.mIsTrusted) {
    gMouseDown = true;
  } else if ((msg == NS_MOUSE_BUTTON_UP ||
              msg == NS_DRAGDROP_END) &&
             aVisitor.mEvent->mFlags.mIsTrusted) {
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

  // Handle 'active' event.
  if (!mIdleObservers.IsEmpty() &&
      aVisitor.mEvent->mFlags.mIsTrusted &&
      (aVisitor.mEvent->HasMouseEventMessage() ||
       aVisitor.mEvent->HasDragEventMessage())) {
    mAddActiveEventFuzzTime = false;
  }

  return NS_OK;
}

bool
nsGlobalWindow::ShouldPromptToBlockDialogs()
{
  MOZ_ASSERT(IsOuterWindow());

  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ASSERTION(!mDocShell, "ShouldPromptToBlockDialogs() called without a top window?");
    return true;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (!topWindow) {
    return true;
  }

  return topWindow->DialogsAreBeingAbused();
}

bool
nsGlobalWindow::AreDialogsEnabled()
{
  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ERROR("AreDialogsEnabled() called without a top window?");
    return false;
  }

  // TODO: Warn if no top window?
  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (!topWindow) {
    return false;
  }

  // Dialogs are blocked if the content viewer is hidden
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));

    bool isHidden;
    cv->GetIsHidden(&isHidden);
    if (isHidden) {
      return false;
    }
  }

  return topWindow->mAreDialogsEnabled;
}

bool
nsGlobalWindow::DialogsAreBeingAbused()
{
  MOZ_ASSERT(IsInnerWindow());
  NS_ASSERTION(GetScriptableTop() &&
               GetScriptableTop()->GetCurrentInnerWindowInternal() == this,
               "DialogsAreBeingAbused called with invalid window");

  if (mLastDialogQuitTime.IsNull() ||
      nsContentUtils::IsCallerChrome()) {
    return false;
  }

  TimeDuration dialogInterval(TimeStamp::Now() - mLastDialogQuitTime);
  if (dialogInterval.ToSeconds() <
      Preferences::GetInt("dom.successive_dialog_time_limit",
                          DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT)) {
    mDialogAbuseCount++;

    return GetPopupControlState() > openAllowed ||
           mDialogAbuseCount > MAX_SUCCESSIVE_DIALOG_COUNT;
  }

  // Reset the abuse counter
  mDialogAbuseCount = 0;

  return false;
}

bool
nsGlobalWindow::ConfirmDialogIfNeeded()
{
  MOZ_ASSERT(IsOuterWindow());

  NS_ENSURE_TRUE(mDocShell, false);
  nsCOMPtr<nsIPromptService> promptSvc =
    do_GetService("@mozilla.org/embedcomp/prompt-service;1");

  if (!promptSvc) {
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
    DisableDialogs();
    return false;
  }

  return true;
}

void
nsGlobalWindow::DisableDialogs()
{
  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ERROR("DisableDialogs() called without a top window?");
    return;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  // TODO: Warn if no top window?
  if (topWindow) {
    topWindow->mAreDialogsEnabled = false;
  }
}

void
nsGlobalWindow::EnableDialogs()
{
  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ERROR("EnableDialogs() called without a top window?");
    return;
  }

  // TODO: Warn if no top window?
  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (topWindow) {
    topWindow->mAreDialogsEnabled = true;
  }
}

nsresult
nsGlobalWindow::PostHandleEvent(EventChainPostVisitor& aVisitor)
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
             aVisitor.mEvent->mFlags.mIsTrusted) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDoc) {
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->message == NS_LOAD &&
             aVisitor.mEvent->mFlags.mIsTrusted) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = true;

    nsCOMPtr<Element> element = GetFrameElementInternal();
    nsIDocShell* docShell = GetDocShell();
    if (element && GetParentInternal() &&
        docShell && docShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      WidgetEvent event(aVisitor.mEvent->mFlags.mIsTrusted, NS_LOAD);
      event.mFlags.mBubbles = false;

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.
      EventDispatcher::Dispatch(element, nullptr, &event, nullptr, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::DispatchDOMEvent(WidgetEvent* aEvent,
                                 nsIDOMEvent* aDOMEvent,
                                 nsPresContext* aPresContext,
                                 nsEventStatus* aEventStatus)
{
  return EventDispatcher::DispatchDOMEvent(static_cast<nsPIDOMWindow*>(this),
                                           aEvent, aDOMEvent, aPresContext,
                                           aEventStatus);
}

void
nsGlobalWindow::PoisonOuterWindowProxy(JSObject *aObject)
{
  MOZ_ASSERT(IsOuterWindow());
  if (aObject == GetWrapperPreserveColor()) {
    PoisonWrapper();
  }
}

nsresult
nsGlobalWindow::SetArguments(nsIArray *aArguments)
{
  MOZ_ASSERT(IsOuterWindow());
  nsresult rv;

  // Historically, we've used the same machinery to handle openDialog arguments
  // (exposed via window.arguments) and showModalDialog arguments (exposed via
  // window.dialogArguments), even though the former is XUL-only and uses an XPCOM
  // array while the latter is web-exposed and uses an arbitrary JS value.
  // Moreover, per-spec |dialogArguments| is a property of the browsing context
  // (outer), whereas |arguments| lives on the inner.
  //
  // We've now mostly separated them, but the difference is still opaque to
  // nsWindowWatcher (the caller of SetArguments in this little back-and-forth
  // embedding waltz we do here).
  //
  // So we need to demultiplex the two cases here.
  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();
  if (mIsModalContentWindow) {
    // nsWindowWatcher blindly converts the original nsISupports into an array
    // of length 1. We need to recover it, and then cast it back to the concrete
    // object we know it to be.
    nsCOMPtr<nsISupports> supports = do_QueryElementAt(aArguments, 0, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mDialogArguments = static_cast<DialogValueHolder*>(supports.get());
  } else {
    mArguments = aArguments;
    rv = currentInner->DefineArgumentsProperty(aArguments);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::DefineArgumentsProperty(nsIArray *aArguments)
{
  MOZ_ASSERT(!mIsModalContentWindow); // Handled separately.
  nsIScriptContext *ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx, NS_ERROR_NOT_INITIALIZED);
  AutoJSContext cx;

  JS::Rooted<JSObject*> obj(cx, GetWrapperPreserveColor());
  return ctx->SetProperty(obj, "arguments", aArguments);
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

  return nullptr;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindow
//*****************************************************************************

nsIURI*
nsPIDOMWindow::GetDocumentURI() const
{
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}

nsIURI*
nsPIDOMWindow::GetDocBaseURI() const
{
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

void
nsPIDOMWindow::MaybeCreateDoc()
{
  MOZ_ASSERT(!mDoc);
  if (nsIDocShell* docShell = GetDocShell()) {
    // Note that |document| here is the same thing as our mDoc, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<nsIDocument> document = do_GetInterface(docShell);
  }
}

Element*
nsPIDOMWindow::GetFrameElementInternal() const
{
  if (mOuterWindow) {
    return mOuterWindow->GetFrameElementInternal();
  }

  NS_ASSERTION(!IsInnerWindow(),
               "GetFrameElementInternal() called on orphan inner window");

  return mFrameElement;
}

void
nsPIDOMWindow::SetFrameElementInternal(Element* aFrameElement)
{
  if (IsOuterWindow()) {
    mFrameElement = aFrameElement;

    return;
  }

  if (!mOuterWindow) {
    NS_ERROR("frameElement set on inner window with no outer!");

    return;
  }

  mOuterWindow->SetFrameElementInternal(aFrameElement);
}

void
nsPIDOMWindow::AddAudioContext(AudioContext* aAudioContext)
{
  mAudioContexts.AppendElement(aAudioContext);

  nsIDocShell* docShell = GetDocShell();
  if (docShell && !docShell->GetAllowMedia() && !aAudioContext->IsOffline()) {
    aAudioContext->Mute();
  }
}

void
nsPIDOMWindow::RemoveAudioContext(AudioContext* aAudioContext)
{
  mAudioContexts.RemoveElement(aAudioContext);
}

void
nsPIDOMWindow::MuteAudioContexts()
{
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Mute();
    }
  }
}

void
nsPIDOMWindow::UnmuteAudioContexts()
{
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Unmute();
    }
  }
}

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocument** aDocument)
{
  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(GetDocument());
  document.forget(aDocument);
  return NS_OK;
}

nsIDOMWindow*
nsGlobalWindow::GetWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetWindow, (aError), aError, nullptr);

  return this;
}

NS_IMETHODIMP
nsGlobalWindow::GetWindow(nsIDOMWindow** aWindow)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMWindow> window = GetWindow(rv);
  window.forget(aWindow);

  return rv.ErrorCode();
}

nsIDOMWindow*
nsGlobalWindow::GetSelf(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetSelf, (aError), aError, nullptr);

  return this;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelf(nsIDOMWindow** aWindow)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMWindow> window = GetSelf(rv);
  window.forget(aWindow);

  return rv.ErrorCode();
}

Navigator*
nsGlobalWindow::GetNavigator(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetNavigator, (aError), aError, nullptr);

  if (!mNavigator) {
    mNavigator = new Navigator(this);
  }

  return mNavigator;
}

NS_IMETHODIMP
nsGlobalWindow::GetNavigator(nsIDOMNavigator** aNavigator)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMNavigator> navigator = GetNavigator(rv);
  navigator.forget(aNavigator);

  return rv.ErrorCode();
}

nsScreen*
nsGlobalWindow::GetScreen(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetScreen, (aError), aError, nullptr);

  if (!mScreen) {
    mScreen = nsScreen::Create(this);
    if (!mScreen) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
  }

  return mScreen;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreen(nsIDOMScreen** aScreen)
{
  ErrorResult rv;
  nsRefPtr<nsScreen> screen = GetScreen(rv);
  screen.forget(aScreen);

  return rv.ErrorCode();
}

nsHistory*
nsGlobalWindow::GetHistory(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetHistory, (aError), aError, nullptr);

  if (!mHistory) {
    mHistory = new nsHistory(this);
  }

  return mHistory;
}

NS_IMETHODIMP
nsGlobalWindow::GetHistory(nsISupports** aHistory)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> history = GetHistory(rv);
  history.forget(aHistory);

  return rv.ErrorCode();
}

nsPerformance*
nsGlobalWindow::GetPerformance(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetPerformance, (aError), aError, nullptr);

  return nsPIDOMWindow::GetPerformance();
}

NS_IMETHODIMP
nsGlobalWindow::GetPerformance(nsISupports** aPerformance)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> performance = GetPerformance(rv);
  performance.forget(aPerformance);

  return rv.ErrorCode();
}

nsPerformance*
nsPIDOMWindow::GetPerformance()
{
  MOZ_ASSERT(IsInnerWindow());
  CreatePerformanceObjectIfNeeded();
  return mPerformance;
}

void
nsPIDOMWindow::CreatePerformanceObjectIfNeeded()
{
  if (mPerformance || !mDoc) {
    return;
  }
  nsRefPtr<nsDOMNavigationTiming> timing = mDoc->GetNavigationTiming();
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(mDoc->GetChannel()));
  bool timingEnabled = false;
  if (!timedChannel ||
      !NS_SUCCEEDED(timedChannel->GetTimingEnabled(&timingEnabled)) ||
      !timingEnabled) {
    timedChannel = nullptr;
  }
  if (timing) {
    // If we are dealing with an iframe, we will need the parent's performance
    // object (so we can add the iframe as a resource of that page).
    nsPerformance* parentPerformance = nullptr;
    nsCOMPtr<nsIDOMWindow> parentWindow;
    GetScriptableParent(getter_AddRefs(parentWindow));
    nsCOMPtr<nsPIDOMWindow> parentPWindow = do_GetInterface(parentWindow);
    if (GetOuterWindow() != parentPWindow) {
      if (parentPWindow && !parentPWindow->IsInnerWindow()) {
        parentPWindow = parentPWindow->GetCurrentInnerWindow();
      }
      if (parentPWindow) {
        parentPerformance = parentPWindow->GetPerformance();
      }
    }
    mPerformance =
      new nsPerformance(this, timing, timedChannel, parentPerformance);
  }
}

bool
nsPIDOMWindow::GetAudioMuted() const
{
  if (!IsInnerWindow()) {
    return mInnerWindow->GetAudioMuted();
  }

  return mAudioMuted;
}

void
nsPIDOMWindow::SetAudioMuted(bool aMuted)
{
  if (!IsInnerWindow()) {
    mInnerWindow->SetAudioMuted(aMuted);
    return;
  }

  if (mAudioMuted == aMuted) {
    return;
  }

  mAudioMuted = aMuted;
  RefreshMediaElements();
}

float
nsPIDOMWindow::GetAudioVolume() const
{
  if (!IsInnerWindow()) {
    return mInnerWindow->GetAudioVolume();
  }

  return mAudioVolume;
}

nsresult
nsPIDOMWindow::SetAudioVolume(float aVolume)
{
  if (!IsInnerWindow()) {
    return mInnerWindow->SetAudioVolume(aVolume);
  }

  if (aVolume < 0.0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (mAudioVolume == aVolume) {
    return NS_OK;
  }

  mAudioVolume = aVolume;
  RefreshMediaElements();
  return NS_OK;
}

float
nsPIDOMWindow::GetAudioGlobalVolume()
{
  float globalVolume = 1.0;
  nsCOMPtr<nsPIDOMWindow> window = this;

  do {
    if (window->GetAudioMuted()) {
      return 0;
    }

    globalVolume *= window->GetAudioVolume();

    nsCOMPtr<nsIDOMWindow> win;
    window->GetParent(getter_AddRefs(win));
    if (window == win) {
      break;
    }

    window = do_QueryInterface(win);

    // If there is not parent, or we are the toplevel or the volume is
    // already 0.0, we don't continue.
  } while (window && window != this && globalVolume);

  return globalVolume;
}

void
nsPIDOMWindow::RefreshMediaElements()
{
  nsRefPtr<AudioChannelService> service =
    AudioChannelService::GetAudioChannelService();
  if (service) {
    service->RefreshAgentsVolume(this);
  }
}

// nsISpeechSynthesisGetter

#ifdef MOZ_WEBSPEECH
SpeechSynthesis*
nsGlobalWindow::GetSpeechSynthesis(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetSpeechSynthesis, (aError), aError, nullptr);

  if (!mSpeechSynthesis) {
    mSpeechSynthesis = new SpeechSynthesis(this);
  }

  return mSpeechSynthesis;
}

NS_IMETHODIMP
nsGlobalWindow::GetSpeechSynthesis(nsISupports** aSpeechSynthesis)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> speechSynthesis;
  if (Preferences::GetBool("media.webspeech.synth.enabled")) {
    speechSynthesis = GetSpeechSynthesis(rv);
  }
  speechSynthesis.forget(aSpeechSynthesis);

  return rv.ErrorCode();
}
#endif

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::GetParent(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetParent, (aError), aError, nullptr);

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMWindow> parent;
  if (mDocShell->GetIsBrowserOrApp()) {
    parent = this;
  } else {
    aError = GetRealParent(getter_AddRefs(parent));
  }

  return parent.forget();
}

/**
 * GetScriptableParent is called when script reads window.parent.
 *
 * In contrast to GetRealParent, GetScriptableParent respects <iframe
 * mozbrowser> boundaries, so if |this| is contained by an <iframe
 * mozbrowser>, we will return |this| as its own parent.
 */
NS_IMETHODIMP
nsGlobalWindow::GetScriptableParent(nsIDOMWindow** aParent)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMWindow> parent = GetParent(rv);
  parent.forget(aParent);

  return rv.ErrorCode();
}

/**
 * nsIDOMWindow::GetParent (when called from C++) is just a wrapper around
 * GetRealParent.
 */
NS_IMETHODIMP
nsGlobalWindow::GetRealParent(nsIDOMWindow** aParent)
{
  FORWARD_TO_OUTER(GetRealParent, (aParent), NS_ERROR_NOT_INITIALIZED);

  *aParent = nullptr;
  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent));

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

static nsresult
GetTopImpl(nsGlobalWindow* aWin, nsIDOMWindow** aTop, bool aScriptable)
{
  *aTop = nullptr;

  // Walk up the parent chain.

  nsCOMPtr<nsIDOMWindow> prevParent = aWin;
  nsCOMPtr<nsIDOMWindow> parent = aWin;
  do {
    if (!parent) {
      break;
    }

    prevParent = parent;

    nsCOMPtr<nsIDOMWindow> newParent;
    nsresult rv;
    if (aScriptable) {
      rv = parent->GetScriptableParent(getter_AddRefs(newParent));
    }
    else {
      rv = parent->GetParent(getter_AddRefs(newParent));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    parent = newParent;

  } while (parent != prevParent);

  if (parent) {
    parent.swap(*aTop);
  }

  return NS_OK;
}

/**
 * GetScriptableTop is called when script reads window.top.
 *
 * In contrast to GetRealTop, GetScriptableTop respects <iframe mozbrowser>
 * boundaries.  If we encounter a window owned by an <iframe mozbrowser> while
 * walking up the window hierarchy, we'll stop and return that window.
 */
NS_IMETHODIMP
nsGlobalWindow::GetScriptableTop(nsIDOMWindow **aTop)
{
  FORWARD_TO_OUTER(GetScriptableTop, (aTop), NS_ERROR_NOT_INITIALIZED);
  return GetTopImpl(this, aTop, /* aScriptable = */ true);
}

/**
 * nsIDOMWindow::GetTop (when called from C++) is just a wrapper around
 * GetRealTop.
 */
NS_IMETHODIMP
nsGlobalWindow::GetRealTop(nsIDOMWindow** aTop)
{
  nsGlobalWindow* outer;
  if (IsInnerWindow()) {
    outer = GetOuterWindowInternal();
    if (!outer) {
      NS_WARNING("No outer window available!");
      return NS_ERROR_NOT_INITIALIZED;
    }
  } else {
    outer = this;
  }
  return GetTopImpl(outer, aTop, /* aScriptable = */ false);
}

JSObject*
nsGlobalWindow::GetContent(JSContext* aCx, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetContent, (aCx, aError), aError, nullptr);

  nsCOMPtr<nsIDOMWindow> content = GetContentInternal(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (content) {
    JS::Rooted<JS::Value> val(aCx);
    aError = nsContentUtils::WrapNative(aCx, content, &val);
    if (aError.Failed()) {
      return nullptr;
    }

    return &val.toObject();
  }

  if (!nsContentUtils::IsCallerChrome() || !IsChromeWindow()) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Something tries to get .content on a ChromeWindow, try to fetch the CPOW.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
  if (!treeOwner) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JS::Rooted<JS::Value> val(aCx, JS::NullValue());
  aError = treeOwner->GetContentWindow(aCx, &val);
  if (aError.Failed()) {
    return nullptr;
  }

  return val.toObjectOrNull();
}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::GetContentInternal(ErrorResult& aError)
{
  // First check for a named frame named "content"
  nsCOMPtr<nsIDOMWindow> domWindow =
    GetChildWindow(NS_LITERAL_STRING("content"));
  if (domWindow) {
    return domWindow.forget();
  }

  // If we're contained in <iframe mozbrowser> or <iframe mozapp>, then
  // GetContent is the same as window.top.
  if (mDocShell && mDocShell->GetIsInBrowserOrApp()) {
    return GetTop(aError);
  }

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
        mDocShell->GetSameTypeRootTreeItem(getter_AddRefs(primaryContent));
      }
    }
  }

  if (!primaryContent) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
    if (!treeOwner) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  domWindow = do_GetInterface(primaryContent);
  return domWindow.forget();
}

NS_IMETHODIMP
nsGlobalWindow::GetContent(nsIDOMWindow** aContent)
{
  ErrorResult rv;
  *aContent = GetContentInternal(rv).take();

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::GetScriptableContent(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  ErrorResult rv;
  JS::Rooted<JSObject*> content(aCx, GetContent(aCx, rv));
  if (!rv.Failed()) {
    aVal.setObjectOrNull(content);
  }

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::GetPrompter(nsIPrompt** aPrompt)
{
  if (IsInnerWindow()) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    if (!outer) {
      NS_WARNING("No outer window available!");
      return NS_ERROR_NOT_INITIALIZED;
    }
    return outer->GetPrompter(aPrompt);
  }

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

BarProp*
nsGlobalWindow::GetMenubar(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetMenubar, (aError), aError, nullptr);

  if (!mMenubar) {
    mMenubar = new MenubarProp(this);
  }

  return mMenubar;
}

NS_IMETHODIMP
nsGlobalWindow::GetMenubar(nsISupports** aMenubar)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> menubar = GetMenubar(rv);
  menubar.forget(aMenubar);

  return rv.ErrorCode();
}

BarProp*
nsGlobalWindow::GetToolbar(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetToolbar, (aError), aError, nullptr);

  if (!mToolbar) {
    mToolbar = new ToolbarProp(this);
  }

  return mToolbar;
}

NS_IMETHODIMP
nsGlobalWindow::GetToolbar(nsISupports** aToolbar)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> toolbar = GetToolbar(rv);
  toolbar.forget(aToolbar);

  return rv.ErrorCode();
}

BarProp*
nsGlobalWindow::GetLocationbar(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetLocationbar, (aError), aError, nullptr);

  if (!mLocationbar) {
    mLocationbar = new LocationbarProp(this);
  }
  return mLocationbar;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocationbar(nsISupports** aLocationbar)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> locationbar = GetLocationbar(rv);
  locationbar.forget(aLocationbar);

  return rv.ErrorCode();
}

BarProp*
nsGlobalWindow::GetPersonalbar(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetPersonalbar, (aError), aError, nullptr);

  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarProp(this);
  }
  return mPersonalbar;
}

NS_IMETHODIMP
nsGlobalWindow::GetPersonalbar(nsISupports** aPersonalbar)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> personalbar = GetPersonalbar(rv);
  personalbar.forget(aPersonalbar);

  return rv.ErrorCode();
}

BarProp*
nsGlobalWindow::GetStatusbar(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetStatusbar, (aError), aError, nullptr);

  if (!mStatusbar) {
    mStatusbar = new StatusbarProp(this);
  }
  return mStatusbar;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatusbar(nsISupports** aStatusbar)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> statusbar = GetStatusbar(rv);
  statusbar.forget(aStatusbar);

  return rv.ErrorCode();
}

BarProp*
nsGlobalWindow::GetScrollbars(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetScrollbars, (aError), aError, nullptr);

  if (!mScrollbars) {
    mScrollbars = new ScrollbarsProp(this);
  }

  return mScrollbars;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollbars(nsISupports** aScrollbars)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> scrollbars = GetScrollbars(rv);
  scrollbars.forget(aScrollbars);

  return rv.ErrorCode();
}

bool
nsGlobalWindow::GetClosed(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetClosed, (aError), aError, false);

  // If someone called close(), or if we don't have a docshell, we're closed.
  return mIsClosed || !mDocShell;
}

NS_IMETHODIMP
nsGlobalWindow::GetClosed(bool* aClosed)
{
  ErrorResult rv;
  *aClosed = GetClosed(rv);

  return rv.ErrorCode();
}

nsDOMWindowList*
nsGlobalWindow::GetWindowList()
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
  }

  return mFrames;
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindowCollection** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = GetWindowList();
  NS_IF_ADDREF(*aFrames);
  return NS_OK;
}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;

  FORWARD_TO_OUTER(IndexedGetter, (aIndex, aFound), nullptr);

  nsDOMWindowList* windows = GetWindowList();
  NS_ENSURE_TRUE(windows, nullptr);

  return windows->IndexedGetter(aIndex, aFound);
}

void
nsGlobalWindow::GetSupportedNames(nsTArray<nsString>& aNames)
{
  FORWARD_TO_OUTER_VOID(GetSupportedNames, (aNames));

  nsDOMWindowList* windows = GetWindowList();
  if (windows) {
    uint32_t length = windows->GetLength();
    nsString* name = aNames.AppendElements(length);
    for (uint32_t i = 0; i < length; ++i, ++name) {
      nsCOMPtr<nsIDocShellTreeItem> item =
        windows->GetDocShellTreeItemAt(i);
      item->GetName(*name);
    }
  }
}

bool
nsGlobalWindow::DoNewResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                             JS::Handle<jsid> aId,
                             JS::MutableHandle<JSPropertyDescriptor> aDesc)
{
  MOZ_ASSERT(IsInnerWindow());

  if (!JSID_IS_STRING(aId)) {
    return true;
  }

  nsresult rv = nsWindowSH::GlobalResolve(this, aCx, aObj, aId, aDesc);
  if (NS_FAILED(rv)) {
    return Throw(aCx, rv);
  }

  return true;
}

struct GlobalNameEnumeratorClosure
{
  GlobalNameEnumeratorClosure(JSContext* aCx, nsGlobalWindow* aWindow,
                              nsTArray<nsString>& aNames)
    : mCx(aCx),
      mWindow(aWindow),
      mWrapper(aCx, aWindow->GetWrapper()),
      mNames(aNames)
  {
  }

  JSContext* mCx;
  nsGlobalWindow* mWindow;
  JS::Rooted<JSObject*> mWrapper;
  nsTArray<nsString>& mNames;
};

static PLDHashOperator
EnumerateGlobalName(const nsAString& aName,
                    const nsGlobalNameStruct& aNameStruct,
                    void* aClosure)
{
  GlobalNameEnumeratorClosure* closure =
    static_cast<GlobalNameEnumeratorClosure*>(aClosure);

  if (aNameStruct.mType == nsGlobalNameStruct::eTypeStaticNameSet) {
    // We have no idea what names this might install.
    return PL_DHASH_NEXT;
  }

  if (nsWindowSH::NameStructEnabled(closure->mCx, closure->mWindow, aName,
                                    aNameStruct) &&
      (!aNameStruct.mConstructorEnabled ||
       aNameStruct.mConstructorEnabled(closure->mCx, closure->mWrapper))) {
    closure->mNames.AppendElement(aName);
  }
  return PL_DHASH_NEXT;
}

void
nsGlobalWindow::GetOwnPropertyNames(JSContext* aCx, nsTArray<nsString>& aNames,
                                    ErrorResult& aRv)
{
  // "Components" is marked as enumerable but only resolved on demand :-/.
  //aNames.AppendElement(NS_LITERAL_STRING("Components"));

  nsScriptNameSpaceManager* nameSpaceManager = GetNameSpaceManager();
  if (nameSpaceManager) {
    GlobalNameEnumeratorClosure closure(aCx, this, aNames);
    nameSpaceManager->EnumerateGlobalNames(EnumerateGlobalName, &closure);
  }
}

/* static */ bool
nsGlobalWindow::IsChromeWindow(JSContext* aCx, JSObject* aObj)
{
  // For now, have to deal with XPConnect objects here.
  nsGlobalWindow* win;
  nsresult rv = UNWRAP_OBJECT(Window, aObj, win);
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryWrapper(aCx, aObj);
    win = static_cast<nsGlobalWindow*>(piWin.get());
  }
  return win->IsChromeWindow();
}

nsIDOMOfflineResourceList*
nsGlobalWindow::GetApplicationCache(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetApplicationCache, (aError), aError, nullptr);

  if (!mApplicationCache) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetDocShell()));
    if (!webNav) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsIURI> uri;
    aError = webNav->GetCurrentURI(getter_AddRefs(uri));
    if (aError.Failed()) {
      return nullptr;
    }

    nsCOMPtr<nsIURI> manifestURI;
    nsContentUtils::GetOfflineAppManifest(mDoc, getter_AddRefs(manifestURI));

    nsRefPtr<nsDOMOfflineResourceList> applicationCache =
      new nsDOMOfflineResourceList(manifestURI, uri, this);

    applicationCache->Init();

    mApplicationCache = applicationCache;
  }

  return mApplicationCache;
}

NS_IMETHODIMP
nsGlobalWindow::GetApplicationCache(nsIDOMOfflineResourceList **aApplicationCache)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMOfflineResourceList> applicationCache =
    GetApplicationCache(rv);
  applicationCache.forget(aApplicationCache);

  return rv.ErrorCode();
}

nsIDOMCrypto*
nsGlobalWindow::GetCrypto(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetCrypto, (aError), aError, nullptr);

  if (!mCrypto) {
#ifndef MOZ_DISABLE_CRYPTOLEGACY
    if (XRE_GetProcessType() != GeckoProcessType_Content) {
      nsresult rv;
      mCrypto = do_CreateInstance(NS_CRYPTO_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aError.Throw(rv);
        return nullptr;
      }
    } else
#endif
    {
      mCrypto = new Crypto();
    }

    mCrypto->Init(this);
  }
  return mCrypto;
}

NS_IMETHODIMP
nsGlobalWindow::GetCrypto(nsIDOMCrypto** aCrypto)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMCrypto> crypto = GetCrypto(rv);
  crypto.forget(aCrypto);

  return rv.ErrorCode();
}

nsIControllers*
nsGlobalWindow::GetControllers(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetControllers, (aError), aError, nullptr);

  if (!mControllers) {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    // Add in the default controller
    nsCOMPtr<nsIController> controller = do_CreateInstance(
                               NS_WINDOWCONTROLLER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }
    
    mControllers->InsertControllerAt(0, controller);
    nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller);
    if (!controllerContext) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    controllerContext->SetCommandContext(static_cast<nsIDOMWindow*>(this));
  }

  return mControllers;
}

NS_IMETHODIMP
nsGlobalWindow::GetControllers(nsIControllers** aResult)
{
  ErrorResult rv;
  nsCOMPtr<nsIControllers> controllers = GetControllers(rv);
  controllers.forget(aResult);

  return rv.ErrorCode();
}

nsIDOMWindow*
nsGlobalWindow::GetOpener(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOpener, (aError), aError, nullptr);

  nsCOMPtr<nsPIDOMWindow> opener = do_QueryReferent(mOpener);
  if (!opener) {
    return nullptr;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::IsCallerChrome()) {
    return opener;
  }

  // First, ensure that we're not handing back a chrome window.
  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(opener.get());
  if (win->IsChromeWindow()) {
    return nullptr;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIDocShell> openerDocShell = opener->GetDocShell();

  if (openerDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    openerDocShell->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      uint32_t appType;
      nsresult rv = openerRootDocShell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
        return opener;
      }
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsGlobalWindow::GetOpener(nsIDOMWindow** aOpener)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMWindow> opener = GetOpener(rv);
  opener.forget(aOpener);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetOpener(nsIDOMWindow* aOpener, ErrorResult& aError)
{
  // Check if we were called from a privileged chrome script.  If not, and if
  // aOpener is not null, just define aOpener on our inner window's JS object,
  // wrapped into the current compartment so that for Xrays we define on the
  // Xray expando object, but don't set it on the outer window, so that it'll
  // get reset on navigation.  This is just like replaceable properties, but
  // we're not quite readonly.
  if (aOpener && !nsContentUtils::IsCallerChrome()) {
    // JS_WrapObject will outerize, so we don't care if aOpener is an inner.
    nsCOMPtr<nsIGlobalObject> glob = do_QueryInterface(aOpener);
    if (!glob) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    AutoJSContext cx;
    JSAutoRequest ar(cx);
    // Note we explicitly do NOT enter any particular compartment here; we want
    // the caller compartment in cases when we have a caller, so that we define
    // expandos on Xrays as needed.

    JS::Rooted<JSObject*> otherObj(cx, glob->GetGlobalJSObject());
    if (!otherObj) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    JS::Rooted<JSObject*> thisObj(cx, GetWrapperPreserveColor());
    if (!thisObj) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return;
    }

    if (!JS_WrapObject(cx, &otherObj) ||
        !JS_WrapObject(cx, &thisObj) ||
        !JS_DefineProperty(cx, thisObj, "opener", otherObj, JSPROP_ENUMERATE,
                           JS_PropertyStub, JS_StrictPropertyStub)) {
      aError.Throw(NS_ERROR_FAILURE);
    }

    return;
  }

  SetOpenerWindow(aOpener, false);
}

NS_IMETHODIMP
nsGlobalWindow::SetOpener(nsIDOMWindow* aOpener)
{
  ErrorResult rv;
  SetOpener(aOpener, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::GetStatus(nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetStatus, (aStatus, aError), aError, );

  aStatus = mStatus;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatus(nsAString& aStatus)
{
  ErrorResult rv;
  GetStatus(aStatus, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetStatus(const nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetStatus, (aStatus, aError), aError, );

  mStatus = aStatus;

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent propagating window.status to the UI by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return;
  }

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = GetWebBrowserChrome();
  if (browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT,
                             PromiseFlatString(aStatus).get());
  }
}

NS_IMETHODIMP
nsGlobalWindow::SetStatus(const nsAString& aStatus)
{
  ErrorResult rv;
  SetStatus(aStatus, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::GetName(nsAString& aName, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetName, (aName, aError), aError, );

  if (mDocShell) {
    mDocShell->GetName(aName);
  }
}

NS_IMETHODIMP
nsGlobalWindow::GetName(nsAString& aName)
{
  ErrorResult rv;
  GetName(aName, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetName(const nsAString& aName, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetName, (aName, aError), aError, );

  if (mDocShell) {
    aError = mDocShell->SetName(aName);
  }
}

NS_IMETHODIMP
nsGlobalWindow::SetName(const nsAString& aName)
{
  ErrorResult rv;
  SetName(aName, rv);

  return rv.ErrorCode();
}

// Helper functions used by many methods below.
int32_t
nsGlobalWindow::DevToCSSIntPixels(int32_t px)
{
  if (!mDocShell)
    return px; // assume 1:1

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return px;

  return presContext->DevPixelsToIntCSSPixels(px);
}

int32_t
nsGlobalWindow::CSSToDevIntPixels(int32_t px)
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

nsresult
nsGlobalWindow::GetInnerSize(CSSIntSize& aSize)
{
  MOZ_ASSERT(IsOuterWindow());

  EnsureSizeUpToDate();

  NS_ENSURE_STATE(mDocShell);

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  nsRefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presContext || !presShell) {
    aSize = CSSIntSize(0, 0);
    return NS_OK;
  }

  /*
   * On platforms with resolution-based zooming, the CSS viewport
   * and visual viewport may not be the same. The inner size should
   * be the visual viewport, but we fall back to the CSS viewport
   * if it is not set.
   */
  if (presShell->IsScrollPositionClampingScrollPortSizeSet()) {
    aSize = CSSIntRect::FromAppUnitsRounded(
      presShell->GetScrollPositionClampingScrollPortSize());
  } else {
    nsRefPtr<nsViewManager> viewManager = presShell->GetViewManager();
    if (viewManager) {
      viewManager->FlushDelayedResize(false);
    }

    aSize = CSSIntRect::FromAppUnitsRounded(
      presContext->GetVisibleArea().Size());
  }
  return NS_OK;
}

int32_t
nsGlobalWindow::GetInnerWidth(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetInnerWidth, (aError), aError, 0);

  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.width;
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerWidth(int32_t* aInnerWidth)
{
  ErrorResult rv;
  *aInnerWidth = GetInnerWidth(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetInnerWidth(int32_t aInnerWidth, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerWidth, (aInnerWidth, aError), aError, );

  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerWidth by exiting early
   */
  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  CheckSecurityWidthAndHeight(&aInnerWidth, nullptr);

  nsRefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nscoord height = 0;

    nsRefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    height = shellArea.height;
    SetCSSViewportWidthAndHeight(nsPresContext::CSSPixelsToAppUnits(aInnerWidth),
                                 height);
    return;
  }

  int32_t height = 0;
  int32_t unused  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&unused, &height);
  aError = SetDocShellWidthAndHeight(CSSToDevIntPixels(aInnerWidth), height);
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerWidth(int32_t aInnerWidth)
{
  ErrorResult rv;
  SetInnerWidth(aInnerWidth, rv);

  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::GetInnerHeight(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetInnerHeight, (aError), aError, 0);

  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.height;
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerHeight(int32_t* aInnerHeight)
{
  ErrorResult rv;
  *aInnerHeight = GetInnerHeight(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetInnerHeight(int32_t aInnerHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerHeight, (aInnerHeight, aError), aError, );

  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.innerHeight by exiting early
   */
  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsRefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nsRefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    nscoord height = aInnerHeight;
    nscoord width = shellArea.width;
    CheckSecurityWidthAndHeight(nullptr, &height);
    SetCSSViewportWidthAndHeight(width,
                                 nsPresContext::CSSPixelsToAppUnits(height));
    return;
  }

  int32_t height = 0;
  int32_t width  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&width, &height);
  CheckSecurityWidthAndHeight(nullptr, &aInnerHeight);
  aError = SetDocShellWidthAndHeight(width, CSSToDevIntPixels(aInnerHeight));
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerHeight(int32_t aInnerHeight)
{
  ErrorResult rv;
  SetInnerHeight(aInnerHeight, rv);

  return rv.ErrorCode();
}

nsIntSize
nsGlobalWindow::GetOuterSize(ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return nsIntSize(0, 0);
  }

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow *>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  nsIntSize sizeDevPixels;
  aError = treeOwnerAsWin->GetSize(&sizeDevPixels.width, &sizeDevPixels.height);
  if (aError.Failed()) {
    return nsIntSize();
  }

  return DevToCSSIntPixels(sizeDevPixels);
}

int32_t
nsGlobalWindow::GetOuterWidth(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterWidth, (aError), aError, 0);
  return GetOuterSize(aError).width;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterWidth(int32_t* aOuterWidth)
{
  ErrorResult rv;
  *aOuterWidth = GetOuterWidth(rv);

  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::GetOuterHeight(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterHeight, (aError), aError, 0);
  return GetOuterSize(aError).height;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterHeight(int32_t* aOuterHeight)
{
  ErrorResult rv;
  *aOuterHeight = GetOuterHeight(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth,
                             ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.outerWidth by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  CheckSecurityWidthAndHeight(aIsWidth ? &aLengthCSSPixels : nullptr,
                              aIsWidth ? nullptr : &aLengthCSSPixels);

  int32_t width, height;
  aError = treeOwnerAsWin->GetSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  int32_t lengthDevPixels = CSSToDevIntPixels(aLengthCSSPixels);
  if (aIsWidth) {
    width = lengthDevPixels;
  } else {
    height = lengthDevPixels;
  }
  aError = treeOwnerAsWin->SetSize(width, height, true);    
}

void
nsGlobalWindow::SetOuterWidth(int32_t aOuterWidth, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterWidth, (aOuterWidth, aError), aError, );

  SetOuterSize(aOuterWidth, true, aError);
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterWidth(int32_t aOuterWidth)
{
  ErrorResult rv;
  SetOuterWidth(aOuterWidth, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetOuterHeight(int32_t aOuterHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterHeight, (aOuterHeight, aError), aError, );

  SetOuterSize(aOuterHeight, false, aError);
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterHeight(int32_t aOuterHeight)
{
  ErrorResult rv;
  SetOuterHeight(aOuterHeight, rv);

  return rv.ErrorCode();
}

nsIntPoint
nsGlobalWindow::GetScreenXY(ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return nsIntPoint(0, 0);
  }

  int32_t x = 0, y = 0;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  return nsIntPoint(x, y);
}

int32_t
nsGlobalWindow::GetScreenX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenX, (aError), aError, 0);

  return DevToCSSIntPixels(GetScreenXY(aError).x);
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenX(int32_t* aScreenX)
{
  ErrorResult rv;
  *aScreenX = GetScreenX(rv);

  return rv.ErrorCode();
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nsRect();
  }

  nsGlobalWindow* rootWindow =
    static_cast<nsGlobalWindow*>(GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  if (!mDocShell) {
    return nsRect();
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    return nsRect();
  }
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return nsRect();
  }

  return rootFrame->GetScreenRectInAppUnits();
}

float
nsGlobalWindow::GetMozInnerScreenX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenX, (aError), aError, 0);

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.x);
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenX(float* aScreenX)
{
  ErrorResult rv;
  *aScreenX = GetMozInnerScreenX(rv);

  return rv.ErrorCode();
}

float
nsGlobalWindow::GetMozInnerScreenY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenY, (aError), aError, 0);

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.y);
}

NS_IMETHODIMP
nsGlobalWindow::GetMozInnerScreenY(float* aScreenY)
{
  ErrorResult rv;
  *aScreenY = GetMozInnerScreenY(rv);

  return rv.ErrorCode();
}

float
nsGlobalWindow::GetDevicePixelRatio(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDevicePixelRatio, (aError), aError, 0.0);

  if (!mDocShell) {
    return 1.0;
  }

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return 1.0;
  }

  return float(nsPresContext::AppUnitsPerCSSPixel())/
      presContext->AppUnitsPerDevPixel();
}

NS_IMETHODIMP
nsGlobalWindow::GetDevicePixelRatio(float* aRatio)
{
  ErrorResult rv;
  *aRatio = GetDevicePixelRatio(rv);

  return rv.ErrorCode();
}

uint64_t
nsGlobalWindow::GetMozPaintCount(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozPaintCount, (aError), aError, 0);

  if (!mDocShell) {
    return 0;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  return presShell ? presShell->GetPaintCount() : 0;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozPaintCount(uint64_t* aResult)
{
  ErrorResult rv;
  *aResult = GetMozPaintCount(rv);

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::MozRequestAnimationFrame(nsIFrameRequestCallback* aCallback,
                                         int32_t *aHandle)
{
  if (!aCallback) {
    if (mDoc) {
      mDoc->WarnOnceAbout(nsIDocument::eMozBeforePaint);
    }
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  ErrorResult rv;
  nsIDocument::FrameRequestCallbackHolder holder(aCallback);
  *aHandle = RequestAnimationFrame(holder, rv);

  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::RequestAnimationFrame(FrameRequestCallback& aCallback,
                                      ErrorResult& aError)
{
  nsIDocument::FrameRequestCallbackHolder holder(&aCallback);
  return RequestAnimationFrame(holder, aError);
}

int32_t
nsGlobalWindow::MozRequestAnimationFrame(nsIFrameRequestCallback* aCallback,
                                         ErrorResult& aError)
{
  nsIDocument::FrameRequestCallbackHolder holder(aCallback);
  return RequestAnimationFrame(holder, aError);
}

int32_t
nsGlobalWindow::RequestAnimationFrame(const nsIDocument::FrameRequestCallbackHolder& aCallback,
                                      ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(RequestAnimationFrame, (aCallback, aError), aError,
                            0);

  if (!mDoc) {
    return 0;
  }

  if (GetWrapperPreserveColor()) {
    js::NotifyAnimationActivity(GetWrapperPreserveColor());
  }

  int32_t handle;
  aError = mDoc->ScheduleFrameRequestCallback(aCallback, &handle);
  return handle;
}

NS_IMETHODIMP
nsGlobalWindow::RequestAnimationFrame(JS::Handle<JS::Value> aCallback,
                                      JSContext* cx,
                                      int32_t* aHandle)
{
  if (!aCallback.isObject() || !JS_ObjectIsCallable(cx, &aCallback.toObject())) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> callbackObj(cx, &aCallback.toObject());
  nsRefPtr<FrameRequestCallback> callback =
    new FrameRequestCallback(callbackObj, GetIncumbentGlobal());

  ErrorResult rv;
  *aHandle = RequestAnimationFrame(*callback, rv);

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::MozCancelRequestAnimationFrame(int32_t aHandle)
{
  return CancelAnimationFrame(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::MozCancelAnimationFrame(int32_t aHandle)
{
  return CancelAnimationFrame(aHandle);
}

void
nsGlobalWindow::CancelAnimationFrame(int32_t aHandle, ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(CancelAnimationFrame, (aHandle, aError), aError, );

  if (!mDoc) {
    return;
  }

  mDoc->CancelFrameRequestCallback(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::CancelAnimationFrame(int32_t aHandle)
{
  ErrorResult rv;
  CancelAnimationFrame(aHandle, rv);

  return rv.ErrorCode();
}

int64_t
nsGlobalWindow::GetMozAnimationStartTime(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetMozAnimationStartTime, (aError), aError, 0);

  if (mDoc) {
    nsIPresShell* presShell = mDoc->GetShell();
    if (presShell) {
      return presShell->GetPresContext()->RefreshDriver()->
        MostRecentRefreshEpochTime() / PR_USEC_PER_MSEC;
    }
  }

  // If all else fails, just be compatible with Date.now()
  return JS_Now() / PR_USEC_PER_MSEC;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozAnimationStartTime(int64_t *aTime)
{
  ErrorResult rv;
  *aTime = GetMozAnimationStartTime(rv);

  return rv.ErrorCode();
}

already_AddRefed<MediaQueryList>
nsGlobalWindow::MatchMedia(const nsAString& aMediaQueryList,
                           ErrorResult& aError)
{
  // FIXME: This whole forward-to-outer and then get a pres
  // shell/context off the docshell dance is sort of silly; it'd make
  // more sense to forward to the inner, but it's what everyone else
  // (GetSelection, GetScrollXY, etc.) does around here.
  FORWARD_TO_OUTER_OR_THROW(MatchMedia, (aMediaQueryList, aError), aError,
                            nullptr);

  // We need this now to ensure that we have a non-null |presContext|
  // when we ought to.
  // This is similar to EnsureSizeUpToDate, but only flushes frames.
  nsGlobalWindow *parent = static_cast<nsGlobalWindow*>(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Frames);
  }

  if (!mDocShell) {
    return nullptr;
  }

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  if (!presContext) {
    return nullptr;
  }

  return presContext->MatchMedia(aMediaQueryList);
}

NS_IMETHODIMP
nsGlobalWindow::MatchMedia(const nsAString& aMediaQueryList,
                           nsISupports** aResult)
{
  ErrorResult rv;
  nsRefPtr<MediaQueryList> mediaQueryList = MatchMedia(aMediaQueryList, rv);
  mediaQueryList.forget(aResult);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetScreenX(int32_t aScreenX, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenX, (aScreenX, aError), aError, );

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenX by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  CheckSecurityLeftAndTop(&aScreenX, nullptr);
  x = CSSToDevIntPixels(aScreenX);

  aError = treeOwnerAsWin->SetPosition(x, y);
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenX(int32_t aScreenX)
{
  ErrorResult rv;
  SetScreenX(aScreenX, rv);

  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::GetScreenY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenY, (aError), aError, 0);

  return DevToCSSIntPixels(GetScreenXY(aError).y);
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenY(int32_t* aScreenY)
{
  ErrorResult rv;
  *aScreenY = GetScreenY(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SetScreenY(int32_t aScreenY, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenY, (aScreenY, aError), aError, );

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent setting window.screenY by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  CheckSecurityLeftAndTop(nullptr, &aScreenY);
  y = CSSToDevIntPixels(aScreenY);

  aError = treeOwnerAsWin->SetPosition(x, y);
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenY(int32_t aScreenY)
{
  ErrorResult rv;
  SetScreenY(aScreenY, rv);

  return rv.ErrorCode();
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindow::CheckSecurityWidthAndHeight(int32_t* aWidth, int32_t* aHeight)
{
  MOZ_ASSERT(IsOuterWindow());

#ifdef MOZ_XUL
  if (!nsContentUtils::IsCallerChrome()) {
    // if attempting to resize the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
  }
#endif

  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (!nsContentUtils::IsCallerChrome()) {
      //sec check failed
      if (aWidth && *aWidth < 100) {
        *aWidth = 100;
      }
      if (aHeight && *aHeight < 100) {
        *aHeight = 100;
      }
    }
  }
}

// NOTE: Arguments to this function should have values in device pixels
nsresult
nsGlobalWindow::SetDocShellWidthAndHeight(int32_t aInnerWidth, int32_t aInnerHeight)
{
  MOZ_ASSERT(IsOuterWindow());

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(mDocShell, aInnerWidth, aInnerHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

// NOTE: Arguments to this function should have values in app units
void
nsGlobalWindow::SetCSSViewportWidthAndHeight(nscoord aInnerWidth, nscoord aInnerHeight)
{
  MOZ_ASSERT(IsOuterWindow());

  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  nsRect shellArea = presContext->GetVisibleArea();
  shellArea.height = aInnerHeight;
  shellArea.width = aInnerWidth;

  presContext->SetVisibleArea(shellArea);
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindow::CheckSecurityLeftAndTop(int32_t* aLeft, int32_t* aTop)
{
  MOZ_ASSERT(IsOuterWindow());

  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!nsContentUtils::IsCallerChrome()) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
#endif

    nsGlobalWindow* rootWindow =
      static_cast<nsGlobalWindow*>(GetPrivateRoot());
    if (rootWindow) {
      rootWindow->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIBaseWindow> treeOwner = GetTreeOwnerWindow();

    nsCOMPtr<nsIDOMScreen> screen;
    GetScreen(getter_AddRefs(screen));

    if (treeOwner && screen) {
      int32_t screenLeft, screenTop, screenWidth, screenHeight;
      int32_t winLeft, winTop, winWidth, winHeight;

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
}

NS_IMETHODIMP
nsGlobalWindow::GetPageXOffset(int32_t* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP
nsGlobalWindow::GetPageYOffset(int32_t* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

void
nsGlobalWindow::GetScrollMaxXY(int32_t* aScrollMaxX, int32_t* aScrollMaxY,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollMaxXY, (aScrollMaxX, aScrollMaxY, aError),
                            aError, );

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf) {
    return;
  }

  nsRect scrollRange = sf->GetScrollRange();

  if (aScrollMaxX) {
    *aScrollMaxX = std::max(0,
      (int32_t)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.XMost())));
  }
  if (aScrollMaxY) {
    *aScrollMaxY = std::max(0,
      (int32_t)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.YMost())));
  }
}

int32_t
nsGlobalWindow::GetScrollMaxX(ErrorResult& aError)
{
  int32_t scrollMaxX = 0;
  GetScrollMaxXY(&scrollMaxX, nullptr, aError);
  return scrollMaxX;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxX(int32_t* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  ErrorResult rv;
  *aScrollMaxX = GetScrollMaxX(rv);

  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::GetScrollMaxY(ErrorResult& aError)
{
  int32_t scrollMaxY = 0;
  GetScrollMaxXY(nullptr, &scrollMaxY, aError);
  return scrollMaxY;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxY(int32_t* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  ErrorResult rv;
  *aScrollMaxY = GetScrollMaxY(rv);

  return rv.ErrorCode();
}

CSSIntPoint
nsGlobalWindow::GetScrollXY(bool aDoFlush, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollXY, (aDoFlush, aError), aError,
                            CSSIntPoint(0, 0));

  if (aDoFlush) {
    FlushPendingNotifications(Flush_Layout);
  } else {
    EnsureSizeUpToDate();
  }

  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf) {
    return CSSIntPoint(0, 0);
  }

  nsPoint scrollPos = sf->GetScrollPosition();
  if (scrollPos != nsPoint(0,0) && !aDoFlush) {
    // Oh, well.  This is the expensive case -- the window is scrolled and we
    // didn't actually flush yet.  Repeat, but with a flush, since the content
    // may get shorter and hence our scroll position may decrease.
    return GetScrollXY(true, aError);
  }

  return sf->GetScrollPositionCSSPixels();
}

int32_t
nsGlobalWindow::GetScrollX(ErrorResult& aError)
{
  return GetScrollXY(false, aError).x;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollX(int32_t* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  ErrorResult rv;
  *aScrollX = GetScrollXY(false, rv).x;
  return rv.ErrorCode();
}

int32_t
nsGlobalWindow::GetScrollY(ErrorResult& aError)
{
  return GetScrollXY(false, aError).y;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollY(int32_t* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  ErrorResult rv;
  *aScrollY = GetScrollXY(false, rv).y;
  return rv.ErrorCode();
}

uint32_t
nsGlobalWindow::Length()
{
  FORWARD_TO_OUTER(Length, (), 0);

  nsDOMWindowList* windows = GetWindowList();

  return windows ? windows->GetLength() : 0;
}

NS_IMETHODIMP
nsGlobalWindow::GetLength(uint32_t* aLength)
{
  *aLength = Length();
  return NS_OK;
}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::GetChildWindow(const nsAString& aName)
{
  nsCOMPtr<nsIDocShell> docShell(GetDocShell());
  NS_ENSURE_TRUE(docShell, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> child;
  docShell->FindChildWithName(PromiseFlatString(aName).get(),
                              false, true, nullptr, nullptr,
                              getter_AddRefs(child));

  nsCOMPtr<nsIDOMWindow> child_win(do_GetInterface(child));
  return child_win.forget();
}

bool
nsGlobalWindow::DispatchCustomEvent(const char *aEventName)
{
  bool defaultActionEnabled = true;
  nsContentUtils::DispatchTrustedEvent(mDoc,
                                       GetOuterWindow(),
                                       NS_ConvertASCIItoUTF16(aEventName),
                                       true, true, &defaultActionEnabled);

  return defaultActionEnabled;
}

// NOTE: Arguments to this function should be CSS pixels, not device pixels.
bool
nsGlobalWindow::DispatchResizeEvent(const nsIntSize& aSize)
{
  ErrorResult res;
  nsRefPtr<Event> domEvent =
    mDoc->CreateEvent(NS_LITERAL_STRING("CustomEvent"), res);
  if (res.Failed()) {
    return false;
  }

  AutoSafeJSContext cx;
  JSAutoCompartment ac(cx, GetWrapperPreserveColor());
  DOMWindowResizeEventDetail detail;
  detail.mWidth = aSize.width;
  detail.mHeight = aSize.height;
  JS::Rooted<JS::Value> detailValue(cx);
  if (!detail.ToObject(cx, &detailValue)) {
    return false;
  }

  CustomEvent* customEvent = static_cast<CustomEvent*>(domEvent.get());
  customEvent->InitCustomEvent(cx,
                               NS_LITERAL_STRING("DOMWindowResize"),
                               /* bubbles = */ true,
                               /* cancelable = */ true,
                               detailValue,
                               res);
  if (res.Failed()) {
    return false;
  }

  domEvent->SetTrusted(true);
  domEvent->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOuterWindow());
  domEvent->SetTarget(target);

  bool defaultActionEnabled = true;
  target->DispatchEvent(domEvent, &defaultActionEnabled);

  return defaultActionEnabled;
}

void
nsGlobalWindow::RefreshCompartmentPrincipal()
{
  FORWARD_TO_INNER(RefreshCompartmentPrincipal, (), /* void */ );

  JS_SetCompartmentPrincipals(js::GetObjectCompartment(GetWrapperPreserveColor()),
                              nsJSPrincipals::get(mDoc->NodePrincipal()));
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsCOMPtr<nsIDocShellTreeItem> callerItem;

  if (cx) {
    nsCOMPtr<nsIWebNavigation> callerWebNav =
      do_GetInterface(nsJSUtils::GetDynamicScriptGlobal(cx));

    callerItem = do_QueryInterface(callerWebNav);
  }

  return callerItem.forget();
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

  if (!caller) {
    caller = mDocShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  mDocShell->FindItemWithName(PromiseFlatString(aName).get(), nullptr, caller,
                              getter_AddRefs(namedItem));
  return namedItem != nullptr;
}

already_AddRefed<nsIWidget>
nsGlobalWindow::GetMainWidget()
{
  FORWARD_TO_OUTER(GetMainWidget, (), nullptr);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();

  nsCOMPtr<nsIWidget> widget;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
  }

  return widget.forget();
}

nsIWidget*
nsGlobalWindow::GetNearestWidget()
{
  nsIDocShell* docShell = GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsIFrame* rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);
  return rootFrame->GetView()->GetNearestWidget(nullptr);
}

void
nsGlobalWindow::SetFullScreen(bool aFullScreen, mozilla::ErrorResult& aError)
{
  aError = SetFullScreenInternal(aFullScreen, true);
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
      (aRequireTrust && !nsContentUtils::IsCallerChrome())) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(rootItem);
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != mDocShell)
    return window->SetFullScreenInternal(aFullScreen, aRequireTrust);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeChrome)
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
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwnerAsWin));
  if (aFullScreen && xulWin) {
    xulWin->SetIntrinsicallySized(false);
  }

  // Set this before so if widget sends an event indicating its
  // gone full screen, the state trap above works.
  mFullScreen = aFullScreen;

  // Sometimes we don't want the top-level widget to actually go fullscreen,
  // for example in the B2G desktop client, we don't want the emulated screen
  // dimensions to appear to increase when entering fullscreen mode; we just
  // want the content to fill the entire client area of the emulator window.
  if (!Preferences::GetBool("full-screen-api.ignore-widgets", false)) {
    nsCOMPtr<nsIWidget> widget = GetMainWidget();
    if (widget)
      widget->MakeFullScreen(aFullScreen);
  }

  if (!mFullScreen) {
    // Force exit from DOM full-screen mode. This is so that if we're in
    // DOM full-screen mode and the user exits full-screen mode with
    // the browser full-screen mode toggle keyboard-shortcut, we'll detect
    // that and leave DOM API full-screen mode too.
    nsIDocument::ExitFullscreen(mDoc, /* async */ false);
  }

  if (!mWakeLock && mFullScreen) {
    nsRefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE(pmService, NS_OK);

    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("DOM_Fullscreen"),
                                       this, rv);
    if (rv.Failed()) {
      return rv.ErrorCode();
    }

  } else if (mWakeLock && !mFullScreen) {
    ErrorResult rv;
    mWakeLock->Unlock(rv);
    NS_WARN_IF_FALSE(!rv.Failed(), "Failed to unlock the wakelock.");
    mWakeLock = nullptr;
  }

  return NS_OK;
}

bool
nsGlobalWindow::GetFullScreen(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFullScreen, (aError), aError, false);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  if (mDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
    if (rootItem != mDocShell) {
      nsCOMPtr<nsIDOMWindow> window = do_GetInterface(rootItem);
      if (window) {
        bool fullScreen = false;
        aError = window->GetFullScreen(&fullScreen);
        return fullScreen;
      }
    }
  }

  // We are the root window, or something went wrong. Return our internal value.
  return mFullScreen;
}

NS_IMETHODIMP
nsGlobalWindow::GetFullScreen(bool* aFullScreen)
{
  ErrorResult rv;
  *aFullScreen = GetFullScreen(rv);

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::Dump(const nsAString& aStr)
{
  if (!nsContentUtils::DOMWindowDumpEnabled()) {
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
#ifdef XP_WIN
    PrintToDebugger(cstr);
#endif
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
  MOZ_ASSERT(IsOuterWindow());
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  if (!mDocShell)
    return;

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

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
            nsAutoCString host;
            fixedURI->GetHost(host);

            if (!host.IsEmpty()) {
              // if this URI has a host we'll show it. For other
              // schemes (e.g. file:) we fall back to the localized
              // generic string

              nsAutoCString prepath;
              fixedURI->GetPrePath(prepath);

              NS_ConvertUTF8toUTF16 ucsPrePath(prepath);
              const char16_t *formatStrings[] = { ucsPrePath.get() };
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
  MOZ_ASSERT(IsOuterWindow());

  // When called from chrome, we can avoid the following checks.
  if (!nsContentUtils::IsCallerChrome()) {
    // Don't allow scripts to move or resize windows that were not opened by a
    // script.
    if (!mHadOriginalOpener) {
      return false;
    }

    if (!CanSetProperty("dom.disable_window_move_resize")) {
      return false;
    }

    // Ignore the request if we have more than one tab in the window.
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
    if (treeOwner) {
      uint32_t itemCount;
      if (NS_SUCCEEDED(treeOwner->GetTargetableShellCount(&itemCount)) &&
          itemCount > 1) {
        return false;
      }
    }
  }

  // The preference is useful for the webapp runtime. Webapps should be able
  // to resize or move their window.
  if (mDocShell && !Preferences::GetBool("dom.always_allow_move_resize_window",
                                         false)) {
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

bool
nsGlobalWindow::AlertOrConfirm(bool aAlert,
                               const nsAString& aMessage,
                               mozilla::ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindow::Prompt, make
  // sure any modifications here don't need to happen over there!
  MOZ_ASSERT(IsOuterWindow());

  if (!AreDialogsEnabled()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

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
  nsAutoString final;
  nsContentUtils::StripNullChars(aMessage, final);

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIPrompt> prompt;
  aError = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                                getter_AddRefs(prompt));
  if (aError.Failed()) {
    return false;
  }

  // Always allow tab modal prompts for alert and confirm.
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt)) {
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), true);
  }

  bool result = false;
  nsAutoSyncOperation sync(mDoc);
  if (ShouldPromptToBlockDialogs()) {
    bool disallowDialog = false;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    aError = aAlert ?
               prompt->AlertCheck(title.get(), final.get(), label.get(),
                                  &disallowDialog) :
               prompt->ConfirmCheck(title.get(), final.get(), label.get(),
                                    &disallowDialog, &result);

    if (disallowDialog)
      DisableDialogs();
  } else {
    aError = aAlert ?
               prompt->Alert(title.get(), final.get()) :
               prompt->Confirm(title.get(), final.get(), &result);
  }

  return result;
}

void
nsGlobalWindow::Alert(const nsAString& aMessage, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Alert, (aMessage, aError), aError, );
  AlertOrConfirm(/* aAlert = */ true, aMessage, aError);
}

NS_IMETHODIMP
nsGlobalWindow::Alert(const nsAString& aString)
{
  ErrorResult rv;
  Alert(aString, rv);

  return rv.ErrorCode();
}

bool
nsGlobalWindow::Confirm(const nsAString& aMessage, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Confirm, (aMessage, aError), aError, false);

  return AlertOrConfirm(/* aAlert = */ false, aMessage, aError);
}

NS_IMETHODIMP
nsGlobalWindow::Confirm(const nsAString& aString, bool* aReturn)
{
  ErrorResult rv;
  *aReturn = Confirm(aString, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       nsAString& aReturn, ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindow::AlertOrConfirm, make
  // sure any modifications here don't need to happen over there!
  FORWARD_TO_OUTER_OR_THROW(Prompt, (aMessage, aInitial, aReturn, aError),
                            aError, );

  SetDOMStringToNull(aReturn);

  if (!AreDialogsEnabled()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

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

  nsresult rv;
  nsCOMPtr<nsIPromptFactory> promptFac =
    do_GetService("@mozilla.org/prompter;1", &rv);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  nsCOMPtr<nsIPrompt> prompt;
  aError = promptFac->GetPrompt(this, NS_GET_IID(nsIPrompt),
                                getter_AddRefs(prompt));
  if (aError.Failed()) {
    return;
  }

  // Always allow tab modal prompts for prompt.
  if (nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt)) {
    promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"), true);
  }

  // Pass in the default value, if any.
  char16_t *inoutValue = ToNewUnicode(fixedInitial);
  bool disallowDialog = false;

  nsXPIDLString label;
  if (ShouldPromptToBlockDialogs()) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);
  }

  nsAutoSyncOperation sync(mDoc);
  bool ok;
  aError = prompt->Prompt(title.get(), fixedMessage.get(),
                          &inoutValue, label.get(), &disallowDialog, &ok);

  if (disallowDialog) {
    DisableDialogs();
  }

  if (aError.Failed()) {
    return;
  }

  nsAdoptingString outValue(inoutValue);

  if (ok && outValue) {
    aReturn.Assign(outValue);
  }
}

NS_IMETHODIMP
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       nsAString& aReturn)
{
  ErrorResult rv;
  Prompt(aMessage, aInitial, aReturn, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Focus(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Focus, (aError), aError, );

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);

  bool isVisible = false;
  if (baseWin) {
    baseWin->GetVisibility(&isVisible);
  }

  if (!isVisible) {
    // A hidden tab is being focused, ignore this call.
    return;
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

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
  bool isActive = (rootWin == activeWindow);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (treeOwnerAsWin && (canFocus || isActive)) {
    bool isEnabled = true;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return;
    }

    // XXXndeakin not sure what this is for or if it should go somewhere else
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  if (!mDocShell) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  // Don't look for a presshell if we're a root chrome window that's got
  // about:blank loaded.  We don't want to focus our widget in that case.
  // XXXbz should we really be checking for IsInitialDocument() instead?
  bool lookForPresShell = true;
  if (mDocShell->ItemType() == nsIDocShellTreeItem::typeChrome &&
      GetPrivateRoot() == static_cast<nsIDOMWindow*>(this) &&
      mDoc) {
    nsIURI* ourURI = mDoc->GetDocumentURI();
    if (ourURI) {
      lookForPresShell = !NS_IsAboutBlank(ourURI);
    }
  }

  if (lookForPresShell) {
    mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
  }

  nsCOMPtr<nsIDocShellTreeItem> parentDsti;
  mDocShell->GetParent(getter_AddRefs(parentDsti));

  // set the parent's current focus to the frame containing this window.
  nsCOMPtr<nsPIDOMWindow> parent = do_GetInterface(parentDsti);
  if (parent) {
    nsCOMPtr<nsIDocument> parentdoc = parent->GetDoc();
    if (!parentdoc) {
      return;
    }

    nsIContent* frame = parentdoc->FindContentForSubDocument(mDoc);
    nsCOMPtr<nsIDOMElement> frameElement = do_QueryInterface(frame);
    if (frameElement) {
      uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
      if (canFocus)
        flags |= nsIFocusManager::FLAG_RAISE;
      aError = fm->SetFocus(frameElement, flags);
    }
    return;
  }
  if (nsCOMPtr<nsITabChild> child = do_GetInterface(mDocShell)) {
    child->SendRequestFocus(canFocus);
    return;
  }
  if (canFocus) {
    // if there is no parent, this must be a toplevel window, so raise the
    // window if canFocus is true
    aError = fm->SetActiveWindow(this);
  }
}

NS_IMETHODIMP
nsGlobalWindow::Focus()
{
  ErrorResult rv;
  Focus(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Blur(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Blur, (aError), aError, );

  // If dom.disable_window_flip == true, then content should not be allowed
  // to call this function (this would allow popunders, bug 369306)
  if (!CanSetProperty("dom.disable_window_flip")) {
    return;
  }

  // If embedding apps don't implement nsIEmbeddingSiteWindow, we
  // shouldn't throw exceptions to web content.

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nullptr.
    siteWindow->Blur();

    // if the root is focused, clear the focus
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm && mDoc) {
      nsCOMPtr<nsIDOMElement> element;
      fm->GetFocusedElementForWindow(this, false, nullptr, getter_AddRefs(element));
      nsCOMPtr<nsIContent> content = do_QueryInterface(element);
      if (content == mDoc->GetRootElement())
        fm->ClearFocus(this);
    }
  }
}

NS_IMETHODIMP
nsGlobalWindow::Blur()
{
  ErrorResult rv;
  Blur(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Back(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Back, (aError), aError, );

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoBack();
}

NS_IMETHODIMP
nsGlobalWindow::Back()
{
  ErrorResult rv;
  Back(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Forward(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Forward, (aError), aError, );

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoForward();
}

NS_IMETHODIMP
nsGlobalWindow::Forward()
{
  ErrorResult rv;
  Forward(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Home(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Home, (aError), aError, );

  if (!mDocShell) {
    return;
  }

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
    int32_t firstPipe = homeURL.FindChar('|');

    if (firstPipe > 0) {
      homeURL.Truncate(firstPipe);
    }
  }
#endif

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->LoadURI(homeURL.get(),
                           nsIWebNavigation::LOAD_FLAGS_NONE,
                           nullptr,
                           nullptr,
                           nullptr);
}

NS_IMETHODIMP
nsGlobalWindow::Home()
{
  ErrorResult rv;
  Home(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Stop(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Stop, (aError), aError, );

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (webNav) {
    aError = webNav->Stop(nsIWebNavigation::STOP_ALL);
  }
}

NS_IMETHODIMP
nsGlobalWindow::Stop()
{
  ErrorResult rv;
  Stop(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Print(ErrorResult& aError)
{
#ifdef NS_PRINTING
  FORWARD_TO_OUTER_OR_THROW(Print, (aError), aError, );

  if (Preferences::GetBool("dom.disable_window_print", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (!AreDialogsEnabled()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (ShouldPromptToBlockDialogs() && !ConfirmDialogIfNeeded()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {
    nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                               GetCurrentInnerWindowInternal()->mDoc :
                               nullptr);

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

      EnterModalState();
      webBrowserPrint->Print(printSettings, nullptr);
      LeaveModalState();

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
      webBrowserPrint->Print(printSettings, nullptr);
    }
  }
#endif //NS_PRINTING
}

NS_IMETHODIMP
nsGlobalWindow::Print()
{
  ErrorResult rv;
  Print(rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::MoveTo(int32_t aXPos, int32_t aYPos, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveTo, (aXPos, aYPos, aError), aError, );

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Mild abuse of a "size" object so we don't need more helper functions.
  nsIntSize cssPos(aXPos, aYPos);
  CheckSecurityLeftAndTop(&cssPos.width, &cssPos.height);

  nsIntSize devPos = CSSToDevIntPixels(cssPos);

  aError = treeOwnerAsWin->SetPosition(devPos.width, devPos.height);
}

NS_IMETHODIMP
nsGlobalWindow::MoveTo(int32_t aXPos, int32_t aYPos)
{
  ErrorResult rv;
  MoveTo(aXPos, aYPos, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveBy, (aXDif, aYDif, aError), aError, );

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // To do this correctly we have to convert what we get from GetPosition
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetPosition.

  int32_t x, y;
  aError = treeOwnerAsWin->GetPosition(&x, &y);
  if (aError.Failed()) {
    return;
  }

  // mild abuse of a "size" object so we don't need more helper functions
  nsIntSize cssPos(DevToCSSIntPixels(nsIntSize(x, y)));

  cssPos.width += aXDif;
  cssPos.height += aYDif;
  
  CheckSecurityLeftAndTop(&cssPos.width, &cssPos.height);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  aError = treeOwnerAsWin->SetPosition(newDevPos.width, newDevPos.height);
}

NS_IMETHODIMP
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif)
{
  ErrorResult rv;
  MoveBy(aXDif, aYDif, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::ResizeTo(int32_t aWidth, int32_t aHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeTo, (aWidth, aHeight, aError), aError, );

  /*
   * If caller is a browser-element then dispatch a resize event to
   * the embedder.
   */
  if (mDocShell && mDocShell->GetIsBrowserOrApp()) {
    nsIntSize size(aWidth, aHeight);
    if (!DispatchResizeEvent(size)) {
      // The embedder chose to prevent the default action for this
      // event, so let's not resize this window after all...
      return;
    }
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeTo() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  
  nsIntSize cssSize(aWidth, aHeight);
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height);

  nsIntSize devSz(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(devSz.width, devSz.height, true);
}

NS_IMETHODIMP
nsGlobalWindow::ResizeTo(int32_t aWidth, int32_t aHeight)
{
  ErrorResult rv;
  ResizeTo(aWidth, aHeight, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                         ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeBy, (aWidthDif, aHeightDif, aError), aError, );

  /*
   * If caller is a browser-element then dispatch a resize event to
   * parent.
   */
  if (mDocShell && mDocShell->GetIsBrowserOrApp()) {
    CSSIntSize size;
    if (NS_FAILED(GetInnerSize(size))) {
      return;
    }

    size.width += aWidthDif;
    size.height += aHeightDif;

    if (!DispatchResizeEvent(nsIntSize(size.width, size.height))) {
      // The embedder chose to prevent the default action for this
      // event, so let's not resize this window after all...
      return;
    }
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t width, height;
  aError = treeOwnerAsWin->GetSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  // To do this correctly we have to convert what we got from GetSize
  // into CSS pixels, add the arguments, do the security check, and
  // then convert back to device pixels for the call to SetSize.

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));

  cssSize.width += aWidthDif;
  cssSize.height += aHeightDif;

  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(newDevSize.width, newDevSize.height, true);
}

NS_IMETHODIMP
nsGlobalWindow::ResizeBy(int32_t aWidthDif, int32_t aHeightDif)
{
  ErrorResult rv;
  ResizeBy(aWidthDif, aHeightDif, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::SizeToContent(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SizeToContent, (aError), aError, );

  if (!mDocShell) {
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows() || IsFrame()) {
    return;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  if (!markupViewer) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t width, height;
  aError = markupViewer->GetContentSize(&width, &height);
  if (aError.Failed()) {
    return;
  }

  // Make sure the new size is following the CheckSecurityWidthAndHeight
  // rules.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
  if (!treeOwner) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwner->SizeShellTo(mDocShell, newDevSize.width,
                                  newDevSize.height);
}

NS_IMETHODIMP
nsGlobalWindow::SizeToContent()
{
  ErrorResult rv;
  SizeToContent(rv);

  return rv.ErrorCode();
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
  nsPIDOMWindow* piWin = GetPrivateRoot();
  if (!piWin) {
    return nullptr;
  }

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

NS_IMETHODIMP
nsGlobalWindow::Scroll(int32_t aXScroll, int32_t aYScroll)
{
  ScrollTo(CSSIntPoint(aXScroll, aYScroll));
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollTo(int32_t aXScroll, int32_t aYScroll)
{
  ScrollTo(CSSIntPoint(aXScroll, aYScroll));
  return NS_OK;
}

void
nsGlobalWindow::ScrollTo(const CSSIntPoint& aScroll)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and subtracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const int32_t maxpx = nsPresContext::AppUnitsToIntCSSPixels(0x7fffffff) - 4;

    CSSIntPoint scroll(aScroll);
    if (scroll.x > maxpx) {
      scroll.x = maxpx;
    }

    if (scroll.y > maxpx) {
      scroll.y = maxpx;
    }
    sf->ScrollToCSSPixels(scroll);
  }
}

NS_IMETHODIMP
nsGlobalWindow::ScrollBy(int32_t aXScrollDif, int32_t aYScrollDif)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    CSSIntPoint scrollPos =
      sf->GetScrollPositionCSSPixels() + CSSIntPoint(aXScrollDif, aYScrollDif);
    // It seems like it would make more sense for ScrollBy to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    ScrollTo(scrollPos);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByLines(int32_t numLines)
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
nsGlobalWindow::ScrollByPages(int32_t numPages)
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

void
nsGlobalWindow::ClearTimeout(int32_t aHandle, ErrorResult& aError)
{
  if (aHandle > 0) {
    ClearTimeoutOrInterval(aHandle, aError);
  }
}

NS_IMETHODIMP
nsGlobalWindow::ClearTimeout(int32_t aHandle)
{
  ErrorResult rv;
  ClearTimeout(aHandle, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::ClearInterval(int32_t aHandle, ErrorResult& aError)
{
  if (aHandle > 0) {
    ClearTimeoutOrInterval(aHandle, aError);
  }
}

NS_IMETHODIMP
nsGlobalWindow::ClearInterval(int32_t aHandle)
{
  ErrorResult rv;
  ClearInterval(aHandle, rv);

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::SetTimeout(int32_t *_retval)
{
  return SetTimeoutOrInterval(false, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetInterval(int32_t *_retval)
{
  return SetTimeoutOrInterval(true, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetResizable(bool aResizable)
{
  // nop

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::CaptureEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfCaptureEvents);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ReleaseEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfReleaseEvents);
  }

  return NS_OK;
}

static
bool IsPopupBlocked(nsIDocument* aDoc)
{
  nsCOMPtr<nsIPopupWindowManager> pm =
    do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);

  if (!pm) {
    return false;
  }

  if (!aDoc) {
    return true;
  }

  uint32_t permission = nsIPopupWindowManager::ALLOW_POPUP;
  pm->TestPermission(aDoc->NodePrincipal(), &permission);
  return permission == nsIPopupWindowManager::DENY_POPUP;
}

/* static */
void
nsGlobalWindow::FirePopupBlockedEvent(nsIDocument* aDoc,
                                      nsIDOMWindow *aRequestingWindow, nsIURI *aPopupURI,
                                      const nsAString &aPopupWindowName,
                                      const nsAString &aPopupWindowFeatures)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about
    // blocked popups.
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDoc);
    nsCOMPtr<nsIDOMEvent> event;
    doc->CreateEvent(NS_LITERAL_STRING("PopupBlockedEvents"), getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMPopupBlockedEvent> pbev(do_QueryInterface(event));
      pbev->InitPopupBlockedEvent(NS_LITERAL_STRING("DOMPopupBlocked"),
                                  true, true, aRequestingWindow,
                                  aPopupURI, aPopupWindowName,
                                  aPopupWindowFeatures);
      event->SetTrusted(true);

      bool defaultActionEnabled;
      aDoc->DispatchEvent(event, &defaultActionEnabled);
    }
  }
}

static void FirePopupWindowEvent(nsIDocument* aDoc)
{
  // Fire a "PopupWindow" event
  nsContentUtils::DispatchTrustedEvent(aDoc, aDoc,
                                       NS_LITERAL_STRING("PopupWindow"),
                                       true, true);
}

// static
bool
nsGlobalWindow::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::IsCallerChrome()) {
    return true;
  }

  // If the pref is set to true, we can not set the property
  // and vice versa.
  return !Preferences::GetBool(aPrefName, true);
}

bool
nsGlobalWindow::PopupWhitelisted()
{
  if (!IsPopupBlocked(mDoc))
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
  MOZ_ASSERT(IsOuterWindow());

  NS_ASSERTION(mDocShell, "Must have docshell");
  
  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeContent) {
    return openAllowed;
  }

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
    int32_t popupMax = Preferences::GetInt("dom.popup_maximum", -1);
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
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(topWindow);
  if (!window) {
    return;
  }

  nsCOMPtr<nsIDocument> topDoc = window->GetDoc();
  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = nullptr;

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsCOMPtr<nsPIDOMWindow> contextWindow;

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);
    if (currentCX) {
      contextWindow = do_QueryInterface(currentCX->GetGlobalObject());
    }
  }
  if (!contextWindow) {
    contextWindow = this;
  }

  nsCOMPtr<nsIDocument> doc = contextWindow->GetDoc();
  if (doc)
    baseURL = doc->GetDocBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  if (aBlocked) {
    FirePopupBlockedEvent(topDoc, this, popupURI, aPopupWindowName,
                          aPopupWindowFeatures);
  }
  if (aWindow)
    FirePopupWindowEvent(topDoc);
}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, ErrorResult& aError)
{
  nsCOMPtr<nsIDOMWindow> window;
  aError = OpenJS(aUrl, aName, aOptions, getter_AddRefs(window));
  return window.forget();
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
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nullptr,           // aJSCallerContext
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
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
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
                      true,                    // aNavigate
                      nullptr, aExtraArgument,    // Arguments
                      GetPrincipal(),             // aCalleePrincipal
                      nullptr,                    // aJSCallerContext
                      _retval);
}

// Like Open, but passes aNavigate=false.
/* virtual */ nsresult
nsGlobalWindow::OpenNoNavigate(const nsAString& aUrl,
                               const nsAString& aName,
                               const nsAString& aOptions,
                               nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      false,          // aNavigate
                      nullptr, nullptr,  // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nullptr,           // aJSCallerContext
                      _retval);

}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::OpenDialog(JSContext* aCx, const nsAString& aUrl,
                           const nsAString& aName, const nsAString& aOptions,
                           const Sequence<JS::Value>& aExtraArgument,
                           ErrorResult& aError)
{
  nsCOMPtr<nsIJSArgArray> argvArray;
  aError = NS_CreateJSArgv(aCx, aExtraArgument.Length(),
                           const_cast<JS::Value*>(aExtraArgument.Elements()),
                           getter_AddRefs(argvArray));
  if (aError.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMWindow> dialog;
  aError = OpenInternal(aUrl, aName, aOptions,
                        true,             // aDialog
                        false,            // aContentModal
                        false,            // aCalledNoScript
                        false,            // aDoJSFixups
                        true,                // aNavigate
                        argvArray, nullptr,  // Arguments
                        GetPrincipal(),      // aCalleePrincipal
                        aCx,                 // aJSCallerContext
                        getter_AddRefs(dialog));
  return dialog.forget();
}

NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions, nsIDOMWindow** _retval)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAXPCNativeCallContext *ncc = nullptr;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nullptr;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t argc;
  JS::Value *argv = nullptr;

  // XXX - need to get this as nsISupports?
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // Strip the url, name and options from the args seen by scripts.
  uint32_t argOffset = argc < 3 ? argc : 3;
  nsCOMPtr<nsIJSArgArray> argvArray;
  rv = NS_CreateJSArgv(cx, argc - argOffset, argv + argOffset,
                       getter_AddRefs(argvArray));
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenInternal(aUrl, aName, aOptions,
                      true,             // aDialog
                      false,            // aContentModal
                      false,            // aCalledNoScript
                      false,            // aDoJSFixups
                      true,                // aNavigate
                      argvArray, nullptr,  // Arguments
                      GetPrincipal(),      // aCalleePrincipal
                      cx,                  // aJSCallerContext
                      _retval);
}

already_AddRefed<nsIDOMWindow>
nsGlobalWindow::GetFrames(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFrames, (aError), aError, nullptr);

  nsRefPtr<nsGlobalWindow> frames(this);
  FlushPendingNotifications(Flush_ContentAndNotify);
  return frames.forget();
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindow** aFrames)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMWindow> frames = GetFrames(rv);
  frames.forget(aFrames);

  return rv.ErrorCode();
}

JSObject* nsGlobalWindow::CallerGlobal()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    NS_ERROR("Please don't call this method from C++!");

    return nullptr;
  }

  // If somebody does sameOriginIframeWindow.postMessage(...), they probably
  // expect the .source attribute of the resulting message event to be |window|
  // rather than |sameOriginIframeWindow|, even though the transparent wrapper
  // semantics of same-origin access will cause us to be in the iframe's
  // compartment at the time of the call. This means that we want the incumbent
  // global here, rather than the global of the current compartment.
  //
  // There are various edge cases in which the incumbent global and the current
  // global would not be same-origin. They include:
  // * A privileged caller (System Principal or Expanded Principal) manipulating
  //   less-privileged content via Xray Waivers.
  // * An unprivileged caller invoking a cross-origin function that was exposed
  //   to it by privileged code (i.e. Sandbox.importFunction).
  //
  // In these cases, we probably don't want the privileged global appearing in the
  // .source attribute. So we fall back to the compartment global there.
  JS::Rooted<JSObject*> incumbentGlobal(cx, &IncumbentJSGlobal());
  JS::Rooted<JSObject*> compartmentGlobal(cx, JS::CurrentGlobalOrNull(cx));
  nsIPrincipal* incumbentPrin = nsContentUtils::GetObjectPrincipal(incumbentGlobal);
  nsIPrincipal* compartmentPrin = nsContentUtils::GetObjectPrincipal(compartmentGlobal);
  return incumbentPrin->EqualsConsideringDomain(compartmentPrin) ? incumbentGlobal
                                                                 : compartmentGlobal;
}


nsGlobalWindow*
nsGlobalWindow::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  NS_ENSURE_TRUE(cx, nullptr);
  JS::Rooted<JSObject*> scope(cx, CallerGlobal());

  // When Jetpack runs content scripts inside a sandbox, it uses
  // sandboxPrototype to make them appear as though they're running in the
  // scope of the page. So when a content script invokes postMessage, it expects
  // the |source| of the received message to be the window set as the
  // sandboxPrototype. This used to work incidentally for unrelated reasons, but
  // now we need to do some special handling to support it.
  {
    JSAutoCompartment ac(cx, scope);
    JS::Rooted<JSObject*> scopeProto(cx);
    bool ok = JS_GetPrototype(cx, scope, &scopeProto);
    NS_ENSURE_TRUE(ok, nullptr);
    if (scopeProto && xpc::IsSandboxPrototypeProxy(scopeProto) &&
        (scopeProto = js::CheckedUnwrap(scopeProto, /* stopAtOuter = */ false)))
    {
      scope = scopeProto;
    }
  }
  JSAutoCompartment ac(cx, scope);

  // We don't use xpc::WindowOrNull here because we want to be able to tell
  // apart the cases of "scope is not an nsISupports at all" and "scope is an
  // nsISupports that's not a window". It's not clear whether that's desirable,
  // see bug 984467.
  nsISupports* native =
    nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, scope);
  if (!native)
    return nullptr;

  // The calling window must be holding a reference, so we can just return a
  // raw pointer here and let the QI's addref be balanced by the nsCOMPtr
  // destructor's release.
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(native);
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
                     nsIPrincipal* aProvidedPrincipal,
                     bool aTrustedCaller)
    : mSource(aSource),
      mCallerOrigin(aCallerOrigin),
      mTargetWindow(aTargetWindow),
      mProvidedPrincipal(aProvidedPrincipal),
      mTrustedCaller(aTrustedCaller)
    {
      MOZ_COUNT_CTOR(PostMessageEvent);
    }
    
    ~PostMessageEvent()
    {
      MOZ_COUNT_DTOR(PostMessageEvent);
    }

    JSAutoStructuredCloneBuffer& Buffer()
    {
      return mBuffer;
    }

    bool StoreISupports(nsISupports* aSupports)
    {
      mSupportsArray.AppendElement(aSupports);
      return true;
    }

  private:
    JSAutoStructuredCloneBuffer mBuffer;
    nsRefPtr<nsGlobalWindow> mSource;
    nsString mCallerOrigin;
    nsRefPtr<nsGlobalWindow> mTargetWindow;
    nsCOMPtr<nsIPrincipal> mProvidedPrincipal;
    bool mTrustedCaller;
    nsTArray<nsCOMPtr<nsISupports> > mSupportsArray;
};

namespace {

struct StructuredCloneInfo {
  PostMessageEvent* event;
  bool subsumes;
  nsPIDOMWindow* window;
  nsRefPtrHashtable<nsRefPtrHashKey<MessagePortBase>, MessagePortBase> ports;
};

static JSObject*
PostMessageReadStructuredClone(JSContext* cx,
                               JSStructuredCloneReader* reader,
                               uint32_t tag,
                               uint32_t data,
                               void* closure)
{
  if (tag == SCTAG_DOM_BLOB || tag == SCTAG_DOM_FILELIST) {
    NS_ASSERTION(!data, "Data should be empty");

    nsISupports* supports;
    if (JS_ReadBytes(reader, &supports, sizeof(supports))) {
      JS::Rooted<JS::Value> val(cx);
      if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, supports, &val))) {
        return val.toObjectOrNull();
      }
    }
  }

  const JSStructuredCloneCallbacks* runtimeCallbacks =
    js::GetContextStructuredCloneCallbacks(cx);

  if (runtimeCallbacks) {
    return runtimeCallbacks->read(cx, reader, tag, data, nullptr);
  }

  return nullptr;
}

static bool
PostMessageWriteStructuredClone(JSContext* cx,
                                JSStructuredCloneWriter* writer,
                                JS::Handle<JSObject*> obj,
                                void *closure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(closure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrappedNative));
  if (wrappedNative) {
    uint32_t scTag = 0;
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
    return runtimeCallbacks->write(cx, writer, obj, nullptr);
  }

  return false;
}

static bool
PostMessageReadTransferStructuredClone(JSContext* aCx,
                                       JSStructuredCloneReader* reader,
                                       uint32_t tag, void* aData,
                                       uint64_t aExtraData,
                                       void* aClosure,
                                       JS::MutableHandle<JSObject*> returnObject)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (MessageChannel::PrefEnabled() && tag == SCTAG_DOM_MAP_MESSAGEPORT) {
    MessagePort* port = static_cast<MessagePort*>(aData);
    port->BindToOwner(scInfo->window);
    scInfo->ports.Put(port, nullptr);

    JS::Rooted<JSObject*> obj(aCx, port->WrapObject(aCx));
    if (JS_WrapObject(aCx, &obj)) {
      MOZ_ASSERT(port->GetOwner() == scInfo->window);
      returnObject.set(obj);
    }

    return true;
  }

  return false;
}

static bool
PostMessageTransferStructuredClone(JSContext* aCx,
                                   JS::Handle<JSObject*> aObj,
                                   void* aClosure,
                                   uint32_t* aTag,
                                   JS::TransferableOwnership* aOwnership,
                                   void** aContent,
                                   uint64_t* aExtraData)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (MessageChannel::PrefEnabled()) {
    MessagePortBase* port = nullptr;
    nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
    if (NS_SUCCEEDED(rv)) {
      nsRefPtr<MessagePortBase> newPort;
      if (scInfo->ports.Get(port, getter_AddRefs(newPort))) {
        // No duplicate.
        return false;
      }

      newPort = port->Clone();
      scInfo->ports.Put(port, newPort);

      *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
      *aOwnership = JS::SCTAG_TMO_CUSTOM;
      *aContent = newPort;
      *aExtraData = 0;

      return true;
    }
  }

  return false;
}

void
PostMessageFreeTransferStructuredClone(uint32_t aTag, JS::TransferableOwnership aOwnership,
                                       void *aContent, uint64_t aExtraData, void* aClosure)
{
  StructuredCloneInfo* scInfo = static_cast<StructuredCloneInfo*>(aClosure);
  NS_ASSERTION(scInfo, "Must have scInfo!");

  if (MessageChannel::PrefEnabled() && aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
    nsRefPtr<MessagePortBase> port(static_cast<MessagePort*>(aContent));
    scInfo->ports.Remove(port);
  }
}

JSStructuredCloneCallbacks kPostMessageCallbacks = {
  PostMessageReadStructuredClone,
  PostMessageWriteStructuredClone,
  nullptr,
  PostMessageReadTransferStructuredClone,
  PostMessageTransferStructuredClone,
  PostMessageFreeTransferStructuredClone
};

} // anonymous namespace

static PLDHashOperator
PopulateMessagePortList(MessagePortBase* aKey, MessagePortBase* aValue, void* aClosure)
{
  nsTArray<nsRefPtr<MessagePortBase> > *array =
    static_cast<nsTArray<nsRefPtr<MessagePortBase> > *>(aClosure);

  array->AppendElement(aKey);
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
PostMessageEvent::Run()
{
  NS_ABORT_IF_FALSE(mTargetWindow->IsOuterWindow(),
                    "should have been passed an outer window!");
  NS_ABORT_IF_FALSE(!mSource || mSource->IsOuterWindow(),
                    "should have been passed an outer window!");

  AutoJSAPI jsapi;
  JSContext* cx = jsapi.cx();

  // If we bailed before this point we're going to leak mMessage, but
  // that's probably better than crashing.

  nsRefPtr<nsGlobalWindow> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsClosedOrClosing())
    return NS_OK;

  NS_ABORT_IF_FALSE(targetWindow->IsInnerWindow(),
                    "we ordered an inner window!");
  JSAutoCompartment ac(cx, targetWindow->GetWrapperPreserveColor());

  // Ensure that any origin which might have been provided is the origin of this
  // window's document.  Note that we do this *now* instead of when postMessage
  // is called because the target window might have been navigated to a
  // different location between then and now.  If this check happened when
  // postMessage was called, it would be fairly easy for a malicious webpage to
  // intercept messages intended for another site by carefully timing navigation
  // of the target window so it changed location after postMessage but before
  // now.
  if (mProvidedPrincipal) {
    // Get the target's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the target's
    // document.
    nsIPrincipal* targetPrin = targetWindow->GetPrincipal();
    if (NS_WARN_IF(!targetPrin))
      return NS_OK;

    // Note: This is contrary to the spec with respect to file: URLs, which
    //       the spec groups into a single origin, but given we intentionally
    //       don't do that in other places it seems better to hold the line for
    //       now.  Long-term, we want HTML5 to address this so that we can
    //       be compliant while being safer.
    if (!targetPrin->Equals(mProvidedPrincipal)) {
      return NS_OK;
    }
  }

  // Deserialize the structured clone data
  JS::Rooted<JS::Value> messageData(cx);
  StructuredCloneInfo scInfo;
  scInfo.event = this;
  scInfo.window = targetWindow;

  if (!mBuffer.read(cx, &messageData, &kPostMessageCallbacks, &scInfo)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  // Create the event
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
    do_QueryInterface(static_cast<nsPIDOMWindow*>(targetWindow.get()));
  nsRefPtr<MessageEvent> event =
    new MessageEvent(eventTarget, nullptr, nullptr);

  event->InitMessageEvent(NS_LITERAL_STRING("message"), false /*non-bubbling */,
                          false /*cancelable */, messageData, mCallerOrigin,
                          EmptyString(), mSource);

  nsTArray<nsRefPtr<MessagePortBase> > ports;
  scInfo.ports.EnumerateRead(PopulateMessagePortList, &ports);
  event->SetPorts(new MessagePortList(static_cast<dom::Event*>(event.get()), ports));

  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  nsIPresShell *shell = targetWindow->mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell)
    presContext = shell->GetPresContext();

  event->SetTrusted(mTrustedCaller);
  WidgetEvent* internalEvent = event->GetInternalNSEvent();

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(static_cast<nsPIDOMWindow*>(mTargetWindow),
                            presContext,
                            internalEvent,
                            static_cast<dom::Event*>(event.get()),
                            &status);
  return NS_OK;
}

void
nsGlobalWindow::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const nsAString& aTargetOrigin,
                               JS::Handle<JS::Value> aTransfer,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PostMessageMoz,
                            (aCx, aMessage, aTargetOrigin, aTransfer, aError),
                            aError, );

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
  if (!callerPrin) {
    return;
  }

  nsCOMPtr<nsIURI> callerOuterURI;
  if (NS_FAILED(callerPrin->GetURI(getter_AddRefs(callerOuterURI)))) {
    return;
  }

  nsAutoString origin;
  if (callerOuterURI) {
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerPrin, origin);
  }
  else if (callerInnerWin) {
    // otherwise use the URI of the document to generate origin
    nsCOMPtr<nsIDocument> doc = callerInnerWin->GetExtantDoc();
    if (!doc) {
      return;
    }
    callerOuterURI = doc->GetDocumentURI();
    // if the principal has a URI, use that to generate the origin
    nsContentUtils::GetUTFOrigin(callerOuterURI, origin);
  }
  else {
    // in case of a sandbox with a system principal origin can be empty
    if (!nsContentUtils::IsSystemPrincipal(callerPrin)) {
      return;
    }
  }

  // Convert the provided origin string into a URI for comparison purposes.
  nsCOMPtr<nsIPrincipal> providedPrincipal;

  if (aTargetOrigin.EqualsASCII("/")) {
    providedPrincipal = BrokenGetEntryGlobal()->PrincipalOrNull();
    if (NS_WARN_IF(!providedPrincipal))
      return;
  }

  // "*" indicates no specific origin is required.
  else if (!aTargetOrigin.EqualsASCII("*")) {
    nsCOMPtr<nsIURI> originURI;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(originURI), aTargetOrigin))) {
      aError.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }

    if (NS_FAILED(originURI->SetUserPass(EmptyCString())) ||
        NS_FAILED(originURI->SetPath(EmptyCString()))) {
      return;
    }

    nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);

    nsCOMPtr<nsIPrincipal> principal = nsContentUtils::GetSubjectPrincipal();
    MOZ_ASSERT(principal);

    uint32_t appId;
    if (NS_WARN_IF(NS_FAILED(principal->GetAppId(&appId))))
      return;

    bool isInBrowser;
    if (NS_WARN_IF(NS_FAILED(principal->GetIsInBrowserElement(&isInBrowser))))
      return;

    // Create a nsIPrincipal inheriting the app/browser attributes from the
    // caller.
    nsresult rv = ssm->GetAppCodebasePrincipal(originURI, appId, isInBrowser,
                                             getter_AddRefs(providedPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  nsRefPtr<PostMessageEvent> event =
    new PostMessageEvent(nsContentUtils::IsCallerChrome() || !callerInnerWin
                         ? nullptr
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         this,
                         providedPrincipal,
                         nsContentUtils::IsCallerChrome());

  // We *must* clone the data here, or the JS::Value could be modified
  // by script
  StructuredCloneInfo scInfo;
  scInfo.event = event;
  scInfo.window = this;

  nsIPrincipal* principal = GetPrincipal();
  JS::Rooted<JS::Value> message(aCx, aMessage);
  JS::Rooted<JS::Value> transfer(aCx, aTransfer);
  if (NS_FAILED(callerPrin->Subsumes(principal, &scInfo.subsumes)) ||
      !event->Buffer().write(aCx, message, transfer, &kPostMessageCallbacks,
                             &scInfo)) {
    aError.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  aError = NS_DispatchToCurrentThread(event);
}

void
nsGlobalWindow::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const nsAString& aTargetOrigin,
                               const Optional<Sequence<JS::Value > >& aTransfer,
                               ErrorResult& aError)
{
  JS::Rooted<JS::Value> transferArray(aCx, JS::UndefinedValue());
  if (aTransfer.WasPassed()) {
    const Sequence<JS::Value >& values = aTransfer.Value();

    // The input sequence only comes from the generated bindings code, which
    // ensures it is rooted.
    JS::HandleValueArray elements =
      JS::HandleValueArray::fromMarkedLocation(values.Length(), values.Elements());

    transferArray = JS::ObjectOrNullValue(JS_NewArrayObject(aCx, elements));
    if (transferArray.isNull()) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  PostMessageMoz(aCx, aMessage, aTargetOrigin, transferArray, aError);
}

NS_IMETHODIMP
nsGlobalWindow::PostMessageMoz(JS::Handle<JS::Value> aMessage,
                               const nsAString& aOrigin,
                               JS::Handle<JS::Value> aTransfer,
                               JSContext* aCx)
{
  ErrorResult rv;
  PostMessageMoz(aCx, aMessage, aOrigin, aTransfer, rv);

  return rv.ErrorCode();
}

class nsCloseEvent : public nsRunnable {

  nsRefPtr<nsGlobalWindow> mWindow;
  bool mIndirect;

  nsCloseEvent(nsGlobalWindow *aWindow, bool aIndirect)
    : mWindow(aWindow)
    , mIndirect(aIndirect)
  {}

public:

  static nsresult
  PostCloseEvent(nsGlobalWindow* aWindow, bool aIndirect) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(aWindow, aIndirect);
    nsresult rv = NS_DispatchToCurrentThread(ev);
    if (NS_SUCCEEDED(rv))
      aWindow->MaybeForgiveSpamCount();
    return rv;
  }

  NS_IMETHOD Run() {
    if (mWindow) {
      if (mIndirect) {
        return PostCloseEvent(mWindow, false);
      }
      mWindow->ReallyCloseWindow();
    }
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

void
nsGlobalWindow::Close(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(Close, (aError), aError, );

  if (!mDocShell || IsInModalState() ||
      (IsFrame() && !mDocShell->GetIsBrowserOrApp())) {
    // window.close() is called on a frame in a frameset, on a window
    // that's already closed, or on a window for which there's
    // currently a modal dialog open. Ignore such calls.
    return;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return;
  }

  if (mBlockScriptedClosingFlag)
  {
    // A script's popup has been blocked and we don't want
    // the window to be closed directly after this event,
    // so the user can see that there was a blocked popup.
    return;
  }

  // Don't allow scripts from content to close non-app or non-neterror
  // windows that were not opened by script.
  nsAutoString url;
  mDoc->GetURL(url);
  if (!mDocShell->GetIsApp() &&
      !StringBeginsWith(url, NS_LITERAL_STRING("about:neterror")) &&
      !mHadOriginalOpener && !nsContentUtils::IsCallerChrome()) {
    bool allowClose = mAllowScriptsToClose ||
      Preferences::GetBool("dom.allow_scripts_to_close_windows", true);
    if (!allowClose) {
      // We're blocking the close operation
      // report localized error msg in JS console
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("DOM Window"), mDoc,  // Better name for the category?
          nsContentUtils::eDOM_PROPERTIES,
          "WindowCloseBlockedWarning");

      return;
    }
  }

  if (!mInClose && !mIsClosed && !CanClose()) {
    return;
  }

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
    return;
  }

  aError = FinalClose();
}

NS_IMETHODIMP
nsGlobalWindow::Close()
{
  ErrorResult rv;
  Close(rv);

  return rv.ErrorCode();
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

  // This stuff is non-sensical but incredibly fragile. The reasons for the
  // behavior here don't make sense today and may not have ever made sense,
  // but various bits of frontend code break when you change them. If you need
  // to fix up this behavior, feel free to. It's a righteous task, but involves
  // wrestling with various download manager tests, frontend code, and possible
  // broken addons. The chrome tests in toolkit/mozapps/downloads are a good
  // testing ground.
  //
  // In particular, if |win|'s JSContext is at the top of the stack, we must
  // complete _two_ round-trips to the event loop before the call to
  // ReallyCloseWindow. This allows setTimeout handlers that are set after
  // FinalClose() is called to run before the window is torn down.
  bool indirect = GetContextInternal() && // Occasionally null. See bug 877390.
                  (nsContentUtils::GetCurrentJSContext() ==
                   GetContextInternal()->GetNativeContext());
  if (NS_FAILED(nsCloseEvent::PostCloseEvent(this, indirect))) {
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

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {

    // but if we're a browser window we could be in some nasty
    // self-destroying cascade that we should mostly ignore

    if (mDocShell) {
      nsCOMPtr<nsIBrowserDOMWindow> bwin;
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
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

    CleanUp();
  }
}

void
nsGlobalWindow::EnterModalState()
{
  FORWARD_TO_OUTER_VOID(EnterModalState, ());

  // GetScriptableTop, not GetTop, so that EnterModalState works properly with
  // <iframe mozbrowser>.
  nsGlobalWindow* topWin = GetScriptableTop();

  if (!topWin) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");
    return;
  }

  // If there is an active ESM in this window, clear it. Otherwise, this can
  // cause a problem if a modal state is entered during a mouseup event.
  EventStateManager* activeESM =
    static_cast<EventStateManager*>(
      EventStateManager::GetActiveEventStateManager());
  if (activeESM && activeESM->GetPresContext()) {
    nsIPresShell* activeShell = activeESM->GetPresContext()->GetPresShell();
    if (activeShell && (
        nsContentUtils::ContentIsCrossDocDescendantOf(activeShell->GetDocument(), mDoc) ||
        nsContentUtils::ContentIsCrossDocDescendantOf(mDoc, activeShell->GetDocument()))) {
      EventStateManager::ClearGlobalActiveContent(activeESM);

      activeShell->SetCapturingContent(nullptr, 0);

      if (activeShell) {
        nsRefPtr<nsFrameSelection> frameSelection = activeShell->FrameSelection();
        frameSelection->SetMouseDownState(false);
      }
    }
  }

  // Clear the capturing content if it is under topDoc.
  // Usually the activeESM check above does that, but there are cases when
  // we don't have activeESM, or it is for different document.
  nsIDocument* topDoc = topWin->GetExtantDoc();
  nsIContent* capturingContent = nsIPresShell::GetCapturingContent();
  if (capturingContent && topDoc &&
      nsContentUtils::ContentIsCrossDocDescendantOf(capturingContent, topDoc)) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
  }

  if (topWin->mModalStateDepth == 0) {
    NS_ASSERTION(!mSuspendedDoc, "Shouldn't have mSuspendedDoc here!");

    mSuspendedDoc = topDoc;
    if (mSuspendedDoc) {
      mSuspendedDoc->SuppressEventHandling(nsIDocument::eAnimationsOnly);
    }
  }
  topWin->mModalStateDepth++;
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

  inner->RunTimeout(nullptr);

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

  uint32_t i, length;
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
nsGlobalWindow::LeaveModalState()
{
  FORWARD_TO_OUTER_VOID(LeaveModalState, ());

  nsGlobalWindow* topWin = GetScriptableTop();

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
      nsCOMPtr<nsIDocument> currentDoc = topWin->GetExtantDoc();
      mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(nsIDocument::eAnimationsOnly,
                                                          currentDoc == mSuspendedDoc);
      mSuspendedDoc = nullptr;
    }
  }

  // Remember the time of the last dialog quit.
  nsGlobalWindow *inner = topWin->GetCurrentInnerWindowInternal();
  if (inner)
    inner->mLastDialogQuitTime = TimeStamp::Now();
}

bool
nsGlobalWindow::IsInModalState()
{
  nsGlobalWindow *topWin = GetScriptableTop();

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
      NotifyObservers(ToSupports(aWindow),
                      DOM_WINDOW_DESTROYED_TOPIC, nullptr);
  }
}

class WindowDestroyedEvent : public nsRunnable
{
public:
  WindowDestroyedEvent(nsPIDOMWindow* aWindow, uint64_t aID,
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
        observerService->NotifyObservers(wrapper, mTopic.get(), nullptr);
      }
    }

    bool skipNukeCrossCompartment = false;
#ifndef DEBUG
    nsCOMPtr<nsIAppStartup> appStartup =
      do_GetService(NS_APPSTARTUP_CONTRACTID);

    if (appStartup) {
      appStartup->GetShuttingDown(&skipNukeCrossCompartment);
    }
#endif

    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    if (!skipNukeCrossCompartment && window) {
      nsGlobalWindow* currentInner =
        window->IsInnerWindow() ? static_cast<nsGlobalWindow*>(window.get()) :
                                  static_cast<nsGlobalWindow*>(window->GetCurrentInnerWindow());
      NS_ENSURE_TRUE(currentInner, NS_OK);

      AutoSafeJSContext cx;
      JS::Rooted<JSObject*> obj(cx, currentInner->FastGetGlobalJSObject());
      // We only want to nuke wrappers for the chrome->content case
      if (obj && !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
        js::NukeCrossCompartmentWrappers(cx,
                                         js::ChromeCompartmentsOnly(),
                                         js::SingleCompartment(js::GetObjectCompartment(obj)),
                                         window->IsInnerWindow() ? js::DontNukeWindowReferences :
                                                                   js::NukeWindowReferences);
      }
    }

    return NS_OK;
  }

private:
  uint64_t mID;
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
        NotifyObservers(ToSupports(aWindow),
                        DOM_WINDOW_FROZEN_TOPIC, nullptr);
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
        NotifyObservers(ToSupports(aWindow),
                        DOM_WINDOW_THAWED_TOPIC, nullptr);
    }
  }
}

JSObject*
nsGlobalWindow::GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey)
{
  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> handler(cx);
  if (mCachedXBLPrototypeHandlers) {
    mCachedXBLPrototypeHandlers->Get(aKey, handler.address());
  }
  return handler;
}

void
nsGlobalWindow::CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                         JS::Handle<JSObject*> aHandler)
{
  if (!mCachedXBLPrototypeHandlers) {
    mCachedXBLPrototypeHandlers = new nsJSThingHashtable<nsPtrHashKey<nsXBLPrototypeHandler>, JSObject*>();
    PreserveWrapper(ToSupports(this));
  }

  mCachedXBLPrototypeHandlers->Put(aKey, aHandler);
}

/**
 * GetScriptableFrameElement is called when script reads
 * nsIGlobalWindow::frameElement.
 *
 * In contrast to GetRealFrameElement, GetScriptableFrameElement says that the
 * window contained by an <iframe mozbrowser> or <iframe mozapp> has no frame
 * element (effectively treating a mozbrowser the same as a content/chrome
 * boundary).
 */
NS_IMETHODIMP
nsGlobalWindow::GetScriptableFrameElement(nsIDOMElement** aFrameElement)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMElement> frameElement = do_QueryInterface(GetFrameElement(rv));
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  frameElement.forget(aFrameElement);

  return NS_OK;
}

Element*
nsGlobalWindow::GetFrameElement(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFrameElement, (aError), aError, nullptr);

  if (!mDocShell || mDocShell->GetIsBrowserOrApp()) {
    return nullptr;
  }

  // Per HTML5, the frameElement getter returns null in cross-origin situations.
  Element* element = GetRealFrameElement(aError);
  if (aError.Failed() || !element) {
    return nullptr;
  }
  if (!nsContentUtils::GetSubjectPrincipal()->
         SubsumesConsideringDomain(element->NodePrincipal())) {
    return nullptr;
  }
  return element;
}

Element*
nsGlobalWindow::GetRealFrameElement(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetRealFrameElement, (aError), aError, nullptr);

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent));

  if (!parent || parent == mDocShell) {
    // We're at a chrome boundary, don't expose the chrome iframe
    // element to content code.
    return nullptr;
  }

  return mFrameElement;
}

/**
 * nsIGlobalWindow::GetFrameElement (when called from C++) is just a wrapper
 * around GetRealFrameElement.
 */
NS_IMETHODIMP
nsGlobalWindow::GetRealFrameElement(nsIDOMElement** aFrameElement)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMElement> frameElement =
    do_QueryInterface(GetRealFrameElement(rv));
  frameElement.forget(aFrameElement);

  return rv.ErrorCode();
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

already_AddRefed<nsIVariant>
nsGlobalWindow::ShowModalDialog(const nsAString& aUrl, nsIVariant* aArgument,
                                const nsAString& aOptions, ErrorResult& aError)
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eShowModalDialog);
  }

  FORWARD_TO_OUTER_OR_THROW(ShowModalDialog,
                            (aUrl, aArgument, aOptions, aError), aError,
                            nullptr);

  if (Preferences::GetBool("dom.disable_window_showModalDialog", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsRefPtr<DialogValueHolder> argHolder =
    new DialogValueHolder(nsContentUtils::GetSubjectPrincipal(), aArgument);

  // Before bringing up the window/dialog, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  if (!AreDialogsEnabled()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (ShouldPromptToBlockDialogs() && !ConfirmDialogIfNeeded()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMWindow> dlgWin;
  nsAutoString options(NS_LITERAL_STRING("-moz-internal-modal=1,status=1"));

  ConvertDialogOptions(aOptions, options);

  options.AppendLiteral(",scrollbars=1,centerscreen=1,resizable=0");

  EnterModalState();
  uint32_t oldMicroTaskLevel = nsContentUtils::MicroTaskLevel();
  nsContentUtils::SetMicroTaskLevel(0);
  aError = OpenInternal(aUrl, EmptyString(), options,
                        false,          // aDialog
                        true,           // aContentModal
                        true,           // aCalledNoScript
                        true,           // aDoJSFixups
                        true,           // aNavigate
                        nullptr, argHolder, // args
                        GetPrincipal(),     // aCalleePrincipal
                        nullptr,            // aJSCallerContext
                        getter_AddRefs(dlgWin));
  nsContentUtils::SetMicroTaskLevel(oldMicroTaskLevel);
  LeaveModalState();
  if (aError.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMModalContentWindow> dialog = do_QueryInterface(dlgWin);
  if (!dialog) {
    return nullptr;
  }

  nsCOMPtr<nsIVariant> retVal;
  aError = dialog->GetReturnValue(getter_AddRefs(retVal));
  MOZ_ASSERT(!aError.Failed());

  return retVal.forget();
}

JS::Value
nsGlobalWindow::ShowModalDialog(JSContext* aCx, const nsAString& aUrl,
                                JS::Handle<JS::Value> aArgument,
                                const nsAString& aOptions,
                                ErrorResult& aError)
{
  nsCOMPtr<nsIVariant> args;
  aError = nsContentUtils::XPConnect()->JSToVariant(aCx,
                                                    aArgument,
                                                    getter_AddRefs(args));

  nsCOMPtr<nsIVariant> retVal = ShowModalDialog(aUrl, args, aOptions, aError);
  if (aError.Failed()) {
    return JS::UndefinedValue();
  }

  JS::Rooted<JS::Value> result(aCx);
  if (retVal) {
    aError = nsContentUtils::XPConnect()->VariantToJS(aCx,
                                                      FastGetGlobalJSObject(),
                                                      retVal, &result);
  } else {
    result = JS::NullValue();
  }
  return result;
}

NS_IMETHODIMP
nsGlobalWindow::ShowModalDialog(const nsAString& aURI, nsIVariant *aArgs_,
                                const nsAString& aOptions, uint8_t aArgc,
                                nsIVariant **aRetVal)
{
  // Per-spec the |arguments| parameter is supposed to pass through unmodified.
  // However, XPConnect default-initializes variants to null, rather than
  // undefined. Fix this up here.
  nsCOMPtr<nsIVariant> aArgs = aArgs_;
  if (aArgc < 1) {
    aArgs = CreateVoidVariant();
  }

  ErrorResult rv;
  nsCOMPtr<nsIVariant> retVal = ShowModalDialog(aURI, aArgs, aOptions, rv);
  retVal.forget(aRetVal);

  return rv.ErrorCode();
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
    do_QueryInterface(rootWindow->GetExtantDoc());
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

Selection*
nsGlobalWindow::GetSelection(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetSelection, (aError), aError, nullptr);

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  return static_cast<Selection*>(presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL));
}

NS_IMETHODIMP
nsGlobalWindow::GetSelection(nsISelection** aSelection)
{
  ErrorResult rv;
  nsCOMPtr<nsISelection> selection = GetSelection(rv);
  selection.forget(aSelection);

  return rv.ErrorCode();
}

bool
nsGlobalWindow::Find(const nsAString& aString, bool aCaseSensitive,
                     bool aBackwards, bool aWrapAround, bool aWholeWord,
                     bool aSearchInFrames, bool aShowDialog,
                     ErrorResult& aError)
{
  if (Preferences::GetBool("dom.disable_window_find", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  FORWARD_TO_OUTER_OR_THROW(Find,
                            (aString, aCaseSensitive, aBackwards, aWrapAround,
                             aWholeWord, aSearchInFrames, aShowDialog, aError),
                            aError, false);

  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));
  if (!finder) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

  // Set the options of the search
  aError = finder->SetSearchString(PromiseFlatString(aString).get());
  if (aError.Failed()) {
    return false;
  }
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
  if (aString.IsEmpty() || aShowDialog) {
    // See if the find dialog is already up using nsIWindowMediator
    nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

    nsCOMPtr<nsIDOMWindow> findDialog;

    if (windowMediator) {
      windowMediator->GetMostRecentWindow(MOZ_UTF16("findInPage"),
                                          getter_AddRefs(findDialog));
    }

    if (findDialog) {
      // The Find dialog is already open, bring it to the top.
      aError = findDialog->Focus();
    } else if (finder) {
      // Open a Find dialog
      nsCOMPtr<nsIDOMWindow> dialog;
      aError = OpenDialog(NS_LITERAL_STRING("chrome://global/content/finddialog.xul"),
                          NS_LITERAL_STRING("_blank"),
                          NS_LITERAL_STRING("chrome, resizable=no, dependent=yes"),
                          finder, getter_AddRefs(dialog));
    }

    return false;
  }

  // Launch the search with the passed in search string
  bool didFind = false;
  aError = finder->FindNext(&didFind);
  return didFind;
}

NS_IMETHODIMP
nsGlobalWindow::Find(const nsAString& aStr, bool aCaseSensitive,
                     bool aBackwards, bool aWrapAround, bool aWholeWord,
                     bool aSearchInFrames, bool aShowDialog,
                     bool *aDidFind)
{
  ErrorResult rv;
  *aDidFind = Find(aStr, aCaseSensitive, aBackwards, aWrapAround, aWholeWord,
                   aSearchInFrames, aShowDialog, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData, ErrorResult& aError)
{
  aError = nsContentUtils::Atob(aAsciiBase64String, aBinaryData);
}

NS_IMETHODIMP
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData)
{
  ErrorResult rv;
  Atob(aAsciiBase64String, aBinaryData, rv);

  return rv.ErrorCode();
}

void
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String, ErrorResult& aError)
{
  aError = nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  ErrorResult rv;
  Btoa(aBinaryData, aAsciiBase64String, rv);

  return rv.ErrorCode();
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    bool aUseCapture)
{
  if (nsRefPtr<EventListenerManager> elm = GetExistingListenerManager()) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsGlobalWindow)

NS_IMETHODIMP
nsGlobalWindow::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, aRetVal), NS_OK);

  if (!IsCurrentInnerWindow()) {
    NS_WARNING("DispatchEvent called on non-current inner window, dropping. "
               "Please check the window in the caller instead.");
    return NS_ERROR_FAILURE;
  }

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
    EventDispatcher::DispatchDOMEvent(GetOuterWindow(), nullptr, aEvent,
                                      presContext, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener *aListener,
                                 bool aUseCapture, bool aWantsUntrusted,
                                 uint8_t aOptionalArgc)
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

  EventListenerManager* manager = GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);
  manager->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

void
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 EventListener* aListener,
                                 bool aUseCapture,
                                 const Nullable<bool>& aWantsUntrusted,
                                 ErrorResult& aRv)
{
  if (IsOuterWindow() && mInnerWindow &&
      !nsContentUtils::CanCallerAccess(mInnerWindow)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  bool wantsUntrusted;
  if (aWantsUntrusted.IsNull()) {
    wantsUntrusted = !nsContentUtils::IsChromeDoc(mDoc);
  } else {
    wantsUntrusted = aWantsUntrusted.Value();
  }

  EventListenerManager* manager = GetOrCreateListenerManager();
  if (!manager) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  manager->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
}

NS_IMETHODIMP
nsGlobalWindow::AddSystemEventListener(const nsAString& aType,
                                       nsIDOMEventListener *aListener,
                                       bool aUseCapture,
                                       bool aWantsUntrusted,
                                       uint8_t aOptionalArgc)
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

EventListenerManager*
nsGlobalWindow::GetOrCreateListenerManager()
{
  FORWARD_TO_INNER_CREATE(GetOrCreateListenerManager, (), nullptr);

  if (!mListenerManager) {
    mListenerManager =
      new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager*
nsGlobalWindow::GetExistingListenerManager() const
{
  FORWARD_TO_INNER(GetExistingListenerManager, (), nullptr);

  return mListenerManager;
}

nsIScriptContext*
nsGlobalWindow::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = NS_ERROR_UNEXPECTED;
  NS_ENSURE_TRUE(!IsInnerWindow() || IsCurrentInnerWindow(), nullptr);

  nsIScriptContext* scx;
  if ((scx = GetContext())) {
    *aRv = NS_OK;
    return scx;
  }
  return nullptr;
}

//*****************************************************************************
// nsGlobalWindow::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindow*
nsGlobalWindow::GetPrivateParent()
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (static_cast<nsIDOMWindow *>(this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nullptr;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetDocument();
    if (!doc)
      return nullptr;             // This is ok, just means a null parent.

    return doc->GetWindow();
  }

  if (parent) {
    return static_cast<nsGlobalWindow *>
                      (static_cast<nsIDOMWindow*>(parent.get()));
  }

  return nullptr;
}

nsPIDOMWindow*
nsGlobalWindow::GetPrivateRoot()
{
  if (IsInnerWindow()) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    if (!outer) {
      NS_WARNING("No outer window available!");
      return nullptr;
    }
    return outer->GetPrivateRoot();
  }

  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

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

  return static_cast<nsGlobalWindow*>(top.get());
}


nsIDOMLocation*
nsGlobalWindow::GetLocation(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetLocation, (aError), aError, nullptr);

  nsIDocShell *docShell = GetDocShell();
  if (!mLocation && docShell) {
    mLocation = new nsLocation(docShell);
  }
  return mLocation;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocation(nsIDOMLocation ** aLocation)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMLocation> location = GetLocation(rv);
  location.forget(aLocation);

  return rv.ErrorCode();
}

void
nsGlobalWindow::ActivateOrDeactivate(bool aActivate)
{
  MOZ_ASSERT(IsOuterWindow());

  // Set / unset mIsActive on the top level window, which is used for the
  // :-moz-window-inactive pseudoclass, and its sheet (if any).
  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  if (!mainWidget)
    return;

  // Get the top level widget (if the main widget is a sheet, this will
  // be the sheet's top (non-sheet) parent).
  nsCOMPtr<nsIWidget> topLevelWidget = mainWidget->GetSheetWindowParent();
  if (!topLevelWidget) {
    topLevelWidget = mainWidget;
  }

  nsCOMPtr<nsPIDOMWindow> piMainWindow(
    do_QueryInterface(static_cast<nsIDOMWindow*>(this)));
  piMainWindow->SetActive(aActivate);

  if (mainWidget != topLevelWidget) {
    // This is a workaround for the following problem:
    // When a window with an open sheet gains or loses focus, only the sheet
    // window receives the NS_ACTIVATE/NS_DEACTIVATE event.  However the
    // styling of the containing top level window also needs to change.  We
    // get around this by calling nsPIDOMWindow::SetActive() on both windows.

    // Get the top level widget's nsGlobalWindow
    nsCOMPtr<nsIDOMWindow> topLevelWindow;

    // widgetListener should be a nsXULWindow
    nsIWidgetListener* listener = topLevelWidget->GetWidgetListener();
    if (listener) {
      nsCOMPtr<nsIXULWindow> window = listener->GetXULWindow();
      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(window));
      topLevelWindow = do_GetInterface(req);
    }

    if (topLevelWindow) {
      nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(topLevelWindow));
      piWin->SetActive(aActivate);
    }
  }
}

static bool
NotifyDocumentTree(nsIDocument* aDocument, void* aData)
{
  aDocument->EnumerateSubDocuments(NotifyDocumentTree, nullptr);
  aDocument->DocumentStatesChanged(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
  return true;
}

void
nsGlobalWindow::SetActive(bool aActive)
{
  nsPIDOMWindow::SetActive(aActive);
  NotifyDocumentTree(mDoc, nullptr);
}

void nsGlobalWindow::SetIsBackground(bool aIsBackground)
{
  MOZ_ASSERT(IsOuterWindow());

  bool resetTimers = (!aIsBackground && IsBackground());
  nsPIDOMWindow::SetIsBackground(aIsBackground);
  if (resetTimers) {
    ResetTimersForNonBackgroundWindow();
  }
#ifdef MOZ_GAMEPAD
  if (!aIsBackground) {
    nsGlobalWindow* inner = GetCurrentInnerWindowInternal();
    if (inner) {
      inner->SyncGamepadState();
    }
  }
#endif
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
      services::GetObserverService();

    if (observerService) {
      observerService->NotifyObservers(static_cast<nsIDOMWindow*>(this),
                                       DOM_TOUCH_LISTENER_ADDED,
                                       nullptr);
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
nsGlobalWindow::EnableGamepadUpdates()
{
  FORWARD_TO_INNER_VOID(EnableGamepadUpdates, ());
  if (mHasGamepad) {
#ifdef MOZ_GAMEPAD
    nsRefPtr<GamepadService> gamepadsvc(GamepadService::GetService());
    if (gamepadsvc) {
      gamepadsvc->AddListener(this);
    }
#endif
  }
}

void
nsGlobalWindow::DisableGamepadUpdates()
{
  FORWARD_TO_INNER_VOID(DisableGamepadUpdates, ());
  if (mHasGamepad) {
#ifdef MOZ_GAMEPAD
    nsRefPtr<GamepadService> gamepadsvc(GamepadService::GetService());
    if (gamepadsvc) {
      gamepadsvc->RemoveListener(this);
    }
#endif
  }
}

void
nsGlobalWindow::SetChromeEventHandler(EventTarget* aChromeEventHandler)
{
  MOZ_ASSERT(IsOuterWindow());

  SetChromeEventHandlerInternal(aChromeEventHandler);
  // update the chrome event handler on all our inner windows
  for (nsGlobalWindow *inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
       inner != this;
       inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
    NS_ASSERTION(!inner->mOuterWindow || inner->mOuterWindow == this,
                 "bad outer window pointer");
    inner->SetChromeEventHandlerInternal(aChromeEventHandler);
  }
}

static bool IsLink(nsIContent* aContent)
{
  return aContent && (aContent->IsHTML(nsGkAtoms::a) ||
                      aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                            nsGkAtoms::simple, eCaseMatters));
}

void
nsGlobalWindow::SetFocusedNode(nsIContent* aNode,
                               uint32_t aFocusMethod,
                               bool aNeedsFocus)
{
  FORWARD_TO_INNER_VOID(SetFocusedNode, (aNode, aFocusMethod, aNeedsFocus));

  if (aNode && aNode->GetCurrentDoc() != mDoc) {
    NS_WARNING("Trying to set focus to a node from a wrong document");
    return;
  }

  if (mCleanedUp) {
    NS_ASSERTION(!aNode, "Trying to focus cleaned up window!");
    aNode = nullptr;
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

uint32_t
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
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    int32_t childCount = 0;
    docShell->GetChildCount(&childCount);

    for (int32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      docShell->GetChildAt(i, getter_AddRefs(childShell));
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
nsGlobalWindow::TakeFocus(bool aFocus, uint32_t aFocusMethod)
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
  if (aFocus && mNeedsFocus && mDoc && mDoc->GetRootElement() != nullptr) {
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
    MOZ_ASSERT(mWindow);
    MOZ_ASSERT(mWindow->IsInnerWindow());
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
  nsAutoCString oldBeforeHash, oldHash, newBeforeHash, newHash;
  nsContentUtils::SplitURIAtHash(aOldURI, oldBeforeHash, oldHash);
  nsContentUtils::SplitURIAtHash(aNewURI, newBeforeHash, newHash);

  NS_ENSURE_STATE(oldBeforeHash.Equals(newBeforeHash));
  NS_ENSURE_STATE(!oldHash.Equals(newHash));

  nsAutoCString oldSpec, newSpec;
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
  MOZ_ASSERT(IsInnerWindow());

  // Don't do anything if the window is frozen.
  if (IsFrozen())
    return NS_OK;

  // Get a presentation shell for use in creating the hashchange event.
  NS_ENSURE_STATE(IsCurrentInnerWindow());

  nsIPresShell *shell = mDoc->GetShell();
  nsRefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  // Create a new hashchange event.
  nsCOMPtr<nsIDOMEvent> domEvent;
  nsresult rv =
    EventDispatcher::CreateEvent(this, presContext, nullptr,
                                 NS_LITERAL_STRING("hashchangeevent"),
                                 getter_AddRefs(domEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHashChangeEvent> hashchangeEvent = do_QueryInterface(domEvent);
  NS_ENSURE_TRUE(hashchangeEvent, NS_ERROR_UNEXPECTED);

  // The hashchange event bubbles and isn't cancellable.
  rv = hashchangeEvent->InitHashChangeEvent(NS_LITERAL_STRING("hashchange"),
                                            true, false,
                                            aOldURL, aNewURL);
  NS_ENSURE_SUCCESS(rv, rv);

  domEvent->SetTrusted(true);

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
  rv = EventDispatcher::CreateEvent(this, presContext, nullptr,
                                    NS_LITERAL_STRING("popstateevent"),
                                    getter_AddRefs(domEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the popstate event, which does bubble but isn't cancellable.
  nsCOMPtr<nsIDOMPopStateEvent> popstateEvent = do_QueryInterface(domEvent);
  rv = popstateEvent->InitPopStateEvent(NS_LITERAL_STRING("popstate"),
                                        true, false,
                                        stateObj);
  NS_ENSURE_SUCCESS(rv, rv);

  domEvent->SetTrusted(true);

  nsCOMPtr<EventTarget> outerWindow =
    do_QueryInterface(GetOuterWindow());
  NS_ENSURE_TRUE(outerWindow, NS_ERROR_UNEXPECTED);

  rv = domEvent->SetTarget(outerWindow);
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

    return nullptr;
}

//-------------------------------------------------------
// Tells the HTMLFrame/CanvasFrame that is now has focus
void
nsGlobalWindow::UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent)
{
  MOZ_ASSERT(IsInnerWindow());

  // this is called from the inner window so use GetDocShell
  nsIDocShell* docShell = GetDocShell();
  if (!docShell)
    return;

  bool editable;
  docShell->GetEditable(&editable);
  if (editable)
    return;

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (!presShell || !mDoc)
    return;

  Element *rootElement = mDoc->GetRootElement();
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

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetComputedStyle(Element& aElt, const nsAString& aPseudoElt,
                                 ErrorResult& aError)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aError);
}

NS_IMETHODIMP
nsGlobalWindow::GetComputedStyle(nsIDOMElement* aElt,
                                 const nsAString& aPseudoElt,
                                 nsIDOMCSSStyleDeclaration** aReturn)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aReturn);
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetDefaultComputedStyle(Element& aElt,
                                        const nsAString& aPseudoElt,
                                        ErrorResult& aError)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, true, aError);
}

NS_IMETHODIMP
nsGlobalWindow::GetDefaultComputedStyle(nsIDOMElement* aElt,
                                        const nsAString& aPseudoElt,
                                        nsIDOMCSSStyleDeclaration** aReturn)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, true, aReturn);
}

nsresult
nsGlobalWindow::GetComputedStyleHelper(nsIDOMElement* aElt,
                                       const nsAString& aPseudoElt,
                                       bool aDefaultStylesOnly,
                                       nsIDOMCSSStyleDeclaration** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

  nsCOMPtr<dom::Element> element = do_QueryInterface(aElt);
  if (!element) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  ErrorResult rv;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> declaration =
    GetComputedStyleHelper(*element, aPseudoElt, aDefaultStylesOnly, rv);
  declaration.forget(aReturn);

  return rv.ErrorCode();
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetComputedStyleHelper(Element& aElt,
                                       const nsAString& aPseudoElt,
                                       bool aDefaultStylesOnly,
                                       ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetComputedStyleHelper,
                            (aElt, aPseudoElt, aDefaultStylesOnly, aError),
                            aError, nullptr);

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presShell) {
    // Try flushing frames on our parent in case there's a pending
    // style change that will create the presshell.
    nsGlobalWindow *parent =
      static_cast<nsGlobalWindow *>(GetPrivateParent());
    if (!parent) {
      return nullptr;
    }

    parent->FlushPendingNotifications(Flush_Frames);

    // Might have killed mDocShell
    if (!mDocShell) {
      return nullptr;
    }

    presShell = mDocShell->GetPresShell();
    if (!presShell) {
      return nullptr;
    }
  }

  nsRefPtr<nsComputedDOMStyle> compStyle =
    NS_NewComputedDOMStyle(&aElt, aPseudoElt, presShell,
                           aDefaultStylesOnly ? nsComputedDOMStyle::eDefaultOnly :
                                                nsComputedDOMStyle::eAll);

  return compStyle.forget();
}

nsIDOMStorage*
nsGlobalWindow::GetSessionStorage(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetSessionStorage, (aError), aError, nullptr);

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell || !Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
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
        mSessionStorage = nullptr;
      }
    }
  }

  if (!mSessionStorage) {
    nsString documentURI;
    if (mDoc) {
      mDoc->GetDocumentURI(documentURI);
    }

    // If the document has the sandboxed origin flag set
    // don't allow access to sessionStorage.
    if (!mDoc) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    if (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    nsresult rv;

    nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(docShell, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);

    aError = storageManager->CreateStorage(principal,
                                           documentURI,
                                           loadContext && loadContext->UsePrivateBrowsing(),
                                           getter_AddRefs(mSessionStorage));
    if (aError.Failed()) {
      return nullptr;
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
      PR_LogPrint("nsGlobalWindow %p tried to get a new sessionStorage %p", this, mSessionStorage.get());
    }
#endif

    if (!mSessionStorage) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
    PR_LogPrint("nsGlobalWindow %p returns %p sessionStorage", this, mSessionStorage.get());
  }
#endif

  return mSessionStorage;
}

NS_IMETHODIMP
nsGlobalWindow::GetSessionStorage(nsIDOMStorage ** aSessionStorage)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMStorage> storage = GetSessionStorage(rv);
  storage.forget(aSessionStorage);

  return rv.ErrorCode();
}

nsIDOMStorage*
nsGlobalWindow::GetLocalStorage(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetLocalStorage, (aError), aError, nullptr);

  if (!Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
  }

  if (!mLocalStorage) {
    if (!DOMStorage::CanUseStorage()) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    nsIPrincipal *principal = GetPrincipal();
    if (!principal) {
      return nullptr;
    }

    nsresult rv;
    nsCOMPtr<nsIDOMStorageManager> storageManager =
      do_GetService("@mozilla.org/dom/localStorage-manager;1", &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    // If the document has the sandboxed origin flag set
    // don't allow access to localStorage.
    if (mDoc && (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    nsString documentURI;
    if (mDoc) {
      mDoc->GetDocumentURI(documentURI);
    }

    nsIDocShell* docShell = GetDocShell();
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);

    aError = storageManager->CreateStorage(principal,
                                           documentURI,
                                           loadContext && loadContext->UsePrivateBrowsing(),
                                           getter_AddRefs(mLocalStorage));
  }

  return mLocalStorage;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocalStorage(nsIDOMStorage ** aLocalStorage)
{
  NS_ENSURE_ARG(aLocalStorage);

  ErrorResult rv;
  nsCOMPtr<nsIDOMStorage> storage = GetLocalStorage(rv);
  storage.forget(aLocalStorage);

  return rv.ErrorCode();
}

indexedDB::IDBFactory*
nsGlobalWindow::GetIndexedDB(ErrorResult& aError)
{
  if (!mIndexedDB) {
    // If the document has the sandboxed origin flag set
    // don't allow access to indexedDB.
    if (mDoc && (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    if (!IsChromeWindow()) {
      // Whitelist about:home, since it doesn't have a base domain it would not
      // pass the thirdPartyUtil check, though it should be able to use
      // indexedDB.
      bool skipThirdPartyCheck = false;
      nsIPrincipal *principal = GetPrincipal();
      if (principal) {
        nsCOMPtr<nsIURI> uri;
        principal->GetURI(getter_AddRefs(uri));
        bool isAbout = false;
        if (uri && NS_SUCCEEDED(uri->SchemeIs("about", &isAbout)) && isAbout) {
          nsAutoCString path;
          skipThirdPartyCheck = NS_SUCCEEDED(uri->GetPath(path)) &&
                                path.EqualsLiteral("home");
        }
      }

      if (!skipThirdPartyCheck) {
        nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
          do_GetService(THIRDPARTYUTIL_CONTRACTID);
        if (!thirdPartyUtil) {
          aError.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
          return nullptr;
        }

        bool isThirdParty;
        aError = thirdPartyUtil->IsThirdPartyWindow(this, nullptr,
                                                    &isThirdParty);
        if (aError.Failed() || isThirdParty) {
          NS_WARN_IF_FALSE(aError.Failed(),
                           "IndexedDB is not permitted in a third-party window.");
          return nullptr;
        }
      }
    }

    // This may be null if being created from a file.
    aError = indexedDB::IDBFactory::Create(this, nullptr,
                                           getter_AddRefs(mIndexedDB));
  }

  return mIndexedDB;
}

NS_IMETHODIMP
nsGlobalWindow::GetIndexedDB(nsISupports** _retval)
{
  ErrorResult rv;
  nsCOMPtr<nsISupports> request(GetIndexedDB(rv));
  request.forget(_retval);

  return rv.ErrorCode();
}

NS_IMETHODIMP
nsGlobalWindow::GetMozIndexedDB(nsISupports** _retval)
{
  return GetIndexedDB(_retval);
}

//*****************************************************************************
// nsGlobalWindow::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetInterface(const nsIID & aIID, void **aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nullptr;

  if (aIID.Equals(NS_GET_IID(nsIDocCharset))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    NS_WARNING("Using deprecated nsIDocCharset: use nsIDocShell.GetCharset() instead ");
    nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(outer->mDocShell));
    docCharset.forget(aSink);
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(outer->mDocShell));
    webNav.forget(aSink);
  }
  else if (aIID.Equals(NS_GET_IID(nsIDocShell))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDocShell> docShell = outer->mDocShell;
    docShell.forget(aSink);
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    if (outer->mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      outer->mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        webBrowserPrint.forget(aSink);
      }
    }
  }
#endif
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    if (!mWindowUtils) {
      mWindowUtils = new nsDOMWindowUtils(outer);
    }

    *aSink = mWindowUtils;
    NS_ADDREF(((nsISupports *) *aSink));
  }
  else if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(outer->mDocShell));
    loadContext.forget(aSink);
  }
  else {
    return QueryInterface(aIID, aSink);
  }

  return *aSink ? NS_OK : NS_ERROR_NO_INTERFACE;
}

JS::Value
nsGlobalWindow::GetInterface(JSContext* aCx, nsIJSID* aIID, ErrorResult& aError)
{
  return dom::GetInterface(aCx, this, aIID, aError);
}

void
nsGlobalWindow::FireOfflineStatusEvent()
{
  if (!IsCurrentInnerWindow())
    return;
  nsAutoString name;
  if (NS_IsOffline()) {
    name.AssignLiteral("offline");
  } else {
    name.AssignLiteral("online");
  }
  // The event is fired at the body element, or if there is no body element,
  // at the document.
  nsCOMPtr<EventTarget> eventTarget = mDoc.get();
  nsHTMLDocument* htmlDoc = mDoc->AsHTMLDocument();
  if (htmlDoc) {
    Element* body = htmlDoc->GetBody();
    if (body) {
      eventTarget = body;
    }
  } else {
    Element* documentElement = mDoc->GetDocumentElement();
    if (documentElement) {
      eventTarget = documentElement;
    }
  }
  nsContentUtils::DispatchTrustedEvent(mDoc, eventTarget, name, true, false);
}

class NotifyIdleObserverRunnable : public nsRunnable
{
public:
  NotifyIdleObserverRunnable(nsIIdleObserver* aIdleObserver,
                             uint32_t aTimeInS,
                             bool aCallOnidle,
                             nsGlobalWindow* aIdleWindow)
    : mIdleObserver(aIdleObserver), mTimeInS(aTimeInS), mIdleWindow(aIdleWindow),
      mCallOnidle(aCallOnidle)
  { }

  NS_IMETHOD Run()
  {
    if (mIdleWindow->ContainsIdleObserver(mIdleObserver, mTimeInS)) {
      return mCallOnidle ? mIdleObserver->Onidle() : mIdleObserver->Onactive();
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIIdleObserver> mIdleObserver;
  uint32_t mTimeInS;
  nsRefPtr<nsGlobalWindow> mIdleWindow;

  // If false then call on active
  bool mCallOnidle;
};

void
nsGlobalWindow::NotifyIdleObserver(IdleObserverHolder* aIdleObserverHolder,
                                   bool aCallOnidle)
{
  MOZ_ASSERT(aIdleObserverHolder);
  aIdleObserverHolder->mPrevNotificationIdle = aCallOnidle;

  nsCOMPtr<nsIRunnable> caller =
    new NotifyIdleObserverRunnable(aIdleObserverHolder->mIdleObserver,
                                   aIdleObserverHolder->mTimeInS,
                                   aCallOnidle, this);
  if (NS_FAILED(NS_DispatchToCurrentThread(caller))) {
    NS_WARNING("Failed to dispatch thread for idle observer notification.");
  }
}

bool
nsGlobalWindow::ContainsIdleObserver(nsIIdleObserver* aIdleObserver, uint32_t aTimeInS)
{
  MOZ_ASSERT(aIdleObserver, "Idle observer not instantiated.");
  bool found = false;
  nsTObserverArray<IdleObserverHolder>::ForwardIterator iter(mIdleObservers);
  while (iter.HasMore()) {
    IdleObserverHolder& idleObserver = iter.GetNext();
    if (idleObserver.mIdleObserver == aIdleObserver &&
        idleObserver.mTimeInS == aTimeInS) {
      found = true;
      break;
    }
  }
  return found;
}

void
IdleActiveTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsRefPtr<nsGlobalWindow> idleWindow = static_cast<nsGlobalWindow*>(aClosure);
  MOZ_ASSERT(idleWindow, "Idle window has not been instantiated.");
  idleWindow->HandleIdleActiveEvent();
}

void
IdleObserverTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsRefPtr<nsGlobalWindow> idleWindow = static_cast<nsGlobalWindow*>(aClosure);
  MOZ_ASSERT(idleWindow, "Idle window has not been instantiated.");
  idleWindow->HandleIdleObserverCallback();
}

void
nsGlobalWindow::HandleIdleObserverCallback()
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");
  MOZ_ASSERT(static_cast<uint32_t>(mIdleCallbackIndex) < mIdleObservers.Length(),
                                  "Idle callback index exceeds array bounds!");
  IdleObserverHolder& idleObserver = mIdleObservers.ElementAt(mIdleCallbackIndex);
  NotifyIdleObserver(&idleObserver, true);
  mIdleCallbackIndex++;
  if (NS_FAILED(ScheduleNextIdleObserverCallback())) {
    NS_WARNING("Failed to set next idle observer callback.");
  }
}

nsresult
nsGlobalWindow::ScheduleNextIdleObserverCallback()
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");
  MOZ_ASSERT(mIdleService, "No idle service!");

  if (mIdleCallbackIndex < 0 ||
      static_cast<uint32_t>(mIdleCallbackIndex) >= mIdleObservers.Length()) {
    return NS_OK;
  }

  IdleObserverHolder& idleObserver =
    mIdleObservers.ElementAt(mIdleCallbackIndex);

  uint32_t userIdleTimeMS = 0;
  nsresult rv = mIdleService->GetIdleTime(&userIdleTimeMS);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t callbackTimeMS = 0;
  if (idleObserver.mTimeInS * 1000 + mIdleFuzzFactor > userIdleTimeMS) {
    callbackTimeMS = idleObserver.mTimeInS * 1000 - userIdleTimeMS + mIdleFuzzFactor;
  }

  mIdleTimer->Cancel();
  rv = mIdleTimer->InitWithFuncCallback(IdleObserverTimerCallback,
                                        this,
                                        callbackTimeMS,
                                        nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

uint32_t
nsGlobalWindow::GetFuzzTimeMS()
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");

  if (sIdleObserversAPIFuzzTimeDisabled) {
    return 0;
  }

  uint32_t randNum = MAX_IDLE_FUZZ_TIME_MS;
  size_t nbytes = PR_GetRandomNoise(&randNum, sizeof(randNum));
  if (nbytes != sizeof(randNum)) {
    NS_WARNING("PR_GetRandomNoise(...) Not implemented or no available noise!");
    return MAX_IDLE_FUZZ_TIME_MS;
  }

  if (randNum > MAX_IDLE_FUZZ_TIME_MS) {
    randNum %= MAX_IDLE_FUZZ_TIME_MS;
  }

  return randNum;
}

nsresult
nsGlobalWindow::ScheduleActiveTimerCallback()
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");

  if (!mAddActiveEventFuzzTime) {
    return HandleIdleActiveEvent();
  }

  MOZ_ASSERT(mIdleTimer);
  mIdleTimer->Cancel();

  uint32_t fuzzFactorInMS = GetFuzzTimeMS();
  nsresult rv = mIdleTimer->InitWithFuncCallback(IdleActiveTimerCallback,
                                                 this,
                                                 fuzzFactorInMS,
                                                 nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsGlobalWindow::HandleIdleActiveEvent()
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");

  if (mCurrentlyIdle) {
    mIdleCallbackIndex = 0;
    mIdleFuzzFactor = GetFuzzTimeMS();
    nsresult rv = ScheduleNextIdleObserverCallback();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  mIdleCallbackIndex = -1;
  MOZ_ASSERT(mIdleTimer);
  mIdleTimer->Cancel();
  nsTObserverArray<IdleObserverHolder>::ForwardIterator iter(mIdleObservers);
  while (iter.HasMore()) {
    IdleObserverHolder& idleObserver = iter.GetNext();
    if (idleObserver.mPrevNotificationIdle) {
      NotifyIdleObserver(&idleObserver, false);
    }
  }

  return NS_OK;
}

nsGlobalWindow::SlowScriptResponse
nsGlobalWindow::ShowSlowScriptDialog()
{
  MOZ_ASSERT(IsInnerWindow());

  nsresult rv;
  AutoJSContext cx;

  // If it isn't safe to run script, then it isn't safe to bring up the prompt
  // (since that spins the event loop). In that (rare) case, we just kill the
  // script and report a warning.
  if (!nsContentUtils::IsSafeToRunScript()) {
    JS_ReportWarning(cx, "A long running script was terminated");
    return KillSlowScript;
  }

  // If our document is not active, just kill the script: we've been unloaded
  if (!HasActiveDocument()) {
    return KillSlowScript;
  }

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIDocShell> ds = GetDocShell();
  NS_ENSURE_TRUE(ds, KillSlowScript);
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ds);
  NS_ENSURE_TRUE(prompt, KillSlowScript);

  // Check if we should offer the option to debug
  JS::AutoFilename filename;
  unsigned lineno;
  bool hasFrame = JS::DescribeScriptedCaller(cx, &filename, &lineno);

  bool debugPossible = hasFrame && js::CanCallContextDebugHandler(cx);
#ifdef MOZ_JSDEBUGGER
  // Get the debugger service if necessary.
  if (debugPossible) {
    bool jsds_IsOn = false;
    const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
    nsCOMPtr<jsdIExecutionHook> jsdHook;
    nsCOMPtr<jsdIDebuggerService> jsds = do_GetService(jsdServiceCtrID, &rv);

    // Check if there's a user for the debugger service that's 'on' for us
    if (NS_SUCCEEDED(rv)) {
      jsds->GetDebuggerHook(getter_AddRefs(jsdHook));
      jsds->GetIsOn(&jsds_IsOn);
    }

    // If there is a debug handler registered for this runtime AND
    // ((jsd is on AND has a hook) OR (jsd isn't on (something else debugs)))
    // then something useful will be done with our request to debug.
    debugPossible = ((jsds_IsOn && (jsdHook != nullptr)) || !jsds_IsOn);
  }
#endif

  // Get localizable strings
  nsXPIDLString title, msg, stopButton, waitButton, debugButton, neverShowDlg;

  rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                          "KillScriptTitle",
                                          title);

  nsresult tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "StopScriptButton",
                                           stopButton);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "WaitForScriptButton",
                                           waitButton);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "DontAskAgain",
                                           neverShowDlg);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }


  if (debugPossible) {
    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "DebugScriptButton",
                                             debugButton);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptWithDebugMessage",
                                             msg);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }
  else {
    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptMessage",
                                             msg);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }

  // GetStringFromName can return NS_OK and still give nullptr string
  if (NS_FAILED(rv) || !title || !msg || !stopButton || !waitButton ||
      (!debugButton && debugPossible) || !neverShowDlg) {
    NS_ERROR("Failed to get localized strings.");
    return ContinueSlowScript;
  }

  // Append file and line number information, if available
  if (filename.get()) {
    nsXPIDLString scriptLocation;
    NS_ConvertUTF8toUTF16 filenameUTF16(filename.get());
    const char16_t *formatParams[] = { filenameUTF16.get() };
    rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "KillScriptLocation",
                                               formatParams,
                                               scriptLocation);

    if (NS_SUCCEEDED(rv) && scriptLocation) {
      msg.AppendLiteral("\n\n");
      msg.Append(scriptLocation);
      msg.Append(':');
      msg.AppendInt(lineno);
    }
  }

  int32_t buttonPressed = 0; // In case the user exits dialog by clicking X.
  bool neverShowDlgChk = false;
  uint32_t buttonFlags = nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          (nsIPrompt::BUTTON_POS_0 + nsIPrompt::BUTTON_POS_1));

  // Add a third button if necessary.
  if (debugPossible)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  // Null out the operation callback while we're re-entering JS here.
  JSRuntime* rt = JS_GetRuntime(cx);
  JSInterruptCallback old = JS_SetInterruptCallback(rt, nullptr);

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, waitButton, stopButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  JS_SetInterruptCallback(rt, old);

  if (NS_SUCCEEDED(rv) && (buttonPressed == 0)) {
    return neverShowDlgChk ? AlwaysContinueSlowScript : ContinueSlowScript;
  }
  if ((buttonPressed == 2) && debugPossible) {
    return js_CallContextDebugHandler(cx) ? ContinueSlowScript : KillSlowScript;
  }
  JS_ClearPendingException(cx);
  return KillSlowScript;
}

uint32_t
nsGlobalWindow::FindInsertionIndex(IdleObserverHolder* aIdleObserver)
{
  MOZ_ASSERT(IsInnerWindow());
  MOZ_ASSERT(aIdleObserver, "Idle observer not instantiated.");

  uint32_t i = 0;
  nsTObserverArray<IdleObserverHolder>::ForwardIterator iter(mIdleObservers);
  while (iter.HasMore()) {
    IdleObserverHolder& idleObserver = iter.GetNext();
    if (idleObserver.mTimeInS > aIdleObserver->mTimeInS) {
      break;
    }
    i++;
    MOZ_ASSERT(i <= mIdleObservers.Length(), "Array index out of bounds error.");
  }

  return i;
}

nsresult
nsGlobalWindow::RegisterIdleObserver(nsIIdleObserver* aIdleObserver)
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");

  nsresult rv;
  if (mIdleObservers.IsEmpty()) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mIdleService->AddIdleObserver(mObserver, MIN_IDLE_NOTIFICATION_TIME_S);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mIdleTimer) {
      mIdleTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      mIdleTimer->Cancel();
    }
  }

  MOZ_ASSERT(mIdleService);
  MOZ_ASSERT(mIdleTimer);

  IdleObserverHolder tmpIdleObserver;
  tmpIdleObserver.mIdleObserver = aIdleObserver;
  rv = aIdleObserver->GetTime(&tmpIdleObserver.mTimeInS);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_MAX(tmpIdleObserver.mTimeInS, UINT32_MAX / 1000);
  NS_ENSURE_ARG_MIN(tmpIdleObserver.mTimeInS, MIN_IDLE_NOTIFICATION_TIME_S);

  uint32_t insertAtIndex = FindInsertionIndex(&tmpIdleObserver);
  if (insertAtIndex == mIdleObservers.Length()) {
    mIdleObservers.AppendElement(tmpIdleObserver);
  }
  else {
    mIdleObservers.InsertElementAt(insertAtIndex, tmpIdleObserver);
  }

  bool userIsIdle = false;
  rv = nsContentUtils::IsUserIdle(MIN_IDLE_NOTIFICATION_TIME_S, &userIsIdle);
  NS_ENSURE_SUCCESS(rv, rv);

  // Special case. First idle observer added to empty list while the user is idle.
  // Haven't received 'idle' topic notification from slow idle service yet.
  // Need to wait for the idle notification and then notify idle observers in the list.
  if (userIsIdle && mIdleCallbackIndex == -1) {
    return NS_OK;
  }

  if (!mCurrentlyIdle) {
    return NS_OK;
  }

  MOZ_ASSERT(mIdleCallbackIndex >= 0);

  if (static_cast<int32_t>(insertAtIndex) < mIdleCallbackIndex) {
    IdleObserverHolder& idleObserver = mIdleObservers.ElementAt(insertAtIndex);
    NotifyIdleObserver(&idleObserver, true);
    mIdleCallbackIndex++;
    return NS_OK;
  }

  if (static_cast<int32_t>(insertAtIndex) == mIdleCallbackIndex) {
    mIdleTimer->Cancel();
    rv = ScheduleNextIdleObserverCallback();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsGlobalWindow::FindIndexOfElementToRemove(nsIIdleObserver* aIdleObserver,
                                           int32_t* aRemoveElementIndex)
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");
  MOZ_ASSERT(aIdleObserver, "Idle observer not instantiated.");

  *aRemoveElementIndex = 0;
  if (mIdleObservers.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  uint32_t aIdleObserverTimeInS;
  nsresult rv = aIdleObserver->GetTime(&aIdleObserverTimeInS);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_MIN(aIdleObserverTimeInS, MIN_IDLE_NOTIFICATION_TIME_S);

  nsTObserverArray<IdleObserverHolder>::ForwardIterator iter(mIdleObservers);
  while (iter.HasMore()) {
    IdleObserverHolder& idleObserver = iter.GetNext();
    if (idleObserver.mTimeInS == aIdleObserverTimeInS &&
        idleObserver.mIdleObserver == aIdleObserver ) {
      break;
    }
    (*aRemoveElementIndex)++;
  }
  return static_cast<uint32_t>(*aRemoveElementIndex) >= mIdleObservers.Length() ?
    NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsGlobalWindow::UnregisterIdleObserver(nsIIdleObserver* aIdleObserver)
{
  MOZ_ASSERT(IsInnerWindow(), "Must be an inner window!");

  int32_t removeElementIndex;
  nsresult rv = FindIndexOfElementToRemove(aIdleObserver, &removeElementIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("Idle observer not found in list of idle observers. No idle observer removed.");
    return NS_OK;
  }
  mIdleObservers.RemoveElementAt(removeElementIndex);

  MOZ_ASSERT(mIdleTimer);
  if (mIdleObservers.IsEmpty() && mIdleService) {
    rv = mIdleService->RemoveIdleObserver(mObserver, MIN_IDLE_NOTIFICATION_TIME_S);
    NS_ENSURE_SUCCESS(rv, rv);
    mIdleService = nullptr;

    mIdleTimer->Cancel();
    mIdleCallbackIndex = -1;
    return NS_OK;
  }

  if (!mCurrentlyIdle) {
    return NS_OK;
  }

  if (removeElementIndex < mIdleCallbackIndex) {
    mIdleCallbackIndex--;
    return NS_OK;
  }

  if (removeElementIndex != mIdleCallbackIndex) {
    return NS_OK;
  }

  mIdleTimer->Cancel();

  // If the last element in the array had been notified then decrement
  // mIdleCallbackIndex because an idle was removed from the list of
  // idle observers.
  // Example: add idle observer with time 1, 2, 3,
  // Idle notifications for idle observers with time 1, 2, 3 are complete
  // Remove idle observer with time 3 while the user is still idle.
  // The user never transitioned to active state.
  // Add an idle observer with idle time 4
  if (static_cast<uint32_t>(mIdleCallbackIndex) == mIdleObservers.Length()) {
    mIdleCallbackIndex--;
  }
  rv = ScheduleNextIdleObserverCallback();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsGlobalWindow::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData)
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

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    mCurrentlyIdle = true;
    if (IsFrozen()) {
      // need to fire only one idle event while the window is frozen.
      mNotifyIdleObserversIdleOnThaw = true;
      mNotifyIdleObserversActiveOnThaw = false;
    } else if (IsCurrentInnerWindow()) {
      HandleIdleActiveEvent();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_ACTIVE)) {
    mCurrentlyIdle = false;
    if (IsFrozen()) {
      mNotifyIdleObserversActiveOnThaw = true;
      mNotifyIdleObserversIdleOnThaw = false;
    } else if (IsCurrentInnerWindow()) {
      MOZ_ASSERT(IsInnerWindow());
      ScheduleActiveTimerCallback();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "dom-storage2-changed")) {
    if (!IsInnerWindow() || !IsCurrentInnerWindow()) {
      return NS_OK;
    }

    nsIPrincipal *principal;
    nsresult rv;

    nsCOMPtr<nsIDOMStorageEvent> event = do_QueryInterface(aSubject, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMStorage> changingStorage;
    rv = event->GetStorageArea(getter_AddRefs(changingStorage));
    NS_ENSURE_SUCCESS(rv, rv);

    bool fireMozStorageChanged = false;
    principal = GetPrincipal();
    if (!principal) {
      return NS_OK;
    }

    nsCOMPtr<nsPIDOMStorage> pistorage = do_QueryInterface(changingStorage);

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
    bool isPrivate = loadContext && loadContext->UsePrivateBrowsing();
    if (pistorage->IsPrivate() != isPrivate) {
      return NS_OK;
    }

    switch (pistorage->GetType())
    {
    case nsPIDOMStorage::SessionStorage:
    {
      bool check = false;

      nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(GetDocShell());
      if (storageManager) {
        rv = storageManager->CheckStorage(principal, changingStorage, &check);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      if (!check) {
        // This storage event is not coming from our storage or is coming
        // from a different docshell, i.e. it is a clone, ignore this event.
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
      nsIPrincipal* storagePrincipal = pistorage->GetPrincipal();

      bool equals = false;
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

    event->SetTrusted(true);

    if (fireMozStorageChanged) {
      WidgetEvent* internalEvent = event->GetInternalNSEvent();
      internalEvent->mFlags.mOnlyChromeDispatch = true;
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

#ifdef MOZ_B2G
  if (!nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC) ||
      !nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC)) {
    MOZ_ASSERT(IsInnerWindow());
    if (!IsCurrentInnerWindow()) {
      return NS_OK;
    }

    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
    nsresult rv = event->InitEvent(
      !nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC)
        ? NETWORK_UPLOAD_EVENT_NAME
        : NETWORK_DOWNLOAD_EVENT_NAME,
      false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    event->SetTrusted(true);

    bool dummy;
    return DispatchEvent(event, &dummy);
  }
#endif // MOZ_B2G

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

  NS_NewDOMStorageEvent(getter_AddRefs(domEvent), this, nullptr, nullptr);
  aEvent = do_QueryInterface(domEvent);
  return aEvent->InitStorageEvent(aType, canBubble, cancelable,
                                  key, oldValue, newValue,
                                  url, storageArea);
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

  for (int32_t i = 0; i < mPendingStorageEvents.Count(); ++i) {
    Observe(mPendingStorageEvents[i], "dom-storage2-changed", nullptr);
  }

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->FirePendingEvents();
  }

  if (mFireOfflineStatusChangeEventOnThaw) {
    mFireOfflineStatusChangeEventOnThaw = false;
    FireOfflineStatusEvent();
  }

  if (mNotifyIdleObserversIdleOnThaw) {
    mNotifyIdleObserversIdleOnThaw = false;
    HandleIdleActiveEvent();
  }

  if (mNotifyIdleObserversActiveOnThaw) {
    mNotifyIdleObserversActiveOnThaw = false;
    ScheduleActiveTimerCallback();
  }

  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    int32_t childCount = 0;
    docShell->GetChildCount(&childCount);

    for (int32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      docShell->GetChildAt(i, getter_AddRefs(childShell));
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
  if (IsInnerWindow()) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    if (!outer) {
      NS_WARNING("No outer window available!");
      return nullptr;
    }
    return outer->GetParentInternal();
  }

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (parent && parent != static_cast<nsIDOMWindow *>(this)) {
    return parent;
  }

  return nullptr;
}

void
nsGlobalWindow::UnblockScriptedClosing()
{
  MOZ_ASSERT(IsOuterWindow());
  mBlockScriptedClosingFlag = false;
}

class AutoUnblockScriptClosing
{
private:
  nsRefPtr<nsGlobalWindow> mWin;
public:
  AutoUnblockScriptClosing(nsGlobalWindow* aWin)
    : mWin(aWin)
  {
    MOZ_ASSERT(mWin);
    MOZ_ASSERT(mWin->IsOuterWindow());
  }
  ~AutoUnblockScriptClosing()
  {
    void (nsGlobalWindow::*run)() = &nsGlobalWindow::UnblockScriptedClosing;
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(mWin, run));
  }
};

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, bool aDialog,
                             bool aContentModal, bool aCalledNoScript,
                             bool aDoJSFixups, bool aNavigate,
                             nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsIPrincipal *aCalleePrincipal,
                             JSContext *aJSCallerContext,
                             nsIDOMWindow **aReturn)
{
  FORWARD_TO_OUTER(OpenInternal, (aUrl, aName, aOptions, aDialog,
                                  aContentModal, aCalledNoScript, aDoJSFixups,
                                  aNavigate, argv, aExtraArgument,
                                  aCalleePrincipal, aJSCallerContext, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

#ifdef DEBUG
  uint32_t argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif
  NS_PRECONDITION(!aExtraArgument || (!argv && argc == 0),
                  "Can't pass in arguments both ways");
  NS_PRECONDITION(!aCalledNoScript || (!argv && argc == 0),
                  "Can't pass JS args when called via the noscript methods");
  NS_PRECONDITION(!aJSCallerContext || !aCalledNoScript,
                  "Shouldn't have caller context when called noscript");

  mozilla::Maybe<AutoUnblockScriptClosing> closeUnblocker;

  // Calls to window.open from script should navigate.
  MOZ_ASSERT(aCalledNoScript || aNavigate);

  *aReturn = nullptr;

  nsCOMPtr<nsIWebBrowserChrome> chrome = GetWebBrowserChrome();
  if (!chrome) {
    // No chrome means we don't want to go through with this open call
    // -- see nsIWindowWatcher.idl
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mDocShell, "Must have docshell here");

  // Popups from apps are never blocked.
  bool isApp = false;
  if (mDoc) {
    isApp = mDoc->NodePrincipal()->GetAppStatus() >=
              nsIPrincipal::APP_STATUS_INSTALLED;
  }

  const bool checkForPopup = !nsContentUtils::IsCallerChrome() &&
    !isApp && !aDialog && !WindowExists(aName, !aCalledNoScript);

  // Note: it's very important that this be an nsXPIDLCString, since we want
  // .get() on it to return nullptr until we write stuff to it.  The window
  // watcher expects a null URL string if there is no URL to load.
  nsXPIDLCString url;
  nsresult rv = NS_OK;

  // It's important to do this security check before determining whether this
  // window opening should be blocked, to ensure that we don't FireAbuseEvents
  // for a window opening that wouldn't have succeeded in the first place.
  if (!aUrl.IsEmpty()) {
    AppendUTF16toUTF8(aUrl, url);

    // It's safe to skip the security check below if we're not a dialog
    // because window.openDialog is not callable from content script.  See bug
    // 56851.
    //
    // If we're not navigating, we assume that whoever *does* navigate the
    // window will do a security check of their own.
    if (url.get() && !aDialog && aNavigate)
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
          closeUnblocker.construct(this);
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

  const char *options_ptr = aOptions.IsEmpty() ? nullptr : options.get();
  const char *name_ptr = aName.IsEmpty() ? nullptr : name.get();

  nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
  NS_ENSURE_STATE(pwwatch);

  {
    // Reset popup state while opening a window to prevent the
    // current state from being active the whole time a modal
    // dialog is open.
    nsAutoPopupStatePusher popupStatePusher(openAbused, true);

    if (!aCalledNoScript) {
      // We asserted at the top of this function that aNavigate is true for
      // !aCalledNoScript.
      rv = pwwatch->OpenWindow2(this, url.get(), name_ptr, options_ptr,
                                /* aCalledFromScript = */ true,
                                aDialog, aNavigate, argv,
                                getter_AddRefs(domReturn));
    } else {
      // Force a system caller here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindow::OpenWindow and nsIWindowWatcher::OpenWindow.

      // Note: Because nsWindowWatcher is so broken, it's actually important
      // that we don't force a system caller here, because that screws it up
      // when it tries to compute the caller principal to associate with dialog
      // arguments. That whole setup just really needs to be rewritten. :-(
      Maybe<AutoNoJSAPI> nojsapi;
      if (!aContentModal) {
        nojsapi.construct();
      }


      rv = pwwatch->OpenWindow2(this, url.get(), name_ptr, options_ptr,
                                /* aCalledFromScript = */ false,
                                aDialog, aNavigate, aExtraArgument,
                                getter_AddRefs(domReturn));

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
        NS_ASSERTION(pidomwin->GetExtantDoc(), "No document in new window!!!");
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

//*****************************************************************************
// nsGlobalWindow: Timeout Functions
//*****************************************************************************

uint32_t sNestingLevel;

nsGlobalWindow*
nsGlobalWindow::InnerForSetTimeoutOrInterval(ErrorResult& aError)
{
  nsGlobalWindow* currentInner;
  nsGlobalWindow* forwardTo;
  if (IsInnerWindow()) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    currentInner = outer ? outer->GetCurrentInnerWindowInternal() : this;

    forwardTo = this;
  } else {
    currentInner = GetCurrentInnerWindowInternal();

    // This needs to forward to the inner window, but since the current
    // inner may not be the inner in the calling scope, we need to treat
    // this specially here as we don't want timeouts registered in a
    // dying inner window to get registered and run on the current inner
    // window. To get this right, we need to forward this call to the
    // inner window that's calling window.setTimeout().

    forwardTo = CallerInnerWindow();
    if (!forwardTo) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    // If the caller and the callee share the same outer window, forward to the
    // caller inner. Else, we forward to the current inner (e.g. someone is
    // calling setTimeout() on a reference to some other window).
    if (forwardTo->GetOuterWindow() != this || !forwardTo->IsInnerWindow()) {
      if (!currentInner) {
        NS_WARNING("No inner window available!");
        aError.Throw(NS_ERROR_NOT_INITIALIZED);
        return nullptr;
      }

      return currentInner;
    }
  }

  // If forwardTo is not the window with an active document then we want the
  // call to setTimeout/Interval to be a noop, so return null but don't set an
  // error.
  return forwardTo->HasActiveDocument() ? currentInner : nullptr;
}

int32_t
nsGlobalWindow::SetTimeout(JSContext* aCx, Function& aFunction,
                           int32_t aTimeout,
                           const Sequence<JS::Value>& aArguments,
                           ErrorResult& aError)
{
  return SetTimeoutOrInterval(aFunction, aTimeout, aArguments, false, aError);
}

int32_t
nsGlobalWindow::SetTimeout(JSContext* aCx, const nsAString& aHandler,
                           int32_t aTimeout,
                           const Sequence<JS::Value>& /* unused */,
                           ErrorResult& aError)
{
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, false, aError);
}

static bool
IsInterval(const Optional<int32_t>& aTimeout, int32_t& aResultTimeout)
{
  if (aTimeout.WasPassed()) {
    aResultTimeout = aTimeout.Value();
    return true;
  }

  // If no interval was specified, treat this like a timeout, to avoid setting
  // an interval of 0 milliseconds.
  aResultTimeout = 0;
  return false;
}

int32_t
nsGlobalWindow::SetInterval(JSContext* aCx, Function& aFunction,
                            const Optional<int32_t>& aTimeout,
                            const Sequence<JS::Value>& aArguments,
                            ErrorResult& aError)
{
  int32_t timeout;
  bool isInterval = IsInterval(aTimeout, timeout);
  return SetTimeoutOrInterval(aFunction, timeout, aArguments, isInterval,
                              aError);
}

int32_t
nsGlobalWindow::SetInterval(JSContext* aCx, const nsAString& aHandler,
                            const Optional<int32_t>& aTimeout,
                            const Sequence<JS::Value>& /* unused */,
                            ErrorResult& aError)
{
  int32_t timeout;
  bool isInterval = IsInterval(aTimeout, timeout);
  return SetTimeoutOrInterval(aCx, aHandler, timeout, isInterval, aError);
}

nsresult
nsGlobalWindow::SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                     int32_t interval,
                                     bool aIsInterval, int32_t *aReturn)
{
  MOZ_ASSERT(IsInnerWindow());

  // If we don't have a document (we could have been unloaded since
  // the call to setTimeout was made), do nothing.
  if (!mDoc) {
    return NS_OK;
  }

  // Disallow negative intervals.  If aIsInterval also disallow 0,
  // because we use that as a "don't repeat" flag.
  interval = std::max(aIsInterval ? 1 : 0, interval);

  // Make sure we don't proceed with an interval larger than our timer
  // code can handle. (Note: we already forced |interval| to be non-negative,
  // so the uint32_t cast (to avoid compiler warnings) is ok.)
  uint32_t maxTimeoutMs = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  if (static_cast<uint32_t>(interval) > maxTimeoutMs) {
    interval = maxTimeoutMs;
  }

  nsRefPtr<nsTimeout> timeout = new nsTimeout();
  timeout->mIsInterval = aIsInterval;
  timeout->mInterval = interval;
  timeout->mScriptHandler = aHandler;

  // Now clamp the actual interval we will use for the timer based on
  uint32_t nestingLevel = sNestingLevel + 1;
  uint32_t realInterval = interval;
  if (aIsInterval || nestingLevel >= DOM_CLAMP_TIMEOUT_NESTING_LEVEL) {
    // Don't allow timeouts less than DOMMinTimeoutValue() from
    // now...
    realInterval = std::max(realInterval, uint32_t(DOMMinTimeoutValue()));
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

    rv = timeout->InitTimer(TimerCallback, realInterval);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // The timeout is now also held in the timer's closure.
    unused << copy.forget();
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

    int32_t delay =
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
nsGlobalWindow::SetTimeoutOrInterval(bool aIsInterval, int32_t *aReturn)
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

  int32_t interval = 0;
  bool isInterval = aIsInterval;
  nsCOMPtr<nsIScriptTimeoutHandler> handler;
  nsresult rv = NS_CreateJSTimeoutHandler(this,
                                          &isInterval,
                                          &interval,
                                          getter_AddRefs(handler));
  if (!handler) {
    *aReturn = 0;
    return rv;
  }

  return SetTimeoutOrInterval(handler, interval, isInterval, aReturn);
}

int32_t
nsGlobalWindow::SetTimeoutOrInterval(Function& aFunction, int32_t aTimeout,
                                     const Sequence<JS::Value>& aArguments,
                                     bool aIsInterval, ErrorResult& aError)
{
  nsGlobalWindow* inner = InnerForSetTimeoutOrInterval(aError);
  if (!inner) {
    return -1;
  }

  if (inner != this) {
    return inner->SetTimeoutOrInterval(aFunction, aTimeout, aArguments,
                                       aIsInterval, aError);
  }

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(this, aFunction, aArguments, aError);
  if (!handler) {
    return 0;
  }

  int32_t result;
  aError = SetTimeoutOrInterval(handler, aTimeout, aIsInterval, &result);
  return result;
}

int32_t
nsGlobalWindow::SetTimeoutOrInterval(JSContext* aCx, const nsAString& aHandler,
                                     int32_t aTimeout, bool aIsInterval,
                                     ErrorResult& aError)
{
  nsGlobalWindow* inner = InnerForSetTimeoutOrInterval(aError);
  if (!inner) {
    return -1;
  }

  if (inner != this) {
    return inner->SetTimeoutOrInterval(aCx, aHandler, aTimeout, aIsInterval,
                                       aError);
  }

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, this, aHandler, aError);
  if (!handler) {
    return 0;
  }

  int32_t result;
  aError = SetTimeoutOrInterval(handler, aTimeout, aIsInterval, &result);
  return result;
}

bool
nsGlobalWindow::RunTimeoutHandler(nsTimeout* aTimeout,
                                  nsIScriptContext* aScx)
{
  // Hold on to the timeout in case mExpr or mFunObj releases its
  // doc.
  nsRefPtr<nsTimeout> timeout = aTimeout;
  nsTimeout* last_running_timeout = mRunningTimeout;
  mRunningTimeout = timeout;
  timeout->mRunning = true;

  // Push this timeout's popup control state, which should only be
  // eabled the first time a timeout fires that was created while
  // popups were enabled and with a delay less than
  // "dom.disable_open_click_delay".
  nsAutoPopupStatePusher popupStatePusher(timeout->mPopupState);

  // Clear the timeout's popup state, if any, to prevent interval
  // timeouts from repeatedly opening poups.
  timeout->mPopupState = openAbused;

  ++gRunningTimeoutDepth;
  ++mTimeoutFiringDepth;

  bool trackNestingLevel = !timeout->mIsInterval;
  uint32_t nestingLevel;
  if (trackNestingLevel) {
    nestingLevel = sNestingLevel;
    sNestingLevel = timeout->mNestingLevel;
  }

  nsCOMPtr<nsIScriptTimeoutHandler> handler(timeout->mScriptHandler);
  nsRefPtr<Function> callback = handler->GetCallback();
  if (!callback) {
    // Evaluate the timeout expression.
    const char16_t* script = handler->GetHandlerText();
    NS_ASSERTION(script, "timeout has no script nor handler text!");

    const char* filename = nullptr;
    uint32_t lineNo = 0;
    handler->GetLocation(&filename, &lineNo);

    // New script entry point required, due to the "Create a script" sub-step of
    // http://www.whatwg.org/specs/web-apps/current-work/#timer-initialization-steps
    AutoEntryScript entryScript(this, true, aScx->GetNativeContext());
    JS::CompileOptions options(entryScript.cx());
    options.setFileAndLine(filename, lineNo)
           .setVersion(JSVERSION_DEFAULT);
    JS::Rooted<JSObject*> global(entryScript.cx(), FastGetGlobalJSObject());
    nsJSUtils::EvaluateString(entryScript.cx(), nsDependentString(script),
                              global, options);
  } else {
    // Hold strong ref to ourselves while we call the callback.
    nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow *>(this));
    ErrorResult ignored;
    callback->Call(me, handler->GetArgs(), ignored);
  }

  // We ignore any failures from calling EvaluateString() on the context or
  // Call() on a Function here since we're in a loop
  // where we're likely to be running timeouts whose OS timers
  // didn't fire in time and we don't want to not fire those timers
  // now just because execution of one timer failed. We can't
  // propagate the error to anyone who cares about it from this
  // point anyway, and the script context should have already reported
  // the script error in the usual way - so we just drop it.

  if (trackNestingLevel) {
    sNestingLevel = nestingLevel;
  }

  --mTimeoutFiringDepth;
  --gRunningTimeoutDepth;

  mRunningTimeout = last_running_timeout;
  timeout->mRunning = false;
  return timeout->mCleared;
}

bool
nsGlobalWindow::RescheduleTimeout(nsTimeout* aTimeout, const TimeStamp& now,
                                  bool aRunningPendingTimeouts)
{
  if (!aTimeout->mIsInterval) {
    if (aTimeout->mTimer) {
      // The timeout still has an OS timer, and it's not an interval,
      // that means that the OS timer could still fire; cancel the OS
      // timer and release its reference to the timeout.
      aTimeout->mTimer->Cancel();
      aTimeout->mTimer = nullptr;
      aTimeout->Release();
    }
    return false;
  }

  // Compute time to next timeout for interval timer.
  // Make sure nextInterval is at least DOMMinTimeoutValue().
  TimeDuration nextInterval =
    TimeDuration::FromMilliseconds(std::max(aTimeout->mInterval,
                                          uint32_t(DOMMinTimeoutValue())));

  // If we're running pending timeouts, set the next interval to be
  // relative to "now", and not to when the timeout that was pending
  // should have fired.
  TimeStamp firingTime;
  if (aRunningPendingTimeouts) {
    firingTime = now + nextInterval;
  } else {
    firingTime = aTimeout->mWhen + nextInterval;
  }

  TimeStamp currentNow = TimeStamp::Now();
  TimeDuration delay = firingTime - currentNow;

  // And make sure delay is nonnegative; that might happen if the timer
  // thread is firing our timers somewhat early or if they're taking a long
  // time to run the callback.
  if (delay < TimeDuration(0)) {
    delay = TimeDuration(0);
  }

  if (!aTimeout->mTimer) {
    NS_ASSERTION(IsFrozen() || mTimeoutsSuspendDepth,
                 "How'd our timer end up null if we're not frozen or "
                 "suspended?");

    aTimeout->mTimeRemaining = delay;
    return true;
  }

  aTimeout->mWhen = currentNow + delay;

  // Reschedule the OS timer. Don't bother returning any error codes if
  // this fails since the callers of this method don't care about them.
  nsresult rv = aTimeout->InitTimer(TimerCallback, delay.ToMilliseconds());

  if (NS_FAILED(rv)) {
    NS_ERROR("Error initializing timer for DOM timeout!");

    // We failed to initialize the new OS timer, this timer does
    // us no good here so we just cancel it (just in case) and
    // null out the pointer to the OS timer, this will release the
    // OS timer. As we continue executing the code below we'll end
    // up deleting the timeout since it's not an interval timeout
    // any more (since timeout->mTimer == nullptr).
    aTimeout->mTimer->Cancel();
    aTimeout->mTimer = nullptr;

    // Now that the OS timer no longer has a reference to the
    // timeout we need to drop that reference.
    aTimeout->Release();

    return false;
  }

  return true;
}

void
nsGlobalWindow::RunTimeout(nsTimeout *aTimeout)
{
  // If a modal dialog is open for this window, return early. Pending
  // timeouts will run when the modal dialog is dismissed.
  if (IsInModalState() || mTimeoutsSuspendDepth) {
    return;
  }

  NS_ASSERTION(IsInnerWindow(), "Timeout running on outer window!");
  NS_ASSERTION(!IsFrozen(), "Timeout running on a window in the bfcache!");

  nsTimeout *nextTimeout;
  nsTimeout *last_expired_timeout, *last_insertion_point;
  uint32_t firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);

  // A native timer has gone off. See which of our timeouts need
  // servicing
  TimeStamp now = TimeStamp::Now();
  TimeStamp deadline;

  if (aTimeout && aTimeout->mWhen > now) {
    // The OS timer fired early (which can happen due to the timers
    // having lower precision than TimeStamp does).  Set |deadline| to
    // be the time when the OS timer *should* have fired so that any
    // timers that *should* have fired before aTimeout *will* be fired
    // now.

    deadline = aTimeout->mWhen;
  } else {
    deadline = now;
  }

  // The timeout list is kept in deadline order. Discover the latest
  // timeout whose deadline has expired. On some platforms, native
  // timeout events fire "early", so we need to test the timer as well
  // as the deadline.
  last_expired_timeout = nullptr;
  for (nsTimeout *timeout = mTimeouts.getFirst(); timeout; timeout = timeout->getNext()) {
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
    uint32_t count = gTimeoutsRecentlySet;
    gTimeoutsRecentlySet = 0;
    Telemetry::Accumulate(Telemetry::DOM_TIMERS_RECENTLY_SET, count);
    gLastRecordedRecentTimeouts = now;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  nsRefPtr<nsTimeout> dummy_timeout = new nsTimeout();
  dummy_timeout->mFiringDepth = firingDepth;
  dummy_timeout->mWhen = now;
  last_expired_timeout->setNext(dummy_timeout);
  dummy_timeout->AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  // If we ever start setting mTimeoutInsertionPoint to a non-dummy timeout,
  // the logic in ResetTimersForNonBackgroundWindow will need to change.
  mTimeoutInsertionPoint = dummy_timeout;

  Telemetry::AutoCounter<Telemetry::DOM_TIMERS_FIRED_PER_NATIVE_TIMEOUT> timeoutsRan;

  for (nsTimeout *timeout = mTimeouts.getFirst();
       timeout != dummy_timeout && !IsFrozen();
       timeout = nextTimeout) {
    nextTimeout = timeout->getNext();

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

    // This timeout is good to run
    ++timeoutsRan;
    bool timeout_was_cleared = RunTimeoutHandler(timeout, scx);

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.
      MOZ_ASSERT(dummy_timeout->HasRefCntOne(), "dummy_timeout may leak");

      mTimeoutInsertionPoint = last_insertion_point;

      return;
    }

    // If we have a regular interval timer, we re-schedule the
    // timeout, accounting for clock drift.
    bool needsReinsertion = RescheduleTimeout(timeout, now, !aTimeout);

    // Running a timeout can cause another timeout to be deleted, so
    // we need to reset the pointer to the following timeout.
    nextTimeout = timeout->getNext();

    timeout->remove();

    if (needsReinsertion) {
      // Insert interval timeout onto list sorted in deadline order.
      // AddRefs timeout.
      InsertTimeoutIntoList(timeout);
    }

    // Release the timeout struct since it's possibly out of the list
    timeout->Release();
  }

  // Take the dummy timeout off the head of the list
  dummy_timeout->remove();
  dummy_timeout->Release();
  MOZ_ASSERT(dummy_timeout->HasRefCntOne(), "dummy_timeout may leak");

  mTimeoutInsertionPoint = last_insertion_point;
}

void
nsGlobalWindow::ClearTimeoutOrInterval(int32_t aTimerID, ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(ClearTimeoutOrInterval, (aTimerID, aError),
                            aError, );

  uint32_t public_id = (uint32_t)aTimerID;
  nsTimeout *timeout;

  for (timeout = mTimeouts.getFirst(); timeout; timeout = timeout->getNext()) {
    if (timeout->mPublicId == public_id) {
      if (timeout->mRunning) {
        /* We're running from inside the timeout. Mark this
           timeout for deferred deletion by the code in
           RunTimeout() */
        timeout->mIsInterval = false;
      }
      else {
        /* Delete the timeout from the pending timeout list */
        timeout->remove();

        if (timeout->mTimer) {
          timeout->mTimer->Cancel();
          timeout->mTimer = nullptr;
          timeout->Release();
        }
        timeout->Release();
      }
      break;
    }
  }
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
         mTimeoutInsertionPoint->getNext() : mTimeouts.getFirst();
       timeout; ) {
    // It's important that this check be <= so that we guarantee that
    // taking std::max with |now| won't make a quantity equal to
    // timeout->mWhen below.
    if (timeout->mWhen <= now) {
      timeout = timeout->getNext();
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
      TimeDuration::FromMilliseconds(std::max(timeout->mInterval,
                                            uint32_t(DOMMinTimeoutValue())));
    uint32_t oldIntervalMillisecs = 0;
    timeout->mTimer->GetDelay(&oldIntervalMillisecs);
    TimeDuration oldInterval = TimeDuration::FromMilliseconds(oldIntervalMillisecs);
    if (oldInterval > interval) {
      // unclamp
      TimeStamp firingTime =
        std::max(timeout->mWhen - oldInterval + interval, now);

      NS_ASSERTION(firingTime < timeout->mWhen,
                   "Our firing time should strictly decrease!");

      TimeDuration delay = firingTime - now;
      timeout->mWhen = firingTime;

      // Since we reset mWhen we need to move |timeout| to the right
      // place in the list so that it remains sorted by mWhen.
      
      // Get the pointer to the next timeout now, before we move the
      // current timeout in the list.
      nsTimeout* nextTimeout = timeout->getNext();

      // It is safe to remove and re-insert because mWhen is now
      // strictly smaller than it used to be, so we know we'll insert
      // |timeout| before nextTimeout.
      NS_ASSERTION(!nextTimeout ||
                   timeout->mWhen < nextTimeout->mWhen, "How did that happen?");
      timeout->remove();
      // InsertTimeoutIntoList will addref |timeout| and reset
      // mFiringDepth.  Make sure to undo that after calling it.
      uint32_t firingDepth = timeout->mFiringDepth;
      InsertTimeoutIntoList(timeout);
      timeout->mFiringDepth = firingDepth;
      timeout->Release();

      nsresult rv = timeout->InitTimer(TimerCallback, delay.ToMilliseconds());

      if (NS_FAILED(rv)) {
        NS_WARNING("Error resetting non background timer for DOM timeout!");
        return rv;
      }

      timeout = nextTimeout;
    } else {
      timeout = timeout->getNext();
    }
  }

  return NS_OK;
}

void
nsGlobalWindow::ClearAllTimeouts()
{
  nsTimeout *timeout, *nextTimeout;

  for (timeout = mTimeouts.getFirst(); timeout; timeout = nextTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nullptr;

    nextTimeout = timeout->getNext();

    if (timeout->mTimer) {
      timeout->mTimer->Cancel();
      timeout->mTimer = nullptr;

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
  mTimeouts.clear();
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
  for (prevSibling = mTimeouts.getLast();
       prevSibling && prevSibling != mTimeoutInsertionPoint &&
         // This condition needs to match the one in SetTimeoutOrInterval that
         // determines whether to set mWhen or mTimeRemaining.
         ((IsFrozen() || mTimeoutsSuspendDepth) ?
          prevSibling->mTimeRemaining > aTimeout->mTimeRemaining :
          prevSibling->mWhen > aTimeout->mWhen);
       prevSibling = prevSibling->getPrevious()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  if (prevSibling) {
    prevSibling->setNext(aTimeout);
  } else {
    mTimeouts.insertFront(aTimeout);
  }

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

already_AddRefed<nsIDocShellTreeOwner>
nsGlobalWindow::GetTreeOwner()
{
  FORWARD_TO_OUTER(GetTreeOwner, (), nullptr);

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  return treeOwner.forget();
}

already_AddRefed<nsIBaseWindow>
nsGlobalWindow::GetTreeOwnerWindow()
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no mDocShell, this window must have been closed,
  // in that case there is no tree owner.

  if (mDocShell) {
    mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  return baseWindow.forget();
}

already_AddRefed<nsIWebBrowserChrome>
nsGlobalWindow::GetWebBrowserChrome()
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_GetInterface(treeOwner);
  return browserChrome.forget();
}

nsIScrollableFrame *
nsGlobalWindow::GetScrollFrame()
{
  FORWARD_TO_OUTER(GetScrollFrame, (), nullptr);

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (presShell) {
    return presShell->GetRootScrollFrameAsScrollable();
  }
  return nullptr;
}

nsresult
nsGlobalWindow::SecurityCheckURL(const char *aURL)
{
  nsCOMPtr<nsPIDOMWindow> sourceWindow;
  JSContext* topCx = nsContentUtils::GetCurrentJSContext();
  if (topCx) {
    sourceWindow = do_QueryInterface(nsJSUtils::GetDynamicScriptGlobal(topCx));
  }
  if (!sourceWindow) {
    sourceWindow = this;
  }
  AutoJSContext cx;
  nsGlobalWindow* sourceWin = static_cast<nsGlobalWindow*>(sourceWindow.get());
  JSAutoCompartment ac(cx, sourceWin->GetGlobalJSObject());

  // Resolve the baseURI, which could be relative to the calling window.
  //
  // Note the algorithm to get the base URI should match the one
  // used to actually kick off the load in nsWindowWatcher.cpp.
  nsCOMPtr<nsIDocument> doc = sourceWindow->GetDoc();
  nsIURI* baseURI = nullptr;
  nsAutoCString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  if (doc) {
    baseURI = doc->GetDocBaseURI();
    charset = doc->GetDocumentCharacterSet();
  }
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURL),
                          charset.get(), baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_FAILED(nsContentUtils::GetSecurityManager()->
        CheckLoadURIFromScript(cx, uri))) {
    return NS_ERROR_FAILURE;
  }

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
  MOZ_ASSERT(IsOuterWindow());

  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindow *parent =
    static_cast<nsGlobalWindow *>(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }
}

already_AddRefed<nsISupports>
nsGlobalWindow::SaveWindowState()
{
  NS_PRECONDITION(IsOuterWindow(), "Can't save the inner window's state");

  if (!mContext || !GetWrapperPreserveColor()) {
    // The window may be getting torn down; don't bother saving state.
    return nullptr;
  }

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();
  NS_ASSERTION(inner, "No inner window to save");

  // Don't do anything else to this inner window! After this point, all
  // calls to SetTimeoutOrInterval will create entries in the timeout
  // list that will only run after this window has come out of the bfcache.
  // Also, while we're frozen, we won't dispatch online/offline events
  // to the page.
  inner->Freeze();

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner);

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  return state.forget();
}

nsresult
nsGlobalWindow::RestoreWindowState(nsISupports *aState)
{
  NS_ASSERTION(IsOuterWindow(), "Cannot restore an inner window");

  if (!mContext || !GetWrapperPreserveColor()) {
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
nsGlobalWindow::SuspendTimeouts(uint32_t aIncrease,
                                bool aFreezeChildren)
{
  FORWARD_TO_INNER_VOID(SuspendTimeouts, (aIncrease, aFreezeChildren));

  bool suspended = (mTimeoutsSuspendDepth != 0);
  mTimeoutsSuspendDepth += aIncrease;

  if (!suspended) {
    nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
    if (ac) {
      for (uint32_t i = 0; i < mEnabledSensors.Length(); i++)
        ac->RemoveWindowListener(mEnabledSensors[i], this);
    }
    DisableGamepadUpdates();

    // Suspend all of the workers for this window.
    mozilla::dom::workers::SuspendWorkersForWindow(this);

    TimeStamp now = TimeStamp::Now();
    for (nsTimeout *t = mTimeouts.getFirst(); t; t = t->getNext()) {
      // Set mTimeRemaining to be the time remaining for this timer.
      if (t->mWhen > now)
        t->mTimeRemaining = t->mWhen - now;
      else
        t->mTimeRemaining = TimeDuration(0);
  
      // Drop the XPCOM timer; we'll reschedule when restoring the state.
      if (t->mTimer) {
        t->mTimer->Cancel();
        t->mTimer = nullptr;
  
        // Drop the reference that the timer's closure had on this timeout, we'll
        // add it back in ResumeTimeouts. Note that it shouldn't matter that we're
        // passing null for the context, since this shouldn't actually release this
        // timeout.
        t->Release();
      }
    }

    // Suspend all of the AudioContexts for this window
    for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
      mAudioContexts[i]->Suspend();
    }
  }

  // Suspend our children as well.
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    int32_t childCount = 0;
    docShell->GetChildCount(&childCount);

    for (int32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      docShell->GetChildAt(i, getter_AddRefs(childShell));
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
        nsCOMPtr<Element> frame = pWin->GetFrameElementInternal();
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
  bool shouldResume = (mTimeoutsSuspendDepth == 0) && !mInnerObjectsFreed;
  nsresult rv;

  if (shouldResume) {
    nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
    if (ac) {
      for (uint32_t i = 0; i < mEnabledSensors.Length(); i++)
        ac->AddWindowListener(mEnabledSensors[i], this);
    }
    EnableGamepadUpdates();

    // Resume all of the AudioContexts for this window
    for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
      mAudioContexts[i]->Resume();
    }

    // Resume all of the workers for this window.
    mozilla::dom::workers::ResumeWorkersForWindow(this);

    // Restore all of the timeouts, using the stored time remaining
    // (stored in timeout->mTimeRemaining).

    TimeStamp now = TimeStamp::Now();

#ifdef DEBUG
    bool _seenDummyTimeout = false;
#endif

    for (nsTimeout *t = mTimeouts.getFirst(); t; t = t->getNext()) {
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
      uint32_t delay =
        std::max(int32_t(t->mTimeRemaining.ToMilliseconds()),
               DOMMinTimeoutValue());

      // Set mWhen back to the time when the timer is supposed to
      // fire.
      t->mWhen = now + t->mTimeRemaining;

      t->mTimer = do_CreateInstance("@mozilla.org/timer;1");
      NS_ENSURE_TRUE(t->mTimer, NS_ERROR_OUT_OF_MEMORY);

      rv = t->InitTimer(TimerCallback, delay);
      if (NS_FAILED(rv)) {
        t->mTimer = nullptr;
        return rv;
      }

      // Add a reference for the new timer's closure.
      t->AddRef();
    }
  }

  // Resume our children as well.
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    int32_t childCount = 0;
    docShell->GetChildCount(&childCount);

    for (int32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      docShell->GetChildAt(i, getter_AddRefs(childShell));
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
        nsCOMPtr<Element> frame = pWin->GetFrameElementInternal();
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

uint32_t
nsGlobalWindow::TimeoutSuspendCount()
{
  FORWARD_TO_INNER(TimeoutSuspendCount, (), 0);
  return mTimeoutsSuspendDepth;
}

void
nsGlobalWindow::EnableDeviceSensor(uint32_t aType)
{
  MOZ_ASSERT(IsInnerWindow());

  bool alreadyEnabled = false;
  for (uint32_t i = 0; i < mEnabledSensors.Length(); i++) {
    if (mEnabledSensors[i] == aType) {
      alreadyEnabled = true;
      break;
    }
  }

  mEnabledSensors.AppendElement(aType);

  if (alreadyEnabled) {
    return;
  }

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac) {
    ac->AddWindowListener(aType, this);
  }
}

void
nsGlobalWindow::DisableDeviceSensor(uint32_t aType)
{
  MOZ_ASSERT(IsInnerWindow());

  int32_t doomedElement = -1;
  int32_t listenerCount = 0;
  for (uint32_t i = 0; i < mEnabledSensors.Length(); i++) {
    if (mEnabledSensors[i] == aType) {
      doomedElement = i;
      listenerCount++;
    }
  }

  if (doomedElement == -1) {
    return;
  }

  mEnabledSensors.RemoveElementAt(doomedElement);

  if (listenerCount > 1) {
    return;
  }

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac) {
    ac->RemoveWindowListener(aType, this);
  }
}

void
nsGlobalWindow::SetHasGamepadEventListener(bool aHasGamepad/* = true*/)
{
  FORWARD_TO_INNER_VOID(SetHasGamepadEventListener, (aHasGamepad));
  mHasGamepad = aHasGamepad;
  if (aHasGamepad) {
    EnableGamepadUpdates();
  }
}

void
nsGlobalWindow::EnableTimeChangeNotifications()
{
  mozilla::time::AddWindowListener(this);
}

void
nsGlobalWindow::DisableTimeChangeNotifications()
{
  mozilla::time::RemoveWindowListener(this);
}

static PLDHashOperator
CollectSizeAndListenerCount(
  nsPtrHashKey<DOMEventTargetHelper>* aEntry,
  void *arg)
{
  nsWindowSizes* windowSizes = static_cast<nsWindowSizes*>(arg);

  DOMEventTargetHelper* et = aEntry->GetKey();

  if (nsCOMPtr<nsISizeOfEventTarget> iSizeOf = do_QueryObject(et)) {
    windowSizes->mDOMEventTargetsSize +=
      iSizeOf->SizeOfEventTargetIncludingThis(windowSizes->mMallocSizeOf);
  }

  if (EventListenerManager* elm = et->GetExistingListenerManager()) {
    windowSizes->mDOMEventListenersCount += elm->ListenerCount();
  }

  return PL_DHASH_NEXT;
}

void
nsGlobalWindow::AddSizeOfIncludingThis(nsWindowSizes* aWindowSizes) const
{
  aWindowSizes->mDOMOtherSize += aWindowSizes->mMallocSizeOf(this);

  if (IsInnerWindow()) {
    EventListenerManager* elm = GetExistingListenerManager();
    if (elm) {
      aWindowSizes->mDOMOtherSize +=
        elm->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf);
      aWindowSizes->mDOMEventListenersCount +=
        elm->ListenerCount();
    }
    if (mDoc) {
      mDoc->DocAddSizeOfIncludingThis(aWindowSizes);
    }
  }

  if (mNavigator) {
    aWindowSizes->mDOMOtherSize +=
      mNavigator->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf);
  }

  // The things pointed to by the entries will be measured below, so we
  // use nullptr for the callback here.
  aWindowSizes->mDOMEventTargetsSize +=
    mEventTargetObjects.SizeOfExcludingThis(nullptr,
                                            aWindowSizes->mMallocSizeOf);
  aWindowSizes->mDOMEventTargetsCount +=
    const_cast<nsTHashtable<nsPtrHashKey<DOMEventTargetHelper> >*>
      (&mEventTargetObjects)->EnumerateEntries(CollectSizeAndListenerCount,
                                               aWindowSizes);
}


#ifdef MOZ_GAMEPAD
void
nsGlobalWindow::AddGamepad(uint32_t aIndex, Gamepad* aGamepad)
{
  FORWARD_TO_INNER_VOID(AddGamepad, (aIndex, aGamepad));
  mGamepads.Put(aIndex, aGamepad);
}

void
nsGlobalWindow::RemoveGamepad(uint32_t aIndex)
{
  FORWARD_TO_INNER_VOID(RemoveGamepad, (aIndex));
  mGamepads.Remove(aIndex);
}

// static
PLDHashOperator
nsGlobalWindow::EnumGamepadsForGet(const uint32_t& aKey, Gamepad* aData,
                                   void* aUserArg)
{
  nsTArray<nsRefPtr<Gamepad> >* array =
    static_cast<nsTArray<nsRefPtr<Gamepad> >*>(aUserArg);
  array->EnsureLengthAtLeast(aKey + 1);
  (*array)[aKey] = aData;
  return PL_DHASH_NEXT;
}

void
nsGlobalWindow::GetGamepads(nsTArray<nsRefPtr<Gamepad> >& aGamepads)
{
  FORWARD_TO_INNER_VOID(GetGamepads, (aGamepads));
  aGamepads.Clear();
  // mGamepads.Count() may not be sufficient, but it's not harmful.
  aGamepads.SetCapacity(mGamepads.Count());
  mGamepads.EnumerateRead(EnumGamepadsForGet, &aGamepads);
}

already_AddRefed<Gamepad>
nsGlobalWindow::GetGamepad(uint32_t aIndex)
{
  FORWARD_TO_INNER(GetGamepad, (aIndex), nullptr);
  nsRefPtr<Gamepad> gamepad;
  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

void
nsGlobalWindow::SetHasSeenGamepadInput(bool aHasSeen)
{
  FORWARD_TO_INNER_VOID(SetHasSeenGamepadInput, (aHasSeen));
  mHasSeenGamepadInput = aHasSeen;
}

bool
nsGlobalWindow::HasSeenGamepadInput()
{
  FORWARD_TO_INNER(HasSeenGamepadInput, (), false);
  return mHasSeenGamepadInput;
}

// static
PLDHashOperator
nsGlobalWindow::EnumGamepadsForSync(const uint32_t& aKey, Gamepad* aData,
                                    void* aUserArg)
{
  nsRefPtr<GamepadService> gamepadsvc(GamepadService::GetService());
  gamepadsvc->SyncGamepadState(aKey, aData);
  return PL_DHASH_NEXT;
}

void
nsGlobalWindow::SyncGamepadState()
{
  FORWARD_TO_INNER_VOID(SyncGamepadState, ());
  if (mHasSeenGamepadInput) {
    mGamepads.EnumerateRead(EnumGamepadsForSync, nullptr);
  }
}
#endif
// nsGlobalChromeWindow implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalChromeWindow)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserDOMWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserDOMWindow)
  if (tmp->mMessageManager) {
    static_cast<nsFrameMessageManager*>(
      tmp->mMessageManager.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
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
nsGlobalChromeWindow::GetWindowState(uint16_t* aWindowState)
{
  *aWindowState = WindowState();
  return NS_OK;
}

uint16_t
nsGlobalWindow::WindowState()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  int32_t mode = widget ? widget->SizeMode() : 0;

  switch (mode) {
    case nsSizeMode_Minimized:
      return nsIDOMChromeWindow::STATE_MINIMIZED;
    case nsSizeMode_Maximized:
      return nsIDOMChromeWindow::STATE_MAXIMIZED;
    case nsSizeMode_Fullscreen:
      return nsIDOMChromeWindow::STATE_FULLSCREEN;
    case nsSizeMode_Normal:
      return nsIDOMChromeWindow::STATE_NORMAL;
    default:
      NS_WARNING("Illegal window state for this chrome window");
      break;
  }

  return nsIDOMChromeWindow::STATE_NORMAL;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Maximize()
{
  ErrorResult rv;
  Maximize(rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::Maximize(ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->SetSizeMode(nsSizeMode_Maximized);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  ErrorResult rv;
  Minimize(rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::Minimize(ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->SetSizeMode(nsSizeMode_Minimized);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::Restore()
{
  ErrorResult rv;
  Restore(rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::Restore(ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->SetSizeMode(nsSizeMode_Normal);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttention()
{
  ErrorResult rv;
  GetAttention(rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::GetAttention(ErrorResult& aResult)
{
  return GetAttentionWithCycleCount(-1, aResult);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttentionWithCycleCount(int32_t aCycleCount)
{
  ErrorResult rv;
  GetAttentionWithCycleCount(aCycleCount, rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::GetAttentionWithCycleCount(int32_t aCycleCount,
                                           ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->GetAttention(aCycleCount);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::BeginWindowMove(nsIDOMEvent *aMouseDownEvent, nsIDOMElement* aPanel)
{
  NS_ENSURE_TRUE(aMouseDownEvent, NS_ERROR_FAILURE);
  Event* mouseDownEvent = aMouseDownEvent->InternalDOMEvent();
  NS_ENSURE_TRUE(mouseDownEvent, NS_ERROR_FAILURE);

  nsCOMPtr<Element> panel = do_QueryInterface(aPanel);
  NS_ENSURE_TRUE(panel || !aPanel, NS_ERROR_FAILURE);

  ErrorResult rv;
  BeginWindowMove(*mouseDownEvent, panel, rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::BeginWindowMove(Event& aMouseDownEvent, Element* aPanel,
                                ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget;

  // if a panel was supplied, use its widget instead.
#ifdef MOZ_XUL
  if (aPanel) {
    nsIFrame* frame = aPanel->GetPrimaryFrame();
    if (!frame || frame->GetType() != nsGkAtoms::menuPopupFrame) {
      return;
    }

    widget = (static_cast<nsMenuPopupFrame*>(frame))->GetWidget();
  }
  else {
#endif
    widget = GetMainWidget();
#ifdef MOZ_XUL
  }
#endif

  if (!widget) {
    return;
  }

  WidgetMouseEvent* mouseEvent =
    aMouseDownEvent.GetInternalNSEvent()->AsMouseEvent();
  if (!mouseEvent || mouseEvent->eventStructType != NS_MOUSE_EVENT) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = widget->BeginMoveDrag(mouseEvent);
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP
nsGlobalChromeWindow::SetCursor(const nsAString& aCursor)
{
  ErrorResult rv;
  SetCursor(aCursor, rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::SetCursor(const nsAString& aCursor, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetCursor, (aCursor, aError), aError, );

  int32_t cursor;

  if (aCursor.EqualsLiteral("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aCursor);
    if (eCSSKeyword_UNKNOWN == keyword ||
        !nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
      return;
    }
  }

  nsRefPtr<nsPresContext> presContext;
  if (mDocShell) {
    mDocShell->GetPresContext(getter_AddRefs(presContext));
  }

  if (presContext) {
    // Need root widget.
    nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
    if (!presShell) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsViewManager* vm = presShell->GetViewManager();
    if (!vm) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsView* rootView = vm->GetRootView();
    if (!rootView) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsIWidget* widget = rootView->GetNearestWidget(nullptr);
    if (!widget) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }

    // Call esm and set cursor.
    aError = presContext->EventStateManager()->SetCursor(cursor, nullptr,
                                                         false, 0.0f, 0.0f,
                                                         widget, true);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aBrowserWindow = GetBrowserDOMWindow(rv));
  return rv.ErrorCode();
}

nsIBrowserDOMWindow*
nsGlobalWindow::GetBrowserDOMWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetBrowserDOMWindow, (aError), aError, nullptr);

  MOZ_ASSERT(IsChromeWindow());
  return static_cast<nsGlobalChromeWindow*>(this)->mBrowserDOMWindow;
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow *aBrowserWindow)
{
  ErrorResult rv;
  SetBrowserDOMWindow(aBrowserWindow, rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserWindow,
                                    ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetBrowserDOMWindow, (aBrowserWindow, aError),
                            aError, );
  MOZ_ASSERT(IsChromeWindow());
  static_cast<nsGlobalChromeWindow*>(this)->mBrowserDOMWindow = aBrowserWindow;
}

NS_IMETHODIMP
nsGlobalChromeWindow::NotifyDefaultButtonLoaded(nsIDOMElement* aDefaultButton)
{
  nsCOMPtr<Element> defaultButton = do_QueryInterface(aDefaultButton);
  NS_ENSURE_ARG(defaultButton);

  ErrorResult rv;
  NotifyDefaultButtonLoaded(*defaultButton, rv);
  return rv.ErrorCode();
}

void
nsGlobalWindow::NotifyDefaultButtonLoaded(Element& aDefaultButton,
                                          ErrorResult& aError)
{
#ifdef MOZ_XUL
  // Don't snap to a disabled button.
  nsCOMPtr<nsIDOMXULControlElement> xulControl =
                                      do_QueryInterface(&aDefaultButton);
  if (!xulControl) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  bool disabled;
  aError = xulControl->GetDisabled(&disabled);
  if (aError.Failed() || disabled) {
    return;
  }

  // Get the button rect in screen coordinates.
  nsIFrame *frame = aDefaultButton.GetPrimaryFrame();
  if (!frame) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIntRect buttonRect = frame->GetScreenRect();

  // Get the widget rect in screen coordinates.
  nsIWidget *widget = GetNearestWidget();
  if (!widget) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsIntRect widgetRect;
  aError = widget->GetScreenBounds(widgetRect);
  if (aError.Failed()) {
    return;
  }

  // Convert the buttonRect coordinates from screen to the widget.
  buttonRect -= widgetRect.TopLeft();
  nsresult rv = widget->OnDefaultButtonLoaded(buttonRect);
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    aError.Throw(rv);
  }
#else
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
#endif
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetMessageManager(nsIMessageBroadcaster** aManager)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aManager = GetMessageManager(rv));
  return rv.ErrorCode();
}

nsIMessageBroadcaster*
nsGlobalWindow::GetMessageManager(ErrorResult& aError)
{
  FORWARD_TO_INNER_OR_THROW(GetMessageManager, (aError), aError, nullptr);
  MOZ_ASSERT(IsChromeWindow());
  nsGlobalChromeWindow* myself = static_cast<nsGlobalChromeWindow*>(this);
  if (!myself->mMessageManager) {
    nsIScriptContext* scx = GetContextInternal();
    if (NS_WARN_IF(!scx || !(scx->GetNativeContext()))) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsCOMPtr<nsIMessageBroadcaster> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    myself->mMessageManager =
      new nsFrameMessageManager(nullptr,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                MM_CHROME | MM_BROADCASTER);
  }
  return myself->mMessageManager;
}

// nsGlobalModalWindow implementation

// QueryInterface implementation for nsGlobalModalWindow
DOMCI_DATA(ModalContentWindow, nsGlobalModalWindow)

NS_INTERFACE_MAP_BEGIN(nsGlobalModalWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMModalContentWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ModalContentWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalModalWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalModalWindow, nsGlobalWindow)


JS::Value
nsGlobalWindow::GetDialogArguments(JSContext* aCx, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDialogArguments, (aCx, aError), aError,
                            JS::UndefinedValue());

  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  // This does an internal origin check, and returns undefined if the subject
  // does not subsumes the origin of the arguments.
  JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
  JSAutoCompartment ac(aCx, wrapper);
  JS::Rooted<JS::Value> args(aCx);
  mDialogArguments->Get(aCx, wrapper, nsContentUtils::GetSubjectPrincipal(),
                        &args, aError);
  return args;
}

NS_IMETHODIMP
nsGlobalModalWindow::GetDialogArguments(nsIVariant **aArguments)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetDialogArguments, (aArguments),
                                        NS_ERROR_NOT_INITIALIZED);

  // This does an internal origin check, and returns undefined if the subject
  // does not subsumes the origin of the arguments.
  return mDialogArguments->Get(nsContentUtils::GetSubjectPrincipal(), aArguments);
}

JS::Value
nsGlobalWindow::GetReturnValue(JSContext* aCx, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetReturnValue, (aCx, aError), aError,
                            JS::UndefinedValue());

  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  JS::Rooted<JS::Value> returnValue(aCx);
  if (mReturnValue) {
    JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
    JSAutoCompartment ac(aCx, wrapper);
    mReturnValue->Get(aCx, wrapper, nsContentUtils::GetSubjectPrincipal(),
                      &returnValue, aError);
  }
  return returnValue;
}

NS_IMETHODIMP
nsGlobalModalWindow::GetReturnValue(nsIVariant **aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetReturnValue, (aRetVal), NS_OK);

  nsCOMPtr<nsIVariant> result;
  if (!mReturnValue) {
    nsCOMPtr<nsIVariant> variant = CreateVoidVariant();
    variant.forget(aRetVal);
    return NS_OK;
  }
  return mReturnValue->Get(nsContentUtils::GetSubjectPrincipal(), aRetVal);
}

void
nsGlobalWindow::SetReturnValue(JSContext* aCx,
                               JS::Handle<JS::Value> aReturnValue,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetReturnValue, (aCx, aReturnValue, aError),
                            aError, );

  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  nsCOMPtr<nsIVariant> returnValue;
  aError =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aReturnValue,
                                             getter_AddRefs(returnValue));
  if (!aError.Failed()) {
    mReturnValue = new DialogValueHolder(nsContentUtils::GetSubjectPrincipal(),
                                         returnValue);
  }
}

NS_IMETHODIMP
nsGlobalModalWindow::SetReturnValue(nsIVariant *aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(SetReturnValue, (aRetVal), NS_OK);

  mReturnValue = new DialogValueHolder(nsContentUtils::GetSubjectPrincipal(),
                                       aRetVal);
  return NS_OK;
}

/* static */
bool
nsGlobalWindow::IsModalContentWindow(JSContext* aCx, JSObject* aGlobal)
{
  // For now, have to deal with XPConnect objects here.
  nsGlobalWindow* win;
  nsresult rv = UNWRAP_OBJECT(Window, aGlobal, win);
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryWrapper(aCx, aGlobal);
    win = static_cast<nsGlobalWindow*>(piWin.get());
  }
  return win->IsModalContentWindow();
}

NS_IMETHODIMP
nsGlobalWindow::GetConsole(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aConsole)
{
  ErrorResult rv;
  nsRefPtr<Console> console = GetConsole(rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  if (!WrapNewBindingObject(aCx, console, aConsole)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetConsole(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  JS::Rooted<JSObject*> thisObj(aCx, GetWrapper());
  if (!thisObj) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JS_WrapObject(aCx, &thisObj) ||
      !JS_DefineProperty(aCx, thisObj, "console", aValue,
                         JSPROP_ENUMERATE, JS_PropertyStub,
                         JS_StrictPropertyStub)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

Console*
nsGlobalWindow::GetConsole(ErrorResult& aRv)
{
  FORWARD_TO_INNER_OR_THROW(GetConsole, (aRv), aRv, nullptr);

  if (!mConsole) {
    mConsole = new Console(this);
  }

  return mConsole;
}

already_AddRefed<External>
nsGlobalWindow::GetExternal(ErrorResult& aRv)
{
  FORWARD_TO_INNER_OR_THROW(GetExternal, (aRv), aRv, nullptr);

#ifdef HAVE_SIDEBAR
  if (!mExternal) {
    AutoJSContext cx;
    JS::Rooted<JSObject*> jsImplObj(cx);
    ConstructJSImplementation(cx, "@mozilla.org/sidebar;1",
                              this, &jsImplObj, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    mExternal = new External(jsImplObj, this);
  }

  nsRefPtr<External> external = static_cast<External*>(mExternal.get());
  return external.forget();
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
#endif
}

void
nsGlobalWindow::GetSidebar(OwningExternalOrWindowProxy& aResult,
                           ErrorResult& aRv)
{
  FORWARD_TO_INNER_OR_THROW(GetSidebar, (aResult, aRv), aRv, );

#ifdef HAVE_SIDEBAR
  // First check for a named frame named "sidebar"
  nsCOMPtr<nsIDOMWindow> domWindow = GetChildWindow(NS_LITERAL_STRING("sidebar"));
  if (domWindow) {
    aResult.SetAsWindowProxy() = domWindow.forget();
    return;
  }

  nsRefPtr<External> external = GetExternal(aRv);
  if (external) {
    aResult.SetAsExternal() = external;
  }
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
#endif
}

/* static */
bool
nsGlobalWindow::WindowOnWebIDL(JSContext* aCx, JSObject* aObj)
{
  DebugOnly<nsGlobalWindow*> win;
  MOZ_ASSERT_IF(IsDOMObject(aObj),
                NS_SUCCEEDED(UNWRAP_OBJECT(Window, aObj, win)));

  return IsDOMObject(aObj);
}

#ifdef MOZ_B2G
void
nsGlobalWindow::EnableNetworkEvent(uint32_t aType)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permMgr) {
    NS_ERROR("No PermissionManager available!");
    return;
  }

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestExactPermissionFromPrincipal(GetPrincipal(), "network-events",
                                            &permission);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    NS_ERROR("ObserverService should be available!");
    return;
  }

  switch (aType) {
    case NS_NETWORK_UPLOAD_EVENT:
      if (!mNetworkUploadObserverEnabled) {
        mNetworkUploadObserverEnabled = true;
        os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC, false);
      }
      break;
    case NS_NETWORK_DOWNLOAD_EVENT:
      if (!mNetworkDownloadObserverEnabled) {
        mNetworkDownloadObserverEnabled = true;
        os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC, false);
      }
      break;
  }
}

void
nsGlobalWindow::DisableNetworkEvent(uint32_t aType)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  switch (aType) {
    case NS_NETWORK_UPLOAD_EVENT:
      if (mNetworkUploadObserverEnabled) {
        mNetworkUploadObserverEnabled = false;
        os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC);
      }
      break;
    case NS_NETWORK_DOWNLOAD_EVENT:
      if (mNetworkDownloadObserverEnabled) {
        mNetworkDownloadObserverEnabled = false;
        os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC);
      }
      break;
  }
}
#endif // MOZ_B2G

#define EVENT(name_, id_, type_, struct_)                                    \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             JS::MutableHandle<JS::Value> vp) { \
    EventHandlerNonNull* h = GetOn##name_();                                 \
    vp.setObjectOrNull(h ? h->Callable().get() : nullptr);                   \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             JS::Handle<JS::Value> v) {      \
    nsRefPtr<EventHandlerNonNull> handler;                                   \
    JS::Rooted<JSObject*> callable(cx);                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      handler = new EventHandlerNonNull(callable, GetIncumbentGlobal());     \
    }                                                                        \
    SetOn##name_(handler);                                                   \
    return NS_OK;                                                            \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                              \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             JS::MutableHandle<JS::Value> vp) { \
    EventListenerManager *elm = GetExistingListenerManager();                \
    if (elm) {                                                               \
      OnErrorEventHandlerNonNull* h = elm->GetOnErrorEventHandler();         \
      if (h) {                                                               \
        vp.setObject(*h->Callable());                                        \
        return NS_OK;                                                        \
      }                                                                      \
    }                                                                        \
    vp.setNull();                                                            \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             JS::Handle<JS::Value> v) {      \
    EventListenerManager *elm = GetOrCreateListenerManager();                \
    if (!elm) {                                                              \
      return NS_ERROR_OUT_OF_MEMORY;                                         \
    }                                                                        \
                                                                             \
    nsRefPtr<OnErrorEventHandlerNonNull> handler;                            \
    JS::Rooted<JSObject*> callable(cx);                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      handler = new OnErrorEventHandlerNonNull(callable, GetIncumbentGlobal()); \
    }                                                                        \
    elm->SetEventHandler(handler);                                           \
    return NS_OK;                                                            \
  }
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                       \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             JS::MutableHandle<JS::Value> vp) { \
    EventListenerManager *elm = GetExistingListenerManager();                \
    if (elm) {                                                               \
      OnBeforeUnloadEventHandlerNonNull* h =                                 \
        elm->GetOnBeforeUnloadEventHandler();                                \
      if (h) {                                                               \
        vp.setObject(*h->Callable());                                        \
        return NS_OK;                                                        \
      }                                                                      \
    }                                                                        \
    vp.setNull();                                                            \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             JS::Handle<JS::Value> v) {      \
    EventListenerManager *elm = GetOrCreateListenerManager();                \
    if (!elm) {                                                              \
      return NS_ERROR_OUT_OF_MEMORY;                                         \
    }                                                                        \
                                                                             \
    nsRefPtr<OnBeforeUnloadEventHandlerNonNull> handler;                     \
    JS::Rooted<JSObject*> callable(cx);                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      handler = new OnBeforeUnloadEventHandlerNonNull(callable, GetIncumbentGlobal()); \
    }                                                                        \
    elm->SetEventHandler(handler);                                           \
    return NS_OK;                                                            \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef BEFOREUNLOAD_EVENT
#undef ERROR_EVENT
#undef EVENT

#ifdef _WINDOWS_
#error "Never include windows.h in this file!"
#endif
