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

#ifndef mozilla_dom_indexeddb_idbevents_h__
#define mozilla_dom_indexeddb_idbevents_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBEvent.h"
#include "nsIIDBErrorEvent.h"
#include "nsIIDBSuccessEvent.h"
#include "nsIIDBTransactionEvent.h"
#include "nsIIDBTransaction.h"
#include "nsIRunnable.h"
#include "nsIVariant.h"

#include "nsDOMEvent.h"

#include "mozilla/dom/indexedDB/IDBObjectStore.h"

#define SUCCESS_EVT_STR "success"
#define ERROR_EVT_STR "error"
#define COMPLETE_EVT_STR "complete"
#define ABORT_EVT_STR "abort"
#define TIMEOUT_EVT_STR "timeout"

BEGIN_INDEXEDDB_NAMESPACE

class IDBRequest;
class IDBTransaction;

class IDBEvent : public nsDOMEvent,
                 public nsIIDBEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBEVENT
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBEvent, nsDOMEvent)

  static already_AddRefed<nsIDOMEvent>
  CreateGenericEvent(const nsAString& aType);

  static already_AddRefed<nsIRunnable>
  CreateGenericEventRunnable(const nsAString& aType,
                             nsIDOMEventTarget* aTarget);

protected:
  IDBEvent() : nsDOMEvent(nsnull, nsnull) { }
  virtual ~IDBEvent() { }

  nsCOMPtr<nsISupports> mSource;
};

class IDBErrorEvent : public IDBEvent,
                      public nsIIDBErrorEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBERROREVENT
  NS_FORWARD_NSIDOMEVENT(IDBEvent::)
  NS_FORWARD_NSIIDBEVENT(IDBEvent::)

  static already_AddRefed<nsIDOMEvent>
  Create(IDBRequest* aRequest,
         PRUint16 aCode);

  static already_AddRefed<nsIRunnable>
  CreateRunnable(IDBRequest* aRequest,
                 PRUint16 aCode);

protected:
  IDBErrorEvent() { }

  PRUint16 mCode;
  nsString mMessage;
};

class IDBSuccessEvent : public IDBEvent,
                        public nsIIDBTransactionEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBSUCCESSEVENT
  NS_DECL_NSIIDBTRANSACTIONEVENT
  NS_FORWARD_NSIDOMEVENT(IDBEvent::)
  NS_FORWARD_NSIIDBEVENT(IDBEvent::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBSuccessEvent, IDBEvent)

  static already_AddRefed<nsIDOMEvent>
  Create(IDBRequest* aRequest,
         nsIVariant* aResult,
         nsIIDBTransaction* aTransaction);

  static already_AddRefed<nsIRunnable>
  CreateRunnable(IDBRequest* aRequest,
                 nsIVariant* aResult,
                 nsIIDBTransaction* aTransaction);

protected:
  IDBSuccessEvent() { }

  nsCOMPtr<nsIVariant> mResult;
  nsCOMPtr<nsIIDBTransaction> mTransaction;
};

class GetSuccessEvent : public IDBSuccessEvent
{
public:
  GetSuccessEvent(const nsAString& aValue)
  : mValue(aValue),
    mCachedValue(JSVAL_VOID),
    mValueRooted(PR_FALSE)
  { }

  ~GetSuccessEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(GetSuccessEvent,
                                                         IDBSuccessEvent)

  NS_IMETHOD GetResult(JSContext* aCx,
                       jsval* aResult);

  nsresult Init(IDBRequest* aRequest,
                IDBTransaction* aTransaction);

private:
  nsString mValue;

protected:
  void RootCachedValue();

  jsval mCachedValue;
  JSRuntime* mJSRuntime;
  PRBool mValueRooted;
};

class GetAllSuccessEvent : public GetSuccessEvent
{
public:
  GetAllSuccessEvent(nsTArray<nsString>& aValues)
  : GetSuccessEvent(EmptyString())
  {
    if (!mValues.SwapElements(aValues)) {
      NS_ERROR("Failed to swap elements!");
    }
  }

  NS_IMETHOD GetResult(JSContext* aCx,
                       jsval* aResult);

private:
  nsTArray<nsString> mValues;
};

class GetAllKeySuccessEvent : public GetSuccessEvent
{
public:
  GetAllKeySuccessEvent(nsTArray<Key>& aKeys)
  : GetSuccessEvent(EmptyString())
  {
    if (!mKeys.SwapElements(aKeys)) {
      NS_ERROR("Failed to swap elements!");
    }
  }

  NS_IMETHOD GetResult(JSContext* aCx,
                       jsval* aResult);

private:
  nsTArray<Key> mKeys;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbevents_h__
