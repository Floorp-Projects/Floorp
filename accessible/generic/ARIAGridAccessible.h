/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_ARIAGridAccessible_h_
#define MOZILLA_A11Y_ARIAGridAccessible_h_

#include "HyperTextAccessibleWrap.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for ARIA grid and treegrid.
 */
class ARIAGridAccessible : public AccessibleWrap,
                           public TableAccessible
{
public:
  ARIAGridAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual TableAccessible* AsTable() { return this; }

  // TableAccessible
  virtual uint32_t ColCount();
  virtual uint32_t RowCount();
  virtual Accessible* CellAt(uint32_t aRowIndex, uint32_t aColumnIndex);
  virtual bool IsColSelected(uint32_t aColIdx);
  virtual bool IsRowSelected(uint32_t aRowIdx);
  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx);
  virtual uint32_t SelectedCellCount();
  virtual uint32_t SelectedColCount();
  virtual uint32_t SelectedRowCount();
  virtual void SelectedCells(nsTArray<Accessible*>* aCells);
  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells);
  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols);
  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows);
  virtual void SelectCol(uint32_t aColIdx);
  virtual void SelectRow(uint32_t aRowIdx);
  virtual void UnselectCol(uint32_t aColIdx);
  virtual void UnselectRow(uint32_t aRowIdx);
  virtual Accessible* AsAccessible() { return this; }

protected:
  virtual ~ARIAGridAccessible() {}

  /**
   * Return row accessible at the given row index.
   */
  Accessible* GetRowAt(int32_t aRow);

  /**
   * Return cell accessible at the given column index in the row.
   */
  Accessible* GetCellInRowAt(Accessible* aRow, int32_t aColumn);

  /**
   * Set aria-selected attribute value on DOM node of the given accessible.
   *
   * @param  aAccessible  [in] accessible
   * @param  aIsSelected  [in] new value of aria-selected attribute
   * @param  aNotify      [in, optional] specifies if DOM should be notified
   *                       about attribute change (used internally).
   */
  nsresult SetARIASelected(Accessible* aAccessible, bool aIsSelected,
                           bool aNotify = true);
};


/**
 * Accessible for ARIA gridcell and rowheader/columnheader.
 */
class ARIAGridCellAccessible : public HyperTextAccessibleWrap,
                               public TableCellAccessible
{
public:
  ARIAGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual TableCellAccessible* AsTableCell() { return this; }
  virtual void ApplyARIAState(uint64_t* aState) const;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() MOZ_OVERRIDE;

protected:
  virtual ~ARIAGridCellAccessible() {}

  /**
   * Return a containing row.
   */
  Accessible* Row() const
  {
    Accessible* row = Parent();
    return row && row->Role() == roles::ROW ? row : nullptr;
  }

  /**
   * Return a table for the given row.
   */
  Accessible* TableFor(Accessible* aRow) const;

  /**
   * Return index of the given row.
   */
  int32_t RowIndexFor(Accessible* aRow) const;

  // TableCellAccessible
  virtual TableAccessible* Table() const MOZ_OVERRIDE;
  virtual uint32_t ColIdx() const MOZ_OVERRIDE;
  virtual uint32_t RowIdx() const MOZ_OVERRIDE;
  virtual bool Selected() MOZ_OVERRIDE;
};

} // namespace a11y
} // namespace mozilla

#endif
