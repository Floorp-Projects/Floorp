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

#include "IMETextTxn.h"
#include "IMECommitTxn.h"
#include "nsEditor.h"
#include "nsIDOMCharacterData.h"
#include "nsIPrivateTextRange.h"
#include "nsIDOMSelection.h"
#include "nsIPresShell.h"
#include "EditAggregateTxn.h"

static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);

nsIAtom *IMETextTxn::gIMETextTxnName = nsnull;

nsresult IMETextTxn::ClassInit()
{
  if (nsnull==gIMETextTxnName)
    gIMETextTxnName = NS_NewAtom("NS_IMETextTxn");
  return NS_OK;
}

IMETextTxn::IMETextTxn()
  : EditTxn()
{
}

IMETextTxn::~IMETextTxn()
{
  mRangeList = do_QueryInterface(nsnull);
}

NS_IMETHODIMP IMETextTxn::Init(nsIDOMCharacterData     *aElement,
                               PRUint32                 aOffset,
                               PRUint32                 aReplaceLength,
                               nsIPrivateTextRangeList *aTextRangeList,
                               const nsString          &aStringToInsert,
                               nsWeakPtr                aPresShellWeak)
{
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mReplaceLength = aReplaceLength;
  mStringToInsert = aStringToInsert;
  mPresShellWeak = aPresShellWeak;
  mRangeList = do_QueryInterface(aTextRangeList);
  mFixed = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::Do(void)
{

#ifdef DEBUG_TAGUE
  printf("Do IME Text element = %p\n", mElement.get());
#endif

  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  // advance caret: This requires the presentation shell to get the selection.
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  NS_ASSERTION(selection,"Could not get selection in IMEtextTxn::Do\n");
  if (NS_SUCCEEDED(result) && selection) {
    if (mReplaceLength==0) {
      result = mElement->InsertData(mOffset,mStringToInsert);
    } else {
      result = mElement->ReplaceData(mOffset,mReplaceLength,mStringToInsert);
    }
    if (NS_SUCCEEDED(result)) {
      result = CollapseTextSelection();
    }
  }

  return result;
}

NS_IMETHODIMP IMETextTxn::Undo(void)
{
#ifdef DEBUG_TAGUE
  printf("Undo IME Text element = %p\n", mElement.get());
#endif

  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsresult result;
  PRUint32 length = mStringToInsert.Length();
  result = mElement->DeleteData(mOffset, length);
  if (NS_SUCCEEDED(result))
  { // set the selection to the insertion point where the string was removed
    nsCOMPtr<nsIDOMSelection> selection;
    result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      result = selection->Collapse(mElement, mOffset);
      NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after undo of IME insert.");
    }
  }
  return result;
}

NS_IMETHODIMP IMETextTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  nsresult  result;
#ifdef DEBUG_TAGUE
  printf("Merge IME Text element = %p\n", mElement.get());
#endif

  //
  // check to make sure we have valid return pointers
  //
  if ((nsnull==aDidMerge) && (nsnull==aTransaction))
  {
    return NS_OK;
  }

  // 
  // check to make sure we aren't fixed, if we are then nothing get's absorbed
  //
  if (mFixed) {
    *aDidMerge = PR_FALSE;
    return NS_OK;
  }

  //
  // if aTransaction is another IMETextTxn then absorbe it
  //
  IMETextTxn*  otherTxn = nsnull;
  result = aTransaction->QueryInterface(IMETextTxn::GetCID(),(void**)&otherTxn);
  if (otherTxn && result==NS_OK)
  {
    //
    //  we absorbe the next IME transaction by adopting it's insert string as our own
    //
    nsIPrivateTextRangeList* newTextRangeList;
    otherTxn->GetData(mStringToInsert,&newTextRangeList);
    mRangeList = do_QueryInterface(newTextRangeList);
    *aDidMerge = PR_TRUE;
#ifdef DEBUG_TAGUE
    printf("IMETextTxn assimilated IMETextTxn:%p\n", aTransaction);
#endif
    NS_RELEASE(otherTxn);
    return NS_OK;
  }

  //
  // second possible case is that we have a commit transaction
  //
  IMECommitTxn* commitTxn = nsnull;
  result = aTransaction->QueryInterface(IMECommitTxn::GetCID(),(void**)&commitTxn);
  if (commitTxn && result==NS_OK)
  {
    (void)CollapseTextSelectionOnCommit();
    mFixed = PR_TRUE;
    *aDidMerge = PR_TRUE;  // absorbe the commit transaction
#ifdef DEBUG_TAGUE
    printf("IMETextTxn assimilated IMECommitTxn%p\n", aTransaction);
#endif
    NS_RELEASE(commitTxn);
    return NS_OK;
  }

  *aDidMerge = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::GetUndoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Remove Text: ";
    *aString += mStringToInsert;
  }
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::GetRedoString(nsString *aString)
{
  if (nsnull!=aString)
  {
    *aString="Insert Text: ";
    *aString += mStringToInsert;
  }
  return NS_OK;
}

/* ============= nsISupports implementation ====================== */

NS_IMETHODIMP
IMETextTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(IMETextTxn::GetCID())) {
    *aInstancePtr = (void*)(IMETextTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (EditTxn::QueryInterface(aIID, aInstancePtr));
}

/* ============ protected methods ================== */

NS_IMETHODIMP IMETextTxn::GetData(nsString& aResult,nsIPrivateTextRangeList** aTextRangeList)
{
  aResult = mStringToInsert;
  *aTextRangeList = mRangeList;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::CollapseTextSelection(void)
{
    nsresult      result;
    PRBool        haveSelectedRange, haveCaretPosition;
    PRUint16      textRangeListLength,selectionStart,selectionEnd,
              textRangeType, caretPosition, i;
    nsIPrivateTextRange*  textRange;

    haveSelectedRange = PR_FALSE;
    haveCaretPosition = PR_FALSE;
    

#ifdef DEBUG_tague
    PRUint16 listlen,start,stop,type;
    nsIPrivateTextRange* rangePtr;
    result = mRangeList->GetLength(&listlen);
    printf("nsIPrivateTextRangeList[%p]\n",mRangeList);
    for (i=0;i<listlen;i++) {
      (void)mRangeList->Item(i,&rangePtr);
      rangePtr->GetRangeStart(&start);
      rangePtr->GetRangeEnd(&stop);
      rangePtr->GetRangeType(&type);
      printf("range[%d] start=%d end=%d type=",i,start,stop,type);
      if (type==nsIPrivateTextRange::TEXTRANGE_RAWINPUT) printf("TEXTRANGE_RAWINPUT\n");
      if (type==nsIPrivateTextRange::TEXTRANGE_SELECTEDRAWTEXT) printf("TEXTRANGE_SELECTEDRAWTEXT\n");
      if (type==nsIPrivateTextRange::TEXTRANGE_CONVERTEDTEXT) printf("TEXTRANGE_CONVERTEDTEXT\n");
      if (type==nsIPrivateTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT) printf("TEXTRANGE_SELECTEDCONVERTEDTEXT\n");
    }
#endif
        
    //
    // run through the text range list
    //
    result = mRangeList->GetLength(&textRangeListLength);
    if (NS_SUCCEEDED(result)) 
    {
      for(i=0;i<textRangeListLength;i++) {
        result = mRangeList->Item(i,&textRange);
        if (NS_SUCCEEDED(result))
        {
          result = textRange->GetRangeType(&textRangeType);
          if (textRangeType==nsIPrivateTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT) 
          {
            haveSelectedRange = PR_TRUE;
            textRange->GetRangeStart(&selectionStart);
            textRange->GetRangeEnd(&selectionEnd);
          }
          if (textRangeType==nsIPrivateTextRange::TEXTRANGE_CARETPOSITION)
          {
            haveCaretPosition = PR_TRUE;
            textRange->GetRangeStart(&caretPosition);
          }
        }
      }
    }

    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMSelection> selection;
    result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection){
      if (haveSelectedRange) {
        result = selection->Collapse(mElement,mOffset+selectionStart);
        result = selection->Extend(mElement,mOffset+selectionEnd);
      } else {
        if (haveCaretPosition)
          result = selection->Collapse(mElement,mOffset+caretPosition);
        else
          result = selection->Collapse(mElement,mOffset+mStringToInsert.Length());
      }
    }

    return result;
}

NS_IMETHODIMP IMETextTxn::CollapseTextSelectionOnCommit(void)
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;
  nsresult result = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection){
      result = selection->Collapse(mElement,mOffset+mStringToInsert.Length());
  }

  return result;
}
