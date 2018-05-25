/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLTableAccessible.h"

#include "mozilla/DebugOnly.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "TreeWalker.h"

#include "mozilla/dom/HTMLTableElement.h"
#include "nsIHTMLCollection.h"
#include "nsIDocument.h"
#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"
#include "nsIPresShell.h"
#include "nsITableCellLayout.h"
#include "nsFrameSelection.h"
#include "nsError.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsNameSpaceManager.h"
#include "nsTableCellFrame.h"
#include "nsTableWrapperFrame.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLTableCellAccessible::
  HTMLTableCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
  mType = eHTMLTableCellType;
  mGenericTypes |= eTableCell;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: Accessible implementation

role
HTMLTableCellAccessible::NativeRole() const
{
  if (mContent->IsMathMLElement(nsGkAtoms::mtd_)) {
    return roles::MATHML_CELL;
  }
  return roles::CELL;
}

uint64_t
HTMLTableCellAccessible::NativeState() const
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
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::abbr, abbrText);

  if (!abbrText.IsEmpty())
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::abbr, abbrText);

  // axis attribute
  nsAutoString axisText;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::axis, axisText);
  if (!axisText.IsEmpty())
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::axis, axisText);

#ifdef DEBUG
  nsAutoString unused;
  attributes->SetStringProperty(NS_LITERAL_CSTRING("cppclass"),
                                NS_LITERAL_STRING("HTMLTableCellAccessible"),
                                unused);
#endif

  return attributes.forget();
}

GroupPos
HTMLTableCellAccessible::GroupPosition()
{
  int32_t count = 0, index = 0;
  TableAccessible* table = Table();
  if (table && nsCoreUtils::GetUIntAttr(table->AsAccessible()->GetContent(),
                                        nsGkAtoms::aria_colcount, &count) &&
      nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_colindex, &index)) {
    return GroupPos(0, index, count);
  }

  return HyperTextAccessibleWrap::GroupPosition();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: TableCellAccessible implementation

TableAccessible*
HTMLTableCellAccessible::Table() const
{
  Accessible* parent = const_cast<HTMLTableCellAccessible*>(this);
  while ((parent = parent->Parent())) {
    if (parent->IsTable())
      return parent->AsTable();
  }

  return nullptr;
}

uint32_t
HTMLTableCellAccessible::ColIdx() const
{
  nsTableCellFrame* cellFrame = GetCellFrame();
  NS_ENSURE_TRUE(cellFrame, 0);
  return cellFrame->ColIndex();
}

uint32_t
HTMLTableCellAccessible::RowIdx() const
{
  nsTableCellFrame* cellFrame = GetCellFrame();
  NS_ENSURE_TRUE(cellFrame, 0);
  return cellFrame->RowIndex();
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
  while (Accessible* cell = itr.Next()) {
    a11y::role cellRole = cell->Role();
    if (cellRole == roles::COLUMNHEADER) {
      aCells->AppendElement(cell);
    } else if (cellRole != roles::ROWHEADER) {
      // If referred table cell is at the same column then treat it as a column
      // header.
      TableCellAccessible* tableCell = cell->AsTableCell();
      if (tableCell && tableCell->ColIdx() == ColIdx())
        aCells->AppendElement(cell);
    }
  }

  if (aCells->IsEmpty())
    TableCellAccessible::ColHeaderCells(aCells);
}

void
HTMLTableCellAccessible::RowHeaderCells(nsTArray<Accessible*>* aCells)
{
  IDRefsIterator itr(mDoc, mContent, nsGkAtoms::headers);
  while (Accessible* cell = itr.Next()) {
    a11y::role cellRole = cell->Role();
    if (cellRole == roles::ROWHEADER) {
      aCells->AppendElement(cell);
    } else if (cellRole != roles::COLUMNHEADER) {
      // If referred table cell is at the same row then treat it as a column
      // header.
      TableCellAccessible* tableCell = cell->AsTableCell();
      if (tableCell && tableCell->RowIdx() == RowIdx())
        aCells->AppendElement(cell);
    }
  }

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

nsTableCellFrame*
HTMLTableCellAccessible::GetCellFrame() const
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
HTMLTableHeaderCellAccessible::NativeRole() const
{
  // Check value of @scope attribute.
  static Element::AttrValuesArray scopeValues[] =
    { &nsGkAtoms::col, &nsGkAtoms::colgroup,
      &nsGkAtoms::row, &nsGkAtoms::rowgroup, nullptr };
  int32_t valueIdx =
    mContent->AsElement()->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::scope,
                                           scopeValues, eCaseMatters);

  switch (valueIdx) {
    case 0:
    case 1:
      return roles::COLUMNHEADER;
    case 2:
    case 3:
      return roles::ROWHEADER;
  }

  TableAccessible* table = Table();
  if (!table)
    return roles::NOTHING;

  // If the cell next to this one is not a header cell then assume this cell is
  // a row header for it.
  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  Accessible* cell = table->CellAt(rowIdx, colIdx + ColExtent());
  if (cell && !nsCoreUtils::IsHTMLTableHeader(cell->GetContent()))
    return roles::ROWHEADER;

  // If the cell below this one is not a header cell then assume this cell is
  // a column header for it.
  uint32_t rowExtent = RowExtent();
  cell = table->CellAt(rowIdx + rowExtent, colIdx);
  if (cell && !nsCoreUtils::IsHTMLTableHeader(cell->GetContent()))
    return roles::COLUMNHEADER;

  // Otherwise if this cell is surrounded by header cells only then make a guess
  // based on its cell spanning. In other words if it is row spanned then assume
  // it's a row header, otherwise it's a column header.
  return rowExtent > 1 ? roles::ROWHEADER : roles::COLUMNHEADER;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLTableRowAccessible
////////////////////////////////////////////////////////////////////////////////

role
HTMLTableRowAccessible::NativeRole() const
{
  if (mContent->IsMathMLElement(nsGkAtoms::mtr_)) {
    return roles::MATHML_TABLE_ROW;
  } else if (mContent->IsMathMLElement(nsGkAtoms::mlabeledtr_)) {
    return roles::MATHML_LABELED_ROW;
  }
  return roles::ROW;
}

GroupPos
HTMLTableRowAccessible::GroupPosition()
{
  int32_t count = 0, index = 0;
  Accessible* table = nsAccUtils::TableFor(this);
  if (table && nsCoreUtils::GetUIntAttr(table->GetContent(),
                                        nsGkAtoms::aria_rowcount, &count) &&
      nsCoreUtils::GetUIntAttr(mContent, nsGkAtoms::aria_rowindex, &index)) {
    return GroupPos(0, index, count);
  }

  return AccessibleWrap::GroupPosition();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: Accessible

bool
HTMLTableAccessible::InsertChildAt(uint32_t aIndex, Accessible* aChild)
{
  // Move caption accessible so that it's the first child. Check for the first
  // caption only, because nsAccessibilityService ensures we don't create
  // accessibles for the other captions, since only the first is actually
  // visible.
  return Accessible::InsertChildAt(aChild->IsHTMLCaption() ? 0 : aIndex, aChild);
}

role
HTMLTableAccessible::NativeRole() const
{
  if (mContent->IsMathMLElement(nsGkAtoms::mtable_)) {
    return roles::MATHML_TABLE;
  }
  return roles::TABLE;
}

uint64_t
HTMLTableAccessible::NativeState() const
{
  return Accessible::NativeState() | states::READONLY;
}

ENameValueFlag
HTMLTableAccessible::NativeName(nsString& aName) const
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
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, aName);
  return eNameOK;
}

already_AddRefed<nsIPersistentProperties>
HTMLTableAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    AccessibleWrap::NativeAttributes();

  if (mContent->IsMathMLElement(nsGkAtoms::mtable_)) {
    GetAccService()->MarkupAttributes(mContent, attributes);
  }

  if (IsProbablyLayoutTable()) {
    nsAutoString unused;
    attributes->SetStringProperty(NS_LITERAL_CSTRING("layout-guess"),
                                  NS_LITERAL_STRING("true"), unused);
  }

  return attributes.forget();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: Accessible

Relation
HTMLTableAccessible::RelationByType(RelationType aType) const
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType == RelationType::LABELLED_BY)
    rel.AppendTarget(Caption());

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessible: Table

Accessible*
HTMLTableAccessible::Caption() const
{
  Accessible* child = mChildren.SafeElementAt(0, nullptr);
  return child && child->Role() == roles::CAPTION ? child : nullptr;
}

void
HTMLTableAccessible::Summary(nsString& aSummary)
{
  dom::HTMLTableElement* table = dom::HTMLTableElement::FromNode(mContent);

  if (table)
    table->GetSummary(aSummary);
}

uint32_t
HTMLTableAccessible::ColCount() const
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  return tableFrame ? tableFrame->GetColCount() : 0;
}

uint32_t
HTMLTableAccessible::RowCount()
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  return tableFrame ? tableFrame->GetRowCount() : 0;
}

uint32_t
HTMLTableAccessible::SelectedCellCount()
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return 0;

  uint32_t count = 0, rowCount = RowCount(), colCount = ColCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(rowIdx, colIdx);
      if (!cellFrame || !cellFrame->IsSelected())
        continue;

      uint32_t startRow = cellFrame->RowIndex();
      uint32_t startCol = cellFrame->ColIndex();
      if (startRow == rowIdx && startCol == colIdx)
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
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return;

  uint32_t rowCount = RowCount(), colCount = ColCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(rowIdx, colIdx);
      if (!cellFrame || !cellFrame->IsSelected())
        continue;

      uint32_t startRow = cellFrame->RowIndex();
      uint32_t startCol = cellFrame->ColIndex();
      if (startRow != rowIdx || startCol != colIdx)
        continue;

      Accessible* cell = mDoc->GetAccessible(cellFrame->GetContent());
        aCells->AppendElement(cell);
    }
  }
}

void
HTMLTableAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return;

  uint32_t rowCount = RowCount(), colCount = ColCount();
  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (uint32_t colIdx = 0; colIdx < colCount; colIdx++) {
      nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(rowIdx, colIdx);
      if (!cellFrame || !cellFrame->IsSelected())
        continue;

      uint32_t startCol = cellFrame->ColIndex();
      uint32_t startRow = cellFrame->RowIndex();
      if (startRow == rowIdx && startCol == colIdx)
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
HTMLTableAccessible::CellAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return nullptr;

  nsIContent* cellContent = tableFrame->GetCellAt(aRowIdx, aColIdx);
  Accessible* cell = mDoc->GetAccessible(cellContent);

  // XXX bug 576838: crazy tables (like table6 in tables/test_table2.html) may
  // return itself as a cell what makes Orca hang.
  return cell == this ? nullptr : cell;
}

int32_t
HTMLTableAccessible::CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return -1;

  return tableFrame->GetIndexByRowAndColumn(aRowIdx, aColIdx);
}

int32_t
HTMLTableAccessible::ColIndexAt(uint32_t aCellIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return -1;

  int32_t rowIdx = -1, colIdx = -1;
  tableFrame->GetRowAndColumnByIndex(aCellIdx, &rowIdx, &colIdx);
  return colIdx;
}

int32_t
HTMLTableAccessible::RowIndexAt(uint32_t aCellIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return -1;

  int32_t rowIdx = -1, colIdx = -1;
  tableFrame->GetRowAndColumnByIndex(aCellIdx, &rowIdx, &colIdx);
  return rowIdx;
}

void
HTMLTableAccessible::RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                        int32_t* aColIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (tableFrame)
    tableFrame->GetRowAndColumnByIndex(aCellIdx, aRowIdx, aColIdx);
}

uint32_t
HTMLTableAccessible::ColExtentAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return 0;

  return tableFrame->GetEffectiveColSpanAt(aRowIdx, aColIdx);
}

uint32_t
HTMLTableAccessible::RowExtentAt(uint32_t aRowIdx, uint32_t aColIdx)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return 0;

  return tableFrame->GetEffectiveRowSpanAt(aRowIdx, aColIdx);
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
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return false;

  nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(aRowIdx, aColIdx);
  return cellFrame ? cellFrame->IsSelected() : false;
}

void
HTMLTableAccessible::SelectRow(uint32_t aRowIdx)
{
  DebugOnly<nsresult> rv =
    RemoveRowsOrColumnsFromSelection(aRowIdx, TableSelection::Row, true);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "RemoveRowsOrColumnsFromSelection() Shouldn't fail!");

  AddRowOrColumnToSelection(aRowIdx, TableSelection::Row);
}

void
HTMLTableAccessible::SelectCol(uint32_t aColIdx)
{
  DebugOnly<nsresult> rv =
    RemoveRowsOrColumnsFromSelection(aColIdx, TableSelection::Column, true);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "RemoveRowsOrColumnsFromSelection() Shouldn't fail!");

  AddRowOrColumnToSelection(aColIdx, TableSelection::Column);
}

void
HTMLTableAccessible::UnselectRow(uint32_t aRowIdx)
{
  RemoveRowsOrColumnsFromSelection(aRowIdx, TableSelection::Row, false);
}

void
HTMLTableAccessible::UnselectCol(uint32_t aColIdx)
{
  RemoveRowsOrColumnsFromSelection(aColIdx, TableSelection::Column, false);
}

nsresult
HTMLTableAccessible::AddRowOrColumnToSelection(int32_t aIndex, TableSelection aTarget)
{
  bool doSelectRow = (aTarget == TableSelection::Row);

  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return NS_OK;

  uint32_t count = 0;
  if (doSelectRow)
    count = ColCount();
  else
    count = RowCount();

  nsIPresShell* presShell(mDoc->PresShell());
  RefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  for (uint32_t idx = 0; idx < count; idx++) {
    int32_t rowIdx = doSelectRow ? aIndex : idx;
    int32_t colIdx = doSelectRow ? idx : aIndex;
    nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(rowIdx, colIdx);
    if (cellFrame && !cellFrame->IsSelected()) {
      nsresult rv = tableSelection->SelectCellElement(cellFrame->GetContent());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
HTMLTableAccessible::RemoveRowsOrColumnsFromSelection(int32_t aIndex,
                                                      TableSelection aTarget,
                                                      bool aIsOuter)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return NS_OK;

  nsIPresShell* presShell(mDoc->PresShell());
  RefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  bool doUnselectRow = (aTarget == TableSelection::Row);
  uint32_t count = doUnselectRow ? ColCount() : RowCount();

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
        mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::summary,
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

////////////////////////////////////////////////////////////////////////////////
// HTMLCaptionAccessible
////////////////////////////////////////////////////////////////////////////////

Relation
HTMLCaptionAccessible::RelationByType(RelationType aType) const
{
  Relation rel = HyperTextAccessible::RelationByType(aType);
  if (aType == RelationType::LABEL_FOR)
    rel.AppendTarget(Parent());

  return rel;
}

role
HTMLCaptionAccessible::NativeRole() const
{
  return roles::CAPTION;
}
