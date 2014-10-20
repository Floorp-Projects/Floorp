/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This class implements a XUL "command" event.  See nsIDOMXULCommandEvent.idl

#ifndef mozilla_dom_XULCommandEvent_h_
#define mozilla_dom_XULCommandEvent_h_

#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/XULCommandEventBinding.h"
#include "nsIDOMXULCommandEvent.h"

namespace mozilla {
namespace dom {

class XULCommandEvent : public UIEvent,
                        public nsIDOMXULCommandEvent
{
public:
  XULCommandEvent(EventTarget* aOwner,
                  nsPresContext* aPresContext,
                  WidgetInputEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULCommandEvent, UIEvent)
  NS_DECL_NSIDOMXULCOMMANDEVENT

  // Forward our inherited virtual methods to the base class
  NS_FORWARD_TO_UIEVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return XULCommandEventBinding::Wrap(aCx, this);
  }

  bool AltKey();
  bool CtrlKey();
  bool ShiftKey();
  bool MetaKey();

  already_AddRefed<Event> GetSourceEvent()
  {
    nsRefPtr<Event> e =
      mSourceEvent ? mSourceEvent->InternalDOMEvent() : nullptr;
    return e.forget();
  }

  void InitCommandEvent(const nsAString& aType,
                        bool aCanBubble, bool aCancelable,
                        nsIDOMWindow* aView,
                        int32_t aDetail,
                        bool aCtrlKey, bool aAltKey,
                        bool aShiftKey, bool aMetaKey,
                        Event* aSourceEvent,
                        ErrorResult& aRv)
  {
    aRv = InitCommandEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                           aCtrlKey, aAltKey, aShiftKey, aMetaKey,
                           aSourceEvent);
  }

protected:
  ~XULCommandEvent() {}

  nsCOMPtr<nsIDOMEvent> mSourceEvent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_XULCommandEvent_h_
