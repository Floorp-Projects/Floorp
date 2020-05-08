/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULListboxAccessible_h__
#define mozilla_a11y_XULListboxAccessible_h__

#include "BaseAccessibles.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "XULMenuAccessible.h"
#include "XULSelectControlAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * XULColumAccessible are accessible for list and tree columns elements
 * (xul:treecols and xul:listheader).
 */
class XULColumAccessible : public AccessibleWrap {
 public:
  XULColumAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
};

/**
 * XULColumnItemAccessible are accessibles for list and tree column elements
 * (xul:treecol).
 */
class XULColumnItemAccessible : public LeafAccessible {
 public:
  XULColumnItemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  enum { eAction_Click = 0 };
};

/*
 * A class the represents the XUL Listbox widget.
 */
class XULListboxAccessible : public XULSelectControlAccessible,
                             public TableAccessible {
 public:
  XULListboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // TableAccessible
  virtual uint32_t ColCount() const override;
  virtual uint32_t RowCount() override;
  virtual Accessible* CellAt(uint32_t aRowIndex,
                             uint32_t aColumnIndex) override;
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
  virtual void SelectRow(uint32_t aRowIdx) override;
  virtual void UnselectRow(uint32_t aRowIdx) override;
  virtual Accessible* AsAccessible() override { return this; }

  // Accessible
  virtual TableAccessible* AsTable() override { return this; }
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;

  virtual Accessible* ContainerWidget() const override;

 protected:
  virtual ~XULListboxAccessible() {}

  bool IsMulticolumn() const { return ColCount() > 1; }
};

/**
 * Listitems -- used in listboxes
 */
class XULListitemAccessible : public XULMenuitemAccessible {
 public:
  enum { eAction_Click = 0 };

  NS_INLINE_DECL_REFCOUNTING_INHERITED(XULListitemAccessible,
                                       XULMenuitemAccessible)

  XULListitemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Description(nsString& aDesc) override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;

  // Actions
  virtual void ActionNameAt(uint8_t index, nsAString& aName) override;

  // Widgets
  virtual Accessible* ContainerWidget() const override;

 protected:
  virtual ~XULListitemAccessible();

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  // XULListitemAccessible

  /**
   * Return listbox accessible for the listitem.
   */
  Accessible* GetListAccessible() const;

 private:
  bool mIsCheckbox;
};

}  // namespace a11y
}  // namespace mozilla

#endif
