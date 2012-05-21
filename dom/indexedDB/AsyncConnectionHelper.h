/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_asyncconnectionhelper_h__
#define mozilla_dom_indexeddb_asyncconnectionhelper_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "DatabaseInfo.h"
#include "IndexedDatabase.h"
#include "IDBDatabase.h"
#include "IDBRequest.h"

#include "mozIStorageProgressHandler.h"
#include "nsIRunnable.h"

#include "nsDOMEvent.h"

class mozIStorageConnection;
class nsIEventTarget;

BEGIN_INDEXEDDB_NAMESPACE

class IDBTransaction;

// A common base class for AsyncConnectionHelper and OpenDatabaseHelper that
// IDBRequest can use.
class HelperBase : public nsIRunnable
{
  friend class IDBRequest;
public:
  virtual nsresult GetResultCode() = 0;

  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal) = 0;

protected:
  HelperBase(IDBRequest* aRequest)
    : mRequest(aRequest)
  { }

  virtual ~HelperBase();

  /**
   * Helper to wrap a native into a jsval. Uses the global object of the request
   * to parent the native.
   */
  nsresult WrapNative(JSContext* aCx,
                      nsISupports* aNative,
                      jsval* aResult);

  /**
   * Gives the subclass a chance to release any objects that must be released
   * on the main thread, regardless of success or failure. Subclasses that
   * implement this method *MUST* call the base class implementation as well.
   */
  virtual void ReleaseMainThreadObjects();

  nsRefPtr<IDBRequest> mRequest;
};

/**
 * Must be subclassed. The subclass must implement DoDatabaseWork. It may then
 * choose to implement OnSuccess and OnError depending on the needs of the
 * subclass. If the default implementation of OnSuccess is desired then the
 * subclass can implement GetSuccessResult to properly set the result of the
 * success event. Call Dispatch to start the database operation. Must be created
 * and Dispatched from the main thread only. Target thread may not be the main
 * thread.
 */
class AsyncConnectionHelper : public HelperBase,
                              public mozIStorageProgressHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_MOZISTORAGEPROGRESSHANDLER

  nsresult Dispatch(nsIEventTarget* aDatabaseThread);

  // Only for transactions!
  nsresult DispatchToTransactionPool();

  void SetError(nsresult aErrorCode)
  {
    NS_ASSERTION(NS_FAILED(aErrorCode), "Not a failure code!");
    mResultCode = aErrorCode;
  }

  static IDBTransaction* GetCurrentTransaction();

  bool HasTransaction()
  {
    return mTransaction;
  }

  nsISupports* GetSource()
  {
    return mRequest ? mRequest->Source() : nsnull;
  }

  nsresult GetResultCode()
  {
    return mResultCode;
  }

protected:
  AsyncConnectionHelper(IDBDatabase* aDatabase,
                        IDBRequest* aRequest);

  AsyncConnectionHelper(IDBTransaction* aTransaction,
                        IDBRequest* aRequest);

  virtual ~AsyncConnectionHelper();

  /**
   * This is called on the main thread after Dispatch is called but before the
   * runnable is actually dispatched to the database thread. Allows the subclass
   * to initialize itself.
   */
  virtual nsresult Init();

  /**
   * This callback is run on the database thread.
   */
  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection) = 0;

  /**
   * This function returns the event to be dispatched at the request when
   * OnSuccess is called.  A subclass can override this to fire an event other
   * than "success" at the request.
   */
  virtual already_AddRefed<nsDOMEvent> CreateSuccessEvent();

  /**
   * This callback is run on the main thread if DoDatabaseWork returned NS_OK.
   * The default implementation fires a "success" DOM event with its target set
   * to the request. Returning anything other than NS_OK from the OnSuccess
   * callback will trigger the OnError callback.
   */
  virtual nsresult OnSuccess();

  /**
   * This callback is run on the main thread if DoDatabaseWork or OnSuccess
   * returned an error code. The default implementation fires an "error" DOM
   * event with its target set to the request.
   */
  virtual void OnError();

  /**
   * This function is called by the request on the main thread when script
   * accesses the result property of the request.
   */
  virtual nsresult GetSuccessResult(JSContext* aCx,
                                    jsval* aVal);

  /**
   * Gives the subclass a chance to release any objects that must be released
   * on the main thread, regardless of success or failure. Subclasses that
   * implement this method *MUST* call the base class implementation as well.
   */
  virtual void ReleaseMainThreadObjects();

  /**
   * Helper to make a JS array object out of an array of clone buffers.
   */
  static nsresult ConvertCloneReadInfosToArray(
                                JSContext* aCx,
                                nsTArray<StructuredCloneReadInfo>& aReadInfos,
                                jsval* aResult);

protected:
  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<IDBTransaction> mTransaction;

private:
  nsCOMPtr<mozIStorageProgressHandler> mOldProgressHandler;
  nsresult mResultCode;
  bool mDispatched;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_asyncconnectionhelper_h__
