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
#include "nsIDOMElement.h"
#include "nsIDOMRange.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMNodeList.h"
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

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableCellAccessible, HyperTextAccessible)

////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessible: Accessible implementation

role
HTMLTableCellAccessible::NativeRole()
{
  if (mContent->IsMathMLElement(nsGkAtoms::mtd_)) {
    return roles::MATHML_CELL;
  }
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
HTMLTableHeaderCellAccessible::NativeRole()
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

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableRowAccessible, Accessible)

role
HTMLTableRowAccessible::NativeRole()
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

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableAccessible, Accessible)

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
HTMLTableAccessible::NativeRole()
{
  if (mContent->IsMathMLElement(nsGkAtoms::mtable_)) {
    return roles::MATHML_TABLE;
  }
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
HTMLTableAccessible::RelationByType(RelationType aType)
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
  dom::HTMLTableElement* table = dom::HTMLTableElement::FromContent(mContent);

  if (table)
    table->GetSummary(aSummary);
}

uint32_t
HTMLTableAccessible::ColCount()
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
  DebugOnly<nsresult> rv =
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
                                                      uint32_t aTarget,
                                                      bool aIsOuter)
{
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    return NS_OK;

  nsIPresShell* presShell(mDoc->PresShell());
  RefPtr<nsFrameSelection> tableSelection =
    const_cast<nsFrameSelection*>(presShell->ConstFrameSelection());

  bool doUnselectRow = (aTarget == nsISelectionPrivate::TABLESELECTION_ROW);
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

bool
HTMLTableAccessible::HasDescendant(const nsAString& aTagName, bool aAllowEmpty)
{
  nsCOMPtr<nsIHTMLCollection> elements =
    mContent->AsElement()->GetElementsByTagName(aTagName);

  Element* foundItem = elements->Item(0);
  if (!foundItem)
    return false;

  if (aAllowEmpty)
    return true;

  // Make sure that the item we found has contents and either has multiple
  // children or the found item is not a whitespace-only text node.
  if (foundItem->GetChildCount() > 1)
    return true; // Treat multiple child nodes as non-empty

  nsIContent *innerItemContent = foundItem->GetFirstChild();
  if (innerItemContent && !innerItemContent->TextIsOnlyWhitespace())
    return true;

  // If we found more than one node then return true not depending on
  // aAllowEmpty flag.
  // XXX it might be dummy but bug 501375 where we changed this addresses
  // performance problems only. Note, currently 'aAllowEmpty' flag is used for
  // caption element only. On another hand we create accessible object for
  // the first entry of caption element (see
  // HTMLTableAccessible::InsertChildAt).
  return !!elements->Item(1);
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

  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::role)) {
    // Role attribute is present, but overridden roles have already been dealt with.
    // Only landmarks and other roles that don't override the role from native
    // markup are left to deal with here.
    RETURN_LAYOUT_ANSWER(false, "Has role attribute, weak role, and role is table");
  }

  NS_ASSERTION(mContent->IsHTMLElement(nsGkAtoms::table),
    "table should not be built by CSS display:table style");

  // Check if datatable attribute has "0" value.
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::datatable,
                            NS_LITERAL_STRING("0"), eCaseMatters)) {
    RETURN_LAYOUT_ANSWER(true, "Has datatable = 0 attribute, it's for layout");
  }

  // Check for legitimate data table attributes.
  nsAutoString summary;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::summary, summary) &&
      !summary.IsEmpty())
    RETURN_LAYOUT_ANSWER(false, "Has summary -- legitimate table structures");

  // Check for legitimate data table elements.
  Accessible* caption = FirstChild();
  if (caption && caption->Role() == roles::CAPTION && caption->HasChildren())
    RETURN_LAYOUT_ANSWER(false, "Not empty caption -- legitimate table structures");

  for (nsIContent* childElm = mContent->GetFirstChild(); childElm;
       childElm = childElm->GetNextSibling()) {
    if (!childElm->IsHTMLElement())
      continue;

    if (childElm->IsAnyOfHTMLElements(nsGkAtoms::col,
                                      nsGkAtoms::colgroup,
                                      nsGkAtoms::tfoot,
                                      nsGkAtoms::thead)) {
      RETURN_LAYOUT_ANSWER(false,
                           "Has col, colgroup, tfoot or thead -- legitimate table structures");
    }

    if (childElm->IsHTMLElement(nsGkAtoms::tbody)) {
      for (nsIContent* rowElm = childElm->GetFirstChild(); rowElm;
           rowElm = rowElm->GetNextSibling()) {
        if (rowElm->IsHTMLElement(nsGkAtoms::tr)) {
          for (nsIContent* cellElm = rowElm->GetFirstChild(); cellElm;
               cellElm = cellElm->GetNextSibling()) {
            if (cellElm->IsHTMLElement()) {

              if (cellElm->NodeInfo()->Equals(nsGkAtoms::th)) {
                RETURN_LAYOUT_ANSWER(false,
                                     "Has th -- legitimate table structures");
              }

              if (cellElm->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::headers) ||
                  cellElm->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::scope) ||
                  cellElm->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::abbr)) {
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
  uint32_t colCount = ColCount();
  if (colCount <=1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 column");
  }
  uint32_t rowCount = RowCount();
  if (rowCount <=1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 row");
  }

  // Check for many columns
  if (colCount >= 5) {
    RETURN_LAYOUT_ANSWER(false, ">=5 columns");
  }

  // Now we know there are 2-4 columns and 2 or more rows
  // Check to see if there are visible borders on the cells
  // XXX currently, we just check the first cell -- do we really need to do more?
  nsTableWrapperFrame* tableFrame = do_QueryFrame(mContent->GetPrimaryFrame());
  if (!tableFrame)
    RETURN_LAYOUT_ANSWER(false, "table with no frame!");

  nsIFrame* cellFrame = tableFrame->GetCellFrameAt(0, 0);
  if (!cellFrame)
    RETURN_LAYOUT_ANSWER(false, "table's first cell has no frame!");

  nsMargin border;
  cellFrame->GetXULBorder(border);
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
      rowColor = rowFrame->StyleBackground()->BackgroundColor(rowFrame);

      if (childIdx > 0 && prevRowColor != rowColor)
        RETURN_LAYOUT_ANSWER(false, "2 styles of row background color, non-bordered");
    }
  }

  // Check for many rows
  const uint32_t kMaxLayoutRows = 20;
  if (rowCount > kMaxLayoutRows) { // A ton of rows, this is probably for data
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
  if (rowCount * colCount <= 10) {
    RETURN_LAYOUT_ANSWER(true, "2-4 columns, 10 cells or less, non-bordered");
  }

  if (HasDescendant(NS_LITERAL_STRING("embed")) ||
      HasDescendant(NS_LITERAL_STRING("object")) ||
      HasDescendant(NS_LITERAL_STRING("iframe"))) {
    RETURN_LAYOUT_ANSWER(true, "Has no borders, and has iframe, object, or iframe, typical of advertisements");
  }

  RETURN_LAYOUT_ANSWER(false, "no layout factor strong enough, so will guess data");
}


////////////////////////////////////////////////////////////////////////////////
// HTMLCaptionAccessible
////////////////////////////////////////////////////////////////////////////////

Relation
HTMLCaptionAccessible::RelationByType(RelationType aType)
{
  Relation rel = HyperTextAccessible::RelationByType(aType);
  if (aType == RelationType::LABEL_FOR)
    rel.AppendTarget(Parent());

  return rel;
}

role
HTMLCaptionAccessible::NativeRole()
{
  return roles::CAPTION;
}
