/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsEventListenerManager.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsJSEnvironment.h"
#include "nsInProcessTabChildGlobal.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/Element.h"
#include "xpcpublic.h"
#include "nsObserverService.h"

using namespace mozilla::dom;

static bool sInited = 0;
uint32_t nsCCUncollectableMarker::sGeneration = 0;
#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif

NS_IMPL_ISUPPORTS1(nsCCUncollectableMarker, nsIObserver)

/* static */
nsresult
nsCCUncollectableMarker::Init()
{
  if (sInited) {
    return NS_OK;
  }
  
  nsCOMPtr<nsIObserver> marker = new nsCCUncollectableMarker;
  NS_ENSURE_TRUE(marker, NS_ERROR_OUT_OF_MEMORY);

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
MarkUserData(void* aNode, nsIAtom* aKey, void* aValue, void* aData)
{
  nsIDocument* d = static_cast<nsINode*>(aNode)->GetCurrentDoc();
  if (d && nsCCUncollectableMarker::InGeneration(d->GetMarkedCCGeneration())) {
    Element::MarkUserData(aNode, aKey, aValue, aData);
  }
}

static void
MarkUserDataHandler(void* aNode, nsIAtom* aKey, void* aValue, void* aData)
{
  nsIDocument* d = static_cast<nsINode*>(aNode)->GetCurrentDoc();
  if (d && nsCCUncollectableMarker::InGeneration(d->GetMarkedCCGeneration())) {
    Element::MarkUserDataHandler(aNode, aKey, aValue, aData);
  }
}

static void
MarkMessageManagers()
{
  nsCOMPtr<nsIMessageBroadcaster> strongGlobalMM =
    do_GetService("@mozilla.org/globalmessagemanager;1");
  if (!strongGlobalMM) {
    return;
  }
  nsIMessageBroadcaster* globalMM = strongGlobalMM;
  strongGlobalMM = nullptr;

  globalMM->MarkForCC();
  uint32_t childCount = 0;
  globalMM->GetChildCount(&childCount);
  for (uint32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIMessageListenerManager> childMM;
    globalMM->GetChildAt(i, getter_AddRefs(childMM));
    if (!childMM) {
      continue;
    }
    nsCOMPtr<nsIMessageBroadcaster> strongWindowMM = do_QueryInterface(childMM);
    nsIMessageBroadcaster* windowMM = strongWindowMM;
    childMM = nullptr;
    strongWindowMM = nullptr;
    windowMM->MarkForCC();
    uint32_t tabChildCount = 0;
    windowMM->GetChildCount(&tabChildCount);
    for (uint32_t j = 0; j < tabChildCount; ++j) {
      nsCOMPtr<nsIMessageListenerManager> childMM;
      windowMM->GetChildAt(j, getter_AddRefs(childMM));
      if (!childMM) {
        continue;
      }
      nsCOMPtr<nsIMessageSender> strongTabMM = do_QueryInterface(childMM);
      nsIMessageSender* tabMM = strongTabMM;
      childMM = nullptr;
      strongTabMM = nullptr;
      tabMM->MarkForCC();
      //XXX hack warning, but works, since we know that
      //    callback is frameloader.
      mozilla::dom::ipc::MessageManagerCallback* cb =
        static_cast<nsFrameMessageManager*>(tabMM)->GetCallback();
      if (cb) {
        nsFrameLoader* fl = static_cast<nsFrameLoader*>(cb);
        nsIDOMEventTarget* et = fl->GetTabChildGlobalAsEventTarget();
        if (!et) {
          continue;
        }
        static_cast<nsInProcessTabChildGlobal*>(et)->MarkForCC();
        nsEventListenerManager* elm = et->GetListenerManager(false);
        if (elm) {
          elm->MarkForCC();
        }
      }
    }
  }
  if (nsFrameMessageManager::sParentProcessManager) {
    nsFrameMessageManager::sParentProcessManager->MarkForCC();
    uint32_t childCount = 0;
    nsFrameMessageManager::sParentProcessManager->GetChildCount(&childCount);
    for (uint32_t i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIMessageListenerManager> childMM;
      nsFrameMessageManager::sParentProcessManager->
        GetChildAt(i, getter_AddRefs(childMM));
      if (!childMM) {
        continue;
      }
      nsIMessageListenerManager* child = childMM;
      childMM = nullptr;
      child->MarkForCC();
    }
  }
  if (nsFrameMessageManager::sSameProcessParentManager) {
    nsFrameMessageManager::sSameProcessParentManager->MarkForCC();
  }
  if (nsFrameMessageManager::sChildProcessManager) {
    nsFrameMessageManager::sChildProcessManager->MarkForCC();
  }
}

void
MarkContentViewer(nsIContentViewer* aViewer, bool aCleanupJS,
                  bool aPrepareForCC)
{
  if (!aViewer) {
    return;
  }

  nsIDocument *doc = aViewer->GetDocument();
  if (doc &&
      doc->GetMarkedCCGeneration() != nsCCUncollectableMarker::sGeneration) {
    doc->MarkUncollectableForCCGeneration(nsCCUncollectableMarker::sGeneration);
    if (aCleanupJS) {
      nsEventListenerManager* elm = doc->GetListenerManager(false);
      if (elm) {
        elm->MarkForCC();
      }
      nsCOMPtr<nsIDOMEventTarget> win = do_QueryInterface(doc->GetInnerWindow());
      if (win) {
        elm = win->GetListenerManager(false);
        if (elm) {
          elm->MarkForCC();
        }
        static_cast<nsGlobalWindow*>(win.get())->UnmarkGrayTimers();
      }

      doc->PropertyTable(DOM_USER_DATA_HANDLER)->
        EnumerateAll(MarkUserDataHandler, &nsCCUncollectableMarker::sGeneration);
    } else if (aPrepareForCC) {
      // Unfortunately we need to still mark user data just before running CC so
      // that it has the right generation. 
      doc->PropertyTable(DOM_USER_DATA)->
        EnumerateAll(MarkUserData, &nsCCUncollectableMarker::sGeneration);
    }
  }
}

void MarkDocShell(nsIDocShellTreeNode* aNode, bool aCleanupJS,
                  bool aPrepareForCC);

void
MarkSHEntry(nsISHEntry* aSHEntry, bool aCleanupJS, bool aPrepareForCC)
{
  if (!aSHEntry) {
    return;
  }

  nsCOMPtr<nsIContentViewer> cview;
  aSHEntry->GetContentViewer(getter_AddRefs(cview));
  MarkContentViewer(cview, aCleanupJS, aPrepareForCC);

  nsCOMPtr<nsIDocShellTreeItem> child;
  int32_t i = 0;
  while (NS_SUCCEEDED(aSHEntry->ChildShellAt(i++, getter_AddRefs(child))) &&
         child) {
    MarkDocShell(child, aCleanupJS, aPrepareForCC);
  }

  nsCOMPtr<nsISHContainer> shCont = do_QueryInterface(aSHEntry);
  int32_t count;
  shCont->GetChildCount(&count);
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> childEntry;
    shCont->GetChildAt(i, getter_AddRefs(childEntry));
    MarkSHEntry(childEntry, aCleanupJS, aPrepareForCC);
  }
  
}

void
MarkDocShell(nsIDocShellTreeNode* aNode, bool aCleanupJS, bool aPrepareForCC)
{
  nsCOMPtr<nsIDocShell> shell = do_QueryInterface(aNode);
  if (!shell) {
    return;
  }

  nsCOMPtr<nsIContentViewer> cview;
  shell->GetContentViewer(getter_AddRefs(cview));
  MarkContentViewer(cview, aCleanupJS, aPrepareForCC);

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(shell);
  nsCOMPtr<nsISHistory> history;
  webNav->GetSessionHistory(getter_AddRefs(history));
  if (history) {
    int32_t i, historyCount;
    history->GetCount(&historyCount);
    for (i = 0; i < historyCount; ++i) {
      nsCOMPtr<nsIHistoryEntry> historyEntry;
      history->GetEntryAtIndex(i, false, getter_AddRefs(historyEntry));
      nsCOMPtr<nsISHEntry> shEntry = do_QueryInterface(historyEntry);

      MarkSHEntry(shEntry, aCleanupJS, aPrepareForCC);
    }
  }

  int32_t i, childCount;
  aNode->GetChildCount(&childCount);
  for (i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aNode->GetChildAt(i, getter_AddRefs(child));
    MarkDocShell(child, aCleanupJS, aPrepareForCC);
  }
}

void
MarkWindowList(nsISimpleEnumerator* aWindowList, bool aCleanupJS,
               bool aPrepareForCC)
{
  nsCOMPtr<nsISupports> iter;
  while (NS_SUCCEEDED(aWindowList->GetNext(getter_AddRefs(iter))) &&
         iter) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(iter);
    if (window) {
      nsCOMPtr<nsIDocShellTreeNode> rootDocShell =
        do_QueryInterface(window->GetDocShell());

      MarkDocShell(rootDocShell, aCleanupJS, aPrepareForCC);
    }
  }
}

nsresult
nsCCUncollectableMarker::Observe(nsISupports* aSubject, const char* aTopic,
                                 const PRUnichar* aData)
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
  bool cleanupJS =
    nsJSContext::CleanupsSinceLastGC() == 0 &&
    !strcmp(aTopic, "cycle-collector-forget-skippable");

  bool prepareForCC = !strcmp(aTopic, "cycle-collector-begin");
  if (prepareForCC) {
    Element::ClearContentUnbinder();
  }

  // Increase generation to effectively unmark all current objects
  if (!++sGeneration) {
    ++sGeneration;
  }

  nsresult rv;

  // Iterate all toplevel windows
  nsCOMPtr<nsISimpleEnumerator> windowList;
  nsCOMPtr<nsIWindowMediator> med =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  if (med) {
    rv = med->GetEnumerator(nullptr, getter_AddRefs(windowList));
    NS_ENSURE_SUCCESS(rv, rv);

    MarkWindowList(windowList, cleanupJS, prepareForCC);
  }

  nsCOMPtr<nsIWindowWatcher> ww =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  if (ww) {
    rv = ww->GetWindowEnumerator(getter_AddRefs(windowList));
    NS_ENSURE_SUCCESS(rv, rv);

    MarkWindowList(windowList, cleanupJS, prepareForCC);
  }

  nsCOMPtr<nsIAppShellService> appShell = 
    do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
  if (appShell) {
    nsCOMPtr<nsIXULWindow> hw;
    appShell->GetHiddenWindow(getter_AddRefs(hw));
    if (hw) {
      nsCOMPtr<nsIDocShell> shell;
      hw->GetDocShell(getter_AddRefs(shell));
      nsCOMPtr<nsIDocShellTreeNode> shellTreeNode = do_QueryInterface(shell);
      MarkDocShell(shellTreeNode, cleanupJS, prepareForCC);
    }
    bool hasHiddenPrivateWindow = false;
    appShell->GetHasHiddenPrivateWindow(&hasHiddenPrivateWindow);
    if (hasHiddenPrivateWindow) {
      appShell->GetHiddenPrivateWindow(getter_AddRefs(hw));
      if (hw) {
        nsCOMPtr<nsIDocShell> shell;
        hw->GetDocShell(getter_AddRefs(shell));
        nsCOMPtr<nsIDocShellTreeNode> shellTreeNode = do_QueryInterface(shell);
        MarkDocShell(shellTreeNode, cleanupJS, prepareForCC);
      }
    }
  }

#ifdef MOZ_XUL
  nsXULPrototypeCache* xulCache = nsXULPrototypeCache::GetInstance();
  if (xulCache) {
    xulCache->MarkInCCGeneration(sGeneration);
  }
#endif

  static bool previousWasJSCleanup = false;
  if (cleanupJS) {
    nsContentUtils::UnmarkGrayJSListenersInCCGenerationDocuments(sGeneration);
    MarkMessageManagers();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    static_cast<nsObserverService *>(obs.get())->UnmarkGrayStrongObservers();

    previousWasJSCleanup = true;
  } else if (previousWasJSCleanup) {
    previousWasJSCleanup = false;
    if (!prepareForCC) {
      xpc_UnmarkSkippableJSHolders();
    }
  }

  return NS_OK;
}

struct TraceClosure
{
  TraceClosure(JSTracer* aTrc, uint32_t aGCNumber)
    : mTrc(aTrc), mGCNumber(aGCNumber)
  {}
  JSTracer* mTrc;
  uint32_t mGCNumber;
};

static PLDHashOperator
TraceActiveWindowGlobal(const uint64_t& aId, nsGlobalWindow*& aWindow, void* aClosure)
{
  if (aWindow->GetDocShell() && aWindow->IsOuterWindow()) {
    TraceClosure* closure = static_cast<TraceClosure*>(aClosure);
    if (JSObject* global = aWindow->FastGetGlobalJSObject()) {
      JS_CallObjectTracer(closure->mTrc, global, "active window global");
    }
#ifdef MOZ_XUL
    nsIDocument* doc = aWindow->GetExtantDoc();
    if (doc && doc->IsXUL()) {
      XULDocument* xulDoc = static_cast<XULDocument*>(doc);
      xulDoc->TraceProtos(closure->mTrc, closure->mGCNumber);
    }
#endif
  }
  return PL_DHASH_NEXT;
}

void
mozilla::dom::TraceBlackJS(JSTracer* aTrc, uint32_t aGCNumber, bool aIsShutdownGC)
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

  TraceClosure closure(aTrc, aGCNumber);

  // Mark globals of active windows black.
  nsGlobalWindow::WindowByIdTable* windowsById =
    nsGlobalWindow::GetWindowsTable();
  if (windowsById) {
    windowsById->Enumerate(TraceActiveWindowGlobal, &closure);
  }

  // Mark the safe context black
  nsContentUtils::TraceSafeJSContext(aTrc);
}
