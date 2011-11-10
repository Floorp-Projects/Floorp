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

#include "AsyncConnectionHelper.h"

#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseManager.h"
#include "TransactionThreadPool.h"

USING_INDEXEDDB_NAMESPACE

namespace {

IDBTransaction* gCurrentTransaction = nsnull;

const PRUint32 kProgressHandlerGranularity = 1000;

NS_STACK_CLASS
class TransactionPoolEventTarget : public nsIEventTarget
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTTARGET

  TransactionPoolEventTarget(IDBTransaction* aTransaction)
  : mTransaction(aTransaction)
  { }

private:
  IDBTransaction* mTransaction;
};

// This inline is just so that we always clear aBuffers appropriately even if
// something fails.
inline
nsresult
ConvertCloneBuffersToArrayInternal(
                                JSContext* aCx,
                                nsTArray<JSAutoStructuredCloneBuffer>& aBuffers,
                                jsval* aResult)
{
  JSObject* array = JS_NewArrayObject(aCx, 0, nsnull);
  if (!array) {
    NS_WARNING("Failed to make array!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (!aBuffers.IsEmpty()) {
    if (!JS_SetArrayLength(aCx, array, jsuint(aBuffers.Length()))) {
      NS_WARNING("Failed to set array length!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32 index = 0, count = aBuffers.Length(); index < count; index++) {
      JSAutoStructuredCloneBuffer& buffer = aBuffers[index];

      jsval val;
      if (!IDBObjectStore::DeserializeValue(aCx, buffer, &val)) {
        NS_WARNING("Failed to decode!");
        return NS_ERROR_DOM_DATA_CLONE_ERR;
      }

      if (!JS_SetElement(aCx, array, index, &val)) {
        NS_WARNING("Failed to set array element!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
  }

  *aResult = OBJECT_TO_JSVAL(array);
  return NS_OK;
}

} // anonymous namespace

HelperBase::~HelperBase()
{
  if (!NS_IsMainThread()) {
    IDBRequest* request;
    mRequest.forget(&request);

    if (request) {
      nsCOMPtr<nsIThread> mainThread;
      NS_GetMainThread(getter_AddRefs(mainThread));
      NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread!");

      if (mainThread) {
        NS_ProxyRelease(mainThread, static_cast<nsIDOMEventTarget*>(request));
      }
    }
  }
}

nsresult
HelperBase::WrapNative(JSContext* aCx,
                       nsISupports* aNative,
                       jsval* aResult)
{
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aNative, "Null pointer!");
  NS_ASSERTION(aResult, "Null pointer!");
  NS_ASSERTION(mRequest, "Null request!");

  JSObject* global = mRequest->ScriptContext()->GetNativeGlobal();
  NS_ENSURE_TRUE(global, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv =
    nsContentUtils::WrapNative(aCx, global, aNative, aResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

void
HelperBase::ReleaseMainThreadObjects()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mRequest = nsnull;
}

AsyncConnectionHelper::AsyncConnectionHelper(IDBDatabase* aDatabase,
                                             IDBRequest* aRequest)
: HelperBase(aRequest),
  mDatabase(aDatabase),
  mResultCode(NS_OK),
  mDispatched(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

AsyncConnectionHelper::AsyncConnectionHelper(IDBTransaction* aTransaction,
                                             IDBRequest* aRequest)
: HelperBase(aRequest),
  mDatabase(aTransaction->mDatabase),
  mTransaction(aTransaction),
  mResultCode(NS_OK),
  mDispatched(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

AsyncConnectionHelper::~AsyncConnectionHelper()
{
  if (!NS_IsMainThread()) {
    IDBDatabase* database;
    mDatabase.forget(&database);

    IDBTransaction* transaction;
    mTransaction.forget(&transaction);

    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread!");

    if (mainThread) {
      if (database) {
        NS_ProxyRelease(mainThread, static_cast<nsIIDBDatabase*>(database));
      }
      if (transaction) {
        NS_ProxyRelease(mainThread,
                        static_cast<nsIIDBTransaction*>(transaction));
      }
    }
  }

  NS_ASSERTION(!mOldProgressHandler, "Should not have anything here!");
}

NS_IMPL_THREADSAFE_ISUPPORTS2(AsyncConnectionHelper, nsIRunnable,
                                                     mozIStorageProgressHandler)

NS_IMETHODIMP
AsyncConnectionHelper::Run()
{
  if (NS_IsMainThread()) {
    if (mTransaction &&
        mTransaction->IsAborted()) {
      // Always fire a "error" event with ABORT_ERR if the transaction was
      // aborted, even if the request succeeded or failed with another error.
      mResultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
    }

    IDBTransaction* oldTransaction = gCurrentTransaction;
    gCurrentTransaction = mTransaction;

    if (mRequest) {
      nsresult rv = mRequest->NotifyHelperCompleted(this);
      if (NS_SUCCEEDED(mResultCode) && NS_FAILED(rv)) {
        mResultCode = rv;
      }
    }

    // Call OnError if the database had an error or if the OnSuccess handler
    // has an error.
    if (NS_FAILED(mResultCode) ||
        NS_FAILED((mResultCode = OnSuccess()))) {
      OnError();
    }

    NS_ASSERTION(gCurrentTransaction == mTransaction, "Should be unchanged!");
    gCurrentTransaction = oldTransaction;

    if (mDispatched && mTransaction) {
      mTransaction->OnRequestFinished();
    }

    ReleaseMainThreadObjects();

    NS_ASSERTION(!(mDatabase || mTransaction || mRequest), "Subclass didn't "
                 "call AsyncConnectionHelper::ReleaseMainThreadObjects!");

    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<mozIStorageConnection> connection;

  if (mTransaction) {
    rv = mTransaction->GetOrCreateConnection(getter_AddRefs(connection));
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(connection, "This should never be null!");
    }
  }

  bool setProgressHandler = false;
  if (connection) {
    rv = connection->SetProgressHandler(kProgressHandlerGranularity, this,
                                        getter_AddRefs(mOldProgressHandler));
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "SetProgressHandler failed!");
    if (NS_SUCCEEDED(rv)) {
      setProgressHandler = true;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    bool hasSavepoint = false;
    if (mDatabase) {
      IndexedDatabaseManager::SetCurrentDatabase(mDatabase);

      // Make the first savepoint.
      if (mTransaction) {
        if (!(hasSavepoint = mTransaction->StartSavepoint())) {
          NS_WARNING("Failed to make savepoint!");
        }
      }
    }

    mResultCode = DoDatabaseWork(connection);

    if (mDatabase) {
      IndexedDatabaseManager::SetCurrentDatabase(nsnull);

      // Release or roll back the savepoint depending on the error code.
      if (hasSavepoint) {
        NS_ASSERTION(mTransaction, "Huh?!");
        if (NS_SUCCEEDED(mResultCode)) {
          mTransaction->ReleaseSavepoint();
        }
        else {
          mTransaction->RollbackSavepoint();
        }
      }
    }
  }
  else {
    // NS_ERROR_NOT_AVAILABLE is our special code for "database is invalidated"
    // and we should fail with RECOVERABLE_ERR.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      mResultCode = NS_ERROR_DOM_INDEXEDDB_RECOVERABLE_ERR;
    }
    else {
      mResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  if (setProgressHandler) {
    nsCOMPtr<mozIStorageProgressHandler> handler;
    rv = connection->RemoveProgressHandler(getter_AddRefs(handler));
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "RemoveProgressHandler failed!");
#ifdef DEBUG
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(SameCOMIdentity(handler, static_cast<nsIRunnable*>(this)),
                   "Mismatch!");
    }
#endif
  }

  return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
AsyncConnectionHelper::OnProgress(mozIStorageConnection* aConnection,
                                  bool* _retval)
{
  if (mDatabase && mDatabase->IsInvalidated()) {
    // Someone is trying to delete the database file. Exit lightningfast!
    *_retval = true;
    return NS_OK;
  }

  if (mOldProgressHandler) {
    return mOldProgressHandler->OnProgress(aConnection, _retval);
  }

  *_retval = false;
  return NS_OK;
}

nsresult
AsyncConnectionHelper::Dispatch(nsIEventTarget* aDatabaseThread)
{
#ifdef DEBUG
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  {
    bool sameThread;
    nsresult rv = aDatabaseThread->IsOnCurrentThread(&sameThread);
    NS_ASSERTION(NS_SUCCEEDED(rv), "IsOnCurrentThread failed!");
    NS_ASSERTION(!sameThread, "Dispatching to main thread not supported!");
  }
#endif

  nsresult rv = Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aDatabaseThread->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mTransaction) {
    mTransaction->OnNewRequest();
  }

  mDispatched = true;

  return NS_OK;
}

nsresult
AsyncConnectionHelper::DispatchToTransactionPool()
{
  NS_ASSERTION(mTransaction, "Only ok to call this with a transaction!");
  TransactionPoolEventTarget target(mTransaction);
  return Dispatch(&target);
}

// static
IDBTransaction*
AsyncConnectionHelper::GetCurrentTransaction()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return gCurrentTransaction;
}

nsresult
AsyncConnectionHelper::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return NS_OK;
}

already_AddRefed<nsDOMEvent>
AsyncConnectionHelper::CreateSuccessEvent()
{
  return CreateGenericEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR));
}

nsresult
AsyncConnectionHelper::OnSuccess()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mRequest, "Null request!");

  nsRefPtr<nsDOMEvent> event = CreateSuccessEvent();
  if (!event) {
    NS_ERROR("Failed to create event!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  bool dummy;
  nsresult rv = mRequest->DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsEvent* internalEvent = event->GetInternalNSEvent();
  NS_ASSERTION(internalEvent, "This should never be null!");

  NS_ASSERTION(!mTransaction ||
               mTransaction->IsOpen() ||
               mTransaction->IsAborted(),
               "How else can this be closed?!");

  if ((internalEvent->flags & NS_EVENT_FLAG_EXCEPTION_THROWN) &&
      mTransaction &&
      mTransaction->IsOpen()) {
    rv = mTransaction->Abort();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
AsyncConnectionHelper::OnError()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mRequest, "Null request!");

  // Make an error event and fire it at the target.
  nsRefPtr<nsDOMEvent> event =
    CreateGenericEvent(NS_LITERAL_STRING(ERROR_EVT_STR), true);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  bool doDefault;
  nsresult rv = mRequest->DispatchEvent(event, &doDefault);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(!mTransaction ||
                 mTransaction->IsOpen() ||
                 mTransaction->IsAborted(),
                 "How else can this be closed?!");

    if (doDefault &&
        mTransaction &&
        mTransaction->IsOpen() &&
        NS_FAILED(mTransaction->Abort())) {
      NS_WARNING("Failed to abort transaction!");
    }
  }
  else {
    NS_WARNING("DispatchEvent failed!");
  }
}

nsresult
AsyncConnectionHelper::GetSuccessResult(JSContext* aCx,
                                        jsval* aVal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aVal = JSVAL_VOID;
  return NS_OK;
}

void
AsyncConnectionHelper::ReleaseMainThreadObjects()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mDatabase = nsnull;
  mTransaction = nsnull;

  HelperBase::ReleaseMainThreadObjects();
}

// static
nsresult
AsyncConnectionHelper::ConvertCloneBuffersToArray(
                                JSContext* aCx,
                                nsTArray<JSAutoStructuredCloneBuffer>& aBuffers,
                                jsval* aResult)
{
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aResult, "Null pointer!");

  JSAutoRequest ar(aCx);

  nsresult rv = ConvertCloneBuffersToArrayInternal(aCx, aBuffers, aResult);

  for (PRUint32 index = 0; index < aBuffers.Length(); index++) {
    aBuffers[index].clear();
  }
  aBuffers.Clear();

  return rv;
}

NS_IMETHODIMP_(nsrefcnt)
TransactionPoolEventTarget::AddRef()
{
  NS_NOTREACHED("Don't call me!");
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
TransactionPoolEventTarget::Release()
{
  NS_NOTREACHED("Don't call me!");
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(TransactionPoolEventTarget, nsIEventTarget)

NS_IMETHODIMP
TransactionPoolEventTarget::Dispatch(nsIRunnable* aRunnable,
                                     PRUint32 aFlags)
{
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL, "Unsupported!");

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  return pool->Dispatch(mTransaction, aRunnable, false, nsnull);
}

NS_IMETHODIMP
TransactionPoolEventTarget::IsOnCurrentThread(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}
