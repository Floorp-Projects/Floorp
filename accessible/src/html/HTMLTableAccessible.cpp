/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLTableAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsIAccessibleRelation.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "TreeWalker.h"

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
#include "nsIMutableArray.h"
#include "nsIPresShell.h"
#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsFrameSelection.h"
#include "nsError.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLTableCellAccessible::
  HTMLTableCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc), xpcAccessibleTableCell(this)
{
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(HTMLTableCellAccessible,
                             HyperTextAccessible,
                             nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: Accessible implementation

void
HTMLTableCellAccessible::Shutdown()
{
  mTableCell = nullptr;
  HyperTextAccessibleWrap::Shutdown();
}

role
HTMLTableCellAccessible::NativeRole()
{
  return roles::CELL;
}

uint64_t
HTMLTableCellAccessible::NativeState()
{
  uint64_t state = HyperTextAccessibleWrap::NativeState();

  nsIFrame *frame = mContent->GetPrimaryFrame();
  NS_ASSERTION(frame, "No frame for valid cell accessible!");

  if (frame && frame->IsSelected())
    state |= states::SELECTED;

  return state;
}

uint64_t
HTMLTableCellAccessible::NativeInteractiveState() const
{
  return HyperTextAccessibleWrap::NativeInteractiveState() | states::SELECTABLE;
}

already_AddRefed<nsIPersistentProperties>
HTMLTableCellAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    HyperTextAccessibleWrap::NativeAttributes();

  // table-cell-index attribute
  TableAccessible* table = Table();
  if (!table)
    return attributes.forget();

  int32_t rowIdx = -1, colIdx = -1;
  nsresult rv = GetCellIndexes(rowIdx, colIdx);
  if (NS_FAILED(rv))
    return attributes.forget();

  nsAutoString stringIdx;
  stringIdx.AppendInt(table->CellIndexAt(rowIdx, colIdx));
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::tableCellIndex, stringIdx);

  // abbr attribute

  // Pick up object attribute from abbr DOM element (a child of the cell) or
  // from abbr DOM attribute.
  nsAutoString abbrText;
  if (ChildCount() == 1) {
    Accessible* abbr = FirstChild();
    if (abbr->IsAbbreviation()) {
      nsIContent* firstChildNode = abbr->GetContent()->GetFirstChild();
      if (firstChildNode) {
        nsTextEquivUtils::
          AppendTextEquivFromTextContent(firstChildNode, &abbrText);
      }
    }
  }
  if (abbrText.IsEmpty())
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::abbr, abbrText);

  if (!abbrText.IsEmpty())
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::abbr, abbrText);

  // axis attribute
  nsAutoString axisText;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::axis, axisText);
  if (!axisText.IsEmpty())
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::axis, axisText);

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: nsIAccessibleTableCell implementation

TableAccessible*
HTMLTableCellAccessible::Table() const
{
  Accessible* parent = const_cast<HTMLTableCellAccessible*>(this);
  while ((parent = parent->Parent())) {
    roles::Role role = parent->Role();
    if (role == roles::TABLE || role == roles::TREE_TABLE)
      return parent->AsTable();
  }

  return nullptr;
}

uint32_t
HTMLTableCellAccessible::ColIdx() const
{
  nsITableCellLayout* cellLayout = GetCellLayout();
  NS_ENSURE_TRUE(cellLayout, 0);

  int32_t colIdx = 0;
  cellLayout->GetColIndex(colIdx);
  return colIdx > 0 ? static_cast<uint32_t>(colIdx) : 0;
}

uint32_t
HTMLTableCellAccessible::RowIdx() const
{
  nsITableCellLayout* cellLayout = GetCellLayout();
  NS_ENSURE_TRUE(cellLayout, 0);

  int32_t rowIdx = 0;
  cellLayout->GetRowIndex(rowIdx);
  return rowIdx > 0 ? static_cast<uint32_t>(rowIdx) : 0;
}

uint32_t
HTMLTableCellAccessible::ColExtent() const
{
  int32_t rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  TableAccessible* table = Table();
  NS_ASSERTION(table, "cell not in a table!");
  if (!table)
    return 0;

  return table->ColExtentAt(rowIdx, colIdx);
}

uint32_t
HTMLTableCellAccessible::RowExtent() const
{
  int32_t rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  TableAccessible* table = Table();
  NS_ASSERTION(table, "cell not in atable!");
  if (!table)
    return 0;

  return table->RowExtentAt(rowIdx, colIdx);
}

void
HTMLTableCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aCells)
{
  IDRefsIterator itr(mDoc, mContent, nsGkAtoms::headers);
  while (Accessible* cell = itr.Next())
    if (cell->Role() == roles::COLUMNHEADER)
      aCells->AppendElement(cell);

  if (aCells->IsEmpty())
    TableCellAccessible::ColHeaderCells(aCells);
}

void
HTMLTableCellAccessible::RowHeaderCells(nsTArray<Accessible*>* aCells)
{
  IDRefsIterator itr(mDoc, mContent, nsGkAtoms::headers);
  while (Accessible* cell = itr.Next())
    if (cell->Role() == roles::ROWHEADER)
      aCells->AppendElement(cell);

  if (aCells->IsEmpty())
    TableCellAccessible::RowHeaderCells(aCells);
}

bool
HTMLTableCellAccessible::Selected()
{
  int32_t rowIdx = -1, colIdx = -1;
  GetCellIndexes(rowIdx, colIdx);

  TableAccessible* table = Table();
  NS_ENSURE_TRUE(table, false);

  return table->IsCellSelected(rowIdx, colIdx);
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: protected implementation

nsITableCellLayout*
HTMLTableCellAccessible::GetCellLayout() const
{
  return do_QueryFrame(mContent->GetPrimaryFrame());
}

nsresult
HTMLTableCellAccessible::GetCellIndexes(int32_t& aRowIdx, int32_t& aColIdx) const
{
  nsITableCellLayout *cellLayout = GetCellLayout();
  NS_ENSURE_STATE(cellLayout);

  return cellLayout->GetCellIndexes(aRowIdx, aColIdx);
}


////////////////////////////////////////////////////////////////////////////////
// HTMLTableHeaderCellAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLTableHeaderCellAccessible::
  HTMLTableHeaderCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HTMLTableCellAccessible(aContent, aDoc)
{
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableHeaderCellAccessible: Accessible implementation

role
HTMLTableHeaderCellAccessible::NativeRole()
{
  // Check value of @scope attribute.
  static nsIContent::AttrValuesArray scopeValues[] =
    {&nsGkAtoms::col, &nsGkAtoms::row, nullptr};
  int32_t valueIdx =
    mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::scope,
                              scopeValues, eCaseMatters);

  switch (valueIdx) {
    case 0:
      return roles::COLUMNHEADER;
    case 1:
      return roles::ROWHEADER;
  }

  // Assume it's columnheader if there are headers in siblings, oterwise
  // rowheader.
  nsIContent* parentContent = mContent->GetParent();
  if (!parentContent) {
    NS_ERROR("Deattached content on alive accessible?");
    return roles::NOTHING;
  }

  for (nsIContent* siblingContent = mContent->GetPreviousSibling(); siblingContent;
       siblingContent = siblingContent->GetPreviousSibling()) {
    if (siblingContent->IsElement()) {
      return nsCoreUtils::IsHTMLTableHeader(siblingContent) ?
	     roles::COLUMNHEADER : roles::ROWHEADER;
    }
  }

  for (nsIContent* siblingContent = mContent->GetNextSibling(); siblingContent;
       siblingContent = siblingContent->GetNextSibling()) {
    if (siblingContent->IsElement()) {
      return nsCoreUtils::IsHTMLTableHeader(siblingContent) ?
	     roles::COLUMNHEADER : roles::ROWHEADER;
    }
  }

  // No elements in siblings what means the table has one column only. Therefore
  // it should be column header.
  return roles::COLUMNHEADER;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLTableRowAccessible
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableRowAccessible, Accessible)

role
HTMLTableRowAccessible::NativeRole()
{
  return roles::ROW;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLTableAccessible::
  HTMLTableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc), xpcAccessibleTable(this)
{
  mFlags |= eTableAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: nsISupports implementation

NS_IMPL_ISUPPORTS_INHERITED1(HTMLTableAccessible, Accessible,
                             nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
//nsAccessNode

void
HTMLTableAccessible::Shutdown()
{
  mTable = nullptr;
  AccessibleWrap::Shutdown();
}


////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: Accessible implementation

void
HTMLTableAccessible::CacheChildren()
{
  // Move caption accessible so that it's the first child. Check for the first
  // caption only, because nsAccessibilityService ensures we don't create
  // accessibles for the other captions, since only the first is actually
  // visible.
  TreeWalker walker(this, mContent);

  Accessible* child = nullptr;
  while ((child = walker.NextChild())) {
    if (child->Role() == roles::CAPTION) {
      InsertChildAt(0, child);
      while ((child = walker.NextChild()) && AppendChild(child));
      break;
    }
    AppendChild(child);
  }
}

role
HTMLTableAccessible::NativeRole()
{
  return roles::TABLE;
}

uint64_t
HTMLTableAccessible::NativeState()
{
  return Accessible::NativeState() | states::READONLY;
}

ENameValueFlag
HTMLTableAccessible::NativeName(nsString& aName)
{
  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  // Use table caption as a name.
  Accessible* caption = Caption();
  if (caption) {
    nsIContent* captionContent = caption->GetContent();
    if (captionContent) {
      nsTextEquivUtils::AppendTextEquivFromContent(this, captionContent, &aName);
      if (!aName.IsEmpty())
        return eNameOK;
    }
  }

  // If no caption then use summary as a name.
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, aName);
  return eNameOK;
}

already_AddRefed<nsIPersistentProperties>
HTMLTableAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    AccessibleWrap::NativeAttributes();
  if (IsProbablyLayoutTable()) {
    nsAutoString unused;
    attributes->SetStringProperty(NS_LITERAL_CSTRING("layout-guess"),
                                  NS_LITERAL_STRING("true"), unused);
  }

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: nsIAccessible implementation

Relation
HTMLTableAccessible::RelationByType(uint32_t aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABELLED_BY)
    rel.AppendTarget(Caption());

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: nsIAccessibleTable implementation

Accessible*
HTMLTableAccessible::Caption()
{
  Accessible* child = mChildren.SafeElementAt(0, nullptr);
  return child && child->Role() == roles::CAPTION ? child : nullptr;
}

void
HTMLTableAccessible::Summary(nsString& aSummary)
{
  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mContent));

  if (table)
    table->GetSummary(aSummary);
}

uint32_t
HTMLTableAccessible::ColCount()
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout)
    return 0;

  int32_t rowCount = 0, colCount = 0;
  tableLayout->GetTableSize(rowCount, colCount);
  return colCount;
}

uint32_t
HTMLTableAccessible::RowCount()
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout)
    return 0;

  int32_t rowCount = 0, colCount = 0;
  tableLayout->GetTableSize(rowCount, colCount);
  return rowCount;
}

uint32_t
HTMLTableAccessible::SelectedCellCount()
{
  nsITableLayout *tableLayout = GetTableLayout();
  if (!tableLayout)
    return 0;

  uint32_t count = 0, rowCount = RowCount(), colCount = ColCount();

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool isSelected = false;

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsresult rv = tableLayout->GetCellDataAt(rowIdx, colIdx,
                                               *getter_AddRefs(domElement),
                                               startRowIndex, startColIndex,
                                               rowSpan, colSpan,
                                               actualRowSpan, actualColSpan,
                                               isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIdx &&
          startColIndex == colIdx && isSelected)
        count++;
    }
  }

  return count;
}

uint32_t
HTMLTableAccessible::SelectedColCount()
{
  uint32_t count = 0, colCount = ColCount();

  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++)
    if (IsColSelected(colIdx))
      count++;

  return count;
}

uint32_t
HTMLTableAccessible::SelectedRowCount()
{
  uint32_t count = 0, rowCount = RowCount();

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      count++;

  return count;
}

void
HTMLTableAccessible::SelectedCells(nsTArray<Accessible*>* aCells)
{
  uint32_t rowCount = RowCount(), colCount = ColCount();

  nsITableLayout *tableLayout = GetTableLayout();
  if (!tableLayout) 
    return;

  nsCOMPtr<nsIDOMElement> cellElement;
  int32_t startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool isSelected = false;

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsresult rv = tableLayout->GetCellDataAt(rowIdx, colIdx,
                                      *getter_AddRefs(cellElement),
                                      startRowIndex, startColIndex,
                                      rowSpan, colSpan,
                                      actualRowSpan, actualColSpan,
                                      isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIdx &&
          startColIndex == colIdx && isSelected) {
        nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
        Accessible* cell = mDoc->GetAccessible(cellContent);
        aCells->AppendElement(cell);
      }
    }
  }
}

void
HTMLTableAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells)
{
  nsITableLayout *tableLayout = GetTableLayout();
  if (!tableLayout)
    return;

  uint32_t rowCount = RowCount(), colCount = ColCount();

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t startRowIndex = 0, startColIndex = 0,
    rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool isSelected = false;

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsresult rv = tableLayout->GetCellDataAt(rowIdx, colIdx,
                                               *getter_AddRefs(domElement),
                                               startRowIndex, startColIndex,
                                               rowSpan, colSpan,
                                               actualRowSpan, actualColSpan,
                                               isSelected);

      if (NS_SUCCEEDED(rv) && startRowIndex == rowIdx &&
          startColIndex == colIdx && isSelected)
        aCells->AppendElement(CellIndexAt(rowIdx, colIdx));
    }
  }
}

void
HTMLTableAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols)
{
  uint32_t colCount = ColCount();
  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++)
    if (IsColSelected(colIdx))
      aCols->AppendElement(colIdx);
}

void
HTMLTableAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows)
{
  uint32_t rowCount = RowCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++)
    if (IsRowSelected(rowIdx))
      aRows->AppendElement(rowIdx);
}

Accessible*
HTMLTableAccessible::CellAt(uint32_t aRowIndex, uint32_t aColumnIndex)
{ 
  nsCOMPtr<nsIDOMElement> cellElement;
  GetCellAt(aRowIndex, aColumnIndex, *getter_AddRefs(cellElement));
  if (!cellElement)
    return nullptr;

  nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
  if (!cellContent)
    return nullptr;

  Accessible* cell = mDoc->GetAccessible(cellContent);

  // XXX bug 576838: crazy tables (like table6 in tables/test_table2.html) may
  // return itself as a cell what makes Orca hang.
  return cell == this ? nullptr : cell;
}

int32_t
HTMLTableAccessible::CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();

  int32_t index = -1;
  tableLayout->GetIndexByRowAndColumn(aRowIdx, aColIdx, &index);
  return index;
}

int32_t
HTMLTableAccessible::ColIndexAt(uint32_t aCellIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout) 
    return -1;

  int32_t rowIdx = -1, colIdx = -1;
  tableLayout->GetRowAndColumnByIndex(aCellIdx, &rowIdx, &colIdx);
  return colIdx;
}

int32_t
HTMLTableAccessible::RowIndexAt(uint32_t aCellIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout) 
    return -1;

  int32_t rowIdx = -1, colIdx = -1;
  tableLayout->GetRowAndColumnByIndex(aCellIdx, &rowIdx, &colIdx);
  return rowIdx;
}

void
HTMLTableAccessible::RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                        int32_t* aColIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();

  if (tableLayout)
    tableLayout->GetRowAndColumnByIndex(aCellIdx, aRowIdx, aColIdx);
}

uint32_t
HTMLTableAccessible::ColExtentAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout)
    return 0;

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t startRowIndex, startColIndex, rowSpan, colSpan, actualRowSpan;
  bool isSelected;
  int32_t columnExtent = 0;

  DebugOnly<nsresult> rv = tableLayout->
    GetCellDataAt(aRowIdx, aColIdx, *getter_AddRefs(domElement),
                  startRowIndex, startColIndex, rowSpan, colSpan,
                  actualRowSpan, columnExtent, isSelected);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get cell data");

  return columnExtent;
}

uint32_t
HTMLTableAccessible::RowExtentAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsITableLayout* tableLayout = GetTableLayout();
  if (!tableLayout)
    return 0;

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t startRowIndex, startColIndex, rowSpan, colSpan, actualColSpan;
  bool isSelected;
  int32_t rowExtent = 0;

  DebugOnly<nsresult> rv = tableLayout->
    GetCellDataAt(aRowIdx, aColIdx, *getter_AddRefs(domElement),
                  startRowIndex, startColIndex, rowSpan, colSpan,
                  rowExtent, actualColSpan, isSelected);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get cell data");

  return rowExtent;
}

bool
HTMLTableAccessible::IsColSelected(uint32_t aColIdx)
{
  bool isSelected = false;

  uint32_t rowCount = RowCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    isSelected = IsCellSelected(rowIdx, aColIdx);
    if (!isSelected)
      return false;
  }

  return isSelected;
}

bool
HTMLTableAccessible::IsRowSelected(uint32_t aRowIdx)
{
  bool isSelected = false;

  uint32_t colCount = ColCount();
  for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
    isSelected = IsCellSelected(aRowIdx, colIdx);
    if (!isSelected)
      return false;
  }

  return isSelected;
}

bool
HTMLTableAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsITableLayout *tableLayout = GetTableLayout();
  if (!tableLayout)
    return false;

  nsCOMPtr<nsIDOMElement> domElement;
  int32_t startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool isSelected = false;

  tableLayout->GetCellDataAt(aRowIdx, aColIdx, *getter_AddRefs(domElement),
                             startRowIndex, startColIndex, rowSpan, colSpan,
                             actualRowSpan, actualColSpan, isSelected);

  return isSelected;
}

void
HTMLTableAccessible::SelectRow(uint32_t aRowIdx)
{
  nsresult rv =
    RemoveRowsOrColumnsFromSelection(aRowIdx,
                                     nsISelectionPrivate::TABLESELECTION_ROW,
                                     true);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "RemoveRowsOrColumnsFromSelection() Shouldn't fail!");

  AddRowOrColumnToSelection(aRowIdx, nsISelectionPrivate::TABLESELECTION_ROW);
}

void
HTMLTableAccessible::SelectCol(uint32_t aColIdx)
{
  nsresult rv =
    RemoveRowsOrColumnsFromSelection(aColIdx,
                                     nsISelectionPrivate::TABLESELECTION_COLUMN,
                                     true);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "RemoveRowsOrColumnsFromSelection() Shouldn't fail!");

  AddRowOrColumnToSelection(aColIdx, nsISelectionPrivate::TABLESELECTION_COLUMN);
}

void
HTMLTableAccessible::UnselectRow(uint32_t aRowIdx)
{
  RemoveRowsOrColumnsFromSelection(aRowIdx,
                                   nsISelectionPrivate::TABLESELECTION_ROW,
                                   false);
}

void
HTMLTableAccessible::UnselectCol(uint32_t aColIdx)
{
  RemoveRowsOrColumnsFromSelection(aColIdx,
                                   nsISelectionPrivate::TABLESELECTION_COLUMN,
                                   false);
}

nsresult
HTMLTableAccessible::AddRowOrColumnToSelection(int32_t aIndex, uint32_t aTarget)
{
  bool doSelectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);

  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsCOMPtr<nsIDOMElement> cellElm;
  int32_t startRowIdx, startColIdx, rowSpan, colSpan,
    actualRowSpan, actualColSpan;
  bool isSelected = false;

  nsresult rv = NS_OK;
  int32_t count = 0;
  if (doSelectRow)
    rv = GetColumnCount(&count);
  else
    rv = GetRowCount(&count);

  NS_ENSURE_SUCCESS(rv, rv);

  nsIPresShell* presShell(mDoc->PresShell());
  nsRefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  for (int32_t idx = 0; idx < count; idx++) {
    int32_t rowIdx = doSelectRow ? aIndex : idx;
    int32_t colIdx = doSelectRow ? idx : aIndex;
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
HTMLTableAccessible::RemoveRowsOrColumnsFromSelection(int32_t aIndex,
                                                      uint32_t aTarget,
                                                      bool aIsOuter)
{
  nsITableLayout *tableLayout = GetTableLayout();
  NS_ENSURE_STATE(tableLayout);

  nsIPresShell* presShell(mDoc->PresShell());
  nsRefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  bool doUnselectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);
  int32_t count = 0;
  nsresult rv = doUnselectRow ? GetColumnCount(&count) : GetRowCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t startRowIdx = doUnselectRow ? aIndex : 0;
  int32_t endRowIdx = doUnselectRow ? aIndex : count - 1;
  int32_t startColIdx = doUnselectRow ? 0 : aIndex;
  int32_t endColIdx = doUnselectRow ? count - 1 : aIndex;

  if (aIsOuter)
    return tableSelection->RestrictCellsToSelection(mContent,
                                                    startRowIdx, startColIdx,
                                                    endRowIdx, endColIdx);

  return tableSelection->RemoveCellsFromSelection(mContent,
                                                  startRowIdx, startColIdx,
                                                  endRowIdx, endColIdx);
}

nsITableLayout*
HTMLTableAccessible::GetTableLayout()
{
  nsIFrame *frame = mContent->GetPrimaryFrame();
  if (!frame)
    return nullptr;

  nsITableLayout *tableLayout = do_QueryFrame(frame);
  return tableLayout;
}

nsresult
HTMLTableAccessible::GetCellAt(int32_t aRowIndex, int32_t aColIndex,
                               nsIDOMElement*& aCell)
{
  int32_t startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;
  bool isSelected;

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
HTMLTableAccessible::Description(nsString& aDescription)
{
  // Helpful for debugging layout vs. data tables
  aDescription.Truncate();
  Accessible::Description(aDescription);
  if (!aDescription.IsEmpty())
    return;

  // Use summary as description if it weren't used as a name.
  // XXX: get rid code duplication with NameInternal().
  Accessible* caption = Caption();
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
    bool isProbablyForLayout = IsProbablyLayoutTable();
    aDescription = mLayoutHeuristic;
  }
  printf("\nTABLE: %s\n", NS_ConvertUTF16toUTF8(mLayoutHeuristic).get());
#endif
}

bool
HTMLTableAccessible::HasDescendant(const nsAString& aTagName, bool aAllowEmpty)
{
  nsCOMPtr<nsIDOMElement> tableElt(do_QueryInterface(mContent));
  NS_ENSURE_TRUE(tableElt, false);

  nsCOMPtr<nsIDOMHTMLCollection> nodeList;
  tableElt->GetElementsByTagName(aTagName, getter_AddRefs(nodeList));
  NS_ENSURE_TRUE(nodeList, false);

  nsCOMPtr<nsIDOMNode> foundItem;
  nodeList->Item(0, getter_AddRefs(foundItem));
  if (!foundItem)
    return false;

  if (aAllowEmpty)
    return true;

  // Make sure that the item we found has contents and either has multiple
  // children or the found item is not a whitespace-only text node.
  nsCOMPtr<nsIContent> foundItemContent = do_QueryInterface(foundItem);
  if (foundItemContent->GetChildCount() > 1)
    return true; // Treat multiple child nodes as non-empty

  nsIContent *innerItemContent = foundItemContent->GetFirstChild();
  if (innerItemContent && !innerItemContent->TextIsOnlyWhitespace())
    return true;

  // If we found more than one node then return true not depending on
  // aAllowEmpty flag.
  // XXX it might be dummy but bug 501375 where we changed this addresses
  // performance problems only. Note, currently 'aAllowEmpty' flag is used for
  // caption element only. On another hand we create accessible object for
  // the first entry of caption element (see
  // HTMLTableAccessible::CacheChildren).
  nodeList->Item(1, getter_AddRefs(foundItem));
  return !!foundItem;
}

bool
HTMLTableAccessible::IsProbablyLayoutTable()
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
  { \
    mLayoutHeuristic = isLayout ? \
      NS_LITERAL_STRING("layout table: " heuristic) : \
      NS_LITERAL_STRING("data table: " heuristic); \
    return isLayout; \
  }
#else
#define RETURN_LAYOUT_ANSWER(isLayout, heuristic) { return isLayout; }
#endif

  DocAccessible* docAccessible = Document();
  if (docAccessible) {
    uint64_t docState = docAccessible->State();
    if (docState & states::EDITABLE) {  // Need to see all elements while document is being edited
      RETURN_LAYOUT_ANSWER(false, "In editable document");
    }
  }

  // Check to see if an ARIA role overrides the role from native markup,
  // but for which we still expose table semantics (treegrid, for example).
  if (Role() != roles::TABLE)
    RETURN_LAYOUT_ANSWER(false, "Has role attribute");

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::role)) {
    // Role attribute is present, but overridden roles have already been dealt with.
    // Only landmarks and other roles that don't override the role from native
    // markup are left to deal with here.
    RETURN_LAYOUT_ANSWER(false, "Has role attribute, weak role, and role is table");
  }

  if (mContent->Tag() != nsGkAtoms::table)
    RETURN_LAYOUT_ANSWER(true, "table built by CSS display:table style");

  // Check if datatable attribute has "0" value.
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::datatable,
                            NS_LITERAL_STRING("0"), eCaseMatters)) {
    RETURN_LAYOUT_ANSWER(true, "Has datatable = 0 attribute, it's for layout");
  }

  // Check for legitimate data table attributes.
  nsAutoString summary;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, summary) &&
      !summary.IsEmpty())
    RETURN_LAYOUT_ANSWER(false, "Has summary -- legitimate table structures");

  // Check for legitimate data table elements.
  Accessible* caption = FirstChild();
  if (caption && caption->Role() == roles::CAPTION && caption->HasChildren()) 
    RETURN_LAYOUT_ANSWER(false, "Not empty caption -- legitimate table structures");

  for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
       childElm = childElm->GetNextSibling()) {
    if (!childElm->IsHTML())
      continue;

    if (childElm->Tag() == nsGkAtoms::col ||
        childElm->Tag() == nsGkAtoms::colgroup ||
        childElm->Tag() == nsGkAtoms::tfoot ||
        childElm->Tag() == nsGkAtoms::thead) {
      RETURN_LAYOUT_ANSWER(false,
                           "Has col, colgroup, tfoot or thead -- legitimate table structures");
    }

    if (childElm->Tag() == nsGkAtoms::tbody) {
      for (nsIContent* rowElm = childElm->GetFirstChild(); rowElm;
           rowElm = rowElm->GetNextSibling()) {
        if (rowElm->IsHTML() && rowElm->Tag() == nsGkAtoms::tr) {
          for (nsIContent* cellElm = rowElm->GetFirstChild(); cellElm;
               cellElm = cellElm->GetNextSibling()) {
            if (cellElm->IsHTML()) {

              if (cellElm->NodeInfo()->Equals(nsGkAtoms::th)) {
                RETURN_LAYOUT_ANSWER(false,
                                     "Has th -- legitimate table structures");
              }

              if (cellElm->HasAttr(kNameSpaceID_None, nsGkAtoms::headers) ||
                  cellElm->HasAttr(kNameSpaceID_None, nsGkAtoms::scope) ||
                  cellElm->HasAttr(kNameSpaceID_None, nsGkAtoms::abbr)) {
                RETURN_LAYOUT_ANSWER(false,
                                     "Has headers, scope, or abbr attribute -- legitimate table structures");
              }

              Accessible* cell = mDoc->GetAccessible(cellElm);
              if (cell && cell->ChildCount() == 1 &&
                  cell->FirstChild()->IsAbbreviation()) {
                RETURN_LAYOUT_ANSWER(false,
                                     "has abbr -- legitimate table structures");
              }
            }
          }
        }
      }
    }
  }

  if (HasDescendant(NS_LITERAL_STRING("table"))) {
    RETURN_LAYOUT_ANSWER(true, "Has a nested table within it");
  }

  // If only 1 column or only 1 row, it's for layout
  int32_t columns, rows;
  GetColumnCount(&columns);
  if (columns <=1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 column");
  }
  GetRowCount(&rows);
  if (rows <=1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 row");
  }

  // Check for many columns
  if (columns >= 5) {
    RETURN_LAYOUT_ANSWER(false, ">=5 columns");
  }

  // Now we know there are 2-4 columns and 2 or more rows
  // Check to see if there are visible borders on the cells
  // XXX currently, we just check the first cell -- do we really need to do more?
  nsCOMPtr<nsIDOMElement> cellElement;
  nsresult rv = GetCellAt(0, 0, *getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIContent> cellContent(do_QueryInterface(cellElement));
  NS_ENSURE_TRUE(cellContent, false);
  nsIFrame *cellFrame = cellContent->GetPrimaryFrame();
  if (!cellFrame) {
    RETURN_LAYOUT_ANSWER(false, "Could not get frame for cellContent");
  }
  nsMargin border;
  cellFrame->GetBorder(border);
  if (border.top && border.bottom && border.left && border.right) {
    RETURN_LAYOUT_ANSWER(false, "Has nonzero border-width on table cell");
  }

  /**
   * Rules for non-bordered tables with 2-4 columns and 2+ rows from here on forward
   */

  // Check for styled background color across rows (alternating background
  // color is a common feature for data tables).
  uint32_t childCount = ChildCount();
  nscolor rowColor = 0;
  nscolor prevRowColor;
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = GetChildAt(childIdx);
    if (child->Role() == roles::ROW) {
      prevRowColor = rowColor;
      nsIFrame* rowFrame = child->GetFrame();
      rowColor = rowFrame->GetStyleBackground()->mBackgroundColor;

      if (childIdx > 0 && prevRowColor != rowColor)
        RETURN_LAYOUT_ANSWER(false, "2 styles of row background color, non-bordered");
    }
  }

  // Check for many rows
  const int32_t kMaxLayoutRows = 20;
  if (rows > kMaxLayoutRows) { // A ton of rows, this is probably for data
    RETURN_LAYOUT_ANSWER(false, ">= kMaxLayoutRows (20) and non-bordered");
  }

  // Check for very wide table.
  nsIFrame* documentFrame = Document()->GetFrame();
  nsSize documentSize = documentFrame->GetSize();
  if (documentSize.width > 0) {
    nsSize tableSize = GetFrame()->GetSize();
    int32_t percentageOfDocWidth = (100 * tableSize.width) / documentSize.width;
    if (percentageOfDocWidth > 95) {
      // 3-4 columns, no borders, not a lot of rows, and 95% of the doc's width
      // Probably for layout
      RETURN_LAYOUT_ANSWER(true,
                           "<= 4 columns, table width is 95% of document width");
    }
  }

  // Two column rules
  if (rows * columns <= 10) {
    RETURN_LAYOUT_ANSWER(true, "2-4 columns, 10 cells or less, non-bordered");
  }

  if (HasDescendant(NS_LITERAL_STRING("embed")) ||
      HasDescendant(NS_LITERAL_STRING("object")) ||
      HasDescendant(NS_LITERAL_STRING("applet")) ||
      HasDescendant(NS_LITERAL_STRING("iframe"))) {
    RETURN_LAYOUT_ANSWER(true, "Has no borders, and has iframe, object, applet or iframe, typical of advertisements");
  }

  RETURN_LAYOUT_ANSWER(false, "no layout factor strong enough, so will guess data");
}


////////////////////////////////////////////////////////////////////////////////
// HTMLCaptionAccessible
////////////////////////////////////////////////////////////////////////////////

Relation
HTMLCaptionAccessible::RelationByType(uint32_t aType)
{
  Relation rel = HyperTextAccessible::RelationByType(aType);
  if (aType == nsIAccessibleRelation::RELATION_LABEL_FOR)
    rel.AppendTarget(Parent());

  return rel;
}

role
HTMLCaptionAccessible::NativeRole()
{
  return roles::CAPTION;
}
