/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SelectionState_h
#define mozilla_SelectionState_h

#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/OwningNonNull.h"
#include "nsCOMPtr.h"
#include "nsDirection.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsTArray.h"
#include "nscore.h"

class nsCycleCollectionTraversalCallback;
class nsRange;
namespace mozilla {
namespace dom {
class Element;
class Selection;
class Text;
}  // namespace dom

/**
 * A helper struct for saving/setting ranges.
 */
struct RangeItem final {
  RangeItem() : mStartOffset(0), mEndOffset(0) {}

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~RangeItem() = default;

 public:
  void StoreRange(const nsRange& aRange);
  void StoreRange(const EditorRawDOMPoint& aStartPoint,
                  const EditorRawDOMPoint& aEndPoint) {
    MOZ_ASSERT(aStartPoint.IsSet());
    MOZ_ASSERT(aEndPoint.IsSet());
    mStartContainer = aStartPoint.GetContainer();
    mStartOffset = aStartPoint.Offset();
    mEndContainer = aEndPoint.GetContainer();
    mEndOffset = aEndPoint.Offset();
  }
  void Clear() {
    mStartContainer = mEndContainer = nullptr;
    mStartOffset = mEndOffset = 0;
  }
  already_AddRefed<nsRange> GetRange() const;

  // Same as the API of dom::AbstractRange
  [[nodiscard]] nsINode* GetRoot() const;
  [[nodiscard]] bool Collapsed() const {
    return mStartContainer == mEndContainer && mStartOffset == mEndOffset;
  }
  [[nodiscard]] bool IsPositioned() const {
    return mStartContainer && mEndContainer;
  }
  [[nodiscard]] bool Equals(const RangeItem& aOther) const {
    return mStartContainer == aOther.mStartContainer &&
           mEndContainer == aOther.mEndContainer &&
           mStartOffset == aOther.mStartOffset &&
           mEndOffset == aOther.mEndOffset;
  }
  EditorDOMPoint StartPoint() const {
    return EditorDOMPoint(mStartContainer, mStartOffset);
  }
  EditorDOMPoint EndPoint() const {
    return EditorDOMPoint(mEndContainer, mEndOffset);
  }
  EditorRawDOMPoint StartRawPoint() const {
    return EditorRawDOMPoint(mStartContainer, mStartOffset);
  }
  EditorRawDOMPoint EndRawPoint() const {
    return EditorRawDOMPoint(mEndContainer, mEndOffset);
  }

  NS_INLINE_DECL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_NATIVE_REFCOUNTING(RangeItem)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(RangeItem)

  nsCOMPtr<nsINode> mStartContainer;
  nsCOMPtr<nsINode> mEndContainer;
  uint32_t mStartOffset;
  uint32_t mEndOffset;
};

/**
 * mozilla::SelectionState
 *
 * Class for recording selection info.  Stores selection as collection of
 * { {startnode, startoffset} , {endnode, endoffset} } tuples.  Can't store
 * ranges since dom gravity will possibly change the ranges.
 */

class SelectionState final {
 public:
  SelectionState() = default;
  explicit SelectionState(const AutoRangeArray& aRanges);

  /**
   * Same as the API as dom::Selection
   */
  [[nodiscard]] bool IsCollapsed() const {
    if (mArray.Length() != 1) {
      return false;
    }
    return mArray[0]->Collapsed();
  }

  void RemoveAllRanges() {
    mArray.Clear();
    mDirection = eDirNext;
  }

  [[nodiscard]] uint32_t RangeCount() const { return mArray.Length(); }

  /**
   * Saving all ranges of aSelection.
   */
  void SaveSelection(dom::Selection& aSelection);

  /**
   * Setting aSelection to have all ranges stored by this instance.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  RestoreSelection(dom::Selection& aSelection);

  /**
   * Setting aRanges to have all ranges stored by this instance.
   */
  void ApplyTo(AutoRangeArray& aRanges);

  /**
   * HasOnlyCollapsedRange() returns true only when there is a positioned range
   * which is collapsed.  I.e., the selection represents a caret point.
   */
  [[nodiscard]] bool HasOnlyCollapsedRange() const {
    if (mArray.Length() != 1) {
      return false;
    }
    if (!mArray[0]->IsPositioned() || !mArray[0]->Collapsed()) {
      return false;
    }
    return true;
  }

  /**
   * Equals() returns true only when there are same number of ranges and
   * all their containers and offsets are exactly same.  This won't check
   * the validity of each range with the current DOM tree.
   */
  [[nodiscard]] bool Equals(const SelectionState& aOther) const;

  /**
   * Returns common root node of all ranges' start and end containers.
   * Some of them have different root nodes, this returns nullptr.
   */
  [[nodiscard]] nsINode* GetCommonRootNode() const {
    nsINode* rootNode = nullptr;
    for (const RefPtr<RangeItem>& rangeItem : mArray) {
      nsINode* newRootNode = rangeItem->GetRoot();
      if (!newRootNode || (rootNode && rootNode != newRootNode)) {
        return nullptr;
      }
      rootNode = newRootNode;
    }
    return rootNode;
  }

 private:
  CopyableAutoTArray<RefPtr<RangeItem>, 1> mArray;
  nsDirection mDirection = eDirNext;

  friend class RangeUpdater;
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          SelectionState&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(SelectionState&);
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, SelectionState& aField,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mArray, aName, aFlags);
}

inline void ImplCycleCollectionUnlink(SelectionState& aField) {
  ImplCycleCollectionUnlink(aField.mArray);
}

class MOZ_STACK_CLASS RangeUpdater final {
 public:
  RangeUpdater();

  void RegisterRangeItem(RangeItem& aRangeItem);
  void DropRangeItem(RangeItem& aRangeItem);
  void RegisterSelectionState(SelectionState& aSelectionState);
  void DropSelectionState(SelectionState& aSelectionState);

  // editor selection gravity routines.  Note that we can't always depend on
  // DOM Range gravity to do what we want to the "real" selection.  For
  // instance, if you move a node, that corresponds to deleting it and
  // reinserting it. DOM Range gravity will promote the selection out of the
  // node on deletion, which is not what you want if you know you are
  // reinserting it.
  template <typename PT, typename CT>
  nsresult SelAdjCreateNode(const EditorDOMPointBase<PT, CT>& aPoint);
  template <typename PT, typename CT>
  nsresult SelAdjInsertNode(const EditorDOMPointBase<PT, CT>& aPoint);
  void SelAdjDeleteNode(nsINode& aNode);

  /**
   * SelAdjSplitNode() is called immediately after spliting aOriginalNode
   * and inserted aNewContent into the DOM tree.
   *
   * @param aOriginalContent    The node which was split.
   * @param aSplitOffset        The old offset in aOriginalContent at splitting
   *                            it.
   * @param aNewContent         The new content node which was inserted into
   *                            the DOM tree.
   * @param aSplitNodeDirection Whether aNewNode was inserted before or after
   *                            aOriginalContent.
   */
  nsresult SelAdjSplitNode(nsIContent& aOriginalContent, uint32_t aSplitOffset,
                           nsIContent& aNewContent,
                           SplitNodeDirection aSplitNodeDirection);

  /**
   * SelAdjJoinNodes() is called immediately after joining aRemovedContent and
   * the container of aStartOfRightContent.
   *
   * @param aStartOfRightContent    The container is joined content node which
   *                                now has all children or text data which were
   *                                in aRemovedContent.  And this points where
   *                                the joined position.
   * @param aRemovedContent         The removed content.
   * @param aOffsetOfRemovedContent The offset which aRemovedContent was in
   *                                its ex-parent.
   */
  nsresult SelAdjJoinNodes(const EditorRawDOMPoint& aStartOfRightContent,
                           const nsIContent& aRemovedContent,
                           uint32_t aOffsetOfRemovedContent,
                           JoinNodesDirection aJoinNodesDirection);
  void SelAdjInsertText(const dom::Text& aTextNode, uint32_t aOffset,
                        uint32_t aInsertedLength);
  void SelAdjDeleteText(const dom::Text& aTextNode, uint32_t aOffset,
                        uint32_t aDeletedLength);
  void SelAdjReplaceText(const dom::Text& aTextNode, uint32_t aOffset,
                         uint32_t aReplacedLength, uint32_t aInsertedLength);
  // the following gravity routines need will/did sandwiches, because the other
  // gravity routines will be called inside of these sandwiches, but should be
  // ignored.
  void WillReplaceContainer() {
    // XXX Isn't this possible with mutation event listener?
    NS_WARNING_ASSERTION(!mLocked, "Has already been locked");
    mLocked = true;
  }
  void DidReplaceContainer(const dom::Element& aRemovedElement,
                           dom::Element& aInsertedElement);
  void WillRemoveContainer() {
    // XXX Isn't this possible with mutation event listener?
    NS_WARNING_ASSERTION(!mLocked, "Has already been locked");
    mLocked = true;
  }
  void DidRemoveContainer(const dom::Element& aRemovedElement,
                          nsINode& aRemovedElementContainerNode,
                          uint32_t aOldOffsetOfRemovedElement,
                          uint32_t aOldChildCountOfRemovedElement);
  void WillInsertContainer() {
    // XXX Isn't this possible with mutation event listener?
    NS_WARNING_ASSERTION(!mLocked, "Has already been locked");
    mLocked = true;
  }
  void DidInsertContainer() {
    NS_WARNING_ASSERTION(mLocked, "Not locked");
    mLocked = false;
  }
  void DidMoveNode(const nsINode& aOldParent, uint32_t aOldOffset,
                   const nsINode& aNewParent, uint32_t aNewOffset);

 private:
  // TODO: A lot of loop in these methods check whether each item `nullptr` or
  //       not. We should make it not nullable later.
  nsTArray<RefPtr<RangeItem>> mArray;
  bool mLocked;
};

/**
 * Helper class for using SelectionState.  Stack based class for doing
 * preservation of dom points across editor actions.
 */

class MOZ_STACK_CLASS AutoTrackDOMPoint final {
 public:
  AutoTrackDOMPoint() = delete;
  AutoTrackDOMPoint(RangeUpdater& aRangeUpdater, nsCOMPtr<nsINode>* aNode,
                    uint32_t* aOffset)
      : mRangeUpdater(aRangeUpdater),
        mNode(aNode),
        mOffset(aOffset),
        mRangeItem(do_AddRef(new RangeItem())) {
    mRangeItem->mStartContainer = *mNode;
    mRangeItem->mEndContainer = *mNode;
    mRangeItem->mStartOffset = *mOffset;
    mRangeItem->mEndOffset = *mOffset;
    mRangeUpdater.RegisterRangeItem(mRangeItem);
  }

  AutoTrackDOMPoint(RangeUpdater& aRangeUpdater, EditorDOMPoint* aPoint)
      : mRangeUpdater(aRangeUpdater),
        mNode(nullptr),
        mOffset(nullptr),
        mPoint(Some(aPoint->IsSet() ? aPoint : nullptr)),
        mRangeItem(do_AddRef(new RangeItem())) {
    if (!aPoint->IsSet()) {
      return;  // Nothing should be tracked.
    }
    mRangeItem->mStartContainer = aPoint->GetContainer();
    mRangeItem->mEndContainer = aPoint->GetContainer();
    mRangeItem->mStartOffset = aPoint->Offset();
    mRangeItem->mEndOffset = aPoint->Offset();
    mRangeUpdater.RegisterRangeItem(mRangeItem);
  }

  ~AutoTrackDOMPoint() {
    if (mPoint.isSome()) {
      if (!mPoint.ref()) {
        return;  // We don't track anything.
      }
      mRangeUpdater.DropRangeItem(mRangeItem);
      // Setting `mPoint` with invalid DOM point causes hitting `NS_ASSERTION()`
      // and the number of times may be too many.  (E.g., 1533913.html hits
      // over 700 times!)  We should just put warning instead.
      if (NS_WARN_IF(!mRangeItem->mStartContainer)) {
        mPoint.ref()->Clear();
        return;
      }
      if (NS_WARN_IF(mRangeItem->mStartContainer->Length() <
                     mRangeItem->mStartOffset)) {
        mPoint.ref()->SetToEndOf(mRangeItem->mStartContainer);
        return;
      }
      mPoint.ref()->Set(mRangeItem->mStartContainer, mRangeItem->mStartOffset);
      return;
    }
    mRangeUpdater.DropRangeItem(mRangeItem);
    *mNode = mRangeItem->mStartContainer;
    *mOffset = mRangeItem->mStartOffset;
  }

 private:
  RangeUpdater& mRangeUpdater;
  // Allow tracking nsINode until nsNode is gone
  nsCOMPtr<nsINode>* mNode;
  uint32_t* mOffset;
  Maybe<EditorDOMPoint*> mPoint;
  OwningNonNull<RangeItem> mRangeItem;
};

class MOZ_STACK_CLASS AutoTrackDOMRange final {
 public:
  AutoTrackDOMRange() = delete;
  AutoTrackDOMRange(RangeUpdater& aRangeUpdater, EditorDOMPoint* aStartPoint,
                    EditorDOMPoint* aEndPoint)
      : mRangeRefPtr(nullptr), mRangeOwningNonNull(nullptr) {
    mStartPointTracker.emplace(aRangeUpdater, aStartPoint);
    mEndPointTracker.emplace(aRangeUpdater, aEndPoint);
  }
  AutoTrackDOMRange(RangeUpdater& aRangeUpdater, EditorDOMRange* aRange)
      : mRangeRefPtr(nullptr), mRangeOwningNonNull(nullptr) {
    mStartPointTracker.emplace(
        aRangeUpdater, const_cast<EditorDOMPoint*>(&aRange->StartRef()));
    mEndPointTracker.emplace(aRangeUpdater,
                             const_cast<EditorDOMPoint*>(&aRange->EndRef()));
  }
  AutoTrackDOMRange(RangeUpdater& aRangeUpdater, RefPtr<nsRange>* aRange)
      : mStartPoint((*aRange)->StartRef()),
        mEndPoint((*aRange)->EndRef()),
        mRangeRefPtr(aRange),
        mRangeOwningNonNull(nullptr) {
    mStartPointTracker.emplace(aRangeUpdater, &mStartPoint);
    mEndPointTracker.emplace(aRangeUpdater, &mEndPoint);
  }
  AutoTrackDOMRange(RangeUpdater& aRangeUpdater, OwningNonNull<nsRange>* aRange)
      : mStartPoint((*aRange)->StartRef()),
        mEndPoint((*aRange)->EndRef()),
        mRangeRefPtr(nullptr),
        mRangeOwningNonNull(aRange) {
    mStartPointTracker.emplace(aRangeUpdater, &mStartPoint);
    mEndPointTracker.emplace(aRangeUpdater, &mEndPoint);
  }
  ~AutoTrackDOMRange() {
    if (!mRangeRefPtr && !mRangeOwningNonNull) {
      // The destructor of the trackers will update automatically.
      return;
    }
    // Otherwise, destroy them now.
    mStartPointTracker.reset();
    mEndPointTracker.reset();
    if (mRangeRefPtr) {
      (*mRangeRefPtr)
          ->SetStartAndEnd(mStartPoint.ToRawRangeBoundary(),
                           mEndPoint.ToRawRangeBoundary());
      return;
    }
    if (mRangeOwningNonNull) {
      (*mRangeOwningNonNull)
          ->SetStartAndEnd(mStartPoint.ToRawRangeBoundary(),
                           mEndPoint.ToRawRangeBoundary());
      return;
    }
  }

 private:
  Maybe<AutoTrackDOMPoint> mStartPointTracker;
  Maybe<AutoTrackDOMPoint> mEndPointTracker;
  EditorDOMPoint mStartPoint;
  EditorDOMPoint mEndPoint;
  RefPtr<nsRange>* mRangeRefPtr;
  OwningNonNull<nsRange>* mRangeOwningNonNull;
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidReplaceContainer()
 */

class MOZ_STACK_CLASS AutoReplaceContainerSelNotify final {
 public:
  AutoReplaceContainerSelNotify() = delete;
  // FYI: Marked as `MOZ_CAN_RUN_SCRIPT` for avoiding to use strong pointers
  //      for the members.
  MOZ_CAN_RUN_SCRIPT
  AutoReplaceContainerSelNotify(RangeUpdater& aRangeUpdater,
                                dom::Element& aOriginalElement,
                                dom::Element& aNewElement)
      : mRangeUpdater(aRangeUpdater),
        mOriginalElement(aOriginalElement),
        mNewElement(aNewElement) {
    mRangeUpdater.WillReplaceContainer();
  }

  ~AutoReplaceContainerSelNotify() {
    mRangeUpdater.DidReplaceContainer(mOriginalElement, mNewElement);
  }

 private:
  RangeUpdater& mRangeUpdater;
  dom::Element& mOriginalElement;
  dom::Element& mNewElement;
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidRemoveContainer()
 */

class MOZ_STACK_CLASS AutoRemoveContainerSelNotify final {
 public:
  AutoRemoveContainerSelNotify() = delete;
  AutoRemoveContainerSelNotify(RangeUpdater& aRangeUpdater,
                               const EditorRawDOMPoint& aAtRemovingElement)
      : mRangeUpdater(aRangeUpdater),
        mRemovingElement(*aAtRemovingElement.GetChild()->AsElement()),
        mParentNode(*aAtRemovingElement.GetContainer()),
        mOffsetInParent(aAtRemovingElement.Offset()),
        mChildCountOfRemovingElement(mRemovingElement->GetChildCount()) {
    MOZ_ASSERT(aAtRemovingElement.IsSet());
    mRangeUpdater.WillRemoveContainer();
  }

  ~AutoRemoveContainerSelNotify() {
    mRangeUpdater.DidRemoveContainer(mRemovingElement, mParentNode,
                                     mOffsetInParent,
                                     mChildCountOfRemovingElement);
  }

 private:
  RangeUpdater& mRangeUpdater;
  OwningNonNull<dom::Element> mRemovingElement;
  OwningNonNull<nsINode> mParentNode;
  uint32_t mOffsetInParent;
  uint32_t mChildCountOfRemovingElement;
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * Will/DidInsertContainer()
 * XXX The lock state isn't useful if the edit action is triggered from
 *     a mutation event listener so that looks like that we can remove
 *     this class.
 */

class MOZ_STACK_CLASS AutoInsertContainerSelNotify final {
 private:
  RangeUpdater& mRangeUpdater;

 public:
  AutoInsertContainerSelNotify() = delete;
  explicit AutoInsertContainerSelNotify(RangeUpdater& aRangeUpdater)
      : mRangeUpdater(aRangeUpdater) {
    mRangeUpdater.WillInsertContainer();
  }

  ~AutoInsertContainerSelNotify() { mRangeUpdater.DidInsertContainer(); }
};

/**
 * Another helper class for SelectionState.  Stack based class for doing
 * DidMoveNode()
 */

class MOZ_STACK_CLASS AutoMoveNodeSelNotify final {
 public:
  AutoMoveNodeSelNotify() = delete;
  AutoMoveNodeSelNotify(RangeUpdater& aRangeUpdater,
                        const EditorRawDOMPoint& aOldPoint,
                        const EditorRawDOMPoint& aNewPoint)
      : mRangeUpdater(aRangeUpdater),
        mOldParent(*aOldPoint.GetContainer()),
        mNewParent(*aNewPoint.GetContainer()),
        mOldOffset(aOldPoint.Offset()),
        mNewOffset(aNewPoint.Offset()) {
    MOZ_ASSERT(aOldPoint.IsSet());
    MOZ_ASSERT(aNewPoint.IsSet());
  }

  ~AutoMoveNodeSelNotify() {
    mRangeUpdater.DidMoveNode(mOldParent, mOldOffset, mNewParent, mNewOffset);
  }

 private:
  RangeUpdater& mRangeUpdater;
  nsINode& mOldParent;
  nsINode& mNewParent;
  const uint32_t mOldOffset;
  const uint32_t mNewOffset;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_SelectionState_h
