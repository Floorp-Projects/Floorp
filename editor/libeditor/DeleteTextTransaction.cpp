/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteTextTransaction.h"

#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/Selection.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

// static
already_AddRefed<DeleteTextTransaction> DeleteTextTransaction::MaybeCreate(
    EditorBase& aEditorBase, Text& aTextNode, uint32_t aOffset,
    uint32_t aLengthToDelete) {
  RefPtr<DeleteTextTransaction> transaction = new DeleteTextTransaction(
      aEditorBase, aTextNode, aOffset, aLengthToDelete);
  return transaction.forget();
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForPreviousCharacter(EditorBase& aEditorBase,
                                                       Text& aTextNode,
                                                       uint32_t aOffset) {
  if (NS_WARN_IF(!aOffset)) {
    return nullptr;
  }

  nsAutoString data;
  aTextNode.GetData(data);
  if (NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  uint32_t offset = aOffset - 1;
  if (offset && NS_IS_SURROGATE_PAIR(data[offset - 1], data[offset])) {
    ++length;
    --offset;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aTextNode, offset,
                                            length);
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForNextCharacter(EditorBase& aEditorBase,
                                                   Text& aTextNode,
                                                   uint32_t aOffset) {
  nsAutoString data;
  aTextNode.GetData(data);
  if (NS_WARN_IF(aOffset >= data.Length()) || NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  if (aOffset + 1 < data.Length() &&
      NS_IS_SURROGATE_PAIR(data[aOffset], data[aOffset + 1])) {
    ++length;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aTextNode, aOffset,
                                            length);
}

DeleteTextTransaction::DeleteTextTransaction(EditorBase& aEditorBase,
                                             Text& aTextNode, uint32_t aOffset,
                                             uint32_t aLengthToDelete)
    : mEditorBase(&aEditorBase),
      mTextNode(&aTextNode),
      mOffset(aOffset),
      mLengthToDelete(aLengthToDelete) {
  NS_ASSERTION(mTextNode->Length() >= aOffset + aLengthToDelete,
               "Trying to delete more characters than in node");
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteTextTransaction, EditTransactionBase,
                                   mEditorBase, mTextNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

bool DeleteTextTransaction::CanDoIt() const {
  if (NS_WARN_IF(!mTextNode) || NS_WARN_IF(!mEditorBase)) {
    return false;
  }
  return mEditorBase->IsModifiableNode(*mTextNode);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
DeleteTextTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the text that we're about to delete
  ErrorResult error;
  mTextNode->SubstringData(mOffset, mLengthToDelete, mDeletedText, error);
  if (error.Failed()) {
    NS_WARNING("Text::SubstringData() failed");
    return error.StealNSResult();
  }

  RefPtr<EditorBase> editorBase = mEditorBase;
  RefPtr<Text> textNode = mTextNode;
  editorBase->DoDeleteText(*textNode, mOffset, mLengthToDelete, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoDeleteText() failed");
    return error.StealNSResult();
  }

  DebugOnly<nsresult> rvIgnored =
      editorBase->RangeUpdaterRef().SelAdjDeleteText(*textNode, mOffset,
                                                     mLengthToDelete);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "RangeUpdater::SelAdjDeleteText() failed, but ignored");

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  selection->Collapse(EditorRawDOMPoint(textNode, mOffset), error);
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::Collapse() failed");
  return error.StealNSResult();
}

// XXX: We may want to store the selection state and restore it properly.  Was
//     it an insertion point or an extended selection?
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
DeleteTextTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mTextNode) || NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RefPtr<EditorBase> editorBase = mEditorBase;
  RefPtr<Text> textNode = mTextNode;
  ErrorResult error;
  editorBase->DoInsertText(*textNode, mOffset, mDeletedText, error);
  NS_WARNING_ASSERTION(!error.Failed(), "EditorBase::DoInsertText() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
