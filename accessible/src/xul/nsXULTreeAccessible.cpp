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
 *   Author: Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsIBoxObject.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsITreeSelection.h"
#include "nsITreeColumns.h"
#include "nsXULTreeAccessibleWrap.h"
#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibleTable.h"
#endif

#define kMaxTreeColumns 100

/* static */
PRBool nsXULTreeAccessible::IsColumnHidden(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsIDOMElement> element;
  aColumn->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  return content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::hidden,
                              nsAccessibilityAtoms::_true, eCaseMatters);
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetNextVisibleColumn(nsITreeColumn *aColumn)
{
  // Skip hidden columns.
  nsCOMPtr<nsITreeColumn> nextColumn;
  aColumn->GetNext(getter_AddRefs(nextColumn));
  while (nextColumn && IsColumnHidden(nextColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    nextColumn->GetNext(getter_AddRefs(tempColumn));
    nextColumn.swap(tempColumn);
  }

  nsITreeColumn *retCol = nsnull;
  nextColumn.swap(retCol);
  return retCol;
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetFirstVisibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  nsCOMPtr<nsITreeColumn> column;
  aTree->GetColumns(getter_AddRefs(cols));
  if (cols) {
    cols->GetFirstColumn(getter_AddRefs(column));
  }

  if (column && IsColumnHidden(column)) {
    column = GetNextVisibleColumn(column);
  }
  NS_ENSURE_TRUE(column, nsnull);

  nsITreeColumn *retCol = nsnull;
  column.swap(retCol);
  return retCol;
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetLastVisibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  nsCOMPtr<nsITreeColumn> column;
  aTree->GetColumns(getter_AddRefs(cols));
  if (cols) {
    cols->GetLastColumn(getter_AddRefs(column));
  }

  // Skip hidden columns.
  while (column && IsColumnHidden(column)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    column->GetPrevious(getter_AddRefs(tempColumn));
    column.swap(tempColumn);
  }
  NS_ENSURE_TRUE(column, nsnull);

  nsITreeColumn *retCol = nsnull;
  column.swap(retCol);
  return retCol;
}

// ---------- nsXULTreeAccessible ----------

nsXULTreeAccessible::nsXULTreeAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULSelectableAccessible(aDOMNode, aShell),
mAccessNodeCache(nsnull)

{
  GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");
  mAccessNodeCache = new nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode>;
  mAccessNodeCache->Init(kDefaultTreeCacheSize);
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeAccessible, nsXULSelectableAccessible, nsIAccessibleTreeCache)
                                                                                                       


// Get the nsITreeBoxObject interface from any levels DOMNode under the <tree>
void nsXULTreeAccessible::GetTreeBoxObject(nsIDOMNode *aDOMNode, nsITreeBoxObject **aBoxObject)
{
  nsAutoString name;
  nsCOMPtr<nsIDOMNode> parentNode, currentNode;

  // Find DOMNode's parents recursively until reach the <tree> tag
  currentNode = aDOMNode;
  while (currentNode) {
    currentNode->GetLocalName(name);
    if (name.EqualsLiteral("tree")) {
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

NS_IMETHODIMP nsXULTreeAccessible::GetState(PRUint32 *_retval)
{
  // Get focus status from base class                                           
  nsAccessible::GetState(_retval);                                           

  // see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    // the default selection type is multiple
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (selType.IsEmpty() || !selType.EqualsLiteral("single"))
      *_retval |= STATE_MULTISELECTABLE;
  }

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

// The value is the first selected child
NS_IMETHODIMP nsXULTreeAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate(0);

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (! selection)
    return NS_ERROR_FAILURE;

  PRInt32 currentIndex;
  nsCOMPtr<nsIDOMElement> selectItem;
  selection->GetCurrentIndex(&currentIndex);
  if (currentIndex >= 0) {
    nsCOMPtr<nsITreeColumn> keyCol;

    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(keyCol));

    return mTreeView->GetCellText(currentIndex, keyCol, _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::Shutdown()
{
  nsXULSelectableAccessible::Shutdown();

  if (mAccessNodeCache) {
    ClearCache(*mAccessNodeCache);
    delete mAccessNodeCache;
    mAccessNodeCache = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetRole(PRUint32 *aRole)
{
  NS_ASSERTION(mTree, "No tree view");
  PRInt32 colCount = 0;
  if (NS_SUCCEEDED(GetColumnCount(mTree, &colCount)) && (colCount > 1))
    *aRole = ROLE_TREE_TABLE;
  else
    *aRole = ROLE_OUTLINE;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetFirstChild(nsIAccessible **aFirstChild)
{
  nsAccessible::GetFirstChild(aFirstChild);

  // in normal case, tree's first child should be treecols, if it is not here, 
  //   use the first row as tree's first child
  if (*aFirstChild == nsnull) {
    NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

    PRInt32 rowCount;
    mTreeView->GetRowCount(&rowCount);
    if (rowCount > 0) {
      nsCOMPtr<nsITreeColumn> column = GetFirstVisibleColumn(mTree);
      return GetCachedTreeitemAccessible(0, column, aFirstChild);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);
  if (rowCount > 0) {
    nsCOMPtr<nsITreeColumn> column = GetLastVisibleColumn(mTree);
    return GetCachedTreeitemAccessible(rowCount - 1, column, aLastChild);
  }
  else // if there is not any rows, use treecols as tree's last child
    nsAccessible::GetLastChild(aLastChild);

  return NS_OK;
}

// tree's children count is row count + treecols count
NS_IMETHODIMP nsXULTreeAccessible::GetChildCount(PRInt32 *aAccChildCount)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsAccessible::GetChildCount(aAccChildCount);

  if (*aAccChildCount != eChildCountUninitialized) {
    PRInt32 rowCount;
    mTreeView->GetRowCount(&rowCount);
    *aAccChildCount += rowCount;
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
    do_QueryInterface(mDOMNode);
  if (multiSelect) {
    PRInt32 row;
    multiSelect->GetCurrentIndex(&row);
    if (row >= 0) {
      GetCachedTreeitemAccessible(row, nsnull, aFocusedChild);
      if (*aFocusedChild) {
        return NS_OK;  // Already addref'd by getter
      }
    }
  }
  NS_ADDREF(*aFocusedChild = this);
  return NS_OK;
}

// Ask treeselection to get all selected children
NS_IMETHODIMP nsXULTreeAccessible::GetSelectedChildren(nsIArray **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIMutableArray> selectedAccessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(selectedAccessibles);

  PRInt32 rowIndex, rowCount;
  PRBool isSelected;
  mTreeView->GetRowCount(&rowCount);
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    selection->IsSelected(rowIndex, &isSelected);
    if (isSelected) {
      nsCOMPtr<nsIAccessible> tempAccess;
      if (NS_FAILED(GetCachedTreeitemAccessible(rowIndex, nsnull, getter_AddRefs(tempAccess))) || !tempAccess)

        return NS_ERROR_OUT_OF_MEMORY;
      selectedAccessibles->AppendElement(tempAccess, PR_FALSE);
    }
  }

  PRUint32 length;
  selectedAccessibles->GetLength(&length);
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
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->GetCount(aSelectionCount);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    selection->IsSelected(aIndex, aSelState);
    if ((!(*aSelState) && eSelection_Add == aMethod) || 
        ((*aSelState) && eSelection_Remove == aMethod))
      return selection->ToggleSelect(aIndex);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::AddChildToSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsXULTreeAccessible::RemoveChildFromSelection(PRInt32 aIndex)
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
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->ClearSelection();

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
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
        return GetCachedTreeitemAccessible(rowIndex, nsnull, _retval);
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
    if (selType.IsEmpty() || !selType.EqualsLiteral("single")) {
      *_retval = PR_TRUE;
      nsCOMPtr<nsITreeSelection> selection;
      mTreeView->GetSelection(getter_AddRefs(selection));
      if (selection)
        selection->SelectAll();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetCachedTreeitemAccessible(PRInt32 aRow, nsITreeColumn* aColumn, nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;

  NS_ASSERTION(mAccessNodeCache, "No accessibility cache for tree");
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  nsCOMPtr<nsITreeColumn> col;
#ifdef MOZ_ACCESSIBILITY_ATK
  col = aColumn;
#endif
  PRInt32 columnIndex = -1;

  if (!col && mTree) {
    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(col));
  }

  if (col)
     col->GetIndex(&columnIndex);

  nsCOMPtr<nsIAccessNode> accessNode;
  GetCacheEntry(*mAccessNodeCache, (void*)(aRow * kMaxTreeColumns + columnIndex), getter_AddRefs(accessNode));
  if (!accessNode)
  {
    accessNode = new nsXULTreeitemAccessibleWrap(this, mDOMNode, mWeakShell, aRow, col);
    nsCOMPtr<nsPIAccessNode> privateAccessNode(do_QueryInterface(accessNode));
    if (!privateAccessNode)
      return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = privateAccessNode->Init();
    NS_ENSURE_SUCCESS(rv, rv);
    PutCacheEntry(*mAccessNodeCache, (void*)(aRow * kMaxTreeColumns + columnIndex), accessNode);
  }
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
  NS_IF_ADDREF(*aAccessible = accessible);
  return NS_OK;
}

nsresult nsXULTreeAccessible::GetColumnCount(nsITreeBoxObject* aBoxObject, PRInt32* aCount)
{
  NS_ENSURE_TRUE(aBoxObject, NS_ERROR_FAILURE);
  nsCOMPtr<nsITreeColumns> treeColumns;
  aBoxObject->GetColumns(getter_AddRefs(treeColumns));
  NS_ENSURE_TRUE(treeColumns, NS_ERROR_FAILURE);
  return treeColumns->GetCount(aCount);
}

// ---------- nsXULTreeitemAccessible ---------- 

nsXULTreeitemAccessible::nsXULTreeitemAccessible(nsIAccessible *aParent, nsIDOMNode *aDOMNode, nsIWeakReference *aShell, PRInt32 aRow, nsITreeColumn* aColumn)
  : nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;  // xxx todo: do we need this? We already have mParent on nsAccessible

  nsXULTreeAccessible::GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  // Since the real tree item does not correspond to any DOMNode, use the row index to distinguish each item
  mRow = aRow;
  mColumn = aColumn;

  if (!mColumn && mTree) {
    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(mColumn));
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeitemAccessible, nsLeafAccessible)

NS_IMETHODIMP nsXULTreeitemAccessible::Shutdown()
{
  mTree = nsnull;
  mTreeView = nsnull;
  mColumn = nsnull;
  return nsLeafAccessible::Shutdown();
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetName(nsAString& aName)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  mTreeView->GetCellText(mRow, mColumn, aName);
  
  // if still no name try cell value
  if (aName.IsEmpty()) {
    mTreeView->GetCellValue(mRow, mColumn, aName);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = NS_STATIC_CAST(void*, this);
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetRole(PRUint32 *aRole)
{
  PRInt32 colCount = 0;
  if (NS_SUCCEEDED(nsXULTreeAccessible::GetColumnCount(mTree, &colCount)) && colCount > 1)
    *aRole = ROLE_CELL;
  else
    *aRole = ROLE_OUTLINEITEM;
  return NS_OK;
}

// Possible states: focused, focusable, selected, expanded/collapsed
NS_IMETHODIMP nsXULTreeitemAccessible::GetState(PRUint32 *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  *_retval = STATE_FOCUSABLE | STATE_SELECTABLE;

  // get expanded/collapsed state
  PRBool isContainer, isContainerOpen, isContainerEmpty;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer) {
    mTreeView->IsContainerEmpty(mRow, &isContainerEmpty);
    if (!isContainerEmpty) {
      mTreeView->IsContainerOpen(mRow, &isContainerOpen);
      *_retval |= isContainerOpen? STATE_EXPANDED: STATE_COLLAPSED;
    }
  }

  // get selected state
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      *_retval |= STATE_SELECTED;
  }

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
    do_QueryInterface(mDOMNode);
  if (multiSelect) {
    PRInt32 currentIndex;
    multiSelect->GetCurrentIndex(&currentIndex);
    if (currentIndex == mRow) {
      *_retval |= STATE_FOCUSED;
    }
  }

  PRInt32 firstVisibleRow, lastVisibleRow;
  mTree->GetFirstVisibleRow(&firstVisibleRow);
  mTree->GetLastVisibleRow(&lastVisibleRow);
  if (mRow < firstVisibleRow || mRow > lastVisibleRow)
    *_retval |= STATE_INVISIBLE;

  return NS_OK;
}

// "activate" (xor "cycle") action is available for all treeitems
// "expand/collapse" action is avaible for treeitem which is container
NS_IMETHODIMP nsXULTreeitemAccessible::GetNumActions(PRUint8 *_retval)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);
  PRBool isContainer;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer)
    *_retval = 2;
  else
    *_retval = 1;

  return NS_OK;
}

// Return the name of our actions
NS_IMETHODIMP nsXULTreeitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  NS_ENSURE_TRUE(mColumn && mTree && mTreeView, NS_ERROR_FAILURE);

  if (aIndex == eAction_Click) {
    PRBool isCycler;
    mColumn->GetCycler(&isCycler);
    if (isCycler) {
      aName.AssignLiteral("cycle");
    }
    else {
      aName.AssignLiteral("activate");
    }
    return NS_OK;
  }
  else if (aIndex == eAction_Expand) {
    PRBool isContainer, isContainerOpen;
    mTreeView->IsContainer(mRow, &isContainer);
    if (isContainer) {
      mTreeView->IsContainerOpen(mRow, &isContainerOpen);
      if (isContainerOpen)
        aName.AssignLiteral("collapse");
      else
        aName.AssignLiteral("expand");
    }

    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetParent(nsIAccessible **aParent)
{
  *aParent = nsnull;

  if (mParent) {
    *aParent = mParent;
    NS_ADDREF(*aParent);
  }

  return NS_OK;
}

// Return the next row of tree if mColumn (if any),
// otherwise return the next cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  *aNextSibling = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(mParent));
  NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);

  if (!mColumn) {
    if (mRow < rowCount - 1)
      return treeCache->GetCachedTreeitemAccessible(mRow + 1, nsnull, aNextSibling);
    else
      return NS_OK;
  }

  nsresult rv = NS_OK;
  PRInt32 row = mRow;
  nsCOMPtr<nsITreeColumn> column;
#ifdef MOZ_ACCESSIBILITY_ATK
  column = nsXULTreeAccessible::GetNextVisibleColumn(mColumn);

  if (!column) {
    if (mRow < rowCount -1) {
      row++;
      column = nsXULTreeAccessible::GetFirstVisibleColumn(mTree);
    } else {
      // the next sibling of the last treeitem is null
      return NS_OK;
    }
  }
#else
  if (++row >= rowCount) {
    return NS_ERROR_FAILURE;
  }
#endif //MOZ_ACCESSIBILITY_ATK

  rv = treeCache->GetCachedTreeitemAccessible(row, column, aNextSibling);
  
  return rv;
}

// Return the previous row of tree if mColumn (if any),
// otherwise return the previous cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling)
{
  *aPreviousSibling = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(mParent));
  NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);

  if (!mColumn && mRow > 0)
    return treeCache->GetCachedTreeitemAccessible(mRow - 1, nsnull, aPreviousSibling);
  
  nsresult rv = NS_OK;


  PRInt32 row = mRow;
  nsCOMPtr<nsITreeColumn> column;
#ifdef MOZ_ACCESSIBILITY_ATK
  rv = mColumn->GetPrevious(getter_AddRefs(column));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!column && mRow > 0) {
    row--;
    column = nsXULTreeAccessible::GetLastVisibleColumn(mTree);
  }
#else
  if (--row < 0) {
    return NS_ERROR_FAILURE;
  }
#endif

  rv = treeCache->GetCachedTreeitemAccessible(row, column, aPreviousSibling);

  return rv;
}

NS_IMETHODIMP nsXULTreeitemAccessible::DoAction(PRUint8 index)
{
  NS_ENSURE_TRUE(mColumn && mTree && mTreeView, NS_ERROR_FAILURE);

  if (index == eAction_Click) {
    nsresult rv = NS_OK;
    PRBool isCycler;
    mColumn->GetCycler(&isCycler);
    if (isCycler) {
      rv = mTreeView->CycleCell(mRow, mColumn);
    } 
    else {
      nsCOMPtr<nsITreeSelection> selection;
      mTreeView->GetSelection(getter_AddRefs(selection));
      if (selection) {
        rv = selection->Select(mRow);
        mTree->EnsureRowIsVisible(mRow);
      }
    }
    return rv;
  }
  else if (index == eAction_Expand) {
    PRBool isContainer;
    mTreeView->IsContainer(mRow, &isContainer);
    if (isContainer)
      return mTreeView->ToggleOpenState(mRow);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  *x = *y = *width = *height = 0;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  // This Bounds are based on Tree's coord
  mTree->GetCoordsForCellItem(mRow, mColumn, EmptyCString(), x, y, width, height);

  // Get treechildren's BoxObject to adjust the Bounds' upper left corner
  // XXXvarga consider using mTree->GetTreeBody()
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
          if (name.EqualsLiteral("treechildren")) {
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

NS_IMETHODIMP nsXULTreeitemAccessible::SetSelected(PRBool aSelect)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected != aSelect)
      selection->ToggleSelect(mRow);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::TakeFocus()
{ 
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->SetCurrentIndex(mRow);

  // focus event will be fired here
  return nsAccessible::TakeFocus();
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetAccessibleRelated(PRUint32 aRelationType, nsIAccessible **aRelated)
{
  //currentlly only for ATK. and in the future, we'll sync MSAA and ATK same. 
  //that's why ATK specific code shows here
  *aRelated = nsnull;
#ifdef MOZ_ACCESSIBILITY_ATK
  if (aRelationType == RELATION_NODE_CHILD_OF) {
    PRInt32 columnIndex;
    if (NS_SUCCEEDED(mColumn->GetIndex(&columnIndex)) && columnIndex == 0) {
      PRInt32 parentIndex;
      if (NS_SUCCEEDED(mTreeView->GetParentIndex(mRow, &parentIndex))) {
        if (parentIndex == -1) {
          NS_IF_ADDREF(*aRelated = mParent);
          return NS_OK;
        } else {
          nsCOMPtr<nsIAccessibleTreeCache> cache =
            do_QueryInterface(mParent);
          return cache->GetCachedTreeitemAccessible(parentIndex, mColumn, aRelated);
        }
      }
    }
    return NS_OK;
  } else { 
#endif
    return nsAccessible::GetAccessibleRelated(aRelationType, aRelated);
#ifdef MOZ_ACCESSIBILITY_ATK
  }
#endif
}

// ---------- nsXULTreeColumnsAccessible ----------

nsXULTreeColumnsAccessible::nsXULTreeColumnsAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsAccessibleWrap(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeColumnsAccessible, nsAccessible)

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetState(PRUint32 *_retval)
{
  *_retval = STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("click");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetNextSibling(nsIAccessible **aNextSibling) 
{
  nsresult ret = nsAccessible::GetNextSibling(aNextSibling);

  if (*aNextSibling == nsnull) { // if there is not other sibling, use the first row as its sibling
    nsCOMPtr<nsITreeBoxObject> tree;
    nsCOMPtr<nsITreeView> treeView;

    nsXULTreeAccessible::GetTreeBoxObject(mDOMNode, getter_AddRefs(tree));
    if (tree) {
      tree->GetView(getter_AddRefs(treeView));
      if (treeView) {
        PRInt32 rowCount;
        treeView->GetRowCount(&rowCount);
        if (rowCount > 0) {
          nsCOMPtr<nsITreeColumn> column = nsXULTreeAccessible::GetFirstVisibleColumn(tree);
          nsCOMPtr<nsIAccessibleTreeCache> treeCache(do_QueryInterface(mParent));
          NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);
          ret = treeCache->GetCachedTreeitemAccessible(0, column, aNextSibling);
        }
      }
    }
  }

  return ret;  
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling) 
{  
  return nsAccessible::GetPreviousSibling(aPreviousSibling);
}

NS_IMETHODIMP nsXULTreeColumnsAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click)
    return NS_OK;

  return NS_ERROR_INVALID_ARG;
}

// ---------- nsXULTreeColumnitemAccessible ----------

nsXULTreeColumnitemAccessible::nsXULTreeColumnitemAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsLeafAccessible(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeColumnitemAccessible, nsLeafAccessible)

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetState(PRUint32 *_retval)
{
  *_retval = STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetName(nsAString& _retval)
{
  return GetXULName(_retval);
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_COLUMNHEADER;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("click");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsXULTreeColumnitemAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    return DoCommand();
  }

  return NS_ERROR_INVALID_ARG;
}
