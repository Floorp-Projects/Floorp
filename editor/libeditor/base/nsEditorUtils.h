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
#include "nsIDOMSelection.h"
#include "nsIEditor.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsEditor.h"

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
    nsCOMPtr<nsIDOMSelection> mSel;
    nsEditor *mEd;  // non-owning ref to nsEditor

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(nsIDOMSelection *aSel, nsEditor *aEd);
    
    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();
};

/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich
 */
class nsAutoRules
{
  public:
  
  nsAutoRules(nsEditor *ed, PRInt32 action, nsIEditor::EDirection aDirection) : 
         mEd(ed), mAction(action), mDirection(aDirection)
                {if (mEd) mEd->StartOperation(mAction, mDirection);}
  ~nsAutoRules() {if (mEd) mEd->EndOperation(mAction, mDirection);}
  
  protected:
  nsEditor *mEd;
  PRInt32 mAction;
  nsIEditor::EDirection mDirection;
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


#endif // nsEditorUtils_h__
