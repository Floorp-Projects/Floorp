/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of mozilla::SelectionChangeEventDispatcher
 */

#include "SelectionChangeEventDispatcher.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsFrameSelection.h"
#include "nsRange.h"
#include "mozilla/dom/Selection.h"

namespace mozilla {

using namespace dom;

SelectionChangeEventDispatcher::RawRangeData::RawRangeData(
    const nsRange* aRange) {
  if (aRange->IsPositioned()) {
    mStartContainer = aRange->GetStartContainer();
    mEndContainer = aRange->GetEndContainer();
    mStartOffset = aRange->StartOffset();
    mEndOffset = aRange->EndOffset();
  } else {
    mStartContainer = nullptr;
    mEndContainer = nullptr;
    mStartOffset = 0;
    mEndOffset = 0;
  }
}

bool SelectionChangeEventDispatcher::RawRangeData::Equals(
    const nsRange* aRange) {
  if (!aRange->IsPositioned()) {
    return !mStartContainer;
  }
  return mStartContainer == aRange->GetStartContainer() &&
         mEndContainer == aRange->GetEndContainer() &&
         mStartOffset == aRange->StartOffset() &&
         mEndOffset == aRange->EndOffset();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    SelectionChangeEventDispatcher::RawRangeData& aField, const char* aName,
    uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mStartContainer,
                              "mStartContainer", aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mEndContainer, "mEndContainer",
                              aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SelectionChangeEventDispatcher)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SelectionChangeEventDispatcher)
  tmp->mOldRanges.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SelectionChangeEventDispatcher)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOldRanges);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SelectionChangeEventDispatcher, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SelectionChangeEventDispatcher, Release)

void SelectionChangeEventDispatcher::OnSelectionChange(Document* aDoc,
                                                       Selection* aSel,
                                                       int16_t aReason) {
  Document* doc = aSel->GetParentObject();
  if (!(doc && doc->NodePrincipal()->IsSystemPrincipal()) &&
      !StaticPrefs::dom_select_events_enabled()) {
    return;
  }

  // Check if the ranges have actually changed
  // Don't bother checking this if we are hiding changes.
  if (mOldRanges.Length() == aSel->RangeCount() &&
      !aSel->IsBlockingSelectionChangeEvents()) {
    bool changed = false;

    for (size_t i = 0; i < mOldRanges.Length(); i++) {
      if (!mOldRanges[i].Equals(aSel->GetRangeAt(i))) {
        changed = true;
        break;
      }
    }

    if (!changed) {
      return;
    }
  }

  // The ranges have actually changed, update the mOldRanges array
  mOldRanges.ClearAndRetainStorage();
  for (size_t i = 0; i < aSel->RangeCount(); i++) {
    mOldRanges.AppendElement(RawRangeData(aSel->GetRangeAt(i)));
  }

  if (doc) {
    nsPIDOMWindowInner* inner = doc->GetInnerWindow();
    if (inner && !inner->HasSelectionChangeEventListeners()) {
      return;
    }
  }

  // If we are hiding changes, then don't do anything else. We do this after we
  // update mOldRanges so that changes after the changes stop being hidden don't
  // incorrectly trigger a change, even though they didn't change anything
  if (aSel->IsBlockingSelectionChangeEvents()) {
    return;
  }

  // The spec currently doesn't say that we should dispatch this event on text
  // controls, so for now we only support doing that under a pref, disabled by
  // default.
  // See https://github.com/w3c/selection-api/issues/53.
  if (StaticPrefs::dom_select_events_textcontrols_enabled()) {
    nsCOMPtr<nsINode> target;

    // Check if we should be firing this event to a different node than the
    // document. The limiter of the nsFrameSelection will be within the native
    // anonymous subtree of the node we want to fire the event on. We need to
    // climb up the parent chain to escape the native anonymous subtree, and
    // then fire the event.
    if (const nsFrameSelection* fs = aSel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        while (root && root->IsInNativeAnonymousSubtree()) {
          root = root->GetParent();
        }

        target = std::move(root);
      }
    }

    // If we didn't get a target before, we can instead fire the event at the
    // document.
    if (!target) {
      target = aDoc;
    }

    if (target) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
          new AsyncEventDispatcher(target, eSelectionChange, CanBubble::eNo);
      asyncDispatcher->PostDOMEvent();
    }
  } else {
    if (const nsFrameSelection* fs = aSel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        if (root->IsInNativeAnonymousSubtree()) {
          return;
        }
      }
    }

    if (aDoc) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
          new AsyncEventDispatcher(aDoc, eSelectionChange, CanBubble::eNo);
      asyncDispatcher->PostDOMEvent();
    }
  }
}

}  // namespace mozilla
