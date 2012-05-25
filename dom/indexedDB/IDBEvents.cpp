/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBEvents.h"

#include "nsIPrivateDOMEvent.h"

#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMException.h"
#include "nsJSON.h"
#include "nsThreadUtils.h"

#include "IDBRequest.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class EventFiringRunnable : public nsRunnable
{
public:
  EventFiringRunnable(nsIDOMEventTarget* aTarget,
                      nsIDOMEvent* aEvent)
  : mTarget(aTarget), mEvent(aEvent)
  { }

  NS_IMETHOD Run() {
    bool dummy;
    return mTarget->DispatchEvent(mEvent, &dummy);
  }

private:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
};

} // anonymous namespace

already_AddRefed<nsDOMEvent>
mozilla::dom::indexedDB::CreateGenericEvent(const nsAString& aType,
                                            Bubbles aBubbles,
                                            Cancelable aCancelable)
{
  nsRefPtr<nsDOMEvent> event(new nsDOMEvent(nsnull, nsnull));
  nsresult rv = event->InitEvent(aType,
                                 aBubbles == eDoesBubble ? true : false,
                                 aCancelable == eCancelable ? true : false);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return event.forget();
}

// static
already_AddRefed<nsDOMEvent>
IDBVersionChangeEvent::CreateInternal(const nsAString& aType,
                                      PRUint64 aOldVersion,
                                      PRUint64 aNewVersion)
{
  nsRefPtr<IDBVersionChangeEvent> event(new IDBVersionChangeEvent());

  nsresult rv = event->InitEvent(aType, false, false);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, nsnull);

  event->mOldVersion = aOldVersion;
  event->mNewVersion = aNewVersion;

  nsDOMEvent* result;
  event.forget(&result);
  return result;
}

// static
already_AddRefed<nsIRunnable>
IDBVersionChangeEvent::CreateRunnableInternal(const nsAString& aType,
                                              PRUint64 aOldVersion,
                                              PRUint64 aNewVersion,
                                              nsIDOMEventTarget* aTarget)
{
  nsRefPtr<nsDOMEvent> event =
    CreateInternal(aType, aOldVersion, aNewVersion);
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(IDBVersionChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBVersionChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBVersionChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

DOMCI_DATA(IDBVersionChangeEvent, IDBVersionChangeEvent)

NS_IMETHODIMP
IDBVersionChangeEvent::GetOldVersion(PRUint64* aOldVersion)
{
  NS_ENSURE_ARG_POINTER(aOldVersion);
  *aOldVersion = mOldVersion;
  return NS_OK;
}

NS_IMETHODIMP
IDBVersionChangeEvent::GetNewVersion(JS::Value* aNewVersion)
{
  NS_ENSURE_ARG_POINTER(aNewVersion);

  if (!mNewVersion) {
    *aNewVersion = JSVAL_NULL;
  }
  else {
    *aNewVersion = INT_TO_JSVAL(mNewVersion);
  }

  return NS_OK;
}
