/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "InsertTextTxn.h"
#include "nsEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMSelection.h"
#include "nsIPresShell.h"
#include "EditAggregateTxn.h"

static NS_DEFINE_IID(kInsertTextTxnIID, INSERT_TEXT_TXN_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

nsIAtom *InsertTextTxn::gInsertTextTxnName;

nsresult InsertTextTxn::ClassInit()
{
  if (nsnull==gInsertTextTxnName)
    gInsertTextTxnName = NS_NewAtom("NS_InsertTextTxn");
  return NS_OK;
}

InsertTextTxn::InsertTextTxn()
  : EditTxn()
{
}

InsertTextTxn::~InsertTextTxn()
{
}

NS_IMETHODIMP InsertTextTxn::Init(nsIDOMCharacterData *aElement,
                                  PRUint32             aOffset,
                                  const nsString      &aStringToInsert,
                                  nsIPresShell        *aPresShell)
{
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mStringToInsert = aStringToInsert;
  mPresShell = aPresShell;
  return NS_OK;
}

NS_IMETHODIMP InsertTextTxn::Do(void)
{
  if (gNoisy) { printf("Do Insert Text element = %p\n", mElement.get()); }
  // advance caret: This requires the presentation shell to get the selection.
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result = mPresShell->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection,"Could not get selection in InsertTextTxn::Do\n");
  if (NS_SUCCEEDED(result) && selection) {
    result = mElement->InsertData(mOffset, mStringToInsert);
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mElement, mOffset+mStringToInsert.Length());
      NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after insert.");
    }
  }
  return result;
}

NS_IMETHODIMP InsertTextTxn::Undo(void)
{
  if (gNoisy) { printf("Undo Insert Text element = %p\n", mElement.get()); }
  nsresult result;
  PRUint32 length = mStringToInsert.Length();
  result = mElement->DeleteData(mOffset, length);
  if (NS_SUCCEEDED(result))
  { // set the selection to the insertion point where the string was removed
    nsCOMPtr<nsIDOMSelection> selection;
    result = mPresShell->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      result = selection->Collapse(mElement, mOffset);
      NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after undo of insert.");
    }
  }
  return result;
}

NS_IMETHODIMP InsertTextTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  // set out param default value
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  nsresult result = NS_OK;
  if ((nsnull!=aDidMerge) && (nsnull!=aTransaction))
  {
    // if aTransaction isa InsertTextTxn, and if the selection hasn't changed, 
    // then absorb it
    nsCOMPtr<InsertTextTxn> otherTxn( do_QueryInterface(aTransaction) );
    if (otherTxn)
    {
      if (PR_TRUE==IsSequentialInsert(otherTxn))
      {
        nsAutoString otherData;
        otherTxn->GetData(otherData);
        mStringToInsert += otherData;
        *aDidMerge = PR_TRUE;
        if (gNoisy) { printf("InsertTextTxn assimilated %p\n", aTransaction); }
      }
    }
    else
    { // the next InsertTextTxn might be inside an aggregate that we have special knowledge of
      nsCOMPtr<EditAggregateTxn> otherTxn( do_QueryInterface(aTransaction) );
      if (otherTxn)
      {
        nsCOMPtr<nsIAtom> txnName;
        otherTxn->GetName(getter_AddRefs(txnName));
        if (txnName && txnName.get()==gInsertTextTxnName)
        { // yep, it's one of ours.  By definition, it must contain only
          // another aggregate with a single child,
          // or a single InsertTextTxn
          nsCOMPtr<EditTxn> childTxn;
          otherTxn->GetTxnAt(0, getter_AddRefs(childTxn));
          if (childTxn)
          {
            nsCOMPtr<InsertTextTxn> otherInsertTxn;
            otherInsertTxn = do_QueryInterface(childTxn);
            if (otherInsertTxn)
            {
              if (PR_TRUE==IsSequentialInsert(otherInsertTxn))
              {
                nsAutoString otherData;
                otherInsertTxn->GetData(otherData);
                mStringToInsert += otherData;
                *aDidMerge = PR_TRUE;
                if (gNoisy) { printf("InsertTextTxn assimilated %p\n", aTransaction); }
              }
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP InsertTextTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP InsertTextTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Text: ";
    **aString += mStringToInsert;
  }
  return NS_OK;
}

NS_IMETHODIMP InsertTextTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Text: ";
    **aString += mStringToInsert;
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

NS_IMETHODIMP
InsertTextTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kInsertTextTxnIID)) {
    *aInstancePtr = (void*)(InsertTextTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}

/* ============ protected methods ================== */

NS_IMETHODIMP InsertTextTxn::GetData(nsString& aResult)
{
  aResult = mStringToInsert;
  return NS_OK;
}

PRBool InsertTextTxn::IsSequentialInsert(InsertTextTxn *aOtherTxn)
{
  NS_ASSERTION(nsnull!=aOtherTxn, "null param");
  PRBool result=PR_FALSE;
  if (nsnull!=aOtherTxn)
  {
    if (aOtherTxn->mElement == mElement)
    {
      // here, we need to compare offsets.
      PRInt32 strlen = mStringToInsert.Length();
      if (aOtherTxn->mOffset==(mOffset+strlen))
      {
        result = PR_TRUE;
      }
    }
  }
  return result;
}
