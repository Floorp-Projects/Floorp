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
#include "mozilla/RangeBoundary.h"
#include "mozilla/Result.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nscore.h"
#include "nsDebug.h"
#include "nsDirection.h"
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
 * HTMLEditor::SplitNodeDeepWithTransaction().
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
      EditorBase& aEditorBase)
      : mEditorBase(aEditorBase) {
    MOZ_KnownLive(mEditorBase).BeginTransaction();
  }

  MOZ_CAN_RUN_SCRIPT ~AutoTransactionBatchExternal() {
    MOZ_KnownLive(mEditorBase).EndTransaction();
  }

 private:
  EditorBase& mEditorBase;
};

/******************************************************************************
 * AutoSelectionRangeArray stores all ranges in `aSelection`.
 * Note that modifying the ranges means modifing the selection ranges.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoSelectionRangeArray final {
 public:
  explicit AutoSelectionRangeArray(dom::Selection& aSelection) {
    uint32_t rangeCount = aSelection.RangeCount();
    for (uint32_t i = 0; i < rangeCount; i++) {
      mRanges.AppendElement(*aSelection.GetRangeAt(i));
    }
  }

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
};

/******************************************************************************
 * AutoRangeArray stores ranges which do no belong any `Selection`.
 * So, different from `AutoSelectionRangeArray`, this can be used for
 * ranges which may need to be modified before touching the DOM tree,
 * but does not want to modify `Selection` for the performance.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoRangeArray final {
 public:
  explicit AutoRangeArray(const dom::Selection& aSelection) {
    Initialize(aSelection);
  }

  void Initialize(const dom::Selection& aSelection) {
    mDirection = aSelection.GetDirection();
    mRanges.Clear();
    for (uint32_t i = 0; i < aSelection.RangeCount(); i++) {
      mRanges.AppendElement(aSelection.GetRangeAt(i)->CloneRange());
      if (aSelection.GetRangeAt(i) == aSelection.GetAnchorFocusRange()) {
        mAnchorFocusRange = mRanges.LastElement();
      }
    }
  }

  /**
   * EnsureOnlyEditableRanges() removes ranges which cannot modify.
   * Note that this is designed only for `HTMLEditor` because this must not
   * be required by `TextEditor`.
   */
  void EnsureOnlyEditableRanges(const dom::Element& aEditingHost);
  static bool IsEditableRange(const dom::AbstractRange& aRange,
                              const dom::Element& aEditingHost);

  auto& Ranges() { return mRanges; }
  const auto& Ranges() const { return mRanges; }
  auto& FirstRangeRef() { return mRanges[0]; }
  const auto& FirstRangeRef() const { return mRanges[0]; }

  template <template <typename> typename StrongPtrType>
  AutoTArray<StrongPtrType<nsRange>, 8> CloneRanges() const {
    AutoTArray<StrongPtrType<nsRange>, 8> ranges;
    for (const auto& range : mRanges) {
      ranges.AppendElement(range->CloneRange());
    }
    return ranges;
  }

  EditorDOMPoint GetStartPointOfFirstRange() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPoint();
    }
    return EditorDOMPoint(mRanges[0]->StartRef());
  }
  EditorDOMPoint GetEndPointOfFirstRange() const {
    if (mRanges.IsEmpty() || !mRanges[0]->IsPositioned()) {
      return EditorDOMPoint();
    }
    return EditorDOMPoint(mRanges[0]->EndRef());
  }

  nsresult SelectNode(nsINode& aNode) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      mAnchorFocusRange = nsRange::Create(&aNode);
      if (!mAnchorFocusRange) {
        return NS_ERROR_FAILURE;
      }
    }
    ErrorResult error;
    mAnchorFocusRange->SelectNode(aNode, error);
    if (error.Failed()) {
      mAnchorFocusRange = nullptr;
      return error.StealNSResult();
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }

  /**
   * ExtendAnchorFocusRangeFor() extends the anchor-focus range for deleting
   * content for aDirectionAndAmount.  The range won't be extended to outer of
   * selection limiter.  Note that if a range is extened, the range is
   * recreated.  Therefore, caller cannot cache pointer of any ranges before
   * calling this.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<nsIEditor::EDirection, nsresult>
  ExtendAnchorFocusRangeFor(const EditorBase& aEditorBase,
                            nsIEditor::EDirection aDirectionAndAmount);

  /**
   * For compatiblity with the other browsers, we should shrink ranges to
   * start from an atomic content and/or end after one instead of start
   * from end of a preceding text node and end by start of a follwing text
   * node.  Returns true if this modifies a range.
   */
  enum class IfSelectingOnlyOneAtomicContent {
    Collapse,  // Collapse to the range selecting only one atomic content to
               // start or after of it.  Whether to collapse start or after
               // it depends on aDirectionAndAmount.  This is ignored if
               // there are multiple ranges.
    KeepSelecting,  // Won't collapse the range.
  };
  Result<bool, nsresult> ShrinkRangesIfStartFromOrEndAfterAtomicContent(
      const HTMLEditor& aHTMLEditor, nsIEditor::EDirection aDirectionAndAmount,
      IfSelectingOnlyOneAtomicContent aIfSelectingOnlyOneAtomicContent,
      const dom::Element* aEditingHost);

  /**
   * The following methods are same as `Selection`'s methods.
   */
  bool IsCollapsed() const {
    return mRanges.IsEmpty() ||
           (mRanges.Length() == 1 && mRanges[0]->Collapsed());
  }
  template <typename PT, typename CT>
  nsresult Collapse(const EditorDOMPointBase<PT, CT>& aPoint) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      ErrorResult error;
      mAnchorFocusRange = nsRange::Create(aPoint.ToRawRangeBoundary(),
                                          aPoint.ToRawRangeBoundary(), error);
      if (error.Failed()) {
        mAnchorFocusRange = nullptr;
        return error.StealNSResult();
      }
    } else {
      nsresult rv = mAnchorFocusRange->CollapseTo(aPoint.ToRawRangeBoundary());
      if (NS_FAILED(rv)) {
        mAnchorFocusRange = nullptr;
        return rv;
      }
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }
  template <typename SPT, typename SCT, typename EPT, typename ECT>
  nsresult SetStartAndEnd(const EditorDOMPointBase<SPT, SCT>& aStart,
                          const EditorDOMPointBase<EPT, ECT>& aEnd) {
    mRanges.Clear();
    if (!mAnchorFocusRange) {
      ErrorResult error;
      mAnchorFocusRange = nsRange::Create(aStart.ToRawRangeBoundary(),
                                          aEnd.ToRawRangeBoundary(), error);
      if (error.Failed()) {
        mAnchorFocusRange = nullptr;
        return error.StealNSResult();
      }
    } else {
      nsresult rv = mAnchorFocusRange->SetStartAndEnd(
          aStart.ToRawRangeBoundary(), aEnd.ToRawRangeBoundary());
      if (NS_FAILED(rv)) {
        mAnchorFocusRange = nullptr;
        return rv;
      }
    }
    mRanges.AppendElement(*mAnchorFocusRange);
    return NS_OK;
  }
  const nsRange* GetAnchorFocusRange() const { return mAnchorFocusRange; }
  nsDirection GetDirection() const { return mDirection; }

  const RangeBoundary& AnchorRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->StartRef()
                                               : mAnchorFocusRange->EndRef();
  }
  nsINode* GetAnchorNode() const {
    return AnchorRef().IsSet() ? AnchorRef().Container() : nullptr;
  }
  uint32_t GetAnchorOffset() const {
    return AnchorRef().IsSet()
               ? AnchorRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  nsIContent* GetChildAtAnchorOffset() const {
    return AnchorRef().IsSet() ? AnchorRef().GetChildAtOffset() : nullptr;
  }

  const RangeBoundary& FocusRef() const {
    if (!mAnchorFocusRange) {
      static RangeBoundary sEmptyRangeBoundary;
      return sEmptyRangeBoundary;
    }
    return mDirection == nsDirection::eDirNext ? mAnchorFocusRange->EndRef()
                                               : mAnchorFocusRange->StartRef();
  }
  nsINode* GetFocusNode() const {
    return FocusRef().IsSet() ? FocusRef().Container() : nullptr;
  }
  uint32_t FocusOffset() const {
    return FocusRef().IsSet()
               ? FocusRef()
                     .Offset(RangeBoundary::OffsetFilter::kValidOffsets)
                     .valueOr(0)
               : 0;
  }
  nsIContent* GetChildAtFocusOffset() const {
    return FocusRef().IsSet() ? FocusRef().GetChildAtOffset() : nullptr;
  }

  void RemoveAllRanges() {
    mRanges.Clear();
    mAnchorFocusRange = nullptr;
    mDirection = nsDirection::eDirNext;
  }

 private:
  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
  RefPtr<nsRange> mAnchorFocusRange;
  nsDirection mDirection = nsDirection::eDirNext;
};

/******************************************************************************
 * some helper classes for iterating the dom tree
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

/**
 * ReplaceRangeDataBase() represents range to be replaced and replacing string.
 */
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
    if (aEditorType == EditorType::HTML && !aContent.IsEditable()) {
      // FIXME(emilio): Why only for HTML editors? All content from the root
      // content in text editors is also editable, so afaict we can remove the
      // special-case.
      return false;
    }
    return IsElementOrText(aContent);
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
    return aContent.IsElement() && !IsPaddingBRElementForEmptyEditor(aContent);
  }

  /**
   * IsContentPreformatted() checks the style info for the node for the
   * preformatted text style.  This does NOT flush layout.
   */
  static bool IsContentPreformatted(nsIContent& aContent);

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
  template <typename SelectionOrAutoRangeArray>
  static bool IsFrameSelectionRequiredToExtendSelection(
      nsIEditor::EDirection aDirectionAndAmount,
      SelectionOrAutoRangeArray& aSelectionOrAutoRangeArray) {
    switch (aDirectionAndAmount) {
      case nsIEditor::eNextWord:
      case nsIEditor::ePreviousWord:
      case nsIEditor::eToBeginningOfLine:
      case nsIEditor::eToEndOfLine:
        return true;
      case nsIEditor::ePrevious:
      case nsIEditor::eNext:
        return aSelectionOrAutoRangeArray.IsCollapsed();
      default:
        return false;
    }
  }

  /**
   * Returns true if aSelection includes the point in aParentContent.
   */
  static bool IsPointInSelection(const Selection& aSelection,
                                 const nsINode& aParentNode, int32_t aOffset);
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
