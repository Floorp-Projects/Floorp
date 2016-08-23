/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#include "nsGenConList.h"
#include "nsLayoutUtils.h"
#include "nsIContent.h"

void
nsGenConList::Clear()
{
  //Delete entire list
  if (!mFirstNode)
    return;

  mNodes.Clear();
  for (nsGenConNode *node = Next(mFirstNode); node != mFirstNode;
       node = Next(mFirstNode))
  {
    Destroy(node);
  }
  delete mFirstNode;

  mFirstNode = nullptr;
  mSize = 0;
}

bool
nsGenConList::DestroyNodesFor(nsIFrame* aFrame)
{
  // This algorithm relies on the invariant that nodes of a frame are
  // put contiguously in the linked list. This is guaranteed because
  // each frame is mapped to only one (nsIContent, pseudoType) pair,
  // and the nodes in the linked list are put in the tree order based
  // on that pair and offset inside frame.
  nsGenConNode* node = mNodes.GetAndRemove(aFrame).valueOr(nullptr);
  if (!node) {
    return false;
  }
  MOZ_ASSERT(node->mPseudoFrame == aFrame);
  // Clear following nodes first as they must not be mFirstNode.
  // If mFirstNode refers to a node of this frame, it should be the
  // one retrieved from mNodes.
  for (nsGenConNode* curNode = Next(node);
       curNode->mPseudoFrame == aFrame && curNode != node;
       /* curNode updated in loop body */) {
    MOZ_ASSERT(curNode != mFirstNode);
    nsGenConNode* nextNode = Next(curNode);
    Destroy(curNode);
    curNode = nextNode;
  }
  if (node == mFirstNode) {
    nsGenConNode* nextNode = Next(mFirstNode);
    mFirstNode = nextNode == mFirstNode ? nullptr : nextNode;
  }
  Destroy(node);
  return true;
}

/**
 * Compute the type of the pseudo and the content for the pseudo that
 * we'll use for comparison purposes.
 * @param aContent the content to use is stored here; it's the element
 * that generated the ::before or ::after content, or (if not for generated
 * content), the frame's own element
 * @return -1 for ::before, +1 for ::after, and 0 otherwise.
 */
inline int32_t PseudoCompareType(nsIFrame* aFrame, nsIContent** aContent)
{
  nsIAtom *pseudo = aFrame->StyleContext()->GetPseudo();
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
  int32_t pseudoType1 = PseudoCompareType(frame1, &content1);
  int32_t pseudoType2 = PseudoCompareType(frame2, &content2);
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
  int32_t cmp = nsLayoutUtils::DoCompareTreePosition(content1, content2,
                                                     pseudoType1, -pseudoType2);
  MOZ_ASSERT(cmp != 0, "same content, different frames");
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
      uint32_t first = 0, last = mSize - 1;

      // A cursor to avoid walking more than the length of the list.
      nsGenConNode *curNode = Prev(mFirstNode);
      uint32_t curIndex = mSize - 1;

      while (first != last) {
        uint32_t test = (first + last) / 2;
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

  // Set the mapping only if it is the first node of the frame.
  // The DEBUG blocks below are for ensuring the invariant required by
  // nsGenConList::DestroyNodesFor. See comment there.
  if (aNode == mFirstNode ||
      Prev(aNode)->mPseudoFrame != aNode->mPseudoFrame) {
#ifdef DEBUG
    if (nsGenConNode* oldFrameFirstNode = mNodes.Get(aNode->mPseudoFrame)) {
      MOZ_ASSERT(Next(aNode) == oldFrameFirstNode,
                 "oldFrameFirstNode should now be immediately after "
                 "the newly-inserted one.");
    } else {
      // If the node is not the only node in the list.
      nsGenConNode* nextNode = Next(aNode);
      if (nextNode != aNode) {
        MOZ_ASSERT(nextNode->mPseudoFrame != aNode->mPseudoFrame,
                   "There shouldn't exist any node for this frame.");
        // If the node is neither the first nor the last node
        if (aNode != mFirstNode && nextNode != mFirstNode) {
          MOZ_ASSERT(Prev(aNode)->mPseudoFrame != nextNode->mPseudoFrame,
                     "New node should not break contiguity of nodes of "
                     "the same frame.");
        }
      }
    }
#endif
    mNodes.Put(aNode->mPseudoFrame, aNode);
  } else {
#ifdef DEBUG
    nsGenConNode* frameFirstNode = mNodes.Get(aNode->mPseudoFrame);
    MOZ_ASSERT(frameFirstNode, "There should exist node map for the frame.");
    for (nsGenConNode* curNode = Prev(aNode);
         curNode != frameFirstNode; curNode = Prev(curNode)) {
      MOZ_ASSERT(curNode->mPseudoFrame == aNode->mPseudoFrame,
                 "Every node between frameFirstNode and the new node inserted "
                 "should refer to the same frame.");
      MOZ_ASSERT(curNode != mFirstNode, "The newly-inserted node should be in "
                 "a contiguous run after frameFirstNode, thus frameFirstNode "
                 "should be reached before mFirstNode.");
    }
#endif
  }

  NS_ASSERTION(aNode == mFirstNode || NodeAfter(aNode, Prev(aNode)),
               "sorting error");
  NS_ASSERTION(IsLast(aNode) || NodeAfter(Next(aNode), aNode),
               "sorting error");
}
