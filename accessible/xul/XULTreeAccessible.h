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

class nsTreeBodyFrame;

namespace mozilla {
namespace a11y {

/*
 * A class the represents the XUL Tree widget.
 */
const uint32_t kMaxTreeColumns = 100;
const uint32_t kDefaultTreeCacheLength = 128;

/**
 * Accessible class for XUL tree element.
 */

class XULTreeAccessible : public AccessibleWrap
{
public:
  XULTreeAccessible(nsIContent* aContent, DocAccessible* aDoc,
                    nsTreeBodyFrame* aTreeframe);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeAccessible, Accessible)

  // Accessible
  virtual void Shutdown();
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);

  virtual Accessible* GetChildAt(uint32_t aIndex) const MOZ_OVERRIDE;
  virtual uint32_t ChildCount() const MOZ_OVERRIDE;
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;

  // SelectAccessible
  virtual void SelectedItems(nsTArray<Accessible*>* aItems) MOZ_OVERRIDE;
  virtual uint32_t SelectedItemCount();
  virtual Accessible* GetSelectedItem(uint32_t aIndex);
  virtual bool IsItemSelected(uint32_t aIndex);
  virtual bool AddItemToSelection(uint32_t aIndex);
  virtual bool RemoveItemFromSelection(uint32_t aIndex);
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
  Accessible* GetTreeItemAccessible(int32_t aRow) const;

  /**
   * Invalidates the number of cached treeitem accessibles.
   *
   * @param aRow    [in] row index the invalidation starts from
   * @param aCount  [in] the number of treeitem accessibles to invalidate,
   *                 the number sign specifies whether rows have been
   *                 inserted (plus) or removed (minus)
   */
  void InvalidateCache(int32_t aRow, int32_t aCount);

  /**
   * Fires name change events for invalidated area of tree.
   *
   * @param aStartRow  [in] row index invalidation starts from
   * @param aEndRow    [in] row index invalidation ends, -1 means last row index
   * @param aStartCol  [in] column index invalidation starts from
   * @param aEndCol    [in] column index invalidation ends, -1 mens last column
   *                    index
   */
  void TreeViewInvalidated(int32_t aStartRow, int32_t aEndRow,
                           int32_t aStartCol, int32_t aEndCol);

  /**
   * Invalidates children created for previous tree view.
   */
  void TreeViewChanged(nsITreeView* aView);

protected:
  virtual ~XULTreeAccessible();

  /**
   * Creates tree item accessible for the given row index.
   */
  virtual already_AddRefed<Accessible>
    CreateTreeItemAccessible(int32_t aRow) const;

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsITreeView* mTreeView;
  mutable AccessibleHashtable mAccessibleCache;
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
  XULTreeItemAccessibleBase(nsIContent* aContent, DocAccessible* aDoc,
                            Accessible* aParent, nsITreeBoxObject* aTree,
                            nsITreeView* aTreeView, int32_t aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessibleBase,
                                           AccessibleWrap)

  // Accessible
  virtual void Shutdown() MOZ_OVERRIDE;
  virtual nsIntRect Bounds() const MOZ_OVERRIDE;
  virtual GroupPos GroupPosition() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;
  virtual uint64_t NativeInteractiveState() const MOZ_OVERRIDE;
  virtual int32_t IndexInParent() const MOZ_OVERRIDE;
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;
  virtual Accessible* FocusedChild() MOZ_OVERRIDE;
  virtual void SetSelected(bool aSelect) MOZ_OVERRIDE;
  virtual void TakeFocus() MOZ_OVERRIDE;

  // ActionAccessible
  virtual uint8_t ActionCount() MOZ_OVERRIDE;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) MOZ_OVERRIDE;
  virtual bool DoAction(uint8_t aIndex) MOZ_OVERRIDE;

  // Widgets
  virtual Accessible* ContainerWidget() const;

  // XULTreeItemAccessibleBase
  NS_DECLARE_STATIC_IID_ACCESSOR(XULTREEITEMBASEACCESSIBLE_IMPL_CID)

  /**
   * Return row index associated with the accessible.
   */
  int32_t GetRowIndex() const { return mRow; }

  /**
   * Return cell accessible for the given column. If XUL tree accessible is not
   * accessible table then return null.
   */
  virtual Accessible* GetCellAccessible(nsITreeColumn* aColumn) const
    { return nullptr; }

  /**
   * Proccess row invalidation. Used to fires name change events.
   */
  virtual void RowInvalidated(int32_t aStartColIdx, int32_t aEndColIdx) = 0;

protected:
  virtual ~XULTreeItemAccessibleBase();

  enum { eAction_Click = 0, eAction_Expand = 1 };

  // Accessible
  virtual void DispatchClickEvent(nsIContent *aContent, uint32_t aActionIndex);
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
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
  int32_t mRow;
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
                        nsITreeView* aTreeView, int32_t aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessible,
                                           XULTreeItemAccessibleBase)

  // Accessible
  virtual void Shutdown();
  virtual ENameValueFlag Name(nsString& aName);
  virtual a11y::role NativeRole() MOZ_OVERRIDE;

  // XULTreeItemAccessibleBase
  virtual void RowInvalidated(int32_t aStartColIdx, int32_t aEndColIdx);

protected:
  virtual ~XULTreeItemAccessible();

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
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult *aError = nullptr) const;
};


////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline XULTreeAccessible*
Accessible::AsXULTree()
{
  return IsXULTree() ? static_cast<XULTreeAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
