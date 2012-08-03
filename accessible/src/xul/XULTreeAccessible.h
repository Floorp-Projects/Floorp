/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeAccessible_h__
#define mozilla_a11y_XULTreeAccessible_h__

#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsITreeColumns.h"
#include "XULListboxAccessible.h"

namespace mozilla {
namespace a11y {

/*
 * A class the represents the XUL Tree widget.
 */
const PRUint32 kMaxTreeColumns = 100;
const PRUint32 kDefaultTreeCacheSize = 256;

/**
 * Accessible class for XUL tree element.
 */

class XULTreeAccessible : public AccessibleWrap
{
public:
  using Accessible::GetChildAt;

  XULTreeAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeAccessible, Accessible)

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual Accessible* ChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   EWhichChildAtPoint aWhichChild);

  virtual Accessible* GetChildAt(PRUint32 aIndex);
  virtual PRUint32 ChildCount() const;

  // SelectAccessible
  virtual bool IsSelect();
  virtual already_AddRefed<nsIArray> SelectedItems();
  virtual PRUint32 SelectedItemCount();
  virtual Accessible* GetSelectedItem(PRUint32 aIndex);
  virtual bool IsItemSelected(PRUint32 aIndex);
  virtual bool AddItemToSelection(PRUint32 aIndex);
  virtual bool RemoveItemFromSelection(PRUint32 aIndex);
  virtual bool SelectAll();
  virtual bool UnselectAll();

  // Widgets
  virtual bool IsWidget() const;
  virtual bool IsActiveWidget() const;
  virtual bool AreItemsOperable() const;
  virtual Accessible* CurrentItem();
  virtual void SetCurrentItem(Accessible* aItem);

  virtual Accessible* ContainerWidget() const;

  // XULTreeAccessible

  /**
   * Return tree item accessible at the givem row. If accessible doesn't exist
   * in the cache then create and cache it.
   *
   * @param aRow         [in] the given row index
   */
  Accessible* GetTreeItemAccessible(PRInt32 aRow);

  /**
   * Invalidates the number of cached treeitem accessibles.
   *
   * @param aRow    [in] row index the invalidation starts from
   * @param aCount  [in] the number of treeitem accessibles to invalidate,
   *                 the number sign specifies whether rows have been
   *                 inserted (plus) or removed (minus)
   */
  void InvalidateCache(PRInt32 aRow, PRInt32 aCount);

  /**
   * Fires name change events for invalidated area of tree.
   *
   * @param aStartRow  [in] row index invalidation starts from
   * @param aEndRow    [in] row index invalidation ends, -1 means last row index
   * @param aStartCol  [in] column index invalidation starts from
   * @param aEndCol    [in] column index invalidation ends, -1 mens last column
   *                    index
   */
  void TreeViewInvalidated(PRInt32 aStartRow, PRInt32 aEndRow,
                           PRInt32 aStartCol, PRInt32 aEndCol);

  /**
   * Invalidates children created for previous tree view.
   */
  void TreeViewChanged(nsITreeView* aView);

protected:
  /**
   * Creates tree item accessible for the given row index.
   */
  virtual already_AddRefed<Accessible> CreateTreeItemAccessible(PRInt32 aRow);

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsITreeView* mTreeView;
  AccessibleHashtable mAccessibleCache;
};

/**
 * Base class for tree item accessibles.
 */

#define XULTREEITEMBASEACCESSIBLE_IMPL_CID            \
{  /* 1ab79ae7-766a-443c-940b-b1e6b0831dfc */         \
  0x1ab79ae7,                                         \
  0x766a,                                             \
  0x443c,                                             \
  { 0x94, 0x0b, 0xb1, 0xe6, 0xb0, 0x83, 0x1d, 0xfc }  \
}

class XULTreeItemAccessibleBase : public AccessibleWrap
{
public:
  using Accessible::GetParent;

  XULTreeItemAccessibleBase(nsIContent* aContent, DocAccessible* aDoc,
                            Accessible* aParent, nsITreeBoxObject* aTree,
                            nsITreeView* aTreeView, PRInt32 aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessibleBase,
                                           AccessibleWrap)

  // nsIAccessible
  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);

  NS_IMETHOD SetSelected(bool aSelect);
  NS_IMETHOD TakeFocus();

  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessNode
  virtual void Shutdown();
  virtual bool IsPrimaryForNode() const;

  // Accessible
  virtual GroupPos GroupPosition();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;
  virtual PRInt32 IndexInParent() const;
  virtual Relation RelationByType(PRUint32 aType);
  virtual Accessible* FocusedChild();

  // ActionAccessible
  virtual PRUint8 ActionCount();

  // Widgets
  virtual Accessible* ContainerWidget() const;

  // XULTreeItemAccessibleBase
  NS_DECLARE_STATIC_IID_ACCESSOR(XULTREEITEMBASEACCESSIBLE_IMPL_CID)

  /**
   * Return row index associated with the accessible.
   */
  PRInt32 GetRowIndex() const { return mRow; }

  /**
   * Return cell accessible for the given column. If XUL tree accessible is not
   * accessible table then return null.
   */
  virtual Accessible* GetCellAccessible(nsITreeColumn* aColumn)
    { return nullptr; }

  /**
   * Proccess row invalidation. Used to fires name change events.
   */
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx) = 0;

protected:
  enum { eAction_Click = 0, eAction_Expand = 1 };

  // Accessible
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);
  virtual Accessible* GetSiblingAtOffset(PRInt32 aOffset,
                                         nsresult *aError = nullptr) const;

  // XULTreeItemAccessibleBase

  /**
   * Return true if the tree item accessible is expandable (contains subrows).
   */
  bool IsExpandable();

  /**
   * Return name for cell at the given column.
   */
  void GetCellName(nsITreeColumn* aColumn, nsAString& aName);

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsITreeView* mTreeView;
  PRInt32 mRow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(XULTreeItemAccessibleBase,
                              XULTREEITEMBASEACCESSIBLE_IMPL_CID)


/**
 * Accessible class for items for XUL tree.
 */
class XULTreeItemAccessible : public XULTreeItemAccessibleBase
{
public:
  XULTreeItemAccessible(nsIContent* aContent, DocAccessible* aDoc,
                        Accessible* aParent, nsITreeBoxObject* aTree,
                        nsITreeView* aTreeView, PRInt32 aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessible,
                                           XULTreeItemAccessibleBase)

  // nsAccessNode
  virtual void Init();
  virtual void Shutdown();

  // Accessible
  virtual ENameValueFlag Name(nsString& aName);
  virtual a11y::role NativeRole();

  // XULTreeItemAccessibleBase
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx);

protected:

  // Accessible
  virtual void CacheChildren();

  // XULTreeItemAccessible
  nsCOMPtr<nsITreeColumn> mColumn;
  nsString mCachedName;
};


/**
 * Accessible class for columns element of XUL tree.
 */
class XULTreeColumAccessible : public XULColumAccessible
{
public:
  XULTreeColumAccessible(nsIContent* aContent, DocAccessible* aDoc);

protected:

  // Accessible
  virtual Accessible* GetSiblingAtOffset(PRInt32 aOffset,
                                         nsresult *aError = nullptr) const;
};

} // namespace a11y
} // namespace mozilla

////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline mozilla::a11y::XULTreeAccessible*
Accessible::AsXULTree()
{
  return IsXULTree() ?
    static_cast<mozilla::a11y::XULTreeAccessible*>(this) : nullptr;
}

#endif
