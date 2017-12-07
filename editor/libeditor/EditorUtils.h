/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/dom/Selection.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/GuardObjects.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nscore.h"

class nsAtom;
class nsIContentIterator;
class nsIDOMDocument;
class nsIDOMEvent;
class nsISimpleEnumerator;
class nsITransferable;
class nsRange;

namespace mozilla {
template <class T> class OwningNonNull;

/***************************************************************************
 * EditActionResult is useful to return multiple results of an editor
 * action handler without out params.
 * Note that when you return an anonymous instance from a method, you should
 * use EditActionIgnored(), EditActionHandled() or EditActionCanceled() for
 * easier to read.  In other words, EditActionResult should be used when
 * declaring return type of a method, being an argument or defined as a local
 * variable.
 */
class MOZ_STACK_CLASS EditActionResult final
{
public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Canceled() const { return mCanceled; }
  bool Handled() const { return mHandled; }

  EditActionResult SetResult(nsresult aRv)
  {
    mRv = aRv;
    return *this;
  }
  EditActionResult MarkAsCanceled()
  {
    mCanceled = true;
    return *this;
  }
  EditActionResult MarkAsHandled()
  {
    mHandled = true;
    return *this;
  }

  explicit EditActionResult(nsresult aRv)
    : mRv(aRv)
    , mCanceled(false)
    , mHandled(false)
  {
  }

  EditActionResult& operator|=(const EditActionResult& aOther)
  {
    mCanceled |= aOther.mCanceled;
    mHandled |= aOther.mHandled;
    // When both result are same, keep the result.
    if (mRv == aOther.mRv) {
      return *this;
    }
    // If one of the results is error, use NS_ERROR_FAILURE.
    if (Failed() || aOther.Failed()) {
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
    : mRv(aRv)
    , mCanceled(aCanceled)
    , mHandled(aHandled)
  {
  }

  EditActionResult()
    : mRv(NS_ERROR_NOT_INITIALIZED)
    , mCanceled(false)
    , mHandled(false)
  {
  }

  friend EditActionResult EditActionIgnored(nsresult aRv);
  friend EditActionResult EditActionHandled(nsresult aRv);
  friend EditActionResult EditActionCanceled(nsresult aRv);
};

/***************************************************************************
 * When an edit action handler (or its helper) does nothing,
 * EditActionIgnored should be returned.
 */
inline EditActionResult
EditActionIgnored(nsresult aRv = NS_OK)
{
  return EditActionResult(aRv, false, false);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and not canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult
EditActionHandled(nsresult aRv = NS_OK)
{
  return EditActionResult(aRv, false, true);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult
EditActionCanceled(nsresult aRv = NS_OK)
{
  return EditActionResult(aRv, true, true);
}

/***************************************************************************
 * SplitNodeResult is a simple class for EditorBase::SplitNodeDeep().
 * This makes the callers' code easier to read.
 */
class MOZ_STACK_CLASS SplitNodeResult final
{
public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }

  /**
   * DidSplit() returns true if a node was actually split.
   */
  bool DidSplit() const
  {
    return mPreviousNode && mNextNode;
  }

  /**
   * GetLeftNode() simply returns the left node which was created at splitting.
   * This returns nullptr if the node wasn't split.
   */
  nsIContent* GetLeftNode() const
  {
    return mPreviousNode && mNextNode ? mPreviousNode.get() : nullptr;
  }

  /**
   * GetRightNode() simply returns the right node which was split.
   * This won't return nullptr unless failed to split due to invalid arguments.
   */
  nsIContent* GetRightNode() const
  {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.GetChild();
    }
    return mPreviousNode && !mNextNode ? mPreviousNode : mNextNode;
  }

  /**
   * GetPreviousNode() returns previous node at the split point.
   */
  nsIContent* GetPreviousNode() const
  {
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.IsEndOfContainer() ?
               mGivenSplitPoint.GetChild() : nullptr;
    }
    return mPreviousNode;
  }

  /**
   * GetNextNode() returns next node at the split point.
   */
  nsIContent* GetNextNode() const
  {
    if (mGivenSplitPoint.IsSet()) {
      return !mGivenSplitPoint.IsEndOfContainer() ?
                mGivenSplitPoint.GetChild() : nullptr;
    }
    return mNextNode;
  }

  /**
   * SplitPoint() returns the split point in the container.
   * This is useful when callers insert an element at split point with
   * EditorBase::CreateNode() or something similar methods.
   *
   * Note that the result is EditorRawDOMPoint but the nodes are grabbed
   * by this instance.  Therefore, the life time of both container node
   * and child node are guaranteed while using the result temporarily.
   */
  EditorRawDOMPoint SplitPoint() const
  {
    if (Failed()) {
      return EditorRawDOMPoint();
    }
    if (mGivenSplitPoint.IsSet()) {
      return mGivenSplitPoint.AsRaw();
    }
    if (!mPreviousNode) {
      return EditorRawDOMPoint(mNextNode);
    }
    EditorRawDOMPoint point(mPreviousNode);
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
    : mPreviousNode(aPreviousNodeOfSplitPoint)
    , mNextNode(aNextNodeOfSplitPoint)
    , mRv(NS_OK)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPreviousNode || mNextNode);
  }

  /**
   * This constructor should be used when the method didn't split any nodes
   * but want to return given split point as right point.
   */
  explicit SplitNodeResult(const EditorRawDOMPoint& aGivenSplitPoint)
    : mGivenSplitPoint(aGivenSplitPoint)
    , mRv(NS_OK)
  {
    MOZ_DIAGNOSTIC_ASSERT(mGivenSplitPoint.IsSet());
  }

  /**
   * This constructor shouldn't be used by anybody except methods which
   * use this as error result when it fails.
   */
  explicit SplitNodeResult(nsresult aRv)
    : mRv(aRv)
  {
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
 * stack based helper class for batching a collection of transactions inside a
 * placeholder transaction.
 */
class MOZ_RAII AutoPlaceholderBatch final
{
private:
  RefPtr<EditorBase> mEditorBase;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  explicit AutoPlaceholderBatch(EditorBase* aEditorBase
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditorBase(aEditorBase)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    BeginPlaceholderTransaction(nullptr);
  }
  AutoPlaceholderBatch(EditorBase* aEditorBase,
                       nsAtom* aTransactionName
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditorBase(aEditorBase)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    BeginPlaceholderTransaction(aTransactionName);
  }
  ~AutoPlaceholderBatch()
  {
    if (mEditorBase) {
      mEditorBase->EndPlaceholderTransaction();
    }
  }

private:
  void BeginPlaceholderTransaction(nsAtom* aTransactionName)
  {
    if (mEditorBase) {
      mEditorBase->BeginPlaceholderTransaction(aTransactionName);
    }
  }
};

/***************************************************************************
 * stack based helper class for saving/restoring selection.  Note that this
 * assumes that the nodes involved are still around afterwards!
 */
class MOZ_RAII AutoSelectionRestorer final
{
private:
  // Ref-counted reference to the selection that we are supposed to restore.
  RefPtr<dom::Selection> mSelection;
  EditorBase* mEditorBase;  // Non-owning ref to EditorBase.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  /**
   * Constructor responsible for remembering all state needed to restore
   * aSelection.
   */
  AutoSelectionRestorer(dom::Selection* aSelection,
                        EditorBase* aEditorBase
                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  /**
   * Destructor restores mSelection to its former state
   */
  ~AutoSelectionRestorer();

  /**
   * Abort() cancels to restore the selection.
   */
  void Abort();
};

/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich
 */
class MOZ_RAII AutoRules final
{
public:
  AutoRules(EditorBase* aEditorBase, EditAction aAction,
            nsIEditor::EDirection aDirection
            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditorBase(aEditorBase)
    , mDoNothing(false)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    // mAction will already be set if this is nested call
    if (mEditorBase && !mEditorBase->mAction) {
      mEditorBase->StartOperation(aAction, aDirection);
    } else {
      mDoNothing = true; // nested calls will end up here
    }
  }

  ~AutoRules()
  {
    if (mEditorBase && !mDoNothing) {
      mEditorBase->EndOperation();
    }
  }

protected:
  EditorBase* mEditorBase;
  bool mDoNothing;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/***************************************************************************
 * stack based helper class for turning off active selection adjustment
 * by low level transactions
 */
class MOZ_RAII AutoTransactionsConserveSelection final
{
public:
  explicit AutoTransactionsConserveSelection(EditorBase* aEditorBase
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditorBase(aEditorBase)
    , mOldState(true)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mEditorBase) {
      mOldState = mEditorBase->GetShouldTxnSetSelection();
      mEditorBase->SetShouldTxnSetSelection(false);
    }
  }

  ~AutoTransactionsConserveSelection()
  {
    if (mEditorBase) {
      mEditorBase->SetShouldTxnSetSelection(mOldState);
    }
  }

protected:
  EditorBase* mEditorBase;
  bool mOldState;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class MOZ_RAII AutoUpdateViewBatch final
{
public:
  explicit AutoUpdateViewBatch(EditorBase* aEditorBase
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditorBase(aEditorBase)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    NS_ASSERTION(mEditorBase, "null mEditorBase pointer!");

    if (mEditorBase) {
      mEditorBase->BeginUpdateViewBatch();
    }
  }

  ~AutoUpdateViewBatch()
  {
    if (mEditorBase) {
      mEditorBase->EndUpdateViewBatch();
    }
  }

protected:
  EditorBase* mEditorBase;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_STACK_CLASS AutoRangeArray final
{
public:
  explicit AutoRangeArray(dom::Selection* aSelection)
  {
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

class BoolDomIterFunctor
{
public:
  virtual bool operator()(nsINode* aNode) const = 0;
};

class MOZ_RAII DOMIterator
{
public:
  explicit DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);

  explicit DOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  virtual ~DOMIterator();

  nsresult Init(nsRange& aRange);

  void AppendList(
         const BoolDomIterFunctor& functor,
         nsTArray<mozilla::OwningNonNull<nsINode>>& arrayOfNodes) const;

protected:
  nsCOMPtr<nsIContentIterator> mIter;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII DOMSubtreeIterator final : public DOMIterator
{
public:
  explicit DOMSubtreeIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  virtual ~DOMSubtreeIterator();

  nsresult Init(nsRange& aRange);
};

class TrivialFunctor final : public BoolDomIterFunctor
{
public:
  // Used to build list of all nodes iterator covers
  virtual bool operator()(nsINode* aNode) const
  {
    return true;
  }
};

class EditorUtils final
{
public:
  /**
   * IsDescendantOf() checks if aNode is a child or a descendant of aParent.
   * aOutPoint is set to the child of aParent.
   *
   * @return            true if aNode is a child or a descendant of aParent.
   */
  static bool IsDescendantOf(const nsINode& aNode,
                             const nsINode& aParent,
                             EditorRawDOMPoint* aOutPoint = nullptr);
  static bool IsDescendantOf(const nsINode& aNode,
                             const nsINode& aParent,
                             EditorDOMPoint* aOutPoint);

  static bool IsLeafNode(nsIDOMNode* aNode);
};

class EditorHookUtils final
{
public:
  static bool DoInsertionHook(nsIDOMDocument* aDoc, nsIDOMEvent* aEvent,
                              nsITransferable* aTrans);

private:
  static nsresult GetHookEnumeratorFromDocument(
                    nsIDOMDocument*aDoc,
                    nsISimpleEnumerator** aEnumerator);
};

} // namespace mozilla

#endif // #ifndef mozilla_EditorUtils_h
