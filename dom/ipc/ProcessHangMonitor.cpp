/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

  void Open(Transport* aTransport, ProcessHandle aHandle,
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

  virtual bool RecvTerminateScript() MOZ_OVERRIDE;
  virtual bool RecvBeginStartingDebugger() MOZ_OVERRIDE;
  virtual bool RecvEndStartingDebugger() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  void Shutdown();

  static HangMonitorChild* Get() { return sInstance; }

  MessageLoop* MonitorLoop() { return mHangMonitor->MonitorLoop(); }

 private:
  void ShutdownOnThread();

  static HangMonitorChild* sInstance;

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

HangMonitorChild* HangMonitorChild::sInstance;

/* Parent process objects */

class HangMonitorParent;

class HangMonitoredProcess MOZ_FINAL
  : public nsIHangReport
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  HangMonitoredProcess(HangMonitorParent* aActor,
                       ContentParent* aContentParent)
    : mActor(aActor), mContentParent(aContentParent) {}

  NS_IMETHOD GetHangType(uint32_t* aHangType) MOZ_OVERRIDE;
  NS_IMETHOD GetScriptBrowser(nsIDOMElement** aBrowser) MOZ_OVERRIDE;
  NS_IMETHOD GetScriptFileName(nsACString& aFileName) MOZ_OVERRIDE;
  NS_IMETHOD GetScriptLineNo(uint32_t* aLineNo) MOZ_OVERRIDE;

  NS_IMETHOD GetPluginName(nsACString& aPluginName) MOZ_OVERRIDE;

  NS_IMETHOD TerminateScript() MOZ_OVERRIDE;
  NS_IMETHOD BeginStartingDebugger() MOZ_OVERRIDE;
  NS_IMETHOD EndStartingDebugger() MOZ_OVERRIDE;
  NS_IMETHOD TerminatePlugin() MOZ_OVERRIDE;
  NS_IMETHOD TerminateProcess() MOZ_OVERRIDE;

  NS_IMETHOD IsReportForBrowser(nsIFrameLoader* aFrameLoader, bool* aResult) MOZ_OVERRIDE;

  void Clear() { mContentParent = nullptr; mActor = nullptr; }

  void SetHangData(const HangData& aHangData) { mHangData = aHangData; }

private:
  ~HangMonitoredProcess() {}

  // Everything here is main thread-only.
  HangMonitorParent* mActor;
  ContentParent* mContentParent;
  HangData mHangData;
};

class HangMonitorParent
  : public PProcessHangMonitorParent
{
public:
  explicit HangMonitorParent(ProcessHangMonitor* aMonitor);
  virtual ~HangMonitorParent();

  void Open(Transport* aTransport, ProcessHandle aHandle,
            MessageLoop* aIOLoop);

  virtual bool RecvHangEvidence(const HangData& aHangData) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  void SetProcess(HangMonitoredProcess* aProcess) { mProcess = aProcess; }

  void Shutdown();

  void TerminateScript();
  void BeginStartingDebugger();
  void EndStartingDebugger();

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
HangMonitorChild::Open(Transport* aTransport, ProcessHandle aHandle,
                       MessageLoop* aIOLoop)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  MOZ_ASSERT(!sInstance);
  sInstance = this;

  DebugOnly<bool> ok = PProcessHangMonitorChild::Open(aTransport, aHandle, aIOLoop);
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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mSentReport = true;

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
   mShutdownDone(false)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mReportHangs = mozilla::Preferences::GetBool("dom.ipc.reportProcessHangs", false);
}

HangMonitorParent::~HangMonitorParent()
{
  // For some reason IPDL doesn't autmatically delete the channel for a
  // bridged protocol (bug 1090570). So we have to do it ourselves.
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(GetTransport()));
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
HangMonitorParent::Open(Transport* aTransport, ProcessHandle aHandle,
                        MessageLoop* aIOLoop)
{
  MOZ_RELEASE_ASSERT(MessageLoop::current() == MonitorLoop());

  DebugOnly<bool> ok = PProcessHangMonitorParent::Open(aTransport, aHandle, aIOLoop);
  MOZ_ASSERT(ok);
}

class HangObserverNotifier MOZ_FINAL : public nsRunnable
{
public:
  HangObserverNotifier(HangMonitoredProcess* aProcess, const HangData& aHangData)
    : mProcess(aProcess),
      mHangData(aHangData)
  {}

  NS_IMETHOD
  Run()
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mProcess->SetHangData(mHangData);

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->NotifyObservers(mProcess, "process-hang-report", nullptr);
    return NS_OK;
  }

private:
  nsRefPtr<HangMonitoredProcess> mProcess;
  HangData mHangData;
};

bool
HangMonitorParent::RecvHangEvidence(const HangData& aHangData)
{
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

  mHangMonitor->InitiateCPOWTimeout();

  MonitorAutoLock lock(mMonitor);

  nsCOMPtr<nsIRunnable> notifier = new HangObserverNotifier(mProcess, aHangData);
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
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
    break;
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

  aPluginName = tag->mName;
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
  plugins::TerminatePlugin(id);
  return NS_OK;
}

NS_IMETHODIMP
HangMonitoredProcess::TerminateProcess()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mContentParent) {
    return NS_ERROR_UNEXPECTED;
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

ProcessHangMonitor* ProcessHangMonitor::sInstance;

ProcessHangMonitor::ProcessHangMonitor()
 : mCPOWTimeout(false)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_COUNT_CTOR(ProcessHangMonitor);

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
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
                                 base::ProcessId aOtherProcess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  HangMonitorParent* parent = new HangMonitorParent(monitor);

  HangMonitoredProcess* process = new HangMonitoredProcess(parent, aContentParent);
  parent->SetProcess(process);

  base::ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  monitor->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(parent, &HangMonitorParent::Open,
                      aTransport, handle, XRE_GetIOMessageLoop()));

  return parent;
}

PProcessHangMonitorChild*
mozilla::CreateHangMonitorChild(mozilla::ipc::Transport* aTransport,
                                base::ProcessId aOtherProcess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProcessHangMonitor* monitor = ProcessHangMonitor::GetOrCreate();
  HangMonitorChild* child = new HangMonitorChild(monitor);

  base::ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  monitor->MonitorLoop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(child, &HangMonitorChild::Open,
                      aTransport, handle, XRE_GetIOMessageLoop()));

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
