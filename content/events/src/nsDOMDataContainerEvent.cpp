/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDataContainerEvent.h"
#include "nsDOMClassInfoID.h"

nsDOMDataContainerEvent::nsDOMDataContainerEvent(nsPresContext *aPresContext,
                                                 nsEvent *aEvent)
  : nsDOMEvent(aPresContext, aEvent)
{
  mData.Init();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMDataContainerEvent)

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

DOMCI_DATA(DataContainerEvent, nsDOMDataContainerEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDataContainerEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataContainerEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DataContainerEvent)
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
  NS_ENSURE_STATE(!(NS_IS_EVENT_IN_DISPATCH(mEvent)));
  NS_ENSURE_STATE(mData.IsInitialized());
  mData.Put(aKey, aData);
  return NS_OK;
}

nsresult
NS_NewDOMDataContainerEvent(nsIDOMEvent** aInstancePtrResult,
                   nsPresContext* aPresContext,
                   nsEvent* aEvent)
{
  nsDOMDataContainerEvent* it =
    new nsDOMDataContainerEvent(aPresContext, aEvent);
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

