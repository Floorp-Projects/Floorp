/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccTreeWalker.h"

#include "Accessible.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "nsINodeList.h"

////////////////////////////////////////////////////////////////////////////////
// WalkState
////////////////////////////////////////////////////////////////////////////////

struct WalkState
{
  WalkState(nsIContent *aContent) :
    content(aContent), childIdx(0), prevState(nullptr) {}

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsINodeList> childList;
  PRUint32 childIdx;
  WalkState *prevState;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccTreeWalker
////////////////////////////////////////////////////////////////////////////////

nsAccTreeWalker::
  nsAccTreeWalker(DocAccessible* aDoc, nsIContent* aContent,
                  bool aWalkAnonContent, bool aWalkCache) :
  mDoc(aDoc), mWalkCache(aWalkCache), mState(nullptr)
{
  NS_ASSERTION(aContent, "No node for the accessible tree walker!");

  if (aContent)
    mState = new WalkState(aContent);

  mChildFilter = aWalkAnonContent ? nsIContent::eAllChildren :
                                  nsIContent::eAllButXBL;

  mChildFilter |= nsIContent::eSkipPlaceholderContent;

  MOZ_COUNT_CTOR(nsAccTreeWalker);
}

nsAccTreeWalker::~nsAccTreeWalker()
{
  // Clear state stack from memory
  while (mState)
    PopState();

  MOZ_COUNT_DTOR(nsAccTreeWalker);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccTreeWalker: private

Accessible*
nsAccTreeWalker::NextChildInternal(bool aNoWalkUp)
{
  if (!mState || !mState->content)
    return nullptr;

  if (!mState->childList)
    mState->childList = mState->content->GetChildren(mChildFilter);

  PRUint32 length = 0;
  if (mState->childList)
    mState->childList->GetLength(&length);

  while (mState->childIdx < length) {
    nsIContent* childNode = mState->childList->GetNodeAt(mState->childIdx);
    mState->childIdx++;

    bool isSubtreeHidden = false;
    Accessible* accessible = mWalkCache ? mDoc->GetAccessible(childNode) :
      GetAccService()->GetOrCreateAccessible(childNode, mDoc, &isSubtreeHidden);

    if (accessible)
      return accessible;

    // Walk down into subtree to find accessibles.
    if (!isSubtreeHidden) {
      if (!PushState(childNode))
        break;

      accessible = NextChildInternal(true);
      if (accessible)
        return accessible;
    }
  }

  // No more children, get back to the parent.
  PopState();

  return aNoWalkUp ? nullptr : NextChildInternal(false);
}

void
nsAccTreeWalker::PopState()
{
  WalkState* prevToLastState = mState->prevState;
  delete mState;
  mState = prevToLastState;
}

bool
nsAccTreeWalker::PushState(nsIContent* aContent)
{
  WalkState* nextToLastState = new WalkState(aContent);
  if (!nextToLastState)
    return false;

  nextToLastState->prevState = mState;
  mState = nextToLastState;

  return true;
}
