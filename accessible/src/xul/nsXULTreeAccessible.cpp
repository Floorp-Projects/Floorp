/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Author: Kyle Yuan (kyle.yuan@sun.com)
 *
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

#include "nsCOMPtr.h"
#include "nsIAccessibilityService.h"
#include "nsIBoxObject.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULElement.h"
#include "nsITreeSelection.h"
#include "nsITreeView.h"
#include "nsXULAtoms.h"
#include "nsXULTreeAccessible.h"

// ---------- nsXULTreeAccessible ----------

nsXULTreeAccessible::nsXULTreeAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULSelectableAccessible(aDOMNode, aShell)
{
  GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  mCaption = nsnull;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeAccessible, nsXULSelectableAccessible, nsIAccessibleTable)

NS_IMETHODIMP nsXULTreeAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class                                           
  nsAccessible::GetAccState(_retval);                                           

  // see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    // the default selection type is multiple
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (selType.IsEmpty() || !selType.Equals(NS_LITERAL_STRING("single")))
      *_retval |= STATE_MULTISELECTABLE;
  }

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

// The value is the first selected child
NS_IMETHODIMP nsXULTreeAccessible::GetAccValue(nsAString& _retval)
{
  _retval.Truncate(0);

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (! selection)
    return NS_ERROR_FAILURE;

  PRInt32 currentIndex;
  nsCOMPtr<nsIDOMElement> selectItem;
  selection->GetCurrentIndex(&currentIndex);
  if (currentIndex >= 0) {
    nsAutoString colID;
    PRInt32 keyColumn;
    mTree->GetKeyColumnIndex(&keyColumn);
    mTree->GetColumnID(keyColumn, colID);
    return mTreeView->GetCellText(currentIndex, colID.get(), _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_OUTLINE;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetAccFirstChild(nsIAccessible **aAccFirstChild)
{
  nsAccessible::GetAccFirstChild(aAccFirstChild);

  // in normal case, tree's first child should be treecols, if it is not here, 
  //   use the first row as tree's first child
  if (*aAccFirstChild == nsnull) {
    NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

    PRInt32 rowCount;
    mTreeView->GetRowCount(&rowCount);
    if (rowCount > 0) {
      *aAccFirstChild = new nsXULTreeitemAccessible(this, mDOMNode, mPresShell, 0);
      if (! *aAccFirstChild)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(*aAccFirstChild);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetAccLastChild(nsIAccessible **aAccLastChild)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);
  if (rowCount > 0) {
    *aAccLastChild = new nsXULTreeitemAccessible(this, mDOMNode, mPresShell, rowCount - 1);
    if (! *aAccLastChild)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aAccLastChild);
  }
  else // if there is not any rows, use treecols as tree's last child
    nsAccessible::GetAccLastChild(aAccLastChild);

  return NS_OK;
}

// tree's children count is row count + treecols count
NS_IMETHODIMP nsXULTreeAccessible::GetAccChildCount(PRInt32 *aAccChildCount)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsAccessible::GetAccChildCount(aAccChildCount);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);
  *aAccChildCount += rowCount;

  return NS_OK;
}

// Ask treeselection to get all selected children
NS_IMETHODIMP nsXULTreeAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> selectedAccessibles;
  NS_NewISupportsArray(getter_AddRefs(selectedAccessibles));
  if (!selectedAccessibles)
    return NS_ERROR_OUT_OF_MEMORY;

  PRInt32 rowIndex, rowCount;
  PRBool isSelected;
  mTreeView->GetRowCount(&rowCount);
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    selection->IsSelected(rowIndex, &isSelected);
    if (isSelected) {
      nsCOMPtr<nsIAccessible> tempAccess;
      tempAccess = new nsXULTreeitemAccessible(this, mDOMNode, mPresShell, rowIndex);
      if (!tempAccess)
        return NS_ERROR_OUT_OF_MEMORY;
      selectedAccessibles->AppendElement(tempAccess);
    }
  }

  PRUint32 length;
  selectedAccessibles->Count(&length);
  if (length != 0) {
    *_retval = selectedAccessibles;
    NS_IF_ADDREF(*_retval);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->GetCount(aSelectionCount);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection) {
    selection->IsSelected(aIndex, aSelState);
    if ((!(*aSelState) && eSelection_Add == aMethod) || 
        ((*aSelState) && eSelection_Remove == aMethod))
      selection->ToggleSelect(aIndex);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::AddSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsXULTreeAccessible::RemoveSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Remove, &isSelected);
}

NS_IMETHODIMP nsXULTreeAccessible::IsChildSelected(PRInt32 aIndex, PRBool *_retval)
{
  return ChangeSelection(aIndex, eSelection_GetState, _retval);
}

NS_IMETHODIMP nsXULTreeAccessible::ClearSelection()
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->ClearSelection();

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return NS_ERROR_FAILURE;

  PRInt32 rowIndex, rowCount;
  PRInt32 selCount = 0;
  PRBool isSelected;
  mTreeView->GetRowCount(&rowCount);
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    selection->IsSelected(rowIndex, &isSelected);
    if (isSelected) {
      if (selCount == aIndex) {
        nsCOMPtr<nsIAccessible> tempAccess;
        tempAccess = new nsXULTreeitemAccessible(this, mDOMNode, mPresShell, rowIndex);
        if (!tempAccess)
          return NS_ERROR_OUT_OF_MEMORY;
        *_retval = tempAccess;
        NS_ADDREF(*_retval);
      }
      selCount++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::SelectAllSelection(PRBool *_retval)
{
  *_retval = PR_FALSE;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  // see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (selType.IsEmpty() || !selType.Equals(NS_LITERAL_STRING("single"))) {
      *_retval = PR_TRUE;
      nsCOMPtr<nsITreeSelection> selection;
      mTree->GetSelection(getter_AddRefs(selection));
      if (selection)
        selection->SelectAll();
    }
  }

  return NS_OK;
}

// Get the nsITreeBoxObject interface from any levels DOMNode under the <tree>
void nsXULTreeAccessible::GetTreeBoxObject(nsIDOMNode *aDOMNode, nsITreeBoxObject **aBoxObject)
{
  nsAutoString name;
  nsCOMPtr<nsIDOMNode> parentNode, currentNode;

  // Find DOMNode's parents recursively until reach the <tree> tag
  currentNode = aDOMNode;
  while (currentNode) {
    currentNode->GetLocalName(name);
    if (name.Equals(NS_LITERAL_STRING("tree"))) {
      // We will get the nsITreeBoxObject from the tree node
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(currentNode));
      if (xulElement) {
        nsCOMPtr<nsIBoxObject> box;
        xulElement->GetBoxObject(getter_AddRefs(box));
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        if (treeBox) {
          *aBoxObject = treeBox;
          NS_ADDREF(*aBoxObject);
          return;
        }
      }
    }
    currentNode->GetParentNode(getter_AddRefs(parentNode));
    currentNode = parentNode;
  }

  *aBoxObject = nsnull;
}

/* Implementation of nsIAccessibleTable for nsXULTreeAccessible */
NS_IMETHODIMP nsXULTreeAccessible::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = mCaption;
  NS_IF_ADDREF(*aCaption);

  return NS_OK;
}
NS_IMETHODIMP nsXULTreeAccessible::SetCaption(nsIAccessible *aCaption)
{
  mCaption = aCaption;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetSummary(nsAString &aSummary)
{
  aSummary = mSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::SetSummary(const nsAString &aSummary)
{
  mSummary = aSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetColumns(PRInt32 *aColumns)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  rv = nsAccessible::GetAccFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  return acc->GetAccChildCount(aColumns);
}

NS_IMETHODIMP nsXULTreeAccessible::GetColumnHeader(nsIAccessibleTable **aColumnHeader)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIAccessible> acc;
  nsAccessible::GetAccFirstChild(getter_AddRefs(acc));
  NS_ENSURE_TRUE(acc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleTable> accTable(do_QueryInterface(acc, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aColumnHeader = accTable;
  NS_IF_ADDREF(*aColumnHeader);

  return rv;
}

NS_IMETHODIMP nsXULTreeAccessible::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  return mTreeView->GetRowCount(aRows);
}

NS_IMETHODIMP nsXULTreeAccessible::GetRowHeader(nsIAccessibleTable **aRowHeader)
{
  // Row header not supported
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessible::GetSelectedColumns(PRUint32 *aNumColumns, PRInt32 **aColumns)
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

NS_IMETHODIMP nsXULTreeAccessible::GetSelectedRows(PRUint32 *aNumRows, PRInt32 **aRows)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aNumRows);

  nsresult rv = NS_OK;

  rv = GetSelectionCount((PRInt32 *)aNumRows);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 *outArray = (PRInt32 *)nsMemory::Alloc((*aNumRows) * sizeof(PRInt32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTree->GetSelection(getter_AddRefs(selection));
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

NS_IMETHODIMP nsXULTreeAccessible::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **_retval)
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
  rv = column->AccGetDOMNode(getter_AddRefs(columnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> columnElement(do_QueryInterface(columnNode, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString id;
  rv = columnElement->GetAttribute(NS_LITERAL_STRING("id"), id);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 realColumn;
  rv = mTree->GetColumnIndex(id.get(), &realColumn);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = new nsXULTreeitemAccessible(this, mDOMNode, mPresShell, aRow, realColumn);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aRow * columns + aColumn;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aIndex % columns;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;

  PRInt32 columns;
  rv = GetColumns(&columns);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aIndex / columns;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessible::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
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

NS_IMETHODIMP nsXULTreeAccessible::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;

  nsCOMPtr<nsITreeSelection> selection;
  rv = mTree->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  return selection->IsSelected(aRow, _retval);
}

NS_IMETHODIMP nsXULTreeAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  return IsRowSelected(aRow, _retval);
}
/* End Implementation of nsIAccessibleTable for nsXULTreeAccessible */

// ---------- nsXULTreeitemAccessible ---------- 

nsXULTreeitemAccessible::nsXULTreeitemAccessible(nsIAccessible *aParent, nsIDOMNode *aDOMNode, nsIWeakReference *aShell, PRInt32 aRow, PRInt32 aColumn):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;

  nsXULTreeAccessible::GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  // Since the real tree item does not correspond to any DOMNode, use the row index to distinguish each item
  mRow = aRow;
  mColumnIndex = aColumn;
  if (mTree) {
    if (mColumnIndex < 0) {
      PRInt32 keyColumn;
      mTree->GetKeyColumnIndex(&keyColumn);
      mTree->GetColumnID(keyColumn, mColumn);
    } else {
      mTree->GetColumnID(aColumn, mColumn);
    }
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeitemAccessible, nsLeafAccessible)

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccName(nsAString& _retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  return mTreeView->GetCellText(mRow, mColumn.get(), _retval);
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccValue(nsAString& _retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 level;
  mTreeView->GetLevel(mRow, &level);

  nsCString str;
  str.AppendInt(level);
  _retval = NS_ConvertASCIItoUCS2(str);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccId(PRInt32 *aAccId)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aAccId = - NS_PTR_TO_INT32(this);
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_OUTLINEITEM;
  return NS_OK;
}

// Possible states: focused, focusable, selected, expanded/collapsed
NS_IMETHODIMP nsXULTreeitemAccessible::GetAccState(PRUint32 *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  *_retval = STATE_FOCUSABLE | STATE_SELECTABLE;

  // get expanded/collapsed state
  PRBool isContainer, isContainerOpen;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer) {
    mTreeView->IsContainerOpen(mRow, &isContainerOpen);
    if (isContainerOpen)
      *_retval |= STATE_EXPANDED;
    else
      *_retval |= STATE_COLLAPSED;
  }

  // get selected state
  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected, currentIndex;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      *_retval |= STATE_SELECTED;
    selection->GetCurrentIndex(&currentIndex);
    if (currentIndex == mRow)
      *_retval |= STATE_FOCUSED;
  }

  PRInt32 firstVisibleRow, lastVisibleRow;
  mTree->GetFirstVisibleRow(&firstVisibleRow);
  mTree->GetLastVisibleRow(&lastVisibleRow);
  if (mRow < firstVisibleRow || mRow > lastVisibleRow)
    *_retval |= STATE_INVISIBLE;

  return NS_OK;
}

// Only one actions available
NS_IMETHODIMP nsXULTreeitemAccessible::GetAccNumActions(PRUint8 *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  *_retval = eNo_Action;

  PRBool isContainer;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer)
    *_retval = eSingle_Action;

  return NS_OK;
}

// Return the name of our only action
NS_IMETHODIMP nsXULTreeitemAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  if (index == eAction_Click) {
    PRBool isContainer, isContainerOpen;
    mTreeView->IsContainer(mRow, &isContainer);
    if (isContainer) {
      mTreeView->IsContainerOpen(mRow, &isContainerOpen);
      if (isContainerOpen)
        nsAccessible::GetTranslatedString(NS_LITERAL_STRING("collapse"), _retval);
      else
        nsAccessible::GetTranslatedString(NS_LITERAL_STRING("expand"), _retval);
    }

    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccParent(nsIAccessible **aAccParent)
{
  *aAccParent = nsnull;

  if (mParent) {
    *aAccParent = mParent;
    NS_ADDREF(*aAccParent);
  }

  return NS_OK;
}

// Return the next row of tree if mColumnIndex < 0 (if any),
// otherwise return the next cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetAccNextSibling(nsIAccessible **aAccNextSibling)
{
  *aAccNextSibling = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);

  if (mColumnIndex < 0) {
    if (mRow < rowCount - 1) {
      *aAccNextSibling = new nsXULTreeitemAccessible(mParent, mDOMNode, mPresShell, mRow + 1);
      if (! *aAccNextSibling)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(*aAccNextSibling);
    }

    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIAccessibleTable> table(do_QueryInterface(mParent, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount, row = mRow, column = mColumnIndex;
  rv = table->GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mColumnIndex < columnCount - 1) {
    column++;
  } else if (mRow < rowCount - 1) {
    column = 0;
    row++;
  }

  *aAccNextSibling = new nsXULTreeitemAccessible(mParent, mDOMNode, mPresShell, row, column);
  NS_ENSURE_TRUE(*aAccNextSibling, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAccNextSibling);
  
  return rv;
}

// Return the previou row of tree if mColumnIndex < 0 (if any),
// otherwise return the previou cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetAccPreviousSibling(nsIAccessible **aAccPreviousSibling)
{
  *aAccPreviousSibling = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  if (mRow > 0 && mColumnIndex < 0) {
    *aAccPreviousSibling = new nsXULTreeitemAccessible(mParent, mDOMNode, mPresShell, mRow - 1);
    if (! *aAccPreviousSibling)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aAccPreviousSibling);

    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIAccessibleTable> table(do_QueryInterface(mParent, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 columnCount, row = mRow, column = mColumnIndex;
  rv = table->GetColumns(&columnCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mColumnIndex > 0) {
    column--;
  } else if (mRow > 0) {
    column = columnCount - 1;
    row--;
  }

  *aAccPreviousSibling = new nsXULTreeitemAccessible(mParent, mDOMNode, mPresShell, row, column);
  NS_ENSURE_TRUE(*aAccPreviousSibling, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAccPreviousSibling);
  
  return rv;
}

NS_IMETHODIMP nsXULTreeitemAccessible::AccDoAction(PRUint8 index)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  if (index == eAction_Click)
    return mTreeView->ToggleOpenState(mRow);

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeitemAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  *x = *y = *width = *height = 0;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  const PRUnichar empty[] = {'\0'};

  // This Bounds are based on Tree's coord
  mTree->GetCoordsForCellItem(mRow, mColumn.get(), empty, x, y, width, height);

  // Get treechildren's BoxObject to adjust the Bounds' upper left corner
  nsCOMPtr<nsIBoxObject> boxObject(do_QueryInterface(mTree));
  if (boxObject) {
    nsCOMPtr<nsIDOMElement> boxElement;
    boxObject->GetElement(getter_AddRefs(boxElement));
    nsCOMPtr<nsIDOMNode> boxNode(do_QueryInterface(boxElement));
    if (boxNode) {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      boxNode->GetChildNodes(getter_AddRefs(childNodes));
      if (childNodes) {
        nsAutoString name;
        nsCOMPtr<nsIDOMNode> childNode;
        PRUint32 childCount, childIndex;

        childNodes->GetLength(&childCount);
        for (childIndex = 0; childIndex < childCount; childIndex++) {
          childNodes->Item(childIndex, getter_AddRefs(childNode));
          childNode->GetLocalName(name);
          if (name.Equals(NS_LITERAL_STRING("treechildren"))) {
            nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(childNode));
            if (xulElement) {
              nsCOMPtr<nsIBoxObject> box;
              xulElement->GetBoxObject(getter_AddRefs(box));
              if (box) {
                PRInt32 myX, myY;
                box->GetScreenX(&myX);
                box->GetScreenY(&myY);
                *x += myX;
                *y += myY;
              }
            }
            break;
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::AccRemoveSelection()
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      selection->ToggleSelect(mRow);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::AccTakeSelection()
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (! isSelected)
      selection->ToggleSelect(mRow);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::AccTakeFocus()
{ 
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTree->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->SetCurrentIndex(mRow);

  // focus event will be fired here
  return nsAccessible::AccTakeFocus();
}

// ---------- nsXULTreeColumnsAccessible ----------

nsXULTreeColumnsAccessible::nsXULTreeColumnsAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsAccessible(aDOMNode, aShell)
{
  mCaption = nsnull;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeColumnsAccessible, nsAccessible, nsIAccessibleTable)

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("click"), _retval);
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccNextSibling(nsIAccessible **aAccNextSibling) 
{
  nsresult ret = nsAccessible::GetAccNextSibling(aAccNextSibling);

  if (*aAccNextSibling == nsnull) { // if there is not other sibling, use the first row as its sibling
    nsCOMPtr<nsITreeBoxObject> tree;
    nsCOMPtr<nsITreeView> treeView;

    nsXULTreeAccessible::GetTreeBoxObject(mDOMNode, getter_AddRefs(tree));
    if (tree) {
      tree->GetView(getter_AddRefs(treeView));
      if (treeView) {
        PRInt32 rowCount;
        treeView->GetRowCount(&rowCount);
        if (rowCount > 0) {
          *aAccNextSibling = new nsXULTreeitemAccessible(mParent, mDOMNode, mPresShell, 0);
          if (! *aAccNextSibling)
            return NS_ERROR_OUT_OF_MEMORY;
          NS_ADDREF(*aAccNextSibling);
          ret = NS_OK;
        }
      }
    }
  }

  return ret;  
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetAccPreviousSibling(nsIAccessible **aAccPreviousSibling) 
{  
  return nsAccessible::GetAccPreviousSibling(aAccPreviousSibling);
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click)
    return NS_OK;

  return NS_ERROR_INVALID_ARG;
}

// Implementation of nsIAccessibleTable for nsXULTreeColumnsAccessible
NS_IMETHODIMP nsXULTreeColumnsAccessible::GetCaption(nsIAccessible **aCaption)
{
  *aCaption = mCaption;
  NS_IF_ADDREF(*aCaption);

  return NS_OK;
}
NS_IMETHODIMP nsXULTreeColumnsAccessible::SetCaption(nsIAccessible *aCaption)
{
  mCaption = aCaption;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetSummary(nsAString &aSummary)
{
  aSummary = mSummary;
  return NS_OK;
}
NS_IMETHODIMP nsXULTreeColumnsAccessible::SetSummary(const nsAString &aSummary)
{
  mSummary = aSummary;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetColumns(PRInt32 *aColumns)
{
  return GetAccChildCount(aColumns);
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetColumnHeader(nsIAccessibleTable * *aColumnHeader)
{
  // Column header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRows(PRInt32 *aRows)
{
  NS_ENSURE_ARG_POINTER(aRows);

  *aRows = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRowHeader(nsIAccessibleTable * *aRowHeader)
{
  // Row header not supported.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetSelectedColumns(PRUint32 *columnsSize, PRInt32 **columns)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetSelectedRows(PRUint32 *rowsSize, PRInt32 **rows)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::CellRefAt(PRInt32 aRow, PRInt32 aColumn, nsIAccessible **_retval)
{
  nsCOMPtr<nsIAccessible> next, temp;
  GetAccFirstChild(getter_AddRefs(next));
  NS_ENSURE_TRUE(next, NS_ERROR_FAILURE);

  for (PRInt32 col = 0; col < aColumn; col++) {
    next->GetAccNextSibling(getter_AddRefs(temp));
    NS_ENSURE_TRUE(temp, NS_ERROR_FAILURE);

    next = temp;
  }

  *_retval = next;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetIndexAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aColumn;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetColumnAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = aIndex;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRowAtIndex(PRInt32 aIndex, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetColumnExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRowExtentAt(PRInt32 aRow, PRInt32 aColumn, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetColumnDescription(PRInt32 aColumn, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRowDescription(PRInt32 aRow, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::IsColumnSelected(PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::IsRowSelected(PRInt32 aRow, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::IsCellSelected(PRInt32 aRow, PRInt32 aColumn, PRBool *_retval)
{
  // Header can not be selected.
  return NS_ERROR_NOT_IMPLEMENTED;
}
// End Implementation of nsIAccessibleTable for nsXULTreeColumnsAccessible

// ---------- nsXULTreeColumnitemAccessible ----------

nsXULTreeColumnitemAccessible::nsXULTreeColumnitemAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsLeafAccessible(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeColumnitemAccessible, nsLeafAccessible)

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetAccName(nsAString& _retval)
{
  return GetXULAccName(_retval);
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_COLUMNHEADER;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("click"), _retval);
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMXULElement> colElement(do_QueryInterface(mDOMNode));
    if (colElement)
      colElement->Click();

    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}
