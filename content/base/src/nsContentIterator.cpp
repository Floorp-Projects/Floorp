/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsIDOMNodeList.h"
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsINode.h"
#include "nsCycleCollectionParticipant.h"

// couple of utility static functs

///////////////////////////////////////////////////////////////////////////
// ContentHasChildren: returns true if the node has children
//
static inline bool
NodeHasChildren(nsINode *aNode)
{
  return aNode->GetChildCount() > 0;
}

///////////////////////////////////////////////////////////////////////////
// NodeToParentOffset: returns the node's parent and offset.
//

static nsINode*
NodeToParentOffset(nsINode *aNode, PRInt32 *aOffset)
{
  *aOffset  = 0;

  nsINode* parent = aNode->GetNodeParent();

  if (parent) {
    *aOffset = parent->IndexOf(aNode);
  }
  
  return parent;
}

///////////////////////////////////////////////////////////////////////////
// NodeIsInTraversalRange: returns true if content is visited during
// the traversal of the range in the specified mode.
//
static bool
NodeIsInTraversalRange(nsINode *aNode, bool aIsPreMode,
                       nsINode *aStartNode, PRInt32 aStartOffset,
                       nsINode *aEndNode, PRInt32 aEndOffset)
{
  if (!aStartNode || !aEndNode || !aNode)
    return false;

  // If a chardata node contains an end point of the traversal range,
  // it is always in the traversal range.
  if (aNode->IsNodeOfType(nsINode::eDATA_NODE) &&
      (aNode == aStartNode || aNode == aEndNode)) {
    return true;
  }

  nsINode* parent = aNode->GetNodeParent();
  if (!parent)
    return false;

  PRInt32 indx = parent->IndexOf(aNode);

  if (!aIsPreMode)
    ++indx;

  return (nsContentUtils::ComparePoints(aStartNode, aStartOffset,
                                        parent, indx) <= 0) &&
         (nsContentUtils::ComparePoints(aEndNode, aEndOffset,
                                        parent, indx) >= 0);
}



/*
 *  A simple iterator class for traversing the content in "close tag" order
 */
class nsContentIterator : public nsIContentIterator //, public nsIEnumerator
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsContentIterator)

  explicit nsContentIterator(bool aPre);
  virtual ~nsContentIterator();

  // nsIContentIterator interface methods ------------------------------

  virtual nsresult Init(nsINode* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void First();

  virtual void Last();
  
  virtual void Next();

  virtual void Prev();

  virtual nsINode *GetCurrentNode();

  virtual bool IsDone();

  virtual nsresult PositionAt(nsINode* aCurNode);

  // nsIEnumertor interface methods ------------------------------
  
  //NS_IMETHOD CurrentItem(nsISupports **aItem);

protected:

  nsINode* GetDeepFirstChild(nsINode *aRoot, nsTArray<PRInt32> *aIndexes);
  nsINode* GetDeepLastChild(nsINode *aRoot, nsTArray<PRInt32> *aIndexes);

  // Get the next sibling of aNode.  Note that this will generally return null
  // if aNode happens not to be a content node.  That's OK.
  nsINode* GetNextSibling(nsINode *aNode, nsTArray<PRInt32> *aIndexes);

  // Get the prev sibling of aNode.  Note that this will generally return null
  // if aNode happens not to be a content node.  That's OK.
  nsINode* GetPrevSibling(nsINode *aNode, nsTArray<PRInt32> *aIndexes);

  nsINode* NextNode(nsINode *aNode, nsTArray<PRInt32> *aIndexes);
  nsINode* PrevNode(nsINode *aNode, nsTArray<PRInt32> *aIndexes);

  // WARNING: This function is expensive
  nsresult RebuildIndexStack();

  void MakeEmpty();
  
  nsCOMPtr<nsINode> mCurNode;
  nsCOMPtr<nsINode> mFirst;
  nsCOMPtr<nsINode> mLast;
  nsCOMPtr<nsINode> mCommonParent;

  // used by nsContentIterator to cache indices
  nsAutoTArray<PRInt32, 8> mIndexes;

  // used by nsSubtreeIterator to cache indices.  Why put them in the base class?
  // Because otherwise I have to duplicate the routines GetNextSibling etc across both classes,
  // with slight variations for caching.  Or alternately, create a base class for the cache
  // itself and have all the cache manipulation go through a vptr.
  // I think this is the best space and speed combo, even though it's ugly.
  PRInt32 mCachedIndex;
  // another note about mCachedIndex: why should the subtree iterator use a trivial cached index
  // instead of the mre robust array of indicies (which is what the basic content iterator uses)?
  // The reason is that subtree iterators do not do much transitioning between parents and children.
  // They tend to stay at the same level.  In fact, you can prove (though I won't attempt it here)
  // that they change levels at most n+m times, where n is the height of the parent hierarchy from the 
  // range start to the common ancestor, and m is the the height of the parent hierarchy from the 
  // range end to the common ancestor.  If we used the index array, we would pay the price up front
  // for n, and then pay the cost for m on the fly later on.  With the simple cache, we only "pay
  // as we go".  Either way, we call IndexOf() once for each change of level in the hierarchy.
  // Since a trivial index is much simpler, we use it for the subtree iterator.
  
  bool mIsDone;
  bool mPre;
  
private:

  // no copy's or assigns  FIX ME
  nsContentIterator(const nsContentIterator&);
  nsContentIterator& operator=(const nsContentIterator&);

};


/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentIterator(false);
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}


nsresult NS_NewPreContentIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentIterator(true);
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}


/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsContentIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsContentIterator)

NS_INTERFACE_MAP_BEGIN(nsContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsIContentIterator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentIterator)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsContentIterator)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_4(nsContentIterator,
                           mCurNode,
                           mFirst,
                           mLast,
                           mCommonParent)

/******************************************************
 * constructor/destructor
 ******************************************************/

nsContentIterator::nsContentIterator(bool aPre) :
  // don't need to explicitly initialize |nsCOMPtr|s, they will automatically be NULL
  mCachedIndex(0), mIsDone(false), mPre(aPre)
{
}


nsContentIterator::~nsContentIterator()
{
}


/******************************************************
 * Init routines
 ******************************************************/


nsresult
nsContentIterator::Init(nsINode* aRoot)
{
  if (!aRoot) 
    return NS_ERROR_NULL_POINTER; 

  mIsDone = false;
  mIndexes.Clear();
  
  if (mPre)
  {
    mFirst = aRoot;
    mLast  = GetDeepLastChild(aRoot, nsnull);
  }
  else
  {
    mFirst = GetDeepFirstChild(aRoot, nsnull); 
    mLast  = aRoot;
  }

  mCommonParent = aRoot;
  mCurNode = mFirst;
  RebuildIndexStack();
  return NS_OK;
}

nsresult
nsContentIterator::Init(nsIDOMRange* aDOMRange)
{
  NS_ENSURE_ARG_POINTER(aDOMRange);
  nsRange* range = static_cast<nsRange*>(aDOMRange);

  mIsDone = false;

  // get common content parent
  mCommonParent = range->GetCommonAncestor();
  NS_ENSURE_TRUE(mCommonParent, NS_ERROR_FAILURE);

  // get the start node and offset
  PRInt32 startIndx = range->StartOffset();
  nsINode* startNode = range->GetStartParent();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  // get the end node and offset
  PRInt32 endIndx = range->EndOffset();
  nsINode* endNode = range->GetEndParent();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  bool startIsData = startNode->IsNodeOfType(nsINode::eDATA_NODE);

  // short circuit when start node == end node
  if (startNode == endNode)
  {
    // Check to see if we have a collapsed range, if so,
    // there is nothing to iterate over.
    //
    // XXX: CharacterDataNodes (text nodes) are currently an exception,
    //      since we always want to be able to iterate text nodes at
    //      the end points of a range.

    if (!startIsData && startIndx == endIndx)
    {
      MakeEmpty();
      return NS_OK;
    }

    if (startIsData)
    {
      // It's a textnode.
      NS_ASSERTION(startNode->IsNodeOfType(nsINode::eCONTENT),
                   "Data node that's not content?");

      mFirst   = static_cast<nsIContent*>(startNode);
      mLast    = mFirst;
      mCurNode = mFirst;

      RebuildIndexStack();
      return NS_OK;
    }
  }
  
  // Find first node in range.

  nsIContent *cChild = nsnull;

  if (!startIsData && NodeHasChildren(startNode))
    cChild = startNode->GetChildAt(startIndx);

  if (!cChild) // no children, must be a text node
  {
    // XXXbz no children might also just mean no children.  So I'm not
    // sure what that comment above is talking about.
    if (mPre)
    {
      // XXX: In the future, if start offset is after the last
      //      character in the cdata node, should we set mFirst to
      //      the next sibling?

      if (!startIsData)
      {
        mFirst = GetNextSibling(startNode, nsnull);

        // Does mFirst node really intersect the range?
        // The range could be 'degenerate', ie not collapsed 
        // but still contain no content.
  
        if (mFirst &&
            !NodeIsInTraversalRange(mFirst, mPre, startNode, startIndx,
                                    endNode, endIndx)) {
          mFirst = nsnull;
        }
      }
      else {
        NS_ASSERTION(startNode->IsNodeOfType(nsINode::eCONTENT),
                   "Data node that's not content?");

        mFirst = static_cast<nsIContent*>(startNode);
      }
    }
    else {
      // post-order
      if (startNode->IsNodeOfType(nsINode::eCONTENT)) {
        mFirst = static_cast<nsIContent*>(startNode);
      } else {
        // What else can we do?
        mFirst = nsnull;
      }
    }
  }
  else
  {
    if (mPre)
      mFirst = cChild;
    else // post-order
    {
      mFirst = GetDeepFirstChild(cChild, nsnull);

      // Does mFirst node really intersect the range?
      // The range could be 'degenerate', ie not collapsed 
      // but still contain no content.
  
      if (mFirst &&
          !NodeIsInTraversalRange(mFirst, mPre, startNode, startIndx,
                                  endNode, endIndx))
        mFirst = nsnull;
    }
  }


  // Find last node in range.

  bool endIsData = endNode->IsNodeOfType(nsINode::eDATA_NODE);

  if (endIsData || !NodeHasChildren(endNode) || endIndx == 0)
  {
    if (mPre) {
      if (endNode->IsNodeOfType(nsINode::eCONTENT)) {
        mLast = static_cast<nsIContent*>(endNode);
      } else {
        // Not much else to do here...
        mLast = nsnull;
      }
    }
    else // post-order
    {
      // XXX: In the future, if end offset is before the first
      //      character in the cdata node, should we set mLast to
      //      the prev sibling?

      if (!endIsData)
      {
        mLast = GetPrevSibling(endNode, nsnull);

        if (!NodeIsInTraversalRange(mLast, mPre, startNode, startIndx,
                                    endNode, endIndx))
          mLast = nsnull;
      }
      else {
        NS_ASSERTION(endNode->IsNodeOfType(nsINode::eCONTENT),
                     "Data node that's not content?");

        mLast = static_cast<nsIContent*>(endNode);
      }
    }
  }
  else
  {
    PRInt32 indx = endIndx;

    cChild = endNode->GetChildAt(--indx);

    if (!cChild)  // No child at offset!
    {
      NS_NOTREACHED("nsContentIterator::nsContentIterator");
      return NS_ERROR_FAILURE; 
    }

    if (mPre)
    {
      mLast  = GetDeepLastChild(cChild, nsnull);

      if (!NodeIsInTraversalRange(mLast, mPre, startNode, startIndx,
                                  endNode, endIndx)) {
        mLast = nsnull;
      }
    }
    else { // post-order 
      mLast = cChild;
    }
  }

  // If either first or last is null, they both
  // have to be null!

  if (!mFirst || !mLast)
  {
    mFirst = nsnull;
    mLast  = nsnull;
  }
  
  mCurNode = mFirst;
  mIsDone  = !mCurNode;

  if (!mCurNode)
    mIndexes.Clear();
  else
    RebuildIndexStack();

  return NS_OK;
}


/******************************************************
 * Helper routines
 ******************************************************/
// WARNING: This function is expensive
nsresult nsContentIterator::RebuildIndexStack()
{
  // Make sure we start at the right indexes on the stack!  Build array up
  // to common parent of start and end.  Perhaps it's too many entries, but
  // that's far better than too few.
  nsINode* parent;
  nsINode* current;

  mIndexes.Clear();
  current = mCurNode;
  if (!current) {
    return NS_OK;
  }

  while (current != mCommonParent)
  {
    parent = current->GetNodeParent();
    
    if (!parent)
      return NS_ERROR_FAILURE;
  
    mIndexes.InsertElementAt(0, parent->IndexOf(current));

    current = parent;
  }
  return NS_OK;
}

void
nsContentIterator::MakeEmpty()
{
  mCurNode      = nsnull;
  mFirst        = nsnull;
  mLast         = nsnull;
  mCommonParent = nsnull;
  mIsDone       = true;
  mIndexes.Clear();
}

nsINode*
nsContentIterator::GetDeepFirstChild(nsINode *aRoot,
                                     nsTArray<PRInt32> *aIndexes)
{
  if (!aRoot) {
    return nsnull;
  }

  nsINode *n = aRoot;
  nsINode *nChild = n->GetFirstChild();

  while (nChild)
  {
    if (aIndexes)
    {
      // Add this node to the stack of indexes
      aIndexes->AppendElement(0);
    }
    n = nChild;
    nChild = n->GetFirstChild();
  }

  return n;
}

nsINode*
nsContentIterator::GetDeepLastChild(nsINode *aRoot, nsTArray<PRInt32> *aIndexes)
{
  if (!aRoot) {
    return nsnull;
  }

  nsINode *deepLastChild = aRoot;

  nsINode *n = aRoot;
  PRInt32 numChildren = n->GetChildCount();

  while (numChildren)
  {
    nsINode *nChild = n->GetChildAt(--numChildren);

    if (aIndexes)
    {
      // Add this node to the stack of indexes
      aIndexes->AppendElement(numChildren);
    }
    numChildren = nChild->GetChildCount();
    n = nChild;

    deepLastChild = n;
  }

  return deepLastChild;
}

// Get the next sibling, or parents next sibling, or grandpa's next sibling...
nsINode *
nsContentIterator::GetNextSibling(nsINode *aNode, 
                                  nsTArray<PRInt32> *aIndexes)
{
  if (!aNode) 
    return nsnull;

  nsINode *parent = aNode->GetNodeParent();
  if (!parent)
    return nsnull;

  PRInt32 indx = 0;

  NS_ASSERTION(!aIndexes || !aIndexes->IsEmpty(),
               "ContentIterator stack underflow");
  if (aIndexes && !aIndexes->IsEmpty())
  {
    // use the last entry on the Indexes array for the current index
    indx = (*aIndexes)[aIndexes->Length()-1];
  }
  else
    indx = mCachedIndex;

  // reverify that the index of the current node hasn't changed.
  // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
  // ignore result this time - the index may now be out of range.
  nsINode *sib = parent->GetChildAt(indx);
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    indx = parent->IndexOf(aNode);
  }

  // indx is now canonically correct
  if ((sib = parent->GetChildAt(++indx)))
  {
    // update index cache
    if (aIndexes && !aIndexes->IsEmpty())
    {
      aIndexes->ElementAt(aIndexes->Length()-1) = indx;
    }
    else mCachedIndex = indx;
  }
  else
  {
    if (parent != mCommonParent)
    {
      if (aIndexes)
      {
        // pop node off the stack, go up one level and return parent or fail.
        // Don't leave the index empty, especially if we're
        // returning NULL.  This confuses other parts of the code.
        if (aIndexes->Length() > 1)
          aIndexes->RemoveElementAt(aIndexes->Length()-1);
      }
    }

    // ok to leave cache out of date here if parent == mCommonParent?
    sib = GetNextSibling(parent, aIndexes);
  }
  
  return sib;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsINode*
nsContentIterator::GetPrevSibling(nsINode *aNode, 
                                  nsTArray<PRInt32> *aIndexes)
{
  if (!aNode)
    return nsnull;

  nsINode *parent = aNode->GetNodeParent();
  if (!parent)
    return nsnull;

  PRInt32 indx = 0;

  NS_ASSERTION(!aIndexes || !aIndexes->IsEmpty(),
               "ContentIterator stack underflow");
  if (aIndexes && !aIndexes->IsEmpty())
  {
    // use the last entry on the Indexes array for the current index
    indx = (*aIndexes)[aIndexes->Length()-1];
  }
  else
    indx = mCachedIndex;

  // reverify that the index of the current node hasn't changed
  // ignore result this time - the index may now be out of range.
  nsINode *sib = parent->GetChildAt(indx);
  if (sib != aNode)
  {
    // someone changed our index - find the new index the painful way
    indx = parent->IndexOf(aNode);
  }

  // indx is now canonically correct
  if (indx > 0 && (sib = parent->GetChildAt(--indx)))
  {
    // update index cache
    if (aIndexes && !aIndexes->IsEmpty())
    {
      aIndexes->ElementAt(aIndexes->Length()-1) = indx;
    }
    else mCachedIndex = indx;
  }
  else if (parent != mCommonParent)
  {
    if (aIndexes && !aIndexes->IsEmpty())
    {
      // pop node off the stack, go up one level and try again.
      aIndexes->RemoveElementAt(aIndexes->Length()-1);
    }
    return GetPrevSibling(parent, aIndexes);
  }

  return sib;
}

nsINode*
nsContentIterator::NextNode(nsINode *aNode, nsTArray<PRInt32> *aIndexes)
{
  nsINode *n = aNode;
  nsINode *nextNode = nsnull;

  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    // if it has children then next node is first child
    if (NodeHasChildren(n))
    {
      nsINode *nFirstChild = n->GetFirstChild();

      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(0);
      }
      else mCachedIndex = 0;
      
      return nFirstChild;
    }

    // else next sibling is next
    nextNode = GetNextSibling(n, aIndexes);
  }
  else  // post-order
  {
    nsINode *parent = n->GetNodeParent();
    nsINode *nSibling = nsnull;
    PRInt32 indx = 0;

    // get the cached index
    NS_ASSERTION(!aIndexes || !aIndexes->IsEmpty(),
                 "ContentIterator stack underflow");
    if (aIndexes && !aIndexes->IsEmpty())
    {
      // use the last entry on the Indexes array for the current index
      indx = (*aIndexes)[aIndexes->Length()-1];
    }
    else indx = mCachedIndex;

    // reverify that the index of the current node hasn't changed.
    // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
    // ignore result this time - the index may now be out of range.
    if (indx >= 0)
      nSibling = parent->GetChildAt(indx);
    if (nSibling != n)
    {
      // someone changed our index - find the new index the painful way
      indx = parent->IndexOf(n);
    }

    // indx is now canonically correct
    nSibling = parent->GetChildAt(++indx);
    if (nSibling)
    {
      // update cache
      if (aIndexes && !aIndexes->IsEmpty())
      {
        // replace an entry on the index stack
        aIndexes->ElementAt(aIndexes->Length()-1) = indx;
      }
      else mCachedIndex = indx;
      
      // next node is siblings "deep left" child
      return GetDeepFirstChild(nSibling, aIndexes); 
    }
  
    // else it's the parent
    // update cache
    if (aIndexes)
    {
      // pop an entry off the index stack
      // Don't leave the index empty, especially if we're
      // returning NULL.  This confuses other parts of the code.
      if (aIndexes->Length() > 1)
        aIndexes->RemoveElementAt(aIndexes->Length()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    nextNode = parent;
  }

  return nextNode;
}

nsINode*
nsContentIterator::PrevNode(nsINode *aNode, nsTArray<PRInt32> *aIndexes)
{
  nsINode *prevNode = nsnull;
  nsINode *n = aNode;
   
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsINode *parent = n->GetNodeParent();
    nsINode *nSibling = nsnull;
    PRInt32 indx = 0;

    // get the cached index
    NS_ASSERTION(!aIndexes || !aIndexes->IsEmpty(),
                 "ContentIterator stack underflow");
    if (aIndexes && !aIndexes->IsEmpty())
    {
      // use the last entry on the Indexes array for the current index
      indx = (*aIndexes)[aIndexes->Length()-1];
    }
    else indx = mCachedIndex;

    // reverify that the index of the current node hasn't changed.
    // not super cheap, but a lot cheaper than IndexOf(), and still O(1).
    // ignore result this time - the index may now be out of range.
    if (indx >= 0)
      nSibling = parent->GetChildAt(indx);

    if (nSibling != n)
    {
      // someone changed our index - find the new index the painful way
      indx = parent->IndexOf(n);
    }

    // indx is now canonically correct
    if (indx && (nSibling = parent->GetChildAt(--indx)))
    {
      // update cache
      if (aIndexes && !aIndexes->IsEmpty())
      {
        // replace an entry on the index stack
        aIndexes->ElementAt(aIndexes->Length()-1) = indx;
      }
      else mCachedIndex = indx;
      
      // prev node is siblings "deep right" child
      return GetDeepLastChild(nSibling, aIndexes); 
    }
  
    // else it's the parent
    // update cache
    if (aIndexes && !aIndexes->IsEmpty())
    {
      // pop an entry off the index stack
      aIndexes->RemoveElementAt(aIndexes->Length()-1);
    }
    else mCachedIndex = 0;   // this might be wrong, but we are better off guessing
    prevNode = parent;
  }
  else  // post-order
  {
    PRInt32 numChildren = n->GetChildCount();
  
    // if it has children then prev node is last child
    if (numChildren)
    {
      nsINode *nLastChild = n->GetLastChild();
      numChildren--;

      // update cache
      if (aIndexes)
      {
        // push an entry on the index stack
        aIndexes->AppendElement(numChildren);
      }
      else mCachedIndex = numChildren;
      
      return nLastChild;
    }

    // else prev sibling is previous
    prevNode = GetPrevSibling(n, aIndexes);
  }

  return prevNode;
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

void
nsContentIterator::First()
{
  if (mFirst) {
#ifdef DEBUG
    nsresult rv =
#endif
    PositionAt(mFirst);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");
  }

  mIsDone = mFirst == nsnull;
}


void
nsContentIterator::Last()
{
  NS_ASSERTION(mLast, "No last node!");

  if (mLast) {
#ifdef DEBUG
    nsresult rv =
#endif
    PositionAt(mLast);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to position iterator!");
  }

  mIsDone = mLast == nsnull;
}


void
nsContentIterator::Next()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mLast) 
  {
    mIsDone = true;
    return;
  }

  mCurNode = NextNode(mCurNode, &mIndexes);
}


void
nsContentIterator::Prev()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mFirst) 
  {
    mIsDone = true;
    return;
  }

  mCurNode = PrevNode(mCurNode, &mIndexes);
}


bool
nsContentIterator::IsDone()
{
  return mIsDone;
}


// Keeping arrays of indexes for the stack of nodes makes PositionAt
// interesting...
nsresult
nsContentIterator::PositionAt(nsINode* aCurNode)
{
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;

  nsINode *newCurNode = aCurNode;
  nsINode *tempNode = mCurNode;

  mCurNode = aCurNode;
  // take an early out if this doesn't actually change the position
  if (mCurNode == tempNode)
  {
    mIsDone = false;  // paranoia
    return NS_OK;
  }

  // Check to see if the node falls within the traversal range.

  nsINode* firstNode = mFirst;
  nsINode* lastNode = mLast;
  PRInt32 firstOffset=0, lastOffset=0;

  if (firstNode && lastNode)
  {
    if (mPre)
    {
      firstNode = NodeToParentOffset(mFirst, &firstOffset);

      if (lastNode->GetChildCount())
        lastOffset = 0;
      else
      {
        lastNode = NodeToParentOffset(mLast, &lastOffset);
        ++lastOffset;
      }
    }
    else
    {
      PRUint32 numChildren = firstNode->GetChildCount();

      if (numChildren)
        firstOffset = numChildren;
      else
        firstNode = NodeToParentOffset(mFirst, &firstOffset);

      lastNode = NodeToParentOffset(mLast, &lastOffset);
      ++lastOffset;
    }
  }

  // The end positions are always in the range even if it has no parent.
  // We need to allow that or 'iter->Init(root)' would assert in Last()
  // or First() for example, bug 327694.
  if (mFirst != mCurNode && mLast != mCurNode &&
      (!firstNode || !lastNode ||
       !NodeIsInTraversalRange(mCurNode, mPre, firstNode, firstOffset,
                               lastNode, lastOffset)))
  {
    mIsDone = true;
    return NS_ERROR_FAILURE;
  }

  // We can be at ANY node in the sequence.
  // Need to regenerate the array of indexes back to the root or common parent!
  nsAutoTArray<nsINode*, 8>     oldParentStack;
  nsAutoTArray<PRInt32, 8>      newIndexes;

  // Get a list of the parents up to the root, then compare the new node
  // with entries in that array until we find a match (lowest common
  // ancestor).  If no match, use IndexOf, take the parent, and repeat.
  // This avoids using IndexOf() N times on possibly large arrays.  We
  // still end up doing it a fair bit.  It's better to use Clone() if
  // possible.

  // we know the depth we're down (though we may not have started at the
  // top).
  if (!oldParentStack.SetCapacity(mIndexes.Length()+1))
    return NS_ERROR_FAILURE;

  // We want to loop mIndexes.Length() + 1 times here, because we want to make
  // sure we include mCommonParent in the oldParentStack, for use in the next
  // for loop, and mIndexes only has entries for nodes from tempNode up through
  // an ancestor of tempNode that's a child of mCommonParent.
  for (PRInt32 i = mIndexes.Length()+1; i > 0 && tempNode; i--)
  {
    // Insert at head since we're walking up
    oldParentStack.InsertElementAt(0, tempNode);

    nsINode *parent = tempNode->GetNodeParent();

    if (!parent)  // this node has no parent, and thus no index
      break;

    if (parent == mCurNode)
    {
      // The position was moved to a parent of the current position. 
      // All we need to do is drop some indexes.  Shortcut here.
      mIndexes.RemoveElementsAt(mIndexes.Length() - oldParentStack.Length(),
                                oldParentStack.Length());
      mIsDone = false;
      return NS_OK;
    }
    tempNode = parent;
  }

  // Ok.  We have the array of old parents.  Look for a match.
  while (newCurNode)
  {
    nsINode *parent = newCurNode->GetNodeParent();

    if (!parent)  // this node has no parent, and thus no index
      break;

    PRInt32 indx = parent->IndexOf(newCurNode);

    // insert at the head!
    newIndexes.InsertElementAt(0, indx);

    // look to see if the parent is in the stack
    indx = oldParentStack.IndexOf(parent);
    if (indx >= 0)
    {
      // ok, the parent IS on the old stack!  Rework things.
      // we want newIndexes to replace all nodes equal to or below the match
      // Note that index oldParentStack.Length()-1 is the last node, which is
      // one BELOW the last index in the mIndexes stack.  In other words, we
      // want to remove elements starting at index (indx+1).
      PRInt32 numToDrop = oldParentStack.Length()-(1+indx);
      if (numToDrop > 0)
        mIndexes.RemoveElementsAt(mIndexes.Length() - numToDrop, numToDrop);
      mIndexes.AppendElements(newIndexes);

      break;
    }
    newCurNode = parent;
  }

  // phew!

  mIsDone = false;
  return NS_OK;
}

nsINode*
nsContentIterator::GetCurrentNode()
{
  if (mIsDone) {
    return nsnull;
  }

  NS_ASSERTION(mCurNode, "Null current node in an iterator that's not done!");

  return mCurNode;
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
  nsContentSubtreeIterator() : nsContentIterator(false) {}
  virtual ~nsContentSubtreeIterator() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsContentSubtreeIterator, nsContentIterator)

  // nsContentIterator overrides ------------------------------

  virtual nsresult Init(nsINode* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void Next();

  virtual void Prev();

  virtual nsresult PositionAt(nsINode* aCurNode);

  // Must override these because we don't do PositionAt
  virtual void First();

  // Must override these because we don't do PositionAt
  virtual void Last();

protected:

  nsresult GetTopAncestorInRange(nsINode *aNode,
                                 nsCOMPtr<nsINode> *outAnestor);

  // no copy's or assigns  FIX ME
  nsContentSubtreeIterator(const nsContentSubtreeIterator&);
  nsContentSubtreeIterator& operator=(const nsContentSubtreeIterator&);

  nsRefPtr<nsRange> mRange;
  // these arrays all typically are used and have elements
#if 0
  nsAutoTArray<nsIContent*, 8> mStartNodes;
  nsAutoTArray<PRInt32, 8>     mStartOffsets;
#endif

  nsAutoTArray<nsIContent*, 8> mEndNodes;
  nsAutoTArray<PRInt32, 8>     mEndOffsets;
};

NS_IMPL_ADDREF_INHERITED(nsContentSubtreeIterator, nsContentIterator)
NS_IMPL_RELEASE_INHERITED(nsContentSubtreeIterator, nsContentIterator)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsContentSubtreeIterator)
NS_INTERFACE_MAP_END_INHERITING(nsContentIterator)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsContentSubtreeIterator)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsContentSubtreeIterator, nsContentIterator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRange, nsIDOMRange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsContentSubtreeIterator, nsContentIterator)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRange)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);




/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult)
{
  nsContentIterator * iter = new nsContentSubtreeIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}



/******************************************************
 * Init routines
 ******************************************************/


nsresult nsContentSubtreeIterator::Init(nsINode* aRoot)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsContentSubtreeIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  mIsDone = false;

  mRange = static_cast<nsRange*>(aRange);
  
  // get the start node and offset, convert to nsINode
  nsCOMPtr<nsIDOMNode> commonParent;
  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  nsCOMPtr<nsINode> nStartP;
  nsCOMPtr<nsINode> nEndP;
  nsCOMPtr<nsINode> n;
  nsINode *firstCandidate = nsnull;
  nsINode *lastCandidate = nsnull;
  PRInt32 indx, startIndx, endIndx;

  // get common content parent
  if (NS_FAILED(aRange->GetCommonAncestorContainer(getter_AddRefs(commonParent))) || !commonParent)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(commonParent);

  // get start content parent
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(startParent))) || !startParent)
    return NS_ERROR_FAILURE;
  nStartP = do_QueryInterface(startParent);
  aRange->GetStartOffset(&startIndx);

  // get end content parent
  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(endParent))) || !endParent)
    return NS_ERROR_FAILURE;
  nEndP = do_QueryInterface(endParent);
  aRange->GetEndOffset(&endIndx);

  // short circuit when start node == end node
  if (startParent == endParent)
  {
    nsINode* nChild = nStartP->GetFirstChild();
  
    if (!nChild) // no children, must be a text node or empty container
    {
      // all inside one text node - empty subtree iterator
      MakeEmpty();
      return NS_OK;
    }
    else
    {
      if (startIndx == endIndx)  // collapsed range
      {
        MakeEmpty();
        return NS_OK;
      }
    }
  }
  
  // cache ancestors
#if 0
  nsContentUtils::GetAncestorsAndOffsets(startParent, startIndx,
                                         &mStartNodes, &mStartOffsets);
#endif
  nsContentUtils::GetAncestorsAndOffsets(endParent, endIndx,
                                         &mEndNodes, &mEndOffsets);

  // find first node in range
  aRange->GetStartOffset(&indx);

  if (!nStartP->GetChildCount()) // no children, start at the node itself
  {
    n = nStartP;
  }
  else
  {
    nsINode* nChild = nStartP->GetChildAt(indx);
    if (!nChild)  // offset after last child
    {
      n = nStartP;
    }
    else
    {
      firstCandidate = nChild;
    }
  }
  
  if (!firstCandidate)
  {
    // then firstCandidate is next node after cN
    firstCandidate = GetNextSibling(n, nsnull);

    if (!firstCandidate)
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  firstCandidate = GetDeepFirstChild(firstCandidate, nsnull);
  
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  bool nodeBefore, nodeAfter;
  if (NS_FAILED(nsRange::CompareNodeToRange(firstCandidate, mRange,
                                            &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the first node in the range.  Now we walk
  // up its ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_FAILED(GetTopAncestorInRange(firstCandidate, address_of(mFirst))))
    return NS_ERROR_FAILURE;

  // now to find the last node
  aRange->GetEndOffset(&indx);
  PRInt32 numChildren = nEndP->GetChildCount();

  if (indx > numChildren) indx = numChildren;
  if (!indx)
  {
    n = nEndP;
  }
  else
  {
    if (!numChildren) // no children, must be a text node
    {
      n = nEndP;
    }
    else
    {
      lastCandidate = nEndP->GetChildAt(--indx);
      NS_ASSERTION(lastCandidate,
                   "tree traversal trouble in nsContentSubtreeIterator::Init");
    }
  }
  
  if (!lastCandidate)
  {
    // then lastCandidate is prev node before n
    lastCandidate = GetPrevSibling(n, nsnull);
  }
  
  lastCandidate = GetDeepLastChild(lastCandidate, nsnull);
  
  // confirm that this last possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.
  
  if (NS_FAILED(nsRange::CompareNodeToRange(lastCandidate, mRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the last node in the range.  Now we walk
  // up its ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_FAILED(GetTopAncestorInRange(lastCandidate, address_of(mLast))))
    return NS_ERROR_FAILURE;
  
  mCurNode = mFirst;

  return NS_OK;
}

/****************************************************************
 * nsContentSubtreeIterator overrides of ContentIterator routines
 ****************************************************************/

// we can't call PositionAt in a subtree iterator...
void
nsContentSubtreeIterator::First()
{
  mIsDone = mFirst == nsnull;

  mCurNode = mFirst;
}

// we can't call PositionAt in a subtree iterator...
void
nsContentSubtreeIterator::Last()
{
  mIsDone = mLast == nsnull;

  mCurNode = mLast;
}


void
nsContentSubtreeIterator::Next()
{
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mLast) 
  {
    mIsDone = true;
    return;
  }

  nsINode *nextNode = GetNextSibling(mCurNode, nsnull);
  NS_ASSERTION(nextNode, "No next sibling!?! This could mean deadlock!");

/*
  nextNode = GetDeepFirstChild(nextNode);
  return GetTopAncestorInRange(nextNode, address_of(mCurNode));
*/
  PRInt32 i = mEndNodes.IndexOf(nextNode);
  while (i != -1)
  {
    // as long as we are finding ancestors of the endpoint of the range,
    // dive down into their children
    nextNode = nextNode->GetFirstChild();
    NS_ASSERTION(nextNode, "Iterator error, expected a child node!");

    // should be impossible to get a null pointer.  If we went all the way
    // down the child chain to the bottom without finding an interior node, 
    // then the previous node should have been the last, which was
    // was tested at top of routine.
    i = mEndNodes.IndexOf(nextNode);
  }

  mCurNode = nextNode;

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mLast is in generated content, we need this
  // to stop the iterator when we've walked past past the last node!
  mIsDone = mCurNode == nsnull;

  return;
}


void
nsContentSubtreeIterator::Prev()
{
  // Prev should be optimized to use the mStartNodes, just as Next
  // uses mEndNodes.
  if (mIsDone || !mCurNode) 
    return;

  if (mCurNode == mFirst) 
  {
    mIsDone = true;
    return;
  }

  nsINode *prevNode = PrevNode(GetDeepFirstChild(mCurNode, nsnull), nsnull);

  prevNode = GetDeepLastChild(prevNode, nsnull);
  
  GetTopAncestorInRange(prevNode, address_of(mCurNode));

  // This shouldn't be needed, but since our selection code can put us
  // in a situation where mFirst is in generated content, we need this
  // to stop the iterator when we've walked past past the first node!
  mIsDone = mCurNode == nsnull;
}


nsresult
nsContentSubtreeIterator::PositionAt(nsINode* aCurNode)
{
  NS_ERROR("Not implemented!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

/****************************************************************
 * nsContentSubtreeIterator helper routines
 ****************************************************************/

nsresult
nsContentSubtreeIterator::GetTopAncestorInRange(nsINode *aNode,
                                                nsCOMPtr<nsINode> *outAncestor)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!outAncestor) 
    return NS_ERROR_NULL_POINTER;
  
  
  // sanity check: aNode is itself in the range
  bool nodeBefore, nodeAfter;
  if (NS_FAILED(nsRange::CompareNodeToRange(aNode, mRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsINode> parent, tmp;
  while (aNode)
  {
    parent = aNode->GetNodeParent();
    if (!parent)
    {
      if (tmp)
      {
        *outAncestor = tmp;
        return NS_OK;
      }
      else return NS_ERROR_FAILURE;
    }
    if (NS_FAILED(nsRange::CompareNodeToRange(parent, mRange, &nodeBefore,
                                              &nodeAfter)))
      return NS_ERROR_FAILURE;

    if (nodeBefore || nodeAfter)
    {
      *outAncestor = aNode;
      return NS_OK;
    }
    tmp = aNode;
    aNode = parent;
  }
  return NS_ERROR_FAILURE;
}

