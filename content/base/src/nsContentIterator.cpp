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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIPresContext.h"
#include "nsIComponentManager.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsVoidArray.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kCSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

// couple of utility static functs

///////////////////////////////////////////////////////////////////////////
// GetNumChildren: returns the number of things inside aNode. 
//
static PRUint32
GetNumChildren(nsIDOMNode *aNode) 
{
  PRUint32 numChildren = 0;
  if (!aNode)
    return 0;

  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsresult res = aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList) 
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
  
  PRBool hasChildNodes;
  aParent->HasChildNodes(&hasChildNodes);
  if (PR_TRUE==hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsresult res = aParent->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList) 
      nodeList->Item(aOffset, getter_AddRefs(resultNode));
  }
  
  return resultNode;
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

  NS_IMETHOD Init(nsIContent* aRoot);

  NS_IMETHOD Init(nsIDOMRange* aRange);

  NS_IMETHOD First();

  NS_IMETHOD Last();
  
  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  NS_IMETHOD CurrentNode(nsIContent **aNode);

  NS_IMETHOD IsDone();

  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  NS_IMETHOD MakePre();

  NS_IMETHOD MakePost();

  
  // nsIEnumertor interface methods ------------------------------
  
  //NS_IMETHOD CurrentItem(nsISupports **aItem);

protected:

  nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot, nsVoidArray *aIndexes);
  nsCOMPtr<nsIContent> GetDeepLastChild(nsCOMPtr<nsIContent> aRoot, nsVoidArray *aIndexes);
  
  nsresult GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling, nsVoidArray *aIndexes);
  nsresult GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling, nsVoidArray *aIndexes);
  
  nsresult NextNode(nsCOMPtr<nsIContent> *ioNextNode, nsVoidArray *aIndexes);
  nsresult PrevNode(nsCOMPtr<nsIContent> *ioPrevNode, nsVoidArray *aIndexes);

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



/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentIterator();
  if (iter)
    return iter->QueryInterface(NS_GET_IID(nsIContentIterator), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}


/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_ADDREF(nsContentIterator)
NS_IMPL_RELEASE(nsContentIterator)

nsresult nsContentIterator::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
/*  if (aIID.Equals(NS_GET_IID(nsIEnumerator))) 
  {
    *aInstancePtrResult = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }  */
  if (aIID.Equals(NS_GET_IID(nsIContentIterator))) 
  {
    *aInstancePtrResult = (void*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


/******************************************************
 * constructor/destructor
 ******************************************************/

nsContentIterator::nsContentIterator() :
  // don't need to explicitly initialize |nsCOMPtr|s, they will automatically be NULL
  mIsDone(PR_FALSE), mPre(PR_FALSE)
{
  NS_INIT_REFCNT();
}


nsContentIterator::~nsContentIterator()
{
}


/******************************************************
 * Init routines
 ******************************************************/


nsresult nsContentIterator::Init(nsIContent* aRoot)
{
  if (!aRoot) 
    return NS_ERROR_NULL_POINTER; 
  mIsDone = PR_FALSE;
  nsCOMPtr<nsIContent> root( do_QueryInterface(aRoot) );
  mIndexes.Clear();
  mFirst = GetDeepFirstChild(root, &mIndexes); 
  mLast = root;
  mCommonParent = root;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  nsCOMPtr<nsIDOMNode> dN;
  nsCOMPtr<nsIContent> cChild;
  
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
  
  // short circuit when start node == end node
  if (startDOM == endDOM)
  {
    startCon->ChildAt(0,*getter_AddRefs(cChild));
  
    if (!cChild) // no children, must be a text node or empty container
    {
      mFirst = startCon;
      mLast = startCon;
      mCurNode = startCon;
      RebuildIndexStack();
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
  
  // find first node in range
  startCon->ChildAt(0,*getter_AddRefs(cChild));
  
  if (!cChild) // no children, must be a text node
  {
    mFirst = startCon; 
  }
  else
  {
    startCon->ChildAt(startIndx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, parent is first node
    {
      mFirst = startCon;
    }
    else
    {
      mFirst = GetDeepFirstChild(cChild, nsnull);
    }
    // Does that first node really intersect the range?
    // the range could be collapsed, or the range could be
    // 'degenerate', ie not collapsed but still containing
    // no content.  In this case, we want the iterator to
    // be empty
  
    if (!IsNodeIntersectsRange(mFirst, aRange))
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  // find last node in range
  endCon->ChildAt(0,*getter_AddRefs(cChild));

  if (!cChild) // no children, must be a text node
  {
    mLast = endCon; 
  }
  else if (endIndx == 0) // before first child, parent is last node
  {
    mLast = endCon; 
  }
  else
  {
    endCon->ChildAt(--endIndx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, last child is last node
    {
      endCon->ChildCount(endIndx);
      endCon->ChildAt(--endIndx,*getter_AddRefs(cChild)); 
      if (!cChild)
      {
        NS_NOTREACHED("nsContentIterator::nsContentIterator");
        return NS_ERROR_FAILURE; 
      }
    }
    mLast = cChild;  
  }
  
  mCurNode = mFirst;
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
  nsCOMPtr<nsIContent> parent;
  nsCOMPtr<nsIContent> current;
  PRInt32              indx;

  mIndexes.Clear();
  current = mCurNode;
  while (current && current != mCommonParent)
  {
    if (NS_FAILED(current->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!parent || NS_FAILED(parent->IndexOf(current, indx)))
      return NS_ERROR_FAILURE;
  
    mIndexes.InsertElementAt(NS_INT32_TO_PTR(indx),0);

    current = parent;
  }
  return NS_OK;
}

void nsContentIterator::MakeEmpty()
{
  nsCOMPtr<nsIContent> noNode;
  mCurNode = noNode;
  mFirst = noNode;
  mLast = noNode;
  mCommonParent = noNode;
  mIsDone = PR_TRUE;
  mIndexes.Clear();
}

nsCOMPtr<nsIContent> nsContentIterator::GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot, nsVoidArray *aIndexes)
{
  nsCOMPtr<nsIContent> deepFirstChild;
  
  if (aRoot) 
  {  
    nsCOMPtr<nsIContent> cN = aRoot;
    nsCOMPtr<nsIContent> cChild;
    cN->ChildAt(0,*getter_AddRefs(cChild));
    while ( cChild )
    {
      if (aIndexes)
      {
        // Add this node to the stack of indexes
        aIndexes->AppendElement(NS_INT32_TO_PTR(0));
      }
      cN = cChild;
      cN->ChildAt(0,*getter_AddRefs(cChild));
    }
    deepFirstChild = cN;
  }
  
  return deepFirstChild;
}

nsCOMPtr<nsIContent> nsContentIterator::GetDeepLastChild(nsCOMPtr<nsIContent> aRoot, nsVoidArray *aIndexes)
{
  nsCOMPtr<nsIContent> deepFirstChild;
  
  if (aRoot) 
  {  
    nsCOMPtr<nsIContent> cN = aRoot;
    nsCOMPtr<nsIContent> cChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);

    while ( numChildren )
    {
      cN->ChildAt(--numChildren,*getter_AddRefs(cChild));
      if (cChild)
      {
        if (aIndexes)
        {
          // Add this node to the stack of indexes
          aIndexes->AppendElement(NS_INT32_TO_PTR(numChildren));
        }
        cChild->ChildCount(numChildren);
        cN = cChild;
      }
      else
      {
        break;
      }
    }
    deepFirstChild = cN;
  }
  
  return deepFirstChild;
}

// Get the next sibling, or parents next sibling, or grandpa's next sibling...
nsresult nsContentIterator::GetNextSibling(nsCOMPtr<nsIContent> aNode, 
                                           nsCOMPtr<nsIContent> *aSibling, 
                                           nsVoidArray *aIndexes)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))) || !parent)
    return NS_ERROR_FAILURE;

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
  (void) parent->ChildAt(indx, *getter_AddRefs(sib)); // sib defaults to nsnull
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    if (NS_FAILED(parent->IndexOf(aNode,indx)))
      return NS_ERROR_FAILURE;
  }

  // indx is now canonically correct
  if (NS_SUCCEEDED(parent->ChildAt(++indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
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
    return GetNextSibling(parent, aSibling, aIndexes);
  }
  else
  {
    *aSibling = nsnull;
    // ok to leave cache out of date here?
  }
  
  return NS_OK;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsresult nsContentIterator::GetPrevSibling(nsCOMPtr<nsIContent> aNode, 
                                           nsCOMPtr<nsIContent> *aSibling, 
                                           nsVoidArray *aIndexes)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))) || !parent)
    return NS_ERROR_FAILURE;

  if (aIndexes)
  {
    NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
    // use the last entry on the Indexes array for the current index
    indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
  }
  else indx = mCachedIndex;
  
  // reverify that the index of the current node hasn't changed
  // ignore result this time - the index may now be out of range.
  (void) parent->ChildAt(indx, *getter_AddRefs(sib)); // sib defaults to nsnull
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    if (NS_FAILED(parent->IndexOf(aNode,indx)))
      return NS_ERROR_FAILURE;
  }

  // indx is now canonically correct
  if (NS_SUCCEEDED(parent->ChildAt(--indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
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
    return GetPrevSibling(parent, aSibling, aIndexes);
  }
  else
  {
    *aSibling = nsnull;
    // ok to leave cache out of date here?
  }

  return NS_OK;
}

nsresult nsContentIterator::NextNode(nsCOMPtr<nsIContent> *ioNextNode, nsVoidArray *aIndexes)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIContent> cN = *ioNextNode;

  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cFirstChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then next node is first child
    if (numChildren)
    {
      if (NS_FAILED(cN->ChildAt(0,*getter_AddRefs(cFirstChild))))
        return NS_ERROR_FAILURE;
      if (!cFirstChild)
        return NS_ERROR_FAILURE;
      
      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(NS_INT32_TO_PTR(0));
      }
      else mCachedIndex = 0;
      
      *ioNextNode = cFirstChild;
      return NS_OK;
    }
  
    // else next sibling is next
    return GetNextSibling(cN, ioNextNode, aIndexes);
  }
  else  // post-order
  {
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get next sibling if there is one
    if (NS_FAILED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
      
    // get the cached index
    if (aIndexes)
    {
      NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
      // use the last entry on the Indexes array for the current index
      indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
    }
    else indx = mCachedIndex;

    if (NS_SUCCEEDED(parent->ChildAt(++indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // update cache
      if (aIndexes)
      {
        // replace an entry on the index stack
        aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
      }
      else mCachedIndex = indx;
      
      // next node is siblings "deep left" child
      *ioNextNode = GetDeepFirstChild(cSibling, aIndexes); 
      return NS_OK;
    }
  
    // else it's the parent
    // update cache
    if (aIndexes)
    {
      // pop an entry off the index stack
      aIndexes->RemoveElementAt(aIndexes->Count()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    *ioNextNode = parent;
  }
  return NS_OK;
}

nsresult nsContentIterator::PrevNode(nsCOMPtr<nsIContent> *ioNextNode, nsVoidArray *aIndexes)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> cN = *ioNextNode;
   
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get prev sibling if there is one
    if (NS_FAILED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;

    // get the cached index
    if (aIndexes)
    {
      NS_ASSERTION(aIndexes->Count() > 0, "ContentIterator stack underflow");
      // use the last entry on the Indexes array for the current index
      indx = NS_PTR_TO_INT32((*aIndexes)[aIndexes->Count()-1]);
    }
    else indx = mCachedIndex;

    if (indx && NS_SUCCEEDED(parent->ChildAt(--indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // update cache
      if (aIndexes)
      {
        // replace an entry on the index stack
        aIndexes->ReplaceElementAt(NS_INT32_TO_PTR(indx),aIndexes->Count()-1);
      }
      else mCachedIndex = indx;
      
      // prev node is siblings "deep right" child
      *ioNextNode = GetDeepLastChild(cSibling, aIndexes); 
      return NS_OK;
    }
  
    // else it's the parent
    // update cache
    if (aIndexes)
    {
      // pop an entry off the index stack
      aIndexes->RemoveElementAt(aIndexes->Count()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    *ioNextNode = parent;
  }
  else  // post-order
  {
    nsCOMPtr<nsIContent> cLastChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then prev node is last child
    if (numChildren)
    {
      if (NS_FAILED(cN->ChildAt(--numChildren,*getter_AddRefs(cLastChild))))
        return NS_ERROR_FAILURE;
      if (!cLastChild)
        return NS_ERROR_FAILURE;

      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(NS_INT32_TO_PTR(numChildren));
      }
      else mCachedIndex = numChildren;
      
      *ioNextNode = cLastChild;
      return NS_OK;
    }
  
    // else prev sibling is previous
    return GetPrevSibling(cN, ioNextNode, aIndexes);
  }
  return NS_OK;
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

nsresult nsContentIterator::First()
{
  if (!mFirst) 
    return NS_ERROR_FAILURE;
  mIsDone = PR_FALSE;
  if (mFirst == mCurNode) 
    return NS_OK;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsContentIterator::Last()
{
  if (!mLast) 
    return NS_ERROR_FAILURE;
  mIsDone = PR_FALSE;
  if (mLast == mCurNode) 
    return NS_OK;
  mCurNode = mLast;
  return NS_OK;
}


nsresult nsContentIterator::Next()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  return NextNode(address_of(mCurNode), &mIndexes);
}


nsresult nsContentIterator::Prev()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  return PrevNode(address_of(mCurNode), &mIndexes);
}


nsresult nsContentIterator::IsDone()
{
  if (mIsDone) 
    return NS_OK;
  else 
    return NS_ENUMERATOR_FALSE;
}


// Keeping arrays of indexes for the stack of nodes makes PositionAt
// interesting...
nsresult nsContentIterator::PositionAt(nsIContent* aCurNode)
{
  nsCOMPtr<nsIContent> newCurNode;
  nsCOMPtr<nsIContent> tempNode(mCurNode);

  // XXX need to confirm that aCurNode is within range
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;
  mCurNode = newCurNode = do_QueryInterface(aCurNode);
  // take an early out if this doesn't actually change the position
  if (mCurNode == tempNode)
  {
    mIsDone = PR_FALSE;  // paranoia
    return NS_OK;
  }

  // We can be at ANY node in the sequence.
  // Need to regenerate the array of indexes back to the root or common parent!
  nsCOMPtr<nsIContent> parent;
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

    if (NS_FAILED(tempNode->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;

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
    PRInt32 indx;

    if (NS_FAILED(newCurNode->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;

    if (!parent)  // this node has no parent, and thus no index
      break;

    if (NS_FAILED(parent->IndexOf(newCurNode, indx)))
      return NS_ERROR_FAILURE;

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


nsresult nsContentIterator::MakePre()
{
  // XXX need to confirm mCurNode is within range
  mPre = PR_TRUE;
  return NS_OK;
}

nsresult nsContentIterator::MakePost()
{
  // XXX need to confirm mCurNode is within range
  mPre = PR_FALSE;
  return NS_OK;
}


nsresult nsContentIterator::CurrentNode(nsIContent **aNode)
{
  if (!mCurNode) 
    return NS_ERROR_FAILURE;
  if (mIsDone) 
    return NS_ERROR_FAILURE;
  return mCurNode->QueryInterface(NS_GET_IID(nsIContent), (void**) aNode);
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

  NS_IMETHOD Init(nsIContent* aRoot);

  NS_IMETHOD Init(nsIDOMRange* aRange);

  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  NS_IMETHOD MakePre();

  NS_IMETHOD MakePost();

protected:

  nsresult GetTopAncestorInRange( nsCOMPtr<nsIContent> aNode,
                                  nsCOMPtr<nsIContent> *outAnestor);
                                  
  // no copy's or assigns  FIX ME
  nsContentSubtreeIterator(const nsContentSubtreeIterator&);
  nsContentSubtreeIterator& operator=(const nsContentSubtreeIterator&);

  nsCOMPtr<nsIDOMRange> mRange;
  // these arrays all typically are used and have elements
  nsAutoVoidArray mStartNodes;
  nsAutoVoidArray mStartOffsets;
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
  if (iter)
    return iter->QueryInterface(NS_GET_IID(nsIContentIterator), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
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

  mRange = do_QueryInterface(aRange);
  
  // get the start node and offset, convert to nsIContent
  nsCOMPtr<nsIDOMNode> commonParent;
  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  nsCOMPtr<nsIContent> cStartP;
  nsCOMPtr<nsIContent> cEndP;
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIContent> firstCandidate;
  nsCOMPtr<nsIContent> lastCandidate;
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
    cStartP->ChildAt(0,*getter_AddRefs(cChild));
  
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
  nsRange::GetAncestorsAndOffsets(startParent, startIndx, &mStartNodes, &mStartOffsets);
  nsRange::GetAncestorsAndOffsets(endParent, endIndx, &mEndNodes, &mEndOffsets);

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
    if (NS_FAILED(GetNextSibling(cN, address_of(firstCandidate), nsnull)) || !firstCandidate)
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
  if (NS_FAILED(CompareNodeToRange(firstCandidate, aRange, &nodeBefore, &nodeAfter)))
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
    if (NS_FAILED(GetPrevSibling(cN, address_of(lastCandidate), nsnull)))
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  lastCandidate = GetDeepLastChild(lastCandidate, nsnull);
  
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  if (NS_FAILED(CompareNodeToRange(lastCandidate, aRange, &nodeBefore, &nodeAfter)))
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

nsresult nsContentSubtreeIterator::Next()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  nsCOMPtr<nsIContent> nextNode;
  if (NS_FAILED(GetNextSibling(mCurNode, address_of(nextNode), nsnull)))
    return NS_OK;
/*
  nextNode = GetDeepFirstChild(nextNode);
  return GetTopAncestorInRange(nextNode, address_of(mCurNode));
*/
  PRInt32 i = mEndNodes.IndexOf((void*)nextNode);
  while (i != -1)
  {
    // as long as we are finding ancestors of the endpoint of the range,
    // dive down into their children
    nsCOMPtr<nsIContent> cChild;
    nextNode->ChildAt(0,*getter_AddRefs(cChild));
    if (!cChild) return NS_ERROR_NULL_POINTER;
    // should be impossible to get a null pointer.  If we went all the 
    // down the child chain to the bottom without finding an interior node, 
    // then the previous node should have been the last, which was
    // was tested at top of routine.
    nextNode = cChild;
    i = mEndNodes.IndexOf((void*)nextNode);
  }

  mCurNode = do_QueryInterface(nextNode);
  return NS_OK;
}


nsresult nsContentSubtreeIterator::Prev()
{
  // Prev should be optimized to use the mStartNodes, just as Next uses mEndNodes.
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  nsCOMPtr<nsIContent> prevNode;
  prevNode = GetDeepFirstChild(mCurNode, nsnull);
  if (NS_FAILED(PrevNode(address_of(prevNode), nsnull)))
    return NS_OK;
  prevNode = GetDeepLastChild(prevNode, nsnull);
  return GetTopAncestorInRange(prevNode, address_of(mCurNode));
}

nsresult nsContentSubtreeIterator::PositionAt(nsIContent* aCurNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsContentSubtreeIterator::MakePre()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsContentSubtreeIterator::MakePost()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/****************************************************************
 * nsContentSubtreeIterator helper routines
 ****************************************************************/

nsresult nsContentSubtreeIterator::GetTopAncestorInRange(
                                       nsCOMPtr<nsIContent> aNode,
                                       nsCOMPtr<nsIContent> *outAnestor)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!outAnestor) 
    return NS_ERROR_NULL_POINTER;
  
  
  // sanity check: aNode is itself in the range
  PRBool nodeBefore, nodeAfter;
  if (NS_FAILED(CompareNodeToRange(aNode, mRange, &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;
  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContent> parent, tmp;
  while (aNode)
  {
    if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!parent)
    {
      if (tmp)
      {
        *outAnestor = tmp;
        return NS_OK;
      }
      else return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(CompareNodeToRange(parent, mRange, &nodeBefore, &nodeAfter)))
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

