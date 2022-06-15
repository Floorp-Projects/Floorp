/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditHelpers.h"

#include "EditorDOMPoint.h"
#include "HTMLEditor.h"
#include "WSRunObject.h"

#include "mozilla/ContentIterator.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsRange.h"

class nsISupports;

namespace mozilla {

using namespace dom;

template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes) const;
template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<HTMLBRElement>>& aArrayOfNodes) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Element>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Text>>& aArrayOfNodes,
    void* aClosure) const;

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(nsINode& aNode) : mIter(&mPostOrderIter) {
  DebugOnly<nsresult> rv = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult DOMIterator::Init(nsRange& aRange) { return mIter->Init(&aRange); }

nsresult DOMIterator::Init(const RawRangeBoundary& aStartRef,
                           const RawRangeBoundary& aEndRef) {
  return mIter->Init(aStartRef, aEndRef);
}

DOMIterator::DOMIterator() : mIter(&mPostOrderIter) {}

template <class NodeClass>
void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    if (NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode())) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

template <class NodeClass>
void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes,
    void* aClosure /* = nullptr */) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode());
    if (node && aFunctor(*node, aClosure)) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

DOMSubtreeIterator::DOMSubtreeIterator() : DOMIterator() {
  mIter = &mSubtreeIter;
}

nsresult DOMSubtreeIterator::Init(nsRange& aRange) {
  return mIter->Init(&aRange);
}

/******************************************************************************
 * mozilla::MoveNodeResult
 *****************************************************************************/

nsresult MoveNodeResult::SuggestCaretPointTo(
    const HTMLEditor& aHTMLEditor, const SuggestCaretOptions& aOptions) const {
  mHandledCaretPoint = true;
  if (!mCaretPoint.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    NS_WARNING("There was no suggestion to put caret");
    return NS_ERROR_FAILURE;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  nsresult rv = aHTMLEditor.CollapseSelectionTo(mCaretPoint);
  if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return aOptions.contains(SuggestCaret::AndIgnoreTrivialError) &&
                 MOZ_UNLIKELY(NS_FAILED(rv))
             ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
             : rv;
}

bool MoveNodeResult::MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                                      const HTMLEditor& aHTMLEditor,
                                      const SuggestCaretOptions& aOptions) {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = UnwrapCaretPoint();
  return true;
}

/******************************************************************************
 * mozilla::SplitNodeResult
 *****************************************************************************/

nsresult SplitNodeResult::SuggestCaretPointTo(
    const HTMLEditor& aHTMLEditor, const SuggestCaretOptions& aOptions) const {
  mHandledCaretPoint = true;
  if (!mCaretPoint.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    NS_WARNING("There was no suggestion to put caret");
    return NS_ERROR_FAILURE;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  nsresult rv = aHTMLEditor.CollapseSelectionTo(mCaretPoint);
  if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return aOptions.contains(SuggestCaret::AndIgnoreTrivialError) &&
                 MOZ_UNLIKELY(NS_FAILED(rv))
             ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
             : rv;
}

bool SplitNodeResult::MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                                       const HTMLEditor& aHTMLEditor,
                                       const SuggestCaretOptions& aOptions) {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = UnwrapCaretPoint();
  return true;
}

/******************************************************************************
 * mozilla::SplitRangeOffFromNodeResult
 *****************************************************************************/

nsresult SplitRangeOffFromNodeResult::SuggestCaretPointTo(
    const HTMLEditor& aHTMLEditor, const SuggestCaretOptions& aOptions) const {
  mHandledCaretPoint = true;
  if (!mCaretPoint.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    NS_WARNING("There was no suggestion to put caret");
    return NS_ERROR_FAILURE;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  nsresult rv = aHTMLEditor.CollapseSelectionTo(mCaretPoint);
  if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return aOptions.contains(SuggestCaret::AndIgnoreTrivialError) &&
                 MOZ_UNLIKELY(NS_FAILED(rv))
             ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
             : rv;
}

bool SplitRangeOffFromNodeResult::MoveCaretPointTo(
    EditorDOMPoint& aPointToPutCaret, const HTMLEditor& aHTMLEditor,
    const SuggestCaretOptions& aOptions) {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aHTMLEditor.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = UnwrapCaretPoint();
  return true;
}

}  // namespace mozilla
