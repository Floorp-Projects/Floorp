/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/ContentIterator.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/GuardObjects.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIEditor.h"
#include "nscore.h"

class nsAtom;
class nsISimpleEnumerator;
class nsITransferable;
class nsRange;

namespace mozilla {
template <class T>
class OwningNonNull;

namespace dom {
class Element;
class Text;
}  // namespace dom

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
  NodeType* GetNewNode() const { return mNode; }

  CreateNodeResultBase() = delete;

  explicit CreateNodeResultBase(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  explicit CreateNodeResultBase(NodeType* aNode)
      : mNode(aNode), mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}

  explicit CreateNodeResultBase(already_AddRefed<NodeType>&& aNode)
      : mNode(aNode), mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}

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
 * SplitNodeResult is a simple class for
 * EditorBase::SplitNodeDeepWithTransaction().
 * This makes the callers' code easier to read.
 */
class MOZ_STACK_CLASS SplitNodeResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }

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

  /**
   * GetLeftContent() returns new created node before the part of quarried out.
   * This may return nullptr if the method didn't split at start edge of
   * the node.
   */
  nsIContent* GetLeftContent() const { return mLeftContent; }
  dom::Element* GetLeftContentAsElement() const {
    return Element::FromNodeOrNull(mLeftContent);
  }

  /**
   * GetMiddleContent() returns new created node between left node and right
   * node.  I.e., this is quarried out from the node.  This may return nullptr
   * if the method unwrapped the middle node.
   */
  nsIContent* GetMiddleContent() const { return mMiddleContent; }
  dom::Element* GetMiddleContentAsElement() const {
    return Element::FromNodeOrNull(mMiddleContent);
  }

  /**
   * GetRightContent() returns the right node after the part of quarried out.
   * This may return nullptr it the method didn't split at end edge of the
   * node.
   */
  nsIContent* GetRightContent() const { return mRightContent; }
  dom::Element* GetRightContentAsElement() const {
    return Element::FromNodeOrNull(mRightContent);
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
    mEditorBase.BeginTransaction();
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

class BoolDomIterFunctor {
 public:
  virtual bool operator()(nsINode* aNode) const = 0;
};

class MOZ_RAII DOMIterator {
 public:
  explicit DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);

  explicit DOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  virtual ~DOMIterator() = default;

  nsresult Init(nsRange& aRange);

  void AppendList(
      const BoolDomIterFunctor& functor,
      nsTArray<mozilla::OwningNonNull<nsINode>>& arrayOfNodes) const;

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

class TrivialFunctor final : public BoolDomIterFunctor {
 public:
  // Used to build list of all nodes iterator covers
  virtual bool operator()(nsINode* aNode) const override { return true; }
};

class EditorUtils final {
 public:
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
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
