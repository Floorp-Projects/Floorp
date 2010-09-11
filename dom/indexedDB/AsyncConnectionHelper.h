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

#ifndef mozilla_dom_indexeddb_asyncconnectionhelper_h__
#define mozilla_dom_indexeddb_asyncconnectionhelper_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "IndexedDatabase.h"
#include "IDBDatabase.h"
#include "IDBRequest.h"

#include "mozIStorageProgressHandler.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIVariant.h"

#include "mozilla/TimeStamp.h"

class mozIStorageConnection;

BEGIN_INDEXEDDB_NAMESPACE

class IDBTransaction;

/**
 * Must be subclassed. The subclass must implement DoDatabaseWork. It may then
 * choose to implement OnSuccess and OnError depending on the needs of the
 * subclass. If the default implementation of OnSuccess is desired then the
 * subclass can implement GetSuccessResult to properly set the result of the
 * success event. Call Dispatch to start the database operation. Must be created
 * and Dispatched from the main thread only. Target thread may not be the main
 * thread.
 */
class AsyncConnectionHelper : public nsIRunnable,
                              public mozIStorageProgressHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_MOZISTORAGEPROGRESSHANDLER

  /**
   * Return this code from DoDatabaseWork to signal the main thread that the
   * database operation suceeded. Once the main thread receives this
   * notification the OnSuccess callback will be invoked allowing the subclass
   * to fire a success event or otherwise note the completed operation.
   */
  static const PRUint16 OK = PR_UINT16_MAX;

  nsresult Dispatch(nsIEventTarget* aDatabaseThread);

  // Only for transactions!
  nsresult DispatchToTransactionPool();

  void SetError(PRUint16 aErrorCode)
  {
    mError = true;
    mErrorCode = aErrorCode;
  }

protected:
  AsyncConnectionHelper(IDBDatabase* aDatabase,
                        IDBRequest* aRequest);

  AsyncConnectionHelper(IDBTransaction* aTransaction,
                        IDBRequest* aRequest);

  virtual ~AsyncConnectionHelper();

  /**
   * Set the timeout duration in milliseconds.
   */
  void SetTimeoutMS(PRUint32 aTimeoutMS)
  {
    mTimeoutDuration = TimeDuration::FromMilliseconds(aTimeoutMS);
  }

  /**
   * This is called on the main thread after Dispatch is called but before the
   * runnable is actually dispatched to the database thread. Allows the subclass
   * to initialize itself. The default implementation does nothing and returns
   * NS_OK.
   */
  virtual nsresult Init();

  /**
   * This callback is run on the database thread. It should return a valid error
   * code from nsIIDBDatabaseError or one of the two special values above.
   */
  virtual PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection) = 0;

  /**
   * This callback is run on the main thread if the DoDatabaseWork returned OK.
   * The default implementation constructs an IDBSuccessEvent with its result
   * property set to the value returned by GetSuccessResult. The event is then
   * fired at the target. Returning anything other than OK from the OnSuccess
   * callback will trigger the OnError callback.
   */
  virtual PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

  /**
   * This callback is run on the main thread if DoDatabaseWork returned an error
   * code. The default implementation constructs an IDBErrorEvent for the error
   * code and fires it at the target.
   */
  virtual void OnError(nsIDOMEventTarget* aTarget,
                       PRUint16 aErrorCode);

  /**
   * This function is called from the default implementation of OnSuccess. A
   * valid nsIWritableVariant is passed into the function which can be modified
   * by the subclass. The default implementation returns an nsIVariant that is
   * set to nsIDataType::VTYPE_EMPTY. Returning anything other than OK from the
   * GetSuccessResult function will trigger the OnError callback.
   */
  virtual PRUint16 GetSuccessResult(nsIWritableVariant* aVariant);

  /**
   * Gives the subclass a chance to release any objects that must be released
   * on the main thread, regardless of success or failure. Subclasses that
   * implement this method *MUST* call the base class implementation as well.
   */
  virtual void ReleaseMainThreadObjects();

protected:
  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<IDBTransaction> mTransaction;
  nsRefPtr<IDBRequest> mRequest;

private:
  nsCOMPtr<mozIStorageProgressHandler> mOldProgressHandler;

  mozilla::TimeStamp mStartTime;
  mozilla::TimeDuration mTimeoutDuration;

  PRUint16 mErrorCode;
  PRPackedBool mError;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_asyncconnectionhelper_h__
