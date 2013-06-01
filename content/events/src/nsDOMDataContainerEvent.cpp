/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDataContainerEvent.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"

nsDOMDataContainerEvent::nsDOMDataContainerEvent(
                                             mozilla::dom::EventTarget* aOwner,
                                             nsPresContext* aPresContext,
                                             nsEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent)
{
  mData.Init();
  SetIsDOMBinding();
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDataContainerEvent,
                                                nsDOMEvent)
  if (tmp->mData.IsInitialized())
    tmp->mData.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDataContainerEvent,
                                                  nsDOMEvent)
  tmp->mData.EnumerateRead(TraverseEntry, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMDataContainerEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDataContainerEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDataContainerEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataContainerEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMDataContainerEvent::GetData(const nsAString& aKey, nsIVariant **aData)
{
  NS_ENSURE_ARG_POINTER(aData);

  NS_ENSURE_STATE(mData.IsInitialized());

  mData.Get(aKey, aData);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDataContainerEvent::SetData(const nsAString& aKey, nsIVariant *aData)
{
  NS_ENSURE_ARG(aData);

  // Make sure this event isn't already being dispatched.
  NS_ENSURE_STATE(!mEvent->mFlags.mIsBeingDispatched);
  NS_ENSURE_STATE(mData.IsInitialized());
  mData.Put(aKey, aData);
  return NS_OK;
}

void
nsDOMDataContainerEvent::SetData(JSContext* aCx, const nsAString& aKey,
                                 JS::Handle<JS::Value> aVal,
                                 mozilla::ErrorResult& aRv)
{
  if (!nsContentUtils::XPConnect()) {
    aRv = NS_ERROR_FAILURE;
    return;
  }
  nsCOMPtr<nsIVariant> val;
  nsresult rv =
    nsContentUtils::XPConnect()->JSToVariant(aCx, aVal, getter_AddRefs(val));
  if (NS_FAILED(rv)) {
    aRv = rv;
    return;
  }
  aRv = SetData(aKey, val);
}

nsresult
NS_NewDOMDataContainerEvent(nsIDOMEvent** aInstancePtrResult,
                            mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            nsEvent* aEvent)
{
  nsDOMDataContainerEvent* it =
    new nsDOMDataContainerEvent(aOwner, aPresContext, aEvent);
  NS_ENSURE_TRUE(it, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(it, aInstancePtrResult);
}

PLDHashOperator
nsDOMDataContainerEvent::TraverseEntry(const nsAString& aKey,
                                       nsIVariant *aDataItem,
                                       void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);
  cb->NoteXPCOMChild(aDataItem);

  return PL_DHASH_NEXT;
}

