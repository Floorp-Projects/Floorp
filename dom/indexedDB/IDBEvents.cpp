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
#include "nsDOMException.h"
#include "nsJSON.h"
#include "nsThreadUtils.h"

#include "IDBRequest.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

namespace {

template <class T>
inline
already_AddRefed<nsIDOMEvent>
ForgetEvent(nsRefPtr<T>& aRefPtr)
{
  T* result;
  aRefPtr.forget(&result);
  return static_cast<nsIDOMEvent*>(static_cast<nsDOMEvent*>(result));
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
                      nsresult aResult)
{
  NS_ASSERTION(NS_FAILED(aResult), "Not a failure code!");
  NS_ASSERTION(NS_ERROR_GET_MODULE(aResult) == NS_ERROR_MODULE_DOM_INDEXEDDB,
               "Not an IndexedDB error code!");

  const char* name = nsnull;
  const char* message = nsnull;
  if (NS_FAILED(NS_GetNameAndMessageForDOMNSResult(aResult, &name, &message))) {
    NS_ERROR("Need a name and message for this code!");
  }

  nsRefPtr<IDBErrorEvent> event(new IDBErrorEvent());

  event->mSource = aRequest->Source();
  event->mCode = NS_ERROR_GET_CODE(aResult);
  event->mMessage.AssignASCII(message);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING(ERROR_EVT_STR), PR_TRUE,
                                 PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return ForgetEvent(event);
}

// static
already_AddRefed<nsIRunnable>
IDBErrorEvent::CreateRunnable(IDBRequest* aRequest,
                              nsresult aResult)
{
  nsCOMPtr<nsIDOMEvent> event(Create(aRequest, aResult));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aRequest, event));
  return runnable.forget();
}

// static
already_AddRefed<nsIDOMEvent>
IDBErrorEvent::MaybeDuplicate(nsIDOMEvent* aOther)
{
  NS_ASSERTION(aOther, "Null pointer!");

  nsCOMPtr<nsIDOMNSEvent> domNSEvent(do_QueryInterface(aOther));
  nsCOMPtr<nsIIDBErrorEvent> errorEvent(do_QueryInterface(aOther));

  if (!domNSEvent || !errorEvent) {
    return nsnull;
  }

  PRBool isTrusted;
  nsresult rv = domNSEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (!isTrusted) {
    return nsnull;
  }

  nsString type;
  rv = errorEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRBool canBubble;
  rv = errorEvent->GetBubbles(&canBubble);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsISupports> source;
  rv = errorEvent->GetSource(getter_AddRefs(source));
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRUint16 code;
  rv = errorEvent->GetCode(&code);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsString message;
  rv = errorEvent->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsRefPtr<IDBErrorEvent> event(new IDBErrorEvent());

  event->mSource = source;
  event->mCode = code;
  event->mMessage = message;

  rv = event->InitEvent(type, canBubble, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return ForgetEvent(event);
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
IDBErrorEvent::GetMessage(nsAString& aMessage)
{
  aMessage.Assign(mMessage);
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

  return ForgetEvent(event);
}

// static
already_AddRefed<nsIRunnable>
IDBSuccessEvent::CreateRunnable(IDBRequest* aRequest,
                                nsIVariant* aResult,
                                nsIIDBTransaction* aTransaction)
{
  nsCOMPtr<nsIDOMEvent> event(Create(aRequest, aResult, aTransaction));
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
  NS_ENSURE_TRUE(xpc, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  JSAutoRequest ar(aCx);
  JSObject* scope = JS_GetGlobalObject(aCx);
  NS_ENSURE_TRUE(scope, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsresult rv = xpc->VariantToJS(aCx, scope, mResult, aResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

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
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
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
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    JSObject* array = JS_NewArrayObject(aCx, 0, NULL);
    if (!array) {
      NS_ERROR("Failed to make array!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mCachedValue = OBJECT_TO_JSVAL(array);

    if (!values.IsEmpty()) {
      if (!JS_SetArrayLength(aCx, array, jsuint(values.Length()))) {
        mCachedValue = JSVAL_VOID;
        NS_ERROR("Failed to set array length!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
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
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        if (!JS_SetElement(aCx, array, index, value.jsval_addr())) {
          mCachedValue = JSVAL_VOID;
          NS_ERROR("Failed to set array element!");
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
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
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    JSObject* array = JS_NewArrayObject(aCx, 0, NULL);
    if (!array) {
      NS_ERROR("Failed to make array!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    mCachedValue = OBJECT_TO_JSVAL(array);

    if (!keys.IsEmpty()) {
      if (!JS_SetArrayLength(aCx, array, jsuint(keys.Length()))) {
        mCachedValue = JSVAL_VOID;
        NS_ERROR("Failed to set array length!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
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
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        if (!JS_SetElement(aCx, array, index, value.jsval_addr())) {
          mCachedValue = JSVAL_VOID;
          NS_WARNING("Failed to set array element!");
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }
      }
    }
  }

  *aResult = mCachedValue;
  return NS_OK;
}

// static
already_AddRefed<nsIDOMEvent>
IDBVersionChangeEvent::CreateInternal(nsISupports* aSource,
                                      const nsAString& aType,
                                      const nsAString& aVersion)
{
  nsRefPtr<IDBVersionChangeEvent> event(new IDBVersionChangeEvent());

  event->mSource = aSource;
  event->mVersion = aVersion;

  nsresult rv = event->InitEvent(aType, PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return ForgetEvent(event);
}

// static
already_AddRefed<nsIRunnable>
IDBVersionChangeEvent::CreateRunnableInternal(nsISupports* aSource,
                                              const nsAString& aType,
                                              const nsAString& aVersion,
                                              nsIDOMEventTarget* aTarget)
{
  nsCOMPtr<nsIDOMEvent> event = CreateInternal(aSource, aType, aVersion);
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeEvent, IDBEvent)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeEvent, IDBEvent)

NS_INTERFACE_MAP_BEGIN(IDBVersionChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBVersionChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBVersionChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(IDBEvent)

DOMCI_DATA(IDBVersionChangeEvent, IDBVersionChangeEvent)

NS_IMETHODIMP
IDBVersionChangeEvent::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(mVersion);
  return NS_OK;
}
