/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULListboxAccessible_h__
#define mozilla_a11y_XULListboxAccessible_h__

#include "BaseAccessibles.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"
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
  virtual PRUint64 NativeState();
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
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

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
  virtual ~XULListboxAccessible() {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // TableAccessible
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual Accessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual bool IsColSelected(PRUint32 aColIdx);
  virtual bool IsRowSelected(PRUint32 aRowIdx);
  virtual bool IsCellSelected(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual PRUint32 SelectedCellCount();
  virtual PRUint32 SelectedColCount();
  virtual PRUint32 SelectedRowCount();
  virtual void SelectedCellIndices(nsTArray<PRUint32>* aCells);
  virtual void SelectedColIndices(nsTArray<PRUint32>* aCols);
  virtual void SelectedRowIndices(nsTArray<PRUint32>* aRows);
  virtual void SelectRow(PRUint32 aRowIdx);
  virtual void UnselectRow(PRUint32 aRowIdx);

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual void Value(nsString& aValue);
  virtual TableAccessible* AsTable() { return this; }
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

  virtual Accessible* ContainerWidget() const;

protected:
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
  virtual ~XULListitemAccessible() {}

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& aName);
  // Don't use XUL menuitems's description attribute

  // Accessible
  virtual void Description(nsString& aDesc);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;
  virtual bool CanHaveAnonChildren();

  // Widgets
  virtual Accessible* ContainerWidget() const;

protected:
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
                              public nsIAccessibleTableCell
{
public:
  XULListCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // Accessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual a11y::role NativeRole();
};

} // namespace a11y
} // namespace mozilla

#endif
