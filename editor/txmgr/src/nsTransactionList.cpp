/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsITransactionManager.h"
#include "nsTransactionItem.h"
#include "nsTransactionList.h"
#include "nsTransactionStack.h"

NS_IMPL_ISUPPORTS1(nsTransactionList, nsITransactionList)

nsTransactionList::nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionStack *aTxnStack)
  : mTxnStack(aTxnStack)
  , mTxnItem(0)
{
  NS_INIT_ISUPPORTS();

  if (aTxnMgr)
    mTxnMgr = getter_AddRefs(NS_GetWeakReference(aTxnMgr));
}

nsTransactionList::nsTransactionList(nsITransactionManager *aTxnMgr, nsTransactionItem *aTxnItem)
  : mTxnStack(0)
  , mTxnItem(aTxnItem)
{
  NS_INIT_ISUPPORTS();

  if (aTxnMgr)
    mTxnMgr = getter_AddRefs(NS_GetWeakReference(aTxnMgr));
}

nsTransactionList::~nsTransactionList()
{
  mTxnStack = 0;
  mTxnItem  = 0;
}

/* readonly attribute long numItems; */
NS_IMETHODIMP nsTransactionList::GetNumItems(PRInt32 *aNumItems)
{
  if (!aNumItems)
    return NS_ERROR_NULL_POINTER;

  *aNumItems = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  if (!txMgr)
    return NS_ERROR_FAILURE;

  nsresult result = NS_ERROR_FAILURE;

  if (mTxnStack)
    result = mTxnStack->GetSize(aNumItems);
  else if (mTxnItem)
    result = mTxnItem->GetNumberOfChildren(aNumItems);

  return result;
}

/* boolean itemIsBatch (in long aIndex); */
NS_IMETHODIMP nsTransactionList::ItemIsBatch(PRInt32 aIndex, PRBool *aIsBatch)
{
  if (!aIsBatch)
    return NS_ERROR_NULL_POINTER;

  *aIsBatch = PR_FALSE;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  if (!txMgr)
    return NS_ERROR_FAILURE;

  nsTransactionItem *item = 0;

  nsresult result = NS_ERROR_FAILURE;

  if (mTxnStack)
    result = mTxnStack->GetItem(aIndex, &item);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, &item);

  if (NS_FAILED(result))
    return result;

  if (!item)
    return NS_ERROR_FAILURE;

  return item->GetIsBatch(aIsBatch);
}

/* nsITransaction getItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetItem(PRInt32 aIndex, nsITransaction **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;

  *aItem = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  if (!txMgr)
    return NS_ERROR_FAILURE;

  nsTransactionItem *item = 0;

  nsresult result = NS_ERROR_FAILURE;

  if (mTxnStack)
    result = mTxnStack->GetItem(aIndex, &item);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, &item);

  if (NS_FAILED(result))
    return result;

  if (!item)
    return NS_ERROR_FAILURE;

  result = item->GetTransaction(aItem);

  if (NS_FAILED(result))
    return result;

  NS_IF_ADDREF(*aItem);

  return NS_OK;
}

/* long getNumChildrenForItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetNumChildrenForItem(PRInt32 aIndex, PRInt32 *aNumChildren)
{
  if (!aNumChildren)
    return NS_ERROR_NULL_POINTER;

  *aNumChildren = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  if (!txMgr)
    return NS_ERROR_FAILURE;

  nsTransactionItem *item = 0;

  nsresult result = NS_ERROR_FAILURE;

  if (mTxnStack)
    result = mTxnStack->GetItem(aIndex, &item);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, &item);

  if (NS_FAILED(result))
    return result;

  if (!item)
    return NS_ERROR_FAILURE;

  return item->GetNumberOfChildren(aNumChildren);
}

/* nsITransactionList getChildListForItem (in long aIndex); */
NS_IMETHODIMP nsTransactionList::GetChildListForItem(PRInt32 aIndex, nsITransactionList **aTxnList)
{
  if (!aTxnList)
    return NS_ERROR_NULL_POINTER;

  *aTxnList = 0;

  nsCOMPtr<nsITransactionManager> txMgr = do_QueryReferent(mTxnMgr);

  if (!txMgr)
    return NS_ERROR_FAILURE;

  nsTransactionItem *item = 0;

  nsresult result = NS_ERROR_FAILURE;

  if (mTxnStack)
    result = mTxnStack->GetItem(aIndex, &item);
  else if (mTxnItem)
    result = mTxnItem->GetChild(aIndex, &item);

  if (NS_FAILED(result))
    return result;

  if (!item)
    return NS_ERROR_FAILURE;

  *aTxnList = (nsITransactionList *)new nsTransactionList(txMgr, item);

  if (!*aTxnList)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aTxnList);

  return NS_OK;
}

