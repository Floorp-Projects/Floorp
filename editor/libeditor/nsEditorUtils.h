/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsEditorUtils_h__
#define nsEditorUtils_h__


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

/***************************************************************************
 * stack based helper class for batching a collection of txns.
 * Note: I changed this to use placeholder batching so that we get
 * proper selection save/restore across undo/redo.
 */
class MOZ_RAII nsAutoEditBatch : public nsAutoPlaceHolderBatch
{
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    explicit nsAutoEditBatch(nsIEditor *aEd MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : nsAutoPlaceHolderBatch(aEd, nullptr)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~nsAutoEditBatch() {}
};

/***************************************************************************
 * stack based helper class for saving/restoring selection.  Note that this
 * assumes that the nodes involved are still around afterwards!
 */
class MOZ_RAII nsAutoSelectionReset
{
  private:
    /** ref-counted reference to the selection that we are supposed to restore */
    nsRefPtr<mozilla::dom::Selection> mSel;
    nsEditor *mEd;  // non-owning ref to nsEditor
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(mozilla::dom::Selection* aSel, nsEditor* aEd MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();

    /** Abort: cancel selection saver */
    void Abort();
};

/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich
 */
class MOZ_RAII nsAutoRules
{
  public:

  nsAutoRules(nsEditor *ed, EditAction action,
              nsIEditor::EDirection aDirection
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEd(ed), mDoNothing(false)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mEd && !mEd->mAction) // mAction will already be set if this is nested call
    {
      mEd->StartOperation(action, aDirection);
    }
    else mDoNothing = true; // nested calls will end up here
  }
  ~nsAutoRules()
  {
    if (mEd && !mDoNothing)
    {
      mEd->EndOperation();
    }
  }

  protected:
  nsEditor *mEd;
  bool mDoNothing;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};


/***************************************************************************
 * stack based helper class for turning off active selection adjustment
 * by low level transactions
 */
class MOZ_RAII nsAutoTxnsConserveSelection
{
  public:

  explicit nsAutoTxnsConserveSelection(nsEditor *ed MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mEd(ed), mOldState(true)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mEd)
    {
      mOldState = mEd->GetShouldTxnSetSelection();
      mEd->SetShouldTxnSetSelection(false);
    }
  }

  ~nsAutoTxnsConserveSelection()
  {
    if (mEd)
    {
      mEd->SetShouldTxnSetSelection(mOldState);
    }
  }

  protected:
  nsEditor *mEd;
  bool mOldState;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class MOZ_RAII nsAutoUpdateViewBatch
{
  public:

  explicit nsAutoUpdateViewBatch(nsEditor *ed MOZ_GUARD_OBJECT_NOTIFIER_PARAM) : mEd(ed)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    NS_ASSERTION(mEd, "null mEd pointer!");

    if (mEd)
      mEd->BeginUpdateViewBatch();
  }

  ~nsAutoUpdateViewBatch()
  {
    if (mEd)
      mEd->EndUpdateViewBatch();
  }

  protected:
  nsEditor *mEd;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

class nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsINode* aNode) const = 0;
};

class MOZ_RAII nsDOMIterator
{
  public:
    explicit nsDOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);

    explicit nsDOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    virtual ~nsDOMIterator();

    nsresult Init(nsRange& aRange);

    void AppendList(const nsBoolDomIterFunctor& functor,
                    nsTArray<mozilla::OwningNonNull<nsINode>>& arrayOfNodes) const;
  protected:
    nsCOMPtr<nsIContentIterator> mIter;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII nsDOMSubtreeIterator : public nsDOMIterator
{
  public:
    explicit nsDOMSubtreeIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
    virtual ~nsDOMSubtreeIterator();

    nsresult Init(nsRange& aRange);
};

class nsTrivialFunctor : public nsBoolDomIterFunctor
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
struct MOZ_STACK_CLASS DOMPoint
{
  nsCOMPtr<nsINode> node;
  int32_t offset;

  DOMPoint() : node(nullptr), offset(-1) {}
  DOMPoint(nsINode* aNode, int32_t aOffset)
    : node(aNode)
    , offset(aOffset)
  {}
  DOMPoint(nsIDOMNode* aNode, int32_t aOffset)
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


class nsEditorUtils
{
  public:
    static bool IsDescendantOf(nsINode* aNode, nsINode* aParent, int32_t* aOffset = 0);
    static bool IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t *aOffset = 0);
    static bool IsLeafNode(nsIDOMNode *aNode);
};


class nsIDOMEvent;
class nsISimpleEnumerator;
class nsITransferable;

class nsEditorHookUtils
{
  public:
    static bool     DoInsertionHook(nsIDOMDocument *aDoc, nsIDOMEvent *aEvent,
                                    nsITransferable *aTrans);
  private:
    static nsresult GetHookEnumeratorFromDocument(nsIDOMDocument *aDoc,
                                                  nsISimpleEnumerator **aEnumerator);
};

#endif // nsEditorUtils_h__
