/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFilteredContentIterator.h"
#include "nsIContentIterator.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsIEnumerator.h"

#include "nsTextServicesDocument.h"

#include "nsIDOMNode.h"
#include "nsIDOMRange.h"

static NS_DEFINE_CID(kCContentIteratorCID,    NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCPreContentIteratorCID, NS_PRECONTENTITERATOR_CID);
static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);

//------------------------------------------------------------
nsFilteredContentIterator::nsFilteredContentIterator(nsITextServicesFilter* aFilter) :
  mFilter(aFilter),
  mDidSkip(PR_FALSE),
  mIsOutOfRange(PR_FALSE),
  mDirection(eDirNotSet)
{
  nsComponentManager::CreateInstance(kCContentIteratorCID,
                                     nsnull,
                                     NS_GET_IID(nsIContentIterator),
                                     getter_AddRefs(mIterator));

  nsComponentManager::CreateInstance(kCPreContentIteratorCID,
                                     nsnull,
                                     NS_GET_IID(nsIContentIterator),
                                     getter_AddRefs(mPreIterator));

}

//------------------------------------------------------------
nsFilteredContentIterator::~nsFilteredContentIterator()
{
  mIterator = nsnull;
}

//------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsFilteredContentIterator, nsIContentIterator);

//------------------------------------------------------------
NS_IMETHODIMP 
nsFilteredContentIterator::Init(nsIContent* aRoot)
{
  NS_ENSURE_TRUE(mPreIterator, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);
  mIsOutOfRange    = PR_FALSE;
  mDirection       = eForward;
  mCurrentIterator = mPreIterator;

  nsresult rv = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              getter_AddRefs(mRange));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMRange> domRange(do_QueryInterface(mRange));
  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(aRoot));
  if (domRange && domNode) {
    domRange->SelectNode(domNode);
  }

  rv = mPreIterator->Init(domRange);
  NS_ENSURE_SUCCESS(rv, rv);
  return mIterator->Init(domRange);
}

//------------------------------------------------------------
NS_IMETHODIMP 
nsFilteredContentIterator::Init(nsIDOMRange* aRange)
{
  NS_ENSURE_TRUE(mPreIterator, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aRange);
  mIsOutOfRange    = PR_FALSE;
  mDirection       = eForward;
  mCurrentIterator = mPreIterator;

  nsCOMPtr<nsIDOMRange> domRange;
  nsresult rv = aRange->CloneRange(getter_AddRefs(domRange));
  NS_ENSURE_SUCCESS(rv, rv);
  mRange = do_QueryInterface(domRange);

  rv = mPreIterator->Init(domRange);
  NS_ENSURE_SUCCESS(rv, rv);
  return mIterator->Init(domRange);
}

//------------------------------------------------------------
nsresult 
nsFilteredContentIterator::SwitchDirections(PRPackedBool aChangeToForward)
{
  nsCOMPtr<nsIContent> node;
  mCurrentIterator->CurrentNode(getter_AddRefs(node));

  if (aChangeToForward) {
    mCurrentIterator = mPreIterator;
    mDirection       = eForward;
  } else {
    mCurrentIterator = mIterator;
    mDirection       = eBackward;
  }

  if (node) {
    nsresult rv = mCurrentIterator->PositionAt(node);
    if (NS_FAILED(rv)) {
      mIsOutOfRange = PR_TRUE;
      return rv;
    }
  }
  return NS_OK;
}

//------------------------------------------------------------
NS_IMETHODIMP 
nsFilteredContentIterator::First()
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eForward) {
    mCurrentIterator = mPreIterator;
    mDirection       = eForward;
    mIsOutOfRange    = PR_FALSE;
  }

  nsresult rv = mCurrentIterator->First();
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_ENUMERATOR_FALSE != mCurrentIterator->IsDone()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> currentContent;
  rv = mCurrentIterator->CurrentNode(getter_AddRefs(currentContent));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(currentContent));

  PRPackedBool didCross;
  CheckAdvNode(node, didCross, eForward);

  return NS_OK;
}

//------------------------------------------------------------
NS_IMETHODIMP 
nsFilteredContentIterator::Last()
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);

  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eBackward) {
    mCurrentIterator = mIterator;
    mDirection       = eBackward;
    mIsOutOfRange    = PR_FALSE;
  }

  nsresult rv = mCurrentIterator->Last();
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_ENUMERATOR_FALSE != mCurrentIterator->IsDone()) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> currentContent;
  rv = mCurrentIterator->CurrentNode(getter_AddRefs(currentContent));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(currentContent));

  PRPackedBool didCross;
  CheckAdvNode(node, didCross, eBackward);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ContentToParentOffset: returns the content node's parent and offset.
//
static void
ContentToParentOffset(nsIContent *aContent, nsIDOMNode **aParent, PRInt32 *aOffset)
{
  if (!aParent || !aOffset)
    return;

  *aParent = nsnull;
  *aOffset  = 0;

  if (!aContent)
    return;

  nsCOMPtr<nsIContent> parent;
  nsresult rv = aContent->GetParent(*getter_AddRefs(parent));

  if (NS_FAILED(rv) || !parent)
    return;

  rv = parent->IndexOf(aContent, *aOffset);

  if (NS_FAILED(rv))
    return;

  CallQueryInterface(parent, aParent);
}

///////////////////////////////////////////////////////////////////////////
// ContentIsInTraversalRange: returns true if content is visited during
// the traversal of the range in the specified mode.
//
static PRBool
ContentIsInTraversalRange(nsIContent *aContent,   PRBool aIsPreMode,
                          nsIDOMNode *aStartNode, PRInt32 aStartOffset,
                          nsIDOMNode *aEndNode,   PRInt32 aEndOffset)
{
  if (!aStartNode || !aEndNode || !aContent)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 indx = 0;

  ContentToParentOffset(aContent, getter_AddRefs(parentNode), &indx);

  if (!parentNode)
    return PR_FALSE;

  if (!aIsPreMode)
    ++indx;

  PRInt32 startRes;
  PRInt32 endRes;
  nsresult rv = nsTextServicesDocument::ComparePoints(aStartNode, aStartOffset, parentNode, indx, &startRes);
  if (NS_FAILED(rv)) return PR_FALSE;

  rv = nsTextServicesDocument::ComparePoints(aEndNode,   aEndOffset,   parentNode, indx,  &endRes);
  if (NS_FAILED(rv)) return PR_FALSE;

  return (startRes <= 0) && (endRes >= 0);
}

static PRBool
ContentIsInTraversalRange(nsIDOMNSRange *aRange, nsIDOMNode* aNextNode, PRBool aIsPreMode)
{
  nsCOMPtr<nsIContent>  content(do_QueryInterface(aNextNode));
  nsCOMPtr<nsIDOMRange> range(do_QueryInterface(aRange));
  if (!content || !range)
    return PR_FALSE;



  nsCOMPtr<nsIDOMNode> sNode;
  nsCOMPtr<nsIDOMNode> eNode;
  PRInt32 sOffset;
  PRInt32 eOffset;
  range->GetStartContainer(getter_AddRefs(sNode));
  range->GetStartOffset(&sOffset);
  range->GetEndContainer(getter_AddRefs(eNode));
  range->GetEndOffset(&eOffset);
  return ContentIsInTraversalRange(content, aIsPreMode, sNode, sOffset, eNode, eOffset);
}

//------------------------------------------------------------
// Helper function to advance to the next or previous node
nsresult 
nsFilteredContentIterator::AdvanceNode(nsIDOMNode* aNode, nsIDOMNode*& aNewNode, eDirectionType aDir)
{
  nsCOMPtr<nsIDOMNode> nextNode;
  if (aDir == eForward) {
    aNode->GetNextSibling(getter_AddRefs(nextNode));
  } else {
    aNode->GetPreviousSibling(getter_AddRefs(nextNode));
  }

  if (nextNode) {
    // If we got here, that means we found the nxt/prv node
    // make sure it is in our DOMRange
    PRBool intersects = ContentIsInTraversalRange(mRange, nextNode, aDir == eForward);
    if (intersects) {
      aNewNode = nextNode;
      NS_ADDREF(aNewNode);
      return NS_OK;
    }
  } else {
    // The next node was null so we need to walk up the parent(s)
    nsCOMPtr<nsIDOMNode> parent;
    aNode->GetParentNode(getter_AddRefs(parent));
    NS_ASSERTION(parent, "parent can't be NULL");

    // Make sure the parent is in the DOMRange before going further
    PRBool intersects = ContentIsInTraversalRange(mRange, nextNode, aDir == eForward);
    if (intersects) {
      // Now find the nxt/prv node after/before this node
      nsresult rv = AdvanceNode(parent, aNewNode, aDir);
      if (NS_SUCCEEDED(rv) && aNewNode) {
        return NS_OK;
      }
    }
  }

  // if we get here it pretty much means 
  // we went out of the DOM Range
  mIsOutOfRange = PR_TRUE;

  return NS_ERROR_FAILURE;
}

//------------------------------------------------------------
// Helper function to see if the next/prev node should be skipped
void
nsFilteredContentIterator::CheckAdvNode(nsIDOMNode* aNode, PRPackedBool& aDidSkip, eDirectionType aDir)
{
  aDidSkip      = PR_FALSE;
  mIsOutOfRange = PR_FALSE;

  if (aNode && mFilter) {
    nsCOMPtr<nsIDOMNode> currentNode = aNode;
    PRBool skipIt;
    while (1) {
      nsresult rv = mFilter->Skip(aNode, &skipIt);
      if (NS_SUCCEEDED(rv) && skipIt) {
        aDidSkip = PR_TRUE;
        // Get the next/prev node and then 
        // see if we should skip that
        nsCOMPtr<nsIDOMNode> advNode;
        rv = AdvanceNode(aNode, *getter_AddRefs(advNode), aDir);
        if (NS_SUCCEEDED(rv) && advNode) {
          aNode = advNode;
        } else {
          return; // fell out of range
        }
      } else {
        if (aNode != currentNode) {
          nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
          mCurrentIterator->PositionAt(content);
        }
        return; // found something
      }
    }
  }
}

NS_IMETHODIMP 
nsFilteredContentIterator::Next()
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);

  if (mIsOutOfRange) {
    return NS_OK;
  }
  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eForward) {
    nsresult rv = SwitchDirections(PR_TRUE);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }
  }

  nsresult rv = mCurrentIterator->Next();
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_ENUMERATOR_FALSE != mCurrentIterator->IsDone()) {
    return NS_OK;
  }

  // If we can't get the current node then 
  // don't check to see if we can skip it
  nsCOMPtr<nsIContent> currentContent;
  rv = mCurrentIterator->CurrentNode(getter_AddRefs(currentContent));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(currentContent));
    CheckAdvNode(node, mDidSkip, eForward);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsFilteredContentIterator::Prev()
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);

  if (mIsOutOfRange) {
    return NS_OK;
  }
  // If we are switching directions then
  // we need to switch how we process the nodes
  if (mDirection != eBackward) {
    nsresult rv = SwitchDirections(PR_FALSE);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }
  }

  nsresult rv = mCurrentIterator->Prev();
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_ENUMERATOR_FALSE != mCurrentIterator->IsDone()) {
    return NS_OK;
  }

  // If we can't get the current node then 
  // don't check to see if we can skip it
  nsCOMPtr<nsIContent> currentContent;
  rv = mCurrentIterator->CurrentNode(getter_AddRefs(currentContent));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(currentContent));
    CheckAdvNode(node, mDidSkip, eBackward);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsFilteredContentIterator::CurrentNode(nsIContent **aNode)
{
  if (mIsOutOfRange) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);
  return mCurrentIterator->CurrentNode(aNode);
}

NS_IMETHODIMP 
nsFilteredContentIterator::IsDone()
{
  if (mIsOutOfRange) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);
  return mCurrentIterator->IsDone();
}

NS_IMETHODIMP 
nsFilteredContentIterator::PositionAt(nsIContent* aCurNode)
{
  NS_ENSURE_TRUE(mCurrentIterator, NS_ERROR_FAILURE);
  mIsOutOfRange = PR_FALSE;
  return mCurrentIterator->PositionAt(aCurNode);
}
