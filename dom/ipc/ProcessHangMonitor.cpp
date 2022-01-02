/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"

#include "jsapi.h"
#include "xpcprivate.h"

#include "mozilla/Atomics.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/TaskFactory.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"

#include "nsExceptionHandler.h"
#include "nsFrameLoader.h"
#include "nsIHangReport.h"
#include "nsIRemoteTab.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsPluginHost.h"
#include "nsThreadUtils.h"

#include "base/task.h"
#include "base/thread.h"

#ifdef XP_WIN
// For IsDebuggerPresent()
#  include <windows.h>
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

class HangMonitorChild : public PProcessHangMonitorChild,
                         public BackgroundHangAnnotator {
 public:
  explicit HangMonitorChild(ProcessHangMonitor* aMonitor);
  ~HangMonitorChild() override;

  void Bind(Endpoint<PProcessHangMonitorChild>&& aEndpoint);

  using SlowScriptAction = ProcessHangMonitor::SlowScriptAction;
  SlowScriptAction NotifySlowScript(nsIBrowserChild* aBrowserChild,
                                    const char* aFileName,
                                    const nsString& aAddonId,
                                    const double aDuration);
  void NotifySlowScriptAsync(TabId aTabId, const nsCString& aFileName,
                             const nsString& aAddonId, const double aDuration);

  bool IsDebuggerStartupComplete();

  void ClearHang();
  void ClearHangAsync();
  void ClearPaintWhileInterruptingJS(const LayersObserverEpoch& aEpoch);

  // MaybeStartPaintWhileInterruptingJS will notify the background hang monitor
  // of activity if this is the first time calling it since
  // ClearPaintWhileInterruptingJS. It should be callable from any thread, but
  // you must be holding mMonitor if using it off the main thread, since it
  // could race with ClearPaintWhileInterruptingJS.
  void MaybeStartPaintWhileInterruptingJS();

  mozilla::ipc::IPCResult RecvTerminateScript() override;
  mozilla::ipc::IPCResult RecvBeginStartingDebugger() override;
  mozilla::ipc::IPCResult RecvEndStartingDebugger() override;

  mozilla::ipc::IPCResult RecvPaintWhileInterruptingJS(
      const TabId& aTabId, const LayersObserverEpoch& aEpoch) override;

  mozilla::ipc::IPCResult RecvCancelContentJSExecutionIfRunning(
      const TabId& aTabId, const nsIRemoteTab::NavigationType& aNavigationType,
      const int32_t& aNavigationIndex,
      const mozilla::Maybe<nsCString>& aNavigationURI,
      const int32_t& aEpoch) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  bool InterruptCallback();
  void Shutdown();

  static HangMonitorChild* Get() { return sInstance; }

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable) {
    mHangMonitor->Dispatch(std::move(aRunnable));
  }
  bool IsOnThread() { return mHangMonitor->IsOnThread(); }

  void AnnotateHang(BackgroundHangAnnotations& aAnnotations) override;

 protected:
  friend class mozilla::ProcessHangMonitor;
  static Maybe<Monitor> sMonitor;

  static Atomic<bool, SequentiallyConsistent> sInitializing;

 private:
  void ShutdownOnThread();

  static Atomic<HangMonitorChild*, SequentiallyConsistent> sInstance;

  const RefPtr<ProcessHangMonitor> mHangMonitor;
  Monitor mMonitor;

  // Main thread-only.
  bool mSentReport;

  // These fields must be accessed with mMonitor held.
  bool mTerminateScript;
  bool mStartDebugger;
  bool mFinishedStartingDebugger;
  bool mPaintWhileInterruptingJS;
  TabId mPaintWhileInterruptingJSTab;
  MOZ_INIT_OUTSIDE_CTOR LayersObserverEpoch mPaintWhileInterruptingJSEpoch;
  bool mCancelContentJS;
  TabId mCancelContentJSTab;
  nsIRemoteTab::NavigationType mCancelContentJSNavigationType;
  int32_t mCancelContentJSNavigationIndex;
  mozilla::Maybe<nsCString> mCancelContentJSNavigationURI;
  int32_t mCancelContentJSEpoch;
  JSContext* mContext;
  bool mShutdownDone;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;

  // Allows us to ensure we NotifyActivity only once, allowing
  // either thread to do so.
  Atomic<bool> mPaintWhileInterruptingJSActive;
};

Maybe<Monitor> HangMonitorChild::sMonitor;

Atomic<bool, SequentiallyConsistent> HangMonitorChild::sInitializing;

Atomic<HangMonitorChild*, SequentiallyConsistent> HangMonitorChild::sInstance;

/* Parent process objects */

class HangMonitorParent;

class HangMonitoredProcess final : public nsIHangReport {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  HangMonitoredProcess(HangMonitorParent* aActor, ContentParent* aContentParent)
      : mActor(aActor), mContentParent(aContentParent) {}

  NS_DECL_NSIHANGREPORT

  // Called when a content process shuts down.
  void Clear() {
    mContentParent = nullptr;
    mActor = nullptr;
  }

  /**
   * Sets the information associated with this hang: this includes the tab ID,
   * filename, duration, and an add-on ID if it was caused by an add-on.
   *
   * @param aDumpId The ID of a minidump taken when the hang occurred
   */
  void SetSlowScriptData(const SlowScriptData& aSlowScriptData,
                         const nsAString& aDumpId) {
    mSlowScriptData = aSlowScriptData;
    mDumpId = aDumpId;
  }

  void ClearHang() {
    mSlowScriptData = SlowScriptData();
    mDumpId.Truncate();
  }

 private:
  ~HangMonitoredProcess() = default;

  // Everything here is main thread-only.
  HangMonitorParent* mActor;
  ContentParent* mContentParent;
  SlowScriptData mSlowScriptData;
  nsAutoString mDumpId;
};

class HangMonitorParent : public PProcessHangMonitorParent,
                          public SupportsWeakPtr {
 public:
  explicit HangMonitorParent(ProcessHangMonitor* aMonitor);
  ~HangMonitorParent() override;

  void Bind(Endpoint<PProcessHangMonitorParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvHangEvidence(
      const SlowScriptData& aSlowScriptData) override;
  mozilla::ipc::IPCResult RecvClearHang() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void SetProcess(HangMonitoredProcess* aProcess) { mProcess = aProcess; }

  void Shutdown();

  void PaintWhileInterruptingJS(dom::BrowserParent* aBrowserParent,
                                const LayersObserverEpoch& aEpoch);
  void CancelContentJSExecutionIfRunning(
      dom::BrowserParent* aBrowserParent,
      nsIRemoteTab::NavigationType aNavigationType,
      const dom::CancelContentJSOptions& aCancelContentJSOptions);

  void TerminateScript();
  void BeginStartingDebugger();
  void EndStartingDebugger();

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable) {
    mHangMonitor->Dispatch(std::move(aRunnable));
  }
  bool IsOnThread() { return mHangMonitor->IsOnThread(); }

 private:
  void SendHangNotification(const SlowScriptData& aSlowScriptData,
                            const nsString& aBrowserDumpId);

  void ClearHangNotification();

  void PaintWhileInterruptingJSOnThread(TabId aTabId,
                                        const LayersObserverEpoch& aEpoch);
  void CancelContentJSExecutionIfRunningOnThread(
      TabId aTabId, nsIRemoteTab::NavigationType aNavigationType,
      int32_t aNavigationIndex, nsIURI* aNavigationURI, int32_t aEpoch);

  void ShutdownOnThread();

  const RefPtr<ProcessHangMonitor> mHangMonitor;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;

  Monitor mMonitor;

  // Must be accessed with mMonitor held.
  RefPtr<HangMonitoredProcess> mProcess;
  bool mShutdownDone;
  // Map from plugin ID to crash dump ID. Protected by
  // mBrowserCrashDumpHashLock.
  nsTHashMap<nsUint32HashKey, nsString> mBrowserCrashDumpIds;
  Mutex mBrowserCrashDumpHashLock;
  mozilla::ipc::TaskFactory<HangMonitorParent> mMainThreadTaskFactory;
};

}  // namespace

/* HangMonitorChild implementation */

HangMonitorChild::HangMonitorChild(ProcessHangMonitor* aMonitor)
    : mHangMonitor(aMonitor),
      mMonitor("HangMonitorChild lock"),
      mSentReport(false),
      mTerminateScript(false),
      mStartDebugger(false),
      mFinishedStartingDebugger(false),
      mPaintWhileInterruptingJS(false),
      mCancelContentJS(false),
      mCancelContentJSNavigationType(nsIRemoteTab::NAVIGATE_BACK),
      mCancelContentJSNavigationIndex(0),
      mCancelContentJSEpoch(0),
      mShutdownDone(false),
      mIPCOpen(true),
      mPaintWhileInterruptingJSActive(false) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInstance);
  mContext = danger::GetJSContext();

  BackgroundHangMonitor::RegisterAnnotator(*this);

  MOZ_ASSERT(!sMonitor.isSome());
  sMonitor.emplace("HangMonitorChild::sMonitor");
  MonitorAutoLock mal(*sMonitor);

  MOZ_ASSERT(!sInitializing);
  sInitializing = true;
}

HangMonitorChild::~HangMonitorChild() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;
}

bool HangMonitorChild::InterruptCallback() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Don't start painting if we're not in a good place to run script. We run
  // chrome script during layout and such, and it wouldn't be good to interrupt
  // painting code from there.
  if (!nsContentUtils::IsSafeToRunScript()) {
    return true;
  }

  bool paintWhileInterruptingJS;
  TabId paintWhileInterruptingJSTab;
  LayersObserverEpoch paintWhileInterruptingJSEpoch;

  {
    MonitorAutoLock lock(mMonitor);
    paintWhileInterruptingJS = mPaintWhileInterruptingJS;
    paintWhileInterruptingJSTab = mPaintWhileInterruptingJSTab;
    paintWhileInterruptingJSEpoch = mPaintWhileInterruptingJSEpoch;

    mPaintWhileInterruptingJS = false;
  }

  if (paintWhileInterruptingJS) {
    RefPtr<BrowserChild> browserChild =
        BrowserChild::FindBrowserChild(paintWhileInterruptingJSTab);
    if (browserChild) {
      js::AutoAssertNoContentJS nojs(mContext);
      browserChild->PaintWhileInterruptingJS(paintWhileInterruptingJSEpoch);
    }
  }

  // Only handle the interrupt for cancelling content JS if we have a
  // non-privileged script (i.e. not part of Gecko or an add-on).
  JS::RootedObject global(mContext, JS::CurrentGlobalOrNull(mContext));
  nsIPrincipal* principal = xpc::GetObjectPrincipal(global);
  if (principal && (principal->IsSystemPrincipal() ||
                    principal->GetIsAddonOrExpandedAddonPrincipal())) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = xpc::WindowOrNull(global);
  if (!win) {
    return true;
  }

  bool cancelContentJS;
  TabId cancelContentJSTab;
  nsIRemoteTab::NavigationType cancelContentJSNavigationType;
  int32_t cancelContentJSNavigationIndex;
  mozilla::Maybe<nsCString> cancelContentJSNavigationURI;
  int32_t cancelContentJSEpoch;

  {
    MonitorAutoLock lock(mMonitor);
    cancelContentJS = mCancelContentJS;
    cancelContentJSTab = mCancelContentJSTab;
    cancelContentJSNavigationType = mCancelContentJSNavigationType;
    cancelContentJSNavigationIndex = mCancelContentJSNavigationIndex;
    cancelContentJSNavigationURI = std::move(mCancelContentJSNavigationURI);
    cancelContentJSEpoch = mCancelContentJSEpoch;

    mCancelContentJS = false;
  }

  if (cancelContentJS) {
    js::AutoAssertNoContentJS nojs(mContext);

    RefPtr<BrowserChild> browserChild =
        BrowserChild::FindBrowserChild(cancelContentJSTab);
    RefPtr<BrowserChild> browserChildFromWin = BrowserChild::GetFrom(win);
    if (!browserChild || !browserChildFromWin) {
      return true;
    }

    TabId tabIdFromWin = browserChildFromWin->GetTabId();
    if (tabIdFromWin != cancelContentJSTab) {
      // The currently-executing content JS doesn't belong to the tab that
      // requested cancellation of JS. Just return and let the JS continue.
      return true;
    }

    nsresult rv;
    nsCOMPtr<nsIURI> uri;

    if (cancelContentJSNavigationURI) {
      rv = NS_NewURI(getter_AddRefs(uri), cancelContentJSNavigationURI.value());
      if (NS_FAILED(rv)) {
        return true;
      }
    }

    bool canCancel;
    rv = browserChild->CanCancelContentJS(cancelContentJSNavigationType,
                                          cancelContentJSNavigationIndex, uri,
                                          cancelContentJSEpoch, &canCancel);
    if (NS_SUCCEEDED(rv) && canCancel) {
      // Don't add this page to the BF cache, since we're cancelling its JS.
      if (Document* doc = win->GetExtantDoc()) {
        doc->DisallowBFCaching();
      }

      return false;
    }
  }

  return true;
}

void HangMonitorChild::AnnotateHang(BackgroundHangAnnotations& aAnnotations) {
  if (mPaintWhileInterruptingJSActive) {
    aAnnotations.AddAnnotation(u"PaintWhileInterruptingJS"_ns, true);
  }
}

void HangMonitorChild::Shutdown() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  BackgroundHangMonitor::UnregisterAnnotator(*this);

  MonitorAutoLock lock(mMonitor);
  while (!mShutdownDone) {
    mMonitor.Wait();
  }
}

void HangMonitorChild::ShutdownOnThread() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void HangMonitorChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  mIPCOpen = false;

  // We use a task here to ensure that IPDL is finished with this
  // HangMonitorChild before it gets deleted on the main thread.
  Dispatch(NewNonOwningRunnableMethod("HangMonitorChild::ShutdownOnThread",
                                      this,
                                      &HangMonitorChild::ShutdownOnThread));
}

mozilla::ipc::IPCResult HangMonitorChild::RecvTerminateScript() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mTerminateScript = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult HangMonitorChild::RecvBeginStartingDebugger() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mStartDebugger = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult HangMonitorChild::RecvEndStartingDebugger() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock lock(mMonitor);
  mFinishedStartingDebugger = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult HangMonitorChild::RecvPaintWhileInterruptingJS(
    const TabId& aTabId, const LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  {
    MonitorAutoLock lock(mMonitor);
    MaybeStartPaintWhileInterruptingJS();
    mPaintWhileInterruptingJS = true;
    mPaintWhileInterruptingJSTab = aTabId;
    mPaintWhileInterruptingJSEpoch = aEpoch;
  }

  JS_RequestInterruptCallback(mContext);

  return IPC_OK();
}

void HangMonitorChild::MaybeStartPaintWhileInterruptingJS() {
  mPaintWhileInterruptingJSActive = true;
}

void HangMonitorChild::ClearPaintWhileInterruptingJS(
    const LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());
  mPaintWhileInterruptingJSActive = false;
}

mozilla::ipc::IPCResult HangMonitorChild::RecvCancelContentJSExecutionIfRunning(
    const TabId& aTabId, const nsIRemoteTab::NavigationType& aNavigationType,
    const int32_t& aNavigationIndex,
    const mozilla::Maybe<nsCString>& aNavigationURI, const int32_t& aEpoch) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  {
    MonitorAutoLock lock(mMonitor);
    mCancelContentJS = true;
    mCancelContentJSTab = aTabId;
    mCancelContentJSNavigationType = aNavigationType;
    mCancelContentJSNavigationIndex = aNavigationIndex;
    mCancelContentJSNavigationURI = aNavigationURI;
    mCancelContentJSEpoch = aEpoch;
  }

  JS_RequestInterruptCallback(mContext);

  return IPC_OK();
}

void HangMonitorChild::Bind(Endpoint<PProcessHangMonitorChild>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  MonitorAutoLock mal(*sMonitor);

  MOZ_ASSERT(!sInstance);
  sInstance = this;

  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);

  sInitializing = false;
  mal.Notify();
}

void HangMonitorChild::NotifySlowScriptAsync(TabId aTabId,
                                             const nsCString& aFileName,
                                             const nsString& aAddonId,
                                             const double aDuration) {
  if (mIPCOpen) {
    Unused << SendHangEvidence(
        SlowScriptData(aTabId, aFileName, aAddonId, aDuration));
  }
}

HangMonitorChild::SlowScriptAction HangMonitorChild::NotifySlowScript(
    nsIBrowserChild* aBrowserChild, const char* aFileName,
    const nsString& aAddonId, const double aDuration) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mSentReport = true;

  {
    MonitorAutoLock lock(mMonitor);

    if (mTerminateScript) {
      mTerminateScript = false;
      return SlowScriptAction::Terminate;
    }

    if (mStartDebugger) {
      mStartDebugger = false;
      return SlowScriptAction::StartDebugger;
    }
  }

  TabId id;
  if (aBrowserChild) {
    RefPtr<BrowserChild> browserChild =
        static_cast<BrowserChild*>(aBrowserChild);
    id = browserChild->GetTabId();
  }
  nsAutoCString filename(aFileName);

  Dispatch(NewNonOwningRunnableMethod<TabId, nsCString, nsString, double>(
      "HangMonitorChild::NotifySlowScriptAsync", this,
      &HangMonitorChild::NotifySlowScriptAsync, id, filename, aAddonId,
      aDuration));
  return SlowScriptAction::Continue;
}

bool HangMonitorChild::IsDebuggerStartupComplete() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  if (mFinishedStartingDebugger) {
    mFinishedStartingDebugger = false;
    return true;
  }

  return false;
}

void HangMonitorChild::ClearHang() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSentReport) {
    // bounce to background thread
    Dispatch(NewNonOwningRunnableMethod("HangMonitorChild::ClearHangAsync",
                                        this,
                                        &HangMonitorChild::ClearHangAsync));

    MonitorAutoLock lock(mMonitor);
    mSentReport = false;
    mTerminateScript = false;
    mStartDebugger = false;
    mFinishedStartingDebugger = false;
  }
}

void HangMonitorChild::ClearHangAsync() {
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
      mMainThreadTaskFactory(this) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

HangMonitorParent::~HangMonitorParent() {
  MutexAutoLock lock(mBrowserCrashDumpHashLock);

  for (const auto& crashId : mBrowserCrashDumpIds.Values()) {
    if (!crashId.IsEmpty()) {
      CrashReporter::DeleteMinidumpFilesForID(crashId);
    }
  }
}

void HangMonitorParent::Shutdown() {
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

void HangMonitorParent::ShutdownOnThread() {
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

void HangMonitorParent::PaintWhileInterruptingJS(
    dom::BrowserParent* aTab, const LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (StaticPrefs::browser_tabs_remote_force_paint()) {
    TabId id = aTab->GetTabId();
    Dispatch(NewNonOwningRunnableMethod<TabId, LayersObserverEpoch>(
        "HangMonitorParent::PaintWhileInterruptingJSOnThread", this,
        &HangMonitorParent::PaintWhileInterruptingJSOnThread, id, aEpoch));
  }
}

void HangMonitorParent::PaintWhileInterruptingJSOnThread(
    TabId aTabId, const LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendPaintWhileInterruptingJS(aTabId, aEpoch);
  }
}

void HangMonitorParent::CancelContentJSExecutionIfRunning(
    dom::BrowserParent* aBrowserParent,
    nsIRemoteTab::NavigationType aNavigationType,
    const dom::CancelContentJSOptions& aCancelContentJSOptions) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!aBrowserParent->CanCancelContentJS(aNavigationType,
                                          aCancelContentJSOptions.mIndex,
                                          aCancelContentJSOptions.mUri)) {
    return;
  }

  TabId id = aBrowserParent->GetTabId();
  Dispatch(NewNonOwningRunnableMethod<TabId, nsIRemoteTab::NavigationType,
                                      int32_t, nsIURI*, int32_t>(
      "HangMonitorParent::CancelContentJSExecutionIfRunningOnThread", this,
      &HangMonitorParent::CancelContentJSExecutionIfRunningOnThread, id,
      aNavigationType, aCancelContentJSOptions.mIndex,
      aCancelContentJSOptions.mUri, aCancelContentJSOptions.mEpoch));
}

void HangMonitorParent::CancelContentJSExecutionIfRunningOnThread(
    TabId aTabId, nsIRemoteTab::NavigationType aNavigationType,
    int32_t aNavigationIndex, nsIURI* aNavigationURI, int32_t aEpoch) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  mozilla::Maybe<nsCString> spec;
  if (aNavigationURI) {
    nsAutoCString tmp;
    nsresult rv = aNavigationURI->GetSpec(tmp);
    if (NS_SUCCEEDED(rv)) {
      spec.emplace(tmp);
    }
  }

  if (mIPCOpen) {
    Unused << SendCancelContentJSExecutionIfRunning(
        aTabId, aNavigationType, aNavigationIndex, spec, aEpoch);
  }
}

void HangMonitorParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_RELEASE_ASSERT(IsOnThread());
  mIPCOpen = false;
}

void HangMonitorParent::Bind(Endpoint<PProcessHangMonitorParent>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(IsOnThread());

  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

void HangMonitorParent::SendHangNotification(
    const SlowScriptData& aSlowScriptData, const nsString& aBrowserDumpId) {
  // chrome process, main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsString dumpId;

  // We already have a full minidump; go ahead and use it.
  dumpId = aBrowserDumpId;

  mProcess->SetSlowScriptData(aSlowScriptData, dumpId);

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  observerService->NotifyObservers(mProcess, "process-hang-report", nullptr);
}

void HangMonitorParent::ClearHangNotification() {
  // chrome process, main thread
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  observerService->NotifyObservers(mProcess, "clear-hang-report", nullptr);

  mProcess->ClearHang();
}

mozilla::ipc::IPCResult HangMonitorParent::RecvHangEvidence(
    const SlowScriptData& aSlowScriptData) {
  // chrome process, background thread
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (!StaticPrefs::dom_ipc_reportProcessHangs()) {
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

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  NS_DispatchToMainThread(mMainThreadTaskFactory.NewRunnableMethod(
      &HangMonitorParent::SendHangNotification, aSlowScriptData, crashId));

  return IPC_OK();
}

mozilla::ipc::IPCResult HangMonitorParent::RecvClearHang() {
  // chrome process, background thread
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (!StaticPrefs::dom_ipc_reportProcessHangs()) {
    return IPC_OK();
  }

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  NS_DispatchToMainThread(mMainThreadTaskFactory.NewRunnableMethod(
      &HangMonitorParent::ClearHangNotification));

  return IPC_OK();
}

void HangMonitorParent::TerminateScript() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendTerminateScript();
  }
}

void HangMonitorParent::BeginStartingDebugger() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendBeginStartingDebugger();
  }
}

void HangMonitorParent::EndStartingDebugger() {
  MOZ_RELEASE_ASSERT(IsOnThread());

  if (mIPCOpen) {
    Unused << SendEndStartingDebugger();
  }
}

/* HangMonitoredProcess implementation */

NS_IMPL_ISUPPORTS(HangMonitoredProcess, nsIHangReport)

NS_IMETHODIMP
HangMonitoredProcess::GetHangDuration(double* aHangDuration) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  *aHangDuration = mSlowScriptData.duration();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetScriptBrowser(Element** aBrowser) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  TabId tabId = mSlowScriptData.tabId();
  if (!mContentParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<PBrowserParent*> tabs;
  mContentParent->ManagedPBrowserParent(tabs);
  for (size_t i = 0; i < tabs.Length(); i++) {
    BrowserParent* tp = BrowserParent::GetFrom(tabs[i]);
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
HangMonitoredProcess::GetScriptFileName(nsACString& aFileName) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  aFileName = mSlowScriptData.filename();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::GetAddonId(nsAString& aAddonId) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  aAddonId = mSlowScriptData.addonId();
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminateScript() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(
      NewNonOwningRunnableMethod("HangMonitorParent::TerminateScript", mActor,
                                 &HangMonitorParent::TerminateScript));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::BeginStartingDebugger() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(NewNonOwningRunnableMethod(
      "HangMonitorParent::BeginStartingDebugger", mActor,
      &HangMonitorParent::BeginStartingDebugger));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::EndStartingDebugger() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!mActor) {
    return NS_ERROR_UNEXPECTED;
  }

  ProcessHangMonitor::Get()->Dispatch(NewNonOwningRunnableMethod(
      "HangMonitorParent::EndStartingDebugger", mActor,
      &HangMonitorParent::EndStartingDebugger));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::IsReportForBrowserOrChildren(nsFrameLoader* aFrameLoader,
                                                   bool* aResult) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());

  if (!mActor) {
    *aResult = false;
    return NS_OK;
  }

  NS_ENSURE_STATE(aFrameLoader);

  AutoTArray<RefPtr<BrowsingContext>, 10> bcs;
  bcs.AppendElement(aFrameLoader->GetExtantBrowsingContext());
  while (!bcs.IsEmpty()) {
    RefPtr<BrowsingContext> bc = bcs[bcs.Length() - 1];
    bcs.RemoveLastElement();
    if (!bc) {
      continue;
    }
    if (mContentParent == bc->Canonical()->GetContentParent()) {
      *aResult = true;
      return NS_OK;
    }
    bc->GetChildren(bcs);
  }

  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::UserCanceled() { return NS_OK; }

NS_IMETHODIMP
HangMonitoredProcess::GetChildID(uint64_t* aChildID) {
  if (!mContentParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aChildID = mContentParent->ChildID();
  return NS_OK;
}

static bool InterruptCallback(JSContext* cx) {
  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    return child->InterruptCallback();
  }

  return true;
}

ProcessHangMonitor* ProcessHangMonitor::sInstance;

ProcessHangMonitor::ProcessHangMonitor() : mCPOWTimeout(false) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, "xpcom-shutdown", false);
  }

  if (NS_FAILED(NS_NewNamedThread("ProcessHangMon", getter_AddRefs(mThread)))) {
    mThread = nullptr;
  }
}

ProcessHangMonitor::~ProcessHangMonitor() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;

  mThread->Shutdown();
  mThread = nullptr;
}

ProcessHangMonitor* ProcessHangMonitor::GetOrCreate() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    sInstance = new ProcessHangMonitor();
  }
  return sInstance;
}

NS_IMPL_ISUPPORTS(ProcessHangMonitor, nsIObserver)

NS_IMETHODIMP
ProcessHangMonitor::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    if (HangMonitorChild::sMonitor) {
      MonitorAutoLock mal(*HangMonitorChild::sMonitor);
      if (HangMonitorChild::sInitializing) {
        mal.Wait();
      }

      if (HangMonitorChild* child = HangMonitorChild::Get()) {
        child->Shutdown();
        delete child;
      }
    }
    HangMonitorChild::sMonitor.reset();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, "xpcom-shutdown");
  }
  return NS_OK;
}

ProcessHangMonitor::SlowScriptAction ProcessHangMonitor::NotifySlowScript(
    nsIBrowserChild* aBrowserChild, const char* aFileName,
    const nsString& aAddonId, const double aDuration) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->NotifySlowScript(aBrowserChild, aFileName,
                                                   aAddonId, aDuration);
}

bool ProcessHangMonitor::IsDebuggerStartupComplete() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->IsDebuggerStartupComplete();
}

bool ProcessHangMonitor::ShouldTimeOutCPOWs() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mCPOWTimeout) {
    mCPOWTimeout = false;
    return true;
  }
  return false;
}

void ProcessHangMonitor::InitiateCPOWTimeout() {
  MOZ_RELEASE_ASSERT(IsOnThread());
  mCPOWTimeout = true;
}

static PProcessHangMonitorParent* CreateHangMonitorParent(
    ContentParent* aContentParent,
    Endpoint<PProcessHangMonitorParent>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  auto* parent = new HangMonitorParent(monitor);

  auto* process = new HangMonitoredProcess(parent, aContentParent);
  parent->SetProcess(process);

  monitor->Dispatch(
      NewNonOwningRunnableMethod<Endpoint<PProcessHangMonitorParent>&&>(
          "HangMonitorParent::Bind", parent, &HangMonitorParent::Bind,
          std::move(aEndpoint)));

  return parent;
}

void mozilla::CreateHangMonitorChild(
    Endpoint<PProcessHangMonitorChild>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  JSContext* cx = danger::GetJSContext();
  JS_AddInterruptCallback(cx, InterruptCallback);

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  auto* child = new HangMonitorChild(monitor);

  monitor->Dispatch(
      NewNonOwningRunnableMethod<Endpoint<PProcessHangMonitorChild>&&>(
          "HangMonitorChild::Bind", child, &HangMonitorChild::Bind,
          std::move(aEndpoint)));
}

void ProcessHangMonitor::Dispatch(already_AddRefed<nsIRunnable> aRunnable) {
  mThread->Dispatch(std::move(aRunnable), nsIEventTarget::NS_DISPATCH_NORMAL);
}

bool ProcessHangMonitor::IsOnThread() {
  bool on;
  return NS_SUCCEEDED(mThread->IsOnCurrentThread(&on)) && on;
}

/* static */
PProcessHangMonitorParent* ProcessHangMonitor::AddProcess(
    ContentParent* aContentParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::dom_ipc_processHangMonitor_AtStartup()) {
    return nullptr;
  }

  Endpoint<PProcessHangMonitorParent> parent;
  Endpoint<PProcessHangMonitorChild> child;
  nsresult rv;
  rv = PProcessHangMonitor::CreateEndpoints(&parent, &child);
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

/* static */
void ProcessHangMonitor::RemoveProcess(PProcessHangMonitorParent* aParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  auto parent = static_cast<HangMonitorParent*>(aParent);
  parent->Shutdown();
  delete parent;
}

/* static */
void ProcessHangMonitor::ClearHang() {
  MOZ_ASSERT(NS_IsMainThread());
  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->ClearHang();
  }
}

/* static */
void ProcessHangMonitor::PaintWhileInterruptingJS(
    PProcessHangMonitorParent* aParent, dom::BrowserParent* aBrowserParent,
    const layers::LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  auto parent = static_cast<HangMonitorParent*>(aParent);
  parent->PaintWhileInterruptingJS(aBrowserParent, aEpoch);
}

/* static */
void ProcessHangMonitor::ClearPaintWhileInterruptingJS(
    const layers::LayersObserverEpoch& aEpoch) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());

  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->ClearPaintWhileInterruptingJS(aEpoch);
  }
}

/* static */
void ProcessHangMonitor::MaybeStartPaintWhileInterruptingJS() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(XRE_IsContentProcess());

  if (HangMonitorChild* child = HangMonitorChild::Get()) {
    child->MaybeStartPaintWhileInterruptingJS();
  }
}

/* static */
void ProcessHangMonitor::CancelContentJSExecutionIfRunning(
    PProcessHangMonitorParent* aParent, dom::BrowserParent* aBrowserParent,
    nsIRemoteTab::NavigationType aNavigationType,
    const dom::CancelContentJSOptions& aCancelContentJSOptions) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  auto parent = static_cast<HangMonitorParent*>(aParent);
  parent->CancelContentJSExecutionIfRunning(aBrowserParent, aNavigationType,
                                            aCancelContentJSOptions);
}
