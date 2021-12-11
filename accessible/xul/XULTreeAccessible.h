/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeAccessible_h__
#define mozilla_a11y_XULTreeAccessible_h__

#include "nsITreeView.h"
#include "XULListboxAccessible.h"
#include "mozilla/dom/XULTreeElement.h"

class nsTreeBodyFrame;
class nsTreeColumn;

namespace mozilla {
namespace a11y {

class XULTreeGridCellAccessible;

/*
 * A class the represents the XUL Tree widget.
 */
const uint32_t kMaxTreeColumns = 100;
const uint32_t kDefaultTreeCacheLength = 128;

/**
 * LocalAccessible class for XUL tree element.
 */

class XULTreeAccessible : public AccessibleWrap {
 public:
  XULTreeAccessible(nsIContent* aContent, DocAccessible* aDoc,
                    nsTreeBodyFrame* aTreeframe);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeAccessible, LocalAccessible)

  // LocalAccessible
  virtual void Shutdown() override;
  virtual void Value(nsString& aValue) const override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual LocalAccessible* LocalChildAtPoint(
      int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) override;

  virtual LocalAccessible* LocalChildAt(uint32_t aIndex) const override;
  virtual uint32_t ChildCount() const override;
  virtual Relation RelationByType(RelationType aType) const override;

  // SelectAccessible
  virtual void SelectedItems(nsTArray<LocalAccessible*>* aItems) override;
  virtual uint32_t SelectedItemCount() override;
  virtual LocalAccessible* GetSelectedItem(uint32_t aIndex) override;
  virtual bool IsItemSelected(uint32_t aIndex) override;
  virtual bool AddItemToSelection(uint32_t aIndex) override;
  virtual bool RemoveItemFromSelection(uint32_t aIndex) override;
  virtual bool SelectAll() override;
  virtual bool UnselectAll() override;

  // Widgets
  virtual bool IsWidget() const override;
  virtual bool IsActiveWidget() const override;
  virtual bool AreItemsOperable() const override;
  virtual LocalAccessible* CurrentItem() const override;
  virtual void SetCurrentItem(const LocalAccessible* aItem) override;

  virtual LocalAccessible* ContainerWidget() const override;

  // XULTreeAccessible

  /**
   * Return tree item accessible at the givem row. If accessible doesn't exist
   * in the cache then create and cache it.
   *
   * @param aRow         [in] the given row index
   */
  LocalAccessible* GetTreeItemAccessible(int32_t aRow) const;

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
  virtual already_AddRefed<LocalAccessible> CreateTreeItemAccessible(
      int32_t aRow) const;

  RefPtr<dom::XULTreeElement> mTree;
  nsITreeView* mTreeView;
  mutable AccessibleHashtable mAccessibleCache;
};

/**
 * Base class for tree item accessibles.
 */

#define XULTREEITEMBASEACCESSIBLE_IMPL_CID           \
  { /* 1ab79ae7-766a-443c-940b-b1e6b0831dfc */       \
    0x1ab79ae7, 0x766a, 0x443c, {                    \
      0x94, 0x0b, 0xb1, 0xe6, 0xb0, 0x83, 0x1d, 0xfc \
    }                                                \
  }

class XULTreeItemAccessibleBase : public AccessibleWrap {
 public:
  XULTreeItemAccessibleBase(nsIContent* aContent, DocAccessible* aDoc,
                            LocalAccessible* aParent,
                            dom::XULTreeElement* aTree, nsITreeView* aTreeView,
                            int32_t aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessibleBase,
                                           AccessibleWrap)

  // LocalAccessible
  virtual void Shutdown() override;
  virtual nsRect BoundsInAppUnits() const override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsIntRect BoundsInCSSPixels() const override;
  virtual GroupPos GroupPosition() override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual int32_t IndexInParent() const override;
  virtual Relation RelationByType(RelationType aType) const override;
  virtual LocalAccessible* FocusedChild() override;
  virtual void SetSelected(bool aSelect) override;
  virtual void TakeFocus() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) const override;

  // Widgets
  virtual LocalAccessible* ContainerWidget() const override;

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
  virtual XULTreeGridCellAccessible* GetCellAccessible(
      nsTreeColumn* aColumn) const {
    return nullptr;
  }

  /**
   * Proccess row invalidation. Used to fires name change events.
   */
  virtual void RowInvalidated(int32_t aStartColIdx, int32_t aEndColIdx) = 0;

 protected:
  virtual ~XULTreeItemAccessibleBase();

  enum { eAction_Click = 0, eAction_Expand = 1 };

  // LocalAccessible
  MOZ_CAN_RUN_SCRIPT
  virtual void DispatchClickEvent(nsIContent* aContent,
                                  uint32_t aActionIndex) const override;
  virtual LocalAccessible* GetSiblingAtOffset(
      int32_t aOffset, nsresult* aError = nullptr) const override;

  // XULTreeItemAccessibleBase

  /**
   * Return true if the tree item accessible is expandable (contains subrows).
   */
  bool IsExpandable() const;

  /**
   * Return name for cell at the given column.
   */
  void GetCellName(nsTreeColumn* aColumn, nsAString& aName) const;

  RefPtr<dom::XULTreeElement> mTree;
  nsITreeView* mTreeView;
  int32_t mRow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(XULTreeItemAccessibleBase,
                              XULTREEITEMBASEACCESSIBLE_IMPL_CID)

/**
 * LocalAccessible class for items for XUL tree.
 */
class XULTreeItemAccessible : public XULTreeItemAccessibleBase {
 public:
  XULTreeItemAccessible(nsIContent* aContent, DocAccessible* aDoc,
                        LocalAccessible* aParent, dom::XULTreeElement* aTree,
                        nsITreeView* aTreeView, int32_t aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeItemAccessible,
                                           XULTreeItemAccessibleBase)

  // LocalAccessible
  virtual void Shutdown() override;
  virtual ENameValueFlag Name(nsString& aName) const override;
  virtual a11y::role NativeRole() const override;

  // XULTreeItemAccessibleBase
  virtual void RowInvalidated(int32_t aStartColIdx,
                              int32_t aEndColIdx) override;

 protected:
  virtual ~XULTreeItemAccessible();

  // XULTreeItemAccessible
  RefPtr<nsTreeColumn> mColumn;
  nsString mCachedName;
};

/**
 * LocalAccessible class for columns element of XUL tree.
 */
class XULTreeColumAccessible : public XULColumAccessible {
 public:
  XULTreeColumAccessible(nsIContent* aContent, DocAccessible* aDoc);

 protected:
  // LocalAccessible
  virtual LocalAccessible* GetSiblingAtOffset(
      int32_t aOffset, nsresult* aError = nullptr) const override;
};

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible downcasting method

inline XULTreeAccessible* LocalAccessible::AsXULTree() {
  return IsXULTree() ? static_cast<XULTreeAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
