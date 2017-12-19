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
#include "mozilla/dom/LocalStorage.h"
#include "mozilla/dom/Storage.h"
#include "mozilla/dom/IdleRequest.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/StorageNotifierService.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/Timeout.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/IntegerPrintfMacros.h"
#if defined(MOZ_WIDGET_ANDROID)
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
#include "nsIDocumentLoader.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsITimeoutHandler.h"
#include "nsIController.h"
#include "nsScriptNameSpaceManager.h"
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
#include "jsapi.h"              // for JSAutoRequest
#include "jswrapper.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSEnvironment.h"
#include "mozilla/dom/ScriptSettings.h"
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
#include "nsIAddonPolicyService.h"
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

#include "gfxVR.h"
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
#include "mozilla/dom/IntlUtils.h"
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

#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientSource.h"
#include "mozilla/dom/ClientState.h"

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

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::cache::CacheStorage;

#define FORWARD_TO_OUTER(method, args, err_rval)                        \
  PR_BEGIN_MACRO                                                        \
  nsGlobalWindowOuter *outer = GetOuterWindowInternal();                \
  if (!HasActiveDocument()) {                                           \
    NS_WARNING(outer ?                                                  \
               "Inner window does not have active document." :          \
               "No outer window available!");                           \
    return err_rval;                                                    \
  }                                                                     \
  return outer->method args;                                            \
  PR_END_MACRO

#define FORWARD_TO_OUTER_OR_THROW(method, args, errorresult, err_rval)  \
  PR_BEGIN_MACRO                                                        \
  nsGlobalWindowOuter *outer = GetOuterWindowInternal();                \
  if (MOZ_LIKELY(HasActiveDocument())) {                                \
    return outer->method args;                                          \
  }                                                                     \
  if (!outer) {                                                         \
    NS_WARNING("No outer window available!");                           \
    errorresult.Throw(NS_ERROR_NOT_INITIALIZED);                        \
  } else {                                                              \
    errorresult.Throw(NS_ERROR_XPC_SECURITY_MANAGER_VETO);              \
  }                                                                     \
  return err_rval;                                                      \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                             \
  PR_BEGIN_MACRO                                                        \
  nsGlobalWindowOuter *outer = GetOuterWindowInternal();                \
  if (!HasActiveDocument()) {                                           \
    NS_WARNING(outer ?                                                  \
               "Inner window does not have active document." :          \
               "No outer window available!");                           \
    return;                                                             \
  }                                                                     \
  outer->method args;                                                   \
  return;                                                               \
  PR_END_MACRO

#define DOM_TOUCH_LISTENER_ADDED "dom-touch-listener-added"
#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"

// Amount of time allowed between alert/prompt/confirm before enabling
// the stop dialog checkbox.
#define DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT 3 // 3 sec

// Maximum number of successive dialogs before we prompt users to disable
// dialogs for this window.
#define MAX_SUCCESSIVE_DIALOG_COUNT 5

// Idle fuzz time upper limit
#define MAX_IDLE_FUZZ_TIME_MS 90000

// Min idle notification time in seconds.
#define MIN_IDLE_NOTIFICATION_TIME_S 1

static LazyLogModule gDOMLeakPRLogInner("DOMLeakInner");

static bool                 gIdleObserversAPIFuzzTimeDisabled = false;
static FILE                *gDumpFile                         = nullptr;

nsGlobalWindowInner::InnerWindowByIdTable *nsGlobalWindowInner::sInnerWindowsById = nullptr;

bool nsGlobalWindowInner::sDragServiceDisabled = false;
bool nsGlobalWindowInner::sMouseDown = false;

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver final : public nsIObserver
                                   , public nsIInterfaceRequestor
                                   , public StorageNotificationObserver
{
public:
  explicit nsGlobalWindowObserver(nsGlobalWindowInner* aWindow) : mWindow(aWindow) {}
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

  void
  ObserveStorageNotification(StorageEvent* aEvent,
                             const char16_t* aStorageType,
                             bool aPrivateBrowsing) override
  {
    if (mWindow) {
      mWindow->ObserveStorageNotification(aEvent, aStorageType,
                                          aPrivateBrowsing);
    }
  }

  nsIPrincipal*
  GetPrincipal() const override
  {
    return mWindow ? mWindow->GetPrincipal() : nullptr;
  }

  bool
  IsPrivateBrowsing() const override
  {
    return mWindow ? mWindow->IsPrivateBrowsing() : false;
  }

  nsIEventTarget*
  GetEventTarget() const override
  {
    return mWindow ? mWindow->EventTargetFor(TaskCategory::Other) : nullptr;
  }

private:
  ~nsGlobalWindowObserver() = default;

  // This reference is non-owning and safe because it's cleared by
  // nsGlobalWindowInner::CleanUp().
  nsGlobalWindowInner* MOZ_NON_OWNING_REF mWindow;
};

NS_IMPL_ISUPPORTS(nsGlobalWindowObserver, nsIObserver, nsIInterfaceRequestor)

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
  ~IdleRequestExecutorTimeoutHandler() override {}
  RefPtr<IdleRequestExecutor> mExecutor;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler, mExecutor)

NS_IMPL_ADDREF_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler)
NS_IMPL_RELEASE_INHERITED(IdleRequestExecutorTimeoutHandler, TimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleRequestExecutorTimeoutHandler)
NS_INTERFACE_MAP_END_INHERITING(TimeoutHandler)


class IdleRequestExecutor final : public nsIRunnable
                                , public nsICancelableRunnable
                                , public nsINamed
                                , public nsIIdleRunnable
{
public:
  explicit IdleRequestExecutor(nsGlobalWindowInner* aWindow)
    : mDispatched(false)
    , mDeadline(TimeStamp::Now())
    , mWindow(aWindow)
  {
    MOZ_DIAGNOSTIC_ASSERT(mWindow);

    mIdlePeriodLimit = { mDeadline, mWindow->LastIdleRequestHandle() };
    mDelayedExecutorDispatcher = new IdleRequestExecutorTimeoutHandler(this);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(IdleRequestExecutor, nsIRunnable)

  NS_DECL_NSIRUNNABLE
  NS_DECL_NSINAMED
  nsresult Cancel() override;
  void SetDeadline(TimeStamp aDeadline) override;

  bool IsCancelled() const { return !mWindow || mWindow->InnerObjectsFreed(); }
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

  ~IdleRequestExecutor() override {}

  bool mDispatched;
  TimeStamp mDeadline;
  IdlePeriodLimit mIdlePeriodLimit;
  RefPtr<nsGlobalWindowInner> mWindow;
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
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsIIdleRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
IdleRequestExecutor::GetName(nsACString& aName)
{
    aName.AssignASCII("IdleRequestExecutor");
    return NS_OK;
}

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
    mWindow->TimeoutManager().ClearTimeout(
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
  mWindow->TimeoutManager().SetTimeout(
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
nsGlobalWindowInner::ScheduleIdleRequestDispatch()
{
  AssertIsOnMainThread();

  if (!mIdleRequestExecutor) {
    mIdleRequestExecutor = new IdleRequestExecutor(this);
  }

  mIdleRequestExecutor->MaybeDispatch();
}

void
nsGlobalWindowInner::SuspendIdleRequests()
{
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->Cancel();
    mIdleRequestExecutor = nullptr;
  }
}

void
nsGlobalWindowInner::ResumeIdleRequests()
{
  MOZ_ASSERT(!mIdleRequestExecutor);

  ScheduleIdleRequestDispatch();
}

void
nsGlobalWindowInner::RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest)
{
  AssertIsOnMainThread();

  if (aRequest->HasTimeout()) {
    mTimeoutManager->ClearTimeout(aRequest->GetTimeoutHandle(),
                                  Timeout::Reason::eIdleCallbackTimeout);
  }

  aRequest->removeFrom(mIdleRequestCallbacks);
}

nsresult
nsGlobalWindowInner::RunIdleRequest(IdleRequest* aRequest,
                               DOMHighResTimeStamp aDeadline,
                               bool aDidTimeout)
{
  AssertIsOnMainThread();
  RefPtr<IdleRequest> request(aRequest);
  RemoveIdleCallback(request);
  return request->IdleRun(this, aDeadline, aDidTimeout);
}

nsresult
nsGlobalWindowInner::ExecuteIdleRequest(TimeStamp aDeadline)
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

  // Running the idle callback could've suspended the window, in which
  // case mIdleRequestExecutor will be null.
  if (mIdleRequestExecutor) {
    mIdleRequestExecutor->MaybeDispatch();
  }
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
    return nsGlobalWindowInner::Cast(mWindow)->RunIdleRequest(mIdleRequest, 0.0, true);
  }

private:
  ~IdleRequestTimeoutHandler() override {}

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
nsGlobalWindowInner::RequestIdleCallback(JSContext* aCx,
                                         IdleRequestCallback& aCallback,
                                         const IdleRequestOptions& aOptions,
                                         ErrorResult& aError)
{
  AssertIsOnMainThread();

  if (mInnerObjectsFreed) {
   return 0;
  }

  uint32_t handle = mIdleRequestCallbackCounter++;

  RefPtr<IdleRequest> request =
    new IdleRequest(&aCallback, handle);

  if (aOptions.mTimeout.WasPassed()) {
    int32_t timeoutHandle;
    nsCOMPtr<nsITimeoutHandler> handler(new IdleRequestTimeoutHandler(aCx, request, this));

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

void
nsGlobalWindowInner::CancelIdleCallback(uint32_t aHandle)
{
  for (IdleRequest* r : mIdleRequestCallbacks) {
    if (r->Handle() == aHandle) {
      RemoveIdleCallback(r);
      break;
    }
  }
}

void
nsGlobalWindowInner::DisableIdleCallbackRequests()
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
nsGlobalWindowInner::IsBackgroundInternal() const
{
  return !mOuterWindow || mOuterWindow->IsBackground();
}

//*****************************************************************************
//***    nsGlobalWindowInner: Object Management
//*****************************************************************************

nsGlobalWindowInner::nsGlobalWindowInner(nsGlobalWindowOuter *aOuterWindow)
  : nsPIDOMWindowInner(aOuterWindow->AsOuter()),
    mIdleFuzzFactor(0),
    mIdleCallbackIndex(-1),
    mCurrentlyIdle(false),
    mAddActiveEventFuzzTime(true),
    mWasOffline(false),
    mHasHadSlowScript(false),
    mNotifyIdleObserversIdleOnThaw(false),
    mNotifyIdleObserversActiveOnThaw(false),
    mIsChrome(false),
    mCleanMessageManager(false),
    mNeedsFocus(true),
    mHasFocus(false),
    mShowFocusRingForContent(false),
    mFocusByKeyOccurred(false),
    mHasGamepad(false),
    mHasVREvents(false),
    mHasVRDisplayActivateEvents(false),
    mHasSeenGamepadInput(false),
    mSuspendDepth(0),
    mFreezeDepth(0),
    mFocusMethod(0),
    mSerial(0),
    mIdleRequestCallbackCounter(1),
    mIdleRequestExecutor(nullptr),
    mCleanedUp(false),
    mDialogAbuseCount(0),
    mAreDialogsEnabled(true),
    mCanSkipCCGeneration(0),
    mBeforeUnloadListenerCount(0)
{
  AssertIsOnMainThread();

  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  if (aOuterWindow) {
    // |this| is an inner window, add this inner window to the outer
    // window list of inners.
    PR_INSERT_AFTER(this, aOuterWindow);

    mTimeoutManager =
      MakeUnique<mozilla::dom::TimeoutManager>(*nsGlobalWindowInner::Cast(AsInner()));

    mObserver = new nsGlobalWindowObserver(this);
    if (mObserver) {
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        // Watch for online/offline status changes so we can fire events. Use
        // a strong reference.
        os->AddObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                        false);

        os->AddObserver(mObserver, MEMORY_PRESSURE_OBSERVER_TOPIC, false);
      }

      Preferences::AddStrongObserver(mObserver, "intl.accept_languages");

      // Watch for storage notifications so we can fire storage events.
      RefPtr<StorageNotifierService> sns =
        StorageNotifierService::GetOrCreate();
      if (sns) {
        sns->Register(mObserver);
      }
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window.
    MOZ_ASSERT(IsFrozen());
  }

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIDocShell> docShell = GetDocShell();
    if (docShell) {
      mTabChild = docShell->GetTabChild();
    }
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.

  mSerial = nsContentUtils::InnerOrOuterWindowCreated();

  static bool sFirstTime = true;
  if (sFirstTime) {
    sFirstTime = false;
    TimeoutManager::Initialize();
    Preferences::AddBoolVarCache(&gIdleObserversAPIFuzzTimeDisabled,
                                 "dom.idle-observers-api.fuzz_time.disabled",
                                 false);
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
  if (!PR_GetEnv("MOZ_QUIET")) {
    printf_stderr("++DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p]\n",
                  nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  static_cast<void*>(ToCanonicalSupports(aOuterWindow)));
  }
#endif

  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
          ("DOMWINDOW %p created outer=%p", this, aOuterWindow));

  // Add ourselves to the inner windows list.
  MOZ_ASSERT(sInnerWindowsById, "Inner Windows hash table must be created!");
  MOZ_ASSERT(!sInnerWindowsById->Get(mWindowID),
             "This window shouldn't be in the hash table yet!");
  // We seem to see crashes in release builds because of null |sInnerWindowsById|.
  if (sInnerWindowsById) {
    sInnerWindowsById->Put(mWindowID, this);
  }
}

#ifdef DEBUG

/* static */
void
nsGlobalWindowInner::AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
}

#endif // DEBUG

/* static */
void
nsGlobalWindowInner::Init()
{
  AssertIsOnMainThread();

  NS_ASSERTION(gDOMLeakPRLogInner, "gDOMLeakPRLogInner should have been initialized!");

  sInnerWindowsById = new InnerWindowByIdTable();
}

nsGlobalWindowInner::~nsGlobalWindowInner()
{
  AssertIsOnMainThread();

  if (IsChromeWindow()) {
    MOZ_ASSERT(mCleanMessageManager,
              "chrome windows may always disconnect the msg manager");

    DisconnectAndClearGroupMessageManagers();

    if (mChromeFields.mMessageManager) {
      static_cast<nsFrameMessageManager *>(
        mChromeFields.mMessageManager.get())->Disconnect();
    }

    mCleanMessageManager = false;
  }

  DisconnectEventTargetObjects();

  if (sInnerWindowsById) {
    MOZ_ASSERT(sInnerWindowsById->Get(mWindowID),
                "This window should be in the hash table");
    sInnerWindowsById->Remove(mWindowID);
  }

  nsContentUtils::InnerOrOuterWindowDestroyed();

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

    nsGlobalWindowOuter* outer = nsGlobalWindowOuter::Cast(mOuterWindow);
    printf_stderr("--DOMWINDOW == %d (%p) [pid = %d] [serial = %d] [outer = %p] [url = %s]\n",
                  nsContentUtils::GetCurrentInnerOrOuterWindowCount(),
                  static_cast<void*>(ToCanonicalSupports(this)),
                  getpid(),
                  mSerial,
                  static_cast<void*>(ToCanonicalSupports(outer)),
                  url.get());
  }
#endif

  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug, ("DOMWINDOW %p destroyed", this));

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
  nsGlobalWindowOuter *outer = GetOuterWindowInternal();
  if (outer) {
    outer->MaybeClearInnerWindow(this);
  }

  // We don't have to leave the tab group if we are an inner window.

  // While CleanUp generally seems to be intended to clean up outers, we've
  // historically called it for both. Changing this would probably involve
  // auditing all of the references that inners and outers can have, and
  // separating the handling into CleanUp() and FreeInnerObjects.
  CleanUp();

  nsCOMPtr<nsIDeviceSensors> ac = do_GetService(NS_DEVICE_SENSORS_CONTRACTID);
  if (ac)
    ac->RemoveWindowAsListener(this);

  nsLayoutStatics::Release();
}

void
nsGlobalWindowInner::AddEventTargetObject(DOMEventTargetHelper* aObject)
{
  mEventTargetObjects.PutEntry(aObject);
}

void
nsGlobalWindowInner::RemoveEventTargetObject(DOMEventTargetHelper* aObject)
{
  mEventTargetObjects.RemoveEntry(aObject);
}

void
nsGlobalWindowInner::DisconnectEventTargetObjects()
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
nsGlobalWindowInner::ShutDown()
{
  AssertIsOnMainThread();

  if (gDumpFile && gDumpFile != stdout) {
    fclose(gDumpFile);
  }
  gDumpFile = nullptr;

  delete sInnerWindowsById;
  sInnerWindowsById = nullptr;
}

// static
void
nsGlobalWindowInner::CleanupCachedXBLHandlers()
{
  if (mCachedXBLPrototypeHandlers &&
      mCachedXBLPrototypeHandlers->Count() > 0) {
    mCachedXBLPrototypeHandlers->Clear();
  }
}

void
nsGlobalWindowInner::CleanUp()
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
      os->RemoveObserver(mObserver, MEMORY_PRESSURE_OBSERVER_TOPIC);
    }

    RefPtr<StorageNotifierService> sns = StorageNotifierService::GetOrCreate();
    if (sns) {
     sns->Unregister(mObserver);
    }

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
  mHistory = nullptr;
  mCustomElements = nullptr;
  mApplicationCache = nullptr;
  mIndexedDB = nullptr;

  mConsole = nullptr;

  mAudioWorklet = nullptr;
  mPaintWorklet = nullptr;

  mExternal = nullptr;

  mPerformance = nullptr;

#ifdef MOZ_WEBSPEECH
  mSpeechSynthesis = nullptr;
#endif

#if defined(MOZ_WIDGET_ANDROID)
  mOrientationChangeObserver = nullptr;
#endif

  mChromeEventHandler = nullptr; // Forces Release
  mParentTarget = nullptr;

  DisableGamepadUpdates();
  mHasGamepad = false;
  DisableVRUpdates();
  mHasVREvents = false;
  mHasVRDisplayActivateEvents = false;
  DisableIdleCallbackRequests();

  if (mCleanMessageManager) {
    MOZ_ASSERT(mIsChrome, "only chrome should have msg manager cleaned");
    if (mChromeFields.mMessageManager) {
      static_cast<nsFrameMessageManager*>(
        mChromeFields.mMessageManager.get())->Disconnect();
    }
  }

  CleanupCachedXBLHandlers();

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = nullptr;
  }

  mServiceWorkerRegistrationTable.Clear();

  mIntlUtils = nullptr;
}

void
nsGlobalWindowInner::FreeInnerObjects()
{
  // Make sure that this is called before we null out the document and
  // other members that the window destroyed observers could
  // re-create.
  NotifyDOMWindowDestroyed(this);
  if (auto* reporter = nsWindowMemoryReporter::Get()) {
    reporter->ObserveDOMWindowDetached(this);
  }

  mInnerObjectsFreed = true;

  // Kill all of the workers for this window.
  mozilla::dom::workers::CancelWorkersForWindow(this);

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

#if defined(MOZ_WIDGET_ANDROID)
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

  CleanupCachedXBLHandlers();

  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    mAudioContexts[i]->Shutdown();
  }
  mAudioContexts.Clear();

  DisableGamepadUpdates();
  mHasGamepad = false;
  mGamepads.Clear();
  DisableVRUpdates();
  mHasVREvents = false;
  mHasVRDisplayActivateEvents = false;
  mVRDisplays.Clear();

  // This breaks a cycle between the window and the ClientSource object.
  mClientSource.reset();

  if (mTabChild) {
    // Remove any remaining listeners, and reset mBeforeUnloadListenerCount.
    for (int i = 0; i < mBeforeUnloadListenerCount; ++i) {
      mTabChild->BeforeUnloadRemoved();
    }
    mBeforeUnloadListenerCount = 0;
  }
}

//*****************************************************************************
// nsGlobalWindowInner::nsISupports
//*****************************************************************************

// QueryInterface implementation for nsGlobalWindowInner
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGlobalWindowInner)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // Make sure this matches the cast in nsGlobalWindowInner::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  if (aIID.Equals(NS_GET_IID(nsPIDOMWindowInner))) {
    foundInterface = static_cast<nsPIDOMWindowInner*>(this);
  } else
  if (aIID.Equals(NS_GET_IID(mozIDOMWindow))) {
    foundInterface = static_cast<mozIDOMWindow*>(this);
  } else
  if (aIID.Equals(NS_GET_IID(nsIDOMChromeWindow)) && IsChromeWindow()) {
    foundInterface = static_cast<nsIDOMChromeWindow*>(this);
  } else
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
    SprintfLiteral(name, "nsGlobalWindowInner # %" PRIu64 " inner %s", tmp->mWindowID,
                   uri.get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsGlobalWindowInner, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerRegistrationTable)

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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mApplicationCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleService)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleRequestExecutor)
  for (IdleRequest* request : tmp->mIdleRequestCallbacks) {
    cb.NoteNativeChild(request, NS_CYCLE_COLLECTION_PARTICIPANT(IdleRequest));
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIdleObservers)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClientSource)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepads)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRDisplays)

  // Traverse stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentTarget)
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIntlUtils)

  tmp->TraverseHostObjectURIs(cb);

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeFields.mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChromeFields.mGroupMessageManagers)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingPromises)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindowInner)
  tmp->CleanupCachedXBLHandlers();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNavigator)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorkerRegistrationTable)

#ifdef MOZ_WEBSPEECH
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechSynthesis)
#endif

  if (tmp->mOuterWindow) {
    nsGlobalWindowOuter::Cast(tmp->mOuterWindow)->
      MaybeClearInnerWindow(tmp);
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocalStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessionStorage)
  if (tmp->mApplicationCache) {
    static_cast<nsDOMOfflineResourceList*>(tmp->mApplicationCache.get())->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mApplicationCache)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndexedDB)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDoc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleObservers)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGamepads)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCacheStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVRDisplays)

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentTarget)
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIntlUtils)

  tmp->UnlinkHostObjectURIs();

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIdleRequestExecutor)

  // Here the IdleRequest list would've been unlinked, but we rely on
  // that IdleRequest objects have been traced and will remove
  // themselves while unlinking.

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClientSource)

  if (tmp->IsChromeWindow()) {
    if (tmp->mChromeFields.mMessageManager) {
      static_cast<nsFrameMessageManager*>(
        tmp->mChromeFields.mMessageManager.get())->Disconnect();
      NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeFields.mMessageManager)
    }
    tmp->DisconnectAndClearGroupMessageManagers();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mChromeFields.mGroupMessageManagers)
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingPromises)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

#ifdef DEBUG
void
nsGlobalWindowInner::RiskyUnlink()
{
  NS_CYCLE_COLLECTION_INNERNAME.Unlink(this);
}
#endif

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGlobalWindowInner)
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
nsGlobalWindowInner::IsBlackForCC(bool aTracingNeeded)
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
// nsGlobalWindowInner::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindowInner::EnsureScriptEnvironment()
{
  // NOTE: We can't use FORWARD_TO_OUTER here because we don't want to fail if
  // we're called on an inactive inner window.
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    NS_WARNING("No outer window available!");
    return NS_ERROR_FAILURE;
  }
  return outer->EnsureScriptEnvironment();
}

nsIScriptContext *
nsGlobalWindowInner::GetScriptContext()
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    return nullptr;
  }
  return outer->GetScriptContext();
}

JSObject *
nsGlobalWindowInner::GetGlobalJSObject()
{
  return FastGetGlobalJSObject();
}

void
nsGlobalWindowInner::TraceGlobalJSObject(JSTracer* aTrc)
{
  TraceWrapper(aTrc, "active window global");
}

PopupControlState
nsGlobalWindowInner::GetPopupControlState() const
{
  return nsContentUtils::GetPopupControlState();
}

nsresult
nsGlobalWindowInner::SetNewDocument(nsIDocument* aDocument,
                                    nsISupports* aState,
                                    bool aForceReuseInnerWindow)
{
  NS_PRECONDITION(mDocumentPrincipal == nullptr,
                  "mDocumentPrincipal prematurely set!");
  MOZ_ASSERT(aDocument);

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

void
nsGlobalWindowInner::InnerSetNewDocument(JSContext* aCx, nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  if (MOZ_LOG_TEST(gDOMLeakPRLogInner, LogLevel::Debug)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
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

nsresult
nsGlobalWindowInner::EnsureClientSource()
{
  MOZ_DIAGNOSTIC_ASSERT(mDoc);

  bool newClientSource = false;

  nsCOMPtr<nsIChannel> channel = mDoc->GetChannel();
  nsCOMPtr<nsILoadInfo> loadInfo = channel ? channel->GetLoadInfo() : nullptr;

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
    UniquePtr<ClientSource> reservedClient = loadInfo->TakeReservedClientSource();
    if (reservedClient) {
      mClientSource.reset();
      mClientSource = Move(reservedClient);
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
    mClientSource = Move(initialClientSource);
    if (mClientSource) {
      newClientSource = true;
    }
  }

  // If we don't have a reserved client or an initial client, then create
  // one now.  This can happen in certain cases where we avoid preallocating
  // the client in the docshell.  This mainly occurs in situations where
  // the principal is not clearly inherited from the parent; e.g. sandboxed
  // iframes, window.open(), etc.
  // TODO: We may not be marking initial about:blank documents created
  //       this way as controlled by a service worker properly.  The
  //       controller should be coming from the same place as the inheritted
  //       principal.  We do this in docshell, but as mentioned we aren't
  //       smart enough to handle all cases yet.  For example, a
  //       window.open() with new URL should inherit the controller from
  //       the opener, but we probably don't handle that yet.
  if (!mClientSource) {
    mClientSource = ClientManager::CreateSource(ClientType::Window,
                                                EventTargetFor(TaskCategory::Other),
                                                mDoc->NodePrincipal());
    MOZ_DIAGNOSTIC_ASSERT(mClientSource);
    newClientSource = true;
  }

  // The load may have started controlling the Client as well.  If
  // so, mark it as controlled immediately here.  The actor may
  // or may not have been notified by the parent side about being
  // controlled yet.
  if (loadInfo) {
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
      mClientSource =
        ClientManager::CreateSource(ClientType::Window,
                                    EventTargetFor(TaskCategory::Other),
                                    mDoc->NodePrincipal());
      MOZ_DIAGNOSTIC_ASSERT(mClientSource);
      newClientSource = true;
    }
  }

  // Its possible that we got a client just after being frozen in
  // the bfcache.  In that case freeze the client immediately.
  if (newClientSource && IsFrozen()) {
    mClientSource->Freeze();
  }

  return NS_OK;
}

nsresult
nsGlobalWindowInner::ExecutionReady()
{
  nsresult rv = EnsureClientSource();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mClientSource->WindowExecutionReady(AsInner());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsGlobalWindowInner::SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
                                     bool aOriginalOpener)
{
  FORWARD_TO_OUTER_VOID(SetOpenerWindow, (aOpener, aOriginalOpener));
}

void
nsGlobalWindowInner::UpdateParentTarget()
{
  // NOTE: This method is identical to
  // nsGlobalWindowOuter::UpdateParentTarget(). IF YOU UPDATE THIS METHOD,
  // UPDATE THE OTHER ONE TOO!

  // Try to get our frame element's tab child global (its in-process message
  // manager).  If that fails, fall back to the chrome event handler's tab
  // child global, and if it doesn't have one, just use the chrome event
  // handler itself.

  nsCOMPtr<Element> frameElement = GetOuterWindow()->GetFrameElementInternal();
  nsCOMPtr<EventTarget> eventTarget =
    nsContentUtils::TryGetTabChildGlobalAsEventTarget(frameElement);

  if (!eventTarget) {
    nsGlobalWindowOuter* topWin = GetScriptableTopInternal();
    if (topWin) {
      frameElement = topWin->AsOuter()->GetFrameElementInternal();
      eventTarget =
        nsContentUtils::TryGetTabChildGlobalAsEventTarget(frameElement);
    }
  }

  if (!eventTarget) {
    eventTarget =
      nsContentUtils::TryGetTabChildGlobalAsEventTarget(mChromeEventHandler);
  }

  if (!eventTarget) {
    eventTarget = mChromeEventHandler;
  }

  mParentTarget = eventTarget;
}

EventTarget*
nsGlobalWindowInner::GetTargetForDOMEvent()
{
  return GetOuterWindowInternal();
}

EventTarget*
nsGlobalWindowInner::GetTargetForEventTargetChain()
{
  return this;
}

nsresult
nsGlobalWindowInner::WillHandleEvent(EventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsresult
nsGlobalWindowInner::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
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
nsGlobalWindowInner::DialogsAreBeingAbused()
{
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

void
nsGlobalWindowInner::DisableDialogs()
{
  FORWARD_TO_OUTER_VOID(DisableDialogs, ());
}

void
nsGlobalWindowInner::EnableDialogs()
{
  FORWARD_TO_OUTER_VOID(EnableDialogs, ());
}

nsresult
nsGlobalWindowInner::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
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
    for (const auto& display : mVRDisplays) {
      if (display->IsPresenting()) {
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

nsresult
nsGlobalWindowInner::DefineArgumentsProperty(nsIArray *aArguments)
{
  nsIScriptContext *ctx = GetOuterWindowInternal()->mContext;
  NS_ENSURE_TRUE(aArguments && ctx, NS_ERROR_NOT_INITIALIZED);

  JS::Rooted<JSObject*> obj(RootingCx(), GetWrapperPreserveColor());
  return ctx->SetProperty(obj, "arguments", aArguments);
}

//*****************************************************************************
// nsGlobalWindowInner::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
nsGlobalWindowInner::GetPrincipal()
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
// nsGlobalWindowInner::nsIDOMWindow
//*****************************************************************************

bool
nsPIDOMWindowInner::AddAudioContext(AudioContext* aAudioContext)
{
  mAudioContexts.AppendElement(aAudioContext);

  // Return true if the context should be muted and false if not.
  nsIDocShell* docShell = GetDocShell();
  return docShell && !docShell->GetAllowMedia() && !aAudioContext->IsOffline();
}

void
nsPIDOMWindowInner::RemoveAudioContext(AudioContext* aAudioContext)
{
  mAudioContexts.RemoveElement(aAudioContext);
}

void
nsPIDOMWindowInner::MuteAudioContexts()
{
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Mute();
    }
  }
}

void
nsPIDOMWindowInner::UnmuteAudioContexts()
{
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    if (!mAudioContexts[i]->IsOffline()) {
      mAudioContexts[i]->Unmute();
    }
  }
}

nsGlobalWindowInner*
nsGlobalWindowInner::Window()
{
  return this;
}

nsGlobalWindowInner*
nsGlobalWindowInner::Self()
{
  return this;
}

Navigator*
nsGlobalWindowInner::Navigator()
{
  if (!mNavigator) {
    mNavigator = new mozilla::dom::Navigator(this);
  }

  return mNavigator;
}

nsIDOMNavigator*
nsGlobalWindowInner::GetNavigator()
{
  return Navigator();
}

nsScreen*
nsGlobalWindowInner::GetScreen(ErrorResult& aError)
{
  if (!mScreen) {
    mScreen = nsScreen::Create(this);
    if (!mScreen) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
  }

  return mScreen;
}

nsIDOMScreen*
nsGlobalWindowInner::GetScreen()
{
  ErrorResult dummy;
  nsIDOMScreen* screen = GetScreen(dummy);
  dummy.SuppressException();
  return screen;
}

nsHistory*
nsGlobalWindowInner::GetHistory(ErrorResult& aError)
{
  if (!mHistory) {
    mHistory = new nsHistory(this);
  }

  return mHistory;
}

CustomElementRegistry*
nsGlobalWindowInner::CustomElements()
{
  if (!mCustomElements) {
    mCustomElements = new CustomElementRegistry(this);
  }

  return mCustomElements;
}

Performance*
nsPIDOMWindowInner::GetPerformance()
{
  CreatePerformanceObjectIfNeeded();
  return mPerformance;
}

void
nsPIDOMWindowInner::CreatePerformanceObjectIfNeeded()
{
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
  return nsGlobalWindowInner::Cast(this)->IsSecureContext();
}

void
nsPIDOMWindowInner::Suspend()
{
  nsGlobalWindowInner::Cast(this)->Suspend();
}

void
nsPIDOMWindowInner::Resume()
{
  nsGlobalWindowInner::Cast(this)->Resume();
}

void
nsPIDOMWindowInner::Freeze()
{
  nsGlobalWindowInner::Cast(this)->Freeze();
}

void
nsPIDOMWindowInner::Thaw()
{
  nsGlobalWindowInner::Cast(this)->Thaw();
}

void
nsPIDOMWindowInner::SyncStateFromParentWindow()
{
  nsGlobalWindowInner::Cast(this)->SyncStateFromParentWindow();
}

Maybe<ClientInfo>
nsPIDOMWindowInner::GetClientInfo() const
{
  return Move(nsGlobalWindowInner::Cast(this)->GetClientInfo());
}

Maybe<ClientState>
nsPIDOMWindowInner::GetClientState() const
{
  return Move(nsGlobalWindowInner::Cast(this)->GetClientState());
}

Maybe<ServiceWorkerDescriptor>
nsPIDOMWindowInner::GetController() const
{
  return Move(nsGlobalWindowInner::Cast(this)->GetController());
}

void
nsPIDOMWindowInner::NoteCalledRegisterForServiceWorkerScope(const nsACString& aScope)
{
  nsGlobalWindowInner::Cast(this)->NoteCalledRegisterForServiceWorkerScope(aScope);
}

bool
nsGlobalWindowInner::ShouldReportForServiceWorkerScope(const nsAString& aScope)
{
  bool result = false;

  nsPIDOMWindowOuter* topOuter = GetScriptableTop();
  NS_ENSURE_TRUE(topOuter, false);

  nsGlobalWindowInner* topInner =
    nsGlobalWindowInner::Cast(topOuter->GetCurrentInnerWindow());
  NS_ENSURE_TRUE(topInner, false);

  topInner->ShouldReportForServiceWorkerScopeInternal(NS_ConvertUTF16toUTF8(aScope),
                                                      &result);
  return result;
}

nsGlobalWindowInner::CallState
nsGlobalWindowInner::ShouldReportForServiceWorkerScopeInternal(const nsACString& aScope,
                                                               bool* aResultOut)
{
  MOZ_DIAGNOSTIC_ASSERT(aResultOut);

  // First check to see if this window is controlled.  If so, then we have
  // found a match and are done.
  const Maybe<ServiceWorkerDescriptor> swd = GetController();
  if (swd.isSome() && swd.ref().Scope() == aScope) {
    *aResultOut = true;
    return CallState::Stop;
  }

  // Next, check to see if this window has called navigator.serviceWorker.register()
  // for this scope.  If so, then treat this as a match so console reports
  // appear in the devtools console.
  if (mClientSource && mClientSource->CalledRegisterForServiceWorkerScope(aScope)) {
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
  return CallOnChildren(&nsGlobalWindowInner::ShouldReportForServiceWorkerScopeInternal,
                        aScope, aResultOut);
}

void
nsGlobalWindowInner::NoteCalledRegisterForServiceWorkerScope(const nsACString& aScope)
{
  if (!mClientSource) {
    return;
  }

  mClientSource->NoteCalledRegisterForServiceWorkerScope(aScope);
}

void
nsGlobalWindowInner::UpdateTopInnerWindow()
{
  if (IsTopInnerWindow() || !mTopInnerWindow) {
    return;
  }

  mTopInnerWindow->UpdateWebSocketCount(-(int32_t)mNumOfOpenWebSockets);
}

void
nsPIDOMWindowInner::AddPeerConnection()
{
  MOZ_ASSERT(NS_IsMainThread());
  mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections++
                  : mActivePeerConnections++;
}

void
nsPIDOMWindowInner::RemovePeerConnection()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections
                             : mActivePeerConnections);

  mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections--
                  : mActivePeerConnections--;
}

bool
nsPIDOMWindowInner::HasActivePeerConnections()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mTopInnerWindow ? mTopInnerWindow->mActivePeerConnections
                         : mActivePeerConnections;
}

bool
nsPIDOMWindowInner::IsPlayingAudio()
{
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

bool
nsPIDOMWindowInner::IsDocumentLoaded() const
{
  return mIsDocumentLoaded;
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
nsPIDOMWindowInner::TryToCacheTopInnerWindow()
{
  if (mHasTriedToCacheTopInnerWindow) {
    return;
  }

  MOZ_ASSERT(!mInnerObjectsFreed);

  mHasTriedToCacheTopInnerWindow = true;

  nsGlobalWindowInner* window = nsGlobalWindowInner::Cast(this);

  MOZ_ASSERT(window);

  if (nsCOMPtr<nsPIDOMWindowOuter> topOutter = window->GetScriptableTop()) {
    mTopInnerWindow = topOutter->GetCurrentInnerWindow();
  }
}

void
nsPIDOMWindowInner::UpdateActiveIndexedDBTransactionCount(int32_t aDelta)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aDelta == 0) {
    return;
  }

  TabGroup()->IndexedDBTransactionCounter() += aDelta;
}

void
nsPIDOMWindowInner::UpdateActiveIndexedDBDatabaseCount(int32_t aDelta)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aDelta == 0) {
    return;
  }

  // We count databases but not transactions because only active databases
  // could block throttling.
  uint32_t& counter = mTopInnerWindow ?
    mTopInnerWindow->mNumOfIndexedDBDatabases : mNumOfIndexedDBDatabases;

  counter+= aDelta;

  TabGroup()->IndexedDBDatabaseCounter() += aDelta;
}

bool
nsPIDOMWindowInner::HasActiveIndexedDBDatabases()
{
  MOZ_ASSERT(NS_IsMainThread());

  return mTopInnerWindow ?
    mTopInnerWindow->mNumOfIndexedDBDatabases > 0 :
    mNumOfIndexedDBDatabases > 0;
}

void
nsPIDOMWindowInner::UpdateWebSocketCount(int32_t aDelta)
{
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

bool
nsPIDOMWindowInner::HasOpenWebSockets() const
{
  MOZ_ASSERT(NS_IsMainThread());

  return mNumOfOpenWebSockets ||
         (mTopInnerWindow && mTopInnerWindow->mNumOfOpenWebSockets);
}

bool
nsPIDOMWindowInner::GetAudioCaptured() const
{
  return mAudioCaptured;
}

nsresult
nsPIDOMWindowInner::SetAudioCapture(bool aCapture)
{
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
nsGlobalWindowInner::GetSpeechSynthesis(ErrorResult& aError)
{
  if (!mSpeechSynthesis) {
    mSpeechSynthesis = new SpeechSynthesis(this);
  }

  return mSpeechSynthesis;
}

bool
nsGlobalWindowInner::HasActiveSpeechSynthesis()
{
  if (mSpeechSynthesis) {
    return !mSpeechSynthesis->HasEmptyQueue();
  }

  return false;
}

#endif

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::GetParent(ErrorResult& aError)
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
nsGlobalWindowInner::GetScriptableParent()
{
  FORWARD_TO_OUTER(GetScriptableParent, (), nullptr);
}

/**
 * Behavies identically to GetScriptableParent extept that it returns null
 * if GetScriptableParent would return this window.
 */
nsPIDOMWindowOuter*
nsGlobalWindowInner::GetScriptableParentOrNull()
{
  FORWARD_TO_OUTER(GetScriptableParentOrNull, (), nullptr);
}

/**
 * GetScriptableTop is called when script reads window.top.
 *
 * In contrast to GetRealTop, GetScriptableTop respects <iframe mozbrowser>
 * boundaries.  If we encounter a window owned by an <iframe mozbrowser> while
 * walking up the window hierarchy, we'll stop and return that window.
 */
nsPIDOMWindowOuter*
nsGlobalWindowInner::GetScriptableTop()
{
  FORWARD_TO_OUTER(GetScriptableTop, (), nullptr);
}

void
nsGlobalWindowInner::GetContent(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aRetval,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetContentOuter,
                            (aCx, aRetval, aCallerType, aError), aError, );
}

BarProp*
nsGlobalWindowInner::GetMenubar(ErrorResult& aError)
{
  if (!mMenubar) {
    mMenubar = new MenubarProp(this);
  }

  return mMenubar;
}

BarProp*
nsGlobalWindowInner::GetToolbar(ErrorResult& aError)
{
  if (!mToolbar) {
    mToolbar = new ToolbarProp(this);
  }

  return mToolbar;
}

BarProp*
nsGlobalWindowInner::GetLocationbar(ErrorResult& aError)
{
  if (!mLocationbar) {
    mLocationbar = new LocationbarProp(this);
  }
  return mLocationbar;
}

BarProp*
nsGlobalWindowInner::GetPersonalbar(ErrorResult& aError)
{
  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarProp(this);
  }
  return mPersonalbar;
}

BarProp*
nsGlobalWindowInner::GetStatusbar(ErrorResult& aError)
{
  if (!mStatusbar) {
    mStatusbar = new StatusbarProp(this);
  }
  return mStatusbar;
}

BarProp*
nsGlobalWindowInner::GetScrollbars(ErrorResult& aError)
{
  if (!mScrollbars) {
    mScrollbars = new ScrollbarsProp(this);
  }

  return mScrollbars;
}

bool
nsGlobalWindowInner::GetClosed(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetClosedOuter, (), aError, false);
}

already_AddRefed<nsIDOMWindowCollection>
nsGlobalWindowInner::GetFrames()
{
  FORWARD_TO_OUTER(GetFrames, (), nullptr);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::IndexedGetter(uint32_t aIndex)
{
  FORWARD_TO_OUTER(IndexedGetterOuter, (aIndex), nullptr);
}

bool
nsGlobalWindowInner::DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                               JS::Handle<jsid> aId,
                               JS::MutableHandle<JS::PropertyDescriptor> aDesc)
{
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
nsGlobalWindowInner::MayResolve(jsid aId)
{
  // Note: This function does not fail and may not have any side-effects.
  // Note: Keep this in sync with DoResolve.
  if (!JSID_IS_STRING(aId)) {
    return false;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS)) {
    return true;
  }

  if (aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS) ||
      aId == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS_CLASS)) {
    // We only resolve .controllers/.Controllers in release builds and on non-chrome
    // windows, but let's not worry about any of that stuff.
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
nsGlobalWindowInner::GetOwnPropertyNames(JSContext* aCx, JS::AutoIdVector& aNames,
                                         bool aEnumerableOnly, ErrorResult& aRv)
{
  if (aEnumerableOnly) {
    // The names we would return from here get defined on the window via one of
    // two codepaths.  The ones coming from the WebIDLGlobalNameHash will end up
    // in the DefineConstructor function in BindingUtils, which always defines
    // things as non-enumerable.  The ones coming from the script namespace
    // manager get defined by nsDOMClassInfo::PostCreatePrototype calling
    // ResolvePrototype and using the resulting descriptot to define the
    // property.  ResolvePrototype always passes 0 to the FillPropertyDescriptor
    // for the property attributes, so all those are non-enumerable as well.
    //
    // So in the aEnumerableOnly case we have nothing to do.
    return;
  }

  // "Components" is marked as enumerable but only resolved on demand :-/.
  //aNames.AppendElement(NS_LITERAL_STRING("Components"));

  nsScriptNameSpaceManager* nameSpaceManager = GetNameSpaceManager();
  if (nameSpaceManager) {
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
      js::IsObjectInContextCompartment(wrapper, aCx) ?
        WebIDLGlobalNameHash::UnresolvedNamesOnly :
        WebIDLGlobalNameHash::AllNames;
    if (!WebIDLGlobalNameHash::GetNames(aCx, wrapper, nameType, aNames)) {
      aRv.NoteJSContextException(aCx);
    }

    for (auto i = nameSpaceManager->GlobalNameIter(); !i.Done(); i.Next()) {
      const GlobalNameMapEntry* entry = i.Get();
      if (nsWindowSH::NameStructEnabled(aCx, this, entry->mKey,
                                        entry->mGlobalName)) {
        // Just append all of these; even if they get deleted our resolve hook
        // just goes ahead and recreates them.
        JSString* str = JS_AtomizeUCStringN(aCx,
                                            entry->mKey.BeginReading(),
                                            entry->mKey.Length());
        if (!str || !aNames.append(NON_INTEGER_ATOM_TO_JSID(str))) {
          aRv.NoteJSContextException(aCx);
          return;
        }
      }
    }
  }
}

/* static */ bool
nsGlobalWindowInner::IsPrivilegedChromeWindow(JSContext* aCx, JSObject* aObj)
{
  // For now, have to deal with XPConnect objects here.
  return xpc::WindowOrNull(aObj)->IsChromeWindow() &&
         nsContentUtils::ObjectPrincipal(aObj) == nsContentUtils::GetSystemPrincipal();
}

/* static */ bool
nsGlobalWindowInner::IsRequestIdleCallbackEnabled(JSContext* aCx, JSObject* aObj)
{
  // The requestIdleCallback should always be enabled for system code.
  return nsContentUtils::RequestIdleCallbackEnabled() ||
         nsContentUtils::IsSystemCaller(aCx);
}

nsIDOMOfflineResourceList*
nsGlobalWindowInner::GetApplicationCache(ErrorResult& aError)
{
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

already_AddRefed<nsIDOMOfflineResourceList>
nsGlobalWindowInner::GetApplicationCache()
{
  ErrorResult dummy;
  nsCOMPtr<nsIDOMOfflineResourceList> applicationCache =
    GetApplicationCache(dummy);
  dummy.SuppressException();
  return applicationCache.forget();
}

Crypto*
nsGlobalWindowInner::GetCrypto(ErrorResult& aError)
{
  if (!mCrypto) {
    mCrypto = new Crypto();
    mCrypto->Init(this);
  }
  return mCrypto;
}

mozilla::dom::U2F*
nsGlobalWindowInner::GetU2f(ErrorResult& aError)
{
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

nsIControllers*
nsGlobalWindowInner::GetControllers(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetControllersOuter, (aError), aError, nullptr);
}

nsresult
nsGlobalWindowInner::GetControllers(nsIControllers** aResult)
{
  ErrorResult rv;
  nsCOMPtr<nsIControllers> controllers = GetControllers(rv);
  controllers.forget(aResult);

  return rv.StealNSResult();
}

nsPIDOMWindowOuter*
nsGlobalWindowInner::GetOpenerWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOpenerWindowOuter, (), aError, nullptr);
}

void
nsGlobalWindowInner::GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                               ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindowOuter> opener = GetOpenerWindow(aError);
  if (aError.Failed() || !opener) {
    aRetval.setNull();
    return;
  }

  aError = nsContentUtils::WrapNative(aCx, opener, aRetval);
}

void
nsGlobalWindowInner::SetOpener(JSContext* aCx, JS::Handle<JS::Value> aOpener,
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

    win = globalWindow;
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
nsGlobalWindowInner::GetStatus(nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetStatusOuter, (aStatus), aError, );
}

void
nsGlobalWindowInner::SetStatus(const nsAString& aStatus, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetStatusOuter, (aStatus), aError, );
}

void
nsGlobalWindowInner::GetName(nsAString& aName, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetNameOuter, (aName), aError, );
}

void
nsGlobalWindowInner::SetName(const nsAString& aName, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetNameOuter, (aName, aError), aError, );
}

int32_t
nsGlobalWindowInner::GetInnerWidth(CallerType aCallerType, ErrorResult& aError)
{
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindowInner::GetInnerWidth(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerWidthOuter, (aError), aError, 0);
}

void
nsGlobalWindowInner::GetInnerWidth(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aValue,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetInnerWidth, aValue,
                            aCallerType, aError);
}

nsresult
nsGlobalWindowInner::GetInnerWidth(int32_t* aInnerWidth)
{
  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerWidth = GetInnerWidth(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindowInner::SetInnerWidth(int32_t aInnerWidth, CallerType aCallerType,
                                   ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerWidthOuter,
                            (aInnerWidth, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetInnerWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetInnerWidth,
                            aValue, "innerWidth", aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetInnerHeight(CallerType aCallerType, ErrorResult& aError)
{
  // We ignore aCallerType; we only have that argument because some other things
  // called by GetReplaceableWindowCoord need it.  If this ever changes, fix
  //   nsresult nsGlobalWindowInner::GetInnerHeight(int32_t* aInnerWidth)
  // to actually take a useful CallerType and pass it in here.
  FORWARD_TO_OUTER_OR_THROW(GetInnerHeightOuter, (aError), aError, 0);
}

void
nsGlobalWindowInner::GetInnerHeight(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aValue,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetInnerHeight, aValue,
                            aCallerType, aError);
}

nsresult
nsGlobalWindowInner::GetInnerHeight(int32_t* aInnerHeight)
{
  ErrorResult rv;
  // Callee doesn't care about the caller type, but play it safe.
  *aInnerHeight = GetInnerHeight(CallerType::NonSystem, rv);

  return rv.StealNSResult();
}

void
nsGlobalWindowInner::SetInnerHeight(int32_t aInnerHeight,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetInnerHeightOuter,
                            (aInnerHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetInnerHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                    CallerType aCallerType, ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetInnerHeight,
                            aValue, "innerHeight", aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetOuterWidth(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterWidthOuter, (aCallerType, aError),
                            aError, 0);
}

void
nsGlobalWindowInner::GetOuterWidth(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aValue,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetOuterWidth, aValue,
                            aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetOuterHeight(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetOuterHeightOuter, (aCallerType, aError),
                            aError, 0);
}

void
nsGlobalWindowInner::GetOuterHeight(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aValue,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetOuterHeight, aValue,
                            aCallerType, aError);
}

void
nsGlobalWindowInner::SetOuterWidth(int32_t aOuterWidth,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterWidthOuter,
                            (aOuterWidth, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetOuterWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                   CallerType aCallerType,
                                   ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetOuterWidth,
                            aValue, "outerWidth", aCallerType, aError);
}

void
nsGlobalWindowInner::SetOuterHeight(int32_t aOuterHeight,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetOuterHeightOuter,
                            (aOuterHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetOuterHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                    CallerType aCallerType,
                                    ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetOuterHeight,
                            aValue, "outerHeight", aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetScreenX(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenXOuter, (aCallerType, aError), aError, 0);
}

void
nsGlobalWindowInner::GetScreenX(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aValue,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetScreenX, aValue,
                            aCallerType, aError);
}

float
nsGlobalWindowInner::GetMozInnerScreenX(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenXOuter, (aCallerType), aError, 0);
}

float
nsGlobalWindowInner::GetMozInnerScreenY(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozInnerScreenYOuter, (aCallerType), aError, 0);
}

double
nsGlobalWindowInner::GetDevicePixelRatio(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetDevicePixelRatioOuter, (aCallerType), aError, 0.0);
}

uint64_t
nsGlobalWindowInner::GetMozPaintCount(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetMozPaintCountOuter, (), aError, 0);
}

int32_t
nsGlobalWindowInner::RequestAnimationFrame(FrameRequestCallback& aCallback,
                                           ErrorResult& aError)
{
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
nsGlobalWindowInner::CancelAnimationFrame(int32_t aHandle, ErrorResult& aError)
{
  if (!mDoc) {
    return;
  }

  mDoc->CancelFrameRequestCallback(aHandle);
}

already_AddRefed<MediaQueryList>
nsGlobalWindowInner::MatchMedia(const nsAString& aMediaQueryList,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  // FIXME: This whole forward-to-outer and then get a pres
  // shell/context off the docshell dance is sort of silly; it'd make
  // more sense to forward to the inner, but it's what everyone else
  // (GetSelection, GetScrollXY, etc.) does around here.
  FORWARD_TO_OUTER_OR_THROW(MatchMediaOuter, (aMediaQueryList, aCallerType), aError, nullptr);
}

void
nsGlobalWindowInner::SetScreenX(int32_t aScreenX,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenXOuter,
                            (aScreenX, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetScreenX(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                CallerType aCallerType, ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetScreenX,
                            aValue, "screenX", aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetScreenY(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScreenYOuter, (aCallerType, aError), aError, 0);
}

void
nsGlobalWindowInner::GetScreenY(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aValue,
                                CallerType aCallerType, ErrorResult& aError)
{
  GetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::GetScreenY, aValue,
                            aCallerType, aError);
}

void
nsGlobalWindowInner::SetScreenY(int32_t aScreenY,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetScreenYOuter,
                            (aScreenY, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::SetScreenY(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                CallerType aCallerType,
                                ErrorResult& aError)
{
  SetReplaceableWindowCoord(aCx, &nsGlobalWindowInner::SetScreenY,
                            aValue, "screenY", aCallerType, aError);
}

int32_t
nsGlobalWindowInner::GetScrollMinX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideLeft), aError, 0);
}

int32_t
nsGlobalWindowInner::GetScrollMinY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideTop), aError, 0);
}

int32_t
nsGlobalWindowInner::GetScrollMaxX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideRight), aError, 0);
}

int32_t
nsGlobalWindowInner::GetScrollMaxY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollBoundaryOuter, (eSideBottom), aError, 0);
}

double
nsGlobalWindowInner::GetScrollX(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollXOuter, (), aError, 0);
}

double
nsGlobalWindowInner::GetScrollY(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetScrollYOuter, (), aError, 0);
}

uint32_t
nsGlobalWindowInner::Length()
{
  FORWARD_TO_OUTER(Length, (), 0);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::GetTop(mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetTopOuter, (), aError, nullptr);
}

nsPIDOMWindowOuter*
nsGlobalWindowInner::GetChildWindow(const nsAString& aName)
{
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->GetChildWindow(aName);
  }
  return nullptr;
}

void
nsGlobalWindowInner::RefreshCompartmentPrincipal()
{
  JS_SetCompartmentPrincipals(js::GetObjectCompartment(GetWrapperPreserveColor()),
                              nsJSPrincipals::get(mDoc->NodePrincipal()));
}

already_AddRefed<nsIWidget>
nsGlobalWindowInner::GetMainWidget()
{
  FORWARD_TO_OUTER(GetMainWidget, (), nullptr);
}

nsIWidget*
nsGlobalWindowInner::GetNearestWidget() const
{
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->GetNearestWidget();
  }
  return nullptr;
}

void
nsGlobalWindowInner::SetFullScreen(bool aFullScreen, mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetFullScreenOuter, (aFullScreen, aError), aError, /* void */);
}

bool
nsGlobalWindowInner::GetFullScreen(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFullScreenOuter, (), aError, false);
}

bool
nsGlobalWindowInner::GetFullScreen()
{
  ErrorResult dummy;
  bool fullscreen = GetFullScreen(dummy);
  dummy.SuppressException();
  return fullscreen;
}

void
nsGlobalWindowInner::Dump(const nsAString& aStr)
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
nsGlobalWindowInner::Alert(nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aError)
{
  Alert(EmptyString(), aSubjectPrincipal, aError);
}

void
nsGlobalWindowInner::Alert(const nsAString& aMessage,
                           nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(AlertOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, );
}

bool
nsGlobalWindowInner::Confirm(const nsAString& aMessage,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ConfirmOuter, (aMessage, aSubjectPrincipal, aError),
                            aError, false);
}

already_AddRefed<Promise>
nsGlobalWindowInner::Fetch(const RequestOrUSVString& aInput,
                           const RequestInit& aInit,
                           CallerType aCallerType, ErrorResult& aRv)
{
  return FetchRequest(this, aInput, aInit, aCallerType, aRv);
}

void
nsGlobalWindowInner::Prompt(const nsAString& aMessage, const nsAString& aInitial,
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
nsGlobalWindowInner::Focus(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(FocusOuter, (aError), aError, );
}

nsresult
nsGlobalWindowInner::Focus()
{
  ErrorResult rv;
  Focus(rv);

  return rv.StealNSResult();
}

void
nsGlobalWindowInner::Blur(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(BlurOuter, (), aError, );
}

void
nsGlobalWindowInner::Back(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(BackOuter, (aError), aError, );
}

void
nsGlobalWindowInner::Forward(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ForwardOuter, (aError), aError, );
}

void
nsGlobalWindowInner::Home(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(HomeOuter, (aSubjectPrincipal, aError), aError, );
}

void
nsGlobalWindowInner::Stop(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(StopOuter, (aError), aError, );
}

/* static */
bool
nsGlobalWindowInner::IsWindowPrintEnabled(JSContext*, JSObject*)
{
  static bool called = false;
  static bool printDisabled = false;
  if (!called) {
    called = true;
    Preferences::AddBoolVarCache(&printDisabled, "dom.disable_window_print");
  }
  return !printDisabled;
}

void
nsGlobalWindowInner::Print(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(PrintOuter, (aError), aError, );
}

void
nsGlobalWindowInner::MoveTo(int32_t aXPos, int32_t aYPos,
                            CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveToOuter,
                            (aXPos, aYPos, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::MoveBy(int32_t aXDif, int32_t aYDif,
                            CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(MoveByOuter,
                            (aXDif, aYDif, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::ResizeTo(int32_t aWidth, int32_t aHeight,
                              CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeToOuter,
                            (aWidth, aHeight, aCallerType, aError), aError, );
}

void
nsGlobalWindowInner::ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                              CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(ResizeByOuter,
                            (aWidthDif, aHeightDif, aCallerType, aError),
                            aError, );
}

void
nsGlobalWindowInner::SizeToContent(CallerType aCallerType, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SizeToContentOuter, (aCallerType, aError),
                            aError, );
}

already_AddRefed<nsPIWindowRoot>
nsGlobalWindowInner::GetTopWindowRoot()
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    return nullptr;
  }
  return outer->GetTopWindowRoot();
}

void
nsGlobalWindowInner::Scroll(double aXScroll, double aYScroll)
{
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void
nsGlobalWindowInner::ScrollTo(double aXScroll, double aYScroll)
{
  // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
  auto scrollPos = CSSIntPoint::Truncate(mozilla::ToZeroIfNonfinite(aXScroll),
                                         mozilla::ToZeroIfNonfinite(aYScroll));
  ScrollTo(scrollPos, ScrollOptions());
}

void
nsGlobalWindowInner::ScrollTo(const ScrollToOptions& aOptions)
{
  // When scrolling to a non-zero offset, we need to determine whether that
  // position is within our scrollable range, so we need updated layout
  // information which requires a layout flush, otherwise all we need is to
  // flush frames to be able to access our scrollable frame here.
  FlushType flushType = ((aOptions.mLeft.WasPassed() &&
                          aOptions.mLeft.Value() > 0) ||
                         (aOptions.mTop.WasPassed() &&
                          aOptions.mTop.Value() > 0)) ?
                          FlushType::Layout :
                          FlushType::Frames;
  FlushPendingNotifications(flushType);
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
nsGlobalWindowInner::Scroll(const ScrollToOptions& aOptions)
{
  ScrollTo(aOptions);
}

void
nsGlobalWindowInner::ScrollTo(const CSSIntPoint& aScroll,
                              const ScrollOptions& aOptions)
{
  // When scrolling to a non-zero offset, we need to determine whether that
  // position is within our scrollable range, so we need updated layout
  // information which requires a layout flush, otherwise all we need is to
  // flush frames to be able to access our scrollable frame here.
  FlushType flushType = (aScroll.x || aScroll.y) ?
                          FlushType::Layout :
                          FlushType::Frames;
  FlushPendingNotifications(flushType);
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
nsGlobalWindowInner::ScrollBy(double aXScrollDif, double aYScrollDif)
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
nsGlobalWindowInner::ScrollBy(const ScrollToOptions& aOptions)
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
nsGlobalWindowInner::ScrollByLines(int32_t numLines,
                                   const ScrollOptions& aOptions)
{
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
nsGlobalWindowInner::ScrollByPages(int32_t numPages,
                                   const ScrollOptions& aOptions)
{
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
nsGlobalWindowInner::MozScrollSnap()
{
  FlushPendingNotifications(FlushType::Layout);
  nsIScrollableFrame *sf = GetScrollFrame();
  if (sf) {
    sf->ScrollSnap();
  }
}

void
nsGlobalWindowInner::ClearTimeout(int32_t aHandle)
{
  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
  }
}

void
nsGlobalWindowInner::ClearInterval(int32_t aHandle)
{
  if (aHandle > 0) {
    mTimeoutManager->ClearTimeout(aHandle, Timeout::Reason::eTimeoutOrInterval);
  }
}

void
nsGlobalWindowInner::SetResizable(bool aResizable) const
{
  // nop
}

void
nsGlobalWindowInner::CaptureEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfCaptureEvents);
  }
}

void
nsGlobalWindowInner::ReleaseEvents()
{
  if (mDoc) {
    mDoc->WarnOnceAbout(nsIDocument::eUseOfReleaseEvents);
  }
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::Open(const nsAString& aUrl, const nsAString& aName,
                          const nsAString& aOptions, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(OpenOuter, (aUrl, aName, aOptions, aError), aError,
                            nullptr);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::OpenDialog(JSContext* aCx, const nsAString& aUrl,
                                const nsAString& aName, const nsAString& aOptions,
                                const Sequence<JS::Value>& aExtraArgument,
                                ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(OpenDialogOuter,
                            (aCx, aUrl, aName, aOptions, aExtraArgument, aError),
                            aError, nullptr);
}

already_AddRefed<nsPIDOMWindowOuter>
nsGlobalWindowInner::GetFrames(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFramesOuter, (), aError, nullptr);
}

nsGlobalWindowInner*
nsGlobalWindowInner::CallerInnerWindow()
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
  return nsGlobalWindowInner::Cast(win);
}

void
nsGlobalWindowInner::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
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
nsGlobalWindowInner::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
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

void
nsGlobalWindowInner::Close(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(CloseOuter, (nsContentUtils::IsCallerChrome()), aError, );
}

nsresult
nsGlobalWindowInner::Close()
{
  FORWARD_TO_OUTER(Close, (), NS_ERROR_UNEXPECTED);
}

void
nsGlobalWindowInner::ReallyCloseWindow()
{
  FORWARD_TO_OUTER_VOID(ReallyCloseWindow, ());
}

bool
nsGlobalWindowInner::IsInModalState()
{
  FORWARD_TO_OUTER(IsInModalState, (), false);
}

// static
void
nsGlobalWindowInner::NotifyDOMWindowDestroyed(nsGlobalWindowInner* aWindow)
{
  nsCOMPtr<nsIObserverService> observerService =
    services::GetObserverService();
  if (observerService) {
    observerService->
      NotifyObservers(ToSupports(aWindow),
                      DOM_WINDOW_DESTROYED_TOPIC, nullptr);
  }
}

void
nsGlobalWindowInner::NotifyWindowIDDestroyed(const char* aTopic)
{
  nsCOMPtr<nsIRunnable> runnable =
    new WindowDestroyedEvent(this, mWindowID, aTopic);
  Dispatch(TaskCategory::Other, runnable.forget());
}

// static
void
nsGlobalWindowInner::NotifyDOMWindowFrozen(nsGlobalWindowInner* aWindow)
{
  if (aWindow) {
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
nsGlobalWindowInner::NotifyDOMWindowThawed(nsGlobalWindowInner* aWindow)
{
  if (aWindow) {
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
nsGlobalWindowInner::GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey)
{
  JS::Rooted<JSObject*> handler(RootingCx());
  if (mCachedXBLPrototypeHandlers) {
    mCachedXBLPrototypeHandlers->Get(aKey, handler.address());
  }
  return handler;
}

void
nsGlobalWindowInner::CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                                              JS::Handle<JSObject*> aHandler)
{
  if (!mCachedXBLPrototypeHandlers) {
    mCachedXBLPrototypeHandlers = MakeUnique<XBLPrototypeHandlerTable>();
    PreserveWrapper(ToSupports(this));
  }

  mCachedXBLPrototypeHandlers->Put(aKey, aHandler);
}

Element*
nsGlobalWindowInner::GetFrameElement(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetFrameElementOuter, (aSubjectPrincipal), aError,
                            nullptr);
}

Element*
nsGlobalWindowInner::GetRealFrameElement(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetRealFrameElementOuter, (), aError, nullptr);
}

/**
 * nsIGlobalWindow::GetFrameElement (when called from C++) is just a wrapper
 * around GetRealFrameElement.
 */
already_AddRefed<nsIDOMElement>
nsGlobalWindowInner::GetFrameElement()
{
  ErrorResult dummy;
  nsCOMPtr<nsIDOMElement> frameElement =
    do_QueryInterface(GetRealFrameElement(dummy));
  dummy.SuppressException();
  return frameElement.forget();
}

/* static */ bool
nsGlobalWindowInner::TokenizeDialogOptions(nsAString& aToken,
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
nsGlobalWindowInner::ConvertDialogOptions(const nsAString& aOptions,
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

nsresult
nsGlobalWindowInner::UpdateCommands(const nsAString& anAction,
                                    nsISelection* aSel,
                                    int16_t aReason)
{
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->UpdateCommands(anAction, aSel, aReason);
  }
  return NS_OK;
}

Selection*
nsGlobalWindowInner::GetSelection(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetSelectionOuter, (), aError, nullptr);
}

bool
nsGlobalWindowInner::Find(const nsAString& aString, bool aCaseSensitive,
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
nsGlobalWindowInner::GetOrigin(nsAString& aOrigin)
{
  nsContentUtils::GetUTFOrigin(GetPrincipal(), aOrigin);
}

void
nsGlobalWindowInner::Atob(const nsAString& aAsciiBase64String,
                          nsAString& aBinaryData, ErrorResult& aError)
{
  aError = nsContentUtils::Atob(aAsciiBase64String, aBinaryData);
}

void
nsGlobalWindowInner::Btoa(const nsAString& aBinaryData,
                          nsAString& aAsciiBase64String, ErrorResult& aError)
{
  aError = nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

//*****************************************************************************
// nsGlobalWindowInner::nsIDOMEventTarget
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindowInner::GetOwnerGlobalForBindings()
{
  return nsPIDOMWindowOuter::GetFromCurrentInner(this);
}

NS_IMETHODIMP
nsGlobalWindowInner::RemoveEventListener(const nsAString& aType,
                                         nsIDOMEventListener* aListener,
                                         bool aUseCapture)
{
  if (RefPtr<EventListenerManager> elm = GetExistingListenerManager()) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsGlobalWindowInner)

NS_IMETHODIMP
nsGlobalWindowInner::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
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
  RefPtr<nsPresContext> presContext;
  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(ToSupports(this), nullptr,
                                                  aEvent, presContext, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsGlobalWindowInner::AddEventListener(const nsAString& aType,
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
nsGlobalWindowInner::AddEventListener(const nsAString& aType,
                                      EventListener* aListener,
                                      const AddEventListenerOptionsOrBoolean& aOptions,
                                      const Nullable<bool>& aWantsUntrusted,
                                      ErrorResult& aRv)
{
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
nsGlobalWindowInner::AddSystemEventListener(const nsAString& aType,
                                            nsIDOMEventListener *aListener,
                                            bool aUseCapture,
                                            bool aWantsUntrusted,
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

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

EventListenerManager*
nsGlobalWindowInner::GetOrCreateListenerManager()
{
  if (!mListenerManager) {
    mListenerManager =
      new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager*
nsGlobalWindowInner::GetExistingListenerManager() const
{
  return mListenerManager;
}

nsIScriptContext*
nsGlobalWindowInner::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = NS_ERROR_UNEXPECTED;
  FORWARD_TO_OUTER(GetContextForEventHandlers, (aRv), nullptr);
}

//*****************************************************************************
// nsGlobalWindowInner::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindowOuter*
nsGlobalWindowInner::GetPrivateRoot()
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    NS_WARNING("No outer window available!");
    return nullptr;
  }
  return outer->GetPrivateRoot();
}

Location*
nsGlobalWindowInner::GetLocation()
{
  if (!mLocation) {
    mLocation = new dom::Location(this, GetDocShell());
  }

  return mLocation;
}

bool
nsGlobalWindowInner::IsTopLevelWindowActive()
{
  if (GetOuterWindowInternal()) {
    return GetOuterWindowInternal()->IsTopLevelWindowActive();
  }
  return false;
}

void
nsGlobalWindowInner::MaybeUpdateTouchState()
{
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
nsGlobalWindowInner::EnableGamepadUpdates()
{
  if (mHasGamepad) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->AddListener(this);
    }
  }
}

void
nsGlobalWindowInner::DisableGamepadUpdates()
{
  if (mHasGamepad) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    if (gamepadManager) {
      gamepadManager->RemoveListener(this);
    }
  }
}

void
nsGlobalWindowInner::EnableVRUpdates()
{
  if (mHasVREvents && !mVREventObserver) {
    mVREventObserver = new VREventObserver(this);
  }
}

void
nsGlobalWindowInner::DisableVRUpdates()
{
  if (mVREventObserver) {
    mVREventObserver->DisconnectFromOwner();
    mVREventObserver = nullptr;
  }
}

void
nsGlobalWindowInner::ResetVRTelemetry(bool aUpdate)
{
  if (mVREventObserver) {
    mVREventObserver->UpdateSpentTimeIn2DTelemetry(aUpdate);
  }
}

static bool ShouldShowFocusRingIfFocusedByMouse(nsIContent* aNode)
{
  if (!aNode) {
    return true;
  }
  return !nsContentUtils::ContentIsLink(aNode) &&
    !aNode->IsAnyOfHTMLElements(nsGkAtoms::video, nsGkAtoms::audio);
}

void
nsGlobalWindowInner::SetFocusedNode(nsIContent* aNode,
                                    uint32_t aFocusMethod,
                                    bool aNeedsFocus)
{
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
nsGlobalWindowInner::GetFocusMethod()
{
  return mFocusMethod;
}

bool
nsGlobalWindowInner::ShouldShowFocusRing()
{
  if (mShowFocusRingForContent || mFocusByKeyOccurred) {
    return true;
  }

  nsCOMPtr<nsPIWindowRoot> root = GetTopWindowRoot();
  return root ? root->ShowFocusRings() : false;
}

bool
nsGlobalWindowInner::TakeFocus(bool aFocus, uint32_t aFocusMethod)
{
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
nsGlobalWindowInner::SetReadyForFocus()
{
  bool oldNeedsFocus = mNeedsFocus;
  mNeedsFocus = false;

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->WindowShown(GetOuterWindow(), oldNeedsFocus);
  }
}

void
nsGlobalWindowInner::PageHidden()
{
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
  HashchangeCallback(const nsAString& aOldURL,
                     const nsAString& aNewURL,
                     nsGlobalWindowInner* aWindow)
    : mozilla::Runnable("HashchangeCallback")
    , mWindow(aWindow)
  {
    MOZ_ASSERT(mWindow);
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
  RefPtr<nsGlobalWindowInner> mWindow;
};

nsresult
nsGlobalWindowInner::DispatchAsyncHashchange(nsIURI *aOldURI, nsIURI *aNewURI)
{
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
  return Dispatch(TaskCategory::Other, callback.forget());
}

nsresult
nsGlobalWindowInner::FireHashchange(const nsAString &aOldURL,
                                    const nsAString &aNewURL)
{
  // Don't do anything if the window is frozen.
  if (IsFrozen()) {
    return NS_OK;
  }

  // Get a presentation shell for use in creating the hashchange event.
  NS_ENSURE_STATE(IsCurrentInnerWindow());

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
nsGlobalWindowInner::DispatchSyncPopState()
{
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
  result = jsapi.Init(this);
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
static nsCanvasFrame*
FindCanvasFrame(nsIFrame* aFrame)
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
nsGlobalWindowInner::UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent)
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
nsGlobalWindowInner::GetComputedStyle(Element& aElt, const nsAString& aPseudoElt,
                                      ErrorResult& aError)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, false, aError);
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindowInner::GetDefaultComputedStyle(Element& aElt,
                                             const nsAString& aPseudoElt,
                                             ErrorResult& aError)
{
  return GetComputedStyleHelper(aElt, aPseudoElt, true, aError);
}

nsresult
nsGlobalWindowInner::GetComputedStyleHelper(nsIDOMElement* aElt,
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

  return rv.StealNSResult();
}

already_AddRefed<nsICSSDeclaration>
nsGlobalWindowInner::GetComputedStyleHelper(Element& aElt,
                                            const nsAString& aPseudoElt,
                                            bool aDefaultStylesOnly,
                                            ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetComputedStyleHelperOuter,
                            (aElt, aPseudoElt, aDefaultStylesOnly),
                            aError, nullptr);
}

Storage*
nsGlobalWindowInner::GetSessionStorage(ErrorResult& aError)
{
  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell* docShell = GetDocShell();

  if (!principal || !docShell || !Storage::StoragePrefIsEnabled()) {
    return nullptr;
  }

  if (mSessionStorage) {
    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
            ("nsGlobalWindowInner %p has %p sessionStorage", this, mSessionStorage.get()));
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

    nsresult rv;

    nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(docShell, &rv);
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
      return nullptr;
    }

    nsCOMPtr<nsIDOMStorage> storage;
    aError = storageManager->CreateStorage(this, principal, documentURI,
                                           IsPrivateBrowsing(),
                                           getter_AddRefs(storage));
    if (aError.Failed()) {
      return nullptr;
    }

    mSessionStorage = static_cast<Storage*>(storage.get());
    MOZ_ASSERT(mSessionStorage);

    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
            ("nsGlobalWindowInner %p tried to get a new sessionStorage %p", this, mSessionStorage.get()));

    if (!mSessionStorage) {
      aError.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
    }
  }

  MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
          ("nsGlobalWindowInner %p returns %p sessionStorage", this, mSessionStorage.get()));

  return mSessionStorage;
}

Storage*
nsGlobalWindowInner::GetLocalStorage(ErrorResult& aError)
{
  if (!Storage::StoragePrefIsEnabled()) {
    return nullptr;
  }

  if (!mLocalStorage) {
    if (nsContentUtils::StorageAllowedForWindow(this) ==
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
    aError = storageManager->CreateStorage(this, principal, documentURI,
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
nsGlobalWindowInner::GetIndexedDB(ErrorResult& aError)
{
  if (!mIndexedDB) {
    // This may keep mIndexedDB null without setting an error.
    aError = IDBFactory::CreateForWindow(this,
                                         getter_AddRefs(mIndexedDB));
  }

  return mIndexedDB;
}

void
nsGlobalWindowInner::AddPendingPromise(mozilla::dom::Promise* aPromise)
{
  mPendingPromises.AppendElement(aPromise);
}

void
nsGlobalWindowInner::RemovePendingPromise(mozilla::dom::Promise* aPromise)
{
  DebugOnly<bool> foundIt = mPendingPromises.RemoveElement(aPromise);
  MOZ_ASSERT(foundIt, "tried to remove a non-existent element from mPendingPromises");
}

//*****************************************************************************
// nsGlobalWindowInner::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindowInner::GetInterface(const nsIID & aIID, void **aSink)
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  NS_ENSURE_TRUE(outer, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = outer->GetInterfaceInternal(aIID, aSink);
  if (rv == NS_ERROR_NO_INTERFACE) {
    return QueryInterface(aIID, aSink);
  }
  return rv;
}

void
nsGlobalWindowInner::GetInterface(JSContext* aCx, nsIJSID* aIID,
                                  JS::MutableHandle<JS::Value> aRetval,
                                  ErrorResult& aError)
{
  dom::GetInterface(aCx, this, aIID, aRetval, aError);
}

already_AddRefed<CacheStorage>
nsGlobalWindowInner::GetCaches(ErrorResult& aRv)
{
  if (!mCacheStorage) {
    bool forceTrustedOrigin =
      GetOuterWindow()->GetServiceWorkersTestingEnabled();

    nsContentUtils::StorageAccess access =
      nsContentUtils::StorageAllowedForWindow(this);

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
nsGlobalWindowInner::FireOfflineStatusEventIfChanged()
{
  if (!IsCurrentInnerWindow())
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
                             nsGlobalWindowInner* aIdleWindow)
    : mozilla::Runnable("NotifyIdleObserverRunnable")
    , mIdleObserver(aIdleObserver)
    , mTimeInS(aTimeInS)
    , mIdleWindow(aIdleWindow)
    , mCallOnidle(aCallOnidle)
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
  RefPtr<nsGlobalWindowInner> mIdleWindow;

  // If false then call on active
  bool mCallOnidle;
};

void
nsGlobalWindowInner::NotifyIdleObserver(IdleObserverHolder* aIdleObserverHolder,
                                        bool aCallOnidle)
{
  MOZ_ASSERT(aIdleObserverHolder);
  aIdleObserverHolder->mPrevNotificationIdle = aCallOnidle;

  nsCOMPtr<nsIRunnable> caller =
    new NotifyIdleObserverRunnable(aIdleObserverHolder->mIdleObserver,
                                   aIdleObserverHolder->mTimeInS,
                                   aCallOnidle, this);
  if (NS_FAILED(Dispatch(TaskCategory::Other, caller.forget()))) {
    NS_WARNING("Failed to dispatch thread for idle observer notification.");
  }
}

bool
nsGlobalWindowInner::ContainsIdleObserver(nsIIdleObserver* aIdleObserver, uint32_t aTimeInS)
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
  RefPtr<nsGlobalWindowInner> idleWindow =
    static_cast<nsGlobalWindowInner*>(aClosure);
  MOZ_ASSERT(idleWindow, "Idle window has not been instantiated.");
  idleWindow->HandleIdleActiveEvent();
}

void
IdleObserverTimerCallback(nsITimer* aTimer, void* aClosure)
{
  RefPtr<nsGlobalWindowInner> idleWindow =
    static_cast<nsGlobalWindowInner*>(aClosure);
  MOZ_ASSERT(idleWindow, "Idle window has not been instantiated.");
  idleWindow->HandleIdleObserverCallback();
}

void
nsGlobalWindowInner::HandleIdleObserverCallback()
{
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
nsGlobalWindowInner::ScheduleNextIdleObserverCallback()
{
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
  rv = mIdleTimer->InitWithNamedFuncCallback(
    IdleObserverTimerCallback,
    this,
    callbackTimeMS,
    nsITimer::TYPE_ONE_SHOT,
    "nsGlobalWindowInner::ScheduleNextIdleObserverCallback");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

uint32_t
nsGlobalWindowInner::GetFuzzTimeMS()
{
  if (gIdleObserversAPIFuzzTimeDisabled) {
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
nsGlobalWindowInner::ScheduleActiveTimerCallback()
{
  if (!mAddActiveEventFuzzTime) {
    return HandleIdleActiveEvent();
  }

  MOZ_ASSERT(mIdleTimer);
  mIdleTimer->Cancel();

  uint32_t fuzzFactorInMS = GetFuzzTimeMS();
  nsresult rv = mIdleTimer->InitWithNamedFuncCallback(
    IdleActiveTimerCallback,
    this,
    fuzzFactorInMS,
    nsITimer::TYPE_ONE_SHOT,
    "nsGlobalWindowInner::ScheduleActiveTimerCallback");
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsGlobalWindowInner::HandleIdleActiveEvent()
{
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

nsGlobalWindowInner::SlowScriptResponse
nsGlobalWindowInner::ShowSlowScriptDialog(const nsString& aAddonId)
{
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
                                       filename.get(),
                                       aAddonId);
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
      SpinEventLoopUntil([&]() { return monitor->IsDebuggerStartupComplete(); });
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

  bool failed = false;
  auto getString = [&] (const char* name,
                        nsContentUtils::PropertiesFile propFile = nsContentUtils::eDOM_PROPERTIES) {
    nsAutoString result;
    nsresult rv = nsContentUtils::GetLocalizedString(
      propFile, name, result);

    // GetStringFromName can return NS_OK and still give nullptr string
    failed = failed || NS_FAILED(rv) || result.IsEmpty();
    return Move(result);
  };

  bool isAddonScript = !aAddonId.IsEmpty();
  bool showDebugButton = debugCallback && !isAddonScript;

  // Get localizable strings

  nsAutoString title, checkboxMsg, debugButton, msg;
  if (isAddonScript) {
    title = getString("KillAddonScriptTitle");
    checkboxMsg = getString("KillAddonScriptGlobalMessage");

    auto appName = getString("brandShortName", nsContentUtils::eBRAND_PROPERTIES);

    nsCOMPtr<nsIAddonPolicyService> aps = do_GetService("@mozilla.org/addons/policy-service;1");
    nsString addonName;
    if (!aps || NS_FAILED(aps->GetExtensionName(aAddonId, addonName))) {
      addonName = aAddonId;
    }

    const char16_t* params[] = {addonName.get(), appName.get()};
    rv = nsContentUtils::FormatLocalizedString(
        nsContentUtils::eDOM_PROPERTIES, "KillAddonScriptMessage",
        params, msg);

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
    const char16_t *formatParams[] = { filenameUTF16.get() };
    rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "KillScriptLocation",
                                               formatParams,
                                               scriptLocation);

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
  int32_t buttonPressed = 0; // In case the user exits dialog by clicking X.
  {
    // Null out the operation callback while we're re-entering JS here.
    AutoDisableJSInterruptCallback disabler(cx);

    // Open the dialog.
    rv = prompt->ConfirmEx(title.get(), msg.get(), buttonFlags,
                           waitButton.get(), stopButton.get(),
                           debugButton.get(), checkboxMsg.get(),
                           &checkboxValue, &buttonPressed);
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

  JS_ClearPendingException(cx);

  if (checkboxValue && isAddonScript)
    return KillScriptGlobal;
  return KillSlowScript;
}

uint32_t
nsGlobalWindowInner::FindInsertionIndex(IdleObserverHolder* aIdleObserver)
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
nsGlobalWindowInner::RegisterIdleObserver(nsIIdleObserver* aIdleObserver)
{
  nsresult rv;
  if (mIdleObservers.IsEmpty()) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mIdleService->AddIdleObserver(mObserver, MIN_IDLE_NOTIFICATION_TIME_S);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mIdleTimer) {
      mIdleTimer = NS_NewTimer();
      NS_ENSURE_TRUE(mIdleTimer, NS_ERROR_OUT_OF_MEMORY);
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
nsGlobalWindowInner::FindIndexOfElementToRemove(nsIIdleObserver* aIdleObserver,
                                                int32_t* aRemoveElementIndex)
{
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
nsGlobalWindowInner::UnregisterIdleObserver(nsIIdleObserver* aIdleObserver)
{
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
nsGlobalWindowInner::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData)
{
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
      ScheduleActiveTimerCallback();
    }
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

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    MOZ_ASSERT(!NS_strcmp(aData, u"intl.accept_languages"));

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
    if (!IsCurrentInnerWindow()) {
      return NS_OK;
    }

    RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
    event->InitEvent(NS_LITERAL_STRING("languagechange"), false, false);
    event->SetTrusted(true);

    bool dummy;
    return DispatchEvent(event, &dummy);
  }

  NS_WARNING("unrecognized topic in nsGlobalWindowInner::Observe");
  return NS_ERROR_FAILURE;
}

void
nsGlobalWindowInner::ObserveStorageNotification(StorageEvent* aEvent,
                                                const char16_t* aStorageType,
                                                bool aPrivateBrowsing)
{
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

  nsIPrincipal *principal = GetPrincipal();
  if (!principal) {
    return;
  }

  bool fireMozStorageChanged = false;
  nsAutoString eventType;
  eventType.AssignLiteral("storage");

  if (!NS_strcmp(aStorageType, u"sessionStorage")) {
    nsCOMPtr<nsIDOMStorage> changingStorage = aEvent->GetStorageArea();
    MOZ_ASSERT(changingStorage);

    bool check = false;

    nsCOMPtr<nsIDOMStorageManager> storageManager = do_QueryInterface(GetDocShell());
    if (storageManager) {
      nsresult rv = storageManager->CheckStorage(principal, changingStorage,
                                                 &check);
      if (NS_FAILED(rv)) {
        return;
      }
    }

    if (!check) {
      // This storage event is not coming from our storage or is coming
      // from a different docshell, i.e. it is a clone, ignore this event.
      return;
    }

    MOZ_LOG(gDOMLeakPRLogInner, LogLevel::Debug,
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
                                                        principal));

    fireMozStorageChanged = mLocalStorage == aEvent->GetStorageArea();

    if (fireMozStorageChanged) {
      eventType.AssignLiteral("MozLocalStorageChanged");
    }
  }

  // Clone the storage event included in the observer notification. We want
  // to dispatch clones rather than the original event.
  IgnoredErrorResult error;
  RefPtr<StorageEvent> clonedEvent =
    CloneStorageEvent(eventType, aEvent, error);
  if (error.Failed()) {
    return;
  }

  clonedEvent->SetTrusted(true);

  if (fireMozStorageChanged) {
    WidgetEvent* internalEvent = clonedEvent->WidgetEventPtr();
    internalEvent->mFlags.mOnlyChromeDispatch = true;
  }

  bool defaultActionEnabled;
  DispatchEvent(clonedEvent, &defaultActionEnabled);
}

already_AddRefed<StorageEvent>
nsGlobalWindowInner::CloneStorageEvent(const nsAString& aType,
                                       const RefPtr<StorageEvent>& aEvent,
                                       ErrorResult& aRv)
{
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

    MOZ_ASSERT(storage->Type() == Storage::eLocalStorage);
    RefPtr<LocalStorage> localStorage =
      static_cast<LocalStorage*>(storage.get());

    // We must apply the current change to the 'local' localStorage.
    localStorage->ApplyEvent(aEvent);
  } else if (storageArea->Type() == Storage::eSessionStorage) {
    storage = GetSessionStorage(aRv);
  } else {
    MOZ_ASSERT(storageArea->Type() == Storage::eLocalStorage);
    storage = GetLocalStorage(aRv);
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
nsGlobalWindowInner::Suspend()
{
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
  CallOnChildren(&nsGlobalWindowInner::Suspend);

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

  mozilla::dom::workers::SuspendWorkersForWindow(this);

  SuspendIdleRequests();

  mTimeoutManager->Suspend();

  // Suspend all of the AudioContexts for this window
  for (uint32_t i = 0; i < mAudioContexts.Length(); ++i) {
    ErrorResult dummy;
    RefPtr<Promise> d = mAudioContexts[i]->Suspend(dummy);
  }
}

void
nsGlobalWindowInner::Resume()
{
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
  CallOnChildren(&nsGlobalWindowInner::Resume);

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
  mozilla::dom::workers::ResumeWorkersForWindow(this);
}

bool
nsGlobalWindowInner::IsSuspended() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mSuspendDepth != 0;
}

void
nsGlobalWindowInner::Freeze()
{
  MOZ_ASSERT(NS_IsMainThread());
  Suspend();
  FreezeInternal();
}

void
nsGlobalWindowInner::FreezeInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  CallOnChildren(&nsGlobalWindowInner::FreezeInternal);

  mFreezeDepth += 1;
  MOZ_ASSERT(mSuspendDepth >= mFreezeDepth);
  if (mFreezeDepth != 1) {
    return;
  }

  mozilla::dom::workers::FreezeWorkersForWindow(this);

  mTimeoutManager->Freeze();
  if (mClientSource) {
    mClientSource->Freeze();
  }

  NotifyDOMWindowFrozen(this);
}

void
nsGlobalWindowInner::Thaw()
{
  MOZ_ASSERT(NS_IsMainThread());
  ThawInternal();
  Resume();
}

void
nsGlobalWindowInner::ThawInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(IsSuspended());

  CallOnChildren(&nsGlobalWindowInner::ThawInternal);

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

  mozilla::dom::workers::ThawWorkersForWindow(this);

  NotifyDOMWindowThawed(this);
}

bool
nsGlobalWindowInner::IsFrozen() const
{
  MOZ_ASSERT(NS_IsMainThread());
  bool frozen = mFreezeDepth != 0;
  MOZ_ASSERT_IF(frozen, IsSuspended());
  return frozen;
}

void
nsGlobalWindowInner::SyncStateFromParentWindow()
{
  // This method should only be called on an inner window that has been
  // assigned to an outer window already.
  MOZ_ASSERT(IsCurrentInnerWindow());
  nsPIDOMWindowOuter* outer = GetOuterWindow();
  MOZ_ASSERT(outer);

  // Attempt to find our parent windows.
  nsCOMPtr<Element> frame = outer->GetFrameElementInternal();
  nsPIDOMWindowOuter* parentOuter = frame ? frame->OwnerDoc()->GetWindow()
                                          : nullptr;
  nsGlobalWindowInner* parentInner =
    parentOuter ? nsGlobalWindowInner::Cast(parentOuter->GetCurrentInnerWindow())
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

template<typename Method, typename... Args>
nsGlobalWindowInner::CallState
nsGlobalWindowInner::CallOnChildren(Method aMethod, Args& ...aArgs)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsCurrentInnerWindow());

  CallState state = CallState::Continue;

  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return state;
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

Maybe<ClientInfo>
nsGlobalWindowInner::GetClientInfo() const
{
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ClientInfo> clientInfo;
  if (mClientSource) {
    clientInfo.emplace(mClientSource->Info());
  }
  return Move(clientInfo);
}

Maybe<ClientState>
nsGlobalWindowInner::GetClientState() const
{
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ClientState> clientState;
  if (mClientSource) {
    ClientState state;
    nsresult rv = mClientSource->SnapshotState(&state);
    if (NS_SUCCEEDED(rv)) {
      clientState.emplace(state);
    }
  }
  return Move(clientState);
}

Maybe<ServiceWorkerDescriptor>
nsGlobalWindowInner::GetController() const
{
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<ServiceWorkerDescriptor> controller;
  if (mClientSource) {
    controller = mClientSource->GetController();
  }
  return Move(controller);
}

nsresult
nsGlobalWindowInner::FireDelayedDOMEvents()
{
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

nsPIDOMWindowOuter*
nsGlobalWindowInner::GetParentInternal()
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  if (!outer) {
    // No outer window available!
    return nullptr;
  }
  return outer->GetParentInternal();
}

//*****************************************************************************
// nsGlobalWindowInner: Timeout Functions
//*****************************************************************************

nsGlobalWindowInner*
nsGlobalWindowInner::InnerForSetTimeoutOrInterval(ErrorResult& aError)
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  nsGlobalWindowInner* currentInner = outer ? outer->GetCurrentInnerWindowInternal() : this;

  // If forwardTo is not the window with an active document then we want the
  // call to setTimeout/Interval to be a noop, so return null but don't set an
  // error.
  return HasActiveDocument() ? currentInner : nullptr;
}

int32_t
nsGlobalWindowInner::SetTimeout(JSContext* aCx, Function& aFunction,
                                int32_t aTimeout,
                                const Sequence<JS::Value>& aArguments,
                                ErrorResult& aError)
{
  return SetTimeoutOrInterval(aCx, aFunction, aTimeout, aArguments, false,
                              aError);
}

int32_t
nsGlobalWindowInner::SetTimeout(JSContext* aCx, const nsAString& aHandler,
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
nsGlobalWindowInner::SetInterval(JSContext* aCx, Function& aFunction,
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
nsGlobalWindowInner::SetInterval(JSContext* aCx, const nsAString& aHandler,
                                 const Optional<int32_t>& aTimeout,
                                 const Sequence<JS::Value>& /* unused */,
                                 ErrorResult& aError)
{
  int32_t timeout;
  bool isInterval = IsInterval(aTimeout, timeout);
  return SetTimeoutOrInterval(aCx, aHandler, timeout, isInterval, aError);
}

int32_t
nsGlobalWindowInner::SetTimeoutOrInterval(JSContext *aCx, Function& aFunction,
                                          int32_t aTimeout,
                                          const Sequence<JS::Value>& aArguments,
                                          bool aIsInterval, ErrorResult& aError)
{
  nsGlobalWindowInner* inner = InnerForSetTimeoutOrInterval(aError);
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
nsGlobalWindowInner::SetTimeoutOrInterval(JSContext* aCx, const nsAString& aHandler,
                                          int32_t aTimeout, bool aIsInterval,
                                          ErrorResult& aError)
{
  nsGlobalWindowInner* inner = InnerForSetTimeoutOrInterval(aError);
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
nsGlobalWindowInner::RunTimeoutHandler(Timeout* aTimeout,
                                       nsIScriptContext* aScx)
{
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
      options.setFileAndLine(filename, lineNo);
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
// nsGlobalWindowInner: Helper Functions
//*****************************************************************************

already_AddRefed<nsIDocShellTreeOwner>
nsGlobalWindowInner::GetTreeOwner()
{
  FORWARD_TO_OUTER(GetTreeOwner, (), nullptr);
}

already_AddRefed<nsIWebBrowserChrome>
nsGlobalWindowInner::GetWebBrowserChrome()
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner = GetTreeOwner();

  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_GetInterface(treeOwner);
  return browserChrome.forget();
}

nsIScrollableFrame *
nsGlobalWindowInner::GetScrollFrame()
{
  FORWARD_TO_OUTER(GetScrollFrame, (), nullptr);
}

bool
nsGlobalWindowInner::IsPrivateBrowsing()
{
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(GetDocShell());
  return loadContext && loadContext->UsePrivateBrowsing();
}

void
nsGlobalWindowInner::FlushPendingNotifications(FlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindowInner::EnableDeviceSensor(uint32_t aType)
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
nsGlobalWindowInner::DisableDeviceSensor(uint32_t aType)
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

#if defined(MOZ_WIDGET_ANDROID)
void
nsGlobalWindowInner::EnableOrientationChangeListener()
{
  // XXX: mDocShell is never set on the inner window?
  nsIDocShell* docShell = nullptr;
  if (!nsContentUtils::ShouldResistFingerprinting(docShell) &&
      !mOrientationChangeObserver) {
    mOrientationChangeObserver =
      MakeUnique<WindowOrientationObserver>(this);
  }
}

void
nsGlobalWindowInner::DisableOrientationChangeListener()
{
  mOrientationChangeObserver = nullptr;
}
#endif

void
nsGlobalWindowInner::SetHasGamepadEventListener(bool aHasGamepad/* = true*/)
{
  mHasGamepad = aHasGamepad;
  if (aHasGamepad) {
    EnableGamepadUpdates();
  }
}


void
nsGlobalWindowInner::EventListenerAdded(nsAtom* aType)
{
  if (aType == nsGkAtoms::onvrdisplayactivate ||
      aType == nsGkAtoms::onvrdisplayconnect ||
      aType == nsGkAtoms::onvrdisplaydeactivate ||
      aType == nsGkAtoms::onvrdisplaydisconnect ||
      aType == nsGkAtoms::onvrdisplaypresentchange) {
    NotifyVREventListenerAdded();
  }

  if (aType == nsGkAtoms::onvrdisplayactivate) {
    mHasVRDisplayActivateEvents = true;
  }

  if (aType == nsGkAtoms::onbeforeunload &&
      mTabChild &&
      (!mDoc || !(mDoc->GetSandboxFlags() & SANDBOXED_MODALS))) {
    mBeforeUnloadListenerCount++;
    MOZ_ASSERT(mBeforeUnloadListenerCount > 0);
    mTabChild->BeforeUnloadAdded();
  }

  // We need to initialize localStorage in order to receive notifications.
  if (aType == nsGkAtoms::onstorage) {
    ErrorResult rv;
    GetLocalStorage(rv);
    rv.SuppressException();
  }
}

void
nsGlobalWindowInner::EventListenerRemoved(nsAtom* aType)
{
  if (aType == nsGkAtoms::onbeforeunload &&
      mTabChild &&
      (!mDoc || !(mDoc->GetSandboxFlags() & SANDBOXED_MODALS))) {
    mBeforeUnloadListenerCount--;
    MOZ_ASSERT(mBeforeUnloadListenerCount >= 0);
    mTabChild->BeforeUnloadRemoved();
  }
}

void
nsGlobalWindowInner::NotifyVREventListenerAdded()
{
  mHasVREvents = true;
  EnableVRUpdates();
}

bool
nsGlobalWindowInner::HasUsedVR() const
{
  // Returns true only if any WebVR API call or related event
  // has been used
  return mHasVREvents;
}

bool
nsGlobalWindowInner::IsVRContentDetected() const
{
  // Returns true only if the content will respond to
  // the VRDisplayActivate event.
  return mHasVRDisplayActivateEvents;
}

bool
nsGlobalWindowInner::IsVRContentPresenting() const
{
  for (const auto& display : mVRDisplays) {
    if (display->IsAnyPresenting(gfx::kVRGroupAll)) {
      return true;
    }
  }
  return false;
}

void
nsGlobalWindowInner::EnableTimeChangeNotifications()
{
  mozilla::time::AddWindowListener(this);
}

void
nsGlobalWindowInner::DisableTimeChangeNotifications()
{
  mozilla::time::RemoveWindowListener(this);
}

void
nsGlobalWindowInner::AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const
{
  aWindowSizes.mDOMOtherSize += aWindowSizes.mState.mMallocSizeOf(this);

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
    if (!mDoc->GetInnerWindow() ||
        mDoc->GetInnerWindow() == this) {
      mDoc->DocAddSizeOfIncludingThis(aWindowSizes);
    }
  }

  if (mNavigator) {
    aWindowSizes.mDOMOtherSize +=
      mNavigator->SizeOfIncludingThis(aWindowSizes.mState.mMallocSizeOf);
  }

  aWindowSizes.mDOMEventTargetsSize +=
    mEventTargetObjects.ShallowSizeOfExcludingThis(
      aWindowSizes.mState.mMallocSizeOf);

  for (auto iter = mEventTargetObjects.ConstIter(); !iter.Done(); iter.Next()) {
    DOMEventTargetHelper* et = iter.Get()->GetKey();
    if (nsCOMPtr<nsISizeOfEventTarget> iSizeOf = do_QueryObject(et)) {
      aWindowSizes.mDOMEventTargetsSize +=
        iSizeOf->SizeOfEventTargetIncludingThis(
          aWindowSizes.mState.mMallocSizeOf);
    }
    if (EventListenerManager* elm = et->GetExistingListenerManager()) {
      aWindowSizes.mDOMEventListenersCount += elm->ListenerCount();
    }
    ++aWindowSizes.mDOMEventTargetsCount;
  }

  if (mPerformance) {
    aWindowSizes.mDOMPerformanceUserEntries =
      mPerformance->SizeOfUserEntries(aWindowSizes.mState.mMallocSizeOf);
    aWindowSizes.mDOMPerformanceResourceEntries =
      mPerformance->SizeOfResourceEntries(aWindowSizes.mState.mMallocSizeOf);
  }

  aWindowSizes.mDOMOtherSize +=
    mPendingPromises.ShallowSizeOfExcludingThis(aWindowSizes.mState.mMallocSizeOf);
}

void
nsGlobalWindowInner::AddGamepad(uint32_t aIndex, Gamepad* aGamepad)
{
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
nsGlobalWindowInner::RemoveGamepad(uint32_t aIndex)
{
  RefPtr<Gamepad> gamepad;
  if (!mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return;
  }
  // Free up the index we were using so it can be reused
  mGamepadIndexSet.Remove(gamepad->Index());
  mGamepads.Remove(aIndex);
}

void
nsGlobalWindowInner::GetGamepads(nsTArray<RefPtr<Gamepad> >& aGamepads)
{
  aGamepads.Clear();

  // navigator.getGamepads() always returns an empty array when
  // privacy.resistFingerprinting is true.
  if (nsContentUtils::ShouldResistFingerprinting()) {
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

already_AddRefed<Gamepad>
nsGlobalWindowInner::GetGamepad(uint32_t aIndex)
{
  RefPtr<Gamepad> gamepad;

  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

void
nsGlobalWindowInner::SetHasSeenGamepadInput(bool aHasSeen)
{
  mHasSeenGamepadInput = aHasSeen;
}

bool
nsGlobalWindowInner::HasSeenGamepadInput()
{
  return mHasSeenGamepadInput;
}

void
nsGlobalWindowInner::SyncGamepadState()
{
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
      gamepadManager->SyncGamepadState(iter.Key(), iter.UserData());
    }
  }
}

void
nsGlobalWindowInner::StopGamepadHaptics()
{
  if (mHasSeenGamepadInput) {
    RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
    gamepadManager->StopHaptics();
  }
}

bool
nsGlobalWindowInner::UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDevices)
{
  VRDisplay::UpdateVRDisplays(mVRDisplays, this);
  aDevices = mVRDisplays;
  return true;
}

void
nsGlobalWindowInner::NotifyActiveVRDisplaysChanged()
{
  if (mNavigator) {
    mNavigator->NotifyActiveVRDisplaysChanged();
  }
}

void
nsGlobalWindowInner::DispatchVRDisplayActivate(uint32_t aDisplayID,
                                               mozilla::dom::VRDisplayEventReason aReason)
{
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
nsGlobalWindowInner::DispatchVRDisplayDeactivate(uint32_t aDisplayID,
                                                 mozilla::dom::VRDisplayEventReason aReason)
{
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
nsGlobalWindowInner::DispatchVRDisplayConnect(uint32_t aDisplayID)
{
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
nsGlobalWindowInner::DispatchVRDisplayDisconnect(uint32_t aDisplayID)
{
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
nsGlobalWindowInner::DispatchVRDisplayPresentChange(uint32_t aDisplayID)
{
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

enum WindowState {
  // These constants need to match the constants in Window.webidl
  STATE_MAXIMIZED = 1,
  STATE_MINIMIZED = 2,
  STATE_NORMAL = 3,
  STATE_FULLSCREEN = 4
};

uint16_t
nsGlobalWindowInner::WindowState()
{
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

bool
nsGlobalWindowInner::IsFullyOccluded()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  return widget && widget->IsFullyOccluded();
}

void
nsGlobalWindowInner::Maximize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Maximized);
  }
}

void
nsGlobalWindowInner::Minimize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Minimized);
  }
}

void
nsGlobalWindowInner::Restore()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    widget->SetSizeMode(nsSizeMode_Normal);
  }
}

void
nsGlobalWindowInner::GetAttention(ErrorResult& aResult)
{
  return GetAttentionWithCycleCount(-1, aResult);
}

void
nsGlobalWindowInner::GetAttentionWithCycleCount(int32_t aCycleCount,
                                                ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  if (widget) {
    aError = widget->GetAttention(aCycleCount);
  }
}

void
nsGlobalWindowInner::BeginWindowMove(Event& aMouseDownEvent, Element* aPanel,
                                     ErrorResult& aError)
{
  nsCOMPtr<nsIWidget> widget;

  // if a panel was supplied, use its widget instead.
#ifdef MOZ_XUL
  if (aPanel) {
    nsIFrame* frame = aPanel->GetPrimaryFrame();
    if (!frame || !frame->IsMenuPopupFrame()) {
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
nsGlobalWindowInner::GetWindowRoot(mozilla::ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetWindowRootOuter, (), aError, nullptr);
}

void
nsGlobalWindowInner::SetCursor(const nsAString& aCursor, ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetCursorOuter, (aCursor, aError), aError, );
}

NS_IMETHODIMP
nsGlobalWindowInner::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  MOZ_RELEASE_ASSERT(IsChromeWindow());

  ErrorResult rv;
  NS_IF_ADDREF(*aBrowserWindow = GetBrowserDOMWindow(rv));
  return rv.StealNSResult();
}

nsIBrowserDOMWindow*
nsGlobalWindowInner::GetBrowserDOMWindow(ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(GetBrowserDOMWindowOuter, (), aError, nullptr);
}

void
nsGlobalWindowInner::SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserWindow,
                                    ErrorResult& aError)
{
  FORWARD_TO_OUTER_OR_THROW(SetBrowserDOMWindowOuter, (aBrowserWindow), aError, );
}

void
nsGlobalWindowInner::NotifyDefaultButtonLoaded(Element& aDefaultButton,
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
nsGlobalWindowInner::GetMessageManager(nsIMessageBroadcaster** aManager)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aManager = GetMessageManager(rv));
  return rv.StealNSResult();
}

nsIMessageBroadcaster*
nsGlobalWindowInner::GetMessageManager(ErrorResult& aError)
{
  MOZ_ASSERT(IsChromeWindow());
  if (!mChromeFields.mMessageManager) {
    nsCOMPtr<nsIMessageBroadcaster> globalMM =
      do_GetService("@mozilla.org/globalmessagemanager;1");
    mChromeFields.mMessageManager =
      new nsFrameMessageManager(nullptr,
                                static_cast<nsFrameMessageManager*>(globalMM.get()),
                                MM_CHROME | MM_BROADCASTER);
  }
  return mChromeFields.mMessageManager;
}

NS_IMETHODIMP
nsGlobalWindowInner::GetGroupMessageManager(const nsAString& aGroup,
                                            nsIMessageBroadcaster** aManager)
{
  MOZ_RELEASE_ASSERT(IsChromeWindow());
  ErrorResult rv;
  NS_IF_ADDREF(*aManager = GetGroupMessageManager(aGroup, rv));
  return rv.StealNSResult();
}

nsIMessageBroadcaster*
nsGlobalWindowInner::GetGroupMessageManager(const nsAString& aGroup,
                                            ErrorResult& aError)
{
  MOZ_ASSERT(IsChromeWindow());

  nsCOMPtr<nsIMessageBroadcaster> messageManager =
    mChromeFields.mGroupMessageManagers.LookupForAdd(aGroup).OrInsert(
      [this, &aError] () {
        nsFrameMessageManager* parent =
          static_cast<nsFrameMessageManager*>(GetMessageManager(aError));

        return new nsFrameMessageManager(nullptr,
                                         parent,
                                         MM_CHROME | MM_BROADCASTER);
      });
  return messageManager;
}

void
nsGlobalWindowInner::InitWasOffline()
{
  mWasOffline = NS_IsOffline();
}

#if defined(MOZ_WIDGET_ANDROID)
int16_t
nsGlobalWindowInner::Orientation(CallerType aCallerType) const
{
  return nsContentUtils::ResistFingerprinting(aCallerType) ?
           0 : WindowOrientationObserver::OrientationAngle();
}
#endif

Console*
nsGlobalWindowInner::GetConsole(ErrorResult& aRv)
{
  if (!mConsole) {
    mConsole = Console::Create(this, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return mConsole;
}

bool
nsGlobalWindowInner::IsSecureContext() const
{
  return JS_GetIsSecureContext(js::GetObjectCompartment(GetWrapperPreserveColor()));
}

already_AddRefed<External>
nsGlobalWindowInner::GetExternal(ErrorResult& aRv)
{
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
nsGlobalWindowInner::GetSidebar(OwningExternalOrWindowProxy& aResult,
                                ErrorResult& aRv)
{
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
nsGlobalWindowInner::ClearDocumentDependentSlots(JSContext* aCx)
{
  // If JSAPI OOMs here, there is basically nothing we can do to recover safely.
  if (!WindowBinding::ClearCachedDocumentValue(aCx, this) ||
      !WindowBinding::ClearCachedPerformanceValue(aCx, this)) {
    MOZ_CRASH("Unhandlable OOM while clearing document dependent slots.");
  }
}

/* static */
JSObject*
nsGlobalWindowInner::CreateNamedPropertiesObject(JSContext *aCx,
                                                 JS::Handle<JSObject*> aProto)
{
  return WindowNamedPropertiesHandler::Create(aCx, aProto);
}

bool
nsGlobalWindowInner::GetIsPrerendered()
{
  nsIDocShell* docShell = GetDocShell();
  return docShell && docShell->GetIsPrerendered();
}

void
nsGlobalWindowInner::RedefineProperty(JSContext* aCx, const char* aPropName,
                                      JS::Handle<JS::Value> aValue,
                                      ErrorResult& aError)
{
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

void
nsGlobalWindowInner::GetReplaceableWindowCoord(JSContext* aCx,
                                               nsGlobalWindowInner::WindowCoordGetter aGetter,
                                               JS::MutableHandle<JS::Value> aRetval,
                                               CallerType aCallerType,
                                               ErrorResult& aError)
{
  int32_t coord = (this->*aGetter)(aCallerType, aError);
  if (!aError.Failed() &&
      !ToJSValue(aCx, coord, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

void
nsGlobalWindowInner::SetReplaceableWindowCoord(JSContext* aCx,
                                               nsGlobalWindowInner::WindowCoordSetter aSetter,
                                               JS::Handle<JS::Value> aValue,
                                               const char* aPropName,
                                               CallerType aCallerType,
                                               ErrorResult& aError)
{
  /*
   * If caller is not chrome and the user has not explicitly exempted the site,
   * just treat this the way we would an IDL replaceable property.
   */
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
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
nsGlobalWindowInner::FireOnNewGlobalObject()
{
  // AutoEntryScript required to invoke debugger hook, which is a
  // Gecko-specific concept at present.
  AutoEntryScript aes(this, "nsGlobalWindowInner report new global");
  JS::Rooted<JSObject*> global(aes.cx(), GetWrapper());
  JS_FireOnNewGlobalObject(aes.cx(), global);
}

#ifdef _WINDOWS_
#error "Never include windows.h in this file!"
#endif

already_AddRefed<Promise>
nsGlobalWindowInner::CreateImageBitmap(JSContext* aCx,
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
nsGlobalWindowInner::CreateImageBitmap(JSContext* aCx,
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
nsGlobalWindowInner::CreateImageBitmap(JSContext* aCx,
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

mozilla::dom::TabGroup*
nsGlobalWindowInner::TabGroupInner()
{
  // If we don't have a TabGroup yet, try to get it from the outer window and
  // cache it.
  if (!mTabGroup) {
    nsGlobalWindowOuter* outer = GetOuterWindowInternal();
    // This will never be called without either an outer window, or a cached tab group.
    // This is because of the following:
    // * This method is only called on inner windows
    // * This method is called as a document is attached to it's script global
    //   by the document
    // * Inner windows are created in nsGlobalWindowInner::SetNewDocument, which
    //   immediately sets a document, which will call this method, causing
    //   the TabGroup to be cached.
    MOZ_RELEASE_ASSERT(outer, "Inner window without outer window has no cached tab group!");
    mTabGroup = outer->TabGroup();
  }
  MOZ_ASSERT(mTabGroup);

#ifdef DEBUG
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  MOZ_ASSERT_IF(outer, outer->TabGroup() == mTabGroup);
#endif

  return mTabGroup;
}

nsresult
nsGlobalWindowInner::Dispatch(TaskCategory aCategory,
                              already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->Dispatch(aCategory, Move(aRunnable));
  }
  return DispatcherTrait::Dispatch(aCategory, Move(aRunnable));
}

nsISerialEventTarget*
nsGlobalWindowInner::EventTargetFor(TaskCategory aCategory) const
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->EventTargetFor(aCategory);
  }
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread*
nsGlobalWindowInner::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (GetDocGroup()) {
    return GetDocGroup()->AbstractMainThreadFor(aCategory);
  }
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}

Worklet*
nsGlobalWindowInner::GetAudioWorklet(ErrorResult& aRv)
{
  if (!mAudioWorklet) {
    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mAudioWorklet = new Worklet(this, principal, Worklet::eAudioWorklet);
  }

  return mAudioWorklet;
}

Worklet*
nsGlobalWindowInner::GetPaintWorklet(ErrorResult& aRv)
{
  if (!mPaintWorklet) {
    nsIPrincipal* principal = GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    mPaintWorklet = new Worklet(this, principal, Worklet::ePaintWorklet);
  }

  return mPaintWorklet;
}

void
nsGlobalWindowInner::GetRegionalPrefsLocales(nsTArray<nsString>& aLocales)
{
  AutoTArray<nsCString, 10> rpLocales;
  mozilla::intl::LocaleService::GetInstance()->GetRegionalPrefsLocales(rpLocales);

  for (const auto& loc : rpLocales) {
    aLocales.AppendElement(NS_ConvertUTF8toUTF16(loc));
  }
}

IntlUtils*
nsGlobalWindowInner::GetIntlUtils(ErrorResult& aError)
{
  if (!mIntlUtils) {
    mIntlUtils = new IntlUtils(this);
  }

  return mIntlUtils;
}

mozilla::dom::TabGroup*
nsPIDOMWindowInner::TabGroup()
{
  return nsGlobalWindowInner::Cast(this)->TabGroupInner();
}

/* static */ already_AddRefed<nsGlobalWindowInner>
nsGlobalWindowInner::Create(nsGlobalWindowOuter *aOuterWindow, bool aIsChrome)
{
  RefPtr<nsGlobalWindowInner> window = new nsGlobalWindowInner(aOuterWindow);
  if (aIsChrome) {
    window->mIsChrome = true;
    window->mCleanMessageManager = true;
  }

  window->InitWasOffline();
  return window.forget();
}

nsIURI*
nsPIDOMWindowInner::GetDocumentURI() const
{
  return mDoc ? mDoc->GetDocumentURI() : mDocumentURI.get();
}

nsIURI*
nsPIDOMWindowInner::GetDocBaseURI() const
{
  return mDoc ? mDoc->GetDocBaseURI() : mDocBaseURI.get();
}

void
nsPIDOMWindowInner::MaybeCreateDoc()
{
  // XXX: Forward to outer?
  MOZ_ASSERT(!mDoc);
  if (nsIDocShell* docShell = GetDocShell()) {
    // Note that |document| here is the same thing as our mDoc, but we
    // don't have to explicitly set the member variable because the docshell
    // has already called SetNewDocument().
    nsCOMPtr<nsIDocument> document = docShell->GetDocument();
    Unused << document;
  }
}

mozilla::dom::DocGroup*
nsPIDOMWindowInner::GetDocGroup() const
{
  nsIDocument* doc = GetExtantDoc();
  if (doc) {
    return doc->GetDocGroup();
  }
  return nullptr;
}

// XXX: Can we define this in a header instead of here?
namespace mozilla {
namespace dom {
extern uint64_t
NextWindowID();
} // namespace dom
} // namespace mozilla

nsPIDOMWindowInner::nsPIDOMWindowInner(nsPIDOMWindowOuter *aOuterWindow)
: mMutationBits(0), mActivePeerConnections(0), mIsDocumentLoaded(false),
  mIsHandlingResizeEvent(false),
  mMayHavePaintEventListener(false), mMayHaveTouchEventListener(false),
  mMayHaveSelectionChangeEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mMayHavePointerEnterLeaveEventListener(false),
  mInnerObjectsFreed(false),
  mAudioCaptured(false),
  mOuterWindow(aOuterWindow),
  // Make sure no actual window ends up with mWindowID == 0
  mWindowID(NextWindowID()), mHasNotifiedGlobalCreated(false),
  mMarkedCCGeneration(0),
  mHasTriedToCacheTopInnerWindow(false),
  mNumOfIndexedDBDatabases(0),
  mNumOfOpenWebSockets(0)
{
  MOZ_ASSERT(aOuterWindow);
}

nsPIDOMWindowInner::~nsPIDOMWindowInner() {}

#undef FORWARD_TO_OUTER
#undef FORWARD_TO_OUTER_OR_THROW
#undef FORWARD_TO_OUTER_VOID
