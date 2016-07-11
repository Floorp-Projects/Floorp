/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/EditorBase.h"
#include "mozilla/GuardObjects.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIDOMNode.h"
#include "nsIEditor.h"
#include "nscore.h"

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

/***************************************************************************
 * stack based helper class for batching a collection of txns inside a
 * placeholder txn.
 */
class MOZ_RAII AutoPlaceHolderBatch
{
private:
  nsCOMPtr<nsIEditor> mEditor;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  AutoPlaceHolderBatch(nsIEditor* aEditor,
                       nsIAtom* aAtom
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEditor(aEditor)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mEditor) {
      mEditor->BeginPlaceHolderTransaction(aAtom);
    }
  }
  ~AutoPlaceHolderBatch()
  {
    if (mEditor) {
      mEditor->EndPlaceHolderTransaction();
    }
  }
};

/***************************************************************************
 * stack based helper class for batching a collection of txns.
 * Note: I changed this to use placeholder batching so that we get
 * proper selection save/restore across undo/redo.
 */
class MOZ_RAII AutoEditBatch final : public AutoPlaceHolderBatch
{
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

public:
  explicit AutoEditBatch(nsIEditor* aEditor
                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : AutoPlaceHolderBatch(aEditor, nullptr)
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

#endif // #ifndef mozilla_EditorUtils_h
