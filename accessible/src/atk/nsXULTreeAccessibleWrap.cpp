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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Zha (pete.zha@sun.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIDOMElement.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsXULTreeAccessibleWrap.h"

// --------------------------------------------------------
// nsXULTreeAccessibleWrap Accessible
// --------------------------------------------------------

nsXULTreeGridAccessibleWrap::
  nsXULTreeGridAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
  nsXULTreeGridAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP
nsXULTreeGridAccessibleWrap::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  nsAccessible::GetFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleTable> accTable(do_QueryInterface(acc, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnHeader = accTable;
  NS_IF_ADDREF(*aColumnHeader);

  return rv;
}

NS_IMETHODIMP
nsXULTreeGridAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  nsCOMPtr<nsIAccessibleTable> columnHeader;
  nsresult rv = GetColumnHeader(getter_AddRefs(columnHeader));
  if (NS_SUCCEEDED(rv) && columnHeader) {
    return columnHeader->GetColumnDescription(aColumn, _retval);
  }
  return NS_ERROR_FAILURE;
}

// --------------------------------------------------------
// nsXULTreeColumnsAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeColumnsAccessibleWrap, nsXULTreeColumnsAccessible, nsIAccessibleTable)

nsXULTreeColumnsAccessibleWrap::nsXULTreeColumnsAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeColumnsAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSummary(nsAString &aSummary)
{
  aSummary.Truncate();
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  nsresult rv = GetChildCount(aColumns);
  return *aColumns > 0 ? rv : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnHeader(nsIAccessibleTable * *aColumnHeader)
{
  // Column header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_ARG_POINTER(aRows);

  *aRows = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowHeader(nsIAccessibleTable * *aRowHeader)
{
  // Row header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedCellsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedColumnsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedRowsCount(PRUint32* aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::GetSelectedCells(PRUint32 *aNumCells,
                                                 PRInt32 **aCells)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSelectedColumns(PRUint32 *columnsSize, PRInt32 **columns)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSelectedRows(PRUint32 *rowsSize, PRInt32 **rows)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **_retval)
{
  nsCOMPtr<nsIAccessible> next, temp;
  GetFirstChild(getter_AddRefs(next));
  NS_ENSURE_TRUE(next, NS_ERROR_FAILURE);

  for (PRInt32 col = 0; col < aColumn; col++) {
    next->GetNextSibling(getter_AddRefs(temp));
    NS_ENSURE_TRUE(temp, NS_ERROR_FAILURE);

    next = temp;
  }

  *_retval = next;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aColumn;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aIndex;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  nsCOMPtr<nsIAccessible> column;  
  nsresult rv = CellRefAt(0, aColumn, getter_AddRefs(column));
  if (NS_SUCCEEDED(rv) && column) {
    return column->GetName(_retval);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::SelectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::SelectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::UnselectRow(PRInt32 aRow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULTreeColumnsAccessibleWrap::UnselectColumn(PRInt32 aColumn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::IsProbablyForLayout(PRBool *aIsProbablyForLayout)
{
  *aIsProbablyForLayout = PR_FALSE;
  return NS_OK;
}
