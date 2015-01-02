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
  virtual TableAccessible* AsTable() MOZ_OVERRIDE { return this; }

  // TableAccessible
  virtual uint32_t ColCount() MOZ_OVERRIDE;
  virtual uint32_t RowCount() MOZ_OVERRIDE;
  virtual Accessible* CellAt(uint32_t aRowIndex, uint32_t aColumnIndex) MOZ_OVERRIDE;
  virtual bool IsColSelected(uint32_t aColIdx) MOZ_OVERRIDE;
  virtual bool IsRowSelected(uint32_t aRowIdx) MOZ_OVERRIDE;
  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) MOZ_OVERRIDE;
  virtual uint32_t SelectedCellCount() MOZ_OVERRIDE;
  virtual uint32_t SelectedColCount() MOZ_OVERRIDE;
  virtual uint32_t SelectedRowCount() MOZ_OVERRIDE;
  virtual void SelectedCells(nsTArray<Accessible*>* aCells) MOZ_OVERRIDE;
  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells) MOZ_OVERRIDE;
  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols) MOZ_OVERRIDE;
  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows) MOZ_OVERRIDE;
  virtual void SelectCol(uint32_t aColIdx) MOZ_OVERRIDE;
  virtual void SelectRow(uint32_t aRowIdx) MOZ_OVERRIDE;
  virtual void UnselectCol(uint32_t aColIdx) MOZ_OVERRIDE;
  virtual void UnselectRow(uint32_t aRowIdx) MOZ_OVERRIDE;
  virtual Accessible* AsAccessible() MOZ_OVERRIDE { return this; }

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
  virtual TableCellAccessible* AsTableCell() MOZ_OVERRIDE { return this; }
  virtual void ApplyARIAState(uint64_t* aState) const MOZ_OVERRIDE;
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
