/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsDOMError.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIScriptGlobalObject.h"
#include "nsIParser.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIHTMLFragmentContentSink.h"
// XXX Temporary inclusion to deal with fragment parsing
#include "nsHTMLParts.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

PRMonitor*   nsRange::mMonitor = nsnull;
nsVoidArray* nsRange::mStartAncestors = nsnull;      
nsVoidArray* nsRange::mEndAncestors = nsnull;        
nsVoidArray* nsRange::mStartAncestorOffsets = nsnull; 
nsVoidArray* nsRange::mEndAncestorOffsets = nsnull;  

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewGenRegularIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);

/******************************************************
 * stack based utilty class for managing monitor
 ******************************************************/

class nsAutoRangeLock
{
  public:
    nsAutoRangeLock()  { nsRange::Lock(); }
    ~nsAutoRangeLock() { nsRange::Unlock(); }
};


/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRange(nsIDOMRange** aInstancePtrResult)
{
  nsRange * range = new nsRange();
  if (range)
    return range->QueryInterface(NS_GET_IID(nsIDOMRange), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}


// Returns -1 if point1 < point2, 1, if point1 > point2,
// 0 if error or if point1 == point2.
PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                      nsIDOMNode* aParent2, PRInt32 aOffset2)
{
  if (aParent1 == aParent2 && aOffset1 == aOffset2)
    return 0;
  nsIDOMRange* range;
  if (NS_FAILED(NS_NewRange(&range)))
    return 0;
  nsresult res = range->SetStart(aParent1, aOffset1);
  if (NS_FAILED(res))
    return 0;
  res = range->SetEnd(aParent2, aOffset2);
  NS_RELEASE(range);
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
  
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(rangeStartParent))))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetStartOffset(&rangeStartOffset)))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(rangeEndParent))))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetEndOffset(&rangeEndOffset)))
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

  if (NS_FAILED(err))
    return err;
    
  if (!isPositioned) 
    return NS_ERROR_UNEXPECTED; 
  
  nsCOMPtr<nsIDOMNode> parent, rangeStartParent, rangeEndParent;
  PRInt32 nodeStart, nodeEnd, rangeStartOffset, rangeEndOffset; 
  
  // gather up the dom point info
  if (!GetNodeBracketPoints(aNode, &parent, &nodeStart, &nodeEnd))
    return NS_ERROR_FAILURE;
  
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(rangeStartParent))))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetStartOffset(&rangeStartOffset)))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(rangeEndParent))))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetEndOffset(&rangeEndOffset)))
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
    *outStartOffset = nsRange::IndexOf(theDOMNode);
    *outEndOffset = *outStartOffset+1;
  }
  return PR_TRUE;
}

/******************************************************
 * constructor/destructor
 ******************************************************/

nsRange::nsRange() :
  mIsPositioned(PR_FALSE),
  mStartOffset(0),
  mEndOffset(0),
  mStartParent(),
  mEndParent(),
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
 * nsISupports
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
  if (aIID.Equals(NS_GET_IID(nsIDOMRange))) 
  {
    *aInstancePtrResult = (void*)(nsIDOMRange*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNSRange)))
  {
    *aInstancePtrResult = (void*)(nsIDOMNSRange*)this;
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

/********************************************************
 * Utilities for comparing points: API from nsIDOMNSRange
 ********************************************************/
NS_IMETHODIMP
nsRange::IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult)
{
  PRInt16 compareResult = 0;
  nsresult res;
  res = ComparePoint(aParent, aOffset, &compareResult);
  if (compareResult) 
    *aResult = PR_FALSE;
  else 
    *aResult = PR_TRUE;
  return res;
}
  
// returns -1 if point is before range, 0 if point is in range,
// 1 if point is after range.
NS_IMETHODIMP
nsRange::ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aResult)
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
  if ((aParent == mStartParent.get()) && (aParent == mEndParent.get()))
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
  if ((aParent == mStartParent.get()) && (aOffset == mStartOffset)) 
  {
    *aResult = 0;
    return NS_OK;
  }
  if ((aParent == mEndParent.get()) && (aOffset == mEndOffset)) 
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
  
NS_IMETHODIMP
nsRange::IntersectsNode(nsIDOMNode* aNode, PRBool* aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
  if (!content)
  {
    *aReturn = 0;
    return NS_ERROR_UNEXPECTED;
  }
  *aReturn = IsNodeIntersectsRange(content, this);
  return NS_OK;
}

// HOW does the node intersect the range?
NS_IMETHODIMP
nsRange::CompareNode(nsIDOMNode* aNode, PRUint16* aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  *aReturn = 0;
  PRBool nodeBefore, nodeAfter;
  nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
  if (!content)
    return NS_ERROR_UNEXPECTED;

  nsresult res = CompareNodeToRange(content, this, &nodeBefore, &nodeAfter);
  if (NS_FAILED(res))
    return res;

  // nodeBefore -> range start after node start, i.e. node starts before range.
  // nodeAfter -> range end before node end, i.e. node ends after range.
  // But I know that I get nodeBefore && !nodeAfter when the node is
  // entirely inside the selection!  This doesn't make sense.
  if (nodeBefore && !nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_BEFORE;  // May or may not intersect
  else if (!nodeBefore && nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_AFTER;   // May or may not intersect
  else if (nodeBefore && nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_BEFORE_AND_AFTER;  // definitely intersects
  else
    *aReturn = nsIDOMNSRange::NODE_INSIDE;            // definitely intersects

  return NS_OK;
}

/******************************************************
 * Private helper routines
 ******************************************************/

PRBool nsRange::InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
{
  nsCOMPtr<nsIContent> cN1;
  nsCOMPtr<nsIContent> cN2;
  nsCOMPtr<nsIDocument> doc1;
  nsCOMPtr<nsIDocument> doc2;
  
  nsresult res = GetContentFromDOMNode(aNode1, &cN1);
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = GetContentFromDOMNode(aNode2, &cN2);
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = cN1->GetDocument(*getter_AddRefs(doc1));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = cN2->GetDocument(*getter_AddRefs(doc2));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  
  // Now compare the two documents: is direct comparison safe?
  if (doc1 == doc2) 
    return PR_TRUE;

  return PR_FALSE;
}


nsresult nsRange::AddToListOf(nsIDOMNode* aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cN));
  if (NS_FAILED(res)) 
    return res;

  res = cN->RangeAdd(*NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}
  

nsresult nsRange::RemoveFromListOf(nsIDOMNode* aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cN));
  if (NS_FAILED(res)) 
    return res;

  res = cN->RangeRemove(*NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}

// It's important that all setting of the range start/end points 
// go through this function, which will do all the right voodoo
// for content notification of range ownership.  
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
  
  if (mStartParent && (mStartParent.get() != aStartN) && (mStartParent.get() != aEndN))
  {
    // if old start parent no longer involved, remove range from that
    // node's range list.
    RemoveFromListOf(mStartParent);
  }
  if (mEndParent && (mEndParent.get() != aStartN) && (mEndParent.get() != aEndN))
  {
    // if old end parent no longer involved, remove range from that
    // node's range list.
    RemoveFromListOf(mEndParent);
  }
 
 
  if (mStartParent.get() != aStartN)
  {
    mStartParent = do_QueryInterface(aStartN);
    if (mStartParent) // if it has a new start node, put it on it's list
    {
      AddToListOf(mStartParent);  // AddToList() detects duplication for us
    }
  }
  mStartOffset = aStartOffset;

  if (mEndParent.get() != aEndN)
  {
    mEndParent = do_QueryInterface(aEndN);
    if (mEndParent) // if it has a new end node, put it on it's list
    {
      AddToListOf(mEndParent);  // AddToList() detects duplication for us
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


PRBool nsRange::IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
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
  
  // thread safety - need locks around here to end of routine to protect use of static members
  nsAutoRangeLock lock;
  
  // lazy allocation of static arrays
  if (!mStartAncestors)
  {
    mStartAncestors = new nsVoidArray();
    if (!mStartAncestors) return NS_ERROR_OUT_OF_MEMORY;
    mStartAncestorOffsets = new nsVoidArray();
    if (!mStartAncestorOffsets) return NS_ERROR_OUT_OF_MEMORY;
    mEndAncestors = new nsVoidArray();
    if (!mEndAncestors) return NS_ERROR_OUT_OF_MEMORY;
    mEndAncestorOffsets = new nsVoidArray();
    if (!mEndAncestorOffsets) return NS_ERROR_OUT_OF_MEMORY;
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

PRInt32 nsRange::IndexOf(nsIDOMNode* aChildNode)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  nsCOMPtr<nsIContent> contentChild;
  nsCOMPtr<nsIContent> contentParent;
  PRInt32    theIndex = nsnull;
  
  if (!aChildNode) 
    return 0;

  // get the parent node
  nsresult res = aChildNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_FAILED(res)) 
    return 0;
  
  // convert node and parent to nsIContent, so that we can find the child index
  res = parentNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(contentParent));
  if (NS_FAILED(res)) 
    return 0;

  res = aChildNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(contentChild));
  if (NS_FAILED(res)) 
    return 0;
  
  // finally we get the index
  res = contentParent->IndexOf(contentChild,theIndex); 
  if (NS_FAILED(res)) 
    return 0;

  return theIndex;
}

PRInt32 nsRange::FillArrayWithAncestors(nsVoidArray* aArray, nsIDOMNode* aNode)
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

PRInt32 nsRange::GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets)
{
  PRInt32    i=0;
  PRInt32    nodeOffset;
  nsresult   res;
  nsCOMPtr<nsIContent> contentNode;
  nsCOMPtr<nsIContent> contentParent;
  
  // callers responsibility to make sure args are non-null and proper type

  res = aNode->QueryInterface(NS_GET_IID(nsIContent),getter_AddRefs(contentNode));
  if (NS_FAILED(res)) 
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

nsCOMPtr<nsIDOMNode> nsRange::CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
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

nsresult nsRange::GetDOMNodeFromContent(nsIContent* inContentNode, nsCOMPtr<nsIDOMNode>* outDomNode)
{
  if (!outDomNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inContentNode->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(*outDomNode));
  if (NS_FAILED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::GetContentFromDOMNode(nsIDOMNode* inDomNode, nsCOMPtr<nsIContent>* outContentNode)
{
  if (!outContentNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inDomNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(*outContentNode));
  if (NS_FAILED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode)
{
  // utility routine to pop all the range endpoints inside the content subtree defined by 
  // aSourceNode, into the node/offset represented by aDestNode/aOffset.
  
  nsCOMPtr<nsIContentIterator> iter;
  nsresult res = NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(aSourceNode);

  nsCOMPtr<nsIContent> cN;
  nsVoidArray* theRangeList;
  
  iter->CurrentNode(getter_AddRefs(cN));
  while (cN && (NS_ENUMERATOR_FALSE == iter->IsDone()))
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
            NS_POSTCONDITION(NS_SUCCEEDED(res), "error updating range list");
            NS_POSTCONDITION(domNode, "error updating range list");
            // sanity check - do range and content agree over ownership?
            res = theRange->ContentOwnsUs(domNode);
            NS_POSTCONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");

            if (theRange->mStartParent == domNode)
            {
              // promote start point up to replacement point
              res = theRange->SetStart(aDestNode, aOffset);
              NS_POSTCONDITION(NS_SUCCEEDED(res), "nsRange::PopRanges() got error from SetStart()");
              if (NS_FAILED(res)) return res;
            }
            if (theRange->mEndParent == domNode)
            {
              // promote end point up to replacement point
              res = theRange->SetEnd(aDestNode, aOffset);
              NS_POSTCONDITION(NS_SUCCEEDED(res), "nsRange::PopRanges() got error from SetEnd()");
              if (NS_FAILED(res)) return res;
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
    res = iter->Next();
    if (NS_FAILED(res)) // a little noise here to catch bugs
    {
      NS_NOTREACHED("nsRange::PopRanges() : iterator failed to advance");
      return res;
    }
    iter->CurrentNode(getter_AddRefs(cN));
  }
  
  return NS_OK;
}

// sanity check routine for content helpers.  confirms that given 
// node owns one or both range endpoints.
nsresult nsRange::ContentOwnsUs(nsIDOMNode* domNode)
{
  NS_PRECONDITION(domNode, "null pointer");
  if ((mStartParent.get() != domNode) && (mEndParent.get() != domNode))
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

nsresult nsRange::GetStartContainer(nsIDOMNode** aStartParent)
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

nsresult nsRange::GetEndContainer(nsIDOMNode** aEndParent)
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

nsresult nsRange::GetCollapsed(PRBool* aIsCollapsed)
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

nsresult nsRange::GetCommonAncestorContainer(nsIDOMNode** aCommonParent)
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
  if (nsnull == aSibling) {
    // Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  if (nsnull == aSibling) {
    // Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling) + 1;
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
  if (nsnull == aSibling) {
    // Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling);
  return SetEnd(nParent,indx);
}

nsresult nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  if (nsnull == aSibling) {
    // Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;
  }
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling) + 1;
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
  if (!aN) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> parent;
  nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(aN) );

  PRUint16 type = 0;
  aN->GetNodeType(&type);

  switch (type) {
    case nsIDOMNode::ATTRIBUTE_NODE :
    case nsIDOMNode::ENTITY_NODE :
    case nsIDOMNode::DOCUMENT_NODE :
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    case nsIDOMNode::NOTATION_NODE :
      return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  nsresult res = aN->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res) || !parent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(theNode);
  return DoSetRange(parent,indx,parent,indx+1);
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(aN) );
  nsCOMPtr<nsIDOMNodeList> aChildNodes;
  
  nsresult res = aN->GetChildNodes(getter_AddRefs(aChildNodes));
  if (NS_FAILED(res))
    return res;
  if (!aChildNodes)
    return NS_ERROR_UNEXPECTED;
  PRUint32 indx;
  res = aChildNodes->GetLength(&indx);
  if (NS_FAILED(res))
    return res;
  return DoSetRange(theNode,0,theNode,indx);
}

nsresult nsRange::DeleteContents()
{ 
  nsCOMPtr<nsIContent> cStart;
  nsCOMPtr<nsIContent> cEnd;
  
  // get the content versions of our endpoints
  nsresult res = mStartParent->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cStart));
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    return NS_ERROR_UNEXPECTED;
  }
  res = mEndParent->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cEnd));
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("nsRange::DeleteContents");
    return NS_ERROR_UNEXPECTED;
  }
  
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    PRBool hasChildren;
    res = cStart->CanContainChildren(hasChildren);
    if (NS_FAILED(res)) 
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
        if (NS_FAILED(res)) 
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
      res = mStartParent->QueryInterface(NS_GET_IID(nsIDOMText), getter_AddRefs(textNode));
      if (NS_FAILED(res)) // if it's not a text node, punt
      {
        NS_NOTREACHED("nsRange::DeleteContents");
        return NS_ERROR_UNEXPECTED;
      }
      // delete the text
      res = textNode->DeleteData(mStartOffset, mEndOffset - mStartOffset);
      if (NS_FAILED(res)) 
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
  if (NS_FAILED(res))  return res;
  res = iter->Init(this);
  if (NS_FAILED(res))  return res;

  // thread safety - need locks around here to end of routine since we
  // aren't ADDREFing nodes as we put them on the delete list
  // and then releasing them when we take them off
  nsAutoRangeLock lock;
  
  nsVoidArray deleteList;
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIContent> cParent;
  PRInt32 indx;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and mark for deletion any node that is not an ancestor
  // of the start node.
  iter->CurrentNode(getter_AddRefs(cN));
  while (cN && (NS_ENUMERATOR_FALSE == iter->IsDone()))
  {
    // if node is not an ancestor of start node, delete it
    if (startAncestorList.IndexOf(NS_STATIC_CAST(void*,cN)) == -1)
    {
      deleteList.AppendElement(NS_STATIC_CAST(void*,cN));
    }
    res = iter->Next();
    if (NS_FAILED(res)) // a little noise here to catch bugs
    {
      NS_NOTREACHED("nsRange::DeleteContents() : iterator failed to advance");
      return res;
    }
    iter->CurrentNode(getter_AddRefs(cN));
  }
  
  // remove the nodes on the delete list
  while (deleteList.Count())
  {
    cN = do_QueryInterface(NS_STATIC_CAST(nsIContent*, deleteList.ElementAt(0)));
    res = cN->GetParent(*getter_AddRefs(cParent));
    if (NS_FAILED(res)) return res;
    res = cParent->IndexOf(cN,indx);
    if (NS_FAILED(res)) return res;
    res = cParent->RemoveChildAt(indx, PR_TRUE);
    if (NS_FAILED(res)) return res;
    deleteList.RemoveElementAt(0);
  }
  
  // If mStartParent is a text node, delete the text after start offset
  nsIDOMText *textNode;
  res = mStartParent->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&textNode);
  if (NS_SUCCEEDED(res)) 
  {
    res = textNode->DeleteData(mStartOffset, 0xFFFFFFFF); // del to end
    if (NS_FAILED(res))
      return res;  // XXX need to switch over to nsCOMPtr to avoid leaks here
  }

  // If mEndParent is a text node, delete the text before end offset
  res = mEndParent->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&textNode);
  if (NS_SUCCEEDED(res)) 
  {
    res = textNode->DeleteData(0, mEndOffset); // del from start
    if (NS_FAILED(res))
      return res;  // XXX need to switch over to nsCOMPtr to avoid leaks here
  }

  // Collapse to start point:
  mEndParent = mStartParent;
  mEndOffset = mStartOffset;

  return NS_OK;
}

nsresult nsRange::CompareBoundaryPoints(PRUint16 how, nsIDOMRange* srcRange,
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
    res = srcRange->GetStartContainer(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::START_TO_END:
    node1 = mStartParent;
    offset1 = mStartOffset;
    res = srcRange->GetEndContainer(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_START:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetStartContainer(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&offset2);
    break;
  case nsIDOMRange::END_TO_END:
    node1 = mEndParent;
    offset1 = mEndOffset;
    res = srcRange->GetEndContainer(getter_AddRefs(node2));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&offset2);
    break;

  default:  // shouldn't get here
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (NS_FAILED(res))
    return res;

  if ((node1 == node2) && (offset1 == offset2))
    *aCmpRet = 0;
  else if (IsIncreasing(node1, offset1, node2, offset2))
    *aCmpRet = 1;
  else
    *aCmpRet = -1;
  return NS_OK;
}

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ 
  nsresult res = CloneContents(aReturn);
  if (NS_FAILED(res))
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
nsRange::CloneSibsAndParents(nsIDOMNode* aParentNode, PRInt32 nodeOffset,
                             nsIDOMNode* aClonedNode,
                             nsIDOMNode* aCommonParent,
                             nsIDOMDocumentFragment* docfrag,
                             PRBool leftP)
{
  nsresult res;

  if (!docfrag)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMNode> parentClone;
  if (!aParentNode)
  {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMNode> parentNode = aParentNode;
  nsCOMPtr<nsIDOMNode> clonedNode = aClonedNode;
  nsCOMPtr<nsIDOMNode> commonParent = aCommonParent;
  
  // Make clone of parent:
  if (parentNode == commonParent || !parentNode)
  {
    parentClone = do_QueryInterface(docfrag);
  }
  else
  {
    res = parentNode->CloneNode(PR_FALSE, getter_AddRefs(parentClone));
    if (NS_FAILED(res))
      return res;
  }

  // Loop over self and left or right siblings, and add to parent clone:
  nsCOMPtr<nsIDOMNode> clone;
  nsCOMPtr<nsIDOMNode> retNode;    // To hold the useless return value of insertBefore

  int i = 0;  // or 1, depending on which base IndexOf uses
  nsCOMPtr<nsIDOMNode> tmpNode;
  res = parentNode->GetFirstChild(getter_AddRefs(tmpNode));
  if (NS_FAILED(res))
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
        if (NS_FAILED(res))
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
    if (NS_FAILED(res))
      break;

    ++i;
    nsCOMPtr<nsIDOMNode> tmptmpNode = tmpNode;
    res = tmpNode->GetNextSibling(getter_AddRefs(tmpNode));
    if (NS_FAILED(res) || tmpNode == 0)
      break;
  }

  if (NS_FAILED(res))	  // Probably broke out of the loop prematurely
    return res;

  // Recurse to parent:
  tmpNode = parentNode;
  res = tmpNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_SUCCEEDED(res))
  {
    PRInt32 indx = IndexOf(parentNode);
    return CloneSibsAndParents(parentNode, indx, parentClone,
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
  if (NS_FAILED(res))
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

nsresult nsRange::CloneRange(nsIDOMRange** aReturn)
{
  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewRange(aReturn);
  if (NS_FAILED(res))
    return res;

  res = (*aReturn)->SetStart(mStartParent, mStartOffset);
  if (NS_FAILED(res))
    return res;
  
  res = (*aReturn)->SetEnd(mEndParent, mEndOffset);
  return res;
}

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ToString(nsAWritableString& aReturn)
{ 
  nsCOMPtr<nsIContent> cStart( do_QueryInterface(mStartParent) );
  nsCOMPtr<nsIContent> cEnd( do_QueryInterface(mEndParent) );
  
  // clear the string
  aReturn.Truncate();
  
  // If we're unpositioned, return the empty string
  if (!cStart || !cEnd) {
    return NS_OK;
  }

#ifdef DEBUG_range
      printf("Range dump: -----------------------\n");
#endif /* DEBUG */
    
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(mStartParent) );
    
    if (textNode)
    {
#ifdef DEBUG_range
      // If debug, dump it:
      nsCOMPtr<nsIContent> cN (do_QueryInterface(mStartParent));
      if (cN) cN->List(stdout);
      printf("End Range dump: -----------------------\n");
#endif /* DEBUG */

      // grab the text
      if (NS_FAILED(textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn)))
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
  while (cN && (NS_ENUMERATOR_FALSE == iter->IsDone()))
  {
#ifdef DEBUG_range
    // If debug, dump it:
    cN->List(stdout);
#endif /* DEBUG */
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
    nsresult res = iter->Next();
    if (NS_FAILED(res)) // a little noise here to catch bugs
    {
      NS_NOTREACHED("nsRange::ToString() : iterator failed to advance");
      return res;
    }
    iter->CurrentNode(getter_AddRefs(cN));
  }

#ifdef DEBUG_range
  printf("End Range dump: -----------------------\n");
#endif /* DEBUG */
  return NS_OK;
}



nsresult
nsRange::Detach()
{
  return DoSetRange(nsnull,0,nsnull,0);
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

  PRInt32  loop = 0;
  nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
  while (theRange)
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
    theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)));
  }
  return NS_OK;
}
  

nsresult nsRange::OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  nsCOMPtr<nsIContent> removed( do_QueryInterface(aRemovedNode) );

  // any ranges in the content subtree rooted by aRemovedNode need to
  // have the enclosed endpoints promoted up to the parentNode/offset
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res = GetDOMNodeFromContent(parent, &domNode);
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;
  res = PopRanges(domNode, aOffset, removed);

  // quick return if no range list
  nsVoidArray *theRangeList;
  parent->GetRangeList(theRangeList);
  if (!theRangeList) return NS_OK;
  
  PRInt32		loop = 0;
  nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
  while (theRange)
  {
    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    {
      if (theRange->mStartParent == domNode)
      {
        // if child deleted before start, move start offset left one
        if (aOffset < theRange->mStartOffset) theRange->mStartOffset--;
      }
      if (theRange->mEndParent == domNode)
      {
        // if child deleted before end, move end offset left one
        if (aOffset < theRange->mEndOffset) 
        {
          if (theRange->mEndOffset>0) theRange->mEndOffset--;
        }
      }
    }
    loop++;
    theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)));
  }
  
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
  nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
  while (theRange)
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
    theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop)));
  }
  
  return NS_OK;
}


// nsIDOMNSRange interface
NS_IMETHODIMP    
nsRange::CreateContextualFragment(const nsAReadableString& aFragment, 
                                  nsIDOMDocumentFragment** aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIParser> parser;
  nsITagStack* tagStack;

  if (!mIsPositioned) {
    return NS_ERROR_FAILURE;
  }

  // Create a new parser for this entire operation
  result = nsComponentManager::CreateInstance(kCParserCID, 
                                              nsnull, 
                                              kCParserIID, 
                                              (void **)getter_AddRefs(parser));
  if (NS_SUCCEEDED(result)) {
    result = parser->CreateTagStack(&tagStack);

    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDOMNode> parent;
      nsCOMPtr<nsIContent> content(do_QueryInterface(mStartParent, &result));

      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDocument> document;
        nsCOMPtr<nsIDOMDocument> domDocument;
        
        result = content->GetDocument(*getter_AddRefs(document));
        if (document && NS_SUCCEEDED(result)) {
          domDocument = do_QueryInterface(document, &result);
        }

        parent = mStartParent;
        while (parent && 
               (parent != domDocument) && 
               NS_SUCCEEDED(result)) {
          nsCOMPtr<nsIDOMNode> temp;
          nsAutoString tagName;
          PRUnichar* name = nsnull;
          PRUint16 nodeType;
          
          parent->GetNodeType(&nodeType);
          if (nsIDOMNode::ELEMENT_NODE == nodeType) {
            parent->GetNodeName(tagName);
            // XXX Wish we didn't have to allocate here
            name = tagName.ToNewUnicode();
            if (name) {
              tagStack->Push(name);
              temp = parent;
              result = temp->GetParentNode(getter_AddRefs(parent));
            }
            else {
              result = NS_ERROR_OUT_OF_MEMORY;
            }
          }
          else {
            temp = parent;
            result = temp->GetParentNode(getter_AddRefs(parent));
          }
        }
        
        if (NS_SUCCEEDED(result)) {
          nsAutoString contentType;
          nsIHTMLFragmentContentSink* sink;
          
          result = NS_NewHTMLFragmentContentSink(&sink);
          if (NS_SUCCEEDED(result)) {
            parser->SetContentSink(sink);
            if (document) {
              document->GetContentType(contentType);
            }
            else {
              // Who're we kidding. This only works for html.
              contentType.Assign(NS_LITERAL_STRING("text/html"));
            }

            result = parser->ParseFragment(aFragment, (void*)0,
                                           *tagStack,
                                           0, contentType);
            
            if (NS_SUCCEEDED(result)) {
              sink->GetFragment(aReturn);
            }
            
            NS_RELEASE(sink);
          }
        }
      }
        
      // XXX Ick! Delete strings we allocated above.
      PRUnichar* str = nsnull;
      str = tagStack->Pop();
      while (str) {
        nsCRT::free(str);
        str = tagStack->Pop();
      }
      
      // XXX Double Ick! Deleting something that someone else newed.
      delete tagStack;
    }
  }

  return result;
}

NS_IMETHODIMP    
nsRange::IsValidFragment(const nsAReadableString& aFragment, PRBool* aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIParser> parser;
  nsITagStack* tagStack;

  if (!mIsPositioned) {
    return NS_ERROR_FAILURE;
  }

  // Create a new parser for this entire operation
  result = nsComponentManager::CreateInstance(kCParserCID, 
                                              nsnull, 
                                              kCParserIID, 
                                              (void **)getter_AddRefs(parser));
  if (NS_SUCCEEDED(result)) {
    result = parser->CreateTagStack(&tagStack);

    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDOMNode> parent;
      nsCOMPtr<nsIContent> content(do_QueryInterface(mStartParent, &result));

      if (NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDocument> document;
        
        result = content->GetDocument(*getter_AddRefs(document));
        
        if (NS_SUCCEEDED(result)) {
          nsCOMPtr<nsIDOMDocument> domDocument(do_QueryInterface(document, &result));

          if (NS_SUCCEEDED(result)) {
            parent = mStartParent;
            while (parent && 
                   (parent != domDocument) && 
                   NS_SUCCEEDED(result)) {
              nsCOMPtr<nsIDOMNode> temp;
              nsAutoString tagName;
              PRUnichar* name = nsnull;
              
              parent->GetNodeName(tagName);
              // XXX Wish we didn't have to allocate here
              name = tagName.ToNewUnicode();
              if (name) {
                tagStack->Push(name);
                temp = parent;
                result = temp->GetParentNode(getter_AddRefs(parent));
              }
              else {
                result = NS_ERROR_OUT_OF_MEMORY;
              }
            }
            
            if (NS_SUCCEEDED(result)) {
              nsAutoString contentType;

              document->GetContentType(contentType);
              *aReturn = parser->IsValidFragment(aFragment,
                                                 *tagStack,
                                                 0, contentType);
            }
          }
        }
      }
        
      // XXX Ick! Delete strings we allocated above.
      PRUnichar* str = nsnull;
      str = tagStack->Pop();
      while (str) {
        nsCRT::free(str);
        str = tagStack->Pop();
      }
      
      // XXX Double Ick! Deleting something that someone else newed.
      delete tagStack;
    }
  }

  return result;
}

NS_IMETHODIMP
nsRange::GetHasGeneratedBefore(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  *aBool = mBeforeGenContent;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::GetHasGeneratedAfter(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  *aBool = mAfterGenContent;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetHasGeneratedBefore(PRBool aBool)
{
  mBeforeGenContent = aBool;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetHasGeneratedAfter(PRBool aBool)
{
  mAfterGenContent = aBool;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetBeforeAndAfter(PRBool aBefore, PRBool aAfter)
{
  mBeforeGenContent = aBefore;
  mBeforeGenContent = aAfter;
  return NS_OK;
}

// BEGIN nsIScriptObjectOwner interface implementations
NS_IMETHODIMP
nsRange::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *globalObj = aContext->GetGlobalObject();

  if (!mScriptObject) {
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

// END nsIScriptObjectOwner interface implementations

nsresult
nsRange::Lock()
{
  if (!mMonitor)
    mMonitor = ::PR_NewMonitor();

  if (mMonitor)
    PR_EnterMonitor(mMonitor);

  return NS_OK;
}

nsresult
nsRange::Unlock()
{
  if (mMonitor)
    PR_ExitMonitor(mMonitor);

  return NS_OK;
}

