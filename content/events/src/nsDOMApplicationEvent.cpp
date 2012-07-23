/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "nsDOMApplicationEvent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"
#include "nsDOMClassInfoID.h"
 
DOMCI_DATA(MozApplicationEvent, nsDOMMozApplicationEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMozApplicationEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMMozApplicationEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mApplication)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMMozApplicationEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mApplication)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMMozApplicationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozApplicationEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozApplicationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)
 
NS_IMPL_ADDREF_INHERITED(nsDOMMozApplicationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozApplicationEvent, nsDOMEvent)
 
NS_IMETHODIMP
nsDOMMozApplicationEvent::GetApplication(mozIDOMApplication** aApplication)
{
  NS_IF_ADDREF(*aApplication = mApplication);
  return NS_OK;
}
 
NS_IMETHODIMP
nsDOMMozApplicationEvent::InitMozApplicationEvent(const nsAString& aType,
                                                  bool aCanBubble,
                                                  bool aCancelable,
                                                  mozIDOMApplication* aApplication)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);
 
  mApplication = aApplication;

  return NS_OK;
}
 
nsresult
nsDOMMozApplicationEvent::InitFromCtor(const nsAString& aType, JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MozApplicationEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  return InitMozApplicationEvent(aType, d.bubbles, d.cancelable, d.application);
}
 
nsresult
NS_NewDOMMozApplicationEvent(nsIDOMEvent** aInstancePtrResult,
                             nsPresContext* aPresContext,
                             nsEvent* aEvent)
{
  nsDOMMozApplicationEvent* e = new nsDOMMozApplicationEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}
