/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * nsContentIterator.cpp: Implementation of the nsContentIterator object.
 * This ite
 */

#include "nsISupports.h"
//#include "nsIEnumerator.h"
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsIContent.h"
#include "nsIDOMText.h"
#include "nsISupportsArray.h"

#include "nsCOMPtr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


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

  static nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot);
  static nsCOMPtr<nsIContent> GetDeepLastChild(nsCOMPtr<nsIContent> aRoot);
  
  nsresult GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  nsresult GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  
  nsresult NextNode(nsCOMPtr<nsIContent> *ioNextNode);
  nsresult PrevNode(nsCOMPtr<nsIContent> *ioPrevNode);

  void MakeEmpty();
  
  nsCOMPtr<nsIContent> mCurNode;
  nsCOMPtr<nsIContent> mFirst;
  nsCOMPtr<nsIContent> mLast;
  nsCOMPtr<nsIContent> mCommonParent;

  PRBool mIsDone;
  PRBool mPre;
  
private:

  // no copy's or assigns  FIX ME
  nsContentIterator(const nsContentIterator&);
  nsContentIterator& operator=(const nsContentIterator&);

};

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);


/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentIterator();
  return iter->QueryInterface(nsIContentIterator::GetIID(), (void**) aInstancePtrResult);
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
/*  if (aIID.Equals(nsIEnumerator::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }  */
  if (aIID.Equals(nsIContentIterator::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
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

  nsCOMPtr<nsIContent> root( do_QueryInterface(aRoot) );
  mFirst = GetDeepFirstChild(root); 
  mLast = root;
  mCommonParent = root;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIDOMNode> dN;
  // get common content parent
  if (!NS_SUCCEEDED(aRange->GetCommonParent(getter_AddRefs(dN))) || !dN)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(dN);

  // get the start node and offset, convert to nsIContent
  aRange->GetStartParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = do_QueryInterface(dN);
  if (!cN) 
    return NS_ERROR_FAILURE;
  
  PRInt32 indx;
  aRange->GetStartOffset(&indx);
  
  // find first node in range
  nsCOMPtr<nsIContent> cChild;
  cN->ChildAt(0,*getter_AddRefs(cChild));
  
  if (!cChild) // no children, must be a text node
  {
    mFirst = cN; 
  }
  else
  {
    cN->ChildAt(indx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, parent is first node
    {
      mFirst = cN;
    }
    else
    {
      mFirst = GetDeepFirstChild(cChild);
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
  
  aRange->GetEndParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = do_QueryInterface(dN);
  if (!cN) 
    return NS_ERROR_FAILURE;

  aRange->GetEndOffset(&indx);
  
  // find last node in range
  cN->ChildAt(0,*getter_AddRefs(cChild));

  if (!cChild) // no children, must be a text node
  {
    mLast = cN; 
  }
  else if (indx == 0) // before first child, parent is last node
  {
    mLast = cN; 
  }
  else
  {
    cN->ChildAt(--indx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, last child is last node
    {
      cN->ChildCount(indx);
      cN->ChildAt(--indx,*getter_AddRefs(cChild)); 
      if (!cChild)
      {
        NS_NOTREACHED("nsContentIterator::nsContentIterator");
        return NS_ERROR_FAILURE; 
      }
    }
    mLast = cChild;  
  }
  
  mCurNode = mFirst;
  return NS_OK;
}


/******************************************************
 * Helper routines
 ******************************************************/

void nsContentIterator::MakeEmpty()
{
  nsCOMPtr<nsIContent> noNode;
  mCurNode = noNode;
  mFirst = noNode;
  mLast = noNode;
  mCommonParent = noNode;
  mIsDone = PR_TRUE;
}

nsCOMPtr<nsIContent> nsContentIterator::GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot)
{
  nsCOMPtr<nsIContent> deepFirstChild;
  
  if (aRoot) 
  {  
    nsCOMPtr<nsIContent> cN = aRoot;
    nsCOMPtr<nsIContent> cChild;
    cN->ChildAt(0,*getter_AddRefs(cChild));
    while ( cChild )
    {
      cN = cChild;
      cN->ChildAt(0,*getter_AddRefs(cChild));
    }
    deepFirstChild = cN;
  }
  
  return deepFirstChild;
}

nsCOMPtr<nsIContent> nsContentIterator::GetDeepLastChild(nsCOMPtr<nsIContent> aRoot)
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
nsresult nsContentIterator::GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (!NS_SUCCEEDED(aNode->GetParent(*getter_AddRefs(parent))))
    return NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(parent->IndexOf(aNode, indx)))
    return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(parent->ChildAt(++indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
  }
  else if (parent != mCommonParent)
  {
    return GetNextSibling(parent, aSibling);
  }
  else
  {
    *aSibling = nsCOMPtr<nsIContent>();
  }
  
  return NS_OK;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsresult nsContentIterator::GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (!NS_SUCCEEDED(aNode->GetParent(*getter_AddRefs(parent))))
    return NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(parent->IndexOf(aNode, indx)))
    return NS_ERROR_FAILURE;

  if (indx && NS_SUCCEEDED(parent->ChildAt(--indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
  }
  else if (parent != mCommonParent)
  {
    return GetPrevSibling(parent, aSibling);
  }
  else
  {
    *aSibling = nsCOMPtr<nsIContent>();
  }
  
  return NS_OK;
}

nsresult nsContentIterator::NextNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cFirstChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then next node is first child
    if (numChildren)
    {
      cN->ChildAt(0,*getter_AddRefs(cFirstChild)); 
      if (!cFirstChild)
        return NS_ERROR_FAILURE;
      *ioNextNode = cFirstChild;
      return NS_OK;
    }
  
    // else next sibling is next
    return GetNextSibling(cN, ioNextNode);
  }
  else  // post-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get next sibling if there is one
    if (!NS_SUCCEEDED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!NS_SUCCEEDED(parent->IndexOf(cN, indx)))
      return NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(parent->ChildAt(++indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // next node is siblings "deep left" child
      *ioNextNode = GetDeepFirstChild(cSibling); 
      return NS_OK;
    }
  
    // else it's the parent
    *ioNextNode = parent;
  }
  return NS_OK;
}

nsresult nsContentIterator::PrevNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
   
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get prev sibling if there is one
    if (!NS_SUCCEEDED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!NS_SUCCEEDED(parent->IndexOf(cN, indx)))
      return NS_ERROR_FAILURE;
    if (indx && NS_SUCCEEDED(parent->ChildAt(--indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // prev node is siblings "deep right" child
      *ioNextNode = GetDeepLastChild(cSibling); 
      return NS_OK;
    }
  
    // else it's the parent
    *ioNextNode = parent;
  }
  else  // post-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cLastChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then prev node is last child
    if (numChildren)
    {
      cN->ChildAt(--numChildren,*getter_AddRefs(cLastChild)); 
      if (!cLastChild)
        return NS_ERROR_FAILURE;
      *ioNextNode = cLastChild;
      return NS_OK;
    }
  
    // else prev sibling is previous
    return GetPrevSibling(cN, ioNextNode);
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
    return NS_ERROR_FAILURE;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  
  return NextNode(&mCurNode);
}


nsresult nsContentIterator::Prev()
{
  if (mIsDone) 
    return NS_ERROR_FAILURE;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  
  return PrevNode(&mCurNode);
}


nsresult nsContentIterator::IsDone()
{
  if (mIsDone) 
    return NS_OK;
  else 
    return NS_COMFALSE;
}


nsresult nsContentIterator::PositionAt(nsIContent* aCurNode)
{
  // XXX need to confirm that aCurNode is within range
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;
  mCurNode = do_QueryInterface(aCurNode);
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
  return mCurNode->QueryInterface(nsIContent::GetIID(), (void**) aNode);
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
};

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);




/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentSubtreeIterator();
  return iter->QueryInterface(nsIContentIterator::GetIID(), (void**) aInstancePtrResult);
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

  // get common content parent
  if (!NS_SUCCEEDED(aRange->GetCommonParent(getter_AddRefs(commonParent))) || !commonParent)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(commonParent);

  // get start content parent
  if (!NS_SUCCEEDED(aRange->GetStartParent(getter_AddRefs(startParent))) || !startParent)
    return NS_ERROR_FAILURE;
  cStartP = do_QueryInterface(startParent);

  // get end content parent
  if (!NS_SUCCEEDED(aRange->GetEndParent(getter_AddRefs(endParent))) || !endParent)
    return NS_ERROR_FAILURE;
  cEndP = do_QueryInterface(endParent);
  
  
  // find first node in range
  PRInt32 indx;
  aRange->GetStartOffset(&indx);
  nsCOMPtr<nsIContent> cChild;
  PRInt32 numChildren;
  cStartP->ChildCount(numChildren);
  
  if (!numChildren) // no children, must be a text node
  {
    cN = cStartP; 
  }
  else
  {
    cStartP->ChildAt(indx,*getter_AddRefs(cChild));
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
    if (!NS_SUCCEEDED(GetNextSibling(cN,  &firstCandidate)))
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  firstCandidate = GetDeepFirstChild(firstCandidate);
  
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  PRBool nodeBefore, nodeAfter;
  if (!NS_SUCCEEDED(CompareNodeToRange(firstCandidate, aRange, &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;
  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the first node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (!NS_SUCCEEDED(GetTopAncestorInRange(firstCandidate, &mFirst)))
    return NS_ERROR_FAILURE;
  
  
  
  // now to find the last node
  aRange->GetEndOffset(&indx);
  if (!indx)
  {
    cN = cEndP;
  }
  else
  {
    cEndP->ChildCount(numChildren);
  
    if (!numChildren) // no children, must be a text node
    {
      cN = cEndP; 
    }
    else
    {
      cEndP->ChildAt(--indx,*getter_AddRefs(cChild));
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
    if (!NS_SUCCEEDED(GetPrevSibling(cN, &lastCandidate)))
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  lastCandidate = GetDeepLastChild(lastCandidate);
  
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  if (!NS_SUCCEEDED(CompareNodeToRange(lastCandidate, aRange, &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;
  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the last node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (!NS_SUCCEEDED(GetTopAncestorInRange(lastCandidate, &mLast)))
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
    return NS_ERROR_FAILURE;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIContent> nextNode;
  if (!NS_SUCCEEDED(GetNextSibling(mCurNode, &nextNode)))
    return NS_ERROR_FAILURE;
  nextNode = GetDeepFirstChild(nextNode);
  return GetTopAncestorInRange(nextNode, &mCurNode);
}


nsresult nsContentSubtreeIterator::Prev()
{
  if (mIsDone) 
    return NS_ERROR_FAILURE;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIContent> prevNode;
  prevNode = GetDeepFirstChild(mCurNode);
  if (!NS_SUCCEEDED(PrevNode(&prevNode)))
    return NS_ERROR_FAILURE;
  prevNode = GetDeepLastChild(prevNode);
  return GetTopAncestorInRange(prevNode, &mCurNode);
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
  if (!NS_SUCCEEDED(CompareNodeToRange(aNode, mRange, &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;
  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContent> parent;
  while (aNode)
  {
    if (!NS_SUCCEEDED(aNode->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!NS_SUCCEEDED(CompareNodeToRange(parent, mRange, &nodeBefore, &nodeAfter)))
      return NS_ERROR_FAILURE;
    if (nodeBefore || nodeAfter)
    {
      *outAnestor = aNode;
      return NS_OK;
    }
    aNode = parent;
  }
  return NS_ERROR_FAILURE;
}




