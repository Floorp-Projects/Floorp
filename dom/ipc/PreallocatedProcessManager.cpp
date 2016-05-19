/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIPropertyBag2.h"
#include "ProcessPriorityManager.h"
#include "nsServiceManagerUtils.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

#ifdef MOZ_B2G_LOADER
#include "ProcessUtils.h"
#endif

// This number is fairly arbitrary ... the intention is to put off
// launching another app process until the last one has finished
// loading its content, to reduce CPU/memory/IO contention.
#define DEFAULT_ALLOCATE_DELAY 1000
#define NUWA_FORK_WAIT_DURATION_MS 2000 // 2 seconds.

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;

namespace {

/**
 * This singleton class implements the static methods on
 * PreallocatedProcessManager.
 */
class PreallocatedProcessManagerImpl final
  : public nsIObserver
{
public:
  static PreallocatedProcessManagerImpl* Singleton();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // See comments on PreallocatedProcessManager for these methods.
  void AllocateAfterDelay();
  void AllocateOnIdle();
  void AllocateNow();
  already_AddRefed<ContentParent> Take();

#ifdef MOZ_NUWA_PROCESS
public:
  void ScheduleDelayedNuwaFork();
  void DelayedNuwaFork();
  void PublishSpareProcess(ContentParent* aContent);
  void MaybeForgetSpare(ContentParent* aContent);
  bool IsNuwaReady();
  void OnNuwaReady();
  bool PreallocatedProcessReady();
  already_AddRefed<ContentParent> GetSpareProcess();

private:
  void NuwaFork();

  // initialization off the critical path of app startup.
  CancelableRunnable* mPreallocateAppProcessTask;

  // The array containing the preallocated processes. 4 as the inline storage size
  // should be enough so we don't need to grow the AutoTArray.
  AutoTArray<RefPtr<ContentParent>, 4> mSpareProcesses;

  // Nuwa process is ready for creating new process.
  bool mIsNuwaReady;
#endif

private:
  static mozilla::StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  ~PreallocatedProcessManagerImpl() {}
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManagerImpl);

  void Init();

  void RereadPrefs();
  void Enable();
  void Disable();

  void ObserveProcessShutdown(nsISupports* aSubject);

  bool mEnabled;
  bool mShutdown;
  RefPtr<ContentParent> mPreallocatedAppProcess;
};

/* static */ StaticRefPtr<PreallocatedProcessManagerImpl>
PreallocatedProcessManagerImpl::sSingleton;

/* static */ PreallocatedProcessManagerImpl*
PreallocatedProcessManagerImpl::Singleton()
{
  if (!sSingleton) {
    sSingleton = new PreallocatedProcessManagerImpl();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

NS_IMPL_ISUPPORTS(PreallocatedProcessManagerImpl, nsIObserver)

PreallocatedProcessManagerImpl::PreallocatedProcessManagerImpl()
  :
#ifdef MOZ_NUWA_PROCESS
    mPreallocateAppProcessTask(nullptr)
  , mIsNuwaReady(false)
  ,
#endif
    mEnabled(false)
  , mShutdown(false)
{}

void
PreallocatedProcessManagerImpl::Init()
{
  Preferences::AddStrongObserver(this, "dom.ipc.processPrelaunch.enabled");
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "ipc:content-shutdown",
                    /* weakRef = */ false);
    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                    /* weakRef = */ false);
  }
#ifdef MOZ_B2G_LOADER
  if (!mozilla::ipc::ProcLoaderIsInitialized()) {
    Disable();
  } else
#endif
  {
    RereadPrefs();
  }
}

NS_IMETHODIMP
PreallocatedProcessManagerImpl::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const char16_t* aData)
{
  if (!strcmp("ipc:content-shutdown", aTopic)) {
    ObserveProcessShutdown(aSubject);
  } else if (!strcmp("nsPref:changed", aTopic)) {
    // The only other observer we registered was for our prefs.
    RereadPrefs();
  } else if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    mShutdown = true;
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

void
PreallocatedProcessManagerImpl::RereadPrefs()
{
  if (Preferences::GetBool("dom.ipc.processPrelaunch.enabled")) {
    Enable();
  } else {
    Disable();
  }
}

already_AddRefed<ContentParent>
PreallocatedProcessManagerImpl::Take()
{
  return mPreallocatedAppProcess.forget();
}

void
PreallocatedProcessManagerImpl::Enable()
{
  if (mEnabled) {
    return;
  }

  mEnabled = true;
#ifdef MOZ_NUWA_PROCESS
  ScheduleDelayedNuwaFork();
#else
  AllocateAfterDelay();
#endif
}

void
PreallocatedProcessManagerImpl::AllocateAfterDelay()
{
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  MessageLoop::current()->PostDelayedTask(
    NewRunnableMethod(this, &PreallocatedProcessManagerImpl::AllocateOnIdle),
    Preferences::GetUint("dom.ipc.processPrelaunch.delayMs",
                         DEFAULT_ALLOCATE_DELAY));
}

void
PreallocatedProcessManagerImpl::AllocateOnIdle()
{
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  MessageLoop::current()->PostIdleTask(NewRunnableMethod(this, &PreallocatedProcessManagerImpl::AllocateNow));
}

void
PreallocatedProcessManagerImpl::AllocateNow()
{
  if (!mEnabled || mPreallocatedAppProcess) {
    return;
  }

  mPreallocatedAppProcess = ContentParent::PreallocateAppProcess();
}

#ifdef MOZ_NUWA_PROCESS

void
PreallocatedProcessManagerImpl::ScheduleDelayedNuwaFork()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mPreallocateAppProcessTask) {
    // Make sure there is only one request running.
    return;
  }

  RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod(
    this, &PreallocatedProcessManagerImpl::DelayedNuwaFork);
  mPreallocateAppProcessTask = task;
  MessageLoop::current()->PostDelayedTask(task.forget(),
    Preferences::GetUint("dom.ipc.processPrelaunch.delayMs",
                         DEFAULT_ALLOCATE_DELAY));
}

void
PreallocatedProcessManagerImpl::DelayedNuwaFork()
{
  MOZ_ASSERT(NS_IsMainThread());

  mPreallocateAppProcessTask = nullptr;

  if (!mIsNuwaReady) {
    if (!mPreallocatedAppProcess && !mShutdown && mEnabled) {
      mPreallocatedAppProcess = ContentParent::RunNuwaProcess();
    }
    // else mPreallocatedAppProcess is starting. It will NuwaFork() when ready.
  } else if (mSpareProcesses.IsEmpty()) {
    NuwaFork();
  }
}

/**
 * Get a spare ContentParent from mSpareProcesses list.
 */
already_AddRefed<ContentParent>
PreallocatedProcessManagerImpl::GetSpareProcess()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIsNuwaReady) {
    return nullptr;
  }

  if (mSpareProcesses.IsEmpty()) {
    // After this call, there should be a spare process.
    mPreallocatedAppProcess->ForkNewProcess(true);
  }

  RefPtr<ContentParent> process = mSpareProcesses.LastElement();
  mSpareProcesses.RemoveElementAt(mSpareProcesses.Length() - 1);

  if (mSpareProcesses.IsEmpty() && mIsNuwaReady) {
    NS_ASSERTION(mPreallocatedAppProcess != nullptr,
                 "Nuwa process is not present!");
    ScheduleDelayedNuwaFork();
  }

  return process.forget();
}

static bool
TestCaseEnabled()
{
  return Preferences::GetBool("dom.ipc.preallocatedProcessManager.testMode");
}

static void
SendTestOnlyNotification(const char* aMessage)
{
  if (!TestCaseEnabled()) {
    return;
  }

  AutoSafeJSContext cx;
  nsString message;
  message.AppendPrintf("%s", aMessage);

  nsCOMPtr<nsIMessageBroadcaster> ppmm =
    do_GetService("@mozilla.org/parentprocessmessagemanager;1");

  mozilla::Unused << ppmm->BroadcastAsyncMessage(
      message, JS::NullHandleValue, JS::NullHandleValue, cx, 1);
}

static void
KillOrCloseProcess(ContentParent* aProcess)
{
  if (TestCaseEnabled()) {
    // KillHard() the process because we don't want the process to abort when we
    // close the IPC channel while it's still running and creating actors.
    aProcess->KillHard("Killed by test case.");
  }
  else {
    aProcess->Close();
  }
}

/**
 * Publish a ContentParent to spare process list.
 */
void
PreallocatedProcessManagerImpl::PublishSpareProcess(ContentParent* aContent)
{
  MOZ_ASSERT(NS_IsMainThread());

  SendTestOnlyNotification("TEST-ONLY:nuwa-add-new-process");

  mSpareProcesses.AppendElement(aContent);
}

void
PreallocatedProcessManagerImpl::MaybeForgetSpare(ContentParent* aContent)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mSpareProcesses.RemoveElement(aContent)) {
    return;
  }

  if (aContent == mPreallocatedAppProcess) {
    mPreallocatedAppProcess = nullptr;
    mIsNuwaReady = false;
    while (mSpareProcesses.Length() > 0) {
      RefPtr<ContentParent> process = mSpareProcesses[mSpareProcesses.Length() - 1];
      KillOrCloseProcess(aContent);
      mSpareProcesses.RemoveElementAt(mSpareProcesses.Length() - 1);
    }
    ScheduleDelayedNuwaFork();
  }
}

bool
PreallocatedProcessManagerImpl::IsNuwaReady()
{
  return mIsNuwaReady;
}

void
PreallocatedProcessManagerImpl::OnNuwaReady()
{
  NS_ASSERTION(!mIsNuwaReady, "Multiple Nuwa processes created!");
  ProcessPriorityManager::SetProcessPriority(mPreallocatedAppProcess,
                                             hal::PROCESS_PRIORITY_MASTER);
  mIsNuwaReady = true;
  SendTestOnlyNotification("TEST-ONLY:nuwa-ready");

  NuwaFork();
}

bool
PreallocatedProcessManagerImpl::PreallocatedProcessReady()
{
  return !mSpareProcesses.IsEmpty();
}

void
PreallocatedProcessManagerImpl::NuwaFork()
{
  mPreallocatedAppProcess->ForkNewProcess(false);
}
#endif

void
PreallocatedProcessManagerImpl::Disable()
{
  if (!mEnabled) {
    return;
  }

  mEnabled = false;

#ifdef MOZ_NUWA_PROCESS
  // Cancel pending fork.
  if (mPreallocateAppProcessTask) {
    mPreallocateAppProcessTask->Cancel();
    mPreallocateAppProcessTask = nullptr;
  }
#endif

  if (mPreallocatedAppProcess) {
#ifdef MOZ_NUWA_PROCESS
    while (mSpareProcesses.Length() > 0){
      RefPtr<ContentParent> process = mSpareProcesses[0];
      KillOrCloseProcess(process);
      mSpareProcesses.RemoveElementAt(0);
    }
    mIsNuwaReady = false;
#endif
    mPreallocatedAppProcess->Close();
    mPreallocatedAppProcess = nullptr;
  }
}

void
PreallocatedProcessManagerImpl::ObserveProcessShutdown(nsISupports* aSubject)
{
  if (!mPreallocatedAppProcess) {
    return;
  }

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  NS_ENSURE_TRUE_VOID(childID != CONTENT_PROCESS_ID_UNKNOWN);

  if (childID == mPreallocatedAppProcess->ChildID()) {
    mPreallocatedAppProcess = nullptr;
  }
}

inline PreallocatedProcessManagerImpl* GetPPMImpl()
{
  return PreallocatedProcessManagerImpl::Singleton();
}

} // namespace

namespace mozilla {

/* static */ void
PreallocatedProcessManager::AllocateAfterDelay()
{
#ifdef MOZ_NUWA_PROCESS
  GetPPMImpl()->ScheduleDelayedNuwaFork();
#else
  GetPPMImpl()->AllocateAfterDelay();
#endif
}

/* static */ void
PreallocatedProcessManager::AllocateOnIdle()
{
  GetPPMImpl()->AllocateOnIdle();
}

/* static */ void
PreallocatedProcessManager::AllocateNow()
{
  GetPPMImpl()->AllocateNow();
}

/* static */ already_AddRefed<ContentParent>
PreallocatedProcessManager::Take()
{
#ifdef MOZ_NUWA_PROCESS
  return GetPPMImpl()->GetSpareProcess();
#else
  return GetPPMImpl()->Take();
#endif
}

#ifdef MOZ_NUWA_PROCESS
/* static */ void
PreallocatedProcessManager::PublishSpareProcess(ContentParent* aContent)
{
  GetPPMImpl()->PublishSpareProcess(aContent);
}

/* static */ void
PreallocatedProcessManager::MaybeForgetSpare(ContentParent* aContent)
{
  GetPPMImpl()->MaybeForgetSpare(aContent);
}

/* static */ void
PreallocatedProcessManager::OnNuwaReady()
{
  GetPPMImpl()->OnNuwaReady();
}

/* static */ bool
PreallocatedProcessManager::IsNuwaReady()
{
  return GetPPMImpl()->IsNuwaReady();
}

/*static */ bool
PreallocatedProcessManager::PreallocatedProcessReady()
{
  return GetPPMImpl()->PreallocatedProcessReady();
}

#endif

} // namespace mozilla
