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

#include "IDBEvents.h"

#include "nsIIDBDatabaseException.h"
#include "nsIPrivateDOMEvent.h"

#include "jscntxt.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSON.h"
#include "nsThreadUtils.h"

#include "IDBRequest.h"
#include "IDBTransaction.h"

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(_class, _condition)  \
  if ((_condition) && (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||               \
                       aIID.Equals(NS_GET_IID(nsXPCClassInfo)))) {            \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

USING_INDEXEDDB_NAMESPACE

namespace {

template<class Class>
inline
nsIDOMEvent*
idomevent_cast(Class* aClassPtr)
{
  return static_cast<nsIDOMEvent*>(static_cast<nsDOMEvent*>(aClassPtr));
}

template<class Class>
inline
nsIDOMEvent*
idomevent_cast(nsRefPtr<Class> aClassAutoPtr)
{
  return idomevent_cast(aClassAutoPtr.get());
}

class EventFiringRunnable : public nsRunnable
{
public:
  EventFiringRunnable(nsIDOMEventTarget* aTarget,
                      nsIDOMEvent* aEvent)
  : mTarget(aTarget), mEvent(aEvent)
  { }

  NS_IMETHOD Run() {
    PRBool dummy;
    return mTarget->DispatchEvent(mEvent, &dummy);
  }

private:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
};

void
GetMessageForErrorCode(PRUint16 aCode,
                       nsAString& aMessage)
{
  switch (aCode) {
    case nsIIDBDatabaseException::NON_TRANSIENT_ERR:
      aMessage.AssignLiteral("This error occurred because an operation was not "
                             "allowed on an object. A retry of the same "
                             "operation would fail unless the cause of the "
                             "error is corrected.");
      break;
    case nsIIDBDatabaseException::NOT_FOUND_ERR:
      aMessage.AssignLiteral("The operation failed because the requested "
                             "database object could not be found. For example, "
                             "an object store did not exist but was being "
                             "opened.");
      break;
    case nsIIDBDatabaseException::CONSTRAINT_ERR:
      aMessage.AssignLiteral("A mutation operation in the transaction failed "
                             "due to a because a constraint was not satisfied. "
                             "For example, an object such as an object store "
                             "or index already exists and a new one was being "
                             "attempted to be created.");
      break;
    case nsIIDBDatabaseException::DATA_ERR:
      aMessage.AssignLiteral("Data provided to an operation does not meet "
                             "requirements.");
      break;
    case nsIIDBDatabaseException::NOT_ALLOWED_ERR:
      aMessage.AssignLiteral("A mutation operation was attempted on a database "
                             "that did not allow mutations.");
      break;
    case nsIIDBDatabaseException::SERIAL_ERR:
      aMessage.AssignLiteral("The operation failed because of the size of the "
                             "data set being returned or because there was a "
                             "problem in serializing or deserializing the "
                             "object being processed.");
      break;
    case nsIIDBDatabaseException::RECOVERABLE_ERR:
      aMessage.AssignLiteral("The operation failed because the database was "
                             "prevented from taking an action. The operation "
                             "might be able to succeed if the application "
                             "performs some recovery steps and retries the "
                             "entire transaction. For example, there was not "
                             "enough remaining storage space, or the storage "
                             "quota was reached and the user declined to give "
                             "more space to the database.");
      break;
    case nsIIDBDatabaseException::TRANSIENT_ERR:
      aMessage.AssignLiteral("The operation failed because of some temporary "
                             "problems. The failed operation might be able to "
                             "succeed when the operation is retried without "
                             "any intervention by application-level "
                             "functionality.");
      break;
    case nsIIDBDatabaseException::TIMEOUT_ERR:
      aMessage.AssignLiteral("A lock for the transaction could not be obtained "
                             "in a reasonable time.");
      break;
    case nsIIDBDatabaseException::DEADLOCK_ERR:
      aMessage.AssignLiteral("The current transaction was automatically rolled "
                             "back by the database becuase of deadlock or "
                             "other transaction serialization failures.");
      break;
    case nsIIDBDatabaseException::UNKNOWN_ERR:
      // Fall through.
    default:
      aMessage.AssignLiteral("The operation failed for reasons unrelated to "
                             "the database itself and not covered by any other "
                             "error code.");
  }
}

} // anonymous namespace

// static
already_AddRefed<nsIDOMEvent>
IDBEvent::CreateGenericEvent(const nsAString& aType)
{
  nsRefPtr<nsDOMEvent> event(new nsDOMEvent(nsnull, nsnull));
  nsresult rv = event->InitEvent(aType, PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsDOMEvent* result;
  event.forget(&result);
  return result;
}

// static
already_AddRefed<nsIRunnable>
IDBEvent::CreateGenericEventRunnable(const nsAString& aType,
                                     nsIDOMEventTarget* aTarget)
{
  nsCOMPtr<nsIDOMEvent> event(IDBEvent::CreateGenericEvent(aType));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(IDBEvent, nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
IDBEvent::GetSource(nsISupports** aSource)
{
  nsCOMPtr<nsISupports> source(mSource);
  source.forget(aSource);
  return NS_OK;
}

// static
already_AddRefed<nsIDOMEvent>
IDBErrorEvent::Create(IDBRequest* aRequest,
                      PRUint16 aCode)
{
  nsRefPtr<IDBErrorEvent> event(new IDBErrorEvent());

  event->mSource = aRequest->Source();
  event->mCode = aCode;
  GetMessageForErrorCode(aCode, event->mMessage);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING(ERROR_EVT_STR), PR_FALSE,
                                 PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  IDBErrorEvent* result;
  event.forget(&result);
  return idomevent_cast(result);
}

// static
already_AddRefed<nsIRunnable>
IDBErrorEvent::CreateRunnable(IDBRequest* aRequest,
                              PRUint16 aCode)
{
  nsCOMPtr<nsIDOMEvent> event(IDBErrorEvent::Create(aRequest, aCode));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aRequest, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBErrorEvent, IDBEvent)
NS_IMPL_RELEASE_INHERITED(IDBErrorEvent, IDBEvent)

NS_INTERFACE_MAP_BEGIN(IDBErrorEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBErrorEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBErrorEvent)
NS_INTERFACE_MAP_END_INHERITING(IDBEvent)

DOMCI_DATA(IDBErrorEvent, IDBErrorEvent)

NS_IMETHODIMP
IDBErrorEvent::GetCode(PRUint16* aCode)
{
  *aCode = mCode;
  return NS_OK;
}

NS_IMETHODIMP
IDBErrorEvent::SetCode(PRUint16 aCode)
{
  mCode = aCode;
  return NS_OK;
}

NS_IMETHODIMP
IDBErrorEvent::GetMessage(nsAString& aMessage)
{
  aMessage.Assign(mMessage);
  return NS_OK;
}

NS_IMETHODIMP
IDBErrorEvent::SetMessage(const nsAString& aMessage)
{
  mMessage.Assign(aMessage);
  return NS_OK;
}

// static
already_AddRefed<nsIDOMEvent>
IDBSuccessEvent::Create(IDBRequest* aRequest,
                        nsIVariant* aResult,
                        nsIIDBTransaction* aTransaction)
{
  nsRefPtr<IDBSuccessEvent> event(new IDBSuccessEvent());

  event->mSource = aRequest->Source();
  event->mResult = aResult;
  event->mTransaction = aTransaction;

  nsresult rv = event->InitEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR), PR_FALSE,
                                 PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  IDBSuccessEvent* result;
  event.forget(&result);
  return idomevent_cast(result);
}

// static
already_AddRefed<nsIRunnable>
IDBSuccessEvent::CreateRunnable(IDBRequest* aRequest,
                                nsIVariant* aResult,
                                nsIIDBTransaction* aTransaction)
{
  nsCOMPtr<nsIDOMEvent> event =
    IDBSuccessEvent::Create(aRequest, aResult, aTransaction);
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aRequest, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBSuccessEvent, IDBEvent)
NS_IMPL_RELEASE_INHERITED(IDBSuccessEvent, IDBEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBSuccessEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBSuccessEvent, IDBEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResult)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTransaction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBSuccessEvent, IDBEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mResult)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTransaction)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBSuccessEvent)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIDBTransactionEvent, mTransaction)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(IDBTransactionEvent,
                                                   mTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIIDBSuccessEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBSuccessEvent)
NS_INTERFACE_MAP_END_INHERITING(IDBEvent)

DOMCI_DATA(IDBSuccessEvent, IDBSuccessEvent)
DOMCI_DATA(IDBTransactionEvent, IDBSuccessEvent)

NS_IMETHODIMP
IDBSuccessEvent::GetResult(JSContext* aCx,
                           jsval* aResult)
{
  if (!mResult) {
    *aResult = JSVAL_VOID;
    return NS_OK;
  }

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_STATE(xpc);

  JSAutoRequest ar(aCx);
  JSObject* scope = JS_GetGlobalObject(aCx);
  NS_ENSURE_STATE(scope);

  nsresult rv = xpc->VariantToJS(aCx, scope, mResult, aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IDBSuccessEvent::GetTransaction(nsIIDBTransaction** aTransaction)
{
  nsCOMPtr<nsIIDBTransaction> transaction(mTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

GetSuccessEvent::~GetSuccessEvent()
{
  if (mValueRooted) {
    NS_DROP_JS_OBJECTS(this, GetSuccessEvent);
  }
}

nsresult
GetSuccessEvent::Init(IDBRequest* aRequest,
                      IDBTransaction* aTransaction)
{
  mSource = aRequest->Source();
  mTransaction = aTransaction;

  nsresult rv = InitEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR), PR_FALSE,
                          PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
GetSuccessEvent::GetResult(JSContext* aCx,
                           jsval* aResult)
{
  if (mValue.IsVoid()) {
    *aResult = JSVAL_VOID;
    return NS_OK;
  }

  if (!mValueRooted) {
    RootCachedValue();

    nsString jsonValue = mValue;
    mValue.Truncate();

    JSAutoRequest ar(aCx);

    nsCOMPtr<nsIJSON> json(new nsJSON());
    nsresult rv = json->DecodeToJSVal(jsonValue, aCx, &mCachedValue);
    if (NS_FAILED(rv)) {
      mCachedValue = JSVAL_VOID;

      NS_ERROR("Failed to decode!");
      return rv;
    }
  }

  *aResult = mCachedValue;
  return NS_OK;
}

void
GetSuccessEvent::RootCachedValue()
{
  mValueRooted = PR_TRUE;
  NS_HOLD_JS_OBJECTS(this, GetSuccessEvent);
}

NS_IMPL_ADDREF_INHERITED(GetSuccessEvent, IDBSuccessEvent)
NS_IMPL_RELEASE_INHERITED(GetSuccessEvent, IDBSuccessEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(GetSuccessEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GetSuccessEvent,
                                                  IDBSuccessEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(GetSuccessEvent)
  if (tmp->mValueRooted) {
    NS_DROP_JS_OBJECTS(tmp, GetSuccessEvent);
    tmp->mCachedValue = JSVAL_VOID;
    tmp->mValueRooted = PR_FALSE;
  }
NS_IMPL_CYCLE_COLLECTION_ROOT_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GetSuccessEvent,
                                                IDBSuccessEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mResult)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTransaction)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(GetSuccessEvent)
  if (JSVAL_IS_GCTHING(tmp->mCachedValue)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mCachedValue);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing)
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GetSuccessEvent)
NS_INTERFACE_MAP_END_INHERITING(IDBSuccessEvent)

NS_IMETHODIMP
GetAllSuccessEvent::GetResult(JSContext* aCx,
                              jsval* aResult)
{
  if (!mValueRooted) {
    RootCachedValue();

    JSAutoRequest ar(aCx);

    // Swap into a stack array so that we don't hang on to the strings if
    // something fails.
    nsTArray<nsString> values;
    if (!mValues.SwapElements(values)) {
      NS_ERROR("Failed to swap elements!");
      return NS_ERROR_FAILURE;
    }

    JSObject* array = JS_NewArrayObject(aCx, 0, NULL);
    if (!array) {
      NS_ERROR("Failed to make array!");
      return NS_ERROR_FAILURE;
    }

    mCachedValue = OBJECT_TO_JSVAL(array);

    if (!values.IsEmpty()) {
      if (!JS_SetArrayLength(aCx, array, jsuint(values.Length()))) {
        mCachedValue = JSVAL_VOID;
        NS_ERROR("Failed to set array length!");
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIJSON> json(new nsJSON());
      js::AutoValueRooter value(aCx);

      jsint count = jsint(values.Length());

      for (jsint index = 0; index < count; index++) {
        nsString jsonValue = values[index];
        values[index].Truncate();

        nsresult rv = json->DecodeToJSVal(jsonValue, aCx, value.jsval_addr());
        if (NS_FAILED(rv)) {
          mCachedValue = JSVAL_VOID;
          NS_ERROR("Failed to decode!");
          return rv;
        }

        if (!JS_SetElement(aCx, array, index, value.jsval_addr())) {
          mCachedValue = JSVAL_VOID;
          NS_ERROR("Failed to set array element!");
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  *aResult = mCachedValue;
  return NS_OK;
}

NS_IMETHODIMP
GetAllKeySuccessEvent::GetResult(JSContext* aCx,
                                 jsval* aResult)
{
  if (!mValueRooted) {
    RootCachedValue();

    JSAutoRequest ar(aCx);

    // Swap into a stack array so that we don't hang on to the strings if
    // something fails.
    nsTArray<Key> keys;
    if (!mKeys.SwapElements(keys)) {
      NS_ERROR("Failed to swap elements!");
      return NS_ERROR_FAILURE;
    }

    JSObject* array = JS_NewArrayObject(aCx, 0, NULL);
    if (!array) {
      NS_ERROR("Failed to make array!");
      return NS_ERROR_FAILURE;
    }

    mCachedValue = OBJECT_TO_JSVAL(array);

    if (!keys.IsEmpty()) {
      if (!JS_SetArrayLength(aCx, array, jsuint(keys.Length()))) {
        mCachedValue = JSVAL_VOID;
        NS_ERROR("Failed to set array length!");
        return NS_ERROR_FAILURE;
      }

      js::AutoValueRooter value(aCx);

      jsint count = jsint(keys.Length());

      for (jsint index = 0; index < count; index++) {
        const Key& key = keys[index];
        NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

        nsresult rv = IDBObjectStore::GetJSValFromKey(key, aCx, value.jsval_addr());
        if (NS_FAILED(rv)) {
          mCachedValue = JSVAL_VOID;
          NS_WARNING("Failed to get jsval for key!");
          return rv;
        }

        if (!JS_SetElement(aCx, array, index, value.jsval_addr())) {
          mCachedValue = JSVAL_VOID;
          NS_WARNING("Failed to set array element!");
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  *aResult = mCachedValue;
  return NS_OK;
}
