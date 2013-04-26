/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This class implements a XUL "command" event.  See nsIDOMXULCommandEvent.idl

#ifndef nsDOMXULCommandEvent_h_
#define nsDOMXULCommandEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMXULCommandEvent.h"
#include "mozilla/dom/XULCommandEventBinding.h"

class nsDOMXULCommandEvent : public nsDOMUIEvent,
                             public nsIDOMXULCommandEvent
{
public:
  nsDOMXULCommandEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext, nsInputEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)
  NS_DECL_NSIDOMXULCOMMANDEVENT

  // Forward our inherited virtual methods to the base class
  NS_FORWARD_TO_NSDOMUIEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::XULCommandEventBinding::Wrap(aCx, aScope, this);
  }

  bool AltKey()
  {
    return Event()->IsAlt();
  }

  bool CtrlKey()
  {
    return Event()->IsControl();
  }

  bool ShiftKey()
  {
    return Event()->IsShift();
  }

  bool MetaKey()
  {
    return Event()->IsMeta();
  }

  already_AddRefed<nsDOMEvent> GetSourceEvent()
  {
    nsRefPtr<nsDOMEvent> e =
      mSourceEvent ? mSourceEvent->InternalDOMEvent() : nullptr;
    return e.forget();
  }

  void InitCommandEvent(const nsAString& aType,
                        bool aCanBubble, bool aCancelable,
                        nsIDOMWindow* aView,
                        int32_t aDetail,
                        bool aCtrlKey, bool aAltKey,
                        bool aShiftKey, bool aMetaKey,
                        nsDOMEvent* aSourceEvent,
                        mozilla::ErrorResult& aRv)
  {
    aRv = InitCommandEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                           aCtrlKey, aAltKey, aShiftKey, aMetaKey,
                           aSourceEvent);
  }

protected:
  // Convenience accessor for the event
  nsInputEvent* Event() {
    return static_cast<nsInputEvent*>(mEvent);
  }

  nsCOMPtr<nsIDOMEvent> mSourceEvent;
};

#endif  // nsDOMXULCommandEvent_h_
