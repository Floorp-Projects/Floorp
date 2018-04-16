/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCCUncollectableMarker.h"
#include "nsIObserverService.h"
#include "nsIDocShell.h"
#include "nsServiceManagerUtils.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "XULDocument.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsISHistory.h"
#include "nsISHEntry.h"
#include "nsISHContainer.h"
#include "nsIWindowWatcher.h"
#include "mozilla/Services.h"
#include "nsIXULWindow.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsJSEnvironment.h"
#include "nsInProcessTabChildGlobal.h"
#include "nsFrameLoader.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ParentProcessMessageManager.h"
#include "mozilla/dom/ProcessGlobal.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TimeoutManager.h"
#include "xpcpublic.h"
#include "nsObserverService.h"
#include "nsFocusManager.h"
#include "nsIInterfaceRequestorUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

static bool sInited = 0;
// The initial value of sGeneration should not be the same as the
// value it is given at xpcom-shutdown, because this will make any GCs
// before we first CC benignly violate the black-gray invariant, due
// to dom::TraceBlackJS().
uint32_t nsCCUncollectableMarker::sGeneration = 1;
#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif

NS_IMPL_ISUPPORTS(nsCCUncollectableMarker, nsIObserver)

/* static */
nsresult
nsCCUncollectableMarker::Init()
{
  if (sInited) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserver> marker = new nsCCUncollectableMarker;

  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;

  nsresult rv;

  // This makes the observer service hold an owning reference to the marker
  rv = obs->AddObserver(marker, "xpcom-shutdown", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obs->AddObserver(marker, "cycle-collector-begin", false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obs->AddObserver(marker, "cycle-collector-forget-skippable", false);
  NS_ENSURE_SUCCESS(rv, rv);

  sInited = true;

  return NS_OK;
}

static void
MarkChildMessageManagers(MessageBroadcaster* aMM)
{
  aMM->MarkForCC();

  uint32_t tabChildCount = aMM->ChildCount();
  for (uint32_t j = 0; j < tabChildCount; ++j) {
    RefPtr<MessageListenerManager> childMM = aMM->GetChildAt(j);
    if (!childMM) {
      continue;
    }

    RefPtr<MessageBroadcaster> strongNonLeafMM = MessageBroadcaster::From(childMM);
    MessageBroadcaster* nonLeafMM = strongNonLeafMM;

    MessageListenerManager* tabMM = childMM;

    strongNonLeafMM = nullptr;
    childMM = nullptr;

    if (nonLeafMM) {
      MarkChildMessageManagers(nonLeafMM);
      continue;
    }

    tabMM->MarkForCC();

    //XXX hack warning, but works, since we know that
    //    callback is frameloader.
    mozilla::dom::ipc::MessageManagerCallback* cb = tabMM->GetCallback();
    if (cb) {
      nsFrameLoader* fl = static_cast<nsFrameLoader*>(cb);
      EventTarget* et = fl->GetTabChildGlobalAsEventTarget();
      if (!et) {
        continue;
      }
      static_cast<nsInProcessTabChildGlobal*>(et)->MarkForCC();
      EventListenerManager* elm = et->GetExistingListenerManager();
      if (elm) {
        elm->MarkForCC();
      }
    }
  }
}

static void
MarkMessageManagers()
{
  if (nsFrameMessageManager::GetChildProcessManager()) {
    // ProcessGlobal's MarkForCC marks also ChildProcessManager.
    ProcessGlobal* pg = ProcessGlobal::Get();
    if (pg) {
      pg->MarkForCC();
    }
  }

  // The global message manager only exists in the root process.
  if (!XRE_IsParentProcess()) {
    return;
  }
  RefPtr<ChromeMessageBroadcaster> strongGlobalMM =
    nsFrameMessageManager::GetGlobalMessageManager();
  if (!strongGlobalMM) {
    return;
  }
  ChromeMessageBroadcaster* globalMM = strongGlobalMM;
  strongGlobalMM = nullptr;
  MarkChildMessageManagers(globalMM);

  if (nsFrameMessageManager::sParentProcessManager) {
    nsFrameMessageManager::sParentProcessManager->MarkForCC();
    uint32_t childCount = nsFrameMessageManager::sParentProcessManager->ChildCount();
    for (uint32_t i = 0; i < childCount; ++i) {
      RefPtr<MessageListenerManager> childMM =
        nsFrameMessageManager::sParentProcessManager->GetChildAt(i);
      if (!childMM) {
        continue;
      }
      MessageListenerManager* child = childMM;
      childMM = nullptr;
      child->MarkForCC();
    }
  }
  if (nsFrameMessageManager::sSameProcessParentManager) {
    nsFrameMessageManager::sSameProcessParentManager->MarkForCC();
  }
}

void
MarkContentViewer(nsIContentViewer* aViewer, bool aCleanupJS)
{
  if (!aViewer) {
    return;
  }

  nsIDocument *doc = aViewer->GetDocument();
  if (doc &&
      doc->GetMarkedCCGeneration() != nsCCUncollectableMarker::sGeneration) {
    doc->MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    if (aCleanupJS) {
      EventListenerManager* elm = doc->GetExistingListenerManager();
      if (elm) {
        elm->MarkForCC();
      }
      nsCOMPtr<EventTarget> win = do_QueryInterface(doc->GetInnerWindow());
      if (win) {
        elm = win->GetExistingListenerManager();
        if (elm) {
          elm->MarkForCC();
        }
        static_cast<nsGlobalWindowInner*>(win.get())->AsInner()->
          TimeoutManager().UnmarkGrayTimers();
      }
    }
  }
  if (doc) {
    if (nsPIDOMWindowInner* inner = doc->GetInnerWindow()) {
      inner->MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    }
    if (nsPIDOMWindowOuter* outer = doc->GetWindow()) {
      outer->MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    }
  }
}

void MarkDocShell(nsIDocShellTreeItem* aNode, bool aCleanupJS);

void
MarkSHEntry(nsISHEntry* aSHEntry, bool aCleanupJS)
{
  if (!aSHEntry) {
    return;
  }

  nsCOMPtr<nsIContentViewer> cview;
  aSHEntry->GetContentViewer(getter_AddRefs(cview));
  MarkContentViewer(cview, aCleanupJS);

  nsCOMPtr<nsIDocShellTreeItem> child;
  int32_t i = 0;
  while (NS_SUCCEEDED(aSHEntry->ChildShellAt(i++, getter_AddRefs(child))) &&
         child) {
    MarkDocShell(child, aCleanupJS);
  }

  nsCOMPtr<nsISHContainer> shCont = do_QueryInterface(aSHEntry);
  int32_t count;
  shCont->GetChildCount(&count);
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> childEntry;
    shCont->GetChildAt(i, getter_AddRefs(childEntry));
    MarkSHEntry(childEntry, aCleanupJS);
  }

}

void
MarkDocShell(nsIDocShellTreeItem* aNode, bool aCleanupJS)
{
  nsCOMPtr<nsIDocShell> shell = do_QueryInterface(aNode);
  if (!shell) {
    return;
  }

  nsCOMPtr<nsIContentViewer> cview;
  shell->GetContentViewer(getter_AddRefs(cview));
  MarkContentViewer(cview, aCleanupJS);

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(shell);
  RefPtr<ChildSHistory> history = webNav->GetSessionHistory();
  if (history) {
    int32_t historyCount = history->Count();
    for (int32_t i = 0; i < historyCount; ++i) {
      nsCOMPtr<nsISHEntry> shEntry;
      history->LegacySHistory()->GetEntryAtIndex(
        i, false, getter_AddRefs(shEntry));

      MarkSHEntry(shEntry, aCleanupJS);
    }
  }

  int32_t i, childCount;
  aNode->GetChildCount(&childCount);
  for (i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aNode->GetChildAt(i, getter_AddRefs(child));
    MarkDocShell(child, aCleanupJS);
  }
}

void
MarkWindowList(nsISimpleEnumerator* aWindowList, bool aCleanupJS)
{
  nsCOMPtr<nsISupports> iter;
  while (NS_SUCCEEDED(aWindowList->GetNext(getter_AddRefs(iter))) &&
         iter) {
    if (nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(iter)) {
      nsCOMPtr<nsIDocShell> rootDocShell = window->GetDocShell();

      MarkDocShell(rootDocShell, aCleanupJS);

      RefPtr<TabChild> tabChild = TabChild::GetFrom(rootDocShell);
      if (tabChild) {
        RefPtr<TabChildGlobal> mm = tabChild->GetMessageManager();
        if (mm) {
          // MarkForCC ends up calling UnmarkGray on message listeners, which
          // TraceBlackJS can't do yet.
          mm->MarkForCC();
        }
      }
    }
  }
}

nsresult
nsCCUncollectableMarker::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    Element::ClearContentUnbinder();

    nsCOMPtr<nsIObserverService> obs =
      mozilla::services::GetObserverService();
    if (!obs)
      return NS_ERROR_FAILURE;

    // No need for kungFuDeathGrip here, yay observerservice!
    obs->RemoveObserver(this, "xpcom-shutdown");
    obs->RemoveObserver(this, "cycle-collector-begin");
    obs->RemoveObserver(this, "cycle-collector-forget-skippable");

    sGeneration = 0;

    return NS_OK;
  }

  NS_ASSERTION(!strcmp(aTopic, "cycle-collector-begin") ||
               !strcmp(aTopic, "cycle-collector-forget-skippable"), "wrong topic");

  // JS cleanup can be slow. Do it only if there has been a GC.
  const bool cleanupJS =
    nsJSContext::CleanupsSinceLastGC() == 0 &&
    !strcmp(aTopic, "cycle-collector-forget-skippable");

  const bool prepareForCC = !strcmp(aTopic, "cycle-collector-begin");
  if (prepareForCC) {
    Element::ClearContentUnbinder();
  }

  // Increase generation to effectively unmark all current objects
  if (!++sGeneration) {
    ++sGeneration;
  }

  nsFocusManager::MarkUncollectableForCCGeneration(sGeneration);

  nsresult rv;

  // Iterate all toplevel windows
  nsCOMPtr<nsISimpleEnumerator> windowList;
  nsCOMPtr<nsIWindowMediator> med =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  if (med) {
    rv = med->GetEnumerator(nullptr, getter_AddRefs(windowList));
    NS_ENSURE_SUCCESS(rv, rv);

    MarkWindowList(windowList, cleanupJS);
  }

  nsCOMPtr<nsIWindowWatcher> ww =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  if (ww) {
    rv = ww->GetWindowEnumerator(getter_AddRefs(windowList));
    NS_ENSURE_SUCCESS(rv, rv);

    MarkWindowList(windowList, cleanupJS);
  }

  nsCOMPtr<nsIAppShellService> appShell =
    do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
  if (appShell) {
    nsCOMPtr<nsIXULWindow> hw;
    appShell->GetHiddenWindow(getter_AddRefs(hw));
    if (hw) {
      nsCOMPtr<nsIDocShell> shell;
      hw->GetDocShell(getter_AddRefs(shell));
      MarkDocShell(shell, cleanupJS);
    }
    bool hasHiddenPrivateWindow = false;
    appShell->GetHasHiddenPrivateWindow(&hasHiddenPrivateWindow);
    if (hasHiddenPrivateWindow) {
      appShell->GetHiddenPrivateWindow(getter_AddRefs(hw));
      if (hw) {
        nsCOMPtr<nsIDocShell> shell;
        hw->GetDocShell(getter_AddRefs(shell));
        MarkDocShell(shell, cleanupJS);
      }
    }
  }

#ifdef MOZ_XUL
  nsXULPrototypeCache* xulCache = nsXULPrototypeCache::GetInstance();
  if (xulCache) {
    xulCache->MarkInCCGeneration(sGeneration);
  }
#endif

  enum ForgetSkippableCleanupState
  {
    eInitial = 0,
    eUnmarkJSEventListeners = 1,
    eUnmarkMessageManagers = 2,
    eUnmarkStrongObservers = 3,
    eUnmarkJSHolders = 4,
    eDone = 5
  };

  static_assert(eDone == NS_MAJOR_FORGET_SKIPPABLE_CALLS,
                "There must be one forgetSkippable call per cleanup state.");

  static uint32_t sFSState = eDone;
  if (prepareForCC) {
    sFSState = eDone;
    return NS_OK;
  }

  if (cleanupJS) {
    // After a GC we start clean up phases from the beginning,
    // but we don't want to do the additional clean up phases here
    // since we have done already plenty of gray unmarking while going through
    // frame message managers and docshells.
    sFSState = eInitial;
    return NS_OK;
  } else {
    ++sFSState;
  }

  switch(sFSState) {
    case eUnmarkJSEventListeners: {
      nsContentUtils::UnmarkGrayJSListenersInCCGenerationDocuments();
      break;
    }
    case eUnmarkMessageManagers: {
      MarkMessageManagers();
      break;
    }
    case eUnmarkStrongObservers: {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      static_cast<nsObserverService *>(obs.get())->UnmarkGrayStrongObservers();
      break;
    }
    case eUnmarkJSHolders: {
      xpc_UnmarkSkippableJSHolders();
      break;
    }
    default: {
      break;
    }
  }

  return NS_OK;
}

void
mozilla::dom::TraceBlackJS(JSTracer* aTrc, bool aIsShutdownGC)
{
#ifdef MOZ_XUL
  // Mark the scripts held in the XULPrototypeCache. This is required to keep
  // the JS script in the cache live across GC.
  nsXULPrototypeCache* cache = nsXULPrototypeCache::MaybeGetInstance();
  if (cache) {
    if (aIsShutdownGC) {
      cache->FlushScripts();
    } else {
      cache->MarkInGC(aTrc);
    }
  }
#endif

  if (!nsCCUncollectableMarker::sGeneration) {
    return;
  }

  if (ProcessGlobal::WasCreated() && nsFrameMessageManager::GetChildProcessManager()) {
    ProcessGlobal* pg = ProcessGlobal::Get();
    if (pg) {
      mozilla::TraceScriptHolder(ToSupports(pg), aTrc);
    }
  }

  // Mark globals of active windows black.
  nsGlobalWindowOuter::OuterWindowByIdTable* windowsById =
    nsGlobalWindowOuter::GetWindowsTable();
  if (windowsById) {
    for (auto iter = windowsById->Iter(); !iter.Done(); iter.Next()) {
      nsGlobalWindowOuter* window = iter.Data();
      if (!window->IsCleanedUp()) {
        nsGlobalWindowInner* inner = nullptr;
        for (PRCList* win = PR_LIST_HEAD(window);
             win != window;
             win = PR_NEXT_LINK(inner)) {
          inner = static_cast<nsGlobalWindowInner*>(win);
          if (inner->IsCurrentInnerWindow() ||
              (inner->GetExtantDoc() &&
               inner->GetExtantDoc()->GetBFCacheEntry())) {
            inner->TraceGlobalJSObject(aTrc);
            EventListenerManager* elm = inner->GetExistingListenerManager();
            if (elm) {
              elm->TraceListeners(aTrc);
            }
          }
        }

        if (window->IsRootOuterWindow()) {
          // In child process trace all the TabChildGlobals.
          // Since there is one root outer window per TabChildGlobal, we need
          // to look for only those windows, not all.
          nsIDocShell* ds = window->GetDocShell();
          if (ds) {
            nsCOMPtr<nsITabChild> tabChild = ds->GetTabChild();
            if (tabChild) {
              nsCOMPtr<nsISupports> mm;
              tabChild->GetMessageManager(getter_AddRefs(mm));
              nsCOMPtr<EventTarget> et = do_QueryInterface(mm);
              if (et) {
                nsCOMPtr<nsISupports> tabChildAsSupports =
                  do_QueryInterface(tabChild);
                mozilla::TraceScriptHolder(tabChildAsSupports, aTrc);
                EventListenerManager* elm = et->GetExistingListenerManager();
                if (elm) {
                  elm->TraceListeners(aTrc);
                }
                // As of now there isn't an easy way to trace message listeners.
              }
            }
          }
        }

#ifdef MOZ_XUL
        nsIDocument* doc = window->GetExtantDoc();
        if (doc && doc->IsXULDocument()) {
          XULDocument* xulDoc = static_cast<XULDocument*>(doc);
          xulDoc->TraceProtos(aTrc);
        }
#endif
      }
    }
  }
}
