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

#include "nsQuoteList.h"
#include "nsIDOMCharacterData.h"
#include "nsCSSPseudoElements.h"
#include "nsLayoutUtils.h"
#include "nsReadableUtils.h"

const nsString*
nsQuoteListNode::Text()
{
  NS_ASSERTION(mType == eStyleContentType_OpenQuote ||
               mType == eStyleContentType_CloseQuote,
               "should only be called when mText should be non-null");
  const nsStyleQuotes* styleQuotes = mPseudoFrame->GetStyleQuotes();
  PRInt32 quotesCount = styleQuotes->QuotesCount(); // 0 if 'quotes:none'
  PRInt32 quoteDepth = Depth();

  // Reuse the last pair when the depth is greater than the number of
  // pairs of quotes.  (Also make 'quotes: none' and close-quote from
  // a depth of 0 equivalent for the next test.)
  if (quoteDepth >= quotesCount)
    quoteDepth = quotesCount - 1;

  const nsString *result;
  if (quoteDepth == -1) {
    // close-quote from a depth of 0 or 'quotes: none' (we want a node
    // with the empty string so dynamic changes are easier to handle)
    result = & EmptyString();
  } else {
    result = eStyleContentType_OpenQuote == mType
               ? styleQuotes->OpenQuoteAt(quoteDepth)
               : styleQuotes->CloseQuoteAt(quoteDepth);
  }
  return result;
}

void
nsQuoteList::Calc(nsQuoteListNode* aNode)
{
  if (aNode == mFirstNode) {
    aNode->mDepthBefore = 0;
  } else {
    aNode->mDepthBefore = Prev(aNode)->DepthAfter();
  }
}

void
nsQuoteList::RecalcAll()
{
  nsQuoteListNode *node = mFirstNode;
  if (!node)
    return;

  do {
    PRInt32 oldDepth = node->mDepthBefore;
    Calc(node);

    if (node->mDepthBefore != oldDepth && node->mText)
      node->mText->SetData(*node->Text());

    // Next node
    node = Next(node);
  } while (node != mFirstNode);
}

#ifdef DEBUG
void
nsQuoteList::PrintChain()
{
  printf("Chain: \n");
  if (!mFirstNode) {
    return;
  }
  nsQuoteListNode* node = mFirstNode;
  do {
    printf("  %p %d - ", node, node->mDepthBefore);
    switch(node->mType) {
        case (eStyleContentType_OpenQuote):
          printf("open");
          break;
        case (eStyleContentType_NoOpenQuote):
          printf("noOpen");
          break;
        case (eStyleContentType_CloseQuote):
          printf("close");
          break;
        case (eStyleContentType_NoCloseQuote):
          printf("noClose");
          break;
        default:
          printf("unknown!!!");
    }
    printf(" %d - %d,", node->Depth(), node->DepthAfter());
    if (node->mText) {
      nsAutoString data;
      node->mText->GetData(data);
      printf(" \"%s\",", NS_ConvertUCS2toUTF8(data).get());
    }
    printf("\n");
    node = Next(node);
  } while (node != mFirstNode);
}
#endif

void
nsQuoteList::Clear()
{
  //Delete entire list
  if (!mFirstNode)
    return;
  for (nsQuoteListNode *node = Next(mFirstNode); node != mFirstNode;
       node = Next(mFirstNode))
  {
    Remove(node);
    delete node;
  }
  delete mFirstNode;

  mFirstNode = nsnull;
  mSize = 0;
}

PRBool
nsQuoteList::DestroyNodesFor(nsIFrame* aFrame)
{
  if (!mFirstNode)
    return PR_FALSE; // list empty
  nsQuoteListNode* node;
  PRBool destroyed = PR_FALSE;
  while (mFirstNode->mPseudoFrame == aFrame) {
    destroyed = PR_TRUE;
    node = Next(mFirstNode);
    if (node == mFirstNode) { // Last link
      mFirstNode = nsnull;
      delete node;
      return PR_TRUE;
    }
    else {
      Remove(mFirstNode);
      delete mFirstNode;
      mFirstNode = node;
    }
  }
  node = Next(mFirstNode);
  while (node != mFirstNode) {
    if (node->mPseudoFrame == aFrame) {
      destroyed = PR_TRUE;
      nsQuoteListNode *nextNode = Next(node);
      Remove(node);
      delete node;
      node = nextNode;
    } else {
      node = Next(node);
    }
  }
  return destroyed;
}

// return -1 for ::before and +1 for ::after
inline PRBool PseudoCompareType(nsIFrame *aFrame)
{
  nsIAtom *pseudo = aFrame->GetStyleContext()->GetPseudoType();
  NS_ASSERTION(pseudo == nsCSSPseudoElements::before ||
               pseudo == nsCSSPseudoElements::after,
               "not a pseudo-element frame");
  return pseudo == nsCSSPseudoElements::before ? -1 : 1;
}

static PRBool NodeAfter(nsQuoteListNode* aNode1, nsQuoteListNode* aNode2)
{
  nsIFrame *frame1 = aNode1->mPseudoFrame;
  nsIFrame *frame2 = aNode2->mPseudoFrame;
  if (frame1 == frame2) {
    NS_ASSERTION(aNode2->mContentIndex != aNode1->mContentIndex, "identical");
    return aNode1->mContentIndex > aNode2->mContentIndex;
  }
  PRInt32 pseudoType1 = PseudoCompareType(frame1);
  PRInt32 pseudoType2 = PseudoCompareType(frame2);
  nsIContent *content1 = frame1->GetContent();
  nsIContent *content2 = frame2->GetContent();
  if (content1 == content2) {
    NS_ASSERTION(pseudoType1 != pseudoType2, "identical");
    return pseudoType1 == 1;
  }
  PRInt32 cmp = nsLayoutUtils::DoCompareTreePosition(content1, content2,
                                                     pseudoType1, -pseudoType2);
  NS_ASSERTION(cmp != 0, "same content, different frames");
  return cmp > 0;
}

void
nsQuoteList::Insert(nsQuoteListNode* aNode)
{
  if (mFirstNode) {
    // Check for append.
    if (NodeAfter(aNode, Prev(mFirstNode))) {
      PR_INSERT_BEFORE(aNode, mFirstNode);
    }
    else {
      // Binary search.

      // the range of indices at which |aNode| could end up.
      PRUint32 first = 0, last = mSize - 1;

      // A cursor to avoid walking more than the length of the list.
      nsQuoteListNode *curNode = Prev(mFirstNode);
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

  Calc(aNode);
}
