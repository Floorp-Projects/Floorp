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
      mPointToInsert(aPointToInsert) {
  MOZ_ASSERT(!mPointToInsert.IsInDataNode());
  // We only need the child node at inserting new node.
  AutoEditorDOMPointOffsetInvalidator lockChild(mPointToInsert);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CreateElementTransaction,
                                   EditTransactionBase, mEditorBase,
                                   mPointToInsert, mNewElement)

NS_IMPL_ADDREF_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(CreateElementTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP CreateElementTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTag) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  OwningNonNull<EditorBase> editorBase = *mEditorBase;

  mNewElement = editorBase->CreateHTMLContent(mTag);
  if (!mNewElement) {
    NS_WARNING("EditorBase::CreateHTMLContent() failed");
    return NS_ERROR_FAILURE;
  }

  // Try to insert formatting whitespace for the new node:
  OwningNonNull<Element> newElement = *mNewElement;
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

  EditorRawDOMPoint afterNewNode(EditorRawDOMPoint::After(newElement));
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
  MOZ_ASSERT(mNewElement);
  MOZ_ASSERT(mPointToInsert.IsSet());

  if (mPointToInsert.IsSetAndValid()) {
    if (mPointToInsert.IsEndOfContainer()) {
      OwningNonNull<nsINode> container = *mPointToInsert.GetContainer();
      OwningNonNull<Element> newElement = *mNewElement;
      container->AppendChild(newElement, aError);
      NS_WARNING_ASSERTION(!aError.Failed(),
                           "nsINode::AppendChild() failed, but ignored");
      return;
    }
    MOZ_ASSERT(mPointToInsert.GetChild());
    OwningNonNull<nsINode> container = *mPointToInsert.GetContainer();
    OwningNonNull<nsIContent> child = *mPointToInsert.GetChild();
    OwningNonNull<Element> newElement = *mNewElement;
    container->InsertBefore(newElement, child, aError);
    NS_WARNING_ASSERTION(!aError.Failed(),
                         "nsINode::InsertBefore() failed, but ignored");
    return;
  }

  // We still know a child, but the child is different element's child,
  // we should just return error.
  if (NS_WARN_IF(mPointToInsert.GetChild() &&
                 mPointToInsert.GetContainer() !=
                     mPointToInsert.GetChild()->GetParentNode())) {
    // XXX Is NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE better? Since it won't
    //     cause throwing exception even if editor user throws an error
    //     returned from editor's public method.
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // If mPointToInsert has only offset and it's not valid, we need to treat
  // it as pointing end of the container.
  OwningNonNull<nsINode> container = *mPointToInsert.GetContainer();
  OwningNonNull<Element> newElement = *mNewElement;
  container->AppendChild(newElement, aError);
  NS_WARNING_ASSERTION(!aError.Failed(),
                       "nsINode::AppendChild() failed, but ignored");
}

NS_IMETHODIMP CreateElementTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mPointToInsert.IsSet()) ||
      NS_WARN_IF(!mNewElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<Element> newElement = *mNewElement;
  OwningNonNull<nsINode> containerNode = *mPointToInsert.GetContainer();
  ErrorResult error;
  containerNode->RemoveChild(newElement, error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::RemoveChild() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP CreateElementTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mPointToInsert.IsSet()) ||
      NS_WARN_IF(!mNewElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // First, reset mNewElement so it has no attributes or content
  // XXX We never actually did this, we only cleared mNewElement's contents if
  // it was a CharacterData node (which it's not, it's an Element)
  // XXX Don't we need to set selection like DoTransaction()?

  // Now, reinsert mNewElement
  ErrorResult error;
  InsertNewNode(error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "CreateElementTransaction::InsertNewNode() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
