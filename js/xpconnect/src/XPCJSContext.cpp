/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per JSContext object */

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"

#include "xpcprivate.h"
#include "xpcpublic.h"
#include "XPCWrapper.h"
#include "XPCJSMemoryReporter.h"
#include "WrapperFactory.h"
#include "mozJSComponentLoader.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIObserverService.h"
#include "nsIDebug2.h"
#include "nsIDocShell.h"
#include "nsIRunnable.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ScriptSettings.h"

#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollector.h"
#include "jsapi.h"
#include "js/MemoryMetrics.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/extensions/WebExtensionPolicy.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "AccessCheck.h"
#include "nsGlobalWindow.h"
#include "nsAboutProtocolUtils.h"

#include "GeckoProfiler.h"
#include "nsIInputStream.h"
#include "nsIXULRuntime.h"
#include "nsJSPrincipals.h"
#include "ExpandedPrincipal.h"
#include "SystemPrincipal.h"

#if defined(XP_LINUX) && !defined(ANDROID)
// For getrlimit and min/max.
#include <algorithm>
#include <sys/resource.h>
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

static MOZ_THREAD_LOCAL(XPCJSContext*) gTlsContext;

using namespace mozilla;
using namespace xpc;
using namespace JS;
using mozilla::dom::AutoEntryScript;

static void WatchdogMain(void* arg);
class Watchdog;
class WatchdogManager;
class MOZ_RAII AutoLockWatchdog final
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    Watchdog* const mWatchdog;

  public:
    explicit AutoLockWatchdog(Watchdog* aWatchdog MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoLockWatchdog();
};

class Watchdog
{
  public:
    explicit Watchdog(WatchdogManager* aManager)
      : mManager(aManager)
      , mLock(nullptr)
      , mWakeup(nullptr)
      , mThread(nullptr)
      , mHibernating(false)
      , mInitialized(false)
      , mShuttingDown(false)
      , mMinScriptRunTimeSeconds(1)
    {}
    ~Watchdog() { MOZ_ASSERT(!Initialized()); }

    WatchdogManager* Manager() { return mManager; }
    bool Initialized() { return mInitialized; }
    bool ShuttingDown() { return mShuttingDown; }
    PRLock* GetLock() { return mLock; }
    bool Hibernating() { return mHibernating; }
    void WakeUp()
    {
        MOZ_ASSERT(Initialized());
        MOZ_ASSERT(Hibernating());
        mHibernating = false;
        PR_NotifyCondVar(mWakeup);
    }

    //
    // Invoked by the main thread only.
    //

    void Init()
    {
        MOZ_ASSERT(NS_IsMainThread());
        mLock = PR_NewLock();
        if (!mLock)
            MOZ_CRASH("PR_NewLock failed.");

        mWakeup = PR_NewCondVar(mLock);
        if (!mWakeup)
            MOZ_CRASH("PR_NewCondVar failed.");

        {
            AutoLockWatchdog lock(this);

            // Gecko uses thread private for accounting and has to clean up at thread exit.
            // Therefore, even though we don't have a return value from the watchdog, we need to
            // join it on shutdown.
            mThread = PR_CreateThread(PR_USER_THREAD, WatchdogMain, this,
                                      PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                      PR_JOINABLE_THREAD, 0);
            if (!mThread)
                MOZ_CRASH("PR_CreateThread failed!");

            // WatchdogMain acquires the lock and then asserts mInitialized. So
            // make sure to set mInitialized before releasing the lock here so
            // that it's atomic with the creation of the thread.
            mInitialized = true;
        }
    }

    void Shutdown()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(Initialized());
        {   // Scoped lock.
            AutoLockWatchdog lock(this);

            // Signal to the watchdog thread that it's time to shut down.
            mShuttingDown = true;

            // Wake up the watchdog, and wait for it to call us back.
            PR_NotifyCondVar(mWakeup);
        }

        PR_JoinThread(mThread);

        // The thread sets mShuttingDown to false as it exits.
        MOZ_ASSERT(!mShuttingDown);

        // Destroy state.
        mThread = nullptr;
        PR_DestroyCondVar(mWakeup);
        mWakeup = nullptr;
        PR_DestroyLock(mLock);
        mLock = nullptr;

        // All done.
        mInitialized = false;
    }

    void SetMinScriptRunTimeSeconds(int32_t seconds)
    {
        // This variable is atomic, and is set from the main thread without
        // locking.
        MOZ_ASSERT(seconds > 0);
        mMinScriptRunTimeSeconds = seconds;
    }

    //
    // Invoked by the watchdog thread only.
    //

    void Hibernate()
    {
        MOZ_ASSERT(!NS_IsMainThread());
        mHibernating = true;
        Sleep(PR_INTERVAL_NO_TIMEOUT);
    }
    void Sleep(PRIntervalTime timeout)
    {
        MOZ_ASSERT(!NS_IsMainThread());
        MOZ_ALWAYS_TRUE(PR_WaitCondVar(mWakeup, timeout) == PR_SUCCESS);
    }
    void Finished()
    {
        MOZ_ASSERT(!NS_IsMainThread());
        mShuttingDown = false;
    }

    int32_t MinScriptRunTimeSeconds()
    {
        return mMinScriptRunTimeSeconds;
    }

  private:
    WatchdogManager* mManager;

    PRLock* mLock;
    PRCondVar* mWakeup;
    PRThread* mThread;
    bool mHibernating;
    bool mInitialized;
    bool mShuttingDown;
    mozilla::Atomic<int32_t> mMinScriptRunTimeSeconds;
};

#define PREF_MAX_SCRIPT_RUN_TIME_CONTENT "dom.max_script_run_time"
#define PREF_MAX_SCRIPT_RUN_TIME_CHROME "dom.max_chrome_script_run_time"
#define PREF_MAX_SCRIPT_RUN_TIME_EXT_CONTENT "dom.max_ext_content_script_run_time"

class WatchdogManager : public nsIObserver
{
  public:

    NS_DECL_ISUPPORTS
    explicit WatchdogManager()
    {
        // All the timestamps start at zero.
        PodArrayZero(mTimestamps);

        // Register ourselves as an observer to get updates on the pref.
        mozilla::Preferences::AddStrongObserver(this, "dom.use_watchdog");
        mozilla::Preferences::AddStrongObserver(this, PREF_MAX_SCRIPT_RUN_TIME_CONTENT);
        mozilla::Preferences::AddStrongObserver(this, PREF_MAX_SCRIPT_RUN_TIME_CHROME);
        mozilla::Preferences::AddStrongObserver(this, PREF_MAX_SCRIPT_RUN_TIME_EXT_CONTENT);
    }

  protected:

    virtual ~WatchdogManager()
    {
        // Shutting down the watchdog requires context-switching to the watchdog
        // thread, which isn't great to do in a destructor. So we require
        // consumers to shut it down manually before releasing it.
        MOZ_ASSERT(!mWatchdog);
    }

  public:

    void Shutdown()
    {
        mozilla::Preferences::RemoveObserver(this, "dom.use_watchdog");
        mozilla::Preferences::RemoveObserver(this, PREF_MAX_SCRIPT_RUN_TIME_CONTENT);
        mozilla::Preferences::RemoveObserver(this, PREF_MAX_SCRIPT_RUN_TIME_CHROME);
        mozilla::Preferences::RemoveObserver(this, PREF_MAX_SCRIPT_RUN_TIME_EXT_CONTENT);
    }

    NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) override
    {
        RefreshWatchdog();
        return NS_OK;
    }

    void
    RegisterContext(XPCJSContext* aContext)
    {
        MOZ_ASSERT(NS_IsMainThread());
        AutoLockWatchdog lock(mWatchdog);

        if (aContext->mActive == XPCJSContext::CONTEXT_ACTIVE) {
            mActiveContexts.insertBack(aContext);
        } else {
            mInactiveContexts.insertBack(aContext);
        }

        // Enable the watchdog, if appropriate.
        RefreshWatchdog();
    }

    void
    UnregisterContext(XPCJSContext* aContext)
    {
        MOZ_ASSERT(NS_IsMainThread());
        AutoLockWatchdog lock(mWatchdog);

        // aContext must be in one of our two lists, simply remove it.
        aContext->LinkedListElement<XPCJSContext>::remove();

#ifdef DEBUG
        // If this was the last context, we should have already shut down
        // the watchdog.
        if (mActiveContexts.isEmpty() && mInactiveContexts.isEmpty()) {
            MOZ_ASSERT(!mWatchdog);
        }
#endif
    }

    // Context statistics. These live on the watchdog manager, are written
    // from the main thread, and are read from the watchdog thread (holding
    // the lock in each case).
    void RecordContextActivity(XPCJSContext* aContext, bool active)
    {
        // The watchdog reads this state, so acquire the lock before writing it.
        MOZ_ASSERT(NS_IsMainThread());
        AutoLockWatchdog lock(mWatchdog);

        // Write state.
        aContext->mLastStateChange = PR_Now();
        aContext->mActive = active ? XPCJSContext::CONTEXT_ACTIVE :
            XPCJSContext::CONTEXT_INACTIVE;
        UpdateContextLists(aContext);

        // The watchdog may be hibernating, waiting for the context to go
        // active. Wake it up if necessary.
        if (active && mWatchdog && mWatchdog->Hibernating())
            mWatchdog->WakeUp();
    }

    bool IsAnyContextActive()
    {
        return !mActiveContexts.isEmpty();
    }
    PRTime TimeSinceLastActiveContext()
    {
        // Must be called on the watchdog thread with the lock held.
        MOZ_ASSERT(!NS_IsMainThread());
        PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(mWatchdog->GetLock());
        MOZ_ASSERT(mActiveContexts.isEmpty());
        MOZ_ASSERT(!mInactiveContexts.isEmpty());

        // We store inactive contexts with the most recently added inactive
        // context at the end of the list.
        return PR_Now() - mInactiveContexts.getLast()->mLastStateChange;
    }

    void RecordTimestamp(WatchdogTimestampCategory aCategory)
    {
        // Must be called on the watchdog thread with the lock held.
        MOZ_ASSERT(!NS_IsMainThread());
        PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(mWatchdog->GetLock());
        MOZ_ASSERT(aCategory != TimestampContextStateChange,
                   "Use RecordContextActivity to update this");

        mTimestamps[aCategory] = PR_Now();
    }

    PRTime GetContextTimestamp(XPCJSContext* aContext,
                               const AutoLockWatchdog& aProofOfLock)
    {
        return aContext->mLastStateChange;
    }

    PRTime GetTimestamp(WatchdogTimestampCategory aCategory,
                        const AutoLockWatchdog& aProofOfLock)
    {
        MOZ_ASSERT(aCategory != TimestampContextStateChange,
                   "Use GetContextTimestamp to retrieve this");
        return mTimestamps[aCategory];
    }

    Watchdog* GetWatchdog() { return mWatchdog; }

    void RefreshWatchdog()
    {
        bool wantWatchdog = Preferences::GetBool("dom.use_watchdog", true);
        if (wantWatchdog != !!mWatchdog) {
            if (wantWatchdog)
                StartWatchdog();
            else
                StopWatchdog();
        }

        if (mWatchdog) {
            int32_t contentTime = Preferences::GetInt(PREF_MAX_SCRIPT_RUN_TIME_CONTENT, 10);
            if (contentTime <= 0)
                contentTime = INT32_MAX;
            int32_t chromeTime = Preferences::GetInt(PREF_MAX_SCRIPT_RUN_TIME_CHROME, 20);
            if (chromeTime <= 0)
                chromeTime = INT32_MAX;
            int32_t extTime = Preferences::GetInt(PREF_MAX_SCRIPT_RUN_TIME_EXT_CONTENT, 5);
            if (extTime <= 0)
                extTime = INT32_MAX;
            mWatchdog->SetMinScriptRunTimeSeconds(std::min({contentTime, chromeTime, extTime}));
        }
    }

    void StartWatchdog()
    {
        MOZ_ASSERT(!mWatchdog);
        mWatchdog = new Watchdog(this);
        mWatchdog->Init();
    }

    void StopWatchdog()
    {
        MOZ_ASSERT(mWatchdog);
        mWatchdog->Shutdown();
        mWatchdog = nullptr;
    }

    template<class Callback>
    void ForAllActiveContexts(Callback&& aCallback)
    {
        // This function must be called on the watchdog thread with the lock held.
        MOZ_ASSERT(!NS_IsMainThread());
        PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(mWatchdog->GetLock());

        for (auto* context = mActiveContexts.getFirst(); context;
             context = context->LinkedListElement<XPCJSContext>::getNext()) {
            if (!aCallback(context)) {
                return;
            }
        }
    }

  private:
    void UpdateContextLists(XPCJSContext* aContext)
    {
        // Given aContext whose activity state or timestamp has just changed,
        // put it back in the proper position in the proper list.
        aContext->LinkedListElement<XPCJSContext>::remove();
        auto& list = aContext->mActive == XPCJSContext::CONTEXT_ACTIVE ?
            mActiveContexts : mInactiveContexts;

        // Either the new list is empty or aContext must be more recent than
        // the existing last element.
        MOZ_ASSERT_IF(!list.isEmpty(),
                      list.getLast()->mLastStateChange < aContext->mLastStateChange);
        list.insertBack(aContext);
    }

    LinkedList<XPCJSContext> mActiveContexts;
    LinkedList<XPCJSContext> mInactiveContexts;
    nsAutoPtr<Watchdog> mWatchdog;

    // We store ContextStateChange on the contexts themselves.
    PRTime mTimestamps[kWatchdogTimestampCategoryCount - 1];
};

NS_IMPL_ISUPPORTS(WatchdogManager, nsIObserver)

AutoLockWatchdog::AutoLockWatchdog(Watchdog* aWatchdog MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mWatchdog(aWatchdog)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mWatchdog) {
        PR_Lock(mWatchdog->GetLock());
    }
}

AutoLockWatchdog::~AutoLockWatchdog()
{
    if (mWatchdog) {
        PR_Unlock(mWatchdog->GetLock());
    }
}

static void
WatchdogMain(void* arg)
{
    AUTO_PROFILER_REGISTER_THREAD("JS Watchdog");
    NS_SetCurrentThreadName("JS Watchdog");

    Watchdog* self = static_cast<Watchdog*>(arg);
    WatchdogManager* manager = self->Manager();

    // Lock lasts until we return
    AutoLockWatchdog lock(self);

    MOZ_ASSERT(self->Initialized());
    while (!self->ShuttingDown()) {
        // Sleep only 1 second if recently (or currently) active; otherwise, hibernate
        if (manager->IsAnyContextActive() ||
            manager->TimeSinceLastActiveContext() <= PRTime(2*PR_USEC_PER_SEC))
        {
            self->Sleep(PR_TicksPerSecond());
        } else {
            manager->RecordTimestamp(TimestampWatchdogHibernateStart);
            self->Hibernate();
            manager->RecordTimestamp(TimestampWatchdogHibernateStop);
        }

        // Rise and shine.
        manager->RecordTimestamp(TimestampWatchdogWakeup);

        // Don't request an interrupt callback unless the current script has
        // been running long enough that we might show the slow script dialog.
        // Triggering the callback from off the main thread can be expensive.

        // We want to avoid showing the slow script dialog if the user's laptop
        // goes to sleep in the middle of running a script. To ensure this, we
        // invoke the interrupt callback after only half the timeout has
        // elapsed. The callback simply records the fact that it was called in
        // the mSlowScriptSecondHalf flag. Then we wait another (timeout/2)
        // seconds and invoke the callback again. This time around it sees
        // mSlowScriptSecondHalf is set and so it shows the slow script
        // dialog. If the computer is put to sleep during one of the (timeout/2)
        // periods, the script still has the other (timeout/2) seconds to
        // finish.
        if (!self->ShuttingDown() && manager->IsAnyContextActive()) {
            bool debuggerAttached = false;
            nsCOMPtr<nsIDebug2> dbg = do_GetService("@mozilla.org/xpcom/debug;1");
            if (dbg)
                dbg->GetIsDebuggerAttached(&debuggerAttached);
            if (debuggerAttached) {
                // We won't be interrupting these scripts anyway.
                continue;
            }

            PRTime usecs = self->MinScriptRunTimeSeconds() * PR_USEC_PER_SEC / 2;
            manager->ForAllActiveContexts([usecs, manager, &lock](XPCJSContext* aContext) -> bool {
                auto timediff = PR_Now() - manager->GetContextTimestamp(aContext, lock);
                if (timediff > usecs) {
                    JS_RequestInterruptCallback(aContext->Context());
                    return true;
                }
                return false;
            });
        }
    }

    // Tell the manager that we've shut down.
    self->Finished();
}

PRTime
XPCJSContext::GetWatchdogTimestamp(WatchdogTimestampCategory aCategory)
{
    AutoLockWatchdog lock(mWatchdogManager->GetWatchdog());
    return aCategory == TimestampContextStateChange ?
        mWatchdogManager->GetContextTimestamp(this, lock) :
        mWatchdogManager->GetTimestamp(aCategory, lock);
}

void
xpc::SimulateActivityCallback(bool aActive)
{
    XPCJSContext::ActivityCallback(XPCJSContext::Get(), aActive);
}

// static
void
XPCJSContext::ActivityCallback(void* arg, bool active)
{
    if (!active) {
        ProcessHangMonitor::ClearHang();
    }

    XPCJSContext* self = static_cast<XPCJSContext*>(arg);
    self->mWatchdogManager->RecordContextActivity(self, active);
}

// static
bool
XPCJSContext::InterruptCallback(JSContext* cx)
{
    XPCJSContext* self = XPCJSContext::Get();

    // Now is a good time to turn on profiling if it's pending.
    PROFILER_JS_INTERRUPT_CALLBACK();

    // Normally we record mSlowScriptCheckpoint when we start to process an
    // event. However, we can run JS outside of event handlers. This code takes
    // care of that case.
    if (self->mSlowScriptCheckpoint.IsNull()) {
        self->mSlowScriptCheckpoint = TimeStamp::NowLoRes();
        self->mSlowScriptSecondHalf = false;
        self->mSlowScriptActualWait = mozilla::TimeDuration();
        self->mTimeoutAccumulated = false;
        return true;
    }

    // Sometimes we get called back during XPConnect initialization, before Gecko
    // has finished bootstrapping. Avoid crashing in nsContentUtils below.
    if (!nsContentUtils::IsInitialized())
        return true;

    // This is at least the second interrupt callback we've received since
    // returning to the event loop. See how long it's been, and what the limit
    // is.
    TimeDuration duration = TimeStamp::NowLoRes() - self->mSlowScriptCheckpoint;
    int32_t limit;

    nsString addonId;
    const char* prefName;

    auto principal = BasePrincipal::Cast(nsContentUtils::SubjectPrincipal(cx));
    bool chrome = principal->Is<SystemPrincipal>();
    if (chrome) {
        prefName = PREF_MAX_SCRIPT_RUN_TIME_CHROME;
        limit = Preferences::GetInt(prefName, 20);
    } else if (auto policy = principal->ContentScriptAddonPolicy()) {
        policy->GetId(addonId);
        prefName = PREF_MAX_SCRIPT_RUN_TIME_EXT_CONTENT;
        limit = Preferences::GetInt(prefName, 5);
    } else {
        prefName = PREF_MAX_SCRIPT_RUN_TIME_CONTENT;
        limit = Preferences::GetInt(prefName, 10);
    }

    // If there's no limit, or we're within the limit, let it go.
    if (limit == 0 || duration.ToSeconds() < limit / 2.0)
        return true;

    self->mSlowScriptActualWait += duration;

    // In order to guard against time changes or laptops going to sleep, we
    // don't trigger the slow script warning until (limit/2) seconds have
    // elapsed twice.
    if (!self->mSlowScriptSecondHalf) {
        self->mSlowScriptCheckpoint = TimeStamp::NowLoRes();
        self->mSlowScriptSecondHalf = true;
        return true;
    }

    //
    // This has gone on long enough! Time to take action. ;-)
    //

    // Get the DOM window associated with the running script. If the script is
    // running in a non-DOM scope, we have to just let it keep running.
    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    RefPtr<nsGlobalWindowInner> win = WindowOrNull(global);
    if (!win && IsSandbox(global)) {
        // If this is a sandbox associated with a DOMWindow via a
        // sandboxPrototype, use that DOMWindow. This supports GreaseMonkey
        // and JetPack content scripts.
        JS::Rooted<JSObject*> proto(cx);
        if (!JS_GetPrototype(cx, global, &proto))
            return false;
        if (proto && xpc::IsSandboxPrototypeProxy(proto) &&
            (proto = js::CheckedUnwrap(proto, /* stopAtWindowProxy = */ false)))
        {
            win = WindowGlobalOrNull(proto);
        }
    }

    if (!win) {
        NS_WARNING("No active window");
        return true;
    }

    if (win->IsDying()) {
        // The window is being torn down. When that happens we try to prevent
        // the dispatch of new runnables, so it also makes sense to kill any
        // long-running script. The user is primarily interested in this page
        // going away.
        return false;
    }

    // Accumulate slow script invokation delay.
    if (!chrome && !self->mTimeoutAccumulated) {
      uint32_t delay = uint32_t(self->mSlowScriptActualWait.ToMilliseconds() - (limit * 1000.0));
      Telemetry::Accumulate(Telemetry::SLOW_SCRIPT_NOTIFY_DELAY, delay);
      self->mTimeoutAccumulated = true;
    }

    // Show the prompt to the user, and kill if requested.
    nsGlobalWindowInner::SlowScriptResponse response = win->ShowSlowScriptDialog(addonId);
    if (response == nsGlobalWindowInner::KillSlowScript) {
        if (Preferences::GetBool("dom.global_stop_script", true))
            xpc::Scriptability::Get(global).Block();
        return false;
    }
    if (response == nsGlobalWindowInner::KillScriptGlobal) {
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

        if (!IsSandbox(global) || !obs)
            return false;

        // Notify the extensions framework that the sandbox should be killed.
        nsIXPConnect* xpc = nsContentUtils::XPConnect();
        JS::RootedObject wrapper(cx, JS_NewPlainObject(cx));
        nsCOMPtr<nsISupports> supports;

        // Store the sandbox object on the wrappedJSObject property of the
        // subject so that JS recipients can access the JS value directly.
        if (!wrapper ||
            !JS_DefineProperty(cx, wrapper, "wrappedJSObject", global, JSPROP_ENUMERATE) ||
            NS_FAILED(xpc->WrapJS(cx, wrapper, NS_GET_IID(nsISupports), getter_AddRefs(supports)))) {
            return false;
        }

        obs->NotifyObservers(supports, "kill-content-script-sandbox", nullptr);
        return false;
    }

    // The user chose to continue the script. Reset the timer, and disable this
    // machinery with a pref of the user opted out of future slow-script dialogs.
    if (response != nsGlobalWindowInner::ContinueSlowScriptAndKeepNotifying)
        self->mSlowScriptCheckpoint = TimeStamp::NowLoRes();

    if (response == nsGlobalWindowInner::AlwaysContinueSlowScript)
        Preferences::SetInt(prefName, 0);

    return true;
}

#define JS_OPTIONS_DOT_STR "javascript.options."

static mozilla::Atomic<bool> sDiscardSystemSource(false);

bool
xpc::ShouldDiscardSystemSource() { return sDiscardSystemSource; }

#ifdef DEBUG
static mozilla::Atomic<bool> sExtraWarningsForSystemJS(false);
bool xpc::ExtraWarningsForSystemJS() { return sExtraWarningsForSystemJS; }
#else
bool xpc::ExtraWarningsForSystemJS() { return false; }
#endif

static mozilla::Atomic<bool> sSharedMemoryEnabled(false);

bool
xpc::SharedMemoryEnabled() { return sSharedMemoryEnabled; }

static void
ReloadPrefsCallback(const char* pref, void* data)
{
    XPCJSContext* xpccx = static_cast<XPCJSContext*>(data);
    JSContext* cx = xpccx->Context();

    bool useBaseline = Preferences::GetBool(JS_OPTIONS_DOT_STR "baselinejit");
    bool useIon = Preferences::GetBool(JS_OPTIONS_DOT_STR "ion");
    bool useAsmJS = Preferences::GetBool(JS_OPTIONS_DOT_STR "asmjs");
    bool useWasm = Preferences::GetBool(JS_OPTIONS_DOT_STR "wasm");
    bool useWasmIon = Preferences::GetBool(JS_OPTIONS_DOT_STR "wasm_ionjit");
    bool useWasmBaseline = Preferences::GetBool(JS_OPTIONS_DOT_STR "wasm_baselinejit");
#ifdef ENABLE_WASM_GC
    bool useWasmGc = Preferences::GetBool(JS_OPTIONS_DOT_STR "wasm_gc");
#endif
    bool throwOnAsmJSValidationFailure = Preferences::GetBool(JS_OPTIONS_DOT_STR
                                                              "throw_on_asmjs_validation_failure");
    bool useNativeRegExp = Preferences::GetBool(JS_OPTIONS_DOT_STR "native_regexp");

    bool parallelParsing = Preferences::GetBool(JS_OPTIONS_DOT_STR "parallel_parsing");
    bool offthreadIonCompilation = Preferences::GetBool(JS_OPTIONS_DOT_STR
                                                       "ion.offthread_compilation");
    bool useBaselineEager = Preferences::GetBool(JS_OPTIONS_DOT_STR
                                                 "baselinejit.unsafe_eager_compilation");
    bool useIonEager = Preferences::GetBool(JS_OPTIONS_DOT_STR "ion.unsafe_eager_compilation");
#ifdef DEBUG
    bool fullJitDebugChecks = Preferences::GetBool(JS_OPTIONS_DOT_STR "jit.full_debug_checks");
#endif

    int32_t baselineThreshold = Preferences::GetInt(JS_OPTIONS_DOT_STR "baselinejit.threshold", -1);
    int32_t ionThreshold = Preferences::GetInt(JS_OPTIONS_DOT_STR "ion.threshold", -1);

    sDiscardSystemSource = Preferences::GetBool(JS_OPTIONS_DOT_STR "discardSystemSource");

    bool useAsyncStack = Preferences::GetBool(JS_OPTIONS_DOT_STR "asyncstack");

    bool throwOnDebuggeeWouldRun = Preferences::GetBool(JS_OPTIONS_DOT_STR
                                                        "throw_on_debuggee_would_run");

    bool dumpStackOnDebuggeeWouldRun = Preferences::GetBool(JS_OPTIONS_DOT_STR
                                                            "dump_stack_on_debuggee_would_run");

    bool werror = Preferences::GetBool(JS_OPTIONS_DOT_STR "werror");

    bool extraWarnings = Preferences::GetBool(JS_OPTIONS_DOT_STR "strict");

    bool streams = Preferences::GetBool(JS_OPTIONS_DOT_STR "streams");

    bool spectreIndexMasking = Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.index_masking");
    bool spectreObjectMitigationsBarriers =
        Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.object_mitigations.barriers");
    bool spectreObjectMitigationsMisc =
        Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.object_mitigations.misc");
    bool spectreStringMitigations =
        Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.string_mitigations");
    bool spectreValueMasking = Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.value_masking");
    bool spectreJitToCxxCalls = Preferences::GetBool(JS_OPTIONS_DOT_STR "spectre.jit_to_C++_calls");

    sSharedMemoryEnabled = Preferences::GetBool(JS_OPTIONS_DOT_STR "shared_memory");

#ifdef DEBUG
    sExtraWarningsForSystemJS = Preferences::GetBool(JS_OPTIONS_DOT_STR "strict.debug");
#endif

#ifdef JS_GC_ZEAL
    int32_t zeal = Preferences::GetInt(JS_OPTIONS_DOT_STR "gczeal", -1);
    int32_t zeal_frequency =
        Preferences::GetInt(JS_OPTIONS_DOT_STR "gczeal.frequency",
                            JS_DEFAULT_ZEAL_FREQ);
    if (zeal >= 0) {
        JS_SetGCZeal(cx, (uint8_t)zeal, zeal_frequency);
    }
#endif // JS_GC_ZEAL

#ifdef FUZZING
    bool fuzzingEnabled = Preferences::GetBool("fuzzing.enabled");
#endif

    JS::ContextOptionsRef(cx).setBaseline(useBaseline)
                             .setIon(useIon)
                             .setAsmJS(useAsmJS)
                             .setWasm(useWasm)
                             .setWasmIon(useWasmIon)
                             .setWasmBaseline(useWasmBaseline)
#ifdef ENABLE_WASM_GC
                             .setWasmGc(useWasmGc)
#endif
                             .setThrowOnAsmJSValidationFailure(throwOnAsmJSValidationFailure)
                             .setNativeRegExp(useNativeRegExp)
                             .setAsyncStack(useAsyncStack)
                             .setThrowOnDebuggeeWouldRun(throwOnDebuggeeWouldRun)
                             .setDumpStackOnDebuggeeWouldRun(dumpStackOnDebuggeeWouldRun)
                             .setWerror(werror)
#ifdef FUZZING
                             .setFuzzing(fuzzingEnabled)
#endif
                             .setStreams(streams)
                             .setExtraWarnings(extraWarnings);

    nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
    if (xr) {
        bool safeMode = false;
        xr->GetInSafeMode(&safeMode);
        if (safeMode) {
            JS::ContextOptionsRef(cx).disableOptionsForSafeMode();
        }
    }

    JS_SetParallelParsingEnabled(cx, parallelParsing);
    JS_SetOffthreadIonCompilationEnabled(cx, offthreadIonCompilation);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_WARMUP_TRIGGER,
                                  useBaselineEager ? 0 : baselineThreshold);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_ION_WARMUP_TRIGGER,
                                  useIonEager ? 0 : ionThreshold);
#ifdef DEBUG
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_FULL_DEBUG_CHECKS, fullJitDebugChecks);
#endif

    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_INDEX_MASKING, spectreIndexMasking);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_OBJECT_MITIGATIONS_BARRIERS,
                                  spectreObjectMitigationsBarriers);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_OBJECT_MITIGATIONS_MISC,
                                  spectreObjectMitigationsMisc);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_STRING_MITIGATIONS,
                                  spectreStringMitigations);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_VALUE_MASKING, spectreValueMasking);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_SPECTRE_JIT_TO_CXX_CALLS,
                                  spectreJitToCxxCalls);
}

XPCJSContext::~XPCJSContext()
{
    MOZ_COUNT_DTOR_INHERITED(XPCJSContext, CycleCollectedJSContext);
    // Elsewhere we abort immediately if XPCJSContext initialization fails.
    // Therefore the context must be non-null.
    MOZ_ASSERT(MaybeContext());

    Preferences::UnregisterPrefixCallback(ReloadPrefsCallback,
                                          JS_OPTIONS_DOT_STR,
                                          this);

#ifdef FUZZING
    Preferences::UnregisterCallback(ReloadPrefsCallback, "fuzzing.enabled", this);
#endif

    js::SetActivityCallback(Context(), nullptr, nullptr);

    // Clear any pending exception.  It might be an XPCWrappedJS, and if we try
    // to destroy it later we will crash.
    SetPendingException(nullptr);

    // If we're the last XPCJSContext around, clean up the watchdog manager.
    if (--sInstanceCount == 0) {
        if (mWatchdogManager->GetWatchdog()) {
            mWatchdogManager->StopWatchdog();
        }

        mWatchdogManager->UnregisterContext(this);
        mWatchdogManager->Shutdown();
        sWatchdogInstance = nullptr;
    } else {
        // Otherwise, simply remove ourselves from the list.
        mWatchdogManager->UnregisterContext(this);
    }

    if (mCallContext)
        mCallContext->SystemIsBeingShutDown();

    PROFILER_CLEAR_JS_CONTEXT();

    gTlsContext.set(nullptr);
}

XPCJSContext::XPCJSContext()
 : mCallContext(nullptr),
   mAutoRoots(nullptr),
   mResolveName(JSID_VOID),
   mResolvingWrapper(nullptr),
   mWatchdogManager(GetWatchdogManager()),
   mSlowScriptSecondHalf(false),
   mTimeoutAccumulated(false),
   mPendingResult(NS_OK),
   mActive(CONTEXT_INACTIVE),
   mLastStateChange(PR_Now())
{
    MOZ_COUNT_CTOR_INHERITED(XPCJSContext, CycleCollectedJSContext);
    MOZ_RELEASE_ASSERT(!gTlsContext.get());
    MOZ_ASSERT(mWatchdogManager);
    ++sInstanceCount;
    mWatchdogManager->RegisterContext(this);
    gTlsContext.set(this);
}

/* static */ XPCJSContext*
XPCJSContext::Get()
{
    return gTlsContext.get();
}

#ifdef XP_WIN
static size_t
GetWindowsStackSize()
{
    // First, get the stack base. Because the stack grows down, this is the top
    // of the stack.
    const uint8_t* stackTop;
#ifdef _WIN64
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    stackTop = reinterpret_cast<const uint8_t*>(pTib->StackBase);
#else
    PNT_TIB pTib = reinterpret_cast<PNT_TIB>(NtCurrentTeb());
    stackTop = reinterpret_cast<const uint8_t*>(pTib->StackBase);
#endif

    // Now determine the stack bottom. Note that we can't use tib->StackLimit,
    // because that's the size of the committed area and we're also interested
    // in the reserved pages below that.
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(&mbi, &mbi, sizeof(mbi)))
        MOZ_CRASH("VirtualQuery failed");

    const uint8_t* stackBottom = reinterpret_cast<const uint8_t*>(mbi.AllocationBase);

    // Do some sanity checks.
    size_t stackSize = size_t(stackTop - stackBottom);
    MOZ_RELEASE_ASSERT(stackSize >= 1 * 1024 * 1024);
    MOZ_RELEASE_ASSERT(stackSize <= 32 * 1024 * 1024);

    // Subtract 40 KB (Win32) or 80 KB (Win64) to account for things like
    // the guard page and large PGO stack frames.
    return stackSize - 10 * sizeof(uintptr_t) * 1024;
}
#endif

XPCJSRuntime*
XPCJSContext::Runtime() const
{
    return static_cast<XPCJSRuntime*>(CycleCollectedJSContext::Runtime());
}

CycleCollectedJSRuntime*
XPCJSContext::CreateRuntime(JSContext* aCx)
{
    return new XPCJSRuntime(aCx);
}

nsresult
XPCJSContext::Initialize(XPCJSContext* aPrimaryContext)
{
    nsresult rv;
    if (aPrimaryContext) {
        rv = CycleCollectedJSContext::InitializeNonPrimary(aPrimaryContext);
    } else {
        rv = CycleCollectedJSContext::Initialize(nullptr,
                                                 JS::DefaultHeapMaxBytes,
                                                 JS::DefaultNurseryBytes);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(Context());
    JSContext* cx = Context();

    // The JS engine permits us to set different stack limits for system code,
    // trusted script, and untrusted script. We have tests that ensure that
    // we can always execute 10 "heavy" (eval+with) stack frames deeper in
    // privileged code. Our stack sizes vary greatly in different configurations,
    // so satisfying those tests requires some care. Manual measurements of the
    // number of heavy stack frames achievable gives us the following rough data,
    // ordered by the effective categories in which they are grouped in the
    // JS_SetNativeStackQuota call (which predates this analysis).
    //
    // The following "Stack Frames" numbers come from `chromeLimit` in
    // js/xpconnect/tests/chrome/test_bug732665.xul
    //
    //  Platform   | Build | Stack Quota | Stack Frames | Stack Frame Size
    // ------------+-------+-------------+--------------+------------------
    //  OSX 64     | Opt   | 7MB         | 1331         | ~5.4k
    //  OSX 64     | Debug | 7MB         | 1202         | ~6.0k
    // ------------+-------+-------------+--------------+------------------
    //  Linux 32   | Opt   | 7.875MB     | 2513         | ~3.2k
    //  Linux 32   | Debug | 7.875MB     | 2146         | ~3.8k
    // ------------+-------+-------------+--------------+------------------
    //  Linux 64   | Opt   | 7.875MB     | 1360         | ~5.9k
    //  Linux 64   | Debug | 7.875MB     | 1180         | ~6.8k
    //  Linux 64   | ASan  | 7.875MB     | 473          | ~17.0k
    // ------------+-------+-------------+--------------+------------------
    //  Windows 32 | Opt   | 984k        | 188          | ~5.2k
    //  Windows 32 | Debug | 984k        | 208          | ~4.7k
    // ------------+-------+-------------+--------------+------------------
    //  Windows 64 | Opt   | 1.922MB     | 189          | ~10.4k
    //  Windows 64 | Debug | 1.922MB     | 175          | ~11.2k
    //
    // We tune the trusted/untrusted quotas for each configuration to achieve our
    // invariants while attempting to minimize overhead. In contrast, our buffer
    // between system code and trusted script is a very unscientific 10k.
    const size_t kSystemCodeBuffer = 10 * 1024;

    // Our "default" stack is what we use in configurations where we don't have
    // a compelling reason to do things differently. This is effectively 512KB
    // on 32-bit platforms and 1MB on 64-bit platforms.
    const size_t kDefaultStackQuota = 128 * sizeof(size_t) * 1024;

    // Set stack sizes for different configurations. It's probably not great for
    // the web to base this decision primarily on the default stack size that the
    // underlying platform makes available, but that seems to be what we do. :-(

#if defined(XP_MACOSX) || defined(DARWIN)
    // MacOS has a gargantuan default stack size of 8MB. Go wild with 7MB,
    // and give trusted script 180k extra. The stack is huge on mac anyway.
    const size_t kStackQuota = 7 * 1024 * 1024;
    const size_t kTrustedScriptBuffer = 180 * 1024;
#elif defined(XP_LINUX) && !defined(ANDROID)
    // Most Linux distributions set default stack size to 8MB.  Use it as the
    // maximum value.
    const size_t kStackQuotaMax = 8 * 1024 * 1024;
#  if defined(MOZ_ASAN) || defined(DEBUG)
    // Bug 803182: account for the 4x difference in the size of js::Interpret
    // between optimized and debug builds.  We use 2x since the JIT part
    // doesn't increase much.
    // See the standalone MOZ_ASAN branch below for the ASan case.
    const size_t kStackQuotaMin = 2 * kDefaultStackQuota;
#  else
    const size_t kStackQuotaMin = kDefaultStackQuota;
#  endif
    // Allocate 128kB margin for the safe space.
    const size_t kStackSafeMargin = 128 * 1024;

    struct rlimit rlim;
    const size_t kStackQuota =
        getrlimit(RLIMIT_STACK, &rlim) == 0
        ? std::max(std::min(size_t(rlim.rlim_cur - kStackSafeMargin),
                            kStackQuotaMax - kStackSafeMargin),
                   kStackQuotaMin)
        : kStackQuotaMin;
#  if defined(MOZ_ASAN)
    // See the standalone MOZ_ASAN branch below for the ASan case.
    const size_t kTrustedScriptBuffer = 450 * 1024;
#  else
    const size_t kTrustedScriptBuffer = 180 * 1024;
#  endif
#elif defined(MOZ_ASAN)
    // ASan requires more stack space due to red-zones, so give it double the
    // default (1MB on 32-bit, 2MB on 64-bit). ASAN stack frame measurements
    // were not taken at the time of this writing, so we hazard a guess that
    // ASAN builds have roughly thrice the stack overhead as normal builds.
    // On normal builds, the largest stack frame size we might encounter is
    // 9.0k (see above), so let's use a buffer of 9.0 * 5 * 10 = 450k.
    //
    // FIXME: Does this branch make sense for Windows and Android?
    // (See bug 1415195)
    const size_t kStackQuota =  2 * kDefaultStackQuota;
    const size_t kTrustedScriptBuffer = 450 * 1024;
#elif defined(XP_WIN)
    // 1MB is the default stack size on Windows. We use the -STACK linker flag
    // (see WIN32_EXE_LDFLAGS in config/config.mk) to request a larger stack,
    // so we determine the stack size at runtime.
    const size_t kStackQuota = GetWindowsStackSize();
    const size_t kTrustedScriptBuffer = (sizeof(size_t) == 8) ? 180 * 1024   //win64
                                                              : 120 * 1024;  //win32
#elif defined(ANDROID)
    // Android appears to have 1MB stacks. Allow the use of 3/4 of that size
    // (768KB on 32-bit), since otherwise we can crash with a stack overflow
    // when nearing the 1MB limit.
    const size_t kStackQuota = kDefaultStackQuota + kDefaultStackQuota / 2;
    const size_t kTrustedScriptBuffer = sizeof(size_t) * 12800;
#else
    // Catch-all configuration for other environments.
#  if defined(DEBUG)
    const size_t kStackQuota = 2 * kDefaultStackQuota;
#  else
    const size_t kStackQuota = kDefaultStackQuota;
#  endif
    // Given the numbers above, we use 50k and 100k trusted buffers on 32-bit
    // and 64-bit respectively.
    const size_t kTrustedScriptBuffer = sizeof(size_t) * 12800;
#endif

    // Avoid an unused variable warning on platforms where we don't use the
    // default.
    (void) kDefaultStackQuota;

    JS_SetNativeStackQuota(cx,
                           kStackQuota,
                           kStackQuota - kSystemCodeBuffer,
                           kStackQuota - kSystemCodeBuffer - kTrustedScriptBuffer);

    PROFILER_SET_JS_CONTEXT(cx);

    js::SetActivityCallback(cx, ActivityCallback, this);
    JS_AddInterruptCallback(cx, InterruptCallback);

    if (!aPrimaryContext) {
        Runtime()->Initialize(cx);
    }

    // Watch for the JS boolean options.
    ReloadPrefsCallback(nullptr, this);
    Preferences::RegisterPrefixCallback(ReloadPrefsCallback,
                                        JS_OPTIONS_DOT_STR,
                                        this);

#ifdef FUZZING
    Preferences::RegisterCallback(ReloadPrefsCallback, "fuzzing.enabled", this);
#endif

    return NS_OK;
}

// static
uint32_t
XPCJSContext::sInstanceCount;

// static
StaticRefPtr<WatchdogManager>
XPCJSContext::sWatchdogInstance;

// static
WatchdogManager*
XPCJSContext::GetWatchdogManager()
{
    if (sWatchdogInstance) {
        return sWatchdogInstance;
    }

    MOZ_ASSERT(sInstanceCount == 0);
    sWatchdogInstance = new WatchdogManager();
    return sWatchdogInstance;
}

// static
void
XPCJSContext::InitTLS()
{
    MOZ_RELEASE_ASSERT(gTlsContext.init());
}

// static
XPCJSContext*
XPCJSContext::NewXPCJSContext(XPCJSContext* aPrimaryContext)
{
    XPCJSContext* self = new XPCJSContext();
    nsresult rv = self->Initialize(aPrimaryContext);
    if (NS_FAILED(rv)) {
        MOZ_CRASH("new XPCJSContext failed to initialize.");
    }

    if (self->Context())
        return self;

    MOZ_CRASH("new XPCJSContext failed to initialize.");
}

void
XPCJSContext::BeforeProcessTask(bool aMightBlock)
{
    MOZ_ASSERT(NS_IsMainThread());

    // Start the slow script timer.
    mSlowScriptCheckpoint = mozilla::TimeStamp::NowLoRes();
    mSlowScriptSecondHalf = false;
    mSlowScriptActualWait = mozilla::TimeDuration();
    mTimeoutAccumulated = false;

    // As we may be entering a nested event loop, we need to
    // cancel any ongoing performance measurement.
    js::ResetPerformanceMonitoring(Context());

    CycleCollectedJSContext::BeforeProcessTask(aMightBlock);
}

void
XPCJSContext::AfterProcessTask(uint32_t aNewRecursionDepth)
{
    // Now that we're back to the event loop, reset the slow script checkpoint.
    mSlowScriptCheckpoint = mozilla::TimeStamp();
    mSlowScriptSecondHalf = false;

    // Call cycle collector occasionally.
    MOZ_ASSERT(NS_IsMainThread());
    nsJSContext::MaybePokeCC();

    CycleCollectedJSContext::AfterProcessTask(aNewRecursionDepth);

    // Now that we are certain that the event is complete,
    // we can flush any ongoing performance measurement.
    js::FlushPerformanceMonitoring(Context());

    mozilla::jsipc::AfterProcessTask();
}

bool
XPCJSContext::IsSystemCaller() const
{
    return nsContentUtils::IsSystemCaller(Context());
}
