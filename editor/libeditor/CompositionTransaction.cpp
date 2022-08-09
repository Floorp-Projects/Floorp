/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositionTransaction.h"

#include "mozilla/EditorBase.h"  // mEditorBase
#include "mozilla/Logging.h"
#include "mozilla/SelectionState.h"   // RangeUpdater
#include "mozilla/TextComposition.h"  // TextComposition
#include "mozilla/ToString.h"
#include "mozilla/dom/Selection.h"   // local var
#include "mozilla/dom/Text.h"        // mTextNode
#include "nsAString.h"               // params
#include "nsDebug.h"                 // for NS_ASSERTION, etc
#include "nsError.h"                 // for NS_SUCCEEDED, NS_FAILED, etc
#include "nsRange.h"                 // local var
#include "nsISelectionController.h"  // for nsISelectionController constants
#include "nsQueryObject.h"           // for do_QueryObject

namespace mozilla {

using namespace dom;

// static
already_AddRefed<CompositionTransaction> CompositionTransaction::Create(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());

  TextComposition* composition = aEditorBase.GetComposition();
  MOZ_RELEASE_ASSERT(composition);
  // XXX Actually, we get different text node and offset from editor in some
  //     cases.  If composition stores text node, we should use it and offset
  //     in it.
  EditorDOMPointInText pointToInsert;
  if (Text* textNode = composition->GetContainerTextNode()) {
    pointToInsert.Set(textNode, composition->XPOffsetInTextNode());
    NS_WARNING_ASSERTION(
        pointToInsert.GetContainerAs<Text>() ==
            composition->GetContainerTextNode(),
        "The editor tries to insert composition string into different node");
    NS_WARNING_ASSERTION(
        pointToInsert.Offset() == composition->XPOffsetInTextNode(),
        "The editor tries to insert composition string into different offset");
  } else {
    pointToInsert = aPointToInsert;
  }
  RefPtr<CompositionTransaction> transaction =
      new CompositionTransaction(aEditorBase, aStringToInsert, pointToInsert);
  return transaction.forget();
}

CompositionTransaction::CompositionTransaction(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert)
    : mTextNode(aPointToInsert.ContainerAs<Text>()),
      mOffset(aPointToInsert.Offset()),
      mReplaceLength(aEditorBase.GetComposition()->XPLengthInTextNode()),
      mRanges(aEditorBase.GetComposition()->GetRanges()),
      mStringToInsert(aStringToInsert),
      mEditorBase(&aEditorBase),
      mFixed(false) {
  MOZ_ASSERT(mTextNode->TextLength() >= mOffset);
}

std::ostream& operator<<(std::ostream& aStream,
                         const CompositionTransaction& aTransaction) {
  aStream << "{ mTextNode=" << aTransaction.mTextNode.get();
  if (aTransaction.mTextNode) {
    aStream << " (" << *aTransaction.mTextNode << ")";
  }
  aStream << ", mOffset=" << aTransaction.mOffset
          << ", mReplaceLength=" << aTransaction.mReplaceLength
          << ", mRanges={ Length()=" << aTransaction.mRanges->Length() << " }"
          << ", mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CompositionTransaction, EditTransactionBase,
                                   mEditorBase, mTextNode)
// mRangeList can't lead to cycles

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CompositionTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)
NS_IMPL_ADDREF_INHERITED(CompositionTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(CompositionTransaction, EditTransactionBase)

NS_IMETHODIMP CompositionTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p CompositionTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Fail before making any changes if there's no selection controller
  if (NS_WARN_IF(!mEditorBase->GetSelectionController())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;

  // Advance caret: This requires the presentation shell to get the selection.
  if (mReplaceLength == 0) {
    ErrorResult error;
    editorBase->DoInsertText(textNode, mOffset, mStringToInsert, error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoInsertText() failed");
      return error.StealNSResult();
    }
    editorBase->RangeUpdaterRef().SelAdjInsertText(textNode, mOffset,
                                                   mStringToInsert.Length());
  } else {
    // If composition string is split to multiple text nodes, we should put
    // whole new composition string to the first text node and remove the
    // compostion string in other nodes.
    // TODO: This should be handled by `TextComposition` because this assumes
    //       that composition string has never touched by JS.  However, it
    //       would occur if the web app is a corrabolation software which
    //       multiple users can modify anyware in an editor.
    // TODO: And if composition starts from a following text node, the offset
    //       here is outdated and it will cause inserting composition string
    //       **before** the proper point from point of view of the users.
    uint32_t replaceableLength = textNode->TextLength() - mOffset;
    ErrorResult error;
    editorBase->DoReplaceText(textNode, mOffset, mReplaceLength,
                              mStringToInsert, error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoReplaceText() failed");
      return error.StealNSResult();
    }

    // Don't use RangeUpdaterRef().SelAdjReplaceText() here because undoing
    // this transaction will remove whole composition string.  Therefore,
    // selection should be restored at start of composition string.
    // XXX Perhaps, this is a bug of our selection managemnt at undoing.
    editorBase->RangeUpdaterRef().SelAdjDeleteText(textNode, mOffset,
                                                   replaceableLength);
    // But some ranges which after the composition string should be restored
    // as-is.
    editorBase->RangeUpdaterRef().SelAdjInsertText(textNode, mOffset,
                                                   mStringToInsert.Length());

    if (replaceableLength < mReplaceLength) {
      // XXX Perhaps, scanning following sibling text nodes with composition
      //     string length which we know is wrong because there may be
      //     non-empty text nodes which are inserted by JS.  Instead, we
      //     should remove all text in the ranges of IME selections.
      uint32_t remainLength = mReplaceLength - replaceableLength;
      IgnoredErrorResult ignoredError;
      for (nsIContent* nextSibling = textNode->GetNextSibling();
           nextSibling && nextSibling->IsText() && remainLength;
           nextSibling = nextSibling->GetNextSibling()) {
        OwningNonNull<Text> followingTextNode =
            *static_cast<Text*>(nextSibling);
        uint32_t textLength = followingTextNode->TextLength();
        editorBase->DoDeleteText(followingTextNode, 0, remainLength,
                                 ignoredError);
        NS_WARNING_ASSERTION(!ignoredError.Failed(),
                             "EditorBase::DoDeleteText() failed, but ignored");
        ignoredError.SuppressException();
        // XXX Needs to check whether the text is deleted as expected.
        editorBase->RangeUpdaterRef().SelAdjDeleteText(followingTextNode, 0,
                                                       remainLength);
        remainLength -= textLength;
      }
    }
  }

  nsresult rv = SetSelectionForRanges();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "CompositionTransaction::SetSelectionForRanges() failed");

  if (TextComposition* composition = editorBase->GetComposition()) {
    composition->OnUpdateCompositionInEditor(mStringToInsert, textNode,
                                             mOffset);
  }

  return rv;
}

NS_IMETHODIMP CompositionTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p CompositionTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (MOZ_UNLIKELY(NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;
  IgnoredErrorResult error;
  editorBase->DoDeleteText(textNode, mOffset, mStringToInsert.Length(), error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("EditorBase::DoDeleteText() failed");
    return error.StealNSResult();
  }

  // set the selection to the insertion point where the string was removed
  editorBase->CollapseSelectionTo(EditorRawDOMPoint(textNode, mOffset), error);
  NS_ASSERTION(!error.Failed(), "EditorBase::CollapseSelectionTo() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP CompositionTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p CompositionTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));
  return DoTransaction();
}

NS_IMETHODIMP CompositionTransaction::Merge(nsITransaction* aOtherTransaction,
                                            bool* aDidMerge) {
  MOZ_LOG(GetLogModule(), LogLevel::Debug,
          ("%p CompositionTransaction::%s(aOtherTransaction=%p) this=%s", this,
           __FUNCTION__, aOtherTransaction, ToString(*this).c_str()));

  if (NS_WARN_IF(!aOtherTransaction) || NS_WARN_IF(!aDidMerge)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aDidMerge = false;

  // Check to make sure we aren't fixed, if we are then nothing gets merged.
  if (mFixed) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p CompositionTransaction::%s returned false due to fixed", this,
             __FUNCTION__));
    return NS_OK;
  }

  RefPtr<EditTransactionBase> otherTransactionBase =
      aOtherTransaction->GetAsEditTransactionBase();
  if (!otherTransactionBase) {
    MOZ_LOG(GetLogModule(), LogLevel::Debug,
            ("%p CompositionTransaction::%s returned false due to not edit "
             "transaction",
             this, __FUNCTION__));
    return NS_OK;
  }

  // If aTransaction is another CompositionTransaction then merge it
  CompositionTransaction* otherCompositionTransaction =
      otherTransactionBase->GetAsCompositionTransaction();
  if (!otherCompositionTransaction) {
    return NS_OK;
  }

  // We merge the next IME transaction by adopting its insert string.
  mStringToInsert = otherCompositionTransaction->mStringToInsert;
  mRanges = otherCompositionTransaction->mRanges;
  *aDidMerge = true;
  MOZ_LOG(GetLogModule(), LogLevel::Debug,
          ("%p CompositionTransaction::%s returned true", this, __FUNCTION__));
  return NS_OK;
}

void CompositionTransaction::MarkFixed() { mFixed = true; }

/* ============ private methods ================== */

nsresult CompositionTransaction::SetSelectionForRanges() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;
  RefPtr<TextRangeArray> ranges = mRanges;
  nsresult rv = SetIMESelection(editorBase, textNode, mOffset,
                                mStringToInsert.Length(), ranges);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "CompositionTransaction::SetIMESelection() failed");
  return rv;
}

// static
nsresult CompositionTransaction::SetIMESelection(
    EditorBase& aEditorBase, Text* aTextNode, uint32_t aOffsetInNode,
    uint32_t aLengthOfCompositionString, const TextRangeArray* aRanges) {
  RefPtr<Selection> selection = aEditorBase.GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  SelectionBatcher selectionBatcher(selection, __FUNCTION__);

  // First, remove all selections of IME composition.
  static const RawSelectionType kIMESelections[] = {
      nsISelectionController::SELECTION_IME_RAWINPUT,
      nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT,
      nsISelectionController::SELECTION_IME_CONVERTEDTEXT,
      nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT};

  nsCOMPtr<nsISelectionController> selectionController =
      aEditorBase.GetSelectionController();
  if (NS_WARN_IF(!selectionController)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  IgnoredErrorResult ignoredError;
  for (uint32_t i = 0; i < ArrayLength(kIMESelections); ++i) {
    RefPtr<Selection> selectionOfIME =
        selectionController->GetSelection(kIMESelections[i]);
    if (!selectionOfIME) {
      NS_WARNING("nsISelectionController::GetSelection() failed");
      continue;
    }
    selectionOfIME->RemoveAllRanges(ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Selection::RemoveAllRanges() failed, but ignored");
    ignoredError.SuppressException();
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
  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < countOfRanges; ++i) {
    const TextRange& textRange = aRanges->ElementAt(i);

    // Caret needs special handling since its length may be 0 and if it's not
    // specified explicitly, we need to handle it ourselves later.
    if (textRange.mRangeType == TextRangeType::eCaret) {
      NS_ASSERTION(!setCaret, "The ranges already has caret position");
      NS_ASSERTION(!textRange.Length(),
                   "EditorBase doesn't support wide caret");
      CheckedUint32 caretOffset(aOffsetInNode);
      caretOffset +=
          std::min(textRange.mStartOffset, aLengthOfCompositionString);
      MOZ_ASSERT(caretOffset.isValid());
      MOZ_ASSERT(caretOffset.value() <= maxOffset);
      rv = selection->CollapseInLimiter(aTextNode, caretOffset.value());
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Selection::CollapseInLimiter() failed, but might be ignored");
      setCaret = setCaret || NS_SUCCEEDED(rv);
      if (!setCaret) {
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
    CheckedUint32 startOffset = aOffsetInNode;
    startOffset += std::min(textRange.mStartOffset, aLengthOfCompositionString);
    MOZ_ASSERT(startOffset.isValid());
    MOZ_ASSERT(startOffset.value() <= maxOffset);
    CheckedUint32 endOffset = aOffsetInNode;
    endOffset += std::min(textRange.mEndOffset, aLengthOfCompositionString);
    MOZ_ASSERT(endOffset.isValid());
    MOZ_ASSERT(endOffset.value() >= startOffset.value());
    MOZ_ASSERT(endOffset.value() <= maxOffset);
    clauseRange = nsRange::Create(aTextNode, startOffset.value(), aTextNode,
                                  endOffset.value(), IgnoreErrors());
    if (!clauseRange) {
      NS_WARNING("nsRange::Create() failed, but might be ignored");
      break;
    }

    // Set the range of the clause to selection.
    RefPtr<Selection> selectionOfIME = selectionController->GetSelection(
        ToRawSelectionType(textRange.mRangeType));
    if (!selectionOfIME) {
      NS_WARNING(
          "nsISelectionController::GetSelection() failed, but might be "
          "ignored");
      break;
    }

    IgnoredErrorResult ignoredError;
    selectionOfIME->AddRangeAndSelectFramesAndNotifyListeners(*clauseRange,
                                                              ignoredError);
    if (ignoredError.Failed()) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed, but "
          "might be ignored");
      break;
    }

    // Set the style of the clause.
    rv = selectionOfIME->SetTextRangeStyle(clauseRange, textRange.mRangeStyle);
    if (NS_FAILED(rv)) {
      NS_WARNING("Selection::SetTextRangeStyle() failed, but might be ignored");
      break;  // but this is unexpected...
    }
  }

  // If the ranges doesn't include explicit caret position, let's set the
  // caret to the end of composition string.
  if (!setCaret) {
    CheckedUint32 caretOffset = aOffsetInNode;
    caretOffset += aLengthOfCompositionString;
    MOZ_ASSERT(caretOffset.isValid());
    MOZ_ASSERT(caretOffset.value() <= maxOffset);
    rv = selection->CollapseInLimiter(aTextNode, caretOffset.value());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Selection::CollapseInLimiter() failed");

    // If caret range isn't specified explicitly, we should hide the caret.
    // Hiding the caret benefits a Windows build (see bug 555642 comment #6).
    // However, when there is no range, we should keep showing caret.
    if (countOfRanges) {
      aEditorBase.HideCaret(true);
    }
  }

  return rv;
}

}  // namespace mozilla
