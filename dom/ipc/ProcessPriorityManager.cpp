/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessPriorityManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Hal.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "AudioChannelService.h"
#include "mozilla/Logging.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsIFrameLoader.h"
#include "nsIObserverService.h"
#include "StaticPtr.h"
#include "nsIMozBrowserFrame.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsIPropertyBag2.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"

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
      "[%schild-id=%" PRIu64 ", pid=%d] " fmt, \
      NameWithComma().get(), \
      static_cast<uint64_t>(ChildID()), Pid(), ## __VA_ARGS__)

#elif defined(ENABLE_LOGGING)
#  define LOG(fmt, ...) \
     printf("ProcessPriorityManager - " fmt "\n", ##__VA_ARGS__)
#  define LOGP(fmt, ...) \
     printf("ProcessPriorityManager[%schild-id=%" PRIu64 ", pid=%d] - " \
       fmt "\n", \
       NameWithComma().get(), \
       static_cast<uint64_t>(ChildID()), Pid(), ##__VA_ARGS__)
#else
  static PRLogModuleInfo*
  GetPPMLog()
  {
    static PRLogModuleInfo *sLog;
    if (!sLog)
      sLog = PR_NewLogModule("ProcessPriorityManager");
    return sLog;
  }
#  define LOG(fmt, ...) \
     MOZ_LOG(GetPPMLog(), LogLevel::Debug, \
            ("ProcessPriorityManager - " fmt, ##__VA_ARGS__))
#  define LOGP(fmt, ...) \
     MOZ_LOG(GetPPMLog(), LogLevel::Debug, \
            ("ProcessPriorityManager[%schild-id=%" PRIu64 ", pid=%d] - " fmt, \
            NameWithComma().get(), \
            static_cast<uint64_t>(ChildID()), Pid(), ##__VA_ARGS__))
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace {

class ParticularProcessPriorityManager;

class ProcessLRUPool final
{
public:
  /**
   * Creates a new process LRU pool for the specified priority.
   */
  explicit ProcessLRUPool(ProcessPriority aPriority);

  /**
   * Used to remove a particular process priority manager from the LRU pool
   * when the associated ContentParent is destroyed or its priority changes.
   */
  void Remove(ParticularProcessPriorityManager* aParticularManager);

  /**
   * Used to add a particular process priority manager into the LRU pool when
   * the associated ContentParent's priority changes.
   */
  void Add(ParticularProcessPriorityManager* aParticularManager);

private:
  ProcessPriority mPriority;
  uint32_t mLRUPoolLevels;
  nsTArray<ParticularProcessPriorityManager*> mLRUPool;

  uint32_t CalculateLRULevel(uint32_t aLRUPoolIndex);

  void AdjustLRUValues(
    nsTArray<ParticularProcessPriorityManager*>::index_type aStart,
    bool removed);

  DISALLOW_EVIL_CONSTRUCTORS(ProcessLRUPool);
};

/**
 * This singleton class does the work to implement the process priority manager
 * in the main process.  This class may not be used in child processes.  (You
 * can call StaticInit, but it won't do anything, and GetSingleton() will
 * return null.)
 *
 * ProcessPriorityManager::CurrentProcessIsForeground() and
 * ProcessPriorityManager::AnyProcessHasHighPriority() which can be called in
 * any process, are handled separately, by the ProcessPriorityManagerChild
 * class.
 */
class ProcessPriorityManagerImpl final
  : public nsIObserver
  , public WakeLockObserver
  , public nsSupportsWeakReference
{
public:
  /**
   * If we're in the main process, get the ProcessPriorityManagerImpl
   * singleton.  If we're in a child process, return null.
   */
  static ProcessPriorityManagerImpl* GetSingleton();

  static void StaticInit();
  static bool PrefsEnabled();
  static bool TestMode();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * This function implements ProcessPriorityManager::SetProcessPriority.
   */
  void SetProcessPriority(ContentParent* aContentParent,
                          ProcessPriority aPriority,
                          uint32_t aLRU = 0);

  /**
   * If a magic testing-only pref is set, notify the observer service on the
   * given topic with the given data.  This is used for testing
   */
  void FireTestOnlyObserverNotification(const char* aTopic,
                                        const nsACString& aData = EmptyCString());

  /**
   * Does one of the child processes have priority FOREGROUND_HIGH?
   */
  bool ChildProcessHasHighPriority();

  /**
   * This must be called by a ParticularProcessPriorityManager when it changes
   * its priority.
   */
  void NotifyProcessPriorityChanged(
    ParticularProcessPriorityManager* aParticularManager,
    hal::ProcessPriority aOldPriority);

  /**
   * Implements WakeLockObserver, used to monitor wake lock changes in the
   * main process.
   */
  virtual void Notify(const WakeLockInformation& aInfo) override;

  /**
   * Prevents processes from changing priority until unfrozen.
   */
  void Freeze();

  /**
   * Allow process' priorities to change again.  This will immediately adjust
   * processes whose priority change did not happen because of the freeze.
   */
  void Unfreeze();

  /**
   * Call ShutDown before destroying the ProcessPriorityManager because
   * WakeLockObserver hols a strong reference to it.
   */
  void ShutDown();

private:
  static bool sPrefsEnabled;
  static bool sRemoteTabsDisabled;
  static bool sTestMode;
  static bool sPrefListenersRegistered;
  static bool sInitialized;
  static bool sFrozen;
  static StaticRefPtr<ProcessPriorityManagerImpl> sSingleton;

  static void PrefChangedCallback(const char* aPref, void* aClosure);

  ProcessPriorityManagerImpl();
  ~ProcessPriorityManagerImpl();
  DISALLOW_EVIL_CONSTRUCTORS(ProcessPriorityManagerImpl);

  void Init();

  already_AddRefed<ParticularProcessPriorityManager>
  GetParticularProcessPriorityManager(ContentParent* aContentParent);

  void ObserveContentParentCreated(nsISupports* aContentParent);
  void ObserveContentParentDestroyed(nsISupports* aSubject);
  void ObserveScreenStateChanged(const char16_t* aData);

  nsDataHashtable<nsUint64HashKey, nsRefPtr<ParticularProcessPriorityManager> >
    mParticularManagers;

  /** True if the main process is holding a high-priority wakelock */
  bool mHighPriority;

  /** Contains the PIDs of child processes holding high-priority wakelocks */
  nsTHashtable<nsUint64HashKey> mHighPriorityChildIDs;

  /** Contains a pseudo-LRU list of background processes */
  ProcessLRUPool mBackgroundLRUPool;

  /** Contains a pseudo-LRU list of background-perceivable processes */
  ProcessLRUPool mBackgroundPerceivableLRUPool;
};

/**
 * This singleton class implements the parts of the process priority manager
 * that are available from all processes.
 */
class ProcessPriorityManagerChild final
  : public nsIObserver
{
public:
  static void StaticInit();
  static ProcessPriorityManagerChild* Singleton();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  bool CurrentProcessIsForeground();
  bool CurrentProcessIsHighPriority();

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
class ParticularProcessPriorityManager final
  : public WakeLockObserver
  , public nsIObserver
  , public nsITimerCallback
  , public nsSupportsWeakReference
{
  ~ParticularProcessPriorityManager();
public:
  explicit ParticularProcessPriorityManager(ContentParent* aContentParent,
                                            bool aFrozen = false);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  virtual void Notify(const WakeLockInformation& aInfo) override;
  static void StaticInit();
  void Init();

  int32_t Pid() const;
  uint64_t ChildID() const;
  bool IsPreallocated() const;

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
  void OnActivityOpened(const char16_t* aData);
  void OnActivityClosed(const char16_t* aData);

  ProcessPriority CurrentPriority();
  ProcessPriority ComputePriority();

  enum TimeoutPref {
    BACKGROUND_PERCEIVABLE_GRACE_PERIOD,
    BACKGROUND_GRACE_PERIOD,
  };

  void ScheduleResetPriority(TimeoutPref aTimeoutPref);
  void ResetPriority();
  void ResetPriorityNow();
  void SetPriorityNow(ProcessPriority aPriority, uint32_t aLRU = 0);
  void Freeze();
  void Unfreeze();

  void ShutDown();

private:
  static uint32_t sBackgroundPerceivableGracePeriodMS;
  static uint32_t sBackgroundGracePeriodMS;

  void FireTestOnlyObserverNotification(
    const char* aTopic,
    const nsACString& aData = EmptyCString());

  void FireTestOnlyObserverNotification(
    const char* aTopic,
    const char* aData = nullptr);

  ContentParent* mContentParent;
  uint64_t mChildID;
  ProcessPriority mPriority;
  uint32_t mLRU;
  bool mHoldsCPUWakeLock;
  bool mHoldsHighPriorityWakeLock;
  bool mIsActivityOpener;
  bool mFrozen;

  /**
   * Used to implement NameWithComma().
   */
  nsAutoCString mNameWithComma;

  nsCOMPtr<nsITimer> mResetPriorityTimer;
};

/* static */ bool ProcessPriorityManagerImpl::sInitialized = false;
/* static */ bool ProcessPriorityManagerImpl::sPrefsEnabled = false;
/* static */ bool ProcessPriorityManagerImpl::sRemoteTabsDisabled = true;
/* static */ bool ProcessPriorityManagerImpl::sTestMode = false;
/* static */ bool ProcessPriorityManagerImpl::sPrefListenersRegistered = false;
/* static */ bool ProcessPriorityManagerImpl::sFrozen = false;
/* static */ StaticRefPtr<ProcessPriorityManagerImpl>
  ProcessPriorityManagerImpl::sSingleton;
/* static */ uint32_t ParticularProcessPriorityManager::sBackgroundPerceivableGracePeriodMS = 0;
/* static */ uint32_t ParticularProcessPriorityManager::sBackgroundGracePeriodMS = 0;

NS_IMPL_ISUPPORTS(ProcessPriorityManagerImpl,
                  nsIObserver,
                  nsISupportsWeakReference);

/* static */ void
ProcessPriorityManagerImpl::PrefChangedCallback(const char* aPref,
                                                void* aClosure)
{
  StaticInit();
  if (!PrefsEnabled() && sSingleton) {
    sSingleton->ShutDown();
    sSingleton = nullptr;
    sInitialized = false;
  }
}

/* static */ bool
ProcessPriorityManagerImpl::PrefsEnabled()
{
  return sPrefsEnabled && !sRemoteTabsDisabled;
}

/* static */ bool
ProcessPriorityManagerImpl::TestMode()
{
  return sTestMode;
}

/* static */ void
ProcessPriorityManagerImpl::StaticInit()
{
  if (sInitialized) {
    return;
  }

  // The process priority manager is main-process only.
  if (!XRE_IsParentProcess()) {
    sInitialized = true;
    return;
  }

  if (!sPrefListenersRegistered) {
    Preferences::AddBoolVarCache(&sPrefsEnabled,
                                 "dom.ipc.processPriorityManager.enabled");
    Preferences::AddBoolVarCache(&sRemoteTabsDisabled,
                                 "dom.ipc.tabs.disabled");
    Preferences::AddBoolVarCache(&sTestMode,
                                 "dom.ipc.processPriorityManager.testMode");
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
    : mHighPriority(false)
    , mBackgroundLRUPool(PROCESS_PRIORITY_BACKGROUND)
    , mBackgroundPerceivableLRUPool(PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  RegisterWakeLockObserver(this);
}

ProcessPriorityManagerImpl::~ProcessPriorityManagerImpl()
{
  ShutDown();
}

void
ProcessPriorityManagerImpl::ShutDown()
{
  UnregisterWakeLockObserver(this);
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
    os->AddObserver(this, "ipc:content-created", /* ownsWeak */ true);
    os->AddObserver(this, "ipc:content-shutdown", /* ownsWeak */ true);
    os->AddObserver(this, "screen-state-changed", /* ownsWeak */ true);
  }
}

NS_IMETHODIMP
ProcessPriorityManagerImpl::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const char16_t* aData)
{
  nsDependentCString topic(aTopic);
  if (topic.EqualsLiteral("ipc:content-created")) {
    ObserveContentParentCreated(aSubject);
  } else if (topic.EqualsLiteral("ipc:content-shutdown")) {
    ObserveContentParentDestroyed(aSubject);
  } else if (topic.EqualsLiteral("screen-state-changed")) {
    ObserveScreenStateChanged(aData);
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

already_AddRefed<ParticularProcessPriorityManager>
ProcessPriorityManagerImpl::GetParticularProcessPriorityManager(
  ContentParent* aContentParent)
{
#ifdef MOZ_NUWA_PROCESS
  // Do not attempt to change the priority of the Nuwa process
  if (aContentParent->IsNuwaProcess()) {
    return nullptr;
  }
#endif

  nsRefPtr<ParticularProcessPriorityManager> pppm;
  uint64_t cpId = aContentParent->ChildID();
  mParticularManagers.Get(cpId, &pppm);
  if (!pppm) {
    pppm = new ParticularProcessPriorityManager(aContentParent, sFrozen);
    pppm->Init();
    mParticularManagers.Put(cpId, pppm);

    FireTestOnlyObserverNotification("process-created",
      nsPrintfCString("%lld", cpId));
  }

  return pppm.forget();
}

void
ProcessPriorityManagerImpl::SetProcessPriority(ContentParent* aContentParent,
                                               ProcessPriority aPriority,
                                               uint32_t aLRU)
{
  MOZ_ASSERT(aContentParent);
  nsRefPtr<ParticularProcessPriorityManager> pppm =
    GetParticularProcessPriorityManager(aContentParent);
  if (pppm) {
    pppm->SetPriorityNow(aPriority, aLRU);
  }
}

void
ProcessPriorityManagerImpl::ObserveContentParentCreated(
  nsISupports* aContentParent)
{
  // Do nothing; it's sufficient to get the PPPM.  But assign to nsRefPtr so we
  // don't leak the already_AddRefed object.
  nsCOMPtr<nsIContentParent> cp = do_QueryInterface(aContentParent);
  nsRefPtr<ParticularProcessPriorityManager> pppm =
    GetParticularProcessPriorityManager(cp->AsContentParent());
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
  if (pppm) {
    // Unconditionally remove the manager from the pools
    mBackgroundLRUPool.Remove(pppm);
    mBackgroundPerceivableLRUPool.Remove(pppm);

    pppm->ShutDown();

    mParticularManagers.Remove(childID);

    if (mHighPriorityChildIDs.Contains(childID)) {
      mHighPriorityChildIDs.RemoveEntry(childID);
    }
  }
}

static PLDHashOperator
FreezeParticularProcessPriorityManagers(
  const uint64_t& aKey,
  nsRefPtr<ParticularProcessPriorityManager> aValue,
  void* aUserData)
{
  aValue->Freeze();
  return PL_DHASH_NEXT;
}

static PLDHashOperator
UnfreezeParticularProcessPriorityManagers(
  const uint64_t& aKey,
  nsRefPtr<ParticularProcessPriorityManager> aValue,
  void* aUserData)
{
  aValue->Unfreeze();
  return PL_DHASH_NEXT;
}

void
ProcessPriorityManagerImpl::ObserveScreenStateChanged(const char16_t* aData)
{
  if (NS_LITERAL_STRING("on").Equals(aData)) {
    sFrozen = false;
    mParticularManagers.EnumerateRead(
      &UnfreezeParticularProcessPriorityManagers, nullptr);
  } else {
    sFrozen = true;
    mParticularManagers.EnumerateRead(
      &FreezeParticularProcessPriorityManagers, nullptr);
  }
}

bool
ProcessPriorityManagerImpl::ChildProcessHasHighPriority( void )
{
  return mHighPriorityChildIDs.Count() > 0;
}

void
ProcessPriorityManagerImpl::NotifyProcessPriorityChanged(
  ParticularProcessPriorityManager* aParticularManager,
  ProcessPriority aOldPriority)
{
  ProcessPriority newPriority = aParticularManager->CurrentPriority();
  bool isPreallocated = aParticularManager->IsPreallocated();

  if (newPriority == PROCESS_PRIORITY_BACKGROUND &&
      aOldPriority != PROCESS_PRIORITY_BACKGROUND &&
      !isPreallocated) {
    mBackgroundLRUPool.Add(aParticularManager);
  } else if (newPriority != PROCESS_PRIORITY_BACKGROUND &&
      aOldPriority == PROCESS_PRIORITY_BACKGROUND &&
      !isPreallocated) {
    mBackgroundLRUPool.Remove(aParticularManager);
  }

  if (newPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE &&
      aOldPriority != PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
    mBackgroundPerceivableLRUPool.Add(aParticularManager);
  } else if (newPriority != PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE &&
      aOldPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
    mBackgroundPerceivableLRUPool.Remove(aParticularManager);
  }

  if (newPriority >= PROCESS_PRIORITY_FOREGROUND_HIGH &&
    aOldPriority < PROCESS_PRIORITY_FOREGROUND_HIGH) {
    mHighPriorityChildIDs.PutEntry(aParticularManager->ChildID());
  } else if (newPriority < PROCESS_PRIORITY_FOREGROUND_HIGH &&
    aOldPriority >= PROCESS_PRIORITY_FOREGROUND_HIGH) {
    mHighPriorityChildIDs.RemoveEntry(aParticularManager->ChildID());
  }
}

/* virtual */ void
ProcessPriorityManagerImpl::Notify(const WakeLockInformation& aInfo)
{
  /* The main process always has an ID of 0, if it is present in the wake-lock
   * information then we explicitly requested a high-priority wake-lock for the
   * main process. */
  if (aInfo.topic().EqualsLiteral("high-priority")) {
    if (aInfo.lockingProcesses().Contains((uint64_t)0)) {
      mHighPriority = true;
    } else {
      mHighPriority = false;
    }

    LOG("Got wake lock changed event. "
        "Now mHighPriorityParent = %d\n", mHighPriority);
  }
}

NS_IMPL_ISUPPORTS(ParticularProcessPriorityManager,
                  nsIObserver,
                  nsITimerCallback,
                  nsISupportsWeakReference);

ParticularProcessPriorityManager::ParticularProcessPriorityManager(
  ContentParent* aContentParent, bool aFrozen)
  : mContentParent(aContentParent)
  , mChildID(aContentParent->ChildID())
  , mPriority(PROCESS_PRIORITY_UNKNOWN)
  , mLRU(0)
  , mHoldsCPUWakeLock(false)
  , mHoldsHighPriorityWakeLock(false)
  , mIsActivityOpener(false)
  , mFrozen(aFrozen)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  LOGP("Creating ParticularProcessPriorityManager.");
}

void
ParticularProcessPriorityManager::StaticInit()
{
  Preferences::AddUintVarCache(&sBackgroundPerceivableGracePeriodMS,
                               "dom.ipc.processPriorityManager.backgroundPerceivableGracePeriodMS");
  Preferences::AddUintVarCache(&sBackgroundGracePeriodMS,
                               "dom.ipc.processPriorityManager.backgroundGracePeriodMS");
}

void
ParticularProcessPriorityManager::Init()
{
  RegisterWakeLockObserver(this);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "audio-channel-process-changed", /* ownsWeak */ true);
    os->AddObserver(this, "remote-browser-shown", /* ownsWeak */ true);
    os->AddObserver(this, "ipc:browser-destroyed", /* ownsWeak */ true);
    os->AddObserver(this, "frameloader-visible-changed", /* ownsWeak */ true);
    os->AddObserver(this, "activity-opened", /* ownsWeak */ true);
    os->AddObserver(this, "activity-closed", /* ownsWeak */ true);
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

  // Unregister our wake lock observer if ShutDown hasn't been called.  (The
  // wake lock observer takes raw refs, so we don't want to take chances here!)
  // We don't call UnregisterWakeLockObserver unconditionally because the code
  // will print a warning if it's called unnecessarily.

  if (mContentParent) {
    UnregisterWakeLockObserver(this);
  }
}

/* virtual */ void
ParticularProcessPriorityManager::Notify(const WakeLockInformation& aInfo)
{
  if (!mContentParent) {
    // We've been shut down.
    return;
  }

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
                                          const char16_t* aData)
{
  if (!mContentParent) {
    // We've been shut down.
    return NS_OK;
  }

  nsDependentCString topic(aTopic);

  if (topic.EqualsLiteral("audio-channel-process-changed")) {
    OnAudioChannelProcessChanged(aSubject);
  } else if (topic.EqualsLiteral("remote-browser-shown")) {
    OnRemoteBrowserFrameShown(aSubject);
  } else if (topic.EqualsLiteral("ipc:browser-destroyed")) {
    OnTabParentDestroyed(aSubject);
  } else if (topic.EqualsLiteral("frameloader-visible-changed")) {
    OnFrameloaderVisibleChanged(aSubject);
  } else if (topic.EqualsLiteral("activity-opened")) {
    OnActivityOpened(aData);
  } else if (topic.EqualsLiteral("activity-closed")) {
    OnActivityClosed(aData);
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

bool
ParticularProcessPriorityManager::IsPreallocated() const
{
  return mContentParent ? mContentParent->IsPreallocated() : false;
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

  TabParent* tp = TabParent::GetFrom(fl);
  NS_ENSURE_TRUE_VOID(tp);

  MOZ_ASSERT(XRE_IsParentProcess());
  if (tp->Manager() != mContentParent) {
    return;
  }

  // Ignore notifications that aren't from a BrowserOrApp
  bool isBrowserOrApp;
  fl->GetOwnerIsBrowserOrAppFrame(&isBrowserOrApp);
  if (isBrowserOrApp) {
    ResetPriority();
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, "remote-browser-shown");
  }
}

void
ParticularProcessPriorityManager::OnTabParentDestroyed(nsISupports* aSubject)
{
  nsCOMPtr<nsITabParent> tp = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(tp);

  MOZ_ASSERT(XRE_IsParentProcess());
  if (TabParent::GetFrom(tp)->Manager() != mContentParent) {
    return;
  }

  ResetPriority();
}

void
ParticularProcessPriorityManager::OnFrameloaderVisibleChanged(nsISupports* aSubject)
{
  nsCOMPtr<nsIFrameLoader> fl = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE_VOID(fl);

  if (mFrozen) {
    return; // Ignore visibility changes when the screen is off
  }

  TabParent* tp = TabParent::GetFrom(fl);
  if (!tp) {
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());
  if (tp->Manager() != mContentParent) {
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
ParticularProcessPriorityManager::OnActivityOpened(const char16_t* aData)
{
  uint64_t childID = nsCRT::atoll(NS_ConvertUTF16toUTF8(aData).get());

  if (ChildID() == childID) {
    LOGP("Marking as activity opener");
    mIsActivityOpener = true;
    ResetPriority();
  }
}

void
ParticularProcessPriorityManager::OnActivityClosed(const char16_t* aData)
{
  uint64_t childID = nsCRT::atoll(NS_ConvertUTF16toUTF8(aData).get());

  if (ChildID() == childID) {
    LOGP("Unmarking as activity opener");
    mIsActivityOpener = false;
    ResetPriority();
  }
}

void
ParticularProcessPriorityManager::ResetPriority()
{
  ProcessPriority processPriority = ComputePriority();
  if (mPriority == PROCESS_PRIORITY_UNKNOWN ||
      mPriority > processPriority) {
    // Apps set at a perceivable background priority are often playing media.
    // Most media will have short gaps while changing tracks between songs,
    // switching videos, etc.  Give these apps a longer grace period so they
    // can get their next track started, if there is one, before getting
    // downgraded.
    if (mPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
      ScheduleResetPriority(BACKGROUND_PERCEIVABLE_GRACE_PERIOD);
    } else {
      ScheduleResetPriority(BACKGROUND_GRACE_PERIOD);
    }
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
ParticularProcessPriorityManager::ScheduleResetPriority(TimeoutPref aTimeoutPref)
{
  if (mResetPriorityTimer) {
    LOGP("ScheduleResetPriority bailing; the timer is already running.");
    return;
  }

  uint32_t timeout = 0;
  switch (aTimeoutPref) {
    case BACKGROUND_PERCEIVABLE_GRACE_PERIOD:
      timeout = sBackgroundPerceivableGracePeriodMS;
      break;
    case BACKGROUND_GRACE_PERIOD:
      timeout = sBackgroundGracePeriodMS;
      break;
    default:
      MOZ_ASSERT(false, "Unrecognized timeout pref");
      break;
  }

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
    TabParent::GetFrom(browsers[i])->GetAppType(appType);
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
    TabParent* tp = TabParent::GetFrom(browsers[i]);
    nsCOMPtr<nsIMozBrowserFrame> bf = do_QueryInterface(tp->GetOwnerElement());
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
ParticularProcessPriorityManager::CurrentPriority()
{
  return mPriority;
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
    if (TabParent::GetFrom(browsers[i])->IsVisible()) {
      isVisible = true;
      break;
    }
  }

  if (isVisible) {
    return HasAppType("inputmethod") ?
      PROCESS_PRIORITY_FOREGROUND_KEYBOARD :
      PROCESS_PRIORITY_FOREGROUND;
  }

  if ((mHoldsCPUWakeLock || mHoldsHighPriorityWakeLock) &&
      IsExpectingSystemMessage()) {
    return PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE;
  }

  nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service->ProcessContentOrNormalChannelIsActive(ChildID())) {
    return PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE;
  }

  return mIsActivityOpener ? PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE
                           : PROCESS_PRIORITY_BACKGROUND;
}

void
ParticularProcessPriorityManager::SetPriorityNow(ProcessPriority aPriority,
                                                 uint32_t aLRU)
{
  if (aPriority == PROCESS_PRIORITY_UNKNOWN) {
    MOZ_ASSERT(false);
    return;
  }

  if (!ProcessPriorityManagerImpl::PrefsEnabled() ||
      !mContentParent ||
      mFrozen ||
      ((mPriority == aPriority) && (mLRU == aLRU))) {
    return;
  }

  if ((mPriority == aPriority) && (mLRU != aLRU)) {
    mLRU = aLRU;
    hal::SetProcessPriority(Pid(), mPriority, aLRU);

    nsPrintfCString processPriorityWithLRU("%s:%d",
      ProcessPriorityToString(mPriority), aLRU);

    FireTestOnlyObserverNotification("process-priority-with-LRU-set",
      processPriorityWithLRU.get());
    return;
  }

  LOGP("Changing priority from %s to %s.",
       ProcessPriorityToString(mPriority),
       ProcessPriorityToString(aPriority));

  ProcessPriority oldPriority = mPriority;

  mPriority = aPriority;
  hal::SetProcessPriority(Pid(), mPriority);

  if (oldPriority != mPriority) {
    ProcessPriorityManagerImpl::GetSingleton()->
      NotifyProcessPriorityChanged(this, oldPriority);

    unused << mContentParent->SendNotifyProcessPriorityChanged(mPriority);
  }

  if (aPriority < PROCESS_PRIORITY_FOREGROUND) {
    unused << mContentParent->SendFlushMemory(NS_LITERAL_STRING("lowering-priority"));
  }

  FireTestOnlyObserverNotification("process-priority-set",
    ProcessPriorityToString(mPriority));
}

void
ParticularProcessPriorityManager::Freeze()
{
  mFrozen = true;
}

void
ParticularProcessPriorityManager::Unfreeze()
{
  mFrozen = false;
}

void
ParticularProcessPriorityManager::ShutDown()
{
  MOZ_ASSERT(mContentParent);

  UnregisterWakeLockObserver(this);

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
  if (!TestMode()) {
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
  if (!ProcessPriorityManagerImpl::TestMode()) {
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
  if (!ProcessPriorityManagerImpl::TestMode()) {
    return;
  }

  nsAutoCString data(nsPrintfCString("%lld", ChildID()));
  if (!aData.IsEmpty()) {
    data.Append(':');
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
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
}

/* static */ ProcessPriorityManagerChild*
ProcessPriorityManagerChild::Singleton()
{
  StaticInit();
  return sSingleton;
}

NS_IMPL_ISUPPORTS(ProcessPriorityManagerChild,
                  nsIObserver)

ProcessPriorityManagerChild::ProcessPriorityManagerChild()
{
  if (XRE_IsParentProcess()) {
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
  if (!XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_TRUE_VOID(os);
    os->AddObserver(this, "ipc:process-priority-changed", /* weak = */ false);
  }
}

NS_IMETHODIMP
ProcessPriorityManagerChild::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const char16_t* aData)
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

bool
ProcessPriorityManagerChild::CurrentProcessIsHighPriority()
{
  return mCachedPriority == PROCESS_PRIORITY_UNKNOWN ||
         mCachedPriority >= PROCESS_PRIORITY_FOREGROUND_HIGH;
}

ProcessLRUPool::ProcessLRUPool(ProcessPriority aPriority)
  : mPriority(aPriority)
  , mLRUPoolLevels(1)
{
  // We set mLRUPoolLevels according to our pref.
  // This value is used to set background process LRU pool
  const char* str = ProcessPriorityToString(aPriority);
  nsPrintfCString pref("dom.ipc.processPriorityManager.%s.LRUPoolLevels", str);

  Preferences::GetUint(pref.get(), &mLRUPoolLevels);

  // GonkHal defines OOM_ADJUST_MAX is 15 and b2g.js defines
  // PROCESS_PRIORITY_BACKGROUND's oom_score_adj is 667 and oom_adj is 10.
  // This means we can only have at most (15 -10 + 1) = 6 background LRU levels.
  // Similarly we can have at most 4 background perceivable LRU levels. We
  // should really be getting rid of oom_adj and just rely on oom_score_adj
  // only which would lift this constraint.
  MOZ_ASSERT(aPriority != PROCESS_PRIORITY_BACKGROUND || mLRUPoolLevels <= 6);
  MOZ_ASSERT(aPriority != PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE ||
             mLRUPoolLevels <= 4);

  // LRU pool size = 2 ^ (number of background LRU pool levels) - 1
  uint32_t LRUPoolSize = (1 << mLRUPoolLevels) - 1;

  LOG("Making %s LRU pool with size(%d)", str, LRUPoolSize);
}

uint32_t
ProcessLRUPool::CalculateLRULevel(uint32_t aLRU)
{
  // This is used to compute the LRU adjustment for the specified LRU position.
  // We use power-of-two groups with increasing adjustments that look like the
  // following:

  // Priority  : LRU0, LRU1
  // Priority+1: LRU2, LRU3
  // Priority+2: LRU4, LRU5, LRU6, LRU7
  // Priority+3: LRU8, LRU9, LRU10, LRU11, LRU12, LRU12, LRU13, LRU14, LRU15
  // ...
  // Priority+L-1: 2^(number of LRU pool levels - 1)
  // (End of buffer)

  int exp;
  unused << frexp(static_cast<double>(aLRU), &exp);
  uint32_t level = std::max(exp - 1, 0);

  return std::min(mLRUPoolLevels - 1, level);
}

void
ProcessLRUPool::Remove(ParticularProcessPriorityManager* aParticularManager)
{
  nsTArray<ParticularProcessPriorityManager*>::index_type index =
    mLRUPool.IndexOf(aParticularManager);

  if (index == nsTArray<ParticularProcessPriorityManager*>::NoIndex) {
    return;
  }

  mLRUPool.RemoveElementAt(index);
  AdjustLRUValues(index, /* removed */ true);

  LOG("Remove ChildID(%" PRIu64 ") from %s LRU pool",
      static_cast<uint64_t>(aParticularManager->ChildID()),
      ProcessPriorityToString(mPriority));
}

/*
 * Adjust the LRU values of all the processes in an LRU pool. When true the
 * `removed` parameter indicates that the processes were shifted left because
 * an element was removed; otherwise it means the elements were shifted right
 * as an element was added.
 */
void
ProcessLRUPool::AdjustLRUValues(
  nsTArray<ParticularProcessPriorityManager*>::index_type aStart,
  bool removed)
{
  uint32_t adj = (removed ? 2 : 1);

  for (nsTArray<ParticularProcessPriorityManager*>::index_type i = aStart;
       i < mLRUPool.Length();
       i++) {
    /* Check whether i is a power of two.  If so, then it crossed a LRU group
     * boundary and we need to assign its new process priority LRU. Note that
     * depending on the direction and the bias this test will pick different
     * elements. */
    if (((i + adj) & (i + adj - 1)) == 0) {
      mLRUPool[i]->SetPriorityNow(mPriority, CalculateLRULevel(i + 1));
    }
  }
}

void
ProcessLRUPool::Add(ParticularProcessPriorityManager* aParticularManager)
{
  // Shift the list in the pool, so we have room at index 0 for the newly added
  // manager
  mLRUPool.InsertElementAt(0, aParticularManager);
  AdjustLRUValues(1, /* removed */ false);

  LOG("Add ChildID(%" PRIu64 ") into %s LRU pool",
      static_cast<uint64_t>(aParticularManager->ChildID()),
      ProcessPriorityToString(mPriority));
}

} // namespace

namespace mozilla {

/* static */ void
ProcessPriorityManager::Init()
{
  ProcessPriorityManagerImpl::StaticInit();
  ProcessPriorityManagerChild::StaticInit();
  ParticularProcessPriorityManager::StaticInit();
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

/* static */ bool
ProcessPriorityManager::AnyProcessHasHighPriority()
{
  ProcessPriorityManagerImpl* singleton =
    ProcessPriorityManagerImpl::GetSingleton();

  if (singleton) {
    return singleton->ChildProcessHasHighPriority();
  } else {
    return ProcessPriorityManagerChild::Singleton()->
      CurrentProcessIsHighPriority();
  }
}

} // namespace mozilla
