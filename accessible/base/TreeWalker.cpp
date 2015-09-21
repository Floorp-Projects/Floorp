/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TreeWalker.h"

#include "Accessible.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TreeWalker
////////////////////////////////////////////////////////////////////////////////

TreeWalker::
  TreeWalker(Accessible* aContext, nsIContent* aContent, uint32_t aFlags) :
  mDoc(aContext->Document()), mContext(aContext), mAnchorNode(aContent),
  mFlags(aFlags)
{
  NS_ASSERTION(aContent, "No node for the accessible tree walker!");

  mChildFilter = mContext->CanHaveAnonChildren() ?
    nsIContent::eAllChildren : nsIContent::eAllButXBL;
  mChildFilter |= nsIContent::eSkipPlaceholderContent;

  if (aContent)
    PushState(aContent);

  MOZ_COUNT_CTOR(TreeWalker);
}

TreeWalker::~TreeWalker()
{
  MOZ_COUNT_DTOR(TreeWalker);
}

////////////////////////////////////////////////////////////////////////////////
// TreeWalker: private

Accessible*
TreeWalker::NextChild()
{
  if (mStateStack.IsEmpty())
    return nullptr;

  dom::AllChildrenIterator* top = &mStateStack[mStateStack.Length() - 1];
  while (top) {
    while (nsIContent* childNode = top->GetNextChild()) {
      bool isSubtreeHidden = false;
      Accessible* accessible = mFlags & eWalkCache ?
        mDoc->GetAccessible(childNode) :
        GetAccService()->GetOrCreateAccessible(childNode, mContext,
                                               &isSubtreeHidden);

      if (accessible)
        return accessible;

      // Walk down into subtree to find accessibles.
      if (!isSubtreeHidden && childNode->IsElement())
        top = PushState(childNode);
    }

    top = PopState();
  }

  // If we traversed the whole subtree of the anchor node. Move to next node
  // relative anchor node within the context subtree if possible.
  if (mFlags != eWalkContextTree)
    return nullptr;

  nsINode* contextNode = mContext->GetNode();
  while (mAnchorNode != contextNode) {
    nsINode* parentNode = mAnchorNode->GetFlattenedTreeParent();
    if (!parentNode || !parentNode->IsElement())
      return nullptr;

    nsIContent* parent = parentNode->AsElement();
    top = mStateStack.AppendElement(dom::AllChildrenIterator(parent,
                                                             mChildFilter));
    while (nsIContent* childNode = top->GetNextChild()) {
      if (childNode == mAnchorNode) {
        mAnchorNode = parent;
        return NextChild();
      }
    }

    // XXX We really should never get here, it means we're trying to find an
    // accessible for a dom node where iterating over its parent's children
    // doesn't return it. However this sometimes happens when we're asked for
    // the nearest accessible to place holder content which we ignore.
    mAnchorNode = parent;
  }

  return nullptr;
}

dom::AllChildrenIterator*
TreeWalker::PopState()
{
  size_t length = mStateStack.Length();
  mStateStack.RemoveElementAt(length - 1);
  return mStateStack.IsEmpty() ? nullptr : &mStateStack[mStateStack.Length() - 1];
}
