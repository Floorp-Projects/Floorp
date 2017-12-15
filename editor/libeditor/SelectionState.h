/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SelectionState_h
#define mozilla_SelectionState_h

#include "mozilla/EditorDOMPoint.h"
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

  NS_INLINE_DECL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_NATIVE_REFCOUNTING(RangeItem)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(RangeItem)

  nsCOMPtr<nsINode> mStartContainer;
  int32_t mStartOffset;
  nsCOMPtr<nsINode> mEndContainer;
  int32_t mEndOffset;
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
  AutoTArray<RefPtr<RangeItem>, 1> mArray;

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
  nsresult SelAdjCreateNode(const EditorRawDOMPoint& aPoint);
  nsresult SelAdjInsertNode(const EditorRawDOMPoint& aPoint);
  void SelAdjDeleteNode(nsINode* aNode);
  nsresult SelAdjSplitNode(nsIContent& aRightNode, nsIContent* aNewLeftNode);
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

/**
 * Helper class for using SelectionState.  Stack based class for doing
 * preservation of dom points across editor actions.
 */

class MOZ_STACK_CLASS AutoTrackDOMPoint final
{
private:
  RangeUpdater& mRangeUpdater;
  // Allow tracking either nsIDOMNode or nsINode until nsIDOMNode is gone
  nsCOMPtr<nsINode>* mNode;
  nsCOMPtr<nsIDOMNode>* mDOMNode;
  int32_t* mOffset;
  EditorDOMPoint* mPoint;
  RefPtr<RangeItem> mRangeItem;

public:
  AutoTrackDOMPoint(RangeUpdater& aRangeUpdater,
                    nsCOMPtr<nsINode>* aNode, int32_t* aOffset)
    : mRangeUpdater(aRangeUpdater)
    , mNode(aNode)
    , mDOMNode(nullptr)
    , mOffset(aOffset)
    , mPoint(nullptr)
  {
    mRangeItem = new RangeItem();
    mRangeItem->mStartContainer = *mNode;
    mRangeItem->mEndContainer = *mNode;
    mRangeItem->mStartOffset = *mOffset;
    mRangeItem->mEndOffset = *mOffset;
    mRangeUpdater.RegisterRangeItem(mRangeItem);
  }

  AutoTrackDOMPoint(RangeUpdater& aRangeUpdater,
                    nsCOMPtr<nsIDOMNode>* aNode, int32_t* aOffset)
    : mRangeUpdater(aRangeUpdater)
    , mNode(nullptr)
    , mDOMNode(aNode)
    , mOffset(aOffset)
    , mPoint(nullptr)
  {
    mRangeItem = new RangeItem();
    mRangeItem->mStartContainer = do_QueryInterface(*mDOMNode);
    mRangeItem->mEndContainer = do_QueryInterface(*mDOMNode);
    mRangeItem->mStartOffset = *mOffset;
    mRangeItem->mEndOffset = *mOffset;
    mRangeUpdater.RegisterRangeItem(mRangeItem);
  }

  AutoTrackDOMPoint(RangeUpdater& aRangeUpdater,
                    EditorDOMPoint* aPoint)
    : mRangeUpdater(aRangeUpdater)
    , mNode(nullptr)
    , mDOMNode(nullptr)
    , mOffset(nullptr)
    , mPoint(aPoint)
  {
    mRangeItem = new RangeItem();
    mRangeItem->mStartContainer = mPoint->GetContainer();
    mRangeItem->mEndContainer = mPoint->GetContainer();
    mRangeItem->mStartOffset = mPoint->Offset();
    mRangeItem->mEndOffset = mPoint->Offset();
    mRangeUpdater.RegisterRangeItem(mRangeItem);
  }

  ~AutoTrackDOMPoint()
  {
    mRangeUpdater.DropRangeItem(mRangeItem);
    if (mPoint) {
      mPoint->Set(mRangeItem->mStartContainer, mRangeItem->mStartOffset);
      return;
    }
    if (mNode) {
      *mNode = mRangeItem->mStartContainer;
    } else {
      *mDOMNode = GetAsDOMNode(mRangeItem->mStartContainer);
    }
    *mOffset = mRangeItem->mStartOffset;
  }
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidReplaceContainer()
 */

class MOZ_STACK_CLASS AutoReplaceContainerSelNotify final
{
private:
  RangeUpdater& mRangeUpdater;
  dom::Element* mOriginalElement;
  dom::Element* mNewElement;

public:
  AutoReplaceContainerSelNotify(RangeUpdater& aRangeUpdater,
                                dom::Element* aOriginalElement,
                                dom::Element* aNewElement)
    : mRangeUpdater(aRangeUpdater)
    , mOriginalElement(aOriginalElement)
    , mNewElement(aNewElement)
  {
    mRangeUpdater.WillReplaceContainer();
  }

  ~AutoReplaceContainerSelNotify()
  {
    mRangeUpdater.DidReplaceContainer(mOriginalElement, mNewElement);
  }
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidRemoveContainer()
 */

class MOZ_STACK_CLASS AutoRemoveContainerSelNotify final
{
private:
  RangeUpdater& mRangeUpdater;
  nsIDOMNode* mNode;
  nsIDOMNode* mParent;
  int32_t mOffset;
  uint32_t mNodeOrigLen;

public:
  AutoRemoveContainerSelNotify(RangeUpdater& aRangeUpdater,
                               nsINode* aNode,
                               nsINode* aParent,
                               int32_t aOffset,
                               uint32_t aNodeOrigLen)
    : mRangeUpdater(aRangeUpdater)
    , mNode(aNode->AsDOMNode())
    , mParent(aParent->AsDOMNode())
    , mOffset(aOffset)
    , mNodeOrigLen(aNodeOrigLen)
  {
    mRangeUpdater.WillRemoveContainer();
  }

  ~AutoRemoveContainerSelNotify()
  {
    mRangeUpdater.DidRemoveContainer(mNode, mParent, mOffset, mNodeOrigLen);
  }
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidInsertContainer()
 */

class MOZ_STACK_CLASS AutoInsertContainerSelNotify final
{
private:
  RangeUpdater& mRangeUpdater;

public:
  explicit AutoInsertContainerSelNotify(RangeUpdater& aRangeUpdater)
    : mRangeUpdater(aRangeUpdater)
  {
    mRangeUpdater.WillInsertContainer();
  }

  ~AutoInsertContainerSelNotify()
  {
    mRangeUpdater.DidInsertContainer();
  }
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidMoveNode()
 */

class MOZ_STACK_CLASS AutoMoveNodeSelNotify final
{
private:
  RangeUpdater& mRangeUpdater;
  nsINode* mOldParent;
  nsINode* mNewParent;
  int32_t mOldOffset;
  int32_t mNewOffset;

public:
  AutoMoveNodeSelNotify(RangeUpdater& aRangeUpdater,
                        nsINode* aOldParent,
                        int32_t aOldOffset,
                        nsINode* aNewParent,
                        int32_t aNewOffset)
    : mRangeUpdater(aRangeUpdater)
    , mOldParent(aOldParent)
    , mNewParent(aNewParent)
    , mOldOffset(aOldOffset)
    , mNewOffset(aNewOffset)
  {
    MOZ_ASSERT(aOldParent);
    MOZ_ASSERT(aNewParent);
    mRangeUpdater.WillMoveNode();
  }

  ~AutoMoveNodeSelNotify()
  {
    mRangeUpdater.DidMoveNode(mOldParent, mOldOffset, mNewParent, mNewOffset);
  }
};

} // namespace mozilla

#endif // #ifndef mozilla_SelectionState_h
