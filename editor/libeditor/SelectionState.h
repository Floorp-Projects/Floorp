/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SelectionState_h
#define SelectionState_h

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsINode.h"
#include "nsTArray.h"
#include "nscore.h"

class nsCycleCollectionTraversalCallback;
class nsIDOMCharacterData;
class nsRange;
namespace mozilla {
class RangeUpdater;
namespace dom {
class Selection;
class Text;
} // namespace dom

/**
 * A helper struct for saving/setting ranges.
 */
struct RangeItem final
{
  RangeItem();

private:
  // Private destructor, to discourage deletion outside of Release():
  ~RangeItem();

public:
  void StoreRange(nsRange* aRange);
  already_AddRefed<nsRange> GetRange();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(RangeItem)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(RangeItem)

  nsCOMPtr<nsINode> startNode;
  int32_t startOffset;
  nsCOMPtr<nsINode> endNode;
  int32_t endOffset;
};

/**
 * mozilla::SelectionState
 *
 * Class for recording selection info.  Stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 */

class SelectionState final
{
public:
  SelectionState();
  ~SelectionState();

  void SaveSelection(dom::Selection *aSel);
  nsresult RestoreSelection(dom::Selection* aSel);
  bool IsCollapsed();
  bool IsEqual(SelectionState *aSelState);
  void MakeEmpty();
  bool IsEmpty();
private:
  nsTArray<RefPtr<RangeItem>> mArray;

  friend class RangeUpdater;
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          SelectionState&,
                                          const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(SelectionState&);
};

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            SelectionState& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mArray, aName, aFlags);
}

inline void
ImplCycleCollectionUnlink(SelectionState& aField)
{
  ImplCycleCollectionUnlink(aField.mArray);
}

class RangeUpdater final
{
public:
  RangeUpdater();
  ~RangeUpdater();

  void RegisterRangeItem(RangeItem* aRangeItem);
  void DropRangeItem(RangeItem* aRangeItem);
  nsresult RegisterSelectionState(SelectionState& aSelState);
  nsresult DropSelectionState(SelectionState& aSelState);

  // editor selection gravity routines.  Note that we can't always depend on
  // DOM Range gravity to do what we want to the "real" selection.  For instance,
  // if you move a node, that corresponds to deleting it and reinserting it.
  // DOM Range gravity will promote the selection out of the node on deletion,
  // which is not what you want if you know you are reinserting it.
  nsresult SelAdjCreateNode(nsINode* aParent, int32_t aPosition);
  nsresult SelAdjCreateNode(nsIDOMNode* aParent, int32_t aPosition);
  nsresult SelAdjInsertNode(nsINode* aParent, int32_t aPosition);
  nsresult SelAdjInsertNode(nsIDOMNode* aParent, int32_t aPosition);
  void SelAdjDeleteNode(nsINode* aNode);
  void SelAdjDeleteNode(nsIDOMNode* aNode);
  nsresult SelAdjSplitNode(nsIContent& aOldRightNode, int32_t aOffset,
                           nsIContent* aNewLeftNode);
  nsresult SelAdjJoinNodes(nsINode& aLeftNode,
                           nsINode& aRightNode,
                           nsINode& aParent,
                           int32_t aOffset,
                           int32_t aOldLeftNodeLength);
  void SelAdjInsertText(dom::Text& aTextNode, int32_t aOffset,
                        const nsAString &aString);
  nsresult SelAdjDeleteText(nsIContent* aTextNode, int32_t aOffset,
                            int32_t aLength);
  nsresult SelAdjDeleteText(nsIDOMCharacterData* aTextNode,
                            int32_t aOffset, int32_t aLength);
  // the following gravity routines need will/did sandwiches, because the other
  // gravity routines will be called inside of these sandwiches, but should be
  // ignored.
  nsresult WillReplaceContainer();
  nsresult DidReplaceContainer(dom::Element* aOriginalNode,
                               dom::Element* aNewNode);
  nsresult WillRemoveContainer();
  nsresult DidRemoveContainer(nsINode* aNode, nsINode* aParent,
                              int32_t aOffset, uint32_t aNodeOrigLen);
  nsresult DidRemoveContainer(nsIDOMNode* aNode, nsIDOMNode* aParent,
                              int32_t aOffset, uint32_t aNodeOrigLen);
  nsresult WillInsertContainer();
  nsresult DidInsertContainer();
  void WillMoveNode();
  void DidMoveNode(nsINode* aOldParent, int32_t aOldOffset,
                   nsINode* aNewParent, int32_t aNewOffset);

private:
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          RangeUpdater&,
                                          const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(RangeUpdater& aField);

  nsTArray<RefPtr<RangeItem>> mArray;
  bool mLock;
};

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            RangeUpdater& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mArray, aName, aFlags);
}

inline void
ImplCycleCollectionUnlink(RangeUpdater& aField)
{
  ImplCycleCollectionUnlink(aField.mArray);
}

} // namespace mozilla

/***************************************************************************
 * helper class for using SelectionState.  stack based class for doing
 * preservation of dom points across editor actions
 */

class MOZ_STACK_CLASS nsAutoTrackDOMPoint
{
  private:
    mozilla::RangeUpdater& mRU;
    // Allow tracking either nsIDOMNode or nsINode until nsIDOMNode is gone
    nsCOMPtr<nsINode>* mNode;
    nsCOMPtr<nsIDOMNode>* mDOMNode;
    int32_t* mOffset;
    RefPtr<mozilla::RangeItem> mRangeItem;
  public:
    nsAutoTrackDOMPoint(mozilla::RangeUpdater& aRangeUpdater,
                        nsCOMPtr<nsINode>* aNode, int32_t* aOffset)
      : mRU(aRangeUpdater)
      , mNode(aNode)
      , mDOMNode(nullptr)
      , mOffset(aOffset)
    {
      mRangeItem = new mozilla::RangeItem();
      mRangeItem->startNode = *mNode;
      mRangeItem->endNode = *mNode;
      mRangeItem->startOffset = *mOffset;
      mRangeItem->endOffset = *mOffset;
      mRU.RegisterRangeItem(mRangeItem);
    }

    nsAutoTrackDOMPoint(mozilla::RangeUpdater& aRangeUpdater,
                        nsCOMPtr<nsIDOMNode>* aNode, int32_t* aOffset)
      : mRU(aRangeUpdater)
      , mNode(nullptr)
      , mDOMNode(aNode)
      , mOffset(aOffset)
    {
      mRangeItem = new mozilla::RangeItem();
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



/******************************************************************************
 * another helper class for SelectionState.  stack based class for doing
 * Will/DidReplaceContainer()
 */

namespace mozilla {
namespace dom {
class MOZ_STACK_CLASS AutoReplaceContainerSelNotify
{
  private:
    RangeUpdater& mRU;
    Element* mOriginalElement;
    Element* mNewElement;

  public:
    AutoReplaceContainerSelNotify(RangeUpdater& aRangeUpdater,
                                  Element* aOriginalElement,
                                  Element* aNewElement)
      : mRU(aRangeUpdater)
      , mOriginalElement(aOriginalElement)
      , mNewElement(aNewElement)
    {
      mRU.WillReplaceContainer();
    }

    ~AutoReplaceContainerSelNotify()
    {
      mRU.DidReplaceContainer(mOriginalElement, mNewElement);
    }
};

} // namespace dom
} // namespace mozilla


/***************************************************************************
 * another helper class for SelectionState.  stack based class for doing
 * Will/DidRemoveContainer()
 */

class MOZ_STACK_CLASS nsAutoRemoveContainerSelNotify
{
  private:
    mozilla::RangeUpdater& mRU;
    nsIDOMNode *mNode;
    nsIDOMNode *mParent;
    int32_t    mOffset;
    uint32_t   mNodeOrigLen;

  public:
    nsAutoRemoveContainerSelNotify(mozilla::RangeUpdater& aRangeUpdater,
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
 * another helper class for SelectionState.  stack based class for doing
 * Will/DidInsertContainer()
 */

class MOZ_STACK_CLASS nsAutoInsertContainerSelNotify
{
  private:
    mozilla::RangeUpdater& mRU;

  public:
    explicit nsAutoInsertContainerSelNotify(
               mozilla::RangeUpdater& aRangeUpdater) :
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
 * another helper class for SelectionState.  stack based class for doing
 * Will/DidMoveNode()
 */

class MOZ_STACK_CLASS nsAutoMoveNodeSelNotify
{
  private:
    mozilla::RangeUpdater& mRU;
    nsINode* mOldParent;
    nsINode* mNewParent;
    int32_t    mOldOffset;
    int32_t    mNewOffset;

  public:
    nsAutoMoveNodeSelNotify(mozilla::RangeUpdater& aRangeUpdater,
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

#endif // #ifndef SelectionState_h
