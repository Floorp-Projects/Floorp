/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "mozilla/dom/XBLChildrenElement.h"
#include "mozilla/dom/HTMLContentElement.h"

namespace mozilla {
namespace dom {

class MatchedNodes {
public:
  MatchedNodes(HTMLContentElement* aInsertionPoint)
    : mIsContentElement(true), mContentElement(aInsertionPoint) {}

  MatchedNodes(XBLChildrenElement* aInsertionPoint)
    : mIsContentElement(false), mChildrenElement(aInsertionPoint) {}

  uint32_t Length() const
  {
    return mIsContentElement ? mContentElement->MatchedNodes().Length()
                             : mChildrenElement->mInsertedChildren.Length();
  }

  nsIContent* operator[](int32_t aIndex) const
  {
    return mIsContentElement ? mContentElement->MatchedNodes()[aIndex]
                             : mChildrenElement->mInsertedChildren[aIndex];
  }

  bool IsEmpty() const
  {
    return mIsContentElement ? mContentElement->MatchedNodes().IsEmpty()
                             : mChildrenElement->mInsertedChildren.IsEmpty();
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
  MOZ_ASSERT(aContent->IsHTML(nsGkAtoms::content));
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
  while (mChild && nsContentUtils::IsContentInsertionPoint(mChild)) {
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
  }

  return mChild;
}

FlattenedChildIterator::FlattenedChildIterator(nsIContent* aParent)
  : ExplicitChildIterator(aParent), mXBLInvolved(false)
{
  nsXBLBinding* binding =
    aParent->OwnerDoc()->BindingManager()->GetBindingWithContent(aParent);

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
  if (!mXBLInvolved && aParent->GetBindingParent()) {
    for (nsIContent* child = aParent->GetFirstChild();
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

nsIContent* FlattenedChildIterator::Get()
{
  MOZ_ASSERT(!mIsFirst);

  if (mIndexInInserted) {
    XBLChildrenElement* point = static_cast<XBLChildrenElement*>(mChild);
    return point->mInsertedChildren[mIndexInInserted - 1];
  }
  return mDefaultChild ? mDefaultChild : mChild;
}

nsIContent* FlattenedChildIterator::GetPreviousChild()
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
  while (mChild && nsContentUtils::IsContentInsertionPoint(mChild)) {
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
  }

  if (!mChild) {
    mIsFirst = true;
  }

  return mChild;
}

} // namespace dom
} // namespace mozilla
