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


} // namespace mozilla
