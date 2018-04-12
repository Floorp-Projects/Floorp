/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionTransaction.h"

#include "mozilla/EditorBase.h"         // mEditorBase
#include "mozilla/SelectionState.h"     // RangeUpdater
#include "mozilla/TextComposition.h"    // TextComposition
#include "mozilla/dom/Selection.h"      // local var
#include "mozilla/dom/Text.h"           // mTextNode
#include "nsAString.h"                  // params
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsError.h"                    // for NS_SUCCEEDED, NS_FAILED, etc
#include "nsIPresShell.h"               // nsISelectionController constants
#include "nsRange.h"                    // local var
#include "nsQueryObject.h"              // for do_QueryObject

namespace mozilla {

using namespace dom;

// static
already_AddRefed<CompositionTransaction>
CompositionTransaction::Create(EditorBase& aEditorBase,
                               const nsAString& aStringToInsert,
                               Text& aTextNode,
                               uint32_t aOffset)
{
  TextComposition* composition = aEditorBase.GetComposition();
  MOZ_RELEASE_ASSERT(composition);
  // XXX Actually, we get different text node and offset from editor in some
  //     cases.  If composition stores text node, we should use it and offset
  //     in it.
  Text* textNode = composition->GetContainerTextNode();
  uint32_t offset;
  if (textNode) {
    offset = composition->XPOffsetInTextNode();
    NS_WARNING_ASSERTION(&aTextNode == composition->GetContainerTextNode(),
      "The editor tries to insert composition string into different node");
    NS_WARNING_ASSERTION(aOffset == composition->XPOffsetInTextNode(),
      "The editor tries to insert composition string into different offset");
  } else {
    textNode = &aTextNode;
    offset = aOffset;
  }
  RefPtr<CompositionTransaction> transaction =
    new CompositionTransaction(aEditorBase, aStringToInsert,
                               *textNode, offset);
  // XXX Now, it might be better to modify the text node information of
  //     the TextComposition instance in DoTransaction() because updating
  //     the information before changing actual DOM tree is pretty odd.
  composition->OnCreateCompositionTransaction(aStringToInsert,
                                              textNode, offset);
  return transaction.forget();
}

CompositionTransaction::CompositionTransaction(
                          EditorBase& aEditorBase,
                          const nsAString& aStringToInsert,
                          Text& aTextNode,
                          uint32_t aOffset)
  : mTextNode(&aTextNode)
  , mOffset(aOffset)
  , mReplaceLength(aEditorBase.GetComposition()->XPLengthInTextNode())
  , mRanges(aEditorBase.GetComposition()->GetRanges())
  , mStringToInsert(aStringToInsert)
  , mEditorBase(&aEditorBase)
  , mFixed(false)
{
  MOZ_ASSERT(mTextNode->TextLength() >= mOffset);
}

CompositionTransaction::~CompositionTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CompositionTransaction, EditTransactionBase,
                                   mEditorBase,
                                   mTextNode)
// mRangeList can't lead to cycles

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CompositionTransaction)
  if (aIID.Equals(NS_GET_IID(CompositionTransaction))) {
    foundInterface = static_cast<nsITransaction*>(this);
  } else
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMPL_ADDREF_INHERITED(CompositionTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(CompositionTransaction, EditTransactionBase)

NS_IMETHODIMP
CompositionTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Fail before making any changes if there's no selection controller
  nsCOMPtr<nsISelectionController> selCon;
  mEditorBase->GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  // Advance caret: This requires the presentation shell to get the selection.
  if (mReplaceLength == 0) {
    ErrorResult rv;
    mTextNode->InsertData(mOffset, mStringToInsert, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
    mEditorBase->RangeUpdaterRef().
                   SelAdjInsertText(*mTextNode, mOffset, mStringToInsert);
  } else {
    uint32_t replaceableLength = mTextNode->TextLength() - mOffset;
    ErrorResult rv;
    mTextNode->ReplaceData(mOffset, mReplaceLength, mStringToInsert, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
    mEditorBase->RangeUpdaterRef().
                   SelAdjDeleteText(mTextNode, mOffset, mReplaceLength);
    mEditorBase->RangeUpdaterRef().
                   SelAdjInsertText(*mTextNode, mOffset, mStringToInsert);

    // If IME text node is multiple node, ReplaceData doesn't remove all IME
    // text.  So we need remove remained text into other text node.
    if (replaceableLength < mReplaceLength) {
      int32_t remainLength = mReplaceLength - replaceableLength;
      nsCOMPtr<nsINode> node = mTextNode->GetNextSibling();
      while (node && node->IsText() && remainLength > 0) {
        Text* text = static_cast<Text*>(node.get());
        uint32_t textLength = text->TextLength();
        text->DeleteData(0, remainLength, IgnoreErrors());
        mEditorBase->RangeUpdaterRef().SelAdjDeleteText(text, 0, remainLength);
        remainLength -= textLength;
        node = node->GetNextSibling();
      }
    }
  }

  nsresult rv = SetSelectionForRanges();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
CompositionTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get the selection first so we'll fail before making any changes if we
  // can't get it
  RefPtr<Selection> selection = mEditorBase->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  ErrorResult err;
  mTextNode->DeleteData(mOffset, mStringToInsert.Length(), err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // set the selection to the insertion point where the string was removed
  nsresult rv = selection->Collapse(mTextNode, mOffset);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Selection could not be collapsed after undo of IME insert.");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
CompositionTransaction::Merge(nsITransaction* aTransaction,
                              bool* aDidMerge)
{
  NS_ENSURE_ARG_POINTER(aTransaction && aDidMerge);

  // Check to make sure we aren't fixed, if we are then nothing gets absorbed
  if (mFixed) {
    *aDidMerge = false;
    return NS_OK;
  }

  // If aTransaction is another CompositionTransaction then absorb it
  RefPtr<CompositionTransaction> otherTransaction =
    do_QueryObject(aTransaction);
  if (otherTransaction) {
    // We absorb the next IME transaction by adopting its insert string
    mStringToInsert = otherTransaction->mStringToInsert;
    mRanges = otherTransaction->mRanges;
    *aDidMerge = true;
    return NS_OK;
  }

  *aDidMerge = false;
  return NS_OK;
}

void
CompositionTransaction::MarkFixed()
{
  mFixed = true;
}

/* ============ private methods ================== */

nsresult
CompositionTransaction::SetSelectionForRanges()
{
  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return SetIMESelection(*mEditorBase, mTextNode, mOffset,
                         mStringToInsert.Length(), mRanges);
}

// static
nsresult
CompositionTransaction::SetIMESelection(EditorBase& aEditorBase,
                                        Text* aTextNode,
                                        uint32_t aOffsetInNode,
                                        uint32_t aLengthOfCompositionString,
                                        const TextRangeArray* aRanges)
{
  RefPtr<Selection> selection = aEditorBase.GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  SelectionBatcher selectionBatcher(selection);

  // First, remove all selections of IME composition.
  static const RawSelectionType kIMESelections[] = {
    nsISelectionController::SELECTION_IME_RAWINPUT,
    nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
    nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
    nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT
  };

  nsCOMPtr<nsISelectionController> selCon;
  aEditorBase.GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < ArrayLength(kIMESelections); ++i) {
    RefPtr<Selection> selectionOfIME = selCon->GetDOMSelection(kIMESelections[i]);
    if (!selectionOfIME) {
      continue;
    }
    selectionOfIME->RemoveAllRanges(IgnoreErrors());
  }

  // Set caret position and selection of IME composition with TextRangeArray.
  bool setCaret = false;
  uint32_t countOfRanges = aRanges ? aRanges->Length() : 0;

#ifdef DEBUG
  // Bounds-checking on debug builds
  uint32_t maxOffset = aTextNode->Length();
#endif

  // NOTE: composition string may be truncated when it's committed and
  //       maxlength attribute value doesn't allow input of all text of this
  //       composition.
  for (uint32_t i = 0; i < countOfRanges; ++i) {
    const TextRange& textRange = aRanges->ElementAt(i);

    // Caret needs special handling since its length may be 0 and if it's not
    // specified explicitly, we need to handle it ourselves later.
    if (textRange.mRangeType == TextRangeType::eCaret) {
      NS_ASSERTION(!setCaret, "The ranges already has caret position");
      NS_ASSERTION(!textRange.Length(),
                   "EditorBase doesn't support wide caret");
      int32_t caretOffset = static_cast<int32_t>(
        aOffsetInNode +
          std::min(textRange.mStartOffset, aLengthOfCompositionString));
      MOZ_ASSERT(caretOffset >= 0 &&
                 static_cast<uint32_t>(caretOffset) <= maxOffset);
      rv = selection->Collapse(aTextNode, caretOffset);
      setCaret = setCaret || NS_SUCCEEDED(rv);
      if (NS_WARN_IF(!setCaret)) {
        continue;
      }
      // If caret range is specified explicitly, we should show the caret if
      // it should be so.
      aEditorBase.HideCaret(false);
      continue;
    }

    // If the clause length is 0, it should be a bug.
    if (!textRange.Length()) {
      NS_WARNING("Any clauses must not be empty");
      continue;
    }

    RefPtr<nsRange> clauseRange;
    int32_t startOffset = static_cast<int32_t>(
      aOffsetInNode +
        std::min(textRange.mStartOffset, aLengthOfCompositionString));
    MOZ_ASSERT(startOffset >= 0 &&
               static_cast<uint32_t>(startOffset) <= maxOffset);
    int32_t endOffset = static_cast<int32_t>(
      aOffsetInNode +
        std::min(textRange.mEndOffset, aLengthOfCompositionString));
    MOZ_ASSERT(endOffset >= startOffset &&
               static_cast<uint32_t>(endOffset) <= maxOffset);
    rv = nsRange::CreateRange(aTextNode, startOffset,
                              aTextNode, endOffset,
                              getter_AddRefs(clauseRange));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create a DOM range for a clause of composition");
      break;
    }

    // Set the range of the clause to selection.
    RefPtr<Selection> selectionOfIME =
      selCon->GetDOMSelection(ToRawSelectionType(textRange.mRangeType));
    if (!selectionOfIME) {
      NS_WARNING("Failed to get IME selection");
      break;
    }

    IgnoredErrorResult err;
    selectionOfIME->AddRange(*clauseRange, err);
    if (err.Failed()) {
      NS_WARNING("Failed to add selection range for a clause of composition");
      break;
    }

    // Set the style of the clause.
    rv = selectionOfIME->SetTextRangeStyle(clauseRange,
                                           textRange.mRangeStyle);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to set selection style");
      break; // but this is unexpected...
    }
  }

  // If the ranges doesn't include explicit caret position, let's set the
  // caret to the end of composition string.
  if (!setCaret) {
    int32_t caretOffset =
      static_cast<int32_t>(aOffsetInNode + aLengthOfCompositionString);
    MOZ_ASSERT(caretOffset >= 0 &&
               static_cast<uint32_t>(caretOffset) <= maxOffset);
    rv = selection->Collapse(aTextNode, caretOffset);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Failed to set caret at the end of composition string");

    // If caret range isn't specified explicitly, we should hide the caret.
    // Hiding the caret benefits a Windows build (see bug 555642 comment #6).
    // However, when there is no range, we should keep showing caret.
    if (countOfRanges) {
      aEditorBase.HideCaret(true);
    }
  }

  return rv;
}

} // namespace mozilla
