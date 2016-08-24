/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindow.h"

#include <algorithm>

#include "mozilla/MemoryReporting.h"

// Local Includes
#include "Navigator.h"
#include "nsContentSecurityManager.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsDOMNavigationTiming.h"
#include "nsIDOMStorageManager.h"
#include "mozilla/dom/DOMStorage.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/IntegerPrintfMacros.h"
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
#include "mozilla/dom/WindowOrientationObserver.h"
#endif
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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIController.h"
#include "nsScriptNameSpaceManager.h"
#include "nsISlowScriptDebug.h"
#include "nsWindowMemoryReporter.h"
#include "WindowNamedPropertiesHandler.h"
#include "nsFrameSelection.h"
#include "nsNetUtil.h"
#include "nsVariant.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jswrapper.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSEnvironment.h"
#include "ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

// Other Classes
#include "mozilla/dom/BarProps.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSPrincipals.h"
#include "mozilla/Attributes.h"
#include "mozilla/Debug.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/ProcessHangMonitor.h"
#include "AudioChannelService.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharTraits.h" // NS_IS_HIGH/LOW_SURROGATE
#include "PostMessageEvent.h"

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
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsThreadUtils.h"
#include "nsILoadContext.h"
#include "nsIPresShell.h"
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
#include "nsDOMCID.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsIScriptError.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsQueryObject.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
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
#include "mozilla/dom/CustomEvent.h"
#include "nsIJARChannel.h"
#include "nsIScreenManager.h"

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
#include "mozilla/dom/Selection.h"
#include "nsFrameLoader.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"
#include "mozilla/Logging.h"
#include "prenv.h"
#include "prprf.h"

#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/Promise.h"

#ifdef MOZ_GAMEPAD
#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadManager.h"
#endif

#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/VREventObserver.h"

#include "nsRefreshDriver.h"
#include "Layers.h"

#include "mozilla/AddonPathService.h"
#include "mozilla/BasePrincipal.h"
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
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/HashChangeEvent.h"
#include "mozilla/dom/MozSelfSupportBinding.h"
#include "mozilla/dom/PopStateEvent.h"
#include "mozilla/dom/PopupBlockedEvent.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsITabChild.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/U2F.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#ifdef HAVE_SIDEBAR
#include "mozilla/dom/ExternalBinding.h"
#endif

#ifdef MOZ_WEBSPEECH
#include "mozilla/dom/SpeechSynthesis.h"
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
using mozilla::BasePrincipal;
using mozilla::PrincipalOriginAttributes;
using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::cache::CacheStorage;

static LazyLogModule gDOMLeakPRLog("DOMLeak");

nsGlobalWindow::WindowByIdTable *nsGlobalWindow::sWindowsById = nullptr;
bool nsGlobalWindow::sWarnedAboutWindowInternal = false;
bool nsGlobalWindow::sIdleObserversAPIFuzzTimeDisabled = false;

static int32_t              gRefCnt                    = 0;
static int32_t              gOpenPopupSpamCount        = 0;
static PopupControlState    gPopupControlState         = openAbused;
static int32_t              gRunningTimeoutDepth       = 0;
static bool                 gMouseDown                 = false;
static bool                 gDragServiceDisabled       = false;
static FILE                *gDumpFile                  = nullptr;
static uint32_t             gSerialCounter             = 0;
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
  // Don't use the background timeout value when there are audio contexts
  // present, so that baackground audio can keep running smoothly. (bug 1181073)
  bool isBackground = mAudioContexts.IsEmpty() &&
    (!mOuterWindow || mOuterWindow->IsBackground());
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
    if (!AsInner()->HasActiveDocument()) {                                    \
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
  MOZ_RELEASE_ASSERT(IsInnerWindow());                                        \
  nsGlobalWindow *outer = GetOuterWindowInternal();                           \
  if (MOZ_LIKELY(AsInner()->HasActiveDocument())) {                           \
    return outer->method args;                                                \
  }                                                                           \
  if (!outer) {                                                               \
    NS_WARNING("No outer window available!");                                 \
    errorresult.Throw(NS_ERROR_NOT_INITIALIZED);                              \
  } else {                                                                    \
    errorresult.Throw(NS_ERROR_XPC_SECURITY_MANAGER_VETO);                    \
  }                                                                           \
  return err_rval;                                                            \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!AsInner()->HasActiveDocument()) {                                    \
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
    if (!AsInner()->HasActiveDocument()) {                                    \
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
    return ((nsGlobalChromeWindow *)nsGlobalWindow::Cast(mInnerWindow))->method args; \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(method, args, err_rval)         \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!AsInner()->HasActiveDocument()) {                                    \
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
      nsCOMPtr<nsIDocument> doc = GetDoc();                                   \
      if (!mInnerWindow) {                                                    \
        return err_rval;                                                      \
      }                                                                       \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

// CIDs
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);

#define NETWORK_UPLOAD_EVENT_NAME     NS_LITERAL_STRING("moznetworkupload")
#define NETWORK_DOWNLOAD_EVENT_NAME   NS_LITERAL_STRING("moznetworkdownload")

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver final : public nsIObserver,
                                     public nsIInterfaceRequestor
{
public:
  explicit nsGlobalWindowObserver(nsGlobalWindow* aWindow) : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData) override
  {
    if (!mWindow)
      return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nullptr; }
  NS_IMETHOD GetInterface(const nsIID& aIID, void** aResult) override
  {
    if (mWindow && aIID.Equals(NS_GET_IID(nsIDOMWindow)) && mWindow) {
      return mWindow->QueryInterface(aIID, aResult);
    }
    return NS_NOINTERFACE;
  }

private:
  ~nsGlobalWindowObserver() {}

  // This reference is non-owning and safe because it's cleared by
  // nsGlobalWindow::CleanUp().
  nsGlobalWindow* MOZ_NON_OWNING_REF mWindow;
};

NS_IMPL_ISUPPORTS(nsGlobalWindowObserver, nsIObserver, nsIInterfaceRequestor)

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

nsresult
nsTimeout::InitTimer(uint32_t aDelay)
{
  return mTimer->InitWithNameableFuncCallback(
    nsGlobalWindow::TimerCallback, this, aDelay,
    nsITimer::TYPE_ONE_SHOT, nsGlobalWindow::TimerNameCallback);
}

// Return true if this timeout has a refcount of 1. This is used to check
// that dummy_timeout doesn't leak from nsGlobalWindow::RunTimeout.
bool
nsTimeout::HasRefCntOne()
{
  return mRefCnt.get() == 1;
}

static already_AddRefed<nsIVariant>
CreateVoidVariant()
{
  RefPtr<nsVariantCC> writable = new nsVariantCC();
  writable->SetAsVoid();
  return writable.forget();
}

nsresult
DialogValueHolder::Get(nsIPrincipal* aSubject, nsIVariant** aResult)
{
  nsCOMPtr<nsIVariant> result;
  if (aSubject->SubsumesConsideringDomain(mOrigin)) {
    result = mValue;
  } else {
    result = CreateVoidVariant();
  }
  result.forget(aResult);
  return NS_OK;
}

void
DialogValueHolder::Get(JSContext* aCx, JS::Handle<JSObject*> aScope,
                       nsIPrincipal* aSubject,
                       JS::MutableHandle<JS::Value> aResult,
                       mozilla::ErrorResult& aError)
{
  if (aSubject->Subsumes(mOrigin)) {
    aError = nsContentUtils::XPConnect()->VariantToJS(aCx, aScope,
                                                      mValue, aResult);
  } else {
    aResult.setUndefined();
  }
}

namespace mozilla {
namespace dom {
extern uint64_t
NextWindowID();
} // namespace dom
} // namespace mozilla

template<class T>
nsPIDOMWindow<T>::nsPIDOMWindow(nsPIDOMWindowOuter *aOuterWindow)
: mFrameElement(nullptr), mDocShell(nullptr), mModalStateDepth(0),
  mRunningTimeout(nullptr), mMutationBits(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nullptr),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mMayHavePointerEnterLeaveEventListener(false),
  mInnerObjectsFreed(false),
  mIsModalContentWindow(false),
  mIsActive(false), mIsBackground(false),
  mMediaSuspend(nsISuspendedTypes::NONE_SUSPENDED),
  mAudioMuted(false), mAudioVolume(1.0), mAudioCaptured(false),
  mDesktopModeViewport(false), mInnerWindow(nullptr),
  mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(NextWindowID()), mHasNotifiedGlobalCreated(false),
  mMarkedCCGeneration(0), mServiceWorkersTestingEnabled(false)
 {}

template<class T>
nsPIDOMWindow<T>::~nsPIDOMWindow() {}

/* static */
nsPIDOMWindowOuter*
nsPIDOMWindowOuter::GetFromCurrentInner(nsPIDOMWindowInner* aInner)
{
  if (!aInner) {
    return nullptr;
  }

  nsPIDOMWindowOuter* outer = aInner->GetOuterWindow();
  if (!outer || outer->GetCurrentInnerWindow() != aInner) {
    return nullptr;
  }

  return outer;
}

// DialogValueHolder CC goop.
NS_IMPL_CYCLE_COLLECTION(DialogValueHolder, mValue)

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
  constexpr nsOuterWindowProxy() : js::Wrapper(0) { }

  virtual bool finalizeInBackground(JS::Value priv) const override {
    return false;
  }

  // Standard internal methods
  virtual bool getOwnPropertyDescriptor(JSContext* cx,
                                        JS::Handle<JSObject*> proxy,
                                        JS::Handle<jsid> id,
                                        JS::MutableHandle<JS::PropertyDescriptor> desc)
                                        const override;
  virtual bool defineProperty(JSContext* cx,
                              JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id,
                              JS::Handle<JS::PropertyDescriptor> desc,
                              JS::ObjectOpResult &result) const override;
  virtual bool ownPropertyKeys(JSContext *cx,
                               JS::Handle<JSObject*> proxy,
                               JS::AutoIdVector &props) const override;
  virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id,
                       JS::ObjectOpResult &result) const override;

  virtual bool getPrototypeIfOrdinary(JSContext* cx,
                                      JS::Handle<JSObject*> proxy,
                                      bool* isOrdinary,
                                      JS::MutableHandle<JSObject*> protop) const override;

  virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::MutableHandle<JSObject*> vp) const override;
  virtual bool preventExtensions(JSContext* cx,
                                 JS::Handle<JSObject*> proxy,
                                 JS::ObjectOpResult& result) const override;
  virtual bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
                            const override;
  virtual bool has(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<jsid> id, bool *bp) const override;
  virtual bool get(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<JS::Value> receiver,
                   JS::Handle<jsid> id,
                   JS::MutableHandle<JS::Value> vp) const override;
  virtual bool set(JSContext *cx, JS::Handle<JSObject*> proxy,
                   JS::Handle<jsid> id, JS::Handle<JS::Value> v,
                   JS::Handle<JS::Value> receiver,
                   JS::ObjectOpResult &result) const override;

  // SpiderMonkey extensions
  virtual bool getPropertyDescriptor(JSContext* cx,
                                     JS::Handle<JSObject*> proxy,
                                     JS::Handle<jsid> id,
                                     JS::MutableHandle<JS::PropertyDescriptor> desc)
                                     const override;
  virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id, bool *bp) const override;
  virtual bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                            JS::AutoIdVector &props) const override;
  virtual const char *className(JSContext *cx,
                                JS::Handle<JSObject*> wrapper) const override;

  virtual void finalize(JSFreeOp *fop, JSObject *proxy) const override;

  virtual bool isCallable(JSObject *obj) const override {
    return false;
  }
  virtual bool isConstructor(JSObject *obj) const override {
    return false;
  }

  virtual bool watch(JSContext *cx, JS::Handle<JSObject*> proxy,
                     JS::Handle<jsid> id, JS::Handle<JSObject*> callable) const override;
  virtual bool unwatch(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id) const override;

  static void ObjectMoved(JSObject *obj, const JSObject *old);

  static const nsOuterWindowProxy singleton;

protected:
  static nsGlobalWindow* GetOuterWindow(JSObject *proxy)
  {
    nsGlobalWindow* outerWindow = nsGlobalWindow::FromSupports(
      static_cast<nsISupports*>(js::GetProxyExtra(proxy, 0).toPrivate()));
    MOZ_ASSERT_IF(outerWindow, outerWindow->IsOuterWindow());
    return outerWindow;
  }

  // False return value means we threw an exception.  True return value
  // but false "found" means we didn't have a subframe at that index.
  bool GetSubframeWindow(JSContext *cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp,
                         bool &found) const;

  // Returns a non-null window only if id is an index and we have a
  // window at that index.
  already_AddRefed<nsPIDOMWindowOuter>
  GetSubframeWindow(JSContext *cx,
                    JS::Handle<JSObject*> proxy,
                    JS::Handle<jsid> id) const;

  bool AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                  JS::AutoIdVector &props) const;
};

static const js::ClassExtension OuterWindowProxyClassExtension = PROXY_MAKE_EXT(
    nsOuterWindowProxy::ObjectMoved
);

const js::Class OuterWindowProxyClass = PROXY_CLASS_WITH_EXT(
    "Proxy",
    0, /* additional class flags */
    &OuterWindowProxyClassExtension);

const char *
nsOuterWindowProxy::className(JSContext *cx, JS::Handle<JSObject*> proxy) const
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "Window";
}

void
nsOuterWindowProxy::finalize(JSFreeOp *fop, JSObject *proxy) const
{
  nsGlobalWindow* outerWindow = GetOuterWindow(proxy);
  if (outerWindow) {
    outerWindow->ClearWrapper();

    // Ideally we would use OnFinalize here, but it's possible that
    // EnsureScriptEnvironment will later be called on the window, and we don't
    // want to create a new script object in that case. Therefore, we need to
    // write a non-null value that will reliably crash when dereferenced.
    outerWindow->PoisonOuterWindowProxy(proxy);
  }
}

bool
nsOuterWindowProxy::getPropertyDescriptor(JSContext* cx,
                                          JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JS::PropertyDescriptor> desc) const
{
  // The only thing we can do differently from js::Wrapper is shadow stuff with
  // our indexed properties, so we can just try getOwnPropertyDescriptor and if
  // that gives us nothing call on through to js::Wrapper.
  desc.object().set(nullptr);
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc)) {
    return false;
  }

  if (desc.object()) {
    return true;
  }

  return js::Wrapper::getPropertyDescriptor(cx, proxy, id, desc);
}

bool
nsOuterWindowProxy::getOwnPropertyDescriptor(JSContext* cx,
                                             JS::Handle<JSObject*> proxy,
                                             JS::Handle<jsid> id,
                                             JS::MutableHandle<JS::PropertyDescriptor> desc)
                                             const
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

  // When we change this to always claim the property is configurable (bug
  // 1178639), update the comments in nsOuterWindowProxy::defineProperty
  // accordingly.
  return js::Wrapper::getOwnPropertyDescriptor(cx, proxy, id, desc);
}

bool
nsOuterWindowProxy::defineProperty(JSContext* cx,
                                   JS::Handle<JSObject*> proxy,
                                   JS::Handle<jsid> id,
                                   JS::Handle<JS::PropertyDescriptor> desc,
                                   JS::ObjectOpResult &result) const
{
  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Spec says to Reject whether this is a supported index or not,
    // since we have no indexed setter or indexed creator.  It is up
    // to the caller to decide whether to throw a TypeError.
    return result.failCantDefineWindowElement();
  }

#ifndef RELEASE_BUILD // To be turned on in bug 1178638.
  // For now, allow chrome code to define non-configurable properties
  // on windows, until we sort out what exactly the addon SDK is
  // doing.  In the meantime, this still allows us to test web compat
  // behavior.
  if (desc.hasConfigurable() && !desc.configurable() &&
      !nsContentUtils::IsCallerChrome()) {
    return ThrowErrorMessage(cx, MSG_DEFINE_NON_CONFIGURABLE_PROP_ON_WINDOW);
  }

  // Note that if hasConfigurable() is false we do NOT want to
  // setConfigurable(true).  That would make this code:
  //
  //   var x;
  //   window.x = 5;
  //
  // fail, because the JS engine ends up converting the assignment into a define
  // with !hasConfigurable(), but the var actually declared a non-configurable
  // property on our underlying Window object, so the set would fail if we
  // forced setConfigurable(true) here.  What we want to do instead is change
  // getOwnPropertyDescriptor to always claim configurable.  See bug 1178639.
#endif

  return js::Wrapper::defineProperty(cx, proxy, id, desc, result);
}

bool
nsOuterWindowProxy::ownPropertyKeys(JSContext *cx,
                                    JS::Handle<JSObject*> proxy,
                                    JS::AutoIdVector &props) const
{
  // Just our indexed stuff followed by our "normal" own property names.
  if (!AppendIndexedPropertyNames(cx, proxy, props)) {
    return false;
  }

  JS::AutoIdVector innerProps(cx);
  if (!js::Wrapper::ownPropertyKeys(cx, proxy, innerProps)) {
    return false;
  }
  return js::AppendUnique(cx, props, innerProps);
}

bool
nsOuterWindowProxy::delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id, JS::ObjectOpResult &result) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    // Fail (which means throw if strict, else return false).
    return result.failCantDeleteWindowElement();
  }

  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Indexed, but not supported.  Spec says return true.
    return result.succeed();
  }

  return js::Wrapper::delete_(cx, proxy, id, result);
}

bool
nsOuterWindowProxy::getPrototypeIfOrdinary(JSContext* cx,
                                           JS::Handle<JSObject*> proxy,
                                           bool* isOrdinary,
                                           JS::MutableHandle<JSObject*> protop) const
{
  // Window's [[GetPrototypeOf]] trap isn't the ordinary definition:
  //
  //   https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getprototypeof
  //
  // We nonetheless can implement it with a static [[Prototype]], because
  // wrapper-class handlers (particularly, XOW in FilteringWrapper.cpp) supply
  // all non-ordinary behavior.
  //
  // But from a spec point of view, it's the exact same object in both cases --
  // only the observer's changed.  So this getPrototypeIfOrdinary trap on the
  // non-wrapper object *must* report non-ordinary, even if static [[Prototype]]
  // usually means ordinary.
  *isOrdinary = false;
  return true;
}

bool
nsOuterWindowProxy::preventExtensions(JSContext* cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::ObjectOpResult& result) const
{
  // If [[Extensible]] could be false, then navigating a window could navigate
  // to a window that's [[Extensible]] after being at one that wasn't: an
  // invariant violation.  So never change a window's extensibility.
  return result.failCantPreventExtensions();
}

bool
nsOuterWindowProxy::isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy,
                                 bool *extensible) const
{
  // See above.
  *extensible = true;
  return true;
}

bool
nsOuterWindowProxy::has(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<jsid> id, bool *bp) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::has(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
                           JS::Handle<jsid> id, bool *bp) const
{
  if (nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id)) {
    *bp = true;
    return true;
  }

  return js::Wrapper::hasOwn(cx, proxy, id, bp);
}

bool
nsOuterWindowProxy::get(JSContext *cx, JS::Handle<JSObject*> proxy,
                        JS::Handle<JS::Value> receiver,
                        JS::Handle<jsid> id,
                        JS::MutableHandle<JS::Value> vp) const
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
                        JS::Handle<jsid> id,
                        JS::Handle<JS::Value> v,
                        JS::Handle<JS::Value> receiver,
                        JS::ObjectOpResult &result) const
{
  if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
    // Reject the set.  It's up to the caller to decide whether to throw a
    // TypeError.  If the caller is strict mode JS code, it'll throw.
    return result.failReadOnly();
  }

  return js::Wrapper::set(cx, proxy, id, v, receiver, result);
}

bool
nsOuterWindowProxy::getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                                 JS::AutoIdVector &props) const
{
  // BaseProxyHandler::keys seems to do what we want here: call
  // ownPropertyKeys and then filter out the non-enumerable properties.
  return js::BaseProxyHandler::getOwnEnumerablePropertyKeys(cx, proxy, props);
}

bool
nsOuterWindowProxy::enumerate(JSContext *cx, JS::Handle<JSObject*> proxy,
                              JS::MutableHandle<JSObject*> objp) const
{
  // BaseProxyHandler::enumerate seems to do what we want here: fall
  // back on the property names returned from js::GetPropertyKeys()
  return js::BaseProxyHandler::enumerate(cx, proxy, objp);
}

bool
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id,
                                      JS::MutableHandle<JS::Value> vp,
                                      bool& found) const
{
  nsCOMPtr<nsPIDOMWindowOuter> frame = GetSubframeWindow(cx, proxy, id);
  if (!frame) {
    found = false;
    return true;
  }

  found = true;
  // Just return the window's global
  nsGlobalWindow* global = nsGlobalWindow::Cast(frame);
  frame->EnsureInnerWindow();
  JSObject* obj = global->FastGetGlobalJSObject();
  // This null check fixes a hard-to-reproduce crash that occurs when we
  // get here when we're mid-call to nsDocShell::Destroy. See bug 640904
  // comment 105.
  if (MOZ_UNLIKELY(!obj)) {
    return xpc::Throw(cx, NS_ERROR_FAILURE);
  }
  JS::ExposeObjectToActiveJS(obj);
  vp.setObject(*obj);
  return JS_WrapValue(cx, vp);
}

already_AddRefed<nsPIDOMWindowOuter>
nsOuterWindowProxy::GetSubframeWindow(JSContext *cx,
                                      JS::Handle<JSObject*> proxy,
                                      JS::Handle<jsid> id) const
{
  uint32_t index = GetArrayIndexFromId(cx, id);
  if (!IsArrayIndex(index)) {
    return nullptr;
  }

  nsGlobalWindow* win = GetOuterWindow(proxy);
  MOZ_ASSERT(win->IsOuterWindow());
  return win->IndexedGetterOuter(index);
}

bool
nsOuterWindowProxy::AppendIndexedPropertyNames(JSContext *cx, JSObject *proxy,
                                               JS::AutoIdVector &props) const
{
  uint32_t length = GetOuterWindow(proxy)->Length();
  MOZ_ASSERT(int32_t(length) >= 0);
  if (!props.reserve(props.length() + length)) {
    return false;
  }
  for (int32_t i = 0; i < int32_t(length); ++i) {
    if (!props.append(INT_TO_JSID(i))) {
      return false;
    }
  }

  return true;
}

bool
nsOuterWindowProxy::watch(JSContext *cx, JS::Handle<JSObject*> proxy,
                          JS::Handle<jsid> id, JS::Handle<JSObject*> callable) const
{
  return js::WatchGuts(cx, proxy, id, callable);
}

bool
nsOuterWindowProxy::unwatch(JSContext *cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id) const
{
  return js::UnwatchGuts(cx, proxy, id);
}

void
nsOuterWindowProxy::ObjectMoved(JSObject *obj, const JSObject *old)
{
  nsGlobalWindow* outerWindow = GetOuterWindow(obj);
  if (outerWindow) {
    outerWindow->UpdateWrapper(obj, old);
  }
}

const nsOuterWindowProxy
nsOuterWindowProxy::singleton;

class nsChromeOuterWindowProxy : public nsOuterWindowProxy
{
public:
  constexpr nsChromeOuterWindowProxy() : nsOuterWindowProxy() { }

  virtual const char *className(JSContext *cx, JS::Handle<JSObject*> wrapper) const override;

  static const nsChromeOuterWindowProxy singleton;
};

const char *
nsChromeOuterWindowProxy::className(JSContext *cx,
                                    JS::Handle<JSObject*> proxy) const
{
    MOZ_ASSERT(js::IsProxy(proxy));

    return "ChromeWindow";
}

const nsChromeOuterWindowProxy
nsChromeOuterWindowProxy::singleton;

static JSObject*
NewOuterWindowProxy(JSContext *cx, JS::Handle<JSObject*> global, bool isChrome)
{
  JSAutoCompartment ac(cx, global);
  MOZ_ASSERT(js::GetGlobalForObjectCrossCompartment(global) == global);

  js::WrapperOptions options;
  options.setClass(&OuterWindowProxyClass);
  options.setSingleton(true);
  JSObject *obj = js::Wrapper::New(cx, global,
                                   isChrome ? &nsChromeOuterWindowProxy::singleton
                                            : &nsOuterWindowProxy::singleton,
                                   options);
  MOZ_ASSERT_IF(obj, js::IsWindowProxy(obj));
  return obj;
}

//*****************************************************************************
//***    nsGlobalWindow: Object Management
//*****************************************************************************

nsGlobalWindow::nsGlobalWindow(nsGlobalWindow *aOuterWindow)
  : nsPIDOMWindow<nsISupports>(aOuterWindow ? aOuterWindow->AsOuter() : nullptr),
    mIdleFuzzFactor(0),
    mIdleCallbackIndex(-1),
    mCurrentlyIdle(false),
    mAddActiveEventFuzzTime(true),
    mIsFrozen(false),
    mFullScreen(false),
    mFullscreenMode(false),
    mIsClosed(false),
    mInClose(false),
    mHavePendingClose(false),
    mHadOriginalOpener(false),
    mOriginalOpenerWasSecureContext(false),
    mIsPopupSpam(false),
    mBlockScriptedClosingFlag(false),
    mWasOffline(false),
    mHasHadSlowScript(false),
    mNotifyIdleObserversIdleOnThaw(false),
    mNotifyIdleObserversActiveOnThaw(false),
    mCreatingInnerWindow(false),
    mIsChrome(false),
    mCleanMessageManager(false),
    mNeedsFocus(true),
    mHasFocus(false),
    mShowFocusRingForContent(false),
    mFocusByKeyOccurred(false),
    mHasGamepad(false),
    mHasVREvents(false),
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
    mAreDialogsEnabled(true),
    mCanSkipCCGeneration(0)
{
  AssertIsOnMainThread();

  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  if (aOuterWindow) {
    // |this| is an inner window, add this inner window to the outer
    // window list of inners.
    PR_INSERT_AFTER(this, aOuterWindow);

    mObserver = new nsGlobalWindowObserver(this);
    if (mObserver) {
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

      Preferences::AddStrongObserver(mObserver, "intl.accept_languages");
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window, so freeze this
    // outer window here.
    Freeze();
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

  if (gDOMLeakPRLog)
    MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
           ("DOMWINDOW %p created outer=%p", this, aOuterWindow));

  NS_ASSERTION(sWindowsById, "Windows hash table must be created!");
  NS_ASSERTION(!sWindowsById->Get(mWindowID),
               "This window shouldn't be in the hash table yet!");
  // We seem to see crashes in release builds because of null |sWindowsById|.
  if (sWindowsById) {
    sWindowsById->Put(mWindowID, this);
  }
}

#ifdef DEBUG

/* static */
void
nsGlobalWindow::AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

#endif // DEBUG

/* static */
void
nsGlobalWindow::Init()
{
  AssertIsOnMainThread();

  NS_ASSERTION(gDOMLeakPRLog, "gDOMLeakPRLog should have been initialized!");

  sWindowsById = new WindowByIdTable();
}

nsGlobalWindow::~nsGlobalWindow()
{
  AssertIsOnMainThread();

  DisconnectEventTargetObjects();

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

    nsGlobalWindow* outer = nsGlobalWindow::Cast(mOuterWindow);
    printf_stderr("--DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p] [url = %s]\n",
                  gRefCnt,
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  static_cast<void*>(ToCanonicalSupports(outer)),
                  url.get());
  }
#endif

  if (gDOMLeakPRLog)
    MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
           ("DOMWINDOW %p destroyed", this));

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
  MOZ_ASSERT(IsInnerWindow());
  mEventTargetObjects.PutEntry(aObject);
}

void
nsGlobalWindow::RemoveEventTargetObject(DOMEventTargetHelper* aObject)
{
  MOZ_ASSERT(IsInnerWindow());
  mEventTargetObjects.RemoveEntry(aObject);
}

void
nsGlobalWindow::DisconnectEventTargetObjects()
{
  for (auto iter = mEventTargetObjects.ConstIter(); !iter.Done();
       iter.Next()) {
    RefPtr<DOMEventTargetHelper> target = iter.Get()->GetKey();
    target->DisconnectFromOwner();
  }
  mEventTargetObjects.Clear();
}

// static
void
nsGlobalWindow::ShutDown()
{
  AssertIsOnMainThread();

  if (gDumpFile && gDumpFile != stdout) {
    fclose(gDumpFile);
  }
  gDumpFile = nullptr;

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

  StartDying();

  DisconnectEventTargetObjects();

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, "dom-storage2-changed");
    }

#ifdef MOZ_B2G
    DisableNetworkEvent(eNetworkUpload);
    DisableNetworkEvent(eNetworkDownload);
#endif // MOZ_B2G

    if (mIdleService) {
      mIdleService->RemoveIdleObserver(mObserver, MIN_IDLE_NOTIFICATION_TIME_S);
    }

    Preferences::RemoveObserver(mObserver, "intl.accept_languages");

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
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
  mCustomElements = nullptr;
  mFrames = nullptr;
  mWindowUtils = nullptr;
  mApplicationCache = nullptr;
  mIndexedDB = nullptr;

  mConsole = nullptr;

  mExternal = nullptr;

  mMozSelfSupport = nullptr;

  mPerformance = nullptr;

#ifdef MOZ_WEBSPEECH
  mSpeechSynthesis = nullptr;
#endif

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  mOrientationChangeObserver = nullptr;
#endif

  ClearControllers();

  mOpener = nullptr;             // Forces Release
  if (mContext) {
    mContext = nullptr;            // Forces Release
  }
  mChromeEventHandler = nullptr; // Forces Release
  mParentTarget = nullptr;

  if (IsOuterWindow()) {
    nsGlobalWindow* inner = GetCurrentInnerWindowInternal();
    if (inner) {
      inner->CleanUp();
    }
  }

  if (IsInnerWindow()) {
    DisableGamepadUpdates();
    mHasGamepad = false;
    DisableVRUpdates();
    mHasVREvents = false;
#ifdef MOZ_B2G
    DisableTimeChangeNotifications();
#endif
  } else {
    MOZ_ASSERT(!mHasGamepad);
    MOZ_ASSERT(!mHasVREvents);
  }

  if (mCleanMessageManager) {
    MOZ_ASSERT(mIsChrome, "only chrome should have msg manager cleaned");
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

  mServiceWorkerRegistrationTable.Clear();
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
  if (auto* reporter = nsWindowMemoryReporter::Get()) {
    reporter->ObserveDOMWindowDetached(this);
  }

  mInnerObjectsFreed = true;

  // Kill all of the workers for this window.
  mozilla::dom::workers::CancelWorkersForWindow(AsInner());

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
  mCustomElements = nullptr;

  if (mNavigator) {
    mNavigator->OnNavigation();
    mNavigator->Invalidate();
    mNavigator = nullptr;
  }

  if (mScreen) {
    mScreen = nullptr;
  }

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  mOrientationChangeObserver = nullptr;
#endif

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

  UnlinkHostObjectURIs();

  NotifyWindowIDDestroyed("inner-window-destroyed");

  CleanupCachedXBLHandlers(this);

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

#ifdef MOZ_GAMEPAD
  DisableGamepadUpdates();
  mHasGamepad = false;
  mGamepads.Clear();
#endif
  DisableVRUpdates();
  mHasVREvents = false;
  mVRDisplays.Clear();
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindow)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
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
  if (aIID.Equals(NS_GET_IID(nsPIDOMWindowInner))) {
    foundInterface = AsInner();
  } else
  if (aIID.Equals(NS_GET_IID(mozIDOMWindow)) && IsInnerWindow()) {
    foundInterface = AsInner();
  } else
  if (aIID.Equals(NS_GET_IID(nsPIDOMWindowOuter))) {
    foundInterface = AsOuter();
  } else
  if (aIID.Equals(NS_GET_IID(mozIDOMWindowProxy)) && IsOuterWindow()) {
    foundInterface = AsOuter();
  } else
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGlobalWindow)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGlobalWindow)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGlobalWindow)
  if (tmp->IsBlackForCC(false)) {
    if (nsCCUncollectableMarker::InGeneration(tmp->mCanSkipCCGeneration)) {
      return true;
    }
    tmp->mCanSkipCCGeneration = nsCCUncollectableMarker::sGeneration;
    if (tmp->mCachedXBLPrototypeHandlers) {
      for (auto iter = tmp->mCachedXBLPrototypeHandlers->Iter();
           !iter.Done();
           iter.Next()) {
        JS::ExposeObjectToActiveJS(iter.Data());
      }
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
    nsAutoCString uri;
    if (tmp->mDoc && tmp->mDoc->GetDocumentURI()) {
      tmp->mDoc->GetDocumentURI()->GetSpec(uri);
    }
    SprintfLiteral(name, "nsGlobalWindow # %" PRIu64 " %s %s", tmp->mWindowID,
                   tmp->IsInnerWindow() ? "inner" : "outer", uri.get());
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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerRegistrationTable)

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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHistory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCustomElements)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSuspendedDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWakeLock)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingStorageEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleObservers)

#ifdef MOZ_GAMEPAD
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepads)
#endif

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRDisplays)

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mU2F)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExternal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMozSelfSupport)

  tmp->TraverseHostObjectURIs(cb);

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

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorkerRegistrationTable)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechSynthesis)
#endif

  if (tmp->mOuterWindow) {
    nsGlobalWindow::Cast(tmp->mOuterWindow)->MaybeClearInnerWindow(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOuterWindow)
  }

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHistory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCustomElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessionStorage)
  if (tmp->mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(tmp->mApplicationCache.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplicationCache)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSuspendedDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWakeLock)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingStorageEvents)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

#ifdef MOZ_GAMEPAD
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGamepads)
#endif

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVRDisplays)

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mU2F)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExternal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMozSelfSupport)

  tmp->UnlinkHostObjectURIs();

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

#ifdef DEBUG
void
nsGlobalWindow::RiskyUnlink()
{
  NS_CYCLE_COLLECTION_INNERNAME.Unlink(this);
}
#endif

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindow)
  if (tmp->mCachedXBLPrototypeHandlers) {
    for (auto iter = tmp->mCachedXBLPrototypeHandlers->Iter();
         !iter.Done();
         iter.Next()) {
      JS::Heap<JSObject*>& data = iter.Data();
      aCallbacks.Trace(&data, "Cached XBL prototype handler", aClosure);
    }
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
        f->MarkForCC();
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
  if (!outer) {
    return nullptr;
  }
  return outer->mContext;
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

bool
nsGlobalWindow::WouldReuseInnerWindow(nsIDocument* aNewDocument)
{
  MOZ_ASSERT(IsOuterWindow());

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
  MOZ_ASSERT(IsOuterWindow());

  // First, grab the subject principal.
  nsCOMPtr<nsIPrincipal> newWindowPrincipal = nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();

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

class WindowStateHolder final : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  explicit WindowStateHolder(nsGlobalWindow *aWindow);

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }

  void DidRestoreWindow()
  {
    mInnerWindow = nullptr;
    mInnerWindowReflector = nullptr;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindow *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  JS::PersistentRooted<JSObject*> mInnerWindowReflector;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow* aWindow)
  : mInnerWindow(aWindow),
    mInnerWindowReflector(RootingCx(), aWindow->GetWrapper())
{
  NS_PRECONDITION(aWindow, "null window");
  NS_PRECONDITION(aWindow->IsInnerWindow(), "Saving an outer window");

  aWindow->SuspendTimeouts();

  // When a global goes into the bfcache, we disable script.
  xpc::Scriptability::Get(mInnerWindowReflector).SetDocShellAllowsScript(false);
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

NS_IMPL_ISUPPORTS(WindowStateHolder, WindowStateHolder)

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

static bool
EnablePrivilege(JSContext* cx, unsigned argc, JS::Value* vp)
{
  Telemetry::Accumulate(Telemetry::ENABLE_PRIVILEGE_EVER_CALLED, true);
  return xpc::EnableUniversalXPConnect(cx);
}

static const JSFunctionSpec EnablePrivilegeSpec[] = {
  JS_FS("enablePrivilege", EnablePrivilege, 1, 0),
  JS_FS_END
};

static bool
InitializeLegacyNetscapeObject(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  JSAutoCompartment ac(aCx, aGlobal);

  // Note: MathJax depends on window.netscape being exposed. See bug 791526.
  JS::Rooted<JSObject*> obj(aCx);
  obj = JS_DefineObject(aCx, aGlobal, "netscape", nullptr);
  NS_ENSURE_TRUE(obj, false);

  obj = JS_DefineObject(aCx, obj, "security", nullptr);
  NS_ENSURE_TRUE(obj, false);

  // We hide enablePrivilege behind a pref because it has been altered in a
  // way that makes it fundamentally insecure to use in production. Mozilla
  // uses this pref during automated testing to support legacy test code that
  // uses enablePrivilege. If you're not doing test automation, you _must_ not
  // flip this pref, or you will be exposing all your users to security
  // vulnerabilities.
  if (!xpc::IsInAutomation()) {
    return true;
  }

  /* Define PrivilegeManager object with the necessary "static" methods. */
  obj = JS_DefineObject(aCx, obj, "PrivilegeManager", nullptr);
  NS_ENSURE_TRUE(obj, false);

  return JS_DefineFunctions(aCx, obj, EnablePrivilegeSpec);
}

bool
nsGlobalWindow::ComputeIsSecureContext(nsIDocument* aDocument)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIPrincipal> principal = aDocument->NodePrincipal();
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return true;
  }

  // Implement https://w3c.github.io/webappsec-secure-contexts/#settings-object

  bool hadNonSecureContextCreator = false;

  nsPIDOMWindowOuter* parentOuterWin = GetScriptableParent();
  MOZ_ASSERT(parentOuterWin, "How can we get here? No docShell somehow?");
  if (nsGlobalWindow::Cast(parentOuterWin) != this) {
    // There may be a small chance that parentOuterWin has navigated in
    // the time that it took us to start loading this sub-document.  If that
    // were the case then parentOuterWin->GetCurrentInnerWindow() wouldn't
    // return the window for the document that is embedding us.  For this
    // reason we only use the GetScriptableParent call above to check that we
    // have a same-type parent, but actually get the inner window via the
    // document that we know is embedding us.
    nsIDocument* creatorDoc = aDocument->GetParentDocument();
    if (!creatorDoc) {
      return false; // we must be tearing down
    }
    nsGlobalWindow* parentWin =
      nsGlobalWindow::Cast(creatorDoc->GetInnerWindow());
    if (!parentWin) {
      return false; // we must be tearing down
    }
    MOZ_ASSERT(parentWin ==
               nsGlobalWindow::Cast(parentOuterWin->GetCurrentInnerWindow()),
               "Creator window mismatch while setting Secure Context state");
    hadNonSecureContextCreator = !parentWin->IsSecureContext();
  } else if (mHadOriginalOpener) {
    hadNonSecureContextCreator = !mOriginalOpenerWasSecureContext;
  }

  if (hadNonSecureContextCreator) {
    return false;
  }

  if (nsContentUtils::HttpsStateIsModern(aDocument)) {
    return true;
  }

  if (principal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIURI> uri = aDocument->GetOriginalURI();
    // IsOriginPotentiallyTrustworthy doesn't care about origin attributes so
    // it doesn't actually matter what we use here, but reusing the document
    // principal's attributes is convenient.
    const PrincipalOriginAttributes& attrs =
      BasePrincipal::Cast(principal)->OriginAttributesRef();
    // CreateCodebasePrincipal correctly gets a useful principal for blob: and
    // other URI_INHERITS_SECURITY_CONTEXT URIs.
    principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
    if (NS_WARN_IF(!principal)) {
      return false;
    }
  }

  nsCOMPtr<nsIContentSecurityManager> csm =
    do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  NS_WARN_IF(!csm);
  if (csm) {
    bool isTrustworthyOrigin = false;
    csm->IsOriginPotentiallyTrustworthy(principal, &isTrustworthyOrigin);
    if (isTrustworthyOrigin) {
      return true;
    }
  }

  return false;
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
                           JS::MutableHandle<JSObject*> aGlobal,
                           bool aIsSecureContext)
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
    top = aNewInner->GetTopInternal();
  }

  JS::CompartmentOptions options;

  // Sometimes add-ons load their own XUL windows, either as separate top-level
  // windows or inside a browser element. In such cases we want to tag the
  // window's compartment with the add-on ID. See bug 1092156.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    options.creationOptions().setAddonId(MapURIToAddonID(aURI));
  }

  if (top && top->GetGlobalJSObject()) {
    options.creationOptions().setSameZoneAs(top->GetGlobalJSObject());
  }

  options.creationOptions().setSecureContext(aIsSecureContext);

  xpc::InitGlobalObjectOptions(options, aPrincipal);

  // Determine if we need the Components object.
  bool needComponents = nsContentUtils::IsSystemPrincipal(aPrincipal) ||
                        TreatAsRemoteXUL(aPrincipal);
  uint32_t flags = needComponents ? 0 : nsIXPConnect::OMIT_COMPONENTS_OBJECT;
  flags |= nsIXPConnect::DONT_FIRE_ONNEWGLOBALHOOK;

  if (!WindowBinding::Wrap(aCx, aNewInner, aNewInner, options,
                           nsJSPrincipals::get(aPrincipal), false, aGlobal) ||
      !xpc::InitGlobalObject(aCx, aGlobal, flags)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aNewInner->GetWrapperPreserveColor() == aGlobal);

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  xpc::SetLocationForGlobal(aGlobal, aURI);

  if (!InitializeLegacyNetscapeObject(aCx, aGlobal)) {
    return NS_ERROR_FAILURE;
  }

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
    if (mOuterWindow->GetCurrentInnerWindow() != AsInner()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return GetOuterWindowInternal()->SetNewDocument(aDocument, aState,
                                                    aForceReuseInnerWindow);
  }

  NS_PRECONDITION(IsOuterWindow(), "Must only be called on outer windows");

  // Bail out early if we're in process of closing down the window.
  NS_ENSURE_STATE(!mCleanedUp);

  if (IsFrozen()) {
    // This outer is now getting its first inner, thaw the outer now
    // that it's ready and is getting an inner window.

    Thaw();
  }

  NS_ASSERTION(!AsOuter()->GetCurrentInnerWindow() ||
               AsOuter()->GetCurrentInnerWindow()->GetExtantDoc() == mDoc,
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

  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext *cx = jsapi.cx();

  // Check if we're anywhere near the stack limit before we reach the
  // transplanting code, since it has no good way to handle errors. This uses
  // the untrusted script limit, which is not strictly necessary since no
  // actual script should run.
  bool overrecursed = false;
  JS_CHECK_RECURSION_CONSERVATIVE_DONT_REPORT(cx, overrecursed = true);
  if (overrecursed) {
    NS_WARNING("Overrecursion in SetNewDocument");
    return NS_ERROR_FAILURE;
  }

  if (!mDoc) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsPIDOMWindowOuter* privateRoot = nsGlobalWindow::GetPrivateRoot();

    if (privateRoot == AsOuter()) {
      nsXBLService::AttachGlobalKeyHandler(mChromeEventHandler);
    }
  }

  /* No mDocShell means we're already been partially closed down.  When that
     happens, setting status isn't a big requirement, so don't. (Doesn't happen
     under normal circumstances, but bug 49615 describes a case.) */

  nsContentUtils::AddScriptRunner(
    NewRunnableMethod(this, &nsGlobalWindow::ClearStatus));

  // Sometimes, WouldReuseInnerWindow() returns true even if there's no inner
  // window (see bug 776497). Be safe.
  bool reUseInnerWindow = (aForceReuseInnerWindow || wouldReuseInnerWindow) &&
                          GetCurrentInnerWindowInternal();

  nsresult rv = NS_OK;

  // We set mDoc even though this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.
  mDoc = aDocument;

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

  RefPtr<nsGlobalWindow> newInnerWindow;
  bool createdInnerWindow = false;

  bool thisChrome = IsChromeWindow();

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
    MOZ_ASSERT_IF(aDocument == oldDoc,
                  xpc::GetCompartmentPrincipal(compartment) ==
                  aDocument->NodePrincipal());
    if (aDocument != oldDoc) {
      JS_SetCompartmentPrincipals(compartment,
                                  nsJSPrincipals::get(aDocument->NodePrincipal()));
      // Make sure we clear out the old content XBL scope, so the new one will
      // get created with a principal that subsumes our new principal.
      xpc::ClearContentXBLScope(newInnerGlobal);
    }
  } else {
    if (aState) {
      newInnerWindow = wsh->GetInnerWindow();
      newInnerGlobal = newInnerWindow->GetWrapperPreserveColor();
    } else {
      if (thisChrome) {
        newInnerWindow = nsGlobalChromeWindow::Create(this);
      } else if (mIsModalContentWindow) {
        newInnerWindow = nsGlobalModalWindow::Create(this);
      } else {
        newInnerWindow = nsGlobalWindow::Create(this);
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
                                      &newInnerGlobal,
                                      ComputeIsSecureContext(aDocument));
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
          newInnerWindow->mNavigator->SetWindow(newInnerWindow->AsInner());
        }

        // Make a copy of the old window's performance object on document.open.
        // Note that we have to force eager creation of it here, because we need
        // to grab the current document channel and whatnot before that changes.
        currentInner->AsInner()->CreatePerformanceObjectIfNeeded();
        if (currentInner->mPerformance) {
          newInnerWindow->mPerformance =
            Performance::CreateForMainThread(newInnerWindow->AsInner(),
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

    mInnerWindow = newInnerWindow->AsInner();

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
      js::SetProxyExtra(outerObject, 0, js::PrivateValue(nullptr));

      outerObject = xpc::TransplantObject(cx, obj, outerObject);
      if (!outerObject) {
        NS_ERROR("unable to transplant wrappers, probably OOM");
        return NS_ERROR_FAILURE;
      }

      js::SetProxyExtra(outerObject, 0, js::PrivateValue(ToSupports(this)));

      SetWrapper(outerObject);

      MOZ_ASSERT(js::GetGlobalForObjectCrossCompartment(outerObject) == newInnerGlobal);

      // Inform the nsJSContext, which is the canonical holder of the outer.
      mContext->SetWindowProxy(outerObject);
    }

    // Enter the new global's compartment.
    JSAutoCompartment ac(cx, GetWrapperPreserveColor());

    {
      JS::Rooted<JSObject*> outer(cx, GetWrapperPreserveColor());
      js::SetWindowProxy(cx, newInnerGlobal, outer);
    }

    // Set scriptability based on the state of the docshell.
    bool allow = GetDocShell()->GetCanExecuteScripts();
    xpc::Scriptability::Get(GetWrapperPreserveColor()).SetDocShellAllowsScript(allow);

    if (!aState) {
      // Get the "window" property once so it will be cached on our inner.  We
      // have to do this here, not in binding code, because this has to happen
      // after we've created the outer window proxy and stashed it in the outer
      // nsGlobalWindow, so GetWrapperPreserveColor() on that outer
      // nsGlobalWindow doesn't return null and nsGlobalWindow::OuterObject
      // works correctly.
      JS::Rooted<JS::Value> unused(cx);
      if (!JS_GetProperty(cx, newInnerGlobal, "window", &unused)) {
        NS_ERROR("can't create the 'window' property");
        return NS_ERROR_FAILURE;
      }

      // And same thing for the "self" property.
      if (!JS_GetProperty(cx, newInnerGlobal, "self", &unused)) {
        NS_ERROR("can't create the 'self' property");
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

    nsCOMPtr<Element> frame = AsOuter()->GetFrameElementInternal();
    if (frame) {
      nsPIDOMWindowOuter* parentWindow = frame->OwnerDoc()->GetWindow();
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

        // The storage objects contain the URL of the window. We have to
        // recreate them when the innerWindow is reused.
        newInnerWindow->mLocalStorage = nullptr;
        newInnerWindow->mSessionStorage = nullptr;

        newInnerWindow->ClearDocumentDependentSlots(cx);
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

  nsJSContext::PokeGC(JS::gcreason::SET_NEW_DOCUMENT);
  mContext->DidInitializeContext();

  // We wait to fire the debugger hook until the window is all set up and hooked
  // up with the outer. See bug 969156.
  if (createdInnerWindow) {
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod(newInnerWindow,
                        &nsGlobalWindow::FireOnNewGlobalObject));
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
        NewRunnableMethod(this, &nsGlobalWindow::DispatchDOMWindowCreated));
    }
  }

  PreloadLocalStorage();

  return NS_OK;
}

void
nsGlobalWindow::PreloadLocalStorage()
{
  MOZ_ASSERT(IsOuterWindow());

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
  MOZ_ASSERT(IsOuterWindow());

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
  SetStatusOuter(EmptyString());
}

void
nsGlobalWindow::InnerSetNewDocument(JSContext* aCx, nsIDocument* aDocument)
{
  NS_PRECONDITION(IsInnerWindow(), "Must only be called on inner windows");
  MOZ_ASSERT(aDocument);

  if (gDOMLeakPRLog && MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    nsAutoCString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOMWINDOW %p SetNewDocument %s", this, spec.get());
  }

  mDoc = aDocument;
  ClearDocumentDependentSlots(aCx);
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
    nsCOMPtr<nsPIDOMWindowOuter> parentWindow = GetParent();
    if (parentWindow.get() != AsOuter()) {
      mChromeEventHandler = parentWindow->GetChromeEventHandler();
    }
    else {
      mChromeEventHandler = NS_NewWindowRoot(AsOuter());
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
  for (RefPtr<nsGlobalWindow> inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
       inner != this;
       inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
    MOZ_ASSERT(!inner->mOuterWindow || inner->mOuterWindow == AsOuter());
    inner->FreeInnerObjects();
  }

  if (auto* reporter = nsWindowMemoryReporter::Get()) {
    reporter->ObserveDOMWindowDetached(this);
  }

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
    nsJSContext::PokeGC(JS::gcreason::SET_DOC_SHELL);
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
nsGlobalWindow::SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
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
    MOZ_ASSERT(!mHadOriginalOpener,
               "Probably too late to call ComputeIsSecureContext again");
    mHadOriginalOpener = true;
    mOriginalOpenerWasSecureContext =
      nsGlobalWindow::Cast(aOpener->GetCurrentInnerWindow())->IsSecureContext();
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

  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
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

  nsCOMPtr<Element> frameElement = GetOuterWindow()->GetFrameElementInternal();
  nsCOMPtr<EventTarget> eventTarget =
    TryGetTabChildGlobalAsEventTarget(frameElement);

  if (!eventTarget) {
    nsGlobalWindow* topWin = GetScriptableTopInternal();
    if (topWin) {
      frameElement = topWin->AsOuter()->GetFrameElementInternal();
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

nsresult
nsGlobalWindow::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(), "PreHandleEvent is used on outer window!?");
  EventMessage msg = aVisitor.mEvent->mMessage;

  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  if (msg == eResize && aVisitor.mEvent->IsTrusted()) {
    // QIing to window so that we can keep the old behavior also in case
    // a child window is handling resize.
    nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
    if (window) {
      mIsHandlingResizeEvent = true;
    }
  } else if (msg == eMouseDown && aVisitor.mEvent->IsTrusted()) {
    gMouseDown = true;
  } else if ((msg == eMouseUp || msg == eDragEnd) &&
             aVisitor.mEvent->IsTrusted()) {
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
      aVisitor.mEvent->IsTrusted() &&
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

  nsGlobalWindow *topWindow = GetScriptableTopInternal();
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
  MOZ_ASSERT(IsOuterWindow());

  nsGlobalWindow *topWindow = GetScriptableTopInternal();
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

  // Dialogs are also blocked if the document is sandboxed with SANDBOXED_MODALS
  // (or if we have no document, of course).  Which document?  Who knows; the
  // spec is daft.  See <https://github.com/whatwg/html/issues/1206>.  For now
  // just go ahead and check mDoc, since in everything except edge cases in
  // which a frame is allow-same-origin but not allow-scripts and is being poked
  // at by some other window this should be the right thing anyway.
  if (!mDoc || (mDoc->GetSandboxFlags() & SANDBOXED_MODALS)) {
    return false;
  }

  return topWindow->mAreDialogsEnabled;
}

bool
nsGlobalWindow::DialogsAreBeingAbused()
{
  MOZ_ASSERT(IsInnerWindow());
  NS_ASSERTION(GetScriptableTopInternal() &&
               GetScriptableTopInternal()->GetCurrentInnerWindowInternal() == this,
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
  promptSvc->Confirm(AsOuter(), title.get(), label.get(), &disableDialog);
  if (disableDialog) {
    DisableDialogs();
    return false;
  }

  return true;
}

void
nsGlobalWindow::DisableDialogs()
{
  nsGlobalWindow *topWindow = GetScriptableTopInternal();
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
  nsGlobalWindow *topWindow = GetScriptableTopInternal();
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
  switch (aVisitor.mEvent->mMessage) {
    case eResize:
    case eUnload:
    case eLoad:
      break;
    default:
      return NS_OK;
  }

  /* mChromeEventHandler and mContext go dangling in the middle of this
   function under some circumstances (events that destroy the window)
   without this addref. */
  nsCOMPtr<nsIDOMEventTarget> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());

  if (aVisitor.mEvent->mMessage == eResize) {
    mIsHandlingResizeEvent = false;
  } else if (aVisitor.mEvent->mMessage == eUnload &&
             aVisitor.mEvent->IsTrusted()) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDoc) {
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->mMessage == eLoad &&
             aVisitor.mEvent->IsTrusted()) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = true;

    nsCOMPtr<Element> element = GetOuterWindow()->GetFrameElementInternal();
    nsIDocShell* docShell = GetDocShell();
    if (element && GetParentInternal() &&
        docShell && docShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      WidgetEvent event(aVisitor.mEvent->IsTrusted(), eLoad);
      event.mFlags.mBubbles = false;
      event.mFlags.mCancelable = false;

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
  MOZ_ASSERT(IsInnerWindow());
  MOZ_ASSERT(!mIsModalContentWindow); // Handled separately.

  nsIScriptContext *ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx, NS_ERROR_NOT_INITIALIZED);

  JS::Rooted<JSObject*> obj(RootingCx(), GetWrapperPreserveColor());
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

template <class T>
nsIURI*
nsPIDOMWindow<T>::GetDocumentURI() const
{
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}

template <class T>
nsIURI*
nsPIDOMWindow<T>::GetDocBaseURI() const
{
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

template <class T>
void
nsPIDOMWindow<T>::MaybeCreateDoc()
{
  MOZ_ASSERT(!mDoc);
  if (nsIDocShell* docShell = GetDocShell()) {
    // Note that |document| here is the same thing as our mDoc, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<nsIDocument> document = docShell->GetDocument();
  }
}

void
nsPIDOMWindowOuter::SetInitialKeyboardIndicators(
  UIStateChangeType aShowAccelerators, UIStateChangeType aShowFocusRings)
{
  MOZ_ASSERT(IsOuterWindow());
  MOZ_ASSERT(!GetCurrentInnerWindow());

  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return;
  }

  MOZ_ASSERT(piWin == AsOuter());

  // only change the flags that have been modified
  nsCOMPtr<nsPIWindowRoot> windowRoot = do_QueryInterface(mChromeEventHandler);
  if (!windowRoot) {
    return;
  }

  if (aShowAccelerators != UIStateChangeType_NoChange) {
    windowRoot->SetShowAccelerators(aShowAccelerators == UIStateChangeType_Set);
  }
  if (aShowFocusRings != UIStateChangeType_NoChange) {
    windowRoot->SetShowFocusRings(aShowFocusRings == UIStateChangeType_Set);
  }

  nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(GetOuterWindow(),
                                                        aShowAccelerators,
                                                        aShowFocusRings);
}

Element*
nsPIDOMWindowOuter::GetFrameElementInternal() const
{
  MOZ_ASSERT(IsOuterWindow());
  return mFrameElement;
}

void
nsPIDOMWindowOuter::SetFrameElementInternal(Element* aFrameElement)
{
  MOZ_ASSERT(IsOuterWindow());
  mFrameElement = aFrameElement;
}

bool
nsPIDOMWindowInner::AddAudioContext(AudioContext* aAudioContext)
{
  MOZ_ASSERT(IsInnerWindow());

  mAudioContexts.AppendElement(aAudioContext);

  // Return true if the context should be muted and false if not.
  nsIDocShell* docShell = GetDocShell();
  return docShell && !docShell->GetAllowMedia() && !aAudioContext->IsOffline();
}

void
nsPIDOMWindowInner::RemoveAudioContext(AudioContext* aAudioContext)
{
  MOZ_ASSERT(IsInnerWindow());

  mAudioContexts.RemoveElement(aAudioContext);
}

void
nsPIDOMWindowInner::MuteAudioContexts()
{
  MOZ_ASSERT(IsInnerWindow());

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Mute();
    }
  }
}

void
nsPIDOMWindowInner::UnmuteAudioContexts()
{
  MOZ_ASSERT(IsInnerWindow());

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Unmute();
    }
  }
}

nsGlobalWindow*
nsGlobalWindow::Window()
{
  return this;
}

nsGlobalWindow*
nsGlobalWindow::Self()
{
  return this;
}

Navigator*
nsGlobalWindow::GetNavigator(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mNavigator) {
    mNavigator = new Navigator(AsInner());
  }

  return mNavigator;
}

nsIDOMNavigator*
nsGlobalWindow::GetNavigator()
{
  FORWARD_TO_INNER(GetNavigator, (), nullptr);

  ErrorResult dummy;
  nsIDOMNavigator* navigator = GetNavigator(dummy);
  dummy.SuppressException();
  return navigator;
}

nsScreen*
nsGlobalWindow::GetScreen(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mScreen) {
    mScreen = nsScreen::Create(AsInner());
    if (!mScreen) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
  }

  return mScreen;
}

nsIDOMScreen*
nsGlobalWindow::GetScreen()
{
  FORWARD_TO_INNER(GetScreen, (), nullptr);

  ErrorResult dummy;
  nsIDOMScreen* screen = GetScreen(dummy);
  dummy.SuppressException();
  return screen;
}

nsHistory*
nsGlobalWindow::GetHistory(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mHistory) {
    mHistory = new nsHistory(AsInner());
  }

  return mHistory;
}

CustomElementsRegistry*
nsGlobalWindow::CustomElements()
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());
  if (!mCustomElements) {
      mCustomElements = CustomElementsRegistry::Create(AsInner());
  }

  return mCustomElements;
}

Performance*
nsPIDOMWindowInner::GetPerformance()
{
  MOZ_ASSERT(IsInnerWindow());
  CreatePerformanceObjectIfNeeded();
  return mPerformance;
}

Performance*
nsGlobalWindow::GetPerformance()
{
  return AsInner()->GetPerformance();
}

void
nsPIDOMWindowInner::CreatePerformanceObjectIfNeeded()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mPerformance || !mDoc) {
    return;
  }
  RefPtr<nsDOMNavigationTiming> timing = mDoc->GetNavigationTiming();
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
    Performance* parentPerformance = nullptr;
    nsCOMPtr<nsPIDOMWindowOuter> parentWindow = GetScriptableParentOrNull();
    if (parentWindow) {
      nsPIDOMWindowInner* parentInnerWindow = nullptr;
      if (parentWindow) {
        parentInnerWindow = parentWindow->GetCurrentInnerWindow();
      }
      if (parentInnerWindow) {
        parentPerformance = parentInnerWindow->GetPerformance();
      }
    }
    mPerformance =
      Performance::CreateForMainThread(this, timing, timedChannel,
                                       parentPerformance);
  }
}

SuspendTypes
nsPIDOMWindowOuter::GetMediaSuspend() const
{
  if (IsInnerWindow()) {
    return mOuterWindow->GetMediaSuspend();
  }

  return mMediaSuspend;
}

void
nsPIDOMWindowOuter::SetMediaSuspend(SuspendTypes aSuspend)
{
  if (IsInnerWindow()) {
    mOuterWindow->SetMediaSuspend(aSuspend);
    return;
  }

  if (!IsDisposableSuspend(aSuspend)) {
    mMediaSuspend = aSuspend;
  }

  RefreshMediaElementsSuspend(aSuspend);
}

bool
nsPIDOMWindowOuter::GetAudioMuted() const
{
  if (IsInnerWindow()) {
    return mOuterWindow->GetAudioMuted();
  }

  return mAudioMuted;
}

void
nsPIDOMWindowOuter::SetAudioMuted(bool aMuted)
{
  if (IsInnerWindow()) {
    mOuterWindow->SetAudioMuted(aMuted);
    return;
  }

  if (mAudioMuted == aMuted) {
    return;
  }

  mAudioMuted = aMuted;
  RefreshMediaElementsVolume();
}

float
nsPIDOMWindowOuter::GetAudioVolume() const
{
  if (IsInnerWindow()) {
    return mOuterWindow->GetAudioVolume();
  }

  return mAudioVolume;
}

nsresult
nsPIDOMWindowOuter::SetAudioVolume(float aVolume)
{
  if (IsInnerWindow()) {
    return mOuterWindow->SetAudioVolume(aVolume);
  }

  if (aVolume < 0.0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (mAudioVolume == aVolume) {
    return NS_OK;
  }

  mAudioVolume = aVolume;
  RefreshMediaElementsVolume();
  return NS_OK;
}

void
nsPIDOMWindowOuter::RefreshMediaElementsVolume()
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->RefreshAgentsVolume(GetOuterWindow());
  }
}

void
nsPIDOMWindowOuter::RefreshMediaElementsSuspend(SuspendTypes aSuspend)
{
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->RefreshAgentsSuspend(GetOuterWindow(), aSuspend);
  }
}

bool
nsPIDOMWindowOuter::IsDisposableSuspend(SuspendTypes aSuspend) const
{
  return (aSuspend == nsISuspendedTypes::SUSPENDED_PAUSE_DISPOSABLE ||
          aSuspend == nsISuspendedTypes::SUSPENDED_STOP_DISPOSABLE);
}

void
nsPIDOMWindowOuter::SetServiceWorkersTestingEnabled(bool aEnabled)
{
  // Devtools should only be setting this on the top level window.  Its
  // ok if devtools clears the flag on clean up of nested windows, though.
  // It will have no affect.
#ifdef DEBUG
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetScriptableTop();
  MOZ_ASSERT_IF(aEnabled, this == topWindow);
#endif
  mServiceWorkersTestingEnabled = aEnabled;
}

bool
nsPIDOMWindowOuter::GetServiceWorkersTestingEnabled()
{
  // Automatically get this setting from the top level window so that nested
  // iframes get the correct devtools setting.
  nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetScriptableTop();
  if (!topWindow) {
    return false;
  }
  return topWindow->mServiceWorkersTestingEnabled;
}

bool
nsPIDOMWindowInner::GetAudioCaptured() const
{
  MOZ_ASSERT(IsInnerWindow());
  return mAudioCaptured;
}

nsresult
nsPIDOMWindowInner::SetAudioCapture(bool aCapture)
{
  MOZ_ASSERT(IsInnerWindow());

  mAudioCaptured = aCapture;

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->SetWindowAudioCaptured(GetOuterWindow(), mWindowID, aCapture);
  }

  return NS_OK;
}

// nsISpeechSynthesisGetter

#ifdef MOZ_WEBSPEECH
SpeechSynthesis*
nsGlobalWindow::GetSpeechSynthesis(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mSpeechSynthesis) {
    mSpeechSynthesis = new SpeechSynthesis(AsInner());
  }

  return mSpeechSynthesis;
}

bool
nsGlobalWindow::HasActiveSpeechSynthesis()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mSpeechSynthesis) {
    return !mSpeechSynthesis->HasEmptyQueue();
  }

  return false;
}

#endif

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetParentOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent;
  if (mDocShell->GetIsMozBrowserOrApp()) {
    parent = AsOuter();
  } else {
    parent = GetParent();
  }

  return parent.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetParent(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetParentOuter, (), aError, nullptr);
}

/**
 * GetScriptableParent is called when script reads window.parent.
 *
 * In contrast to GetRealParent, GetScriptableParent respects <iframe
 * mozbrowser> boundaries, so if |this| is contained by an <iframe
 * mozbrowser>, we will return |this| as its own parent.
 */
nsPIDOMWindowOuter*
nsGlobalWindow::GetScriptableParent()
{
  FORWARD_TO_OUTER(GetScriptableParent, (), nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParentOuter();
  return parent.get();
}

/**
 * Behavies identically to GetScriptableParent extept that it returns null
 * if GetScriptableParent would return this window.
 */
nsPIDOMWindowOuter*
nsGlobalWindow::GetScriptableParentOrNull()
{
  FORWARD_TO_OUTER(GetScriptableParentOrNull, (), nullptr);

  nsPIDOMWindowOuter* parent = GetScriptableParent();
  return (Cast(parent) == this) ? nullptr : parent;
}

/**
 * nsPIDOMWindow::GetParent (when called from C++) is just a wrapper around
 * GetRealParent.
 */
already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetParent()
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserAndAppBoundaries(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsPIDOMWindowOuter> win = parent->GetWindow();
    return win.forget();
  }

  nsCOMPtr<nsPIDOMWindowOuter> win(AsOuter());
  return win.forget();
}

static nsresult
GetTopImpl(nsGlobalWindow* aWin, nsPIDOMWindowOuter** aTop, bool aScriptable)
{
  *aTop = nullptr;

  // Walk up the parent chain.

  nsCOMPtr<nsPIDOMWindowOuter> prevParent = aWin->AsOuter();
  nsCOMPtr<nsPIDOMWindowOuter> parent = aWin->AsOuter();
  do {
    if (!parent) {
      break;
    }

    prevParent = parent;

    nsCOMPtr<nsPIDOMWindowOuter> newParent;
    if (aScriptable) {
      newParent = parent->GetScriptableParent();
    }
    else {
      newParent = parent->GetParent();
    }

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
nsPIDOMWindowOuter*
nsGlobalWindow::GetScriptableTop()
{
  FORWARD_TO_OUTER(GetScriptableTop, (), nullptr);
  nsCOMPtr<nsPIDOMWindowOuter> window;
  GetTopImpl(this, getter_AddRefs(window), /* aScriptable = */ true);
  return window.get();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetTop()
{
  MOZ_ASSERT(IsOuterWindow());
  nsCOMPtr<nsPIDOMWindowOuter> window;
  GetTopImpl(this, getter_AddRefs(window), /* aScriptable = */ false);
  return window.forget();
}

void
nsGlobalWindow::GetContentOuter(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aRetval,
                                ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> content =
    GetContentInternal(aError, !nsContentUtils::IsCallerChrome());
  if (aError.Failed()) {
    return;
  }

  if (content) {
    JS::Rooted<JS::Value> val(aCx);
    aError = nsContentUtils::WrapNative(aCx, content, &val);
    if (aError.Failed()) {
      return;
    }

    aRetval.set(&val.toObject());
    return;
  }

  aRetval.set(nullptr);
  return;
}

void
nsGlobalWindow::GetContent(JSContext* aCx,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetContentOuter, (aCx, aRetval, aError), aError, );
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetContentInternal(ErrorResult& aError, bool aUnprivilegedCaller)
{
  MOZ_ASSERT(IsOuterWindow());

  // First check for a named frame named "content"
  nsCOMPtr<nsPIDOMWindowOuter> domWindow =
    GetChildWindow(NS_LITERAL_STRING("content"));
  if (domWindow) {
    return domWindow.forget();
  }

  // If we're contained in <iframe mozbrowser> or <iframe mozapp>, then
  // GetContent is the same as window.top.
  if (mDocShell && mDocShell->GetIsInMozBrowserOrApp()) {
    return GetTopOuter();
  }

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;
  if (aUnprivilegedCaller) {
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

  if (!primaryContent) {
    return nullptr;
  }

  domWindow = primaryContent->GetWindow();
  return domWindow.forget();
}

MozSelfSupport*
nsGlobalWindow::GetMozSelfSupport(ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  if (mMozSelfSupport) {
    return mMozSelfSupport;
  }

  // We're called from JS and want to use out existing JSContext (and,
  // importantly, its compartment!) here.
  AutoJSContext cx;
  GlobalObject global(cx, FastGetGlobalJSObject());
  mMozSelfSupport = MozSelfSupport::Constructor(global, cx, aError);
  return mMozSelfSupport;
}

nsresult
nsGlobalWindow::GetScriptableContent(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  ErrorResult rv;
  JS::Rooted<JSObject*> content(aCx);
  GetContent(aCx, &content, rv);
  if (!rv.Failed()) {
    aVal.setObjectOrNull(content);
  }

  return rv.StealNSResult();
}

nsresult
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

  prompter.forget(aPrompt);
  return NS_OK;
}

BarProp*
nsGlobalWindow::GetMenubar(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mMenubar) {
    mMenubar = new MenubarProp(this);
  }

  return mMenubar;
}

BarProp*
nsGlobalWindow::GetToolbar(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mToolbar) {
    mToolbar = new ToolbarProp(this);
  }

  return mToolbar;
}

BarProp*
nsGlobalWindow::GetLocationbar(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mLocationbar) {
    mLocationbar = new LocationbarProp(this);
  }
  return mLocationbar;
}

BarProp*
nsGlobalWindow::GetPersonalbar(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarProp(this);
  }
  return mPersonalbar;
}

BarProp*
nsGlobalWindow::GetStatusbar(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mStatusbar) {
    mStatusbar = new StatusbarProp(this);
  }
  return mStatusbar;
}

BarProp*
nsGlobalWindow::GetScrollbars(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mScrollbars) {
    mScrollbars = new ScrollbarsProp(this);
  }

  return mScrollbars;
}

bool
nsGlobalWindow::GetClosedOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // If someone called close(), or if we don't have a docshell, we're closed.
  return mIsClosed || !mDocShell;
}

bool
nsGlobalWindow::GetClosed(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetClosedOuter, (), aError, false);
}

bool
nsGlobalWindow::Closed()
{
  MOZ_ASSERT(IsOuterWindow());

  return GetClosedOuter();
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

already_AddRefed<nsIDOMWindowCollection>
nsGlobalWindow::GetFrames()
{
  FORWARD_TO_OUTER(GetFrames, (), nullptr);

  nsCOMPtr<nsIDOMWindowCollection> frames = GetWindowList();
  return frames.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::IndexedGetterOuter(uint32_t aIndex)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsDOMWindowList* windows = GetWindowList();
  NS_ENSURE_TRUE(windows, nullptr);

  return windows->IndexedGetter(aIndex);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::IndexedGetter(uint32_t aIndex)
{
  FORWARD_TO_OUTER(IndexedGetterOuter, (aIndex), nullptr);
  MOZ_CRASH();
}

bool
nsGlobalWindow::DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                          JS::Handle<jsid> aId,
                          JS::MutableHandle<JS::PropertyDescriptor> aDesc)
{
  MOZ_ASSERT(IsInnerWindow());

  // Note: Keep this in sync with MayResolve.

  // Note: The infallibleInit call in GlobalResolve depends on this check.
  if (!JSID_IS_STRING(aId)) {
    return true;
  }

  bool found;
  if (!WebIDLGlobalNameHash::DefineIfEnabled(aCx, aObj, aId, aDesc, &found)) {
    return false;
  }

  if (found) {
    return true;
  }

  nsresult rv = nsWindowSH::GlobalResolve(this, aCx, aObj, aId, aDesc);
  if (NS_FAILED(rv)) {
    return Throw(aCx, rv);
  }

  return true;
}

/* static */
bool
nsGlobalWindow::MayResolve(jsid aId)
{
  // Note: This function does not fail and may not have any side-effects.
  // Note: Keep this in sync with DoResolve.
  if (!JSID_IS_STRING(aId)) {
    return false;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_COMPONENTS)) {
    return true;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_CONTROLLERS)) {
    // We only resolve .controllers in release builds and on non-chrome windows,
    // but let's not worry about any of that stuff.
    return true;
  }

  if (WebIDLGlobalNameHash::MayResolve(aId)) {
    return true;
  }

  nsScriptNameSpaceManager *nameSpaceManager = PeekNameSpaceManager();
  if (!nameSpaceManager) {
    // Really shouldn't happen.  Fail safe.
    return true;
  }

  nsAutoString name;
  AssignJSFlatString(name, JSID_TO_FLAT_STRING(aId));

  return nameSpaceManager->LookupName(name);
}

void
nsGlobalWindow::GetOwnPropertyNames(JSContext* aCx, nsTArray<nsString>& aNames,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(IsInnerWindow());
  // "Components" is marked as enumerable but only resolved on demand :-/.
  //aNames.AppendElement(NS_LITERAL_STRING("Components"));

  nsScriptNameSpaceManager* nameSpaceManager = GetNameSpaceManager();
  if (nameSpaceManager) {
    JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());

    WebIDLGlobalNameHash::GetNames(aCx, wrapper, aNames);

    for (auto i = nameSpaceManager->GlobalNameIter(); !i.Done(); i.Next()) {
      const GlobalNameMapEntry* entry = i.Get();
      if (nsWindowSH::NameStructEnabled(aCx, this, entry->mKey,
                                        entry->mGlobalName)) {
        aNames.AppendElement(entry->mKey);
      }
    }
  }
}

/* static */ bool
nsGlobalWindow::IsPrivilegedChromeWindow(JSContext* aCx, JSObject* aObj)
{
  // For now, have to deal with XPConnect objects here.
  return xpc::WindowOrNull(aObj)->IsChromeWindow() &&
         nsContentUtils::ObjectPrincipal(aObj) == nsContentUtils::GetSystemPrincipal();
}

/* static */ bool
nsGlobalWindow::IsShowModalDialogEnabled(JSContext*, JSObject*)
{
  static bool sAddedPrefCache = false;
  static bool sIsDisabled;
  static const char sShowModalDialogPref[] = "dom.disable_window_showModalDialog";

  if (!sAddedPrefCache) {
    Preferences::AddBoolVarCache(&sIsDisabled, sShowModalDialogPref, false);
    sAddedPrefCache = true;
  }

  return !sIsDisabled && !XRE_IsContentProcess();
}

nsIDOMOfflineResourceList*
nsGlobalWindow::GetApplicationCache(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mApplicationCache) {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetDocShell()));
    if (!webNav || !mDoc) {
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

    RefPtr<nsDOMOfflineResourceList> applicationCache =
      new nsDOMOfflineResourceList(manifestURI, uri, mDoc->NodePrincipal(),
                                   AsInner());

    applicationCache->Init();

    mApplicationCache = applicationCache;
  }

  return mApplicationCache;
}

already_AddRefed<nsIDOMOfflineResourceList>
nsGlobalWindow::GetApplicationCache()
{
  FORWARD_TO_INNER(GetApplicationCache, (), nullptr);

  ErrorResult dummy;
  nsCOMPtr<nsIDOMOfflineResourceList> applicationCache =
    GetApplicationCache(dummy);
  dummy.SuppressException();
  return applicationCache.forget();
}

Crypto*
nsGlobalWindow::GetCrypto(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mCrypto) {
    mCrypto = new Crypto();
    mCrypto->Init(this);
  }
  return mCrypto;
}

mozilla::dom::U2F*
nsGlobalWindow::GetU2f(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mU2F) {
    RefPtr<U2F> u2f = new U2F();
    u2f->Init(AsInner(), aError);
    if (NS_WARN_IF(aError.Failed())) {
      return nullptr;
    }

    mU2F = u2f;
  }
  return mU2F;
}

nsIControllers*
nsGlobalWindow::GetControllersOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

nsIControllers*
nsGlobalWindow::GetControllers(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetControllersOuter, (aError), aError, nullptr);
}

nsresult
nsGlobalWindow::GetControllers(nsIControllers** aResult)
{
  FORWARD_TO_INNER(GetControllers, (aResult), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  nsCOMPtr<nsIControllers> controllers = GetControllers(rv);
  controllers.forget(aResult);

  return rv.StealNSResult();
}

nsPIDOMWindowOuter*
nsGlobalWindow::GetOpenerWindowOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> opener = do_QueryReferent(mOpener);
  if (!opener) {
    return nullptr;
  }

  nsGlobalWindow* win = nsGlobalWindow::Cast(opener);

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    // Catch the case where we're chrome but the opener is not...
    if (GetPrincipal() == nsContentUtils::GetSystemPrincipal() &&
        win->GetPrincipal() != nsContentUtils::GetSystemPrincipal()) {
      return nullptr;
    }
    return opener;
  }

  // First, ensure that we're not handing back a chrome window to content:
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

nsPIDOMWindowOuter*
nsGlobalWindow::GetOpenerWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOpenerWindowOuter, (), aError, nullptr);
}

void
nsGlobalWindow::GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpenerWindow(aError);
  if (aError.Failed() || !opener) {
    aRetval.setNull();
    return;
  }

  aError = nsContentUtils::WrapNative(aCx, opener, aRetval);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetOpener()
{
  FORWARD_TO_INNER(GetOpener, (), nullptr);

  ErrorResult dummy;
  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpenerWindow(dummy);
  dummy.SuppressException();
  return opener.forget();
}

void
nsGlobalWindow::SetOpener(JSContext* aCx, JS::Handle<JS::Value> aOpener,
                          ErrorResult& aError)
{
  // Check if we were called from a privileged chrome script.  If not, and if
  // aOpener is not null, just define aOpener on our inner window's JS object,
  // wrapped into the current compartment so that for Xrays we define on the
  // Xray expando object, but don't set it on the outer window, so that it'll
  // get reset on navigation.  This is just like replaceable properties, but
  // we're not quite readonly.
  if (!aOpener.isNull() && !nsContentUtils::IsCallerChrome()) {
    RedefineProperty(aCx, "opener", aOpener, aError);
    return;
  }

  if (!aOpener.isObjectOrNull()) {
    // Chrome code trying to set some random value as opener
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsPIDOMWindowInner* win = nullptr;
  if (aOpener.isObject()) {
    JSObject* unwrapped = js::CheckedUnwrap(&aOpener.toObject(),
                                            /* stopAtWindowProxy = */ false);
    if (!unwrapped) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    auto* globalWindow = xpc::WindowOrNull(unwrapped);
    if (!globalWindow) {
      // Wasn't a window
      aError.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    win = globalWindow->AsInner();
  }

  nsPIDOMWindowOuter* outer = nullptr;
  if (win) {
    if (!win->IsCurrentInnerWindow()) {
      aError.Throw(NS_ERROR_FAILURE);
      return;
    }
    outer = win->GetOuterWindow();
  }

  SetOpenerWindow(outer, false);
}

void
nsGlobalWindow::GetStatusOuter(nsAString& aStatus)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  aStatus = mStatus;
}

void
nsGlobalWindow::GetStatus(nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetStatusOuter, (aStatus), aError, );
}

void
nsGlobalWindow::SetStatusOuter(const nsAString& aStatus)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

void
nsGlobalWindow::SetStatus(const nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetStatusOuter, (aStatus), aError, );
}

void
nsGlobalWindow::GetNameOuter(nsAString& aName)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (mDocShell) {
    mDocShell->GetName(aName);
  }
}

void
nsGlobalWindow::GetName(nsAString& aName, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetNameOuter, (aName), aError, );
}

void
nsGlobalWindow::SetNameOuter(const nsAString& aName, mozilla::ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (mDocShell) {
    aError = mDocShell->SetName(aName);
  }
}

void
nsGlobalWindow::SetName(const nsAString& aName, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetNameOuter, (aName, aError), aError, );
}

// Helper functions used by many methods below.
int32_t
nsGlobalWindow::DevToCSSIntPixels(int32_t px)
{
  if (!mDocShell)
    return px; // assume 1:1

  RefPtr<nsPresContext> presContext;
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

  RefPtr<nsPresContext> presContext;
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

  RefPtr<nsPresContext> presContext;
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

  RefPtr<nsPresContext> presContext;
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

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

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
    RefPtr<nsViewManager> viewManager = presShell->GetViewManager();
    if (viewManager) {
      viewManager->FlushDelayedResize(false);
    }

    aSize = CSSIntRect::FromAppUnitsRounded(
      presContext->GetVisibleArea().Size());
  }
  return NS_OK;
}

int32_t
nsGlobalWindow::GetInnerWidthOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.width;
}

int32_t
nsGlobalWindow::GetInnerWidth(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetInnerWidthOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetInnerWidth(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetInnerWidth, aValue,
                            aError);
}

nsresult
nsGlobalWindow::GetInnerWidth(int32_t* aInnerWidth)
{
  FORWARD_TO_INNER(GetInnerWidth, (aInnerWidth), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  *aInnerWidth = GetInnerWidth(rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::SetInnerWidthOuter(int32_t aInnerWidth, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  CheckSecurityWidthAndHeight(&aInnerWidth, nullptr, aCallerIsChrome);

  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    nscoord height = 0;

    RefPtr<nsPresContext> presContext;
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

void
nsGlobalWindow::SetInnerWidth(int32_t aInnerWidth, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerWidthOuter, (aInnerWidth, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetInnerWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetInnerWidth,
                            aValue, "innerWidth", aError);
}

int32_t
nsGlobalWindow::GetInnerHeightOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  CSSIntSize size;
  aError = GetInnerSize(size);
  return size.height;
}

int32_t
nsGlobalWindow::GetInnerHeight(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetInnerHeightOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetInnerHeight(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetInnerHeight, aValue,
                            aError);
}

nsresult
nsGlobalWindow::GetInnerHeight(int32_t* aInnerHeight)
{
  FORWARD_TO_INNER(GetInnerHeight, (aInnerHeight), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  *aInnerHeight = GetInnerHeight(rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::SetInnerHeightOuter(int32_t aInnerHeight, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (presShell && presShell->GetIsViewportOverridden())
  {
    RefPtr<nsPresContext> presContext;
    presContext = presShell->GetPresContext();

    nsRect shellArea = presContext->GetVisibleArea();
    nscoord height = aInnerHeight;
    nscoord width = shellArea.width;
    CheckSecurityWidthAndHeight(nullptr, &height, aCallerIsChrome);
    SetCSSViewportWidthAndHeight(width,
                                 nsPresContext::CSSPixelsToAppUnits(height));
    return;
  }

  int32_t height = 0;
  int32_t width  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&width, &height);
  CheckSecurityWidthAndHeight(nullptr, &aInnerHeight, aCallerIsChrome);
  aError = SetDocShellWidthAndHeight(width, CSSToDevIntPixels(aInnerHeight));
}

void
nsGlobalWindow::SetInnerHeight(int32_t aInnerHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerHeightOuter, (aInnerHeight, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetInnerHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                               ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetInnerHeight,
                            aValue, "innerHeight", aError);
}

nsIntSize
nsGlobalWindow::GetOuterSize(ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  if (nsContentUtils::ShouldResistFingerprinting(mDocShell)) {
    CSSIntSize size;
    aError = GetInnerSize(size);
    return nsIntSize(size.width, size.height);
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return nsIntSize(0, 0);
  }

  nsGlobalWindow* rootWindow = nsGlobalWindow::Cast(GetPrivateRoot());
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
nsGlobalWindow::GetOuterWidthOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetOuterSize(aError).width;
}

int32_t
nsGlobalWindow::GetOuterWidth(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterWidthOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetOuterWidth(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetOuterWidth, aValue,
                            aError);
}

int32_t
nsGlobalWindow::GetOuterHeightOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetOuterSize(aError).height;
}

int32_t
nsGlobalWindow::GetOuterHeight(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterHeightOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetOuterHeight(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aValue,
                               ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetOuterHeight, aValue,
                            aError);
}

void
nsGlobalWindow::SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth,
                             ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  CheckSecurityWidthAndHeight(aIsWidth ? &aLengthCSSPixels : nullptr,
                              aIsWidth ? nullptr : &aLengthCSSPixels,
                              aCallerIsChrome);

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

  CheckForDPIChange();
}

void
nsGlobalWindow::SetOuterWidthOuter(int32_t aOuterWidth, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  SetOuterSize(aOuterWidth, true, aError, aCallerIsChrome);
}

void
nsGlobalWindow::SetOuterWidth(int32_t aOuterWidth, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterWidthOuter, (aOuterWidth, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetOuterWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetOuterWidth,
                            aValue, "outerWidth", aError);
}

void
nsGlobalWindow::SetOuterHeightOuter(int32_t aOuterHeight, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  SetOuterSize(aOuterHeight, false, aError, aCallerIsChrome);
}

void
nsGlobalWindow::SetOuterHeight(int32_t aOuterHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterHeightOuter, (aOuterHeight, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetOuterHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                               ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetOuterHeight,
                            aValue, "outerHeight", aError);
}

CSSIntPoint
nsGlobalWindow::GetScreenXY(ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  // When resisting fingerprinting, always return (0,0)
  if (nsContentUtils::ShouldResistFingerprinting(mDocShell)) {
    return CSSIntPoint(0, 0);
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return CSSIntPoint(0, 0);
  }

  int32_t x = 0, y = 0;
  aError = treeOwnerAsWin->GetPosition(&x, &y); // LayoutDevice px values

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return CSSIntPoint(x, y);
  }

  // Find the global desktop coordinate of the top-left of the screen.
  // We'll use this as a "fake origin" when converting to CSS px units,
  // to avoid overlapping coordinates in cases such as a hi-dpi screen
  // placed to the right of a lo-dpi screen on Windows. (Instead, there
  // may be "gaps" in the resulting CSS px coordinates in some cases.)
  nsDeviceContext *dc = presContext->DeviceContext();
  nsRect screenRect;
  dc->GetRect(screenRect);
  LayoutDeviceRect screenRectDev =
    LayoutDevicePixel::FromAppUnits(screenRect, dc->AppUnitsPerDevPixel());

  DesktopToLayoutDeviceScale scale = dc->GetDesktopToDeviceScale();
  DesktopRect screenRectDesk = screenRectDev / scale;

  CSSPoint cssPt =
    LayoutDevicePoint(x - screenRectDev.x, y - screenRectDev.y) /
    presContext->CSSToDevPixelScale();
  cssPt.x += screenRectDesk.x;
  cssPt.y += screenRectDesk.y;

  return CSSIntPoint(NSToIntRound(cssPt.x), NSToIntRound(cssPt.y));
}

int32_t
nsGlobalWindow::GetScreenXOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return GetScreenXY(aError).x;
}

int32_t
nsGlobalWindow::GetScreenX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenXOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetScreenX(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aValue,
                           ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetScreenX, aValue,
                            aError);
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nsRect();
  }

  nsGlobalWindow* rootWindow = nsGlobalWindow::Cast(GetPrivateRoot());
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
nsGlobalWindow::GetMozInnerScreenXOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // When resisting fingerprinting, always return 0.
  if (nsContentUtils::ShouldResistFingerprinting(mDocShell)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.x);
}

float
nsGlobalWindow::GetMozInnerScreenX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenXOuter, (), aError, 0);
}

float
nsGlobalWindow::GetMozInnerScreenYOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // Return 0 to prevent fingerprinting.
  if (nsContentUtils::ShouldResistFingerprinting(mDocShell)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.y);
}

float
nsGlobalWindow::GetMozInnerScreenY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenYOuter, (), aError, 0);
}

float
nsGlobalWindow::GetDevicePixelRatioOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return 1.0;
  }

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) {
    return 1.0;
  }

  if (nsContentUtils::ShouldResistFingerprinting(mDocShell)) {
    return 1.0;
  }

  return float(nsPresContext::AppUnitsPerCSSPixel())/
      presContext->AppUnitsPerDevPixel();
}

float
nsGlobalWindow::GetDevicePixelRatio(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDevicePixelRatioOuter, (), aError, 0.0);
}

nsresult
nsGlobalWindow::GetDevicePixelRatio(float* aRatio)
{
  FORWARD_TO_INNER(GetDevicePixelRatio, (aRatio), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  *aRatio = GetDevicePixelRatio(rv);

  return rv.StealNSResult();
}

uint64_t
nsGlobalWindow::GetMozPaintCountOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return 0;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  return presShell ? presShell->GetPaintCount() : 0;
}

uint64_t
nsGlobalWindow::GetMozPaintCount(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozPaintCountOuter, (), aError, 0);
}

int32_t
nsGlobalWindow::RequestAnimationFrame(FrameRequestCallback& aCallback,
                                      ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

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

void
nsGlobalWindow::CancelAnimationFrame(int32_t aHandle, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mDoc) {
    return;
  }

  mDoc->CancelFrameRequestCallback(aHandle);
}

already_AddRefed<MediaQueryList>
nsGlobalWindow::MatchMediaOuter(const nsAString& aMediaQueryList)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDoc) {
    return nullptr;
  }

  return mDoc->MatchMedia(aMediaQueryList);
}

already_AddRefed<MediaQueryList>
nsGlobalWindow::MatchMedia(const nsAString& aMediaQueryList,
                           ErrorResult& aError)
{
  // FIXME: This whole forward-to-outer and then get a pres
  // shell/context off the docshell dance is sort of silly; it'd make
  // more sense to forward to the inner, but it's what everyone else
  // (GetSelection, GetScrollXY, etc.) does around here.
  FORWARD_TO_OUTER_OR_THROW(MatchMediaOuter, (aMediaQueryList), aError, nullptr);
}

void
nsGlobalWindow::SetScreenXOuter(int32_t aScreenX, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

  CheckSecurityLeftAndTop(&aScreenX, nullptr, aCallerIsChrome);
  x = CSSToDevIntPixels(aScreenX);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

void
nsGlobalWindow::SetScreenX(int32_t aScreenX, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenXOuter, (aScreenX, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetScreenX(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetScreenX,
                            aValue, "screenX", aError);
}

int32_t
nsGlobalWindow::GetScreenYOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return GetScreenXY(aError).y;
}

int32_t
nsGlobalWindow::GetScreenY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenYOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetScreenY(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aValue,
                           ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetScreenY, aValue,
                            aError);
}

void
nsGlobalWindow::SetScreenYOuter(int32_t aScreenY, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

  CheckSecurityLeftAndTop(nullptr, &aScreenY, aCallerIsChrome);
  y = CSSToDevIntPixels(aScreenY);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

void
nsGlobalWindow::SetScreenY(int32_t aScreenY, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenYOuter, (aScreenY, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SetScreenY(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetScreenY,
                            aValue, "screenY", aError);
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindow::CheckSecurityWidthAndHeight(int32_t* aWidth, int32_t* aHeight, bool aCallerIsChrome)
{
  MOZ_ASSERT(IsOuterWindow());

#ifdef MOZ_XUL
  if (!aCallerIsChrome) {
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

  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));

  nsRect shellArea = presContext->GetVisibleArea();
  shellArea.height = aInnerHeight;
  shellArea.width = aInnerWidth;

  presContext->SetVisibleArea(shellArea);
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindow::CheckSecurityLeftAndTop(int32_t* aLeft, int32_t* aTop, bool aCallerIsChrome)
{
  MOZ_ASSERT(IsOuterWindow());

  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!aCallerIsChrome) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
#endif

    if (nsGlobalWindow* rootWindow = nsGlobalWindow::Cast(GetPrivateRoot())) {
      rootWindow->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIBaseWindow> treeOwner = GetTreeOwnerWindow();

    nsCOMPtr<nsIDOMScreen> screen = GetScreen();

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

int32_t
nsGlobalWindow::GetScrollBoundaryOuter(Side aSide)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  FlushPendingNotifications(Flush_Layout);
  if (nsIScrollableFrame *sf = GetScrollFrame()) {
    return nsPresContext::
      AppUnitsToIntCSSPixels(sf->GetScrollRange().Edge(aSide));
  }
  return 0;
}

int32_t
nsGlobalWindow::GetScrollMinX(ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideLeft), aError, 0);
}

int32_t
nsGlobalWindow::GetScrollMinY(ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideTop), aError, 0);
}

int32_t
nsGlobalWindow::GetScrollMaxX(ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideRight), aError, 0);
}

int32_t
nsGlobalWindow::GetScrollMaxY(ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideBottom), aError, 0);
}

CSSIntPoint
nsGlobalWindow::GetScrollXY(bool aDoFlush)
{
  MOZ_ASSERT(IsOuterWindow());

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
    return GetScrollXY(true);
  }

  return sf->GetScrollPositionCSSPixels();
}

int32_t
nsGlobalWindow::GetScrollXOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetScrollXY(false).x;
}

int32_t
nsGlobalWindow::GetScrollX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollXOuter, (), aError, 0);
}

int32_t
nsGlobalWindow::GetScrollYOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetScrollXY(false).y;
}

int32_t
nsGlobalWindow::GetScrollY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollYOuter, (), aError, 0);
}

uint32_t
nsGlobalWindow::Length()
{
  FORWARD_TO_OUTER(Length, (), 0);

  nsDOMWindowList* windows = GetWindowList();

  return windows ? windows->GetLength() : 0;
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetTopOuter()
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> top = GetScriptableTop();
  return top.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetTop(mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetTopOuter, (), aError, nullptr);
}

nsPIDOMWindowOuter*
nsGlobalWindow::GetChildWindow(const nsAString& aName)
{
  nsCOMPtr<nsIDocShell> docShell(GetDocShell());
  NS_ENSURE_TRUE(docShell, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> child;
  docShell->FindChildWithName(PromiseFlatString(aName).get(),
                              false, true, nullptr, nullptr,
                              getter_AddRefs(child));

  return child ? child->GetWindow() : nullptr;
}

bool
nsGlobalWindow::DispatchCustomEvent(const nsAString& aEventName)
{
  MOZ_ASSERT(IsOuterWindow());

  bool defaultActionEnabled = true;
  nsContentUtils::DispatchTrustedEvent(mDoc, ToSupports(this), aEventName,
                                       true, true, &defaultActionEnabled);

  return defaultActionEnabled;
}

bool
nsGlobalWindow::DispatchResizeEvent(const CSSIntSize& aSize)
{
  MOZ_ASSERT(IsOuterWindow());

  ErrorResult res;
  RefPtr<Event> domEvent =
    mDoc->CreateEvent(NS_LITERAL_STRING("CustomEvent"), res);
  if (res.Failed()) {
    return false;
  }

  // We don't init the AutoJSAPI with ourselves because we don't want it
  // reporting errors to our onerror handlers.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JSAutoCompartment ac(cx, GetWrapperPreserveColor());

  DOMWindowResizeEventDetail detail;
  detail.mWidth = aSize.width;
  detail.mHeight = aSize.height;
  JS::Rooted<JS::Value> detailValue(cx);
  if (!ToJSValue(cx, detail, &detailValue)) {
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
  domEvent->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOuterWindow());
  domEvent->SetTarget(target);

  bool defaultActionEnabled = true;
  target->DispatchEvent(domEvent, &defaultActionEnabled);

  return defaultActionEnabled;
}

void
nsGlobalWindow::RefreshCompartmentPrincipal()
{
  MOZ_ASSERT(IsInnerWindow());

  JS_SetCompartmentPrincipals(js::GetObjectCompartment(GetWrapperPreserveColor()),
                              nsJSPrincipals::get(mDoc->NodePrincipal()));
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  nsCOMPtr<nsIWebNavigation> callerWebNav = do_GetInterface(GetEntryGlobal());
  nsCOMPtr<nsIDocShellTreeItem> callerItem = do_QueryInterface(callerWebNav);

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
nsGlobalWindow::GetNearestWidget() const
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
nsGlobalWindow::SetFullScreenOuter(bool aFullScreen, mozilla::ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  aError = SetFullscreenInternal(FullscreenReason::ForFullscreenMode, aFullScreen);
}

void
nsGlobalWindow::SetFullScreen(bool aFullScreen, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetFullScreenOuter, (aFullScreen, aError), aError, /* void */);
}

nsresult
nsGlobalWindow::SetFullScreen(bool aFullScreen)
{
  FORWARD_TO_OUTER(SetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  return SetFullscreenInternal(FullscreenReason::ForFullscreenMode, aFullScreen);
}

void
FinishDOMFullscreenChange(nsIDocument* aDoc, bool aInDOMFullscreen)
{
  if (aInDOMFullscreen) {
    // Ask the document to handle any pending DOM fullscreen change.
    nsIDocument::HandlePendingFullscreenRequests(aDoc);
  } else {
    // If the window is leaving fullscreen state, also ask the document
    // to exit from DOM Fullscreen.
    nsIDocument::ExitFullscreenInDocTree(aDoc);
  }
}

struct FullscreenTransitionDuration
{
  // The unit of the durations is millisecond
  uint16_t mFadeIn = 0;
  uint16_t mFadeOut = 0;
  bool IsSuppressed() const
  {
    return mFadeIn == 0 && mFadeOut == 0;
  }
};

static void
GetFullscreenTransitionDuration(bool aEnterFullscreen,
                                FullscreenTransitionDuration* aDuration)
{
  const char* pref = aEnterFullscreen ?
    "full-screen-api.transition-duration.enter" :
    "full-screen-api.transition-duration.leave";
  nsAdoptingCString prefValue = Preferences::GetCString(pref);
  if (!prefValue.IsEmpty()) {
    sscanf(prefValue.get(), "%hu%hu",
           &aDuration->mFadeIn, &aDuration->mFadeOut);
  }
}

class FullscreenTransitionTask : public Runnable
{
public:
  FullscreenTransitionTask(const FullscreenTransitionDuration& aDuration,
                           nsGlobalWindow* aWindow, bool aFullscreen,
                           nsIWidget* aWidget, nsIScreen* aScreen,
                           nsISupports* aTransitionData)
    : mWindow(aWindow)
    , mWidget(aWidget)
    , mScreen(aScreen)
    , mTransitionData(aTransitionData)
    , mDuration(aDuration)
    , mStage(eBeforeToggle)
    , mFullscreen(aFullscreen)
  {
    MOZ_COUNT_CTOR(FullscreenTransitionTask);
  }

  NS_IMETHOD Run() override;

private:
  virtual ~FullscreenTransitionTask()
  {
    MOZ_COUNT_DTOR(FullscreenTransitionTask);
  }

  /**
   * The flow of fullscreen transition:
   *
   *         parent process         |         child process
   * ----------------------------------------------------------------
   *
   *                                    | request/exit fullscreen
   *                              <-----|
   *         BeforeToggle stage |
   *                            |
   *  ToggleFullscreen stage *1 |----->
   *                                    | HandleFullscreenRequests
   *                                    |
   *                              <-----| MozAfterPaint event
   *       AfterToggle stage *2 |
   *                            |
   *                  End stage |
   *
   * Note we also start a timer at *1 so that if we don't get MozAfterPaint
   * from the child process in time, we continue going to *2.
   */
  enum Stage {
    // BeforeToggle stage happens before we enter or leave fullscreen
    // state. In this stage, the task triggers the pre-toggle fullscreen
    // transition on the widget.
    eBeforeToggle,
    // ToggleFullscreen stage actually executes the fullscreen toggle,
    // and wait for the next paint on the content to continue.
    eToggleFullscreen,
    // AfterToggle stage happens after we toggle the fullscreen state.
    // In this stage, the task triggers the post-toggle fullscreen
    // transition on the widget.
    eAfterToggle,
    // End stage is triggered after the final transition finishes.
    eEnd
  };

  class Observer final : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    explicit Observer(FullscreenTransitionTask* aTask)
      : mTask(aTask) { }

  private:
    ~Observer() {}

    RefPtr<FullscreenTransitionTask> mTask;
  };

  static const char* const kPaintedTopic;

  RefPtr<nsGlobalWindow> mWindow;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIScreen> mScreen;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsISupports> mTransitionData;

  TimeStamp mFullscreenChangeStartTime;
  FullscreenTransitionDuration mDuration;
  Stage mStage;
  bool mFullscreen;
};

const char* const
FullscreenTransitionTask::kPaintedTopic = "fullscreen-painted";

NS_IMETHODIMP
FullscreenTransitionTask::Run()
{
  Stage stage = mStage;
  mStage = Stage(mStage + 1);
  if (MOZ_UNLIKELY(mWidget->Destroyed())) {
    // If the widget has been destroyed before we get here, don't try to
    // do anything more. Just let it go and release ourselves.
    NS_WARNING("The widget to fullscreen has been destroyed");
    return NS_OK;
  }
  if (stage == eBeforeToggle) {
    PROFILER_MARKER("Fullscreen transition start");
    mWidget->PerformFullscreenTransition(nsIWidget::eBeforeFullscreenToggle,
                                         mDuration.mFadeIn, mTransitionData,
                                         this);
  } else if (stage == eToggleFullscreen) {
    PROFILER_MARKER("Fullscreen toggle start");
    mFullscreenChangeStartTime = TimeStamp::Now();
    if (MOZ_UNLIKELY(mWindow->mFullScreen != mFullscreen)) {
      // This could happen in theory if several fullscreen requests in
      // different direction happen continuously in a short time. We
      // need to ensure the fullscreen state matches our target here,
      // otherwise the widget would change the window state as if we
      // toggle for Fullscreen Mode instead of Fullscreen API.
      NS_WARNING("The fullscreen state of the window does not match");
      mWindow->mFullScreen = mFullscreen;
    }
    // Toggle the fullscreen state on the widget
    if (!mWindow->SetWidgetFullscreen(FullscreenReason::ForFullscreenAPI,
                                      mFullscreen, mWidget, mScreen)) {
      // Fail to setup the widget, call FinishFullscreenChange to
      // complete fullscreen change directly.
      mWindow->FinishFullscreenChange(mFullscreen);
    }
    // Set observer for the next content paint.
    nsCOMPtr<nsIObserver> observer = new Observer(this);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(observer, kPaintedTopic, false);
    // There are several edge cases where we may never get the paint
    // notification, including:
    // 1. the window/tab is closed before the next paint;
    // 2. the user has switched to another tab before we get here.
    // Completely fixing those cases seems to be tricky, and since they
    // should rarely happen, it probably isn't worth to fix. Hence we
    // simply add a timeout here to ensure we never hang forever.
    // In addition, if the page is complicated or the machine is less
    // powerful, layout could take a long time, in which case, staying
    // in black screen for that long could hurt user experience even
    // more than exposing an intermediate state.
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    uint32_t timeout =
      Preferences::GetUint("full-screen-api.transition.timeout", 1000);
    mTimer->Init(observer, timeout, nsITimer::TYPE_ONE_SHOT);
  } else if (stage == eAfterToggle) {
    Telemetry::AccumulateTimeDelta(Telemetry::FULLSCREEN_TRANSITION_BLACK_MS,
                                   mFullscreenChangeStartTime);
    mWidget->PerformFullscreenTransition(nsIWidget::eAfterFullscreenToggle,
                                         mDuration.mFadeOut, mTransitionData,
                                         this);
  } else if (stage == eEnd) {
    PROFILER_MARKER("Fullscreen transition end");
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(FullscreenTransitionTask::Observer, nsIObserver)

NS_IMETHODIMP
FullscreenTransitionTask::Observer::Observe(nsISupports* aSubject,
                                            const char* aTopic,
                                            const char16_t* aData)
{
  bool shouldContinue = false;
  if (strcmp(aTopic, FullscreenTransitionTask::kPaintedTopic) == 0) {
    nsCOMPtr<nsPIDOMWindowInner> win(do_QueryInterface(aSubject));
    nsCOMPtr<nsIWidget> widget = win ?
      nsGlobalWindow::Cast(win)->GetMainWidget() : nullptr;
    if (widget == mTask->mWidget) {
      // The paint notification arrives first. Cancel the timer.
      mTask->mTimer->Cancel();
      shouldContinue = true;
      PROFILER_MARKER("Fullscreen toggle end");
    }
  } else {
#ifdef DEBUG
    MOZ_ASSERT(strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) == 0,
               "Should only get fullscreen-painted or timer-callback");
    nsCOMPtr<nsITimer> timer(do_QueryInterface(aSubject));
    MOZ_ASSERT(timer && timer == mTask->mTimer,
               "Should only trigger this with the timer the task created");
#endif
    shouldContinue = true;
    PROFILER_MARKER("Fullscreen toggle timeout");
  }
  if (shouldContinue) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, kPaintedTopic);
    mTask->mTimer = nullptr;
    mTask->Run();
  }
  return NS_OK;
}

static bool
MakeWidgetFullscreen(nsGlobalWindow* aWindow, FullscreenReason aReason,
                     bool aFullscreen)
{
  nsCOMPtr<nsIWidget> widget = aWindow->GetMainWidget();
  if (!widget) {
    return false;
  }

  FullscreenTransitionDuration duration;
  bool performTransition = false;
  nsCOMPtr<nsISupports> transitionData;
  if (aReason == FullscreenReason::ForFullscreenAPI) {
    GetFullscreenTransitionDuration(aFullscreen, &duration);
    if (!duration.IsSuppressed()) {
      performTransition = widget->
        PrepareForFullscreenTransition(getter_AddRefs(transitionData));
    }
  }
  // We pass nullptr as the screen to SetWidgetFullscreen
  // and FullscreenTransitionTask, as we do not wish to override
  // the default screen selection behavior.  The screen containing
  // most of the widget will be selected.
  if (!performTransition) {
    return aWindow->SetWidgetFullscreen(aReason, aFullscreen, widget, nullptr);
  } else {
    nsCOMPtr<nsIRunnable> task =
      new FullscreenTransitionTask(duration, aWindow, aFullscreen,
                                   widget, nullptr, transitionData);
    task->Run();
    return true;
  }
}

nsresult
nsGlobalWindow::SetFullscreenInternal(FullscreenReason aReason,
                                      bool aFullScreen)
{
  MOZ_ASSERT(IsOuterWindow());
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript(),
             "Requires safe to run script as it "
             "may call FinishDOMFullscreenChange");

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  MOZ_ASSERT(aReason != FullscreenReason::ForForceExitFullscreen || !aFullScreen,
             "FullscreenReason::ForForceExitFullscreen can "
             "only be used with exiting fullscreen");

  // Only chrome can change our fullscreen mode. Otherwise, the state
  // can only be changed for DOM fullscreen.
  if (aReason == FullscreenReason::ForFullscreenMode &&
      !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> window = rootItem ? rootItem->GetWindow() : nullptr;
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != mDocShell)
    return window->SetFullscreenInternal(aReason, aFullScreen);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_FAILURE;

  // If we are already in full screen mode, just return.
  if (mFullScreen == aFullScreen)
    return NS_OK;

  // Note that although entering DOM fullscreen could also cause
  // consequential calls to this method, those calls will be skipped
  // at the condition above.
  if (aReason == FullscreenReason::ForFullscreenMode) {
    if (!aFullScreen && !mFullscreenMode) {
      // If we are exiting fullscreen mode, but we actually didn't
      // entered fullscreen mode, the fullscreen state was only for
      // the Fullscreen API. Change the reason here so that we can
      // perform transition for it.
      aReason = FullscreenReason::ForFullscreenAPI;
    } else {
      mFullscreenMode = aFullScreen;
    }
  } else {
    // If we are exiting from DOM fullscreen while we initially make
    // the window fullscreen because of fullscreen mode, don't restore
    // the window. But we still need to exit the DOM fullscreen state.
    if (!aFullScreen && mFullscreenMode) {
      FinishDOMFullscreenChange(mDoc, false);
      return NS_OK;
    }
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
    if (MakeWidgetFullscreen(this, aReason, aFullScreen)) {
      // The rest of code for switching fullscreen is in nsGlobalWindow::
      // FinishFullscreenChange() which will be called after sizemodechange
      // event is dispatched.
      return NS_OK;
    }
  }

  FinishFullscreenChange(aFullScreen);
  return NS_OK;
}

bool
nsGlobalWindow::SetWidgetFullscreen(FullscreenReason aReason, bool aIsFullscreen,
                                    nsIWidget* aWidget, nsIScreen* aScreen)
{
  MOZ_ASSERT(IsOuterWindow());
  MOZ_ASSERT(this == GetTopInternal(), "Only topmost window should call this");
  MOZ_ASSERT(!AsOuter()->GetFrameElementInternal(), "Content window should not call this");
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (!NS_WARN_IF(!IsChromeWindow())) {
    auto chromeWin = static_cast<nsGlobalChromeWindow*>(this);
    if (!NS_WARN_IF(chromeWin->mFullscreenPresShell)) {
      if (nsIPresShell* shell = mDocShell->GetPresShell()) {
        if (nsRefreshDriver* rd = shell->GetRefreshDriver()) {
          chromeWin->mFullscreenPresShell = do_GetWeakReference(shell);
          MOZ_ASSERT(chromeWin->mFullscreenPresShell);
          rd->SetIsResizeSuppressed();
          rd->Freeze();
        }
      }
    }
  }
  nsresult rv = aReason == FullscreenReason::ForFullscreenMode ?
    // If we enter fullscreen for fullscreen mode, we want
    // the native system behavior.
    aWidget->MakeFullScreenWithNativeTransition(aIsFullscreen, aScreen) :
    aWidget->MakeFullScreen(aIsFullscreen, aScreen);
  return NS_SUCCEEDED(rv);
}

/* virtual */ void
nsGlobalWindow::FinishFullscreenChange(bool aIsFullscreen)
{
  MOZ_ASSERT(IsOuterWindow());

  if (aIsFullscreen != mFullScreen) {
    NS_WARNING("Failed to toggle fullscreen state of the widget");
    // We failed to make the widget enter fullscreen.
    // Stop further changes and restore the state.
    if (!aIsFullscreen) {
      mFullScreen = false;
      mFullscreenMode = false;
    } else {
      MOZ_ASSERT_UNREACHABLE("Failed to exit fullscreen?");
      mFullScreen = true;
      // We don't know how code can reach here. Not sure
      // what value should be set for fullscreen mode.
      mFullscreenMode = false;
    }
    return;
  }

  // Note that we must call this to toggle the DOM fullscreen state
  // of the document before dispatching the "fullscreen" event, so
  // that the chrome can distinguish between browser fullscreen mode
  // and DOM fullscreen.
  FinishDOMFullscreenChange(mDoc, mFullScreen);

  // dispatch a "fullscreen" DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  DispatchCustomEvent(NS_LITERAL_STRING("fullscreen"));

  if (!NS_WARN_IF(!IsChromeWindow())) {
    auto chromeWin = static_cast<nsGlobalChromeWindow*>(this);
    if (nsCOMPtr<nsIPresShell> shell =
        do_QueryReferent(chromeWin->mFullscreenPresShell)) {
      if (nsRefreshDriver* rd = shell->GetRefreshDriver()) {
        rd->Thaw();
      }
      chromeWin->mFullscreenPresShell = nullptr;
    }
  }

  if (!mWakeLock && mFullScreen) {
    RefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
    if (!pmService) {
      return;
    }

    // XXXkhuey using the inner here, do we need to do something if it changes?
    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("DOM_Fullscreen"),
                                       AsOuter()->GetCurrentInnerWindow(), rv);
    NS_WARN_IF_FALSE(!rv.Failed(), "Failed to lock the wakelock");
    rv.SuppressException();
  } else if (mWakeLock && !mFullScreen) {
    ErrorResult rv;
    mWakeLock->Unlock(rv);
    mWakeLock = nullptr;
    rv.SuppressException();
  }
}

bool
nsGlobalWindow::FullScreen() const
{
  MOZ_ASSERT(IsOuterWindow());

  NS_ENSURE_TRUE(mDocShell, mFullScreen);

  // Get the fullscreen value of the root window, to always have the value
  // accurate, even when called from content.
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  if (rootItem == mDocShell) {
    if (!XRE_IsContentProcess()) {
      // We are the root window. Return our internal value.
      return mFullScreen;
    }
    if (nsCOMPtr<nsIWidget> widget = GetNearestWidget()) {
      // We are in content process, figure out the value from
      // the sizemode of the puppet widget.
      return widget->SizeMode() == nsSizeMode_Fullscreen;
    }
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = rootItem->GetWindow();
  NS_ENSURE_TRUE(window, mFullScreen);

  return nsGlobalWindow::Cast(window)->FullScreen();
}

bool
nsGlobalWindow::GetFullScreenOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return FullScreen();
}

bool
nsGlobalWindow::GetFullScreen(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFullScreenOuter, (), aError, false);
}

bool
nsGlobalWindow::GetFullScreen()
{
  FORWARD_TO_INNER(GetFullScreen, (), false);

  ErrorResult dummy;
  bool fullscreen = GetFullScreen(dummy);
  dummy.SuppressException();
  return fullscreen;
}

void
nsGlobalWindow::Dump(const nsAString& aStr)
{
  if (!nsContentUtils::DOMWindowDumpEnabled()) {
    return;
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
    MOZ_LOG(nsContentUtils::DOMDumpLog(), LogLevel::Debug, ("[Window.Dump] %s", cstr));
#ifdef XP_WIN
    PrintToDebugger(cstr);
#endif
#ifdef ANDROID
    __android_log_write(ANDROID_LOG_INFO, "GeckoDump", cstr);
#endif
    FILE *fp = gDumpFile ? gDumpFile : stdout;
    fputs(cstr, fp);
    fflush(fp);
    free(cstr);
  }
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

// static
void
nsGlobalWindow::MakeScriptDialogTitle(nsAString &aOutTitle)
{
  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::SubjectPrincipal();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = principal->GetURI(getter_AddRefs(uri));
  // Note - The check for the current JSContext here isn't necessarily sensical.
  // It's just designed to preserve existing behavior during a mass-conversion
  // patch.
  if (NS_SUCCEEDED(rv) && uri && nsContentUtils::GetCurrentJSContext()) {
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
nsGlobalWindow::CanMoveResizeWindows(bool aCallerIsChrome)
{
  MOZ_ASSERT(IsOuterWindow());

  // When called from chrome, we can avoid the following checks.
  if (!aCallerIsChrome) {
    // Don't allow scripts to move or resize windows that were not opened by a
    // script.
    if (!mHadOriginalOpener) {
      return false;
    }

    if (!CanSetProperty("dom.disable_window_move_resize")) {
      return false;
    }

    // Ignore the request if we have more than one tab in the window.
    uint32_t itemCount = 0;
    if (XRE_IsContentProcess()) {
      nsCOMPtr<nsIDocShell> docShell = GetDocShell();
      if (docShell) {
        nsCOMPtr<nsITabChild> child = docShell->GetTabChild();
        if (child) {
          child->SendGetTabCount(&itemCount);
        }
      }
    } else {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();
      if (treeOwner) {
        treeOwner->GetTargetableShellCount(&itemCount);
      }
    }
    if (itemCount > 1) {
      return false;
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

bool
nsGlobalWindow::AlertOrConfirm(bool aAlert,
                               const nsAString& aMessage,
                               mozilla::ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindow::Prompt, make
  // sure any modifications here don't need to happen over there!
  MOZ_ASSERT(IsOuterWindow());

  if (!AreDialogsEnabled()) {
    // Just silently return.  In the case of alert(), the return value is
    // ignored.  In the case of confirm(), returning false is the same thing as
    // would happen if the user cancels.
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
  aError = promptFac->GetPrompt(AsOuter(), NS_GET_IID(nsIPrompt),
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
nsGlobalWindow::Alert(mozilla::ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  Alert(EmptyString(), aError);
}

void
nsGlobalWindow::AlertOuter(const nsAString& aMessage, mozilla::ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  AlertOrConfirm(/* aAlert = */ true, aMessage, aError);
}

void
nsGlobalWindow::Alert(const nsAString& aMessage, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(AlertOuter, (aMessage, aError), aError, );
}

bool
nsGlobalWindow::ConfirmOuter(const nsAString& aMessage, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return AlertOrConfirm(/* aAlert = */ false, aMessage, aError);
}

bool
nsGlobalWindow::Confirm(const nsAString& aMessage, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ConfirmOuter, (aMessage, aError), aError, false);
}

already_AddRefed<Promise>
nsGlobalWindow::Fetch(const RequestOrUSVString& aInput,
                      const RequestInit& aInit, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aRv);
}

void
nsGlobalWindow::PromptOuter(const nsAString& aMessage, const nsAString& aInitial,
                            nsAString& aReturn, ErrorResult& aError)
{
  // XXX This method is very similar to nsGlobalWindow::AlertOrConfirm, make
  // sure any modifications here don't need to happen over there!
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  SetDOMStringToNull(aReturn);

  if (!AreDialogsEnabled()) {
    // Return null, as if the user just canceled the prompt.
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
  aError = promptFac->GetPrompt(AsOuter(), NS_GET_IID(nsIPrompt),
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

void
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       nsAString& aReturn, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PromptOuter, (aMessage, aInitial, aReturn, aError),
                            aError, );
}

void
nsGlobalWindow::FocusOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

  nsCOMPtr<nsPIDOMWindowInner> caller = do_QueryInterface(GetEntryGlobal());
  nsPIDOMWindowOuter* callerOuter = caller ? caller->GetOuterWindow() : nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpener();

  // Enforce dom.disable_window_flip (for non-chrome), but still allow the
  // window which opened us to raise us at times when popups are allowed
  // (bugs 355482 and 369306).
  bool canFocus = CanSetProperty("dom.disable_window_flip") ||
                    (opener == callerOuter &&
                     RevisePopupAbuseLevel(gPopupControlState) < openAbused);

  nsCOMPtr<mozIDOMWindowProxy> activeDOMWindow;
  fm->GetActiveWindow(getter_AddRefs(activeDOMWindow));

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  mDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem ? rootItem->GetWindow() : nullptr;
  auto* activeWindow = nsPIDOMWindowOuter::From(activeDOMWindow);
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
      GetPrivateRoot() == AsOuter() && mDoc) {
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
  nsCOMPtr<nsPIDOMWindowOuter> parent =
    parentDsti ? parentDsti->GetWindow() : nullptr;
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

  if (canFocus) {
    // if there is no parent, this must be a toplevel window, so raise the
    // window if canFocus is true. If this is a child process, the raise
    // window request will get forwarded to the parent by the puppet widget.
    aError = fm->SetActiveWindow(AsOuter());
  }
}

void
nsGlobalWindow::Focus(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(FocusOuter, (aError), aError, );
}

nsresult
nsGlobalWindow::Focus()
{
  FORWARD_TO_INNER(Focus, (), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  Focus(rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::BlurOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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
      fm->GetFocusedElementForWindow(AsOuter(), false, nullptr, getter_AddRefs(element));
      nsCOMPtr<nsIContent> content = do_QueryInterface(element);
      if (content == mDoc->GetRootElement()) {
        fm->ClearFocus(AsOuter());
      }
    }
  }
}

void
nsGlobalWindow::Blur(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(BlurOuter, (), aError, );
}

void
nsGlobalWindow::BackOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoBack();
}

void
nsGlobalWindow::Back(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(BackOuter, (aError), aError, );
}

void
nsGlobalWindow::ForwardOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = webNav->GoForward();
}

void
nsGlobalWindow::Forward(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ForwardOuter, (aError), aError, );
}

void
nsGlobalWindow::HomeOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

void
nsGlobalWindow::Home(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(HomeOuter, (aError), aError, );
}

void
nsGlobalWindow::StopOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (webNav) {
    aError = webNav->Stop(nsIWebNavigation::STOP_ALL);
  }
}

void
nsGlobalWindow::Stop(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(StopOuter, (aError), aError, );
}

void
nsGlobalWindow::PrintOuter(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

#ifdef NS_PRINTING
  if (Preferences::GetBool("dom.disable_window_print", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (!AreDialogsEnabled()) {
    // We probably want to keep throwing here; silently doing nothing is a bit
    // weird given the typical use cases of print().
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
                               GetCurrentInnerWindowInternal()->mDoc.get() :
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

void
nsGlobalWindow::Print(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PrintOuter, (aError), aError, );
}

void
nsGlobalWindow::MoveToOuter(int32_t aXPos, int32_t aYPos, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerIsChrome) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  nsCOMPtr<nsIScreen> screen;
  if (screenMgr) {
    CSSIntSize size;
    GetInnerSize(size);
    screenMgr->ScreenForRect(aXPos, aYPos, size.width, size.height,
                             getter_AddRefs(screen));
  }

  if (screen) {
    // On secondary displays, the "CSS px" coordinates are offset so that they
    // share their origin with global desktop pixels, to avoid ambiguities in
    // the coordinate space when there are displays with different DPIs.
    // (See the corresponding code in GetScreenXY() above.)
    int32_t screenLeftDeskPx, screenTopDeskPx, w, h;
    screen->GetRectDisplayPix(&screenLeftDeskPx, &screenTopDeskPx, &w, &h);
    CSSIntPoint cssPos(aXPos - screenLeftDeskPx, aYPos - screenTopDeskPx);
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerIsChrome);

    double scale;
    screen->GetDefaultCSSScaleFactor(&scale);
    LayoutDevicePoint devPos = cssPos * CSSToLayoutDeviceScale(scale);

    screen->GetContentsScaleFactor(&scale);
    DesktopPoint deskPos = devPos / DesktopToLayoutDeviceScale(scale);
    aError = treeOwnerAsWin->SetPositionDesktopPix(screenLeftDeskPx + deskPos.x,
                                                   screenTopDeskPx + deskPos.y);
  } else {
    // We couldn't find a screen? Just assume a 1:1 mapping.
    CSSIntPoint cssPos(aXPos, aXPos);
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerIsChrome);
    LayoutDevicePoint devPos = cssPos * CSSToLayoutDeviceScale(1.0);
    aError = treeOwnerAsWin->SetPosition(devPos.x, devPos.y);
  }

  CheckForDPIChange();
}

void
nsGlobalWindow::MoveTo(int32_t aXPos, int32_t aYPos, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveToOuter, (aXPos, aYPos, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::MoveByOuter(int32_t aXDif, int32_t aYDif, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerIsChrome) || IsFrame()) {
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

  CheckSecurityLeftAndTop(&cssPos.width, &cssPos.height, aCallerIsChrome);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  aError = treeOwnerAsWin->SetPosition(newDevPos.width, newDevPos.height);

  CheckForDPIChange();
}

void
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveByOuter, (aXDif, aYDif, aError, nsContentUtils::IsCallerChrome()), aError, );
}

nsresult
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif)
{
  FORWARD_TO_OUTER(MoveBy, (aXDif, aYDif), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  MoveByOuter(aXDif, aYDif, rv, /* aCallerIsChrome = */ true);

  return rv.StealNSResult();
}

void
nsGlobalWindow::ResizeToOuter(int32_t aWidth, int32_t aHeight, ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is a browser-element then dispatch a resize event to
   * the embedder.
   */
  if (mDocShell && mDocShell->GetIsMozBrowserOrApp()) {
    CSSIntSize size(aWidth, aHeight);
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

  if (!CanMoveResizeWindows(aCallerIsChrome) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIntSize cssSize(aWidth, aHeight);
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerIsChrome);

  nsIntSize devSz(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(devSz.width, devSz.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindow::ResizeTo(int32_t aWidth, int32_t aHeight, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeToOuter, (aWidth, aHeight, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::ResizeByOuter(int32_t aWidthDif, int32_t aHeightDif,
                              ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is a browser-element then dispatch a resize event to
   * parent.
   */
  if (mDocShell && mDocShell->GetIsMozBrowserOrApp()) {
    CSSIntSize size;
    if (NS_FAILED(GetInnerSize(size))) {
      return;
    }

    size.width += aWidthDif;
    size.height += aHeightDif;

    if (!DispatchResizeEvent(size)) {
      // The embedder chose to prevent the default action for this
      // event, so let's not resize this window after all...
      return;
    }
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerIsChrome) || IsFrame()) {
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

  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerIsChrome);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(newDevSize.width, newDevSize.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindow::ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                         ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeByOuter, (aWidthDif, aHeightDif, aError, nsContentUtils::IsCallerChrome()), aError, );
}

void
nsGlobalWindow::SizeToContentOuter(ErrorResult& aError, bool aCallerIsChrome)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerIsChrome) || IsFrame()) {
    return;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t width, height;
  aError = cv->GetContentSize(&width, &height);
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
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerIsChrome);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwner->SizeShellTo(mDocShell, newDevSize.width,
                                  newDevSize.height);
}

void
nsGlobalWindow::SizeToContent(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SizeToContentOuter, (aError, nsContentUtils::IsCallerChrome()), aError, );
}

already_AddRefed<nsPIWindowRoot>
nsGlobalWindow::GetTopWindowRoot()
{
  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return nullptr;
  }

  nsCOMPtr<nsPIWindowRoot> window = do_QueryInterface(piWin->GetChromeEventHandler());
  return window.forget();
}

void
nsGlobalWindow::Scroll(double aXScroll, double aYScroll)
{
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void
nsGlobalWindow::ScrollTo(double aXScroll, double aYScroll)
{
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void
nsGlobalWindow::ScrollTo(const ScrollToOptions& aOptions)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    CSSIntPoint scrollPos = sf->GetScrollPositionCSSPixels();
    if (aOptions.mLeft.WasPassed()) {
      scrollPos.x = mozilla::ToZeroIfNonfinite(aOptions.mLeft.Value());
    }
    if (aOptions.mTop.WasPassed()) {
      scrollPos.y = mozilla::ToZeroIfNonfinite(aOptions.mTop.Value());
    }

    ScrollTo(scrollPos, aOptions);
  }
}

void
nsGlobalWindow::Scroll(const ScrollToOptions& aOptions)
{
  ScrollTo(aOptions);
}

void
nsGlobalWindow::ScrollTo(const CSSIntPoint& aScroll,
                         const ScrollOptions& aOptions)
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

    bool smoothScroll = sf->GetScrollbarStyles().IsSmoothScroll(aOptions.mBehavior);

    sf->ScrollToCSSPixels(scroll, smoothScroll
                            ? nsIScrollableFrame::SMOOTH_MSD
                            : nsIScrollableFrame::INSTANT);
  }
}

void
nsGlobalWindow::ScrollBy(double aXScrollDif, double aYScrollDif)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
    auto scrollDif = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScrollDif),
                                           mozilla::ToZeroIfNonfinite(aYScrollDif));
    // It seems like it would make more sense for ScrollBy to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    ScrollTo(sf->GetScrollPositionCSSPixels() + scrollDif, ScrollOptions());
  }
}

void
nsGlobalWindow::ScrollBy(const ScrollToOptions& aOptions)
{
  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();

  if (sf) {
    CSSIntPoint scrollPos = sf->GetScrollPositionCSSPixels();
    if (aOptions.mLeft.WasPassed()) {
      scrollPos.x += mozilla::ToZeroIfNonfinite(aOptions.mLeft.Value());
    }
    if (aOptions.mTop.WasPassed()) {
      scrollPos.y += mozilla::ToZeroIfNonfinite(aOptions.mTop.Value());
    }

    ScrollTo(scrollPos, aOptions);
  }
}

void
nsGlobalWindow::ScrollByLines(int32_t numLines,
                              const ScrollOptions& aOptions)
{
  MOZ_ASSERT(IsInnerWindow());

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByLines to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    bool smoothScroll = sf->GetScrollbarStyles().IsSmoothScroll(aOptions.mBehavior);

    sf->ScrollBy(nsIntPoint(0, numLines), nsIScrollableFrame::LINES,
                 smoothScroll
                   ? nsIScrollableFrame::SMOOTH_MSD
                   : nsIScrollableFrame::INSTANT);
  }
}

void
nsGlobalWindow::ScrollByPages(int32_t numPages,
                              const ScrollOptions& aOptions)
{
  MOZ_ASSERT(IsInnerWindow());

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByPages to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    bool smoothScroll = sf->GetScrollbarStyles().IsSmoothScroll(aOptions.mBehavior);

    sf->ScrollBy(nsIntPoint(0, numPages), nsIScrollableFrame::PAGES,
                 smoothScroll
                   ? nsIScrollableFrame::SMOOTH_MSD
                   : nsIScrollableFrame::INSTANT);
  }
}

void
nsGlobalWindow::MozScrollSnap()
{
  MOZ_ASSERT(IsInnerWindow());

  FlushPendingNotifications(Flush_Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    sf->ScrollSnap();
  }
}

void
nsGlobalWindow::MozRequestOverfill(OverfillCallback& aCallback,
                                   mozilla::ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  nsIWidget* widget = nsContentUtils::WidgetForDocument(mDoc);
  if (widget) {
    mozilla::layers::LayerManager* manager = widget->GetLayerManager();
    if (manager) {
      manager->RequestOverfill(&aCallback);
      return;
    }
  }

  aError.Throw(NS_ERROR_NOT_AVAILABLE);
}

void
nsGlobalWindow::ClearTimeout(int32_t aHandle)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (aHandle > 0) {
    ClearTimeoutOrInterval(aHandle);
  }
}

void
nsGlobalWindow::ClearInterval(int32_t aHandle)
{
  if (aHandle > 0) {
    ClearTimeoutOrInterval(aHandle);
  }
}

void
nsGlobalWindow::SetResizable(bool aResizable) const
{
  // nop
}

void
nsGlobalWindow::CaptureEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfCaptureEvents);
  }
}

void
nsGlobalWindow::ReleaseEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfReleaseEvents);
  }
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

void
nsGlobalWindow::FirePopupBlockedEvent(nsIDocument* aDoc,
                                      nsIURI* aPopupURI,
                                      const nsAString& aPopupWindowName,
                                      const nsAString& aPopupWindowFeatures)
{
  MOZ_ASSERT(aDoc);

  // Fire a "DOMPopupBlocked" event so that the UI can hear about
  // blocked popups.
  PopupBlockedEventInit init;
  init.mBubbles = true;
  init.mCancelable = true;
  init.mRequestingWindow = this;
  init.mPopupWindowURI = aPopupURI;
  init.mPopupWindowName = aPopupWindowName;
  init.mPopupWindowFeatures = aPopupWindowFeatures;

  RefPtr<PopupBlockedEvent> event =
    PopupBlockedEvent::Constructor(aDoc,
                                   NS_LITERAL_STRING("DOMPopupBlocked"),
                                   init);

  event->SetTrusted(true);

  bool defaultActionEnabled;
  aDoc->DispatchEvent(event, &defaultActionEnabled);
}

// static
bool
nsGlobalWindow::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
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

  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();
  if (parent == AsOuter())
  {
    return false;
  }

  return nsGlobalWindow::Cast(parent)->PopupWhitelisted();
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
    break;
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

/* If a window open is blocked, fire the appropriate DOM events. */
void
nsGlobalWindow::FireAbuseEvents(const nsAString &aPopupURL,
                                const nsAString &aPopupWindowName,
                                const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsPIDOMWindowOuter> window = GetTop();
  if (!window) {
    return;
  }

  nsCOMPtr<nsIDocument> topDoc = window->GetDoc();
  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = nullptr;

  nsCOMPtr<nsIDocument> doc = GetEntryDocument();
  if (doc)
    baseURL = doc->GetDocBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  FirePopupBlockedEvent(topDoc, popupURI, aPopupWindowName,
                        aPopupWindowFeatures);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::OpenOuter(const nsAString& aUrl, const nsAString& aName,
                          const nsAString& aOptions, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  nsCOMPtr<nsPIDOMWindowOuter> window;
  aError = OpenJS(aUrl, aName, aOptions, getter_AddRefs(window));
  return window.forget();
}


already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(OpenOuter, (aUrl, aName, aOptions, aError), aError,
                            nullptr);
}

nsresult
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, nsPIDOMWindowOuter **_retval)
{
  FORWARD_TO_OUTER(Open, (aUrl, aName, aOptions, _retval),
                   NS_ERROR_NOT_INITIALIZED);
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      _retval);
}

nsresult
nsGlobalWindow::OpenJS(const nsAString& aUrl, const nsAString& aName,
                       const nsAString& aOptions, nsPIDOMWindowOuter **_retval)
{
  MOZ_ASSERT(IsOuterWindow());
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      false,          // aCalledNoScript
                      true,           // aDoJSFixups
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      _retval);
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
nsresult
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions,
                           nsISupports* aExtraArgument,
                           nsPIDOMWindowOuter** _retval)
{
  MOZ_ASSERT(IsOuterWindow());
  return OpenInternal(aUrl, aName, aOptions,
                      true,                    // aDialog
                      false,                   // aContentModal
                      true,                    // aCalledNoScript
                      false,                   // aDoJSFixups
                      true,                    // aNavigate
                      nullptr, aExtraArgument,    // Arguments
                      _retval);
}

// Like Open, but passes aNavigate=false.
/* virtual */ nsresult
nsGlobalWindow::OpenNoNavigate(const nsAString& aUrl,
                               const nsAString& aName,
                               const nsAString& aOptions,
                               nsPIDOMWindowOuter **_retval)
{
  MOZ_ASSERT(IsOuterWindow());
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      false,          // aNavigate
                      nullptr, nullptr,  // No args
                      _retval);

}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::OpenDialogOuter(JSContext* aCx, const nsAString& aUrl,
                                const nsAString& aName, const nsAString& aOptions,
                                const Sequence<JS::Value>& aExtraArgument,
                                ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIJSArgArray> argvArray;
  aError = NS_CreateJSArgv(aCx, aExtraArgument.Length(),
                           aExtraArgument.Elements(),
                           getter_AddRefs(argvArray));
  if (aError.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> dialog;
  aError = OpenInternal(aUrl, aName, aOptions,
                        true,             // aDialog
                        false,            // aContentModal
                        false,            // aCalledNoScript
                        false,            // aDoJSFixups
                        true,                // aNavigate
                        argvArray, nullptr,  // Arguments
                        getter_AddRefs(dialog));
  return dialog.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::OpenDialog(JSContext* aCx, const nsAString& aUrl,
                           const nsAString& aName, const nsAString& aOptions,
                           const Sequence<JS::Value>& aExtraArgument,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(OpenDialogOuter,
                            (aCx, aUrl, aName, aOptions, aExtraArgument, aError),
                            aError, nullptr);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetFramesOuter()
{
  RefPtr<nsPIDOMWindowOuter> frames(AsOuter());
  FlushPendingNotifications(Flush_ContentAndNotify);
  return frames.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetFrames(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFramesOuter, (), aError, nullptr);
}

nsGlobalWindow*
nsGlobalWindow::CallerInnerWindow()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  NS_ENSURE_TRUE(cx, nullptr);
  nsIGlobalObject* global = GetIncumbentGlobal();
  NS_ENSURE_TRUE(global, nullptr);
  JS::Rooted<JSObject*> scope(cx, global->GetGlobalJSObject());
  NS_ENSURE_TRUE(scope, nullptr);

  // When Jetpack runs content scripts inside a sandbox, it uses
  // sandboxPrototype to make them appear as though they're running in the
  // scope of the page. So when a content script invokes postMessage, it expects
  // the |source| of the received message to be the window set as the
  // sandboxPrototype. This used to work incidentally for unrelated reasons, but
  // now we need to do some special handling to support it.
  if (xpc::IsSandbox(scope)) {
    JSAutoCompartment ac(cx, scope);
    JS::Rooted<JSObject*> scopeProto(cx);
    bool ok = JS_GetPrototype(cx, scope, &scopeProto);
    NS_ENSURE_TRUE(ok, nullptr);
    if (scopeProto && xpc::IsSandboxPrototypeProxy(scopeProto) &&
        (scopeProto = js::CheckedUnwrap(scopeProto, /* stopAtWindowProxy = */ false)))
    {
      global = xpc::NativeGlobal(scopeProto);
      NS_ENSURE_TRUE(global, nullptr);
    }
  }

  // The calling window must be holding a reference, so we can return a weak
  // pointer.
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
  return nsGlobalWindow::Cast(win);
}

void
nsGlobalWindow::PostMessageMozOuter(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                                    const nsAString& aTargetOrigin,
                                    JS::Handle<JS::Value> aTransfer,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  //
  // Window.postMessage is an intentional subversion of the same-origin policy.
  // As such, this code must be particularly careful in the information it
  // exposes to calling code.
  //
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/section-crossDocumentMessages.html
  //

  // First, get the caller's window
  RefPtr<nsGlobalWindow> callerInnerWin = CallerInnerWindow();
  nsIPrincipal* callerPrin;
  if (callerInnerWin) {
    MOZ_ASSERT(callerInnerWin->IsInnerWindow(),
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
    nsIGlobalObject* global = GetIncumbentGlobal();
    NS_ASSERTION(global, "Why is there no global object?");
    callerPrin = global->PrincipalOrNull();
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
    providedPrincipal = callerPrin;
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

    nsCOMPtr<nsIPrincipal> principal = nsContentUtils::SubjectPrincipal();
    MOZ_ASSERT(principal);

    PrincipalOriginAttributes attrs = BasePrincipal::Cast(principal)->OriginAttributesRef();
    // Create a nsIPrincipal inheriting the app/browser attributes from the
    // caller.
    providedPrincipal = BasePrincipal::CreateCodebasePrincipal(originURI, attrs);
    if (NS_WARN_IF(!providedPrincipal)) {
      return;
    }
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  RefPtr<PostMessageEvent> event =
    new PostMessageEvent(nsContentUtils::IsCallerChrome() || !callerInnerWin
                         ? nullptr
                         : callerInnerWin->GetOuterWindowInternal(),
                         origin,
                         this,
                         providedPrincipal,
                         callerInnerWin
                         ? callerInnerWin->GetDoc()
                         : nullptr,
                         nsContentUtils::IsCallerChrome());

  JS::Rooted<JS::Value> message(aCx, aMessage);
  JS::Rooted<JS::Value> transfer(aCx, aTransfer);

  event->Write(aCx, message, transfer, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  aError = NS_DispatchToCurrentThread(event);
}

void
nsGlobalWindow::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const nsAString& aTargetOrigin,
                               JS::Handle<JS::Value> aTransfer,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PostMessageMozOuter,
                            (aCx, aMessage, aTargetOrigin, aTransfer, aError),
                            aError, );
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

class nsCloseEvent : public Runnable {

  RefPtr<nsGlobalWindow> mWindow;
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

  NS_IMETHOD Run() override {
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
  MOZ_ASSERT(IsOuterWindow());

  if (mIsChrome) {
    nsCOMPtr<nsIBrowserDOMWindow> bwin;
    nsIDOMChromeWindow* chromeWin = static_cast<nsGlobalChromeWindow*>(this);
    chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

    bool canClose = true;
    if (bwin && NS_SUCCEEDED(bwin->CanClose(&canClose))) {
      return canClose;
    }
  }

  if (!mDocShell) {
    return true;
  }

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    bool canClose;
    nsresult rv = cv->PermitUnload(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;

    rv = cv->RequestWindowClose(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return false;
  }

  return true;
}

void
nsGlobalWindow::CloseOuter(bool aTrustedCaller)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell || IsInModalState() ||
      (IsFrame() && !mDocShell->GetIsMozBrowserOrApp())) {
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
      !mHadOriginalOpener && !aTrustedCaller) {
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

  if (!DispatchCustomEvent(NS_LITERAL_STRING("DOMWindowClose"))) {
    // Someone chose to prevent the default action for this event, if
    // so, let's not close this window after all...

    mInClose = wasInClose;
    return;
  }

  FinalClose();
}

void
nsGlobalWindow::Close(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(CloseOuter, (nsContentUtils::IsCallerChrome()), aError, );
}

nsresult
nsGlobalWindow::Close()
{
  FORWARD_TO_OUTER(Close, (), NS_ERROR_UNEXPECTED);
  CloseOuter(/* aTrustedCaller = */ true);
  return NS_OK;
}

void
nsGlobalWindow::ForceClose()
{
  MOZ_ASSERT(IsOuterWindow());
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (IsFrame() || !mDocShell) {
    // This may be a frame in a frameset, or a window that's already closed.
    // Ignore such calls.
    return;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return;
  }

  mInClose = true;

  DispatchCustomEvent(NS_LITERAL_STRING("DOMWindowClose"));

  FinalClose();
}

void
nsGlobalWindow::FinalClose()
{
  MOZ_ASSERT(IsOuterWindow());

  // Flag that we were closed.
  mIsClosed = true;

  // If we get here from CloseOuter then it means that the parent process is
  // going to close our window for us. It's just important to set mIsClosed.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return;
  }

  // This stuff is non-sensical but incredibly fragile. The reasons for the
  // behavior here don't make sense today and may not have ever made sense,
  // but various bits of frontend code break when you change them. If you need
  // to fix up this behavior, feel free to. It's a righteous task, but involves
  // wrestling with various download manager tests, frontend code, and possible
  // broken addons. The chrome tests in toolkit/mozapps/downloads are a good
  // testing ground.
  //
  // In particular, if some inner of |win| is the entry global, we must
  // complete _two_ round-trips to the event loop before the call to
  // ReallyCloseWindow. This allows setTimeout handlers that are set after
  // FinalClose() is called to run before the window is torn down.
  nsCOMPtr<nsPIDOMWindowInner> entryWindow =
    do_QueryInterface(GetEntryGlobal());
  bool indirect =
    entryWindow && entryWindow->GetOuterWindow() == this->AsOuter();
  if (NS_FAILED(nsCloseEvent::PostCloseEvent(this, indirect))) {
    ReallyCloseWindow();
  } else {
    mHavePendingClose = true;
  }
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
      nsCOMPtr<nsPIDOMWindowOuter> rootWin =
       rootItem ? rootItem->GetWindow() : nullptr;
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
        if (rootWin == AsOuter() ||
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
  MOZ_ASSERT(IsOuterWindow(), "Modal state is maintained on outer windows");

  // GetScriptableTop, not GetTop, so that EnterModalState works properly with
  // <iframe mozbrowser>.
  nsGlobalWindow* topWin = GetScriptableTopInternal();

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
        RefPtr<nsFrameSelection> frameSelection = activeShell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    }
  }

  // If there are any drag and drop operations in flight, try to end them.
  nsCOMPtr<nsIDragService> ds =
    do_GetService("@mozilla.org/widget/dragservice;1");
  if (ds) {
    ds->EndDragSession(true);
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
    NS_ASSERTION(!topWin->mSuspendedDoc, "Shouldn't have mSuspendedDoc here!");

    topWin->mSuspendedDoc = topDoc;
    if (topDoc) {
      topDoc->SuppressEventHandling(nsIDocument::eAnimationsOnly);
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

  nsCOMPtr<nsIDOMWindowCollection> frames = aWindow->GetFrames();
  if (!frames) {
    return;
  }

  uint32_t length = 0;
  frames->GetLength(&length);

  for (uint32_t i = 0; i < length && aTopWindow->mModalStateDepth == 0; i++) {
    nsCOMPtr<mozIDOMWindowProxy> child;
    frames->Item(i, getter_AddRefs(child));

    if (!child) {
      return;
    }

    auto* childWin = nsGlobalWindow::Cast(child);

    RunPendingTimeoutsRecursive(aTopWindow, childWin);
  }
}

class nsPendingTimeoutRunner : public Runnable
{
public:
  explicit nsPendingTimeoutRunner(nsGlobalWindow* aWindow)
    : mWindow(aWindow)
  {
    NS_ASSERTION(mWindow, "mWindow is null.");
  }

  NS_IMETHOD Run() override
  {
    nsGlobalWindow::RunPendingTimeoutsRecursive(mWindow, mWindow);

    return NS_OK;
  }

private:
  RefPtr<nsGlobalWindow> mWindow;
};

void
nsGlobalWindow::LeaveModalState()
{
  MOZ_ASSERT(IsOuterWindow(), "Modal state is maintained on outer windows");

  nsGlobalWindow* topWin = GetScriptableTopInternal();

  if (!topWin) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");
    return;
  }

  topWin->mModalStateDepth--;

  if (topWin->mModalStateDepth == 0) {
    nsCOMPtr<nsIRunnable> runner = new nsPendingTimeoutRunner(topWin);
    if (NS_FAILED(NS_DispatchToCurrentThread(runner)))
      NS_WARNING("failed to dispatch pending timeout runnable");

    if (topWin->mSuspendedDoc) {
      nsCOMPtr<nsIDocument> currentDoc = topWin->GetExtantDoc();
      topWin->mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(nsIDocument::eAnimationsOnly,
                                                                  currentDoc == topWin->mSuspendedDoc);
      topWin->mSuspendedDoc = nullptr;
    }
  }

  // Remember the time of the last dialog quit.
  nsGlobalWindow *inner = topWin->GetCurrentInnerWindowInternal();
  if (inner)
    inner->mLastDialogQuitTime = TimeStamp::Now();

  if (topWin->mModalStateDepth == 0) {
    RefPtr<Event> event = NS_NewDOMEvent(inner, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("endmodalstate"), true, false);
    event->SetTrusted(true);
    event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
    bool dummy;
    topWin->DispatchEvent(event, &dummy);
  }
}

bool
nsGlobalWindow::IsInModalState()
{
  nsGlobalWindow *topWin = GetScriptableTopInternal();

  if (!topWin) {
    // IsInModalState() getting called w/o a reachable top window is a bit
    // iffy, but valid enough not to make noise about it.  See bug 404828
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

// Try to match compartments that are not web content by matching compartments
// with principals that are either the system principal or an expanded principal.
// This may not return true for all non-web-content compartments.
struct BrowserCompartmentMatcher : public js::CompartmentFilter {
  virtual bool match(JSCompartment* c) const override
  {
    nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(c));
    return nsContentUtils::IsSystemOrExpandedPrincipal(pc);
  }
};


class WindowDestroyedEvent : public Runnable
{
public:
  WindowDestroyedEvent(nsIDOMWindow* aWindow, uint64_t aID,
                       const char* aTopic) :
    mID(aID), mTopic(aTopic)
  {
    mWindow = do_GetWeakReference(aWindow);
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
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

    nsCOMPtr<nsISupports> window = do_QueryReferent(mWindow);
    if (!skipNukeCrossCompartment && window) {
      nsGlobalWindow* win = nsGlobalWindow::FromSupports(window);
      nsGlobalWindow* currentInner = win->IsInnerWindow() ? win : win->GetCurrentInnerWindowInternal();
      NS_ENSURE_TRUE(currentInner, NS_OK);

      AutoSafeJSContext cx;
      JS::Rooted<JSObject*> obj(cx, currentInner->FastGetGlobalJSObject());
      // We only want to nuke wrappers for the chrome->content case
      if (obj && !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
        js::NukeCrossCompartmentWrappers(cx,
                                         BrowserCompartmentMatcher(),
                                         js::SingleCompartment(js::GetObjectCompartment(obj)),
                                         win->IsInnerWindow() ? js::DontNukeWindowReferences
                                                              : js::NukeWindowReferences);
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
  nsCOMPtr<nsIRunnable> runnable = new WindowDestroyedEvent(this, mWindowID, aTopic);
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
  JS::Rooted<JSObject*> handler(RootingCx());
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

Element*
nsGlobalWindow::GetFrameElementOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell || mDocShell->GetIsMozBrowserOrApp()) {
    return nullptr;
  }

  // Per HTML5, the frameElement getter returns null in cross-origin situations.
  Element* element = GetRealFrameElementOuter();
  if (!element) {
    return nullptr;
  }

  if (!nsContentUtils::SubjectPrincipal()->
         SubsumesConsideringDomain(element->NodePrincipal())) {
    return nullptr;
  }

  return element;
}

Element*
nsGlobalWindow::GetFrameElement(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFrameElementOuter, (), aError, nullptr);
}

Element*
nsGlobalWindow::GetRealFrameElementOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

Element*
nsGlobalWindow::GetRealFrameElement(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetRealFrameElementOuter, (), aError, nullptr);
}

/**
 * nsIGlobalWindow::GetFrameElement (when called from C++) is just a wrapper
 * around GetRealFrameElement.
 */
already_AddRefed<nsIDOMElement>
nsGlobalWindow::GetFrameElement()
{
  FORWARD_TO_INNER(GetFrameElement, (), nullptr);

  ErrorResult dummy;
  nsCOMPtr<nsIDOMElement> frameElement =
    do_QueryInterface(GetRealFrameElement(dummy));
  dummy.SuppressException();
  return frameElement.forget();
}

/* static */ bool
nsGlobalWindow::TokenizeDialogOptions(nsAString& aToken,
                                      nsAString::const_iterator& aIter,
                                      nsAString::const_iterator aEnd)
{
  while (aIter != aEnd && nsCRT::IsAsciiSpace(*aIter)) {
    ++aIter;
  }

  if (aIter == aEnd) {
    return false;
  }

  if (*aIter == ';' || *aIter == ':' || *aIter == '=') {
    aToken.Assign(*aIter);
    ++aIter;
    return true;
  }

  nsAString::const_iterator start = aIter;

  // Skip characters until we find whitespace, ';', ':', or '='
  while (aIter != aEnd && !nsCRT::IsAsciiSpace(*aIter) &&
         *aIter != ';' &&
         *aIter != ':' &&
         *aIter != '=') {
    ++aIter;
  }

  aToken.Assign(Substring(start, aIter));
  return true;
}

// Helper for converting window.showModalDialog() options (list of ';'
// separated name (:|=) value pairs) to a format that's parsable by
// our normal window opening code.

/* static */
void
nsGlobalWindow::ConvertDialogOptions(const nsAString& aOptions,
                                     nsAString& aResult)
{
  nsAString::const_iterator end;
  aOptions.EndReading(end);

  nsAString::const_iterator iter;
  aOptions.BeginReading(iter);

  nsAutoString token;
  nsAutoString name;
  nsAutoString value;

  while (true) {
    if (!TokenizeDialogOptions(name, iter, end)) {
      break;
    }

    // Invalid name.
    if (name.EqualsLiteral("=") ||
        name.EqualsLiteral(":") ||
        name.EqualsLiteral(";")) {
      break;
    }

    if (!TokenizeDialogOptions(token, iter, end)) {
      break;
    }

    if (!token.EqualsLiteral(":") && !token.EqualsLiteral("=")) {
      continue;
    }

    // We found name followed by ':' or '='. Look for a value.
    if (!TokenizeDialogOptions(value, iter, end)) {
      break;
    }

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

    if (iter == end ||
        !TokenizeDialogOptions(token, iter, end) ||
        !token.EqualsLiteral(";")) {
      break;
    }
  }
}

already_AddRefed<nsIVariant>
nsGlobalWindow::ShowModalDialogOuter(const nsAString& aUrl, nsIVariant* aArgument,
                                     const nsAString& aOptions, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eShowModalDialog);
  }

  if (!IsShowModalDialogEnabled()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  RefPtr<DialogValueHolder> argHolder =
    new DialogValueHolder(nsContentUtils::SubjectPrincipal(), aArgument);

  // Before bringing up the window/dialog, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  if (!AreDialogsEnabled()) {
    // We probably want to keep throwing here; silently doing nothing is a bit
    // weird given the typical use cases of showModalDialog().
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (ShouldPromptToBlockDialogs() && !ConfirmDialogIfNeeded()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> dlgWin;
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

already_AddRefed<nsIVariant>
nsGlobalWindow::ShowModalDialog(const nsAString& aUrl, nsIVariant* aArgument,
                                const nsAString& aOptions, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ShowModalDialogOuter,
                            (aUrl, aArgument, aOptions, aError), aError,
                            nullptr);
}

void
nsGlobalWindow::ShowModalDialog(JSContext* aCx, const nsAString& aUrl,
                                JS::Handle<JS::Value> aArgument,
                                const nsAString& aOptions,
                                JS::MutableHandle<JS::Value> aRetval,
                                ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIVariant> args;
  aError = nsContentUtils::XPConnect()->JSToVariant(aCx,
                                                    aArgument,
                                                    getter_AddRefs(args));
  if (aError.Failed()) {
    return;
  }

  nsCOMPtr<nsIVariant> retVal = ShowModalDialog(aUrl, args, aOptions, aError);
  if (aError.Failed()) {
    return;
  }

  JS::Rooted<JS::Value> result(aCx);
  if (retVal) {
    aError = nsContentUtils::XPConnect()->VariantToJS(aCx,
                                                      FastGetGlobalJSObject(),
                                                      retVal, aRetval);
  } else {
    aRetval.setNull();
  }
}

class ChildCommandDispatcher : public Runnable
{
public:
  ChildCommandDispatcher(nsGlobalWindow* aWindow,
                         nsITabChild* aTabChild,
                         const nsAString& aAction)
  : mWindow(aWindow), mTabChild(aTabChild), mAction(aAction) {}

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsPIWindowRoot> root = mWindow->GetTopWindowRoot();
    if (!root) {
      return NS_OK;
    }

    nsTArray<nsCString> enabledCommands, disabledCommands;
    root->GetEnabledDisabledCommands(enabledCommands, disabledCommands);
    if (enabledCommands.Length() || disabledCommands.Length()) {
      mTabChild->EnableDisableCommands(mAction, enabledCommands, disabledCommands);
    }

    return NS_OK;
  }

private:
  RefPtr<nsGlobalWindow>             mWindow;
  nsCOMPtr<nsITabChild>                mTabChild;
  nsString                             mAction;
};

class CommandDispatcher : public Runnable
{
public:
  CommandDispatcher(nsIDOMXULCommandDispatcher* aDispatcher,
                    const nsAString& aAction)
  : mDispatcher(aDispatcher), mAction(aAction) {}

  NS_IMETHOD Run() override
  {
    return mDispatcher->UpdateCommands(mAction);
  }

  nsCOMPtr<nsIDOMXULCommandDispatcher> mDispatcher;
  nsString                             mAction;
};

nsresult
nsGlobalWindow::UpdateCommands(const nsAString& anAction, nsISelection* aSel, int16_t aReason)
{
  // If this is a child process, redirect to the parent process.
  if (nsIDocShell* docShell = GetDocShell()) {
    if (nsCOMPtr<nsITabChild> child = docShell->GetTabChild()) {
      nsContentUtils::AddScriptRunner(new ChildCommandDispatcher(this, child,
                                                                 anAction));
      return NS_OK;
    }
  }

  nsPIDOMWindowOuter *rootWindow = nsGlobalWindow::GetPrivateRoot();
  if (!rootWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(rootWindow->GetExtantDoc());
  // See if we contain a XUL document.
  // selectionchange action is only used for mozbrowser, not for XUL. So we bypass
  // XUL command dispatch if anAction is "selectionchange".
  if (xulDoc && !anAction.EqualsLiteral("selectionchange")) {
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
nsGlobalWindow::GetSelectionOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsISelection* domSelection =
    presShell->GetCurrentSelection(SelectionType::eNormal);
  return domSelection ? domSelection->AsSelection() : nullptr;
}

Selection*
nsGlobalWindow::GetSelection(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetSelectionOuter, (), aError, nullptr);
}

already_AddRefed<nsISelection>
nsGlobalWindow::GetSelection()
{
  nsCOMPtr<nsISelection> selection = GetSelectionOuter();
  return selection.forget();
}

bool
nsGlobalWindow::FindOuter(const nsAString& aString, bool aCaseSensitive,
                          bool aBackwards, bool aWrapAround, bool aWholeWord,
                          bool aSearchInFrames, bool aShowDialog,
                          ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (Preferences::GetBool("dom.disable_window_find", false)) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return false;
  }

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
    framesFinder->SetRootSearchFrame(AsOuter());   // paranoia
    framesFinder->SetCurrentSearchFrame(AsOuter());
  }

  // The Find API does not accept empty strings. Launch the Find Dialog.
  if (aString.IsEmpty() || aShowDialog) {
    // See if the find dialog is already up using nsIWindowMediator
    nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

    nsCOMPtr<mozIDOMWindowProxy> findDialog;

    if (windowMediator) {
      windowMediator->GetMostRecentWindow(u"findInPage",
                                          getter_AddRefs(findDialog));
    }

    if (findDialog) {
      // The Find dialog is already open, bring it to the top.
      auto* piFindDialog = nsPIDOMWindowOuter::From(findDialog);
      aError = piFindDialog->Focus();
    } else if (finder) {
      // Open a Find dialog
      nsCOMPtr<nsPIDOMWindowOuter> dialog;
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

bool
nsGlobalWindow::Find(const nsAString& aString, bool aCaseSensitive,
                     bool aBackwards, bool aWrapAround, bool aWholeWord,
                     bool aSearchInFrames, bool aShowDialog,
                     ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(FindOuter,
                            (aString, aCaseSensitive, aBackwards, aWrapAround,
                             aWholeWord, aSearchInFrames, aShowDialog, aError),
                            aError, false);
}

void
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData, ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  aError = nsContentUtils::Atob(aAsciiBase64String, aBinaryData);
}

void
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String, ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  aError = nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMEventTarget
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindow::GetOwnerGlobalForBindings()
{
  if (IsOuterWindow()) {
    return AsOuter();
  }

  return nsPIDOMWindowOuter::GetFromCurrentInner(AsInner());
}

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    bool aUseCapture)
{
  if (RefPtr<EventListenerManager> elm = GetExistingListenerManager()) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsGlobalWindow)

NS_IMETHODIMP
nsGlobalWindow::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, aRetVal), NS_OK);

  if (!AsInner()->IsCurrentInnerWindow()) {
    NS_WARNING("DispatchEvent called on non-current inner window, dropping. "
               "Please check the window in the caller instead.");
    return NS_ERROR_FAILURE;
  }

  if (!mDoc) {
    return NS_ERROR_FAILURE;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = mDoc->GetShell();
  RefPtr<nsPresContext> presContext;
  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(AsInner(), nullptr, aEvent,
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
                                 const AddEventListenerOptionsOrBoolean& aOptions,
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

  manager->AddEventListener(aType, aListener, aOptions, wantsUntrusted);
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
      !nsContentUtils::LegacyIsCallerNativeCode() &&
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
  NS_ENSURE_TRUE(!IsInnerWindow() || AsInner()->IsCurrentInnerWindow(), nullptr);

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

nsPIDOMWindowOuter*
nsGlobalWindow::GetPrivateParent()
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();

  if (AsOuter() == parent) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nullptr;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetComposedDoc();
    if (!doc)
      return nullptr;             // This is ok, just means a null parent.

    return doc->GetWindow();
  }

  return parent;
}

nsPIDOMWindowOuter*
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

  nsCOMPtr<nsPIDOMWindowOuter> top = GetTop();

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetComposedDoc();
    if (doc) {
      nsCOMPtr<nsPIDOMWindowOuter> parent = doc->GetWindow();
      if (parent) {
        top = parent->GetTop();
      }
    }
  }

  return top;
}

nsLocation*
nsGlobalWindow::GetLocation(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  nsIDocShell *docShell = GetDocShell();
  if (!mLocation && docShell) {
    mLocation = new nsLocation(AsInner(), docShell);
  }
  return mLocation;
}

nsIDOMLocation*
nsGlobalWindow::GetLocation()
{
  FORWARD_TO_INNER(GetLocation, (), nullptr);

  ErrorResult dummy;
  nsIDOMLocation* location = GetLocation(dummy);
  dummy.SuppressException();
  return location;
}

void
nsGlobalWindow::ActivateOrDeactivate(bool aActivate)
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mDoc) {
    return;
  }

  // Set / unset mIsActive on the top level window, which is used for the
  // :-moz-window-inactive pseudoclass, and its sheet (if any).
  nsCOMPtr<nsIWidget> mainWidget = GetMainWidget();
  nsCOMPtr<nsIWidget> topLevelWidget;
  if (mainWidget) {
    // Get the top level widget (if the main widget is a sheet, this will
    // be the sheet's top (non-sheet) parent).
    topLevelWidget = mainWidget->GetSheetWindowParent();
    if (!topLevelWidget) {
      topLevelWidget = mainWidget;
    }
  }

  SetActive(aActivate);

  if (mainWidget != topLevelWidget) {
    // This is a workaround for the following problem:
    // When a window with an open sheet gains or loses focus, only the sheet
    // window receives the NS_ACTIVATE/NS_DEACTIVATE event.  However the
    // styling of the containing top level window also needs to change.  We
    // get around this by calling nsPIDOMWindow::SetActive() on both windows.

    // Get the top level widget's nsGlobalWindow
    nsCOMPtr<nsPIDOMWindowOuter> topLevelWindow;

    // widgetListener should be a nsXULWindow
    nsIWidgetListener* listener = topLevelWidget->GetWidgetListener();
    if (listener) {
      nsCOMPtr<nsIXULWindow> window = listener->GetXULWindow();
      nsCOMPtr<nsIInterfaceRequestor> req(do_QueryInterface(window));
      topLevelWindow = do_GetInterface(req);
    }

    if (topLevelWindow) {
      topLevelWindow->SetActive(aActivate);
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

bool
nsGlobalWindow::IsTopLevelWindowActive()
{
   nsCOMPtr<nsIDocShellTreeItem> treeItem(GetDocShell());
   if (!treeItem) {
     return false;
   }

   nsCOMPtr<nsIDocShellTreeItem> rootItem;
   treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
   if (!rootItem) {
     return false;
   }

   nsCOMPtr<nsPIDOMWindowOuter> domWindow = rootItem->GetWindow();
   return domWindow && domWindow->IsActive();
}

void nsGlobalWindow::SetIsBackground(bool aIsBackground)
{
  MOZ_ASSERT(IsOuterWindow());

  bool resetTimers = (!aIsBackground && AsOuter()->IsBackground());
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

void
nsGlobalWindow::EnableGamepadUpdates()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mHasGamepad) {
#ifdef MOZ_GAMEPAD
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->AddListener(this);
    }
#endif
  }
}

void
nsGlobalWindow::DisableGamepadUpdates()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mHasGamepad) {
#ifdef MOZ_GAMEPAD
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->RemoveListener(this);
    }
#endif
  }
}

void
nsGlobalWindow::EnableVRUpdates()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mHasVREvents && !mVREventObserver) {
    mVREventObserver = new VREventObserver(this);
  }
}

void
nsGlobalWindow::DisableVRUpdates()
{
  MOZ_ASSERT(IsInnerWindow());

  mVREventObserver = nullptr;
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
    NS_ASSERTION(!inner->mOuterWindow || inner->mOuterWindow == AsOuter(),
                 "bad outer window pointer");
    inner->SetChromeEventHandlerInternal(aChromeEventHandler);
  }
}

static bool IsLink(nsIContent* aContent)
{
  return aContent && (aContent->IsHTMLElement(nsGkAtoms::a) ||
                      aContent->AttrValueIs(kNameSpaceID_XLink, nsGkAtoms::type,
                                            nsGkAtoms::simple, eCaseMatters));
}

static bool ShouldShowFocusRingIfFocusedByMouse(nsIContent* aNode)
{
  if (!aNode) {
    return true;
  }
  return !IsLink(aNode) &&
         !aNode->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio);
}

void
nsGlobalWindow::SetFocusedNode(nsIContent* aNode,
                               uint32_t aFocusMethod,
                               bool aNeedsFocus)
{
  FORWARD_TO_INNER_VOID(SetFocusedNode, (aNode, aFocusMethod, aNeedsFocus));

  if (aNode && aNode->GetComposedDoc() != mDoc) {
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
      // are only visible on some elements.
#ifndef XP_WIN
      !(mFocusMethod & nsIFocusManager::FLAG_BYMOUSE) ||
      ShouldShowFocusRingIfFocusedByMouse(aNode) ||
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

  if (mShowFocusRingForContent || mFocusByKeyOccurred) {
    return true;
  }

  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return root ? root->ShowFocusRings() : false;
}

void
nsGlobalWindow::SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                      UIStateChangeType aShowFocusRings)
{
  MOZ_ASSERT(IsOuterWindow());

  nsPIDOMWindowOuter* piWin = GetPrivateRoot();
  if (!piWin) {
    return;
  }

  MOZ_ASSERT(piWin == AsOuter());

  bool oldShouldShowFocusRing = ShouldShowFocusRing();

  // only change the flags that have been modified
  nsCOMPtr<nsPIWindowRoot> windowRoot = do_QueryInterface(mChromeEventHandler);
  if (!windowRoot) {
    return;
  }

  if (aShowAccelerators != UIStateChangeType_NoChange) {
    windowRoot->SetShowAccelerators(aShowAccelerators == UIStateChangeType_Set);
  }
  if (aShowFocusRings != UIStateChangeType_NoChange) {
    windowRoot->SetShowFocusRings(aShowFocusRings == UIStateChangeType_Set);
  }

  nsContentUtils::SetKeyboardIndicatorsOnRemoteChildren(GetOuterWindow(),
                                                        aShowAccelerators,
                                                        aShowFocusRings);

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

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->WindowShown(GetOuterWindow(), oldNeedsFocus);
  }
}

void
nsGlobalWindow::PageHidden()
{
  FORWARD_TO_INNER_VOID(PageHidden, ());

  // the window is being hidden, so tell the focus manager that the frame is
  // no longer valid. Use the persisted field to determine if the document
  // is being destroyed.

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->WindowHidden(GetOuterWindow());
  }

  mNeedsFocus = true;
}

class HashchangeCallback : public Runnable
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

  NS_IMETHOD Run() override
  {
    NS_PRECONDITION(NS_IsMainThread(), "Should be called on the main thread.");
    return mWindow->FireHashchange(mOldURL, mNewURL);
  }

private:
  nsString mOldURL;
  nsString mNewURL;
  RefPtr<nsGlobalWindow> mWindow;
};

nsresult
nsGlobalWindow::DispatchAsyncHashchange(nsIURI *aOldURI, nsIURI *aNewURI)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  // Make sure that aOldURI and aNewURI are identical up to the '#', and that
  // their hashes are different.
  bool equal = false;
  NS_ENSURE_STATE(NS_SUCCEEDED(aOldURI->EqualsExceptRef(aNewURI, &equal)) && equal);
  nsAutoCString oldHash, newHash;
  bool oldHasHash, newHasHash;
  NS_ENSURE_STATE(NS_SUCCEEDED(aOldURI->GetRef(oldHash)) &&
                  NS_SUCCEEDED(aNewURI->GetRef(newHash)) &&
                  NS_SUCCEEDED(aOldURI->GetHasRef(&oldHasHash)) &&
                  NS_SUCCEEDED(aNewURI->GetHasRef(&newHasHash)) &&
                  (oldHasHash != newHasHash || !oldHash.Equals(newHash)));

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
  if (IsFrozen()) {
    return NS_OK;
  }

  // Get a presentation shell for use in creating the hashchange event.
  NS_ENSURE_STATE(AsInner()->IsCurrentInnerWindow());

  nsIPresShell *shell = mDoc->GetShell();
  RefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  HashChangeEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mNewURL = aNewURL;
  init.mOldURL = aOldURL;

  RefPtr<HashChangeEvent> event =
    HashChangeEvent::Constructor(this, NS_LITERAL_STRING("hashchange"),
                                 init);

  event->SetTrusted(true);

  bool dummy;
  return DispatchEvent(event, &dummy);
}

nsresult
nsGlobalWindow::DispatchSyncPopState()
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be safe to run script here.");

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
  RefPtr<nsPresContext> presContext;
  if (shell) {
    presContext = shell->GetPresContext();
  }

  bool result = true;
  AutoJSAPI jsapi;
  result = jsapi.Init(AsInner());
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> stateJSValue(cx, JS::NullValue());
  result = stateObj ? VariantToJsval(cx, stateObj, &stateJSValue) : true;
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  RootedDictionary<PopStateEventInit> init(cx);
  init.mBubbles = true;
  init.mCancelable = false;
  init.mState = stateJSValue;

  RefPtr<PopStateEvent> event =
    PopStateEvent::Constructor(this, NS_LITERAL_STRING("popstate"),
                               init);
  event->SetTrusted(true);
  event->SetTarget(this);

  bool dummy; // default action
  return DispatchEvent(event, &dummy);
}

// Find an nsICanvasFrame under aFrame.  Only search the principal
// child lists.  aFrame must be non-null.
static nsCanvasFrame* FindCanvasFrame(nsIFrame* aFrame)
{
    nsCanvasFrame* canvasFrame = do_QueryFrame(aFrame);
    if (canvasFrame) {
        return canvasFrame;
    }

    for (nsIFrame* kid : aFrame->PrincipalChildList()) {
        canvasFrame = FindCanvasFrame(kid);
        if (canvasFrame) {
            return canvasFrame;
        }
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
  MOZ_ASSERT(IsInnerWindow());
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aError);
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetDefaultComputedStyle(Element& aElt,
                                        const nsAString& aPseudoElt,
                                        ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  return GetComputedStyleHelper(aElt, aPseudoElt, true, aError);
}

nsresult
nsGlobalWindow::GetComputedStyleHelper(nsIDOMElement* aElt,
                                       const nsAString& aPseudoElt,
                                       bool aDefaultStylesOnly,
                                       nsIDOMCSSStyleDeclaration** aReturn)
{
  MOZ_ASSERT(IsInnerWindow());

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

  return rv.StealNSResult();
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetComputedStyleHelperOuter(Element& aElt,
                                            const nsAString& aPseudoElt,
                                            bool aDefaultStylesOnly)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();

  if (!presShell) {
    // Try flushing frames on our parent in case there's a pending
    // style change that will create the presshell.
    auto* parent = nsGlobalWindow::Cast(GetPrivateParent());
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

  RefPtr<nsComputedDOMStyle> compStyle =
    NS_NewComputedDOMStyle(&aElt, aPseudoElt, presShell,
                           aDefaultStylesOnly ? nsComputedDOMStyle::eDefaultOnly :
                                                nsComputedDOMStyle::eAll);

  return compStyle.forget();
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetComputedStyleHelper(Element& aElt,
                                       const nsAString& aPseudoElt,
                                       bool aDefaultStylesOnly,
                                       ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetComputedStyleHelperOuter,
                            (aElt, aPseudoElt, aDefaultStylesOnly),
                            aError, nullptr);
}

DOMStorage*
nsGlobalWindow::GetSessionStorage(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell || !Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
  }

  if (mSessionStorage) {
    if (MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
      PR_LogPrint("nsGlobalWindow %p has %p sessionStorage", this, mSessionStorage.get());
    }
    bool canAccess = mSessionStorage->CanAccess(principal);
    NS_ASSERTION(canAccess,
                 "This window owned sessionStorage "
                 "that could not be accessed!");
    if (!canAccess) {
      mSessionStorage = nullptr;
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

    nsCOMPtr<nsIDOMStorage> storage;
    aError = storageManager->CreateStorage(AsInner(), principal, documentURI,
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mSessionStorage = static_cast<DOMStorage*>(storage.get());
    MOZ_ASSERT(mSessionStorage);

    if (MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
      PR_LogPrint("nsGlobalWindow %p tried to get a new sessionStorage %p", this, mSessionStorage.get());
    }

    if (!mSessionStorage) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  if (MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
    PR_LogPrint("nsGlobalWindow %p returns %p sessionStorage", this, mSessionStorage.get());
  }

  return mSessionStorage;
}

DOMStorage*
nsGlobalWindow::GetLocalStorage(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
  }

  if (!mLocalStorage) {
    if (!DOMStorage::CanUseStorage(AsInner())) {
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

    nsString documentURI;
    if (mDoc) {
      mDoc->GetDocumentURI(documentURI);
    }

    nsCOMPtr<nsIDOMStorage> storage;
    aError = storageManager->CreateStorage(AsInner(), principal, documentURI,
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mLocalStorage = static_cast<DOMStorage*>(storage.get());
    MOZ_ASSERT(mLocalStorage);
  }

  return mLocalStorage;
}

IDBFactory*
nsGlobalWindow::GetIndexedDB(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());
  if (!mIndexedDB) {
    // This may keep mIndexedDB null without setting an error.
    aError = IDBFactory::CreateForWindow(AsInner(),
                                         getter_AddRefs(mIndexedDB));
  }

  return mIndexedDB;
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

void
nsGlobalWindow::GetInterface(JSContext* aCx, nsIJSID* aIID,
                             JS::MutableHandle<JS::Value> aRetval,
                             ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  dom::GetInterface(aCx, this, aIID, aRetval, aError);
}

already_AddRefed<CacheStorage>
nsGlobalWindow::GetCaches(ErrorResult& aRv)
{
  MOZ_ASSERT(IsInnerWindow());

  if (!mCacheStorage) {
    bool forceTrustedOrigin =
      GetOuterWindow()->GetServiceWorkersTestingEnabled();

    nsContentUtils::StorageAccess access =
      nsContentUtils::StorageAllowedForWindow(AsInner());

    // We don't block the cache API when being told to only allow storage for the
    // current session.
    bool storageBlocked = access <= nsContentUtils::StorageAccess::ePrivateBrowsing;

    mCacheStorage = CacheStorage::CreateOnMainThread(cache::DEFAULT_NAMESPACE,
                                                     this, GetPrincipal(),
                                                     storageBlocked,
                                                     forceTrustedOrigin, aRv);
  }

  RefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
}

already_AddRefed<ServiceWorkerRegistration>
nsPIDOMWindowInner::GetServiceWorkerRegistration(const nsAString& aScope)
{
  RefPtr<ServiceWorkerRegistration> registration;
  if (!mServiceWorkerRegistrationTable.Get(aScope,
                                           getter_AddRefs(registration))) {
    registration =
      ServiceWorkerRegistration::CreateForMainThread(this, aScope);
    mServiceWorkerRegistrationTable.Put(aScope, registration);
  }
  return registration.forget();
}

void
nsPIDOMWindowInner::InvalidateServiceWorkerRegistration(const nsAString& aScope)
{
  mServiceWorkerRegistrationTable.Remove(aScope);
}

void
nsGlobalWindow::FireOfflineStatusEventIfChanged()
{
  if (!AsInner()->IsCurrentInnerWindow())
    return;

  bool isOffline = NS_IsOffline() || NS_IsAppOffline(GetPrincipal());

  // Don't fire an event if the status hasn't changed
  if (mWasOffline == isOffline) {
    return;
  }

  mWasOffline = isOffline;

  nsAutoString name;
  if (isOffline) {
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

class NotifyIdleObserverRunnable : public Runnable
{
public:
  NotifyIdleObserverRunnable(nsIIdleObserver* aIdleObserver,
                             uint32_t aTimeInS,
                             bool aCallOnidle,
                             nsGlobalWindow* aIdleWindow)
    : mIdleObserver(aIdleObserver), mTimeInS(aTimeInS), mIdleWindow(aIdleWindow),
      mCallOnidle(aCallOnidle)
  { }

  NS_IMETHOD Run() override
  {
    if (mIdleWindow->ContainsIdleObserver(mIdleObserver, mTimeInS)) {
      return mCallOnidle ? mIdleObserver->Onidle() : mIdleObserver->Onactive();
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIIdleObserver> mIdleObserver;
  uint32_t mTimeInS;
  RefPtr<nsGlobalWindow> mIdleWindow;

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
  RefPtr<nsGlobalWindow> idleWindow = static_cast<nsGlobalWindow*>(aClosure);
  MOZ_ASSERT(idleWindow, "Idle window has not been instantiated.");
  idleWindow->HandleIdleActiveEvent();
}

void
IdleObserverTimerCallback(nsITimer* aTimer, void* aClosure)
{
  RefPtr<nsGlobalWindow> idleWindow = static_cast<nsGlobalWindow*>(aClosure);
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

  if (Preferences::GetBool("dom.always_stop_slow_scripts")) {
    return KillSlowScript;
  }

  // If it isn't safe to run script, then it isn't safe to bring up the prompt
  // (since that spins the event loop). In that (rare) case, we just kill the
  // script and report a warning.
  if (!nsContentUtils::IsSafeToRunScript()) {
    JS_ReportWarning(cx, "A long running script was terminated");
    return KillSlowScript;
  }

  // If our document is not active, just kill the script: we've been unloaded
  if (!AsInner()->HasActiveDocument()) {
    return KillSlowScript;
  }

  // Check if we should offer the option to debug
  JS::AutoFilename filename;
  unsigned lineno;
  bool hasFrame = JS::DescribeScriptedCaller(cx, &filename, &lineno);

  // Record the slow script event if we haven't done so already for this inner window
  // (which represents a particular page to the user).
  if (!mHasHadSlowScript) {
    Telemetry::Accumulate(Telemetry::SLOW_SCRIPT_PAGE_COUNT, 1);
  }
  mHasHadSlowScript = true;

  if (XRE_IsContentProcess() &&
      ProcessHangMonitor::Get()) {
    ProcessHangMonitor::SlowScriptAction action;
    RefPtr<ProcessHangMonitor> monitor = ProcessHangMonitor::Get();
    nsIDocShell* docShell = GetDocShell();
    nsCOMPtr<nsITabChild> child = docShell ? docShell->GetTabChild() : nullptr;
    action = monitor->NotifySlowScript(child,
                                       filename.get(),
                                       lineno);
    if (action == ProcessHangMonitor::Terminate) {
      return KillSlowScript;
    }

    if (action == ProcessHangMonitor::StartDebugger) {
      // Spin a nested event loop so that the debugger in the parent can fetch
      // any information it needs. Once the debugger has started, return to the
      // script.
      RefPtr<nsGlobalWindow> outer = GetOuterWindowInternal();
      outer->EnterModalState();
      while (!monitor->IsDebuggerStartupComplete()) {
        NS_ProcessNextEvent(nullptr, true);
      }
      outer->LeaveModalState();
      return ContinueSlowScript;
    }

    return ContinueSlowScriptAndKeepNotifying;
  }

  // Reached only on non-e10s - once per slow script dialog.
  // On e10s - we probe once at ProcessHangsMonitor.jsm
  Telemetry::Accumulate(Telemetry::SLOW_SCRIPT_NOTICE_COUNT, 1);

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIDocShell> ds = GetDocShell();
  NS_ENSURE_TRUE(ds, KillSlowScript);
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ds);
  NS_ENSURE_TRUE(prompt, KillSlowScript);

  // Prioritize the SlowScriptDebug interface over JSD1.
  nsCOMPtr<nsISlowScriptDebugCallback> debugCallback;

  if (hasFrame) {
    const char *debugCID = "@mozilla.org/dom/slow-script-debug;1";
    nsCOMPtr<nsISlowScriptDebug> debugService = do_GetService(debugCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      debugService->GetActivationHandler(getter_AddRefs(debugCallback));
    }
  }

  bool showDebugButton = !!debugCallback;

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


  if (showDebugButton) {
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
      (!debugButton && showDebugButton) || !neverShowDlg) {
    NS_ERROR("Failed to get localized strings.");
    return ContinueSlowScript;
  }

  // Append file and line number information, if available
  if (filename.get()) {
    nsXPIDLString scriptLocation;
    // We want to drop the middle part of too-long locations.  We'll
    // define "too-long" as longer than 60 UTF-16 code units.  Just
    // have to be a bit careful about unpaired surrogates.
    NS_ConvertUTF8toUTF16 filenameUTF16(filename.get());
    if (filenameUTF16.Length() > 60) {
      // XXXbz Do we need to insert any bidi overrides here?
      size_t cutStart = 30;
      size_t cutLength = filenameUTF16.Length() - 60;
      MOZ_ASSERT(cutLength > 0);
      if (NS_IS_LOW_SURROGATE(filenameUTF16[cutStart])) {
        // Don't truncate before the low surrogate, in case it's preceded by a
        // high surrogate and forms a single Unicode character.  Instead, just
        // include the low surrogate.
        ++cutStart;
        --cutLength;
      }
      if (NS_IS_LOW_SURROGATE(filenameUTF16[cutStart + cutLength])) {
        // Likewise, don't drop a trailing low surrogate here.  We want to
        // increase cutLength, since it might be 0 already so we can't very well
        // decrease it.
        ++cutLength;
      }

      // Insert U+2026 HORIZONTAL ELLIPSIS
      filenameUTF16.Replace(cutStart, cutLength, NS_LITERAL_STRING(u"\x2026"));
    }
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
  if (showDebugButton)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  // Null out the operation callback while we're re-entering JS here.
  JSInterruptCallback old = JS_SetInterruptCallback(cx, nullptr);

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, waitButton, stopButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  JS_SetInterruptCallback(cx, old);

  if (NS_SUCCEEDED(rv) && (buttonPressed == 0)) {
    return neverShowDlgChk ? AlwaysContinueSlowScript : ContinueSlowScript;
  }
  if (buttonPressed == 2) {
    if (debugCallback) {
      rv = debugCallback->HandleSlowScriptDebug(this);
      return NS_SUCCEEDED(rv) ? ContinueSlowScript : KillSlowScript;
    }
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
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) ||
      !nsCRT::strcmp(aTopic, NS_IOSERVICE_APP_OFFLINE_STATUS_TOPIC)) {
    if (!IsFrozen()) {
        // Fires an offline status event if the offline status has changed
        FireOfflineStatusEventIfChanged();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    mCurrentlyIdle = true;
    if (IsFrozen()) {
      // need to fire only one idle event while the window is frozen.
      mNotifyIdleObserversIdleOnThaw = true;
      mNotifyIdleObserversActiveOnThaw = false;
    } else if (AsInner()->IsCurrentInnerWindow()) {
      HandleIdleActiveEvent();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_ACTIVE)) {
    mCurrentlyIdle = false;
    if (IsFrozen()) {
      mNotifyIdleObserversActiveOnThaw = true;
      mNotifyIdleObserversIdleOnThaw = false;
    } else if (AsInner()->IsCurrentInnerWindow()) {
      MOZ_ASSERT(IsInnerWindow());
      ScheduleActiveTimerCallback();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "dom-storage2-changed")) {
    if (!IsInnerWindow() || !AsInner()->IsCurrentInnerWindow()) {
      return NS_OK;
    }

    nsIPrincipal *principal;
    nsresult rv;

    RefPtr<StorageEvent> event = static_cast<StorageEvent*>(aSubject);
    if (!event) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DOMStorage> changingStorage = event->GetStorageArea();
    if (!changingStorage) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDOMStorage> istorage = changingStorage.get();

    bool fireMozStorageChanged = false;
    nsAutoString eventType;
    eventType.AssignLiteral("storage");
    principal = GetPrincipal();
    if (!principal) {
      return NS_OK;
    }

    uint32_t privateBrowsingId = 0;
    nsIPrincipal *storagePrincipal = changingStorage->GetPrincipal();
    rv = storagePrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if ((privateBrowsingId > 0) != IsPrivateBrowsing()) {
      return NS_OK;
    }

    switch (changingStorage->GetType())
    {
    case DOMStorage::SessionStorage:
    {
      bool check = false;

      nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(GetDocShell());
      if (storageManager) {
        rv = storageManager->CheckStorage(principal, istorage, &check);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      if (!check) {
        // This storage event is not coming from our storage or is coming
        // from a different docshell, i.e. it is a clone, ignore this event.
        return NS_OK;
      }

      if (MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
        PR_LogPrint("nsGlobalWindow %p with sessionStorage %p passing event from %p",
                    this, mSessionStorage.get(), changingStorage.get());
      }

      fireMozStorageChanged = mSessionStorage == changingStorage;
      if (fireMozStorageChanged) {
        eventType.AssignLiteral("MozSessionStorageChanged");
      }
      break;
    }

    case DOMStorage::LocalStorage:
    {
      // Allow event fire only for the same principal storages
      // XXX We have to use EqualsIgnoreDomain after bug 495337 lands
      nsIPrincipal* storagePrincipal = changingStorage->GetPrincipal();

      bool equals = false;
      rv = storagePrincipal->Equals(principal, &equals);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!equals)
        return NS_OK;

      fireMozStorageChanged = mLocalStorage == changingStorage;
      if (fireMozStorageChanged) {
        eventType.AssignLiteral("MozLocalStorageChanged");
      }
      break;
    }
    default:
      return NS_OK;
    }

    // Clone the storage event included in the observer notification. We want
    // to dispatch clones rather than the original event.
    ErrorResult error;
    RefPtr<StorageEvent> newEvent = CloneStorageEvent(eventType,
                                                        event, error);
    if (error.Failed()) {
      return error.StealNSResult();
    }

    newEvent->SetTrusted(true);

    if (fireMozStorageChanged) {
      WidgetEvent* internalEvent = newEvent->WidgetEventPtr();
      internalEvent->mFlags.mOnlyChromeDispatch = true;
    }

    if (IsFrozen()) {
      // This window is frozen, rather than firing the events here,
      // store the domain in which the change happened and fire the
      // events if we're ever thawed.

      mPendingStorageEvents.AppendElement(newEvent);
      return NS_OK;
    }

    bool defaultActionEnabled;
    DispatchEvent(newEvent, &defaultActionEnabled);

    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "offline-cache-update-added")) {
    if (mApplicationCache)
      return NS_OK;

    // Instantiate the application object now. It observes update belonging to
    // this window's document and correctly updates the applicationCache object
    // state.
    nsCOMPtr<nsIDOMOfflineResourceList> applicationCache = GetApplicationCache();
    nsCOMPtr<nsIObserver> observer = do_QueryInterface(applicationCache);
    if (observer)
      observer->Observe(aSubject, aTopic, aData);

    return NS_OK;
  }

#ifdef MOZ_B2G
  if (!nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC) ||
      !nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC)) {
    MOZ_ASSERT(IsInnerWindow());
    if (!AsInner()->IsCurrentInnerWindow()) {
      return NS_OK;
    }

    RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
    event->InitEvent(
      !nsCRT::strcmp(aTopic, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC)
        ? NETWORK_UPLOAD_EVENT_NAME
        : NETWORK_DOWNLOAD_EVENT_NAME,
      false, false);
    event->SetTrusted(true);

    bool dummy;
    return DispatchEvent(event, &dummy);
  }
#endif // MOZ_B2G

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    MOZ_ASSERT(!NS_strcmp(aData, u"intl.accept_languages"));
    MOZ_ASSERT(IsInnerWindow());

    // The user preferred languages have changed, we need to fire an event on
    // Window object and invalidate the cache for navigator.languages. It is
    // done for every change which can be a waste of cycles but those should be
    // fairly rare.
    // We MUST invalidate navigator.languages before sending the event in the
    // very likely situation where an event handler will try to read its value.

    if (mNavigator) {
      NavigatorBinding::ClearCachedLanguageValue(mNavigator);
      NavigatorBinding::ClearCachedLanguagesValue(mNavigator);
    }

    // The event has to be dispatched only to the current inner window.
    if (!AsInner()->IsCurrentInnerWindow()) {
      return NS_OK;
    }

    RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("languagechange"), false, false);
    event->SetTrusted(true);

    bool dummy;
    return DispatchEvent(event, &dummy);
  }

  NS_WARNING("unrecognized topic in nsGlobalWindow::Observe");
  return NS_ERROR_FAILURE;
}

already_AddRefed<StorageEvent>
nsGlobalWindow::CloneStorageEvent(const nsAString& aType,
                                  const RefPtr<StorageEvent>& aEvent,
                                  ErrorResult& aRv)
{
  MOZ_ASSERT(IsInnerWindow());

  StorageEventInit dict;

  dict.mBubbles = aEvent->Bubbles();
  dict.mCancelable = aEvent->Cancelable();
  aEvent->GetKey(dict.mKey);
  aEvent->GetOldValue(dict.mOldValue);
  aEvent->GetNewValue(dict.mNewValue);
  aEvent->GetUrl(dict.mUrl);

  RefPtr<DOMStorage> storageArea = aEvent->GetStorageArea();
  MOZ_ASSERT(storageArea);

  RefPtr<DOMStorage> storage;
  if (storageArea->GetType() == DOMStorage::LocalStorage) {
    storage = GetLocalStorage(aRv);
  } else {
    MOZ_ASSERT(storageArea->GetType() == DOMStorage::SessionStorage);
    storage = GetSessionStorage(aRv);
  }

  if (aRv.Failed() || !storage) {
    return nullptr;
  }

  MOZ_ASSERT(storage);
  MOZ_ASSERT(storage->IsForkOf(storageArea));

  dict.mStorageArea = storage;

  RefPtr<StorageEvent> event = StorageEvent::Constructor(this, aType, dict);
  return event.forget();
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

  for (uint32_t i = 0, len = mPendingStorageEvents.Length(); i < len; ++i) {
    Observe(mPendingStorageEvents[i], "dom-storage2-changed", nullptr);
  }

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())->FirePendingEvents();
  }

  // Fires an offline status event if the offline status has changed
  FireOfflineStatusEventIfChanged();

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

      if (nsCOMPtr<nsPIDOMWindowOuter> pWin = childShell->GetWindow()) {
        auto* win = nsGlobalWindow::Cast(pWin);
        win->FireDelayedDOMEvents();
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow: Window Control Functions
//*****************************************************************************

nsPIDOMWindowOuter*
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

  nsCOMPtr<nsPIDOMWindowOuter> parent = GetParent();

  if (parent && parent != AsOuter()) {
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
  RefPtr<nsGlobalWindow> mWin;
public:
  explicit AutoUnblockScriptClosing(nsGlobalWindow* aWin)
    : mWin(aWin)
  {
    MOZ_ASSERT(mWin);
    MOZ_ASSERT(mWin->IsOuterWindow());
  }
  ~AutoUnblockScriptClosing()
  {
    void (nsGlobalWindow::*run)() = &nsGlobalWindow::UnblockScriptedClosing;
    NS_DispatchToCurrentThread(NewRunnableMethod(mWin, run));
  }
};

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, bool aDialog,
                             bool aContentModal, bool aCalledNoScript,
                             bool aDoJSFixups, bool aNavigate,
                             nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsPIDOMWindowOuter **aReturn)
{
  MOZ_ASSERT(IsOuterWindow());

#ifdef DEBUG
  uint32_t argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif
  NS_PRECONDITION(!aExtraArgument || (!argv && argc == 0),
                  "Can't pass in arguments both ways");
  NS_PRECONDITION(!aCalledNoScript || (!argv && argc == 0),
                  "Can't pass JS args when called via the noscript methods");

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

  // XXXbz When this gets fixed to not use LegacyIsCallerNativeCode()
  // (indirectly) maybe we can nix the AutoJSAPI usage OnLinkClickEvent::Run.
  // But note that if you change this to GetEntryGlobal(), say, then
  // OnLinkClickEvent::Run will need a full-blown AutoEntryScript.
  const bool checkForPopup = !nsContentUtils::LegacyIsCallerChromeOrNativeCode() &&
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
      if (!aCalledNoScript) {
        // If script in some other window is doing a window.open on us and
        // it's being blocked, then it's OK to close us afterwards, probably.
        // But if we're doing a window.open on ourselves and block the popup,
        // prevent this window from closing until after this script terminates
        // so that whatever popup blocker UI the app has will be visible.
        nsCOMPtr<nsPIDOMWindowInner> entryWindow =
          do_QueryInterface(GetEntryGlobal());
        // Note that entryWindow can be null here if some JS component was the
        // place where script was entered for this JS execution.
        if (entryWindow &&
            entryWindow->GetOuterWindow() == this->AsOuter()) {
          mBlockScriptedClosingFlag = true;
          closeUnblocker.emplace(this);
        }
      }

      FireAbuseEvents(aUrl, aName, aOptions);
      return aDoJSFixups ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<mozIDOMWindowProxy> domReturn;

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
      rv = pwwatch->OpenWindow2(AsOuter(), url.get(), name_ptr,
                                options_ptr, /* aCalledFromScript = */ true,
                                aDialog, aNavigate, argv,
                                1.0f, 0, getter_AddRefs(domReturn));
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
        nojsapi.emplace();
      }


      rv = pwwatch->OpenWindow2(AsOuter(), url.get(), name_ptr,
                                options_ptr, /* aCalledFromScript = */ false,
                                aDialog, aNavigate, aExtraArgument,
                                1.0f, 0, getter_AddRefs(domReturn));

    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // success!

  NS_ENSURE_TRUE(domReturn, NS_OK);
  nsCOMPtr<nsPIDOMWindowOuter> outerReturn =
    nsPIDOMWindowOuter::From(domReturn);
  outerReturn.swap(*aReturn);

  if (aDoJSFixups) {
    nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*aReturn));
    if (!chrome_win) {
      // A new non-chrome window was created from a call to
      // window.open() from JavaScript, make sure there's a document in
      // the new window. We do this by simply asking the new window for
      // its document, this will synchronously create an empty document
      // if there is no document in the window.
      // XXXbz should this just use EnsureInnerWindow()?

      // Force document creation.
      nsCOMPtr<nsIDocument> doc = (*aReturn)->GetDoc();
    }
  }

  if (checkForPopup) {
    MOZ_ASSERT(abuseLevel < openAbused, "Why didn't we take the early return?");

    if (abuseLevel >= openControlled) {
      nsGlobalWindow *opened = nsGlobalWindow::Cast(*aReturn);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(true);
        ++gOpenPopupSpamCount;
      }
    }
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
    if (!forwardTo && nsContentUtils::IsCallerChrome()) {
      forwardTo = currentInner;
    }
    if (!forwardTo) {
      aError.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    // If the caller and the callee share the same outer window, forward to the
    // caller inner. Else, we forward to the current inner (e.g. someone is
    // calling setTimeout() on a reference to some other window).
    if (forwardTo->GetOuterWindow() != AsOuter() ||
        !forwardTo->IsInnerWindow()) {
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
  return forwardTo->AsInner()->HasActiveDocument() ? currentInner : nullptr;
}

int32_t
nsGlobalWindow::SetTimeout(JSContext* aCx, Function& aFunction,
                           int32_t aTimeout,
                           const Sequence<JS::Value>& aArguments,
                           ErrorResult& aError)
{
  return SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments, false,
                              aError);
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
  return SetTimeoutOrInterval(aCx, aFunction, timeout, aArguments, isInterval,
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

  RefPtr<nsTimeout> timeout = new nsTimeout();
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
  //
  // Note the direction of this test: We don't allow setTimeouts running with
  // chrome privileges on content windows, but we do allow setTimeouts running
  // with content privileges on chrome windows (where they can't do very much,
  // of course).
  nsCOMPtr<nsIPrincipal> subjectPrincipal = nsContentUtils::SubjectPrincipal();
  nsCOMPtr<nsIPrincipal> ourPrincipal = GetPrincipal();
  if (ourPrincipal->Subsumes(subjectPrincipal)) {
    timeout->mPrincipal = subjectPrincipal;
  } else {
    timeout->mPrincipal = ourPrincipal;
  }

  TimeDuration delta = TimeDuration::FromMilliseconds(realInterval);

  if (!IsFrozen() && !mTimeoutsSuspendDepth) {
    // If we're not currently frozen, then we set timeout->mWhen to be the
    // actual firing time of the timer (i.e., now + delta). We also actually
    // create a timer and fire it off.

    timeout->mWhen = TimeStamp::Now() + delta;

    nsresult rv;
    timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    RefPtr<nsTimeout> copy = timeout;

    rv = timeout->InitTimer(realInterval);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // The timeout is now also held in the timer's closure.
    Unused << copy.forget();
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

int32_t
nsGlobalWindow::SetTimeoutOrInterval(JSContext *aCx, Function& aFunction,
                                     int32_t aTimeout,
                                     const Sequence<JS::Value>& aArguments,
                                     bool aIsInterval, ErrorResult& aError)
{
  nsGlobalWindow* inner = InnerForSetTimeoutOrInterval(aError);
  if (!inner) {
    return -1;
  }

  if (inner != this) {
    return inner->SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments,
                                       aIsInterval, aError);
  }

  nsCOMPtr<nsIScriptTimeoutHandler> handler =
    NS_CreateJSTimeoutHandler(aCx, this, aFunction, aArguments, aError);
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
  RefPtr<nsTimeout> timeout = aTimeout;
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

  const char *reason;
  if (timeout->mIsInterval) {
    reason = "setInterval handler";
  } else {
    reason = "setTimeout handler";
  }

  bool abortIntervalHandler = false;
  nsCOMPtr<nsIScriptTimeoutHandler> handler(timeout->mScriptHandler);
  RefPtr<Function> callback = handler->GetCallback();
  if (!callback) {
    // Evaluate the timeout expression.
    nsAutoString script;
    handler->GetHandlerText(script);

    const char* filename = nullptr;
    uint32_t lineNo = 0, dummyColumn = 0;
    handler->GetLocation(&filename, &lineNo, &dummyColumn);

    // New script entry point required, due to the "Create a script" sub-step of
    // http://www.whatwg.org/specs/web-apps/current-work/#timer-initialisation-steps
    nsAutoMicroTask mt;
    AutoEntryScript aes(this, reason, true);
    JS::CompileOptions options(aes.cx());
    options.setFileAndLine(filename, lineNo)
           .setVersion(JSVERSION_DEFAULT);
    JS::Rooted<JSObject*> global(aes.cx(), FastGetGlobalJSObject());
    nsresult rv = nsJSUtils::EvaluateString(aes.cx(), script, global, options);
    if (rv == NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE) {
      abortIntervalHandler = true;
    }
  } else {
    // Hold strong ref to ourselves while we call the callback.
    nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow *>(this));
    ErrorResult rv;
    JS::Rooted<JS::Value> ignoredVal(RootingCx());
    callback->Call(me, handler->GetArgs(), &ignoredVal, rv, reason);
    if (rv.IsUncatchableException()) {
      abortIntervalHandler = true;
    }

    rv.SuppressException();
  }

  // If we received an uncatchable exception, do not schedule the timeout again.
  // This allows the slow script dialog to break easy DoS attacks like
  // setInterval(function() { while(1); }, 100);
  if (abortIntervalHandler) {
    // If it wasn't an interval timer to begin with, this does nothing.  If it
    // was, we'll treat it as a timeout that we just ran and discard it when
    // we return.
    timeout->mIsInterval = false;
   }

  // We ignore any failures from calling EvaluateString() on the context or
  // Call() on a Function here since we're in a loop
  // where we're likely to be running timeouts whose OS timers
  // didn't fire in time and we don't want to not fire those timers
  // now just because execution of one timer failed. We can't
  // propagate the error to anyone who cares about it from this
  // point anyway, and the script context should have already reported
  // the script error in the usual way - so we just drop it.

  // Since we might be processing more timeouts, go ahead and flush the promise
  // queue now before we do that.  We need to do that while we're still in our
  // "running JS is safe" state (e.g. mRunningTimeout is set, timeout->mRunning
  // is false).
  Promise::PerformMicroTaskCheckpoint();

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
  nsresult rv = aTimeout->InitTimer(delay.ToMilliseconds());

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

  // The timeout list is kept in deadline order. Discover the latest timeout
  // whose deadline has expired. On some platforms, native timeout events fire
  // "early", but we handled that above by setting deadline to aTimeout->mWhen
  // if the timer fired early.  So we can stop walking if we get to timeouts
  // whose mWhen is greater than deadline, since once that happens we know
  // nothing past that point is expired.
  last_expired_timeout = nullptr;
  for (nsTimeout *timeout = mTimeouts.getFirst();
       timeout && timeout->mWhen <= deadline;
       timeout = timeout->getNext()) {
    if (timeout->mFiringDepth == 0) {
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
    gLastRecordedRecentTimeouts = now;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  RefPtr<nsTimeout> dummy_timeout = new nsTimeout();
  dummy_timeout->mFiringDepth = firingDepth;
  dummy_timeout->mWhen = now;
  last_expired_timeout->setNext(dummy_timeout);
  RefPtr<nsTimeout> timeoutExtraRef(dummy_timeout);

  last_insertion_point = mTimeoutInsertionPoint;
  // If we ever start setting mTimeoutInsertionPoint to a non-dummy timeout,
  // the logic in ResetTimersForNonBackgroundWindow will need to change.
  mTimeoutInsertionPoint = dummy_timeout;

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
    bool timeout_was_cleared = RunTimeoutHandler(timeout, scx);

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.
      MOZ_ASSERT(dummy_timeout->HasRefCntOne(), "dummy_timeout may leak");
      Unused << timeoutExtraRef.forget().take();

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
  timeoutExtraRef = nullptr;
  MOZ_ASSERT(dummy_timeout->HasRefCntOne(), "dummy_timeout may leak");

  mTimeoutInsertionPoint = last_insertion_point;
}

void
nsGlobalWindow::ClearTimeoutOrInterval(int32_t aTimerID)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

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

      nsresult rv = timeout->InitTimer(delay.ToMilliseconds());

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
  RefPtr<nsTimeout> timeout = (nsTimeout *)aClosure;

  timeout->mWindow->RunTimeout(timeout);
}

// static
void
nsGlobalWindow::TimerNameCallback(nsITimer* aTimer, void* aClosure, char* aBuf,
                                  size_t aLen)
{
  RefPtr<nsTimeout> timeout = (nsTimeout*)aClosure;

  const char* filename;
  uint32_t lineNum, column;
  timeout->mScriptHandler->GetLocation(&filename, &lineNum, &column);
  snprintf(aBuf, aLen, "[content] %s:%u:%u", filename, lineNum, column);
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
  nsCOMPtr<nsPIDOMWindowInner> sourceWindow = do_QueryInterface(GetEntryGlobal());
  if (!sourceWindow) {
    sourceWindow = AsOuter()->GetCurrentInnerWindow();
  }
  AutoJSContext cx;
  nsGlobalWindow* sourceWin = nsGlobalWindow::Cast(sourceWindow);
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

bool
nsGlobalWindow::IsPrivateBrowsing()
{
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
  return loadContext && loadContext->UsePrivateBrowsing();
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
  nsGlobalWindow *parent = nsGlobalWindow::Cast(GetPrivateParent());
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
                                bool aFreezeChildren,
                                bool aFreezeWorkers)
{
  FORWARD_TO_INNER_VOID(SuspendTimeouts,
                        (aIncrease, aFreezeChildren, aFreezeWorkers));

  bool suspended = (mTimeoutsSuspendDepth != 0);
  mTimeoutsSuspendDepth += aIncrease;

  if (!suspended) {
    nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
    if (ac) {
      for (uint32_t i = 0; i < mEnabledSensors.Length(); i++)
        ac->RemoveWindowListener(mEnabledSensors[i], this);
    }
    DisableGamepadUpdates();
    DisableVRUpdates();

    // Freeze or suspend all of the workers for this window.
    if (aFreezeWorkers) {
      mozilla::dom::workers::FreezeWorkersForWindow(AsInner());
    } else {
      mozilla::dom::workers::SuspendWorkersForWindow(AsInner());
    }

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
      ErrorResult dummy;
      RefPtr<Promise> d = mAudioContexts[i]->Suspend(dummy);
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

      if (nsCOMPtr<nsPIDOMWindowOuter> pWin = childShell->GetWindow()) {
        auto* win = nsGlobalWindow::Cast(pWin);
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

        // This is a bit hackish. Only freeze/suspend windows which are truly our
        // subwindows.
        nsCOMPtr<Element> frame = pWin->GetFrameElementInternal();
        if (!mDoc || !frame || mDoc != frame->OwnerDoc() || !inner) {
          continue;
        }

        win->SuspendTimeouts(aIncrease, aFreezeChildren, aFreezeWorkers);

        if (inner && aFreezeChildren) {
          inner->Freeze();
        }
      }
    }
  }
}

nsresult
nsGlobalWindow::ResumeTimeouts(bool aThawChildren, bool aThawWorkers)
{
  FORWARD_TO_INNER(ResumeTimeouts, (aThawChildren, aThawWorkers),
                   NS_ERROR_NOT_INITIALIZED);

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
    EnableVRUpdates();

    // Resume all of the AudioContexts for this window
    for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
      ErrorResult dummy;
      RefPtr<Promise> d = mAudioContexts[i]->Resume(dummy);
    }

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

      rv = t->InitTimer(delay);
      if (NS_FAILED(rv)) {
        t->mTimer = nullptr;
        return rv;
      }

      // Add a reference for the new timer's closure.
      t->AddRef();
    }

    // Thaw or resume all of the workers for this window.  We must do this
    // after timeouts since workers may have queued events that can trigger
    // a setTimeout().
    if (aThawWorkers) {
      mozilla::dom::workers::ThawWorkersForWindow(AsInner());
    } else {
      mozilla::dom::workers::ResumeWorkersForWindow(AsInner());
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

      if (nsCOMPtr<nsPIDOMWindowOuter> pWin = childShell->GetWindow()) {
        auto* win = nsGlobalWindow::Cast(pWin);
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

        rv = win->ResumeTimeouts(aThawChildren, aThawWorkers);
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

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
void
nsGlobalWindow::EnableOrientationChangeListener()
{
  MOZ_ASSERT(IsInnerWindow());
  if (!nsContentUtils::ShouldResistFingerprinting(mDocShell) &&
      !mOrientationChangeObserver) {
    mOrientationChangeObserver =
      new WindowOrientationObserver(this);
  }
}

void
nsGlobalWindow::DisableOrientationChangeListener()
{
  MOZ_ASSERT(IsInnerWindow());

  mOrientationChangeObserver = nullptr;
}
#endif

void
nsGlobalWindow::SetHasGamepadEventListener(bool aHasGamepad/* = true*/)
{
  MOZ_ASSERT(IsInnerWindow());
  mHasGamepad = aHasGamepad;
  if (aHasGamepad) {
    EnableGamepadUpdates();
  }
}


void
nsGlobalWindow::EventListenerAdded(nsIAtom* aType)
{
  if (aType == nsGkAtoms::onvrdisplayconnect ||
      aType == nsGkAtoms::onvrdisplaydisconnect ||
      aType == nsGkAtoms::onvrdisplaypresentchange) {
    NotifyVREventListenerAdded();
  }
}

void
nsGlobalWindow::NotifyVREventListenerAdded()
{
  MOZ_ASSERT(IsInnerWindow());
  mHasVREvents = true;
  EnableVRUpdates();
}

void
nsGlobalWindow::EnableTimeChangeNotifications()
{
  mozilla::time::AddWindowListener(AsInner());
}

void
nsGlobalWindow::DisableTimeChangeNotifications()
{
  mozilla::time::RemoveWindowListener(AsInner());
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
      // Multiple global windows can share a document. So only measure the
      // document if it (a) doesn't have a global window, or (b) it's the
      // primary document for the window.
      if (!mDoc->GetInnerWindow() ||
          mDoc->GetInnerWindow() == AsInner()) {
        mDoc->DocAddSizeOfIncludingThis(aWindowSizes);
      }
    }
  }

  if (mNavigator) {
    aWindowSizes->mDOMOtherSize +=
      mNavigator->SizeOfIncludingThis(aWindowSizes->mMallocSizeOf);
  }

  aWindowSizes->mDOMEventTargetsSize +=
    mEventTargetObjects.ShallowSizeOfExcludingThis(aWindowSizes->mMallocSizeOf);

  for (auto iter = mEventTargetObjects.ConstIter(); !iter.Done(); iter.Next()) {
    DOMEventTargetHelper* et = iter.Get()->GetKey();
    if (nsCOMPtr<nsISizeOfEventTarget> iSizeOf = do_QueryObject(et)) {
      aWindowSizes->mDOMEventTargetsSize +=
        iSizeOf->SizeOfEventTargetIncludingThis(aWindowSizes->mMallocSizeOf);
    }
    if (EventListenerManager* elm = et->GetExistingListenerManager()) {
      aWindowSizes->mDOMEventListenersCount += elm->ListenerCount();
    }
    ++aWindowSizes->mDOMEventTargetsCount;
  }
}


#ifdef MOZ_GAMEPAD
void
nsGlobalWindow::AddGamepad(uint32_t aIndex, Gamepad* aGamepad)
{
  MOZ_ASSERT(IsInnerWindow());
  // Create the index we will present to content based on which indices are
  // already taken, as required by the spec.
  // https://w3c.github.io/gamepad/gamepad.html#widl-Gamepad-index
  int index = 0;
  while(mGamepadIndexSet.Contains(index)) {
    ++index;
  }
  mGamepadIndexSet.Put(index);
  aGamepad->SetIndex(index);
  mGamepads.Put(aIndex, aGamepad);
}

void
nsGlobalWindow::RemoveGamepad(uint32_t aIndex)
{
  MOZ_ASSERT(IsInnerWindow());
  RefPtr<Gamepad> gamepad;
  if (!mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return;
  }
  // Free up the index we were using so it can be reused
  mGamepadIndexSet.Remove(gamepad->Index());
  mGamepads.Remove(aIndex);
}

void
nsGlobalWindow::GetGamepads(nsTArray<RefPtr<Gamepad> >& aGamepads)
{
  MOZ_ASSERT(IsInnerWindow());
  aGamepads.Clear();
  // mGamepads.Count() may not be sufficient, but it's not harmful.
  aGamepads.SetCapacity(mGamepads.Count());
  for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
    Gamepad* gamepad = iter.UserData();
    aGamepads.EnsureLengthAtLeast(gamepad->Index() + 1);
    aGamepads[gamepad->Index()] = gamepad;
  }
}

already_AddRefed<Gamepad>
nsGlobalWindow::GetGamepad(uint32_t aIndex)
{
  MOZ_ASSERT(IsInnerWindow());
  RefPtr<Gamepad> gamepad;

  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

void
nsGlobalWindow::SetHasSeenGamepadInput(bool aHasSeen)
{
  MOZ_ASSERT(IsInnerWindow());
  mHasSeenGamepadInput = aHasSeen;
}

bool
nsGlobalWindow::HasSeenGamepadInput()
{
  MOZ_ASSERT(IsInnerWindow());
  return mHasSeenGamepadInput;
}

void
nsGlobalWindow::SyncGamepadState()
{
  MOZ_ASSERT(IsInnerWindow());
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
      gamepadManager->SyncGamepadState(iter.Key(), iter.UserData());
    }
  }
}
#endif // MOZ_GAMEPAD

bool
nsGlobalWindow::UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDevices)
{
  FORWARD_TO_INNER(UpdateVRDisplays, (aDevices), false);

  VRDisplay::UpdateVRDisplays(mVRDisplays, AsInner());
  aDevices = mVRDisplays;
  return true;
}

void
nsGlobalWindow::NotifyActiveVRDisplaysChanged()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mNavigator) {
    mNavigator->NotifyActiveVRDisplaysChanged();
  }
}


// nsGlobalChromeWindow implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalChromeWindow)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserDOMWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGroupMessageManagers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserDOMWindow)
  if (tmp->mMessageManager) {
    static_cast<nsFrameMessageManager*>(
      tmp->mMessageManager.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  }
  tmp->DisconnectAndClearGroupMessageManagers();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGroupMessageManagers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// QueryInterface implementation for nsGlobalChromeWindow
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsGlobalChromeWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMChromeWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)

/* static */ already_AddRefed<nsGlobalChromeWindow>
nsGlobalChromeWindow::Create(nsGlobalWindow *aOuterWindow)
{
  RefPtr<nsGlobalChromeWindow> window = new nsGlobalChromeWindow(aOuterWindow);
  window->InitWasOffline();
  return window.forget();
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetWindowState(uint16_t* aWindowState)
{
  FORWARD_TO_INNER_CHROME(GetWindowState, (aWindowState), NS_ERROR_UNEXPECTED);

  *aWindowState = WindowState();
  return NS_OK;
}

uint16_t
nsGlobalWindow::WindowState()
{
  MOZ_ASSERT(IsInnerWindow());
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
  FORWARD_TO_INNER_CHROME(Maximize, (), NS_ERROR_UNEXPECTED);

  nsGlobalWindow::Maximize();
  return NS_OK;
}

void
nsGlobalWindow::Maximize()
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Maximized);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  FORWARD_TO_INNER_CHROME(Minimize, (), NS_ERROR_UNEXPECTED);

  nsGlobalWindow::Minimize();
  return NS_OK;
}

void
nsGlobalWindow::Minimize()
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Minimized);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::Restore()
{
  FORWARD_TO_INNER_CHROME(Restore, (), NS_ERROR_UNEXPECTED);

  nsGlobalWindow::Restore();
  return NS_OK;
}

void
nsGlobalWindow::Restore()
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Normal);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttention()
{
  FORWARD_TO_INNER_CHROME(GetAttention, (), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  GetAttention(rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::GetAttention(ErrorResult& aResult)
{
  MOZ_ASSERT(IsInnerWindow());
  return GetAttentionWithCycleCount(-1, aResult);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttentionWithCycleCount(int32_t aCycleCount)
{
  FORWARD_TO_INNER_CHROME(GetAttentionWithCycleCount, (aCycleCount), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  GetAttentionWithCycleCount(aCycleCount, rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::GetAttentionWithCycleCount(int32_t aCycleCount,
                                           ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->GetAttention(aCycleCount);
  }
}

NS_IMETHODIMP
nsGlobalChromeWindow::BeginWindowMove(nsIDOMEvent *aMouseDownEvent, nsIDOMElement* aPanel)
{
  FORWARD_TO_INNER_CHROME(BeginWindowMove, (aMouseDownEvent, aPanel), NS_ERROR_UNEXPECTED);

  NS_ENSURE_TRUE(aMouseDownEvent, NS_ERROR_FAILURE);
  Event* mouseDownEvent = aMouseDownEvent->InternalDOMEvent();
  NS_ENSURE_TRUE(mouseDownEvent, NS_ERROR_FAILURE);

  nsCOMPtr<Element> panel = do_QueryInterface(aPanel);
  NS_ENSURE_TRUE(panel || !aPanel, NS_ERROR_FAILURE);

  ErrorResult rv;
  BeginWindowMove(*mouseDownEvent, panel, rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::BeginWindowMove(Event& aMouseDownEvent, Element* aPanel,
                                ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

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
    aMouseDownEvent.WidgetEventPtr()->AsMouseEvent();
  if (!mouseEvent || mouseEvent->mClass != eMouseEventClass) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  aError = widget->BeginMoveDrag(mouseEvent);
}

already_AddRefed<nsWindowRoot>
nsGlobalWindow::GetWindowRootOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return root.forget().downcast<nsWindowRoot>();
}

already_AddRefed<nsWindowRoot>
nsGlobalWindow::GetWindowRoot(mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetWindowRootOuter, (), aError, nullptr);
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP
nsGlobalChromeWindow::SetCursor(const nsAString& aCursor)
{
  FORWARD_TO_INNER_CHROME(SetCursor, (aCursor), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  SetCursor(aCursor, rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::SetCursorOuter(const nsAString& aCursor, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

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

  RefPtr<nsPresContext> presContext;
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

void
nsGlobalWindow::SetCursor(const nsAString& aCursor, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetCursorOuter, (aCursor, aError), aError, );
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  FORWARD_TO_INNER_CHROME(GetBrowserDOMWindow, (aBrowserWindow), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  NS_IF_ADDREF(*aBrowserWindow = GetBrowserDOMWindow(rv));
  return rv.StealNSResult();
}

nsIBrowserDOMWindow*
nsGlobalWindow::GetBrowserDOMWindowOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsChromeWindow());
  return static_cast<nsGlobalChromeWindow*>(this)->mBrowserDOMWindow;
}

nsIBrowserDOMWindow*
nsGlobalWindow::GetBrowserDOMWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetBrowserDOMWindowOuter, (), aError, nullptr);
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow *aBrowserWindow)
{
  FORWARD_TO_INNER_CHROME(SetBrowserDOMWindow, (aBrowserWindow), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  SetBrowserDOMWindow(aBrowserWindow, rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::SetBrowserDOMWindowOuter(nsIBrowserDOMWindow* aBrowserWindow)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsChromeWindow());
  static_cast<nsGlobalChromeWindow*>(this)->mBrowserDOMWindow = aBrowserWindow;
}

void
nsGlobalWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserWindow,
                                    ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetBrowserDOMWindowOuter, (aBrowserWindow), aError, );
}

NS_IMETHODIMP
nsGlobalChromeWindow::NotifyDefaultButtonLoaded(nsIDOMElement* aDefaultButton)
{
  FORWARD_TO_INNER_CHROME(NotifyDefaultButtonLoaded,
                          (aDefaultButton), NS_ERROR_UNEXPECTED);

  nsCOMPtr<Element> defaultButton = do_QueryInterface(aDefaultButton);
  NS_ENSURE_ARG(defaultButton);

  ErrorResult rv;
  NotifyDefaultButtonLoaded(*defaultButton, rv);
  return rv.StealNSResult();
}

void
nsGlobalWindow::NotifyDefaultButtonLoaded(Element& aDefaultButton,
                                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
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
  LayoutDeviceIntRect buttonRect =
    LayoutDeviceIntRect::FromUnknownRect(frame->GetScreenRect());

  // Get the widget rect in screen coordinates.
  nsIWidget *widget = GetNearestWidget();
  if (!widget) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  LayoutDeviceIntRect widgetRect = widget->GetScreenBounds();

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
  FORWARD_TO_INNER_CHROME(GetMessageManager, (aManager), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  NS_IF_ADDREF(*aManager = GetMessageManager(rv));
  return rv.StealNSResult();
}

nsIMessageBroadcaster*
nsGlobalWindow::GetMessageManager(ErrorResult& aError)
{
  MOZ_ASSERT(IsChromeWindow());
  MOZ_RELEASE_ASSERT(IsInnerWindow());
  nsGlobalChromeWindow* myself = static_cast<nsGlobalChromeWindow*>(this);
  if (!myself->mMessageManager) {
    nsCOMPtr<nsIMessageBroadcaster> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    myself->mMessageManager =
      new nsFrameMessageManager(nullptr,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                MM_CHROME | MM_BROADCASTER);
  }
  return myself->mMessageManager;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetGroupMessageManager(const nsAString& aGroup,
                                             nsIMessageBroadcaster** aManager)
{
  FORWARD_TO_INNER_CHROME(GetGroupMessageManager, (aGroup, aManager), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  NS_IF_ADDREF(*aManager = GetGroupMessageManager(aGroup, rv));
  return rv.StealNSResult();
}

nsIMessageBroadcaster*
nsGlobalWindow::GetGroupMessageManager(const nsAString& aGroup,
                                       ErrorResult& aError)
{
  MOZ_ASSERT(IsChromeWindow());
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  nsGlobalChromeWindow* myself = static_cast<nsGlobalChromeWindow*>(this);
  nsCOMPtr<nsIMessageBroadcaster> messageManager =
    myself->mGroupMessageManagers.Get(aGroup);

  if (!messageManager) {
    nsFrameMessageManager* parent =
      static_cast<nsFrameMessageManager*>(GetMessageManager(aError));

    messageManager = new nsFrameMessageManager(nullptr,
                                               parent,
                                               MM_CHROME | MM_BROADCASTER);
    myself->mGroupMessageManagers.Put(aGroup, messageManager);
  }

  return messageManager;
}

// nsGlobalModalWindow implementation

// QueryInterface implementation for nsGlobalModalWindow
NS_INTERFACE_MAP_BEGIN(nsGlobalModalWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMModalContentWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalModalWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalModalWindow, nsGlobalWindow)


void
nsGlobalWindow::GetDialogArgumentsOuter(JSContext* aCx,
                                        JS::MutableHandle<JS::Value> aRetval,
                                        ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  if (!mDialogArguments) {
    MOZ_ASSERT(mIsClosed, "This window should be closed!");
    aRetval.setUndefined();
    return;
  }

  // This does an internal origin check, and returns undefined if the subject
  // does not subsumes the origin of the arguments.
  JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
  JSAutoCompartment ac(aCx, wrapper);
  mDialogArguments->Get(aCx, wrapper, nsContentUtils::SubjectPrincipal(),
                        aRetval, aError);
}

void
nsGlobalWindow::GetDialogArguments(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aRetval,
                                   ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDialogArgumentsOuter, (aCx, aRetval, aError),
                            aError, );
}

/* static */ already_AddRefed<nsGlobalModalWindow>
nsGlobalModalWindow::Create(nsGlobalWindow *aOuterWindow)
{
  RefPtr<nsGlobalModalWindow> window = new nsGlobalModalWindow(aOuterWindow);
  window->InitWasOffline();
  return window.forget();
}

NS_IMETHODIMP
nsGlobalModalWindow::GetDialogArguments(nsIVariant **aArguments)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetDialogArguments, (aArguments),
                                        NS_ERROR_NOT_INITIALIZED);

  // This does an internal origin check, and returns undefined if the subject
  // does not subsumes the origin of the arguments.
  return mDialogArguments->Get(nsContentUtils::SubjectPrincipal(), aArguments);
}

/* static */ already_AddRefed<nsGlobalWindow>
nsGlobalWindow::Create(nsGlobalWindow *aOuterWindow)
{
  RefPtr<nsGlobalWindow> window = new nsGlobalWindow(aOuterWindow);
  window->InitWasOffline();
  return window.forget();
}

void
nsGlobalWindow::InitWasOffline()
{
  mWasOffline = NS_IsOffline() || NS_IsAppOffline(GetPrincipal());
}

void
nsGlobalWindow::GetReturnValueOuter(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aReturnValue,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  if (mReturnValue) {
    JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
    JSAutoCompartment ac(aCx, wrapper);
    mReturnValue->Get(aCx, wrapper, nsContentUtils::SubjectPrincipal(),
                      aReturnValue, aError);
  } else {
    aReturnValue.setUndefined();
  }
}

void
nsGlobalWindow::GetReturnValue(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aReturnValue,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetReturnValueOuter, (aCx, aReturnValue, aError),
                            aError, );
}

NS_IMETHODIMP
nsGlobalModalWindow::GetReturnValue(nsIVariant **aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(GetReturnValue, (aRetVal), NS_OK);

  if (!mReturnValue) {
    nsCOMPtr<nsIVariant> variant = CreateVoidVariant();
    variant.forget(aRetVal);
    return NS_OK;
  }
  return mReturnValue->Get(nsContentUtils::SubjectPrincipal(), aRetVal);
}

void
nsGlobalWindow::SetReturnValueOuter(JSContext* aCx,
                                    JS::Handle<JS::Value> aReturnValue,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  nsCOMPtr<nsIVariant> returnValue;
  aError =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aReturnValue,
                                             getter_AddRefs(returnValue));
  if (!aError.Failed()) {
    mReturnValue = new DialogValueHolder(nsContentUtils::SubjectPrincipal(),
                                         returnValue);
  }
}

void
nsGlobalWindow::SetReturnValue(JSContext* aCx,
                               JS::Handle<JS::Value> aReturnValue,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetReturnValueOuter, (aCx, aReturnValue, aError),
                            aError, );
}

NS_IMETHODIMP
nsGlobalModalWindow::SetReturnValue(nsIVariant *aRetVal)
{
  FORWARD_TO_OUTER_MODAL_CONTENT_WINDOW(SetReturnValue, (aRetVal), NS_OK);

  mReturnValue = new DialogValueHolder(nsContentUtils::SubjectPrincipal(),
                                       aRetVal);
  return NS_OK;
}

/* static */
bool
nsGlobalWindow::IsModalContentWindow(JSContext* aCx, JSObject* aGlobal)
{
  return xpc::WindowOrNull(aGlobal)->IsModalContentWindow();
}

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
int16_t
nsGlobalWindow::Orientation() const
{
  return nsContentUtils::ShouldResistFingerprinting(mDocShell) ?
           0 : WindowOrientationObserver::OrientationAngle();
}
#endif

Console*
nsGlobalWindow::GetConsole(ErrorResult& aRv)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mConsole) {
    mConsole = Console::Create(AsInner(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return mConsole;
}

bool
nsGlobalWindow::IsSecureContext() const
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  return JS_GetIsSecureContext(js::GetObjectCompartment(GetWrapperPreserveColor()));
}

already_AddRefed<External>
nsGlobalWindow::GetExternal(ErrorResult& aRv)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

#ifdef HAVE_SIDEBAR
  if (!mExternal) {
    AutoJSContext cx;
    JS::Rooted<JSObject*> jsImplObj(cx);
    ConstructJSImplementation("@mozilla.org/sidebar;1", this, &jsImplObj, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    mExternal = new External(jsImplObj, this);
  }

  RefPtr<External> external = static_cast<External*>(mExternal.get());
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
  MOZ_RELEASE_ASSERT(IsInnerWindow());

#ifdef HAVE_SIDEBAR
  // First check for a named frame named "sidebar"
  nsCOMPtr<nsPIDOMWindowOuter> domWindow = GetChildWindow(NS_LITERAL_STRING("sidebar"));
  if (domWindow) {
    aResult.SetAsWindowProxy() = domWindow.forget();
    return;
  }

  RefPtr<External> external = GetExternal(aRv);
  if (external) {
    aResult.SetAsExternal() = external;
  }
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
#endif
}

void
nsGlobalWindow::ClearDocumentDependentSlots(JSContext* aCx)
{
  MOZ_ASSERT(IsInnerWindow());

  // If JSAPI OOMs here, there is basically nothing we can do to recover safely.
  if (!WindowBinding::ClearCachedDocumentValue(aCx, this) ||
      !WindowBinding::ClearCachedPerformanceValue(aCx, this)) {
    MOZ_CRASH("Unhandlable OOM while clearing document dependent slots.");
  }
}

/* static */
JSObject*
nsGlobalWindow::CreateNamedPropertiesObject(JSContext *aCx,
                                            JS::Handle<JSObject*> aProto)
{
  return WindowNamedPropertiesHandler::Create(aCx, aProto);
}

bool
nsGlobalWindow::GetIsPrerendered()
{
  nsIDocShell* docShell = GetDocShell();
  return docShell && docShell->GetIsPrerendered();
}

#ifdef MOZ_B2G
void
nsGlobalWindow::EnableNetworkEvent(EventMessage aEventMessage)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIPermissionManager> permMgr =
    services::GetPermissionManager();
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

  switch (aEventMessage) {
    case eNetworkUpload:
      if (!mNetworkUploadObserverEnabled) {
        mNetworkUploadObserverEnabled = true;
        os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC, false);
      }
      break;
    case eNetworkDownload:
      if (!mNetworkDownloadObserverEnabled) {
        mNetworkDownloadObserverEnabled = true;
        os->AddObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC, false);
      }
      break;
    default:
      break;
  }
}

void
nsGlobalWindow::DisableNetworkEvent(EventMessage aEventMessage)
{
  MOZ_ASSERT(IsInnerWindow());

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  switch (aEventMessage) {
    case eNetworkUpload:
      if (mNetworkUploadObserverEnabled) {
        mNetworkUploadObserverEnabled = false;
        os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC);
      }
      break;
    case eNetworkDownload:
      if (mNetworkDownloadObserverEnabled) {
        mNetworkDownloadObserverEnabled = false;
        os->RemoveObserver(mObserver, NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC);
      }
      break;
    default:
      break;
  }
}
#endif // MOZ_B2G

void
nsGlobalWindow::RedefineProperty(JSContext* aCx, const char* aPropName,
                                 JS::Handle<JS::Value> aValue,
                                 ErrorResult& aError)
{
  JS::Rooted<JSObject*> thisObj(aCx, GetWrapperPreserveColor());
  if (!thisObj) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!JS_WrapObject(aCx, &thisObj) ||
      !JS_DefineProperty(aCx, thisObj, aPropName, aValue, JSPROP_ENUMERATE,
                         JS_STUBGETTER, JS_STUBSETTER)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

void
nsGlobalWindow::GetReplaceableWindowCoord(JSContext* aCx,
                                          nsGlobalWindow::WindowCoordGetter aGetter,
                                          JS::MutableHandle<JS::Value> aRetval,
                                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  int32_t coord = (this->*aGetter)(aError);
  if (!aError.Failed() &&
      !ToJSValue(aCx, coord, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

void
nsGlobalWindow::SetReplaceableWindowCoord(JSContext* aCx,
                                          nsGlobalWindow::WindowCoordSetter aSetter,
                                          JS::Handle<JS::Value> aValue,
                                          const char* aPropName,
                                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * just treat this the way we would an IDL replaceable property.
   */
  nsGlobalWindow* outer = GetOuterWindowInternal();
  if (!outer || !outer->CanMoveResizeWindows(nsContentUtils::IsCallerChrome()) || outer->IsFrame()) {
    RedefineProperty(aCx, aPropName, aValue, aError);
    return;
  }

  int32_t value;
  if (!ValueToPrimitive<int32_t, eDefault>(aCx, aValue, &value)) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  (this->*aSetter)(value, aError);
}

void
nsGlobalWindow::FireOnNewGlobalObject()
{
  MOZ_ASSERT(IsInnerWindow());

  // AutoEntryScript required to invoke debugger hook, which is a
  // Gecko-specific concept at present.
  AutoEntryScript aes(this, "nsGlobalWindow report new global");
  JS::Rooted<JSObject*> global(aes.cx(), GetWrapper());
  JS_FireOnNewGlobalObject(aes.cx(), global);
}

#ifdef _WINDOWS_
#error "Never include windows.h in this file!"
#endif

already_AddRefed<Promise>
nsGlobalWindow::CreateImageBitmap(const ImageBitmapSource& aImage,
                                  ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  return ImageBitmap::Create(this, aImage, Nothing(), aRv);
}

already_AddRefed<Promise>
nsGlobalWindow::CreateImageBitmap(const ImageBitmapSource& aImage,
                                  int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
                                  ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  return ImageBitmap::Create(this, aImage, Some(gfx::IntRect(aSx, aSy, aSw, aSh)), aRv);
}

already_AddRefed<mozilla::dom::Promise>
nsGlobalWindow::CreateImageBitmap(const ImageBitmapSource& aImage,
                                  int32_t aOffset, int32_t aLength,
                                  ImageBitmapFormat aFormat,
                                  const Sequence<ChannelPixelLayout>& aLayout,
                                  ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    return ImageBitmap::Create(this, aImage, aOffset, aLength, aFormat, aLayout,
                               aRv);
  } else {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }
}

// Helper called by methods that move/resize the window,
// to ensure the presContext (if any) is aware of resolution
// change that may happen in multi-monitor configuration.
void
nsGlobalWindow::CheckForDPIChange()
{
  if (mDocShell) {
    RefPtr<nsPresContext> presContext;
    mDocShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      if (presContext->DeviceContext()->CheckDPIChange()) {
        presContext->UIResolutionChanged();
      }
    }
  }
}

nsGlobalWindow::TemporarilyDisableDialogs::TemporarilyDisableDialogs(
  nsGlobalWindow* aWindow MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  MOZ_ASSERT(aWindow);
  nsGlobalWindow* topWindow = aWindow->GetScriptableTopInternal();
  if (!topWindow) {
    NS_ERROR("nsGlobalWindow::TemporarilyDisableDialogs used without a top "
             "window?");
    return;
  }

  // TODO: Warn if no top window?
  topWindow = topWindow->GetCurrentInnerWindowInternal();
  if (topWindow) {
    mTopWindow = topWindow;
    mSavedDialogsEnabled = mTopWindow->mAreDialogsEnabled;
    mTopWindow->mAreDialogsEnabled = false;
  }
}

nsGlobalWindow::TemporarilyDisableDialogs::~TemporarilyDisableDialogs()
{
  if (mTopWindow) {
    mTopWindow->mAreDialogsEnabled = mSavedDialogsEnabled;
  }
}

template class nsPIDOMWindow<mozIDOMWindowProxy>;
template class nsPIDOMWindow<mozIDOMWindow>;
template class nsPIDOMWindow<nsISupports>;
