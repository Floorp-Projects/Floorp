/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsXULListboxAccessible_h__
#define __nsXULListboxAccessible_h__

#include "nsCOMPtr.h"
#include "nsXULMenuAccessible.h"
#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"
#include "XULSelectControlAccessible.h"

class nsIWeakReference;

/**
 * nsXULColumnsAccessible are accessible for list and tree columns elements
 * (xul:treecols and xul:listcols).
 */
class nsXULColumnsAccessible : public nsAccessibleWrap
{
public:
  nsXULColumnsAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
};

/**
 * nsXULColumnAccessible are accessibles for list and tree column elements
 * (xul:listcol and xul:treecol).
 */
class nsXULColumnItemAccessible : public nsLeafAccessible
{
public:
  nsXULColumnItemAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  enum { eAction_Click = 0 };
};

/*
 * A class the represents the XUL Listbox widget.
 */
class nsXULListboxAccessible : public XULSelectControlAccessible,
                               public xpcAccessibleTable,
                               public nsIAccessibleTable,
                               public mozilla::a11y::TableAccessible
{
public:
  nsXULListboxAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsXULListboxAccessible() {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // TableAccessible
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual nsAccessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual void UnselectRow(PRUint32 aRowIdx);

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual void Value(nsString& aValue);
  virtual mozilla::a11y::TableAccessible* AsTable() { return this; }
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;

  virtual nsAccessible* ContainerWidget() const;

protected:
  bool IsMulticolumn();
};

/**
  * Listitems -- used in listboxes 
  */
class nsXULListitemAccessible : public nsXULMenuitemAccessible
{
public:
  enum { eAction_Click = 0 };

  NS_DECL_ISUPPORTS_INHERITED
  
  nsXULListitemAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsXULListitemAccessible() {}

  // nsIAccessible
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& aName);
  // Don't use XUL menuitems's description attribute

  // nsAccessible
  virtual void Description(nsString& aDesc);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual bool CanHaveAnonChildren();

  // Widgets
  virtual nsAccessible* ContainerWidget() const;

protected:
  /**
   * Return listbox accessible for the listitem.
   */
  nsAccessible *GetListAccessible();

private:
  bool mIsCheckbox;
};

/**
 * Class represents xul:listcell.
 */
class nsXULListCellAccessible : public nsHyperTextAccessibleWrap,
                                public nsIAccessibleTableCell
{
public:
  nsXULListCellAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual mozilla::a11y::role NativeRole();
};

#endif
