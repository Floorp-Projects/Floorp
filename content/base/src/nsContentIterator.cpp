/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * nsContentIterator.cpp: Implementation of the nsContentIterator object.
 * This ite
 */

#include "nsISupports.h"
#include "nsIDOMNodeList.h"
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsIContent.h"
#include "nsIDOMText.h"
#include "nsISupportsArray.h"
#include "nsIFocusTracker.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIComponentManager.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsVoidArray.h"
#include "nsContentUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// couple of utility static functs

///////////////////////////////////////////////////////////////////////////
// GetNumChildren: returns the number of things inside aNode. 
//
static PRUint32
GetNumChildren(nsIDOMNode *aNode) 
{
  if (!aNode)
    return 0;

  PRUint32 numChildren = 0;
  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (hasChildNodes)
  {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

    if (content)
      return content->GetChildCount();

    nsCOMPtr<nsIDOMNodeList>nodeList;
    aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (nodeList) 
      nodeList->GetLength(&numChildren);
  }

  return numChildren;
}

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
static nsCOMPtr<nsIDOMNode> 
GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;

  if (!aParent) 
    return resultNode;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aParent));

  if (content) {
    resultNode = do_QueryInterface(content->GetChildAt(aOffset));
  } else if (aParent) {
    PRBool hasChildNodes;
    aParent->HasChildNodes(&hasChildNodes);
    if (hasChildNodes)
    {
      nsCOMPtr<nsIDOMNodeList>nodeList;
      aParent->GetChildNodes(getter_AddRefs(nodeList));
      if (nodeList) 
        nodeList->Item(aOffset, getter_AddRefs(resultNode));
    }
  }
  
  return resultNode;
}
  
///////////////////////////////////////////////////////////////////////////
// ContentHasChildren: returns true if the content has children
//
static inline PRBool
ContentHasChildren(nsIContent *aContent)
{
  return aContent->GetChildCount() > 0;
}

///////////////////////////////////////////////////////////////////////////
// ContentToParentOffset: returns the content node's parent and offset.
//

static void
ContentToParentOffset(nsIContent *aContent, nsIDOMNode **aParent,
                      PRInt32 *aOffset)
{
  *aParent = nsnull;
  *aOffset  = 0;

  nsIContent* parent = aContent->GetParent();

  if (!parent)
    return;

  *aOffset = parent->IndexOf(aContent);

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

  nsCOMPtr<nsIDOMCharacterData> cData(do_QueryInterface(aContent));

  if (cData)
  {
    // If a chardata node contains an end point of the traversal range,
    // it is always in the traversal range.

    nsCOMPtr<nsIContent> startContent(do_QueryInterface(aStartNode));
    nsCOMPtr<nsIContent> endContent(do_QueryInterface(aEndNode));

    if (aContent == startContent || aContent == endContent)
      return PR_TRUE;
  }

  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 indx = 0;

  ContentToParentOffset(aContent, getter_AddRefs(parentNode), &indx);

  if (!parentNode)
    return PR_FALSE;

  if (!aIsPreMode)
    ++indx;

  return (ComparePoints(aStartNode, aStartOffset, parentNode, indx) <= 0) &&
         (ComparePoints(aEndNode,   aEndOffset,   parentNode, indx) >= 0);
}



/*
 *  A simple iterator class for traversing the content in "close tag" order
 */
class nsContentIterator : public nsIContentIterator //, public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS

  nsContentIterator();
  virtual ~nsContentIterator();

  // nsIContentIterator interface methods ------------------------------

  virtual nsresult Init(nsIContent* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void First();

  virtual void Last();
  
  virtual void Next();

  virtual void Prev();

  virtual nsIContent *GetCurrentNode();

  virtual PRBool IsDone();

  virtual nsresult PositionAt(nsIContent* aCurNode);

  // nsIEnumertor interface methods ------------------------------
  
  //NS_IMETHOD CurrentItem(nsISupports **aItem);

protected:

  nsIContent *GetDeepFirstChild(nsIContent *aRoot, nsVoidArray *aIndexes);
  nsIContent *GetDeepLastChild(nsIContent *aRoot, nsVoidArray *aIndexes);

  nsIContent *GetNextSibling(nsIContent *aNode, nsVoidArray *aIndexes);
  nsIContent *GetPrevSibling(nsIContent *aNode, nsVoidArray *aIndexes);

  nsIContent *NextNode(nsIContent *aNode, nsVoidArray *aIndexes);
  nsIContent *PrevNode(nsIContent *aNode, nsVoidArray *aIndexes);

  // WARNING: This function is expensive
  nsresult RebuildIndexStack();

  void MakeEmpty();
  
  nsCOMPtr<nsIContent> mCurNode;
  nsCOMPtr<nsIContent> mFirst;
  nsCOMPtr<nsIContent> mLast;
  nsCOMPtr<nsIContent> mCommonParent;

  // used by nsContentIterator to cache indices
  nsAutoVoidArray mIndexes;

  // used by nsSubtreeIterator to cache indices.  Why put them in the base class?
  // Because otherwise I have to duplicate the routines GetNextSibling etc across both classes,
  // with slight variations for caching.  Or alternately, create a base class for the cache
  // itself and have all the cache manipulation go through a vptr.
  // I think this is the best space and speed combo, even though it's ugly.
  PRInt32 mCachedIndex;
  // another note about mCachedIndex: why should the subtree iterator use a trivial cached index
  // instead of the mre robust array of indicies (which is what the basic content iterator uses)?
  // The reason is that subtree iterators do not do much transitioning between parents and children.
  // They tend to stay at the same level.  In fact, you can prove (though I won't attempt it here)
  // that they change levels at most n+m times, where n is the height of the parent heirarchy from the 
  // range start to the common ancestor, and m is the the height of the parent heirarchy from the 
  // range end to the common ancestor.  If we used the index array, we would pay the price up front
  // for n, and then pay the cost for m on the fly later on.  With the simple cache, we only "pay
  // as we go".  Either way, we call IndexOf() once for each change of level in the heirarchy.
  // Since a trivial index is much simpler, we use it for the subtree iterator.
  
  PRBool mIsDone;
  PRBool mPre;
  
private:

  // no copy's or assigns  FIX ME
  nsContentIterator(const nsContentIterator&);
  nsContentIterator& operator=(const nsContentIterator&);

};


/*
 *  A simple iterator class for traversing the content in "open tag" order
 */

class nsPreContentIterator : public nsContentIterator
{
public:
  nsPreContentIterator() { mPre = PR_TRUE; }
};



/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}


nsresult NS_NewPreContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsPreContentIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}


/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_ISUPPORTS1(nsContentIterator, nsIContentIterator)


/******************************************************
 * constructor/destructor
 ******************************************************/

nsContentIterator::nsContentIterator() :
  // don't need to explicitly initialize |nsCOMPtr|s, they will automatically be NULL
  mCachedIndex(0), mIsDone(PR_FALSE), mPre(PR_FALSE)
{
}


nsContentIterator::~nsContentIterator()
{
}


/******************************************************
 * Init routines
 ******************************************************/


nsresult
nsContentIterator::Init(nsIContent* aRoot)
{
  if (!aRoot) 
    return NS_ERROR_NULL_POINTER; 
  mIsDone = PR_FALSE;
  mIndexes.Clear();
  
  if (mPre)
  {
    mFirst = aRoot;
    mLast  = GetDeepLastChild(aRoot, nsnull);
  }
  else
  {
    mFirst = GetDeepFirstChild(aRoot, nsnull); 
    mLast  = aRoot;
  }

  mCommonParent = aRoot;
  mCurNode = mFirst;
  RebuildIndexStack();
  return NS_OK;
}


nsresult
nsContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  nsCOMPtr<nsIDOMNode> dN;

  nsCOMPtr<nsIContent> startCon;
  nsCOMPtr<nsIDOMNode> startDOM;
  nsCOMPtr<nsIContent> endCon;
  nsCOMPtr<nsIDOMNode> endDOM;
  PRInt32 startIndx;
  PRInt32 endIndx;
  
  mIsDone = PR_FALSE;

  // get common content parent
  if (NS_FAILED(aRange->GetCommonAncestorContainer(getter_AddRefs(dN))) || !dN)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(dN);

  // get the start node and offset, convert to nsIContent
  aRange->GetStartContainer(getter_AddRefs(startDOM));
  if (!startDOM) 
    return NS_ERROR_ILLEGAL_VALUE;
  startCon = do_QueryInterface(startDOM);
  if (!startCon) 
    return NS_ERROR_FAILURE;
  
  aRange->GetStartOffset(&startIndx);
  
  // get the end node and offset, convert to nsIContent
  aRange->GetEndContainer(getter_AddRefs(endDOM));
  if (!endDOM) 
    return NS_ERROR_ILLEGAL_VALUE;
  endCon = do_QueryInterface(endDOM);
  if (!endCon) 
    return NS_ERROR_FAILURE;

  aRange->GetEndOffset(&endIndx);
  
  nsCOMPtr<nsIDOMCharacterData> cData(do_QueryInterface(startCon));

  // short circuit when start node == end node
  if (startDOM == endDOM)
  {
    // Check to see if we have a collapsed range, if so,
    // there is nothing to iterate over.
    //
    // XXX: CharacterDataNodes (text nodes) are currently an exception,
    //      since we always want to be able to iterate text nodes at
    //      the end points of a range.

    if (!cData && startIndx == endIndx)
    {
      MakeEmpty();
      return NS_OK;
    }

    if (cData)
    {
      // It's a textnode.

      mFirst   = startCon;
      mLast    = startCon;
      mCurNode = startCon;

      RebuildIndexStack();
      return NS_OK;
    }
  }
  
  // Find first node in range.

  nsIContent *cChild = nsnull;

  if (!cData && ContentHasChildren(startCon))
    cChild = startCon->GetChildAt(startIndx);

  if (!cChild) // no children, must be a text node
  {
    if (mPre)
    {
      // XXX: In the future, if start offset is after the last
      //      character in the cdata node, should we set mFirst to
      //      the next sibling?

      if (!cData)
      {
        mFirst = GetNextSibling(startCon, nsnull);

        // Does mFirst node really intersect the range?
        // The range could be 'degenerate', ie not collapsed 
        // but still contain no content.
  
        if (mFirst && !ContentIsInTraversalRange(mFirst, mPre, startDOM, startIndx, endDOM, endIndx))
          mFirst = nsnull;
      }
      else
        mFirst = startCon;
    }
    else // post-order
      mFirst = startCon;
  }
  else
  {
    if (mPre)
      mFirst = cChild;
    else // post-order
    {
      mFirst = GetDeepFirstChild(cChild, nsnull);

      // Does mFirst node really intersect the range?
      // The range could be 'degenerate', ie not collapsed 
      // but still contain no content.
  
      if (mFirst && !ContentIsInTraversalRange(mFirst, mPre, startDOM, startIndx, endDOM, endIndx))
        mFirst = nsnull;
    }
  }


  // Find last node in range.

  cData = do_QueryInterface(endCon);

  if (cData || !ContentHasChildren(endCon) || endIndx == 0)
  {
    if (mPre)
      mLast = endCon;
    else // post-order
    {
      // XXX: In the future, if end offset is before the first
      //      character in the cdata node, should we set mLast to
      //      the prev sibling?

      if (!cData)
      {
        mLast = GetPrevSibling(endCon, nsnull);

        if (!ContentIsInTraversalRange(mLast, mPre, startDOM, startIndx, endDOM, endIndx))
          mLast = nsnull;
      }
      else
        mLast = endCon;
    }
  }
  else
  {
    PRInt32 indx = endIndx;

    cChild = endCon->GetChildAt(--indx);

    if (!cChild)  // No child at offset!
    {
      NS_NOTREACHED("nsContentIterator::nsContentIterator");
      return NS_ERROR_FAILURE; 
    }

    if (mPre)
    {
      mLast  = GetDeepLastChild(cChild, nsnull);

      if (!ContentIsInTraversalRange(mLast, mPre, startDOM, startIndx, endDOM, endIndx))
        mLast = nsnull;
    }
    else // post-order
      mLast = cChild;
  }

  // If either first or last is null, they both
  // have to be null!

  if (!mFirst || !mLast)
  {
    mFirst = nsnull;
    mLast  = nsnull;
  }
  
  mCurNode = mFirst;
  mIsDone  = !mCurNode;

  if (!mCurNode)
    mIndexes.Clear();
  else
    RebuildIndexStack();

  return NS_OK;
}


/******************************************************
 * Helper routines
 ******************************************************/
// WARNING: This function is expensive
nsresult nsContentIterator::RebuildIndexStack()
{
  // Make sure we start at the right indexes on the stack!  Build array up
  // to common parent of start and end.  Perhaps it's too many entries, but
  // thats far better than too few.
  nsIContent* parent;
  nsIContent* current;

  mIndexes.Clear();
  current = mCurNode;
  if (!current) {
    return NS_OK;
  }

  while (current != mCommonParent)
  {
    parent = current->GetParent();
    
    if (!parent)
      return NS_ERROR_FAILURE;
  
    mIndexes.InsertElementAt(NS_INT32_TO_PTR(parent->IndexOf(current)), 0);

    current = parent;
  }
  return NS_OK;
}

void
nsContentIterator::MakeEmpty()
{
  mCurNode      = nsnull;
  mFirst        = nsnull;
  mLast         = nsnull;
  mCommonParent = nsnull;
  mIsDone       = PR_TRUE;
  mIndexes.Clear();
}

nsIContent *
nsContentIterator::GetDeepFirstChild(nsIContent *aRoot, nsVoidArray *aIndexes)
{
  if (!aRoot) {
    return nsnull;
  }

  nsIContent *cN = aRoot;
  nsIContent *cChild = cN->GetChildAt(0);

  while (cChild)
  {
    if (aIndexes)
    {
      // Add this node to the stack of indexes
      aIndexes->AppendElement(NS_INT32_TO_PTR(0));
    }
    cN = cChild;
    cChild = cN->GetChildAt(0);
  }

  return cN;
}

nsIContent *
nsContentIterator::GetDeepLastChild(nsIContent *aRoot, nsVoidArray *aIndexes)
{
  if (!aRoot) {
    return nsnull;
  }

  nsIContent *deepLastChild = aRoot;

  nsIContent *cN = aRoot;
  PRInt32 numChildren = cN->GetChildCount();

  while (numChildren)
  {
    nsIContent *cChild = cN->GetChildAt(--numChildren);

    if (aIndexes)
    {
      // Add this node to the stack of indexes
      aIndexes->AppendElement(NS_INT32_TO_PTR(numChildren));
    }
    numChildren = cChild->GetChildCount();
    cN = cChild;

    deepLastChild = cN;
  }

  return deepLastChild;
}

// Get the next sibling, or parents next sibling, or grandpa's next sibling...
nsIContent *
nsContentIterator::GetNextSibling(nsIContent *aNode, 
                                  nsVoidArray *aIndexes)
{
  if (!aNode) 
    return nsnull;

  nsIContent *parent = aNode->GetParent();
  if (!parent)
    return nsnull;

  PRInt32 indx;

  if (aIndexes)
  {
    NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
    // use the last entry on the Indexes array for the current index
    indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
  }
  else
    indx = mCachedIndex;

  // reverify that the index of the current node hasn't changed.
  // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
  // ignore result this time - the index may now be out of range.
  nsIContent *sib = parent->GetChildAt(indx);
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    indx = parent->IndexOf(aNode);
  }

  // indx is now canonically correct
  if ((sib = parent->GetChildAt(++indx)))
  {
    // update index cache
    if (aIndexes)
    {
      aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
    }
    else mCachedIndex = indx;
  }
  else
  {
    if (parent != mCommonParent)
    {
      if (aIndexes)
      {
        // pop node off the stack, go up one level and return parent or fail.
        // Don't leave the index empty, especially if we're
        // returning NULL.  This confuses other parts of the code.
        if (aIndexes->Count() > 1)
          aIndexes->RemoveElementAt(aIndexes->Count()-1);
      }
    }

    // ok to leave cache out of date here if parent == mCommonParent?
    sib = GetNextSibling(parent, aIndexes);
  }
  
  return sib;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsIContent *
nsContentIterator::GetPrevSibling(nsIContent *aNode, 
                                  nsVoidArray *aIndexes)
{
  if (!aNode)
    return nsnull;

  nsIContent *parent = aNode->GetParent();
  if (!parent)
    return nsnull;

  PRInt32 indx;

  if (aIndexes)
  {
    NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
    // use the last entry on the Indexes array for the current index
    indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
  }
  else
    indx = mCachedIndex;

  // reverify that the index of the current node hasn't changed
  // ignore result this time - the index may now be out of range.
  nsIContent *sib = parent->GetChildAt(indx);
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    indx = parent->IndexOf(aNode);
  }

  // indx is now canonically correct
  if (indx > 0 && (sib = parent->GetChildAt(--indx)))
  {
    // update index cache
    if (aIndexes)
    {
      aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
    }
    else mCachedIndex = indx;
  }
  else if (parent != mCommonParent)
  {
    if (aIndexes)
    {
      // pop node off the stack, go up one level and try again.
      aIndexes->RemoveElementAt(aIndexes->Count()-1);
    }
    return GetPrevSibling(parent, aIndexes);
  }

  return sib;
}

nsIContent *
nsContentIterator::NextNode(nsIContent *aNode, nsVoidArray *aIndexes)
{
  nsIContent *cN = aNode;
  nsIContent *nextNode = nsnull;

  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    // if it has children then next node is first child
    if (ContentHasChildren(cN))
    {
      nsIContent *cFirstChild = cN->GetChildAt(0);

      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(NS_INT32_TO_PTR(0));
      }
      else mCachedIndex = 0;
      
      return cFirstChild;
    }

    // else next sibling is next
    nextNode = GetNextSibling(cN, aIndexes);
  }
  else  // post-order
  {
    nsIContent *parent = cN->GetParent();
    nsIContent *cSibling = nsnull;
    PRInt32 indx;

    // get the cached index
    if (aIndexes)
    {
      NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
      // use the last entry on the Indexes array for the current index
      indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
    }
    else indx = mCachedIndex;

    // reverify that the index of the current node hasn't changed.
    // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
    // ignore result this time - the index may now be out of range.
    if (indx >= 0)
      cSibling = parent->GetChildAt(indx);
    if (cSibling != cN)
    {
      // someone changed our index - find the new index the painful way
      indx = parent->IndexOf(cN);
    }

    // indx is now canonically correct
    cSibling = parent->GetChildAt(++indx);
    if (cSibling)
    {
      // update cache
      if (aIndexes)
      {
        // replace an entry on the index stack
        aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
      }
      else mCachedIndex = indx;
      
      // next node is siblings "deep left" child
      return GetDeepFirstChild(cSibling, aIndexes); 
    }
  
    // else it's the parent
    // update cache
    if (aIndexes)
    {
      // pop an entry off the index stack
      // Don't leave the index empty, especially if we're
      // returning NULL.  This confuses other parts of the code.
      if (aIndexes->Count() > 1)
        aIndexes->RemoveElementAt(aIndexes->Count()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    nextNode = parent;
  }

  return nextNode;
}

nsIContent *
nsContentIterator::PrevNode(nsIContent *aNode, nsVoidArray *aIndexes)
{
  nsIContent *prevNode = nsnull;
  nsIContent *cN = aNode;
   
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsIContent *parent = cN->GetParent();
    nsIContent *cSibling = nsnull;
    PRInt32 indx;

    // get the cached index
    if (aIndexes)
    {
      NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
      // use the last entry on the Indexes array for the current index
      indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
    }
    else indx = mCachedIndex;

    // reverify that the index of the current node hasn't changed.
    // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
    // ignore result this time - the index may now be out of range.
    if (indx >= 0)
      cSibling = parent->GetChildAt(indx);

    if (cSibling != cN)
    {
      // someone changed our index - find the new index the painful way
      indx = parent->IndexOf(cN);
    }

    // indx is now canonically correct
    if (indx && (cSibling = parent->GetChildAt(--indx)))
    {
      // update cache
      if (aIndexes)
      {
        // replace an entry on the index stack
        aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
      }
      else mCachedIndex = indx;
      
      // prev node is siblings "deep right" child
      return GetDeepLastChild(cSibling, aIndexes); 
    }
  
    // else it's the parent
    // update cache
    if (aIndexes)
    {
      // pop an entry off the index stack
      aIndexes->RemoveElementAt(aIndexes->Count()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    prevNode = parent;
  }
  else  // post-order
  {
    PRInt32 numChildren = cN->GetChildCount();
  
    // if it has children then prev node is last child
    if (numChildren)
    {
      nsIContent *cLastChild = cN->GetChildAt(--numChildren);

      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(NS_INT32_TO_PTR(numChildren));
      }
      else mCachedIndex = numChildren;
      
      return cLastChild;
    }

    // else prev sibling is previous
    prevNode = GetPrevSibling(cN, aIndexes);
  }

  return prevNode;
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

void
nsContentIterator::First()
{
  NS_ASSERTION(mFirst, "No first node!");

  if (mFirst) {
#ifdef DEBUG
    nsresult rv =
#endif
    PositionAt(mFirst);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");
  }

  mIsDone = mFirst == nsnull;
}


void
nsContentIterator::Last()
{
  NS_ASSERTION(mLast, "No last node!");

  if (mLast) {
#ifdef DEBUG
    nsresult rv =
#endif
    PositionAt(mLast);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");
  }

  mIsDone = mLast == nsnull;
}


void
nsContentIterator::Next()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return;
  }

  mCurNode = NextNode(mCurNode, &mIndexes);
}


void
nsContentIterator::Prev()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return;
  }

  mCurNode = PrevNode(mCurNode, &mIndexes);
}


PRBool
nsContentIterator::IsDone()
{
  return mIsDone;
}


// Keeping arrays of indexes for the stack of nodes makes PositionAt
// interesting...
nsresult
nsContentIterator::PositionAt(nsIContent* aCurNode)
{
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;

  nsIContent *newCurNode = aCurNode;
  nsIContent *tempNode = mCurNode;

  mCurNode = aCurNode;
  // take an early out if this doesn't actually change the position
  if (mCurNode == tempNode)
  {
    mIsDone = PR_FALSE;  // paranoia
    return NS_OK;
  }

  // Check to see if the node falls within the traversal range.

  nsCOMPtr<nsIDOMNode> firstNode(do_QueryInterface(mFirst));
  nsCOMPtr<nsIDOMNode> lastNode(do_QueryInterface(mLast));
  PRInt32 firstOffset=0, lastOffset=0;

  if (firstNode && lastNode)
  {
    PRUint32 numChildren;

    if (mPre)
    {
      ContentToParentOffset(mFirst, getter_AddRefs(firstNode), &firstOffset);

      numChildren = GetNumChildren(lastNode);

      if (numChildren)
        lastOffset = 0;
      else
      {
        ContentToParentOffset(mLast, getter_AddRefs(lastNode), &lastOffset);
        ++lastOffset;
      }
    }
    else
    {
      numChildren = GetNumChildren(firstNode);

      if (numChildren)
        firstOffset = numChildren;
      else
        ContentToParentOffset(mFirst, getter_AddRefs(firstNode), &firstOffset);

      ContentToParentOffset(mLast, getter_AddRefs(lastNode), &lastOffset);
      ++lastOffset;
    }
  }

  if (!firstNode || !lastNode ||
      !ContentIsInTraversalRange(mCurNode, mPre, firstNode, firstOffset,
                                 lastNode, lastOffset))
  {
    mIsDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }

  // We can be at ANY node in the sequence.
  // Need to regenerate the array of indexes back to the root or common parent!
  nsAutoVoidArray      oldParentStack;
  nsAutoVoidArray      newIndexes;

  // Get a list of the parents up to the root, then compare the new node
  // with entries in that array until we find a match (lowest common
  // ancestor).  If no match, use IndexOf, take the parent, and repeat.
  // This avoids using IndexOf() N times on possibly large arrays.  We
  // still end up doing it a fair bit.  It's better to use Clone() if
  // possible.

  // we know the depth we're down (though we may not have started at the
  // top).
  if (!oldParentStack.SizeTo(mIndexes.Count()+1))
    return NS_ERROR_FAILURE;

  // plus one for the node we're currently on.
  for (PRInt32 i = mIndexes.Count()+1; i > 0 && tempNode; i--)
  {
    // Insert at head since we're walking up
    oldParentStack.InsertElementAt(tempNode,0);

    nsIContent *parent = tempNode->GetParent();

    if (!parent)  // this node has no parent, and thus no index
      break;

    if (parent == mCurNode)
    {
      // The position was moved to a parent of the current position. 
      // All we need to do is drop some indexes.  Shortcut here.
      mIndexes.RemoveElementsAt(mIndexes.Count() - (oldParentStack.Count()+1),
                                oldParentStack.Count());
      mIsDone = PR_FALSE;
      return NS_OK;
    }
    tempNode = parent;
  }

  // Ok.  We have the array of old parents.  Look for a match.
  while (newCurNode)
  {
    nsIContent *parent = newCurNode->GetParent();

    if (!parent)  // this node has no parent, and thus no index
      break;

    PRInt32 indx = parent->IndexOf(newCurNode);

    // insert at the head!
    newIndexes.InsertElementAt(NS_INT32_TO_PTR(indx),0);

    // look to see if the parent is in the stack
    indx = oldParentStack.IndexOf(parent);
    if (indx >= 0)
    {
      // ok, the parent IS on the old stack!  Rework things.
      // we want newIndexes to replace all nodes equal to or below the match
      // Note that index oldParentStack.Count()-1 is the last node, which is
      // one BELOW the last index in the mIndexes stack.
      PRInt32 numToDrop = oldParentStack.Count()-(1+indx);
      if (numToDrop > 0)
        mIndexes.RemoveElementsAt(mIndexes.Count() - numToDrop,numToDrop);
      mIndexes.InsertElementsAt(newIndexes,mIndexes.Count());

      break;
    }
    newCurNode = parent;
  }

  // phew!

  mIsDone = PR_FALSE;
  return NS_OK;
}


nsIContent *
nsContentIterator::GetCurrentNode()
{
  if (mIsDone) {
    return nsnull;
  }

  NS_ASSERTION(mCurNode, "Null current node in an iterator that's not done!");

  return mCurNode;
}





/*====================================================================================*/
/*====================================================================================*/






/******************************************************
 * nsContentSubtreeIterator
 ******************************************************/


/*
 *  A simple iterator class for traversing the content in "top subtree" order
 */
class nsContentSubtreeIterator : public nsContentIterator 
{
public:
  nsContentSubtreeIterator() {};
  virtual ~nsContentSubtreeIterator() {};

  // nsContentIterator overrides ------------------------------

  virtual nsresult Init(nsIContent* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void Next();

  virtual void Prev();

  virtual nsresult PositionAt(nsIContent* aCurNode);

  // Must override these because we don't do PositionAt
  virtual void First();

  // Must override these because we don't do PositionAt
  virtual void Last();

protected:

  nsresult GetTopAncestorInRange(nsIContent *aNode,
                                 nsCOMPtr<nsIContent> *outAnestor);

  // no copy's or assigns  FIX ME
  nsContentSubtreeIterator(const nsContentSubtreeIterator&);
  nsContentSubtreeIterator& operator=(const nsContentSubtreeIterator&);

  nsCOMPtr<nsIDOMRange> mRange;
  // these arrays all typically are used and have elements
#if 0
  nsAutoVoidArray mStartNodes;
  nsAutoVoidArray mStartOffsets;
#endif

  nsAutoVoidArray mEndNodes;
  nsAutoVoidArray mEndOffsets;
};

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);




/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentSubtreeIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}



/******************************************************
 * Init routines
 ******************************************************/


nsresult nsContentSubtreeIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsContentSubtreeIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  mIsDone = PR_FALSE;

  mRange = aRange;
  
  // get the start node and offset, convert to nsIContent
  nsCOMPtr<nsIDOMNode> commonParent;
  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  nsCOMPtr<nsIContent> cStartP;
  nsCOMPtr<nsIContent> cEndP;
  nsCOMPtr<nsIContent> cN;
  nsIContent *firstCandidate = nsnull;
  nsIContent *lastCandidate = nsnull;
  nsCOMPtr<nsIDOMNode> dChild;
  nsCOMPtr<nsIContent> cChild;
  PRInt32 indx, startIndx, endIndx;
  PRInt32 numChildren;

  // get common content parent
  if (NS_FAILED(aRange->GetCommonAncestorContainer(getter_AddRefs(commonParent))) || !commonParent)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(commonParent);

  // get start content parent
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(startParent))) || !startParent)
    return NS_ERROR_FAILURE;
  cStartP = do_QueryInterface(startParent);
  aRange->GetStartOffset(&startIndx);

  // get end content parent
  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(endParent))) || !endParent)
    return NS_ERROR_FAILURE;
  cEndP = do_QueryInterface(endParent);
  aRange->GetEndOffset(&endIndx);
  
  // short circuit when start node == end node
  if (startParent == endParent)
  {
    cChild = cStartP->GetChildAt(0);
  
    if (!cChild) // no children, must be a text node or empty container
    {
      // all inside one text node - empty subtree iterator
      MakeEmpty();
      return NS_OK;
    }
    else
    {
      if (startIndx == endIndx)  // collapsed range
      {
        MakeEmpty();
        return NS_OK;
      }
    }
  }
  
  // cache ancestors
#if 0
  nsContentUtils::GetAncestorsAndOffsets(startParent, startIndx,
                                         &mStartNodes, &mStartOffsets);
#endif
  nsContentUtils::GetAncestorsAndOffsets(endParent, endIndx,
                                         &mEndNodes, &mEndOffsets);

  // find first node in range
  aRange->GetStartOffset(&indx);
  numChildren = GetNumChildren(startParent);
  
  if (!numChildren) // no children, must be a text node
  {
    cN = cStartP; 
  }
  else
  {
    dChild = GetChildAt(startParent, indx);
    cChild = do_QueryInterface(dChild);
    if (!cChild)  // offset after last child
    {
      cN = cStartP;
    }
    else
    {
      firstCandidate = cChild;
    }
  }
  
  if (!firstCandidate)
  {
    // then firstCandidate is next node after cN
    firstCandidate = GetNextSibling(cN, nsnull);

    if (!firstCandidate)
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  firstCandidate = GetDeepFirstChild(firstCandidate, nsnull);
  
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  PRBool nodeBefore, nodeAfter;
  if (NS_FAILED(nsRange::CompareNodeToRange(firstCandidate, aRange,
                                            &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the first node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_FAILED(GetTopAncestorInRange(firstCandidate, address_of(mFirst))))
    return NS_ERROR_FAILURE;

  // now to find the last node
  aRange->GetEndOffset(&indx);
  numChildren = GetNumChildren(endParent);

  if (indx > numChildren) indx = numChildren;
  if (!indx)
  {
    cN = cEndP;
  }
  else
  {
    if (!numChildren) // no children, must be a text node
    {
      cN = cEndP; 
    }
    else
    {
      dChild = GetChildAt(endParent, --indx);
      cChild = do_QueryInterface(dChild);
      if (!cChild)  // shouldn't happen
      {
        NS_ASSERTION(0,"tree traversal trouble in nsContentSubtreeIterator::Init");
        return NS_ERROR_FAILURE;
      }
      else
      {
        lastCandidate = cChild;
      }
    }
  }
  
  if (!lastCandidate)
  {
    // then lastCandidate is prev node before cN
    lastCandidate = GetPrevSibling(cN, nsnull);
  }
  
  lastCandidate = GetDeepLastChild(lastCandidate, nsnull);
  
  // confirm that this last possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  if (NS_FAILED(nsRange::CompareNodeToRange(lastCandidate, aRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the last node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_FAILED(GetTopAncestorInRange(lastCandidate, address_of(mLast))))
    return NS_ERROR_FAILURE;
  
  mCurNode = mFirst;

  return NS_OK;
}


/****************************************************************
 * nsContentSubtreeIterator overrides of ContentIterator routines
 ****************************************************************/

// we can't call PositionAt in a subtree iterator...
void
nsContentSubtreeIterator::First()
{
  mIsDone = mFirst == nsnull;

  mCurNode = mFirst;
}

// we can't call PositionAt in a subtree iterator...
void
nsContentSubtreeIterator::Last()
{
  mIsDone = mLast == nsnull;

  mCurNode = mLast;
}


void
nsContentSubtreeIterator::Next()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return;
  }

  nsIContent *nextNode = GetNextSibling(mCurNode, nsnull);
  NS_ASSERTION(nextNode, "No next sibling!?! This could mean deadlock!");

/*
  nextNode = GetDeepFirstChild(nextNode);
  return GetTopAncestorInRange(nextNode, address_of(mCurNode));
*/
  PRInt32 i = mEndNodes.IndexOf(nextNode);
  while (i != -1)
  {
    // as long as we are finding ancestors of the endpoint of the range,
    // dive down into their children
    nextNode = nextNode->GetChildAt(0);
    NS_ASSERTION(nextNode, "Iterator error, expected a child node!");

    // should be impossible to get a null pointer.  If we went all the way
    // down the child chain to the bottom without finding an interior node, 
    // then the previous node should have been the last, which was
    // was tested at top of routine.
    i = mEndNodes.IndexOf(nextNode);
  }

  mCurNode = nextNode;

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mLast is in generated content, we need this
  // to stop the iterator when we've walked past past the last node!
  mIsDone = mCurNode == nsnull;

  return;
}


void
nsContentSubtreeIterator::Prev()
{
  // Prev should be optimized to use the mStartNodes, just as Next
  // uses mEndNodes.
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return;
  }

  nsIContent *prevNode = PrevNode(GetDeepFirstChild(mCurNode, nsnull), nsnull);

  prevNode = GetDeepLastChild(prevNode, nsnull);
  
  GetTopAncestorInRange(prevNode, address_of(mCurNode));

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mFirst is in generated content, we need this
  // to stop the iterator when we've walked past past the first node!
  mIsDone = mCurNode == nsnull;
}


nsresult
nsContentSubtreeIterator::PositionAt(nsIContent* aCurNode)
{
  NS_ERROR("Not implemented!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

/****************************************************************
 * nsContentSubtreeIterator helper routines
 ****************************************************************/

nsresult
nsContentSubtreeIterator::GetTopAncestorInRange(nsIContent *aNode,
                                                nsCOMPtr<nsIContent> *outAnestor)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!outAnestor) 
    return NS_ERROR_NULL_POINTER;
  
  
  // sanity check: aNode is itself in the range
  PRBool nodeBefore, nodeAfter;
  if (NS_FAILED(nsRange::CompareNodeToRange(aNode, mRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContent> parent, tmp;
  while (aNode)
  {
    parent = aNode->GetParent();
    if (!parent)
    {
      if (tmp)
      {
        *outAnestor = tmp;
        return NS_OK;
      }
      else return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(nsRange::CompareNodeToRange(parent, mRange, &nodeBefore,
                                              &nodeAfter)))
      return NS_ERROR_FAILURE;

    if (nodeBefore || nodeAfter)
    {
      *outAnestor = aNode;
      return NS_OK;
    }
    tmp = aNode;
    aNode = parent;
  }
  return NS_ERROR_FAILURE;
}

