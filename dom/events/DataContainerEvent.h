/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataContainerEvent_h_
#define mozilla_dom_DataContainerEvent_h_

#include "mozilla/dom/DataContainerEventBinding.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsInterfaceHashtable.h"

namespace mozilla {
namespace dom {

class DataContainerEvent : public Event,
                           public nsIDOMDataContainerEvent
{
public:
  DataContainerEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DataContainerEvent, Event)

  NS_FORWARD_TO_EVENT

  NS_DECL_NSIDOMDATACONTAINEREVENT

  virtual JSObject*
  WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return DataContainerEventBinding::Wrap(aCx, this, aGivenProto);
  }

  already_AddRefed<nsIVariant> GetData(const nsAString& aKey)
  {
    nsCOMPtr<nsIVariant> val;
    GetData(aKey, getter_AddRefs(val));
    return val.forget();
  }

  void SetData(JSContext* aCx, const nsAString& aKey,
               JS::Handle<JS::Value> aVal, ErrorResult& aRv);

protected:
  ~DataContainerEvent() {}

private:
  static PLDHashOperator
    TraverseEntry(const nsAString& aKey, nsIVariant* aDataItem, void* aUserArg);

  nsInterfaceHashtable<nsStringHashKey, nsIVariant> mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DataContainerEvent_h_
