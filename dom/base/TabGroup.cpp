/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabGroup.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/AbstractThread.h"
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
 , mNumOfIndexedDBTransactions(0)
 , mNumOfIndexedDBDatabases(0)
 , mIsChrome(aIsChrome)
 , mForegroundCount(0)
{
  CreateEventTargets(/* aNeedValidation = */ !aIsChrome);

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
  MOZ_RELEASE_ASSERT(mLastWindowLeft || mIsChrome);
}

void
TabGroup::EnsureThrottledEventQueues()
{
  if (mThrottledQueuesInitialized) {
    return;
  }

  mThrottledQueuesInitialized = true;

  for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
    TaskCategory category = static_cast<TaskCategory>(i);
    if (category == TaskCategory::Worker || category == TaskCategory::Timer) {
      nsCOMPtr<nsISerialEventTarget> target = ThrottledEventQueue::Create(mEventTargets[i]);
      if (target) {
        // This may return nullptr during xpcom shutdown.  This is ok as we
        // do not guarantee a ThrottledEventQueue will be present.
        mEventTargets[i] = target;
      }
    }
  }
}

/* static */ TabGroup*
TabGroup::GetChromeTabGroup()
{
  if (!sChromeTabGroup) {
    sChromeTabGroup = new TabGroup(true /* chrome tab group */);
    ClearOnShutdown(&sChromeTabGroup);
  }
  return sChromeTabGroup;
}

/* static */ TabGroup*
TabGroup::GetFromWindow(mozIDOMWindowProxy* aWindow)
{
  if (TabChild* tabChild = TabChild::GetFrom(aWindow)) {
    return tabChild->TabGroup();
  }

  return nullptr;
}

/* static */ TabGroup*
TabGroup::GetFromActor(TabChild* aTabChild)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIEventTarget> target = aTabChild->Manager()->GetEventTargetFor(aTabChild);
  if (!target) {
    return nullptr;
  }

  // We have an event target. We assume the IPC code created it via
  // TabGroup::CreateEventTarget.
  RefPtr<SchedulerGroup> group =
    SchedulerGroup::FromEventTarget(target);
  MOZ_RELEASE_ASSERT(group);
  auto tabGroup = group->AsTabGroup();
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
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<TabGroup> tabGroup = aTabGroup;
  if (!tabGroup) {
    tabGroup = new TabGroup();
  }
  MOZ_RELEASE_ASSERT(!tabGroup->mLastWindowLeft);
  MOZ_ASSERT(!tabGroup->mWindows.Contains(aWindow));
  tabGroup->mWindows.AppendElement(aWindow);

  if (!aWindow->IsBackground()) {
    tabGroup->mForegroundCount++;
  }

  return tabGroup.forget();
}

void
TabGroup::Leave(nsPIDOMWindowOuter* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWindows.Contains(aWindow));
  mWindows.RemoveElement(aWindow);

  if (!aWindow->IsBackground()) {
    MOZ_DIAGNOSTIC_ASSERT(mForegroundCount > 0);
    mForegroundCount--;
  }

  // The Chrome TabGroup doesn't have cyclical references through mEventTargets
  // to itself, meaning that we don't have to worry about nulling mEventTargets
  // out after the last window leaves.
  if (!mIsChrome && mWindows.IsEmpty()) {
    mLastWindowLeft = true;
    Shutdown(false);
  }
}

nsresult
TabGroup::FindItemWithName(const nsAString& aName,
                           nsIDocShellTreeItem* aRequestor,
                           nsIDocShellTreeItem* aOriginalRequestor,
                           nsIDocShellTreeItem** aFoundItem)
{
  MOZ_ASSERT(NS_IsMainThread());
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
      root->FindItemWithName(aName, aRequestor, aOriginalRequestor,
                             /* aSkipTabGroup = */ true, aFoundItem);
      if (*aFoundItem) {
        break;
      }
    }
  }

  return NS_OK;
}

nsTArray<nsPIDOMWindowOuter*>
TabGroup::GetTopLevelWindows() const
{
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<nsPIDOMWindowOuter*> array;

  for (nsPIDOMWindowOuter* outerWindow : mWindows) {
    if (outerWindow->GetDocShell() &&
        !outerWindow->GetScriptableParentOrNull()) {
      array.AppendElement(outerWindow);
    }
  }

  return array;
}

TabGroup::HashEntry::HashEntry(const nsACString* aKey)
  : nsCStringHashKey(aKey), mDocGroup(nullptr)
{}

nsISerialEventTarget*
TabGroup::EventTargetFor(TaskCategory aCategory) const
{
  if (aCategory == TaskCategory::Worker || aCategory == TaskCategory::Timer) {
    MOZ_RELEASE_ASSERT(mThrottledQueuesInitialized || mIsChrome);
  }
  return SchedulerGroup::EventTargetFor(aCategory);
}

AbstractThread*
TabGroup::AbstractMainThreadForImpl(TaskCategory aCategory)
{
  // The mEventTargets of the chrome TabGroup are all set to do_GetMainThread().
  // We could just return AbstractThread::MainThread() without a wrapper.
  // Once we've disconnected everything, we still allow people to dispatch.
  // We'll just go directly to the main thread.
  if (this == sChromeTabGroup || NS_WARN_IF(mLastWindowLeft)) {
    return AbstractThread::MainThread();
  }

  return SchedulerGroup::AbstractMainThreadForImpl(aCategory);
}

void
TabGroup::WindowChangedBackgroundStatus(bool aIsNowBackground)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aIsNowBackground) {
    MOZ_DIAGNOSTIC_ASSERT(mForegroundCount > 0);
    mForegroundCount -= 1;
  } else {
    mForegroundCount += 1;
  }
}

bool
TabGroup::IsBackground() const
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  uint32_t foregrounded = 0;
  for (auto& window : mWindows) {
    if (!window->IsBackground()) {
      foregrounded++;
    }
  }
  MOZ_ASSERT(foregrounded == mForegroundCount);
#endif

  return mForegroundCount == 0;
}

} // namespace dom
} // namespace mozilla
