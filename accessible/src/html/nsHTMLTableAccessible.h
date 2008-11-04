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

class nsHTMLTableCellAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLTableCellAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
};

class nsITableLayout;

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

class nsHTMLTableAccessible : public nsAccessibleWrap,
                              public nsIAccessibleTable
{
public:
  nsHTMLTableAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLE

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aResult); 
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD GetAccessibleRelated(PRUint32 aRelationType, nsIAccessible **aRelated);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
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

protected:

  /**
   * Selects or unselects row or column.
   *
   * @param aIndex - index of row or column to be selected
   * @param aTarget - indicates what should be selected, either row or column
   *                  (see nsISelectionPrivate)
   * @param aDoSelect - indicates whether row or column should selected or
   *                    unselected
   */
  nsresult SelectRowOrColumn(PRInt32 aIndex, PRUint32 aTarget, PRBool aDoSelect);

  /**
   * Selects or unselects the cell.
   *
   * @param aSelection - the selection of document
   * @param aDocument - the document that contains the cell
   * @param aCellElement - the cell of table
   * @param aDoSelect - indicates whether cell should be selected or unselected
   */
  nsresult SelectCell(nsISelection *aSelection, nsIDocument *aDocument,
                      nsIDOMElement *aCellElement, PRBool aDoSelect);

  virtual void CacheChildren();
  nsresult GetTableNode(nsIDOMNode **_retval);
  nsresult GetTableLayout(nsITableLayout **aLayoutObject);
  nsresult GetCellAt(PRInt32        aRowIndex,
                     PRInt32        aColIndex,
                     nsIDOMElement* &aCell);
  PRBool HasDescendant(char *aTagName, PRBool aAllowEmpty = PR_TRUE);
#ifdef SHOW_LAYOUT_HEURISTIC
  nsAutoString mLayoutHeuristic;
#endif
};

class nsHTMLTableHeadAccessible : public nsHTMLTableAccessible
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsHTMLTableHeadAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell);

  /* nsIAccessible */
  NS_IMETHOD GetRole(PRUint32 *aResult);

  /* nsIAccessibleTable */
  NS_IMETHOD GetCaption(nsIAccessible **aCaption);
  NS_IMETHOD GetSummary(nsAString &aSummary);
  NS_IMETHOD GetColumnHeader(nsIAccessibleTable **aColumnHeader);
  NS_IMETHOD GetRows(PRInt32 *aRows);
};

class nsHTMLCaptionAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsHTMLCaptionAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell) :
    nsHyperTextAccessibleWrap(aDomNode, aShell) { }

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole)
    { *aRole = nsIAccessibleRole::ROLE_CAPTION; return NS_OK; }
  NS_IMETHOD GetAccessibleRelated(PRUint32 aRelationType, nsIAccessible **aRelated);
};

#endif  
