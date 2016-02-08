/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return XULCommandEventBinding::Wrap(aCx, this, aGivenProto);
  }

  bool AltKey();
  bool CtrlKey();
  bool ShiftKey();
  bool MetaKey();

  already_AddRefed<Event> GetSourceEvent()
  {
    RefPtr<Event> e =
      mSourceEvent ? mSourceEvent->InternalDOMEvent() : nullptr;
    return e.forget();
  }

  void InitCommandEvent(const nsAString& aType,
                        bool aCanBubble, bool aCancelable,
                        nsGlobalWindow* aView,
                        int32_t aDetail,
                        bool aCtrlKey, bool aAltKey,
                        bool aShiftKey, bool aMetaKey,
                        Event* aSourceEvent)
  {
    InitCommandEvent(aType, aCanBubble, aCancelable, aView->AsInner(),
                     aDetail, aCtrlKey, aAltKey, aShiftKey, aMetaKey,
                     aSourceEvent);
  }

protected:
  ~XULCommandEvent() {}

  nsCOMPtr<nsIDOMEvent> mSourceEvent;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::XULCommandEvent>
NS_NewDOMXULCommandEvent(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         mozilla::WidgetInputEvent* aEvent);

#endif // mozilla_dom_XULCommandEvent_h_
