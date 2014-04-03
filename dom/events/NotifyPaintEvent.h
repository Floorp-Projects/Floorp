/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
                   uint32_t aEventType,
                   nsInvalidateRequestList* aInvalidateRequests);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMNOTIFYPAINTEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData() MOZ_OVERRIDE
  {
    return Event::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return NotifyPaintEventBinding::Wrap(aCx, aScope, this);
  }

  already_AddRefed<DOMRectList> ClientRects();

  already_AddRefed<DOMRect> BoundingClientRect();

  already_AddRefed<PaintRequestList> PaintRequests();
private:
  nsRegion GetRegion();

  nsTArray<nsInvalidateRequestList::Request> mInvalidateRequests;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_NotifyPaintEvent_h_
