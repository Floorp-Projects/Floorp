/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#include "nsGenConList.h"
#include "nsLayoutUtils.h"

void
nsGenConList::Clear()
{
  //Delete entire list
  if (!mFirstNode)
    return;
  for (nsGenConNode *node = Next(mFirstNode); node != mFirstNode;
       node = Next(mFirstNode))
  {
    Remove(node);
    delete node;
  }
  delete mFirstNode;

  mFirstNode = nullptr;
  mSize = 0;
}

bool
nsGenConList::DestroyNodesFor(nsIFrame* aFrame)
{
  if (!mFirstNode)
    return false; // list empty
  nsGenConNode* node;
  bool destroyed = false;
  while (mFirstNode->mPseudoFrame == aFrame) {
    destroyed = true;
    node = Next(mFirstNode);
    bool isLastNode = node == mFirstNode; // before they're dangling
    Remove(mFirstNode);
    delete mFirstNode;
    if (isLastNode) {
      mFirstNode = nullptr;
      return true;
    }
    else {
      mFirstNode = node;
    }
  }
  node = Next(mFirstNode);
  while (node != mFirstNode) {
    if (node->mPseudoFrame == aFrame) {
      destroyed = true;
      nsGenConNode *nextNode = Next(node);
      Remove(node);
      delete node;
      node = nextNode;
    } else {
      node = Next(node);
    }
  }
  return destroyed;
}

/**
 * Compute the type of the pseudo and the content for the pseudo that
 * we'll use for comparison purposes.
 * @param aContent the content to use is stored here; it's the element
 * that generated the ::before or ::after content, or (if not for generated
 * content), the frame's own element
 * @return -1 for ::before, +1 for ::after, and 0 otherwise.
 */
inline PRInt32 PseudoCompareType(nsIFrame* aFrame, nsIContent** aContent)
{
  nsIAtom *pseudo = aFrame->GetStyleContext()->GetPseudo();
  if (pseudo == nsCSSPseudoElements::before) {
    *aContent = aFrame->GetContent()->GetParent();
    return -1;
  }
  if (pseudo == nsCSSPseudoElements::after) {
    *aContent = aFrame->GetContent()->GetParent();
    return 1;
  }
  *aContent = aFrame->GetContent();
  return 0;
}

/* static */ bool
nsGenConList::NodeAfter(const nsGenConNode* aNode1, const nsGenConNode* aNode2)
{
  nsIFrame *frame1 = aNode1->mPseudoFrame;
  nsIFrame *frame2 = aNode2->mPseudoFrame;
  if (frame1 == frame2) {
    NS_ASSERTION(aNode2->mContentIndex != aNode1->mContentIndex, "identical");
    return aNode1->mContentIndex > aNode2->mContentIndex;
  }
  nsIContent *content1;
  nsIContent *content2;
  PRInt32 pseudoType1 = PseudoCompareType(frame1, &content1);
  PRInt32 pseudoType2 = PseudoCompareType(frame2, &content2);
  if (pseudoType1 == 0 || pseudoType2 == 0) {
    if (content1 == content2) {
      NS_ASSERTION(pseudoType1 != pseudoType2, "identical");
      return pseudoType2 == 0;
    }
    // We want to treat an element as coming before its :before (preorder
    // traversal), so treating both as :before now works.
    if (pseudoType1 == 0) pseudoType1 = -1;
    if (pseudoType2 == 0) pseudoType2 = -1;
  } else {
    if (content1 == content2) {
      NS_ASSERTION(pseudoType1 != pseudoType2, "identical");
      return pseudoType1 == 1;
    }
  }
  // XXX Switch to the frame version of DoCompareTreePosition?
  PRInt32 cmp = nsLayoutUtils::DoCompareTreePosition(content1, content2,
                                                     pseudoType1, -pseudoType2);
  NS_ASSERTION(cmp != 0, "same content, different frames");
  return cmp > 0;
}

void
nsGenConList::Insert(nsGenConNode* aNode)
{
  if (mFirstNode) {
    // Check for append.
    if (NodeAfter(aNode, Prev(mFirstNode))) {
      PR_INSERT_BEFORE(aNode, mFirstNode);
    }
    else {
      // Binary search.

      // the range of indices at which |aNode| could end up.
      // (We already know it can't be at index mSize.)
      PRUint32 first = 0, last = mSize - 1;

      // A cursor to avoid walking more than the length of the list.
      nsGenConNode *curNode = Prev(mFirstNode);
      PRUint32 curIndex = mSize - 1;

      while (first != last) {
        PRUint32 test = (first + last) / 2;
        if (last == curIndex) {
          for ( ; curIndex != test; --curIndex)
            curNode = Prev(curNode);
        } else {
          for ( ; curIndex != test; ++curIndex)
            curNode = Next(curNode);
        }

        if (NodeAfter(aNode, curNode)) {
          first = test + 1;
          // if we exit the loop, we need curNode to be right
          ++curIndex;
          curNode = Next(curNode);
        } else {
          last = test;
        }
      }
      PR_INSERT_BEFORE(aNode, curNode);
      if (curNode == mFirstNode) {
        mFirstNode = aNode;
      }
    }
  }
  else {
    // initialize list with first node
    PR_INIT_CLIST(aNode);
    mFirstNode = aNode;
  }
  ++mSize;

  NS_ASSERTION(aNode == mFirstNode || NodeAfter(aNode, Prev(aNode)),
               "sorting error");
  NS_ASSERTION(IsLast(aNode) || NodeAfter(Next(aNode), aNode),
               "sorting error");
}
