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
 */

#include "nsContentIterator.h"
#include "nsIDOMRange.h"
#include "nsIContent.h"
#include "nsIDOMText.h"

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

nsIContent* nsContentIterator::GetDeepFirstChild(nsIContent* aRoot)
{
  if (!aRoot) return nsnull;
  nsIContent *cN = aRoot;
  nsIContent *cChild;
  cN->ChildAt(0,cChild);
  while ( cChild )
  {
    NS_IF_RELEASE(cN); // balance addref inside ChildAt()
    cN = cChild;
    cN->ChildAt(0,cChild);
  }
  NS_ADDREF(cN); 
  return cN;
}


nsContentIterator::nsContentIterator(nsIContent* aRoot)
{
  mCurNode = nsnull;
  mFirst = nsnull;
  mLast = nsnull;
  
  if (!aRoot)
  {
    NS_NOTREACHED("nsContentIterator::nsContentIterator");
    return; 
  }

  mFirst = GetDeepFirstChild(aRoot); // already addref'd
  mLast = aRoot;
  NS_ADDREF(mLast); 
  mCurNode = mFirst;
}


nsContentIterator::nsContentIterator(nsIDOMRange* aRange)
{
  mCurNode = nsnull;
  mFirst = nsnull;
  mLast = nsnull;
  
  if (!aRange)
  {
    NS_NOTREACHED("nsContentIterator::nsContentIterator");
    return; 
  }

  // get the start node and offset, convert to nsIContent
  nsIContent *cN;
  nsIDOMNode *dN;
  aRange->GetStartParent(&dN);
  nsresult res = dN->QueryInterface(kIContentIID, (void**)&cN);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsContentIterator::nsContentIterator");
    NS_IF_RELEASE(dN);
    return; 
  }
  NS_IF_RELEASE(dN);
  PRInt32 indx;
  aRange->GetStartOffset(&indx);
  
  if (!cN)
  {
    NS_NOTREACHED("nsContentIterator::nsContentIterator");
    return; 
  }

  // find first node in range
  nsIContent *cChild;
  cN->ChildAt(0,cChild);
  if (!cChild) // no children, must be a text node
  {
    mFirst = cN; // already addref'd
  }
  else
  {
    cN->ChildAt(indx,cChild);
    if (!cChild)  // offset after last child, parent is first node
    {
      mFirst = cN; // already addref'd
    }
    else
    {
      mFirst = GetDeepFirstChild(cChild);  // already addref'd
      NS_IF_RELEASE(cChild);
    }
  }
  
  aRange->GetEndParent(&dN);
  res = dN->QueryInterface(kIContentIID, (void**)&cN);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsContentIterator::nsContentIterator");
    NS_IF_RELEASE(dN);
    return; 
  }
  NS_IF_RELEASE(dN);
  aRange->GetEndOffset(&indx);
  
  cN->ChildAt(0,cChild);
  if (!cChild) // no children, must be a text node
  {
    mLast = cN;  // already addref'd
  }
  else if (indx == 0) // before first child, parent is last node
  {
    mLast = cN;  // already addref'd
  }
  else
  {
    NS_IF_RELEASE(cChild);  // addref'd in previous ChildAt()
    cN->ChildAt(indx,cChild);
    if (!cChild)  // offset after last child, last child is last node
    {
      cN->ChildCount(indx);
      cN->ChildAt(indx,cChild);
      if (!cChild)
      {
        NS_NOTREACHED("nsContentIterator::nsContentIterator");
        return; 
      }
      mLast = cChild;  // already addref'd
    }
    else // child just before indx is last node
    {
      --indx;
      cN->ChildAt(indx,cChild);
      mLast = cChild;  // already addref'd
    }
  }
  
  mCurNode = mFirst;
  NS_ADDREF(mCurNode);
}


nsContentIterator::~nsContentIterator()
{
  NS_IF_RELEASE(mCurNode);
  NS_IF_RELEASE(mFirst);
  NS_IF_RELEASE(mLast);
}


nsresult nsContentIterator::First()
{
  if (!mFirst) return NS_ERROR_FAILURE;
  if (mFirst == mCurNode) return NS_OK;
  NS_IF_RELEASE(mCurNode);
  mCurNode = mFirst;
  NS_ADDREF(mCurNode);
  return NS_OK;
}


nsresult nsContentIterator::Last()
{
  if (!mLast) return NS_ERROR_FAILURE;
  if (mLast == mCurNode) return NS_OK;
  NS_IF_RELEASE(mCurNode);
  mCurNode = mLast;
  NS_ADDREF(mCurNode);
  return NS_OK;
}


nsresult nsContentIterator::Next()
{
  if (!mCurNode) return NS_OK;
  if (mCurNode == mLast) return NS_ERROR_FAILURE;
  
  nsIContent *cParent;
  nsIContent *cSibling;
  
  mCurNode->GetParent(cParent);
  // no parent and we are not done?  something is wrong
  if (!cParent)
  {
    NS_NOTREACHED("nsContentIterator::Next");
    NS_IF_RELEASE(cParent);
    return NS_ERROR_UNEXPECTED; 
  }
  // get next sibling
  PRInt32 indx;
  cParent->IndexOf(mCurNode,indx);
  indx++;
  cParent->ChildAt(indx,cSibling);
  if (!cSibling)
  {
    // curent node has no next sibling, so parent is next node
    mCurNode = cParent;
    NS_IF_RELEASE(cParent);
    return NS_OK;
  }
  NS_IF_RELEASE(cParent);
  
  // else next node is siblings "deep left"  child
  NS_IF_RELEASE(mCurNode);
  mCurNode = GetDeepFirstChild(cSibling); // addref happened in GetDeepFirstChild()
  NS_IF_RELEASE(cSibling);
  return NS_OK;
}


nsresult nsContentIterator::Prev()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsContentIterator::CurrentNode(nsIContent **aNode)
{
  if (!mCurNode) return NS_ERROR_FAILURE;
  *aNode = mCurNode;
  NS_ADDREF(*aNode);
  return NS_OK;
}


nsresult nsContentIterator::IsDone()
{
  if (mCurNode == mLast) return NS_OK;
  else return NS_COMFALSE;
}

