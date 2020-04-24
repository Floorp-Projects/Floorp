/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nscore.h"
#include "nsDebug.h"
#include "nsRange.h"
#include "nsString.h"

class nsISimpleEnumerator;
class nsITransferable;

namespace mozilla {
class MoveNodeResult;
template <class T>
class OwningNonNull;

namespace dom {
class Element;
class Text;
}  // namespace dom

/***************************************************************************
 * EditResult returns nsresult and preferred point where selection should be
 * collapsed or the range where selection should select.
 *
 * NOTE: If we stop modifying selection at every DOM tree change, perhaps,
 *       the following classes need to inherit this class.
 */
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

/***************************************************************************
 * EditActionResult is useful to return multiple results of an editor
 * action handler without out params.
 * Note that when you return an anonymous instance from a method, you should
 * use EditActionIgnored(), EditActionHandled() or EditActionCanceled() for
 * easier to read.  In other words, EditActionResult should be used when
 * declaring return type of a method, being an argument or defined as a local
 * variable.
 */
class MOZ_STACK_CLASS EditActionResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Canceled() const { return mCanceled; }
  bool Handled() const { return mHandled; }
  bool Ignored() const { return !mCanceled && !mHandled; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  EditActionResult SetResult(nsresult aRv) {
    mRv = aRv;
    return *this;
  }
  EditActionResult MarkAsCanceled() {
    mCanceled = true;
    return *this;
  }
  EditActionResult MarkAsHandled() {
    mHandled = true;
    return *this;
  }

  explicit EditActionResult(nsresult aRv)
      : mRv(aRv), mCanceled(false), mHandled(false) {}

  EditActionResult& operator|=(const EditActionResult& aOther) {
    mCanceled |= aOther.mCanceled;
    mHandled |= aOther.mHandled;
    // When both result are same, keep the result.
    if (mRv == aOther.mRv) {
      return *this;
    }
    // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
    // the most important error code for editor.
    if (EditorDestroyed() || aOther.EditorDestroyed()) {
      mRv = NS_ERROR_EDITOR_DESTROYED;
    }
    // If one of the results is error, use NS_ERROR_FAILURE.
    else if (Failed() || aOther.Failed()) {
      mRv = NS_ERROR_FAILURE;
    } else {
      // Otherwise, use generic success code, NS_OK.
      mRv = NS_OK;
    }
    return *this;
  }

  EditActionResult& operator|=(const MoveNodeResult& aMoveNodeResult);

 private:
  nsresult mRv;
  bool mCanceled;
  bool mHandled;

  EditActionResult(nsresult aRv, bool aCanceled, bool aHandled)
      : mRv(aRv), mCanceled(aCanceled), mHandled(aHandled) {}

  EditActionResult()
      : mRv(NS_ERROR_NOT_INITIALIZED), mCanceled(false), mHandled(false) {}

  friend EditActionResult EditActionIgnored(nsresult aRv);
  friend EditActionResult EditActionHandled(nsresult aRv);
  friend EditActionResult EditActionCanceled(nsresult aRv);
};

/***************************************************************************
 * When an edit action handler (or its helper) does nothing,
 * EditActionIgnored should be returned.
 */
inline EditActionResult EditActionIgnored(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, false);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and not canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionHandled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, true);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionCanceled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, true, true);
}

/***************************************************************************
 * CreateNodeResultBase is a simple class for CreateSomething() methods
 * which want to return new node.
 */
template <typename NodeType>
class CreateNodeResultBase;

typedef CreateNodeResultBase<dom::Element> CreateElementResult;

template <typename NodeType>
class MOZ_STACK_CLASS CreateNodeResultBase final {
  typedef CreateNodeResultBase<NodeType> SelfType;

 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }
  NodeType* GetNewNode() const { return mNode; }

  CreateNodeResultBase() = delete;

  explicit CreateNodeResultBase(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  explicit CreateNodeResultBase(NodeType* aNode)
      : mNode(aNode), mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}

  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode)
      : mNode(std::move(aNode)), mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}

  CreateNodeResultBase(const SelfType& aOther) = delete;
  SelfType& operator=(const SelfType& aOther) = delete;
  CreateNodeResultBase(SelfType&& aOther) = default;
  SelfType& operator=(SelfType&& aOther) = default;

  already_AddRefed<NodeType> forget() {
    mRv = NS_ERROR_NOT_INITIALIZED;
    return mNode.forget();
  }

 private:
  RefPtr<NodeType> mNode;
  nsresult mRv;
};

/***************************************************************************
 * MoveNodeResult is a simple class for MoveSomething() methods.
 * This holds error code and next insertion point if moving contents succeeded.
 */
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

/***************************************************************************
 * When a move node handler (or its helper) does nothing,
 * MoveNodeIgnored should be returned.
 */
inline MoveNodeResult MoveNodeIgnored(nsINode* aParentNode,
                                      uint32_t aOffsetOfNextInsertionPoint) {
  return MoveNodeResult(aParentNode, aOffsetOfNextInsertionPoint, false);
}

template <typename PT, typename CT>
inline MoveNodeResult MoveNodeIgnored(
    const EditorDOMPointBase<PT, CT>& aNextInsertionPoint) {
  return MoveNodeResult(aNextInsertionPoint, false);
}

/***************************************************************************
 * When a move node handler (or its helper) handled and not canceled,
 * MoveNodeHandled should be returned.
 */
inline MoveNodeResult MoveNodeHandled(nsINode* aParentNode,
                                      uint32_t aOffsetOfNextInsertionPoint) {
  return MoveNodeResult(aParentNode, aOffsetOfNextInsertionPoint, true);
}

template <typename PT, typename CT>
inline MoveNodeResult MoveNodeHandled(
    const EditorDOMPointBase<PT, CT>& aNextInsertionPoint) {
  return MoveNodeResult(aNextInsertionPoint, true);
}

/***************************************************************************
 * SplitNodeResult is a simple class for
 * EditorBase::SplitNodeDeepWithTransaction().
 * This makes the callers' code easier to read.
 */
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
   * GetLeftNode() simply returns the left node which was created at splitting.
   * This returns nullptr if the node wasn't split.
   */
  nsIContent* GetLeftNode() const {
    return mPreviousNode && mNextNode ? mPreviousNode.get() : nullptr;
  }

  /**
   * GetRightNode() simply returns the right node which was split.
   * This won't return nullptr unless failed to split due to invalid arguments.
   */
  nsIContent* GetRightNode() const {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.GetChild();
    }
    return mPreviousNode && !mNextNode ? mPreviousNode : mNextNode;
  }

  /**
   * GetPreviousNode() returns previous node at the split point.
   */
  nsIContent* GetPreviousNode() const {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                 : nullptr;
    }
    return mPreviousNode;
  }

  /**
   * GetNextNode() returns next node at the split point.
   */
  nsIContent* GetNextNode() const {
    if (mGivenSplitPoint.IsSet()) {
      return !mGivenSplitPoint.IsEndOfContainer() ? mGivenSplitPoint.GetChild()
                                                  : nullptr;
    }
    return mNextNode;
  }

  /**
   * SplitPoint() returns the split point in the container.
   * This is useful when callers insert an element at split point with
   * EditorBase::CreateNodeWithTransaction() or something similar methods.
   *
   * Note that the result is EditorRawDOMPoint but the nodes are grabbed
   * by this instance.  Therefore, the life time of both container node
   * and child node are guaranteed while using the result temporarily.
   */
  EditorDOMPoint SplitPoint() const {
    if (Failed()) {
      return EditorDOMPoint();
    }
    if (mGivenSplitPoint.IsSet()) {
      return EditorDOMPoint(mGivenSplitPoint);
    }
    if (!mPreviousNode) {
      return EditorDOMPoint(mNextNode);
    }
    EditorDOMPoint point(mPreviousNode);
    DebugOnly<bool> advanced = point.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to after previous node");
    return point;
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as result when it succeeds.
   *
   * @param aPreviousNodeOfSplitPoint   Previous node immediately before
   *                                    split point.
   * @param aNextNodeOfSplitPoint       Next node immediately after split
   *                                    point.
   */
  SplitNodeResult(nsIContent* aPreviousNodeOfSplitPoint,
                  nsIContent* aNextNodeOfSplitPoint)
      : mPreviousNode(aPreviousNodeOfSplitPoint),
        mNextNode(aNextNodeOfSplitPoint),
        mRv(NS_OK) {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }

  /**
   * This constructor should be used when the method didn't split any nodes
   * but want to return given split point as right point.
   */
  explicit SplitNodeResult(const EditorRawDOMPoint& aGivenSplitPoint)
      : mGivenSplitPoint(aGivenSplitPoint), mRv(NS_OK) {
    MOZ_DIAGNOSTIC_ASSERT(mGivenSplitPoint.IsSet());
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as error result when it fails.
   */
  explicit SplitNodeResult(nsresult aRv) : mRv(aRv) {
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

  SplitNodeResult() = delete;
};

/***************************************************************************
 * SplitRangeOffFromNodeResult class is a simple class for methods which split a
 * node at 2 points for making part of the node split off from the node.
 */
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
      mLeftContent = aSplitResultAtLeftOfMiddleNode.GetPreviousNode();
    }
    if (aSplitResultAtRightOfMiddleNode.Succeeded()) {
      mRightContent = aSplitResultAtRightOfMiddleNode.GetNextNode();
      mMiddleContent = aSplitResultAtRightOfMiddleNode.GetPreviousNode();
    }
    if (!mMiddleContent && aSplitResultAtLeftOfMiddleNode.Succeeded()) {
      mMiddleContent = aSplitResultAtLeftOfMiddleNode.GetNextNode();
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

/***************************************************************************
 * SplitRangeOffResult class is a simple class for methods which splits
 * specific ancestor elements at 2 DOM points.
 */
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

/***************************************************************************
 * stack based helper class for calling EditorBase::EndTransaction() after
 * EditorBase::BeginTransaction().  This shouldn't be used in editor classes
 * or helper classes while an edit action is being handled.  Use
 * AutoTransactionBatch in such cases since it uses non-virtual internal
 * methods.
 ***************************************************************************/
class MOZ_RAII AutoTransactionBatchExternal final {
 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoTransactionBatchExternal(
      EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mEditorBase(aEditorBase) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_KnownLive(mEditorBase).BeginTransaction();
  }

  MOZ_CAN_RUN_SCRIPT ~AutoTransactionBatchExternal() {
    MOZ_KnownLive(mEditorBase).EndTransaction();
  }

 private:
  EditorBase& mEditorBase;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_STACK_CLASS AutoRangeArray final {
 public:
  explicit AutoRangeArray(dom::Selection* aSelection) {
    if (!aSelection) {
      return;
    }
    uint32_t rangeCount = aSelection->RangeCount();
    for (uint32_t i = 0; i < rangeCount; i++) {
      mRanges.AppendElement(*aSelection->GetRangeAt(i));
    }
  }

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
};

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

class MOZ_RAII DOMIterator {
 public:
  explicit DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  explicit DOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
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
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII DOMSubtreeIterator final : public DOMIterator {
 public:
  explicit DOMSubtreeIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  virtual ~DOMSubtreeIterator() = default;

  nsresult Init(nsRange& aRange);

 private:
  ContentSubtreeIterator mSubtreeIter;
  explicit DOMSubtreeIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM) =
      delete;
};

class EditorUtils final {
 public:
  using EditorType = EditorBase::EditorType;
  using Selection = dom::Selection;

  /**
   * IsDescendantOf() checks if aNode is a child or a descendant of aParent.
   * aOutPoint is set to the child of aParent.
   *
   * @return            true if aNode is a child or a descendant of aParent.
   */
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorRawDOMPoint* aOutPoint = nullptr);
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorDOMPoint* aOutPoint);

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty editor.
   */
  static bool IsPaddingBRElementForEmptyEditor(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyEditor();
  }

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty last line.
   */
  static bool IsPaddingBRElementForEmptyLastLine(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyLastLine();
  }

  /**
   * IsEditableContent() returns true if aContent's data or children is ediable
   * for the given editor type.  Be aware, returning true does NOT mean the
   * node can be removed from its parent node, and returning false does NOT
   * mean the node cannot be removed from the parent node.
   * XXX May be the anonymous nodes in TextEditor not editable?  If it's not
   *     so, we can get rid of aEditorType.
   */
  static bool IsEditableContent(const nsIContent& aContent,
                                EditorType aEditorType) {
    if ((aEditorType == EditorType::HTML && !aContent.IsEditable()) ||
        EditorUtils::IsPaddingBRElementForEmptyEditor(aContent)) {
      return false;
    }

    // In HTML editors, if we're dealing with an element, then ask it
    // whether it's editable.
    if (aContent.IsElement()) {
      return aEditorType == EditorType::HTML ? aContent.IsEditable() : true;
    }
    // Text nodes are considered to be editable by both typed of editors.
    return aContent.IsText();
  }

  /**
   * Returns true if aContent is a usual element node (not padding <br> element
   * for empty editor) or a text node.  In other words, returns true if
   * aContent is a usual element node or visible data node.
   */
  static bool IsElementOrText(const nsIContent& aContent) {
    if (aContent.IsText()) {
      return true;
    }
    return aContent.IsElement() &&
           !EditorUtils::IsPaddingBRElementForEmptyEditor(aContent);
  }

  /**
   * Helper method for `AppendString()` and `AppendSubString()`.  This should
   * be called only when `aText` is in a password field.  This method masks
   * A part of or all of `aText` (`aStartOffsetInText` and later) should've
   * been copied (apppended) to `aString`.  `aStartOffsetInString` is where
   * the password was appended into `aString`.
   */
  static void MaskString(nsString& aString, dom::Text* aText,
                         uint32_t aStartOffsetInString,
                         uint32_t aStartOffsetInText);

  static nsStaticAtom* GetTagNameAtom(const nsAString& aTagName) {
    if (aTagName.IsEmpty()) {
      return nullptr;
    }
    nsAutoString lowerTagName;
    nsContentUtils::ASCIIToLower(aTagName, lowerTagName);
    return NS_GetStaticAtom(lowerTagName);
  }

  static nsStaticAtom* GetAttributeAtom(const nsAString& aAttribute) {
    if (aAttribute.IsEmpty()) {
      return nullptr;  // Don't use nsGkAtoms::_empty for attribute.
    }
    return NS_GetStaticAtom(aAttribute);
  }

  /**
   * Helper method for deletion.  When this returns true, Selection will be
   * computed with nsFrameSelection that also requires flushed layout
   * information.
   */
  static bool IsFrameSelectionRequiredToExtendSelection(
      nsIEditor::EDirection aDirectionAndAmount, Selection& aSelection) {
    switch (aDirectionAndAmount) {
      case nsIEditor::eNextWord:
      case nsIEditor::ePreviousWord:
      case nsIEditor::eToBeginningOfLine:
      case nsIEditor::eToEndOfLine:
        return true;
      case nsIEditor::ePrevious:
      case nsIEditor::eNext:
        return aSelection.IsCollapsed();
      default:
        return false;
    }
  }
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
