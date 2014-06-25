/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __selectionstate_h__
#define __selectionstate_h__

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsINode.h"
#include "nsTArray.h"
#include "nscore.h"

class nsCycleCollectionTraversalCallback;
class nsIDOMCharacterData;
class nsIDOMRange;
class nsISelection;
class nsRange;
namespace mozilla {
namespace dom {
class Selection;
}
}

/***************************************************************************
 * class for recording selection info.  stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 */

// first a helper struct for saving/setting ranges
struct nsRangeStore MOZ_FINAL
{
  nsRangeStore();

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsRangeStore();

public:
  void StoreRange(nsRange* aRange);
  already_AddRefed<nsRange> GetRange();

  NS_INLINE_DECL_REFCOUNTING(nsRangeStore)
        
  nsCOMPtr<nsINode> startNode;
  int32_t           startOffset;
  nsCOMPtr<nsINode> endNode;
  int32_t           endOffset;
};

class nsSelectionState
{
  public:
      
    nsSelectionState();
    ~nsSelectionState();

    void DoTraverse(nsCycleCollectionTraversalCallback &cb);
    void DoUnlink() { MakeEmpty(); }
  
    void     SaveSelection(mozilla::dom::Selection *aSel);
    nsresult RestoreSelection(nsISelection *aSel);
    bool     IsCollapsed();
    bool     IsEqual(nsSelectionState *aSelState);
    void     MakeEmpty();
    bool     IsEmpty();
  protected:    
    nsTArray<nsRefPtr<nsRangeStore> > mArray;
    
    friend class nsRangeUpdater;
};

class nsRangeUpdater
{
  public:    
  
    nsRangeUpdater();
    ~nsRangeUpdater();
  
    void RegisterRangeItem(nsRangeStore *aRangeItem);
    void DropRangeItem(nsRangeStore *aRangeItem);
    nsresult RegisterSelectionState(nsSelectionState &aSelState);
    nsresult DropSelectionState(nsSelectionState &aSelState);
    
    // editor selection gravity routines.  Note that we can't always depend on
    // DOM Range gravity to do what we want to the "real" selection.  For instance,
    // if you move a node, that corresponds to deleting it and reinserting it.
    // DOM Range gravity will promote the selection out of the node on deletion,
    // which is not what you want if you know you are reinserting it.
    nsresult SelAdjCreateNode(nsINode* aParent, int32_t aPosition);
    nsresult SelAdjCreateNode(nsIDOMNode *aParent, int32_t aPosition);
    nsresult SelAdjInsertNode(nsINode* aParent, int32_t aPosition);
    nsresult SelAdjInsertNode(nsIDOMNode *aParent, int32_t aPosition);
    void     SelAdjDeleteNode(nsINode* aNode);
    void     SelAdjDeleteNode(nsIDOMNode *aNode);
    nsresult SelAdjSplitNode(nsINode* aOldRightNode, int32_t aOffset,
                             nsINode* aNewLeftNode);
    nsresult SelAdjSplitNode(nsIDOMNode *aOldRightNode, int32_t aOffset, nsIDOMNode *aNewLeftNode);
    nsresult SelAdjJoinNodes(nsINode* aLeftNode,
                             nsINode* aRightNode,
                             nsINode* aParent,
                             int32_t aOffset,
                             int32_t aOldLeftNodeLength);
    nsresult SelAdjJoinNodes(nsIDOMNode *aLeftNode, 
                             nsIDOMNode *aRightNode, 
                             nsIDOMNode *aParent, 
                             int32_t aOffset,
                             int32_t aOldLeftNodeLength);
    nsresult SelAdjInsertText(nsIContent* aTextNode, int32_t aOffset,
                              const nsAString &aString);
    nsresult SelAdjInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString);
    nsresult SelAdjDeleteText(nsIContent* aTextNode, int32_t aOffset,
                              int32_t aLength);
    nsresult SelAdjDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength);
    // the following gravity routines need will/did sandwiches, because the other gravity
    // routines will be called inside of these sandwiches, but should be ignored.
    nsresult WillReplaceContainer();
    nsresult DidReplaceContainer(nsINode* aOriginalNode, nsINode* aNewNode);
    nsresult DidReplaceContainer(nsIDOMNode *aOriginalNode, nsIDOMNode *aNewNode);
    nsresult WillRemoveContainer();
    nsresult DidRemoveContainer(nsINode* aNode, nsINode* aParent,
                                int32_t aOffset, uint32_t aNodeOrigLen);
    nsresult DidRemoveContainer(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aOffset, uint32_t aNodeOrigLen);
    nsresult WillInsertContainer();
    nsresult DidInsertContainer();
    void WillMoveNode();
    void DidMoveNode(nsINode* aOldParent, int32_t aOldOffset,
                     nsINode* aNewParent, int32_t aNewOffset);
  protected:    
    nsTArray<nsRefPtr<nsRangeStore> > mArray;
    bool mLock;
};


/***************************************************************************
 * helper class for using nsSelectionState.  stack based class for doing
 * preservation of dom points across editor actions
 */

class MOZ_STACK_CLASS nsAutoTrackDOMPoint
{
  private:
    nsRangeUpdater &mRU;
    // Allow tracking either nsIDOMNode or nsINode until nsIDOMNode is gone
    nsCOMPtr<nsINode>* mNode;
    nsCOMPtr<nsIDOMNode>* mDOMNode;
    int32_t* mOffset;
    nsRefPtr<nsRangeStore> mRangeItem;
  public:
    nsAutoTrackDOMPoint(nsRangeUpdater &aRangeUpdater,
                        nsCOMPtr<nsINode>* aNode, int32_t* aOffset)
      : mRU(aRangeUpdater)
      , mNode(aNode)
      , mDOMNode(nullptr)
      , mOffset(aOffset)
    {
      mRangeItem = new nsRangeStore();
      mRangeItem->startNode = *mNode;
      mRangeItem->endNode = *mNode;
      mRangeItem->startOffset = *mOffset;
      mRangeItem->endOffset = *mOffset;
      mRU.RegisterRangeItem(mRangeItem);
    }

    nsAutoTrackDOMPoint(nsRangeUpdater &aRangeUpdater,
                        nsCOMPtr<nsIDOMNode>* aNode, int32_t* aOffset)
      : mRU(aRangeUpdater)
      , mNode(nullptr)
      , mDOMNode(aNode)
      , mOffset(aOffset)
    {
      mRangeItem = new nsRangeStore();
      mRangeItem->startNode = do_QueryInterface(*mDOMNode);
      mRangeItem->endNode = do_QueryInterface(*mDOMNode);
      mRangeItem->startOffset = *mOffset;
      mRangeItem->endOffset = *mOffset;
      mRU.RegisterRangeItem(mRangeItem);
    }

    ~nsAutoTrackDOMPoint()
    {
      mRU.DropRangeItem(mRangeItem);
      if (mNode) {
        *mNode = mRangeItem->startNode;
      } else {
        *mDOMNode = GetAsDOMNode(mRangeItem->startNode);
      }
      *mOffset = mRangeItem->startOffset;
    }
};



/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidReplaceContainer()
 */

class MOZ_STACK_CLASS nsAutoReplaceContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsIDOMNode *mOriginalNode;
    nsIDOMNode *mNewNode;

  public:
    nsAutoReplaceContainerSelNotify(nsRangeUpdater &aRangeUpdater, nsIDOMNode *aOriginalNode, nsIDOMNode *aNewNode) :
    mRU(aRangeUpdater)
    ,mOriginalNode(aOriginalNode)
    ,mNewNode(aNewNode)
    {
      mRU.WillReplaceContainer();
    }
    
    ~nsAutoReplaceContainerSelNotify()
    {
      mRU.DidReplaceContainer(mOriginalNode, mNewNode);
    }
};


/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidRemoveContainer()
 */

class MOZ_STACK_CLASS nsAutoRemoveContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsIDOMNode *mNode;
    nsIDOMNode *mParent;
    int32_t    mOffset;
    uint32_t   mNodeOrigLen;

  public:
    nsAutoRemoveContainerSelNotify(nsRangeUpdater& aRangeUpdater,
                                   nsINode* aNode,
                                   nsINode* aParent,
                                   int32_t aOffset,
                                   uint32_t aNodeOrigLen)
      : mRU(aRangeUpdater)
      , mNode(aNode->AsDOMNode())
      , mParent(aParent->AsDOMNode())
      , mOffset(aOffset)
      , mNodeOrigLen(aNodeOrigLen)
    {
      mRU.WillRemoveContainer();
    }
    
    ~nsAutoRemoveContainerSelNotify()
    {
      mRU.DidRemoveContainer(mNode, mParent, mOffset, mNodeOrigLen);
    }
};

/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidInsertContainer()
 */

class MOZ_STACK_CLASS nsAutoInsertContainerSelNotify
{
  private:
    nsRangeUpdater &mRU;

  public:
    nsAutoInsertContainerSelNotify(nsRangeUpdater &aRangeUpdater) :
    mRU(aRangeUpdater)
    {
      mRU.WillInsertContainer();
    }
    
    ~nsAutoInsertContainerSelNotify()
    {
      mRU.DidInsertContainer();
    }
};


/***************************************************************************
 * another helper class for nsSelectionState.  stack based class for doing
 * Will/DidMoveNode()
 */

class MOZ_STACK_CLASS nsAutoMoveNodeSelNotify
{
  private:
    nsRangeUpdater &mRU;
    nsINode* mOldParent;
    nsINode* mNewParent;
    int32_t    mOldOffset;
    int32_t    mNewOffset;

  public:
    nsAutoMoveNodeSelNotify(nsRangeUpdater &aRangeUpdater, 
                            nsINode* aOldParent,
                            int32_t aOldOffset, 
                            nsINode* aNewParent,
                            int32_t aNewOffset)
      : mRU(aRangeUpdater)
      , mOldParent(aOldParent)
      , mNewParent(aNewParent)
      , mOldOffset(aOldOffset)
      , mNewOffset(aNewOffset)
    {
      MOZ_ASSERT(aOldParent);
      MOZ_ASSERT(aNewParent);
      mRU.WillMoveNode();
    }
    
    ~nsAutoMoveNodeSelNotify()
    {
      mRU.DidMoveNode(mOldParent, mOldOffset, mNewParent, mNewOffset);
    }
};

#endif


