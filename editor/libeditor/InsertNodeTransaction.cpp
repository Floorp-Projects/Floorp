/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertNodeTransaction.h"

#include "mozilla/EditorBase.h"      // for EditorBase
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint
#include "mozilla/HTMLEditor.h"      // for HTMLEditor
#include "mozilla/Logging.h"
#include "mozilla/TextEditor.h"  // for TextEditor
#include "mozilla/ToString.h"

#include "mozilla/dom/Selection.h"  // for Selection

#include "nsAString.h"
#include "nsDebug.h"          // for NS_WARNING, etc.
#include "nsError.h"          // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"       // for nsIContent
#include "nsMemory.h"         // for nsMemory
#include "nsReadableUtils.h"  // for ToNewCString
#include "nsString.h"         // for nsString

namespace mozilla {

using namespace dom;

template already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPoint& aPointToInsert);
template already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorRawDOMPoint& aPointToInsert);

// static
template <typename PT, typename CT>
already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPointBase<PT, CT>& aPointToInsert) {
  RefPtr<InsertNodeTransaction> transaction =
      new InsertNodeTransaction(aEditorBase, aContentToInsert, aPointToInsert);
  return transaction.forget();
}

template <typename PT, typename CT>
InsertNodeTransaction::InsertNodeTransaction(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPointBase<PT, CT>& aPointToInsert)
    : mContentToInsert(&aContentToInsert),
      mPointToInsert(aPointToInsert),
      mEditorBase(&aEditorBase) {
  MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  // Ensure mPointToInsert stores child at offset.
  Unused << mPointToInsert.GetChild();
}

std::ostream& operator<<(std::ostream& aStream,
                         const InsertNodeTransaction& aTransaction) {
  aStream << "{ mContentToInsert=" << aTransaction.mContentToInsert.get();
  if (aTransaction.mContentToInsert) {
    if (aTransaction.mContentToInsert->IsText()) {
      nsAutoString data;
      aTransaction.mContentToInsert->AsText()->GetData(data);
      aStream << " (#text \"" << NS_ConvertUTF16toUTF8(data).get() << "\")";
    } else {
      aStream << " (" << *aTransaction.mContentToInsert << ")";
    }
  }
  aStream << ", mPointToInsert=" << aTransaction.mPointToInsert
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertNodeTransaction, EditTransactionBase,
                                   mEditorBase, mContentToInsert,
                                   mPointToInsert)

NS_IMPL_ADDREF_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP InsertNodeTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mContentToInsert) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT_IF(mEditorBase->IsTextEditor(), !mContentToInsert->IsText());

  if (!mPointToInsert.IsSetAndValid()) {
    // It seems that DOM tree has been changed after first DoTransaction()
    // and current RedoTranaction() call.
    if (mPointToInsert.GetChild()) {
      EditorDOMPoint newPointToInsert(mPointToInsert.GetChild());
      if (!newPointToInsert.IsSet()) {
        // The insertion point has been removed from the DOM tree.
        // In this case, we should append the node to the container instead.
        newPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
        if (NS_WARN_IF(!newPointToInsert.IsSet())) {
          return NS_ERROR_FAILURE;
        }
      }
      mPointToInsert = newPointToInsert;
    } else {
      mPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
      if (NS_WARN_IF(!mPointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;
  OwningNonNull<nsIContent> contentToInsert = *mContentToInsert;
  OwningNonNull<nsINode> container = *mPointToInsert.GetContainer();
  nsCOMPtr<nsIContent> refChild = mPointToInsert.GetChild();
  if (contentToInsert->IsElement()) {
    nsresult rv = editorBase->MarkElementDirty(
        MOZ_KnownLive(*contentToInsert->AsElement()));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(rv);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  ErrorResult error;
  container->InsertBefore(contentToInsert, refChild, error);
  // InsertBefore() may call MightThrowJSException() even if there is no
  // error. We don't need the flag here.
  error.WouldReportJSException();
  if (error.Failed()) {
    NS_WARNING("nsINode::InsertBefore() failed");
    return error.StealNSResult();
  }

  if (!mEditorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  // Place the selection just after the inserted element.
  EditorRawDOMPoint afterInsertedNode(
      EditorRawDOMPoint::After(contentToInsert));
  NS_WARNING_ASSERTION(afterInsertedNode.IsSet(),
                       "Failed to set after the inserted node");
  IgnoredErrorResult ignoredError;
  selection->CollapseInLimiter(afterInsertedNode, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::CollapseInLimiter() failed, but ignored");
  return NS_OK;
}

NS_IMETHODIMP InsertNodeTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mContentToInsert) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // XXX If the inserted node has been moved to different container node or
  //     just removed from the DOM tree, this always fails.
  OwningNonNull<nsINode> container = *mPointToInsert.GetContainer();
  OwningNonNull<nsIContent> contentToInsert = *mContentToInsert;
  ErrorResult error;
  container->RemoveChild(contentToInsert, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP InsertNodeTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));
  return DoTransaction();
}

}  // namespace mozilla
