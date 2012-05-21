/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXULTreeGridAccessible_h__
#define __nsXULTreeGridAccessible_h__

#include "nsXULTreeAccessible.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"

/**
 * Represents accessible for XUL tree in the case when it has multiple columns.
 */
class nsXULTreeGridAccessible : public nsXULTreeAccessible,
                                public xpcAccessibleTable,
                                public nsIAccessibleTable,
                                public mozilla::a11y::TableAccessible
{
public:
  nsXULTreeGridAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // TableAccessible
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual void UnselectRow(PRUint32 aRowIdx);

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual mozilla::a11y::TableAccessible* AsTable() { return this; }
  virtual mozilla::a11y::role NativeRole();

protected:

  // nsXULTreeAccessible
  virtual already_AddRefed<nsAccessible> CreateTreeItemAccessible(PRInt32 aRow);
};


/**
 * Represents accessible for XUL tree item in the case when XUL tree has
 * multiple columns.
 */
class nsXULTreeGridRowAccessible : public nsXULTreeItemAccessibleBase
{
public:
  using nsAccessible::GetChildCount;
  using nsAccessible::GetChildAt;

  nsXULTreeGridRowAccessible(nsIContent* aContent, nsDocAccessible* aDoc,
                             nsAccessible* aParent, nsITreeBoxObject* aTree,
                             nsITreeView* aTreeView, PRInt32 aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeGridRowAccessible,
                                           nsXULTreeItemAccessibleBase)

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual nsAccessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     EWhichChildAtPoint aWhichChild);

  virtual nsAccessible* GetChildAt(PRUint32 aIndex);
  virtual PRInt32 GetChildCount();

  // nsXULTreeItemAccessibleBase
  virtual nsAccessible* GetCellAccessible(nsITreeColumn *aColumn);
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx);

protected:

  // nsAccessible
  virtual void CacheChildren();

  // nsXULTreeItemAccessibleBase
  nsAccessibleHashtable mAccessibleCache;
};


/**
 * Represents an accessible for XUL tree cell in the case when XUL tree has
 * multiple columns.
 */

#define NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID         \
{  /* 84588ad4-549c-4196-a932-4c5ca5de5dff */         \
  0x84588ad4,                                         \
  0x549c,                                             \
  0x4196,                                             \
  { 0xa9, 0x32, 0x4c, 0x5c, 0xa5, 0xde, 0x5d, 0xff }  \
}

class nsXULTreeGridCellAccessible : public nsLeafAccessible,
                                    public nsIAccessibleTableCell
{
public:
  using nsAccessible::GetParent;

  nsXULTreeGridCellAccessible(nsIContent* aContent, nsDocAccessible* aDoc,
                              nsXULTreeGridRowAccessible* aRowAcc,
                              nsITreeBoxObject* aTree, nsITreeView* aTreeView,
                              PRInt32 aRow, nsITreeColumn* aColumn);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeGridCellAccessible,
                                           nsLeafAccessible)

  // nsIAccessible

  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);

  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessNode
  virtual bool Init();
  virtual bool IsPrimaryForNode() const;

  // nsAccessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual nsAccessible* FocusedChild();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual PRInt32 IndexInParent() const;
  virtual Relation RelationByType(PRUint32 aType);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // nsXULTreeGridCellAccessible
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

  /**
   * Return index of the column.
   */
  PRInt32 GetColumnIndex() const;

  /**
   * Fire name or state change event if the accessible text or value has been
   * changed.
   */
  void CellInvalidated();

protected:
  // nsAccessible
  virtual nsAccessible* GetSiblingAtOffset(PRInt32 aOffset,
                                           nsresult *aError = nsnull) const;
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);

  // nsXULTreeGridCellAccessible

  /**
   * Return true if value of cell can be modified.
   */
  bool IsEditable() const;

  enum { eAction_Click = 0 };

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsITreeView* mTreeView;

  PRInt32 mRow;
  nsCOMPtr<nsITreeColumn> mColumn;

  nsString mCachedTextEquiv;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeGridCellAccessible,
                              NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

#endif
