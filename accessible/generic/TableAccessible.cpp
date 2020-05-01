/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TableAccessible.h"

#include "Accessible-inl.h"
#include "AccIterator.h"

#include "nsTableCellFrame.h"
#include "nsTableWrapperFrame.h"
#include "TableCellAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

bool TableAccessible::IsProbablyLayoutTable() {
  // Implement a heuristic to determine if table is most likely used for layout.

  // XXX do we want to look for rowspan or colspan, especialy that span all but
  // a couple cells  at the beginning or end of a row/col, and especially when
  // they occur at the edge of a table?

  // XXX For now debugging descriptions are always on via SHOW_LAYOUT_HEURISTIC
  // This will allow release trunk builds to be used by testers to refine
  // the algorithm. Integrate it into Logging.
  // Change to |#define SHOW_LAYOUT_HEURISTIC DEBUG| before final release
#ifdef SHOW_LAYOUT_HEURISTIC
#  define RETURN_LAYOUT_ANSWER(isLayout, heuristic)                          \
    {                                                                        \
      mLayoutHeuristic = isLayout                                            \
                             ? NS_LITERAL_STRING("layout table: " heuristic) \
                             : NS_LITERAL_STRING("data table: " heuristic);  \
      return isLayout;                                                       \
    }
#else
#  define RETURN_LAYOUT_ANSWER(isLayout, heuristic) \
    { return isLayout; }
#endif

  Accessible* thisacc = AsAccessible();

  MOZ_ASSERT(!thisacc->IsDefunct(), "Table accessible should not be defunct");

  // Need to see all elements while document is being edited.
  if (thisacc->Document()->State() & states::EDITABLE) {
    RETURN_LAYOUT_ANSWER(false, "In editable document");
  }

  // Check to see if an ARIA role overrides the role from native markup,
  // but for which we still expose table semantics (treegrid, for example).
  if (thisacc->HasARIARole()) {
    RETURN_LAYOUT_ANSWER(false, "Has role attribute");
  }

  dom::Element* el = thisacc->Elm();
  if (el->IsMathMLElement(nsGkAtoms::mtable_)) {
    RETURN_LAYOUT_ANSWER(false, "MathML matrix");
  }

  MOZ_ASSERT(el->IsHTMLElement(nsGkAtoms::table),
             "Table should not be built by CSS display:table style");

  // Check if datatable attribute has "0" value.
  if (el->AttrValueIs(kNameSpaceID_None, nsGkAtoms::datatable,
                      NS_LITERAL_STRING("0"), eCaseMatters)) {
    RETURN_LAYOUT_ANSWER(true, "Has datatable = 0 attribute, it's for layout");
  }

  // Check for legitimate data table attributes.
  if (el->Element::HasNonEmptyAttr(nsGkAtoms::summary)) {
    RETURN_LAYOUT_ANSWER(false, "Has summary -- legitimate table structures");
  }

  // Check for legitimate data table elements.
  Accessible* caption = thisacc->FirstChild();
  if (caption && caption->IsHTMLCaption() && caption->HasChildren()) {
    RETURN_LAYOUT_ANSWER(false,
                         "Not empty caption -- legitimate table structures");
  }

  for (nsIContent* childElm = el->GetFirstChild(); childElm;
       childElm = childElm->GetNextSibling()) {
    if (!childElm->IsHTMLElement()) continue;

    if (childElm->IsAnyOfHTMLElements(nsGkAtoms::col, nsGkAtoms::colgroup,
                                      nsGkAtoms::tfoot, nsGkAtoms::thead)) {
      RETURN_LAYOUT_ANSWER(
          false,
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

              if (cellElm->AsElement()->HasAttr(kNameSpaceID_None,
                                                nsGkAtoms::headers) ||
                  cellElm->AsElement()->HasAttr(kNameSpaceID_None,
                                                nsGkAtoms::scope) ||
                  cellElm->AsElement()->HasAttr(kNameSpaceID_None,
                                                nsGkAtoms::abbr)) {
                RETURN_LAYOUT_ANSWER(false,
                                     "Has headers, scope, or abbr attribute -- "
                                     "legitimate table structures");
              }

              Accessible* cell = thisacc->Document()->GetAccessible(cellElm);
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

  // Check for nested tables.
  nsCOMPtr<nsIHTMLCollection> nestedTables =
      el->GetElementsByTagName(NS_LITERAL_STRING("table"));
  if (nestedTables->Length() > 0) {
    RETURN_LAYOUT_ANSWER(true, "Has a nested table within it");
  }

  // If only 1 column or only 1 row, it's for layout.
  auto colCount = ColCount();
  if (colCount <= 1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 column");
  }
  auto rowCount = RowCount();
  if (rowCount <= 1) {
    RETURN_LAYOUT_ANSWER(true, "Has only 1 row");
  }

  // Check for many columns.
  if (colCount >= 5) {
    RETURN_LAYOUT_ANSWER(false, ">=5 columns");
  }

  // Now we know there are 2-4 columns and 2 or more rows. Check to see if
  // there are visible borders on the cells.
  // XXX currently, we just check the first cell -- do we really need to do
  // more?
  nsTableWrapperFrame* tableFrame = do_QueryFrame(el->GetPrimaryFrame());
  if (!tableFrame) {
    RETURN_LAYOUT_ANSWER(false, "table with no frame!");
  }

  nsIFrame* cellFrame = tableFrame->GetCellFrameAt(0, 0);
  if (!cellFrame) {
    RETURN_LAYOUT_ANSWER(false, "table's first cell has no frame!");
  }

  nsMargin border;
  cellFrame->GetXULBorder(border);
  if (border.top && border.bottom && border.left && border.right) {
    RETURN_LAYOUT_ANSWER(false, "Has nonzero border-width on table cell");
  }

  // Rules for non-bordered tables with 2-4 columns and 2+ rows from here on
  // forward.

  // Check for styled background color across rows (alternating background
  // color is a common feature for data tables).
  auto childCount = thisacc->ChildCount();
  nscolor rowColor = 0;
  nscolor prevRowColor;
  for (auto childIdx = 0U; childIdx < childCount; childIdx++) {
    Accessible* child = thisacc->GetChildAt(childIdx);
    if (child->IsHTMLTableRow()) {
      prevRowColor = rowColor;
      nsIFrame* rowFrame = child->GetFrame();
      MOZ_ASSERT(rowFrame, "Table hierarchy got screwed up");
      if (!rowFrame) {
        RETURN_LAYOUT_ANSWER(false, "Unexpected table hierarchy");
      }

      rowColor = rowFrame->StyleBackground()->BackgroundColor(rowFrame);

      if (childIdx > 0 && prevRowColor != rowColor) {
        RETURN_LAYOUT_ANSWER(false,
                             "2 styles of row background color, non-bordered");
      }
    }
  }

  // Check for many rows.
  const uint32_t kMaxLayoutRows = 20;
  if (rowCount > kMaxLayoutRows) {  // A ton of rows, this is probably for data
    RETURN_LAYOUT_ANSWER(false, ">= kMaxLayoutRows (20) and non-bordered");
  }

  // Check for very wide table.
  nsIFrame* documentFrame = thisacc->Document()->GetFrame();
  nsSize documentSize = documentFrame->GetSize();
  if (documentSize.width > 0) {
    nsSize tableSize = thisacc->GetFrame()->GetSize();
    int32_t percentageOfDocWidth = (100 * tableSize.width) / documentSize.width;
    if (percentageOfDocWidth > 95) {
      // 3-4 columns, no borders, not a lot of rows, and 95% of the doc's width
      // Probably for layout
      RETURN_LAYOUT_ANSWER(
          true, "<= 4 columns, table width is 95% of document width");
    }
  }

  // Two column rules.
  if (rowCount * colCount <= 10) {
    RETURN_LAYOUT_ANSWER(true, "2-4 columns, 10 cells or less, non-bordered");
  }

  static const nsLiteralString tags[] = {NS_LITERAL_STRING("embed"),
                                         NS_LITERAL_STRING("object"),
                                         NS_LITERAL_STRING("iframe")};
  for (auto& tag : tags) {
    nsCOMPtr<nsIHTMLCollection> descendants = el->GetElementsByTagName(tag);
    if (descendants->Length() > 0) {
      RETURN_LAYOUT_ANSWER(true,
                           "Has no borders, and has iframe, object or embed, "
                           "typical of advertisements");
    }
  }

  RETURN_LAYOUT_ANSWER(false,
                       "No layout factor strong enough, so will guess data");
}

Accessible* TableAccessible::RowAt(int32_t aRow) {
  int32_t rowIdx = aRow;

  AccIterator rowIter(this->AsAccessible(), filters::GetRow);

  Accessible* row = rowIter.Next();
  while (rowIdx != 0 && (row = rowIter.Next())) {
    rowIdx--;
  }

  return row;
}

Accessible* TableAccessible::CellInRowAt(Accessible* aRow, int32_t aColumn) {
  int32_t colIdx = aColumn;

  AccIterator cellIter(aRow, filters::GetCell);
  Accessible* cell = nullptr;

  while (colIdx >= 0 && (cell = cellIter.Next())) {
    MOZ_ASSERT(cell->IsTableCell(), "No table or grid cell!");
    colIdx -= cell->AsTableCell()->ColExtent();
  }

  return cell;
}

int32_t TableAccessible::ColIndexAt(uint32_t aCellIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    return -1;  // Error: column count is 0 or index out of bounds.
  }

  return aCellIdx % colCount;
}

int32_t TableAccessible::RowIndexAt(uint32_t aCellIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    return -1;  // Error: column count is 0 or index out of bounds.
  }

  return aCellIdx / colCount;
}

void TableAccessible::RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                         int32_t* aColIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    *aRowIdx = -1;
    *aColIdx = -1;
    return;  // Error: column count is 0 or index out of bounds.
  }

  *aRowIdx = aCellIdx / colCount;
  *aColIdx = aCellIdx % colCount;
}
