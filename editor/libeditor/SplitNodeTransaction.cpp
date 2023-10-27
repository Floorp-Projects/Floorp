/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SplitNodeTransaction.h"

#include "EditorDOMPoint.h"   // for EditorRawDOMPoint
#include "HTMLEditHelpers.h"  // for SplitNodeResult
#include "HTMLEditor.h"       // for HTMLEditor
#include "HTMLEditorInlines.h"
#include "HTMLEditUtils.h"
#include "SelectionState.h"  // for AutoTrackDOMPoint and RangeUpdater

#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/ToString.h"
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
    : mHTMLEditor(&aHTMLEditor),
      mSplitContent(aStartOfRightContent.template GetContainerAs<nsIContent>()),
      mSplitOffset(aStartOfRightContent.Offset()) {
  // printf("SplitNodeTransaction size: %zu\n", sizeof(SplitNodeTransaction));
  static_assert(sizeof(SplitNodeTransaction) <= 64,
                "Transaction classes may be created a lot and may be alive "
                "long so that keep the foot print smaller as far as possible");
  MOZ_DIAGNOSTIC_ASSERT(aStartOfRightContent.IsInContentNode());
  MOZ_DIAGNOSTIC_ASSERT(HTMLEditUtils::IsSplittableNode(
      *aStartOfRightContent.template ContainerAs<nsIContent>()));
}

std::ostream& operator<<(std::ostream& aStream,
                         const SplitNodeTransaction& aTransaction) {
  aStream << "{ mParentNode=" << aTransaction.mParentNode.get();
  if (aTransaction.mParentNode) {
    aStream << " (" << *aTransaction.mParentNode << ")";
  }
  aStream << ", mNewContent=" << aTransaction.mNewContent.get();
  if (aTransaction.mNewContent) {
    aStream << " (" << *aTransaction.mNewContent << ")";
  }
  aStream << ", mSplitContent=" << aTransaction.mSplitContent.get();
  if (aTransaction.mSplitContent) {
    aStream << " (" << *aTransaction.mSplitContent << ")";
  }
  aStream << ", mSplitOffset=" << aTransaction.mSplitOffset
          << ", mHTMLEditor=" << aTransaction.mHTMLEditor.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SplitNodeTransaction, EditTransactionBase,
                                   mHTMLEditor, mParentNode, mSplitContent,
                                   mNewContent)

NS_IMPL_ADDREF_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(SplitNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SplitNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP SplitNodeTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p SplitNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (MOZ_UNLIKELY(NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mSplitContent))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  MOZ_ASSERT(mSplitOffset <= mSplitContent->Length());

  // Create a new node
  IgnoredErrorResult error;
  // Don't use .downcast directly because AsContent has an assertion we want
  nsCOMPtr<nsINode> newNode = mSplitContent->CloneNode(false, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("nsINode::CloneNode() failed");
    return error.StealNSResult();
  }
  if (MOZ_UNLIKELY(NS_WARN_IF(!newNode))) {
    return NS_ERROR_UNEXPECTED;
  }

  mNewContent = newNode->AsContent();
  mParentNode = mSplitContent->GetParentNode();
  if (!mParentNode) {
    NS_WARNING("The splitting content was an orphan node");
    return NS_ERROR_NOT_AVAILABLE;
  }

  const OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  const OwningNonNull<nsIContent> splittingContent = *mSplitContent;
  // MOZ_KnownLive(*mNewContent): it's grabbed by newNode
  Result<SplitNodeResult, nsresult> splitNodeResult = DoTransactionInternal(
      htmlEditor, splittingContent, MOZ_KnownLive(*mNewContent), mSplitOffset);
  if (MOZ_UNLIKELY(splitNodeResult.isErr())) {
    NS_WARNING("SplitNodeTransaction::DoTransactionInternal() failed");
    return EditorBase::ToGenericNSResult(splitNodeResult.unwrapErr());
  }
  // The user should handle selection rather here.
  splitNodeResult.inspect().IgnoreCaretPointSuggestion();
  return NS_OK;
}

Result<SplitNodeResult, nsresult> SplitNodeTransaction::DoTransactionInternal(
    HTMLEditor& aHTMLEditor, nsIContent& aSplittingContent,
    nsIContent& aNewContent, uint32_t aSplitOffset) {
  if (Element* const splittingElement = Element::FromNode(aSplittingContent)) {
    // MOZ_KnownLive(*splittingElement): aSplittingContent should be grabbed by
    // the callers.
    nsresult rv =
        aHTMLEditor.MarkElementDirty(MOZ_KnownLive(*splittingElement));
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::MarkElementDirty() failed, but ignored");
  }

  Result<SplitNodeResult, nsresult> splitNodeResult = aHTMLEditor.DoSplitNode(
      EditorDOMPoint(&aSplittingContent,
                     std::min(aSplitOffset, aSplittingContent.Length())),
      aNewContent);
  if (MOZ_UNLIKELY(splitNodeResult.isErr())) {
    NS_WARNING("HTMLEditor::DoSplitNode() failed");
    return splitNodeResult;
  }
  // When adding caret suggestion to SplitNodeResult, here didn't change
  // selection so that just ignore it.
  splitNodeResult.inspect().IgnoreCaretPointSuggestion();
  return splitNodeResult;
}

NS_IMETHODIMP SplitNodeTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p SplitNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (MOZ_UNLIKELY(NS_WARN_IF(!mHTMLEditor) || NS_WARN_IF(!mNewContent) ||
                   NS_WARN_IF(!mParentNode) || NS_WARN_IF(!mSplitContent) ||
                   NS_WARN_IF(mNewContent->IsBeingRemoved()))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // This assumes Do inserted the new node in front of the prior existing node
  const OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  const OwningNonNull<nsIContent> keepingContent = *mSplitContent;
  const OwningNonNull<nsIContent> removingContent = *mNewContent;
  EditorDOMPoint joinedPoint;
  // Unfortunately, we cannot track joining point if moving right node content
  // into left node since it cannot track changes from web apps and HTMLEditor
  // never removes the content of the left node.  So it should be true that
  // we don't need to track the point in this case.
  nsresult rv = htmlEditor->DoJoinNodes(keepingContent, removingContent);
  if (NS_SUCCEEDED(rv)) {
    // Adjust split offset for redo here
    if (joinedPoint.IsSet()) {
      mSplitOffset = joinedPoint.Offset();
    }
  } else {
    NS_WARNING("HTMLEditor::DoJoinNodes() failed");
  }
  return rv;
}

/* Redo cannot simply resplit the right node, because subsequent transactions
 * on the redo stack may depend on the left node existing in its previous
 * state.
 */
NS_IMETHODIMP SplitNodeTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p SplitNodeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (MOZ_UNLIKELY(NS_WARN_IF(!mNewContent) || NS_WARN_IF(!mParentNode) ||
                   NS_WARN_IF(!mSplitContent) || NS_WARN_IF(!mHTMLEditor))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const OwningNonNull<HTMLEditor> htmlEditor = *mHTMLEditor;
  const OwningNonNull<nsIContent> newContent = *mNewContent;
  const OwningNonNull<nsIContent> splittingContent = *mSplitContent;
  Result<SplitNodeResult, nsresult> splitNodeResult = DoTransactionInternal(
      htmlEditor, splittingContent, newContent, mSplitOffset);
  if (MOZ_UNLIKELY(splitNodeResult.isErr())) {
    NS_WARNING("SplitNodeTransaction::DoTransactionInternal() failed");
    return EditorBase::ToGenericNSResult(splitNodeResult.unwrapErr());
  }
  // When adding caret suggestion to SplitNodeResult, here didn't change
  // selection so that just ignore it.
  splitNodeResult.inspect().IgnoreCaretPointSuggestion();
  return NS_OK;
}

}  // namespace mozilla
