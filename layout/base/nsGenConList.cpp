/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#include "nsGenConList.h"
#include "nsLayoutUtils.h"
#include "nsIContent.h"

void nsGenConList::Clear() {
  // Delete entire list.
  mNodes.Clear();
  while (nsGenConNode* node = mList.popFirst()) {
    delete node;
  }
  mSize = 0;
  mLastInserted = nullptr;
}

bool nsGenConList::DestroyNodesFor(nsIFrame* aFrame) {
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

  while (node && node->mPseudoFrame == aFrame) {
    nsGenConNode* nextNode = Next(node);
    Destroy(node);
    node = nextNode;
  }

  // Modification of the list invalidates the cached pointer.
  mLastInserted = nullptr;

  return true;
}

/**
 * Compute the type of the pseudo and the content for the pseudo that
 * we'll use for comparison purposes.
 * @param aContent the content to use is stored here; it's the element
 * that generated the pseudo, or (if not for generated content), the frame's
 * own element
 * @return -2 for ::marker, -1 for ::before, +1 for ::after, and 0 otherwise.
 */
inline int32_t PseudoCompareType(nsIFrame* aFrame, nsIContent** aContent) {
  auto pseudo = aFrame->Style()->GetPseudoType();
  if (pseudo == mozilla::PseudoStyleType::marker) {
    *aContent = aFrame->GetContent()->GetParent();
    return -2;
  }
  if (pseudo == mozilla::PseudoStyleType::before) {
    *aContent = aFrame->GetContent()->GetParent();
    return -1;
  }
  if (pseudo == mozilla::PseudoStyleType::after) {
    *aContent = aFrame->GetContent()->GetParent();
    return 1;
  }
  *aContent = aFrame->GetContent();
  return 0;
}

#ifdef DEBUG
static bool IsXBLInvolved(nsIContent* aContent1, nsIContent* aContent2) {
  auto* ancestor = nsContentUtils::GetCommonAncestor(aContent1, aContent2);
  return ancestor && ancestor->IsElement() &&
         ancestor->AsElement()->GetXBLBinding();
}
#endif

/* static */
bool nsGenConList::NodeAfter(const nsGenConNode* aNode1,
                             const nsGenConNode* aNode2) {
  nsIFrame* frame1 = aNode1->mPseudoFrame;
  nsIFrame* frame2 = aNode2->mPseudoFrame;
  if (frame1 == frame2) {
    NS_ASSERTION(aNode2->mContentIndex != aNode1->mContentIndex, "identical");
    return aNode1->mContentIndex > aNode2->mContentIndex;
  }
  nsIContent* content1;
  nsIContent* content2;
  int32_t pseudoType1 = PseudoCompareType(frame1, &content1);
  int32_t pseudoType2 = PseudoCompareType(frame2, &content2);
  if (content1 == content2) {
    NS_ASSERTION(pseudoType1 != pseudoType2, "identical");
    if (pseudoType1 == 0 || pseudoType2 == 0) {
      return pseudoType2 == 0;
    }
    return pseudoType1 > pseudoType2;
  }

  // Two pseudo-elements of different elements, we want to treat them as if
  // they were normal elements and just use tree order.
  content1 = frame1->GetContent();
  content2 = frame2->GetContent();

  int32_t cmp = nsLayoutUtils::CompareTreePosition(content1, content2);
  // DoCompareTreePosition doesn't know about XBL anonymous content, and we
  // probably shouldn't bother teaching it about it.
  MOZ_ASSERT(cmp != 0 || IsXBLInvolved(content1, content2),
             "same content, different frames");
  return cmp > 0;
}

void nsGenConList::Insert(nsGenConNode* aNode) {
  // Check for append.
  if (mList.isEmpty() || NodeAfter(aNode, mList.getLast())) {
    mList.insertBack(aNode);
  } else if (mLastInserted && mLastInserted != mList.getLast() &&
             NodeAfter(aNode, mLastInserted) &&
             NodeAfter(Next(mLastInserted), aNode)) {
    // Fast path for inserting many consecutive nodes in one place
    mLastInserted->setNext(aNode);
  } else {
    // Binary search.

    // the range of indices at which |aNode| could end up.
    // (We already know it can't be at index mSize.)
    uint32_t first = 0, last = mSize - 1;

    // A cursor to avoid walking more than the length of the list.
    nsGenConNode* curNode = mList.getLast();
    uint32_t curIndex = mSize - 1;

    while (first != last) {
      uint32_t test = (first + last) / 2;
      if (last == curIndex) {
        for (; curIndex != test; --curIndex) curNode = Prev(curNode);
      } else {
        for (; curIndex != test; ++curIndex) curNode = Next(curNode);
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
    curNode->setPrevious(aNode);
  }
  ++mSize;

  mLastInserted = aNode;

  // Set the mapping only if it is the first node of the frame.
  // The DEBUG blocks below are for ensuring the invariant required by
  // nsGenConList::DestroyNodesFor. See comment there.
  if (IsFirst(aNode) || Prev(aNode)->mPseudoFrame != aNode->mPseudoFrame) {
#ifdef DEBUG
    if (nsGenConNode* oldFrameFirstNode = mNodes.Get(aNode->mPseudoFrame)) {
      MOZ_ASSERT(Next(aNode) == oldFrameFirstNode,
                 "oldFrameFirstNode should now be immediately after "
                 "the newly-inserted one.");
    } else {
      // If the node is not the only node in the list.
      if (!IsFirst(aNode) || !IsLast(aNode)) {
        nsGenConNode* nextNode = Next(aNode);
        MOZ_ASSERT(!nextNode || nextNode->mPseudoFrame != aNode->mPseudoFrame,
                   "There shouldn't exist any node for this frame.");
        // If the node is neither the first nor the last node
        if (!IsFirst(aNode) && !IsLast(aNode)) {
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
    for (nsGenConNode* curNode = Prev(aNode); curNode != frameFirstNode;
         curNode = Prev(curNode)) {
      MOZ_ASSERT(curNode->mPseudoFrame == aNode->mPseudoFrame,
                 "Every node between frameFirstNode and the new node inserted "
                 "should refer to the same frame.");
      MOZ_ASSERT(!IsFirst(curNode),
                 "The newly-inserted node should be in a contiguous run after "
                 "frameFirstNode, thus frameFirstNode should be reached before "
                 "the first node of mList.");
    }
#endif
  }

  NS_ASSERTION(IsFirst(aNode) || NodeAfter(aNode, Prev(aNode)),
               "sorting error");
  NS_ASSERTION(IsLast(aNode) || NodeAfter(Next(aNode), aNode), "sorting error");
}
