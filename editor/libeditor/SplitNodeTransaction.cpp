/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SplitNodeTransaction.h"

#include "mozilla/EditorDOMPoint.h"  // for RangeBoundary, EditorRawDOMPoint
#include "mozilla/HTMLEditor.h"      // for HTMLEditor
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"     // for NS_ASSERTION, etc.
#include "nsError.h"     // for NS_ERROR_NOT_INITIALIZED, etc.
#include "nsIContent.h"  // for nsIContent

namespace mozilla {

using namespace dom;

template already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aStartOfRightContent);
template already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aStartOfRightContent);

// static
template <typename PT, typename CT>
already_AddRefed<SplitNodeTransaction> SplitNodeTransaction::Create(
    HTMLEditor& aHTMLEditor,
    const EditorDOMPointBase<PT, CT>& aStartOfRightContent) {
  RefPtr<SplitNodeTransaction> transaction =
      new SplitNodeTransaction(aHTMLEditor, aStartOfRightContent);
  return transaction.forget();
}

template <typename PT, typename CT>
SplitNodeTransaction::SplitNodeTransaction(
    HTMLEditor& aHTMLEditor,
    const EditorDOMPointBase<PT, CT>& aStartOfRightContent)
    : mHTMLEditor(&aHTMLEditor), mStartOfRightContent(aStartOfRightContent) {
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightContent.IsInContentNode());
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SplitNodeTransaction, EditTransactionBase,
                                   mHTMLEditor, mStartOfRightContent,
                                   mContainerParentNode, mNewLeftContent)

NS_IMPL_ADDREF_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP SplitNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!mHTMLEditor) ||
      NS_WARN_IF(!mStartOfRightContent.IsInContentNode())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  MOZ_ASSERT(mStartOfRightContent.IsSetAndValid());

  // Create a new node
  ErrorResult error;
  // Don't use .downcast directly because AsContent has an assertion we want
  nsCOMPtr<nsINode> cloneOfRightContainer =
      mStartOfRightContent.GetContainer()->CloneNode(false, error);
  if (error.Failed()) {
    NS_WARNING("nsINode::CloneNode() failed");
    return error.StealNSResult();
  }
  if (NS_WARN_IF(!cloneOfRightContainer)) {
    return NS_ERROR_UNEXPECTED;
  }

  mNewLeftContent = cloneOfRightContainer->AsContent();

  mContainerParentNode = mStartOfRightContent.GetContainerParent();
  if (!mContainerParentNode) {
    NS_WARNING("Right container was an orphan node");
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsIContent> newLeftContent = *mNewLeftContent;
  OwningNonNull<nsINode> containerParentNode = *mContainerParentNode;
  EditorDOMPoint startOfRightContent(mStartOfRightContent);

  if (RefPtr<Element> startOfRightNode =
          startOfRightContent.GetContainerAsElement()) {
    nsresult rv = htmlEditor->MarkElementDirty(*startOfRightNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(rv);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  // Insert the new node
  htmlEditor->DoSplitNode(startOfRightContent, newLeftContent, error);
  if (error.Failed()) {
    NS_WARNING("HTMLEditor::DoSplitNode() failed");
    return error.StealNSResult();
  }

  if (!htmlEditor->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  NS_WARNING_ASSERTION(
      !htmlEditor->Destroyed(),
      "The editor has gone but SplitNodeTransaction keeps trying to modify "
      "Selection");
  RefPtr<Selection> selection = htmlEditor->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint atEndOfLeftNode(EditorRawDOMPoint::AtEndOf(newLeftContent));
  selection->Collapse(atEndOfLeftNode, error);
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::Collapse() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP SplitNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mNewLeftContent) ||
      NS_WARN_IF(!mContainerParentNode) ||
      NS_WARN_IF(!mStartOfRightContent.IsInContentNode())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // This assumes Do inserted the new node in front of the prior existing node
  // XXX Perhaps, we should reset mStartOfRightNode with current first child
  //     of the right node.
  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsIContent> containerContent =
      *mStartOfRightContent.ContainerAsContent();
  OwningNonNull<nsIContent> newLeftContent = *mNewLeftContent;
  nsresult rv = htmlEditor->DoJoinNodes(containerContent, newLeftContent);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HTMLEditor::DoJoinNodes() failed");
  return rv;
}

/* Redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous
 * state.
 */
NS_IMETHODIMP SplitNodeTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mNewLeftContent) || NS_WARN_IF(!mContainerParentNode) ||
      NS_WARN_IF(!mStartOfRightContent.IsInContentNode()) ||
      NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  OwningNonNull<nsINode> newLeftContent = *mNewLeftContent;
  OwningNonNull<nsINode> containerParentNode = *mContainerParentNode;
  EditorDOMPoint startOfRightContent(mStartOfRightContent);

  // First, massage the existing node so it is in its post-split state
  ErrorResult error;
  if (startOfRightContent.IsInTextNode()) {
    Text* rightTextNode = startOfRightContent.ContainerAsText();
    htmlEditor->DoDeleteText(MOZ_KnownLive(*rightTextNode), 0,
                             startOfRightContent.Offset(), error);
    if (error.Failed()) {
      NS_WARNING("EditorBase::DoDeleteText() failed");
      return error.StealNSResult();
    }
  } else {
    AutoTArray<OwningNonNull<nsIContent>, 24> movingChildren;
    if (nsIContent* child =
            startOfRightContent.GetContainer()->GetFirstChild()) {
      movingChildren.AppendElement(*child);
      for (uint32_t i = 0; i < startOfRightContent.Offset(); i++) {
        child = child->GetNextSibling();
        if (!child) {
          break;
        }
        movingChildren.AppendElement(*child);
      }
    }
    ErrorResult error;
    for (OwningNonNull<nsIContent>& child : movingChildren) {
      newLeftContent->AppendChild(child, error);
      if (error.Failed()) {
        NS_WARNING("nsINode::AppendChild() failed");
        return error.StealNSResult();
      }
    }
  }
  MOZ_ASSERT(!error.Failed());
  // Second, re-insert the left node into the tree
  containerParentNode->InsertBefore(newLeftContent,
                                    startOfRightContent.GetContainer(), error);
  NS_WARNING_ASSERTION(!error.Failed(), "nsINode::InsertBefore() failed");
  return error.StealNSResult();
}

}  // namespace mozilla
