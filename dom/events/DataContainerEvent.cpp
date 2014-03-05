/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DataContainerEvent.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"

namespace mozilla {
namespace dom {

DataContainerEvent::DataContainerEvent(EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DataContainerEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DataContainerEvent, Event)
  tmp->mData.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DataContainerEvent, Event)
  tmp->mData.EnumerateRead(TraverseEntry, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DataContainerEvent, Event)
NS_IMPL_RELEASE_INHERITED(DataContainerEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DataContainerEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataContainerEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMETHODIMP
DataContainerEvent::GetData(const nsAString& aKey, nsIVariant** aData)
{
  NS_ENSURE_ARG_POINTER(aData);

  mData.Get(aKey, aData);
  return NS_OK;
}

NS_IMETHODIMP
DataContainerEvent::SetData(const nsAString& aKey, nsIVariant* aData)
{
  NS_ENSURE_ARG(aData);

  // Make sure this event isn't already being dispatched.
  NS_ENSURE_STATE(!mEvent->mFlags.mIsBeingDispatched);
  mData.Put(aKey, aData);
  return NS_OK;
}

void
DataContainerEvent::SetData(JSContext* aCx, const nsAString& aKey,
                            JS::Handle<JS::Value> aVal,
                            ErrorResult& aRv)
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

PLDHashOperator
DataContainerEvent::TraverseEntry(const nsAString& aKey,
                                  nsIVariant* aDataItem,
                                  void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);
  cb->NoteXPCOMChild(aDataItem);

  return PL_DHASH_NEXT;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMDataContainerEvent(nsIDOMEvent** aInstancePtrResult,
                            EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            WidgetEvent* aEvent)
{
  DataContainerEvent* it = new DataContainerEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}

