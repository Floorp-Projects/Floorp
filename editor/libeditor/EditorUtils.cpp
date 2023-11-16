/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorUtils.h"

#include "EditorDOMPoint.h"   // for EditorDOMPoint, EditorDOMRange, etc
#include "HTMLEditHelpers.h"  // for MoveNodeResult
#include "HTMLEditUtils.h"    // for HTMLEditUtils
#include "TextEditor.h"       // for TextEditor

#include "mozilla/ComputedStyle.h"  // for ComputedStyle
#include "mozilla/IntegerRange.h"   // for IntegerRange
#include "mozilla/dom/Document.h"   // for dom::Document
#include "mozilla/dom/Selection.h"  // for dom::Selection
#include "mozilla/dom/Text.h"       // for dom::Text

#include "nsComponentManagerUtils.h"  // for do_CreateInstance
#include "nsContentUtils.h"           // for nsContentUtils
#include "nsComputedDOMStyle.h"       // for nsComputedDOMStyle
#include "nsError.h"                  // for NS_SUCCESS_* and NS_ERROR_*
#include "nsFrameSelection.h"         // for nsFrameSelection
#include "nsIContent.h"               // for nsIContent
#include "nsINode.h"                  // for nsINode
#include "nsITransferable.h"          // for nsITransferable
#include "nsRange.h"                  // for nsRange
#include "nsStyleConsts.h"            // for StyleWhiteSpace
#include "nsStyleStruct.h"            // for nsStyleText, etc

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::EditActionResult
 *****************************************************************************/

EditActionResult& EditActionResult::operator|=(
    const MoveNodeResult& aMoveNodeResult) {
  mHandled |= aMoveNodeResult.Handled();
  return *this;
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorRawDOMPoint* aOutPoint /* = nullptr */) {
  if (aOutPoint) {
    aOutPoint->Clear();
  }

  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      if (aOutPoint) {
        MOZ_ASSERT(node->IsContent());
        aOutPoint->Set(node->AsContent());
      }
      return true;
    }
  }

  return false;
}

bool EditorUtils::IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                                 EditorDOMPoint* aOutPoint) {
  MOZ_ASSERT(aOutPoint);
  aOutPoint->Clear();
  if (&aNode == &aParent) {
    return false;
  }

  for (const nsINode* node = &aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == &aParent) {
      MOZ_ASSERT(node->IsContent());
      aOutPoint->Set(node->AsContent());
      return true;
    }
  }

  return false;
}

// static
Maybe<StyleWhiteSpace> EditorUtils::GetComputedWhiteSpaceStyle(
    const nsIContent& aContent) {
  if (MOZ_UNLIKELY(!aContent.IsElement() && !aContent.GetParentElement())) {
    return Nothing();
  }
  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(
          aContent.IsElement() ? aContent.AsElement()
                               : aContent.GetParentElement());
  if (NS_WARN_IF(!elementStyle)) {
    return Nothing();
  }
  return Some(elementStyle->StyleText()->mWhiteSpace);
}

// static
bool EditorUtils::IsWhiteSpacePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->WhiteSpaceIsSignificant();
}

// static
bool EditorUtils::IsNewLinePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->NewlineIsSignificantStyle();
}

// static
bool EditorUtils::IsOnlyNewLinePreformatted(const nsIContent& aContent) {
  // Look at the node (and its parent if it's not an element), and grab its
  // ComputedStyle.
  Element* element = aContent.GetAsElementOrParentElement();
  if (!element) {
    return false;
  }

  RefPtr<const ComputedStyle> elementStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(element);
  if (!elementStyle) {
    // Consider nodes without a ComputedStyle to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no ComputedStyle).
    return false;
  }

  return elementStyle->StyleText()->mWhiteSpace == StyleWhiteSpace::PreLine;
}

// static
Result<nsCOMPtr<nsITransferable>, nsresult>
EditorUtils::CreateTransferableForPlainText(const Document& aDocument) {
  // Create generic Transferable for getting the data
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("do_CreateInstance() failed to create nsITransferable instance");
    return Err(rv);
  }

  if (!transferable) {
    NS_WARNING("do_CreateInstance() returned nullptr, but ignored");
    return nsCOMPtr<nsITransferable>();
  }

  DebugOnly<nsresult> rvIgnored =
      transferable->Init(aDocument.GetLoadContext());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsITransferable::Init() failed, but ignored");

  rvIgnored = transferable->AddDataFlavor(kTextMime);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kTextMime) failed, but ignored");
  rvIgnored = transferable->AddDataFlavor(kMozTextInternal);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kMozTextInternal) failed, but ignored");
  return transferable;
}

/******************************************************************************
 * mozilla::EditorDOMPointBase
 *****************************************************************************/

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleASCIISpace() const {
  if (IsCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleNBSP() const {
  // TODO: Perhaps, we should return false if neither previous char nor
  //       next char is collapsible white-space or NBSP.
  return IsCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharCollapsibleASCIISpaceOrNBSP() const {
  if (IsCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleASCIISpace() const {
  if (IsPreviousCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsPreviousCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsPreviousCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleNBSP() const {
  return IsPreviousCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharCollapsibleASCIISpaceOrNBSP()
    const {
  if (IsPreviousCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsPreviousCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsNextCharCollapsibleASCIISpace);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleASCIISpace() const {
  if (IsNextCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsNextCharASCIISpace() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsNextCharCollapsibleNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleNBSP() const {
  return IsNextCharNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsNextCharCollapsibleASCIISpaceOrNBSP);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharCollapsibleASCIISpaceOrNBSP() const {
  if (IsNextCharNewLine()) {
    return !EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
  }
  return IsNextCharASCIISpaceOrNBSP() &&
         !EditorUtils::IsWhiteSpacePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool, IsCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsCharPreformattedNewLine() const {
  return IsCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsPreviousCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsPreviousCharPreformattedNewLine() const {
  return IsPreviousCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsPreviousCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsPreviousCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsPreviousCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(bool,
                                             IsNextCharPreformattedNewLine);

template <typename PT, typename CT>
bool EditorDOMPointBase<PT, CT>::IsNextCharPreformattedNewLine() const {
  return IsNextCharNewLine() &&
         EditorUtils::IsNewLinePreformatted(*ContainerAs<Text>());
}

NS_INSTANTIATE_EDITOR_DOM_POINT_CONST_METHOD(
    bool, IsNextCharPreformattedNewLineCollapsedWithWhiteSpaces);

template <typename PT, typename CT>
bool EditorDOMPointBase<
    PT, CT>::IsNextCharPreformattedNewLineCollapsedWithWhiteSpaces() const {
  return IsNextCharNewLine() &&
         EditorUtils::IsOnlyNewLinePreformatted(*ContainerAs<Text>());
}

/******************************************************************************
 * mozilla::EditorDOMRangeBase
 *****************************************************************************/

NS_INSTANTIATE_EDITOR_DOM_RANGE_CONST_METHOD(nsINode*,
                                             GetClosestCommonInclusiveAncestor);

template <typename EditorDOMPointType>
nsINode* EditorDOMRangeBase<
    EditorDOMPointType>::GetClosestCommonInclusiveAncestor() const {
  if (NS_WARN_IF(!IsPositioned())) {
    return nullptr;
  }
  return nsContentUtils::GetClosestCommonInclusiveAncestor(
      mStart.GetContainer(), mEnd.GetContainer());
}

/******************************************************************************
 * mozilla::CaretPoint
 *****************************************************************************/

nsresult CaretPoint::SuggestCaretPointTo(
    EditorBase& aEditorBase, const SuggestCaretOptions& aOptions) const {
  mHandledCaretPoint = true;
  if (!mCaretPoint.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    NS_WARNING("There was no suggestion to put caret");
    return NS_ERROR_FAILURE;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aEditorBase.AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  nsresult rv = aEditorBase.CollapseSelectionTo(mCaretPoint);
  if (MOZ_UNLIKELY(rv == NS_ERROR_EDITOR_DESTROYED)) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return aOptions.contains(SuggestCaret::AndIgnoreTrivialError) && NS_FAILED(rv)
             ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
             : rv;
}

bool CaretPoint::CopyCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                                  const EditorBase& aEditorBase,
                                  const SuggestCaretOptions& aOptions) const {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aEditorBase.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = mCaretPoint;
  return true;
}

bool CaretPoint::MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                                  const EditorBase& aEditorBase,
                                  const SuggestCaretOptions& aOptions) {
  MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
  mHandledCaretPoint = true;
  if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
      !mCaretPoint.IsSet()) {
    return false;
  }
  if (aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt) &&
      !aEditorBase.AllowsTransactionsToChangeSelection()) {
    return false;
  }
  aPointToPutCaret = UnwrapCaretPoint();
  return true;
}

}  // namespace mozilla
