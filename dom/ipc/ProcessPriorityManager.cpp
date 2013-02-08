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
#include "nsIDOMEventTarget.h"
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

/**
 * Get the appropriate backround priority for this process.
 */
ProcessPriority
GetBackgroundPriority()
{
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

/**
 * Determine if the priority is a backround priority.
 */
bool
IsBackgroundPriority(ProcessPriority aPriority)
{
  return (aPriority == PROCESS_PRIORITY_BACKGROUND ||
          aPriority == PROCESS_PRIORITY_BACKGROUND_HOMESCREEN ||
          aPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE);
}

/**
 * This class listens to window creation and visibilitychange events and
 * informs the hal back-end when this process transitions between having no
 * visible top-level windows, and when it has at least one visible top-level
 * window.
 *
 *
 * An important heuristic here is that we don't mark a process as background
 * until it's had no visible top-level windows for some amount of time.
 *
 * We do this because the notion of visibility is tied to inner windows
 * (actually, to documents).  When we navigate a page with outer window W, we
 * first destroy W's inner window and document, then insert a new inner window
 * and document into W.  If not for our grace period, this transition could
 * cause us to inform hal that this process quickly transitioned from
 * foreground to background to foreground again.
 *
 */
class ProcessPriorityManager MOZ_FINAL
  : public nsIObserver
  , public nsIDOMEventListener
  , public nsITimerCallback
{
public:
  ProcessPriorityManager();
  void Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  ProcessPriority GetPriority() const { return mProcessPriority; }

  /**
   * If this process is not already in the foreground, move it into the
   * foreground and set a timer to call ResetPriorityNow() in a few seconds.
   */
  void TemporarilySetIsForeground();

  /**
   * Recompute this process's priority and apply it, potentially after a brief
   * delay.
   *
   * If the new priority is FOREGROUND, it takes effect immediately.
   *
   * If the new priority is a BACKGROUND* priority and this process's priority
   * is currently a BACKGROUND* priority, the new priority takes effect
   * immediately.
   *
   * But if the new priority is a BACKGROUND* priority and this process is not
   * currently in the background, we schedule a timer and run
   * ResetPriorityNow() after a short period of time.
   */
  void ResetPriority();

  /**
   * Recompute this process's priority and apply it immediately.
   */
  void ResetPriorityNow();

private:
  void OnContentDocumentGlobalCreated(nsISupports* aOuterWindow);

  /**
   * Compute whether this process is in the foreground and return the result.
   */
  bool ComputeIsInForeground();

  /**
   * Set this process's priority to FOREGROUND immediately.
   */
  void SetIsForeground();

  /**
   * Set this process's priority to the appropriate BACKGROUND* priority
   * immediately.
   */
  void SetIsBackgroundNow();

  /**
   * If mResetPriorityTimer is null (i.e., not running), create a timer and set
   * it to invoke ResetPriorityNow() after
   * dom.ipc.processPriorityManager.aTimeoutPref ms.
   */
  void
  ScheduleResetPriority(const char* aTimeoutPref);

  // mProcessPriority tracks the priority we've given this process in hal.
  ProcessPriority mProcessPriority;

  nsTArray<nsWeakPtr> mWindows;

  // When this timer expires, we set mResetPriorityTimer to null and run
  // ResetPriorityNow().
  nsCOMPtr<nsITimer> mResetPriorityTimer;

  nsWeakPtr mMemoryMinimizerRunnable;
};

NS_IMPL_ISUPPORTS3(ProcessPriorityManager, nsIObserver,
                   nsIDOMEventListener, nsITimerCallback)

ProcessPriorityManager::ProcessPriorityManager()
  : mProcessPriority(ProcessPriority(-1))
{
  // When our parent process forked us, it set our priority either to
  // FOREGROUND (if our parent launched this process to meet an immediate need)
  // or one of the BACKGROUND priorities (if our parent launched this process
  // to meet a future need).
  //
  // We don't know which situation we're in, so we set mProcessPriority to -1
  // so that the next time ResetPriorityNow is run, we'll definitely call into
  // hal and set our priority.
}

void
ProcessPriorityManager::Init()
{
  LOG("Starting up.");

  // We can't do this in the constructor because we need to hold a strong ref
  // to |this| before calling these methods.
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  os->AddObserver(this, "content-document-global-created", /* ownsWeak = */ false);
  os->AddObserver(this, "inner-window-destroyed", /* ownsWeak = */ false);
  os->AddObserver(this, "audio-channel-agent-changed", /* ownsWeak = */ false);
}

NS_IMETHODIMP
ProcessPriorityManager::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const PRUnichar* aData)
{
  if (!strcmp(aTopic, "content-document-global-created")) {
    OnContentDocumentGlobalCreated(aSubject);
  } else if (!strcmp(aTopic, "inner-window-destroyed") ||
             !strcmp(aTopic, "audio-channel-agent-changed")) {
    ResetPriority();
  } else {
    MOZ_ASSERT(false);
  }
  return NS_OK;
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

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(innerWindow);
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

void
ProcessPriorityManager::ResetPriority()
{
  if (ComputeIsInForeground()) {
    SetIsForeground();
  } else if (IsBackgroundPriority(mProcessPriority)) {
    // If we're already in the background, recompute our background priority
    // and set it immediately.
    SetIsBackgroundNow();
  } else {
    ScheduleResetPriority("backgroundGracePeriodMS");
  }
}

void
ProcessPriorityManager::ResetPriorityNow()
{
  if (ComputeIsInForeground()) {
    SetIsForeground();
  } else {
    SetIsBackgroundNow();
  }
}

bool
ProcessPriorityManager::ComputeIsInForeground()
{
  // We could try to be clever and count the number of visible windows, instead
  // of iterating over mWindows every time one window's visibility state changes.
  // But experience suggests that iterating over the windows is prone to fewer
  // errors (and one mistake doesn't mess you up for the entire session).
  // Moreover, mWindows should be a very short list, since it contains only
  // top-level content windows.

  bool allHidden = true;
  for (uint32_t i = 0; i < mWindows.Length(); i++) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(mWindows[i]);
    if (!window) {
      mWindows.RemoveElementAt(i);
      i--;
      continue;
    }

    nsCOMPtr<nsIDOMDocument> doc;
    window->GetDocument(getter_AddRefs(doc));
    if (!doc) {
      continue;
    }

    bool hidden = false;
    doc->GetHidden(&hidden);
#ifdef DEBUG
    nsAutoString spec;
    doc->GetDocumentURI(spec);
    LOG("Document at %s has visibility %d.", NS_ConvertUTF16toUTF8(spec).get(), !hidden);
#endif

    allHidden = allHidden && hidden;

    // We could break out early from this loop if
    //   !hidden && mProcessPriority == BACKGROUND,
    // but then we might not clean up all the weak refs.
  }

  return !allHidden;
}

void
ProcessPriorityManager::SetIsForeground()
{
  if (mProcessPriority == PROCESS_PRIORITY_FOREGROUND) {
    return;
  }

  // Cancel the memory minimization procedure we might have started.
  nsCOMPtr<nsICancelableRunnable> runnable =
    do_QueryReferent(mMemoryMinimizerRunnable);
  if (runnable) {
    runnable->Cancel();
  }

  mProcessPriority = PROCESS_PRIORITY_FOREGROUND;
  LOG("Setting priority to %s.", ProcessPriorityToString(mProcessPriority));
  hal::SetProcessPriority(getpid(), PROCESS_PRIORITY_FOREGROUND);
}

void
ProcessPriorityManager::SetIsBackgroundNow()
{
  ProcessPriority backgroundPriority = GetBackgroundPriority();
  if (mProcessPriority == backgroundPriority) {
    return;
  }

  mProcessPriority = backgroundPriority;
  LOG("Setting priority to %s", ProcessPriorityToString(mProcessPriority));
  hal::SetProcessPriority(getpid(), mProcessPriority);

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

void
ProcessPriorityManager::ScheduleResetPriority(const char* aTimeoutPref)
{
  if (mResetPriorityTimer) {
    // The timer is already running.
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
ProcessPriorityManager::TemporarilySetIsForeground()
{
  LOG("TemporarilySetIsForeground");
  SetIsForeground();

  // Each call to TemporarilySetIsForeground guarantees us temporaryPriorityMS
  // in the foreground.  So cancel our timer if it's running (which is due to a
  // previous call to either TemporarilySetIsForeground() or ResetPriority()).
  if (mResetPriorityTimer) {
    mResetPriorityTimer->Cancel();
    mResetPriorityTimer = nullptr;
  }
  ScheduleResetPriority("temporaryPriorityMS");
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
TemporarilySetProcessPriorityToForeground()
{
  if (sManager) {
    sManager->TemporarilySetIsForeground();
  } else {
    LOG("TemporarilySetProcessPriorityToForeground called before "
        "InitProcessPriorityManager.  Bailing.");
  }
}

} // namespace ipc
} // namespace dom
} // namespace mozilla
