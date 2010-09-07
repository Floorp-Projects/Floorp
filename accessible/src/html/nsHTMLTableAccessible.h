/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsHTMLTableAccessible_H_
#define _nsHTMLTableAccessible_H_

#include "nsHyperTextAccessibleWrap.h"
#include "nsIAccessibleTable.h"

class nsITableLayout;
class nsITableCellLayout;

/**
 * HTML table cell accessible (html:td).
 */
class nsHTMLTableCellAccessible : public nsHyperTextAccessibleWrap,
                                  public nsIAccessibleTableCell
{
public:
  nsHTMLTableCellAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
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
  nsHTMLTableHeaderCellAccessible(nsIContent *aContent,
                                  nsIWeakReference *aShell);

  // nsAccessible
  virtual PRUint32 NativeRole();
};


/**
 * HTML table accessible (html:table).
 */

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

#define NS_TABLEACCESSIBLE_IMPL_CID                     \
{  /* 8d6d9c40-74bd-47ac-88dc-4a23516aa23d */           \
  0x8d6d9c40,                                           \
  0x74bd,                                               \
  0x47ac,                                               \
  { 0x88, 0xdc, 0x4a, 0x23, 0x51, 0x6a, 0xa2, 0x3d }    \
}

class nsHTMLTableAccessible : public nsAccessibleWrap,
                              public nsIAccessibleTable
{
public:
  nsHTMLTableAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLE
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TABLEACCESSIBLE_IMPL_CID)

  // nsIAccessible
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

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
                                            PRBool aIsOuter);

  /**
   * Return true if table has an element with the given tag name.
   *
   * @param  aTagName     [in] tag name of searched element
   * @param  aAllowEmpty  [in, optional] points if found element can be empty
   *                       or contain whitespace text only.
   */
  PRBool HasDescendant(const nsAString& aTagName, PRBool aAllowEmpty = PR_TRUE);

#ifdef SHOW_LAYOUT_HEURISTIC
  nsString mLayoutHeuristic;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHTMLTableAccessible,
                              NS_TABLEACCESSIBLE_IMPL_CID)


/**
 * HTML caption accessible (html:caption).
 */
class nsHTMLCaptionAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLCaptionAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
    nsHyperTextAccessibleWrap(aContent, aShell) { }

  // nsIAccessible
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsAccessible
  virtual PRUint32 NativeRole();
};

#endif  
