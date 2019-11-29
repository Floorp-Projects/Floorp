/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseHistory.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::Link;

static Document* GetLinkDocument(const Link& aLink) {
  Element* element = aLink.GetElement();
  // Element can only be null for mock_Link.
  return element ? element->OwnerDoc() : nullptr;
}

void BaseHistory::DispatchNotifyVisited(nsIURI* aURI, dom::Document* aDoc,
                                        VisitedStatus aStatus) {
  MOZ_ASSERT(aStatus != VisitedStatus::Unknown);

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<nsCOMPtr<nsIURI>, RefPtr<dom::Document>, VisitedStatus>(
          "BaseHistory::DispatchNotifyVisited", this,
          &BaseHistory::NotifyVisitedForDocument, aURI, aDoc, aStatus);
  if (aDoc) {
    aDoc->Dispatch(TaskCategory::Other, runnable.forget());
  } else {
    NS_DispatchToMainThread(runnable.forget());
  }
}

void BaseHistory::NotifyVisitedForDocument(nsIURI* aURI, dom::Document* aDoc,
                                           VisitedStatus aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStatus != VisitedStatus::Unknown);

  // Make sure that nothing invalidates our observer array while we're walking
  // over it.
  nsAutoScriptBlocker scriptBlocker;

  // If we have no observers for this URI, we have nothing to notify about.
  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    return;
  }

  ObservingLinks& links = entry.Data();

  const bool visited = aStatus == VisitedStatus::Visited;
  {
    // Update status of each Link node. We iterate over the array backwards so
    // we can remove the items as we encounter them.
    ObserverArray::BackwardIterator iter(links.mLinks);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      if (GetLinkDocument(*link) == aDoc) {
        link->VisitedQueryFinished(visited);
        if (visited) {
          iter.Remove();
        }
      }
    }
  }

  // If we don't have any links left, we can remove the array.
  if (links.mLinks.IsEmpty()) {
    entry.Remove();
  }
}

void BaseHistory::ScheduleVisitedQuery(nsIURI* aURI) {
  mPendingQueries.PutEntry(aURI);
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
                self->StartPendingVisitedQueries(queries);
                MOZ_DIAGNOSTIC_ASSERT(self->mPendingQueries.IsEmpty());
              }),
          EventQueuePriority::Idle));
}

void BaseHistory::CancelVisitedQueryIfPossible(nsIURI* aURI) {
  mPendingQueries.RemoveEntry(aURI);
  // TODO(bug 1591393): It could be worth to make this virtual and allow places
  // to stop the existing database query? Needs some measurement.
}

nsresult BaseHistory::RegisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI, "Must pass a non-null URI!");
  if (XRE_IsContentProcess()) {
    MOZ_ASSERT(aLink, "Must pass a non-null Link!");
  }

  // Obtain our array of observers for this URI.
  auto entry = mTrackedURIs.LookupForAdd(aURI);
  MOZ_DIAGNOSTIC_ASSERT(!entry || !entry.Data().mLinks.IsEmpty(),
                        "An empty key was kept around in our hashtable!");
  if (!entry) {
    ScheduleVisitedQuery(aURI);
  }

  if (!aLink) {
    // In IPC builds, we are passed a nullptr Link from
    // ContentParent::RecvStartVisitedQuery.  All of our code after this point
    // assumes aLink is non-nullptr, so we have to return now.
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                          "We should only ever get a null Link "
                          "in the parent process!");
    // We don't want to remove if we're tracking other links.
    if (!entry) {
      entry.OrRemove();
    }
    return NS_OK;
  }

  ObservingLinks& links = entry.OrInsert([] { return ObservingLinks{}; });

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  MOZ_DIAGNOSTIC_ASSERT(!links.mLinks.Contains(aLink),
                        "Already tracking this Link object!");

  // Start tracking our Link.
  links.mLinks.AppendElement(aLink);

  // If this link has already been visited, we cannot synchronously mark
  // ourselves as visited, so instead we fire a runnable into our docgroup,
  // which will handle it for us.
  if (links.mStatus != VisitedStatus::Unknown) {
    DispatchNotifyVisited(aURI, GetLinkDocument(*aLink), links.mStatus);
  }

  return NS_OK;
}

void BaseHistory::UnregisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI, "Must pass a non-null URI!");
  MOZ_ASSERT(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    MOZ_ASSERT_UNREACHABLE("Trying to unregister URI that wasn't registered!");
    return;
  }

  ObserverArray& observers = entry.Data().mLinks;
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

void BaseHistory::NotifyVisited(nsIURI* aURI, VisitedStatus aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStatus != VisitedStatus::Unknown);
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

  // TODO(bug 1591090): Maybe a hashtable for this? An array could be bad.
  nsTArray<Document*> seen;  // Don't dispatch duplicate runnables.
  ObserverArray::BackwardIterator iter(links.mLinks);
  while (iter.HasMore()) {
    Link* link = iter.GetNext();
    Document* doc = GetLinkDocument(*link);
    if (seen.Contains(doc)) {
      continue;
    }
    seen.AppendElement(doc);
    DispatchNotifyVisited(aURI, doc, aStatus);
  }
}

}  // namespace mozilla
