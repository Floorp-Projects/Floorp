/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsEditorUtils_h__
#define nsEditorUtils_h__


#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsIEditor.h"
#include "nsIAtom.h"
#include "nsEditor.h"
#include "nsIContentIterator.h"
#include "nsCOMArray.h"

class nsPlaintextEditor;

/***************************************************************************
 * stack based helper class for batching a collection of txns inside a 
 * placeholder txn.
 */
class NS_STACK_CLASS nsAutoPlaceHolderBatch
{
  private:
    nsCOMPtr<nsIEditor> mEd;
  public:
    nsAutoPlaceHolderBatch( nsIEditor *aEd, nsIAtom *atom) : mEd(do_QueryInterface(aEd)) 
                   { if (mEd) mEd->BeginPlaceHolderTransaction(atom); }
    ~nsAutoPlaceHolderBatch() { if (mEd) mEd->EndPlaceHolderTransaction(); }
};

/***************************************************************************
 * stack based helper class for batching a collection of txns.  
 * Note: I changed this to use placeholder batching so that we get
 * proper selection save/restore across undo/redo.
 */
class nsAutoEditBatch : public nsAutoPlaceHolderBatch
{
  public:
    nsAutoEditBatch( nsIEditor *aEd) : nsAutoPlaceHolderBatch(aEd,nsnull)  {}
    ~nsAutoEditBatch() {}
};

/***************************************************************************
 * stack based helper class for saving/restoring selection.  Note that this
 * assumes that the nodes involved are still around afterwards!
 */
class NS_STACK_CLASS nsAutoSelectionReset
{
  private:
    /** ref-counted reference to the selection that we are supposed to restore */
    nsCOMPtr<nsISelection> mSel;
    nsEditor *mEd;  // non-owning ref to nsEditor

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(nsISelection *aSel, nsEditor *aEd);
    
    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();

    /** Abort: cancel selection saver */
    void Abort();
};

/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich
 */
class NS_STACK_CLASS nsAutoRules
{
  public:
  
  nsAutoRules(nsEditor *ed, nsEditor::OperationID action,
              nsIEditor::EDirection aDirection) :
         mEd(ed), mDoNothing(false)
  { 
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
};


/***************************************************************************
 * stack based helper class for turning off active selection adjustment
 * by low level transactions
 */
class NS_STACK_CLASS nsAutoTxnsConserveSelection
{
  public:
  
  nsAutoTxnsConserveSelection(nsEditor *ed) : mEd(ed), mOldState(true)
  {
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
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class NS_STACK_CLASS nsAutoUpdateViewBatch
{
  public:
  
  nsAutoUpdateViewBatch(nsEditor *ed) : mEd(ed)
  {
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
};

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

class nsBoolDomIterFunctor 
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)=0;
};

class NS_STACK_CLASS nsDOMIterator
{
  public:
    nsDOMIterator();
    virtual ~nsDOMIterator();
    
    nsresult Init(nsIDOMRange* aRange);
    nsresult Init(nsIDOMNode* aNode);
    nsresult AppendList(nsBoolDomIterFunctor& functor,
                        nsCOMArray<nsIDOMNode>& arrayOfNodes) const;
  protected:
    nsCOMPtr<nsIContentIterator> mIter;
};

class nsDOMSubtreeIterator : public nsDOMIterator
{
  public:
    nsDOMSubtreeIterator();
    virtual ~nsDOMSubtreeIterator();

    nsresult Init(nsIDOMRange* aRange);
};

class nsTrivialFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsIDOMNode* aNode)  // used to build list of all nodes iterator covers
    {
      return true;
    }
};


/******************************************************************************
 * general dom point utility struct
 *****************************************************************************/
struct NS_STACK_CLASS DOMPoint
{
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  DOMPoint() : node(0),offset(0) {}
  DOMPoint(nsIDOMNode *aNode, PRInt32 aOffset) : 
                 node(aNode),offset(aOffset) {}
  void SetPoint(nsIDOMNode *aNode, PRInt32 aOffset)
  {
    node = aNode; offset = aOffset;
  }
  void GetPoint(nsCOMPtr<nsIDOMNode> &aNode, PRInt32 &aOffset)
  {
    aNode = node; aOffset = offset;
  }
};


class nsEditorUtils
{
  public:
    static bool IsDescendantOf(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 *aOffset = 0);
    static bool IsLeafNode(nsIDOMNode *aNode);
};


class nsITransferable;
class nsIDOMEvent;
class nsISimpleEnumerator;

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
