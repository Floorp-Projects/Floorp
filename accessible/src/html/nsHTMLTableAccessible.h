/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsHTMLTableAccessible_H_
#define _nsHTMLTableAccessible_H_

#include "nsHyperTextAccessibleWrap.h"
#include "nsIAccessibleTable.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"

class nsITableLayout;
class nsITableCellLayout;

/**
 * HTML table cell accessible (html:td).
 */
class nsHTMLTableCellAccessible : public nsHyperTextAccessibleWrap,
                                  public nsIAccessibleTableCell
{
public:
  nsHTMLTableCellAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
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
class nsHTMLTableHeaderCellAccessible : public nsHTMLTableCellAccessible
{
public:
  nsHTMLTableHeaderCellAccessible(nsIContent* aContent,
                                  nsDocAccessible* aDoc);

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
};


/**
 * HTML table accessible (html:table).
 */

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

class nsHTMLTableAccessible : public nsAccessibleWrap,
                              public xpcAccessibleTable,
                              public nsIAccessibleTable,
                              public mozilla::a11y::TableAccessible
{
public:
  nsHTMLTableAccessible(nsIContent* aContent, nsDocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible Table
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // TableAccessible
  virtual nsAccessible* Caption();
  virtual void Summary(nsString& aSummary);
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual nsAccessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual PRInt32 CellIndexAt(PRUint32 aRowIdx, PRUint32 aColIdx);
  virtual void UnselectCol(PRUint32 aColIdx);
  virtual void UnselectRow(PRUint32 aRowIdx);
  virtual bool IsProbablyLayoutTable();

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual mozilla::a11y::TableAccessible* AsTable() { return this; }
  virtual void Description(nsString& aDescription);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual Relation RelationByType(PRUint32 aRelationType);

  // nsHTMLTableAccessible

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

  // nsAccessible
  virtual void CacheChildren();

  // nsHTMLTableAccessible

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
class nsHTMLCaptionAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLCaptionAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
    nsHyperTextAccessibleWrap(aContent, aDoc) { }
  virtual ~nsHTMLCaptionAccessible() { }

  // nsIAccessible

  // nsAccessible
  virtual mozilla::a11y::role NativeRole();
  virtual Relation RelationByType(PRUint32 aRelationType);
};

#endif  
