/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMETextTxn.h"

#include "mozilla/dom/Selection.h"      // local var
#include "mozilla/dom/Text.h"           // mTextNode
#include "nsAString.h"                  // params
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsEditor.h"                   // mEditor
#include "nsError.h"                    // for NS_SUCCEEDED, NS_FAILED, etc
#include "nsIPresShell.h"               // nsISelectionController constants
#include "nsRange.h"                    // local var

using namespace mozilla;
using namespace mozilla::dom;

IMETextTxn::IMETextTxn(Text& aTextNode, uint32_t aOffset,
                       uint32_t aReplaceLength,
                       TextRangeArray* aTextRangeArray,
                       const nsAString& aStringToInsert,
                       nsEditor& aEditor)
  : EditTxn()
  , mTextNode(&aTextNode)
  , mOffset(aOffset)
  , mReplaceLength(aReplaceLength)
  , mRanges(aTextRangeArray)
  , mStringToInsert(aStringToInsert)
  , mEditor(aEditor)
  , mFixed(false)
{
}

IMETextTxn::~IMETextTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IMETextTxn, EditTxn,
                                   mTextNode)
// mRangeList can't lead to cycles

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IMETextTxn)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsITransaction, IMETextTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMPL_ADDREF_INHERITED(IMETextTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(IMETextTxn, EditTxn)

NS_IMETHODIMP
IMETextTxn::DoTransaction()
{
  // Fail before making any changes if there's no selection controller
  nsCOMPtr<nsISelectionController> selCon;
  mEditor.GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  // Advance caret: This requires the presentation shell to get the selection.
  nsresult res;
  if (mReplaceLength == 0) {
    res = mTextNode->InsertData(mOffset, mStringToInsert);
  } else {
    res = mTextNode->ReplaceData(mOffset, mReplaceLength, mStringToInsert);
  }
  NS_ENSURE_SUCCESS(res, res);

  res = SetSelectionForRanges();
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

NS_IMETHODIMP
IMETextTxn::UndoTransaction()
{
  // Get the selection first so we'll fail before making any changes if we
  // can't get it
  nsRefPtr<Selection> selection = mEditor.GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  nsresult res = mTextNode->DeleteData(mOffset, mStringToInsert.Length());
  NS_ENSURE_SUCCESS(res, res);

  // set the selection to the insertion point where the string was removed
  res = selection->Collapse(mTextNode, mOffset);
  NS_ASSERTION(NS_SUCCEEDED(res),
               "Selection could not be collapsed after undo of IME insert.");
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

NS_IMETHODIMP
IMETextTxn::Merge(nsITransaction* aTransaction, bool* aDidMerge)
{
  NS_ENSURE_ARG_POINTER(aTransaction && aDidMerge);

  // Check to make sure we aren't fixed, if we are then nothing gets absorbed
  if (mFixed) {
    *aDidMerge = false;
    return NS_OK;
  }

  // If aTransaction is another IMETextTxn then absorb it
  nsRefPtr<IMETextTxn> otherTxn = do_QueryObject(aTransaction);
  if (otherTxn) {
    // We absorb the next IME transaction by adopting its insert string
    mStringToInsert = otherTxn->mStringToInsert;
    mRanges = otherTxn->mRanges;
    *aDidMerge = true;
    return NS_OK;
  }

  *aDidMerge = false;
  return NS_OK;
}

void
IMETextTxn::MarkFixed()
{
  mFixed = true;
}

NS_IMETHODIMP
IMETextTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("IMETextTxn: ");
  aString += mStringToInsert;
  return NS_OK;
}

/* ============ private methods ================== */
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
  nsRefPtr<Selection> selection = mEditor.GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = selection->StartBatchChanges();
  NS_ENSURE_SUCCESS(rv, rv);

  // First, remove all selections of IME composition.
  static const SelectionType kIMESelections[] = {
    nsISelectionController::SELECTION_IME_RAWINPUT,
    nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
    nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
    nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT
  };

  nsCOMPtr<nsISelectionController> selCon;
  mEditor.GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  for (uint32_t i = 0; i < ArrayLength(kIMESelections); ++i) {
    nsCOMPtr<nsISelection> selectionOfIME;
    if (NS_FAILED(selCon->GetSelection(kIMESelections[i],
                                       getter_AddRefs(selectionOfIME)))) {
      continue;
    }
    rv = selectionOfIME->RemoveAllRanges();
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Failed to remove all ranges of IME selection");
  }

  // Set caret position and selection of IME composition with TextRangeArray.
  bool setCaret = false;
  uint32_t countOfRanges = mRanges ? mRanges->Length() : 0;

#ifdef DEBUG
  // Bounds-checking on debug builds
  uint32_t maxOffset = mTextNode->Length();
#endif

  // The mStringToInsert may be truncated if maxlength attribute value doesn't
  // allow input of all text of this composition. So, we can get actual length
  // of the inserted string from it.
  uint32_t insertedLength = mStringToInsert.Length();
  for (uint32_t i = 0; i < countOfRanges; ++i) {
    const TextRange& textRange = mRanges->ElementAt(i);

    // Caret needs special handling since its length may be 0 and if it's not
    // specified explicitly, we need to handle it ourselves later.
    if (textRange.mRangeType == NS_TEXTRANGE_CARETPOSITION) {
      NS_ASSERTION(!setCaret, "The ranges already has caret position");
      NS_ASSERTION(!textRange.Length(), "nsEditor doesn't support wide caret");
      int32_t caretOffset = static_cast<int32_t>(
        mOffset + std::min(textRange.mStartOffset, insertedLength));
      MOZ_ASSERT(caretOffset >= 0 &&
                 static_cast<uint32_t>(caretOffset) <= maxOffset);
      rv = selection->Collapse(mTextNode, caretOffset);
      setCaret = setCaret || NS_SUCCEEDED(rv);
      NS_ASSERTION(setCaret, "Failed to collapse normal selection");
      continue;
    }

    // If the clause length is 0, it should be a bug.
    if (!textRange.Length()) {
      NS_WARNING("Any clauses must not be empty");
      continue;
    }

    nsRefPtr<nsRange> clauseRange;
    int32_t startOffset = static_cast<int32_t>(
      mOffset + std::min(textRange.mStartOffset, insertedLength));
    MOZ_ASSERT(startOffset >= 0 &&
               static_cast<uint32_t>(startOffset) <= maxOffset);
    int32_t endOffset = static_cast<int32_t>(
      mOffset + std::min(textRange.mEndOffset, insertedLength));
    MOZ_ASSERT(endOffset >= startOffset &&
               static_cast<uint32_t>(endOffset) <= maxOffset);
    rv = nsRange::CreateRange(mTextNode, startOffset,
                              mTextNode, endOffset,
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
    int32_t caretOffset = static_cast<int32_t>(mOffset + insertedLength);
    MOZ_ASSERT(caretOffset >= 0 &&
               static_cast<uint32_t>(caretOffset) <= maxOffset);
    rv = selection->Collapse(mTextNode, caretOffset);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Failed to set caret at the end of composition string");
  }

  rv = selection->EndBatchChanges();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to end batch changes");

  return rv;
}
