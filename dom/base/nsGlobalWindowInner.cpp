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
#include "mozilla/dom/CallbackDebuggerNotification.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/DebuggerNotification.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/LocalStorage.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/LSObject.h"
#include "mozilla/dom/PartitionedLocalStorage.h"
#include "mozilla/dom/Storage.h"
#include "mozilla/dom/IdleRequest.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/StorageNotifierService.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/Timeout.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/dom/VisualViewport.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Result.h"
#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/dom/WindowOrientationObserver.h"
#endif
#include "nsDOMOfflineResourceList.h"
#include "nsICookieService.h"
#include "nsError.h"
#include "nsISizeOfEventTarget.h"
#include "nsDOMJSUtils.h"
#include "nsArrayUtils.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocumentLoader.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptContext.h"
#include "nsIController.h"
#include "nsISlowScriptDebug.h"
#include "nsWindowMemoryReporter.h"
#include "nsWindowSizes.h"
#include "WindowNamedPropertiesHandler.h"
#include "nsFrameSelection.h"
#include "nsNetUtil.h"
#include "nsVariant.h"
#include "nsPrintfCString.h"
#include "mozilla/intl/LocaleService.h"
#include "WindowDestroyedEvent.h"

// Helper Classes
#include "nsJSUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Warnings.h"  // JS::WarnASCII
#include "js/Wrapper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsReadableUtils.h"
#include "nsJSEnvironment.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/Unused.h"

// Other Classes
#include "mozilla/dom/BarProps.h"
#include "nsContentCID.h"
#include "nsLayoutStatics.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSPrincipals.h"
#include "mozilla/Attributes.h"
#include "mozilla/Debug.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/ThrottledEventQueue.h"
#include "AudioChannelService.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharTraits.h"  // NS_IS_HIGH/LOW_SURROGATE
#include "PostMessageEvent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "PaintWorkletImpl.h"

// Interfaces Needed
#include "nsIFrame.h"
#include "nsCanvasFrame.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"
#include "nsIBaseWindow.h"
#include "nsIDeviceSensors.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "Crypto.h"
#include "nsDOMString.h"
#include "nsThreadUtils.h"
#include "nsILoadContext.h"
#include "nsIScrollableFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIPrompt.h"
#include "nsIAddonPolicyService.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserChrome.h"
#include "nsDOMCID.h"
#include "nsDOMWindowUtils.h"
#include "nsIControllers.h"
#include "nsGlobalWindowCommands.h"
#include "nsQueryObject.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"
#include "nsITimedChannel.h"
#include "nsServiceManagerUtils.h"
#ifdef MOZ_XUL
#  include "nsIDOMXULControlElement.h"
#  include "nsMenuPopupFrame.h"
#endif
#include "mozilla/dom/CustomEvent.h"
#include "nsIScreenManager.h"
#include "nsICSSDeclaration.h"

#include "xpcprivate.h"

#ifdef NS_PRINTING
#  include "nsIPrintSettings.h"
#  include "nsIPrintSettingsService.h"
#  include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"

#include "nsIDragService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsFrameLoader.h"
#include "nsXPCOMCID.h"
#include "mozilla/Logging.h"
#include "prenv.h"

#include "mozilla/dom/IDBFactory.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/Promise.h"

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadManager.h"

#include "gfxVR.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/VRDisplayEvent.h"
#include "mozilla/dom/VRDisplayEventBinding.h"
#include "mozilla/dom/VREventObserver.h"
#include "mozilla/dom/XRPermissionRequest.h"

#include "nsRefreshDriver.h"
#include "Layers.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Location.h"
#include "nsHTMLDocument.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "prrng.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/BrowserElementDictionariesBinding.h"
#include "mozilla/dom/cache/CacheStorage.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/HashChangeEvent.h"
#include "mozilla/dom/IntlUtils.h"
#include "mozilla/dom/PopStateEvent.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/PopupBlockedEvent.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsIBrowserChild.h"
#include "mozilla/dom/LoadedScript.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/InstallTriggerBinding.h"
#include "mozilla/dom/Report.h"
#include "mozilla/dom/ReportingObserver.h"
#include "mozilla/dom/SharedWorker.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "mozilla/dom/U2F.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/Worklet.h"
#ifdef HAVE_SIDEBAR
#  include "mozilla/dom/ExternalBinding.h"
#endif

#ifdef MOZ_WEBSPEECH
#  include "mozilla/dom/SpeechSynthesis.h"
#endif

#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/ClientState.h"

#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/BrowserChild.h"

#include "mozilla/net/CookieJarSettings.h"

#include "AccessCheck.h"
#include "SessionStorageCache.h"

#ifdef ANDROID
#  include <android/log.h>
#endif

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>  // for getpid()
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::dom::cache::CacheStorage;

#define FORWARD_TO_OUTER(method, args, err_rval)                     \
  PR_BEGIN_MACRO                                                     \
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();             \
  if (!HasActiveDocument()) {                                        \
    NS_WARNING(outer ? "Inner window does not have active document." \
                     : "No outer window available!");                \
    return err_rval;                                                 \
  }                                                                  \
  return outer->method args;                                         \
  PR_END_MACRO

static nsGlobalWindowOuter* GetOuterWindowForForwarding(
    nsGlobalWindowInner* aInner, ErrorResult& aError) {
  nsGlobalWindowOuter* outer = aInner->GetOuterWindowInternal();
  if (MOZ_LIKELY(aInner->HasActiveDocument())) {
    return outer;
  }
  if (!outer) {
    NS_WARNING("No outer window available!");
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
  } else {
    aError.Throw(NS_ERROR_XPC_SECURITY_MANAGER_VETO);
  }
  return nullptr;
}

#define FORWARD_TO_OUTER_OR_THROW(method, args, errorresult, err_rval)         \
  PR_BEGIN_MACRO                                                               \
  nsGlobalWindowOuter* outer = GetOuterWindowForForwarding(this, errorresult); \
  if (MOZ_LIKELY(outer)) {                                                     \
    return outer->method args;                                                 \
  }                                                                            \
  return err_rval;                                                             \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                          \
  PR_BEGIN_MACRO                                                     \
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();             \
  if (!HasActiveDocument()) {                                        \
    NS_WARNING(outer ? "Inner window does not have active document." \
                     : "No outer window available!");                \
    return;                                                          \
  }                                                                  \
  outer->method args;                                                \
  return;                                                            \
  PR_END_MACRO

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"
#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"

// Amount of time allowed between alert/prompt/confirm before enabling
// the stop dialog checkbox.
#define DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT 3  // 3 sec

// Maximum number of successive dialogs before we prompt users to disable
// dialogs for this window.
#define MAX_SUCCESSIVE_DIALOG_COUNT 5

// Max number of Report objects
#define MAX_REPORT_RECORDS 100

static LazyLogModule gDOMLeakPRLogInner("DOMLeakInner");
extern mozilla::LazyLogModule gTimeoutLog;

#ifdef DEBUG
static LazyLogModule gDocShellAndDOMWindowLeakLogging(
    "DocShellAndDOMWindowLeak");
#endif

static FILE* gDumpFile = nullptr;

nsGlobalWindowInner::InnerWindowByIdTable*
    nsGlobalWindowInner::sInnerWindowsById = nullptr;

bool nsGlobalWindowInner::sDragServiceDisabled = false;
bool nsGlobalWindowInner::sMouseDown = false;

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver final : public nsIObserver,
                                     public nsIInterfaceRequestor,
                                     public StorageNotificationObserver {
 public:
  explicit nsGlobalWindowObserver(nsGlobalWindowInner* aWindow)
      : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (!mWindow) return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nullptr; }
  NS_IMETHOD GetInterface(const nsIID& aIID, void** aResult) override {
    if (mWindow && aIID.Equals(NS_GET_IID(nsIDOMWindow)) && mWindow) {
      return mWindow->QueryInterface(aIID, aResult);
    }
    return NS_NOINTERFACE;
  }

  void ObserveStorageNotification(StorageEvent* aEvent,
                                  const char16_t* aStorageType,
                                  bool aPrivateBrowsing) override {
    if (mWindow) {
      mWindow->ObserveStorageNotification(aEvent, aStorageType,
                                          aPrivateBrowsing);
    }
  }

  nsIPrincipal* GetEffectiveStoragePrincipal() const override {
    return mWindow ? mWindow->GetEffectiveStoragePrincipal() : nullptr;
  }

  bool IsPrivateBrowsing() const override {
    return mWindow ? mWindow->IsPrivateBrowsing() : false;
  }

  nsIEventTarget* GetEventTarget() const override {
    return mWindow ? mWindow->EventTargetFor(TaskCategory::Other) : nullptr;
  }

 private:
  ~nsGlobalWindowObserver() = default;

  // This reference is non-owning and safe because it's cleared by
  // nsGlobalWindowInner::FreeInnerObjects().
  nsGlobalWindowInner* MOZ_NON_OWNING_REF mWindow;
};

NS_IMPL_ISUPPORTS(nsGlobalWindowObserver, nsIObserver, nsIInterfaceRequestor)

class IdleRequestExecutor;

class IdleRequestExecutorTimeoutHandler final : public TimeoutHandler {
 public:
  explicit IdleRequestExecutorTimeoutHandler(IdleRequestExecutor* aExecutor)
      : mExecutor(aExecutor) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(IdleRequestExecutorTimeoutHandler)

  bool Call(const char* /* unused */) override;

 private:
  ~IdleRequestExecutorTimeoutHandler() override = default;
  RefPtr<IdleRequestExecutor> mExecutor;
};

NS_IMPL_CYCLE_COLLECTION(IdleRequestExecutorTimeoutHandler, mExecutor)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IdleRequestExecutorTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IdleRequestExecutorTimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestExecutorTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class IdleRequestExecutor final : public nsIRunnable,
                                  public nsICancelableRunnable,
                                  public nsINamed,
                                  public nsIIdleRunnable {
 public:
  explicit IdleRequestExecutor(nsGlobalWindowInner* aWindow)
      : mDispatched(false), mDeadline(TimeStamp::Now()), mWindow(aWindow) {
    MOZ_DIAGNOSTIC_ASSERT(mWindow);

    mIdlePeriodLimit = {mDeadline, mWindow->LastIdleRequestHandle()};
    mDelayedExecutorDispatcher = new IdleRequestExecutorTimeoutHandler(this);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IdleRequestExecutor, nsIRunnable)

  NS_DECL_NSIRUNNABLE
  NS_DECL_NSINAMED
  nsresult Cancel() override;
  void SetDeadline(TimeStamp aDeadline) override;

  bool IsCancelled() const { return !mWindow || mWindow->IsDying(); }
  // Checks if aRequest shouldn't execute in the current idle period
  // since it has been queued from a chained call to
  // requestIdleCallback from within a running idle callback.
  bool IneligibleForCurrentIdlePeriod(IdleRequest* aRequest) const {
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
  struct IdlePeriodLimit {
    TimeStamp mEndOfIdlePeriod;
    uint32_t mLastRequestIdInIdlePeriod;
  };

  void DelayedDispatch(uint32_t aDelay);

  ~IdleRequestExecutor() override = default;

  bool mDispatched;
  TimeStamp mDeadline;
  IdlePeriodLimit mIdlePeriodLimit;
  RefPtr<nsGlobalWindowInner> mWindow;
  // The timeout handler responsible for dispatching this executor in
  // the case of immediate dispatch to the idle queue isn't
  // desirable. This is used if we've dispatched all idle callbacks
  // that are allowed to run in the current idle period, or if the
  // associated window is currently in the background.
  RefPtr<TimeoutHandler> mDelayedExecutorDispatcher;
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
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsIIdleRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
IdleRequestExecutor::GetName(nsACString& aName) {
  aName.AssignLiteral("IdleRequestExecutor");
  return NS_OK;
}

// MOZ_CAN_RUN_SCRIPT_BOUNDARY until nsIRunnable::Run is MOZ_CAN_RUN_SCRIPT.
// See bug 1535398.
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP IdleRequestExecutor::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  mDispatched = false;
  if (mWindow) {
    RefPtr<nsGlobalWindowInner> window(mWindow);
    window->ExecuteIdleRequest(mDeadline);
  }

  return NS_OK;
}

nsresult IdleRequestExecutor::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mDelayedExecutorHandle && mWindow) {
    mWindow->TimeoutManager().ClearTimeout(
        mDelayedExecutorHandle.value(), Timeout::Reason::eIdleCallbackTimeout);
  }

  mWindow = nullptr;
  return NS_OK;
}

void IdleRequestExecutor::SetDeadline(TimeStamp aDeadline) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow) {
    return;
  }

  mDeadline = aDeadline;
}

void IdleRequestExecutor::MaybeUpdateIdlePeriodLimit() {
  if (TimeStamp::Now() > mIdlePeriodLimit.mEndOfIdlePeriod) {
    mIdlePeriodLimit = {mDeadline, mWindow->LastIdleRequestHandle()};
  }
}

void IdleRequestExecutor::MaybeDispatch(TimeStamp aDelayUntil) {
  // If we've already dispatched the executor we don't want to do it
  // again. Also, if we've called IdleRequestExecutor::Cancel mWindow
  // will be null, which indicates that we shouldn't dispatch this
  // executor either.
  if (mDispatched || IsCancelled()) {
    return;
  }

  mDispatched = true;

  nsPIDOMWindowOuter* outer = mWindow->GetOuterWindow();
  if (outer && outer->IsBackground()) {
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

void IdleRequestExecutor::ScheduleDispatch() {
  MOZ_ASSERT(mWindow);
  mDelayedExecutorHandle = Nothing();
  RefPtr<IdleRequestExecutor> request = this;
  NS_DispatchToCurrentThreadQueue(request.forget(), EventQueuePriority::Idle);
}

void IdleRequestExecutor::DelayedDispatch(uint32_t aDelay) {
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mDelayedExecutorHandle.isNothing());
  int32_t handle;
  mWindow->TimeoutManager().SetTimeout(
      mDelayedExecutorDispatcher, aDelay, false,
      Timeout::Reason::eIdleCallbackTimeout, &handle);
  mDelayedExecutorHandle = Some(handle);
}

bool IdleRequestExecutorTimeoutHandler::Call(const char* /* unused */) {
  if (!mExecutor->IsCancelled()) {
    mExecutor->ScheduleDispatch();
  }
  return true;
}

void nsGlobalWindowInner::ScheduleIdleRequestDispatch() {
  AssertIsOnMainThread();

  if (!mIdleRequestExecutor) {
    mIdleRequestExecutor = new IdleRequestExecutor(this);
  }

  mIdleRequestExecutor->MaybeDispatch();
}

void nsGlobalWindowInner::SuspendIdleRequests() {
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->Cancel();
    mIdleRequestExecutor = nullptr;
  }
}

void nsGlobalWindowInner::ResumeIdleRequests() {
  MOZ_ASSERT(!mIdleRequestExecutor);

  ScheduleIdleRequestDispatch();
}

void nsGlobalWindowInner::RemoveIdleCallback(
    mozilla::dom::IdleRequest* aRequest) {
  AssertIsOnMainThread();

  if (aRequest->HasTimeout()) {
    mTimeoutManager->ClearTimeout(aRequest->GetTimeoutHandle(),
                                  Timeout::Reason::eIdleCallbackTimeout);
  }

  aRequest->removeFrom(mIdleRequestCallbacks);
}

void nsGlobalWindowInner::RunIdleRequest(IdleRequest* aRequest,
                                         DOMHighResTimeStamp aDeadline,
                                         bool aDidTimeout) {
  AssertIsOnMainThread();
  // XXXbz Do we still need this RefPtr?  MOZ_CAN_RUN_SCRIPT should
  // guarantee that caller is holding a strong ref on the stack.
  RefPtr<IdleRequest> request(aRequest);
  RemoveIdleCallback(request);
  request->IdleRun(this, aDeadline, aDidTimeout);
}

void nsGlobalWindowInner::ExecuteIdleRequest(TimeStamp aDeadline) {
  AssertIsOnMainThread();
  RefPtr<IdleRequest> request = mIdleRequestCallbacks.getFirst();

  if (!request) {
    // There are no more idle requests, so stop scheduling idle
    // request callbacks.
    return;
  }

  // If the request that we're trying to execute has been queued
  // during the current idle period, then dispatch it again at the end
  // of the idle period.
  if (mIdleRequestExecutor->IneligibleForCurrentIdlePeriod(request)) {
    mIdleRequestExecutor->MaybeDispatch(aDeadline);
    return;
  }

  DOMHighResTimeStamp deadline = 0.0;

  if (Performance* perf = GetPerformance()) {
    deadline = perf->GetDOMTiming()->TimeStampToDOMHighRes(aDeadline);
  }

  mIdleRequestExecutor->MaybeUpdateIdlePeriodLimit();
  RunIdleRequest(request, deadline, false);

  // Running the idle callback could've suspended the window, in which
  // case mIdleRequestExecutor will be null.
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->MaybeDispatch();
  }
}

class IdleRequestTimeoutHandler final : public TimeoutHandler {
 public:
  IdleRequestTimeoutHandler(JSContext* aCx, IdleRequest* aIdleRequest,
                            nsPIDOMWindowInner* aWindow)
      : TimeoutHandler(aCx), mIdleRequest(aIdleRequest), mWindow(aWindow) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(IdleRequestTimeoutHandler)

  MOZ_CAN_RUN_SCRIPT bool Call(const char* /* unused */) override {
    RefPtr<nsGlobalWindowInner> window(nsGlobalWindowInner::Cast(mWindow));
    RefPtr<IdleRequest> request(mIdleRequest);
    window->RunIdleRequest(request, 0.0, true);
    return true;
  }

 private:
  ~IdleRequestTimeoutHandler() override = default;

  RefPtr<IdleRequest> mIdleRequest;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

NS_IMPL_CYCLE_COLLECTION(IdleRequestTimeoutHandler, mIdleRequest, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IdleRequestTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IdleRequestTimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

uint32_t nsGlobalWindowInner::RequestIdleCallback(
    JSContext* aCx, IdleRequestCallback& aCallback,
    const IdleRequestOptions& aOptions, ErrorResult& aError) {
  AssertIsOnMainThread();

  if (IsDying()) {
    return 0;
  }

  uint32_t handle = mIdleRequestCallbackCounter++;

  RefPtr<IdleRequest> request = new IdleRequest(&aCallback, handle);

  if (aOptions.mTimeout.WasPassed()) {
    int32_t timeoutHandle;
    RefPtr<TimeoutHandler> handler(
        new IdleRequestTimeoutHandler(aCx, request, this));

    nsresult rv = mTimeoutManager->SetTimeout(
        handler, aOptions.mTimeout.Value(), false,
        Timeout::Reason::eIdleCallbackTimeout, &timeoutHandle);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return 0;
    }

    request->SetTimeoutHandle(timeoutHandle);
  }

  mIdleRequestCallbacks.insertBack(request);

  if (!IsSuspended()) {
    ScheduleIdleRequestDispatch();
  }

  return handle;
}

void nsGlobalWindowInner::CancelIdleCallback(uint32_t aHandle) {
  for (IdleRequest* r : mIdleRequestCallbacks) {
    if (r->Handle() == aHandle) {
      RemoveIdleCallback(r);
      break;
    }
  }
}

void nsGlobalWindowInner::DisableIdleCallbackRequests() {
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->Cancel();
    mIdleRequestExecutor = nullptr;
  }

  while (!mIdleRequestCallbacks.isEmpty()) {
    RefPtr<IdleRequest> request = mIdleRequestCallbacks.getFirst();
    RemoveIdleCallback(request);
  }
}

bool nsGlobalWindowInner::IsBackgroundInternal() const {
  return !mOuterWindow || mOuterWindow->IsBackground();
}

class PromiseDocumentFlushedResolver final {
 public:
  PromiseDocumentFlushedResolver(Promise* aPromise,
                                 PromiseDocumentFlushedCallback& aCallback)
      : mPromise(aPromise), mCallback(&aCallback) {}

  virtual ~PromiseDocumentFlushedResolver() = default;

  void Call() {
    nsMutationGuard guard;
    ErrorResult error;
    JS::Rooted<JS::Value> returnVal(RootingCx());
    mCallback->Call(&returnVal, error);

    if (error.Failed()) {
      mPromise->MaybeReject(std::move(error));
    } else if (guard.Mutated(0)) {
      // Something within the callback mutated the DOM.
      mPromise->MaybeReject(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    } else {
      mPromise->MaybeResolve(returnVal);
    }
  }

  void Cancel() { mPromise->MaybeReject(NS_ERROR_ABORT); }

  RefPtr<Promise> mPromise;
  RefPtr<PromiseDocumentFlushedCallback> mCallback;
};

//*****************************************************************************
//***    nsGlobalWindowInner: Object Management
//*****************************************************************************

nsGlobalWindowInner::nsGlobalWindowInner(nsGlobalWindowOuter* aOuterWindow,
                                         WindowGlobalChild* aActor)
    : nsPIDOMWindowInner(aOuterWindow, aActor),
      mWasOffline(false),
      mHasHadSlowScript(false),
      mIsChrome(false),
      mCleanMessageManager(false),
      mNeedsFocus(true),
      mHasFocus(false),
      mShowFocusRingForContent(false),
      mFocusByKeyOccurred(false),
      mDidFireDocElemInserted(false),
      mHasGamepad(false),
      mHasXRSession(false),
      mHasVRDisplayActivateEvents(false),
      mXRRuntimeDetectionInFlight(false),
      mXRPermissionRequestInFlight(false),
      mXRPermissionGranted(false),
      mWasCurrentInnerWindow(false),
      mHasSeenGamepadInput(false),
      mHintedWasLoading(false),
      mSuspendDepth(0),
      mFreezeDepth(0),
#ifdef DEBUG
      mSerial(0),
#endif
      mFocusMethod(0),
      mIdleRequestCallbackCounter(1),
      mIdleRequestExecutor(nullptr),
      mDialogAbuseCount(0),
      mAreDialogsEnabled(true),
      mObservingDidRefresh(false),
      mIteratingDocumentFlushedResolvers(false),
      mCanSkipCCGeneration(0),
      mBeforeUnloadListenerCount(0) {
  mIsInnerWindow = true;

  AssertIsOnMainThread();
  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  // add this inner window to the outer window list of inners.
  PR_INSERT_AFTER(this, aOuterWindow);

  mTimeoutManager = MakeUnique<dom::TimeoutManager>(
      *this, StaticPrefs::dom_timeout_max_idle_defer_ms());

  mObserver = new nsGlobalWindowObserver(this);
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    // Watch for online/offline status changes so we can fire events. Use
    // a strong reference.
    os->AddObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false);

    os->AddObserver(mObserver, MEMORY_PRESSURE_OBSERVER_TOPIC, false);
  }

  Preferences::AddStrongObserver(mObserver, "intl.accept_languages");

  // Watch for storage notifications so we can fire storage events.
  RefPtr<StorageNotifierService> sns = StorageNotifierService::GetOrCreate();
  if (sns) {
    sns->Register(mObserver);
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIDocShell> docShell = GetDocShell();
    if (docShell) {
      mBrowserChild = docShell->GetBrowserChild();
    }
  }

  if (gDumpFile == nullptr) {
    nsAutoCString fname;
    Preferences::GetCString("browser.dom.window.dump.file", fname);
    if (!fname.IsEmpty()) {
      // If this fails to open, Dump() knows to just go to stdout on null.
      gDumpFile = fopen(fname.get(), "wb+");
    } else {
      gDumpFile = stdout;
    }
  }

#ifdef DEBUG
  mSerial = nsContentUtils::InnerOrOuterWindowCreated();

  MOZ_LOG(gDocShellAndDOMWindowLeakLogging, LogLevel::Info,
          ("++DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p]\n",
           nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
           static_cast<void*>(ToCanonicalSupports(this)), getpid(), mSerial,
           static_cast<void*>(ToCanonicalSupports(aOuterWindow))));
#endif

  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
          ("DOMWINDOW %p created outer=%p", this, aOuterWindow));

  // Add ourselves to the inner windows list.
  MOZ_ASSERT(sInnerWindowsById, "Inner Windows hash table must be created!");
  MOZ_ASSERT(!sInnerWindowsById->Get(mWindowID),
             "This window shouldn't be in the hash table yet!");
  // We seem to see crashes in release builds because of null
  // |sInnerWindowsById|.
  if (sInnerWindowsById) {
    sInnerWindowsById->Put(mWindowID, this);
  }
}

#ifdef DEBUG

/* static */
void nsGlobalWindowInner::AssertIsOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());
}

#endif  // DEBUG

/* static */
void nsGlobalWindowInner::Init() {
  AssertIsOnMainThread();

  NS_ASSERTION(gDOMLeakPRLogInner,
               "gDOMLeakPRLogInner should have been initialized!");

  sInnerWindowsById = new InnerWindowByIdTable();
}

nsGlobalWindowInner::~nsGlobalWindowInner() {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mHintedWasLoading);

  if (IsChromeWindow()) {
    MOZ_ASSERT(mCleanMessageManager,
               "chrome windows may always disconnect the msg manager");

    DisconnectAndClearGroupMessageManagers();

    if (mChromeFields.mMessageManager) {
      static_cast<nsFrameMessageManager*>(mChromeFields.mMessageManager.get())
          ->Disconnect();
    }

    mCleanMessageManager = false;
  }

  // In most cases this should already have been called, but call it again
  // here to catch any corner cases.
  FreeInnerObjects();

  if (sInnerWindowsById) {
    sInnerWindowsById->Remove(mWindowID);
  }

  nsContentUtils::InnerOrOuterWindowDestroyed();

#ifdef DEBUG
  if (MOZ_LOG_TEST(gDocShellAndDOMWindowLeakLogging, LogLevel::Info)) {
    nsAutoCString url;
    if (mLastOpenedURI) {
      url = mLastOpenedURI->GetSpecOrDefault();

      // Data URLs can be very long, so truncate to avoid flooding the log.
      const uint32_t maxURLLength = 1000;
      if (url.Length() > maxURLLength) {
        url.Truncate(maxURLLength);
      }
    }

    nsGlobalWindowOuter* outer = nsGlobalWindowOuter::Cast(mOuterWindow);
    MOZ_LOG(
        gDocShellAndDOMWindowLeakLogging, LogLevel::Info,
        ("--DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p] [url = "
         "%s]\n",
         nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
         static_cast<void*>(ToCanonicalSupports(this)), getpid(), mSerial,
         static_cast<void*>(ToCanonicalSupports(outer)), url.get()));
  }
#endif
  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
          ("DOMWINDOW %p destroyed", this));

  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                        mMutationBits ? 1 : 0);
  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_TEXT_EVENT_LISTENERS,
                        mMayHaveTextEventListenerInDefaultGroup ? 1 : 0);

  // An inner window is destroyed, pull it out of the outer window's
  // list if inner windows.

  PR_REMOVE_LINK(this);

  // If our outer window's inner window is this window, null out the
  // outer window's reference to this window that's being deleted.
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (outer) {
    outer->MaybeClearInnerWindow(this);
  }

  // We don't have to leave the tab group if we are an inner window.

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac) ac->RemoveWindowAsListener(this);

  nsLayoutStatics::Release();
}

// static
void nsGlobalWindowInner::ShutDown() {
  AssertIsOnMainThread();

  if (gDumpFile && gDumpFile != stdout) {
    fclose(gDumpFile);
  }
  gDumpFile = nullptr;

  delete sInnerWindowsById;
  sInnerWindowsById = nullptr;
}

void nsGlobalWindowInner::FreeInnerObjects() {
  if (IsDying()) {
    return;
  }
  StartDying();

  // Make sure that this is called before we null out the document and
  // other members that the window destroyed observers could
  // re-create.
  NotifyDOMWindowDestroyed(this);
  if (auto* reporter = nsWindowMemoryReporter::Get()) {
    reporter->ObserveDOMWindowDetached(this);
  }

  // Kill all of the workers for this window.
  CancelWorkersForWindow(this);

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>>::ForwardIterator iter(
      mSharedWorkers);
  while (iter.HasMore()) {
    iter.GetNext()->Close();
  }

  if (mTimeoutManager) {
    mTimeoutManager->ClearAllTimeouts();
  }

  DisableIdleCallbackRequests();

  mChromeEventHandler = nullptr;

  if (mListenerManager) {
    mListenerManager->RemoveAllListeners();
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }

  mHistory = nullptr;

  if (mNavigator) {
    mNavigator->OnNavigation();
    mNavigator->Invalidate();
    mNavigator = nullptr;
  }

  mScreen = nullptr;

#if defined(MOZ_WIDGET_ANDROID)
  mOrientationChangeObserver = nullptr;
#endif

  if (mDoc) {
    // Remember the document's principal, URI, and CSP.
    mDocumentPrincipal = mDoc->NodePrincipal();
    mDocumentStoragePrincipal = mDoc->EffectiveStoragePrincipal();
    mDocumentURI = mDoc->GetDocumentURI();
    mDocBaseURI = mDoc->GetDocBaseURI();
    mDocContentBlockingAllowListPrincipal =
        mDoc->GetContentBlockingAllowListPrincipal();
    mDocumentCsp = mDoc->GetCsp();

    while (mDoc->EventHandlingSuppressed()) {
      mDoc->UnsuppressEventHandlingAndFireEvents(false);
    }

    if (mObservingDidRefresh) {
      PresShell* presShell = mDoc->GetPresShell();
      if (presShell) {
        Unused << presShell->RemovePostRefreshObserver(this);
      }
    }
  }

  // Remove our reference to the document and the document principal.
  mFocusedElement = nullptr;

  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())
        ->Disconnect();
    mApplicationCache = nullptr;
  }

  if (mIndexedDB) {
    mIndexedDB->DisconnectFromGlobal(this);
    mIndexedDB = nullptr;
  }

  UnlinkHostObjectURIs();

  NotifyWindowIDDestroyed("inner-window-destroyed");

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

  DisableGamepadUpdates();
  mHasGamepad = false;
  mGamepads.Clear();
  DisableVRUpdates();
  mHasXRSession = false;
  mHasVRDisplayActivateEvents = false;
  mXRRuntimeDetectionInFlight = false;
  mXRPermissionRequestInFlight = false;
  mXRPermissionGranted = false;
  mVRDisplays.Clear();

  // This breaks a cycle between the window and the ClientSource object.
  mClientSource.reset();

  if (mWindowGlobalChild) {
    // Remove any remaining listeners.
    int64_t nListeners = mWindowGlobalChild->BeforeUnloadListeners();
    for (int64_t i = 0; i < nListeners; ++i) {
      mWindowGlobalChild->BeforeUnloadRemoved();
    }
    MOZ_ASSERT(mWindowGlobalChild->BeforeUnloadListeners() == 0);
  }

  // If we have any promiseDocumentFlushed callbacks, fire them now so
  // that the Promises can resolve.
  CallDocumentFlushedResolvers();
  mObservingDidRefresh = false;

  DisconnectEventTargetObjects();

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, MEMORY_PRESSURE_OBSERVER_TOPIC);
    }

    RefPtr<StorageNotifierService> sns = StorageNotifierService::GetOrCreate();
    if (sns) {
      sns->Unregister(mObserver);
    }

    Preferences::RemoveObserver(mObserver, "intl.accept_languages");

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
  }

  mMenubar = nullptr;
  mToolbar = nullptr;
  mLocationbar = nullptr;
  mPersonalbar = nullptr;
  mStatusbar = nullptr;
  mScrollbars = nullptr;

  mConsole = nullptr;

  mPaintWorklet = nullptr;

  mExternal = nullptr;
  mInstallTrigger = nullptr;

  mLocalStorage = nullptr;
  mSessionStorage = nullptr;
  mPerformance = nullptr;

  mSharedWorkers.Clear();

#ifdef MOZ_WEBSPEECH
  mSpeechSynthesis = nullptr;
#endif

  mParentTarget = nullptr;

  if (mCleanMessageManager) {
    MOZ_ASSERT(mIsChrome, "only chrome should have msg manager cleaned");
    if (mChromeFields.mMessageManager) {
      mChromeFields.mMessageManager->Disconnect();
    }
  }

  if (mWindowGlobalChild && !mWindowGlobalChild->IsClosed()) {
    mWindowGlobalChild->Destroy();
  }
  mWindowGlobalChild = nullptr;

  mIntlUtils = nullptr;

  HintIsLoading(false);
}

//*****************************************************************************
// nsGlobalWindowInner::nsISupports
//*****************************************************************************

// QueryInterface implementation for nsGlobalWindowInner
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindowInner)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindowInner)
  NS_INTERFACE_MAP_ENTRY(mozIDOMWindow)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMChromeWindow, IsChromeWindow())
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGlobalWindowInner)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGlobalWindowInner)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGlobalWindowInner)
  if (tmp->IsBlackForCC(false)) {
    if (nsCCUncollectableMarker::InGeneration(tmp->mCanSkipCCGeneration)) {
      return true;
    }
    tmp->mCanSkipCCGeneration = nsCCUncollectableMarker::sGeneration;
    if (EventListenerManager* elm = tmp->GetExistingListenerManager()) {
      elm->MarkForCC();
    }
    if (tmp->mTimeoutManager) {
      tmp->mTimeoutManager->UnmarkGrayTimers();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGlobalWindowInner)
  return tmp->IsBlackForCC(true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGlobalWindowInner)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindowInner)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsGlobalWindowInner)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoCString uri;
    if (tmp->mDoc && tmp->mDoc->GetDocumentURI()) {
      uri = tmp->mDoc->GetDocumentURI()->GetSpecOrDefault();
    }
    SprintfLiteral(name, "nsGlobalWindowInner # %" PRIu64 " inner %s",
                   tmp->mWindowID, uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGlobalWindowInner, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSpeechSynthesis)
#endif

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOuterWindow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopInnerWindow)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)

  if (tmp->mTimeoutManager) {
    tmp->mTimeoutManager->ForEachUnorderedTimeout([&cb](Timeout* timeout) {
      cb.NoteNativeChild(timeout, NS_CYCLE_COLLECTION_PARTICIPANT(Timeout));
    });
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHistory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCustomElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSharedWorkers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentStoragePrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentCsp)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleRequestExecutor)
  for (IdleRequest* request : tmp->mIdleRequestCallbacks) {
    cb.NoteNativeChild(request, NS_CYCLE_COLLECTION_PARTICIPANT(IdleRequest));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClientSource)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepads)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRDisplays)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDebuggerNotificationManager)

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusedElement)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMenubar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mToolbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocationbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPersonalbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStatusbar)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScrollbars)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mU2F)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPaintWorklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExternal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInstallTrigger)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIntlUtils)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReportRecords)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReportingObservers)

  tmp->TraverseHostObjectURIs(cb);

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeFields.mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeFields.mGroupMessageManagers)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingPromises)

  for (size_t i = 0; i < tmp->mDocumentFlushedResolvers.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentFlushedResolvers[i]->mPromise);
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentFlushedResolvers[i]->mCallback);
  }

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindowInner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
  if (sInnerWindowsById) {
    sInnerWindowsById->Remove(tmp->mWindowID);
  }

  JSObject* wrapper = tmp->GetWrapperPreserveColor();
  if (wrapper) {
    // Mark our realm as dead, so the JS engine won't hand out our
    // global after this point.
    JS::RealmBehaviorsRef(js::GetNonCCWObjectRealm(wrapper)).setNonLive();
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechSynthesis)
#endif

  if (tmp->mOuterWindow) {
    nsGlobalWindowOuter::Cast(tmp->mOuterWindow)->MaybeClearInnerWindow(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOuterWindow)
  }

  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
  }

  // Here the Timeouts list would've been unlinked, but we rely on
  // that Timeout objects have been traced and will remove themselves
  // while unlinking.

  tmp->UpdateTopInnerWindow();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTopInnerWindow)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHistory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCustomElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSharedWorkers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessionStorage)
  if (tmp->mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(tmp->mApplicationCache.get())
        ->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplicationCache)
  }
  if (tmp->mIndexedDB) {
    tmp->mIndexedDB->DisconnectFromGlobal(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentStoragePrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentCsp)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGamepads)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVRDisplays)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDebuggerNotificationManager)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFocusedElement)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMenubar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mToolbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocationbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPersonalbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStatusbar)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScrollbars)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCrypto)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mU2F)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPaintWorklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExternal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInstallTrigger)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntlUtils)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReportRecords)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReportingObservers)

  tmp->UnlinkHostObjectURIs();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleRequestExecutor)

  // Here the IdleRequest list would've been unlinked, but we rely on
  // that IdleRequest objects have been traced and will remove
  // themselves while unlinking.

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClientSource)

  if (tmp->IsChromeWindow()) {
    if (tmp->mChromeFields.mMessageManager) {
      static_cast<nsFrameMessageManager*>(
          tmp->mChromeFields.mMessageManager.get())
          ->Disconnect();
      NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeFields.mMessageManager)
    }
    tmp->DisconnectAndClearGroupMessageManagers();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeFields.mGroupMessageManagers)
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingPromises)
  for (size_t i = 0; i < tmp->mDocumentFlushedResolvers.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentFlushedResolvers[i]->mPromise);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentFlushedResolvers[i]->mCallback);
  }
  tmp->mDocumentFlushedResolvers.Clear();

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

#ifdef DEBUG
void nsGlobalWindowInner::RiskyUnlink() {
  NS_CYCLE_COLLECTION_INNERNAME.Unlink(this);
}
#endif

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindowInner)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool nsGlobalWindowInner::IsBlackForCC(bool aTracingNeeded) {
  if (!nsCCUncollectableMarker::sGeneration) {
    return false;
  }

  return (nsCCUncollectableMarker::InGeneration(GetMarkedCCGeneration()) ||
          HasKnownLiveWrapper()) &&
         (!aTracingNeeded || HasNothingToTrace(ToSupports(this)));
}

//*****************************************************************************
// nsGlobalWindowInner::nsIScriptGlobalObject
//*****************************************************************************

nsresult nsGlobalWindowInner::EnsureScriptEnvironment() {
  // NOTE: We can't use FORWARD_TO_OUTER here because we don't want to fail if
  // we're called on an inactive inner window.
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    NS_WARNING("No outer window available!");
    return NS_ERROR_FAILURE;
  }
  return outer->EnsureScriptEnvironment();
}

nsIScriptContext* nsGlobalWindowInner::GetScriptContext() {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    return nullptr;
  }
  return outer->GetScriptContext();
}

void nsGlobalWindowInner::TraceGlobalJSObject(JSTracer* aTrc) {
  TraceWrapper(aTrc, "active window global");
}

void nsGlobalWindowInner::InitDocumentDependentState(JSContext* aCx) {
  MOZ_ASSERT(mDoc);

  if (MOZ_LOG_TEST(gDOMLeakPRLogInner, LogLevel::Debug)) {
    nsIURI* uri = mDoc->GetDocumentURI();
    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
            ("DOMWINDOW %p SetNewDocument %s", this,
             uri ? uri->GetSpecOrDefault().get() : ""));
  }

  mFocusedElement = nullptr;
  mLocalStorage = nullptr;
  mSessionStorage = nullptr;
  mPerformance = nullptr;

  // This must be called after nullifying the internal objects because here we
  // could recreate them, calling the getter methods, and store them into the JS
  // slots. If we nullify them after, the slot values and the objects will be
  // out of sync.
  ClearDocumentDependentSlots(aCx);

  // FIXME: Currently, devtools can crete a fallback webextension window global
  // in the content process which does not have a corresponding BrowserChild
  // actor. This means we have no actor to be our parent. (Bug 1498293)
  if (!mWindowGlobalChild && (XRE_IsParentProcess() || mBrowserChild)) {
    mWindowGlobalChild = WindowGlobalChild::Create(this);
  }

  if (mWindowGlobalChild && GetBrowsingContext()) {
    GetBrowsingContext()->NotifyResetUserGestureActivation();
  }

#if defined(MOZ_WIDGET_ANDROID)
  // When we insert the new document to the window in the top-level browsing
  // context, we should reset the status of the request which is used for the
  // previous document.
  if (mWindowGlobalChild && GetBrowsingContext() &&
      !GetBrowsingContext()->GetParent()) {
    GetBrowsingContext()->ResetGVAutoplayRequestStatus();
  }
#endif

#ifdef DEBUG
  mLastOpenedURI = mDoc->GetDocumentURI();
#endif

  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_MUTATION_LISTENERS,
                        mMutationBits ? 1 : 0);
  Telemetry::Accumulate(Telemetry::INNERWINDOWS_WITH_TEXT_EVENT_LISTENERS,
                        mMayHaveTextEventListenerInDefaultGroup ? 1 : 0);

  // Clear our mutation bitfield.
  mMutationBits = 0;

  mMayHaveTextEventListenerInDefaultGroup = false;
}

nsresult nsGlobalWindowInner::EnsureClientSource() {
  MOZ_DIAGNOSTIC_ASSERT(mDoc);

  bool newClientSource = false;

  // Get the load info for the document if we performed a load.  Be careful not
  // to look at local URLs, though. Local URLs are those that have a scheme of:
  //  * about:
  //  * data:
  //  * blob:
  // We also do an additional check here so that we only treat about:blank
  // and about:srcdoc as local URLs.  Other internal firefox about: URLs should
  // not be treated this way.
  nsCOMPtr<nsILoadInfo> loadInfo;
  nsCOMPtr<nsIChannel> channel = mDoc->GetChannel();
  if (channel) {
    nsCOMPtr<nsIURI> uri;
    Unused << channel->GetURI(getter_AddRefs(uri));

    bool ignoreLoadInfo = false;

    // Note, this is mostly copied from NS_IsAboutBlank().  Its duplicated
    // here so we can efficiently check about:srcdoc as well.
    if (uri->SchemeIs("about")) {
      nsCString spec = uri->GetSpecOrDefault();
      ignoreLoadInfo = spec.EqualsLiteral("about:blank") ||
                       spec.EqualsLiteral("about:srcdoc");
    } else {
      // Its not an about: URL, so now check for our other URL types.
      ignoreLoadInfo = uri->SchemeIs("data") || uri->SchemeIs("blob");
    }

    if (!ignoreLoadInfo) {
      loadInfo = channel->LoadInfo();
    }
  }

  // Take the initial client source from the docshell immediately.  Even if we
  // don't end up using it here we should consume it.
  UniquePtr<ClientSource> initialClientSource;
  nsIDocShell* docshell = GetDocShell();
  if (docshell) {
    initialClientSource = docshell->TakeInitialClientSource();
  }

  // Try to get the reserved client from the LoadInfo.  A Client is
  // reserved at the start of the channel load if there is not an
  // initial about:blank document that will be reused.  It is also
  // created if the channel load encounters a cross-origin redirect.
  if (loadInfo) {
    UniquePtr<ClientSource> reservedClient =
        loadInfo->TakeReservedClientSource();
    if (reservedClient) {
      mClientSource.reset();
      mClientSource = std::move(reservedClient);
      newClientSource = true;
    }
  }

  // We don't have a LoadInfo reserved client, but maybe we should
  // be inheriting an initial one from the docshell.  This means
  // that the docshell started the channel load before creating the
  // initial about:blank document.  This is an optimization, though,
  // and it created an initial Client as a placeholder for the document.
  // In this case we want to inherit this placeholder Client here.
  if (!mClientSource) {
    mClientSource = std::move(initialClientSource);
    if (mClientSource) {
      newClientSource = true;
    }
  }

  // Verify the final ClientSource principal matches the final document
  // principal.  The ClientChannelHelper handles things like network
  // redirects, but there are other ways the document principal can change.
  // For example, if something sets the nsIChannel.owner property, then
  // the final channel principal can be anything.  Unfortunately there is
  // no good way to detect this until after the channel completes loading.
  //
  // For now we handle this just by reseting the ClientSource.  This will
  // result in a new ClientSource with the correct principal being created.
  // To APIs like ServiceWorker and Clients API it will look like there was
  // an initial content page created that was then immediately replaced.
  // This is pretty close to what we are actually doing.
  if (mClientSource) {
    nsCOMPtr<nsIPrincipal> clientPrincipal(
        mClientSource->Info().GetPrincipal());
    if (!clientPrincipal || !clientPrincipal->Equals(mDoc->NodePrincipal())) {
      mClientSource.reset();
    }
  }

  // If we don't have a reserved client or an initial client, then create
  // one now.  This can happen in certain cases where we avoid preallocating
  // the client in the docshell.  This mainly occurs in situations where
  // the principal is not clearly inherited from the parent; e.g. sandboxed
  // iframes, window.open(), etc.
  //
  // We also do this late ClientSource creation if the final document ended
  // up with a different principal.
  //
  // TODO: We may not be marking initial about:blank documents created
  //       this way as controlled by a service worker properly.  The
  //       controller should be coming from the same place as the inheritted
  //       principal.  We do this in docshell, but as mentioned we aren't
  //       smart enough to handle all cases yet.  For example, a
  //       window.open() with new URL should inherit the controller from
  //       the opener, but we probably don't handle that yet.
  if (!mClientSource) {
    mClientSource = ClientManager::CreateSource(
        ClientType::Window, EventTargetFor(TaskCategory::Other),
        mDoc->NodePrincipal());
    MOZ_DIAGNOSTIC_ASSERT(mClientSource);
    newClientSource = true;

    // Note, we don't apply the loadinfo controller below if we create
    // the ClientSource here.
  }

  // The load may have started controlling the Client as well.  If
  // so, mark it as controlled immediately here.  The actor may
  // or may not have been notified by the parent side about being
  // controlled yet.
  //
  // Note: We should be careful not to control a client that was created late.
  //       These clients were not seen by the ServiceWorkerManager when it
  //       marked the LoadInfo controlled and it won't know about them.  Its
  //       also possible we are creating the client late due to the final
  //       principal changing and these clients should definitely not be
  //       controlled by a service worker with a different principal.
  else if (loadInfo) {
    const Maybe<ServiceWorkerDescriptor> controller = loadInfo->GetController();
    if (controller.isSome()) {
      mClientSource->SetController(controller.ref());
    }

    // We also have to handle the case where te initial about:blank is
    // controlled due to inheritting the service worker from its parent,
    // but the actual nsIChannel load is not covered by any service worker.
    // In this case we want the final page to be uncontrolled.  There is
    // an open spec issue about how exactly this should be handled, but for
    // now we just force creation of a new ClientSource to clear the
    // controller.
    //
    //  https://github.com/w3c/ServiceWorker/issues/1232
    //
    else if (mClientSource->GetController().isSome()) {
      mClientSource.reset();
      mClientSource = ClientManager::CreateSource(
          ClientType::Window, EventTargetFor(TaskCategory::Other),
          mDoc->NodePrincipal());
      MOZ_DIAGNOSTIC_ASSERT(mClientSource);
      newClientSource = true;
    }
  }

  if (mClientSource) {
    // Generally the CSP is stored within the Client and cached on the document.
    // At the time of CSP parsing however, the Client has not been created yet,
    // hence we store the CSP on the document and propagate/sync the CSP with
    // Client here when we create the Client.
    mClientSource->SetCsp(mDoc->GetCsp());

    DocGroup* docGroup = GetDocGroup();
    MOZ_DIAGNOSTIC_ASSERT(docGroup);
    mClientSource->SetAgentClusterId(docGroup->AgentClusterId());

    if (mWindowGlobalChild) {
      mWindowGlobalChild->SendSetClientInfo(mClientSource->Info().ToIPC());
    }
  }

  // Its possible that we got a client just after being frozen in
  // the bfcache.  In that case freeze the client immediately.
  if (newClientSource && IsFrozen()) {
    mClientSource->Freeze();
  }

  return NS_OK;
}

nsresult nsGlobalWindowInner::ExecutionReady() {
  nsresult rv = EnsureClientSource();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mClientSource->WindowExecutionReady(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void nsGlobalWindowInner::UpdateParentTarget() {
  // NOTE: This method is identical to
  // nsGlobalWindowOuter::UpdateParentTarget(). IF YOU UPDATE THIS METHOD,
  // UPDATE THE OTHER ONE TOO!

  // Try to get our frame element's tab child global (its in-process message
  // manager).  If that fails, fall back to the chrome event handler's tab
  // child global, and if it doesn't have one, just use the chrome event
  // handler itself.

  nsPIDOMWindowOuter* outer = GetOuterWindow();
  if (!outer) {
    return;
  }
  nsCOMPtr<Element> frameElement = outer->GetFrameElementInternal();
  nsCOMPtr<EventTarget> eventTarget =
      nsContentUtils::TryGetBrowserChildGlobal(frameElement);

  if (!eventTarget) {
    nsGlobalWindowOuter* topWin = GetInProcessScriptableTopInternal();
    if (topWin) {
      frameElement = topWin->GetFrameElementInternal();
      eventTarget = nsContentUtils::TryGetBrowserChildGlobal(frameElement);
    }
  }

  if (!eventTarget) {
    eventTarget = nsContentUtils::TryGetBrowserChildGlobal(mChromeEventHandler);
  }

  if (!eventTarget) {
    eventTarget = mChromeEventHandler;
  }

  mParentTarget = eventTarget;
}

EventTarget* nsGlobalWindowInner::GetTargetForDOMEvent() {
  return GetOuterWindowInternal();
}

void nsGlobalWindowInner::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  EventMessage msg = aVisitor.mEvent->mMessage;

  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true;  // FIXME! Bug 329119
  if (msg == eResize && aVisitor.mEvent->IsTrusted()) {
    // QIing to window so that we can keep the old behavior also in case
    // a child window is handling resize.
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
    if (window) {
      mIsHandlingResizeEvent = true;
    }
  } else if (msg == eMouseDown && aVisitor.mEvent->IsTrusted()) {
    sMouseDown = true;
  } else if ((msg == eMouseUp || msg == eDragEnd) &&
             aVisitor.mEvent->IsTrusted()) {
    sMouseDown = false;
    if (sDragServiceDisabled) {
      nsCOMPtr<nsIDragService> ds =
          do_GetService("@mozilla.org/widget/dragservice;1");
      if (ds) {
        sDragServiceDisabled = false;
        ds->Unsuppress();
      }
    }
  }

  aVisitor.SetParentTarget(GetParentTarget(), true);
}

bool nsGlobalWindowInner::DialogsAreBeingAbused() {
  NS_ASSERTION(GetInProcessScriptableTopInternal() &&
                   GetInProcessScriptableTopInternal()
                           ->GetCurrentInnerWindowInternal() == this,
               "DialogsAreBeingAbused called with invalid window");

  if (mLastDialogQuitTime.IsNull() || nsContentUtils::IsCallerChrome()) {
    return false;
  }

  TimeDuration dialogInterval(TimeStamp::Now() - mLastDialogQuitTime);
  if (dialogInterval.ToSeconds() <
      Preferences::GetInt("dom.successive_dialog_time_limit",
                          DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT)) {
    mDialogAbuseCount++;

    return PopupBlocker::GetPopupControlState() > PopupBlocker::openAllowed ||
           mDialogAbuseCount > MAX_SUCCESSIVE_DIALOG_COUNT;
  }

  // Reset the abuse counter
  mDialogAbuseCount = 0;

  return false;
}

void nsGlobalWindowInner::FireFrameLoadEvent() {
  // If we're not in a content frame, or are at a BrowsingContext tree boundary,
  // such as the content-chrome boundary, don't fire the "load" event.
  if (GetBrowsingContext()->IsTopContent() ||
      GetBrowsingContext()->IsChrome()) {
    return;
  }

  // If embedder is same-process, fire the event on our embedder element.
  //
  // XXX: Bug 1440212 is looking into potentially changing this behaviour to act
  // more like the remote case when in-process.
  RefPtr<Element> element = GetBrowsingContext()->GetEmbedderElement();
  if (element) {
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetEvent event(/* aIsTrusted = */ true, eLoad);
    event.mFlags.mBubbles = false;
    event.mFlags.mCancelable = false;

    if (mozilla::dom::DocGroup::TryToLoadIframesInBackground()) {
      nsDocShell* ds = nsDocShell::Cast(GetDocShell());

      if (ds && !ds->HasFakeOnLoadDispatched()) {
        EventDispatcher::Dispatch(element, nullptr, &event, nullptr, &status);
      }
    } else {
      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.
      EventDispatcher::Dispatch(element, nullptr, &event, nullptr, &status);
    }
    return;
  }

  // We don't have an in-process embedder. Try to get our `BrowserChild` actor
  // to send a message to that embedder. We want to double-check that our outer
  // window is actually the one at the root of this browserChild though, just in
  // case.
  RefPtr<BrowserChild> browserChild =
      BrowserChild::GetFrom(static_cast<nsPIDOMWindowInner*>(this));
  if (browserChild) {
    // Double-check that our outer window is actually at the root of this
    // `BrowserChild`, in case we're in an odd maybe-unhosted situation like a
    // print preview dialog.
    nsCOMPtr<nsPIDOMWindowOuter> rootOuter =
        do_GetInterface(browserChild->WebNavigation());
    if (!rootOuter || rootOuter != GetOuterWindow()) {
      return;
    }

    mozilla::Unused << browserChild->SendMaybeFireEmbedderLoadEvents(
        /*aFireLoadAtEmbeddingElement*/ true);
  }
}

nsresult nsGlobalWindowInner::PostHandleEvent(EventChainPostVisitor& aVisitor) {
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
  RefPtr<EventTarget> kungFuDeathGrip1(mChromeEventHandler);
  mozilla::Unused
      << kungFuDeathGrip1;  // These aren't referred to through the function
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());
  mozilla::Unused
      << kungFuDeathGrip2;  // These aren't referred to through the function

  if (aVisitor.mEvent->mMessage == eResize) {
    mIsHandlingResizeEvent = false;
  } else if (aVisitor.mEvent->mMessage == eUnload &&
             aVisitor.mEvent->IsTrusted()) {
    // If any VR display presentation is active at unload, the next page
    // will receive a vrdisplayactive event to indicate that it should
    // immediately begin vr presentation. This should occur when navigating
    // forwards, navigating backwards, and on page reload.
    for (const auto& display : mVRDisplays) {
      if (display->IsPresenting()) {
        display->StartVRNavigation();
        // Save this VR display ID to trigger vrdisplayactivate event
        // after the next load event.
        nsGlobalWindowOuter* outer = GetOuterWindowInternal();
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
    mIsDocumentLoaded = false;
  } else if (aVisitor.mEvent->mMessage == eLoad &&
             aVisitor.mEvent->IsTrusted()) {
    // This is page load event since load events don't propagate to |window|.
    // @see Document::GetEventTargetParent.
    mIsDocumentLoaded = true;

    mTimeoutManager->OnDocumentLoaded();

    MOZ_ASSERT(aVisitor.mEvent->IsTrusted());
    FireFrameLoadEvent();

    if (mVREventObserver) {
      mVREventObserver->NotifyAfterLoad();
    }

    uint32_t autoActivateVRDisplayID = 0;
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
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

nsresult nsGlobalWindowInner::DefineArgumentsProperty(nsIArray* aArguments) {
  nsIScriptContext* ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx, NS_ERROR_NOT_INITIALIZED);

  JS::Rooted<JSObject*> obj(RootingCx(), GetWrapperPreserveColor());
  return ctx->SetProperty(obj, "arguments", aArguments);
}

//*****************************************************************************
// nsGlobalWindowInner::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal* nsGlobalWindowInner::GetPrincipal() {
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
      do_QueryInterface(GetInProcessParentInternal());

  if (objPrincipal) {
    return objPrincipal->GetPrincipal();
  }

  return nullptr;
}

nsIPrincipal* nsGlobalWindowInner::GetEffectiveStoragePrincipal() {
  if (mDoc) {
    // If we have a document, get the principal from the document
    return mDoc->EffectiveStoragePrincipal();
  }

  if (mDocumentStoragePrincipal) {
    return mDocumentStoragePrincipal;
  }

  // If we don't have a storage principal and we don't have a document we ask
  // the parent window for the storage principal.

  nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(GetInProcessParentInternal());

  if (objPrincipal) {
    return objPrincipal->GetEffectiveStoragePrincipal();
  }

  return nullptr;
}

//*****************************************************************************
// nsGlobalWindowInner::nsIDOMWindow
//*****************************************************************************

bool nsPIDOMWindowInner::AddAudioContext(AudioContext* aAudioContext) {
  mAudioContexts.AppendElement(aAudioContext);

  // Return true if the context should be muted and false if not.
  nsIDocShell* docShell = GetDocShell();
  return docShell && !docShell->GetAllowMedia() && !aAudioContext->IsOffline();
}

void nsPIDOMWindowInner::RemoveAudioContext(AudioContext* aAudioContext) {
  mAudioContexts.RemoveElement(aAudioContext);
}

void nsPIDOMWindowInner::MuteAudioContexts() {
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Mute();
    }
  }
}

void nsPIDOMWindowInner::UnmuteAudioContexts() {
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Unmute();
    }
  }
}

WindowProxyHolder nsGlobalWindowInner::Window() {
  return WindowProxyHolder(GetBrowsingContext());
}

Navigator* nsPIDOMWindowInner::Navigator() {
  if (!mNavigator) {
    mNavigator = new mozilla::dom::Navigator(this);
  }

  return mNavigator;
}

VisualViewport* nsGlobalWindowInner::VisualViewport() {
  if (!mVisualViewport) {
    mVisualViewport = new mozilla::dom::VisualViewport(this);
  }

  return mVisualViewport;
}

nsScreen* nsGlobalWindowInner::GetScreen(ErrorResult& aError) {
  if (!mScreen) {
    mScreen = nsScreen::Create(this);
    if (!mScreen) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
  }

  return mScreen;
}

nsHistory* nsGlobalWindowInner::GetHistory(ErrorResult& aError) {
  if (!mHistory) {
    mHistory = new nsHistory(this);
  }

  return mHistory;
}

CustomElementRegistry* nsGlobalWindowInner::CustomElements() {
  if (!mCustomElements) {
    mCustomElements = new CustomElementRegistry(this);
  }

  return mCustomElements;
}

CustomElementRegistry* nsGlobalWindowInner::GetExistingCustomElements() {
  return mCustomElements;
}

Performance* nsPIDOMWindowInner::GetPerformance() {
  CreatePerformanceObjectIfNeeded();
  return mPerformance;
}

void nsPIDOMWindowInner::QueuePerformanceNavigationTiming() {
  CreatePerformanceObjectIfNeeded();
  if (mPerformance) {
    mPerformance->QueueNavigationTimingEntry();
  }
}

void nsPIDOMWindowInner::CreatePerformanceObjectIfNeeded() {
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
    mPerformance = Performance::CreateForMainThread(this, mDoc->NodePrincipal(),
                                                    timing, timedChannel);
  }
}

bool nsPIDOMWindowInner::IsSecureContext() const {
  return nsGlobalWindowInner::Cast(this)->IsSecureContext();
}

void nsPIDOMWindowInner::Suspend() {
  nsGlobalWindowInner::Cast(this)->Suspend();
}

void nsPIDOMWindowInner::Resume() { nsGlobalWindowInner::Cast(this)->Resume(); }

void nsPIDOMWindowInner::SyncStateFromParentWindow() {
  nsGlobalWindowInner::Cast(this)->SyncStateFromParentWindow();
}

Maybe<ClientInfo> nsPIDOMWindowInner::GetClientInfo() const {
  return nsGlobalWindowInner::Cast(this)->GetClientInfo();
}

Maybe<ClientState> nsPIDOMWindowInner::GetClientState() const {
  return nsGlobalWindowInner::Cast(this)->GetClientState();
}

Maybe<ServiceWorkerDescriptor> nsPIDOMWindowInner::GetController() const {
  return nsGlobalWindowInner::Cast(this)->GetController();
}

void nsPIDOMWindowInner::SetCsp(nsIContentSecurityPolicy* aCsp) {
  return nsGlobalWindowInner::Cast(this)->SetCsp(aCsp);
}

void nsPIDOMWindowInner::SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCsp) {
  return nsGlobalWindowInner::Cast(this)->SetPreloadCsp(aPreloadCsp);
}

nsIContentSecurityPolicy* nsPIDOMWindowInner::GetCsp() {
  return nsGlobalWindowInner::Cast(this)->GetCsp();
}

void nsPIDOMWindowInner::NoteCalledRegisterForServiceWorkerScope(
    const nsACString& aScope) {
  nsGlobalWindowInner::Cast(this)->NoteCalledRegisterForServiceWorkerScope(
      aScope);
}

void nsPIDOMWindowInner::NoteDOMContentLoaded() {
  nsGlobalWindowInner::Cast(this)->NoteDOMContentLoaded();
}

bool nsGlobalWindowInner::ShouldReportForServiceWorkerScope(
    const nsAString& aScope) {
  bool result = false;

  nsPIDOMWindowOuter* topOuter = GetInProcessScriptableTop();
  NS_ENSURE_TRUE(topOuter, false);

  nsGlobalWindowInner* topInner =
      nsGlobalWindowInner::Cast(topOuter->GetCurrentInnerWindow());
  NS_ENSURE_TRUE(topInner, false);

  topInner->ShouldReportForServiceWorkerScopeInternal(
      NS_ConvertUTF16toUTF8(aScope), &result);
  return result;
}

InstallTriggerImpl* nsGlobalWindowInner::GetInstallTrigger() {
  if (!mInstallTrigger) {
    ErrorResult rv;
    mInstallTrigger = ConstructJSImplementation<InstallTriggerImpl>(
        "@mozilla.org/addons/installtrigger;1", this, rv);
    if (rv.Failed()) {
      rv.SuppressException();
      return nullptr;
    }
  }

  return mInstallTrigger;
}

nsIDOMWindowUtils* nsGlobalWindowInner::GetWindowUtils(ErrorResult& aRv) {
  FORWARD_TO_OUTER_OR_THROW(WindowUtils, (), aRv, nullptr);
}

CallState nsGlobalWindowInner::ShouldReportForServiceWorkerScopeInternal(
    const nsACString& aScope, bool* aResultOut) {
  MOZ_DIAGNOSTIC_ASSERT(aResultOut);

  // First check to see if this window is controlled.  If so, then we have
  // found a match and are done.
  const Maybe<ServiceWorkerDescriptor> swd = GetController();
  if (swd.isSome() && swd.ref().Scope() == aScope) {
    *aResultOut = true;
    return CallState::Stop;
  }

  // Next, check to see if this window has called
  // navigator.serviceWorker.register() for this scope.  If so, then treat this
  // as a match so console reports appear in the devtools console.
  if (mClientSource &&
      mClientSource->CalledRegisterForServiceWorkerScope(aScope)) {
    *aResultOut = true;
    return CallState::Stop;
  }

  // Finally check the current docshell nsILoadGroup to see if there are any
  // outstanding navigation requests.  If so, match the scope against the
  // channel's URL.  We want to show console reports during the FetchEvent
  // intercepting the navigation itself.
  nsCOMPtr<nsIDocumentLoader> loader(do_QueryInterface(GetDocShell()));
  if (loader) {
    nsCOMPtr<nsILoadGroup> loadgroup;
    Unused << loader->GetLoadGroup(getter_AddRefs(loadgroup));
    if (loadgroup) {
      nsCOMPtr<nsISimpleEnumerator> iter;
      Unused << loadgroup->GetRequests(getter_AddRefs(iter));
      if (iter) {
        nsCOMPtr<nsISupports> tmp;
        bool hasMore = true;
        // Check each network request in the load group.
        while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
          iter->GetNext(getter_AddRefs(tmp));
          nsCOMPtr<nsIChannel> loadingChannel(do_QueryInterface(tmp));
          // Ignore subresource requests.  Logging for a subresource
          // FetchEvent should be handled above since the client is
          // already controlled.
          if (!loadingChannel ||
              !nsContentUtils::IsNonSubresourceRequest(loadingChannel)) {
            continue;
          }
          nsCOMPtr<nsIURI> loadingURL;
          Unused << loadingChannel->GetURI(getter_AddRefs(loadingURL));
          if (!loadingURL) {
            continue;
          }
          nsAutoCString loadingSpec;
          Unused << loadingURL->GetSpec(loadingSpec);
          // Perform a simple substring comparison to match the scope
          // against the channel URL.
          if (StringBeginsWith(loadingSpec, aScope)) {
            *aResultOut = true;
            return CallState::Stop;
          }
        }
      }
    }
  }

  // The current window doesn't care about this service worker, but maybe
  // one of our child frames does.
  return CallOnInProcessChildren(
      &nsGlobalWindowInner::ShouldReportForServiceWorkerScopeInternal, aScope,
      aResultOut);
}

void nsGlobalWindowInner::NoteCalledRegisterForServiceWorkerScope(
    const nsACString& aScope) {
  if (!mClientSource) {
    return;
  }

  mClientSource->NoteCalledRegisterForServiceWorkerScope(aScope);
}

void nsGlobalWindowInner::NoteDOMContentLoaded() {
  if (!mClientSource) {
    return;
  }

  mClientSource->NoteDOMContentLoaded();
}

void nsGlobalWindowInner::UpdateTopInnerWindow() {
  if (IsTopInnerWindow() || !mTopInnerWindow) {
    return;
  }

  mTopInnerWindow->UpdateWebSocketCount(-(int32_t)mNumOfOpenWebSockets);
}

bool nsGlobalWindowInner::IsSharedMemoryAllowed() const {
  MOZ_ASSERT(NS_IsMainThread());

  if (StaticPrefs::
          dom_postMessage_sharedArrayBuffer_bypassCOOP_COEP_insecure_enabled()) {
    return true;
  }

  return CrossOriginIsolated();
}

bool nsGlobalWindowInner::CrossOriginIsolated() const {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::dom_postMessage_sharedArrayBuffer_withCOOP_COEP()) {
    return false;
  }

  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  MOZ_DIAGNOSTIC_ASSERT(bc);
  if (bc->Top()->GetOpenerPolicy() !=
      nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP) {
    return false;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  if (!cc ||
      !StringBeginsWith(cc->GetRemoteType(),
                        NS_LITERAL_STRING(WITH_COOP_COEP_REMOTE_TYPE_PREFIX))) {
    return false;
  }

  return true;
}

void nsPIDOMWindowInner::AddPeerConnection() {
  MOZ_ASSERT(NS_IsMainThread());
  mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections++
                  : mActivePeerConnections++;
}

void nsPIDOMWindowInner::RemovePeerConnection() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections
                             : mActivePeerConnections);

  mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections--
                  : mActivePeerConnections--;
}

bool nsPIDOMWindowInner::HasActivePeerConnections() {
  MOZ_ASSERT(NS_IsMainThread());
  return mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections
                         : mActivePeerConnections;
}

bool nsPIDOMWindowInner::IsPlayingAudio() {
  for (uint32_t i = 0; i < mAudioContexts.Length(); i++) {
    if (mAudioContexts[i]->IsRunning()) {
      return true;
    }
  }
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

bool nsPIDOMWindowInner::IsDocumentLoaded() const { return mIsDocumentLoaded; }

mozilla::dom::TimeoutManager& nsPIDOMWindowInner::TimeoutManager() {
  return *mTimeoutManager;
}

bool nsPIDOMWindowInner::IsRunningTimeout() {
  return TimeoutManager().IsRunningTimeout();
}

void nsPIDOMWindowInner::TryToCacheTopInnerWindow() {
  if (mHasTriedToCacheTopInnerWindow) {
    return;
  }

  nsGlobalWindowInner* window = nsGlobalWindowInner::Cast(this);

  MOZ_ASSERT(!window->IsDying());

  mHasTriedToCacheTopInnerWindow = true;

  MOZ_ASSERT(window);

  if (nsCOMPtr<nsPIDOMWindowOuter> topOutter =
          window->GetInProcessScriptableTop()) {
    mTopInnerWindow = topOutter->GetCurrentInnerWindow();
  }
}

void nsPIDOMWindowInner::UpdateActiveIndexedDBDatabaseCount(int32_t aDelta) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aDelta == 0) {
    return;
  }

  // We count databases but not transactions because only active databases
  // could block throttling.
  uint32_t& counter = mTopInnerWindow
                          ? mTopInnerWindow->mNumOfIndexedDBDatabases
                          : mNumOfIndexedDBDatabases;

  counter += aDelta;
}

bool nsPIDOMWindowInner::HasActiveIndexedDBDatabases() {
  MOZ_ASSERT(NS_IsMainThread());

  return mTopInnerWindow ? mTopInnerWindow->mNumOfIndexedDBDatabases > 0
                         : mNumOfIndexedDBDatabases > 0;
}

void nsPIDOMWindowInner::UpdateWebSocketCount(int32_t aDelta) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aDelta == 0) {
    return;
  }

  if (mTopInnerWindow && !IsTopInnerWindow()) {
    mTopInnerWindow->UpdateWebSocketCount(aDelta);
  }

  MOZ_DIAGNOSTIC_ASSERT(
      aDelta > 0 || ((aDelta + mNumOfOpenWebSockets) < mNumOfOpenWebSockets));

  mNumOfOpenWebSockets += aDelta;
}

bool nsPIDOMWindowInner::HasOpenWebSockets() const {
  MOZ_ASSERT(NS_IsMainThread());

  return mNumOfOpenWebSockets ||
         (mTopInnerWindow && mTopInnerWindow->mNumOfOpenWebSockets);
}

bool nsPIDOMWindowInner::IsCurrentInnerWindow() const {
  auto* bc = GetBrowsingContext();
  MOZ_ASSERT(bc);

  if (bc->IsDiscarded()) {
    // If our BrowsingContext has been discarded, we consider ourselves
    // still-current if we were current at the time it was discarded.
    return mOuterWindow && WasCurrentInnerWindow();
  }

  nsPIDOMWindowOuter* outer = bc->GetDOMWindow();
  return outer && outer->GetCurrentInnerWindow() == this;
}

void nsPIDOMWindowInner::SetAudioCapture(bool aCapture) {
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    service->SetWindowAudioCaptured(GetOuterWindow(), mWindowID, aCapture);
  }
}

void nsGlobalWindowInner::SetActiveLoadingState(bool aIsLoading) {
  MOZ_LOG(
      gTimeoutLog, mozilla::LogLevel::Debug,
      ("SetActiveLoadingState innerwindow %p: %d", (void*)this, aIsLoading));
  if (GetBrowsingContext()) {
    GetBrowsingContext()->SetLoading(aIsLoading);
  }

  if (!nsGlobalWindowInner::Cast(this)->IsChromeWindow()) {
    mTimeoutManager->SetLoading(aIsLoading);
  }

  HintIsLoading(aIsLoading);
}

void nsGlobalWindowInner::HintIsLoading(bool aIsLoading) {
  // Hint to tell the JS GC to use modified triggers during pageload.
  if (mHintedWasLoading != aIsLoading) {
    using namespace js::gc;
    SetPerformanceHint(danger::GetJSContext(), aIsLoading
                                                   ? PerformanceHint::InPageLoad
                                                   : PerformanceHint::Normal);
    mHintedWasLoading = aIsLoading;
  }
}

// nsISpeechSynthesisGetter

#ifdef MOZ_WEBSPEECH
SpeechSynthesis* nsGlobalWindowInner::GetSpeechSynthesis(ErrorResult& aError) {
  if (!mSpeechSynthesis) {
    mSpeechSynthesis = new SpeechSynthesis(this);
  }

  return mSpeechSynthesis;
}

bool nsGlobalWindowInner::HasActiveSpeechSynthesis() {
  if (mSpeechSynthesis) {
    return !mSpeechSynthesis->HasEmptyQueue();
  }

  return false;
}

#endif

Nullable<WindowProxyHolder> nsGlobalWindowInner::GetParent(
    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetParentOuter, (), aError, nullptr);
}

/**
 * GetInProcessScriptableParent is called when script reads window.parent.
 *
 * In contrast to GetRealParent, GetInProcessScriptableParent respects <iframe
 * mozbrowser> boundaries, so if |this| is contained by an <iframe
 * mozbrowser>, we will return |this| as its own parent.
 */
nsPIDOMWindowOuter* nsGlobalWindowInner::GetInProcessScriptableParent() {
  FORWARD_TO_OUTER(GetInProcessScriptableParent, (), nullptr);
}

/**
 * GetInProcessScriptableTop is called when script reads window.top.
 *
 * In contrast to GetRealTop, GetInProcessScriptableTop respects <iframe
 * mozbrowser> boundaries.  If we encounter a window owned by an <iframe
 * mozbrowser> while walking up the window hierarchy, we'll stop and return that
 * window.
 */
nsPIDOMWindowOuter* nsGlobalWindowInner::GetInProcessScriptableTop() {
  FORWARD_TO_OUTER(GetInProcessScriptableTop, (), nullptr);
}

void nsGlobalWindowInner::GetContent(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     CallerType aCallerType,
                                     ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetContentOuter,
                            (aCx, aRetval, aCallerType, aError), aError, );
}

BarProp* nsGlobalWindowInner::GetMenubar(ErrorResult& aError) {
  if (!mMenubar) {
    mMenubar = new MenubarProp(this);
  }

  return mMenubar;
}

BarProp* nsGlobalWindowInner::GetToolbar(ErrorResult& aError) {
  if (!mToolbar) {
    mToolbar = new ToolbarProp(this);
  }

  return mToolbar;
}

BarProp* nsGlobalWindowInner::GetLocationbar(ErrorResult& aError) {
  if (!mLocationbar) {
    mLocationbar = new LocationbarProp(this);
  }
  return mLocationbar;
}

BarProp* nsGlobalWindowInner::GetPersonalbar(ErrorResult& aError) {
  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarProp(this);
  }
  return mPersonalbar;
}

BarProp* nsGlobalWindowInner::GetStatusbar(ErrorResult& aError) {
  if (!mStatusbar) {
    mStatusbar = new StatusbarProp(this);
  }
  return mStatusbar;
}

BarProp* nsGlobalWindowInner::GetScrollbars(ErrorResult& aError) {
  if (!mScrollbars) {
    mScrollbars = new ScrollbarsProp(this);
  }

  return mScrollbars;
}

bool nsGlobalWindowInner::GetClosed(ErrorResult& aError) {
  // If we're called from JS (which is the only way we should be getting called
  // here) and we reach this point, that means our JS global is the current
  // target of the WindowProxy, which means that we are the "current inner"
  // of our outer. So if FORWARD_TO_OUTER fails to forward, that means the
  // outer is already torn down, which corresponds to the closed state.
  FORWARD_TO_OUTER(GetClosedOuter, (), true);
}

Nullable<WindowProxyHolder> nsGlobalWindowInner::IndexedGetter(
    uint32_t aIndex) {
  FORWARD_TO_OUTER(IndexedGetterOuter, (aIndex), nullptr);
}

namespace {

struct InterfaceShimEntry {
  const char* geckoName;
  const char* domName;
};

}  // anonymous namespace

// We add shims from Components.interfaces.nsIDOMFoo to window.Foo for each
// interface that has interface constants that sites might be getting off
// of Ci.
const InterfaceShimEntry kInterfaceShimMap[] = {
    {"nsIXMLHttpRequest", "XMLHttpRequest"},
    {"nsIDOMDOMException", "DOMException"},
    {"nsIDOMNode", "Node"},
    {"nsIDOMCSSRule", "CSSRule"},
    {"nsIDOMEvent", "Event"},
    {"nsIDOMNSEvent", "Event"},
    {"nsIDOMKeyEvent", "KeyEvent"},
    {"nsIDOMMouseEvent", "MouseEvent"},
    {"nsIDOMMouseScrollEvent", "MouseScrollEvent"},
    {"nsIDOMMutationEvent", "MutationEvent"},
    {"nsIDOMUIEvent", "UIEvent"},
    {"nsIDOMHTMLMediaElement", "HTMLMediaElement"},
    {"nsIDOMRange", "Range"},
    // Think about whether Ci.nsINodeFilter can just go away for websites!
    {"nsIDOMNodeFilter", "NodeFilter"},
    {"nsIDOMXPathResult", "XPathResult"}};

bool nsGlobalWindowInner::ResolveComponentsShim(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) {
  // Keep track of how often this happens.
  Telemetry::Accumulate(Telemetry::COMPONENTS_SHIM_ACCESSED_BY_CONTENT, true);

  // Warn once.
  nsCOMPtr<Document> doc = GetExtantDoc();
  if (doc) {
    doc->WarnOnceAbout(Document::eComponents, /* asError = */ true);
  }

  // Create a fake Components object.
  AssertSameCompartment(aCx, aGlobal);
  JS::Rooted<JSObject*> components(aCx, JS_NewPlainObject(aCx));
  if (NS_WARN_IF(!components)) {
    return false;
  }

  // Create a fake interfaces object.
  JS::Rooted<JSObject*> interfaces(aCx, JS_NewPlainObject(aCx));
  if (NS_WARN_IF(!interfaces)) {
    return false;
  }
  bool ok =
      JS_DefineProperty(aCx, components, "interfaces", interfaces,
                        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
  if (NS_WARN_IF(!ok)) {
    return false;
  }

  // Define a bunch of shims from the Ci.nsIDOMFoo to window.Foo for DOM
  // interfaces with constants.
  for (uint32_t i = 0; i < ArrayLength(kInterfaceShimMap); ++i) {
    // Grab the names from the table.
    const char* geckoName = kInterfaceShimMap[i].geckoName;
    const char* domName = kInterfaceShimMap[i].domName;

    // Look up the appopriate interface object on the global.
    JS::Rooted<JS::Value> v(aCx, JS::UndefinedValue());
    ok = JS_GetProperty(aCx, aGlobal, domName, &v);
    if (NS_WARN_IF(!ok)) {
      return false;
    }
    if (!v.isObject()) {
      NS_WARNING("Unable to find interface object on global");
      continue;
    }

    // Define the shim on the interfaces object.
    ok = JS_DefineProperty(
        aCx, interfaces, geckoName, v,
        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
    if (NS_WARN_IF(!ok)) {
      return false;
    }
  }

  FillPropertyDescriptor(aDesc, aGlobal, JS::ObjectValue(*components), false);

  return true;
}

#ifdef RELEASE_OR_BETA
#  define USE_CONTROLLERS_SHIM
#endif

#ifdef USE_CONTROLLERS_SHIM
static const JSClass ControllersShimClass = {"Controllers", 0};
static const JSClass XULControllersShimClass = {"XULControllers", 0};
#endif

bool nsGlobalWindowInner::DoResolve(
    JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) {
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

  // We support a cut-down Components.interfaces in case websites are
  // using Components.interfaces.nsIFoo.CONSTANT_NAME for the ones
  // that have constants.
  if (StaticPrefs::dom_use_components_shim() &&
      aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS)) {
    return ResolveComponentsShim(aCx, aObj, aDesc);
  }

  // We also support a "window.controllers" thing; apparently some
  // sites use it for browser-sniffing.  See bug 1010577.
#ifdef USE_CONTROLLERS_SHIM
  // Note: We use |aObj| rather than |this| to get the principal here, because
  // this is called during Window setup when the Document isn't necessarily
  // hooked up yet.
  if ((aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS) ||
       aId == XPCJSRuntime::Get()->GetStringID(
                  XPCJSContext::IDX_CONTROLLERS_CLASS)) &&
      !xpc::IsXrayWrapper(aObj) &&
      !nsContentUtils::ObjectPrincipal(aObj)->IsSystemPrincipal()) {
    if (GetExtantDoc()) {
      GetExtantDoc()->WarnOnceAbout(Document::eWindow_Cc_ontrollers);
    }
    const JSClass* clazz;
    if (aId ==
        XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS)) {
      clazz = &XULControllersShimClass;
    } else {
      clazz = &ControllersShimClass;
    }
    MOZ_ASSERT(JS_IsGlobalObject(aObj));
    JS::Rooted<JSObject*> shim(aCx, JS_NewObject(aCx, clazz));
    if (NS_WARN_IF(!shim)) {
      return false;
    }
    FillPropertyDescriptor(aDesc, aObj, JS::ObjectValue(*shim),
                           /* readOnly = */ false);
    return true;
  }
#endif

  return true;
}

/* static */
bool nsGlobalWindowInner::MayResolve(jsid aId) {
  // Note: This function does not fail and may not have any side-effects.
  // Note: Keep this in sync with DoResolve.
  if (!JSID_IS_STRING(aId)) {
    return false;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS)) {
    return true;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS) ||
      aId == XPCJSRuntime::Get()->GetStringID(
                 XPCJSContext::IDX_CONTROLLERS_CLASS)) {
    // We only resolve .controllers/.Controllers in release builds and on
    // non-chrome windows, but let's not worry about any of that stuff.
    return true;
  }

  return WebIDLGlobalNameHash::MayResolve(aId);
}

void nsGlobalWindowInner::GetOwnPropertyNames(
    JSContext* aCx, JS::MutableHandleVector<jsid> aNames, bool aEnumerableOnly,
    ErrorResult& aRv) {
  if (aEnumerableOnly) {
    // The names we would return from here get defined on the window via one of
    // two codepaths.  The ones coming from the WebIDLGlobalNameHash will end up
    // in the DefineConstructor function in BindingUtils, which always defines
    // things as non-enumerable.  The ones coming from the script namespace
    // manager get defined by our resolve hook using FillPropertyDescriptor with
    // 0 for the property attributes, so non-enumerable as well.
    //
    // So in the aEnumerableOnly case we have nothing to do.
    return;
  }

  // "Components" is marked as enumerable but only resolved on demand :-/.
  // aNames.AppendElement(NS_LITERAL_STRING("Components"));

  JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());

  // There are actually two ways we can get called here: For normal
  // enumeration or for Xray enumeration.  In the latter case, we want to
  // return all possible WebIDL names, because we don't really support
  // deleting these names off our Xray; trying to resolve them will just make
  // them come back.  In the former case, we want to avoid returning deleted
  // names.  But the JS engine already knows about the non-deleted
  // already-resolved names, so we can just return the so-far-unresolved ones.
  //
  // We can tell which case we're in by whether aCx is in our wrapper's
  // compartment.  If not, we're in the Xray case.
  WebIDLGlobalNameHash::NameType nameType =
      js::IsObjectInContextCompartment(wrapper, aCx)
          ? WebIDLGlobalNameHash::UnresolvedNamesOnly
          : WebIDLGlobalNameHash::AllNames;
  if (!WebIDLGlobalNameHash::GetNames(aCx, wrapper, nameType, aNames)) {
    aRv.NoteJSContextException(aCx);
  }
}

/* static */
bool nsGlobalWindowInner::IsPrivilegedChromeWindow(JSContext* aCx,
                                                   JSObject* aObj) {
  // For now, have to deal with XPConnect objects here.
  nsGlobalWindowInner* win = xpc::WindowOrNull(aObj);
  return win && win->IsChromeWindow() &&
         nsContentUtils::ObjectPrincipal(aObj) ==
             nsContentUtils::GetSystemPrincipal();
}

/* static */
bool nsGlobalWindowInner::IsRequestIdleCallbackEnabled(JSContext* aCx,
                                                       JSObject* aObj) {
  // The requestIdleCallback should always be enabled for system code.
  return StaticPrefs::dom_requestIdleCallback_enabled() ||
         nsContentUtils::IsSystemCaller(aCx);
}

/* static */
bool nsGlobalWindowInner::RegisterProtocolHandlerAllowedForContext(
    JSContext* aCx, JSObject* aObj) {
  return IsSecureContextOrObjectIsFromSecureContext(aCx, aObj) ||
         Preferences::GetBool("dom.registerProtocolHandler.insecure.enabled");
}

/* static */
bool nsGlobalWindowInner::DeviceSensorsEnabled(JSContext* aCx, JSObject* aObj) {
  return Preferences::GetBool("device.sensors.enabled");
}

nsDOMOfflineResourceList* nsGlobalWindowInner::GetApplicationCache(
    ErrorResult& aError) {
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
                                     this);

    applicationCache->Init();

    mApplicationCache = applicationCache;
  }

  return mApplicationCache;
}

nsDOMOfflineResourceList* nsGlobalWindowInner::GetApplicationCache() {
  return GetApplicationCache(IgnoreErrors());
}

Crypto* nsGlobalWindowInner::GetCrypto(ErrorResult& aError) {
  if (!mCrypto) {
    mCrypto = new Crypto(this);
  }
  return mCrypto;
}

mozilla::dom::U2F* nsGlobalWindowInner::GetU2f(ErrorResult& aError) {
  if (!mU2F) {
    RefPtr<U2F> u2f = new U2F(this);
    u2f->Init(aError);
    if (NS_WARN_IF(aError.Failed())) {
      return nullptr;
    }

    mU2F = u2f;
  }
  return mU2F;
}

nsIControllers* nsGlobalWindowInner::GetControllers(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetControllersOuter, (aError), aError, nullptr);
}

nsresult nsGlobalWindowInner::GetControllers(nsIControllers** aResult) {
  ErrorResult rv;
  nsCOMPtr<nsIControllers> controllers = GetControllers(rv);
  controllers.forget(aResult);

  return rv.StealNSResult();
}

Nullable<WindowProxyHolder> nsGlobalWindowInner::GetOpenerWindow(
    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetOpenerWindowOuter, (), aError, nullptr);
}

void nsGlobalWindowInner::GetOpener(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aRetval,
                                    ErrorResult& aError) {
  Nullable<WindowProxyHolder> opener = GetOpenerWindow(aError);
  if (aError.Failed() || opener.IsNull()) {
    aRetval.setNull();
    return;
  }

  if (!ToJSValue(aCx, opener.Value(), aRetval)) {
    aError.NoteJSContextException(aCx);
  }
}

void nsGlobalWindowInner::SetOpener(JSContext* aCx,
                                    JS::Handle<JS::Value> aOpener,
                                    ErrorResult& aError) {
  if (aOpener.isNull()) {
    RefPtr<BrowsingContext> bc(GetBrowsingContext());
    if (!bc->IsDiscarded()) {
      bc->SetOpener(nullptr);
    }
    return;
  }

  // If something other than null is passed, just define aOpener on our inner
  // window's JS object, wrapped into the current compartment so that for Xrays
  // we define on the Xray expando object, but don't set it on the outer window,
  // so that it'll get reset on navigation.  This is just like replaceable
  // properties, but we're not quite readonly.
  RedefineProperty(aCx, "opener", aOpener, aError);
}

void nsGlobalWindowInner::GetEvent(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aRetval) {
  if (mEvent) {
    Unused << nsContentUtils::WrapNative(aCx, mEvent, aRetval);
  } else {
    aRetval.setUndefined();
  }
}

void nsGlobalWindowInner::GetStatus(nsAString& aStatus, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetStatusOuter, (aStatus), aError, );
}

void nsGlobalWindowInner::SetStatus(const nsAString& aStatus,
                                    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetStatusOuter, (aStatus), aError, );
}

void nsGlobalWindowInner::GetName(nsAString& aName, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetNameOuter, (aName), aError, );
}

void nsGlobalWindowInner::SetName(const nsAString& aName,
                                  mozilla::ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetNameOuter, (aName, aError), aError, );
}

int32_t nsGlobalWindowInner::GetInnerWidth(CallerType aCallerType,
                                           ErrorResult& aError) {
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindowInner::GetInnerWidth(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerWidthOuter, (aError), aError, 0);
}

void nsGlobalWindowInner::GetInnerWidth(JSContext* aCx,
                                        JS::MutableHandle<JS::Value> aValue,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetInnerWidth, aValue,
                            aCallerType, aError);
}

nsresult nsGlobalWindowInner::GetInnerWidth(int32_t* aInnerWidth) {
  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerWidth = GetInnerWidth(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void nsGlobalWindowInner::SetInnerWidth(int32_t aInnerWidth,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetInnerWidthOuter,
                            (aInnerWidth, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::SetInnerWidth(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetInnerWidth, aValue,
                            "innerWidth", aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetInnerHeight(CallerType aCallerType,
                                            ErrorResult& aError) {
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindowInner::GetInnerHeight(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerHeightOuter, (aError), aError, 0);
}

void nsGlobalWindowInner::GetInnerHeight(JSContext* aCx,
                                         JS::MutableHandle<JS::Value> aValue,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetInnerHeight, aValue,
                            aCallerType, aError);
}

nsresult nsGlobalWindowInner::GetInnerHeight(int32_t* aInnerHeight) {
  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerHeight = GetInnerHeight(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void nsGlobalWindowInner::SetInnerHeight(int32_t aInnerHeight,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetInnerHeightOuter,
                            (aInnerHeight, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::SetInnerHeight(JSContext* aCx,
                                         JS::Handle<JS::Value> aValue,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetInnerHeight, aValue,
                            "innerHeight", aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetOuterWidth(CallerType aCallerType,
                                           ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetOuterWidthOuter, (aCallerType, aError), aError,
                            0);
}

void nsGlobalWindowInner::GetOuterWidth(JSContext* aCx,
                                        JS::MutableHandle<JS::Value> aValue,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetOuterWidth, aValue,
                            aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetOuterHeight(CallerType aCallerType,
                                            ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetOuterHeightOuter, (aCallerType, aError), aError,
                            0);
}

void nsGlobalWindowInner::GetOuterHeight(JSContext* aCx,
                                         JS::MutableHandle<JS::Value> aValue,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetOuterHeight, aValue,
                            aCallerType, aError);
}

void nsGlobalWindowInner::SetOuterWidth(int32_t aOuterWidth,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetOuterWidthOuter,
                            (aOuterWidth, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::SetOuterWidth(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue,
                                        CallerType aCallerType,
                                        ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetOuterWidth, aValue,
                            "outerWidth", aCallerType, aError);
}

void nsGlobalWindowInner::SetOuterHeight(int32_t aOuterHeight,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetOuterHeightOuter,
                            (aOuterHeight, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::SetOuterHeight(JSContext* aCx,
                                         JS::Handle<JS::Value> aValue,
                                         CallerType aCallerType,
                                         ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetOuterHeight, aValue,
                            "outerHeight", aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetScreenX(CallerType aCallerType,
                                        ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScreenXOuter, (aCallerType, aError), aError, 0);
}

void nsGlobalWindowInner::GetScreenX(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aValue,
                                     CallerType aCallerType,
                                     ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetScreenX, aValue,
                            aCallerType, aError);
}

float nsGlobalWindowInner::GetMozInnerScreenX(CallerType aCallerType,
                                              ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenXOuter, (aCallerType), aError, 0);
}

float nsGlobalWindowInner::GetMozInnerScreenY(CallerType aCallerType,
                                              ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenYOuter, (aCallerType), aError, 0);
}

double nsGlobalWindowInner::GetDevicePixelRatio(CallerType aCallerType,
                                                ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetDevicePixelRatioOuter, (aCallerType), aError,
                            0.0);
}

uint64_t nsGlobalWindowInner::GetMozPaintCount(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetMozPaintCountOuter, (), aError, 0);
}

int32_t nsGlobalWindowInner::RequestAnimationFrame(
    FrameRequestCallback& aCallback, ErrorResult& aError) {
  if (!mDoc) {
    return 0;
  }

  if (GetWrapperPreserveColor()) {
    js::NotifyAnimationActivity(GetWrapperPreserveColor());
  }

  DebuggerNotificationDispatch(this,
                               DebuggerNotificationType::RequestAnimationFrame);

  int32_t handle;
  aError = mDoc->ScheduleFrameRequestCallback(aCallback, &handle);
  return handle;
}

void nsGlobalWindowInner::CancelAnimationFrame(int32_t aHandle,
                                               ErrorResult& aError) {
  if (!mDoc) {
    return;
  }

  DebuggerNotificationDispatch(this,
                               DebuggerNotificationType::CancelAnimationFrame);

  mDoc->CancelFrameRequestCallback(aHandle);
}

already_AddRefed<MediaQueryList> nsGlobalWindowInner::MatchMedia(
    const nsAString& aMediaQueryList, CallerType aCallerType,
    ErrorResult& aError) {
  // FIXME: This whole forward-to-outer and then get a pres
  // shell/context off the docshell dance is sort of silly; it'd make
  // more sense to forward to the inner, but it's what everyone else
  // (GetSelection, GetScrollXY, etc.) does around here.
  FORWARD_TO_OUTER_OR_THROW(MatchMediaOuter, (aMediaQueryList, aCallerType),
                            aError, nullptr);
}

void nsGlobalWindowInner::SetScreenX(int32_t aScreenX, CallerType aCallerType,
                                     ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetScreenXOuter, (aScreenX, aCallerType, aError),
                            aError, );
}

void nsGlobalWindowInner::SetScreenX(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     CallerType aCallerType,
                                     ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetScreenX, aValue,
                            "screenX", aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetScreenY(CallerType aCallerType,
                                        ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScreenYOuter, (aCallerType, aError), aError, 0);
}

void nsGlobalWindowInner::GetScreenY(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aValue,
                                     CallerType aCallerType,
                                     ErrorResult& aError) {
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetScreenY, aValue,
                            aCallerType, aError);
}

void nsGlobalWindowInner::SetScreenY(int32_t aScreenY, CallerType aCallerType,
                                     ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetScreenYOuter, (aScreenY, aCallerType, aError),
                            aError, );
}

void nsGlobalWindowInner::SetScreenY(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue,
                                     CallerType aCallerType,
                                     ErrorResult& aError) {
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetScreenY, aValue,
                            "screenY", aCallerType, aError);
}

int32_t nsGlobalWindowInner::GetScrollMinX(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideLeft), aError, 0);
}

int32_t nsGlobalWindowInner::GetScrollMinY(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideTop), aError, 0);
}

int32_t nsGlobalWindowInner::GetScrollMaxX(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideRight), aError, 0);
}

int32_t nsGlobalWindowInner::GetScrollMaxY(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideBottom), aError, 0);
}

double nsGlobalWindowInner::GetScrollX(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollXOuter, (), aError, 0);
}

double nsGlobalWindowInner::GetScrollY(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetScrollYOuter, (), aError, 0);
}

uint32_t nsGlobalWindowInner::Length() { FORWARD_TO_OUTER(Length, (), 0); }

Nullable<WindowProxyHolder> nsGlobalWindowInner::GetTop(
    mozilla::ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetTopOuter, (), aError, nullptr);
}

already_AddRefed<BrowsingContext> nsGlobalWindowInner::GetChildWindow(
    const nsAString& aName) {
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->GetChildWindow(aName);
  }
  return nullptr;
}

void nsGlobalWindowInner::RefreshRealmPrincipal() {
  JS::SetRealmPrincipals(js::GetNonCCWObjectRealm(GetWrapperPreserveColor()),
                         nsJSPrincipals::get(mDoc->NodePrincipal()));
}

already_AddRefed<nsIWidget> nsGlobalWindowInner::GetMainWidget() {
  FORWARD_TO_OUTER(GetMainWidget, (), nullptr);
}

nsIWidget* nsGlobalWindowInner::GetNearestWidget() const {
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->GetNearestWidget();
  }
  return nullptr;
}

void nsGlobalWindowInner::SetFullScreen(bool aFullscreen,
                                        mozilla::ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetFullscreenOuter, (aFullscreen, aError), aError,
                            /* void */);
}

bool nsGlobalWindowInner::GetFullScreen(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetFullscreenOuter, (), aError, false);
}

bool nsGlobalWindowInner::GetFullScreen() {
  ErrorResult dummy;
  bool fullscreen = GetFullScreen(dummy);
  dummy.SuppressException();
  return fullscreen;
}

void nsGlobalWindowInner::Dump(const nsAString& aStr) {
  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

  char* cstr = ToNewUTF8String(aStr);

#if defined(XP_MACOSX)
  // have to convert \r to \n so that printing to the console works
  char *c = cstr, *cEnd = cstr + strlen(cstr);
  while (c < cEnd) {
    if (*c == '\r') *c = '\n';
    c++;
  }
#endif

  if (cstr) {
    MOZ_LOG(nsContentUtils::DOMDumpLog(), LogLevel::Debug,
            ("[Window.Dump] %s", cstr));
#ifdef XP_WIN
    PrintToDebugger(cstr);
#endif
#ifdef ANDROID
    __android_log_write(ANDROID_LOG_INFO, "GeckoDump", cstr);
#endif
    FILE* fp = gDumpFile ? gDumpFile : stdout;
    fputs(cstr, fp);
    fflush(fp);
    free(cstr);
  }
}

void nsGlobalWindowInner::Alert(nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aError) {
  Alert(EmptyString(), aSubjectPrincipal, aError);
}

void nsGlobalWindowInner::Alert(const nsAString& aMessage,
                                nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(AlertOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, );
}

bool nsGlobalWindowInner::Confirm(const nsAString& aMessage,
                                  nsIPrincipal& aSubjectPrincipal,
                                  ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(ConfirmOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, false);
}

already_AddRefed<Promise> nsGlobalWindowInner::Fetch(
    const RequestOrUSVString& aInput, const RequestInit& aInit,
    CallerType aCallerType, ErrorResult& aRv) {
  return FetchRequest(this, aInput, aInit, aCallerType, aRv);
}

void nsGlobalWindowInner::Prompt(const nsAString& aMessage,
                                 const nsAString& aInitial, nsAString& aReturn,
                                 nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(
      PromptOuter, (aMessage, aInitial, aReturn, aSubjectPrincipal, aError),
      aError, );
}

void nsGlobalWindowInner::Focus(CallerType aCallerType, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(FocusOuter, (aCallerType), aError, );
}

nsresult nsGlobalWindowInner::Focus(CallerType aCallerType) {
  ErrorResult rv;
  Focus(aCallerType, rv);

  return rv.StealNSResult();
}

void nsGlobalWindowInner::Blur(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(BlurOuter, (), aError, );
}

void nsGlobalWindowInner::Stop(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(StopOuter, (aError), aError, );
}

void nsGlobalWindowInner::Print(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(PrintOuter, (aError), aError, );
}

void nsGlobalWindowInner::MoveTo(int32_t aXPos, int32_t aYPos,
                                 CallerType aCallerType, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(MoveToOuter, (aXPos, aYPos, aCallerType, aError),
                            aError, );
}

void nsGlobalWindowInner::MoveBy(int32_t aXDif, int32_t aYDif,
                                 CallerType aCallerType, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(MoveByOuter, (aXDif, aYDif, aCallerType, aError),
                            aError, );
}

void nsGlobalWindowInner::ResizeTo(int32_t aWidth, int32_t aHeight,
                                   CallerType aCallerType,
                                   ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(ResizeToOuter,
                            (aWidth, aHeight, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                                   CallerType aCallerType,
                                   ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(
      ResizeByOuter, (aWidthDif, aHeightDif, aCallerType, aError), aError, );
}

void nsGlobalWindowInner::SizeToContent(CallerType aCallerType,
                                        ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SizeToContentOuter, (aCallerType, aError),
                            aError, );
}

already_AddRefed<nsPIWindowRoot> nsGlobalWindowInner::GetTopWindowRoot() {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    return nullptr;
  }
  return outer->GetTopWindowRoot();
}

void nsGlobalWindowInner::Scroll(double aXScroll, double aYScroll) {
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void nsGlobalWindowInner::ScrollTo(double aXScroll, double aYScroll) {
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void nsGlobalWindowInner::ScrollTo(const ScrollToOptions& aOptions) {
  // When scrolling to a non-zero offset, we need to determine whether that
  // position is within our scrollable range, so we need updated layout
  // information which requires a layout flush, otherwise all we need is to
  // flush frames to be able to access our scrollable frame here.
  FlushType flushType =
      ((aOptions.mLeft.WasPassed() && aOptions.mLeft.Value() > 0) ||
       (aOptions.mTop.WasPassed() && aOptions.mTop.Value() > 0))
          ? FlushType::Layout
          : FlushType::Frames;
  FlushPendingNotifications(flushType);
  nsIScrollableFrame* sf = GetScrollFrame();

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

void nsGlobalWindowInner::Scroll(const ScrollToOptions& aOptions) {
  ScrollTo(aOptions);
}

void nsGlobalWindowInner::ScrollTo(const CSSIntPoint& aScroll,
                                   const ScrollOptions& aOptions) {
  // When scrolling to a non-zero offset, we need to determine whether that
  // position is within our scrollable range, so we need updated layout
  // information which requires a layout flush, otherwise all we need is to
  // flush frames to be able to access our scrollable frame here.
  FlushType flushType =
      (aScroll.x || aScroll.y) ? FlushType::Layout : FlushType::Frames;
  FlushPendingNotifications(flushType);
  nsIScrollableFrame* sf = GetScrollFrame();

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

    ScrollMode scrollMode = sf->IsSmoothScroll(aOptions.mBehavior)
                                ? ScrollMode::SmoothMsd
                                : ScrollMode::Instant;

    sf->ScrollToCSSPixels(scroll, scrollMode);
  }
}

void nsGlobalWindowInner::ScrollBy(double aXScrollDif, double aYScrollDif) {
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame* sf = GetScrollFrame();

  if (sf) {
    // It seems like it would make more sense for ScrollBy to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    ScrollToOptions options;
    options.mLeft.Construct(aXScrollDif);
    options.mTop.Construct(aYScrollDif);
    ScrollBy(options);
  }
}

void nsGlobalWindowInner::ScrollBy(const ScrollToOptions& aOptions) {
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame* sf = GetScrollFrame();

  if (sf) {
    CSSIntPoint scrollDelta;
    if (aOptions.mLeft.WasPassed()) {
      scrollDelta.x = mozilla::ToZeroIfNonfinite(aOptions.mLeft.Value());
    }
    if (aOptions.mTop.WasPassed()) {
      scrollDelta.y = mozilla::ToZeroIfNonfinite(aOptions.mTop.Value());
    }

    ScrollMode scrollMode = sf->IsSmoothScroll(aOptions.mBehavior)
                                ? ScrollMode::SmoothMsd
                                : ScrollMode::Instant;

    sf->ScrollByCSSPixels(scrollDelta, scrollMode, nsGkAtoms::relative);
  }
}

void nsGlobalWindowInner::ScrollByLines(int32_t numLines,
                                        const ScrollOptions& aOptions) {
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByLines to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    ScrollMode scrollMode = sf->IsSmoothScroll(aOptions.mBehavior)
                                ? ScrollMode::SmoothMsd
                                : ScrollMode::Instant;

    sf->ScrollBy(nsIntPoint(0, numLines), ScrollUnit::LINES, scrollMode);
  }
}

void nsGlobalWindowInner::ScrollByPages(int32_t numPages,
                                        const ScrollOptions& aOptions) {
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    // It seems like it would make more sense for ScrollByPages to use
    // SMOOTH mode, but tests seem to depend on the synchronous behaviour.
    // Perhaps Web content does too.
    ScrollMode scrollMode = sf->IsSmoothScroll(aOptions.mBehavior)
                                ? ScrollMode::SmoothMsd
                                : ScrollMode::Instant;

    sf->ScrollBy(nsIntPoint(0, numPages), ScrollUnit::PAGES, scrollMode);
  }
}

void nsGlobalWindowInner::MozScrollSnap() {
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame* sf = GetScrollFrame();
  if (sf) {
    sf->ScrollSnap();
  }
}

void nsGlobalWindowInner::ClearTimeout(int32_t aHandle) {
  DebuggerNotificationDispatch(this, DebuggerNotificationType::ClearTimeout);

  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
  }
}

void nsGlobalWindowInner::ClearInterval(int32_t aHandle) {
  DebuggerNotificationDispatch(this, DebuggerNotificationType::ClearInterval);

  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
  }
}

void nsGlobalWindowInner::SetResizable(bool aResizable) const {
  // nop
}

void nsGlobalWindowInner::CaptureEvents() {
  if (mDoc) {
    mDoc->WarnOnceAbout(Document::eUseOfCaptureEvents);
  }
}

void nsGlobalWindowInner::ReleaseEvents() {
  if (mDoc) {
    mDoc->WarnOnceAbout(Document::eUseOfReleaseEvents);
  }
}

Nullable<WindowProxyHolder> nsGlobalWindowInner::Open(const nsAString& aUrl,
                                                      const nsAString& aName,
                                                      const nsAString& aOptions,
                                                      ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(OpenOuter, (aUrl, aName, aOptions, aError), aError,
                            nullptr);
}

Nullable<WindowProxyHolder> nsGlobalWindowInner::OpenDialog(
    JSContext* aCx, const nsAString& aUrl, const nsAString& aName,
    const nsAString& aOptions, const Sequence<JS::Value>& aExtraArgument,
    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(
      OpenDialogOuter, (aCx, aUrl, aName, aOptions, aExtraArgument, aError),
      aError, nullptr);
}

WindowProxyHolder nsGlobalWindowInner::GetFrames(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetFramesOuter, (), aError, Window());
}

void nsGlobalWindowInner::PostMessageMoz(JSContext* aCx,
                                         JS::Handle<JS::Value> aMessage,
                                         const nsAString& aTargetOrigin,
                                         JS::Handle<JS::Value> aTransfer,
                                         nsIPrincipal& aSubjectPrincipal,
                                         ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(
      PostMessageMozOuter,
      (aCx, aMessage, aTargetOrigin, aTransfer, aSubjectPrincipal, aError),
      aError, );
}

void nsGlobalWindowInner::PostMessageMoz(JSContext* aCx,
                                         JS::Handle<JS::Value> aMessage,
                                         const nsAString& aTargetOrigin,
                                         const Sequence<JSObject*>& aTransfer,
                                         nsIPrincipal& aSubjectPrincipal,
                                         ErrorResult& aRv) {
  JS::Rooted<JS::Value> transferArray(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransfer,
                                                          &transferArray);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  PostMessageMoz(aCx, aMessage, aTargetOrigin, transferArray, aSubjectPrincipal,
                 aRv);
}

void nsGlobalWindowInner::PostMessageMoz(
    JSContext* aCx, JS::Handle<JS::Value> aMessage,
    const WindowPostMessageOptions& aOptions, nsIPrincipal& aSubjectPrincipal,
    ErrorResult& aRv) {
  JS::Rooted<JS::Value> transferArray(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(
      aCx, aOptions.mTransfer, &transferArray);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  PostMessageMoz(aCx, aMessage, aOptions.mTargetOrigin, transferArray,
                 aSubjectPrincipal, aRv);
}

void nsGlobalWindowInner::Close(CallerType aCallerType, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(CloseOuter, (aCallerType == CallerType::System),
                            aError, );
}

nsresult nsGlobalWindowInner::Close() {
  FORWARD_TO_OUTER(Close, (), NS_ERROR_UNEXPECTED);
}

bool nsGlobalWindowInner::IsInModalState() {
  FORWARD_TO_OUTER(IsInModalState, (), false);
}

// static
void nsGlobalWindowInner::NotifyDOMWindowDestroyed(
    nsGlobalWindowInner* aWindow) {
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(ToSupports(aWindow),
                                     DOM_WINDOW_DESTROYED_TOPIC, nullptr);
  }
}

void nsGlobalWindowInner::NotifyWindowIDDestroyed(const char* aTopic) {
  nsCOMPtr<nsIRunnable> runnable =
      new WindowDestroyedEvent(this, mWindowID, aTopic);
  Dispatch(TaskCategory::Other, runnable.forget());
}

// static
void nsGlobalWindowInner::NotifyDOMWindowFrozen(nsGlobalWindowInner* aWindow) {
  if (aWindow) {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aWindow),
                                       DOM_WINDOW_FROZEN_TOPIC, nullptr);
    }
  }
}

// static
void nsGlobalWindowInner::NotifyDOMWindowThawed(nsGlobalWindowInner* aWindow) {
  if (aWindow) {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aWindow),
                                       DOM_WINDOW_THAWED_TOPIC, nullptr);
    }
  }
}

Element* nsGlobalWindowInner::GetFrameElement(nsIPrincipal& aSubjectPrincipal,
                                              ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetFrameElement, (aSubjectPrincipal), aError,
                            nullptr);
}

Element* nsGlobalWindowInner::GetRealFrameElement(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetFrameElement, (), aError, nullptr);
}

void nsGlobalWindowInner::UpdateCommands(const nsAString& anAction,
                                         Selection* aSel, int16_t aReason) {
  if (GetOuterWindowInternal()) {
    GetOuterWindowInternal()->UpdateCommands(anAction, aSel, aReason);
  }
}

Selection* nsGlobalWindowInner::GetSelection(ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetSelectionOuter, (), aError, nullptr);
}

bool nsGlobalWindowInner::Find(const nsAString& aString, bool aCaseSensitive,
                               bool aBackwards, bool aWrapAround,
                               bool aWholeWord, bool aSearchInFrames,
                               bool aShowDialog, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(FindOuter,
                            (aString, aCaseSensitive, aBackwards, aWrapAround,
                             aWholeWord, aSearchInFrames, aShowDialog, aError),
                            aError, false);
}

void nsGlobalWindowInner::GetOrigin(nsAString& aOrigin) {
  nsContentUtils::GetUTFOrigin(GetPrincipal(), aOrigin);
}

void nsGlobalWindowInner::Atob(const nsAString& aAsciiBase64String,
                               nsAString& aBinaryData, ErrorResult& aError) {
  aError = nsContentUtils::Atob(aAsciiBase64String, aBinaryData);
}

void nsGlobalWindowInner::Btoa(const nsAString& aBinaryData,
                               nsAString& aAsciiBase64String,
                               ErrorResult& aError) {
  aError = nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

//*****************************************************************************
// EventTarget
//*****************************************************************************

nsPIDOMWindowOuter* nsGlobalWindowInner::GetOwnerGlobalForBindingsInternal() {
  return nsPIDOMWindowOuter::GetFromCurrentInner(this);
}

bool nsGlobalWindowInner::DispatchEvent(Event& aEvent, CallerType aCallerType,
                                        ErrorResult& aRv) {
  if (!IsCurrentInnerWindow()) {
    NS_WARNING(
        "DispatchEvent called on non-current inner window, dropping. "
        "Please check the window in the caller instead.");
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (!mDoc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  // Obtain a presentation shell
  RefPtr<nsPresContext> presContext = mDoc->GetPresContext();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(
      ToSupports(this), nullptr, &aEvent, presContext, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

bool nsGlobalWindowInner::ComputeDefaultWantsUntrusted(ErrorResult& aRv) {
  return !nsContentUtils::IsChromeDoc(mDoc);
}

EventListenerManager* nsGlobalWindowInner::GetOrCreateListenerManager() {
  if (!mListenerManager) {
    mListenerManager =
        new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager* nsGlobalWindowInner::GetExistingListenerManager() const {
  return mListenerManager;
}

mozilla::dom::DebuggerNotificationManager*
nsGlobalWindowInner::GetOrCreateDebuggerNotificationManager() {
  if (!mDebuggerNotificationManager) {
    mDebuggerNotificationManager = new DebuggerNotificationManager(this);
  }

  return mDebuggerNotificationManager;
}

mozilla::dom::DebuggerNotificationManager*
nsGlobalWindowInner::GetExistingDebuggerNotificationManager() {
  return mDebuggerNotificationManager;
}

//*****************************************************************************
// nsGlobalWindowInner::nsPIDOMWindow
//*****************************************************************************

Location* nsGlobalWindowInner::Location() {
  if (!mLocation) {
    mLocation = new dom::Location(this, GetBrowsingContext());
  }

  return mLocation;
}

void nsGlobalWindowInner::MaybeUpdateTouchState() {
  if (mMayHaveTouchEventListener) {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();

    if (observerService) {
      observerService->NotifyObservers(static_cast<nsIDOMWindow*>(this),
                                       DOM_TOUCH_LISTENER_ADDED, nullptr);
    }
  }
}

void nsGlobalWindowInner::EnableGamepadUpdates() {
  if (mHasGamepad) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->AddListener(this);
    }
  }
}

void nsGlobalWindowInner::DisableGamepadUpdates() {
  if (mHasGamepad) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->RemoveListener(this);
    }
  }
}

void nsGlobalWindowInner::EnableVRUpdates() {
  // We need to create a VREventObserver before we can either detect XR runtimes
  // or start an XR session
  if (!mVREventObserver && (mHasXRSession || mXRRuntimeDetectionInFlight)) {
    // Assert that we are not creating the observer while IsDying() as
    // that would result in a leak.  VREventObserver holds a RefPtr to
    // this nsGlobalWindowInner and would prevent it from being deallocated.
    MOZ_ASSERT(!IsDying(),
               "Creating a VREventObserver for an nsGlobalWindow that is "
               "dying would cause it to leak.");
    mVREventObserver = new VREventObserver(this);
  }
  // If the content has an XR session, then we need to tell
  // VREventObserver that there is VR activity.
  if (mHasXRSession) {
    nsPIDOMWindowOuter* outer = GetOuterWindow();
    if (outer && !outer->IsBackground()) {
      StartVRActivity();
    }
  }
}

void nsGlobalWindowInner::DisableVRUpdates() {
  if (mVREventObserver) {
    mVREventObserver->DisconnectFromOwner();
    mVREventObserver = nullptr;
  }
}

void nsGlobalWindowInner::ResetVRTelemetry(bool aUpdate) {
  if (mVREventObserver) {
    mVREventObserver->UpdateSpentTimeIn2DTelemetry(aUpdate);
  }
}

void nsGlobalWindowInner::StartVRActivity() {
  /**
   * If the content has an XR session, tell
   * the VREventObserver that the window is accessing
   * VR devices.
   *
   * It's possible to have a VREventObserver without
   * and XR session, if we are using it to get updates
   * about XR runtime enumeration.  In this case,
   * we would not tell the VREventObserver that
   * we are accessing VR devices.
   */
  if (mVREventObserver && mHasXRSession) {
    mVREventObserver->StartActivity();
  }
}

void nsGlobalWindowInner::StopVRActivity() {
  /**
   * If the content has an XR session, tell
   * the VReventObserver that the window is no longer
   * accessing VR devices.  This does not stop the
   * XR session itself, which may be resumed with
   * EnableVRUpdates.
   * It's possible to have a VREventObserver without
   * and XR session, if we are using it to get updates
   * about XR runtime enumeration.  In this case,
   * we would not tell the VREventObserver that
   * we ending an activity that accesses VR devices.
   */
  if (mVREventObserver && mHasXRSession) {
    mVREventObserver->StopActivity();
  }
}

#ifndef XP_WIN  // This guard should match the guard at the callsite.
static bool ShouldShowFocusRingIfFocusedByMouse(nsIContent* aNode) {
  if (!aNode) {
    return true;
  }
  return !nsContentUtils::ContentIsLink(aNode) &&
         !aNode->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio);
}
#endif

void nsGlobalWindowInner::SetFocusedElement(Element* aElement,
                                            uint32_t aFocusMethod,
                                            bool aNeedsFocus) {
  if (aElement && aElement->GetComposedDoc() != mDoc) {
    NS_WARNING("Trying to set focus to a node from a wrong document");
    return;
  }

  if (IsDying()) {
    NS_ASSERTION(!aElement, "Trying to focus cleaned up window!");
    aElement = nullptr;
    aNeedsFocus = false;
  }
  if (mFocusedElement != aElement) {
    UpdateCanvasFocus(false, aElement);
    mFocusedElement = aElement;
    mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;
    mShowFocusRingForContent = false;
  }

  if (mFocusedElement) {
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
        ShouldShowFocusRingIfFocusedByMouse(aElement) ||
#endif
        aFocusMethod & nsIFocusManager::FLAG_SHOWRING) {
      mShowFocusRingForContent = true;
    }
  }

  if (aNeedsFocus) mNeedsFocus = aNeedsFocus;
}

uint32_t nsGlobalWindowInner::GetFocusMethod() { return mFocusMethod; }

bool nsGlobalWindowInner::ShouldShowFocusRing() {
  if (mShowFocusRingForContent || mFocusByKeyOccurred) {
    return true;
  }

  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return root ? root->ShowFocusRings() : false;
}

bool nsGlobalWindowInner::TakeFocus(bool aFocus, uint32_t aFocusMethod) {
  if (IsDying()) {
    return false;
  }

  if (aFocus) mFocusMethod = aFocusMethod & FOCUSMETHOD_MASK;

  if (mHasFocus != aFocus) {
    mHasFocus = aFocus;
    UpdateCanvasFocus(true, mFocusedElement);
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

void nsGlobalWindowInner::SetReadyForFocus() {
  bool oldNeedsFocus = mNeedsFocus;
  mNeedsFocus = false;

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->WindowShown(GetOuterWindow(), oldNeedsFocus);
  }
}

void nsGlobalWindowInner::PageHidden() {
  // the window is being hidden, so tell the focus manager that the frame is
  // no longer valid. Use the persisted field to determine if the document
  // is being destroyed.

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->WindowHidden(GetOuterWindow());
  }

  mNeedsFocus = true;
}

class HashchangeCallback : public Runnable {
 public:
  HashchangeCallback(const nsAString& aOldURL, const nsAString& aNewURL,
                     nsGlobalWindowInner* aWindow)
      : mozilla::Runnable("HashchangeCallback"), mWindow(aWindow) {
    MOZ_ASSERT(mWindow);
    mOldURL.Assign(aOldURL);
    mNewURL.Assign(aNewURL);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Should be called on the main thread.");
    return mWindow->FireHashchange(mOldURL, mNewURL);
  }

 private:
  nsString mOldURL;
  nsString mNewURL;
  RefPtr<nsGlobalWindowInner> mWindow;
};

nsresult nsGlobalWindowInner::DispatchAsyncHashchange(nsIURI* aOldURI,
                                                      nsIURI* aNewURI) {
  // Make sure that aOldURI and aNewURI are identical up to the '#', and that
  // their hashes are different.
  bool equal = false;
  NS_ENSURE_STATE(NS_SUCCEEDED(aOldURI->EqualsExceptRef(aNewURI, &equal)) &&
                  equal);
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
  return Dispatch(TaskCategory::Other, callback.forget());
}

nsresult nsGlobalWindowInner::FireHashchange(const nsAString& aOldURL,
                                             const nsAString& aNewURL) {
  // Don't do anything if the window is frozen.
  if (IsFrozen()) {
    return NS_OK;
  }

  // Get a presentation shell for use in creating the hashchange event.
  NS_ENSURE_STATE(IsCurrentInnerWindow());

  HashChangeEventInit init;
  init.mNewURL = aNewURL;
  init.mOldURL = aOldURL;

  RefPtr<HashChangeEvent> event =
      HashChangeEvent::Constructor(this, NS_LITERAL_STRING("hashchange"), init);

  event->SetTrusted(true);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

nsresult nsGlobalWindowInner::DispatchSyncPopState() {
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Must be safe to run script here.");

  // Bail if the window is frozen.
  if (IsFrozen()) {
    return NS_OK;
  }

  // Get the document's pending state object -- it contains the data we're
  // going to send along with the popstate event.  The object is serialized
  // using structured clone.
  nsCOMPtr<nsIVariant> stateObj;
  nsresult rv = mDoc->GetStateObject(getter_AddRefs(stateObj));
  NS_ENSURE_SUCCESS(rv, rv);

  AutoJSAPI jsapi;
  bool result = jsapi.Init(this);
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> stateJSValue(cx, JS::NullValue());
  result = stateObj ? VariantToJsval(cx, stateObj, &stateJSValue) : true;
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  RootedDictionary<PopStateEventInit> init(cx);
  init.mState = stateJSValue;

  RefPtr<PopStateEvent> event =
      PopStateEvent::Constructor(this, NS_LITERAL_STRING("popstate"), init);
  event->SetTrusted(true);
  event->SetTarget(this);

  ErrorResult err;
  DispatchEvent(*event, err);
  return err.StealNSResult();
}

//-------------------------------------------------------
// Tells the HTMLFrame/CanvasFrame that is now has focus
void nsGlobalWindowInner::UpdateCanvasFocus(bool aFocusChanged,
                                            nsIContent* aNewContent) {
  // this is called from the inner window so use GetDocShell
  nsIDocShell* docShell = GetDocShell();
  if (!docShell) return;

  bool editable;
  docShell->GetEditable(&editable);
  if (editable) return;

  PresShell* presShell = docShell->GetPresShell();
  if (!presShell || !mDoc) {
    return;
  }

  Element* rootElement = mDoc->GetRootElement();
  if (rootElement) {
    if ((mHasFocus || aFocusChanged) &&
        (mFocusedElement == rootElement || aNewContent == rootElement)) {
      nsCanvasFrame* canvasFrame = presShell->GetCanvasFrame();
      if (canvasFrame) {
        canvasFrame->SetHasFocus(mHasFocus && rootElement == aNewContent);
      }
    }
  } else {
    // XXXbz I would expect that there is never a canvasFrame in this case...
    nsCanvasFrame* canvasFrame = presShell->GetCanvasFrame();
    if (canvasFrame) {
      canvasFrame->SetHasFocus(false);
    }
  }
}

already_AddRefed<nsICSSDeclaration> nsGlobalWindowInner::GetComputedStyle(
    Element& aElt, const nsAString& aPseudoElt, ErrorResult& aError) {
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aError);
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindowInner::GetDefaultComputedStyle(Element& aElt,
                                             const nsAString& aPseudoElt,
                                             ErrorResult& aError) {
  return GetComputedStyleHelper(aElt, aPseudoElt, true, aError);
}

already_AddRefed<nsICSSDeclaration> nsGlobalWindowInner::GetComputedStyleHelper(
    Element& aElt, const nsAString& aPseudoElt, bool aDefaultStylesOnly,
    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetComputedStyleHelperOuter,
                            (aElt, aPseudoElt, aDefaultStylesOnly), aError,
                            nullptr);
}

Storage* nsGlobalWindowInner::GetSessionStorage(ErrorResult& aError) {
  nsIPrincipal* principal = GetPrincipal();
  BrowsingContext* browsingContext = GetBrowsingContext();

  if (!principal || !browsingContext || !Storage::StoragePrefIsEnabled()) {
    return nullptr;
  }

  if (mSessionStorage) {
    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
            ("nsGlobalWindowInner %p has %p sessionStorage", this,
             mSessionStorage.get()));
    bool canAccess = principal->Subsumes(mSessionStorage->Principal());
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

    uint32_t rejectedReason = 0;
    StorageAccess access = StorageAllowedForWindow(this, &rejectedReason);

    // SessionStorage is an ephemeral per-tab per-origin storage that only lives
    // as long as the tab is open, although it may survive browser restarts
    // thanks to the session store. So we interpret storage access differently
    // than we would for persistent per-origin storage like LocalStorage and so
    // it may be okay to provide SessionStorage even when we receive a value of
    // eDeny.
    //
    // ContentBlocking::ShouldAllowAccessFor will return false for 3 main
    // reasons.
    //
    // 1. Cookies are entirely blocked due to a per-origin permission
    // (nsICookiePermission::ACCESS_DENY for the top-level principal or this
    // window's principal) or the very broad BEHAVIOR_REJECT. This will return
    // eDeny with a reason of STATE_COOKIES_BLOCKED_BY_PERMISSION or
    // STATE_COOKIES_BLOCKED_ALL.
    //
    // 2. Third-party cookies are limited via BEHAVIOR_REJECT_FOREIGN and
    // BEHAVIOR_LIMIT_FOREIGN and this is a third-party window. This will return
    // eDeny with a reason of STATE_COOKIES_BLOCKED_FOREIGN.
    //
    // 3. Tracking protection (BEHAVIOR_REJECT_TRACKER) is in effect and
    // IsThirdPartyTrackingResourceWindow() returned true and there wasn't a
    // permission that allows it. This will return ePartitionTrackersOrDeny with
    // a reason of STATE_COOKIES_BLOCKED_TRACKER or
    // STATE_COOKIES_BLOCKED_SOCIALTRACKER.
    //
    // In the 1st case, the user has explicitly indicated that they don't want
    // to allow any storage to the origin or all origins and so we throw an
    // error and deny access to SessionStorage. In the 2nd case, a legacy
    // decision reasoned that there's no harm in providing SessionStorage
    // because the information is not durable and cannot escape the current tab.
    // The rationale is similar for the 3rd case.
    if (access == StorageAccess::eDeny &&
        rejectedReason !=
            nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    const RefPtr<SessionStorageManager> storageManager =
        browsingContext->GetSessionStorageManager();
    if (!storageManager) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }

    RefPtr<Storage> storage;
    // No StoragePrincipal for sessions.
    aError = storageManager->CreateStorage(this, principal, principal,
                                           documentURI, IsPrivateBrowsing(),
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mSessionStorage = storage;
    MOZ_ASSERT(mSessionStorage);

    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
            ("nsGlobalWindowInner %p tried to get a new sessionStorage %p",
             this, mSessionStorage.get()));

    if (!mSessionStorage) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
          ("nsGlobalWindowInner %p returns %p sessionStorage", this,
           mSessionStorage.get()));

  return mSessionStorage;
}

Storage* nsGlobalWindowInner::GetLocalStorage(ErrorResult& aError) {
  if (!Storage::StoragePrefIsEnabled()) {
    return nullptr;
  }

  // LocalStorage needs to be exposed in every context except for sandboxes and
  // NullPrincipals (data: URLs, for instance). But we need to keep data
  // separate in some scenarios: private-browsing and partitioned trackers.
  // In private-browsing, LocalStorage keeps data in memory, and it shares
  // StorageEvents just with other origins in the same private-browsing
  // environment.
  // For Partitioned Trackers, we expose a partitioned LocalStorage, which
  // doesn't share data with other contexts, and it's just in memory.
  // Partitioned localStorage is available only for trackers listed in the
  // privacy.restrict3rdpartystorage.partitionedHosts pref. See
  // nsContentUtils::IsURIInPrefList to know the syntax for the pref value.
  // This is a temporary web-compatibility hack.

  StorageAccess access = StorageAllowedForWindow(this);

  // We allow partitioned localStorage only to some hosts.
  bool isolated = false;
  if (ShouldPartitionStorage(access)) {
    if (!mDoc) {
      access = StorageAccess::eDeny;
    } else if (!StoragePartitioningEnabled(access, mDoc->CookieJarSettings())) {
      static const char* kPrefName =
          "privacy.restrict3rdpartystorage.partitionedHosts";

      bool isInList = false;
      mDoc->NodePrincipal()->IsURIInPrefList(kPrefName, &isInList);
      if (!isInList) {
        access = StorageAccess::eDeny;
      } else {
        isolated = true;
      }
    }
  }

  if (access == StorageAccess::eDeny) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (mDoc) {
    cookieJarSettings = mDoc->CookieJarSettings();
  } else {
    cookieJarSettings = net::CookieJarSettings::GetBlockingAll();
  }

  bool partitioningEnabled =
      StoragePartitioningEnabled(access, cookieJarSettings);
  bool shouldPartition = ShouldPartitionStorage(access);
  bool partition = partitioningEnabled && shouldPartition;

  // Note that this behavior is observable: if we grant storage permission to a
  // tracker, we pass from the partitioned LocalStorage (or a partitioned cookie
  // jar) to the 'normal' one. The previous data is lost and the 2
  // window.localStorage objects, before and after the permission granted, will
  // be different.
  if ((partitioningEnabled || !shouldPartition) &&
      ((mLocalStorage && ((mLocalStorage->Type() !=
                           (partition ? Storage::ePartitionedLocalStorage
                                      : Storage::eLocalStorage)) ||
                          mLocalStorage->StoragePrincipal() !=
                              GetEffectiveStoragePrincipal())) ||
       (!partition && !mLocalStorage))) {
    RefPtr<Storage> storage;

    if (NextGenLocalStorageEnabled()) {
      aError = LSObject::CreateForWindow(this, getter_AddRefs(storage));
    } else {
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

      nsIPrincipal* principal = GetPrincipal();
      if (!principal) {
        aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }

      nsIPrincipal* storagePrincipal = GetEffectiveStoragePrincipal();
      if (!storagePrincipal) {
        aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }

      aError = storageManager->CreateStorage(this, principal, storagePrincipal,
                                             documentURI, IsPrivateBrowsing(),
                                             getter_AddRefs(storage));
    }

    if (aError.Failed()) {
      return nullptr;
    }

    mLocalStorage = storage;
    MOZ_ASSERT(mLocalStorage);
  }

  if (((partitioningEnabled && shouldPartition) || isolated) &&
      !mLocalStorage) {
    nsresult rv;
    nsCOMPtr<nsIDOMSessionStorageManager> storageManager =
        do_GetService("@mozilla.org/dom/sessionStorage-manager;1", &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    nsIPrincipal* storagePrincipal = GetEffectiveStoragePrincipal();
    if (!storagePrincipal) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    RefPtr<SessionStorageCache> cache;
    if (isolated) {
      cache = new SessionStorageCache();
    } else {
      // This will clone the session storage if it exists.
      rv = storageManager->GetSessionStorageCache(principal, storagePrincipal,
                                                  &cache);
      if (NS_FAILED(rv)) {
        aError.Throw(rv);
        return nullptr;
      }
    }

    mLocalStorage =
        new PartitionedLocalStorage(this, principal, storagePrincipal, cache);
  }

  MOZ_ASSERT_IF(!partitioningEnabled,
                shouldPartition == (mLocalStorage->Type() ==
                                    Storage::ePartitionedLocalStorage));

  return mLocalStorage;
}

IDBFactory* nsGlobalWindowInner::GetIndexedDB(ErrorResult& aError) {
  if (!mIndexedDB) {
    // This may keep mIndexedDB null without setting an error.
    auto res = IDBFactory::CreateForWindow(this);
    if (res.isErr()) {
      aError = res.unwrapErr();
    } else {
      mIndexedDB = res.unwrap();
    }
  }

  return mIndexedDB;
}

//*****************************************************************************
// nsGlobalWindowInner::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindowInner::GetInterface(const nsIID& aIID, void** aSink) {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = outer->GetInterfaceInternal(aIID, aSink);
  if (rv == NS_ERROR_NO_INTERFACE) {
    return QueryInterface(aIID, aSink);
  }
  return rv;
}

void nsGlobalWindowInner::GetInterface(JSContext* aCx,
                                       JS::Handle<JS::Value> aIID,
                                       JS::MutableHandle<JS::Value> aRetval,
                                       ErrorResult& aError) {
  dom::GetInterface(aCx, this, aIID, aRetval, aError);
}

already_AddRefed<CacheStorage> nsGlobalWindowInner::GetCaches(
    ErrorResult& aRv) {
  if (!mCacheStorage) {
    bool forceTrustedOrigin =
        GetOuterWindow()->GetServiceWorkersTestingEnabled();
    mCacheStorage = CacheStorage::CreateOnMainThread(
        cache::DEFAULT_NAMESPACE, this, GetEffectiveStoragePrincipal(),
        forceTrustedOrigin, aRv);
  }

  RefPtr<CacheStorage> ref = mCacheStorage;
  return ref.forget();
}

void nsGlobalWindowInner::FireOfflineStatusEventIfChanged() {
  if (!IsCurrentInnerWindow()) return;

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
  nsContentUtils::DispatchTrustedEvent(mDoc, static_cast<EventTarget*>(this),
                                       name, CanBubble::eNo, Cancelable::eNo);
}

nsGlobalWindowInner::SlowScriptResponse
nsGlobalWindowInner::ShowSlowScriptDialog(JSContext* aCx,
                                          const nsString& aAddonId) {
  nsresult rv;

  if (Preferences::GetBool("dom.always_stop_slow_scripts")) {
    return KillSlowScript;
  }

  // If it isn't safe to run script, then it isn't safe to bring up the prompt
  // (since that spins the event loop). In that (rare) case, we just kill the
  // script and report a warning.
  if (!nsContentUtils::IsSafeToRunScript()) {
    JS::WarnASCII(aCx, "A long running script was terminated");
    return KillSlowScript;
  }

  // If our document is not active, just kill the script: we've been unloaded
  if (!HasActiveDocument()) {
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
  bool hasFrame = JS::DescribeScriptedCaller(aCx, &filename, linenop);

  // Record the slow script event if we haven't done so already for this inner
  // window (which represents a particular page to the user).
  if (!mHasHadSlowScript) {
    Telemetry::Accumulate(Telemetry::SLOW_SCRIPT_PAGE_COUNT, 1);
  }
  mHasHadSlowScript = true;

  if (XRE_IsContentProcess() && ProcessHangMonitor::Get()) {
    ProcessHangMonitor::SlowScriptAction action;
    RefPtr<ProcessHangMonitor> monitor = ProcessHangMonitor::Get();
    nsIDocShell* docShell = GetDocShell();
    nsCOMPtr<nsIBrowserChild> child =
        docShell ? docShell->GetBrowserChild() : nullptr;
    action = monitor->NotifySlowScript(child, filename.get(), aAddonId);
    if (action == ProcessHangMonitor::Terminate) {
      return KillSlowScript;
    }
    if (action == ProcessHangMonitor::TerminateGlobal) {
      return KillScriptGlobal;
    }

    if (action == ProcessHangMonitor::StartDebugger) {
      // Spin a nested event loop so that the debugger in the parent can fetch
      // any information it needs. Once the debugger has started, return to the
      // script.
      RefPtr<nsGlobalWindowOuter> outer = GetOuterWindowInternal();
      outer->EnterModalState();
      SpinEventLoopUntil(
          [&]() { return monitor->IsDebuggerStartupComplete(); });
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
    const char* debugCID = "@mozilla.org/dom/slow-script-debug;1";
    nsCOMPtr<nsISlowScriptDebug> debugService = do_GetService(debugCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      debugService->GetActivationHandler(getter_AddRefs(debugCallback));
    }
  }

  bool failed = false;
  auto getString = [&](const char* name,
                       nsContentUtils::PropertiesFile propFile =
                           nsContentUtils::eDOM_PROPERTIES) {
    nsAutoString result;
    nsresult rv = nsContentUtils::GetLocalizedString(propFile, name, result);

    // GetStringFromName can return NS_OK and still give nullptr string
    failed = failed || NS_FAILED(rv) || result.IsEmpty();
    return result;
  };

  bool isAddonScript = !aAddonId.IsEmpty();
  bool showDebugButton = debugCallback && !isAddonScript;

  // Get localizable strings

  nsAutoString title, checkboxMsg, debugButton, msg;
  if (isAddonScript) {
    title = getString("KillAddonScriptTitle");
    checkboxMsg = getString("KillAddonScriptGlobalMessage");

    auto appName =
        getString("brandShortName", nsContentUtils::eBRAND_PROPERTIES);

    nsCOMPtr<nsIAddonPolicyService> aps =
        do_GetService("@mozilla.org/addons/policy-service;1");
    nsString addonName;
    if (!aps || NS_FAILED(aps->GetExtensionName(aAddonId, addonName))) {
      addonName = aAddonId;
    }

    rv = nsContentUtils::FormatLocalizedString(
        msg, nsContentUtils::eDOM_PROPERTIES, "KillAddonScriptMessage",
        addonName, appName);

    failed = failed || NS_FAILED(rv);
  } else {
    title = getString("KillScriptTitle");
    checkboxMsg = getString("DontAskAgain");

    if (showDebugButton) {
      debugButton = getString("DebugScriptButton");
      msg = getString("KillScriptWithDebugMessage");
    } else {
      msg = getString("KillScriptMessage");
    }
  }

  auto stopButton = getString("StopScriptButton");
  auto waitButton = getString("WaitForScriptButton");

  if (failed) {
    NS_ERROR("Failed to get localized strings.");
    return ContinueSlowScript;
  }

  // Append file and line number information, if available
  if (filename.get()) {
    nsAutoString scriptLocation;
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
      filenameUTF16.ReplaceLiteral(cutStart, cutLength, u"\x2026");
    }
    rv = nsContentUtils::FormatLocalizedString(
        scriptLocation, nsContentUtils::eDOM_PROPERTIES, "KillScriptLocation",
        filenameUTF16);

    if (NS_SUCCEEDED(rv)) {
      msg.AppendLiteral("\n\n");
      msg.Append(scriptLocation);
      msg.Append(':');
      msg.AppendInt(lineno);
    }
  }

  uint32_t buttonFlags = nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          (nsIPrompt::BUTTON_POS_0 + nsIPrompt::BUTTON_POS_1));

  // Add a third button if necessary.
  if (showDebugButton)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  bool checkboxValue = false;
  int32_t buttonPressed = 0;  // In case the user exits dialog by clicking X.
  {
    // Null out the operation callback while we're re-entering JS here.
    AutoDisableJSInterruptCallback disabler(aCx);

    // Open the dialog.
    rv = prompt->ConfirmEx(
        title.get(), msg.get(), buttonFlags, waitButton.get(), stopButton.get(),
        debugButton.get(), checkboxMsg.get(), &checkboxValue, &buttonPressed);
  }

  if (buttonPressed == 0) {
    if (checkboxValue && !isAddonScript && NS_SUCCEEDED(rv))
      return AlwaysContinueSlowScript;
    return ContinueSlowScript;
  }

  if (buttonPressed == 2) {
    MOZ_RELEASE_ASSERT(debugCallback);

    rv = debugCallback->HandleSlowScriptDebug(this);
    return NS_SUCCEEDED(rv) ? ContinueSlowScript : KillSlowScript;
  }

  JS_ClearPendingException(aCx);

  if (checkboxValue && isAddonScript) return KillScriptGlobal;
  return KillSlowScript;
}

nsresult nsGlobalWindowInner::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    if (!IsFrozen()) {
      // Fires an offline status event if the offline status has changed
      FireOfflineStatusEventIfChanged();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, MEMORY_PRESSURE_OBSERVER_TOPIC)) {
    if (mPerformance) {
      mPerformance->MemoryPressure();
      mReportRecords.Clear();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "offline-cache-update-added")) {
    if (mApplicationCache) return NS_OK;

    // Instantiate the application object now. It observes update belonging to
    // this window's document and correctly updates the applicationCache object
    // state.
    nsCOMPtr<nsIObserver> observer = GetApplicationCache();
    if (observer) observer->Observe(aSubject, aTopic, aData);

    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    MOZ_ASSERT(!NS_strcmp(aData, u"intl.accept_languages"));

    // The user preferred languages have changed, we need to fire an event on
    // Window object and invalidate the cache for navigator.languages. It is
    // done for every change which can be a waste of cycles but those should be
    // fairly rare.
    // We MUST invalidate navigator.languages before sending the event in the
    // very likely situation where an event handler will try to read its value.

    if (mNavigator) {
      Navigator_Binding::ClearCachedLanguageValue(mNavigator);
      Navigator_Binding::ClearCachedLanguagesValue(mNavigator);
    }

    // The event has to be dispatched only to the current inner window.
    if (!IsCurrentInnerWindow()) {
      return NS_OK;
    }

    RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("languagechange"), false, false);
    event->SetTrusted(true);

    ErrorResult rv;
    DispatchEvent(*event, rv);
    return rv.StealNSResult();
  }

  NS_WARNING("unrecognized topic in nsGlobalWindowInner::Observe");
  return NS_ERROR_FAILURE;
}

void nsGlobalWindowInner::ObserveStorageNotification(
    StorageEvent* aEvent, const char16_t* aStorageType, bool aPrivateBrowsing) {
  MOZ_ASSERT(aEvent);

  // The private browsing check must be done here again because this window
  // could have changed its state before the notification check and now. This
  // happens in case this window did have a docShell at that time.
  if (aPrivateBrowsing != IsPrivateBrowsing()) {
    return;
  }

  // LocalStorage can only exist on an inner window, and we don't want to
  // generate events on frozen or otherwise-navigated-away from windows.
  // (Actually, this code used to try and buffer events for frozen windows,
  // but it never worked, so we've removed it.  See bug 1285898.)
  if (!IsCurrentInnerWindow() || IsFrozen()) {
    return;
  }

  nsIPrincipal* principal = GetPrincipal();
  if (!principal) {
    return;
  }

  nsIPrincipal* storagePrincipal = GetEffectiveStoragePrincipal();
  if (!storagePrincipal) {
    return;
  }

  bool fireMozStorageChanged = false;
  nsAutoString eventType;
  eventType.AssignLiteral("storage");

  if (!NS_strcmp(aStorageType, u"sessionStorage")) {
    RefPtr<Storage> changingStorage = aEvent->GetStorageArea();
    MOZ_ASSERT(changingStorage);

    bool check = false;

    if (const RefPtr<SessionStorageManager> storageManager =
            GetBrowsingContext()->GetSessionStorageManager()) {
      nsresult rv =
          storageManager->CheckStorage(principal, changingStorage, &check);
      if (NS_FAILED(rv)) {
        return;
      }
    }

    if (!check) {
      // This storage event is not coming from our storage or is coming
      // from a different docshell, i.e. it is a clone, ignore this event.
      return;
    }

    MOZ_LOG(
        gDOMLeakPRLogInner, LogLevel::Debug,
        ("nsGlobalWindowInner %p with sessionStorage %p passing event from %p",
         this, mSessionStorage.get(), changingStorage.get()));

    fireMozStorageChanged = mSessionStorage == changingStorage;
    if (fireMozStorageChanged) {
      eventType.AssignLiteral("MozSessionStorageChanged");
    }
  }

  else {
    MOZ_ASSERT(!NS_strcmp(aStorageType, u"localStorage"));

    MOZ_DIAGNOSTIC_ASSERT(StorageUtils::PrincipalsEqual(aEvent->GetPrincipal(),
                                                        storagePrincipal));

    fireMozStorageChanged =
        mLocalStorage && mLocalStorage == aEvent->GetStorageArea();

    if (fireMozStorageChanged) {
      eventType.AssignLiteral("MozLocalStorageChanged");
    }
  }

  // Clone the storage event included in the observer notification. We want
  // to dispatch clones rather than the original event.
  IgnoredErrorResult error;
  RefPtr<StorageEvent> clonedEvent =
      CloneStorageEvent(eventType, aEvent, error);
  if (error.Failed() || !clonedEvent) {
    return;
  }

  clonedEvent->SetTrusted(true);

  if (fireMozStorageChanged) {
    WidgetEvent* internalEvent = clonedEvent->WidgetEventPtr();
    internalEvent->mFlags.mOnlyChromeDispatch = true;
  }

  DispatchEvent(*clonedEvent);
}

already_AddRefed<StorageEvent> nsGlobalWindowInner::CloneStorageEvent(
    const nsAString& aType, const RefPtr<StorageEvent>& aEvent,
    ErrorResult& aRv) {
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
    if (!NextGenLocalStorageEnabled()) {
      if (aRv.Failed() || !storage) {
        return nullptr;
      }

      if (storage->Type() == Storage::eLocalStorage) {
        RefPtr<LocalStorage> localStorage =
            static_cast<LocalStorage*>(storage.get());

        // We must apply the current change to the 'local' localStorage.
        localStorage->ApplyEvent(aEvent);
      }
    }
  } else if (storageArea->Type() == Storage::eSessionStorage) {
    storage = GetSessionStorage(aRv);
  } else {
    MOZ_ASSERT(storageArea->Type() == Storage::eLocalStorage);
    storage = GetLocalStorage(aRv);
  }

  if (aRv.Failed() || !storage) {
    return nullptr;
  }

  if (storage->Type() == Storage::ePartitionedLocalStorage) {
    // This error message is not exposed.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  MOZ_ASSERT(storage);
  MOZ_ASSERT_IF(storageArea, storage->IsForkOf(storageArea));

  dict.mStorageArea = storage;

  RefPtr<StorageEvent> event = StorageEvent::Constructor(this, aType, dict);
  return event.forget();
}

void nsGlobalWindowInner::Suspend() {
  MOZ_ASSERT(NS_IsMainThread());

  // We can only safely suspend windows that are the current inner window.  If
  // its not the current inner, then we are in one of two different cases.
  // Either we are in the bfcache or we are doomed window that is going away.
  // When a window becomes inactive we purposely avoid placing already suspended
  // windows into the bfcache.  It only expects windows suspended due to the
  // Freeze() method which occurs while the window is still the current inner.
  // So we must not call Suspend() on bfcache windows at this point or this
  // invariant will be broken.  If the window is doomed there is no point in
  // suspending it since it will soon be gone.
  if (!IsCurrentInnerWindow()) {
    return;
  }

  // All children are also suspended.  This ensure mSuspendDepth is
  // set properly and the timers are properly canceled for each child.
  CallOnInProcessChildren(&nsGlobalWindowInner::Suspend);

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

  SuspendWorkersForWindow(this);

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>>::ForwardIterator iter(
      mSharedWorkers);
  while (iter.HasMore()) {
    iter.GetNext()->Suspend();
  }

  SuspendIdleRequests();

  mTimeoutManager->Suspend();

  // Suspend all of the AudioContexts for this window
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->SuspendFromChrome();
  }
}

void nsGlobalWindowInner::Resume() {
  MOZ_ASSERT(NS_IsMainThread());

  // We can only safely resume a window if its the current inner window.  If
  // its not the current inner, then we are in one of two different cases.
  // Either we are in the bfcache or we are doomed window that is going away.
  // If a window is suspended when it becomes inactive we purposely do not
  // put it in the bfcache, so Resume should never be needed in that case.
  // If the window is doomed then there is no point in resuming it.
  if (!IsCurrentInnerWindow()) {
    return;
  }

  // Resume all children.  This restores timers recursively canceled
  // in Suspend() and ensures all children have the correct mSuspendDepth.
  CallOnInProcessChildren(&nsGlobalWindowInner::Resume);

  if (mSuspendDepth == 0) {
    // Ignore if the window is not suspended.
    return;
  }

  mSuspendDepth -= 1;

  if (mSuspendDepth != 0) {
    return;
  }

  // We should not be able to resume a frozen window.  It must be Thaw()'d
  // first.
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
    mAudioContexts[i]->ResumeFromChrome();
  }

  mTimeoutManager->Resume();

  ResumeIdleRequests();

  // Resume all of the workers for this window.  We must do this
  // after timeouts since workers may have queued events that can trigger
  // a setTimeout().
  ResumeWorkersForWindow(this);

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>>::ForwardIterator iter(
      mSharedWorkers);
  while (iter.HasMore()) {
    iter.GetNext()->Resume();
  }
}

bool nsGlobalWindowInner::IsSuspended() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mSuspendDepth != 0;
}

void nsGlobalWindowInner::Freeze() {
  MOZ_ASSERT(NS_IsMainThread());
  Suspend();
  FreezeInternal();
}

void nsGlobalWindowInner::FreezeInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  HintIsLoading(false);

  CallOnInProcessChildren(&nsGlobalWindowInner::FreezeInternal);

  mFreezeDepth += 1;
  MOZ_ASSERT(mSuspendDepth >= mFreezeDepth);
  if (mFreezeDepth != 1) {
    return;
  }

  FreezeWorkersForWindow(this);

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>>::ForwardIterator iter(
      mSharedWorkers);
  while (iter.HasMore()) {
    iter.GetNext()->Freeze();
  }

  mTimeoutManager->Freeze();
  if (mClientSource) {
    mClientSource->Freeze();
  }

  NotifyDOMWindowFrozen(this);
}

void nsGlobalWindowInner::Thaw() {
  MOZ_ASSERT(NS_IsMainThread());
  ThawInternal();
  Resume();
}

void nsGlobalWindowInner::ThawInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  CallOnInProcessChildren(&nsGlobalWindowInner::ThawInternal);

  MOZ_ASSERT(mFreezeDepth != 0);
  mFreezeDepth -= 1;
  MOZ_ASSERT(mSuspendDepth >= mFreezeDepth);
  if (mFreezeDepth != 0) {
    return;
  }

  if (mClientSource) {
    mClientSource->Thaw();
  }
  mTimeoutManager->Thaw();

  ThawWorkersForWindow(this);

  nsTObserverArray<RefPtr<mozilla::dom::SharedWorker>>::ForwardIterator iter(
      mSharedWorkers);
  while (iter.HasMore()) {
    iter.GetNext()->Thaw();
  }

  NotifyDOMWindowThawed(this);
}

bool nsGlobalWindowInner::IsFrozen() const {
  MOZ_ASSERT(NS_IsMainThread());
  bool frozen = mFreezeDepth != 0;
  MOZ_ASSERT_IF(frozen, IsSuspended());
  return frozen;
}

void nsGlobalWindowInner::SyncStateFromParentWindow() {
  // This method should only be called on an inner window that has been
  // assigned to an outer window already.
  MOZ_ASSERT(IsCurrentInnerWindow());
  nsPIDOMWindowOuter* outer = GetOuterWindow();
  MOZ_ASSERT(outer);

  // Attempt to find our parent windows.
  nsCOMPtr<Element> frame = outer->GetFrameElementInternal();
  nsPIDOMWindowOuter* parentOuter =
      frame ? frame->OwnerDoc()->GetWindow() : nullptr;
  nsGlobalWindowInner* parentInner =
      parentOuter
          ? nsGlobalWindowInner::Cast(parentOuter->GetCurrentInnerWindow())
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

template <typename Method, typename... Args>
CallState nsGlobalWindowInner::CallOnInProcessChildren(Method aMethod,
                                                       Args&... aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsCurrentInnerWindow());

  CallState state = CallState::Continue;

  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return state;
  }

  BrowsingContext::Children children;
  GetBrowsingContext()->GetChildren(children);

  for (const RefPtr<BrowsingContext>& bc : children) {
    nsCOMPtr<nsPIDOMWindowOuter> pWin = bc->GetDOMWindow();
    if (!pWin) {
      continue;
    }

    auto* win = nsGlobalWindowOuter::Cast(pWin);
    nsGlobalWindowInner* inner = win->GetCurrentInnerWindowInternal();

    // This is a bit hackish. Only freeze/suspend windows which are truly our
    // subwindows.
    nsCOMPtr<Element> frame = pWin->GetFrameElementInternal();
    if (!mDoc || !frame || mDoc != frame->OwnerDoc() || !inner) {
      continue;
    }

    // Call the child method using our helper CallChild() template method.
    // This allows us to handle both void returning methods and methods
    // that return CallState explicitly.  For void returning methods we
    // assume CallState::Continue.
    typedef decltype((inner->*aMethod)(aArgs...)) returnType;
    state = CallChild<returnType>(inner, aMethod, aArgs...);

    if (state == CallState::Stop) {
      return state;
    }
  }

  return state;
}

Maybe<ClientInfo> nsGlobalWindowInner::GetClientInfo() const {
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ClientInfo> clientInfo;
  if (mClientSource) {
    clientInfo.emplace(mClientSource->Info());
  }
  return clientInfo;
}

Maybe<ClientState> nsGlobalWindowInner::GetClientState() const {
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ClientState> clientState;
  if (mClientSource) {
    Result<ClientState, ErrorResult> res = mClientSource->SnapshotState();
    if (res.isOk()) {
      clientState.emplace(res.unwrap());
    } else {
      res.unwrapErr().SuppressException();
    }
  }
  return clientState;
}

Maybe<ServiceWorkerDescriptor> nsGlobalWindowInner::GetController() const {
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ServiceWorkerDescriptor> controller;
  if (mClientSource) {
    controller = mClientSource->GetController();
  }
  return controller;
}

void nsGlobalWindowInner::SetCsp(nsIContentSecurityPolicy* aCsp) {
  if (!mClientSource) {
    return;
  }
  mClientSource->SetCsp(aCsp);
  // Also cache the CSP within the document
  mDoc->SetCsp(aCsp);
}

void nsGlobalWindowInner::SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCsp) {
  if (!mClientSource) {
    return;
  }
  mClientSource->SetPreloadCsp(aPreloadCsp);
  // Also cache the preload CSP within the document
  mDoc->SetPreloadCsp(aPreloadCsp);
}

nsIContentSecurityPolicy* nsGlobalWindowInner::GetCsp() {
  if (mDoc) {
    return mDoc->GetCsp();
  }

  // If the window is partially torn down and has its document nulled out,
  // we query the CSP we snapshot in FreeInnerObjects.
  if (mDocumentCsp) {
    return mDocumentCsp;
  }
  return nullptr;
}

RefPtr<ServiceWorker> nsGlobalWindowInner::GetOrCreateServiceWorker(
    const ServiceWorkerDescriptor& aDescriptor) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorker> ref;
  ForEachEventTargetObject([&](DOMEventTargetHelper* aTarget, bool* aDoneOut) {
    RefPtr<ServiceWorker> sw = do_QueryObject(aTarget);
    if (!sw || !sw->Descriptor().Matches(aDescriptor)) {
      return;
    }

    ref = std::move(sw);
    *aDoneOut = true;
  });

  if (!ref) {
    ref = ServiceWorker::Create(this, aDescriptor);
  }

  return ref;
}

RefPtr<mozilla::dom::ServiceWorkerRegistration>
nsGlobalWindowInner::GetServiceWorkerRegistration(
    const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor)
    const {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerRegistration> ref;
  ForEachEventTargetObject([&](DOMEventTargetHelper* aTarget, bool* aDoneOut) {
    RefPtr<ServiceWorkerRegistration> swr = do_QueryObject(aTarget);
    if (!swr || !swr->MatchesDescriptor(aDescriptor)) {
      return;
    }

    ref = std::move(swr);
    *aDoneOut = true;
  });
  return ref;
}

RefPtr<ServiceWorkerRegistration>
nsGlobalWindowInner::GetOrCreateServiceWorkerRegistration(
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerRegistration> ref =
      GetServiceWorkerRegistration(aDescriptor);
  if (!ref) {
    ref = ServiceWorkerRegistration::CreateForMainThread(this, aDescriptor);
  }
  return ref;
}

nsresult nsGlobalWindowInner::FireDelayedDOMEvents() {
  if (mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(mApplicationCache.get())
        ->FirePendingEvents();
  }

  // Fires an offline status event if the offline status has changed
  FireOfflineStatusEventIfChanged();

  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    int32_t childCount = 0;
    docShell->GetInProcessChildCount(&childCount);

    // Take a copy of the current children so that modifications to
    // the child list don't affect to the iteration.
    AutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> children;
    for (int32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      docShell->GetInProcessChildAt(i, getter_AddRefs(childShell));
      if (childShell) {
        children.AppendElement(childShell);
      }
    }

    for (nsCOMPtr<nsIDocShellTreeItem> childShell : children) {
      if (nsCOMPtr<nsPIDOMWindowOuter> pWin = childShell->GetWindow()) {
        auto* win = nsGlobalWindowOuter::Cast(pWin);
        win->FireDelayedDOMEvents();
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindowInner: Window Control Functions
//*****************************************************************************

nsPIDOMWindowOuter* nsGlobalWindowInner::GetInProcessParentInternal() {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    // No outer window available!
    return nullptr;
  }
  return outer->GetInProcessParentInternal();
}

nsIPrincipal* nsGlobalWindowInner::GetTopLevelAntiTrackingPrincipal() {
  nsPIDOMWindowOuter* outerWindow = GetOuterWindowInternal();
  if (!outerWindow) {
    return nullptr;
  }

  nsPIDOMWindowOuter* topLevelOuterWindow =
      GetBrowsingContext()->Top()->GetDOMWindow();
  if (!topLevelOuterWindow) {
    return nullptr;
  }

  bool stopAtOurLevel =
      mDoc && mDoc->CookieJarSettings()->GetCookieBehavior() ==
                  nsICookieService::BEHAVIOR_REJECT_TRACKER;

  if (stopAtOurLevel && topLevelOuterWindow == outerWindow) {
    return nullptr;
  }

  nsPIDOMWindowInner* topLevelInnerWindow =
      topLevelOuterWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!topLevelInnerWindow)) {
    return nullptr;
  }

  nsIPrincipal* topLevelPrincipal =
      nsGlobalWindowInner::Cast(topLevelInnerWindow)->GetPrincipal();
  if (NS_WARN_IF(!topLevelPrincipal)) {
    return nullptr;
  }

  return topLevelPrincipal;
}

nsIPrincipal* nsGlobalWindowInner::GetTopLevelStorageAreaPrincipal() {
  if (mDoc && (mDoc->StorageAccessSandboxed())) {
    // Storage access is disabled
    return nullptr;
  }

  BrowsingContext* parent = GetBrowsingContext()->GetParent();
  nsPIDOMWindowOuter* outerWindow = parent ? parent->GetDOMWindow() : nullptr;
  if (!outerWindow) {
    // No outer window available!
    return nullptr;
  }

  if (!outerWindow->IsTopLevelWindow()) {
    return nullptr;
  }

  nsPIDOMWindowInner* innerWindow = outerWindow->GetCurrentInnerWindow();
  if (NS_WARN_IF(!innerWindow)) {
    return nullptr;
  }

  nsIPrincipal* parentPrincipal =
      nsGlobalWindowInner::Cast(innerWindow)->GetPrincipal();
  if (NS_WARN_IF(!parentPrincipal)) {
    return nullptr;
  }

  return parentPrincipal;
}

//*****************************************************************************
// nsGlobalWindowInner: Timeout Functions
//*****************************************************************************

class WindowScriptTimeoutHandler final : public ScriptTimeoutHandler {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WindowScriptTimeoutHandler,
                                           ScriptTimeoutHandler)

  WindowScriptTimeoutHandler(JSContext* aCx, nsIGlobalObject* aGlobal,
                             const nsAString& aExpression)
      : ScriptTimeoutHandler(aCx, aGlobal, aExpression),
        mInitiatingScript(ScriptLoader::GetActiveScript(aCx)) {}

  MOZ_CAN_RUN_SCRIPT virtual bool Call(const char* aExecutionReason) override;

 private:
  virtual ~WindowScriptTimeoutHandler() = default;

  // Initiating script for use when evaluating mExpr on the main thread.
  RefPtr<LoadedScript> mInitiatingScript;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(WindowScriptTimeoutHandler,
                                   ScriptTimeoutHandler, mInitiatingScript)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowScriptTimeoutHandler)
NS_INTERFACE_MAP_END_INHERITING(ScriptTimeoutHandler)

NS_IMPL_ADDREF_INHERITED(WindowScriptTimeoutHandler, ScriptTimeoutHandler)
NS_IMPL_RELEASE_INHERITED(WindowScriptTimeoutHandler, ScriptTimeoutHandler)

bool WindowScriptTimeoutHandler::Call(const char* aExecutionReason) {
  // New script entry point required, due to the "Create a script" sub-step
  // of
  // http://www.whatwg.org/specs/web-apps/current-work/#timer-initialisation-steps
  nsAutoMicroTask mt;
  AutoEntryScript aes(mGlobal, aExecutionReason, true);
  JS::CompileOptions options(aes.cx());
  options.setFileAndLine(mFileName.get(), mLineNo);
  options.setNoScriptRval(true);
  JS::Rooted<JSObject*> global(aes.cx(), mGlobal->GetGlobalJSObject());
  {
    nsJSUtils::ExecutionContext exec(aes.cx(), global);
    nsresult rv = exec.Compile(options, mExpr);

    JS::Rooted<JSScript*> script(aes.cx(), exec.MaybeGetScript());
    if (script) {
      if (mInitiatingScript) {
        mInitiatingScript->AssociateWithScript(script);
      }

      rv = exec.ExecScript();
    }

    if (rv == NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE) {
      return false;
    }
  }

  return true;
};

nsGlobalWindowInner* nsGlobalWindowInner::InnerForSetTimeoutOrInterval(
    ErrorResult& aError) {
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  nsGlobalWindowInner* currentInner =
      outer ? outer->GetCurrentInnerWindowInternal() : this;

  // If forwardTo is not the window with an active document then we want the
  // call to setTimeout/Interval to be a noop, so return null but don't set an
  // error.
  return HasActiveDocument() ? currentInner : nullptr;
}

int32_t nsGlobalWindowInner::SetTimeout(JSContext* aCx, Function& aFunction,
                                        int32_t aTimeout,
                                        const Sequence<JS::Value>& aArguments,
                                        ErrorResult& aError) {
  return SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments, false,
                              aError);
}

int32_t nsGlobalWindowInner::SetTimeout(JSContext* aCx,
                                        const nsAString& aHandler,
                                        int32_t aTimeout,
                                        const Sequence<JS::Value>& /* unused */,
                                        ErrorResult& aError) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, false, aError);
}

int32_t nsGlobalWindowInner::SetInterval(JSContext* aCx, Function& aFunction,
                                         const int32_t aTimeout,
                                         const Sequence<JS::Value>& aArguments,
                                         ErrorResult& aError) {
  return SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments, true,
                              aError);
}

int32_t nsGlobalWindowInner::SetInterval(
    JSContext* aCx, const nsAString& aHandler, const int32_t aTimeout,
    const Sequence<JS::Value>& /* unused */, ErrorResult& aError) {
  return SetTimeoutOrInterval(aCx, aHandler, aTimeout, true, aError);
}

int32_t nsGlobalWindowInner::SetTimeoutOrInterval(
    JSContext* aCx, Function& aFunction, int32_t aTimeout,
    const Sequence<JS::Value>& aArguments, bool aIsInterval,
    ErrorResult& aError) {
  nsGlobalWindowInner* inner = InnerForSetTimeoutOrInterval(aError);
  if (!inner) {
    return -1;
  }

  if (inner != this) {
    RefPtr<nsGlobalWindowInner> innerRef(inner);
    return innerRef->SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments,
                                          aIsInterval, aError);
  }

  DebuggerNotificationDispatch(
      this, aIsInterval ? DebuggerNotificationType::SetInterval
                        : DebuggerNotificationType::SetTimeout);

  if (!GetContextInternal() || !HasJSGlobal()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  nsTArray<JS::Heap<JS::Value>> args;
  if (!args.AppendElements(aArguments, fallible)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return 0;
  }

  RefPtr<TimeoutHandler> handler =
      new CallbackTimeoutHandler(aCx, this, &aFunction, std::move(args));

  int32_t result;
  aError =
      mTimeoutManager->SetTimeout(handler, aTimeout, aIsInterval,
                                  Timeout::Reason::eTimeoutOrInterval, &result);
  return result;
}

int32_t nsGlobalWindowInner::SetTimeoutOrInterval(JSContext* aCx,
                                                  const nsAString& aHandler,
                                                  int32_t aTimeout,
                                                  bool aIsInterval,
                                                  ErrorResult& aError) {
  nsGlobalWindowInner* inner = InnerForSetTimeoutOrInterval(aError);
  if (!inner) {
    return -1;
  }

  if (inner != this) {
    RefPtr<nsGlobalWindowInner> innerRef(inner);
    return innerRef->SetTimeoutOrInterval(aCx, aHandler, aTimeout, aIsInterval,
                                          aError);
  }

  DebuggerNotificationDispatch(
      this, aIsInterval ? DebuggerNotificationType::SetInterval
                        : DebuggerNotificationType::SetTimeout);

  if (!GetContextInternal() || !HasJSGlobal()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return 0;
  }

  bool allowEval = false;
  aError = CSPEvalChecker::CheckForWindow(aCx, this, aHandler, &allowEval);
  if (NS_WARN_IF(aError.Failed()) || !allowEval) {
    return 0;
  }

  RefPtr<TimeoutHandler> handler =
      new WindowScriptTimeoutHandler(aCx, this, aHandler);

  int32_t result;
  aError =
      mTimeoutManager->SetTimeout(handler, aTimeout, aIsInterval,
                                  Timeout::Reason::eTimeoutOrInterval, &result);
  return result;
}

static const char* GetTimeoutReasonString(Timeout* aTimeout) {
  switch (aTimeout->mReason) {
    case Timeout::Reason::eTimeoutOrInterval:
      if (aTimeout->mIsInterval) {
        return "setInterval handler";
      }
      return "setTimeout handler";
    case Timeout::Reason::eIdleCallbackTimeout:
      return "setIdleCallback handler (timed out)";
    default:
      MOZ_CRASH("Unexpected enum value");
      return "";
  }
}

bool nsGlobalWindowInner::RunTimeoutHandler(Timeout* aTimeout,
                                            nsIScriptContext* aScx) {
  // Hold on to the timeout in case mExpr or mFunObj releases its
  // doc.
  // XXXbz Our caller guarantees it'll hold on to the timeout (because
  // we're MOZ_CAN_RUN_SCRIPT), so we can probably stop doing that...
  RefPtr<Timeout> timeout = aTimeout;
  Timeout* last_running_timeout = mTimeoutManager->BeginRunningTimeout(timeout);
  timeout->mRunning = true;

  // Push this timeout's popup control state, which should only be
  // enabled the first time a timeout fires that was created while
  // popups were enabled and with a delay less than
  // "dom.disable_open_click_delay".
  AutoPopupStatePusher popupStatePusher(timeout->mPopupState);

  // Clear the timeout's popup state, if any, to prevent interval
  // timeouts from repeatedly opening poups.
  timeout->mPopupState = PopupBlocker::openAbused;

  bool trackNestingLevel = !timeout->mIsInterval;
  uint32_t nestingLevel;
  if (trackNestingLevel) {
    nestingLevel = TimeoutManager::GetNestingLevel();
    TimeoutManager::SetNestingLevel(timeout->mNestingLevel);
  }

  const char* reason = GetTimeoutReasonString(timeout);

#ifdef MOZ_GECKO_PROFILER
  nsCString str;
  if (profiler_can_accept_markers()) {
    TimeDuration originalInterval = timeout->When() - timeout->SubmitTime();
    str.Append(reason);
    str.Append(" with interval ");
    str.AppendInt(int(originalInterval.ToMilliseconds()));
    str.Append("ms: ");
    nsCString handlerDescription;
    timeout->mScriptHandler->GetDescription(handlerDescription);
    str.Append(handlerDescription);
  }
  AUTO_PROFILER_TEXT_MARKER_CAUSE("setTimeout callback", str, JS,
                                  Some(mWindowID),
                                  timeout->TakeProfilerBacktrace());
#endif

  bool abortIntervalHandler;
  {
    RefPtr<TimeoutHandler> handler(timeout->mScriptHandler);

    CallbackDebuggerNotificationGuard guard(
        this, timeout->mIsInterval
                  ? DebuggerNotificationType::SetIntervalCallback
                  : DebuggerNotificationType::SetTimeoutCallback);
    abortIntervalHandler = !handler->Call(reason);
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

  if (trackNestingLevel) {
    TimeoutManager::SetNestingLevel(nestingLevel);
  }

  mTimeoutManager->EndRunningTimeout(last_running_timeout);
  timeout->mRunning = false;

  return timeout->mCleared;
}

//*****************************************************************************
// nsGlobalWindowInner: Helper Functions
//*****************************************************************************

already_AddRefed<nsIDocShellTreeOwner> nsGlobalWindowInner::GetTreeOwner() {
  FORWARD_TO_OUTER(GetTreeOwner, (), nullptr);
}

already_AddRefed<nsIWebBrowserChrome>
nsGlobalWindowInner::GetWebBrowserChrome() {
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_GetInterface(treeOwner);
  return browserChrome.forget();
}

nsIScrollableFrame* nsGlobalWindowInner::GetScrollFrame() {
  FORWARD_TO_OUTER(GetScrollFrame, (), nullptr);
}

bool nsGlobalWindowInner::IsPrivateBrowsing() {
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
  return loadContext && loadContext->UsePrivateBrowsing();
}

void nsGlobalWindowInner::FlushPendingNotifications(FlushType aType) {
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void nsGlobalWindowInner::EnableDeviceSensor(uint32_t aType) {
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

void nsGlobalWindowInner::DisableDeviceSensor(uint32_t aType) {
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

#if defined(MOZ_WIDGET_ANDROID)
void nsGlobalWindowInner::EnableOrientationChangeListener() {
  if (!nsContentUtils::ShouldResistFingerprinting(GetDocShell()) &&
      !mOrientationChangeObserver) {
    mOrientationChangeObserver = MakeUnique<WindowOrientationObserver>(this);
  }
}

void nsGlobalWindowInner::DisableOrientationChangeListener() {
  mOrientationChangeObserver = nullptr;
}
#endif

void nsGlobalWindowInner::SetHasGamepadEventListener(
    bool aHasGamepad /* = true*/) {
  mHasGamepad = aHasGamepad;
  if (aHasGamepad) {
    EnableGamepadUpdates();
  }
}

void nsGlobalWindowInner::NotifyDetectXRRuntimesCompleted() {
  if (!mXRRuntimeDetectionInFlight) {
    return;
  }
  mXRRuntimeDetectionInFlight = false;
  if (mXRPermissionRequestInFlight) {
    return;
  }
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  bool supported = vm->RuntimeSupportsVR();
  if (!supported) {
    // A VR runtime was not installed; we can suppress
    // the permission prompt
    OnXRPermissionRequestCancel();
    return;
  }
  // A VR runtime was found.  Display a permission prompt before
  // allowing it to be accessed.
  // Connect to the VRManager in order to receive the runtime
  // detection results.
  mXRPermissionRequestInFlight = true;
  RefPtr<XRPermissionRequest> request =
      new XRPermissionRequest(this, WindowID());
  Unused << NS_WARN_IF(NS_FAILED(request->Start()));
}

void nsGlobalWindowInner::RequestXRPermission() {
  if (IsDying()) {
    // Do not proceed if the window is dying, as that will result
    // in leaks of objects that get re-allocated after FreeInnerObjects
    // has been called, including mVREventObserver.
    return;
  }
  if (mXRPermissionGranted) {
    // Don't prompt redundantly once permission to
    // access XR devices has been granted.
    OnXRPermissionRequestAllow();
    return;
  }
  if (mXRRuntimeDetectionInFlight || mXRPermissionRequestInFlight) {
    // Don't allow multiple simultaneous permissions requests;
    return;
  }
  // Before displaying a permission prompt, detect
  // if there is any VR runtime installed.
  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  mXRRuntimeDetectionInFlight = true;
  EnableVRUpdates();
  vm->DetectRuntimes();
}

void nsGlobalWindowInner::OnXRPermissionRequestAllow() {
  mXRPermissionRequestInFlight = false;
  if (IsDying()) {
    // The window may have started dying while the permission request
    // is in flight.
    // Do not proceed if the window is dying, as that will result
    // in leaks of objects that get re-allocated after FreeInnerObjects
    // has been called, including mNavigator.
    return;
  }
  mXRPermissionGranted = true;

  NotifyHasXRSession();

  dom::Navigator* nav = Navigator();
  MOZ_ASSERT(nav != nullptr);
  nav->OnXRPermissionRequestAllow();
}

void nsGlobalWindowInner::OnXRPermissionRequestCancel() {
  mXRPermissionRequestInFlight = false;
  if (IsDying()) {
    // The window may have started dying while the permission request
    // is in flight.
    // Do not proceed if the window is dying, as that will result
    // in leaks of objects that get re-allocated after FreeInnerObjects
    // has been called, including mNavigator.
    return;
  }
  dom::Navigator* nav = Navigator();
  MOZ_ASSERT(nav != nullptr);
  nav->OnXRPermissionRequestCancel();
}

void nsGlobalWindowInner::EventListenerAdded(nsAtom* aType) {
  if (aType == nsGkAtoms::onvrdisplayactivate ||
      aType == nsGkAtoms::onvrdisplayconnect ||
      aType == nsGkAtoms::onvrdisplaydeactivate ||
      aType == nsGkAtoms::onvrdisplaydisconnect ||
      aType == nsGkAtoms::onvrdisplaypresentchange) {
    RequestXRPermission();
  }

  if (aType == nsGkAtoms::onvrdisplayactivate) {
    mHasVRDisplayActivateEvents = true;
  }

  if (aType == nsGkAtoms::onbeforeunload && mWindowGlobalChild &&
      (!mDoc || !(mDoc->GetSandboxFlags() & SANDBOXED_MODALS))) {
    mWindowGlobalChild->BeforeUnloadAdded();
    MOZ_ASSERT(mWindowGlobalChild->BeforeUnloadListeners() > 0);
  }

  // We need to initialize localStorage in order to receive notifications.
  if (aType == nsGkAtoms::onstorage) {
    ErrorResult rv;
    GetLocalStorage(rv);
    rv.SuppressException();

    if (NextGenLocalStorageEnabled() && mLocalStorage &&
        mLocalStorage->Type() == Storage::eLocalStorage) {
      auto object = static_cast<LSObject*>(mLocalStorage.get());

      Unused << NS_WARN_IF(NS_FAILED(object->EnsureObserver()));
    }
  }
}

void nsGlobalWindowInner::EventListenerRemoved(nsAtom* aType) {
  if (aType == nsGkAtoms::onbeforeunload && mWindowGlobalChild &&
      (!mDoc || !(mDoc->GetSandboxFlags() & SANDBOXED_MODALS))) {
    mWindowGlobalChild->BeforeUnloadRemoved();
    MOZ_ASSERT(mWindowGlobalChild->BeforeUnloadListeners() >= 0);
  }

  if (aType == nsGkAtoms::onstorage) {
    if (NextGenLocalStorageEnabled() && mLocalStorage &&
        mLocalStorage->Type() == Storage::eLocalStorage &&
        // The remove event is fired even if this isn't the last listener, so
        // only remove if there are no other listeners left.
        mListenerManager &&
        !mListenerManager->HasListenersFor(nsGkAtoms::onstorage)) {
      auto object = static_cast<LSObject*>(mLocalStorage.get());

      object->DropObserver();
    }
  }
}

void nsGlobalWindowInner::NotifyHasXRSession() {
  if (IsDying()) {
    // Do not proceed if the window is dying, as that will result
    // in leaks of objects that get re-allocated after FreeInnerObjects
    // has been called, including mVREventObserver.
    return;
  }
  mHasXRSession = true;
  EnableVRUpdates();
}

bool nsGlobalWindowInner::HasUsedVR() const {
  // Returns true only if content has enumerated and activated
  // XR devices.  Detection of XR runtimes without activation
  // will not cause true to be returned.
  return mHasXRSession;
}

bool nsGlobalWindowInner::IsVRContentDetected() const {
  // Returns true only if the content will respond to
  // the VRDisplayActivate event.
  return mHasVRDisplayActivateEvents;
}

bool nsGlobalWindowInner::IsVRContentPresenting() const {
  for (const auto& display : mVRDisplays) {
    if (display->IsAnyPresenting(gfx::kVRGroupAll)) {
      return true;
    }
  }
  return false;
}

void nsGlobalWindowInner::AddSizeOfIncludingThis(
    nsWindowSizes& aWindowSizes) const {
  aWindowSizes.mDOMOtherSize += aWindowSizes.mState.mMallocSizeOf(this);
  aWindowSizes.mDOMOtherSize += nsIGlobalObject::ShallowSizeOfExcludingThis(
      aWindowSizes.mState.mMallocSizeOf);

  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    aWindowSizes.mDOMOtherSize +=
        elm->SizeOfIncludingThis(aWindowSizes.mState.mMallocSizeOf);
    aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
  }
  if (mDoc) {
    // Multiple global windows can share a document. So only measure the
    // document if it (a) doesn't have a global window, or (b) it's the
    // primary document for the window.
    if (!mDoc->GetInnerWindow() || mDoc->GetInnerWindow() == this) {
      mDoc->DocAddSizeOfIncludingThis(aWindowSizes);
    }
  }

  if (mNavigator) {
    aWindowSizes.mDOMOtherSize +=
        mNavigator->SizeOfIncludingThis(aWindowSizes.mState.mMallocSizeOf);
  }

  ForEachEventTargetObject([&](DOMEventTargetHelper* et, bool* aDoneOut) {
    if (nsCOMPtr<nsISizeOfEventTarget> iSizeOf = do_QueryObject(et)) {
      aWindowSizes.mDOMEventTargetsSize +=
          iSizeOf->SizeOfEventTargetIncludingThis(
              aWindowSizes.mState.mMallocSizeOf);
    }
    if (EventListenerManager* elm = et->GetExistingListenerManager()) {
      aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
    }
    ++aWindowSizes.mDOMEventTargetsCount;
  });

  if (mPerformance) {
    aWindowSizes.mDOMPerformanceUserEntries =
        mPerformance->SizeOfUserEntries(aWindowSizes.mState.mMallocSizeOf);
    aWindowSizes.mDOMPerformanceResourceEntries =
        mPerformance->SizeOfResourceEntries(aWindowSizes.mState.mMallocSizeOf);
  }

  aWindowSizes.mDOMOtherSize += mPendingPromises.ShallowSizeOfExcludingThis(
      aWindowSizes.mState.mMallocSizeOf);
}

void nsGlobalWindowInner::AddGamepad(uint32_t aIndex, Gamepad* aGamepad) {
  // Create the index we will present to content based on which indices are
  // already taken, as required by the spec.
  // https://w3c.github.io/gamepad/gamepad.html#widl-Gamepad-index
  int index = 0;
  while (mGamepadIndexSet.Contains(index)) {
    ++index;
  }
  mGamepadIndexSet.Put(index);
  aGamepad->SetIndex(index);
  mGamepads.Put(aIndex, RefPtr{aGamepad});
}

void nsGlobalWindowInner::RemoveGamepad(uint32_t aIndex) {
  RefPtr<Gamepad> gamepad;
  if (!mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return;
  }
  // Free up the index we were using so it can be reused
  mGamepadIndexSet.Remove(gamepad->Index());
  mGamepads.Remove(aIndex);
}

void nsGlobalWindowInner::GetGamepads(nsTArray<RefPtr<Gamepad>>& aGamepads) {
  aGamepads.Clear();

  // navigator.getGamepads() always returns an empty array when
  // privacy.resistFingerprinting is true.
  if (nsContentUtils::ShouldResistFingerprinting(GetDocShell())) {
    return;
  }

  // mGamepads.Count() may not be sufficient, but it's not harmful.
  aGamepads.SetCapacity(mGamepads.Count());
  for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
    Gamepad* gamepad = iter.UserData();
    aGamepads.EnsureLengthAtLeast(gamepad->Index() + 1);
    aGamepads[gamepad->Index()] = gamepad;
  }
}

already_AddRefed<Gamepad> nsGlobalWindowInner::GetGamepad(uint32_t aIndex) {
  RefPtr<Gamepad> gamepad;

  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

void nsGlobalWindowInner::SetHasSeenGamepadInput(bool aHasSeen) {
  mHasSeenGamepadInput = aHasSeen;
}

bool nsGlobalWindowInner::HasSeenGamepadInput() { return mHasSeenGamepadInput; }

void nsGlobalWindowInner::SyncGamepadState() {
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
      gamepadManager->SyncGamepadState(iter.Key(), this, iter.UserData());
    }
  }
}

void nsGlobalWindowInner::StopGamepadHaptics() {
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    gamepadManager->StopHaptics();
  }
}

bool nsGlobalWindowInner::UpdateVRDisplays(
    nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDevices) {
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);
  aDevices = mVRDisplays;
  return true;
}

void nsGlobalWindowInner::NotifyActiveVRDisplaysChanged() {
  if (mNavigator) {
    mNavigator->NotifyActiveVRDisplaysChanged();
  }
}

void nsGlobalWindowInner::NotifyPresentationGenerationChanged(
    uint32_t aDisplayID) {
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      display->OnPresentationGenerationChanged();
    }
  }
}

void nsGlobalWindowInner::DispatchVRDisplayActivate(
    uint32_t aDisplayID, mozilla::dom::VRDisplayEventReason aReason) {
  // Ensure that our list of displays is up to date
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);

  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      if (aReason != VRDisplayEventReason::Navigation &&
          display->IsAnyPresenting(gfx::kVRGroupContent)) {
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

      RefPtr<VRDisplayEvent> event = VRDisplayEvent::Constructor(
          this, NS_LITERAL_STRING("vrdisplayactivate"), init);
      // vrdisplayactivate is a trusted event, allowing VRDisplay.requestPresent
      // to be used in response to link traversal, user request (chrome UX), and
      // HMD mounting detection sensors.
      event->SetTrusted(true);
      // VRDisplay.requestPresent normally requires a user gesture; however, an
      // exception is made to allow it to be called in response to
      // vrdisplayactivate during VR link traversal.
      display->StartHandlingVRNavigationEvent();
      DispatchEvent(*event);
      display->StopHandlingVRNavigationEvent();
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void nsGlobalWindowInner::DispatchVRDisplayDeactivate(
    uint32_t aDisplayID, mozilla::dom::VRDisplayEventReason aReason) {
  // Ensure that our list of displays is up to date
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);

  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID && display->IsPresenting()) {
      // We only want to trigger this event to content that is presenting to
      // the display already.

      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      init.mReason.Construct(aReason);

      RefPtr<VRDisplayEvent> event = VRDisplayEvent::Constructor(
          this, NS_LITERAL_STRING("vrdisplaydeactivate"), init);
      event->SetTrusted(true);
      DispatchEvent(*event);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void nsGlobalWindowInner::DispatchVRDisplayConnect(uint32_t aDisplayID) {
  // Ensure that our list of displays is up to date
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);

  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplayconnect

      RefPtr<VRDisplayEvent> event = VRDisplayEvent::Constructor(
          this, NS_LITERAL_STRING("vrdisplayconnect"), init);
      event->SetTrusted(true);
      DispatchEvent(*event);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void nsGlobalWindowInner::DispatchVRDisplayDisconnect(uint32_t aDisplayID) {
  // Ensure that our list of displays is up to date
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);

  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplaydisconnect

      RefPtr<VRDisplayEvent> event = VRDisplayEvent::Constructor(
          this, NS_LITERAL_STRING("vrdisplaydisconnect"), init);
      event->SetTrusted(true);
      DispatchEvent(*event);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

void nsGlobalWindowInner::DispatchVRDisplayPresentChange(uint32_t aDisplayID) {
  // Ensure that our list of displays is up to date
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);

  // Search for the display identified with aDisplayID and fire the
  // event if found.
  for (const auto& display : mVRDisplays) {
    if (display->DisplayId() == aDisplayID) {
      // Fire event even if not presenting to the display.
      VRDisplayEventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      init.mDisplay = display;
      // VRDisplayEvent.reason is not set for vrdisplaypresentchange
      RefPtr<VRDisplayEvent> event = VRDisplayEvent::Constructor(
          this, NS_LITERAL_STRING("vrdisplaypresentchange"), init);
      event->SetTrusted(true);
      DispatchEvent(*event);
      // Once we dispatch the event, we must not access any members as an event
      // listener can do anything, including closing windows.
      return;
    }
  }
}

enum WindowState {
  // These constants need to match the constants in Window.webidl
  STATE_MAXIMIZED = 1,
  STATE_MINIMIZED = 2,
  STATE_NORMAL = 3,
  STATE_FULLSCREEN = 4
};

uint16_t nsGlobalWindowInner::WindowState() {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  int32_t mode = widget ? widget->SizeMode() : 0;

  switch (mode) {
    case nsSizeMode_Minimized:
      return STATE_MINIMIZED;
    case nsSizeMode_Maximized:
      return STATE_MAXIMIZED;
    case nsSizeMode_Fullscreen:
      return STATE_FULLSCREEN;
    case nsSizeMode_Normal:
      return STATE_NORMAL;
    default:
      NS_WARNING("Illegal window state for this chrome window");
      break;
  }

  return STATE_NORMAL;
}

bool nsGlobalWindowInner::IsFullyOccluded() {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  return widget && widget->IsFullyOccluded();
}

void nsGlobalWindowInner::Maximize() {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Maximized);
  }
}

void nsGlobalWindowInner::Minimize() {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Minimized);
  }
}

void nsGlobalWindowInner::Restore() {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Normal);
  }
}

void nsGlobalWindowInner::GetWorkspaceID(nsAString& workspaceID) {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  workspaceID.Truncate();
  if (widget) {
    return widget->GetWorkspaceID(workspaceID);
  }
}

void nsGlobalWindowInner::MoveToWorkspace(const nsAString& workspaceID) {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->MoveToWorkspace(workspaceID);
  }
}

void nsGlobalWindowInner::GetAttention(ErrorResult& aResult) {
  return GetAttentionWithCycleCount(-1, aResult);
}

void nsGlobalWindowInner::GetAttentionWithCycleCount(int32_t aCycleCount,
                                                     ErrorResult& aError) {
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->GetAttention(aCycleCount);
  }
}

already_AddRefed<Promise> nsGlobalWindowInner::PromiseDocumentFlushed(
    PromiseDocumentFlushedCallback& aCallback, ErrorResult& aError) {
  MOZ_RELEASE_ASSERT(IsChromeWindow());

  if (!IsCurrentInnerWindow()) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (mIteratingDocumentFlushedResolvers) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!mDoc) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  PresShell* presShell = mDoc->GetPresShell();
  if (!presShell) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // We need to associate the lifetime of the Promise to the lifetime
  // of the caller's global. That way, if the window we're observing
  // refresh driver ticks on goes away before our observer is fired,
  // we can still resolve the Promise.
  nsIGlobalObject* global = GetIncumbentGlobal();
  if (!global) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> resultPromise = Promise::Create(global, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  UniquePtr<PromiseDocumentFlushedResolver> flushResolver(
      new PromiseDocumentFlushedResolver(resultPromise, aCallback));

  if (!presShell->NeedStyleFlush() && !presShell->NeedLayoutFlush()) {
    flushResolver->Call();
    return resultPromise.forget();
  }

  if (!mObservingDidRefresh) {
    bool success = presShell->AddPostRefreshObserver(this);
    if (!success) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    mObservingDidRefresh = true;
  }

  mDocumentFlushedResolvers.AppendElement(std::move(flushResolver));
  return resultPromise.forget();
}

template <bool call>
void nsGlobalWindowInner::CallOrCancelDocumentFlushedResolvers() {
  MOZ_ASSERT(!mIteratingDocumentFlushedResolvers);

  while (true) {
    {
      // To coalesce MicroTask checkpoints inside callback call, enclose the
      // inner loop with nsAutoMicroTask, and perform a MicroTask checkpoint
      // after the loop.
      nsAutoMicroTask mt;

      mIteratingDocumentFlushedResolvers = true;
      for (const auto& documentFlushedResolver : mDocumentFlushedResolvers) {
        if (call) {
          documentFlushedResolver->Call();
        } else {
          documentFlushedResolver->Cancel();
        }
      }
      mDocumentFlushedResolvers.Clear();
      mIteratingDocumentFlushedResolvers = false;
    }

    // Leaving nsAutoMicroTask above will perform MicroTask checkpoint, and
    // Promise callbacks there may create mDocumentFlushedResolvers items.

    // If there's no new item, there's nothing to do here.
    if (!mDocumentFlushedResolvers.Length()) {
      break;
    }

    // If there are new items, the observer is not added for them when calling
    // PromiseDocumentFlushed.  Add here and leave.
    // FIXME: Handle this case inside PromiseDocumentFlushed (bug 1442824).
    if (mDoc) {
      PresShell* presShell = mDoc->GetPresShell();
      if (presShell) {
        Unused << presShell->AddPostRefreshObserver(this);
        break;
      }
    }

    // If we fail adding observer, keep looping to resolve or reject all
    // promises.  This case happens while destroying window.
    // This violates the constraint that the promiseDocumentFlushed callback
    // only ever run when no flush needed, but it's necessary to resolve
    // Promise returned by that.
  }
}

void nsGlobalWindowInner::CallDocumentFlushedResolvers() {
  CallOrCancelDocumentFlushedResolvers<true>();
}

void nsGlobalWindowInner::CancelDocumentFlushedResolvers() {
  CallOrCancelDocumentFlushedResolvers<false>();
}

void nsGlobalWindowInner::DidRefresh() {
  auto rejectionGuard = MakeScopeExit([&] {
    CancelDocumentFlushedResolvers();
    mObservingDidRefresh = false;
  });

  MOZ_ASSERT(mDoc);

  PresShell* presShell = mDoc->GetPresShell();
  MOZ_ASSERT(presShell);

  if (presShell->NeedStyleFlush() || presShell->NeedLayoutFlush()) {
    // By the time our observer fired, something has already invalidated
    // style or layout - or perhaps we're still in the middle of a flush that
    // was interrupted. In either case, we'll wait until the next refresh driver
    // tick instead and try again.
    rejectionGuard.release();
    return;
  }

  bool success = presShell->RemovePostRefreshObserver(this);
  if (!success) {
    return;
  }

  rejectionGuard.release();

  CallDocumentFlushedResolvers();
  mObservingDidRefresh = false;
}

already_AddRefed<nsWindowRoot> nsGlobalWindowInner::GetWindowRoot(
    mozilla::ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetWindowRootOuter, (), aError, nullptr);
}

void nsGlobalWindowInner::SetCursor(const nsACString& aCursor,
                                    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetCursorOuter, (aCursor, aError), aError, );
}

NS_IMETHODIMP
nsGlobalWindowInner::GetBrowserDOMWindow(nsIBrowserDOMWindow** aBrowserWindow) {
  MOZ_RELEASE_ASSERT(IsChromeWindow());

  ErrorResult rv;
  NS_IF_ADDREF(*aBrowserWindow = GetBrowserDOMWindow(rv));
  return rv.StealNSResult();
}

nsIBrowserDOMWindow* nsGlobalWindowInner::GetBrowserDOMWindow(
    ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(GetBrowserDOMWindowOuter, (), aError, nullptr);
}

void nsGlobalWindowInner::SetBrowserDOMWindow(
    nsIBrowserDOMWindow* aBrowserWindow, ErrorResult& aError) {
  FORWARD_TO_OUTER_OR_THROW(SetBrowserDOMWindowOuter, (aBrowserWindow),
                            aError, );
}

void nsGlobalWindowInner::NotifyDefaultButtonLoaded(Element& aDefaultButton,
                                                    ErrorResult& aError) {
#ifdef MOZ_XUL
  // Don't snap to a disabled button.
  nsCOMPtr<nsIDOMXULControlElement> xulControl = aDefaultButton.AsXULControl();
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
  nsIFrame* frame = aDefaultButton.GetPrimaryFrame();
  if (!frame) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  LayoutDeviceIntRect buttonRect = LayoutDeviceIntRect::FromAppUnitsToNearest(
      frame->GetScreenRectInAppUnits(),
      frame->PresContext()->AppUnitsPerDevPixel());

  // Get the widget rect in screen coordinates.
  nsIWidget* widget = GetNearestWidget();
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

ChromeMessageBroadcaster* nsGlobalWindowInner::MessageManager() {
  MOZ_ASSERT(IsChromeWindow());
  if (!mChromeFields.mMessageManager) {
    RefPtr<ChromeMessageBroadcaster> globalMM =
        nsFrameMessageManager::GetGlobalMessageManager();
    mChromeFields.mMessageManager = new ChromeMessageBroadcaster(globalMM);
  }
  return mChromeFields.mMessageManager;
}

ChromeMessageBroadcaster* nsGlobalWindowInner::GetGroupMessageManager(
    const nsAString& aGroup) {
  MOZ_ASSERT(IsChromeWindow());

  RefPtr<ChromeMessageBroadcaster> messageManager =
      mChromeFields.mGroupMessageManagers.LookupForAdd(aGroup).OrInsert(
          [this]() { return new ChromeMessageBroadcaster(MessageManager()); });
  return messageManager;
}

void nsGlobalWindowInner::InitWasOffline() { mWasOffline = NS_IsOffline(); }

#if defined(MOZ_WIDGET_ANDROID)
int16_t nsGlobalWindowInner::Orientation(CallerType aCallerType) const {
  return nsContentUtils::ResistFingerprinting(aCallerType)
             ? 0
             : WindowOrientationObserver::OrientationAngle();
}
#endif

already_AddRefed<Console> nsGlobalWindowInner::GetConsole(JSContext* aCx,
                                                          ErrorResult& aRv) {
  if (!mConsole) {
    mConsole = Console::Create(aCx, this, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  RefPtr<Console> console = mConsole;
  return console.forget();
}

bool nsGlobalWindowInner::IsSecureContext() const {
  JS::Realm* realm = js::GetNonCCWObjectRealm(GetWrapperPreserveColor());
  return JS::GetIsSecureContext(realm);
}

External* nsGlobalWindowInner::GetExternal(ErrorResult& aRv) {
#ifdef HAVE_SIDEBAR
  if (!mExternal) {
    mExternal = ConstructJSImplementation<External>("@mozilla.org/sidebar;1",
                                                    this, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return static_cast<External*>(mExternal.get());
#else
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
#endif
}

void nsGlobalWindowInner::GetSidebar(OwningExternalOrWindowProxy& aResult,
                                     ErrorResult& aRv) {
#ifdef HAVE_SIDEBAR
  // First check for a named frame named "sidebar"
  RefPtr<BrowsingContext> domWindow =
      GetChildWindow(NS_LITERAL_STRING("sidebar"));
  if (domWindow) {
    aResult.SetAsWindowProxy() = std::move(domWindow);
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

void nsGlobalWindowInner::ClearDocumentDependentSlots(JSContext* aCx) {
  // If JSAPI OOMs here, there is basically nothing we can do to recover safely.
  if (!Window_Binding::ClearCachedDocumentValue(aCx, this) ||
      !Window_Binding::ClearCachedPerformanceValue(aCx, this)) {
    MOZ_CRASH("Unhandlable OOM while clearing document dependent slots.");
  }
}

/* static */
JSObject* nsGlobalWindowInner::CreateNamedPropertiesObject(
    JSContext* aCx, JS::Handle<JSObject*> aProto) {
  return WindowNamedPropertiesHandler::Create(aCx, aProto);
}

void nsGlobalWindowInner::RedefineProperty(JSContext* aCx,
                                           const char* aPropName,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aError) {
  JS::Rooted<JSObject*> thisObj(aCx, GetWrapperPreserveColor());
  if (!thisObj) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!JS_WrapObject(aCx, &thisObj) ||
      !JS_DefineProperty(aCx, thisObj, aPropName, aValue, JSPROP_ENUMERATE)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

void nsGlobalWindowInner::GetReplaceableWindowCoord(
    JSContext* aCx, nsGlobalWindowInner::WindowCoordGetter aGetter,
    JS::MutableHandle<JS::Value> aRetval, CallerType aCallerType,
    ErrorResult& aError) {
  int32_t coord = (this->*aGetter)(aCallerType, aError);
  if (!aError.Failed() && !ToJSValue(aCx, coord, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

void nsGlobalWindowInner::SetReplaceableWindowCoord(
    JSContext* aCx, nsGlobalWindowInner::WindowCoordSetter aSetter,
    JS::Handle<JS::Value> aValue, const char* aPropName, CallerType aCallerType,
    ErrorResult& aError) {
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * just treat this the way we would an IDL replaceable property.
   */
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer || !outer->CanMoveResizeWindows(aCallerType) || outer->IsFrame()) {
    RedefineProperty(aCx, aPropName, aValue, aError);
    return;
  }

  int32_t value;
  if (!ValueToPrimitive<int32_t, eDefault>(aCx, aValue, aPropName, &value)) {
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

    if (innerWidthSpecified || innerHeightSpecified || outerWidthSpecified ||
        outerHeightSpecified) {
      nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = outer->GetTreeOwnerWindow();
      nsCOMPtr<nsIScreen> screen;
      nsCOMPtr<nsIScreenManager> screenMgr(
          do_GetService("@mozilla.org/gfx/screenmanager;1"));
      int32_t winLeft = 0;
      int32_t winTop = 0;
      int32_t winWidth = 0;
      int32_t winHeight = 0;
      double scale = 1.0;

      if (treeOwnerAsWin && screenMgr) {
        // Acquire current window size.
        treeOwnerAsWin->GetUnscaledDevicePixelsPerCSSPixel(&scale);
        treeOwnerAsWin->GetPositionAndSize(&winLeft, &winTop, &winWidth,
                                           &winHeight);
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
          int32_t* targetContentWidth = nullptr;
          int32_t* targetContentHeight = nullptr;
          int32_t screenWidth = 0;
          int32_t screenHeight = 0;
          int32_t chromeWidth = 0;
          int32_t chromeHeight = 0;
          int32_t inputWidth = 0;
          int32_t inputHeight = 0;
          int32_t unused = 0;

          // Get screen dimensions (in device pixels)
          screen->GetAvailRect(&unused, &unused, &screenWidth, &screenHeight);
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
              chromeWidth, chromeHeight, screenWidth, screenHeight, inputWidth,
              inputHeight, outerWidthSpecified, outerHeightSpecified,
              targetContentWidth, targetContentHeight);
        }
      }
    }
  }

  (this->*aSetter)(value, aCallerType, aError);
}

void nsGlobalWindowInner::FireOnNewGlobalObject() {
  // AutoEntryScript required to invoke debugger hook, which is a
  // Gecko-specific concept at present.
  AutoEntryScript aes(this, "nsGlobalWindowInner report new global");
  JS::Rooted<JSObject*> global(aes.cx(), GetWrapper());
  JS_FireOnNewGlobalObject(aes.cx(), global);
}

#if defined(_WINDOWS_) && !defined(MOZ_WRAPPED_WINDOWS_H)
#  pragma message( \
      "wrapper failure reason: " MOZ_WINDOWS_WRAPPER_DISABLED_REASON)
#  error "Never include unwrapped windows.h in this file!"
#endif

already_AddRefed<Promise> nsGlobalWindowInner::CreateImageBitmap(
    JSContext* aCx, const ImageBitmapSource& aImage, ErrorResult& aRv) {
  return ImageBitmap::Create(this, aImage, Nothing(), aRv);
}

already_AddRefed<Promise> nsGlobalWindowInner::CreateImageBitmap(
    JSContext* aCx, const ImageBitmapSource& aImage, int32_t aSx, int32_t aSy,
    int32_t aSw, int32_t aSh, ErrorResult& aRv) {
  return ImageBitmap::Create(this, aImage,
                             Some(gfx::IntRect(aSx, aSy, aSw, aSh)), aRv);
}

nsresult nsGlobalWindowInner::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->Dispatch(aCategory, std::move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget* nsGlobalWindowInner::EventTargetFor(
    TaskCategory aCategory) const {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread* nsGlobalWindowInner::AbstractMainThreadFor(
    TaskCategory aCategory) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}

Worklet* nsGlobalWindowInner::GetPaintWorklet(ErrorResult& aRv) {
  if (!mPaintWorklet) {
    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mPaintWorklet = PaintWorkletImpl::CreateWorklet(this, principal);
  }

  return mPaintWorklet;
}

void nsGlobalWindowInner::GetRegionalPrefsLocales(
    nsTArray<nsString>& aLocales) {
  MOZ_ASSERT(mozilla::intl::LocaleService::GetInstance());

  AutoTArray<nsCString, 10> rpLocales;
  mozilla::intl::LocaleService::GetInstance()->GetRegionalPrefsLocales(
      rpLocales);

  for (const auto& loc : rpLocales) {
    aLocales.AppendElement(NS_ConvertUTF8toUTF16(loc));
  }
}

void nsGlobalWindowInner::GetWebExposedLocales(nsTArray<nsString>& aLocales) {
  MOZ_ASSERT(mozilla::intl::LocaleService::GetInstance());

  AutoTArray<nsCString, 10> rpLocales;
  mozilla::intl::LocaleService::GetInstance()->GetWebExposedLocales(rpLocales);

  for (const auto& loc : rpLocales) {
    aLocales.AppendElement(NS_ConvertUTF8toUTF16(loc));
  }
}

IntlUtils* nsGlobalWindowInner::GetIntlUtils(ErrorResult& aError) {
  if (!mIntlUtils) {
    mIntlUtils = new IntlUtils(this);
  }

  return mIntlUtils;
}

void nsGlobalWindowInner::StoreSharedWorker(SharedWorker* aSharedWorker) {
  MOZ_ASSERT(aSharedWorker);
  MOZ_ASSERT(!mSharedWorkers.Contains(aSharedWorker));

  mSharedWorkers.AppendElement(aSharedWorker);
}

void nsGlobalWindowInner::ForgetSharedWorker(SharedWorker* aSharedWorker) {
  MOZ_ASSERT(aSharedWorker);
  MOZ_ASSERT(mSharedWorkers.Contains(aSharedWorker));

  mSharedWorkers.RemoveElement(aSharedWorker);
}

void nsGlobalWindowInner::StorageAccessGranted() {
  PropagateFirstPartyStorageAccessGrantedToWorkers(this);

  // If we have a partitioned localStorage, it's time to replace it with a real
  // one in order to receive notifications.

  if (mLocalStorage) {
    IgnoredErrorResult error;
    GetLocalStorage(error);
    if (NS_WARN_IF(error.Failed())) {
      return;
    }

    MOZ_ASSERT(mLocalStorage &&
               mLocalStorage->Type() == Storage::eLocalStorage);

    if (NextGenLocalStorageEnabled() && mListenerManager &&
        mListenerManager->HasListenersFor(nsGkAtoms::onstorage)) {
      auto object = static_cast<LSObject*>(mLocalStorage.get());

      object->EnsureObserver();
    }
  }

  // Reset the IndexedDB factory.
  mIndexedDB = nullptr;

  // Reset DOM Cache
  mCacheStorage = nullptr;

  // Reset the active storage principal
  if (mDoc) {
    mDoc->ClearActiveStoragePrincipal();
  }
}

/* static */
already_AddRefed<nsGlobalWindowInner> nsGlobalWindowInner::Create(
    nsGlobalWindowOuter* aOuterWindow, bool aIsChrome,
    WindowGlobalChild* aActor) {
  RefPtr<nsGlobalWindowInner> window =
      new nsGlobalWindowInner(aOuterWindow, aActor);
  if (aIsChrome) {
    window->mIsChrome = true;
    window->mCleanMessageManager = true;
  }

  if (aActor) {
    aActor->InitWindowGlobal(window);
  }

  window->InitWasOffline();
  return window.forget();
}

nsIURI* nsPIDOMWindowInner::GetDocumentURI() const {
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}

nsIURI* nsPIDOMWindowInner::GetDocBaseURI() const {
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

nsIPrincipal* nsPIDOMWindowInner::GetDocumentContentBlockingAllowListPrincipal()
    const {
  return mDoc ? mDoc->GetContentBlockingAllowListPrincipal()
              : mDocContentBlockingAllowListPrincipal.get();
}

mozilla::dom::WindowContext* nsPIDOMWindowInner::GetWindowContext() const {
  return mWindowGlobalChild ? mWindowGlobalChild->WindowContext() : nullptr;
}

void nsPIDOMWindowInner::MaybeCreateDoc() {
  // XXX: Forward to outer?
  MOZ_ASSERT(!mDoc);
  if (nsIDocShell* docShell = GetDocShell()) {
    // Note that |document| here is the same thing as our mDoc, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<Document> document = docShell->GetDocument();
    Unused << document;
  }
}

mozilla::dom::DocGroup* nsPIDOMWindowInner::GetDocGroup() const {
  Document* doc = GetExtantDoc();
  if (doc) {
    return doc->GetDocGroup();
  }
  return nullptr;
}

mozilla::dom::BrowsingContextGroup*
nsPIDOMWindowInner::GetBrowsingContextGroup() const {
  return mBrowsingContext ? mBrowsingContext->Group() : nullptr;
}

nsIGlobalObject* nsPIDOMWindowInner::AsGlobal() {
  return nsGlobalWindowInner::Cast(this);
}

const nsIGlobalObject* nsPIDOMWindowInner::AsGlobal() const {
  return nsGlobalWindowInner::Cast(this);
}

void nsPIDOMWindowInner::SaveStorageAccessGranted(
    const nsACString& aPermissionKey) {
  if (!HasStorageAccessGranted(aPermissionKey)) {
    mStorageAccessGranted.AppendElement(aPermissionKey);
  }

  nsGlobalWindowInner::Cast(this)->ClearActiveStoragePrincipal();
}

void nsGlobalWindowInner::ClearActiveStoragePrincipal() {
  Document* doc = GetExtantDoc();
  if (doc) {
    doc->ClearActiveStoragePrincipal();
  }

  CallOnInProcessChildren(&nsGlobalWindowInner::ClearActiveStoragePrincipal);
}

bool nsPIDOMWindowInner::HasStorageAccessGranted(
    const nsACString& aPermissionKey) {
  return mStorageAccessGranted.Contains(aPermissionKey);
}

nsPIDOMWindowInner::nsPIDOMWindowInner(nsPIDOMWindowOuter* aOuterWindow,
                                       WindowGlobalChild* aActor)
    : mMutationBits(0),
      mActivePeerConnections(0),
      mIsDocumentLoaded(false),
      mIsHandlingResizeEvent(false),
      mMayHavePaintEventListener(false),
      mMayHaveTouchEventListener(false),
      mMayHaveSelectionChangeEventListener(false),
      mMayHaveMouseEnterLeaveEventListener(false),
      mMayHavePointerEnterLeaveEventListener(false),
      mMayHaveTextEventListenerInDefaultGroup(false),
      mOuterWindow(aOuterWindow),
      mWindowID(0),
      mHasNotifiedGlobalCreated(false),
      mMarkedCCGeneration(0),
      mHasTriedToCacheTopInnerWindow(false),
      mNumOfIndexedDBDatabases(0),
      mNumOfOpenWebSockets(0),
      mEvent(nullptr),
      mWindowGlobalChild(aActor) {
  MOZ_ASSERT(aOuterWindow);
  mBrowsingContext = aOuterWindow->GetBrowsingContext();

  if (mWindowGlobalChild) {
    mWindowID = aActor->InnerWindowId();

    MOZ_ASSERT(mWindowGlobalChild->BrowsingContext() == mBrowsingContext);
  } else {
    mWindowID = nsContentUtils::GenerateWindowId();
  }
}

void nsPIDOMWindowInner::RegisterReportingObserver(ReportingObserver* aObserver,
                                                   bool aBuffered) {
  MOZ_ASSERT(aObserver);

  if (mReportingObservers.Contains(aObserver)) {
    return;
  }

  if (NS_WARN_IF(!mReportingObservers.AppendElement(aObserver, fallible))) {
    return;
  }

  if (!aBuffered) {
    return;
  }

  for (Report* report : mReportRecords) {
    aObserver->MaybeReport(report);
  }
}

void nsPIDOMWindowInner::UnregisterReportingObserver(
    ReportingObserver* aObserver) {
  MOZ_ASSERT(aObserver);
  mReportingObservers.RemoveElement(aObserver);
}

void nsPIDOMWindowInner::BroadcastReport(Report* aReport) {
  MOZ_ASSERT(aReport);

  for (ReportingObserver* observer : mReportingObservers) {
    observer->MaybeReport(aReport);
  }

  if (NS_WARN_IF(!mReportRecords.AppendElement(aReport, fallible))) {
    return;
  }

  while (mReportRecords.Length() > MAX_REPORT_RECORDS) {
    mReportRecords.RemoveElementAt(0);
  }
}

void nsPIDOMWindowInner::NotifyReportingObservers() {
  const nsTArray<RefPtr<ReportingObserver>> reportingObservers(
      mReportingObservers);
  for (auto& observer : reportingObservers) {
    // MOZ_KnownLive because 'reportingObservers' is guaranteed to
    // keep it alive.
    //
    // This can go away once
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
    MOZ_KnownLive(observer)->MaybeNotify();
  }
}

nsPIDOMWindowInner::~nsPIDOMWindowInner() = default;

#undef FORWARD_TO_OUTER
#undef FORWARD_TO_OUTER_OR_THROW
#undef FORWARD_TO_OUTER_VOID
