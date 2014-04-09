/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CompositionEvent_h_
#define mozilla_dom_CompositionEvent_h_

#include "mozilla/dom/CompositionEventBinding.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMCompositionEvent.h"

namespace mozilla {
namespace dom {

class CompositionEvent : public UIEvent,
                         public nsIDOMCompositionEvent
{
public:
  CompositionEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetCompositionEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_UIEVENT
  NS_DECL_NSIDOMCOMPOSITIONEVENT

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return CompositionEventBinding::Wrap(aCx, this);
  }

  void InitCompositionEvent(const nsAString& aType,
                            bool aCanBubble,
                            bool aCancelable,
                            nsIDOMWindow* aView,
                            const nsAString& aData,
                            const nsAString& aLocale,
                            ErrorResult& aRv)
  {
    aRv = InitCompositionEvent(aType, aCanBubble, aCancelable, aView,
                               aData, aLocale);
  }

protected:
  nsString mData;
  nsString mLocale;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CompositionEvent_h_
