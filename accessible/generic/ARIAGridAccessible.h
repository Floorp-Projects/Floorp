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
class ARIAGridAccessible : public HyperTextAccessibleWrap,
                           public TableAccessible {
 public:
  ARIAGridAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ARIAGridAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;
  virtual TableAccessible* AsTable() override { return this; }

  // TableAccessible
  virtual uint32_t ColCount() const override;
  virtual uint32_t RowCount() override;
  virtual LocalAccessible* CellAt(uint32_t aRowIndex,
                                  uint32_t aColumnIndex) override;
  virtual bool IsColSelected(uint32_t aColIdx) override;
  virtual bool IsRowSelected(uint32_t aRowIdx) override;
  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) override;
  virtual uint32_t SelectedCellCount() override;
  virtual uint32_t SelectedColCount() override;
  virtual uint32_t SelectedRowCount() override;
  virtual void SelectedCells(nsTArray<LocalAccessible*>* aCells) override;
  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells) override;
  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols) override;
  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows) override;
  virtual void SelectCol(uint32_t aColIdx) override;
  virtual void SelectRow(uint32_t aRowIdx) override;
  virtual void UnselectCol(uint32_t aColIdx) override;
  virtual void UnselectRow(uint32_t aRowIdx) override;
  virtual LocalAccessible* AsAccessible() override { return this; }

 protected:
  virtual ~ARIAGridAccessible() {}

  /**
   * Set aria-selected attribute value on DOM node of the given accessible.
   *
   * @param  aAccessible  [in] accessible
   * @param  aIsSelected  [in] new value of aria-selected attribute
   * @param  aNotify      [in, optional] specifies if DOM should be notified
   *                       about attribute change (used internally).
   */
  nsresult SetARIASelected(LocalAccessible* aAccessible, bool aIsSelected,
                           bool aNotify = true);
};

/**
 * Accessible for ARIA row.
 */
class ARIARowAccessible : public HyperTextAccessibleWrap {
 public:
  ARIARowAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ARIARowAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual mozilla::a11y::GroupPos GroupPosition() override;

 protected:
  virtual ~ARIARowAccessible() {}

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * Accessible for ARIA gridcell and rowheader/columnheader.
 */
class ARIAGridCellAccessible : public HyperTextAccessibleWrap,
                               public TableCellAccessible {
 public:
  ARIAGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ARIAGridCellAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual TableCellAccessible* AsTableCell() override { return this; }
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;
  virtual mozilla::a11y::GroupPos GroupPosition() override;

 protected:
  virtual ~ARIAGridCellAccessible() {}

  /**
   * Return a containing row.
   */
  LocalAccessible* Row() const {
    LocalAccessible* row = LocalParent();
    return row && row->IsTableRow() ? row : nullptr;
  }

  /**
   * Return index of the given row.
   * Returns -1 upon error.
   */
  int32_t RowIndexFor(LocalAccessible* aRow) const;

  // TableCellAccessible
  virtual TableAccessible* Table() const override;
  virtual uint32_t ColIdx() const override;
  virtual uint32_t RowIdx() const override;
  virtual bool Selected() override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
