/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "nsIDocShell.h"
#include "nsDOMMutationObserver.h"
#if defined(XP_WIN)
#include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#include <unistd.h> // for getpid()
#endif // defined(XP_WIN)

namespace mozilla {
namespace dom {

AutoTArray<RefPtr<DocGroup>, 2>* DocGroup::sPendingDocGroups = nullptr;

/* static */ nsresult
DocGroup::GetKey(nsIPrincipal* aPrincipal, nsACString& aKey)
{
  // Use GetBaseDomain() to handle things like file URIs, IP address URIs,
  // etc. correctly.
  nsresult rv = aPrincipal->GetBaseDomain(aKey);
  if (NS_FAILED(rv)) {
    // We don't really know what to do here.  But we should be conservative,
    // otherwise it would be possible to reorder two events incorrectly in the
    // future if we interrupt at the DocGroup level, so to be safe, use an
    // empty string to classify all such documents as belonging to the same
    // DocGroup.
    aKey.Truncate();
  }

  return rv;
}

void
DocGroup::RemoveDocument(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);
}

DocGroup::DocGroup(TabGroup* aTabGroup, const nsACString& aKey)
  : mKey(aKey), mTabGroup(aTabGroup)
{
  // This method does not add itself to mTabGroup->mDocGroups as the caller does it for us.
  if (mozilla::StaticPrefs::dom_performance_enable_scheduler_timing()) {
    mPerformanceCounter = new mozilla::PerformanceCounter(NS_LITERAL_CSTRING("DocGroup:") + aKey);
  }
}

DocGroup::~DocGroup()
{
  MOZ_ASSERT(mDocuments.IsEmpty());
  if (!NS_IsMainThread()) {
    nsIEventTarget* target = EventTargetFor(TaskCategory::Other);
    NS_ProxyRelease("DocGroup::mReactionsStack", target, mReactionsStack.forget());
  }

  mTabGroup->mDocGroups.RemoveEntry(mKey);
}

PerformanceInfo
DocGroup::ReportPerformanceInfo()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mPerformanceCounter);
#if defined(XP_WIN)
  uint32_t pid = GetCurrentProcessId();
#else
  uint32_t pid = getpid();
#endif
  uint64_t pwid = 0;
  uint16_t count = 0;
  uint64_t duration = 0;
  bool isTopLevel = false;
  nsCString host;

  // iterating on documents until we find the top window
  for (const auto& document : *this) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);
    MOZ_ASSERT(doc);
    // looking for the top level document URI
    nsPIDOMWindowInner* win = doc->GetInnerWindow();
    if (!win) {
      continue;
    }
    nsPIDOMWindowOuter* outer = win->GetOuterWindow();
    if (!outer) {
      continue;
    }
    nsCOMPtr<nsPIDOMWindowOuter> top = outer->GetTop();
    if (!top) {
      continue;
    }
    nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
    if (!docURI) {
      continue;
    }
    pwid = top->WindowID();
    isTopLevel = top->IsTopLevelWindow();;
    docURI->GetHost(host);
    // If the host is empty, using the url
    if (host.IsEmpty()) {
      docURI->GetSpec(host);
    }
    break;
  }

  duration = mPerformanceCounter->GetExecutionDuration();
  FallibleTArray<CategoryDispatch> items;

  // now that we have the host and window ids, let's look at the perf counters
  for (uint32_t index = 0; index < (uint32_t)TaskCategory::Count; index++) {
    TaskCategory category = static_cast<TaskCategory>(index);
    count = mPerformanceCounter->GetDispatchCount(DispatchCategory(category));
    CategoryDispatch item = CategoryDispatch(index, count);
    if (!items.AppendElement(item, fallible)) {
      NS_ERROR("Could not complete the operation");
      return PerformanceInfo(host, pid, pwid, duration, false, isTopLevel, items);
    }
  }

  return PerformanceInfo(host, pid, pwid, duration, false, isTopLevel, items);
}

nsresult
DocGroup::Dispatch(TaskCategory aCategory,
                   already_AddRefed<nsIRunnable>&& aRunnable)
{
  if (mPerformanceCounter) {
    mPerformanceCounter->IncrementDispatchCounter(DispatchCategory(aCategory));
  }
  return mTabGroup->DispatchWithDocGroup(aCategory, std::move(aRunnable), this);
}

nsISerialEventTarget*
DocGroup::EventTargetFor(TaskCategory aCategory) const
{
  return mTabGroup->EventTargetFor(aCategory);
}

AbstractThread*
DocGroup::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return mTabGroup->AbstractMainThreadFor(aCategory);
}

bool*
DocGroup::GetValidAccessPtr()
{
  return mTabGroup->GetValidAccessPtr();
}

void
DocGroup::SignalSlotChange(HTMLSlotElement& aSlot)
{
  MOZ_ASSERT(!mSignalSlotList.Contains(&aSlot));
  mSignalSlotList.AppendElement(&aSlot);

  if (!sPendingDocGroups) {
    // Queue a mutation observer compound microtask.
    nsDOMMutationObserver::QueueMutationObserverMicroTask();
    sPendingDocGroups = new AutoTArray<RefPtr<DocGroup>, 2>;
  }

  sPendingDocGroups->AppendElement(this);
}

void
DocGroup::MoveSignalSlotListTo(nsTArray<RefPtr<HTMLSlotElement>>& aDest)
{
  aDest.SetCapacity(aDest.Length() + mSignalSlotList.Length());
  for (RefPtr<HTMLSlotElement>& slot : mSignalSlotList) {
    slot->RemovedFromSignalSlotList();
    aDest.AppendElement(std::move(slot));
  }
  mSignalSlotList.Clear();
}

bool
DocGroup::IsActive() const
{
  for (nsIDocument* doc : mDocuments) {
    if (doc->IsCurrentActiveDocument()) {
      return true;
    }
  }

  return false;
}

}
}
