/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "IDBTransactionRequest.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"

USING_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBTransactionRequest>
IDBTransactionRequest::Create(IDBDatabaseRequest* aDatabase,
                              nsTArray<ObjectStoreInfo>& aObjectStores,
                              PRUint16 aMode,
                              PRUint32 aTimeout)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBTransactionRequest> transaction = new IDBTransactionRequest();

  transaction->mDatabase = aDatabase;
  transaction->mObjectStores.SwapElements(aObjectStores);
  transaction->mMode = aMode;
  transaction->mTimeout = aTimeout;

  return transaction.forget();
}

IDBTransactionRequest::IDBTransactionRequest()
: mReadyState(nsIIDBTransaction::INITIAL),
  mMode(nsIIDBTransaction::READ_ONLY),
  mTimeout(0),
  mPendingRequests(0)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBTransactionRequest::~IDBTransactionRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mPendingRequests, "Should have no pending requests here!");

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

void
IDBTransactionRequest::OnNewRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!mPendingRequests) {
    mDatabase->DisableConnectionThreadTimeout();

    NS_ASSERTION(mReadyState == nsIIDBTransaction::INITIAL,
                 "Reusing a transaction!");
    mReadyState = nsIIDBTransaction::LOADING;
  }
  ++mPendingRequests;
}

void
IDBTransactionRequest::OnRequestFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mPendingRequests, "Mismatched calls!");
  --mPendingRequests;
  if (!mPendingRequests) {
    NS_ASSERTION(mReadyState == nsIIDBTransaction::LOADING, "Bad readyState!");
    mReadyState = nsIIDBTransaction::DONE;

    Commit();

    mDatabase->EnableConnectionThreadTimeout();
  }
}

nsresult
IDBTransactionRequest::Commit()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mReadyState == nsIIDBTransaction::DONE, "Bad readyState!");

  NS_WARNING("Commit doesn't actually do anything yet! Fix me now!");

  nsCOMPtr<nsIRunnable> runnable =
    IDBEvent::CreateGenericEventRunnable(NS_LITERAL_STRING(COMPLETE_EVT_STR),
                                         this);
  NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

  nsresult rv = NS_DispatchToCurrentThread(runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBTransactionRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBTransactionRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnCompleteListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnTimeoutListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBTransactionRequest,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnCompleteListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnTimeoutListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBTransactionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBTransactionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBTransaction)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBTransactionRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBTransactionRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBTransactionRequest, nsDOMEventTargetHelper)

DOMCI_DATA(IDBTransactionRequest, IDBTransactionRequest)

NS_IMETHODIMP
IDBTransactionRequest::GetDb(nsIIDBDatabase** aDB)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ADDREF(*aDB = mDatabase);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::GetReadyState(PRUint16* aReadyState)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  *aReadyState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::GetMode(PRUint16* aMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::GetObjectStoreNames(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mObjectStores.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mObjectStores[index].name),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::ObjectStore(const nsAString& aName,
                                   nsIIDBObjectStoreRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  ObjectStoreInfo* objectStoreInfo = nsnull;

  PRUint32 count = mObjectStores.Length();
  for (PRUint32 index = 0; index < count; index++) {
    ObjectStoreInfo& info = mObjectStores[index];
    if (info.name == aName) {
      objectStoreInfo = &info;
      break;
    }
  }

  if (!objectStoreInfo) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<IDBObjectStoreRequest> objectStore =
    IDBObjectStoreRequest::Create(mDatabase, this, *objectStoreInfo, mMode);
  NS_ENSURE_TRUE(objectStore, NS_ERROR_FAILURE);

  objectStore.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::Abort()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_WARNING("Abort doesn't actually do anything yet! Fix me now!");

  nsCOMPtr<nsIRunnable> runnable =
    IDBEvent::CreateGenericEventRunnable(NS_LITERAL_STRING(ABORT_EVT_STR),
                                         this);
  NS_ENSURE_TRUE(runnable, NS_ERROR_FAILURE);

  nsresult rv = NS_DispatchToCurrentThread(runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IDBTransactionRequest::GetOncomplete(nsIDOMEventListener** aOncomplete)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return GetInnerEventListener(mOnCompleteListener, aOncomplete);
}

NS_IMETHODIMP
IDBTransactionRequest::SetOncomplete(nsIDOMEventListener* aOncomplete)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return RemoveAddEventListener(NS_LITERAL_STRING(COMPLETE_EVT_STR),
                                mOnCompleteListener, aOncomplete);
}

NS_IMETHODIMP
IDBTransactionRequest::GetOnabort(nsIDOMEventListener** aOnabort)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return GetInnerEventListener(mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
IDBTransactionRequest::SetOnabort(nsIDOMEventListener* aOnabort)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return RemoveAddEventListener(NS_LITERAL_STRING(ABORT_EVT_STR),
                                mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
IDBTransactionRequest::GetOntimeout(nsIDOMEventListener** aOntimeout)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return GetInnerEventListener(mOnTimeoutListener, aOntimeout);
}

NS_IMETHODIMP
IDBTransactionRequest::SetOntimeout(nsIDOMEventListener* aOntimeout)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return RemoveAddEventListener(NS_LITERAL_STRING(TIMEOUT_EVT_STR),
                                mOnTimeoutListener, aOntimeout);
}
