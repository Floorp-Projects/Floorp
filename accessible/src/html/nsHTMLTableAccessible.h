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

#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleTable.h"

class nsITableLayout;

/**
 * HTML table cell accessible (html:td).
 */
class nsHTMLTableCellAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLTableCellAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);
  
  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

protected:
  already_AddRefed<nsIAccessibleTable> GetTableAccessible();
  nsresult GetCellIndexes(PRInt32& aRowIdx, PRInt32& aColIdx);

  /**
   * Search hint enum constants. Used by FindCellsForRelation method.
   */
  enum {
    // search for header cells, up-left direction search
    eHeadersForCell,
    // search for row header cell, right direction search
    eCellsForRowHeader,
    // search for column header cell, down direction search
    eCellsForColumnHeader
  };

  /**
   * Add found cells as relation targets.
   *
   * @param  aSearchHint    [in] enum constan defined above, defines an
   *                         algorithm to search cells
   * @param  aRelationType  [in] relation type
   * @param  aRelation      [in, out] relation object
   */
  nsresult FindCellsForRelation(PRInt32 aSearchHint, PRUint32 aRelationType,
                                nsIAccessibleRelation **aRelation);

  /**
   * Return the cell or header cell at the given row and column.
   *
   * @param  aTableAcc       [in] table accessible the search is prepared in
   * @param  aAnchorCell     [in] anchor cell, found cell should be different
   *                          from it
   * @param  aRowIdx         [in] row index
   * @param  aColIdx         [in] column index
   * @param  aLookForHeader  [in] flag specifies if found cell must be a header
   * @return                 found cell content
   */
  nsIContent* FindCell(nsHTMLTableAccessible *aTableAcc, nsIContent *aAnchorCell,
                       PRInt32 aRowIdx, PRInt32 aColIdx,
                       PRInt32 aLookForHeader);
};


/**
 * HTML table row/column header accessible (html:th or html:td@scope).
 */
class nsHTMLTableHeaderAccessible : public nsHTMLTableCellAccessible
{
public:
  nsHTMLTableHeaderAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);

  // nsIAccessible
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);
};


/**
 * HTML table accessible.
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
  nsHTMLTableAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLE
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TABLEACCESSIBLE_IMPL_CID)

  // nsIAccessible
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

  // nsHTMLTableAccessible

  /**
    * Returns true if the column index is in the valid column range.
    *
    * @param aColumn  The index to check for validity.
    */
  PRBool IsValidColumn(PRInt32 aColumn);

  /**
    * Returns true if the given index is in the valid row range.
    *
    * @param aRow  The index to check for validity.
    */
  PRBool IsValidRow(PRInt32 aRow);

  /**
   * Retun cell element at the given row and column index.
   */
  nsresult GetCellAt(PRInt32 aRowIndex, PRInt32 aColIndex,
                     nsIDOMElement* &aCell);
protected:

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

  virtual void CacheChildren();
  nsresult GetTableNode(nsIDOMNode **_retval);
  nsresult GetTableLayout(nsITableLayout **aLayoutObject);

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


class nsHTMLTableHeadAccessible : public nsHTMLTableAccessible
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsHTMLTableHeadAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell);

  /* nsIAccessibleTable */
  NS_IMETHOD GetCaption(nsIAccessible **aCaption);
  NS_IMETHOD GetSummary(nsAString &aSummary);
  NS_IMETHOD GetColumnHeader(nsIAccessibleTable **aColumnHeader);
  NS_IMETHOD GetRows(PRInt32 *aRows);

  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
};

class nsHTMLCaptionAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLCaptionAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell) :
    nsHyperTextAccessibleWrap(aDomNode, aShell) { }

  // nsIAccessible
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
};

#endif  
