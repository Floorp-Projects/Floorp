/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditHelpers_h
#define mozilla_HTMLEditHelpers_h

/**
 * This header declares/defines trivial helper classes which are used by
 * HTMLEditor.  If you want to create or look for static utility methods,
 * see HTMLEditUtils.h.
 */

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/StaticRange.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsRange.h"
#include "nsString.h"

class nsISimpleEnumerator;

namespace mozilla {
template <class T>
class OwningNonNull;

// JoinNodesDirection is also affected to which one is new node at splitting
// a node because a couple of undo/redo.
enum class JoinNodesDirection {
  LeftNodeIntoRightNode,
  RightNodeIntoLeftNode,
};
// SplitNodeDirection is also affected to which one is removed at joining a
// node because a couple of undo/redo.
enum class SplitNodeDirection {
  LeftNodeIsNewOne,
  RightNodeIsNewOne,
};

/*****************************************************************************
 * EditResult returns nsresult and preferred point where selection should be
 * collapsed or the range where selection should select.
 *
 * NOTE: If we stop modifying selection at every DOM tree change, perhaps,
 *       the following classes need to inherit this class.
 *****************************************************************************/
class MOZ_STACK_CLASS EditResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }
  const EditorDOMPoint& PointRefToCollapseSelection() const {
    MOZ_DIAGNOSTIC_ASSERT(mStartPoint.IsSet());
    MOZ_DIAGNOSTIC_ASSERT(mStartPoint == mEndPoint);
    return mStartPoint;
  }
  const EditorDOMPoint& StartPointRef() const { return mStartPoint; }
  const EditorDOMPoint& EndPointRef() const { return mEndPoint; }
  already_AddRefed<dom::StaticRange> CreateStaticRange() const {
    return dom::StaticRange::Create(mStartPoint.ToRawRangeBoundary(),
                                    mEndPoint.ToRawRangeBoundary(),
                                    IgnoreErrors());
  }
  already_AddRefed<nsRange> CreateRange() const {
    return nsRange::Create(mStartPoint.ToRawRangeBoundary(),
                           mEndPoint.ToRawRangeBoundary(), IgnoreErrors());
  }

  EditResult() = delete;
  explicit EditResult(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }
  template <typename PT, typename CT>
  explicit EditResult(const EditorDOMPointBase<PT, CT>& aPointToPutCaret)
      : mRv(aPointToPutCaret.IsSet() ? NS_OK : NS_ERROR_FAILURE),
        mStartPoint(aPointToPutCaret),
        mEndPoint(aPointToPutCaret) {}

  template <typename SPT, typename SCT, typename EPT, typename ECT>
  EditResult(const EditorDOMPointBase<SPT, SCT>& aStartPoint,
             const EditorDOMPointBase<EPT, ECT>& aEndPoint)
      : mRv(aStartPoint.IsSet() && aEndPoint.IsSet() ? NS_OK
                                                     : NS_ERROR_FAILURE),
        mStartPoint(aStartPoint),
        mEndPoint(aEndPoint) {}

  EditResult(const EditResult& aOther) = delete;
  EditResult& operator=(const EditResult& aOther) = delete;
  EditResult(EditResult&& aOther) = default;
  EditResult& operator=(EditResult&& aOther) = default;

 private:
  nsresult mRv;
  EditorDOMPoint mStartPoint;
  EditorDOMPoint mEndPoint;
};

/*****************************************************************************
 * MoveNodeResult is a simple class for MoveSomething() methods.
 * This holds error code and next insertion point if moving contents succeeded.
 *****************************************************************************/
class MOZ_STACK_CLASS MoveNodeResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  bool Handled() const { return mHandled; }
  bool Ignored() const { return !mHandled; }
  nsresult Rv() const { return mRv; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }
  const EditorDOMPoint& NextInsertionPointRef() const {
    return mNextInsertionPoint;
  }
  EditorDOMPoint NextInsertionPoint() const { return mNextInsertionPoint; }

  void MarkAsHandled() { mHandled = true; }

  MoveNodeResult() : mRv(NS_ERROR_NOT_INITIALIZED), mHandled(false) {}

  explicit MoveNodeResult(nsresult aRv) : mRv(aRv), mHandled(false) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  MoveNodeResult(const MoveNodeResult& aOther) = delete;
  MoveNodeResult& operator=(const MoveNodeResult& aOther) = delete;
  MoveNodeResult(MoveNodeResult&& aOther) = default;
  MoveNodeResult& operator=(MoveNodeResult&& aOther) = default;

  MoveNodeResult& operator|=(const MoveNodeResult& aOther) {
    mHandled |= aOther.mHandled;
    // When both result are same, keep the result but use newer point.
    if (mRv == aOther.mRv) {
      mNextInsertionPoint = aOther.mNextInsertionPoint;
      return *this;
    }
    // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
    // the most important error code for editor.
    if (EditorDestroyed() || aOther.EditorDestroyed()) {
      mRv = NS_ERROR_EDITOR_DESTROYED;
      mNextInsertionPoint.Clear();
      return *this;
    }
    // If the other one has not been set explicit nsresult, keep current
    // value.
    if (aOther.mRv == NS_ERROR_NOT_INITIALIZED) {
      return *this;
    }
    // If this one has not been set explicit nsresult, copy the other one's.
    if (mRv == NS_ERROR_NOT_INITIALIZED) {
      mRv = aOther.mRv;
      mNextInsertionPoint = aOther.mNextInsertionPoint;
      return *this;
    }
    // If one of the results is error, use NS_ERROR_FAILURE.
    if (Failed() || aOther.Failed()) {
      mRv = NS_ERROR_FAILURE;
      mNextInsertionPoint.Clear();
      return *this;
    }
    // Otherwise, use generic success code, NS_OK, and use newer point.
    mRv = NS_OK;
    mNextInsertionPoint = aOther.mNextInsertionPoint;
    return *this;
  }

 private:
  template <typename PT, typename CT>
  explicit MoveNodeResult(const EditorDOMPointBase<PT, CT>& aNextInsertionPoint,
                          bool aHandled)
      : mNextInsertionPoint(aNextInsertionPoint),
        mRv(aNextInsertionPoint.IsSet() ? NS_OK : NS_ERROR_FAILURE),
        mHandled(aHandled && aNextInsertionPoint.IsSet()) {
    if (mNextInsertionPoint.IsSet()) {
      AutoEditorDOMPointChildInvalidator computeOffsetAndForgetChild(
          mNextInsertionPoint);
    }
  }

  MoveNodeResult(nsINode* aParentNode, uint32_t aOffsetOfNextInsertionPoint,
                 bool aHandled) {
    if (!aParentNode) {
      mRv = NS_ERROR_FAILURE;
      mHandled = false;
      return;
    }
    aOffsetOfNextInsertionPoint =
        std::min(aOffsetOfNextInsertionPoint, aParentNode->Length());
    mNextInsertionPoint.Set(aParentNode, aOffsetOfNextInsertionPoint);
    mRv = mNextInsertionPoint.IsSet() ? NS_OK : NS_ERROR_FAILURE;
    mHandled = aHandled && mNextInsertionPoint.IsSet();
  }

  EditorDOMPoint mNextInsertionPoint;
  nsresult mRv;
  bool mHandled;

  friend MoveNodeResult MoveNodeIgnored(nsINode* aParentNode,
                                        uint32_t aOffsetOfNextInsertionPoint);
  friend MoveNodeResult MoveNodeHandled(nsINode* aParentNode,
                                        uint32_t aOffsetOfNextInsertionPoint);
  template <typename PT, typename CT>
  friend MoveNodeResult MoveNodeIgnored(
      const EditorDOMPointBase<PT, CT>& aNextInsertionPoint);
  template <typename PT, typename CT>
  friend MoveNodeResult MoveNodeHandled(
      const EditorDOMPointBase<PT, CT>& aNextInsertionPoint);
};

/*****************************************************************************
 * When a move node handler (or its helper) does nothing,
 * MoveNodeIgnored should be returned.
 *****************************************************************************/
inline MoveNodeResult MoveNodeIgnored(nsINode* aParentNode,
                                      uint32_t aOffsetOfNextInsertionPoint) {
  return MoveNodeResult(aParentNode, aOffsetOfNextInsertionPoint, false);
}

template <typename PT, typename CT>
inline MoveNodeResult MoveNodeIgnored(
    const EditorDOMPointBase<PT, CT>& aNextInsertionPoint) {
  return MoveNodeResult(aNextInsertionPoint, false);
}

/*****************************************************************************
 * When a move node handler (or its helper) handled and not canceled,
 * MoveNodeHandled should be returned.
 *****************************************************************************/
inline MoveNodeResult MoveNodeHandled(nsINode* aParentNode,
                                      uint32_t aOffsetOfNextInsertionPoint) {
  return MoveNodeResult(aParentNode, aOffsetOfNextInsertionPoint, true);
}

template <typename PT, typename CT>
inline MoveNodeResult MoveNodeHandled(
    const EditorDOMPointBase<PT, CT>& aNextInsertionPoint) {
  return MoveNodeResult(aNextInsertionPoint, true);
}

/*****************************************************************************
 * SplitNodeResult is a simple class for
 * HTMLEditor::SplitNodeDeepWithTransaction().
 * This makes the callers' code easier to read.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitNodeResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Handled() const { return mPreviousNode || mNextNode; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  /**
   * DidSplit() returns true if a node was actually split.
   */
  bool DidSplit() const { return mPreviousNode && mNextNode; }

  /**
   * GetPreviousContent() returns previous content node at the split point.
   */
  MOZ_KNOWN_LIVE nsIContent* GetPreviousContent() const {
    MOZ_ASSERT(Succeeded());
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                 : nullptr;
    }
    return mPreviousNode;
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtPreviousContent() const {
    if (nsIContent* previousContent = GetPreviousContent()) {
      return EditorDOMPointType(previousContent);
    }
    return EditorDOMPointType();
  }

  /**
   * GetNextContent() returns next content node at the split point.
   */
  MOZ_KNOWN_LIVE nsIContent* GetNextContent() const {
    MOZ_ASSERT(Succeeded());
    if (mGivenSplitPoint.IsSet()) {
      return !mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                  : nullptr;
    }
    return mNextNode;
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtNextContent() const {
    if (nsIContent* nextContent = GetNextContent()) {
      return EditorDOMPointType(nextContent);
    }
    return EditorDOMPointType();
  }

  /**
   * Returns new content node which is created at splitting a node.  I.e., this
   * returns nullptr if no node was split.
   */
  MOZ_KNOWN_LIVE nsIContent* GetNewContent() const {
    MOZ_ASSERT(Succeeded());
    if (!DidSplit()) {
      return nullptr;
    }
    return mDirection == SplitNodeDirection::LeftNodeIsNewOne ? mPreviousNode
                                                              : mNextNode;
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtNewContent() const {
    if (nsIContent* newContent = GetNewContent()) {
      return EditorDOMPointType(newContent);
    }
    return EditorDOMPointType();
  }

  /**
   * Returns original content node which is (or is just tried to be) split.
   */
  MOZ_KNOWN_LIVE nsIContent* GetOriginalContent() const {
    MOZ_ASSERT(Succeeded());
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.GetChild();
    }
    if (mDirection == SplitNodeDirection::LeftNodeIsNewOne) {
      return mNextNode ? mNextNode : mPreviousNode;
    }
    return mPreviousNode ? mPreviousNode : mNextNode;
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtOriginalContent() const {
    if (nsIContent* originalContent = GetOriginalContent()) {
      return EditorDOMPointType(originalContent);
    }
    return EditorDOMPointType();
  }

  /**
   * AtSplitPoint() returns the split point in the container.
   * HTMLEditor::CreateAndInsertElementWithTransaction() or something similar
   * methods.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType AtSplitPoint() const {
    if (Failed()) {
      return EditorDOMPointType();
    }
    if (mGivenSplitPoint.IsSet()) {
      return EditorDOMPointType(mGivenSplitPoint);
    }
    if (!mPreviousNode) {
      return EditorDOMPointType(mNextNode);
    }
    return EditorDOMPointType::After(mPreviousNode);
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as result when it succeeds.
   *
   * @param aPreviousNodeOfSplitPoint   Previous node immediately before
   *                                    split point.
   * @param aNextNodeOfSplitPoint       Next node immediately after split
   *                                    point.
   * @param aDirection                  The split direction which the HTML
   *                                    editor tried to split a node with.
   */
  SplitNodeResult(nsIContent* aPreviousNodeOfSplitPoint,
                  nsIContent* aNextNodeOfSplitPoint,
                  SplitNodeDirection aDirection)
      : mPreviousNode(aPreviousNodeOfSplitPoint),
        mNextNode(aNextNodeOfSplitPoint),
        mRv(NS_OK),
        mDirection(aDirection) {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }
  SplitNodeResult(nsCOMPtr<nsIContent>&& aPreviousNodeOfSplitPoint,
                  nsIContent* aNextNodeOfSplitPoint,
                  SplitNodeDirection aDirection)
      : mPreviousNode(std::move(aPreviousNodeOfSplitPoint)),
        mNextNode(aNextNodeOfSplitPoint),
        mRv(NS_OK),
        mDirection(aDirection) {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }
  SplitNodeResult(nsIContent* aPreviousNodeOfSplitPoint,
                  nsCOMPtr<nsIContent>&& aNextNodeOfSplitPoint,
                  SplitNodeDirection aDirection)
      : mPreviousNode(aPreviousNodeOfSplitPoint),
        mNextNode(std::move(aNextNodeOfSplitPoint)),
        mRv(NS_OK),
        mDirection(aDirection) {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }
  SplitNodeResult(nsCOMPtr<nsIContent>&& aPreviousNodeOfSplitPoint,
                  nsCOMPtr<nsIContent>&& aNextNodeOfSplitPoint,
                  SplitNodeDirection aDirection)
      : mPreviousNode(std::move(aPreviousNodeOfSplitPoint)),
        mNextNode(std::move(aNextNodeOfSplitPoint)),
        mRv(NS_OK),
        mDirection(aDirection) {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }

  /**
   * This constructor should be used when the method didn't split any nodes
   * but want to return given split point as right point.
   */
  explicit SplitNodeResult(const EditorRawDOMPoint& aGivenSplitPoint)
      : mGivenSplitPoint(aGivenSplitPoint),
        mRv(NS_OK),
        mDirection(SplitNodeDirection::LeftNodeIsNewOne) {
    MOZ_DIAGNOSTIC_ASSERT(mGivenSplitPoint.IsSet());
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as error result when it fails.
   */
  explicit SplitNodeResult(nsresult aRv)
      : mRv(aRv), mDirection(SplitNodeDirection::LeftNodeIsNewOne) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

 private:
  // When methods which return this class split some nodes actually, they
  // need to set a set of left node and right node to this class.  However,
  // one or both of them may be moved or removed by mutation observer.
  // In such case, we cannot represent the point with EditorDOMPoint since
  // it requires current container node.  Therefore, we need to use
  // nsCOMPtr<nsIContent> here instead.
  nsCOMPtr<nsIContent> mPreviousNode;
  nsCOMPtr<nsIContent> mNextNode;

  // Methods which return this class may not split any nodes actually.  Then,
  // they may want to return given split point as is since such behavior makes
  // their callers simpler.  In this case, the point may be in a text node
  // which cannot be represented as a node.  Therefore, we need EditorDOMPoint
  // for representing the point.
  EditorDOMPoint mGivenSplitPoint;

  nsresult mRv;
  SplitNodeDirection mDirection;

  SplitNodeResult() = delete;
};

/*****************************************************************************
 * JoinNodesResult is a simple class for HTMLEditor::JoinNodesWithTransaction().
 * This makes the callers' code easier to read.
 *****************************************************************************/
class MOZ_STACK_CLASS JoinNodesResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Handled() const { return Succeeded(); }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  MOZ_KNOWN_LIVE nsIContent* ExistingContent() const {
    MOZ_ASSERT(Succeeded());
    return mJoinedPoint.ContainerAsContent();
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtExistingContent() const {
    MOZ_ASSERT(Succeeded());
    return EditorDOMPointType(mJoinedPoint.ContainerAsContent());
  }

  MOZ_KNOWN_LIVE nsIContent* RemovedContent() const {
    MOZ_ASSERT(Succeeded());
    return mRemovedContent;
  }
  template <typename EditorDOMPointType>
  EditorDOMPointType AtRemovedContent() const {
    MOZ_ASSERT(Succeeded());
    if (mRemovedContent) {
      return EditorDOMPointType(mRemovedContent);
    }
    return EditorDOMPointType();
  }

  template <typename EditorDOMPointType>
  EditorDOMPointType AtJoinedPoint() const {
    MOZ_ASSERT(Succeeded());
    return mJoinedPoint;
  }

  JoinNodesResult() = delete;

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as result when it succeeds.
   *
   * @param aJoinedPoint        First child of right node or first character.
   * @param aRemovedContent     The node which was removed from the parent.
   * @param aDirection          The join direction which the HTML editor tried
   *                            to join the nodes with.
   */
  JoinNodesResult(const EditorDOMPoint& aJoinedPoint,
                  nsIContent& aRemovedContent, JoinNodesDirection aDirection)
      : mJoinedPoint(aJoinedPoint),
        mRemovedContent(&aRemovedContent),
        mRv(NS_OK) {
    MOZ_DIAGNOSTIC_ASSERT(aJoinedPoint.IsInContentNode());
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as error result when it fails.
   */
  explicit JoinNodesResult(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

 private:
  EditorDOMPoint mJoinedPoint;
  nsCOMPtr<nsIContent> mRemovedContent;

  nsresult mRv;
};

/*****************************************************************************
 * SplitRangeOffFromNodeResult class is a simple class for methods which split a
 * node at 2 points for making part of the node split off from the node.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitRangeOffFromNodeResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  /**
   * GetLeftContent() returns new created node before the part of quarried out.
   * This may return nullptr if the method didn't split at start edge of
   * the node.
   */
  nsIContent* GetLeftContent() const { return mLeftContent; }
  dom::Element* GetLeftContentAsElement() const {
    return dom::Element::FromNodeOrNull(mLeftContent);
  }

  /**
   * GetMiddleContent() returns new created node between left node and right
   * node.  I.e., this is quarried out from the node.  This may return nullptr
   * if the method unwrapped the middle node.
   */
  nsIContent* GetMiddleContent() const { return mMiddleContent; }
  dom::Element* GetMiddleContentAsElement() const {
    return dom::Element::FromNodeOrNull(mMiddleContent);
  }

  /**
   * GetRightContent() returns the right node after the part of quarried out.
   * This may return nullptr it the method didn't split at end edge of the
   * node.
   */
  nsIContent* GetRightContent() const { return mRightContent; }
  dom::Element* GetRightContentAsElement() const {
    return dom::Element::FromNodeOrNull(mRightContent);
  }

  SplitRangeOffFromNodeResult(nsIContent* aLeftContent,
                              nsIContent* aMiddleContent,
                              nsIContent* aRightContent)
      : mLeftContent(aLeftContent),
        mMiddleContent(aMiddleContent),
        mRightContent(aRightContent),
        mRv(NS_OK) {}

  SplitRangeOffFromNodeResult(SplitNodeResult& aSplitResultAtLeftOfMiddleNode,
                              SplitNodeResult& aSplitResultAtRightOfMiddleNode)
      : mRv(NS_OK) {
    if (aSplitResultAtLeftOfMiddleNode.Succeeded()) {
      mLeftContent = aSplitResultAtLeftOfMiddleNode.GetPreviousContent();
    }
    if (aSplitResultAtRightOfMiddleNode.Succeeded()) {
      mRightContent = aSplitResultAtRightOfMiddleNode.GetNextContent();
      mMiddleContent = aSplitResultAtRightOfMiddleNode.GetPreviousContent();
    }
    if (!mMiddleContent && aSplitResultAtLeftOfMiddleNode.Succeeded()) {
      mMiddleContent = aSplitResultAtLeftOfMiddleNode.GetNextContent();
    }
  }

  explicit SplitRangeOffFromNodeResult(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  SplitRangeOffFromNodeResult(const SplitRangeOffFromNodeResult& aOther) =
      delete;
  SplitRangeOffFromNodeResult& operator=(
      const SplitRangeOffFromNodeResult& aOther) = delete;
  SplitRangeOffFromNodeResult(SplitRangeOffFromNodeResult&& aOther) = default;
  SplitRangeOffFromNodeResult& operator=(SplitRangeOffFromNodeResult&& aOther) =
      default;

 private:
  nsCOMPtr<nsIContent> mLeftContent;
  nsCOMPtr<nsIContent> mMiddleContent;
  nsCOMPtr<nsIContent> mRightContent;

  nsresult mRv;

  SplitRangeOffFromNodeResult() = delete;
};

/*****************************************************************************
 * SplitRangeOffResult class is a simple class for methods which splits
 * specific ancestor elements at 2 DOM points.
 *****************************************************************************/
class MOZ_STACK_CLASS SplitRangeOffResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Handled() const { return mHandled; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  /**
   * This is at right node of split at start point.
   */
  const EditorDOMPoint& SplitPointAtStart() const { return mSplitPointAtStart; }
  /**
   * This is at right node of split at end point.  I.e., not in the range.
   * This is after the range.
   */
  const EditorDOMPoint& SplitPointAtEnd() const { return mSplitPointAtEnd; }

  SplitRangeOffResult() = delete;

  /**
   * Constructor for success case.
   *
   * @param aTrackedRangeStart          This should be at topmost right node
   *                                    child at start point if actually split
   *                                    there, or at start point to be tried
   *                                    to split.  Note that if the method
   *                                    allows to run script after splitting
   *                                    at start point, the point should be
   *                                    tracked with AutoTrackDOMPoint.
   * @param aSplitNodeResultAtStart     Raw split node result at start point.
   * @param aTrackedRangeEnd            This should be at topmost right node
   *                                    child at end point if actually split
   *                                    here, or at end point to be tried to
   *                                    split.  As same as aTrackedRangeStart,
   *                                    this value should be tracked while
   *                                    running some script.
   * @param aSplitNodeResultAtEnd       Raw split node result at start point.
   */
  SplitRangeOffResult(const EditorDOMPoint& aTrackedRangeStart,
                      const SplitNodeResult& aSplitNodeResultAtStart,
                      const EditorDOMPoint& aTrackedRangeEnd,
                      const SplitNodeResult& aSplitNodeResultAtEnd)
      : mSplitPointAtStart(aTrackedRangeStart),
        mSplitPointAtEnd(aTrackedRangeEnd),
        mRv(NS_OK),
        mHandled(aSplitNodeResultAtStart.Handled() ||
                 aSplitNodeResultAtEnd.Handled()) {
    MOZ_ASSERT(mSplitPointAtStart.IsSet());
    MOZ_ASSERT(mSplitPointAtEnd.IsSet());
    MOZ_ASSERT(aSplitNodeResultAtStart.Succeeded());
    MOZ_ASSERT(aSplitNodeResultAtEnd.Succeeded());
  }

  explicit SplitRangeOffResult(nsresult aRv) : mRv(aRv), mHandled(false) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  SplitRangeOffResult(const SplitRangeOffResult& aOther) = delete;
  SplitRangeOffResult& operator=(const SplitRangeOffResult& aOther) = delete;
  SplitRangeOffResult(SplitRangeOffResult&& aOther) = default;
  SplitRangeOffResult& operator=(SplitRangeOffResult&& aOther) = default;

 private:
  EditorDOMPoint mSplitPointAtStart;
  EditorDOMPoint mSplitPointAtEnd;

  // If you need to store previous and/or next node at start/end point,
  // you might be able to use `SplitNodeResult::GetPreviousNode()` etc in the
  // constructor only when `SplitNodeResult::Handled()` returns true.  But
  // the node might have gone with another DOM tree mutation.  So, be careful
  // if you do it.

  nsresult mRv;

  bool mHandled;
};

/******************************************************************************
 * DOM tree iterators
 *****************************************************************************/

class MOZ_RAII DOMIterator {
 public:
  explicit DOMIterator();
  explicit DOMIterator(nsINode& aNode);
  virtual ~DOMIterator() = default;

  nsresult Init(nsRange& aRange);
  nsresult Init(const RawRangeBoundary& aStartRef,
                const RawRangeBoundary& aEndRef);

  template <class NodeClass>
  void AppendAllNodesToArray(
      nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes) const;

  /**
   * AppendNodesToArray() calls aFunctor before appending found node to
   * aArrayOfNodes.  If aFunctor returns false, the node will be ignored.
   * You can use aClosure instead of capturing something with lambda.
   * Note that aNode is guaranteed that it's an instance of NodeClass
   * or its sub-class.
   * XXX If we can make type of aNode templated without std::function,
   *     it'd be better, though.
   */
  typedef bool (*BoolFunctor)(nsINode& aNode, void* aClosure);
  template <class NodeClass>
  void AppendNodesToArray(BoolFunctor aFunctor,
                          nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes,
                          void* aClosure = nullptr) const;

 protected:
  ContentIteratorBase* mIter;
  PostContentIterator mPostOrderIter;
};

class MOZ_RAII DOMSubtreeIterator final : public DOMIterator {
 public:
  explicit DOMSubtreeIterator();
  virtual ~DOMSubtreeIterator() = default;

  nsresult Init(nsRange& aRange);

 private:
  ContentSubtreeIterator mSubtreeIter;
  explicit DOMSubtreeIterator(nsINode& aNode) = delete;
};

/******************************************************************************
 * ReplaceRangeData
 *
 * This represents range to be replaced and replacing string.
 *****************************************************************************/

template <typename EditorDOMPointType>
class MOZ_STACK_CLASS ReplaceRangeDataBase final {
 public:
  ReplaceRangeDataBase() = default;
  template <typename OtherEditorDOMRangeType>
  ReplaceRangeDataBase(const OtherEditorDOMRangeType& aRange,
                       const nsAString& aReplaceString)
      : mRange(aRange), mReplaceString(aReplaceString) {}
  template <typename StartPointType, typename EndPointType>
  ReplaceRangeDataBase(const StartPointType& aStart, const EndPointType& aEnd,
                       const nsAString& aReplaceString)
      : mRange(aStart, aEnd), mReplaceString(aReplaceString) {}

  bool IsSet() const { return mRange.IsPositioned(); }
  bool IsSetAndValid() const { return mRange.IsPositionedAndValid(); }
  bool Collapsed() const { return mRange.Collapsed(); }
  bool HasReplaceString() const { return !mReplaceString.IsEmpty(); }
  const EditorDOMPointType& StartRef() const { return mRange.StartRef(); }
  const EditorDOMPointType& EndRef() const { return mRange.EndRef(); }
  const EditorDOMRangeBase<EditorDOMPointType>& RangeRef() const {
    return mRange;
  }
  const nsString& ReplaceStringRef() const { return mReplaceString; }

  template <typename PointType>
  MOZ_NEVER_INLINE_DEBUG void SetStart(const PointType& aStart) {
    mRange.SetStart(aStart);
  }
  template <typename PointType>
  MOZ_NEVER_INLINE_DEBUG void SetEnd(const PointType& aEnd) {
    mRange.SetEnd(aEnd);
  }
  template <typename StartPointType, typename EndPointType>
  MOZ_NEVER_INLINE_DEBUG void SetStartAndEnd(const StartPointType& aStart,
                                             const EndPointType& aEnd) {
    mRange.SetRange(aStart, aEnd);
  }
  template <typename OtherEditorDOMRangeType>
  MOZ_NEVER_INLINE_DEBUG void SetRange(const OtherEditorDOMRangeType& aRange) {
    mRange = aRange;
  }
  void SetReplaceString(const nsAString& aReplaceString) {
    mReplaceString = aReplaceString;
  }
  template <typename StartPointType, typename EndPointType>
  MOZ_NEVER_INLINE_DEBUG void SetStartAndEnd(const StartPointType& aStart,
                                             const EndPointType& aEnd,
                                             const nsAString& aReplaceString) {
    SetStartAndEnd(aStart, aEnd);
    SetReplaceString(aReplaceString);
  }
  template <typename OtherEditorDOMRangeType>
  MOZ_NEVER_INLINE_DEBUG void Set(const OtherEditorDOMRangeType& aRange,
                                  const nsAString& aReplaceString) {
    SetRange(aRange);
    SetReplaceString(aReplaceString);
  }

 private:
  EditorDOMRangeBase<EditorDOMPointType> mRange;
  // This string may be used with ReplaceTextTransaction.  Therefore, for
  // avoiding memory copy, we should store it with nsString rather than
  // nsAutoString.
  nsString mReplaceString;
};

using ReplaceRangeData = ReplaceRangeDataBase<EditorDOMPoint>;
using ReplaceRangeInTextsData = ReplaceRangeDataBase<EditorDOMPointInText>;

}  // namespace mozilla

#endif  // #ifndef mozilla_HTMLEditHelpers_h
