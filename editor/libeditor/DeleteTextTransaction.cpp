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
#include "nsIEditor.h"
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
  if (offset && NS_IS_LOW_SURROGATE(data[offset]) &&
      NS_IS_HIGH_SURROGATE(data[offset - 1])) {
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
  if (aOffset + 1 < data.Length() && NS_IS_HIGH_SURROGATE(data[aOffset]) &&
      NS_IS_LOW_SURROGATE(data[aOffset + 1])) {
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

MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
DeleteTextTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the text that we're about to delete
  ErrorResult err;
  mTextNode->SubstringData(mOffset, mLengthToDelete, mDeletedText, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  RefPtr<EditorBase> editorBase = mEditorBase;
  RefPtr<Text> textNode = mTextNode;
  editorBase->DoDeleteText(*textNode, mOffset, mLengthToDelete, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  editorBase->RangeUpdaterRef().SelAdjDeleteText(textNode, mOffset,
                                                 mLengthToDelete);

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult error;
  selection->Collapse(EditorRawDOMPoint(textNode, mOffset), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

// XXX: We may want to store the selection state and restore it properly.  Was
//     it an insertion point or an extended selection?
MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
DeleteTextTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mTextNode) || NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  RefPtr<EditorBase> editorBase = mEditorBase;
  RefPtr<Text> textNode = mTextNode;
  ErrorResult error;
  editorBase->DoInsertText(*textNode, mOffset, mDeletedText, error);
  return error.StealNSResult();
}

}  // namespace mozilla
