/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReplaceTextTransaction.h"

#include "HTMLEditUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/ToString.h"

namespace mozilla {

using namespace dom;

std::ostream& operator<<(std::ostream& aStream,
                         const ReplaceTextTransaction& aTransaction) {
  aStream << "{ mTextNode=" << aTransaction.mTextNode.get();
  if (aTransaction.mTextNode) {
    aStream << " (" << *aTransaction.mTextNode << ")";
  }
  aStream << ", mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mStringToBeReplaced=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToBeReplaced).get()
          << "\", mOffset=" << aTransaction.mOffset
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ReplaceTextTransaction, EditTransactionBase,
                                   mEditorBase, mTextNode)

NS_IMPL_ADDREF_INHERITED(ReplaceTextTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ReplaceTextTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReplaceTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP ReplaceTextTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode) ||
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*mTextNode))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;

  ErrorResult error;
  editorBase->DoReplaceText(textNode, mOffset, mStringToBeReplaced.Length(),
                            mStringToInsert, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(textNode, mOffset,
                                                  mStringToBeReplaced.Length(),
                                                  mStringToInsert.Length());

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  // XXX Should we stop setting selection when mutation event listener
  //     modifies the text node?
  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored = selection->CollapseInLimiter(
      textNode, mOffset + mStringToInsert.Length());
  if (NS_WARN_IF(editorBase->Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(NS_SUCCEEDED(rvIgnored),
               "Selection::CollapseInLimiter() failed, but ignored");
  return NS_OK;
}

NS_IMETHODIMP ReplaceTextTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode) ||
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*mTextNode))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ErrorResult error;
  nsAutoString insertedString;
  mTextNode->SubstringData(mOffset, mStringToInsert.Length(), insertedString,
                           error);
  if (error.Failed()) {
    NS_WARNING("CharacterData::SubstringData() failed");
    return error.StealNSResult();
  }
  if (insertedString != mStringToInsert) {
    NS_WARNING(
        "ReplaceTextTransaction::UndoTransaction() did nothing due to "
        "unexpected text");
    return NS_OK;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;

  editorBase->DoReplaceText(textNode, mOffset, mStringToInsert.Length(),
                            mStringToBeReplaced, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(textNode, mOffset,
                                                  mStringToInsert.Length(),
                                                  mStringToBeReplaced.Length());

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  // XXX Should we stop setting selection when mutation event listener
  //     modifies the text node?
  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored = selection->CollapseInLimiter(
      textNode, mOffset + mStringToBeReplaced.Length());
  if (NS_WARN_IF(editorBase->Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(NS_SUCCEEDED(rvIgnored),
               "Selection::CollapseInLimiter() failed, but ignored");
  return NS_OK;
}

NS_IMETHODIMP ReplaceTextTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode) ||
      NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*mTextNode))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ErrorResult error;
  nsAutoString undoneString;
  mTextNode->SubstringData(mOffset, mStringToBeReplaced.Length(), undoneString,
                           error);
  if (error.Failed()) {
    NS_WARNING("CharacterData::SubstringData() failed");
    return error.StealNSResult();
  }
  if (undoneString != mStringToBeReplaced) {
    NS_WARNING(
        "ReplaceTextTransaction::RedoTransaction() did nothing due to "
        "unexpected text");
    return NS_OK;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<Text> textNode = *mTextNode;

  editorBase->DoReplaceText(textNode, mOffset, mStringToBeReplaced.Length(),
                            mStringToInsert, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(textNode, mOffset,
                                                  mStringToBeReplaced.Length(),
                                                  mStringToInsert.Length());

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  // XXX Should we stop setting selection when mutation event listener
  //     modifies the text node?
  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  DebugOnly<nsresult> rvIgnored = selection->CollapseInLimiter(
      textNode, mOffset + mStringToInsert.Length());
  if (NS_WARN_IF(editorBase->Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(NS_SUCCEEDED(rvIgnored),
               "Selection::CollapseInLimiter() failed, but ignored");
  return NS_OK;
}

}  // namespace mozilla
