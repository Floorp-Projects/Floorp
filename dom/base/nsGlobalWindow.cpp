/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include <algorithm>

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
#include "nsError.h"
#include "nsIIdleService.h"
#include "nsIPowerManagerService.h"
#include "nsISizeOfEventTarget.h"

#ifdef XP_WIN
#ifdef GetClassName
#undef GetClassName
#endif // GetClassName
#endif // XP_WIN

// Helper Classes
#include "nsXPIDLString.h"
#include "nsJSUtils.h"
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
#include "mozilla/Likely.h"

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
#include "nsJSPrincipals.h"
#include "mozilla/Attributes.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsIBaseWindow.h"
#include "nsDeviceSensors.h"
#include "nsIContent.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "Crypto.h"
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
#include "nsIEmbeddingSiteWindow.h"
#include "nsThreadUtils.h"
#include "nsEventStateManager.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsILoadContext.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsIProgrammingLanguage.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectOwner.h"
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
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWebContentHandlerRegistrar.h"
#include "nsIWindowMediator.h"  // For window.find()
#include "nsComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
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
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIURIFixup.h"
#include "nsIAppStartup.h"
#include "nsToolkitCompsCID.h"
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
#include "prrng.h"
#include "nsSandboxFlags.h"
#include "TimeChangeObserver.h"
#include "nsPISocketTransportService.h"
#include "mozilla/dom/AudioContext.h"

// Apple system headers seem to have a check() macro.  <sigh>
#ifdef check
#undef check
#endif // check
#include "AccessCheck.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gDOMLeakPRLog;
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

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
static bool                 gDOMWindowDumpEnabled      = false;
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
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
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

  MOZ_COUNT_DTOR(nsTimeout);
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsTimeout)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTimeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTimeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTimeout, Release)

nsPIDOMWindow::nsPIDOMWindow(nsPIDOMWindow *aOuterWindow)
: mFrameElement(nullptr), mDocShell(nullptr), mModalStateDepth(0),
  mRunningTimeout(nullptr), mMutationBits(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nullptr),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mIsModalContentWindow(false),
  mIsActive(false), mIsBackground(false),
  mInnerWindow(nullptr), mOuterWindow(aOuterWindow),
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
  nsOuterWindowProxy() : js::Wrapper(0) { }

  virtual bool isOuterWindow() {
    return true;
  }
  virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE;
  virtual void finalize(JSFreeOp *fop, JSObject *proxy) MOZ_OVERRIDE;

  // Fundamental traps
  virtual bool getPropertyDescriptor(JSContext* cx, JSObject* proxy,
                                     jsid id, JSPropertyDescriptor* desc,
                                     unsigned flags) MOZ_OVERRIDE;
  virtual bool getOwnPropertyDescriptor(JSContext* cx, JSObject* proxy,
                                        jsid id, JSPropertyDescriptor* desc,
                                        unsigned flags) MOZ_OVERRIDE;
  virtual bool defineProperty(JSContext* cx, JSObject* proxy,
                              jsid id, JSPropertyDescriptor* desc) MOZ_OVERRIDE;
  virtual bool getOwnPropertyNames(JSContext *cx, JSObject *proxy,
                                   JS::AutoIdVector &props) MOZ_OVERRIDE;
  virtual bool delete_(JSContext *cx, JSObject *proxy, jsid id,
                       bool *bp) MOZ_OVERRIDE;
  virtual bool enumerate(JSContext *cx, JSObject *proxy,
                         JS::AutoIdVector &props) MOZ_OVERRIDE;

  // Derived traps
  virtual bool has(JSContext *cx, JSObject *proxy, jsid id,
                   bool *bp) MOZ_OVERRIDE;
  virtual bool hasOwn(JSContext *cx, JSObject *proxy, jsid id,
                      bool *bp) MOZ_OVERRIDE;
  virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver,
                   jsid id, JS::Value *vp) MOZ_OVERRIDE;
  virtual bool set(JSContext *cx, JSObject *proxy, JSObject *receiver,
                   jsid id, bool strict, JS::Value *vp) MOZ_OVERRIDE;
  virtual bool keys(JSContext *cx, JSObject *proxy,
                    JS::AutoIdVector &props) MOZ_OVERRIDE;
  virtual bool iterate(JSContext *cx, JSObject *proxy, unsigned flags,
                       JS::Value *vp) MOZ_OVERRIDE;

  static nsOuterWindowProxy singleton;

protected:
  nsGlobalWindow* GetWindow(JSObject *proxy)
  {
    return nsGlobalWindow::FromSupports(
      static_cast<nsISupports*>(js::GetProxyExtra(proxy, 0).toPrivate()));
  }

  // False return value means we threw an exception.  True return value
  // but false "found" means we didn't have a subframe at that index.
  bool GetSubframeWindow(JSContext *cx, JSObject *proxy, jsid id,
                         JS::Value *vp, bool &found);

  // Returns a non-null window only if id is an index and we have a
  // window at that index.
  already_AddRefed<nsIDOMWindow> GetSubframeWindow(JSContext *cx,
                                                   JSObject *proxy,
                                                   jsid id);

  bool AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                  JS::AutoIdVector &props);
};


JSString *
nsOuterWindowProxy::obj_toString(JSContext *cx, JSObject *proxy)
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return JS_NewStringCopyZ(cx, "[object Window]");
}

void
nsOuterWindowProxy::finalize(JSFreeOp *fop, JSObject *proxy)
{
  nsGlobalWindow* global = GetWindow(proxy);
  if (global) {
    global->ClearWrapper();
  }
}

bool
nsOuterWindowProxy::getPropertyDescriptor(JSContext* cx, JSObject* proxy,
                                          jsid id,
                                          JSPropertyDescriptor* desc,
                                          unsigned flags)
{
  // The only thing we can do differently from js::Wrapper is shadow stuff with
  // our indexed properties, so we can just try getOwnPropertyDescriptor and if
  // that gives us nothing call on through to js::Wrapper.
  desc->obj = nullptr;
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc, flags)) {
    return false;
  }

  if (desc->obj) {
    return true;
  }

  return js::Wrapper::getPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
nsOuterWindowProxy::getOwnPropertyDescriptor(JSContext* cx, JSObject* proxy,
                                             jsid id,
                                             JSPropertyDescriptor* desc,
                                             unsigned flags)
{
  bool found;
  if (!GetSubframeWindow(cx, proxy, id, &desc->value, found)) {
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
nsOuterWindowProxy::defineProperty(JSContext* cx, JSObject* proxy,
                                   jsid id, JSPropertyDescriptor* desc)
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
nsOuterWindowProxy::getOwnPropertyNames(JSContext *cx, JSObject *proxy,
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
nsOuterWindowProxy::delete_(JSContext *cx, JSObject *proxy, jsid id,
                            bool *bp)
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
nsOuterWindowProxy::enumerate(JSContext *cx, JSObject *proxy,
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
nsOuterWindowProxy::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
  if (nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::has(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
  if (nsCOMPtr<nsIDOMWindow> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::hasOwn(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::get(JSContext *cx, JSObject *wrapper, JSObject *receiver,
                        jsid id, JS::Value *vp)
{
  if (id == nsDOMClassInfo::sWrappedJSObject_id &&
      xpc::AccessCheck::isChrome(js::GetContextCompartment(cx))) {
    *vp = JS::ObjectValue(*wrapper);
    return true;
  }

  bool found;
  if (!GetSubframeWindow(cx, wrapper, id, vp, found)) {
    return false;
  }
  if (found) {
    return true;
  }
  // Else fall through to js::Wrapper

  return js::Wrapper::get(cx, wrapper, receiver, id, vp);
}

bool
nsOuterWindowProxy::set(JSContext *cx, JSObject *proxy, JSObject *receiver,
                        jsid id, bool strict, JS::Value *vp)
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
nsOuterWindowProxy::keys(JSContext *cx, JSObject *proxy,
                         JS::AutoIdVector &props)
{
  // BaseProxyHandler::keys seems to do what we want here: call
  // getOwnPropertyNames and then filter out the non-enumerable properties.
  return js::BaseProxyHandler::keys(cx, proxy, props);
}

bool
nsOuterWindowProxy::iterate(JSContext *cx, JSObject *proxy, unsigned flags,
                            JS::Value *vp)
{
  // BaseProxyHandler::iterate seems to do what we want here: fall
  // back on the property names returned from keys() and enumerate().
  return js::BaseProxyHandler::iterate(cx, proxy, flags, vp);
}

bool
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx, JSObject *proxy,
                                      jsid id, JS::Value* vp,
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
  *vp = JS::ObjectValue(*obj);
  return JS_WrapValue(cx, vp);
}

already_AddRefed<nsIDOMWindow>
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx, JSObject *proxy, jsid id)
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
  uint32_t length = GetWindow(proxy)->GetLength();
  MOZ_ASSERT(int32_t(length) >= 0);
  if (!props.reserve(props.length() + length)) {
    return false;
  }
  for (int32_t i = 0; i < int32_t(length); ++i) {
    props.append(INT_TO_JSID(i));
  }

  return true;
}

nsOuterWindowProxy
nsOuterWindowProxy::singleton;

class nsChromeOuterWindowProxy : public nsOuterWindowProxy
{
public:
  nsChromeOuterWindowProxy() : nsOuterWindowProxy() {}

  virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper);

  static nsChromeOuterWindowProxy singleton;
};

JSString *
nsChromeOuterWindowProxy::obj_toString(JSContext *cx, JSObject *proxy)
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return JS_NewStringCopyZ(cx, "[object ChromeWindow]");
}

nsChromeOuterWindowProxy
nsChromeOuterWindowProxy::singleton;

static JSObject*
NewOuterWindowProxy(JSContext *cx, JSObject *parent, bool isChrome)
{
  JSAutoCompartment ac(cx, parent);
  JSObject *proto;
  if (!js::GetObjectProto(cx, parent, &proto))
    return nullptr;

  JSObject *obj = js::Wrapper::New(cx, parent, proto, parent,
                                   isChrome ? &nsChromeOuterWindowProxy::singleton
                                            : &nsOuterWindowProxy::singleton);

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
    mNotifiedIDDestroyed(false),
    mAllowScriptsToClose(false),
    mTimeoutInsertionPoint(nullptr),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mJSObject(nullptr),
    mTimeoutsSuspendDepth(0),
    mFocusMethod(0),
    mSerial(0),
#ifdef DEBUG
    mSetOpenerWindowCalled(false),
#endif
    mCleanedUp(false),
    mCallCleanUpAfterModalDialogCloses(false),
    mDialogAbuseCount(0),
    mStopAbuseDialogs(false),
    mDialogsPermanentlyDisabled(false)
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
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
    Preferences::AddBoolVarCache(&gDOMWindowDumpEnabled,
                                 "browser.dom.window.dump.enabled");
#endif
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
    printf("++DOMWINDOW == %d (%p) [serial = %d] [outer = %p]\n", gRefCnt,
           static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
           gSerialCounter,
           static_cast<void*>(static_cast<nsIScriptGlobalObject*>(aOuterWindow)));
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
    printf("--DOMWINDOW == %d (%p) [serial = %d] [outer = %p] [url = %s]\n",
           gRefCnt, static_cast<void*>(static_cast<nsIScriptGlobalObject*>(this)),
           mSerial, static_cast<void*>(static_cast<nsIScriptGlobalObject*>(outer)), url.get());
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

  mDocument = nullptr;           // Forces Release
  mDoc = nullptr;

  NS_ASSERTION(!mArguments, "mArguments wasn't cleaned up properly!");

  CleanUp(true);

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
  gDumpFile = nullptr;

  NS_IF_RELEASE(gEntropyCollector);

  delete sWindowsById;
  sWindowsById = nullptr;
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

  mNavigator = nullptr;
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

  mPerformance = nullptr;

  ClearControllers();

  mOpener = nullptr;             // Forces Release
  if (mContext) {
    mContext = nullptr;            // Forces Release
  }
  mChromeEventHandler = nullptr; // Forces Release
  mParentTarget = nullptr;

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

  mInnerWindowHolder = nullptr;
  mArguments = nullptr;
  mArgumentsLast = nullptr;
  mArgumentsOrigin = nullptr;

  CleanupCachedXBLHandlers(this);

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

  // Kill all of the workers for this window.
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = scx ? scx->GetNativeContext() : nullptr;
  mozilla::dom::workers::CancelWorkersForWindow(cx, this);

  // Close all IndexedDB databases for this window.
  indexedDB::IndexedDatabaseManager* idbManager =
    indexedDB::IndexedDatabaseManager::Get();
  if (idbManager) {
    idbManager->AbortCloseDatabasesForWindow(this);
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
    mScreen->Reset();
    mScreen = nullptr;
  }

  if (mDocument) {
    NS_ASSERTION(mDoc, "Why is mDoc null?");

    // Remember the document's principal and URI.
    mDocumentPrincipal = mDoc->NodePrincipal();
    mDocumentURI = mDoc->GetDocumentURI();
    mDocBaseURI = mDoc->GetDocBaseURI();
  }

  // Remove our reference to the document and the document principal.
  mDocument = nullptr;
  mDoc = nullptr;
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
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

#define OUTER_WINDOW_ONLY                                                     \
  if (IsOuterWindow()) {

#define END_OUTER_WINDOW_ONLY                                                 \
    foundInterface = 0;                                                       \
  } else

DOMCI_DATA(Window, nsGlobalWindow)

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindow)
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
#ifdef MOZ_B2G
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowB2G)
#endif // MOZ_B2G
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
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
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
      tmp->mCachedXBLPrototypeHandlers.EnumerateRead(MarkXBLHandlers, nullptr);
    }
    nsEventListenerManager* elm = tmp->GetListenerManager(false);
    if (elm) {
      elm->MarkForCC();
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

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            IdleObserverHolder& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mIdleObserver.get(), aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsGlobalWindow)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    PR_snprintf(name, sizeof(name), "nsGlobalWindow #%ld", tmp->mWindowID);
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGlobalWindow, tmp->mRefCnt.get())
  }

  if (!cb.WantAllTraces() && tmp->IsBlackForCC()) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInnerWindowHolder)
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

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusedNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioContexts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindow)
  nsGlobalWindow::CleanupCachedXBLHandlers(tmp);

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mArgumentsLast)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInnerWindowHolder)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingStorageEvents)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFocusedNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioContexts)
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
  for (nsTimeout* timeout = mTimeouts.getFirst();
       timeout;
       timeout = timeout->getNext()) {
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

  // If this window is a [i]frame, don't bother GC'ing when the frame's context
  // is destroyed since a GC will happen when the frameset or host document is
  // destroyed anyway.
  nsCOMPtr<nsIScriptContext> context =
    scriptRuntime->CreateContext(!IsFrame(), this);

  NS_ASSERTION(!mContext, "Will overwrite mContext!");

  // should probably assert the context is clean???
  context->WillInitializeContext();

  rv = context->InitContext();
  NS_ENSURE_SUCCESS(rv, rv);

  mContext = context;
  return NS_OK;
}

nsIScriptContext *
nsGlobalWindow::GetScriptContext()
{
  FORWARD_TO_OUTER(GetScriptContext, (), nullptr);
  return mContext;
}

nsIScriptContext *
nsGlobalWindow::GetContext()
{
  FORWARD_TO_OUTER(GetContext, (), nullptr);

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

  // First, grab the subject principal. These methods never fail.
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> newWindowPrincipal, systemPrincipal;
  ssm->GetSubjectPrincipal(getter_AddRefs(newWindowPrincipal));
  ssm->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  if (!newWindowPrincipal) {
    newWindowPrincipal = systemPrincipal;
  }

  // Now, if we're about to use the system principal, make sure we're not using
  // it for a content docshell.
  if (newWindowPrincipal == systemPrincipal) {
    int32_t itemType;
    nsresult rv = GetDocShell()->GetItemType(&itemType);
    if (NS_FAILED(rv) || itemType != nsIDocShellTreeItem::typeChrome) {
      newWindowPrincipal = nullptr;
    }
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

class WindowStateHolder MOZ_FINAL : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  WindowStateHolder(nsGlobalWindow *aWindow,
                    nsIXPConnectJSObjectHolder *aHolder);

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }
  nsIXPConnectJSObjectHolder *GetInnerWindowHolder()
  { return mInnerWindowHolder; }

  void DidRestoreWindow()
  {
    mInnerWindow = nullptr;
    mInnerWindowHolder = nullptr;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindow *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  nsCOMPtr<nsIXPConnectJSObjectHolder> mInnerWindowHolder;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow *aWindow,
                                     nsIXPConnectJSObjectHolder *aHolder)
  : mInnerWindow(aWindow)
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

  JSObject* outer = NewOuterWindowProxy(cx, aNewInner->FastGetGlobalJSObject(),
                                        IsChromeWindow());
  if (!outer) {
    return NS_ERROR_FAILURE;
  }

  js::SetProxyExtra(outer, 0, js::PrivateValue(ToSupports(this)));

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
  JSObject* proto;
  if (!JS_GetPrototype(aCx, inner, &proto)) {
    return NS_ERROR_FAILURE;
  }
  JS_SetPrototype(aCx, aOuterObject, proto);

  return NS_OK;
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
                           JSObject** aNativeGlobal,
                           nsIXPConnectJSObjectHolder** aHolder)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aNewInner);
  MOZ_ASSERT(aNewInner->IsInnerWindow());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aNativeGlobal);
  MOZ_ASSERT(aHolder);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsRefPtr<nsIXPConnectJSObjectHolder> jsholder;
  nsresult rv = xpc->InitClassesWithNewWrappedGlobal(
    aCx, ToSupports(aNewInner),
    aPrincipal, 0, getter_AddRefs(jsholder));
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(jsholder);
  jsholder->GetJSObject(aNativeGlobal);
  jsholder.forget(aHolder);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  MOZ_ASSERT(*aNativeGlobal);
  xpc::SetLocationForGlobal(*aNativeGlobal, aURI);

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

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  // clear smartcard events, our document has gone away.
  if (mCrypto) {
    nsresult rv = mCrypto->SetEnableSmartCardEvents(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  if (!mDocument) {
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

  if (currentInner && currentInner->mNavigator) {
    currentInner->mNavigator->OnNavigation();
  }

  nsRefPtr<nsGlobalWindow> newInnerWindow;
  bool createdInnerWindow = false;

  bool thisChrome = IsChromeWindow();

  nsCxPusher cxPusher;
  if (!cxPusher.Push(cx, nsCxPusher::ASSERT_SCRIPT_CONTEXT)) {
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
      if (!nsWindowSH::InvalidateGlobalScopePolluter(cx, currentInner->mJSObject)) {
        return NS_ERROR_FAILURE;
      }
    }

    // We're reusing the inner window, but this still counts as a navigation,
    // so all expandos and such defined on the outer window should go away. Force
    // all Xray wrappers to be recomputed.
    xpc_UnmarkGrayObject(mJSObject);
    if (!JS_RefreshCrossCompartmentWrappers(cx, mJSObject)) {
      return NS_ERROR_FAILURE;
    }

    // Inner windows are only reused for same-origin principals, but the principals
    // don't necessarily match exactly. Update the principal on the compartment to
    // match the new document.
    // NB: We don't just call currentInner->RefreshCompartmentPrincipals() here
    // because we haven't yet set its mDoc to aDocument.
    JSCompartment *compartment = js::GetObjectCompartment(currentInner->mJSObject);
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
      mInnerWindowHolder = wsh->GetInnerWindowHolder();

      NS_ASSERTION(newInnerWindow, "Got a state without inner window");
    } else if (thisChrome) {
      newInnerWindow = new nsGlobalChromeWindow(this);
    } else if (mIsModalContentWindow) {
      newInnerWindow = new nsGlobalModalWindow(this);
    } else {
      newInnerWindow = new nsGlobalWindow(this);
    }

    if (!aState) {
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
                                      &newInnerWindow->mJSObject,
                                      getter_AddRefs(mInnerWindowHolder));
      NS_ASSERTION(NS_SUCCEEDED(rv) && newInnerWindow->mJSObject && mInnerWindowHolder,
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
                              currentInner->mPerformance->GetChannel());
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
      JSObject *outerObject = NewOuterWindowProxy(cx, xpc_UnmarkGrayObject(newInnerWindow->mJSObject),
                                                  thisChrome);
      if (!outerObject) {
        NS_ERROR("out of memory");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyExtra(mJSObject, 0, js::PrivateValue(NULL));

      outerObject = xpc::TransplantObject(cx, mJSObject, outerObject);
      if (!outerObject) {
        NS_ERROR("unable to transplant wrappers, probably OOM");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyExtra(outerObject, 0, js::PrivateValue(ToSupports(this)));

      mJSObject = outerObject;
      SetWrapper(mJSObject);

      {
        JSAutoCompartment ac(cx, mJSObject);

        JS_SetParent(cx, mJSObject, newInnerWindow->mJSObject);

        rv = SetOuterObject(cx, mJSObject);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ASSERTION(!JS_IsExceptionPending(cx),
                     "We might overwrite a pending exception!");
        XPCWrappedNativeScope* scope = xpc::GetObjectScope(mJSObject);
        if (scope->mWaiverWrapperMap) {
          scope->mWaiverWrapperMap->Reparent(cx, newInnerWindow->mJSObject);
        }
      }
    }

    // Enter the new global's compartment.
    JSAutoCompartment ac(cx, mJSObject);

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

    if (!aState) {
      if (!JS_DefineProperty(cx, newInnerWindow->mJSObject, "window",
                             OBJECT_TO_JSVAL(mJSObject),
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
        NS_ERROR("can't create the 'window' property");
        return NS_ERROR_FAILURE;
      }
    }
  }

  JSAutoCompartment ac(cx, mJSObject);

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
#ifdef DEBUG
    JSObject *proto1, *proto2;
    JS_GetPrototype(cx, mJSObject, &proto1);
    JS_GetPrototype(cx, newInnerJSObject, &proto2);
    NS_ASSERTION(proto1 == proto2,
                 "outer and inner globals should have the same prototype");
#endif

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

  aDocument->SetScriptGlobalObject(newInnerWindow);

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
      newInnerWindow->InnerSetNewDocument(aDocument);

      // Initialize DOM classes etc on the inner window.
      rv = mContext->InitClasses(newInnerWindow->mJSObject);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mArguments) {
      newInnerWindow->DefineArgumentsProperty(mArguments);
      newInnerWindow->mArguments = mArguments;
      newInnerWindow->mArgumentsOrigin = mArgumentsOrigin;

      mArguments = nullptr;
      mArgumentsOrigin = nullptr;
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
    int32_t itemType = nsIDocShellTreeItem::typeContent;
    if (mDocShell) {
      mDocShell->GetItemType(&itemType);
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

void
nsGlobalWindow::InnerSetNewDocument(nsIDocument* aDocument)
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

  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;
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
      NS_NewWindowRoot(this, getter_AddRefs(mChromeEventHandler));
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
  // (mJSObject) so that it can be retrieved later (until it is
  // finalized by the JS GC).

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
    mDocument = nullptr;
    mDoc = nullptr;
    mFocusedNode = nullptr;
  }

  ClearControllers();

  mChromeEventHandler = nullptr; // force release now

  if (mArguments) { 
    // We got no new document after someone called
    // SetArguments(), drop our reference to the arguments.
    mArguments = nullptr;
    mArgumentsLast = nullptr;
    mArgumentsOrigin = nullptr;
  }

  if (mContext) {
    mContext->GC(js::gcreason::SET_DOC_SHELL);
    mContext = nullptr;
  }

  mDocShell = nullptr; // Weak Reference

  NS_ASSERTION(!mNavigator, "Non-null mNavigator in outer window!");

  if (mFrames) {
    mFrames->SetDocShell(nullptr);
  }

  MaybeForgiveSpamCount();
  CleanUp(false);

    if (mLocalStorage) {
      nsCOMPtr<nsIPrivacyTransitionObserver> obs = do_GetInterface(mLocalStorage);
      if (obs) {
        mDocShell->AddWeakPrivacyTransitionObserver(obs);
      }
    }
    if (mSessionStorage) {
      nsCOMPtr<nsIPrivacyTransitionObserver> obs = do_GetInterface(mSessionStorage);
      if (obs) {
        mDocShell->AddWeakPrivacyTransitionObserver(obs);
      }
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
  return nullptr;
}

nsresult
nsGlobalWindow::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
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
      (NS_IS_MOUSE_EVENT(aVisitor.mEvent) ||
       NS_IS_DRAG_EVENT(aVisitor.mEvent))) {
    mAddActiveEventFuzzTime = false;
  }

  return NS_OK;
}

bool
nsGlobalWindow::DialogsAreBlocked(bool *aBeingAbused)
{
  *aBeingAbused = false;

  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ASSERTION(!mDocShell, "DialogsAreBlocked() called without a top window?");
    return true;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (!topWindow) {
    return true;
  }

  if (topWindow->mDialogsPermanentlyDisabled) {
    return true;
  }

  // Dialogs are blocked if the content viewer is hidden
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));

    bool isHidden;
    cv->GetIsHidden(&isHidden);
    if (isHidden) {
      return true;
    }
  }

  *aBeingAbused = topWindow->DialogsAreBeingAbused();

  return topWindow->mStopAbuseDialogs && *aBeingAbused;
}

bool
nsGlobalWindow::DialogsAreBeingAbused()
{
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
  FORWARD_TO_OUTER(ConfirmDialogIfNeeded, (), false);

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
    PreventFurtherDialogs(false);
    return false;
  }

  return true;
}

void
nsGlobalWindow::PreventFurtherDialogs(bool aPermanent)
{
  nsGlobalWindow *topWindow = GetScriptableTop();
  if (!topWindow) {
    NS_ERROR("PreventFurtherDialogs() called without a top window?");
    return;
  }

  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (topWindow) {
    topWindow->mStopAbuseDialogs = true;
    if (aPermanent) {
      topWindow->mDialogsPermanentlyDisabled = true;
    }
  }
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
             aVisitor.mEvent->mFlags.mIsTrusted) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDocument) {
      NS_ASSERTION(mDoc, "Must have doc");
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->message == NS_LOAD &&
             aVisitor.mEvent->mFlags.mIsTrusted) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = true;

    nsCOMPtr<nsIContent> content(do_QueryInterface(GetFrameElementInternal()));
    nsIDocShell* docShell = GetDocShell();

    int32_t itemType = nsIDocShellTreeItem::typeChrome;

    if (docShell) {
      docShell->GetItemType(&itemType);
    }

    if (content && GetParentInternal() &&
        itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(aVisitor.mEvent->mFlags.mIsTrusted, NS_LOAD);
      event.mFlags.mBubbles = false;

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.
      nsEventDispatcher::Dispatch(content, nullptr, &event, nullptr, &status);
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
    // Note that |document| here is the same thing as our mDocument, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<nsIDocument> document = do_GetInterface(docShell);
  }
}

void
nsPIDOMWindow::AddAudioContext(AudioContext* aAudioContext)
{
  mAudioContexts.AppendElement(aAudioContext);
}

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocument** aDocument)
{
  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(GetDoc());
  document.forget(aDocument);
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

  *aNavigator = nullptr;

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

  *aScreen = nullptr;

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

  *aHistory = nullptr;

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
nsGlobalWindow::GetPerformance(nsISupports** aPerformance)
{
  FORWARD_TO_INNER(GetPerformance, (aPerformance), NS_ERROR_NOT_INITIALIZED);

  NS_IF_ADDREF(*aPerformance = nsPIDOMWindow::GetPerformance());
  return NS_OK;
}

nsPerformance*
nsPIDOMWindow::GetPerformance()
{
  MOZ_ASSERT(IsInnerWindow());
  if (HasPerformanceSupport()) {
    CreatePerformanceObjectIfNeeded();
    return mPerformance;
  }
  return nullptr;
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
    mPerformance = new nsPerformance(this, timing, timedChannel);
  }
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
  FORWARD_TO_OUTER(GetScriptableParent, (aParent), NS_ERROR_NOT_INITIALIZED);

  *aParent = NULL;
  if (!mDocShell) {
    return NS_OK;
  }

  if (mDocShell->GetIsBrowserOrApp()) {
    nsCOMPtr<nsIDOMWindow> parent = static_cast<nsIDOMWindow*>(this);
    parent.swap(*aParent);
    return NS_OK;
  }

  return GetRealParent(aParent);
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
  return GetTopImpl(aTop, /* aScriptable = */ true);
}

/**
 * nsIDOMWindow::GetTop (when called from C++) is just a wrapper around
 * GetRealTop.
 */
NS_IMETHODIMP
nsGlobalWindow::GetRealTop(nsIDOMWindow** aTop)
{
  return GetTopImpl(aTop, /* aScriptable = */ false);
}

nsresult
nsGlobalWindow::GetTopImpl(nsIDOMWindow** aTop, bool aScriptable)
{
  FORWARD_TO_OUTER(GetTopImpl, (aTop, aScriptable), NS_ERROR_NOT_INITIALIZED);
  *aTop = nullptr;

  // Walk up the parent chain.

  nsCOMPtr<nsIDOMWindow> prevParent = this;
  nsCOMPtr<nsIDOMWindow> parent = this;
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

NS_IMETHODIMP
nsGlobalWindow::GetContent(nsIDOMWindow** aContent)
{
  FORWARD_TO_OUTER(GetContent, (aContent), NS_ERROR_NOT_INITIALIZED);
  *aContent = nullptr;

  // If we're contained in <iframe mozbrowser> or <iframe mozapp>, then
  // GetContent is the same as window.top.
  if (mDocShell && mDocShell->GetIsInBrowserOrApp()) {
    return GetScriptableTop(aContent);
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
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(primaryContent));
  domWindow.forget(aContent);

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

  *aMenubar = nullptr;

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

  *aToolbar = nullptr;

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

  *aLocationbar = nullptr;

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

  *aPersonalbar = nullptr;

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

  *aStatusbar = nullptr;

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

  *aScrollbars = nullptr;

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
  FORWARD_TO_OUTER(GetCrypto, (aCrypto), NS_ERROR_NOT_INITIALIZED);

  if (!mCrypto) {
#ifndef MOZ_DISABLE_CRYPTOLEGACY
    mCrypto = do_CreateInstance(NS_CRYPTO_CONTRACTID);
#else
    mCrypto = new Crypto();
#endif
  }
  NS_IF_ADDREF(*aCrypto = mCrypto);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  *aPkcs11 = nullptr;
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

  *aOpener = nullptr;

  nsCOMPtr<nsPIDOMWindow> opener = do_QueryReferent(mOpener);
  if (!opener) {
    return NS_OK;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::IsCallerChrome()) {
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
  nsCOMPtr<nsIDocShell> openerDocShell = openerPwin->GetDocShell();

  if (openerDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    openerDocShell->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      uint32_t appType;
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
  if (aOpener && !nsContentUtils::IsCallerChrome()) {
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
  if (mDocShell)
    mDocShell->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetName(const nsAString& aName)
{
  FORWARD_TO_OUTER(SetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsresult result = NS_OK;
  if (mDocShell)
    result = mDocShell->SetName(PromiseFlatString(aName).get());
  return result;
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


NS_IMETHODIMP
nsGlobalWindow::GetInnerWidth(int32_t* aInnerWidth)
{
  FORWARD_TO_OUTER(GetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  EnsureSizeUpToDate();

  NS_ENSURE_STATE(mDocShell);

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
nsGlobalWindow::SetInnerWidth(int32_t aInnerWidth)
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

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aInnerWidth, nullptr),
                    NS_ERROR_FAILURE);


  nsRefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

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
    int32_t height = 0;
    int32_t width  = 0;

    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
    docShellAsWin->GetSize(&width, &height);
    width  = CSSToDevIntPixels(aInnerWidth);
    return SetDocShellWidthAndHeight(width, height);
  }
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerHeight(int32_t* aInnerHeight)
{
  FORWARD_TO_OUTER(GetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  EnsureSizeUpToDate();

  NS_ENSURE_STATE(mDocShell);

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
nsGlobalWindow::SetInnerHeight(int32_t aInnerHeight)
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

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nullptr, &aInnerHeight),
                    NS_ERROR_FAILURE);

  nsRefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

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
    int32_t height = 0;
    int32_t width  = 0;

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
nsGlobalWindow::GetOuterWidth(int32_t* aOuterWidth)
{
  FORWARD_TO_OUTER(GetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterWidth = sizeCSSPixels.width;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterHeight(int32_t* aOuterHeight)
{
  FORWARD_TO_OUTER(GetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  nsIntSize sizeCSSPixels;
  nsresult rv = GetOuterSize(&sizeCSSPixels);
  NS_ENSURE_SUCCESS(rv, rv);

  *aOuterHeight = sizeCSSPixels.height;
  return NS_OK;
}

nsresult
nsGlobalWindow::SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth)
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
                        aIsWidth ? &aLengthCSSPixels : nullptr,
                        aIsWidth ? nullptr : &aLengthCSSPixels),
                    NS_ERROR_FAILURE);

  int32_t width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  int32_t lengthDevPixels = CSSToDevIntPixels(aLengthCSSPixels);
  if (aIsWidth) {
    width = lengthDevPixels;
  } else {
    height = lengthDevPixels;
  }
  return treeOwnerAsWin->SetSize(width, height, true);    
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterWidth(int32_t aOuterWidth)
{
  FORWARD_TO_OUTER(SetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterWidth, true);
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterHeight(int32_t aOuterHeight)
{
  FORWARD_TO_OUTER(SetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  return SetOuterSize(aOuterHeight, false);
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenX(int32_t* aScreenX)
{
  FORWARD_TO_OUTER(GetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  int32_t x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenX = DevToCSSIntPixels(x);
  return NS_OK;
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
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
nsGlobalWindow::GetDevicePixelRatio(float* aRatio)
{
  FORWARD_TO_OUTER(GetDevicePixelRatio, (aRatio), NS_ERROR_NOT_INITIALIZED);

  *aRatio = 1.0;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return NS_OK;

  *aRatio = float(nsPresContext::AppUnitsPerCSSPixel())/
      presContext->AppUnitsPerDevPixel();
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozPaintCount(uint64_t* aResult)
{
  FORWARD_TO_OUTER(GetMozPaintCount, (aResult), NS_ERROR_NOT_INITIALIZED);

  *aResult = 0;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell)
    return NS_OK;

  *aResult = presShell->GetPaintCount();
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MozRequestAnimationFrame(nsIFrameRequestCallback* aCallback,
                                         int32_t *aHandle)
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
nsGlobalWindow::MozCancelRequestAnimationFrame(int32_t aHandle)
{
  return MozCancelAnimationFrame(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::MozCancelAnimationFrame(int32_t aHandle)
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
nsGlobalWindow::GetMozAnimationStartTime(int64_t *aTime)
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

  *aResult = nullptr;

  // We need this now to ensure that we have a non-null |presContext|
  // when we ought to.
  // This is similar to EnsureSizeUpToDate, but only flushes frames.
  nsGlobalWindow *parent = static_cast<nsGlobalWindow*>(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Frames);
  }

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
nsGlobalWindow::SetScreenX(int32_t aScreenX)
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

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aScreenX, nullptr),
                    NS_ERROR_FAILURE);

  int32_t x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  x = CSSToDevIntPixels(aScreenX);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenY(int32_t* aScreenY)
{
  FORWARD_TO_OUTER(GetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  int32_t x, y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  *aScreenY = DevToCSSIntPixels(y);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenY(int32_t aScreenY)
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

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(nullptr, &aScreenY),
                    NS_ERROR_FAILURE);

  int32_t x, y;
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
nsGlobalWindow::CheckSecurityWidthAndHeight(int32_t* aWidth, int32_t* aHeight)
{
#ifdef MOZ_XUL
  if (!nsContentUtils::IsCallerChrome()) {
    // if attempting to resize the window, hide any open popups
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsContentUtils::HidePopupsInDocument(doc);
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

  return NS_OK;
}

// NOTE: Arguments to this function should have values in device pixels
nsresult
nsGlobalWindow::SetDocShellWidthAndHeight(int32_t aInnerWidth, int32_t aInnerHeight)
{
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(mDocShell, aInnerWidth, aInnerHeight),
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
nsGlobalWindow::CheckSecurityLeftAndTop(int32_t* aLeft, int32_t* aTop)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!nsContentUtils::IsCallerChrome()) {
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

  return NS_OK;
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

nsresult
nsGlobalWindow::GetScrollMaxXY(int32_t* aScrollMaxX, int32_t* aScrollMaxY)
{
  FORWARD_TO_OUTER(GetScrollMaxXY, (aScrollMaxX, aScrollMaxY),
                   NS_ERROR_NOT_INITIALIZED);

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (!sf)
    return NS_OK;

  nsRect scrollRange = sf->GetScrollRange();

  if (aScrollMaxX)
    *aScrollMaxX = std::max(0,
      (int32_t)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.XMost())));
  if (aScrollMaxY)
    *aScrollMaxY = std::max(0,
      (int32_t)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrollRange.YMost())));

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxX(int32_t* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  *aScrollMaxX = 0;
  return GetScrollMaxXY(aScrollMaxX, nullptr);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxY(int32_t* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  *aScrollMaxY = 0;
  return GetScrollMaxXY(nullptr, aScrollMaxY);
}

nsresult
nsGlobalWindow::GetScrollXY(int32_t* aScrollX, int32_t* aScrollY,
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

  nsIntPoint scrollPosCSSPixels = sf->GetScrollPositionCSSPixels();
  if (aScrollX) {
    *aScrollX = scrollPosCSSPixels.x;
  }
  if (aScrollY) {
    *aScrollY = scrollPosCSSPixels.y;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollX(int32_t* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  *aScrollX = 0;
  return GetScrollXY(aScrollX, nullptr, false);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollY(int32_t* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  *aScrollY = 0;
  return GetScrollXY(nullptr, aScrollY, false);
}

uint32_t
nsGlobalWindow::GetLength()
{
  FORWARD_TO_OUTER(GetLength, (), 0);

  nsDOMWindowList* windows = GetWindowList();
  NS_ENSURE_TRUE(windows, 0);

  return windows->GetLength();
}

NS_IMETHODIMP
nsGlobalWindow::GetLength(uint32_t* aLength)
{
  *aLength = GetLength();
  return NS_OK;
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

void
nsGlobalWindow::RefreshCompartmentPrincipal()
{
  FORWARD_TO_INNER(RefreshCompartmentPrincipal, (), /* void */ );

  JS_SetCompartmentPrincipals(js::GetObjectCompartment(mJSObject),
                              nsJSPrincipals::get(mDoc->NodePrincipal()));
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  nsIDocShellTreeItem *callerItem = nullptr;

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
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  nsIWidget *widget = nullptr;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(&widget);
  }

  return widget;
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
  int32_t itemType;
  mDocShell->GetItemType(&itemType);
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
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsIDocument::ExitFullscreen(doc, /* async */ false);
  }

  if (!mWakeLock && mFullScreen) {
    nsCOMPtr<nsIPowerManagerService> pmService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(pmService, NS_OK);

    pmService->NewWakeLock(NS_LITERAL_STRING("DOM_Fullscreen"), this, getter_AddRefs(mWakeLock));
  } else if (mWakeLock && !mFullScreen) {
    mWakeLock->Unlock();
    mWakeLock = NULL;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFullScreen(bool* aFullScreen)
{
  FORWARD_TO_OUTER(GetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  if (mDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
    if (rootItem != mDocShell) {
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
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
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
#ifdef XP_WIN
    if (IsDebuggerPresent()) {
      OutputDebugStringA(cstr);
    }
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
  NS_ASSERTION(IsOuterWindow(), "EnsureReflowFlushAndPaint() must be called on"
               "the outer window");
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
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    if (treeOwner) {
      uint32_t itemCount;
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

  bool needToPromptForAbuse;
  if (DialogsAreBlocked(&needToPromptForAbuse)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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
                             nullptr);
  if (needToPromptForAbuse) {
    bool disallowDialog = false;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = prompt->AlertCheck(title.get(), final.get(), label.get(),
                            &disallowDialog);
    if (disallowDialog)
      PreventFurtherDialogs(false);
  } else {
    rv = prompt->Alert(title.get(), final.get());
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Confirm(const nsAString& aString, bool* aReturn)
{
  FORWARD_TO_OUTER(Confirm, (aString, aReturn), NS_ERROR_NOT_INITIALIZED);

  bool needToPromptForAbuse;
  if (DialogsAreBlocked(&needToPromptForAbuse)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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
                             nullptr);
  if (needToPromptForAbuse) {
    bool disallowDialog = false;
    nsXPIDLString label;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);

    rv = prompt->ConfirmCheck(title.get(), final.get(), label.get(),
                              &disallowDialog, aReturn);
    if (disallowDialog)
      PreventFurtherDialogs(false);
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

  bool needToPromptForAbuse;
  if (DialogsAreBlocked(&needToPromptForAbuse)) {
    return NS_ERROR_NOT_AVAILABLE;
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
  if (needToPromptForAbuse) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDialogLabel", label);
  }

  nsAutoSyncOperation sync(GetCurrentInnerWindowInternal() ? 
                             GetCurrentInnerWindowInternal()->mDoc :
                             nullptr);
  bool ok;
  rv = prompt->Prompt(title.get(), fixedMessage.get(),
                      &inoutValue, label.get(), &disallowDialog, &ok);

  if (disallowDialog) {
    PreventFurtherDialogs(false);
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

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
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
  int32_t itemType = nsIDocShellTreeItem::typeContent;
  mDocShell->GetItemType(&itemType);
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
  mDocShell->GetParent(getter_AddRefs(parentDsti));

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
      uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
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

  // If embedding apps don't implement nsIEmbeddingSiteWindow, we
  // shouldn't throw exceptions to web content.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nullptr.
    rv = siteWindow->Blur();

    // if the root is focused, clear the focus
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (fm && mDocument) {
      nsCOMPtr<nsIDOMElement> element;
      fm->GetFocusedElementForWindow(this, false, nullptr, getter_AddRefs(element));
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
    int32_t firstPipe = homeURL.FindChar('|');

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
                       nullptr,
                       nullptr,
                       nullptr);
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

  bool needToPromptForAbuse;
  if (DialogsAreBlocked(&needToPromptForAbuse)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (needToPromptForAbuse && !ConfirmDialogIfNeeded()) {
    return NS_ERROR_NOT_AVAILABLE;
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

      nsCOMPtr<nsIDOMWindow> callerWin = EnterModalState();
      webBrowserPrint->Print(printSettings, nullptr);
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
      webBrowserPrint->Print(printSettings, nullptr);
    }
  }
#endif //NS_PRINTING

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveTo(int32_t aXPos, int32_t aYPos)
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
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif)
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

  int32_t x, y;
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
nsGlobalWindow::ResizeTo(int32_t aWidth, int32_t aHeight)
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
nsGlobalWindow::ResizeBy(int32_t aWidthDif, int32_t aHeightDif)
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

  int32_t width, height;
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

  int32_t width, height;
  NS_ENSURE_SUCCESS(markupViewer->GetContentSize(&width, &height),
                    NS_ERROR_FAILURE);

  // Make sure the new size is following the CheckSecurityWidthAndHeight
  // rules.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  nsIntSize cssSize(DevToCSSIntPixels(nsIntSize(width, height)));
  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&cssSize.width,
                                                &cssSize.height),
                    NS_ERROR_FAILURE);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(mDocShell,
                                           newDevSize.width, newDevSize.height),
                    NS_ERROR_FAILURE);

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
    return nullptr;

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

NS_IMETHODIMP
nsGlobalWindow::Scroll(int32_t aXScroll, int32_t aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP
nsGlobalWindow::ScrollTo(int32_t aXScroll, int32_t aYScroll)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and substracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const int32_t maxpx = nsPresContext::AppUnitsToIntCSSPixels(0x7fffffff) - 4;

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
nsGlobalWindow::ScrollBy(int32_t aXScrollDif, int32_t aYScrollDif)
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

NS_IMETHODIMP
nsGlobalWindow::ClearTimeout(int32_t aHandle)
{
  if (aHandle <= 0) {
    return NS_OK;
  }

  return ClearTimeoutOrInterval(aHandle);
}

NS_IMETHODIMP
nsGlobalWindow::ClearInterval(int32_t aHandle)
{
  if (aHandle <= 0) {
    return NS_OK;
  }

  return ClearTimeoutOrInterval(aHandle);
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
nsGlobalWindow::CaptureEvents(int32_t aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfCaptureEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ReleaseEvents(int32_t aEventFlags)
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
    uint32_t permission = nsIPopupWindowManager::ALLOW_POPUP;
    pm->TestPermission(doc->NodePrincipal(), &permission);
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
      event->SetTrusted(true);

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
  
  int32_t type = nsIDocShellTreeItem::typeChrome;
  mDocShell->GetItemType(&type);
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
  jsval *argv = nullptr;

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

    return nullptr;
  }

  return JS_GetScriptedGlobal(cx);
}

nsGlobalWindow*
nsGlobalWindow::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    NS_ERROR("Please don't call this method from C++!");

    return nullptr;
  }

  JSObject *scope = CallerGlobal();
  JSAutoCompartment ac(cx, scope);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, scope, getter_AddRefs(wrapper));
  if (!wrapper)
    return nullptr;

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
      mMessage(nullptr),
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
                               uint32_t tag,
                               uint32_t data,
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
    return runtimeCallbacks->read(cx, reader, tag, data, nullptr);
  }

  return nullptr;
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

  return JS_FALSE;
}

JSStructuredCloneCallbacks kPostMessageCallbacks = {
  PostMessageReadStructuredClone,
  PostMessageWriteStructuredClone,
  nullptr
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
  JSContext* cx = nullptr;
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
  mMessage = nullptr;
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

  message->SetTrusted(mTrustedCaller);
  nsEvent *internalEvent = message->GetInternalNSEvent();

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
                               const jsval& aTransfer,
                               JSContext* aCx)
{
  FORWARD_TO_OUTER(PostMessageMoz, (aMessage, aOrigin, aTransfer, aCx),
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
                         ? nullptr
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         this,
                         providedOrigin,
                         nsContentUtils::IsCallerChrome());

  // We *must* clone the data here, or the jsval could be modified
  // by script
  JSAutoStructuredCloneBuffer buffer;
  StructuredCloneInfo scInfo;
  scInfo.event = event;

  nsIPrincipal* principal = GetPrincipal();
  if (NS_FAILED(callerPrin->Subsumes(principal, &scInfo.subsumes)))
    return NS_ERROR_DOM_DATA_CLONE_ERR;

  if (!buffer.write(aCx, aMessage, aTransfer, &kPostMessageCallbacks, &scInfo))
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

  if (!mDocShell || IsInModalState() ||
      (IsFrame() && !mDocShell->GetIsBrowserOrApp())) {
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

  // Don't allow scripts from content to close non-app windows that were not
  // opened by script.
  if (!mDocShell->GetIsApp() &&
      !mHadOriginalOpener && !nsContentUtils::IsCallerChrome()) {
    bool allowClose = mAllowScriptsToClose ||
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

  JSContext *cx = nullptr;

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

    CleanUp(false);
  }
}

nsIDOMWindow *
nsGlobalWindow::EnterModalState()
{
  // GetScriptableTop, not GetTop, so that EnterModalState works properly with
  // <iframe mozbrowser>.
  nsGlobalWindow* topWin = GetScriptableTop();

  if (!topWin) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");

    return nullptr;
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

      activeShell->SetCapturingContent(nullptr, 0);

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
      mSuspendedDoc = nullptr;
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
nsGlobalWindow::LeaveModalState(nsIDOMWindow *aCallerWin)
{
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
      nsCOMPtr<nsIDocument> currentDoc =
        do_QueryInterface(topWin->GetExtantDocument());
      mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(currentDoc == mSuspendedDoc);
      mSuspendedDoc = nullptr;
    }
  }

  if (aCallerWin) {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aCallerWin));
    if (sgo) {
      nsIScriptContext *scx = sgo->GetContext();
      if (scx)
        scx->LeaveModalState();
    }
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

      JSObject* obj = currentInner->FastGetGlobalJSObject();
      // We only want to nuke wrappers for the chrome->content case
      if (obj && !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
        JSContext* cx =
          nsContentUtils::ThreadJSContextStack()->GetSafeJSContext();

        JSAutoRequest ar(cx);
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
  JSObject* handler = nullptr;
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

    nsContentUtils::HoldJSObjects(thisSupports, participant);
  }

  mCachedXBLPrototypeHandlers.Put(aKey, aHandler.get());
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
  FORWARD_TO_OUTER(GetScriptableFrameElement, (aFrameElement), NS_ERROR_NOT_INITIALIZED);
  *aFrameElement = NULL;

  if (!mDocShell || mDocShell->GetIsBrowserOrApp()) {
    return NS_OK;
  }

  return GetFrameElement(aFrameElement);
}

/**
 * nsIGlobalWindow::GetFrameElement (when called from C++) is just a wrapper
 * around GetRealFrameElement.
 */
NS_IMETHODIMP
nsGlobalWindow::GetRealFrameElement(nsIDOMElement** aFrameElement)
{
  FORWARD_TO_OUTER(GetRealFrameElement, (aFrameElement), NS_ERROR_NOT_INITIALIZED);

  *aFrameElement = NULL;

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent));

  if (!parent || parent == mDocShell) {
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

  *aRetVal = nullptr;

  if (Preferences::GetBool("dom.disable_window_showModalDialog", false))
    return NS_ERROR_NOT_AVAILABLE;

  // Before bringing up the window/dialog, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  bool needToPromptForAbuse;
  if (DialogsAreBlocked(&needToPromptForAbuse)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (needToPromptForAbuse && !ConfirmDialogIfNeeded()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIDOMWindow> dlgWin;
  nsAutoString options(NS_LITERAL_STRING("-moz-internal-modal=1,status=1"));

  ConvertDialogOptions(aOptions, options);

  options.AppendLiteral(",scrollbars=1,centerscreen=1,resizable=0");

  nsCOMPtr<nsIDOMWindow> callerWin = EnterModalState();
  uint32_t oldMicroTaskLevel = nsContentUtils::MicroTaskLevel();
  nsContentUtils::SetMicroTaskLevel(0);
  nsresult rv = OpenInternal(aURI, EmptyString(), options,
                             false,          // aDialog
                             true,           // aContentModal
                             true,           // aCalledNoScript
                             true,           // aDoJSFixups
                             true,           // aNavigate
                             nullptr, aArgs, // args
                             GetPrincipal(),    // aCalleePrincipal
                             nullptr,            // aJSCallerContext
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
  *aSelection = nullptr;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

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
    nsEventDispatcher::DispatchDOMEvent(GetOuterWindow(), nullptr, aEvent,
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

nsEventListenerManager*
nsGlobalWindow::GetListenerManager(bool aCreateIfNotFound)
{
  FORWARD_TO_INNER_CREATE(GetListenerManager, (aCreateIfNotFound), nullptr);

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
    NS_ENSURE_TRUE(outer && outer->GetCurrentInnerWindow() == this, nullptr);
  }

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
  FORWARD_TO_OUTER(GetPrivateParent, (), nullptr);

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (static_cast<nsIDOMWindow *>(this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nullptr;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetDocument();
    if (!doc)
      return nullptr;             // This is ok, just means a null parent.

    nsIScriptGlobalObject *globalObject = doc->GetScriptGlobalObject();
    if (!globalObject)
      return nullptr;             // This is ok, just means a null parent.

    parent = do_QueryInterface(globalObject);
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
  FORWARD_TO_OUTER(GetPrivateRoot, (), nullptr);

  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  NS_ASSERTION(ptop, "cannot get ptop");
  if (!ptop)
    return nullptr;

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

  *aLocation = nullptr;

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

    // widgetListener should be a nsXULWindow
    nsIWidgetListener* listener = topLevelWidget->GetWidgetListener();
    if (listener) {
      nsCOMPtr<nsIXULWindow> window = listener->GetXULWindow();
      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(window));
      topLevelWindow = do_GetInterface(req);
    }
  }
  if (topLevelWindow) {
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(topLevelWindow));
    piWin->SetActive(aActivate);
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
    nsEventDispatcher::CreateEvent(presContext, nullptr,
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
  rv = nsEventDispatcher::CreateEvent(presContext, nullptr,
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

  nsCOMPtr<nsIDOMEventTarget> outerWindow =
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
  // this is called from the inner window so use GetDocShell
  nsIDocShell* docShell = GetDocShell();
  if (!docShell)
    return;

  bool editable;
  docShell->GetEditable(&editable);
  if (editable)
    return;

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
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
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aReturn);
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
  FORWARD_TO_OUTER(GetComputedStyleHelper, (aElt, aPseudoElt,
                                            aDefaultStylesOnly, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

  if (!aElt) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presShell) {
    // Try flushing frames on our parent in case there's a pending
    // style change that will create the presshell.
    nsGlobalWindow *parent =
      static_cast<nsGlobalWindow *>(GetPrivateParent());
    if (!parent) {
      return NS_OK;
    }

    parent->FlushPendingNotifications(Flush_Frames);

    // Might have killed mDocShell
    if (!mDocShell) {
      return NS_OK;
    }

    presShell = mDocShell->GetPresShell();
    if (!presShell) {
      return NS_OK;
    }
  }

  nsCOMPtr<dom::Element> element = do_QueryInterface(aElt);
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
  nsRefPtr<nsComputedDOMStyle> compStyle =
    NS_NewComputedDOMStyle(element, aPseudoElt, presShell,
                           aDefaultStylesOnly ? nsComputedDOMStyle::eDefaultOnly :
                                                nsComputedDOMStyle::eAll);

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
    *aSessionStorage = nullptr;
    return NS_OK;
  }

  if (!Preferences::GetBool(kStorageEnabled)) {
    *aSessionStorage = nullptr;
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
          mSessionStorage = nullptr;
      }
    }
  }

  if (!mSessionStorage) {
    *aSessionStorage = nullptr;

    nsString documentURI;
    if (mDocument) {
      mDocument->GetDocumentURI(documentURI);
    }

    // If the document has the sandboxed origin flag set
    // don't allow access to localStorage.
    if (!mDoc) {
      return NS_ERROR_FAILURE;
    }

    if (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN) {
      return NS_ERROR_DOM_SECURITY_ERR;
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

    nsCOMPtr<nsIPrivacyTransitionObserver> obs = do_GetInterface(mSessionStorage);
    if (obs) {
      docShell->AddWeakPrivacyTransitionObserver(obs);
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
    *aLocalStorage = nullptr;
    return NS_OK;
  }

  if (!mLocalStorage) {
    *aLocalStorage = nullptr;

    nsresult rv;

    if (!nsDOMStorage::CanUseStorage())
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

    // If the document has the sandboxed origin flag set
    // don't allow access to localStorage.
    if (mDoc && (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsIDocShell* docShell = GetDocShell();
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);

    rv = storageManager->GetLocalStorageForPrincipal(principal,
                                                     documentURI,
                                                     loadContext && loadContext->UsePrivateBrowsing(),
                                                     getter_AddRefs(mLocalStorage));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrivacyTransitionObserver> obs = do_GetInterface(mLocalStorage);
    if (obs && docShell) {
      docShell->AddWeakPrivacyTransitionObserver(obs);
    }
  }

  NS_ADDREF(*aLocalStorage = mLocalStorage);
  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMStorageIndexedDB
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetIndexedDB(nsIIDBFactory** _retval)
{
  if (!mIndexedDB) {
    nsresult rv;

    // If the document has the sandboxed origin flag set
    // don't allow access to indexedDB.
    if (mDoc && (mDoc->GetSandboxFlags() & SANDBOXED_ORIGIN)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (!IsChromeWindow()) {
      nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID);
      NS_ENSURE_TRUE(thirdPartyUtil, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      bool isThirdParty;
      rv = thirdPartyUtil->IsThirdPartyWindow(this, nullptr, &isThirdParty);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      if (isThirdParty) {
        NS_WARNING("IndexedDB is not permitted in a third-party window.");
        *_retval = nullptr;
        return NS_OK;
      }
    }

    // This may be null if being created from a file.
    rv = indexedDB::IDBFactory::Create(this, nullptr,
                                       getter_AddRefs(mIndexedDB));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIIDBFactory> request(mIndexedDB);
  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMozIndexedDB(nsIIDBFactory** _retval)
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
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    NS_WARNING("Using deprecated nsIDocCharset: use nsIDocShell.GetCharset() instead ");
    nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(mDocShell));
    docCharset.forget(aSink);
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    webNav.forget(aSink);
  }
  else if (aIID.Equals(NS_GET_IID(nsIDocShell))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDocShell> docShell = mDocShell;
    docShell.forget(aSink);
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        webBrowserPrint.forget(aSink);
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
  else if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(mDocShell));
    loadContext.forget(aSink);
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

uint32_t
nsGlobalWindow::FindInsertionIndex(IdleObserverHolder* aIdleObserver)
{
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

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    mCurrentlyIdle = true;
    if (IsFrozen()) {
      // need to fire only one idle event while the window is frozen.
      mNotifyIdleObserversIdleOnThaw = true;
      mNotifyIdleObserversActiveOnThaw = false;
    } else if (mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == this) {
      HandleIdleActiveEvent();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_ACTIVE)) {
    mCurrentlyIdle = false;
    if (IsFrozen()) {
      mNotifyIdleObserversActiveOnThaw = true;
      mNotifyIdleObserversIdleOnThaw = false;
    } else if (mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == this) {
      ScheduleActiveTimerCallback();
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

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
    bool isPrivate = loadContext && loadContext->UsePrivateBrowsing();
    if (pistorage->IsPrivate() != isPrivate) {
      return NS_OK;
    }

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

    event->SetTrusted(true);

    if (fireMozStorageChanged) {
      nsEvent *internalEvent = event->GetInternalNSEvent();
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
    nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
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

  NS_NewDOMStorageEvent(getter_AddRefs(domEvent), nullptr, nullptr);
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
  FORWARD_TO_OUTER(GetParentInternal, (), nullptr);

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

  // Calls to window.open from script should navigate.
  MOZ_ASSERT(aCalledNoScript || aNavigate);

  *aReturn = nullptr;

  nsCOMPtr<nsIWebBrowserChrome> chrome;
  GetWebBrowserChrome(getter_AddRefs(chrome));
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
      // Push a null JSContext here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindow::OpenWindow and nsIWindowWatcher::OpenWindow.
      nsCOMPtr<nsIJSContextStack> stack;

      if (!aContentModal) {
        stack = do_GetService(sJSStackContractID);
      }

      if (stack) {
        rv = stack->Push(nullptr);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = pwwatch->OpenWindow2(this, url.get(), name_ptr, options_ptr,
                                /* aCalledFromScript = */ false,
                                aDialog, aNavigate, aExtraArgument,
                                getter_AddRefs(domReturn));

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

uint32_t sNestingLevel;

nsresult
nsGlobalWindow::SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                     int32_t interval,
                                     bool aIsInterval, int32_t *aReturn)
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
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_DOM_TYPE_ERR) ? NS_OK : rv;

  return SetTimeoutOrInterval(handler, interval, isInterval, aReturn);
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
  JSObject* scriptObject = handler->GetScriptObject();
  if (!scriptObject) {
    // Evaluate the timeout expression.
    const PRUnichar* script = handler->GetHandlerText();
    NS_ASSERTION(script, "timeout has no script nor handler text!");

    const char* filename = nullptr;
    uint32_t lineNo = 0;
    handler->GetLocation(&filename, &lineNo);

    JS::CompileOptions options(aScx->GetNativeContext());
    options.setFileAndLine(filename, lineNo)
           .setVersion(JSVERSION_DEFAULT);
    aScx->EvaluateString(nsDependentString(script), *FastGetGlobalJSObject(),
                         options, /*aCoerceToString = */ false, nullptr);
  } else {
    nsCOMPtr<nsIVariant> dummy;
    nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow *>(this));
    aScx->CallEventHandler(me, FastGetGlobalJSObject(),
                           scriptObject, handler->GetArgv(),
                           // XXXmarkh - consider allowing CallEventHandler to
                           // accept nullptr?
                           getter_AddRefs(dummy));

  }

  // We ignore any failures from calling EvaluateString() or
  // CallEventHandler() on the context here since we're in a loop
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

  nsTimeout *nextTimeout, *timeout;
  nsTimeout *last_expired_timeout, *last_insertion_point;
  nsTimeout dummy_timeout;
  uint32_t firingDepth = mTimeoutFiringDepth + 1;

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
  last_expired_timeout = nullptr;
  for (timeout = mTimeouts.getFirst(); timeout; timeout = timeout->getNext()) {
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
  dummy_timeout.mFiringDepth = firingDepth;
  dummy_timeout.mWhen = now;
  last_expired_timeout->setNext(&dummy_timeout);

  // Don't let ClearWindowTimeouts throw away our stack-allocated
  // dummy timeout.
  dummy_timeout.AddRef();
  dummy_timeout.AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  // If we ever start setting mTimeoutInsertionPoint to a non-dummy timeout,
  // the logic in ResetTimersForNonBackgroundWindow will need to change.
  mTimeoutInsertionPoint = &dummy_timeout;

  Telemetry::AutoCounter<Telemetry::DOM_TIMERS_FIRED_PER_NATIVE_TIMEOUT> timeoutsRan;

  for (timeout = mTimeouts.getFirst();
       timeout != &dummy_timeout && !IsFrozen();
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

    // The "scripts disabled" concept is still a little vague wrt
    // multiple languages.  Prepare for the day when languages can be
    // disabled independently of the other languages...
    if (!scx->GetScriptsEnabled()) {
      // Scripts were enabled once in this window (unless aTimeout ==
      // nullptr) but now scripts are disabled (we might be in
      // print-preview, for instance), this means we shouldn't run any
      // timeouts at this point.
      //
      // If scripts are enabled for this language in this window again
      // we'll fire the timeouts that are due at that point.
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
  dummy_timeout.remove();

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
    mTimer = nullptr;
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
nsGlobalWindow::ClearTimeoutOrInterval(int32_t aTimerID)
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (aTimerID), NS_ERROR_NOT_INITIALIZED);

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

nsresult
nsGlobalWindow::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!mDocShell) {
    *aTreeOwner = nullptr;

    return NS_OK;
  }

  return mDocShell->GetTreeOwner(aTreeOwner);
}

nsresult
nsGlobalWindow::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no mDocShell, this window must have been closed,
  // in that case there is no tree owner.

  if (mDocShell) {
    mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  if (!treeOwner) {
    *aTreeOwner = nullptr;
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
nsGlobalWindow::BuildURIfromBase(const char *aURL, nsIURI **aBuiltURI,
                                 bool *aFreeSecurityPass,
                                 JSContext **aCXused)
{
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = nullptr;

  *aBuiltURI = nullptr;
  *aFreeSecurityPass = false;
  if (aCXused)
    *aCXused = nullptr;

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
  nsAutoCString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  nsIURI* baseURI = nullptr;
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

already_AddRefed<nsISupports>
nsGlobalWindow::SaveWindowState()
{
  NS_PRECONDITION(IsOuterWindow(), "Can't save the inner window's state");

  if (!mContext || !mJSObject) {
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

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner,
                                                      mInnerWindowHolder);

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  return state.forget();
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

    // Suspend all of the workers for this window.
    nsIScriptContext *scx = GetContextInternal();
    JSContext *cx = scx ? scx->GetNativeContext() : nullptr;
    mozilla::dom::workers::SuspendWorkersForWindow(cx, this);

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
      for (uint32_t i = 0; i < mEnabledSensors.Length(); i++)
        ac->AddWindowListener(mEnabledSensors[i], this);
    }

    // Resume all of the workers for this window.
    nsIScriptContext *scx = GetContextInternal();
    JSContext *cx = scx ? scx->GetNativeContext() : nullptr;
    mozilla::dom::workers::ResumeWorkersForWindow(cx, this);

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

uint32_t
nsGlobalWindow::TimeoutSuspendCount()
{
  FORWARD_TO_INNER(TimeoutSuspendCount, (), 0);
  return mTimeoutsSuspendDepth;
}

void
nsGlobalWindow::EnableDeviceSensor(uint32_t aType)
{
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
nsGlobalWindow::EnableTimeChangeNotifications()
{
  nsSystemTimeChangeObserver::AddWindowListener(this);
}

void
nsGlobalWindow::DisableTimeChangeNotifications()
{
  nsSystemTimeChangeObserver::RemoveWindowListener(this);
}

// static
bool
nsGlobalWindow::HasIndexedDBSupport()
{
  return Preferences::GetBool("indexedDB.feature.enabled", true);
}

// static
bool
nsPIDOMWindow::HasPerformanceSupport()
{
  return Preferences::GetBool("dom.enable_performance", false);
}

static size_t
SizeOfEventTargetObjectsEntryExcludingThisFun(
  nsPtrHashKey<nsDOMEventTargetHelper> *aEntry,
  nsMallocSizeOfFun aMallocSizeOf,
  void *arg)
{
  nsISupports *supports = aEntry->GetKey();
  nsCOMPtr<nsISizeOfEventTarget> iface = do_QueryInterface(supports);
  return iface ? iface->SizeOfEventTargetIncludingThis(aMallocSizeOf) : 0;
}

void
nsGlobalWindow::SizeOfIncludingThis(nsWindowSizes* aWindowSizes) const
{
  aWindowSizes->mDOMOther += aWindowSizes->mMallocSizeOf(this);

  if (IsInnerWindow()) {
    nsEventListenerManager* elm =
      const_cast<nsGlobalWindow*>(this)->GetListenerManager(false);
    if (elm) {
      aWindowSizes->mDOMOther +=
        elm->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf);
    }
    if (mDoc) {
      mDoc->DocSizeOfIncludingThis(aWindowSizes);
    }
  }

  aWindowSizes->mDOMOther +=
    mNavigator ?
      mNavigator->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf) : 0;

  aWindowSizes->mDOMEventTargets +=
    mEventTargetObjects.SizeOfExcludingThis(
      SizeOfEventTargetObjectsEntryExcludingThisFun,
      aWindowSizes->mMallocSizeOf);
}

// nsGlobalChromeWindow implementation

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
  *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  int32_t aMode = 0;

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
nsGlobalChromeWindow::GetAttentionWithCycleCount(int32_t aCycleCount)
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

  NS_ENSURE_TRUE(aMouseDownEvent, NS_ERROR_FAILURE);
  nsEvent *internalEvent = aMouseDownEvent->GetInternalNSEvent();
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
  int32_t cursor;

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
    nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    nsView* rootView = vm->GetRootView();
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    nsIWidget* widget = rootView->GetNearestWidget(nullptr);
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    // Call esm and set cursor.
    rv = presContext->EventStateManager()->SetCursor(cursor, nullptr,
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
nsGlobalChromeWindow::GetMessageManager(nsIMessageBroadcaster** aManager)
{
  FORWARD_TO_INNER_CHROME(GetMessageManager, (aManager), NS_ERROR_FAILURE);
  if (!mMessageManager) {
    nsIScriptContext* scx = GetContextInternal();
    NS_ENSURE_STATE(scx);
    JSContext* cx = scx->GetNativeContext();
    NS_ENSURE_STATE(cx);
    nsCOMPtr<nsIMessageBroadcaster> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    mMessageManager =
      new nsFrameMessageManager(nullptr,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                cx,
                                MM_CHROME | MM_BROADCASTER);
    NS_ENSURE_TRUE(mMessageManager, NS_ERROR_OUT_OF_MEMORY);
  }
  CallQueryInterface(mMessageManager, aManager);
  return NS_OK;
}

// nsGlobalModalWindow implementation

// QueryInterface implementation for nsGlobalModalWindow
NS_IMPL_CYCLE_COLLECTION_INHERITED_1(nsGlobalModalWindow,
                                     nsGlobalWindow,
                                     mReturnValue)

DOMCI_DATA(ModalContentWindow, nsGlobalModalWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalModalWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMModalContentWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ModalContentWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalModalWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalModalWindow, nsGlobalWindow)


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
    *aArguments = nullptr;
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
  MOZ_ASSERT(aDocument);

  // If we're loading a new document into a modal dialog, clear the
  // return value that was set, if any, by the current document.
  mReturnValue = nullptr;

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

#ifdef MOZ_B2G
void
nsGlobalWindow::EnableNetworkEvent(uint32_t aType)
{
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
      os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC, false);
      break;
    case NS_NETWORK_DOWNLOAD_EVENT:
      os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC, false);
      break;
  }
}

void
nsGlobalWindow::DisableNetworkEvent(uint32_t aType)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  switch (aType) {
    case NS_NETWORK_UPLOAD_EVENT:
      os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC);
      break;
    case NS_NETWORK_DOWNLOAD_EVENT:
      os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC);
      break;
  }
}
#endif // MOZ_B2G

#define EVENT(name_, id_, type_, struct_)                                    \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             jsval *vp) {                    \
    EventHandlerNonNull* h = GetOn##name_();                                 \
    vp->setObjectOrNull(h ? h->Callable() : nullptr);                        \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             const jsval &v) {               \
    JSObject *obj = mJSObject;                                               \
    if (!obj) {                                                              \
      /* Just silently do nothing */                                         \
      return NS_OK;                                                          \
    }                                                                        \
    nsRefPtr<EventHandlerNonNull> handler;                                   \
    JSObject *callable;                                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      bool ok;                                                               \
      handler = new EventHandlerNonNull(cx, obj, callable, &ok);             \
      if (!ok) {                                                             \
        return NS_ERROR_OUT_OF_MEMORY;                                       \
      }                                                                      \
    }                                                                        \
    ErrorResult rv;                                                          \
    SetOn##name_(handler, rv);                                               \
    return rv.ErrorCode();                                                   \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                              \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             jsval *vp) {                    \
    nsEventListenerManager *elm = GetListenerManager(false);                 \
    if (elm) {                                                               \
      OnErrorEventHandlerNonNull* h = elm->GetOnErrorEventHandler();         \
      if (h) {                                                               \
        *vp = JS::ObjectValue(*h->Callable());                               \
        return NS_OK;                                                        \
      }                                                                      \
    }                                                                        \
    *vp = JSVAL_NULL;                                                        \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             const jsval &v) {               \
    nsEventListenerManager *elm = GetListenerManager(true);                  \
    if (!elm) {                                                              \
      return NS_ERROR_OUT_OF_MEMORY;                                         \
    }                                                                        \
                                                                             \
    JSObject *obj = mJSObject;                                               \
    if (!obj) {                                                              \
      return NS_ERROR_UNEXPECTED;                                            \
    }                                                                        \
    nsRefPtr<OnErrorEventHandlerNonNull> handler;                            \
    JSObject *callable;                                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      bool ok;                                                               \
      handler = new OnErrorEventHandlerNonNull(cx, obj, callable, &ok);      \
      if (!ok) {                                                             \
        return NS_ERROR_OUT_OF_MEMORY;                                       \
      }                                                                      \
    }                                                                        \
    return elm->SetEventHandler(handler);                                    \
  }
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                       \
  NS_IMETHODIMP nsGlobalWindow::GetOn##name_(JSContext *cx,                  \
                                             jsval *vp) {                    \
    nsEventListenerManager *elm = GetListenerManager(false);                 \
    if (elm) {                                                               \
      BeforeUnloadEventHandlerNonNull* h =                                   \
        elm->GetOnBeforeUnloadEventHandler();                                \
      if (h) {                                                               \
        *vp = JS::ObjectValue(*h->Callable());                               \
        return NS_OK;                                                        \
      }                                                                      \
    }                                                                        \
    *vp = JSVAL_NULL;                                                        \
    return NS_OK;                                                            \
  }                                                                          \
  NS_IMETHODIMP nsGlobalWindow::SetOn##name_(JSContext *cx,                  \
                                             const jsval &v) {               \
    nsEventListenerManager *elm = GetListenerManager(true);                  \
    if (!elm) {                                                              \
      return NS_ERROR_OUT_OF_MEMORY;                                         \
    }                                                                        \
                                                                             \
    JSObject *obj = mJSObject;                                               \
    if (!obj) {                                                              \
      return NS_ERROR_UNEXPECTED;                                            \
    }                                                                        \
    nsRefPtr<BeforeUnloadEventHandlerNonNull> handler;                       \
    JSObject *callable;                                                      \
    if (v.isObject() &&                                                      \
        JS_ObjectIsCallable(cx, callable = &v.toObject())) {                 \
      bool ok;                                                               \
      handler = new BeforeUnloadEventHandlerNonNull(cx, obj, callable, &ok); \
      if (!ok) {                                                             \
        return NS_ERROR_OUT_OF_MEMORY;                                       \
      }                                                                      \
    }                                                                        \
    return elm->SetEventHandler(handler);                                    \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "nsEventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef BEFOREUNLOAD_EVENT
#undef ERROR_EVENT
#undef EVENT

