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

#include "nsHTMLTableAccessibleWrap.h"
#include "nsIAccessibilityService.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsITableLayout.h"

// --------------------------------------------------------
// nsHTMLTableCellAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED2(nsHTMLTableCellAccessibleWrap, nsHTMLTableCellAccessible, nsIAccessibleText, nsIAccessibleHyperText)

nsHTMLTableCellAccessibleWrap::nsHTMLTableCellAccessibleWrap(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHTMLTableCellAccessible(aDomNode, aShell), nsAccessibleHyperText(aDomNode, aShell)
{
}

NS_IMETHODIMP nsHTMLTableCellAccessibleWrap::Shutdown()
{
  nsAccessibleHyperText::Shutdown();
  return nsHTMLTableCellAccessible::Shutdown();
}

// --------------------------------------------------------
// nsHTMLTableAccessibleWrap Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLTableAccessibleWrap, nsHTMLTableAccessible, nsIAccessibleTable)

nsHTMLTableAccessibleWrap::nsHTMLTableAccessibleWrap(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHTMLTableAccessible(aDomNode, aShell)
{
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = nsnull;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLTableCaptionElement> caption;
  rv = table->GetCaption(getter_AddRefs(caption));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> captionNode(do_QueryInterface(caption));
  NS_ENSURE_TRUE(captionNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibilityService>
    accService(do_GetService("@mozilla.org/accessibilityService;1"));
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  accService->GetCachedAccessible(captionNode, mWeakShell, aCaption);
  if (*aCaption)
    return NS_OK;

  return accService->CreateHTMLTableCaptionAccessible(captionNode, aCaption);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::SetCaption(nsIAccessible *aCaption)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aCaption));
  NS_ASSERTION(accessNode, "Unable to QI to nsIAccessNode");
  nsCOMPtr<nsIDOMNode> domNode;
  rv = accessNode->GetDOMNode(getter_AddRefs(domNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> newDOMNode;
  rv = domNode->CloneNode(PR_TRUE, getter_AddRefs(newDOMNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLTableCaptionElement>
    captionElement(do_QueryInterface(newDOMNode));
  NS_ENSURE_TRUE(captionElement, NS_ERROR_FAILURE);

  return table->SetCaption(captionElement);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetSummary(nsAString &aSummary)
{
  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  return table->GetSummary(aSummary);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::SetSummary(const nsAString &aSummary)
{
  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  return table->SetSummary(aSummary);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetColumns(PRInt32 *aColumns)
{
  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rows;
  return tableLayout->GetTableSize(rows, *aColumns);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(table, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;
  rv = table->GetTHead(getter_AddRefs(section));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibilityService>
    accService(do_GetService("@mozilla.org/accessibilityService;1"));
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessible> accHead;
  nsCOMPtr<nsIDOMNode> sectionNode(do_QueryInterface(section));
  if (sectionNode) {
    rv = accService->GetCachedAccessible(sectionNode, mWeakShell,
                                    getter_AddRefs(accHead));
  }

  if (!accHead) {
    rv = accService->CreateHTMLTableHeadAccessible(section,
                                                   getter_AddRefs(accHead));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibleTable> accTableHead(do_QueryInterface(accHead));
  NS_ENSURE_TRUE(accTableHead, NS_ERROR_FAILURE);

  *aColumnHeader = accTableHead;
  NS_IF_ADDREF(*aColumnHeader);

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetRows(PRInt32 *aRows)
{
  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columns;
  return tableLayout->GetTableSize(*aRows, columns);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  // Can not implement because there is no row header in html table
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetSelectedColumns(PRUint32 *aNumColumns,
                                          PRInt32 **aColumns)
{
  nsresult rv = NS_OK;

  PRInt32 columnCount;
  rv = GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool *states = new PRBool[columnCount];
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  *aNumColumns = 0;
  PRInt32 index;
  for (index = 0; index < columnCount; index++) {
    rv = IsColumnSelected(index, &states[index]);
    NS_ENSURE_SUCCESS(rv, rv);

    if (states[index]) {
      (*aNumColumns)++;
    }
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumColumns) * sizeof(PRInt32));
  if (!outArray) {
    delete []states;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 curr = 0;
  for (index = 0; index < columnCount; index++) {
    if (states[index]) {
      outArray[curr++] = index;
    }
  }

  delete []states;
  *aColumns = outArray;
  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
{
  nsresult rv = NS_OK;

  PRInt32 rowCount;
  rv = GetRows(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool *states = new PRBool[rowCount];
  NS_ENSURE_TRUE(states, NS_ERROR_OUT_OF_MEMORY);

  *aNumRows = 0;
  PRInt32 index;
  for (index = 0; index < rowCount; index++) {
    rv = IsRowSelected(index, &states[index]);
    NS_ENSURE_SUCCESS(rv, rv);

    if (states[index]) {
      (*aNumRows)++;
    }
  }

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumRows) * sizeof(PRInt32));
  if (!outArray) {
    delete []states;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 curr = 0;
  for (index = 0; index < rowCount; index++) {
    if (states[index]) {
      outArray[curr++] = index;
    }
  }

  delete []states;
  *aRows = outArray;
  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::CellRefAt(PRInt32 aRow, PRInt32 aColumn,
                                 nsIAccessible **aTableCellAccessible)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> cellElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(cellElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibilityService>
    accService(do_GetService("@mozilla.org/accessibilityService;1"));
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  return accService->GetAccessibleInWeakShell(cellElement, mWeakShell,
                                              aTableCellAccessible);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetIndexAt(PRInt32 aRow, PRInt32 aColumn,
                                  PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aRow * columns + aColumn;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aIndex % columns;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aIndex / columns;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                         PRInt32 *_retval)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> domElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLTableCellElement> cell(do_QueryInterface(domElement));
  NS_ENSURE_TRUE(cell, NS_ERROR_FAILURE);

  return cell->GetColSpan(_retval);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn,
                                      PRInt32 *_retval)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMElement> domElement;
  rv = GetCellAt(aRow, aColumn, *getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLTableCellElement> cell(do_QueryInterface(domElement));
  NS_ENSURE_TRUE(cell, NS_ERROR_FAILURE);

  return cell->GetRowSpan(_retval);
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetColumnDescription(PRInt32 aColumn, nsAString &_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::GetRowDescription(PRInt32 aRow, nsAString &_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 rows;
  rv = GetRows(&rows);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 index = 0; index < rows; index++) {
    rv = IsCellSelected(index, aColumn, _retval);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!*_retval) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 index = 0; index < columns; index++) {
    rv = IsCellSelected(aRow, index, _retval);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!*_retval) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableAccessibleWrap::IsCellSelected(PRInt32 aRow, PRInt32 aColumn,
                                      PRBool *_retval)
{
  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> domElement;
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;

  return tableLayout->GetCellDataAt(aRow, aColumn,
                                    *getter_AddRefs(domElement),
                                    startRowIndex, startColIndex, rowSpan,
                                    colSpan, actualRowSpan, actualColSpan,
                                    *_retval);
}

nsresult
nsHTMLTableAccessibleWrap::GetTableNode(nsIDOMNode **_retval)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableElement> table(do_QueryInterface(mDOMNode));
  if (table) {
    *_retval = table;
    NS_IF_ADDREF(*_retval);
    return rv;
  }

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section(do_QueryInterface(mDOMNode));
  if (section) {
    nsCOMPtr<nsIDOMNode> parent;
    rv = section->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    *_retval = parent;
    NS_IF_ADDREF(*_retval);
    return rv;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsHTMLTableAccessibleWrap::GetTableLayout(nsITableLayout **aLayoutObject)
{
  *aLayoutObject = nsnull;

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMNode> tableNode;
  rv = GetTableNode(getter_AddRefs(tableNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content(do_QueryInterface(tableNode));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIPresShell *presShell = content->GetDocument()->GetShellAt(0);

  nsCOMPtr<nsISupports> layoutObject;
  rv = presShell->GetLayoutObjectFor(content, getter_AddRefs(layoutObject));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(layoutObject, aLayoutObject);
}

nsresult
nsHTMLTableAccessibleWrap::GetCellAt(PRInt32        aRowIndex,
                                 PRInt32        aColIndex,
                                 nsIDOMElement* &aCell)
{
  PRInt32 startRowIndex = 0, startColIndex = 0,
          rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected;

  nsITableLayout *tableLayout;
  nsresult rv = GetTableLayout(&tableLayout);
  NS_ENSURE_SUCCESS(rv, rv);

  return tableLayout->GetCellDataAt(aRowIndex, aColIndex, aCell,
                                    startRowIndex, startColIndex,
                                    rowSpan, colSpan,
                                    actualRowSpan, actualColSpan,
                                    isSelected);
}

// --------------------------------------------------------
// nsHTMLTableHeadAccessible Accessible
// --------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTableHeadAccessible, nsHTMLTableAccessibleWrap)

nsHTMLTableHeadAccessible::nsHTMLTableHeadAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell):
nsHTMLTableAccessibleWrap(aDomNode, aShell)
{
  Init(); // Make sure this generated accessible of the table is cached
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = ROLE_COLUMNHEADER;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetCaption(nsIAccessible **aCaption)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::SetCaption(nsIAccessible *aCaption)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetSummary(nsAString &aSummary)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::SetSummary(const nsAString &aSummary)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLTableHeadAccessible::GetRows(PRInt32 *aRows)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMHTMLTableSectionElement> head(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(head, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  rv = head->GetRows(getter_AddRefs(rows));
  NS_ENSURE_SUCCESS(rv, rv);

  return rows->GetLength((PRUint32 *)aRows);
}
