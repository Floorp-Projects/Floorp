/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Esben Mose Hansen.
 *
 * Contributor(s):
 *   Esben Mose Hansen <esben@oek.dk> (original author)
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  mFirstNode = nsnull;
  mSize = 0;
}

bool
nsGenConList::DestroyNodesFor(nsIFrame* aFrame)
{
  if (!mFirstNode)
    return PR_FALSE; // list empty
  nsGenConNode* node;
  bool destroyed = false;
  while (mFirstNode->mPseudoFrame == aFrame) {
    destroyed = PR_TRUE;
    node = Next(mFirstNode);
    bool isLastNode = node == mFirstNode; // before they're dangling
    Remove(mFirstNode);
    delete mFirstNode;
    if (isLastNode) {
      mFirstNode = nsnull;
      return PR_TRUE;
    }
    else {
      mFirstNode = node;
    }
  }
  node = Next(mFirstNode);
  while (node != mFirstNode) {
    if (node->mPseudoFrame == aFrame) {
      destroyed = PR_TRUE;
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
