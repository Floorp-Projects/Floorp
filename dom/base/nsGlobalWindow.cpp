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
#include "mozilla/dom/Storage.h"
#include "mozilla/dom/IdleRequest.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/Timeout.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
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
#include "nsITimeoutHandler.h"
#include "nsIController.h"
#include "nsScriptNameSpaceManager.h"
#include "nsISlowScriptDebug.h"
#include "nsWindowMemoryReporter.h"
#include "WindowNamedPropertiesHandler.h"
#include "nsFrameSelection.h"
#include "nsNetUtil.h"
#include "nsVariant.h"
#include "nsPrintfCString.h"
#include "mozilla/intl/LocaleService.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jswrapper.h"
#include "nsCharSeparatedTokenizer.h"
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
#include "mozilla/ThrottledEventQueue.h"
#include "AudioChannelService.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharTraits.h" // NS_IS_HIGH/LOW_SURROGATE
#include "PostMessageEvent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"

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
#include "nsIEffectiveTLDService.h"

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

#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/Promise.h"

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadManager.h"

#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/VRDisplayEvent.h"
#include "mozilla/dom/VRDisplayEventBinding.h"
#include "mozilla/dom/VREventObserver.h"

#include "nsRefreshDriver.h"
#include "Layers.h"

#include "mozilla/AddonPathService.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Location.h"
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
#ifdef ENABLE_INTL_API
#include "mozilla/dom/IntlUtils.h"
#endif
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
#include "mozilla/dom/Worklet.h"
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
using mozilla::OriginAttributes;
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
static bool                 gMouseDown                 = false;
static bool                 gDragServiceDisabled       = false;
static FILE                *gDumpFile                  = nullptr;
static uint32_t             gSerialCounter             = 0;

#ifdef DEBUG_jst
int32_t gTimeoutCnt                                    = 0;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"

// The interval at which we execute idle callbacks
static uint32_t gThrottledIdlePeriodLength;

#define DEFAULT_THROTTLED_IDLE_PERIOD_LENGTH 10000

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
      nsCOMPtr<nsIDocument> kungFuDeathGrip = GetDoc();                       \
      ::mozilla::Unused << kungFuDeathGrip;                                   \
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
  ~nsGlobalWindowObserver() = default;

  // This reference is non-owning and safe because it's cleared by
  // nsGlobalWindow::CleanUp().
  nsGlobalWindow* MOZ_NON_OWNING_REF mWindow;
};

NS_IMPL_ISUPPORTS(nsGlobalWindowObserver, nsIObserver, nsIInterfaceRequestor)

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

class IdleRequestExecutor;

class IdleRequestExecutorTimeoutHandler final : public TimeoutHandler
{
public:
  explicit IdleRequestExecutorTimeoutHandler(IdleRequestExecutor* aExecutor)
    : mExecutor(aExecutor)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IdleRequestExecutorTimeoutHandler,
                                           TimeoutHandler)

  nsresult Call() override;

private:
  ~IdleRequestExecutorTimeoutHandler() {}
  RefPtr<IdleRequestExecutor> mExecutor;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler, mExecutor)

NS_IMPL_ADDREF_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler)
NS_IMPL_RELEASE_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestExecutorTimeoutHandler)
NS_INTERFACE_MAP_END_INHERITING(TimeoutHandler)


class IdleRequestExecutor final : public nsIRunnable
                                , public nsICancelableRunnable
                                , public nsIIncrementalRunnable
{
public:
  explicit IdleRequestExecutor(nsGlobalWindow* aWindow)
    : mDispatched(false)
    , mDeadline(TimeStamp::Now())
    , mWindow(aWindow)
  {
    MOZ_DIAGNOSTIC_ASSERT(mWindow);
    MOZ_DIAGNOSTIC_ASSERT(mWindow->IsInnerWindow());

    mIdlePeriodLimit = { mDeadline, mWindow->LastIdleRequestHandle() };
    mDelayedExecutorDispatcher = new IdleRequestExecutorTimeoutHandler(this);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IdleRequestExecutor, nsIRunnable)

  NS_DECL_NSIRUNNABLE
  nsresult Cancel() override;
  void SetDeadline(TimeStamp aDeadline) override;

  bool IsCancelled() const { return !mWindow || mWindow->AsInner()->InnerObjectsFreed(); }
  // Checks if aRequest shouldn't execute in the current idle period
  // since it has been queued from a chained call to
  // requestIdleCallback from within a running idle callback.
  bool IneligibleForCurrentIdlePeriod(IdleRequest* aRequest) const
  {
    return aRequest->Handle() >= mIdlePeriodLimit.mLastRequestIdInIdlePeriod &&
           TimeStamp::Now() <= mIdlePeriodLimit.mEndOfIdlePeriod;
  }

  void MaybeUpdateIdlePeriodLimit();

  // Maybe dispatch the IdleRequestExecutor. MabyeDispatch will
  // schedule a delayed dispatch if the associated window is in the
  // background or if given a time to wait until dispatching.
  void MaybeDispatch(TimeStamp aDelayUntil = TimeStamp());
  void ScheduleDispatch();
private:
  struct IdlePeriodLimit
  {
    TimeStamp mEndOfIdlePeriod;
    uint32_t mLastRequestIdInIdlePeriod;
  };

  void DelayedDispatch(uint32_t aDelay);

  ~IdleRequestExecutor() {}

  bool mDispatched;
  TimeStamp mDeadline;
  IdlePeriodLimit mIdlePeriodLimit;
  RefPtr<nsGlobalWindow> mWindow;
  // The timeout handler responsible for dispatching this executor in
  // the case of immediate dispatch to the idle queue isn't
  // desirable. This is used if we've dispatched all idle callbacks
  // that are allowed to run in the current idle period, or if the
  // associated window is currently in the background.
  nsCOMPtr<nsITimeoutHandler> mDelayedExecutorDispatcher;
  // If not Nothing() then this value is the handle to the currently
  // scheduled delayed executor dispatcher. This is needed to be able
  // to cancel the timeout handler in case of the executor being
  // cancelled.
  Maybe<int32_t> mDelayedExecutorHandle;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(IdleRequestExecutor)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IdleRequestExecutor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IdleRequestExecutor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IdleRequestExecutor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDelayedExecutorDispatcher)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IdleRequestExecutor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDelayedExecutorDispatcher)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestExecutor)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsICancelableRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIIncrementalRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
IdleRequestExecutor::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  mDispatched = false;
  if (mWindow) {
    return mWindow->ExecuteIdleRequest(mDeadline);
  }

  return NS_OK;
}

nsresult
IdleRequestExecutor::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mDelayedExecutorHandle && mWindow) {
    mWindow->AsInner()->TimeoutManager().ClearTimeout(
      mDelayedExecutorHandle.value(),
      Timeout::Reason::eIdleCallbackTimeout);
  }

  mWindow = nullptr;
  return NS_OK;
}

void
IdleRequestExecutor::SetDeadline(TimeStamp aDeadline)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow) {
    return;
  }

  mDeadline = aDeadline;
}

void
IdleRequestExecutor::MaybeUpdateIdlePeriodLimit()
{
  if (TimeStamp::Now() > mIdlePeriodLimit.mEndOfIdlePeriod) {
    mIdlePeriodLimit = { mDeadline, mWindow->LastIdleRequestHandle() };
  }
}

void
IdleRequestExecutor::MaybeDispatch(TimeStamp aDelayUntil)
{
  // If we've already dispatched the executor we don't want to do it
  // again. Also, if we've called IdleRequestExecutor::Cancel mWindow
  // will be null, which indicates that we shouldn't dispatch this
  // executor either.
  if (mDispatched || IsCancelled()) {
    return;
  }

  mDispatched = true;

  nsPIDOMWindowOuter* outer = mWindow->GetOuterWindow();
  if (outer && outer->AsOuter()->IsBackground()) {
    // Set a timeout handler with a timeout of 0 ms to throttle idle
    // callback requests coming from a backround window using
    // background timeout throttling.
    DelayedDispatch(0);
    return;
  }

  TimeStamp now = TimeStamp::Now();
  if (!aDelayUntil || aDelayUntil < now) {
    ScheduleDispatch();
    return;
  }

  TimeDuration delay = aDelayUntil - now;
  DelayedDispatch(static_cast<uint32_t>(delay.ToMilliseconds()));
}

void
IdleRequestExecutor::ScheduleDispatch()
{
  MOZ_ASSERT(mWindow);
  mDelayedExecutorHandle = Nothing();
  RefPtr<IdleRequestExecutor> request = this;
  NS_IdleDispatchToCurrentThread(request.forget());
}

void
IdleRequestExecutor::DelayedDispatch(uint32_t aDelay)
{
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mDelayedExecutorHandle.isNothing());
  int32_t handle;
  mWindow->AsInner()->TimeoutManager().SetTimeout(
    mDelayedExecutorDispatcher, aDelay, false, Timeout::Reason::eIdleCallbackTimeout, &handle);
  mDelayedExecutorHandle = Some(handle);
}

nsresult
IdleRequestExecutorTimeoutHandler::Call()
{
  if (!mExecutor->IsCancelled()) {
    mExecutor->ScheduleDispatch();
  }
  return NS_OK;
}

void
nsGlobalWindow::ScheduleIdleRequestDispatch()
{
  AssertIsOnMainThread();

  if (!mIdleRequestExecutor) {
    mIdleRequestExecutor = new IdleRequestExecutor(this);
  }

  mIdleRequestExecutor->MaybeDispatch();
}

void
nsGlobalWindow::SuspendIdleRequests()
{
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->Cancel();
    mIdleRequestExecutor = nullptr;
  }
}

void
nsGlobalWindow::ResumeIdleRequests()
{
  MOZ_ASSERT(!mIdleRequestExecutor);

  ScheduleIdleRequestDispatch();
}

void
nsGlobalWindow::InsertIdleCallback(IdleRequest* aRequest)
{
  AssertIsOnMainThread();
  mIdleRequestCallbacks.insertBack(aRequest);
  aRequest->AddRef();
}

void
nsGlobalWindow::RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest)
{
  AssertIsOnMainThread();

  if (aRequest->HasTimeout()) {
    mTimeoutManager->ClearTimeout(aRequest->GetTimeoutHandle(),
                                  Timeout::Reason::eIdleCallbackTimeout);
  }

  aRequest->removeFrom(mIdleRequestCallbacks);
  aRequest->Release();
}

nsresult
nsGlobalWindow::RunIdleRequest(IdleRequest* aRequest,
                               DOMHighResTimeStamp aDeadline,
                               bool aDidTimeout)
{
  AssertIsOnMainThread();
  RefPtr<IdleRequest> request(aRequest);
  RemoveIdleCallback(request);
  return request->IdleRun(AsInner(), aDeadline, aDidTimeout);
}

nsresult
nsGlobalWindow::ExecuteIdleRequest(TimeStamp aDeadline)
{
  AssertIsOnMainThread();
  RefPtr<IdleRequest> request = mIdleRequestCallbacks.getFirst();

  if (!request) {
    // There are no more idle requests, so stop scheduling idle
    // request callbacks.
    return NS_OK;
  }

  // If the request that we're trying to execute has been queued
  // during the current idle period, then dispatch it again at the end
  // of the idle period.
  if (mIdleRequestExecutor->IneligibleForCurrentIdlePeriod(request)) {
    mIdleRequestExecutor->MaybeDispatch(aDeadline);
    return NS_OK;
  }

  DOMHighResTimeStamp deadline = 0.0;

  if (Performance* perf = GetPerformance()) {
    deadline = perf->GetDOMTiming()->TimeStampToDOMHighRes(aDeadline);
  }

  mIdleRequestExecutor->MaybeUpdateIdlePeriodLimit();
  nsresult result = RunIdleRequest(request, deadline, false);

  mIdleRequestExecutor->MaybeDispatch();
  return result;
}

class IdleRequestTimeoutHandler final : public TimeoutHandler
{
public:
  IdleRequestTimeoutHandler(JSContext* aCx,
                            IdleRequest* aIdleRequest,
                            nsPIDOMWindowInner* aWindow)
    : TimeoutHandler(aCx)
    , mIdleRequest(aIdleRequest)
    , mWindow(aWindow)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IdleRequestTimeoutHandler,
                                           TimeoutHandler)

  nsresult Call() override
  {
    return nsGlobalWindow::Cast(mWindow)->RunIdleRequest(mIdleRequest, 0.0, true);
  }

private:
  ~IdleRequestTimeoutHandler() {}

  RefPtr<IdleRequest> mIdleRequest;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(IdleRequestTimeoutHandler,
                                   TimeoutHandler,
                                   mIdleRequest,
                                   mWindow)

NS_IMPL_ADDREF_INHERITED(IdleRequestTimeoutHandler, TimeoutHandler)
NS_IMPL_RELEASE_INHERITED(IdleRequestTimeoutHandler, TimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestTimeoutHandler)
NS_INTERFACE_MAP_END_INHERITING(TimeoutHandler)

uint32_t
nsGlobalWindow::RequestIdleCallback(JSContext* aCx,
                                    IdleRequestCallback& aCallback,
                                    const IdleRequestOptions& aOptions,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());
  AssertIsOnMainThread();

  uint32_t handle = mIdleRequestCallbackCounter++;

  RefPtr<IdleRequest> request =
    new IdleRequest(&aCallback, handle);

  if (aOptions.mTimeout.WasPassed()) {
    int32_t timeoutHandle;
    nsCOMPtr<nsITimeoutHandler> handler(new IdleRequestTimeoutHandler(aCx, request, AsInner()));

    nsresult rv = mTimeoutManager->SetTimeout(
        handler, aOptions.mTimeout.Value(), false,
        Timeout::Reason::eIdleCallbackTimeout, &timeoutHandle);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return 0;
    }

    request->SetTimeoutHandle(timeoutHandle);
  }

  // mIdleRequestCallbacks now owns request
  InsertIdleCallback(request);

  if (!IsSuspended()) {
    ScheduleIdleRequestDispatch();
  }

  return handle;
}

void
nsGlobalWindow::CancelIdleCallback(uint32_t aHandle)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  for (IdleRequest* r : mIdleRequestCallbacks) {
    if (r->Handle() == aHandle) {
      RemoveIdleCallback(r);
      break;
    }
  }
}

void
nsGlobalWindow::DisableIdleCallbackRequests()
{
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->Cancel();
    mIdleRequestExecutor = nullptr;
  }

  while (!mIdleRequestCallbacks.isEmpty()) {
    RefPtr<IdleRequest> request = mIdleRequestCallbacks.getFirst();
    RemoveIdleCallback(request);
  }
}

bool
nsGlobalWindow::IsBackgroundInternal() const
{
  return !mOuterWindow || mOuterWindow->IsBackground();
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
  mMutationBits(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false), mIsInnerWindow(aOuterWindow != nullptr),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mMayHavePointerEnterLeaveEventListener(false),
  mInnerObjectsFreed(false),
  mIsModalContentWindow(false),
  mIsActive(false), mIsBackground(false),
  mMediaSuspend(Preferences::GetBool("media.block-autoplay-until-in-foreground", true) ?
    nsISuspendedTypes::SUSPENDED_BLOCK : nsISuspendedTypes::NONE_SUSPENDED),
  mAudioMuted(false), mAudioVolume(1.0), mAudioCaptured(false),
  mDesktopModeViewport(false), mIsRootOuterWindow(false), mInnerWindow(nullptr),
  mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(NextWindowID()), mHasNotifiedGlobalCreated(false),
  mMarkedCCGeneration(0), mServiceWorkersTestingEnabled(false),
  mLargeAllocStatus(LargeAllocStatus::NONE),
  mShouldResumeOnFirstActiveMediaComponent(false)
{
  if (aOuterWindow) {
    mTimeoutManager =
      MakeUnique<mozilla::dom::TimeoutManager>(*nsGlobalWindow::Cast(AsInner()));
  }
}

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

  bool finalizeInBackground(const JS::Value& priv) const override {
    return false;
  }

  // Standard internal methods
  bool getOwnPropertyDescriptor(JSContext* cx,
                                JS::Handle<JSObject*> proxy,
                                JS::Handle<jsid> id,
                                JS::MutableHandle<JS::PropertyDescriptor> desc)
                                const override;
  bool defineProperty(JSContext* cx,
                      JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult &result) const override;
  bool ownPropertyKeys(JSContext *cx,
                       JS::Handle<JSObject*> proxy,
                       JS::AutoIdVector &props) const override;
  bool delete_(JSContext *cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id,
                       JS::ObjectOpResult &result) const override;

  bool getPrototypeIfOrdinary(JSContext* cx,
                              JS::Handle<JSObject*> proxy,
                              bool* isOrdinary,
                              JS::MutableHandle<JSObject*> protop) const override;

  bool enumerate(JSContext *cx, JS::Handle<JSObject*> proxy,
                 JS::MutableHandle<JSObject*> vp) const override;
  bool preventExtensions(JSContext* cx,
                         JS::Handle<JSObject*> proxy,
                         JS::ObjectOpResult& result) const override;
  bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
                    const override;
  bool has(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<jsid> id, bool *bp) const override;
  bool get(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<JS::Value> receiver,
           JS::Handle<jsid> id,
           JS::MutableHandle<JS::Value> vp) const override;
  bool set(JSContext *cx, JS::Handle<JSObject*> proxy,
           JS::Handle<jsid> id, JS::Handle<JS::Value> v,
           JS::Handle<JS::Value> receiver,
           JS::ObjectOpResult &result) const override;

  // SpiderMonkey extensions
  bool getPropertyDescriptor(JSContext* cx,
                             JS::Handle<JSObject*> proxy,
                             JS::Handle<jsid> id,
                             JS::MutableHandle<JS::PropertyDescriptor> desc)
                             const override;
  bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy,
              JS::Handle<jsid> id, bool *bp) const override;
  bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                    JS::AutoIdVector &props) const override;
  const char *className(JSContext *cx,
                        JS::Handle<JSObject*> wrapper) const override;

  void finalize(JSFreeOp *fop, JSObject *proxy) const override;

  bool isCallable(JSObject *obj) const override {
    return false;
  }
  bool isConstructor(JSObject *obj) const override {
    return false;
  }

  bool watch(JSContext *cx, JS::Handle<JSObject*> proxy,
             JS::Handle<jsid> id, JS::Handle<JSObject*> callable) const override;
  bool unwatch(JSContext *cx, JS::Handle<JSObject*> proxy,
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
    outerWindow->ClearWrapper(proxy);

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

  const char *className(JSContext *cx, JS::Handle<JSObject*> wrapper) const override;

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
    mHasSeenGamepadInput(false),
    mNotifiedIDDestroyed(false),
    mAllowScriptsToClose(false),
    mTopLevelOuterContentWindow(false),
    mSuspendDepth(0),
    mFreezeDepth(0),
    mFocusMethod(0),
    mSerial(0),
    mIdleRequestCallbackCounter(1),
    mIdleRequestExecutor(nullptr),
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
#ifdef DEBUG
    mIsValidatingTabGroup(false),
#endif
    mCanSkipCCGeneration(0),
    mAutoActivateVRDisplayID(0)
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

        // Watch for dom-storage2-changed and dom-private-storage2-changed so we
        // can fire storage events. Use a strong reference.
        os->AddObserver(mObserver, "dom-storage2-changed", false);
        os->AddObserver(mObserver, "dom-private-storage2-changed", false);
      }

      Preferences::AddStrongObserver(mObserver, "intl.accept_languages");
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window.
    MOZ_ASSERT(IsFrozen());
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  gRefCnt++;

  static bool sFirstTime = true;
  if (sFirstTime) {
    TimeoutManager::Initialize();
    Preferences::AddBoolVarCache(&sIdleObserversAPIFuzzTimeDisabled,
                                 "dom.idle-observers-api.fuzz_time.disabled",
                                 false);

    Preferences::AddUintVarCache(&gThrottledIdlePeriodLength,
                                 "dom.idle_period.throttled_length",
                                 DEFAULT_THROTTLED_IDLE_PERIOD_LENGTH);
    sFirstTime = false;
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
      url = mLastOpenedURI->GetSpecOrDefault();

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

  MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug, ("DOMWINDOW %p destroyed", this));

  if (IsOuterWindow()) {
    JSObject *proxy = GetWrapperMaybeDead();
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

  // We don't have to leave the tab group if we are an inner window.
  if (mTabGroup && IsOuterWindow()) {
    mTabGroup->Leave(AsOuter());
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
      IsPopupSpamWindow()) {
    SetIsPopupSpamWindow(false);
  }
}

void
nsGlobalWindow::SetIsPopupSpamWindow(bool aIsPopupSpam)
{
  MOZ_ASSERT(IsOuterWindow());

  mIsPopupSpam = aIsPopupSpam;
  if (aIsPopupSpam) {
    ++gOpenPopupSpamCount;
  } else {
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
      os->RemoveObserver(mObserver, "dom-private-storage2-changed");
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

  mAudioWorklet = nullptr;
  mPaintWorklet = nullptr;

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
    DisableIdleCallbackRequests();
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

#ifdef ENABLE_INTL_API
  mIntlUtils = nullptr;
#endif
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

  if (mTimeoutManager) {
    mTimeoutManager->ClearAllTimeouts();
  }

  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = nullptr;
  }

  mIdleObservers.Clear();

  DisableIdleCallbackRequests();

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
      mDoc->UnsuppressEventHandlingAndFireEvents(false);
    }
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

  DisableGamepadUpdates();
  mHasGamepad = false;
  mGamepads.Clear();
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
        iter.Data().exposeToActiveJS();
      }
    }
    if (EventListenerManager* elm = tmp->GetExistingListenerManager()) {
      elm->MarkForCC();
    }
    if (tmp->mTimeoutManager) {
      tmp->mTimeoutManager->UnmarkGrayTimers();
    }
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
      uri = tmp->mDoc->GetDocumentURI()->GetSpecOrDefault();
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

  if (tmp->mTimeoutManager) {
    tmp->mTimeoutManager->ForEachUnorderedTimeout([&cb](Timeout* timeout) {
      cb.NoteNativeChild(timeout, NS_CYCLE_COLLECTION_PARTICIPANT(Timeout));
    });
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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleRequestExecutor)
  for (IdleRequest* request : tmp->mIdleRequestCallbacks) {
    cb.NoteNativeChild(request, NS_CYCLE_COLLECTION_PARTICIPANT(IdleRequest));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleObservers)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepads)

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioWorklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPaintWorklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExternal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMozSelfSupport)
#ifdef ENABLE_INTL_API
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIntlUtils)
#endif

  tmp->TraverseHostObjectURIs(cb);
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGamepads)

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioWorklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPaintWorklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExternal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMozSelfSupport)
#ifdef ENABLE_INTL_API
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntlUtils)
#endif

  tmp->UnlinkHostObjectURIs();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleRequestExecutor)
  tmp->DisableIdleCallbackRequests();

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
      aCallbacks.Trace(&iter.Data(), "Cached XBL prototype handler", aClosure);
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
          HasKnownLiveWrapper()) &&
         (!aTracingNeeded ||
          HasNothingToTrace(static_cast<nsIDOMEventTarget*>(this)));
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

#ifdef DEBUG
{
  nsCOMPtr<nsIURI> uri;
  mDoc->GetDocumentURI()->CloneIgnoringRef(getter_AddRefs(uri));
  NS_ASSERTION(NS_IsAboutBlank(uri), "How'd this happen?");
}
#endif

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

  // We should never create windows with an expanded principal.
  // If we have a system principal, make sure we're not using it for a content
  // docshell.
  // NOTE: Please keep this logic in sync with nsWebShellWindow::Initialize().
  if (nsContentUtils::IsExpandedPrincipal(newWindowPrincipal) ||
      (nsContentUtils::IsSystemPrincipal(newWindowPrincipal) &&
       GetDocShell()->ItemType() != nsIDocShellTreeItem::typeChrome)) {
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
    // Otherwise, something is really weird. An about:blank page has a
    // NullPrincipal.
    bool isNullPrincipal;
    MOZ_ASSERT(NS_SUCCEEDED(mDoc->NodePrincipal()->GetIsNullPrincipal(&isNullPrincipal)) &&
               isNullPrincipal);
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

  aWindow->Suspend();

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
nsGlobalWindow::ComputeIsSecureContext(nsIDocument* aDocument, SecureContextFlags aFlags)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIPrincipal> principal = aDocument->NodePrincipal();
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return true;
  }

  // Implement https://w3c.github.io/webappsec-secure-contexts/#settings-object
  // With some modifications to allow for aFlags.

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
    if (aFlags != SecureContextFlags::eIgnoreOpener) {
      hadNonSecureContextCreator = !parentWin->IsSecureContext();
    } else {
      hadNonSecureContextCreator = !parentWin->IsSecureContextIfOpenerIgnored();
    }
  } else if (mHadOriginalOpener) {
    if (aFlags != SecureContextFlags::eIgnoreOpener) {
      hadNonSecureContextCreator = !mOriginalOpenerWasSecureContext;
    }
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
    const OriginAttributes& attrs = principal->OriginAttributesRef();
    // CreateCodebasePrincipal correctly gets a useful principal for blob: and
    // other URI_INHERITS_SECURITY_CONTEXT URIs.
    principal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
    if (NS_WARN_IF(!principal)) {
      return false;
    }
  }

  nsCOMPtr<nsIContentSecurityManager> csm =
    do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  NS_WARNING_ASSERTION(csm, "csm is null");
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
    options.creationOptions().setExistingZone(top->GetGlobalJSObject());
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
  if (!js::CheckRecursionLimitConservativeDontReport(cx)) {
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
    JS::Rooted<JSObject*> rootedObject(cx, GetWrapper());
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

      // The outer window is automatically treated as frozen when we
      // null out the inner window. As a result, initializing classes
      // on the new inner won't end up reaching into the old inner
      // window for classes etc.
      //
      // [This happens with Object.prototype when XPConnect creates
      // a temporary global while initializing classes; the reason
      // being that xpconnect creates the temp global w/o a parent
      // and proto, which makes the JS engine look up classes in
      // cx->globalObject, i.e. this outer window].

      mInnerWindow = nullptr;

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
      newInnerWindow->mIsSecureContextIfOpenerIgnored =
        ComputeIsSecureContext(aDocument, SecureContextFlags::eIgnoreOpener);

      mCreatingInnerWindow = false;
      createdInnerWindow = true;

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
                                             currentInner->mPerformance->GetChannel());
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

    mInnerWindow->SyncStateFromParentWindow();
  }

  // Add an extra ref in case we release mContext during GC.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(mContext);

  aDocument->SetScriptGlobalObject(newInnerWindow);
  MOZ_ASSERT(newInnerWindow->mTabGroup,
             "We must have a TabGroup cached at this point");

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
      rv = kungFuDeathGrip->InitClasses(obj);
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

  // Ask the JS engine to assert that it's valid to access our DocGroup whenever
  // it runs JS code for this compartment. We skip the check if this window is
  // for chrome JS or an add-on.
  nsCOMPtr<nsIPrincipal> principal = mDoc->NodePrincipal();
  nsString addonId;
  principal->GetAddonId(addonId);
  if (GetDocGroup() && !nsContentUtils::IsSystemPrincipal(principal) && addonId.IsEmpty()) {
    js::SetCompartmentValidAccessPtr(cx, newInnerGlobal,
                                     newInnerWindow->GetDocGroup()->GetValidAccessPtr());
  }

  kungFuDeathGrip->DidInitializeContext();

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

  // If we have a recorded interesting Large-Allocation header status, report it
  // to the newly attached document.
  ReportLargeAllocStatus();
  mLargeAllocStatus = LargeAllocStatus::NONE;

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

  // private browsing windows do not persist local storage to disk so we should
  // only try to precache storage when we're not a private browsing window.
  if (principal->GetPrivateBrowsingId() == 0) {
    nsCOMPtr<nsIDOMStorage> storage;
    rv = storageManager->PrecacheStorage(principal, getter_AddRefs(storage));
    if (NS_SUCCEEDED(rv)) {
      mLocalStorage = static_cast<Storage*>(storage.get());
    }
  }
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

  // The event dispatching could possibly cause docshell destory, and
  // consequently cause mDoc to be set to nullptr by DropOuterWindowDocs(),
  // so check it again here.
  if (observerService && mDoc) {
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

  if (MOZ_LOG_TEST(gDOMLeakPRLog, LogLevel::Debug)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
            ("DOMWINDOW %p SetNewDocument %s",
             this, uri ? uri->GetSpecOrDefault().get() : ""));
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

  nsCOMPtr<nsPIDOMWindowOuter> parentWindow = GetScriptableParentOrNull();
  MOZ_RELEASE_ASSERT(!parentWindow || !mTabGroup || mTabGroup == Cast(parentWindow)->mTabGroup);

  mTopLevelOuterContentWindow =
    !mIsChrome && GetScriptableTopInternal() == this;

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
      mIsRootOuterWindow = true;
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
    // When we're about to destroy a top level content window
    // (for example a tab), we trigger a full GC by passing null as the last
    // param. We also trigger a full GC for chrome windows.
    nsJSContext::PokeGC(JS::gcreason::SET_DOC_SHELL,
                        (mTopLevelOuterContentWindow || mIsChrome) ?
                          nullptr : GetWrapperPreserveColor());
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

  nsWeakPtr opener = do_GetWeakReference(aOpener);
  if (opener == mOpener) {
    return;
  }

  NS_ASSERTION(!aOriginalOpener || !mSetOpenerWindowCalled,
               "aOriginalOpener is true, but not first call to "
               "SetOpenerWindow!");
  NS_ASSERTION(aOpener || !aOriginalOpener,
               "Shouldn't set mHadOriginalOpener if aOpener is null");

  mOpener = opener.forget();
  NS_ASSERTION(mOpener || !aOpener, "Opener must support weak references!");

  // Check that the js visible opener matches! We currently don't depend on this
  // being true outside of nightly, so we disable the assertion in optimized
  // release / beta builds.
  nsPIDOMWindowOuter* contentOpener = GetSanitizedOpener(aOpener);

  // contentOpener is not used when the DIAGNOSTIC_ASSERT is compiled out.
  mozilla::Unused << contentOpener;
  MOZ_DIAGNOSTIC_ASSERT(!contentOpener || !mTabGroup ||
    mTabGroup == Cast(contentOpener)->mTabGroup);

  if (aOriginalOpener) {
    MOZ_ASSERT(!mHadOriginalOpener,
               "Probably too late to call ComputeIsSecureContext again");
    mHadOriginalOpener = true;
    mOriginalOpenerWasSecureContext =
      aOpener->GetCurrentInnerWindow()->IsSecureContext();
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
nsGlobalWindow::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  NS_PRECONDITION(IsInnerWindow(),
                  "GetEventTargetParent is used on outer window!?");
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
  mozilla::Unused << kungFuDeathGrip1; // These aren't referred to through the function
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());
  mozilla::Unused << kungFuDeathGrip2; // These aren't referred to through the function


  if (aVisitor.mEvent->mMessage == eResize) {
    mIsHandlingResizeEvent = false;
  } else if (aVisitor.mEvent->mMessage == eUnload &&
             aVisitor.mEvent->IsTrusted()) {

    // If any VR display presentation is active at unload, the next page
    // will receive a vrdisplayactive event to indicate that it should
    // immediately begin vr presentation. This should occur when navigating
    // forwards, navigating backwards, and on page reload.
    for (auto display : mVRDisplays) {
      if (display->IsPresenting()) {
        // Save this VR display ID to trigger vrdisplayactivate event
        // after the next load event.
        nsGlobalWindow* outer = GetOuterWindowInternal();
        if (outer) {
          outer->SetAutoActivateVRDisplayID(display->DisplayId());
        }

        // XXX The WebVR 1.1 spec does not define which of multiple VR
        // presenting VR displays will be chosen during navigation.
        // As the underlying platform VR API's currently only allow a single
        // VR display, it is safe to choose the first VR display for now.
        break;
      }
    }
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDoc) {
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->mMessage == eLoad &&
             aVisitor.mEvent->IsTrusted()) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::GetEventTargetParent.
    mIsDocumentLoaded = true;

    mTimeoutManager->OnDocumentLoaded();

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

    uint32_t autoActivateVRDisplayID = 0;
    nsGlobalWindow* outer = GetOuterWindowInternal();
    if (outer) {
      autoActivateVRDisplayID = outer->GetAutoActivateVRDisplayID();
    }
    if (autoActivateVRDisplayID) {
      DispatchVRDisplayActivate(autoActivateVRDisplayID,
                                VRDisplayEventReason::Navigation);
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
  if (aObject == GetWrapperMaybeDead()) {
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
    Unused << document;
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
nsGlobalWindow::Navigator()
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mNavigator) {
    mNavigator = new mozilla::dom::Navigator(AsInner());
  }

  return mNavigator;
}

nsIDOMNavigator*
nsGlobalWindow::GetNavigator()
{
  FORWARD_TO_INNER(GetNavigator, (), nullptr);

  return Navigator();
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

CustomElementRegistry*
nsGlobalWindow::CustomElements()
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mCustomElements) {
    mCustomElements = new CustomElementRegistry(AsInner());
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
    mPerformance = Performance::CreateForMainThread(this, timing, timedChannel);
  }
}

bool
nsPIDOMWindowInner::IsSecureContext() const
{
  return nsGlobalWindow::Cast(this)->IsSecureContext();
}

bool
nsPIDOMWindowInner::IsSecureContextIfOpenerIgnored() const
{
  return nsGlobalWindow::Cast(this)->IsSecureContextIfOpenerIgnored();
}

void
nsPIDOMWindowInner::Suspend()
{
  nsGlobalWindow::Cast(this)->Suspend();
}

void
nsPIDOMWindowInner::Resume()
{
  nsGlobalWindow::Cast(this)->Resume();
}

void
nsPIDOMWindowInner::Freeze()
{
  nsGlobalWindow::Cast(this)->Freeze();
}

void
nsPIDOMWindowInner::Thaw()
{
  nsGlobalWindow::Cast(this)->Thaw();
}

void
nsPIDOMWindowInner::SyncStateFromParentWindow()
{
  nsGlobalWindow::Cast(this)->SyncStateFromParentWindow();
}

bool
nsPIDOMWindowInner::IsPlayingAudio()
{
  RefPtr<AudioChannelService> acs = AudioChannelService::Get();
  if (!acs) {
    return false;
  }
  auto outer = GetOuterWindow();
  if (!outer) {
    // We've been unlinked and are about to die.  Not a good time to pretend to
    // be playing audio.
    return false;
  }
  return acs->IsWindowActive(outer);
}

mozilla::dom::TimeoutManager&
nsPIDOMWindowInner::TimeoutManager()
{
  return *mTimeoutManager;
}

bool
nsPIDOMWindowInner::IsRunningTimeout()
{
  return TimeoutManager().IsRunningTimeout();
}

void
nsPIDOMWindowOuter::NotifyCreatedNewMediaComponent()
{
  // We would only active media component when there is any alive one.
  mShouldResumeOnFirstActiveMediaComponent = true;

  // If the document is already on the foreground but the suspend state is still
  // suspend-block, that means the media component was created after calling
  // MaybeActiveMediaComponents, so the window's suspend state doesn't be
  // changed yet. Therefore, we need to call it again, because the state is only
  // changed after there exists alive media within the window.
  MaybeActiveMediaComponents();
}

void
nsPIDOMWindowOuter::MaybeActiveMediaComponents()
{
  if (IsInnerWindow()) {
    return mOuterWindow->MaybeActiveMediaComponents();
  }

  // Resume the media when the tab was blocked and the tab already has
  // alive media components.
  if (!mShouldResumeOnFirstActiveMediaComponent ||
      mMediaSuspend != nsISuspendedTypes::SUSPENDED_BLOCK) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = GetCurrentInnerWindow();
  if (!inner) {
    return;
  }

  // If the document is not visible, don't need to resume it.
  nsCOMPtr<nsIDocument> doc = inner->GetExtantDoc();
  if (!doc || doc->Hidden()) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("nsPIDOMWindowOuter, MaybeActiveMediaComponents, "
          "resume the window from blocked, this = %p\n", this));

  SetMediaSuspend(nsISuspendedTypes::NONE_SUSPENDED);
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
    MaybeNotifyMediaResumedFromBlock(aSuspend);
    mMediaSuspend = aSuspend;
  }

  RefreshMediaElementsSuspend(aSuspend);
}

void
nsPIDOMWindowOuter::MaybeNotifyMediaResumedFromBlock(SuspendTypes aSuspend)
{
  if (mMediaSuspend == nsISuspendedTypes::SUSPENDED_BLOCK &&
      aSuspend == nsISuspendedTypes::NONE_SUSPENDED) {
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      service->NotifyMediaResumedFromBlock(GetOuterWindow());
    }
  }
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
  if (mDocShell->GetIsMozBrowser()) {
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
  mDocShell->GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent));

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
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> content =
    GetContentInternal(aError, aCallerType);
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
                           CallerType aCallerType,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetContentOuter,
                            (aCx, aRetval, aCallerType, aError), aError, );
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindow::GetContentInternal(ErrorResult& aError, CallerType aCallerType)
{
  MOZ_ASSERT(IsOuterWindow());

  // First check for a named frame named "content"
  nsCOMPtr<nsPIDOMWindowOuter> domWindow =
    GetChildWindow(NS_LITERAL_STRING("content"));
  if (domWindow) {
    return domWindow.forget();
  }

  // If we're contained in <iframe mozbrowser>, then GetContent is the same as
  // window.top.
  if (mDocShell && mDocShell->GetIsInMozBrowser()) {
    return GetTopOuter();
  }

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;
  if (aCallerType != CallerType::System) {
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

  if (aId == XPCJSContext::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS)) {
    return true;
  }

  if (aId == XPCJSContext::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS)) {
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
nsGlobalWindow::GetSanitizedOpener(nsPIDOMWindowOuter* aOpener)
{
  if (!aOpener) {
    return nullptr;
  }

  nsGlobalWindow* win = nsGlobalWindow::Cast(aOpener);

  // First, ensure that we're not handing back a chrome window to content:
  if (win->IsChromeWindow()) {
    return nullptr;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIDocShell> openerDocShell = aOpener->GetDocShell();

  if (openerDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
    openerDocShell->GetRootTreeItem(getter_AddRefs(openerRootItem));
    nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
    if (openerRootDocShell) {
      uint32_t appType;
      nsresult rv = openerRootDocShell->GetAppType(&appType);
      if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
        return aOpener;
      }
    }
  }

  return nullptr;
}

nsPIDOMWindowOuter*
nsGlobalWindow::GetOpenerWindowOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  nsCOMPtr<nsPIDOMWindowOuter> opener = do_QueryReferent(mOpener);

  if (!opener) {
    return nullptr;
  }

  // First, check if we were called from a privileged chrome script
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    // Catch the case where we're chrome but the opener is not...
    if (GetPrincipal() == nsContentUtils::GetSystemPrincipal() &&
        nsGlobalWindow::Cast(opener)->GetPrincipal() != nsContentUtils::GetSystemPrincipal()) {
      return nullptr;
    }
    return opener;
  }

  return GetSanitizedOpener(opener);
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
  FORWARD_TO_OUTER(GetOpener, (), nullptr);

  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpenerWindowOuter();
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

  EnsureSizeAndPositionUpToDate();

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
nsGlobalWindow::GetInnerWidth(CallerType aCallerType, ErrorResult& aError)
{
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindow::GetInnerWidth(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerWidthOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetInnerWidth(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetInnerWidth, aValue,
                            aCallerType, aError);
}

nsresult
nsGlobalWindow::GetInnerWidth(int32_t* aInnerWidth)
{
  FORWARD_TO_INNER(GetInnerWidth, (aInnerWidth), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerWidth = GetInnerWidth(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::SetInnerWidthOuter(int32_t aInnerWidth,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  CheckSecurityWidthAndHeight(&aInnerWidth, nullptr, aCallerType);

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
nsGlobalWindow::SetInnerWidth(int32_t aInnerWidth, CallerType aCallerType,
                              ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerWidthOuter,
                            (aInnerWidth, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetInnerWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetInnerWidth,
                            aValue, "innerWidth", aCallerType, aError);
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
nsGlobalWindow::GetInnerHeight(CallerType aCallerType, ErrorResult& aError)
{
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindow::GetInnerHeight(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerHeightOuter, (aError), aError, 0);
}

void
nsGlobalWindow::GetInnerHeight(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetInnerHeight, aValue,
                            aCallerType, aError);
}

nsresult
nsGlobalWindow::GetInnerHeight(int32_t* aInnerHeight)
{
  FORWARD_TO_INNER(GetInnerHeight, (aInnerHeight), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerHeight = GetInnerHeight(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::SetInnerHeightOuter(int32_t aInnerHeight,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
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
    CheckSecurityWidthAndHeight(nullptr, &height, aCallerType);
    SetCSSViewportWidthAndHeight(width,
                                 nsPresContext::CSSPixelsToAppUnits(height));
    return;
  }

  int32_t height = 0;
  int32_t width  = 0;

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  docShellAsWin->GetSize(&width, &height);
  CheckSecurityWidthAndHeight(nullptr, &aInnerHeight, aCallerType);
  aError = SetDocShellWidthAndHeight(width, CSSToDevIntPixels(aInnerHeight));
}

void
nsGlobalWindow::SetInnerHeight(int32_t aInnerHeight,
                               CallerType aCallerType,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerHeightOuter,
                            (aInnerHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetInnerHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                               CallerType aCallerType, ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetInnerHeight,
                            aValue, "innerHeight", aCallerType, aError);
}

nsIntSize
nsGlobalWindow::GetOuterSize(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
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
    rootWindow->FlushPendingNotifications(FlushType::Layout);
  }

  nsIntSize sizeDevPixels;
  aError = treeOwnerAsWin->GetSize(&sizeDevPixels.width, &sizeDevPixels.height);
  if (aError.Failed()) {
    return nsIntSize();
  }

  return DevToCSSIntPixels(sizeDevPixels);
}

int32_t
nsGlobalWindow::GetOuterWidthOuter(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetOuterSize(aCallerType, aError).width;
}

int32_t
nsGlobalWindow::GetOuterWidth(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterWidthOuter, (aCallerType, aError),
                            aError, 0);
}

void
nsGlobalWindow::GetOuterWidth(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetOuterWidth, aValue,
                            aCallerType, aError);
}

int32_t
nsGlobalWindow::GetOuterHeightOuter(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetOuterSize(aCallerType, aError).height;
}

int32_t
nsGlobalWindow::GetOuterHeight(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterHeightOuter, (aCallerType, aError),
                            aError, 0);
}

void
nsGlobalWindow::GetOuterHeight(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aValue,
                               CallerType aCallerType,
                               ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetOuterHeight, aValue,
                            aCallerType, aError);
}

void
nsGlobalWindow::SetOuterSize(int32_t aLengthCSSPixels, bool aIsWidth,
                             CallerType aCallerType, ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  CheckSecurityWidthAndHeight(aIsWidth ? &aLengthCSSPixels : nullptr,
                              aIsWidth ? nullptr : &aLengthCSSPixels,
                              aCallerType);

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
nsGlobalWindow::SetOuterWidthOuter(int32_t aOuterWidth,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  SetOuterSize(aOuterWidth, true, aCallerType, aError);
}

void
nsGlobalWindow::SetOuterWidth(int32_t aOuterWidth,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterWidthOuter,
                            (aOuterWidth, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetOuterWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetOuterWidth,
                            aValue, "outerWidth", aCallerType, aError);
}

void
nsGlobalWindow::SetOuterHeightOuter(int32_t aOuterHeight,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  SetOuterSize(aOuterHeight, false, aCallerType, aError);
}

void
nsGlobalWindow::SetOuterHeight(int32_t aOuterHeight,
                               CallerType aCallerType,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterHeightOuter,
                            (aOuterHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetOuterHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                               CallerType aCallerType,
                               ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetOuterHeight,
                            aValue, "outerHeight", aCallerType, aError);
}

CSSIntPoint
nsGlobalWindow::GetScreenXY(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_ASSERT(IsOuterWindow());

  // When resisting fingerprinting, always return (0,0)
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
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
nsGlobalWindow::GetScreenXOuter(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return GetScreenXY(aCallerType, aError).x;
}

int32_t
nsGlobalWindow::GetScreenX(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenXOuter, (aCallerType, aError), aError, 0);
}

void
nsGlobalWindow::GetScreenX(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aValue,
                           CallerType aCallerType,
                           ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetScreenX, aValue,
                            aCallerType, aError);
}

nsRect
nsGlobalWindow::GetInnerScreenRect()
{
  MOZ_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nsRect();
  }

  EnsureSizeAndPositionUpToDate();

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
nsGlobalWindow::GetMozInnerScreenXOuter(CallerType aCallerType)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // When resisting fingerprinting, always return 0.
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.x);
}

float
nsGlobalWindow::GetMozInnerScreenX(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenXOuter, (aCallerType), aError, 0);
}

float
nsGlobalWindow::GetMozInnerScreenYOuter(CallerType aCallerType)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // Return 0 to prevent fingerprinting.
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 0.0;
  }

  nsRect r = GetInnerScreenRect();
  return nsPresContext::AppUnitsToFloatCSSPixels(r.y);
}

float
nsGlobalWindow::GetMozInnerScreenY(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenYOuter, (aCallerType), aError, 0);
}

float
nsGlobalWindow::GetDevicePixelRatioOuter(CallerType aCallerType)
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

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    return 1.0;
  }

  float overrideDPPX = presContext->GetOverrideDPPX();

  if (overrideDPPX > 0) {
    return overrideDPPX;
  }

  return float(nsPresContext::AppUnitsPerCSSPixel())/
      presContext->AppUnitsPerDevPixel();
}

float
nsGlobalWindow::GetDevicePixelRatio(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDevicePixelRatioOuter, (aCallerType), aError, 0.0);
}

float
nsPIDOMWindowOuter::GetDevicePixelRatio(CallerType aCallerType)
{
  return nsGlobalWindow::Cast(this)->GetDevicePixelRatioOuter(aCallerType);
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
nsGlobalWindow::SetScreenXOuter(int32_t aScreenX,
                                CallerType aCallerType,
                                ErrorResult& aError)
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

  CheckSecurityLeftAndTop(&aScreenX, nullptr, aCallerType);
  x = CSSToDevIntPixels(aScreenX);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

void
nsGlobalWindow::SetScreenX(int32_t aScreenX,
                           CallerType aCallerType,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenXOuter,
                            (aScreenX, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetScreenX(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           CallerType aCallerType, ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetScreenX,
                            aValue, "screenX", aCallerType, aError);
}

int32_t
nsGlobalWindow::GetScreenYOuter(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return GetScreenXY(aCallerType, aError).y;
}

int32_t
nsGlobalWindow::GetScreenY(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenYOuter, (aCallerType, aError), aError, 0);
}

void
nsGlobalWindow::GetScreenY(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aValue,
                           CallerType aCallerType, ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindow::GetScreenY, aValue,
                            aCallerType, aError);
}

void
nsGlobalWindow::SetScreenYOuter(int32_t aScreenY,
                                CallerType aCallerType,
                                ErrorResult& aError)
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

  CheckSecurityLeftAndTop(nullptr, &aScreenY, aCallerType);
  y = CSSToDevIntPixels(aScreenY);

  aError = treeOwnerAsWin->SetPosition(x, y);

  CheckForDPIChange();
}

void
nsGlobalWindow::SetScreenY(int32_t aScreenY,
                           CallerType aCallerType,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenYOuter,
                            (aScreenY, aCallerType, aError), aError, );
}

void
nsGlobalWindow::SetScreenY(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           CallerType aCallerType,
                           ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindow::SetScreenY,
                            aValue, "screenY", aCallerType, aError);
}

// NOTE: Arguments to this function should have values scaled to
// CSS pixels, not device pixels.
void
nsGlobalWindow::CheckSecurityWidthAndHeight(int32_t* aWidth, int32_t* aHeight,
                                            CallerType aCallerType)
{
  MOZ_ASSERT(IsOuterWindow());

#ifdef MOZ_XUL
  if (aCallerType != CallerType::System) {
    // if attempting to resize the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
  }
#endif

  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (aCallerType != CallerType::System) {
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
nsGlobalWindow::CheckSecurityLeftAndTop(int32_t* aLeft, int32_t* aTop,
                                        CallerType aCallerType)
{
  MOZ_ASSERT(IsOuterWindow());

  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (aCallerType != CallerType::System) {
#ifdef MOZ_XUL
    // if attempting to move the window, hide any open popups
    nsContentUtils::HidePopupsInDocument(mDoc);
#endif

    if (nsGlobalWindow* rootWindow = nsGlobalWindow::Cast(GetPrivateRoot())) {
      rootWindow->FlushPendingNotifications(FlushType::Layout);
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

  FlushPendingNotifications(FlushType::Layout);
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

CSSPoint
nsGlobalWindow::GetScrollXY(bool aDoFlush)
{
  MOZ_ASSERT(IsOuterWindow());

  if (aDoFlush) {
    FlushPendingNotifications(FlushType::Layout);
  } else {
    EnsureSizeAndPositionUpToDate();
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

  return CSSPoint::FromAppUnits(scrollPos);
}

double
nsGlobalWindow::GetScrollXOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetScrollXY(false).x;
}

double
nsGlobalWindow::GetScrollX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollXOuter, (), aError, 0);
}

double
nsGlobalWindow::GetScrollYOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  return GetScrollXY(false).y;
}

double
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
  docShell->FindChildWithName(aName, false, true, nullptr, nullptr,
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
    mDoc->CreateEvent(NS_LITERAL_STRING("CustomEvent"), CallerType::System,
                      res);
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
                               /* aCanBubble = */ true,
                               /* aCancelable = */ true,
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
                             bool aForceNoOpener,
                             bool aLookForCallerOnJSStack)
{
  NS_PRECONDITION(IsOuterWindow(), "Must be outer window");
  NS_PRECONDITION(mDocShell, "Must have docshell");

  if (aForceNoOpener) {
    return aName.LowerCaseEqualsLiteral("_self") ||
           aName.LowerCaseEqualsLiteral("_top") ||
           aName.LowerCaseEqualsLiteral("_parent");
  }

  nsCOMPtr<nsIDocShellTreeItem> caller;
  if (aLookForCallerOnJSStack) {
    caller = GetCallerDocShellTreeItem();
  }

  if (!caller) {
    caller = mDocShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  mDocShell->FindItemWithName(aName, nullptr, caller,
                              /* aSkipTabGroup = */ false,
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

static void
FinishDOMFullscreenChange(nsIDocument* aDoc, bool aInDOMFullscreen)
{
  if (aInDOMFullscreen) {
    // Ask the document to handle any pending DOM fullscreen change.
    if (!nsIDocument::HandlePendingFullscreenRequests(aDoc)) {
      // If we don't end up having anything in fullscreen,
      // async request exiting fullscreen.
      nsIDocument::AsyncExitFullscreen(aDoc);
    }
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
  }

  NS_IMETHOD Run() override;

private:
  ~FullscreenTransitionTask() override
  {
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
    ~Observer() = default;

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
  }
  nsCOMPtr<nsIRunnable> task =
    new FullscreenTransitionTask(duration, aWindow, aFullscreen,
                                 widget, nullptr, transitionData);
  task->Run();
  return true;
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
    NS_WARNING_ASSERTION(!rv.Failed(), "Failed to lock the wakelock");
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
    mDoc->FlushPendingNotifications(FlushType::Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

// static
void
nsGlobalWindow::MakeScriptDialogTitle(nsAString& aOutTitle,
                                      nsIPrincipal* aSubjectPrincipal)
{
  MOZ_ASSERT(aSubjectPrincipal);

  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aSubjectPrincipal->GetURI(getter_AddRefs(uri));
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
nsGlobalWindow::CanMoveResizeWindows(CallerType aCallerType)
{
  MOZ_ASSERT(IsOuterWindow());

  // When called from chrome, we can avoid the following checks.
  if (aCallerType != CallerType::System) {
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
      if (!treeOwner || NS_FAILED(treeOwner->GetTabCount(&itemCount))) {
        itemCount = 0;
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
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aError)
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
  MakeScriptDialogTitle(title, &aSubjectPrincipal);

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
nsGlobalWindow::Alert(nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());
  Alert(EmptyString(), aSubjectPrincipal, aError);
}

void
nsGlobalWindow::AlertOuter(const nsAString& aMessage,
                           nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  AlertOrConfirm(/* aAlert = */ true, aMessage, aSubjectPrincipal, aError);
}

void
nsGlobalWindow::Alert(const nsAString& aMessage,
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(AlertOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, );
}

bool
nsGlobalWindow::ConfirmOuter(const nsAString& aMessage,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  return AlertOrConfirm(/* aAlert = */ false, aMessage, aSubjectPrincipal,
                        aError);
}

bool
nsGlobalWindow::Confirm(const nsAString& aMessage,
                        nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ConfirmOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, false);
}

already_AddRefed<Promise>
nsGlobalWindow::Fetch(const RequestOrUSVString& aInput,
                      const RequestInit& aInit,
                      CallerType aCallerType, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aCallerType, aRv);
}

void
nsGlobalWindow::PromptOuter(const nsAString& aMessage,
                            const nsAString& aInitial,
                            nsAString& aReturn,
                            nsIPrincipal& aSubjectPrincipal,
                            ErrorResult& aError)
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
  MakeScriptDialogTitle(title, &aSubjectPrincipal);

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
                       nsAString& aReturn,
                       nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PromptOuter,
                            (aMessage, aInitial, aReturn, aSubjectPrincipal,
                             aError),
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
nsGlobalWindow::HomeOuter(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError)
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
                           nullptr,
                           &aSubjectPrincipal);
}

void
nsGlobalWindow::Home(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(HomeOuter, (aSubjectPrincipal, aError), aError, );
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

        bool shouldGetDefaultPrinterName = printerName.IsEmpty();
#ifdef MOZ_X11
        // In Linux, GTK backend does not support per printer settings.
        // Calling GetDefaultPrinterName causes a sandbox violation (see Bug 1329216).
        // The printer name is not needed anywhere else on Linux before it gets to the parent.
        // In the parent, we will then query the default printer name if no name is set.
        // Unless we are in the parent, we will skip this part.
        if (!XRE_IsParentProcess()) {
          shouldGetDefaultPrinterName = false;
        }
#endif
        if (shouldGetDefaultPrinterName) {
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
nsGlobalWindow::MoveToOuter(int32_t aXPos, int32_t aYPos,
                            CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveTo() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
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
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerType);

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
    CheckSecurityLeftAndTop(&cssPos.x, &cssPos.y, aCallerType);
    LayoutDevicePoint devPos = cssPos * CSSToLayoutDeviceScale(1.0);
    aError = treeOwnerAsWin->SetPosition(devPos.x, devPos.y);
  }

  CheckForDPIChange();
}

void
nsGlobalWindow::MoveTo(int32_t aXPos, int32_t aYPos,
                       CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveToOuter,
                            (aXPos, aYPos, aCallerType, aError), aError, );
}

void
nsGlobalWindow::MoveByOuter(int32_t aXDif, int32_t aYDif,
                            CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.moveBy() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
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

  CheckSecurityLeftAndTop(&cssPos.width, &cssPos.height, aCallerType);

  nsIntSize newDevPos(CSSToDevIntPixels(cssPos));

  aError = treeOwnerAsWin->SetPosition(newDevPos.width, newDevPos.height);

  CheckForDPIChange();
}

void
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif,
                       CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveByOuter,
                            (aXDif, aYDif, aCallerType, aError), aError, );
}

nsresult
nsGlobalWindow::MoveBy(int32_t aXDif, int32_t aYDif)
{
  FORWARD_TO_OUTER(MoveBy, (aXDif, aYDif), NS_ERROR_UNEXPECTED);

  ErrorResult rv;
  MoveByOuter(aXDif, aYDif, CallerType::System, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindow::ResizeToOuter(int32_t aWidth, int32_t aHeight,
                              CallerType aCallerType,
                              ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is a browser-element then dispatch a resize event to
   * the embedder.
   */
  if (mDocShell && mDocShell->GetIsMozBrowser()) {
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

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
    return;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = GetTreeOwnerWindow();
  if (!treeOwnerAsWin) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIntSize cssSize(aWidth, aHeight);
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize devSz(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(devSz.width, devSz.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindow::ResizeTo(int32_t aWidth, int32_t aHeight,
                         CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeToOuter,
                            (aWidth, aHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindow::ResizeByOuter(int32_t aWidthDif, int32_t aHeightDif,
                              CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  /*
   * If caller is a browser-element then dispatch a resize event to
   * parent.
   */
  if (mDocShell && mDocShell->GetIsMozBrowser()) {
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

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
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

  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwnerAsWin->SetSize(newDevSize.width, newDevSize.height, true);

  CheckForDPIChange();
}

void
nsGlobalWindow::ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                         CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeByOuter,
                            (aWidthDif, aHeightDif, aCallerType, aError),
                            aError, );
}

void
nsGlobalWindow::SizeToContentOuter(CallerType aCallerType, ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return;
  }

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * prevent window.sizeToContent() by exiting early
   */

  if (!CanMoveResizeWindows(aCallerType) || IsFrame()) {
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
  CheckSecurityWidthAndHeight(&cssSize.width, &cssSize.height, aCallerType);

  nsIntSize newDevSize(CSSToDevIntPixels(cssSize));

  aError = treeOwner->SizeShellTo(mDocShell, newDevSize.width,
                                  newDevSize.height);
}

void
nsGlobalWindow::SizeToContent(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SizeToContentOuter, (aCallerType, aError),
                            aError, );
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
  FlushPendingNotifications(FlushType::Layout);
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
  FlushPendingNotifications(FlushType::Layout);
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
  FlushPendingNotifications(FlushType::Layout);
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
  FlushPendingNotifications(FlushType::Layout);
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

  FlushPendingNotifications(FlushType::Layout);
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

  FlushPendingNotifications(FlushType::Layout);
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

  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    sf->ScrollSnap();
  }
}

void
nsGlobalWindow::ClearTimeout(int32_t aHandle)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
  }
}

void
nsGlobalWindow::ClearInterval(int32_t aHandle)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
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
                     const nsAString& aOptions, nsIDocShellLoadInfo* aLoadInfo,
                     bool aForceNoOpener, nsPIDOMWindowOuter **_retval)
{
  FORWARD_TO_OUTER(Open, (aUrl, aName, aOptions, aLoadInfo, aForceNoOpener,
                          _retval),
                   NS_ERROR_NOT_INITIALIZED);
  return OpenInternal(aUrl, aName, aOptions,
                      false,          // aDialog
                      false,          // aContentModal
                      true,           // aCalledNoScript
                      false,          // aDoJSFixups
                      true,           // aNavigate
                      nullptr, nullptr,  // No args
                      aLoadInfo,
                      aForceNoOpener,
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
                      nullptr,        // aLoadInfo
                      false,          // aForceNoOpener
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
                      nullptr, aExtraArgument, // Arguments
                      nullptr,                 // aLoadInfo
                      false,                   // aForceNoOpener
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
                      nullptr,        // aLoadInfo
                      false,          // aForceNoOpener
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
                        nullptr,          // aLoadInfo
                        false,            // aForceNoOpener
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
  FlushPendingNotifications(FlushType::ContentAndNotify);
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
                                    nsIPrincipal& aSubjectPrincipal,
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

    OriginAttributes attrs = aSubjectPrincipal.OriginAttributesRef();
    if (aSubjectPrincipal.GetIsSystemPrincipal()) {
      auto principal = BasePrincipal::Cast(GetPrincipal());

      if (attrs != principal->OriginAttributesRef()) {
        nsCOMPtr<nsIURI> targetURI;
        nsAutoCString targetURL;
        nsAutoCString sourceOrigin;
        nsAutoCString targetOrigin;

        if (NS_FAILED(principal->GetURI(getter_AddRefs(targetURI))) ||
            NS_FAILED(targetURI->GetAsciiSpec(targetURL)) ||
            NS_FAILED(principal->GetOrigin(targetOrigin)) ||
            NS_FAILED(aSubjectPrincipal.GetOrigin(sourceOrigin))) {
          NS_WARNING("Failed to get source and target origins");
          return;
        }

        nsContentUtils::LogSimpleConsoleError(
          NS_ConvertUTF8toUTF16(nsPrintfCString(
            R"(Attempting to post a message to window with url "%s" and )"
            R"(origin "%s" from a system principal scope with mismatched )"
            R"(origin "%s".)",
            targetURL.get(), targetOrigin.get(), sourceOrigin.get())),
          "DOM");

        attrs = principal->OriginAttributesRef();
      }
    }

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

  event->Write(aCx, message, transfer, JS::CloneDataPolicy(), aError);
  if (NS_WARN_IF(aError.Failed())) {
    return;
  }

  aError = Dispatch("PostMessageEvent", TaskCategory::Other, event.forget());
}

void
nsGlobalWindow::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const nsAString& aTargetOrigin,
                               JS::Handle<JS::Value> aTransfer,
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PostMessageMozOuter,
                            (aCx, aMessage, aTargetOrigin, aTransfer,
                             aSubjectPrincipal, aError),
                            aError, );
}

void
nsGlobalWindow::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const nsAString& aTargetOrigin,
                               const Sequence<JSObject*>& aTransfer,
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aRv)
{
  JS::Rooted<JS::Value> transferArray(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransfer,
                                                          &transferArray);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  PostMessageMoz(aCx, aMessage, aTargetOrigin, transferArray,
                 aSubjectPrincipal, aRv);
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
    nsresult rv =
      aWindow->Dispatch("nsCloseEvent", TaskCategory::Other, ev.forget());
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
      (IsFrame() && !mDocShell->GetIsMozBrowser())) {
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

  // Don't allow scripts from content to close non-neterror windows that
  // were not opened by script.
  nsAutoString url;
  nsresult rv = mDoc->GetURL(url);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!StringBeginsWith(url, NS_LITERAL_STRING("about:neterror")) &&
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
        bool isTab;
        if (rootWin == AsOuter() ||
            !bwin ||
            (NS_SUCCEEDED(bwin->IsTabContentWindow(GetOuterWindowInternal(),
                                                   &isTab)) && isTab)) {
          treeOwnerAsWin->Destroy();
        }
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
    ds->EndDragSession(true, 0);
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
      topDoc->SuppressEventHandling();
    }

    nsGlobalWindow* inner = topWin->GetCurrentInnerWindowInternal();
    if (inner) {
      topWin->GetCurrentInnerWindowInternal()->Suspend();
    }
  }
  topWin->mModalStateDepth++;
}

void
nsGlobalWindow::LeaveModalState()
{
  MOZ_ASSERT(IsOuterWindow(), "Modal state is maintained on outer windows");

  nsGlobalWindow* topWin = GetScriptableTopInternal();

  if (!topWin) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");
    return;
  }

  MOZ_ASSERT(topWin->mModalStateDepth != 0);
  MOZ_ASSERT(IsSuspended());
  MOZ_ASSERT(topWin->IsSuspended());
  topWin->mModalStateDepth--;

  nsGlobalWindow* inner = topWin->GetCurrentInnerWindowInternal();

  if (topWin->mModalStateDepth == 0) {
    if (inner) {
      inner->Resume();
    }

    if (topWin->mSuspendedDoc) {
      nsCOMPtr<nsIDocument> currentDoc = topWin->GetExtantDoc();
      topWin->mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(currentDoc == topWin->mSuspendedDoc);
      topWin->mSuspendedDoc = nullptr;
    }
  }

  // Remember the time of the last dialog quit.
  if (inner) {
    inner->mLastDialogQuitTime = TimeStamp::Now();
  }

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
  bool match(JSCompartment* aC) const override
  {
    nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(aC));
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
    PROFILER_LABEL("WindowDestroyedEvent", "Run",
                   js::ProfileEntry::Category::OTHER);

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
      if (obj && !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
        JSCompartment* cpt = js::GetObjectCompartment(obj);
        nsCOMPtr<nsIPrincipal> pc = nsJSPrincipals::get(JS_GetCompartmentPrincipals(cpt));

        nsAutoString addonId;
        if (NS_SUCCEEDED(pc->GetAddonId(addonId)) && !addonId.IsEmpty()) {
          // We want to nuke all references to the add-on compartment.
          xpc::NukeAllWrappersForCompartment(cx, cpt,
                                             win->IsInnerWindow() ? js::DontNukeWindowReferences
                                                                  : js::NukeWindowReferences);
        } else {
          // We only want to nuke wrappers for the chrome->content case
          js::NukeCrossCompartmentWrappers(cx, BrowserCompartmentMatcher(),
                                           js::SingleCompartment(cpt),
                                           win->IsInnerWindow() ? js::DontNukeWindowReferences
                                                                : js::NukeWindowReferences,
                                           js::NukeIncomingReferences);
        }
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
  nsresult rv =
    Dispatch("WindowDestroyedEvent", TaskCategory::Other, runnable.forget());
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
    mCachedXBLPrototypeHandlers = new XBLPrototypeHandlerTable();
    PreserveWrapper(ToSupports(this));
  }

  mCachedXBLPrototypeHandlers->Put(aKey, aHandler);
}

Element*
nsGlobalWindow::GetFrameElementOuter(nsIPrincipal& aSubjectPrincipal)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell || mDocShell->GetIsMozBrowser()) {
    return nullptr;
  }

  // Per HTML5, the frameElement getter returns null in cross-origin situations.
  Element* element = GetRealFrameElementOuter();
  if (!element) {
    return nullptr;
  }

  if (!aSubjectPrincipal.SubsumesConsideringDomain(element->NodePrincipal())) {
    return nullptr;
  }

  return element;
}

Element*
nsGlobalWindow::GetFrameElement(nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFrameElementOuter, (aSubjectPrincipal), aError,
                            nullptr);
}

Element*
nsGlobalWindow::GetRealFrameElementOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  if (!mDocShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> parent;
  mDocShell->GetSameTypeParentIgnoreBrowserBoundaries(getter_AddRefs(parent));

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
nsGlobalWindow::ShowModalDialogOuter(const nsAString& aUrl,
                                     nsIVariant* aArgument,
                                     const nsAString& aOptions,
                                     nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError)
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
    new DialogValueHolder(&aSubjectPrincipal, aArgument);

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
                        nullptr,        // aLoadInfo
                        false,          // aForceNoOpener
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
                                const nsAString& aOptions,
                                nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ShowModalDialogOuter,
                            (aUrl, aArgument, aOptions, aSubjectPrincipal,
                             aError), aError, nullptr);
}

void
nsGlobalWindow::ShowModalDialog(JSContext* aCx, const nsAString& aUrl,
                                JS::Handle<JS::Value> aArgument,
                                const nsAString& aOptions,
                                JS::MutableHandle<JS::Value> aRetval,
                                nsIPrincipal& aSubjectPrincipal,
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

  nsCOMPtr<nsIVariant> retVal =
    ShowModalDialog(aUrl, args, aOptions, aSubjectPrincipal, aError);
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

  Unused << aShowDialog;

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

  if (aString.IsEmpty()) {
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
nsGlobalWindow::GetOrigin(nsAString& aOrigin)
{
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());
  nsContentUtils::GetUTFOrigin(GetPrincipal(), aOrigin);
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

Location*
nsGlobalWindow::GetLocation(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  nsIDocShell *docShell = GetDocShell();
  if (!mLocation && docShell) {
    mLocation = new Location(AsInner(), docShell);
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
  if (mDoc) {
    NotifyDocumentTree(mDoc, nullptr);
  }
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

  nsGlobalWindow* inner = GetCurrentInnerWindowInternal();

  if (aIsBackground) {
    MOZ_ASSERT(!resetTimers);
    // Notify gamepadManager we are at the background window,
    // we need to stop vibrate.
    if (inner) {
      inner->StopGamepadHaptics();
    }
    return;
  } else if (inner) {
    if (resetTimers) {
      inner->mTimeoutManager->ResetTimersForThrottleReduction();
    }
    inner->SyncGamepadState();
  }
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
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->AddListener(this);
    }
  }
}

void
nsGlobalWindow::DisableGamepadUpdates()
{
  MOZ_ASSERT(IsInnerWindow());

  if (mHasGamepad) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->RemoveListener(this);
    }
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
  if (mVREventObserver) {
    mVREventObserver->DisconnectFromOwner();
    mVREventObserver = nullptr;
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
  nsresult rv = aOldURI->GetSpec(oldSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aNewURI->GetSpec(newSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 oldWideSpec(oldSpec);
  NS_ConvertUTF8toUTF16 newWideSpec(newSpec);

  nsCOMPtr<nsIRunnable> callback =
    new HashchangeCallback(oldWideSpec, newWideSpec, this);
  return Dispatch("HashchangeCallback", TaskCategory::Other, callback.forget());
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
  FORWARD_TO_OUTER_OR_THROW(GetComputedStyleOuter,
                            (aElt, aPseudoElt), aError, nullptr);
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindow::GetComputedStyleOuter(Element& aElt,
                                      const nsAString& aPseudoElt)
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

    parent->FlushPendingNotifications(FlushType::Frames);

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
    NS_NewComputedDOMStyle(&aElt, aPseudoElt, presShell);

  return compStyle.forget();
}

Storage*
nsGlobalWindow::GetSessionStorage(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell || !Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
  }

  if (mSessionStorage) {
    MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
            ("nsGlobalWindow %p has %p sessionStorage", this, mSessionStorage.get()));
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
      aError = mDoc->GetDocumentURI(documentURI);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
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
                                           IsPrivateBrowsing(),
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mSessionStorage = static_cast<Storage*>(storage.get());
    MOZ_ASSERT(mSessionStorage);

    MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
            ("nsGlobalWindow %p tried to get a new sessionStorage %p", this, mSessionStorage.get()));

    if (!mSessionStorage) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
          ("nsGlobalWindow %p returns %p sessionStorage", this, mSessionStorage.get()));

  return mSessionStorage;
}

Storage*
nsGlobalWindow::GetLocalStorage(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!Preferences::GetBool(kStorageEnabled)) {
    return nullptr;
  }

  if (!mLocalStorage) {
    if (nsContentUtils::StorageAllowedForWindow(AsInner()) ==
          nsContentUtils::StorageAccess::eDeny) {
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
      aError = mDoc->GetDocumentURI(documentURI);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    }

    nsCOMPtr<nsIDOMStorage> storage;
    aError = storageManager->CreateStorage(AsInner(), principal, documentURI,
                                           IsPrivateBrowsing(),
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mLocalStorage = static_cast<Storage*>(storage.get());
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

  // Don't fire an event if the status hasn't changed
  if (mWasOffline == NS_IsOffline()) {
    return;
  }

  mWasOffline = !mWasOffline;

  nsAutoString name;
  if (mWasOffline) {
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
  if (NS_FAILED(Dispatch("NotifyIdleObserverRunnable", TaskCategory::Other,
                         caller.forget()))) {
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
    JS_ReportWarningASCII(cx, "A long running script was terminated");
    return KillSlowScript;
  }

  // If our document is not active, just kill the script: we've been unloaded
  if (!AsInner()->HasActiveDocument()) {
    return KillSlowScript;
  }

  // Check if we should offer the option to debug
  JS::AutoFilename filename;
  unsigned lineno;
  // Computing the line number can be very expensive (see bug 1330231 for
  // example), and we don't use the line number anywhere except than in the
  // parent process, so we avoid computing it elsewhere.  This gives us most of
  // the wins we are interested in, since the source of the slowness here is
  // minified scripts which is more common in Web content that is loaded in the
  // content process.
  unsigned* linenop = XRE_IsParentProcess() ? &lineno : nullptr;
  bool hasFrame = JS::DescribeScriptedCaller(cx, &filename, linenop);

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
                                       filename.get());
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
  bool old = JS_DisableInterruptCallback(cx);

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, waitButton, stopButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  JS_ResetInterruptCallback(cx, old);

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
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
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

  // We need these for our private-browsing check below; so save them off.
  // (And we can't do the private-browsing enforcement check as part of our
  // outer conditional because otherwise we'll trigger an NS_WARNING if control
  // flow reaches the bottom of this method.)
  bool isNonPrivateLocalStorageChange =
    !nsCRT::strcmp(aTopic, "dom-storage2-changed");
  bool isPrivateLocalStorageChange =
    !nsCRT::strcmp(aTopic, "dom-private-storage2-changed");
  if (isNonPrivateLocalStorageChange || isPrivateLocalStorageChange) {
    // Enforce that the source storage area's private browsing state matches
    // this window's state.  These flag checks and their maintenance independent
    // from the principal's OriginAttributes matter because chrome docshells
    // that are part of private browsing windows can be private browsing without
    // having their OriginAttributes set (because they have the system
    // principal).
    bool isPrivateBrowsing = IsPrivateBrowsing();
    if ((isNonPrivateLocalStorageChange && isPrivateBrowsing) ||
        (isPrivateLocalStorageChange && !isPrivateBrowsing)) {
      return NS_OK;
    }

    // We require that aData be either u"SessionStorage" or u"localStorage".
    // Assert under debug, but ignore the bogus event under non-debug.
    MOZ_ASSERT(aData);
    if (!aData) {
      return NS_ERROR_INVALID_ARG;
    }

    // LocalStorage can only exist on an inner window, and we don't want to
    // generate events on frozen or otherwise-navigated-away from windows.
    // (Actually, this code used to try and buffer events for frozen windows,
    // but it never worked, so we've removed it.  See bug 1285898.)
    if (!IsInnerWindow() || !AsInner()->IsCurrentInnerWindow() || IsFrozen()) {
      return NS_OK;
    }

    nsIPrincipal *principal = GetPrincipal();
    if (!principal) {
      return NS_OK;
    }

    RefPtr<StorageEvent> event = static_cast<StorageEvent*>(aSubject);
    if (!event) {
      return NS_ERROR_FAILURE;
    }

    bool fireMozStorageChanged = false;
    nsAutoString eventType;
    eventType.AssignLiteral("storage");

    if (!NS_strcmp(aData, u"sessionStorage")) {
      nsCOMPtr<nsIDOMStorage> changingStorage = event->GetStorageArea();
      MOZ_ASSERT(changingStorage);

      bool check = false;

      nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(GetDocShell());
      if (storageManager) {
        nsresult rv = storageManager->CheckStorage(principal, changingStorage,
                                                   &check);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      if (!check) {
        // This storage event is not coming from our storage or is coming
        // from a different docshell, i.e. it is a clone, ignore this event.
        return NS_OK;
      }

      MOZ_LOG(gDOMLeakPRLog, LogLevel::Debug,
              ("nsGlobalWindow %p with sessionStorage %p passing event from %p",
               this, mSessionStorage.get(), changingStorage.get()));

      fireMozStorageChanged = mSessionStorage == changingStorage;
      if (fireMozStorageChanged) {
        eventType.AssignLiteral("MozSessionStorageChanged");
      }
    }

    else {
      MOZ_ASSERT(!NS_strcmp(aData, u"localStorage"));
      nsIPrincipal* storagePrincipal = event->GetPrincipal();
      if (!storagePrincipal) {
        return NS_OK;
      }

      bool equals = false;
      nsresult rv = storagePrincipal->Equals(principal, &equals);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!equals) {
        return NS_OK;
      }

      fireMozStorageChanged = mLocalStorage == event->GetStorageArea();

      if (fireMozStorageChanged) {
        eventType.AssignLiteral("MozLocalStorageChanged");
      }
    }

    // Clone the storage event included in the observer notification. We want
    // to dispatch clones rather than the original event.
    ErrorResult error;
    RefPtr<StorageEvent> clonedEvent =
      CloneStorageEvent(eventType, event, error);
    if (error.Failed()) {
      return error.StealNSResult();
    }

    clonedEvent->SetTrusted(true);

    if (fireMozStorageChanged) {
      WidgetEvent* internalEvent = clonedEvent->WidgetEventPtr();
      internalEvent->mFlags.mOnlyChromeDispatch = true;
    }

    bool defaultActionEnabled;
    DispatchEvent(clonedEvent, &defaultActionEnabled);

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

  RefPtr<Storage> storageArea = aEvent->GetStorageArea();

  RefPtr<Storage> storage;

  // If null, this is a localStorage event received by IPC.
  if (!storageArea) {
    storage = GetLocalStorage(aRv);
    if (aRv.Failed() || !storage) {
      return nullptr;
    }

    // We must apply the current change to the 'local' localStorage.
    storage->ApplyEvent(aEvent);
  } else if (storageArea->GetType() == Storage::LocalStorage) {
    storage = GetLocalStorage(aRv);
  } else {
    MOZ_ASSERT(storageArea->GetType() == Storage::SessionStorage);
    storage = GetSessionStorage(aRv);
  }

  if (aRv.Failed() || !storage) {
    return nullptr;
  }

  MOZ_ASSERT(storage);
  MOZ_ASSERT_IF(storageArea, storage->IsForkOf(storageArea));

  dict.mStorageArea = storage;

  RefPtr<StorageEvent> event = StorageEvent::Constructor(this, aType, dict);
  return event.forget();
}

void
nsGlobalWindow::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());

  // We can only safely suspend windows that are the current inner window.  If
  // its not the current inner, then we are in one of two different cases.
  // Either we are in the bfcache or we are doomed window that is going away.
  // When a window becomes inactive we purposely avoid placing already suspended
  // windows into the bfcache.  It only expects windows suspended due to the
  // Freeze() method which occurs while the window is still the current inner.
  // So we must not call Suspend() on bfcache windows at this point or this
  // invariant will be broken.  If the window is doomed there is no point in
  // suspending it since it will soon be gone.
  if (!AsInner()->IsCurrentInnerWindow()) {
    return;
  }

  // All children are also suspended.  This ensure mSuspendDepth is
  // set properly and the timers are properly canceled for each child.
  CallOnChildren(&nsGlobalWindow::Suspend);

  mSuspendDepth += 1;
  if (mSuspendDepth != 1) {
    return;
  }

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac) {
    for (uint32_t i = 0; i < mEnabledSensors.Length(); i++)
      ac->RemoveWindowListener(mEnabledSensors[i], this);
  }
  DisableGamepadUpdates();
  DisableVRUpdates();

  mozilla::dom::workers::SuspendWorkersForWindow(AsInner());

  SuspendIdleRequests();

  mTimeoutManager->Suspend();

  // Suspend all of the AudioContexts for this window
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    ErrorResult dummy;
    RefPtr<Promise> d = mAudioContexts[i]->Suspend(dummy);
  }
}

void
nsGlobalWindow::Resume()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());

  // We can only safely resume a window if its the current inner window.  If
  // its not the current inner, then we are in one of two different cases.
  // Either we are in the bfcache or we are doomed window that is going away.
  // If a window is suspended when it becomes inactive we purposely do not
  // put it in the bfcache, so Resume should never be needed in that case.
  // If the window is doomed then there is no point in resuming it.
  if (!AsInner()->IsCurrentInnerWindow()) {
    return;
  }

  // Resume all children.  This restores timers recursively canceled
  // in Suspend() and ensures all children have the correct mSuspendDepth.
  CallOnChildren(&nsGlobalWindow::Resume);

  MOZ_ASSERT(mSuspendDepth != 0);
  mSuspendDepth -= 1;
  if (mSuspendDepth != 0) {
    return;
  }

  // We should not be able to resume a frozen window.  It must be Thaw()'d first.
  MOZ_ASSERT(mFreezeDepth == 0);

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

  mTimeoutManager->Resume();

  ResumeIdleRequests();

  // Resume all of the workers for this window.  We must do this
  // after timeouts since workers may have queued events that can trigger
  // a setTimeout().
  mozilla::dom::workers::ResumeWorkersForWindow(AsInner());
}

bool
nsGlobalWindow::IsSuspended() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // No inner means we are effectively suspended
  if (IsOuterWindow()) {
    if (!mInnerWindow) {
      return true;
    }
    return mInnerWindow->IsSuspended();
  }
  return mSuspendDepth != 0;
}

void
nsGlobalWindow::Freeze()
{
  MOZ_ASSERT(NS_IsMainThread());
  Suspend();
  FreezeInternal();
}

void
nsGlobalWindow::FreezeInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(AsInner()->IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  CallOnChildren(&nsGlobalWindow::FreezeInternal);

  mFreezeDepth += 1;
  MOZ_ASSERT(mSuspendDepth >= mFreezeDepth);
  if (mFreezeDepth != 1) {
    return;
  }

  mozilla::dom::workers::FreezeWorkersForWindow(AsInner());

  mTimeoutManager->Freeze();

  NotifyDOMWindowFrozen(this);
}

void
nsGlobalWindow::Thaw()
{
  MOZ_ASSERT(NS_IsMainThread());
  ThawInternal();
  Resume();
}

void
nsGlobalWindow::ThawInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(AsInner()->IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  CallOnChildren(&nsGlobalWindow::ThawInternal);

  MOZ_ASSERT(mFreezeDepth != 0);
  mFreezeDepth -= 1;
  MOZ_ASSERT(mSuspendDepth >= mFreezeDepth);
  if (mFreezeDepth != 0) {
    return;
  }

  mTimeoutManager->Thaw();

  mozilla::dom::workers::ThawWorkersForWindow(AsInner());

  NotifyDOMWindowThawed(this);
}

bool
nsGlobalWindow::IsFrozen() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // No inner means we are effectively frozen
  if (IsOuterWindow()) {
    if (!mInnerWindow) {
      return true;
    }
    return mInnerWindow->IsFrozen();
  }
  bool frozen = mFreezeDepth != 0;
  MOZ_ASSERT_IF(frozen, IsSuspended());
  return frozen;
}

void
nsGlobalWindow::SyncStateFromParentWindow()
{
  // This method should only be called on an inner window that has been
  // assigned to an outer window already.
  MOZ_ASSERT(IsInnerWindow());
  MOZ_ASSERT(AsInner()->IsCurrentInnerWindow());
  nsPIDOMWindowOuter* outer = GetOuterWindow();
  MOZ_ASSERT(outer);

  // Attempt to find our parent windows.
  nsCOMPtr<Element> frame = outer->GetFrameElementInternal();
  nsPIDOMWindowOuter* parentOuter = frame ? frame->OwnerDoc()->GetWindow()
                                          : nullptr;
  nsGlobalWindow* parentInner =
    parentOuter ? nsGlobalWindow::Cast(parentOuter->GetCurrentInnerWindow())
                : nullptr;

  // If our outer is in a modal state, but our parent is not in a modal
  // state, then we must apply the suspend directly.  If our parent is
  // in a modal state then we should get the suspend automatically
  // via the parentSuspendDepth application below.
  if ((!parentInner || !parentInner->IsInModalState()) && IsInModalState()) {
    Suspend();
  }

  uint32_t parentFreezeDepth = parentInner ? parentInner->mFreezeDepth : 0;
  uint32_t parentSuspendDepth = parentInner ? parentInner->mSuspendDepth : 0;

  // Since every Freeze() calls Suspend(), the suspend count must
  // be equal or greater to the freeze count.
  MOZ_ASSERT(parentFreezeDepth <= parentSuspendDepth);

  // First apply the Freeze() calls.
  for (uint32_t i = 0; i < parentFreezeDepth; ++i) {
    Freeze();
  }

  // Now apply only the number of Suspend() calls to reach the target
  // suspend count after applying the Freeze() calls.
  for (uint32_t i = 0; i < (parentSuspendDepth - parentFreezeDepth); ++i) {
    Suspend();
  }
}

template<typename Method>
void
nsGlobalWindow::CallOnChildren(Method aMethod)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsInnerWindow());
  MOZ_ASSERT(AsInner()->IsCurrentInnerWindow());

  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return;
  }

  int32_t childCount = 0;
  docShell->GetChildCount(&childCount);

  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childShell;
    docShell->GetChildAt(i, getter_AddRefs(childShell));
    NS_ASSERTION(childShell, "null child shell");

    nsCOMPtr<nsPIDOMWindowOuter> pWin = childShell->GetWindow();
    if (!pWin) {
      continue;
    }

    auto* win = nsGlobalWindow::Cast(pWin);
    nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();

    // This is a bit hackish. Only freeze/suspend windows which are truly our
    // subwindows.
    nsCOMPtr<Element> frame = pWin->GetFrameElementInternal();
    if (!mDoc || !frame || mDoc != frame->OwnerDoc() || !inner) {
      continue;
    }

    (inner->*aMethod)();
  }
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

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
    nsCOMPtr<nsIRunnable> caller = NewRunnableMethod(mWin, run);
    mWin->Dispatch("nsGlobalWindow::UnblockScriptedClosing",
                   TaskCategory::Other, caller.forget());
  }
};

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, bool aDialog,
                             bool aContentModal, bool aCalledNoScript,
                             bool aDoJSFixups, bool aNavigate,
                             nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsIDocShellLoadInfo* aLoadInfo,
                             bool aForceNoOpener,
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

  bool forceNoOpener = aForceNoOpener;
  if (!forceNoOpener) {
    // Unlike other window flags, "noopener" comes from splitting on commas with
    // HTML whitespace trimming...
    nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tok(
      aOptions, ',');
    while (tok.hasMoreTokens()) {
      if (tok.nextToken().EqualsLiteral("noopener")) {
        forceNoOpener = true;
        break;
      }
    }
  }

  // XXXbz When this gets fixed to not use LegacyIsCallerNativeCode()
  // (indirectly) maybe we can nix the AutoJSAPI usage OnLinkClickEvent::Run.
  // But note that if you change this to GetEntryGlobal(), say, then
  // OnLinkClickEvent::Run will need a full-blown AutoEntryScript.
  const bool checkForPopup = !nsContentUtils::LegacyIsCallerChromeOrNativeCode() &&
    !aDialog && !WindowExists(aName, forceNoOpener, !aCalledNoScript);

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

  MOZ_ASSERT_IF(checkForPopup, abuseLevel < openAbused);
  // At this point we should know for a fact that if checkForPopup then
  // abuseLevel < openAbused, so we could just check for abuseLevel ==
  // openControlled.  But let's be defensive just in case and treat anything
  // that fails the above assert as a spam popup too, if it ever happens.
  bool isPopupSpamWindow = checkForPopup && (abuseLevel >= openControlled);

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
                                isPopupSpamWindow,
                                forceNoOpener,
                                aLoadInfo,
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
        nojsapi.emplace();
      }

      rv = pwwatch->OpenWindow2(AsOuter(), url.get(), name_ptr,
                                options_ptr, /* aCalledFromScript = */ false,
                                aDialog, aNavigate, aExtraArgument,
                                isPopupSpamWindow,
                                forceNoOpener,
                                aLoadInfo,
                                getter_AddRefs(domReturn));

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
      Unused << doc;
    }
  }

  return rv;
}

//*****************************************************************************
// nsGlobalWindow: Timeout Functions
//*****************************************************************************

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
  aError = mTimeoutManager->SetTimeout(handler, aTimeout, aIsInterval,
                                      Timeout::Reason::eTimeoutOrInterval,
                                      &result);
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
  aError = mTimeoutManager->SetTimeout(handler, aTimeout, aIsInterval,
                                      Timeout::Reason::eTimeoutOrInterval,
                                      &result);
  return result;
}

bool
nsGlobalWindow::RunTimeoutHandler(Timeout* aTimeout,
                                  nsIScriptContext* aScx)
{
  MOZ_ASSERT(IsInnerWindow());

  // Hold on to the timeout in case mExpr or mFunObj releases its
  // doc.
  RefPtr<Timeout> timeout = aTimeout;
  Timeout* last_running_timeout = mTimeoutManager->BeginRunningTimeout(timeout);
  timeout->mRunning = true;

  // Push this timeout's popup control state, which should only be
  // eabled the first time a timeout fires that was created while
  // popups were enabled and with a delay less than
  // "dom.disable_open_click_delay".
  nsAutoPopupStatePusher popupStatePusher(timeout->mPopupState);

  // Clear the timeout's popup state, if any, to prevent interval
  // timeouts from repeatedly opening poups.
  timeout->mPopupState = openAbused;

  bool trackNestingLevel = !timeout->mIsInterval;
  uint32_t nestingLevel;
  if (trackNestingLevel) {
    nestingLevel = TimeoutManager::GetNestingLevel();
    TimeoutManager::SetNestingLevel(timeout->mNestingLevel);
  }

  const char *reason;
  if (timeout->mIsInterval) {
    reason = "setInterval handler";
  } else {
    reason = "setTimeout handler";
  }

  bool abortIntervalHandler = false;
  nsCOMPtr<nsIScriptTimeoutHandler> handler(do_QueryInterface(timeout->mScriptHandler));
  if (handler) {
    RefPtr<Function> callback = handler->GetCallback();

    if (!callback) {
      // Evaluate the timeout expression.
      const nsAString& script = handler->GetHandlerText();

      const char* filename = nullptr;
      uint32_t lineNo = 0, dummyColumn = 0;
      handler->GetLocation(&filename, &lineNo, &dummyColumn);

      // New script entry point required, due to the "Create a script" sub-step of
      // http://www.whatwg.org/specs/web-apps/current-work/#timer-initialisation-steps
      nsAutoMicroTask mt;
      AutoEntryScript aes(this, reason, true);
      JS::CompileOptions options(aes.cx());
      options.setFileAndLine(filename, lineNo).setVersion(JSVERSION_DEFAULT);
      options.setNoScriptRval(true);
      JS::Rooted<JSObject*> global(aes.cx(), FastGetGlobalJSObject());
      nsresult rv = NS_OK;
      {
        nsJSUtils::ExecutionContext exec(aes.cx(), global);
        rv = exec.CompileAndExec(options, script);
      }

      if (rv == NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE) {
        abortIntervalHandler = true;
      }
    } else {
      // Hold strong ref to ourselves while we call the callback.
      nsCOMPtr<nsISupports> me(static_cast<nsIDOMWindow*>(this));
      ErrorResult rv;
      JS::Rooted<JS::Value> ignoredVal(RootingCx());
      callback->Call(me, handler->GetArgs(), &ignoredVal, rv, reason);
      if (rv.IsUncatchableException()) {
        abortIntervalHandler = true;
      }

      rv.SuppressException();
    }
  } else {
    nsCOMPtr<nsITimeoutHandler> basicHandler(timeout->mScriptHandler);
    nsCOMPtr<nsISupports> kungFuDeathGrip(static_cast<nsIDOMWindow*>(this));
    mozilla::Unused << kungFuDeathGrip;
    basicHandler->Call();
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
    TimeoutManager::SetNestingLevel(nestingLevel);
  }

  mTimeoutManager->EndRunningTimeout(last_running_timeout);
  timeout->mRunning = false;

  return timeout->mCleared;
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
nsGlobalWindow::FlushPendingNotifications(FlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindow::EnsureSizeAndPositionUpToDate()
{
  MOZ_ASSERT(IsOuterWindow());

  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindow *parent = nsGlobalWindow::Cast(GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(FlushType::Layout);
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
  if (aType == nsGkAtoms::onvrdisplayactivate ||
      aType == nsGkAtoms::onvrdisplayconnect ||
      aType == nsGkAtoms::onvrdisplaydeactivate ||
      aType == nsGkAtoms::onvrdisplaydisconnect ||
      aType == nsGkAtoms::onvrdisplaypresentchange) {
    NotifyVREventListenerAdded();
  }

  // We need to initialize localStorage in order to receive notifications.
  if (aType == nsGkAtoms::onstorage) {
    ErrorResult rv;
    GetLocalStorage(rv);
    rv.SuppressException();
  }
}

void
nsGlobalWindow::NotifyVREventListenerAdded()
{
  MOZ_ASSERT(IsInnerWindow());
  mHasVREvents = true;
  EnableVRUpdates();
}

bool
nsGlobalWindow::HasUsedVR() const
{
  MOZ_ASSERT(IsInnerWindow());

  return mHasVREvents;
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

void
nsGlobalWindow::StopGamepadHaptics()
{
  MOZ_ASSERT(IsInnerWindow());
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    gamepadManager->StopHaptics();
  }
}

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

uint32_t
nsGlobalWindow::GetAutoActivateVRDisplayID()
{
  MOZ_ASSERT(IsOuterWindow());
  uint32_t retVal = mAutoActivateVRDisplayID;
  mAutoActivateVRDisplayID = 0;
  return retVal;
}

void
nsGlobalWindow::SetAutoActivateVRDisplayID(uint32_t aAutoActivateVRDisplayID)
{
  MOZ_ASSERT(IsOuterWindow());
  mAutoActivateVRDisplayID = aAutoActivateVRDisplayID;
}

void
nsGlobalWindow::DispatchVRDisplayActivate(uint32_t aDisplayID,
                                          mozilla::dom::VRDisplayEventReason aReason)
{
  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (auto display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      if (aReason != VRDisplayEventReason::Navigation &&
          display->IsAnyPresenting()) {
        // We only want to trigger this event if nobody is presenting to the
        // display already or when a page is loaded by navigating away
        // from a page with an active VR Presentation.
        continue;
      }

      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      init.mReason.Construct(aReason);

      RefPtr<VRDisplayEvent> event =
        VRDisplayEvent::Constructor(this,
                                    NS_LITERAL_STRING("vrdisplayactivate"),
                                    init);
      // vrdisplayactivate is a trusted event, allowing VRDisplay.requestPresent
      // to be used in response to link traversal, user request (chrome UX), and
      // HMD mounting detection sensors.
      event->SetTrusted(true);
      bool defaultActionEnabled;
      // VRDisplay.requestPresent normally requires a user gesture; however, an
      // exception is made to allow it to be called in response to vrdisplayactivate
      // during VR link traversal.
      display->StartHandlingVRNavigationEvent();
      Unused << DispatchEvent(event, &defaultActionEnabled);
      display->StopHandlingVRNavigationEvent();
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void
nsGlobalWindow::DispatchVRDisplayDeactivate(uint32_t aDisplayID,
                                            mozilla::dom::VRDisplayEventReason aReason)
{
  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (auto display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID && display->IsPresenting()) {
      // We only want to trigger this event to content that is presenting to
      // the display already.

      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      init.mReason.Construct(aReason);

      RefPtr<VRDisplayEvent> event =
        VRDisplayEvent::Constructor(this,
                                    NS_LITERAL_STRING("vrdisplaydeactivate"),
                                    init);
      event->SetTrusted(true);
      bool defaultActionEnabled;
      Unused << DispatchEvent(event, &defaultActionEnabled);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void
nsGlobalWindow::DispatchVRDisplayConnect(uint32_t aDisplayID)
{
  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (auto display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplayconnect

      RefPtr<VRDisplayEvent> event =
        VRDisplayEvent::Constructor(this,
                                    NS_LITERAL_STRING("vrdisplayconnect"),
                                    init);
      event->SetTrusted(true);
      bool defaultActionEnabled;
      Unused << DispatchEvent(event, &defaultActionEnabled);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void
nsGlobalWindow::DispatchVRDisplayDisconnect(uint32_t aDisplayID)
{
  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (auto display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplaydisconnect

      RefPtr<VRDisplayEvent> event =
        VRDisplayEvent::Constructor(this,
                                    NS_LITERAL_STRING("vrdisplaydisconnect"),
                                    init);
      event->SetTrusted(true);
      bool defaultActionEnabled;
      Unused << DispatchEvent(event, &defaultActionEnabled);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void
nsGlobalWindow::DispatchVRDisplayPresentChange(uint32_t aDisplayID)
{
  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (auto display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplaypresentchange

      RefPtr<VRDisplayEvent> event =
        VRDisplayEvent::Constructor(this,
                                    NS_LITERAL_STRING("vrdisplaypresentchange"),
                                    init);
      event->SetTrusted(true);
      bool defaultActionEnabled;
      Unused << DispatchEvent(event, &defaultActionEnabled);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

// nsGlobalChromeWindow implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalChromeWindow)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGlobalChromeWindow,
                                                  nsGlobalWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserDOMWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGroupMessageManagers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOpenerForInitialContentBrowser)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOpenerForInitialContentBrowser)
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
    if (!nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
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
    LayoutDeviceIntRect::FromAppUnitsToNearest(
      frame->GetScreenRectInAppUnits(),
      frame->PresContext()->AppUnitsPerDevPixel());

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

nsresult
nsGlobalChromeWindow::SetOpenerForInitialContentBrowser(mozIDOMWindowProxy* aOpenerWindow)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(!mOpenerForInitialContentBrowser);
  mOpenerForInitialContentBrowser = aOpenerWindow;
  return NS_OK;
}

nsresult
nsGlobalChromeWindow::TakeOpenerForInitialContentBrowser(mozIDOMWindowProxy** aOpenerWindow)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  // Intentionally forget our own member
  mOpenerForInitialContentBrowser.forget(aOpenerWindow);
  return NS_OK;
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
                                        nsIPrincipal& aSubjectPrincipal,
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
  mDialogArguments->Get(aCx, wrapper, &aSubjectPrincipal, aRetval, aError);
}

void
nsGlobalWindow::GetDialogArguments(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aRetval,
                                   nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDialogArgumentsOuter,
                            (aCx, aRetval, aSubjectPrincipal, aError),
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
  mWasOffline = NS_IsOffline();
}

void
nsGlobalWindow::GetReturnValueOuter(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aReturnValue,
                                    nsIPrincipal& aSubjectPrincipal,
                                    ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());
  MOZ_ASSERT(IsModalContentWindow(),
             "This should only be called on modal windows!");

  if (mReturnValue) {
    JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
    JSAutoCompartment ac(aCx, wrapper);
    mReturnValue->Get(aCx, wrapper, &aSubjectPrincipal, aReturnValue, aError);
  } else {
    aReturnValue.setUndefined();
  }
}

void
nsGlobalWindow::GetReturnValue(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aReturnValue,
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetReturnValueOuter,
                            (aCx, aReturnValue, aSubjectPrincipal, aError),
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
                                    nsIPrincipal& aSubjectPrincipal,
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
    mReturnValue = new DialogValueHolder(&aSubjectPrincipal, returnValue);
  }
}

void
nsGlobalWindow::SetReturnValue(JSContext* aCx,
                               JS::Handle<JS::Value> aReturnValue,
                               nsIPrincipal& aSubjectPrincipal,
                               ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetReturnValueOuter,
                            (aCx, aReturnValue, aSubjectPrincipal, aError),
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
nsGlobalWindow::Orientation(CallerType aCallerType) const
{
  return nsContentUtils::ResistFingerprinting(aCallerType) ?
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

bool
nsGlobalWindow::IsSecureContextIfOpenerIgnored() const
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  return mIsSecureContextIfOpenerIgnored;
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

void
nsPIDOMWindowOuter::SetLargeAllocStatus(LargeAllocStatus aStatus)
{
  MOZ_ASSERT(mLargeAllocStatus == LargeAllocStatus::NONE);
  mLargeAllocStatus = aStatus;
}

bool
nsPIDOMWindowOuter::IsTopLevelWindow()
{
  return nsGlobalWindow::Cast(this)->IsTopLevelWindow();
}

bool
nsPIDOMWindowOuter::HadOriginalOpener() const
{
  return nsGlobalWindow::Cast(this)->HadOriginalOpener();
}

void
nsGlobalWindow::ReportLargeAllocStatus()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  uint32_t errorFlags = nsIScriptError::warningFlag;
  const char* message = nullptr;

  switch (mLargeAllocStatus) {
    case LargeAllocStatus::SUCCESS:
      // Override the error flags such that the success message isn't reported
      // as a warning.
      errorFlags = nsIScriptError::infoFlag;
      message = "LargeAllocationSuccess";
      break;
    case LargeAllocStatus::NON_WIN32:
      errorFlags = nsIScriptError::infoFlag;
      message = "LargeAllocationNonWin32";
      break;
    case LargeAllocStatus::NON_GET:
      message = "LargeAllocationNonGetRequest";
      break;
    case LargeAllocStatus::NON_E10S:
      message = "LargeAllocationNonE10S";
      break;
    case LargeAllocStatus::NOT_ONLY_TOPLEVEL_IN_TABGROUP:
      message = "LargeAllocationNotOnlyToplevelInTabGroup";
      break;
    default: // LargeAllocStatus::NONE
      return; // Don't report a message to the console
  }

  nsContentUtils::ReportToConsole(errorFlags,
                                  NS_LITERAL_CSTRING("DOM"),
                                  mDoc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  message);
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
                                          CallerType aCallerType,
                                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  int32_t coord = (this->*aGetter)(aCallerType, aError);
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
                                          CallerType aCallerType,
                                          ErrorResult& aError)
{
  MOZ_ASSERT(IsInnerWindow());

  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * just treat this the way we would an IDL replaceable property.
   */
  nsGlobalWindow* outer = GetOuterWindowInternal();
  if (!outer ||
      !outer->CanMoveResizeWindows(aCallerType) ||
      outer->IsFrame()) {
    RedefineProperty(aCx, aPropName, aValue, aError);
    return;
  }

  int32_t value;
  if (!ValueToPrimitive<int32_t, eDefault>(aCx, aValue, &value)) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (nsContentUtils::ShouldResistFingerprinting(GetDocShell())) {
    bool innerWidthSpecified = false;
    bool innerHeightSpecified = false;
    bool outerWidthSpecified = false;
    bool outerHeightSpecified = false;

    if (strcmp(aPropName, "innerWidth") == 0) {
      innerWidthSpecified = true;
    } else if (strcmp(aPropName, "innerHeight") == 0) {
      innerHeightSpecified = true;
    } else if (strcmp(aPropName, "outerWidth") == 0) {
      outerWidthSpecified = true;
    } else if (strcmp(aPropName, "outerHeight") == 0) {
      outerHeightSpecified = true;
    }

    if (innerWidthSpecified || innerHeightSpecified ||
        outerWidthSpecified || outerHeightSpecified)
    {
      nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = outer->GetTreeOwnerWindow();
      nsCOMPtr<nsIScreen> screen;
      nsCOMPtr<nsIScreenManager> screenMgr(
        do_GetService("@mozilla.org/gfx/screenmanager;1"));
      int32_t winLeft   = 0;
      int32_t winTop    = 0;
      int32_t winWidth  = 0;
      int32_t winHeight = 0;
      double scale = 1.0;


      if (treeOwnerAsWin && screenMgr) {
        // Acquire current window size.
        treeOwnerAsWin->GetUnscaledDevicePixelsPerCSSPixel(&scale);
        treeOwnerAsWin->GetPositionAndSize(&winLeft, &winTop, &winWidth, &winHeight);
        winLeft = NSToIntRound(winHeight / scale);
        winTop = NSToIntRound(winWidth / scale);
        winWidth = NSToIntRound(winWidth / scale);
        winHeight = NSToIntRound(winHeight / scale);

        // Acquire content window size.
        CSSIntSize contentSize;
        outer->GetInnerSize(contentSize);

        screenMgr->ScreenForRect(winLeft, winTop, winWidth, winHeight,
                                 getter_AddRefs(screen));

        if (screen) {
          int32_t* targetContentWidth  = nullptr;
          int32_t* targetContentHeight = nullptr;
          int32_t screenWidth  = 0;
          int32_t screenHeight = 0;
          int32_t chromeWidth  = 0;
          int32_t chromeHeight = 0;
          int32_t inputWidth   = 0;
          int32_t inputHeight  = 0;
          int32_t unused = 0;

          // Get screen dimensions (in device pixels)
          screen->GetAvailRect(&unused, &unused, &screenWidth,
                               &screenHeight);
          // Convert them to CSS pixels
          screenWidth = NSToIntRound(screenWidth / scale);
          screenHeight = NSToIntRound(screenHeight / scale);

          // Calculate the chrome UI size.
          chromeWidth = winWidth - contentSize.width;
          chromeHeight = winHeight - contentSize.height;

          if (innerWidthSpecified || outerWidthSpecified) {
            inputWidth = value;
            targetContentWidth = &value;
            targetContentHeight = &unused;
          } else if (innerHeightSpecified || outerHeightSpecified) {
            inputHeight = value;
            targetContentWidth = &unused;
            targetContentHeight = &value;
          }

          nsContentUtils::CalcRoundedWindowSizeForResistingFingerprinting(
            chromeWidth,
            chromeHeight,
            screenWidth,
            screenHeight,
            inputWidth,
            inputHeight,
            outerWidthSpecified,
            outerHeightSpecified,
            targetContentWidth,
            targetContentHeight
          );
        }
      }
    }
  }

  (this->*aSetter)(value, aCallerType, aError);
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
nsGlobalWindow::CreateImageBitmap(JSContext* aCx,
                                  const ImageBitmapSource& aImage,
                                  ErrorResult& aRv)
{
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  return ImageBitmap::Create(this, aImage, Nothing(), aRv);
}

already_AddRefed<Promise>
nsGlobalWindow::CreateImageBitmap(JSContext* aCx,
                                  const ImageBitmapSource& aImage,
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
nsGlobalWindow::CreateImageBitmap(JSContext* aCx,
                                  const ImageBitmapSource& aImage,
                                  int32_t aOffset, int32_t aLength,
                                  ImageBitmapFormat aFormat,
                                  const Sequence<ChannelPixelLayout>& aLayout,
                                  ErrorResult& aRv)
{
  if (!ImageBitmap::ExtensionsEnabled(aCx)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }
  if (aImage.IsArrayBuffer() || aImage.IsArrayBufferView()) {
    return ImageBitmap::Create(this, aImage, aOffset, aLength, aFormat, aLayout,
                               aRv);
  }
  aRv.Throw(NS_ERROR_TYPE_ERR);
  return nullptr;
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

mozilla::dom::TabGroup*
nsGlobalWindow::TabGroupOuter()
{
  MOZ_RELEASE_ASSERT(IsOuterWindow());

  // Outer windows lazily join TabGroups when requested. This is usually done
  // because a document is getting its NodePrincipal, and asking for the
  // TabGroup to determine its DocGroup.
  if (!mTabGroup) {
    // Get mOpener ourselves, instead of relying on GetOpenerWindowOuter,
    // because that way we dodge the LegacyIsCallerChromeOrNativeCode() call
    // which we want to return false.
    nsCOMPtr<nsPIDOMWindowOuter> piOpener = do_QueryReferent(mOpener);
    nsPIDOMWindowOuter* opener = GetSanitizedOpener(piOpener);
    nsPIDOMWindowOuter* parent = GetScriptableParentOrNull();
    MOZ_ASSERT(!parent || !opener, "Only one of parent and opener may be provided");

    mozilla::dom::TabGroup* toJoin = nullptr;
    if (GetDocShell()->ItemType() == nsIDocShellTreeItem::typeChrome) {
      toJoin = TabGroup::GetChromeTabGroup();
    } else if (opener) {
      toJoin = opener->TabGroup();
    } else if (parent) {
      toJoin = parent->TabGroup();
    } else {
      toJoin = TabGroup::GetFromWindow(AsOuter());
    }

#ifdef DEBUG
    // Make sure that, if we have a tab group from the actor, it matches the one
    // we're planning to join.
    mozilla::dom::TabGroup* testGroup = TabGroup::GetFromWindow(AsOuter());
    MOZ_ASSERT_IF(testGroup, testGroup == toJoin);
#endif

    mTabGroup = mozilla::dom::TabGroup::Join(AsOuter(), toJoin);
  }
  MOZ_ASSERT(mTabGroup);

#ifdef DEBUG
  // Ensure that we don't recurse forever
  if (!mIsValidatingTabGroup) {
    mIsValidatingTabGroup = true;
    // We only need to do this check if we aren't in the chrome tab group
    if (mIsChrome) {
      MOZ_ASSERT(mTabGroup == TabGroup::GetChromeTabGroup());
    } else {
      // Sanity check that our tabgroup matches our opener or parent.
      RefPtr<nsPIDOMWindowOuter> parent = GetScriptableParentOrNull();
      MOZ_ASSERT_IF(parent, parent->TabGroup() == mTabGroup);
      nsCOMPtr<nsPIDOMWindowOuter> piOpener = do_QueryReferent(mOpener);
      nsPIDOMWindowOuter* opener = GetSanitizedOpener(piOpener);
      MOZ_ASSERT_IF(opener && Cast(opener) != this, opener->TabGroup() == mTabGroup);
    }
    mIsValidatingTabGroup = false;
  }
#endif

  return mTabGroup;
}

mozilla::dom::TabGroup*
nsGlobalWindow::TabGroupInner()
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  // If we don't have a TabGroup yet, try to get it from the outer window and
  // cache it.
  if (!mTabGroup) {
    nsGlobalWindow* outer = GetOuterWindowInternal();
    // This will never be called without either an outer window, or a cached tab group.
    // This is because of the following:
    // * This method is only called on inner windows
    // * This method is called as a document is attached to it's script global
    //   by the document
    // * Inner windows are created in nsGlobalWindow::SetNewDocument, which
    //   immediately sets a document, which will call this method, causing
    //   the TabGroup to be cached.
    MOZ_RELEASE_ASSERT(outer, "Inner window without outer window has no cached tab group!");
    mTabGroup = outer->TabGroup();
  }
  MOZ_ASSERT(mTabGroup);

#ifdef DEBUG
  nsGlobalWindow* outer = GetOuterWindowInternal();
  MOZ_ASSERT_IF(outer, outer->TabGroup() == mTabGroup);
#endif

  return mTabGroup;
}

template<typename T>
mozilla::dom::TabGroup*
nsPIDOMWindow<T>::TabGroup()
{
  nsGlobalWindow* globalWindow =
    static_cast<nsGlobalWindow*>(
        reinterpret_cast<nsPIDOMWindow<nsISupports>*>(this));

  if (IsInnerWindow()) {
    return globalWindow->TabGroupInner();
  }
  return globalWindow->TabGroupOuter();
}

template<typename T>
mozilla::dom::DocGroup*
nsPIDOMWindow<T>::GetDocGroup() const
{
  nsIDocument* doc = GetExtantDoc();
  if (doc) {
    return doc->GetDocGroup();
  }
  return nullptr;
}

nsresult
nsGlobalWindow::Dispatch(const char* aName,
                         TaskCategory aCategory,
                         already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->Dispatch(aName, aCategory, Move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aName, aCategory, Move(aRunnable));
}

nsIEventTarget*
nsGlobalWindow::EventTargetFor(TaskCategory aCategory) const
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread*
nsGlobalWindow::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
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

Worklet*
nsGlobalWindow::GetAudioWorklet(ErrorResult& aRv)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mAudioWorklet) {
    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mAudioWorklet = new Worklet(AsInner(), principal, Worklet::eAudioWorklet);
  }

  return mAudioWorklet;
}

Worklet*
nsGlobalWindow::GetPaintWorklet(ErrorResult& aRv)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mPaintWorklet) {
    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mPaintWorklet = new Worklet(AsInner(), principal, Worklet::ePaintWorklet);
  }

  return mPaintWorklet;
}

void
nsGlobalWindow::GetAppLocalesAsBCP47(nsTArray<nsString>& aLocales)
{
  nsTArray<nsCString> appLocales;
  mozilla::intl::LocaleService::GetInstance()->GetAppLocalesAsBCP47(appLocales);

  for (uint32_t i = 0; i < appLocales.Length(); i++) {
    aLocales.AppendElement(NS_ConvertUTF8toUTF16(appLocales[i]));
  }
}

#ifdef ENABLE_INTL_API
IntlUtils*
nsGlobalWindow::GetIntlUtils(ErrorResult& aError)
{
  MOZ_RELEASE_ASSERT(IsInnerWindow());

  if (!mIntlUtils) {
    mIntlUtils = new IntlUtils(AsInner());
  }

  return mIntlUtils;
}
#endif

template class nsPIDOMWindow<mozIDOMWindowProxy>;
template class nsPIDOMWindow<mozIDOMWindow>;
template class nsPIDOMWindow<nsISupports>;
