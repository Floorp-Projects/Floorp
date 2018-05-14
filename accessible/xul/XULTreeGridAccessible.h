/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeGridAccessible_h__
#define mozilla_a11y_XULTreeGridAccessible_h__

#include "XULTreeAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "xpcAccessibleTable.h"
#include "xpcAccessibleTableCell.h"

namespace mozilla {
namespace a11y {

class XULTreeGridCellAccessible;

/**
 * Represents accessible for XUL tree in the case when it has multiple columns.
 */
class XULTreeGridAccessible : public XULTreeAccessible,
                              public TableAccessible
{
public:
  XULTreeGridAccessible(nsIContent* aContent, DocAccessible* aDoc,
                        nsTreeBodyFrame* aTreeFrame) :
    XULTreeAccessible(aContent, aDoc, aTreeFrame)
    { mGenericTypes |= eTable; }

  // TableAccessible
  virtual uint32_t ColCount() const override;
  virtual uint32_t RowCount() override;
  virtual Accessible* CellAt(uint32_t aRowIndex, uint32_t aColumnIndex) override;
  virtual void ColDescription(uint32_t aColIdx, nsString& aDescription) override;
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

protected:
  virtual ~XULTreeGridAccessible();

  // XULTreeAccessible
  virtual already_AddRefed<Accessible>
    CreateTreeItemAccessible(int32_t aRow) const override;
};


/**
 * Represents accessible for XUL tree item in the case when XUL tree has
 * multiple columns.
 */
class XULTreeGridRowAccessible final : public XULTreeItemAccessibleBase
{
public:
  using Accessible::GetChildAt;

  XULTreeGridRowAccessible(nsIContent* aContent, DocAccessible* aDoc,
                           Accessible* aParent, nsITreeBoxObject* aTree,
                           nsITreeView* aTreeView, int32_t aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeGridRowAccessible,
                                           XULTreeItemAccessibleBase)

  // Accessible
  virtual void Shutdown() override;
  virtual a11y::role NativeRole() const override;
  virtual ENameValueFlag Name(nsString& aName) override;
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) override;

  virtual Accessible* GetChildAt(uint32_t aIndex) const override;
  virtual uint32_t ChildCount() const override;

  // XULTreeItemAccessibleBase
  XULTreeGridCellAccessible* GetCellAccessible(nsITreeColumn* aColumn)
    const final;
  virtual void RowInvalidated(int32_t aStartColIdx, int32_t aEndColIdx) override;

protected:
  virtual ~XULTreeGridRowAccessible();

  // XULTreeItemAccessibleBase
  mutable nsRefPtrHashtable<nsPtrHashKey<const void>, XULTreeGridCellAccessible>
    mAccessibleCache;
};


/**
 * Represents an accessible for XUL tree cell in the case when XUL tree has
 * multiple columns.
 */

class XULTreeGridCellAccessible : public LeafAccessible,
                                  public TableCellAccessible
{
public:

  XULTreeGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc,
                            XULTreeGridRowAccessible* aRowAcc,
                            nsITreeBoxObject* aTree, nsITreeView* aTreeView,
                            int32_t aRow, nsITreeColumn* aColumn);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeGridCellAccessible,
                                           LeafAccessible)

  // Accessible
  virtual void Shutdown() override;
  virtual TableCellAccessible* AsTableCell() override { return this; }
  virtual nsRect BoundsInAppUnits() const override;
  virtual nsIntRect BoundsInCSSPixels() const override;
  virtual ENameValueFlag Name(nsString& aName) override;
  virtual Accessible* FocusedChild() override;
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() override;
  virtual int32_t IndexInParent() const override;
  virtual Relation RelationByType(RelationType aType) override;
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() override;
  virtual uint64_t NativeInteractiveState() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // TableCellAccessible
  virtual TableAccessible* Table() const override;
  virtual uint32_t ColIdx() const override;
  virtual uint32_t RowIdx() const override;
  virtual void ColHeaderCells(nsTArray<Accessible*>* aHeaderCells) override;
  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells) override { }
  virtual bool Selected() override;

  /**
   * Fire name or state change event if the accessible text or value has been
   * changed.
   * @return true if name has changed
   */
  bool CellInvalidated();

protected:
  virtual ~XULTreeGridCellAccessible();

  // Accessible
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult* aError = nullptr) const override;
  virtual void DispatchClickEvent(nsIContent* aContent,
                                  uint32_t aActionIndex) const override;

  // XULTreeGridCellAccessible

  /**
   * Return true if value of cell can be modified.
   */
  bool IsEditable() const;

  enum { eAction_Click = 0 };

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsITreeView* mTreeView;

  int32_t mRow;
  nsCOMPtr<nsITreeColumn> mColumn;

  nsString mCachedTextEquiv;
};

} // namespace a11y
} // namespace mozilla

#endif
