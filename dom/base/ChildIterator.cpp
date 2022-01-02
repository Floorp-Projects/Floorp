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

ExplicitChildIterator::ExplicitChildIterator(const nsIContent* aParent,
                                             bool aStartAtBeginning)
    : mParent(aParent),
      mChild(nullptr),
      mDefaultChild(nullptr),
      mIsFirst(aStartAtBeginning),
      mIndexInInserted(0) {
  mParentAsSlot = HTMLSlotElement::FromNode(mParent);
}

nsIContent* ExplicitChildIterator::GetNextChild() {
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    MOZ_ASSERT(mChild);
    MOZ_ASSERT(!mDefaultChild);

    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();

      mChild = (mIndexInInserted < assignedNodes.Length())
                   ? assignedNodes[mIndexInInserted++]->AsContent()
                   : nullptr;
      if (!mChild) {
        mIndexInInserted = 0;
      }
      return mChild;
    }

    MOZ_ASSERT_UNREACHABLE("This needs to be revisited");
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    MOZ_ASSERT(mChild);

    mDefaultChild = mDefaultChild->GetNextSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetNextSibling();
  } else if (mIsFirst) {  // at the beginning of the child list
    // For slot parent, iterate over assigned nodes if not empty, otherwise
    // fall through and iterate over direct children (fallback content).
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();
      if (!assignedNodes.IsEmpty()) {
        mIndexInInserted = 1;
        mChild = assignedNodes[0]->AsContent();
        mIsFirst = false;
        return mChild;
      }
    }

    mChild = mParent->GetFirstChild();
    mIsFirst = false;
  } else if (mChild) {  // in the middle of the child list
    mChild = mChild->GetNextSibling();
  }

  return mChild;
}

void FlattenedChildIterator::Init() {
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
  if (mParentAsSlot) {
    mShadowDOMInvolved = true;
  }
}

bool ExplicitChildIterator::Seek(const nsIContent* aChildToFind) {
  if (aChildToFind->GetParent() == mParent &&
      !aChildToFind->IsRootOfNativeAnonymousSubtree()) {
    // Fast path: just point ourselves to aChildToFind, which is a
    // normal DOM child of ours.
    mChild = const_cast<nsIContent*>(aChildToFind);
    mIndexInInserted = 0;
    mDefaultChild = nullptr;
    mIsFirst = false;
    return true;
  }

  // Can we add more fast paths here based on whether the parent of aChildToFind
  // is a shadow insertion point or content insertion point?

  // Slow path: just walk all our kids.
  return Seek(aChildToFind, nullptr);
}

nsIContent* ExplicitChildIterator::Get() const {
  MOZ_ASSERT(!mIsFirst);

  // When mParentAsSlot is set, mChild is always set to the current child. It
  // does not matter whether mChild is an assigned node or a fallback content.
  if (mParentAsSlot) {
    return mChild;
  }

  if (mIndexInInserted) {
    MOZ_ASSERT_UNREACHABLE("This needs to be revisited");
  }

  return mDefaultChild ? mDefaultChild : mChild;
}

nsIContent* ExplicitChildIterator::GetPreviousChild() {
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();

      mChild = (--mIndexInInserted)
                   ? assignedNodes[mIndexInInserted - 1]->AsContent()
                   : nullptr;

      if (!mChild) {
        mIsFirst = true;
      }
      return mChild;
    }

    MOZ_ASSERT_UNREACHABLE("This needs to be revisited");
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    mDefaultChild = mDefaultChild->GetPreviousSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetPreviousSibling();
  } else if (mIsFirst) {  // at the beginning of the child list
    return nullptr;
  } else if (mChild) {  // in the middle of the child list
    mChild = mChild->GetPreviousSibling();
  } else {  // at the end of the child list
    // For slot parent, iterate over assigned nodes if not empty, otherwise
    // fall through and iterate over direct children (fallback content).
    if (mParentAsSlot) {
      const nsTArray<RefPtr<nsINode>>& assignedNodes =
          mParentAsSlot->AssignedNodes();
      if (!assignedNodes.IsEmpty()) {
        mIndexInInserted = assignedNodes.Length();
        mChild = assignedNodes[mIndexInInserted - 1]->AsContent();
        return mChild;
      }
    }

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
      Element* marker = nsLayoutUtils::GetMarkerPseudo(mOriginalContent);
      MOZ_ASSERT(marker, "No content marker frame at eAtMarkerKid phase");
      return marker;
    }

    case eAtBeforeKid: {
      Element* before = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
      MOZ_ASSERT(before, "No content before frame at eAtBeforeKid phase");
      return before;
    }

    case eAtExplicitKids:
      return ExplicitChildIterator::Get();

    case eAtAnonKids:
      return mAnonKids[mAnonKidsIdx];

    case eAtAfterKid: {
      Element* after = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
      MOZ_ASSERT(after, "No content after frame at eAtAfterKid phase");
      return after;
    }

    default:
      return nullptr;
  }
}

bool AllChildrenIterator::Seek(const nsIContent* aChildToFind) {
  if (mPhase == eAtBegin || mPhase == eAtMarkerKid) {
    mPhase = eAtBeforeKid;
    Element* markerPseudo = nsLayoutUtils::GetMarkerPseudo(mOriginalContent);
    if (markerPseudo && markerPseudo == aChildToFind) {
      mPhase = eAtMarkerKid;
      return true;
    }
  }
  if (mPhase == eAtBeforeKid) {
    mPhase = eAtExplicitKids;
    Element* beforePseudo = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforePseudo && beforePseudo == aChildToFind) {
      mPhase = eAtBeforeKid;
      return true;
    }
  }

  if (mPhase == eAtExplicitKids) {
    if (ExplicitChildIterator::Seek(aChildToFind)) {
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
  nsContentUtils::AppendNativeAnonymousChildren(mOriginalContent, mAnonKids,
                                                mFlags);
}

nsIContent* AllChildrenIterator::GetNextChild() {
  if (mPhase == eAtBegin) {
    Element* markerContent = nsLayoutUtils::GetMarkerPseudo(mOriginalContent);
    if (markerContent) {
      mPhase = eAtMarkerKid;
      return markerContent;
    }
  }

  if (mPhase == eAtBegin || mPhase == eAtMarkerKid) {
    mPhase = eAtExplicitKids;
    Element* beforeContent = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforeContent) {
      mPhase = eAtBeforeKid;
      return beforeContent;
    }
  }

  if (mPhase == eAtBeforeKid) {
    // Advance into our explicit kids.
    mPhase = eAtExplicitKids;
  }

  if (mPhase == eAtExplicitKids) {
    nsIContent* kid = ExplicitChildIterator::GetNextChild();
    if (kid) {
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

    Element* afterContent = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
    if (afterContent) {
      mPhase = eAtAfterKid;
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
    Element* afterContent = nsLayoutUtils::GetAfterPseudo(mOriginalContent);
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
    mPhase = eAtExplicitKids;
  }

  if (mPhase == eAtExplicitKids) {
    nsIContent* kid = ExplicitChildIterator::GetPreviousChild();
    if (kid) {
      return kid;
    }

    Element* beforeContent = nsLayoutUtils::GetBeforePseudo(mOriginalContent);
    if (beforeContent) {
      mPhase = eAtBeforeKid;
      return beforeContent;
    }
  }

  if (mPhase == eAtExplicitKids || mPhase == eAtBeforeKid) {
    Element* markerContent = nsLayoutUtils::GetMarkerPseudo(mOriginalContent);
    if (markerContent) {
      mPhase = eAtMarkerKid;
      return markerContent;
    }
  }

  mPhase = eAtBegin;
  return nullptr;
}

}  // namespace mozilla::dom
