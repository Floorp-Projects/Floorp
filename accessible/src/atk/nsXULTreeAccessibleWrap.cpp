/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Pete Zha (pete.zha@sun.com)
 *
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMElement.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsXULTreeAccessibleWrap.h"

// --------------------------------------------------------
// nsXULTreeAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeAccessibleWrap, nsXULTreeAccessible, nsIAccessibleTable)

nsXULTreeAccessibleWrap::nsXULTreeAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeAccessible(aDOMNode, aShell)
{
  mCaption = nsnull;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = mCaption;
  NS_IF_ADDREF(*aCaption);
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::SetCaption(nsIAccessible *aCaption)
{
  mCaption = aCaption;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSummary(nsAString &aSummary)
{
  aSummary = mSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::SetSummary(const nsAString &aSummary)
{
  mSummary = aSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  rv = nsAccessible::GetFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  return acc->GetChildCount(aColumns);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
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

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  return mTreeView->GetRowCount(aRows);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  // Row header not supported
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSelectedColumns(PRUint32 *aNumColumns, PRInt32 **aColumns)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aNumColumns);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRows;
  rv = GetSelectionCount(&selectedRows);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rows == selectedRows) {
    PRInt32 columns;
    rv = GetColumns(&columns);
    NS_ENSURE_SUCCESS(rv, rv);

    *aNumColumns = columns;
  } else {
    *aNumColumns = 0;
    return rv;
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumColumns) * sizeof(PRInt32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 index = 0; index < *aNumColumns; index++) {
    outArray[index] = index;
  }

  *aColumns = outArray;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aNumRows);

  nsresult rv = NS_OK;

  rv = GetSelectionCount((PRInt32 *)aNumRows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumRows) * sizeof(PRInt32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeView> view;
  rv = mTree->GetView(getter_AddRefs(view));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeSelection> selection;
  rv = view->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = GetRows(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelected;
  PRInt32 index, curr = 0;
  for (index = 0; index < rowCount; index++) {
    selection->IsSelected(index, &isSelected);
    if (isSelected) {
      outArray[curr++] = index;
    }
  }

  *aRows = outArray;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **_retval)
{
  NS_ENSURE_TRUE(mDOMNode && mTree, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessibleTable> header;
  rv = GetColumnHeader(getter_AddRefs(header));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessible> column;
  rv = header->CellRefAt(0, aColumn, getter_AddRefs(column));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> columnNode;
  rv = column->GetDOMNode(getter_AddRefs(columnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> columnElement(do_QueryInterface(columnNode, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeColumns> treeColumns;
  rv = mTree->GetColumns(getter_AddRefs(treeColumns));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeColumn> treeColumn;
  rv = treeColumns->GetColumnFor(columnElement, getter_AddRefs(treeColumn));
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCachedTreeitemAccessible(aRow, treeColumn, _retval);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aRow * columns + aColumn;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 treeCols;
  nsAccessible::GetChildCount(&treeCols);

  *_retval = (aIndex - treeCols) % columns;
  
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 treeCols;
  nsAccessible::GetChildCount(&treeCols);

  *_retval = (aIndex - treeCols) / columns;

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  // If all the row has been selected, then all the columns are selected.
  // Because we can't select a column alone.
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selectedRows;
  rv = GetSelectionCount(&selectedRows);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = rows == selectedRows;
  return rv;
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;

  nsCOMPtr<nsITreeView> view;
  rv = mTree->GetView(getter_AddRefs(view));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITreeSelection> selection;
  rv = view->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->IsSelected(aRow, _retval);
}

NS_IMETHODIMP nsXULTreeAccessibleWrap::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  return IsRowSelected(aRow, _retval);
}

// --------------------------------------------------------
// nsXULTreeAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeColumnsAccessibleWrap, nsXULTreeColumnsAccessible, nsIAccessibleTable)

nsXULTreeColumnsAccessibleWrap::nsXULTreeColumnsAccessibleWrap(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULTreeColumnsAccessible(aDOMNode, aShell)
{
  mCaption = nsnull;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = mCaption;
  NS_IF_ADDREF(*aCaption);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::SetCaption(nsIAccessible *aCaption)
{
  mCaption = aCaption;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetSummary(nsAString &aSummary)
{
  aSummary = mSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::SetSummary(const nsAString &aSummary)
{
  mSummary = aSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  return GetChildCount(aColumns);
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
