/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "mozilla/HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/FlushType.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsISupportsUtils.h"
#include "nsITableCellLayout.h"  // For efficient access to table cell
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTableCellFrame.h"
#include "nsTableWrapperFrame.h"
#include "nscore.h"
#include <algorithm>

namespace mozilla {

using namespace dom;
using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;

/**
 * Stack based helper class for restoring selection after table edit.
 */
class MOZ_STACK_CLASS AutoSelectionSetterAfterTableEdit final {
 private:
  RefPtr<HTMLEditor> mHTMLEditor;
  RefPtr<Element> mTable;
  int32_t mCol, mRow, mDirection, mSelected;

 public:
  AutoSelectionSetterAfterTableEdit(HTMLEditor& aHTMLEditor, Element* aTable,
                                    int32_t aRow, int32_t aCol,
                                    int32_t aDirection, bool aSelected)
      : mHTMLEditor(&aHTMLEditor),
        mTable(aTable),
        mCol(aCol),
        mRow(aRow),
        mDirection(aDirection),
        mSelected(aSelected) {}

  MOZ_CAN_RUN_SCRIPT ~AutoSelectionSetterAfterTableEdit() {
    if (mHTMLEditor) {
      MOZ_KnownLive(mHTMLEditor)
          ->SetSelectionAfterTableEdit(MOZ_KnownLive(mTable), mRow, mCol,
                                       mDirection, mSelected);
    }
  }

  // This is needed to abort the caret reset in the destructor
  // when one method yields control to another
  void CancelSetCaret() {
    mHTMLEditor = nullptr;
    mTable = nullptr;
  }
};

nsresult HTMLEditor::InsertCell(Element* aCell, int32_t aRowSpan,
                                int32_t aColSpan, bool aAfter, bool aIsHeader,
                                Element** aNewCell) {
  if (aNewCell) {
    *aNewCell = nullptr;
  }

  if (NS_WARN_IF(!aCell)) {
    return NS_ERROR_INVALID_ARG;
  }

  // And the parent and offsets needed to do an insert
  EditorDOMPoint pointToInsert(aCell);
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Element> newCell =
      CreateElementWithDefaults(aIsHeader ? *nsGkAtoms::th : *nsGkAtoms::td);
  if (!newCell) {
    NS_WARNING(
        "HTMLEditor::CreateElementWithDefaults(nsGkAtoms::th or td) failed");
    return NS_ERROR_FAILURE;
  }

  // Optional: return new cell created
  if (aNewCell) {
    *aNewCell = do_AddRef(newCell).take();
  }

  if (aRowSpan > 1) {
    // Note: Do NOT use editor transaction for this
    nsAutoString newRowSpan;
    newRowSpan.AppendInt(aRowSpan, 10);
    DebugOnly<nsresult> rvIgnored = newCell->SetAttr(
        kNameSpaceID_None, nsGkAtoms::rowspan, newRowSpan, true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::rawspan) failed, but ignored");
  }
  if (aColSpan > 1) {
    // Note: Do NOT use editor transaction for this
    nsAutoString newColSpan;
    newColSpan.AppendInt(aColSpan, 10);
    DebugOnly<nsresult> rvIgnored = newCell->SetAttr(
        kNameSpaceID_None, nsGkAtoms::colspan, newColSpan, true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::colspan) failed, but ignored");
  }
  if (aAfter) {
    DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to advance offset to after the old cell");
  }

  // Don't let Rules System change the selection.
  AutoTransactionsConserveSelection dontChangeSelection(*this);
  nsresult rv = InsertNodeWithTransaction(*newCell, pointToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::SetColSpan(Element* aCell, int32_t aColSpan) {
  if (NS_WARN_IF(!aCell)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsAutoString newSpan;
  newSpan.AppendInt(aColSpan, 10);
  nsresult rv =
      SetAttributeWithTransaction(*aCell, *nsGkAtoms::colspan, newSpan);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::SetAttributeWithTransaction(nsGkAtoms::colspan) failed");
  return rv;
}

nsresult HTMLEditor::SetRowSpan(Element* aCell, int32_t aRowSpan) {
  if (NS_WARN_IF(!aCell)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsAutoString newSpan;
  newSpan.AppendInt(aRowSpan, 10);
  nsresult rv =
      SetAttributeWithTransaction(*aCell, *nsGkAtoms::rowspan, newSpan);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::SetAttributeWithTransaction(nsGkAtoms::rowspan) failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::InsertTableCell(int32_t aNumberOfCellsToInsert,
                                          bool aInsertAfterSelectedCell) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertTableCellElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = InsertTableCellsWithTransaction(
      aNumberOfCellsToInsert, aInsertAfterSelectedCell
                                  ? InsertPosition::eAfterSelectedCell
                                  : InsertPosition::eBeforeSelectedCell);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertTableCellsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::InsertTableCellsWithTransaction(
    int32_t aNumberOfCellsToInsert, InsertPosition aInsertPosition) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> curCell;
  nsCOMPtr<nsINode> cellParent;
  int32_t cellOffset, startRowIndex, startColIndex;
  nsresult rv = GetCellContext(getter_AddRefs(table), getter_AddRefs(curCell),
                               getter_AddRefs(cellParent), &cellOffset,
                               &startRowIndex, &startColIndex);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!table || !curCell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if no cell found.
    return NS_OK;
  }

  // Get more data for current cell in row we are inserting at since we need
  // colspan value.
  IgnoredErrorResult ignoredError;
  CellData cellDataAtSelection(*this, *table, startRowIndex, startColIndex,
                               ignoredError);
  if (cellDataAtSelection.FailedOrNotFound()) {
    NS_WARNING("CellData couldn't find selected cell");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!ignoredError.Failed());
  MOZ_ASSERT(curCell == cellDataAtSelection.mElement);

  int32_t newCellIndex;
  switch (aInsertPosition) {
    case InsertPosition::eBeforeSelectedCell:
      newCellIndex = cellDataAtSelection.mCurrent.mColumn;
      break;
    case InsertPosition::eAfterSelectedCell:
      MOZ_ASSERT(!cellDataAtSelection.IsSpannedFromOtherRowOrColumn());
      newCellIndex = cellDataAtSelection.NextColumnIndex();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid InsertPosition");
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of BR in new cell until we're done
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  // We control selection resetting after the insert.
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, cellDataAtSelection.mCurrent.mRow, newCellIndex,
      ePreviousColumn, false);
  // So, suppress Rules System selection munging.
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  EditorDOMPoint pointToInsert(cellParent, cellOffset);
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  if (aInsertPosition == InsertPosition::eAfterSelectedCell) {
    DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
                         "Failed to move insertion point after the cell");
  }
  for (int32_t i = 0; i < aNumberOfCellsToInsert; i++) {
    RefPtr<Element> newCell = CreateElementWithDefaults(*nsGkAtoms::td);
    if (!newCell) {
      NS_WARNING("HTMLEditor::CreateElementWithDefaults(nsGkAtoms::td) failed");
      return NS_ERROR_FAILURE;
    }
    AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
    nsresult rv = InsertNodeWithTransaction(*newCell, pointToInsert);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetFirstRow(Element* aTableOrElementInTable,
                                      Element** aFirstRowElement) {
  if (NS_WARN_IF(!aTableOrElementInTable) || NS_WARN_IF(!aFirstRowElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ErrorResult error;
  RefPtr<Element> firstRowElement =
      GetFirstTableRowElement(*aTableOrElementInTable, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "HTMLEditor::GetFirstTableRowElement() failed");
  firstRowElement.forget(aFirstRowElement);
  return EditorBase::ToGenericNSResult(error.StealNSResult());
}

Element* HTMLEditor::GetFirstTableRowElement(Element& aTableOrElementInTable,
                                             ErrorResult& aRv) const {
  MOZ_ASSERT(!aRv.Failed());

  Element* tableElement = GetInclusiveAncestorByTagNameInternal(
      *nsGkAtoms::table, aTableOrElementInTable);
  // If the element is not in <table>, return error.
  if (!tableElement) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
        "failed");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  for (nsIContent* tableChild = tableElement->GetFirstChild(); tableChild;
       tableChild = tableChild->GetNextSibling()) {
    if (tableChild->IsHTMLElement(nsGkAtoms::tr)) {
      // Found a row directly under <table>
      return tableChild->AsElement();
    }
    // <table> can have table section elements like <tbody>.  <tr> elements
    // may be children of them.
    if (tableChild->IsAnyOfHTMLElements(nsGkAtoms::tbody, nsGkAtoms::thead,
                                        nsGkAtoms::tfoot)) {
      for (nsIContent* tableSectionChild = tableChild->GetFirstChild();
           tableSectionChild;
           tableSectionChild = tableSectionChild->GetNextSibling()) {
        if (tableSectionChild->IsHTMLElement(nsGkAtoms::tr)) {
          return tableSectionChild->AsElement();
        }
      }
    }
  }
  // Don't return error when there is no <tr> element in the <table>.
  return nullptr;
}

Element* HTMLEditor::GetNextTableRowElement(Element& aTableRowElement,
                                            ErrorResult& aRv) const {
  MOZ_ASSERT(!aRv.Failed());

  if (NS_WARN_IF(!aTableRowElement.IsHTMLElement(nsGkAtoms::tr))) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  for (nsIContent* maybeNextRow = aTableRowElement.GetNextSibling();
       maybeNextRow; maybeNextRow = maybeNextRow->GetNextSibling()) {
    if (maybeNextRow->IsHTMLElement(nsGkAtoms::tr)) {
      return maybeNextRow->AsElement();
    }
  }

  // In current table section (e.g., <tbody>), there is no <tr> element.
  // Then, check the following table sections.
  Element* parentElementOfRow = aTableRowElement.GetParentElement();
  if (!parentElementOfRow) {
    NS_WARNING("aTableRowElement was an orphan node");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Basically, <tr> elements should be in table section elements even if
  // they are not written in the source explicitly.  However, for preventing
  // cross table boundary, check it now.
  if (parentElementOfRow->IsHTMLElement(nsGkAtoms::table)) {
    // Don't return error since this means just not found.
    return nullptr;
  }

  for (nsIContent* maybeNextTableSection = parentElementOfRow->GetNextSibling();
       maybeNextTableSection;
       maybeNextTableSection = maybeNextTableSection->GetNextSibling()) {
    // If the sibling of parent of given <tr> is a table section element,
    // check its children.
    if (maybeNextTableSection->IsAnyOfHTMLElements(
            nsGkAtoms::tbody, nsGkAtoms::thead, nsGkAtoms::tfoot)) {
      for (nsIContent* maybeNextRow = maybeNextTableSection->GetFirstChild();
           maybeNextRow; maybeNextRow = maybeNextRow->GetNextSibling()) {
        if (maybeNextRow->IsHTMLElement(nsGkAtoms::tr)) {
          return maybeNextRow->AsElement();
        }
      }
    }
    // I'm not sure whether this is a possible case since table section
    // elements are created automatically.  However, DOM API may create
    // <tr> elements without table section elements.  So, let's check it.
    else if (maybeNextTableSection->IsHTMLElement(nsGkAtoms::tr)) {
      return maybeNextTableSection->AsElement();
    }
  }
  // Don't return error when the given <tr> element is the last <tr> element in
  // the <table>.
  return nullptr;
}

nsresult HTMLEditor::GetLastCellInRow(nsINode* aRowNode, nsINode** aCellNode) {
  if (NS_WARN_IF(!aCellNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aCellNode = nullptr;

  if (NS_WARN_IF(!aRowNode)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> rowChild = aRowNode->GetLastChild();
  while (rowChild && !HTMLEditUtils::IsTableCell(rowChild)) {
    // Skip over textnodes
    rowChild = rowChild->GetPreviousSibling();
  }
  if (rowChild) {
    rowChild.forget(aCellNode);
    return NS_OK;
  }
  // If here, cell was not found
  return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
}

NS_IMETHODIMP HTMLEditor::InsertTableColumn(int32_t aNumberOfColumnsToInsert,
                                            bool aInsertAfterSelectedCell) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertTableColumn);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = InsertTableColumnsWithTransaction(
      aNumberOfColumnsToInsert, aInsertAfterSelectedCell
                                    ? InsertPosition::eAfterSelectedCell
                                    : InsertPosition::eBeforeSelectedCell);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::InsertTableColumnsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::InsertTableColumnsWithTransaction(
    int32_t aNumberOfColumnsToInsert, InsertPosition aInsertPosition) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> curCell;
  int32_t startRowIndex, startColIndex;
  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(curCell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!table || !curCell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if no cell found.
    return NS_OK;
  }

  // Get more data for current cell, we need rowspan value.
  IgnoredErrorResult ignoredError;
  CellData cellDataAtSelection(*this, *table, startRowIndex, startColIndex,
                               ignoredError);
  if (cellDataAtSelection.FailedOrNotFound()) {
    NS_WARNING("CellData couldn't find selected cell");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!ignoredError.Failed());
  MOZ_ASSERT(curCell == cellDataAtSelection.mElement);

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }
  // Should not be empty since we've already found a cell.
  MOZ_ASSERT(!tableSize.IsEmpty());

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of <br> element in new cell until we're done.
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  switch (aInsertPosition) {
    case InsertPosition::eBeforeSelectedCell:
      break;
    case InsertPosition::eAfterSelectedCell:
      // Use column after current cell.
      startColIndex += cellDataAtSelection.mEffectiveColSpan;

      // Detect when user is adding after a colspan=0 case.
      // Assume they want to stop the "0" behavior and really add a new column.
      // Thus we set the colspan to its true value.
      if (!cellDataAtSelection.mColSpan) {
        DebugOnly<nsresult> rvIgnored =
            SetColSpan(MOZ_KnownLive(cellDataAtSelection.mElement),
                       cellDataAtSelection.mEffectiveColSpan);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                             "HTMLEditor::SetColSpan() failed, but ignored");
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid InsertPosition");
  }

  // We control selection resetting after the insert.
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, cellDataAtSelection.mCurrent.mRow, startColIndex,
      ePreviousRow, false);
  // Suppress Rules System selection munging.
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  // If we are inserting after all existing columns, make sure table is
  // "well formed" before appending new column.
  // XXX As far as I've tested, NormalizeTableInternal() always fails to
  //     normalize non-rectangular table.  So, the following CellData will
  //     fail if the table is not rectangle.
  if (startColIndex >= tableSize.mColumnCount) {
    DebugOnly<nsresult> rv = NormalizeTableInternal(*table);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::NormalizeTableInternal() failed, but ignored");
  }

  RefPtr<Element> rowElement;
  for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount; rowIndex++) {
    if (startColIndex < tableSize.mColumnCount) {
      // We are inserting before an existing column.
      CellData cellData(*this, *table, rowIndex, startColIndex, ignoredError);
      if (cellData.FailedOrNotFound()) {
        NS_WARNING("CellData failed");
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(!ignoredError.Failed());

      // Don't fail entire process if we fail to find a cell (may fail just in
      // particular rows with < adequate cells per row).
      // XXX So, here wants to know whether the CellData actually failed above.
      //     Fix this later.
      if (!cellData.mElement) {
        continue;
      }

      if (cellData.IsSpannedFromOtherColumn()) {
        // If we have a cell spanning this location, simply increase its
        // colspan to keep table rectangular.
        // Note: we do nothing if colsspan=0, since it should automatically
        // span the new column.
        if (cellData.mColSpan > 0) {
          DebugOnly<nsresult> rvIgnored =
              SetColSpan(MOZ_KnownLive(cellData.mElement),
                         cellData.mColSpan + aNumberOfColumnsToInsert);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "HTMLEditor::SetColSpan() failed, but ignored");
        }
        continue;
      }

      // Simply set selection to the current cell. So, we can let
      // InsertTableCellsWithTransaction() do the work.  Insert a new cell
      // before current one.
      CollapseSelectionToStartOf(MOZ_KnownLive(*cellData.mElement),
                                 ignoredError);
      if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          !ignoredError.Failed(),
          "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
      ignoredError.SuppressException();
      rv = InsertTableCellsWithTransaction(aNumberOfColumnsToInsert,
                                           InsertPosition::eBeforeSelectedCell);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::InsertTableCellsWithTransaction() "
                           "failed, but might be ignored");
      continue;
    }

    // Get current row and append new cells after last cell in row
    if (!rowIndex) {
      rowElement = GetFirstTableRowElement(*table, error);
      if (error.Failed()) {
        NS_WARNING("HTMLEditor::GetFirstTableRowElement() failed");
        return error.StealNSResult();
      }
      if (!rowElement) {
        NS_WARNING("There was no table row");
        continue;
      }
    } else {
      if (!rowElement) {
        NS_WARNING("Have not found table row yet");
        // XXX Looks like that when rowIndex is 0, startColIndex is always
        //     same as or larger than tableSize.mColumnCount.  Is it true?
        return NS_ERROR_FAILURE;
      }
      rowElement = GetNextTableRowElement(*rowElement, error);
      if (error.Failed()) {
        NS_WARNING("HTMLEditor::GetNextTableRowElement() failed");
        return error.StealNSResult();
      }
      if (!rowElement) {
        NS_WARNING(
            "HTMLEditor::GetNextTableRowElement() didn't return <tr> element");
        continue;
      }
    }

    nsCOMPtr<nsINode> lastCellNode;
    rv = GetLastCellInRow(rowElement, getter_AddRefs(lastCellNode));
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::GetLastCellInRow() failed");
      return rv;
    }
    if (!lastCellNode) {
      NS_WARNING("HTMLEditor::GetLastCellInRow() didn't return cell");
      return NS_ERROR_FAILURE;
    }

    // Simply add same number of cells to each row.  Although tempted to check
    // cell indexes for current cell, the effects of colspan > 1 in some cells
    // makes this futile.  We must use NormalizeTableInternal() first to assure
    // that there are cells in each cellmap location.
    CollapseSelectionToStartOf(*lastCellNode, ignoredError);
    if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "HTMLEditor::CollapseSelectionToStartOf() failed, but ignored");
    ignoredError.SuppressException();
    rv = InsertTableCellsWithTransaction(aNumberOfColumnsToInsert,
                                         InsertPosition::eAfterSelectedCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::InsertTableCellsWithTransaction() "
                         "failed, but might be ignored");
  }
  // XXX This is perhaps the result of the last call of
  //     InsertTableCellsWithTransaction().
  return rv;
}

NS_IMETHODIMP HTMLEditor::InsertTableRow(int32_t aNumberOfRowsToInsert,
                                         bool aInsertAfterSelectedCell) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eInsertTableRowElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = InsertTableRowsWithTransaction(
      aNumberOfRowsToInsert, aInsertAfterSelectedCell
                                 ? InsertPosition::eAfterSelectedCell
                                 : InsertPosition::eBeforeSelectedCell);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::InsertTableRowsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::InsertTableRowsWithTransaction(
    int32_t aNumberOfRowsToInsert, InsertPosition aInsertPosition) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> curCell;

  int32_t startRowIndex, startColIndex;
  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(curCell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!table || !curCell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if no cell found.
    return NS_OK;
  }

  // Get more data for current cell in row we are inserting at because we need
  // colspan.
  IgnoredErrorResult ignoredError;
  CellData cellDataAtSelection(*this, *table, startRowIndex, startColIndex,
                               ignoredError);
  ignoredError.SuppressException();
  if (cellDataAtSelection.FailedOrNotFound()) {
    NS_WARNING("CellData couldn't find selected cell");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(curCell == cellDataAtSelection.mElement);

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }
  // Should not be empty since we've already found a cell.
  MOZ_ASSERT(!tableSize.IsEmpty());

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of BR in new cell until we're done
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  switch (aInsertPosition) {
    case InsertPosition::eBeforeSelectedCell:
      break;
    case InsertPosition::eAfterSelectedCell:
      // Use row after current cell.
      startRowIndex += cellDataAtSelection.mEffectiveRowSpan;

      // Detect when user is adding after a rowspan=0 case.
      // Assume they want to stop the "0" behavior and really add a new row.
      // Thus we set the rowspan to its true value.
      if (!cellDataAtSelection.mRowSpan) {
        DebugOnly<nsresult> rvIgnored =
            SetRowSpan(MOZ_KnownLive(cellDataAtSelection.mElement),
                       cellDataAtSelection.mEffectiveRowSpan);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                             "HTMLEditor::SetRowSpan() failed, but ignored");
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid InsertPosition");
  }

  // We control selection resetting after the insert.
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, cellDataAtSelection.mCurrent.mColumn,
      ePreviousColumn, false);
  // Suppress Rules System selection munging.
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  RefPtr<Element> cellForRowParent;
  int32_t cellsInRow = 0;
  if (startRowIndex < tableSize.mRowCount) {
    // We are inserting above an existing row.  Get each cell in the insert
    // row to adjust for colspan effects while we count how many cells are
    // needed.
    CellData cellData;
    for (int32_t colIndex = 0;; colIndex = cellData.NextColumnIndex()) {
      cellData.Update(*this, *table, startRowIndex, colIndex, ignoredError);
      if (cellData.FailedOrNotFound()) {
        break;  // Perhaps, we reach end of the row.
      }

      // XXX So, this is impossible case. Will be removed.
      if (!cellData.mElement) {
        NS_WARNING("CellData::Update() succeeded, but didn't set mElement");
        break;
      }

      if (cellData.IsSpannedFromOtherRow()) {
        // We have a cell spanning this location.  Increase its rowspan.
        // Note that if rowspan is 0, we do nothing since that cell should
        // automatically extend into the new row.
        if (cellData.mRowSpan > 0) {
          DebugOnly<nsresult> rvIgnored =
              SetRowSpan(MOZ_KnownLive(cellData.mElement),
                         cellData.mRowSpan + aNumberOfRowsToInsert);
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                               "HTMLEditor::SetRowSpan() failed, but ignored");
        }
        continue;
      }

      cellsInRow += cellData.mEffectiveColSpan;
      if (!cellForRowParent) {
        // FYI: Don't use std::move() here since NextColumnIndex() needs it.
        cellForRowParent = cellData.mElement;
      }

      MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
    }
  } else {
    // We are adding a new row after all others.  If it weren't for colspan=0
    // effect,  we could simply use tableSize.mColumnCount for number of new
    // cells...
    // XXX colspan=0 support has now been removed in table layout so maybe this
    //     can be cleaned up now? (bug 1243183)
    cellsInRow = tableSize.mColumnCount;

    // but we must compensate for all cells with rowspan = 0 in the last row.
    const int32_t kLastRowIndex = tableSize.mRowCount - 1;
    CellData cellData;
    for (int32_t colIndex = 0;; colIndex = cellData.NextColumnIndex()) {
      cellData.Update(*this, *table, kLastRowIndex, colIndex, ignoredError);
      if (cellData.FailedOrNotFound()) {
        break;  // Perhaps, we reach end of the row.
      }

      if (!cellData.mRowSpan) {
        MOZ_ASSERT(cellsInRow >= cellData.mEffectiveColSpan);
        cellsInRow -= cellData.mEffectiveColSpan;
      }

      // Save cell from the last row that we will use below
      if (!cellForRowParent && !cellData.IsSpannedFromOtherRow()) {
        // FYI: Don't use std::move() here since NextColumnIndex() needs it.
        cellForRowParent = cellData.mElement;
      }

      MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
    }
  }

  if (!cellsInRow) {
    NS_WARNING("There was no cell element in the last row");
    return NS_OK;
  }

  if (!cellForRowParent) {
    NS_WARNING("There was no cell element for the <tr> element");
    return NS_ERROR_FAILURE;
  }
  Element* parentRow =
      GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::tr, *cellForRowParent);
  if (!parentRow) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::tr) "
        "failed");
    return NS_ERROR_FAILURE;
  }

  // The row parent and offset where we will insert new row.
  EditorDOMPoint pointToInsert(parentRow);
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  // Adjust for when adding past the end.
  if (aInsertPosition == InsertPosition::eAfterSelectedCell &&
      startRowIndex >= tableSize.mRowCount) {
    DebugOnly<bool> advanced = pointToInsert.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced, "Failed to advance offset");
  }

  for (int32_t row = 0; row < aNumberOfRowsToInsert; row++) {
    // Create a new row
    RefPtr<Element> newRow = CreateElementWithDefaults(*nsGkAtoms::tr);
    if (!newRow) {
      NS_WARNING("HTMLEditor::CreateElementWithDefaults(nsGkAtoms::tr) failed");
      return NS_ERROR_FAILURE;
    }

    for (int32_t i = 0; i < cellsInRow; i++) {
      RefPtr<Element> newCell = CreateElementWithDefaults(*nsGkAtoms::td);
      if (!newCell) {
        NS_WARNING(
            "HTMLEditor::CreateElementWithDefaults(nsGkAtoms::td) failed");
        return NS_ERROR_FAILURE;
      }
      newRow->AppendChild(*newCell, error);
      if (error.Failed()) {
        NS_WARNING("nsINode::AppendChild() failed");
        return error.StealNSResult();
      }
    }

    AutoEditorDOMPointChildInvalidator lockOffset(pointToInsert);
    nsresult rv = InsertNodeWithTransaction(*newRow, pointToInsert);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return rv;
    }
  }

  // SetSelectionAfterTableEdit from AutoSelectionSetterAfterTableEdit will
  // access frame selection, so we need reframe.
  // Because GetTableCellElementAt() depends on frame.
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(FlushType::Frames);
  }

  return NS_OK;
}

nsresult HTMLEditor::DeleteTableElementAndChildrenWithTransaction(
    Element& aTableElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Block selectionchange event.  It's enough to dispatch selectionchange
  // event immediately after removing the table element.
  {
    AutoHideSelectionChanges hideSelection(SelectionRef());

    // Select the <table> element after clear current selection.
    if (SelectionRef().RangeCount()) {
      ErrorResult error;
      SelectionRef().RemoveAllRanges(error);
      if (error.Failed()) {
        NS_WARNING("Selection::RemoveAllRanges() failed");
        return error.StealNSResult();
      }
    }

    RefPtr<nsRange> range = nsRange::Create(&aTableElement);
    ErrorResult error;
    range->SelectNode(aTableElement, error);
    if (error.Failed()) {
      NS_WARNING("nsRange::SelectNode() failed");
      return error.StealNSResult();
    }
    SelectionRef().AddRangeAndSelectFramesAndNotifyListeners(*range, error);
    if (error.Failed()) {
      NS_WARNING(
          "Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
      return error.StealNSResult();
    }

#ifdef DEBUG
    range = SelectionRef().GetRangeAt(0);
    MOZ_ASSERT(range);
    MOZ_ASSERT(range->GetStartContainer() == aTableElement.GetParent());
    MOZ_ASSERT(range->GetEndContainer() == aTableElement.GetParent());
    MOZ_ASSERT(range->GetChildAtStartOffset() == &aTableElement);
    MOZ_ASSERT(range->GetChildAtEndOffset() == aTableElement.GetNextSibling());
#endif  // #ifdef DEBUG
  }

  nsresult rv = DeleteSelectionAsSubAction(eNext, eStrip);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::DeleteSelectionAsSubAction(eNext, eStrip) failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::DeleteTable() {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eRemoveTableElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<Element> table;
  rv = GetCellContext(getter_AddRefs(table), nullptr, nullptr, nullptr, nullptr,
                      nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!table) {
    NS_WARNING("HTMLEditor::GetCellContext() didn't return <table> element");
    return NS_ERROR_FAILURE;
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  rv = DeleteTableElementAndChildrenWithTransaction(*table);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::DeleteTableCell(int32_t aNumberOfCellsToDelete) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eRemoveTableCellElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteTableCellWithTransaction(aNumberOfCellsToDelete);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DeleteTableCellWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DeleteTableCellWithTransaction(
    int32_t aNumberOfCellsToDelete) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> cell;
  int32_t startRowIndex, startColIndex;

  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!table || !cell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if we didn't find a table or cell.
    return NS_OK;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  MOZ_ASSERT(SelectionRef().RangeCount());

  SelectedTableCellScanner scanner(SelectionRef());

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  MOZ_ASSERT(!tableSize.IsEmpty());

  // If only one cell is selected or no cell is selected, remove cells
  // starting from the first selected cell or a cell containing first
  // selection range.
  if (!scanner.IsInTableCellSelectionMode() ||
      SelectionRef().RangeCount() == 1) {
    for (int32_t i = 0; i < aNumberOfCellsToDelete; i++) {
      nsresult rv =
          GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                         nullptr, &startRowIndex, &startColIndex);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::GetCellContext() failed");
        return rv;
      }
      if (!table || !cell) {
        NS_WARNING(
            "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
        // Don't fail if no cell found
        return NS_OK;
      }

      int32_t numberOfCellsInRow = GetNumberOfCellsInRow(*table, startRowIndex);
      NS_WARNING_ASSERTION(
          numberOfCellsInRow >= 0,
          "HTMLEditor::GetNumberOfCellsInRow() failed, but ignored");

      if (numberOfCellsInRow == 1) {
        // Remove <tr> or <table> if we're removing all cells in the row or
        // the table.
        if (tableSize.mRowCount == 1) {
          nsresult rv = DeleteTableElementAndChildrenWithTransaction(*table);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() "
              "failed");
          return rv;
        }

        // We need to call DeleteSelectedTableRowsWithTransaction() to handle
        // cells with rowspan attribute.
        rv = DeleteSelectedTableRowsWithTransaction(1);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "HTMLEditor::DeleteSelectedTableRowsWithTransaction(1) failed");
          return rv;
        }

        // Adjust table rows simply.  In strictly speaking, we should
        // recompute table size with the latest layout information since
        // mutation event listener may have changed the DOM tree. However,
        // this is not in usual path of Firefox.  So, we can assume that
        // there are no mutation event listeners.
        MOZ_ASSERT(tableSize.mRowCount);
        tableSize.mRowCount--;
        continue;
      }

      // The setCaret object will call AutoSelectionSetterAfterTableEdit in its
      // destructor
      AutoSelectionSetterAfterTableEdit setCaret(
          *this, table, startRowIndex, startColIndex, ePreviousColumn, false);
      AutoTransactionsConserveSelection dontChangeSelection(*this);

      // XXX Removing cell element causes not adjusting colspan.
      rv = DeleteNodeWithTransaction(*cell);
      // If we fail, don't try to delete any more cells???
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      // Note that we don't refer column number in this loop.  So, it must
      // be safe not to recompute table size since number of row is synced
      // above.
    }
    return NS_OK;
  }

  // When 2 or more cells are selected, ignore aNumberOfCellsToRemove and
  // remove all selected cells.
  const RefPtr<PresShell> presShell{GetPresShell()};
  // `MOZ_KnownLive(scanner.ElementsRef()[0])` is safe because scanner grabs
  // it until it's destroyed later.
  CellIndexes firstCellIndexes(MOZ_KnownLive(scanner.ElementsRef()[0]),
                               presShell, error);
  if (error.Failed()) {
    NS_WARNING("CellIndexes failed");
    return error.StealNSResult();
  }
  startRowIndex = firstCellIndexes.mRow;
  startColIndex = firstCellIndexes.mColumn;

  // The setCaret object will call AutoSelectionSetterAfterTableEdit in its
  // destructor
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, startColIndex, ePreviousColumn, false);
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  bool checkToDeleteRow = true;
  bool checkToDeleteColumn = true;
  for (RefPtr<Element> selectedCellElement = scanner.GetFirstElement();
       selectedCellElement;) {
    if (checkToDeleteRow) {
      // Optimize to delete an entire row
      // Clear so we don't repeat AllCellsInRowSelected within the same row
      checkToDeleteRow = false;
      if (AllCellsInRowSelected(table, startRowIndex, tableSize.mColumnCount)) {
        // First, find the next cell in a different row to continue after we
        // delete this row.
        int32_t nextRow = startRowIndex;
        while (nextRow == startRowIndex) {
          selectedCellElement = scanner.GetNextElement();
          if (!selectedCellElement) {
            break;
          }
          CellIndexes nextSelectedCellIndexes(*selectedCellElement, presShell,
                                              error);
          if (error.Failed()) {
            NS_WARNING("CellIndexes failed");
            return error.StealNSResult();
          }
          nextRow = nextSelectedCellIndexes.mRow;
          startColIndex = nextSelectedCellIndexes.mColumn;
        }
        if (tableSize.mRowCount == 1) {
          nsresult rv = DeleteTableElementAndChildrenWithTransaction(*table);
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() "
              "failed");
          return rv;
        }
        nsresult rv = DeleteTableRowWithTransaction(*table, startRowIndex);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteTableRowWithTransaction() failed");
          return rv;
        }
        // Adjust table rows simply.  In strictly speaking, we should
        // recompute table size with the latest layout information since
        // mutation event listener may have changed the DOM tree. However,
        // this is not in usual path of Firefox.  So, we can assume that
        // there are no mutation event listeners.
        MOZ_ASSERT(tableSize.mRowCount);
        tableSize.mRowCount--;
        if (!selectedCellElement) {
          break;  // XXX Seems like a dead path
        }
        // For the next cell: Subtract 1 for row we deleted
        startRowIndex = nextRow - 1;
        // Set true since we know we will look at a new row next
        checkToDeleteRow = true;
        continue;
      }
    }

    if (checkToDeleteColumn) {
      // Optimize to delete an entire column
      // Clear this so we don't repeat AllCellsInColSelected within the same Col
      checkToDeleteColumn = false;
      if (AllCellsInColumnSelected(table, startColIndex,
                                   tableSize.mColumnCount)) {
        // First, find the next cell in a different column to continue after
        // we delete this column.
        int32_t nextCol = startColIndex;
        while (nextCol == startColIndex) {
          selectedCellElement = scanner.GetNextElement();
          if (!selectedCellElement) {
            break;
          }
          CellIndexes nextSelectedCellIndexes(*selectedCellElement, presShell,
                                              error);
          if (error.Failed()) {
            NS_WARNING("CellIndexes failed");
            return error.StealNSResult();
          }
          startRowIndex = nextSelectedCellIndexes.mRow;
          nextCol = nextSelectedCellIndexes.mColumn;
        }
        // Delete all cells which belong to the column.
        nsresult rv = DeleteTableColumnWithTransaction(*table, startColIndex);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteTableColumnWithTransaction() failed");
          return rv;
        }
        // Adjust table columns simply.  In strictly speaking, we should
        // recompute table size with the latest layout information since
        // mutation event listener may have changed the DOM tree. However,
        // this is not in usual path of Firefox.  So, we can assume that
        // there are no mutation event listeners.
        MOZ_ASSERT(tableSize.mColumnCount);
        tableSize.mColumnCount--;
        if (!selectedCellElement) {
          break;
        }
        // For the next cell, subtract 1 for col. deleted
        startColIndex = nextCol - 1;
        // Set true since we know we will look at a new column next
        checkToDeleteColumn = true;
        continue;
      }
    }

    nsresult rv = DeleteNodeWithTransaction(*selectedCellElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
      return rv;
    }

    selectedCellElement = scanner.GetNextElement();
    if (!selectedCellElement) {
      return NS_OK;
    }

    CellIndexes nextCellIndexes(*selectedCellElement, presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return error.StealNSResult();
    }
    startRowIndex = nextCellIndexes.mRow;
    startColIndex = nextCellIndexes.mColumn;
    // When table cell is removed, table size of column may be changed.
    // For example, if there are 2 rows, one has 2 cells, the other has
    // 3 cells, tableSize.mColumnCount is 3.  When this removes a cell
    // in the latter row, mColumnCount should be come 2.  However, we
    // don't use mColumnCount in this loop, so, this must be okay for now.
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::DeleteTableCellContents() {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eDeleteTableCellContents);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteTableCellContentsWithTransaction();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteTableCellContentsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DeleteTableCellContentsWithTransaction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> cell;
  int32_t startRowIndex, startColIndex;
  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!cell) {
    NS_WARNING("HTMLEditor::GetCellContext() didn't return cell element");
    // Don't fail if no cell found.
    return NS_OK;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Don't let Rules System change the selection
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  SelectedTableCellScanner scanner(SelectionRef());
  if (scanner.IsInTableCellSelectionMode()) {
    const RefPtr<PresShell> presShell{GetPresShell()};
    // `MOZ_KnownLive(scanner.ElementsRef()[0])` is safe because scanner
    // grabs it until it's destroyed later.
    ErrorResult error;
    CellIndexes firstCellIndexes(MOZ_KnownLive(scanner.ElementsRef()[0]),
                                 presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return error.StealNSResult();
    }
    cell = scanner.ElementsRef()[0];
    startRowIndex = firstCellIndexes.mRow;
    startColIndex = firstCellIndexes.mColumn;
  }

  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, startColIndex, ePreviousColumn, false);

  for (RefPtr<Element> selectedCellElement = std::move(cell);
       selectedCellElement; selectedCellElement = scanner.GetNextElement()) {
    DebugOnly<nsresult> rvIgnored =
        DeleteAllChildrenWithTransaction(*selectedCellElement);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteAllChildrenWithTransaction() failed, but ignored");
    if (!scanner.IsInTableCellSelectionMode()) {
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::DeleteTableColumn(int32_t aNumberOfColumnsToDelete) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eRemoveTableColumn);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteSelectedTableColumnsWithTransaction(aNumberOfColumnsToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteSelectedTableColumnsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DeleteSelectedTableColumnsWithTransaction(
    int32_t aNumberOfColumnsToDelete) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> cell;
  int32_t startRowIndex, startColIndex;
  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!table || !cell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if no cell found.
    return NS_OK;
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);

  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Shortcut the case of deleting all columns in table
  if (!startColIndex && aNumberOfColumnsToDelete >= tableSize.mColumnCount) {
    nsresult rv = DeleteTableElementAndChildrenWithTransaction(*table);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() failed");
    return rv;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  SelectedTableCellScanner scanner(SelectionRef());
  if (scanner.IsInTableCellSelectionMode() && SelectionRef().RangeCount() > 1) {
    const RefPtr<PresShell> presShell{GetPresShell()};
    // `MOZ_KnownLive(scanner.ElementsRef()[0])` is safe because `scanner`
    // grabs it until it's destroyed later.
    CellIndexes firstCellIndexes(MOZ_KnownLive(scanner.ElementsRef()[0]),
                                 presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return error.StealNSResult();
    }
    startRowIndex = firstCellIndexes.mRow;
    startColIndex = firstCellIndexes.mColumn;
  }

  // We control selection resetting after the insert...
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, startColIndex, ePreviousRow, false);

  // If 2 or more cells are not selected, removing columns starting from
  // a column which contains first selection range.
  if (!scanner.IsInTableCellSelectionMode() ||
      SelectionRef().RangeCount() == 1) {
    int32_t columnCountToRemove = std::min(
        aNumberOfColumnsToDelete, tableSize.mColumnCount - startColIndex);
    for (int32_t i = 0; i < columnCountToRemove; i++) {
      nsresult rv = DeleteTableColumnWithTransaction(*table, startColIndex);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteTableColumnWithTransaction() failed");
        return rv;
      }
    }
    return NS_OK;
  }

  // If 2 or more cells are selected, remove all columns which contain selected
  // cells.  I.e., we ignore aNumberOfColumnsToDelete in this case.
  const RefPtr<PresShell> presShell{GetPresShell()};
  for (RefPtr<Element> selectedCellElement = scanner.GetFirstElement();
       selectedCellElement;) {
    if (selectedCellElement != scanner.ElementsRef()[0]) {
      CellIndexes cellIndexes(*selectedCellElement, presShell, error);
      if (error.Failed()) {
        NS_WARNING("CellIndexes failed");
        return error.StealNSResult();
      }
      startRowIndex = cellIndexes.mRow;
      startColIndex = cellIndexes.mColumn;
    }
    // Find the next cell in a different column
    // to continue after we delete this column
    int32_t nextCol = startColIndex;
    while (nextCol == startColIndex) {
      selectedCellElement = scanner.GetNextElement();
      if (!selectedCellElement) {
        break;
      }
      CellIndexes cellIndexes(*selectedCellElement, presShell, error);
      if (error.Failed()) {
        NS_WARNING("CellIndexes failed");
        return error.StealNSResult();
      }
      startRowIndex = cellIndexes.mRow;
      nextCol = cellIndexes.mColumn;
    }
    nsresult rv = DeleteTableColumnWithTransaction(*table, startColIndex);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTableColumnWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::DeleteTableColumnWithTransaction(Element& aTableElement,
                                                      int32_t aColumnIndex) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // XXX Why don't this method remove proper <col> (and <colgroup>)?
  ErrorResult error;
  IgnoredErrorResult ignoredError;
  for (int32_t rowIndex = 0;; rowIndex++) {
    CellData cellData(*this, aTableElement, rowIndex, aColumnIndex,
                      ignoredError);
    // Failure means that there is no more row in the table.  In this case,
    // we shouldn't return error since we just reach the end of the table.
    // XXX Should distinguish whether CellData returns error or just not found
    //     later.
    if (cellData.FailedOrNotFound()) {
      return NS_OK;
    }

    // Find cells that don't start in column we are deleting.
    MOZ_ASSERT(cellData.mColSpan >= 0);
    if (cellData.IsSpannedFromOtherColumn() || cellData.mColSpan != 1) {
      // If we have a cell spanning this location, decrease its colspan to
      // keep table rectangular, but if colspan is 0, it'll be adjusted
      // automatically.
      if (cellData.mColSpan > 0) {
        NS_WARNING_ASSERTION(cellData.mColSpan > 1,
                             "colspan should be 2 or larger");
        DebugOnly<nsresult> rvIgnored =
            SetColSpan(MOZ_KnownLive(cellData.mElement), cellData.mColSpan - 1);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                             "HTMLEditor::SetColSpan() failed, but ignored");
      }
      if (!cellData.IsSpannedFromOtherColumn()) {
        // Cell is in column to be deleted, but must have colspan > 1,
        // so delete contents of cell instead of cell itself (We must have
        // reset colspan above).
        DebugOnly<nsresult> rvIgnored =
            DeleteAllChildrenWithTransaction(MOZ_KnownLive(*cellData.mElement));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                             "HTMLEditor::DeleteAllChildrenWithTransaction() "
                             "failed, but ignored");
      }
      // Skip rows which the removed cell spanned.
      rowIndex += cellData.NumberOfFollowingRows();
      continue;
    }

    // Delete the cell
    int32_t numberOfCellsInRow =
        GetNumberOfCellsInRow(aTableElement, cellData.mCurrent.mRow);
    NS_WARNING_ASSERTION(
        numberOfCellsInRow > 0,
        "HTMLEditor::GetNumberOfCellsInRow() failed, but ignored");
    if (numberOfCellsInRow != 1) {
      // If removing cell is not the last cell of the row, we can just remove
      // it.
      nsresult rv =
          DeleteNodeWithTransaction(MOZ_KnownLive(*cellData.mElement));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      // Skip rows which the removed cell spanned.
      rowIndex += cellData.NumberOfFollowingRows();
      continue;
    }

    // When the cell is the last cell in the row, remove the row instead.
    Element* parentRow = GetInclusiveAncestorByTagNameInternal(
        *nsGkAtoms::tr, *cellData.mElement);
    if (!parentRow) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::tr) "
          "failed");
      return NS_ERROR_FAILURE;
    }

    // Check if its the only row left in the table.  If so, we can delete
    // the table instead.
    TableSize tableSize(*this, aTableElement, error);
    if (error.Failed()) {
      NS_WARNING("TableSize failed");
      return error.StealNSResult();
    }

    if (tableSize.mRowCount == 1) {
      // We're deleting the last row.  So, let's remove the <table> now.
      nsresult rv = DeleteTableElementAndChildrenWithTransaction(aTableElement);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() failed");
      return rv;
    }

    // Delete the row by placing caret in cell we were to delete.  We need
    // to call DeleteTableRowWithTransaction() to handle cells with rowspan.
    nsresult rv =
        DeleteTableRowWithTransaction(aTableElement, cellData.mFirst.mRow);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTableRowWithTransaction() failed");
      return rv;
    }

    // Note that we decrement rowIndex since a row was deleted.
    rowIndex--;
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::DeleteTableRow(int32_t aNumberOfRowsToDelete) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eRemoveTableRowElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = DeleteSelectedTableRowsWithTransaction(aNumberOfRowsToDelete);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditor::DeleteSelectedTableRowsWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::DeleteSelectedTableRowsWithTransaction(
    int32_t aNumberOfRowsToDelete) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> table;
  RefPtr<Element> cell;
  int32_t startRowIndex, startColIndex;
  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return rv;
  }
  if (!table || !cell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    // Don't fail if no cell found.
    return NS_OK;
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);

  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Shortcut the case of deleting all rows in table
  if (!startRowIndex && aNumberOfRowsToDelete >= tableSize.mRowCount) {
    nsresult rv = DeleteTableElementAndChildrenWithTransaction(*table);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::DeleteTableElementAndChildrenWithTransaction() failed");
    return rv;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  SelectedTableCellScanner scanner(SelectionRef());
  if (scanner.IsInTableCellSelectionMode() && SelectionRef().RangeCount() > 1) {
    // Fetch indexes again - may be different for selected cells
    const RefPtr<PresShell> presShell{GetPresShell()};
    // `MOZ_KnownLive(scanner.ElementsRef()[0])` is safe because `scanner`
    // grabs it until it's destroyed later.
    CellIndexes firstCellIndexes(MOZ_KnownLive(scanner.ElementsRef()[0]),
                                 presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return error.StealNSResult();
    }
    startRowIndex = firstCellIndexes.mRow;
    startColIndex = firstCellIndexes.mColumn;
  }

  // We control selection resetting after the insert...
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, startColIndex, ePreviousRow, false);
  // Don't change selection during deletions
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  // XXX Perhaps, the following loops should collect <tr> elements to remove
  //     first, then, remove them from the DOM tree since mutation event
  //     listener may change the DOM tree during the loops.

  // If 2 or more cells are not selected, removing rows starting from
  // a row which contains first selection range.
  if (!scanner.IsInTableCellSelectionMode() ||
      SelectionRef().RangeCount() == 1) {
    int32_t rowCountToRemove =
        std::min(aNumberOfRowsToDelete, tableSize.mRowCount - startRowIndex);
    for (int32_t i = 0; i < rowCountToRemove; i++) {
      nsresult rv = DeleteTableRowWithTransaction(*table, startRowIndex);
      // If failed in current row, try the next
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "HTMLEditor::DeleteTableRowWithTransaction() failed, but trying "
            "next...");
        startRowIndex++;
      }
      // Check if there's a cell in the "next" row.
      cell = GetTableCellElementAt(*table, startRowIndex, startColIndex);
      if (!cell) {
        return NS_OK;
      }
    }
    return NS_OK;
  }

  // If 2 or more cells are selected, remove all rows which contain selected
  // cells.  I.e., we ignore aNumberOfRowsToDelete in this case.
  const RefPtr<PresShell> presShell{GetPresShell()};
  for (RefPtr<Element> selectedCellElement = scanner.GetFirstElement();
       selectedCellElement;) {
    if (selectedCellElement != scanner.ElementsRef()[0]) {
      CellIndexes cellIndexes(*selectedCellElement, presShell, error);
      if (error.Failed()) {
        NS_WARNING("CellIndexes failed");
        return error.StealNSResult();
      }
      startRowIndex = cellIndexes.mRow;
      startColIndex = cellIndexes.mColumn;
    }
    // Find the next cell in a different row
    // to continue after we delete this row
    int32_t nextRow = startRowIndex;
    while (nextRow == startRowIndex) {
      selectedCellElement = scanner.GetNextElement();
      if (!selectedCellElement) {
        break;
      }
      CellIndexes cellIndexes(*selectedCellElement, presShell, error);
      if (error.Failed()) {
        NS_WARNING("CellIndexes failed");
        return error.StealNSResult();
      }
      nextRow = cellIndexes.mRow;
      startColIndex = cellIndexes.mColumn;
    }
    // Delete the row containing selected cell(s).
    nsresult rv = DeleteTableRowWithTransaction(*table, startRowIndex);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::DeleteTableRowWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

// Helper that doesn't batch or change the selection
nsresult HTMLEditor::DeleteTableRowWithTransaction(Element& aTableElement,
                                                   int32_t aRowIndex) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  ErrorResult error;
  TableSize tableSize(*this, aTableElement, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");
  ignoredError.SuppressException();

  // Scan through cells in row to do rowspan adjustments
  // Note that after we delete row, startRowIndex will point to the cells in
  // the next row to be deleted.

  // The list of cells we will change rowspan in and the new rowspan values
  // for each.
  struct MOZ_STACK_CLASS SpanCell final {
    RefPtr<Element> mElement;
    int32_t mNewRowSpanValue;

    SpanCell(Element* aSpanCellElement, int32_t aNewRowSpanValue)
        : mElement(aSpanCellElement), mNewRowSpanValue(aNewRowSpanValue) {}
  };
  AutoTArray<SpanCell, 10> spanCellArray;
  RefPtr<Element> cellInDeleteRow;
  int32_t columnIndex = 0;
  while (aRowIndex < tableSize.mRowCount &&
         columnIndex < tableSize.mColumnCount) {
    CellData cellData(*this, aTableElement, aRowIndex, columnIndex,
                      ignoredError);
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData failed");
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // XXX So, we should distinguish if CellDate returns error or just not
    //     found later.
    if (!cellData.mElement) {
      break;
    }

    // Compensate for cells that don't start or extend below the row we are
    // deleting.
    if (cellData.IsSpannedFromOtherRow()) {
      // If a cell starts in row above us, decrease its rowspan to keep table
      // rectangular but we don't need to do this if rowspan=0, since it will
      // be automatically adjusted.
      if (cellData.mRowSpan > 0) {
        // Build list of cells to change rowspan.  We can't do it now since
        // it upsets cell map, so we will do it after deleting the row.
        int32_t newRowSpanValue = std::max(cellData.NumberOfPrecedingRows(),
                                           cellData.NumberOfFollowingRows());
        spanCellArray.AppendElement(
            SpanCell(cellData.mElement, newRowSpanValue));
      }
    } else {
      if (cellData.mRowSpan > 1) {
        // Cell spans below row to delete, so we must insert new cells to
        // keep rows below.  Note that we test "rowSpan" so we don't do this
        // if rowSpan = 0 (automatic readjustment).
        int32_t aboveRowToInsertNewCellInto =
            cellData.NumberOfPrecedingRows() + 1;
        nsresult rv = SplitCellIntoRows(
            &aTableElement, cellData.mFirst.mRow, cellData.mFirst.mColumn,
            aboveRowToInsertNewCellInto, cellData.NumberOfFollowingRows(),
            nullptr);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::SplitCellIntoRows() failed");
          return rv;
        }
      }
      if (!cellInDeleteRow) {
        // Reference cell to find row to delete.
        cellInDeleteRow = std::move(cellData.mElement);
      }
    }
    // Skip over other columns spanned by this cell
    columnIndex += cellData.mEffectiveColSpan;
  }

  // Things are messed up if we didn't find a cell in the row!
  if (!cellInDeleteRow) {
    NS_WARNING("There was no cell in deleting row");
    return NS_ERROR_FAILURE;
  }

  // Delete the entire row.
  RefPtr<Element> parentRow =
      GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::tr, *cellInDeleteRow);
  if (parentRow) {
    nsresult rv = DeleteNodeWithTransaction(*parentRow);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::tr) "
          "failed");
      return rv;
    }
  }

  // Now we can set new rowspans for cells stored above.
  for (SpanCell& spanCell : spanCellArray) {
    if (NS_WARN_IF(!spanCell.mElement)) {
      continue;
    }
    nsresult rv =
        SetRowSpan(MOZ_KnownLive(spanCell.mElement), spanCell.mNewRowSpanValue);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetRawSpan() failed");
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::SelectTable() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> table =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
  if (!table) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::table)"
        " failed");
    return NS_OK;  // Don't fail if we didn't find a table.
  }

  nsresult rv = ClearSelection();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::ClearSelection() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  rv = AppendNodeToSelectionAsRange(table);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::AppendNodeToSelectionAsRange() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SelectTableCell() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> cell =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cell) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::td) "
        "failed");
    // Don't fail if we didn't find a cell.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  nsresult rv = ClearSelection();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::ClearSelection() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  rv = AppendNodeToSelectionAsRange(cell);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::AppendNodeToSelectionAsRange() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SelectAllTableCells() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> cell =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cell) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::td) "
        "failed");
    // Don't fail if we didn't find a cell.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  RefPtr<Element> startCell = cell;

  // Get parent table
  RefPtr<Element> table =
      GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::table, *cell);
  if (!table) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
        "failed");
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }

  // Suppress nsISelectionListener notification
  // until all selection changes are finished
  SelectionBatcher selectionBatcher(SelectionRef());

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  nsresult rv = ClearSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::ClearSelection() failed, but might be ignored");

  // Select all cells in the same column as current cell
  bool cellSelected = false;
  IgnoredErrorResult ignoredError;
  for (int32_t row = 0; row < tableSize.mRowCount; row++) {
    CellData cellData;
    for (int32_t col = 0; col < tableSize.mColumnCount;
         col = cellData.NextColumnIndex()) {
      cellData.Update(*this, *table, row, col, ignoredError);
      if (cellData.FailedOrNotFound()) {
        NS_WARNING("CellData::Update() failed, but might be ignored");
        rv = NS_ERROR_FAILURE;
        break;
      }
      MOZ_ASSERT(!ignoredError.Failed());

      // Skip cells that are spanned from previous rows or columns
      // XXX So, we should distinguish whether CellData returns error or just
      //     not found later.
      if (cellData.mElement && !cellData.IsSpannedFromOtherRowOrColumn()) {
        nsresult rv = AppendNodeToSelectionAsRange(cellData.mElement);
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "EditorBase::AppendNodeToSelectionAsRange() failed, but might be "
              "ignored");
          break;
        }
        cellSelected = true;
      }
      MOZ_ASSERT(col < cellData.NextColumnIndex());
    }
  }

  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected) {
    // XXX In this case, we ignore `NS_ERROR_FAILURE` set by above inner
    //     `for` loop.
    nsresult rv = AppendNodeToSelectionAsRange(startCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::AppendNodeToSelectionAsRange() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::ClearSelection() or CellData::Update() or "
                       "EditorBase::AppendNodeToSelectionAsRange() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SelectTableRow() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> cell =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cell) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::td) "
        "failed");
    // Don't fail if we didn't find a cell.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  RefPtr<Element> startCell = cell;

  // Get table and location of cell:
  RefPtr<Element> table;
  int32_t startRowIndex, startColIndex;

  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!table) {
    NS_WARNING("HTMLEditor::GetCellContext() didn't return <table> element");
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }

  // Note: At this point, we could get first and last cells in row,
  // then call SelectBlockOfCells, but that would take just
  // a little less code, so the following is more efficient

  // Suppress nsISelectionListener notification
  // until all selection changes are finished
  SelectionBatcher selectionBatcher(SelectionRef());

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  rv = ClearSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::ClearSelection() failed, but might be ignored");

  // Select all cells in the same row as current cell
  bool cellSelected = false;
  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t col = 0; col < tableSize.mColumnCount;
       col = cellData.NextColumnIndex()) {
    cellData.Update(*this, *table, startRowIndex, col, ignoredError);
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed, but might be ignored");
      rv = NS_ERROR_FAILURE;
      break;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // Skip cells that are spanned from previous rows or columns
    // XXX So, we should distinguish whether CellData returns error or just
    //     not found later.
    if (cellData.mElement && !cellData.IsSpannedFromOtherRowOrColumn()) {
      rv = AppendNodeToSelectionAsRange(cellData.mElement);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::AppendNodeToSelectionAsRange() failed, but ignored");
        break;
      }
      cellSelected = true;
    }
    MOZ_ASSERT(col < cellData.NextColumnIndex());
  }

  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected) {
    // XXX In this case, we ignore `NS_ERROR_FAILURE` set by above inner
    //     `for` loop.
    nsresult rv = AppendNodeToSelectionAsRange(startCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::AppendNodeToSelectionAsRange() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::ClearSelection() or CellData::Update() or "
                       "EditorBase::AppendNodeToSelectionAsRange() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SelectTableColumn() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> cell =
      GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cell) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::td) "
        "failed");
    // Don't fail if we didn't find a cell.
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  RefPtr<Element> startCell = cell;

  // Get location of cell:
  RefPtr<Element> table;
  int32_t startRowIndex, startColIndex;

  nsresult rv =
      GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                     nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!table) {
    NS_WARNING("HTMLEditor::GetCellContext() didn't return <table> element");
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }

  // Suppress nsISelectionListener notification
  // until all selection changes are finished
  SelectionBatcher selectionBatcher(SelectionRef());

  // It is now safe to clear the selection
  // BE SURE TO RESET IT BEFORE LEAVING!
  rv = ClearSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::ClearSelection() failed, but might be ignored");

  // Select all cells in the same column as current cell
  bool cellSelected = false;
  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t row = 0; row < tableSize.mRowCount;
       row = cellData.NextRowIndex()) {
    cellData.Update(*this, *table, row, startColIndex, ignoredError);
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed, but might be ignored");
      rv = NS_ERROR_FAILURE;
      break;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // Skip cells that are spanned from previous rows or columns
    // XXX So, we should distinguish whether CellData returns error or just
    //     not found later.
    if (cellData.mElement && !cellData.IsSpannedFromOtherRowOrColumn()) {
      rv = AppendNodeToSelectionAsRange(cellData.mElement);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "EditorBase::AppendNodeToSelectionAsRange() failed, but ignored");
        break;
      }
      cellSelected = true;
    }
    MOZ_ASSERT(row < cellData.NextRowIndex());
  }

  // Safety code to select starting cell if nothing else was selected
  if (!cellSelected) {
    nsresult rv = AppendNodeToSelectionAsRange(startCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::AppendNodeToSelectionAsRange() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::ClearSelection() or CellData::Update() or "
                       "EditorBase::AppendNodeToSelectionAsRange() failed");
  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP HTMLEditor::SplitTableCell() {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eSplitTableCellElement);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<Element> table;
  RefPtr<Element> cell;
  int32_t startRowIndex, startColIndex, actualRowSpan, actualColSpan;
  // Get cell, table, etc. at selection anchor node
  rv = GetCellContext(getter_AddRefs(table), getter_AddRefs(cell), nullptr,
                      nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!table || !cell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
  }

  // We need rowspan and colspan data
  rv = GetCellSpansAt(table, startRowIndex, startColIndex, actualRowSpan,
                      actualColSpan);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellSpansAt() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // Must have some span to split
  if (actualRowSpan <= 1 && actualColSpan <= 1) {
    return NS_OK;
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of BR in new cell until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // We reset selection
  AutoSelectionSetterAfterTableEdit setCaret(
      *this, table, startRowIndex, startColIndex, ePreviousColumn, false);
  //...so suppress Rules System selection munging
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  RefPtr<Element> newCell;
  int32_t rowIndex = startRowIndex;
  int32_t rowSpanBelow, colSpanAfter;

  // Split up cell row-wise first into rowspan=1 above, and the rest below,
  // whittling away at the cell below until no more extra span
  for (rowSpanBelow = actualRowSpan - 1; rowSpanBelow >= 0; rowSpanBelow--) {
    // We really split row-wise only if we had rowspan > 1
    if (rowSpanBelow > 0) {
      nsresult rv = SplitCellIntoRows(table, rowIndex, startColIndex, 1,
                                      rowSpanBelow, getter_AddRefs(newCell));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::SplitCellIntoRows() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
      DebugOnly<nsresult> rvIgnored = CopyCellBackgroundColor(newCell, cell);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::CopyCellBackgroundColor() failed, but ignored");
    }
    int32_t colIndex = startColIndex;
    // Now split the cell with rowspan = 1 into cells if it has colSpan > 1
    for (colSpanAfter = actualColSpan - 1; colSpanAfter > 0; colSpanAfter--) {
      nsresult rv = SplitCellIntoColumns(table, rowIndex, colIndex, 1,
                                         colSpanAfter, getter_AddRefs(newCell));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::SplitCellIntoColumns() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
      DebugOnly<nsresult> rvIgnored = CopyCellBackgroundColor(newCell, cell);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::CopyCellBackgroundColor() failed, but ignored");
      colIndex++;
    }
    // Point to the new cell and repeat
    rowIndex++;
  }
  return NS_OK;
}

nsresult HTMLEditor::CopyCellBackgroundColor(Element* aDestCell,
                                             Element* aSourceCell) {
  if (NS_WARN_IF(!aDestCell) || NS_WARN_IF(!aSourceCell)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aSourceCell->HasAttr(nsGkAtoms::bgcolor)) {
    return NS_OK;
  }

  // Copy backgournd color to new cell.
  nsString backgroundColor;
  aSourceCell->GetAttr(nsGkAtoms::bgcolor, backgroundColor);
  nsresult rv = SetAttributeWithTransaction(*aDestCell, *nsGkAtoms::bgcolor,
                                            backgroundColor);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::SetAttributeWithTransaction(nsGkAtoms::bgcolor) failed");
  return rv;
}

nsresult HTMLEditor::SplitCellIntoColumns(Element* aTable, int32_t aRowIndex,
                                          int32_t aColIndex,
                                          int32_t aColSpanLeft,
                                          int32_t aColSpanRight,
                                          Element** aNewCell) {
  if (NS_WARN_IF(!aTable)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aNewCell) {
    *aNewCell = nullptr;
  }

  IgnoredErrorResult ignoredError;
  CellData cellData(*this, *aTable, aRowIndex, aColIndex, ignoredError);
  if (cellData.FailedOrNotFound()) {
    NS_WARNING("CellData failed");
    return NS_ERROR_FAILURE;
  }

  // We can't split!
  if (cellData.mEffectiveColSpan <= 1 ||
      aColSpanLeft + aColSpanRight > cellData.mEffectiveColSpan) {
    return NS_OK;
  }

  // Reduce colspan of cell to split
  nsresult rv = SetColSpan(MOZ_KnownLive(cellData.mElement), aColSpanLeft);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::SetColSpan() failed");
    return rv;
  }

  // Insert new cell after using the remaining span
  // and always get the new cell so we can copy the background color;
  RefPtr<Element> newCellElement;
  rv = InsertCell(MOZ_KnownLive(cellData.mElement), cellData.mEffectiveRowSpan,
                  aColSpanRight, true, false, getter_AddRefs(newCellElement));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::InsertCell() failed");
    return rv;
  }
  if (!newCellElement) {
    return NS_OK;
  }
  if (aNewCell) {
    *aNewCell = do_AddRef(newCellElement).take();
  }
  rv =
      CopyCellBackgroundColor(newCellElement, MOZ_KnownLive(cellData.mElement));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CopyCellBackgroundColor() failed");
  return rv;
}

nsresult HTMLEditor::SplitCellIntoRows(Element* aTable, int32_t aRowIndex,
                                       int32_t aColIndex, int32_t aRowSpanAbove,
                                       int32_t aRowSpanBelow,
                                       Element** aNewCell) {
  if (NS_WARN_IF(!aTable)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aNewCell) {
    *aNewCell = nullptr;
  }

  IgnoredErrorResult ignoredError;
  CellData cellData(*this, *aTable, aRowIndex, aColIndex, ignoredError);
  if (cellData.FailedOrNotFound()) {
    NS_WARNING("CellData failed");
    return NS_ERROR_FAILURE;
  }

  // We can't split!
  if (cellData.mEffectiveRowSpan <= 1 ||
      aRowSpanAbove + aRowSpanBelow > cellData.mEffectiveRowSpan) {
    return NS_OK;
  }

  ErrorResult error;
  TableSize tableSize(*this, *aTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  // Find a cell to insert before or after
  RefPtr<Element> cellElementAtInsertionPoint;
  RefPtr<Element> lastCellFound;
  bool insertAfter = (cellData.mFirst.mColumn > 0);
  CellData cellDataAtInsertionPoint;
  for (int32_t colIndex = 0,
               rowBelowIndex = cellData.mFirst.mRow + aRowSpanAbove;
       colIndex <= tableSize.mColumnCount;
       colIndex = cellData.NextColumnIndex()) {
    cellDataAtInsertionPoint.Update(*this, *aTable, rowBelowIndex, colIndex,
                                    ignoredError);
    // If we fail here, it could be because row has bad rowspan values,
    // such as all cells having rowspan > 1 (Call FixRowSpan first!).
    // XXX According to the comment, this does not assume that
    //     FixRowSpan() doesn't work well and user can create non-rectangular
    //     table.  So, we should not return error when CellData cannot find
    //     a cell.
    if (cellDataAtInsertionPoint.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed");
      return NS_ERROR_FAILURE;
    }

    // FYI: Don't use std::move() here since the following checks will use
    //      utility methods of cellDataAtInsertionPoint, but some of them
    //      check whether its mElement is not nullptr.
    cellElementAtInsertionPoint = cellDataAtInsertionPoint.mElement;

    // Skip over cells spanned from above (like the one we are splitting!)
    if (cellDataAtInsertionPoint.mElement &&
        !cellDataAtInsertionPoint.IsSpannedFromOtherRow()) {
      if (!insertAfter) {
        // Inserting before, so stop at first cell in row we want to insert
        // into.
        break;
      }
      // New cell isn't first in row,
      // so stop after we find the cell just before new cell's column
      if (cellDataAtInsertionPoint.NextColumnIndex() ==
          cellData.mFirst.mColumn) {
        break;
      }
      // If cell found is AFTER desired new cell colum,
      // we have multiple cells with rowspan > 1 that
      // prevented us from finding a cell to insert after...
      if (cellDataAtInsertionPoint.mFirst.mColumn > cellData.mFirst.mColumn) {
        // ... so instead insert before the cell we found
        insertAfter = false;
        break;
      }
      // FYI: Don't use std::move() here since
      //      cellDataAtInsertionPoint.NextColumnIndex() needs it.
      lastCellFound = cellDataAtInsertionPoint.mElement;
    }
    MOZ_ASSERT(colIndex < cellDataAtInsertionPoint.NextColumnIndex());
  }

  if (!cellElementAtInsertionPoint && lastCellFound) {
    // Edge case where we didn't find a cell to insert after
    // or before because column(s) before desired column
    // and all columns after it are spanned from above.
    // We can insert after the last cell we found
    cellElementAtInsertionPoint = std::move(lastCellFound);
    insertAfter = true;  // Should always be true, but let's be sure
  }

  // Reduce rowspan of cell to split
  nsresult rv = SetRowSpan(MOZ_KnownLive(cellData.mElement), aRowSpanAbove);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::SetRowSpan() failed");
    return rv;
  }

  // Insert new cell after using the remaining span
  // and always get the new cell so we can copy the background color;
  RefPtr<Element> newCell;
  rv = InsertCell(cellElementAtInsertionPoint, aRowSpanBelow,
                  cellData.mEffectiveColSpan, insertAfter, false,
                  getter_AddRefs(newCell));
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::InsertCell() failed");
    return rv;
  }
  if (!newCell) {
    return NS_OK;
  }
  if (aNewCell) {
    *aNewCell = do_AddRef(newCell).take();
  }
  rv = CopyCellBackgroundColor(newCell, cellElementAtInsertionPoint);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::CopyCellBackgroundColor() failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::SwitchTableCellHeaderType(Element* aSourceCell,
                                                    Element** aNewCell) {
  if (NS_WARN_IF(!aSourceCell)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eSetTableCellElementType);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of BR in new cell created by
  // ReplaceContainerAndCloneAttributesWithTransaction().
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Save current selection to restore when done.
  // This is needed so ReplaceContainerAndCloneAttributesWithTransaction()
  // can monitor selection when replacing nodes.
  AutoSelectionRestorer restoreSelectionLater(*this);

  // Set to the opposite of current type
  nsAtom* newCellName =
      aSourceCell->IsHTMLElement(nsGkAtoms::td) ? nsGkAtoms::th : nsGkAtoms::td;

  // This creates new node, moves children, copies attributes (true)
  //   and manages the selection!
  RefPtr<Element> newCell = ReplaceContainerAndCloneAttributesWithTransaction(
      *aSourceCell, MOZ_KnownLive(*newCellName));
  if (!newCell) {
    NS_WARNING(
        "EditorBase::ReplaceContainerAndCloneAttributesWithTransaction() "
        "failed");
    return NS_ERROR_FAILURE;
  }

  // Return the new cell
  if (aNewCell) {
    newCell.forget(aNewCell);
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::JoinTableCells(bool aMergeNonContiguousContents) {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eJoinTableCellElements);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  RefPtr<Element> table;
  RefPtr<Element> targetCell;
  int32_t startRowIndex, startColIndex;

  // Get cell, table, etc. at selection anchor node
  rv = GetCellContext(getter_AddRefs(table), getter_AddRefs(targetCell),
                      nullptr, nullptr, &startRowIndex, &startColIndex);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetCellContext() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!table || !targetCell) {
    NS_WARNING(
        "HTMLEditor::GetCellContext() didn't return <table> and/or cell");
    return NS_OK;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Don't let Rules System change the selection
  AutoTransactionsConserveSelection dontChangeSelection(*this);

  // Note: We dont' use AutoSelectionSetterAfterTableEdit here so the selection
  // is retained after joining. This leaves the target cell selected
  // as well as the "non-contiguous" cells, so user can see what happened.

  SelectedTableCellScanner scanner(SelectionRef());

  // If only one cell is selected, join with cell to the right
  if (scanner.ElementsRef().Length() > 1) {
    // We have selected cells: Join just contiguous cells
    // and just merge contents if not contiguous
    ErrorResult error;
    TableSize tableSize(*this, *table, error);
    if (error.Failed()) {
      NS_WARNING("TableSize failed");
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }

    RefPtr<PresShell> presShell = GetPresShell();
    // `MOZ_KnownLive(scanner.ElementsRef()[0])` is safe because `scanner`
    // grabs it until it's destroyed later.
    CellIndexes firstSelectedCellIndexes(
        MOZ_KnownLive(scanner.ElementsRef()[0]), presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }

    // Get spans for cell we will merge into
    int32_t firstRowSpan, firstColSpan;
    nsresult rv = GetCellSpansAt(table, firstSelectedCellIndexes.mRow,
                                 firstSelectedCellIndexes.mColumn, firstRowSpan,
                                 firstColSpan);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::GetCellSpansAt() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    // This defines the last indexes along the "edges"
    // of the contiguous block of cells, telling us
    // that we can join adjacent cells to the block
    // Start with same as the first values,
    // then expand as we find adjacent selected cells
    int32_t lastRowIndex = firstSelectedCellIndexes.mRow;
    int32_t lastColIndex = firstSelectedCellIndexes.mColumn;

    // First pass: Determine boundaries of contiguous rectangular block that
    // we will join into one cell, favoring adjacent cells in the same row.
    IgnoredErrorResult ignoredError;
    for (int32_t rowIndex = firstSelectedCellIndexes.mRow;
         rowIndex <= lastRowIndex; rowIndex++) {
      int32_t currentRowCount = tableSize.mRowCount;
      // Be sure each row doesn't have rowspan errors
      rv = FixBadRowSpan(table, rowIndex, tableSize.mRowCount);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::FixBadRowSpan() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
      // Adjust rowcount by number of rows we removed
      lastRowIndex -= currentRowCount - tableSize.mRowCount;

      bool cellFoundInRow = false;
      bool lastRowIsSet = false;
      int32_t lastColInRow = 0;
      int32_t firstColInRow = firstSelectedCellIndexes.mColumn;
      int32_t colIndex = firstSelectedCellIndexes.mColumn;
      for (CellData cellData; colIndex < tableSize.mColumnCount;
           colIndex = cellData.NextColumnIndex()) {
        cellData.Update(*this, *table, rowIndex, colIndex, ignoredError);
        if (cellData.FailedOrNotFound()) {
          NS_WARNING("CellData::Update() failed");
          return NS_ERROR_FAILURE;
        }
        MOZ_ASSERT(!ignoredError.Failed());

        if (cellData.mIsSelected) {
          if (!cellFoundInRow) {
            // We've just found the first selected cell in this row
            firstColInRow = cellData.mCurrent.mColumn;
          }
          if (cellData.mCurrent.mRow > firstSelectedCellIndexes.mRow &&
              firstColInRow != firstSelectedCellIndexes.mColumn) {
            // We're in at least the second row,
            // but left boundary is "ragged" (not the same as 1st row's start)
            // Let's just end block on previous row
            // and keep previous lastColIndex
            // TODO: We could try to find the Maximum firstColInRow
            //      so our block can still extend down more rows?
            lastRowIndex = std::max(0, cellData.mCurrent.mRow - 1);
            lastRowIsSet = true;
            break;
          }
          // Save max selected column in this row, including extra colspan
          lastColInRow = cellData.LastColumnIndex();
          cellFoundInRow = true;
        } else if (cellFoundInRow) {
          // No cell or not selected, but at least one cell in row was found
          if (cellData.mCurrent.mRow > firstSelectedCellIndexes.mRow + 1 &&
              cellData.mCurrent.mColumn <= lastColIndex) {
            // Cell is in a column less than current right border in
            // the third or higher selected row, so stop block at the previous
            // row
            lastRowIndex = std::max(0, cellData.mCurrent.mRow - 1);
            lastRowIsSet = true;
          }
          // We're done with this row
          break;
        }
        MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
      }  // End of column loop

      // Done with this row
      if (cellFoundInRow) {
        if (rowIndex == firstSelectedCellIndexes.mRow) {
          // First row always initializes the right boundary
          lastColIndex = lastColInRow;
        }

        // If we didn't determine last row above...
        if (!lastRowIsSet) {
          if (colIndex < lastColIndex) {
            // (don't think we ever get here?)
            // Cell is in a column less than current right boundary,
            // so stop block at the previous row
            lastRowIndex = std::max(0, rowIndex - 1);
          } else {
            // Go on to examine next row
            lastRowIndex = rowIndex + 1;
          }
        }
        // Use the minimum col we found so far for right boundary
        lastColIndex = std::min(lastColIndex, lastColInRow);
      } else {
        // No selected cells in this row -- stop at row above
        // and leave last column at its previous value
        lastRowIndex = std::max(0, rowIndex - 1);
      }
    }

    // The list of cells we will delete after joining
    nsTArray<RefPtr<Element>> deleteList;

    // 2nd pass: Do the joining and merging
    for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount; rowIndex++) {
      CellData cellData;
      for (int32_t colIndex = 0; colIndex < tableSize.mColumnCount;
           colIndex = cellData.NextColumnIndex()) {
        cellData.Update(*this, *table, rowIndex, colIndex, ignoredError);
        if (cellData.FailedOrNotFound()) {
          NS_WARNING("CellData::Update() failed");
          return NS_ERROR_FAILURE;
        }
        MOZ_ASSERT(!ignoredError.Failed());

        // If this is 0, we are past last cell in row, so exit the loop
        if (!cellData.mEffectiveColSpan) {
          break;
        }

        // Merge only selected cells (skip cell we're merging into, of course)
        if (cellData.mIsSelected &&
            cellData.mElement != scanner.ElementsRef()[0]) {
          if (cellData.mCurrent.mRow >= firstSelectedCellIndexes.mRow &&
              cellData.mCurrent.mRow <= lastRowIndex &&
              cellData.mCurrent.mColumn >= firstSelectedCellIndexes.mColumn &&
              cellData.mCurrent.mColumn <= lastColIndex) {
            // We are within the join region
            // Problem: It is very tricky to delete cells as we merge,
            // since that will upset the cellmap
            // Instead, build a list of cells to delete and do it later
            NS_ASSERTION(!cellData.IsSpannedFromOtherRow(),
                         "JoinTableCells: StartRowIndex is in row above");

            if (cellData.mEffectiveColSpan > 1) {
              // Check if cell "hangs" off the boundary because of colspan > 1
              // Use split methods to chop off excess
              int32_t extraColSpan = cellData.mFirst.mColumn +
                                     cellData.mEffectiveColSpan -
                                     (lastColIndex + 1);
              if (extraColSpan > 0) {
                nsresult rv = SplitCellIntoColumns(
                    table, cellData.mFirst.mRow, cellData.mFirst.mColumn,
                    cellData.mEffectiveColSpan - extraColSpan, extraColSpan,
                    nullptr);
                if (NS_FAILED(rv)) {
                  NS_WARNING("HTMLEditor::SplitCellIntoColumns() failed");
                  return EditorBase::ToGenericNSResult(rv);
                }
              }
            }

            nsresult rv =
                MergeCells(scanner.ElementsRef()[0], cellData.mElement, false);
            if (NS_FAILED(rv)) {
              NS_WARNING("HTMLEditor::MergeCells() failed");
              return EditorBase::ToGenericNSResult(rv);
            }

            // Add cell to list to delete
            deleteList.AppendElement(cellData.mElement.get());
          } else if (aMergeNonContiguousContents) {
            // Cell is outside join region -- just merge the contents
            nsresult rv =
                MergeCells(scanner.ElementsRef()[0], cellData.mElement, false);
            if (NS_FAILED(rv)) {
              NS_WARNING("HTMLEditor::MergeCells() failed");
              return rv;
            }
          }
        }
        MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
      }
    }

    // All cell contents are merged. Delete the empty cells we accumulated
    // Prevent rules testing until we're done
    AutoEditSubActionNotifier startToHandleEditSubAction(
        *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
    if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
      return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "HTMLEditor::OnStartToHandleTopLevelEditSubAction() "
                         "failed, but ignored");

    for (uint32_t i = 0, n = deleteList.Length(); i < n; i++) {
      RefPtr<Element> nodeToBeRemoved = deleteList[i];
      if (nodeToBeRemoved) {
        nsresult rv = DeleteNodeWithTransaction(*nodeToBeRemoved);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return EditorBase::ToGenericNSResult(rv);
        }
      }
    }
    // Cleanup selection: remove ranges where cells were deleted
    uint32_t rangeCount = SelectionRef().RangeCount();

    RefPtr<nsRange> range;
    for (uint32_t i = 0; i < rangeCount; i++) {
      range = SelectionRef().GetRangeAt(i);
      if (NS_WARN_IF(!range)) {
        return NS_ERROR_FAILURE;
      }

      Element* deletedCell =
          HTMLEditUtils::GetTableCellElementIfOnlyOneSelected(*range);
      if (!deletedCell) {
        SelectionRef().RemoveRangeAndUnselectFramesAndNotifyListeners(
            *range, ignoredError);
        NS_WARNING_ASSERTION(
            !ignoredError.Failed(),
            "Selection::RemoveRangeAndUnselectFramesAndNotifyListeners() "
            "failed, but ignored");
        rangeCount--;
        i--;
      }
    }

    // Set spans for the cell everything merged into
    rv = SetRowSpan(MOZ_KnownLive(scanner.ElementsRef()[0]),
                    lastRowIndex - firstSelectedCellIndexes.mRow + 1);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetRowSpan() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    rv = SetColSpan(MOZ_KnownLive(scanner.ElementsRef()[0]),
                    lastColIndex - firstSelectedCellIndexes.mColumn + 1);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetColSpan() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    // Fixup disturbances in table layout
    DebugOnly<nsresult> rvIgnored = NormalizeTableInternal(*table);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::NormalizeTableInternal() failed, but ignored");
  } else {
    // Joining with cell to the right -- get rowspan and colspan data of target
    // cell.
    IgnoredErrorResult ignoredError;
    CellData leftCellData(*this, *table, startRowIndex, startColIndex,
                          ignoredError);
    if (leftCellData.FailedOrNotFound()) {
      NS_WARNING("CellData for left cell failed");
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // Get data for cell to the right.
    CellData rightCellData(
        *this, *table, leftCellData.mFirst.mRow,
        leftCellData.mFirst.mColumn + leftCellData.mEffectiveColSpan,
        ignoredError);
    if (rightCellData.FailedOrNotFound()) {
      NS_WARNING("CellData for right cell failed");
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // XXX So, this does not assume that CellData returns error when just not
    //     found.  We need to fix this later.
    if (!rightCellData.mElement) {
      return NS_OK;  // Don't fail if there's no cell
    }

    // sanity check
    NS_ASSERTION(
        rightCellData.mCurrent.mRow >= rightCellData.mFirst.mRow,
        "JoinCells: rightCellData.mCurrent.mRow < rightCellData.mFirst.mRow");

    // Figure out span of merged cell starting from target's starting row
    // to handle case of merged cell starting in a row above
    int32_t spanAboveMergedCell = rightCellData.NumberOfPrecedingRows();
    int32_t effectiveRowSpan2 =
        rightCellData.mEffectiveRowSpan - spanAboveMergedCell;
    if (effectiveRowSpan2 > leftCellData.mEffectiveRowSpan) {
      // Cell to the right spans into row below target
      // Split off portion below target cell's bottom-most row
      nsresult rv = SplitCellIntoRows(
          table, rightCellData.mFirst.mRow, rightCellData.mFirst.mColumn,
          spanAboveMergedCell + leftCellData.mEffectiveRowSpan,
          effectiveRowSpan2 - leftCellData.mEffectiveRowSpan, nullptr);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::SplitCellIntoRows() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
    }

    // Move contents from cell to the right
    // Delete the cell now only if it starts in the same row
    //   and has enough row "height"
    nsresult rv =
        MergeCells(leftCellData.mElement, rightCellData.mElement,
                   !rightCellData.IsSpannedFromOtherRow() &&
                       effectiveRowSpan2 >= leftCellData.mEffectiveRowSpan);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::MergeCells() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    if (effectiveRowSpan2 < leftCellData.mEffectiveRowSpan) {
      // Merged cell is "shorter"
      // (there are cells(s) below it that are row-spanned by target cell)
      // We could try splitting those cells, but that's REAL messy,
      // so the safest thing to do is NOT really join the cells
      return NS_OK;
    }

    if (spanAboveMergedCell > 0) {
      // Cell we merged started in a row above the target cell
      // Reduce rowspan to give room where target cell will extend its colspan
      nsresult rv = SetRowSpan(MOZ_KnownLive(rightCellData.mElement),
                               spanAboveMergedCell);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::SetRowSpan() failed");
        return EditorBase::ToGenericNSResult(rv);
      }
    }

    // Reset target cell's colspan to encompass cell to the right
    rv = SetColSpan(
        MOZ_KnownLive(leftCellData.mElement),
        leftCellData.mEffectiveColSpan + rightCellData.mEffectiveColSpan);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetColSpan() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::MergeCells(RefPtr<Element> aTargetCell,
                                RefPtr<Element> aCellToMerge,
                                bool aDeleteCellToMerge) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aTargetCell) || NS_WARN_IF(!aCellToMerge)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Prevent rules testing until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // Don't need to merge if cell is empty
  if (!IsEmptyCell(aCellToMerge)) {
    // Get index of last child in target cell
    // If we fail or don't have children,
    // we insert at index 0
    int32_t insertIndex = 0;

    // Start inserting just after last child
    uint32_t len = aTargetCell->GetChildCount();
    if (len == 1 && IsEmptyCell(aTargetCell)) {
      // Delete the empty node
      nsCOMPtr<nsIContent> cellChild = aTargetCell->GetFirstChild();
      if (NS_WARN_IF(!cellChild)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = DeleteNodeWithTransaction(*cellChild);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      insertIndex = 0;
    } else {
      insertIndex = (int32_t)len;
    }

    // Move the contents
    while (aCellToMerge->HasChildren()) {
      nsCOMPtr<nsIContent> cellChild = aCellToMerge->GetLastChild();
      if (NS_WARN_IF(!cellChild)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = DeleteNodeWithTransaction(*cellChild);
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return rv;
      }
      rv = InsertNodeWithTransaction(*cellChild,
                                     EditorDOMPoint(aTargetCell, insertIndex));
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
        return rv;
      }
    }
  }

  if (!aDeleteCellToMerge) {
    return NS_OK;
  }

  // Delete cells whose contents were moved.
  nsresult rv = DeleteNodeWithTransaction(*aCellToMerge);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::DeleteNodeWithTransaction() failed");
  return rv;
}

nsresult HTMLEditor::FixBadRowSpan(Element* aTable, int32_t aRowIndex,
                                   int32_t& aNewRowCount) {
  if (NS_WARN_IF(!aTable)) {
    return NS_ERROR_INVALID_ARG;
  }

  ErrorResult error;
  TableSize tableSize(*this, *aTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  int32_t minRowSpan = -1;
  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t colIndex = 0; colIndex < tableSize.mColumnCount;
       colIndex = cellData.NextColumnIndex()) {
    cellData.Update(*this, *aTable, aRowIndex, colIndex, ignoredError);
    // NOTE: This is a *real* failure.
    // CellData passes if cell is missing from cellmap
    // XXX If <table> has large rowspan value or colspan value than actual
    //     cells, we may hit error.  So, this method is always failed to
    //     "fix" the rowspan...
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed");
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // XXX So, this does not assume that CellData returns error when just not
    //     found.  We need to fix this later.
    if (!cellData.mElement) {
      break;
    }

    if (cellData.mRowSpan > 0 && !cellData.IsSpannedFromOtherRow() &&
        (cellData.mRowSpan < minRowSpan || minRowSpan == -1)) {
      minRowSpan = cellData.mRowSpan;
    }
    MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
  }

  if (minRowSpan > 1) {
    // The amount to reduce everyone's rowspan
    // so at least one cell has rowspan = 1
    int32_t rowsReduced = minRowSpan - 1;
    CellData cellData;
    for (int32_t colIndex = 0; colIndex < tableSize.mColumnCount;
         colIndex = cellData.NextColumnIndex()) {
      cellData.Update(*this, *aTable, aRowIndex, colIndex, ignoredError);
      if (cellData.FailedOrNotFound()) {
        NS_WARNING("CellData::Udpate() failed");
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(!ignoredError.Failed());

      // Fixup rowspans only for cells starting in current row
      // XXX So, this does not assume that CellData returns error when just
      //     not found a cell.  Fix this later.
      if (cellData.mElement && cellData.mRowSpan > 0 &&
          !cellData.IsSpannedFromOtherRowOrColumn()) {
        nsresult rv = SetRowSpan(MOZ_KnownLive(cellData.mElement),
                                 cellData.mRowSpan - rowsReduced);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::SetRawSpan() failed");
          return rv;
        }
      }
      MOZ_ASSERT(colIndex < cellData.NextColumnIndex());
    }
  }
  tableSize.Update(*this, *aTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize::Update() failed");
    return error.StealNSResult();
  }
  aNewRowCount = tableSize.mRowCount;
  return NS_OK;
}

nsresult HTMLEditor::FixBadColSpan(Element* aTable, int32_t aColIndex,
                                   int32_t& aNewColCount) {
  if (NS_WARN_IF(!aTable)) {
    return NS_ERROR_INVALID_ARG;
  }

  ErrorResult error;
  TableSize tableSize(*this, *aTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  int32_t minColSpan = -1;
  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount;
       rowIndex = cellData.NextRowIndex()) {
    cellData.Update(*this, *aTable, rowIndex, aColIndex, ignoredError);
    // NOTE: This is a *real* failure.
    // CellData passes if cell is missing from cellmap
    // XXX If <table> has large rowspan value or colspan value than actual
    //     cells, we may hit error.  So, this method is always failed to
    //     "fix" the colspan...
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed");
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // XXX So, this does not assume that CellData returns error when just
    //     not found a cell.  Fix this later.
    if (!cellData.mElement) {
      break;
    }
    if (cellData.mColSpan > 0 && !cellData.IsSpannedFromOtherColumn() &&
        (cellData.mColSpan < minColSpan || minColSpan == -1)) {
      minColSpan = cellData.mColSpan;
    }
    MOZ_ASSERT(rowIndex < cellData.NextRowIndex());
  }

  if (minColSpan > 1) {
    // The amount to reduce everyone's colspan
    // so at least one cell has colspan = 1
    int32_t colsReduced = minColSpan - 1;
    CellData cellData;
    for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount;
         rowIndex = cellData.NextRowIndex()) {
      cellData.Update(*this, *aTable, rowIndex, aColIndex, ignoredError);
      if (cellData.FailedOrNotFound()) {
        NS_WARNING("CellData::Update() failed");
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(!ignoredError.Failed());

      // Fixup colspans only for cells starting in current column
      // XXX So, this does not assume that CellData returns error when just
      //     not found a cell.  Fix this later.
      if (cellData.mElement && cellData.mColSpan > 0 &&
          !cellData.IsSpannedFromOtherRowOrColumn()) {
        nsresult rv = SetColSpan(MOZ_KnownLive(cellData.mElement),
                                 cellData.mColSpan - colsReduced);
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::SetColSpan() failed");
          return rv;
        }
      }
      MOZ_ASSERT(rowIndex < cellData.NextRowIndex());
    }
  }
  tableSize.Update(*this, *aTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize::Update() failed");
    return error.StealNSResult();
  }
  aNewColCount = tableSize.mColumnCount;
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::NormalizeTable(Element* aTableOrElementInTable) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNormalizeTable);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (!aTableOrElementInTable) {
    aTableOrElementInTable =
        GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
    if (!aTableOrElementInTable) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::"
          "table) failed");
      return NS_OK;  // Don't throw error even if the element is not in <table>.
    }
  }
  rv = NormalizeTableInternal(*aTableOrElementInTable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::NormalizeTableInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::NormalizeTableInternal(Element& aTableOrElementInTable) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  RefPtr<Element> tableElement;
  if (aTableOrElementInTable.NodeInfo()->NameAtom() == nsGkAtoms::table) {
    tableElement = &aTableOrElementInTable;
  } else {
    tableElement = GetInclusiveAncestorByTagNameInternal(
        *nsGkAtoms::table, aTableOrElementInTable);
    if (!tableElement) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
          "failed");
      return NS_OK;  // Don't throw error even if the element is not in <table>.
    }
  }

  ErrorResult error;
  TableSize tableSize(*this, *tableElement, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return error.StealNSResult();
  }

  // Save current selection
  AutoSelectionRestorer restoreSelectionLater(*this);

  AutoPlaceholderBatch treateAsOneTransaction(*this,
                                              ScrollSelectionIntoView::Yes);
  // Prevent auto insertion of BR in new cell until we're done
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertNode, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // XXX If there is a cell which has bigger or smaller "rowspan" or "colspan"
  //     values, FixBadRowSpan() will return error.  So, we can do nothing
  //     if the table needs normalization...
  // Scan all cells in each row to detect bad rowspan values
  for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount; rowIndex++) {
    nsresult rv = FixBadRowSpan(tableElement, rowIndex, tableSize.mRowCount);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::FixBadRowSpan() failed");
      return rv;
    }
  }
  // and same for colspans
  for (int32_t colIndex = 0; colIndex < tableSize.mColumnCount; colIndex++) {
    nsresult rv = FixBadColSpan(tableElement, colIndex, tableSize.mColumnCount);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::FixBadColSpan() failed");
      return rv;
    }
  }

  // Fill in missing cellmap locations with empty cells
  for (int32_t rowIndex = 0; rowIndex < tableSize.mRowCount; rowIndex++) {
    RefPtr<Element> previousCellElementInRow;
    for (int32_t colIndex = 0; colIndex < tableSize.mColumnCount; colIndex++) {
      CellData cellData(*this, *tableElement, rowIndex, colIndex, ignoredError);
      // NOTE: This is a *real* failure.
      // CellData passes if cell is missing from cellmap
      // XXX So, this method assumes that CellData won't return error when
      //     just not found.  Fix this later.
      if (cellData.FailedOrNotFound()) {
        NS_WARNING("CellData::Update() failed");
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT(!ignoredError.Failed());

      if (cellData.mElement) {
        // Save the last cell found in the same row we are scanning
        if (!cellData.IsSpannedFromOtherRow()) {
          previousCellElementInRow = std::move(cellData.mElement);
        }
        continue;
      }

      // We are missing a cell at a cellmap location.
      // Add a cell after the previous cell element in the current row.
      if (NS_WARN_IF(!previousCellElementInRow)) {
        // We don't have any cells in this row -- We are really messed up!
        return NS_ERROR_FAILURE;
      }

      // Insert a new cell after (true), and return the new cell to us
      RefPtr<Element> newCellElement;
      nsresult rv = InsertCell(previousCellElementInRow, 1, 1, true, false,
                               getter_AddRefs(newCellElement));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::InsertCell() failed");
        return rv;
      }

      if (newCellElement) {
        previousCellElementInRow = std::move(newCellElement);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetCellIndexes(Element* aCellElement,
                                         int32_t* aRowIndex,
                                         int32_t* aColumnIndex) {
  if (NS_WARN_IF(!aRowIndex) || NS_WARN_IF(!aColumnIndex)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aRowIndex = 0;
  *aColumnIndex = 0;

  if (!aCellElement) {
    // Use cell element which contains anchor of Selection when aCellElement is
    // nullptr.
    ErrorResult error;
    CellIndexes cellIndexes(*this, SelectionRef(), error);
    if (error.Failed()) {
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }
    *aRowIndex = cellIndexes.mRow;
    *aColumnIndex = cellIndexes.mColumn;
    return NS_OK;
  }

  ErrorResult error;
  const RefPtr<PresShell> presShell{GetPresShell()};
  CellIndexes cellIndexes(*aCellElement, presShell, error);
  if (error.Failed()) {
    NS_WARNING("CellIndexes failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }
  *aRowIndex = cellIndexes.mRow;
  *aColumnIndex = cellIndexes.mColumn;
  return NS_OK;
}

void HTMLEditor::CellIndexes::Update(HTMLEditor& aHTMLEditor,
                                     Selection& aSelection, ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  // Guarantee the life time of the cell element since Init() will access
  // layout methods.
  RefPtr<Element> cellElement =
      aHTMLEditor.GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::td);
  if (!cellElement) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::td) "
        "failed");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<PresShell> presShell{aHTMLEditor.GetPresShell()};
  Update(*cellElement, presShell, aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "CellIndexes::Update() failed");
}

void HTMLEditor::CellIndexes::Update(Element& aCellElement,
                                     PresShell* aPresShell, ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  // If the table cell is created immediately before this call, e.g., using
  // innerHTML, frames have not been created yet. Hence, flush layout to create
  // them.
  if (NS_WARN_IF(!aPresShell)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aPresShell->FlushPendingNotifications(FlushType::Frames);

  nsIFrame* frameOfCell = aCellElement.GetPrimaryFrame();
  if (!frameOfCell) {
    NS_WARNING("There was no layout information of aCellElement");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsITableCellLayout* tableCellLayout = do_QueryFrame(frameOfCell);
  if (!tableCellLayout) {
    NS_WARNING("aCellElement was not a table cell");
    aRv.Throw(NS_ERROR_FAILURE);  // not a cell element.
    return;
  }

  aRv = tableCellLayout->GetCellIndexes(mRow, mColumn);
  NS_WARNING_ASSERTION(!aRv.Failed(),
                       "nsITableCellLayout::GetCellIndexes() failed");
}

// static
nsTableWrapperFrame* HTMLEditor::GetTableFrame(Element* aTableElement) {
  if (NS_WARN_IF(!aTableElement)) {
    return nullptr;
  }
  return do_QueryFrame(aTableElement->GetPrimaryFrame());
}

// Return actual number of cells (a cell with colspan > 1 counts as just 1)
int32_t HTMLEditor::GetNumberOfCellsInRow(Element& aTableElement,
                                          int32_t aRowIndex) {
  IgnoredErrorResult ignoredError;
  TableSize tableSize(*this, aTableElement, ignoredError);
  if (ignoredError.Failed()) {
    NS_WARNING("TableSize failed");
    return -1;
  }

  int32_t numberOfCells = 0;
  CellData cellData;
  for (int32_t columnIndex = 0; columnIndex < tableSize.mColumnCount;
       columnIndex = cellData.NextColumnIndex()) {
    cellData.Update(*this, aTableElement, aRowIndex, columnIndex, ignoredError);
    // Failure means that there is no more cell in the row.  In this case,
    // we shouldn't return error since we just reach the end of the row.
    // XXX So, this method assumes that CellData won't return error when
    //     just not found.  Fix this later.
    if (cellData.FailedOrNotFound()) {
      break;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // Only count cells that start in row we are working with
    if (cellData.mElement && !cellData.IsSpannedFromOtherRow()) {
      numberOfCells++;
    }
    MOZ_ASSERT(columnIndex < cellData.NextColumnIndex());
  }
  return numberOfCells;
}

NS_IMETHODIMP HTMLEditor::GetTableSize(Element* aTableOrElementInTable,
                                       int32_t* aRowCount,
                                       int32_t* aColumnCount) {
  if (NS_WARN_IF(!aRowCount) || NS_WARN_IF(!aColumnCount)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aRowCount = 0;
  *aColumnCount = 0;

  Element* tableOrElementInTable = aTableOrElementInTable;
  if (!tableOrElementInTable) {
    tableOrElementInTable =
        GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
    if (!tableOrElementInTable) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::"
          "table) failed");
      return NS_ERROR_FAILURE;
    }
  }

  ErrorResult error;
  TableSize tableSize(*this, *tableOrElementInTable, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }
  *aRowCount = tableSize.mRowCount;
  *aColumnCount = tableSize.mColumnCount;
  return NS_OK;
}

void HTMLEditor::TableSize::Update(HTMLEditor& aHTMLEditor,
                                   Element& aTableOrElementInTable,
                                   ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  // Currently, nsTableWrapperFrame::GetRowCount() and
  // nsTableWrapperFrame::GetColCount() are safe to use without grabbing
  // <table> element.  However, editor developers may not watch layout API
  // changes.  So, for keeping us safer, we should use RefPtr here.
  RefPtr<Element> tableElement =
      aHTMLEditor.GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::table,
                                                        aTableOrElementInTable);
  if (!tableElement) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
        "failed");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsTableWrapperFrame* tableFrame =
      do_QueryFrame(tableElement->GetPrimaryFrame());
  if (!tableFrame) {
    NS_WARNING("There was no layout information of the <table> element");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  mRowCount = tableFrame->GetRowCount();
  mColumnCount = tableFrame->GetColCount();
}

NS_IMETHODIMP HTMLEditor::GetCellDataAt(
    Element* aTableElement, int32_t aRowIndex, int32_t aColumnIndex,
    Element** aCellElement, int32_t* aStartRowIndex, int32_t* aStartColumnIndex,
    int32_t* aRowSpan, int32_t* aColSpan, int32_t* aEffectiveRowSpan,
    int32_t* aEffectiveColSpan, bool* aIsSelected) {
  if (NS_WARN_IF(!aCellElement) || NS_WARN_IF(!aStartRowIndex) ||
      NS_WARN_IF(!aStartColumnIndex) || NS_WARN_IF(!aRowSpan) ||
      NS_WARN_IF(!aColSpan) || NS_WARN_IF(!aEffectiveRowSpan) ||
      NS_WARN_IF(!aEffectiveColSpan) || NS_WARN_IF(!aIsSelected)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aStartRowIndex = 0;
  *aStartColumnIndex = 0;
  *aRowSpan = 0;
  *aColSpan = 0;
  *aEffectiveRowSpan = 0;
  *aEffectiveColSpan = 0;
  *aIsSelected = false;
  *aCellElement = nullptr;

  // Let's keep the table element with strong pointer since editor developers
  // may not handle layout code of <table>, however, this method depends on
  // them.
  RefPtr<Element> table = aTableElement;
  if (!table) {
    // Get the selected table or the table enclosing the selection anchor.
    table = GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
    if (!table) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::"
          "table) failed");
      return NS_ERROR_FAILURE;
    }
  }

  IgnoredErrorResult ignoredError;
  CellData cellData(*this, *table, aRowIndex, aColumnIndex, ignoredError);
  if (cellData.FailedOrNotFound()) {
    NS_WARNING("CellData failed");
    return NS_ERROR_FAILURE;
  }
  cellData.mElement.forget(aCellElement);
  *aIsSelected = cellData.mIsSelected;
  *aStartRowIndex = cellData.mFirst.mRow;
  *aStartColumnIndex = cellData.mFirst.mColumn;
  *aRowSpan = cellData.mRowSpan;
  *aColSpan = cellData.mColSpan;
  *aEffectiveRowSpan = cellData.mEffectiveRowSpan;
  *aEffectiveColSpan = cellData.mEffectiveColSpan;
  return NS_OK;
}

void HTMLEditor::CellData::Update(HTMLEditor& aHTMLEditor,
                                  Element& aTableElement, ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed());

  mElement = nullptr;
  mIsSelected = false;
  mFirst.mRow = -1;
  mFirst.mColumn = -1;
  mRowSpan = -1;
  mColSpan = -1;
  mEffectiveRowSpan = -1;
  mEffectiveColSpan = -1;

  nsTableWrapperFrame* tableFrame = HTMLEditor::GetTableFrame(&aTableElement);
  if (!tableFrame) {
    NS_WARNING("There was no layout information of the table");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // If there is no cell at the indexes.  Don't return error.
  // XXX If we have pending layout and that causes the cell frame hasn't been
  //     created, we should return error, but how can we do it?
  nsTableCellFrame* cellFrame =
      tableFrame->GetCellFrameAt(mCurrent.mRow, mCurrent.mColumn);
  if (!cellFrame) {
    return;
  }

  mElement = cellFrame->GetContent()->AsElement();
  if (!mElement) {
    NS_WARNING("The cell frame didn't have cell element");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  mIsSelected = cellFrame->IsSelected();
  mFirst.mRow = cellFrame->RowIndex();
  mFirst.mColumn = cellFrame->ColIndex();
  mRowSpan = cellFrame->GetRowSpan();
  mColSpan = cellFrame->GetColSpan();
  mEffectiveRowSpan =
      tableFrame->GetEffectiveRowSpanAt(mCurrent.mRow, mCurrent.mColumn);
  mEffectiveColSpan =
      tableFrame->GetEffectiveColSpanAt(mCurrent.mRow, mCurrent.mColumn);
}

NS_IMETHODIMP HTMLEditor::GetCellAt(Element* aTableElement, int32_t aRowIndex,
                                    int32_t aColumnIndex,
                                    Element** aCellElement) {
  if (NS_WARN_IF(!aCellElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aCellElement = nullptr;

  Element* tableElement = aTableElement;
  if (!tableElement) {
    // Get the selected table or the table enclosing the selection anchor.
    tableElement = GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
    if (!tableElement) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::"
          "table) failed");
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<Element> cellElement =
      GetTableCellElementAt(*tableElement, aRowIndex, aColumnIndex);
  cellElement.forget(aCellElement);
  return NS_OK;
}

Element* HTMLEditor::GetTableCellElementAt(Element& aTableElement,
                                           int32_t aRowIndex,
                                           int32_t aColumnIndex) const {
  // Let's grab the <table> element while we're retrieving layout API since
  // editor developers do not watch all layout API changes.  So, it may
  // become unsafe.
  OwningNonNull<Element> tableElement(aTableElement);
  nsTableWrapperFrame* tableFrame = HTMLEditor::GetTableFrame(tableElement);
  if (!tableFrame) {
    NS_WARNING("There was no table layout information");
    return nullptr;
  }
  nsIContent* cell = tableFrame->GetCellAt(aRowIndex, aColumnIndex);
  return Element::FromNodeOrNull(cell);
}

// When all you want are the rowspan and colspan (not exposed in nsITableEditor)
nsresult HTMLEditor::GetCellSpansAt(Element* aTable, int32_t aRowIndex,
                                    int32_t aColIndex, int32_t& aActualRowSpan,
                                    int32_t& aActualColSpan) {
  nsTableWrapperFrame* tableFrame = HTMLEditor::GetTableFrame(aTable);
  if (!tableFrame) {
    NS_WARNING("There was no table layout information");
    return NS_ERROR_FAILURE;
  }
  aActualRowSpan = tableFrame->GetEffectiveRowSpanAt(aRowIndex, aColIndex);
  aActualColSpan = tableFrame->GetEffectiveColSpanAt(aRowIndex, aColIndex);

  return NS_OK;
}

nsresult HTMLEditor::GetCellContext(Element** aTable, Element** aCell,
                                    nsINode** aCellParent, int32_t* aCellOffset,
                                    int32_t* aRowIndex, int32_t* aColumnIndex) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Initialize return pointers
  if (aTable) {
    *aTable = nullptr;
  }
  if (aCell) {
    *aCell = nullptr;
  }
  if (aCellParent) {
    *aCellParent = nullptr;
  }
  if (aCellOffset) {
    *aCellOffset = 0;
  }
  if (aRowIndex) {
    *aRowIndex = 0;
  }
  if (aColumnIndex) {
    *aColumnIndex = 0;
  }

  RefPtr<Element> table;
  RefPtr<Element> cell;

  // Caller may supply the cell...
  if (aCell && *aCell) {
    cell = *aCell;
  }

  // ...but if not supplied,
  //    get cell if it's the child of selection anchor node,
  //    or get the enclosing by a cell
  if (!cell) {
    // Find a selected or enclosing table element
    ErrorResult error;
    RefPtr<Element> cellOrRowOrTableElement =
        GetSelectedOrParentTableElement(error);
    if (error.Failed()) {
      NS_WARNING("HTMLEditor::GetSelectedOrParentTableElement() failed");
      return error.StealNSResult();
    }
    if (!cellOrRowOrTableElement) {
      return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
    }
    if (cellOrRowOrTableElement->IsHTMLElement(nsGkAtoms::table)) {
      // We have a selected table, not a cell
      if (aTable) {
        cellOrRowOrTableElement.forget(aTable);
      }
      return NS_OK;
    }
    if (!cellOrRowOrTableElement->IsAnyOfHTMLElements(nsGkAtoms::td,
                                                      nsGkAtoms::th)) {
      return NS_SUCCESS_EDITOR_ELEMENT_NOT_FOUND;
    }

    // We found a cell
    cell = std::move(cellOrRowOrTableElement);
  }
  if (aCell) {
    // we don't want to cell.forget() here, because we use it below.
    *aCell = do_AddRef(cell).take();
  }

  // Get containing table
  table = GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::table, *cell);
  if (!table) {
    NS_WARNING(
        "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
        "failed");
    // Cell must be in a table, so fail if not found
    return NS_ERROR_FAILURE;
  }
  if (aTable) {
    table.forget(aTable);
  }

  // Get the rest of the related data only if requested
  if (aRowIndex || aColumnIndex) {
    ErrorResult error;
    const RefPtr<PresShell> presShell{GetPresShell()};
    CellIndexes cellIndexes(*cell, presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return error.StealNSResult();
    }
    if (aRowIndex) {
      *aRowIndex = cellIndexes.mRow;
    }
    if (aColumnIndex) {
      *aColumnIndex = cellIndexes.mColumn;
    }
  }
  if (aCellParent) {
    // Get the immediate parent of the cell
    EditorRawDOMPoint atCellElement(cell);
    if (NS_WARN_IF(!atCellElement.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    if (aCellOffset) {
      *aCellOffset = atCellElement.Offset();
    }

    // Now it's safe to hand over the reference to cellParent, since
    // we don't need it anymore.
    *aCellParent = do_AddRef(atCellElement.GetContainer()).take();
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetSelectedCells(
    nsTArray<RefPtr<Element>>& aOutSelectedCellElements) {
  MOZ_ASSERT(aOutSelectedCellElements.IsEmpty());

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  SelectedTableCellScanner scanner(SelectionRef());
  if (!scanner.IsInTableCellSelectionMode()) {
    return NS_OK;
  }

  aOutSelectedCellElements.SetCapacity(scanner.ElementsRef().Length());
  for (const OwningNonNull<Element>& cellElement : scanner.ElementsRef()) {
    aOutSelectedCellElements.AppendElement(cellElement);
  }
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetFirstSelectedCellInTable(int32_t* aRowIndex,
                                                      int32_t* aColumnIndex,
                                                      Element** aCellElement) {
  if (NS_WARN_IF(!aRowIndex) || NS_WARN_IF(!aColumnIndex) ||
      NS_WARN_IF(!aCellElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should return NS_OK?
  }

  *aRowIndex = 0;
  *aColumnIndex = 0;
  *aCellElement = nullptr;
  RefPtr<Element> firstSelectedCellElement =
      HTMLEditUtils::GetFirstSelectedTableCellElement(SelectionRef());
  if (!firstSelectedCellElement) {
    return NS_OK;
  }

  ErrorResult error;
  RefPtr<PresShell> presShell = GetPresShell();
  CellIndexes indexes(*firstSelectedCellElement, presShell, error);
  if (error.Failed()) {
    NS_WARNING("CellIndexes failed");
    return error.StealNSResult();
  }

  firstSelectedCellElement.forget(aCellElement);
  *aRowIndex = indexes.mRow;
  *aColumnIndex = indexes.mColumn;
  return NS_OK;
}

void HTMLEditor::SetSelectionAfterTableEdit(Element* aTable, int32_t aRow,
                                            int32_t aCol, int32_t aDirection,
                                            bool aSelected) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aTable) || NS_WARN_IF(Destroyed())) {
    return;
  }

  RefPtr<Element> cell;
  bool done = false;
  do {
    cell = GetTableCellElementAt(*aTable, aRow, aCol);
    if (cell) {
      if (aSelected) {
        // Reselect the cell
        DebugOnly<nsresult> rv = SelectContentInternal(*cell);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "HTMLEditor::SelectContentInternal() failed, but ignored");
        return;
      }

      // Set the caret to deepest first child
      //   but don't go into nested tables
      // TODO: Should we really be placing the caret at the END
      // of the cell content?
      CollapseSelectionToDeepestNonTableFirstChild(cell);
      return;
    }

    // Setup index to find another cell in the
    //   direction requested, but move in other direction if already at
    //   beginning of row or column
    switch (aDirection) {
      case ePreviousColumn:
        if (!aCol) {
          if (aRow > 0) {
            aRow--;
          } else {
            done = true;
          }
        } else {
          aCol--;
        }
        break;
      case ePreviousRow:
        if (!aRow) {
          if (aCol > 0) {
            aCol--;
          } else {
            done = true;
          }
        } else {
          aRow--;
        }
        break;
      default:
        done = true;
    }
  } while (!done);

  // We didn't find a cell
  // Set selection to just before the table
  if (aTable->GetParentNode()) {
    EditorRawDOMPoint atTable(aTable);
    if (NS_WARN_IF(!atTable.IsSetAndValid())) {
      return;
    }
    DebugOnly<nsresult> rvIgnored = CollapseSelectionTo(atTable);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::CollapseSelectionTo() failed, but ignored");
    return;
  }
  // Last resort: Set selection to start of doc
  // (it's very bad to not have a valid selection!)
  DebugOnly<nsresult> rvIgnored = SetSelectionAtDocumentStart();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "HTMLEditor::SetSelectionAtDocumentStart() failed, but ignored");
}

NS_IMETHODIMP HTMLEditor::GetSelectedOrParentTableElement(
    nsAString& aTagName, int32_t* aSelectedCount,
    Element** aCellOrRowOrTableElement) {
  if (NS_WARN_IF(!aSelectedCount) || NS_WARN_IF(!aCellOrRowOrTableElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  aTagName.Truncate();
  *aCellOrRowOrTableElement = nullptr;
  *aSelectedCount = 0;

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool isCellSelected = false;
  ErrorResult aRv;
  RefPtr<Element> cellOrRowOrTableElement =
      GetSelectedOrParentTableElement(aRv, &isCellSelected);
  if (aRv.Failed()) {
    NS_WARNING("HTMLEditor::GetSelectedOrParentTableElement() failed");
    return EditorBase::ToGenericNSResult(aRv.StealNSResult());
  }
  if (!cellOrRowOrTableElement) {
    return NS_OK;
  }

  if (isCellSelected) {
    aTagName.AssignLiteral("td");
    *aSelectedCount = SelectionRef().RangeCount();
    cellOrRowOrTableElement.forget(aCellOrRowOrTableElement);
    return NS_OK;
  }

  if (cellOrRowOrTableElement->IsAnyOfHTMLElements(nsGkAtoms::td,
                                                   nsGkAtoms::th)) {
    aTagName.AssignLiteral("td");
    // Keep *aSelectedCount as 0.
    cellOrRowOrTableElement.forget(aCellOrRowOrTableElement);
    return NS_OK;
  }

  if (cellOrRowOrTableElement->IsHTMLElement(nsGkAtoms::table)) {
    aTagName.AssignLiteral("table");
    *aSelectedCount = 1;
    cellOrRowOrTableElement.forget(aCellOrRowOrTableElement);
    return NS_OK;
  }

  if (cellOrRowOrTableElement->IsHTMLElement(nsGkAtoms::tr)) {
    aTagName.AssignLiteral("tr");
    *aSelectedCount = 1;
    cellOrRowOrTableElement.forget(aCellOrRowOrTableElement);
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Which element was returned?");
  return NS_ERROR_UNEXPECTED;
}

already_AddRefed<Element> HTMLEditor::GetSelectedOrParentTableElement(
    ErrorResult& aRv, bool* aIsCellSelected /* = nullptr */) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(!aRv.Failed());

  if (aIsCellSelected) {
    *aIsCellSelected = false;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    aRv.Throw(NS_ERROR_FAILURE);  // XXX Shouldn't throw an exception?
    return nullptr;
  }

  // Try to get the first selected cell, first.
  RefPtr<Element> cellElement =
      HTMLEditUtils::GetFirstSelectedTableCellElement(SelectionRef());
  if (cellElement) {
    if (aIsCellSelected) {
      *aIsCellSelected = true;
    }
    return cellElement.forget();
  }

  const RangeBoundary& anchorRef = SelectionRef().AnchorRef();
  if (NS_WARN_IF(!anchorRef.IsSet())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // If anchor selects a <td>, <table> or <tr>, return it.
  if (anchorRef.Container()->HasChildNodes()) {
    nsIContent* selectedContent = anchorRef.GetChildAtOffset();
    if (selectedContent) {
      // XXX Why do we ignore <th> element in this case?
      if (selectedContent->IsHTMLElement(nsGkAtoms::td)) {
        // FYI: If first range selects a <tr> element, but the other selects
        //      a <td> element, you can reach here.
        // Each cell is in its own selection range in this case.
        // XXX Although, other ranges may not select cells, though.
        if (aIsCellSelected) {
          *aIsCellSelected = true;
        }
        return do_AddRef(selectedContent->AsElement());
      }
      if (selectedContent->IsAnyOfHTMLElements(nsGkAtoms::table,
                                               nsGkAtoms::tr)) {
        return do_AddRef(selectedContent->AsElement());
      }
    }
  }

  if (NS_WARN_IF(!anchorRef.Container()->IsContent())) {
    return nullptr;
  }

  // Then, look for a cell element (either <td> or <th>) which contains
  // the anchor container.
  cellElement = GetInclusiveAncestorByTagNameInternal(
      *nsGkAtoms::td, *anchorRef.Container()->AsContent());
  if (!cellElement) {
    return nullptr;  // Not in table.
  }
  // Don't set *aIsCellSelected to true in this case because it does NOT
  // select a cell, just in a cell.
  return cellElement.forget();
}

NS_IMETHODIMP HTMLEditor::GetSelectedCellsType(Element* aElement,
                                               uint32_t* aSelectionType) {
  if (NS_WARN_IF(!aSelectionType)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aSelectionType = 0;

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (NS_WARN_IF(!SelectionRef().RangeCount())) {
    return NS_ERROR_FAILURE;  // XXX Should we just return NS_OK?
  }

  // Be sure we have a table element
  //  (if aElement is null, this uses selection's anchor node)
  RefPtr<Element> table;
  if (aElement) {
    table = GetInclusiveAncestorByTagNameInternal(*nsGkAtoms::table, *aElement);
    if (!table) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameInternal(nsGkAtoms::table) "
          "failed");
      return NS_ERROR_FAILURE;
    }
  } else {
    table = GetInclusiveAncestorByTagNameAtSelection(*nsGkAtoms::table);
    if (!table) {
      NS_WARNING(
          "HTMLEditor::GetInclusiveAncestorByTagNameAtSelection(nsGkAtoms::"
          "table) failed");
      return NS_ERROR_FAILURE;
    }
  }

  ErrorResult error;
  TableSize tableSize(*this, *table, error);
  if (error.Failed()) {
    NS_WARNING("TableSize failed");
    return EditorBase::ToGenericNSResult(error.StealNSResult());
  }

  // Traverse all selected cells
  SelectedTableCellScanner scanner(SelectionRef());
  if (!scanner.IsInTableCellSelectionMode()) {
    return NS_OK;
  }

  // We have at least one selected cell, so set return value
  *aSelectionType = static_cast<uint32_t>(TableSelectionMode::Cell);

  // Store indexes of each row/col to avoid duplication of searches
  nsTArray<int32_t> indexArray;

  const RefPtr<PresShell> presShell{GetPresShell()};
  bool allCellsInRowAreSelected = false;
  for (const OwningNonNull<Element>& selectedCellElement :
       scanner.ElementsRef()) {
    // `MOZ_KnownLive(selectedCellElement)` is safe because `scanner` grabs
    // it until it's destroyed later.
    CellIndexes selectedCellIndexes(MOZ_KnownLive(selectedCellElement),
                                    presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }
    if (!indexArray.Contains(selectedCellIndexes.mColumn)) {
      indexArray.AppendElement(selectedCellIndexes.mColumn);
      allCellsInRowAreSelected = AllCellsInRowSelected(
          table, selectedCellIndexes.mRow, tableSize.mColumnCount);
      // We're done as soon as we fail for any row
      if (!allCellsInRowAreSelected) {
        break;
      }
    }
  }

  if (allCellsInRowAreSelected) {
    *aSelectionType = static_cast<uint32_t>(TableSelectionMode::Row);
    return NS_OK;
  }
  // Test for columns

  // Empty the indexArray
  indexArray.Clear();

  // Start at first cell again
  bool allCellsInColAreSelected = false;
  for (const OwningNonNull<Element>& selectedCellElement :
       scanner.ElementsRef()) {
    // `MOZ_KnownLive(selectedCellElement)` is safe because `scanner` grabs
    // it until it's destroyed later.
    CellIndexes selectedCellIndexes(MOZ_KnownLive(selectedCellElement),
                                    presShell, error);
    if (error.Failed()) {
      NS_WARNING("CellIndexes failed");
      return EditorBase::ToGenericNSResult(error.StealNSResult());
    }

    if (!indexArray.Contains(selectedCellIndexes.mRow)) {
      indexArray.AppendElement(selectedCellIndexes.mColumn);
      allCellsInColAreSelected = AllCellsInColumnSelected(
          table, selectedCellIndexes.mColumn, tableSize.mRowCount);
      // We're done as soon as we fail for any column
      if (!allCellsInRowAreSelected) {
        break;
      }
    }
  }
  if (allCellsInColAreSelected) {
    *aSelectionType = static_cast<uint32_t>(TableSelectionMode::Column);
  }

  return NS_OK;
}

bool HTMLEditor::AllCellsInRowSelected(Element* aTable, int32_t aRowIndex,
                                       int32_t aNumberOfColumns) {
  if (NS_WARN_IF(!aTable)) {
    return false;
  }

  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t col = 0; col < aNumberOfColumns;
       col = cellData.NextColumnIndex()) {
    cellData.Update(*this, *aTable, aRowIndex, col, ignoredError);
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed");
      return false;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // If no cell, we may have a "ragged" right edge, so return TRUE only if
    // we already found a cell in the row.
    // XXX So, this does not assume that CellData returns error when just
    //     not found a cell.  Fix this later.
    if (!cellData.mElement) {
      NS_WARNING("CellData didn't set mElement");
      return cellData.mCurrent.mColumn > 0;
    }

    // Return as soon as a non-selected cell is found.
    // XXX Odd, this is testing if each cell element is selected.  Why do
    //     we need to warn if it's false??
    if (!cellData.mIsSelected) {
      NS_WARNING("CellData didn't set mIsSelected");
      return false;
    }

    MOZ_ASSERT(col < cellData.NextColumnIndex());
  }
  return true;
}

bool HTMLEditor::AllCellsInColumnSelected(Element* aTable, int32_t aColIndex,
                                          int32_t aNumberOfRows) {
  if (NS_WARN_IF(!aTable)) {
    return false;
  }

  IgnoredErrorResult ignoredError;
  CellData cellData;
  for (int32_t row = 0; row < aNumberOfRows; row = cellData.NextRowIndex()) {
    cellData.Update(*this, *aTable, row, aColIndex, ignoredError);
    if (cellData.FailedOrNotFound()) {
      NS_WARNING("CellData::Update() failed");
      return false;
    }
    MOZ_ASSERT(!ignoredError.Failed());

    // If no cell, we must have a "ragged" right edge on the last column so
    // return TRUE only if we already found a cell in the row.
    // XXX So, this does not assume that CellData returns error when just
    //     not found a cell.  Fix this later.
    if (!cellData.mElement) {
      NS_WARNING("CellData didn't set mElement");
      return cellData.mCurrent.mRow > 0;
    }

    // Return as soon as a non-selected cell is found.
    // XXX Odd, this is testing if each cell element is selected.  Why do
    //     we need to warn if it's false??
    if (!cellData.mIsSelected) {
      NS_WARNING("CellData didn't set mIsSelected");
      return false;
    }

    MOZ_ASSERT(row < cellData.NextRowIndex());
  }
  return true;
}

bool HTMLEditor::IsEmptyCell(dom::Element* aCell) {
  MOZ_ASSERT(aCell);

  // Check if target only contains empty text node or <br>
  nsCOMPtr<nsINode> cellChild = aCell->GetFirstChild();
  if (!cellChild) {
    return false;
  }

  nsCOMPtr<nsINode> nextChild = cellChild->GetNextSibling();
  if (nextChild) {
    return false;
  }

  // We insert a single break into a cell by default
  //   to have some place to locate a cursor -- it is dispensable
  if (cellChild->IsHTMLElement(nsGkAtoms::br)) {
    return true;
  }

  // Or check if no real content
  return HTMLEditUtils::IsEmptyNode(
      *cellChild, {EmptyCheckOption::TreatSingleBRElementAsVisible});
}

}  // namespace mozilla
