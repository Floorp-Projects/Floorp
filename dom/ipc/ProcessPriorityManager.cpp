/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ipc/ProcessPriorityManager.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/HalTypes.h"
#include "mozilla/TimeStamp.h"
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

#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

using namespace mozilla::hal;

namespace mozilla {
namespace dom {
namespace ipc {

namespace {
static bool sInitialized = false;
class ProcessPriorityManager;
static StaticRefPtr<ProcessPriorityManager> sManager;

// Some header defines a LOG macro, but we don't want it here.
#ifdef LOG
#undef LOG
#endif

// Enable logging by setting
//
//   NSPR_LOG_MODULES=ProcessPriorityManager:5
//
// in your environment.  Or just comment out the "&& 0" below, if you're on
// Android/B2G.

#if defined(ANDROID) && 0
#include <android/log.h>
#define LOG(fmt, ...) \
  __android_log_print(ANDROID_LOG_INFO, \
      "Gecko:ProcessPriorityManager", \
      fmt, ## __VA_ARGS__)
#elif defined(PR_LOGGING)
static PRLogModuleInfo*
GetPPMLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("ProcessPriorityManager");
  return sLog;
}
#define LOG(fmt, ...) \
  PR_LOG(GetPPMLog(), PR_LOG_DEBUG,                                     \
         ("[%d] ProcessPriorityManager - " fmt, getpid(), ##__VA_ARGS__))
#else
#define LOG(fmt, ...)
#endif

uint64_t
GetContentChildID()
{
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (!contentChild) {
    return 0;
  }

  return contentChild->GetID();
}

/**
 * This class listens to various Gecko events and asks the hal back-end to
 * change this process's priority when it transitions between various states of
 * "importance".
 *
 * The process's priority determines its CPU priority and also how likely it is
 * to be killed when the system is running out of memory.
 *
 * The most basic dichotomy in the ProcessPriorityManager is between
 * "foreground" processes, which usually have at least one active docshell, and
 * "background" processes.
 *
 * An important heuristic here is that we don't always mark a process as having
 * "background" priority until it's met the requisite criteria for some amount
 * of time.
 *
 * We do this because otherwise there are cases where we'd thrash a process
 * between foreground and background priorities; for example, Gaia sometimes
 * releases and re-acquires CPU wake locks in quick succession.
 *
 * On the other hand, when the embedder of an <iframe mozbrowser> calls
 * setVisible(false) on an iframe, we immediately send the relevant process to
 * the background, if it has no other foreground docshells.  This is necessary
 * to ensure that when we load an app, the embedder first has a chance to send
 * the previous app into the background.  This ensures that there's only one
 * foreground app at a time, thus ensuring that we kill the right process if we
 * come under memory pressure.
 */
class ProcessPriorityManager MOZ_FINAL
  : public nsIObserver
  , public nsIDOMEventListener
  , public nsITimerCallback
  , public WakeLockObserver
{
public:
  ProcessPriorityManager();
  void Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  void Notify(const WakeLockInformation& aWakeLockInfo);

  ProcessPriority GetPriority() const { return mProcessPriority; }

  /**
   * This function doesn't do exactly what you might think; see the comment on
   * ProcessPriorityManager.h::TemporarilyLockProcessPriority().
   */
  void TemporarilyLockProcessPriority();

  /**
   * Recompute this process's priority and apply it, potentially after a brief
   * delay.
   *
   * If we are transitioning to a priority that is lower than the current
   * priority, that transition happens after a grace period.  Otherwise the
   * transition happens immediately.
   *
   * Note that PROCESS_PRIORITY_UNKNOWN is considered the highest priority.
   * Going from UNKNOWN to any other priority requires a grace period.
   */
  void ResetPriority();

  /**
   * Recompute this process's priority and apply it immediately.
   */
  void ResetPriorityNow();

private:
  void OnContentDocumentGlobalCreated(nsISupports* aOuterWindow);

  /**
   * Is this process a "critical" process that's holding the "CPU" or
   * "high-priority" wake lock?
   */
  bool IsCriticalProcessWithWakeLock();

  /**
   * Compute whether this process is in the foreground and return the result.
   */
  bool ComputeIsInForeground();

  /**
   * Compute the priority this process ought to have, based on what we know at
   * the moment.
   */
  ProcessPriority ComputePriority();

  /**
   * Immediately set this process's priority to the given priority.
   */
  void SetPriorityNow(ProcessPriority aPriority);

  /**
   * If mResetPriorityTimer is null (i.e., not running), create a timer and set
   * it to invoke ResetPriorityNow() after
   * dom.ipc.processPriorityManager.aTimeoutPref ms.
   */
  void ScheduleResetPriority(const char* aTimeoutPref);

  // Tracks whether this process holds the "cpu" lock.
  bool mHoldsCPUWakeLock;

  // Tracks whether this process holds the "high-priority" lock.
  bool mHoldsHighPriorityWakeLock;

  // mProcessPriority tracks the priority we've given this process in hal.
  ProcessPriority mProcessPriority;

  // Have we seen at least one tab-child-created event yet?  Until this is
  // true, ResetPriority() and ResetPriorityNow() do nothing.
  bool mObservedTabChildCreated;

  nsTArray<nsWeakPtr> mWindows;

  // When this timer expires, we set mResetPriorityTimer to null and run
  // ResetPriorityNow().
  nsCOMPtr<nsITimer> mResetPriorityTimer;

  nsWeakPtr mMemoryMinimizerRunnable;
};

NS_IMPL_ISUPPORTS3(ProcessPriorityManager,
                   nsIObserver,
                   nsIDOMEventListener,
                   nsITimerCallback)

ProcessPriorityManager::ProcessPriorityManager()
  : mHoldsCPUWakeLock(false)
  , mHoldsHighPriorityWakeLock(false)
  , mProcessPriority(ProcessPriority(-1))
  , mObservedTabChildCreated(false)
{
  // When our parent process forked us, it may have set our process's priority
  // to one of a few of the process priorities, depending on exactly why this
  // process was created.
  //
  // We don't know which priority we were given, so we set mProcessPriority to
  // -1 so that the next time ResetPriorityNow is run, we'll definitely call
  // into hal and set our priority.
}

void
ProcessPriorityManager::Init()
{
  LOG("Starting up.");

  // We can't do this in the constructor because we need to hold a strong ref
  // to |this| before calling these methods.
  //
  // Notice that we track /window/ creation and destruction even though our
  // notion of "is-foreground" is tied to /docshell/ activity.  We do this
  // because docshells don't fire an event when their visibility changes, but
  // windows do.
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  os->AddObserver(this, "tab-child-created", /* ownsWeak = */ false);
  os->AddObserver(this, "content-document-global-created", /* ownsWeak = */ false);
  os->AddObserver(this, "inner-window-destroyed", /* ownsWeak = */ false);
  os->AddObserver(this, "audio-channel-agent-changed", /* ownsWeak = */ false);
  os->AddObserver(this, "process-priority:reset-now", /* ownsWeak = */ false);

  RegisterWakeLockObserver(this);

  // This process may already hold the CPU lock; for example, our parent may
  // have acquired it on our behalf.
  WakeLockInformation info1, info2;
  GetWakeLockInfo(NS_LITERAL_STRING("cpu"), &info1);
  mHoldsCPUWakeLock = info1.lockingProcesses().Contains(GetContentChildID());

  GetWakeLockInfo(NS_LITERAL_STRING("high-priority"), &info2);
  mHoldsHighPriorityWakeLock = info2.lockingProcesses().Contains(GetContentChildID());

  LOG("Done starting up.  mHoldsCPUWakeLock=%d, mHoldsHighPriorityWakeLock=%d",
      mHoldsCPUWakeLock, mHoldsHighPriorityWakeLock);
}

NS_IMETHODIMP
ProcessPriorityManager::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const PRUnichar* aData)
{
  if (!strcmp(aTopic, "tab-child-created")) {
    mObservedTabChildCreated = true;
    ResetPriority();
  } else if (!strcmp(aTopic, "content-document-global-created")) {
    OnContentDocumentGlobalCreated(aSubject);
  } else if (!strcmp(aTopic, "inner-window-destroyed") ||
             !strcmp(aTopic, "audio-channel-agent-changed")) {
    ResetPriority();
  } else if (!strcmp(aTopic, "process-priority:reset-now")) {
    LOG("Got process-priority:reset-now notification.");
    ResetPriorityNow();
  } else {
    MOZ_ASSERT(false);
  }
  return NS_OK;
}

void
ProcessPriorityManager::Notify(const WakeLockInformation& aInfo)
{
  bool* dest = nullptr;
  if (aInfo.topic() == NS_LITERAL_STRING("cpu")) {
    dest = &mHoldsCPUWakeLock;
  } else if (aInfo.topic() == NS_LITERAL_STRING("high-priority")) {
    dest = &mHoldsHighPriorityWakeLock;
  }

  if (dest) {
    bool thisProcessLocks =
      aInfo.lockingProcesses().Contains(GetContentChildID());

    if (thisProcessLocks != *dest) {
      *dest = thisProcessLocks;
      LOG("Got wake lock changed event. "
          "Now mHoldsCPUWakeLock=%d, mHoldsHighPriorityWakeLock=%d",
          mHoldsCPUWakeLock, mHoldsHighPriorityWakeLock);
      ResetPriority();
    }
  }
}

NS_IMETHODIMP
ProcessPriorityManager::HandleEvent(
  nsIDOMEvent* aEvent)
{
  LOG("Got visibilitychange.");
  ResetPriority();
  return NS_OK;
}

void
ProcessPriorityManager::OnContentDocumentGlobalCreated(
  nsISupports* aOuterWindow)
{
  LOG("DocumentGlobalCreated");
  // Get the inner window (the topic of content-document-global-created is
  // the /outer/ window!).
  nsCOMPtr<nsPIDOMWindow> outerWindow = do_QueryInterface(aOuterWindow);
  NS_ENSURE_TRUE_VOID(outerWindow);
  nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();
  NS_ENSURE_TRUE_VOID(innerWindow);

  // We're only interested in top-level windows.
  nsCOMPtr<nsIDOMWindow> parentOuterWindow;
  innerWindow->GetScriptableParent(getter_AddRefs(parentOuterWindow));
  NS_ENSURE_TRUE_VOID(parentOuterWindow);
  if (parentOuterWindow != outerWindow) {
    return;
  }

  nsCOMPtr<EventTarget> target = do_QueryInterface(innerWindow);
  NS_ENSURE_TRUE_VOID(target);

  nsWeakPtr weakWin = do_GetWeakReference(innerWindow);
  NS_ENSURE_TRUE_VOID(weakWin);

  if (mWindows.Contains(weakWin)) {
    return;
  }

  target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                 this,
                                 /* useCapture = */ false,
                                 /* wantsUntrusted = */ false);

  mWindows.AppendElement(weakWin);

  ResetPriority();
}

bool
ProcessPriorityManager::IsCriticalProcessWithWakeLock()
{
  if (!(mHoldsCPUWakeLock || mHoldsHighPriorityWakeLock)) {
    return false;
  }

  ContentChild* contentChild = ContentChild::GetSingleton();
  if (!contentChild) {
    return false;
  }

  const InfallibleTArray<PBrowserChild*>& browsers =
    contentChild->ManagedPBrowserChild();
  for (uint32_t i = 0; i < browsers.Length(); i++) {
    nsAutoString appType;
    static_cast<TabChild*>(browsers[i])->GetAppType(appType);
    if (appType.EqualsLiteral("critical")) {
      return true;
    }
  }

  return false;
}

void
ProcessPriorityManager::ResetPriority()
{
  if (!mObservedTabChildCreated) {
    LOG("ResetPriority bailing because we haven't observed "
        "a tab-child-created event.");
    return;
  }

  ProcessPriority processPriority = ComputePriority();
  if (mProcessPriority == PROCESS_PRIORITY_UNKNOWN ||
      mProcessPriority > processPriority) {
    ScheduleResetPriority("backgroundGracePeriodMS");
    return;
  }

  SetPriorityNow(processPriority);
}

void
ProcessPriorityManager::ResetPriorityNow()
{
  if (!mObservedTabChildCreated) {
    LOG("ResetPriorityNow bailing because we haven't observed "
        "a tab-child-created event.");
    return;
  }

  SetPriorityNow(ComputePriority());
}

bool
ProcessPriorityManager::ComputeIsInForeground()
{
  // Critical processes holding the CPU/high-priority wake lock are always
  // considered to be in the foreground.
  if (IsCriticalProcessWithWakeLock()) {
    return true;
  }

  // We could try to be clever and keep a running count of the number of active
  // docshells, instead of iterating over mWindows every time one window's
  // visibility state changes.  But experience suggests that iterating over the
  // windows is prone to fewer errors (and one mistake doesn't mess you up for
  // the entire session).  Moreover, mWindows should be a very short list,
  // since it contains only top-level content windows.

  bool allHidden = true;
  for (uint32_t i = 0; i < mWindows.Length(); i++) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindows[i]);
    if (!window) {
      mWindows.RemoveElementAt(i);
      i--;
      continue;
    }

    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(window);
    if (!docshell) {
      continue;
    }

    bool isActive = false;
    docshell->GetIsActive(&isActive);


#ifdef DEBUG
    nsAutoCString spec;
    nsCOMPtr<nsIURI> uri = window->GetDocumentURI();
    if (uri) {
      uri->GetSpec(spec);
    }
    LOG("Docshell at %s has visibility %d.", spec.get(), isActive);
#endif

    allHidden = allHidden && !isActive;

    // We could break out early from this loop if
    //   isActive && mProcessPriority == BACKGROUND,
    // but then we might not clean up all the weak refs.
  }

  return !allHidden;
}

ProcessPriority
ProcessPriorityManager::ComputePriority()
{
  if (ComputeIsInForeground()) {
    if (IsCriticalProcessWithWakeLock()) {
      return PROCESS_PRIORITY_FOREGROUND_HIGH;
    }
    return PROCESS_PRIORITY_FOREGROUND;
  }

  AudioChannelService* service = AudioChannelService::GetAudioChannelService();
  if (service->ContentOrNormalChannelIsActive()) {
    return PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE;
  }

  bool isHomescreen = false;

  ContentChild* contentChild = ContentChild::GetSingleton();
  if (contentChild) {
    const InfallibleTArray<PBrowserChild*>& browsers =
      contentChild->ManagedPBrowserChild();
    for (uint32_t i = 0; i < browsers.Length(); i++) {
      nsAutoString appType;
      static_cast<TabChild*>(browsers[i])->GetAppType(appType);
      if (appType.EqualsLiteral("homescreen")) {
        isHomescreen = true;
        break;
      }
    }
  }

  return isHomescreen ?
         PROCESS_PRIORITY_BACKGROUND_HOMESCREEN :
         PROCESS_PRIORITY_BACKGROUND;
}

void
ProcessPriorityManager::SetPriorityNow(ProcessPriority aPriority)
{
  if (aPriority == PROCESS_PRIORITY_UNKNOWN) {
    MOZ_ASSERT(false);
    return;
  }

  if (mProcessPriority == aPriority) {
    return;
  }

  LOG("Changing priority from %s to %s.",
      ProcessPriorityToString(mProcessPriority),
      ProcessPriorityToString(aPriority));
  mProcessPriority = aPriority;
  hal::SetProcessPriority(getpid(), mProcessPriority);

  if (aPriority >= PROCESS_PRIORITY_FOREGROUND) {
    // Cancel the memory minimization procedure we might have started.
    nsCOMPtr<nsICancelableRunnable> runnable =
      do_QueryReferent(mMemoryMinimizerRunnable);
    if (runnable) {
      runnable->Cancel();
    }
  } else {
    // We're in the background; dump as much memory as we can.
    nsCOMPtr<nsIMemoryReporterManager> mgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr) {
      nsCOMPtr<nsICancelableRunnable> runnable =
        do_QueryReferent(mMemoryMinimizerRunnable);

      // Cancel the previous task if it's still pending
      if (runnable) {
        runnable->Cancel();
      }

      mgr->MinimizeMemoryUsage(/* callback = */ nullptr,
                               getter_AddRefs(runnable));
      mMemoryMinimizerRunnable = do_GetWeakReference(runnable);
    }
  }
}

void
ProcessPriorityManager::ScheduleResetPriority(const char* aTimeoutPref)
{
  if (mResetPriorityTimer) {
    LOG("ScheduleResetPriority bailing; the timer is already running.");
    return;
  }

  uint32_t timeout = Preferences::GetUint(
    nsPrintfCString("dom.ipc.processPriorityManager.%s", aTimeoutPref).get());
  LOG("Scheduling reset timer to fire in %dms.", timeout);
  mResetPriorityTimer = do_CreateInstance("@mozilla.org/timer;1");
  mResetPriorityTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
}

NS_IMETHODIMP
ProcessPriorityManager::Notify(nsITimer* aTimer)
{
  LOG("Reset priority timer callback; about to ResetPriorityNow.");
  ResetPriorityNow();
  mResetPriorityTimer = nullptr;
  return NS_OK;
}

void
ProcessPriorityManager::TemporarilyLockProcessPriority()
{
  LOG("TemporarilyLockProcessPriority");

  // Each call to TemporarilyLockProcessPriority gives us an additional
  // temporaryPriorityMS at our current priority (unless we receive a
  // process-priority:reset-now notification).  So cancel our timer if it's
  // running (which is due to a previous call to either
  // TemporarilyLockProcessPriority() or ResetPriority()).
  if (mResetPriorityTimer) {
    mResetPriorityTimer->Cancel();
    mResetPriorityTimer = nullptr;
  }
  ScheduleResetPriority("temporaryPriorityLockMS");
}

} // anonymous namespace

void
InitProcessPriorityManager()
{
  if (sInitialized) {
    return;
  }

  // If IPC tabs aren't enabled at startup, don't bother with any of this.
  if (!Preferences::GetBool("dom.ipc.processPriorityManager.enabled") ||
      Preferences::GetBool("dom.ipc.tabs.disabled")) {
    LOG("InitProcessPriorityManager bailing due to prefs.");
    return;
  }

  sInitialized = true;

  // If we're the master process, mark ourselves as such and don't create a
  // ProcessPriorityManager (we never want to mark the master process as
  // backgrounded).
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    LOG("This is the master process.");
    hal::SetProcessPriority(getpid(), PROCESS_PRIORITY_MASTER);
    return;
  }

  sManager = new ProcessPriorityManager();
  sManager->Init();
  ClearOnShutdown(&sManager);
}

bool
CurrentProcessIsForeground()
{
  // The process priority manager is the only thing which changes our priority,
  // so if the manager does not exist, then we must be in the foreground.
  if (!sManager) {
    return true;
  }

  return sManager->GetPriority() >= PROCESS_PRIORITY_FOREGROUND;
}

void
TemporarilyLockProcessPriority()
{
  if (sManager) {
    sManager->TemporarilyLockProcessPriority();
  } else {
    LOG("TemporarilyLockProcessPriority called before "
        "InitProcessPriorityManager.  Bailing.");
  }
}

} // namespace ipc
} // namespace dom
} // namespace mozilla
