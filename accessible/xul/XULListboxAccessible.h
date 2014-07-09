/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULListboxAccessible_h__
#define mozilla_a11y_XULListboxAccessible_h__

#include "BaseAccessibles.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "xpcAccessibleTable.h"
#include "xpcAccessibleTableCell.h"
#include "XULMenuAccessible.h"
#include "XULSelectControlAccessible.h"

class nsIWeakReference;

namespace mozilla {
namespace a11y {

/**
 * XULColumAccessible are accessible for list and tree columns elements
 * (xul:treecols and xul:listcols).
 */
class XULColumAccessible : public AccessibleWrap
{
public:
  XULColumAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
};

/**
 * XULColumnItemAccessible are accessibles for list and tree column elements
 * (xul:listcol and xul:treecol).
 */
class XULColumnItemAccessible : public LeafAccessible
{
public:
  XULColumnItemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t aIndex);

  // Accessible
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();

  // ActionAccessible
  virtual uint8_t ActionCount();

  enum { eAction_Click = 0 };
};

/*
 * A class the represents the XUL Listbox widget.
 */
class XULListboxAccessible : public XULSelectControlAccessible,
                             public xpcAccessibleTable,
                             public nsIAccessibleTable,
                             public TableAccessible
{
public:
  XULListboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_FORWARD_NSIACCESSIBLETABLE(xpcAccessibleTable::)

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
  virtual void SelectRow(uint32_t aRowIdx);
  virtual void UnselectRow(uint32_t aRowIdx);
  virtual Accessible* AsAccessible() { return this; }

  // Accessible
  virtual void Shutdown();
  virtual void Value(nsString& aValue);
  virtual TableAccessible* AsTable() { return this; }
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

  virtual Accessible* ContainerWidget() const;

protected:
  virtual ~XULListboxAccessible() {}

  bool IsMulticolumn();
};

/**
  * Listitems -- used in listboxes
  */
class XULListitemAccessible : public XULMenuitemAccessible
{
public:
  enum { eAction_Click = 0 };

  NS_DECL_ISUPPORTS_INHERITED

  XULListitemAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t index, nsAString& aName);
  // Don't use XUL menuitems's description attribute

  // Accessible
  virtual void Description(nsString& aDesc);
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual uint64_t NativeInteractiveState() const;
  virtual bool CanHaveAnonChildren();

  // Widgets
  virtual Accessible* ContainerWidget() const;

protected:
  virtual ~XULListitemAccessible();

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;

  // XULListitemAccessible

  /**
   * Return listbox accessible for the listitem.
   */
  Accessible* GetListAccessible();

private:
  bool mIsCheckbox;
};

/**
 * Class represents xul:listcell.
 */
class XULListCellAccessible : public HyperTextAccessibleWrap,
                              public nsIAccessibleTableCell,
                              public TableCellAccessible,
                              public xpcAccessibleTableCell
{
public:
  XULListCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_FORWARD_NSIACCESSIBLETABLECELL(xpcAccessibleTableCell::)

  // Accessible
  virtual TableCellAccessible* AsTableCell() { return this; }
  virtual void Shutdown();
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() MOZ_OVERRIDE;
  virtual a11y::role NativeRole();

  // TableCellAccessible
  virtual TableAccessible* Table() const MOZ_OVERRIDE;
  virtual uint32_t ColIdx() const MOZ_OVERRIDE;
  virtual uint32_t RowIdx() const MOZ_OVERRIDE;
  virtual void ColHeaderCells(nsTArray<Accessible*>* aHeaderCells) MOZ_OVERRIDE;
  virtual bool Selected() MOZ_OVERRIDE;

protected:
  virtual ~XULListCellAccessible() {}
};

} // namespace a11y
} // namespace mozilla

#endif
