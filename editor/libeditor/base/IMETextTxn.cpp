/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMETextTxn.h"
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/mozalloc.h"           // for operator new
#include "mozilla/TextEvents.h"         // for TextRangeStyle
#include "nsAString.h"                  // for nsAString_internal::Length, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsError.h"                    // for NS_SUCCEEDED, NS_FAILED, etc
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMRange.h"                // for nsRange::SetEnd, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIPresShell.h"               // for SelectionType
#include "nsISelection.h"               // for nsISelection
#include "nsISelectionController.h"     // for nsISelectionController, etc
#include "nsISelectionPrivate.h"        // for nsISelectionPrivate
#include "nsISupportsImpl.h"            // for nsRange::AddRef, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF_THIS, NS_RELEASE
#include "nsITransaction.h"             // for nsITransaction
#include "nsRange.h"                    // for nsRange
#include "nsString.h"                   // for nsString

using namespace mozilla;

// #define DEBUG_IMETXN

IMETextTxn::IMETextTxn()
  : EditTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IMETextTxn, EditTxn,
                                   mElement)
// mRangeList can't lead to cycles

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMETextTxn)
  if (aIID.Equals(IMETextTxn::GetCID())) {
    *aInstancePtr = (void*)(IMETextTxn*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP IMETextTxn::Init(nsIDOMCharacterData     *aElement,
                               uint32_t                 aOffset,
                               uint32_t                 aReplaceLength,
                               TextRangeArray          *aTextRangeArray,
                               const nsAString         &aStringToInsert,
                               nsIEditor               *aEditor)
{
  NS_ENSURE_ARG_POINTER(aElement);
  mElement = aElement;
  mOffset = aOffset;
  mReplaceLength = aReplaceLength;
  mStringToInsert = aStringToInsert;
  mEditor = aEditor;
  mRanges = aTextRangeArray;
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
    result = SetSelectionForRanges();
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
  IMETextTxn*  otherTxn = nullptr;
  nsresult result = aTransaction->QueryInterface(IMETextTxn::GetCID(),(void**)&otherTxn);
  if (otherTxn && NS_SUCCEEDED(result))
  {
    //
    //  we absorb the next IME transaction by adopting its insert string as our own
    //
    mStringToInsert = otherTxn->mStringToInsert;
    mRanges = otherTxn->mRanges;
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
static SelectionType
ToSelectionType(uint32_t aTextRangeType)
{
  switch(aTextRangeType) {
    case NS_TEXTRANGE_RAWINPUT:
      return nsISelectionController::SELECTION_IME_RAWINPUT;
    case NS_TEXTRANGE_SELECTEDRAWTEXT:
      return nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT;
    case NS_TEXTRANGE_CONVERTEDTEXT:
      return nsISelectionController::SELECTION_IME_CONVERTEDTEXT;
    case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT:
      return nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT;
    default:
      MOZ_CRASH("Selection type is invalid");
      return nsISelectionController::SELECTION_NORMAL;
  }
}

nsresult
IMETextTxn::SetSelectionForRanges()
{
  nsCOMPtr<nsISelectionController> selCon;
  mEditor->GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelection> selection;
  nsresult rv =
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                         getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  rv = selPriv->StartBatchChanges();
  NS_ENSURE_SUCCESS(rv, rv);

  // First, remove all selections of IME composition.
  static const SelectionType kIMESelections[] = {
    nsISelectionController::SELECTION_IME_RAWINPUT,
    nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
    nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
    nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT
  };
  for (uint32_t i = 0; i < ArrayLength(kIMESelections); ++i) {
    nsCOMPtr<nsISelection> selectionOfIME;
    if (NS_FAILED(selCon->GetSelection(kIMESelections[i],
                                       getter_AddRefs(selectionOfIME)))) {
      continue;
    }
    DebugOnly<nsresult> rv = selectionOfIME->RemoveAllRanges();
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Failed to remove all ranges of IME selection");
  }

  // Set caret position and selection of IME composition with TextRangeArray.
  bool setCaret = false;
  uint32_t countOfRanges = mRanges ? mRanges->Length() : 0;
  for (uint32_t i = 0; i < countOfRanges; ++i) {
    const TextRange& textRange = mRanges->ElementAt(i);

    // Caret needs special handling since its length may be 0 and if it's not
    // specified explicitly, we need to handle it ourselves later.
    if (textRange.mRangeType == NS_TEXTRANGE_CARETPOSITION) {
      NS_ASSERTION(!setCaret, "The ranges already has caret position");
      NS_ASSERTION(!textRange.Length(), "nsEditor doesn't support wide caret");
      // NOTE: If the caret position is larger than max length of the editor
      //       content, this may fail.
      rv = selection->Collapse(mElement, mOffset + textRange.mStartOffset);
      setCaret = setCaret || NS_SUCCEEDED(rv);
      NS_ASSERTION(setCaret, "Failed to collapse normal selection");
      continue;
    }

    // If the clause length is 0, it's should be a bug.
    if (!textRange.Length()) {
      NS_WARNING("Any clauses must not be empty");
      continue;
    }

    nsRefPtr<nsRange> clauseRange;
    rv = nsRange::CreateRange(mElement, mOffset + textRange.mStartOffset,
                              mElement, mOffset + textRange.mEndOffset,
                              getter_AddRefs(clauseRange));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create a DOM range for a clause of composition");
      break;
    }

    // Set the range of the clause to selection.
    nsCOMPtr<nsISelection> selectionOfIME;
    rv = selCon->GetSelection(ToSelectionType(textRange.mRangeType),
                              getter_AddRefs(selectionOfIME));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get IME selection");
      break;
    }

    rv = selectionOfIME->AddRange(clauseRange);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add selection range for a clause of composition");
      break;
    }

    // Set the style of the clause.
    nsCOMPtr<nsISelectionPrivate> selectionOfIMEPriv =
                                    do_QueryInterface(selectionOfIME);
    if (!selectionOfIMEPriv) {
      NS_WARNING("Failed to get nsISelectionPrivate interface from selection");
      continue; // Since this is additional feature, we can continue this job.
    }
    rv = selectionOfIMEPriv->SetTextRangeStyle(clauseRange,
                                               textRange.mRangeStyle);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to set selection style");
      break; // but this is unexpected...
    }
  }

  // If the ranges doesn't include explicit caret position, let's set the
  // caret to the end of composition string.
  if (!setCaret) {
    rv = selection->Collapse(mElement, mOffset + mStringToInsert.Length());
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Failed to set caret at the end of composition string");
  }

  rv = selPriv->EndBatchChanges();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to end batch changes");

  return rv;
}

