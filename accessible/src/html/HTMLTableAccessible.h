/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLTableAccessible_h__
#define mozilla_a11y_HTMLTableAccessible_h__

#include "HyperTextAccessibleWrap.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"

class nsITableLayout;
class nsITableCellLayout;

namespace mozilla {
namespace a11y {

/**
 * HTML table cell accessible (html:td).
 */
class HTMLTableCellAccessible : public HyperTextAccessibleWrap,
                                public nsIAccessibleTableCell
{
public:
  HTMLTableCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // Accessible
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual PRUint64 NativeInteractiveState() const;
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

protected:
  /**
   * Return host table accessible.
   */
  already_AddRefed<nsIAccessibleTable> GetTableAccessible();

  /**
   * Return nsITableCellLayout of the table cell frame.
   */
  nsITableCellLayout* GetCellLayout();

  /**
   * Return row and column indices of the cell.
   */
  nsresult GetCellIndexes(PRInt32& aRowIdx, PRInt32& aColIdx);

  /**
   * Return an array of row or column header cells.
   */
  nsresult GetHeaderCells(PRInt32 aRowOrColumnHeaderCell,
                          nsIArray **aHeaderCells);
};


/**
 * HTML table row/column header accessible (html:th or html:td@scope).
 */
class HTMLTableHeaderCellAccessible : public HTMLTableCellAccessible
{
public:
  HTMLTableHeaderCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole();
};


/**
 * HTML table accessible (html:table).
 */

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

class HTMLTableAccessible : public AccessibleWrap,
                            public xpcAccessibleTable,
                            public nsIAccessibleTable,
                            public TableAccessible
{
public:
  HTMLTableAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible Table
  NS_FORWARD_NSIACCESSIBLETABLE(xpcAccessibleTable::)

  // TableAccessible
  virtual Accessible* Caption();
  virtual void Summary(nsString& aSummary);
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual Accessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual PRInt32 CellIndexAt(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual PRInt32 ColIndexAt(PRUint32 aCellIdx);
  virtual PRInt32 RowIndexAt(PRUint32 aCellIdx);
  virtual void RowAndColIndicesAt(PRUint32 aCellIdx, PRInt32* aRowIdx,
                                  PRInt32* aColIdx);
  virtual PRUint32 ColExtentAt(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual PRUint32 RowExtentAt(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual bool IsColSelected(PRUint32 aColIdx);
  virtual bool IsRowSelected(PRUint32 aRowIdx);
  virtual bool IsCellSelected(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual PRUint32 SelectedCellCount();
  virtual PRUint32 SelectedColCount();
  virtual PRUint32 SelectedRowCount();
  virtual void SelectedCells(nsTArray<Accessible*>* aCells);
  virtual void SelectedCellIndices(nsTArray<PRUint32>* aCells);
  virtual void SelectedColIndices(nsTArray<PRUint32>* aCols);
  virtual void SelectedRowIndices(nsTArray<PRUint32>* aRows);
  virtual void SelectCol(PRUint32 aColIdx);
  virtual void SelectRow(PRUint32 aRowIdx);
  virtual void UnselectCol(PRUint32 aColIdx);
  virtual void UnselectRow(PRUint32 aRowIdx);
  virtual bool IsProbablyLayoutTable();

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual TableAccessible* AsTable() { return this; }
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual Relation RelationByType(PRUint32 aRelationType);

  // HTMLTableAccessible

  /**
   * Retun cell element at the given row and column index.
   */
  nsresult GetCellAt(PRInt32 aRowIndex, PRInt32 aColIndex,
                     nsIDOMElement* &aCell);

  /**
   * Return nsITableLayout for the frame of the accessible table.
   */
  nsITableLayout* GetTableLayout();

protected:

  // Accessible
  virtual void CacheChildren();

  // HTMLTableAccessible

  /**
   * Add row or column to selection.
   *
   * @param aIndex   [in] index of row or column to be selected
   * @param aTarget  [in] indicates what should be selected, either row or column
   *                  (see nsISelectionPrivate)
   */
  nsresult AddRowOrColumnToSelection(PRInt32 aIndex, PRUint32 aTarget);

  /**
   * Removes rows or columns at the given index or outside it from selection.
   *
   * @param  aIndex    [in] row or column index
   * @param  aTarget   [in] indicates whether row or column should unselected
   * @param  aIsOuter  [in] indicates whether all rows or column excepting
   *                    the given one should be unselected or the given one
   *                    should be unselected only
   */
  nsresult RemoveRowsOrColumnsFromSelection(PRInt32 aIndex,
                                            PRUint32 aTarget,
                                            bool aIsOuter);

  /**
   * Return true if table has an element with the given tag name.
   *
   * @param  aTagName     [in] tag name of searched element
   * @param  aAllowEmpty  [in, optional] points if found element can be empty
   *                       or contain whitespace text only.
   */
  bool HasDescendant(const nsAString& aTagName, bool aAllowEmpty = true);

#ifdef SHOW_LAYOUT_HEURISTIC
  nsString mLayoutHeuristic;
#endif
};

/**
 * HTML caption accessible (html:caption).
 */
class HTMLCaptionAccessible : public HyperTextAccessibleWrap
{
public:
  HTMLCaptionAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessibleWrap(aContent, aDoc) { }
  virtual ~HTMLCaptionAccessible() { }

  // nsIAccessible

  // Accessible
  virtual a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aRelationType);
};

} // namespace a11y
} // namespace mozilla

#endif
