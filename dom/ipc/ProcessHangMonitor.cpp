/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProcessHangMonitorIPC.h"

#include "mozilla/Atomics.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Monitor.h"
#include "mozilla/plugins/PluginBridge.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"

#include "nsIFrameLoader.h"
#include "nsIHangReport.h"
#include "nsITabParent.h"
#include "nsPluginHost.h"
#include "nsThreadUtils.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "base/task.h"
#include "base/thread.h"

#ifdef XP_WIN
// For IsDebuggerPresent()
#include <windows.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;

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
  virtual ~HangMonitorChild();

  void Open(Transport* aTransport, ProcessId aOtherPid,
            MessageLoop* aIOLoop);

  typedef ProcessHangMonitor::SlowScriptAction SlowScriptAction;
  SlowScriptAction NotifySlowScript(nsITabChild* aTabChild,
                                    const char* aFileName,
                                    unsigned aLineNo);
  void NotifySlowScriptAsync(TabId aTabId,
                             const nsCString& aFileName,
                             unsigned aLineNo);

  bool IsDebuggerStartupComplete();

  void NotifyPluginHang(uint32_t aPluginId);
  void NotifyPluginHangAsync(uint32_t aPluginId);

  void ClearHang();

  virtual bool RecvTerminateScript() override;
  virtual bool RecvBeginStartingDebugger() override;
  virtual bool RecvEndStartingDebugger() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void Shutdown();

  static HangMonitorChild* Get() { return sInstance; }

  MessageLoop* MonitorLoop() { return mHangMonitor->MonitorLoop(); }

 private:
  void ShutdownOnThread();

  static Atomic<HangMonitorChild*> sInstance;

  const nsRefPtr<ProcessHangMonitor> mHangMonitor;
  Monitor mMonitor;

  // Main thread-only.
  bool mSentReport;

  // These fields must be accessed with mMonitor held.
  bool mTerminateScript;
  bool mStartDebugger;
  bool mFinishedStartingDebugger;
  bool mShutdownDone;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;
};

Atomic<HangMonitorChild*> HangMonitorChild::sInstance;

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

  NS_IMETHOD GetHangType(uint32_t* aHangType) override;
  NS_IMETHOD GetScriptBrowser(nsIDOMElement** aBrowser) override;
  NS_IMETHOD GetScriptFileName(nsACString& aFileName) override;
  NS_IMETHOD GetScriptLineNo(uint32_t* aLineNo) override;

  NS_IMETHOD GetPluginName(nsACString& aPluginName) override;

  NS_IMETHOD TerminateScript() override;
  NS_IMETHOD BeginStartingDebugger() override;
  NS_IMETHOD EndStartingDebugger() override;
  NS_IMETHOD TerminatePlugin() override;
  NS_IMETHOD TerminateProcess() override;
  NS_IMETHOD UserCanceled() override;

  NS_IMETHOD IsReportForBrowser(nsIFrameLoader* aFrameLoader, bool* aResult) override;

  // Called when a content process shuts down.
  void Clear() {
    mContentParent = nullptr;
    mActor = nullptr;
  }

  void SetHangData(const HangData& aHangData) { mHangData = aHangData; }
  void SetBrowserDumpId(nsAutoString& aId) {
    mBrowserDumpId = aId;
  }

private:
  ~HangMonitoredProcess() {}

  // Everything here is main thread-only.
  HangMonitorParent* mActor;
  ContentParent* mContentParent;
  HangData mHangData;
  nsAutoString mBrowserDumpId;
};

class HangMonitorParent
  : public PProcessHangMonitorParent
{
public:
  explicit HangMonitorParent(ProcessHangMonitor* aMonitor);
  virtual ~HangMonitorParent();

  void Open(Transport* aTransport, ProcessId aPid, MessageLoop* aIOLoop);

  virtual bool RecvHangEvidence(const HangData& aHangData) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void SetProcess(HangMonitoredProcess* aProcess) { mProcess = aProcess; }

  void Shutdown();

  void TerminateScript();
  void BeginStartingDebugger();
  void EndStartingDebugger();
  void CleanupPluginHang(uint32_t aPluginId, bool aRemoveFiles);

  MessageLoop* MonitorLoop() { return mHangMonitor->MonitorLoop(); }

 private:
  void ShutdownOnThread();

  const nsRefPtr<ProcessHangMonitor> mHangMonitor;

  // This field is read-only after construction.
  bool mReportHangs;

  // This field is only accessed on the hang thread.
  bool mIPCOpen;

  Monitor mMonitor;

  // Must be accessed with mMonitor held.
  nsRefPtr<HangMonitoredProcess> mProcess;
  bool mShutdownDone;
  // Map from plugin ID to crash dump ID. Protected by mBrowserCrashDumpHashLock.
  nsDataHashtable<nsUint32HashKey, nsString> mBrowserCrashDumpIds;
  Mutex mBrowserCrashDumpHashLock;
};

} // namespace

template<>
struct RunnableMethodTraits<HangMonitorChild>
{
  typedef HangMonitorChild Class;
  static void RetainCallee(Class* obj) { }
  static void ReleaseCallee(Class* obj) { }
};

template<>
struct RunnableMethodTraits<HangMonitorParent>
{
  typedef HangMonitorParent Class;
  static void RetainCallee(Class* obj) { }
  static void ReleaseCallee(Class* obj) { }
};

/* HangMonitorChild implementation */

HangMonitorChild::HangMonitorChild(ProcessHangMonitor* aMonitor)
 : mHangMonitor(aMonitor),
   mMonitor("HangMonitorChild lock"),
   mSentReport(false),
   mTerminateScript(false),
   mStartDebugger(false),
   mFinishedStartingDebugger(false),
   mShutdownDone(false),
   mIPCOpen(true)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

HangMonitorChild::~HangMonitorChild()
{
  // For some reason IPDL doesn't automatically delete the channel for a
  // bridged protocol (bug 1090570). So we have to do it ourselves.
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(GetTransport()));

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;
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
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void
HangMonitorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  mIPCOpen = false;

  // We use a task here to ensure that IPDL is finished with this
  // HangMonitorChild before it gets deleted on the main thread.
  MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &HangMonitorChild::ShutdownOnThread));
}

bool
HangMonitorChild::RecvTerminateScript()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MonitorAutoLock lock(mMonitor);
  mTerminateScript = true;
  return true;
}

bool
HangMonitorChild::RecvBeginStartingDebugger()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MonitorAutoLock lock(mMonitor);
  mStartDebugger = true;
  return true;
}

bool
HangMonitorChild::RecvEndStartingDebugger()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MonitorAutoLock lock(mMonitor);
  mFinishedStartingDebugger = true;
  return true;
}

void
HangMonitorChild::Open(Transport* aTransport, ProcessId aPid,
                       MessageLoop* aIOLoop)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MOZ_ASSERT(!sInstance);
  sInstance = this;

  DebugOnly<bool> ok = PProcessHangMonitorChild::Open(aTransport, aPid, aIOLoop);
  MOZ_ASSERT(ok);
}

void
HangMonitorChild::NotifySlowScriptAsync(TabId aTabId,
                                        const nsCString& aFileName,
                                        unsigned aLineNo)
{
  if (mIPCOpen) {
    unused << SendHangEvidence(SlowScriptData(aTabId, aFileName, aLineNo));
  }
}

HangMonitorChild::SlowScriptAction
HangMonitorChild::NotifySlowScript(nsITabChild* aTabChild,
                                   const char* aFileName,
                                   unsigned aLineNo)
{
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
  if (aTabChild) {
    nsRefPtr<TabChild> tabChild = static_cast<TabChild*>(aTabChild);
    id = tabChild->GetTabId();
  }
  nsAutoCString filename(aFileName);

  MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &HangMonitorChild::NotifySlowScriptAsync,
                      id, filename, aLineNo));
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
  MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this,
                      &HangMonitorChild::NotifyPluginHangAsync,
                      aPluginId));
}

void
HangMonitorChild::NotifyPluginHangAsync(uint32_t aPluginId)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  // bounce back to parent on background thread
  if (mIPCOpen) {
    unused << SendHangEvidence(PluginHangData(aPluginId));
  }
}

void
HangMonitorChild::ClearHang()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mSentReport) {
    MonitorAutoLock lock(mMonitor);
    mSentReport = false;
    mTerminateScript = false;
    mStartDebugger = false;
    mFinishedStartingDebugger = false;
  }
}

/* HangMonitorParent implementation */

HangMonitorParent::HangMonitorParent(ProcessHangMonitor* aMonitor)
 : mHangMonitor(aMonitor),
   mIPCOpen(true),
   mMonitor("HangMonitorParent lock"),
   mShutdownDone(false),
   mBrowserCrashDumpHashLock("mBrowserCrashDumpIds lock")
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mReportHangs = mozilla::Preferences::GetBool("dom.ipc.reportProcessHangs", false);
}

static PLDHashOperator
DeleteMinidump(const uint32_t& aPluginId, nsString aCrashId, void* aUserData)
{
#ifdef MOZ_CRASHREPORTER
  if (!aCrashId.IsEmpty()) {
    CrashReporter::DeleteMinidumpFilesForID(aCrashId);
  }
#endif
  return PL_DHASH_NEXT;
}

HangMonitorParent::~HangMonitorParent()
{
  // For some reason IPDL doesn't automatically delete the channel for a
  // bridged protocol (bug 1090570). So we have to do it ourselves.
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(GetTransport()));

  MutexAutoLock lock(mBrowserCrashDumpHashLock);
  mBrowserCrashDumpIds.EnumerateRead(DeleteMinidump, nullptr);
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

  MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &HangMonitorParent::ShutdownOnThread));

  while (!mShutdownDone) {
    mMonitor.Wait();
  }
}

void
HangMonitorParent::ShutdownOnThread()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

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
HangMonitorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());
  mIPCOpen = false;
}

void
HangMonitorParent::Open(Transport* aTransport, ProcessId aPid,
                        MessageLoop* aIOLoop)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  DebugOnly<bool> ok = PProcessHangMonitorParent::Open(aTransport, aPid, aIOLoop);
  MOZ_ASSERT(ok);
}

class HangObserverNotifier final : public nsRunnable
{
public:
  HangObserverNotifier(HangMonitoredProcess* aProcess,
                       const HangData& aHangData,
                       const nsString& aBrowserDumpId)
    : mProcess(aProcess),
      mHangData(aHangData),
      mBrowserDumpId(aBrowserDumpId)
  {}

  NS_IMETHOD
  Run()
  {
    // chrome process, main thread
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mProcess->SetHangData(mHangData);
    mProcess->SetBrowserDumpId(mBrowserDumpId);

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->NotifyObservers(mProcess, "process-hang-report", nullptr);
    return NS_OK;
  }

private:
  nsRefPtr<HangMonitoredProcess> mProcess;
  HangData mHangData;
  nsAutoString mBrowserDumpId;
};

bool
HangMonitorParent::RecvHangEvidence(const HangData& aHangData)
{
  // chrome process, background thread
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  if (!mReportHangs) {
    return true;
  }

#ifdef XP_WIN
  // Don't report hangs if we're debugging the process. You can comment this
  // line out for testing purposes.
  if (IsDebuggerPresent()) {
    return true;
  }
#endif

  // Before we wake up the browser main thread we want to take a
  // browser minidump.
  nsAutoString crashId;
#ifdef MOZ_CRASHREPORTER
  if (aHangData.type() == HangData::TPluginHangData) {
    MutexAutoLock lock(mBrowserCrashDumpHashLock);
    const PluginHangData& phd = aHangData.get_PluginHangData();
    if (!mBrowserCrashDumpIds.Get(phd.pluginId(), &crashId)) {
      nsCOMPtr<nsIFile> browserDump;
      if (CrashReporter::TakeMinidump(getter_AddRefs(browserDump), true)) {
        if (!CrashReporter::GetIDFromMinidump(browserDump, crashId) || crashId.IsEmpty()) {
          browserDump->Remove(false);
          NS_WARNING("Failed to generate timely browser stack, this is bad for plugin hang analysis!");
        } else {
          mBrowserCrashDumpIds.Put(phd.pluginId(), crashId);
        }
      }
    }
  }
#endif

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  nsCOMPtr<nsIRunnable> notifier =
    new HangObserverNotifier(mProcess, aHangData, crashId);
  NS_DispatchToMainThread(notifier);

  return true;
}

void
HangMonitorParent::TerminateScript()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  if (mIPCOpen) {
    unused << SendTerminateScript();
  }
}

void
HangMonitorParent::BeginStartingDebugger()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  if (mIPCOpen) {
    unused << SendBeginStartingDebugger();
  }
}

void
HangMonitorParent::EndStartingDebugger()
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  if (mIPCOpen) {
    unused << SendEndStartingDebugger();
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
#ifdef MOZ_CRASHREPORTER
  if (aRemoveFiles && !crashId.IsEmpty()) {
    CrashReporter::DeleteMinidumpFilesForID(crashId);
  }
#endif
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
HangMonitoredProcess::GetScriptBrowser(nsIDOMElement** aBrowser)
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
      nsCOMPtr<nsIDOMElement> node = do_QueryInterface(tp->GetOwnerElement());
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
HangMonitoredProcess::GetScriptLineNo(uint32_t* aLineNo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TSlowScriptData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aLineNo = mHangData.get_SlowScriptData().lineno();
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

  nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst();
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

  ProcessHangMonitor::Get()->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(mActor, &HangMonitorParent::TerminateScript));
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

  ProcessHangMonitor::Get()->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(mActor, &HangMonitorParent::BeginStartingDebugger));
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

  ProcessHangMonitor::Get()->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(mActor, &HangMonitorParent::EndStartingDebugger));
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminatePlugin()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mHangData.type() != HangData::TPluginHangData) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t id = mHangData.get_PluginHangData().pluginId();
  plugins::TerminatePlugin(id, NS_LITERAL_CSTRING("HangMonitor"),
                           mBrowserDumpId);

  if (mActor) {
    mActor->CleanupPluginHang(id, false);
  }
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminateProcess()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mContentParent) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mActor && mHangData.type() == HangData::TPluginHangData) {
    uint32_t id = mHangData.get_PluginHangData().pluginId();
    mActor->CleanupPluginHang(id, true);
  }

  mContentParent->KillHard("HangMonitor");
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::IsReportForBrowser(nsIFrameLoader* aFrameLoader, bool* aResult)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mActor) {
    *aResult = false;
    return NS_OK;
  }

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

ProcessHangMonitor* ProcessHangMonitor::sInstance;

ProcessHangMonitor::ProcessHangMonitor()
 : mCPOWTimeout(false)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_COUNT_CTOR(ProcessHangMonitor);

  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, "xpcom-shutdown", false);
  }

  mThread = new base::Thread("ProcessHangMonitor");
  if (!mThread->Start()) {
    delete mThread;
    mThread = nullptr;
  }
}

ProcessHangMonitor::~ProcessHangMonitor()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_COUNT_DTOR(ProcessHangMonitor);

  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;

  delete mThread;
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
                                     unsigned aLineNo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->NotifySlowScript(aTabChild, aFileName, aLineNo);
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
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());
  mCPOWTimeout = true;
}

void
ProcessHangMonitor::NotifyPluginHang(uint32_t aPluginId)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return HangMonitorChild::Get()->NotifyPluginHang(aPluginId);
}

PProcessHangMonitorParent*
mozilla::CreateHangMonitorParent(ContentParent* aContentParent,
                                 mozilla::ipc::Transport* aTransport,
                                 base::ProcessId aOtherPid)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  HangMonitorParent* parent = new HangMonitorParent(monitor);

  HangMonitoredProcess* process = new HangMonitoredProcess(parent, aContentParent);
  parent->SetProcess(process);

  monitor->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(parent, &HangMonitorParent::Open,
                      aTransport, aOtherPid, XRE_GetIOMessageLoop()));

  return parent;
}

PProcessHangMonitorChild*
mozilla::CreateHangMonitorChild(mozilla::ipc::Transport* aTransport,
                                base::ProcessId aOtherPid)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  HangMonitorChild* child = new HangMonitorChild(monitor);

  monitor->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(child, &HangMonitorChild::Open,
                      aTransport, aOtherPid, XRE_GetIOMessageLoop()));

  return child;
}

MessageLoop*
ProcessHangMonitor::MonitorLoop()
{
  return mThread->message_loop();
}

/* static */ void
ProcessHangMonitor::AddProcess(ContentParent* aContentParent)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mozilla::Preferences::GetBool("dom.ipc.processHangMonitor", false)) {
    DebugOnly<bool> opened = PProcessHangMonitor::Open(aContentParent);
    MOZ_ASSERT(opened);
  }
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
