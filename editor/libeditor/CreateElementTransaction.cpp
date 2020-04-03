/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateElementTransaction.h"

#include <algorithm>
#include <stdio.h>

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"

#include "mozilla/Casting.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"

#include "nsAlgorithm.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISupportsUtils.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

template already_AddRefed<CreateElementTransaction>
CreateElementTransaction::Create(EditorBase& aEditorBase, nsAtom& aTag,
                                 const EditorDOMPoint& aPointToInsert);
template already_AddRefed<CreateElementTransaction>
CreateElementTransaction::Create(EditorBase& aEditorBase, nsAtom& aTag,
                                 const EditorRawDOMPoint& aPointToInsert);

template <typename PT, typename CT>
already_AddRefed<CreateElementTransaction> CreateElementTransaction::Create(
    EditorBase& aEditorBase, nsAtom& aTag,
    const EditorDOMPointBase<PT, CT>& aPointToInsert) {
  RefPtr<CreateElementTransaction> transaction =
      new CreateElementTransaction(aEditorBase, aTag, aPointToInsert);
  return transaction.forget();
}

template <typename PT, typename CT>
CreateElementTransaction::CreateElementTransaction(
    EditorBase& aEditorBase, nsAtom& aTag,
    const EditorDOMPointBase<PT, CT>& aPointToInsert)
    : EditTransactionBase(),
      mEditorBase(&aEditorBase),
      mTag(&aTag),
      mPointToInsert(aPointToInsert) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CreateElementTransaction,
                                   EditTransactionBase, mEditorBase,
                                   mPointToInsert, mNewNode)

NS_IMPL_ADDREF_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
CreateElementTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTag) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<EditorBase> editorBase(mEditorBase);

  mNewNode = editorBase->CreateHTMLContent(mTag);
  if (!mNewNode) {
    NS_WARNING("EditorBase::CreateHTMLContent() failed");
    return NS_ERROR_FAILURE;
  }

  // Try to insert formatting whitespace for the new node:
  OwningNonNull<Element> newElement(*mNewNode);
  nsresult rv = editorBase->MarkElementDirty(newElement);
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return EditorBase::ToGenericNSResult(rv);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::MarkElementDirty() failed, but ignored");

  // Insert the new node
  ErrorResult error;
  InsertNewNode(error);
  if (error.Failed()) {
    NS_WARNING("CreateElementTransaction::InsertNewNode() failed");
    return error.StealNSResult();
  }

  // Only set selection to insertion point if editor gives permission
  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    // Do nothing - DOM range gravity will adjust selection
    return NS_OK;
  }

  RefPtr<Selection> selection = editorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  EditorRawDOMPoint afterNewNode(EditorRawDOMPoint::After(mNewNode));
  if (NS_WARN_IF(!afterNewNode.IsSet())) {
    // If mutation observer or mutation event listener moved or removed the
    // new node, we hit this case.  Should we use script blocker while we're
    // in this method?
    return NS_ERROR_FAILURE;
  }
  IgnoredErrorResult ignoredError;
  selection->Collapse(afterNewNode, ignoredError);
  NS_WARNING_ASSERTION(!ignoredError.Failed(),
                       "Selection::Collapse() failed, but ignored");
  return NS_OK;
}

void CreateElementTransaction::InsertNewNode(ErrorResult& aError) {
  if (mPointToInsert.IsSetAndValid()) {
    if (mPointToInsert.IsEndOfContainer()) {
      mPointToInsert.GetContainer()->AppendChild(*mNewNode, aError);
      NS_WARNING_ASSERTION(!aError.Failed(),
                           "nsINode::AppendChild() failed, but ignored");
      return;
    }
    mPointToInsert.GetContainer()->InsertBefore(
        *mNewNode, mPointToInsert.GetChild(), aError);
    NS_WARNING_ASSERTION(!aError.Failed(),
                         "nsINode::InsertBefore() failed, but ignored");
    return;
  }

  if (NS_WARN_IF(mPointToInsert.GetChild() &&
                 mPointToInsert.GetContainer() !=
                     mPointToInsert.GetChild()->GetParentNode())) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // If mPointToInsert has only offset and it's not valid, we need to treat
  // it as pointing end of the container.
  mPointToInsert.GetContainer()->AppendChild(*mNewNode, aError);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "nsINode::AppendChild() failed, but ignored");
}

NS_IMETHODIMP CreateElementTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  mPointToInsert.GetContainer()->RemoveChild(*mNewNode, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP CreateElementTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // First, reset mNewNode so it has no attributes or content
  // XXX We never actually did this, we only cleared mNewNode's contents if it
  // was a CharacterData node (which it's not, it's an Element)
  // XXX Don't we need to set selection like DoTransaction()?

  // Now, reinsert mNewNode
  ErrorResult error;
  InsertNewNode(error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "CreateElementTransaction::InsertNewNode() failed");
  return error.StealNSResult();
}

already_AddRefed<Element> CreateElementTransaction::GetNewNode() {
  return nsCOMPtr<Element>(mNewNode).forget();
}

}  // namespace mozilla
