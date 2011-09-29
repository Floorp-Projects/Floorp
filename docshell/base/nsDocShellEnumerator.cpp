/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsDocShellEnumerator.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

nsDocShellEnumerator::nsDocShellEnumerator(PRInt32 inEnumerationDirection)
: mRootItem(nsnull)
, mCurIndex(0)
, mDocShellType(nsIDocShellTreeItem::typeAll)
, mArrayValid(PR_FALSE)
, mEnumerationDirection(inEnumerationDirection)
{
}

nsDocShellEnumerator::~nsDocShellEnumerator()
{
}

NS_IMPL_ISUPPORTS1(nsDocShellEnumerator, nsISimpleEnumerator)


/* nsISupports getNext (); */
NS_IMETHODIMP nsDocShellEnumerator::GetNext(nsISupports **outCurItem)
{
  NS_ENSURE_ARG_POINTER(outCurItem);
  *outCurItem = nsnull;
  
  nsresult rv = EnsureDocShellArray();
  if (NS_FAILED(rv)) return rv;
  
  if (mCurIndex >= mItemArray.Length()) {
    return NS_ERROR_FAILURE;
  }

  // post-increment is important here
  nsCOMPtr<nsISupports> item = do_QueryReferent(mItemArray[mCurIndex++], &rv);
  item.forget(outCurItem);
  return rv;
}

/* boolean hasMoreElements (); */
NS_IMETHODIMP nsDocShellEnumerator::HasMoreElements(bool *outHasMore)
{
  NS_ENSURE_ARG_POINTER(outHasMore);
  *outHasMore = PR_FALSE;

  nsresult rv = EnsureDocShellArray();
  if (NS_FAILED(rv)) return rv;

  *outHasMore = (mCurIndex < mItemArray.Length());
  return NS_OK;
}

nsresult nsDocShellEnumerator::GetEnumerationRootItem(nsIDocShellTreeItem * *aEnumerationRootItem)
{
  NS_ENSURE_ARG_POINTER(aEnumerationRootItem);
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryReferent(mRootItem);
  item.forget(aEnumerationRootItem);
  return NS_OK;
}

nsresult nsDocShellEnumerator::SetEnumerationRootItem(nsIDocShellTreeItem * aEnumerationRootItem)
{
  mRootItem = do_GetWeakReference(aEnumerationRootItem);
  ClearState();
  return NS_OK;
}

nsresult nsDocShellEnumerator::GetEnumDocShellType(PRInt32 *aEnumerationItemType)
{
  NS_ENSURE_ARG_POINTER(aEnumerationItemType);
  *aEnumerationItemType = mDocShellType;
  return NS_OK;
}

nsresult nsDocShellEnumerator::SetEnumDocShellType(PRInt32 aEnumerationItemType)
{
  mDocShellType = aEnumerationItemType;
  ClearState();
  return NS_OK;
}

nsresult nsDocShellEnumerator::First()
{
  mCurIndex = 0;
  return EnsureDocShellArray();
}

nsresult nsDocShellEnumerator::EnsureDocShellArray()
{
  if (!mArrayValid)
  {
    mArrayValid = PR_TRUE;
    return BuildDocShellArray(mItemArray);
  }
  
  return NS_OK;
}

nsresult nsDocShellEnumerator::ClearState()
{
  mItemArray.Clear();
  mArrayValid = PR_FALSE;
  mCurIndex = 0;
  return NS_OK;
}

nsresult nsDocShellEnumerator::BuildDocShellArray(nsTArray<nsWeakPtr>& inItemArray)
{
  NS_ENSURE_TRUE(mRootItem, NS_ERROR_NOT_INITIALIZED);
  inItemArray.Clear();
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryReferent(mRootItem);
  return BuildArrayRecursive(item, inItemArray);
}

nsresult nsDocShellForwardsEnumerator::BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsTArray<nsWeakPtr>& inItemArray)
{
  nsresult rv;
  nsCOMPtr<nsIDocShellTreeNode> itemAsNode = do_QueryInterface(inItem, &rv);
  if (NS_FAILED(rv)) return rv;

  PRInt32   itemType;
  // add this item to the array
  if ((mDocShellType == nsIDocShellTreeItem::typeAll) ||
      (NS_SUCCEEDED(inItem->GetItemType(&itemType)) && (itemType == mDocShellType)))
  {
    if (!inItemArray.AppendElement(do_GetWeakReference(inItem)))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32   numChildren;
  rv = itemAsNode->GetChildCount(&numChildren);
  if (NS_FAILED(rv)) return rv;
  
  for (PRInt32 i = 0; i < numChildren; ++i)
  {
    nsCOMPtr<nsIDocShellTreeItem> curChild;
    rv = itemAsNode->GetChildAt(i, getter_AddRefs(curChild));
    if (NS_FAILED(rv)) return rv;
      
    rv = BuildArrayRecursive(curChild, inItemArray);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}


nsresult nsDocShellBackwardsEnumerator::BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsTArray<nsWeakPtr>& inItemArray)
{
  nsresult rv;
  nsCOMPtr<nsIDocShellTreeNode> itemAsNode = do_QueryInterface(inItem, &rv);
  if (NS_FAILED(rv)) return rv;

  PRInt32   numChildren;
  rv = itemAsNode->GetChildCount(&numChildren);
  if (NS_FAILED(rv)) return rv;
  
  for (PRInt32 i = numChildren - 1; i >= 0; --i)
  {
    nsCOMPtr<nsIDocShellTreeItem> curChild;
    rv = itemAsNode->GetChildAt(i, getter_AddRefs(curChild));
    if (NS_FAILED(rv)) return rv;
      
    rv = BuildArrayRecursive(curChild, inItemArray);
    if (NS_FAILED(rv)) return rv;
  }

  PRInt32   itemType;
  // add this item to the array
  if ((mDocShellType == nsIDocShellTreeItem::typeAll) ||
      (NS_SUCCEEDED(inItem->GetItemType(&itemType)) && (itemType == mDocShellType)))
  {
    if (!inItemArray.AppendElement(do_GetWeakReference(inItem)))
      return NS_ERROR_OUT_OF_MEMORY;
  }


  return NS_OK;
}


