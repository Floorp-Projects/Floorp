/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NotifyPaintEvent_h_
#define mozilla_dom_NotifyPaintEvent_h_

#include "mozilla/Attributes.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/NotifyPaintEventBinding.h"
#include "nsIDOMNotifyPaintEvent.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {

class DOMRect;
class DOMRectList;
class PaintRequestList;

class NotifyPaintEvent : public Event,
                         public nsIDOMNotifyPaintEvent
{

public:
  NotifyPaintEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetEvent* aEvent,
                   EventMessage aEventMessage,
                   nsInvalidateRequestList* aInvalidateRequests);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMNOTIFYPAINTEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData() override
  {
    return Event::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) override;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) override;

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return NotifyPaintEventBinding::Wrap(aCx, this, aGivenProto);
  }

  already_AddRefed<DOMRectList> ClientRects();

  already_AddRefed<DOMRect> BoundingClientRect();

  already_AddRefed<PaintRequestList> PaintRequests();

protected:
  ~NotifyPaintEvent() {}

private:
  nsRegion GetRegion();

  nsTArray<nsInvalidateRequestList::Request> mInvalidateRequests;
};

} // namespace dom
} // namespace mozilla

// This empties aInvalidateRequests.
already_AddRefed<mozilla::dom::NotifyPaintEvent>
NS_NewDOMNotifyPaintEvent(mozilla::dom::EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          mozilla::WidgetEvent* aEvent,
                          mozilla::EventMessage aEventMessage =
                            mozilla::NS_EVENT_NULL,
                          nsInvalidateRequestList* aInvalidateRequests =
                            nullptr);

#endif // mozilla_dom_NotifyPaintEvent_h_
