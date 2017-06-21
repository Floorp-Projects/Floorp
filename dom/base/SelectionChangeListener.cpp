/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of mozilla::dom::SelectionChangeListener
 */

#include "SelectionChangeListener.h"

#include "nsContentUtils.h"
#include "nsFrameSelection.h"
#include "nsRange.h"
#include "Selection.h"

using namespace mozilla;
using namespace mozilla::dom;

SelectionChangeListener::RawRangeData::RawRangeData(const nsRange* aRange)
{
  mozilla::ErrorResult rv;
  mStartParent = aRange->GetStartContainer(rv);
  rv.SuppressException();
  mEndParent = aRange->GetEndContainer(rv);
  rv.SuppressException();
  mStartOffset = aRange->GetStartOffset(rv);
  rv.SuppressException();
  mEndOffset = aRange->GetEndOffset(rv);
  rv.SuppressException();
}

bool
SelectionChangeListener::RawRangeData::Equals(const nsRange* aRange)
{
  mozilla::ErrorResult rv;
  bool eq = mStartParent == aRange->GetStartContainer(rv);
  rv.SuppressException();
  eq = eq && mEndParent == aRange->GetEndContainer(rv);
  rv.SuppressException();
  eq = eq && mStartOffset == aRange->GetStartOffset(rv);
  rv.SuppressException();
  eq = eq && mEndOffset == aRange->GetEndOffset(rv);
  rv.SuppressException();
  return eq;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            SelectionChangeListener::RawRangeData& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mStartParent, "mStartParent", aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mEndParent, "mEndParent", aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SelectionChangeListener)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SelectionChangeListener)
  tmp->mOldRanges.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SelectionChangeListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOldRanges);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SelectionChangeListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SelectionChangeListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SelectionChangeListener)

NS_IMETHODIMP
SelectionChangeListener::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                                nsISelection* aSel, int16_t aReason)
{
  RefPtr<Selection> sel = aSel->AsSelection();

  nsIDocument* doc = sel->GetParentObject();
  if (!(doc && nsContentUtils::IsSystemPrincipal(doc->NodePrincipal())) &&
      !nsFrameSelection::sSelectionEventsEnabled) {
    return NS_OK;
  }

  // Check if the ranges have actually changed
  // Don't bother checking this if we are hiding changes.
  if (mOldRanges.Length() == sel->RangeCount() && !sel->IsBlockingSelectionChangeEvents()) {
    bool changed = false;

    for (size_t i = 0; i < mOldRanges.Length(); i++) {
      if (!mOldRanges[i].Equals(sel->GetRangeAt(i))) {
        changed = true;
        break;
      }
    }

    if (!changed) {
      return NS_OK;
    }
  }

  // The ranges have actually changed, update the mOldRanges array
  mOldRanges.ClearAndRetainStorage();
  for (size_t i = 0; i < sel->RangeCount(); i++) {
    mOldRanges.AppendElement(RawRangeData(sel->GetRangeAt(i)));
  }

  // If we are hiding changes, then don't do anything else. We do this after we
  // update mOldRanges so that changes after the changes stop being hidden don't
  // incorrectly trigger a change, even though they didn't change anything
  if (sel->IsBlockingSelectionChangeEvents()) {
    return NS_OK;
  }

  // The spec currently doesn't say that we should dispatch this event on text
  // controls, so for now we only support doing that under a pref, disabled by
  // default.
  // See https://github.com/w3c/selection-api/issues/53.
  if (nsFrameSelection::sSelectionEventsOnTextControlsEnabled) {
    nsCOMPtr<nsINode> target;

    // Check if we should be firing this event to a different node than the
    // document. The limiter of the nsFrameSelection will be within the native
    // anonymous subtree of the node we want to fire the event on. We need to
    // climb up the parent chain to escape the native anonymous subtree, and then
    // fire the event.
    if (const nsFrameSelection* fs = sel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        while (root && root->IsInNativeAnonymousSubtree()) {
          root = root->GetParent();
        }

        target = root.forget();
      }
    }

    // If we didn't get a target before, we can instead fire the event at the document.
    if (!target) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
      target = doc.forget();
    }

    if (target) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(target, eSelectionChange, false);
      asyncDispatcher->PostDOMEvent();
    }
  } else {
    if (const nsFrameSelection* fs = sel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        if (root->IsInNativeAnonymousSubtree()) {
          return NS_OK;
        }
      }
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
    if (doc) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(doc, eSelectionChange, false);
      asyncDispatcher->PostDOMEvent();
    }
  }

  return NS_OK;
}
