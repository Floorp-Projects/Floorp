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

#include "mozIStorageConnection.h"
#include "nsIIDBDatabaseException.h"

#include "nsComponentManagerUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "IDBEvents.h"

USING_INDEXEDDB_NAMESPACE

AsyncConnectionHelper::AsyncConnectionHelper(IDBDatabaseRequest* aDatabase,
                                             IDBRequest* aRequest)
: mDatabase(aDatabase),
  mRequest(aRequest),
  mErrorCode(0),
  mError(PR_FALSE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  // mDatabase may be null if we're opening the database.
  NS_ASSERTION(mRequest, "Null request!");
}

AsyncConnectionHelper::~AsyncConnectionHelper()
{
  if (!NS_IsMainThread()) {
    NS_ASSERTION(mErrorCode == NOREPLY,
                 "This should only happen if NOREPLY was returned!");

    IDBDatabaseRequest* database;
    mDatabase.forget(&database);

    IDBRequest* request;
    mRequest.forget(&request);

    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread!");

    if (mainThread) {
      if (database) {
        NS_ProxyRelease(mainThread, static_cast<nsIIDBDatabase*>(database));
      }
      if (request) {
        NS_ProxyRelease(mainThread, static_cast<nsIDOMEventTarget*>(request));
      }
    }
  }

  NS_ASSERTION(!mDatabaseThread, "Should have been released before now!");
}

NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncConnectionHelper, nsIRunnable)

NS_IMETHODIMP
AsyncConnectionHelper::Run()
{
  if (NS_IsMainThread()) {
    if (mError || ((mErrorCode = OnSuccess(mRequest)) != OK)) {
      OnError(mRequest, mErrorCode);
    }

    mDatabase = nsnull;
    mRequest = nsnull;
    return NS_OK;
  }

#ifdef DEBUG
  {
    PRBool ok;
    NS_ASSERTION(NS_SUCCEEDED(mDatabaseThread->IsOnCurrentThread(&ok)) && ok,
                 "Wrong thread!");
    mDatabaseThread = nsnull;
  }
#endif

  mErrorCode = DoDatabaseWork();

  if (mErrorCode != NOREPLY) {
    mError = mErrorCode != OK;

    return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

nsresult
AsyncConnectionHelper::Dispatch(nsIThread* aDatabaseThread)
{
#ifdef DEBUG
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  {
    PRBool sameThread;
    nsresult rv = aDatabaseThread->IsOnCurrentThread(&sameThread);
    NS_ASSERTION(NS_SUCCEEDED(rv), "IsOnCurrentThread failed!");
    NS_ASSERTION(!sameThread, "Dispatching to main thread not supported!");
  }
  mDatabaseThread = aDatabaseThread;
#endif

  nsresult rv = Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return aDatabaseThread->Dispatch(this, NS_DISPATCH_NORMAL);
}

nsresult
AsyncConnectionHelper::Init()
{
  return NS_OK;
}

PRUint16
AsyncConnectionHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  PRUint16 result = GetSuccessResult(variant);
  if (result != OK) {
    return result;
  }

  if (NS_FAILED(variant->SetWritable(PR_FALSE))) {
    NS_ERROR("Failed to make variant readonly!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCOMPtr<nsIDOMEvent> event(IDBSuccessEvent::Create(mRequest, variant));
  if (!event) {
    NS_ERROR("Failed to create event!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  PRBool dummy;
  aTarget->DispatchEvent(event, &dummy);
  return OK;
}

void
AsyncConnectionHelper::OnError(nsIDOMEventTarget* aTarget,
                               PRUint16 aErrorCode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Make an error event and fire it at the target.
  nsCOMPtr<nsIDOMEvent> event(IDBErrorEvent::Create(mRequest, aErrorCode));
  if (!event) {
    NS_ERROR("Failed to create event!");
    return;
  }

  PRBool dummy;
  aTarget->DispatchEvent(event, &dummy);
}

PRUint16
AsyncConnectionHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Leave the variant set to empty.

  return OK;
}
