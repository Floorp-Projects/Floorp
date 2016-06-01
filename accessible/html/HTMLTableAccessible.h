/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLTableAccessible_h__
#define mozilla_a11y_HTMLTableAccessible_h__

#include "HyperTextAccessibleWrap.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"

class nsITableCellLayout;

namespace mozilla {
namespace a11y {

/**
 * HTML table cell accessible (html:td).
 */
class HTMLTableCellAccessible : public HyperTextAccessibleWrap,
                                public TableCellAccessible
{
public:
  HTMLTableCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual TableCellAccessible* AsTableCell() override { return this; }
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;
  virtual mozilla::a11y::GroupPos GroupPosition() override;

  // TableCellAccessible
  virtual TableAccessible* Table() const override;
  virtual uint32_t ColIdx() const override;
  virtual uint32_t RowIdx() const override;
  virtual uint32_t ColExtent() const override;
  virtual uint32_t RowExtent() const override;
  virtual void ColHeaderCells(nsTArray<Accessible*>* aCells) override;
  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells) override;
  virtual bool Selected() override;

protected:
  virtual ~HTMLTableCellAccessible() {}

  /**
   * Return nsITableCellLayout of the table cell frame.
   */
  nsITableCellLayout* GetCellLayout() const;

  /**
   * Return row and column indices of the cell.
   */
  nsresult GetCellIndexes(int32_t& aRowIdx, int32_t& aColIdx) const;
};


/**
 * HTML table row/column header accessible (html:th or html:td@scope).
 */
class HTMLTableHeaderCellAccessible : public HTMLTableCellAccessible
{
public:
  HTMLTableHeaderCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() override;
};


/**
 * HTML table row accessible (html:tr).
 */
class HTMLTableRowAccessible : public AccessibleWrap
{
public:
  HTMLTableRowAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc)
  {
    mType = eHTMLTableRowType;
    mGenericTypes |= eTableRow;
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual mozilla::a11y::GroupPos GroupPosition() override;

protected:
  virtual ~HTMLTableRowAccessible() { }
};


/**
 * HTML table accessible (html:table).
 */

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

class HTMLTableAccessible : public AccessibleWrap,
                            public TableAccessible
{
public:
  HTMLTableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    AccessibleWrap(aContent, aDoc)
  {
    mType = eHTMLTableType;
    mGenericTypes |= eTable;
  }

  NS_DECL_ISUPPORTS_INHERITED

  // TableAccessible
  virtual Accessible* Caption() const override;
  virtual void Summary(nsString& aSummary) override;
  virtual uint32_t ColCount() override;
  virtual uint32_t RowCount() override;
  virtual Accessible* CellAt(uint32_t aRowIndex, uint32_t aColumnIndex) override;
  virtual int32_t CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx) override;
  virtual int32_t ColIndexAt(uint32_t aCellIdx) override;
  virtual int32_t RowIndexAt(uint32_t aCellIdx) override;
  virtual void RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                  int32_t* aColIdx) override;
  virtual uint32_t ColExtentAt(uint32_t aRowIdx, uint32_t aColIdx) override;
  virtual uint32_t RowExtentAt(uint32_t aRowIdx, uint32_t aColIdx) override;
  virtual bool IsColSelected(uint32_t aColIdx) override;
  virtual bool IsRowSelected(uint32_t aRowIdx) override;
  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) override;
  virtual uint32_t SelectedCellCount() override;
  virtual uint32_t SelectedColCount() override;
  virtual uint32_t SelectedRowCount() override;
  virtual void SelectedCells(nsTArray<Accessible*>* aCells) override;
  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells) override;
  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols) override;
  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows) override;
  virtual void SelectCol(uint32_t aColIdx) override;
  virtual void SelectRow(uint32_t aRowIdx) override;
  virtual void UnselectCol(uint32_t aColIdx) override;
  virtual void UnselectRow(uint32_t aRowIdx) override;
  virtual bool IsProbablyLayoutTable() override;
  virtual Accessible* AsAccessible() override { return this; }

  // Accessible
  virtual TableAccessible* AsTable() override { return this; }
  virtual void Description(nsString& aDescription) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;
  virtual Relation RelationByType(RelationType aRelationType) override;

  virtual bool InsertChildAt(uint32_t aIndex, Accessible* aChild) override;

protected:
  virtual ~HTMLTableAccessible() {}

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

  // HTMLTableAccessible

  /**
   * Add row or column to selection.
   *
   * @param aIndex   [in] index of row or column to be selected
   * @param aTarget  [in] indicates what should be selected, either row or column
   *                  (see nsISelectionPrivate)
   */
  nsresult AddRowOrColumnToSelection(int32_t aIndex, uint32_t aTarget);

  /**
   * Removes rows or columns at the given index or outside it from selection.
   *
   * @param  aIndex    [in] row or column index
   * @param  aTarget   [in] indicates whether row or column should unselected
   * @param  aIsOuter  [in] indicates whether all rows or column excepting
   *                    the given one should be unselected or the given one
   *                    should be unselected only
   */
  nsresult RemoveRowsOrColumnsFromSelection(int32_t aIndex,
                                            uint32_t aTarget,
                                            bool aIsOuter);

  /**
   * Return true if table has an element with the given tag name.
   *
   * @param  aTagName     [in] tag name of searched element
   * @param  aAllowEmpty  [in, optional] points if found element can be empty
   *                       or contain whitespace text only.
   */
  bool HasDescendant(const nsAString& aTagName, bool aAllowEmpty = true);

#ifdef SHOW_LAYOUT_HEURISTIC
  nsString mLayoutHeuristic;
#endif
};

/**
 * HTML caption accessible (html:caption).
 */
class HTMLCaptionAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLCaptionAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessibleWrap(aContent, aDoc) { mType = eHTMLCaptionType; }

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual Relation RelationByType(RelationType aRelationType) override;

protected:
  virtual ~HTMLCaptionAccessible() { }
};

} // namespace a11y
} // namespace mozilla

#endif
