/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseHistory.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {

using mozilla::dom::ContentParent;
using mozilla::dom::Link;

BaseHistory::BaseHistory() : mTrackedURIs(kTrackedUrisInitialSize) {}

BaseHistory::~BaseHistory() = default;

static constexpr nsLiteralCString kDisallowedSchemes[] = {
    "about"_ns,         "blob"_ns,       "data"_ns,     "chrome"_ns,
    "imap"_ns,          "javascript"_ns, "mailbox"_ns,  "moz-anno"_ns,
    "news"_ns,          "page-icon"_ns,  "resource"_ns, "view-source"_ns,
    "moz-extension"_ns,
};

bool BaseHistory::CanStore(nsIURI* aURI) {
  nsAutoCString scheme;
  if (NS_WARN_IF(NS_FAILED(aURI->GetScheme(scheme)))) {
    return false;
  }

  if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https")) {
    for (const nsLiteralCString& disallowed : kDisallowedSchemes) {
      if (scheme.Equals(disallowed)) {
        return false;
      }
    }
  }

  nsAutoCString spec;
  aURI->GetSpec(spec);
  return spec.Length() <= StaticPrefs::browser_history_maxUrlLength();
}

void BaseHistory::ScheduleVisitedQuery(nsIURI* aURI,
                                       dom::ContentParent* aForProcess) {
  mPendingQueries.WithEntryHandle(aURI, [&](auto&& entry) {
    auto& set = entry.OrInsertWith([] { return ContentParentSet(); });
    if (aForProcess) {
      set.Insert(aForProcess);
    }
  });
  if (mStartPendingVisitedQueriesScheduled) {
    return;
  }
  mStartPendingVisitedQueriesScheduled =
      NS_SUCCEEDED(NS_DispatchToMainThreadQueue(
          NS_NewRunnableFunction(
              "BaseHistory::StartPendingVisitedQueries",
              [self = RefPtr<BaseHistory>(this)] {
                self->mStartPendingVisitedQueriesScheduled = false;
                auto queries = std::move(self->mPendingQueries);
                self->StartPendingVisitedQueries(std::move(queries));
                MOZ_DIAGNOSTIC_ASSERT(self->mPendingQueries.IsEmpty());
              }),
          EventQueuePriority::Idle));
}

void BaseHistory::CancelVisitedQueryIfPossible(nsIURI* aURI) {
  mPendingQueries.Remove(aURI);
  // TODO(bug 1591393): It could be worth to make this virtual and allow places
  // to stop the existing database query? Needs some measurement.
}

void BaseHistory::RegisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI, "Must pass a non-null URI!");
  MOZ_ASSERT(aLink, "Must pass a non-null Link!");

  if (!CanStore(aURI)) {
    aLink->VisitedQueryFinished(/* visited = */ false);
    return;
  }

  // Obtain our array of observers for this URI.
  auto* const links =
      mTrackedURIs.WithEntryHandle(aURI, [&](auto&& entry) -> ObservingLinks* {
        MOZ_DIAGNOSTIC_ASSERT(!entry || !entry->mLinks.IsEmpty(),
                              "An empty key was kept around in our hashtable!");
        if (!entry) {
          ScheduleVisitedQuery(aURI, nullptr);
        }

        return &entry.OrInsertWith([] { return ObservingLinks{}; });
      });

  if (!links) {
    return;
  }

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  MOZ_DIAGNOSTIC_ASSERT(!links->mLinks.Contains(aLink),
                        "Already tracking this Link object!");
  // FIXME(emilio): We should consider changing this (see the entry.Remove()
  // call in NotifyVisitedInThisProcess).
  MOZ_DIAGNOSTIC_ASSERT(links->mStatus != VisitedStatus::Visited,
                        "We don't keep tracking known-visited links");

  links->mLinks.AppendElement(aLink);

  // If this link has already been queried and we should notify, do so now.
  switch (links->mStatus) {
    case VisitedStatus::Unknown:
      break;
    case VisitedStatus::Unvisited:
      if (!StaticPrefs::layout_css_notify_of_unvisited()) {
        break;
      }
      [[fallthrough]];
    case VisitedStatus::Visited:
      aLink->VisitedQueryFinished(links->mStatus == VisitedStatus::Visited);
      break;
  }
}

void BaseHistory::UnregisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI, "Must pass a non-null URI!");
  MOZ_ASSERT(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    MOZ_ASSERT(!CanStore(aURI),
               "Trying to unregister URI that wasn't registered, "
               "and that could be visited!");
    return;
  }

  ObserverArray& observers = entry->mLinks;
  if (!observers.RemoveElement(aLink)) {
    MOZ_ASSERT_UNREACHABLE("Trying to unregister node that wasn't registered!");
    return;
  }

  // If the array is now empty, we should remove it from the hashtable.
  if (observers.IsEmpty()) {
    entry.Remove();
    CancelVisitedQueryIfPossible(aURI);
  }
}

void BaseHistory::NotifyVisited(
    nsIURI* aURI, VisitedStatus aStatus,
    const ContentParentSet* aListOfProcessesToNotify) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStatus != VisitedStatus::Unknown);

  if (aStatus == VisitedStatus::Unvisited &&
      !StaticPrefs::layout_css_notify_of_unvisited()) {
    return;
  }

  NotifyVisitedInThisProcess(aURI, aStatus);
  if (XRE_IsParentProcess()) {
    NotifyVisitedFromParent(aURI, aStatus, aListOfProcessesToNotify);
  }
}

void BaseHistory::NotifyVisitedInThisProcess(nsIURI* aURI,
                                             VisitedStatus aStatus) {
  if (NS_WARN_IF(!aURI)) {
    return;
  }

  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    // If we have no observers for this URI, we have nothing to notify about.
    return;
  }

  ObservingLinks& links = entry.Data();
  links.mStatus = aStatus;

  // If we have a key, it should have at least one observer.
  MOZ_ASSERT(!links.mLinks.IsEmpty());

  // Dispatch an event to each document which has a Link observing this URL.
  // These will fire asynchronously in the correct DocGroup.

  const bool visited = aStatus == VisitedStatus::Visited;
  for (Link* link : links.mLinks.BackwardRange()) {
    link->VisitedQueryFinished(visited);
  }

  // We never go from visited -> unvisited.
  //
  // FIXME(emilio): It seems unfortunate to remove a link to a visited uri and
  // then re-add it to the document to trigger a new visited query. It shouldn't
  // if we keep track of mStatus.
  if (visited) {
    entry.Remove();
  }
}

void BaseHistory::SendPendingVisitedResultsToChildProcesses() {
  MOZ_ASSERT(!mPendingResults.IsEmpty());

  mStartPendingResultsScheduled = false;

  auto results = std::move(mPendingResults);
  MOZ_ASSERT(mPendingResults.IsEmpty());

  nsTArray<ContentParent*> cplist;
  nsTArray<dom::VisitedQueryResult> resultsForProcess;
  ContentParent::GetAll(cplist);
  for (ContentParent* cp : cplist) {
    resultsForProcess.ClearAndRetainStorage();
    for (auto& result : results) {
      if (result.mProcessesToNotify.IsEmpty() ||
          result.mProcessesToNotify.Contains(cp)) {
        resultsForProcess.AppendElement(result.mResult);
      }
    }
    Unused << NS_WARN_IF(!cp->SendNotifyVisited(resultsForProcess));
  }
}

void BaseHistory::NotifyVisitedFromParent(
    nsIURI* aURI, VisitedStatus aStatus,
    const ContentParentSet* aListOfProcessesToNotify) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (aListOfProcessesToNotify && aListOfProcessesToNotify->IsEmpty()) {
    return;
  }

  auto& result = *mPendingResults.AppendElement();
  result.mResult.visited() = aStatus == VisitedStatus::Visited;
  result.mResult.uri() = aURI;
  if (aListOfProcessesToNotify) {
    for (auto* entry : *aListOfProcessesToNotify) {
      result.mProcessesToNotify.Insert(entry);
    }
  }

  if (mStartPendingResultsScheduled) {
    return;
  }

  mStartPendingResultsScheduled = NS_SUCCEEDED(NS_DispatchToMainThreadQueue(
      NewRunnableMethod(
          "BaseHistory::SendPendingVisitedResultsToChildProcesses", this,
          &BaseHistory::SendPendingVisitedResultsToChildProcesses),
      EventQueuePriority::Idle));
}

}  // namespace mozilla
