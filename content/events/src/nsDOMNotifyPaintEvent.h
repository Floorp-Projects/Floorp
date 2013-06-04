/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNotifyPaintEvent_h_
#define nsDOMNotifyPaintEvent_h_

#include "mozilla/Attributes.h"
#include "nsIDOMNotifyPaintEvent.h"
#include "nsDOMEvent.h"
#include "nsPresContext.h"
#include "mozilla/dom/NotifyPaintEventBinding.h"

class nsPaintRequestList;
class nsClientRectList;
class nsClientRect;

class nsDOMNotifyPaintEvent : public nsDOMEvent,
                              public nsIDOMNotifyPaintEvent
{
public:
  nsDOMNotifyPaintEvent(mozilla::dom::EventTarget* aOwner,
                        nsPresContext*           aPresContext,
                        nsEvent*                 aEvent,
                        uint32_t                 aEventType,
                        nsInvalidateRequestList* aInvalidateRequests);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMNOTIFYPAINTEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData() MOZ_OVERRIDE
  {
    return nsDOMEvent::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::NotifyPaintEventBinding::Wrap(aCx, aScope, this);
  }

  already_AddRefed<nsClientRectList> ClientRects();

  already_AddRefed<nsClientRect> BoundingClientRect();

  already_AddRefed<nsPaintRequestList> PaintRequests();
private:
  nsRegion GetRegion();

  nsTArray<nsInvalidateRequestList::Request> mInvalidateRequests;
};

#endif // nsDOMNotifyPaintEvent_h_
