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
 * nsRange.cpp: Implementation of the nsIDOMRange object.
 */

#include "nsIDOMRange.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsVoidArray.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

class nsRange : public nsIDOMRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);
  NS_IMETHOD    SetIsPositioned(PRBool aIsPositioned);

  NS_IMETHOD    GetStartParent(nsIDOMNode** aStartParent);
  NS_IMETHOD    SetStartParent(nsIDOMNode* aStartParent);

  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);
  NS_IMETHOD    SetStartOffset(PRInt32 aStartOffset);

  NS_IMETHOD    GetEndParent(nsIDOMNode** aEndParent);
  NS_IMETHOD    SetEndParent(nsIDOMNode* aEndParent);

  NS_IMETHOD    GetEndOffset(PRInt32* aEndOffset);
  NS_IMETHOD    SetEndOffset(PRInt32 aEndOffset);

  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);
  NS_IMETHOD    SetIsCollapsed(PRBool aIsCollapsed);

  NS_IMETHOD    GetCommonParent(nsIDOMNode** aCommonParent);
  NS_IMETHOD    SetCommonParent(nsIDOMNode* aCommonParent);

  NS_IMETHOD    SetStart(nsIDOMNode* aParent, PRInt32 aOffset);

  NS_IMETHOD    SetEnd(nsIDOMNode* aParent, PRInt32 aOffset);

  NS_IMETHOD    Collapse(PRBool aToStart);

  NS_IMETHOD    Unposition();

  NS_IMETHOD    SelectNode(nsIDOMNode* aN);

  NS_IMETHOD    SelectNodeContents(nsIDOMNode* aN);

  NS_IMETHOD    DeleteContents();

  NS_IMETHOD    ExtractContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    CopyContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    InsertNode(nsIDOMNode* aN);

  NS_IMETHOD    SurroundContents(nsIDOMNode* aN);

  NS_IMETHOD    Clone(nsIDOMRange** aReturn);

  NS_IMETHOD    ToString(nsString& aReturn);

private:
  PRBool mIsPositioned;
  nsIDOMNode* mStartParent;
  PRInt32 mStartOffset;
  nsIDOMNode* mEndParent;
  PRInt32 mEndOffset;

  PRBool        IsIncreasing(nsIDOMNode* sParent, PRInt32 sOffset,
                       nsIDOMNode* eParent, PRInt32 eOffset);
                       
  PRBool        IsPointInRange(nsIDOMNode* pParent, PRInt32 pOffset);
  
  PRInt32       IndexOf(nsIDOMNode* aNode);
  
  PRInt32       FillArrayWithAncestors(nsVoidArray* aArray,nsIDOMNode* aNode);
  
  nsIDOMNode*   CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2);

};

nsresult
NS_NewRange(nsIDOMRange** aInstancePtrResult)
{
  nsRange * range = new nsRange();
  return range->QueryInterface(kIRangeIID, (void**) aInstancePtrResult);
}

/******************************************************
 * constructor/destructor
 ******************************************************/
 
nsRange::nsRange() {
  NS_INIT_REFCNT();

  mIsPositioned = PR_FALSE;
  nsIDOMNode* mStartParent = NULL;
  PRInt32 mStartOffset = 0;
  nsIDOMNode* mEndParent = NULL;
  PRInt32 mEndOffset = 0;
} 

nsRange::~nsRange() {
} 

/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)

nsresult nsRange::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIRangeIID)) {
    *aInstancePtrResult = (void*)(nsIDOMRange*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}


/******************************************************
 * Private helper routines
 ******************************************************/
 
PRBool nsRange::IsIncreasing(nsIDOMNode* sParent, PRInt32 sOffset,
                              nsIDOMNode* eParent, PRInt32 eOffset)
{
  // XXX NEED IMPLEMENTATION!
  return PR_TRUE;
}

PRBool nsRange::IsPointInRange(nsIDOMNode* pParent, PRInt32 pOffset)
{
  // XXX NEED IMPLEMENTATION!
  return PR_TRUE;
}
  
PRInt32 nsRange::IndexOf(nsIDOMNode* aChildNode)
{
  nsIDOMNode *parentNode;
  nsIContent *contentChild;
  nsIContent *contentParent;
  PRInt32    theIndex = NULL;
  
  // lots of error checking for now
  if (!aChildNode)
  {
    NS_NOTREACHED("nsRange::IndexOf");
    return 0;
  }

  // get the parent node
  nsresult res = aChildNode->GetParentNode(&parentNode);
  if (!NS_SUCCEEDED(res))
  {
    NS_NOTREACHED("nsRange::IndexOf");
    return 0;
  }
  
  // convert node and parent to nsIContent, so that we can find the child index
  res = parentNode->QueryInterface(kIContentIID, (void**)&contentParent);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::IndexOf");
    return 0;
  }
  res = aChildNode->QueryInterface(kIContentIID, (void**)&contentChild);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::IndexOf");
    return 0;
  }
  
  // finally we get the index
  res = contentParent->IndexOf(contentChild,theIndex);  // why is theIndex passed by reference?  why hide that theIndex is changing?
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::IndexOf");
    return 0;
  }
  return theIndex;
}

PRInt32 nsRange::FillArrayWithAncestors(nsVoidArray* aArray,nsIDOMNode* aNode)
{
  PRInt32    i=0;
  nsresult   res;
  nsIDOMNode *node = aNode;
  
  // callers responsibility to make sure args are non-null and proper type
  
  // insert node itself
  aArray->InsertElementAt((void*)node,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  while(1)
  {
    ++i;
    res = node->GetParentNode(&node);
  	// should be able to remove this error check later
    if (!NS_SUCCEEDED(res))
    {
      NS_NOTREACHED("nsRange::FillArrayWithAncestors");
      return -1;  // poor man's error code
    }
    if (!node) break;
  }
  
  return i;
}

nsIDOMNode* nsRange::CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
{
  nsIDOMNode  *theParent = nsnull;
  
  // no null nodes please
  if (!aNode1 || !aNode2)
  {
    NS_NOTREACHED("nsRange::CommonParent");
    return nsnull;
  }
  
  // shortcut for common case - both nodes are the same
  if (aNode1 == aNode2)
  {
  	// should be able to remove this error check later
    nsresult res = aNode1->GetParentNode(&theParent);
    if (!NS_SUCCEEDED(res))
    {
      NS_NOTREACHED("nsRange::CommonParent");
      return nsnull;
    }
    return theParent;
  }

  // otherwise traverse the tree for the common ancestor
  // For now, a pretty dumb hack on computing this
  nsVoidArray *array1 = new nsVoidArray();
  nsVoidArray *array2 = new nsVoidArray();
  PRInt32     i=0, j=0;
  
  // out of mem? out of luck!
  if (!array1 || !array2)
  {
    NS_NOTREACHED("nsRange::CommonParent");
      goto errorLabel;
  }
  
  // get ancestors of each node
  i = FillArrayWithAncestors(array1,aNode1);
  j = FillArrayWithAncestors(array2,aNode2);
  
  // sanity test (for now) - FillArrayWithAncestors succeeded
  if ((i==-1) || (j==-1))
  {
    NS_NOTREACHED("nsRange::CommonParent");
      goto errorLabel;
  }
  
  // sanity test (for now) - the end of each array
  // should match and be the root
  if (array1->ElementAt(i) != array2->ElementAt(j))
  {
    NS_NOTREACHED("nsRange::CommonParent");
      goto errorLabel;
  }
  
  // back through the ancestors, starting from the root, until
  // first different ancestor found.  
  while (array1->ElementAt(i) != array2->ElementAt(j))
  {
    --i;
    --j;
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  i++;
  theParent = (nsIDOMNode*)array1->ElementAt(i);
  
  return theParent;  
  
errorLabel:
  delete array1;
  delete array2;
  return nsnull;
}


/******************************************************
 * public functionality
 ******************************************************/

nsresult nsRange::GetIsPositioned(PRBool* aIsPositioned)
{
  *aIsPositioned = mIsPositioned;
  return NS_OK;
}

nsresult nsRange::GetStartParent(nsIDOMNode** aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartParent)
    return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(*aStartParent);
  NS_IF_ADDREF(mStartParent);
  *aStartParent = mStartParent;
  return NS_OK;
}

nsresult nsRange::SetStartParent(nsIDOMNode* aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(aStartParent, mStartOffset,
                    mEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_RELEASE(mStartParent);
  NS_IF_ADDREF(aStartParent);
  mStartParent = aStartParent;
  return NS_OK;
}

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartOffset)
    return NS_ERROR_NULL_POINTER;
  *aStartOffset = mStartOffset;
  return NS_OK;
}

nsresult nsRange::SetStartOffset(PRInt32 aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, aStartOffset,
                    mEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  mStartOffset = aStartOffset;
  return NS_OK;
}

nsresult nsRange::GetEndParent(nsIDOMNode** aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndParent)
    return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(*aEndParent);
  NS_IF_ADDREF(mEndParent);
  *aEndParent = mEndParent;
  return NS_OK;
}

nsresult nsRange::SetEndParent(nsIDOMNode* aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, mStartOffset,
                    aEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_RELEASE(mEndParent);
  NS_IF_ADDREF(aEndParent);
  mEndParent = aEndParent;
  return NS_OK;
}

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndOffset)
    return NS_ERROR_NULL_POINTER;
  *aEndOffset = mEndOffset;
  return NS_OK;
}

nsresult nsRange::SetEndOffset(PRInt32 aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, mStartOffset,
                    mEndParent, aEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  mEndOffset = aEndOffset;
  return NS_OK;
}

nsresult nsRange::GetIsCollapsed(PRBool* aIsCollapsed)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (mEndParent == 0 ||
      (mStartParent == mEndParent && mStartOffset == mEndOffset))
    *aIsCollapsed = PR_TRUE;
  else
    *aIsCollapsed = PR_FALSE;
  return NS_OK;
}

nsresult nsRange::GetCommonParent(nsIDOMNode** aCommonParent)
{ 
  *aCommonParent = CommonParent(mStartParent,mEndParent);
  return NS_OK;
}

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if (!mIsPositioned)
  {
    NS_IF_RELEASE(mEndParent);
    mEndParent = NULL;
    mEndOffset = NULL;
    mIsPositioned = PR_TRUE;
  }
  NS_IF_ADDREF(aParent);
  mStartParent = aParent;
  mStartOffset = aOffset;
  return NS_OK;
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;  // can't set end before start

  NS_IF_ADDREF(aParent);
  mEndParent = aParent;
  mEndOffset = aOffset;
  return NS_OK;
}

nsresult nsRange::Collapse(PRBool aToStart)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  if (aToStart)
  {
    NS_IF_RELEASE(mEndParent);
    NS_IF_ADDREF(mStartParent);
    mEndParent = mStartParent;
    mEndOffset = mStartOffset;
    return NS_OK;
  }
  else
  {
    NS_IF_RELEASE(mStartParent);
    NS_IF_ADDREF(mEndParent);
    mStartParent = mEndParent;
    mStartOffset = mEndOffset;
    return NS_OK;
  }
}

nsresult nsRange::Unposition()
{
  NS_IF_RELEASE(mStartParent);
  mStartParent = NULL;
  mStartOffset = 0;
  NS_IF_RELEASE(mEndParent);
  mEndParent = NULL;
  mEndOffset = 0;
  mIsPositioned = PR_FALSE;
  return NS_OK;
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  nsIDOMNode * parent;
  nsresult res = aN->GetParentNode(&parent);
  if (!NS_SUCCEEDED(res))
    return res;

  if (mIsPositioned)
    Unposition();
  NS_IF_ADDREF(parent);
  mStartParent = parent;
  mStartOffset = 0;      // XXX NO DIRECT WAY TO GET CHILD # OF THIS NODE!
  NS_IF_ADDREF(parent);
  mEndParent = parent;
  mEndOffset = mStartOffset;
  return NS_OK;
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  if (mIsPositioned)
    Unposition();

  NS_IF_ADDREF(aN);
  mStartParent = aN;
  mStartOffset = 0;
  NS_IF_ADDREF(aN);
  mEndParent = aN;
  mEndOffset = 0;        // WRONG!  SHOULD BE # OF LAST CHILD!
  return NS_OK;
}

nsresult nsRange::DeleteContents()
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::CopyContents(nsIDOMDocumentFragment** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::Clone(nsIDOMRange** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ToString(nsString& aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

//
// We don't actually want to allow setting this ...
// it should be set only by actually positioning the range.
//
nsresult nsRange::SetIsPositioned(PRBool aIsPositioned)
{ return NS_ERROR_NOT_IMPLEMENTED; }

//
// Various other things which we don't want to implement yet:
//
nsresult nsRange::SetIsCollapsed(PRBool aIsCollapsed)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetCommonParent(nsIDOMNode* aCommonParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

