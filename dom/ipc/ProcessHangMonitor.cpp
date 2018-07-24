/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"

#include "jsapi.h"

#include "mozilla/Atomics.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/ipc/TaskFactory.h"
#include "mozilla/Monitor.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"

#include "nsExceptionHandler.h"
#include "nsFrameLoader.h"
#include "nsIHangReport.h"
#include "nsITabParent.h"
#include "nsQueryObject.h"
#include "nsPluginHost.h"
#include "nsThreadUtils.h"

#include "base/task.h"
#include "base/thread.h"

#ifdef XP_WIN
// For IsDebuggerPresent()
#include <windows.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

/*
 * Basic architecture:
 *
 * Each process has its own ProcessHangMonitor singleton. This singleton exists
 * as long as there is at least one content process in the system. Each content
 * process has a HangMonitorChild and the chrome process has one
 * HangMonitorParent per process. Each process (including the chrome process)
 * runs a hang monitoring thread. The PHangMonitor actors are bound to this
 * thread so that they never block on the main thread.
 *
 * When the content process detects a hang, it posts a task to its hang thread,
 * which sends an IPC message to the hang thread in the parent. The parent
 * cancels any ongoing CPOW requests and then posts a runnable to the main
 * thread that notifies Firefox frontend code of the hang. The frontend code is
 * passed an nsIHangReport, which can be used to terminate the hang.
 *
 * If the user chooses to terminate a script, a task is posted to the chrome
 * process's hang monitoring thread, which sends an IPC message to the hang
 * thread in the content process. That thread sets a flag to indicate that JS
 * execution should be terminated the next time it hits the interrupt
 * callback. A similar scheme is used for debugging slow scripts. If a content
 * process or plug-in needs to be terminated, the chrome process does so
 * directly, without messaging the content process.
 */

namespace {

/* Child process objects */

class HangMonitorChild
  : public PProcessHangMonitorChild
{
 public:
  explicit HangMonitorChild(ProcessHangMonitor* aMonitor);
  ~HangMonitorChild() override;

  void Bind(Endpoint<PProcessHangMonitorChild>&& aEndpoint);

  typedef ProcessHangMonitor::SlowScriptAction SlowScriptAction;
  SlowScriptAction NotifySlowScript(nsITabChild* aTabChild,
                                    const char* aFileName,
                                    const nsString& aAddonId);
  void NotifySlowScriptAsync(TabId aTabId,
                             const nsCString& aFileName,
                             const nsString& aAddonId);

  bool IsDebuggerStartupComplete();

  void NotifyPluginHang(uint32_t aPluginId);
  void NotifyPluginHangAsync(uint32_t aPluginId);

  void ClearHang();
  void ClearHangAsync();
  void ClearPaintWhileInterruptingJS(uint64_t aLayerObserverEpoch);

  // MaybeStartPaintWhileInterruptingJS will notify the background hang monitor of activity
  // if this is the first time calling it since ClearPaintWhileInterruptingJS. It should be
  // callable from any thread, but you must be holding mMonitor if using it off
  // the main thread, since it could race with ClearPaintWhileInterruptingJS.
  void MaybeStartPaintWhileInterruptingJS();

  mozilla::ipc::IPCResult RecvTerminateScript(const bool& aTerminateGlobal) override;
  mozilla::ipc::IPCResult RecvBeginStartingDebugger() override;
  mozilla::ipc::IPCResult RecvEndStartingDebugger() override;

  mozilla::ipc::IPCResult RecvPaintWhileInterruptingJS(const TabId& aTabId,
                                                       const bool& aForceRepaint,
                                                       const uint64_t& aLayerObserverEpoch) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void InterruptCallback();
  void Shutdown();

  static HangMonitorChild* Get() { return sInstance; }

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable)
  {
    mHangMonitor->Dispatch(std::move(aRunnable));
  }
  bool IsOnThread() { return mHangMonitor->IsOnThread(); }

 private:
  void ShutdownOnThread();

  // Ordering of this atomic is not preserved while recording/replaying, as it
  // may be accessed during the JS interrupt callback.
  static Atomic<HangMonitorChild*, SequentiallyConsistent,
                recordreplay::Behavior::DontPreserve> sInstance;
  UniquePtr<BackgroundHangMonitor> mPaintWhileInterruptingJSMonitor;

  const RefPtr<ProcessHangMonitor> mHangMonitor;
  Monitor mMonitor;

  // Main thread-only.
  bool mSentReport;

  // These fields must be accessed with mMonitor held.
  bool mTerminateScript;
  bool mTerminateGlobal;
  bool mStartDebugger;
  bool mFinishedStartingDebugger;
  bool mPaintWhileInterruptingJS;
  bool mPaintWhileInterruptingJSForce;
  TabId mPaintWhileInterruptingJSTab;
  MOZ_INIT_OUTSIDE_CTOR uint64_t mPaintWhileInterruptingJSEpoch;
  JSContext* mContext;
  bool mShutdownDone;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;

  // Allows us to ensure we NotifyActivity only once, allowing
  // either thread to do so.
  Atomic<bool> mBHRMonitorActive;
};

Atomic<HangMonitorChild*, SequentiallyConsistent,
       recordreplay::Behavior::DontPreserve> HangMonitorChild::sInstance;

/* Parent process objects */

class HangMonitorParent;

class HangMonitoredProcess final
  : public nsIHangReport
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  HangMonitoredProcess(HangMonitorParent* aActor,
                       ContentParent* aContentParent)
    : mActor(aActor), mContentParent(aContentParent) {}

  NS_DECL_NSIHANGREPORT

  // Called when a content process shuts down.
  void Clear() {
    mContentParent = nullptr;
    mActor = nullptr;
  }

  /**
   * Sets the information associated with this hang: this includes the ID of
   * the plugin which caused the hang as well as the content PID. The ID of
   * a minidump taken during the hang can also be provided.
   *
   * @param aHangData The hang information
   * @param aDumpId The ID of a minidump taken when the hang occurred
   */
  void SetHangData(const HangData& aHangData, const nsAString& aDumpId) {
    mHangData = aHangData;
    mDumpId = aDumpId;
  }

  void ClearHang() {
    mHangData = HangData();
    mDumpId.Truncate();
  }

private:
  ~HangMonitoredProcess() = default;

  // Everything here is main thread-only.
  HangMonitorParent* mActor;
  ContentParent* mContentParent;
  HangData mHangData;
  nsAutoString mDumpId;
};

class HangMonitorParent
  : public PProcessHangMonitorParent
  , public SupportsWeakPtr<HangMonitorParent>
{
public:
  explicit HangMonitorParent(ProcessHangMonitor* aMonitor);
  ~HangMonitorParent() override;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(HangMonitorParent)

  void Bind(Endpoint<PProcessHangMonitorParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvHangEvidence(const HangData& aHangData) override;
  mozilla::ipc::IPCResult RecvClearHang() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void SetProcess(HangMonitoredProcess* aProcess) { mProcess = aProcess; }

  void Shutdown();

  void PaintWhileInterruptingJS(dom::TabParent* aTabParent,
                                bool aForceRepaint,
                                uint64_t aLayerObserverEpoch);

  void TerminateScript(bool aTerminateGlobal);
  void BeginStartingDebugger();
  void EndStartingDebugger();
  void CleanupPluginHang(uint32_t aPluginId, bool aRemoveFiles);

  /**
   * Update the dump for the specified plugin. This method is thread-safe and
   * is used to replace a browser minidump with a full minidump. If aDumpId is
   * empty this is a no-op.
   */
  void UpdateMinidump(uint32_t aPluginId, const nsString& aDumpId);

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable)
  {
    mHangMonitor->Dispatch(std::move(aRunnable));
  }
  bool IsOnThread() { return mHangMonitor->IsOnThread(); }

private:
  bool TakeBrowserMinidump(const PluginHangData& aPhd, nsString& aCrashId);

  void SendHangNotification(const HangData& aHangData,
                            const nsString& aBrowserDumpId,
                            bool aTakeMinidump);
  void OnTakeFullMinidumpComplete(const HangData& aHangData,
                                  const nsString& aDumpId);

  void ClearHangNotification();

  void PaintWhileInterruptingJSOnThread(TabId aTabId, bool aForceRepaint, uint64_t aLayerObserverEpoch);

  void ShutdownOnThread();

  const RefPtr<ProcessHangMonitor> mHangMonitor;

  // This field is read-only after construction.
  bool mReportHangs;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;

  Monitor mMonitor;

  // Must be accessed with mMonitor held.
  RefPtr<HangMonitoredProcess> mProcess;
  bool mShutdownDone;
  // Map from plugin ID to crash dump ID. Protected by mBrowserCrashDumpHashLock.
  nsDataHashtable<nsUint32HashKey, nsString> mBrowserCrashDumpIds;
  Mutex mBrowserCrashDumpHashLock;
  mozilla::ipc::TaskFactory<HangMonitorParent> mMainThreadTaskFactory;

  static bool sShouldPaintWhileInterruptingJS;
};

bool HangMonitorParent::sShouldPaintWhileInterruptingJS = true;

} // namespace

/* HangMonitorChild implementation */

HangMonitorChild::HangMonitorChild(ProcessHangMonitor* aMonitor)
 : mHangMonitor(aMonitor),
   // Ordering of this atomic is not preserved while recording/replaying, as it
   // may be accessed during the JS interrupt callback.
   mMonitor("HangMonitorChild lock", recordreplay::Behavior::DontPreserve),
   mSentReport(false),
   mTerminateScript(false),
   mTerminateGlobal(false),
   mStartDebugger(false),
   mFinishedStartingDebugger(false),
   mPaintWhileInterruptingJS(false),
   mPaintWhileInterruptingJSForce(false),
   mShutdownDone(false),
   mIPCOpen(true)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mContext = danger::GetJSContext();
  mPaintWhileInterruptingJSMonitor =
    MakeUnique<mozilla::BackgroundHangMonitor>("Gecko_Child_ForcePaint",
                                               128, /* ms timeout for microhangs */
                                               1024, /* ms timeout for permahangs */
                                               BackgroundHangMonitor::THREAD_PRIVATE);
}

HangMonitorChild::~HangMonitorChild()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance == this);
  mPaintWhileInterruptingJSMonitor = nullptr;
  sInstance = nullptr;
}

void
HangMonitorChild::InterruptCallback()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  bool paintWhileInterruptingJS;
  bool paintWhileInterruptingJSForce;
  TabId paintWhileInterruptingJSTab;
  uint64_t paintWhileInterruptingJSEpoch;

  {
    MonitorAutoLock lock(mMonitor);
    paintWhileInterruptingJS = mPaintWhileInterruptingJS;
    paintWhileInterruptingJSForce = mPaintWhileInterruptingJSForce;
    paintWhileInterruptingJSTab = mPaintWhileInterruptingJSTab;
    paintWhileInterruptingJSEpoch = mPaintWhileInterruptingJSEpoch;

    mPaintWhileInterruptingJS = false;
  }

  // Don't paint from the interrupt callback when recording or replaying, as
  // the interrupt callback is triggered non-deterministically.
  if (paintWhileInterruptingJS && !recordreplay::IsRecordingOrReplaying()) {
    RefPtr<TabChild> tabChild = TabChild::FindTabChild(paintWhileInterruptingJSTab);
    if (tabChild) {
      js::AutoAssertNoContentJS nojs(mContext);
      tabChild->PaintWhileInterruptingJS(paintWhileInterruptingJSEpoch,
                                         paintWhileInterruptingJSForce);
    }
  }
}

void
HangMonitorChild::Shutdown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);
  while (!mShutdownDone) {
    mMonitor.Wait();
  }
}

void
HangMonitorChild::ShutdownOnThread()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void
HangMonitorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  mIPCOpen = false;

  // We use a task here to ensure that IPDL is finished with this
  // HangMonitorChild before it gets deleted on the main thread.
  Dispatch(NewNonOwningRunnableMethod("HangMonitorChild::ShutdownOnThread",
                                      this,
                                      &HangMonitorChild::ShutdownOnThread));
}

mozilla::ipc::IPCResult
HangMonitorChild::RecvTerminateScript(const bool& aTerminateGlobal)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  if (aTerminateGlobal) {
    mTerminateGlobal = true;
  } else {
    mTerminateScript = true;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HangMonitorChild::RecvBeginStartingDebugger()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mStartDebugger = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
HangMonitorChild::RecvEndStartingDebugger()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mFinishedStartingDebugger = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
HangMonitorChild::RecvPaintWhileInterruptingJS(const TabId& aTabId,
                                               const bool& aForceRepaint,
                                               const uint64_t& aLayerObserverEpoch)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  {
    MonitorAutoLock lock(mMonitor);
    MaybeStartPaintWhileInterruptingJS();
    mPaintWhileInterruptingJS = true;
    mPaintWhileInterruptingJSForce = aForceRepaint;
    mPaintWhileInterruptingJSTab = aTabId;
    mPaintWhileInterruptingJSEpoch = aLayerObserverEpoch;
  }

  JS_RequestInterruptCallback(mContext);

  return IPC_OK();
}

void
HangMonitorChild::MaybeStartPaintWhileInterruptingJS()
{
  // See Bug 1449662. The body of this function other than assertions
  // has been temporarily removed to diagnose a tab switch spinner
  // problem.
  if (!NS_IsMainThread()) {
    mMonitor.AssertCurrentThreadOwns();
  }
}

void
HangMonitorChild::ClearPaintWhileInterruptingJS(uint64_t aLayerObserverEpoch)
{
  // See Bug 1449662. The body of this function other than assertions
  // has been temporarily removed to diagnose a tab switch spinner
  // problem.
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());
}

void
HangMonitorChild::Bind(Endpoint<PProcessHangMonitorChild>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  MOZ_ASSERT(!sInstance);
  sInstance = this;

  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

void
HangMonitorChild::NotifySlowScriptAsync(TabId aTabId,
                                        const nsCString& aFileName,
                                        const nsString& aAddonId)
{
  if (mIPCOpen) {
    Unused << SendHangEvidence(SlowScriptData(aTabId, aFileName, aAddonId));
  }
}

HangMonitorChild::SlowScriptAction
HangMonitorChild::NotifySlowScript(nsITabChild* aTabChild,
                                   const char* aFileName,
                                   const nsString& aAddonId)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mSentReport = true;

  {
    MonitorAutoLock lock(mMonitor);

    if (mTerminateScript) {
      mTerminateScript = false;
      return SlowScriptAction::Terminate;
    }

    if (mTerminateGlobal) {
      mTerminateGlobal = false;
      return SlowScriptAction::TerminateGlobal;
    }

    if (mStartDebugger) {
      mStartDebugger = false;
      return SlowScriptAction::StartDebugger;
    }
  }

  TabId id;
  if (aTabChild) {
    RefPtr<TabChild> tabChild = static_cast<TabChild*>(aTabChild);
    id = tabChild->GetTabId();
  }
  nsAutoCString filename(aFileName);

  Dispatch(NewNonOwningRunnableMethod<TabId, nsCString, nsString>(
    "HangMonitorChild::NotifySlowScriptAsync",
    this,
    &HangMonitorChild::NotifySlowScriptAsync,
    id,
    filename, aAddonId));
  return SlowScriptAction::Continue;
}

bool
HangMonitorChild::IsDebuggerStartupComplete()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  if (mFinishedStartingDebugger) {
    mFinishedStartingDebugger = false;
    return true;
  }

  return false;
}

void
HangMonitorChild::NotifyPluginHang(uint32_t aPluginId)
{
  // main thread in the child
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mSentReport = true;

  // bounce to background thread
  Dispatch(NewNonOwningRunnableMethod<uint32_t>(
    "HangMonitorChild::NotifyPluginHangAsync",
    this,
    &HangMonitorChild::NotifyPluginHangAsync,
    aPluginId));
}

void
HangMonitorChild::NotifyPluginHangAsync(uint32_t aPluginId)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  // bounce back to parent on background thread
  if (mIPCOpen) {
    Unused << SendHangEvidence(PluginHangData(aPluginId,
                                              base::GetCurrentProcId()));
  }
}

void
HangMonitorChild::ClearHang()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mSentReport) {
    // bounce to background thread
    Dispatch(NewNonOwningRunnableMethod("HangMonitorChild::ClearHangAsync",
                                        this,
                                        &HangMonitorChild::ClearHangAsync));

    MonitorAutoLock lock(mMonitor);
    mSentReport = false;
    mTerminateScript = false;
    mTerminateGlobal = false;
    mStartDebugger = false;
    mFinishedStartingDebugger = false;
  }
}

void
HangMonitorChild::ClearHangAsync()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  // bounce back to parent on background thread
  if (mIPCOpen) {
    Unused << SendClearHang();
  }
}

/* HangMonitorParent implementation */

HangMonitorParent::HangMonitorParent(ProcessHangMonitor* aMonitor)
 : mHangMonitor(aMonitor),
   mIPCOpen(true),
   mMonitor("HangMonitorParent lock"),
   mShutdownDone(false),
   mBrowserCrashDumpHashLock("mBrowserCrashDumpIds lock"),
   mMainThreadTaskFactory(this)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mReportHangs = mozilla::Preferences::GetBool("dom.ipc.reportProcessHangs", false);

  static bool sInited = false;
  if (!sInited) {
    sInited = true;
    Preferences::AddBoolVarCache(&sShouldPaintWhileInterruptingJS,
                                 "browser.tabs.remote.force-paint", true);
  }
}

HangMonitorParent::~HangMonitorParent()
{
  MutexAutoLock lock(mBrowserCrashDumpHashLock);

  for (auto iter = mBrowserCrashDumpIds.Iter(); !iter.Done(); iter.Next()) {
    nsString crashId = iter.UserData();
    if (!crashId.IsEmpty()) {
      CrashReporter::DeleteMinidumpFilesForID(crashId);
    }
  }
}

void
HangMonitorParent::Shutdown()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  if (mProcess) {
    mProcess->Clear();
    mProcess = nullptr;
  }

  Dispatch(NewNonOwningRunnableMethod("HangMonitorParent::ShutdownOnThread",
                                      this,
                                      &HangMonitorParent::ShutdownOnThread));

  while (!mShutdownDone) {
    mMonitor.Wait();
  }
}

void
HangMonitorParent::ShutdownOnThread()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  // mIPCOpen is only written from this thread, so need need to take the lock
  // here. We'd be shooting ourselves in the foot, because ActorDestroy takes
  // it.
  if (mIPCOpen) {
    Close();
  }

  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void
HangMonitorParent::PaintWhileInterruptingJS(dom::TabParent* aTab,
                                            bool aForceRepaint,
                                            uint64_t aLayerObserverEpoch)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (sShouldPaintWhileInterruptingJS) {
    TabId id = aTab->GetTabId();
    Dispatch(NewNonOwningRunnableMethod<TabId, bool, uint64_t>(
      "HangMonitorParent::PaintWhileInterruptingJSOnThread",
      this,
      &HangMonitorParent::PaintWhileInterruptingJSOnThread,
      id,
      aForceRepaint,
      aLayerObserverEpoch));
  }
}

void
HangMonitorParent::PaintWhileInterruptingJSOnThread(TabId aTabId,
                                                    bool aForceRepaint,
                                                    uint64_t aLayerObserverEpoch)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendPaintWhileInterruptingJS(aTabId, aForceRepaint,
                                           aLayerObserverEpoch);
  }
}

void
HangMonitorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_RELEASE_ASSERT(IsOnThread());
  mIPCOpen = false;
}

void
HangMonitorParent::Bind(Endpoint<PProcessHangMonitorParent>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

void
HangMonitorParent::SendHangNotification(const HangData& aHangData,
                                        const nsString& aBrowserDumpId,
                                        bool aTakeMinidump)
{
  // chrome process, main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if ((aHangData.type() == HangData::TPluginHangData) && aTakeMinidump) {
    // We've been handed a partial minidump; complete it with plugin and
    // content process dumps.
    const PluginHangData& phd = aHangData.get_PluginHangData();

    WeakPtr<HangMonitorParent> self = this;
    std::function<void(nsString)> callback =
      [self, aHangData](nsString aResult) {
        MOZ_RELEASE_ASSERT(NS_IsMainThread());

        if (!self) {
          // Don't report hang since the process has already shut down.
          return;
        }

        self->UpdateMinidump(aHangData.get_PluginHangData().pluginId(),
                       aResult);
        self->OnTakeFullMinidumpComplete(aHangData, aResult);
      };

    plugins::TakeFullMinidump(phd.pluginId(),
                              phd.contentProcessId(),
                              aBrowserDumpId,
                              std::move(callback),
                              true);
  } else {
    // We already have a full minidump; go ahead and use it.
    OnTakeFullMinidumpComplete(aHangData, aBrowserDumpId);
  }
}

void
HangMonitorParent::OnTakeFullMinidumpComplete(const HangData& aHangData,
                                              const nsString& aDumpId)
{
  mProcess->SetHangData(aHangData, aDumpId);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->NotifyObservers(mProcess,
                                   "process-hang-report",
                                   nullptr);
}

void
HangMonitorParent::ClearHangNotification()
{
  // chrome process, main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mProcess->ClearHang();

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->NotifyObservers(mProcess, "clear-hang-report", nullptr);
}

// Take a minidump of the browser process if one wasn't already taken for the
// plugin that caused the hang. Return false if a dump was already available or
// true if new one has been taken.
bool
HangMonitorParent::TakeBrowserMinidump(const PluginHangData& aPhd,
                                       nsString& aCrashId)
{
  MutexAutoLock lock(mBrowserCrashDumpHashLock);
  if (!mBrowserCrashDumpIds.Get(aPhd.pluginId(), &aCrashId)) {
    nsCOMPtr<nsIFile> browserDump;
    if (CrashReporter::TakeMinidump(getter_AddRefs(browserDump), true)) {
      if (!CrashReporter::GetIDFromMinidump(browserDump, aCrashId)
          || aCrashId.IsEmpty()) {
        browserDump->Remove(false);
        NS_WARNING("Failed to generate timely browser stack, "
                   "this is bad for plugin hang analysis!");
      } else {
        mBrowserCrashDumpIds.Put(aPhd.pluginId(), aCrashId);
        return true;
      }
    }
  }

  return false;
}

mozilla::ipc::IPCResult
HangMonitorParent::RecvHangEvidence(const HangData& aHangData)
{
  // chrome process, background thread
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (!mReportHangs) {
    return IPC_OK();
  }

#ifdef XP_WIN
  // Don't report hangs if we're debugging the process. You can comment this
  // line out for testing purposes.
  if (IsDebuggerPresent()) {
    return IPC_OK();
  }
#endif

  // Before we wake up the browser main thread we want to take a
  // browser minidump.
  nsAutoString crashId;
  bool takeMinidump = false;
  if (aHangData.type() == HangData::TPluginHangData) {
    takeMinidump = TakeBrowserMinidump(aHangData.get_PluginHangData(), crashId);
  }

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  NS_DispatchToMainThread(
    mMainThreadTaskFactory.NewRunnableMethod(
      &HangMonitorParent::SendHangNotification, aHangData, crashId,
      takeMinidump));

  return IPC_OK();
}

mozilla::ipc::IPCResult
HangMonitorParent::RecvClearHang()
{
  // chrome process, background thread
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (!mReportHangs) {
    return IPC_OK();
  }

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  NS_DispatchToMainThread(
    mMainThreadTaskFactory.NewRunnableMethod(
      &HangMonitorParent::ClearHangNotification));

  return IPC_OK();
}

void
HangMonitorParent::TerminateScript(bool aTerminateGlobal)
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendTerminateScript(aTerminateGlobal);
  }
}

void
HangMonitorParent::BeginStartingDebugger()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendBeginStartingDebugger();
  }
}

void
HangMonitorParent::EndStartingDebugger()
{
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendEndStartingDebugger();
  }
}

void
HangMonitorParent::CleanupPluginHang(uint32_t aPluginId, bool aRemoveFiles)
{
  MutexAutoLock lock(mBrowserCrashDumpHashLock);
  nsAutoString crashId;
  if (!mBrowserCrashDumpIds.Get(aPluginId, &crashId)) {
    return;
  }
  mBrowserCrashDumpIds.Remove(aPluginId);

  if (aRemoveFiles && !crashId.IsEmpty()) {
    CrashReporter::DeleteMinidumpFilesForID(crashId);
  }
}

void
HangMonitorParent::UpdateMinidump(uint32_t aPluginId, const nsString& aDumpId)
{
  if (aDumpId.IsEmpty()) {
    return;
  }

  MutexAutoLock lock(mBrowserCrashDumpHashLock);
  mBrowserCrashDumpIds.Put(aPluginId, aDumpId);
}

/* HangMonitoredProcess implementation */

NS_IMPL_ISUPPORTS(HangMonitoredProcess, nsIHangReport)

NS_IMETHODIMP
HangMonitoredProcess::GetHangType(uint32_t* aHangType)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  switch (mHangData.type()) {
   case HangData::TSlowScriptData:
    *aHangType = SLOW_SCRIPT;
    break;
   case HangData::TPluginHangData:
    *aHangType = PLUGIN_HANG;
    break;
   default:
    MOZ_ASSERT_UNREACHABLE("Unexpected HangData type");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetScriptBrowser(Element** aBrowser)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  TabId tabId = mHangData.get_SlowScriptData().tabId();
  if (!mContentParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<PBrowserParent*> tabs;
  mContentParent->ManagedPBrowserParent(tabs);
  for (size_t i = 0; i < tabs.Length(); i++) {
    TabParent* tp = TabParent::GetFrom(tabs[i]);
    if (tp->GetTabId() == tabId) {
      RefPtr<Element> node = tp->GetOwnerElement();
      node.forget(aBrowser);
      return NS_OK;
    }
  }

  *aBrowser = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetScriptFileName(nsACString& aFileName)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aFileName = mHangData.get_SlowScriptData().filename();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetAddonId(nsAString& aAddonId)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aAddonId = mHangData.get_SlowScriptData().addonId();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetPluginName(nsACString& aPluginName)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TPluginHangData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t id = mHangData.get_PluginHangData().pluginId();

  RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
  nsPluginTag* tag = host->PluginWithId(id);
  if (!tag) {
    return NS_ERROR_UNEXPECTED;
  }

  aPluginName = tag->Name();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminateScript()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(
    NewNonOwningRunnableMethod<bool>("HangMonitorParent::TerminateScript",
                                     mActor,
                                     &HangMonitorParent::TerminateScript, false));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminateGlobal()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(
    NewNonOwningRunnableMethod<bool>("HangMonitorParent::TerminateScript",
                                     mActor,
                                     &HangMonitorParent::TerminateScript, true));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::BeginStartingDebugger()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(
    NewNonOwningRunnableMethod("HangMonitorParent::BeginStartingDebugger",
                               mActor,
                               &HangMonitorParent::BeginStartingDebugger));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::EndStartingDebugger()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(
    NewNonOwningRunnableMethod("HangMonitorParent::EndStartingDebugger",
                               mActor,
                               &HangMonitorParent::EndStartingDebugger));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminatePlugin()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TPluginHangData) {
    return NS_ERROR_UNEXPECTED;
  }

  // Use the multi-process crash report generated earlier.
  uint32_t id = mHangData.get_PluginHangData().pluginId();
  base::ProcessId contentPid = mHangData.get_PluginHangData().contentProcessId();

  RefPtr<HangMonitoredProcess> self{this};
  std::function<void(bool)> callback =
    [self, id](bool aResult) {
      if (self->mActor) {
        self->mActor->CleanupPluginHang(id, false);
      }
    };

  plugins::TerminatePlugin(id,
                           contentPid,
                           NS_LITERAL_CSTRING("HangMonitor"),
                           mDumpId,
                           std::move(callback));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::IsReportForBrowser(nsFrameLoader* aFrameLoader, bool* aResult)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mActor) {
    *aResult = false;
    return NS_OK;
  }

  NS_ENSURE_STATE(aFrameLoader);

  TabParent* tp = TabParent::GetFrom(aFrameLoader);
  if (!tp) {
    *aResult = false;
    return NS_OK;
  }

  *aResult = mContentParent == tp->Manager();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::UserCanceled()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TPluginHangData) {
    return NS_OK;
  }

  if (mActor) {
    uint32_t id = mHangData.get_PluginHangData().pluginId();
    mActor->CleanupPluginHang(id, true);
  }
  return NS_OK;
}

static bool
InterruptCallback(JSContext* cx)
{
  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->InterruptCallback();
  }

  return true;
}

ProcessHangMonitor* ProcessHangMonitor::sInstance;

ProcessHangMonitor::ProcessHangMonitor()
 : mCPOWTimeout(false)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, "xpcom-shutdown", false);
  }

  if (NS_FAILED(NS_NewNamedThread("ProcessHangMon", getter_AddRefs(mThread)))) {
    mThread = nullptr;
  }
}

ProcessHangMonitor::~ProcessHangMonitor()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;

  mThread->Shutdown();
  mThread = nullptr;
}

ProcessHangMonitor*
ProcessHangMonitor::GetOrCreate()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    sInstance = new ProcessHangMonitor();
  }
  return sInstance;
}

NS_IMPL_ISUPPORTS(ProcessHangMonitor, nsIObserver)

NS_IMETHODIMP
ProcessHangMonitor::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    if (HangMonitorChild* child = HangMonitorChild::Get()) {
      child->Shutdown();
      delete child;
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
    }
  }
  return NS_OK;
}

ProcessHangMonitor::SlowScriptAction
ProcessHangMonitor::NotifySlowScript(nsITabChild* aTabChild,
                                     const char* aFileName,
                                     const nsString& aAddonId)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->NotifySlowScript(aTabChild, aFileName, aAddonId);
}

bool
ProcessHangMonitor::IsDebuggerStartupComplete()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->IsDebuggerStartupComplete();
}

bool
ProcessHangMonitor::ShouldTimeOutCPOWs()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mCPOWTimeout) {
    mCPOWTimeout = false;
    return true;
  }
  return false;
}

void
ProcessHangMonitor::InitiateCPOWTimeout()
{
  MOZ_RELEASE_ASSERT(IsOnThread());
  mCPOWTimeout = true;
}

void
ProcessHangMonitor::NotifyPluginHang(uint32_t aPluginId)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->NotifyPluginHang(aPluginId);
}

static PProcessHangMonitorParent*
CreateHangMonitorParent(ContentParent* aContentParent,
                        Endpoint<PProcessHangMonitorParent>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  auto* parent = new HangMonitorParent(monitor);

  auto* process = new HangMonitoredProcess(parent, aContentParent);
  parent->SetProcess(process);

  monitor->Dispatch(
    NewNonOwningRunnableMethod<Endpoint<PProcessHangMonitorParent>&&>(
      "HangMonitorParent::Bind",
      parent,
      &HangMonitorParent::Bind,
      std::move(aEndpoint)));

  return parent;
}

void
mozilla::CreateHangMonitorChild(Endpoint<PProcessHangMonitorChild>&& aEndpoint)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  JSContext* cx = danger::GetJSContext();
  JS_AddInterruptCallback(cx, InterruptCallback);

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  auto* child = new HangMonitorChild(monitor);

  monitor->Dispatch(
    NewNonOwningRunnableMethod<Endpoint<PProcessHangMonitorChild>&&>(
      "HangMonitorChild::Bind",
      child,
      &HangMonitorChild::Bind,
      std::move(aEndpoint)));
}

void
ProcessHangMonitor::Dispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  mThread->Dispatch(std::move(aRunnable), nsIEventTarget::NS_DISPATCH_NORMAL);
}

bool
ProcessHangMonitor::IsOnThread()
{
  bool on;
  return NS_SUCCEEDED(mThread->IsOnCurrentThread(&on)) && on;
}

/* static */ PProcessHangMonitorParent*
ProcessHangMonitor::AddProcess(ContentParent* aContentParent)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mozilla::Preferences::GetBool("dom.ipc.processHangMonitor", false)) {
    return nullptr;
  }

  Endpoint<PProcessHangMonitorParent> parent;
  Endpoint<PProcessHangMonitorChild> child;
  nsresult rv;
  rv = PProcessHangMonitor::CreateEndpoints(base::GetCurrentProcId(),
                                            aContentParent->OtherPid(),
                                            &parent, &child);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "PProcessHangMonitor::CreateEndpoints failed");
    return nullptr;
  }

  if (!aContentParent->SendInitProcessHangMonitor(std::move(child))) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  return CreateHangMonitorParent(aContentParent, std::move(parent));
}

/* static */ void
ProcessHangMonitor::RemoveProcess(PProcessHangMonitorParent* aParent)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  auto parent = static_cast<HangMonitorParent*>(aParent);
  parent->Shutdown();
  delete parent;
}

/* static */ void
ProcessHangMonitor::ClearHang()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->ClearHang();
  }
}

/* static */ void
ProcessHangMonitor::PaintWhileInterruptingJS(PProcessHangMonitorParent* aParent,
                                             dom::TabParent* aTabParent,
                                             bool aForceRepaint,
                                             uint64_t aLayerObserverEpoch)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  auto parent = static_cast<HangMonitorParent*>(aParent);
  parent->PaintWhileInterruptingJS(aTabParent, aForceRepaint, aLayerObserverEpoch);
}

/* static */ void
ProcessHangMonitor::ClearPaintWhileInterruptingJS(uint64_t aLayerObserverEpoch)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());

  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->ClearPaintWhileInterruptingJS(aLayerObserverEpoch);
  }
}

/* static */ void
ProcessHangMonitor::MaybeStartPaintWhileInterruptingJS()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());

  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->MaybeStartPaintWhileInterruptingJS();
  }
}
