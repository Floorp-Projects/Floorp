/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessPriorityManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "AudioChannelService.h"
#include "prlog.h"
#include "nsPrintfCString.h"
#include "nsWeakPtr.h"
#include "nsXULAppAPI.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMWindow.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "StaticPtr.h"
#include "nsIMozBrowserFrame.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsPrintfCString.h"
#include "prlog.h"

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifdef LOG
#undef LOG
#endif

// Use LOGP inside a ParticularProcessPriorityManager method; use LOG
// everywhere else.  LOGP prints out information about the particular process
// priority manager.
//
// (Wow, our logging story is a huge mess.)

// #define ENABLE_LOGGING 1

#if defined(ANDROID) && defined(ENABLE_LOGGING)
#  include <android/log.h>
#  define LOG(fmt, ...) \
     __android_log_print(ANDROID_LOG_INFO, \
       "Gecko:ProcessPriorityManager", \
       fmt, ## __VA_ARGS__)
#  define LOGP(fmt, ...) \
    __android_log_print(ANDROID_LOG_INFO, \
      "Gecko:ProcessPriorityManager", \
      "[%schild-id=%llu, pid=%d] " fmt, \
      NameWithComma().get(), \
      (long long unsigned) ChildID(), Pid(), ## __VA_ARGS__)

#elif defined(ENABLE_LOGGING)
#  define LOG(fmt, ...) \
     printf("ProcessPriorityManager - " fmt "\n", ##__VA_ARGS__)
#  define LOGP(fmt, ...) \
     printf("ProcessPriorityManager[%schild-id=%llu, pid=%d] - " fmt "\n", \
       NameWithComma().get(), \
       (unsigned long long) ChildID(), Pid(), ##__VA_ARGS__)

#elif defined(PR_LOGGING)
  static PRLogModuleInfo*
  GetPPMLog()
  {
    static PRLogModuleInfo *sLog;
    if (!sLog)
      sLog = PR_NewLogModule("ProcessPriorityManager");
    return sLog;
  }
#  define LOG(fmt, ...) \
     PR_LOG(GetPPMLog(), PR_LOG_DEBUG, \
            ("ProcessPriorityManager - " fmt, ##__VA_ARGS__))
#  define LOGP(fmt, ...) \
     PR_LOG(GetPPMLog(), PR_LOG_DEBUG, \
            ("ProcessPriorityManager[%schild-id=%llu, pid=%d] - " fmt, \
            NameWithComma().get(), \
            (unsigned long long) ChildID(), Pid(), ##__VA_ARGS__))
#else
#define LOG(fmt, ...)
#define LOGP(fmt, ...)
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace {

class ParticularProcessPriorityManager;

/**
 * This singleton class does the work to implement the process priority manager
 * in the main process.  This class may not be used in child processes.  (You
 * can call StaticInit, but it won't do anything, and GetSingleton() will
 * return null.)
 *
 * ProcessPriorityManager::CurrentProcessIsForeground(), which can be called in
 * any process, is handled separately, by the ProcessPriorityManagerChild
 * class.
 */
class ProcessPriorityManagerImpl MOZ_FINAL
  : public nsIObserver
{
public:
  /**
   * If we're in the main process, get the ProcessPriorityManagerImpl
   * singleton.  If we're in a child process, return null.
   */
  static ProcessPriorityManagerImpl* GetSingleton();

  static void StaticInit();
  static bool PrefsEnabled();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * This function implements ProcessPriorityManager::SetProcessPriority.
   */
  void SetProcessPriority(ContentParent* aContentParent,
                          ProcessPriority aPriority);

  /**
   * If a magic testing-only pref is set, notify the observer service on the
   * given topic with the given data.  This is used for testing
   */
  void FireTestOnlyObserverNotification(const char* aTopic,
                                        const nsACString& aData = EmptyCString());

private:
  static bool sPrefListenersRegistered;
  static bool sInitialized;
  static StaticRefPtr<ProcessPriorityManagerImpl> sSingleton;

  static int PrefChangedCallback(const char* aPref, void* aClosure);

  ProcessPriorityManagerImpl();
  ~ProcessPriorityManagerImpl() {}
  DISALLOW_EVIL_CONSTRUCTORS(ProcessPriorityManagerImpl);

  void Init();

  already_AddRefed<ParticularProcessPriorityManager>
  GetParticularProcessPriorityManager(ContentParent* aContentParent);

  void ObserveContentParentCreated(nsISupports* aContentParent);
  void ObserveContentParentDestroyed(nsISupports* aSubject);

  nsDataHashtable<nsUint64HashKey, nsRefPtr<ParticularProcessPriorityManager> >
    mParticularManagers;
};

/**
 * This singleton class implements the parts of the process priority manager
 * that are available from all processes.
 */
class ProcessPriorityManagerChild MOZ_FINAL
  : public nsIObserver
{
public:
  static void StaticInit();
  static ProcessPriorityManagerChild* Singleton();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  bool CurrentProcessIsForeground();

private:
  static StaticRefPtr<ProcessPriorityManagerChild> sSingleton;

  ProcessPriorityManagerChild();
  ~ProcessPriorityManagerChild() {}
  DISALLOW_EVIL_CONSTRUCTORS(ProcessPriorityManagerChild);

  void Init();

  hal::ProcessPriority mCachedPriority;
};

/**
 * This class manages the priority of one particular process.  It is
 * main-process only.
 */
class ParticularProcessPriorityManager MOZ_FINAL
  : public WakeLockObserver
  , public nsIObserver
  , public nsITimerCallback
{
public:
  ParticularProcessPriorityManager(ContentParent* aContentParent);
  ~ParticularProcessPriorityManager();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  virtual void Notify(const WakeLockInformation& aInfo) MOZ_OVERRIDE;
  void Init();

  int32_t Pid() const;
  uint64_t ChildID() const;

  /**
   * Used in logging, this method returns the ContentParent's name followed by
   * ", ".  If we can't get the ContentParent's name for some reason, it
   * returns an empty string.
   *
   * The reference returned here is guaranteed to be live until the next call
   * to NameWithComma() or until the ParticularProcessPriorityManager is
   * destroyed, whichever comes first.
   */
  const nsAutoCString& NameWithComma();

  bool HasAppType(const char* aAppType);
  bool IsExpectingSystemMessage();

  void OnAudioChannelProcessChanged(nsISupports* aSubject);
  void OnRemoteBrowserFrameShown(nsISupports* aSubject);
  void OnTabParentDestroyed(nsISupports* aSubject);
  void OnFrameloaderVisibleChanged(nsISupports* aSubject);
  void OnChannelConnected(nsISupports* aSubject);

  ProcessPriority ComputePriority();
  void ScheduleResetPriority(const char* aTimeoutPref);
  void ResetPriority();
  void ResetPriorityNow();

  void SetPriorityNow(ProcessPriority aPriority);

  void ShutDown();

private:
  void FireTestOnlyObserverNotification(
    const char* aTopic,
    const nsACString& aData = EmptyCString());

  void FireTestOnlyObserverNotification(
    const char* aTopic,
    const char* aData = nullptr);

  ContentParent* mContentParent;
  uint64_t mChildID;
  ProcessPriority mPriority;
  bool mHoldsCPUWakeLock;
  bool mHoldsHighPriorityWakeLock;

  /**
   * Used to implement NameWithComma().
   */
  nsAutoCString mNameWithComma;

  nsCOMPtr<nsITimer> mResetPriorityTimer;
};

/* static */ bool ProcessPriorityManagerImpl::sInitialized = false;
/* static */ bool ProcessPriorityManagerImpl::sPrefListenersRegistered = false;
/* static */ StaticRefPtr<ProcessPriorityManagerImpl>
  ProcessPriorityManagerImpl::sSingleton;

NS_IMPL_ISUPPORTS1(ProcessPriorityManagerImpl,
                   nsIObserver);

/* static */ int
ProcessPriorityManagerImpl::PrefChangedCallback(const char* aPref,
                                                void* aClosure)
{
  StaticInit();
  return 0;
}

/* static */ bool
ProcessPriorityManagerImpl::PrefsEnabled()
{
  return Preferences::GetBool("dom.ipc.processPriorityManager.enabled") &&
         !Preferences::GetBool("dom.ipc.tabs.disabled");
}

/* static */ void
ProcessPriorityManagerImpl::StaticInit()
{
  if (sInitialized) {
    return;
  }

  // The process priority manager is main-process only.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    sInitialized = true;
    return;
  }

  // If IPC tabs aren't enabled at startup, don't bother with any of this.
  if (!PrefsEnabled()) {
    LOG("InitProcessPriorityManager bailing due to prefs.");

    // Run StaticInit() again if the prefs change.  We don't expect this to
    // happen in normal operation, but it happens during testing.
    if (!sPrefListenersRegistered) {
      sPrefListenersRegistered = true;
      Preferences::RegisterCallback(PrefChangedCallback,
                                    "dom.ipc.processPriorityManager.enabled");
      Preferences::RegisterCallback(PrefChangedCallback,
                                    "dom.ipc.tabs.disabled");
    }
    return;
  }

  sInitialized = true;

  sSingleton = new ProcessPriorityManagerImpl();
  sSingleton->Init();
  ClearOnShutdown(&sSingleton);
}

/* static */ ProcessPriorityManagerImpl*
ProcessPriorityManagerImpl::GetSingleton()
{
  if (!sSingleton) {
    StaticInit();
  }

  return sSingleton;
}

ProcessPriorityManagerImpl::ProcessPriorityManagerImpl()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  mParticularManagers.Init();
}

void
ProcessPriorityManagerImpl::Init()
{
  LOG("Starting up.  This is the master process.");

  // The master process's priority never changes; set it here and then forget
  // about it.  We'll manage only subprocesses' priorities using the process
  // priority manager.
  hal::SetProcessPriority(getpid(), PROCESS_PRIORITY_MASTER);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "ipc:content-created", /* ownsWeak */ false);
    os->AddObserver(this, "ipc:content-shutdown", /* ownsWeak */ false);
  }
}

NS_IMETHODIMP
ProcessPriorityManagerImpl::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const PRUnichar* aData)
{
  nsDependentCString topic(aTopic);
  if (topic.EqualsLiteral("ipc:content-created")) {
    ObserveContentParentCreated(aSubject);
  } else if (topic.EqualsLiteral("ipc:content-shutdown")) {
    ObserveContentParentDestroyed(aSubject);
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

already_AddRefed<ParticularProcessPriorityManager>
ProcessPriorityManagerImpl::GetParticularProcessPriorityManager(
  ContentParent* aContentParent)
{
  nsRefPtr<ParticularProcessPriorityManager> pppm;
  mParticularManagers.Get(aContentParent->ChildID(), &pppm);
  if (!pppm) {
    pppm = new ParticularProcessPriorityManager(aContentParent);
    pppm->Init();
    mParticularManagers.Put(aContentParent->ChildID(), pppm);

    FireTestOnlyObserverNotification("process-created",
      nsPrintfCString("%lld", aContentParent->ChildID()));
  }

  return pppm.forget();
}

void
ProcessPriorityManagerImpl::SetProcessPriority(ContentParent* aContentParent,
                                               ProcessPriority aPriority)
{
  MOZ_ASSERT(aContentParent);
  nsRefPtr<ParticularProcessPriorityManager> pppm =
    GetParticularProcessPriorityManager(aContentParent);
  pppm->SetPriorityNow(aPriority);
}

void
ProcessPriorityManagerImpl::ObserveContentParentCreated(
  nsISupports* aContentParent)
{
  // Do nothing; it's sufficient to get the PPPM.  But assign to nsRefPtr so we
  // don't leak the already_AddRefed object.
  nsCOMPtr<nsIObserver> cp = do_QueryInterface(aContentParent);
  nsRefPtr<ParticularProcessPriorityManager> pppm =
    GetParticularProcessPriorityManager(static_cast<ContentParent*>(cp.get()));
}

void
ProcessPriorityManagerImpl::ObserveContentParentDestroyed(nsISupports* aSubject)
{
  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  NS_ENSURE_TRUE_VOID(childID != CONTENT_PROCESS_ID_UNKNOWN);

  nsRefPtr<ParticularProcessPriorityManager> pppm;
  mParticularManagers.Get(childID, &pppm);
  MOZ_ASSERT(pppm);
  if (pppm) {
    pppm->ShutDown();
  }

  mParticularManagers.Remove(childID);
}

NS_IMPL_ISUPPORTS2(ParticularProcessPriorityManager,
                   nsIObserver,
                   nsITimerCallback);

ParticularProcessPriorityManager::ParticularProcessPriorityManager(
  ContentParent* aContentParent)
  : mContentParent(aContentParent)
  , mChildID(aContentParent->ChildID())
  , mPriority(PROCESS_PRIORITY_UNKNOWN)
  , mHoldsCPUWakeLock(false)
  , mHoldsHighPriorityWakeLock(false)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  LOGP("Creating ParticularProcessPriorityManager.");
}

void
ParticularProcessPriorityManager::Init()
{
  RegisterWakeLockObserver(this);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "audio-channel-process-changed", /* ownsWeak */ false);
    os->AddObserver(this, "remote-browser-frame-shown", /* ownsWeak */ false);
    os->AddObserver(this, "ipc:browser-destroyed", /* ownsWeak */ false);
    os->AddObserver(this, "frameloader-visible-changed", /* ownsWeak */ false);
  }

  // This process may already hold the CPU lock; for example, our parent may
  // have acquired it on our behalf.
  WakeLockInformation info1, info2;
  GetWakeLockInfo(NS_LITERAL_STRING("cpu"), &info1);
  mHoldsCPUWakeLock = info1.lockingProcesses().Contains(ChildID());

  GetWakeLockInfo(NS_LITERAL_STRING("high-priority"), &info2);
  mHoldsHighPriorityWakeLock = info2.lockingProcesses().Contains(ChildID());
  LOGP("Done starting up.  mHoldsCPUWakeLock=%d, mHoldsHighPriorityWakeLock=%d",
       mHoldsCPUWakeLock, mHoldsHighPriorityWakeLock);
}

ParticularProcessPriorityManager::~ParticularProcessPriorityManager()
{
  LOGP("Destroying ParticularProcessPriorityManager.");
  UnregisterWakeLockObserver(this);
}

/* virtual */ void
ParticularProcessPriorityManager::Notify(const WakeLockInformation& aInfo)
{
  bool* dest = nullptr;
  if (aInfo.topic().EqualsLiteral("cpu")) {
    dest = &mHoldsCPUWakeLock;
  } else if (aInfo.topic().EqualsLiteral("high-priority")) {
    dest = &mHoldsHighPriorityWakeLock;
  }

  if (dest) {
    bool thisProcessLocks = aInfo.lockingProcesses().Contains(ChildID());
    if (thisProcessLocks != *dest) {
      *dest = thisProcessLocks;
      LOGP("Got wake lock changed event. "
           "Now mHoldsCPUWakeLock=%d, mHoldsHighPriorityWakeLock=%d",
           mHoldsCPUWakeLock, mHoldsHighPriorityWakeLock);
      ResetPriority();
    }
  }
}

NS_IMETHODIMP
ParticularProcessPriorityManager::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const PRUnichar* aData)
{
  if (!mContentParent) {
    // We've been shut down.
    return NS_OK;
  }

  nsDependentCString topic(aTopic);

  if (topic.EqualsLiteral("audio-channel-process-changed")) {
    OnAudioChannelProcessChanged(aSubject);
  } else if (topic.EqualsLiteral("remote-browser-frame-shown")) {
    OnRemoteBrowserFrameShown(aSubject);
  } else if (topic.EqualsLiteral("ipc:browser-destroyed")) {
    OnTabParentDestroyed(aSubject);
  } else if (topic.EqualsLiteral("frameloader-visible-changed")) {
    OnFrameloaderVisibleChanged(aSubject);
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

uint64_t
ParticularProcessPriorityManager::ChildID() const
{
  // We have to cache mContentParent->ChildID() instead of getting it from the
  // ContentParent each time because after ShutDown() is called, mContentParent
  // is null.  If we didn't cache ChildID(), then we wouldn't be able to run
  // LOGP() after ShutDown().
  return mChildID;
}

int32_t
ParticularProcessPriorityManager::Pid() const
{
  return mContentParent ? mContentParent->Pid() : -1;
}

const nsAutoCString&
ParticularProcessPriorityManager::NameWithComma()
{
  mNameWithComma.Truncate();
  if (!mContentParent) {
    return mNameWithComma; // empty string
  }

  nsAutoString name;
  mContentParent->FriendlyName(name);
  if (name.IsEmpty()) {
    return mNameWithComma; // empty string
  }

  mNameWithComma = NS_ConvertUTF16toUTF8(name);
  mNameWithComma.AppendLiteral(", ");
  return mNameWithComma;
}

void
ParticularProcessPriorityManager::OnAudioChannelProcessChanged(nsISupports* aSubject)
{
  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(props);

  uint64_t childID = CONTENT_PROCESS_ID_UNKNOWN;
  props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"), &childID);
  if (childID == ChildID()) {
    ResetPriority();
  }
}

void
ParticularProcessPriorityManager::OnRemoteBrowserFrameShown(nsISupports* aSubject)
{
  nsCOMPtr<nsIFrameLoader> fl = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(fl);

  nsCOMPtr<nsITabParent> tp;
  fl->GetTabParent(getter_AddRefs(tp));
  NS_ENSURE_TRUE_VOID(tp);

  if (static_cast<TabParent*>(tp.get())->Manager() != mContentParent) {
    return;
  }

  ResetPriority();
}

void
ParticularProcessPriorityManager::OnTabParentDestroyed(nsISupports* aSubject)
{
  nsCOMPtr<nsITabParent> tp = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(tp);

  if (static_cast<TabParent*>(tp.get())->Manager() != mContentParent) {
    return;
  }

  ResetPriority();
}

void
ParticularProcessPriorityManager::OnFrameloaderVisibleChanged(nsISupports* aSubject)
{
  nsCOMPtr<nsIFrameLoader> fl = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(fl);

  nsCOMPtr<nsITabParent> tp;
  fl->GetTabParent(getter_AddRefs(tp));
  if (!tp) {
    return;
  }

  if (static_cast<TabParent*>(tp.get())->Manager() != mContentParent) {
    return;
  }

  // Most of the time when something changes in a process we call
  // ResetPriority(), giving a grace period before downgrading its priority.
  // But notice that here don't give a grace period: We call ResetPriorityNow()
  // instead.
  //
  // We do this because we're reacting here to a setVisibility() call, which is
  // an explicit signal from the process embedder that we should re-prioritize
  // a process.  If we gave a grace period in response to setVisibility()
  // calls, it would be impossible for the embedder to explicitly prioritize
  // processes and prevent e.g. the case where we switch which process is in
  // the foreground and, during the old fg processs's grace period, it OOMs the
  // new fg process.

  ResetPriorityNow();
}

void
ParticularProcessPriorityManager::ResetPriority()
{
  ProcessPriority processPriority = ComputePriority();
  if (mPriority == PROCESS_PRIORITY_UNKNOWN ||
      mPriority > processPriority) {
    ScheduleResetPriority("backgroundGracePeriodMS");
    return;
  }

  SetPriorityNow(processPriority);
}

void
ParticularProcessPriorityManager::ResetPriorityNow()
{
  SetPriorityNow(ComputePriority());
}

void
ParticularProcessPriorityManager::ScheduleResetPriority(const char* aTimeoutPref)
{
  if (mResetPriorityTimer) {
    LOGP("ScheduleResetPriority bailing; the timer is already running.");
    return;
  }

  uint32_t timeout = Preferences::GetUint(
    nsPrintfCString("dom.ipc.processPriorityManager.%s", aTimeoutPref).get());
  LOGP("Scheduling reset timer to fire in %dms.", timeout);
  mResetPriorityTimer = do_CreateInstance("@mozilla.org/timer;1");
  mResetPriorityTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
}


NS_IMETHODIMP
ParticularProcessPriorityManager::Notify(nsITimer* aTimer)
{
  LOGP("Reset priority timer callback; about to ResetPriorityNow.");
  ResetPriorityNow();
  mResetPriorityTimer = nullptr;
  return NS_OK;
}

bool
ParticularProcessPriorityManager::HasAppType(const char* aAppType)
{
  const InfallibleTArray<PBrowserParent*>& browsers =
    mContentParent->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); i++) {
    nsAutoString appType;
    static_cast<TabParent*>(browsers[i])->GetAppType(appType);
    if (appType.EqualsASCII(aAppType)) {
      return true;
    }
  }

  return false;
}

bool
ParticularProcessPriorityManager::IsExpectingSystemMessage()
{
  const InfallibleTArray<PBrowserParent*>& browsers =
    mContentParent->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); i++) {
    TabParent* tp = static_cast<TabParent*>(browsers[i]);
    nsCOMPtr<nsIDOMElement> ownerElement = tp->GetOwnerElement();
    nsCOMPtr<nsIMozBrowserFrame> bf = do_QueryInterface(ownerElement);
    if (!bf) {
      continue;
    }

    if (bf->GetIsExpectingSystemMessage()) {
      return true;
    }
  }

  return false;
}

ProcessPriority
ParticularProcessPriorityManager::ComputePriority()
{
  if ((mHoldsCPUWakeLock || mHoldsHighPriorityWakeLock) &&
      HasAppType("critical")) {
    return PROCESS_PRIORITY_FOREGROUND_HIGH;
  }

  bool isVisible = false;
  const InfallibleTArray<PBrowserParent*>& browsers =
    mContentParent->ManagedPBrowserParent();
  for (uint32_t i = 0; i < browsers.Length(); i++) {
    if (static_cast<TabParent*>(browsers[i])->IsVisible()) {
      isVisible = true;
      break;
    }
  }

  if (isVisible) {
    return PROCESS_PRIORITY_FOREGROUND;
  }

  if ((mHoldsCPUWakeLock || mHoldsHighPriorityWakeLock) &&
      IsExpectingSystemMessage()) {
    return PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE;
  }

  AudioChannelService* service = AudioChannelService::GetAudioChannelService();
  if (service->ProcessContentOrNormalChannelIsActive(ChildID())) {
    return PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE;
  }

  return HasAppType("homescreen") ?
         PROCESS_PRIORITY_BACKGROUND_HOMESCREEN :
         PROCESS_PRIORITY_BACKGROUND;
}

void
ParticularProcessPriorityManager::SetPriorityNow(ProcessPriority aPriority)
{
  if (aPriority == PROCESS_PRIORITY_UNKNOWN) {
    MOZ_ASSERT(false);
    return;
  }

  if (mPriority == aPriority) {
    return;
  }

  // If the prefs were disabled after this ParticularProcessPriorityManager was
  // created, we can at least avoid any further calls to
  // hal::SetProcessPriority.  Supporting dynamic enabling/disabling of the
  // ProcessPriorityManager is mostly for testing.
  if (!ProcessPriorityManagerImpl::PrefsEnabled()) {
    return;
  }

  LOGP("Changing priority from %s to %s.",
       ProcessPriorityToString(mPriority),
       ProcessPriorityToString(aPriority));
  mPriority = aPriority;
  hal::SetProcessPriority(Pid(), mPriority);

  unused << mContentParent->SendNotifyProcessPriorityChanged(mPriority);

  if (aPriority >= PROCESS_PRIORITY_FOREGROUND) {
    unused << mContentParent->SendCancelMinimizeMemoryUsage();
  } else {
    unused << mContentParent->SendMinimizeMemoryUsage();
  }

  FireTestOnlyObserverNotification("process-priority-set",
                                   ProcessPriorityToString(mPriority));
}

void
ParticularProcessPriorityManager::ShutDown()
{
  MOZ_ASSERT(mContentParent);

  if (mResetPriorityTimer) {
    mResetPriorityTimer->Cancel();
    mResetPriorityTimer = nullptr;
  }

  mContentParent = nullptr;
}

void
ProcessPriorityManagerImpl::FireTestOnlyObserverNotification(
  const char* aTopic,
  const nsACString& aData /* = EmptyCString() */)
{
  if (!Preferences::GetBool("dom.ipc.processPriorityManager.testMode")) {
    return;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(os);

  nsPrintfCString topic("process-priority-manager:TEST-ONLY:%s", aTopic);

  LOG("Notifying observer %s, data %s",
      topic.get(), PromiseFlatCString(aData).get());
  os->NotifyObservers(nullptr, topic.get(), NS_ConvertUTF8toUTF16(aData).get());
}

void
ParticularProcessPriorityManager::FireTestOnlyObserverNotification(
  const char* aTopic,
  const char* aData /* = nullptr */ )
{
  if (!Preferences::GetBool("dom.ipc.processPriorityManager.testMode")) {
    return;
  }

  nsAutoCString data;
  if (aData) {
    data.AppendASCII(aData);
  }

  FireTestOnlyObserverNotification(aTopic, data);
}

void
ParticularProcessPriorityManager::FireTestOnlyObserverNotification(
  const char* aTopic,
  const nsACString& aData /* = EmptyCString() */)
{
  if (!Preferences::GetBool("dom.ipc.processPriorityManager.testMode")) {
    return;
  }

  nsAutoCString data(nsPrintfCString("%lld", ChildID()));
  if (!aData.IsEmpty()) {
    data.AppendLiteral(":");
    data.Append(aData);
  }

  // ProcessPriorityManagerImpl::GetSingleton() is guaranteed not to return
  // null, since ProcessPriorityManagerImpl is the only class which creates
  // ParticularProcessPriorityManagers.

  ProcessPriorityManagerImpl::GetSingleton()->
    FireTestOnlyObserverNotification(aTopic, data);
}

StaticRefPtr<ProcessPriorityManagerChild>
ProcessPriorityManagerChild::sSingleton;

/* static */ void
ProcessPriorityManagerChild::StaticInit()
{
  if (!sSingleton) {
    sSingleton = new ProcessPriorityManagerChild();
    ClearOnShutdown(&sSingleton);
  }
}

/* static */ ProcessPriorityManagerChild*
ProcessPriorityManagerChild::Singleton()
{
  StaticInit();
  return sSingleton;
}

NS_IMPL_ISUPPORTS1(ProcessPriorityManagerChild,
                   nsIObserver)

ProcessPriorityManagerChild::ProcessPriorityManagerChild()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    mCachedPriority = PROCESS_PRIORITY_MASTER;
  } else {
    mCachedPriority = PROCESS_PRIORITY_UNKNOWN;
  }
}

void
ProcessPriorityManagerChild::Init()
{
  // The process priority should only be changed in child processes; don't even
  // bother listening for changes if we're in the main process.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_TRUE_VOID(os);
    os->AddObserver(this, "ipc:process-priority-changed", /* weak = */ false);
  }
}

NS_IMETHODIMP
ProcessPriorityManagerChild::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const PRUnichar* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, "ipc:process-priority-changed"));

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(props, NS_OK);

  int32_t priority = static_cast<int32_t>(PROCESS_PRIORITY_UNKNOWN);
  props->GetPropertyAsInt32(NS_LITERAL_STRING("priority"), &priority);
  NS_ENSURE_TRUE(ProcessPriority(priority) != PROCESS_PRIORITY_UNKNOWN, NS_OK);

  mCachedPriority = static_cast<ProcessPriority>(priority);

  return NS_OK;
}

bool
ProcessPriorityManagerChild::CurrentProcessIsForeground()
{
  return mCachedPriority == PROCESS_PRIORITY_UNKNOWN ||
         mCachedPriority >= PROCESS_PRIORITY_FOREGROUND;
}

} // anonymous namespace

namespace mozilla {

/* static */ void
ProcessPriorityManager::Init()
{
  ProcessPriorityManagerImpl::StaticInit();
  ProcessPriorityManagerChild::StaticInit();
}

/* static */ void
ProcessPriorityManager::SetProcessPriority(ContentParent* aContentParent,
                                           ProcessPriority aPriority)
{
  MOZ_ASSERT(aContentParent);

  ProcessPriorityManagerImpl* singleton =
    ProcessPriorityManagerImpl::GetSingleton();
  if (singleton) {
    singleton->SetProcessPriority(aContentParent, aPriority);
  }
}

/* static */ bool
ProcessPriorityManager::CurrentProcessIsForeground()
{
  return ProcessPriorityManagerChild::Singleton()->
    CurrentProcessIsForeground();
}

} // namespace mozilla
