/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TreeWalker.h"

#include "Accessible.h"
#include "AccIterator.h"
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
  TreeWalker(Accessible* aContext) :
  mDoc(aContext->Document()), mContext(aContext), mAnchorNode(nullptr),
  mARIAOwnsIdx(0),
  mChildFilter(nsIContent::eSkipPlaceholderContent), mFlags(0)
{
  mChildFilter |= mContext->NoXBLKids() ?
    nsIContent::eAllButXBL : nsIContent::eAllChildren;

  mAnchorNode = mContext->IsDoc() ?
    mDoc->DocumentNode()->GetRootElement() : mContext->GetContent();

  if (mAnchorNode) {
    PushState(mAnchorNode);
  }

  MOZ_COUNT_CTOR(TreeWalker);
}

TreeWalker::
  TreeWalker(Accessible* aContext, nsIContent* aAnchorNode, uint32_t aFlags) :
  mDoc(aContext->Document()), mContext(aContext), mAnchorNode(aAnchorNode),
  mARIAOwnsIdx(0),
  mChildFilter(nsIContent::eSkipPlaceholderContent), mFlags(aFlags)
{
  MOZ_ASSERT(mFlags & eWalkCache, "This constructor cannot be used for tree creation");
  MOZ_ASSERT(aAnchorNode, "No anchor node for the accessible tree walker");

  mChildFilter |= mContext->NoXBLKids() ?
    nsIContent::eAllButXBL : nsIContent::eAllChildren;

  PushState(aAnchorNode);

  MOZ_COUNT_CTOR(TreeWalker);
}

TreeWalker::~TreeWalker()
{
  MOZ_COUNT_DTOR(TreeWalker);
}

////////////////////////////////////////////////////////////////////////////////
// TreeWalker: private

Accessible*
TreeWalker::Next()
{
  if (mStateStack.IsEmpty()) {
    return mDoc->ARIAOwnedAt(mContext, mARIAOwnsIdx++);
  }

  dom::AllChildrenIterator* top = &mStateStack[mStateStack.Length() - 1];
  while (top) {
    while (nsIContent* childNode = top->GetNextChild()) {
      bool skipSubtree = false;
      Accessible* child = nullptr;
      if (mFlags & eWalkCache) {
        child = mDoc->GetAccessible(childNode);
      }
      else if (mContext->IsAcceptableChild(childNode)) {
        child = GetAccService()->
          GetOrCreateAccessible(childNode, mContext, &skipSubtree);
      }

      // Ignore the accessible and its subtree if it was repositioned by means
      // of aria-owns.
      if (child) {
        if (child->IsRelocated()) {
          continue;
        }
        return child;
      }

      // Walk down into subtree to find accessibles.
      if (!skipSubtree && childNode->IsElement()) {
        top = PushState(childNode);
      }
    }
    top = PopState();
  }

  // If we traversed the whole subtree of the anchor node. Move to next node
  // relative anchor node within the context subtree if asked.
  if (mFlags != eWalkContextTree) {
    // eWalkCache flag presence indicates that the search is scoped to the
    // anchor (no ARIA owns stuff).
    if (mFlags & eWalkCache) {
      return nullptr;
    }
    return Next();
  }

  nsINode* contextNode = mContext->GetNode();
  while (mAnchorNode != contextNode) {
    nsINode* parentNode = mAnchorNode->GetFlattenedTreeParent();
    if (!parentNode || !parentNode->IsElement())
      return nullptr;

    nsIContent* parent = parentNode->AsElement();
    top = PushState(parent);
    if (top->Seek(mAnchorNode)) {
      mAnchorNode = parent;
      return Next();
    }

    // XXX We really should never get here, it means we're trying to find an
    // accessible for a dom node where iterating over its parent's children
    // doesn't return it. However this sometimes happens when we're asked for
    // the nearest accessible to place holder content which we ignore.
    mAnchorNode = parent;
  }

  return Next();
}

dom::AllChildrenIterator*
TreeWalker::PopState()
{
  size_t length = mStateStack.Length();
  mStateStack.RemoveElementAt(length - 1);
  return mStateStack.IsEmpty() ? nullptr : &mStateStack[mStateStack.Length() - 1];
}
