/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#ifndef nsEditorUtils_h__
#define nsEditorUtils_h__


#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsIEditor.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsEditor.h"
#include "nsIContentIterator.h"

class nsPlaintextEditor;

/***************************************************************************
 * stack based helper class for batching a collection of txns inside a 
 * placeholder txn.
 */
class nsAutoPlaceHolderBatch
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
class nsAutoSelectionReset
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
class nsAutoRules
{
  public:
  
  nsAutoRules(nsEditor *ed, PRInt32 action, nsIEditor::EDirection aDirection) : 
         mEd(ed), mDoNothing(PR_FALSE)
  { 
    if (mEd && !mEd->mAction) // mAction will already be set if this is nested call
    {
      mEd->StartOperation(action, aDirection);
    }
    else mDoNothing = PR_TRUE; // nested calls will end up here
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
  PRBool mDoNothing;
};


/***************************************************************************
 * stack based helper class for turning off active selection adjustment
 * by low level transactions
 */
class nsAutoTxnsConserveSelection
{
  public:
  
  nsAutoTxnsConserveSelection(nsEditor *ed) : mEd(ed), mOldState(PR_TRUE)
  {
    if (mEd) 
    {
      mOldState = mEd->GetShouldTxnSetSelection();
      mEd->SetShouldTxnSetSelection(PR_FALSE);
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
  PRBool mOldState;
};

/***************************************************************************
 * stack based helper class for batching reflow and paint requests.
 */
class nsAutoUpdateViewBatch
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

class nsDomIterFunctor 
{
  public:
    virtual void* operator()(nsIDOMNode* aNode)=0;
};

class nsBoolDomIterFunctor 
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)=0;
};

class nsDOMIterator
{
  public:
    nsDOMIterator();
    virtual ~nsDOMIterator();
    
    nsresult Init(nsIDOMRange* aRange);
    nsresult Init(nsIDOMNode* aNode);
    void ForEach(nsDomIterFunctor& functor) const;
    nsresult MakeList(nsBoolDomIterFunctor& functor,
                      nsCOMPtr<nsISupportsArray> *outArrayOfNodes) const;
    nsresult AppendList(nsBoolDomIterFunctor& functor,
                      nsCOMPtr<nsISupportsArray> arrayOfNodes) const;
  protected:
    nsCOMPtr<nsIContentIterator> mIter;
};

class nsDOMSubtreeIterator : public nsDOMIterator
{
  public:
    nsDOMSubtreeIterator();
    virtual ~nsDOMSubtreeIterator();

    nsresult Init(nsIDOMRange* aRange);
    nsresult Init(nsIDOMNode* aNode);
};

class nsTrivialFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all nodes iterator covers
    {
      return PR_TRUE;
    }
};


#endif // nsEditorUtils_h__
