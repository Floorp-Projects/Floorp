/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TreeWalker.h"

#include "Accessible.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// WalkState
////////////////////////////////////////////////////////////////////////////////

namespace mozilla {
namespace a11y {

struct WalkState
{
  WalkState(nsIContent *aContent) :
    content(aContent), childIdx(0), prevState(nullptr) {}

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsINodeList> childList;
  uint32_t childIdx;
  WalkState *prevState;
};

} // namespace a11y
} // namespace mozilla

////////////////////////////////////////////////////////////////////////////////
// TreeWalker
////////////////////////////////////////////////////////////////////////////////

TreeWalker::
  TreeWalker(Accessible* aContext, nsIContent* aContent, uint32_t aFlags) :
  mDoc(aContext->Document()), mContext(aContext),
  mFlags(aFlags), mState(nullptr)
{
  NS_ASSERTION(aContent, "No node for the accessible tree walker!");

  if (aContent)
    mState = new WalkState(aContent);

  mChildFilter = mContext->CanHaveAnonChildren() ?
    nsIContent::eAllChildren : nsIContent::eAllButXBL;

  mChildFilter |= nsIContent::eSkipPlaceholderContent;

  MOZ_COUNT_CTOR(TreeWalker);
}

TreeWalker::~TreeWalker()
{
  // Clear state stack from memory
  while (mState)
    PopState();

  MOZ_COUNT_DTOR(TreeWalker);
}

////////////////////////////////////////////////////////////////////////////////
// TreeWalker: private

Accessible*
TreeWalker::NextChildInternal(bool aNoWalkUp)
{
  if (!mState || !mState->content)
    return nullptr;

  if (!mState->childList)
    mState->childList = mState->content->GetChildren(mChildFilter);

  uint32_t length = 0;
  if (mState->childList)
    mState->childList->GetLength(&length);

  while (mState->childIdx < length) {
    nsIContent* childNode = mState->childList->Item(mState->childIdx);
    mState->childIdx++;

    bool isSubtreeHidden = false;
    Accessible* accessible = mFlags & eWalkCache ?
      mDoc->GetAccessible(childNode) :
      GetAccService()->GetOrCreateAccessible(childNode, mContext,
                                             &isSubtreeHidden);

    if (accessible)
      return accessible;

    // Walk down into subtree to find accessibles.
    if (!isSubtreeHidden) {
      PushState(childNode);
      accessible = NextChildInternal(true);
      if (accessible)
        return accessible;
    }
  }

  // No more children, get back to the parent.
  nsIContent* anchorNode = mState->content;
  PopState();
  if (aNoWalkUp)
    return nullptr;

  if (mState)
    return NextChildInternal(false);

  // If we traversed the whole subtree of the anchor node. Move to next node
  // relative anchor node within the context subtree if possible.
  if (mFlags != eWalkContextTree)
    return nullptr;

  while (anchorNode != mContext->GetNode()) {
    nsINode* parentNode = anchorNode->GetFlattenedTreeParent();
    if (!parentNode || !parentNode->IsElement())
      return nullptr;

    PushState(parentNode->AsElement());
    mState->childList = mState->content->GetChildren(mChildFilter);
    length = 0;
    if (mState->childList)
      mState->childList->GetLength(&length);

    while (mState->childIdx < length) {
      nsIContent* childNode = mState->childList->Item(mState->childIdx);
      mState->childIdx++;
      if (childNode == anchorNode)
        return NextChildInternal(false);
    }
    PopState();

    anchorNode = parentNode->AsElement();
  }

  return nullptr;
}

void
TreeWalker::PopState()
{
  WalkState* prevToLastState = mState->prevState;
  delete mState;
  mState = prevToLastState;
}

void
TreeWalker::PushState(nsIContent* aContent)
{
  WalkState* nextToLastState = new WalkState(aContent);
  nextToLastState->prevState = mState;
  mState = nextToLastState;
}
