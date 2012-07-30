/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsDocShellEnumerator.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

nsDocShellEnumerator::nsDocShellEnumerator(PRInt32 inEnumerationDirection)
: mRootItem(nullptr)
, mCurIndex(0)
, mDocShellType(nsIDocShellTreeItem::typeAll)
, mArrayValid(false)
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
  *outCurItem = nullptr;
  
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
  *outHasMore = false;

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
    mArrayValid = true;
    return BuildDocShellArray(mItemArray);
  }
  
  return NS_OK;
}

nsresult nsDocShellEnumerator::ClearState()
{
  mItemArray.Clear();
  mArrayValid = false;
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


