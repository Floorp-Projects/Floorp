/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollAreaEvent_h_
#define mozilla_dom_ScrollAreaEvent_h_

#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ScrollAreaEventBinding.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMScrollAreaEvent.h"

namespace mozilla {
namespace dom {

class ScrollAreaEvent : public UIEvent,
                        public nsIDOMScrollAreaEvent
{
public:
  ScrollAreaEvent(EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  InternalScrollAreaEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSCROLLAREAEVENT

  NS_FORWARD_NSIDOMUIEVENT(UIEvent::)

  NS_FORWARD_TO_EVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData()
  {
    return Event::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return ScrollAreaEventBinding::Wrap(aCx, this);
  }

  float X() const
  {
    return mClientArea.Left();
  }

  float Y() const
  {
    return mClientArea.Top();
  }

  float Width() const
  {
    return mClientArea.Width();
  }

  float Height() const
  {
    return mClientArea.Height();
  }

  void InitScrollAreaEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsIDOMWindow* aView,
                           int32_t aDetail,
                           float aX, float aY,
                           float aWidth, float aHeight,
                           ErrorResult& aRv)
  {
    aRv = InitScrollAreaEvent(aType, aCanBubble, aCancelable, aView,
                              aDetail, aX, aY, aWidth, aHeight);
  }

protected:
  ~ScrollAreaEvent() {}

  DOMRect mClientArea;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScrollAreaEvent_h_
