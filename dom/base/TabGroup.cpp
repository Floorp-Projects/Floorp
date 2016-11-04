/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabGroup.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsIDocShell.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURI.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<TabGroup> sChromeTabGroup;

TabGroup::TabGroup(bool aIsChrome)
 : mLastWindowLeft(false)
 , mThrottledQueuesInitialized(false)
{
  for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
    TaskCategory category = static_cast<TaskCategory>(i);
    mEventTargets[i] = CreateEventTargetFor(category);
  }

  // Do not throttle runnables from chrome windows.  In theory we should
  // not have abuse issues from these windows and many browser chrome
  // tests have races that fail if we do throttle chrome runnables.
  if (aIsChrome) {
    MOZ_ASSERT(!sChromeTabGroup);
    return;
  }

  // This constructor can be called from the IPC I/O thread. In that case, we
  // won't actually use the TabGroup on the main thread until GetFromWindowActor
  // is called, so we initialize the throttled queues there.
  if (NS_IsMainThread()) {
    EnsureThrottledEventQueues();
  }
}

TabGroup::~TabGroup()
{
  MOZ_ASSERT(mDocGroups.IsEmpty());
  MOZ_ASSERT(mWindows.IsEmpty());
}

void
TabGroup::EnsureThrottledEventQueues()
{
  if (mThrottledQueuesInitialized) {
    return;
  }

  mThrottledQueuesInitialized = true;

  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  MOZ_DIAGNOSTIC_ASSERT(mainThread);

  // This may return nullptr during xpcom shutdown.  This is ok as we
  // do not guarantee a ThrottledEventQueue will be present.
  mThrottledEventQueue = ThrottledEventQueue::Create(mainThread);
}

TabGroup*
TabGroup::GetChromeTabGroup()
{
  if (!sChromeTabGroup) {
    sChromeTabGroup = new TabGroup(true /* chrome tab group */);
    ClearOnShutdown(&sChromeTabGroup);
  }
  return sChromeTabGroup;
}

/* static */ TabGroup*
TabGroup::GetFromWindowActor(mozIDOMWindowProxy* aWindow)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  TabChild* tabChild = TabChild::GetFrom(aWindow);
  if (!tabChild) {
    return nullptr;
  }

  ContentChild* cc = ContentChild::GetSingleton();
  nsCOMPtr<nsIEventTarget> target = cc->GetActorEventTarget(tabChild);
  if (!target) {
    return nullptr;
  }

  // We have an event target. We assume the IPC code created it via
  // TabGroup::CreateEventTarget.
  RefPtr<Dispatcher> dispatcher = Dispatcher::FromEventTarget(target);
  MOZ_RELEASE_ASSERT(dispatcher);
  auto tabGroup = dispatcher->AsTabGroup();
  MOZ_RELEASE_ASSERT(tabGroup);

  // We delay creating the event targets until now since the TabGroup
  // constructor ran off the main thread.
  tabGroup->EnsureThrottledEventQueues();

  return tabGroup;
}

already_AddRefed<DocGroup>
TabGroup::GetDocGroup(const nsACString& aKey)
{
  RefPtr<DocGroup> docGroup(mDocGroups.GetEntry(aKey)->mDocGroup);
  return docGroup.forget();
}

already_AddRefed<DocGroup>
TabGroup::AddDocument(const nsACString& aKey, nsIDocument* aDocument)
{
  HashEntry* entry = mDocGroups.PutEntry(aKey);
  RefPtr<DocGroup> docGroup;
  if (entry->mDocGroup) {
    docGroup = entry->mDocGroup;
  } else {
    docGroup = new DocGroup(this, aKey);
    entry->mDocGroup = docGroup;
  }

  // Make sure that the hashtable was updated and now contains the correct value
  MOZ_ASSERT(RefPtr<DocGroup>(GetDocGroup(aKey)) == docGroup);

  docGroup->mDocuments.AppendElement(aDocument);

  return docGroup.forget();
}

/* static */ already_AddRefed<TabGroup>
TabGroup::Join(nsPIDOMWindowOuter* aWindow, TabGroup* aTabGroup)
{
  RefPtr<TabGroup> tabGroup = aTabGroup;
  if (!tabGroup) {
    tabGroup = new TabGroup();
  }
  MOZ_ASSERT(!tabGroup->mWindows.Contains(aWindow));
  tabGroup->mWindows.AppendElement(aWindow);
  return tabGroup.forget();
}

void
TabGroup::Leave(nsPIDOMWindowOuter* aWindow)
{
  MOZ_ASSERT(mWindows.Contains(aWindow));
  mWindows.RemoveElement(aWindow);

  if (mWindows.IsEmpty()) {
    mLastWindowLeft = true;

    // There is a RefPtr cycle TabGroup -> DispatcherEventTarget -> TabGroup. To
    // avoid leaks, we need to break the chain somewhere. We shouldn't be using
    // the ThrottledEventQueue for this TabGroup when no windows belong to it,
    // so it's safe to null out the queue here.
    for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
      mEventTargets[i] = nullptr;
    }
  }
}

nsresult
TabGroup::FindItemWithName(const nsAString& aName,
                           nsIDocShellTreeItem* aRequestor,
                           nsIDocShellTreeItem* aOriginalRequestor,
                           nsIDocShellTreeItem** aFoundItem)
{
  NS_ENSURE_ARG_POINTER(aFoundItem);
  *aFoundItem = nullptr;

  MOZ_ASSERT(!aName.LowerCaseEqualsLiteral("_blank") &&
             !aName.LowerCaseEqualsLiteral("_top") &&
             !aName.LowerCaseEqualsLiteral("_parent") &&
             !aName.LowerCaseEqualsLiteral("_self"));

  for (nsPIDOMWindowOuter* outerWindow : mWindows) {
    // Ignore non-toplevel windows
    if (outerWindow->GetScriptableParentOrNull()) {
      continue;
    }

    nsCOMPtr<nsIDocShellTreeItem> docshell = outerWindow->GetDocShell();
    if (!docshell) {
      continue;
    }

    nsCOMPtr<nsIDocShellTreeItem> root;
    docshell->GetSameTypeRootTreeItem(getter_AddRefs(root));
    MOZ_RELEASE_ASSERT(docshell == root);
    if (root && aRequestor != root) {
      root->FindItemWithName(aName, this, aOriginalRequestor, aFoundItem);
      if (*aFoundItem) {
        break;
      }
    }
  }

  return NS_OK;
}

nsTArray<nsPIDOMWindowOuter*>
TabGroup::GetTopLevelWindows()
{
  nsTArray<nsPIDOMWindowOuter*> array;

  for (nsPIDOMWindowOuter* outerWindow : mWindows) {
    if (outerWindow->GetDocShell() &&
        !outerWindow->GetScriptableParentOrNull()) {
      array.AppendElement(outerWindow);
    }
  }

  return array;
}

ThrottledEventQueue*
TabGroup::GetThrottledEventQueue() const
{
  MOZ_RELEASE_ASSERT(mThrottledQueuesInitialized || this == GetChromeTabGroup());
  MOZ_RELEASE_ASSERT(!mLastWindowLeft);
  return mThrottledEventQueue;
}

NS_IMPL_ISUPPORTS(TabGroup, nsISupports)

TabGroup::HashEntry::HashEntry(const nsACString* aKey)
  : nsCStringHashKey(aKey), mDocGroup(nullptr)
{}

nsresult
TabGroup::Dispatch(const char* aName,
                   TaskCategory aCategory,
                   already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  if (aName) {
    if (nsCOMPtr<nsINamed> named = do_QueryInterface(runnable)) {
      named->SetName(aName);
    }
  }
  if (NS_IsMainThread()) {
    return NS_DispatchToCurrentThread(runnable.forget());
  } else {
    return NS_DispatchToMainThread(runnable.forget());
  }
}

already_AddRefed<nsIEventTarget>
TabGroup::EventTargetFor(TaskCategory aCategory) const
{
  MOZ_RELEASE_ASSERT(!mLastWindowLeft);
  nsCOMPtr<nsIEventTarget> target = mEventTargets[size_t(aCategory)];
  return target.forget();
}

}
}
