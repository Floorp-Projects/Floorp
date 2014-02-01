/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIPropertyBag2.h"
#include "ProcessPriorityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsCxPusher.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
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
class PreallocatedProcessManagerImpl MOZ_FINAL
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
  void OnNuwaReady();
  bool PreallocatedProcessReady();
  already_AddRefed<ContentParent> GetSpareProcess();
  void RunAfterPreallocatedProcessReady(nsIRunnable* aRunnable);

private:
  void OnNuwaForkTimeout();
  void NuwaFork();

  // initialization off the critical path of app startup.
  CancelableTask* mPreallocateAppProcessTask;

  // The array containing the preallocated processes. 4 as the inline storage size
  // should be enough so we don't need to grow the nsAutoTArray.
  nsAutoTArray<nsRefPtr<ContentParent>, 4> mSpareProcesses;
  nsTArray<CancelableTask*> mNuwaForkWaitTasks;

  nsTArray<nsCOMPtr<nsIRunnable> > mDelayedContentParentRequests;

  // Nuwa process is ready for creating new process.
  bool mIsNuwaReady;
#endif

private:
  static mozilla::StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  DISALLOW_EVIL_CONSTRUCTORS(PreallocatedProcessManagerImpl);

  void Init();

  void RereadPrefs();
  void Enable();
  void Disable();

  void ObserveProcessShutdown(nsISupports* aSubject);

  bool mEnabled;
  bool mShutdown;
  nsRefPtr<ContentParent> mPreallocatedAppProcess;
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

NS_IMPL_ISUPPORTS1(PreallocatedProcessManagerImpl, nsIObserver)

PreallocatedProcessManagerImpl::PreallocatedProcessManagerImpl()
  : mEnabled(false)
#ifdef MOZ_NUWA_PROCESS
  , mPreallocateAppProcessTask(nullptr)
  , mIsNuwaReady(false)
#endif
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
  RereadPrefs();
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
    FROM_HERE,
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

  MessageLoop::current()->PostIdleTask(
    FROM_HERE,
    NewRunnableMethod(this, &PreallocatedProcessManagerImpl::AllocateNow));
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
PreallocatedProcessManagerImpl::RunAfterPreallocatedProcessReady(nsIRunnable* aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDelayedContentParentRequests.AppendElement(aRequest);

  if (!mPreallocateAppProcessTask) {
    // This is an urgent NuwaFork() request.
    DelayedNuwaFork();
  }
}

void
PreallocatedProcessManagerImpl::ScheduleDelayedNuwaFork()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mPreallocateAppProcessTask) {
    // Make sure there is only one request running.
    return;
  }

  mPreallocateAppProcessTask = NewRunnableMethod(
    this, &PreallocatedProcessManagerImpl::DelayedNuwaFork);
  MessageLoop::current()->PostDelayedTask(
    FROM_HERE, mPreallocateAppProcessTask,
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

  if (mSpareProcesses.IsEmpty()) {
    return nullptr;
  }

  nsRefPtr<ContentParent> process = mSpareProcesses.LastElement();
  mSpareProcesses.RemoveElementAt(mSpareProcesses.Length() - 1);

  if (mSpareProcesses.IsEmpty() && mIsNuwaReady) {
    NS_ASSERTION(mPreallocatedAppProcess != nullptr,
                 "Nuwa process is not present!");
    ScheduleDelayedNuwaFork();
  }

  return process.forget();
}

/**
 * Publish a ContentParent to spare process list.
 */
void
PreallocatedProcessManagerImpl::PublishSpareProcess(ContentParent* aContent)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (Preferences::GetBool("dom.ipc.processPriorityManager.testMode")) {
    AutoJSContext cx;
    nsCOMPtr<nsIMessageBroadcaster> ppmm =
      do_GetService("@mozilla.org/parentprocessmessagemanager;1");
    nsresult rv = ppmm->BroadcastAsyncMessage(
      NS_LITERAL_STRING("TEST-ONLY:nuwa-add-new-process"),
      JSVAL_NULL, JSVAL_NULL, cx, 1);
  }

  if (!mNuwaForkWaitTasks.IsEmpty()) {
    mNuwaForkWaitTasks.ElementAt(0)->Cancel();
    mNuwaForkWaitTasks.RemoveElementAt(0);
  }

  mSpareProcesses.AppendElement(aContent);

  if (!mDelayedContentParentRequests.IsEmpty()) {
    nsCOMPtr<nsIRunnable> runnable = mDelayedContentParentRequests[0];
    mDelayedContentParentRequests.RemoveElementAt(0);
    NS_DispatchToMainThread(runnable);
  }
}

void
PreallocatedProcessManagerImpl::MaybeForgetSpare(ContentParent* aContent)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDelayedContentParentRequests.IsEmpty()) {
    if (!mPreallocateAppProcessTask) {
      // This NuwaFork request is urgent. Don't delay it.
      DelayedNuwaFork();
    }
  }

  if (mSpareProcesses.RemoveElement(aContent)) {
    return;
  }

  if (aContent == mPreallocatedAppProcess) {
    mPreallocatedAppProcess = nullptr;
    mIsNuwaReady = false;
    ScheduleDelayedNuwaFork();
  }
}

void
PreallocatedProcessManagerImpl::OnNuwaReady()
{
  NS_ASSERTION(!mIsNuwaReady, "Multiple Nuwa processes created!");
  ProcessPriorityManager::SetProcessPriority(mPreallocatedAppProcess,
                                             hal::PROCESS_PRIORITY_MASTER);
  mIsNuwaReady = true;
  if (Preferences::GetBool("dom.ipc.processPriorityManager.testMode")) {
    AutoJSContext cx;
    nsCOMPtr<nsIMessageBroadcaster> ppmm =
      do_GetService("@mozilla.org/parentprocessmessagemanager;1");
    nsresult rv = ppmm->BroadcastAsyncMessage(
      NS_LITERAL_STRING("TEST-ONLY:nuwa-ready"),
      JSVAL_NULL, JSVAL_NULL, cx, 1);
  }
  NuwaFork();
}

bool
PreallocatedProcessManagerImpl::PreallocatedProcessReady()
{
  return !mSpareProcesses.IsEmpty();
}


void
PreallocatedProcessManagerImpl::OnNuwaForkTimeout()
{
  if (!mNuwaForkWaitTasks.IsEmpty()) {
    mNuwaForkWaitTasks.RemoveElementAt(0);
  }

  // We haven't RecvAddNewProcess() after NuwaFork(). Maybe the main
  // thread of the Nuwa process is in deadlock.
  MOZ_ASSERT(false, "Can't fork from the nuwa process.");
}

void
PreallocatedProcessManagerImpl::NuwaFork()
{
  CancelableTask* nuwaForkTimeoutTask = NewRunnableMethod(
    this, &PreallocatedProcessManagerImpl::OnNuwaForkTimeout);
  mNuwaForkWaitTasks.AppendElement(nuwaForkTimeoutTask);

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          nuwaForkTimeoutTask,
                                          NUWA_FORK_WAIT_DURATION_MS);
  mPreallocatedAppProcess->SendNuwaFork();
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
      nsRefPtr<ContentParent> process = mSpareProcesses[0];
      process->Close();
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

} // anonymous namespace

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

/*static */ bool
PreallocatedProcessManager::PreallocatedProcessReady()
{
  return GetPPMImpl()->PreallocatedProcessReady();
}

/* static */ void
PreallocatedProcessManager::RunAfterPreallocatedProcessReady(nsIRunnable* aRequest)
{
  GetPPMImpl()->RunAfterPreallocatedProcessReady(aRequest);
}

#endif

} // namespace mozilla
