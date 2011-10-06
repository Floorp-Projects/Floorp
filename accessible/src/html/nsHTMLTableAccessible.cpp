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
 *   Aaron Leventhal <aaronl@netscape.com> (original author)
 *   Alexander Surkov <surkov.alexander@gmail.com>
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

#include "Relation.h"
#include "States.h"
#include "nsAccessibilityService.h"
#include "nsAccTreeWalker.h"
#include "nsAccUtils.h"
#include "nsDocAccessible.h"
#include "nsTextEquivUtils.h"

#include "nsIAccessibleRelation.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsISelectionPrivate.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsFrameSelection.h"
#include "nsLayoutErrors.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTableCellAccessible::
  nsHTMLTableCellAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLTableCellAccessible,
                             nsHyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible: nsAccessible implementation

PRUint32
nsHTMLTableCellAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CELL;
}

PRUint64
nsHTMLTableCellAccessible::NativeState()
{
  PRUint64 state = nsHyperTextAccessibleWrap::NativeState();

  nsIFrame *frame = mContent->GetPrimaryFrame();
  NS_ASSERTION(frame, "No frame for valid cell accessible!");

  if (frame) {
    state |= states::SELECTABLE;
    PRBool isSelected = PR_FALSE;
    frame->GetSelected(&isSelected);
    if (isSelected)
      state |= states::SELECTED;
  }

  return state;
}

nsresult
nsHTMLTableCellAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = nsHyperTextAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibleTable> tableAcc(GetTableAccessible());
  if (!tableAcc)
    return NS_OK;

  PRInt32 rowIdx = -1, colIdx = -1;
  rv = GetCellIndexes(rowIdx, colIdx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 idx = -1;
  rv = tableAcc->GetCellIndexAt(rowIdx, colIdx, &idx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString stringIdx;
  stringIdx.AppendInt(idx);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::tableCellIndex, stringIdx);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible: nsIAccessibleTableCell implementation

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetTable(nsIAccessibleTable **aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  if (IsDefunct())
    return NS_OK;

  nsCOMPtr<nsIAccessibleTable> table = GetTableAccessible();
  table.swap(*aTable);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetColumnIndex(PRInt32 *aColumnIndex)
{
  NS_ENSURE_ARG_POINTER(aColumnIndex);
  *aColumnIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableCellLayout* cellLayout = GetCellLayout();
  NS_ENSURE_STATE(cellLayout);

  return cellLayout->GetColIndex(*aColumnIndex);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetRowIndex(PRInt32 *aRowIndex)
{
  NS_ENSURE_ARG_POINTER(aRowIndex);
  *aRowIndex = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableCellLayout* cellLayout = GetCellLayout();
  NS_ENSURE_STATE(cellLayout);

  return cellLayout->GetRowIndex(*aRowIndex);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetColumnExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  PRInt32 rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  nsCOMPtr<nsIAccessibleTable> table = GetTableAccessible();
  NS_ENSURE_STATE(table);

  return table->GetColumnExtentAt(rowIdx, colIdx, aExtentCount);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetRowExtent(PRInt32 *aExtentCount)
{
  NS_ENSURE_ARG_POINTER(aExtentCount);
  *aExtentCount = 1;

  PRInt32 rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  nsCOMPtr<nsIAccessibleTable> table = GetTableAccessible();
  NS_ENSURE_STATE(table);

  return table->GetRowExtentAt(rowIdx, colIdx, aExtentCount);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetColumnHeaderCells(nsIArray **aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return GetHeaderCells(nsAccUtils::eColumnHeaderCells, aHeaderCells);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::GetRowHeaderCells(nsIArray **aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return GetHeaderCells(nsAccUtils::eRowHeaderCells, aHeaderCells);
}

NS_IMETHODIMP
nsHTMLTableCellAccessible::IsSelected(PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  nsCOMPtr<nsIAccessibleTable> table = GetTableAccessible();
  NS_ENSURE_STATE(table);

  return table->IsCellSelected(rowIdx, colIdx, aIsSelected);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableCellAccessible: protected implementation

already_AddRefed<nsIAccessibleTable>
nsHTMLTableCellAccessible::GetTableAccessible()
{
  nsAccessible* parent = this;
  while ((parent = parent->Parent())) {
    PRUint32 role = parent->Role();
    if (role == nsIAccessibleRole::ROLE_TABLE ||
        role == nsIAccessibleRole::ROLE_TREE_TABLE) {
      nsIAccessibleTable* tableAcc = nsnull;
      CallQueryInterface(parent, &tableAcc);
      return tableAcc;
    }
  }

  return nsnull;
}

nsITableCellLayout*
nsHTMLTableCellAccessible::GetCellLayout()
{
  nsIFrame *frame = mContent->GetPrimaryFrame();
  NS_ASSERTION(frame, "The frame cannot be obtaied for HTML table cell.");
  if (!frame)
    return nsnull;

  nsITableCellLayout *cellLayout = do_QueryFrame(frame);
  return cellLayout;
}

nsresult
nsHTMLTableCellAccessible::GetCellIndexes(PRInt32& aRowIndex,
                                          PRInt32& aColIndex)
{
  nsITableCellLayout *cellLayout = GetCellLayout();
  NS_ENSURE_STATE(cellLayout);

  return cellLayout->GetCellIndexes(aRowIndex, aColIndex);
}

nsresult
nsHTMLTableCellAccessible::GetHeaderCells(PRInt32 aRowOrColumnHeaderCell,
                                          nsIArray **aHeaderCells)
{
  // Get header cells from @header attribute.
  IDRefsIterator iter(mContent, nsGkAtoms::headers);
  nsIContent* headerCellElm = iter.NextElem();
  if (headerCellElm) {
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMutableArray> headerCells =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 desiredRole = -1;
    if (aRowOrColumnHeaderCell == nsAccUtils::eRowHeaderCells)
      desiredRole = nsIAccessibleRole::ROLE_ROWHEADER;
    else if (aRowOrColumnHeaderCell == nsAccUtils::eColumnHeaderCells)
      desiredRole = nsIAccessibleRole::ROLE_COLUMNHEADER;

    do {
      nsAccessible* headerCell =
        GetAccService()->GetAccessibleInWeakShell(headerCellElm, mWeakShell);

      if (headerCell && headerCell->Role() == desiredRole)
        headerCells->AppendElement(static_cast<nsIAccessible*>(headerCell),
                                   PR_FALSE);
    } while ((headerCellElm = iter.NextElem()));

    NS_ADDREF(*aHeaderCells = headerCells);
    return NS_OK;
  }

  // Otherwise calculate header cells from hierarchy (see 11.4.3 "Algorithm to
  // find heading information" of w3c HTML 4.01).
  nsCOMPtr<nsIAccessibleTable> table = GetTableAccessible();
  if (table) {
    return nsAccUtils::GetHeaderCellsFor(table, this, aRowOrColumnHeaderCell,
                                         aHeaderCells);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableHeaderAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTableHeaderCellAccessible::
  nsHTMLTableHeaderCellAccessible(nsIContent *aContent,
                                  nsIWeakReference *aShell) :
  nsHTMLTableCellAccessible(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableHeaderAccessible: nsAccessible implementation

PRUint32
nsHTMLTableHeaderCellAccessible::NativeRole()
{
  // Check value of @scope attribute.
  static nsIContent::AttrValuesArray scopeValues[] =
    {&nsGkAtoms::col, &nsGkAtoms::row, nsnull};
  PRInt32 valueIdx = 
    mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::scope,
                              scopeValues, eCaseMatters);

  switch (valueIdx) {
    case 0:
      return nsIAccessibleRole::ROLE_COLUMNHEADER;
    case 1:
      return nsIAccessibleRole::ROLE_ROWHEADER;
  }

  // Assume it's columnheader if there are headers in siblings, oterwise
  // rowheader.
  nsIContent *parent = mContent->GetParent();
  if (!parent) {
    NS_ERROR("Deattached content on alive accessible?");
    return nsIAccessibleRole::ROLE_NOTHING;
  }

  PRInt32 indexInParent = parent->IndexOf(mContent);

  for (PRInt32 idx = indexInParent - 1; idx >= 0; idx--) {
    nsIContent* sibling = parent->GetChildAt(idx);
    if (sibling && sibling->IsElement()) {
      if (nsCoreUtils::IsHTMLTableHeader(sibling))
        return nsIAccessibleRole::ROLE_COLUMNHEADER;
      return nsIAccessibleRole::ROLE_ROWHEADER;
    }
  }

  PRInt32 childCount = parent->GetChildCount();
  for (PRInt32 idx = indexInParent + 1; idx < childCount; idx++) {
    nsIContent* sibling = parent->GetChildAt(idx);
    if (sibling && sibling->IsElement()) {
      if (nsCoreUtils::IsHTMLTableHeader(sibling))
        return nsIAccessibleRole::ROLE_COLUMNHEADER;
      return nsIAccessibleRole::ROLE_ROWHEADER;
    }
  }

  // No elements in siblings what means the table has one column only. Therefore
  // it should be column header.
  return nsIAccessibleRole::ROLE_COLUMNHEADER;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTableAccessible::
  nsHTMLTableAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED2(nsHTMLTableAccessible, nsAccessible,
                             nsHTMLTableAccessible, nsIAccessibleTable)


////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible: nsAccessible implementation

void
nsHTMLTableAccessible::CacheChildren()
{
  // Move caption accessible so that it's the first child. Check for the first
  // caption only, because nsAccessibilityService ensures we don't create
  // accessibles for the other captions, since only the first is actually
  // visible.
  nsAccTreeWalker walker(mWeakShell, mContent, GetAllowsAnonChildAccessibles());

  nsAccessible* child = nsnull;
  while ((child = walker.NextChild())) {
    if (child->Role() == nsIAccessibleRole::ROLE_CAPTION) {
      InsertChildAt(0, child);
      while ((child = walker.NextChild()) && AppendChild(child));
      break;
    }
    AppendChild(child);
  }
}

PRUint32
nsHTMLTableAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_TABLE;
}

PRUint64
nsHTMLTableAccessible::NativeState()
{
  return nsAccessible::NativeState() | states::READONLY;
}

nsresult
nsHTMLTableAccessible::GetNameInternal(nsAString& aName)
{
  nsAccessible::GetNameInternal(aName);
  if (!aName.IsEmpty())
    return NS_OK;

  // Use table caption as a name.
  nsAccessible* caption = Caption();
  if (caption) {
    nsIContent* captionContent = caption->GetContent();
    if (captionContent) {
      nsTextEquivUtils::AppendTextEquivFromContent(this, captionContent, &aName);
      if (!aName.IsEmpty())
        return NS_OK;
    }
  }

  // If no caption then use summary as a name.
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, aName);
  return NS_OK;
}

nsresult
nsHTMLTableAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
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

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible: nsIAccessible implementation

Relation
nsHTMLTableAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsAccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABELLED_BY)
    rel.AppendTarget(Caption());

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTableAccessible: nsIAccessibleTable implementation

NS_IMETHODIMP
nsHTMLTableAccessible::GetCaption(nsIAccessible **aCaption)
{
  NS_ENSURE_ARG_POINTER(aCaption);

  NS_IF_ADDREF(*aCaption = Caption());
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSummary(nsAString &aSummary)
{
  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mContent));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  return table->GetSummary(aSummary);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnCount(PRInt32 *acolumnCount)
{
  NS_ENSURE_ARG_POINTER(acolumnCount);
  *acolumnCount = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  PRInt32 rows;
  return tableLayout->GetTableSize(rows, *acolumnCount);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowCount(PRInt32 *arowCount)
{
  NS_ENSURE_ARG_POINTER(arowCount);
  *arowCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  PRInt32 columns;
  return tableLayout->GetTableSize(*arowCount, columns);
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedCellCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected = PR_FALSE;

  PRInt32 rowIndex;
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnCount; columnIndex++) {
      rv = tableLayout->GetCellDataAt(rowIndex, columnIndex,
                                      *getter_AddRefs(domElement),
                                      startRowIndex, startColIndex,
                                      rowSpan, colSpan,
                                      actualRowSpan, actualColSpan,
                                      isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIndex &&
          startColIndex == columnIndex && isSelected) {
        (*aCount)++;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedColumnCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 count = 0;
  nsresult rv = GetColumnCount(&count);
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
nsHTMLTableAccessible::GetSelectedRowCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  PRInt32 count = 0;
  nsresult rv = GetRowCount(&count);
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
nsHTMLTableAccessible::GetSelectedCells(nsIArray **aCells)
{
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIMutableArray> selCells =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> cellElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected = PR_FALSE;

  PRInt32 rowIndex, index;
  for (rowIndex = 0, index = 0; rowIndex < rowCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnCount; columnIndex++, index++) {
      rv = tableLayout->GetCellDataAt(rowIndex, columnIndex,
                                      *getter_AddRefs(cellElement),
                                      startRowIndex, startColIndex,
                                      rowSpan, colSpan,
                                      actualRowSpan, actualColSpan,
                                      isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIndex &&
          startColIndex == columnIndex && isSelected) {
        nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
        nsAccessible *cell =
          GetAccService()->GetAccessibleInWeakShell(cellContent, mWeakShell);
        selCells->AppendElement(static_cast<nsIAccessible*>(cell), PR_FALSE);
      }
    }
  }

  NS_ADDREF(*aCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedCellIndices(PRUint32 *aNumCells,
                                              PRInt32 **aCells)
{
  NS_ENSURE_ARG_POINTER(aNumCells);
  *aNumCells = 0;
  NS_ENSURE_ARG_POINTER(aCells);
  *aCells = nsnull;

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount = 0;
  rv = GetColumnCount(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected = PR_FALSE;

  PRInt32 cellsCount = columnCount * rowCount;
  nsAutoArrayPtr<PRBool> states(new PRBool[cellsCount]);
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 rowIndex, index;
  for (rowIndex = 0, index = 0; rowIndex < rowCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnCount; columnIndex++, index++) {
      rv = tableLayout->GetCellDataAt(rowIndex, columnIndex,
                                      *getter_AddRefs(domElement),
                                      startRowIndex, startColIndex,
                                      rowSpan, colSpan,
                                      actualRowSpan, actualColSpan,
                                      isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIndex &&
          startColIndex == columnIndex && isSelected) {
        states[index] = PR_TRUE;
        (*aNumCells)++;
      } else {
        states[index] = PR_FALSE;
      }
    }
  }

  PRInt32 *cellsArray =
    static_cast<PRInt32*>(nsMemory::Alloc((*aNumCells) * sizeof(PRInt32)));
  NS_ENSURE_TRUE(cellsArray, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 curr = 0;
  for (rowIndex = 0, index = 0; rowIndex < rowCount; rowIndex++) {
    PRInt32 columnIndex;
    for (columnIndex = 0; columnIndex < columnCount; columnIndex++, index++) {
      if (states[index]) {
        PRInt32 cellIndex = -1;
        GetCellIndexAt(rowIndex, columnIndex, &cellIndex);
        cellsArray[curr++] = cellIndex;
      }
    }
  }

  *aCells = cellsArray;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetSelectedColumnIndices(PRUint32 *aNumColumns,
                                                PRInt32 **aColumns)
{
  nsresult rv = NS_OK;

  PRInt32 columnCount;
  rv = GetColumnCount(&columnCount);
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
nsHTMLTableAccessible::GetSelectedRowIndices(PRUint32 *aNumRows,
                                             PRInt32 **aRows)
{
  nsresult rv = NS_OK;

  PRInt32 rowCount;
  rv = GetRowCount(&rowCount);
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
nsHTMLTableAccessible::GetCellAt(PRInt32 aRow, PRInt32 aColumn,
                                 nsIAccessible **aTableCellAccessible)
{
  nsCOMPtr<nsIDOMElement> cellElement;
  nsresult rv = GetCellAt(aRow, aColumn, *getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
  nsAccessible *cell =
    GetAccService()->GetAccessibleInWeakShell(cellContent, mWeakShell);

  if (!cell) {
    return NS_ERROR_INVALID_ARG;
  }

  if (cell != this) {
    // XXX bug 576838: crazy tables (like table6 in tables/test_table2.html) may
    // return itself as a cell what makes Orca hang.
    NS_ADDREF(*aTableCellAccessible = cell);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetCellIndexAt(PRInt32 aRow, PRInt32 aColumn,
                                      PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsresult rv = tableLayout->GetIndexByRowAndColumn(aRow, aColumn, aIndex);
  if (rv == NS_TABLELAYOUT_CELL_NOT_FOUND)
    return NS_ERROR_INVALID_ARG;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnIndexAt(PRInt32 aIndex, PRInt32 *aColumn)
{
  NS_ENSURE_ARG_POINTER(aColumn);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  PRInt32 row;
  nsresult rv = tableLayout->GetRowAndColumnByIndex(aIndex, &row, aColumn);
  NS_ENSURE_SUCCESS(rv, rv);

  return (row == -1 || *aColumn == -1) ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowIndexAt(PRInt32 aIndex, PRInt32 *aRow)
{
  NS_ENSURE_ARG_POINTER(aRow);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  PRInt32 column;
  nsresult rv = tableLayout->GetRowAndColumnByIndex(aIndex, aRow, &column);
  NS_ENSURE_SUCCESS(rv, rv);

  return (*aRow == -1 || column == -1) ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowAndColumnIndicesAt(PRInt32 aIndex,
                                                PRInt32* aRowIdx,
                                                PRInt32* aColumnIdx)
{
  NS_ENSURE_ARG_POINTER(aRowIdx);
  *aRowIdx = -1;
  NS_ENSURE_ARG_POINTER(aColumnIdx);
  *aColumnIdx = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsITableLayout* tableLayout = GetTableLayout();
  if (tableLayout)
    tableLayout->GetRowAndColumnByIndex(aIndex, aRowIdx, aColumnIdx);

  return (*aRowIdx == -1 || *aColumnIdx == -1) ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetColumnExtentAt(PRInt32 aRowIndex,
                                         PRInt32 aColumnIndex,
                                         PRInt32 *aExtentCount)
{
  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan;
  PRBool isSelected;

  nsresult rv = tableLayout->
    GetCellDataAt(aRowIndex, aColumnIndex, *getter_AddRefs(domElement),
                  startRowIndex, startColIndex, rowSpan, colSpan,
                  actualRowSpan, *aExtentCount, isSelected);

  return (rv == NS_TABLELAYOUT_CELL_NOT_FOUND) ? NS_ERROR_INVALID_ARG : NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::GetRowExtentAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                                      PRInt32 *aExtentCount)
{
  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex, startColIndex, rowSpan, colSpan, actualColSpan;
  PRBool isSelected;

  nsresult rv = tableLayout->
    GetCellDataAt(aRowIndex, aColumnIndex, *getter_AddRefs(domElement),
                  startRowIndex, startColIndex, rowSpan, colSpan,
                  *aExtentCount, actualColSpan, isSelected);

  return (rv == NS_TABLELAYOUT_CELL_NOT_FOUND) ? NS_ERROR_INVALID_ARG : NS_OK;
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
nsHTMLTableAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  PRInt32 colCount = 0;
  nsresult rv = GetColumnCount(&colCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aColumn < 0 || aColumn >= colCount)
    return NS_ERROR_INVALID_ARG;

  PRInt32 rowCount = 0;
  rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    PRBool isSelected = PR_FALSE;
    rv = IsCellSelected(rowIdx, aColumn, &isSelected);
    if (NS_SUCCEEDED(rv)) {
      *aIsSelected = isSelected;
      if (!isSelected)
        break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsRowSelected(PRInt32 aRow, PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  PRInt32 rowCount = 0;
  nsresult rv = GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRow < 0 || aRow >= rowCount)
    return NS_ERROR_INVALID_ARG;

  PRInt32 colCount = 0;
  rv = GetColumnCount(&colCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 colIdx = 0; colIdx < colCount; colIdx++) {
    PRBool isSelected = PR_FALSE;
    rv = IsCellSelected(aRow, colIdx, &isSelected);
    if (NS_SUCCEEDED(rv)) {
      *aIsSelected = isSelected;
      if (!isSelected)
        break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                      PRBool *aIsSelected)
{
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = PR_FALSE;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;

  nsresult rv = tableLayout->
    GetCellDataAt(aRow, aColumn, *getter_AddRefs(domElement),
                  startRowIndex, startColIndex, rowSpan, colSpan,
                  actualRowSpan, actualColSpan, *aIsSelected);

  if (rv == NS_TABLELAYOUT_CELL_NOT_FOUND)
    return NS_ERROR_INVALID_ARG;
  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessible::SelectRow(PRInt32 aRow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv =
    RemoveRowsOrColumnsFromSelection(aRow,
                                     nsISelectionPrivate::TABLESELECTION_ROW,
                                     PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return AddRowOrColumnToSelection(aRow,
                                   nsISelectionPrivate::TABLESELECTION_ROW);
}

NS_IMETHODIMP
nsHTMLTableAccessible::SelectColumn(PRInt32 aColumn)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv =
    RemoveRowsOrColumnsFromSelection(aColumn,
                                     nsISelectionPrivate::TABLESELECTION_COLUMN,
                                     PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return AddRowOrColumnToSelection(aColumn,
                                   nsISelectionPrivate::TABLESELECTION_COLUMN);
}

NS_IMETHODIMP
nsHTMLTableAccessible::UnselectRow(PRInt32 aRow)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return
    RemoveRowsOrColumnsFromSelection(aRow,
                                     nsISelectionPrivate::TABLESELECTION_ROW,
                                     PR_FALSE);
}

NS_IMETHODIMP
nsHTMLTableAccessible::UnselectColumn(PRInt32 aColumn)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  return
    RemoveRowsOrColumnsFromSelection(aColumn,
                                     nsISelectionPrivate::TABLESELECTION_COLUMN,
                                     PR_FALSE);
}

nsresult
nsHTMLTableAccessible::AddRowOrColumnToSelection(PRInt32 aIndex,
                                                 PRUint32 aTarget)
{
  PRBool doSelectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> cellElm;
  PRInt32 startRowIdx, startColIdx, rowSpan, colSpan,
    actualRowSpan, actualColSpan;
  PRBool isSelected = PR_FALSE;

  nsresult rv = NS_OK;
  PRInt32 count = 0;
  if (doSelectRow)
    rv = GetColumnCount(&count);
  else
    rv = GetRowCount(&count);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  nsRefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  for (PRInt32 idx = 0; idx < count; idx++) {
    PRInt32 rowIdx = doSelectRow ? aIndex : idx;
    PRInt32 colIdx = doSelectRow ? idx : aIndex;
    rv = tableLayout->GetCellDataAt(rowIdx, colIdx,
                                    *getter_AddRefs(cellElm),
                                    startRowIdx, startColIdx,
                                    rowSpan, colSpan,
                                    actualRowSpan, actualColSpan,
                                    isSelected);      

    if (NS_SUCCEEDED(rv) && !isSelected) {
      nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElm));
      rv = tableSelection->SelectCellElement(cellContent);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLTableAccessible::RemoveRowsOrColumnsFromSelection(PRInt32 aIndex,
                                                        PRUint32 aTarget,
                                                        PRBool aIsOuter)
{
  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  nsRefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  PRBool doUnselectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);
  PRInt32 count = 0;
  nsresult rv = doUnselectRow ? GetColumnCount(&count) : GetRowCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 startRowIdx = doUnselectRow ? aIndex : 0;
  PRInt32 endRowIdx = doUnselectRow ? aIndex : count - 1;
  PRInt32 startColIdx = doUnselectRow ? 0 : aIndex;
  PRInt32 endColIdx = doUnselectRow ? count - 1 : aIndex;

  if (aIsOuter)
    return tableSelection->RestrictCellsToSelection(mContent,
                                                    startRowIdx, startColIdx, 
                                                    endRowIdx, endColIdx);

  return tableSelection->RemoveCellsFromSelection(mContent,
                                                  startRowIdx, startColIdx, 
                                                  endRowIdx, endColIdx);
}

nsITableLayout*
nsHTMLTableAccessible::GetTableLayout()
{
  nsIFrame *frame = mContent->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  nsITableLayout *tableLayout = do_QueryFrame(frame);
  return tableLayout;
}

nsresult
nsHTMLTableAccessible::GetCellAt(PRInt32        aRowIndex,
                                 PRInt32        aColIndex,
                                 nsIDOMElement* &aCell)
{
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected;

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsresult rv = tableLayout->
    GetCellDataAt(aRowIndex, aColIndex, aCell, startRowIndex, startColIndex,
                  rowSpan, colSpan, actualRowSpan, actualColSpan, isSelected);

  if (rv == NS_TABLELAYOUT_CELL_NOT_FOUND)
    return NS_ERROR_INVALID_ARG;
  return rv;
}

void
nsHTMLTableAccessible::Description(nsString& aDescription)
{
  // Helpful for debugging layout vs. data tables
  aDescription.Truncate();
  nsAccessible::Description(aDescription);
  if (!aDescription.IsEmpty())
    return;

  // Use summary as description if it weren't used as a name.
  // XXX: get rid code duplication with NameInternal().
  nsAccessible* caption = Caption();
  if (caption) {
    nsIContent* captionContent = caption->GetContent();
    if (captionContent) {
      nsAutoString captionText;
      nsTextEquivUtils::AppendTextEquivFromContent(this, captionContent,
                                                   &captionText);

      if (!captionText.IsEmpty()) { // summary isn't used as a name.
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::summary,
                          aDescription);
      }
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
}

PRBool
nsHTMLTableAccessible::HasDescendant(const nsAString& aTagName,
                                     PRBool aAllowEmpty)
{
  nsCOMPtr<nsIDOMElement> tableElt(do_QueryInterface(mContent));
  NS_ENSURE_TRUE(tableElt, PR_FALSE);

  nsCOMPtr<nsIDOMNodeList> nodeList;
  tableElt->GetElementsByTagName(aTagName, getter_AddRefs(nodeList));
  NS_ENSURE_TRUE(nodeList, PR_FALSE);

  nsCOMPtr<nsIDOMNode> foundItem;
  nodeList->Item(0, getter_AddRefs(foundItem));
  if (!foundItem)
    return PR_FALSE;

  if (aAllowEmpty)
    return PR_TRUE;

  // Make sure that the item we found has contents and either has multiple
  // children or the found item is not a whitespace-only text node.
  nsCOMPtr<nsIContent> foundItemContent = do_QueryInterface(foundItem);
  if (foundItemContent->GetChildCount() > 1)
    return PR_TRUE; // Treat multiple child nodes as non-empty

  nsIContent *innerItemContent = foundItemContent->GetChildAt(0);
  if (innerItemContent && !innerItemContent->TextIsOnlyWhitespace())
    return PR_TRUE;

  // If we found more than one node then return true not depending on
  // aAllowEmpty flag.
  // XXX it might be dummy but bug 501375 where we changed this addresses
  // performance problems only. Note, currently 'aAllowEmpty' flag is used for
  // caption element only. On another hand we create accessible object for
  // the first entry of caption element (see
  // nsHTMLTableAccessible::CacheChildren).
  nodeList->Item(1, getter_AddRefs(foundItem));
  return !!foundItem;
}

NS_IMETHODIMP
nsHTMLTableAccessible::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
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

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsDocAccessible *docAccessible = GetDocAccessible();
  if (docAccessible) {
    PRUint64 docState = docAccessible->State();
    if (docState & states::EDITABLE) {  // Need to see all elements while document is being edited
      RETURN_LAYOUT_ANSWER(PR_FALSE, "In editable document");
    }
  }

  // Check to see if an ARIA role overrides the role from native markup,
  // but for which we still expose table semantics (treegrid, for example).
  PRBool hasNonTableRole = (Role() != nsIAccessibleRole::ROLE_TABLE);
  if (hasNonTableRole) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has role attribute");
  }

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::role)) {
    // Role attribute is present, but overridden roles have already been dealt with.
    // Only landmarks and other roles that don't override the role from native
    // markup are left to deal with here.
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has role attribute, weak role, and role is table");
  }
  
  // Check for legitimate data table elements or attributes
  nsAutoString summary;
  if ((mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, summary) &&
       !summary.IsEmpty()) || 
      HasDescendant(NS_LITERAL_STRING("caption"), PR_FALSE) ||
      HasDescendant(NS_LITERAL_STRING("th")) ||
      HasDescendant(NS_LITERAL_STRING("thead")) ||
      HasDescendant(NS_LITERAL_STRING("tfoot")) ||
      HasDescendant(NS_LITERAL_STRING("colgroup"))) {
    RETURN_LAYOUT_ANSWER(PR_FALSE, "Has caption, summary, th, thead, tfoot or colgroup -- legitimate table structures");
  }
  if (HasDescendant(NS_LITERAL_STRING("table"))) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has a nested table within it");
  }
  
  // If only 1 column or only 1 row, it's for layout
  PRInt32 columns, rows;
  GetColumnCount(&columns);
  if (columns <=1) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has only 1 column");
  }
  GetRowCount(&rows);
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
  nsIFrame *cellFrame = cellContent->GetPrimaryFrame();
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
  nsCOMPtr<nsIDOMElement> tableElt(do_QueryInterface(mContent));    
  tableElt->GetElementsByTagName(NS_LITERAL_STRING("tr"), getter_AddRefs(nodeList));
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);
  PRUint32 length;
  nodeList->GetLength(&length);
  nsAutoString color, lastRowColor;
  for (PRUint32 rowCount = 0; rowCount < length; rowCount ++) {
    nsCOMPtr<nsIDOMNode> rowNode;
    nodeList->Item(rowCount, getter_AddRefs(rowNode));
    nsCOMPtr<nsIContent> rowContent(do_QueryInterface(rowNode));

    nsCOMPtr<nsIDOMCSSStyleDeclaration> styleDecl =
      nsCoreUtils::GetComputedStyleDeclaration(EmptyString(), rowContent);
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

    nsDocAccessible *docAccessible = GetDocAccessible();
    NS_ENSURE_TRUE(docAccessible, NS_ERROR_FAILURE);
    nsIFrame *docFrame = docAccessible->GetFrame();
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

  if (HasDescendant(NS_LITERAL_STRING("embed")) ||
      HasDescendant(NS_LITERAL_STRING("object")) ||
      HasDescendant(NS_LITERAL_STRING("applet")) ||
      HasDescendant(NS_LITERAL_STRING("iframe"))) {
    RETURN_LAYOUT_ANSWER(PR_TRUE, "Has no borders, and has iframe, object, applet or iframe, typical of advertisements");
  }

  RETURN_LAYOUT_ANSWER(PR_FALSE, "no layout factor strong enough, so will guess data");
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLCaptionAccessible
////////////////////////////////////////////////////////////////////////////////

Relation
nsHTMLCaptionAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsHyperTextAccessible::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABEL_FOR)
    rel.AppendTarget(Parent());

  return rel;
}

PRUint32
nsHTMLCaptionAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CAPTION;
}
