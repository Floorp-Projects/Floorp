/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIFrame.h"
#include "nsCSSAnonBoxes.h"
#include "nsLayoutUtils.h"

namespace mozilla::dom {

FlattenedChildIterator::FlattenedChildIterator(const nsIContent* aParent,
                                               bool aStartAtBeginning)
    : mParent(aParent), mOriginalParent(aParent), mIsFirst(aStartAtBeginning) {
  if (!mParent->IsElement()) {
    // TODO(emilio): I think it probably makes sense to only allow constructing
    // FlattenedChildIterators with Element.
    return;
  }

  if (ShadowRoot* shadow = mParent->AsElement()->GetShadowRoot()) {
    mParent = shadow;
    mShadowDOMInvolved = true;
    return;
  }

  if (const auto* slot = HTMLSlotElement::FromNode(mParent)) {
    if (!slot->AssignedNodes().IsEmpty()) {
      mParentAsSlot = slot;
      if (!aStartAtBeginning) {
        mIndexInInserted = slot->AssignedNodes().Length();
      }
      mShadowDOMInvolved = true;
    }
  }
}

nsIContent* FlattenedChildIterator::GetNextChild() {
  // If we're already in the inserted-children array, look there first
  if (mParentAsSlot) {
    const nsTArray<RefPtr<nsINode>>& assignedNodes =
        mParentAsSlot->AssignedNodes();
    if (mIsFirst) {
      mIsFirst = false;
      MOZ_ASSERT(mIndexInInserted == 0);
      mChild = assignedNodes[0]->AsContent();
      return mChild;
    }
    MOZ_ASSERT(mIndexInInserted <= assignedNodes.Length());
    if (mIndexInInserted + 1 >= assignedNodes.Length()) {
      mIndexInInserted = assignedNodes.Length();
      return nullptr;
    }
    mChild = assignedNodes[++mIndexInInserted]->AsContent();
    return mChild;
  }

  if (mIsFirst) {  // at the beginning of the child list
    mChild = mParent->GetFirstChild();
    mIsFirst = false;
  } else if (mChild) {  // in the middle of the child list
    mChild = mChild->GetNextSibling();
  }

  return mChild;
}

bool FlattenedChildIterator::Seek(const nsIContent* aChildToFind) {
  if (!mParentAsSlot && aChildToFind->GetParent() == mParent &&
      !aChildToFind->IsRootOfNativeAnonymousSubtree()) {
    // Fast path: just point ourselves to aChildToFind, which is a
    // normal DOM child of ours.
    mChild = const_cast<nsIContent*>(aChildToFind);
    mIndexInInserted = 0;
    mIsFirst = false;
    return true;
  }

  // Can we add more fast paths here based on whether the parent of aChildToFind
  // is a This version can take shortcuts that the two-argument version
  // can't, so can be faster (and in fact cshadow insertion point or content
  // insertion point?

  // It would be nice to assert that we find aChildToFind, but bz thinks that
  // we might not find aChildToFind when called from ContentInserted
  // if first-letter frames are about.
  while (nsIContent* child = GetNextChild()) {
    if (child == aChildToFind) {
      return true;
    }
  }

  return false;
}

nsIContent* FlattenedChildIterator::GetPreviousChild() {
  if (mIsFirst) {  // at the beginning of the child list
    return nullptr;
  }
  if (mParentAsSlot) {
    const nsTArray<RefPtr<nsINode>>& assignedNodes =
        mParentAsSlot->AssignedNodes();
    MOZ_ASSERT(mIndexInInserted <= assignedNodes.Length());
    if (mIndexInInserted == 0) {
      mIsFirst = true;
      return nullptr;
    }
    mChild = assignedNodes[--mIndexInInserted]->AsContent();
    return mChild;
  }
  if (mChild) {  // in the middle of the child list
    mChild = mChild->GetPreviousSibling();
  } else {  // at the end of the child list
    mChild = mParent->GetLastChild();
  }
  if (!mChild) {
    mIsFirst = true;
  }

  return mChild;
}

nsIContent* AllChildrenIterator::Get() const {
  switch (mPhase) {
    case eAtMarkerKid: {
      Element* marker = nsLayoutUtils::GetMarkerPseudo(Parent());
      MOZ_ASSERT(marker, "No content marker frame at eAtMarkerKid phase");
      return marker;
    }

    case eAtBeforeKid: {
      Element* before = nsLayoutUtils::GetBeforePseudo(Parent());
      MOZ_ASSERT(before, "No content before frame at eAtBeforeKid phase");
      return before;
    }

    case eAtFlatTreeKids:
      return FlattenedChildIterator::Get();

    case eAtAnonKids:
      return mAnonKids[mAnonKidsIdx];

    case eAtAfterKid: {
      Element* after = nsLayoutUtils::GetAfterPseudo(Parent());
      MOZ_ASSERT(after, "No content after frame at eAtAfterKid phase");
      return after;
    }

    default:
      return nullptr;
  }
}

bool AllChildrenIterator::Seek(const nsIContent* aChildToFind) {
  if (mPhase == eAtBegin || mPhase == eAtMarkerKid) {
    Element* markerPseudo = nsLayoutUtils::GetMarkerPseudo(Parent());
    if (markerPseudo && markerPseudo == aChildToFind) {
      mPhase = eAtMarkerKid;
      return true;
    }
    mPhase = eAtBeforeKid;
  }
  if (mPhase == eAtBeforeKid) {
    Element* beforePseudo = nsLayoutUtils::GetBeforePseudo(Parent());
    if (beforePseudo && beforePseudo == aChildToFind) {
      return true;
    }
    mPhase = eAtFlatTreeKids;
  }

  if (mPhase == eAtFlatTreeKids) {
    if (FlattenedChildIterator::Seek(aChildToFind)) {
      return true;
    }
    mPhase = eAtAnonKids;
  }

  nsIContent* child = nullptr;
  do {
    child = GetNextChild();
  } while (child && child != aChildToFind);

  return child == aChildToFind;
}

void AllChildrenIterator::AppendNativeAnonymousChildren() {
  nsContentUtils::AppendNativeAnonymousChildren(Parent(), mAnonKids, mFlags);
}

nsIContent* AllChildrenIterator::GetNextChild() {
  if (mPhase == eAtBegin) {
    mPhase = eAtMarkerKid;
    if (Element* markerContent = nsLayoutUtils::GetMarkerPseudo(Parent())) {
      return markerContent;
    }
  }

  if (mPhase == eAtMarkerKid) {
    mPhase = eAtBeforeKid;
    if (Element* beforeContent = nsLayoutUtils::GetBeforePseudo(Parent())) {
      return beforeContent;
    }
  }

  if (mPhase == eAtBeforeKid) {
    // Advance into our explicit kids.
    mPhase = eAtFlatTreeKids;
  }

  if (mPhase == eAtFlatTreeKids) {
    if (nsIContent* kid = FlattenedChildIterator::GetNextChild()) {
      return kid;
    }
    mPhase = eAtAnonKids;
  }

  if (mPhase == eAtAnonKids) {
    if (mAnonKids.IsEmpty()) {
      MOZ_ASSERT(mAnonKidsIdx == UINT32_MAX);
      AppendNativeAnonymousChildren();
      mAnonKidsIdx = 0;
    } else {
      if (mAnonKidsIdx == UINT32_MAX) {
        mAnonKidsIdx = 0;
      } else {
        mAnonKidsIdx++;
      }
    }

    if (mAnonKidsIdx < mAnonKids.Length()) {
      return mAnonKids[mAnonKidsIdx];
    }

    mPhase = eAtAfterKid;
    if (Element* afterContent = nsLayoutUtils::GetAfterPseudo(Parent())) {
      return afterContent;
    }
  }

  mPhase = eAtEnd;
  return nullptr;
}

nsIContent* AllChildrenIterator::GetPreviousChild() {
  if (mPhase == eAtEnd) {
    MOZ_ASSERT(mAnonKidsIdx == mAnonKids.Length());
    mPhase = eAtAnonKids;
    Element* afterContent = nsLayoutUtils::GetAfterPseudo(Parent());
    if (afterContent) {
      mPhase = eAtAfterKid;
      return afterContent;
    }
  }

  if (mPhase == eAtAfterKid) {
    mPhase = eAtAnonKids;
  }

  if (mPhase == eAtAnonKids) {
    if (mAnonKids.IsEmpty()) {
      AppendNativeAnonymousChildren();
      mAnonKidsIdx = mAnonKids.Length();
    }

    // If 0 then it turns into UINT32_MAX, which indicates the iterator is
    // before the anonymous children.
    --mAnonKidsIdx;
    if (mAnonKidsIdx < mAnonKids.Length()) {
      return mAnonKids[mAnonKidsIdx];
    }
    mPhase = eAtFlatTreeKids;
  }

  if (mPhase == eAtFlatTreeKids) {
    if (nsIContent* kid = FlattenedChildIterator::GetPreviousChild()) {
      return kid;
    }

    Element* beforeContent = nsLayoutUtils::GetBeforePseudo(Parent());
    if (beforeContent) {
      mPhase = eAtBeforeKid;
      return beforeContent;
    }
  }

  if (mPhase == eAtFlatTreeKids || mPhase == eAtBeforeKid) {
    Element* markerContent = nsLayoutUtils::GetMarkerPseudo(Parent());
    if (markerContent) {
      mPhase = eAtMarkerKid;
      return markerContent;
    }
  }

  mPhase = eAtBegin;
  return nullptr;
}

}  // namespace mozilla::dom
