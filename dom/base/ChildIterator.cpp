/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "mozilla/dom/XBLChildrenElement.h"
#include "mozilla/dom/HTMLContentElement.h"
#include "mozilla/dom/HTMLShadowElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIFrame.h"
#include "nsCSSAnonBoxes.h"

namespace mozilla {
namespace dom {

class MatchedNodes {
public:
  explicit MatchedNodes(HTMLContentElement* aInsertionPoint)
    : mIsContentElement(true), mContentElement(aInsertionPoint) {}

  explicit MatchedNodes(XBLChildrenElement* aInsertionPoint)
    : mIsContentElement(false), mChildrenElement(aInsertionPoint) {}

  uint32_t Length() const
  {
    return mIsContentElement ? mContentElement->MatchedNodes().Length()
                             : mChildrenElement->InsertedChildrenLength();
  }

  nsIContent* operator[](int32_t aIndex) const
  {
    return mIsContentElement ? mContentElement->MatchedNodes()[aIndex]
                             : mChildrenElement->InsertedChild(aIndex);
  }

  bool IsEmpty() const
  {
    return mIsContentElement ? mContentElement->MatchedNodes().IsEmpty()
                             : !mChildrenElement->HasInsertedChildren();
  }
protected:
  bool mIsContentElement;
  union {
    HTMLContentElement* mContentElement;
    XBLChildrenElement* mChildrenElement;
  };
};

static inline MatchedNodes
GetMatchedNodesForPoint(nsIContent* aContent)
{
  if (aContent->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
    // XBL case
    return MatchedNodes(static_cast<XBLChildrenElement*>(aContent));
  }

  // Web components case
  MOZ_ASSERT(aContent->IsHTMLElement(nsGkAtoms::content));
  return MatchedNodes(HTMLContentElement::FromContent(aContent));
}

nsIContent*
ExplicitChildIterator::GetNextChild()
{
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    MOZ_ASSERT(mChild);
    MOZ_ASSERT(nsContentUtils::IsContentInsertionPoint(mChild));
    MOZ_ASSERT(!mDefaultChild);

    MatchedNodes assignedChildren = GetMatchedNodesForPoint(mChild);
    if (mIndexInInserted < assignedChildren.Length()) {
      return assignedChildren[mIndexInInserted++];
    }
    mIndexInInserted = 0;
    mChild = mChild->GetNextSibling();
  } else if (mShadowIterator) {
    // If we're inside of a <shadow> element, look through the
    // explicit children of the projected ShadowRoot via
    // the mShadowIterator.
    nsIContent* nextChild = mShadowIterator->GetNextChild();
    if (nextChild) {
      return nextChild;
    }

    mShadowIterator = nullptr;
    mChild = mChild->GetNextSibling();
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    MOZ_ASSERT(mChild);
    MOZ_ASSERT(nsContentUtils::IsContentInsertionPoint(mChild));

    mDefaultChild = mDefaultChild->GetNextSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetNextSibling();
  } else if (mIsFirst) {  // at the beginning of the child list
    mChild = mParent->GetFirstChild();
    mIsFirst = false;
  } else if (mChild) { // in the middle of the child list
    mChild = mChild->GetNextSibling();
  }

  // Iterate until we find a non-insertion point, or an insertion point with
  // content.
  while (mChild) {
    // If the current child being iterated is a shadow insertion point then
    // the iterator needs to go into the projected ShadowRoot.
    if (ShadowRoot::IsShadowInsertionPoint(mChild)) {
      // Look for the next child in the projected ShadowRoot for the <shadow>
      // element.
      HTMLShadowElement* shadowElem = HTMLShadowElement::FromContent(mChild);
      ShadowRoot* projectedShadow = shadowElem->GetOlderShadowRoot();
      if (projectedShadow) {
        mShadowIterator = new ExplicitChildIterator(projectedShadow);
        nsIContent* nextChild = mShadowIterator->GetNextChild();
        if (nextChild) {
          return nextChild;
        }
        mShadowIterator = nullptr;
      }
      mChild = mChild->GetNextSibling();
    } else if (nsContentUtils::IsContentInsertionPoint(mChild)) {
      // If the current child being iterated is a content insertion point
      // then the iterator needs to return the nodes distributed into
      // the content insertion point.
      MatchedNodes assignedChildren = GetMatchedNodesForPoint(mChild);
      if (!assignedChildren.IsEmpty()) {
        // Iterate through elements projected on insertion point.
        mIndexInInserted = 1;
        return assignedChildren[0];
      }

      // Insertion points inside fallback/default content
      // are considered inactive and do not get assigned nodes.
      mDefaultChild = mChild->GetFirstChild();
      if (mDefaultChild) {
        return mDefaultChild;
      }

      // If we have an insertion point with no assigned nodes and
      // no default content, move on to the next node.
      mChild = mChild->GetNextSibling();
    } else {
      // mChild is not an insertion point, thus it is the next node to
      // return from this iterator.
      break;
    }
  }

  return mChild;
}

void
FlattenedChildIterator::Init(bool aIgnoreXBL)
{
  if (aIgnoreXBL) {
    return;
  }

  nsXBLBinding* binding =
    mParent->OwnerDoc()->BindingManager()->GetBindingWithContent(mParent);

  if (binding) {
    nsIContent* anon = binding->GetAnonymousContent();
    if (anon) {
      mParent = anon;
      mXBLInvolved = true;
    }
  }

  // We set mXBLInvolved to true if either:
  // - The node we're iterating has a binding with content attached to it.
  // - The node is generated XBL content and has an <xbl:children> child.
  if (!mXBLInvolved && mParent->GetBindingParent()) {
    for (nsIContent* child = mParent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
        MOZ_ASSERT(child->GetBindingParent());
        mXBLInvolved = true;
        break;
      }
    }
  }
}

bool
ExplicitChildIterator::Seek(nsIContent* aChildToFind)
{
  if (aChildToFind->GetParent() == mParent &&
      !aChildToFind->IsRootOfAnonymousSubtree()) {
    // Fast path: just point ourselves to aChildToFind, which is a
    // normal DOM child of ours.
    MOZ_ASSERT(!ShadowRoot::IsShadowInsertionPoint(aChildToFind));
    MOZ_ASSERT(!nsContentUtils::IsContentInsertionPoint(aChildToFind));
    mChild = aChildToFind;
    mIndexInInserted = 0;
    mShadowIterator = nullptr;
    mDefaultChild = nullptr;
    mIsFirst = false;
    return true;
  }

  // Can we add more fast paths here based on whether the parent of aChildToFind
  // is a shadow insertion point or content insertion point?

  // Slow path: just walk all our kids.
  return Seek(aChildToFind, nullptr);
}

nsIContent*
ExplicitChildIterator::Get() const
{
  MOZ_ASSERT(!mIsFirst);

  if (mIndexInInserted) {
    MatchedNodes assignedChildren = GetMatchedNodesForPoint(mChild);
    return assignedChildren[mIndexInInserted - 1];
  } else if (mShadowIterator)  {
    return mShadowIterator->Get();
  }
  return mDefaultChild ? mDefaultChild : mChild;
}

nsIContent*
ExplicitChildIterator::GetPreviousChild()
{
  // If we're already in the inserted-children array, look there first
  if (mIndexInInserted) {
    // NB: mIndexInInserted points one past the last returned child so we need
    // to look *two* indices back in order to return the previous child.
    MatchedNodes assignedChildren = GetMatchedNodesForPoint(mChild);
    if (--mIndexInInserted) {
      return assignedChildren[mIndexInInserted - 1];
    }
    mChild = mChild->GetPreviousSibling();
  } else if (mShadowIterator) {
    nsIContent* previousChild = mShadowIterator->GetPreviousChild();
    if (previousChild) {
      return previousChild;
    }
    mShadowIterator = nullptr;
    mChild = mChild->GetPreviousSibling();
  } else if (mDefaultChild) {
    // If we're already in default content, check if there are more nodes there
    mDefaultChild = mDefaultChild->GetPreviousSibling();
    if (mDefaultChild) {
      return mDefaultChild;
    }

    mChild = mChild->GetPreviousSibling();
  } else if (mIsFirst) { // at the beginning of the child list
    return nullptr;
  } else if (mChild) { // in the middle of the child list
    mChild = mChild->GetPreviousSibling();
  } else { // at the end of the child list
    mChild = mParent->GetLastChild();
  }

  // Iterate until we find a non-insertion point, or an insertion point with
  // content.
  while (mChild) {
    if (ShadowRoot::IsShadowInsertionPoint(mChild)) {
      // If the current child being iterated is a shadow insertion point then
      // the iterator needs to go into the projected ShadowRoot.
      HTMLShadowElement* shadowElem = HTMLShadowElement::FromContent(mChild);
      ShadowRoot* projectedShadow = shadowElem->GetOlderShadowRoot();
      if (projectedShadow) {
        // Create a ExplicitChildIterator that begins iterating from the end.
        mShadowIterator = new ExplicitChildIterator(projectedShadow, false);
        nsIContent* previousChild = mShadowIterator->GetPreviousChild();
        if (previousChild) {
          return previousChild;
        }
        mShadowIterator = nullptr;
      }
      mChild = mChild->GetPreviousSibling();
    } else if (nsContentUtils::IsContentInsertionPoint(mChild)) {
      // If the current child being iterated is a content insertion point
      // then the iterator needs to return the nodes distributed into
      // the content insertion point.
      MatchedNodes assignedChildren = GetMatchedNodesForPoint(mChild);
      if (!assignedChildren.IsEmpty()) {
        mIndexInInserted = assignedChildren.Length();
        return assignedChildren[mIndexInInserted - 1];
      }

      mDefaultChild = mChild->GetLastChild();
      if (mDefaultChild) {
        return mDefaultChild;
      }

      mChild = mChild->GetPreviousSibling();
    } else {
      // mChild is not an insertion point, thus it is the next node to
      // return from this iterator.
      break;
    }
  }

  if (!mChild) {
    mIsFirst = true;
  }

  return mChild;
}

nsIContent*
AllChildrenIterator::Get() const
{
  switch (mPhase) {
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


bool
AllChildrenIterator::Seek(nsIContent* aChildToFind)
{
  if (mPhase == eAtBegin || mPhase == eAtBeforeKid) {
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

void
AllChildrenIterator::AppendNativeAnonymousChildren()
{
  if (nsIFrame* primaryFrame = mOriginalContent->GetPrimaryFrame()) {
    // NAC created by the element's primary frame.
    AppendNativeAnonymousChildrenFromFrame(primaryFrame);

    // NAC created by any other non-primary frames for the element.
    AutoTArray<nsIFrame::OwnedAnonBox,8> ownedAnonBoxes;
    primaryFrame->AppendOwnedAnonBoxes(ownedAnonBoxes);
    for (nsIFrame::OwnedAnonBox& box : ownedAnonBoxes) {
      MOZ_ASSERT(box.mAnonBoxFrame->GetContent() == mOriginalContent);
      AppendNativeAnonymousChildrenFromFrame(box.mAnonBoxFrame);
    }
  }

  // The root scroll frame is not the primary frame of the root element.
  // Detect and handle this case.
  if (!(mFlags & nsIContent::eSkipDocumentLevelNativeAnonymousContent) &&
      mOriginalContent == mOriginalContent->OwnerDoc()->GetRootElement()) {
    nsContentUtils::AppendDocumentLevelNativeAnonymousContentTo(
        mOriginalContent->OwnerDoc(), mAnonKids);
  }
}

void
AllChildrenIterator::AppendNativeAnonymousChildrenFromFrame(nsIFrame* aFrame)
{
  nsIAnonymousContentCreator* ac = do_QueryFrame(aFrame);
  if (ac) {
    ac->AppendAnonymousContentTo(mAnonKids, mFlags);
  }
}

nsIContent*
AllChildrenIterator::GetNextChild()
{
  if (mPhase == eAtBegin) {
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
    }
    else {
      if (mAnonKidsIdx == UINT32_MAX) {
        mAnonKidsIdx = 0;
      }
      else {
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

nsIContent*
AllChildrenIterator::GetPreviousChild()
{
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

  mPhase = eAtBegin;
  return nullptr;
}

/* static */ bool
StyleChildrenIterator::IsNeeded(const Element* aElement)
{
  // If the node is in an anonymous subtree, we conservatively return true to
  // handle insertion points.
  if (aElement->IsInAnonymousSubtree()) {
    return true;
  }

  // If the node has an XBL binding with anonymous content return true.
  if (aElement->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsBindingManager* manager = aElement->OwnerDoc()->BindingManager();
    nsXBLBinding* binding = manager->GetBindingWithContent(aElement);
    if (binding && binding->GetAnonymousContent()) {
      return true;
    }
  }

  // If the node has a ::before or ::after pseudo, return true, because we want
  // to visit those.
  //
  // TODO(emilio): Make this fast adding a bit? or, perhaps just using
  // ProbePseudoElementStyle? It should be quite fast in Stylo.
  if (aElement->GetProperty(nsGkAtoms::beforePseudoProperty) ||
      aElement->GetProperty(nsGkAtoms::afterPseudoProperty)) {
    return true;
  }

  // If the node has native anonymous content, return true.
  nsIAnonymousContentCreator* ac = do_QueryFrame(aElement->GetPrimaryFrame());
  if (ac) {
    return true;
  }

  return false;
}


nsIContent*
StyleChildrenIterator::GetNextChild()
{
  return AllChildrenIterator::GetNextChild();
}

} // namespace dom
} // namespace mozilla
