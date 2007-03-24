/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Implementation of the DOM nsIDOMRange object.
 */

#include "nscore.h"
#include "nsRange.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMText.h"
#include "nsDOMError.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);

/******************************************************
 * stack based utilty class for managing monitor
 ******************************************************/

// NS_ERROR_DOM_NOT_OBJECT_ERR is not the correct one to throw, but spec doesn't say
// what is
#define VALIDATE_ACCESS(node_)                                                     \
  PR_BEGIN_MACRO                                                                   \
    if (!node_) {                                                                  \
      return NS_ERROR_DOM_NOT_OBJECT_ERR;                                          \
    }                                                                              \
    if (!nsContentUtils::CanCallerAccess(node_)) {                                 \
      return NS_ERROR_DOM_SECURITY_ERR;                                            \
    }                                                                              \
    if (mIsDetached) {                                                             \
      return NS_ERROR_DOM_INVALID_STATE_ERR;                                       \
    }                                                                              \
  PR_END_MACRO

// Utility routine to detect if a content node is completely contained in a range
// If outNodeBefore is returned true, then the node starts before the range does.
// If outNodeAfter is returned true, then the node ends after the range does.
// Note that both of the above might be true.
// If neither are true, the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range! 

// static
nsresult
nsRange::CompareNodeToRange(nsIContent* aNode, nsIDOMRange* aRange,
                            PRBool *outNodeBefore, PRBool *outNodeAfter)
{
  nsresult rv;
  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return CompareNodeToRange(aNode, range, outNodeBefore, outNodeAfter);
}

// static
nsresult
nsRange::CompareNodeToRange(nsIContent* aNode, nsIRange* aRange,
                            PRBool *outNodeBefore, PRBool *outNodeAfter)
{
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.
  
  nsresult rv;
  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!range->IsPositioned()) 
    return NS_ERROR_UNEXPECTED; 
  
  // gather up the dom point info
  PRInt32 nodeStart, nodeEnd;
  nsINode* parent = aNode->GetNodeParent();
  if (!parent) {
    // can't make a parent/offset pair to represent start or 
    // end of the root node, becasue it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    parent = aNode;
    nodeStart = 0;
    nodeEnd = aNode->GetChildCount();
    if (!nodeEnd) {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    nodeStart = parent->IndexOf(aNode);
    nodeEnd = nodeStart + 1;
  }

  nsINode* rangeStartParent = range->GetStartParent();
  nsINode* rangeEndParent = range->GetEndParent();
  PRInt32 rangeStartOffset = range->StartOffset();
  PRInt32 rangeEndOffset = range->EndOffset();

  // is RANGE(start) <= NODE(start) ?
  *outNodeBefore = nsContentUtils::ComparePoints(rangeStartParent,
                                                 rangeStartOffset,
                                                 parent, nodeStart) > 0;
  // is RANGE(end) >= NODE(end) ?
  *outNodeAfter = nsContentUtils::ComparePoints(rangeEndParent,
                                                rangeEndOffset,
                                                parent, nodeEnd) < 0;

  return NS_OK;
}

/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRangeUtils(nsIRangeUtils** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsRangeUtils* rangeUtil = new nsRangeUtils();
  if (!rangeUtil) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(rangeUtil, aResult);
}

/******************************************************
 * nsISupports
 ******************************************************/
NS_IMPL_ISUPPORTS1(nsRangeUtils, nsIRangeUtils)

/******************************************************
 * nsIRangeUtils methods
 ******************************************************/
 
NS_IMETHODIMP_(PRInt32) 
nsRangeUtils::ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                            nsIDOMNode* aParent2, PRInt32 aOffset2)
{
  nsCOMPtr<nsINode> parent1 = do_QueryInterface(aParent1);
  nsCOMPtr<nsINode> parent2 = do_QueryInterface(aParent2);

  NS_ENSURE_TRUE(parent1 && parent2, -1);

  return nsContentUtils::ComparePoints(parent1, aOffset1, parent2, aOffset2);
}

NS_IMETHODIMP
nsRangeUtils::CompareNodeToRange(nsIContent* aNode, nsIDOMRange* aRange,
                                 PRBool *outNodeBefore, PRBool *outNodeAfter)
{
  return nsRange::CompareNodeToRange(aNode, aRange, outNodeBefore,
                                     outNodeAfter);
}

/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRange(nsIDOMRange** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsRange * range = new nsRange();
  if (!range) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(range, aResult);
}

/******************************************************
 * constructor/destructor
 ******************************************************/

nsRange::~nsRange() 
{
  DoSetRange(nsnull, 0, nsnull, 0, nsnull);
  // we want the side effects (releases and list removals)
} 

/******************************************************
 * nsISupports
 ******************************************************/

// QueryInterface implementation for nsRange
NS_INTERFACE_MAP_BEGIN(nsRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRange)
  NS_INTERFACE_MAP_ENTRY(nsIRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSRange)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRange)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Range)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)

/******************************************************
 * nsIMutationObserver implementation
 ******************************************************/

void
nsRange::CharacterDataChanged(nsIDocument* aDocument,
                              nsIContent* aContent,
                              CharacterDataChangeInfo* aInfo)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  // If the changed node contains our start boundry and the change starts
  // before the boundry we'll need to adjust the offset.
  if (aContent == mStartParent &&
      aInfo->mChangeStart < (PRUint32)mStartOffset) {
    // If boundry is inside changed text, position it before change
    // else adjust start offset for the change in length
    mStartOffset = (PRUint32)mStartOffset < aInfo->mChangeEnd ?
       aInfo->mChangeStart :
       mStartOffset + aInfo->mChangeStart - aInfo->mChangeEnd +
         aInfo->mReplaceLength;
  }

  // Do same thing for end boundry.
  if (aContent == mEndParent && aInfo->mChangeStart < (PRUint32)mEndOffset) {
    mEndOffset = (PRUint32)mEndOffset < aInfo->mChangeEnd ?
       aInfo->mChangeStart :
       mEndOffset + aInfo->mChangeStart - aInfo->mChangeEnd +
         aInfo->mReplaceLength;
  }
}

void
nsRange::ContentInserted(nsIDocument* aDocument,
                         nsIContent* aContainer,
                         nsIContent* aChild,
                         PRInt32 aIndexInContainer)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* container = NODE_FROM(aContainer, aDocument);

  // Adjust position if a sibling was inserted.
  if (container == mStartParent && aIndexInContainer < mStartOffset) {
    ++mStartOffset;
  }
  if (container == mEndParent && aIndexInContainer < mEndOffset) {
    ++mEndOffset;
  }
}

void
nsRange::ContentRemoved(nsIDocument* aDocument,
                        nsIContent* aContainer,
                        nsIContent* aChild,
                        PRInt32 aIndexInContainer)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  nsINode* container = NODE_FROM(aContainer, aDocument);

  // Adjust position if a sibling was removed...
  if (container == mStartParent && aIndexInContainer < mStartOffset) {
    --mStartOffset;
  }
  // ...or gravitate if an ancestor was removed.
  else if (nsContentUtils::ContentIsDescendantOf(mStartParent, aChild)) {
    mStartParent = container;
    mStartOffset = aIndexInContainer;
  }

  // Do same thing for end boundry.
  if (container == mEndParent && aIndexInContainer < mEndOffset) {
    --mEndOffset;
  }
  else if (nsContentUtils::ContentIsDescendantOf(mEndParent, aChild)) {
    mEndParent = container;
    mEndOffset = aIndexInContainer;
  }
}

void
nsRange::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_ASSERTION(mIsPositioned, "shouldn't be notified if not positioned");

  // No need to detach, but reset positions so that the endpoints don't
  // end up disconnected from each other.
  // An alternative solution would be to make mRoot a strong pointer.
  DoSetRange(nsnull, 0, nsnull, 0, nsnull);
}

/********************************************************
 * Utilities for comparing points: API from nsIDOMNSRange
 ********************************************************/
NS_IMETHODIMP
nsRange::IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult)
{
  PRInt16 compareResult = 0;
  nsresult rv = ComparePoint(aParent, aOffset, &compareResult);
  *aResult = compareResult == 0;

  return rv;
}
  
// returns -1 if point is before range, 0 if point is in range,
// 1 if point is after range.
NS_IMETHODIMP
nsRange::ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aResult)
{
  if (mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // our range is in a good state?
  if (!mIsPositioned) 
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);

  if (!nsContentUtils::ContentIsDescendantOf(parent, mRoot)) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }
  
  PRInt32 cmp;
  if ((cmp = nsContentUtils::ComparePoints(parent, aOffset,
                                           mStartParent, mStartOffset)) <= 0) {
    
    *aResult = cmp;
  }
  else if (nsContentUtils::ComparePoints(mEndParent, mEndOffset,
                                         parent, aOffset) == -1) {
    *aResult = 1;
  }
  else {
    *aResult = 0;
  }
  
  return NS_OK;
}
  
/******************************************************
 * Private helper routines
 ******************************************************/

static nsINode* IsValidBoundary(nsINode* aNode);

// Get the length of aNode
static PRInt32 GetNodeLength(nsINode *aNode)
{
  if(aNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    return NS_STATIC_CAST(nsIContent*, aNode)->TextLength();
  }

  return aNode->GetChildCount();
}

// It's important that all setting of the range start/end points 
// go through this function, which will do all the right voodoo
// for content notification of range ownership.  
// Calling DoSetRange with either parent argument null will collapse
// the range to have both endpoints point to the other node
void
nsRange::DoSetRange(nsINode* aStartN, PRInt32 aStartOffset,
                    nsINode* aEndN, PRInt32 aEndOffset,
                    nsINode* aRoot)
{
  NS_PRECONDITION((aStartN && aEndN && aRoot) ||
                  (!aStartN && !aEndN && !aRoot),
                  "Set all or none");
  NS_PRECONDITION(!aRoot ||
                  (nsContentUtils::ContentIsDescendantOf(aStartN, aRoot) &&
                   nsContentUtils::ContentIsDescendantOf(aEndN, aRoot) &&
                   aRoot == IsValidBoundary(aStartN) &&
                   aRoot == IsValidBoundary(aEndN)),
                  "Wrong root");
  NS_PRECONDITION(!aRoot ||
                  (aStartN->IsNodeOfType(nsINode::eCONTENT) &&
                   aEndN->IsNodeOfType(nsINode::eCONTENT) &&
                   aRoot ==
                    NS_STATIC_CAST(nsIContent*, aStartN)->GetBindingParent() &&
                   aRoot ==
                    NS_STATIC_CAST(nsIContent*, aEndN)->GetBindingParent()) ||
                  (!aRoot->GetNodeParent() &&
                   (aRoot->IsNodeOfType(nsINode::eDOCUMENT) ||
                    aRoot->IsNodeOfType(nsINode::eATTRIBUTE) ||
                    aRoot->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT))),
                  "Bad root");

  if (mRoot != aRoot) {
    if (mRoot) {
      mRoot->RemoveMutationObserver(this);
    }
    if (aRoot) {
      aRoot->AddMutationObserver(this);
    }
  }
 
  mStartParent = aStartN;
  mStartOffset = aStartOffset;
  mEndParent = aEndN;
  mEndOffset = aEndOffset;
  mIsPositioned = !!mStartParent;
  mRoot = aRoot;
}

static PRInt32
IndexOf(nsIDOMNode* aChildNode)
{
  // convert node to nsIContent, so that we can find the child index

  nsCOMPtr<nsINode> child = do_QueryInterface(aChildNode);
  if (!child) {
    return -1;
  }

  nsINode *parent = child->GetNodeParent();

  // finally we get the index
  return parent ? parent->IndexOf(child) : -1;
}

/******************************************************
 * nsIRange implementation
 ******************************************************/

nsINode*
nsRange::GetCommonAncestor()
{
  return mIsPositioned ?
    nsContentUtils::GetCommonAncestor(mStartParent, mEndParent) :
    nsnull;
}

void
nsRange::Reset()
{
  DoSetRange(nsnull, 0, nsnull, 0, nsnull);
}

/******************************************************
 * public functionality
 ******************************************************/

nsresult nsRange::GetStartContainer(nsIDOMNode** aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mStartParent, aStartParent);
}

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aStartOffset = mStartOffset;

  return NS_OK;
}

nsresult nsRange::GetEndContainer(nsIDOMNode** aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  return CallQueryInterface(mEndParent, aEndParent);
}

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aEndOffset = mEndOffset;

  return NS_OK;
}

nsresult nsRange::GetCollapsed(PRBool* aIsCollapsed)
{
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  *aIsCollapsed = Collapsed();

  return NS_OK;
}

nsresult nsRange::GetCommonAncestorContainer(nsIDOMNode** aCommonParent)
{
  *aCommonParent = nsnull;
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  nsINode* container = nsContentUtils::GetCommonAncestor(mStartParent, mEndParent);
  if (container) {
    return CallQueryInterface(container, aCommonParent);
  }

  return NS_ERROR_NOT_INITIALIZED;
}

static nsINode*
IsValidBoundary(nsINode* aNode)
{
  if (!aNode) {
    return nsnull;
  }

  if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
    nsIContent* content = NS_STATIC_CAST(nsIContent*, aNode);
    if (content->Tag() == nsGkAtoms::documentTypeNodeName) {
      return nsnull;
    }

    // If the node has a binding parent, that should be the root.
    // XXXbz maybe only for native anonymous content?
    nsINode* root = content->GetBindingParent();
    if (root) {
      return root;
    }
  }

  // Elements etc. must be in document or in document fragment,
  // text nodes in document, in document fragment or in attribute.
  nsINode* root = aNode->GetCurrentDoc();
  if (root) {
    return root;
  }

  root = aNode;
  while ((aNode = aNode->GetNodeParent())) {
    root = aNode;
  }

  NS_ASSERTION(!root->IsNodeOfType(nsINode::eDOCUMENT),
               "GetCurrentDoc should have returned a doc");

  if (root->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT) ||
      root->IsNodeOfType(nsINode::eATTRIBUTE)) {
    return root;
  }

#ifdef DEBUG_smaug
  nsCOMPtr<nsIContent> cont = do_QueryInterface(root);
  if (cont) {
    nsAutoString name;
    cont->Tag()->ToString(name);
    printf("nsRange::IsValidBoundary: node is not a valid boundary point [%s]\n",
           NS_ConvertUTF16toUTF8(name).get());
  }
#endif

  return nsnull;
}

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{
  VALIDATE_ACCESS(aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsINode* newRoot = IsValidBoundary(parent);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR);

  PRInt32 len = GetNodeLength(parent);
  if (aOffset < 0 || aOffset > len)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new start is after end.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(parent, aOffset,
                                    mEndParent, mEndOffset) == 1) {
    DoSetRange(parent, aOffset, parent, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(parent, aOffset, mEndParent, mEndOffset, mRoot);
  
  return NS_OK;
}

nsresult nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  VALIDATE_ACCESS(aSibling);
  
  nsCOMPtr<nsIDOMNode> parent;
  nsresult rv = aSibling->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(rv) || !parent) {
    return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  return SetStart(parent, IndexOf(aSibling));
}

nsresult nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  VALIDATE_ACCESS(aSibling);

  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) {
    return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  return SetStart(nParent, IndexOf(aSibling) + 1);
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  VALIDATE_ACCESS(aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsINode* newRoot = IsValidBoundary(parent);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR);

  PRInt32 len = GetNodeLength(parent);
  if (aOffset < 0 || aOffset > len) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Collapse if not positioned yet, if positioned in another doc or
  // if the new end is before start.
  if (!mIsPositioned || newRoot != mRoot ||
      nsContentUtils::ComparePoints(mStartParent, mStartOffset,
                                    parent, aOffset) == 1) {
    DoSetRange(parent, aOffset, parent, aOffset, newRoot);

    return NS_OK;
  }

  DoSetRange(mStartParent, mStartOffset, parent, aOffset, mRoot);

  return NS_OK;
}

nsresult nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  VALIDATE_ACCESS(aSibling);
  
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult rv = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(rv) || !nParent) {
    return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  return SetEnd(nParent, IndexOf(aSibling));
}

nsresult nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  VALIDATE_ACCESS(aSibling);
  
  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) {
    return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  return SetEnd(nParent, IndexOf(aSibling) + 1);
}

nsresult nsRange::Collapse(PRBool aToStart)
{
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  if (aToStart)
    DoSetRange(mStartParent, mStartOffset, mStartParent, mStartOffset, mRoot);
  else
    DoSetRange(mEndParent, mEndOffset, mEndParent, mEndOffset, mRoot);

  return NS_OK;
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  VALIDATE_ACCESS(aN);
  
  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  NS_ENSURE_TRUE(node, NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR);

  nsINode* parent = node->GetNodeParent();
  nsINode* newRoot = IsValidBoundary(parent);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR);

  PRInt32 index = parent->IndexOf(node);
  if (index < 0) {
    return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  DoSetRange(parent, index, parent, index + 1, newRoot);
  
  return NS_OK;
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  VALIDATE_ACCESS(aN);

  nsCOMPtr<nsINode> node = do_QueryInterface(aN);
  nsINode* newRoot = IsValidBoundary(node);
  NS_ENSURE_TRUE(newRoot, NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR);
  
  DoSetRange(node, 0, node, GetNodeLength(node), newRoot);
  
  return NS_OK;
}

// The Subtree Content Iterator only returns subtrees that are
// completely within a given range. It doesn't return a CharacterData
// node that contains either the start or end point of the range.
// We need an iterator that will also include these start/end points
// so that our methods/algorithms aren't cluttered with special
// case code that tries to include these points while iterating.
//
// The RangeSubtreeIterator class mimics the nsIContentIterator
// methods we need, so should the Content Iterator support the
// start/end points in the future, we can switchover relatively
// easy.

class RangeSubtreeIterator
{
private:

  enum RangeSubtreeIterState { eDone=0,
                               eUseStartCData,
                               eUseIterator,
                               eUseEndCData };

  nsCOMPtr<nsIContentIterator>  mIter;
  RangeSubtreeIterState         mIterState;

  nsCOMPtr<nsIDOMCharacterData> mStartCData;
  nsCOMPtr<nsIDOMCharacterData> mEndCData;

public:

  RangeSubtreeIterator()
    : mIterState(eDone)
  {
  }
  ~RangeSubtreeIterator()
  {
  }

  nsresult Init(nsIDOMRange *aRange);
  already_AddRefed<nsIDOMNode> GetCurrentNode();
  void First();
  void Last();
  void Next();
  void Prev();

  PRBool IsDone()
  {
    return mIterState == eDone;
  }
};

nsresult
RangeSubtreeIterator::Init(nsIDOMRange *aRange)
{
  mIterState = eDone;

  nsCOMPtr<nsIDOMNode> node;

  // Grab the start point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  nsresult res = aRange->GetStartContainer(getter_AddRefs(node));
  if (!node) return NS_ERROR_FAILURE;

  mStartCData = do_QueryInterface(node);

  // Grab the end point of the range and QI it to
  // a CharacterData pointer. If it is CharacterData store
  // a pointer to the node.

  res = aRange->GetEndContainer(getter_AddRefs(node));
  if (!node) return NS_ERROR_FAILURE;

  mEndCData = do_QueryInterface(node);

  if (mStartCData && mStartCData == mEndCData)
  {
    // The range starts and stops in the same CharacterData
    // node. Null out the end pointer so we only visit the
    // node once!

    mEndCData = nsnull;
  }
  else
  {
    // Now create a Content Subtree Iterator to be used
    // for the subtrees between the end points!

    res = NS_NewContentSubtreeIterator(getter_AddRefs(mIter));
    if (NS_FAILED(res)) return res;

    res = mIter->Init(aRange);
    if (NS_FAILED(res)) return res;

    if (mIter->IsDone())
    {
      // The subtree iterator thinks there's nothing
      // to iterate over, so just free it up so we
      // don't accidentally call into it.

      mIter = nsnull;
    }
  }

  // Initialize the iterator by calling First().
  // Note that we are ignoring the return value on purpose!

  First();

  return NS_OK;
}

already_AddRefed<nsIDOMNode>
RangeSubtreeIterator::GetCurrentNode()
{
  nsIDOMNode *node = nsnull;

  if (mIterState == eUseStartCData && mStartCData) {
    NS_ADDREF(node = mStartCData);
  } else if (mIterState == eUseEndCData && mEndCData)
    NS_ADDREF(node = mEndCData);
  else if (mIterState == eUseIterator && mIter)
  {
    nsIContent *content = mIter->GetCurrentNode();

    if (content) {
      CallQueryInterface(content, &node);
    }
  }

  return node;
}

void
RangeSubtreeIterator::First()
{
  if (mStartCData)
    mIterState = eUseStartCData;
  else if (mIter)
  {
    mIter->First();

    mIterState = eUseIterator;
  }
  else if (mEndCData)
    mIterState = eUseEndCData;
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Last()
{
  if (mEndCData)
    mIterState = eUseEndCData;
  else if (mIter)
  {
    mIter->Last();

    mIterState = eUseIterator;
  }
  else if (mStartCData)
    mIterState = eUseStartCData;
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Next()
{
  if (mIterState == eUseStartCData)
  {
    if (mIter)
    {
      mIter->First();

      mIterState = eUseIterator;
    }
    else if (mEndCData)
      mIterState = eUseEndCData;
    else
      mIterState = eDone;
  }
  else if (mIterState == eUseIterator)
  {
    mIter->Next();

    if (mIter->IsDone())
    {
      if (mEndCData)
        mIterState = eUseEndCData;
      else
        mIterState = eDone;
    }
  }
  else
    mIterState = eDone;
}

void
RangeSubtreeIterator::Prev()
{
  if (mIterState == eUseEndCData)
  {
    if (mIter)
    {
      mIter->Last();

      mIterState = eUseIterator;
    }
    else if (mStartCData)
      mIterState = eUseStartCData;
    else
      mIterState = eDone;
  }
  else if (mIterState == eUseIterator)
  {
    mIter->Prev();

    if (mIter->IsDone())
    {
      if (mStartCData)
        mIterState = eUseStartCData;
      else
        mIterState = eDone;
    }
  }
  else
    mIterState = eDone;
}


// CollapseRangeAfterDelete() is a utility method that is used by
// DeleteContents() and ExtractContents() to collapse the range
// in the correct place, under the range's root container (the
// range end points common container) as outlined by the Range spec:
//
// http://www.w3.org/TR/2000/REC-DOM-Level-2-Traversal-Range-20001113/ranges.html
// The assumption made by this method is that the delete or extract
// has been done already, and left the range in a state where there is
// no content between the 2 end points.

static nsresult
CollapseRangeAfterDelete(nsIDOMRange *aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Check if range gravity took care of collapsing the range for us!

  PRBool isCollapsed = PR_FALSE;
  nsresult res = aRange->GetCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  if (isCollapsed)
  {
    // aRange is collapsed so there's nothing for us to do.
    //
    // There are 2 possible scenarios here:
    //
    // 1. aRange could've been collapsed prior to the delete/extract,
    //    which would've resulted in nothing being removed, so aRange
    //    is already where it should be.
    //
    // 2. Prior to the delete/extract, aRange's start and end were in
    //    the same container which would mean everything between them
    //    was removed, causing range gravity to collapse the range.

    return NS_OK;
  }

  // aRange isn't collapsed so figure out the appropriate place to collapse!
  // First get both end points and their common ancestor.

  nsCOMPtr<nsIDOMNode> commonAncestor;
  res = aRange->GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  if(NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> startContainer, endContainer;

  res = aRange->GetStartContainer(getter_AddRefs(startContainer));
  if (NS_FAILED(res)) return res;

  res = aRange->GetEndContainer(getter_AddRefs(endContainer));
  if (NS_FAILED(res)) return res;

  // Collapse to one of the end points if they are already in the
  // commonAncestor. This should work ok since this method is called
  // immediately after a delete or extract that leaves no content
  // between the 2 end points!

  if (startContainer == commonAncestor)
    return aRange->Collapse(PR_TRUE);
  if (endContainer == commonAncestor)
    return aRange->Collapse(PR_FALSE);

  // End points are at differing levels. We want to collapse to the
  // point that is between the 2 subtrees that contain each point,
  // under the common ancestor.

  nsCOMPtr<nsIDOMNode> nodeToSelect(startContainer), parent;

  while (nodeToSelect)
  {
    nsresult res = nodeToSelect->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;

    if (parent == commonAncestor)
      break; // We found the nodeToSelect!

    nodeToSelect = parent;
  }

  if (!nodeToSelect)
    return NS_ERROR_FAILURE; // This should never happen!

  res = aRange->SelectNode(nodeToSelect);
  if (NS_FAILED(res)) return res;

  return aRange->Collapse(PR_FALSE);
}

nsresult nsRange::DeleteContents()
{ 
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(mRoot ? mRoot->GetOwnerDoc(): nsnull, nsnull);

  // Save the range end points locally to avoid interference
  // of Range gravity during our edits!

  nsCOMPtr<nsIDOMNode> startContainer = do_QueryInterface(mStartParent);
  PRInt32              startOffset = mStartOffset;
  nsCOMPtr<nsIDOMNode> endContainer = do_QueryInterface(mEndParent);
  PRInt32              endOffset = mEndOffset;

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  nsresult res = iter.Init(this);
  if (NS_FAILED(res)) return res;

  if (iter.IsDone())
  {
    // There's nothing for us to delete.
    return CollapseRangeAfterDelete(this);
  }

  // We delete backwards to avoid iterator problems!

  iter.Last();

  PRBool handled = PR_FALSE;

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());

    // Before we delete anything, advance the iterator to the
    // next subtree.

    iter.Prev();

    handled = PR_FALSE;

    // If it's CharacterData, make sure we might need to delete
    // part of the data, instead of removing the whole node.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(node));

    if (charData)
    {
      PRUint32 dataLength = 0;

      if (node == startContainer)
      {
        if (node == endContainer)
        {
          // This range is completely contained within a single text node.
          // Delete the data between startOffset and endOffset.

          if (endOffset > startOffset)
          {
            res = charData->DeleteData(startOffset, endOffset - startOffset);
            if (NS_FAILED(res)) return res;
          }

          handled = PR_TRUE;
        }
        else
        {
          // Delete everything after startOffset.

          res = charData->GetLength(&dataLength);
          if (NS_FAILED(res)) return res;

          if (dataLength > (PRUint32)startOffset)
          {
            res = charData->DeleteData(startOffset, dataLength - startOffset);
            if (NS_FAILED(res)) return res;
          }

          handled = PR_TRUE;
        }
      }
      else if (node == endContainer)
      {
        // Delete the data between 0 and endOffset.

        if (endOffset > 0)
        {
          res = charData->DeleteData(0, endOffset);
          if (NS_FAILED(res)) return res;
        }

        handled = PR_TRUE;
      }       
    }

    if (!handled)
    {
      // node was not handled above, so it must be completely contained
      // within the range. Just remove it from the tree!

      nsCOMPtr<nsIDOMNode> parent, tmpNode;

      node->GetParentNode(getter_AddRefs(parent));

      if (parent) {
        res = parent->RemoveChild(node, getter_AddRefs(tmpNode));
        if (NS_FAILED(res)) return res;
      }
    }
  }

  // XXX_kin: At this point we should be checking for the case
  // XXX_kin: where we have 2 adjacent text nodes left, each
  // XXX_kin: containing one of the range end points. The spec
  // XXX_kin: says the 2 nodes should be merged in that case,
  // XXX_kin: and to use Normalize() to do the merging, but
  // XXX_kin: calling Normalize() on the common parent to accomplish
  // XXX_kin: this might also normalize nodes that are outside the
  // XXX_kin: range but under the common parent. Need to verify
  // XXX_kin: with the range commitee members that this was the
  // XXX_kin: desired behavior. For now we don't merge anything!

  return CollapseRangeAfterDelete(this);
}

NS_IMETHODIMP
nsRange::CompareBoundaryPoints(PRUint16 aHow, nsIDOMRange* aOtherRange,
                               PRInt16* aCmpRet)
{
  nsCOMPtr<nsIRange> otherRange = do_QueryInterface(aOtherRange);
  NS_ENSURE_TRUE(otherRange, NS_ERROR_NULL_POINTER);

  if(mIsDetached || otherRange->IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned || !otherRange->IsPositioned())
    return NS_ERROR_NOT_INITIALIZED;

  nsINode *ourNode, *otherNode;
  PRInt32 ourOffset, otherOffset;

  switch (aHow) {
    case nsIDOMRange::START_TO_START:
      ourNode = mStartParent;
      ourOffset = mStartOffset;
      otherNode = otherRange->GetStartParent();
      otherOffset = otherRange->StartOffset();
      break;
    case nsIDOMRange::START_TO_END:
      ourNode = mEndParent;
      ourOffset = mEndOffset;
      otherNode = otherRange->GetStartParent();
      otherOffset = otherRange->StartOffset();
      break;
    case nsIDOMRange::END_TO_START:
      ourNode = mStartParent;
      ourOffset = mStartOffset;
      otherNode = otherRange->GetEndParent();
      otherOffset = otherRange->EndOffset();
      break;
    case nsIDOMRange::END_TO_END:
      ourNode = mEndParent;
      ourOffset = mEndOffset;
      otherNode = otherRange->GetEndParent();
      otherOffset = otherRange->EndOffset();
      break;
    default:
      // We were passed an illegal value
      return NS_ERROR_ILLEGAL_VALUE;
  }

  if (mRoot != otherRange->GetRoot())
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;

  *aCmpRet = nsContentUtils::ComparePoints(ourNode, ourOffset,
                                           otherNode, otherOffset);

  return NS_OK;
}

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ 
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(mRoot ? mRoot->GetOwnerDoc(): nsnull, nsnull);

  // XXX_kin: The spec says that nodes that are completely in the
  // XXX_kin: range should be moved into the document fragment, not
  // XXX_kin: copied. This method will have to be rewritten using
  // XXX_kin: DeleteContents() as a template, with the charData cloning
  // XXX_kin: code from CloneContents() merged in.

  nsresult res = CloneContents(aReturn);
  if (NS_FAILED(res))
    return res;
  res = DeleteContents();
  return res; 
}

static nsresult
CloneParentsBetween(nsIDOMNode *aAncestor,
                    nsIDOMNode *aNode,
                    nsIDOMNode **aClosestAncestor,
                    nsIDOMNode **aFarthestAncestor)
{
  NS_ENSURE_ARG_POINTER((aAncestor && aNode && aClosestAncestor && aFarthestAncestor));

  *aClosestAncestor  = nsnull;
  *aFarthestAncestor = nsnull;

  if (aAncestor == aNode)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> parent, firstParent, lastParent;

  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));

  while(parent && parent != aAncestor)
  {
    nsCOMPtr<nsIDOMNode> clone, tmpNode;

    res = parent->CloneNode(PR_FALSE, getter_AddRefs(clone));

    if (NS_FAILED(res)) return res;
    if (!clone)         return NS_ERROR_FAILURE;

    if (! firstParent)
      firstParent = lastParent = clone;
    else
    {
      res = clone->AppendChild(lastParent, getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;

      lastParent = clone;
    }

    tmpNode = parent;
    res = tmpNode->GetParentNode(getter_AddRefs(parent));
  }

  *aClosestAncestor  = firstParent;
  NS_IF_ADDREF(*aClosestAncestor);

  *aFarthestAncestor = lastParent;
  NS_IF_ADDREF(*aFarthestAncestor);

  return NS_OK;
}

nsresult nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  if (IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsresult res;
  nsCOMPtr<nsIDOMNode> commonAncestor;
  res = GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMDocument> document =
    do_QueryInterface(mStartParent->GetOwnerDoc());
  NS_ASSERTION(document, "CloneContents needs a document to continue.");
  if (!document) return NS_ERROR_FAILURE;

  // Create a new document fragment in the context of this document,
  // which might be null

  nsCOMPtr<nsIDOMDocumentFragment> clonedFrag;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));

  res = NS_NewDocumentFragment(getter_AddRefs(clonedFrag),
                               doc->NodeInfoManager());
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> commonCloneAncestor(do_QueryInterface(clonedFrag));
  if (!commonCloneAncestor) return NS_ERROR_FAILURE;

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  res = iter.Init(this);
  if (NS_FAILED(res)) return res;

  if (iter.IsDone())
  {
    // There's nothing to add to the doc frag, we must be done!

    *aReturn = clonedFrag;
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
  }

  iter.First();

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.
  //
  // Unfortunately these subtrees don't contain the parent hierarchy/context
  // that the Range spec requires us to return. This loop clones the
  // parent hierarchy, adds a cloned version of the subtree, to it, then
  // correctly places this new subtree into the doc fragment.

  while (!iter.IsDone())
  {
    nsCOMPtr<nsIDOMNode> node(iter.GetCurrentNode());
    nsCOMPtr<nsINode> iNode = do_QueryInterface(node);
    // Clone the current subtree!

    nsCOMPtr<nsIDOMNode> clone;
    res = node->CloneNode(PR_TRUE, getter_AddRefs(clone));
    if (NS_FAILED(res)) return res;

    // If it's CharacterData, make sure we only clone what
    // is in the range.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(clone));

    if (charData)
    {
      if (iNode == mEndParent)
      {
        // We only need the data before mEndOffset, so get rid of any
        // data after it.

        PRUint32 dataLength = 0;
        res = charData->GetLength(&dataLength);
        if (NS_FAILED(res)) return res;

        if (dataLength > (PRUint32)mEndOffset)
        {
          res = charData->DeleteData(mEndOffset, dataLength - mEndOffset);
          if (NS_FAILED(res)) return res;
        }
      }       

      if (iNode == mStartParent)
      {
        // We don't need any data before mStartOffset, so just
        // delete it!

        if (mStartOffset > 0)
        {
          res = charData->DeleteData(0, mStartOffset);
          if (NS_FAILED(res)) return res;
        }
      }
    }

    // Clone the parent hierarchy between commonAncestor and node.

    nsCOMPtr<nsIDOMNode> closestAncestor, farthestAncestor;

    res = CloneParentsBetween(commonAncestor, node,
                              getter_AddRefs(closestAncestor),
                              getter_AddRefs(farthestAncestor));

    if (NS_FAILED(res)) return res;

    // Hook the parent hierarchy/context of the subtree into the clone tree.

    nsCOMPtr<nsIDOMNode> tmpNode;

    if (farthestAncestor)
    {
      res = commonCloneAncestor->AppendChild(farthestAncestor,
                                             getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;
    }

    // Place the cloned subtree into the cloned doc frag tree!

    if (closestAncestor)
    {
      // Append the subtree under closestAncestor since it is the
      // immediate parent of the subtree.

      res = closestAncestor->AppendChild(clone, getter_AddRefs(tmpNode));
    }
    else
    {
      // If we get here, there is no missing parent hierarchy between 
      // commonAncestor and node, so just append clone to commonCloneAncestor.

      res = commonCloneAncestor->AppendChild(clone, getter_AddRefs(tmpNode));
    }
    if (NS_FAILED(res)) return res;

    // Get the next subtree to be processed. The idea here is to setup
    // the parameters for the next iteration of the loop.

    iter.Next();

    if (iter.IsDone())
      break; // We must be done!

    nsCOMPtr<nsIDOMNode> nextNode(iter.GetCurrentNode());
    if (!nextNode) return NS_ERROR_FAILURE;

    // Get node and nextNode's common parent.
    nsContentUtils::GetCommonAncestor(node, nextNode, getter_AddRefs(commonAncestor));

    if (!commonAncestor)
      return NS_ERROR_FAILURE;

    // Find the equivalent of commonAncestor in the cloned tree!

    while (node && node != commonAncestor)
    {
      tmpNode = node;
      res = tmpNode->GetParentNode(getter_AddRefs(node));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_FAILURE;

      tmpNode = clone;
      res = tmpNode->GetParentNode(getter_AddRefs(clone));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_FAILURE;
    }

    commonCloneAncestor = clone;
  }

  *aReturn = clonedFrag;
  NS_IF_ADDREF(*aReturn);

  return NS_OK;
}

nsresult nsRange::CloneRange(nsIDOMRange** aReturn)
{
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

  nsRange* range = new nsRange();
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aReturn = range);
  
  range->DoSetRange(mStartParent, mStartOffset, mEndParent, mEndOffset, mRoot);

  return NS_OK;
}

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{
  VALIDATE_ACCESS(aN);
  
  nsresult res;
  PRInt32 tStartOffset;
  this->GetStartOffset(&tStartOffset);

  nsCOMPtr<nsIDOMNode> tStartContainer;
  res = this->GetStartContainer(getter_AddRefs(tStartContainer));
  if(NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMText> startTextNode(do_QueryInterface(tStartContainer));
  if (startTextNode)
  {
    nsCOMPtr<nsIDOMNode> tSCParentNode;
    res = tStartContainer->GetParentNode(getter_AddRefs(tSCParentNode));
    if(NS_FAILED(res)) return res;
    
    PRBool isCollapsed;
    res = GetCollapsed(&isCollapsed);
    if(NS_FAILED(res)) return res;

    PRInt32 tEndOffset;
    GetEndOffset(&tEndOffset);

    nsCOMPtr<nsIDOMText> secondPart;
    res = startTextNode->SplitText(tStartOffset, getter_AddRefs(secondPart));
    if (NS_FAILED(res)) return res;
    
    // SplitText collapses the range; fix that (bug 253609)
    if (!isCollapsed)
    {
      res = SetEnd(secondPart, tEndOffset - tStartOffset);
      if(NS_FAILED(res)) return res;
    }
    
    nsCOMPtr<nsIDOMNode> tResultNode;
    return tSCParentNode->InsertBefore(aN, secondPart, getter_AddRefs(tResultNode));
  }  

  nsCOMPtr<nsIDOMNodeList>tChildList;
  res = tStartContainer->GetChildNodes(getter_AddRefs(tChildList));
  if(NS_FAILED(res)) return res;
  PRUint32 tChildListLength;
  res = tChildList->GetLength(&tChildListLength);
  if(NS_FAILED(res)) return res;

  // find the insertion point in the DOM and insert the Node
  nsCOMPtr<nsIDOMNode>tChildNode;
  res = tChildList->Item(tStartOffset, getter_AddRefs(tChildNode));
  if(NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> tResultNode;
  return tStartContainer->InsertBefore(aN, tChildNode, getter_AddRefs(tResultNode));
}

nsresult nsRange::SurroundContents(nsIDOMNode* aNewParent)
{
  VALIDATE_ACCESS(aNewParent);
  
  // Extract the contents within the range.

  nsCOMPtr<nsIDOMDocumentFragment> docFrag;

  nsresult res = ExtractContents(getter_AddRefs(docFrag));

  if (NS_FAILED(res)) return res;
  if (!docFrag) return NS_ERROR_FAILURE;

  // Spec says we need to remove all of aNewParent's
  // children prior to insertion.

  nsCOMPtr<nsIDOMNodeList> children;
  res = aNewParent->GetChildNodes(getter_AddRefs(children));

  if (NS_FAILED(res)) return res;
  if (!children) return NS_ERROR_FAILURE;

  PRUint32 numChildren = 0;
  res = children->GetLength(&numChildren);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> tmpNode;

  while (numChildren)
  {
    nsCOMPtr<nsIDOMNode> child;
    res = children->Item(--numChildren, getter_AddRefs(child));

    if (NS_FAILED(res)) return res;
    if (!child) return NS_ERROR_FAILURE;

    res = aNewParent->RemoveChild(child, getter_AddRefs(tmpNode));
    if (NS_FAILED(res)) return res;
  }

  // Insert aNewParent at the range's start point.

  res = InsertNode(aNewParent);
  if (NS_FAILED(res)) return res;

  // Append the content we extracted under aNewParent.

  res = aNewParent->AppendChild(docFrag, getter_AddRefs(tmpNode));
  if (NS_FAILED(res)) return res;

  // Select aNewParent, and its contents.

  return SelectNode(aNewParent);
}

nsresult nsRange::ToString(nsAString& aReturn)
{ 
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // clear the string
  aReturn.Truncate();
  
  // If we're unpositioned, return the empty string
  if (!mIsPositioned) {
    return NS_OK;
  }

#ifdef DEBUG_range
      printf("Range dump: -----------------------\n");
#endif /* DEBUG */
    
  // effeciency hack for simple case
  if (mStartParent == mEndParent)
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
  
  /* complex case: mStartParent != mEndParent, or mStartParent not a text node
     revisit - there are potential optimizations here and also tradeoffs.
  */

  nsCOMPtr<nsIContentIterator> iter;
  NS_NewContentIterator(getter_AddRefs(iter));
  nsresult rv = iter->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString tempString;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and grab the text from any text node
  while (!iter->IsDone())
  {
    nsIContent *cN = iter->GetCurrentNode();

#ifdef DEBUG_range
    // If debug, dump it:
    cN->List(stdout);
#endif /* DEBUG */
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(cN) );
    if (textNode) // if it's a text node, get the text
    {
      if (cN == mStartParent) // only include text past start offset
      {
        PRUint32 strLength;
        textNode->GetLength(&strLength);
        textNode->SubstringData(mStartOffset,strLength-mStartOffset,tempString);
        aReturn += tempString;
      }
      else if (cN == mEndParent)  // only include text before end offset
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
  }

#ifdef DEBUG_range
  printf("End Range dump: -----------------------\n");
#endif /* DEBUG */
  return NS_OK;
}



nsresult
nsRange::Detach()
{
  if(mIsDetached)
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  mIsDetached = PR_TRUE;

  DoSetRange(nsnull, 0, nsnull, 0, nsnull);
  
  return NS_OK;
}

// nsIDOMNSRange interface
NS_IMETHODIMP    
nsRange::CreateContextualFragment(const nsAString& aFragment,
                                  nsIDOMDocumentFragment** aReturn)
{
  nsCOMPtr<nsIDOMNode> start = do_QueryInterface(mStartParent);
  return
    mIsPositioned
    ? nsContentUtils::CreateContextualFragment(start, aFragment, aReturn)
    : NS_ERROR_FAILURE;
}
