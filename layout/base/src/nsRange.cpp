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
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsContentIterator.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);

class nsRange : public nsIDOMRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  // IsPositioned attribute disappeared from the dom spec
  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);

  NS_IMETHOD    GetStartParent(nsIDOMNode** aStartParent);
  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);

  NS_IMETHOD    GetEndParent(nsIDOMNode** aEndParent);
  NS_IMETHOD    GetEndOffset(PRInt32* aEndOffset);

  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);

  NS_IMETHOD    GetCommonParent(nsIDOMNode** aCommonParent);

  NS_IMETHOD    SetStart(nsIDOMNode* aParent, PRInt32 aOffset);
  NS_IMETHOD    SetStartBefore(nsIDOMNode* aSibling);
  NS_IMETHOD    SetStartAfter(nsIDOMNode* aSibling);

  NS_IMETHOD    SetEnd(nsIDOMNode* aParent, PRInt32 aOffset);
  NS_IMETHOD    SetEndBefore(nsIDOMNode* aSibling);
  NS_IMETHOD    SetEndAfter(nsIDOMNode* aSibling);

  NS_IMETHOD    Collapse(PRBool aToStart);

  NS_IMETHOD    Unposition();

  NS_IMETHOD    SelectNode(nsIDOMNode* aN);
  NS_IMETHOD    SelectNodeContents(nsIDOMNode* aN);

  NS_IMETHOD    CompareEndPoints(PRUint16 how, nsIDOMRange* srcRange, PRInt32* ret);

  NS_IMETHOD    DeleteContents();

  NS_IMETHOD    ExtractContents(nsIDOMDocumentFragment** aReturn);
  NS_IMETHOD    CloneContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    InsertNode(nsIDOMNode* aN);
  NS_IMETHOD    SurroundContents(nsIDOMNode* aN);

  NS_IMETHOD    Clone(nsIDOMRange** aReturn);

  NS_IMETHOD    ToString(nsString& aReturn);

private:
  PRBool       mIsPositioned;
  nsIDOMNode   *mStartParent;
  nsIDOMNode   *mEndParent;
  PRInt32      mStartOffset;
  PRInt32      mEndOffset;
  nsVoidArray  *mStartAncestors;       // just keeping these around to avoid reallocing the arrays.
  nsVoidArray  *mEndAncestors;         // the contents of these arrays are discarded across calls.
  nsVoidArray  *mStartAncestorOffsets; //
  nsVoidArray  *mEndAncestorOffsets;   //

  // no copy's or assigns
  nsRange(const nsRange&);
  nsRange& operator=(const nsRange&);
  
  // helper routines
  
  static PRBool        InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  static PRInt32       IndexOf(nsIDOMNode* aNode);
  static PRInt32       FillArrayWithAncestors(nsVoidArray* aArray,nsIDOMNode* aNode);
  static nsIDOMNode*   CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  
  nsresult      DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset);

  PRBool        IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOff,
                             nsIDOMNode* aEndN, PRInt32 aEndOff);
                       
  nsresult      IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult);
  
  nsresult      ComparePointToRange(nsIDOMNode* aParent, PRInt32 aOffset, PRInt32* aResult);
  
  
  PRInt32       GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets);
  

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
 
nsRange::nsRange() 
{
  NS_INIT_REFCNT();

  mIsPositioned = PR_FALSE;
  mStartParent = nsnull;
  mStartOffset = 0;
  mEndParent = nsnull;
  mEndOffset = 0;
  mStartAncestors = new nsVoidArray();
  mStartAncestorOffsets = new nsVoidArray();
  mEndAncestors = new nsVoidArray();
  mEndAncestorOffsets = new nsVoidArray();
} 

nsRange::~nsRange() 
{
  delete mStartAncestors;
  delete mEndAncestors;
  delete mStartAncestorOffsets;
  delete mEndAncestorOffsets;
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

PRBool nsRange::InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
{
  nsresult res;
  nsIDOMDocument* document1;
  res = aNode1->GetOwnerDocument(&document1);
  if (!NS_SUCCEEDED(res))
    return PR_FALSE;

  nsIDOMDocument* document2;
  res = aNode2->GetOwnerDocument(&document2);
  if (!NS_SUCCEEDED(res))
  {
    NS_IF_RELEASE(document1);
    return PR_FALSE;
  }

  PRBool retval = PR_FALSE;

  // Now compare the two documents: is direct comparison safe?
  if (document1 == document2)
    retval = PR_TRUE;

  NS_IF_RELEASE(document1);
  NS_IF_RELEASE(document2);

  return retval;
}
  
nsresult nsRange::DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
{
  if (mStartParent != aStartN)
  {
    NS_IF_RELEASE(mStartParent);
    mStartParent = aStartN;
    NS_ADDREF(mStartParent);
  }
  mStartOffset = aStartOffset;
  if (mEndParent != aEndN)
  {
    NS_IF_RELEASE(mEndParent);
    mEndParent = aEndN;
    NS_ADDREF(mEndParent);
  }
  mEndOffset = aEndOffset;
  
  return NS_OK;
}


PRBool nsRange::IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
{
  PRInt32 numStartAncestors = 0;
  PRInt32 numEndAncestors = 0;
  PRInt32 commonNodeStartOffset = 0;
  PRInt32 commonNodeEndOffset = 0;
  
  // no trivial cases please
  if (!aStartN || !aEndN) return PR_FALSE;
  
  // check common case first
  if (aStartN==aEndN)
  {
    if (aStartOffset>aEndOffset) return PR_FALSE;
    else return PR_TRUE;
  }
  
  // refresh ancestor data
  mStartAncestors->Clear();
  mStartAncestorOffsets->Clear();
  mEndAncestors->Clear();
  mEndAncestorOffsets->Clear();

  numStartAncestors = GetAncestorsAndOffsets(aStartN,aStartOffset,mStartAncestors,mStartAncestorOffsets);
  numEndAncestors = GetAncestorsAndOffsets(aEndN,aEndOffset,mEndAncestors,mEndAncestorOffsets);
  
  --numStartAncestors; // adjusting for 0-based counting 
  --numEndAncestors; 
  // back through the ancestors, starting from the root, until first non-matching ancestor found
  while (mStartAncestors->ElementAt(numStartAncestors) == mEndAncestors->ElementAt(numEndAncestors))
  {
    --numStartAncestors;
    --numEndAncestors;
    if (numStartAncestors<0) break; // this will only happen if one endpoint's node is the common ancestor of the other
    if (numEndAncestors<0) break;
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  numStartAncestors++;
  numEndAncestors++;
  commonNodeStartOffset = (PRInt32)mStartAncestorOffsets->ElementAt(numStartAncestors);
  commonNodeEndOffset   = (PRInt32)mEndAncestorOffsets->ElementAt(numEndAncestors);
  
  if (commonNodeStartOffset>commonNodeEndOffset) return PR_FALSE;
  else  return PR_TRUE;
}

nsresult nsRange::IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult)
{
  PRInt32  compareResult = 0;
  nsresult res;
  res = ComparePointToRange(aParent, aOffset, &compareResult);
  if (compareResult) *aResult = PR_FALSE;
  else *aResult = PR_TRUE;
  return res;
}
  
// returns -1 if point is before range, 0 if point is in range, 1 if point is after range
nsresult nsRange::ComparePointToRange(nsIDOMNode* aParent, PRInt32 aOffset, PRInt32* aResult)
{
  // check arguments
  if (!aResult) return NS_ERROR_NULL_POINTER;
  
  // no trivial cases please
  if (!aParent) return NS_ERROR_NULL_POINTER;
  
  // our rnage is in a good state?
  if (!mIsPositioned) return NS_ERROR_NOT_INITIALIZED;
  
  // check common case first
  if ((aParent == mStartParent) && (aParent == mEndParent))
  {
    if (aOffset<mStartOffset)
    {
      *aResult = -1;
      return NS_OK;
    }
    if (aOffset>mEndOffset)
    {
      *aResult = 1;
      return NS_OK;
    }
    *aResult = 0;
    return NS_OK;
  }
  
  // ok, do it the hard way
  nsVoidArray *pointAncestors = new nsVoidArray();
  nsVoidArray *pointAncestorOffsets = new nsVoidArray();
  PRInt32     numPointAncestors = 0;
  PRInt32     numStartAncestors = 0;
  PRInt32     numEndAncestors = 0;
  PRInt32     commonNodeStartOffset = 0;
  PRInt32     commonNodeEndOffset = 0;
  
  if (!pointAncestors || !pointAncestorOffsets)
  {
    NS_NOTREACHED("nsRange::ComparePointToRange");
    delete pointAncestors;
    delete pointAncestorOffsets;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  // refresh ancestor data
  mStartAncestors->Clear();
  mStartAncestorOffsets->Clear();
  mEndAncestors->Clear();
  mEndAncestorOffsets->Clear();

  numStartAncestors = GetAncestorsAndOffsets(mStartParent,mStartOffset,mStartAncestors,mStartAncestorOffsets);
  numEndAncestors   = GetAncestorsAndOffsets(mEndParent,mEndOffset,mEndAncestors,mEndAncestorOffsets);
  numPointAncestors = GetAncestorsAndOffsets(aParent,aOffset,pointAncestors,pointAncestorOffsets);
  
  // walk down the arrays until we either find that the point is outside, or we hit the
  // end of one of the range endpoint arrays, in which case the point is inside.  If we
  // hit the end of the point array and NOT of the range endopint arrays, then the point
  // is before the range (a bit of a special case, that)
  
  --numStartAncestors; // adjusting for 0-based counting 
  --numEndAncestors; 
  --numPointAncestors;
  // back through the ancestors, starting from the root
  while (mStartAncestors->ElementAt(numStartAncestors) == pointAncestors->ElementAt(numEndAncestors))
  {
    if ((PRInt32)pointAncestorOffsets->ElementAt(numPointAncestors) < 
           (PRInt32)mStartAncestorOffsets->ElementAt(numStartAncestors))
    {
      *aResult = -1;
      break;
    }
    if ((PRInt32)pointAncestorOffsets->ElementAt(numPointAncestors) > 
           (PRInt32)mEndAncestorOffsets->ElementAt(numEndAncestors))
    {
      *aResult = 1;
      break;
    }
    
    --numStartAncestors;
    --numEndAncestors;
    --numPointAncestors;
    
    if ((numStartAncestors < 0) || (numEndAncestors < 0))
    {
      *aResult = 0;
      break;
    }
    if (numPointAncestors < 0)
    {
      *aResult = -1;
      break;
    }
  }

  // clean up
  delete pointAncestors;
  delete pointAncestorOffsets;
  
  return NS_OK;
}
  
PRInt32 nsRange::IndexOf(nsIDOMNode* aChildNode)
{
  nsIDOMNode *parentNode;
  nsIContent *contentChild;
  nsIContent *contentParent;
  PRInt32    theIndex = nsnull;
  
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
    NS_IF_RELEASE(contentParent);
    return 0;
  }
  res = aChildNode->QueryInterface(kIContentIID, (void**)&contentChild);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::IndexOf");
    NS_IF_RELEASE(contentParent);
    NS_IF_RELEASE(contentChild);
    return 0;
  }
  
  // finally we get the index
  res = contentParent->IndexOf(contentChild,theIndex); 
  NS_IF_RELEASE(contentParent);
  NS_IF_RELEASE(contentChild);
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
  nsIDOMNode *node = aNode;
  
  // callers responsibility to make sure args are non-null and proper type
  
  // insert node itself
  aArray->InsertElementAt((void*)node,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  node->GetParentNode(&node);  
  while(node)
  {
    ++i;
    aArray->InsertElementAt((void*)node,i);
    node->GetParentNode(&node);
  }
  
  return i;
}

PRInt32 nsRange::GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets)
{
  PRInt32    i=0;
  PRInt32    nodeOffset;
  nsresult   res;
  nsIContent *contentNode;
  nsIContent *contentParent;
  
  // callers responsibility to make sure args are non-null and proper type

  res = aNode->QueryInterface(kIContentIID, (void**)&contentNode);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::GetAncestorsAndOffsets");
    NS_IF_RELEASE(contentNode);
    return -1;  // poor man's error code
  }
  
  // insert node itself
  aAncestorNodes->InsertElementAt((void*)contentNode,i);
  aAncestorOffsets->InsertElementAt((void*)aOffset,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  contentNode->GetParent(contentParent);
  while(contentParent)
  {
    if (!contentParent) break;
    contentParent->IndexOf(contentNode, nodeOffset);
    ++i;
    aAncestorNodes->InsertElementAt((void*)contentParent,i);
    aAncestorOffsets->InsertElementAt((void*)nodeOffset,i);
    contentNode->GetParent(contentParent);
    NS_IF_RELEASE(contentNode);
    contentNode = contentParent;
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
  while (array1->ElementAt(i) == array2->ElementAt(j))
  {
    --i;
    --j;
    if (i<0) break; // this will only happen if one endpoint's node is the common ancestor of the other
    if (j<0) break;
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

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartOffset)
    return NS_ERROR_NULL_POINTER;
  *aStartOffset = mStartOffset;
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

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndOffset)
    return NS_ERROR_NULL_POINTER;
  *aEndOffset = mEndOffset;
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
  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;
  
  // must be in same document as endpoint, else 
  // endpoint is collapsed to new start.
  if (mIsPositioned && !InSameDoc(aParent,mEndParent))
  {
    res = DoSetRange(aParent,aOffset,aParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(aParent,aOffset,mEndParent,mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, end must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(aParent,aOffset,mEndParent,mEndOffset);
  return res;
}

nsresult nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  PRInt32 indx = IndexOf(aSibling);
  nsIDOMNode *nParent;
  aSibling->GetParentNode(&nParent);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  PRInt32 indx = IndexOf(aSibling) + 1;
  nsIDOMNode *nParent;
  aSibling->GetParentNode(&nParent);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;
  
  // must be in same document as startpoint, else 
  // endpoint is collapsed to new end.
  if (mIsPositioned && !InSameDoc(aParent,mEndParent))
  {
    res = DoSetRange(aParent,aOffset,aParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(mStartParent,mStartOffset,aParent,aOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, start must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(mStartParent,mStartOffset,aParent,aOffset);
  return res;
}

nsresult nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  PRInt32 indx = IndexOf(aSibling);
  nsIDOMNode *nParent;
  aSibling->GetParentNode(&nParent);
  return SetEnd(nParent,indx);
}

nsresult nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  PRInt32 indx = IndexOf(aSibling) + 1;
  nsIDOMNode *nParent;
  aSibling->GetParentNode(&nParent);
  return SetEnd(nParent,indx);
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
  mStartParent = nsnull;
  mStartOffset = 0;
  NS_IF_RELEASE(mEndParent);
  mEndParent = nsnull;
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
{ 
  nsIContent *cStart;
  nsIContent *cEnd;
  
  // get the content versions of our endpoints
  nsresult res = mStartParent->QueryInterface(kIContentIID, (void**)&cStart);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    return NS_ERROR_UNEXPECTED;
  }
  res = mEndParent->QueryInterface(kIContentIID, (void**)&cEnd);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    NS_IF_RELEASE(cStart);
    return NS_ERROR_UNEXPECTED;
  }
  
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    PRBool hasChildren;
    res = cStart->CanContainChildren(hasChildren);
    if (!NS_SUCCEEDED(res)) 
    {
      NS_NOTREACHED("nsRange::DeleteContents");
      NS_IF_RELEASE(cStart);
      NS_IF_RELEASE(cEnd);
      return res;
    }
    
    if (hasChildren)
    {
      PRInt32 i;  
      for (i=mEndOffset; --i; i>=mStartOffset)
      {
        res = cStart->RemoveChildAt(i, PR_TRUE);
        if (!NS_SUCCEEDED(res)) 
        {
          NS_NOTREACHED("nsRange::DeleteContents");
          NS_IF_RELEASE(cStart);
          NS_IF_RELEASE(cEnd);
          return res;
        }
      }
      NS_IF_RELEASE(cStart);
      NS_IF_RELEASE(cEnd);
      return NS_OK;
    }
    else // textnode, or somesuch.  offsets refer to data in node
    {
      nsIDOMText *textNode;
      res = mStartParent->QueryInterface(kIDOMTextIID, (void**)&textNode);
      if (!NS_SUCCEEDED(res)) // if it's not a text node, punt
      {
        NS_NOTREACHED("nsRange::DeleteContents");
        NS_IF_RELEASE(cStart);
        NS_IF_RELEASE(cEnd);
        return NS_ERROR_UNEXPECTED;
      }
      // delete the text
      res = textNode->DeleteData(mStartOffset, mEndOffset);
      if (!NS_SUCCEEDED(res)) 
      {
        NS_NOTREACHED("nsRange::DeleteContents");
        NS_IF_RELEASE(cStart);
        NS_IF_RELEASE(cEnd);
        NS_IF_RELEASE(textNode);
        return res;
      }
      NS_IF_RELEASE(textNode);
      return NS_OK;
    }
  } 
  
  /* complex case: cStart != cEnd
     revisit - there are potential optimizations here and also tradeoffs.
  */
  
  // get start node ancestors
  nsVoidArray startAncestorList;
  FillArrayWithAncestors(&startAncestorList,mStartParent);

  nsContentIterator iter(this);
  nsVoidArray deleteList;
  nsIContent *cN;
  nsIContent *cParent;
  PRInt32 indx;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and mark for deletion any node that is not an ancestor
  // of the start node.
  res = iter.CurrentNode(&cN);
  while (NS_COMFALSE == iter.IsDone())
  {
    // if node is not an ancestor of start node, delete it
    if (mStartAncestors->IndexOf(NS_STATIC_CAST(void*,cN)) == -1)
    {
      deleteList.AppendElement(NS_STATIC_CAST(void*,cN));
    }
    iter.Next();
    res = iter.CurrentNode(&cN);
  }
  
  // remove the nodes on the delete list
  while (deleteList.Count())
  {
    cN = NS_STATIC_CAST(nsIContent*, deleteList.ElementAt(0));
    res = cN->GetParent(cParent);
    res = cParent->IndexOf(cN,indx);
    res = cParent->RemoveChildAt(indx, PR_TRUE);
  }
  
  return NS_OK;
}

nsresult nsRange::CompareEndPoints(PRUint16 how, nsIDOMRange* srcRange,
                                   PRInt32* aCmpRet)
{
  nsresult res;
  if (aCmpRet == 0)
    return NS_ERROR_NULL_POINTER;
  if (srcRange == 0)
    return NS_ERROR_INVALID_ARG;

  nsIDOMNode* node1 = 0;
  nsIDOMNode* node2 = 0;
  PRInt32 offset1, offset2;
  switch (how)
  {
  case nsIDOMRange::START_TO_START:
    node1 = mStartParent;
    offset1 = mStartOffset;
    res = srcRange->GetStartParent(&node2);
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::START_TO_END:
    node1 = mStartParent;
    offset1 = mStartOffset;
    res = srcRange->GetEndParent(&node2);
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_START:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetStartParent(&node2);
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_END:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetEndParent(&node2);
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;

  default:  // shouldn't get here
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!NS_SUCCEEDED(res))
  {
    NS_IF_RELEASE(node2);
    return res;
  }

  if ((node1 == node2) && (offset1 == offset2))
    *aCmpRet = 0;
  else if (IsIncreasing(node1, offset2, node2, offset2))
    *aCmpRet = 1;
  else
    *aCmpRet = -1;
  NS_IF_RELEASE(node2);
  return NS_OK;
}

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ 
  nsresult res = CloneContents(aReturn);
  if (!NS_SUCCEEDED(res))
    return res;
  res = DeleteContents();
  return res; 
}

nsresult nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult res;

  nsIDOMDocument* document;
  res = mStartParent->GetOwnerDocument(&document);
  if (!NS_SUCCEEDED(res))
    return res;

  // Create a new document fragment in the context of this document
  nsIDOMDocumentFragment* docfrag;
  res = document->CreateDocumentFragment(&docfrag);
  NS_IF_RELEASE(document);

  // XXX but who owns this doc frag -- when will it be released?
  if (NS_SUCCEEDED(res))
  {
    // Loop over the nodes contained in this Range:
    // XXX Getting the nodes still needs to be implemented!
    return NS_ERROR_NOT_IMPLEMENTED;
#if 0
    while (1)
    {
      // This is how to append a node to a doc frag:
      nsIDOMNode* clonedNode;
      res = thisNode->cloneNode(PR_TRUE, &clonedNode);
      if (!NS_SUCCEEDED(res))
        break;    // punt
      nsIDOMNode* retNode;
      res = docfrag->insertBefore(clonedNode, (nsIDOMNode*)0, &retNode);
      if (!NS_SUCCEEDED(res))
        break;
    }

    // haven't implemented it yet, so just punt:
    if (!NS_SUCCEEDED(res))
    {
      NS_IF_RELEASE(docfrag);
      return res;
    }
    return NS_OK;
#endif
  }

  NS_IF_RELEASE(docfrag);
  return res;
}

nsresult nsRange::Clone(nsIDOMRange** aReturn)
{
  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

  return NS_ERROR_NOT_IMPLEMENTED;

  nsresult res = NS_NewRange(aReturn);
  if (!NS_SUCCEEDED(res))
    return res;

  res = (*aReturn)->SetStart(mStartParent, mStartOffset);
  if (NS_SUCCEEDED(res))
    res = (*aReturn)->SetEnd(mEndParent, mEndOffset);
  if (!NS_SUCCEEDED(res))
  {
    NS_IF_RELEASE(*aReturn);
    return res;
  }

  return NS_OK;
}

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ToString(nsString& aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

