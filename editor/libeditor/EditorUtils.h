/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef EditorUtils_h
#define EditorUtils_h

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nscore.h"
#include "mozilla/GuardObjects.h"

class nsIAtom;
class nsIContentIterator;
class nsIDOMDocument;
class nsIDOMEvent;
class nsISimpleEnumerator;
class nsITransferable;
class nsRange;
namespace mozilla {
template <class T> class OwningNonNull;
namespace dom {
class Selection;
} // namespace dom
} // namespace mozilla

/***************************************************************************
 * stack based helper class for batching a collection of txns inside a
 * placeholder txn.
 */
class MOZ_RAII nsAutoPlaceHolderBatch
{
  private:
    nsCOMPtr<nsIEditor> mEd;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    nsAutoPlaceHolderBatch(nsIEditor *aEd, nsIAtom *atom MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mEd(do_QueryInterface(aEd))
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      if (mEd) {
        mEd->BeginPlaceHolderTransaction(atom);
      }
    }
    ~nsAutoPlaceHolderBatch()
    {
      if (mEd) {
        mEd->EndPlaceHolderTransaction();
      }
    }
};

namespace mozilla {

/***************************************************************************
 * stack based helper class for batching a collection of txns.
 * Note: I changed this to use placeholder batching so that we get
 * proper selection save/restore across undo/redo.
 */
class MOZ_RAII AutoEditBatch final : public nsAutoPlaceHolderBatch
{
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  explicit AutoEditBatch(nsIEditor* aEditor
                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : nsAutoPlaceHolderBatch(aEditor, nullptr)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }
  ~AutoEditBatch() {}
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
  nsEditor* mEditor;  // Non-owning ref to nsEditor.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  /**
   * Constructor responsible for remembering all state needed to restore
   * aSelection.
   */
  AutoSelectionRestorer(dom::Selection* aSelection,
                        nsEditor* aEditor
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
  AutoRules(nsEditor* aEditor, EditAction aAction,
            nsIEditor::EDirection aDirection
            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditor(aEditor)
    , mDoNothing(false)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    // mAction will already be set if this is nested call
    if (mEditor && !mEditor->mAction) {
      mEditor->StartOperation(aAction, aDirection);
    } else {
      mDoNothing = true; // nested calls will end up here
    }
  }

  ~AutoRules()
  {
    if (mEditor && !mDoNothing) {
      mEditor->EndOperation();
    }
  }

protected:
  nsEditor* mEditor;
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
  explicit AutoTransactionsConserveSelection(nsEditor* aEditor
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditor(aEditor)
    , mOldState(true)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mEditor) {
      mOldState = mEditor->GetShouldTxnSetSelection();
      mEditor->SetShouldTxnSetSelection(false);
    }
  }

  ~AutoTransactionsConserveSelection()
  {
    if (mEditor) {
      mEditor->SetShouldTxnSetSelection(mOldState);
    }
  }

protected:
  nsEditor* mEditor;
  bool mOldState;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class MOZ_RAII AutoUpdateViewBatch final
{
public:
  explicit AutoUpdateViewBatch(nsEditor* aEditor
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditor(aEditor)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    NS_ASSERTION(mEditor, "null mEditor pointer!");

    if (mEditor) {
      mEditor->BeginUpdateViewBatch();
    }
  }

  ~AutoUpdateViewBatch()
  {
    if (mEditor) {
      mEditor->EndUpdateViewBatch();
    }
  }

protected:
  nsEditor* mEditor;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
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

/******************************************************************************
 * general dom point utility struct
 *****************************************************************************/
struct MOZ_STACK_CLASS EditorDOMPoint final
{
  nsCOMPtr<nsINode> node;
  int32_t offset;

  EditorDOMPoint()
    : node(nullptr)
    , offset(-1)
  {}
  EditorDOMPoint(nsINode* aNode, int32_t aOffset)
    : node(aNode)
    , offset(aOffset)
  {}
  EditorDOMPoint(nsIDOMNode* aNode, int32_t aOffset)
    : node(do_QueryInterface(aNode))
    , offset(aOffset)
  {}

  void SetPoint(nsINode* aNode, int32_t aOffset)
  {
    node = aNode;
    offset = aOffset;
  }
  void SetPoint(nsIDOMNode* aNode, int32_t aOffset)
  {
    node = do_QueryInterface(aNode);
    offset = aOffset;
  }
};

class EditorUtils final
{
public:
  static bool IsDescendantOf(nsINode* aNode, nsINode* aParent,
                             int32_t* aOffset = 0);
  static bool IsDescendantOf(nsIDOMNode* aNode, nsIDOMNode* aParent,
                             int32_t* aOffset = 0);
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

#endif // #ifndef EditorUtils_h
