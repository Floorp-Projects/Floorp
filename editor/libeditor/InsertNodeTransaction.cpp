/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertNodeTransaction.h"

#include "mozilla/EditorBase.h"         // for EditorBase
#include "mozilla/EditorDOMPoint.h"     // for EditorDOMPoint

#include "mozilla/dom/Selection.h"      // for Selection

#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc.
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"                 // for nsIContent
#include "nsMemory.h"                   // for nsMemory
#include "nsReadableUtils.h"            // for ToNewCString
#include "nsString.h"                   // for nsString

namespace mozilla {

using namespace dom;

InsertNodeTransaction::InsertNodeTransaction(
                         EditorBase& aEditorBase,
                         nsIContent& aContentToInsert,
                         const EditorRawDOMPoint& aPointToInsert)
  : mContentToInsert(&aContentToInsert)
  , mPointToInsert(aPointToInsert)
  , mEditorBase(&aEditorBase)
{
  MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  // Ensure mPointToInsert stores child at offset.
  Unused << mPointToInsert.GetChildAtOffset();
}

InsertNodeTransaction::~InsertNodeTransaction()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertNodeTransaction, EditTransactionBase,
                                   mEditorBase,
                                   mContentToInsert,
                                   mPointToInsert)

NS_IMPL_ADDREF_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
InsertNodeTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) ||
      NS_WARN_IF(!mContentToInsert) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mPointToInsert.IsSetAndValid()) {
    // It seems that DOM tree has been changed after first DoTransaction()
    // and current RedoTranaction() call.
    if (mPointToInsert.GetChildAtOffset()) {
      EditorDOMPoint newPointToInsert(mPointToInsert.GetChildAtOffset());
      if (!newPointToInsert.IsSet()) {
        // The insertion point has been removed from the DOM tree.
        // In this case, we should append the node to the container instead.
        newPointToInsert.SetToEndOf(mPointToInsert.Container());
        if (NS_WARN_IF(!newPointToInsert.IsSet())) {
          return NS_ERROR_FAILURE;
        }
      }
      mPointToInsert = newPointToInsert;
    } else {
      mPointToInsert.SetToEndOf(mPointToInsert.Container());
      if (NS_WARN_IF(!mPointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  mEditorBase->MarkNodeDirty(GetAsDOMNode(mContentToInsert));

  ErrorResult error;
  mPointToInsert.Container()->InsertBefore(*mContentToInsert,
                                           mPointToInsert.GetChildAtOffset(),
                                           error);
  error.WouldReportJSException();
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  // Only set selection to insertion point if editor gives permission
  if (mEditorBase->GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    // Place the selection just after the inserted element
    EditorRawDOMPoint afterInsertedNode(mContentToInsert);
    DebugOnly<bool> advanced = afterInsertedNode.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset after the inserted node");
    selection->Collapse(afterInsertedNode, error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertNodeTransaction::UndoTransaction()
{
  if (NS_WARN_IF(!mContentToInsert) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // XXX If the inserted node has been moved to different container node or
  //     just removed from the DOM tree, this always fails.
  ErrorResult error;
  mPointToInsert.Container()->RemoveChild(*mContentToInsert, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertNodeTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertNodeTransaction");
  return NS_OK;
}

} // namespace mozilla
