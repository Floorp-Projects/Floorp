/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorInlines_h
#define HTMLEditorInlines_h

#include "HTMLEditor.h"

#include "EditorDOMPoint.h"
#include "HTMLEditHelpers.h"
#include "SelectionState.h"  // for RangeItem

#include "ErrorList.h"  // for nsresult

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Debug.h"
#include "mozilla/Likely.h"
#include "mozilla/RefPtr.h"

#include "mozilla/dom/Element.h"

#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsRange.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

Result<CreateElementResult, nsresult>
HTMLEditor::ReplaceContainerAndCloneAttributesWithTransaction(
    Element& aOldContainer, const nsAtom& aTagName) {
  return ReplaceContainerWithTransactionInternal(
      aOldContainer, aTagName, *nsGkAtoms::_empty, u""_ns, true);
}

Result<CreateElementResult, nsresult>
HTMLEditor::ReplaceContainerWithTransaction(Element& aOldContainer,
                                            const nsAtom& aTagName,
                                            const nsAtom& aAttribute,
                                            const nsAString& aAttributeValue) {
  return ReplaceContainerWithTransactionInternal(
      aOldContainer, aTagName, aAttribute, aAttributeValue, false);
}

Result<CreateElementResult, nsresult>
HTMLEditor::ReplaceContainerWithTransaction(Element& aOldContainer,
                                            const nsAtom& aTagName) {
  return ReplaceContainerWithTransactionInternal(
      aOldContainer, aTagName, *nsGkAtoms::_empty, u""_ns, false);
}

Result<MoveNodeResult, nsresult> HTMLEditor::MoveNodeToEndWithTransaction(
    nsIContent& aContentToMove, nsINode& aNewContainer) {
  return MoveNodeWithTransaction(aContentToMove,
                                 EditorDOMPoint::AtEndOf(aNewContainer));
}

Element* HTMLEditor::GetTableCellElementAt(
    Element& aTableElement, const CellIndexes& aCellIndexes) const {
  return GetTableCellElementAt(aTableElement, aCellIndexes.mRow,
                               aCellIndexes.mColumn);
}

already_AddRefed<RangeItem>
HTMLEditor::GetSelectedRangeItemForTopLevelEditSubAction() const {
  if (!mSelectedRangeForTopLevelEditSubAction) {
    mSelectedRangeForTopLevelEditSubAction = new RangeItem();
  }
  return do_AddRef(mSelectedRangeForTopLevelEditSubAction);
}

already_AddRefed<nsRange> HTMLEditor::GetChangedRangeForTopLevelEditSubAction()
    const {
  if (!mChangedRangeForTopLevelEditSubAction) {
    mChangedRangeForTopLevelEditSubAction = nsRange::Create(GetDocument());
  }
  return do_AddRef(mChangedRangeForTopLevelEditSubAction);
}

// static
template <typename EditorDOMPointType>
HTMLEditor::CharPointType HTMLEditor::GetPreviousCharPointType(
    const EditorDOMPointType& aPoint) {
  MOZ_ASSERT(aPoint.IsInTextNode());
  if (aPoint.IsStartOfContainer()) {
    return CharPointType::TextEnd;
  }
  if (aPoint.IsPreviousCharPreformattedNewLine()) {
    return CharPointType::PreformattedLineBreak;
  }
  if (EditorUtils::IsWhiteSpacePreformatted(
          *aPoint.template ContainerAs<Text>())) {
    return CharPointType::PreformattedChar;
  }
  if (aPoint.IsPreviousCharASCIISpace()) {
    return CharPointType::ASCIIWhiteSpace;
  }
  return aPoint.IsPreviousCharNBSP() ? CharPointType::NoBreakingSpace
                                     : CharPointType::VisibleChar;
}

// static
template <typename EditorDOMPointType>
HTMLEditor::CharPointType HTMLEditor::GetCharPointType(
    const EditorDOMPointType& aPoint) {
  MOZ_ASSERT(aPoint.IsInTextNode());
  if (aPoint.IsEndOfContainer()) {
    return CharPointType::TextEnd;
  }
  if (aPoint.IsCharPreformattedNewLine()) {
    return CharPointType::PreformattedLineBreak;
  }
  if (EditorUtils::IsWhiteSpacePreformatted(
          *aPoint.template ContainerAs<Text>())) {
    return CharPointType::PreformattedChar;
  }
  if (aPoint.IsCharASCIISpace()) {
    return CharPointType::ASCIIWhiteSpace;
  }
  return aPoint.IsCharNBSP() ? CharPointType::NoBreakingSpace
                             : CharPointType::VisibleChar;
}

}  // namespace mozilla

#endif  // HTMLEditorInlines_h
