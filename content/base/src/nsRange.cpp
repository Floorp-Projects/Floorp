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
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

nsVoidArray* nsRange::mStartAncestors = nsnull;      
nsVoidArray* nsRange::mEndAncestors = nsnull;        
nsVoidArray* nsRange::mStartAncestorOffsets = nsnull; 
nsVoidArray* nsRange::mEndAncestorOffsets = nsnull;  

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);

/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRange(nsIDOMRange** aInstancePtrResult)
{
  nsRange * range = new nsRange();
  return range->QueryInterface(nsIDOMRange::GetIID(), (void**) aInstancePtrResult);
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

// Utility routine to detect if a content node intersects a range
PRBool IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange)
{
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) < NODE(end))  and (RANGE(end) > NODE(start))
  // then the Node intersect the Range.
  
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMNode> parent, rangeStartParent, rangeEndParent;
  PRInt32 nodeStart, nodeEnd, rangeStartOffset, rangeEndOffset; 
  
  // gather up the dom point info
  if (!GetNodeBracketPoints(aNode, &parent, &nodeStart, &nodeEnd))
    return PR_FALSE;
  
  if (!NS_SUCCEEDED(aRange->GetStartParent(getter_AddRefs(rangeStartParent))))
    return PR_FALSE;

  if (!NS_SUCCEEDED(aRange->GetStartOffset(&rangeStartOffset)))
    return PR_FALSE;

  if (!NS_SUCCEEDED(aRange->GetEndParent(getter_AddRefs(rangeEndParent))))
    return PR_FALSE;

  if (!NS_SUCCEEDED(aRange->GetEndOffset(&rangeEndOffset)))
    return PR_FALSE;

  // is RANGE(start) < NODE(end) ?
  PRInt32 comp = ComparePoints(rangeStartParent, rangeStartOffset, parent, nodeEnd);
  if (comp >= 0)
    return PR_FALSE; // range start is after node end
    
  // is RANGE(end) > NODE(start) ?
  comp = ComparePoints(rangeEndParent, rangeEndOffset, parent, nodeStart);
  if (comp <= 0)
    return PR_FALSE; // range end is before node start
    
  // if we got here then the node intersects the range
  return PR_TRUE;
}


// Utility routine to detect if a content node is completely contained in a range
// If outNodeBefore is returned true, then the node starts before the range does.
// If outNodeAfter is returned true, then the node ends after the range does.
// Note that both of the above might be true.
// If neither are true, the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range!
nsresult CompareNodeToRange(nsIContent* aNode, 
                        nsIDOMRange* aRange,
                        PRBool *outNodeBefore,
                        PRBool *outNodeAfter)
{
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.
  
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aRange) 
    return NS_ERROR_NULL_POINTER;
  if (!outNodeBefore) 
    return NS_ERROR_NULL_POINTER;
  if (!outNodeAfter) 
    return NS_ERROR_NULL_POINTER;
  
  PRBool isPositioned;
  nsresult err = ((nsRange*)aRange)->GetIsPositioned(&isPositioned);
  // Why do I have to cast above?  Because GetIsPositioned() is
  // mysteriously missing from the nsIDOMRange interface.  dunno why.

  if (!NS_SUCCEEDED(err))
    return err;
    
  if (!isPositioned) 
    return NS_ERROR_UNEXPECTED; 
  
  nsCOMPtr<nsIDOMNode> parent, rangeStartParent, rangeEndParent;
  PRInt32 nodeStart, nodeEnd, rangeStartOffset, rangeEndOffset; 
  
  // gather up the dom point info
  if (!GetNodeBracketPoints(aNode, &parent, &nodeStart, &nodeEnd))
    return NS_ERROR_FAILURE;
  
  if (!NS_SUCCEEDED(aRange->GetStartParent(getter_AddRefs(rangeStartParent))))
    return NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(aRange->GetStartOffset(&rangeStartOffset)))
    return NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(aRange->GetEndParent(getter_AddRefs(rangeEndParent))))
    return NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(aRange->GetEndOffset(&rangeEndOffset)))
    return NS_ERROR_FAILURE;

  *outNodeBefore = PR_FALSE;
  *outNodeAfter = PR_FALSE;
  
  // is RANGE(start) <= NODE(start) ?
  PRInt32 comp = ComparePoints(rangeStartParent, rangeStartOffset, parent, nodeStart);
  if (comp > 0)
    *outNodeBefore = PR_TRUE; // range start is after node start
    
  // is RANGE(end) >= NODE(end) ?
  comp = ComparePoints(rangeEndParent, rangeEndOffset, parent, nodeEnd);
  if (comp < 0)
    *outNodeAfter = PR_TRUE; // range end is before node end
    
  return NS_OK;
}


// Utility routine to create a pair of dom points to represent 
// the start and end locations of a single node.  Return false
// if we dont' succeed.
PRBool GetNodeBracketPoints(nsIContent* aNode, 
                            nsCOMPtr<nsIDOMNode>* outParent,
                            PRInt32* outStartOffset,
                            PRInt32* outEndOffset)
{
  if (!aNode) 
    return PR_FALSE;
  if (!outParent) 
    return PR_FALSE;
  if (!outStartOffset)
    return PR_FALSE;
  if (!outEndOffset)
    return PR_FALSE;
    
  nsCOMPtr<nsIDOMNode> theDOMNode( do_QueryInterface(aNode) );
  PRInt32 indx;
  theDOMNode->GetParentNode(getter_AddRefs(*outParent));
  
  if (!(*outParent)) // special case for root node
  {
    // can't make a parent/offset pair to represent start or 
    // end of the root node, becasue it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    *outParent = do_QueryInterface(aNode);
    nsCOMPtr<nsIContent> cN(do_QueryInterface(*outParent));
    if (!cN)
      return PR_FALSE;
    cN->ChildCount(indx);
    if (!indx) 
      return PR_FALSE;
    *outStartOffset = 0;
    *outEndOffset = indx;
  }
  else
  {
    nsCOMPtr<nsIContent> cN(do_QueryInterface(*outParent));
    if (!NS_SUCCEEDED(cN->IndexOf(aNode, indx)))
      return PR_FALSE;
    *outStartOffset = indx;
    *outEndOffset = indx+1;
  }
  return PR_TRUE;
}

/******************************************************
 * constructor/destructor
 ******************************************************/
 
nsRange::nsRange() :
  mIsPositioned(PR_FALSE),
  mStartParent(),
  mStartOffset(0),
  mEndParent(),
  mEndOffset(0),
  mScriptObject(nsnull)
{
  NS_INIT_REFCNT();
} 

nsRange::~nsRange() 
{
  DoSetRange(nsCOMPtr<nsIDOMNode>(),0,nsCOMPtr<nsIDOMNode>(),0); 
  // we want the side effects (releases and list removals)
  // note that "nsCOMPtr<nsIDOMmNode>()" is the moral equivalent of null
} 

/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)

nsresult nsRange::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMRange *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMRange::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsIDOMRange*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

/******************************************************
 * Private helper routines
 ******************************************************/

PRBool nsRange::InSameDoc(nsCOMPtr<nsIDOMNode> aNode1, nsCOMPtr<nsIDOMNode> aNode2)
{
  nsCOMPtr<nsIContent> cN1;
  nsCOMPtr<nsIContent> cN2;
  nsCOMPtr<nsIDocument> doc1;
  nsCOMPtr<nsIDocument> doc2;
  
  nsresult res = GetContentFromDOMNode(aNode1, &cN1);
  if (!NS_SUCCEEDED(res)) 
    return PR_FALSE;
  res = GetContentFromDOMNode(aNode2, &cN2);
  if (!NS_SUCCEEDED(res)) 
    return PR_FALSE;
  res = cN1->GetDocument(*getter_AddRefs(doc1));
  if (!NS_SUCCEEDED(res)) 
    return PR_FALSE;
  res = cN2->GetDocument(*getter_AddRefs(doc2));
  if (!NS_SUCCEEDED(res)) 
    return PR_FALSE;
  
  // Now compare the two documents: is direct comparison safe?
  if (doc1 == doc2) 
    return PR_TRUE;

  return PR_FALSE;
}


nsresult nsRange::AddToListOf(nsCOMPtr<nsIDOMNode> aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(nsIContent::GetIID(), getter_AddRefs(cN));
  if (!NS_SUCCEEDED(res)) 
    return res;

  res = cN->RangeAdd(*NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}
  

nsresult nsRange::RemoveFromListOf(nsCOMPtr<nsIDOMNode> aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(nsIContent::GetIID(), getter_AddRefs(cN));
  if (!NS_SUCCEEDED(res)) 
    return res;

  res = cN->RangeRemove(*NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}

// It's important that all setting of the range start/end points 
// go through this function, which will do all the right voodoo
// for content notification of range ownership.  
// Calling DoSetRange with either parent argument null will collapse
// the range to have both endpoints point to the other node
nsresult nsRange::DoSetRange(nsCOMPtr<nsIDOMNode> aStartN, PRInt32 aStartOffset,
                             nsCOMPtr<nsIDOMNode> aEndN, PRInt32 aEndOffset)
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
    }
    mStartParent = aStartN;
    if (mStartParent) // if it has a new start node, put it on it's list
    {
      AddToListOf(mStartParent);
    }
  }
  mStartOffset = aStartOffset;

  if (mEndParent != aEndN)
  {
    if (mEndParent) // if it had a former end node, take it off it's list
    {
      RemoveFromListOf(mEndParent);
    }
    mEndParent = aEndN;
    if (mEndParent) // if it has a new end node, put it on it's list
    {
      AddToListOf(mEndParent);
    }
  }
  mEndOffset = aEndOffset;

  if (mStartParent) 
    mIsPositioned = PR_TRUE;
  else
    mIsPositioned = PR_FALSE;

  // FIX ME need to handle error cases 
  // (range lists return error, or setting only one endpoint to null)
  return NS_OK;
}


PRBool nsRange::IsIncreasing(nsCOMPtr<nsIDOMNode> aStartN, PRInt32 aStartOffset,
                             nsCOMPtr<nsIDOMNode> aEndN, PRInt32 aEndOffset)
{
  PRInt32 numStartAncestors = 0;
  PRInt32 numEndAncestors = 0;
  PRInt32 commonNodeStartOffset = 0;
  PRInt32 commonNodeEndOffset = 0;
  
  // no trivial cases please
  if (!aStartN || !aEndN) 
    return PR_FALSE;
  
  // check common case first
  if (aStartN==aEndN)
  {
    if (aStartOffset>aEndOffset) 
      return PR_FALSE;
    else 
      return PR_TRUE;
  }
  
  // XXX - therad safety - need locks around here to end of routine
  
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
    if (numStartAncestors<0) 
      break; // this will only happen if one endpoint's node is the common ancestor of the other
    if (numEndAncestors<0) 
      break;
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  numStartAncestors++;
  numEndAncestors++;
  commonNodeStartOffset = (PRInt32)mStartAncestorOffsets->ElementAt(numStartAncestors);
  commonNodeEndOffset   = (PRInt32)mEndAncestorOffsets->ElementAt(numEndAncestors);
  
  if (commonNodeStartOffset > commonNodeEndOffset) 
    return PR_FALSE;
  else if (commonNodeStartOffset < commonNodeEndOffset) 
    return PR_TRUE;
  else 
  {
    // The offsets are equal.  This can happen when one endpoint parent is the common parent
    // of both endpoints.  In this case, we compare the depth of the ancestor tree to determine
    // the ordering.
    if (numStartAncestors < numEndAncestors)
      return PR_TRUE;
    else if (numStartAncestors > numEndAncestors)
      return PR_FALSE;
    else
    {
      // whoa nelly. shouldn't get here.
      NS_NOTREACHED("nsRange::IsIncreasing");
      return PR_FALSE;
    }
  }
}

nsresult nsRange::IsPointInRange(nsCOMPtr<nsIDOMNode> aParent, PRInt32 aOffset, PRBool* aResult)
{
  PRInt32  compareResult = 0;
  nsresult res;
  res = ComparePointToRange(aParent, aOffset, &compareResult);
  if (compareResult) 
    *aResult = PR_FALSE;
  else 
    *aResult = PR_TRUE;
  return res;
}
  
// returns -1 if point is before range, 0 if point is in range, 1 if point is after range
nsresult nsRange::ComparePointToRange(nsCOMPtr<nsIDOMNode> aParent, PRInt32 aOffset, PRInt32* aResult)
{
  // check arguments
  if (!aResult) 
    return NS_ERROR_NULL_POINTER;
  
  // no trivial cases please
  if (!aParent) 
    return NS_ERROR_NULL_POINTER;
  
  // our range is in a good state?
  if (!mIsPositioned) 
    return NS_ERROR_NOT_INITIALIZED;
  
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
  if (IsIncreasing(aParent,aOffset,mStartParent,mStartOffset)) 
    *aResult = -1;
  else if (IsIncreasing(mEndParent,mEndOffset,aParent,aOffset)) 
    *aResult = 1;
  else 
    *aResult = 0;
  
  return NS_OK;
}
  
PRInt32 nsRange::IndexOf(nsCOMPtr<nsIDOMNode> aChildNode)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  nsCOMPtr<nsIContent> contentChild;
  nsCOMPtr<nsIContent> contentParent;
  PRInt32    theIndex = nsnull;
  
  if (!aChildNode) 
    return 0;

  // get the parent node
  nsresult res = aChildNode->GetParentNode(getter_AddRefs(parentNode));
  if (!NS_SUCCEEDED(res)) 
    return 0;
  
  // convert node and parent to nsIContent, so that we can find the child index
  res = parentNode->QueryInterface(nsIContent::GetIID(), getter_AddRefs(contentParent));
  if (!NS_SUCCEEDED(res)) 
    return 0;

  res = aChildNode->QueryInterface(nsIContent::GetIID(), getter_AddRefs(contentChild));
  if (!NS_SUCCEEDED(res)) 
    return 0;
  
  // finally we get the index
  res = contentParent->IndexOf(contentChild,theIndex); 
  if (!NS_SUCCEEDED(res)) 
    return 0;

  return theIndex;
}

PRInt32 nsRange::FillArrayWithAncestors(nsVoidArray* aArray, nsCOMPtr<nsIDOMNode> aNode)
{
  PRInt32    i=0;
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> parent;
  // callers responsibility to make sure args are non-null and proper type
  
  // insert node itself
  aArray->InsertElementAt((void*)node,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  node->GetParentNode(getter_AddRefs(parent));  
  while(parent)
  {
    node = parent;
    ++i;
    aArray->InsertElementAt(NS_STATIC_CAST(void*,node),i);
    node->GetParentNode(getter_AddRefs(parent));
  }
  
  return i;
}

PRInt32 nsRange::GetAncestorsAndOffsets(nsCOMPtr<nsIDOMNode> aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets)
{
  PRInt32    i=0;
  PRInt32    nodeOffset;
  nsresult   res;
  nsCOMPtr<nsIContent> contentNode;
  nsCOMPtr<nsIContent> contentParent;
  
  // callers responsibility to make sure args are non-null and proper type

  res = aNode->QueryInterface(nsIContent::GetIID(),getter_AddRefs(contentNode));
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::GetAncestorsAndOffsets");
    return -1;  // poor man's error code
  }
  
  // insert node itself
  aAncestorNodes->InsertElementAt((void*)contentNode,i);
  aAncestorOffsets->InsertElementAt((void*)aOffset,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  contentNode->GetParent(*getter_AddRefs(contentParent));
  while(contentParent)
  {
    contentParent->IndexOf(contentNode, nodeOffset);
    ++i;
    aAncestorNodes->InsertElementAt((void*)contentParent,i);
    aAncestorOffsets->InsertElementAt((void*)nodeOffset,i);
    contentNode = contentParent;
    contentNode->GetParent(*getter_AddRefs(contentParent));
  }
  
  return i;
}

nsCOMPtr<nsIDOMNode> nsRange::CommonParent(nsCOMPtr<nsIDOMNode> aNode1, nsCOMPtr<nsIDOMNode> aNode2)
{
  nsCOMPtr<nsIDOMNode> theParent;
  
  // no null nodes please
  if (!aNode1 || !aNode2) 
    return theParent;  // moral equiv of nsnull
  
  // shortcut for common case - both nodes are the same
  if (aNode1 == aNode2)
  {
    theParent = aNode1; // this forces an AddRef
    return theParent;
  }

  // otherwise traverse the tree for the common ancestor
  // For now, a pretty dumb hack on computing this
  nsVoidArray array1;
  nsVoidArray array2;
  PRInt32     i=0, j=0;
  
  // get ancestors of each node
  i = FillArrayWithAncestors(&array1,aNode1);
  j = FillArrayWithAncestors(&array2,aNode2);
  
  // sanity test (for now) - FillArrayWithAncestors succeeded
  if ((i==-1) || (j==-1))
  {
    NS_NOTREACHED("nsRange::CommonParent");
    return theParent; // moral equiv of nsnull
  }
  
  // sanity test (for now) - the end of each array
  // should match and be the root
  if (array1.ElementAt(i) != array2.ElementAt(j))
  {
    NS_NOTREACHED("nsRange::CommonParent");
    return theParent; // moral equiv of nsnull
  }
  
  // back through the ancestors, starting from the root, until
  // first different ancestor found.  
  while (array1.ElementAt(i) == array2.ElementAt(j))
  {
    --i;
    --j;
    if (i<0) 
      break; // this will only happen if one endpoint's node is the common ancestor of the other
    if (j<0) 
      break;
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  i++;
  nsIDOMNode *node = NS_STATIC_CAST(nsIDOMNode*, array1.ElementAt(i));
  theParent = do_QueryInterface(node);
  return theParent;  
}

nsresult nsRange::GetDOMNodeFromContent(nsCOMPtr<nsIContent> inContentNode, nsCOMPtr<nsIDOMNode>* outDomNode)
{
  if (!outDomNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inContentNode->QueryInterface(nsIDOMNode::GetIID(), getter_AddRefs(*outDomNode));
  if (!NS_SUCCEEDED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::GetContentFromDOMNode(nsCOMPtr<nsIDOMNode> inDomNode, nsCOMPtr<nsIContent>* outContentNode)
{
  if (!outContentNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inDomNode->QueryInterface(nsIContent::GetIID(), getter_AddRefs(*outContentNode));
  if (!NS_SUCCEEDED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::PopRanges(nsCOMPtr<nsIDOMNode> aDestNode, PRInt32 aOffset, nsCOMPtr<nsIContent> aSourceNode)
{
  // utility routine to pop all the range endpoints inside the content subtree defined by 
  // aSourceNode, into the node/offset represented by aDestNode/aOffset.
  
  nsCOMPtr<nsIContentIterator> iter;
  nsresult res = NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(aSourceNode);

  nsCOMPtr<nsIContent> cN;
  nsVoidArray* theRangeList;
  
  res = iter->CurrentNode(getter_AddRefs(cN));
  while (NS_COMFALSE == iter->IsDone())
  {
    cN->GetRangeList(theRangeList);
    if (theRangeList)
    {
       nsRange* theRange;
       PRInt32  theCount = theRangeList->Count();
       while (theCount)
       {
          theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(0)));
          if (theRange)
          {
            nsCOMPtr<nsIDOMNode> domNode;
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
          }
          // must refresh theRangeList - it might have gone away!
          cN->GetRangeList(theRangeList);
          if (theRangeList)
            theCount = theRangeList->Count();
          else
            theCount = 0;
       } 
    }
    iter->Next();
    res = iter->CurrentNode(getter_AddRefs(cN));
  }
  
  return NS_OK;
}

// sanity check routine for content helpers.  confirms that given 
// node owns one or both range endpoints.
nsresult nsRange::ContentOwnsUs(nsCOMPtr<nsIDOMNode> domNode)
{
  NS_PRECONDITION(domNode, "null pointer");
  if ((mStartParent != domNode) && (mEndParent != domNode))
  {
    NS_NOTREACHED("nsRange::ContentOwnsUs");
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
  //NS_IF_RELEASE(*aStartParent); don't think we should be doing this
  *aStartParent = mStartParent;
  NS_IF_ADDREF(*aStartParent);
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
  //NS_IF_RELEASE(*aEndParent); don't think we should be doing this
  *aEndParent = mEndParent;
  NS_IF_ADDREF(*aEndParent);
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
  NS_IF_ADDREF(*aCommonParent);
  return NS_OK;
}

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{
  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode>theParent( do_QueryInterface(aParent) );
    
  // must be in same document as endpoint, else 
  // endpoint is collapsed to new start.
  if (mIsPositioned && !InSameDoc(theParent,mEndParent))
  {
    res = DoSetRange(theParent,aOffset,theParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(theParent,aOffset,mEndParent,mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, end must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(theParent,aOffset,mEndParent,mEndOffset);
  return res;
}

nsresult nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsIDOMNode>theSibling( do_QueryInterface(aSibling) );
  PRInt32 indx = IndexOf(theSibling);
  nsIDOMNode *nParent;
  theSibling->GetParentNode(&nParent);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsIDOMNode>theSibling( do_QueryInterface(aSibling) );
  PRInt32 indx = IndexOf(theSibling) + 1;
  nsIDOMNode *nParent;
  theSibling->GetParentNode(&nParent);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode>theParent( do_QueryInterface(aParent) );
  
  // must be in same document as startpoint, else 
  // endpoint is collapsed to new end.
  if (mIsPositioned && !InSameDoc(theParent,mStartParent))
  {
    res = DoSetRange(theParent,aOffset,theParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(mStartParent,mStartOffset,theParent,aOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, start must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(mStartParent,mStartOffset,theParent,aOffset);
  return res;
}

nsresult nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsIDOMNode>theSibling( do_QueryInterface(aSibling) );
  PRInt32 indx = IndexOf(theSibling);
  nsIDOMNode *nParent;
  theSibling->GetParentNode(&nParent);
  return SetEnd(nParent,indx);
}

nsresult nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  nsCOMPtr<nsIDOMNode>theSibling( do_QueryInterface(aSibling) );
  PRInt32 indx = IndexOf(theSibling) + 1;
  nsIDOMNode *nParent;
  theSibling->GetParentNode(&nParent);
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
  return DoSetRange(nsCOMPtr<nsIDOMNode>(),0,nsCOMPtr<nsIDOMNode>(),0); 
  // note that "nsCOMPtr<nsIDOMmNode>()" is the moral equivalent of null
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  nsCOMPtr<nsIDOMNode> parent;
  nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(aN) );
  
  nsresult res = aN->GetParentNode(getter_AddRefs(parent));
  if (!NS_SUCCEEDED(res))
    return res;

  PRInt32 indx = IndexOf(theNode);
  return DoSetRange(parent,indx,parent,indx+1);
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(aN) );
  nsCOMPtr<nsIDOMNodeList> aChildNodes;
  
  nsresult res = aN->GetChildNodes(getter_AddRefs(aChildNodes));
  if (!NS_SUCCEEDED(res))
    return res;
  if (!aChildNodes)
    return NS_ERROR_UNEXPECTED;
  PRUint32 indx;
  res = aChildNodes->GetLength(&indx);
  if (!NS_SUCCEEDED(res))
    return res;
  return DoSetRange(theNode,0,theNode,indx);
}

nsresult nsRange::DeleteContents()
{ 
  nsCOMPtr<nsIContent> cStart;
  nsCOMPtr<nsIContent> cEnd;
  
  // get the content versions of our endpoints
  nsresult res = mStartParent->QueryInterface(nsIContent::GetIID(), getter_AddRefs(cStart));
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    return NS_ERROR_UNEXPECTED;
  }
  res = mEndParent->QueryInterface(nsIContent::GetIID(), getter_AddRefs(cEnd));
  if (!NS_SUCCEEDED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
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
          return res;
        }
      }
      return NS_OK;
    }
    else // textnode -  offsets refer to data in node
    {
      nsCOMPtr<nsIDOMText> textNode;
      res = mStartParent->QueryInterface(nsIDOMText::GetIID(), getter_AddRefs(textNode));
      if (!NS_SUCCEEDED(res)) // if it's not a text node, punt
      {
        NS_NOTREACHED("nsRange::DeleteContents");
        return NS_ERROR_UNEXPECTED;
      }
      // delete the text
      res = textNode->DeleteData(mStartOffset, mEndOffset - mStartOffset);
      if (!NS_SUCCEEDED(res)) 
      {
        NS_NOTREACHED("nsRange::DeleteContents");
        return res;
      }

      // collapse range endpoints; gravity should do this, but just in case ...
      mEndOffset = mStartOffset;

      return NS_OK;
    }
  } 
  
  /* complex case: cStart != cEnd
     revisit - there are potential optimizations here and also tradeoffs.
  */
  
  // get start node ancestors
  nsVoidArray startAncestorList;
  
  FillArrayWithAncestors(&startAncestorList,mStartParent);
  
  nsCOMPtr<nsIContentIterator> iter;
  res = NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(this);


  // XXX Note that this chunk is also thread unsafe, since we
  // aren't ADDREFing nodes as we put them on the delete list
  // and then releasing them wehn we take them off
  
  nsVoidArray deleteList;
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIContent> cParent;
  PRInt32 indx;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and mark for deletion any node that is not an ancestor
  // of the start node.
  res = iter->CurrentNode(getter_AddRefs(cN));
  while (NS_COMFALSE == iter->IsDone())
  {
    // if node is not an ancestor of start node, delete it
    if (mStartAncestors->IndexOf(NS_STATIC_CAST(void*,cN)) == -1)
    {
      deleteList.AppendElement(NS_STATIC_CAST(void*,cN));
    }
    iter->Next();
    res = iter->CurrentNode(getter_AddRefs(cN));
  }
  
  // remove the nodes on the delete list
  while (deleteList.Count())
  {
    cN = do_QueryInterface(NS_STATIC_CAST(nsIContent*, deleteList.ElementAt(0)));
    res = cN->GetParent(*getter_AddRefs(cParent));
    res = cParent->IndexOf(cN,indx);
    res = cParent->RemoveChildAt(indx, PR_TRUE);
    deleteList.RemoveElementAt(0);
  }
  
  // If mStartParent is a text node, delete the text after start offset
  nsIDOMText *textNode;
  res = mStartParent->QueryInterface(nsIDOMText::GetIID(), (void**)&textNode);
  if (NS_SUCCEEDED(res)) 
  {
    res = textNode->DeleteData(mStartOffset, 0xFFFFFFFF); // del to end
    if (!NS_SUCCEEDED(res))
      return res;  // XXX need to switch over to nsCOMPtr to avoid leaks here
  }

  // If mEndParent is a text node, delete the text before end offset
  res = mEndParent->QueryInterface(nsIDOMText::GetIID(), (void**)&textNode);
  if (NS_SUCCEEDED(res)) 
  {
    res = textNode->DeleteData(0, mEndOffset); // del from start
    if (!NS_SUCCEEDED(res))
      return res;  // XXX need to switch over to nsCOMPtr to avoid leaks here
  }

  // Collapse to start point:
  mEndParent = mStartParent;
  mEndOffset = mStartOffset;

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

  nsCOMPtr<nsIDOMNode> node1;
  nsCOMPtr<nsIDOMNode> node2;
  PRInt32 offset1, offset2;
  switch (how)
  {
  case nsIDOMRange::START_TO_START:
    node1 = mStartParent;
    offset1 = mStartOffset;
    res = srcRange->GetStartParent(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::START_TO_END:
    node1 = mStartParent;
    offset1 = mStartOffset;
    res = srcRange->GetEndParent(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_START:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetStartParent(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_END:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetEndParent(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;

  default:  // shouldn't get here
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!NS_SUCCEEDED(res))
    return res;

  if ((node1 == node2) && (offset1 == offset2))
    *aCmpRet = 0;
  else if (IsIncreasing(node1, offset2, node2, offset2))
    *aCmpRet = 1;
  else
    *aCmpRet = -1;
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
nsRange::CloneSibsAndParents(nsCOMPtr<nsIDOMNode> parentNode, PRInt32 nodeOffset,
                             nsCOMPtr<nsIDOMNode> clonedNode,
                             nsCOMPtr<nsIDOMNode> commonParent,
                             nsCOMPtr<nsIDOMDocumentFragment> docfrag,
                             PRBool leftP)
{
  nsresult res;

  if (!docfrag)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMNode> parentClone;
  if (!parentNode)
  {
    return NS_ERROR_INVALID_ARG;
  }

  // Make clone of parent:
  if (parentNode == commonParent || !parentNode)
  {
    parentClone = do_QueryInterface(docfrag);
  }
  else
  {
    res = parentNode->CloneNode(PR_FALSE, getter_AddRefs(parentClone));
    if (!NS_SUCCEEDED(res))
      return res;
  }

  // Loop over self and left or right siblings, and add to parent clone:
  nsCOMPtr<nsIDOMNode> clone;
  nsCOMPtr<nsIDOMNode> retNode;    // To hold the useless return value of insertBefore

  int i = 0;  // or 1, depending on which base IndexOf uses
  nsCOMPtr<nsIDOMNode> tmpNode;
  res = parentNode->GetFirstChild(getter_AddRefs(tmpNode));
  if (!NS_SUCCEEDED(res))
  {
    return res;
  }

  while (1)
  {
    if (i == nodeOffset)
    {
      // If we weren't passed in a clone of node, make one now:
      if (!clonedNode)
      {
        res = tmpNode->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
        if (!NS_SUCCEEDED(res))
          break;
      }

      res = parentClone->InsertBefore(clonedNode, (nsIDOMNode*)0, getter_AddRefs(retNode));
    }
    else if ((leftP && (i < nodeOffset))
             || (!leftP && (i > nodeOffset)))
    {
      res = tmpNode->CloneNode(PR_TRUE, getter_AddRefs(clone));
      res = parentClone->InsertBefore(clone, (nsIDOMNode*)0, getter_AddRefs(retNode));
    }
    else
      res = NS_OK;
    if (!NS_SUCCEEDED(res))
      break;

    ++i;
    nsCOMPtr<nsIDOMNode> tmptmpNode = tmpNode;
    res = tmpNode->GetNextSibling(getter_AddRefs(tmpNode));
    if (!NS_SUCCEEDED(res) || tmpNode == 0)
      break;
  }

  if (!NS_SUCCEEDED(res))	  // Probably broke out of the loop prematurely
    return res;

  // Recurse to parent:
  tmpNode = parentNode;
  res = tmpNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_SUCCEEDED(res))
  {
    PRInt32 index = IndexOf(parentNode);
    return CloneSibsAndParents(parentNode, index, parentClone,
                               commonParent, docfrag, leftP);
  }

  //
  // XXX This is fine for left children but it will include too much
  // XXX instead of stopping at the left children of the end node.
  //
  
  return res;
}

nsresult nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
// XXX  Not fully implemented  XXX
return NS_ERROR_NOT_IMPLEMENTED; 

#if 0
// partial implementation here
  if (!aReturn) 
    return NS_ERROR_NULL_POINTER;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMNode> commonParent = CommonParent(mStartParent,mEndParent);

  nsresult res;
  nsCOMPtr<nsIDOMDocument> document;
  res = mStartParent->GetOwnerDocument(getter_AddRefs(document));
  if (!NS_SUCCEEDED(res))
    return res;

  // Create a new document fragment in the context of this document
  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = document->CreateDocumentFragment(getter_AddRefs(docfrag));

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

  *aReturn = docFrag;
  return res;
#endif
}

nsresult nsRange::Clone(nsIDOMRange** aReturn)
{
  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewRange(aReturn);
  if (!NS_SUCCEEDED(res))
    return res;

  return DoSetRange(mStartParent, mStartOffset, mEndParent, mEndOffset);
}

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ToString(nsString& aReturn)
{ 
  nsCOMPtr<nsIContent> cStart( do_QueryInterface(mStartParent) );
  nsCOMPtr<nsIContent> cEnd( do_QueryInterface(mEndParent) );
  
  // clear the string
  aReturn.Truncate();
  
  if (!cStart || !cEnd)
    return NS_ERROR_UNEXPECTED;
    
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(mStartParent) );
    
    if (textNode)
    {
      // grab the text
      if (!NS_SUCCEEDED(textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn)))
        return NS_ERROR_UNEXPECTED;
      return NS_OK;
    }
  } 
  
  /* complex case: cStart != cEnd, or cStart not a text node
     revisit - there are potential optimizations here and also tradeoffs.
  */

  nsCOMPtr<nsIContentIterator> iter;
  NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(this);
  
  nsString tempString;
  nsCOMPtr<nsIContent> cN;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and grab the text from any text node
  iter->CurrentNode(getter_AddRefs(cN));
  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(cN) );
    if (textNode) // if it's a text node, get the text
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
    }
    iter->Next();
    iter->CurrentNode(getter_AddRefs(cN));
  }
  return NS_OK;
}


nsresult nsRange::OwnerGone(nsIContent* aDyingNode)
{
  // nothing for now - should be impossible to getter here
  // No node should be deleted if it holds a range endpoint,
  // since the range endpoint addrefs the node.
  NS_ASSERTION(PR_FALSE,"Deleted content holds a range endpoint");  
  return NS_OK;
}
  

nsresult nsRange::OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  // quick return if no range list
  nsVoidArray *theRangeList;
  parent->GetRangeList(theRangeList);
  if (!theRangeList) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(parent, &domNode);
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  PRInt32		loop = 0;
  nsRange*	theRange; 
  while ((theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)))) != nsnull)
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
  return NS_OK;
}
  

nsresult nsRange::OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  nsCOMPtr<nsIContent> removed( do_QueryInterface(aRemovedNode) );
  // quick return if no range list
  nsVoidArray *theRangeList;
  parent->GetRangeList(theRangeList);
  if (!theRangeList) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(parent, &domNode);
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  PRInt32		loop = 0;
  nsRange*	theRange; 
  while ((theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)))) != nsnull)
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
  res = PopRanges(domNode, aOffset, removed);

  return NS_OK;
}
  

nsresult nsRange::OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  // don't need to adjust ranges whose endpoints are in this parent,
  // but we do need to pop out any range endpoints inside the subtree
  // rooted by aReplacedNode.
  
  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  nsCOMPtr<nsIContent> replaced( do_QueryInterface(aReplacedNode) );
  nsCOMPtr<nsIDOMNode> parentDomNode; 
  nsresult res;
  
  res = GetDOMNodeFromContent(parent, &parentDomNode);
  if (NS_FAILED(res))  return res;
  if (!parentDomNode) return NS_ERROR_UNEXPECTED;
  
  res = PopRanges(parentDomNode, aOffset, replaced);
  
  return res;
}
  

nsresult nsRange::TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartChanged, PRInt32 aEndChanged, PRInt32 aReplaceLength)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aTextNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> textNode( do_QueryInterface(aTextNode) );
  nsVoidArray *theRangeList;
  aTextNode->GetRangeList(theRangeList);
  // the caller already checked to see if there was a range list
  
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(textNode, &domNode);
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  PRInt32		loop = 0;
  nsRange*	theRange; 
  while ((theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)))) != nsnull)
  {
    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    { 
      PRBool bStartPointInChangedText = PR_FALSE;
      
      if (theRange->mStartParent == domNode)
      {
        // if range start is inside changed text, position it after change
        if ((aStartChanged <= theRange->mStartOffset) && (aEndChanged >= theRange->mStartOffset))
        { 
          theRange->mStartOffset = aStartChanged+aReplaceLength;
          bStartPointInChangedText = PR_TRUE;
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
  
  return NS_OK;
}

// BEGIN nsIScriptContextOwner interface implementations
NS_IMETHODIMP
nsRange::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *globalObj = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptRange(aContext, (nsISupports *)(nsIDOMRange *)this, globalObj, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(globalObj);
  return res;
}

NS_IMETHODIMP
nsRange::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

// END nsIScriptContextOwner interface implementations
