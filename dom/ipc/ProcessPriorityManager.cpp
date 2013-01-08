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
// in your environment.

#ifdef PR_LOGGING
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
  if (service->ContentChannelIsActive()) {
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
{
public:
  ProcessPriorityManager();
  void Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  ProcessPriority GetPriority() const { return mProcessPriority; }

private:
  void SetPriority(ProcessPriority aPriority);
  void OnAudioChannelAgentChanged();
  void OnContentDocumentGlobalCreated(nsISupports* aOuterWindow);
  void OnInnerWindowDestroyed();
  void OnGracePeriodTimerFired();
  void RecomputeNumVisibleWindows();

  // mProcessPriority tracks the priority we've given this process in hal,
  // except that, when the grace period timer is active, mProcessPriority ==
  // BACKGROUND or HOMESCREEN_BACKGROUND even though hal still thinks we're a
  // foreground process.
  ProcessPriority mProcessPriority;

  nsTArray<nsWeakPtr> mWindows;
  nsCOMPtr<nsITimer> mGracePeriodTimer;
  nsWeakPtr mMemoryMinimizerRunnable;
  TimeStamp mStartupTime;
};

NS_IMPL_ISUPPORTS2(ProcessPriorityManager, nsIObserver, nsIDOMEventListener);

ProcessPriorityManager::ProcessPriorityManager()
  : mProcessPriority(PROCESS_PRIORITY_FOREGROUND)
  , mStartupTime(TimeStamp::Now())
{
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

  SetPriority(PROCESS_PRIORITY_FOREGROUND);
}

NS_IMETHODIMP
ProcessPriorityManager::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const PRUnichar* aData)
{
  if (!strcmp(aTopic, "content-document-global-created")) {
    OnContentDocumentGlobalCreated(aSubject);
  } else if (!strcmp(aTopic, "inner-window-destroyed")) {
    OnInnerWindowDestroyed();
  } else if (!strcmp(aTopic, "timer-callback")) {
    OnGracePeriodTimerFired();
  } else if (!strcmp(aTopic, "audio-channel-agent-changed")) {
    OnAudioChannelAgentChanged();
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
  RecomputeNumVisibleWindows();
  return NS_OK;
}

void
ProcessPriorityManager::OnAudioChannelAgentChanged()
{
  if (IsBackgroundPriority(mProcessPriority)) {
    SetPriority(GetBackgroundPriority());
  }
}

void
ProcessPriorityManager::OnContentDocumentGlobalCreated(
  nsISupports* aOuterWindow)
{
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
  RecomputeNumVisibleWindows();
}

void
ProcessPriorityManager::OnInnerWindowDestroyed()
{
  RecomputeNumVisibleWindows();
}

void
ProcessPriorityManager::RecomputeNumVisibleWindows()
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

  SetPriority(allHidden ?
              GetBackgroundPriority() :
              PROCESS_PRIORITY_FOREGROUND);
}

void
ProcessPriorityManager::SetPriority(ProcessPriority aPriority)
{
  if (aPriority == mProcessPriority) {
    return;
  }

  if (IsBackgroundPriority(aPriority)) {
    // If this is a foreground --> background transition, give ourselves a
    // grace period before informing hal.
    uint32_t gracePeriodMS = Preferences::GetUint("dom.ipc.processPriorityManager.gracePeriodMS", 1000);
    if (mGracePeriodTimer) {
      LOG("Grace period timer already active.");
      return;
    }

    LOG("Initializing grace period timer.");
    mProcessPriority = aPriority;
    mGracePeriodTimer = do_CreateInstance("@mozilla.org/timer;1");
    mGracePeriodTimer->Init(this, gracePeriodMS, nsITimer::TYPE_ONE_SHOT);

  } else if (aPriority == PROCESS_PRIORITY_FOREGROUND) {
    // If this is a background --> foreground transition, do it immediately, and
    // cancel the outstanding grace period timer, if there is one.
    if (mGracePeriodTimer) {
      mGracePeriodTimer->Cancel();
      mGracePeriodTimer = nullptr;
    }

    // Cancel the memory minimization procedure we might have started.
    nsCOMPtr<nsICancelableRunnable> runnable =
      do_QueryReferent(mMemoryMinimizerRunnable);
    if (runnable) {
      runnable->Cancel();
    }

    LOG("Setting priority to %d.", aPriority);
    mProcessPriority = aPriority;
    hal::SetProcessPriority(getpid(), aPriority);

  } else {
    MOZ_ASSERT(false);
  }
}

void
ProcessPriorityManager::OnGracePeriodTimerFired()
{
  LOG("Grace period timer fired; setting priority to %d.",
      mProcessPriority);

  // mProcessPriority should already be one of the BACKGROUND values: We set it
  // in SetPriority(BACKGROUND), and we canceled this timer if there was an
  // intervening SetPriority(FOREGROUND) call.
  MOZ_ASSERT(IsBackgroundPriority(mProcessPriority));

  mGracePeriodTimer = nullptr;
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

} // namespace ipc
} // namespace dom
} // namespace mozilla
