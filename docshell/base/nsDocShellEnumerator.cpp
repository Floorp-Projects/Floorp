/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShellEnumerator.h"

#include "nsIDocShellTreeItem.h"

nsDocShellEnumerator::nsDocShellEnumerator(int32_t aEnumerationDirection)
  : mRootItem(nullptr)
  , mCurIndex(0)
  , mDocShellType(nsIDocShellTreeItem::typeAll)
  , mArrayValid(false)
  , mEnumerationDirection(aEnumerationDirection)
{
}

nsDocShellEnumerator::~nsDocShellEnumerator()
{
}

NS_IMPL_ISUPPORTS(nsDocShellEnumerator, nsISimpleEnumerator)

/* nsISupports getNext (); */
NS_IMETHODIMP
nsDocShellEnumerator::GetNext(nsISupports** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  nsresult rv = EnsureDocShellArray();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mCurIndex >= mItemArray.Length()) {
    return NS_ERROR_FAILURE;
  }

  // post-increment is important here
  nsCOMPtr<nsISupports> item = do_QueryReferent(mItemArray[mCurIndex++], &rv);
  item.forget(aResult);
  return rv;
}

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsDocShellEnumerator::HasMoreElements(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  nsresult rv = EnsureDocShellArray();
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = (mCurIndex < mItemArray.Length());
  return NS_OK;
}

nsresult
nsDocShellEnumerator::GetEnumerationRootItem(
    nsIDocShellTreeItem** aEnumerationRootItem)
{
  NS_ENSURE_ARG_POINTER(aEnumerationRootItem);
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryReferent(mRootItem);
  item.forget(aEnumerationRootItem);
  return NS_OK;
}

nsresult
nsDocShellEnumerator::SetEnumerationRootItem(
    nsIDocShellTreeItem* aEnumerationRootItem)
{
  mRootItem = do_GetWeakReference(aEnumerationRootItem);
  ClearState();
  return NS_OK;
}

nsresult
nsDocShellEnumerator::GetEnumDocShellType(int32_t* aEnumerationItemType)
{
  NS_ENSURE_ARG_POINTER(aEnumerationItemType);
  *aEnumerationItemType = mDocShellType;
  return NS_OK;
}

nsresult
nsDocShellEnumerator::SetEnumDocShellType(int32_t aEnumerationItemType)
{
  mDocShellType = aEnumerationItemType;
  ClearState();
  return NS_OK;
}

nsresult
nsDocShellEnumerator::First()
{
  mCurIndex = 0;
  return EnsureDocShellArray();
}

nsresult
nsDocShellEnumerator::EnsureDocShellArray()
{
  if (!mArrayValid) {
    mArrayValid = true;
    return BuildDocShellArray(mItemArray);
  }

  return NS_OK;
}

nsresult
nsDocShellEnumerator::ClearState()
{
  mItemArray.Clear();
  mArrayValid = false;
  mCurIndex = 0;
  return NS_OK;
}

nsresult
nsDocShellEnumerator::BuildDocShellArray(nsTArray<nsWeakPtr>& aItemArray)
{
  NS_ENSURE_TRUE(mRootItem, NS_ERROR_NOT_INITIALIZED);
  aItemArray.Clear();
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryReferent(mRootItem);
  return BuildArrayRecursive(item, aItemArray);
}

nsresult
nsDocShellForwardsEnumerator::BuildArrayRecursive(
    nsIDocShellTreeItem* aItem,
    nsTArray<nsWeakPtr>& aItemArray)
{
  nsresult rv;

  // add this item to the array
  if (mDocShellType == nsIDocShellTreeItem::typeAll ||
      aItem->ItemType() == mDocShellType) {
    if (!aItemArray.AppendElement(do_GetWeakReference(aItem))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  int32_t numChildren;
  rv = aItem->GetChildCount(&numChildren);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (int32_t i = 0; i < numChildren; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> curChild;
    rv = aItem->GetChildAt(i, getter_AddRefs(curChild));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = BuildArrayRecursive(curChild, aItemArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
nsDocShellBackwardsEnumerator::BuildArrayRecursive(
    nsIDocShellTreeItem* aItem,
    nsTArray<nsWeakPtr>& aItemArray)
{
  nsresult rv;

  int32_t numChildren;
  rv = aItem->GetChildCount(&numChildren);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (int32_t i = numChildren - 1; i >= 0; --i) {
    nsCOMPtr<nsIDocShellTreeItem> curChild;
    rv = aItem->GetChildAt(i, getter_AddRefs(curChild));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = BuildArrayRecursive(curChild, aItemArray);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // add this item to the array
  if (mDocShellType == nsIDocShellTreeItem::typeAll ||
      aItem->ItemType() == mDocShellType) {
    if (!aItemArray.AppendElement(do_GetWeakReference(aItem))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}
