/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMETextTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsRange.h"
#include "nsIPrivateTextRange.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsComponentManagerUtils.h"
#include "nsIEditor.h"

// #define DEBUG_IMETXN

IMETextTxn::IMETextTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IMETextTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IMETextTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mElement)
  // mRangeList can't lead to cycles
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IMETextTxn, EditTxn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mElement)
  // mRangeList can't lead to cycles
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMETextTxn)
  if (aIID.Equals(IMETextTxn::GetCID())) {
    *aInstancePtr = (void*)(IMETextTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP IMETextTxn::Init(nsIDOMCharacterData     *aElement,
                               PRUint32                 aOffset,
                               PRUint32                 aReplaceLength,
                               nsIPrivateTextRangeList *aTextRangeList,
                               const nsAString         &aStringToInsert,
                               nsIEditor               *aEditor)
{
  NS_ASSERTION(aElement, "illegal value- null ptr- aElement");
  NS_ASSERTION(aTextRangeList, "illegal value- null ptr - aTextRangeList");
  NS_ENSURE_TRUE(aElement && aTextRangeList, NS_ERROR_NULL_POINTER);
  mElement = do_QueryInterface(aElement);
  mOffset = aOffset;
  mReplaceLength = aReplaceLength;
  mStringToInsert = aStringToInsert;
  mEditor = aEditor;
  mRangeList = do_QueryInterface(aTextRangeList);
  mFixed = false;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::DoTransaction(void)
{

#ifdef DEBUG_IMETXN
  printf("Do IME Text element = %p replace = %d len = %d\n", mElement.get(), mReplaceLength, mStringToInsert.Length());
#endif

  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  // advance caret: This requires the presentation shell to get the selection.
  nsresult result;
  if (mReplaceLength == 0) {
    result = mElement->InsertData(mOffset, mStringToInsert);
  } else {
    result = mElement->ReplaceData(mOffset, mReplaceLength, mStringToInsert);
  }
  if (NS_SUCCEEDED(result)) {
    result = CollapseTextSelection();
  }

  return result;
}

NS_IMETHODIMP IMETextTxn::UndoTransaction(void)
{
#ifdef DEBUG_IMETXN
  printf("Undo IME Text element = %p\n", mElement.get());
#endif

  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  nsresult result = mElement->DeleteData(mOffset, mStringToInsert.Length());
  if (NS_SUCCEEDED(result))
  { // set the selection to the insertion point where the string was removed
    nsCOMPtr<nsISelection> selection;
    result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_SUCCEEDED(result) && selection) {
      result = selection->Collapse(mElement, mOffset);
      NS_ASSERTION((NS_SUCCEEDED(result)), "selection could not be collapsed after undo of IME insert.");
    }
  }
  return result;
}

NS_IMETHODIMP IMETextTxn::Merge(nsITransaction *aTransaction, bool *aDidMerge)
{
  NS_ASSERTION(aDidMerge, "illegal vaule- null ptr- aDidMerge");
  NS_ASSERTION(aTransaction, "illegal vaule- null ptr- aTransaction");
  NS_ENSURE_TRUE(aDidMerge && aTransaction, NS_ERROR_NULL_POINTER);
    
#ifdef DEBUG_IMETXN
  printf("Merge IME Text element = %p\n", mElement.get());
#endif

  // 
  // check to make sure we aren't fixed, if we are then nothing get's absorbed
  //
  if (mFixed) {
    *aDidMerge = false;
    return NS_OK;
  }

  //
  // if aTransaction is another IMETextTxn then absorb it
  //
  IMETextTxn*  otherTxn = nsnull;
  nsresult result = aTransaction->QueryInterface(IMETextTxn::GetCID(),(void**)&otherTxn);
  if (otherTxn && NS_SUCCEEDED(result))
  {
    //
    //  we absorb the next IME transaction by adopting its insert string as our own
    //
    nsIPrivateTextRangeList* newTextRangeList;
    otherTxn->GetData(mStringToInsert,&newTextRangeList);
    mRangeList = do_QueryInterface(newTextRangeList);
    *aDidMerge = true;
#ifdef DEBUG_IMETXN
    printf("IMETextTxn assimilated IMETextTxn:%p\n", aTransaction);
#endif
    NS_RELEASE(otherTxn);
    return NS_OK;
  }

  *aDidMerge = false;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::MarkFixed(void)
{
  mFixed = true;
  return NS_OK;
}

NS_IMETHODIMP IMETextTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("IMETextTxn: ");
  aString += mStringToInsert;
  return NS_OK;
}

/* ============ protected methods ================== */
static SelectionType TextRangeToSelection(int aTextRangeType)
{
   switch(aTextRangeType)
   {
      case nsIPrivateTextRange::TEXTRANGE_RAWINPUT:
           return nsISelectionController::SELECTION_IME_RAWINPUT;
      case nsIPrivateTextRange::TEXTRANGE_SELECTEDRAWTEXT:
           return nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT;
      case nsIPrivateTextRange::TEXTRANGE_CONVERTEDTEXT:
           return nsISelectionController::SELECTION_IME_CONVERTEDTEXT;
      case nsIPrivateTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT:
           return nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT;
      case nsIPrivateTextRange::TEXTRANGE_CARETPOSITION:
      default:
           return nsISelectionController::SELECTION_NORMAL;
   };
}

NS_IMETHODIMP IMETextTxn::GetData(nsString& aResult,nsIPrivateTextRangeList** aTextRangeList)
{
  NS_ASSERTION(aTextRangeList, "illegal value- null ptr- aTextRangeList");
  NS_ENSURE_TRUE(aTextRangeList, NS_ERROR_NULL_POINTER);
  aResult = mStringToInsert;
  *aTextRangeList = mRangeList;
  return NS_OK;
}

static SelectionType sel[4]=
{
  nsISelectionController::SELECTION_IME_RAWINPUT,
  nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
  nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
  nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT
};

NS_IMETHODIMP IMETextTxn::CollapseTextSelection(void)
{
    nsresult      result;
    PRUint16      i;

#ifdef DEBUG_IMETXN
    PRUint16 listlen,start,stop,type;
    result = mRangeList->GetLength(&listlen);
    printf("nsIPrivateTextRangeList[%p]\n",mRangeList);
    nsIPrivateTextRange* rangePtr;
    for (i=0;i<listlen;i++) {
      (void)mRangeList->Item(i,&rangePtr);
      rangePtr->GetRangeStart(&start);
      rangePtr->GetRangeEnd(&stop);
      rangePtr->GetRangeType(&type);
      printf("range[%d] start=%d end=%d type=",i,start,stop,type);
      if (type==nsIPrivateTextRange::TEXTRANGE_RAWINPUT)
                             printf("TEXTRANGE_RAWINPUT\n");
      else if (type==nsIPrivateTextRange::TEXTRANGE_SELECTEDRAWTEXT)
                                  printf("TEXTRANGE_SELECTEDRAWTEXT\n");
      else if (type==nsIPrivateTextRange::TEXTRANGE_CONVERTEDTEXT)
                                  printf("TEXTRANGE_CONVERTEDTEXT\n");
      else if (type==nsIPrivateTextRange::TEXTRANGE_SELECTEDCONVERTEDTEXT)
                                  printf("TEXTRANGE_SELECTEDCONVERTEDTEXT\n");
      else if (type==nsIPrivateTextRange::TEXTRANGE_CARETPOSITION)
                                  printf("TEXTRANGE_CARETPOSITION\n");
      else printf("unknown constant\n");
    }
#endif
        
    //
    // run through the text range list, if any
    //
    nsCOMPtr<nsISelectionController> selCon;
    mEditor->GetSelectionController(getter_AddRefs(selCon));
    NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

    PRUint16      textRangeListLength,selectionStart,selectionEnd,
                  textRangeType;
    
    textRangeListLength = mRangeList->GetLength();
    nsCOMPtr<nsISelection> selection;
    result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if(NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
      result = selPriv->StartBatchChanges();
      if (NS_SUCCEEDED(result))
      {
        nsCOMPtr<nsISelection> imeSel;
        for(PRInt8 selIdx = 0; selIdx < 4;selIdx++)
        {
          result = selCon->GetSelection(sel[selIdx], getter_AddRefs(imeSel));
          if (NS_SUCCEEDED(result))
          {
            result = imeSel->RemoveAllRanges();
            NS_ASSERTION(NS_SUCCEEDED(result), "Cannot ClearSelection");
            // we just ignore the result and clean up the next one here
          }
        }

        nsCOMPtr<nsIPrivateTextRange> textRange;
        bool setCaret=false;
        for(i=0;i<textRangeListLength;i++)
        {
          textRange = mRangeList->Item(i);
          NS_ASSERTION(textRange, "cannot get item");
          if(!textRange)
               break;

          result = textRange->GetRangeType(&textRangeType);
          NS_ASSERTION(NS_SUCCEEDED(result), "cannot get range type");
          if(NS_FAILED(result))
               break;

          result = textRange->GetRangeStart(&selectionStart);
          NS_ASSERTION(NS_SUCCEEDED(result), "cannot get range start");
          if(NS_FAILED(result))
               break;
          result = textRange->GetRangeEnd(&selectionEnd);
          NS_ASSERTION(NS_SUCCEEDED(result), "cannot get range end");
          if(NS_FAILED(result))
               break;

          if(nsIPrivateTextRange::TEXTRANGE_CARETPOSITION == textRangeType)
          {
             NS_ASSERTION(selectionStart == selectionEnd,
                          "nsEditor doesn't support wide caret");
             // Set the caret....
             result = selection->Collapse(mElement,
                      mOffset+selectionStart);
             NS_ASSERTION(NS_SUCCEEDED(result), "Cannot Collapse");
             if(NS_SUCCEEDED(result))
             setCaret = true;
          } else {
             // NS_ASSERTION(selectionStart != selectionEnd, "end == start");
             if(selectionStart == selectionEnd)
                continue;

             result= selCon->GetSelection(TextRangeToSelection(textRangeType),
                     getter_AddRefs(imeSel));
             NS_ASSERTION(NS_SUCCEEDED(result), "Cannot get selction");
             if(NS_FAILED(result))
                break;

             nsRefPtr<nsRange> newRange = new nsRange();
             result = newRange->SetStart(mElement,mOffset+selectionStart);
             NS_ASSERTION(NS_SUCCEEDED(result), "Cannot SetStart");
             if(NS_FAILED(result))
                break;

             result = newRange->SetEnd(mElement,mOffset+selectionEnd);
             NS_ASSERTION(NS_SUCCEEDED(result), "Cannot SetEnd");
             if(NS_FAILED(result))
                break;

             result = imeSel->AddRange(newRange);
             NS_ASSERTION(NS_SUCCEEDED(result), "Cannot AddRange");
             if(NS_FAILED(result))
                break;

             nsCOMPtr<nsISelectionPrivate> imeSelPriv(
                                             do_QueryInterface(imeSel));
             if (imeSelPriv) {
               nsTextRangeStyle textRangeStyle;
               result = textRange->GetRangeStyle(&textRangeStyle);
               NS_ASSERTION(NS_SUCCEEDED(result),
                            "nsIPrivateTextRange::GetRangeStyle failed");
               if (NS_FAILED(result))
                 break;
               result = imeSelPriv->SetTextRangeStyle(newRange, textRangeStyle);
               NS_ASSERTION(NS_SUCCEEDED(result),
                 "nsISelectionPrivate::SetTextRangeStyle failed");
               if (NS_FAILED(result))
                 break;
             } else {
               NS_WARNING("IME selection doesn't have nsISelectionPrivate");
             }
          } // if GetRangeEnd
        } // for textRangeListLength
        if(! setCaret) {
          // set cursor
          result = selection->Collapse(mElement,mOffset+mStringToInsert.Length());
          NS_ASSERTION(NS_SUCCEEDED(result), "Cannot Collapse");
        }
        result = selPriv->EndBatchChanges();
        NS_ASSERTION(NS_SUCCEEDED(result), "Cannot EndBatchChanges");
      } // if StartBatchChanges
    } // if GetSelection

    return result;
}

