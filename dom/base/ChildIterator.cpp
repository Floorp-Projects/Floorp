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
  return MatchedNodes(static_cast<HTMLContentElement*>(aContent));
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
      HTMLShadowElement* shadowElem = static_cast<HTMLShadowElement*>(mChild);
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
      HTMLShadowElement* shadowElem = static_cast<HTMLShadowElement*>(mChild);
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
      nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
      MOZ_ASSERT(frame, "No frame at eAtBeforeKid phase");
      nsIFrame* beforeFrame = nsLayoutUtils::GetBeforeFrame(frame);
      MOZ_ASSERT(beforeFrame, "No content before frame at eAtBeforeKid phase");
      return beforeFrame->GetContent();
    }

    case eAtExplicitKids:
      return ExplicitChildIterator::Get();

    case eAtAnonKids:
      return mAnonKids[mAnonKidsIdx];

    case eAtAfterKid: {
      nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
      MOZ_ASSERT(frame, "No frame at eAtAfterKid phase");
      nsIFrame* afterFrame = nsLayoutUtils::GetAfterFrame(frame);
      MOZ_ASSERT(afterFrame, "No content before frame at eAtBeforeKid phase");
      return afterFrame->GetContent();
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
    nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
    if (frame) {
      nsIFrame* beforeFrame = nsLayoutUtils::GetBeforeFrame(frame);
      if (beforeFrame) {
        if (beforeFrame->GetContent() == aChildToFind) {
          mPhase = eAtBeforeKid;
          return true;
        }
      }
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
  AppendNativeAnonymousChildrenFromFrame(mOriginalContent->GetPrimaryFrame());

  // The root scroll frame is not the primary frame of the root element.
  // Detect and handle this case.
  if (mOriginalContent == mOriginalContent->OwnerDoc()->GetRootElement()) {
    nsIPresShell* presShell = mOriginalContent->OwnerDoc()->GetShell();
    nsIFrame* scrollFrame = presShell ? presShell->GetRootScrollFrame() : nullptr;
    if (scrollFrame) {
      AppendNativeAnonymousChildrenFromFrame(scrollFrame);
    }
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
    nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
    if (frame) {
      nsIFrame* beforeFrame = nsLayoutUtils::GetBeforeFrame(frame);
      if (beforeFrame) {
        mPhase = eAtBeforeKid;
        return beforeFrame->GetContent();
      }
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

    nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
    if (frame) {
      nsIFrame* afterFrame = nsLayoutUtils::GetAfterFrame(frame);
      if (afterFrame) {
        mPhase = eAtAfterKid;
        return afterFrame->GetContent();
      }
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
    nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
    if (frame) {
      nsIFrame* afterFrame = nsLayoutUtils::GetAfterFrame(frame);
      if (afterFrame) {
        mPhase = eAtAfterKid;
        return afterFrame->GetContent();
      }
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

    nsIFrame* frame = mOriginalContent->GetPrimaryFrame();
    if (frame) {
      nsIFrame* beforeFrame = nsLayoutUtils::GetBeforeFrame(frame);
      if (beforeFrame) {
        mPhase = eAtBeforeKid;
        return beforeFrame->GetContent();
      }
    }
  }

  mPhase = eAtBegin;
  return nullptr;
}

static bool
IsNativeAnonymousImplementationOfPseudoElement(nsIContent* aContent)
{
  // First, we need a frame. This leads to the tricky issue of what we can
  // infer if the frame is null.
  //
  // Unlike regular nodes, native anonymous content (NAC) gets created during
  // frame construction, which happens after the main style traversal. This
  // means that we have to manually resolve style for those nodes shortly after
  // they're created, either by (a) invoking ResolvePseudoElementStyle (for PE
  // NAC), or (b) handing the subtree off to Servo for a mini-traversal (for
  // non-PE NAC). We have assertions in nsCSSFrameConstructor that we don't do
  // both.
  //
  // Once that happens, the NAC has a frame. So if we have no frame here,
  // we're either not NAC, or in the process of doing (b). Either way, this
  // isn't a PE.
  nsIFrame* f = aContent->GetPrimaryFrame();
  if (!f) {
    return false;
  }

  // Get the pseudo type.
  CSSPseudoElementType pseudoType = f->StyleContext()->GetPseudoType();

  // In general nodes never get anonymous box style. However, there are a few
  // special cases:
  //
  // * We somewhat-confusingly give text nodes a style context tagged with
  //   ":-moz-text", so we need to check for the anonymous box case here.
  // * The primary frame for table elements is an anonymous box that inherits
  //   from the table's style.
  if (pseudoType == CSSPseudoElementType::AnonBox) {
    MOZ_ASSERT(f->StyleContext()->GetPseudo() == nsCSSAnonBoxes::mozText ||
               f->StyleContext()->GetPseudo() == nsCSSAnonBoxes::tableWrapper);
    return false;
  }

  // Finally check the actual pseudo type.
  bool isImpl = pseudoType != CSSPseudoElementType::NotPseudo;
  MOZ_ASSERT_IF(isImpl, aContent->IsRootOfNativeAnonymousSubtree());
  return isImpl;
}

/* static */ bool
StyleChildrenIterator::IsNeeded(Element* aElement)
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

  // If the node has native anonymous content, return true.
  nsIAnonymousContentCreator* ac = do_QueryFrame(aElement->GetPrimaryFrame());
  if (ac) {
    return true;
  }

  // The root element has a scroll frame that is not the primary frame, so we
  // need to do special checking for that case.
  if (aElement == aElement->OwnerDoc()->GetRootElement()) {
    return true;
  }

  return false;
}


nsIContent*
StyleChildrenIterator::GetNextChild()
{
  while (nsIContent* child = AllChildrenIterator::GetNextChild()) {
    if (IsNativeAnonymousImplementationOfPseudoElement(child)) {
      // Skip any native-anonymous children that are used to implement pseudo-
      // elements. These match pseudo-element selectors instead of being
      // considered a child of their host, and thus the style system needs to
      // handle them separately.
    } else {
      return child;
    }
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla
