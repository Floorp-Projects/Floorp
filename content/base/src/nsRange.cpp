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

#include "nsRange.h"

#include "nsIDOMRange.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsContentIterator.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
nsVoidArray* nsRange::mStartAncestors = nsnull;      
nsVoidArray* nsRange::mEndAncestors = nsnull;        
nsVoidArray* nsRange::mStartAncestorOffsets = nsnull; 
nsVoidArray* nsRange::mEndAncestorOffsets = nsnull;  


/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRange(nsIDOMRange** aInstancePtrResult)
{
  nsRange * range = new nsRange();
  return range->QueryInterface(nsIDOMRange::IID(), (void**) aInstancePtrResult);
}


// Returns -1 if point1 < point2, 1, if point1 > point2,
// 0 if error or if point1 == point2.
PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                      nsIDOMNode* aParent2, PRInt32 aOffset2)
{
  if (aParent1 == aParent2 && aOffset1 == aOffset2)
    return 0;
  nsRange* range = new nsRange;
  nsresult res = range->SetStart(aParent1, aOffset1);
  if (!NS_SUCCEEDED(res))
    return 0;
  res = range->SetEnd(aParent2, aOffset2);
  delete range;
  if (NS_SUCCEEDED(res))
    return -1;
  else
    return 1;
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
} 

nsRange::~nsRange() 
{
  DoSetRange(nsnull,0,nsnull,0); // we want the side effects (releases and list removals)
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
  if (aIID.Equals(nsIDOMRange::IID())) {
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
  PRBool retval = PR_FALSE;
  nsIContent *cN1 = nsnull;
  nsIContent *cN2 = nsnull;
  nsIDocument *doc1 = nsnull;
  nsIDocument *doc2 = nsnull;
  
  nsresult res = GetContentFromDOMNode(aNode1, &cN1);
  if (!NS_SUCCEEDED(res)) goto inSameDoc_err_label;
  res = GetContentFromDOMNode(aNode2, &cN2);
  if (!NS_SUCCEEDED(res)) goto inSameDoc_err_label;
  
  res = cN1->GetDocument(doc1);
  if (!NS_SUCCEEDED(res)) goto inSameDoc_err_label;
  res = cN2->GetDocument(doc2);
  if (!NS_SUCCEEDED(res)) goto inSameDoc_err_label;

  // Now compare the two documents: is direct comparison safe?
  if (doc1 == doc2)
    retval = PR_TRUE;
  
inSameDoc_err_label:
  NS_IF_RELEASE(cN1);
  NS_IF_RELEASE(cN2);
  NS_IF_RELEASE(doc1);
  NS_IF_RELEASE(doc2);
  return retval;

/* Beats me why GetOwnerDocument always returns not succeeded...
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
*/
}


nsresult nsRange::AddToListOf(nsIDOMNode* aNode)
{
  if (!aNode)
  {
    return NS_ERROR_NULL_POINTER;
  }
  
  nsIContent *cN;

  nsresult res = aNode->QueryInterface(nsIContent::IID(), (void**)&cN);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::AddToListOf");
    NS_IF_RELEASE(cN);
    return res;
  }

  res = cN->RangeAdd(*NS_STATIC_CAST(nsIDOMRange*,this));
  NS_RELEASE(cN);
  return res;
}
  

nsresult nsRange::RemoveFromListOf(nsIDOMNode* aNode)
{
  if (!aNode)
  {
    return NS_ERROR_NULL_POINTER;
  }
  
  nsIContent *cN;

  nsresult res = aNode->QueryInterface(nsIContent::IID(), (void**)&cN);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::RemoveFromListOf");
    NS_IF_RELEASE(cN);
    return res;
  }

  res = cN->RangeRemove(*NS_STATIC_CAST(nsIDOMRange*,this));
  NS_RELEASE(cN);
  return res;
}

// It's important that all setting of the range start/end points 
// go through this function, which will do all the right voodoo
// for both refcounting and content notification of range ownership  
// Calling DoSetRange with either parent argument null will collapse
// the range to have both endpoints point to the other node
nsresult nsRange::DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
{
  //if only one endpoint is null, set it to the other one
  if (aStartN && !aEndN) 
  {
    aEndN = aStartN;
    aEndOffset = aStartOffset;
  }
  if (aEndN && !aStartN)
  {
    aStartN = aEndN;
    aStartOffset = aEndOffset;
  }
  
  if (mStartParent != aStartN)
  {
    if (mStartParent) // if it had a former start node, take it off it's list
    {
      RemoveFromListOf(mStartParent);
      NS_RELEASE(mStartParent);
    }
    mStartParent = aStartN;
    if (mStartParent) // if it has a new start node, put it on it's list
    {
      AddToListOf(mStartParent);
      NS_ADDREF(mStartParent);
    }
  }
  mStartOffset = aStartOffset;

  if (mEndParent != aEndN)
  {
    if (mEndParent) // if it had a former end node, take it off it's list
    {
      RemoveFromListOf(mEndParent);
      NS_RELEASE(mEndParent);
    }
    mEndParent = aEndN;
    if (mEndParent) // if it has a new end node, put it on it's list
    {
      AddToListOf(mEndParent);
      NS_ADDREF(mEndParent);
    }
  }
  mEndOffset = aEndOffset;

  if (mStartParent) mIsPositioned = PR_TRUE;
  else mIsPositioned = PR_FALSE;

  // FIX ME need to handle error cases 
  // (range lists return error, or setting only one endpoint to null)
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
  
  // lazy allocation of static arrays
  if (!mStartAncestors)
  {
    mStartAncestors = new nsVoidArray();
    mStartAncestorOffsets = new nsVoidArray();
    mEndAncestors = new nsVoidArray();
    mEndAncestorOffsets = new nsVoidArray();
  }

  // XXX Threading alert - these array structures are shared across all ranges
  // access to ranges is assumed to be from only one thread.  Add monitors (or
  // stop sharing these) if that changes

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
  
  // our range is in a good state?
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
  
  // more common cases
  if ((aParent == mStartParent) && (aOffset == mStartOffset)) 
  {
    *aResult = 0;
    return NS_OK;
  }
  if ((aParent == mEndParent) && (aOffset == mEndOffset)) 
  {
    *aResult = 0;
    return NS_OK;
  }
  
  // ok, do it the hard way
  if (IsIncreasing(aParent,aOffset,mStartParent,mStartOffset)) *aResult = -1;
  else if (IsIncreasing(mEndParent,mEndOffset,aParent,aOffset)) *aResult = 1;
  else *aResult = 0;
  
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
  res = parentNode->QueryInterface(nsIContent::IID(), (void**)&contentParent);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::IndexOf");
    NS_IF_RELEASE(contentParent);
    return 0;
  }
  res = aChildNode->QueryInterface(nsIContent::IID(), (void**)&contentChild);
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

  res = aNode->QueryInterface(nsIContent::IID(), (void**)&contentNode);
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
    NS_IF_RELEASE(contentNode);
    contentNode = contentParent;
    contentNode->GetParent(contentParent);
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

nsresult nsRange::GetDOMNodeFromContent(nsIContent* inContentNode, nsIDOMNode** outDomNode)
{
  *outDomNode = nsnull;
  nsresult res = inContentNode->QueryInterface(nsIDOMNode::IID(), (void**)outDomNode);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::GetDOMNodeFromContent");
    NS_IF_RELEASE(*outDomNode);
    return res;
  }
  return NS_OK;
}

nsresult nsRange::GetContentFromDOMNode(nsIDOMNode* inDomNode, nsIContent** outContentNode)
{
  *outContentNode = nsnull;
  nsresult res = inDomNode->QueryInterface(nsIContent::IID(), (void**)outContentNode);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::GetContentFromDOMNode");
    NS_IF_RELEASE(*outContentNode);
    return res;
  }
  return NS_OK;
}

nsresult nsRange::PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode)
{
  // utility routine to pop all the range endpoints inside the content subtree defined by 
  // aSourceNode, into the node/offset represented by aDestNode/aOffset.
  
  nsContentIterator iter(aSourceNode);
  nsIContent* cN;
  nsVoidArray* theRangeList;
  nsresult res;
  
  res = iter.CurrentNode(&cN);
  while (NS_COMFALSE == iter.IsDone())
  {
    cN->GetRangeList(theRangeList);
    if (theRangeList)
    {
       nsRange* theRange;
       PRInt32 loop = 0;
       while (NULL != (theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)))))
       {
          nsIDOMNode *domNode;
          res = GetDOMNodeFromContent(cN, &domNode);
          NS_PRECONDITION(NS_SUCCEEDED(res), "error updating range list");
          NS_PRECONDITION(domNode, "error updating range list");
          // sanity check - do range and content agree over ownership?
          res = theRange->ContentOwnsUs(domNode);
          NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");

          if (theRange->mStartParent == domNode)
          {
            // promote start point up to replacement point
            theRange->SetStart(aDestNode, aOffset);
          }
          if (theRange->mEndParent == domNode)
          {
            // promote end point up to replacement point
            theRange->SetEnd(aDestNode, aOffset);
          }
          
          NS_IF_RELEASE(domNode);
          loop++;
       } 
      
    }
    iter.Next();
    NS_IF_RELEASE(cN);  // balances addref inside CurrentNode()
    res = iter.CurrentNode(&cN);
  }
  
  return NS_OK;
}

// sanity check routine for content helpers.  confirms that given 
// node owns one or both range endpoints.
nsresult nsRange::ContentOwnsUs(nsIDOMNode* domNode)
{
  NS_PRECONDITION(domNode, "null pointer");
  if ((mStartParent != domNode) && (mEndParent != domNode))
  {
    NS_NOTREACHED("nsRange::ContentOwnsUs");
    NS_IF_RELEASE(domNode);
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
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
  if (mIsPositioned && !InSameDoc(aParent,mStartParent))
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
    return DoSetRange(mStartParent,mStartOffset,mStartParent,mStartOffset);
  }
  else
  {
    return DoSetRange(mEndParent,mEndOffset,mEndParent,mEndOffset);
  }
}

nsresult nsRange::Unposition()
{
  return DoSetRange(nsnull,0,nsnull,0);
  return NS_OK;
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  nsIDOMNode * parent;
  nsresult res = aN->GetParentNode(&parent);
  if (!NS_SUCCEEDED(res))
    return res;

  PRInt32 indx = IndexOf(aN);
  return DoSetRange(parent,indx,parent,indx+1);
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  nsIDOMNodeList *aChildNodes;
  nsresult res = aN->GetChildNodes(&aChildNodes);
  if (!NS_SUCCEEDED(res))
    return res;
  if (aChildNodes==nsnull)
    return NS_ERROR_UNEXPECTED;
  PRUint32 indx;
  res = aChildNodes->GetLength(&indx);
  NS_RELEASE(aChildNodes);
  if (!NS_SUCCEEDED(res))
    return res;
  return DoSetRange(aN,0,aN,indx);
}

nsresult nsRange::DeleteContents()
{ 
  nsIContent *cStart;
  nsIContent *cEnd;
  
  // get the content versions of our endpoints
  nsresult res = mStartParent->QueryInterface(nsIContent::IID(), (void**)&cStart);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    return NS_ERROR_UNEXPECTED;
  }
  res = mEndParent->QueryInterface(nsIContent::IID(), (void**)&cEnd);
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
      for (i=mEndOffset; i>=mStartOffset; --i)
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
      res = mStartParent->QueryInterface(nsIDOMText::IID(), (void**)&textNode);
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
    NS_IF_RELEASE(cN);  // balances addref inside CurrentNode()
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
  
  NS_IF_RELEASE(cStart);
  NS_IF_RELEASE(cEnd);
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

// CloneSibsAndParents(node, dogfrag, leftP)
// Assumes that the node passed in is already a clone.
// If node == 0, adds directly to doc fragment.
// Algorithm:
// - Loop from start child until common parent
//   - clone parent (or use doc frag if parent == common parent)
//   - add self & left or right right sibs' clones to parent
//   - recurse to parent
//
nsresult
nsRange::CloneSibsAndParents(nsIDOMNode* parentNode, PRInt32 nodeOffset,
                             nsIDOMNode* clonedNode,
                             nsIDOMNode* commonParent,
                             nsIDOMDocumentFragment* docfrag,
                             PRBool leftP)
{
  nsresult res;

  if (docfrag == 0)
    return NS_ERROR_INVALID_ARG;

  nsIDOMNode* parentClone;
  if (parentNode == 0)
  {
    return NS_ERROR_INVALID_ARG;
  }

  // Make clone of parent:
  if (parentNode == commonParent || parentNode == 0)
  {
    //NS_ADDREF(docfrag);
    parentClone = docfrag;
  }
  else
  {
    res = parentNode->CloneNode(PR_FALSE, &parentClone);
    if (!NS_SUCCEEDED(res))
      return res;
  }

  // Loop over self and left or right siblings, and add to parent clone:
  nsIDOMNode* clone;
  nsIDOMNode* retNode;    // To hold the useless return value of insertBefore

  int i = 0;  // or 1, depending on which base IndexOf uses
  nsIDOMNode* tmpNode;
  res = parentNode->GetFirstChild(&tmpNode);
  if (!NS_SUCCEEDED(res))
  {
    return res;
  }

  while (1)
  {
    if (i == nodeOffset)
    {
      // If we weren't passed in a clone of node, make one now:
      if (clonedNode == 0)
      {
        res = tmpNode->CloneNode(PR_TRUE, &clonedNode);
        if (!NS_SUCCEEDED(res))
          break;
      }

      res = parentClone->InsertBefore(clonedNode, (nsIDOMNode*)0, &retNode);
      NS_IF_RELEASE(retNode); // don't need this -- can we just pass in nsnull?
    }
    else if ((leftP && (i < nodeOffset))
             || (!leftP && (i > nodeOffset)))
    {
      res = tmpNode->CloneNode(PR_TRUE, &clone);
      res = parentClone->InsertBefore(clone, (nsIDOMNode*)0, &retNode);
      NS_IF_RELEASE(retNode); // don't need this -- can we just pass in nsnull?
    }
    else
      res = NS_OK;
    if (!NS_SUCCEEDED(res))
      break;

    ++i;
    nsIDOMNode* tmptmpNode = tmpNode;
    res = tmpNode->GetNextSibling(&tmpNode);
    NS_IF_RELEASE(tmptmpNode);
    if (!NS_SUCCEEDED(res) || tmpNode == 0)
      break;
  }

  NS_IF_RELEASE(clonedNode);

  if (!NS_SUCCEEDED(res))	  // Probably broke out of the loop prematurely
    return res;

  // Recurse to parent:
  tmpNode = parentNode;
  res = tmpNode->GetParentNode(&parentNode);
  if (NS_SUCCEEDED(res))
  {
    PRInt32 index = IndexOf(parentNode);
    return CloneSibsAndParents(parentNode, index, parentClone,
                               commonParent, docfrag, leftP);
  }
  NS_IF_RELEASE(parentNode);

  //
  // XXX This is fine for left children but it will include too much
  // XXX instead of stopping at the left children of the end node.
  //
  
  return res;
}

nsresult nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  nsIDOMNode* commonParent = CommonParent(mStartParent,mEndParent);

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
    // Loop over the nodes contained in this Range,
    // from the start and end points, and add them
    // to the parent doc frag:
    res = CloneSibsAndParents(mStartParent, mStartOffset, 0,
                              commonParent, docfrag, PR_TRUE);
    res = CloneSibsAndParents(mEndParent, mEndOffset, 0,
                              commonParent, docfrag, PR_FALSE);

    // XXX Now we need to add the sibs between the two top-level
    // XXX doc frag cloned elements.

    if (NS_SUCCEEDED(res))
      return NS_OK;
  }

  NS_IF_RELEASE(docfrag);
  return res;
}

nsresult nsRange::Clone(nsIDOMRange** aReturn)
{
  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

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
{ 
 
  nsIContent *cStart;
  nsIContent *cEnd;
  
  // clear the string
  aReturn.Truncate();
  
  // get the content versions of our endpoints
  nsresult res = mStartParent->QueryInterface(nsIContent::IID(), (void**)&cStart);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::ToString");
    return NS_ERROR_UNEXPECTED;
  }
  res = mEndParent->QueryInterface(nsIContent::IID(), (void**)&cEnd);
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::ToString");
    NS_IF_RELEASE(cStart);
    return NS_ERROR_UNEXPECTED;
  }
  
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    nsIDOMText *textNode;
    res = mStartParent->QueryInterface(nsIDOMText::IID(), (void**)&textNode);
    if (!NS_SUCCEEDED(res)) // if it's not a text node, skip to iterator approach
    {
       goto toStringComplexLabel;
    }
    // grab the text
    res = textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn);
    if (!NS_SUCCEEDED(res)) 
    {
      NS_NOTREACHED("nsRange::ToString");
      NS_IF_RELEASE(cStart);
      NS_IF_RELEASE(cEnd);
      NS_IF_RELEASE(textNode);
      return res;
    }
    NS_IF_RELEASE(textNode);
    return NS_OK;
  } 
  
toStringComplexLabel:
  /* complex case: cStart != cEnd, or cStart not a text node
     revisit - there are potential optimizations here and also tradeoffs.
  */
  
  nsContentIterator iter(this);
  nsString tempString;
  nsIContent *cN;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and grab the text from any text node
  res = iter.CurrentNode(&cN);
  while (NS_COMFALSE == iter.IsDone())
  {
    nsIDOMText *textNode;
    res = cN->QueryInterface(nsIDOMText::IID(), (void**)&textNode);
    if (NS_SUCCEEDED(res)) // if it's a text node, get the text
    {
      if (cN == cStart) // only include text past start offset
      {
        PRUint32 strLength;
        textNode->GetLength(&strLength);
        textNode->SubstringData(mStartOffset,strLength-mStartOffset,tempString);
        aReturn += tempString;
      }
      else if (cN == cEnd)  // only include text before end offset
      {
        textNode->SubstringData(0,mEndOffset,tempString);
        aReturn += tempString;
      }
      else  // grab the whole kit-n-kaboodle
      {
        textNode->GetData(tempString);
        aReturn += tempString;
      }
      NS_IF_RELEASE(textNode);
    }
    iter.Next();
    NS_IF_RELEASE(cN);  // balances addref inside CurrentNode()
    res = iter.CurrentNode(&cN);
  }

  NS_IF_RELEASE(cStart);
  NS_IF_RELEASE(cEnd);
  return NS_OK;
}


nsresult nsRange::OwnerGone(nsIContent* aDyingNode)
{
  // nothing for now - ideally we would pop out all the enclosed
  // ranges, but that it an expensive bunch of tests to do when
  // destroying content nodes.  
  return NS_OK;
}
  

nsresult nsRange::OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  // quick return if no range list
  nsVoidArray *theRangeList;
  aParentNode->GetRangeList(theRangeList);
  if (theRangeList == nsnull) return NS_OK;
  
  PRInt32  loop = 0;
  nsRange* theRange;
  nsIDOMNode *domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(aParentNode, &domNode);
  if (NS_SUCCEEDED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  while (NULL != (theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))))) 
  {
    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    {
      if (theRange->mStartParent == domNode)
      {
        // if child inserted before start, move start offset right one
        if (aOffset <= theRange->mStartOffset) theRange->mStartOffset++;
      }
      if (theRange->mEndParent == domNode)
      {
        // if child inserted before end, move end offset right one
        if (aOffset <= theRange->mEndOffset) theRange->mEndOffset++;
      }
      NS_PRECONDITION(NS_SUCCEEDED(res), "error updating range list");
    }
    loop++;
  }
  NS_RELEASE(domNode);
  return NS_OK;
}
  

nsresult nsRange::OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  // quick return if no range list
  nsVoidArray *theRangeList;
  aParentNode->GetRangeList(theRangeList);
  if (theRangeList == nsnull) return NS_OK;
  
  PRInt32  loop = 0;
  nsRange *theRange;
  nsIDOMNode *domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(aParentNode, &domNode);
  if (NS_SUCCEEDED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  // any ranges that are in the parentNode may need to have offsets updated
  while (NULL != (theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))))) 
  {
    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    {
      if (theRange->mStartParent == domNode)
      {
        // if child deleted before start, move start offset left one
        if (aOffset <= theRange->mStartOffset) theRange->mStartOffset--;
      }
      if (theRange->mEndParent == domNode)
      {
        // if child deleted before end, move end offset left one
        if (aOffset <= theRange->mEndOffset) 
        {
          if (theRange->mEndOffset>0) theRange->mEndOffset--;
        }
      }
    }
    loop++;
  }
  
  // any ranges in the content subtree rooted by aRemovedNode need to
  // have the enclosed endpoints promoted up to the parentNode/offset
  res = PopRanges(domNode, aOffset, aRemovedNode);

  NS_RELEASE(domNode);
  return NS_OK;
}
  

nsresult nsRange::OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  // don't need to adjust ranges whose endpoints are in this parent,
  // but we do need to pop out any range endpoints inside the subtree
  // rooted by aReplacedNode.
  
  nsIDOMNode* parentDomNode; 
  nsresult res;
  
  res = GetDOMNodeFromContent(aParentNode, &parentDomNode);
  if (NS_SUCCEEDED(res))  return res;
  if (!parentDomNode) return NS_ERROR_UNEXPECTED;
  
  res = PopRanges(parentDomNode, aOffset, aReplacedNode);
  
  NS_RELEASE(parentDomNode);
  return res;
}
  

nsresult nsRange::TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartChanged, PRInt32 aEndChanged, PRInt32 aReplaceLength)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aTextNode) return NS_ERROR_UNEXPECTED;

  nsVoidArray *theRangeList;
  aTextNode->GetRangeList(theRangeList);
  // the caller already checked to see if there was a range list
  
  PRInt32  loop = 0;
  nsRange *theRange;
  nsIDOMNode *domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(aTextNode, &domNode);
  if (NS_SUCCEEDED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  // any ranges that are in the textNode may need to have offsets updated
  while (NULL != (theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)))))
  {
    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    { 
      PRBool bStartPointInChangedText = false;
      
      if (theRange->mStartParent == domNode)
      {
        // if range start is inside changed text, position it after change
        if ((aStartChanged <= theRange->mStartOffset) && (aEndChanged >= theRange->mStartOffset))
        { 
          theRange->mStartOffset = aStartChanged+aReplaceLength;
          bStartPointInChangedText = true;
        }
        // else if text changed before start, adjust start offset
        else if (aEndChanged <= theRange->mStartOffset) 
          theRange->mStartOffset += aStartChanged + aReplaceLength - aEndChanged;
      }
      if (theRange->mEndParent == domNode)
      {
        // if range end is inside changed text, position it before change
        if ((aStartChanged <= theRange->mEndOffset) && (aEndChanged >= theRange->mEndOffset)) 
        {
          theRange->mEndOffset = aStartChanged;
          // hack: if BOTH range endpoints were inside the change, then they
          // both get collapsed to the beginning of the change.  
          if (bStartPointInChangedText) theRange->mStartOffset = aStartChanged;
        }
        // else if text changed before end, adjust end offset
        else if (aEndChanged <= theRange->mEndOffset) 
          theRange->mEndOffset += aStartChanged + aReplaceLength - aEndChanged;
      }
    }
    loop++;
  }
  
  NS_RELEASE(domNode);
  return NS_OK;
}
