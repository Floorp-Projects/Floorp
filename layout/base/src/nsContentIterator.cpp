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
#include "nsIDOMRange.h"
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
  
  nsCOMPtr<nsIContent> cParent;
  nsCOMPtr<nsIContent> cSibling;
  
  mCurNode->GetParent(*getter_AddRefs(cParent));
  // no parent and we are not done?  something is wrong
  if (!cParent) 
    return NS_ERROR_UNEXPECTED; 
  
  // get next sibling
  PRInt32 indx;
  cParent->IndexOf(mCurNode,indx);
  indx++;
  cParent->ChildAt(indx,*getter_AddRefs(cSibling));
  if (!cSibling)
  {
    // curent node has no next sibling, so parent is next node
    mCurNode = cParent;
    return NS_OK;
  }
  
  // else next node is siblings "deep left"  child
  mCurNode = GetDeepFirstChild(cSibling); 
  return NS_OK;
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
  return NS_ERROR_NOT_IMPLEMENTED;
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

