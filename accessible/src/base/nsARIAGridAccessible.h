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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsARIAGridAccessible_h_
#define nsARIAGridAccessible_h_

#include "nsIAccessibleTable.h"

#include "nsHyperTextAccessibleWrap.h"

/**
 * Accessible for ARIA grid and treegrid.
 */
class nsARIAGridAccessible : public nsAccessibleWrap,
                             public nsIAccessibleTable
{
public:
  nsARIAGridAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_NSIACCESSIBLETABLE

protected:
  /**
   * Return true if the given row index is valid.
   */
  PRBool IsValidRow(PRInt32 aRow);

  /**
   * Retrn true if the given column index is valid.
   */
  PRBool IsValidColumn(PRInt32 aColumn);

  /**
   * Retrun true if given row and column indexes are valid.
   */
  PRBool IsValidRowNColumn(PRInt32 aRow, PRInt32 aColumn);

  /**
   * Return row accessible at the given row index.
   */
  already_AddRefed<nsIAccessible> GetRowAt(PRInt32 aRow);

  /**
   * Return cell accessible at the given column index in the row.
   */
  already_AddRefed<nsIAccessible> GetCellInRowAt(nsIAccessible *aRow,
                                                 PRInt32 aColumn);

  /**
   * Return next row accessible relative given row accessible or first row
   * accessible if it is null.
   *
   * @param  aRow  [in, optional] row accessible
   */
  already_AddRefed<nsIAccessible> GetNextRow(nsIAccessible *aRow = nsnull);

  /**
   * Return next cell accessible relative given cell accessible or first cell
   * in the given row accessible if given cell accessible is null.
   *
   * @param  aRow   [in] row accessible
   * @param  aCell  [in, optional] cell accessible
   */
  already_AddRefed<nsIAccessible> GetNextCellInRow(nsIAccessible *aRow,
                                                   nsIAccessible *aCell = nsnull);

  /**
   * Set aria-selected attribute value on DOM node of the given accessible.
   *
   * @param  aAccessible  [in] accessible
   * @param  aIsSelected  [in] new value of aria-selected attribute
   * @param  aNotify      [in, optional] specifies if DOM should be notified
   *                       about attribute change (used internally).
   */
  nsresult SetARIASelected(nsIAccessible *aAccessible, PRBool aIsSelected,
                           PRBool aNotify = PR_TRUE);

  /**
   * Helper method for GetSelectedColumnCount and GetSelectedColumns.
   */
  nsresult GetSelectedColumnsArray(PRUint32 *acolumnCount,
                                   PRInt32 **aColumns = nsnull);
};


/**
 * Accessible for ARIA gridcell and rowheader/columnheader.
 */
class nsARIAGridCellAccessible : public nsHyperTextAccessibleWrap,
                                 public nsIAccessibleTableCell
{
public:
  nsARIAGridCellAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessible
  virtual nsresult GetARIAState(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
};

#endif
