/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeGridAccessible_h__
#define mozilla_a11y_XULTreeGridAccessible_h__

#include "XULTreeAccessible.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"

namespace mozilla {
namespace a11y {

/**
 * Represents accessible for XUL tree in the case when it has multiple columns.
 */
class XULTreeGridAccessible : public XULTreeAccessible,
                              public xpcAccessibleTable,
                              public nsIAccessibleTable,
                              public TableAccessible
{
public:
  XULTreeGridAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // TableAccessible
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual Accessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual void SelectRow(PRUint32 aRowIdx);
  virtual void UnselectRow(PRUint32 aRowIdx);

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual TableAccessible* AsTable() { return this; }
  virtual a11y::role NativeRole();

protected:

  // XULTreeAccessible
  virtual already_AddRefed<Accessible> CreateTreeItemAccessible(PRInt32 aRow);
};


/**
 * Represents accessible for XUL tree item in the case when XUL tree has
 * multiple columns.
 */
class XULTreeGridRowAccessible : public XULTreeItemAccessibleBase
{
public:
  using Accessible::GetChildAt;

  XULTreeGridRowAccessible(nsIContent* aContent, DocAccessible* aDoc,
                           Accessible* aParent, nsITreeBoxObject* aTree,
                           nsITreeView* aTreeView, PRInt32 aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeGridRowAccessible,
                                           XULTreeItemAccessibleBase)

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual a11y::role NativeRole();
  virtual ENameValueFlag Name(nsString& aName);
  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);

  virtual Accessible* GetChildAt(PRUint32 aIndex);
  virtual PRUint32 ChildCount() const;

  // XULTreeItemAccessibleBase
  virtual Accessible* GetCellAccessible(nsITreeColumn* aColumn);
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx);

protected:

  // Accessible
  virtual void CacheChildren();

  // XULTreeItemAccessibleBase
  AccessibleHashtable mAccessibleCache;
};


/**
 * Represents an accessible for XUL tree cell in the case when XUL tree has
 * multiple columns.
 */

#define XULTREEGRIDCELLACCESSIBLE_IMPL_CID            \
{  /* 84588ad4-549c-4196-a932-4c5ca5de5dff */         \
  0x84588ad4,                                         \
  0x549c,                                             \
  0x4196,                                             \
  { 0xa9, 0x32, 0x4c, 0x5c, 0xa5, 0xde, 0x5d, 0xff }  \
}

class XULTreeGridCellAccessible : public LeafAccessible,
                                  public nsIAccessibleTableCell
{
public:

  XULTreeGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc,
                            XULTreeGridRowAccessible* aRowAcc,
                            nsITreeBoxObject* aTree, nsITreeView* aTreeView,
                            PRInt32 aRow, nsITreeColumn* aColumn);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeGridCellAccessible,
                                           LeafAccessible)

  // nsIAccessible

  NS_IMETHOD GetBounds(PRInt32* aX, PRInt32* aY,
                       PRInt32* aWidth, PRInt32* aHeight);

  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessNode
  virtual bool Init();
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual ENameValueFlag Name(nsString& aName);
  virtual Accessible* FocusedChild();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties* aAttributes);
  virtual PRInt32 IndexInParent() const;
  virtual Relation RelationByType(PRUint32 aType);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // XULTreeGridCellAccessible
  NS_DECLARE_STATIC_IID_ACCESSOR(XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

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
  // Accessible
  virtual Accessible* GetSiblingAtOffset(PRInt32 aOffset,
                                         nsresult* aError = nsnull) const;
  virtual void DispatchClickEvent(nsIContent* aContent, PRUint32 aActionIndex);

  // XULTreeGridCellAccessible

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

NS_DEFINE_STATIC_IID_ACCESSOR(XULTreeGridCellAccessible,
                              XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

} // namespace a11y
} // namespace mozilla

#endif
