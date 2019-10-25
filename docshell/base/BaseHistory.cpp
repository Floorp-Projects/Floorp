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
using mozilla::dom::Link;

Document* BaseHistory::GetLinkDocument(Link& aLink) {
  Element* element = aLink.GetElement();
  // Element can only be null for mock_Link.
  return element ? element->OwnerDoc() : nullptr;
}

void BaseHistory::DispatchNotifyVisited(nsIURI* aURI, dom::Document* aDoc) {
  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<nsCOMPtr<nsIURI>, RefPtr<dom::Document>>(
          "BaseHistory::DispatchNotifyVisited", this,
          &BaseHistory::NotifyVisitedForDocument, aURI, aDoc);
  if (aDoc) {
    aDoc->Dispatch(TaskCategory::Other, runnable.forget());
  } else {
    NS_DispatchToMainThread(runnable.forget());
  }
}

void BaseHistory::NotifyVisitedForDocument(nsIURI* aURI, dom::Document* aDoc) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure that nothing invalidates our observer array while we're walking
  // over it.
  nsAutoScriptBlocker scriptBlocker;

  // If we have no observers for this URI, we have nothing to notify about.
  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    return;
  }

  TrackedURI& trackedURI = entry.Data();

  {
    // Update status of each Link node. We iterate over the array backwards so
    // we can remove the items as we encounter them.
    ObserverArray::BackwardIterator iter(trackedURI.mLinks);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      if (GetLinkDocument(*link) == aDoc) {
        link->SetLinkState(eLinkState_Visited);
        iter.Remove();
      }
    }
  }

  // If we don't have any links left, we can remove the array.
  if (trackedURI.mLinks.IsEmpty()) {
    entry.Remove();
  }
}

NS_IMETHODIMP
BaseHistory::RegisterVisitedCallback(nsIURI* aURI, Link* aLink) {
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
    // This is the first request for this URI, thus we must query its visited
    // state.
    auto result = StartVisitedQuery(aURI);
    if (result.isErr()) {
      entry.OrRemove();
      return result.unwrapErr();
    }
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

  TrackedURI& trackedURI = entry.OrInsert([] { return TrackedURI {}; });

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  MOZ_DIAGNOSTIC_ASSERT(!trackedURI.mLinks.Contains(aLink),
                        "Already tracking this Link object!");

  // Start tracking our Link.
  trackedURI.mLinks.AppendElement(aLink);

  // If this link has already been visited, we cannot synchronously mark
  // ourselves as visited, so instead we fire a runnable into our docgroup,
  // which will handle it for us.
  if (trackedURI.mVisited) {
    DispatchNotifyVisited(aURI, GetLinkDocument(*aLink));
  }

  return NS_OK;
}

NS_IMETHODIMP
BaseHistory::UnregisterVisitedCallback(nsIURI* aURI, Link* aLink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI, "Must pass a non-null URI!");
  MOZ_ASSERT(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  auto entry = mTrackedURIs.Lookup(aURI);
  if (!entry) {
    // GeckoViewHistory sometimes, for somewhat dubious reasons, removes links
    // from mTrackedURIs.
#ifndef MOZ_WIDGET_ANDROID
    MOZ_ASSERT_UNREACHABLE("Trying to unregister URI that wasn't registered!");
#endif
    return NS_ERROR_UNEXPECTED;
  }

  ObserverArray& observers = entry.Data().mLinks;
  if (!observers.RemoveElement(aLink)) {
#ifndef MOZ_WIDGET_ANDROID
    MOZ_ASSERT_UNREACHABLE("Trying to unregister node that wasn't registered!");
#endif
    return NS_ERROR_UNEXPECTED;
  }

  // If the array is now empty, we should remove it from the hashtable.
  if (observers.IsEmpty()) {
    entry.Remove();
    CancelVisitedQueryIfPossible(aURI);
  }

  return NS_OK;
}

} // namespace mozilla
