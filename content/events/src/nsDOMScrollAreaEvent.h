/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMScrollAreaEvent_h__
#define nsDOMScrollAreaEvent_h__

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMScrollAreaEvent.h"
#include "nsDOMUIEvent.h"

#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/ScrollAreaEventBinding.h"

class nsDOMScrollAreaEvent : public nsDOMUIEvent,
                             public nsIDOMScrollAreaEvent
{
  typedef mozilla::dom::DOMRect DOMRect;

public:
  nsDOMScrollAreaEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext *aPresContext,
                       mozilla::InternalScrollAreaEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSCROLLAREAEVENT

  NS_FORWARD_NSIDOMUIEVENT(nsDOMUIEvent::)

  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData()
  {
    return nsDOMEvent::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter) MOZ_OVERRIDE;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::ScrollAreaEventBinding::Wrap(aCx, aScope, this);
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
                           mozilla::ErrorResult& aRv)
  {
    aRv = InitScrollAreaEvent(aType, aCanBubble, aCancelable, aView,
                              aDetail, aX, aY, aWidth, aHeight);
  }

protected:
  DOMRect mClientArea;
};

#endif // nsDOMScrollAreaEvent_h__
