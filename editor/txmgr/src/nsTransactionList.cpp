/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsID.h"
#include "nsISupportsUtils.h"
#include "nsITransactionManager.h"
#include "nsTransactionItem.h"
#include "nsTransactionList.h"
#include "nsTransactionStack.h"
#include "nscore.h"
#include "prtypes.h"

NS_IMPL_ISUPPORTS1(nsTransactionList, nsITransactionList)

nsTransactionList::nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionStack *aTxnStack)
  : mTxnStack(aTxnStack)
  , mTxnItem(0)
{
  if (aTxnMgr)
    mTxnMgr = do_GetWeakReference(aTxnMgr);
}

nsTransactionList::nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionItem *aTxnItem)
  : mTxnStack(0)
  , mTxnItem(aTxnItem)
{
  if (aTxnMgr)
    mTxnMgr = do_GetWeakReference(aTxnMgr);
}

nsTransactionList::~nsTransactionList()
{
  mTxnStack = 0;
  mTxnItem  = 0;
}

/* readonly attribute long numItems; */
NS_IMETHODIMP nsTransactionList::GetNumItems(PRInt32 *aNumItems)
{
  NS_ENSURE_TRUE(aNumItems, NS_ERROR_NULL_POINTER);

  *aNumItems = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  NS_ENSURE_TRUE(txMgr, NS_ERROR_FAILURE);

  nsresult result = NS_OK;

  if (mTxnStack)
    *aNumItems = mTxnStack->GetSize();
  else if (mTxnItem)
    result = mTxnItem->GetNumberOfChildren(aNumItems);

  return result;
}

/* boolean itemIsBatch (in long aIndex); */
NS_IMETHODIMP nsTransactionList::ItemIsBatch(PRInt32 aIndex, bool *aIsBatch)
{
  NS_ENSURE_TRUE(aIsBatch, NS_ERROR_NULL_POINTER);

  *aIsBatch = false;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  NS_ENSURE_TRUE(txMgr, NS_ERROR_FAILURE);

  nsRefPtr<nsTransactionItem> item;

  nsresult result = NS_OK;

  if (mTxnStack)
    item = mTxnStack->GetItem(aIndex);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, getter_AddRefs(item));

  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  return item->GetIsBatch(aIsBatch);
}

/* nsITransaction getItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetItem(PRInt32 aIndex, nsITransaction **aItem)
{
  NS_ENSURE_TRUE(aItem, NS_ERROR_NULL_POINTER);

  *aItem = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  NS_ENSURE_TRUE(txMgr, NS_ERROR_FAILURE);

  nsRefPtr<nsTransactionItem> item;

  nsresult result = NS_OK;

  if (mTxnStack)
    item = mTxnStack->GetItem(aIndex);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, getter_AddRefs(item));

  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  *aItem = item->GetTransaction().get();

  return NS_OK;
}

/* long getNumChildrenForItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetNumChildrenForItem(PRInt32 aIndex, PRInt32 *aNumChildren)
{
  NS_ENSURE_TRUE(aNumChildren, NS_ERROR_NULL_POINTER);

  *aNumChildren = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  NS_ENSURE_TRUE(txMgr, NS_ERROR_FAILURE);

  nsRefPtr<nsTransactionItem> item;

  nsresult result = NS_OK;

  if (mTxnStack)
    item = mTxnStack->GetItem(aIndex);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, getter_AddRefs(item));

  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  return item->GetNumberOfChildren(aNumChildren);
}

/* nsITransactionList getChildListForItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetChildListForItem(PRInt32 aIndex, nsITransactionList **aTxnList)
{
  NS_ENSURE_TRUE(aTxnList, NS_ERROR_NULL_POINTER);

  *aTxnList = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  NS_ENSURE_TRUE(txMgr, NS_ERROR_FAILURE);

  nsRefPtr<nsTransactionItem> item;

  nsresult result = NS_OK;

  if (mTxnStack)
    item = mTxnStack->GetItem(aIndex);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, getter_AddRefs(item));

  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

  *aTxnList = (nsITransactionList *)new nsTransactionList(txMgr, item);

  NS_ENSURE_TRUE(*aTxnList, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aTxnList);

  return NS_OK;
}

