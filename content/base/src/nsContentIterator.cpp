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

  // nsIEnumertor interface methods ------------------------------
  
  //NS_IMETHOD CurrentItem(nsISupports **aItem);

protected:

  static nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot);
  static nsresult NextNode(nsCOMPtr<nsIContent> *ioNextNode);
  static nsresult PrevNode(nsCOMPtr<nsIContent> *ioPrevNode);

  nsCOMPtr<nsIContent> mCurNode;
  nsCOMPtr<nsIContent> mFirst;
  nsCOMPtr<nsIContent> mLast;

  PRBool mIsDone;
  
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
  return iter->QueryInterface(nsIContentIterator::IID(), (void**) aInstancePtrResult);
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
/*  if (aIID.Equals(nsIEnumerator::IID())) 
  {
    *aInstancePtrResult = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }  */
  if (aIID.Equals(nsIContentIterator::IID())) 
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
  mCurNode(nsnull),
  mFirst(nsnull),
  mLast(nsnull),
  mIsDone(PR_FALSE)
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

  nsCOMPtr<nsIContent> root(aRoot);
  mFirst = GetDeepFirstChild(root); 
  mLast = aRoot;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  // get the start node and offset, convert to nsIContent
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIDOMNode> dN;
  aRange->GetStartParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = dN;
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
    // not return a first or last node, and always answer 
    // 'true' to IsDone()
  
    if (!IsNodeIntersectsRange(mFirst, aRange))
    {
      nsCOMPtr<nsIContent> noNode;
      mCurNode = noNode;
      mFirst = noNode;
      mLast = noNode;
      mIsDone = PR_TRUE;
      return NS_OK;
    }
  }
  
  aRange->GetEndParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = dN;
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

nsCOMPtr<nsIContent> nsContentIterator::GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot)
{
  if (!aRoot) 
    return aRoot;
  nsCOMPtr<nsIContent> cN = aRoot;
  nsCOMPtr<nsIContent> cChild;
  cN->ChildAt(0,*getter_AddRefs(cChild));
  while ( cChild )
  {
    cN = cChild;
    cN->ChildAt(0,*getter_AddRefs(cChild));
  }
  return cN;
}

nsresult nsContentIterator::NextNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIContent> cN = *ioNextNode;
  nsCOMPtr<nsIContent> cParent;
  nsCOMPtr<nsIContent> cSibling;
  
  cN->GetParent(*getter_AddRefs(cParent));
  // no parent? then no next node!
  if (!cParent) 
    return NS_ERROR_FAILURE; 
  
  // get next sibling
  PRInt32 indx;
  cParent->IndexOf(cN,indx);
  indx++;
  cParent->ChildAt(indx,*getter_AddRefs(cSibling));
  if (!cSibling)
  {
    // curent node has no next sibling, so parent is next node
    *ioNextNode = cParent;
    return NS_OK;
  }
  
  // else next node is siblings "deep left"  child
  *ioNextNode = GetDeepFirstChild(cSibling); 
  return NS_OK;
}

nsresult nsContentIterator::PrevNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIContent> cN = *ioNextNode; 
  PRInt32 numChildren;
  if (!NS_SUCCEEDED(cN->ChildCount(numChildren)))
    return NS_ERROR_FAILURE;
    
  // if it has children, prev is last child
  if (numChildren)
  {
    cN->ChildAt(--numChildren, *getter_AddRefs(*ioNextNode));
    return NS_OK;
  }
  
  // else the prev node is the previous sibling of this node,
  // or prev sibling of it's parent, or prev sibling of it's grandparent,
  // or.... you get the idea.
  nsCOMPtr<nsIContent> cParent; 
  PRInt32 cIndex;
  cN->GetParent(*getter_AddRefs(cParent));
  while (cParent)  
  {
    cParent->IndexOf(cN, cIndex);
    if (cIndex>1)   // node has a prev sibling, that's what we want
    {
      cIndex -= 2;  // one for the 0 based counting, and one to get to prev sibling
      cParent->ChildAt(cIndex, *getter_AddRefs(*ioNextNode));
      return NS_OK;
    }
    cN = cParent;
    cN->GetParent(*getter_AddRefs(cParent));
  }
  
  return  NS_ERROR_FAILURE; // there was no prev node
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

nsresult nsContentIterator::First()
{
  if (!mFirst) 
    return NS_ERROR_FAILURE;
  mIsDone = false;
  if (mFirst == mCurNode) 
    return NS_OK;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsContentIterator::Last()
{
  if (!mLast) 
    return NS_ERROR_FAILURE;
  mIsDone = false;
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
    mIsDone = true;
    return NS_ERROR_FAILURE;
  }
  
  return NextNode(&mCurNode);
}


nsresult nsContentIterator::Prev()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsContentIterator::IsDone()
{
  if (mIsDone) 
    return NS_OK;
  else 
    return NS_COMFALSE;
}


nsresult nsContentIterator::CurrentNode(nsIContent **aNode)
{
  if (!mCurNode) 
    return NS_ERROR_FAILURE;
  if (mIsDone) 
    return NS_ERROR_FAILURE;
  return mCurNode->QueryInterface(nsIContent::IID(), (void**) aNode);
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

private:

  // no copy's or assigns  FIX ME
  nsContentSubtreeIterator(const nsContentSubtreeIterator&);
  nsContentSubtreeIterator& operator=(const nsContentSubtreeIterator&);

};

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);

/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentSubtreeIterator();
  return iter->QueryInterface(nsIContentIterator::IID(), (void**) aInstancePtrResult);
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

  // get the start node and offset, convert to nsIContent
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIDOMNode> dN;
  aRange->GetStartParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = dN;
  if (!cN) 
    return NS_ERROR_FAILURE;
  
  PRInt32 indx;
  aRange->GetStartOffset(&indx);
  
  // find first node partially in range
  nsCOMPtr<nsIContent> cCandidateFirst;
  nsCOMPtr<nsIContent> cChild;
  cN->ChildAt(0,*getter_AddRefs(cChild));
  
  if (!cChild) // no children, must be a text node
  {
    cCandidateFirst = cN;

  }
  else
  {
    cN->ChildAt(indx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, parent is first node
    {
      cCandidateFirst = cN;
    }
    else
    {
      cCandidateFirst = cChild;
    }
  }
  
  // now walk the tree until we find a node completely contained
  // that's our 'real' first node
  PRBool isBefore, isAfter;
  if (!NS_SUCCEEDED(CompareNodeToRange(cCandidateFirst, aRange, &isBefore, &isAfter)))
    return NS_ERROR_FAILURE;
  
  PRBool isDone = isBefore || isAfter;  // if cCandidateFirst is inside, we're done
  while (!isDone)
  {
    if (isAfter)  // if cCandidateFirst is after the range, we fall out and
      break;      // set up an empty iterator
    if (!NS_SUCCEEDED(NextNode(&cCandidateFirst)))
      break;      // hit end of tree - set up an empty iterator
    if (!NS_SUCCEEDED(CompareNodeToRange(cCandidateFirst, aRange, &isBefore, &isAfter)))
      return NS_ERROR_FAILURE;
  }
  if (isBefore || isAfter)  // set up an empty iterator
  {
    nsCOMPtr<nsIContent> noNode;
    mCurNode = noNode;
    mFirst = noNode;
    mLast = noNode;
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  else
  {
    mFirst = cCandidateFirst;
  }
  
 
  // Now do something similar to find the last node
  aRange->GetEndParent(getter_AddRefs(dN));
  if (!dN) 
    return NS_ERROR_ILLEGAL_VALUE;
  cN = dN;
  if (!cN) 
    return NS_ERROR_FAILURE;

  aRange->GetEndOffset(&indx);
  
  // find last node partially in range
  nsCOMPtr<nsIContent> cCandidateLast;
  cN->ChildAt(0,*getter_AddRefs(cChild));

  if (!cChild) // no children, must be a text node
  {
    cCandidateLast = cN; 
  }
  else if (indx == 0) // before first child, parent is last node
  {
    cCandidateLast = cN; 
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
    cCandidateLast = cChild;  
  }

  // now walk the tree until we find a node completely contained
  // that's our 'real' last node
  if (!NS_SUCCEEDED(CompareNodeToRange(cCandidateLast, aRange, &isBefore, &isAfter)))
    return NS_ERROR_FAILURE;
  
  isDone = isBefore || isAfter;  // if cCandidateLast is inside, we're done
  while (!isDone)
  {
    if (isBefore)  // if cCandidateLast is before the range, panic
      return NS_ERROR_UNEXPECTED;  // how could we find a beginning, but no end?
    if (!NS_SUCCEEDED(PrevNode(&cCandidateLast)))
      break;      // hit end of tree - set up an empty iterator
    if (!NS_SUCCEEDED(CompareNodeToRange(cCandidateLast, aRange, &isBefore, &isAfter)))
      return NS_ERROR_FAILURE;
  }

  mLast = cCandidateLast;
  mCurNode = mFirst;
  return NS_OK;
}


/******************************************************
 *  Helper routines
 ******************************************************/


/****************************************************************
 * nsContentSubtreeIterator overrides of ContentIterator routines
 ****************************************************************/

nsresult nsContentSubtreeIterator::Next()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsContentSubtreeIterator::Prev()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

