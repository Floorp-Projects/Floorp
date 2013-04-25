/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCompositionEvent_h__
#define nsDOMCompositionEvent_h__

#include "nsDOMUIEvent.h"
#include "nsIDOMCompositionEvent.h"
#include "mozilla/dom/CompositionEventBinding.h"

class nsDOMCompositionEvent : public nsDOMUIEvent,
                              public nsIDOMCompositionEvent
{
public:
  nsDOMCompositionEvent(mozilla::dom::EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        nsCompositionEvent* aEvent);
  virtual ~nsDOMCompositionEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMUIEVENT
  NS_DECL_NSIDOMCOMPOSITIONEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::CompositionEventBinding::Wrap(aCx, aScope, this);
  }

  void
  InitCompositionEvent(const nsAString& aType,
                       bool aCanBubble,
                       bool aCancelable,
                       nsIDOMWindow* aView,
                       const nsAString& aData,
                       const nsAString& aLocale,
                       mozilla::ErrorResult& aRv)
  {
    aRv = InitCompositionEvent(aType, aCanBubble, aCancelable, aView,
                               aData, aLocale);
  }

protected:
  nsString mData;
  nsString mLocale;
};

#endif // nsDOMCompositionEvent_h__
