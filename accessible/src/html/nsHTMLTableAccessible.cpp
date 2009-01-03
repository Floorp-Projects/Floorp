/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLTableAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessibleTreeWalker.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMRange.h"
#include "nsISelection2.h"
#include "nsISelectionPrivate.h"
#include "nsINameSpaceManager.h"
#include "nsIAccessibilityService.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsLayoutErrors.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTableCellAccessible, nsHyperTextAccessible)

nsHTMLTableCellAccessible::nsHTMLTableCellAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aDomNode, aShell)
{ 
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLTableCellAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = nsIAccessibleRole::ROLE_CELL;
  return NS_OK;
}

nsresult
nsHTMLTableCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = nsHyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_STATE(shell);
  
  nsIFrame *frame = shell->GetPrimaryFrameFor(content);
  NS_ASSERTION(frame, "The frame cannot be obtaied for HTML table cell.");
  NS_ENSURE_STATE(frame);

  nsITableCellLayout *cellLayout = nsnull;
  CallQueryInterface(frame, &cellLayout);
  NS_ENSURE_STATE(cellLayout);

  PRInt32 rowIdx = -1, cellIdx = -1;
  rv = cellLayout->GetCellIndexes(rowIdx, cellIdx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessible> childAcc(this);

  nsCOMPtr<nsIAccessible> parentAcc;
  rv = childAcc->GetParent(getter_AddRefs(parentAcc));
  NS_ENSURE_SUCCESS(rv, rv);

  while (parentAcc) {
    if (nsAccUtils::Role(parentAcc) == nsIAccessibleRole::ROLE_TABLE) {
      // Table accessible must implement nsIAccessibleTable interface but if
      // it isn't happen (for example because of ARIA usage) we shouldn't fail
      // on getting other attributes.
      nsCOMPtr<nsIAccessibleTable> tableAcc(do_QueryInterface(parentAcc));
      if (!tableAcc)
        return NS_OK;

      PRInt32 idx = -1;
      rv = tableAcc->GetIndexAt(rowIdx, cellIdx, &idx);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString stringIdx;
      stringIdx.AppendInt(idx);
      nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::tableCellIndex,
                             stringIdx);
      return NS_OK;
    }

    parentAcc.swap(childAcc);
    rv = childAcc->GetParent(getter_AddRefs(parentAcc));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLTableAccessible, nsAccessible, nsIAccessibleTable)

nsHTMLTableAccessible::nsHTMLTableAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsAccessibleWrap(aDomNode, aShell)
{ 
}

void nsHTMLTableAccessible::CacheChildren()
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }
  
  if (mAccChildCount == eChildCountUninitialized) {
    nsAccessible::CacheChildren();
    nsCOMPtr<nsIAccessible> captionAccessible;
    while (NextChild(captionAccessible)) {
      if (nsAccUtils::Role(captionAccessible) == nsIAccessibleRole::ROLE_CAPTION) {
        nsCOMPtr<nsIAccessible> captionParentAccessible;
        captionAccessible->GetParent(getter_AddRefs(captionParentAccessible));
        if (captionParentAccessible != this) {
          NS_WARNING("Should not happen: parser ensures caption is the table's child, not the tbody's");
          return;
        }
        nsCOMPtr<nsIAccessible> beforeCaptionAccessible;
        captionAccessible->GetPreviousSibling(getter_AddRefs(beforeCaptionAccessible));
        if (beforeCaptionAccessible) {
          // Move caption accessible so that it's the first child
          nsCOMPtr<nsIAccessible> afterCaptionAccessible;
          captionAccessible->GetNextSibling(getter_AddRefs(afterCaptionAccessible));
          nsCOMPtr<nsPIAccessible> privateAcc = do_QueryInterface(beforeCaptionAccessible);
          privateAcc->SetNextSibling(afterCaptionAccessible);
          GetFirstChild(getter_AddRefs(afterCaptionAccessible));
          SetFirstChild(captionAccessible);
          privateAcc = do_QueryInterface(captionAccessible);
          privateAcc->SetNextSibling(afterCaptionAccessible);        
        }
        // Don't check for more captions, because nsAccessibilityService ensures
        // we don't create accessibles for the other captions, since only the
        // first is actually visible
        break;
      }
    }
  }
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLTableAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = nsIAccessibleRole::ROLE_TABLE;
  return NS_OK;
}

nsresult
nsHTMLTableAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv= nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

nsresult
nsHTMLTableAccessible::GetNameInternal(nsAString& aName)
{
  nsAccessible::GetNameInternal(aName);

  if (aName.IsEmpty()) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    if (content) { 
      content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::summary, aName);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLTableAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }

  nsresult rv = nsAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool isProbablyForLayout;
  IsProbablyForLayout(&isProbablyForLayout);
  if (isProbablyForLayout) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("layout-guess"),
                                   NS_LITERAL_STRING("true"), oldValueUnused);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                            nsIAccessible **aRelated)
{
  NS_ENSURE_ARG_POINTER(aRelated);
  *aRelated = nsnull;

  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = nsAccessibleWrap::GetAccessibleRelated(aRelationType, aRelated);
  if (NS_FAILED(rv) || *aRelated) {
    // Either the node is shut down, or another relation mechanism has been used
    return rv;
  }

  if (aRelationType == nsIAccessibleRelation::RELATION_DESCRIBED_BY) {
    return GetCaption(aRelated);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableAccessible::GetCaption(nsIAccessible **aCaption)
{
  nsCOMPtr<nsIAccessible> firstChild;
  GetFirstChild(getter_AddRefs(firstChild));
  if (nsAccUtils::Role(firstChild) == nsIAccessibleRole::ROLE_CAPTION)
    NS_ADDREF(*aCaption = firstChild);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSummary(nsAString &aSummary)
{
  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  return table->GetSummary(aSummary);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumns(PRInt32 *aColumns)
{
  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rows;
  return tableLayout->GetTableSize(rows, *aColumns);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;
  rv = table->GetTHead(getter_AddRefs(section));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibilityService>
    accService(do_GetService("@mozilla.org/accessibilityService;1"));
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessible> accHead;
  nsCOMPtr<nsIDOMNode> sectionNode(do_QueryInterface(section));
  if (sectionNode) {
    rv = accService->GetCachedAccessible(sectionNode, mWeakShell,
                                         getter_AddRefs(accHead));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!accHead) {
    accService->CreateHTMLTableHeadAccessible(section, getter_AddRefs(accHead));
    NS_ENSURE_STATE(accHead);

    nsRefPtr<nsAccessNode> accessNode = nsAccUtils::QueryAccessNode(accHead);
    rv = accessNode->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIAccessibleTable> accTableHead(do_QueryInterface(accHead));
  NS_ENSURE_TRUE(accTableHead, NS_ERROR_FAILURE);

  *aColumnHeader = accTableHead;
  NS_IF_ADDREF(*aColumnHeader);

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRows(PRInt32 *aRows)
{
  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columns;
  return tableLayout->GetTableSize(*aRows, columns);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  // Can not implement because there is no row header in html table
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedCellsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 rowsCount = 0;
  nsresult rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowIndex;
  for (rowIndex = 0; rowIndex < rowsCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnsCount; columnIndex++) {
      PRBool state = PR_FALSE;
      rv = IsCellSelected(rowIndex, columnIndex, &state);
      NS_ENSURE_SUCCESS(rv, rv);

      if (state)
        (*aCount)++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedColumnsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 count = 0;
  nsresult rv = GetColumns(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index;
  for (index = 0; index < count; index++) {
    PRBool state = PR_FALSE;
    rv = IsColumnSelected(index, &state);
    NS_ENSURE_SUCCESS(rv, rv);

    if (state)
      (*aCount)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedRowsCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 count = 0;
  nsresult rv = GetRows(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index;
  for (index = 0; index < count; index++) {
    PRBool state = PR_FALSE;
    rv = IsRowSelected(index, &state);
    NS_ENSURE_SUCCESS(rv, rv);

    if (state)
      (*aCount)++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedCells(PRUint32 *aNumCells,
                                        PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aNumCells);
  *aNumCells = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  PRInt32 rowsCount = 0;
  nsresult rv = GetRows(&rowsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnsCount = 0;
  rv = GetColumns(&columnsCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 cellsCount = columnsCount * rowsCount;
  nsAutoArrayPtr<PRBool> states(new PRBool[cellsCount]);
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 rowIndex, index;
  for (rowIndex = 0, index = 0; rowIndex < rowsCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnsCount; columnIndex++, index++) {
      rv = IsCellSelected(rowIndex, columnIndex, &states[index]);
      NS_ENSURE_SUCCESS(rv, rv);

      if (states[index])
        (*aNumCells)++;
    }
  }

  PRInt32 *cellsArray =
    (PRInt32 *)nsMemory::Alloc((*aNumCells) * sizeof(PRInt32));
  NS_ENSURE_TRUE(cellsArray, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 curr = 0;
  for (rowIndex = 0, index = 0; rowIndex < rowsCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnsCount; columnIndex++, index++) {
      if (states[index]) {
        PRInt32 cellIndex = -1;
        GetIndexAt(rowIndex, columnIndex, &cellIndex);
        cellsArray[curr++] = cellIndex;
      }
    }
  }

  *aCells = cellsArray;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedColumns(PRUint32 *aNumColumns,
                                          PRInt32 **aColumns)
{
  nsresult rv = NS_OK;

  PRInt32 columnCount;
  rv = GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool *states = new PRBool[columnCount];
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  *aNumColumns = 0;
  PRInt32 index;
  for (index = 0; index < columnCount; index++) {
    rv = IsColumnSelected(index, &states[index]);
    NS_ENSURE_SUCCESS(rv, rv);

    if (states[index]) {
      (*aNumColumns)++;
    }
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumColumns) * sizeof(PRInt32));
  if (!outArray) {
    delete []states;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 curr = 0;
  for (index = 0; index < columnCount; index++) {
    if (states[index]) {
      outArray[curr++] = index;
    }
  }

  delete []states;
  *aColumns = outArray;
  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
{
  nsresult rv = NS_OK;

  PRInt32 rowCount;
  rv = GetRows(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool *states = new PRBool[rowCount];
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  *aNumRows = 0;
  PRInt32 index;
  for (index = 0; index < rowCount; index++) {
    rv = IsRowSelected(index, &states[index]);
    NS_ENSURE_SUCCESS(rv, rv);

    if (states[index]) {
      (*aNumRows)++;
    }
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumRows) * sizeof(PRInt32));
  if (!outArray) {
    delete []states;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 curr = 0;
  for (index = 0; index < rowCount; index++) {
    if (states[index]) {
      outArray[curr++] = index;
    }
  }

  delete []states;
  *aRows = outArray;
  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::CellRefAt(PRInt32 aRow, PRInt32 aColumn,
                                 nsIAccessible **aTableCellAccessible)
{
  NS_ENSURE_TRUE(IsValidRow(aRow) && IsValidColumn(aColumn), NS_ERROR_INVALID_ARG);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> cellElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibilityService>
    accService(do_GetService("@mozilla.org/accessibilityService;1"));
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  return accService->GetAccessibleInWeakShell(cellElement, mWeakShell,
                                              aTableCellAccessible);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetIndexAt(PRInt32 aRow, PRInt32 aColumn,
                                  PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  NS_ENSURE_TRUE(IsValidRow(aRow) && IsValidColumn(aColumn), NS_ERROR_INVALID_ARG);

  nsITableLayout *tableLayout = nsnull;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  return tableLayout->GetIndexByRowAndColumn(aRow, aColumn, aIndex);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *aColumn)
{
  NS_ENSURE_ARG_POINTER(aColumn);

  nsITableLayout *tableLayout = nsnull;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 row;
  return tableLayout->GetRowAndColumnByIndex(aIndex, &row, aColumn);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowAtIndex(PRInt32 aIndex, PRInt32 *aRow)
{
  NS_ENSURE_ARG_POINTER(aRow);

  nsITableLayout *tableLayout = nsnull;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 column;
  return tableLayout->GetRowAndColumnByIndex(aIndex, aRow, &column);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                         PRInt32 *_retval)
{
  NS_ENSURE_TRUE(IsValidRow(aRow) && IsValidColumn(aColumn), NS_ERROR_INVALID_ARG);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> domElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLTableCellElement> cell(do_QueryInterface(domElement));
  NS_ENSURE_TRUE(cell, NS_ERROR_FAILURE);

  return cell->GetColSpan(_retval);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                      PRInt32 *_retval)
{
  NS_ENSURE_TRUE(IsValidRow(aRow) && IsValidColumn(aColumn), NS_ERROR_INVALID_ARG);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> domElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLTableCellElement> cell(do_QueryInterface(domElement));
  NS_ENSURE_TRUE(cell, NS_ERROR_FAILURE);

  return cell->GetRowSpan(_retval);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnDescription(PRInt32 aColumn, nsAString &_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowDescription(PRInt32 aRow, nsAString &_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  NS_ENSURE_TRUE(IsValidColumn(aColumn), NS_ERROR_INVALID_ARG);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 index = 0; index < rows; index++) {
    rv = IsCellSelected(index, aColumn, _retval);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!*_retval) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  NS_ENSURE_TRUE(IsValidRow(aRow), NS_ERROR_INVALID_ARG);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 index = 0; index < columns; index++) {
    rv = IsCellSelected(aRow, index, _retval);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!*_retval) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                      PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  NS_ENSURE_TRUE(IsValidRow(aRow) && IsValidColumn(aColumn),
                 NS_ERROR_INVALID_ARG);

  nsITableLayout *tableLayout = nsnull;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;

  rv = tableLayout->GetCellDataAt(aRow, aColumn, *getter_AddRefs(domElement),
                                  startRowIndex, startColIndex,
                                  rowSpan, colSpan,
                                  actualRowSpan, actualColSpan, *aIsSelected);

  if (rv == NS_TABLELAYOUT_CELL_NOT_FOUND)
    return NS_ERROR_INVALID_ARG;
  return rv;
}

PRBool
nsHTMLTableAccessible::IsValidColumn(PRInt32 aColumn)
{
  PRInt32 colCount = 0;
  nsresult rv = GetColumns(&colCount);
  return NS_SUCCEEDED(rv) && (aColumn >= 0) && (aColumn < colCount);
}

PRBool
nsHTMLTableAccessible::IsValidRow(PRInt32 aRow)
{
  PRInt32 rowCount = 0;
  nsresult rv = GetRows(&rowCount);
  return NS_SUCCEEDED(rv) && (aRow >= 0) && (aRow < rowCount);
}

NS_IMETHODIMP
nsHTMLTableAccessible::SelectRow(PRInt32 aRow)
{
  return SelectRowOrColumn(aRow, nsISelectionPrivate::TABLESELECTION_ROW,
                           PR_TRUE);
}

NS_IMETHODIMP
nsHTMLTableAccessible::SelectColumn(PRInt32 aColumn)
{
  return SelectRowOrColumn(aColumn, nsISelectionPrivate::TABLESELECTION_COLUMN,
                           PR_TRUE);
}

NS_IMETHODIMP
nsHTMLTableAccessible::UnselectRow(PRInt32 aRow)
{
  return SelectRowOrColumn(aRow, nsISelectionPrivate::TABLESELECTION_ROW,
                           PR_FALSE);
}

NS_IMETHODIMP
nsHTMLTableAccessible::UnselectColumn(PRInt32 aColumn)
{
  return SelectRowOrColumn(aColumn, nsISelectionPrivate::TABLESELECTION_COLUMN,
                           PR_FALSE);
}

nsresult
nsHTMLTableAccessible::SelectRowOrColumn(PRInt32 aIndex, PRUint32 aTarget,
                                         PRBool aDoSelect)
{
  PRBool doSelectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content)
    return NS_OK;

  nsCOMPtr<nsIDocument> document = content->GetCurrentDoc();
  NS_ENSURE_STATE(document);

  nsCOMPtr<nsISelectionController> selController(
    do_QueryInterface(document->GetPrimaryShell()));
  NS_ENSURE_STATE(selController);

  nsCOMPtr<nsISelection> selection;
  selController->GetSelection(nsISelectionController::SELECTION_NORMAL,
                              getter_AddRefs(selection));
  NS_ENSURE_STATE(selection);

  PRInt32 count = 0;
  nsresult rv = doSelectRow ? GetColumns(&count) : GetRows(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMElement> cellElm;
    PRInt32 column = doSelectRow ? index : aIndex;
    PRInt32 row = doSelectRow ? aIndex : index;

    rv = GetCellAt(row, column, *getter_AddRefs(cellElm));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SelectCell(selection, document, cellElm, aDoSelect);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

nsresult
nsHTMLTableAccessible::SelectCell(nsISelection *aSelection,
                                  nsIDocument *aDocument,
                                  nsIDOMElement *aCellElement,
                                  PRBool aDoSelect)
{
  if (aDoSelect) {
    nsCOMPtr<nsIDOMDocumentRange> documentRange(do_QueryInterface(aDocument));
    NS_ENSURE_STATE(documentRange);

    nsCOMPtr<nsIDOMRange> range;
    documentRange->CreateRange(getter_AddRefs(range));

    nsCOMPtr<nsIDOMNode> cellNode(do_QueryInterface(aCellElement));
    NS_ENSURE_STATE(cellNode);

    range->SelectNode(cellNode);
    return aSelection->AddRange(range);
  }

  nsCOMPtr<nsIContent> cell(do_QueryInterface(aCellElement));
  NS_ENSURE_STATE(cell);

  nsCOMPtr<nsIContent> cellParent = cell->GetParent();
  NS_ENSURE_STATE(cellParent);

  PRInt32 offset = cellParent->IndexOf(cell);
  NS_ENSURE_STATE(offset != -1);

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(cellParent));
  NS_ENSURE_STATE(parent);

  nsCOMPtr<nsISelection2> selection2(do_QueryInterface(aSelection));
  NS_ENSURE_STATE(selection2);

  nsCOMArray<nsIDOMRange> ranges;
  nsresult rv = selection2->GetRangesForIntervalCOMArray(parent, offset,
                                                         parent, offset,
                                                         PR_TRUE, &ranges);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < ranges.Count(); i ++)
    aSelection->RemoveRange(ranges[i]);

  return NS_OK;
}

nsresult
nsHTMLTableAccessible::GetTableNode(nsIDOMNode **_retval)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  if (table) {
    *_retval = table;
    NS_IF_ADDREF(*_retval);
    return rv;
  }

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section(do_QueryInterface(mDOMNode));
  if (section) {
    nsCOMPtr<nsIDOMNode> parent;
    rv = section->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    *_retval = parent;
    NS_IF_ADDREF(*_retval);
    return rv;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsHTMLTableAccessible::GetTableLayout(nsITableLayout **aTableLayout)
{
  *aTableLayout = nsnull;

  nsCOMPtr<nsIDOMNode> tableNode;
  GetTableNode(getter_AddRefs(tableNode));
  nsCOMPtr<nsIContent> tableContent(do_QueryInterface(tableNode));
  if (!tableContent) {
    return NS_ERROR_FAILURE; // Table shut down
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIFrame *frame = shell->GetPrimaryFrameFor(tableContent);
  return frame ? CallQueryInterface(frame, aTableLayout) : NS_ERROR_FAILURE;
}

nsresult
nsHTMLTableAccessible::GetCellAt(PRInt32        aRowIndex,
                                 PRInt32        aColIndex,
                                 nsIDOMElement* &aCell)
{
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected;

  nsITableLayout *tableLayout = nsnull;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = tableLayout->GetCellDataAt(aRowIndex, aColIndex, aCell,
                                  startRowIndex, startColIndex,
                                  rowSpan, colSpan,
                                  actualRowSpan, actualColSpan, isSelected);

  if (rv == NS_TABLELAYOUT_CELL_NOT_FOUND)
    return NS_ERROR_INVALID_ARG;
  return rv;
}

NS_IMETHODIMP nsHTMLTableAccessible::GetDescription(nsAString& aDescription)
{
  // Helpful for debugging layout vs. data tables
  aDescription.Truncate();
  nsAccessible::GetDescription(aDescription);
  if (!aDescription.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIAccessible> captionAccessible;
  GetCaption(getter_AddRefs(captionAccessible));
  nsCOMPtr<nsIAccessNode> captionAccessNode = do_QueryInterface(captionAccessible);
  if (captionAccessNode) {
    nsCOMPtr<nsIDOMNode> captionNode;
    captionAccessNode->GetDOMNode(getter_AddRefs(captionNode));
    nsCOMPtr<nsIContent> captionContent = do_QueryInterface(captionNode);
    if (captionContent) {
      AppendFlatStringFromSubtree(captionContent, &aDescription);
    }
  }
#ifdef SHOW_LAYOUT_HEURISTIC
  if (aDescription.IsEmpty()) {
    PRBool isProbablyForLayout;
    IsProbablyForLayout(&isProbablyForLayout);
    aDescription = mLayoutHeuristic;
  }
#ifdef DEBUG_A11Y
  printf("\nTABLE: %s\n", NS_ConvertUTF16toUTF8(mLayoutHeuristic).get());
#endif
#endif

  return NS_OK;
}

PRBool nsHTMLTableAccessible::HasDescendant(char *aTagName, PRBool aAllowEmpty)
{
  nsCOMPtr<nsIDOMElement> tableElt(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(tableElt, PR_FALSE);

  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsAutoString tagName;
  tagName.AssignWithConversion(aTagName);
  tableElt->GetElementsByTagName(tagName, getter_AddRefs(nodeList));
  NS_ENSURE_TRUE(nodeList, PR_FALSE);
  PRUint32 length;
  nodeList->GetLength(&length);
  
  if (length == 1) {
    // Make sure it's not the table itself
    nsCOMPtr<nsIDOMNode> foundItem;
    nodeList->Item(0, getter_AddRefs(foundItem));
    if (foundItem == mDOMNode) {
      return PR_FALSE;
    }
    if (!aAllowEmpty) {
      // Make sure that the item we found has contents
      // and either has multiple children or the
      // found item is not a whitespace-only text node
      nsCOMPtr<nsIContent> foundItemContent = do_QueryInterface(foundItem);
      if (!foundItemContent) {
        return PR_FALSE;
      }
      if (foundItemContent->GetChildCount() > 1) {
        return PR_TRUE; // Treat multiple child nodes as non-empty
      }
      nsIContent *innerItemContent = foundItemContent->GetChildAt(0);
      if (!innerItemContent || innerItemContent->TextIsOnlyWhitespace()) {
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }

  return length > 0;
}

NS_IMETHODIMP nsHTMLTableAccessible::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  // Implement a heuristic to determine if table is most likely used for layout
  // XXX do we want to look for rowspan or colspan, especialy that span all but a couple cells
  // at the beginning or end of a row/col, and especially when they occur at the edge of a table?
  // XXX expose this info via object attributes to AT-SPI

  // XXX For now debugging descriptions are always on via SHOW_LAYOUT_HEURISTIC
  // This will allow release trunk builds to be used by testers to refine the algorithm
  // Change to |#define SHOW_LAYOUT_HEURISTIC DEBUG| before final release
#ifdef SHOW_LAYOUT_HEURISTIC
#define RETURN_LAYOUT_ANSWER(isLayout, heuristic) \
  { *aIsProbablyForLayout = isLayout; \
    mLayoutHeuristic = isLayout ? NS_LITERAL_STRING("layout table: ") : NS_LITERAL_STRING("data table: "); \
    mLayoutHeuristic += NS_LITERAL_STRING(heuristic); return NS_OK; }
#else
#define RETURN_LAYOUT_ANSWER(isLayout, heuristic) { *aIsProbablyForLayout = isLayout; return NS_OK; }
#endif

  *aIsProbablyForLayout = PR_FALSE;
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE; // Table shut down
  }

  nsCOMPtr<nsIAccessible> docAccessible = do_QueryInterface(nsCOMPtr<nsIAccessibleDocument>(GetDocAccessible()));
  if (docAccessible) {
    PRUint32 state, extState;
    docAccessible->GetState(&state, &extState);
    if (extState & nsIAccessibleStates::EXT_STATE_EDITABLE) {  // Need to see all elements while document is being edited
      RETURN_LAYOUT_ANSWER(PR_FALSE, "In editable document");
    }
  }

  // Check role and role attribute
  PRBool hasNonTableRole =
    (nsAccUtils::Role(this) != nsIAccessibleRole::ROLE_TABLE);
  if (hasNonTableRole) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has role attribute");
  }

  if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has role attribute, and role is table");
  }
  
  // Check for legitimate data table elements or attributes
  nsAutoString summary;
  if ((content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::summary, summary) && !summary.IsEmpty()) || 
      HasDescendant("caption", PR_FALSE) || HasDescendant("th") || HasDescendant("thead") ||
      HasDescendant("tfoot")   || HasDescendant("colgroup")) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has caption, summary, th, thead, tfoot or colgroup -- legitimate table structures");
  }
  if (HasDescendant("table")) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has a nested table within it");
  }
  
  // If only 1 column or only 1 row, it's for layout
  PRInt32 columns, rows;
  GetColumns(&columns);
  if (columns <=1) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has only 1 column");
  }
  GetRows(&rows);
  if (rows <=1) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has only 1 row");
  }

  // Check for many columns
  if (columns >= 5) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, ">=5 columns");
  }
  
  // Now we know there are 2-4 columns and 2 or more rows
  // Check to see if there are visible borders on the cells
  // XXX currently, we just check the first cell -- do we really need to do more?
  nsCOMPtr<nsIDOMElement> cellElement;
  nsresult rv = GetCellAt(0, 0, *getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
  NS_ENSURE_TRUE(cellContent, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  nsIFrame *cellFrame = shell->GetPrimaryFrameFor(cellContent);
  if (!cellFrame) {
    return NS_OK;
  }
  nsMargin border;
  cellFrame->GetBorder(border);
  if (border.top && border.bottom && border.left && border.right) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has nonzero border-width on table cell");
  }

  /**
   * Rules for non-bordered tables with 2-4 columns and 2+ rows from here on forward
   */

  // Check for styled background color across the row
  // Alternating background color is a common way 
  nsCOMPtr<nsIDOMNodeList> nodeList;
  nsCOMPtr<nsIDOMElement> tableElt(do_QueryInterface(mDOMNode));    
  tableElt->GetElementsByTagName(NS_LITERAL_STRING("tr"), getter_AddRefs(nodeList));
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);
  PRUint32 length;
  nodeList->GetLength(&length);
  nsAutoString color, lastRowColor;
  for (PRInt32 rowCount = 0; rowCount < rows; rowCount ++) {
    nsCOMPtr<nsIDOMNode> rowNode;
    nodeList->Item(rowCount, getter_AddRefs(rowNode));

    nsCOMPtr<nsIDOMCSSStyleDeclaration> styleDecl;
    nsCoreUtils::GetComputedStyleDeclaration(EmptyString(), rowNode,
                                             getter_AddRefs(styleDecl));
    NS_ENSURE_TRUE(styleDecl, NS_ERROR_FAILURE);

    lastRowColor = color;
    styleDecl->GetPropertyValue(NS_LITERAL_STRING("background-color"), color);
    if (rowCount > 0 && PR_FALSE == lastRowColor.Equals(color)) {
      RETURN_LAYOUT_ANSWER(PR_FALSE, "2 styles of row background color, non-bordered");
    }
  }

  // Check for many rows
  const PRInt32 kMaxLayoutRows = 20;
  if (rows > kMaxLayoutRows) { // A ton of rows, this is probably for data
    RETURN_LAYOUT_ANSWER(PR_FALSE, ">= kMaxLayoutRows (20) and non-bordered");
  }

  // Check for very wide table
  nsAutoString styledWidth;
  GetComputedStyleValue(EmptyString(), NS_LITERAL_STRING("width"), styledWidth);
  if (styledWidth.EqualsLiteral("100%")) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "<=4 columns and 100% width");
  }
  if (styledWidth.Find(NS_LITERAL_STRING("px"))) { // Hardcoded in pixels
    nsIFrame *tableFrame = GetFrame();
    NS_ENSURE_TRUE(tableFrame , NS_ERROR_FAILURE);
    nsSize tableSize  = tableFrame->GetSize();

    nsCOMPtr<nsIAccessibleDocument> docAccessible = GetDocAccessible();
    NS_ENSURE_TRUE(docAccessible, NS_ERROR_FAILURE);

    nsRefPtr<nsAccessNode> docAccessNode = nsAccUtils::QueryAccessNode(docAccessible);

    nsIFrame *docFrame = docAccessNode->GetFrame();
    NS_ENSURE_TRUE(docFrame , NS_ERROR_FAILURE);

    nsSize docSize = docFrame->GetSize();
    if (docSize.width > 0) {
      PRInt32 percentageOfDocWidth = (100 * tableSize.width) / docSize.width;
      if (percentageOfDocWidth > 95) {
        // 3-4 columns, no borders, not a lot of rows, and 95% of the doc's width
        // Probably for layout
        RETURN_LAYOUT_ANSWER(PR_TRUE, "<=4 columns, width hardcoded in pixels and 95% of document width");
      }
    }
  }

  // Two column rules
  if (rows * columns <= 10) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "2-4 columns, 10 cells or less, non-bordered");
  }

  if (HasDescendant("embed") || HasDescendant("object") || HasDescendant("applet") || HasDescendant("iframe")) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has no borders, and has iframe, object, applet or iframe, typical of advertisements");
  }

  RETURN_LAYOUT_ANSWER(PR_FALSE, "no layout factor strong enough, so will guess data");
}

// --------------------------------------------------------
// nsHTMLTableHeadAccessible Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTableHeadAccessible, nsHTMLTableAccessible)

nsHTMLTableHeadAccessible::nsHTMLTableHeadAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell):
nsHTMLTableAccessible(aDomNode, aShell)
{
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = nsIAccessibleRole::ROLE_COLUMNHEADER;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetCaption(nsIAccessible **aCaption)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetSummary(nsAString &aSummary)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetRows(PRInt32 *aRows)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableSectionElement> head(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(head, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  rv = head->GetRows(getter_AddRefs(rows));
  NS_ENSURE_SUCCESS(rv, rv);

  return rows->GetLength((PRUint32 *)aRows);
}

NS_IMETHODIMP
nsHTMLCaptionAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                              nsIAccessible **aRelated)
{
  NS_ENSURE_ARG_POINTER(aRelated);
  *aRelated = nsnull;

  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = nsHyperTextAccessible::GetAccessibleRelated(aRelationType, aRelated);
  if (NS_FAILED(rv) || *aRelated) {
    // Either the node is shut down, or another relation mechanism has been used
    return rv;
  }

  if (aRelationType == nsIAccessibleRelation::RELATION_DESCRIPTION_FOR) {
    return GetParent(aRelated);
  }

  return NS_OK;
}



